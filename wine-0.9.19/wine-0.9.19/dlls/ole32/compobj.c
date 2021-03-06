/*
 *	COMPOBJ library
 *
 *	Copyright 1995	Martin von Loewis
 *	Copyright 1998	Justin Bradford
 *      Copyright 1999  Francis Beaudet
 *      Copyright 1999  Sylvain St-Germain
 *      Copyright 2002  Marcus Meissner
 *      Copyright 2004  Mike Hearn
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
 * Note
 * 1. COINIT_MULTITHREADED is 0; it is the lack of COINIT_APARTMENTTHREADED
 *    Therefore do not test against COINIT_MULTITHREADED
 *
 * TODO list:           (items bunched together depend on each other)
 *
 *   - Implement the service control manager (in rpcss) to keep track
 *     of registered class objects: ISCM::ServerRegisterClsid et al
 *   - Implement the OXID resolver so we don't need magic endpoint names for
 *     clients and servers to meet up
 *
 *   - Call IMessageFilter functions.
 *
 *   - Make all ole interface marshaling use NDR to be wire compatible with
 *     native DCOM
 *   - Use & interpret ORPCTHIS & ORPCTHAT.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "winuser.h"
#include "objbase.h"
#include "ole2.h"
#include "ole2ver.h"

#include "compobj_private.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

HINSTANCE OLE32_hInstance = 0; /* FIXME: make static ... */

#define ARRAYSIZE(array) (sizeof(array)/sizeof((array)[0]))

/****************************************************************************
 * This section defines variables internal to the COM module.
 *
 * TODO: Most of these things will have to be made thread-safe.
 */

static HRESULT COM_GetRegisteredClassObject(REFCLSID rclsid, DWORD dwClsContext, LPUNKNOWN*  ppUnk);
static void COM_RevokeAllClasses(void);

static APARTMENT *MTA; /* protected by csApartment */
static struct list apts = LIST_INIT( apts ); /* protected by csApartment */

static CRITICAL_SECTION csApartment;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &csApartment,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": csApartment") }
};
static CRITICAL_SECTION csApartment = { &critsect_debug, -1, 0, 0, 0, 0 };

struct registered_psclsid
{
    struct list entry;
    IID iid;
    CLSID clsid;
};

/*
 * This lock count counts the number of times CoInitialize is called. It is
 * decreased every time CoUninitialize is called. When it hits 0, the COM
 * libraries are freed
 */
static LONG s_COMLockCount = 0;

/*
 * This linked list contains the list of registered class objects. These
 * are mostly used to register the factories for out-of-proc servers of OLE
 * objects.
 *
 * TODO: Make this data structure aware of inter-process communication. This
 *       means that parts of this will be exported to the Wine Server.
 */
typedef struct tagRegisteredClass
{
  CLSID     classIdentifier;
  LPUNKNOWN classObject;
  DWORD     runContext;
  DWORD     connectFlags;
  DWORD     dwCookie;
  LPSTREAM  pMarshaledData; /* FIXME: only really need to store OXID and IPID */
  struct tagRegisteredClass* nextClass;
} RegisteredClass;

static RegisteredClass* firstRegisteredClass = NULL;

static CRITICAL_SECTION csRegisteredClassList;
static CRITICAL_SECTION_DEBUG class_cs_debug =
{
    0, 0, &csRegisteredClassList,
    { &class_cs_debug.ProcessLocksList, &class_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": csRegisteredClassList") }
};
static CRITICAL_SECTION csRegisteredClassList = { &class_cs_debug, -1, 0, 0, 0, 0 };

/*****************************************************************************
 * This section contains OpenDllList definitions
 *
 * The OpenDllList contains only handles of dll loaded by CoGetClassObject or
 * other functions that do LoadLibrary _without_ giving back a HMODULE.
 * Without this list these handles would never be freed.
 *
 * FIXME: a DLL that says OK when asked for unloading is unloaded in the
 * next unload-call but not before 600 sec.
 */

typedef struct tagOpenDll {
  HINSTANCE hLibrary;
  struct tagOpenDll *next;
} OpenDll;

static OpenDll *openDllList = NULL; /* linked list of open dlls */

static CRITICAL_SECTION csOpenDllList;
static CRITICAL_SECTION_DEBUG dll_cs_debug =
{
    0, 0, &csOpenDllList,
    { &dll_cs_debug.ProcessLocksList, &dll_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": csOpenDllList") }
};
static CRITICAL_SECTION csOpenDllList = { &dll_cs_debug, -1, 0, 0, 0, 0 };

static const WCHAR wszAptWinClass[] = {'O','l','e','M','a','i','n','T','h','r','e','a','d','W','n','d','C','l','a','s','s',' ',
                                       '0','x','#','#','#','#','#','#','#','#',' ',0};
static LRESULT CALLBACK apartment_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void COMPOBJ_DLLList_Add(HANDLE hLibrary);
static void COMPOBJ_DllList_FreeUnused(int Timeout);

static void COMPOBJ_InitProcess( void )
{
    WNDCLASSW wclass;

    /* Dispatching to the correct thread in an apartment is done through
     * window messages rather than RPC transports. When an interface is
     * marshalled into another apartment in the same process, a window of the
     * following class is created. The *caller* of CoMarshalInterface (ie the
     * application) is responsible for pumping the message loop in that thread.
     * The WM_USER messages which point to the RPCs are then dispatched to
     * COM_AptWndProc by the user's code from the apartment in which the interface
     * was unmarshalled.
     */
    memset(&wclass, 0, sizeof(wclass));
    wclass.lpfnWndProc = apartment_wndproc;
    wclass.hInstance = OLE32_hInstance;
    wclass.lpszClassName = wszAptWinClass;
    RegisterClassW(&wclass);
}

static void COMPOBJ_UninitProcess( void )
{
    UnregisterClassW(wszAptWinClass, OLE32_hInstance);
}

static void COM_TlsDestroy(void)
{
    struct oletls *info = NtCurrentTeb()->ReservedForOle;
    if (info)
    {
        if (info->apt) apartment_release(info->apt);
        if (info->errorinfo) IErrorInfo_Release(info->errorinfo);
        if (info->state) IUnknown_Release(info->state);
        HeapFree(GetProcessHeap(), 0, info);
        NtCurrentTeb()->ReservedForOle = NULL;
    }
}

/******************************************************************************
 * Manage apartments.
 */

/* allocates memory and fills in the necessary fields for a new apartment
 * object */
static APARTMENT *apartment_construct(DWORD model)
{
    APARTMENT *apt;

    TRACE("creating new apartment, model=%ld\n", model);

    apt = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*apt));
    apt->tid = GetCurrentThreadId();

    list_init(&apt->proxies);
    list_init(&apt->stubmgrs);
    list_init(&apt->psclsids);
    apt->ipidc = 0;
    apt->refs = 1;
    apt->remunk_exported = FALSE;
    apt->oidc = 1;
    InitializeCriticalSection(&apt->cs);
    DEBUG_SET_CRITSEC_NAME(&apt->cs, "apartment");

    apt->multi_threaded = !(model & COINIT_APARTMENTTHREADED);

    if (apt->multi_threaded)
    {
        /* FIXME: should be randomly generated by in an RPC call to rpcss */
        apt->oxid = ((OXID)GetCurrentProcessId() << 32) | 0xcafe;
    }
    else
    {
        /* FIXME: should be randomly generated by in an RPC call to rpcss */
        apt->oxid = ((OXID)GetCurrentProcessId() << 32) | GetCurrentThreadId();
    }

    TRACE("Created apartment on OXID %s\n", wine_dbgstr_longlong(apt->oxid));

    /* the locking here is not currently needed for the MTA case, but it
     * doesn't hurt and makes the code simpler */
    EnterCriticalSection(&csApartment);
    list_add_head(&apts, &apt->entry);
    LeaveCriticalSection(&csApartment);

    return apt;
}

/* gets and existing apartment if one exists or otherwise creates an apartment
 * structure which stores OLE apartment-local information and stores a pointer
 * to it in the thread-local storage */
static APARTMENT *apartment_get_or_create(DWORD model)
{
    APARTMENT *apt = COM_CurrentApt();

    if (!apt)
    {
        if (model & COINIT_APARTMENTTHREADED)
        {
            apt = apartment_construct(model);
            COM_CurrentInfo()->apt = apt;
        }
        else
        {
            EnterCriticalSection(&csApartment);

            /* The multi-threaded apartment (MTA) contains zero or more threads interacting
             * with free threaded (ie thread safe) COM objects. There is only ever one MTA
             * in a process */
            if (MTA)
            {
                TRACE("entering the multithreaded apartment %s\n", wine_dbgstr_longlong(MTA->oxid));
                apartment_addref(MTA);
            }
            else
                MTA = apartment_construct(model);

            apt = MTA;
            COM_CurrentInfo()->apt = apt;

            LeaveCriticalSection(&csApartment);
        }
    }

    return apt;
}

static inline BOOL apartment_is_model(APARTMENT *apt, DWORD model)
{
    return (apt->multi_threaded == !(model & COINIT_APARTMENTTHREADED));
}

DWORD apartment_addref(struct apartment *apt)
{
    DWORD refs = InterlockedIncrement(&apt->refs);
    TRACE("%s: before = %ld\n", wine_dbgstr_longlong(apt->oxid), refs - 1);
    return refs;
}

DWORD apartment_release(struct apartment *apt)
{
    DWORD ret;

    EnterCriticalSection(&csApartment);

    ret = InterlockedDecrement(&apt->refs);
    TRACE("%s: after = %ld\n", wine_dbgstr_longlong(apt->oxid), ret);
    /* destruction stuff that needs to happen under csApartment CS */
    if (ret == 0)
    {
        if (apt == MTA) MTA = NULL;
        list_remove(&apt->entry);
    }

    LeaveCriticalSection(&csApartment);

    if (ret == 0)
    {
        struct list *cursor, *cursor2;

        TRACE("destroying apartment %p, oxid %s\n", apt, wine_dbgstr_longlong(apt->oxid));

        /* no locking is needed for this apartment, because no other thread
         * can access it at this point */

        apartment_disconnectproxies(apt);

        if (apt->win) DestroyWindow(apt->win);

        LIST_FOR_EACH_SAFE(cursor, cursor2, &apt->stubmgrs)
        {
            struct stub_manager *stubmgr = LIST_ENTRY(cursor, struct stub_manager, entry);
            /* release the implicit reference given by the fact that the
             * stub has external references (it must do since it is in the
             * stub manager list in the apartment and all non-apartment users
             * must have a ref on the apartment and so it cannot be destroyed).
             */
            stub_manager_int_release(stubmgr);
        }

        LIST_FOR_EACH_SAFE(cursor, cursor2, &apt->psclsids)
        {
            struct registered_psclsid *registered_psclsid =
                LIST_ENTRY(cursor, struct registered_psclsid, entry);

            HeapFree(GetProcessHeap(), 0, registered_psclsid);
            list_remove(&registered_psclsid->entry);
        }

        /* if this assert fires, then another thread took a reference to a
         * stub manager without taking a reference to the containing
         * apartment, which it must do. */
        assert(list_empty(&apt->stubmgrs));

        if (apt->filter) IUnknown_Release(apt->filter);

        DEBUG_CLEAR_CRITSEC_NAME(&apt->cs);
        DeleteCriticalSection(&apt->cs);

        HeapFree(GetProcessHeap(), 0, apt);
    }

    return ret;
}

/* The given OXID must be local to this process: 
 *
 * The ref parameter is here mostly to ensure people remember that
 * they get one, you should normally take a ref for thread safety.
 */
APARTMENT *apartment_findfromoxid(OXID oxid, BOOL ref)
{
    APARTMENT *result = NULL;
    struct list *cursor;

    EnterCriticalSection(&csApartment);
    LIST_FOR_EACH( cursor, &apts )
    {
        struct apartment *apt = LIST_ENTRY( cursor, struct apartment, entry );
        if (apt->oxid == oxid)
        {
            result = apt;
            if (ref) apartment_addref(result);
            break;
        }
    }
    LeaveCriticalSection(&csApartment);

    return result;
}

