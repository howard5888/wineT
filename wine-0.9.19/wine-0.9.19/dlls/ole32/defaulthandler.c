/*
 *	OLE 2 default object handler
 *
 *      Copyright 1999  Francis Beaudet
 *      Copyright 2000  Abey George
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
 *
 * NOTES:
 *    The OLE2 default object handler supports a whole whack of
 *    interfaces including:
 *       IOleObject, IDataObject, IPersistStorage, IViewObject2,
 *       IRunnableObject, IOleCache2, IOleCacheControl and much more.
 *
 *    All the implementation details are taken from: Inside OLE
 *    second edition by Kraig Brockschmidt,
 *
 * TODO
 * - This implementation of the default handler does not launch the
 *   server in the DoVerb, Update, GetData, GetDataHere and Run
 *   methods. When it is fixed to do so, all the methods will have
 *   to be  revisited to allow delegating to the running object
 *
 * - All methods in the class that use the class ID should be
 *   aware that it is possible for a class to be treated as
 *   another one and go into emulation mode. Nothing has been
 *   done in this area.
 *
 * - Some functions still return E_NOTIMPL they have to be
 *   implemented. Most of those are related to the running of the
 *   actual server.
 *
 * - All the methods related to notification and advise sinks are
 *   in place but no notifications are sent to the sinks yet.
 */
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "ole2.h"

#include "compobj_private.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

/****************************************************************************
 * DefaultHandler
 *
 */
struct DefaultHandler
{
  const IOleObjectVtbl*      lpVtbl;
  const IUnknownVtbl*        lpvtblIUnknown;
  const IDataObjectVtbl*     lpvtblIDataObject;
  const IRunnableObjectVtbl* lpvtblIRunnableObject;
  const IAdviseSinkVtbl     *lpvtblIAdviseSink;

  /* Reference count of this object */
  LONG ref;

  /* IUnknown implementation of the outer object. */
  IUnknown* outerUnknown;

  /* Class Id that this handler object represents. */
  CLSID clsid;

  /* IUnknown implementation of the datacache. */
  IUnknown* dataCache;

  /* Client site for the embedded object. */
  IOleClientSite* clientSite;

  /*
   * The IOleAdviseHolder maintains the connections
   * on behalf of the default handler.
   */
  IOleAdviseHolder* oleAdviseHolder;

  /*
   * The IDataAdviseHolder maintains the data
   * connections on behalf of the default handler.
   */
  IDataAdviseHolder* dataAdviseHolder;

  /* Name of the container and object contained */
  LPWSTR containerApp;
  LPWSTR containerObj;

  /* IOleObject delegate */
  IOleObject *pOleDelegate;
  /* IPersistStorage delegate */
  IPersistStorage *pPSDelegate;
  /* IDataObject delegate */
  IDataObject *pDataDelegate;

  /* connection cookie for the advise on the delegate OLE object */
  DWORD dwAdvConn;
};

typedef struct DefaultHandler DefaultHandler;

/*
 * Here, I define utility functions to help with the casting of the
 * "This" parameter.
 * There is a version to accommodate all of the VTables implemented
 * by this object.
 */
static inline DefaultHandler *impl_from_IOleObject( IOleObject *iface )
{
    return (DefaultHandler *)((char*)iface - FIELD_OFFSET(DefaultHandler, lpVtbl));
}

static inline DefaultHandler *impl_from_NDIUnknown( IUnknown *iface )
{
    return (DefaultHandler *)((char*)iface - FIELD_OFFSET(DefaultHandler, lpvtblIUnknown));
}

static inline DefaultHandler *impl_from_IDataObject( IDataObject *iface )
{
    return (DefaultHandler *)((char*)iface - FIELD_OFFSET(DefaultHandler, lpvtblIDataObject));
}

static inline DefaultHandler *impl_from_IRunnableObject( IRunnableObject *iface )
{
    return (DefaultHandler *)((char*)iface - FIELD_OFFSET(DefaultHandler, lpvtblIRunnableObject));
}

static inline DefaultHandler *impl_from_IAdviseSink( IAdviseSink *iface )
{
    return (DefaultHandler *)((char*)iface - FIELD_OFFSET(DefaultHandler, lpvtblIAdviseSink));
}

static void DefaultHandler_Destroy(DefaultHandler* This);


/*********************************************************
 * Method implementation for the  non delegating IUnknown
 * part of the DefaultHandler class.
 */

/************************************************************************
 * DefaultHandler_NDIUnknown_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 *
 * This version of QueryInterface will not delegate it's implementation
 * to the outer unknown.
 */
static HRESULT WINAPI DefaultHandler_NDIUnknown_QueryInterface(
            IUnknown*      iface,
            REFIID         riid,
            void**         ppvObject)
{
  DefaultHandler *This = impl_from_NDIUnknown(iface);

  /* Perform a sanity check on the parameters. */
  if (!ppvObject)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(&IID_IUnknown, riid))
    *ppvObject = iface;
  else if (IsEqualIID(&IID_IOleObject, riid))
    *ppvObject = (IOleObject*)&This->lpVtbl;
  else if (IsEqualIID(&IID_IDataObject, riid))
    *ppvObject = (IDataObject*)&This->lpvtblIDataObject;
  else if (IsEqualIID(&IID_IRunnableObject, riid))
    *ppvObject = (IRunnableObject*)&This->lpvtblIRunnableObject;
  else if (IsEqualIID(&IID_IPersist, riid) ||
           IsEqualIID(&IID_IPersistStorage, riid) ||
           IsEqualIID(&IID_IViewObject, riid) ||
           IsEqualIID(&IID_IViewObject2, riid) ||
           IsEqualIID(&IID_IOleCache, riid) ||
           IsEqualIID(&IID_IOleCache2, riid))
  {
    HRESULT hr = IUnknown_QueryInterface(This->dataCache, riid, ppvObject);
    if (FAILED(hr)) FIXME("interface %s not implemented by data cache\n", debugstr_guid(riid));
    return hr;
  }

  /* Check that we obtained an interface. */
  if (*ppvObject == NULL)
  {
    WARN( "() : asking for un supported interface %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
  }

  /*
   * Query Interface always increases the reference count by one when it is
   * successful.
   */
  IUnknown_AddRef((IUnknown*)*ppvObject);

  return S_OK;
}

