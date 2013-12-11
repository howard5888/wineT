/*
 * Unit test suite for ndr marshalling functions
 *
 * Copyright 2006 Huw Davies
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

#include "wine/test.h"
#include <windef.h>
#include <winbase.h>
#include <winnt.h>
#include <winerror.h>

#include "rpc.h"
#include "rpcdce.h"
#include "rpcproxy.h"


static int my_alloc_called;
static int my_free_called;
void * CALLBACK my_alloc(size_t size)
{
    my_alloc_called++;
    return NdrOleAllocate(size);
}

void CALLBACK my_free(void *ptr)
{
    my_free_called++;
    return NdrOleFree(ptr);
}

static const MIDL_STUB_DESC Object_StubDesc = 
    {
    NULL,
    my_alloc,
    my_free,
    { 0 },
    0,
    0,
    0,
    0,
    NULL, /* format string, filled in by tests */
    1, /* -error bounds_check flag */
    0x20000, /* Ndr library version */
    0,
    0x50100a4, /* MIDL Version 5.1.164 */
    0,
    NULL,
    0,  /* notify & notify_flag routine table */
    1,  /* Flags */
    0,  /* Reserved3 */
    0,  /* Reserved4 */
    0   /* Reserved5 */
    };


static void test_pointer_marshal(const unsigned char *formattypes,
                                 void *memsrc,
                                 long srcsize,
                                 const void *wiredata,
                                 long wiredatalen,
                                 int(*cmp)(const void*,const void*,size_t),
                                 long num_additional_allocs,
                                 const char *msgpfx)
{
    RPC_MESSAGE RpcMessage;
    MIDL_STUB_MESSAGE StubMsg;
    MIDL_STUB_DESC StubDesc;
    DWORD size;
    void *ptr;
    unsigned char *mem, *mem_orig;

    my_alloc_called = my_free_called = 0;
    if(!cmp)
        cmp = memcmp;

    StubDesc = Object_StubDesc;
    StubDesc.pFormatTypes = formattypes;

    NdrClientInitializeNew(
                           &RpcMessage,
                           &StubMsg,
                           &StubDesc,
                           0);

    StubMsg.BufferLength = 0;
    NdrPointerBufferSize( &StubMsg,
                          memsrc,
                          formattypes );
    ok(StubMsg.BufferLength >= wiredatalen, "%s: length %ld\n", msgpfx, StubMsg.BufferLength);

    /*NdrGetBuffer(&_StubMsg, _StubMsg.BufferLength, NULL);*/
    StubMsg.RpcMsg->Buffer = StubMsg.BufferStart = StubMsg.Buffer = HeapAlloc(GetProcessHeap(), 0, StubMsg.BufferLength);
    StubMsg.BufferEnd = StubMsg.BufferStart + StubMsg.BufferLength;

    memset(StubMsg.BufferStart, 0x0, StubMsg.BufferLength); /* This is a hack to clear the padding between the ptr and longlong/double */

    ptr = NdrPointerMarshall( &StubMsg,  memsrc, formattypes );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen);
    ok(!memcmp(StubMsg.BufferStart, wiredata, wiredatalen), "%s: incorrectly marshaled\n", msgpfx);

    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 0;

#if 0 /* NdrPointerMemorySize crashes under Wine, remove #if 0 when this is fixed */
    size = NdrPointerMemorySize( &StubMsg, formattypes );
    ok(size == StubMsg.MemorySize, "%s: mem size %ld size %ld\n", msgpfx, StubMsg.MemorySize, size); 
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen); 
    if(formattypes[1] & 0x10 /* FC_POINTER_DEREF */)
        ok(size == srcsize + 4, "%s: mem size %ld\n", msgpfx, size);
    else
        ok(size == srcsize, "%s: mem size %ld\n", msgpfx, size);

    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 16;
    size = NdrPointerMemorySize( &StubMsg, formattypes );
    ok(size == StubMsg.MemorySize, "%s: mem size %ld size %ld\n", msgpfx, StubMsg.MemorySize, size); 
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen); 
    if(formattypes[1] & 0x10 /* FC_POINTER_DEREF */)
        ok(size == srcsize + 4 + 16, "%s: mem size %ld\n", msgpfx, size);
    else
        ok(size == srcsize + 16, "%s: mem size %ld\n", msgpfx, size);
 
    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 1;
    size = NdrPointerMemorySize( &StubMsg, formattypes );
    ok(size == StubMsg.MemorySize, "%s: mem size %ld size %ld\n", msgpfx, StubMsg.MemorySize, size); 
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen); 
    if(formattypes[1] & 0x10 /* FC_POINTER_DEREF */)
        ok(size == srcsize + 4 + (srcsize == 8 ? 8 : 4), "%s: mem size %ld\n", msgpfx, size);
    else
        ok(size == srcsize + (srcsize == 8 ? 8 : 4), "%s: mem size %ld\n", msgpfx, size);

