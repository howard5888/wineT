/*
 * Server-side pipe management
 *
 * Copyright (C) 1998 Alexandre Julliard
 * Copyright (C) 2001 Mike McCormack
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * TODO:
 *   message mode
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <time.h>
#include <unistd.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "file.h"
#include "handle.h"
#include "thread.h"
#include "request.h"

enum pipe_state
{
    ps_idle_server,
    ps_wait_open,
    ps_connected_server,
    ps_wait_disconnect,
    ps_disconnected_server,
    ps_wait_connect
};

struct named_pipe;

struct pipe_server
{
    struct object        obj;        /* object header */
    struct fd           *fd;         /* pipe file descriptor */
    struct list          entry;      /* entry in named pipe servers list */
    enum pipe_state      state;      /* server state */
    struct pipe_client  *client;     /* client that this server is connected to */
    struct named_pipe   *pipe;
    struct timeout_user *flush_poll;
    struct event        *event;
    struct list          wait_q;     /* only a single one can be queued */
    unsigned int         options;    /* pipe options */
};

struct pipe_client
{
    struct object        obj;        /* object header */
    struct fd           *fd;         /* pipe file descriptor */
    struct pipe_server  *server;     /* server that this client is connected to */
    unsigned int         flags;      /* file flags */
};

struct named_pipe
{
    struct object       obj;         /* object header */
    unsigned int        flags;
    unsigned int        maxinstances;
    unsigned int        outsize;
    unsigned int        insize;
    unsigned int        timeout;
    unsigned int        instances;
    struct list         servers;     /* list of servers using this pipe */
    struct list         waiters;     /* list of clients waiting to connect */
};

struct named_pipe_device
{
    struct object       obj;         /* object header */
    struct fd          *fd;          /* pseudo-fd for ioctls */
    struct namespace   *pipes;       /* named pipe namespace */
};

static void named_pipe_dump( struct object *obj, int verbose );
static unsigned int named_pipe_map_access( struct object *obj, unsigned int access );
static void named_pipe_destroy( struct object *obj );

static const struct object_ops named_pipe_ops =
{
    sizeof(struct named_pipe),    /* size */
    named_pipe_dump,              /* dump */
    no_add_queue,                 /* add_queue */
    NULL,                         /* remove_queue */
    NULL,                         /* signaled */
    NULL,                         /* satisfied */
    no_signal,                    /* signal */
    no_get_fd,                    /* get_fd */
    named_pipe_map_access,        /* map_access */
    no_lookup_name,               /* lookup_name */
    no_close_handle,              /* close_handle */
    named_pipe_destroy            /* destroy */
};

/* functions common to server and client */
static unsigned int pipe_map_access( struct object *obj, unsigned int access );

/* server end functions */
static void pipe_server_dump( struct object *obj, int verbose );
static struct fd *pipe_server_get_fd( struct object *obj );
static void pipe_server_destroy( struct object *obj);
static int pipe_server_flush( struct fd *fd, struct event **event );
static int pipe_server_get_info( struct fd *fd );

static const struct object_ops pipe_server_ops =
{
    sizeof(struct pipe_server),   /* size */
    pipe_server_dump,             /* dump */
    default_fd_add_queue,         /* add_queue */
    default_fd_remove_queue,      /* remove_queue */
    default_fd_signaled,          /* signaled */
    no_satisfied,                 /* satisfied */
    no_signal,                    /* signal */
    pipe_server_get_fd,           /* get_fd */
    pipe_map_access,              /* map_access */
    no_lookup_name,               /* lookup_name */
    no_close_handle,              /* close_handle */
    pipe_server_destroy           /* destroy */
};

static const struct fd_ops pipe_server_fd_ops =
{
    default_fd_get_poll_events,     /* get_poll_events */
    default_poll_event,           /* poll_event */
    pipe_server_flush,            /* flush */
    pipe_server_get_info,         /* get_file_info */
    default_fd_queue_async,       /* queue_async */
    default_fd_cancel_async,      /* cancel_async */
};

