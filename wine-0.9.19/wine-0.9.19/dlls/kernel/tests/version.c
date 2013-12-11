/*
 * Unit test suite for version functions
 *
 * Copyright 2006 Robert Shearman
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

#include "wine/test.h"
#include "winbase.h"

static BOOL (WINAPI * pVerifyVersionInfoA)(LPOSVERSIONINFOEXA, DWORD, DWORDLONG);
static ULONGLONG (WINAPI * pVerSetConditionMask)(ULONGLONG, DWORD, BYTE);

#define KERNEL32_GET_PROC(func)                                     \
    p##func = (void *)GetProcAddress(hKernel32, #func);             \
    if(!p##func) trace("GetProcAddress(hKernel32, '%s') failed\n", #func);

static void init_function_pointers(void)
{
    HMODULE hKernel32;

    pVerifyVersionInfoA = NULL;
    pVerSetConditionMask = NULL;

    hKernel32 = GetModuleHandleA("kernel32.dll");
    assert(hKernel32);
    KERNEL32_GET_PROC(VerifyVersionInfoA);
    KERNEL32_GET_PROC(VerSetConditionMask);
}

START_TEST(version)
{
    OSVERSIONINFOEX info = { sizeof(info) };
    BOOL ret;

    init_function_pointers();
    if(!pVerifyVersionInfoA || !pVerSetConditionMask) return;

    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION,
        pVerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    ret = pVerifyVersionInfoA(&info, VER_BUILDNUMBER | VER_MAJORVERSION |
        VER_MINORVERSION/* | VER_PLATFORMID | VER_SERVICEPACKMAJOR |
        VER_SERVICEPACKMINOR | VER_SUITENAME | VER_PRODUCT_TYPE */,
        pVerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    /* tests special handling of VER_SUITENAME */

    ret = pVerifyVersionInfoA(&info, VER_SUITENAME,
        pVerSetConditionMask(0, VER_SUITENAME, VER_AND));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    ret = pVerifyVersionInfoA(&info, VER_SUITENAME,
        pVerSetConditionMask(0, VER_SUITENAME, VER_OR));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    /* test handling of version numbers */
    
    /* v3.10 is always less than v4.x even
     * if the minor version is tested */
    info.dwMajorVersion = 3;
    info.dwMinorVersion = 10;
    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL),
            VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    info.dwMinorVersion = 0;
    info.wServicePackMajor = 10;
    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL),
            VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    info.wServicePackMajor = 0;
    info.wServicePackMinor = 10;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL),
            VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMinor++;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor--;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor--;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor++;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_LESS));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor++;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_LESS_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor--;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    /* test the failure hierarchy for the four version fields */

    GetVersionEx((OSVERSIONINFO *)&info);
    info.wServicePackMajor++;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.dwMinorVersion++;
    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    GetVersionEx((OSVERSIONINFO *)&info);
    info.dwMajorVersion++;
    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());


    GetVersionEx((OSVERSIONINFO *)&info);
    info.dwBuildNumber++;
    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(!ret && (GetLastError() == ERROR_OLD_WIN_VERSION),
        "VerifyVersionInfoA should have failed with ERROR_OLD_WIN_VERSION instead of %ld\n", GetLastError());

    ret = pVerifyVersionInfoA(&info, VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MINORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());

    /* test bad dwOSVersionInfoSize */
    GetVersionEx((OSVERSIONINFO *)&info);
    info.dwOSVersionInfoSize = 0;
    ret = pVerifyVersionInfoA(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        pVerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL));
    ok(ret, "VerifyVersionInfoA failed with error %ld\n", GetLastError());
}