#endif

    size = srcsize;
    if(formattypes[1] & 0x10) size += 4;

    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 0;
    mem_orig = mem = HeapAlloc(GetProcessHeap(), 0, size); 

    if(formattypes[1] & 0x10 /* FC_POINTER_DEREF */)
        *(void**)mem = NULL;
    ptr = NdrPointerUnmarshall( &StubMsg, &mem, formattypes, 0 );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(mem == mem_orig, "%s: mem has changed %p %p\n", msgpfx, mem, mem_orig);
    ok(!cmp(mem, memsrc, srcsize), "%s: incorrectly unmarshaled\n", msgpfx);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen);
    ok(StubMsg.MemorySize == 0, "%s: memorysize %ld\n", msgpfx, StubMsg.MemorySize);
    ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called); 
    my_alloc_called = 0;

    /* reset the buffer and call with must alloc */
    StubMsg.Buffer = StubMsg.BufferStart;
    if(formattypes[1] & 0x10 /* FC_POINTER_DEREF */)
        *(void**)mem = NULL;
    ptr = NdrPointerUnmarshall( &StubMsg, &mem, formattypes, 1 );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    /* doesn't allocate mem in this case */
todo_wine {
    ok(mem == mem_orig, "%s: mem has changed %p %p\n", msgpfx, mem, mem_orig);
 }
    ok(!cmp(mem, memsrc, srcsize), "%s: incorrectly unmarshaled\n", msgpfx);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen);
    ok(StubMsg.MemorySize == 0, "%s: memorysize %ld\n", msgpfx, StubMsg.MemorySize);

todo_wine {
    ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called); 
}
    my_alloc_called = 0;
    if(formattypes[0] != 0x11 /* FC_RP */)
    {
        /* now pass the address of a NULL ptr */
        mem = NULL;
        StubMsg.Buffer = StubMsg.BufferStart;
        ptr = NdrPointerUnmarshall( &StubMsg, &mem, formattypes, 0 );
        ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
        ok(mem != StubMsg.BufferStart + wiredatalen - srcsize, "%s: mem points to buffer %p %p\n", msgpfx, mem, StubMsg.BufferStart);
        ok(!cmp(mem, memsrc, size), "%s: incorrectly unmarshaled\n", msgpfx);
        ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen);
        ok(StubMsg.MemorySize == 0, "%s: memorysize %ld\n", msgpfx, StubMsg.MemorySize);
        ok(my_alloc_called == num_additional_allocs + 1, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called); 
        my_alloc_called = 0;
        NdrPointerFree(&StubMsg, mem, formattypes);
 
        /* again pass address of NULL ptr, but pretend we're a server */
        mem = NULL;
        StubMsg.Buffer = StubMsg.BufferStart;
        StubMsg.IsClient = 0;
        ptr = NdrPointerUnmarshall( &StubMsg, &mem, formattypes, 0 );
        ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
todo_wine {
        ok(mem == StubMsg.BufferStart + wiredatalen - srcsize, "%s: mem doesn't point to buffer %p %p\n", msgpfx, mem, StubMsg.BufferStart);
 }
        ok(!cmp(mem, memsrc, size), "%s: incorrecly unmarshaled\n", msgpfx);
        ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p len %ld\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart, wiredatalen);
        ok(StubMsg.MemorySize == 0, "%s: memorysize %ld\n", msgpfx, StubMsg.MemorySize); 
todo_wine {
        ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called); 
        my_alloc_called = 0;
 }
    }
    HeapFree(GetProcessHeap(), 0, mem_orig);
    HeapFree(GetProcessHeap(), 0, StubMsg.BufferStart);
}

static int deref_cmp(const void *s1, const void *s2, size_t num)
{
    return memcmp(*(void**)s1, *(void**)s2, num);
}


