/*
 * Implementation of hyperlinking (hlink.dll)
 *
 * Copyright 2005 Aric Stewart for CodeWeavers
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

#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "unknwn.h"
#include "objidl.h"
#include "shellapi.h"

#include "wine/debug.h"
#include "hlink.h"
#include "hlguids.h"

WINE_DEFAULT_DEBUG_CHANNEL(hlink);

static const IHlinkVtbl              hlvt;
static const IPersistStreamVtbl      psvt;
static const IDataObjectVtbl         dovt;

typedef struct
{
    const IHlinkVtbl    *lpVtbl;
    LONG                ref;

    const IPersistStreamVtbl    *lpPSVtbl;
    const IDataObjectVtbl       *lpDOVtbl;

    LPWSTR              FriendlyName;
    LPWSTR              Location;
    LPWSTR              Target;
    LPWSTR              TargetFrameName;
    IMoniker            *Moniker;
    IHlinkSite          *Site;
    DWORD               SiteData;
} HlinkImpl;


static inline HlinkImpl* HlinkImpl_from_IPersistStream( IPersistStream* iface)
{
    return (HlinkImpl*) ((CHAR*)iface - FIELD_OFFSET(HlinkImpl, lpPSVtbl));
}

static inline HlinkImpl* HlinkImpl_from_IDataObject( IDataObject* iface)
{
    return (HlinkImpl*) ((CHAR*)iface - FIELD_OFFSET(HlinkImpl, lpDOVtbl));
}

static inline LPWSTR strdupW( LPCWSTR str )
{
    LPWSTR r;
    UINT len;

    if (!str)
        return NULL;
    len = (lstrlenW(str)+1) * sizeof (WCHAR);
    r = HeapAlloc(GetProcessHeap(), 0, len);
    if (r)
        memcpy(r, str, len);
    return r;
}

static inline LPWSTR co_strdupW( LPCWSTR str )
{
    LPWSTR r;
    UINT len;

    if (!str)
        return NULL;
    len = (lstrlenW(str)+1) * sizeof (WCHAR);
    r = CoTaskMemAlloc(len);
    if (r)
        memcpy(r, str, len);
    return r;
}

static inline void __GetMoniker(HlinkImpl* This, IMoniker** moniker)
{
    *moniker = NULL;
    if (This->Moniker)
    {
        *moniker = This->Moniker;
        if (*moniker)
            IMoniker_AddRef(*moniker);
    }
    else if (This->Site)
    {
        IHlinkSite_GetMoniker(This->Site, This->SiteData,
                OLEGETMONIKER_FORCEASSIGN, OLEWHICHMK_CONTAINER,
                (LPVOID)moniker);
    }
}

HRESULT WINAPI HLink_Constructor(IUnknown *pUnkOuter, REFIID riid,
        LPVOID *ppv)
{
    HlinkImpl * hl;

    TRACE("unkOut=%p riid=%s\n", pUnkOuter, debugstr_guid(riid));
    *ppv = NULL;

    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    hl = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HlinkImpl));
    if (!hl)
        return E_OUTOFMEMORY;

    hl->ref = 1;
    hl->lpVtbl = &hlvt;
    hl->lpPSVtbl = &psvt;
    hl->lpDOVtbl = &dovt;

    *ppv = hl;
    return S_OK;
}

static HRESULT WINAPI IHlink_fnQueryInterface(IHlink* iface, REFIID riid,
        LPVOID *ppvObj)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE ("(%p)->(%s,%p)\n", This, debugstr_guid (riid), ppvObj);

    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || (IsEqualIID(riid, &IID_IHlink)))
        *ppvObj = This;
    else if (IsEqualIID(riid, &IID_IPersistStream))
        *ppvObj = (LPVOID*)&(This->lpPSVtbl);
    else if (IsEqualIID(riid, &IID_IDataObject))
        *ppvObj = (LPVOID*)&(This->lpDOVtbl);

    if (*ppvObj)
    {
        IUnknown_AddRef((IUnknown*)(*ppvObj));
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI IHlink_fnAddRef (IHlink* iface)
{
    HlinkImpl  *This = (HlinkImpl*)iface;
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(count=%lu)\n", This, refCount - 1);

    return refCount;
}

static ULONG WINAPI IHlink_fnRelease (IHlink* iface)
{
    HlinkImpl  *This = (HlinkImpl*)iface;
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(count=%lu)\n", This, refCount + 1);
    if (refCount)
        return refCount;

    TRACE("-- destroying IHlink (%p)\n", This);
    HeapFree(GetProcessHeap(), 0, This->FriendlyName);
    HeapFree(GetProcessHeap(), 0, This->Target);
    HeapFree(GetProcessHeap(), 0, This->TargetFrameName);
    HeapFree(GetProcessHeap(), 0, This->Location);
    if (This->Moniker)
        IMoniker_Release(This->Moniker);
    if (This->Site)
        IHlinkSite_Release(This->Site);
    HeapFree(GetProcessHeap(), 0, This);
    return 0;
}

static HRESULT WINAPI IHlink_fnSetHlinkSite( IHlink* iface,
        IHlinkSite* pihlSite, DWORD dwSiteData)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p)->(%p %li)\n", This, pihlSite, dwSiteData);

    if (This->Site)
        IHlinkSite_Release(This->Site);

    This->Site = pihlSite;
    if (This->Site)
        IHlinkSite_AddRef(This->Site);

    This->SiteData = dwSiteData;

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetHlinkSite( IHlink* iface,
        IHlinkSite** ppihlSite, DWORD *pdwSiteData)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p)->(%p %p)\n", This, ppihlSite, pdwSiteData);

    *ppihlSite = This->Site;
    *pdwSiteData = This->SiteData;

    if (This->Site)
        IHlinkSite_AddRef(This->Site);

    return S_OK;
}

static HRESULT WINAPI IHlink_fnSetMonikerReference( IHlink* iface,
        DWORD rfHLSETF, IMoniker *pmkTarget, LPCWSTR pwzLocation)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    FIXME("(%p)->(%li %p %s)\n", This, rfHLSETF, pmkTarget,
            debugstr_w(pwzLocation));

    if (This->Moniker)
        IMoniker_Release(This->Moniker);

    This->Moniker = pmkTarget;
    if (This->Moniker)
        IMoniker_AddRef(This->Moniker);

    HeapFree(GetProcessHeap(), 0, This->Location);
    This->Location = strdupW( pwzLocation );

    return S_OK;
}

static HRESULT WINAPI IHlink_fnSetStringReference(IHlink* iface,
        DWORD grfHLSETF, LPCWSTR pwzTarget, LPCWSTR pwzLocation)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p)->(%li %s %s)\n", This, grfHLSETF, debugstr_w(pwzTarget),
            debugstr_w(pwzLocation));

    if (grfHLSETF & HLINKSETF_TARGET)
    {
        HeapFree(GetProcessHeap(), 0, This->Target);
        This->Target = strdupW( pwzTarget );
    }
    if (grfHLSETF & HLINKSETF_LOCATION)
    {
        HeapFree(GetProcessHeap(), 0, This->Location);
        This->Location = strdupW( pwzLocation );
    }

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetMonikerReference(IHlink* iface,
        DWORD dwWhichRef, IMoniker **ppimkTarget, LPWSTR *ppwzLocation)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p) -> (%li %p %p)\n", This, dwWhichRef, ppimkTarget,
            ppwzLocation);

    if(ppimkTarget)
        __GetMoniker(This, ppimkTarget);

    if (ppwzLocation)
        IHlink_GetStringReference(iface, dwWhichRef, NULL, ppwzLocation);

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetStringReference (IHlink* iface,
        DWORD dwWhichRef, LPWSTR *ppwzTarget, LPWSTR *ppwzLocation)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    FIXME("(%p) -> (%li %p %p)\n", This, dwWhichRef, ppwzTarget, ppwzLocation);

    if (ppwzTarget)
    {
        *ppwzTarget = co_strdupW( This->Target );

        if (!This->Target)
        {
            IMoniker* mon;
            __GetMoniker(This, &mon);
            if (mon)
            {
                IBindCtx *pbc;

                CreateBindCtx( 0, &pbc);
                IMoniker_GetDisplayName(mon, pbc, NULL, ppwzTarget);
                IBindCtx_Release(pbc);
                IMoniker_Release(mon);
            }
            else
                FIXME("Unhandled case, no set Target and no moniker\n");
        }
    }
    if (ppwzLocation)
        *ppwzLocation = co_strdupW( This->Location );

    TRACE("(Target: %s Location: %s)\n",
            (ppwzTarget)?debugstr_w(*ppwzTarget):"<NULL>",
            (ppwzLocation)?debugstr_w(*ppwzLocation):"<NULL>");

    return S_OK;
}

static HRESULT WINAPI IHlink_fnSetFriendlyName (IHlink *iface,
        LPCWSTR pwzFriendlyName)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p) -> (%s)\n", This, debugstr_w(pwzFriendlyName));

    HeapFree(GetProcessHeap(), 0, This->FriendlyName);
    This->FriendlyName = strdupW( pwzFriendlyName );

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetFriendlyName (IHlink* iface,
        DWORD grfHLFNAMEF, LPWSTR* ppwzFriendlyName)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p) -> (%li %p)\n", This, grfHLFNAMEF, ppwzFriendlyName);

    /* FIXME: Only using explicitly set and cached friendly names */

    if (This->FriendlyName)
        *ppwzFriendlyName = co_strdupW( This->FriendlyName );
    else
    {
        IMoniker *moniker;
        __GetMoniker(This, &moniker);
        if (moniker)
        {
            IBindCtx *bcxt;
            CreateBindCtx(0, &bcxt);

            IMoniker_GetDisplayName(moniker, bcxt, NULL, ppwzFriendlyName);
            IBindCtx_Release(bcxt);
            IMoniker_Release(moniker);
        }
        else
            *ppwzFriendlyName = NULL;
    }

    return S_OK;
}

