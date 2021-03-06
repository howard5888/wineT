/*		DirectInput Keyboard device
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998,1999 Lionel Ulmer
 * Copyright 2000-2001 TransGaming Technologies Inc.
 * Copyright 2005 Raphael Junqueira
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
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "dinput.h"

#include "dinput_private.h"
#include "device_private.h"
#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

#define WINE_DINPUT_KEYBOARD_MAX_KEYS 256

static const IDirectInputDevice8AVtbl SysKeyboardAvt;
static const IDirectInputDevice8WVtbl SysKeyboardWvt;

typedef struct SysKeyboardImpl SysKeyboardImpl;
struct SysKeyboardImpl
{
    const void                 *lpVtbl;
    LONG                        ref;
    GUID                        guid;

    IDirectInputImpl*           dinput;

    HANDLE                      hEvent;
    /* SysKeyboardAImpl */
    int                         acquired;

    LPDIDEVICEOBJECTDATA        data_queue;  /* buffer for 'GetDeviceData'. Alloc at
                                                'Acquire', Free at 'Unacquire'              */
    int                         queue_len;   /* size of the queue - set in 'SetProperty'    */
    int                         queue_head;  /* position to write new event into queue      */
    int                         queue_tail;  /* next event to read from queue               */
    BOOL                        overflow;    /* return DI_BUFFEROVERFLOW in 'GetDeviceData' */
    CRITICAL_SECTION            crit;
};

static SysKeyboardImpl* current_lock = NULL; 
/* Today's acquired device
 * FIXME: currently this can be only one.
 * Maybe this should be a linked list or st.
 * I don't know what the rules are for multiple acquired keyboards,
 * but 'DI_LOSTFOCUS' and 'DI_UNACQUIRED' exist for a reason.
*/

static BYTE DInputKeyState[WINE_DINPUT_KEYBOARD_MAX_KEYS]; /* array for 'GetDeviceState' */

LRESULT CALLBACK KeyboardCallback( int code, WPARAM wparam, LPARAM lparam )
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)current_lock;
    int dik_code;
    KBDLLHOOKSTRUCT *hook = (KBDLLHOOKSTRUCT *)lparam;
    BYTE new_diks;

    TRACE("(%d,%d,%ld)\n", code, wparam, lparam);

    /* returns now if not HC_ACTION */
    if (code != HC_ACTION) return CallNextHookEx(0, code, wparam, lparam);
  
    dik_code = hook->scanCode & 0xff;
    if (hook->flags & LLKHF_EXTENDED) dik_code |= 0x80;

    new_diks = hook->flags & LLKHF_UP ? 0 : 0x80;

    /* returns now if key event already known */
    if (new_diks == DInputKeyState[dik_code])
        return CallNextHookEx(0, code, wparam, lparam);

    DInputKeyState[dik_code] = new_diks;
    TRACE(" setting %02X to %02X\n", dik_code, DInputKeyState[dik_code]);
      
    EnterCriticalSection(&This->crit);
    GEN_EVENT(dik_code, new_diks, hook->time, This->dinput->evsequence++);
    LeaveCriticalSection(&This->crit);

    if (This->hEvent) SetEvent(This->hEvent);
    
    return CallNextHookEx(0, code, wparam, lparam);
}

static GUID DInput_Wine_Keyboard_GUID = { /* 0ab8648a-7735-11d2-8c73-71df54a96441 */
  0x0ab8648a,
  0x7735,
  0x11d2,
  {0x8c, 0x73, 0x71, 0xdf, 0x54, 0xa9, 0x64, 0x41}
};

static void fill_keyboard_dideviceinstanceA(LPDIDEVICEINSTANCEA lpddi, DWORD version) {
    DWORD dwSize;
    DIDEVICEINSTANCEA ddi;
    
    dwSize = lpddi->dwSize;

    TRACE("%ld %p\n", dwSize, lpddi);
    
    memset(lpddi, 0, dwSize);
    memset(&ddi, 0, sizeof(ddi));

    ddi.dwSize = dwSize;
    ddi.guidInstance = GUID_SysKeyboard;/* DInput's GUID */
    ddi.guidProduct = DInput_Wine_Keyboard_GUID; /* Vendor's GUID */
    if (version >= 0x0800)
        ddi.dwDevType = DI8DEVTYPE_KEYBOARD | (DI8DEVTYPEKEYBOARD_UNKNOWN << 8);
    else
        ddi.dwDevType = DIDEVTYPE_KEYBOARD | (DIDEVTYPEKEYBOARD_UNKNOWN << 8);
    strcpy(ddi.tszInstanceName, "Keyboard");
    strcpy(ddi.tszProductName, "Wine Keyboard");

    memcpy(lpddi, &ddi, (dwSize < sizeof(ddi) ? dwSize : sizeof(ddi)));
}