/************************************************************************
 * DefaultHandler_NDIUnknown_AddRef (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 *
 * This version of QueryInterface will not delegate it's implementation
 * to the outer unknown.
 */
static ULONG WINAPI DefaultHandler_NDIUnknown_AddRef(
            IUnknown*      iface)
{
  DefaultHandler *This = impl_from_NDIUnknown(iface);
  return InterlockedIncrement(&This->ref);
}

/************************************************************************
 * DefaultHandler_NDIUnknown_Release (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 *
 * This version of QueryInterface will not delegate it's implementation
 * to the outer unknown.
 */
static ULONG WINAPI DefaultHandler_NDIUnknown_Release(
            IUnknown*      iface)
{
  DefaultHandler *This = impl_from_NDIUnknown(iface);
  ULONG ref;

  /* Decrease the reference count on this object. */
  ref = InterlockedDecrement(&This->ref);

  if (!ref) DefaultHandler_Destroy(This);

  return ref;
}

/*********************************************************
 * Methods implementation for the IOleObject part of
 * the DefaultHandler class.
 */

/************************************************************************
 * DefaultHandler_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static HRESULT WINAPI DefaultHandler_QueryInterface(
            IOleObject*      iface,
            REFIID           riid,
            void**           ppvObject)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  return IUnknown_QueryInterface(This->outerUnknown, riid, ppvObject);
}

/************************************************************************
 * DefaultHandler_AddRef (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_AddRef(
            IOleObject*        iface)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  return IUnknown_AddRef(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_Release (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_Release(
            IOleObject*        iface)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  return IUnknown_Release(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_SetClientSite (IOleObject)
 *
 * The default handler's implementation of this method only keeps the
 * client site pointer for future reference.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetClientSite(
	    IOleObject*        iface,
	    IOleClientSite*    pClientSite)
{
  DefaultHandler *This = impl_from_IOleObject(iface);
  HRESULT hr = S_OK;

  TRACE("(%p, %p)\n", iface, pClientSite);

  if (This->pOleDelegate)
    hr = IOleObject_SetClientSite(This->pOleDelegate, pClientSite);

  /*
   * Make sure we release the previous client site if there
   * was one.
   */
  if (This->clientSite)
    IOleClientSite_Release(This->clientSite);

  This->clientSite = pClientSite;

  if (This->clientSite)
    IOleClientSite_AddRef(This->clientSite);

  return S_OK;
}

/************************************************************************
 * DefaultHandler_GetClientSite (IOleObject)
 *
 * The default handler's implementation of this method returns the
 * last pointer set in IOleObject_SetClientSite.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetClientSite(
	    IOleObject*        iface,
	    IOleClientSite**   ppClientSite)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  /* Sanity check. */
  if (!ppClientSite)
    return E_POINTER;

  *ppClientSite = This->clientSite;

  if (This->clientSite)
    IOleClientSite_AddRef(This->clientSite);

  return S_OK;
}

/************************************************************************
 * DefaultHandler_SetHostNames (IOleObject)
 *
 * The default handler's implementation of this method just stores
 * the strings and returns S_OK.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetHostNames(
	    IOleObject*        iface,
	    LPCOLESTR          szContainerApp,
	    LPCOLESTR          szContainerObj)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %s, %s)\n",
	iface,
	debugstr_w(szContainerApp),
	debugstr_w(szContainerObj));

  if (This->pOleDelegate)
    IOleObject_SetHostNames(This->pOleDelegate, szContainerApp, szContainerObj);

  /* Be sure to cleanup before re-assinging the strings. */
  HeapFree( GetProcessHeap(), 0, This->containerApp );
  This->containerApp = NULL;
  HeapFree( GetProcessHeap(), 0, This->containerObj );
  This->containerObj = NULL;

  /* Copy the string supplied. */
  if (szContainerApp)
  {
      if ((This->containerApp = HeapAlloc( GetProcessHeap(), 0,
                                           (lstrlenW(szContainerApp) + 1) * sizeof(WCHAR) )))
          strcpyW( This->containerApp, szContainerApp );
  }

  if (szContainerObj)
  {
      if ((This->containerObj = HeapAlloc( GetProcessHeap(), 0,
                                           (lstrlenW(szContainerObj) + 1) * sizeof(WCHAR) )))
          strcpyW( This->containerObj, szContainerObj );
  }
  return S_OK;
}

/* undos the work done by DefaultHandler_Run */
static void WINAPI DefaultHandler_Stop(DefaultHandler *This)
{
  if (!This->pOleDelegate)
    return;

  IOleObject_Unadvise(This->pOleDelegate, This->dwAdvConn);

  /* FIXME: call IOleCache_OnStop */

  if (This->dataAdviseHolder)
    DataAdviseHolder_OnDisconnect(This->dataAdviseHolder);
  if (This->pDataDelegate)
  {
     IDataObject_Release(This->pDataDelegate);
     This->pDataDelegate = NULL;
  }
  if (This->pPSDelegate)
  {
     IPersistStorage_Release(This->pPSDelegate);
     This->pPSDelegate = NULL;
  }
  IOleObject_Release(This->pOleDelegate);
  This->pOleDelegate = NULL;
}

