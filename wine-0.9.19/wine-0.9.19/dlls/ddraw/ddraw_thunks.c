/* Direct Draw Thunks and old vtables
 * Copyright 2000 TransGaming Technologies Inc.
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

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "wingdi.h"
#include "wine/exception.h"
#include "excpt.h"

#include "ddraw.h"
#include "d3d.h"

#include "ddraw_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ddraw_thunk);
WINE_DECLARE_DEBUG_CHANNEL(ddraw);

static HRESULT WINAPI
IDirectDrawImpl_QueryInterface(LPDIRECTDRAW This, REFIID iid, LPVOID *ppObj)
{
    return IDirectDraw7_QueryInterface(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw,
							  IDirectDraw7, This),
				       iid, ppObj);
}

static HRESULT WINAPI
IDirectDraw2Impl_QueryInterface(LPDIRECTDRAW2 This, REFIID iid, LPVOID *ppObj)
{
    return IDirectDraw7_QueryInterface(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw2,
							  IDirectDraw7, This),
				       iid, ppObj);
}


static HRESULT WINAPI
IDirectDraw4Impl_QueryInterface(LPDIRECTDRAW4 This, REFIID iid, LPVOID *ppObj)
{
    return IDirectDraw7_QueryInterface(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw4,
							  IDirectDraw7, This),
				       iid, ppObj);
}

static ULONG WINAPI
IDirectDrawImpl_AddRef(LPDIRECTDRAW iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw, iface);
    ULONG ref = InterlockedIncrement(&This->ref1);

    TRACE("(%p) : incrementing IDirectDraw refcount from %lu.\n", This, ref -1);

    if(ref == 1) InterlockedIncrement(&This->numIfaces);

    return ref;
}

static ULONG WINAPI
IDirectDraw2Impl_AddRef(LPDIRECTDRAW2 iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw2, iface);
    ULONG ref = InterlockedIncrement(&This->ref2);

    TRACE("(%p) : incrementing IDirectDraw2 refcount from %lu.\n", This, ref -1);

    if(ref == 1) InterlockedIncrement(&This->numIfaces);

    return ref;
}

static ULONG WINAPI
IDirectDraw4Impl_AddRef(LPDIRECTDRAW4 iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw4, iface);
    ULONG ref = InterlockedIncrement(&This->ref4);

    TRACE("(%p) : incrementing IDirectDraw4 refcount from %lu.\n", This, ref -1);

    if(ref == 1) InterlockedIncrement(&This->numIfaces);

    return ref;
}

static ULONG WINAPI
IDirectDrawImpl_Release(LPDIRECTDRAW iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw, iface);
    ULONG ref = InterlockedDecrement(&This->ref1);

    TRACE_(ddraw)("(%p)->() decrementing IDirectDraw refcount from %lu.\n", This, ref +1);

    if(ref == 0)
    {
        ULONG ifacecount = InterlockedDecrement(&This->numIfaces);
        if(ifacecount == 0) IDirectDrawImpl_Destroy(This);
    }

    return ref;
}

static ULONG WINAPI
IDirectDraw2Impl_Release(LPDIRECTDRAW2 iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw2, iface);
    ULONG ref = InterlockedDecrement(&This->ref2);

    TRACE_(ddraw)("(%p)->() decrementing IDirectDraw2 refcount from %lu.\n", This, ref +1);

    if(ref == 0)
    {
        ULONG ifacecount = InterlockedDecrement(&This->numIfaces);
        if(ifacecount == 0) IDirectDrawImpl_Destroy(This);
    }

    return ref;
}

static ULONG WINAPI
IDirectDraw4Impl_Release(LPDIRECTDRAW4 iface)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw4, iface);
    ULONG ref = InterlockedDecrement(&This->ref4);

    TRACE_(ddraw)("(%p)->() decrementing IDirectDraw4 refcount from %lu.\n", This, ref +1);

    if(ref == 0)
    {
        ULONG ifacecount = InterlockedDecrement(&This->numIfaces);
        if(ifacecount == 0) IDirectDrawImpl_Destroy(This);
    }

    return ref;
}

static HRESULT WINAPI
IDirectDrawImpl_Compact(LPDIRECTDRAW This)
{
    return IDirectDraw7_Compact(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw, IDirectDraw7,
						   This));
}

static HRESULT WINAPI
IDirectDraw2Impl_Compact(LPDIRECTDRAW2 This)
{
    return IDirectDraw7_Compact(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw2, IDirectDraw7,
						   This));
}

static HRESULT WINAPI
IDirectDraw4Impl_Compact(LPDIRECTDRAW4 This)
{
    return IDirectDraw7_Compact(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw4, IDirectDraw7,
						   This));
}

static HRESULT WINAPI
IDirectDrawImpl_CreateClipper(LPDIRECTDRAW This, DWORD dwFlags,
			      LPDIRECTDRAWCLIPPER *ppClipper,
			      IUnknown *pUnkOuter)
{
    return IDirectDraw7_CreateClipper(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw,
							 IDirectDraw7,
							 This),
				      dwFlags, ppClipper, pUnkOuter);
}

static HRESULT WINAPI
IDirectDraw2Impl_CreateClipper(LPDIRECTDRAW2 This, DWORD dwFlags,
			       LPDIRECTDRAWCLIPPER *ppClipper,
			       IUnknown *pUnkOuter)
{
    return IDirectDraw7_CreateClipper(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw2,
							 IDirectDraw7,
							 This),
				      dwFlags, ppClipper, pUnkOuter);
}

static HRESULT WINAPI
IDirectDraw4Impl_CreateClipper(LPDIRECTDRAW4 This, DWORD dwFlags,
			       LPDIRECTDRAWCLIPPER *ppClipper,
			       IUnknown *pUnkOuter)
{
    return IDirectDraw7_CreateClipper(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw4,
							 IDirectDraw7,
							 This),
				      dwFlags, ppClipper, pUnkOuter);
}

static HRESULT WINAPI
IDirectDrawImpl_CreatePalette(LPDIRECTDRAW This, DWORD dwFlags,
			      LPPALETTEENTRY pEntries,
			      LPDIRECTDRAWPALETTE *ppPalette,
			      IUnknown *pUnkOuter)
{
    HRESULT hr;
    hr = IDirectDraw7_CreatePalette(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw,
							 IDirectDraw7,
							 This),
				      dwFlags, pEntries, ppPalette, pUnkOuter);
    if(SUCCEEDED(hr) && *ppPalette)
    {
        IDirectDrawPaletteImpl *impl = ICOM_OBJECT(IDirectDrawPaletteImpl, IDirectDrawPalette, *ppPalette);
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw,
                             IDirectDraw7,
                             This));
        impl->ifaceToRelease = NULL;

    }
    return hr;
}

static HRESULT WINAPI
IDirectDraw2Impl_CreatePalette(LPDIRECTDRAW2 This, DWORD dwFlags,
			       LPPALETTEENTRY pEntries,
			       LPDIRECTDRAWPALETTE *ppPalette,
			       IUnknown *pUnkOuter)
{
    HRESULT hr;
    hr = IDirectDraw7_CreatePalette(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw2,
							 IDirectDraw7,
							 This),
				      dwFlags, pEntries, ppPalette, pUnkOuter);
    if(SUCCEEDED(hr) && *ppPalette)
    {
        IDirectDrawPaletteImpl *impl = ICOM_OBJECT(IDirectDrawPaletteImpl, IDirectDrawPalette, *ppPalette);
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw2,
                             IDirectDraw7,
                             This));
        impl->ifaceToRelease = NULL;
    }
    return hr;
}

static HRESULT WINAPI
IDirectDraw4Impl_CreatePalette(LPDIRECTDRAW4 This, DWORD dwFlags,
			       LPPALETTEENTRY pEntries,
			       LPDIRECTDRAWPALETTE *ppPalette,
			       IUnknown *pUnkOuter)
{
    HRESULT hr;
    hr = IDirectDraw7_CreatePalette(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw4,
							 IDirectDraw7,
							 This),
				      dwFlags, pEntries, ppPalette, pUnkOuter);
    if(SUCCEEDED(hr) && *ppPalette)
    {
        IDirectDrawPaletteImpl *impl = ICOM_OBJECT(IDirectDrawPaletteImpl, IDirectDrawPalette, *ppPalette);
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw4,
                             IDirectDraw7,
                             This));
        IDirectDraw4_AddRef(This);
        impl->ifaceToRelease = (IUnknown *) This;
    }
    return hr;
}

static HRESULT WINAPI
IDirectDrawImpl_CreateSurface(LPDIRECTDRAW This, LPDDSURFACEDESC pSDesc,
			      LPDIRECTDRAWSURFACE *ppSurface,
			      IUnknown *pUnkOuter)
{
    LPDIRECTDRAWSURFACE7 pSurface7;
    IDirectDrawSurfaceImpl *impl;
    HRESULT hr;

    /* the LPDDSURFACEDESC -> LPDDSURFACEDESC2 conversion should be ok,
     * since the data layout is the same */
    hr = IDirectDraw7_CreateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw,
						       IDirectDraw7,
						       This),
				    (LPDDSURFACEDESC2)pSDesc, &pSurface7, pUnkOuter);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppSurface = (LPDIRECTDRAWSURFACE) COM_INTERFACE_CAST(IDirectDrawSurfaceImpl,
				    IDirectDrawSurface7, IDirectDrawSurface3,
				    pSurface7);

    impl = ICOM_OBJECT(IDirectDrawSurfaceImpl, IDirectDrawSurface7, pSurface7);
    if(SUCCEEDED(hr) && impl)
    {
        impl->version = 1;
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw,
                             IDirectDraw7,
                             This));
        IDirectDraw_AddRef(This);
        impl->ifaceToRelease = (IUnknown *) This;
    }

    return hr;
}

