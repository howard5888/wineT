/*
 * Unit tests for rsaenh functions
 *
 * Copyright (c) 2004 Michael Jung
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

#include <string.h>
#include <stdio.h>
#include "wine/test.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wincrypt.h"

static HCRYPTPROV hProv;
static const char szContainer[] = "winetest";
static const unsigned char pbData[] = "Wine rocks totally!";
static const char szProvider[] = MS_ENHANCED_PROV_A;

static BOOL (WINAPI *pCryptDuplicateHash) (HCRYPTHASH, DWORD*, DWORD, HCRYPTHASH*);

/*
static void trace_hex(BYTE *pbData, DWORD dwLen) {
    char szTemp[256];
    DWORD i, j;

    for (i = 0; i < dwLen-7; i+=8) {
        sprintf(szTemp, "0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n", 
            pbData[i], pbData[i+1], pbData[i+2], pbData[i+3], pbData[i+4], pbData[i+5], 
            pbData[i+6], pbData[i+7]);
        trace(szTemp);
    }
    for (j=0; i<dwLen; j++,i++) {
        sprintf(szTemp+6*j, "0x%02x, \n", pbData[i]);
    }
    trace(szTemp);
}
*/

static int init_environment(void)
{
    HCRYPTKEY hKey;
    BOOL result;
        
    pCryptDuplicateHash = (void *)GetProcAddress(GetModuleHandleA("advapi32.dll"), "CryptDuplicateHash");
        
    hProv = (HCRYPTPROV)INVALID_HANDLE_VALUE;

    result = CryptAcquireContext(&hProv, szContainer, szProvider, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    ok(!result && GetLastError()==NTE_BAD_FLAGS, "%d, %08lx\n", result, GetLastError());
    
    if (!CryptAcquireContext(&hProv, szContainer, szProvider, PROV_RSA_FULL, 0))
    {
        ok(GetLastError()==NTE_BAD_KEYSET, "%08lx\n", GetLastError());
        if (GetLastError()!=NTE_BAD_KEYSET) return 0;
        result = CryptAcquireContext(&hProv, szContainer, szProvider, PROV_RSA_FULL, 
                                    CRYPT_NEWKEYSET);
        ok(result, "%08lx\n", GetLastError());
        if (!result) return 0;
        result = CryptGenKey(hProv, AT_KEYEXCHANGE, 0, &hKey);
        ok(result, "%08lx\n", GetLastError());
        if (result) CryptDestroyKey(hKey);
        result = CryptGenKey(hProv, AT_SIGNATURE, 0, &hKey);
        ok(result, "%08lx\n", GetLastError());
        if (result) CryptDestroyKey(hKey);
    }
    return 1;
}

static void clean_up_environment(void)
{
    BOOL result;

    result = CryptReleaseContext(hProv, 1);
    ok(!result && GetLastError()==NTE_BAD_FLAGS, "%08lx\n", GetLastError());
        
    CryptAcquireContext(&hProv, szContainer, szProvider, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
}

static void test_prov(void) 
{
    BOOL result;
    DWORD dwLen, dwInc;
    
    dwLen = (DWORD)sizeof(DWORD);
    result = CryptGetProvParam(hProv, PP_SIG_KEYSIZE_INC, (BYTE*)&dwInc, &dwLen, 0);
    ok(result && dwInc==8, "%08lx, %ld\n", GetLastError(), dwInc);
    
    dwLen = (DWORD)sizeof(DWORD);
    result = CryptGetProvParam(hProv, PP_KEYX_KEYSIZE_INC, (BYTE*)&dwInc, &dwLen, 0);
    ok(result && dwInc==8, "%08lx, %ld\n", GetLastError(), dwInc);
}

static void test_gen_random(void)
{
    BOOL result;
    BYTE rnd1[16], rnd2[16];

    memset(rnd1, 0, sizeof(rnd1));
    memset(rnd2, 0, sizeof(rnd2));

    result = CryptGenRandom(hProv, sizeof(rnd1), rnd1);
    if (!result && GetLastError() == NTE_FAIL) {
        /* rsaenh compiled without OpenSSL */
        return;
    }
    
    ok(result, "%08lx\n", GetLastError());

    result = CryptGenRandom(hProv, sizeof(rnd2), rnd2);
    ok(result, "%08lx\n", GetLastError());

    ok(memcmp(rnd1, rnd2, sizeof(rnd1)), "CryptGenRandom generates non random data\n");
}

static BOOL derive_key(ALG_ID aiAlgid, HCRYPTKEY *phKey, DWORD len) 
{
    HCRYPTHASH hHash;
    BOOL result;
    unsigned char pbData[2000];
    int i;

    *phKey = (HCRYPTKEY)NULL;
    for (i=0; i<2000; i++) pbData[i] = (unsigned char)i;
    result = CryptCreateHash(hProv, CALG_MD2, 0, 0, &hHash);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError()==NTE_BAD_ALGID, "%08lx\n", GetLastError());
        return FALSE;
    } 
    ok(result, "%08lx\n", GetLastError());
    if (!result) return FALSE;
    result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return FALSE;
    result = CryptDeriveKey(hProv, aiAlgid, hHash, (len << 16) | CRYPT_EXPORTABLE, phKey);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return FALSE;
    len = 2000;
    result = CryptGetHashParam(hHash, HP_HASHVAL, pbData, &len, 0);
    ok(result, "%08lx\n", GetLastError());
    CryptDestroyHash(hHash);
    return TRUE;
}

static void test_hashes(void)
{
    static const unsigned char md2hash[16] = {
        0x12, 0xcb, 0x1b, 0x08, 0xc8, 0x48, 0xa4, 0xa9, 
        0xaa, 0xf3, 0xf1, 0x9f, 0xfc, 0x29, 0x28, 0x68 };
    static const unsigned char md4hash[16] = {
        0x8e, 0x2a, 0x58, 0xbf, 0xf2, 0xf5, 0x26, 0x23, 
        0x79, 0xd2, 0x92, 0x36, 0x1b, 0x23, 0xe3, 0x81 };
    static const unsigned char md5hash[16] = { 
        0x15, 0x76, 0xa9, 0x4d, 0x6c, 0xb3, 0x34, 0xdd, 
        0x12, 0x6c, 0xb1, 0xc2, 0x7f, 0x19, 0xe0, 0xf2 };    
    static const unsigned char sha1hash[20] = { 
        0xf1, 0x0c, 0xcf, 0xde, 0x60, 0xc1, 0x7d, 0xb2, 0x6e, 0x7d, 
        0x85, 0xd3, 0x56, 0x65, 0xc7, 0x66, 0x1d, 0xbb, 0xeb, 0x2c };
    unsigned char pbData[2048];
    BOOL result;
    HCRYPTHASH hHash, hHashClone;
    BYTE pbHashValue[36];
    DWORD hashlen, len;
    int i;

    for (i=0; i<2048; i++) pbData[i] = (unsigned char)i;

    /* MD2 Hashing */
    result = CryptCreateHash(hProv, CALG_MD2, 0, 0, &hHash);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError() == NTE_BAD_ALGID, "%08lx\n", GetLastError());
    } else {
        result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
        ok(result, "%08lx\n", GetLastError());

        len = sizeof(DWORD);
        result = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashlen, &len, 0);
           ok(result && (hashlen == 16), "%08lx, hashlen: %ld\n", GetLastError(), hashlen);

        len = 16;
        result = CryptGetHashParam(hHash, HP_HASHVAL, pbHashValue, &len, 0);
        ok(result, "%08lx\n", GetLastError());

        ok(!memcmp(pbHashValue, md2hash, 16), "Wrong MD2 hash!\n");

        result = CryptDestroyHash(hHash);
        ok(result, "%08lx\n", GetLastError());
    } 

    /* MD4 Hashing */
    result = CryptCreateHash(hProv, CALG_MD4, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());

    result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
    ok(result, "%08lx\n", GetLastError());

    len = sizeof(DWORD);
    result = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashlen, &len, 0);
    ok(result && (hashlen == 16), "%08lx, hashlen: %ld\n", GetLastError(), hashlen);

    len = 16;
    result = CryptGetHashParam(hHash, HP_HASHVAL, pbHashValue, &len, 0);
    ok(result, "%08lx\n", GetLastError());

    ok(!memcmp(pbHashValue, md4hash, 16), "Wrong MD4 hash!\n");

    result = CryptDestroyHash(hHash);
    ok(result, "%08lx\n", GetLastError());

    /* MD5 Hashing */
    result = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());

    result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
    ok(result, "%08lx\n", GetLastError());

    len = sizeof(DWORD);
    result = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashlen, &len, 0);
    ok(result && (hashlen == 16), "%08lx, hashlen: %ld\n", GetLastError(), hashlen);

    len = 16;
    result = CryptGetHashParam(hHash, HP_HASHVAL, pbHashValue, &len, 0);
    ok(result, "%08lx\n", GetLastError());

    ok(!memcmp(pbHashValue, md5hash, 16), "Wrong MD5 hash!\n");

    result = CryptDestroyHash(hHash);
    ok(result, "%08lx\n", GetLastError());

    /* SHA1 Hashing */
    result = CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());

    result = CryptHashData(hHash, (BYTE*)pbData, 5, 0);
    ok(result, "%08lx\n", GetLastError());

    if(pCryptDuplicateHash) {
        result = pCryptDuplicateHash(hHash, 0, 0, &hHashClone);
        ok(result, "%08lx\n", GetLastError());

        result = CryptHashData(hHashClone, (BYTE*)pbData+5, sizeof(pbData)-5, 0);
        ok(result, "%08lx\n", GetLastError());

        len = sizeof(DWORD);
        result = CryptGetHashParam(hHashClone, HP_HASHSIZE, (BYTE*)&hashlen, &len, 0);
        ok(result && (hashlen == 20), "%08lx, hashlen: %ld\n", GetLastError(), hashlen);

        len = 20;
        result = CryptGetHashParam(hHashClone, HP_HASHVAL, pbHashValue, &len, 0);
        ok(result, "%08lx\n", GetLastError());

        ok(!memcmp(pbHashValue, sha1hash, 20), "Wrong SHA1 hash!\n");

        result = CryptDestroyHash(hHashClone);
        ok(result, "%08lx\n", GetLastError());
    }

    result = CryptDestroyHash(hHash);
    ok(result, "%08lx\n", GetLastError());
}

