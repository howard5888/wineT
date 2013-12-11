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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlguid.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define LOAD_INITIAL_DOCUMENT_URI 0x80000

#define NS_IOSERVICE_CLASSNAME "nsIOService"
#define NS_IOSERVICE_CONTRACTID "@mozilla.org/network/io-service;1"

static const IID NS_IOSERVICE_CID =
    {0x9ac9e770, 0x18bc, 0x11d3, {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40}};

static nsIIOService *nsio = NULL;

typedef struct {
    const nsIWineURIVtbl *lpWineURIVtbl;

    LONG ref;

    nsIURI *uri;
    NSContainer *container;
    IMoniker *mon;
    LPSTR spec;
} nsURI;

#define NSURI(x)         ((nsIURI*)            &(x)->lpWineURIVtbl)

static nsresult create_uri(nsIURI*,NSContainer*,nsIURI**);

static BOOL exec_shldocvw_67(HTMLDocument *doc, LPCWSTR url)
{
    IOleCommandTarget *cmdtrg = NULL;
    HRESULT hres;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IOleCommandTarget,
                                         (void**)&cmdtrg);
    if(SUCCEEDED(hres)) {
        VARIANT varUrl, varRes;

        V_VT(&varUrl) = VT_BSTR;
        V_BSTR(&varUrl) = SysAllocString(url);
        V_VT(&varRes) = VT_BOOL;

        hres = IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 67, 0, &varUrl, &varRes);

        IOleCommandTarget_Release(cmdtrg);
        SysFreeString(V_BSTR(&varUrl));

        if(SUCCEEDED(hres) && !V_BOOL(&varRes)) {
            TRACE("got VARIANT_FALSE, do not load\n");
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL handle_uri(NSContainer *container, nsChannel *channel, LPCWSTR uri)
{
    IServiceProvider *service_provider;
    HTMLDocument *doc = container->doc;
    DWORD hlnf = 0;
    HRESULT hres;

    if(!doc) {
        NSContainer *container_iter = container;

        hlnf = HLNF_OPENINNEWWINDOW;
        while(!container_iter->doc)
            container_iter = container_iter->parent;
        doc = container_iter->doc;
    }

    if(!hlnf && !exec_shldocvw_67(doc, uri))
        return FALSE;

    hres = IOleClientSite_QueryInterface(doc->client, &IID_IServiceProvider,
                                         (void**)&service_provider);
    if(SUCCEEDED(hres)) {
        IHlinkFrame *hlink_frame;

        hres = IServiceProvider_QueryService(service_provider, &IID_IHlinkFrame,
                                             &IID_IHlinkFrame, (void**)&hlink_frame);
        IServiceProvider_Release(service_provider);
        if(SUCCEEDED(hres)) {
            hlink_frame_navigate(doc, hlink_frame, uri, channel->post_data_stream, hlnf);
            IHlinkFrame_Release(hlink_frame);

            return FALSE;
        }
    }

    return TRUE;
}

static BOOL before_async_open(nsChannel *channel, NSContainer *container)
{
    nsACString uri_str;
    const char *uria;
    LPWSTR uri;
    DWORD len;
    BOOL ret;

    nsACString_Init(&uri_str, NULL);
    nsIWineURI_GetSpec(channel->uri, &uri_str);
    nsACString_GetData(&uri_str, &uria, NULL);
    len = MultiByteToWideChar(CP_ACP, 0, uria, -1, NULL, 0);
    uri = mshtml_alloc(len*sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, uria, -1, uri, len);
    nsACString_Finish(&uri_str);

    ret = handle_uri(container, channel, uri);

    mshtml_free(uri);

    return ret;
}

#define NSCHANNEL_THIS(iface) DEFINE_THIS(nsChannel, HttpChannel, iface)

static nsresult NSAPI nsChannel_QueryInterface(nsIHttpChannel *iface, nsIIDRef riid, nsQIResult result)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = NSCHANNEL(This);
    }else if(IsEqualGUID(&IID_nsIRequest, riid)) {
        TRACE("(%p)->(IID_nsIRequest %p)\n", This, result);
        *result = NSCHANNEL(This);
    }else if(IsEqualGUID(&IID_nsIChannel, riid)) {
        TRACE("(%p)->(IID_nsIChannel %p)\n", This, result);
        *result = NSCHANNEL(This);
    }else if(This->http_channel && IsEqualGUID(&IID_nsIHttpChannel, riid)) {
        TRACE("(%p)->(IID_nsIHttpChannel %p)\n", This, result);
        *result = NSHTTPCHANNEL(This);
    }else if(IsEqualGUID(&IID_nsIUploadChannel, riid)) {
        TRACE("(%p)->(IID_nsIUploadChannel %p)\n", This, result);
        *result = NSUPCHANNEL(This);
    }

    if(*result) {
        nsIChannel_AddRef(NSCHANNEL(This));
        return NS_OK;
    }

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);

    if(This->channel)
        return nsIChannel_QueryInterface(This->channel, riid, result);
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsChannel_AddRef(nsIHttpChannel *iface)
{
    nsChannel *This = NSCHANNEL_THIS(iface);
    nsrefcnt ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsChannel_Release(nsIHttpChannel *iface)
{
    nsChannel *This = NSCHANNEL_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    if(!ref) {
        nsIWineURI_Release(This->uri);
        if(This->channel)
            nsIChannel_Release(This->channel);
        if(This->http_channel)
            nsIHttpChannel_Release(This->http_channel);
        if(This->post_data_stream)
            nsIInputStream_Release(This->post_data_stream);
        if(This->load_group)
            nsILoadGroup_Release(This->load_group);
        if(This->notif_callback)
            nsIInterfaceRequestor_Release(This->notif_callback);
        if(This->original_uri)
            nsIURI_Release(This->original_uri);
        mshtml_free(This->content);
        mshtml_free(This);
    }

    return ref;
}

static nsresult NSAPI nsChannel_GetName(nsIHttpChannel *iface, nsACString *aName)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aName);

    if(This->channel)
        return nsIChannel_GetName(This->channel, aName);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_IsPending(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, _retval);

    if(This->channel)
        return nsIChannel_IsPending(This->channel, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetStatus(nsIHttpChannel *iface, nsresult *aStatus)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aStatus);

    if(This->channel)
        return nsIChannel_GetStatus(This->channel, aStatus);

    TRACE("returning NS_OK\n");
    return *aStatus = NS_OK;
}

