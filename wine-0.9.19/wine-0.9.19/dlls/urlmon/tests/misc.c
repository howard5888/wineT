/*
 * Copyright 2005-2006 Jacek Caban for CodeWeavers
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

#define COBJMACROS

#include <wine/test.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "ole2.h"
#include "urlmon.h"

#include "initguid.h"

DEFINE_GUID(CLSID_AboutProtocol, 0x3050F406, 0x98B5, 0x11CF, 0xBB,0x82, 0x00,0xAA,0x00,0xBD,0xCE,0x0B);

#define DEFINE_EXPECT(func) \
    static BOOL expect_ ## func = FALSE, called_ ## func = FALSE

#define SET_EXPECT(func) \
    expect_ ## func = TRUE

#define CHECK_EXPECT(func) \
    do { \
        ok(expect_ ##func, "unexpected call " #func "\n"); \
        expect_ ## func = FALSE; \
        called_ ## func = TRUE; \
    }while(0)

#define CHECK_EXPECT2(func) \
    do { \
        ok(expect_ ##func, "unexpected call " #func "\n"); \
        called_ ## func = TRUE; \
    }while(0)

#define CHECK_CALLED(func) \
    do { \
        ok(called_ ## func, "expected " #func "\n"); \
        expect_ ## func = called_ ## func = FALSE; \
    }while(0)

DEFINE_EXPECT(ParseUrl);
DEFINE_EXPECT(QI_IInternetProtocolInfo);
DEFINE_EXPECT(CreateInstance);
DEFINE_EXPECT(unk_Release);

static void test_CreateFormatEnum(void)
{
    IEnumFORMATETC *fenum = NULL, *fenum2 = NULL;
    FORMATETC fetc[5];
    ULONG ul;
    HRESULT hres;

    static DVTARGETDEVICE dev = {sizeof(dev),0,0,0,0,{0}};
    static FORMATETC formatetc[] = {
        {0,&dev,0,0,0},
        {0,&dev,0,1,0},
        {0,NULL,0,2,0},
        {0,NULL,0,3,0},
        {0,NULL,0,4,0}
    };

    hres = CreateFormatEnumerator(0, formatetc, &fenum);
    ok(hres == E_FAIL, "CreateFormatEnumerator failed: %08lx, expected E_FAIL\n", hres);
    hres = CreateFormatEnumerator(0, formatetc, NULL);
    ok(hres == E_INVALIDARG, "CreateFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = CreateFormatEnumerator(5, formatetc, NULL);
    ok(hres == E_INVALIDARG, "CreateFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);


    hres = CreateFormatEnumerator(5, formatetc, &fenum);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = IEnumFORMATETC_Next(fenum, 2, NULL, &ul);
    ok(hres == E_INVALIDARG, "Next failed: %08lx, expected E_INVALIDARG\n", hres);
    ul = 100;
    hres = IEnumFORMATETC_Next(fenum, 0, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);

    hres = IEnumFORMATETC_Next(fenum, 2, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 0, "fetc[0].lindex=%ld, expected 0\n", fetc[0].lindex);
    ok(fetc[1].lindex == 1, "fetc[1].lindex=%ld, expected 1\n", fetc[1].lindex);
    ok(fetc[0].ptd == &dev, "fetc[0].ptd=%p, expected %p\n", fetc[0].ptd, &dev);
    ok(ul == 2, "ul=%ld, expected 2\n", ul);

    hres = IEnumFORMATETC_Skip(fenum, 1);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Next(fenum, 4, fetc, &ul);
    ok(hres == S_FALSE, "Next failed: %08lx, expected S_FALSE\n", hres);
    ok(fetc[0].lindex == 3, "fetc[0].lindex=%ld, expected 3\n", fetc[0].lindex);
    ok(fetc[1].lindex == 4, "fetc[1].lindex=%ld, expected 4\n", fetc[1].lindex);
    ok(fetc[0].ptd == NULL, "fetc[0].ptd=%p, expected NULL\n", fetc[0].ptd);
    ok(ul == 2, "ul=%ld, expected 2\n", ul);

    hres = IEnumFORMATETC_Next(fenum, 4, fetc, &ul);
    ok(hres == S_FALSE, "Next failed: %08lx, expected S_FALSE\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);
    ul = 100;
    hres = IEnumFORMATETC_Next(fenum, 0, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);

    hres = IEnumFORMATETC_Skip(fenum, 3);
    ok(hres == S_FALSE, "Skip failed: %08lx, expected S_FALSE\n", hres);

    hres = IEnumFORMATETC_Reset(fenum);
    ok(hres == S_OK, "Reset failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Next(fenum, 5, fetc, NULL);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 0, "fetc[0].lindex=%ld, expected 0\n", fetc[0].lindex);

    hres = IEnumFORMATETC_Reset(fenum);
    ok(hres == S_OK, "Reset failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Skip(fenum, 2);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Clone(fenum, NULL);
    ok(hres == E_INVALIDARG, "Clone failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IEnumFORMATETC_Clone(fenum, &fenum2);
    ok(hres == S_OK, "Clone failed: %08lx\n", hres);

    if(SUCCEEDED(hres)) {
        ok(fenum != fenum2, "fenum == fenum2\n");

        hres = IEnumFORMATETC_Next(fenum2, 2, fetc, &ul);
        ok(hres == S_OK, "Next failed: %08lx\n", hres);
        ok(fetc[0].lindex == 2, "fetc[0].lindex=%ld, expected 2\n", fetc[0].lindex);

        IEnumFORMATETC_Release(fenum2);
    }

    hres = IEnumFORMATETC_Next(fenum, 2, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 2, "fetc[0].lindex=%ld, expected 2\n", fetc[0].lindex);

    hres = IEnumFORMATETC_Skip(fenum, 1);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);
    
    IEnumFORMATETC_Release(fenum);
}

static void test_RegisterFormatEnumerator(void)
{
    IBindCtx *bctx = NULL;
    IEnumFORMATETC *format = NULL, *format2 = NULL;
    IUnknown *unk = NULL;
    HRESULT hres;

    static FORMATETC formatetc = {0,NULL,0,0,0};
    static WCHAR wszEnumFORMATETC[] =
        {'_','E','n','u','m','F','O','R','M','A','T','E','T','C','_',0};

    CreateBindCtx(0, &bctx);

    hres = CreateFormatEnumerator(1, &formatetc, &format);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = RegisterFormatEnumerator(NULL, format, 0);
    ok(hres == E_INVALIDARG,
            "RegisterFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = RegisterFormatEnumerator(bctx, NULL, 0);
    ok(hres == E_INVALIDARG,
            "RegisterFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == S_OK, "GetObjectParam failed: %08lx\n", hres);
    ok(unk == (IUnknown*)format, "unk != format\n");

    hres = RevokeFormatEnumerator(NULL, format);
    ok(hres == E_INVALIDARG,
            "RevokeFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = RevokeFormatEnumerator(bctx, format);
    ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);

    hres = RevokeFormatEnumerator(bctx, format);
    ok(hres == E_FAIL, "RevokeFormatEnumerator failed: %08lx, expected E_FAIL\n", hres);

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);

    hres = CreateFormatEnumerator(1, &formatetc, &format2);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);

    if(SUCCEEDED(hres)) {
        hres = RevokeFormatEnumerator(bctx, format);
        ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);

        IEnumFORMATETC_Release(format2);
    }

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    IEnumFORMATETC_Release(format);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);
    hres = RevokeFormatEnumerator(bctx, NULL);
    ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);
    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    IBindCtx_Release(bctx);
}

static const WCHAR url1[] = {'r','e','s',':','/','/','m','s','h','t','m','l','.','d','l','l',
        '/','b','l','a','n','k','.','h','t','m',0};
static const WCHAR url2[] = {'i','n','d','e','x','.','h','t','m',0};
static const WCHAR url3[] = {'f','i','l','e',':','c',':','\\','I','n','d','e','x','.','h','t','m',0};
static const WCHAR url4[] = {'f','i','l','e',':','s','o','m','e','%','2','0','f','i','l','e',
        '%','2','e','j','p','g',0};
static const WCHAR url5[] = {'h','t','t','p',':','/','/','w','w','w','.','w','i','n','e','h','q',
        '.','o','r','g',0};
static const WCHAR url6[] = {'a','b','o','u','t',':','b','l','a','n','k',0};
static const WCHAR url7[] = {'f','t','p',':','/','/','w','i','n','e','h','q','.','o','r','g','/',
        'f','i','l','e','.','t','e','s','t',0};
static const WCHAR url8[] = {'t','e','s','t',':','1','2','3','a','b','c',0};


static const WCHAR url4e[] = {'f','i','l','e',':','s','o','m','e',' ','f','i','l','e',
        '.','j','p','g',0};

static const WCHAR path3[] = {'c',':','\\','I','n','d','e','x','.','h','t','m',0};
static const WCHAR path4[] = {'s','o','m','e',' ','f','i','l','e','.','j','p','g',0};

static const WCHAR wszRes[] = {'r','e','s',0};
static const WCHAR wszFile[] = {'f','i','l','e',0};
static const WCHAR wszHttp[] = {'h','t','t','p',0};
static const WCHAR wszAbout[] = {'a','b','o','u','t',0};
static const WCHAR wszEmpty[] = {0};

struct parse_test {
    LPCWSTR url;
    HRESULT secur_hres;
    LPCWSTR encoded_url;
    HRESULT path_hres;
    LPCWSTR path;
    LPCWSTR schema;
};

static const struct parse_test parse_tests[] = {
    {url1, S_OK,   url1,  E_INVALIDARG, NULL, wszRes},
    {url2, E_FAIL, url2,  E_INVALIDARG, NULL, wszEmpty},
    {url3, E_FAIL, url3,  S_OK, path3,        wszFile},
    {url4, E_FAIL, url4e, S_OK, path4,        wszFile},
    {url5, E_FAIL, url5,  E_INVALIDARG, NULL, wszHttp},
    {url6, S_OK,   url6,  E_INVALIDARG, NULL, wszAbout}
};

static void test_CoInternetParseUrl(void)
{
    HRESULT hres;
    DWORD size;
    int i;

    static WCHAR buf[4096];

    memset(buf, 0xf0, sizeof(buf));
    hres = CoInternetParseUrl(parse_tests[0].url, PARSE_SCHEMA, 0, buf,
            3, &size, 0);
    ok(hres == E_POINTER, "schema failed: %08lx, expected E_POINTER\n", hres);

    for(i=0; i < sizeof(parse_tests)/sizeof(parse_tests[0]); i++) {
        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_SECURITY_URL, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == parse_tests[i].secur_hres, "[%d] security url failed: %08lx, expected %08lx\n",
                i, hres, parse_tests[i].secur_hres);

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_ENCODE, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == S_OK, "[%d] encoding failed: %08lx\n", i, hres);
        ok(size == lstrlenW(parse_tests[i].encoded_url), "[%d] wrong size\n", i);
        ok(!lstrcmpW(parse_tests[i].encoded_url, buf), "[%d] wrong encoded url\n", i);

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_PATH_FROM_URL, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == parse_tests[i].path_hres, "[%d] path failed: %08lx, expected %08lx\n",
                i, hres, parse_tests[i].path_hres);
        if(parse_tests[i].path) {
            ok(size == lstrlenW(parse_tests[i].path), "[%d] wrong size\n", i);
            ok(!lstrcmpW(parse_tests[i].path, buf), "[%d] wrong path\n", i);
        }

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_SCHEMA, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == S_OK, "[%d] schema failed: %08lx\n", i, hres);
        ok(size == lstrlenW(parse_tests[i].schema), "[%d] wrong size\n", i);
        ok(!lstrcmpW(parse_tests[i].schema, buf), "[%d] wrong schema\n", i);
    }
}

static const WCHAR mimeTextHtml[] = {'t','e','x','t','/','h','t','m','l',0};
static const WCHAR mimeTextPlain[] = {'t','e','x','t','/','p','l','a','i','n',0};
static const WCHAR mimeAppOctetStream[] = {'a','p','p','l','i','c','a','t','i','o','n','/',
    'o','c','t','e','t','-','s','t','r','e','a','m',0};
static const WCHAR mimeImagePjpeg[] = {'i','m','a','g','e','/','p','j','p','e','g',0};
static const WCHAR mimeImageGif[] = {'i','m','a','g','e','/','g','i','f',0};
static const WCHAR mimeImageBmp[] = {'i','m','a','g','e','/','b','m','p',0};
static const WCHAR mimeImageXPng[] = {'i','m','a','g','e','/','x','-','p','n','g',0};

static const struct {
    LPCWSTR url;
    LPCWSTR mime;
} mime_tests[] = {
    {url1, mimeTextHtml},
    {url2, mimeTextHtml},
    {url3, mimeTextHtml},
    {url4, NULL},
    {url5, NULL},
    {url6, NULL},
    {url7, NULL}
};

static BYTE data1[] = "test data\n";
static BYTE data2[] = {31,'t','e','s',0xfa,'t',' ','d','a','t','a','\n',0};
static BYTE data3[] = {0,0,0};
static BYTE data4[] = {'t','e','s',0xfa,'t',' ','d','a','t','a','\n',0,0};
static BYTE data5[] = {0xa,0xa,0xa,'x',32,'x',0};
static BYTE data6[] = {0xfa,0xfa,0xfa,0xfa,'\n','\r','\t','x','x','x',1};
static BYTE data7[] = "<html>blahblah";
static BYTE data8[] = {'t','e','s',0xfa,'t',' ','<','h','t','m','l','>','d','a','t','a','\n',0,0};
static BYTE data9[] = {'t','e',0,'s',0xfa,'t',' ','<','h','t','m','l','>','d','a','t','a','\n',0,0};
static BYTE data10[] = "<HtmL>blahblah";
static BYTE data11[] = "blah<HTML>blahblah";
static BYTE data12[] = "blah<HTMLblahblah";
static BYTE data13[] = "blahHTML>blahblah";
static BYTE data14[] = "blah<HTMblahblah";
static BYTE data15[] = {0xff,0xd8};
static BYTE data16[] = {0xff,0xd8,'h'};
static BYTE data17[] = {0,0xff,0xd8};
static BYTE data18[] = {0xff,0xd8,'<','h','t','m','l','>'};
static BYTE data19[] = {'G','I','F','8','7','a'};
static BYTE data20[] = {'G','I','F','8','9','a'};
static BYTE data21[] = {'G','I','F','8','7'};
static BYTE data22[] = {'G','i','F','8','7','a'};
static BYTE data23[] = {'G','i','F','8','8','a'};
static BYTE data24[] = {'g','i','f','8','7','a'};
static BYTE data25[] = {'G','i','F','8','7','A'};
static BYTE data26[] = {'G','i','F','8','7','a','<','h','t','m','l','>'};
static BYTE data27[] = {0x30,'G','i','F','8','7','A'};
static BYTE data28[] = {0x42,0x4d,0x6e,0x42,0x1c,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00};
static BYTE data29[] = {0x42,0x4d,'x','x','x','x',0x00,0x00,0x00,0x00,'x','x','x','x'};
static BYTE data30[] = {0x42,0x4d,'x','x','x','x',0x00,0x01,0x00,0x00,'x','x','x','x'};
static BYTE data31[] = {0x42,0x4d,'x','x','x','x',0x00,0x00,0x00,0x00,'<','h','t','m','l','>'};
static BYTE data32[] = {0x42,0x4d,'x','x','x','x',0x00,0x00,0x00,0x00,'x','x','x'};
static BYTE data33[] = {0x00,0x42,0x4d,'x','x','x','x',0x00,0x00,0x00,0x00,'x','x','x'};
static BYTE data34[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a,'x'};
static BYTE data35[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a,'x','x','x','x',0};
static BYTE data36[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,'x','x'};
static BYTE data37[] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a,'<','h','t','m','l','>'};
static BYTE data38[] = {0x00,0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a,'x'};

static const struct {
    BYTE *data;
    DWORD size;
    LPCWSTR mime;
} mime_tests2[] = {
    {data1, sizeof(data1), mimeTextPlain},
    {data2, sizeof(data2), mimeAppOctetStream},
    {data3, sizeof(data3), mimeAppOctetStream},
    {data4, sizeof(data4), mimeAppOctetStream},
    {data5, sizeof(data5), mimeTextPlain},
    {data6, sizeof(data6), mimeTextPlain},
    {data7, sizeof(data7), mimeTextHtml},
    {data8, sizeof(data8), mimeTextHtml},
    {data9, sizeof(data9), mimeTextHtml},
    {data10, sizeof(data10), mimeTextHtml},
    {data11, sizeof(data11), mimeTextHtml},
    {data12, sizeof(data12), mimeTextHtml},
    {data13, sizeof(data13), mimeTextPlain},
    {data14, sizeof(data14), mimeTextPlain},
    {data15, sizeof(data15), mimeTextPlain},
    {data16, sizeof(data16), mimeImagePjpeg},
    {data17, sizeof(data17), mimeAppOctetStream},
    {data18, sizeof(data18), mimeTextHtml},
    {data19, sizeof(data19), mimeImageGif},
    {data20, sizeof(data20), mimeImageGif},
    {data21, sizeof(data21), mimeTextPlain},
    {data22, sizeof(data22), mimeImageGif},
    {data23, sizeof(data23), mimeTextPlain},
    {data24, sizeof(data24), mimeImageGif},
    {data25, sizeof(data25), mimeImageGif},
    {data26, sizeof(data26), mimeTextHtml},
    {data27, sizeof(data27), mimeTextPlain},
    {data28, sizeof(data28), mimeImageBmp},
    {data29, sizeof(data29), mimeImageBmp},
    {data30, sizeof(data30), mimeAppOctetStream},
    {data31, sizeof(data31), mimeTextHtml},
    {data32, sizeof(data32), mimeAppOctetStream},
    {data33, sizeof(data33), mimeAppOctetStream},
    {data34, sizeof(data34), mimeImageXPng},
    {data35, sizeof(data35), mimeImageXPng},
    {data36, sizeof(data36), mimeAppOctetStream},
    {data37, sizeof(data37), mimeTextHtml},
    {data38, sizeof(data38), mimeAppOctetStream}
};

static void test_FindMimeFromData(void)
{
    HRESULT hres;
    LPWSTR mime;
    int i;

    for(i=0; i<sizeof(mime_tests)/sizeof(mime_tests[0]); i++) {
        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, NULL, 0, &mime, 0);
        if(mime_tests[i].mime) {
            ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
            ok(!lstrcmpW(mime, mime_tests[i].mime), "[%d] wrong mime\n", i);
            CoTaskMemFree(mime);
        }else {
            ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);
            ok(mime == (LPWSTR)0xf0f0f0f0, "[%d] mime != 0xf0f0f0f0\n", i);
        }

        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, mimeTextPlain, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mimeTextPlain), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);

        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, mimeAppOctetStream, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mimeAppOctetStream), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);
    }

    for(i=0; i < sizeof(mime_tests2)/sizeof(mime_tests2[0]); i++) {
        hres = FindMimeFromData(NULL, NULL, mime_tests2[i].data, mime_tests2[i].size,
                NULL, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mime_tests2[i].mime), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);

        hres = FindMimeFromData(NULL, NULL, mime_tests2[i].data, mime_tests2[i].size,
                mimeTextHtml, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        if(!lstrcmpW(mimeAppOctetStream, mime_tests2[i].mime)
           || !lstrcmpW(mimeTextPlain, mime_tests2[i].mime))
            ok(!lstrcmpW(mime, mimeTextHtml), "[%d] wrong mime\n", i);
        else
            ok(!lstrcmpW(mime, mime_tests2[i].mime), "[%d] wrong mime\n", i);

        hres = FindMimeFromData(NULL, NULL, mime_tests2[i].data, mime_tests2[i].size,
                mimeImagePjpeg, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        if(!lstrcmpW(mimeAppOctetStream, mime_tests2[i].mime) || i == 17)
            ok(!lstrcmpW(mime, mimeImagePjpeg), "[%d] wrong mime\n", i);
        else
            ok(!lstrcmpW(mime, mime_tests2[i].mime), "[%d] wrong mime\n", i);

        CoTaskMemFree(mime);
    }

    hres = FindMimeFromData(NULL, url1, data1, sizeof(data1), NULL, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, url1, data1, sizeof(data1), mimeAppOctetStream, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, url4, data1, sizeof(data1), mimeAppOctetStream, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, NULL, NULL, 0, NULL, 0, &mime, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, excepted E_INVALIDARG\n", hres);

    hres = FindMimeFromData(NULL, NULL, NULL, 0, mimeTextPlain, 0, &mime, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = FindMimeFromData(NULL, NULL, data1, 0, NULL, 0, &mime, 0);
    ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);

    hres = FindMimeFromData(NULL, url1, data1, 0, NULL, 0, &mime, 0);
    ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);

    hres = FindMimeFromData(NULL, NULL, data1, 0, mimeTextPlain, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, NULL, data1, 0, mimeTextPlain, 0, NULL, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, expected E_INVALIDARG\n", hres);
}

static const BYTE secid1[] = {'f','i','l','e',':',0,0,0,0};
static const BYTE secid4[] ={'f','i','l','e',':',3,0,0,0};
static const BYTE secid5[] = {'h','t','t','p',':','w','w','w','.','w','i','n','e','h','q',
    '.','o','r','g',3,0,0,0};
static const BYTE secid6[] = {'a','b','o','u','t',':','b','l','a','n','k',3,0,0,0};
static const BYTE secid7[] = {'f','t','p',':','w','i','n','e','h','q','.','o','r','g',
                              3,0,0,0};

static struct secmgr_test {
    LPCWSTR url;
    DWORD zone;
    HRESULT zone_hres;
    DWORD secid_size;
    const BYTE *secid;
    HRESULT secid_hres;
} secmgr_tests[] = {
    {url1, 0,   S_OK, sizeof(secid1), secid1, S_OK},
    {url2, 100, 0x80041001, 0, NULL, E_INVALIDARG},
    {url3, 0,   S_OK, sizeof(secid1), secid1, S_OK},
    {url4, 3,   S_OK, sizeof(secid4), secid4, S_OK},
    {url5, 3,   S_OK, sizeof(secid5), secid5, S_OK},
    {url6, 3,   S_OK, sizeof(secid6), secid6, S_OK},
    {url7, 3,   S_OK, sizeof(secid7), secid7, S_OK}
};

static void test_SecurityManager(void)
{
    int i;
    IInternetSecurityManager *secmgr = NULL;
    BYTE buf[512];
    DWORD zone, size;
    HRESULT hres;

    hres = CoInternetCreateSecurityManager(NULL, &secmgr, 0);
    ok(hres == S_OK, "CoInternetCreateSecurityManager failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    for(i=0; i < sizeof(secmgr_tests)/sizeof(secmgr_tests[0]); i++) {
        zone = 100;
        hres = IInternetSecurityManager_MapUrlToZone(secmgr, secmgr_tests[i].url,
                                                     &zone, 0);
        ok(hres == secmgr_tests[i].zone_hres,
           "[%d] MapUrlToZone failed: %08lx, expected %08lx\n",
                i, hres, secmgr_tests[i].zone_hres);
        ok(zone == secmgr_tests[i].zone, "[%d] zone=%ld, expected %ld\n", i, zone,
                secmgr_tests[i].zone);

        size = sizeof(buf);
        memset(buf, 0xf0, sizeof(buf));
        hres = IInternetSecurityManager_GetSecurityId(secmgr, secmgr_tests[i].url,
                buf, &size, 0);
        ok(hres == secmgr_tests[i].secid_hres,
           "[%d] GetSecurityId failed: %08lx, expected %08lx\n",
           i, hres, secmgr_tests[i].secid_hres);
        if(secmgr_tests[i].secid) {
            ok(size == secmgr_tests[i].secid_size, "[%d] size=%ld, expected %ld\n",
                    i, size, secmgr_tests[i].secid_size);
            ok(!memcmp(buf, secmgr_tests[i].secid, size), "[%d] wrong secid\n", i);
        }
    }

    zone = 100;
    hres = IInternetSecurityManager_MapUrlToZone(secmgr, NULL, &zone, 0);
    ok(hres == E_INVALIDARG, "MapUrlToZone failed: %08lx, expected E_INVALIDARG\n", hres);

    size = sizeof(buf);
    hres = IInternetSecurityManager_GetSecurityId(secmgr, NULL, buf, &size, 0);
    ok(hres == E_INVALIDARG,
       "GetSecurityId failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = IInternetSecurityManager_GetSecurityId(secmgr, secmgr_tests[1].url,
                                                  NULL, &size, 0);
    ok(hres == E_INVALIDARG,
       "GetSecurityId failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = IInternetSecurityManager_GetSecurityId(secmgr, secmgr_tests[1].url,
                                                  buf, NULL, 0);
    ok(hres == E_INVALIDARG,
       "GetSecurityId failed: %08lx, expected E_INVALIDARG\n", hres);

    IInternetSecurityManager_Release(secmgr);
}

static void test_ZoneManager(void)
{
    IInternetZoneManager *zonemgr = NULL;
    BYTE buf[32];
    HRESULT hres;

    hres = CoInternetCreateZoneManager(NULL, &zonemgr, 0);
    ok(hres == S_OK, "CoInternetCreateZoneManager failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == S_OK, "GetZoneActionPolicy failed: %08lx\n", hres);
    ok(*(DWORD*)buf == 1, "policy=%ld, expected 1\n", *(DWORD*)buf);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, NULL,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, buf,
            2, URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1fff, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_FAIL, "GetZoneActionPolicy failed: %08lx, expected E_FAIL\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 13, 0x1a10, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    IInternetZoneManager_Release(zonemgr);
}

static void register_protocols(void)
{
    IInternetSession *session;
    IClassFactory *factory;
    HRESULT hres;

    static const WCHAR wszAbout[] = {'a','b','o','u','t',0};

    hres = CoInternetGetSession(0, &session, 0);
    ok(hres == S_OK, "CoInternetGetSession failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = CoGetClassObject(&CLSID_AboutProtocol, CLSCTX_INPROC_SERVER, NULL,
            &IID_IClassFactory, (void**)&factory);
    ok(hres == S_OK, "Coud not get AboutProtocol factory: %08lx\n", hres);
    if(FAILED(hres))
        return;

    IInternetSession_RegisterNameSpace(session, factory, &CLSID_AboutProtocol,
                                       wszAbout, 0, NULL, 0);
    IClassFactory_Release(factory);

}

static HRESULT WINAPI InternetProtocolInfo_QueryInterface(IInternetProtocolInfo *iface,
                                                          REFIID riid, void **ppv)
{
    ok(0, "unexpected call\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI InternetProtocolInfo_AddRef(IInternetProtocolInfo *iface)
{
    return 2;
}

static ULONG WINAPI InternetProtocolInfo_Release(IInternetProtocolInfo *iface)
{
    return 1;
}

static HRESULT WINAPI InternetProtocolInfo_ParseUrl(IInternetProtocolInfo *iface, LPCWSTR pwzUrl,
        PARSEACTION ParseAction, DWORD dwParseFlags, LPWSTR pwzResult, DWORD cchResult,
        DWORD *pcchResult, DWORD dwReserved)
{
    CHECK_EXPECT(ParseUrl);
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetProtocolInfo_CombineUrl(IInternetProtocolInfo *iface,
        LPCWSTR pwzBaseUrl, LPCWSTR pwzRelativeUrl, DWORD dwCombineFlags,
        LPWSTR pwzResult, DWORD cchResult, DWORD *pcchResult, DWORD dwReserved)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetProtocolInfo_CompareUrl(IInternetProtocolInfo *iface,
        LPCWSTR pwzUrl1, LPCWSTR pwzUrl2, DWORD dwCompareFlags)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI InternetProtocolInfo_QueryInfo(IInternetProtocolInfo *iface,
        LPCWSTR pwzUrl, QUERYOPTION OueryOption, DWORD dwQueryFlags, LPVOID pBuffer,
        DWORD cbBuffer, DWORD *pcbBuf, DWORD dwReserved)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const IInternetProtocolInfoVtbl InternetProtocolInfoVtbl = {
    InternetProtocolInfo_QueryInterface,
    InternetProtocolInfo_AddRef,
    InternetProtocolInfo_Release,
    InternetProtocolInfo_ParseUrl,
    InternetProtocolInfo_CombineUrl,
    InternetProtocolInfo_CompareUrl,
    InternetProtocolInfo_QueryInfo
};

static IInternetProtocolInfo protocol_info = { &InternetProtocolInfoVtbl };

static HRESULT qiret;
static IClassFactory *expect_cf;

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
    if(IsEqualGUID(&IID_IInternetProtocolInfo, riid)) {
        CHECK_EXPECT(QI_IInternetProtocolInfo);
        ok(iface == expect_cf, "unexpected iface\n");
        *ppv = &protocol_info;
        return qiret;
    }

    ok(0, "unexpected call\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                        REFIID riid, void **ppv)
{
    CHECK_EXPECT(CreateInstance);

    ok(iface == expect_cf, "unexpected iface\n");
    ok(pOuter == NULL, "pOuter = %p\n", pOuter);
    ok(IsEqualGUID(&IID_IInternetProtocolInfo, riid), "unexpected riid\n");
    ok(ppv != NULL, "ppv == NULL\n");

    *ppv = &protocol_info;
    return S_OK;
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL dolock)
{
    ok(0, "unexpected call\n");
    return S_OK;
}

static const IClassFactoryVtbl ClassFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactory test_protocol_cf = { &ClassFactoryVtbl };
static IClassFactory test_protocol_cf2 = { &ClassFactoryVtbl };

static void test_NameSpace(void)
{
    IInternetSession *session;
    WCHAR buf[200];
    DWORD size;
    HRESULT hres;

    static const WCHAR wszTest[] = {'t','e','s','t',0};

    hres = CoInternetGetSession(0, &session, 0);
    ok(hres == S_OK, "CoInternetGetSession failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = IInternetSession_RegisterNameSpace(session, NULL, &IID_NULL,
                                              wszTest, 0, NULL, 0);
    ok(hres == E_INVALIDARG, "RegisterNameSpace failed: %08lx\n", hres);

    hres = IInternetSession_RegisterNameSpace(session, &test_protocol_cf, &IID_NULL,
                                              NULL, 0, NULL, 0);
    ok(hres == E_INVALIDARG, "RegisterNameSpace failed: %08lx\n", hres);

    hres = IInternetSession_RegisterNameSpace(session, &test_protocol_cf, &IID_NULL,
                                              wszTest, 0, NULL, 0);
    ok(hres == S_OK, "RegisterNameSpace failed: %08lx\n", hres);

    qiret = E_NOINTERFACE;
    expect_cf = &test_protocol_cf;
    SET_EXPECT(QI_IInternetProtocolInfo);
    SET_EXPECT(CreateInstance);
    SET_EXPECT(ParseUrl);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    CHECK_CALLED(QI_IInternetProtocolInfo);
    CHECK_CALLED(CreateInstance);
    CHECK_CALLED(ParseUrl);

    qiret = S_OK;
    SET_EXPECT(QI_IInternetProtocolInfo);
    SET_EXPECT(ParseUrl);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    CHECK_CALLED(QI_IInternetProtocolInfo);
    CHECK_CALLED(ParseUrl);

    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    hres = IInternetSession_RegisterNameSpace(session, &test_protocol_cf2, &IID_NULL,
                                              wszTest, 0, NULL, 0);
    ok(hres == S_OK, "RegisterNameSpace failed: %08lx\n", hres);

    hres = IInternetSession_RegisterNameSpace(session, &test_protocol_cf, &IID_NULL,
                                              wszTest, 0, NULL, 0);
    ok(hres == S_OK, "RegisterNameSpace failed: %08lx\n", hres);

    hres = IInternetSession_RegisterNameSpace(session, &test_protocol_cf, &IID_NULL,
                                              wszTest, 0, NULL, 0);
    ok(hres == S_OK, "RegisterNameSpace failed: %08lx\n", hres);

    SET_EXPECT(QI_IInternetProtocolInfo);
    SET_EXPECT(ParseUrl);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    CHECK_CALLED(QI_IInternetProtocolInfo);
    CHECK_CALLED(ParseUrl);

    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);

    SET_EXPECT(QI_IInternetProtocolInfo);
    SET_EXPECT(ParseUrl);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    CHECK_CALLED(QI_IInternetProtocolInfo);
    CHECK_CALLED(ParseUrl);

    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);

    expect_cf = &test_protocol_cf2;
    SET_EXPECT(QI_IInternetProtocolInfo);
    SET_EXPECT(ParseUrl);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    CHECK_CALLED(QI_IInternetProtocolInfo);
    CHECK_CALLED(ParseUrl);

    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);
    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);
    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf, NULL);
    ok(hres == E_INVALIDARG, "UnregisterNameSpace failed: %08lx\n", hres);
    hres = IInternetSession_UnregisterNameSpace(session, NULL, wszTest);
    ok(hres == E_INVALIDARG, "UnregisterNameSpace failed: %08lx\n", hres);

    hres = IInternetSession_UnregisterNameSpace(session, &test_protocol_cf2, wszTest);
    ok(hres == S_OK, "UnregisterNameSpace failed: %08lx\n", hres);

    hres = CoInternetParseUrl(url8, PARSE_ENCODE, 0, buf, sizeof(buf)/sizeof(WCHAR),
                              &size, 0);
    ok(hres == S_OK, "CoInternetParseUrl failed: %08lx\n", hres);

    IInternetSession_Release(session);
}

static ULONG WINAPI unk_Release(IUnknown *iface)
{
    CHECK_EXPECT(unk_Release);
    return 0;
}

static const IUnknownVtbl unk_vtbl = {
    (void*)0xdeadbeef,
    (void*)0xdeadbeef,
    unk_Release
};

static void test_ReleaseBindInfo(void)
{
    BINDINFO bi;
    IUnknown unk = { &unk_vtbl };

    ReleaseBindInfo(NULL); /* shouldn't crash */

    memset(&bi, 0, sizeof(bi));
    bi.cbSize = sizeof(BINDINFO);
    bi.pUnk = &unk;
    SET_EXPECT(unk_Release);
    ReleaseBindInfo(&bi);
    ok(bi.cbSize == sizeof(BINDINFO), "bi.cbSize=%ld\n", bi.cbSize);
    ok(bi.pUnk == NULL, "bi.pUnk=%p, expected NULL\n", bi.pUnk);
    CHECK_CALLED(unk_Release);

    memset(&bi, 0, sizeof(bi));
    bi.cbSize = offsetof(BINDINFO, pUnk);
    bi.pUnk = &unk;
    ReleaseBindInfo(&bi);
    ok(bi.cbSize == offsetof(BINDINFO, pUnk), "bi.cbSize=%ld\n", bi.cbSize);
    ok(bi.pUnk == &unk, "bi.pUnk=%p, expected %p\n", bi.pUnk, &unk);

    memset(&bi, 0, sizeof(bi));
    bi.pUnk = &unk;
    ReleaseBindInfo(&bi);
    ok(!bi.cbSize, "bi.cbSize=%ld, expected 0\n", bi.cbSize);
    ok(bi.pUnk == &unk, "bi.pUnk=%p, expected %p\n", bi.pUnk, &unk);
}