static void test_block_cipher_modes(void)
{
    static const BYTE plain[23] = { 
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
    static const BYTE ecb[24] = {   
        0xc0, 0x9a, 0xe4, 0x2f, 0x0a, 0x47, 0x67, 0x11, 0xf2, 0xb2, 0x5d, 0x5f, 
        0x08, 0xff, 0x49, 0xa4, 0x45, 0x3a, 0x68, 0x14, 0xca, 0x18, 0xe5, 0xf4 };
    static const BYTE cbc[24] = {   
        0xc0, 0x9a, 0xe4, 0x2f, 0x0a, 0x47, 0x67, 0x11, 0x10, 0xf5, 0xda, 0x61,
        0x4e, 0x3d, 0xab, 0xc0, 0x97, 0x85, 0x01, 0x12, 0x97, 0xa4, 0xf7, 0xd3 };
    static const BYTE cfb[24] = {   
        0x29, 0xb5, 0x67, 0x85, 0x0b, 0x1b, 0xec, 0x07, 0x67, 0x2d, 0xa1, 0xa4,
        0x1a, 0x47, 0x24, 0x6a, 0x54, 0xe1, 0xe0, 0x92, 0xf9, 0x0e, 0xf6, 0xeb };
    HCRYPTKEY hKey;
    BOOL result;
    BYTE abData[24];
    DWORD dwMode, dwLen;

    result = derive_key(CALG_RC2, &hKey, 40);
    if (!result) return;

    memcpy(abData, plain, sizeof(abData));

    dwMode = CRYPT_MODE_ECB;
    result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
    ok(result, "%08lx\n", GetLastError());

    dwLen = 23;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, NULL, &dwLen, 24);
    ok(result, "CryptEncrypt failed: %08lx\n", GetLastError());
    ok(dwLen == 24, "Unexpected length %ld\n", dwLen);

    SetLastError(ERROR_SUCCESS);
    dwLen = 23;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData, &dwLen, 24);
    ok(result && dwLen == 24 && !memcmp(ecb, abData, sizeof(ecb)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData, &dwLen);
    ok(result && dwLen == 23 && !memcmp(plain, abData, sizeof(plain)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    dwMode = CRYPT_MODE_CBC;
    result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
    ok(result, "%08lx\n", GetLastError());
    
    dwLen = 23;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, NULL, &dwLen, 24);
    ok(result, "CryptEncrypt failed: %08lx\n", GetLastError());
    ok(dwLen == 24, "Unexpected length %ld\n", dwLen);

    dwLen = 23;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData, &dwLen, 24);
    ok(result && dwLen == 24 && !memcmp(cbc, abData, sizeof(cbc)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData, &dwLen);
    ok(result && dwLen == 23 && !memcmp(plain, abData, sizeof(plain)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    dwMode = CRYPT_MODE_CFB;
    result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
    ok(result, "%08lx\n", GetLastError());
    
    dwLen = 16;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, FALSE, 0, abData, &dwLen, 24);
    ok(result && dwLen == 16, "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    dwLen = 7;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData+16, &dwLen, 8);
    ok(result && dwLen == 8 && !memcmp(cfb, abData, sizeof(cfb)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);
    
    dwLen = 8;
    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, FALSE, 0, abData, &dwLen);
    ok(result && dwLen == 8, "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    dwLen = 16;
    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData+8, &dwLen);
    ok(result && dwLen == 15 && !memcmp(plain, abData, sizeof(plain)), 
       "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    dwMode = CRYPT_MODE_OFB;
    result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
    ok(result, "%08lx\n", GetLastError());
    
    dwLen = 23;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abData, &dwLen, 24);
    ok(!result && GetLastError() == NTE_BAD_ALGID, "%08lx\n", GetLastError());
}

static void test_3des112(void)
{
    HCRYPTKEY hKey;
    BOOL result;
    DWORD dwLen;
    unsigned char pbData[16];
    int i;

    result = derive_key(CALG_3DES_112, &hKey, 0);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError() == NTE_BAD_ALGID, "%08lx\n", GetLastError());
        return;
    }

    for (i=0; i<sizeof(pbData); i++) pbData[i] = (unsigned char)i;
    
    dwLen = 13;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen, 16);
    ok(result, "%08lx\n", GetLastError());
    
    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen);
    ok(result, "%08lx\n", GetLastError());

    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());
}

static void test_des(void) 
{
    HCRYPTKEY hKey;
    BOOL result;
    DWORD dwLen, dwMode;
    unsigned char pbData[16];
    int i;

    result = derive_key(CALG_DES, &hKey, 56);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError()==NTE_BAD_ALGID, "%08lx\n", GetLastError());
        return;
    }

    dwMode = CRYPT_MODE_ECB;
    result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
    ok(result, "%08lx\n", GetLastError());
    
    dwLen = sizeof(DWORD);
    result = CryptGetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, &dwLen, 0);
    ok(result, "%08lx\n", GetLastError());
    
    for (i=0; i<sizeof(pbData); i++) pbData[i] = (unsigned char)i;
    
    dwLen = 13;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen, 16);
    ok(result, "%08lx\n", GetLastError());
    
    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen);
    ok(result, "%08lx\n", GetLastError());

    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());
}

static void test_3des(void)
{
    HCRYPTKEY hKey;
    BOOL result;
    DWORD dwLen;
    unsigned char pbData[16];
    static const BYTE des3[16] = { 
        0x7b, 0xba, 0xdd, 0xa2, 0x39, 0xd3, 0x7b, 0xb3, 
        0xc7, 0x51, 0x81, 0x41, 0x53, 0xe8, 0xcf, 0xeb };
    int i;

    result = derive_key(CALG_3DES, &hKey, 0);
    if (!result) return;

    for (i=0; i<sizeof(pbData); i++) pbData[i] = (unsigned char)i;
    
    dwLen = 13;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen, 16);
    ok(result, "%08lx\n", GetLastError());
    
    ok(!memcmp(pbData, des3, sizeof(des3)), "3DES encryption failed!\n");
    
    result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwLen);
    ok(result, "%08lx\n", GetLastError());

    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());
}

static void test_rc2(void)
{
    static const BYTE rc2encrypted[16] = { 
        0x02, 0x34, 0x7d, 0xf6, 0x1d, 0xc5, 0x9b, 0x8b, 
        0x2e, 0x0d, 0x63, 0x80, 0x72, 0xc1, 0xc2, 0xb1 };
    HCRYPTHASH hHash;
    HCRYPTKEY hKey;
    BOOL result;
    DWORD dwLen, dwKeyLen, dwDataLen, dwMode, dwModeBits;
    BYTE *pbTemp;
    unsigned char pbData[2000], pbHashValue[16];
    int i;
    
    for (i=0; i<2000; i++) pbData[i] = (unsigned char)i;

    /* MD2 Hashing */
    result = CryptCreateHash(hProv, CALG_MD2, 0, 0, &hHash);
    if (!result) {
        ok(GetLastError()==NTE_BAD_ALGID, "%08lx\n", GetLastError());
    } else {
        result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
        ok(result, "%08lx\n", GetLastError());

        dwLen = 16;
        result = CryptGetHashParam(hHash, HP_HASHVAL, pbHashValue, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        result = CryptDeriveKey(hProv, CALG_RC2, hHash, 56 << 16, &hKey);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_KEYLEN, (BYTE*)&dwKeyLen, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        dwMode = CRYPT_MODE_CBC;
        result = CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_MODE_BITS, (BYTE*)&dwModeBits, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_PERMISSIONS, (BYTE*)&dwModeBits, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_BLOCKLEN, (BYTE*)&dwModeBits, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        result = CryptGetKeyParam(hKey, KP_IV, NULL, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());
        pbTemp = HeapAlloc(GetProcessHeap(), 0, dwLen);
        CryptGetKeyParam(hKey, KP_IV, pbTemp, &dwLen, 0);
        HeapFree(GetProcessHeap(), 0, pbTemp);

        result = CryptGetKeyParam(hKey, KP_SALT, NULL, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());
        pbTemp = HeapAlloc(GetProcessHeap(), 0, dwLen);
        CryptGetKeyParam(hKey, KP_SALT, pbTemp, &dwLen, 0);
        HeapFree(GetProcessHeap(), 0, pbTemp);

        dwLen = sizeof(DWORD);
        CryptGetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, &dwLen, 0);

        result = CryptDestroyHash(hHash);
        ok(result, "%08lx\n", GetLastError());

        dwDataLen = 13;
        result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwDataLen, 24);
        ok(result, "%08lx\n", GetLastError());

        ok(!memcmp(pbData, rc2encrypted, 8), "RC2 encryption failed!\n");

        result = CryptGetKeyParam(hKey, KP_IV, NULL, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());
        pbTemp = HeapAlloc(GetProcessHeap(), 0, dwLen);
        CryptGetKeyParam(hKey, KP_IV, pbTemp, &dwLen, 0);
        HeapFree(GetProcessHeap(), 0, pbTemp);

        result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwDataLen);
        ok(result, "%08lx\n", GetLastError());

        result = CryptDestroyKey(hKey);
        ok(result, "%08lx\n", GetLastError());
    }
}

static void test_rc4(void)
{
    static const BYTE rc4[16] = { 
        0x17, 0x0c, 0x44, 0x8e, 0xae, 0x90, 0xcd, 0xb0, 
        0x7f, 0x87, 0xf5, 0x7a, 0xec, 0xb2, 0x2e, 0x35 };    
    BOOL result;
    HCRYPTHASH hHash;
    HCRYPTKEY hKey;
    DWORD dwDataLen = 5, dwKeyLen, dwLen = sizeof(DWORD), dwMode;
    unsigned char pbData[2000], *pbTemp;
    unsigned char pszBuffer[256];
    int i;

    for (i=0; i<2000; i++) pbData[i] = (unsigned char)i;

    /* MD2 Hashing */
    result = CryptCreateHash(hProv, CALG_MD2, 0, 0, &hHash);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError() == NTE_BAD_ALGID, "%08lx\n", GetLastError());
    } else {
        result = CryptHashData(hHash, (BYTE*)pbData, sizeof(pbData), 0);
           ok(result, "%08lx\n", GetLastError());

        dwLen = 16;
        result = CryptGetHashParam(hHash, HP_HASHVAL, pszBuffer, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        result = CryptDeriveKey(hProv, CALG_RC4, hHash, 56 << 16, &hKey);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_KEYLEN, (BYTE*)&dwKeyLen, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        dwLen = sizeof(DWORD);
        result = CryptGetKeyParam(hKey, KP_BLOCKLEN, (BYTE*)&dwKeyLen, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());

        result = CryptGetKeyParam(hKey, KP_IV, NULL, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());
        pbTemp = HeapAlloc(GetProcessHeap(), 0, dwLen);
        CryptGetKeyParam(hKey, KP_IV, pbTemp, &dwLen, 0);
        HeapFree(GetProcessHeap(), 0, pbTemp);

        result = CryptGetKeyParam(hKey, KP_SALT, NULL, &dwLen, 0);
        ok(result, "%08lx\n", GetLastError());
        pbTemp = HeapAlloc(GetProcessHeap(), 0, dwLen);
        CryptGetKeyParam(hKey, KP_SALT, pbTemp, &dwLen, 0);
        HeapFree(GetProcessHeap(), 0, pbTemp);

        dwLen = sizeof(DWORD);
        CryptGetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, &dwLen, 0);

        result = CryptDestroyHash(hHash);
        ok(result, "%08lx\n", GetLastError());

        dwDataLen = 16;
        result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, NULL, &dwDataLen, 24);
        ok(result, "%08lx\n", GetLastError());
        dwDataLen = 16;
        result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwDataLen, 24);
        ok(result, "%08lx\n", GetLastError());

        ok(!memcmp(pbData, rc4, dwDataLen), "RC4 encryption failed!\n");

        result = CryptDecrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, pbData, &dwDataLen);
        ok(result, "%08lx\n", GetLastError());

        result = CryptDestroyKey(hKey);
        ok(result, "%08lx\n", GetLastError());
    }
}