static void fill_keyboard_dideviceinstanceW(LPDIDEVICEINSTANCEW lpddi, DWORD version) {
    DWORD dwSize;
    DIDEVICEINSTANCEW ddi;
    
    dwSize = lpddi->dwSize;

    TRACE("%ld %p\n", dwSize, lpddi);
    
    memset(lpddi, 0, dwSize);
    memset(&ddi, 0, sizeof(ddi));
 
    ddi.dwSize = dwSize;
    ddi.guidInstance = GUID_SysKeyboard;/* DInput's GUID */
    ddi.guidProduct = DInput_Wine_Keyboard_GUID; /* Vendor's GUID */
    if (version >= 0x0800)
        ddi.dwDevType = DI8DEVTYPE_KEYBOARD | (DI8DEVTYPEKEYBOARD_UNKNOWN << 8);
    else
        ddi.dwDevType = DIDEVTYPE_KEYBOARD | (DIDEVTYPEKEYBOARD_UNKNOWN << 8);
    MultiByteToWideChar(CP_ACP, 0, "Keyboard", -1, ddi.tszInstanceName, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, "Wine Keyboard", -1, ddi.tszProductName, MAX_PATH);

    memcpy(lpddi, &ddi, (dwSize < sizeof(ddi) ? dwSize : sizeof(ddi)));
}
 
static BOOL keyboarddev_enum_deviceA(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEA lpddi, DWORD version, int id)
{
  if (id != 0)
    return FALSE;

  if ((dwDevType == 0) ||
      ((dwDevType == DIDEVTYPE_KEYBOARD) && (version < 0x0800)) ||
      (((dwDevType == DI8DEVCLASS_KEYBOARD) || (dwDevType == DI8DEVTYPE_KEYBOARD)) && (version >= 0x0800))) {
    TRACE("Enumerating the Keyboard device\n");
 
    fill_keyboard_dideviceinstanceA(lpddi, version);
    
    return TRUE;
  }

  return FALSE;
}

static BOOL keyboarddev_enum_deviceW(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEW lpddi, DWORD version, int id)
{
  if (id != 0)
    return FALSE;

  if ((dwDevType == 0) ||
      ((dwDevType == DIDEVTYPE_KEYBOARD) && (version < 0x0800)) ||
      (((dwDevType == DI8DEVCLASS_KEYBOARD) || (dwDevType == DI8DEVTYPE_KEYBOARD)) && (version >= 0x0800))) {
    TRACE("Enumerating the Keyboard device\n");

    fill_keyboard_dideviceinstanceW(lpddi, version);
    
    return TRUE;
  }

  return FALSE;
}

static SysKeyboardImpl *alloc_device(REFGUID rguid, const void *kvt, IDirectInputImpl *dinput)
{
    SysKeyboardImpl* newDevice;

    newDevice = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SysKeyboardImpl));
    newDevice->lpVtbl = kvt;
    newDevice->ref = 1;
    memcpy(&(newDevice->guid),rguid,sizeof(*rguid));
    newDevice->dinput = dinput;
    InitializeCriticalSection(&(newDevice->crit));

    return newDevice;
}


static HRESULT keyboarddev_create_deviceA(IDirectInputImpl *dinput, REFGUID rguid, REFIID riid, LPDIRECTINPUTDEVICEA* pdev)
{
  if ((IsEqualGUID(&GUID_SysKeyboard,rguid)) ||          /* Generic Keyboard */
      (IsEqualGUID(&DInput_Wine_Keyboard_GUID,rguid))) { /* Wine Keyboard */
    if ((riid == NULL) ||
	IsEqualGUID(&IID_IDirectInputDeviceA,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice2A,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice7A,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice8A,riid)) {
      *pdev = (IDirectInputDeviceA*) alloc_device(rguid, &SysKeyboardAvt, dinput);
      TRACE("Creating a Keyboard device (%p)\n", *pdev);
      return DI_OK;
    } else
      return DIERR_NOINTERFACE;
  }
  return DIERR_DEVICENOTREG;
}