static nsresult NSAPI nsChannel_Cancel(nsIHttpChannel *iface, nsresult aStatus)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%08lx)\n", This, aStatus);

    if(This->channel)
        return nsIChannel_Cancel(This->channel, aStatus);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Suspend(nsIHttpChannel *iface)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)\n", This);

    if(This->channel)
        return nsIChannel_Suspend(This->channel);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Resume(nsIHttpChannel *iface)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)\n", This);

    if(This->channel)
        return nsIChannel_Resume(This->channel);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetLoadGroup(nsIHttpChannel *iface, nsILoadGroup **aLoadGroup)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aLoadGroup);

    if(This->load_group)
        nsILoadGroup_AddRef(This->load_group);

    *aLoadGroup = This->load_group;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetLoadGroup(nsIHttpChannel *iface, nsILoadGroup *aLoadGroup)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aLoadGroup);

    if(This->load_group)
        nsILoadGroup_Release(This->load_group);
    if(aLoadGroup)
        nsILoadGroup_AddRef(aLoadGroup);

    This->load_group = aLoadGroup;

    if(This->channel)
        return nsIChannel_SetLoadGroup(This->channel, aLoadGroup);
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetLoadFlags(nsIHttpChannel *iface, nsLoadFlags *aLoadFlags)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aLoadFlags);

    *aLoadFlags = This->load_flags;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetLoadFlags(nsIHttpChannel *iface, nsLoadFlags aLoadFlags)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%08lx)\n", This, aLoadFlags);

    This->load_flags = aLoadFlags;

    if(This->channel)
        return nsIChannel_SetLoadFlags(This->channel, aLoadFlags);
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetOriginalURI(nsIHttpChannel *iface, nsIURI **aOriginalURI)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aOriginalURI);

    if(This->original_uri)
        nsIURI_AddRef(This->original_uri);

    *aOriginalURI = This->original_uri;
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetOriginalURI(nsIHttpChannel *iface, nsIURI *aOriginalURI)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aOriginalURI);

    if(This->original_uri)
        nsIURI_Release(This->original_uri);

    nsIURI_AddRef(aOriginalURI);
    This->original_uri = aOriginalURI;

    if(This->channel)
        return nsIChannel_SetOriginalURI(This->channel, aOriginalURI);
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetURI(nsIHttpChannel *iface, nsIURI **aURI)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aURI);

    nsIWineURI_AddRef(This->uri);
    *aURI = (nsIURI*)This->uri;

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetOwner(nsIHttpChannel *iface, nsISupports **aOwner)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aOwner);

    if(This->channel)
        return nsIChannel_GetOwner(This->channel, aOwner);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetOwner(nsIHttpChannel *iface, nsISupports *aOwner)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aOwner);

    if(This->channel)
        return nsIChannel_SetOwner(This->channel, aOwner);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetNotificationCallbacks(nsIHttpChannel *iface,
        nsIInterfaceRequestor **aNotificationCallbacks)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aNotificationCallbacks);

    if(This->notif_callback)
        nsIInterfaceRequestor_AddRef(This->notif_callback);
    *aNotificationCallbacks = This->notif_callback;

    return NS_OK;
}

static nsresult NSAPI nsChannel_SetNotificationCallbacks(nsIHttpChannel *iface,
        nsIInterfaceRequestor *aNotificationCallbacks)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aNotificationCallbacks);

    if(This->notif_callback)
        nsIInterfaceRequestor_Release(This->notif_callback);
    if(aNotificationCallbacks)
        nsIInterfaceRequestor_AddRef(aNotificationCallbacks);

    This->notif_callback = aNotificationCallbacks;

    if(This->channel)
        return nsIChannel_SetNotificationCallbacks(This->channel, aNotificationCallbacks);
    return NS_OK;
}

static nsresult NSAPI nsChannel_GetSecurityInfo(nsIHttpChannel *iface, nsISupports **aSecurityInfo)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aSecurityInfo);

    if(This->channel)
        return nsIChannel_GetSecurityInfo(This->channel, aSecurityInfo);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetContentType(nsIHttpChannel *iface, nsACString *aContentType)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContentType);

    if(This->content) {
        nsACString_Init(aContentType, This->content);
        return S_OK;
    }

    if(This->channel)
        return nsIChannel_GetContentType(This->channel, aContentType);

    TRACE("returning default text/html\n");
    nsACString_Init(aContentType, "text/html");
    return NS_OK;
}

