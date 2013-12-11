/*
 * Copyright 2006 Alexandre Julliard
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
#include "winreg.h"
#include "winerror.h"
#include "shellapi.h"
#include "shlwapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(url);

/***********************************************************************
 *		DllMain  (URL.@)
 */
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    switch(reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( inst );
        break;
    }
    return TRUE;
}

/***********************************************************************
 * InetIsOffline    (URL.@)
 *
 */
BOOL WINAPI InetIsOffline(void)
{
    FIXME("stub!\n");

    return FALSE;
}

/***********************************************************************
 * FileProtocolHandlerA    (URL.@)
 *
 * Handles a URL given to it and executes it.
 *
 * HWND hWnd - Parent Window
 * LPCSTR pszUrl - The URL that needs to be handled
 * int nShowCmd - How to display the operation.
 */

HRESULT WINAPI FileProtocolHandlerA(HWND hWnd, LPCSTR pszUrl,int nShowCmd)
{
    LPSTR pszPath = NULL;
    DWORD size = MAX_PATH;
    HRESULT createpath = PathCreateFromUrlA(pszUrl,pszPath,&size,0);

    TRACE("(%p, %p, %d)\n",hWnd,pszUrl,nShowCmd);

    if(createpath != S_OK)
        return E_FAIL;

    ShellExecuteA(hWnd,NULL,pszPath,NULL,NULL,nShowCmd);

    return S_OK;
}

/***********************************************************************
 * TelnetProtocolHandlerA    (URL.@)
 *
 */

HRESULT WINAPI TelnetProtocolHandlerA(HWND hWnd, LPSTR lpStr)
{
    FIXME("(%p, %p): stub!\n",hWnd,lpStr);

    return E_NOTIMPL;
}