/************************************************************************
 * DefaultHandler_Close (IOleObject)
 *
 * The default handler's implementation of this method is meaningless
 * without a running server so it does nothing.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_Close(
	    IOleObject*        iface,
	    DWORD              dwSaveOption)
{
  DefaultHandler *This = impl_from_IOleObject(iface);
  HRESULT hr;

  TRACE("(%ld)\n", dwSaveOption);

  if (!This->pOleDelegate)
    return S_OK;

  hr = IOleObject_Close(This->pOleDelegate, dwSaveOption);

  DefaultHandler_Stop(This);

  return hr;
}

/************************************************************************
 * DefaultHandler_SetMoniker (IOleObject)
 *
 * The default handler's implementation of this method does nothing.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetMoniker(
	    IOleObject*        iface,
	    DWORD              dwWhichMoniker,
	    IMoniker*          pmk)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %ld, %p)\n",
	iface,
	dwWhichMoniker,
	pmk);

  if (This->pOleDelegate)
    return IOleObject_SetMoniker(This->pOleDelegate, dwWhichMoniker, pmk);

  return S_OK;
}

/************************************************************************
 * DefaultHandler_GetMoniker (IOleObject)
 *
 * Delegate this request to the client site if we have one.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetMoniker(
	    IOleObject*        iface,
	    DWORD              dwAssign,
	    DWORD              dwWhichMoniker,
	    IMoniker**         ppmk)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %ld, %ld, %p)\n",
	iface, dwAssign, dwWhichMoniker, ppmk);

  if (This->pOleDelegate)
    return IOleObject_GetMoniker(This->pOleDelegate, dwAssign, dwWhichMoniker,
                                 ppmk);

  /* FIXME: dwWhichMoniker == OLEWHICHMK_CONTAINER only? */
  if (This->clientSite)
  {
    return IOleClientSite_GetMoniker(This->clientSite,
				     dwAssign,
				     dwWhichMoniker,
				     ppmk);

  }

  return E_FAIL;
}

/************************************************************************
 * DefaultHandler_InitFromData (IOleObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_InitFromData(
	    IOleObject*        iface,
	    IDataObject*       pDataObject,
	    BOOL               fCreation,
	    DWORD              dwReserved)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %p, %d, %ld)\n",
	iface, pDataObject, fCreation, dwReserved);

  if (This->pOleDelegate)
    return IOleObject_InitFromData(This->pOleDelegate, pDataObject, fCreation,
		                   dwReserved);
  return OLE_E_NOTRUNNING;
}

/************************************************************************
 * DefaultHandler_GetClipboardData (IOleObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetClipboardData(
	    IOleObject*        iface,
	    DWORD              dwReserved,
	    IDataObject**      ppDataObject)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %ld, %p)\n",
	iface, dwReserved, ppDataObject);

  if (This->pOleDelegate)
    return IOleObject_GetClipboardData(This->pOleDelegate, dwReserved,
                                       ppDataObject);

  return OLE_E_NOTRUNNING;
}

static HRESULT WINAPI DefaultHandler_DoVerb(
	    IOleObject*        iface,
	    LONG               iVerb,
	    struct tagMSG*     lpmsg,
	    IOleClientSite*    pActiveSite,
	    LONG               lindex,
	    HWND               hwndParent,
	    LPCRECT            lprcPosRect)
{
  DefaultHandler *This = impl_from_IOleObject(iface);
  IRunnableObject *pRunnableObj = (IRunnableObject *)&This->lpvtblIRunnableObject;
  HRESULT hr;

  TRACE("(%ld, %p, %p, %ld, %p, %s)\n", iVerb, lpmsg, pActiveSite, lindex, hwndParent, wine_dbgstr_rect(lprcPosRect));

  hr = IRunnableObject_Run(pRunnableObj, NULL);
  if (FAILED(hr)) return hr;

  return IOleObject_DoVerb(This->pOleDelegate, iVerb, lpmsg, pActiveSite,
                           lindex, hwndParent, lprcPosRect);
}

/************************************************************************
 * DefaultHandler_EnumVerbs (IOleObject)
 *
 * The default handler implementation of this method simply delegates
 * to OleRegEnumVerbs
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_EnumVerbs(
	    IOleObject*        iface,
	    IEnumOLEVERB**     ppEnumOleVerb)
{
  DefaultHandler *This = impl_from_IOleObject(iface);
  HRESULT hr = OLE_S_USEREG;

  TRACE("(%p, %p)\n", iface, ppEnumOleVerb);

  if (This->pOleDelegate)
    hr = IOleObject_EnumVerbs(This->pOleDelegate, ppEnumOleVerb);

  if (hr == OLE_S_USEREG)
    return OleRegEnumVerbs(&This->clsid, ppEnumOleVerb);
  else
    return hr;
}

static HRESULT WINAPI DefaultHandler_Update(
	    IOleObject*        iface)
{
  FIXME(": Stub\n");
  return E_NOTIMPL;
}

/************************************************************************
 * DefaultHandler_IsUpToDate (IOleObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_IsUpToDate(
	    IOleObject*        iface)
{
  TRACE("(%p)\n", iface);

  return OLE_E_NOTRUNNING;
}

/************************************************************************
 * DefaultHandler_GetUserClassID (IOleObject)
 *
 * TODO: Map to a new class ID if emulation is active.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetUserClassID(
	    IOleObject*        iface,
	    CLSID*             pClsid)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %p)\n", iface, pClsid);

  if (This->pOleDelegate)
    return IOleObject_GetUserClassID(This->pOleDelegate, pClsid);

  /* Sanity check. */
  if (!pClsid)
    return E_POINTER;

  memcpy(pClsid, &This->clsid, sizeof(CLSID));

  return S_OK;
}