static nsresult NSAPI nsChannel_SetContentType(nsIHttpChannel *iface,
                                               const nsACString *aContentType)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContentType);

    if(This->channel)
        return nsIChannel_SetContentType(This->channel, aContentType);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetContentCharset(nsIHttpChannel *iface,
                                                  nsACString *aContentCharset)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContentCharset);

    if(This->channel)
        return nsIChannel_GetContentCharset(This->channel, aContentCharset);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetContentCharset(nsIHttpChannel *iface,
                                                  const nsACString *aContentCharset)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContentCharset);

    if(This->channel)
        return nsIChannel_SetContentCharset(This->channel, aContentCharset);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetContentLength(nsIHttpChannel *iface, PRInt32 *aContentLength)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContentLength);

    if(This->channel)
        return nsIChannel_GetContentLength(This->channel, aContentLength);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetContentLength(nsIHttpChannel *iface, PRInt32 aContentLength)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%ld)\n", This, aContentLength);

    if(This->channel)
        return nsIChannel_SetContentLength(This->channel, aContentLength);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_Open(nsIHttpChannel *iface, nsIInputStream **_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, _retval);

    if(This->channel)
        return nsIChannel_Open(This->channel, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_AsyncOpen(nsIHttpChannel *iface, nsIStreamListener *aListener,
                                          nsISupports *aContext)
{
    nsChannel *This = NSCHANNEL_THIS(iface);
    BSCallback *bscallback;
    nsIWineURI *wine_uri;
    IMoniker *mon;
    nsresult nsres;

    TRACE("(%p)->(%p %p)\n", This, aListener, aContext);

    if(This->load_flags & LOAD_INITIAL_DOCUMENT_URI) {
        NSContainer *container;

        nsIWineURI_GetNSContainer(This->uri, &container);
        if(!container) {
            ERR("container = NULL\n");
            return NS_ERROR_UNEXPECTED;
        }

        if(container->bscallback) {
            nsIChannel_AddRef(NSCHANNEL(This));
            container->bscallback->nschannel = This;

            nsIStreamListener_AddRef(aListener);
            container->bscallback->nslistener = aListener;

            if(aContext) {
                nsISupports_AddRef(aContext);
                container->bscallback->nscontext = aContext;
            }

            nsIWebBrowserChrome_Release(NSWBCHROME(container));

            if(!This->channel)
                return WINE_NS_LOAD_FROM_MONIKER;
        }else {
            BOOL cont = before_async_open(This, container);
            nsIWebBrowserChrome_Release(NSWBCHROME(container));

            if(!cont) {
                TRACE("canceled\n");
                return NS_ERROR_UNEXPECTED;
            }
        }
    }

    if(This->channel) {
        if(This->post_data_stream) {
            nsIUploadChannel *upload_channel;

            nsres = nsIChannel_QueryInterface(This->channel, &IID_nsIUploadChannel,
                                          (void**)&upload_channel);
            if(NS_SUCCEEDED(nsres)) {
                nsACString empty_string;
                nsACString_Init(&empty_string, "");

                nsres = nsIUploadChannel_SetUploadStream(upload_channel, This->post_data_stream,
                                                         &empty_string, -1);
                nsIUploadChannel_Release(upload_channel);
                if(NS_FAILED(nsres))
                    WARN("SetUploadStream failed: %08lx\n", nsres);

                nsACString_Finish(&empty_string);
            }
        }

        return nsIChannel_AsyncOpen(This->channel, aListener, aContext);
    }

    TRACE("channel == NULL\n");

    if(!This->original_uri) {
        ERR("original_uri == NULL\n");
        return NS_ERROR_UNEXPECTED;
    }

    nsres = nsIURI_QueryInterface(This->original_uri, &IID_nsIWineURI, (void**)&wine_uri);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIWineURI: %08lx\n", nsres);
        return NS_ERROR_UNEXPECTED;
    }

    nsIWineURI_GetMoniker(wine_uri, &mon);
    nsIWineURI_Release(wine_uri);

    if(!mon) {
        WARN("mon == NULL\n");
        return NS_ERROR_UNEXPECTED;
    }

    bscallback = create_bscallback(NULL, mon);
    IMoniker_Release(mon);

    nsIChannel_AddRef(NSCHANNEL(This));
    bscallback->nschannel = This;

    nsIStreamListener_AddRef(aListener);
    bscallback->nslistener = aListener;

    if(aContext) {
        nsISupports_AddRef(aContext);
        bscallback->nscontext = aContext;
    }

    start_binding(bscallback);
    IBindStatusCallback_Release(STATUSCLB(bscallback));

    return NS_OK;
}

static nsresult NSAPI nsChannel_GetRequestMethod(nsIHttpChannel *iface, nsACString *aRequestMethod)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aRequestMethod);

    if(This->http_channel)
        return nsIHttpChannel_GetRequestMethod(This->http_channel, aRequestMethod);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetRequestMethod(nsIHttpChannel *iface,
                                                 const nsACString *aRequestMethod)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aRequestMethod);

    if(This->http_channel)
        return nsIHttpChannel_SetRequestMethod(This->http_channel, aRequestMethod);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetReferrer(nsIHttpChannel *iface, nsIURI **aReferrer)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aReferrer);

    if(This->http_channel)
        return nsIHttpChannel_GetReferrer(This->http_channel, aReferrer);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetReferrer(nsIHttpChannel *iface, nsIURI *aReferrer)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aReferrer);

    if(This->http_channel)
        return nsIHttpChannel_SetReferrer(This->http_channel, aReferrer);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetRequestHeader(nsIHttpChannel *iface,
         const nsACString *aHeader, nsACString *_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p %p)\n", This, aHeader, _retval);

    if(This->http_channel)
        return nsIHttpChannel_GetRequestHeader(This->http_channel, aHeader, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetRequestHeader(nsIHttpChannel *iface,
         const nsACString *aHeader, const nsACString *aValue, PRBool aMerge)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p %p %x)\n", This, aHeader, aValue, aMerge);

    if(This->http_channel)
        return nsIHttpChannel_SetRequestHeader(This->http_channel, aHeader, aValue, aMerge);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_VisitRequestHeaders(nsIHttpChannel *iface,
                                                    nsIHttpHeaderVisitor *aVisitor)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aVisitor);

    if(This->http_channel)
        return nsIHttpChannel_VisitRequestHeaders(This->http_channel, aVisitor);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetAllowPipelining(nsIHttpChannel *iface, PRBool *aAllowPipelining)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aAllowPipelining);

    if(This->http_channel)
        return nsIHttpChannel_GetAllowPipelining(This->http_channel, aAllowPipelining);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetAllowPipelining(nsIHttpChannel *iface, PRBool aAllowPipelining)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%x)\n", This, aAllowPipelining);

    if(This->http_channel)
        return nsIHttpChannel_SetAllowPipelining(This->http_channel, aAllowPipelining);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetRedirectionLimit(nsIHttpChannel *iface, PRUint32 *aRedirectionLimit)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aRedirectionLimit);

    if(This->http_channel)
        return nsIHttpChannel_GetRedirectionLimit(This->http_channel, aRedirectionLimit);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetRedirectionLimit(nsIHttpChannel *iface, PRUint32 aRedirectionLimit)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%lu)\n", This, aRedirectionLimit);

    if(This->http_channel)
        return nsIHttpChannel_SetRedirectionLimit(This->http_channel, aRedirectionLimit);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetResponseStatus(nsIHttpChannel *iface, PRUint32 *aResponseStatus)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aResponseStatus);

    if(This->http_channel)
        return nsIHttpChannel_GetResponseStatus(This->http_channel, aResponseStatus);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetResponseStatusText(nsIHttpChannel *iface,
                                                      nsACString *aResponseStatusText)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aResponseStatusText);

    if(This->http_channel)
        return nsIHttpChannel_GetResponseStatusText(This->http_channel, aResponseStatusText);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetRequestSucceeded(nsIHttpChannel *iface,
                                                    PRBool *aRequestSucceeded)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aRequestSucceeded);

    if(This->http_channel)
        return nsIHttpChannel_GetRequestSucceeded(This->http_channel, aRequestSucceeded);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_GetResponseHeader(nsIHttpChannel *iface,
         const nsACString *header, nsACString *_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p %p)\n", This, header, _retval);

    if(This->http_channel)
        return nsIHttpChannel_GetResponseHeader(This->http_channel, header, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_SetResponseHeader(nsIHttpChannel *iface,
        const nsACString *header, const nsACString *value, PRBool merge)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p %p %x)\n", This, header, value, merge);

    if(This->http_channel)
        return nsIHttpChannel_SetResponseHeader(This->http_channel, header, value, merge);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_VisitResponseHeaders(nsIHttpChannel *iface,
        nsIHttpHeaderVisitor *aVisitor)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aVisitor);

    if(This->http_channel)
        return nsIHttpChannel_VisitResponseHeaders(This->http_channel, aVisitor);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_IsNoStoreResponse(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, _retval);

    if(This->http_channel)
        return nsIHttpChannel_IsNoStoreResponse(This->http_channel, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsChannel_IsNoCacheResponse(nsIHttpChannel *iface, PRBool *_retval)
{
    nsChannel *This = NSCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, _retval);

    if(This->http_channel)
        return nsIHttpChannel_IsNoCacheResponse(This->http_channel, _retval);

    return NS_ERROR_NOT_IMPLEMENTED;
}