static void test_simple_types()
{
    unsigned char wiredata[16];
    unsigned char ch;
    unsigned char *ch_ptr;
    unsigned short s;
    unsigned int i;
    unsigned long l;
    ULONGLONG ll;
    float f;
    double d;

    static const unsigned char fmtstr_up_char[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x2,            /* FC_CHAR */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_byte[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x1,            /* FC_BYTE */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_small[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x3,            /* FC_SMALL */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_usmall[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x4,            /* FC_USMALL */
        0x5c,           /* FC_PAD */
    };  
    static const unsigned char fmtstr_rp_char[] =
    {
        0x11, 0x8,      /* FC_RP [simple_pointer] */
        0x2,            /* FC_CHAR */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_rpup_char[] =
    {
        0x11, 0x14,     /* FC_RP [alloced_on_stack] */
        NdrFcShort( 0x2 ),      /* Offset= 2 (4) */
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x2,            /* FC_CHAR */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_rpup_char2[] =
    {
        0x11, 0x04,     /* FC_RP [alloced_on_stack] */
        NdrFcShort( 0x2 ),      /* Offset= 2 (4) */
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x2,            /* FC_CHAR */
        0x5c,           /* FC_PAD */
    };

    static const unsigned char fmtstr_up_wchar[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x5,            /* FC_WCHAR */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_short[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x6,            /* FC_SHORT */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_ushort[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x7,            /* FC_USHORT */
        0x5c,           /* FC_PAD */
    };
#if 0
    static const unsigned char fmtstr_up_enum16[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0xd,            /* FC_ENUM16 */
        0x5c,           /* FC_PAD */
    };
#endif
    static const unsigned char fmtstr_up_long[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x8,            /* FC_LONG */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_ulong[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x9,            /* FC_ULONG */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_enum32[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0xe,            /* FC_ENUM32 */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_errorstatus[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0x10,           /* FC_ERROR_STATUS_T */
        0x5c,           /* FC_PAD */
    };

    static const unsigned char fmtstr_up_longlong[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0xb,            /* FC_HYPER */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_float[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0xa,            /* FC_FLOAT */
        0x5c,           /* FC_PAD */
    };
    static const unsigned char fmtstr_up_double[] =
    {
        0x12, 0x8,      /* FC_UP [simple_pointer] */
        0xc,            /* FC_DOUBLE */
        0x5c,           /* FC_PAD */
    };

    ch = 0xa5;
    ch_ptr = &ch;
    *(void**)wiredata = ch_ptr;
    wiredata[sizeof(void*)] = ch;
 
    test_pointer_marshal(fmtstr_up_char, ch_ptr, 1, wiredata, 5, NULL, 0, "up_char");
    test_pointer_marshal(fmtstr_up_byte, ch_ptr, 1, wiredata, 5, NULL, 0, "up_byte");
    test_pointer_marshal(fmtstr_up_small, ch_ptr, 1, wiredata, 5, NULL, 0,  "up_small");
    test_pointer_marshal(fmtstr_up_usmall, ch_ptr, 1, wiredata, 5, NULL, 0, "up_usmall");

    test_pointer_marshal(fmtstr_rp_char, ch_ptr, 1, &ch, 1, NULL, 0, "rp_char");

    test_pointer_marshal(fmtstr_rpup_char, &ch_ptr, 1, wiredata, 5, deref_cmp, 1, "rpup_char");
    test_pointer_marshal(fmtstr_rpup_char2, ch_ptr, 1, wiredata, 5, NULL, 0, "rpup_char2");

    s = 0xa597;
    *(void**)wiredata = &s;
    *(unsigned short*)(wiredata + sizeof(void*)) = s;

    test_pointer_marshal(fmtstr_up_wchar, &s, 2, wiredata, 6, NULL, 0, "up_wchar");
    test_pointer_marshal(fmtstr_up_short, &s, 2, wiredata, 6, NULL, 0, "up_short");
    test_pointer_marshal(fmtstr_up_ushort, &s, 2, wiredata, 6, NULL, 0, "up_ushort");

    i = s;
    *(void**)wiredata = &i;
#if 0 /* Not sure why this crashes under Windows */
    test_pointer_marshal(fmtstr_up_enum16, &i, 2, wiredata, 6, NULL, 0, "up_enum16");
#endif

    l = 0xcafebabe;
    *(void**)wiredata = &l;
    *(unsigned long*)(wiredata + sizeof(void*)) = l;

    test_pointer_marshal(fmtstr_up_long, &l, 4, wiredata, 8, NULL, 0, "up_long");
    test_pointer_marshal(fmtstr_up_ulong, &l, 4, wiredata, 8, NULL, 0,  "up_ulong");
    test_pointer_marshal(fmtstr_up_enum32, &l, 4, wiredata, 8, NULL, 0,  "up_emun32");
    test_pointer_marshal(fmtstr_up_errorstatus, &l, 4, wiredata, 8, NULL, 0,  "up_errorstatus");

    ll = ((ULONGLONG)0xcafebabe) << 32 | 0xdeadbeef;
    *(void**)wiredata = &ll;
    *(void**)(wiredata + sizeof(void*)) = NULL;
    *(ULONGLONG*)(wiredata + 2 * sizeof(void*)) = ll;
    test_pointer_marshal(fmtstr_up_longlong, &ll, 8, wiredata, 16, NULL, 0, "up_longlong");

    f = 3.1415;
    *(void**)wiredata = &f;
    *(float*)(wiredata + sizeof(void*)) = f;
    test_pointer_marshal(fmtstr_up_float, &f, 4, wiredata, 8, NULL, 0, "up_float");

    d = 3.1415;
    *(void**)wiredata = &d;
    *(void**)(wiredata + sizeof(void*)) = NULL;
    *(double*)(wiredata + 2 * sizeof(void*)) = d;
    test_pointer_marshal(fmtstr_up_double, &d, 8, wiredata, 16, NULL, 0,  "up_double");

}

static void test_simple_struct_marshal(const unsigned char *formattypes,
                                       void *memsrc,
                                       long srcsize,
                                       const void *wiredata,
                                       long wiredatalen,
                                       int(*cmp)(const void*,const void*,size_t),
                                       long num_additional_allocs,
                                       const char *msgpfx)
{
    RPC_MESSAGE RpcMessage;
    MIDL_STUB_MESSAGE StubMsg;
    MIDL_STUB_DESC StubDesc;
    DWORD size;
    void *ptr;
    unsigned char *mem, *mem_orig;

    my_alloc_called = my_free_called = 0;
    if(!cmp)
        cmp = memcmp;

    StubDesc = Object_StubDesc;
    StubDesc.pFormatTypes = formattypes;

    NdrClientInitializeNew(&RpcMessage, &StubMsg, &StubDesc, 0);

    StubMsg.BufferLength = 0;
    NdrSimpleStructBufferSize( &StubMsg, (unsigned char *)memsrc, formattypes );
    ok(StubMsg.BufferLength >= wiredatalen, "%s: length %ld\n", msgpfx, StubMsg.BufferLength);
    StubMsg.RpcMsg->Buffer = StubMsg.BufferStart = StubMsg.Buffer = HeapAlloc(GetProcessHeap(), 0, StubMsg.BufferLength);
    StubMsg.BufferEnd = StubMsg.BufferStart + StubMsg.BufferLength;
    ptr = NdrSimpleStructMarshall( &StubMsg,  (unsigned char*)memsrc, formattypes );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart);
    ok(!memcmp(StubMsg.BufferStart, wiredata, wiredatalen), "%s: incorrectly marshaled %08lx %08lx %08lx\n", msgpfx, *(DWORD*)StubMsg.BufferStart,*((DWORD*)StubMsg.BufferStart+1),*((DWORD*)StubMsg.BufferStart+2));

#if 0
    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 0;
    size = NdrSimpleStructMemorySize( &StubMsg, formattypes );
    ok(size == StubMsg.MemorySize, "%s: size != MemorySize\n", msgpfx);
    ok(size == srcsize, "%s: mem size %ld\n", msgpfx, size);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart);

    StubMsg.Buffer = StubMsg.BufferStart;
    size = NdrSimpleStructMemorySize( &StubMsg, formattypes );
todo_wine {
    ok(size == StubMsg.MemorySize, "%s: size != MemorySize\n", msgpfx);
}
    ok(StubMsg.MemorySize == ((srcsize + 3) & ~3) + srcsize, "%s: mem size %ld\n", msgpfx, size);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart);
#endif
    size = srcsize;
    /*** Unmarshalling first with must_alloc false ***/

    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.MemorySize = 0;
    mem_orig = mem = HeapAlloc(GetProcessHeap(), 0, srcsize);
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 0 );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(StubMsg.Buffer - StubMsg.BufferStart == wiredatalen, "%s: Buffer %p Start %p\n", msgpfx, StubMsg.Buffer, StubMsg.BufferStart);
    ok(mem == mem_orig, "%s: mem has changed %p %p\n", msgpfx, mem, mem_orig);
    ok(!cmp(mem, memsrc, srcsize), "%s: incorrectly unmarshaled\n", msgpfx);
    ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called); 
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "%s: memorysize touched in unmarshal\n", msgpfx);

    /* if we're a server we still use the suppiled memory */
    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.IsClient = 0;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 0 );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(mem == mem_orig, "%s: mem has changed %p %p\n", msgpfx, mem, mem_orig);
    ok(!cmp(mem, memsrc, srcsize), "%s: incorrectly unmarshaled\n", msgpfx); 
    ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "%s: memorysize touched in unmarshal\n", msgpfx);

    /* ...unless we pass a NULL ptr, then the buffer is used. 
       Passing a NULL ptr while we're a client && !must_alloc
       crashes on Windows, so we won't do that. */

    mem = NULL;
    StubMsg.IsClient = 0;
    StubMsg.Buffer = StubMsg.BufferStart;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 0 );
    ok(ptr == NULL, "%s: ret %p\n", msgpfx, ptr);
    ok(mem == StubMsg.BufferStart, "%s: mem not equal buffer\n", msgpfx);
    ok(!cmp(mem, memsrc, srcsize), "%s: incorrectly unmarshaled\n", msgpfx);
    ok(my_alloc_called == num_additional_allocs, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "%s: memorysize touched in unmarshal\n", msgpfx);

    /*** now must_alloc is true ***/

    /* with must_alloc set we always allocate new memory whether or not we're
       a server and also when passing NULL */
    mem = mem_orig;
    StubMsg.IsClient = 1;
    StubMsg.Buffer = StubMsg.BufferStart;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 1 );
    ok(ptr == NULL, "ret %p\n", ptr);
    ok(mem != mem_orig, "mem not changed %p %p\n", mem, mem_orig);
    ok(!cmp(mem, memsrc, srcsize), "incorrectly unmarshaled\n");
    ok(my_alloc_called == num_additional_allocs + 1, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "memorysize touched in unmarshal\n");

    mem = NULL;
    StubMsg.Buffer = StubMsg.BufferStart;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 1 );
    ok(ptr == NULL, "ret %p\n", ptr);
    ok(mem != mem_orig, "mem not changed %p %p\n", mem, mem_orig);
    ok(!cmp(mem, memsrc, srcsize), "incorrectly unmarshaled\n");
    ok(my_alloc_called == num_additional_allocs + 1, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0; 
    ok(StubMsg.MemorySize == 0, "memorysize touched in unmarshal\n");

    mem = mem_orig;
    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.IsClient = 0;
    StubMsg.ReuseBuffer = 1;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 1 );
    ok(ptr == NULL, "ret %p\n", ptr);
    ok(mem != mem_orig, "mem not changed %p %p\n", mem, mem_orig);
    ok(mem != StubMsg.BufferStart, "mem is buffer mem\n");
    ok(!cmp(mem, memsrc, srcsize), "incorrectly unmarshaled\n");
    ok(my_alloc_called == num_additional_allocs + 1, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "memorysize touched in unmarshal\n");

    mem = NULL;
    StubMsg.Buffer = StubMsg.BufferStart;
    StubMsg.IsClient = 0;
    StubMsg.ReuseBuffer = 1;
    ptr = NdrSimpleStructUnmarshall( &StubMsg, &mem, formattypes, 1 );
    ok(ptr == NULL, "ret %p\n", ptr);
    ok(mem != StubMsg.BufferStart, "mem is buffer mem\n");
    ok(!cmp(mem, memsrc, srcsize), "incorrectly unmarshaled\n"); 
    ok(my_alloc_called == num_additional_allocs + 1, "%s: my_alloc got called %d times\n", msgpfx, my_alloc_called);
    my_alloc_called = 0;
    ok(StubMsg.MemorySize == 0, "memorysize touched in unmarshal\n");

}