/************************************************************************
 * DefaultHandler_GetUserType (IOleObject)
 *
 * The default handler implementation of this method simply delegates
 * to OleRegGetUserType
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetUserType(
	    IOleObject*        iface,
	    DWORD              dwFormOfType,
	    LPOLESTR*          pszUserType)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %ld, %p)\n", iface, dwFormOfType, pszUserType);

  return OleRegGetUserType(&This->clsid, dwFormOfType, pszUserType);
}

/************************************************************************
 * DefaultHandler_SetExtent (IOleObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetExtent(
	    IOleObject*        iface,
	    DWORD              dwDrawAspect,
	    SIZEL*             psizel)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %lx, (%ld x %ld))\n", iface,
        dwDrawAspect, psizel->cx, psizel->cy);

  if (This->pOleDelegate)
    IOleObject_SetExtent(This->pOleDelegate, dwDrawAspect, psizel);

  return OLE_E_NOTRUNNING;
}

/************************************************************************
 * DefaultHandler_GetExtent (IOleObject)
 *
 * The default handler's implementation of this method returns uses
 * the cache to locate the aspect and extract the extent from it.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetExtent(
	    IOleObject*        iface,
	    DWORD              dwDrawAspect,
	    SIZEL*             psizel)
{
  DVTARGETDEVICE* targetDevice;
  IViewObject2*   cacheView = NULL;
  HRESULT         hres;

  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %lx, %p)\n", iface, dwDrawAspect, psizel);

  if (This->pOleDelegate)
    return IOleObject_GetExtent(This->pOleDelegate, dwDrawAspect, psizel);

  hres = IUnknown_QueryInterface(This->dataCache, &IID_IViewObject2, (void**)&cacheView);
  if (FAILED(hres))
    return E_UNEXPECTED;

  /*
   * Prepare the call to the cache's GetExtent method.
   *
   * Here we would build a valid DVTARGETDEVICE structure
   * but, since we are calling into the data cache, we
   * know it's implementation and we'll skip this
   * extra work until later.
   */
  targetDevice = NULL;

  hres = IViewObject2_GetExtent(cacheView,
				dwDrawAspect,
				-1,
				targetDevice,
				psizel);

  /*
   * Cleanup
   */
  IViewObject2_Release(cacheView);

  return hres;
}

/************************************************************************
 * DefaultHandler_Advise (IOleObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the OleAdviseHolder.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_Advise(
	    IOleObject*        iface,
	    IAdviseSink*       pAdvSink,
	    DWORD*             pdwConnection)
{
  HRESULT hres = S_OK;
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %p, %p)\n", iface, pAdvSink, pdwConnection);

  /* Make sure we have an advise holder before we start. */
  if (!This->oleAdviseHolder)
    hres = CreateOleAdviseHolder(&This->oleAdviseHolder);

  if (SUCCEEDED(hres))
    hres = IOleAdviseHolder_Advise(This->oleAdviseHolder,
				   pAdvSink,
				   pdwConnection);

  return hres;
}

/************************************************************************
 * DefaultHandler_Unadvise (IOleObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the OleAdviseHolder.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_Unadvise(
	    IOleObject*        iface,
	    DWORD              dwConnection)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %ld)\n", iface, dwConnection);

  /*
   * If we don't have an advise holder yet, it means we don't have
   * a connection.
   */
  if (!This->oleAdviseHolder)
    return OLE_E_NOCONNECTION;

  return IOleAdviseHolder_Unadvise(This->oleAdviseHolder,
				   dwConnection);
}

/************************************************************************
 * DefaultHandler_EnumAdvise (IOleObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the OleAdviseHolder.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_EnumAdvise(
	    IOleObject*        iface,
	    IEnumSTATDATA**    ppenumAdvise)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %p)\n", iface, ppenumAdvise);

  /* Sanity check */
  if (!ppenumAdvise)
    return E_POINTER;

  *ppenumAdvise = NULL;

  if (!This->oleAdviseHolder)
      return S_OK;

  return IOleAdviseHolder_EnumAdvise(This->oleAdviseHolder, ppenumAdvise);
}

/************************************************************************
 * DefaultHandler_GetMiscStatus (IOleObject)
 *
 * The default handler's implementation of this method simply delegates
 * to OleRegGetMiscStatus.
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetMiscStatus(
	    IOleObject*        iface,
	    DWORD              dwAspect,
	    DWORD*             pdwStatus)
{
  HRESULT hres;
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %lx, %p)\n", iface, dwAspect, pdwStatus);

  if (This->pOleDelegate)
    return IOleObject_GetMiscStatus(This->pOleDelegate, dwAspect, pdwStatus);

  hres = OleRegGetMiscStatus(&This->clsid, dwAspect, pdwStatus);

  if (FAILED(hres))
    *pdwStatus = 0;

  return S_OK;
}

/************************************************************************
 * DefaultHandler_SetColorScheme (IOleObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IOleObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetColorScheme(
	    IOleObject*           iface,
	    struct tagLOGPALETTE* pLogpal)
{
  DefaultHandler *This = impl_from_IOleObject(iface);

  TRACE("(%p, %p))\n", iface, pLogpal);

  if (This->pOleDelegate)
    return IOleObject_SetColorScheme(This->pOleDelegate, pLogpal);

  return OLE_E_NOTRUNNING;
}

/*********************************************************
 * Methods implementation for the IDataObject part of
 * the DefaultHandler class.
 */

