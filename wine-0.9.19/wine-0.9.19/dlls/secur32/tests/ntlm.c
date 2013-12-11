/*
 * Tests for the NTLM security provider
 *
 * Copyright 2005, 2006 Kai Blin
 * Copyright 2006 Dmitry Timoshkov
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
 * The code that tests for the behaviour of ISC_REQ_ALLOCATE_MEMORY is based
 * on code written by Dmitry Timoshkov.
 
 */

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <windef.h>
#include <winbase.h>
#define SECURITY_WIN32
#include <sspi.h>

#include "wine/test.h"

static HMODULE secdll;
static PSecurityFunctionTableA (SEC_ENTRY * pInitSecurityInterfaceA)(void);
static SECURITY_STATUS (SEC_ENTRY * pFreeContextBuffer)(PVOID pv);
static SECURITY_STATUS (SEC_ENTRY * pQuerySecurityPackageInfoA)(SEC_CHAR*, PSecPkgInfoA*);
static SECURITY_STATUS (SEC_ENTRY * pAcquireCredentialsHandleA)(SEC_CHAR*, SEC_CHAR*,
                            ULONG, PLUID, PVOID, SEC_GET_KEY_FN, PVOID, PCredHandle, PTimeStamp);
static SECURITY_STATUS (SEC_ENTRY * pInitializeSecurityContextA)(PCredHandle, PCtxtHandle,
                            SEC_CHAR*, ULONG, ULONG, ULONG, PSecBufferDesc, ULONG, 
                            PCtxtHandle, PSecBufferDesc, PULONG, PTimeStamp);
static SECURITY_STATUS (SEC_ENTRY * pCompleteAuthToken)(PCtxtHandle, PSecBufferDesc);
static SECURITY_STATUS (SEC_ENTRY * pAcceptSecurityContext)(PCredHandle, PCtxtHandle,
                            PSecBufferDesc, ULONG, ULONG, PCtxtHandle, PSecBufferDesc,
                            PULONG, PTimeStamp);
static SECURITY_STATUS (SEC_ENTRY * pFreeCredentialsHandle)(PCredHandle);
static SECURITY_STATUS (SEC_ENTRY * pDeleteSecurityContext)(PCtxtHandle);
static SECURITY_STATUS (SEC_ENTRY * pQueryContextAttributesA)(PCtxtHandle, ULONG, PVOID);
static SECURITY_STATUS (SEC_ENTRY * pMakeSignature)(PCtxtHandle, ULONG,
                            PSecBufferDesc, ULONG);
static SECURITY_STATUS (SEC_ENTRY * pVerifySignature)(PCtxtHandle, PSecBufferDesc,
                            ULONG, PULONG);
static SECURITY_STATUS (SEC_ENTRY * pEncryptMessage)(PCtxtHandle, ULONG,
                            PSecBufferDesc, ULONG);
static SECURITY_STATUS (SEC_ENTRY * pDecryptMessage)(PCtxtHandle, PSecBufferDesc,
                            ULONG, PULONG);

typedef struct _SspiData {
    PCredHandle cred;
    PCtxtHandle ctxt;
    PSecBufferDesc in_buf;
    PSecBufferDesc out_buf;
    PSEC_WINNT_AUTH_IDENTITY id;
    ULONG max_token;
} SspiData;

static BYTE network_challenge[] = 
   {0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x05, 0x82, 0x82, 0xa0, 0xe9, 0x58, 0x7f, 0x14, 0xa2, 0x86,
    0x3b, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x54, 0x00, 0x40, 0x00, 0x00, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x02, 0x00, 0x10, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x01, 0x00, 0x10, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x04, 0x00, 0x10, 0x00, 0x63, 0x00,
    0x61, 0x00, 0x73, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x6f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x03, 0x00, 0x10, 0x00, 0x63, 0x00,
    0x61, 0x00, 0x73, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x6f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00};

static BYTE native_challenge[] = 
   {0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x30, 0x00, 0x00, 0x00,
    0x05, 0x82, 0x82, 0xa0, 0xb5, 0x60, 0x8e, 0x95, 0xb5, 0x3c,
    0xee, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x54, 0x00, 0x40, 0x00, 0x00, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x02, 0x00, 0x10, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x01, 0x00, 0x10, 0x00, 0x43, 0x00,
    0x41, 0x00, 0x53, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x4f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x04, 0x00, 0x10, 0x00, 0x63, 0x00,
    0x61, 0x00, 0x73, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x6f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x03, 0x00, 0x10, 0x00, 0x63, 0x00,
    0x61, 0x00, 0x73, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x6f, 0x00,
    0x30, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00};

