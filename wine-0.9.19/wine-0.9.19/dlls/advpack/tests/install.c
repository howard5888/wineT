/*
 * Unit tests for advpack.dll install functions
 *
 * Copyright (C) 2006 James Hawkins
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

#include <stdio.h>
#include <windows.h>
#include <advpub.h>
#include "wine/test.h"

/* function pointers */
static HRESULT (WINAPI *pRunSetupCommand)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, HANDLE*, DWORD, LPVOID);
static HRESULT (WINAPI *pLaunchINFSection)(HWND, HINSTANCE, LPSTR, INT);
static HRESULT (WINAPI *pLaunchINFSectionEx)(HWND, HINSTANCE, LPSTR, INT);

static char CURR_DIR[MAX_PATH];

static BOOL init_function_pointers()
{
    HMODULE hAdvPack = LoadLibraryA("advpack.dll");
    if (!hAdvPack)
        return FALSE;

    pRunSetupCommand = (void *)GetProcAddress(hAdvPack, "RunSetupCommand");
    pLaunchINFSection = (void *)GetProcAddress(hAdvPack, "LaunchINFSection");
    pLaunchINFSectionEx = (void *)GetProcAddress(hAdvPack, "LaunchINFSectionEx");

    if (!pRunSetupCommand || !pLaunchINFSection || !pLaunchINFSectionEx)
        return FALSE;

    return TRUE;
}

static BOOL is_spapi_err(DWORD err)
{
    const DWORD SPAPI_PREFIX = 0x800F0000L;
    const DWORD SPAPI_MASK = 0xFFFF0000L;

    return (((err & SPAPI_MASK) ^ SPAPI_PREFIX) == 0);
}

static void append_str(char **str, const char *data)
{
    sprintf(*str, data);
    *str += strlen(*str);
}