static HRESULT WINAPI
IDirectDraw2Impl_CreateSurface(LPDIRECTDRAW2 This, LPDDSURFACEDESC pSDesc,
			       LPDIRECTDRAWSURFACE *ppSurface,
			       IUnknown *pUnkOuter)
{
    LPDIRECTDRAWSURFACE7 pSurface7;
    IDirectDrawSurfaceImpl *impl;
    HRESULT hr;

    hr = IDirectDraw7_CreateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw2,
						       IDirectDraw7,
						       This),
				    (LPDDSURFACEDESC2)pSDesc, &pSurface7, pUnkOuter);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppSurface = (LPDIRECTDRAWSURFACE)COM_INTERFACE_CAST(IDirectDrawSurfaceImpl,
				    IDirectDrawSurface7, IDirectDrawSurface3,
				    pSurface7);

    impl = ICOM_OBJECT(IDirectDrawSurfaceImpl, IDirectDrawSurface7, pSurface7);
    if(SUCCEEDED(hr) && impl)
    {
        impl->version = 2;
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw2,
                             IDirectDraw7,
                             This));
        IDirectDraw2_AddRef(This);
        impl->ifaceToRelease = (IUnknown *) This;
    }

    return hr;
}

static HRESULT WINAPI
IDirectDraw4Impl_CreateSurface(LPDIRECTDRAW4 This, LPDDSURFACEDESC2 pSDesc,
			       LPDIRECTDRAWSURFACE4 *ppSurface,
			       IUnknown *pUnkOuter)
{
    HRESULT hr;
    IDirectDrawSurfaceImpl *impl;

    hr = IDirectDraw7_CreateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw4,
						       IDirectDraw7,
						        This),
				    pSDesc,
				    (LPDIRECTDRAWSURFACE7 *)ppSurface,
				    pUnkOuter);
    impl = ICOM_OBJECT(IDirectDrawSurfaceImpl, IDirectDrawSurface7, *ppSurface);
    if(SUCCEEDED(hr) && impl)
    {
        impl->version = 4;
        IDirectDraw7_Release(COM_INTERFACE_CAST(IDirectDrawImpl,
                             IDirectDraw4,
                             IDirectDraw7,
                             This));
        IDirectDraw4_AddRef(This);
        impl->ifaceToRelease = (IUnknown *) This;
    }
    return hr;
}

