/*
 * RPC binding API
 *
 * Copyright 2001 Ove K�ven, TransGaming Technologies
 * Copyright 2003 Mike Hearn
 * Copyright 2004 Filip Navara
 * Copyright 2006 CodeWeavers
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/unicode.h"

#include "rpc.h"
#include "rpcndr.h"

#include "wine/debug.h"

#include "rpc_binding.h"
#include "rpc_message.h"

WINE_DEFAULT_DEBUG_CHANNEL(rpc);

LPSTR RPCRT4_strndupA(LPCSTR src, INT slen)
{
  DWORD len;
  LPSTR s;
  if (!src) return NULL;
  if (slen == -1) slen = strlen(src);
  len = slen;
  s = HeapAlloc(GetProcessHeap(), 0, len+1);
  memcpy(s, src, len);
  s[len] = 0;
  return s;
}

LPSTR RPCRT4_strdupWtoA(LPWSTR src)
{
  DWORD len;
  LPSTR s;
  if (!src) return NULL;
  len = WideCharToMultiByte(CP_ACP, 0, src, -1, NULL, 0, NULL, NULL);
  s = HeapAlloc(GetProcessHeap(), 0, len);
  WideCharToMultiByte(CP_ACP, 0, src, -1, s, len, NULL, NULL);
  return s;
}

LPWSTR RPCRT4_strdupAtoW(LPSTR src)
{
  DWORD len;
  LPWSTR s;
  if (!src) return NULL;
  len = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
  s = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
  MultiByteToWideChar(CP_ACP, 0, src, -1, s, len);
  return s;
}

LPWSTR RPCRT4_strndupW(LPWSTR src, INT slen)
{
  DWORD len;
  LPWSTR s;
  if (!src) return NULL;
  if (slen == -1) slen = strlenW(src);
  len = slen;
  s = HeapAlloc(GetProcessHeap(), 0, (len+1)*sizeof(WCHAR));
  memcpy(s, src, len*sizeof(WCHAR));
  s[len] = 0;
  return s;
}

void RPCRT4_strfree(LPSTR src)
{
  HeapFree(GetProcessHeap(), 0, src);
}

static RPC_STATUS RPCRT4_AllocBinding(RpcBinding** Binding, BOOL server)
{
  RpcBinding* NewBinding;

  NewBinding = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RpcBinding));
  NewBinding->refs = 1;
  NewBinding->server = server;

  *Binding = NewBinding;

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_CreateBindingA(RpcBinding** Binding, BOOL server, LPSTR Protseq)
{
  RpcBinding* NewBinding;

  RPCRT4_AllocBinding(&NewBinding, server);
  NewBinding->Protseq = RPCRT4_strdupA(Protseq);

  TRACE("binding: %p\n", NewBinding);
  *Binding = NewBinding;

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_CreateBindingW(RpcBinding** Binding, BOOL server, LPWSTR Protseq)
{
  RpcBinding* NewBinding;

  RPCRT4_AllocBinding(&NewBinding, server);
  NewBinding->Protseq = RPCRT4_strdupWtoA(Protseq);

  TRACE("binding: %p\n", NewBinding);
  *Binding = NewBinding;

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_CompleteBindingA(RpcBinding* Binding, LPSTR NetworkAddr,  LPSTR Endpoint,  LPSTR NetworkOptions)
{
  TRACE("(RpcBinding == ^%p, NetworkAddr == \"%s\", EndPoint == \"%s\", NetworkOptions == \"%s\")\n", Binding,
   debugstr_a(NetworkAddr), debugstr_a(Endpoint), debugstr_a(NetworkOptions));

  RPCRT4_strfree(Binding->NetworkAddr);
  Binding->NetworkAddr = RPCRT4_strdupA(NetworkAddr);
  RPCRT4_strfree(Binding->Endpoint);
  if (Endpoint) {
    Binding->Endpoint = RPCRT4_strdupA(Endpoint);
  } else {
    Binding->Endpoint = RPCRT4_strdupA("");
  }
  if (!Binding->Endpoint) ERR("out of memory?\n");

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_CompleteBindingW(RpcBinding* Binding, LPWSTR NetworkAddr, LPWSTR Endpoint, LPWSTR NetworkOptions)
{
  TRACE("(RpcBinding == ^%p, NetworkAddr == \"%s\", EndPoint == \"%s\", NetworkOptions == \"%s\")\n", Binding, 
   debugstr_w(NetworkAddr), debugstr_w(Endpoint), debugstr_w(NetworkOptions));

  RPCRT4_strfree(Binding->NetworkAddr);
  Binding->NetworkAddr = RPCRT4_strdupWtoA(NetworkAddr);
  RPCRT4_strfree(Binding->Endpoint);
  if (Endpoint) {
    Binding->Endpoint = RPCRT4_strdupWtoA(Endpoint);
  } else {
    Binding->Endpoint = RPCRT4_strdupA("");
  }
  if (!Binding->Endpoint) ERR("out of memory?\n");

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_ResolveBinding(RpcBinding* Binding, LPSTR Endpoint)
{
  TRACE("(RpcBinding == ^%p, EndPoint == \"%s\"\n", Binding, Endpoint);

  RPCRT4_strfree(Binding->Endpoint);
  Binding->Endpoint = RPCRT4_strdupA(Endpoint);

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_SetBindingObject(RpcBinding* Binding, UUID* ObjectUuid)
{
  TRACE("(*RpcBinding == ^%p, UUID == %s)\n", Binding, debugstr_guid(ObjectUuid)); 
  if (ObjectUuid) memcpy(&Binding->ObjectUuid, ObjectUuid, sizeof(UUID));
  else UuidCreateNil(&Binding->ObjectUuid);
  return RPC_S_OK;
}

RPC_STATUS RPCRT4_MakeBinding(RpcBinding** Binding, RpcConnection* Connection)
{
  RpcBinding* NewBinding;
  TRACE("(RpcBinding == ^%p, Connection == ^%p)\n", Binding, Connection);

  RPCRT4_AllocBinding(&NewBinding, Connection->server);
  NewBinding->Protseq = RPCRT4_strdupA(rpcrt4_conn_get_name(Connection));
  NewBinding->NetworkAddr = RPCRT4_strdupA(Connection->NetworkAddr);
  NewBinding->Endpoint = RPCRT4_strdupA(Connection->Endpoint);
  NewBinding->FromConn = Connection;

  TRACE("binding: %p\n", NewBinding);
  *Binding = NewBinding;

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_ExportBinding(RpcBinding** Binding, RpcBinding* OldBinding)
{
  InterlockedIncrement(&OldBinding->refs);
  *Binding = OldBinding;
  return RPC_S_OK;
}

RPC_STATUS RPCRT4_DestroyBinding(RpcBinding* Binding)
{
  if (InterlockedDecrement(&Binding->refs))
    return RPC_S_OK;

  TRACE("binding: %p\n", Binding);
  /* FIXME: release connections */
  RPCRT4_strfree(Binding->Endpoint);
  RPCRT4_strfree(Binding->NetworkAddr);
  RPCRT4_strfree(Binding->Protseq);
  HeapFree(GetProcessHeap(), 0, Binding);
  return RPC_S_OK;
}

