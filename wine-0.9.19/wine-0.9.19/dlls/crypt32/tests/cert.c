/*
 * crypt32 cert functions tests
 *
 * Copyright 2005-2006 Juan Lang
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

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <winerror.h>
#include <wincrypt.h>

#include "wine/test.h"

static BOOL (WINAPI * pCryptVerifyCertificateSignatureEx)
                        (HCRYPTPROV, DWORD, DWORD, void *, DWORD, void *, DWORD, void *);

#define CRYPT_GET_PROC(func)                                       \
    p ## func = (void *)GetProcAddress(hCrypt32, #func);           \
    if(!p ## func)                                                 \
        trace("GetProcAddress(hCrypt32, \"%s\") failed\n", #func); \

static void init_function_pointers(void)
{
    HMODULE hCrypt32;

    pCryptVerifyCertificateSignatureEx = NULL;

    hCrypt32 = GetModuleHandleA("crypt32.dll");
    assert(hCrypt32);

    CRYPT_GET_PROC(CryptVerifyCertificateSignatureEx);
}

static const BYTE subjectName[] = { 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06,
 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61,
 0x6e, 0x67, 0x00 };
static const BYTE serialNum[] = { 1 };
static const BYTE bigCert[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE bigCertHash[] = { 0x6e, 0x30, 0x90, 0x71, 0x5f, 0xd9, 0x23,
 0x56, 0xeb, 0xae, 0x25, 0x40, 0xe6, 0x22, 0xda, 0x19, 0x26, 0x02, 0xa6, 0x08 };

static const BYTE bigCertWithDifferentSubject[] = { 0x30, 0x7a, 0x02, 0x01, 0x02,
 0x30, 0x02, 0x06, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55,
 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67,
 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31,
 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15,
 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c,
 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06,
 0x00, 0x03, 0x01, 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55,
 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02,
 0x01, 0x01 };
static const BYTE bigCertWithDifferentIssuer[] = { 0x30, 0x7a, 0x02, 0x01,
 0x01, 0x30, 0x02, 0x06, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e,
 0x67, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30,
 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30,
 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a,
 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02,
 0x06, 0x00, 0x03, 0x01, 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03,
 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff,
 0x02, 0x01, 0x01 };

static const BYTE subjectName2[] = { 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06,
 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61,
 0x6e, 0x67, 0x00 };
static const BYTE bigCert2[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE bigCert2WithDifferentSerial[] = { 0x30, 0x7a, 0x02, 0x01,
 0x02, 0x30, 0x02, 0x06, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41, 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e,
 0x67, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30,
 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30,
 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x41,
 0x6c, 0x65, 0x78, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02,
 0x06, 0x00, 0x03, 0x01, 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03,
 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff,
 0x02, 0x01, 0x01 };
static const BYTE bigCert2Hash[] = { 0x4a, 0x7f, 0x32, 0x1f, 0xcf, 0x3b, 0xc0,
 0x87, 0x48, 0x2b, 0xa1, 0x86, 0x54, 0x18, 0xe4, 0x3a, 0x0e, 0x53, 0x7e, 0x2b };

static const BYTE certWithUsage[] = { 0x30, 0x81, 0x93, 0x02, 0x01, 0x01, 0x30,
 0x02, 0x06, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04,
 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00,
 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31,
 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61,
 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00,
 0x03, 0x01, 0x00, 0xa3, 0x2f, 0x30, 0x2d, 0x30, 0x2b, 0x06, 0x03, 0x55, 0x1d,
 0x25, 0x01, 0x01, 0xff, 0x04, 0x21, 0x30, 0x1f, 0x06, 0x08, 0x2b, 0x06, 0x01,
 0x05, 0x05, 0x07, 0x03, 0x03, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
 0x03, 0x02, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };

static void testAddCert(void)
{
    HCERTSTORE store;

    store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
     CERT_STORE_CREATE_NEW_FLAG, NULL);
    ok(store != NULL, "CertOpenStore failed: %ld\n", GetLastError());
    if (store != NULL)
    {
        HCERTSTORE collection;
        PCCERT_CONTEXT context;
        BOOL ret;

        /* Weird--bad add disposition leads to an access violation in Windows.
         */
        ret = CertAddEncodedCertificateToStore(0, X509_ASN_ENCODING, bigCert,
         sizeof(bigCert), 0, NULL);
        ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
         "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert, sizeof(bigCert), 0, NULL);
        ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
         "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());

        /* Weird--can add a cert to the NULL store (does this have special
         * meaning?)
         */
        context = NULL;
        ret = CertAddEncodedCertificateToStore(0, X509_ASN_ENCODING, bigCert,
         sizeof(bigCert), CERT_STORE_ADD_ALWAYS, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        if (context)
            CertFreeCertificateContext(context);

        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert, sizeof(bigCert), CERT_STORE_ADD_ALWAYS, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert2, sizeof(bigCert2), CERT_STORE_ADD_NEW, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        /* This has the same name as bigCert, so finding isn't done by name */
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         certWithUsage, sizeof(certWithUsage), CERT_STORE_ADD_NEW, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ok(context != NULL, "Expected a context\n");
        if (context)
        {
            CRYPT_DATA_BLOB hash = { sizeof(bigCert2Hash),
             (LPBYTE)bigCert2Hash };

            /* Duplicate (AddRef) the context so we can still use it after
             * deleting it from the store.
             */
            CertDuplicateCertificateContext(context);
            CertDeleteCertificateFromStore(context);
            /* Set the same hash as bigCert2, and try to readd it */
            ret = CertSetCertificateContextProperty(context, CERT_HASH_PROP_ID,
             0, &hash);
            ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
             GetLastError());
            ret = CertAddCertificateContextToStore(store, context,
             CERT_STORE_ADD_NEW, NULL);
            /* The failure is a bit odd (CRYPT_E_ASN1_BADTAG), so just check
             * that it fails.
             */
            ok(!ret, "Expected failure\n");
            CertFreeCertificateContext(context);
        }
        context = CertCreateCertificateContext(X509_ASN_ENCODING, bigCert2,
         sizeof(bigCert2));
        ok(context != NULL, "Expected a context\n");
        if (context)
        {
            /* Try to readd bigCert2 to the store */
            ret = CertAddCertificateContextToStore(store, context,
             CERT_STORE_ADD_NEW, NULL);
            ok(!ret && GetLastError() == CRYPT_E_EXISTS,
             "Expected CRYPT_E_EXISTS, got %08lx\n", GetLastError());
            CertFreeCertificateContext(context);
        }

        /* Adding a cert with the same issuer name and serial number (but
         * different subject) as an existing cert succeeds.
         */
        context = NULL;
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert2WithDifferentSerial, sizeof(bigCert2WithDifferentSerial),
         CERT_STORE_ADD_NEW, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        if (context)
            CertDeleteCertificateFromStore(context);

        /* Adding a cert with the same subject name and serial number (but
         * different issuer) as an existing cert succeeds.
         */
        context = NULL;
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCertWithDifferentSubject, sizeof(bigCertWithDifferentSubject),
         CERT_STORE_ADD_NEW, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        if (context)
            CertDeleteCertificateFromStore(context);

        /* Adding a cert with the same issuer name and serial number (but
         * different otherwise) as an existing cert succeeds.
         */
        context = NULL;
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCertWithDifferentIssuer, sizeof(bigCertWithDifferentIssuer),
         CERT_STORE_ADD_NEW, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        if (context)
            CertDeleteCertificateFromStore(context);

        collection = CertOpenStore(CERT_STORE_PROV_COLLECTION, 0, 0,
         CERT_STORE_CREATE_NEW_FLAG, NULL);
        ok(collection != NULL, "CertOpenStore failed: %08lx\n", GetLastError());
        if (collection)
        {
            /* Add store to the collection, but disable updates */
            CertAddStoreToCollection(collection, store, 0, 0);

            context = CertCreateCertificateContext(X509_ASN_ENCODING, bigCert2,
             sizeof(bigCert2));
            ok(context != NULL, "Expected a context\n");
            if (context)
            {
                /* Try to readd bigCert2 to the collection */
                ret = CertAddCertificateContextToStore(collection, context,
                 CERT_STORE_ADD_NEW, NULL);
                ok(!ret && GetLastError() == CRYPT_E_EXISTS,
                 "Expected CRYPT_E_EXISTS, got %08lx\n", GetLastError());
                /* Replacing an existing certificate context is allowed, even
                 * though updates to the collection aren't..
                 */
                ret = CertAddCertificateContextToStore(collection, context,
                 CERT_STORE_ADD_REPLACE_EXISTING, NULL);
                ok(ret, "CertAddCertificateContextToStore failed: %08lx\n",
                 GetLastError());
                /* but adding a new certificate isn't allowed. */
                ret = CertAddCertificateContextToStore(collection, context,
                 CERT_STORE_ADD_ALWAYS, NULL);
                ok(!ret && GetLastError() == E_ACCESSDENIED,
                 "Expected E_ACCESSDENIED, got %08lx\n", GetLastError());
                CertFreeCertificateContext(context);
            }

            CertCloseStore(collection, 0);
        }

        CertCloseStore(store, 0);
    }
}

static void checkHash(const BYTE *data, DWORD dataLen, ALG_ID algID,
 PCCERT_CONTEXT context, DWORD propID)
{
    BYTE hash[20] = { 0 }, hashProperty[20];
    BOOL ret;
    DWORD size;

    memset(hash, 0, sizeof(hash));
    memset(hashProperty, 0, sizeof(hashProperty));
    size = sizeof(hash);
    ret = CryptHashCertificate(0, algID, 0, data, dataLen, hash, &size);
    ok(ret, "CryptHashCertificate failed: %08lx\n", GetLastError());
    ret = CertGetCertificateContextProperty(context, propID, hashProperty,
     &size);
    ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
     GetLastError());
    ok(!memcmp(hash, hashProperty, size), "Unexpected hash for property %ld\n",
     propID);
}

static const WCHAR cspNameW[] = { 'W','i','n','e','C','r','y','p','t','T','e',
 'm','p',0 };