static HRESULT WINAPI
IDirectDrawImpl_DuplicateSurface(LPDIRECTDRAW This, LPDIRECTDRAWSURFACE pSrc,
				 LPDIRECTDRAWSURFACE *ppDst)
{
    LPDIRECTDRAWSURFACE7 pDst7;
    HRESULT hr;

    hr = IDirectDraw7_DuplicateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw,
							  IDirectDraw7, This),
				       COM_INTERFACE_CAST(IDirectDrawSurfaceImpl,
							  IDirectDrawSurface3,
							  IDirectDrawSurface7,
							  pSrc),
				       &pDst7);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppDst = (LPDIRECTDRAWSURFACE)COM_INTERFACE_CAST(IDirectDrawSurfaceImpl, IDirectDrawSurface7,
				IDirectDrawSurface3, pDst7);

    return hr;
}

static HRESULT WINAPI
IDirectDraw2Impl_DuplicateSurface(LPDIRECTDRAW2 This, LPDIRECTDRAWSURFACE pSrc,
				  LPDIRECTDRAWSURFACE *ppDst)
{
    LPDIRECTDRAWSURFACE7 pDst7;
    HRESULT hr;

    hr = IDirectDraw7_DuplicateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw2,
							  IDirectDraw7, This),
				       COM_INTERFACE_CAST(IDirectDrawSurfaceImpl,
							  IDirectDrawSurface3,
							  IDirectDrawSurface7,
							  pSrc),
				       &pDst7);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppDst = (LPDIRECTDRAWSURFACE)COM_INTERFACE_CAST(IDirectDrawSurfaceImpl, IDirectDrawSurface7,
				IDirectDrawSurface3, pDst7);

    return hr;
}

static HRESULT WINAPI
IDirectDraw4Impl_DuplicateSurface(LPDIRECTDRAW4 This,
				  LPDIRECTDRAWSURFACE4 pSrc,
				  LPDIRECTDRAWSURFACE4 *ppDst)
{
    return IDirectDraw7_DuplicateSurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw4,
							    IDirectDraw7,
							    This),
					 (LPDIRECTDRAWSURFACE7)pSrc,
					 (LPDIRECTDRAWSURFACE7 *)ppDst);
}

struct displaymodescallback_context
{
    LPDDENUMMODESCALLBACK func;
    LPVOID context;
};

static HRESULT CALLBACK
EnumDisplayModesCallbackThunk(LPDDSURFACEDESC2 pDDSD2, LPVOID context)
{
    DDSURFACEDESC DDSD;
    struct displaymodescallback_context *cbcontext = context;

    memcpy(&DDSD,pDDSD2,sizeof(DDSD));
    DDSD.dwSize = sizeof(DDSD);

    return cbcontext->func(&DDSD, cbcontext->context);
}