static HRESULT WINAPI IHlink_fnSetTargetFrameName(IHlink* iface,
        LPCWSTR pwzTargetFramename)
{
    HlinkImpl  *This = (HlinkImpl*)iface;
    TRACE("(%p)->(%s)\n", This, debugstr_w(pwzTargetFramename));

    HeapFree(GetProcessHeap(), 0, This->TargetFrameName);
    This->TargetFrameName = strdupW( pwzTargetFramename );

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetTargetFrameName(IHlink* iface,
        LPWSTR *ppwzTargetFrameName)
{
    HlinkImpl  *This = (HlinkImpl*)iface;

    TRACE("(%p)->(%p)\n", This, ppwzTargetFrameName);
    *ppwzTargetFrameName = co_strdupW( This->TargetFrameName );

    return S_OK;
}

static HRESULT WINAPI IHlink_fnGetMiscStatus(IHlink* iface, DWORD* pdwStatus)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IHlink_fnNavigate(IHlink* iface, DWORD grfHLNF, LPBC pbc,
        IBindStatusCallback *pbsc, IHlinkBrowseContext *phbc)
{
    HlinkImpl  *This = (HlinkImpl*)iface;
    IMoniker *mon = NULL;

    FIXME("Semi-Stub:(%p)->(%li %p %p %p)\n", This, grfHLNF, pbc, pbsc, phbc);

    if (This->Site)
        IHlinkSite_ReadyToNavigate(This->Site, This->SiteData, 0);

    __GetMoniker(This, &mon);
    TRACE("Moniker %p\n", mon);

    if (mon)
    {
        IBindCtx *bcxt;
        IHlinkTarget *target = NULL;
        HRESULT r = S_OK;

        CreateBindCtx(0, &bcxt);

        RegisterBindStatusCallback(bcxt, pbsc, NULL, 0);

        r = IMoniker_BindToObject(mon, bcxt, NULL, &IID_IHlinkTarget,
                (LPVOID*)&target);
        TRACE("IHlinkTarget returned 0x%lx\n", r);
        if (r == S_OK)
        {
            IHlinkTarget_SetBrowseContext(target, phbc);
            IHlinkTarget_Navigate(target, grfHLNF, This->Location);
            IHlinkTarget_Release(target);
        }
        else
        {
            static const WCHAR szOpen[] = {'o','p','e','n',0};
            LPWSTR target = NULL;

            r = IHlink_GetStringReference(iface, HLINKGETREF_DEFAULT, &target, NULL);
            if (SUCCEEDED(r) && target)
            {
                ShellExecuteW(NULL, szOpen, target, NULL, NULL, SW_SHOW);
                CoTaskMemFree(target);
            }
        }

        RevokeBindStatusCallback(bcxt, pbsc);

        IBindCtx_Release(bcxt);
        IMoniker_Release(mon);
    }

    if (This->Site)
        IHlinkSite_OnNavigationComplete(This->Site, This->SiteData, 0, 0, NULL);

    TRACE("Finished Navigation\n");
    return S_OK;
}

static HRESULT WINAPI IHlink_fnSetAdditonalParams(IHlink* iface,
        LPCWSTR pwzAdditionalParams)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IHlink_fnGetAdditionalParams(IHlink* iface,
        LPWSTR* ppwzAdditionalParams)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static const IHlinkVtbl hlvt =
{
    IHlink_fnQueryInterface,
    IHlink_fnAddRef,
    IHlink_fnRelease,
    IHlink_fnSetHlinkSite,
    IHlink_fnGetHlinkSite,
    IHlink_fnSetMonikerReference,
    IHlink_fnGetMonikerReference,
    IHlink_fnSetStringReference,
    IHlink_fnGetStringReference,
    IHlink_fnSetFriendlyName,
    IHlink_fnGetFriendlyName,
    IHlink_fnSetTargetFrameName,
    IHlink_fnGetTargetFrameName,
    IHlink_fnGetMiscStatus,
    IHlink_fnNavigate,
    IHlink_fnSetAdditonalParams,
    IHlink_fnGetAdditionalParams
};

static HRESULT WINAPI IDataObject_fnQueryInterface(IDataObject* iface,
        REFIID riid, LPVOID *ppvObj)
{
    HlinkImpl *This = HlinkImpl_from_IDataObject(iface);
    TRACE("%p\n", This);
    return IHlink_QueryInterface((IHlink*)This, riid, ppvObj);
}

static ULONG WINAPI IDataObject_fnAddRef (IDataObject* iface)
{
    HlinkImpl *This = HlinkImpl_from_IDataObject(iface);
    TRACE("%p\n", This);
    return IHlink_AddRef((IHlink*)This);
}

static ULONG WINAPI IDataObject_fnRelease (IDataObject* iface)
{
    HlinkImpl *This = HlinkImpl_from_IDataObject(iface);
    TRACE("%p\n", This);
    return IHlink_Release((IHlink*)This);
}

static HRESULT WINAPI IDataObject_fnGetData(IDataObject* iface,
        FORMATETC* pformatetcIn, STGMEDIUM* pmedium)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnGetDataHere(IDataObject* iface,
        FORMATETC* pformatetc, STGMEDIUM* pmedium)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnQueryGetData(IDataObject* iface,
        FORMATETC* pformatetc)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnGetConicalFormatEtc(IDataObject* iface,
        FORMATETC* pformatetcIn, FORMATETC* pformatetcOut)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnSetData(IDataObject* iface,
        FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnEnumFormatEtc(IDataObject* iface,
        DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnDAdvise(IDataObject* iface,
        FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
        DWORD* pdwConnection)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnDUnadvise(IDataObject* iface,
        DWORD dwConnection)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IDataObject_fnEnumDAdvise(IDataObject* iface,
        IEnumSTATDATA** ppenumAdvise)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static const IDataObjectVtbl dovt =
{
    IDataObject_fnQueryInterface,
    IDataObject_fnAddRef,
    IDataObject_fnRelease,
    IDataObject_fnGetData,
    IDataObject_fnGetDataHere,
    IDataObject_fnQueryGetData,
    IDataObject_fnGetConicalFormatEtc,
    IDataObject_fnSetData,
    IDataObject_fnEnumFormatEtc,
    IDataObject_fnDAdvise,
    IDataObject_fnDUnadvise,
    IDataObject_fnEnumDAdvise
};

static HRESULT WINAPI IPersistStream_fnQueryInterface(IPersistStream* iface,
        REFIID riid, LPVOID *ppvObj)
{
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    TRACE("(%p)\n", This);
    return IHlink_QueryInterface((IHlink*)This, riid, ppvObj);
}

static ULONG WINAPI IPersistStream_fnAddRef (IPersistStream* iface)
{
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    TRACE("(%p)\n", This);
    return IHlink_AddRef((IHlink*)This);
}

static ULONG WINAPI IPersistStream_fnRelease (IPersistStream* iface)
{
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    TRACE("(%p)\n", This);
    return IHlink_Release((IHlink*)This);
}

static HRESULT WINAPI IPersistStream_fnGetClassID(IPersistStream* iface,
        CLSID* pClassID)
{
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    TRACE("(%p)\n", This);
    memcpy(pClassID, &CLSID_StdHlink, sizeof(CLSID));
    return S_OK;
}

static HRESULT WINAPI IPersistStream_fnIsDirty(IPersistStream* iface)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI IPersistStream_fnLoad(IPersistStream* iface,
        IStream* pStm)
{
    HRESULT r = E_NOTIMPL;
    DWORD hdr[2];
    DWORD read;
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);

    IStream_Read(pStm, &hdr, sizeof(hdr), &read);
    /* FIXME: unknown header values */

    r = OleLoadFromStream(pStm, &IID_IMoniker, (LPVOID*)&(This->Moniker));
    TRACE("Load Result 0x%lx (%p)\n", r, This->Moniker);

    return r;
}

static HRESULT WINAPI IPersistStream_fnSave(IPersistStream* iface,
        IStream* pStm, BOOL fClearDirty)
{
    HRESULT r = E_FAIL;
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    DWORD hdr[2];
    IMoniker *moniker;

    FIXME("(%p) Moniker(%p)\n", This, This->Moniker);

    __GetMoniker(This, &moniker);
    if (moniker)
    {
        IPersistStream* monstream;
        /* FIXME: Unknown values in the header */
        hdr[0] = 2;
        hdr[1] = 2;

        IStream_Write(pStm, &hdr, sizeof(hdr), NULL);

        monstream = NULL;
        IMoniker_QueryInterface(moniker, &IID_IPersistStream,
                (LPVOID*)&monstream);
        if (monstream)
        {
            r = OleSaveToStream(monstream, pStm);
            IPersistStream_Release(monstream);
        }
        IMoniker_Release(moniker);
    }
    TRACE("Save Result 0x%lx\n", r);

    return r;
}

static HRESULT WINAPI IPersistStream_fnGetSizeMax(IPersistStream* iface,
        ULARGE_INTEGER* pcbSize)
{
    HRESULT r = E_FAIL;
    HlinkImpl *This = HlinkImpl_from_IPersistStream(iface);
    IMoniker *moniker;

    FIXME("(%p) Moniker(%p)\n", This, This->Moniker);

    __GetMoniker(This, &moniker);
    if (moniker)
    {
        IPersistStream* monstream = NULL;
        IMoniker_QueryInterface(moniker, &IID_IPersistStream,
                (LPVOID*)&monstream);
        if (monstream)
        {
            r = IPersistStream_GetSizeMax(monstream, pcbSize);
            /* FIXME: Handle ULARGE_INTEGER correctly */
            pcbSize->u.LowPart += sizeof(DWORD)*2;
            IPersistStream_Release(monstream);
        }
        IMoniker_Release(moniker);
    }

    return r;
}

static const IPersistStreamVtbl psvt =
{
    IPersistStream_fnQueryInterface,
    IPersistStream_fnAddRef,
    IPersistStream_fnRelease,
    IPersistStream_fnGetClassID,
    IPersistStream_fnIsDirty,
    IPersistStream_fnLoad,
    IPersistStream_fnSave,
    IPersistStream_fnGetSizeMax,
};