/* gets the apartment which has a given creator thread ID. The caller must
 * release the reference from the apartment as soon as the apartment pointer
 * is no longer required. */
APARTMENT *apartment_findfromtid(DWORD tid)
{
    APARTMENT *result = NULL;
    struct list *cursor;

    EnterCriticalSection(&csApartment);
    LIST_FOR_EACH( cursor, &apts )
    {
        struct apartment *apt = LIST_ENTRY( cursor, struct apartment, entry );
        if (apt->tid == tid)
        {
            result = apt;
            apartment_addref(result);
            break;
        }
    }
    LeaveCriticalSection(&csApartment);

    return result;
}

static LRESULT CALLBACK apartment_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case DM_EXECUTERPC:
        RPC_ExecuteCall((struct dispatch_params *)lParam);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

HRESULT apartment_createwindowifneeded(struct apartment *apt)
{
    if (apt->multi_threaded)
        return S_OK;

    if (!apt->win)
    {
        HWND hwnd = CreateWindowW(wszAptWinClass, NULL, 0,
                                  0, 0, 0, 0,
                                  0, 0, OLE32_hInstance, NULL);
        if (!hwnd)
        {
            ERR("CreateWindow failed with error %ld\n", GetLastError());
            return HRESULT_FROM_WIN32(GetLastError());
        }
        if (InterlockedCompareExchangePointer((PVOID *)&apt->win, hwnd, NULL))
            /* someone beat us to it */
            DestroyWindow(hwnd);
    }

    return S_OK;
}

HWND apartment_getwindow(struct apartment *apt)
{
    assert(!apt->multi_threaded);
    return apt->win;
}

void apartment_joinmta(void)
{
    apartment_addref(MTA);
    COM_CurrentInfo()->apt = MTA;
}

/*****************************************************************************
 * This section contains OpenDllList implementation
 */

static void COMPOBJ_DLLList_Add(HANDLE hLibrary)
{
    OpenDll *ptr;
    OpenDll *tmp;

    TRACE("\n");

    EnterCriticalSection( &csOpenDllList );

    if (openDllList == NULL) {
        /* empty list -- add first node */
        openDllList = HeapAlloc(GetProcessHeap(),0, sizeof(OpenDll));
	openDllList->hLibrary=hLibrary;
	openDllList->next = NULL;
    } else {
        /* search for this dll */
        int found = FALSE;
        for (ptr = openDllList; ptr->next != NULL; ptr=ptr->next) {
  	    if (ptr->hLibrary == hLibrary) {
	        found = TRUE;
		break;
	    }
        }
	if (!found) {
	    /* dll not found, add it */
 	    tmp = openDllList;
	    openDllList = HeapAlloc(GetProcessHeap(),0, sizeof(OpenDll));
	    openDllList->hLibrary = hLibrary;
	    openDllList->next = tmp;
	}
    }

    LeaveCriticalSection( &csOpenDllList );
}

static void COMPOBJ_DllList_FreeUnused(int Timeout)
{
    OpenDll *curr, *next, *prev = NULL;
    typedef HRESULT (WINAPI *DllCanUnloadNowFunc)(void);
    DllCanUnloadNowFunc DllCanUnloadNow;

    TRACE("\n");

    EnterCriticalSection( &csOpenDllList );

    for (curr = openDllList; curr != NULL; ) {
	DllCanUnloadNow = (DllCanUnloadNowFunc) GetProcAddress(curr->hLibrary, "DllCanUnloadNow");

	if ( (DllCanUnloadNow != NULL) && (DllCanUnloadNow() == S_OK) ) {
	    next = curr->next;

	    TRACE("freeing %p\n", curr->hLibrary);
	    FreeLibrary(curr->hLibrary);

	    HeapFree(GetProcessHeap(), 0, curr);
	    if (curr == openDllList) {
		openDllList = next;
	    } else {
	      prev->next = next;
	    }

	    curr = next;
	} else {
	    prev = curr;
	    curr = curr->next;
	}
    }

    LeaveCriticalSection( &csOpenDllList );
}

/******************************************************************************
 *           CoBuildVersion [OLE32.@]
 *           CoBuildVersion [COMPOBJ.1]
 *
 * Gets the build version of the DLL.
 *
 * PARAMS
 *
 * RETURNS
 *	Current build version, hiword is majornumber, loword is minornumber
 */
DWORD WINAPI CoBuildVersion(void)
{
    TRACE("Returning version %d, build %d.\n", rmm, rup);
    return (rmm<<16)+rup;
}

/******************************************************************************
 *		CoInitialize	[OLE32.@]
 *
 * Initializes the COM libraries by calling CoInitializeEx with
 * COINIT_APARTMENTTHREADED, ie it enters a STA thread.
 *
 * PARAMS
 *  lpReserved [I] Pointer to IMalloc interface (obsolete, should be NULL).
 *
 * RETURNS
 *  Success: S_OK if not already initialized, S_FALSE otherwise.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *   CoInitializeEx
 */
HRESULT WINAPI CoInitialize(LPVOID lpReserved)
{
  /*
   * Just delegate to the newer method.
   */
  return CoInitializeEx(lpReserved, COINIT_APARTMENTTHREADED);
}

/******************************************************************************
 *		CoInitializeEx	[OLE32.@]
 *
 * Initializes the COM libraries.
 *
 * PARAMS
 *  lpReserved [I] Pointer to IMalloc interface (obsolete, should be NULL).
 *  dwCoInit   [I] One or more flags from the COINIT enumeration. See notes.
 *
 * RETURNS
 *  S_OK               if successful,
 *  S_FALSE            if this function was called already.
 *  RPC_E_CHANGED_MODE if a previous call to CoInitializeEx specified another
 *                     threading model.
 *
 * NOTES
 *
 * The behavior used to set the IMalloc used for memory management is
 * obsolete.
 * The dwCoInit parameter must specify of of the following apartment
 * threading models:
 *| COINIT_APARTMENTTHREADED - A single-threaded apartment (STA).
 *| COINIT_MULTITHREADED - A multi-threaded apartment (MTA).
 * The parameter may also specify zero or more of the following flags:
 *| COINIT_DISABLE_OLE1DDE - Don't use DDE for OLE1 support.
 *| COINIT_SPEED_OVER_MEMORY - Trade memory for speed.
 *
 * SEE ALSO
 *   CoUninitialize
 */
HRESULT WINAPI CoInitializeEx(LPVOID lpReserved, DWORD dwCoInit)
{
  HRESULT hr = S_OK;
  APARTMENT *apt;

  TRACE("(%p, %x)\n", lpReserved, (int)dwCoInit);

  if (lpReserved!=NULL)
  {
    ERR("(%p, %x) - Bad parameter passed-in %p, must be an old Windows Application\n", lpReserved, (int)dwCoInit, lpReserved);
  }

  /*
   * Check the lock count. If this is the first time going through the initialize
   * process, we have to initialize the libraries.
   *
   * And crank-up that lock count.
   */
  if (InterlockedExchangeAdd(&s_COMLockCount,1)==0)
  {
    /*
     * Initialize the various COM libraries and data structures.
     */
    TRACE("() - Initializing the COM libraries\n");

    /* we may need to defer this until after apartment initialisation */
    RunningObjectTableImpl_Initialize();
  }

  if (!(apt = COM_CurrentInfo()->apt))
  {
    apt = apartment_get_or_create(dwCoInit);
    if (!apt) return E_OUTOFMEMORY;
  }
  else if (!apartment_is_model(apt, dwCoInit))
  {
    /* Changing the threading model after it's been set is illegal. If this warning is triggered by Wine
       code then we are probably using the wrong threading model to implement that API. */
    ERR("Attempt to change threading model of this apartment from %s to %s\n",
        apt->multi_threaded ? "multi-threaded" : "apartment threaded",
        dwCoInit & COINIT_APARTMENTTHREADED ? "apartment threaded" : "multi-threaded");
    return RPC_E_CHANGED_MODE;
  }
  else
    hr = S_FALSE;

  COM_CurrentInfo()->inits++;

  return hr;
}

/* On COM finalization for a STA thread, the message queue is flushed to ensure no
   pending RPCs are ignored. Non-COM messages are discarded at this point.
 */