static HRESULT WINAPI
IDirectDrawImpl_EnumDisplayModes(LPDIRECTDRAW This, DWORD dwFlags,
				 LPDDSURFACEDESC pDDSD, LPVOID context,
				 LPDDENUMMODESCALLBACK cb)
{
    struct displaymodescallback_context cbcontext;

    cbcontext.func    = cb;
    cbcontext.context = context;

    return IDirectDraw7_EnumDisplayModes(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw,
							    IDirectDraw7,
							    This),
					 dwFlags, (LPDDSURFACEDESC2)pDDSD, &cbcontext,
					 EnumDisplayModesCallbackThunk);
}

static HRESULT WINAPI
IDirectDraw2Impl_EnumDisplayModes(LPDIRECTDRAW2 This, DWORD dwFlags,
				  LPDDSURFACEDESC pDDSD, LPVOID context,
				  LPDDENUMMODESCALLBACK cb)
{
    struct displaymodescallback_context cbcontext;

    cbcontext.func    = cb;
    cbcontext.context = context;

    return IDirectDraw7_EnumDisplayModes(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw2,
							    IDirectDraw7,
							    This),
					 dwFlags, (LPDDSURFACEDESC2)pDDSD, &cbcontext,
					 EnumDisplayModesCallbackThunk);
}

static HRESULT WINAPI
IDirectDraw4Impl_EnumDisplayModes(LPDIRECTDRAW4 This, DWORD dwFlags,
				  LPDDSURFACEDESC2 pDDSD, LPVOID context,
				  LPDDENUMMODESCALLBACK2 cb)
{
    return IDirectDraw7_EnumDisplayModes(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw4,
							    IDirectDraw7,
							    This),
					 dwFlags, pDDSD, context, cb);
}

struct surfacescallback_context
{
    LPDDENUMSURFACESCALLBACK func;
    LPVOID context;
};

static HRESULT CALLBACK
EnumSurfacesCallbackThunk(LPDIRECTDRAWSURFACE7 pSurf, LPDDSURFACEDESC2 pDDSD,
			  LPVOID context)
{
    struct surfacescallback_context *cbcontext = context;

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    return cbcontext->func((LPDIRECTDRAWSURFACE)
                           COM_INTERFACE_CAST(IDirectDrawSurfaceImpl,
					      IDirectDrawSurface7,
					      IDirectDrawSurface3, pSurf),
			   (LPDDSURFACEDESC)pDDSD, cbcontext->context);
}

static HRESULT WINAPI
IDirectDrawImpl_EnumSurfaces(LPDIRECTDRAW This, DWORD dwFlags,
			     LPDDSURFACEDESC pDDSD, LPVOID context,
			     LPDDENUMSURFACESCALLBACK cb)
{
    struct surfacescallback_context cbcontext;

    cbcontext.func    = cb;
    cbcontext.context = context;

    return IDirectDraw7_EnumSurfaces(COM_INTERFACE_CAST(IDirectDrawImpl,
							IDirectDraw,
							IDirectDraw7, This),
				     dwFlags, (LPDDSURFACEDESC2)pDDSD,
				     &cbcontext, EnumSurfacesCallbackThunk);
}

static HRESULT WINAPI
IDirectDraw2Impl_EnumSurfaces(LPDIRECTDRAW2 This, DWORD dwFlags,
			      LPDDSURFACEDESC pDDSD, LPVOID context,
			      LPDDENUMSURFACESCALLBACK cb)
{
    struct surfacescallback_context cbcontext;

    cbcontext.func    = cb;
    cbcontext.context = context;

    return IDirectDraw7_EnumSurfaces(COM_INTERFACE_CAST(IDirectDrawImpl,
							IDirectDraw2,
							IDirectDraw7, This),
				     dwFlags, (LPDDSURFACEDESC2)pDDSD,
				     &cbcontext, EnumSurfacesCallbackThunk);
}

static HRESULT WINAPI
IDirectDraw4Impl_EnumSurfaces(LPDIRECTDRAW4 This, DWORD dwFlags,
			      LPDDSURFACEDESC2 pDDSD, LPVOID context,
			      LPDDENUMSURFACESCALLBACK2 cb)
{
    return IDirectDraw7_EnumSurfaces(COM_INTERFACE_CAST(IDirectDrawImpl,
							IDirectDraw4,
							IDirectDraw7, This),
				     dwFlags, pDDSD, context,
				     (LPDDENUMSURFACESCALLBACK7)cb);
}

static HRESULT WINAPI
IDirectDrawImpl_FlipToGDISurface(LPDIRECTDRAW This)
{
    return IDirectDraw7_FlipToGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw,
							    IDirectDraw7,
							    This));
}