static HRESULT keyboarddev_create_deviceW(IDirectInputImpl *dinput, REFGUID rguid, REFIID riid, LPDIRECTINPUTDEVICEW* pdev)
{
  if ((IsEqualGUID(&GUID_SysKeyboard,rguid)) ||          /* Generic Keyboard */
      (IsEqualGUID(&DInput_Wine_Keyboard_GUID,rguid))) { /* Wine Keyboard */
    if ((riid == NULL) ||
	IsEqualGUID(&IID_IDirectInputDeviceW,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice2W,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice7W,riid) ||
	IsEqualGUID(&IID_IDirectInputDevice8W,riid)) {
      *pdev = (IDirectInputDeviceW*) alloc_device(rguid, &SysKeyboardWvt, dinput);
      TRACE("Creating a Keyboard device (%p)\n", *pdev);
      return DI_OK;
    } else
      return DIERR_NOINTERFACE;
  }
  return DIERR_DEVICENOTREG;
}

const struct dinput_device keyboard_device = {
  "Wine keyboard driver",
  keyboarddev_enum_deviceA,
  keyboarddev_enum_deviceW,
  keyboarddev_create_deviceA,
  keyboarddev_create_deviceW
};

static ULONG WINAPI SysKeyboardAImpl_Release(LPDIRECTINPUTDEVICE8A iface)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    ULONG ref;

    ref = InterlockedDecrement(&(This->ref));
    if (ref) return ref;

    set_dinput_hook(WH_KEYBOARD_LL, NULL);

    /* Free the data queue */
    HeapFree(GetProcessHeap(), 0, This->data_queue);

    DeleteCriticalSection(&(This->crit));

    HeapFree(GetProcessHeap(),0,This);
    return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_SetProperty(
	LPDIRECTINPUTDEVICE8A iface,REFGUID rguid,LPCDIPROPHEADER ph
)
{
	SysKeyboardImpl *This = (SysKeyboardImpl *)iface;

	TRACE("(this=%p,%s,%p)\n",This,debugstr_guid(rguid),ph);
	TRACE("(size=%ld,headersize=%ld,obj=%ld,how=%ld\n",
            ph->dwSize,ph->dwHeaderSize,ph->dwObj,ph->dwHow);
	if (!HIWORD(rguid)) {
		switch (LOWORD(rguid)) {
		case (DWORD) DIPROP_BUFFERSIZE: {
			LPCDIPROPDWORD	pd = (LPCDIPROPDWORD)ph;

			TRACE("(buffersize=%ld)\n",pd->dwData);

                        if (This->acquired)
                           return DIERR_INVALIDPARAM;

                        This->queue_len = pd->dwData;

			break;
		}
		default:
			WARN("Unknown type %p\n",rguid);
			break;
		}
	}
	return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_GetProperty(
	LPDIRECTINPUTDEVICE8A iface,REFGUID rguid,LPDIPROPHEADER ph
)
{
	SysKeyboardImpl *This = (SysKeyboardImpl *)iface;

	TRACE("(this=%p,%s,%p)\n",This,debugstr_guid(rguid),ph);
	TRACE("(size=%ld,headersize=%ld,obj=%ld,how=%ld\n",
            ph->dwSize,ph->dwHeaderSize,ph->dwObj,ph->dwHow);
	if (!HIWORD(rguid)) {
		switch (LOWORD(rguid)) {
		case (DWORD) DIPROP_BUFFERSIZE: {
			LPDIPROPDWORD	pd = (LPDIPROPDWORD)ph;

			TRACE("(buffersize=%ld)\n",pd->dwData);

                        if (This->acquired)
                           return DIERR_INVALIDPARAM;

                        pd->dwData = This->queue_len;

			break;
		}
		default:
			WARN("Unknown type %p\n",rguid);
			break;
		}
	}
	return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_GetDeviceState(
	LPDIRECTINPUTDEVICE8A iface,DWORD len,LPVOID ptr
)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    TRACE("(%p)->(%ld,%p)\n", This, len, ptr);

    if (This->acquired == 0) return DIERR_NOTACQUIRED;

    if (len != WINE_DINPUT_KEYBOARD_MAX_KEYS)
      return DIERR_INVALIDPARAM;

    MsgWaitForMultipleObjectsEx(0, NULL, 0, QS_ALLINPUT, 0);

    EnterCriticalSection(&(This->crit));

    if (TRACE_ON(dinput)) {
	int i;
	for (i = 0; i < WINE_DINPUT_KEYBOARD_MAX_KEYS; i++) {
	    if (DInputKeyState[i] != 0x00) {
		TRACE(" - %02X: %02x\n", i, DInputKeyState[i]);
	    }
	}
    }
    
    memcpy(ptr, DInputKeyState, WINE_DINPUT_KEYBOARD_MAX_KEYS);
    LeaveCriticalSection(&(This->crit));

    return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_GetDeviceData(
	LPDIRECTINPUTDEVICE8A iface,DWORD dodsize,LPDIDEVICEOBJECTDATA dod,
	LPDWORD entries,DWORD flags
)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    HRESULT ret = DI_OK;
    int len;

    TRACE("(%p) %p -> %p(%ld) x%ld, 0x%08lx\n",
          This, dod, entries, entries ? *entries : 0, dodsize, flags);

    if (!This->acquired)
        return DIERR_NOTACQUIRED;

    if (!This->data_queue)
        return DIERR_NOTBUFFERED;

    if (dodsize < sizeof(DIDEVICEOBJECTDATA_DX3))
        return DIERR_INVALIDPARAM;

    MsgWaitForMultipleObjectsEx(0, NULL, 0, QS_ALLINPUT, 0);
    EnterCriticalSection(&This->crit);

    len = ((This->queue_head < This->queue_tail) ? This->queue_len : 0) +
            This->queue_head - This->queue_tail;
    if ((*entries != INFINITE) && (len > *entries)) len = *entries;

    if (dod)
    {
        int i;
        for(i = 0; i < len; i++)
        {
            int n = (This->queue_tail + i) % This->queue_len;
            memcpy((char *)dod + dodsize * i, This->data_queue + n, dodsize);
        }
    }
    *entries = len;

    if (This->overflow)
        ret = DI_BUFFEROVERFLOW;

    if (!(flags & DIGDD_PEEK))
    {
        /* Empty buffer */
        This->queue_tail = (This->queue_tail + len) % This->queue_len;
        This->overflow = FALSE;
    }

    LeaveCriticalSection(&This->crit);

    TRACE("Returning %ld events queued\n", *entries);
    return ret;
}

static HRESULT WINAPI SysKeyboardAImpl_EnumObjects(
	LPDIRECTINPUTDEVICE8A iface,
	LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback,
	LPVOID lpvRef,
	DWORD dwFlags)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    DIDEVICEOBJECTINSTANCEA ddoi;
    int i;
    
    TRACE("(this=%p,%p,%p,%08lx)\n", This, lpCallback, lpvRef, dwFlags);
    if (TRACE_ON(dinput)) {
        TRACE("  - flags = ");
	_dump_EnumObjects_flags(dwFlags);
	TRACE("\n");
    }

    /* Only the fields till dwFFMaxForce are relevant */
    memset(&ddoi, 0, sizeof(ddoi));
    ddoi.dwSize = FIELD_OFFSET(DIDEVICEOBJECTINSTANCEA, dwFFMaxForce);

    for (i = 0; i < WINE_DINPUT_KEYBOARD_MAX_KEYS; i++) {
        /* Report 255 keys :-) */
        ddoi.guidType = GUID_Key;
	ddoi.dwOfs = i;
	ddoi.dwType = DIDFT_MAKEINSTANCE(i) | DIDFT_BUTTON;
	GetKeyNameTextA(((i & 0x7f) << 16) | ((i & 0x80) << 17), ddoi.tszName, sizeof(ddoi.tszName));
	_dump_OBJECTINSTANCEA(&ddoi);
	if (lpCallback(&ddoi, lpvRef) != DIENUM_CONTINUE) return DI_OK;
    }
    
    return DI_OK;
}