RPC_STATUS RPCRT4_OpenBinding(RpcBinding* Binding, RpcConnection** Connection,
                              PRPC_SYNTAX_IDENTIFIER TransferSyntax,
                              PRPC_SYNTAX_IDENTIFIER InterfaceId)
{
  RpcConnection* NewConnection;
  RPC_STATUS status;

  TRACE("(Binding == ^%p)\n", Binding);

  if (!Binding->server) {
    /* try to find a compatible connection from the connection pool */
    NewConnection = RPCRT4_GetIdleConnection(InterfaceId, TransferSyntax,
        Binding->Protseq, Binding->NetworkAddr, Binding->Endpoint,
        Binding->AuthInfo);
    if (NewConnection) {
      *Connection = NewConnection;
      return RPC_S_OK;
    }
  } else {
    /* we already have a connection with acceptable binding, so use it */
    if (Binding->FromConn) {
      *Connection = Binding->FromConn;
      return RPC_S_OK;
    }
  }
  
  /* create a new connection */
  RPCRT4_CreateConnection(&NewConnection, Binding->server, Binding->Protseq,
                          Binding->NetworkAddr, Binding->Endpoint, NULL,
                          Binding->AuthInfo, Binding);
  status = RPCRT4_OpenConnection(NewConnection);
  if (status != RPC_S_OK)
  {
    RPCRT4_DestroyConnection(NewConnection);
    return status;
  }
 
  /* we need to send a binding packet if we are client. */
  if (!NewConnection->server) {
    RpcPktHdr *hdr;
    RpcPktHdr *response_hdr;
    RPC_MESSAGE msg;

    TRACE("sending bind request to server\n");

    hdr = RPCRT4_BuildBindHeader(NDR_LOCAL_DATA_REPRESENTATION,
                                 RPC_MAX_PACKET_SIZE, RPC_MAX_PACKET_SIZE,
                                 InterfaceId, TransferSyntax);

    status = RPCRT4_Send(NewConnection, hdr, NULL, 0);
    RPCRT4_FreeHeader(hdr);
    if (status != RPC_S_OK) {
      RPCRT4_DestroyConnection(NewConnection);
      return status;
    }

    status = RPCRT4_Receive(NewConnection, &response_hdr, &msg);
    if (status != RPC_S_OK) {
      ERR("receive failed\n");
      RPCRT4_DestroyConnection(NewConnection);
      return status;
    }

    if (response_hdr->common.ptype != PKT_BIND_ACK ||
        response_hdr->bind_ack.max_tsize < RPC_MIN_PACKET_SIZE) {
      ERR("failed to bind for interface %s, %d.%d\n",
        debugstr_guid(&InterfaceId->SyntaxGUID),
        InterfaceId->SyntaxVersion.MajorVersion,
        InterfaceId->SyntaxVersion.MinorVersion);
      RPCRT4_FreeHeader(response_hdr);
      RPCRT4_DestroyConnection(NewConnection);
      return RPC_S_PROTOCOL_ERROR;
    }

    /* FIXME: do more checks? */

    NewConnection->MaxTransmissionSize = response_hdr->bind_ack.max_tsize;
    NewConnection->ActiveInterface = *InterfaceId;
    RPCRT4_FreeHeader(response_hdr);
  }

  if (Binding->server)
    Binding->FromConn = NewConnection;
  *Connection = NewConnection;

  return RPC_S_OK;
}

RPC_STATUS RPCRT4_CloseBinding(RpcBinding* Binding, RpcConnection* Connection)
{
  TRACE("(Binding == ^%p)\n", Binding);
  if (!Connection) return RPC_S_OK;
  if (Binding->server) {
    /* don't destroy a connection that is cached in the binding */
    if (Binding->FromConn == Connection)
      return RPC_S_OK;
    return RPCRT4_DestroyConnection(Connection);
  }
  else {
    RPCRT4_ReleaseIdleConnection(Connection);
    return RPC_S_OK;
  }
}

