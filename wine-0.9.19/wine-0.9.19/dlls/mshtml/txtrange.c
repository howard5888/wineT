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
    const IHTMLTxtRangeVtbl *lpHTMLTxtRangeVtbl;

    LONG ref;

    nsISelection *nsselection;
} HTMLTxtRange;

#define HTMLTXTRANGE(x)  ((IHTMLTxtRange*)  &(x)->lpHTMLTxtRangeVtbl)

#define HTMLTXTRANGE_THIS(iface) DEFINE_THIS(HTMLTxtRange, HTMLTxtRange, iface)

static HRESULT WINAPI HTMLTxtRange_QueryInterface(IHTMLTxtRange *iface, REFIID riid, void **ppv)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);

    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = HTMLTXTRANGE(This);
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = HTMLTXTRANGE(This);
    }else if(IsEqualGUID(&IID_IHTMLTxtRange, riid)) {
        TRACE("(%p)->(IID_IHTMLTxtRange %p)\n", This, ppv);
        *ppv = HTMLTXTRANGE(This);
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI HTMLTxtRange_AddRef(IHTMLTxtRange *iface)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLTxtRange_Release(IHTMLTxtRange *iface)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->nsselection)
            nsISelection_Release(This->nsselection);
        mshtml_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLTxtRange_GetTypeInfoCount(IHTMLTxtRange *iface, UINT *pctinfo)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_GetTypeInfo(IHTMLTxtRange *iface, UINT iTInfo,
                                               LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%u %lu %p)\n", This, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_GetIDsOfNames(IHTMLTxtRange *iface, REFIID riid,
                                                 LPOLESTR *rgszNames, UINT cNames,
                                                 LCID lcid, DISPID *rgDispId)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p %u %lu %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_Invoke(IHTMLTxtRange *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%ld %s %ld %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_get_htmlText(IHTMLTxtRange *iface, BSTR *p)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_put_text(IHTMLTxtRange *iface, BSTR v)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_get_text(IHTMLTxtRange *iface, BSTR *p)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    PRUnichar *nstext = NULL;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!This->nsselection) {
        static const WCHAR empty[] = {0};
        *p = SysAllocString(empty);
        return S_OK;
    }

    nsres = nsISelection_ToString(This->nsselection, &nstext);
    if(NS_FAILED(nsres) || !nstext) {
        ERR("toString failed: %08lx\n", nsres);
        return E_FAIL;
    }

    *p = SysAllocString(nstext);
    nsfree(nstext);
    return S_OK;
}