/************************************************************************
 * DefaultHandler_IDataObject_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static HRESULT WINAPI DefaultHandler_IDataObject_QueryInterface(
            IDataObject*     iface,
           REFIID           riid,
            void**           ppvObject)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  return IUnknown_QueryInterface(This->outerUnknown, riid, ppvObject);
}

/************************************************************************
 * DefaultHandler_IDataObject_AddRef (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_IDataObject_AddRef(
            IDataObject*     iface)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  return IUnknown_AddRef(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_IDataObject_Release (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_IDataObject_Release(
            IDataObject*     iface)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  return IUnknown_Release(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_GetData
 *
 * Get Data from a source dataobject using format pformatetcIn->cfFormat
 * See Windows documentation for more details on GetData.
 * Default handler's implementation of this method delegates to the cache.
 */
static HRESULT WINAPI DefaultHandler_GetData(
	    IDataObject*     iface,
	    LPFORMATETC      pformatetcIn,
	    STGMEDIUM*       pmedium)
{
  IDataObject* cacheDataObject = NULL;
  HRESULT      hres;

  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %p, %p)\n", iface, pformatetcIn, pmedium);

  hres = IUnknown_QueryInterface(This->dataCache,
				 &IID_IDataObject,
				 (void**)&cacheDataObject);

  if (FAILED(hres))
    return E_UNEXPECTED;

  hres = IDataObject_GetData(cacheDataObject,
			     pformatetcIn,
			     pmedium);

  IDataObject_Release(cacheDataObject);

  return hres;
}

static HRESULT WINAPI DefaultHandler_GetDataHere(
	    IDataObject*     iface,
	    LPFORMATETC      pformatetc,
	    STGMEDIUM*       pmedium)
{
  FIXME(": Stub\n");
  return E_NOTIMPL;
}

/************************************************************************
 * DefaultHandler_QueryGetData (IDataObject)
 *
 * The default handler's implementation of this method delegates to
 * the cache.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_QueryGetData(
	    IDataObject*     iface,
	    LPFORMATETC      pformatetc)
{
  IDataObject* cacheDataObject = NULL;
  HRESULT      hres;

  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %p)\n", iface, pformatetc);

  hres = IUnknown_QueryInterface(This->dataCache,
				 &IID_IDataObject,
				 (void**)&cacheDataObject);

  if (FAILED(hres))
    return E_UNEXPECTED;

  hres = IDataObject_QueryGetData(cacheDataObject,
				  pformatetc);

  IDataObject_Release(cacheDataObject);

  return hres;
}

/************************************************************************
 * DefaultHandler_GetCanonicalFormatEtc (IDataObject)
 *
 * This method is meaningless if the server is not running
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetCanonicalFormatEtc(
	    IDataObject*     iface,
	    LPFORMATETC      pformatetcIn,
	    LPFORMATETC      pformatetcOut)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %p, %p)\n", iface, pformatetcIn, pformatetcOut);

  if (!This->pDataDelegate)
    return OLE_E_NOTRUNNING;

  return IDataObject_GetCanonicalFormatEtc(This->pDataDelegate, pformatetcIn, pformatetcOut);
}

/************************************************************************
 * DefaultHandler_SetData (IDataObject)
 *
 * The default handler's implementation of this method delegates to
 * the cache.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetData(
	    IDataObject*     iface,
	    LPFORMATETC      pformatetc,
	    STGMEDIUM*       pmedium,
	    BOOL             fRelease)
{
  DefaultHandler *This = impl_from_IDataObject(iface);
  IDataObject* cacheDataObject = NULL;
  HRESULT      hres;

  TRACE("(%p, %p, %p, %d)\n", iface, pformatetc, pmedium, fRelease);

  hres = IUnknown_QueryInterface(This->dataCache,
				 &IID_IDataObject,
				 (void**)&cacheDataObject);

  if (FAILED(hres))
    return E_UNEXPECTED;

  hres = IDataObject_SetData(cacheDataObject,
			     pformatetc,
			     pmedium,
			     fRelease);

  IDataObject_Release(cacheDataObject);

  return hres;
}

/************************************************************************
 * DefaultHandler_EnumFormatEtc (IDataObject)
 *
 * The default handler's implementation of This method simply delegates
 * to OleRegEnumFormatEtc.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_EnumFormatEtc(
	    IDataObject*     iface,
	    DWORD            dwDirection,
	    IEnumFORMATETC** ppenumFormatEtc)
{
  HRESULT hres;
  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %lx, %p)\n", iface, dwDirection, ppenumFormatEtc);

  hres = OleRegEnumFormatEtc(&This->clsid, dwDirection, ppenumFormatEtc);

  return hres;
}

/************************************************************************
 * DefaultHandler_DAdvise (IDataObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the DataAdviseHolder.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_DAdvise(
	    IDataObject*     iface,
	    FORMATETC*       pformatetc,
	    DWORD            advf,
	    IAdviseSink*     pAdvSink,
	    DWORD*           pdwConnection)
{
  HRESULT hres = S_OK;
  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %p, %ld, %p, %p)\n",
	iface, pformatetc, advf, pAdvSink, pdwConnection);

  /* Make sure we have a data advise holder before we start. */
  if (!This->dataAdviseHolder)
  {
    hres = CreateDataAdviseHolder(&This->dataAdviseHolder);
    if (SUCCEEDED(hres) && This->pDataDelegate)
      DataAdviseHolder_OnConnect(This->dataAdviseHolder, This->pDataDelegate);
  }

  if (SUCCEEDED(hres))
    hres = IDataAdviseHolder_Advise(This->dataAdviseHolder,
				    iface,
				    pformatetc,
				    advf,
				    pAdvSink,
				    pdwConnection);

  return hres;
}