static void COM_FlushMessageQueue(void)
{
    MSG message;
    APARTMENT *apt = COM_CurrentApt();

    if (!apt || !apt->win) return;

    TRACE("Flushing STA message queue\n");

    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.hwnd != apt->win)
        {
            WARN("discarding message 0x%x for window %p\n", message.message, message.hwnd);
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

/***********************************************************************
 *           CoUninitialize   [OLE32.@]
 *
 * This method will decrement the refcount on the current apartment, freeing
 * the resources associated with it if it is the last thread in the apartment.
 * If the last apartment is freed, the function will additionally release
 * any COM resources associated with the process.
 *
 * PARAMS
 *
 * RETURNS
 *  Nothing.
 *
 * SEE ALSO
 *   CoInitializeEx
 */
void WINAPI CoUninitialize(void)
{
  struct oletls * info = COM_CurrentInfo();
  LONG lCOMRefCnt;

  TRACE("()\n");

  /* will only happen on OOM */
  if (!info) return;

  /* sanity check */
  if (!info->inits)
  {
    ERR("Mismatched CoUninitialize\n");
    return;
  }

  if (!--info->inits)
  {
    apartment_release(info->apt);
    info->apt = NULL;
  }

  /*
   * Decrease the reference count.
   * If we are back to 0 locks on the COM library, make sure we free
   * all the associated data structures.
   */
  lCOMRefCnt = InterlockedExchangeAdd(&s_COMLockCount,-1);
  if (lCOMRefCnt==1)
  {
    TRACE("() - Releasing the COM libraries\n");

    RunningObjectTableImpl_UnInitialize();

    /* Release the references to the registered class objects */
    COM_RevokeAllClasses();

    /* This will free the loaded COM Dlls  */
    CoFreeAllLibraries();

    /* This ensures we deal with any pending RPCs */
    COM_FlushMessageQueue();
  }
  else if (lCOMRefCnt<1) {
    ERR( "CoUninitialize() - not CoInitialized.\n" );
    InterlockedExchangeAdd(&s_COMLockCount,1); /* restore the lock count. */
  }
}

/******************************************************************************
 *		CoDisconnectObject	[OLE32.@]
 *		CoDisconnectObject	[COMPOBJ.15]
 *
 * Disconnects all connections to this object from remote processes. Dispatches
 * pending RPCs while blocking new RPCs from occurring, and then calls
 * IMarshal::DisconnectObject on the given object.
 *
 * Typically called when the object server is forced to shut down, for instance by
 * the user.
 *
 * PARAMS
 *  lpUnk    [I] The object whose stub should be disconnected.
 *  reserved [I] Reserved. Should be set to 0.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoMarshalInterface, CoReleaseMarshalData, CoLockObjectExternal
 */
HRESULT WINAPI CoDisconnectObject( LPUNKNOWN lpUnk, DWORD reserved )
{
    HRESULT hr;
    IMarshal *marshal;
    APARTMENT *apt;

    TRACE("(%p, 0x%08lx)\n", lpUnk, reserved);

    hr = IUnknown_QueryInterface(lpUnk, &IID_IMarshal, (void **)&marshal);
    if (hr == S_OK)
    {
        hr = IMarshal_DisconnectObject(marshal, reserved);
        IMarshal_Release(marshal);
        return hr;
    }

    apt = COM_CurrentApt();
    if (!apt)
        return CO_E_NOTINITIALIZED;

    apartment_disconnectobject(apt, lpUnk);

    /* Note: native is pretty broken here because it just silently
     * fails, without returning an appropriate error code if the object was
     * not found, making apps think that the object was disconnected, when
     * it actually wasn't */

    return S_OK;
}

/******************************************************************************
 *		CoCreateGuid [OLE32.@]
 *
 * Simply forwards to UuidCreate in RPCRT4.
 *
 * PARAMS
 *  pguid [O] Points to the GUID to initialize.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *   UuidCreate
 */
HRESULT WINAPI CoCreateGuid(GUID *pguid)
{
    return UuidCreate(pguid);
}

/******************************************************************************
 *		CLSIDFromString	[OLE32.@]
 *		IIDFromString   [OLE32.@]
 *
 * Converts a unique identifier from its string representation into
 * the GUID struct.
 *
 * PARAMS
 *  idstr [I] The string representation of the GUID.
 *  id    [O] GUID converted from the string.
 *
 * RETURNS
 *   S_OK on success
 *   CO_E_CLASSSTRING if idstr is not a valid CLSID
 *
 * SEE ALSO
 *  StringFromCLSID
 */
static HRESULT WINAPI __CLSIDFromString(LPCWSTR s, CLSID *id)
{
  int	i;
  BYTE table[256];

  if (!s) {
    memset( id, 0, sizeof (CLSID) );
    return S_OK;
  }

  /* validate the CLSID string */
  if (strlenW(s) != 38)
    return CO_E_CLASSSTRING;

  if ((s[0]!='{') || (s[9]!='-') || (s[14]!='-') || (s[19]!='-') || (s[24]!='-') || (s[37]!='}'))
    return CO_E_CLASSSTRING;

  for (i=1; i<37; i++) {
    if ((i == 9)||(i == 14)||(i == 19)||(i == 24)) continue;
    if (!(((s[i] >= '0') && (s[i] <= '9'))  ||
          ((s[i] >= 'a') && (s[i] <= 'f'))  ||
          ((s[i] >= 'A') && (s[i] <= 'F'))))
       return CO_E_CLASSSTRING;
  }

  TRACE("%s -> %p\n", debugstr_w(s), id);

  /* quick lookup table */
  memset(table, 0, 256);

  for (i = 0; i < 10; i++) {
    table['0' + i] = i;
  }
  for (i = 0; i < 6; i++) {
    table['A' + i] = i+10;
    table['a' + i] = i+10;
  }

  /* in form {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} */

  id->Data1 = (table[s[1]] << 28 | table[s[2]] << 24 | table[s[3]] << 20 | table[s[4]] << 16 |
               table[s[5]] << 12 | table[s[6]] << 8  | table[s[7]] << 4  | table[s[8]]);
  id->Data2 = table[s[10]] << 12 | table[s[11]] << 8 | table[s[12]] << 4 | table[s[13]];
  id->Data3 = table[s[15]] << 12 | table[s[16]] << 8 | table[s[17]] << 4 | table[s[18]];

  /* these are just sequential bytes */
  id->Data4[0] = table[s[20]] << 4 | table[s[21]];
  id->Data4[1] = table[s[22]] << 4 | table[s[23]];
  id->Data4[2] = table[s[25]] << 4 | table[s[26]];
  id->Data4[3] = table[s[27]] << 4 | table[s[28]];
  id->Data4[4] = table[s[29]] << 4 | table[s[30]];
  id->Data4[5] = table[s[31]] << 4 | table[s[32]];
  id->Data4[6] = table[s[33]] << 4 | table[s[34]];
  id->Data4[7] = table[s[35]] << 4 | table[s[36]];

  return S_OK;
}

/*****************************************************************************/

HRESULT WINAPI CLSIDFromString(LPOLESTR idstr, CLSID *id )
{
    HRESULT ret;

    ret = __CLSIDFromString(idstr, id);
    if(ret != S_OK) { /* It appears a ProgID is also valid */
        ret = CLSIDFromProgID(idstr, id);
    }
    return ret;
}

/* Converts a GUID into the respective string representation. */
HRESULT WINE_StringFromCLSID(
	const CLSID *id,	/* [in] GUID to be converted */
	LPSTR idstr		/* [out] pointer to buffer to contain converted guid */
) {
  static const char *hex = "0123456789ABCDEF";
  char *s;
  int	i;

  if (!id)
	{ ERR("called with id=Null\n");
	  *idstr = 0x00;
	  return E_FAIL;
	}

  sprintf(idstr, "{%08lX-%04X-%04X-%02X%02X-",
	  id->Data1, id->Data2, id->Data3,
	  id->Data4[0], id->Data4[1]);
  s = &idstr[25];

  /* 6 hex bytes */
  for (i = 2; i < 8; i++) {
    *s++ = hex[id->Data4[i]>>4];
    *s++ = hex[id->Data4[i] & 0xf];
  }

  *s++ = '}';
  *s++ = '\0';

  TRACE("%p->%s\n", id, idstr);

  return S_OK;
}


/******************************************************************************
 *		StringFromCLSID	[OLE32.@]
 *		StringFromIID   [OLE32.@]
 *
 * Converts a GUID into the respective string representation.
 * The target string is allocated using the OLE IMalloc.
 *
 * PARAMS
 *  id    [I] the GUID to be converted.
 *  idstr [O] A pointer to a to-be-allocated pointer pointing to the resulting string.
 *
 * RETURNS
 *   S_OK
 *   E_FAIL
 *
 * SEE ALSO
 *  StringFromGUID2, CLSIDFromString
 */
HRESULT WINAPI StringFromCLSID(REFCLSID id, LPOLESTR *idstr)
{
	char            buf[80];
	HRESULT       ret;
	LPMALLOC	mllc;

	if ((ret = CoGetMalloc(0,&mllc)))
		return ret;

	ret=WINE_StringFromCLSID(id,buf);
	if (!ret) {
            DWORD len = MultiByteToWideChar( CP_ACP, 0, buf, -1, NULL, 0 );
            *idstr = IMalloc_Alloc( mllc, len * sizeof(WCHAR) );
            MultiByteToWideChar( CP_ACP, 0, buf, -1, *idstr, len );
	}
	return ret;
}

/******************************************************************************
 *		StringFromGUID2	[OLE32.@]
 *		StringFromGUID2	[COMPOBJ.76]
 *
 * Modified version of StringFromCLSID that allows you to specify max
 * buffer size.
 *
 * PARAMS
 *  id   [I] GUID to convert to string.
 *  str  [O] Buffer where the result will be stored.
 *  cmax [I] Size of the buffer in characters.
 *
 * RETURNS
 *	Success: The length of the resulting string in characters.
 *  Failure: 0.
 */
INT WINAPI StringFromGUID2(REFGUID id, LPOLESTR str, INT cmax)
{
  char		xguid[80];

  if (WINE_StringFromCLSID(id,xguid))
  	return 0;
  return MultiByteToWideChar( CP_ACP, 0, xguid, -1, str, cmax );
}

/* open HKCR\\CLSID\\{string form of clsid}\\{keyname} key */
HRESULT COM_OpenKeyForCLSID(REFCLSID clsid, LPCWSTR keyname, REGSAM access, HKEY *subkey)
{
    static const WCHAR wszCLSIDSlash[] = {'C','L','S','I','D','\\',0};
    WCHAR path[CHARS_IN_GUID + ARRAYSIZE(wszCLSIDSlash) - 1];
    LONG res;
    HKEY key;

    strcpyW(path, wszCLSIDSlash);
    StringFromGUID2(clsid, path + strlenW(wszCLSIDSlash), CHARS_IN_GUID);
    res = RegOpenKeyExW(HKEY_CLASSES_ROOT, path, 0, keyname ? KEY_READ : access, &key);
    if (res == ERROR_FILE_NOT_FOUND)
        return REGDB_E_CLASSNOTREG;
    else if (res != ERROR_SUCCESS)
        return REGDB_E_READREGDB;

    if (!keyname)
    {
        *subkey = key;
        return S_OK;
    }

    res = RegOpenKeyExW(key, keyname, 0, access, subkey);
    RegCloseKey(key);
    if (res == ERROR_FILE_NOT_FOUND)
        return REGDB_E_KEYMISSING;
    else if (res != ERROR_SUCCESS)
        return REGDB_E_READREGDB;

    return S_OK;
}

/******************************************************************************
 *               ProgIDFromCLSID [OLE32.@]
 *
 * Converts a class id into the respective program ID.
 *
 * PARAMS
 *  clsid        [I] Class ID, as found in registry.
 *  lplpszProgID [O] Associated ProgID.
 *
 * RETURNS
 *   S_OK
 *   E_OUTOFMEMORY
 *   REGDB_E_CLASSNOTREG if the given clsid has no associated ProgID
 */
HRESULT WINAPI ProgIDFromCLSID(REFCLSID clsid, LPOLESTR *lplpszProgID)
{
    static const WCHAR wszProgID[] = {'P','r','o','g','I','D',0};
    HKEY     hkey;
    HRESULT  ret;
    LONG progidlen = 0;

    *lplpszProgID = NULL;
    ret = COM_OpenKeyForCLSID(clsid, wszProgID, KEY_READ, &hkey);
    if (FAILED(ret))
        return ret;

    if (RegQueryValueW(hkey, NULL, NULL, &progidlen))
      ret = REGDB_E_CLASSNOTREG;

    if (ret == S_OK)
    {
      *lplpszProgID = CoTaskMemAlloc(progidlen * sizeof(WCHAR));
      if (*lplpszProgID)
      {
        if (RegQueryValueW(hkey, NULL, *lplpszProgID, &progidlen))
          ret = REGDB_E_CLASSNOTREG;
      }
      else
        ret = E_OUTOFMEMORY;
    }

    RegCloseKey(hkey);
    return ret;
}

/******************************************************************************
 *		CLSIDFromProgID	[OLE32.@]
 *
 * Converts a program id into the respective GUID.
 *
 * PARAMS
 *  progid [I] Unicode program ID, as found in registry.
 *  riid   [O] Associated CLSID.
 *
 * RETURNS
 *	Success: S_OK
 *  Failure: CO_E_CLASSSTRING - the given ProgID cannot be found.
 */
HRESULT WINAPI CLSIDFromProgID(LPCOLESTR progid, LPCLSID riid)
{
    static const WCHAR clsidW[] = { '\\','C','L','S','I','D',0 };
    WCHAR buf2[CHARS_IN_GUID];
    LONG buf2len = sizeof(buf2);
    HKEY xhkey;

    WCHAR *buf = HeapAlloc( GetProcessHeap(),0,(strlenW(progid)+8) * sizeof(WCHAR) );
    strcpyW( buf, progid );
    strcatW( buf, clsidW );
    if (RegOpenKeyW(HKEY_CLASSES_ROOT,buf,&xhkey))
    {
        HeapFree(GetProcessHeap(),0,buf);
        return CO_E_CLASSSTRING;
    }
    HeapFree(GetProcessHeap(),0,buf);

    if (RegQueryValueW(xhkey,NULL,buf2,&buf2len))
    {
        RegCloseKey(xhkey);
        return CO_E_CLASSSTRING;
    }
    RegCloseKey(xhkey);
    return CLSIDFromString(buf2,riid);
}


/*****************************************************************************
 *             CoGetPSClsid [OLE32.@]
 *
 * Retrieves the CLSID of the proxy/stub factory that implements
 * IPSFactoryBuffer for the specified interface.
 *
 * PARAMS
 *  riid   [I] Interface whose proxy/stub CLSID is to be returned.
 *  pclsid [O] Where to store returned proxy/stub CLSID.
 * 
 * RETURNS
 *   S_OK
 *   E_OUTOFMEMORY
 *   REGDB_E_IIDNOTREG if no PSFactoryBuffer is associated with the IID, or it could not be parsed
 *
 * NOTES
 *
 * The standard marshaller activates the object with the CLSID
 * returned and uses the CreateProxy and CreateStub methods on its
 * IPSFactoryBuffer interface to construct the proxies and stubs for a
 * given object.
 *
 * CoGetPSClsid determines this CLSID by searching the
 * HKEY_CLASSES_ROOT\Interface\{string form of riid}\ProxyStubClsid32
 * in the registry and any interface id registered by
 * CoRegisterPSClsid within the current process.
 *
 * BUGS
 *
 * Native returns S_OK for interfaces with a key in HKCR\Interface, but
 * without a ProxyStubClsid32 key and leaves garbage in pclsid. This should be
 * considered a bug in native unless an application depends on this (unlikely).
 *
 * SEE ALSO
 *  CoRegisterPSClsid.
 */
HRESULT WINAPI CoGetPSClsid(REFIID riid, CLSID *pclsid)
{
    static const WCHAR wszInterface[] = {'I','n','t','e','r','f','a','c','e','\\',0};
    static const WCHAR wszPSC[] = {'\\','P','r','o','x','y','S','t','u','b','C','l','s','i','d','3','2',0};
    WCHAR path[ARRAYSIZE(wszInterface) - 1 + CHARS_IN_GUID - 1 + ARRAYSIZE(wszPSC)];
    WCHAR value[CHARS_IN_GUID];
    LONG len;
    HKEY hkey;
    APARTMENT *apt = COM_CurrentApt();
    struct registered_psclsid *registered_psclsid;

    TRACE("() riid=%s, pclsid=%p\n", debugstr_guid(riid), pclsid);

    if (!apt)
    {
        ERR("apartment not initialised\n");
        return CO_E_NOTINITIALIZED;
    }

    EnterCriticalSection(&apt->cs);

    LIST_FOR_EACH_ENTRY(registered_psclsid, &apt->psclsids, struct registered_psclsid, entry)
        if (IsEqualIID(&registered_psclsid->iid, riid))
        {
            *pclsid = registered_psclsid->clsid;
            LeaveCriticalSection(&apt->cs);
            return S_OK;
        }

    LeaveCriticalSection(&apt->cs);

    /* Interface\\{string form of riid}\\ProxyStubClsid32 */
    strcpyW(path, wszInterface);
    StringFromGUID2(riid, path + ARRAYSIZE(wszInterface) - 1, CHARS_IN_GUID);
    strcpyW(path + ARRAYSIZE(wszInterface) - 1 + CHARS_IN_GUID - 1, wszPSC);

    /* Open the key.. */
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, path, 0, KEY_READ, &hkey))
    {
        WARN("No PSFactoryBuffer object is registered for IID %s\n", debugstr_guid(riid));
        return REGDB_E_IIDNOTREG;
    }

    /* ... Once we have the key, query the registry to get the
       value of CLSID as a string, and convert it into a
       proper CLSID structure to be passed back to the app */
    len = sizeof(value);
    if (ERROR_SUCCESS != RegQueryValueW(hkey, NULL, value, &len))
    {
        RegCloseKey(hkey);
        return REGDB_E_IIDNOTREG;
    }
    RegCloseKey(hkey);

    /* We have the CLSid we want back from the registry as a string, so
       lets convert it into a CLSID structure */
    if (CLSIDFromString(value, pclsid) != NOERROR)
        return REGDB_E_IIDNOTREG;

    TRACE ("() Returning CLSID=%s\n", debugstr_guid(pclsid));
    return S_OK;
}