static void test_hmac(void) {
    HCRYPTKEY hKey;
    HCRYPTHASH hHash;
    BOOL result;
    HMAC_INFO hmacInfo = { CALG_MD2, NULL, 0, NULL, 0 };
    DWORD dwLen;
    BYTE abData[256];
    static const BYTE hmac[16] = { 
        0xfd, 0x16, 0xb5, 0xb6, 0x13, 0x1c, 0x2b, 0xd6, 
        0x0a, 0xc7, 0xae, 0x92, 0x76, 0xa3, 0x05, 0x71 };
    int i;

    for (i=0; i<sizeof(abData)/sizeof(BYTE); i++) abData[i] = (BYTE)i;

    if (!derive_key(CALG_RC2, &hKey, 56)) return;

    result = CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptSetHashParam(hHash, HP_HMAC_INFO, (BYTE*)&hmacInfo, 0);
    ok(result, "%08lx\n", GetLastError());

    result = CryptHashData(hHash, (BYTE*)abData, sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());

    dwLen = sizeof(abData)/sizeof(BYTE);
    result = CryptGetHashParam(hHash, HP_HASHVAL, abData, &dwLen, 0);
    ok(result, "%08lx\n", GetLastError());

    ok(!memcmp(abData, hmac, sizeof(hmac)), "HMAC failed!\n");
    
    result = CryptDestroyHash(hHash);
    ok(result, "%08lx\n", GetLastError());
    
    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());

    /* Provoke errors */
    result = CryptCreateHash(hProv, CALG_HMAC, 0, 0, &hHash);
    ok(!result && GetLastError() == NTE_BAD_KEY, "%08lx\n", GetLastError());
}

static void test_mac(void) {
    HCRYPTKEY hKey;
    HCRYPTHASH hHash;
    BOOL result;
    DWORD dwLen;
    BYTE abData[256], abEnc[264];
    static const BYTE mac[8] = { 0x0d, 0x3e, 0x15, 0x6b, 0x85, 0x63, 0x5c, 0x11 };
    int i;

    for (i=0; i<sizeof(abData)/sizeof(BYTE); i++) abData[i] = (BYTE)i;
    for (i=0; i<sizeof(abData)/sizeof(BYTE); i++) abEnc[i] = (BYTE)i;

    if (!derive_key(CALG_RC2, &hKey, 56)) return;

    dwLen = 256;
    result = CryptEncrypt(hKey, (HCRYPTHASH)NULL, TRUE, 0, abEnc, &dwLen, 264);
    ok (result && dwLen == 264, "%08lx, dwLen: %ld\n", GetLastError(), dwLen);
    
    result = CryptCreateHash(hProv, CALG_MAC, hKey, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHash, (BYTE*)abData, sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());

    dwLen = sizeof(abData)/sizeof(BYTE);
    result = CryptGetHashParam(hHash, HP_HASHVAL, abData, &dwLen, 0);
    ok(result && dwLen == 8, "%08lx, dwLen: %ld\n", GetLastError(), dwLen);

    ok(!memcmp(abData, mac, sizeof(mac)), "MAC failed!\n");
    
    result = CryptDestroyHash(hHash);
    ok(result, "%08lx\n", GetLastError());
    
    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());
    
    /* Provoke errors */
    if (!derive_key(CALG_RC4, &hKey, 56)) return;

    result = CryptCreateHash(hProv, CALG_MAC, hKey, 0, &hHash);
    ok(!result && GetLastError() == NTE_BAD_KEY, "%08lx\n", GetLastError());

    result = CryptDestroyKey(hKey);
    ok(result, "%08lx\n", GetLastError());
}

static BYTE abPlainPrivateKey[596] = {
    0x07, 0x02, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00,
    0x52, 0x53, 0x41, 0x32, 0x00, 0x04, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x9b, 0x64, 0xef, 0xce,
    0x31, 0x7c, 0xad, 0x56, 0xe2, 0x1e, 0x9b, 0x96,
    0xb3, 0xf0, 0x29, 0x88, 0x6e, 0xa8, 0xc2, 0x11,
    0x33, 0xd6, 0xcc, 0x8c, 0x69, 0xb2, 0x1a, 0xfd,
    0xfc, 0x23, 0x21, 0x30, 0x4d, 0x29, 0x45, 0xb6,
    0x3a, 0x67, 0x11, 0x80, 0x1a, 0x91, 0xf2, 0x9f,
    0x01, 0xac, 0xc0, 0x11, 0x50, 0x5f, 0xcd, 0xb9,
    0xad, 0x76, 0x9f, 0x6e, 0x91, 0x55, 0x71, 0xda,
    0x97, 0x96, 0x96, 0x22, 0x75, 0xb4, 0x83, 0x44,
    0x89, 0x9e, 0xf8, 0x44, 0x40, 0x7c, 0xd6, 0xcd,
    0x9d, 0x88, 0xd6, 0x88, 0xbc, 0x56, 0xb7, 0x64,
    0xe9, 0x2c, 0x24, 0x2f, 0x0d, 0x78, 0x55, 0x1c,
    0xb2, 0x67, 0xb1, 0x5e, 0xbc, 0x0c, 0xcf, 0x1c,
    0xe9, 0xd3, 0x9e, 0xa2, 0x15, 0x24, 0x73, 0xd6,
    0xdb, 0x6f, 0x83, 0xb2, 0xf8, 0xbc, 0xe7, 0x47,
    0x3b, 0x01, 0xef, 0x49, 0x08, 0x98, 0xd6, 0xa3,
    0xf9, 0x25, 0x57, 0xe9, 0x39, 0x3c, 0x53, 0x30,
    0x1b, 0xf2, 0xc9, 0x62, 0x31, 0x43, 0x5d, 0x84,
    0x24, 0x30, 0x21, 0x9a, 0xad, 0xdb, 0x62, 0x91,
    0xc8, 0x07, 0xd9, 0x2f, 0xd6, 0xb5, 0x37, 0x6f,
    0xfe, 0x7a, 0x12, 0xbc, 0xd9, 0xd2, 0x2b, 0xbf,
    0xd7, 0xb1, 0xfa, 0x7d, 0xc0, 0x48, 0xdd, 0x74,
    0xdd, 0x55, 0x04, 0xa1, 0x8b, 0xc1, 0x0a, 0xc4,
    0xa5, 0x57, 0x62, 0xee, 0x08, 0x8b, 0xf9, 0x19,
    0x6c, 0x52, 0x06, 0xf8, 0x73, 0x0f, 0x24, 0xc9,
    0x71, 0x9f, 0xc5, 0x45, 0x17, 0x3e, 0xae, 0x06,
    0x81, 0xa2, 0x96, 0x40, 0x06, 0xbf, 0xeb, 0x9e,
    0x80, 0x2b, 0x27, 0x20, 0x8f, 0x38, 0xcf, 0xeb,
    0xff, 0x3b, 0x38, 0x41, 0x35, 0x69, 0x66, 0x13,
    0x1d, 0x3c, 0x01, 0x3b, 0xf6, 0x37, 0xca, 0x9c,
    0x61, 0x74, 0x98, 0xcf, 0xc9, 0x6e, 0xe8, 0x90,
    0xc7, 0xb7, 0x33, 0xc0, 0x07, 0x3c, 0xf8, 0xc8,
    0xf6, 0xf2, 0xd7, 0xf0, 0x21, 0x62, 0x58, 0x8a,
    0x55, 0xbf, 0xa1, 0x2d, 0x3d, 0xa6, 0x69, 0xc5,
    0x02, 0x19, 0x31, 0xf0, 0x94, 0x0f, 0x45, 0x5c,
    0x95, 0x1b, 0x53, 0xbc, 0xf5, 0xb0, 0x1a, 0x8f,
    0xbf, 0x40, 0xe0, 0xc7, 0x73, 0xe7, 0x72, 0x6e,
    0xeb, 0xb1, 0x0f, 0x38, 0xc5, 0xf8, 0xee, 0x04,
    0xed, 0x34, 0x1a, 0x10, 0xf9, 0x53, 0x34, 0xf3,
    0x3e, 0xe6, 0x5c, 0xd1, 0x47, 0x65, 0xcd, 0xbd,
    0xf1, 0x06, 0xcb, 0xb4, 0xb1, 0x26, 0x39, 0x9f,
    0x71, 0xfe, 0x3d, 0xf8, 0x62, 0xab, 0x22, 0x8b,
    0x0e, 0xdc, 0xb9, 0xe8, 0x74, 0x06, 0xfc, 0x8c,
    0x25, 0xa1, 0xa9, 0xcf, 0x07, 0xf9, 0xac, 0x21,
    0x01, 0x7b, 0x1c, 0xdc, 0x94, 0xbd, 0x47, 0xe1,
    0xa0, 0x86, 0x59, 0x35, 0x6a, 0x6f, 0xb9, 0x70,
    0x26, 0x7c, 0x3c, 0xfd, 0xbd, 0x81, 0x39, 0x36,
    0x42, 0xc2, 0xbd, 0xbe, 0x84, 0x27, 0x9a, 0x69,
    0x81, 0xda, 0x99, 0x27, 0xc2, 0x4f, 0x62, 0x33,
    0xf4, 0x79, 0x30, 0xc5, 0x63, 0x54, 0x71, 0xf1,
    0x47, 0x22, 0x25, 0x9b, 0x6c, 0x00, 0x2f, 0x1c,
    0xf4, 0x1f, 0x85, 0xbc, 0xf6, 0x67, 0x6a, 0xe3,
    0xf6, 0x55, 0x8a, 0xef, 0xd0, 0x0b, 0xd3, 0xa2,
    0xc5, 0x51, 0x70, 0x15, 0x0a, 0xf0, 0x98, 0x4c,
    0xb7, 0x19, 0x62, 0x0e, 0x2d, 0x2a, 0x4a, 0x7d,
    0x7a, 0x0a, 0xc4, 0x17, 0xe3, 0x5d, 0x20, 0x52,
    0xa9, 0x98, 0xc3, 0xaa, 0x11, 0xf6, 0xbf, 0x4c,
    0x94, 0x99, 0x81, 0x89, 0xf0, 0x7f, 0x66, 0xaa,
    0xc8, 0x88, 0xd7, 0x31, 0x84, 0x71, 0xb6, 0x64,
    0x09, 0x76, 0x0b, 0x7f, 0x1a, 0x1f, 0x2e, 0xfe,
    0xcd, 0x59, 0x2a, 0x54, 0x11, 0x84, 0xd4, 0x6a,
    0x61, 0xdf, 0xaa, 0x76, 0x66, 0x9d, 0x82, 0x11,
    0x56, 0x3d, 0xd2, 0x52, 0xe6, 0x42, 0x5a, 0x77,
    0x92, 0x98, 0x34, 0xf3, 0x56, 0x6c, 0x96, 0x10,
    0x40, 0x59, 0x16, 0xcb, 0x77, 0x61, 0xe3, 0xbf,
    0x4b, 0xd4, 0x39, 0xfb, 0xb1, 0x4e, 0xc1, 0x74,
    0xec, 0x7a, 0xea, 0x3d, 0x68, 0xbb, 0x0b, 0xe6,
    0xc6, 0x06, 0xbf, 0xdd, 0x7f, 0x94, 0x42, 0xc0,
    0x0f, 0xe4, 0x92, 0x33, 0x6c, 0x6e, 0x1b, 0xba,
    0x73, 0xf9, 0x79, 0x84, 0xdf, 0x45, 0x00, 0xe4,
    0x94, 0x88, 0x9d, 0x08, 0x89, 0xcf, 0xf2, 0xa4,
    0xc5, 0x47, 0x45, 0x85, 0x86, 0xa5, 0xcc, 0xa8,
    0xf2, 0x5d, 0x58, 0x07
};