/* utility functions for string composing and parsing */
static unsigned RPCRT4_strcopyA(LPSTR data, LPCSTR src)
{
  unsigned len = strlen(src);
  memcpy(data, src, len*sizeof(CHAR));
  return len;
}

static unsigned RPCRT4_strcopyW(LPWSTR data, LPCWSTR src)
{
  unsigned len = strlenW(src);
  memcpy(data, src, len*sizeof(WCHAR));
  return len;
}

static LPSTR RPCRT4_strconcatA(LPSTR dst, LPCSTR src)
{
  DWORD len = strlen(dst), slen = strlen(src);
  LPSTR ndst = HeapReAlloc(GetProcessHeap(), 0, dst, (len+slen+2)*sizeof(CHAR));
  if (!ndst)
  {
    HeapFree(GetProcessHeap(), 0, dst);
    return NULL;
  }
  ndst[len] = ',';
  memcpy(ndst+len+1, src, slen+1);
  return ndst;
}

static LPWSTR RPCRT4_strconcatW(LPWSTR dst, LPCWSTR src)
{
  DWORD len = strlenW(dst), slen = strlenW(src);
  LPWSTR ndst = HeapReAlloc(GetProcessHeap(), 0, dst, (len+slen+2)*sizeof(WCHAR));
  if (!ndst) 
  {
    HeapFree(GetProcessHeap(), 0, dst);
    return NULL;
  }
  ndst[len] = ',';
  memcpy(ndst+len+1, src, (slen+1)*sizeof(WCHAR));
  return ndst;
}


/***********************************************************************
 *             RpcStringBindingComposeA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcStringBindingComposeA(unsigned char *ObjUuid, unsigned char *Protseq,
                                           unsigned char *NetworkAddr, unsigned char *Endpoint,
                                           unsigned char *Options, unsigned char** StringBinding )
{
  DWORD len = 1;
  LPSTR data;

  TRACE( "(%s,%s,%s,%s,%s,%p)\n",
        debugstr_a( (char*)ObjUuid ), debugstr_a( (char*)Protseq ),
        debugstr_a( (char*)NetworkAddr ), debugstr_a( (char*)Endpoint ),
        debugstr_a( (char*)Options ), StringBinding );

  if (ObjUuid && *ObjUuid) len += strlen((char*)ObjUuid) + 1;
  if (Protseq && *Protseq) len += strlen((char*)Protseq) + 1;
  if (NetworkAddr && *NetworkAddr) len += strlen((char*)NetworkAddr);
  if (Endpoint && *Endpoint) len += strlen((char*)Endpoint) + 2;
  if (Options && *Options) len += strlen((char*)Options) + 2;

  data = HeapAlloc(GetProcessHeap(), 0, len);
  *StringBinding = (unsigned char*)data;

  if (ObjUuid && *ObjUuid) {
    data += RPCRT4_strcopyA(data, (char*)ObjUuid);
    *data++ = '@';
  }
  if (Protseq && *Protseq) {
    data += RPCRT4_strcopyA(data, (char*)Protseq);
    *data++ = ':';
  }
  if (NetworkAddr && *NetworkAddr)
    data += RPCRT4_strcopyA(data, (char*)NetworkAddr);

  if ((Endpoint && *Endpoint) ||
      (Options && *Options)) {
    *data++ = '[';
    if (Endpoint && *Endpoint) {
      data += RPCRT4_strcopyA(data, (char*)Endpoint);
      if (Options && *Options) *data++ = ',';
    }
    if (Options && *Options) {
      data += RPCRT4_strcopyA(data, (char*)Options);
    }
    *data++ = ']';
  }
  *data = 0;

  return RPC_S_OK;
}

/***********************************************************************
 *             RpcStringBindingComposeW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcStringBindingComposeW( LPWSTR ObjUuid, LPWSTR Protseq,
                                            LPWSTR NetworkAddr, LPWSTR Endpoint,
                                            LPWSTR Options, LPWSTR* StringBinding )
{
  DWORD len = 1;
  LPWSTR data;

  TRACE("(%s,%s,%s,%s,%s,%p)\n",
       debugstr_w( ObjUuid ), debugstr_w( Protseq ),
       debugstr_w( NetworkAddr ), debugstr_w( Endpoint ),
       debugstr_w( Options ), StringBinding);

  if (ObjUuid && *ObjUuid) len += strlenW(ObjUuid) + 1;
  if (Protseq && *Protseq) len += strlenW(Protseq) + 1;
  if (NetworkAddr && *NetworkAddr) len += strlenW(NetworkAddr);
  if (Endpoint && *Endpoint) len += strlenW(Endpoint) + 2;
  if (Options && *Options) len += strlenW(Options) + 2;

  data = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
  *StringBinding = data;

  if (ObjUuid && *ObjUuid) {
    data += RPCRT4_strcopyW(data, ObjUuid);
    *data++ = '@';
  }
  if (Protseq && *Protseq) {
    data += RPCRT4_strcopyW(data, Protseq);
    *data++ = ':';
  }
  if (NetworkAddr && *NetworkAddr) {
    data += RPCRT4_strcopyW(data, NetworkAddr);
  }
  if ((Endpoint && *Endpoint) ||
      (Options && *Options)) {
    *data++ = '[';
    if (Endpoint && *Endpoint) {
      data += RPCRT4_strcopyW(data, Endpoint);
      if (Options && *Options) *data++ = ',';
    }
    if (Options && *Options) {
      data += RPCRT4_strcopyW(data, Options);
    }
    *data++ = ']';
  }
  *data = 0;

  return RPC_S_OK;
}


/***********************************************************************
 *             RpcStringBindingParseA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcStringBindingParseA( unsigned char *StringBinding, unsigned char **ObjUuid,
                                          unsigned char **Protseq, unsigned char **NetworkAddr,
                                          unsigned char **Endpoint, unsigned char **Options)
{
  CHAR *data, *next;
  static const char ep_opt[] = "endpoint=";

  TRACE("(%s,%p,%p,%p,%p,%p)\n", debugstr_a((char*)StringBinding),
       ObjUuid, Protseq, NetworkAddr, Endpoint, Options);

  if (ObjUuid) *ObjUuid = NULL;
  if (Protseq) *Protseq = NULL;
  if (NetworkAddr) *NetworkAddr = NULL;
  if (Endpoint) *Endpoint = NULL;
  if (Options) *Options = NULL;

  data = (char*) StringBinding;

  next = strchr(data, '@');
  if (next) {
    if (ObjUuid) *ObjUuid = (unsigned char*)RPCRT4_strndupA(data, next - data);
    data = next+1;
  }

  next = strchr(data, ':');
  if (next) {
    if (Protseq) *Protseq = (unsigned char*)RPCRT4_strndupA(data, next - data);
    data = next+1;
  }

  next = strchr(data, '[');
  if (next) {
    CHAR *close, *opt;

    if (NetworkAddr) *NetworkAddr = (unsigned char*)RPCRT4_strndupA(data, next - data);
    data = next+1;
    close = strchr(data, ']');
    if (!close) goto fail;

    /* tokenize options */
    while (data < close) {
      next = strchr(data, ',');
      if (!next || next > close) next = close;
      /* FIXME: this is kind of inefficient */
      opt = RPCRT4_strndupA(data, next - data);
      data = next+1;

      /* parse option */
      next = strchr(opt, '=');
      if (!next) {
        /* not an option, must be an endpoint */
        if (*Endpoint) goto fail;
        *Endpoint = (unsigned char*) opt;
      } else {
        if (strncmp(opt, ep_opt, strlen(ep_opt)) == 0) {
          /* endpoint option */
          if (*Endpoint) goto fail;
          *Endpoint = (unsigned char*) RPCRT4_strdupA(next+1);
          HeapFree(GetProcessHeap(), 0, opt);
        } else {
          /* network option */
          if (*Options) {
            /* FIXME: this is kind of inefficient */
            *Options = (unsigned char*) RPCRT4_strconcatA( (char*)*Options, opt);
            HeapFree(GetProcessHeap(), 0, opt);
          } else 
	    *Options = (unsigned char*) opt;
        }
      }
    }

    data = close+1;
    if (*data) goto fail;
  }
  else if (NetworkAddr) 
    *NetworkAddr = (unsigned char*)RPCRT4_strdupA(data);

  return RPC_S_OK;

