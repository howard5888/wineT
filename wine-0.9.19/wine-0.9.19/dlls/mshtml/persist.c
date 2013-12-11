/*
 * Copyright 2005 Jacek Caban
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

#include <stdarg.h>
#include <stdio.h>

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlguid.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

/**********************************************************
 * IPersistMoniker implementation
 */

#define PERSISTMON_THIS(iface) DEFINE_THIS(HTMLDocument, PersistMoniker, iface)

static HRESULT WINAPI PersistMoniker_QueryInterface(IPersistMoniker *iface, REFIID riid,
                                                            void **ppvObject)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistMoniker_AddRef(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistMoniker_Release(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistMoniker_GetClassID(IPersistMoniker *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistMoniker_IsDirty(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static nsIInputStream *get_post_data_stream(IBindCtx *bctx)
{
    nsIInputStream *ret = NULL;
    IBindStatusCallback *callback;
    IHttpNegotiate *http_negotiate;
    BINDINFO bindinfo;
    DWORD bindf = 0;
    DWORD post_len = 0, headers_len = 0;
    LPWSTR headers = NULL;
    WCHAR emptystr[] = {0};
    char *data;
    HRESULT hres;

    static WCHAR _BSCB_Holder_[] =
        {'_','B','S','C','B','_','H','o','l','d','e','r','_',0};


    /* FIXME: This should be done in URLMoniker */
    if(!bctx)
        return NULL;

    hres = IBindCtx_GetObjectParam(bctx, _BSCB_Holder_, (IUnknown**)&callback);
    if(FAILED(hres))
        return NULL;

    hres = IBindStatusCallback_QueryInterface(callback, &IID_IHttpNegotiate,
                                              (void**)&http_negotiate);
    if(SUCCEEDED(hres)) {
        hres = IHttpNegotiate_BeginningTransaction(http_negotiate, emptystr,
                                                   emptystr, 0, &headers);
        IHttpNegotiate_Release(http_negotiate);

        if(SUCCEEDED(hres) && headers)
            headers_len = WideCharToMultiByte(CP_ACP, 0, headers, -1, NULL, 0, NULL, NULL);
    }

    memset(&bindinfo, 0, sizeof(bindinfo));
    bindinfo.cbSize = sizeof(bindinfo);

    hres = IBindStatusCallback_GetBindInfo(callback, &bindf, &bindinfo);

    if(SUCCEEDED(hres) && bindinfo.dwBindVerb == BINDVERB_POST)
        post_len = bindinfo.cbStgmedData;

    if(headers_len || post_len) {
        int len = headers_len ? headers_len-1 : 0;

        static const char content_length[] = "Content-Length: %lu\r\n\r\n";

        data = mshtml_alloc(headers_len+post_len+sizeof(content_length)+8);

        if(headers_len) {
            WideCharToMultiByte(CP_ACP, 0, headers, -1, data, -1, NULL, NULL);
            CoTaskMemFree(headers);
        }

        if(post_len) {
            sprintf(data+len, content_length, post_len);
            len = strlen(data);

            memcpy(data+len, bindinfo.stgmedData.u.hGlobal, post_len);
        }

        TRACE("data = %s\n", debugstr_an(data, len+post_len));

        ret = create_nsstream(data, len+post_len);
    }

    ReleaseBindInfo(&bindinfo);
    IBindStatusCallback_Release(callback);

    return ret;
}

static HRESULT WINAPI PersistMoniker_Load(IPersistMoniker *iface, BOOL fFullyAvailable,
        IMoniker *pimkName, LPBC pibc, DWORD grfMode)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    BSCallback *bscallback;
    LPOLESTR url = NULL;
    HRESULT hres;
    nsresult nsres;

    TRACE("(%p)->(%x %p %p %08lx)\n", This, fFullyAvailable, pimkName, pibc, grfMode);

    if(pibc) {
        IUnknown *unk = NULL;

        /* FIXME:
         * Use params:
         * "__PrecreatedObject"
         * "BIND_CONTEXT_PARAM"
         * "__HTMLLOADOPTIONS"
         * "__DWNBINDINFO"
         * "URL Context"
         * "CBinding Context"
         * "_ITransData_Object_"
         * "_EnumFORMATETC_"
         */

        IBindCtx_GetObjectParam(pibc, (LPOLESTR)SZ_HTML_CLIENTSITE_OBJECTPARAM, &unk);
        if(unk) {
            IOleClientSite *client = NULL;

            hres = IUnknown_QueryInterface(unk, &IID_IOleClientSite, (void**)&client);
            if(SUCCEEDED(hres)) {
                TRACE("Got client site %p\n", client);
                IOleObject_SetClientSite(OLEOBJ(This), client);
                IOleClientSite_Release(client);
            }

            IUnknown_Release(unk);
        }
    }

    HTMLDocument_LockContainer(This, TRUE);
    
    hres = IMoniker_GetDisplayName(pimkName, pibc, NULL, &url);
    if(FAILED(hres)) {
        WARN("GetDiaplayName failed: %08lx\n", hres);
        return hres;
    }

    TRACE("got url: %s\n", debugstr_w(url));

    if(This->client) {
        IOleCommandTarget *cmdtrg = NULL;

        hres = IOleClientSite_QueryInterface(This->client, &IID_IOleCommandTarget,
                (void**)&cmdtrg);
        if(SUCCEEDED(hres)) {
            VARIANT var;

            V_VT(&var) = VT_I4;
            V_I4(&var) = 0;
            IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 37, 0, &var, NULL);
        }
    }

    bscallback = create_bscallback(This, pimkName);

    if(This->nscontainer) {
        nsIInputStream *post_data_stream = get_post_data_stream(pibc);

        This->nscontainer->bscallback = bscallback;
        nsres = nsIWebNavigation_LoadURI(This->nscontainer->navigation, url,
                LOAD_FLAGS_NONE, NULL, post_data_stream, NULL);
        This->nscontainer->bscallback = NULL;

        if(post_data_stream)
            nsIInputStream_Release(post_data_stream);

        if(!bscallback->nschannel)
            ERR("bscallback->nschannel == NULL\n");

        if(NS_SUCCEEDED(nsres)) {
            /* FIXME: don't return here (URL Moniker needs to be good enough) */

            IBindStatusCallback_Release(STATUSCLB(bscallback));
            CoTaskMemFree(url);
            return S_OK;
        }else if(nsres != WINE_NS_LOAD_FROM_MONIKER) {
            WARN("LoadURI failed: %08lx\n", nsres);
        }
    }

    /* FIXME: Use grfMode */

    if(fFullyAvailable)
        FIXME("not supported fFullyAvailable\n");
    if(pibc)
        FIXME("not supported pibc\n");

    hres = start_binding(bscallback);

    IBindStatusCallback_Release(STATUSCLB(bscallback));
    CoTaskMemFree(url);

    return hres;
}

static HRESULT WINAPI PersistMoniker_Save(IPersistMoniker *iface, IMoniker *pimkName,
        LPBC pbc, BOOL fRemember)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p %x)\n", This, pimkName, pbc, fRemember);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_SaveCompleted(IPersistMoniker *iface, IMoniker *pimkName, LPBC pibc)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, pimkName, pibc);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_GetCurMoniker(IPersistMoniker *iface, IMoniker **ppimkName)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p)\n", This, ppimkName);
    return E_NOTIMPL;
}

