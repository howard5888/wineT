/*
 * Implementation of IWebBrowser interface for WebBrowser control
 *
 * Copyright 2001 John R. Sheets (for CodeWeavers)
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

#include "wine/debug.h"
#include "shdocvw.h"
#include "exdispid.h"

WINE_DEFAULT_DEBUG_CHANNEL(shdocvw);

/**********************************************************************
 * Implement the IWebBrowser interface
 */

#define WEBBROWSER_THIS(iface) DEFINE_THIS(WebBrowser, WebBrowser2, iface)

static HRESULT WINAPI WebBrowser_QueryInterface(IWebBrowser2 *iface, REFIID riid, LPVOID *ppv)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    if (ppv == NULL)
        return E_POINTER;
    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = WEBBROWSER(This);
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = WEBBROWSER(This);
    }else if(IsEqualGUID(&IID_IWebBrowser, riid)) {
        TRACE("(%p)->(IID_IWebBrowser %p)\n", This, ppv);
        *ppv = WEBBROWSER(This);
    }else if(IsEqualGUID(&IID_IWebBrowserApp, riid)) {
        TRACE("(%p)->(IID_IWebBrowserApp %p)\n", This, ppv);
        *ppv = WEBBROWSER(This);
    }else if(IsEqualGUID(&IID_IWebBrowser2, riid)) {
        TRACE("(%p)->(IID_IWebBrowser2 %p)\n", This, ppv);
        *ppv = WEBBROWSER(This);
    }else if(IsEqualGUID(&IID_IOleObject, riid)) {
        TRACE("(%p)->(IID_IOleObject %p)\n", This, ppv);
        *ppv = OLEOBJ(This);
    }else if(IsEqualGUID(&IID_IOleWindow, riid)) {
        TRACE("(%p)->(IID_IOleWindow %p)\n", This, ppv);
        *ppv = INPLACEOBJ(This);
    }else if(IsEqualGUID (&IID_IOleInPlaceObject, riid)) {
        TRACE("(%p)->(IID_IOleInPlaceObject %p)\n", This, ppv);
        *ppv = INPLACEOBJ(This);
    }else if(IsEqualGUID(&IID_IOleControl, riid)) {
        TRACE("(%p)->(IID_IOleControl %p)\n", This, ppv);
        *ppv = CONTROL(This);
    }else if(IsEqualGUID(&IID_IPersist, riid)) {
        TRACE("(%p)->(IID_IPersist %p)\n", This, ppv);
        *ppv = PERSTORAGE(This);
    }else if(IsEqualGUID(&IID_IPersistStorage, riid)) {
        TRACE("(%p)->(IID_IPersistStorage %p)\n", This, ppv);
        *ppv = PERSTORAGE(This);
    }else if(IsEqualGUID (&IID_IPersistStreamInit, riid)) {
        TRACE("(%p)->(IID_IPersistStreamInit %p)\n", This, ppv);
        *ppv = PERSTRINIT(This);
    }else if(IsEqualGUID(&IID_IProvideClassInfo, riid)) {
        TRACE("(%p)->(IID_IProvideClassInfo %p)\n", This, ppv);
        *ppv = CLASSINFO(This);
    }else if(IsEqualGUID(&IID_IProvideClassInfo2, riid)) {
        TRACE("(%p)->(IID_IProvideClassInfo2 %p)\n", This, ppv);
        *ppv = CLASSINFO(This);
    }else if(IsEqualGUID(&IID_IConnectionPointContainer, riid)) {
        TRACE("(%p)->(IID_IConnectionPointContainer %p)\n", This, ppv);
        *ppv = CONPTCONT(&This->doc_host.cps);
    }else if(IsEqualGUID(&IID_IViewObject, riid)) {
        TRACE("(%p)->(IID_IViewObject %p)\n", This, ppv);
        *ppv = VIEWOBJ(This);
    }else if(IsEqualGUID(&IID_IViewObject2, riid)) {
        TRACE("(%p)->(IID_IViewObject2 %p)\n", This, ppv);
        *ppv = VIEWOBJ2(This);
    }else if(IsEqualGUID(&IID_IOleInPlaceActiveObject, riid)) {
        TRACE("(%p)->(IID_IOleInPlaceActiveObject %p)\n", This, ppv);
        *ppv = ACTIVEOBJ(This);
    }else if(IsEqualGUID(&IID_IOleCommandTarget, riid)) {
        TRACE("(%p)->(IID_IOleCommandTarget %p)\n", This, ppv);
        *ppv = OLECMD(This);
    }else if(IsEqualGUID(&IID_IHlinkFrame, riid)) {
        TRACE("(%p)->(IID_IHlinkFrame %p)\n", This, ppv);
        *ppv = HLINKFRAME(This);
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    FIXME("(%p)->(%s %p) interface not supported\n", This, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI WebBrowser_AddRef(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref=%ld\n", This, ref);
    return ref;
}

static ULONG WINAPI WebBrowser_Release(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->doc_host.document)
            IUnknown_Release(This->doc_host.document);

        DocHost_Release(&This->doc_host);

        WebBrowser_OleObject_Destroy(This);

        shdocvw_free(This);
        SHDOCVW_UnlockModule();
    }

    return ref;
}

