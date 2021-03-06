/*
 * Copyright 2005 Kai Blin
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
 * A dispatcher to run ntlm_auth for wine's sspi module.
 */

#include "config.h"
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif 
#include <stdlib.h>
#include <fcntl.h>
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "sspi.h"
#include "secur32_priv.h"
#include "wine/debug.h"

#define INITIAL_BUFFER_SIZE 200

WINE_DEFAULT_DEBUG_CHANNEL(secur32);

SECURITY_STATUS fork_helper(PNegoHelper *new_helper, const char *prog,
        char* const argv[])
{
    int pipe_in[2];
    int pipe_out[2];
    int i;
    PNegoHelper helper;

    TRACE("%s ", debugstr_a(prog));
    for(i = 0; argv[i] != NULL; ++i)
    {
        TRACE("%s ", debugstr_a(argv[i]));
    }
    TRACE("\n");

    if( pipe(pipe_in) < 0 )
    {
        return SEC_E_INTERNAL_ERROR;
    }
    if( pipe(pipe_out) < 0 )
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        return SEC_E_INTERNAL_ERROR;
    }
    if (!(helper = HeapAlloc(GetProcessHeap(),0, sizeof(NegoHelper))))
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return SEC_E_INSUFFICIENT_MEMORY;
    }

    helper->helper_pid = fork();

    if(helper->helper_pid == -1)
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        HeapFree( GetProcessHeap(), 0, helper );
        return SEC_E_INTERNAL_ERROR;
    }

    if(helper->helper_pid == 0)
    {
        /* We're in the child now */
        close(0);
        close(1);

        dup2(pipe_out[0], 0);
        close(pipe_out[0]);
        close(pipe_out[1]);

        dup2(pipe_in[1], 1);
        close(pipe_in[0]);
        close(pipe_in[1]);

        execvp(prog, argv);

        /* Whoops, we shouldn't get here. Big badaboom.*/
        write(STDOUT_FILENO, "BH\n", 3);
        exit(1);
    }
    else
    {
        *new_helper = helper;
        helper->version = -1;
        helper->password = NULL;
        helper->com_buf = NULL;
        helper->com_buf_size = 0;
        helper->com_buf_offset = 0;
        helper->pipe_in = pipe_in[0];
        close(pipe_in[1]);
        helper->pipe_out = pipe_out[1];
        close(pipe_out[0]);
    }

    return SEC_E_OK;
}

static SECURITY_STATUS read_line(PNegoHelper helper, int *offset_len)
{
    char *newline;
    int read_size;
    
    if(helper->com_buf == NULL)
    {
        TRACE("Creating a new buffer for the helper\n");
        if((helper->com_buf = HeapAlloc(GetProcessHeap(), 0, INITIAL_BUFFER_SIZE)) == NULL)
            return SEC_E_INSUFFICIENT_MEMORY;
        
        /* Created a new buffer, size is INITIAL_BUFFER_SIZE, offset is 0 */
        helper->com_buf_size = INITIAL_BUFFER_SIZE;
        helper->com_buf_offset = 0;
    }

    do
    {
        TRACE("offset = %d, size = %d\n", helper->com_buf_offset, helper->com_buf_size);
        if(helper->com_buf_offset + INITIAL_BUFFER_SIZE > helper->com_buf_size)
        {
            /* increment buffer size in INITIAL_BUFFER_SIZE steps */
            char *buf = HeapReAlloc(GetProcessHeap(), 0, helper->com_buf,
                                    helper->com_buf_size + INITIAL_BUFFER_SIZE);
            TRACE("Resizing buffer!\n");
            if (!buf) return SEC_E_INSUFFICIENT_MEMORY;
            helper->com_buf_size += INITIAL_BUFFER_SIZE;
            helper->com_buf = buf;
        }
        if((read_size = read(helper->pipe_in, helper->com_buf + helper->com_buf_offset,
                    helper->com_buf_size - helper->com_buf_offset)) <= 0)
        {
            return SEC_E_INTERNAL_ERROR;
        }
        
        TRACE("read_size = %d, read: %s\n", read_size, 
                debugstr_a(helper->com_buf + helper->com_buf_offset));
        helper->com_buf_offset += read_size;
        newline = memchr(helper->com_buf, '\n', helper->com_buf_offset);
    }while(newline == NULL);

    /* Now, if there's a newline character, and we read more than that newline,
     * we have to store the offset so we can preserve the additional data.*/
    if( newline != helper->com_buf + helper->com_buf_offset)
    {
        TRACE("offset_len is calculated from %p - %p\n", 
                (helper->com_buf + helper->com_buf_offset), newline+1);
        /* the length of the offset is the number of chars after the newline */
        *offset_len = (helper->com_buf + helper->com_buf_offset) - (newline + 1);
    }
    else
    {
        *offset_len = 0;
    }
    
    *newline = '\0';

    return SEC_E_OK;
}