static BYTE message_signature[] =
   {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static BYTE message_binary[] =
   {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72,
    0x6c, 0x64, 0x21};

static char message[] = "Hello, world!";

static BYTE crypt_trailer_client[] =
   {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0xc7,
    0xaa, 0x26, 0x16, 0x39, 0x07, 0x4e};

static BYTE crypt_message_client[] =
   {0x86, 0x9c, 0x5a, 0x10, 0x78, 0xb3, 0x30, 0x98, 0x46, 0x15,
    0xa0, 0x31, 0xd9};

static BYTE crypt_trailer_server[] =
   {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x46,
    0x2e, 0x77, 0xeb, 0xf0, 0xf6, 0x9e};

static BYTE crypt_message_server[] =
   {0xf6, 0xb7, 0x92, 0x0c, 0xac, 0xea, 0x98, 0xe6, 0xef, 0xa0,
    0x29, 0x66, 0xfd};

static void InitFunctionPtrs(void)
{
    secdll = LoadLibraryA("secur32.dll");
    if(!secdll)
        secdll = LoadLibraryA("security.dll");
    if(secdll)
    {
        pInitSecurityInterfaceA = (PVOID)GetProcAddress(secdll, "InitSecurityInterfaceA");
        pFreeContextBuffer = (PVOID)GetProcAddress(secdll, "FreeContextBuffer");
        pQuerySecurityPackageInfoA = (PVOID)GetProcAddress(secdll, "QuerySecurityPackageInfoA");
        pAcquireCredentialsHandleA = (PVOID)GetProcAddress(secdll, "AcquireCredentialsHandleA");
        pInitializeSecurityContextA = (PVOID)GetProcAddress(secdll, "InitializeSecurityContextA");
        pCompleteAuthToken = (PVOID)GetProcAddress(secdll, "CompleteAuthToken");
        pAcceptSecurityContext = (PVOID)GetProcAddress(secdll, "AcceptSecurityContext");
        pFreeCredentialsHandle = (PVOID)GetProcAddress(secdll, "FreeCredentialsHandle");
        pDeleteSecurityContext = (PVOID)GetProcAddress(secdll, "DeleteSecurityContext");
        pQueryContextAttributesA = (PVOID)GetProcAddress(secdll, "QueryContextAttributesA");
        pMakeSignature = (PVOID)GetProcAddress(secdll, "MakeSignature");
        pVerifySignature = (PVOID)GetProcAddress(secdll, "VerifySignature");
        pEncryptMessage = (PVOID)GetProcAddress(secdll, "EncryptMessage");
        pDecryptMessage = (PVOID)GetProcAddress(secdll, "DecryptMessage");
    }
}

static const char* getSecError(SECURITY_STATUS status)
{
    static char buf[20];

#define _SEC_ERR(x) case (x): return #x;
    switch(status)
    {
        _SEC_ERR(SEC_E_OK);
        _SEC_ERR(SEC_E_INSUFFICIENT_MEMORY);
        _SEC_ERR(SEC_E_INVALID_HANDLE);
        _SEC_ERR(SEC_E_UNSUPPORTED_FUNCTION);
        _SEC_ERR(SEC_E_TARGET_UNKNOWN);
        _SEC_ERR(SEC_E_INTERNAL_ERROR);
        _SEC_ERR(SEC_E_SECPKG_NOT_FOUND);
        _SEC_ERR(SEC_E_NOT_OWNER);
        _SEC_ERR(SEC_E_CANNOT_INSTALL);
        _SEC_ERR(SEC_E_INVALID_TOKEN);
        _SEC_ERR(SEC_E_CANNOT_PACK);
        _SEC_ERR(SEC_E_QOP_NOT_SUPPORTED);
        _SEC_ERR(SEC_E_NO_IMPERSONATION);
        _SEC_ERR(SEC_I_CONTINUE_NEEDED);
        _SEC_ERR(SEC_E_BUFFER_TOO_SMALL);
        _SEC_ERR(SEC_E_ILLEGAL_MESSAGE);
        _SEC_ERR(SEC_E_LOGON_DENIED);
        _SEC_ERR(SEC_E_NO_CREDENTIALS);
        _SEC_ERR(SEC_E_OUT_OF_SEQUENCE);
        default:
            sprintf(buf, "%08lx\n", status);
            return buf;
    }
#undef _SEC_ERR
}

/**********************************************************************/

static SECURITY_STATUS setupBuffers(SspiData *sspi_data, SecPkgInfoA *sec_pkg_info)
{
    
    sspi_data->in_buf  = HeapAlloc(GetProcessHeap(), 0, sizeof(SecBufferDesc));
    sspi_data->out_buf = HeapAlloc(GetProcessHeap(), 0, sizeof(SecBufferDesc));
    sspi_data->max_token = sec_pkg_info->cbMaxToken;

    if(sspi_data->in_buf != NULL)
    {
        PSecBuffer sec_buffer = HeapAlloc(GetProcessHeap(), 0,
                sizeof(SecBuffer));
        if(sec_buffer == NULL){
            trace("in_buf: sec_buffer == NULL\n");
            return SEC_E_INSUFFICIENT_MEMORY;
        }
        
        sspi_data->in_buf->ulVersion = SECBUFFER_VERSION;
        sspi_data->in_buf->cBuffers = 1;
        sspi_data->in_buf->pBuffers = sec_buffer;

        sec_buffer->cbBuffer = sec_pkg_info->cbMaxToken;
        sec_buffer->BufferType = SECBUFFER_TOKEN;
        if((sec_buffer->pvBuffer = HeapAlloc(GetProcessHeap(), 0, 
                        sec_pkg_info->cbMaxToken)) == NULL)
        {
            trace("in_buf: sec_buffer->pvBuffer == NULL\n");
            return SEC_E_INSUFFICIENT_MEMORY;
        }
    }
    else
    {
        trace("HeapAlloc in_buf returned NULL\n");
        return SEC_E_INSUFFICIENT_MEMORY;
    }
    
    if(sspi_data->out_buf != NULL)
    {
        PSecBuffer sec_buffer = HeapAlloc(GetProcessHeap(), 0,
                sizeof(SecBuffer));
        
        if(sec_buffer == NULL){
            trace("out_buf: sec_buffer == NULL\n");
            return SEC_E_INSUFFICIENT_MEMORY;
        }

        sspi_data->out_buf->ulVersion = SECBUFFER_VERSION;
        sspi_data->out_buf->cBuffers = 1;
        sspi_data->out_buf->pBuffers = sec_buffer;

        sec_buffer->cbBuffer = sec_pkg_info->cbMaxToken;
        sec_buffer->BufferType = SECBUFFER_TOKEN;
        if((sec_buffer->pvBuffer = HeapAlloc(GetProcessHeap(), 0, 
                        sec_pkg_info->cbMaxToken)) == NULL){
            trace("out_buf: sec_buffer->pvBuffer == NULL\n");
            return SEC_E_INSUFFICIENT_MEMORY;
        }
    }
    else
    {
        trace("HeapAlloc out_buf returned NULL\n");
        return SEC_E_INSUFFICIENT_MEMORY;
    }

    return SEC_E_OK;
}

/**********************************************************************/

static void cleanupBuffers(SspiData *sspi_data)
{
    ULONG i;

    if(sspi_data->in_buf != NULL)
    {
        for(i = 0; i < sspi_data->in_buf->cBuffers; ++i)
        {
            HeapFree(GetProcessHeap(), 0, sspi_data->in_buf->pBuffers[i].pvBuffer);
        }
        HeapFree(GetProcessHeap(), 0, sspi_data->in_buf->pBuffers);
        HeapFree(GetProcessHeap(), 0, sspi_data->in_buf);
    }
    
    if(sspi_data->out_buf != NULL)
    {
        for(i = 0; i < sspi_data->out_buf->cBuffers; ++i)
        {
            HeapFree(GetProcessHeap(), 0, sspi_data->out_buf->pBuffers[i].pvBuffer);
        }
        HeapFree(GetProcessHeap(), 0, sspi_data->out_buf->pBuffers);
        HeapFree(GetProcessHeap(), 0, sspi_data->out_buf);
    }
}

/**********************************************************************/

static SECURITY_STATUS setupClient(SspiData *sspi_data, SEC_CHAR *provider)
{
    SECURITY_STATUS ret;
    TimeStamp ttl;
    SecPkgInfoA *sec_pkg_info;

    trace("Running setupClient\n");
    
    sspi_data->cred = HeapAlloc(GetProcessHeap(), 0, sizeof(CredHandle));
    sspi_data->ctxt = HeapAlloc(GetProcessHeap(), 0, sizeof(CtxtHandle));
    
    ret = pQuerySecurityPackageInfoA(provider, &sec_pkg_info);

    ok(ret == SEC_E_OK, "QuerySecurityPackageInfo returned %s\n", getSecError(ret));

    setupBuffers(sspi_data, sec_pkg_info);
    
    if((ret = pAcquireCredentialsHandleA(NULL, provider, SECPKG_CRED_OUTBOUND,
            NULL, sspi_data->id, NULL, NULL, sspi_data->cred, &ttl))
            != SEC_E_OK)
    {
        trace("AcquireCredentialsHandle() returned %s\n", getSecError(ret));
    }

    ok(ret == SEC_E_OK, "AcquireCredentialsHande() returned %s\n", 
            getSecError(ret));

    return ret;
}
/**********************************************************************/

static SECURITY_STATUS setupServer(SspiData *sspi_data, SEC_CHAR *provider)
{
    SECURITY_STATUS ret;
    TimeStamp ttl;
    SecPkgInfoA *sec_pkg_info;

    trace("Running setupServer\n");

    sspi_data->cred = HeapAlloc(GetProcessHeap(), 0, sizeof(CredHandle));
    sspi_data->ctxt = HeapAlloc(GetProcessHeap(), 0, sizeof(CtxtHandle));

    ret = pQuerySecurityPackageInfoA(provider, &sec_pkg_info);

    ok(ret == SEC_E_OK, "QuerySecurityPackageInfo returned %s\n", getSecError(ret));

    setupBuffers(sspi_data, sec_pkg_info);

    if((ret = pAcquireCredentialsHandleA(NULL, provider, SECPKG_CRED_INBOUND, 
            NULL, NULL, NULL, NULL, sspi_data->cred, &ttl)) != SEC_E_OK)
    {
        trace("AcquireCredentialsHandle() returned %s\n", getSecError(ret));
    }

    ok(ret == SEC_E_OK, "AcquireCredentialsHande() returned %s\n",
            getSecError(ret));

    return ret;
}

/**********************************************************************/

static SECURITY_STATUS setupFakeServer(SspiData *sspi_data, SEC_CHAR *provider)
{
    SECURITY_STATUS ret;
    SecPkgInfoA *sec_pkg_info;

    trace("Running setupFakeServer\n");

    sspi_data->cred = HeapAlloc(GetProcessHeap(), 0, sizeof(CredHandle));
    sspi_data->ctxt = HeapAlloc(GetProcessHeap(), 0, sizeof(CtxtHandle));

    ret = pQuerySecurityPackageInfoA(provider, &sec_pkg_info);

    ok(ret == SEC_E_OK, "QuerySecurityPackageInfo returned %s\n", getSecError(ret));

    ret = setupBuffers(sspi_data, sec_pkg_info);
    
    return ret;
}


/**********************************************************************/

static SECURITY_STATUS runClient(SspiData *sspi_data, BOOL first, ULONG data_rep)
{
    SECURITY_STATUS ret;
    ULONG req_attr = 0;
    ULONG ctxt_attr;
    TimeStamp ttl;
    PSecBufferDesc in_buf = sspi_data->in_buf;
    PSecBufferDesc out_buf = sspi_data->out_buf;

    assert(in_buf->cBuffers >= 1);
    assert(in_buf->pBuffers[0].pvBuffer != NULL);
    assert(in_buf->pBuffers[0].cbBuffer != 0);

    assert(out_buf->cBuffers >= 1);
    assert(out_buf->pBuffers[0].pvBuffer != NULL);
    assert(out_buf->pBuffers[0].cbBuffer != 0);

    trace("Running the client the %s time.\n", first?"first":"second");

    /* We can either use ISC_REQ_ALLOCATE_MEMORY flag to ask the provider
     * always allocate output buffers for us, or initialize cbBuffer
     * before each call because the API changes it to represent actual
     * amount of data in the buffer.
     */

    /* test a failing call only the first time, otherwise we get
     * SEC_E_OUT_OF_SEQUENCE
     */
    if (first)
    {
        void *old_buf;

        /* pass NULL as an output buffer */
        ret = pInitializeSecurityContextA(sspi_data->cred, NULL, NULL, req_attr,
            0, data_rep, NULL, 0, sspi_data->ctxt, NULL,
            &ctxt_attr, &ttl);

        ok(ret == SEC_E_BUFFER_TOO_SMALL, "expected SEC_E_BUFFER_TOO_SMALL, got %s\n", getSecError(ret));

        /* pass NULL as an output buffer */
        old_buf = out_buf->pBuffers[0].pvBuffer;
        out_buf->pBuffers[0].pvBuffer = NULL;

        ret = pInitializeSecurityContextA(sspi_data->cred, NULL, NULL, req_attr, 
            0, data_rep, NULL, 0, sspi_data->ctxt, out_buf,
            &ctxt_attr, &ttl);

        ok(ret == SEC_E_INTERNAL_ERROR, "expected SEC_E_INTERNAL_ERROR, got %s\n", getSecError(ret));

        out_buf->pBuffers[0].pvBuffer = old_buf;

        /* pass an output buffer of 0 size */
        out_buf->pBuffers[0].cbBuffer = 0;

        ret = pInitializeSecurityContextA(sspi_data->cred, NULL, NULL, req_attr, 
            0, data_rep, NULL, 0, sspi_data->ctxt, out_buf,
            &ctxt_attr, &ttl);

        ok(ret == SEC_E_BUFFER_TOO_SMALL, "expected SEC_E_BUFFER_TOO_SMALL, got %s\n", getSecError(ret));

        ok(out_buf->pBuffers[0].cbBuffer == 0,
           "InitializeSecurityContext set buffer size to %lu\n", out_buf->pBuffers[0].cbBuffer);
    }

    out_buf->pBuffers[0].cbBuffer = sspi_data->max_token;

    ret = pInitializeSecurityContextA(sspi_data->cred, first?NULL:sspi_data->ctxt, NULL, req_attr, 
            0, data_rep, first?NULL:in_buf, 0, sspi_data->ctxt, out_buf,
            &ctxt_attr, &ttl);

    if(ret == SEC_I_COMPLETE_AND_CONTINUE || ret == SEC_I_COMPLETE_NEEDED)
    {
        pCompleteAuthToken(sspi_data->ctxt, out_buf);
        if(ret == SEC_I_COMPLETE_AND_CONTINUE)
            ret = SEC_I_CONTINUE_NEEDED;
        else if(ret == SEC_I_COMPLETE_NEEDED)
            ret = SEC_E_OK;
    }       

    ok(out_buf->pBuffers[0].cbBuffer < sspi_data->max_token,
       "InitializeSecurityContext set buffer size to %lu\n", out_buf->pBuffers[0].cbBuffer);

    return ret;
}

/**********************************************************************/

static SECURITY_STATUS runServer(SspiData *sspi_data, BOOL first, ULONG data_rep)
{
    SECURITY_STATUS ret;
    ULONG ctxt_attr;
    TimeStamp ttl;

    trace("Running the server the %s time\n", first?"first":"second");

    ret = pAcceptSecurityContext(sspi_data->cred, first?NULL:sspi_data->ctxt, 
            sspi_data->in_buf, 0, data_rep, sspi_data->ctxt, 
            sspi_data->out_buf, &ctxt_attr, &ttl);

    if(ret == SEC_I_COMPLETE_AND_CONTINUE || ret == SEC_I_COMPLETE_NEEDED)
    {
        pCompleteAuthToken(sspi_data->ctxt, sspi_data->out_buf);
        if(ret == SEC_I_COMPLETE_AND_CONTINUE)
            ret = SEC_I_CONTINUE_NEEDED;
        else if(ret == SEC_I_COMPLETE_NEEDED)
            ret = SEC_E_OK;
    }

    return ret;
}

/**********************************************************************/

static SECURITY_STATUS runFakeServer(SspiData *sspi_data, BOOL first, ULONG data_rep)
{
    trace("Running the fake server the %s time\n", first?"first":"second");

    if(!first)
    {
        sspi_data->out_buf->pBuffers[0].cbBuffer = 0;
        return SEC_E_OK;
    }
    
    if(data_rep == SECURITY_NATIVE_DREP)
    {
        sspi_data->out_buf->pBuffers[0].cbBuffer = sizeof(native_challenge);
        memcpy(sspi_data->out_buf->pBuffers[0].pvBuffer, native_challenge, 
                sspi_data->out_buf->pBuffers[0].cbBuffer);
    }
    else
    {
        sspi_data->out_buf->pBuffers[0].cbBuffer = sizeof(network_challenge);
        memcpy(sspi_data->out_buf->pBuffers[0].pvBuffer, network_challenge, 
                sspi_data->out_buf->pBuffers[0].cbBuffer);
    }

    return SEC_I_CONTINUE_NEEDED;
}

/**********************************************************************/

static void communicate(SspiData *from, SspiData *to)
{
    if(from->out_buf != NULL && to->in_buf != NULL)
    {
        trace("Running communicate.\n");
        if((from->out_buf->cBuffers >= 1) && (to->in_buf->cBuffers >= 1))
        {
            if((from->out_buf->pBuffers[0].pvBuffer != NULL) && 
                    (to->in_buf->pBuffers[0].pvBuffer != NULL))
            {
                memset(to->in_buf->pBuffers[0].pvBuffer, 0, to->max_token);
                
                memcpy(to->in_buf->pBuffers[0].pvBuffer, 
                    from->out_buf->pBuffers[0].pvBuffer,
                    from->out_buf->pBuffers[0].cbBuffer);
                
                to->in_buf->pBuffers[0].cbBuffer = from->out_buf->pBuffers[0].cbBuffer;
                
                memset(from->out_buf->pBuffers[0].pvBuffer, 0, from->max_token);
            }
        }
    }
}
   
/**********************************************************************/

static void testAuth(ULONG data_rep, BOOL fake)
{
    SECURITY_STATUS         client_stat = SEC_I_CONTINUE_NEEDED;
    SECURITY_STATUS         server_stat = SEC_I_CONTINUE_NEEDED;
    SECURITY_STATUS         sec_status;
    PSecPkgInfo             pkg_info = NULL;
    BOOL                    first = TRUE;
    SspiData                client, server;
    SEC_WINNT_AUTH_IDENTITY id;
    SecPkgContext_Sizes     ctxt_sizes;
    static char             sec_pkg_name[] = "NTLM";

    if(pQuerySecurityPackageInfoA( sec_pkg_name, &pkg_info)== SEC_E_OK)
    {
        pFreeContextBuffer(pkg_info);
        id.User = (unsigned char*) "testuser";
        id.UserLength = strlen((char *) id.User);
        id.Domain = (unsigned char *) "WORKGROUP";
        id.DomainLength = strlen((char *) id.Domain);
        id.Password = (unsigned char*) "testpass";
        id.PasswordLength = strlen((char *) id.Password);
        id.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

        client.id = &id;
        
        sec_status = setupClient(&client, sec_pkg_name);

        if(sec_status != SEC_E_OK)
        {
            trace("Error: Setting up the client returned %s, exiting test!\n",
                    getSecError(sec_status));
            pFreeCredentialsHandle(client.cred);
            return;
        }

        if(fake)
            sec_status = setupFakeServer(&server, sec_pkg_name);
        else
            sec_status = setupServer(&server, sec_pkg_name);

        if(sec_status != SEC_E_OK)
        {
            trace("Error: Setting up the server returned %s, exiting test!\n",
                    getSecError(sec_status));
            pFreeCredentialsHandle(server.cred);
            pFreeCredentialsHandle(client.cred);
            return;
        }


        while(client_stat == SEC_I_CONTINUE_NEEDED && server_stat == SEC_I_CONTINUE_NEEDED)
        {
            client_stat = runClient(&client, first, data_rep);

            ok(client_stat == SEC_E_OK || client_stat == SEC_I_CONTINUE_NEEDED,
                    "Running the client returned %s, more tests will fail.\n",
                    getSecError(client_stat));

            communicate(&client, &server);

            if(fake)
                server_stat = runFakeServer(&server, first, data_rep);
            else
                server_stat = runServer(&server, first, data_rep);

            ok(server_stat == SEC_E_OK || server_stat == SEC_I_CONTINUE_NEEDED ||
                    server_stat == SEC_E_LOGON_DENIED,
                    "Running the server returned %s, more tests will fail from now.\n",
                    getSecError(server_stat));

            communicate(&server, &client);
            trace("Looping\n");
            first = FALSE;
        }

        if(client_stat == SEC_E_OK)
        {
            sec_status = pQueryContextAttributesA(client.ctxt,
                    SECPKG_ATTR_SIZES, &ctxt_sizes);

            ok(sec_status == SEC_E_OK, 
                    "pQueryContextAttributesA(SECPKG_ATTR_SIZES) returned %s\n",
                    getSecError(sec_status));
            ok(ctxt_sizes.cbMaxToken == 1904,
                    "cbMaxToken should be 1904 but is %lu\n", 
                    ctxt_sizes.cbMaxToken);
            ok(ctxt_sizes.cbMaxSignature == 16,
                    "cbMaxSignature should be 16 but is %lu\n",
                    ctxt_sizes.cbMaxSignature);
            ok(ctxt_sizes.cbSecurityTrailer == 16,
                    "cbSecurityTrailer should be 16 but is  %lu\n",
                    ctxt_sizes.cbSecurityTrailer);
            ok(ctxt_sizes.cbBlockSize == 0,
                    "cbBlockSize should be 0 but is %lu\n",
                    ctxt_sizes.cbBlockSize);
        }
        else
            trace("Authentication failed, skipping test.\n");

        cleanupBuffers(&client);
        cleanupBuffers(&server);
        
        if(!fake)
        {
            sec_status = pDeleteSecurityContext(server.ctxt);
            ok(sec_status == SEC_E_OK, "DeleteSecurityContext(server) returned %s\n",
                getSecError(sec_status));
        }

        sec_status = pDeleteSecurityContext(client.ctxt);
        ok(sec_status == SEC_E_OK, "DeleteSecurityContext(client) returned %s\n",
                getSecError(sec_status));
        
        if(!fake)
        {   
            sec_status = pFreeCredentialsHandle(server.cred);
            ok(sec_status == SEC_E_OK, "FreeCredentialsHandle(server) returned %s\n",
                getSecError(sec_status));
        }
        
        sec_status = pFreeCredentialsHandle(client.cred);
        ok(sec_status == SEC_E_OK, "FreeCredentialsHandle(client) returned %s\n",
                getSecError(sec_status));
    }
    else
    {
        trace("Package not installed, skipping test.\n");
    }
}

static void testSignSeal()
{
    SECURITY_STATUS         client_stat = SEC_I_CONTINUE_NEEDED;
    SECURITY_STATUS         server_stat = SEC_I_CONTINUE_NEEDED;
    SECURITY_STATUS         sec_status;
    PSecPkgInfo             pkg_info = NULL;
    BOOL                    first = TRUE;
    SspiData                client, server;
    SEC_WINNT_AUTH_IDENTITY id;
    static char             sec_pkg_name[] = "NTLM";
    PSecBufferDesc          crypt = NULL;
    PSecBuffer              data = NULL;
    ULONG                   qop = 0;
    SecPkgContext_Sizes     ctxt_sizes;

    /****************************************************************
     * This is basically the same as in testAuth with a fake server,
     * as we need a valid, authenticated context.
     */
    if(pQuerySecurityPackageInfoA( sec_pkg_name, &pkg_info)== SEC_E_OK)
    {
        pFreeContextBuffer(pkg_info);
        id.User = (unsigned char*) "testuser";
        id.UserLength = strlen((char *) id.User);
        id.Domain = (unsigned char *) "WORKGROUP";
        id.DomainLength = strlen((char *) id.Domain);
        id.Password = (unsigned char*) "testpass";
        id.PasswordLength = strlen((char *) id.Password);
        id.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

        client.id = &id;

        sec_status = setupClient(&client, sec_pkg_name);

        if(sec_status != SEC_E_OK)
        {
            trace("Error: Setting up the client returned %s, exiting test!\n",
                    getSecError(sec_status));
            pFreeCredentialsHandle(client.cred);
            return;
        }

        sec_status = setupFakeServer(&server, sec_pkg_name);

        while(client_stat == SEC_I_CONTINUE_NEEDED && server_stat == SEC_I_CONTINUE_NEEDED)
        {
            client_stat = runClient(&client, first, SECURITY_NETWORK_DREP);

            communicate(&client, &server);

            server_stat = runFakeServer(&server, first, SECURITY_NETWORK_DREP);

            communicate(&server, &client);
            trace("Looping\n");
            first = FALSE;
        }

        /********************************************
         *    Now start with the actual testing     *
         ********************************************/

        if(pQueryContextAttributesA(client.ctxt, SECPKG_ATTR_SIZES,
                    &ctxt_sizes) != SEC_E_OK)
        {
            trace("Failed to get context sizes, aborting test.\n");
            goto end;
        }

        if((crypt = HeapAlloc(GetProcessHeap(), 0, sizeof(SecBufferDesc))) == NULL)
        {
            trace("Failed to allocate the crypto buffer, aborting test.\n");
            goto end;
        }

        crypt->ulVersion = SECBUFFER_VERSION;
        crypt->cBuffers = 2;

        if((data = HeapAlloc(GetProcessHeap(), 0, sizeof(SecBuffer) * 2)) == NULL)
        {
            trace("Failed to allocate the crypto buffer, aborting test.\n");
            goto end;
        }

        crypt->pBuffers = data;

        data[0].BufferType = SECBUFFER_TOKEN;
        data[0].cbBuffer = ctxt_sizes.cbSecurityTrailer;
        data[0].pvBuffer = HeapAlloc(GetProcessHeap(), 0, data[0].cbBuffer);

        data[1].BufferType = SECBUFFER_DATA;
        data[1].cbBuffer = lstrlen(message);
        data[1].pvBuffer = HeapAlloc(GetProcessHeap(), 0, data[1].cbBuffer);
        memcpy(data[1].pvBuffer, message, data[1].cbBuffer);

        /* As we forced NTLM to fall back to a password-derived session key,
         * we should get the same signature for our data, no matter if
         * it is sent by the client or the server
         */
        sec_status = pMakeSignature(client.ctxt, 0, crypt, 0);
        todo_wine {
        ok(sec_status == SEC_E_OK, "MakeSignature returned %s, not SEC_E_OK.\n",
                getSecError(sec_status));
        ok(!memcmp(crypt->pBuffers[0].pvBuffer, message_signature,
                   crypt->pBuffers[0].cbBuffer), "Signature is not as expected.\n");
        }

        data[0].cbBuffer = sizeof(message_signature);
        memcpy(data[0].pvBuffer, message_signature, data[0].cbBuffer);

        sec_status = pVerifySignature(client.ctxt, crypt, 0, &qop);
        todo_wine {
        ok(sec_status == SEC_E_OK, "VerifySignature returned %s, not SEC_E_OK.\n",
                getSecError(sec_status));
        }

        sec_status = pEncryptMessage(client.ctxt, 0, crypt, 0);
        todo_wine{
        ok(sec_status == SEC_E_OK, "EncryptMessage returned %s, not SEC_E_OK.\n",
                getSecError(sec_status));
        ok(!memcmp(crypt->pBuffers[0].pvBuffer, crypt_trailer_client,
                   crypt->pBuffers[0].cbBuffer), "Crypt trailer not as expected.\n");
        ok(!memcmp(crypt->pBuffers[1].pvBuffer, crypt_message_client,
                   crypt->pBuffers[1].cbBuffer), "Crypt message not as expected.\n");
        }

        data[0].cbBuffer = sizeof(crypt_trailer_server);
        data[1].cbBuffer = sizeof(crypt_message_server);
        memcpy(data[0].pvBuffer, crypt_trailer_server, data[0].cbBuffer);
        memcpy(data[1].pvBuffer, crypt_message_server, data[1].cbBuffer);

        sec_status = pDecryptMessage(client.ctxt, crypt, 0, &qop);
        todo_wine {
        ok(sec_status == SEC_E_OK, "DecryptMessage returned %s, not SEC_E_OK.\n",
                getSecError(sec_status));
        ok(!memcmp(crypt->pBuffers[1].pvBuffer, message_binary,
                   crypt->pBuffers[1].cbBuffer),
                "Failed to decrypt message correctly.\n");
        }

end:
        cleanupBuffers(&client);
        cleanupBuffers(&server);

        pDeleteSecurityContext(client.ctxt);
        pFreeCredentialsHandle(client.cred);
        if(data)
        {
            HeapFree(GetProcessHeap(), 0, data[0].pvBuffer);
            HeapFree(GetProcessHeap(), 0, data[1].pvBuffer);
        }
        HeapFree(GetProcessHeap(), 0, data);
        HeapFree(GetProcessHeap(), 0, crypt);
    }
    else
    {
        trace("Package not installed, skipping test.\n");
    }
}

START_TEST(ntlm)
{
    InitFunctionPtrs();

    if(pFreeCredentialsHandle && pDeleteSecurityContext &&
       pDeleteSecurityContext && pAcquireCredentialsHandleA &&
       pInitializeSecurityContextA && pCompleteAuthToken && 
       pQuerySecurityPackageInfoA)
    {
        if(pAcceptSecurityContext)
        {
            testAuth(SECURITY_NATIVE_DREP, TRUE);
            testAuth(SECURITY_NETWORK_DREP, TRUE);
            testAuth(SECURITY_NATIVE_DREP, FALSE);
            testAuth(SECURITY_NETWORK_DREP, FALSE);
        }
        if(pMakeSignature && pVerifySignature && pEncryptMessage &&
           pDecryptMessage)
            testSignSeal();
     }

    if(secdll)
        FreeLibrary(secdll);
}