/* IDispatch methods */
static HRESULT WINAPI WebBrowser_GetTypeInfoCount(IWebBrowser2 *iface, UINT *pctinfo)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GetTypeInfo(IWebBrowser2 *iface, UINT iTInfo, LCID lcid,
                                     LPTYPEINFO *ppTInfo)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%d %ld %p)\n", This, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GetIDsOfNames(IWebBrowser2 *iface, REFIID riid,
                                       LPOLESTR *rgszNames, UINT cNames,
                                       LCID lcid, DISPID *rgDispId)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%s %p %d %ld %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
            lcid, rgDispId);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Invoke(IWebBrowser2 *iface, DISPID dispIdMember,
                                REFIID riid, LCID lcid, WORD wFlags,
                                DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                EXCEPINFO *pExepInfo, UINT *puArgErr)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    ITypeInfo *typeinfo;
    HRESULT hres;

    TRACE("(%p)->(%ld %s %ld %08x %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
            lcid, wFlags, pDispParams, pVarResult, pExepInfo, puArgErr);

    hres = get_typeinfo(&typeinfo);
    if(FAILED(hres))
        return hres;

    return ITypeInfo_Invoke(typeinfo, WEBBROWSER2(This), dispIdMember, wFlags, pDispParams,
                            pVarResult, pExepInfo, puArgErr);
}

/* IWebBrowser methods */
static HRESULT WINAPI WebBrowser_GoBack(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GoForward(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GoHome(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GoSearch(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Navigate(IWebBrowser2 *iface, BSTR szUrl,
                                  VARIANT *Flags, VARIANT *TargetFrameName,
                                  VARIANT *PostData, VARIANT *Headers)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANT url;
    HRESULT r;

    FIXME("(%p)->(%s %p %p %p %p)\n", This, debugstr_w(szUrl), Flags, TargetFrameName,
          PostData, Headers);

    V_VT(&url) = VT_BSTR;
    V_BSTR(&url) = szUrl;
    r = IWebBrowser2_Navigate2(iface, &url, Flags,
                               TargetFrameName, PostData, Headers);

    return r;
}

static HRESULT WINAPI WebBrowser_Refresh(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Refresh2(IWebBrowser2 *iface, VARIANT *Level)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Level);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Stop(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Application(IWebBrowser2 *iface, IDispatch **ppDisp)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, ppDisp);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Parent(IWebBrowser2 *iface, IDispatch **ppDisp)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, ppDisp);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Container(IWebBrowser2 *iface, IDispatch **ppDisp)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, ppDisp);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Document(IWebBrowser2 *iface, IDispatch **ppDisp)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, ppDisp);

    *ppDisp = NULL;
    if(This->doc_host.document)
        IUnknown_QueryInterface(This->doc_host.document, &IID_IDispatch, (void**)ppDisp);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_TopLevelContainer(IWebBrowser2 *iface, VARIANT_BOOL *pBool)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pBool);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Type(IWebBrowser2 *iface, BSTR *Type)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Type);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Left(IWebBrowser2 *iface, long *pl)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pl);

    *pl = This->pos_rect.left;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_Left(IWebBrowser2 *iface, long Left)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    RECT rect;

    TRACE("(%p)->(%ld)\n", This, Left);

    if(!This->inplace)
        return E_UNEXPECTED;

    memcpy(&rect, &This->pos_rect, sizeof(RECT));
    rect.left = Left;

    /* We don't really change the window position here.
     * We just notify the embedder that he should do so. */
    return IOleInPlaceSite_OnPosRectChange(This->inplace, &rect);
}