/* client end functions */
static void pipe_client_dump( struct object *obj, int verbose );
static struct fd *pipe_client_get_fd( struct object *obj );
static void pipe_client_destroy( struct object *obj );
static int pipe_client_flush( struct fd *fd, struct event **event );
static int pipe_client_get_info( struct fd *fd );

static const struct object_ops pipe_client_ops =
{
    sizeof(struct pipe_client),   /* size */
    pipe_client_dump,             /* dump */
    default_fd_add_queue,         /* add_queue */
    default_fd_remove_queue,      /* remove_queue */
    default_fd_signaled,          /* signaled */
    no_satisfied,                 /* satisfied */
    no_signal,                    /* signal */
    pipe_client_get_fd,           /* get_fd */
    pipe_map_access,              /* map_access */
    no_lookup_name,               /* lookup_name */
    no_close_handle,              /* close_handle */
    pipe_client_destroy           /* destroy */
};

static const struct fd_ops pipe_client_fd_ops =
{
    default_fd_get_poll_events,   /* get_poll_events */
    default_poll_event,           /* poll_event */
    pipe_client_flush,            /* flush */
    pipe_client_get_info,         /* get_file_info */
    default_fd_queue_async,       /* queue_async */
    default_fd_cancel_async       /* cancel_async */
};

static void named_pipe_device_dump( struct object *obj, int verbose );
static struct fd *named_pipe_device_get_fd( struct object *obj );
static struct object *named_pipe_device_lookup_name( struct object *obj,
    struct unicode_str *name, unsigned int attr );
static void named_pipe_device_destroy( struct object *obj );
static int named_pipe_device_get_file_info( struct fd *fd );

static const struct object_ops named_pipe_device_ops =
{
    sizeof(struct named_pipe_device), /* size */
    named_pipe_device_dump,           /* dump */
    no_add_queue,                     /* add_queue */
    NULL,                             /* remove_queue */
    NULL,                             /* signaled */
    no_satisfied,                     /* satisfied */
    no_signal,                        /* signal */
    named_pipe_device_get_fd,         /* get_fd */
    pipe_map_access,                  /* map_access */
    named_pipe_device_lookup_name,    /* lookup_name */
    no_close_handle,                  /* close_handle */
    named_pipe_device_destroy         /* destroy */
};

static const struct fd_ops named_pipe_device_fd_ops =
{
    default_fd_get_poll_events,       /* get_poll_events */
    default_poll_event,               /* poll_event */
    no_flush,                         /* flush */
    named_pipe_device_get_file_info,  /* get_file_info */
    default_fd_queue_async,           /* queue_async */
    default_fd_cancel_async           /* cancel_async */
};

static void named_pipe_dump( struct object *obj, int verbose )
{
    struct named_pipe *pipe = (struct named_pipe *) obj;
    assert( obj->ops == &named_pipe_ops );
    fprintf( stderr, "Named pipe " );
    dump_object_name( &pipe->obj );
    fprintf( stderr, "\n" );
}