static void test_import_private(void) 
{
    DWORD dwLen;
    HCRYPTKEY hKeyExchangeKey, hSessionKey;
    BOOL result;
    static BYTE abSessionKey[148] = {
        0x01, 0x02, 0x00, 0x00, 0x01, 0x68, 0x00, 0x00,
        0x00, 0xa4, 0x00, 0x00, 0xb8, 0xa4, 0xdf, 0x5e,
        0x9e, 0xb1, 0xbf, 0x85, 0x3d, 0x24, 0x2d, 0x1e,
        0x69, 0xb7, 0x67, 0x13, 0x8e, 0x78, 0xf2, 0xdf,
        0xc6, 0x69, 0xce, 0x46, 0x7e, 0xf2, 0xf2, 0x33,
        0x20, 0x6f, 0xa1, 0xa5, 0x59, 0x83, 0x25, 0xcb,
        0x3a, 0xb1, 0x8a, 0x12, 0x63, 0x02, 0x3c, 0xfb,
        0x4a, 0xfa, 0xef, 0x8e, 0xf7, 0x29, 0x57, 0xb1,
        0x9e, 0xa7, 0xf3, 0x02, 0xfd, 0xca, 0xdf, 0x5a,
        0x1f, 0x71, 0xb6, 0x26, 0x09, 0x24, 0x39, 0xda,
        0xc0, 0xde, 0x2a, 0x0e, 0xcd, 0x1f, 0xe5, 0xb6,
        0x4f, 0x82, 0xa0, 0xa9, 0x90, 0xd3, 0xd9, 0x6a,
        0x43, 0x14, 0x2a, 0xf7, 0xac, 0xd5, 0xa0, 0x54,
        0x93, 0xc4, 0xb9, 0xe7, 0x24, 0x84, 0x4d, 0x69,
        0x5e, 0xcc, 0x2a, 0x32, 0xb5, 0xfb, 0xe4, 0xb4,
        0x08, 0xd5, 0x36, 0x58, 0x59, 0x40, 0xfb, 0x29,
        0x7f, 0xa7, 0x17, 0x25, 0xc4, 0x0d, 0x78, 0x37,
        0x04, 0x8c, 0x49, 0x92
    };
    static BYTE abEncryptedMessage[12] = {
        0x40, 0x64, 0x28, 0xe8, 0x8a, 0xe7, 0xa4, 0xd4,
        0x1c, 0xfd, 0xde, 0x71
    };
            
    dwLen = (DWORD)sizeof(abPlainPrivateKey);
    result = CryptImportKey(hProv, abPlainPrivateKey, dwLen, 0, 0, &hKeyExchangeKey);
    if (!result) {
        /* rsaenh compiled without OpenSSL */
        ok(GetLastError() == NTE_FAIL, "%08lx\n", GetLastError());
        return;
    }

    dwLen = (DWORD)sizeof(abSessionKey);
    result = CryptImportKey(hProv, abSessionKey, dwLen, hKeyExchangeKey, 0, &hSessionKey);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = (DWORD)sizeof(abEncryptedMessage);
    result = CryptDecrypt(hSessionKey, 0, TRUE, 0, abEncryptedMessage, &dwLen);
    ok(result && dwLen == 12 && !memcmp(abEncryptedMessage, "Wine rocks!",12), 
       "%08lx, len: %ld\n", GetLastError(), dwLen);
    
    if (!derive_key(CALG_RC4, &hSessionKey, 56)) return;

    dwLen = (DWORD)sizeof(abSessionKey);
    result = CryptExportKey(hSessionKey, hKeyExchangeKey, SIMPLEBLOB, 0, abSessionKey, &dwLen);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = (DWORD)sizeof(abSessionKey);
    result = CryptImportKey(hProv, abSessionKey, dwLen, hKeyExchangeKey, 0, &hSessionKey);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;
}