static void testCertProperties(void)
{
    PCCERT_CONTEXT context = CertCreateCertificateContext(X509_ASN_ENCODING,
     bigCert, sizeof(bigCert));

    ok(context != NULL, "CertCreateCertificateContext failed: %08lx\n",
     GetLastError());
    if (context)
    {
        DWORD propID, numProps, access, size;
        BOOL ret;
        BYTE hash[20] = { 0 }, hashProperty[20];
        CRYPT_DATA_BLOB blob;
        CERT_KEY_CONTEXT keyContext;
        HCRYPTPROV csp;

        /* This crashes
        propID = CertEnumCertificateContextProperties(NULL, 0);
         */

        propID = 0;
        numProps = 0;
        do {
            propID = CertEnumCertificateContextProperties(context, propID);
            if (propID)
                numProps++;
        } while (propID != 0);
        ok(numProps == 0, "Expected 0 properties, got %ld\n", numProps);

        /* Tests with a NULL cert context.  Prop ID 0 fails.. */
        ret = CertSetCertificateContextProperty(NULL, 0, 0, NULL);
        ok(!ret && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        /* while this just crashes.
        ret = CertSetCertificateContextProperty(NULL,
         CERT_KEY_PROV_HANDLE_PROP_ID, 0, NULL);
         */

        ret = CertSetCertificateContextProperty(context, 0, 0, NULL);
        ok(!ret && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        /* Can't set the cert property directly, this crashes.
        ret = CertSetCertificateContextProperty(context,
         CERT_CERT_PROP_ID, 0, bigCert2);
         */

        /* These all crash.
        ret = CertGetCertificateContextProperty(context,
         CERT_ACCESS_STATE_PROP_ID, 0, NULL);
        ret = CertGetCertificateContextProperty(context, CERT_HASH_PROP_ID, 
         NULL, NULL);
        ret = CertGetCertificateContextProperty(context, CERT_HASH_PROP_ID, 
         hashProperty, NULL);
         */
        /* A missing prop */
        size = 0;
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_PROV_INFO_PROP_ID, NULL, &size);
        ok(!ret && GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        /* And, an implicit property */
        size = sizeof(access);
        ret = CertGetCertificateContextProperty(context,
         CERT_ACCESS_STATE_PROP_ID, &access, &size);
        ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        ok(!(access & CERT_ACCESS_STATE_WRITE_PERSIST_FLAG),
         "Didn't expect a persisted cert\n");
        /* Trying to set this "read only" property crashes.
        access |= CERT_ACCESS_STATE_WRITE_PERSIST_FLAG;
        ret = CertSetCertificateContextProperty(context,
         CERT_ACCESS_STATE_PROP_ID, 0, &access);
         */

        /* Can I set the hash to an invalid hash? */
        blob.pbData = hash;
        blob.cbData = sizeof(hash);
        ret = CertSetCertificateContextProperty(context, CERT_HASH_PROP_ID, 0,
         &blob);
        ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        size = sizeof(hashProperty);
        ret = CertGetCertificateContextProperty(context, CERT_HASH_PROP_ID,
         hashProperty, &size);
        ok(!memcmp(hashProperty, hash, sizeof(hash)), "Unexpected hash\n");
        /* Delete the (bogus) hash, and get the real one */
        ret = CertSetCertificateContextProperty(context, CERT_HASH_PROP_ID, 0,
         NULL);
        ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        checkHash(bigCert, sizeof(bigCert), CALG_SHA1, context,
         CERT_HASH_PROP_ID);

        /* Now that the hash property is set, we should get one property when
         * enumerating.
         */
        propID = 0;
        numProps = 0;
        do {
            propID = CertEnumCertificateContextProperties(context, propID);
            if (propID)
                numProps++;
        } while (propID != 0);
        ok(numProps == 1, "Expected 1 properties, got %ld\n", numProps);

        /* Check a few other implicit properties */
        checkHash(bigCert, sizeof(bigCert), CALG_MD5, context,
         CERT_MD5_HASH_PROP_ID);
        checkHash(
         context->pCertInfo->Subject.pbData,
         context->pCertInfo->Subject.cbData,
         CALG_MD5, context, CERT_SUBJECT_NAME_MD5_HASH_PROP_ID);
        checkHash(
         context->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData,
         context->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData,
         CALG_MD5, context, CERT_SUBJECT_PUBLIC_KEY_MD5_HASH_PROP_ID);

        /* Test key identifiers and handles and such */
        size = 0;
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, NULL, &size);
        ok(!ret && GetLastError() == ERROR_INVALID_DATA,
         "Expected ERROR_INVALID_DATA, got %08lx\n", GetLastError());
        size = sizeof(CERT_KEY_CONTEXT);
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, NULL, &size);
        ok(!ret && GetLastError() == ERROR_INVALID_DATA,
         "Expected ERROR_INVALID_DATA, got %08lx\n", GetLastError());
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, &keyContext, &size);
        ok(!ret && GetLastError() == ERROR_INVALID_DATA,
         "Expected ERROR_INVALID_DATA, got %08lx\n", GetLastError());
        /* Key context with an invalid size */
        keyContext.cbSize = 0;
        ret = CertSetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, 0, &keyContext);
        ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        size = sizeof(keyContext);
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, &keyContext, &size);
        ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        keyContext.cbSize = sizeof(keyContext);
        keyContext.hCryptProv = 0;
        keyContext.dwKeySpec = AT_SIGNATURE;
        /* Crash
        ret = CertSetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, 0, &keyContext);
        ret = CertSetCertificateContextProperty(context,
         CERT_KEY_IDENTIFIER_PROP_ID, CERT_STORE_NO_CRYPT_RELEASE_FLAG,
         &keyContext);
         */
        ret = CryptAcquireContextW(&csp, cspNameW,
         MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET);
        ok(ret, "CryptAcquireContextW failed: %08lx\n", GetLastError());
        keyContext.hCryptProv = csp;
        ret = CertSetCertificateContextProperty(context,
         CERT_KEY_CONTEXT_PROP_ID, 0, &keyContext);
        ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        /* Now that that's set, the key prov handle property is also gettable.
         */
        size = sizeof(DWORD);
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_PROV_HANDLE_PROP_ID, &keyContext.hCryptProv, &size);
        ok(ret, "Expected to get the CERT_KEY_PROV_HANDLE_PROP_ID, got %08lx\n",
         GetLastError());
        /* Remove the key prov handle property.. */
        ret = CertSetCertificateContextProperty(context,
         CERT_KEY_PROV_HANDLE_PROP_ID, 0, NULL);
        ok(ret, "CertSetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        /* and the key context's CSP is set to NULL. */
        size = sizeof(keyContext);
        ret = CertGetCertificateContextProperty(context,
         CERT_KEY_CONTEXT_PROP_ID, &keyContext, &size);
        ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
         GetLastError());
        ok(keyContext.hCryptProv == 0, "Expected no hCryptProv\n");

        CryptReleaseContext(csp, 0);

        CertFreeCertificateContext(context);
    }
}

static void testDupCert(void)
{
    HCERTSTORE store;

    store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
     CERT_STORE_CREATE_NEW_FLAG, NULL);
    ok(store != NULL, "CertOpenStore failed: %ld\n", GetLastError());
    if (store != NULL)
    {
        PCCERT_CONTEXT context, dupContext;
        BOOL ret;

        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert, sizeof(bigCert), CERT_STORE_ADD_ALWAYS, &context);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ok(context != NULL, "Expected a valid cert context\n");
        if (context)
        {
            ok(context->cbCertEncoded == sizeof(bigCert),
             "Wrong cert size %ld\n", context->cbCertEncoded);
            ok(!memcmp(context->pbCertEncoded, bigCert, sizeof(bigCert)),
             "Unexpected encoded cert in context\n");
            ok(context->hCertStore == store, "Unexpected store\n");

            dupContext = CertDuplicateCertificateContext(context);
            ok(dupContext != NULL, "Expected valid duplicate\n");
            /* Not only is it a duplicate, it's identical: the address is the
             * same.
             */
            ok(dupContext == context, "Expected identical context addresses\n");
            CertFreeCertificateContext(dupContext);
            CertFreeCertificateContext(context);
        }
        CertCloseStore(store, 0);
    }
}

static void testFindCert(void)
{
    HCERTSTORE store;

    store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
     CERT_STORE_CREATE_NEW_FLAG, NULL);
    ok(store != NULL, "CertOpenStore failed: %ld\n", GetLastError());
    if (store)
    {
        PCCERT_CONTEXT context = NULL;
        BOOL ret;
        CERT_INFO certInfo = { 0 };
        CRYPT_HASH_BLOB blob;

        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert, sizeof(bigCert), CERT_STORE_ADD_NEW, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert2, sizeof(bigCert2), CERT_STORE_ADD_NEW, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        /* This has the same name as bigCert */
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         certWithUsage, sizeof(certWithUsage), CERT_STORE_ADD_NEW, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());

        /* Crashes
        context = CertFindCertificateInStore(NULL, 0, 0, 0, NULL, NULL);
         */

        /* Check first cert's there, by issuer */
        certInfo.Subject.pbData = (LPBYTE)subjectName;
        certInfo.Subject.cbData = sizeof(subjectName);
        certInfo.SerialNumber.pbData = (LPBYTE)serialNum;
        certInfo.SerialNumber.cbData = sizeof(serialNum);
        context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
         CERT_FIND_ISSUER_NAME, &certInfo.Subject, NULL);
        ok(context != NULL, "CertFindCertificateInStore failed: %08lx\n",
         GetLastError());
        if (context)
        {
            context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
             CERT_FIND_ISSUER_NAME, &certInfo.Subject, context);
            ok(context != NULL, "Expected more than one cert\n");
            if (context)
            {
                context = CertFindCertificateInStore(store, X509_ASN_ENCODING,
                 0, CERT_FIND_ISSUER_NAME, &certInfo.Subject, context);
                ok(context == NULL, "Expected precisely two certs\n");
            }
        }

        /* Check second cert's there as well, by subject name */
        certInfo.Subject.pbData = (LPBYTE)subjectName2;
        certInfo.Subject.cbData = sizeof(subjectName2);
        context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
         CERT_FIND_SUBJECT_NAME, &certInfo.Subject, NULL);
        ok(context != NULL, "CertFindCertificateInStore failed: %08lx\n",
         GetLastError());
        if (context)
        {
            context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
             CERT_FIND_ISSUER_NAME, &certInfo.Subject, context);
            ok(context == NULL, "Expected one cert only\n");
        }

        /* Strange but true: searching for the subject cert requires you to set
         * the issuer, not the subject
         */
        context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
         CERT_FIND_SUBJECT_CERT, &certInfo.Subject, NULL);
        ok(context == NULL, "Expected no certificate\n");
        certInfo.Subject.pbData = NULL;
        certInfo.Subject.cbData = 0;
        certInfo.Issuer.pbData = (LPBYTE)subjectName2;
        certInfo.Issuer.cbData = sizeof(subjectName2);
        context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
         CERT_FIND_SUBJECT_CERT, &certInfo, NULL);
        ok(context != NULL, "CertFindCertificateInStore failed: %08lx\n",
         GetLastError());
        if (context)
        {
            context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
             CERT_FIND_ISSUER_NAME, &certInfo.Subject, context);
            ok(context == NULL, "Expected one cert only\n");
        }

        /* The nice thing about hashes, they're unique */
        blob.pbData = (LPBYTE)bigCertHash;
        blob.cbData = sizeof(bigCertHash);
        context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
         CERT_FIND_SHA1_HASH, &blob, NULL);
        ok(context != NULL, "CertFindCertificateInStore failed: %08lx\n",
         GetLastError());
        if (context)
        {
            context = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
             CERT_FIND_ISSUER_NAME, &certInfo.Subject, context);
            ok(context == NULL, "Expected one cert only\n");
        }

        CertCloseStore(store, 0);
    }
}

