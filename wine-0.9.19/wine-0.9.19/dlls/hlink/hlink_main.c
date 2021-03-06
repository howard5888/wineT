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
#include "winreg.h"
#include "ole2.h"
#include "unknwn.h"

#include "wine/debug.h"
#include "wine/unicode.h"
#include "hlink.h"

#include "initguid.h"
#include "hlguids.h"

WINE_DEFAULT_DEBUG_CHANNEL(hlink);

extern HRESULT WINAPI HLink_Constructor(IUnknown *, REFIID, LPVOID*);
extern HRESULT WINAPI HLinkBrowseContext_Constructor(IUnknown *, REFIID, LPVOID*);

typedef HRESULT (CALLBACK *LPFNCREATEINSTANCE)(IUnknown*, REFIID, LPVOID*);

typedef struct
{
    const IClassFactoryVtbl *lpVtbl;
    LPFNCREATEINSTANCE      lpfnCI;
} CFImpl;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%p %ld %p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow( void )
{
    FIXME("\n");
    return S_OK;
}

HRESULT WINAPI HlinkCreateFromMoniker( IMoniker *pimkTrgt, LPCWSTR pwzLocation,
        LPCWSTR pwzFriendlyName, IHlinkSite* pihlsite, DWORD dwSiteData,
        IUnknown* piunkOuter, REFIID riid, void** ppvObj)
{
    IHlink *hl = NULL;
    HRESULT r = S_OK;

    TRACE("%p %s %s %p %li %p %s %p\n", pimkTrgt, debugstr_w(pwzLocation),
            debugstr_w(pwzFriendlyName), pihlsite, dwSiteData, piunkOuter,
            debugstr_guid(riid), ppvObj);

    r = CoCreateInstance(&CLSID_StdHlink, piunkOuter, CLSCTX_INPROC_SERVER, riid, (LPVOID*)&hl);
    if (FAILED(r))
        return r;

    if (pwzLocation)
        IHlink_SetStringReference(hl, HLINKSETF_LOCATION, NULL, pwzLocation);
    if (pwzFriendlyName)
        IHlink_SetFriendlyName(hl, pwzFriendlyName);
    if (pihlsite)
        IHlink_SetHlinkSite(hl, pihlsite, dwSiteData);
    if (pimkTrgt)
        IHlink_SetMonikerReference(hl, 0, pimkTrgt, pwzLocation);

    *ppvObj = hl;

    TRACE("Returning %li\n",r);

    return r;
}

HRESULT WINAPI HlinkCreateFromString( LPCWSTR pwzTarget, LPCWSTR pwzLocation,
        LPCWSTR pwzFriendlyName, IHlinkSite* pihlsite, DWORD dwSiteData,
        IUnknown* piunkOuter, REFIID riid, void** ppvObj)
{
    IHlink *hl = NULL;
    HRESULT r = S_OK;

    TRACE("%s %s %s %p %li %p %s %p\n", debugstr_w(pwzTarget),
            debugstr_w(pwzLocation), debugstr_w(pwzFriendlyName), pihlsite,
            dwSiteData, piunkOuter, debugstr_guid(riid), ppvObj);

    r = CoCreateInstance(&CLSID_StdHlink, piunkOuter, CLSCTX_INPROC_SERVER, riid, (LPVOID*)&hl);
    if (FAILED(r))
        return r;

    if (pwzLocation)
        IHlink_SetStringReference(hl, HLINKSETF_LOCATION, NULL, pwzLocation);

    if (pwzTarget)
    {
        IMoniker *pTgtMk = NULL;
        IBindCtx *pbc = NULL;
        ULONG eaten;

        CreateBindCtx(0, &pbc);
        r = MkParseDisplayName(pbc, pwzTarget, &eaten, &pTgtMk);
        IBindCtx_Release(pbc);

        if (FAILED(r))
        {
            FIXME("ParseDisplayName failed, falling back to file\n");
            r = CreateFileMoniker(pwzTarget,&pTgtMk);
        }

        if (pTgtMk)
        {
            IHlink_SetMonikerReference(hl, 0, pTgtMk, pwzLocation);
            IMoniker_Release(pTgtMk);
        }
        else
            FIXME("Unable to come up with a moniker, expect problems\n");

        IHlink_SetStringReference(hl, HLINKSETF_TARGET, pwzTarget, NULL);
    }

    if (pwzFriendlyName)
        IHlink_SetFriendlyName(hl, pwzFriendlyName);
    if (pihlsite)
        IHlink_SetHlinkSite(hl, pihlsite, dwSiteData);

    TRACE("Returning %li\n",r);
    *ppvObj = hl;

    return r;
}