static void test_verify_signature(void) {
    HCRYPTHASH hHash;
    HCRYPTKEY hPubSignKey;
    BYTE abData[] = "Wine rocks!";
    BOOL result;
    BYTE abPubKey[148] = {
        0x06, 0x02, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 
        0x52, 0x53, 0x41, 0x31, 0x00, 0x04, 0x00, 0x00, 
        0x01, 0x00, 0x01, 0x00, 0x71, 0x64, 0x9f, 0x19, 
        0x89, 0x1c, 0x21, 0xcc, 0x36, 0xa3, 0xc9, 0x27, 
        0x08, 0x8a, 0x09, 0xc6, 0xbe, 0xeb, 0xd3, 0xf8, 
        0x19, 0xa9, 0x92, 0x57, 0xe4, 0xb9, 0x5d, 0xda, 
        0x88, 0x93, 0xe4, 0x6b, 0x38, 0x77, 0x14, 0x8a, 
        0x96, 0xc0, 0xb6, 0x4e, 0x42, 0xf5, 0x01, 0xdc, 
        0xf0, 0xeb, 0x3c, 0xc7, 0x7b, 0xc4, 0xfd, 0x7c, 
        0xde, 0x93, 0x34, 0x0a, 0x92, 0xe5, 0x97, 0x9c, 
        0x3e, 0x65, 0xb8, 0x91, 0x2f, 0xe3, 0xf3, 0x89, 
        0xcd, 0x6c, 0x26, 0xa4, 0x6c, 0xc7, 0x6d, 0x0b, 
        0x2c, 0xa2, 0x0b, 0x29, 0xe2, 0xfc, 0x30, 0xfa, 
        0x20, 0xdb, 0x4c, 0xb8, 0x91, 0xb8, 0x69, 0x63, 
        0x96, 0x41, 0xc2, 0xb4, 0x60, 0xeb, 0xcd, 0xff, 
        0x3a, 0x1f, 0x94, 0xb1, 0x23, 0xcf, 0x0f, 0x49, 
        0xad, 0xd5, 0x33, 0x85, 0x71, 0xaf, 0x12, 0x87, 
        0x84, 0xef, 0xa0, 0xea, 0xe1, 0xc1, 0xd4, 0xc7, 
        0xe1, 0x21, 0x50, 0xac
    };
    /* md2 with hash oid */
    BYTE abSignatureMD2[128] = {
        0x4a, 0x4e, 0xb7, 0x5e, 0x32, 0xda, 0xdb, 0x67, 
        0x9f, 0x77, 0x84, 0x32, 0x00, 0xba, 0x5f, 0x6b, 
        0x0d, 0xcf, 0xd9, 0x99, 0xbd, 0x96, 0x31, 0xda, 
        0x23, 0x4c, 0xd9, 0x4a, 0x90, 0x84, 0x20, 0x59, 
        0x51, 0xdc, 0xd4, 0x93, 0x3a, 0xae, 0x0a, 0x0a, 
        0xa1, 0x76, 0xfa, 0xb5, 0x68, 0xee, 0xc7, 0x34, 
        0x41, 0xd3, 0xe7, 0x5a, 0x0e, 0x22, 0x61, 0x40, 
        0xea, 0x24, 0x56, 0xf1, 0x91, 0x5a, 0xf7, 0xa7, 
        0x5b, 0xf4, 0x98, 0x6b, 0xc3, 0xef, 0xad, 0xc0, 
        0x5e, 0x6b, 0x87, 0x76, 0xcb, 0x1f, 0x62, 0x06, 
        0x7c, 0xf6, 0x48, 0x97, 0x81, 0x8d, 0xef, 0x51, 
        0x51, 0xdc, 0x21, 0x91, 0x57, 0x1e, 0x79, 0x6f, 
        0x49, 0xb5, 0xde, 0x31, 0x07, 0x45, 0x99, 0x46, 
        0xc3, 0x4f, 0xca, 0x2d, 0x0e, 0x4c, 0x10, 0x25, 
        0xcb, 0x1a, 0x98, 0x63, 0x41, 0x93, 0x47, 0xc0, 
        0xb2, 0xbc, 0x10, 0x3c, 0xe7, 0xd4, 0x3c, 0x1e
    };
    /* md2 without hash oid */
    BYTE abSignatureMD2NoOID[128] = {
        0x0c, 0x21, 0x3e, 0x60, 0xf9, 0xd0, 0x36, 0x2d, 
        0xe1, 0x10, 0x45, 0x45, 0x85, 0x03, 0x29, 0x19, 
        0xef, 0x19, 0xd9, 0xa6, 0x7e, 0x9c, 0x0d, 0xbd, 
        0x03, 0x0e, 0xb9, 0x51, 0x9e, 0x74, 0x79, 0xc4, 
        0xde, 0x25, 0xf2, 0x35, 0x74, 0x55, 0xbc, 0x65, 
        0x7e, 0x33, 0x28, 0xa8, 0x1e, 0x72, 0xaa, 0x99, 
        0xdd, 0xf5, 0x26, 0x20, 0x29, 0xf8, 0xa6, 0xdf, 
        0x28, 0x4b, 0x1c, 0xdb, 0xa1, 0x41, 0x56, 0xbc, 
        0xf9, 0x9c, 0x66, 0xc0, 0x37, 0x41, 0x55, 0xa0, 
        0xe2, 0xec, 0xbf, 0x71, 0xf0, 0x5d, 0x25, 0x01, 
        0x75, 0x91, 0xe2, 0x81, 0xb2, 0x9f, 0x57, 0xa7, 
        0x5c, 0xd2, 0xfa, 0x66, 0xdb, 0x71, 0x2b, 0x1f, 
        0xad, 0x30, 0xde, 0xea, 0x49, 0x73, 0x30, 0x6a, 
        0x22, 0x54, 0x49, 0x4e, 0xae, 0xf6, 0x88, 0xc9, 
        0xff, 0x71, 0xba, 0xbf, 0x27, 0xc5, 0xfa, 0x06, 
        0xe2, 0x91, 0x71, 0x8a, 0x7e, 0x0c, 0xc2, 0x07
    };
    /* md4 with hash oid */
    BYTE abSignatureMD4[128] = {
        0x1c, 0x78, 0xaa, 0xea, 0x74, 0xf4, 0x83, 0x51, 
        0xae, 0x66, 0xe3, 0xa9, 0x1c, 0x03, 0x39, 0x1b, 
        0xac, 0x7e, 0x4e, 0x85, 0x7e, 0x1c, 0x38, 0xd2, 
        0x82, 0x43, 0xb3, 0x6f, 0x6f, 0x46, 0x45, 0x8e, 
        0x17, 0x74, 0x58, 0x29, 0xca, 0xe1, 0x03, 0x13, 
        0x45, 0x79, 0x34, 0xdf, 0x5c, 0xd6, 0xc3, 0xf9, 
        0x7a, 0x1c, 0x9d, 0xff, 0x6f, 0x03, 0x7d, 0x0f, 
        0x59, 0x1a, 0x2d, 0x0e, 0x94, 0xb4, 0x75, 0x96, 
        0xd1, 0x48, 0x63, 0x6e, 0xb2, 0xc4, 0x5c, 0xd9, 
        0xab, 0x49, 0xb4, 0x90, 0xd9, 0x57, 0x04, 0x6e, 
        0x4c, 0xb6, 0xea, 0x00, 0x94, 0x4a, 0x34, 0xa0, 
        0xd9, 0x63, 0xef, 0x2c, 0xde, 0x5b, 0xb9, 0xbe, 
        0x35, 0xc8, 0xc1, 0x31, 0xb5, 0x31, 0x15, 0x18, 
        0x90, 0x39, 0xf5, 0x2a, 0x34, 0x6d, 0xb4, 0xab, 
        0x09, 0x34, 0x69, 0x54, 0x4d, 0x11, 0x2f, 0xf3, 
        0xa2, 0x36, 0x0e, 0xa8, 0x45, 0xe7, 0x36, 0xac
    };
    /* md4 without hash oid */
    BYTE abSignatureMD4NoOID[128] = {
        0xd3, 0x60, 0xb2, 0xb0, 0x22, 0x0a, 0x99, 0xda, 
        0x04, 0x85, 0x64, 0xc6, 0xc6, 0xdb, 0x11, 0x24, 
        0xe9, 0x68, 0x2d, 0xf7, 0x09, 0xef, 0xb6, 0xa0, 
        0xa2, 0xfe, 0x45, 0xee, 0x85, 0x49, 0xcd, 0x36, 
        0xf7, 0xc7, 0x9d, 0x2b, 0x4c, 0x68, 0xda, 0x85, 
        0x8c, 0x50, 0xcc, 0x4f, 0x4b, 0xe1, 0x82, 0xc3, 
        0xbe, 0xa3, 0xf1, 0x78, 0x6b, 0x60, 0x42, 0x3f, 
        0x67, 0x22, 0x14, 0xe4, 0xe1, 0xa4, 0x6e, 0xa9, 
        0x4e, 0xf1, 0xd4, 0xb0, 0xce, 0x82, 0xac, 0x06, 
        0xba, 0x2c, 0xbc, 0xf7, 0xcb, 0xf6, 0x0c, 0x3f, 
        0xf6, 0x79, 0xfe, 0xb3, 0xd8, 0x5a, 0xbc, 0xdb, 
        0x05, 0x41, 0xa4, 0x07, 0x57, 0x9e, 0xa2, 0x96, 
        0xfc, 0x60, 0x4b, 0xf7, 0x6f, 0x86, 0x26, 0x1f, 
        0xc2, 0x2c, 0x67, 0x08, 0xcd, 0x7f, 0x91, 0xe9, 
        0x16, 0xb5, 0x0e, 0xd9, 0xc4, 0xc4, 0x97, 0xeb, 
        0x91, 0x3f, 0x20, 0x6c, 0xf0, 0x68, 0x86, 0x7f
    }; 
    /* md5 with hash oid */
    BYTE abSignatureMD5[128] = {
        0x4f, 0xe0, 0x8c, 0x9b, 0x43, 0xdd, 0x02, 0xe5, 
        0xf4, 0xa1, 0xdd, 0x88, 0x4c, 0x9c, 0x40, 0x0f, 
        0x6c, 0x43, 0x86, 0x64, 0x00, 0xe6, 0xac, 0xf7, 
        0xd0, 0x92, 0xaa, 0xc4, 0x62, 0x9a, 0x48, 0x98, 
        0x1a, 0x56, 0x6d, 0x75, 0xec, 0x04, 0x89, 0xec, 
        0x69, 0x93, 0xd6, 0x61, 0x37, 0xb2, 0x36, 0xb5, 
        0xb2, 0xba, 0xf2, 0xf5, 0x21, 0x0c, 0xf1, 0x04, 
        0xc8, 0x2d, 0xf5, 0xa0, 0x8d, 0x6d, 0x10, 0x0b, 
        0x68, 0x63, 0xf2, 0x08, 0x68, 0xdc, 0xbd, 0x95, 
        0x25, 0x7d, 0xee, 0x63, 0x5c, 0x3b, 0x98, 0x4c, 
        0xea, 0x41, 0xdc, 0x6a, 0x8b, 0x6c, 0xbb, 0x29, 
        0x2b, 0x1c, 0x5c, 0x8b, 0x7d, 0x94, 0x24, 0xa9, 
        0x7a, 0x62, 0x94, 0xf3, 0x3a, 0x6a, 0xb2, 0x4c, 
        0x33, 0x59, 0x00, 0xcd, 0x7d, 0x37, 0x79, 0x90, 
        0x31, 0xd1, 0xd9, 0x84, 0x12, 0xe5, 0x08, 0x5e, 
        0xb3, 0x60, 0x61, 0x27, 0x78, 0x37, 0x63, 0x01
    };
    /* md5 without hash oid */
    BYTE abSignatureMD5NoOID[128] = {
        0xc6, 0xad, 0x5c, 0x2b, 0x9b, 0xe0, 0x99, 0x2f, 
        0x5e, 0x55, 0x04, 0x32, 0x65, 0xe0, 0xb5, 0x75, 
        0x01, 0x9a, 0x11, 0x4d, 0x0e, 0x9a, 0xe1, 0x9f, 
        0xc7, 0xbf, 0x77, 0x6d, 0xa9, 0xfd, 0xcc, 0x9d, 
        0x8b, 0xd1, 0x31, 0xed, 0x5a, 0xd2, 0xe5, 0x5f, 
        0x42, 0x3b, 0xb5, 0x3c, 0x32, 0x30, 0x88, 0x49, 
        0xcb, 0x67, 0xb8, 0x2e, 0xc9, 0xf5, 0x2b, 0xc8, 
        0x35, 0x71, 0xb5, 0x1b, 0x32, 0x3f, 0x44, 0x4c, 
        0x66, 0x93, 0xcb, 0xe8, 0x48, 0x7c, 0x14, 0x23, 
        0xfb, 0x12, 0xa5, 0xb7, 0x86, 0x94, 0x6b, 0x19, 
        0x17, 0x20, 0xc6, 0xb8, 0x09, 0xe8, 0xbb, 0xdb, 
        0x00, 0x2b, 0x96, 0x4a, 0x93, 0x00, 0x26, 0xd3, 
        0x07, 0xa0, 0x06, 0xce, 0x5a, 0x13, 0x69, 0x6b, 
        0x62, 0x5a, 0x56, 0x61, 0x6a, 0xd8, 0x11, 0x3b, 
        0xd5, 0x67, 0xc7, 0x4d, 0xf6, 0x66, 0x63, 0xc5, 
        0xe3, 0x8f, 0x7c, 0x7c, 0xb1, 0x3e, 0x55, 0x43
    };
    /* sha with hash oid */
    BYTE abSignatureSHA[128] = {
        0x5a, 0x4c, 0x66, 0xc9, 0x30, 0x67, 0xcb, 0x91, 
        0x3c, 0x4d, 0xd5, 0x8d, 0xea, 0x4e, 0x85, 0xcd, 
        0xd9, 0x68, 0x3a, 0xf3, 0x24, 0x3c, 0x99, 0x24, 
        0x25, 0x32, 0x93, 0x3d, 0xd6, 0x2f, 0x86, 0x94, 
        0x23, 0x09, 0xee, 0x02, 0xd4, 0x15, 0xdc, 0x5f, 
        0x0e, 0x44, 0x45, 0x13, 0x5f, 0x18, 0x5d, 0x1a, 
        0xd7, 0x0b, 0xd1, 0x23, 0xd6, 0x35, 0x98, 0x52, 
        0x57, 0x45, 0x74, 0x92, 0xe3, 0x50, 0xb4, 0x20, 
        0x28, 0x2a, 0x11, 0xbf, 0x49, 0xb4, 0x2c, 0xc5, 
        0xd4, 0x1a, 0x27, 0x4e, 0xdf, 0xa0, 0xb5, 0x7a, 
        0xc8, 0x14, 0xdd, 0x9b, 0xb6, 0xca, 0xd6, 0xff, 
        0xb2, 0x6b, 0xd8, 0x98, 0x67, 0x80, 0xab, 0x53, 
        0x52, 0xbb, 0xe1, 0x2a, 0xce, 0x79, 0x2f, 0x00, 
        0x53, 0x26, 0xd8, 0xa7, 0x43, 0xca, 0x72, 0x0e, 
        0x68, 0x97, 0x37, 0x71, 0x87, 0xc2, 0x6a, 0x98, 
        0xbb, 0x6c, 0xa0, 0x01, 0xff, 0x04, 0x9d, 0xa6
    };
    /* sha without hash oid */
    BYTE abSignatureSHANoOID[128] = {
        0x86, 0xa6, 0x2b, 0x9a, 0x04, 0xda, 0x47, 0xc6, 
        0x4f, 0x97, 0x8a, 0x8a, 0xf4, 0xfa, 0x63, 0x1a, 
        0x32, 0x89, 0x56, 0x41, 0x37, 0x91, 0x15, 0x2f, 
        0x2d, 0x1c, 0x8f, 0xdc, 0x88, 0x40, 0xbb, 0x37, 
        0x3e, 0x06, 0x33, 0x1b, 0xde, 0xda, 0x7c, 0x65, 
        0x91, 0x35, 0xca, 0x45, 0x17, 0x0e, 0x24, 0xbe, 
        0x9e, 0xf6, 0x4e, 0x8a, 0xa4, 0x3e, 0xca, 0xe6, 
        0x11, 0x36, 0xb8, 0x3a, 0xf0, 0xde, 0x71, 0xfe, 
        0xdd, 0xb3, 0xcb, 0x6c, 0x39, 0xe0, 0x5f, 0x0c, 
        0x9e, 0xa8, 0x40, 0x26, 0x9c, 0x81, 0xe9, 0xc4, 
        0x15, 0x90, 0xbf, 0x4f, 0xd2, 0xc1, 0xa1, 0x80, 
        0x52, 0xfd, 0xf6, 0x3d, 0x99, 0x1b, 0x9c, 0x8a, 
        0x27, 0x1b, 0x0c, 0x9a, 0xf3, 0xf9, 0xa2, 0x00, 
        0x3e, 0x5b, 0xdf, 0xc2, 0xb4, 0x71, 0xa5, 0xbd, 
        0xf8, 0xae, 0x63, 0xbb, 0x4a, 0xc9, 0xdd, 0x67, 
        0xc1, 0x3e, 0x93, 0xee, 0xf1, 0x1f, 0x24, 0x5b
    }; 
    
    result = CryptImportKey(hProv, abPubKey, 148, 0, 0, &hPubSignKey);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptCreateHash(hProv, CALG_MD2, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHash, abData, (DWORD)sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD2, 128, hPubSignKey, NULL, 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD2NoOID, 128, hPubSignKey, NULL, CRYPT_NOHASHOID);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Next test fails on WinXP SP2. It seems that CPVerifySignature doesn't care about 
     * the OID at all. */
    /*result = CryptVerifySignature(hHash, abSignatureMD2NoOID, 128, hPubSignKey, NULL, 0);
    ok(!result && GetLastError()==NTE_BAD_SIGNATURE, "%08lx\n", GetLastError());
    if (result) return;*/

    result = CryptCreateHash(hProv, CALG_MD4, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHash, abData, (DWORD)sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD4, 128, hPubSignKey, NULL, 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD4NoOID, 128, hPubSignKey, NULL, CRYPT_NOHASHOID);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHash, abData, (DWORD)sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD5, 128, hPubSignKey, NULL, 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureMD5NoOID, 128, hPubSignKey, NULL, CRYPT_NOHASHOID);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHash, abData, (DWORD)sizeof(abData), 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureSHA, 128, hPubSignKey, NULL, 0);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptVerifySignature(hHash, abSignatureSHANoOID, 128, hPubSignKey, NULL, CRYPT_NOHASHOID);
    ok(result, "%08lx\n", GetLastError());
    if (!result) return;
}