fail:
  if (ObjUuid) RpcStringFreeA((unsigned char**)ObjUuid);
  if (Protseq) RpcStringFreeA((unsigned char**)Protseq);
  if (NetworkAddr) RpcStringFreeA((unsigned char**)NetworkAddr);
  if (Endpoint) RpcStringFreeA((unsigned char**)Endpoint);
  if (Options) RpcStringFreeA((unsigned char**)Options);
  return RPC_S_INVALID_STRING_BINDING;
}

/***********************************************************************
 *             RpcStringBindingParseW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcStringBindingParseW( LPWSTR StringBinding, LPWSTR *ObjUuid,
                                          LPWSTR *Protseq, LPWSTR *NetworkAddr,
                                          LPWSTR *Endpoint, LPWSTR *Options)
{
  WCHAR *data, *next;
  static const WCHAR ep_opt[] = {'e','n','d','p','o','i','n','t','=',0};

  TRACE("(%s,%p,%p,%p,%p,%p)\n", debugstr_w(StringBinding),
       ObjUuid, Protseq, NetworkAddr, Endpoint, Options);

  if (ObjUuid) *ObjUuid = NULL;
  if (Protseq) *Protseq = NULL;
  if (NetworkAddr) *NetworkAddr = NULL;
  if (Endpoint) *Endpoint = NULL;
  if (Options) *Options = NULL;

  data = StringBinding;

  next = strchrW(data, '@');
  if (next) {
    if (ObjUuid) *ObjUuid = RPCRT4_strndupW(data, next - data);
    data = next+1;
  }

  next = strchrW(data, ':');
  if (next) {
    if (Protseq) *Protseq = RPCRT4_strndupW(data, next - data);
    data = next+1;
  }

  next = strchrW(data, '[');
  if (next) {
    WCHAR *close, *opt;

    if (NetworkAddr) *NetworkAddr = RPCRT4_strndupW(data, next - data);
    data = next+1;
    close = strchrW(data, ']');
    if (!close) goto fail;

    /* tokenize options */
    while (data < close) {
      next = strchrW(data, ',');
      if (!next || next > close) next = close;
      /* FIXME: this is kind of inefficient */
      opt = RPCRT4_strndupW(data, next - data);
      data = next+1;

      /* parse option */
      next = strchrW(opt, '=');
      if (!next) {
        /* not an option, must be an endpoint */
        if (*Endpoint) goto fail;
        *Endpoint = opt;
      } else {
        if (strncmpW(opt, ep_opt, strlenW(ep_opt)) == 0) {
          /* endpoint option */
          if (*Endpoint) goto fail;
          *Endpoint = RPCRT4_strdupW(next+1);
          HeapFree(GetProcessHeap(), 0, opt);
        } else {
          /* network option */
          if (*Options) {
            /* FIXME: this is kind of inefficient */
            *Options = RPCRT4_strconcatW(*Options, opt);
            HeapFree(GetProcessHeap(), 0, opt);
          } else 
	    *Options = opt;
        }
      }
    }

    data = close+1;
    if (*data) goto fail;
  } else if (NetworkAddr) 
    *NetworkAddr = RPCRT4_strdupW(data);

  return RPC_S_OK;