static HRESULT WINAPI WebBrowser_get_Top(IWebBrowser2 *iface, long *pl)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pl);

    *pl = This->pos_rect.top;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_Top(IWebBrowser2 *iface, long Top)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    RECT rect;

    TRACE("(%p)->(%ld)\n", This, Top);

    if(!This->inplace)
        return E_UNEXPECTED;

    memcpy(&rect, &This->pos_rect, sizeof(RECT));
    rect.top = Top;

    /* We don't really change the window position here.
     * We just notify the embedder that he should do so. */
    return IOleInPlaceSite_OnPosRectChange(This->inplace, &rect);
}

static HRESULT WINAPI WebBrowser_get_Width(IWebBrowser2 *iface, long *pl)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pl);

    *pl = This->pos_rect.right - This->pos_rect.left;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_Width(IWebBrowser2 *iface, long Width)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    RECT rect;

    TRACE("(%p)->(%ld)\n", This, Width);

    if(!This->inplace)
        return E_UNEXPECTED;

    memcpy(&rect, &This->pos_rect, sizeof(RECT));
    rect.right = rect.left+Width;
 
    /* We don't really change the window size here.
     * We just notify the embedder that he should do so. */
   return IOleInPlaceSite_OnPosRectChange(This->inplace, &rect);
}

static HRESULT WINAPI WebBrowser_get_Height(IWebBrowser2 *iface, long *pl)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pl);

    *pl = This->pos_rect.bottom - This->pos_rect.top;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_Height(IWebBrowser2 *iface, long Height)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    RECT rect;

    TRACE("(%p)->(%ld)\n", This, Height);

    if(!This->inplace)
        return E_UNEXPECTED;

    memcpy(&rect, &This->pos_rect, sizeof(RECT));
    rect.bottom = rect.top+Height;

    /* We don't really change the window size here.
     * We just notify the embedder that he should do so. */
    return IOleInPlaceSite_OnPosRectChange(This->inplace, &rect);
}

static HRESULT WINAPI WebBrowser_get_LocationName(IWebBrowser2 *iface, BSTR *LocationName)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, LocationName);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_LocationURL(IWebBrowser2 *iface, BSTR *LocationURL)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    FIXME("(%p)->(%p)\n", This, LocationURL);

    if(!This->doc_host.url) {
        static const WCHAR null_char = 0;
        *LocationURL = SysAllocString(&null_char);
        return S_FALSE;
    }

    *LocationURL = SysAllocString(This->doc_host.url);
    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_Busy(IWebBrowser2 *iface, VARIANT_BOOL *pBool)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pBool);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Quit(IWebBrowser2 *iface)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_ClientToWindow(IWebBrowser2 *iface, int *pcx, int *pcy)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, pcx, pcy);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_PutProperty(IWebBrowser2 *iface, BSTR szProperty, VARIANT vtValue)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(szProperty));
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_GetProperty(IWebBrowser2 *iface, BSTR szProperty, VARIANT *pvtValue)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(szProperty), pvtValue);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Name(IWebBrowser2 *iface, BSTR *Name)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Name);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_HWND(IWebBrowser2 *iface, long *pHWND)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pHWND);

    /* WebBrowser control never has a frame window (in opposition to InternetExplorer) */
    *pHWND = 0;
    return E_FAIL;
}

