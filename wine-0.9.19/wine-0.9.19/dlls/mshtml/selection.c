/*
 * Copyright 2006 Jacek Caban for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "ole2.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

typedef struct {
    const IHTMLSelectionObjectVtbl *lpHTMLSelectionObjectVtbl;

    LONG ref;

    nsISelection *nsselection;
} HTMLSelectionObject;

#define HTMLSELOBJ(x)  ((IHTMLSelectionObject*) &(x)->lpHTMLSelectionObjectVtbl)

#define HTMLSELOBJ_THIS(iface) DEFINE_THIS(HTMLSelectionObject, HTMLSelectionObject, iface)

static HRESULT WINAPI HTMLSelectionObject_QueryInterface(IHTMLSelectionObject *iface,
                                                         REFIID riid, void **ppv)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);

    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = HTMLSELOBJ(This);
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = HTMLSELOBJ(This);
    }else if(IsEqualGUID(&IID_IHTMLSelectionObject, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = HTMLSELOBJ(This);
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI HTMLSelectionObject_AddRef(IHTMLSelectionObject *iface)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLSelectionObject_Release(IHTMLSelectionObject *iface)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->nsselection)
            nsISelection_Release(This->nsselection);
        mshtml_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLSelectionObject_GetTypeInfoCount(IHTMLSelectionObject *iface, UINT *pctinfo)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_GetTypeInfo(IHTMLSelectionObject *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)->(%u %lu %p)\n", This, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_GetIDsOfNames(IHTMLSelectionObject *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)->(%s %p %u %lu %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_Invoke(IHTMLSelectionObject *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)->(%ld %s %ld %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_createRange(IHTMLSelectionObject *iface, IDispatch **range)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);

    TRACE("(%p)->(%p)\n", This, range);

    *range = (IDispatch*)HTMLTxtRange_Create(This->nsselection);
    return S_OK;
}

static HRESULT WINAPI HTMLSelectionObject_empty(IHTMLSelectionObject *iface)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_clear(IHTMLSelectionObject *iface)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLSelectionObject_get_type(IHTMLSelectionObject *iface, BSTR *p)
{
    HTMLSelectionObject *This = HTMLSELOBJ_THIS(iface);
    PRInt32 range_cnt = 0;

    static const WCHAR wszNone[] = {'N','o','n','e',0};
    static const WCHAR wszText[] = {'T','e','x','t',0};

    TRACE("(%p)->(%p)\n", This, p);

    if(This->nsselection)
        nsISelection_GetRangeCount(This->nsselection, &range_cnt);

    *p = SysAllocString(range_cnt ? wszText : wszNone); /* FIXME: control */
    return S_OK;
}

#undef HTMLSELOBJ_THIS

static const IHTMLSelectionObjectVtbl HTMLSelectionObjectVtbl = {
    HTMLSelectionObject_QueryInterface,
    HTMLSelectionObject_AddRef,
    HTMLSelectionObject_Release,
    HTMLSelectionObject_GetTypeInfoCount,
    HTMLSelectionObject_GetTypeInfo,
    HTMLSelectionObject_GetIDsOfNames,
    HTMLSelectionObject_Invoke,
    HTMLSelectionObject_createRange,
    HTMLSelectionObject_empty,
    HTMLSelectionObject_clear,
    HTMLSelectionObject_get_type
};

IHTMLSelectionObject *HTMLSelectionObject_Create(nsISelection *nsselection)
{
    HTMLSelectionObject *ret = mshtml_alloc(sizeof(HTMLSelectionObject));

    ret->lpHTMLSelectionObjectVtbl = &HTMLSelectionObjectVtbl;
    ret->ref = 1;
    ret->nsselection = nsselection; /* We shouldn't call AddRef here */

    return HTMLSELOBJ(ret);
}
