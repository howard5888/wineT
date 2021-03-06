/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2004 Mike McCormack for CodeWeavers
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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "wine/debug.h"
#include "msi.h"
#include "msiquery.h"
#include "msipriv.h"
#include "objidl.h"
#include "winnls.h"
#include "ole2.h"

#include "winreg.h"
#include "shlwapi.h"

#include "query.h"

WINE_DEFAULT_DEBUG_CHANNEL(msidb);

#define MSIFIELD_NULL   0
#define MSIFIELD_INT    1
#define MSIFIELD_STR    2
#define MSIFIELD_WSTR   3
#define MSIFIELD_STREAM 4

static void MSI_FreeField( MSIFIELD *field )
{
    switch( field->type )
    {
    case MSIFIELD_NULL:
    case MSIFIELD_INT:
        break;
    case MSIFIELD_WSTR:
        msi_free( field->u.szwVal);
        break;
    case MSIFIELD_STREAM:
        IStream_Release( field->u.stream );
        break;
    default:
        ERR("Invalid field type %d\n", field->type);
    }
}

static void MSI_CloseRecord( MSIOBJECTHDR *arg )
{
    MSIRECORD *rec = (MSIRECORD *) arg;
    UINT i;

    for( i=0; i<=rec->count; i++ )
        MSI_FreeField( &rec->fields[i] );
}

MSIRECORD *MSI_CreateRecord( unsigned int cParams )
{
    MSIRECORD *rec;
    UINT len;

    TRACE("%d\n", cParams);

    if( cParams>65535 )
        return NULL;

    len = sizeof (MSIRECORD) + sizeof (MSIFIELD)*cParams;
    rec = alloc_msiobject( MSIHANDLETYPE_RECORD, len, MSI_CloseRecord );
    if( rec )
        rec->count = cParams;
    return rec;
}

MSIHANDLE WINAPI MsiCreateRecord( unsigned int cParams )
{
    MSIRECORD *rec;
    MSIHANDLE ret = 0;

    TRACE("%d\n", cParams);

    rec = MSI_CreateRecord( cParams );
    if( rec )
    {
        ret = alloc_msihandle( &rec->hdr );
        msiobj_release( &rec->hdr );
    }
    return ret;
}

unsigned int MSI_RecordGetFieldCount( MSIRECORD *rec )
{
    return rec->count;
}

unsigned int WINAPI MsiRecordGetFieldCount( MSIHANDLE handle )
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld\n", handle );

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return -1;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordGetFieldCount( rec );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );

    return ret;
}

static BOOL string2intW( LPCWSTR str, int *out )
{
    int x = 0;
    LPCWSTR p = str;

    if( *p == '-' ) /* skip the minus sign */
        p++;
    while ( *p )
    {
        if( (*p < '0') || (*p > '9') )
            return FALSE;
        x *= 10;
        x += (*p - '0');
        p++;
    }

    if( str[0] == '-' ) /* check if it's negative */
        x = -x;
    *out = x; 

    return TRUE;
}

int MSI_RecordGetInteger( MSIRECORD *rec, unsigned int iField)
{
    int ret = 0;

    TRACE("%p %d\n", rec, iField );

    if( iField > rec->count )
        return MSI_NULL_INTEGER;

    switch( rec->fields[iField].type )
    {
    case MSIFIELD_INT:
        return rec->fields[iField].u.iVal;
    case MSIFIELD_WSTR:
        if( string2intW( rec->fields[iField].u.szwVal, &ret ) )
            return ret;
        return MSI_NULL_INTEGER;
    default:
        break;
    }

    return MSI_NULL_INTEGER;
}

int WINAPI MsiRecordGetInteger( MSIHANDLE handle, unsigned int iField)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d\n", handle, iField );

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return MSI_NULL_INTEGER;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordGetInteger( rec, iField );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );

    return ret;
}

UINT WINAPI MsiRecordClearData( MSIHANDLE handle )
{
    MSIRECORD *rec;
    UINT i;

    TRACE("%ld\n", handle );

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;

    msiobj_lock( &rec->hdr );
    for( i=0; i<=rec->count; i++)
    {
        MSI_FreeField( &rec->fields[i] );
        rec->fields[i].type = MSIFIELD_NULL;
        rec->fields[i].u.iVal = 0;
    }
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );

    return ERROR_SUCCESS;
}