static HRESULT WINAPI HTMLTxtRange_parentElement(IHTMLTxtRange *iface, IHTMLElement **parent)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, parent);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_duplicate(IHTMLTxtRange *iface, IHTMLTxtRange **Duplicate)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Duplicate);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_inRange(IHTMLTxtRange *iface, IHTMLTxtRange *Range,
        VARIANT_BOOL *InRange)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, Range, InRange);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_isEqual(IHTMLTxtRange *iface, IHTMLTxtRange *Range,
        VARIANT_BOOL *IsEqual)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_scrollIntoView(IHTMLTxtRange *iface, VARIANT_BOOL fStart)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%x)\n", This, fStart);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_collapse(IHTMLTxtRange *iface, VARIANT_BOOL Start)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%x)\n", This, Start);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_expand(IHTMLTxtRange *iface, BSTR Unit, VARIANT_BOOL *Success)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(Unit), Success);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_move(IHTMLTxtRange *iface, BSTR Unit,
        long Count, long *ActualCount)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %ld %p)\n", This, debugstr_w(Unit), Count, ActualCount);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_moveStart(IHTMLTxtRange *iface, BSTR Unit,
        long Count, long *ActualCount)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %ld %p)\n", This, debugstr_w(Unit), Count, ActualCount);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_moveEnd(IHTMLTxtRange *iface, BSTR Unit,
        long Count, long *ActualCount)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %ld %p)\n", This, debugstr_w(Unit), Count, ActualCount);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_select(IHTMLTxtRange *iface)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->()\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_pasteHTML(IHTMLTxtRange *iface, BSTR html)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(html));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_moveToElementText(IHTMLTxtRange *iface, IHTMLElement *element)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, element);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_setEndPoint(IHTMLTxtRange *iface, BSTR how,
        IHTMLTxtRange *SourceRange)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(how), SourceRange);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_compareEndPoints(IHTMLTxtRange *iface, BSTR how,
        IHTMLTxtRange *SourceRange, long *ret)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p %p)\n", This, debugstr_w(how), SourceRange, ret);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_findText(IHTMLTxtRange *iface, BSTR String,
        long count, long Flags, VARIANT_BOOL *Success)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %ld %08lx %p)\n", This, debugstr_w(String), count, Flags, Success);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_moveToPoint(IHTMLTxtRange *iface, long x, long y)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%ld %ld)\n", This, x, y);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_getBookmark(IHTMLTxtRange *iface, BSTR *Bookmark)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, Bookmark);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_moveToBookmark(IHTMLTxtRange *iface, BSTR Bookmark,
        VARIANT_BOOL *Success)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(Bookmark), Success);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandSupported(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandEnabled(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandState(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandIndeterm(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandText(IHTMLTxtRange *iface, BSTR cmdID,
        BSTR *pcmdText)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pcmdText);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_queryCommandValue(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT *pcmdValue)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pcmdValue);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_execCommand(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %x v %p)\n", This, debugstr_w(cmdID), showUI, pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLTxtRange_execCommandShowHelp(IHTMLTxtRange *iface, BSTR cmdID,
        VARIANT_BOOL *pfRet)
{
    HTMLTxtRange *This = HTMLTXTRANGE_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

#undef HTMLTXTRANGE_THIS

static const IHTMLTxtRangeVtbl HTMLTxtRangeVtbl = {
    HTMLTxtRange_QueryInterface,
    HTMLTxtRange_AddRef,
    HTMLTxtRange_Release,
    HTMLTxtRange_GetTypeInfoCount,
    HTMLTxtRange_GetTypeInfo,
    HTMLTxtRange_GetIDsOfNames,
    HTMLTxtRange_Invoke,
    HTMLTxtRange_get_htmlText,
    HTMLTxtRange_put_text,
    HTMLTxtRange_get_text,
    HTMLTxtRange_parentElement,
    HTMLTxtRange_duplicate,
    HTMLTxtRange_inRange,
    HTMLTxtRange_isEqual,
    HTMLTxtRange_scrollIntoView,
    HTMLTxtRange_collapse,
    HTMLTxtRange_expand,
    HTMLTxtRange_move,
    HTMLTxtRange_moveStart,
    HTMLTxtRange_moveEnd,
    HTMLTxtRange_select,
    HTMLTxtRange_pasteHTML,
    HTMLTxtRange_moveToElementText,
    HTMLTxtRange_setEndPoint,
    HTMLTxtRange_compareEndPoints,
    HTMLTxtRange_findText,
    HTMLTxtRange_moveToPoint,
    HTMLTxtRange_getBookmark,
    HTMLTxtRange_moveToBookmark,
    HTMLTxtRange_queryCommandSupported,
    HTMLTxtRange_queryCommandEnabled,
    HTMLTxtRange_queryCommandState,
    HTMLTxtRange_queryCommandIndeterm,
    HTMLTxtRange_queryCommandText,
    HTMLTxtRange_queryCommandValue,
    HTMLTxtRange_execCommand,
    HTMLTxtRange_execCommandShowHelp
};

IHTMLTxtRange *HTMLTxtRange_Create(nsISelection *nsselection)
{
    HTMLTxtRange *ret = mshtml_alloc(sizeof(HTMLTxtRange));

    ret->lpHTMLTxtRangeVtbl = &HTMLTxtRangeVtbl;
    ret->ref = 1;

    if(nsselection)
        nsISelection_AddRef(nsselection);
    ret->nsselection = nsselection;

    return HTMLTXTRANGE(ret);
}