static void test_rsa_encrypt(void)
{
    HCRYPTKEY hRSAKey;
    BYTE abData[2048] = "Wine rocks!";
    BOOL result;
    DWORD dwLen;

    /* It is allowed to use the key exchange key for encryption/decryption */
    result = CryptGetUserKey(hProv, AT_KEYEXCHANGE, &hRSAKey);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = 12;
    result = CryptEncrypt(hRSAKey, 0, TRUE, 0, NULL, &dwLen, (DWORD)sizeof(abData));
    ok(result, "CryptEncrypt failed: %08lx\n", GetLastError());
    ok(dwLen == 128, "Unexpected length %ld\n", dwLen);
    dwLen = 12;
    result = CryptEncrypt(hRSAKey, 0, TRUE, 0, abData, &dwLen, (DWORD)sizeof(abData));
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptDecrypt(hRSAKey, 0, TRUE, 0, abData, &dwLen);
    ok (result && dwLen == 12 && !memcmp(abData, "Wine rocks!", 12), "%08lx\n", GetLastError());
    
    CryptDestroyKey(hRSAKey);

    /* It is not allowed to use the signature key for encryption/decryption */
    result = CryptGetUserKey(hProv, AT_SIGNATURE, &hRSAKey);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = 12;
    result = CryptEncrypt(hRSAKey, 0, TRUE, 0, abData, &dwLen, (DWORD)sizeof(abData));
    ok (!result && GetLastError() == NTE_BAD_KEY, "%08lx\n", GetLastError());

    CryptDestroyKey(hRSAKey);
}
        