UINT MSI_RecordSetInteger( MSIRECORD *rec, unsigned int iField, int iVal )
{
    TRACE("%p %u %d\n", rec, iField, iVal);

    if( iField > rec->count )
        return ERROR_INVALID_PARAMETER;
 
    MSI_FreeField( &rec->fields[iField] );
    rec->fields[iField].type = MSIFIELD_INT;
    rec->fields[iField].u.iVal = iVal;

    return ERROR_SUCCESS;
}

UINT WINAPI MsiRecordSetInteger( MSIHANDLE handle, unsigned int iField, int iVal )
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %u %d\n", handle, iField, iVal);

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordSetInteger( rec, iField, iVal );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

BOOL MSI_RecordIsNull( MSIRECORD *rec, unsigned int iField )
{
    BOOL r = TRUE;

    TRACE("%p %d\n", rec, iField );

    r = ( iField > rec->count ) ||
        ( rec->fields[iField].type == MSIFIELD_NULL );

    return r;
}

BOOL WINAPI MsiRecordIsNull( MSIHANDLE handle, unsigned int iField )
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d\n", handle, iField );

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return 0;
    msiobj_lock( &rec->hdr );
    ret = MSI_RecordIsNull( rec, iField );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;

}

UINT MSI_RecordGetStringA(MSIRECORD *rec, unsigned int iField, 
               LPSTR szValue, DWORD *pcchValue)
{
    UINT len=0, ret;
    CHAR buffer[16];

    TRACE("%p %d %p %p\n", rec, iField, szValue, pcchValue);

    if( iField > rec->count )
        return ERROR_INVALID_PARAMETER;

    ret = ERROR_SUCCESS;
    switch( rec->fields[iField].type )
    {
    case MSIFIELD_INT:
        wsprintfA(buffer, "%d", rec->fields[iField].u.iVal);
        len = lstrlenA( buffer );
        lstrcpynA(szValue, buffer, *pcchValue);
        break;
    case MSIFIELD_WSTR:
        len = WideCharToMultiByte( CP_ACP, 0, rec->fields[iField].u.szwVal, -1,
                             NULL, 0 , NULL, NULL);
        WideCharToMultiByte( CP_ACP, 0, rec->fields[iField].u.szwVal, -1,
                             szValue, *pcchValue, NULL, NULL);
        if( *pcchValue && len>*pcchValue )
            szValue[*pcchValue-1] = 0;
        if( len )
            len--;
        break;
    case MSIFIELD_NULL:
        if( *pcchValue > 0 )
            szValue[0] = 0;
        break;
    default:
        ret = ERROR_INVALID_PARAMETER;
        break;
    }

    if( *pcchValue <= len )
        ret = ERROR_MORE_DATA;
    *pcchValue = len;

    return ret;
}

UINT WINAPI MsiRecordGetStringA(MSIHANDLE handle, unsigned int iField, 
               LPSTR szValue, DWORD *pcchValue)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %p %p\n", handle, iField, szValue, pcchValue);

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;
    msiobj_lock( &rec->hdr );
    ret = MSI_RecordGetStringA( rec, iField, szValue, pcchValue);
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

const WCHAR *MSI_RecordGetString( MSIRECORD *rec, unsigned int iField )
{
    if( iField > rec->count )
        return NULL;

    if( rec->fields[iField].type != MSIFIELD_WSTR )
        return NULL;

    return rec->fields[iField].u.szwVal;
}