HRESULT WINAPI HlinkCreateBrowseContext( IUnknown* piunkOuter, REFIID riid, void** ppvObj)
{
    HRESULT r = S_OK;

    TRACE("%p %s %p\n", piunkOuter, debugstr_guid(riid), ppvObj);

    r = CoCreateInstance(&CLSID_StdHlinkBrowseContext, piunkOuter, CLSCTX_INPROC_SERVER, riid, ppvObj);

    TRACE("returning %li\n",r);

    return r;
}

HRESULT WINAPI HlinkNavigate(IHlink *phl, IHlinkFrame *phlFrame,
        DWORD grfHLNF, LPBC pbc, IBindStatusCallback *pbsc,
        IHlinkBrowseContext *phlbc)
{
    HRESULT r = S_OK;

    TRACE("%p %p %li %p %p %p\n", phl, phlFrame, grfHLNF, pbc, pbsc, phlbc);

    if (phlFrame)
        r = IHlinkFrame_Navigate(phlFrame, grfHLNF, pbc, pbsc, phl);
    else if (phl)
        r = IHlink_Navigate(phl, grfHLNF, pbc, pbsc, phlbc);

    return r;
}

HRESULT WINAPI HlinkOnNavigate( IHlinkFrame *phlFrame,
        IHlinkBrowseContext* phlbc, DWORD grfHLNF, IMoniker *pmkTarget,
        LPCWSTR pwzLocation, LPCWSTR pwzFriendlyName, ULONG* puHLID)
{
    HRESULT r = S_OK;

    TRACE("%p %p %li %p %s %s %p\n",phlFrame, phlbc, grfHLNF, pmkTarget,
            debugstr_w(pwzLocation), debugstr_w(pwzFriendlyName), puHLID);

    r = IHlinkBrowseContext_OnNavigateHlink(phlbc, grfHLNF, pmkTarget,
            pwzLocation, pwzFriendlyName, puHLID);

    if (phlFrame)
        r = IHlinkFrame_OnNavigate(phlFrame,grfHLNF,pmkTarget, pwzLocation,
                pwzFriendlyName, 0);

    return r;
}

HRESULT WINAPI HlinkCreateFromData(IDataObject *piDataObj,
        IHlinkSite *pihlsite, DWORD dwSiteData, IUnknown *piunkOuter,
        REFIID riid, void **ppvObj)
{
    FIXME("%p %p %ld %p %p %p\n",
          piDataObj, pihlsite, dwSiteData, piunkOuter, riid, ppvObj);
    *ppvObj = NULL;
    return E_NOTIMPL;
}

HRESULT WINAPI HlinkQueryCreateFromData(IDataObject* piDataObj)
{
    FIXME("%p\n", piDataObj);
    return E_NOTIMPL;
}

HRESULT WINAPI HlinkCreateExtensionServices(LPCWSTR pwzAdditionalHeaders,
        HWND phwnd, LPCWSTR pszUsername, LPCWSTR pszPassword,
        IUnknown *punkOuter, REFIID riid, void** ppvObj)
{
    FIXME("%s %p %s %s %p %s %p\n",debugstr_w(pwzAdditionalHeaders),
            phwnd, debugstr_w(pszUsername), debugstr_w(pszPassword),
            punkOuter, debugstr_guid(riid), ppvObj);
    return E_NOTIMPL;
}

HRESULT WINAPI HlinkNavigateToStringReference( LPCWSTR pwzTarget,
        LPCWSTR pwzLocation, IHlinkSite *pihlsite, DWORD dwSiteData,
        IHlinkFrame *pihlframe, DWORD grfHLNF, LPBC pibc,
        IBindStatusCallback *pibsc, IHlinkBrowseContext *pihlbc)
{
    HRESULT r;
    IHlink *hlink = NULL;

    FIXME("%s %s %p %08lx %p %08lx %p %p %p\n",
          debugstr_w(pwzTarget), debugstr_w(pwzLocation), pihlsite,
          dwSiteData, pihlframe, grfHLNF, pibc, pibsc, pihlbc);

    r = HlinkCreateFromString( pwzTarget, pwzLocation, NULL, pihlsite,
                               dwSiteData, NULL, &IID_IHlink, (LPVOID*) &hlink );
    if (SUCCEEDED(r))
        r = HlinkNavigate(hlink, pihlframe, grfHLNF, pibc, pibsc, pihlbc);

    return r;
}

HRESULT WINAPI HlinkIsShortcut(LPCWSTR pwzFileName)
{
    int len;

    static const WCHAR url_ext[] = {'.','u','r','l',0};

    TRACE("(%s)\n", debugstr_w(pwzFileName));

    if(!pwzFileName)
        return E_INVALIDARG;

    len = strlenW(pwzFileName)-4;
    if(len < 0)
        return S_FALSE;

    return strcmpiW(pwzFileName+len, url_ext) ? S_FALSE : S_OK;
}