/************************************************************************
 * DefaultHandler_DUnadvise (IDataObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the DataAdviseHolder.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_DUnadvise(
	    IDataObject*     iface,
	    DWORD            dwConnection)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %ld)\n", iface, dwConnection);

  /*
   * If we don't have a data advise holder yet, it means that
   * we don't have any connections..
   */
  if (!This->dataAdviseHolder)
    return OLE_E_NOCONNECTION;

  return IDataAdviseHolder_Unadvise(This->dataAdviseHolder,
				    dwConnection);
}

/************************************************************************
 * DefaultHandler_EnumDAdvise (IDataObject)
 *
 * The default handler's implementation of this method simply
 * delegates to the DataAdviseHolder.
 *
 * See Windows documentation for more details on IDataObject methods.
 */
static HRESULT WINAPI DefaultHandler_EnumDAdvise(
	    IDataObject*     iface,
	    IEnumSTATDATA**  ppenumAdvise)
{
  DefaultHandler *This = impl_from_IDataObject(iface);

  TRACE("(%p, %p)\n", iface, ppenumAdvise);

  /* Sanity check */
  if (!ppenumAdvise)
    return E_POINTER;

  *ppenumAdvise = NULL;

  /* If we have a data advise holder object, delegate. */
  if (This->dataAdviseHolder)
    return IDataAdviseHolder_EnumAdvise(This->dataAdviseHolder,
					ppenumAdvise);

  return S_OK;
}

/*********************************************************
 * Methods implementation for the IRunnableObject part
 * of the DefaultHandler class.
 */

/************************************************************************
 * DefaultHandler_IRunnableObject_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static HRESULT WINAPI DefaultHandler_IRunnableObject_QueryInterface(
            IRunnableObject*     iface,
            REFIID               riid,
            void**               ppvObject)
{
  DefaultHandler *This = impl_from_IRunnableObject(iface);

  return IUnknown_QueryInterface(This->outerUnknown, riid, ppvObject);
}

/************************************************************************
 * DefaultHandler_IRunnableObject_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_IRunnableObject_AddRef(
            IRunnableObject*     iface)
{
  DefaultHandler *This = impl_from_IRunnableObject(iface);

  return IUnknown_AddRef(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_IRunnableObject_QueryInterface (IUnknown)
 *
 * See Windows documentation for more details on IUnknown methods.
 */
static ULONG WINAPI DefaultHandler_IRunnableObject_Release(
            IRunnableObject*     iface)
{
  DefaultHandler *This = impl_from_IRunnableObject(iface);

  return IUnknown_Release(This->outerUnknown);
}

/************************************************************************
 * DefaultHandler_GetRunningClass (IRunnableObject)
 *
 * See Windows documentation for more details on IRunnableObject methods.
 */
static HRESULT WINAPI DefaultHandler_GetRunningClass(
            IRunnableObject*     iface,
	    LPCLSID              lpClsid)
{
  FIXME("()\n");
  return S_OK;
}

static HRESULT WINAPI DefaultHandler_Run(
            IRunnableObject*     iface,
	    IBindCtx*            pbc)
{
  DefaultHandler *This = impl_from_IRunnableObject(iface);
  HRESULT hr;

  FIXME("(%p): semi-stub\n", pbc);

  /* already running? if so nothing to do */
  if (This->pOleDelegate)
    return S_OK;

  hr = CoCreateInstance(&This->clsid, NULL, CLSCTX_LOCAL_SERVER,
                        &IID_IOleObject, (void **)&This->pOleDelegate);
  if (FAILED(hr))
    return hr;

  hr = IOleObject_Advise(This->pOleDelegate,
                         (IAdviseSink *)&This->lpvtblIAdviseSink,
                         &This->dwAdvConn);

  if (SUCCEEDED(hr) && This->clientSite)
    hr = IOleObject_SetClientSite(This->pOleDelegate, This->clientSite);

  if (SUCCEEDED(hr))
  {
    IOleObject_QueryInterface(This->pOleDelegate, &IID_IPersistStorage,
                              (void **)&This->pPSDelegate);
    if (This->pPSDelegate)
      hr = IPersistStorage_InitNew(This->pPSDelegate, NULL);
  }

  if (SUCCEEDED(hr) && This->containerApp)
    hr = IOleObject_SetHostNames(This->pOleDelegate, This->containerApp,
                                 This->containerObj);

  /* FIXME: do more stuff here:
   * - IOleObject_GetMiscStatus
   * - IOleObject_GetMoniker
   * - IOleCache_OnRun
   */

  if (SUCCEEDED(hr))
    hr = IOleObject_QueryInterface(This->pOleDelegate, &IID_IDataObject,
                                   (void **)&This->pDataDelegate);

  if (SUCCEEDED(hr) && This->dataAdviseHolder)
    hr = DataAdviseHolder_OnConnect(This->dataAdviseHolder, This->pDataDelegate);

  if (FAILED(hr))
    DefaultHandler_Stop(This);

  return hr;
}

/************************************************************************
 * DefaultHandler_IsRunning (IRunnableObject)
 *
 * See Windows documentation for more details on IRunnableObject methods.
 */
static BOOL    WINAPI DefaultHandler_IsRunning(
            IRunnableObject*     iface)
{
  DefaultHandler *This = impl_from_IRunnableObject(iface);

  TRACE("()\n");

  if (This->pOleDelegate)
    return TRUE;
  else
    return FALSE;
}

