/*
 * Sparc register context support
 *
 * Copyright (C) 2000 Ulrich Weigand
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
 */

#include "config.h"

#ifdef __sparc__

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_REG_H
# include <sys/reg.h>
#endif
#include <stdarg.h>
#include <unistd.h>
#ifdef HAVE_SYS_PTRACE_H
# include <sys/ptrace.h>
#endif

#include "windef.h"

#include "file.h"
#include "thread.h"
#include "request.h"

#if 0  /* no longer used */

#if defined(__sun) || defined(__sun__)

/* retrieve a thread context */
static void get_thread_context_ptrace( struct thread *thread, unsigned int flags, CONTEXT *context )
{
    int pid = get_ptrace_pid(thread);
    if (flags & CONTEXT_FULL)
    {
        struct regs regs;
        if (ptrace( PTRACE_GETREGS, pid, 0, (int) &regs ) == -1) goto error;
        if (flags & CONTEXT_INTEGER)
        {
            context->g0 = 0;
            context->g1 = regs.r_g1;
            context->g2 = regs.r_g2;
            context->g3 = regs.r_g3;
            context->g4 = regs.r_g4;
            context->g5 = regs.r_g5;
            context->g6 = regs.r_g6;
            context->g7 = regs.r_g7;

            context->o0 = regs.r_o0;
            context->o1 = regs.r_o1;
            context->o2 = regs.r_o2;
            context->o3 = regs.r_o3;
            context->o4 = regs.r_o4;
            context->o5 = regs.r_o5;
            context->o6 = regs.r_o6;
            context->o7 = regs.r_o7;

            /* FIXME: local and in registers */
        }
        if (flags & CONTEXT_CONTROL)
        {
            context->psr = regs.r_psr;
            context->pc  = regs.r_pc;
            context->npc = regs.r_npc;
            context->y   = regs.r_y;
            context->wim = 0;  /* FIXME */
            context->tbr = 0;  /* FIXME */
        }
        context |= flags & (CONTEXT_CONTROL|CONTEXT_INTEGER);
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        /* FIXME */
    }
    return;
 error:
    file_set_error();
}


/* set a thread context */
static void set_thread_context_ptrace( struct thread *thread, unsigned int flags, const CONTEXT *context )
{
    /* FIXME */
}

#else  /* __sun__ */
#error You must implement get/set_thread_context_ptrace for your platform
#endif  /* __sun__ */

#endif /* 0 */

/* copy a context structure according to the flags */
void copy_context( CONTEXT *to, const CONTEXT *from, unsigned int flags )
{
    flags &= ~CONTEXT_SPARC;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL)
    {
        to->psr    = from->psr;
        to->pc     = from->pc;
        to->npc    = from->npc;
        to->y      = from->y;
        to->wim    = from->wim;
        to->tbr    = from->tbr;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->g0 = from->g0;
        to->g1 = from->g1;
        to->g2 = from->g2;
        to->g3 = from->g3;
        to->g4 = from->g4;
        to->g5 = from->g5;
        to->g6 = from->g6;
        to->g7 = from->g7;
        to->o0 = from->o0;
        to->o1 = from->o1;
        to->o2 = from->o2;
        to->o3 = from->o3;
        to->o4 = from->o4;
        to->o5 = from->o5;
        to->o6 = from->o6;
        to->o7 = from->o7;
        to->l0 = from->l0;
        to->l1 = from->l1;
        to->l2 = from->l2;
        to->l3 = from->l3;
        to->l4 = from->l4;
        to->l5 = from->l5;
        to->l6 = from->l6;
        to->l7 = from->l7;
        to->i0 = from->i0;
        to->i1 = from->i1;
        to->i2 = from->i2;
        to->i3 = from->i3;
        to->i4 = from->i4;
        to->i5 = from->i5;
        to->i6 = from->i6;
        to->i7 = from->i7;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        /* FIXME */
    }
    context |= flags & (CONTEXT_CONTROL|CONTEXT_INTEGER);
}

/* retrieve the current instruction pointer of a context */
void *get_context_ip( const CONTEXT *context )
{
    return (void *)context->pc;
}

/* return the context flag that contains the CPU id */
unsigned int get_context_cpu_flag(void)
{
    return CONTEXT_SPARC;
}

/* return only the context flags that correspond to system regs */
/* (system regs are the ones we can't access on the client side) */
unsigned int get_context_system_regs( unsigned int flags )
{
    return 0;  /* FIXME: implement client-side handling */
}

#endif  /* __sparc__ */