static HRESULT WINAPI
IDirectDraw2Impl_FlipToGDISurface(LPDIRECTDRAW2 This)
{
    return IDirectDraw7_FlipToGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw2,
							    IDirectDraw7,
							    This));
}

static HRESULT WINAPI
IDirectDraw4Impl_FlipToGDISurface(LPDIRECTDRAW4 This)
{
    return IDirectDraw7_FlipToGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw4,
							    IDirectDraw7,
							    This));
}

static HRESULT WINAPI
IDirectDrawImpl_GetCaps(LPDIRECTDRAW This, LPDDCAPS pDDC1, LPDDCAPS pDDC2)
{
    return IDirectDraw7_GetCaps(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw, IDirectDraw7,
						   This), pDDC1, pDDC2);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetCaps(LPDIRECTDRAW2 This, LPDDCAPS pDDC1, LPDDCAPS pDDC2)
{
    return IDirectDraw7_GetCaps(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw2, IDirectDraw7,
						   This), pDDC1, pDDC2);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetCaps(LPDIRECTDRAW4 This, LPDDCAPS pDDC1, LPDDCAPS pDDC2)
{
    return IDirectDraw7_GetCaps(COM_INTERFACE_CAST(IDirectDrawImpl,
						   IDirectDraw4, IDirectDraw7,
						   This), pDDC1, pDDC2);
}

static HRESULT WINAPI
IDirectDrawImpl_GetDisplayMode(LPDIRECTDRAW This, LPDDSURFACEDESC pDDSD)
{
    return IDirectDraw7_GetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw,
							  IDirectDraw7, This),
				       (LPDDSURFACEDESC2)pDDSD);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetDisplayMode(LPDIRECTDRAW2 This, LPDDSURFACEDESC pDDSD)
{
    return IDirectDraw7_GetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw2,
							  IDirectDraw7, This),
				       (LPDDSURFACEDESC2)pDDSD);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetDisplayMode(LPDIRECTDRAW4 This, LPDDSURFACEDESC2 pDDSD)
{
    return IDirectDraw7_GetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw4,
							  IDirectDraw7, This),
				       pDDSD);
}

static HRESULT WINAPI
IDirectDrawImpl_GetFourCCCodes(LPDIRECTDRAW This, LPDWORD lpNumCodes,
			       LPDWORD lpCodes)
{
    return IDirectDraw7_GetFourCCCodes(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw,
							  IDirectDraw7,
							  This),
				       lpNumCodes, lpCodes);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetFourCCCodes(LPDIRECTDRAW2 This, LPDWORD lpNumCodes,
				LPDWORD lpCodes)
{
    return IDirectDraw7_GetFourCCCodes(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw2,
							  IDirectDraw7,
							  This),
				       lpNumCodes, lpCodes);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetFourCCCodes(LPDIRECTDRAW4 This, LPDWORD lpNumCodes,
				LPDWORD lpCodes)
{
    return IDirectDraw7_GetFourCCCodes(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw4,
							  IDirectDraw7,
							  This),
				       lpNumCodes, lpCodes);
}

static HRESULT WINAPI
IDirectDrawImpl_GetGDISurface(LPDIRECTDRAW This, LPDIRECTDRAWSURFACE *ppSurf)
{
    LPDIRECTDRAWSURFACE7 pSurf7;
    HRESULT hr;

    hr = IDirectDraw7_GetGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw,
						       IDirectDraw7,
						       This), &pSurf7);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppSurf = (LPDIRECTDRAWSURFACE)COM_INTERFACE_CAST(IDirectDrawSurfaceImpl, IDirectDrawSurface7,
				 IDirectDrawSurface3, pSurf7);

    return hr;
}

static HRESULT WINAPI
IDirectDraw2Impl_GetGDISurface(LPDIRECTDRAW2 This, LPDIRECTDRAWSURFACE *ppSurf)
{
    LPDIRECTDRAWSURFACE7 pSurf7;
    HRESULT hr;

    hr = IDirectDraw7_GetGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw2,
						       IDirectDraw7,
						       This), &pSurf7);

    /* This coercion is safe, since the IDirectDrawSurface3 vtable has the
     * IDirectDrawSurface vtable layout at the beginning  */
    *ppSurf = (LPDIRECTDRAWSURFACE)COM_INTERFACE_CAST(IDirectDrawSurfaceImpl, IDirectDrawSurface7,
				 IDirectDrawSurface3, pSurf7);

    return hr;
}

static HRESULT WINAPI
IDirectDraw4Impl_GetGDISurface(LPDIRECTDRAW4 This,
			       LPDIRECTDRAWSURFACE4 *ppSurf)
{
    return IDirectDraw7_GetGDISurface(COM_INTERFACE_CAST(IDirectDrawImpl,
							 IDirectDraw4,
							 IDirectDraw7,
							 This),
				      (LPDIRECTDRAWSURFACE7 *)ppSurf);
}