HRESULT WINAPI HlinkTranslateURL(LPCWSTR pwzURL, DWORD grfFlags, LPWSTR *ppwzTranslatedURL)
{
    FIXME("(%s %08lx %p)\n", debugstr_w(pwzURL), grfFlags, ppwzTranslatedURL);
    return E_NOTIMPL;
}

static HRESULT WINAPI HLinkCF_fnQueryInterface ( LPCLASSFACTORY iface,
        REFIID riid, LPVOID *ppvObj)
{
    CFImpl *This = (CFImpl *)iface;

    TRACE("(%p)->(%s)\n",This,debugstr_guid(riid));

    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IClassFactory))
    {
        *ppvObj = This;
        return S_OK;
    }

    TRACE("-- E_NOINTERFACE\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI HLinkCF_fnAddRef (LPCLASSFACTORY iface)
{
    return 2;
}

static ULONG WINAPI HLinkCF_fnRelease(LPCLASSFACTORY iface)
{
    return 1;
}

static HRESULT WINAPI HLinkCF_fnCreateInstance( LPCLASSFACTORY iface,
        LPUNKNOWN pUnkOuter, REFIID riid, LPVOID *ppvObject)
{
    CFImpl *This = (CFImpl *)iface;

    TRACE("%p->(%p,%s,%p)\n", This, pUnkOuter, debugstr_guid(riid), ppvObject);

    *ppvObject = NULL;

    return This->lpfnCI(pUnkOuter, riid, ppvObject);
}

static HRESULT WINAPI HLinkCF_fnLockServer(LPCLASSFACTORY iface, BOOL fLock)
{
    FIXME("%p %d\n", iface, fLock);
    return E_NOTIMPL;
}

static const IClassFactoryVtbl hlcfvt =
{
    HLinkCF_fnQueryInterface,
    HLinkCF_fnAddRef,
    HLinkCF_fnRelease,
    HLinkCF_fnCreateInstance,
    HLinkCF_fnLockServer
};

static CFImpl HLink_cf = { &hlcfvt, &HLink_Constructor };
static CFImpl HLinkBrowseContext_cf = { &hlcfvt, &HLinkBrowseContext_Constructor };

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID iid, LPVOID *ppv)
{
    IClassFactory   *pcf = NULL;

    TRACE("%s %s %p\n", debugstr_guid(rclsid), debugstr_guid(iid), ppv);

    if (!ppv)
        return E_INVALIDARG;
    *ppv = NULL;

    if (IsEqualIID(rclsid, &CLSID_StdHlink))
        pcf = (IClassFactory*) &HLink_cf;
    else if (IsEqualIID(rclsid, &CLSID_StdHlinkBrowseContext))
        pcf = (IClassFactory*) &HLinkBrowseContext_cf;
    else
        return CLASS_E_CLASSNOTAVAILABLE;

    return IClassFactory_QueryInterface(pcf, iid, ppv);
}

static HRESULT register_clsid(LPCGUID guid)
{
    static const WCHAR clsid[] =
        {'C','L','S','I','D','\\',0};
    static const WCHAR ips[] =
        {'\\','I','n','p','r','o','c','S','e','r','v','e','r','3','2',0};
    static const WCHAR hlink[] =
        {'h','l','i','n','k','.','d','l','l',0};
    static const WCHAR threading_model[] =
        {'T','h','r','e','a','d','i','n','g','M','o','d','e','l',0};
    static const WCHAR apartment[] =
        {'A','p','a','r','t','m','e','n','t',0};
    WCHAR path[80];
    HKEY key = NULL;
    LONG r;

    lstrcpyW(path, clsid);
    StringFromGUID2(guid, &path[6], 80);
    lstrcatW(path, ips);
    r = RegCreateKeyW(HKEY_CLASSES_ROOT, path, &key);
    if (r != ERROR_SUCCESS)
        return E_FAIL;

    RegSetValueExW(key, NULL, 0, REG_SZ, (LPBYTE)hlink, sizeof hlink);
    RegSetValueExW(key, threading_model, 0, REG_SZ, (LPBYTE)apartment, sizeof apartment);
    RegCloseKey(key);

    return S_OK;
}

HRESULT WINAPI DllRegisterServer(void)
{
    HRESULT r;

    r = register_clsid(&CLSID_StdHlink);
    if (SUCCEEDED(r))
        r = register_clsid(&CLSID_StdHlinkBrowseContext);

    return S_OK;
}