static unsigned int named_pipe_map_access( struct object *obj, unsigned int access )
{
    if (access & GENERIC_READ)    access |= STANDARD_RIGHTS_READ;
    if (access & GENERIC_WRITE)   access |= STANDARD_RIGHTS_WRITE | FILE_CREATE_PIPE_INSTANCE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE;
    if (access & GENERIC_ALL)     access |= STANDARD_RIGHTS_ALL;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static void pipe_server_dump( struct object *obj, int verbose )
{
    struct pipe_server *server = (struct pipe_server *) obj;
    assert( obj->ops == &pipe_server_ops );
    fprintf( stderr, "Named pipe server pipe=%p state=%d\n", server->pipe, server->state );
}

static void pipe_client_dump( struct object *obj, int verbose )
{
    struct pipe_client *client = (struct pipe_client *) obj;
    assert( obj->ops == &pipe_client_ops );
    fprintf( stderr, "Named pipe client server=%p\n", client->server );
}

static void named_pipe_destroy( struct object *obj)
{
    struct named_pipe *pipe = (struct named_pipe *) obj;

    assert( list_empty( &pipe->servers ) );
    assert( !pipe->instances );
    async_terminate_queue( &pipe->waiters, STATUS_HANDLES_CLOSED );
}

static struct fd *pipe_client_get_fd( struct object *obj )
{
    struct pipe_client *client = (struct pipe_client *) obj;
    if (client->fd)
        return (struct fd *) grab_object( client->fd );
    set_error( STATUS_PIPE_DISCONNECTED );
    return NULL;
}

static struct fd *pipe_server_get_fd( struct object *obj )
{
    struct pipe_server *server = (struct pipe_server *) obj;

    switch(server->state)
    {
    case ps_connected_server:
    case ps_wait_disconnect:
        assert( server->fd );
        return (struct fd *) grab_object( server->fd );

    case ps_wait_open:
    case ps_idle_server:
        set_error( STATUS_PIPE_LISTENING );
        break;

    case ps_disconnected_server:
    case ps_wait_connect:
        set_error( STATUS_PIPE_DISCONNECTED );
        break;
    }
    return NULL;
}


static void notify_empty( struct pipe_server *server )
{
    if (!server->flush_poll)
        return;
    assert( server->state == ps_connected_server );
    assert( server->event );
    remove_timeout_user( server->flush_poll );
    server->flush_poll = NULL;
    set_event( server->event );
    release_object( server->event );
    server->event = NULL;
}

static void do_disconnect( struct pipe_server *server )
{
    /* we may only have a server fd, if the client disconnected */
    if (server->client)
    {
        assert( server->client->server == server );
        assert( server->client->fd );
        release_object( server->client->fd );
        server->client->fd = NULL;
    }
    assert( server->fd );
    release_object( server->fd );
    server->fd = NULL;
}

static unsigned int pipe_map_access( struct object *obj, unsigned int access )
{
    if (access & GENERIC_READ)    access |= FILE_GENERIC_READ;
    if (access & GENERIC_WRITE)   access |= FILE_GENERIC_WRITE;
    if (access & GENERIC_EXECUTE) access |= FILE_GENERIC_EXECUTE;
    if (access & GENERIC_ALL)     access |= FILE_ALL_ACCESS;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static void pipe_server_destroy( struct object *obj)
{
    struct pipe_server *server = (struct pipe_server *)obj;

    assert( obj->ops == &pipe_server_ops );

    if (server->fd)
    {
        notify_empty( server );
        do_disconnect( server );
    }

    if (server->client)
    {
        server->client->server = NULL;
        server->client = NULL;
    }

    async_terminate_head( &server->wait_q, STATUS_HANDLES_CLOSED );

    assert( server->pipe->instances );
    server->pipe->instances--;

    list_remove( &server->entry );
    release_object( server->pipe );
}

static void pipe_client_destroy( struct object *obj)
{
    struct pipe_client *client = (struct pipe_client *)obj;
    struct pipe_server *server = client->server;

    assert( obj->ops == &pipe_client_ops );

    if (server)
    {
        notify_empty( server );

        switch(server->state)
        {
        case ps_connected_server:
            /* Don't destroy the server's fd here as we can't
               do a successful flush without it. */
            server->state = ps_wait_disconnect;
            release_object( client->fd );
            client->fd = NULL;
            break;
        case ps_disconnected_server:
            server->state = ps_wait_connect;
            break;
        case ps_idle_server:
        case ps_wait_open:
        case ps_wait_disconnect:
        case ps_wait_connect:
            assert( 0 );
        }
        assert( server->client );
        server->client = NULL;
        client->server = NULL;
    }
    assert( !client->fd );
}

static void named_pipe_device_dump( struct object *obj, int verbose )
{
    assert( obj->ops == &named_pipe_device_ops );
    fprintf( stderr, "Named pipe device\n" );
}

static struct fd *named_pipe_device_get_fd( struct object *obj )
{
    struct named_pipe_device *device = (struct named_pipe_device *)obj;
    return (struct fd *)grab_object( device->fd );
}

static struct object *named_pipe_device_lookup_name( struct object *obj, struct unicode_str *name,
                                                     unsigned int attr )
{
    struct named_pipe_device *device = (struct named_pipe_device*)obj;
    struct object *found;

    assert( obj->ops == &named_pipe_device_ops );
    assert( device->pipes );

    if ((found = find_object( device->pipes, name, attr | OBJ_CASE_INSENSITIVE )))
        name->len = 0;

    return found;
}

static void named_pipe_device_destroy( struct object *obj )
{
    struct named_pipe_device *device = (struct named_pipe_device*)obj;
    assert( obj->ops == &named_pipe_device_ops );
    if (device->fd) release_object( device->fd );
    if (device->pipes) free( device->pipes );
}

static int named_pipe_device_get_file_info( struct fd *fd )
{
    return 0;
}

void create_named_pipe_device( struct directory *root, const struct unicode_str *name )
{
    struct named_pipe_device *dev;

    if ((dev = create_named_object_dir( root, name, 0, &named_pipe_device_ops )) &&
        get_error() != STATUS_OBJECT_NAME_EXISTS)
    {
        dev->pipes = NULL;
        if (!(dev->fd = alloc_pseudo_fd( &named_pipe_device_fd_ops, &dev->obj )) ||
            !(dev->pipes = create_namespace( 7 )))
        {
            release_object( dev );
            dev = NULL;
        }
    }
    if (dev) make_object_static( &dev->obj );
}

static int pipe_data_remaining( struct pipe_server *server )
{
    struct pollfd pfd;
    int fd;

    assert( server->client );

    fd = get_unix_fd( server->client->fd );
    if (fd < 0)
        return 0;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if (0 > poll( &pfd, 1, 0 ))
        return 0;
 
    return pfd.revents&POLLIN;
}

static void check_flushed( void *arg )
{
    struct pipe_server *server = (struct pipe_server*) arg;

    assert( server->event );
    if (pipe_data_remaining( server ))
    {
        struct timeval tv;

        gettimeofday( &tv, NULL );
        add_timeout( &tv, 100 );
        server->flush_poll = add_timeout_user( &tv, check_flushed, server );
    }
    else
    {
        /* notify_empty( server ); */
        server->flush_poll = NULL;
        set_event( server->event );
        release_object( server->event );
        server->event = NULL;
    }
}

static int pipe_server_flush( struct fd *fd, struct event **event )
{
    struct pipe_server *server = get_fd_user( fd );

    if (!server)
        return 0;

    if (server->state != ps_connected_server)
        return 0;

    /* FIXME: if multiple threads flush the same pipe,
              maybe should create a list of processes to notify */
    if (server->flush_poll)
        return 0;

    if (pipe_data_remaining( server ))
    {
        struct timeval tv;

        /* this kind of sux - 
           there's no unix way to be alerted when a pipe becomes empty */
        server->event = create_event( NULL, NULL, 0, 0, 0 );
        if (!server->event)
            return 0;
        gettimeofday( &tv, NULL );
        add_timeout( &tv, 100 );
        server->flush_poll = add_timeout_user( &tv, check_flushed, server );
        *event = server->event;
    }

    return 0; 
}

static int pipe_client_flush( struct fd *fd, struct event **event )
{
    /* FIXME: what do we have to do for this? */
    return 0;
}

static inline int is_overlapped( unsigned int options )
{
    return !(options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT));
}

static int pipe_server_get_info( struct fd *fd )
{
    struct pipe_server *server = get_fd_user( fd );
    int flags = FD_FLAG_AVAILABLE;
 
    if (is_overlapped( server->options )) flags |= FD_FLAG_OVERLAPPED;

    return flags;
}

static int pipe_client_get_info( struct fd *fd )
{
    struct pipe_client *client = get_fd_user( fd );
    int flags = FD_FLAG_AVAILABLE;

    if (is_overlapped( client->flags )) flags |= FD_FLAG_OVERLAPPED;

    return flags;
}

static struct named_pipe *create_named_pipe( struct directory *root, const struct unicode_str *name,
                                             unsigned int attr )
{
    struct object *obj;
    struct named_pipe *pipe = NULL;
    struct unicode_str new_name;

    if (!name || !name->len) return alloc_object( &named_pipe_ops );

    if (!(obj = find_object_dir( root, name, attr, &new_name ))) return NULL;
    if (!new_name.len)
    {
        if (attr & OBJ_OPENIF && obj->ops == &named_pipe_ops)
            set_error( STATUS_OBJECT_NAME_EXISTS );
        else
        {
            release_object( obj );
            obj = NULL;
            if (attr & OBJ_OPENIF)
                set_error( STATUS_OBJECT_TYPE_MISMATCH );
            else
                set_error( STATUS_OBJECT_NAME_COLLISION );
        }
        return (struct named_pipe *)obj;
    }

    if (obj->ops != &named_pipe_device_ops)
        set_error( STATUS_OBJECT_TYPE_MISMATCH );
    else
    {
        struct named_pipe_device *dev = (struct named_pipe_device *)obj;
        if ((pipe = create_object( dev->pipes, &named_pipe_ops, &new_name, NULL )))
            clear_error();
    }

    release_object( obj );
    return pipe;
}

static struct pipe_server *get_pipe_server_obj( struct process *process,
                                obj_handle_t handle, unsigned int access )
{
    struct object *obj;
    obj = get_handle_obj( process, handle, access, &pipe_server_ops );
    return (struct pipe_server *) obj;
}

static struct pipe_server *create_pipe_server( struct named_pipe *pipe, unsigned int options )
{
    struct pipe_server *server;

    server = alloc_object( &pipe_server_ops );
    if (!server)
        return NULL;

    server->fd = NULL;
    server->pipe = pipe;
    server->state = ps_idle_server;
    server->client = NULL;
    server->flush_poll = NULL;
    server->options = options;
    list_init( &server->wait_q );

    list_add_head( &pipe->servers, &server->entry );
    grab_object( pipe );

    return server;
}

static struct pipe_client *create_pipe_client( unsigned int flags )
{
    struct pipe_client *client;

    client = alloc_object( &pipe_client_ops );
    if (!client)
        return NULL;

    client->fd = NULL;
    client->server = NULL;
    client->flags = flags;

    return client;
}

static inline struct pipe_server *find_server( struct named_pipe *pipe, enum pipe_state state )
{
    struct pipe_server *server;

    LIST_FOR_EACH_ENTRY( server, &pipe->servers, struct pipe_server, entry )
    {
        if (server->state == state) return (struct pipe_server *)grab_object( server );
    }
    return NULL;
}

static inline struct pipe_server *find_server2( struct named_pipe *pipe,
                                                enum pipe_state state1, enum pipe_state state2 )
{
    struct pipe_server *server;

    LIST_FOR_EACH_ENTRY( server, &pipe->servers, struct pipe_server, entry )
    {
        if (server->state == state1 || server->state == state2)
            return (struct pipe_server *)grab_object( server );
    }
    return NULL;
}

DECL_HANDLER(create_named_pipe)
{
    struct named_pipe *pipe;
    struct pipe_server *server;
    struct unicode_str name;
    struct directory *root = NULL;

    reply->handle = 0;
    get_req_unicode_str( &name );
    if (req->rootdir && !(root = get_directory_obj( current->process, req->rootdir, 0 )))
        return;

    pipe = create_named_pipe( root, &name, req->attributes | OBJ_OPENIF );

    if (root) release_object( root );
    if (!pipe) return;

    if (get_error() != STATUS_OBJECT_NAME_EXISTS)
    {
        /* initialize it if it didn't already exist */
        pipe->instances = 0;
        list_init( &pipe->servers );
        list_init( &pipe->waiters );
        pipe->insize = req->insize;
        pipe->outsize = req->outsize;
        pipe->maxinstances = req->maxinstances;
        pipe->timeout = req->timeout;
        pipe->flags = req->flags;
    }
    else
    {
        if (pipe->maxinstances <= pipe->instances)
        {
            set_error( STATUS_INSTANCE_NOT_AVAILABLE );
            release_object( pipe );
            return;
        }
        if ((pipe->maxinstances != req->maxinstances) ||
            (pipe->timeout != req->timeout) ||
            (pipe->flags != req->flags))
        {
            set_error( STATUS_ACCESS_DENIED );
            release_object( pipe );
            return;
        }
        clear_error(); /* clear the name collision */
    }

    server = create_pipe_server( pipe, req->options );
    if (server)
    {
        reply->handle = alloc_handle( current->process, server, req->access, req->attributes );
        server->pipe->instances++;
        release_object( server );
    }

    release_object( pipe );
}

DECL_HANDLER(open_named_pipe)
{
    struct pipe_server *server;
    struct pipe_client *client;
    struct unicode_str name;
    struct directory *root = NULL;
    struct named_pipe *pipe;
    int fds[2];

    get_req_unicode_str( &name );
    if (req->rootdir && !(root = get_directory_obj( current->process, req->rootdir, 0 )))
        return;

    pipe = open_object_dir( root, &name, req->attributes, &named_pipe_ops );

    if (root) release_object( root );
    if (!pipe) return;

    server = find_server2( pipe, ps_idle_server, ps_wait_open );
    release_object( pipe );

    if (!server)
    {
        set_error( STATUS_PIPE_NOT_AVAILABLE );
        return;
    }

    client = create_pipe_client( req->flags );
    if (client)
    {
        if (!socketpair( PF_UNIX, SOCK_STREAM, 0, fds ))
        {
            int res = 0;

            assert( !client->fd );
            assert( !server->fd );

            /* for performance reasons, only set nonblocking mode when using
             * overlapped I/O. Otherwise, we will be doing too much busy
             * looping */
            if (is_overlapped( req->flags ))
                res = fcntl( fds[1], F_SETFL, O_NONBLOCK );
            if ((res != -1) && is_overlapped( server->options ))
                res = fcntl( fds[0], F_SETFL, O_NONBLOCK );

            client->fd = create_anonymous_fd( &pipe_client_fd_ops,
                                            fds[1], &client->obj );
            server->fd = create_anonymous_fd( &pipe_server_fd_ops,
                                            fds[0], &server->obj );
            if (client->fd && server->fd && res != 1)
            {
                if (server->state == ps_wait_open)
                    async_terminate_head( &server->wait_q, STATUS_SUCCESS );
                assert( list_empty( &server->wait_q ) );
                server->state = ps_connected_server;
                server->client = client;
                client->server = server;
                reply->handle = alloc_handle( current->process, client, req->access, req->attributes );
            }
        }
        else
            file_set_error();

        release_object( client );
    }
    release_object( server );
}

DECL_HANDLER(connect_named_pipe)
{
    struct pipe_server *server;

    server = get_pipe_server_obj(current->process, req->handle, 0);
    if (!server)
        return;

    switch(server->state)
    {
    case ps_idle_server:
    case ps_wait_connect:
        assert( !server->fd );
        server->state = ps_wait_open;
        create_async( current, NULL, &server->wait_q,
                      req->func, req->event, NULL );
        async_terminate_queue( &server->pipe->waiters, STATUS_SUCCESS );
        break;
    case ps_connected_server:
        assert( server->fd );
        set_error( STATUS_PIPE_CONNECTED );
        break;
    case ps_disconnected_server:
        set_error( STATUS_PIPE_BUSY );
        break;
    case ps_wait_disconnect:
        set_error( STATUS_NO_DATA_DETECTED );
        break;
    case ps_wait_open:
        set_error( STATUS_INVALID_HANDLE );
        break;
    }

    release_object(server);
}

DECL_HANDLER(wait_named_pipe)
{
    struct named_pipe_device *device;
    struct named_pipe *pipe;
    struct pipe_server *server;
    struct unicode_str name;

    device = (struct named_pipe_device *)get_handle_obj( current->process, req->handle,
                                                         FILE_READ_ATTRIBUTES, &named_pipe_device_ops );
    if (!device) return;

    get_req_unicode_str( &name );
    pipe = (struct named_pipe *)find_object( device->pipes, &name, OBJ_CASE_INSENSITIVE );
    release_object( device );
    if (!pipe)
    {
        set_error( STATUS_PIPE_NOT_AVAILABLE );
        return;
    }
    server = find_server( pipe, ps_wait_open );
    if (server)
    {
        /* there's already a server waiting for a client to connect */
        thread_queue_apc( current, NULL, req->func, APC_ASYNC_IO,
                          1, req->event, NULL, (void *)STATUS_SUCCESS );
        release_object( server );
    }
    else
    {
        if (req->timeout == NMPWAIT_WAIT_FOREVER)
            create_async( current, NULL, &pipe->waiters,
                          req->func, req->event, NULL );
        else
        {
            struct timeval when;

            gettimeofday( &when, NULL );
            if (req->timeout == NMPWAIT_USE_DEFAULT_WAIT) add_timeout( &when, pipe->timeout );
            else add_timeout( &when, req->timeout );
            create_async( current, &when, &pipe->waiters, req->func, req->event, NULL );
        }
    }

    release_object( pipe );
}

DECL_HANDLER(disconnect_named_pipe)
{
    struct pipe_server *server;

    reply->fd = -1;
    server = get_pipe_server_obj( current->process, req->handle, 0 );
    if (!server)
        return;
    switch(server->state)
    {
    case ps_connected_server:
        assert( server->fd );
        assert( server->client );
        assert( server->client->fd );

        notify_empty( server );

        /* Dump the client and server fds, but keep the pointers
           around - client loses all waiting data */
        server->state = ps_disconnected_server;
        do_disconnect( server );
        reply->fd = flush_cached_fd( current->process, req->handle );
        break;

    case ps_wait_disconnect:
        assert( !server->client );
        assert( server->fd );
        do_disconnect( server );
        server->state = ps_wait_connect;
        reply->fd = flush_cached_fd( current->process, req->handle );
        break;

    case ps_idle_server:
    case ps_wait_open:
    case ps_disconnected_server:
    case ps_wait_connect:
        set_error( STATUS_PIPE_DISCONNECTED );
        break;
    }
    release_object( server );
}

DECL_HANDLER(get_named_pipe_info)
{
    struct pipe_server *server;
    struct pipe_client *client = NULL;

    server = get_pipe_server_obj( current->process, req->handle, FILE_READ_ATTRIBUTES );
    if (!server)
    {
        clear_error();
        client = (struct pipe_client *)get_handle_obj( current->process, req->handle,
                                                       FILE_READ_ATTRIBUTES, &pipe_client_ops );
        if (!client) return;
        server = client->server;
    }

    reply->flags        = server->pipe->flags;
    reply->maxinstances = server->pipe->maxinstances;
    reply->instances    = server->pipe->instances;
    reply->insize       = server->pipe->insize;
    reply->outsize      = server->pipe->outsize;

    if (client)
        release_object(client);
    else
    {
        reply->flags |= NAMED_PIPE_SERVER_END;
        release_object(server);
    }
}