UINT MSI_RecordGetStringW(MSIRECORD *rec, unsigned int iField,
               LPWSTR szValue, DWORD *pcchValue)
{
    UINT len=0, ret;
    WCHAR buffer[16];
    static const WCHAR szFormat[] = { '%','d',0 };

    TRACE("%p %d %p %p\n", rec, iField, szValue, pcchValue);

    if( iField > rec->count )
        return ERROR_INVALID_PARAMETER;

    ret = ERROR_SUCCESS;
    switch( rec->fields[iField].type )
    {
    case MSIFIELD_INT:
        wsprintfW(buffer, szFormat, rec->fields[iField].u.iVal);
        len = lstrlenW( buffer );
        lstrcpynW(szValue, buffer, *pcchValue);
        break;
    case MSIFIELD_WSTR:
        len = lstrlenW( rec->fields[iField].u.szwVal );
        lstrcpynW(szValue, rec->fields[iField].u.szwVal, *pcchValue);
        break;
    case MSIFIELD_NULL:
        len = 1;
        if( *pcchValue > 0 )
            szValue[0] = 0;
    default:
        break;
    }

    if( *pcchValue <= len )
        ret = ERROR_MORE_DATA;
    *pcchValue = len;

    return ret;
}

UINT WINAPI MsiRecordGetStringW(MSIHANDLE handle, unsigned int iField,
               LPWSTR szValue, DWORD *pcchValue)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %p %p\n", handle, iField, szValue, pcchValue);

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordGetStringW( rec, iField, szValue, pcchValue );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

static UINT msi_get_stream_size( IStream *stm )
{
    STATSTG stat;
    HRESULT r;

    r = IStream_Stat( stm, &stat, STATFLAG_NONAME );
    if( FAILED(r) )
        return 0;
    return stat.cbSize.QuadPart;
}

UINT MSI_RecordDataSize(MSIRECORD *rec, unsigned int iField)
{
    TRACE("%p %d\n", rec, iField);

    if( iField > rec->count )
        return 0;

    switch( rec->fields[iField].type )
    {
    case MSIFIELD_INT:
        return sizeof (INT);
    case MSIFIELD_WSTR:
        return lstrlenW( rec->fields[iField].u.szwVal );
    case MSIFIELD_NULL:
        break;
    case MSIFIELD_STREAM:
        return msi_get_stream_size( rec->fields[iField].u.stream );
    }
    return 0;
}

UINT WINAPI MsiRecordDataSize(MSIHANDLE handle, unsigned int iField)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d\n", handle, iField);

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return 0;
    msiobj_lock( &rec->hdr );
    ret = MSI_RecordDataSize( rec, iField);
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

UINT MSI_RecordSetStringA( MSIRECORD *rec, unsigned int iField, LPCSTR szValue )
{
    LPWSTR str;

    TRACE("%p %d %s\n", rec, iField, debugstr_a(szValue));

    if( iField > rec->count )
        return ERROR_INVALID_FIELD;

    MSI_FreeField( &rec->fields[iField] );
    if( szValue && szValue[0] )
    {
        str = strdupAtoW( szValue );
        rec->fields[iField].type = MSIFIELD_WSTR;
        rec->fields[iField].u.szwVal = str;
    }
    else
    {
        rec->fields[iField].type = MSIFIELD_NULL;
        rec->fields[iField].u.szwVal = NULL;
    }

    return 0;
}

UINT WINAPI MsiRecordSetStringA( MSIHANDLE handle, unsigned int iField, LPCSTR szValue )
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %s\n", handle, iField, debugstr_a(szValue));

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;
    msiobj_lock( &rec->hdr );
    ret = MSI_RecordSetStringA( rec, iField, szValue );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

UINT MSI_RecordSetStringW( MSIRECORD *rec, unsigned int iField, LPCWSTR szValue )
{
    LPWSTR str;

    TRACE("%p %d %s\n", rec, iField, debugstr_w(szValue));

    if( iField > rec->count )
        return ERROR_INVALID_FIELD;

    MSI_FreeField( &rec->fields[iField] );

    if( szValue && szValue[0] )
    {
        str = strdupW( szValue );
        rec->fields[iField].type = MSIFIELD_WSTR;
        rec->fields[iField].u.szwVal = str;
    }
    else
    {
        rec->fields[iField].type = MSIFIELD_NULL;
        rec->fields[iField].u.szwVal = NULL;
    }

    return 0;
}

UINT WINAPI MsiRecordSetStringW( MSIHANDLE handle, unsigned int iField, LPCWSTR szValue )
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %s\n", handle, iField, debugstr_w(szValue));

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordSetStringW( rec, iField, szValue );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