/************************************************************************
 * DefaultHandler_LockRunning (IRunnableObject)
 *
 * See Windows documentation for more details on IRunnableObject methods.
 */
static HRESULT WINAPI DefaultHandler_LockRunning(
            IRunnableObject*     iface,
	    BOOL                 fLock,
	    BOOL                 fLastUnlockCloses)
{
  FIXME("()\n");
  return S_OK;
}

/************************************************************************
 * DefaultHandler_SetContainedObject (IRunnableObject)
 *
 * See Windows documentation for more details on IRunnableObject methods.
 */
static HRESULT WINAPI DefaultHandler_SetContainedObject(
            IRunnableObject*     iface,
	    BOOL                 fContained)
{
  FIXME("()\n");
  return S_OK;
}

static HRESULT WINAPI DefaultHandler_IAdviseSink_QueryInterface(
    IAdviseSink *iface,
    REFIID riid,
    void **ppvObject)
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAdviseSink))
    {
        *ppvObject = iface;
        IAdviseSink_AddRef(iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG WINAPI DefaultHandler_IAdviseSink_AddRef(
    IAdviseSink *iface)
{
    DefaultHandler *This = impl_from_IAdviseSink(iface);

    return IUnknown_AddRef((IUnknown *)&This->lpvtblIUnknown);
}

static ULONG WINAPI DefaultHandler_IAdviseSink_Release(
            IAdviseSink *iface)
{
    DefaultHandler *This = impl_from_IAdviseSink(iface);

    return IUnknown_Release((IUnknown *)&This->lpvtblIUnknown);
}

static void WINAPI DefaultHandler_IAdviseSink_OnDataChange(
    IAdviseSink *iface,
    FORMATETC *pFormatetc,
    STGMEDIUM *pStgmed)
{
    FIXME(": stub\n");
}

static void WINAPI DefaultHandler_IAdviseSink_OnViewChange(
    IAdviseSink *iface,
    DWORD dwAspect,
    LONG lindex)
{
    FIXME(": stub\n");
}

static void WINAPI DefaultHandler_IAdviseSink_OnRename(
    IAdviseSink *iface,
    IMoniker *pmk)
{
    DefaultHandler *This = impl_from_IAdviseSink(iface);

    TRACE("(%p)\n", pmk);

    if (This->oleAdviseHolder)
        IOleAdviseHolder_SendOnRename(This->oleAdviseHolder, pmk);
}

static void WINAPI DefaultHandler_IAdviseSink_OnSave(
    IAdviseSink *iface)
{
    DefaultHandler *This = impl_from_IAdviseSink(iface);

    TRACE("()\n");

    if (This->oleAdviseHolder)
        IOleAdviseHolder_SendOnSave(This->oleAdviseHolder);
}

static void WINAPI DefaultHandler_IAdviseSink_OnClose(
    IAdviseSink *iface)
{
    DefaultHandler *This = impl_from_IAdviseSink(iface);
    
    TRACE("()\n");

    if (This->oleAdviseHolder)
        IOleAdviseHolder_SendOnClose(This->oleAdviseHolder);

    DefaultHandler_Stop(This);
}

/*
 * Virtual function tables for the DefaultHandler class.
 */
static const IOleObjectVtbl DefaultHandler_IOleObject_VTable =
{
  DefaultHandler_QueryInterface,
  DefaultHandler_AddRef,
  DefaultHandler_Release,
  DefaultHandler_SetClientSite,
  DefaultHandler_GetClientSite,
  DefaultHandler_SetHostNames,
  DefaultHandler_Close,
  DefaultHandler_SetMoniker,
  DefaultHandler_GetMoniker,
  DefaultHandler_InitFromData,
  DefaultHandler_GetClipboardData,
  DefaultHandler_DoVerb,
  DefaultHandler_EnumVerbs,
  DefaultHandler_Update,
  DefaultHandler_IsUpToDate,
  DefaultHandler_GetUserClassID,
  DefaultHandler_GetUserType,
  DefaultHandler_SetExtent,
  DefaultHandler_GetExtent,
  DefaultHandler_Advise,
  DefaultHandler_Unadvise,
  DefaultHandler_EnumAdvise,
  DefaultHandler_GetMiscStatus,
  DefaultHandler_SetColorScheme
};

static const IUnknownVtbl DefaultHandler_NDIUnknown_VTable =
{
  DefaultHandler_NDIUnknown_QueryInterface,
  DefaultHandler_NDIUnknown_AddRef,
  DefaultHandler_NDIUnknown_Release,
};

static const IDataObjectVtbl DefaultHandler_IDataObject_VTable =
{
  DefaultHandler_IDataObject_QueryInterface,
  DefaultHandler_IDataObject_AddRef,
  DefaultHandler_IDataObject_Release,
  DefaultHandler_GetData,
  DefaultHandler_GetDataHere,
  DefaultHandler_QueryGetData,
  DefaultHandler_GetCanonicalFormatEtc,
  DefaultHandler_SetData,
  DefaultHandler_EnumFormatEtc,
  DefaultHandler_DAdvise,
  DefaultHandler_DUnadvise,
  DefaultHandler_EnumDAdvise
};

static const IRunnableObjectVtbl DefaultHandler_IRunnableObject_VTable =
{
  DefaultHandler_IRunnableObject_QueryInterface,
  DefaultHandler_IRunnableObject_AddRef,
  DefaultHandler_IRunnableObject_Release,
  DefaultHandler_GetRunningClass,
  DefaultHandler_Run,
  DefaultHandler_IsRunning,
  DefaultHandler_LockRunning,
  DefaultHandler_SetContainedObject
};

static const IAdviseSinkVtbl DefaultHandler_IAdviseSink_VTable =
{
  DefaultHandler_IAdviseSink_QueryInterface,
  DefaultHandler_IAdviseSink_AddRef,
  DefaultHandler_IAdviseSink_Release,
  DefaultHandler_IAdviseSink_OnDataChange,
  DefaultHandler_IAdviseSink_OnViewChange,
  DefaultHandler_IAdviseSink_OnRename,
  DefaultHandler_IAdviseSink_OnSave,
  DefaultHandler_IAdviseSink_OnClose
};

/*********************************************************
 * Methods implementation for the DefaultHandler class.
 */
static DefaultHandler* DefaultHandler_Construct(
  REFCLSID  clsid,
  LPUNKNOWN pUnkOuter)
{
  DefaultHandler* This = NULL;

  /*
   * Allocate space for the object.
   */
  This = HeapAlloc(GetProcessHeap(), 0, sizeof(DefaultHandler));

  if (!This)
    return This;

  This->lpVtbl = &DefaultHandler_IOleObject_VTable;
  This->lpvtblIUnknown = &DefaultHandler_NDIUnknown_VTable;
  This->lpvtblIDataObject = &DefaultHandler_IDataObject_VTable;
  This->lpvtblIRunnableObject = &DefaultHandler_IRunnableObject_VTable;
  This->lpvtblIAdviseSink = &DefaultHandler_IAdviseSink_VTable;

  /*
   * Start with one reference count. The caller of this function
   * must release the interface pointer when it is done.
   */
  This->ref = 1;

  /*
   * Initialize the outer unknown
   * We don't keep a reference on the outer unknown since, the way
   * aggregation works, our lifetime is at least as large as it's
   * lifetime.
   */
  if (!pUnkOuter)
    pUnkOuter = (IUnknown*)&This->lpvtblIUnknown;

  This->outerUnknown = pUnkOuter;

  /*
   * Create a datacache object.
   * We aggregate with the datacache. Make sure we pass our outer
   * unknown as the datacache's outer unknown.
   */
  CreateDataCache(This->outerUnknown,
		  clsid,
		  &IID_IUnknown,
		  (void**)&This->dataCache);

  /*
   * Initialize the other data members of the class.
   */
  memcpy(&This->clsid, clsid, sizeof(CLSID));
  This->clientSite = NULL;
  This->oleAdviseHolder = NULL;
  This->dataAdviseHolder = NULL;
  This->containerApp = NULL;
  This->containerObj = NULL;
  This->pOleDelegate = NULL;
  This->pPSDelegate = NULL;
  This->pDataDelegate = NULL;

  This->dwAdvConn = 0;

  return This;
}

static void DefaultHandler_Destroy(
  DefaultHandler* This)
{
  /* release delegates */
  DefaultHandler_Stop(This);

  /* Free the strings idenfitying the object */
  HeapFree( GetProcessHeap(), 0, This->containerApp );
  This->containerApp = NULL;
  HeapFree( GetProcessHeap(), 0, This->containerObj );
  This->containerObj = NULL;

  /* Release our reference to the data cache. */
  if (This->dataCache)
  {
    IUnknown_Release(This->dataCache);
    This->dataCache = NULL;
  }

  /* Same thing for the client site. */
  if (This->clientSite)
  {
    IOleClientSite_Release(This->clientSite);
    This->clientSite = NULL;
  }

  /* And the advise holder. */
  if (This->oleAdviseHolder)
  {
    IOleAdviseHolder_Release(This->oleAdviseHolder);
    This->oleAdviseHolder = NULL;
  }

  /* And the data advise holder. */
  if (This->dataAdviseHolder)
  {
    IDataAdviseHolder_Release(This->dataAdviseHolder);
    This->dataAdviseHolder = NULL;
  }

  /* Free the actual default handler structure. */
  HeapFree(GetProcessHeap(), 0, This);
}

/******************************************************************************
 * OleCreateDefaultHandler [OLE32.@]
 */
HRESULT WINAPI OleCreateDefaultHandler(
  REFCLSID  clsid,
  LPUNKNOWN pUnkOuter,
  REFIID    riid,
  LPVOID*   ppvObj)
{
  DefaultHandler* newHandler = NULL;
  HRESULT         hr         = S_OK;

  TRACE("(%s, %p, %s, %p)\n", debugstr_guid(clsid), pUnkOuter, debugstr_guid(riid), ppvObj);

  /*
   * Sanity check
   */
  if (!ppvObj)
    return E_POINTER;

  *ppvObj = NULL;

  /*
   * If This handler is constructed for aggregation, make sure
   * the caller is requesting the IUnknown interface.
   * This is necessary because it's the only time the non-delegating
   * IUnknown pointer can be returned to the outside.
   */
  if (pUnkOuter && !IsEqualIID(&IID_IUnknown, riid))
    return CLASS_E_NOAGGREGATION;

  /*
   * Try to construct a new instance of the class.
   */
  newHandler = DefaultHandler_Construct(clsid, pUnkOuter);

  if (!newHandler)
    return E_OUTOFMEMORY;

  /*
   * Make sure it supports the interface required by the caller.
   */
  hr = IUnknown_QueryInterface((IUnknown*)&newHandler->lpvtblIUnknown, riid, ppvObj);

  /*
   * Release the reference obtained in the constructor. If
   * the QueryInterface was unsuccessful, it will free the class.
   */
  IUnknown_Release((IUnknown*)&newHandler->lpvtblIUnknown);

  return hr;
}