typedef struct
{
    long l1;
    long *pl1;
    char *pc1;
} ps1_t;

static int ps1_cmp(const void *s1, const void *s2, size_t num)
{
    const ps1_t *p1, *p2;

    p1 = s1;
    p2 = s2;

    if(p1->l1 != p2->l1)
        return 1;

    if(p1->pl1 && p2->pl1)
    {
        if(*p1->pl1 != *p2->pl1)
            return 1;
    }
    else if(p1->pl1 || p1->pl1)
        return 1;

    if(p1->pc1 && p2->pc1)
    {
        if(*p1->pc1 != *p2->pc1)
            return 1;
    }
    else if(p1->pc1 || p1->pc1)
        return 1;

    return 0;
}

static void test_simple_struct(void)
{
    unsigned char wiredata[28];
    unsigned long wiredatalen;
    long l;
    char c;
    ps1_t ps1;

    static const unsigned char fmtstr_simple_struct[] =
    {
        0x12, 0x0,      /* FC_UP */
        NdrFcShort( 0x2 ), /* Offset=2 */
        0x15, 0x3,      /* FC_STRUCT [align 4] */
        NdrFcShort( 0x18 ),      /* [size 24] */
        0x6,            /* FC_SHORT */
        0x2,            /* FC_CHAR */ 
        0x38,		/* FC_ALIGNM4 */
	0x8,		/* FC_LONG */
	0x8,		/* FC_LONG */
        0x39,		/* FC_ALIGNM8 */
        0xb,		/* FC_HYPER */ 
        0x5b,		/* FC_END */
    };
    struct {
        short s;
        char c;
        long l1, l2;
        LONGLONG ll;
    } s1;

    static const unsigned char fmtstr_pointer_struct[] =
    { 
        0x12, 0x0,      /* FC_UP */
        NdrFcShort( 0x2 ), /* Offset=2 */
        0x16, 0x3,      /* FC_PSTRUCT [align 4] */
        NdrFcShort( 0xc ),      /* [size 12] */
        0x4b,		/* FC_PP */
        0x5c,		/* FC_PAD */
        0x46,		/* FC_NO_REPEAT */
        0x5c,		/* FC_PAD */
        NdrFcShort( 0x4 ),	/* 4 */
	NdrFcShort( 0x4 ),	/* 4 */
        0x13, 0x8,	/* FC_OP [simple_pointer] */
        0x8,		/* FC_LONG */
        0x5c,		/* FC_PAD */
        0x46,		/* FC_NO_REPEAT */
        0x5c,		/* FC_PAD */
	NdrFcShort( 0x8 ),	/* 8 */
	NdrFcShort( 0x8 ),	/* 8 */
	0x13, 0x8,	/* FC_OP [simple_pointer] */
        0x2,		/* FC_CHAR */
        0x5c,		/* FC_PAD */
        0x5b,		/* FC_END */
        0x8,		/* FC_LONG */
        0x8,		/* FC_LONG */
        0x8,		/* FC_LONG */
        0x5c,		/* FC_PAD */
        0x5b,		/* FC_END */

    };

    /* FC_STRUCT */
    s1.s = 0x1234;
    s1.c = 0xa5;
    s1.l1 = 0xdeadbeef;
    s1.l2 = 0xcafebabe;
    s1.ll = ((LONGLONG) 0xbadefeed << 32) || 0x2468ace0;

    wiredatalen = 24;
    memcpy(wiredata, &s1, wiredatalen); 
    test_simple_struct_marshal(fmtstr_simple_struct + 4, &s1, 24, wiredata, 24, NULL, 0, "struct");

    *(void**)wiredata = &s1;
    memcpy(wiredata + 4, &s1, wiredatalen);
#if 0 /* one of the unmarshallings crashes Wine */
    test_pointer_marshal(fmtstr_simple_struct, &s1, 24, wiredata, 28, NULL, 0, "struct");
#endif

    /* FC_PSTRUCT */
    ps1.l1 = 0xdeadbeef;
    l = 0xcafebabe;
    ps1.pl1 = &l;
    c = 'a';
    ps1.pc1 = &c;
    memcpy(wiredata + 4, &ps1, 12);
    memcpy(wiredata + 16, &l, 4);
    memcpy(wiredata + 20, &c, 1);

    test_simple_struct_marshal(fmtstr_pointer_struct + 4, &ps1, 17, wiredata + 4, 17, ps1_cmp, 2, "pointer_struct");
    *(void**)wiredata = &ps1;
#if 0 /* one of the unmarshallings crashes Wine */
    test_pointer_marshal(fmtstr_pointer_struct, &ps1, 17, wiredata, 21, ps1_cmp, 2, "pointer_struct");
#endif
}

