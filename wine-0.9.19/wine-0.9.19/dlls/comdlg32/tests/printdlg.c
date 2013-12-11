/* 
 * Unit test suite for comdlg32 API functions: printer dialogs
 *
 * Copyright 2006 Detlef Riekenberg
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
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wingdi.h"
#include "wingdi.h"
#include "winuser.h"

#include "cderr.h"
#include "commdlg.h"

#include "wine/test.h"


/* ##### */

static void test_PrintDlgA(void)
{
    DWORD       res;
    LPPRINTDLGA pDlg;


    pDlg = HeapAlloc(GetProcessHeap(), 0, (sizeof(PRINTDLGA)) * 2);
    if (!pDlg) return;


#if 0
    /* will crash with unpatched wine */
    SetLastError(0xdeadbeef);
    res = PrintDlgA(NULL);
    ok( !res && (CommDlgExtendedError() == CDERR_INITIALIZATION),
        "returned %ld with 0x%lx and 0x%lx (expected '0' and " \
        "CDERR_INITIALIZATION)\n", res, GetLastError(), CommDlgExtendedError());
    }
#endif

    ZeroMemory(pDlg, sizeof(PRINTDLGA));
    pDlg->lStructSize = sizeof(PRINTDLGA) - 1;
    SetLastError(0xdeadbeef);
    res = PrintDlgA(pDlg);
    ok( !res && (CommDlgExtendedError() == CDERR_STRUCTSIZE),
        "returned %ld with 0x%lx and 0x%lx (expected '0' and " \
        "CDERR_STRUCTSIZE)\n", res, GetLastError(), CommDlgExtendedError());


    ZeroMemory(pDlg, sizeof(PRINTDLGA));
    pDlg->lStructSize = sizeof(PRINTDLGA);
    pDlg->Flags = PD_RETURNDEFAULT;
    SetLastError(0xdeadbeef);
    res = PrintDlgA(pDlg);
    ok( res || (CommDlgExtendedError() == PDERR_NODEFAULTPRN),
        "returned %ld with 0x%lx and 0x%lx (expected '!= 0' or '0' and " \
        "PDERR_NODEFAULTPRN)\n", res, GetLastError(), CommDlgExtendedError());

    HeapFree(GetProcessHeap(), 0, pDlg);

}


START_TEST(printdlg)
{
    test_PrintDlgA();

}