/* read the data in a file into an IStream */
static UINT RECORD_StreamFromFile(LPCWSTR szFile, IStream **pstm)
{
    DWORD sz, szHighWord = 0, read;
    HANDLE handle;
    HGLOBAL hGlob = 0;
    HRESULT hr;
    ULARGE_INTEGER ulSize;

    TRACE("reading %s\n", debugstr_w(szFile));

    /* read the file into memory */
    handle = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if( handle == INVALID_HANDLE_VALUE )
        return GetLastError();
    sz = GetFileSize(handle, &szHighWord);
    if( sz != INVALID_FILE_SIZE && szHighWord == 0 )
    {
        hGlob = GlobalAlloc(GMEM_FIXED, sz);
        if( hGlob )
        {
            BOOL r = ReadFile(handle, hGlob, sz, &read, NULL);
            if( !r )
            {
                GlobalFree(hGlob);
                hGlob = 0;
            }
        }
    }
    CloseHandle(handle);
    if( !hGlob )
        return ERROR_FUNCTION_FAILED;

    /* make a stream out of it, and set the correct file size */
    hr = CreateStreamOnHGlobal(hGlob, TRUE, pstm);
    if( FAILED( hr ) )
    {
        GlobalFree(hGlob);
        return ERROR_FUNCTION_FAILED;
    }

    /* set the correct size - CreateStreamOnHGlobal screws it up */
    ulSize.QuadPart = sz;
    IStream_SetSize(*pstm, ulSize);

    TRACE("read %s, %ld bytes into IStream %p\n", debugstr_w(szFile), sz, *pstm);

    return ERROR_SUCCESS;
}

UINT MSI_RecordSetStreamW(MSIRECORD *rec, unsigned int iField, LPCWSTR szFilename)
{
    IStream *stm = NULL;
    HRESULT r;

    if( (iField == 0) || (iField > rec->count) )
        return ERROR_INVALID_PARAMETER;

    /* no filename means we should seek back to the start of the stream */
    if( !szFilename )
    {
        LARGE_INTEGER ofs;
        ULARGE_INTEGER cur;

        if( rec->fields[iField].type != MSIFIELD_STREAM )
            return ERROR_INVALID_FIELD;

        stm = rec->fields[iField].u.stream;
        if( !stm )
            return ERROR_INVALID_FIELD;

        ofs.QuadPart = 0;
        r = IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        if( FAILED( r ) )
            return ERROR_FUNCTION_FAILED;
    }
    else
    {
        /* read the file into a stream and save the stream in the record */
        r = RECORD_StreamFromFile(szFilename, &stm);
        if( r != ERROR_SUCCESS )
            return r;

        /* if all's good, store it in the record */
        MSI_FreeField( &rec->fields[iField] );
        rec->fields[iField].type = MSIFIELD_STREAM;
        rec->fields[iField].u.stream = stm;
    }

    return ERROR_SUCCESS;
}

UINT WINAPI MsiRecordSetStreamA(MSIHANDLE hRecord, unsigned int iField, LPCSTR szFilename)
{
    LPWSTR wstr = NULL;
    UINT ret;

    TRACE("%ld %d %s\n", hRecord, iField, debugstr_a(szFilename));

    if( szFilename )
    {
        wstr = strdupAtoW( szFilename );
        if( !wstr )
             return ERROR_OUTOFMEMORY;
    }
    ret = MsiRecordSetStreamW(hRecord, iField, wstr);
    msi_free(wstr);

    return ret;
}

UINT WINAPI MsiRecordSetStreamW(MSIHANDLE handle, unsigned int iField, LPCWSTR szFilename)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %s\n", handle, iField, debugstr_w(szFilename));

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;

    msiobj_lock( &rec->hdr );
    ret = MSI_RecordSetStreamW( rec, iField, szFilename );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