fail:
  if (ObjUuid) RpcStringFreeW(ObjUuid);
  if (Protseq) RpcStringFreeW(Protseq);
  if (NetworkAddr) RpcStringFreeW(NetworkAddr);
  if (Endpoint) RpcStringFreeW(Endpoint);
  if (Options) RpcStringFreeW(Options);
  return RPC_S_INVALID_STRING_BINDING;
}

/***********************************************************************
 *             RpcBindingFree (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingFree( RPC_BINDING_HANDLE* Binding )
{
  RPC_STATUS status;
  TRACE("(%p) = %p\n", Binding, *Binding);
  status = RPCRT4_DestroyBinding(*Binding);
  if (status == RPC_S_OK) *Binding = 0;
  return status;
}
  
/***********************************************************************
 *             RpcBindingVectorFree (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingVectorFree( RPC_BINDING_VECTOR** BindingVector )
{
  RPC_STATUS status;
  unsigned long c;

  TRACE("(%p)\n", BindingVector);
  for (c=0; c<(*BindingVector)->Count; c++) {
    status = RpcBindingFree(&(*BindingVector)->BindingH[c]);
  }
  HeapFree(GetProcessHeap(), 0, *BindingVector);
  *BindingVector = NULL;
  return RPC_S_OK;
}
  
/***********************************************************************
 *             RpcBindingInqObject (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingInqObject( RPC_BINDING_HANDLE Binding, UUID* ObjectUuid )
{
  RpcBinding* bind = (RpcBinding*)Binding;

  TRACE("(%p,%p) = %s\n", Binding, ObjectUuid, debugstr_guid(&bind->ObjectUuid));
  memcpy(ObjectUuid, &bind->ObjectUuid, sizeof(UUID));
  return RPC_S_OK;
}
  
/***********************************************************************
 *             RpcBindingSetObject (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingSetObject( RPC_BINDING_HANDLE Binding, UUID* ObjectUuid )
{
  RpcBinding* bind = (RpcBinding*)Binding;

  TRACE("(%p,%s)\n", Binding, debugstr_guid(ObjectUuid));
  if (bind->server) return RPC_S_WRONG_KIND_OF_BINDING;
  return RPCRT4_SetBindingObject(Binding, ObjectUuid);
}

/***********************************************************************
 *             RpcBindingFromStringBindingA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingFromStringBindingA( unsigned char *StringBinding, RPC_BINDING_HANDLE* Binding )
{
  RPC_STATUS ret;
  RpcBinding* bind = NULL;
  unsigned char *ObjectUuid, *Protseq, *NetworkAddr, *Endpoint, *Options;
  UUID Uuid;

  TRACE("(%s,%p)\n", debugstr_a((char*)StringBinding), Binding);

  ret = RpcStringBindingParseA(StringBinding, &ObjectUuid, &Protseq,
                              &NetworkAddr, &Endpoint, &Options);
  if (ret != RPC_S_OK) return ret;

  ret = UuidFromStringA(ObjectUuid, &Uuid);

  if (ret == RPC_S_OK)
    ret = RPCRT4_CreateBindingA(&bind, FALSE, (char*)Protseq);
  if (ret == RPC_S_OK)
    ret = RPCRT4_SetBindingObject(bind, &Uuid);
  if (ret == RPC_S_OK)
    ret = RPCRT4_CompleteBindingA(bind, (char*)NetworkAddr, (char*)Endpoint, (char*)Options);

  RpcStringFreeA((unsigned char**)&Options);
  RpcStringFreeA((unsigned char**)&Endpoint);
  RpcStringFreeA((unsigned char**)&NetworkAddr);
  RpcStringFreeA((unsigned char**)&Protseq);
  RpcStringFreeA((unsigned char**)&ObjectUuid);

  if (ret == RPC_S_OK) 
    *Binding = (RPC_BINDING_HANDLE)bind;
  else 
    RPCRT4_DestroyBinding(bind);

  return ret;
}

/***********************************************************************
 *             RpcBindingFromStringBindingW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingFromStringBindingW( LPWSTR StringBinding, RPC_BINDING_HANDLE* Binding )
{
  RPC_STATUS ret;
  RpcBinding* bind = NULL;
  LPWSTR ObjectUuid, Protseq, NetworkAddr, Endpoint, Options;
  UUID Uuid;

  TRACE("(%s,%p)\n", debugstr_w(StringBinding), Binding);

  ret = RpcStringBindingParseW(StringBinding, &ObjectUuid, &Protseq,
                              &NetworkAddr, &Endpoint, &Options);
  if (ret != RPC_S_OK) return ret;

  ret = UuidFromStringW(ObjectUuid, &Uuid);

  if (ret == RPC_S_OK)
    ret = RPCRT4_CreateBindingW(&bind, FALSE, Protseq);
  if (ret == RPC_S_OK)
    ret = RPCRT4_SetBindingObject(bind, &Uuid);
  if (ret == RPC_S_OK)
    ret = RPCRT4_CompleteBindingW(bind, NetworkAddr, Endpoint, Options);

  RpcStringFreeW(&Options);
  RpcStringFreeW(&Endpoint);
  RpcStringFreeW(&NetworkAddr);
  RpcStringFreeW(&Protseq);
  RpcStringFreeW(&ObjectUuid);

  if (ret == RPC_S_OK)
    *Binding = (RPC_BINDING_HANDLE)bind;
  else
    RPCRT4_DestroyBinding(bind);

  return ret;
}
  
/***********************************************************************
 *             RpcBindingToStringBindingA (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingToStringBindingA( RPC_BINDING_HANDLE Binding, unsigned char** StringBinding )
{
  RPC_STATUS ret;
  RpcBinding* bind = (RpcBinding*)Binding;
  LPSTR ObjectUuid;

  TRACE("(%p,%p)\n", Binding, StringBinding);

  ret = UuidToStringA(&bind->ObjectUuid, (unsigned char**)&ObjectUuid);
  if (ret != RPC_S_OK) return ret;

  ret = RpcStringBindingComposeA((unsigned char*) ObjectUuid, (unsigned char*)bind->Protseq, (unsigned char*) bind->NetworkAddr,
                                 (unsigned char*) bind->Endpoint, NULL, StringBinding);

  RpcStringFreeA((unsigned char**)&ObjectUuid);

  return ret;
}
  
/***********************************************************************
 *             RpcBindingToStringBindingW (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingToStringBindingW( RPC_BINDING_HANDLE Binding, unsigned short** StringBinding )
{
  RPC_STATUS ret;
  unsigned char *str = NULL;
  TRACE("(%p,%p)\n", Binding, StringBinding);
  ret = RpcBindingToStringBindingA(Binding, &str);
  *StringBinding = RPCRT4_strdupAtoW((char*)str);
  RpcStringFreeA((unsigned char**)&str);
  return ret;
}

/***********************************************************************
 *             I_RpcBindingSetAsync (RPCRT4.@)
 * NOTES
 *  Exists in win9x and winNT, but with different number of arguments
 *  (9x version has 3 arguments, NT has 2).
 */