#undef NSCHANNEL_THIS

static const nsIHttpChannelVtbl nsChannelVtbl = {
    nsChannel_QueryInterface,
    nsChannel_AddRef,
    nsChannel_Release,
    nsChannel_GetName,
    nsChannel_IsPending,
    nsChannel_GetStatus,
    nsChannel_Cancel,
    nsChannel_Suspend,
    nsChannel_Resume,
    nsChannel_GetLoadGroup,
    nsChannel_SetLoadGroup,
    nsChannel_GetLoadFlags,
    nsChannel_SetLoadFlags,
    nsChannel_GetOriginalURI,
    nsChannel_SetOriginalURI,
    nsChannel_GetURI,
    nsChannel_GetOwner,
    nsChannel_SetOwner,
    nsChannel_GetNotificationCallbacks,
    nsChannel_SetNotificationCallbacks,
    nsChannel_GetSecurityInfo,
    nsChannel_GetContentType,
    nsChannel_SetContentType,
    nsChannel_GetContentCharset,
    nsChannel_SetContentCharset,
    nsChannel_GetContentLength,
    nsChannel_SetContentLength,
    nsChannel_Open,
    nsChannel_AsyncOpen,
    nsChannel_GetRequestMethod,
    nsChannel_SetRequestMethod,
    nsChannel_GetReferrer,
    nsChannel_SetReferrer,
    nsChannel_GetRequestHeader,
    nsChannel_SetRequestHeader,
    nsChannel_VisitRequestHeaders,
    nsChannel_GetAllowPipelining,
    nsChannel_SetAllowPipelining,
    nsChannel_GetRedirectionLimit,
    nsChannel_SetRedirectionLimit,
    nsChannel_GetResponseStatus,
    nsChannel_GetResponseStatusText,
    nsChannel_GetRequestSucceeded,
    nsChannel_GetResponseHeader,
    nsChannel_SetResponseHeader,
    nsChannel_VisitResponseHeaders,
    nsChannel_IsNoStoreResponse,
    nsChannel_IsNoCacheResponse
};

#define NSUPCHANNEL_THIS(iface) DEFINE_THIS(nsChannel, UploadChannel, iface)

static nsresult NSAPI nsUploadChannel_QueryInterface(nsIUploadChannel *iface, nsIIDRef riid,
                                                     nsQIResult result)
{
    nsChannel *This = NSUPCHANNEL_THIS(iface);
    return nsIChannel_QueryInterface(NSCHANNEL(This), riid, result);
}

static nsrefcnt NSAPI nsUploadChannel_AddRef(nsIUploadChannel *iface)
{
    nsChannel *This = NSUPCHANNEL_THIS(iface);
    return nsIChannel_AddRef(NSCHANNEL(This));
}

static nsrefcnt NSAPI nsUploadChannel_Release(nsIUploadChannel *iface)
{
    nsChannel *This = NSUPCHANNEL_THIS(iface);
    return nsIChannel_Release(NSCHANNEL(This));
}

static nsresult NSAPI nsUploadChannel_SetUploadStream(nsIUploadChannel *iface,
        nsIInputStream *aStream, const nsACString *aContentType, PRInt32 aContentLength)
{
    nsChannel *This = NSUPCHANNEL_THIS(iface);
    const char *content_type;

    TRACE("(%p)->(%p %p %ld)\n", This, aStream, aContentType, aContentLength);

    if(This->post_data_stream)
        nsIInputStream_Release(This->post_data_stream);

    if(aContentType) {
        nsACString_GetData(aContentType, &content_type, NULL);
        if(*content_type)
            FIXME("Unsupported aContentType argument: %s\n", debugstr_a(content_type));
    }

    if(aContentLength != -1)
        FIXME("Unsupported acontentLength = %ld\n", aContentLength);

    if(aStream)
        nsIInputStream_AddRef(aStream);
    This->post_data_stream = aStream;

    return NS_OK;
}