static void create_inf_file(LPCSTR filename)
{
    char data[1024];
    char *ptr = data;
    DWORD dwNumberOfBytesWritten;
    HANDLE hf = CreateFile(filename, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    append_str(&ptr, "[Version]\n");
    append_str(&ptr, "Signature=\"$Chicago$\"\n");
    append_str(&ptr, "AdvancedINF=2.5\n");
    append_str(&ptr, "[DefaultInstall]\n");
    append_str(&ptr, "RegisterOCXs=RegisterOCXsSection\n");
    append_str(&ptr, "[RegisterOCXsSection]\n");
    append_str(&ptr, "%%11%%\\ole32.dll\n");

    WriteFile(hf, data, ptr - data, &dwNumberOfBytesWritten, NULL);
    CloseHandle(hf);
}

static void test_RunSetupCommand()
{
    HRESULT hr;
    HANDLE hexe;
    char path[MAX_PATH];
    char dir[MAX_PATH];

    /* try an invalid cmd name */
    hr = pRunSetupCommand(NULL, NULL, "Install", "Dir", "Title", NULL, 0, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %ld\n", hr);

    /* try an invalid directory */
    hr = pRunSetupCommand(NULL, "winver.exe", "Install", NULL, "Title", NULL, 0, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %ld\n", hr);

    /* try to run a nonexistent exe */
    hexe = (HANDLE)0xdeadbeef;
    hr = pRunSetupCommand(NULL, "idontexist.exe", "Install", "c:\\windows\\system32", "Title", &hexe, 0, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
       "Expected HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), got %ld\n", hr);
    ok(hexe == NULL, "Expcted hexe to be NULL\n");
    ok(!TerminateProcess(hexe, 0), "Expected TerminateProcess to fail\n");

    /* try a bad directory */
    hexe = (HANDLE)0xdeadbeef;
    hr = pRunSetupCommand(NULL, "winver.exe", "Install", "non\\existent\\directory", "Title", &hexe, 0, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_DIRECTORY),
       "Expected HRESULT_FROM_WIN32(ERROR_DIRECTORY), got %ld\n", hr);
    ok(hexe == NULL, "Expcted hexe to be NULL\n");
    ok(!TerminateProcess(hexe, 0), "Expected TerminateProcess to fail\n");

    /* try to run an exe with the RSC_FLAG_INF flag */
    hexe = (HANDLE)0xdeadbeef;
    hr = pRunSetupCommand(NULL, "winver.exe", "Install", "c:\\windows\\system32", "Title", &hexe, RSC_FLAG_INF, NULL);
    ok(is_spapi_err(hr), "Expected a setupapi error, got %ld\n", hr);
    ok(hexe == (HANDLE)0xdeadbeef, "Expected hexe to be 0xdeadbeef\n");
    ok(!TerminateProcess(hexe, 0), "Expected TerminateProcess to fail\n");

    /* run winver.exe */
    hexe = (HANDLE)0xdeadbeef;
    hr = pRunSetupCommand(NULL, "winver.exe", "Install", "c:\\windows\\system32", "Title", &hexe, 0, NULL);
    ok(hr == S_ASYNCHRONOUS, "Expected S_ASYNCHRONOUS, got %ld\n", hr);
    ok(hexe != NULL, "Expected hexe to be non-NULL\n");
    ok(TerminateProcess(hexe, 0), "Expected TerminateProcess to succeed\n");

    CreateDirectoryA("one", NULL);
    create_inf_file("one\\test.inf");

    /* try a full path to the INF, with working dir provided */
    lstrcpy(path, CURR_DIR);
    lstrcat(path, "\\one\\test.inf");
    lstrcpy(dir, CURR_DIR);
    lstrcat(dir, "\\one");
    hr = pRunSetupCommand(NULL, path, "DefaultInstall", dir, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %ld\n", hr);

    /* try a full path to the INF, NULL working dir */
    hr = pRunSetupCommand(NULL, path, "DefaultInstall", NULL, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
       "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %ld\n", hr);

    /* try a full path to the INF, empty working dir */
    hr = pRunSetupCommand(NULL, path, "DefaultInstall", "", "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %ld\n", hr);

    /* try a relative path to the INF, with working dir provided */
    hr = pRunSetupCommand(NULL, "one\\test.inf", "DefaultInstall", dir, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %ld\n", hr);

    /* try a relative path to the INF, NULL working dir */
    hr = pRunSetupCommand(NULL, "one\\test.inf", "DefaultInstall", NULL, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
       "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %ld\n", hr);

    /* try a relative path to the INF, empty working dir */
    hr = pRunSetupCommand(NULL, "one\\test.inf", "DefaultInstall", "", "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == ERROR_SUCCESS, "Expected ERROR_SUCCESS, got %ld\n", hr);

    /* try only the INF filename, with working dir provided */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", dir, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
       "Expected HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), got %ld\n", hr);

    /* try only the INF filename, NULL working dir */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", NULL, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
       "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %ld\n", hr);

    /* try only the INF filename, empty working dir */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", "", "Title", NULL, RSC_FLAG_INF, NULL);
    todo_wine
    {
        ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
           "Expected HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), got %ld\n", hr);
    }

    DeleteFileA("one\\test.inf");
    RemoveDirectoryA("one");

    create_inf_file("test.inf");

    /* try INF file in the current directory, working directory provided */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", CURR_DIR, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
       "Expected HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), got %ld\n", hr);

    /* try INF file in the current directory, NULL working directory */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", NULL, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
       "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %ld\n", hr);

    /* try INF file in the current directory, empty working directory */
    hr = pRunSetupCommand(NULL, "test.inf", "DefaultInstall", CURR_DIR, "Title", NULL, RSC_FLAG_INF, NULL);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND),
       "Expected HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), got %ld\n", hr);
}

static void test_LaunchINFSection()
{
    HRESULT hr;
    char cmdline[MAX_PATH];
    static char file[] = "test.inf,DefaultInstall,4,0";

    /* try an invalid cmdline */
    hr = pLaunchINFSection(NULL, NULL, NULL, 0);
    ok(hr == 1, "Expected 1, got %ld\n", hr);

    CreateDirectoryA("one", NULL);
    create_inf_file("one\\test.inf");

    /* try a full path to the INF */
    lstrcpy(cmdline, CURR_DIR);
    lstrcat(cmdline, "\\");
    lstrcat(cmdline, "one\\test.inf,DefaultInstall,,4");
    hr = pLaunchINFSection(NULL, NULL, cmdline, 0);
    ok(hr == 0, "Expected 0, got %ld\n", hr);

    DeleteFileA("one\\test.inf");
    RemoveDirectoryA("one");

    create_inf_file("test.inf");

    /* try just the INF filename */
    hr = pLaunchINFSection(NULL, NULL, file, 0);
    todo_wine
    {
        ok(hr == 0, "Expected 0, got %ld\n", hr);
    }

    DeleteFileA("test.inf");
}

START_TEST(install)
{
    if (!init_function_pointers())
        return;

    GetCurrentDirectoryA(MAX_PATH, CURR_DIR);

    test_RunSetupCommand();
    test_LaunchINFSection();
}
