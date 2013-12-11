/*
 * Direct3D 8
 *
 * Copyright 2005 Oliver Stieber
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
 */

#include "config.h"
#include "initguid.h"
#include "d3d8_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d8);

void (*wine_tsx11_lock_ptr)(void) = NULL;
void (*wine_tsx11_unlock_ptr)(void) = NULL;


HRESULT WINAPI D3D8GetSWInfo(void) {
    FIXME("(void): stub\n");
    return 0;
}

void WINAPI DebugSetMute(void) {
    /* nothing to do */
}

IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion) {
    IDirect3D8Impl* object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3D8Impl));

    object->lpVtbl = &Direct3D8_Vtbl;
    object->ref = 1;
    object->WineD3D = WineDirect3DCreate(SDKVersion, 8, (IUnknown *)object);

    TRACE("SDKVersion = %x, Created Direct3D object @ %p, WineObj @ %p\n", SDKVersion, object, object->WineD3D);

    return (IDirect3D8*) object;
}

/* At process attach */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv) {
    TRACE("fdwReason=%ld\n", fdwReason);
    if (fdwReason == DLL_PROCESS_ATTACH) {
        HMODULE mod;

        DisableThreadLibraryCalls(hInstDLL);

        mod = GetModuleHandleA( "winex11.drv" );
        if (mod)
        {
            wine_tsx11_lock_ptr   = (void*) GetProcAddress(mod, "wine_tsx11_lock");
            wine_tsx11_unlock_ptr = (void*) GetProcAddress(mod, "wine_tsx11_unlock");
        }
    }
    return TRUE;
}

/***********************************************************************
 *		ValidateVertexShader (D3D8.@)
 *
 * PARAMS
 * toto       result?
 */
BOOL WINAPI ValidateVertexShader(LPVOID pFunction, int param1, int param2, LPVOID toto)
{
  FIXME("(%p %d %d %p): stub\n", pFunction, param1, param2, toto);
  return TRUE;
}

/***********************************************************************
 *		ValidatePixelShader (D3D8.@)
 *
 * PARAMS
 * toto       result?
 */
BOOL WINAPI ValidatePixelShader(LPVOID pFunction, int param1, int param2, LPVOID toto)
{
  FIXME("(%p %d %d %p): stub\n", pFunction, param1, param2, toto);
  return TRUE;
}