/*****************************************************************************
 *             CoRegisterPSClsid [OLE32.@]
 *
 * Register a proxy/stub CLSID for the given interface in the current process
 * only.
 *
 * PARAMS
 *  riid   [I] Interface whose proxy/stub CLSID is to be registered.
 *  rclsid [I] CLSID of the proxy/stub.
 * 
 * RETURNS
 *   Success: S_OK
 *   Failure: E_OUTOFMEMORY
 *
 * NOTES
 *
 * This function does not add anything to the registry and the effects are
 * limited to the lifetime of the current process.
 *
 * SEE ALSO
 *  CoGetPSClsid.
 */
HRESULT WINAPI CoRegisterPSClsid(REFIID riid, REFCLSID rclsid)
{
    APARTMENT *apt = COM_CurrentApt();
    struct registered_psclsid *registered_psclsid;

    TRACE("(%s, %s)\n", debugstr_guid(riid), debugstr_guid(rclsid));

    if (!apt)
    {
        ERR("apartment not initialised\n");
        return CO_E_NOTINITIALIZED;
    }

    EnterCriticalSection(&apt->cs);

    LIST_FOR_EACH_ENTRY(registered_psclsid, &apt->psclsids, struct registered_psclsid, entry)
        if (IsEqualIID(&registered_psclsid->iid, riid))
        {
            registered_psclsid->clsid = *rclsid;
            LeaveCriticalSection(&apt->cs);
            return S_OK;
        }

    registered_psclsid = HeapAlloc(GetProcessHeap(), 0, sizeof(struct registered_psclsid));
    if (!registered_psclsid)
    {
        LeaveCriticalSection(&apt->cs);
        return E_OUTOFMEMORY;
    }

    registered_psclsid->iid = *riid;
    registered_psclsid->clsid = *rclsid;
    list_add_head(&apt->psclsids, &registered_psclsid->entry);

    LeaveCriticalSection(&apt->cs);

    return S_OK;
}



/***********************************************************************
 *		WriteClassStm (OLE32.@)
 *
 * Writes a CLSID to a stream.
 *
 * PARAMS
 *  pStm   [I] Stream to write to.
 *  rclsid [I] CLSID to write.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 */
HRESULT WINAPI WriteClassStm(IStream *pStm,REFCLSID rclsid)
{
    TRACE("(%p,%p)\n",pStm,rclsid);

    if (rclsid==NULL)
        return E_INVALIDARG;

    return IStream_Write(pStm,rclsid,sizeof(CLSID),NULL);
}

/***********************************************************************
 *		ReadClassStm (OLE32.@)
 *
 * Reads a CLSID from a stream.
 *
 * PARAMS
 *  pStm   [I] Stream to read from.
 *  rclsid [O] CLSID to read.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 */
HRESULT WINAPI ReadClassStm(IStream *pStm,CLSID *pclsid)
{
    ULONG nbByte;
    HRESULT res;

    TRACE("(%p,%p)\n",pStm,pclsid);

    if (pclsid==NULL)
        return E_INVALIDARG;

    res = IStream_Read(pStm,(void*)pclsid,sizeof(CLSID),&nbByte);

    if (FAILED(res))
        return res;

    if (nbByte != sizeof(CLSID))
        return S_FALSE;
    else
        return S_OK;
}


/***
 * COM_GetRegisteredClassObject
 *
 * This internal method is used to scan the registered class list to
 * find a class object.
 *
 * Params:
 *   rclsid        Class ID of the class to find.
 *   dwClsContext  Class context to match.
 *   ppv           [out] returns a pointer to the class object. Complying
 *                 to normal COM usage, this method will increase the
 *                 reference count on this object.
 */
static HRESULT COM_GetRegisteredClassObject(
	REFCLSID    rclsid,
	DWORD       dwClsContext,
	LPUNKNOWN*  ppUnk)
{
  HRESULT hr = S_FALSE;
  RegisteredClass* curClass;

  EnterCriticalSection( &csRegisteredClassList );

  /*
   * Sanity check
   */
  assert(ppUnk!=0);

  /*
   * Iterate through the whole list and try to match the class ID.
   */
  curClass = firstRegisteredClass;

  while (curClass != 0)
  {
    /*
     * Check if we have a match on the class ID.
     */
    if (IsEqualGUID(&(curClass->classIdentifier), rclsid))
    {
      /*
       * Since we don't do out-of process or DCOM just right away, let's ignore the
       * class context.
       */

      /*
       * We have a match, return the pointer to the class object.
       */
      *ppUnk = curClass->classObject;

      IUnknown_AddRef(curClass->classObject);

      hr = S_OK;
      goto end;
    }

    /*
     * Step to the next class in the list.
     */
    curClass = curClass->nextClass;
  }

end:
  LeaveCriticalSection( &csRegisteredClassList );
  /*
   * If we get to here, we haven't found our class.
   */
  return hr;
}

/******************************************************************************
 *		CoRegisterClassObject	[OLE32.@]
 *
 * Registers the class object for a given class ID. Servers housed in EXE
 * files use this method instead of exporting DllGetClassObject to allow
 * other code to connect to their objects.
 *
 * PARAMS
 *  rclsid       [I] CLSID of the object to register.
 *  pUnk         [I] IUnknown of the object.
 *  dwClsContext [I] CLSCTX flags indicating the context in which to run the executable.
 *  flags        [I] REGCLS flags indicating how connections are made.
 *  lpdwRegister [I] A unique cookie that can be passed to CoRevokeClassObject.
 *
 * RETURNS
 *   S_OK on success,
 *   E_INVALIDARG if lpdwRegister or pUnk are NULL,
 *   CO_E_OBJISREG if the object is already registered. We should not return this.
 *
 * SEE ALSO
 *   CoRevokeClassObject, CoGetClassObject
 *
 * BUGS
 *  MSDN claims that multiple interface registrations are legal, but we
 *  can't do that with our current implementation.
 */
HRESULT WINAPI CoRegisterClassObject(
    REFCLSID rclsid,
    LPUNKNOWN pUnk,
    DWORD dwClsContext,
    DWORD flags,
    LPDWORD lpdwRegister)
{
  RegisteredClass* newClass;
  LPUNKNOWN        foundObject;
  HRESULT          hr;

  TRACE("(%s,%p,0x%08lx,0x%08lx,%p)\n",
	debugstr_guid(rclsid),pUnk,dwClsContext,flags,lpdwRegister);

  if ( (lpdwRegister==0) || (pUnk==0) )
    return E_INVALIDARG;

  if (!COM_CurrentApt())
  {
      ERR("COM was not initialized\n");
      return CO_E_NOTINITIALIZED;
  }

  *lpdwRegister = 0;

  /*
   * First, check if the class is already registered.
   * If it is, this should cause an error.
   */
  hr = COM_GetRegisteredClassObject(rclsid, dwClsContext, &foundObject);
  if (hr == S_OK) {
    if (flags & REGCLS_MULTIPLEUSE) {
      if (dwClsContext & CLSCTX_LOCAL_SERVER)
        hr = CoLockObjectExternal(foundObject, TRUE, FALSE);
      IUnknown_Release(foundObject);
      return hr;
    }
    IUnknown_Release(foundObject);
    ERR("object already registered for class %s\n", debugstr_guid(rclsid));
    return CO_E_OBJISREG;
  }

  newClass = HeapAlloc(GetProcessHeap(), 0, sizeof(RegisteredClass));
  if ( newClass == NULL )
    return E_OUTOFMEMORY;

  EnterCriticalSection( &csRegisteredClassList );

  newClass->classIdentifier = *rclsid;
  newClass->runContext      = dwClsContext;
  newClass->connectFlags    = flags;
  newClass->pMarshaledData  = NULL;

  /*
   * Use the address of the chain node as the cookie since we are sure it's
   * unique. FIXME: not on 64-bit platforms.
   */
  newClass->dwCookie        = (DWORD)newClass;
  newClass->nextClass       = firstRegisteredClass;

  /*
   * Since we're making a copy of the object pointer, we have to increase its
   * reference count.
   */
  newClass->classObject     = pUnk;
  IUnknown_AddRef(newClass->classObject);

  firstRegisteredClass = newClass;
  LeaveCriticalSection( &csRegisteredClassList );

  *lpdwRegister = newClass->dwCookie;

  if (dwClsContext & CLSCTX_LOCAL_SERVER) {
      IClassFactory *classfac;

      hr = IUnknown_QueryInterface(newClass->classObject, &IID_IClassFactory,
                                   (LPVOID*)&classfac);
      if (hr) return hr;

      hr = CreateStreamOnHGlobal(0, TRUE, &newClass->pMarshaledData);
      if (hr) {
          FIXME("Failed to create stream on hglobal, %lx\n", hr);
          IUnknown_Release(classfac);
          return hr;
      }
      hr = CoMarshalInterface(newClass->pMarshaledData, &IID_IClassFactory,
                              (LPVOID)classfac, MSHCTX_LOCAL, NULL,
                              MSHLFLAGS_TABLESTRONG);
      if (hr) {
          FIXME("CoMarshalInterface failed, %lx!\n",hr);
          IUnknown_Release(classfac);
          return hr;
      }

      IUnknown_Release(classfac);

      RPC_StartLocalServer(&newClass->classIdentifier, newClass->pMarshaledData);
  }
  return S_OK;
}

/***********************************************************************
 *           CoRevokeClassObject [OLE32.@]
 *
 * Removes a class object from the class registry.
 *
 * PARAMS
 *  dwRegister [I] Cookie returned from CoRegisterClassObject().
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoRegisterClassObject
 */