static HRESULT WINAPI SysKeyboardWImpl_EnumObjects(LPDIRECTINPUTDEVICE8W iface,
						   LPDIENUMDEVICEOBJECTSCALLBACKW lpCallback,
						   LPVOID lpvRef,
						   DWORD dwFlags)
{
  SysKeyboardImpl *This = (SysKeyboardImpl *)iface;

  device_enumobjects_AtoWcb_data data;

  data.lpCallBack = lpCallback;
  data.lpvRef = lpvRef;

  return SysKeyboardAImpl_EnumObjects((LPDIRECTINPUTDEVICE8A) This, (LPDIENUMDEVICEOBJECTSCALLBACKA) DIEnumDevicesCallbackAtoW, (LPVOID) &data, dwFlags);
}

static HRESULT WINAPI SysKeyboardAImpl_Unacquire(LPDIRECTINPUTDEVICE8A iface);

static HRESULT WINAPI SysKeyboardAImpl_Acquire(LPDIRECTINPUTDEVICE8A iface)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;

    TRACE("(%p)\n",This);

    if (This->acquired) return DI_NOEFFECT;
    This->acquired = 1;

    if (current_lock != NULL) {
        FIXME("Not more than one keyboard can be acquired at the same time.\n");
        SysKeyboardAImpl_Unacquire((LPDIRECTINPUTDEVICE8A)current_lock);
    }
    current_lock = This;

    if (This->queue_len > 0)
    {
        This->data_queue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                     This->queue_len * sizeof(*(This->data_queue)));
        This->queue_head = 0;
        This->queue_tail = 0;
        This->overflow = FALSE;
    } else
        This->data_queue = NULL;

    set_dinput_hook(WH_KEYBOARD_LL, KeyboardCallback);

    return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_Unacquire(LPDIRECTINPUTDEVICE8A iface)
{
	SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
	TRACE("(this=%p)\n",This);

        if (This->acquired == 0)
          return DI_NOEFFECT;

        set_dinput_hook(WH_KEYBOARD_LL, NULL);

	/* No more locks */
        if (current_lock == This)
          current_lock = NULL;
        else
          ERR("this != current_lock\n");

	/* Unacquire device */
        This->acquired = 0;

    if (This->queue_len >= 0) {
        HeapFree(GetProcessHeap(), 0, This->data_queue);
        This->data_queue = NULL;
    }

    return DI_OK;
}