static void test_UrlMkGetSessionOption(void)
{
    DWORD encoding, size;
    HRESULT hres;

    size = encoding = 0xdeadbeef;
    hres = UrlMkGetSessionOption(URLMON_OPTION_URL_ENCODING, &encoding,
                                 sizeof(encoding), &size, 0);
    ok(hres == S_OK, "UrlMkGetSessionOption failed: %08lx\n", hres);
    ok(encoding != 0xdeadbeef, "encoding not changed\n");
    ok(size == sizeof(encoding), "size=%ld\n", size);

    size = encoding = 0xdeadbeef;
    hres = UrlMkGetSessionOption(URLMON_OPTION_URL_ENCODING, &encoding,
                                 sizeof(encoding)+1, &size, 0);
    ok(hres == S_OK, "UrlMkGetSessionOption failed: %08lx\n", hres);
    ok(encoding != 0xdeadbeef, "encoding not changed\n");
    ok(size == sizeof(encoding), "size=%ld\n", size);

    size = encoding = 0xdeadbeef;
    hres = UrlMkGetSessionOption(URLMON_OPTION_URL_ENCODING, &encoding,
                                 sizeof(encoding)-1, &size, 0);
    ok(hres == E_INVALIDARG, "UrlMkGetSessionOption failed: %08lx\n", hres);
    ok(encoding == 0xdeadbeef, "encoding = %08lx, exepcted 0xdeadbeef\n", encoding);
    ok(size == 0xdeadbeef, "size=%ld\n", size);

    size = encoding = 0xdeadbeef;
    hres = UrlMkGetSessionOption(URLMON_OPTION_URL_ENCODING, NULL,
                                 sizeof(encoding)-1, &size, 0);
    ok(hres == E_INVALIDARG, "UrlMkGetSessionOption failed: %08lx\n", hres);
    ok(encoding == 0xdeadbeef, "encoding = %08lx, exepcted 0xdeadbeef\n", encoding);
    ok(size == 0xdeadbeef, "size=%ld\n", size);

    encoding = 0xdeadbeef;
    hres = UrlMkGetSessionOption(URLMON_OPTION_URL_ENCODING, &encoding,
                                 sizeof(encoding)-1, NULL, 0);
    ok(hres == E_INVALIDARG, "UrlMkGetSessionOption failed: %08lx\n", hres);
    ok(encoding == 0xdeadbeef, "encoding = %08lx, exepcted 0xdeadbeef\n", encoding);
}

START_TEST(misc)
{
    OleInitialize(NULL);

    register_protocols();

    test_CreateFormatEnum();
    test_RegisterFormatEnumerator();
    test_CoInternetParseUrl();
    test_FindMimeFromData();
    test_SecurityManager();
    test_ZoneManager();
    test_NameSpace();
    test_ReleaseBindInfo();
    test_UrlMkGetSessionOption();

    OleUninitialize();
}