HRESULT WINAPI CoRevokeClassObject(
        DWORD dwRegister)
{
  HRESULT hr = E_INVALIDARG;
  RegisteredClass** prevClassLink;
  RegisteredClass*  curClass;

  TRACE("(%08lx)\n",dwRegister);

  EnterCriticalSection( &csRegisteredClassList );

  /*
   * Iterate through the whole list and try to match the cookie.
   */
  curClass      = firstRegisteredClass;
  prevClassLink = &firstRegisteredClass;

  while (curClass != 0)
  {
    /*
     * Check if we have a match on the cookie.
     */
    if (curClass->dwCookie == dwRegister)
    {
      /*
       * Remove the class from the chain.
       */
      *prevClassLink = curClass->nextClass;

      /*
       * Release the reference to the class object.
       */
      IUnknown_Release(curClass->classObject);

      if (curClass->pMarshaledData)
      {
        LARGE_INTEGER zero;
        memset(&zero, 0, sizeof(zero));
        /* FIXME: stop local server thread */
        IStream_Seek(curClass->pMarshaledData, zero, SEEK_SET, NULL);
        CoReleaseMarshalData(curClass->pMarshaledData);
      }

      /*
       * Free the memory used by the chain node.
       */
      HeapFree(GetProcessHeap(), 0, curClass);

      hr = S_OK;
      goto end;
    }

    /*
     * Step to the next class in the list.
     */
    prevClassLink = &(curClass->nextClass);
    curClass      = curClass->nextClass;
  }

end:
  LeaveCriticalSection( &csRegisteredClassList );
  /*
   * If we get to here, we haven't found our class.
   */
  return hr;
}

/***********************************************************************
 *	COM_RegReadPath	[internal]
 *
 *	Reads a registry value and expands it when necessary
 */
static DWORD COM_RegReadPath(HKEY hkeyroot, const WCHAR *keyname, const WCHAR *valuename, WCHAR * dst, DWORD dstlen)
{
	DWORD ret;
	HKEY key;
	DWORD keytype;
	WCHAR src[MAX_PATH];
	DWORD dwLength = dstlen * sizeof(WCHAR);

	if((ret = RegOpenKeyExW(hkeyroot, keyname, 0, KEY_READ, &key)) == ERROR_SUCCESS) {
          if( (ret = RegQueryValueExW(key, NULL, NULL, &keytype, (LPBYTE)src, &dwLength)) == ERROR_SUCCESS ) {
            if (keytype == REG_EXPAND_SZ) {
              if (dstlen <= ExpandEnvironmentStringsW(src, dst, dstlen)) ret = ERROR_MORE_DATA;
            } else {
              lstrcpynW(dst, src, dstlen);
            }
	  }
          RegCloseKey (key);
	}
	return ret;
}

static HRESULT get_inproc_class_object(HKEY hkeydll, REFCLSID rclsid, REFIID riid, void **ppv)
{
    HINSTANCE hLibrary;
    typedef HRESULT (CALLBACK *DllGetClassObjectFunc)(REFCLSID clsid, REFIID iid, LPVOID *ppv);
    DllGetClassObjectFunc DllGetClassObject;
    WCHAR dllpath[MAX_PATH+1];
    HRESULT hr;

    if (COM_RegReadPath(hkeydll, NULL, NULL, dllpath, ARRAYSIZE(dllpath)) != ERROR_SUCCESS)
    {
        /* failure: CLSID is not found in registry */
        WARN("class %s not registered inproc\n", debugstr_guid(rclsid));
        return REGDB_E_CLASSNOTREG;
    }

    if ((hLibrary = LoadLibraryExW(dllpath, 0, LOAD_WITH_ALTERED_SEARCH_PATH)) == 0)
    {
        /* failure: DLL could not be loaded */
        ERR("couldn't load in-process dll %s\n", debugstr_w(dllpath));
        return E_ACCESSDENIED; /* FIXME: or should this be CO_E_DLLNOTFOUND? */
    }

    if (!(DllGetClassObject = (DllGetClassObjectFunc)GetProcAddress(hLibrary, "DllGetClassObject")))
    {
        /* failure: the dll did not export DllGetClassObject */
        ERR("couldn't find function DllGetClassObject in %s\n", debugstr_w(dllpath));
        FreeLibrary( hLibrary );
        return CO_E_DLLNOTFOUND;
    }

    /* OK: get the ClassObject */
    COMPOBJ_DLLList_Add( hLibrary );
    hr = DllGetClassObject(rclsid, riid, ppv);

    if (hr != S_OK)
        ERR("DllGetClassObject returned error 0x%08lx\n", hr);

    return hr;
}

/***********************************************************************
 *           CoGetClassObject [OLE32.@]
 *
 * FIXME.  If request allows of several options and there is a failure
 *         with one (other than not being registered) do we try the
 *         others or return failure?  (E.g. inprocess is registered but
 *         the DLL is not found but the server version works)
 */
HRESULT WINAPI CoGetClassObject(
    REFCLSID rclsid, DWORD dwClsContext, COSERVERINFO *pServerInfo,
    REFIID iid, LPVOID *ppv)
{
    LPUNKNOWN	regClassObject;
    HRESULT	hres = E_UNEXPECTED;

    TRACE("\n\tCLSID:\t%s,\n\tIID:\t%s\n", debugstr_guid(rclsid), debugstr_guid(iid));

    if (!ppv)
        return E_INVALIDARG;

    *ppv = NULL;

    if (!COM_CurrentApt())
    {
        ERR("apartment not initialised\n");
        return CO_E_NOTINITIALIZED;
    }

    if (pServerInfo) {
	FIXME("\tpServerInfo: name=%s\n",debugstr_w(pServerInfo->pwszName));
	FIXME("\t\tpAuthInfo=%p\n",pServerInfo->pAuthInfo);
    }

    /*
     * First, try and see if we can't match the class ID with one of the
     * registered classes.
     */
    if (S_OK == COM_GetRegisteredClassObject(rclsid, dwClsContext, &regClassObject))
    {
      /* Get the required interface from the retrieved pointer. */
      hres = IUnknown_QueryInterface(regClassObject, iid, ppv);

      /*
       * Since QI got another reference on the pointer, we want to release the
       * one we already have. If QI was unsuccessful, this will release the object. This
       * is good since we are not returning it in the "out" parameter.
       */
      IUnknown_Release(regClassObject);

      return hres;
    }

    /* First try in-process server */
    if (CLSCTX_INPROC_SERVER & dwClsContext)
    {
        static const WCHAR wszInprocServer32[] = {'I','n','p','r','o','c','S','e','r','v','e','r','3','2',0};
        HKEY hkey;

        if (IsEqualCLSID(rclsid, &CLSID_InProcFreeMarshaler))
            return FTMarshalCF_Create(iid, ppv);

        hres = COM_OpenKeyForCLSID(rclsid, wszInprocServer32, KEY_READ, &hkey);
        if (FAILED(hres))
        {
            if (hres == REGDB_E_CLASSNOTREG)
                ERR("class %s not registered\n", debugstr_guid(rclsid));
            else
                WARN("class %s not registered as in-proc server\n", debugstr_guid(rclsid));
        }

        if (SUCCEEDED(hres))
        {
            hres = get_inproc_class_object(hkey, rclsid, iid, ppv);
            RegCloseKey(hkey);
        }

        /* return if we got a class, otherwise fall through to one of the
         * other types */
        if (SUCCEEDED(hres))
            return hres;
    }

    /* Next try in-process handler */
    if (CLSCTX_INPROC_HANDLER & dwClsContext)
    {
        static const WCHAR wszInprocHandler32[] = {'I','n','p','r','o','c','H','a','n','d','l','e','r','3','2',0};
        HKEY hkey;

        hres = COM_OpenKeyForCLSID(rclsid, wszInprocHandler32, KEY_READ, &hkey);
        if (FAILED(hres))
        {
            if (hres == REGDB_E_CLASSNOTREG)
                ERR("class %s not registered\n", debugstr_guid(rclsid));
            else
                WARN("class %s not registered in-proc handler\n", debugstr_guid(rclsid));
        }

        if (SUCCEEDED(hres))
        {
            hres = get_inproc_class_object(hkey, rclsid, iid, ppv);
            RegCloseKey(hkey);
        }

        /* return if we got a class, otherwise fall through to one of the
         * other types */
        if (SUCCEEDED(hres))
            return hres;
    }

    /* Next try out of process */
    if (CLSCTX_LOCAL_SERVER & dwClsContext)
    {
        hres = RPC_GetLocalClassObject(rclsid,iid,ppv);
        if (SUCCEEDED(hres))
            return hres;
    }

    /* Finally try remote: this requires networked DCOM (a lot of work) */
    if (CLSCTX_REMOTE_SERVER & dwClsContext)
    {
        FIXME ("CLSCTX_REMOTE_SERVER not supported\n");
        hres = E_NOINTERFACE;
    }

    if (FAILED(hres))
        ERR("no class object %s could be created for context 0x%lx\n",
            debugstr_guid(rclsid), dwClsContext);
    return hres;
}

/***********************************************************************
 *        CoResumeClassObjects (OLE32.@)
 *
 * Resumes all class objects registered with REGCLS_SUSPENDED.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 */
HRESULT WINAPI CoResumeClassObjects(void)
{
       FIXME("stub\n");
	return S_OK;
}

/***********************************************************************
 *        GetClassFile (OLE32.@)
 *
 * This function supplies the CLSID associated with the given filename.
 */
HRESULT WINAPI GetClassFile(LPCOLESTR filePathName,CLSID *pclsid)
{
    IStorage *pstg=0;
    HRESULT res;
    int nbElm, length, i;
    LONG sizeProgId;
    LPOLESTR *pathDec=0,absFile=0,progId=0;
    LPWSTR extension;
    static const WCHAR bkslashW[] = {'\\',0};
    static const WCHAR dotW[] = {'.',0};

    TRACE("%s, %p\n", debugstr_w(filePathName), pclsid);

    /* if the file contain a storage object the return the CLSID written by IStorage_SetClass method*/
    if((StgIsStorageFile(filePathName))==S_OK){

        res=StgOpenStorage(filePathName,NULL,STGM_READ | STGM_SHARE_DENY_WRITE,NULL,0,&pstg);

        if (SUCCEEDED(res))
            res=ReadClassStg(pstg,pclsid);

        IStorage_Release(pstg);

        return res;
    }
    /* if the file is not a storage object then attemps to match various bits in the file against a
       pattern in the registry. this case is not frequently used ! so I present only the psodocode for
       this case

     for(i=0;i<nFileTypes;i++)

        for(i=0;j<nPatternsForType;j++){

            PATTERN pat;
            HANDLE  hFile;

            pat=ReadPatternFromRegistry(i,j);
            hFile=CreateFileW(filePathName,,,,,,hFile);
            SetFilePosition(hFile,pat.offset);
            ReadFile(hFile,buf,pat.size,&r,NULL);
            if (memcmp(buf&pat.mask,pat.pattern.pat.size)==0){

                *pclsid=ReadCLSIDFromRegistry(i);
                return S_OK;
            }
        }
     */

    /* if the above strategies fail then search for the extension key in the registry */

    /* get the last element (absolute file) in the path name */
    nbElm=FileMonikerImpl_DecomposePath(filePathName,&pathDec);
    absFile=pathDec[nbElm-1];

    /* failed if the path represente a directory and not an absolute file name*/
    if (!lstrcmpW(absFile, bkslashW))
        return MK_E_INVALIDEXTENSION;

    /* get the extension of the file */
    extension = NULL;
    length=lstrlenW(absFile);
    for(i = length-1; (i >= 0) && *(extension = &absFile[i]) != '.'; i--)
        /* nothing */;

    if (!extension || !lstrcmpW(extension, dotW))
        return MK_E_INVALIDEXTENSION;

    res=RegQueryValueW(HKEY_CLASSES_ROOT, extension, NULL, &sizeProgId);

    /* get the progId associated to the extension */
    progId = CoTaskMemAlloc(sizeProgId);
    res = RegQueryValueW(HKEY_CLASSES_ROOT, extension, progId, &sizeProgId);

    if (res==ERROR_SUCCESS)
        /* return the clsid associated to the progId */
        res= CLSIDFromProgID(progId,pclsid);

    for(i=0; pathDec[i]!=NULL;i++)
        CoTaskMemFree(pathDec[i]);
    CoTaskMemFree(pathDec);

    CoTaskMemFree(progId);

    if (res==ERROR_SUCCESS)
        return res;

    return MK_E_INVALIDEXTENSION;
}