static nsresult NSAPI nsUploadChannel_GetUploadStream(nsIUploadChannel *iface,
        nsIInputStream **aUploadStream)
{
    nsChannel *This = NSUPCHANNEL_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aUploadStream);

    if(This->post_data_stream)
        nsIInputStream_AddRef(This->post_data_stream);

    *aUploadStream = This->post_data_stream;
    return NS_OK;
}

#undef NSUPCHANNEL_THIS

static const nsIUploadChannelVtbl nsUploadChannelVtbl = {
    nsUploadChannel_QueryInterface,
    nsUploadChannel_AddRef,
    nsUploadChannel_Release,
    nsUploadChannel_SetUploadStream,
    nsUploadChannel_GetUploadStream
};

#define NSURI_THIS(iface) DEFINE_THIS(nsURI, WineURI, iface)

static nsresult NSAPI nsURI_QueryInterface(nsIWineURI *iface, nsIIDRef riid, nsQIResult result)
{
    nsURI *This = NSURI_THIS(iface);

    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(%p)->(IID_nsISupports %p)\n", This, result);
        *result = NSURI(This);
    }else if(IsEqualGUID(&IID_nsIURI, riid)) {
        TRACE("(%p)->(IID_nsIURI %p)\n", This, result);
        *result = NSURI(This);
    }else if(IsEqualGUID(&IID_nsIWineURI, riid)) {
        TRACE("(%p)->(IID_nsIWineURI %p)\n", This, result);
        *result = NSURI(This);
    }

    if(*result) {
        nsIURI_AddRef(NSURI(This));
        return NS_OK;
    }

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), result);
    return This->uri ? nsIURI_QueryInterface(This->uri, riid, result) : NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsURI_AddRef(nsIWineURI *iface)
{
    nsURI *This = NSURI_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static nsrefcnt NSAPI nsURI_Release(nsIWineURI *iface)
{
    nsURI *This = NSURI_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if(!ref) {
        if(This->container)
            nsIWebBrowserChrome_Release(NSWBCHROME(This->container));
        if(This->uri)
            nsIURI_Release(This->uri);
        if(This->mon)
            IMoniker_Release(This->mon);
        mshtml_free(This->spec);
        mshtml_free(This);
    }

    return ref;
}

static nsresult NSAPI nsURI_GetSpec(nsIWineURI *iface, nsACString *aSpec)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aSpec);

    if(This->uri)
        return nsIURI_GetSpec(This->uri, aSpec);

    if(This->spec) {
        nsACString_Init(aSpec, This->spec);
        return NS_OK;
    }

    WARN("mon and uri are NULL\n");
    return NS_ERROR_NOT_IMPLEMENTED;

}

static nsresult NSAPI nsURI_SetSpec(nsIWineURI *iface, const nsACString *aSpec)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aSpec);

    if(This->uri)
        return nsIURI_SetSpec(This->uri, aSpec);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetPrePath(nsIWineURI *iface, nsACString *aPrePath)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPrePath);

    if(This->uri)
        return nsIURI_GetPrePath(This->uri, aPrePath);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetScheme(nsIWineURI *iface, nsACString *aScheme)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aScheme);

    if(This->uri)
        return nsIURI_GetScheme(This->uri, aScheme);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetScheme(nsIWineURI *iface, const nsACString *aScheme)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aScheme);

    if(This->uri)
        return nsIURI_SetScheme(This->uri, aScheme);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetUserPass(nsIWineURI *iface, nsACString *aUserPass)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aUserPass);

    if(This->uri)
        return nsIURI_GetUserPass(This->uri, aUserPass);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetUserPass(nsIWineURI *iface, const nsACString *aUserPass)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aUserPass);

    if(This->uri)
        return nsIURI_SetUserPass(This->uri, aUserPass);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetUsername(nsIWineURI *iface, nsACString *aUsername)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aUsername);

    if(This->uri)
        return nsIURI_GetUsername(This->uri, aUsername);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetUsername(nsIWineURI *iface, const nsACString *aUsername)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aUsername);

    if(This->uri)
        return nsIURI_SetUsername(This->uri, aUsername);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetPassword(nsIWineURI *iface, nsACString *aPassword)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPassword);

    if(This->uri)
        return nsIURI_GetPassword(This->uri, aPassword);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetPassword(nsIWineURI *iface, const nsACString *aPassword)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPassword);

    if(This->uri)
        return nsIURI_SetPassword(This->uri, aPassword);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetHostPort(nsIWineURI *iface, nsACString *aHostPort)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aHostPort);

    if(This->uri)
        return nsIURI_GetHostPort(This->uri, aHostPort);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetHostPort(nsIWineURI *iface, const nsACString *aHostPort)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aHostPort);

    if(This->uri)
        return nsIURI_SetHostPort(This->uri, aHostPort);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetHost(nsIWineURI *iface, nsACString *aHost)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aHost);

    if(This->uri)
        return nsIURI_GetHost(This->uri, aHost);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetHost(nsIWineURI *iface, const nsACString *aHost)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aHost);

    if(This->uri)
        return nsIURI_SetHost(This->uri, aHost);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetPort(nsIWineURI *iface, PRInt32 *aPort)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPort);

    if(This->uri)
        return nsIURI_GetPort(This->uri, aPort);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetPort(nsIWineURI *iface, PRInt32 aPort)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%ld)\n", This, aPort);

    if(This->uri)
        return nsIURI_SetPort(This->uri, aPort);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetPath(nsIWineURI *iface, nsACString *aPath)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPath);

    if(This->uri)
        return nsIURI_GetPath(This->uri, aPath);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SetPath(nsIWineURI *iface, const nsACString *aPath)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aPath);

    if(This->uri)
        return nsIURI_SetPath(This->uri, aPath);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_Equals(nsIWineURI *iface, nsIURI *other, PRBool *_retval)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p %p)\n", This, other, _retval);

    if(This->uri)
        return nsIURI_Equals(This->uri, other, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_SchemeIs(nsIWineURI *iface, const char *scheme, PRBool *_retval)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_a(scheme), _retval);

    if(This->uri)
        return nsIURI_SchemeIs(This->uri, scheme, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_Clone(nsIWineURI *iface, nsIURI **_retval)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, _retval);

    if(This->uri) {
        nsIURI *uri;
        nsresult nsres;

        nsres = nsIURI_Clone(This->uri, &uri);
        if(NS_FAILED(nsres)) {
            WARN("Clone failed: %08lx\n", nsres);
            return nsres;
        }

        return create_uri(uri, This->container, _retval);
    }

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_Resolve(nsIWineURI *iface, const nsACString *arelativePath,
        nsACString *_retval)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p %p)\n", This, arelativePath, _retval);

    if(This->uri)
        return nsIURI_Resolve(This->uri, arelativePath, _retval);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetAsciiSpec(nsIWineURI *iface, nsACString *aAsciiSpec)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aAsciiSpec);

    if(This->uri)
        return nsIURI_GetAsciiSpec(This->uri, aAsciiSpec);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetAsciiHost(nsIWineURI *iface, nsACString *aAsciiHost)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aAsciiHost);

    if(This->uri)
        return nsIURI_GetAsciiHost(This->uri, aAsciiHost);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetOriginCharset(nsIWineURI *iface, nsACString *aOriginCharset)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aOriginCharset);

    if(This->uri)
        return nsIURI_GetOriginCharset(This->uri, aOriginCharset);

    FIXME("default action not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult NSAPI nsURI_GetNSContainer(nsIWineURI *iface, NSContainer **aContainer)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContainer);

    if(This->container)
        nsIWebBrowserChrome_AddRef(NSWBCHROME(This->container));
    *aContainer = This->container;

    return NS_OK;
}