static const IPersistMonikerVtbl PersistMonikerVtbl = {
    PersistMoniker_QueryInterface,
    PersistMoniker_AddRef,
    PersistMoniker_Release,
    PersistMoniker_GetClassID,
    PersistMoniker_IsDirty,
    PersistMoniker_Load,
    PersistMoniker_Save,
    PersistMoniker_SaveCompleted,
    PersistMoniker_GetCurMoniker
};

/**********************************************************
 * IMonikerProp implementation
 */

#define MONPROP_THIS(iface) DEFINE_THIS(HTMLDocument, MonikerProp, iface)

static HRESULT WINAPI MonikerProp_QueryInterface(IMonikerProp *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI MonikerProp_AddRef(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI MonikerProp_Release(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument_Release(HTMLDOC(This));
}

static HRESULT WINAPI MonikerProp_PutProperty(IMonikerProp *iface, MONIKERPROPERTY mkp, LPCWSTR val)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    FIXME("(%p)->(%d %s)\n", This, mkp, debugstr_w(val));
    return E_NOTIMPL;
}

static const IMonikerPropVtbl MonikerPropVtbl = {
    MonikerProp_QueryInterface,
    MonikerProp_AddRef,
    MonikerProp_Release,
    MonikerProp_PutProperty
};

/**********************************************************
 * IPersistFile implementation
 */

#define PERSISTFILE_THIS(iface) DEFINE_THIS(HTMLDocument, PersistFile, iface)

static HRESULT WINAPI PersistFile_QueryInterface(IPersistFile *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistFile_AddRef(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistFile_Release(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistFile_GetClassID(IPersistFile *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pClassID);

    if(!pClassID)
        return E_INVALIDARG;

    memcpy(pClassID, &CLSID_HTMLDocument, sizeof(CLSID));
    return S_OK;
}

static HRESULT WINAPI PersistFile_IsDirty(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Load(IPersistFile *iface, LPCOLESTR pszFileName, DWORD dwMode)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s %08lx)\n", This, debugstr_w(pszFileName), dwMode);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Save(IPersistFile *iface, LPCOLESTR pszFileName, BOOL fRemember)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s %x)\n", This, debugstr_w(pszFileName), fRemember);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_SaveCompleted(IPersistFile *iface, LPCOLESTR pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(pszFileName));
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_GetCurFile(IPersistFile *iface, LPOLESTR *pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pszFileName);
    return E_NOTIMPL;
}

static const IPersistFileVtbl PersistFileVtbl = {
    PersistFile_QueryInterface,
    PersistFile_AddRef,
    PersistFile_Release,
    PersistFile_GetClassID,
    PersistFile_IsDirty,
    PersistFile_Load,
    PersistFile_Save,
    PersistFile_SaveCompleted,
    PersistFile_GetCurFile
};

#define PERSTRINIT_THIS(iface) DEFINE_THIS(HTMLDocument, PersistStreamInit, iface);

static HRESULT WINAPI PersistStreamInit_QueryInterface(IPersistStreamInit *iface,
                                                       REFIID riid, void **ppv)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppv);
}

