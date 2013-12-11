/*
 * DNS support
 *
 * Copyright (C) 2006 Matthew Kehrer
 * Copyright (C) 2006 Hans Leidekker
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "windns.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dnsapi);

HINSTANCE hdnsapi;

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    TRACE( "(%p, %ld, %p)\n", hinst, reason, reserved );

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        hdnsapi = hinst;
        DisableThreadLibraryCalls( hinst );
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

/******************************************************************************
 * DnsAcquireContextHandle_A              [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsAcquireContextHandle_A( DWORD flags, PVOID cred,
                                             HANDLE *context )
{
    FIXME( "(0x%08lx,%p,%p) stub\n", flags, cred, context );

    *context = (HANDLE)0xdeadbeef;
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsAcquireContextHandle_UTF8              [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsAcquireContextHandle_UTF8( DWORD flags, PVOID cred,
                                                HANDLE *context )
{
    FIXME( "(0x%08lx,%p,%p) stub\n", flags, cred, context );

    *context = (HANDLE)0xdeadbeef;
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsAcquireContextHandle_W              [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsAcquireContextHandle_W( DWORD flags, PVOID cred,
                                             HANDLE *context )
{
    FIXME( "(0x%08lx,%p,%p) stub\n", flags, cred, context );

    *context = (HANDLE)0xdeadbeef;
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsReleaseContextHandle                [DNSAPI.@]
 *
 */
void WINAPI DnsReleaseContextHandle( HANDLE context )
{
    FIXME( "(%p) stub\n", context );
}

/******************************************************************************
 * DnsExtractRecordsFromMessage_UTF8       [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsExtractRecordsFromMessage_UTF8( PDNS_MESSAGE_BUFFER buffer,
                                                     WORD len, PDNS_RECORDA *record )
{
    FIXME( "(%p,%d,%p) stub\n", buffer, len, record );

    *record = NULL;
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsExtractRecordsFromMessage_W          [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsExtractRecordsFromMessage_W( PDNS_MESSAGE_BUFFER buffer,
                                                  WORD len, PDNS_RECORDW *record )
{
    FIXME( "(%p,%d,%p) stub\n", buffer, len, record );

    *record = NULL;
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsModifyRecordsInSet_A                 [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsModifyRecordsInSet_A( PDNS_RECORDA add, PDNS_RECORDA delete,
                                           DWORD options, HANDLE context,
                                           PIP4_ARRAY servers, PVOID reserved )
{
    FIXME( "(%p,%p,0x%08lx,%p,%p,%p) stub\n", add, delete, options,
           context, servers, reserved );
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsModifyRecordsInSet_UTF8              [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsModifyRecordsInSet_UTF8( PDNS_RECORDA add, PDNS_RECORDA delete,
                                              DWORD options, HANDLE context,
                                              PIP4_ARRAY servers, PVOID reserved )
{
    FIXME( "(%p,%p,0x%08lx,%p,%p,%p) stub\n", add, delete, options,
           context, servers, reserved );
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsModifyRecordsInSet_W                 [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsModifyRecordsInSet_W( PDNS_RECORDW add, PDNS_RECORDW delete,
                                           DWORD options, HANDLE context,
                                           PIP4_ARRAY servers, PVOID reserved )
{
    FIXME( "(%p,%p,0x%08lx,%p,%p,%p) stub\n", add, delete, options,
           context, servers, reserved );
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsWriteQuestionToBuffer               [DNSAPI.@]
 *
 */
BOOL WINAPI DnsWriteQuestionToBuffer( PDNS_MESSAGE_BUFFER buffer, LPDWORD size,
                                      LPWSTR name, WORD type, WORD xid,
                                      BOOL recurse )
{
    FIXME( "(%p,%p,%s,%d,%d,%d) stub\n", buffer, size, debugstr_w(name),
           type, xid, recurse );
    return FALSE;
}

/******************************************************************************
 * DnsReplaceRecordSetA                    [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsReplaceRecordSetA( PDNS_RECORDA set, DWORD options,
                                        HANDLE context, PIP4_ARRAY servers,
                                        PVOID reserved )
{
    FIXME( "(%p,0x%08lx,%p,%p,%p) stub\n", set, options, context,
           servers, reserved );
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsReplaceRecordSetUTF8                 [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsReplaceRecordSetUTF8( PDNS_RECORDA set, DWORD options,
                                           HANDLE context, PIP4_ARRAY servers,
                                           PVOID reserved )
{
    FIXME( "(%p,0x%08lx,%p,%p,%p) stub\n", set, options, context,
           servers, reserved );
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DnsReplaceRecordSetW                    [DNSAPI.@]
 *
 */
DNS_STATUS WINAPI DnsReplaceRecordSetW( PDNS_RECORDW set, DWORD options,
                                        HANDLE context, PIP4_ARRAY servers,
                                        PVOID reserved )
{
    FIXME( "(%p,0x%08lx,%p,%p,%p) stub\n", set, options, context,
           servers, reserved );
    return ERROR_SUCCESS;
}
