/*
 * Thread pooling
 *
 * Copyright (c) 2006 Robert Shearman
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
#include "wine/port.h"

#include <stdarg.h>
#include <limits.h>

#define NONAMELESSUNION
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "wine/debug.h"
#include "wine/list.h"

#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(threadpool);

#define WORKER_TIMEOUT 30000 /* 30 seconds */

static LONG num_workers;
static LONG num_work_items;
static LONG num_busy_workers;

static struct list work_item_list = LIST_INIT(work_item_list);
static HANDLE work_item_event;

static CRITICAL_SECTION threadpool_cs;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &threadpool_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": threadpool_cs") }
};
static CRITICAL_SECTION threadpool_cs = { &critsect_debug, -1, 0, 0, 0, 0 };

struct work_item
{
    struct list entry;
    PRTL_WORK_ITEM_ROUTINE function;
    PVOID context;
};

inline static LONG interlocked_inc( PLONG dest )
{
    return interlocked_xchg_add( (int *)dest, 1 ) + 1;
}

inline static LONG interlocked_dec( PLONG dest )
{
    return interlocked_xchg_add( (int *)dest, -1 ) - 1;
}

static void WINAPI worker_thread_proc(void * param)
{
    interlocked_inc(&num_workers);

    /* free the work item memory sooner to reduce memory usage */
    while (TRUE)
    {
        if (num_work_items > 0)
        {
            struct list *item;
            RtlEnterCriticalSection(&threadpool_cs);
            item = list_head(&work_item_list);
            if (item)
            {
                struct work_item *work_item_ptr = LIST_ENTRY(item, struct work_item, entry);
                struct work_item work_item;
                list_remove(&work_item_ptr->entry);
                interlocked_dec(&num_work_items);

                RtlLeaveCriticalSection(&threadpool_cs);

                work_item = *work_item_ptr;
                RtlFreeHeap(GetProcessHeap(), 0, work_item_ptr);

                TRACE("executing %p(%p)\n", work_item.function, work_item.context);

                interlocked_inc(&num_busy_workers);

                /* do the work */
                work_item.function(work_item.context);

                interlocked_dec(&num_busy_workers);
            }
            else
                RtlLeaveCriticalSection(&threadpool_cs);
        }
        else
        {
            NTSTATUS status;
            LARGE_INTEGER timeout;
            timeout.QuadPart = -(WORKER_TIMEOUT * (ULONGLONG)10000);
            status = NtWaitForSingleObject(work_item_event, FALSE, &timeout);
            if (status != STATUS_WAIT_0)
                break;
        }
    }

    interlocked_dec(&num_workers);

    RtlExitUserThread(0);

    /* never reached */
}

static NTSTATUS add_work_item_to_queue(struct work_item *work_item)
{
    NTSTATUS status;

    RtlEnterCriticalSection(&threadpool_cs);
    list_add_tail(&work_item_list, &work_item->entry);
    num_work_items++;
    RtlLeaveCriticalSection(&threadpool_cs);

    if (!work_item_event)
    {
        HANDLE sem;
        status = NtCreateSemaphore(&sem, SEMAPHORE_ALL_ACCESS, NULL, 1, LONG_MAX);
        if (interlocked_cmpxchg_ptr( (PVOID *)&work_item_event, (PVOID)sem, 0 ))
            NtClose(sem);  /* somebody beat us to it */
    }
    else
        status = NtReleaseSemaphore(work_item_event, 1, NULL);

    return status;
}

/***********************************************************************
 *              RtlQueueWorkItem   (NTDLL.@)
 *
 * Queues a work item into a thread in the thread pool.
 *
 * PARAMS
 *  Function [I] Work function to execute.
 *  Context  [I] Context to pass to the work function when it is executed.
 *  Flags    [I] Flags. See notes.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: Any NTSTATUS code.
 *
 * NOTES
 *  Flags can be one or more of the following:
 *|WT_EXECUTEDEFAULT - Executes the work item in a non-I/O worker thread.
 *|WT_EXECUTEINIOTHREAD - Executes the work item in an I/O worker thread.
 *|WT_EXECUTEINPERSISTENTTHREAD - Executes the work item in a thread that is persistent.
 *|WT_EXECUTELONGFUNCTION - Hints that the execution can take a long time.
 *|WT_TRANSFER_IMPERSONATION - Executes the function with the current access token.
 */
NTSTATUS WINAPI RtlQueueWorkItem(PRTL_WORK_ITEM_ROUTINE Function, PVOID Context, ULONG Flags)
{
    HANDLE thread;
    NTSTATUS status;
    struct work_item *work_item = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(struct work_item));

    if (!work_item)
        return STATUS_NO_MEMORY;

    work_item->function = Function;
    work_item->context = Context;

    if (Flags != WT_EXECUTEDEFAULT)
        FIXME("Flags 0x%lx not supported\n", Flags);

    status = add_work_item_to_queue(work_item);

    if ((status == STATUS_SUCCESS) &&
        ((num_workers == 0) || (num_workers == num_busy_workers)))
    {
        status = RtlCreateUserThread( GetCurrentProcess(), NULL, FALSE,
                                    NULL, 0, 0,
                                    worker_thread_proc, NULL, &thread, NULL );
        if (status == STATUS_SUCCESS)
            NtClose( thread );

        /* NOTE: we don't care if we couldn't create the thread if there is at
         * least one other available to process the request */
        if ((num_workers > 0) && (status != STATUS_SUCCESS))
            status = STATUS_SUCCESS;
    }

    if (status != STATUS_SUCCESS)
    {
        RtlEnterCriticalSection(&threadpool_cs);

        interlocked_dec(&num_work_items);
        list_remove(&work_item->entry);
        RtlFreeHeap(GetProcessHeap(), 0, work_item);

        RtlLeaveCriticalSection(&threadpool_cs);

        return status;
    }

    return STATUS_SUCCESS;
}