static void test_fullpointer_xlat(void)
{
    PFULL_PTR_XLAT_TABLES pXlatTables;
    unsigned long RefId;
    int ret;
    void *Pointer;

    pXlatTables = NdrFullPointerXlatInit(2, XLAT_CLIENT);

    /* "marshaling" phase */

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 1, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x1, "RefId should be 0x1 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x1, "RefId should be 0x1 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebabe, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x2, "RefId should be 0x2 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xdeadbeef, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, NULL, 0, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0, "RefId should be 0 instead of 0x%lx\n", RefId);

    /* "unmarshaling" phase */

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x2, 0, &Pointer);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(Pointer == (void *)0xcafebabe, "Pointer should be 0xcafebabe instead of %p\n", Pointer);

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x4, 0, &Pointer);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(Pointer == NULL, "Pointer should be NULL instead of %p\n", Pointer);

    NdrFullPointerInsertRefId(pXlatTables, 0x4, (void *)0xdeadbabe);

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x4, 1, &Pointer);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(Pointer == (void *)0xdeadbabe, "Pointer should be (void *)0xdeadbabe instead of %p\n", Pointer);

    NdrFullPointerXlatFree(pXlatTables);

    pXlatTables = NdrFullPointerXlatInit(2, XLAT_SERVER);

    /* "unmarshaling" phase */

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x2, 1, &Pointer);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(Pointer == NULL, "Pointer should be NULL instead of %p\n", Pointer);

    NdrFullPointerInsertRefId(pXlatTables, 0x2, (void *)0xcafebabe);

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x2, 0, &Pointer);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(Pointer == (void *)0xcafebabe, "Pointer should be (void *)0xcafebabe instead of %p\n", Pointer);

    ret = NdrFullPointerQueryRefId(pXlatTables, 0x2, 1, &Pointer);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(Pointer == (void *)0xcafebabe, "Pointer should be (void *)0xcafebabe instead of %p\n", Pointer);

    /* "marshaling" phase */

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 1, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 1, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebabe, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x2, "RefId should be 0x2 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xdeadbeef, 0, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x4, "RefId should be 0x4 instead of 0x%lx\n", RefId);

    /* "freeing" phase */

    ret = NdrFullPointerFree(pXlatTables, (void *)0xcafebeef);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 0x20, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xcafebeef, 1, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0x3, "RefId should be 0x3 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerFree(pXlatTables, (void *)0xcafebabe);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);

    ret = NdrFullPointerFree(pXlatTables, (void *)0xdeadbeef);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xdeadbeef, 0x20, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0x4, "RefId should be 0x4 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xdeadbeef, 1, &RefId);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);
    ok(RefId == 0x4, "RefId should be 0x4 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerQueryPointer(pXlatTables, (void *)0xdeadbeef, 1, &RefId);
    ok(ret == 1, "ret should be 1 instead of 0x%x\n", ret);
    ok(RefId == 0x4, "RefId should be 0x4 instead of 0x%lx\n", RefId);

    ret = NdrFullPointerFree(pXlatTables, (void *)0xdeadbeef);
    ok(ret == 0, "ret should be 0 instead of 0x%x\n", ret);

    NdrFullPointerXlatFree(pXlatTables);
}