static HRESULT WINAPI
IDirectDrawImpl_GetMonitorFrequency(LPDIRECTDRAW This, LPDWORD pdwFreq)
{
    return IDirectDraw7_GetMonitorFrequency(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw,
							       IDirectDraw7,
							       This),
					    pdwFreq);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetMonitorFrequency(LPDIRECTDRAW2 This, LPDWORD pdwFreq)
{
    return IDirectDraw7_GetMonitorFrequency(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw2,
							       IDirectDraw7,
							       This),
					    pdwFreq);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetMonitorFrequency(LPDIRECTDRAW4 This, LPDWORD pdwFreq)
{
    return IDirectDraw7_GetMonitorFrequency(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw4,
							       IDirectDraw7,
							       This),
					    pdwFreq);
}

static HRESULT WINAPI
IDirectDrawImpl_GetScanLine(LPDIRECTDRAW This, LPDWORD pdwLine)
{
    return IDirectDraw7_GetScanLine(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw,
						       IDirectDraw7,
						       This), pdwLine);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetScanLine(LPDIRECTDRAW2 This, LPDWORD pdwLine)
{
    return IDirectDraw7_GetScanLine(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw2,
						       IDirectDraw7,
						       This), pdwLine);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetScanLine(LPDIRECTDRAW4 This, LPDWORD pdwLine)
{
    return IDirectDraw7_GetScanLine(COM_INTERFACE_CAST(IDirectDrawImpl,
						       IDirectDraw4,
						       IDirectDraw7,
						       This), pdwLine);
}

static HRESULT WINAPI
IDirectDrawImpl_GetVerticalBlankStatus(LPDIRECTDRAW This, LPBOOL lpbIsInVB)
{
    return IDirectDraw7_GetVerticalBlankStatus(COM_INTERFACE_CAST(IDirectDrawImpl,
								  IDirectDraw,
								  IDirectDraw7,
								  This),
					       lpbIsInVB);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetVerticalBlankStatus(LPDIRECTDRAW2 This, LPBOOL lpbIsInVB)
{
    return IDirectDraw7_GetVerticalBlankStatus(COM_INTERFACE_CAST(IDirectDrawImpl,
								  IDirectDraw2,
								  IDirectDraw7,
								  This),
					       lpbIsInVB);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetVerticalBlankStatus(LPDIRECTDRAW4 This, LPBOOL lpbIsInVB)
{
    return IDirectDraw7_GetVerticalBlankStatus(COM_INTERFACE_CAST(IDirectDrawImpl,
								  IDirectDraw4,
								  IDirectDraw7,
								  This),
					       lpbIsInVB);
}

static HRESULT WINAPI
IDirectDrawImpl_Initialize(LPDIRECTDRAW iface, LPGUID pGUID)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw, iface);
    HRESULT ret_value;

    ret_value = IDirectDraw7_Initialize(ICOM_INTERFACE(This, IDirectDraw7), pGUID);

    return ret_value;
}

static HRESULT WINAPI
IDirectDraw2Impl_Initialize(LPDIRECTDRAW2 iface, LPGUID pGUID)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw2, iface);
    HRESULT ret_value;

    ret_value = IDirectDraw7_Initialize(ICOM_INTERFACE(This, IDirectDraw7), pGUID);

    return ret_value;
}

static HRESULT WINAPI
IDirectDraw4Impl_Initialize(LPDIRECTDRAW4 iface, LPGUID pGUID)
{
    ICOM_THIS_FROM(IDirectDrawImpl, IDirectDraw4, iface);
    HRESULT ret_value;

    ret_value = IDirectDraw7_Initialize(ICOM_INTERFACE(This, IDirectDraw7), pGUID);

    return ret_value;
}


static HRESULT WINAPI
IDirectDrawImpl_RestoreDisplayMode(LPDIRECTDRAW This)
{
    return IDirectDraw7_RestoreDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw,
							      IDirectDraw7,
							      This));
}

static HRESULT WINAPI
IDirectDraw2Impl_RestoreDisplayMode(LPDIRECTDRAW2 This)
{
    return IDirectDraw7_RestoreDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw2,
							      IDirectDraw7,
							      This));
}

static HRESULT WINAPI
IDirectDraw4Impl_RestoreDisplayMode(LPDIRECTDRAW4 This)
{
    return IDirectDraw7_RestoreDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw4,
							      IDirectDraw7,
							      This));
}

static HRESULT WINAPI
IDirectDrawImpl_SetCooperativeLevel(LPDIRECTDRAW This, HWND hWnd,
				    DWORD dwFlags)
{
    return IDirectDraw7_SetCooperativeLevel(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw,
							       IDirectDraw7,
							       This),
					    hWnd, dwFlags);
}

static HRESULT WINAPI
IDirectDraw2Impl_SetCooperativeLevel(LPDIRECTDRAW2 This, HWND hWnd,
				     DWORD dwFlags)
{
    return IDirectDraw7_SetCooperativeLevel(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw2,
							       IDirectDraw7,
							       This),
					    hWnd, dwFlags);
}