static void testGetSubjectCert(void)
{
    HCERTSTORE store;

    store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
     CERT_STORE_CREATE_NEW_FLAG, NULL);
    ok(store != NULL, "CertOpenStore failed: %ld\n", GetLastError());
    if (store != NULL)
    {
        PCCERT_CONTEXT context1, context2;
        CERT_INFO info = { 0 };
        BOOL ret;

        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert, sizeof(bigCert), CERT_STORE_ADD_ALWAYS, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         bigCert2, sizeof(bigCert2), CERT_STORE_ADD_NEW, &context1);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());
        ok(context1 != NULL, "Expected a context\n");
        ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
         certWithUsage, sizeof(certWithUsage), CERT_STORE_ADD_NEW, NULL);
        ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
         GetLastError());

        context2 = CertGetSubjectCertificateFromStore(store, X509_ASN_ENCODING,
         NULL);
        ok(!context2 && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        context2 = CertGetSubjectCertificateFromStore(store, X509_ASN_ENCODING,
         &info);
        ok(!context2 && GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        info.SerialNumber.cbData = sizeof(serialNum);
        info.SerialNumber.pbData = (LPBYTE)serialNum;
        context2 = CertGetSubjectCertificateFromStore(store, X509_ASN_ENCODING,
         &info);
        ok(!context2 && GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        info.Issuer.cbData = sizeof(subjectName2);
        info.Issuer.pbData = (LPBYTE)subjectName2;
        context2 = CertGetSubjectCertificateFromStore(store, X509_ASN_ENCODING,
         &info);
        ok(context2 != NULL,
         "CertGetSubjectCertificateFromStore failed: %08lx\n", GetLastError());
        /* Not only should this find a context, but it should be the same
         * (same address) as context1.
         */
        ok(context1 == context2, "Expected identical context addresses\n");
        CertFreeCertificateContext(context2);

        CertFreeCertificateContext(context1);
        CertCloseStore(store, 0);
    }
}

/* This expires in 1970 or so */
static const BYTE expiredCert[] = { 0x30, 0x82, 0x01, 0x33, 0x30, 0x81, 0xe2,
 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x10, 0xc4, 0xd7, 0x7f, 0x0e, 0x6f, 0xa6,
 0x8c, 0xaa, 0x47, 0x47, 0x40, 0xe7, 0xb7, 0x0b, 0x4a, 0x7f, 0x30, 0x09, 0x06,
 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1d, 0x05, 0x00, 0x30, 0x1f, 0x31, 0x1d, 0x30,
 0x1b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x14, 0x61, 0x72, 0x69, 0x63, 0x40,
 0x63, 0x6f, 0x64, 0x65, 0x77, 0x65, 0x61, 0x76, 0x65, 0x72, 0x73, 0x2e, 0x63,
 0x6f, 0x6d, 0x30, 0x1e, 0x17, 0x0d, 0x36, 0x39, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x17, 0x0d, 0x37, 0x30, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x36, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x1f, 0x31, 0x1d, 0x30,
 0x1b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x14, 0x61, 0x72, 0x69, 0x63, 0x40,
 0x63, 0x6f, 0x64, 0x65, 0x77, 0x65, 0x61, 0x76, 0x65, 0x72, 0x73, 0x2e, 0x63,
 0x6f, 0x6d, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41,
 0x00, 0xa1, 0xaf, 0x4a, 0xea, 0xa7, 0x83, 0x57, 0xc0, 0x37, 0x33, 0x7e, 0x29,
 0x5e, 0x0d, 0xfc, 0x44, 0x74, 0x3a, 0x1d, 0xc3, 0x1b, 0x1d, 0x96, 0xed, 0x4e,
 0xf4, 0x1b, 0x98, 0xec, 0x69, 0x1b, 0x04, 0xea, 0x25, 0xcf, 0xb3, 0x2a, 0xf5,
 0xd9, 0x22, 0xd9, 0x8d, 0x08, 0x39, 0x81, 0xc6, 0xe0, 0x4f, 0x12, 0x37, 0x2a,
 0x3f, 0x80, 0xa6, 0x6c, 0x67, 0x43, 0x3a, 0xdd, 0x95, 0x0c, 0xbb, 0x2f, 0x6b,
 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02,
 0x1d, 0x05, 0x00, 0x03, 0x41, 0x00, 0x8f, 0xa2, 0x5b, 0xd6, 0xdf, 0x34, 0xd0,
 0xa2, 0xa7, 0x47, 0xf1, 0x13, 0x79, 0xd3, 0xf3, 0x39, 0xbd, 0x4e, 0x2b, 0xa3,
 0xf4, 0x63, 0x37, 0xac, 0x5a, 0x0c, 0x5e, 0x4d, 0x0d, 0x54, 0x87, 0x4f, 0x31,
 0xfb, 0xa0, 0xce, 0x8f, 0x9a, 0x2f, 0x4d, 0x48, 0xc6, 0x84, 0x8d, 0xf5, 0x70,
 0x74, 0x17, 0xa5, 0xf3, 0x66, 0x47, 0x06, 0xd6, 0x64, 0x45, 0xbc, 0x52, 0xef,
 0x49, 0xe5, 0xf9, 0x65, 0xf3 };

/* This expires in 2036 or so */
static const BYTE childOfExpired[] = { 0x30, 0x81, 0xcc, 0x30, 0x78, 0xa0,
 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86,
 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x1f, 0x31, 0x1d,
 0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x14, 0x61, 0x72, 0x69, 0x63,
 0x40, 0x63, 0x6f, 0x64, 0x65, 0x77, 0x65, 0x61, 0x76, 0x65, 0x72, 0x73, 0x2e,
 0x63, 0x6f, 0x6d, 0x30, 0x1e, 0x17, 0x0d, 0x30, 0x36, 0x30, 0x35, 0x30, 0x35,
 0x31, 0x37, 0x31, 0x32, 0x34, 0x39, 0x5a, 0x17, 0x0d, 0x33, 0x36, 0x30, 0x35,
 0x30, 0x35, 0x31, 0x37, 0x31, 0x32, 0x34, 0x39, 0x5a, 0x30, 0x15, 0x31, 0x13,
 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e,
 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03,
 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
 0x01, 0x05, 0x05, 0x00, 0x03, 0x41, 0x00, 0x20, 0x3b, 0xdb, 0x4d, 0x67, 0x50,
 0xec, 0x73, 0x9d, 0xf9, 0x85, 0x5d, 0x18, 0xe9, 0xb4, 0x98, 0xe3, 0x31, 0xb7,
 0x03, 0x0b, 0xc0, 0x39, 0x93, 0x56, 0x81, 0x0a, 0xfc, 0x78, 0xa8, 0x29, 0x42,
 0x5f, 0x69, 0xfb, 0xbc, 0x5b, 0xf2, 0xa6, 0x2a, 0xbe, 0x91, 0x2c, 0xfc, 0x89,
 0x69, 0x15, 0x18, 0x58, 0xe5, 0x02, 0x75, 0xf7, 0x2a, 0xb6, 0xa9, 0xfb, 0x47,
 0x6a, 0x6e, 0x0a, 0x9b, 0xe9, 0xdc };

static void testGetIssuerCert(void)
{
    BOOL ret;
    PCCERT_CONTEXT parent, child;
    DWORD flags = 0xffffffff;
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
     CERT_STORE_CREATE_NEW_FLAG, NULL);

    ok(store != NULL, "CertOpenStore failed: %08lx\n", GetLastError());

    ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
     expiredCert, sizeof(expiredCert), CERT_STORE_ADD_ALWAYS, NULL);
    ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
     GetLastError());

    ret = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING,
     childOfExpired, sizeof(childOfExpired), CERT_STORE_ADD_ALWAYS, &child);
    ok(ret, "CertAddEncodedCertificateToStore failed: %08lx\n",
     GetLastError());

    /* These crash:
    parent = CertGetIssuerCertificateFromStore(NULL, NULL, NULL, NULL);
    parent = CertGetIssuerCertificateFromStore(store, NULL, NULL, NULL);
     */
    parent = CertGetIssuerCertificateFromStore(NULL, NULL, NULL, &flags);
    ok(!parent && GetLastError() == E_INVALIDARG,
     "Expected E_INVALIDARG, got %08lx\n", GetLastError());
    parent = CertGetIssuerCertificateFromStore(store, NULL, NULL, &flags);
    ok(!parent && GetLastError() == E_INVALIDARG,
     "Expected E_INVALIDARG, got %08lx\n", GetLastError());
    parent = CertGetIssuerCertificateFromStore(store, child, NULL, &flags);
    ok(!parent && GetLastError() == E_INVALIDARG,
     "Expected E_INVALIDARG, got %08lx\n", GetLastError());
    /* Confusing: the caller cannot set either of the
     * CERT_STORE_NO_*_FLAGs, as these are not checks,
     * they're results:
     */
    flags = CERT_STORE_NO_CRL_FLAG | CERT_STORE_NO_ISSUER_FLAG;
    parent = CertGetIssuerCertificateFromStore(store, child, NULL, &flags);
    ok(!parent && GetLastError() == E_INVALIDARG,
     "Expected E_INVALIDARG, got %08lx\n", GetLastError());
    /* Perform no checks */
    flags = 0;
    parent = CertGetIssuerCertificateFromStore(store, child, NULL, &flags);
    ok(parent != NULL, "CertGetIssuerCertificateFromStore failed: %08lx\n",
     GetLastError());
    if (parent)
        CertFreeCertificateContext(parent);
    /* Check revocation and signature only */
    flags = CERT_STORE_REVOCATION_FLAG | CERT_STORE_SIGNATURE_FLAG;
    parent = CertGetIssuerCertificateFromStore(store, child, NULL, &flags);
    ok(parent != NULL, "CertGetIssuerCertificateFromStore failed: %08lx\n",
     GetLastError());
    /* Confusing: CERT_STORE_REVOCATION_FLAG succeeds when there is no CRL by
     * setting CERT_STORE_NO_CRL_FLAG.
     */
    ok(flags == (CERT_STORE_REVOCATION_FLAG | CERT_STORE_NO_CRL_FLAG),
     "Expected CERT_STORE_REVOCATION_FLAG | CERT_STORE_NO_CRL_FLAG, got %08lx\n",
     flags);
    if (parent)
        CertFreeCertificateContext(parent);
    /* Now check just the time */
    flags = CERT_STORE_TIME_VALIDITY_FLAG;
    parent = CertGetIssuerCertificateFromStore(store, child, NULL, &flags);
    ok(parent != NULL, "CertGetIssuerCertificateFromStore failed: %08lx\n",
     GetLastError());
    /* Oops: the child is not expired, so the time validity check actually
     * succeeds, even though the signing cert is expired.
     */
    ok(!flags, "Expected check to succeed, got %08lx\n", flags);
    if (parent)
        CertFreeCertificateContext(parent);

    CertFreeCertificateContext(child);
    CertCloseStore(store, 0);
}