RPC_STATUS WINAPI I_RpcBindingSetAsync( RPC_BINDING_HANDLE Binding, RPC_BLOCKING_FN BlockingFn)
{
  RpcBinding* bind = (RpcBinding*)Binding;

  TRACE( "(%p,%p): stub\n", Binding, BlockingFn );

  bind->BlockingFn = BlockingFn;

  return RPC_S_OK;
}

/***********************************************************************
 *             RpcBindingCopy (RPCRT4.@)
 */
RPC_STATUS RPC_ENTRY RpcBindingCopy(
  RPC_BINDING_HANDLE SourceBinding,
  RPC_BINDING_HANDLE* DestinationBinding)
{
  RpcBinding *DestBinding;
  RpcBinding *SrcBinding = (RpcBinding*)SourceBinding;
  RPC_STATUS status;

  TRACE("(%p, %p)\n", SourceBinding, DestinationBinding);

  status = RPCRT4_AllocBinding(&DestBinding, SrcBinding->server);
  if (status != RPC_S_OK) return status;

  DestBinding->ObjectUuid = SrcBinding->ObjectUuid;
  DestBinding->BlockingFn = SrcBinding->BlockingFn;
  DestBinding->Protseq = RPCRT4_strndupA(SrcBinding->Protseq, -1);
  DestBinding->NetworkAddr = RPCRT4_strndupA(SrcBinding->NetworkAddr, -1);
  DestBinding->Endpoint = RPCRT4_strndupA(SrcBinding->Endpoint, -1);

  if (SrcBinding->AuthInfo) RpcAuthInfo_AddRef(SrcBinding->AuthInfo);
  DestBinding->AuthInfo = SrcBinding->AuthInfo;

  *DestinationBinding = DestBinding;
  return RPC_S_OK;
}

/***********************************************************************
 *             RpcImpersonateClient (RPCRT4.@)
 *
 * Impersonates the client connected via a binding handle so that security
 * checks are done in the context of the client.
 *
 * PARAMS
 *  BindingHandle [I] Handle to the binding to the client.
 *
 * RETURNS
 *  Success: RPS_S_OK.
 *  Failure: RPC_STATUS value.
 *
 * NOTES
 *
 * If BindingHandle is NULL then the function impersonates the client
 * connected to the binding handle of the current thread.
 */
RPC_STATUS WINAPI RpcImpersonateClient(RPC_BINDING_HANDLE BindingHandle)
{
    FIXME("(%p): stub\n", BindingHandle);
    ImpersonateSelf(SecurityImpersonation);
    return RPC_S_OK;
}

/***********************************************************************
 *             RpcRevertToSelfEx (RPCRT4.@)
 *
 * Stops impersonating the client connected to the binding handle so that security
 * checks are no longer done in the context of the client.
 *
 * PARAMS
 *  BindingHandle [I] Handle to the binding to the client.
 *
 * RETURNS
 *  Success: RPS_S_OK.
 *  Failure: RPC_STATUS value.
 *
 * NOTES
 *
 * If BindingHandle is NULL then the function stops impersonating the client
 * connected to the binding handle of the current thread.
 */
RPC_STATUS WINAPI RpcRevertToSelfEx(RPC_BINDING_HANDLE BindingHandle)
{
    FIXME("(%p): stub\n", BindingHandle);
    return RPC_S_OK;
}

static RPC_STATUS RpcAuthInfo_Create(unsigned long AuthnLevel, unsigned long AuthnSvc, CredHandle cred, TimeStamp exp, RpcAuthInfo **ret)
{
    RpcAuthInfo *AuthInfo = HeapAlloc(GetProcessHeap(), 0, sizeof(*AuthInfo));
    if (!AuthInfo)
        return ERROR_OUTOFMEMORY;

    AuthInfo->AuthnLevel = AuthnLevel;
    AuthInfo->AuthnSvc = AuthnSvc;
    AuthInfo->cred = cred;
    AuthInfo->exp = exp;
    *ret = AuthInfo;
    return RPC_S_OK;
}