/***********************************************************************
 *           CoCreateInstance [OLE32.@]
 */
HRESULT WINAPI CoCreateInstance(
	REFCLSID rclsid,
	LPUNKNOWN pUnkOuter,
	DWORD dwClsContext,
	REFIID iid,
	LPVOID *ppv)
{
  HRESULT hres;
  LPCLASSFACTORY lpclf = 0;

  TRACE("(rclsid=%s, pUnkOuter=%p, dwClsContext=%08lx, riid=%s, ppv=%p)\n", debugstr_guid(rclsid),
        pUnkOuter, dwClsContext, debugstr_guid(iid), ppv);

  /*
   * Sanity check
   */
  if (ppv==0)
    return E_POINTER;

  /*
   * Initialize the "out" parameter
   */
  *ppv = 0;

  if (!COM_CurrentApt())
  {
      ERR("apartment not initialised\n");
      return CO_E_NOTINITIALIZED;
  }

  /*
   * The Standard Global Interface Table (GIT) object is a process-wide singleton.
   * Rather than create a class factory, we can just check for it here
   */
  if (IsEqualIID(rclsid, &CLSID_StdGlobalInterfaceTable)) {
    if (StdGlobalInterfaceTableInstance == NULL)
      StdGlobalInterfaceTableInstance = StdGlobalInterfaceTable_Construct();
    hres = IGlobalInterfaceTable_QueryInterface( (IGlobalInterfaceTable*) StdGlobalInterfaceTableInstance, iid, ppv);
    if (hres) return hres;

    TRACE("Retrieved GIT (%p)\n", *ppv);
    return S_OK;
  }

  /*
   * Get a class factory to construct the object we want.
   */
  hres = CoGetClassObject(rclsid,
			  dwClsContext,
			  NULL,
			  &IID_IClassFactory,
			  (LPVOID)&lpclf);

  if (FAILED(hres))
    return hres;

  /*
   * Create the object and don't forget to release the factory
   */
	hres = IClassFactory_CreateInstance(lpclf, pUnkOuter, iid, ppv);
	IClassFactory_Release(lpclf);
	if(FAILED(hres))
	  FIXME("no instance created for interface %s of class %s, hres is 0x%08lx\n",
		debugstr_guid(iid), debugstr_guid(rclsid),hres);

	return hres;
}

/***********************************************************************
 *           CoCreateInstanceEx [OLE32.@]
 */
HRESULT WINAPI CoCreateInstanceEx(
  REFCLSID      rclsid,
  LPUNKNOWN     pUnkOuter,
  DWORD         dwClsContext,
  COSERVERINFO* pServerInfo,
  ULONG         cmq,
  MULTI_QI*     pResults)
{
  IUnknown* pUnk = NULL;
  HRESULT   hr;
  ULONG     index;
  ULONG     successCount = 0;

  /*
   * Sanity check
   */
  if ( (cmq==0) || (pResults==NULL))
    return E_INVALIDARG;

  if (pServerInfo!=NULL)
    FIXME("() non-NULL pServerInfo not supported!\n");

  /*
   * Initialize all the "out" parameters.
   */
  for (index = 0; index < cmq; index++)
  {
    pResults[index].pItf = NULL;
    pResults[index].hr   = E_NOINTERFACE;
  }

  /*
   * Get the object and get its IUnknown pointer.
   */
  hr = CoCreateInstance(rclsid,
			pUnkOuter,
			dwClsContext,
			&IID_IUnknown,
			(VOID**)&pUnk);

  if (hr)
    return hr;

  /*
   * Then, query for all the interfaces requested.
   */
  for (index = 0; index < cmq; index++)
  {
    pResults[index].hr = IUnknown_QueryInterface(pUnk,
						 pResults[index].pIID,
						 (VOID**)&(pResults[index].pItf));

    if (pResults[index].hr == S_OK)
      successCount++;
  }

  /*
   * Release our temporary unknown pointer.
   */
  IUnknown_Release(pUnk);

  if (successCount == 0)
    return E_NOINTERFACE;

  if (successCount!=cmq)
    return CO_S_NOTALLINTERFACES;

  return S_OK;
}

/***********************************************************************
 *           CoLoadLibrary (OLE32.@)
 *
 * Loads a library.
 *
 * PARAMS
 *  lpszLibName [I] Path to library.
 *  bAutoFree   [I] Whether the library should automatically be freed.
 *
 * RETURNS
 *  Success: Handle to loaded library.
 *  Failure: NULL.
 *
 * SEE ALSO
 *  CoFreeLibrary, CoFreeAllLibraries, CoFreeUnusedLibraries
 */