UINT MSI_RecordReadStream(MSIRECORD *rec, unsigned int iField, char *buf, DWORD *sz)
{
    ULONG count;
    HRESULT r;
    IStream *stm;

    TRACE("%p %d %p %p\n", rec, iField, buf, sz);

    if( !sz )
        return ERROR_INVALID_PARAMETER;

    if( iField > rec->count)
        return ERROR_INVALID_PARAMETER;

    if( rec->fields[iField].type != MSIFIELD_STREAM )
        return ERROR_INVALID_DATATYPE;

    stm = rec->fields[iField].u.stream;
    if( !stm )
        return ERROR_INVALID_PARAMETER;

    /* if there's no buffer pointer, calculate the length to the end */
    if( !buf )
    {
        LARGE_INTEGER ofs;
        ULARGE_INTEGER end, cur;

        ofs.QuadPart = cur.QuadPart = 0;
        end.QuadPart = 0;
        r = IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        IStream_Seek( stm, ofs, STREAM_SEEK_END, &end );
        ofs.QuadPart = cur.QuadPart;
        IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        *sz = end.QuadPart - cur.QuadPart;

        return ERROR_SUCCESS;
    }

    /* read the data */
    count = 0;
    r = IStream_Read( stm, buf, *sz, &count );
    if( FAILED( r ) )
    {
        *sz = 0;
        return ERROR_FUNCTION_FAILED;
    }

    *sz = count;

    return ERROR_SUCCESS;
}

UINT WINAPI MsiRecordReadStream(MSIHANDLE handle, unsigned int iField, char *buf, DWORD *sz)
{
    MSIRECORD *rec;
    UINT ret;

    TRACE("%ld %d %p %p\n", handle, iField, buf, sz);

    rec = msihandle2msiinfo( handle, MSIHANDLETYPE_RECORD );
    if( !rec )
        return ERROR_INVALID_HANDLE;
    msiobj_lock( &rec->hdr );
    ret = MSI_RecordReadStream( rec, iField, buf, sz );
    msiobj_unlock( &rec->hdr );
    msiobj_release( &rec->hdr );
    return ret;
}

UINT MSI_RecordSetIStream( MSIRECORD *rec, unsigned int iField, IStream *stm )
{
    TRACE("%p %d %p\n", rec, iField, stm);

    if( iField > rec->count )
        return ERROR_INVALID_FIELD;

    MSI_FreeField( &rec->fields[iField] );

    rec->fields[iField].type = MSIFIELD_STREAM;
    rec->fields[iField].u.stream = stm;
    IStream_AddRef( stm );

    return ERROR_SUCCESS;
}

UINT MSI_RecordGetIStream( MSIRECORD *rec, unsigned int iField, IStream **pstm)
{
    TRACE("%p %d %p\n", rec, iField, pstm);

    if( iField > rec->count )
        return ERROR_INVALID_FIELD;

    if( rec->fields[iField].type != MSIFIELD_STREAM )
        return ERROR_INVALID_FIELD;

    *pstm = rec->fields[iField].u.stream;
    IStream_AddRef( *pstm );

    return ERROR_SUCCESS;
}

static UINT msi_dump_stream_to_file( IStream *stm, LPCWSTR name )
{
    ULARGE_INTEGER size;
    LARGE_INTEGER pos;
    IStream *out;
    DWORD stgm;
    HRESULT r;

    stgm = STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_FAILIFTHERE;
    r = SHCreateStreamOnFileW( name, stgm, &out );
    if( FAILED( r ) )
        return ERROR_FUNCTION_FAILED;

    pos.QuadPart = 0;
    r = IStream_Seek( stm, pos, STREAM_SEEK_END, &size );
    if( FAILED( r ) )
        goto end;

    pos.QuadPart = 0;
    r = IStream_Seek( stm, pos, STREAM_SEEK_SET, NULL );
    if( FAILED( r ) )
        goto end;

    r = IStream_CopyTo( stm, out, size, NULL, NULL );

end:
    IStream_Release( out );
    if( FAILED( r ) )
        return ERROR_FUNCTION_FAILED;
    return ERROR_SUCCESS;
}

UINT MSI_RecordStreamToFile( MSIRECORD *rec, unsigned int iField, LPCWSTR name )
{
    IStream *stm = NULL;
    UINT r;

    TRACE("%p %u %s\n", rec, iField, debugstr_w(name));

    msiobj_lock( &rec->hdr );

    r = MSI_RecordGetIStream( rec, iField, &stm );
    if( r == ERROR_SUCCESS )
    {
        r = msi_dump_stream_to_file( stm, name );
        IStream_Release( stm );
    }

    msiobj_unlock( &rec->hdr );

    return r;
}