ULONG RpcAuthInfo_AddRef(RpcAuthInfo *AuthInfo)
{
    return InterlockedIncrement(&AuthInfo->refs);
}

ULONG RpcAuthInfo_Release(RpcAuthInfo *AuthInfo)
{
    ULONG refs = InterlockedDecrement(&AuthInfo->refs);

    if (!refs)
    {
        FreeCredentialsHandle(&AuthInfo->cred);
        HeapFree(GetProcessHeap(), 0, AuthInfo);
    }

    return refs;
}

/***********************************************************************
 *             RpcRevertToSelf (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcRevertToSelf(void)
{
    FIXME("stub\n");
    RevertToSelf();
    return RPC_S_OK;
}

/***********************************************************************
 *             RpcMgmtSetComTimeout (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcMgmtSetComTimeout(RPC_BINDING_HANDLE BindingHandle, unsigned int Timeout)
{
    FIXME("(%p, %d): stub\n", BindingHandle, Timeout);
    return RPC_S_OK;
}

/***********************************************************************
 *             RpcBindingInqAuthInfoExA (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingInqAuthInfoExA( RPC_BINDING_HANDLE Binding, unsigned char ** ServerPrincName, unsigned long *AuthnLevel,
                          unsigned long *AuthnSvc, RPC_AUTH_IDENTITY_HANDLE *AuthIdentity, unsigned long *AuthzSvc,
                          unsigned long RpcQosVersion, RPC_SECURITY_QOS *SecurityQOS )
{
    FIXME("%p %p %p %p %p %p %lu %p\n", Binding, ServerPrincName, AuthnLevel,
          AuthnSvc, AuthIdentity, AuthzSvc, RpcQosVersion, SecurityQOS);
    return RPC_S_INVALID_BINDING;
}

/***********************************************************************
 *             RpcBindingInqAuthInfoExW (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingInqAuthInfoExW( RPC_BINDING_HANDLE Binding, unsigned short ** ServerPrincName, unsigned long *AuthnLevel,
                          unsigned long *AuthnSvc, RPC_AUTH_IDENTITY_HANDLE *AuthIdentity, unsigned long *AuthzSvc,
                          unsigned long RpcQosVersion, RPC_SECURITY_QOS *SecurityQOS )
{
    FIXME("%p %p %p %p %p %p %lu %p\n", Binding, ServerPrincName, AuthnLevel,
          AuthnSvc, AuthIdentity, AuthzSvc, RpcQosVersion, SecurityQOS);
    return RPC_S_INVALID_BINDING;
}

/***********************************************************************
 *             RpcBindingInqAuthInfoA (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingInqAuthInfoA( RPC_BINDING_HANDLE Binding, unsigned char ** ServerPrincName, unsigned long *AuthnLevel,
                        unsigned long *AuthnSvc, RPC_AUTH_IDENTITY_HANDLE *AuthIdentity, unsigned long *AuthzSvc )
{
    FIXME("%p %p %p %p %p %p\n", Binding, ServerPrincName, AuthnLevel,
          AuthnSvc, AuthIdentity, AuthzSvc);
    return RPC_S_INVALID_BINDING;
}

/***********************************************************************
 *             RpcBindingInqAuthInfoW (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingInqAuthInfoW( RPC_BINDING_HANDLE Binding, unsigned short ** ServerPrincName, unsigned long *AuthnLevel,
                        unsigned long *AuthnSvc, RPC_AUTH_IDENTITY_HANDLE *AuthIdentity, unsigned long *AuthzSvc )
{
    FIXME("%p %p %p %p %p %p\n", Binding, ServerPrincName, AuthnLevel,
          AuthnSvc, AuthIdentity, AuthzSvc);
    return RPC_S_INVALID_BINDING;
}

/***********************************************************************
 *             RpcBindingSetAuthInfoExA (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingSetAuthInfoExA( RPC_BINDING_HANDLE Binding, unsigned char *ServerPrincName,
                          unsigned long AuthnLevel, unsigned long AuthnSvc,
                          RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvr,
                          RPC_SECURITY_QOS *SecurityQos )
{
  RpcBinding* bind = (RpcBinding*)Binding;
  SECURITY_STATUS r;
  CredHandle cred;
  TimeStamp exp;
  ULONG package_count;
  ULONG i;
  PSecPkgInfoA packages;

  TRACE("%p %s %lu %lu %p %lu %p\n", Binding, debugstr_a((const char*)ServerPrincName),
        AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr, SecurityQos);

  if (AuthnLevel != RPC_C_AUTHN_LEVEL_CONNECT)
  {
    FIXME("unsupported AuthnLevel %lu\n", AuthnLevel);
    return RPC_S_UNKNOWN_AUTHN_LEVEL;
  }

  if (AuthzSvr)
  {
    FIXME("unsupported AuthzSvr %lu\n", AuthzSvr);
    return RPC_S_UNKNOWN_AUTHZ_SERVICE;
  }

  if (SecurityQos)
    FIXME("SecurityQos ignored\n");

  r = EnumerateSecurityPackagesA(&package_count, &packages);
  if (r != SEC_E_OK)
  {
    ERR("EnumerateSecurityPackagesA failed with error 0x%08lx\n", r);
    return RPC_S_SEC_PKG_ERROR;
  }

  for (i = 0; i < package_count; i++)
    if (packages[i].wRPCID == AuthnSvc)
        break;

  if (i == package_count)
  {
    FIXME("unsupported AuthnSvc %lu\n", AuthnSvc);
    FreeContextBuffer(packages);
    return RPC_S_UNKNOWN_AUTHN_SERVICE;
  }

  TRACE("found package %s for service %ld\n", packages[i].Name, AuthnSvc);
  r = AcquireCredentialsHandleA((SEC_CHAR *)ServerPrincName, packages[i].Name, SECPKG_CRED_OUTBOUND, NULL,
                                AuthIdentity, NULL, NULL, &cred, &exp);
  FreeContextBuffer(packages);
  if (r == ERROR_SUCCESS)
  {
    if (bind->AuthInfo) RpcAuthInfo_Release(bind->AuthInfo);
    bind->AuthInfo = NULL;
    r = RpcAuthInfo_Create(AuthnLevel, AuthnSvc, cred, exp, &bind->AuthInfo);
    if (r != RPC_S_OK)
      FreeCredentialsHandle(&cred);
    return RPC_S_OK;
  }
  else
  {
    ERR("AcquireCredentialsHandleA failed with error 0x%08lx\n", r);
    return RPC_S_SEC_PKG_ERROR;
  }
}

/***********************************************************************
 *             RpcBindingSetAuthInfoExW (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingSetAuthInfoExW( RPC_BINDING_HANDLE Binding, unsigned short *ServerPrincName, unsigned long AuthnLevel,
                          unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvr,
                          RPC_SECURITY_QOS *SecurityQos )
{
  RpcBinding* bind = (RpcBinding*)Binding;
  SECURITY_STATUS r;
  CredHandle cred;
  TimeStamp exp;
  ULONG package_count;
  ULONG i;
  PSecPkgInfoW packages;

  TRACE("%p %s %lu %lu %p %lu %p\n", Binding, debugstr_w((const WCHAR*)ServerPrincName),
        AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr, SecurityQos);

  if (AuthnLevel != RPC_C_AUTHN_LEVEL_CONNECT)
  {
    FIXME("unsupported AuthnLevel %lu\n", AuthnLevel);
    return RPC_S_UNKNOWN_AUTHN_LEVEL;
  }

  if (AuthzSvr)
  {
    FIXME("unsupported AuthzSvr %lu\n", AuthzSvr);
    return RPC_S_UNKNOWN_AUTHZ_SERVICE;
  }

  if (SecurityQos)
    FIXME("SecurityQos ignored\n");

  r = EnumerateSecurityPackagesW(&package_count, &packages);
  if (r != SEC_E_OK)
  {
    ERR("EnumerateSecurityPackagesA failed with error 0x%08lx\n", r);
    return RPC_S_SEC_PKG_ERROR;
  }

  for (i = 0; i < package_count; i++)
    if (packages[i].wRPCID == AuthnSvc)
        break;

  if (i == package_count)
  {
    FIXME("unsupported AuthnSvc %lu\n", AuthnSvc);
    FreeContextBuffer(packages);
    return RPC_S_UNKNOWN_AUTHN_SERVICE;
  }

  TRACE("found package %s for service %ld\n", debugstr_w(packages[i].Name), AuthnSvc);
  r = AcquireCredentialsHandleW((SEC_WCHAR *)ServerPrincName, packages[i].Name, SECPKG_CRED_OUTBOUND, NULL,
                                AuthIdentity, NULL, NULL, &cred, &exp);
  FreeContextBuffer(packages);
  if (r == ERROR_SUCCESS)
  {
    if (bind->AuthInfo) RpcAuthInfo_Release(bind->AuthInfo);
    bind->AuthInfo = NULL;
    r = RpcAuthInfo_Create(AuthnLevel, AuthnSvc, cred, exp, &bind->AuthInfo);
    if (r != RPC_S_OK)
      FreeCredentialsHandle(&cred);
    return RPC_S_OK;
  }
  else
  {
    ERR("AcquireCredentialsHandleA failed with error 0x%08lx\n", r);
    return RPC_S_SEC_PKG_ERROR;
  }
}

/***********************************************************************
 *             RpcBindingSetAuthInfoA (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingSetAuthInfoA( RPC_BINDING_HANDLE Binding, unsigned char *ServerPrincName, unsigned long AuthnLevel,
                        unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvr )
{
    TRACE("%p %s %lu %lu %p %lu\n", Binding, debugstr_a((const char*)ServerPrincName),
          AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr);
    return RpcBindingSetAuthInfoExA(Binding, ServerPrincName, AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr, NULL);
}

/***********************************************************************
 *             RpcBindingSetAuthInfoW (RPCRT4.@)
 */
RPCRTAPI RPC_STATUS RPC_ENTRY
RpcBindingSetAuthInfoW( RPC_BINDING_HANDLE Binding, unsigned short *ServerPrincName, unsigned long AuthnLevel,
                        unsigned long AuthnSvc, RPC_AUTH_IDENTITY_HANDLE AuthIdentity, unsigned long AuthzSvr )
{
    TRACE("%p %s %lu %lu %p %lu\n", Binding, debugstr_w((const WCHAR*)ServerPrincName),
          AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr);
    return RpcBindingSetAuthInfoExW(Binding, ServerPrincName, AuthnLevel, AuthnSvc, AuthIdentity, AuthzSvr, NULL);
}

/***********************************************************************
 *             RpcBindingSetOption (RPCRT4.@)
 */
RPC_STATUS WINAPI RpcBindingSetOption(RPC_BINDING_HANDLE BindingHandle, ULONG Option, ULONG OptionValue)
{
    FIXME("(%p, %ld, %ld): stub\n", BindingHandle, Option, OptionValue);
    return RPC_S_OK;
}
