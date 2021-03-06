/*
 * Copyright 2000 Juergen Schmied
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

#ifndef __WINE_NTDLL_MISC_H
#define __WINE_NTDLL_MISC_H

#include <stdarg.h>

#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "winioctl.h"
#include "wine/server.h"

#define MAX_NT_PATH_LENGTH 277

/* exceptions */
extern void wait_suspend( CONTEXT *context );
extern void WINAPI __regs_RtlRaiseException( PEXCEPTION_RECORD, PCONTEXT );
extern void get_cpu_context( CONTEXT *context );
extern void set_cpu_context( const CONTEXT *context );

/* debug helper */
extern LPCSTR debugstr_us( const UNICODE_STRING *str );
extern void dump_ObjectAttributes (const OBJECT_ATTRIBUTES *ObjectAttributes);

extern void NTDLL_get_server_abstime( abs_time_t *when, const LARGE_INTEGER *timeout );
extern void NTDLL_from_server_abstime( LARGE_INTEGER *time, const abs_time_t *when );
extern NTSTATUS NTDLL_wait_for_multiple_objects( UINT count, const HANDLE *handles, UINT flags,
                                                 const LARGE_INTEGER *timeout, HANDLE signal_object );

/* init routines */
extern BOOL SIGNAL_Init(void);
extern size_t get_signal_stack_total_size(void);
extern void version_init( const WCHAR *appname );
extern void debug_init(void);
extern HANDLE thread_init(void);
extern void virtual_init(void);
extern void virtual_init_threading(void);

/* server support */
extern abs_time_t server_start_time;
extern void server_init_process(void);
extern size_t server_init_thread( int unix_pid, int unix_tid, void *entry_point );
extern void DECLSPEC_NORETURN server_protocol_error( const char *err, ... );
extern void DECLSPEC_NORETURN server_protocol_perror( const char *err );
extern void DECLSPEC_NORETURN server_exit_thread( int status );
extern void DECLSPEC_NORETURN server_abort_thread( int status );

/* module handling */
extern NTSTATUS MODULE_DllThreadAttach( LPVOID lpReserved );
extern FARPROC RELAY_GetProcAddress( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                     DWORD exp_size, FARPROC proc, DWORD ordinal, const WCHAR *user );
extern FARPROC SNOOP_GetProcAddress( HMODULE hmod, const IMAGE_EXPORT_DIRECTORY *exports, DWORD exp_size,
                                     FARPROC origfun, DWORD ordinal, const WCHAR *user );
extern void RELAY_SetupDLL( HMODULE hmod );
extern void SNOOP_SetupDLL( HMODULE hmod );
extern UNICODE_STRING system_dir;

/* redefine these to make sure we don't reference kernel symbols */
#define GetProcessHeap()       (NtCurrentTeb()->Peb->ProcessHeap)
#define GetCurrentProcessId()  ((DWORD)NtCurrentTeb()->ClientId.UniqueProcess)
#define GetCurrentThreadId()   ((DWORD)NtCurrentTeb()->ClientId.UniqueThread)

/* Device IO */
extern NTSTATUS CDROM_DeviceIoControl(HANDLE hDevice, 
                                      HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                      PVOID UserApcContext, 
                                      PIO_STATUS_BLOCK piosb, 
                                      ULONG IoControlCode,
                                      LPVOID lpInBuffer, DWORD nInBufferSize,
                                      LPVOID lpOutBuffer, DWORD nOutBufferSize);
extern NTSTATUS COMM_DeviceIoControl(HANDLE hDevice, 
                                     HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                     PVOID UserApcContext, 
                                     PIO_STATUS_BLOCK piosb, 
                                     ULONG IoControlCode,
                                     LPVOID lpInBuffer, DWORD nInBufferSize,
                                     LPVOID lpOutBuffer, DWORD nOutBufferSize);
extern NTSTATUS TAPE_DeviceIoControl(HANDLE hDevice, 
                                     HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                     PVOID UserApcContext, 
                                     PIO_STATUS_BLOCK piosb, 
                                     ULONG IoControlCode,
                                     LPVOID lpInBuffer, DWORD nInBufferSize,
                                     LPVOID lpOutBuffer, DWORD nOutBufferSize);

/* file I/O */
extern NTSTATUS FILE_GetNtStatus(void);
extern NTSTATUS FILE_GetDeviceInfo( int fd, FILE_FS_DEVICE_INFORMATION *info );
extern BOOL DIR_is_hidden_file( const UNICODE_STRING *name );
extern NTSTATUS DIR_unmount_device( HANDLE handle );
extern NTSTATUS DIR_get_unix_cwd( char **cwd );

/* virtual memory */
extern NTSTATUS VIRTUAL_HandleFault(LPCVOID addr);
extern BOOL VIRTUAL_HasMapping( LPCVOID addr );
extern void VIRTUAL_UseLargeAddressSpace(void);

extern BOOL is_current_process( HANDLE handle );

/* code pages */
extern int ntdll_umbstowcs(DWORD flags, const char* src, int srclen, WCHAR* dst, int dstlen);
extern int ntdll_wcstoumbs(DWORD flags, const WCHAR* src, int srclen, char* dst, int dstlen,
                           const char* defchar, int *used );

/* load order */

enum loadorder
{
    LO_INVALID,
    LO_DISABLED,
    LO_NATIVE,
    LO_BUILTIN,
    LO_NATIVE_BUILTIN,  /* native then builtin */
    LO_BUILTIN_NATIVE,  /* builtin then native */
    LO_DEFAULT          /* nothing specified, use default strategy */
};

extern enum loadorder get_load_order( const WCHAR *app_name, const WCHAR *path );

struct debug_info
{
    char *str_pos;       /* current position in strings buffer */
    char *out_pos;       /* current position in output buffer */
    char  strings[1024]; /* buffer for temporary strings */
    char  output[1024];  /* current output line */
};

struct ntdll_thread_data
{
    struct debug_info *debug_info;    /* info for debugstr functions */
    int                request_fd;    /* fd for sending server requests */
    int                reply_fd;      /* fd for receiving server replies */
    int                wait_fd[2];    /* fd for sleeping server requests */
    void              *vm86_ptr;      /* data for vm86 mode */

    void              *pad[4];        /* change this if you add fields! */
};

static inline struct ntdll_thread_data *ntdll_get_thread_data(void)
{
    return (struct ntdll_thread_data *)NtCurrentTeb()->SystemReserved2;
}

/* thread registers, stored in NtCurrentTeb()->SpareBytes1 */
struct ntdll_thread_regs
{
    DWORD              fs;            /* 00 TEB selector */
    DWORD              gs;            /* 04 libc selector; update winebuild if you move this! */
    DWORD              dr0;           /* 08 debug registers */
    DWORD              dr1;           /* 0c */
    DWORD              dr2;           /* 10 */
    DWORD              dr3;           /* 14 */
    DWORD              dr6;           /* 18 */
    DWORD              dr7;           /* 1c */
    DWORD              spare[2];      /* 20 change this if you add fields! */
};

static inline struct ntdll_thread_regs *ntdll_get_thread_regs(void)
{
    return (struct ntdll_thread_regs *)NtCurrentTeb()->SpareBytes1;
}

#endif