static SECURITY_STATUS preserve_unused(PNegoHelper helper, int offset_len)
{
    TRACE("offset_len = %d\n", offset_len);

    if(offset_len > 0)
    {
        memmove(helper->com_buf, helper->com_buf + helper->com_buf_offset, 
                offset_len);
        helper->com_buf_offset = offset_len;
    }
    else
    {
        helper->com_buf_offset = 0;
    }

    TRACE("helper->com_buf_offset was set to: %d\n", helper->com_buf_offset);
    return SEC_E_OK;
}

SECURITY_STATUS run_helper(PNegoHelper helper, char *buffer,
        unsigned int max_buflen, int *buflen)
{
    int offset_len;
    SECURITY_STATUS sec_status = SEC_E_OK;

    TRACE("In helper: sending %s\n", debugstr_a(buffer));

    /* buffer + '\n' */
    write(helper->pipe_out, buffer, lstrlenA(buffer));
    write(helper->pipe_out, "\n", 1);

    if((sec_status = read_line(helper, &offset_len)) != SEC_E_OK)
    {
        return sec_status;
    }
    
    TRACE("In helper: received %s\n", debugstr_a(helper->com_buf));
    *buflen = lstrlenA(helper->com_buf);

    if( *buflen > max_buflen)
    {   
        ERR("Buffer size too small(%d given, %d required) dropping data!\n",
                max_buflen, *buflen);
        return SEC_E_BUFFER_TOO_SMALL;
    }

    if( *buflen < 2 )
    {
        return SEC_E_ILLEGAL_MESSAGE;
    }

    if( (*buflen <= 3) && (strncmp(helper->com_buf, "BH", 2) == 0))
    {
        return SEC_E_INTERNAL_ERROR;
    }

    /* We only get ERR if the input size is too big. On a GENSEC error,
     * ntlm_auth will return BH */
    if(strncmp(helper->com_buf, "ERR", 3) == 0)
    {
        return SEC_E_INVALID_TOKEN;
    }

    memcpy(buffer, helper->com_buf, *buflen+1);

    sec_status = preserve_unused(helper, offset_len);
    
    return sec_status;
}

void cleanup_helper(PNegoHelper helper)
{

    TRACE("Killing helper %p\n", helper);
    if( (helper == NULL) || (helper->helper_pid == 0))
        return;
      
    HeapFree(GetProcessHeap(), 0, helper->password);
    HeapFree(GetProcessHeap(), 0, helper->com_buf);

    /* closing stdin will terminate ntlm_auth */
    close(helper->pipe_out);
    close(helper->pipe_in);

    waitpid(helper->helper_pid, NULL, 0);

    helper->helper_pid = 0;
    HeapFree(GetProcessHeap(), 0, helper);
}

void check_version(PNegoHelper helper)
{
    char temp[80];
    char *newline;

    TRACE("Checking version of helper\n");
    if(helper != NULL)
    {
        int len = read(helper->pipe_in, temp, sizeof(temp)-1);
        if (len > 8)
        {
            if((newline = memchr(temp, '\n', len)) != NULL)
                *newline = '\0';
            else
                temp[len] = 0;

            TRACE("Exact version is %s\n", debugstr_a(temp));
            if(strncmp(temp+8, "4", 1) == 0)
            {
                helper->version = 4;
            }
            else if(strncmp(temp+8, "3", 1) == 0)
            {
                helper->version = 3;
            }
            else
            {
                TRACE("Unknown version!\n");
                helper->version = -1;
            }
        }
    }
}