HINSTANCE WINAPI CoLoadLibrary(LPOLESTR lpszLibName, BOOL bAutoFree)
{
    TRACE("(%s, %d)\n", debugstr_w(lpszLibName), bAutoFree);

    return LoadLibraryExW(lpszLibName, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
}

/***********************************************************************
 *           CoFreeLibrary [OLE32.@]
 *
 * Unloads a library from memory.
 *
 * PARAMS
 *  hLibrary [I] Handle to library to unload.
 *
 * RETURNS
 *  Nothing
 *
 * SEE ALSO
 *  CoLoadLibrary, CoFreeAllLibraries, CoFreeUnusedLibraries
 */
void WINAPI CoFreeLibrary(HINSTANCE hLibrary)
{
    FreeLibrary(hLibrary);
}


/***********************************************************************
 *           CoFreeAllLibraries [OLE32.@]
 *
 * Function for backwards compatibility only. Does nothing.
 *
 * RETURNS
 *  Nothing.
 *
 * SEE ALSO
 *  CoLoadLibrary, CoFreeLibrary, CoFreeUnusedLibraries
 */
void WINAPI CoFreeAllLibraries(void)
{
    /* NOP */
}


/***********************************************************************
 *           CoFreeUnusedLibraries [OLE32.@]
 *           CoFreeUnusedLibraries [COMPOBJ.17]
 *
 * Frees any unused libraries. Unused are identified as those that return
 * S_OK from their DllCanUnloadNow function.
 *
 * RETURNS
 *  Nothing.
 *
 * SEE ALSO
 *  CoLoadLibrary, CoFreeAllLibraries, CoFreeLibrary
 */
void WINAPI CoFreeUnusedLibraries(void)
{
    /* FIXME: Calls to CoFreeUnusedLibraries from any thread always route
     * through the main apartment's thread to call DllCanUnloadNow */
    COMPOBJ_DllList_FreeUnused(0);
}

/***********************************************************************
 *           CoFileTimeNow [OLE32.@]
 *           CoFileTimeNow [COMPOBJ.82]
 *
 * Retrieves the current time in FILETIME format.
 *
 * PARAMS
 *  lpFileTime [O] The current time.
 *
 * RETURNS
 *	S_OK.
 */
HRESULT WINAPI CoFileTimeNow( FILETIME *lpFileTime )
{
    GetSystemTimeAsFileTime( lpFileTime );
    return S_OK;
}

static void COM_RevokeAllClasses()
{
  EnterCriticalSection( &csRegisteredClassList );

  while (firstRegisteredClass!=0)
  {
    CoRevokeClassObject(firstRegisteredClass->dwCookie);
  }

  LeaveCriticalSection( &csRegisteredClassList );
}

/******************************************************************************
 *		CoLockObjectExternal	[OLE32.@]
 *
 * Increments or decrements the external reference count of a stub object.
 *
 * PARAMS
 *  pUnk                [I] Stub object.
 *  fLock               [I] If TRUE then increments the external ref-count,
 *                          otherwise decrements.
 *  fLastUnlockReleases [I] If TRUE then the last unlock has the effect of
 *                          calling CoDisconnectObject.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * NOTES
 *  If fLock is TRUE and an object is passed in that doesn't have a stub
 *  manager then a new stub manager is created for the object.
 */
HRESULT WINAPI CoLockObjectExternal(
    LPUNKNOWN pUnk,
    BOOL fLock,
    BOOL fLastUnlockReleases)
{
    struct stub_manager *stubmgr;
    struct apartment *apt;

    TRACE("pUnk=%p, fLock=%s, fLastUnlockReleases=%s\n",
          pUnk, fLock ? "TRUE" : "FALSE", fLastUnlockReleases ? "TRUE" : "FALSE");

    apt = COM_CurrentApt();
    if (!apt) return CO_E_NOTINITIALIZED;

    stubmgr = get_stub_manager_from_object(apt, pUnk);
    
    if (stubmgr)
    {
        if (fLock)
            stub_manager_ext_addref(stubmgr, 1);
        else
            stub_manager_ext_release(stubmgr, 1, fLastUnlockReleases);
        
        stub_manager_int_release(stubmgr);

        return S_OK;
    }
    else if (fLock)
    {
        stubmgr = new_stub_manager(apt, pUnk);

        if (stubmgr)
        {
            stub_manager_ext_addref(stubmgr, 1);
            stub_manager_int_release(stubmgr);
        }

        return S_OK;
    }
    else
    {
        WARN("stub object not found %p\n", pUnk);
        /* Note: native is pretty broken here because it just silently
         * fails, without returning an appropriate error code, making apps
         * think that the object was disconnected, when it actually wasn't */
        return S_OK;
    }
}

/***********************************************************************
 *           CoInitializeWOW (OLE32.@)
 *
 * WOW equivalent of CoInitialize?
 *
 * PARAMS
 *  x [I] Unknown.
 *  y [I] Unknown.
 *
 * RETURNS
 *  Unknown.
 */
HRESULT WINAPI CoInitializeWOW(DWORD x,DWORD y)
{
    FIXME("(0x%08lx,0x%08lx),stub!\n",x,y);
    return 0;
}

/***********************************************************************
 *           CoGetState [OLE32.@]
 *
 * Retrieves the thread state object previously stored by CoSetState().
 *
 * PARAMS
 *  ppv [I] Address where pointer to object will be stored.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: E_OUTOFMEMORY.
 *
 * NOTES
 *  Crashes on all invalid ppv addresses, including NULL.
 *  If the function returns a non-NULL object then the caller must release its
 *  reference on the object when the object is no longer required.
 *
 * SEE ALSO
 *  CoSetState().
 */
HRESULT WINAPI CoGetState(IUnknown ** ppv)
{
    struct oletls *info = COM_CurrentInfo();
    if (!info) return E_OUTOFMEMORY;

    *ppv = NULL;

    if (info->state)
    {
        IUnknown_AddRef(info->state);
        *ppv = info->state;
        TRACE("apt->state=%p\n", info->state);
    }

    return S_OK;
}

/***********************************************************************
 *           CoSetState [OLE32.@]
 *
 * Sets the thread state object.
 *
 * PARAMS
 *  pv [I] Pointer to state object to be stored.
 *
 * NOTES
 *  The system keeps a reference on the object while the object stored.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: E_OUTOFMEMORY.
 */
HRESULT WINAPI CoSetState(IUnknown * pv)
{
    struct oletls *info = COM_CurrentInfo();
    if (!info) return E_OUTOFMEMORY;

    if (pv) IUnknown_AddRef(pv);

    if (info->state)
    {
        TRACE("-- release %p now\n", info->state);
        IUnknown_Release(info->state);
    }

    info->state = pv;

    return S_OK;
}


/******************************************************************************
 *              OleGetAutoConvert        [OLE32.@]
 */
HRESULT WINAPI OleGetAutoConvert(REFCLSID clsidOld, LPCLSID pClsidNew)
{
    static const WCHAR wszAutoConvertTo[] = {'A','u','t','o','C','o','n','v','e','r','t','T','o',0};
    HKEY hkey = NULL;
    WCHAR buf[CHARS_IN_GUID];
    LONG len;
    HRESULT res = S_OK;

    res = COM_OpenKeyForCLSID(clsidOld, wszAutoConvertTo, KEY_READ, &hkey);
    if (FAILED(res))
        goto done;

    len = sizeof(buf);
    if (RegQueryValueW(hkey, NULL, buf, &len))
    {
        res = REGDB_E_KEYMISSING;
        goto done;
    }
    res = CLSIDFromString(buf, pClsidNew);
done:
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************************
 *              CoTreatAsClass        [OLE32.@]
 *
 * Sets the TreatAs value of a class.
 *
 * PARAMS
 *  clsidOld [I] Class to set TreatAs value on.
 *  clsidNew [I] The class the clsidOld should be treated as.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoGetTreatAsClass
 */
HRESULT WINAPI CoTreatAsClass(REFCLSID clsidOld, REFCLSID clsidNew)
{
    static const WCHAR wszAutoTreatAs[] = {'A','u','t','o','T','r','e','a','t','A','s',0};
    static const WCHAR wszTreatAs[] = {'T','r','e','a','t','A','s',0};
    HKEY hkey = NULL;
    WCHAR szClsidNew[CHARS_IN_GUID];
    HRESULT res = S_OK;
    WCHAR auto_treat_as[CHARS_IN_GUID];
    LONG auto_treat_as_size = sizeof(auto_treat_as);
    CLSID id;

    res = COM_OpenKeyForCLSID(clsidOld, NULL, KEY_READ | KEY_WRITE, &hkey);
    if (FAILED(res))
        goto done;
    if (!memcmp( clsidOld, clsidNew, sizeof(*clsidOld) ))
    {
       if (!RegQueryValueW(hkey, wszAutoTreatAs, auto_treat_as, &auto_treat_as_size) &&
           !CLSIDFromString(auto_treat_as, &id))
       {
           if (RegSetValueW(hkey, wszTreatAs, REG_SZ, auto_treat_as, sizeof(auto_treat_as)))
           {
               res = REGDB_E_WRITEREGDB;
               goto done;
           }
       }
       else
       {
           RegDeleteKeyW(hkey, wszTreatAs);
           goto done;
       }
    }
    else if (!StringFromGUID2(clsidNew, szClsidNew, ARRAYSIZE(szClsidNew)) &&
             !RegSetValueW(hkey, wszTreatAs, REG_SZ, szClsidNew, sizeof(szClsidNew)))
    {
        res = REGDB_E_WRITEREGDB;
	goto done;
    }

done:
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************************
 *              CoGetTreatAsClass        [OLE32.@]
 *
 * Gets the TreatAs value of a class.
 *
 * PARAMS
 *  clsidOld [I] Class to get the TreatAs value of.
 *  clsidNew [I] The class the clsidOld should be treated as.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoSetTreatAsClass
 */
HRESULT WINAPI CoGetTreatAsClass(REFCLSID clsidOld, LPCLSID clsidNew)
{
    static const WCHAR wszTreatAs[] = {'T','r','e','a','t','A','s',0};
    HKEY hkey = NULL;
    WCHAR szClsidNew[CHARS_IN_GUID];
    HRESULT res = S_OK;
    LONG len = sizeof(szClsidNew);

    FIXME("(%s,%p)\n", debugstr_guid(clsidOld), clsidNew);
    memcpy(clsidNew,clsidOld,sizeof(CLSID)); /* copy over old value */

    res = COM_OpenKeyForCLSID(clsidOld, wszTreatAs, KEY_READ, &hkey);
    if (FAILED(res))
        goto done;
    if (RegQueryValueW(hkey, NULL, szClsidNew, &len))
    {
        res = S_FALSE;
	goto done;
    }
    res = CLSIDFromString(szClsidNew,clsidNew);
    if (FAILED(res))
        ERR("Failed CLSIDFromStringA(%s), hres 0x%08lx\n", debugstr_w(szClsidNew), res);
done:
    if (hkey) RegCloseKey(hkey);
    return res;
}

/******************************************************************************
 *		CoGetCurrentProcess	[OLE32.@]
 *		CoGetCurrentProcess	[COMPOBJ.34]
 *
 * Gets the current process ID.
 *
 * RETURNS
 *  The current process ID.
 *
 * NOTES
 *   Is DWORD really the correct return type for this function?
 */
DWORD WINAPI CoGetCurrentProcess(void)
{
	return GetCurrentProcessId();
}

/******************************************************************************
 *		CoRegisterMessageFilter	[OLE32.@]
 *
 * Registers a message filter.
 *
 * PARAMS
 *  lpMessageFilter [I] Pointer to interface.
 *  lplpMessageFilter [O] Indirect pointer to prior instance if non-NULL.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * NOTES
 *  Both lpMessageFilter and lplpMessageFilter are optional. Passing in a NULL
 *  lpMessageFilter removes the message filter.
 *
 *  If lplpMessageFilter is not NULL the previous message filter will be
 *  returned in the memory pointer to this parameter and the caller is
 *  responsible for releasing the object.
 *
 *  The current thread be in an apartment otherwise the function will crash.
 */
HRESULT WINAPI CoRegisterMessageFilter(
    LPMESSAGEFILTER lpMessageFilter,
    LPMESSAGEFILTER *lplpMessageFilter)
{
    struct apartment *apt;
    IMessageFilter *lpOldMessageFilter;

    TRACE("(%p, %p)\n", lpMessageFilter, lplpMessageFilter);

    apt = COM_CurrentApt();

    /* can't set a message filter in a multi-threaded apartment */
    if (apt->multi_threaded)
    {
        ERR("can't set message filter in MTA\n");
        return CO_E_NOT_SUPPORTED;
    }

    if (lpMessageFilter)
        IMessageFilter_AddRef(lpMessageFilter);

    EnterCriticalSection(&apt->cs);

    lpOldMessageFilter = apt->filter;
    apt->filter = lpMessageFilter;

    LeaveCriticalSection(&apt->cs);

    if (lplpMessageFilter)
        *lplpMessageFilter = lpOldMessageFilter;
    else if (lpOldMessageFilter)
        IMessageFilter_Release(lpOldMessageFilter);

    if (lpMessageFilter)
        FIXME("message filter has been registered, but will not be used\n");

    return S_OK;
}

/***********************************************************************
 *           CoIsOle1Class [OLE32.@]
 *
 * Determines whether the specified class an OLE v1 class.
 *
 * PARAMS
 *  clsid [I] Class to test.
 *
 * RETURNS
 *  TRUE if the class is an OLE v1 class, or FALSE otherwise.
 */
BOOL WINAPI CoIsOle1Class(REFCLSID clsid)
{
  FIXME("%s\n", debugstr_guid(clsid));
  return FALSE;
}

/***********************************************************************
 *           IsEqualGUID [OLE32.@]
 *
 * Compares two Unique Identifiers.
 *
 * PARAMS
 *  rguid1 [I] The first GUID to compare.
 *  rguid2 [I] The other GUID to compare.
 *
 * RETURNS
 *	TRUE if equal
 */
#undef IsEqualGUID
BOOL WINAPI IsEqualGUID(
     REFGUID rguid1,
     REFGUID rguid2)
{
    return !memcmp(rguid1,rguid2,sizeof(GUID));
}

/***********************************************************************
 *           CoInitializeSecurity [OLE32.@]
 */
HRESULT WINAPI CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
                                    SOLE_AUTHENTICATION_SERVICE* asAuthSvc,
                                    void* pReserved1, DWORD dwAuthnLevel,
                                    DWORD dwImpLevel, void* pReserved2,
                                    DWORD dwCapabilities, void* pReserved3)
{
  FIXME("(%p,%ld,%p,%p,%ld,%ld,%p,%ld,%p) - stub!\n", pSecDesc, cAuthSvc,
        asAuthSvc, pReserved1, dwAuthnLevel, dwImpLevel, pReserved2,
        dwCapabilities, pReserved3);
  return S_OK;
}

/***********************************************************************
 *           CoSuspendClassObjects [OLE32.@]
 *
 * Suspends all registered class objects to prevent further requests coming in
 * for those objects.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 */
HRESULT WINAPI CoSuspendClassObjects(void)
{
    FIXME("\n");
    return S_OK;
}

/***********************************************************************
 *           CoAddRefServerProcess [OLE32.@]
 *
 * Helper function for incrementing the reference count of a local-server
 * process.
 *
 * RETURNS
 *  New reference count.
 */
ULONG WINAPI CoAddRefServerProcess(void)
{
    FIXME("\n");
    return 2;
}

/***********************************************************************
 *           CoReleaseServerProcess [OLE32.@]
 *
 * Helper function for decrementing the reference count of a local-server
 * process.
 *
 * RETURNS
 *  New reference count.
 */
ULONG WINAPI CoReleaseServerProcess(void)
{
    FIXME("\n");
    return 1;
}

/***********************************************************************
 *           CoIsHandlerConnected [OLE32.@]
 *
 * Determines whether a proxy is connected to a remote stub.
 *
 * PARAMS
 *  pUnk [I] Pointer to object that may or may not be connected.
 *
 * RETURNS
 *  TRUE if pUnk is not a proxy or if pUnk is connected to a remote stub, or
 *  FALSE otherwise.
 */
BOOL WINAPI CoIsHandlerConnected(IUnknown *pUnk)
{
    FIXME("%p\n", pUnk);

    return TRUE;
}

/***********************************************************************
 *           CoAllowSetForegroundWindow [OLE32.@]
 *
 */
HRESULT WINAPI CoAllowSetForegroundWindow(IUnknown *pUnk, void *pvReserved)
{
    FIXME("(%p, %p): stub\n", pUnk, pvReserved);
    return S_OK;
}
 
/***********************************************************************
 *           CoQueryProxyBlanket [OLE32.@]
 *
 * Retrieves the security settings being used by a proxy.
 *
 * PARAMS
 *  pProxy        [I] Pointer to the proxy object.
 *  pAuthnSvc     [O] The type of authentication service.
 *  pAuthzSvc     [O] The type of authorization service.
 *  ppServerPrincName [O] Optional. The server prinicple name.
 *  pAuthnLevel   [O] The authentication level.
 *  pImpLevel     [O] The impersonation level.
 *  ppAuthInfo    [O] Information specific to the authorization/authentication service.
 *  pCapabilities [O] Flags affecting the security behaviour.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoCopyProxy, CoSetProxyBlanket.
 */
HRESULT WINAPI CoQueryProxyBlanket(IUnknown *pProxy, DWORD *pAuthnSvc,
    DWORD *pAuthzSvc, OLECHAR **ppServerPrincName, DWORD *pAuthnLevel,
    DWORD *pImpLevel, void **ppAuthInfo, DWORD *pCapabilities)
{
    IClientSecurity *pCliSec;
    HRESULT hr;

    TRACE("%p\n", pProxy);

    hr = IUnknown_QueryInterface(pProxy, &IID_IClientSecurity, (void **)&pCliSec);
    if (SUCCEEDED(hr))
    {
        hr = IClientSecurity_QueryBlanket(pCliSec, pProxy, pAuthnSvc,
                                          pAuthzSvc, ppServerPrincName,
                                          pAuthnLevel, pImpLevel, ppAuthInfo,
                                          pCapabilities);
        IClientSecurity_Release(pCliSec);
    }

    if (FAILED(hr)) ERR("-- failed with 0x%08lx\n", hr);
    return hr;
}

/***********************************************************************
 *           CoSetProxyBlanket [OLE32.@]
 *
 * Sets the security settings for a proxy.
 *
 * PARAMS
 *  pProxy       [I] Pointer to the proxy object.
 *  AuthnSvc     [I] The type of authentication service.
 *  AuthzSvc     [I] The type of authorization service.
 *  pServerPrincName [I] The server prinicple name.
 *  AuthnLevel   [I] The authentication level.
 *  ImpLevel     [I] The impersonation level.
 *  pAuthInfo    [I] Information specific to the authorization/authentication service.
 *  Capabilities [I] Flags affecting the security behaviour.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoQueryProxyBlanket, CoCopyProxy.
 */
HRESULT WINAPI CoSetProxyBlanket(IUnknown *pProxy, DWORD AuthnSvc,
    DWORD AuthzSvc, OLECHAR *pServerPrincName, DWORD AuthnLevel,
    DWORD ImpLevel, void *pAuthInfo, DWORD Capabilities)
{
    IClientSecurity *pCliSec;
    HRESULT hr;

    TRACE("%p\n", pProxy);

    hr = IUnknown_QueryInterface(pProxy, &IID_IClientSecurity, (void **)&pCliSec);
    if (SUCCEEDED(hr))
    {
        hr = IClientSecurity_SetBlanket(pCliSec, pProxy, AuthnSvc,
                                        AuthzSvc, pServerPrincName,
                                        AuthnLevel, ImpLevel, pAuthInfo,
                                        Capabilities);
        IClientSecurity_Release(pCliSec);
    }

    if (FAILED(hr)) ERR("-- failed with 0x%08lx\n", hr);
    return hr;
}

/***********************************************************************
 *           CoCopyProxy [OLE32.@]
 *
 * Copies a proxy.
 *
 * PARAMS
 *  pProxy [I] Pointer to the proxy object.
 *  ppCopy [O] Copy of the proxy.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoQueryProxyBlanket, CoSetProxyBlanket.
 */
HRESULT WINAPI CoCopyProxy(IUnknown *pProxy, IUnknown **ppCopy)
{
    IClientSecurity *pCliSec;
    HRESULT hr;

    TRACE("%p\n", pProxy);

    hr = IUnknown_QueryInterface(pProxy, &IID_IClientSecurity, (void **)&pCliSec);
    if (SUCCEEDED(hr))
    {
        hr = IClientSecurity_CopyProxy(pCliSec, pProxy, ppCopy);
        IClientSecurity_Release(pCliSec);
    }

    if (FAILED(hr)) ERR("-- failed with 0x%08lx\n", hr);
    return hr;
}


/***********************************************************************
 *           CoGetCallContext [OLE32.@]
 *
 * Gets the context of the currently executing server call in the current
 * thread.
 *
 * PARAMS
 *  riid [I] Context interface to return.
 *  ppv  [O] Pointer to memory that will receive the context on return.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 */
HRESULT WINAPI CoGetCallContext(REFIID riid, void **ppv)
{
    FIXME("(%s, %p): stub\n", debugstr_guid(riid), ppv);

    *ppv = NULL;
    return E_NOINTERFACE;
}

/***********************************************************************
 *           CoQueryClientBlanket [OLE32.@]
 *
 * Retrieves the authentication information about the client of the currently
 * executing server call in the current thread.
 *
 * PARAMS
 *  pAuthnSvc     [O] Optional. The type of authentication service.
 *  pAuthzSvc     [O] Optional. The type of authorization service.
 *  pServerPrincName [O] Optional. The server prinicple name.
 *  pAuthnLevel   [O] Optional. The authentication level.
 *  pImpLevel     [O] Optional. The impersonation level.
 *  pPrivs        [O] Optional. Information about the privileges of the client.
 *  pCapabilities [IO] Optional. Flags affecting the security behaviour.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoImpersonateClient, CoRevertToSelf, CoGetCallContext.
 */
HRESULT WINAPI CoQueryClientBlanket(
    DWORD *pAuthnSvc,
    DWORD *pAuthzSvc,
    OLECHAR **pServerPrincName,
    DWORD *pAuthnLevel,
    DWORD *pImpLevel,
    RPC_AUTHZ_HANDLE *pPrivs,
    DWORD *pCapabilities)
{
    IServerSecurity *pSrvSec;
    HRESULT hr;

    TRACE("(%p, %p, %p, %p, %p, %p, %p)\n",
        pAuthnSvc, pAuthzSvc, pServerPrincName, pAuthnLevel, pImpLevel,
        pPrivs, pCapabilities);

    hr = CoGetCallContext(&IID_IServerSecurity, (void **)&pSrvSec);
    if (SUCCEEDED(hr))
    {
        hr = IServerSecurity_QueryBlanket(
            pSrvSec, pAuthnSvc, pAuthzSvc, pServerPrincName, pAuthnLevel,
            pImpLevel, pPrivs, pCapabilities);
        IServerSecurity_Release(pSrvSec);
    }

    return hr;
}

/***********************************************************************
 *           CoImpersonateClient [OLE32.@]
 *
 * Impersonates the client of the currently executing server call in the
 * current thread.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * NOTES
 *  If this function fails then the current thread will not be impersonating
 *  the client and all actions will take place on behalf of the server.
 *  Therefore, it is important to check the return value from this function.
 *
 * SEE ALSO
 *  CoRevertToSelf, CoQueryClientBlanket, CoGetCallContext.
 */
HRESULT WINAPI CoImpersonateClient(void)
{
    IServerSecurity *pSrvSec;
    HRESULT hr;

    TRACE("\n");

    hr = CoGetCallContext(&IID_IServerSecurity, (void **)&pSrvSec);
    if (SUCCEEDED(hr))
    {
        hr = IServerSecurity_ImpersonateClient(pSrvSec);
        IServerSecurity_Release(pSrvSec);
    }

    return hr;
}

/***********************************************************************
 *           CoRevertToSelf [OLE32.@]
 *
 * Ends the impersonation of the client of the currently executing server
 * call in the current thread.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  CoImpersonateClient, CoQueryClientBlanket, CoGetCallContext.
 */
HRESULT WINAPI CoRevertToSelf(void)
{
    IServerSecurity *pSrvSec;
    HRESULT hr;

    TRACE("\n");

    hr = CoGetCallContext(&IID_IServerSecurity, (void **)&pSrvSec);
    if (SUCCEEDED(hr))
    {
        hr = IServerSecurity_RevertToSelf(pSrvSec);
        IServerSecurity_Release(pSrvSec);
    }

    return hr;
}

static BOOL COM_PeekMessage(struct apartment *apt, MSG *msg)
{
    /* first try to retrieve messages for incoming COM calls to the apartment window */
    return PeekMessageW(msg, apt->win, WM_USER, WM_APP - 1, PM_REMOVE|PM_NOYIELD) ||
           /* next retrieve other messages necessary for the app to remain responsive */
           PeekMessageW(msg, NULL, 0, WM_USER - 1, PM_REMOVE|PM_NOYIELD);
}

/***********************************************************************
 *           CoWaitForMultipleHandles [OLE32.@]
 *
 * Waits for one or more handles to become signaled.
 *
 * PARAMS
 *  dwFlags   [I] Flags. See notes.
 *  dwTimeout [I] Timeout in milliseconds.
 *  cHandles  [I] Number of handles pointed to by pHandles.
 *  pHandles  [I] Handles to wait for.
 *  lpdwindex [O] Index of handle that was signaled.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: RPC_S_CALLPENDING on timeout.
 *
 * NOTES
 *
 * The dwFlags parameter can be zero or more of the following:
 *| COWAIT_WAITALL - Wait for all of the handles to become signaled.
 *| COWAIT_ALERTABLE - Allows a queued APC to run during the wait.
 *
 * SEE ALSO
 *  MsgWaitForMultipleObjects, WaitForMultipleObjects.
 */
HRESULT WINAPI CoWaitForMultipleHandles(DWORD dwFlags, DWORD dwTimeout,
    ULONG cHandles, const HANDLE* pHandles, LPDWORD lpdwindex)
{
    HRESULT hr = S_OK;
    DWORD start_time = GetTickCount();
    APARTMENT *apt = COM_CurrentApt();
    BOOL message_loop = apt && !apt->multi_threaded;

    TRACE("(0x%08lx, 0x%08lx, %ld, %p, %p)\n", dwFlags, dwTimeout, cHandles,
        pHandles, lpdwindex);

    while (TRUE)
    {
        DWORD now = GetTickCount();
        DWORD res;

        if ((dwTimeout != INFINITE) && (start_time + dwTimeout >= now))
        {
            hr = RPC_S_CALLPENDING;
            break;
        }

        if (message_loop)
        {
            DWORD wait_flags = (dwFlags & COWAIT_WAITALL) ? MWMO_WAITALL : 0 |
                    (dwFlags & COWAIT_ALERTABLE ) ? MWMO_ALERTABLE : 0;

            TRACE("waiting for rpc completion or window message\n");

            res = MsgWaitForMultipleObjectsEx(cHandles, pHandles,
                (dwTimeout == INFINITE) ? INFINITE : start_time + dwTimeout - now,
                QS_ALLINPUT, wait_flags);

            if (res == WAIT_OBJECT_0 + cHandles)  /* messages available */
            {
                MSG msg;

                /* note: using "if" here instead of "while" might seem less
                 * efficient, but only if we are optimising for quick delivery
                 * of pending messages, rather than quick completion of the
                 * COM call */
                if (COM_PeekMessage(apt, &msg))
                {
                    /* FIXME: filter the messages here */
                    TRACE("received message whilst waiting for RPC: 0x%04x\n", msg.message);
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                    if (msg.message == WM_QUIT)
                    {
                        TRACE("resending WM_QUIT to outer message loop\n");
                        PostQuitMessage(msg.wParam);
                        /* no longer need to process messages */
                        message_loop = FALSE;
                    }
                }
                continue;
            }
        }
        else
        {
            TRACE("waiting for rpc completion\n");

            res = WaitForMultipleObjectsEx(cHandles, pHandles,
                (dwFlags & COWAIT_WAITALL) ? TRUE : FALSE,
                (dwTimeout == INFINITE) ? INFINITE : start_time + dwTimeout - now,
                (dwFlags & COWAIT_ALERTABLE) ? TRUE : FALSE);
        }

        if ((res >= WAIT_OBJECT_0) && (res < WAIT_OBJECT_0 + cHandles))
        {
            /* handle signaled, store index */
            *lpdwindex = (res - WAIT_OBJECT_0);
            break;
        }
        else if (res == WAIT_TIMEOUT)
        {
            hr = RPC_S_CALLPENDING;
            break;
        }
        else
        {
            ERR("Unexpected wait termination: %ld, %ld\n", res, GetLastError());
            hr = E_UNEXPECTED;
            break;
        }
    }
    TRACE("-- 0x%08lx\n", hr);
    return hr;
}


/***********************************************************************
 *           CoGetObject [OLE32.@]
 *
 * Gets the object named by coverting the name to a moniker and binding to it.
 *
 * PARAMS
 *  pszName      [I] String representing the object.
 *  pBindOptions [I] Parameters affecting the binding to the named object.
 *  riid         [I] Interface to bind to on the objecct.
 *  ppv          [O] On output, the interface riid of the object represented
 *                   by pszName.
 *
 * RETURNS
 *  Success: S_OK.
 *  Failure: HRESULT code.
 *
 * SEE ALSO
 *  MkParseDisplayName.
 */
HRESULT WINAPI CoGetObject(LPCWSTR pszName, BIND_OPTS *pBindOptions,
    REFIID riid, void **ppv)
{
    IBindCtx *pbc;
    HRESULT hr;

    *ppv = NULL;

    hr = CreateBindCtx(0, &pbc);
    if (SUCCEEDED(hr))
    {
        if (pBindOptions)
            hr = IBindCtx_SetBindOptions(pbc, pBindOptions);

        if (SUCCEEDED(hr))
        {
            ULONG chEaten;
            IMoniker *pmk;

            hr = MkParseDisplayName(pbc, pszName, &chEaten, &pmk);
            if (SUCCEEDED(hr))
            {
                hr = IMoniker_BindToObject(pmk, pbc, NULL, riid, ppv);
                IMoniker_Release(pmk);
            }
        }

        IBindCtx_Release(pbc);
    }
    return hr;
}

/***********************************************************************
 *		DllMain (OLE32.@)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
    TRACE("%p 0x%lx %p\n", hinstDLL, fdwReason, fImpLoad);

    switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
        OLE32_hInstance = hinstDLL;
        COMPOBJ_InitProcess();
	if (TRACE_ON(ole)) CoRegisterMallocSpy((LPVOID)-1);
	break;

    case DLL_PROCESS_DETACH:
        if (TRACE_ON(ole)) CoRevokeMallocSpy();
        COMPOBJ_UninitProcess();
        OLE32_hInstance = 0;
	break;

    case DLL_THREAD_DETACH:
        COM_TlsDestroy();
        break;
    }
    return TRUE;
}

/* NOTE: DllRegisterServer and DllUnregisterServer are in regsvr.c */