static void test_client_init(void)
{
    MIDL_STUB_MESSAGE stubMsg;
    RPC_MESSAGE rpcMsg;

    memset(&stubMsg, 0xcc, sizeof(stubMsg));

    NdrClientInitializeNew(&rpcMsg, &stubMsg, &Object_StubDesc, 1);

#define TEST_ZERO(field, fmt) ok(stubMsg.field == 0, #field " should have be set to zero instead of " fmt "\n", stubMsg.field)
#define TEST_POINTER_UNSET(field) ok(stubMsg.field == (void *)0xcccccccc, #field " should have be unset instead of %p\n", stubMsg.field)
#define TEST_ULONG_UNSET(field) ok(stubMsg.field == 0xcccccccc, #field " should have be unset instead of 0x%lx\n", stubMsg.field)

    ok(stubMsg.RpcMsg == &rpcMsg, "stubMsg.RpcMsg should have been %p instead of %p\n", &rpcMsg, stubMsg.RpcMsg);
    TEST_POINTER_UNSET(Buffer);
    TEST_ZERO(BufferStart, "%p");
    TEST_ZERO(BufferEnd, "%p");
    TEST_POINTER_UNSET(BufferMark);
    TEST_ZERO(BufferLength, "%ld");
    TEST_ULONG_UNSET(MemorySize);
    TEST_POINTER_UNSET(Memory);
    ok(stubMsg.IsClient == 1, "stubMsg.IsClient should have been 1 instead of %u\n", stubMsg.IsClient);
    TEST_ZERO(ReuseBuffer, "%d");
    TEST_ZERO(pAllocAllNodesContext, "%p");
    TEST_ZERO(pPointerQueueState, "%p");
    TEST_ZERO(IgnoreEmbeddedPointers, "%d");
    TEST_ZERO(PointerBufferMark, "%p");
    TEST_ZERO(fBufferValid, "%d");
    TEST_ZERO(uFlags, "%d");
    /* FIXME: UniquePtrCount */
    TEST_ULONG_UNSET(MaxCount);
    TEST_ULONG_UNSET(Offset);
    TEST_ULONG_UNSET(ActualCount);
    ok(stubMsg.pfnAllocate == my_alloc, "stubMsg.pfnAllocate should have been %p instead of %p\n", my_alloc, stubMsg.pfnAllocate);
    ok(stubMsg.pfnFree == my_free, "stubMsg.pfnFree should have been %p instead of %p\n", my_free, stubMsg.pfnFree);
    TEST_ZERO(StackTop, "%p");
    TEST_POINTER_UNSET(pPresentedType);
    TEST_POINTER_UNSET(pTransmitType);
    TEST_POINTER_UNSET(SavedHandle);
    ok(stubMsg.StubDesc == &Object_StubDesc, "stubMsg.StubDesc should have been %p instead of %p\n", &Object_StubDesc, stubMsg.StubDesc);
    TEST_POINTER_UNSET(FullPtrXlatTables);
    TEST_ZERO(FullPtrRefId, "%ld");
    TEST_ZERO(PointerLength, "%ld");
    TEST_ZERO(fInDontFree, "%d");
    TEST_ZERO(fDontCallFreeInst, "%d");
    TEST_ZERO(fInOnlyParam, "%d");
    TEST_ZERO(fHasReturn, "%d");
    TEST_ZERO(fHasExtensions, "%d");
    TEST_ZERO(fHasNewCorrDesc, "%d");
    TEST_ZERO(fUnused, "%d");
    ok(stubMsg.fUnused2 == 0xffffcccc, "stubMsg.fUnused2 should have been 0xcccc instead of 0x%x\n", stubMsg.fUnused2);
    ok(stubMsg.dwDestContext == MSHCTX_DIFFERENTMACHINE, "stubMsg.dwDestContext should have been MSHCTX_DIFFERENTMACHINE instead of %ld\n", stubMsg.dwDestContext);
    TEST_ZERO(pvDestContext, "%p");
    TEST_POINTER_UNSET(SavedContextHandles);
    TEST_ULONG_UNSET(ParamNumber);
    TEST_ZERO(pRpcChannelBuffer, "%p");
    TEST_ZERO(pArrayInfo, "%p");
    TEST_POINTER_UNSET(SizePtrCountArray);
    TEST_POINTER_UNSET(SizePtrOffsetArray);
    TEST_POINTER_UNSET(SizePtrLengthArray);
    TEST_POINTER_UNSET(pArgQueue);
    TEST_ZERO(dwStubPhase, "%ld");
    /* FIXME: where does this value come from? */
    trace("LowStackMark is %p\n", stubMsg.LowStackMark);
    TEST_ZERO(pAsyncMsg, "%p");
    TEST_ZERO(pCorrInfo, "%p");
    TEST_ZERO(pCorrMemory, "%p");
    TEST_ZERO(pMemoryList, "%p");
    TEST_POINTER_UNSET(pCSInfo);
    TEST_POINTER_UNSET(ConformanceMark);
    TEST_POINTER_UNSET(VarianceMark);
    ok(stubMsg.Unused == 0xcccccccc, "Unused should have be unset instead of 0x%x\n", stubMsg.Unused);
    TEST_POINTER_UNSET(pContext);
#if 0
    TEST_ULONG_UNSET(Reserved51_1);
    TEST_ULONG_UNSET(Reserved51_2);
    TEST_ULONG_UNSET(Reserved51_3);
    TEST_ULONG_UNSET(Reserved51_4);
    TEST_ULONG_UNSET(Reserved51_5);
#endif
#undef TEST_ULONG_UNSET
#undef TEST_POINTER_UNSET
#undef TEST_ZERO

}

START_TEST( ndr_marshall )
{
    test_simple_types();
    test_simple_struct();
    test_fullpointer_xlat();
    test_client_init();
}