static void testCryptHashCert(void)
{
    static const BYTE emptyHash[] = { 0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b,
     0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07,
     0x09 };
    static const BYTE knownHash[] = { 0xae, 0x9d, 0xbf, 0x6d, 0xf5, 0x46, 0xee,
     0x8b, 0xc5, 0x7a, 0x13, 0xba, 0xc2, 0xb1, 0x04, 0xf2, 0xbf, 0x52, 0xa8,
     0xa2 };
    static const BYTE toHash[] = "abcdefghijklmnopqrstuvwxyz0123456789.,;!?:";
    BOOL ret;
    BYTE hash[20];
    DWORD hashLen = sizeof(hash);

    /* NULL buffer and nonzero length crashes
    ret = CryptHashCertificate(0, 0, 0, NULL, size, hash, &hashLen);
       empty hash length also crashes
    ret = CryptHashCertificate(0, 0, 0, buf, size, hash, NULL);
     */
    /* Test empty hash */
    ret = CryptHashCertificate(0, 0, 0, toHash, sizeof(toHash), NULL,
     &hashLen);
    ok(ret, "CryptHashCertificate failed: %08lx\n", GetLastError());
    ok(hashLen == sizeof(hash), "Got unexpected size of hash %ld\n", hashLen);
    /* Test with empty buffer */
    ret = CryptHashCertificate(0, 0, 0, NULL, 0, hash, &hashLen);
    ok(ret, "CryptHashCertificate failed: %08lx\n", GetLastError());
    ok(!memcmp(hash, emptyHash, sizeof(emptyHash)),
     "Unexpected hash of nothing\n");
    /* Test a known value */
    ret = CryptHashCertificate(0, 0, 0, toHash, sizeof(toHash), hash,
     &hashLen);
    ok(ret, "CryptHashCertificate failed: %08lx\n", GetLastError());
    ok(!memcmp(hash, knownHash, sizeof(knownHash)), "Unexpected hash\n");
}

static void verifySig(HCRYPTPROV csp, const BYTE *toSign, size_t toSignLen,
 const BYTE *sig, unsigned int sigLen)
{
    HCRYPTHASH hash;
    BOOL ret = CryptCreateHash(csp, CALG_SHA1, 0, 0, &hash);

    ok(ret, "CryptCreateHash failed: %08lx\n", GetLastError());
    if (ret)
    {
        BYTE mySig[64];
        DWORD mySigSize = sizeof(mySig);

        ret = CryptHashData(hash, toSign, toSignLen, 0);
        ok(ret, "CryptHashData failed: %08lx\n", GetLastError());
        /* use the A variant so the test can run on Win9x */
        ret = CryptSignHashA(hash, AT_SIGNATURE, NULL, 0, mySig, &mySigSize);
        ok(ret, "CryptSignHash failed: %08lx\n", GetLastError());
        if (ret)
        {
            ok(mySigSize == sigLen, "Expected sig length %d, got %ld\n",
             sigLen, mySigSize);
            ok(!memcmp(mySig, sig, sigLen), "Unexpected signature\n");
        }
        CryptDestroyHash(hash);
    }
}

/* Tests signing the certificate described by toBeSigned with the CSP passed in,
 * using the algorithm with OID sigOID.  The CSP is assumed to be empty, and a
 * keyset named AT_SIGNATURE will be added to it.  The signing key will be
 * stored in *key, and the signature will be stored in sig.  sigLen should be
 * at least 64 bytes.
 */
static void testSignCert(HCRYPTPROV csp, const CRYPT_DATA_BLOB *toBeSigned,
 LPCSTR sigOID, HCRYPTKEY *key, BYTE *sig, DWORD *sigLen)
{
    BOOL ret;
    DWORD size = 0;
    CRYPT_ALGORITHM_IDENTIFIER algoID = { NULL, { 0, NULL } };

    /* These all crash
    ret = CryptSignCertificate(0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL);
    ret = CryptSignCertificate(0, 0, 0, NULL, 0, NULL, NULL, NULL, &size);
    ret = CryptSignCertificate(0, 0, 0, toBeSigned->pbData, toBeSigned->cbData,
     NULL, NULL, NULL, &size);
     */
    ret = CryptSignCertificate(0, 0, 0, toBeSigned->pbData, toBeSigned->cbData,
     &algoID, NULL, NULL, &size);
    ok(!ret && GetLastError() == NTE_BAD_ALGID, 
     "Expected NTE_BAD_ALGID, got %08lx\n", GetLastError());
    algoID.pszObjId = (LPSTR)sigOID;
    ret = CryptSignCertificate(0, 0, 0, toBeSigned->pbData, toBeSigned->cbData,
     &algoID, NULL, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    ret = CryptSignCertificate(0, AT_SIGNATURE, 0, toBeSigned->pbData,
     toBeSigned->cbData, &algoID, NULL, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());

    /* No keys exist in the new CSP yet.. */
    ret = CryptSignCertificate(csp, AT_SIGNATURE, 0, toBeSigned->pbData,
     toBeSigned->cbData, &algoID, NULL, NULL, &size);
    ok(!ret && (GetLastError() == NTE_BAD_KEYSET || GetLastError() ==
     NTE_NO_KEY), "Expected NTE_BAD_KEYSET or NTE_NO_KEY, got %08lx\n",
     GetLastError());
    ret = CryptGenKey(csp, AT_SIGNATURE, 0, key);
    ok(ret, "CryptGenKey failed: %08lx\n", GetLastError());
    if (ret)
    {
        ret = CryptSignCertificate(csp, AT_SIGNATURE, 0, toBeSigned->pbData,
         toBeSigned->cbData, &algoID, NULL, NULL, &size);
        ok(ret, "CryptSignCertificate failed: %08lx\n", GetLastError());
        ok(size <= *sigLen, "Expected size <= %ld, got %ld\n", *sigLen, size);
        if (ret)
        {
            ret = CryptSignCertificate(csp, AT_SIGNATURE, 0, toBeSigned->pbData,
             toBeSigned->cbData, &algoID, NULL, sig, &size);
            ok(ret, "CryptSignCertificate failed: %08lx\n", GetLastError());
            if (ret)
            {
                *sigLen = size;
                verifySig(csp, toBeSigned->pbData, toBeSigned->cbData, sig,
                 size);
            }
        }
    }
}

static void testVerifyCertSig(HCRYPTPROV csp, const CRYPT_DATA_BLOB *toBeSigned,
 LPCSTR sigOID, const BYTE *sig, DWORD sigLen)
{
    CERT_SIGNED_CONTENT_INFO info;
    LPBYTE cert = NULL;
    DWORD size = 0;
    BOOL ret;

    if(pCryptVerifyCertificateSignatureEx) {
        ret = pCryptVerifyCertificateSignatureEx(0, 0, 0, NULL, 0, NULL, 0, NULL);
        ok(!ret && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        ret = pCryptVerifyCertificateSignatureEx(csp, 0, 0, NULL, 0, NULL, 0, NULL);
        ok(!ret && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING, 0, NULL, 0,
         NULL, 0, NULL);
        ok(!ret && GetLastError() == E_INVALIDARG,
         "Expected E_INVALIDARG, got %08lx\n", GetLastError());
        /* This crashes
        ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
         CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, NULL, 0, NULL, 0, NULL);
         */
    }
    info.ToBeSigned.cbData = toBeSigned->cbData;
    info.ToBeSigned.pbData = toBeSigned->pbData;
    info.SignatureAlgorithm.pszObjId = (LPSTR)sigOID;
    info.SignatureAlgorithm.Parameters.cbData = 0;
    info.Signature.cbData = sigLen;
    info.Signature.pbData = (BYTE *)sig;
    info.Signature.cUnusedBits = 0;
    ret = CryptEncodeObjectEx(X509_ASN_ENCODING, X509_CERT, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&cert, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (cert)
    {
        CRYPT_DATA_BLOB certBlob = { 0, NULL };
        PCERT_PUBLIC_KEY_INFO pubKeyInfo = NULL;

        if(pCryptVerifyCertificateSignatureEx) {
            ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob, 0, NULL, 0, NULL);
            ok(!ret && GetLastError() == CRYPT_E_ASN1_EOD,
             "Expected CRYPT_E_ASN1_EOD, got %08lx\n", GetLastError());
            certBlob.cbData = 1;
            certBlob.pbData = (void *)0xdeadbeef;
            ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob, 0, NULL, 0, NULL);
            ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
             "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
            certBlob.cbData = size;
            certBlob.pbData = cert;
            ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob, 0, NULL, 0, NULL);
            ok(!ret && GetLastError() == E_INVALIDARG,
             "Expected E_INVALIDARG, got %08lx\n", GetLastError());
            ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob,
             CRYPT_VERIFY_CERT_SIGN_ISSUER_NULL, NULL, 0, NULL);
            ok(!ret && GetLastError() == E_INVALIDARG,
             "Expected E_INVALIDARG, got %08lx\n", GetLastError());
            /* This crashes
            ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
             CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob,
             CRYPT_VERIFY_CERT_SIGN_ISSUER_PUBKEY, NULL, 0, NULL);
             */
        }
        CryptExportPublicKeyInfoEx(csp, AT_SIGNATURE, X509_ASN_ENCODING,
         (LPSTR)sigOID, 0, NULL, NULL, &size);
        pubKeyInfo = HeapAlloc(GetProcessHeap(), 0, size);
        if (pubKeyInfo)
        {
            ret = CryptExportPublicKeyInfoEx(csp, AT_SIGNATURE,
             X509_ASN_ENCODING, (LPSTR)sigOID, 0, NULL, pubKeyInfo, &size);
            ok(ret, "CryptExportKey failed: %08lx\n", GetLastError());
            if (ret && pCryptVerifyCertificateSignatureEx)
            {
                ret = pCryptVerifyCertificateSignatureEx(csp, X509_ASN_ENCODING,
                 CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB, &certBlob,
                 CRYPT_VERIFY_CERT_SIGN_ISSUER_PUBKEY, pubKeyInfo, 0, NULL);
                ok(ret, "CryptVerifyCertificateSignatureEx failed: %08lx\n",
                 GetLastError());
            }
            HeapFree(GetProcessHeap(), 0, pubKeyInfo);
        }
        LocalFree(cert);
    }
}

