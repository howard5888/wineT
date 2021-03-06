/*
 * Clipboard unit tests
 *
 * Copyright 2006 Kevin Koltzau
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

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"

#include "wine/test.h"

#define InitFormatEtc(fe, cf, med) \
        {\
        (fe).cfFormat=cf;\
        (fe).dwAspect=DVASPECT_CONTENT;\
        (fe).ptd=NULL;\
        (fe).tymed=med;\
        (fe).lindex=-1;\
        };

typedef struct DataObjectImpl {
    const IDataObjectVtbl *lpVtbl;
    LONG ref;

    FORMATETC *fmtetc;
    UINT fmtetc_cnt;

    HANDLE text;
} DataObjectImpl;

typedef struct EnumFormatImpl {
    const IEnumFORMATETCVtbl *lpVtbl;
    LONG ref;

    FORMATETC *fmtetc;
    UINT fmtetc_cnt;

    UINT cur;
} EnumFormatImpl;

static HRESULT EnumFormatImpl_Create(FORMATETC *fmtetc, UINT size, LPENUMFORMATETC *lplpformatetc);

static HRESULT WINAPI EnumFormatImpl_QueryInterface(IEnumFORMATETC *iface, REFIID riid, LPVOID *ppvObj)
{
    EnumFormatImpl *This = (EnumFormatImpl*)iface;

    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IEnumFORMATETC)) {
        IEnumFORMATETC_AddRef(iface);
        *ppvObj = (LPVOID)This;
        return S_OK;
    }
    *ppvObj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI EnumFormatImpl_AddRef(IEnumFORMATETC *iface)
{
    EnumFormatImpl *This = (EnumFormatImpl*)iface;
    LONG ref = InterlockedIncrement(&This->ref);
    return ref;
}

static ULONG WINAPI EnumFormatImpl_Release(IEnumFORMATETC *iface)
{
    EnumFormatImpl *This = (EnumFormatImpl*)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    if(!ref) {
        HeapFree(GetProcessHeap(), 0, This->fmtetc);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI EnumFormatImpl_Next(IEnumFORMATETC *iface, ULONG celt,
                                          FORMATETC *rgelt, ULONG *pceltFetched)
{
    EnumFormatImpl *This = (EnumFormatImpl*)iface;
    ULONG count = 0;

    if(!rgelt)
        return E_INVALIDARG;

    count = min(celt, This->fmtetc_cnt-This->cur);
    if(count > 0) {
        memcpy(rgelt, This->fmtetc+This->cur, count*sizeof(FORMATETC));
        This->cur += count;
    }
    if(pceltFetched)
        *pceltFetched = count;
    return count == celt ? S_OK : S_FALSE;
}

static HRESULT WINAPI EnumFormatImpl_Skip(IEnumFORMATETC *iface, ULONG celt)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI EnumFormatImpl_Reset(IEnumFORMATETC *iface)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI EnumFormatImpl_Clone(IEnumFORMATETC *iface, IEnumFORMATETC **ppenum)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const IEnumFORMATETCVtbl VT_EnumFormatImpl = {
    EnumFormatImpl_QueryInterface,
    EnumFormatImpl_AddRef,
    EnumFormatImpl_Release,
    EnumFormatImpl_Next,
    EnumFormatImpl_Skip,
    EnumFormatImpl_Reset,
    EnumFormatImpl_Clone
};

static HRESULT EnumFormatImpl_Create(FORMATETC *fmtetc, UINT fmtetc_cnt, IEnumFORMATETC **lplpformatetc)
{
    EnumFormatImpl *ret;

    ret = HeapAlloc(GetProcessHeap(), 0, sizeof(EnumFormatImpl));
    ret->lpVtbl = &VT_EnumFormatImpl;
    ret->ref = 1;
    ret->cur = 0;
    ret->fmtetc_cnt = fmtetc_cnt;
    ret->fmtetc = HeapAlloc(GetProcessHeap(), 0, fmtetc_cnt*sizeof(FORMATETC));
    memcpy(ret->fmtetc, fmtetc, fmtetc_cnt*sizeof(FORMATETC));
    *lplpformatetc = (LPENUMFORMATETC)ret;
    return S_OK;
}

static HRESULT WINAPI DataObjectImpl_QueryInterface(IDataObject *iface, REFIID riid, LPVOID *ppvObj)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;

    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDataObject)) {
        IDataObject_AddRef(iface);
        *ppvObj = (LPVOID)This;
        return S_OK;
    }
    *ppvObj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI DataObjectImpl_AddRef(IDataObject* iface)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;
    ULONG ref = InterlockedIncrement(&This->ref);
    return ref;
}

static ULONG WINAPI DataObjectImpl_Release(IDataObject* iface)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    if(!ref) {
        if(This->text) GlobalFree(This->text);
        if(This->fmtetc) GlobalFree(This->fmtetc);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI DataObjectImpl_GetData(IDataObject* iface, FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;

    if(pformatetc->lindex != -1)
        return DV_E_LINDEX;

    if(!(pformatetc->tymed & TYMED_HGLOBAL))
        return DV_E_TYMED;

    if(This->text && pformatetc->cfFormat == CF_TEXT)
        pmedium->u.hGlobal = This->text;
    else
        return DV_E_FORMATETC;

    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->pUnkForRelease = (LPUNKNOWN)iface;
    IUnknown_AddRef(pmedium->pUnkForRelease);
    return S_OK;
}

static HRESULT WINAPI DataObjectImpl_GetDataHere(IDataObject* iface, FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObjectImpl_QueryGetData(IDataObject* iface, FORMATETC *pformatetc)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;
    UINT i;
    BOOL foundFormat = FALSE;

    if(pformatetc->lindex != -1)
        return DV_E_LINDEX;

    for(i=0; i<This->fmtetc_cnt; i++) {
        if(This->fmtetc[i].cfFormat == pformatetc->cfFormat) {
            foundFormat = TRUE;
            if(This->fmtetc[i].tymed == pformatetc->tymed)
                return S_OK;
        }
    }
    return foundFormat?DV_E_FORMATETC:DV_E_TYMED;
}

static HRESULT WINAPI DataObjectImpl_GetCanonicalFormatEtc(IDataObject* iface, FORMATETC *pformatectIn,
                                                           FORMATETC *pformatetcOut)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObjectImpl_SetData(IDataObject* iface, FORMATETC *pformatetc,
                                             STGMEDIUM *pmedium, BOOL fRelease)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObjectImpl_EnumFormatEtc(IDataObject* iface, DWORD dwDirection,
                                                   IEnumFORMATETC **ppenumFormatEtc)
{
    DataObjectImpl *This = (DataObjectImpl*)iface;

    if(dwDirection != DATADIR_GET) {
        ok(0, "unexpected direction %ld\n", dwDirection);
        return E_NOTIMPL;
    }
    return EnumFormatImpl_Create(This->fmtetc, This->fmtetc_cnt, ppenumFormatEtc);
}

static HRESULT WINAPI DataObjectImpl_DAdvise(IDataObject* iface, FORMATETC *pformatetc, DWORD advf,
                                             IAdviseSink *pAdvSink, DWORD *pdwConnection)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObjectImpl_DUnadvise(IDataObject* iface, DWORD dwConnection)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI DataObjectImpl_EnumDAdvise(IDataObject* iface, IEnumSTATDATA **ppenumAdvise)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const IDataObjectVtbl VT_DataObjectImpl =
{
    DataObjectImpl_QueryInterface,
    DataObjectImpl_AddRef,
    DataObjectImpl_Release,
    DataObjectImpl_GetData,
    DataObjectImpl_GetDataHere,
    DataObjectImpl_QueryGetData,
    DataObjectImpl_GetCanonicalFormatEtc,
    DataObjectImpl_SetData,
    DataObjectImpl_EnumFormatEtc,
    DataObjectImpl_DAdvise,
    DataObjectImpl_DUnadvise,
    DataObjectImpl_EnumDAdvise
};

static HRESULT DataObjectImpl_CreateText(LPCSTR text, LPDATAOBJECT *lplpdataobj)
{
    DataObjectImpl *obj;

    obj = HeapAlloc(GetProcessHeap(), 0, sizeof(DataObjectImpl));
    obj->lpVtbl = &VT_DataObjectImpl;
    obj->ref = 1;
    obj->text = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    strcpy(GlobalLock(obj->text), text);
    GlobalUnlock(obj->text);

    obj->fmtetc_cnt = 1;
    obj->fmtetc = HeapAlloc(GetProcessHeap(), 0, obj->fmtetc_cnt*sizeof(FORMATETC));
    InitFormatEtc(obj->fmtetc[0], CF_TEXT, TYMED_HGLOBAL);

    *lplpdataobj = (LPDATAOBJECT)obj;
    return S_OK;
}

static void test_set_clipboard(void)
{
    HRESULT hr;
    ULONG ref;
    LPDATAOBJECT data1, data2;
    hr = DataObjectImpl_CreateText("data1", &data1);
    ok(SUCCEEDED(hr), "Failed to create data1 object: %ld\n", hr);
    if(FAILED(hr))
        return;
    hr = DataObjectImpl_CreateText("data2", &data2);
    ok(SUCCEEDED(hr), "Failed to create data2 object: %ld\n", hr);
    if(FAILED(hr))
        return;

    ok(OleSetClipboard(data1) == S_OK, "failed to set clipboard to data1\n");
    ok(OleIsCurrentClipboard(data1) == S_OK, "expected current clipboard to be data1\n");
    ok(OleIsCurrentClipboard(data2) == S_FALSE, "did not expect current clipboard to be data2\n");

    ok(OleSetClipboard(data2) == S_OK, "failed to set clipboard to data2\n");
    ok(OleIsCurrentClipboard(data1) == S_FALSE, "did not expect current clipboard to be data1\n");
    ok(OleIsCurrentClipboard(data2) == S_OK, "expected current clipboard to be data2\n");

    ok(OleFlushClipboard() == S_OK, "failed to flush clipboard\n");
    ok(OleIsCurrentClipboard(data1) == S_FALSE, "did not expect current clipboard to be data1\n");
    ok(OleIsCurrentClipboard(data2) == S_FALSE, "did not expect current clipboard to be data2\n");

    ok(OleSetClipboard(NULL) == S_OK, "failed to clear clipboard\n");

    ref = IDataObject_Release(data1);
    ok(ref == 0, "expected data1 ref=0, got %ld\n", ref);
    ref = IDataObject_Release(data2);
    ok(ref == 0, "expected data2 ref=0, got %ld\n", ref);
}


START_TEST(clipboard)
{
    test_set_clipboard();
}
