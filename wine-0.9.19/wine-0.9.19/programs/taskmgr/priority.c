/*
 *  ReactOS Task Manager
 *
 *  priority.c
 *
 *  Copyright (C) 1999 - 2001  Brian Palmer  <brianp@reactos.org>
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
    
#define WIN32_LEAN_AND_MEAN    /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <winnt.h>
    
#include "taskmgr.h"
#include "perfdata.h"

static void DoSetPriority(DWORD priority)
{
    LVITEM            lvitem;
    ULONG            Index;
    DWORD            dwProcessId;
    HANDLE            hProcess;
    TCHAR            strErrorText[260];

    for (Index=0; Index<(ULONG)ListView_GetItemCount(hProcessPageListCtrl); Index++)
    {
        memset(&lvitem, 0, sizeof(LVITEM));

        lvitem.mask = LVIF_STATE;
        lvitem.stateMask = LVIS_SELECTED;
        lvitem.iItem = Index;

        SendMessage(hProcessPageListCtrl, LVM_GETITEM, 0, (LPARAM)&lvitem);

        if (lvitem.state & LVIS_SELECTED)
            break;
    }

    dwProcessId = PerfDataGetProcessId(Index);

    if ((ListView_GetSelectedCount(hProcessPageListCtrl) != 1) || (dwProcessId == 0))
        return;

    if (MessageBox(hMainWnd, _T("WARNING: Changing the priority class of this process may\ncause undesired results including system instability. Are you\nsure you want to change the priority class?"), _T("Task Manager Warning"), MB_YESNO|MB_ICONWARNING) != IDYES)
        return;

    hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, dwProcessId);

    if (!hProcess)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Change Priority"), MB_OK|MB_ICONSTOP);
        return;
    }

    if (!SetPriorityClass(hProcess, priority))
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Change Priority"), MB_OK|MB_ICONSTOP);
    }

    CloseHandle(hProcess);
}

void ProcessPage_OnSetPriorityRealTime(void)
{
    DoSetPriority(REALTIME_PRIORITY_CLASS);
}

void ProcessPage_OnSetPriorityHigh(void)
{
    DoSetPriority(HIGH_PRIORITY_CLASS);
}

void ProcessPage_OnSetPriorityAboveNormal(void)
{
    DoSetPriority(ABOVE_NORMAL_PRIORITY_CLASS);
}

void ProcessPage_OnSetPriorityNormal(void)
{
    DoSetPriority(NORMAL_PRIORITY_CLASS);
}

void ProcessPage_OnSetPriorityBelowNormal(void)
{
    DoSetPriority(BELOW_NORMAL_PRIORITY_CLASS);
}

void ProcessPage_OnSetPriorityLow(void)
{
    DoSetPriority(IDLE_PRIORITY_CLASS);
}
