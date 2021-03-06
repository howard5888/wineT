/*
 * UrlMon
 *
 * Copyright (c) 2000 Patrik Stridvall
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
#include "winreg.h"

#define NO_SHLWAPI_REG
#include "shlwapi.h"
#include "wine/debug.h"
#include "wine/unicode.h"

#include "winuser.h"
#include "urlmon.h"
#include "urlmon_main.h"
#include "ole2.h"

WINE_DEFAULT_DEBUG_CHANNEL(urlmon);

LONG URLMON_refCount = 0;

HINSTANCE URLMON_hInstance = 0;
static HMODULE hCabinet = NULL;

DWORD urlmon_tls = 0;

static void init_session(BOOL);

/***********************************************************************
 *		DllMain (URLMON.init)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
    TRACE("%p 0x%lx %p\n", hinstDLL, fdwReason, fImpLoad);

    switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        URLMON_hInstance = hinstDLL;
        init_session(TRUE);
	break;

    case DLL_PROCESS_DETACH:
        if(urlmon_tls)
            TlsFree(urlmon_tls);
        if (hCabinet)
            FreeLibrary(hCabinet);
        hCabinet = NULL;
        init_session(FALSE);
        URLMON_hInstance = 0;
	break;
    }
    return TRUE;
}


/***********************************************************************
 *		DllInstall (URLMON.@)
 */
HRESULT WINAPI DllInstall(BOOL bInstall, LPCWSTR cmdline)
{
  FIXME("(%s, %s): stub\n", bInstall?"TRUE":"FALSE",
	debugstr_w(cmdline));

  return S_OK;
}

/***********************************************************************
 *		DllCanUnloadNow (URLMON.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return URLMON_refCount != 0 ? S_FALSE : S_OK;
}



/******************************************************************************
 * Urlmon ClassFactory
 */
typedef struct {
    const IClassFactoryVtbl *lpClassFactoryVtbl;

    HRESULT (*pfnCreateInstance)(IUnknown *pUnkOuter, LPVOID *ppObj);
} ClassFactory;

#define CLASSFACTORY(x)  ((IClassFactory*)  &(x)->lpClassFactoryVtbl)