static HRESULT WINAPI SysKeyboardAImpl_SetEventNotification(LPDIRECTINPUTDEVICE8A iface,
							    HANDLE hnd) {
  SysKeyboardImpl *This = (SysKeyboardImpl *)iface;

  TRACE("(this=%p,%p)\n",This,hnd);

  This->hEvent = hnd;
  return DI_OK;
}

/******************************************************************************
  *     GetCapabilities : get the device capablitites
  */
static HRESULT WINAPI SysKeyboardAImpl_GetCapabilities(
	LPDIRECTINPUTDEVICE8A iface,
	LPDIDEVCAPS lpDIDevCaps)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    DIDEVCAPS devcaps;

    TRACE("(this=%p,%p)\n",This,lpDIDevCaps);

    if ((lpDIDevCaps->dwSize != sizeof(DIDEVCAPS)) && (lpDIDevCaps->dwSize != sizeof(DIDEVCAPS_DX3))) {
        WARN("invalid parameter\n");
        return DIERR_INVALIDPARAM;
    }
    
    devcaps.dwSize = lpDIDevCaps->dwSize;
    devcaps.dwFlags = DIDC_ATTACHED;
    if (This->dinput->dwVersion >= 0x0800)
	devcaps.dwDevType = DI8DEVTYPE_KEYBOARD | (DI8DEVTYPEKEYBOARD_UNKNOWN << 8);
    else
	devcaps.dwDevType = DIDEVTYPE_KEYBOARD | (DIDEVTYPEKEYBOARD_UNKNOWN << 8);
    devcaps.dwAxes = 0;
    devcaps.dwButtons = WINE_DINPUT_KEYBOARD_MAX_KEYS;
    devcaps.dwPOVs = 0;
    devcaps.dwFFSamplePeriod = 0;
    devcaps.dwFFMinTimeResolution = 0;
    devcaps.dwFirmwareRevision = 100;
    devcaps.dwHardwareRevision = 100;
    devcaps.dwFFDriverVersion = 0;

    memcpy(lpDIDevCaps, &devcaps, lpDIDevCaps->dwSize);
    
    return DI_OK;
}

/******************************************************************************
  *     GetObjectInfo : get information about a device object such as a button
  *                     or axis
  */