static HRESULT WINAPI WebBrowser_get_FullName(IWebBrowser2 *iface, BSTR *FullName)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, FullName);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Path(IWebBrowser2 *iface, BSTR *Path)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Path);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Visible(IWebBrowser2 *iface, VARIANT_BOOL *pBool)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pBool);

    *pBool = This->visible;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_Visible(IWebBrowser2 *iface, VARIANT_BOOL Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANTARG arg;
    DISPPARAMS dispparams = {&arg, NULL, 1, 0};

    TRACE("(%p)->(%x)\n", This, Value);

    This->visible = Value;

    V_VT(&arg) = VT_BOOL;
    V_BOOL(&arg) = Value;
    call_sink(This->doc_host.cps.wbe2, DISPID_ONVISIBLE, &dispparams);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_StatusBar(IWebBrowser2 *iface, VARIANT_BOOL *pBool)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pBool);

    *pBool = This->status_bar;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_StatusBar(IWebBrowser2 *iface, VARIANT_BOOL Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANTARG arg;
    DISPPARAMS dispparams = {&arg, NULL, 1, 0};

    TRACE("(%p)->(%x)\n", This, Value);

    This->status_bar = Value ? VARIANT_TRUE : VARIANT_FALSE;

    /* In opposition to InternetExplorer, all we should do here is
     * inform the embedder about the status bar change. */

    V_VT(&arg) = VT_BOOL;
    V_BOOL(&arg) = Value;
    call_sink(This->doc_host.cps.wbe2, DISPID_ONSTATUSBAR, &dispparams);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_StatusText(IWebBrowser2 *iface, BSTR *StatusText)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, StatusText);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_StatusText(IWebBrowser2 *iface, BSTR StatusText)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(StatusText));
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_ToolBar(IWebBrowser2 *iface, int *Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, Value);

    *Value = This->tool_bar;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_ToolBar(IWebBrowser2 *iface, int Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANTARG arg;
    DISPPARAMS dispparams = {&arg, NULL, 1, 0};

    TRACE("(%p)->(%x)\n", This, Value);

    This->tool_bar = Value ? VARIANT_TRUE : VARIANT_FALSE;

    /* In opposition to InternetExplorer, all we should do here is
     * inform the embedder about the tool bar change. */

    V_VT(&arg) = VT_BOOL;
    V_BOOL(&arg) = Value;
    call_sink(This->doc_host.cps.wbe2, DISPID_ONTOOLBAR, &dispparams);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_MenuBar(IWebBrowser2 *iface, VARIANT_BOOL *Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, Value);

    *Value = This->menu_bar;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_MenuBar(IWebBrowser2 *iface, VARIANT_BOOL Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANTARG arg;
    DISPPARAMS dispparams = {&arg, NULL, 1, 0};

    TRACE("(%p)->(%x)\n", This, Value);

    This->menu_bar = Value ? VARIANT_TRUE : VARIANT_FALSE;

    /* In opposition to InternetExplorer, all we should do here is
     * inform the embedder about the menu bar change. */

    V_VT(&arg) = VT_BOOL;
    V_BOOL(&arg) = Value;
    call_sink(This->doc_host.cps.wbe2, DISPID_ONMENUBAR, &dispparams);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_FullScreen(IWebBrowser2 *iface, VARIANT_BOOL *pbFullScreen)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbFullScreen);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_FullScreen(IWebBrowser2 *iface, VARIANT_BOOL bFullScreen)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bFullScreen);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_Navigate2(IWebBrowser2 *iface, VARIANT *URL, VARIANT *Flags,
        VARIANT *TargetFrameName, VARIANT *PostData, VARIANT *Headers)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    PBYTE post_data = NULL;
    ULONG post_data_len = 0;
    LPWSTR headers = NULL;
    HRESULT hres;

    TRACE("(%p)->(%p %p %p %p %p)\n", This, URL, Flags, TargetFrameName, PostData, Headers);

    if(!This->client)
        return E_FAIL;

    if((Flags && V_VT(Flags) != VT_EMPTY) 
       || (TargetFrameName && V_VT(TargetFrameName) != VT_EMPTY))
        FIXME("Unsupported arguments\n");


    if(!URL)
        return S_OK;

    if(V_VT(URL) != VT_BSTR)
        return E_INVALIDARG;

    if(PostData && V_VT(PostData) != VT_EMPTY) {
        if(V_VT(PostData) != (VT_ARRAY | VT_UI1)
           || V_ARRAY(PostData)->cDims != 1) {
            WARN("Invalid PostData\n");
            return E_INVALIDARG;
        }

        SafeArrayAccessData(V_ARRAY(PostData), (void**)&post_data);
        post_data_len = V_ARRAY(PostData)->rgsabound[0].cElements;
    }

    if(Headers && V_VT(Headers) != VT_EMPTY) {
        if(V_VT(Headers) != VT_BSTR)
            return E_INVALIDARG;

        headers = V_BSTR(Headers);
        TRACE("Headers: %s\n", debugstr_w(headers));
    }

    hres = navigate_url(&This->doc_host, V_BSTR(URL), post_data, post_data_len, headers);

    if(post_data)
        SafeArrayUnaccessData(V_ARRAY(PostData));

    return hres;
}