static ULONG WINAPI PersistStreamInit_AddRef(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistStreamInit_Release(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static HRESULT WINAPI PersistStreamInit_GetClassID(IPersistStreamInit *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistStreamInit_IsDirty(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_Load(IPersistStreamInit *iface, LPSTREAM pStm)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pStm);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_Save(IPersistStreamInit *iface, LPSTREAM pStm,
                                             BOOL fClearDirty)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)->(%p %x)\n", This, pStm, fClearDirty);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_GetSizeMax(IPersistStreamInit *iface,
                                                   ULARGE_INTEGER *pcbSize)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pcbSize);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_InitNew(IPersistStreamInit *iface)
{
    HTMLDocument *This = PERSTRINIT_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

#undef PERSTRINIT_THIS

static const IPersistStreamInitVtbl PersistStreamInitVtbl = {
    PersistStreamInit_QueryInterface,
    PersistStreamInit_AddRef,
    PersistStreamInit_Release,
    PersistStreamInit_GetClassID,
    PersistStreamInit_IsDirty,
    PersistStreamInit_Load,
    PersistStreamInit_Save,
    PersistStreamInit_GetSizeMax,
    PersistStreamInit_InitNew
};

void HTMLDocument_Persist_Init(HTMLDocument *This)
{
    This->lpPersistMonikerVtbl = &PersistMonikerVtbl;
    This->lpPersistFileVtbl = &PersistFileVtbl;
    This->lpMonikerPropVtbl = &MonikerPropVtbl;
    This->lpPersistStreamInitVtbl = &PersistStreamInitVtbl;
}