static HRESULT WINAPI
SysKeyboardAImpl_GetObjectInfo(
	LPDIRECTINPUTDEVICE8A iface,
	LPDIDEVICEOBJECTINSTANCEA pdidoi,
	DWORD dwObj,
	DWORD dwHow)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    DIDEVICEOBJECTINSTANCEA ddoi;
    DWORD dwSize = pdidoi->dwSize;
    
    TRACE("(this=%p,%p,%ld,0x%08lx)\n", This, pdidoi, dwObj, dwHow);

    if (dwHow == DIPH_BYID) {
        WARN(" querying by id not supported yet...\n");
	return DI_OK;
    }

    memset(pdidoi, 0, dwSize);
    memset(&ddoi, 0, sizeof(ddoi));

    ddoi.dwSize = dwSize;
    ddoi.guidType = GUID_Key;
    ddoi.dwOfs = dwObj;
    ddoi.dwType = DIDFT_MAKEINSTANCE(dwObj) | DIDFT_BUTTON;
    GetKeyNameTextA(((dwObj & 0x7f) << 16) | ((dwObj & 0x80) << 17), ddoi.tszName, sizeof(ddoi.tszName));

    /* And return our just filled device object instance structure */
    memcpy(pdidoi, &ddoi, (dwSize < sizeof(ddoi) ? dwSize : sizeof(ddoi)));
    
    _dump_OBJECTINSTANCEA(pdidoi);

    return DI_OK;
}

static HRESULT WINAPI SysKeyboardWImpl_GetObjectInfo(LPDIRECTINPUTDEVICE8W iface,
						     LPDIDEVICEOBJECTINSTANCEW pdidoi,
						     DWORD dwObj,
						     DWORD dwHow)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    DIDEVICEOBJECTINSTANCEW ddoi;
    DWORD dwSize = pdidoi->dwSize;
    
    TRACE("(this=%p,%p,%ld,0x%08lx)\n", This, pdidoi, dwObj, dwHow);

    if (dwHow == DIPH_BYID) {
        WARN(" querying by id not supported yet...\n");
	return DI_OK;
    }

    memset(pdidoi, 0, dwSize);
    memset(&ddoi, 0, sizeof(ddoi));

    ddoi.dwSize = dwSize;
    ddoi.guidType = GUID_Key;
    ddoi.dwOfs = dwObj;
    ddoi.dwType = DIDFT_MAKEINSTANCE(dwObj) | DIDFT_BUTTON;
    GetKeyNameTextW(((dwObj & 0x7f) << 16) | ((dwObj & 0x80) << 17), ddoi.tszName, sizeof(ddoi.tszName));

    /* And return our just filled device object instance structure */
    memcpy(pdidoi, &ddoi, (dwSize < sizeof(ddoi) ? dwSize : sizeof(ddoi)));
    
    _dump_OBJECTINSTANCEW(pdidoi);

    return DI_OK;
}

/******************************************************************************
  *     GetDeviceInfo : get information about a device's identity
  */
static HRESULT WINAPI SysKeyboardAImpl_GetDeviceInfo(
	LPDIRECTINPUTDEVICE8A iface,
	LPDIDEVICEINSTANCEA pdidi)
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    TRACE("(this=%p,%p)\n", This, pdidi);

    if (pdidi->dwSize != sizeof(DIDEVICEINSTANCEA)) {
        WARN(" dinput3 not supported yet...\n");
	return DI_OK;
    }

    fill_keyboard_dideviceinstanceA(pdidi, This->dinput->dwVersion);
    
    return DI_OK;
}

static HRESULT WINAPI SysKeyboardWImpl_GetDeviceInfo(LPDIRECTINPUTDEVICE8W iface, LPDIDEVICEINSTANCEW pdidi) 
{
    SysKeyboardImpl *This = (SysKeyboardImpl *)iface;
    TRACE("(this=%p,%p)\n", This, pdidi);

    if (pdidi->dwSize != sizeof(DIDEVICEINSTANCEW)) {
        WARN(" dinput3 not supported yet...\n");
	return DI_OK;
    }

    fill_keyboard_dideviceinstanceW(pdidi, This->dinput->dwVersion);
    
    return DI_OK;
}