static const BYTE emptyCert[] = { 0x30, 0x00 };

static void testCertSigs(void)
{
    HCRYPTPROV csp;
    CRYPT_DATA_BLOB toBeSigned = { sizeof(emptyCert), (LPBYTE)emptyCert };
    BOOL ret;
    HCRYPTKEY key;
    BYTE sig[64];
    DWORD sigSize = sizeof(sig);

    /* Just in case a previous run failed, delete this thing */
    CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    ret = CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ok(ret, "CryptAcquireContext failed: %08lx\n", GetLastError());

    testSignCert(csp, &toBeSigned, szOID_RSA_SHA1RSA, &key, sig, &sigSize);
    testVerifyCertSig(csp, &toBeSigned, szOID_RSA_SHA1RSA, sig, sigSize);

    CryptDestroyKey(key);
    CryptReleaseContext(csp, 0);
    ret = CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
}

static void testCreateSelfSignCert(void)
{
    PCCERT_CONTEXT context;
    CERT_NAME_BLOB name = { sizeof(subjectName), (LPBYTE)subjectName };
    HCRYPTPROV csp;
    BOOL ret;
    HCRYPTKEY key;

    /* This crashes:
    context = CertCreateSelfSignCertificate(0, NULL, 0, NULL, NULL, NULL, NULL,
     NULL);
     * Calling this with no first parameter creates a new key container, which
     * lasts beyond the test, so I don't test that.  Nb: the generated key
     * name is a GUID.
    context = CertCreateSelfSignCertificate(0, &name, 0, NULL, NULL, NULL, NULL,
     NULL);
     */

    /* Acquire a CSP */
    CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    ret = CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ok(ret, "CryptAcquireContext failed: %08lx\n", GetLastError());

    context = CertCreateSelfSignCertificate(csp, &name, 0, NULL, NULL, NULL,
     NULL, NULL);
    ok(!context && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    ret = CryptGenKey(csp, AT_SIGNATURE, 0, &key);
    ok(ret, "CryptGenKey failed: %08lx\n", GetLastError());
    if (ret)
    {
        context = CertCreateSelfSignCertificate(csp, &name, 0, NULL, NULL, NULL,
         NULL, NULL);
        ok(context != NULL, "CertCreateSelfSignCertificate failed: %08lx\n",
         GetLastError());
        if (context)
        {
            DWORD size = 0;
            PCRYPT_KEY_PROV_INFO info;

            /* The context must have a key provider info property */
            ret = CertGetCertificateContextProperty(context,
             CERT_KEY_PROV_INFO_PROP_ID, NULL, &size);
            ok(ret && size, "Expected non-zero key provider info\n");
            if (size)
            {
                info = HeapAlloc(GetProcessHeap(), 0, size);
                if (info)
                {
                    ret = CertGetCertificateContextProperty(context,
                     CERT_KEY_PROV_INFO_PROP_ID, info, &size);
                    ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
                     GetLastError());
                    if (ret)
                    {
                        /* Sanity-check the key provider */
                        ok(!lstrcmpW(info->pwszContainerName, cspNameW),
                         "Unexpected key container\n");
                        ok(!lstrcmpW(info->pwszProvName, MS_DEF_PROV_W),
                         "Unexpected provider\n");
                        ok(info->dwKeySpec == AT_SIGNATURE,
                         "Expected AT_SIGNATURE, got %ld\n", info->dwKeySpec);
                    }
                    HeapFree(GetProcessHeap(), 0, info);
                }
            }

            CertFreeCertificateContext(context);
        }

        CryptDestroyKey(key);
    }

    CryptReleaseContext(csp, 0);
    ret = CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
}

static const LPCSTR keyUsages[] = { szOID_PKIX_KP_CODE_SIGNING,
 szOID_PKIX_KP_CLIENT_AUTH, szOID_RSA_RSA };