static HRESULT WINAPI
IDirectDraw4Impl_SetCooperativeLevel(LPDIRECTDRAW4 This, HWND hWnd,
				     DWORD dwFlags)
{
    return IDirectDraw7_SetCooperativeLevel(COM_INTERFACE_CAST(IDirectDrawImpl,
							       IDirectDraw4,
							       IDirectDraw7,
							       This),
					    hWnd, dwFlags);
}

static HRESULT WINAPI
IDirectDrawImpl_SetDisplayMode(LPDIRECTDRAW This, DWORD a, DWORD b, DWORD c)
{
    return IDirectDraw7_SetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw,
							  IDirectDraw7,
							  This),
				       a, b, c, 0, 0);
}

static HRESULT WINAPI
IDirectDraw2Impl_SetDisplayMode(LPDIRECTDRAW2 This, DWORD a, DWORD b, DWORD c,
				DWORD d, DWORD e)
{
    return IDirectDraw7_SetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw2,
							  IDirectDraw7,
							  This),
				       a, b, c, d, e);
}

static HRESULT WINAPI
IDirectDraw4Impl_SetDisplayMode(LPDIRECTDRAW4 This, DWORD a, DWORD b, DWORD c,
				DWORD d, DWORD e)
{
    return IDirectDraw7_SetDisplayMode(COM_INTERFACE_CAST(IDirectDrawImpl,
							  IDirectDraw4,
							  IDirectDraw7,
							  This),
				       a, b, c, d, e);
}

static HRESULT WINAPI
IDirectDrawImpl_WaitForVerticalBlank(LPDIRECTDRAW This, DWORD dwFlags,
				     HANDLE hEvent)
{
    return IDirectDraw7_WaitForVerticalBlank(COM_INTERFACE_CAST(IDirectDrawImpl,
								IDirectDraw,
								IDirectDraw7,
								This),
					     dwFlags, hEvent);
}

static HRESULT WINAPI
IDirectDraw2Impl_WaitForVerticalBlank(LPDIRECTDRAW2 This, DWORD dwFlags,
				      HANDLE hEvent)
{
    return IDirectDraw7_WaitForVerticalBlank(COM_INTERFACE_CAST(IDirectDrawImpl,
								IDirectDraw2,
								IDirectDraw7,
								This),
					     dwFlags, hEvent);
}

static HRESULT WINAPI
IDirectDraw4Impl_WaitForVerticalBlank(LPDIRECTDRAW4 This, DWORD dwFlags,
				      HANDLE hEvent)
{
    return IDirectDraw7_WaitForVerticalBlank(COM_INTERFACE_CAST(IDirectDrawImpl,
								IDirectDraw4,
								IDirectDraw7,
								This),
					     dwFlags, hEvent);
}

static HRESULT WINAPI
IDirectDraw2Impl_GetAvailableVidMem(LPDIRECTDRAW2 This, LPDDSCAPS pCaps,
				    LPDWORD pdwTotal, LPDWORD pdwFree)
{
    DDSCAPS2 Caps2;
    DDRAW_Convert_DDSCAPS_1_To_2(pCaps, &Caps2);

    return IDirectDraw7_GetAvailableVidMem(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw2,
							      IDirectDraw7,
							      This),
					   &Caps2, pdwTotal, pdwFree);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetAvailableVidMem(LPDIRECTDRAW4 This, LPDDSCAPS2 pCaps,
				    LPDWORD pdwTotal, LPDWORD pdwFree)
{
    return IDirectDraw7_GetAvailableVidMem(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw4,
							      IDirectDraw7,
							      This),
					   pCaps, pdwTotal, pdwFree);
}

static HRESULT WINAPI
IDirectDraw4Impl_GetSurfaceFromDC(LPDIRECTDRAW4 This, HDC hDC,
				  LPDIRECTDRAWSURFACE4 *pSurf)
{
    return IDirectDraw7_GetSurfaceFromDC(COM_INTERFACE_CAST(IDirectDrawImpl,
							    IDirectDraw4,
							    IDirectDraw7,
							    This),
					 hDC, (LPDIRECTDRAWSURFACE7 *)pSurf);
}

static HRESULT WINAPI
IDirectDraw4Impl_RestoreAllSurfaces(LPDIRECTDRAW4 This)
{
    return IDirectDraw7_RestoreAllSurfaces(COM_INTERFACE_CAST(IDirectDrawImpl,
							      IDirectDraw4,
							      IDirectDraw7,
							      This));
}

static HRESULT WINAPI
IDirectDraw4Impl_TestCooperativeLevel(LPDIRECTDRAW4 This)
{
    return IDirectDraw7_TestCooperativeLevel(COM_INTERFACE_CAST(IDirectDrawImpl,
								IDirectDraw4,
								IDirectDraw7,
								This));
}