static nsresult NSAPI nsURI_SetNSContainer(nsIWineURI *iface, NSContainer *aContainer)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aContainer);

    if(This->container) {
        WARN("Container already set: %p\n", This->container);
        nsIWebBrowserChrome_Release(NSWBCHROME(This->container));
    }

    if(aContainer)
        nsIWebBrowserChrome_AddRef(NSWBCHROME(aContainer));
    This->container = aContainer;

    return NS_OK;
}

static nsresult NSAPI nsURI_GetMoniker(nsIWineURI *iface, IMoniker **aMoniker)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aMoniker);

    if(This->mon)
        IMoniker_AddRef(This->mon);
    *aMoniker = This->mon;

    return NS_OK;
}

static nsresult NSAPI nsURI_SetMoniker(nsIWineURI *iface, IMoniker *aMoniker)
{
    nsURI *This = NSURI_THIS(iface);

    TRACE("(%p)->(%p)\n", This, aMoniker);

    if(This->mon) {
        WARN("Moniker already set: %p\n", This->container);
        IMoniker_Release(This->mon);

        mshtml_free(This->spec);
        This->spec = NULL;
    }

    if(aMoniker) {
        LPWSTR url = NULL;
        HRESULT hres;

        hres = IMoniker_GetDisplayName(aMoniker, NULL, NULL, &url);
        if(SUCCEEDED(hres)) {
            DWORD len;

            len = WideCharToMultiByte(CP_ACP, 0, url, -1, NULL, 0, NULL, NULL);
            This->spec = mshtml_alloc(len*sizeof(WCHAR));
            WideCharToMultiByte(CP_ACP, 0, url, -1, This->spec, -1, NULL, NULL);
            CoTaskMemFree(url);

            TRACE("spec %s\n", debugstr_a(This->spec));
        }else {
            ERR("GetDisplayName failed: %08lx\n", hres);
        }

        IMoniker_AddRef(aMoniker);
    }
    This->mon = aMoniker;

    return NS_OK;
}

#undef NSURI_THIS

static const nsIWineURIVtbl nsWineURIVtbl = {
    nsURI_QueryInterface,
    nsURI_AddRef,
    nsURI_Release,
    nsURI_GetSpec,
    nsURI_SetSpec,
    nsURI_GetPrePath,
    nsURI_GetScheme,
    nsURI_SetScheme,
    nsURI_GetUserPass,
    nsURI_SetUserPass,
    nsURI_GetUsername,
    nsURI_SetUsername,
    nsURI_GetPassword,
    nsURI_SetPassword,
    nsURI_GetHostPort,
    nsURI_SetHostPort,
    nsURI_GetHost,
    nsURI_SetHost,
    nsURI_GetPort,
    nsURI_SetPort,
    nsURI_GetPath,
    nsURI_SetPath,
    nsURI_Equals,
    nsURI_SchemeIs,
    nsURI_Clone,
    nsURI_Resolve,
    nsURI_GetAsciiSpec,
    nsURI_GetAsciiHost,
    nsURI_GetOriginCharset,
    nsURI_GetNSContainer,
    nsURI_SetNSContainer,
    nsURI_GetMoniker,
    nsURI_SetMoniker
};

static nsresult create_uri(nsIURI *uri, NSContainer *container, nsIURI **_retval)
{
    nsURI *ret = mshtml_alloc(sizeof(nsURI));

    ret->lpWineURIVtbl = &nsWineURIVtbl;
    ret->ref = 1;
    ret->uri = uri;
    ret->container = container;
    ret->mon = NULL;
    ret->spec = NULL;

    if(container)
        nsIWebBrowserChrome_AddRef(NSWBCHROME(container));

    TRACE("retval=%p\n", ret);
    *_retval = NSURI(ret);
    return NS_OK;
}

static nsresult NSAPI nsIOService_QueryInterface(nsIIOService *iface, nsIIDRef riid,
                                                 nsQIResult result)
{
    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(IID_nsISupports %p)\n", result);
        *result = iface;
    }else if(IsEqualGUID(&IID_nsIIOService, riid)) {
        TRACE("(IID_nsIIOService %p)\n", result);
        *result = iface;
    }

    if(*result) {
        nsIIOService_AddRef(iface);
        return S_OK;
    }

    WARN("(%s %p)\n", debugstr_guid(riid), result);
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsIOService_AddRef(nsIIOService *iface)
{
    return 2;
}