static HRESULT WINAPI CF_QueryInterface(IClassFactory *iface, REFIID riid, LPVOID *ppv)
{
    *ppv = NULL;

    if(IsEqualGUID(riid, &IID_IUnknown)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
        *ppv = iface;
    }else if(IsEqualGUID(riid, &IID_IClassFactory)) {
        TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
        *ppv = iface;
    }

    if(*ppv) {
	IUnknown_AddRef((IUnknown*)*ppv);
	return S_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", iface, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI CF_AddRef(IClassFactory *iface)
{
    URLMON_LockModule();
    return 2;
}

static ULONG WINAPI CF_Release(IClassFactory *iface)
{
    URLMON_UnlockModule();
    return 1;
}


static HRESULT WINAPI CF_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                        REFIID riid, LPVOID *ppobj)
{
    ClassFactory *This = (ClassFactory*)iface;
    HRESULT hres;
    LPUNKNOWN punk;
    
    TRACE("(%p)->(%p,%s,%p)\n",This,pOuter,debugstr_guid(riid),ppobj);

    *ppobj = NULL;
    if(SUCCEEDED(hres = This->pfnCreateInstance(pOuter, (LPVOID *) &punk))) {
        hres = IUnknown_QueryInterface(punk, riid, ppobj);
        IUnknown_Release(punk);
    }
    return hres;
}

static HRESULT WINAPI CF_LockServer(LPCLASSFACTORY iface,BOOL dolock)
{
    TRACE("(%d)\n", dolock);

    if (dolock)
	   URLMON_LockModule();
    else
	   URLMON_UnlockModule();

    return S_OK;
}

static const IClassFactoryVtbl ClassFactoryVtbl =
{
    CF_QueryInterface,
    CF_AddRef,
    CF_Release,
    CF_CreateInstance,
    CF_LockServer
};

static const ClassFactory FileProtocolCF =
    { &ClassFactoryVtbl, FileProtocol_Construct};
static const ClassFactory FtpProtocolCF =
    { &ClassFactoryVtbl, FtpProtocol_Construct};
static const ClassFactory HttpProtocolCF =
    { &ClassFactoryVtbl, HttpProtocol_Construct};
static const ClassFactory SecurityManagerCF =
    { &ClassFactoryVtbl, SecManagerImpl_Construct};
static const ClassFactory ZoneManagerCF =
    { &ClassFactoryVtbl, ZoneMgrImpl_Construct};
 
struct object_creation_info
{
    const CLSID *clsid;
    IClassFactory *cf;
    LPCWSTR protocol;
};

static const WCHAR wszFile[] = {'f','i','l','e',0};
static const WCHAR wszFtp[]  = {'f','t','p',0};
static const WCHAR wszHttp[] = {'h','t','t','p',0};

static const struct object_creation_info object_creation[] =
{
    { &CLSID_FileProtocol,            CLASSFACTORY(&FileProtocolCF),    wszFile },
    { &CLSID_FtpProtocol,             CLASSFACTORY(&FtpProtocolCF),     wszFtp  },
    { &CLSID_HttpProtocol,            CLASSFACTORY(&HttpProtocolCF),    wszHttp },
    { &CLSID_InternetSecurityManager, CLASSFACTORY(&SecurityManagerCF), NULL    },
    { &CLSID_InternetZoneManager,     CLASSFACTORY(&ZoneManagerCF),     NULL    }
};

static void init_session(BOOL init)
{
    IInternetSession *session;
    int i;

    CoInternetGetSession(0, &session, 0);

    for(i=0; i < sizeof(object_creation)/sizeof(object_creation[0]); i++) {
        if(object_creation[i].protocol) {
            if(init)
                IInternetSession_RegisterNameSpace(session, object_creation[i].cf,
                        object_creation[i].clsid, object_creation[i].protocol, 0, NULL, 0);
            else
                IInternetSession_UnregisterNameSpace(session, object_creation[i].cf,
                        object_creation[i].protocol);
        }
    }

    IInternetSession_Release(session);
}

/*******************************************************************************
 * DllGetClassObject [URLMON.@]
 * Retrieves class object from a DLL object
 *
 * NOTES
 *    Docs say returns STDAPI
 *
 * PARAMS
 *    rclsid [I] CLSID for the class object
 *    riid   [I] Reference to identifier of interface for class object
 *    ppv    [O] Address of variable to receive interface pointer for riid
 *
 * RETURNS
 *    Success: S_OK
 *    Failure: CLASS_E_CLASSNOTAVAILABLE, E_OUTOFMEMORY, E_INVALIDARG,
 *             E_UNEXPECTED
 */

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    int i;
    
    TRACE("(%s,%s,%p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    
    for (i=0; i < sizeof(object_creation)/sizeof(object_creation[0]); i++)
    {
	if (IsEqualGUID(object_creation[i].clsid, rclsid))
	    return IClassFactory_QueryInterface(object_creation[i].cf, riid, ppv);
    }

    FIXME("%s: no class found.\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}


/***********************************************************************
 *		DllRegisterServerEx (URLMON.@)
 */
HRESULT WINAPI DllRegisterServerEx(void)
{
    FIXME("(void): stub\n");

    return E_FAIL;
}

/**************************************************************************
 *                 UrlMkSetSessionOption (URLMON.@)
 */
HRESULT WINAPI UrlMkSetSessionOption(DWORD dwOption, LPVOID pBuffer, DWORD dwBufferLength,
 					DWORD Reserved)
{
    FIXME("(%#lx, %p, %#lx): stub\n", dwOption, pBuffer, dwBufferLength);

    return S_OK;
}

static const CHAR Agent[] = "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0)";

/**************************************************************************
 *                 ObtainUserAgentString (URLMON.@)
 */
HRESULT WINAPI ObtainUserAgentString(DWORD dwOption, LPSTR pcszUAOut, DWORD *cbSize)
{
    FIXME("(%ld, %p, %p): stub\n", dwOption, pcszUAOut, cbSize);

    if(dwOption) {
      ERR("dwOption: %ld, must be zero\n", dwOption);
    }

    if (sizeof(Agent) < *cbSize)
        *cbSize = sizeof(Agent);
    lstrcpynA(pcszUAOut, Agent, *cbSize); 

    return S_OK;
}

HRESULT WINAPI CoInternetCompareUrl(LPCWSTR pwzUrl1, LPCWSTR pwzUrl2, DWORD dwCompareFlags)
{
    TRACE("(%s,%s,%08lx)\n", debugstr_w(pwzUrl1), debugstr_w(pwzUrl2), dwCompareFlags);
    return UrlCompareW(pwzUrl1, pwzUrl2, dwCompareFlags)==0?S_OK:S_FALSE;
}

/**************************************************************************
 *                 IsValidURL (URLMON.@)
 * 
 * Determines if a specified string is a valid URL.
 *
 * PARAMS
 *  pBC        [I] ignored, must be NULL.
 *  szURL      [I] string that represents the URL in question.
 *  dwReserved [I] reserved and must be zero.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: S_FALSE.
 *  returns E_INVALIDARG if one or more of the args is invalid.
 *
 * TODO:
 *  test functionality against windows to see what a valid URL is.
 */
HRESULT WINAPI IsValidURL(LPBC pBC, LPCWSTR szURL, DWORD dwReserved)
{
    FIXME("(%p, %s, %ld): stub\n", pBC, debugstr_w(szURL), dwReserved);
    
    if (pBC != NULL || dwReserved != 0)
        return E_INVALIDARG;
    
    return S_OK;
}

/**************************************************************************
 *                 FaultInIEFeature (URLMON.@)
 *
 *  Undocumented.  Appears to be used by native shdocvw.dll.
 */
HRESULT WINAPI FaultInIEFeature( HWND hwnd, uCLSSPEC * pClassSpec,
                                 QUERYCONTEXT *pQuery, DWORD flags )
{
    FIXME("%p %p %p %08lx\n", hwnd, pClassSpec, pQuery, flags);
    return E_NOTIMPL;
}

/**************************************************************************
 *                 CoGetClassObjectFromURL (URLMON.@)
 */
HRESULT WINAPI CoGetClassObjectFromURL( REFCLSID rclsid, LPCWSTR szCodeURL, DWORD dwFileVersionMS,
                                        DWORD dwFileVersionLS, LPCWSTR szContentType,
                                        LPBINDCTX pBindCtx, DWORD dwClsContext, LPVOID pvReserved,
                                        REFIID riid, LPVOID *ppv )
{
    FIXME("(%s %s %ld %ld %s %p %ld %p %s %p) Stub!\n", debugstr_guid(rclsid), debugstr_w(szCodeURL),
	dwFileVersionMS, dwFileVersionLS, debugstr_w(szContentType), pBindCtx, dwClsContext, pvReserved,
	debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

/***********************************************************************
 *           ReleaseBindInfo (URLMON.@)
 *
 * Release the resources used by the specified BINDINFO structure.
 *
 * PARAMS
 *  pbindinfo [I] BINDINFO to release.
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI ReleaseBindInfo(BINDINFO* pbindinfo)
{
    DWORD size;

    TRACE("(%p)\n", pbindinfo);

    if(!pbindinfo || !(size = pbindinfo->cbSize))
        return;

    CoTaskMemFree(pbindinfo->szExtraInfo);
    ReleaseStgMedium(&pbindinfo->stgmedData);

    if(offsetof(BINDINFO, szExtraInfo) < size)
        CoTaskMemFree(pbindinfo->szCustomVerb);


    if(pbindinfo->pUnk && offsetof(BINDINFO, pUnk) < size)
        IUnknown_Release(pbindinfo->pUnk);

    memset(pbindinfo, 0, size);
    pbindinfo->cbSize = size;
}

/***********************************************************************
 *           FindMimeFromData (URLMON.@)
 *
 * Determines the Multipurpose Internet Mail Extensions (MIME) type from the data provided.
 */
static BOOL text_html_filter(LPVOID buf, DWORD size)
{
    const char *b = buf;
    int i;

    if(size < 5)
        return FALSE;

    for(i=0; i < size-5; i++) {
        if(b[i] == '<'
           && (b[i+1] == 'h' || b[i+1] == 'H')
           && (b[i+2] == 't' || b[i+2] == 'T')
           && (b[i+3] == 'm' || b[i+3] == 'M')
           && (b[i+4] == 'l' || b[i+4] == 'L'))
            return TRUE;
    }

    return FALSE;
}

static BOOL image_gif_filter(LPVOID buf, DWORD size)
{
    const BYTE const *b = buf;

    return size >= 6
        && (b[0] == 'G' || b[0] == 'g')
        && (b[1] == 'I' || b[1] == 'i')
        && (b[2] == 'F' || b[2] == 'f')
        &&  b[3] == '8'
        && (b[4] == '7' || b[4] == '9')
        && (b[5] == 'A' || b[5] == 'a');
}

static BOOL image_pjpeg_filter(LPVOID buf, DWORD size)
{
    return size > 2 && *(BYTE*)buf == 0xff && *((BYTE*)buf+1) == 0xd8;
}

static BOOL image_xpng_filter(LPVOID buf, DWORD size)
{
    static const BYTE xpng_header[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};
    return size > sizeof(xpng_header) && !memcmp(buf, xpng_header, sizeof(xpng_header));
}

static BOOL image_bmp_filter(LPVOID buf, DWORD size)
{
    return size >= 14
        && *(BYTE*)buf == 0x42
        && *((BYTE*)buf+1) == 0x4d
        && *(DWORD*)((BYTE*)buf+6) == 0;
}

static BOOL text_plain_filter(LPVOID buf, DWORD size)
{
    UCHAR *ptr;

    for(ptr = buf; ptr < (UCHAR*)buf+size-1; ptr++) {
        if(*ptr < 0x20 && *ptr != '\n' && *ptr != '\r' && *ptr != '\t')
            return FALSE;
    }

    return TRUE;
}

static BOOL application_octet_stream_filter(LPVOID buf, DWORD size)
{
    return TRUE;
}

HRESULT WINAPI FindMimeFromData(LPBC pBC, LPCWSTR pwzUrl, LPVOID pBuffer,
        DWORD cbSize, LPCWSTR pwzMimeProposed, DWORD dwMimeFlags,
        LPWSTR* ppwzMimeOut, DWORD dwReserved)
{
    TRACE("(%p,%s,%p,%ld,%s,0x%lx,%p,0x%lx)\n", pBC, debugstr_w(pwzUrl), pBuffer, cbSize,
            debugstr_w(pwzMimeProposed), dwMimeFlags, ppwzMimeOut, dwReserved);

    if(dwMimeFlags)
        WARN("dwMimeFlags=%08lx\n", dwMimeFlags);
    if(dwReserved)
        WARN("dwReserved=%ld\n", dwReserved);

    /* pBC seams to not be used */

    if(!ppwzMimeOut || (!pwzUrl && !pBuffer))
        return E_INVALIDARG;

    if(pwzMimeProposed && (!pBuffer || (pBuffer && !cbSize))) {
        DWORD len;

        if(!pwzMimeProposed)
            return E_FAIL;

        len = strlenW(pwzMimeProposed)+1;
        *ppwzMimeOut = CoTaskMemAlloc(len*sizeof(WCHAR));
        memcpy(*ppwzMimeOut, pwzMimeProposed, len*sizeof(WCHAR));
        return S_OK;
    }

    if(pBuffer) {
        DWORD len;
        LPCWSTR ret = NULL;
        int i;

        static const WCHAR wszTextHtml[] = {'t','e','x','t','/','h','t','m','l',0};
        static const WCHAR wszImageGif[] = {'i','m','a','g','e','/','g','i','f',0};
        static const WCHAR wszImagePjpeg[] = {'i','m','a','g','e','/','p','j','p','e','g',0};
        static const WCHAR wszImageXPng[] = {'i','m','a','g','e','/','x','-','p','n','g',0};
        static const WCHAR wszImageBmp[] = {'i','m','a','g','e','/','b','m','p',0};
        static const WCHAR wszTextPlain[] = {'t','e','x','t','/','p','l','a','i','n','\0'};
        static const WCHAR wszAppOctetStream[] = {'a','p','p','l','i','c','a','t','i','o','n','/',
            'o','c','t','e','t','-','s','t','r','e','a','m','\0'};

        static const struct {
            LPCWSTR mime;
            BOOL (*filter)(LPVOID,DWORD);
        } mime_filters[] = {
            {wszTextHtml,       text_html_filter},
            {wszImageGif,       image_gif_filter},
            {wszImagePjpeg,     image_pjpeg_filter},
            {wszImageXPng,      image_xpng_filter},
            {wszImageBmp,       image_bmp_filter},
            {wszTextPlain,      text_plain_filter},
            {wszAppOctetStream, application_octet_stream_filter}
        };

        if(!cbSize)
            return E_FAIL;

        if(pwzMimeProposed && strcmpW(pwzMimeProposed, wszAppOctetStream)) {
            for(i=0; i < sizeof(mime_filters)/sizeof(*mime_filters); i++) {
                if(!strcmpW(pwzMimeProposed, mime_filters[i].mime))
                    break;
            }

            if(i == sizeof(mime_filters)/sizeof(*mime_filters)
                    || mime_filters[i].filter(pBuffer, cbSize)) {
                len = strlenW(pwzMimeProposed)+1;
                *ppwzMimeOut = CoTaskMemAlloc(len*sizeof(WCHAR));
                memcpy(*ppwzMimeOut, pwzMimeProposed, len*sizeof(WCHAR));
                return S_OK;
            }
        }

        i=0;
        while(!ret) {
            if(mime_filters[i].filter(pBuffer, cbSize))
                ret = mime_filters[i].mime;
            i++;
        }

        if(pwzMimeProposed) {
            if(i == sizeof(mime_filters)/sizeof(*mime_filters))
                ret = pwzMimeProposed;

            /* text/html is a special case */
           if(!strcmpW(pwzMimeProposed, wszTextHtml) && !strcmpW(ret, wszTextPlain))
                ret = wszTextHtml;
        }

        len = strlenW(ret)+1;
        *ppwzMimeOut = CoTaskMemAlloc(len*sizeof(WCHAR));
        memcpy(*ppwzMimeOut, ret, len*sizeof(WCHAR));
        return S_OK;
    }

    if(pwzUrl) {
        HKEY hkey;
        DWORD res, size;
        LPCWSTR ptr;
        WCHAR mime[64];

        static const WCHAR wszContentType[] =
                {'C','o','n','t','e','n','t',' ','T','y','p','e','\0'};

        ptr = strrchrW(pwzUrl, '.');
        if(!ptr)
            return E_FAIL;

        res = RegOpenKeyW(HKEY_CLASSES_ROOT, ptr, &hkey);
        if(res != ERROR_SUCCESS)
            return E_FAIL;

        size = sizeof(mime);
        res = RegQueryValueExW(hkey, wszContentType, NULL, NULL, (LPBYTE)mime, &size);
        RegCloseKey(hkey);
        if(res != ERROR_SUCCESS)
            return E_FAIL;

        *ppwzMimeOut = CoTaskMemAlloc(size);
        memcpy(*ppwzMimeOut, mime, size);
        return S_OK;
    }

    return E_FAIL;
}

/***********************************************************************
 * Extract (URLMON.@)
 */
HRESULT WINAPI Extract(void *dest, LPCSTR szCabName)
{
    HRESULT (WINAPI *pExtract)(void *, LPCSTR);

    if (!hCabinet)
        hCabinet = LoadLibraryA("cabinet.dll");

    if (!hCabinet) return HRESULT_FROM_WIN32(GetLastError());
    pExtract = (void *)GetProcAddress(hCabinet, "Extract");
    if (!pExtract) return HRESULT_FROM_WIN32(GetLastError());

    return pExtract(dest, szCabName);
}