static void test_schannel_provider(void)
{
    HCRYPTPROV hProv;
    HCRYPTKEY hRSAKey, hMasterSecret, hServerWriteKey, hServerWriteMACKey;
    HCRYPTHASH hMasterHash, hTLS1PRF, hHMAC;
    BOOL result;
    DWORD dwLen;
    SCHANNEL_ALG saSChannelAlg;
    CRYPT_DATA_BLOB data_blob;
    HMAC_INFO hmacInfo = { CALG_MD5, NULL, 0, NULL, 0 };
    BYTE abPlainPrivateKey[596] = {
        0x07, 0x02, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00,
        0x52, 0x53, 0x41, 0x32, 0x00, 0x04, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x9b, 0x64, 0xef, 0xce,
        0x31, 0x7c, 0xad, 0x56, 0xe2, 0x1e, 0x9b, 0x96,
        0xb3, 0xf0, 0x29, 0x88, 0x6e, 0xa8, 0xc2, 0x11,
        0x33, 0xd6, 0xcc, 0x8c, 0x69, 0xb2, 0x1a, 0xfd,
        0xfc, 0x23, 0x21, 0x30, 0x4d, 0x29, 0x45, 0xb6,
        0x3a, 0x67, 0x11, 0x80, 0x1a, 0x91, 0xf2, 0x9f,
        0x01, 0xac, 0xc0, 0x11, 0x50, 0x5f, 0xcd, 0xb9,
        0xad, 0x76, 0x9f, 0x6e, 0x91, 0x55, 0x71, 0xda,
        0x97, 0x96, 0x96, 0x22, 0x75, 0xb4, 0x83, 0x44,
        0x89, 0x9e, 0xf8, 0x44, 0x40, 0x7c, 0xd6, 0xcd,
        0x9d, 0x88, 0xd6, 0x88, 0xbc, 0x56, 0xb7, 0x64,
        0xe9, 0x2c, 0x24, 0x2f, 0x0d, 0x78, 0x55, 0x1c,
        0xb2, 0x67, 0xb1, 0x5e, 0xbc, 0x0c, 0xcf, 0x1c,
        0xe9, 0xd3, 0x9e, 0xa2, 0x15, 0x24, 0x73, 0xd6,
        0xdb, 0x6f, 0x83, 0xb2, 0xf8, 0xbc, 0xe7, 0x47,
        0x3b, 0x01, 0xef, 0x49, 0x08, 0x98, 0xd6, 0xa3,
        0xf9, 0x25, 0x57, 0xe9, 0x39, 0x3c, 0x53, 0x30,
        0x1b, 0xf2, 0xc9, 0x62, 0x31, 0x43, 0x5d, 0x84,
        0x24, 0x30, 0x21, 0x9a, 0xad, 0xdb, 0x62, 0x91,
        0xc8, 0x07, 0xd9, 0x2f, 0xd6, 0xb5, 0x37, 0x6f,
        0xfe, 0x7a, 0x12, 0xbc, 0xd9, 0xd2, 0x2b, 0xbf,
        0xd7, 0xb1, 0xfa, 0x7d, 0xc0, 0x48, 0xdd, 0x74,
        0xdd, 0x55, 0x04, 0xa1, 0x8b, 0xc1, 0x0a, 0xc4,
        0xa5, 0x57, 0x62, 0xee, 0x08, 0x8b, 0xf9, 0x19,
        0x6c, 0x52, 0x06, 0xf8, 0x73, 0x0f, 0x24, 0xc9,
        0x71, 0x9f, 0xc5, 0x45, 0x17, 0x3e, 0xae, 0x06,
        0x81, 0xa2, 0x96, 0x40, 0x06, 0xbf, 0xeb, 0x9e,
        0x80, 0x2b, 0x27, 0x20, 0x8f, 0x38, 0xcf, 0xeb,
        0xff, 0x3b, 0x38, 0x41, 0x35, 0x69, 0x66, 0x13,
        0x1d, 0x3c, 0x01, 0x3b, 0xf6, 0x37, 0xca, 0x9c,
        0x61, 0x74, 0x98, 0xcf, 0xc9, 0x6e, 0xe8, 0x90,
        0xc7, 0xb7, 0x33, 0xc0, 0x07, 0x3c, 0xf8, 0xc8,
        0xf6, 0xf2, 0xd7, 0xf0, 0x21, 0x62, 0x58, 0x8a,
        0x55, 0xbf, 0xa1, 0x2d, 0x3d, 0xa6, 0x69, 0xc5,
        0x02, 0x19, 0x31, 0xf0, 0x94, 0x0f, 0x45, 0x5c,
        0x95, 0x1b, 0x53, 0xbc, 0xf5, 0xb0, 0x1a, 0x8f,
        0xbf, 0x40, 0xe0, 0xc7, 0x73, 0xe7, 0x72, 0x6e,
        0xeb, 0xb1, 0x0f, 0x38, 0xc5, 0xf8, 0xee, 0x04,
        0xed, 0x34, 0x1a, 0x10, 0xf9, 0x53, 0x34, 0xf3,
        0x3e, 0xe6, 0x5c, 0xd1, 0x47, 0x65, 0xcd, 0xbd,
        0xf1, 0x06, 0xcb, 0xb4, 0xb1, 0x26, 0x39, 0x9f,
        0x71, 0xfe, 0x3d, 0xf8, 0x62, 0xab, 0x22, 0x8b,
        0x0e, 0xdc, 0xb9, 0xe8, 0x74, 0x06, 0xfc, 0x8c,
        0x25, 0xa1, 0xa9, 0xcf, 0x07, 0xf9, 0xac, 0x21,
        0x01, 0x7b, 0x1c, 0xdc, 0x94, 0xbd, 0x47, 0xe1,
        0xa0, 0x86, 0x59, 0x35, 0x6a, 0x6f, 0xb9, 0x70,
        0x26, 0x7c, 0x3c, 0xfd, 0xbd, 0x81, 0x39, 0x36,
        0x42, 0xc2, 0xbd, 0xbe, 0x84, 0x27, 0x9a, 0x69,
        0x81, 0xda, 0x99, 0x27, 0xc2, 0x4f, 0x62, 0x33,
        0xf4, 0x79, 0x30, 0xc5, 0x63, 0x54, 0x71, 0xf1,
        0x47, 0x22, 0x25, 0x9b, 0x6c, 0x00, 0x2f, 0x1c,
        0xf4, 0x1f, 0x85, 0xbc, 0xf6, 0x67, 0x6a, 0xe3,
        0xf6, 0x55, 0x8a, 0xef, 0xd0, 0x0b, 0xd3, 0xa2,
        0xc5, 0x51, 0x70, 0x15, 0x0a, 0xf0, 0x98, 0x4c,
        0xb7, 0x19, 0x62, 0x0e, 0x2d, 0x2a, 0x4a, 0x7d,
        0x7a, 0x0a, 0xc4, 0x17, 0xe3, 0x5d, 0x20, 0x52,
        0xa9, 0x98, 0xc3, 0xaa, 0x11, 0xf6, 0xbf, 0x4c,
        0x94, 0x99, 0x81, 0x89, 0xf0, 0x7f, 0x66, 0xaa,
        0xc8, 0x88, 0xd7, 0x31, 0x84, 0x71, 0xb6, 0x64,
        0x09, 0x76, 0x0b, 0x7f, 0x1a, 0x1f, 0x2e, 0xfe,
        0xcd, 0x59, 0x2a, 0x54, 0x11, 0x84, 0xd4, 0x6a,
        0x61, 0xdf, 0xaa, 0x76, 0x66, 0x9d, 0x82, 0x11,
        0x56, 0x3d, 0xd2, 0x52, 0xe6, 0x42, 0x5a, 0x77,
        0x92, 0x98, 0x34, 0xf3, 0x56, 0x6c, 0x96, 0x10,
        0x40, 0x59, 0x16, 0xcb, 0x77, 0x61, 0xe3, 0xbf,
        0x4b, 0xd4, 0x39, 0xfb, 0xb1, 0x4e, 0xc1, 0x74,
        0xec, 0x7a, 0xea, 0x3d, 0x68, 0xbb, 0x0b, 0xe6,
        0xc6, 0x06, 0xbf, 0xdd, 0x7f, 0x94, 0x42, 0xc0,
        0x0f, 0xe4, 0x92, 0x33, 0x6c, 0x6e, 0x1b, 0xba,
        0x73, 0xf9, 0x79, 0x84, 0xdf, 0x45, 0x00, 0xe4,
        0x94, 0x88, 0x9d, 0x08, 0x89, 0xcf, 0xf2, 0xa4,
        0xc5, 0x47, 0x45, 0x85, 0x86, 0xa5, 0xcc, 0xa8,
        0xf2, 0x5d, 0x58, 0x07
    };
    BYTE abTLS1Master[140] = {
        0x01, 0x02, 0x00, 0x00, 0x06, 0x4c, 0x00, 0x00, 
        0x00, 0xa4, 0x00, 0x00, 0x5b, 0x13, 0xc7, 0x68, 
        0xd8, 0x55, 0x23, 0x5d, 0xbc, 0xa6, 0x9d, 0x97, 
        0x0e, 0xcd, 0x6b, 0xcf, 0xc0, 0xdc, 0xc5, 0x53, 
        0x28, 0xa0, 0xca, 0xc1, 0x63, 0x4e, 0x3a, 0x24, 
        0x22, 0xe5, 0x4d, 0x15, 0xbb, 0xa5, 0x06, 0xc3, 
        0x98, 0x25, 0xdc, 0x35, 0xd3, 0xdb, 0xab, 0xb8, 
        0x44, 0x1b, 0xfe, 0x63, 0x88, 0x7c, 0x2e, 0x6d, 
        0x34, 0xd9, 0x0f, 0x7e, 0x2f, 0xc2, 0xb2, 0x6e, 
        0x56, 0xfa, 0xab, 0xb2, 0x88, 0xf6, 0x15, 0x6e, 
        0xa8, 0xcd, 0x70, 0x16, 0x94, 0x61, 0x07, 0x40, 
        0x9e, 0x25, 0x22, 0xf8, 0x64, 0x9f, 0xcc, 0x0b, 
        0xf1, 0x92, 0x4d, 0xfe, 0xc3, 0x5d, 0x52, 0xdb, 
        0x0f, 0xff, 0x12, 0x0f, 0x49, 0x43, 0x7d, 0xc6, 
        0x52, 0x61, 0xb0, 0x06, 0xc8, 0x1b, 0x90, 0xac, 
        0x09, 0x7e, 0x4b, 0x95, 0x69, 0x3b, 0x0d, 0x41, 
        0x1b, 0x4c, 0x65, 0x75, 0x4d, 0x85, 0x16, 0xc4, 
        0xd3, 0x1e, 0x82, 0xb3
    };
    BYTE abServerSecret[33] = "Super Secret Server Secret 12345";
    BYTE abClientSecret[33] = "Super Secret Client Secret 12345";
    BYTE abHashedHandshakes[37] = "123456789012345678901234567890123456";
    BYTE abClientFinished[16] = "client finished";
    BYTE abData[16] = "Wine rocks!";
    BYTE abMD5Hash[16];
    static const BYTE abEncryptedData[16] = {
        0x13, 0xd2, 0xdd, 0xeb, 0x6c, 0x3f, 0xbe, 0xb2,
        0x04, 0x86, 0xb5, 0xe5, 0x08, 0xe5, 0xf3, 0x0d    
    };
    static const BYTE abPRF[16] = {
        0xa8, 0xb2, 0xa6, 0xef, 0x83, 0x4e, 0x74, 0xb1,
        0xf3, 0xb1, 0x51, 0x5a, 0x1a, 0x2b, 0x11, 0x31
    };
    static const BYTE abMD5[16] = {
        0xe1, 0x65, 0x3f, 0xdb, 0xbb, 0x3d, 0x99, 0x3c,
        0x3d, 0xca, 0x6a, 0x6f, 0xfa, 0x15, 0x4e, 0xaa
    };
    
    result = CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_SCHANNEL, CRYPT_VERIFYCONTEXT);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;
    
    /* To get deterministic results, we import the TLS1 master secret (which
     * is typically generated from a random generator). Therefore, we need
     * an RSA key. */
    dwLen = (DWORD)sizeof(abPlainPrivateKey);
    result = CryptImportKey(hProv, abPlainPrivateKey, dwLen, 0, 0, &hRSAKey);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = (DWORD)sizeof(abTLS1Master);
    result = CryptImportKey(hProv, abTLS1Master, dwLen, hRSAKey, 0, &hMasterSecret);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;    
   
    /* Setting the TLS1 client and server random parameters, as well as the 
     * MAC and encryption algorithm parameters. */
    data_blob.cbData = 33;
    data_blob.pbData = abClientSecret;
    result = CryptSetKeyParam(hMasterSecret, KP_CLIENT_RANDOM, (BYTE*)&data_blob, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    data_blob.cbData = 33;
    data_blob.pbData = abServerSecret;
    result = CryptSetKeyParam(hMasterSecret, KP_SERVER_RANDOM, (BYTE*)&data_blob, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;
    
    saSChannelAlg.dwUse = SCHANNEL_ENC_KEY;
    saSChannelAlg.Algid = CALG_DES;
    saSChannelAlg.cBits = 64;
    saSChannelAlg.dwFlags = 0;
    saSChannelAlg.dwReserved = 0;
    result = CryptSetKeyParam(hMasterSecret, KP_SCHANNEL_ALG, (PBYTE)&saSChannelAlg, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    saSChannelAlg.dwUse = SCHANNEL_MAC_KEY;
    saSChannelAlg.Algid = CALG_MD5;
    saSChannelAlg.cBits = 128;
    saSChannelAlg.dwFlags = 0;
    saSChannelAlg.dwReserved = 0;
    result = CryptSetKeyParam(hMasterSecret, KP_SCHANNEL_ALG, (PBYTE)&saSChannelAlg, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Deriving a hash from the master secret. This is due to the CryptoAPI architecture.
     * (Keys can only be derived from hashes, not from other keys.) */
    result = CryptCreateHash(hProv, CALG_SCHANNEL_MASTER_HASH, hMasterSecret, 0, &hMasterHash);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Deriving the server write encryption key from the master hash */
    result = CryptDeriveKey(hProv, CALG_SCHANNEL_ENC_KEY, hMasterHash, CRYPT_SERVER, &hServerWriteKey);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Encrypting some data with the server write encryption key and checking the result. */
    dwLen = 12;
    result = CryptEncrypt(hServerWriteKey, 0, TRUE, 0, abData, &dwLen, 16);
    ok (result && (dwLen == 16) && !memcmp(abData, abEncryptedData, 16), "%08lx\n", GetLastError());

    /* Second test case: Test the TLS1 pseudo random number function. */
    result = CryptCreateHash(hProv, CALG_TLS1PRF, hMasterSecret, 0, &hTLS1PRF);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Set the label and seed parameters for the random number function */
    data_blob.cbData = 36;
    data_blob.pbData = abHashedHandshakes;
    result = CryptSetHashParam(hTLS1PRF, HP_TLS1PRF_SEED, (BYTE*)&data_blob, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    data_blob.cbData = 15;
    data_blob.pbData = abClientFinished;
    result = CryptSetHashParam(hTLS1PRF, HP_TLS1PRF_LABEL, (BYTE*)&data_blob, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    /* Generate some pseudo random bytes and check if they are correct. */
    dwLen = (DWORD)sizeof(abData);
    result = CryptGetHashParam(hTLS1PRF, HP_HASHVAL, abData, &dwLen, 0);
    ok (result && (dwLen==(DWORD)sizeof(abData)) && !memcmp(abData, abPRF, sizeof(abData)), 
        "%08lx\n", GetLastError());

    /* Third test case. Derive the server write mac key. Derive an HMAC object from this one.
     * Hash some data with the HMAC. Compare results. */
    result = CryptDeriveKey(hProv, CALG_SCHANNEL_MAC_KEY, hMasterHash, CRYPT_SERVER, &hServerWriteMACKey);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;
    
    result = CryptCreateHash(hProv, CALG_HMAC, hServerWriteMACKey, 0, &hHMAC);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptSetHashParam(hHMAC, HP_HMAC_INFO, (PBYTE)&hmacInfo, 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    result = CryptHashData(hHMAC, abData, (DWORD)sizeof(abData), 0);
    ok (result, "%08lx\n", GetLastError());
    if (!result) return;

    dwLen = (DWORD)sizeof(abMD5Hash);
    result = CryptGetHashParam(hHMAC, HP_HASHVAL, abMD5Hash, &dwLen, 0);
    ok (result && (dwLen == 16) && !memcmp(abMD5Hash, abMD5, 16), "%08lx\n", GetLastError());

    CryptDestroyHash(hHMAC);
    CryptDestroyHash(hTLS1PRF);
    CryptDestroyHash(hMasterHash);
    CryptDestroyKey(hServerWriteMACKey);
    CryptDestroyKey(hServerWriteKey);
    CryptDestroyKey(hRSAKey);
    CryptDestroyKey(hMasterSecret);
    CryptReleaseContext(hProv, 0);
    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_SCHANNEL, CRYPT_DELETEKEYSET);
}

static void test_enum_container(void)
{
    BYTE abContainerName[256];
    DWORD dwBufferLen;
    BOOL result, fFound = FALSE;

    /* If PP_ENUMCONTAINERS is queried with CRYPT_FIRST and abData == NULL, it returns
     * the maximum legal length of container names (which is MAX_PATH + 1 == 261) */
    result = CryptGetProvParam(hProv, PP_ENUMCONTAINERS, NULL, &dwBufferLen, CRYPT_FIRST);
    ok (result && dwBufferLen == MAX_PATH + 1, "%08lx\n", GetLastError());

    /* If the result fits into abContainerName dwBufferLen is left untouched */
    dwBufferLen = (DWORD)sizeof(abContainerName);
    result = CryptGetProvParam(hProv, PP_ENUMCONTAINERS, abContainerName, &dwBufferLen, CRYPT_FIRST);
    ok (result && dwBufferLen == (DWORD)sizeof(abContainerName), "%08lx\n", GetLastError());
    
    /* We only check, if the currently open 'winetest' container is among the enumerated. */
    do {
        if (!strcmp((const char*)abContainerName, "winetest")) fFound = TRUE;
        dwBufferLen = (DWORD)sizeof(abContainerName);
    } while (CryptGetProvParam(hProv, PP_ENUMCONTAINERS, abContainerName, &dwBufferLen, 0));
        
    ok (fFound && GetLastError() == ERROR_NO_MORE_ITEMS, "%d, %08lx\n", fFound, GetLastError());
}

static BYTE signBlob[] = {
0x07,0x02,0x00,0x00,0x00,0x24,0x00,0x00,0x52,0x53,0x41,0x32,0x00,0x02,0x00,0x00,
0x01,0x00,0x01,0x00,0xf1,0x82,0x9e,0x84,0xb5,0x79,0x9a,0xbe,0x4d,0x06,0x20,0x21,
0xb1,0x89,0x0c,0xca,0xb0,0x35,0x72,0x18,0xc6,0x92,0xa8,0xe2,0xb1,0xe1,0xf6,0x56,
0x53,0x99,0x47,0x10,0x6e,0x1c,0x81,0xaf,0xb8,0xf9,0x5f,0xfe,0x76,0x7f,0x2c,0x93,
0xec,0x54,0x7e,0x5e,0xc2,0x25,0x3c,0x50,0x56,0x10,0x20,0x72,0x4a,0x93,0x03,0x12,
0x29,0x98,0xcc,0xc9,0x47,0xbf,0xbf,0x93,0xcc,0xb0,0xe5,0x53,0x14,0xc8,0x7e,0x1f,
0xa4,0x03,0x2d,0x8e,0x84,0x7a,0xd2,0xeb,0xf7,0x92,0x5e,0xa2,0xc7,0x6b,0x35,0x7d,
0xcb,0x60,0xae,0xfb,0x07,0x78,0x11,0x73,0xb5,0x79,0xe5,0x7e,0x96,0xe3,0x50,0x95,
0x80,0x0e,0x1c,0xf6,0x56,0xc6,0xe9,0x0a,0xaf,0x03,0xc6,0xdc,0x9a,0x81,0xcf,0x7a,
0x63,0x16,0x43,0xcd,0xab,0x74,0xa1,0x7d,0xe7,0xe0,0x75,0x6d,0xbd,0x19,0xae,0x0b,
0xa3,0x7f,0x6a,0x7b,0x05,0x4e,0xbc,0xec,0x18,0xfc,0x19,0xc2,0x00,0xf0,0x6a,0x2e,
0xc4,0x31,0x73,0xba,0x07,0xcc,0x9d,0x57,0xeb,0xc7,0x7c,0x00,0x7d,0x5d,0x11,0x16,
0x42,0x4b,0xe5,0x3a,0xf5,0xc7,0xf8,0xee,0xc3,0x2c,0x0d,0x86,0x03,0xe2,0xaf,0xb2,
0xd2,0x91,0xdb,0x71,0xcd,0xdf,0x81,0x5f,0x06,0xfc,0x48,0x0d,0xb6,0x88,0x9f,0xc1,
0x5e,0x24,0xa2,0x05,0x4f,0x30,0x2e,0x8f,0x8b,0x0d,0x76,0xa1,0x84,0xda,0x7b,0x44,
0x70,0x85,0xf1,0x50,0xb1,0x21,0x3d,0xe2,0x57,0x3d,0xd0,0x01,0x93,0x49,0x8e,0xc5,
0x0b,0x8b,0x0d,0x7b,0x08,0xe9,0x14,0xec,0x20,0x0d,0xea,0x02,0x00,0x63,0xe8,0x0a,
0x52,0xe8,0xfb,0x21,0xbd,0x37,0xde,0x4c,0x4d,0xc2,0xf6,0xb9,0x0d,0x2a,0xc3,0xe2,
0xc9,0xdf,0x48,0x3e,0x55,0x3d,0xe3,0xc0,0x22,0x37,0xf9,0x52,0xc0,0xd7,0x61,0x22,
0xb6,0x85,0x86,0x07 };

static void test_null_provider(void)
{
    HCRYPTPROV prov;
    HCRYPTKEY key;
    BOOL result;
    DWORD keySpec, dataLen;

    result = CryptAcquireContext(NULL, szContainer, NULL, 0, 0);
    ok(!result && GetLastError() == NTE_BAD_PROV_TYPE,
     "Expected NTE_BAD_PROV_TYPE, got %08lx\n", GetLastError());
    result = CryptAcquireContext(NULL, szContainer, NULL, PROV_RSA_FULL, 0);
    ok(!result && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    result = CryptAcquireContext(NULL, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    ok(!result && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    ok(!result && GetLastError() == NTE_BAD_KEYSET,
     "Expected NTE_BAD_KEYSET, got %08lx\n", GetLastError());
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL, 0);
    ok(!result && GetLastError() == NTE_BAD_KEYSET,
     "Expected NTE_BAD_KEYSET, got %08lx\n", GetLastError());

    /* Delete the default container. */
    CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    /* Once you've deleted the default container you can't open it as if it
     * already exists.
     */
    result = CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, 0);
    ok(!result && GetLastError() == NTE_BAD_KEYSET,
     "Expected NTE_BAD_KEYSET, got %08lx\n", GetLastError());
    /* But you can always open the default container for CRYPT_VERIFYCONTEXT. */
    result = CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL,
     CRYPT_VERIFYCONTEXT);
    ok(result, "CryptAcquireContext failed: %08lx\n", GetLastError());
    if (!result) return;
    dataLen = sizeof(keySpec);
    result = CryptGetProvParam(prov, PP_KEYSPEC, (LPBYTE)&keySpec, &dataLen, 0);
    if (result)
        ok(keySpec == (AT_KEYEXCHANGE | AT_SIGNATURE),
         "Expected AT_KEYEXCHANGE | AT_SIGNATURE, got %08lx\n", keySpec);
    /* Even though PP_KEYSPEC says both AT_KEYEXCHANGE and AT_SIGNATURE are
     * supported, you can't get the keys from this container.
     */
    result = CryptGetUserKey(prov, AT_KEYEXCHANGE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    result = CryptGetUserKey(prov, AT_SIGNATURE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    result = CryptReleaseContext(prov, 0);
    ok(result, "CryptReleaseContext failed: %08lx\n", GetLastError());
    /* You can create a new default container. */
    result = CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ok(result, "CryptAcquireContext failed: %08lx\n", GetLastError());
    /* But you still can't get the keys (until one's been generated.) */
    result = CryptGetUserKey(prov, AT_KEYEXCHANGE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    result = CryptGetUserKey(prov, AT_SIGNATURE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    CryptReleaseContext(prov, 0);
    CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);

    CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL, 0);
    ok(!result && GetLastError() == NTE_BAD_KEYSET,
     "Expected NTE_BAD_KEYSET, got %08lx\n", GetLastError());
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_VERIFYCONTEXT);
    ok(!result && GetLastError() == NTE_BAD_FLAGS,
     "Expected NTE_BAD_FLAGS, got %08lx\n", GetLastError());
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ok(result, "CryptAcquireContext failed: %08lx\n", GetLastError());
    if (!result) return;
    dataLen = sizeof(keySpec);
    result = CryptGetProvParam(prov, PP_KEYSPEC, (LPBYTE)&keySpec, &dataLen, 0);
    if (result)
        ok(keySpec == (AT_KEYEXCHANGE | AT_SIGNATURE),
         "Expected AT_KEYEXCHANGE | AT_SIGNATURE, got %08lx\n", keySpec);
    result = CryptGetUserKey(prov, AT_KEYEXCHANGE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    result = CryptGetUserKey(prov, AT_SIGNATURE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());

    /* Importing a key exchange blob.. */
    result = CryptImportKey(prov, abPlainPrivateKey, sizeof(abPlainPrivateKey),
     0, 0, &key);
    ok(result, "CryptImportKey failed: %08lx\n", GetLastError());
    CryptDestroyKey(key);
    /* allows access to the key exchange key.. */
    result = CryptGetUserKey(prov, AT_KEYEXCHANGE, &key);
    ok(result, "CryptGetUserKey failed: %08lx\n", GetLastError());
    CryptDestroyKey(key);
    /* but not to the private key. */
    result = CryptGetUserKey(prov, AT_SIGNATURE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    CryptReleaseContext(prov, 0);
    CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);

    /* Whereas importing a sign blob.. */
    result = CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);
    ok(result, "CryptAcquireContext failed: %08lx\n", GetLastError());
    if (!result) return;
    result = CryptImportKey(prov, signBlob, sizeof(signBlob), 0, 0, &key);
    ok(result, "CryptGenKey failed: %08lx\n", GetLastError());
    /* doesn't allow access to the key exchange key.. */
    result = CryptGetUserKey(prov, AT_KEYEXCHANGE, &key);
    ok(!result && GetLastError() == NTE_NO_KEY,
     "Expected NTE_NO_KEY, got %08lx\n", GetLastError());
    /* but does to the private key. */
    result = CryptGetUserKey(prov, AT_SIGNATURE, &key);
    ok(result, "CryptGetUserKey failed: %08lx\n", GetLastError());
    CryptDestroyKey(key);

    CryptAcquireContext(&prov, szContainer, NULL, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
}

START_TEST(rsaenh)
{
    if (!init_environment()) 
        return;
    test_prov();
    test_gen_random();
    test_hashes();
    test_rc4();
    test_rc2();
    test_des();
    test_3des112();
    test_3des();
    test_hmac();
    test_mac();
    test_block_cipher_modes();
    test_import_private();
    test_verify_signature();
    test_rsa_encrypt();
    test_enum_container();
    clean_up_environment();
    test_schannel_provider();
    test_null_provider();
}