static nsrefcnt NSAPI nsIOService_Release(nsIIOService *iface)
{
    return 1;
}

static nsresult NSAPI nsIOService_GetProtocolHandler(nsIIOService *iface, const char *aScheme,
                                                     nsIProtocolHandler **_retval)
{
    TRACE("(%s %p)\n", debugstr_a(aScheme), _retval);
    return nsIIOService_GetProtocolHandler(nsio, aScheme, _retval);
}

static nsresult NSAPI nsIOService_GetProtocolFlags(nsIIOService *iface, const char *aScheme,
                                                    PRUint32 *_retval)
{
    TRACE("(%s %p)\n", debugstr_a(aScheme), _retval);
    return nsIIOService_GetProtocolFlags(nsio, aScheme, _retval);
}

static nsresult NSAPI nsIOService_NewURI(nsIIOService *iface, const nsACString *aSpec,
        const char *aOriginCharset, nsIURI *aBaseURI, nsIURI **_retval)
{
    const char *spec = NULL;
    NSContainer *nscontainer = NULL;
    nsIURI *uri = NULL;
    PRBool is_javascript = FALSE;
    IMoniker *base_mon = NULL;
    nsresult nsres;

    nsACString_GetData(aSpec, &spec, NULL);

    TRACE("(%p(%s) %s %p %p)\n", aSpec, debugstr_a(spec), debugstr_a(aOriginCharset),
          aBaseURI, _retval);

    if(aBaseURI) {
        nsACString base_uri_str;
        const char *base_uri = NULL;

        nsACString_Init(&base_uri_str, NULL);

        nsres = nsIURI_GetSpec(aBaseURI, &base_uri_str);
        if(NS_SUCCEEDED(nsres)) {
            nsACString_GetData(&base_uri_str, &base_uri, NULL);
            TRACE("uri=%s\n", debugstr_a(base_uri));
        }else {
            ERR("GetSpec failed: %08lx\n", nsres);
        }

        nsACString_Finish(&base_uri_str);
    }

    nsres = nsIIOService_NewURI(nsio, aSpec, aOriginCharset, aBaseURI, &uri);
    if(NS_FAILED(nsres))
        TRACE("NewURI failed: %08lx\n", nsres);

    if(uri) {
        nsIURI_SchemeIs(uri, "javascript", &is_javascript);
        if(is_javascript) {
            TRACE("returning javascript uri: %p\n", uri);
            *_retval = uri;
            return NS_OK;
        }
    }

    if(aBaseURI) {
        nsIWineURI *wine_uri;

        nsres = nsIURI_QueryInterface(aBaseURI, &IID_nsIWineURI, (void**)&wine_uri);
        if(NS_SUCCEEDED(nsres)) {
            nsIWineURI_GetNSContainer(wine_uri, &nscontainer);
            nsIWineURI_GetMoniker(wine_uri, &base_mon);
            nsIWineURI_Release(wine_uri);
        }else {
            ERR("Could not get nsIWineURI: %08lx\n", nsres);
        }
    }

    nsres = create_uri(uri, nscontainer, _retval);

    if(nscontainer)
        nsIWebBrowserChrome_Release(NSWBCHROME(nscontainer));

    if(base_mon) {
        LPWSTR url;
        IMoniker *mon;
        DWORD len;
        HRESULT hres;

        len = MultiByteToWideChar(CP_ACP, 0, spec, -1, NULL, 0);
        url = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, spec, -1, url, -1);

        hres = CreateURLMoniker(base_mon, url, &mon);
        HeapFree(GetProcessHeap(), 0, url);
        if(SUCCEEDED(hres)) {
            nsIWineURI_SetMoniker((nsIWineURI*)*_retval, mon);
            IMoniker_Release(mon);
        }else {
            WARN("CreateURLMoniker failed: %08lx\n", hres);
        }
    }

    return nsres;
}

static nsresult NSAPI nsIOService_NewFileURI(nsIIOService *iface, nsIFile *aFile,
                                             nsIURI **_retval)
{
    TRACE("(%p %p)\n", aFile, _retval);
    return nsIIOService_NewFileURI(nsio, aFile, _retval);
}

static nsresult NSAPI nsIOService_NewChannelFromURI(nsIIOService *iface, nsIURI *aURI,
                                                     nsIChannel **_retval)
{
    nsIChannel *channel = NULL;
    nsChannel *ret;
    nsIWineURI *wine_uri;
    nsresult nsres;

    TRACE("(%p %p)\n", aURI, _retval);

    nsres = nsIIOService_NewChannelFromURI(nsio, aURI, &channel);
    if(NS_FAILED(nsres) && nsres != NS_ERROR_UNKNOWN_PROTOCOL) {
        WARN("NewChannelFromURI failed: %08lx\n", nsres);
        *_retval = channel;
        return nsres;
    }

    nsres = nsIURI_QueryInterface(aURI, &IID_nsIWineURI, (void**)&wine_uri);
    if(NS_FAILED(nsres)) {
        WARN("Could not get nsIWineURI: %08lx\n", nsres);
        *_retval = channel;
        return channel ? NS_OK : NS_ERROR_UNEXPECTED;
    }

    ret = mshtml_alloc(sizeof(nsChannel));

    ret->lpHttpChannelVtbl = &nsChannelVtbl;
    ret->lpUploadChannelVtbl = &nsUploadChannelVtbl;
    ret->ref = 1;
    ret->channel = channel;
    ret->http_channel = NULL;
    ret->uri = wine_uri;
    ret->post_data_stream = NULL;
    ret->load_group = NULL;
    ret->notif_callback = NULL;
    ret->load_flags = 0;
    ret->content = NULL;

    nsIURI_AddRef(aURI);
    ret->original_uri = aURI;

    if(channel)
        nsIChannel_QueryInterface(channel, &IID_nsIHttpChannel, (void**)&ret->http_channel);

    *_retval = NSCHANNEL(ret);
    return NS_OK;
}

