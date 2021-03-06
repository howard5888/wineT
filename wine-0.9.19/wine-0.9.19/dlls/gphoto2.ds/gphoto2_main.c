/*
 * SANE.DS functions
 *
 * Copyright 2000 Shi Quan He <shiquan@cyberdude.com>
 * Copyright 2006 Marcus Meissner
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

#include "windef.h"
#include "winbase.h"
#include "twain.h"
#include "gphoto2_i.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(twain);

HINSTANCE GPHOTO2_instance;

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%p,%lx,%p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
	    GPHOTO2_instance = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
#ifdef HAVE_GPHOTO2
	    activeDS.context = gp_context_new ();
#endif
            break;

        case DLL_PROCESS_DETACH:
	    GPHOTO2_instance = NULL;
            break;
    }

    return TRUE;
}

#ifdef HAVE_GPHOTO2
static TW_UINT16 GPHOTO2_GetIdentity( pTW_IDENTITY, pTW_IDENTITY);
static TW_UINT16 GPHOTO2_OpenDS( pTW_IDENTITY, pTW_IDENTITY);
#endif

TW_UINT16 GPHOTO2_SourceControlHandler (
   pTW_IDENTITY pOrigin,
   TW_UINT16    DAT,
   TW_UINT16    MSG,
   TW_MEMREF    pData)
{
    TW_UINT16 twRC = TWRC_SUCCESS;

    switch (DAT)
    {
	case DAT_IDENTITY:
	    switch (MSG)
	    {
		case MSG_CLOSEDS:
#ifdef HAVE_GPHOTO2
		     if (activeDS.camera) {
			gp_camera_free (activeDS.camera);
			activeDS.camera = NULL;
		     }
#endif
		     break;
		case MSG_GET:
#ifdef HAVE_GPHOTO2
		     twRC = GPHOTO2_GetIdentity(pOrigin,(pTW_IDENTITY)pData);
#else
		     twRC = TWRC_FAILURE;
#endif
		     break;
		case MSG_OPENDS:
#ifdef HAVE_GPHOTO2
		     twRC = GPHOTO2_OpenDS(pOrigin,(pTW_IDENTITY)pData);
#else
		     twRC = TWRC_FAILURE;
#endif
		     break;
	    }
	    break;
        case DAT_CAPABILITY:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_CapabilityGet (pOrigin, pData);
                    break;
                case MSG_GETCURRENT:
                    twRC = GPHOTO2_CapabilityGetCurrent (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_CapabilityGetDefault (pOrigin, pData);
                    break;
                case MSG_QUERYSUPPORT:
                    twRC = GPHOTO2_CapabilityQuerySupport (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_CapabilityReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_CapabilitySet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    FIXME("unrecognized opertion triplet\n");
            }
            break;

        case DAT_CUSTOMDSDATA:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_CustomDSDataGet (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_CustomDSDataSet (pOrigin, pData);
                    break;
                default:
                    break;
            }
            break;

        case DAT_FILESYSTEM:
            switch (MSG)
            {
                /*case MSG_AUTOMATICCAPTUREDIRECTORY:
                    twRC = GPHOTO2_AutomaticCaptureDirectory
                               (pOrigin, pData);
                    break;*/
                case MSG_CHANGEDIRECTORY:
                    twRC = GPHOTO2_ChangeDirectory (pOrigin, pData);
                    break;
                /*case MSG_COPY:
                    twRC = GPHOTO2_FileSystemCopy (pOrigin, pData);
                    break;*/
                case MSG_CREATEDIRECTORY:
                    twRC = GPHOTO2_CreateDirectory (pOrigin, pData);
                    break;
                case MSG_DELETE:
                    twRC = GPHOTO2_FileSystemDelete (pOrigin, pData);
                    break;
                case MSG_FORMATMEDIA:
                    twRC = GPHOTO2_FormatMedia (pOrigin, pData);
                    break;
                case MSG_GETCLOSE:
                    twRC = GPHOTO2_FileSystemGetClose (pOrigin, pData);
                    break;
                case MSG_GETFIRSTFILE:
                    twRC = GPHOTO2_FileSystemGetFirstFile (pOrigin, pData);
                    break;
                case MSG_GETINFO:
                    twRC = GPHOTO2_FileSystemGetInfo (pOrigin, pData);
                    break;
                case MSG_GETNEXTFILE:
                    twRC = GPHOTO2_FileSystemGetNextFile (pOrigin, pData);
                    break;
                case MSG_RENAME:
                    twRC = GPHOTO2_FileSystemRename (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    break;
            }
            break;

        case DAT_EVENT:
            if (MSG == MSG_PROCESSEVENT)
                twRC = GPHOTO2_ProcessEvent (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_PASSTHRU:
            if (MSG == MSG_PASSTHRU)
                twRC = GPHOTO2_PassThrough (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_PENDINGXFERS:
            switch (MSG)
            {
                case MSG_ENDXFER:
                    twRC = GPHOTO2_PendingXfersEndXfer (pOrigin, pData);
                    break;
                case MSG_GET:
                    twRC = GPHOTO2_PendingXfersGet (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_PendingXfersReset (pOrigin, pData);
                    break;
                /*case MSG_STOPFEEDER:
                    twRC = GPHOTO2_PendingXfersStopFeeder (pOrigin, pData);
                    break;*/
                default:
                    twRC = TWRC_FAILURE;
            }
            break;

        case DAT_SETUPFILEXFER:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_SetupFileXferGet (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_SetupFileXferGetDefault (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_SetupFileXferReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_SetupFileXferSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    break;
            }
            break;

        /*case DAT_SETUPFILEXFER2:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_SetupFileXfer2Get (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_SetupFileXfer2GetDefault (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_SetupFileXfer2Reset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_SetupFileXfer2Set (pOrigin, pData);
                    break;
            }
            break;*/
        case DAT_SETUPMEMXFER:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_SetupMemXferGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_STATUS:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_GetDSStatus (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_USERINTERFACE:
            switch (MSG)
            {
                case MSG_DISABLEDS:
                    twRC = GPHOTO2_DisableDSUserInterface (pOrigin, pData);
                    break;
                case MSG_ENABLEDS:
                    twRC = GPHOTO2_EnableDSUserInterface (pOrigin, pData);
                    break;
                case MSG_ENABLEDSUIONLY:
                    twRC = GPHOTO2_EnableDSUIOnly (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    break;
            }
            break;

        case DAT_XFERGROUP:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_XferGroupGet (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_XferGroupSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    break;
            }
            break;

        default:
	    FIXME("code unknown: %d\n", DAT);
            twRC = TWRC_FAILURE;
            break;
    }

    return twRC;
}


TW_UINT16 GPHOTO2_ImageGroupHandler (
           pTW_IDENTITY pOrigin,
           TW_UINT16    DAT,
           TW_UINT16    MSG,
           TW_MEMREF    pData)
{
    TW_UINT16 twRC = TWRC_SUCCESS;

    switch (DAT)
    {
        case DAT_CIECOLOR:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_CIEColorGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_EXTIMAGEINFO:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_ExtImageInfoGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_GRAYRESPONSE:
            switch (MSG)
            {
                case MSG_RESET:
                    twRC = GPHOTO2_GrayResponseReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_GrayResponseSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    activeDS.twCC = TWCC_BADPROTOCOL;
                    FIXME("unrecognized operation triplet\n");
                    break;
            }
            break;
        case DAT_IMAGEFILEXFER:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_ImageFileXferGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_IMAGEINFO:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_ImageInfoGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_IMAGELAYOUT:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_ImageLayoutGet (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_ImageLayoutGetDefault (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_ImageLayoutReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_ImageLayoutSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    activeDS.twCC = TWCC_BADPROTOCOL;
                    ERR("unrecognized operation triplet\n");
                    break;
            }
            break;

        case DAT_IMAGEMEMXFER:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_ImageMemXferGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_IMAGENATIVEXFER:
            if (MSG == MSG_GET)
                twRC = GPHOTO2_ImageNativeXferGet (pOrigin, pData);
            else
                twRC = TWRC_FAILURE;
            break;

        case DAT_JPEGCOMPRESSION:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_JPEGCompressionGet (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_JPEGCompressionGetDefault (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_JPEGCompressionReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_JPEGCompressionSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    activeDS.twCC = TWCC_BADPROTOCOL;
                    WARN("unrecognized operation triplet\n");
                    break;
            }
            break;

        case DAT_PALETTE8:
            switch (MSG)
            {
                case MSG_GET:
                    twRC = GPHOTO2_Palette8Get (pOrigin, pData);
                    break;
                case MSG_GETDEFAULT:
                    twRC = GPHOTO2_Palette8GetDefault (pOrigin, pData);
                    break;
                case MSG_RESET:
                    twRC = GPHOTO2_Palette8Reset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_Palette8Set (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    activeDS.twCC = TWCC_BADPROTOCOL;
                    WARN("unrecognized operation triplet\n");
            }
            break;

        case DAT_RGBRESPONSE:
            switch (MSG)
            {
                case MSG_RESET:
                    twRC = GPHOTO2_RGBResponseReset (pOrigin, pData);
                    break;
                case MSG_SET:
                    twRC = GPHOTO2_RGBResponseSet (pOrigin, pData);
                    break;
                default:
                    twRC = TWRC_FAILURE;
                    activeDS.twCC = TWCC_BADPROTOCOL;
                    WARN("unrecognized operation triplet\n");
                    break;
            }
            break;

        default:
            twRC = TWRC_FAILURE;
            activeDS.twCC = TWCC_BADPROTOCOL;
            FIXME("unrecognized DG type %d\n", DAT);
    }
    return twRC;
}

/* Main entry point for the TWAIN library */
TW_UINT16 WINAPI
DS_Entry ( pTW_IDENTITY pOrigin,
           TW_UINT32    DG,
           TW_UINT16    DAT,
           TW_UINT16    MSG,
           TW_MEMREF    pData)
{
    TW_UINT16 twRC = TWRC_SUCCESS;  /* Return Code */

    TRACE("(DG=%ld DAT=%d MSG=%d)\n", DG, DAT, MSG);

    switch (DG)
    {
        case DG_CONTROL:
            twRC = GPHOTO2_SourceControlHandler (pOrigin,DAT,MSG,pData);
            break;
        case DG_IMAGE:
            twRC = GPHOTO2_ImageGroupHandler (pOrigin,DAT,MSG,pData);
            break;
        case DG_AUDIO:
            FIXME("The audio group of entry codes is not implemented.\n");
        default:
            activeDS.twCC = TWCC_BADPROTOCOL;
            twRC = TWRC_FAILURE;
    }

    return twRC;
}

#ifdef HAVE_GPHOTO2
static GPPortInfoList *port_list;
static int curcamera;
static CameraList *detected_cameras;
static CameraAbilitiesList *abilities_list;

static TW_UINT16
gphoto2_auto_detect() {
    int result, count;

    if (detected_cameras && (gp_list_count (detected_cameras) == 0)) {
	/* Reload if previously no cameras, we might detect new ones. */
	TRACE("Reloading portlist trying to detect cameras.\n");
	if (port_list) {
	    gp_port_info_list_free (port_list);
	    port_list = NULL;
	}
    }
    if (!port_list) {
	TRACE("Auto detecting gphoto cameras.\n");
	TRACE("Loading ports...\n");
	if (gp_port_info_list_new (&port_list) < GP_OK)
	    return TWRC_FAILURE;
	result = gp_port_info_list_load (port_list);
	if (result < 0) {
	    gp_port_info_list_free (port_list);
	    return TWRC_FAILURE;
	}
	count = gp_port_info_list_count (port_list);
	if (count <= 0)
	    return TWRC_FAILURE;
	if (gp_list_new (&detected_cameras) < GP_OK)
	    return TWRC_FAILURE;
	if (!abilities_list) { /* Load only once per program start */
	    gp_abilities_list_new (&abilities_list);
	    TRACE("Loading cameras...\n");
	    gp_abilities_list_load (abilities_list, NULL);
	}
	TRACE("Detecting cameras...\n");
	gp_abilities_list_detect (abilities_list, port_list, detected_cameras, NULL);
        curcamera = 0;
        TRACE("%d cameras detected\n", gp_list_count(detected_cameras));
    }
    return TWRC_SUCCESS;
}

static TW_UINT16
GPHOTO2_GetIdentity( pTW_IDENTITY pOrigin, pTW_IDENTITY self) {
    int count;
    const char *cname, *pname;

    if (TWRC_SUCCESS != gphoto2_auto_detect())
	return TWRC_FAILURE;

    count = gp_list_count (detected_cameras);
    if (count < GP_OK) {
	gp_list_free (detected_cameras);
	return TWRC_FAILURE;
    }
    TRACE("%d cameras detected.\n", count);
    self->ProtocolMajor = TWON_PROTOCOLMAJOR;
    self->ProtocolMinor = TWON_PROTOCOLMINOR;
    lstrcpynA (self->Manufacturer, "The Wine Team", sizeof(self->Manufacturer) - 1);
    lstrcpynA (self->ProductFamily, "GPhoto2 Camera", sizeof(self->ProductFamily) - 1);

    if (!count) { /* No camera detected. But we need to return an IDENTITY anyway. */
	lstrcpynA (self->ProductName, "GPhoto2 Camera", sizeof(self->ProductName) - 1);
	return TWRC_SUCCESS;
    }
    gp_list_get_name  (detected_cameras, curcamera, &cname);
    gp_list_get_value (detected_cameras, curcamera, &pname);
    if (count == 1) /* Normal case, only one camera. */
	snprintf (self->ProductName, sizeof(self->ProductName), "%s", cname);
    else
	snprintf (self->ProductName, sizeof(self->ProductName), "%s@%s", cname, pname);
    curcamera = (curcamera+1) % count;
    return TWRC_SUCCESS;
}

static TW_UINT16
GPHOTO2_OpenDS( pTW_IDENTITY pOrigin, pTW_IDENTITY self) {
    int ret, m, p, count, i;
    CameraAbilities a;
    GPPortInfo info;
    const char	*model, *port;

    if (TWRC_SUCCESS != gphoto2_auto_detect())
	return TWRC_FAILURE;

    if (lstrcmpA(self->ProductFamily,"GPhoto2 Camera")) {
	FIXME("identity passed is not a gphoto camera, but %s!?!\n", self->ProductFamily);
	return TWRC_FAILURE;
    }
    count = gp_list_count (detected_cameras);
    if (!count) {
	ERR("No camera found by autodetection. Returning failure.\n");
	return TWRC_FAILURE;
    }

    if (!lstrcmpA (self->ProductName, "GPhoto2 Camera")) {
	TRACE("Potential undetected camera. Just using the first autodetected one.\n");
	i = 0;
    } else {
	for (i=0;i<count;i++) {
	    const char *cname, *pname;
	    TW_STR32	name;

	    gp_list_get_name  (detected_cameras, i, &cname);
	    gp_list_get_value (detected_cameras, i, &pname);
	    if (!lstrcmpA(self->ProductName,cname))
		break;
	    snprintf(name, sizeof(name), "%s", cname);
	    if (!lstrcmpA(self->ProductName,name))
		break;
	    snprintf(name, sizeof(name), "%s@%s", cname, pname);
	    if (!lstrcmpA(self->ProductName,name))
		break;
        }
        if (i == count) {
	    TRACE("Camera %s not found in autodetected list. Using first entry.\n", self->ProductName);
	    i=0;
        }
    }
    gp_list_get_name  (detected_cameras, i, &model);
    gp_list_get_value  (detected_cameras, i, &port);
    TRACE("model %s, port %s\n", model, port);
    ret = gp_camera_new (&activeDS.camera);
    if (ret < GP_OK) {
	ERR("gp_camera_new: %d\n", ret);
	return TWRC_FAILURE;
    }
    m = gp_abilities_list_lookup_model (abilities_list, model);
    if (m < GP_OK) {
	FIXME("Model %s not found, %d!\n", model, m);
	return TWRC_FAILURE;
    }
    ret = gp_abilities_list_get_abilities (abilities_list, m, &a);
    if (ret < GP_OK) {
	FIXME("gp_camera_list_get_abilities failed? %d\n", ret);
	return TWRC_FAILURE;
    }
    ret = gp_camera_set_abilities (activeDS.camera, a);
    if (ret < GP_OK) {
	FIXME("gp_camera_set_abilities failed? %d\n", ret);
	return TWRC_FAILURE;
    }

    p = gp_port_info_list_lookup_path (port_list, port);
    if (p < GP_OK) {
	FIXME("port %s not in portlist?\n", port);
	return TWRC_FAILURE;
    }
    ret = gp_port_info_list_get_info (port_list, p, &info);
    if (ret < GP_OK) {
	FIXME("could not get portinfo for port %s?\n", port);
	return TWRC_FAILURE;
    }
    ret = gp_camera_set_port_info (activeDS.camera, info);
    if (ret < GP_OK) {
	FIXME("could not set portinfo for port %s to camera?\n", port);
	return TWRC_FAILURE;
    }
    list_init( &(activeDS.files) );
    activeDS.currentState = 4;
    activeDS.twCC 		= TWRC_SUCCESS;
    activeDS.pixelflavor	= TWPF_CHOCOLATE;
    activeDS.pixeltype		= TWPT_RGB;
    activeDS.capXferMech	= TWSX_MEMORY;
    TRACE("OK!\n");
    return TWRC_SUCCESS;
}
#endif