static HRESULT WINAPI WebBrowser_QueryStatusWB(IWebBrowser2 *iface, OLECMDID cmdID, OLECMDF *pcmdf)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%d %p)\n", This, cmdID, pcmdf);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_ExecWB(IWebBrowser2 *iface, OLECMDID cmdID,
        OLECMDEXECOPT cmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%d %d %p %p)\n", This, cmdID, cmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_ShowBrowserBar(IWebBrowser2 *iface, VARIANT *pvaClsid,
        VARIANT *pvarShow, VARIANT *pvarSize)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p %p %p)\n", This, pvaClsid, pvarShow, pvarSize);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_ReadyState(IWebBrowser2 *iface, READYSTATE *lpReadyState)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, lpReadyState);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Offline(IWebBrowser2 *iface, VARIANT_BOOL *pbOffline)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbOffline);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_Offline(IWebBrowser2 *iface, VARIANT_BOOL bOffline)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bOffline);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_Silent(IWebBrowser2 *iface, VARIANT_BOOL *pbSilent)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbSilent);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_Silent(IWebBrowser2 *iface, VARIANT_BOOL bSilent)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bSilent);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_RegisterAsBrowser(IWebBrowser2 *iface,
        VARIANT_BOOL *pbRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_RegisterAsBrowser(IWebBrowser2 *iface,
        VARIANT_BOOL bRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_RegisterAsDropTarget(IWebBrowser2 *iface,
        VARIANT_BOOL *pbRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_RegisterAsDropTarget(IWebBrowser2 *iface,
        VARIANT_BOOL bRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_TheaterMode(IWebBrowser2 *iface, VARIANT_BOOL *pbRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pbRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_TheaterMode(IWebBrowser2 *iface, VARIANT_BOOL bRegister)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, bRegister);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_get_AddressBar(IWebBrowser2 *iface, VARIANT_BOOL *Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);

    TRACE("(%p)->(%p)\n", This, Value);

    *Value = This->address_bar;
    return S_OK;
}

static HRESULT WINAPI WebBrowser_put_AddressBar(IWebBrowser2 *iface, VARIANT_BOOL Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    VARIANTARG arg;
    DISPPARAMS dispparams = {&arg, NULL, 1, 0};

    TRACE("(%p)->(%x)\n", This, Value);

    This->address_bar = Value ? VARIANT_TRUE : VARIANT_FALSE;

    /* In opposition to InternetExplorer, all we should do here is
     * inform the embedder about the address bar change. */

    V_VT(&arg) = VT_BOOL;
    V_BOOL(&arg) = Value;
    call_sink(This->doc_host.cps.wbe2, DISPID_ONADDRESSBAR, &dispparams);

    return S_OK;
}

static HRESULT WINAPI WebBrowser_get_Resizable(IWebBrowser2 *iface, VARIANT_BOOL *Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Value);
    return E_NOTIMPL;
}

static HRESULT WINAPI WebBrowser_put_Resizable(IWebBrowser2 *iface, VARIANT_BOOL Value)
{
    WebBrowser *This = WEBBROWSER_THIS(iface);
    FIXME("(%p)->(%x)\n", This, Value);
    return E_NOTIMPL;
}

#undef WEBBROWSER_THIS

static const IWebBrowser2Vtbl WebBrowser2Vtbl =
{
    WebBrowser_QueryInterface,
    WebBrowser_AddRef,
    WebBrowser_Release,
    WebBrowser_GetTypeInfoCount,
    WebBrowser_GetTypeInfo,
    WebBrowser_GetIDsOfNames,
    WebBrowser_Invoke,
    WebBrowser_GoBack,
    WebBrowser_GoForward,
    WebBrowser_GoHome,
    WebBrowser_GoSearch,
    WebBrowser_Navigate,
    WebBrowser_Refresh,
    WebBrowser_Refresh2,
    WebBrowser_Stop,
    WebBrowser_get_Application,
    WebBrowser_get_Parent,
    WebBrowser_get_Container,
    WebBrowser_get_Document,
    WebBrowser_get_TopLevelContainer,
    WebBrowser_get_Type,
    WebBrowser_get_Left,
    WebBrowser_put_Left,
    WebBrowser_get_Top,
    WebBrowser_put_Top,
    WebBrowser_get_Width,
    WebBrowser_put_Width,
    WebBrowser_get_Height,
    WebBrowser_put_Height,
    WebBrowser_get_LocationName,
    WebBrowser_get_LocationURL,
    WebBrowser_get_Busy,
    WebBrowser_Quit,
    WebBrowser_ClientToWindow,
    WebBrowser_PutProperty,
    WebBrowser_GetProperty,
    WebBrowser_get_Name,
    WebBrowser_get_HWND,
    WebBrowser_get_FullName,
    WebBrowser_get_Path,
    WebBrowser_get_Visible,
    WebBrowser_put_Visible,
    WebBrowser_get_StatusBar,
    WebBrowser_put_StatusBar,
    WebBrowser_get_StatusText,
    WebBrowser_put_StatusText,
    WebBrowser_get_ToolBar,
    WebBrowser_put_ToolBar,
    WebBrowser_get_MenuBar,
    WebBrowser_put_MenuBar,
    WebBrowser_get_FullScreen,
    WebBrowser_put_FullScreen,
    WebBrowser_Navigate2,
    WebBrowser_QueryStatusWB,
    WebBrowser_ExecWB,
    WebBrowser_ShowBrowserBar,
    WebBrowser_get_ReadyState,
    WebBrowser_get_Offline,
    WebBrowser_put_Offline,
    WebBrowser_get_Silent,
    WebBrowser_put_Silent,
    WebBrowser_get_RegisterAsBrowser,
    WebBrowser_put_RegisterAsBrowser,
    WebBrowser_get_RegisterAsDropTarget,
    WebBrowser_put_RegisterAsDropTarget,
    WebBrowser_get_TheaterMode,
    WebBrowser_put_TheaterMode,
    WebBrowser_get_AddressBar,
    WebBrowser_put_AddressBar,
    WebBrowser_get_Resizable,
    WebBrowser_put_Resizable
};

static HRESULT WebBrowser_Create(INT version, IUnknown *pOuter, REFIID riid, void **ppv)
{
    WebBrowser *ret;
    HRESULT hres;

    TRACE("(%p %s %p) version=%d\n", pOuter, debugstr_guid(riid), ppv, version);

    ret = shdocvw_alloc(sizeof(WebBrowser));

    ret->lpWebBrowser2Vtbl = &WebBrowser2Vtbl;
    ret->ref = 0;
    ret->version = version;

    DocHost_Init(&ret->doc_host, (IDispatch*)WEBBROWSER2(ret));

    ret->visible = VARIANT_TRUE;
    ret->menu_bar = VARIANT_TRUE;
    ret->address_bar = VARIANT_TRUE;
    ret->status_bar = VARIANT_TRUE;
    ret->tool_bar = VARIANT_TRUE;

    WebBrowser_OleObject_Init(ret);
    WebBrowser_ViewObject_Init(ret);
    WebBrowser_Persist_Init(ret);
    WebBrowser_ClassInfo_Init(ret);
    WebBrowser_HlinkFrame_Init(ret);

    hres = IWebBrowser_QueryInterface(WEBBROWSER(ret), riid, ppv);
    if(SUCCEEDED(hres)) {
        SHDOCVW_LockModule();
    }else {
        shdocvw_free(ret);
        return hres;
    }

    return hres;
}

HRESULT WebBrowserV1_Create(IUnknown *pOuter, REFIID riid, void **ppv)
{
    return WebBrowser_Create(1, pOuter, riid, ppv);
}

HRESULT WebBrowserV2_Create(IUnknown *pOuter, REFIID riid, void **ppv)
{
    return WebBrowser_Create(2, pOuter, riid, ppv);
}