static nsresult NSAPI nsIOService_NewChannel(nsIIOService *iface, const nsACString *aSpec,
        const char *aOriginCharset, nsIURI *aBaseURI, nsIChannel **_retval)
{
    TRACE("(%p %s %p %p)\n", aSpec, debugstr_a(aOriginCharset), aBaseURI, _retval);
    return nsIIOService_NewChannel(nsio, aSpec, aOriginCharset, aBaseURI, _retval);
}

static nsresult NSAPI nsIOService_GetOffline(nsIIOService *iface, PRBool *aOffline)
{
    TRACE("(%p)\n", aOffline);
    return nsIIOService_GetOffline(nsio, aOffline);
}

static nsresult NSAPI nsIOService_SetOffline(nsIIOService *iface, PRBool aOffline)
{
    TRACE("(%x)\n", aOffline);
    return nsIIOService_SetOffline(nsio, aOffline);
}

static nsresult NSAPI nsIOService_AllowPort(nsIIOService *iface, PRInt32 aPort,
                                             const char *aScheme, PRBool *_retval)
{
    TRACE("(%ld %s %p)\n", aPort, debugstr_a(aScheme), _retval);
    return nsIIOService_AllowPort(nsio, aPort, debugstr_a(aScheme), _retval);
}

static nsresult NSAPI nsIOService_ExtractScheme(nsIIOService *iface, const nsACString *urlString,
                                                 nsACString * _retval)
{
    TRACE("(%p %p)\n", urlString, _retval);
    return nsIIOService_ExtractScheme(nsio, urlString, _retval);
}

static const nsIIOServiceVtbl nsIOServiceVtbl = {
    nsIOService_QueryInterface,
    nsIOService_AddRef,
    nsIOService_Release,
    nsIOService_GetProtocolHandler,
    nsIOService_GetProtocolFlags,
    nsIOService_NewURI,
    nsIOService_NewFileURI,
    nsIOService_NewChannelFromURI,
    nsIOService_NewChannel,
    nsIOService_GetOffline,
    nsIOService_SetOffline,
    nsIOService_AllowPort,
    nsIOService_ExtractScheme
};

static nsIIOService nsIOService = { &nsIOServiceVtbl };

static nsresult NSAPI nsIOServiceFactory_QueryInterface(nsIFactory *iface, nsIIDRef riid,
                                                        nsQIResult result)
{
    *result = NULL;

    if(IsEqualGUID(&IID_nsISupports, riid)) {
        TRACE("(IID_nsISupoprts %p)\n", result);
        *result = iface;
    }else if(IsEqualGUID(&IID_nsIFactory, riid)) {
        TRACE("(IID_nsIFactory %p)\n", result);
        *result = iface;
    }

    if(*result) {
        nsIFactory_AddRef(iface);
        return NS_OK;
    }

    WARN("(%s %p)\n", debugstr_guid(riid), result);
    return NS_NOINTERFACE;
}

static nsrefcnt NSAPI nsIOServiceFactory_AddRef(nsIFactory *iface)
{
    return 2;
}

static nsrefcnt NSAPI nsIOServiceFactory_Release(nsIFactory *iface)
{
    return 1;
}

static nsresult NSAPI nsIOServiceFactory_CreateInstance(nsIFactory *iface,
        nsISupports *aOuter, const nsIID *iid, void **result)
{
    return nsIIOService_QueryInterface(&nsIOService, iid, result);
}

static nsresult NSAPI nsIOServiceFactory_LockFactory(nsIFactory *iface, PRBool lock)
{
    WARN("(%x)\n", lock);
    return NS_OK;
}

static const nsIFactoryVtbl nsIOServiceFactoryVtbl = {
    nsIOServiceFactory_QueryInterface,
    nsIOServiceFactory_AddRef,
    nsIOServiceFactory_Release,
    nsIOServiceFactory_CreateInstance,
    nsIOServiceFactory_LockFactory
};

static nsIFactory nsIOServiceFactory = { &nsIOServiceFactoryVtbl };

void init_nsio(nsIComponentManager *component_manager, nsIComponentRegistrar *registrar)
{
    nsIFactory *old_factory = NULL;
    nsresult nsres;

    nsres = nsIComponentManager_GetClassObject(component_manager, &NS_IOSERVICE_CID,
                                               &IID_nsIFactory, (void**)&old_factory);
    if(NS_FAILED(nsres)) {
        ERR("Could not get factory: %08lx\n", nsres);
        nsIFactory_Release(old_factory);
        return;
    }

    nsres = nsIFactory_CreateInstance(old_factory, NULL, &IID_nsIIOService, (void**)&nsio);
    if(NS_FAILED(nsres)) {
        ERR("Couldn not create nsIOService instance %08lx\n", nsres);
        nsIFactory_Release(old_factory);
        return;
    }

    nsres = nsIComponentRegistrar_UnregisterFactory(registrar, &NS_IOSERVICE_CID, old_factory);
    nsIFactory_Release(old_factory);
    if(NS_FAILED(nsres))
        ERR("UnregisterFactory failed: %08lx\n", nsres);

    nsres = nsIComponentRegistrar_RegisterFactory(registrar, &NS_IOSERVICE_CID,
            NS_IOSERVICE_CLASSNAME, NS_IOSERVICE_CONTRACTID, &nsIOServiceFactory);
    if(NS_FAILED(nsres))
        ERR("RegisterFactory failed: %08lx\n", nsres);
}

nsIURI *get_nsIURI(LPCWSTR url)
{
    nsIURI *ret;
    nsACString acstr;
    nsresult nsres;
    char *urla;
    int len;

    len = WideCharToMultiByte(CP_ACP, 0, url, -1, NULL, -1, NULL, NULL);
    urla = mshtml_alloc(len);
    WideCharToMultiByte(CP_ACP, 0, url, -1, urla, -1, NULL, NULL);

    nsACString_Init(&acstr, urla);

    nsres = nsIIOService_NewURI(nsio, &acstr, NULL, NULL, &ret);
    if(NS_FAILED(nsres))
        FIXME("NewURI failed: %08lx\n", nsres);

    nsACString_Finish(&acstr);
    mshtml_free(urla);

    return ret;
}
