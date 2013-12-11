/*
 * IWineD3DIndexBuffer Implementation
 *
 * Copyright 2002-2004 Jason Edmeades
 * Copyright 2003-2004 Raphael Junqueira
 * Copyright 2004 Christian Costa
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
 */

#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);
#define GLINFO_LOCATION ((IWineD3DImpl *)(((IWineD3DDeviceImpl *)This->resource.wineD3DDevice)->wineD3D))->gl_info

/* *******************************************
   IWineD3DIndexBuffer IUnknown parts follow
   ******************************************* */
static HRESULT WINAPI IWineD3DIndexBufferImpl_QueryInterface(IWineD3DIndexBuffer *iface, REFIID riid, LPVOID *ppobj)
{
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;
    TRACE("(%p)->(%s,%p)\n",This,debugstr_guid(riid),ppobj);
    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IWineD3DBase)
        || IsEqualGUID(riid, &IID_IWineD3DResource)
        || IsEqualGUID(riid, &IID_IWineD3DIndexBuffer)){
        IUnknown_AddRef(iface);
        *ppobj = This;
        return S_OK;
    }
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI IWineD3DIndexBufferImpl_AddRef(IWineD3DIndexBuffer *iface) {
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;
    ULONG ref = InterlockedIncrement(&This->resource.ref);
    TRACE("(%p) : AddRef increasing from %ld\n", This, ref - 1);
    return ref;
}

static ULONG WINAPI IWineD3DIndexBufferImpl_Release(IWineD3DIndexBuffer *iface) {
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;
    ULONG ref = InterlockedDecrement(&This->resource.ref);
    TRACE("(%p) : Releasing from %ld\n", This, ref + 1);
    if (ref == 0) {
        IWineD3DResourceImpl_CleanUp((IWineD3DResource *)iface);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* ****************************************************
   IWineD3DIndexBuffer IWineD3DResource parts follow
   **************************************************** */
static HRESULT WINAPI IWineD3DIndexBufferImpl_GetDevice(IWineD3DIndexBuffer *iface, IWineD3DDevice** ppDevice) {
    return IWineD3DResourceImpl_GetDevice((IWineD3DResource *)iface, ppDevice);
}

static HRESULT WINAPI IWineD3DIndexBufferImpl_SetPrivateData(IWineD3DIndexBuffer *iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    return IWineD3DResourceImpl_SetPrivateData((IWineD3DResource *)iface, refguid, pData, SizeOfData, Flags);
}

static HRESULT WINAPI IWineD3DIndexBufferImpl_GetPrivateData(IWineD3DIndexBuffer *iface, REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    return IWineD3DResourceImpl_GetPrivateData((IWineD3DResource *)iface, refguid, pData, pSizeOfData);
}

static HRESULT WINAPI IWineD3DIndexBufferImpl_FreePrivateData(IWineD3DIndexBuffer *iface, REFGUID refguid) {
    return IWineD3DResourceImpl_FreePrivateData((IWineD3DResource *)iface, refguid);
}

static DWORD WINAPI IWineD3DIndexBufferImpl_SetPriority(IWineD3DIndexBuffer *iface, DWORD PriorityNew) {
    return IWineD3DResourceImpl_SetPriority((IWineD3DResource *)iface, PriorityNew);
}

static DWORD WINAPI IWineD3DIndexBufferImpl_GetPriority(IWineD3DIndexBuffer *iface) {
    return IWineD3DResourceImpl_GetPriority((IWineD3DResource *)iface);
}

static void WINAPI IWineD3DIndexBufferImpl_PreLoad(IWineD3DIndexBuffer *iface) {
    return IWineD3DResourceImpl_PreLoad((IWineD3DResource *)iface);
}

static WINED3DRESOURCETYPE WINAPI IWineD3DIndexBufferImpl_GetType(IWineD3DIndexBuffer *iface) {
    return IWineD3DResourceImpl_GetType((IWineD3DResource *)iface);
}

static HRESULT WINAPI IWineD3DIndexBufferImpl_GetParent(IWineD3DIndexBuffer *iface, IUnknown **pParent) {
    return IWineD3DResourceImpl_GetParent((IWineD3DResource *)iface, pParent);
}

/* ******************************************************
   IWineD3DIndexBuffer IWineD3DIndexBuffer parts follow
   ****************************************************** */
static HRESULT WINAPI IWineD3DIndexBufferImpl_Lock(IWineD3DIndexBuffer *iface, UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) {
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;
    TRACE("(%p) : no real locking yet, offset %d, size %d, Flags=%lx\n", This, OffsetToLock, SizeToLock, Flags);
    *ppbData = (BYTE *)This->resource.allocatedMemory + OffsetToLock;
    return WINED3D_OK;
}
static HRESULT WINAPI IWineD3DIndexBufferImpl_Unlock(IWineD3DIndexBuffer *iface) {
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;
    TRACE("(%p) : stub\n", This);
    return WINED3D_OK;
}
static HRESULT WINAPI IWineD3DIndexBufferImpl_GetDesc(IWineD3DIndexBuffer *iface, WINED3DINDEXBUFFER_DESC *pDesc) {
    IWineD3DIndexBufferImpl *This = (IWineD3DIndexBufferImpl *)iface;

    TRACE("(%p)\n", This);
    pDesc->Format = This->resource.format;
    pDesc->Type   = This->resource.resourceType;
    pDesc->Usage  = This->resource.usage;
    pDesc->Pool   = This->resource.pool;
    pDesc->Size   = This->resource.size;
    return WINED3D_OK;
}

const IWineD3DIndexBufferVtbl IWineD3DIndexBuffer_Vtbl =
{
    /* IUnknown */
    IWineD3DIndexBufferImpl_QueryInterface,
    IWineD3DIndexBufferImpl_AddRef,
    IWineD3DIndexBufferImpl_Release,
    /* IWineD3DResource */
    IWineD3DIndexBufferImpl_GetParent,
    IWineD3DIndexBufferImpl_GetDevice,
    IWineD3DIndexBufferImpl_SetPrivateData,
    IWineD3DIndexBufferImpl_GetPrivateData,
    IWineD3DIndexBufferImpl_FreePrivateData,
    IWineD3DIndexBufferImpl_SetPriority,
    IWineD3DIndexBufferImpl_GetPriority,
    IWineD3DIndexBufferImpl_PreLoad,
    IWineD3DIndexBufferImpl_GetType,
    /* IWineD3DIndexBuffer */
    IWineD3DIndexBufferImpl_Lock,
    IWineD3DIndexBufferImpl_Unlock,
    IWineD3DIndexBufferImpl_GetDesc
};