static void testKeyUsage(void)
{
    BOOL ret;
    PCCERT_CONTEXT context;
    DWORD size;

    /* Test base cases */
    ret = CertGetEnhancedKeyUsage(NULL, 0, NULL, NULL);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    size = 1;
    ret = CertGetEnhancedKeyUsage(NULL, 0, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    size = 0;
    ret = CertGetEnhancedKeyUsage(NULL, 0, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    /* These crash
    ret = CertSetEnhancedKeyUsage(NULL, NULL);
    usage.cUsageIdentifier = 0;
    ret = CertSetEnhancedKeyUsage(NULL, &usage);
     */
    /* Test with a cert with no enhanced key usage extension */
    context = CertCreateCertificateContext(X509_ASN_ENCODING, bigCert,
     sizeof(bigCert));
    ok(context != NULL, "CertCreateCertificateContext failed: %08lx\n",
     GetLastError());
    if (context)
    {
        static const char oid[] = "1.2.3.4";
        BYTE buf[sizeof(CERT_ENHKEY_USAGE) + 2 * (sizeof(LPSTR) + sizeof(oid))];
        PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

        ret = CertGetEnhancedKeyUsage(context, 0, NULL, NULL);
        ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
         "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
        size = 1;
        ret = CertGetEnhancedKeyUsage(context, 0, NULL, &size);
        if (ret)
        {
            /* Windows 2000, ME, or later: even though it succeeded, we expect
             * CRYPT_E_NOT_FOUND, which indicates there is no enhanced key
             * usage set for this cert (which implies it's valid for all uses.)
             */
            ok(GetLastError() == CRYPT_E_NOT_FOUND,
             "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
            ok(size == sizeof(CERT_ENHKEY_USAGE), "Wrong size %ld\n", size);
            ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
            ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 0, "Expected 0 usages, got %ld\n",
             pUsage->cUsageIdentifier);
        }
        else
        {
            /* Windows NT, 95, or 98: it fails, and the last error is
             * CRYPT_E_NOT_FOUND.
             */
            ok(GetLastError() == CRYPT_E_NOT_FOUND,
             "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        }
        /* I can add a usage identifier when no key usage has been set */
        ret = CertAddEnhancedKeyUsageIdentifier(context, oid);
        ok(ret, "CertAddEnhancedKeyUsageIdentifier failed: %08lx\n",
         GetLastError());
        size = sizeof(buf);
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, pUsage, &size);
        ok(ret && GetLastError() == 0,
         "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ok(pUsage->cUsageIdentifier == 1, "Expected 1 usage, got %ld\n",
         pUsage->cUsageIdentifier);
        if (pUsage->cUsageIdentifier)
            ok(!strcmp(pUsage->rgpszUsageIdentifier[0], oid),
             "Expected %s, got %s\n", oid, pUsage->rgpszUsageIdentifier[0]);
        /* Now set an empty key usage */
        pUsage->cUsageIdentifier = 0;
        ret = CertSetEnhancedKeyUsage(context, pUsage);
        ok(ret, "CertSetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        /* Shouldn't find it in the cert */
        size = sizeof(buf);
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, pUsage, &size);
        ok(!ret && GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        /* Should find it as an extended property */
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, pUsage, &size);
        ok(ret && GetLastError() == 0,
         "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ok(pUsage->cUsageIdentifier == 0, "Expected 0 usages, got %ld\n",
         pUsage->cUsageIdentifier);
        /* Should find it as either */
        ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
        ok(ret && GetLastError() == 0,
         "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ok(pUsage->cUsageIdentifier == 0, "Expected 0 usages, got %ld\n",
         pUsage->cUsageIdentifier);
        /* Add a usage identifier */
        ret = CertAddEnhancedKeyUsageIdentifier(context, oid);
        ok(ret, "CertAddEnhancedKeyUsageIdentifier failed: %08lx\n",
         GetLastError());
        size = sizeof(buf);
        ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
        ok(ret && GetLastError() == 0,
         "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ok(pUsage->cUsageIdentifier == 1, "Expected 1 identifier, got %ld\n",
         pUsage->cUsageIdentifier);
        if (pUsage->cUsageIdentifier)
            ok(!strcmp(pUsage->rgpszUsageIdentifier[0], oid),
             "Expected %s, got %s\n", oid, pUsage->rgpszUsageIdentifier[0]);
        /* Yep, I can re-add the same usage identifier */
        ret = CertAddEnhancedKeyUsageIdentifier(context, oid);
        ok(ret, "CertAddEnhancedKeyUsageIdentifier failed: %08lx\n",
         GetLastError());
        size = sizeof(buf);
        ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
        ok(ret && GetLastError() == 0,
         "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ok(pUsage->cUsageIdentifier == 2, "Expected 2 identifiers, got %ld\n",
         pUsage->cUsageIdentifier);
        if (pUsage->cUsageIdentifier)
            ok(!strcmp(pUsage->rgpszUsageIdentifier[0], oid),
             "Expected %s, got %s\n", oid, pUsage->rgpszUsageIdentifier[0]);
        if (pUsage->cUsageIdentifier >= 2)
            ok(!strcmp(pUsage->rgpszUsageIdentifier[1], oid),
             "Expected %s, got %s\n", oid, pUsage->rgpszUsageIdentifier[1]);
        /* Now set a NULL extended property--this deletes the property. */
        ret = CertSetEnhancedKeyUsage(context, NULL);
        ok(ret, "CertSetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        SetLastError(0xbaadcafe);
        size = sizeof(buf);
        ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
        ok(GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());

        CertFreeCertificateContext(context);
    }
    /* Now test with a cert with an enhanced key usage extension */
    context = CertCreateCertificateContext(X509_ASN_ENCODING, certWithUsage,
     sizeof(certWithUsage));
    ok(context != NULL, "CertCreateCertificateContext failed: %08lx\n",
     GetLastError());
    if (context)
    {
        LPBYTE buf = NULL;
        DWORD bufSize = 0, i;

        /* The size may depend on what flags are used to query it, so I
         * realloc the buffer for each test.
         */
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, NULL, &bufSize);
        ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (buf)
        {
            PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

            /* Should find it in the cert */
            size = bufSize;
            ret = CertGetEnhancedKeyUsage(context,
             CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, pUsage, &size);
            ok(ret && GetLastError() == 0,
             "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 3, "Expected 3 usages, got %ld\n",
             pUsage->cUsageIdentifier);
            for (i = 0; i < pUsage->cUsageIdentifier; i++)
                ok(!strcmp(pUsage->rgpszUsageIdentifier[i], keyUsages[i]),
                 "Expected %s, got %s\n", keyUsages[i],
                 pUsage->rgpszUsageIdentifier[i]);
            HeapFree(GetProcessHeap(), 0, buf);
        }
        ret = CertGetEnhancedKeyUsage(context, 0, NULL, &bufSize);
        ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (buf)
        {
            PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

            /* Should find it as either */
            size = bufSize;
            ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
            /* In Windows, GetLastError returns CRYPT_E_NOT_FOUND not found
             * here, even though the return is successful and the usage id
             * count is positive.  I don't enforce that here.
             */
            ok(ret,
             "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 3, "Expected 3 usages, got %ld\n",
             pUsage->cUsageIdentifier);
            for (i = 0; i < pUsage->cUsageIdentifier; i++)
                ok(!strcmp(pUsage->rgpszUsageIdentifier[i], keyUsages[i]),
                 "Expected %s, got %s\n", keyUsages[i],
                 pUsage->rgpszUsageIdentifier[i]);
            HeapFree(GetProcessHeap(), 0, buf);
        }
        /* Shouldn't find it as an extended property */
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, NULL, &size);
        ok(!ret && GetLastError() == CRYPT_E_NOT_FOUND,
         "Expected CRYPT_E_NOT_FOUND, got %08lx\n", GetLastError());
        /* Adding a usage identifier overrides the cert's usage!? */
        ret = CertAddEnhancedKeyUsageIdentifier(context, szOID_RSA_RSA);
        ok(ret, "CertAddEnhancedKeyUsageIdentifier failed: %08lx\n",
         GetLastError());
        ret = CertGetEnhancedKeyUsage(context, 0, NULL, &bufSize);
        ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (buf)
        {
            PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

            /* Should find it as either */
            size = bufSize;
            ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
            ok(ret,
             "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 1, "Expected 1 usage, got %ld\n",
             pUsage->cUsageIdentifier);
            ok(!strcmp(pUsage->rgpszUsageIdentifier[0], szOID_RSA_RSA),
             "Expected %s, got %s\n", szOID_RSA_RSA,
             pUsage->rgpszUsageIdentifier[0]);
            HeapFree(GetProcessHeap(), 0, buf);
        }
        /* But querying the cert directly returns its usage */
        ret = CertGetEnhancedKeyUsage(context,
         CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, NULL, &bufSize);
        ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (buf)
        {
            PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

            size = bufSize;
            ret = CertGetEnhancedKeyUsage(context,
             CERT_FIND_EXT_ONLY_ENHKEY_USAGE_FLAG, pUsage, &size);
            ok(ret,
             "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 3, "Expected 3 usages, got %ld\n",
             pUsage->cUsageIdentifier);
            for (i = 0; i < pUsage->cUsageIdentifier; i++)
                ok(!strcmp(pUsage->rgpszUsageIdentifier[i], keyUsages[i]),
                 "Expected %s, got %s\n", keyUsages[i],
                 pUsage->rgpszUsageIdentifier[i]);
            HeapFree(GetProcessHeap(), 0, buf);
        }
        /* And removing the only usage identifier in the extended property
         * results in the cert's key usage being found.
         */
        ret = CertRemoveEnhancedKeyUsageIdentifier(context, szOID_RSA_RSA);
        ok(ret, "CertRemoveEnhancedKeyUsage failed: %08lx\n", GetLastError());
        ret = CertGetEnhancedKeyUsage(context, 0, NULL, &bufSize);
        ok(ret, "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
        buf = HeapAlloc(GetProcessHeap(), 0, bufSize);
        if (buf)
        {
            PCERT_ENHKEY_USAGE pUsage = (PCERT_ENHKEY_USAGE)buf;

            /* Should find it as either */
            size = bufSize;
            ret = CertGetEnhancedKeyUsage(context, 0, pUsage, &size);
            ok(ret,
             "CertGetEnhancedKeyUsage failed: %08lx\n", GetLastError());
            ok(pUsage->cUsageIdentifier == 3, "Expected 3 usages, got %ld\n",
             pUsage->cUsageIdentifier);
            for (i = 0; i < pUsage->cUsageIdentifier; i++)
                ok(!strcmp(pUsage->rgpszUsageIdentifier[i], keyUsages[i]),
                 "Expected %s, got %s\n", keyUsages[i],
                 pUsage->rgpszUsageIdentifier[i]);
            HeapFree(GetProcessHeap(), 0, buf);
        }

        CertFreeCertificateContext(context);
    }
}

static void testCompareCertName(void)
{
    static const BYTE bogus[] = { 1, 2, 3, 4 };
    static const BYTE bogusPrime[] = { 0, 1, 2, 3, 4 };
    static const BYTE emptyPrime[] = { 0x30, 0x00, 0x01 };
    BOOL ret;
    CERT_NAME_BLOB blob1, blob2;

    /* crashes
    ret = CertCompareCertificateName(0, NULL, NULL);
     */
    /* An empty name checks against itself.. */
    blob1.pbData = (LPBYTE)emptyCert;
    blob1.cbData = sizeof(emptyCert);
    ret = CertCompareCertificateName(0, &blob1, &blob1);
    ok(ret, "CertCompareCertificateName failed: %08lx\n", GetLastError());
    /* It doesn't have to be a valid encoded name.. */
    blob1.pbData = (LPBYTE)bogus;
    blob1.cbData = sizeof(bogus);
    ret = CertCompareCertificateName(0, &blob1, &blob1);
    ok(ret, "CertCompareCertificateName failed: %08lx\n", GetLastError());
    /* Leading zeroes matter.. */
    blob2.pbData = (LPBYTE)bogusPrime;
    blob2.cbData = sizeof(bogusPrime);
    ret = CertCompareCertificateName(0, &blob1, &blob2);
    ok(!ret, "Expected failure\n");
    /* As do trailing extra bytes. */
    blob2.pbData = (LPBYTE)emptyPrime;
    blob2.cbData = sizeof(emptyPrime);
    ret = CertCompareCertificateName(0, &blob1, &blob2);
    ok(!ret, "Expected failure\n");
}

static const BYTE int1[] = { 0x88, 0xff, 0xff, 0xff };
static const BYTE int2[] = { 0x88, 0xff };
static const BYTE int3[] = { 0x23, 0xff };
static const BYTE int4[] = { 0x7f, 0x00 };
static const BYTE int5[] = { 0x7f };
static const BYTE int6[] = { 0x80, 0x00, 0x00, 0x00 };
static const BYTE int7[] = { 0x80, 0x00 };

struct IntBlobTest
{
    CRYPT_INTEGER_BLOB blob1;
    CRYPT_INTEGER_BLOB blob2;
    BOOL areEqual;
} intBlobs[] = {
 { { sizeof(int1), (LPBYTE)int1 }, { sizeof(int2), (LPBYTE)int2 }, TRUE },
 { { sizeof(int3), (LPBYTE)int3 }, { sizeof(int3), (LPBYTE)int3 }, TRUE },
 { { sizeof(int4), (LPBYTE)int4 }, { sizeof(int5), (LPBYTE)int5 }, TRUE },
 { { sizeof(int6), (LPBYTE)int6 }, { sizeof(int7), (LPBYTE)int7 }, TRUE },
 { { sizeof(int1), (LPBYTE)int1 }, { sizeof(int7), (LPBYTE)int7 }, FALSE },
};

static void testCompareIntegerBlob(void)
{
    DWORD i;
    BOOL ret;

    for (i = 0; i < sizeof(intBlobs) / sizeof(intBlobs[0]); i++)
    {
        ret = CertCompareIntegerBlob(&intBlobs[i].blob1, &intBlobs[i].blob2);
        ok(ret == intBlobs[i].areEqual,
         "%ld: expected blobs %s compare\n", i, intBlobs[i].areEqual ?
         "to" : "not to");
    }
}

static void testComparePublicKeyInfo(void)
{
    BOOL ret;
    CERT_PUBLIC_KEY_INFO info1 = { { 0 } }, info2 = { { 0 } };
    static CHAR oid_rsa_rsa[]     = szOID_RSA_RSA;
    static CHAR oid_rsa_sha1rsa[] = szOID_RSA_SHA1RSA;
    static CHAR oid_x957_dsa[]    = szOID_X957_DSA;
    static const BYTE bits1[] = { 1, 0 };
    static const BYTE bits2[] = { 0 };
    static const BYTE bits3[] = { 1 };

    /* crashes
    ret = CertComparePublicKeyInfo(0, NULL, NULL);
     */
    /* Empty public keys compare */
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    ok(ret, "CertComparePublicKeyInfo failed: %08lx\n", GetLastError());
    /* Different OIDs appear to compare */
    info1.Algorithm.pszObjId = oid_rsa_rsa;
    info2.Algorithm.pszObjId = oid_rsa_sha1rsa;
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    ok(ret, "CertComparePublicKeyInfo failed: %08lx\n", GetLastError());
    info2.Algorithm.pszObjId = oid_x957_dsa;
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    ok(ret, "CertComparePublicKeyInfo failed: %08lx\n", GetLastError());
    info1.PublicKey.cbData = sizeof(bits1);
    info1.PublicKey.pbData = (LPBYTE)bits1;
    info1.PublicKey.cUnusedBits = 0;
    info2.PublicKey.cbData = sizeof(bits1);
    info2.PublicKey.pbData = (LPBYTE)bits1;
    info2.PublicKey.cUnusedBits = 0;
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    ok(ret, "CertComparePublicKeyInfo failed: %08lx\n", GetLastError());
    /* Even though they compare in their used bits, these do not compare */
    info1.PublicKey.cbData = sizeof(bits2);
    info1.PublicKey.pbData = (LPBYTE)bits2;
    info1.PublicKey.cUnusedBits = 0;
    info2.PublicKey.cbData = sizeof(bits3);
    info2.PublicKey.pbData = (LPBYTE)bits3;
    info2.PublicKey.cUnusedBits = 1;
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    /* Simple (non-comparing) case */
    ok(!ret, "Expected keys not to compare\n");
    info2.PublicKey.cbData = sizeof(bits1);
    info2.PublicKey.pbData = (LPBYTE)bits1;
    info2.PublicKey.cUnusedBits = 0;
    ret = CertComparePublicKeyInfo(0, &info1, &info2);
    ok(!ret, "Expected keys not to compare\n");
}

static void testHashPublicKeyInfo(void)
{
    BOOL ret;
    CERT_PUBLIC_KEY_INFO info = { { 0 } };
    DWORD len;

    /* Crash
    ret = CryptHashPublicKeyInfo(0, 0, 0, 0, NULL, NULL, NULL);
    ret = CryptHashPublicKeyInfo(0, 0, 0, 0, &info, NULL, NULL);
     */
    ret = CryptHashPublicKeyInfo(0, 0, 0, 0, NULL, NULL, &len);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    ret = CryptHashPublicKeyInfo(0, 0, 0, X509_ASN_ENCODING, NULL, NULL, &len);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    ret = CryptHashPublicKeyInfo(0, 0, 0, X509_ASN_ENCODING, &info, NULL, &len);
    ok(ret, "CryptHashPublicKeyInfo failed: %08lx\n", GetLastError());
    ok(len == 16, "Expected hash size 16, got %ld\n", len);
    if (len == 16)
    {
        static const BYTE emptyHash[] = { 0xb8,0x51,0x3a,0x31,0x0e,0x9f,0x40,
         0x36,0x9c,0x92,0x45,0x1b,0x9d,0xc8,0xf9,0xf6 };
        BYTE buf[16];

        ret = CryptHashPublicKeyInfo(0, 0, 0, X509_ASN_ENCODING, &info, buf,
         &len);
        ok(ret, "CryptHashPublicKeyInfo failed: %08lx\n", GetLastError());
        ok(!memcmp(buf, emptyHash, len), "Unexpected hash\n");
    }
}

void testCompareCert(void)
{
    CERT_INFO info1 = { 0 }, info2 = { 0 };
    BOOL ret;

    /* Crashes
    ret = CertCompareCertificate(X509_ASN_ENCODING, NULL, NULL);
     */

    /* Certs with the same issuer and serial number are equal, even if they
     * differ in other respects (like subject).
     */
    info1.SerialNumber.pbData = (LPBYTE)serialNum;
    info1.SerialNumber.cbData = sizeof(serialNum);
    info1.Issuer.pbData = (LPBYTE)subjectName;
    info1.Issuer.cbData = sizeof(subjectName);
    info1.Subject.pbData = (LPBYTE)subjectName2;
    info1.Subject.cbData = sizeof(subjectName2);
    info2.SerialNumber.pbData = (LPBYTE)serialNum;
    info2.SerialNumber.cbData = sizeof(serialNum);
    info2.Issuer.pbData = (LPBYTE)subjectName;
    info2.Issuer.cbData = sizeof(subjectName);
    info2.Subject.pbData = (LPBYTE)subjectName;
    info2.Subject.cbData = sizeof(subjectName);
    ret = CertCompareCertificate(X509_ASN_ENCODING, &info1, &info2);
    ok(ret, "Expected certs to be equal\n");

    info2.Issuer.pbData = (LPBYTE)subjectName2;
    info2.Issuer.cbData = sizeof(subjectName2);
    ret = CertCompareCertificate(X509_ASN_ENCODING, &info1, &info2);
    ok(!ret, "Expected certs not to be equal\n");
}

static void testVerifySubjectCert(void)
{
    BOOL ret;
    DWORD flags;
    PCCERT_CONTEXT context1, context2;

    /* Crashes
    ret = CertVerifySubjectCertificateContext(NULL, NULL, NULL);
     */
    flags = 0;
    ret = CertVerifySubjectCertificateContext(NULL, NULL, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    flags = CERT_STORE_NO_CRL_FLAG;
    ret = CertVerifySubjectCertificateContext(NULL, NULL, &flags);
    ok(!ret && GetLastError() == E_INVALIDARG,
     "Expected E_INVALIDARG, got %08lx\n", GetLastError());

    flags = 0;
    context1 = CertCreateCertificateContext(X509_ASN_ENCODING, bigCert,
     sizeof(bigCert));
    ret = CertVerifySubjectCertificateContext(NULL, context1, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    ret = CertVerifySubjectCertificateContext(context1, NULL, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    ret = CertVerifySubjectCertificateContext(context1, context1, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());

    context2 = CertCreateCertificateContext(X509_ASN_ENCODING,
     bigCertWithDifferentSubject, sizeof(bigCertWithDifferentSubject));
    SetLastError(0xdeadbeef);
    ret = CertVerifySubjectCertificateContext(context1, context2, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    flags = CERT_STORE_REVOCATION_FLAG;
    ret = CertVerifySubjectCertificateContext(context1, context2, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    ok(flags == (CERT_STORE_REVOCATION_FLAG | CERT_STORE_NO_CRL_FLAG),
     "Expected CERT_STORE_REVOCATION_FLAG | CERT_STORE_NO_CRL_FLAG, got %08lx\n",
     flags);
    flags = CERT_STORE_SIGNATURE_FLAG;
    ret = CertVerifySubjectCertificateContext(context1, context2, &flags);
    ok(ret, "CertVerifySubjectCertificateContext failed; %08lx\n",
     GetLastError());
    ok(flags == CERT_STORE_SIGNATURE_FLAG,
     "Expected CERT_STORE_SIGNATURE_FLAG, got %08lx\n", flags);
    CertFreeCertificateContext(context2);

    CertFreeCertificateContext(context1);
}

static const BYTE privKey[] = {
 0x07, 0x02, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x52, 0x53, 0x41, 0x32, 0x00,
 0x02, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x79, 0x10, 0x1c, 0xd0, 0x6b, 0x10,
 0x18, 0x30, 0x94, 0x61, 0xdc, 0x0e, 0xcb, 0x96, 0x4e, 0x21, 0x3f, 0x79, 0xcd,
 0xa9, 0x17, 0x62, 0xbc, 0xbb, 0x61, 0x4c, 0xe0, 0x75, 0x38, 0x6c, 0xf3, 0xde,
 0x60, 0x86, 0x03, 0x97, 0x65, 0xeb, 0x1e, 0x6b, 0xdb, 0x53, 0x85, 0xad, 0x68,
 0x21, 0xf1, 0x5d, 0xe7, 0x1f, 0xe6, 0x53, 0xb4, 0xbb, 0x59, 0x3e, 0x14, 0x27,
 0xb1, 0x83, 0xa7, 0x3a, 0x54, 0xe2, 0x8f, 0x65, 0x8e, 0x6a, 0x4a, 0xcf, 0x3b,
 0x1f, 0x65, 0xff, 0xfe, 0xf1, 0x31, 0x3a, 0x37, 0x7a, 0x8b, 0xcb, 0xc6, 0xd4,
 0x98, 0x50, 0x36, 0x67, 0xe4, 0xa1, 0xe8, 0x7e, 0x8a, 0xc5, 0x23, 0xf2, 0x77,
 0xf5, 0x37, 0x61, 0x49, 0x72, 0x59, 0xe8, 0x3d, 0xf7, 0x60, 0xb2, 0x77, 0xca,
 0x78, 0x54, 0x6d, 0x65, 0x9e, 0x03, 0x97, 0x1b, 0x61, 0xbd, 0x0c, 0xd8, 0x06,
 0x63, 0xe2, 0xc5, 0x48, 0xef, 0xb3, 0xe2, 0x6e, 0x98, 0x7d, 0xbd, 0x4e, 0x72,
 0x91, 0xdb, 0x31, 0x57, 0xe3, 0x65, 0x3a, 0x49, 0xca, 0xec, 0xd2, 0x02, 0x4e,
 0x22, 0x7e, 0x72, 0x8e, 0xf9, 0x79, 0x84, 0x82, 0xdf, 0x7b, 0x92, 0x2d, 0xaf,
 0xc9, 0xe4, 0x33, 0xef, 0x89, 0x5c, 0x66, 0x99, 0xd8, 0x80, 0x81, 0x47, 0x2b,
 0xb1, 0x66, 0x02, 0x84, 0x59, 0x7b, 0xc3, 0xbe, 0x98, 0x45, 0x4a, 0x3d, 0xdd,
 0xea, 0x2b, 0xdf, 0x4e, 0xb4, 0x24, 0x6b, 0xec, 0xe7, 0xd9, 0x0c, 0x45, 0xb8,
 0xbe, 0xca, 0x69, 0x37, 0x92, 0x4c, 0x38, 0x6b, 0x96, 0x6d, 0xcd, 0x86, 0x67,
 0x5c, 0xea, 0x54, 0x94, 0xa4, 0xca, 0xa4, 0x02, 0xa5, 0x21, 0x4d, 0xae, 0x40,
 0x8f, 0x9d, 0x51, 0x83, 0xf2, 0x3f, 0x33, 0xc1, 0x72, 0xb4, 0x1d, 0x94, 0x6e,
 0x7d, 0xe4, 0x27, 0x3f, 0xea, 0xff, 0xe5, 0x9b, 0xa7, 0x5e, 0x55, 0x8e, 0x0d,
 0x69, 0x1c, 0x7a, 0xff, 0x81, 0x9d, 0x53, 0x52, 0x97, 0x9a, 0x76, 0x79, 0xda,
 0x93, 0x32, 0x16, 0xec, 0x69, 0x51, 0x1a, 0x4e, 0xc3, 0xf1, 0x72, 0x80, 0x78,
 0x5e, 0x66, 0x4a, 0x8d, 0x85, 0x2f, 0x3f, 0xb2, 0xa7 };

static const BYTE selfSignedCert[] = {
 0x30, 0x82, 0x01, 0x1f, 0x30, 0x81, 0xce, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
 0x10, 0xeb, 0x0d, 0x57, 0x2a, 0x9c, 0x09, 0xba, 0xa4, 0x4a, 0xb7, 0x25, 0x49,
 0xd9, 0x3e, 0xb5, 0x73, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1d,
 0x05, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03,
 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30,
 0x1e, 0x17, 0x0d, 0x30, 0x36, 0x30, 0x36, 0x32, 0x39, 0x30, 0x35, 0x30, 0x30,
 0x34, 0x36, 0x5a, 0x17, 0x0d, 0x30, 0x37, 0x30, 0x36, 0x32, 0x39, 0x31, 0x31,
 0x30, 0x30, 0x34, 0x36, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03,
 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e,
 0x67, 0x00, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41,
 0x00, 0xe2, 0x54, 0x3a, 0xa7, 0x83, 0xb1, 0x27, 0x14, 0x3e, 0x59, 0xbb, 0xb4,
 0x53, 0xe6, 0x1f, 0xe7, 0x5d, 0xf1, 0x21, 0x68, 0xad, 0x85, 0x53, 0xdb, 0x6b,
 0x1e, 0xeb, 0x65, 0x97, 0x03, 0x86, 0x60, 0xde, 0xf3, 0x6c, 0x38, 0x75, 0xe0,
 0x4c, 0x61, 0xbb, 0xbc, 0x62, 0x17, 0xa9, 0xcd, 0x79, 0x3f, 0x21, 0x4e, 0x96,
 0xcb, 0x0e, 0xdc, 0x61, 0x94, 0x30, 0x18, 0x10, 0x6b, 0xd0, 0x1c, 0x10, 0x79,
 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02,
 0x1d, 0x05, 0x00, 0x03, 0x41, 0x00, 0x25, 0x90, 0x53, 0x34, 0xd9, 0x56, 0x41,
 0x5e, 0xdb, 0x7e, 0x01, 0x36, 0xec, 0x27, 0x61, 0x5e, 0xb7, 0x4d, 0x90, 0x66,
 0xa2, 0xe1, 0x9d, 0x58, 0x76, 0xd4, 0x9c, 0xba, 0x2c, 0x84, 0xc6, 0x83, 0x7a,
 0x22, 0x0d, 0x03, 0x69, 0x32, 0x1a, 0x6d, 0xcb, 0x0c, 0x15, 0xb3, 0x6b, 0xc7,
 0x0a, 0x8c, 0xb4, 0x5c, 0x34, 0x78, 0xe0, 0x3c, 0x9c, 0xe9, 0xf3, 0x30, 0x9f,
 0xa8, 0x76, 0x57, 0x92, 0x36 };

static void testAcquireCertPrivateKey(void)
{
    BOOL ret;
    PCCERT_CONTEXT cert;
    HCRYPTPROV csp;
    DWORD keySpec;
    BOOL callerFree;
    CRYPT_KEY_PROV_INFO keyProvInfo;
    HCRYPTKEY key;

    keyProvInfo.pwszContainerName = (LPWSTR)cspNameW;
    keyProvInfo.pwszProvName = (LPWSTR)MS_DEF_PROV_W;
    keyProvInfo.dwProvType = PROV_RSA_FULL;
    keyProvInfo.dwFlags = 0;
    keyProvInfo.cProvParam = 0;
    keyProvInfo.rgProvParam = NULL;
    keyProvInfo.dwKeySpec = AT_SIGNATURE;

    CryptAcquireContextW(NULL, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);

    cert = CertCreateCertificateContext(X509_ASN_ENCODING, selfSignedCert,
     sizeof(selfSignedCert));

    /* Crash
    ret = CryptAcquireCertificatePrivateKey(NULL, 0, NULL, NULL, NULL, NULL);
    ret = CryptAcquireCertificatePrivateKey(NULL, 0, NULL, NULL, NULL,
     &callerFree);
    ret = CryptAcquireCertificatePrivateKey(NULL, 0, NULL, NULL, &keySpec,
     NULL);
    ret = CryptAcquireCertificatePrivateKey(NULL, 0, NULL, &csp, NULL, NULL);
    ret = CryptAcquireCertificatePrivateKey(NULL, 0, NULL, &csp, &keySpec,
     &callerFree);
    ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, NULL, NULL, NULL);
     */

    /* Missing private key */
    ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, &csp, NULL, NULL);
    ok(!ret && GetLastError() == CRYPT_E_NO_KEY_PROPERTY,
     "Expected CRYPT_E_NO_KEY_PROPERTY, got %08lx\n", GetLastError());
    ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, &csp, &keySpec,
     &callerFree);
    ok(!ret && GetLastError() == CRYPT_E_NO_KEY_PROPERTY,
     "Expected CRYPT_E_NO_KEY_PROPERTY, got %08lx\n", GetLastError());
    CertSetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, 0,
     &keyProvInfo);
    ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, &csp, &keySpec,
     &callerFree);
    ok(!ret && GetLastError() == CRYPT_E_NO_KEY_PROPERTY,
     "Expected CRYPT_E_NO_KEY_PROPERTY, got %08lx\n", GetLastError());

    CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ret = CryptImportKey(csp, (LPBYTE)privKey, sizeof(privKey), 0, 0, &key);
    ok(ret, "CryptImportKey failed: %08lx\n", GetLastError());
    if (ret)
    {
        HCRYPTPROV certCSP;
        DWORD size;
        CERT_KEY_CONTEXT keyContext;

        /* Don't cache provider */
        ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, &certCSP,
         &keySpec, &callerFree);
        ok(ret, "CryptAcquireCertificatePrivateKey failed: %08lx\n",
         GetLastError());
        ok(callerFree, "Expected callerFree to be TRUE\n");
        CryptReleaseContext(certCSP, 0);
        ret = CryptAcquireCertificatePrivateKey(cert, 0, NULL, &certCSP,
         NULL, NULL);
        ok(ret, "CryptAcquireCertificatePrivateKey failed: %08lx\n",
         GetLastError());
        CryptReleaseContext(certCSP, 0);

        /* Use the key prov info's caching (there shouldn't be any) */
        ret = CryptAcquireCertificatePrivateKey(cert,
         CRYPT_ACQUIRE_USE_PROV_INFO_FLAG, NULL, &certCSP, &keySpec,
         &callerFree);
        ok(ret, "CryptAcquireCertificatePrivateKey failed: %08lx\n",
         GetLastError());
        ok(callerFree, "Expected callerFree to be TRUE\n");
        CryptReleaseContext(certCSP, 0);

        /* Cache it (and check that it's cached) */
        ret = CryptAcquireCertificatePrivateKey(cert,
         CRYPT_ACQUIRE_CACHE_FLAG, NULL, &certCSP, &keySpec, &callerFree);
        ok(ret, "CryptAcquireCertificatePrivateKey failed: %08lx\n",
         GetLastError());
        ok(!callerFree, "Expected callerFree to be FALSE\n");
        size = sizeof(keyContext);
        ret = CertGetCertificateContextProperty(cert, CERT_KEY_CONTEXT_PROP_ID,
         &keyContext, &size);
        ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
         GetLastError());

        /* Remove the cached provider */
        CryptReleaseContext(keyContext.hCryptProv, 0);
        CertSetCertificateContextProperty(cert, CERT_KEY_CONTEXT_PROP_ID, 0,
         NULL);
        /* Allow caching via the key prov info */
        keyProvInfo.dwFlags = CERT_SET_KEY_CONTEXT_PROP_ID;
        CertSetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, 0,
         &keyProvInfo);
        /* Now use the key prov info's caching */
        ret = CryptAcquireCertificatePrivateKey(cert,
         CRYPT_ACQUIRE_USE_PROV_INFO_FLAG, NULL, &certCSP, &keySpec,
         &callerFree);
        ok(ret, "CryptAcquireCertificatePrivateKey failed: %08lx\n",
         GetLastError());
        ok(!callerFree, "Expected callerFree to be FALSE\n");
        size = sizeof(keyContext);
        ret = CertGetCertificateContextProperty(cert, CERT_KEY_CONTEXT_PROP_ID,
         &keyContext, &size);
        ok(ret, "CertGetCertificateContextProperty failed: %08lx\n",
         GetLastError());

        CryptDestroyKey(key);
    }

    CryptReleaseContext(csp, 0);
    CryptAcquireContextW(&csp, cspNameW, MS_DEF_PROV_W, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);

    CertFreeCertificateContext(cert);
}

START_TEST(cert)
{
    init_function_pointers();

    testAddCert();
    testCertProperties();
    testDupCert();
    testFindCert();
    testGetSubjectCert();
    testGetIssuerCert();

    testCryptHashCert();
    testCertSigs();
    testCreateSelfSignCert();
    testKeyUsage();
    testCompareCertName();
    testCompareIntegerBlob();
    testComparePublicKeyInfo();
    testHashPublicKeyInfo();
    testCompareCert();
    testVerifySubjectCert();
    testAcquireCertPrivateKey();
}