static HRESULT WINAPI
IDirectDraw4Impl_GetDeviceIdentifier(LPDIRECTDRAW4 This,
				     LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags)
{
    DDDEVICEIDENTIFIER2 DDDI2;
    HRESULT hr;

    hr = IDirectDraw7_GetDeviceIdentifier(COM_INTERFACE_CAST(IDirectDrawImpl,
							     IDirectDraw4,
							     IDirectDraw7,
							     This),
					  &DDDI2, dwFlags);

    DDRAW_Convert_DDDEVICEIDENTIFIER_2_To_1(&DDDI2, pDDDI);

    return hr;
}

const IDirectDrawVtbl IDirectDraw1_Vtbl =
{
    IDirectDrawImpl_QueryInterface,
    IDirectDrawImpl_AddRef,
    IDirectDrawImpl_Release,
    IDirectDrawImpl_Compact,
    IDirectDrawImpl_CreateClipper,
    IDirectDrawImpl_CreatePalette,
    IDirectDrawImpl_CreateSurface,
    IDirectDrawImpl_DuplicateSurface,
    IDirectDrawImpl_EnumDisplayModes,
    IDirectDrawImpl_EnumSurfaces,
    IDirectDrawImpl_FlipToGDISurface,
    IDirectDrawImpl_GetCaps,
    IDirectDrawImpl_GetDisplayMode,
    IDirectDrawImpl_GetFourCCCodes,
    IDirectDrawImpl_GetGDISurface,
    IDirectDrawImpl_GetMonitorFrequency,
    IDirectDrawImpl_GetScanLine,
    IDirectDrawImpl_GetVerticalBlankStatus,
    IDirectDrawImpl_Initialize,
    IDirectDrawImpl_RestoreDisplayMode,
    IDirectDrawImpl_SetCooperativeLevel,
    IDirectDrawImpl_SetDisplayMode,
    IDirectDrawImpl_WaitForVerticalBlank,
};

const IDirectDraw2Vtbl IDirectDraw2_Vtbl =
{
    IDirectDraw2Impl_QueryInterface,
    IDirectDraw2Impl_AddRef,
    IDirectDraw2Impl_Release,
    IDirectDraw2Impl_Compact,
    IDirectDraw2Impl_CreateClipper,
    IDirectDraw2Impl_CreatePalette,
    IDirectDraw2Impl_CreateSurface,
    IDirectDraw2Impl_DuplicateSurface,
    IDirectDraw2Impl_EnumDisplayModes,
    IDirectDraw2Impl_EnumSurfaces,
    IDirectDraw2Impl_FlipToGDISurface,
    IDirectDraw2Impl_GetCaps,
    IDirectDraw2Impl_GetDisplayMode,
    IDirectDraw2Impl_GetFourCCCodes,
    IDirectDraw2Impl_GetGDISurface,
    IDirectDraw2Impl_GetMonitorFrequency,
    IDirectDraw2Impl_GetScanLine,
    IDirectDraw2Impl_GetVerticalBlankStatus,
    IDirectDraw2Impl_Initialize,
    IDirectDraw2Impl_RestoreDisplayMode,
    IDirectDraw2Impl_SetCooperativeLevel,
    IDirectDraw2Impl_SetDisplayMode,
    IDirectDraw2Impl_WaitForVerticalBlank,
    IDirectDraw2Impl_GetAvailableVidMem
};

const IDirectDraw4Vtbl IDirectDraw4_Vtbl =
{
    IDirectDraw4Impl_QueryInterface,
    IDirectDraw4Impl_AddRef,
    IDirectDraw4Impl_Release,
    IDirectDraw4Impl_Compact,
    IDirectDraw4Impl_CreateClipper,
    IDirectDraw4Impl_CreatePalette,
    IDirectDraw4Impl_CreateSurface,
    IDirectDraw4Impl_DuplicateSurface,
    IDirectDraw4Impl_EnumDisplayModes,
    IDirectDraw4Impl_EnumSurfaces,
    IDirectDraw4Impl_FlipToGDISurface,
    IDirectDraw4Impl_GetCaps,
    IDirectDraw4Impl_GetDisplayMode,
    IDirectDraw4Impl_GetFourCCCodes,
    IDirectDraw4Impl_GetGDISurface,
    IDirectDraw4Impl_GetMonitorFrequency,
    IDirectDraw4Impl_GetScanLine,
    IDirectDraw4Impl_GetVerticalBlankStatus,
    IDirectDraw4Impl_Initialize,
    IDirectDraw4Impl_RestoreDisplayMode,
    IDirectDraw4Impl_SetCooperativeLevel,
    IDirectDraw4Impl_SetDisplayMode,
    IDirectDraw4Impl_WaitForVerticalBlank,
    IDirectDraw4Impl_GetAvailableVidMem,
    IDirectDraw4Impl_GetSurfaceFromDC,
    IDirectDraw4Impl_RestoreAllSurfaces,
    IDirectDraw4Impl_TestCooperativeLevel,
    IDirectDraw4Impl_GetDeviceIdentifier
};