static const IDirectInputDevice8AVtbl SysKeyboardAvt =
{
	IDirectInputDevice2AImpl_QueryInterface,
	IDirectInputDevice2AImpl_AddRef,
	SysKeyboardAImpl_Release,
	SysKeyboardAImpl_GetCapabilities,
	SysKeyboardAImpl_EnumObjects,
	SysKeyboardAImpl_GetProperty,
	SysKeyboardAImpl_SetProperty,
	SysKeyboardAImpl_Acquire,
	SysKeyboardAImpl_Unacquire,
	SysKeyboardAImpl_GetDeviceState,
	SysKeyboardAImpl_GetDeviceData,
	IDirectInputDevice2AImpl_SetDataFormat,
	SysKeyboardAImpl_SetEventNotification,
	IDirectInputDevice2AImpl_SetCooperativeLevel,
	SysKeyboardAImpl_GetObjectInfo,
	SysKeyboardAImpl_GetDeviceInfo,
	IDirectInputDevice2AImpl_RunControlPanel,
	IDirectInputDevice2AImpl_Initialize,
	IDirectInputDevice2AImpl_CreateEffect,
	IDirectInputDevice2AImpl_EnumEffects,
	IDirectInputDevice2AImpl_GetEffectInfo,
	IDirectInputDevice2AImpl_GetForceFeedbackState,
	IDirectInputDevice2AImpl_SendForceFeedbackCommand,
	IDirectInputDevice2AImpl_EnumCreatedEffectObjects,
	IDirectInputDevice2AImpl_Escape,
	IDirectInputDevice2AImpl_Poll,
        IDirectInputDevice2AImpl_SendDeviceData,
        IDirectInputDevice7AImpl_EnumEffectsInFile,
        IDirectInputDevice7AImpl_WriteEffectToFile,
        IDirectInputDevice8AImpl_BuildActionMap,
        IDirectInputDevice8AImpl_SetActionMap,
        IDirectInputDevice8AImpl_GetImageInfo
};

#if !defined(__STRICT_ANSI__) && defined(__GNUC__)
# define XCAST(fun)	(typeof(SysKeyboardWvt.fun))
#else
# define XCAST(fun)	(void*)
#endif

static const IDirectInputDevice8WVtbl SysKeyboardWvt =
{
	IDirectInputDevice2WImpl_QueryInterface,
	XCAST(AddRef)IDirectInputDevice2AImpl_AddRef,
	XCAST(Release)SysKeyboardAImpl_Release,
	XCAST(GetCapabilities)SysKeyboardAImpl_GetCapabilities,
	SysKeyboardWImpl_EnumObjects,
	XCAST(GetProperty)SysKeyboardAImpl_GetProperty,
	XCAST(SetProperty)SysKeyboardAImpl_SetProperty,
	XCAST(Acquire)SysKeyboardAImpl_Acquire,
	XCAST(Unacquire)SysKeyboardAImpl_Unacquire,
	XCAST(GetDeviceState)SysKeyboardAImpl_GetDeviceState,
	XCAST(GetDeviceData)SysKeyboardAImpl_GetDeviceData,
	XCAST(SetDataFormat)IDirectInputDevice2AImpl_SetDataFormat,
	XCAST(SetEventNotification)SysKeyboardAImpl_SetEventNotification,
	XCAST(SetCooperativeLevel)IDirectInputDevice2AImpl_SetCooperativeLevel,
	SysKeyboardWImpl_GetObjectInfo,
	SysKeyboardWImpl_GetDeviceInfo,
	XCAST(RunControlPanel)IDirectInputDevice2AImpl_RunControlPanel,
	XCAST(Initialize)IDirectInputDevice2AImpl_Initialize,
	XCAST(CreateEffect)IDirectInputDevice2AImpl_CreateEffect,
	IDirectInputDevice2WImpl_EnumEffects,
	IDirectInputDevice2WImpl_GetEffectInfo,
	XCAST(GetForceFeedbackState)IDirectInputDevice2AImpl_GetForceFeedbackState,
	XCAST(SendForceFeedbackCommand)IDirectInputDevice2AImpl_SendForceFeedbackCommand,
	XCAST(EnumCreatedEffectObjects)IDirectInputDevice2AImpl_EnumCreatedEffectObjects,
	XCAST(Escape)IDirectInputDevice2AImpl_Escape,
	XCAST(Poll)IDirectInputDevice2AImpl_Poll,
        XCAST(SendDeviceData)IDirectInputDevice2AImpl_SendDeviceData,
        IDirectInputDevice7WImpl_EnumEffectsInFile,
        IDirectInputDevice7WImpl_WriteEffectToFile,
        IDirectInputDevice8WImpl_BuildActionMap,
        IDirectInputDevice8WImpl_SetActionMap,
        IDirectInputDevice8WImpl_GetImageInfo
};
#undef XCAST
