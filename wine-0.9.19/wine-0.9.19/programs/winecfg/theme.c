/*
 * Desktop Integration
 * - Theme configuration code
 * - User Shell Folder mapping
 *
 * Copyright (c) 2005 by Frank Richter
 * Copyright (c) 2006 by Michael Jung
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
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define COBJMACROS

#include <windows.h>
#include <uxtheme.h>
#include <tmschema.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wine/debug.h>

#include "resource.h"
#include "winecfg.h"

WINE_DEFAULT_DEBUG_CHANNEL(winecfg);

/* UXTHEME functions not in the headers */

typedef struct tagTHEMENAMES
{
    WCHAR szName[MAX_PATH+1];
    WCHAR szDisplayName[MAX_PATH+1];
    WCHAR szTooltip[MAX_PATH+1];
} THEMENAMES, *PTHEMENAMES;

typedef void* HTHEMEFILE;
typedef BOOL (CALLBACK *EnumThemeProc)(LPVOID lpReserved, 
				       LPCWSTR pszThemeFileName,
                                       LPCWSTR pszThemeName, 
				       LPCWSTR pszToolTip, LPVOID lpReserved2,
                                       LPVOID lpData);

HRESULT WINAPI EnumThemeColors (LPWSTR pszThemeFileName, LPWSTR pszSizeName,
				DWORD dwColorNum, PTHEMENAMES pszColorNames);
HRESULT WINAPI EnumThemeSizes (LPWSTR pszThemeFileName, LPWSTR pszColorName,
			       DWORD dwSizeNum, PTHEMENAMES pszSizeNames);
HRESULT WINAPI ApplyTheme (HTHEMEFILE hThemeFile, char* unknown, HWND hWnd);
HRESULT WINAPI OpenThemeFile (LPCWSTR pszThemeFileName, LPCWSTR pszColorName,
			      LPCWSTR pszSizeName, HTHEMEFILE* hThemeFile,
			      DWORD unknown);
HRESULT WINAPI CloseThemeFile (HTHEMEFILE hThemeFile);
HRESULT WINAPI EnumThemes (LPCWSTR pszThemePath, EnumThemeProc callback,
                           LPVOID lpData);

/* A struct to keep both the internal and "fancy" name of a color or size */
typedef struct
{
  WCHAR* name;
  WCHAR* fancyName;
} ThemeColorOrSize;

/* wrapper around DSA that also keeps an item count */
typedef struct
{
  HDSA dsa;
  int count;
} WrappedDsa;

/* Some helper functions to deal with ThemeColorOrSize structs in WrappedDSAs */

static void color_or_size_dsa_add (WrappedDsa* wdsa, const WCHAR* name,
				   const WCHAR* fancyName)
{
    ThemeColorOrSize item;
    
    item.name = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (name) + 1) * sizeof(WCHAR));
    lstrcpyW (item.name, name);

    item.fancyName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (fancyName) + 1) * sizeof(WCHAR));
    lstrcpyW (item.fancyName, fancyName);

    DSA_InsertItem (wdsa->dsa, wdsa->count, &item);
    wdsa->count++;
}

static int CALLBACK dsa_destroy_callback (LPVOID p, LPVOID pData)
{
    ThemeColorOrSize* item = (ThemeColorOrSize*)p;
    HeapFree (GetProcessHeap(), 0, item->name);
    HeapFree (GetProcessHeap(), 0, item->fancyName);
    return 1;
}

static void free_color_or_size_dsa (WrappedDsa* wdsa)
{
    DSA_DestroyCallback (wdsa->dsa, dsa_destroy_callback, NULL);
}

static void create_color_or_size_dsa (WrappedDsa* wdsa)
{
    wdsa->dsa = DSA_Create (sizeof (ThemeColorOrSize), 1);
    wdsa->count = 0;
}

static ThemeColorOrSize* color_or_size_dsa_get (WrappedDsa* wdsa, int index)
{
    return (ThemeColorOrSize*)DSA_GetItemPtr (wdsa->dsa, index);
}

static int color_or_size_dsa_find (WrappedDsa* wdsa, const WCHAR* name)
{
    int i = 0;
    for (; i < wdsa->count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (wdsa, i);
	if (lstrcmpiW (item->name, name) == 0) break;
    }
    return i;
}

/* A theme file, contains file name, display name, color and size scheme names */
typedef struct
{
    WCHAR* themeFileName;
    WCHAR* fancyName;
    WrappedDsa colors;
    WrappedDsa sizes;
} ThemeFile;

static HDSA themeFiles = NULL;
static int themeFilesCount = 0;

static int CALLBACK theme_dsa_destroy_callback (LPVOID p, LPVOID pData)
{
    ThemeFile* item = (ThemeFile*)p;
    HeapFree (GetProcessHeap(), 0, item->themeFileName);
    HeapFree (GetProcessHeap(), 0, item->fancyName);
    free_color_or_size_dsa (&item->colors);
    free_color_or_size_dsa (&item->sizes);
    return 1;
}

/* Free memory occupied by the theme list */
static void free_theme_files(void)
{
    if (themeFiles == NULL) return;
      
    DSA_DestroyCallback (themeFiles , theme_dsa_destroy_callback, NULL);
    themeFiles = NULL;
    themeFilesCount = 0;
}

typedef HRESULT (WINAPI * EnumTheme) (LPWSTR, LPWSTR, DWORD, PTHEMENAMES);

/* fill a string list with either colors or sizes of a theme */
static void fill_theme_string_array (const WCHAR* filename, 
				     WrappedDsa* wdsa,
				     EnumTheme enumTheme)
{
    DWORD index = 0;
    THEMENAMES names;

    WINE_TRACE ("%s %p %p\n", wine_dbgstr_w (filename), wdsa, enumTheme);

    while (SUCCEEDED (enumTheme ((WCHAR*)filename, NULL, index++, &names)))
    {
	WINE_TRACE ("%s: %s\n", wine_dbgstr_w (names.szName), 
            wine_dbgstr_w (names.szDisplayName));
	color_or_size_dsa_add (wdsa, names.szName, names.szDisplayName);
    }
}

/* Theme enumeration callback, adds theme to theme list */
static BOOL CALLBACK myEnumThemeProc (LPVOID lpReserved, 
				      LPCWSTR pszThemeFileName,
				      LPCWSTR pszThemeName, 
				      LPCWSTR pszToolTip, 
				      LPVOID lpReserved2, LPVOID lpData)
{
    ThemeFile newEntry;

    /* fill size/color lists */
    create_color_or_size_dsa (&newEntry.colors);
    fill_theme_string_array (pszThemeFileName, &newEntry.colors, EnumThemeColors);
    create_color_or_size_dsa (&newEntry.sizes);
    fill_theme_string_array (pszThemeFileName, &newEntry.sizes, EnumThemeSizes);

    newEntry.themeFileName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (pszThemeFileName) + 1) * sizeof(WCHAR));
    lstrcpyW (newEntry.themeFileName, pszThemeFileName);
  
    newEntry.fancyName = HeapAlloc (GetProcessHeap(), 0, 
	(lstrlenW (pszThemeName) + 1) * sizeof(WCHAR));
    lstrcpyW (newEntry.fancyName, pszThemeName);
  
    /*list_add_tail (&themeFiles, &newEntry->entry);*/
    DSA_InsertItem (themeFiles, themeFilesCount, &newEntry);
    themeFilesCount++;

    return TRUE;
}

/* Scan for themes */
static void scan_theme_files(void)
{
    static const WCHAR themesSubdir[] = { '\\','T','h','e','m','e','s',0 };
    WCHAR themesPath[MAX_PATH];

    free_theme_files();

    if (FAILED (SHGetFolderPathW (NULL, CSIDL_RESOURCES, NULL, 
        SHGFP_TYPE_CURRENT, themesPath))) return;

    themeFiles = DSA_Create (sizeof (ThemeFile), 1);
    lstrcatW (themesPath, themesSubdir);

    EnumThemes (themesPath, myEnumThemeProc, 0);
}

/* fill the color & size combo boxes for a given theme */
static void fill_color_size_combos (ThemeFile* theme, HWND comboColor, 
                                    HWND comboSize)
{
    int i;

    SendMessageW (comboColor, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < theme->colors.count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (&theme->colors, i);
	SendMessageW (comboColor, CB_ADDSTRING, 0, (LPARAM)item->fancyName);
    }

    SendMessageW (comboSize, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < theme->sizes.count; i++)
    {
	ThemeColorOrSize* item = color_or_size_dsa_get (&theme->sizes, i);
	SendMessageW (comboSize, CB_ADDSTRING, 0, (LPARAM)item->fancyName);
    }
}

/* Select the item of a combo box that matches a theme's color and size 
 * scheme. */
static void select_color_and_size (ThemeFile* theme, 
			    const WCHAR* colorName, HWND comboColor, 
			    const WCHAR* sizeName, HWND comboSize)
{
    SendMessageW (comboColor, CB_SETCURSEL, 
	color_or_size_dsa_find (&theme->colors, colorName), 0);
    SendMessageW (comboSize, CB_SETCURSEL, 
	color_or_size_dsa_find (&theme->sizes, sizeName), 0);
}

/* Fill theme, color and sizes combo boxes with the know themes and select
 * the entries matching the currently active theme. */
static BOOL fill_theme_list (HWND comboTheme, HWND comboColor, HWND comboSize)
{
    WCHAR textNoTheme[256];
    int themeIndex = 0;
    BOOL ret = TRUE;
    int i;
    WCHAR currentTheme[MAX_PATH];
    WCHAR currentColor[MAX_PATH];
    WCHAR currentSize[MAX_PATH];
    ThemeFile* theme = NULL;

    LoadStringW (GetModuleHandle (NULL), IDS_NOTHEME, textNoTheme,
	sizeof(textNoTheme) / sizeof(WCHAR));

    SendMessageW (comboTheme, CB_RESETCONTENT, 0, 0);
    SendMessageW (comboTheme, CB_ADDSTRING, 0, (LPARAM)textNoTheme);

    for (i = 0; i < themeFilesCount; i++)
    {
	ThemeFile* item = (ThemeFile*)DSA_GetItemPtr (themeFiles, i);
	SendMessageW (comboTheme, CB_ADDSTRING, 0, 
	    (LPARAM)item->fancyName);
    }
  
    if (IsThemeActive () && SUCCEEDED (GetCurrentThemeName (currentTheme, 
	    sizeof(currentTheme) / sizeof(WCHAR),
	    currentColor, sizeof(currentColor) / sizeof(WCHAR),
	    currentSize, sizeof(currentSize) / sizeof(WCHAR))))
    {
	/* Determine the index of the currently active theme. */
	BOOL found = FALSE;
	for (i = 0; i < themeFilesCount; i++)
	{
	    theme = (ThemeFile*)DSA_GetItemPtr (themeFiles, i);
	    if (lstrcmpiW (theme->themeFileName, currentTheme) == 0)
	    {
		found = TRUE;
		themeIndex = i+1;
		break;
	    }
	}
	if (!found)
	{
	    /* Current theme not found?... add to the list, then... */
	    WINE_TRACE("Theme %s not in list of enumerated themes",
		wine_dbgstr_w (currentTheme));
	    myEnumThemeProc (NULL, currentTheme, currentTheme, 
		currentTheme, NULL, NULL);
	    themeIndex = themeFilesCount;
	    theme = (ThemeFile*)DSA_GetItemPtr (themeFiles, 
		themeFilesCount-1);
	}
	fill_color_size_combos (theme, comboColor, comboSize);
	select_color_and_size (theme, currentColor, comboColor,
	    currentSize, comboSize);
    }
    else
    {
	/* No theme selected */
	ret = FALSE;
    }

    SendMessageW (comboTheme, CB_SETCURSEL, themeIndex, 0);
    return ret;
}

/* Update the color & size combo boxes when the selection of the theme
 * combo changed. Selects the current color and size scheme if the theme
 * is currently active, otherwise the first color and size. */
static BOOL update_color_and_size (int themeIndex, HWND comboColor, 
				   HWND comboSize)
{
    if (themeIndex == 0)
    {
	return FALSE;
    }
    else
    {
	WCHAR currentTheme[MAX_PATH];
	WCHAR currentColor[MAX_PATH];
	WCHAR currentSize[MAX_PATH];
	ThemeFile* theme = 
	    (ThemeFile*)DSA_GetItemPtr (themeFiles, themeIndex - 1);
    
	fill_color_size_combos (theme, comboColor, comboSize);
      
	if ((SUCCEEDED (GetCurrentThemeName (currentTheme, 
	    sizeof(currentTheme) / sizeof(WCHAR),
	    currentColor, sizeof(currentColor) / sizeof(WCHAR),
	    currentSize, sizeof(currentSize) / sizeof(WCHAR))))
	    && (lstrcmpiW (currentTheme, theme->themeFileName) == 0))
	{
	    select_color_and_size (theme, currentColor, comboColor,
		currentSize, comboSize);
	}
	else
	{
	    SendMessageW (comboColor, CB_SETCURSEL, 0, 0);
	    SendMessageW (comboSize, CB_SETCURSEL, 0, 0);
	}
    }
    return TRUE;
}

/* Apply a theme from a given theme, color and size combo box item index. */
static void do_apply_theme (int themeIndex, int colorIndex, int sizeIndex)
{
    static char b[] = "\0";

    if (themeIndex == 0)
    {
	/* no theme */
	ApplyTheme (NULL, b, NULL);
    }
    else
    {
	ThemeFile* theme = 
	    (ThemeFile*)DSA_GetItemPtr (themeFiles, themeIndex-1);
	const WCHAR* themeFileName = theme->themeFileName;
	const WCHAR* colorName = NULL;
	const WCHAR* sizeName = NULL;
	HTHEMEFILE hTheme;
	ThemeColorOrSize* item;
    
	item = color_or_size_dsa_get (&theme->colors, colorIndex);
	colorName = item->name;
	
	item = color_or_size_dsa_get (&theme->sizes, sizeIndex);
	sizeName = item->name;
	
	if (SUCCEEDED (OpenThemeFile (themeFileName, colorName, sizeName,
	    &hTheme, 0)))
	{
	    ApplyTheme (hTheme, b, NULL);
	    CloseThemeFile (hTheme);
	}
	else
	{
	    ApplyTheme (NULL, b, NULL);
	}
    }
}

int updating_ui;
BOOL theme_dirty;

static void enable_size_and_color_controls (HWND dialog, BOOL enable)
{
    EnableWindow (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_COLORTEXT), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), enable);
    EnableWindow (GetDlgItem (dialog, IDC_THEME_SIZETEXT), enable);
}
  
static void init_dialog (HWND dialog)
{
    updating_ui = TRUE;
    
    scan_theme_files();
    if (!fill_theme_list (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
    {
        SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        enable_size_and_color_controls (dialog, FALSE);
    }
    else
    {
        enable_size_and_color_controls (dialog, TRUE);
    }
    theme_dirty = FALSE;
    
    updating_ui = FALSE;
}

static void on_theme_changed(HWND dialog) {
    int index = SendMessageW (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        CB_GETCURSEL, 0, 0);
    if (!update_color_and_size (index, GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
    {
        SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
        enable_size_and_color_controls (dialog, FALSE);
    }
    else
    {
        enable_size_and_color_controls (dialog, TRUE);
    }
    theme_dirty = TRUE;
}

static void apply_theme(HWND dialog)
{
    int themeIndex, colorIndex, sizeIndex;

    if (!theme_dirty) return;

    themeIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
        CB_GETCURSEL, 0, 0);
    colorIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
        CB_GETCURSEL, 0, 0);
    sizeIndex = SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO),
        CB_GETCURSEL, 0, 0);

    do_apply_theme (themeIndex, colorIndex, sizeIndex);
    theme_dirty = FALSE;
}

static void on_theme_install(HWND dialog)
{
  static const WCHAR filterMask[] = {0,'*','.','m','s','s','t','y','l','e','s',0,0};
  const int filterMaskLen = sizeof(filterMask)/sizeof(filterMask[0]);
  OPENFILENAMEW ofn;
  WCHAR filetitle[MAX_PATH];
  WCHAR file[MAX_PATH];
  WCHAR filter[100];
  WCHAR title[100];

  LoadStringW (GetModuleHandle (NULL), IDS_THEMEFILE, 
      filter, sizeof (filter) / sizeof (filter[0]) - filterMaskLen);
  memcpy (filter + lstrlenW (filter), filterMask, 
      filterMaskLen * sizeof (WCHAR));
  LoadStringW (GetModuleHandle (NULL), IDS_THEMEFILE_SELECT, 
      title, sizeof (title) / sizeof (title[0]));

  ofn.lStructSize = sizeof(OPENFILENAMEW);
  ofn.hwndOwner = 0;
  ofn.hInstance = 0;
  ofn.lpstrFilter = filter;
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = file;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(file)/sizeof(filetitle[0]);
  ofn.lpstrFileTitle = filetitle;
  ofn.lpstrFileTitle[0] = '\0';
  ofn.nMaxFileTitle = sizeof(filetitle)/sizeof(filetitle[0]);
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = title;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lCustData = 0;
  ofn.lpfnHook = NULL;
  ofn.lpTemplateName = NULL;

  if (GetOpenFileNameW(&ofn))
  {
      static const WCHAR themesSubdir[] = { '\\','T','h','e','m','e','s',0 };
      static const WCHAR backslash[] = { '\\',0 };
      WCHAR themeFilePath[MAX_PATH];
      SHFILEOPSTRUCTW shfop;

      if (FAILED (SHGetFolderPathW (NULL, CSIDL_RESOURCES|CSIDL_FLAG_CREATE, NULL, 
          SHGFP_TYPE_CURRENT, themeFilePath))) return;

      PathRemoveExtensionW (filetitle);

      /* Construct path into which the theme file goes */
      lstrcatW (themeFilePath, themesSubdir);
      lstrcatW (themeFilePath, backslash);
      lstrcatW (themeFilePath, filetitle);

      /* Create the directory */
      SHCreateDirectoryExW (dialog, themeFilePath, NULL);

      /* Append theme file name itself */
      lstrcatW (themeFilePath, backslash);
      lstrcatW (themeFilePath, PathFindFileNameW (file));
      /* SHFileOperation() takes lists as input, so double-nullterminate */
      themeFilePath[lstrlenW (themeFilePath)+1] = 0;
      file[lstrlenW (file)+1] = 0;

      /* Do the copying */
      WINE_TRACE("copying: %s -> %s\n", wine_dbgstr_w (file), 
          wine_dbgstr_w (themeFilePath));
      shfop.hwnd = dialog;
      shfop.wFunc = FO_COPY;
      shfop.pFrom = file;
      shfop.pTo = themeFilePath;
      shfop.fFlags = FOF_NOCONFIRMMKDIR;
      if (SHFileOperationW (&shfop) == 0)
      {
          scan_theme_files();
          if (!fill_theme_list (GetDlgItem (dialog, IDC_THEME_THEMECOMBO),
              GetDlgItem (dialog, IDC_THEME_COLORCOMBO),
              GetDlgItem (dialog, IDC_THEME_SIZECOMBO)))
          {
              SendMessageW (GetDlgItem (dialog, IDC_THEME_COLORCOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
              SendMessageW (GetDlgItem (dialog, IDC_THEME_SIZECOMBO), CB_SETCURSEL, (WPARAM)-1, 0);
              enable_size_and_color_controls (dialog, FALSE);
          }
          else
          {
              enable_size_and_color_controls (dialog, TRUE);
          }
      }
      else
          WINE_TRACE("copy operation failed\n");
  }
  else WINE_TRACE("user cancelled\n");
}

/* Information about symbolic link targets of certain User Shell Folders. */
struct ShellFolderInfo {
    int nFolder;
    char szLinkTarget[FILENAME_MAX];
};

static struct ShellFolderInfo asfiInfo[] = {
    { CSIDL_DESKTOP,  "" },
    { CSIDL_PERSONAL, "" },
    { CSIDL_MYPICTURES, "" },
    { CSIDL_MYMUSIC, "" },
    { CSIDL_MYVIDEO, "" }
};

static struct ShellFolderInfo *psfiSelected = NULL;

#define NUM_ELEMS(x) (sizeof(x)/sizeof(*(x)))

static void init_shell_folder_listview_headers(HWND dialog) {
    LVCOLUMN listColumn;
    RECT viewRect;
    char szShellFolder[64] = "Shell Folder";
    char szLinksTo[64] = "Links to";
    int width;

    LoadString(GetModuleHandle(NULL), IDS_SHELL_FOLDER, szShellFolder, sizeof(szShellFolder));
    LoadString(GetModuleHandle(NULL), IDS_LINKS_TO, szLinksTo, sizeof(szLinksTo));
    
    GetClientRect(GetDlgItem(dialog, IDC_LIST_SFPATHS), &viewRect);
    width = (viewRect.right - viewRect.left) / 4;

    listColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    listColumn.pszText = szShellFolder;
    listColumn.cchTextMax = lstrlen(listColumn.pszText);
    listColumn.cx = width;

    SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_INSERTCOLUMN, 0, (LPARAM) &listColumn);

    listColumn.pszText = szLinksTo;
    listColumn.cchTextMax = lstrlen(listColumn.pszText);
    listColumn.cx = viewRect.right - viewRect.left - width - 1;

    SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_INSERTCOLUMN, 1, (LPARAM) &listColumn);
}

/* Reads the currently set shell folder symbol link targets into asfiInfo. */
static void read_shell_folder_link_targets() {
    WCHAR wszPath[MAX_PATH];
    HRESULT hr;
    int i;
   
    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        asfiInfo[i].szLinkTarget[0] = '\0';
        hr = SHGetFolderPathW(NULL, asfiInfo[i].nFolder|CSIDL_FLAG_DONT_VERIFY, NULL, 
                              SHGFP_TYPE_CURRENT, wszPath);
        if (SUCCEEDED(hr)) {
            char *pszUnixPath = wine_get_unix_file_name(wszPath);
            if (pszUnixPath) {
                struct stat statPath;
                if (!lstat(pszUnixPath, &statPath) && S_ISLNK(statPath.st_mode)) {
                    int cLen = readlink(pszUnixPath, asfiInfo[i].szLinkTarget, FILENAME_MAX-1);
                    if (cLen >= 0) asfiInfo[i].szLinkTarget[cLen] = '\0';
                }
                HeapFree(GetProcessHeap(), 0, pszUnixPath);
            }
        } 
    }    
}

static void update_shell_folder_listview(HWND dialog) {
    int i;
    LVITEM item;
    LONG lSelected = SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_GETNEXTITEM, (WPARAM)-1, 
                                        MAKELPARAM(LVNI_SELECTED,0));
    
    SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_DELETEALLITEMS, 0, 0);

    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        char buffer[MAX_PATH];
        HRESULT hr;
        LPITEMIDLIST pidlCurrent;

        /* Some acrobatic to get the localized name of the shell folder */
        hr = SHGetFolderLocation(dialog, asfiInfo[i].nFolder, NULL, 0, &pidlCurrent);
        if (SUCCEEDED(hr)) { 
            LPSHELLFOLDER psfParent;
            LPCITEMIDLIST pidlLast;
            hr = SHBindToParent(pidlCurrent, &IID_IShellFolder, (LPVOID*)&psfParent, &pidlLast);
            if (SUCCEEDED(hr)) {
                STRRET strRet;
                hr = IShellFolder_GetDisplayNameOf(psfParent, pidlLast, SHGDN_FORADDRESSBAR, &strRet);
                if (SUCCEEDED(hr)) {
                    hr = StrRetToBufA(&strRet, pidlLast, buffer, 256);
                }
                IShellFolder_Release(psfParent);
            }
            ILFree(pidlCurrent);
        }

        /* If there's a dangling symlink for the current shell folder, SHGetFolderLocation
         * will fail above. We fall back to the (non-verified) path of the shell folder. */
        if (FAILED(hr)) {
            hr = SHGetFolderPath(dialog, asfiInfo[i].nFolder|CSIDL_FLAG_DONT_VERIFY, NULL, 
                                 SHGFP_TYPE_CURRENT, buffer);
        }
    
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = buffer;
        item.lParam = (LPARAM)&asfiInfo[i];
        SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_INSERTITEM, 0, (LPARAM)&item);

        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 1;
        item.pszText = asfiInfo[i].szLinkTarget;
        SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_SETITEM, 0, (LPARAM)&item);
    }

    /* Ensure that the previously selected item is selected again. */
    if (lSelected >= 0) {
        item.mask = LVIF_STATE;
        item.state = LVIS_SELECTED;
        item.stateMask = LVIS_SELECTED;
        SendDlgItemMessage(dialog, IDC_LIST_SFPATHS, LVM_SETITEMSTATE, (WPARAM)lSelected, 
                           (LPARAM)&item);
    }
}

static void on_shell_folder_selection_changed(HWND hDlg, LPNMLISTVIEW lpnm) {
    if (lpnm->uNewState & LVIS_SELECTED) {
        psfiSelected = (struct ShellFolderInfo *)lpnm->lParam;
        EnableWindow(GetDlgItem(hDlg, IDC_LINK_SFPATH), 1);
        if (strlen(psfiSelected->szLinkTarget)) {
            CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_CHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 1);
            EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 1);
            SetWindowText(GetDlgItem(hDlg, IDC_EDIT_SFPATH), psfiSelected->szLinkTarget);
        } else {
            CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 0);
            EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 0);
            SetWindowText(GetDlgItem(hDlg, IDC_EDIT_SFPATH), "");
        }
    } else {
        psfiSelected = NULL;
        CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
        SetWindowText(GetDlgItem(hDlg, IDC_EDIT_SFPATH), "");
        EnableWindow(GetDlgItem(hDlg, IDC_LINK_SFPATH), 0);
        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_SFPATH), 0);
        EnableWindow(GetDlgItem(hDlg, IDC_BROWSE_SFPATH), 0);
    }
}

/* Keep the contents of the edit control, the listview control and the symlink 
 * information in sync. */
static void on_shell_folder_edit_changed(HWND hDlg) {
    LVITEM item;
    char *text = get_text(hDlg, IDC_EDIT_SFPATH);
    LONG iSel = SendDlgItemMessage(hDlg, IDC_LIST_SFPATHS, LVM_GETNEXTITEM, -1,
                                   MAKELPARAM(LVNI_SELECTED,0));
    
    if (!text || !psfiSelected || iSel < 0) {
        HeapFree(GetProcessHeap(), 0, text);
        return;
    }

    strncpy(psfiSelected->szLinkTarget, text, FILENAME_MAX);
    HeapFree(GetProcessHeap(), 0, text);

    item.mask = LVIF_TEXT;
    item.iItem = iSel;
    item.iSubItem = 1;
    item.pszText = psfiSelected->szLinkTarget;
    SendDlgItemMessage(hDlg, IDC_LIST_SFPATHS, LVM_SETITEM, 0, (LPARAM)&item);

    SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
}

static void apply_shell_folder_changes() {
    WCHAR wszPath[MAX_PATH];
    char szBackupPath[FILENAME_MAX], szUnixPath[FILENAME_MAX], *pszUnixPath = NULL;
    int i, cUnixPathLen;
    struct stat statPath;
    HRESULT hr;

    for (i=0; i<NUM_ELEMS(asfiInfo); i++) {
        /* Ignore nonexistent link targets */
        if (asfiInfo[i].szLinkTarget[0] && stat(asfiInfo[i].szLinkTarget, &statPath))
            continue;
        
        hr = SHGetFolderPathW(NULL, asfiInfo[i].nFolder|CSIDL_FLAG_CREATE, NULL, 
                              SHGFP_TYPE_CURRENT, wszPath);
        if (FAILED(hr)) continue;

        /* Retrieve the corresponding unix path. */
        pszUnixPath = wine_get_unix_file_name(wszPath);
        if (!pszUnixPath) continue;
        lstrcpyA(szUnixPath, pszUnixPath);
        HeapFree(GetProcessHeap(), 0, pszUnixPath);
            
        /* Derive name for folder backup. */
        cUnixPathLen = lstrlenA(szUnixPath);    
        lstrcpyA(szBackupPath, szUnixPath);
        lstrcatA(szBackupPath, ".winecfg");
        
        if (lstat(szUnixPath, &statPath)) continue;
    
        /* Move old folder/link out of the way. */
        if (S_ISLNK(statPath.st_mode)) {
            if (unlink(szUnixPath)) continue; /* Unable to remove link. */
        } else { 
            if (!*asfiInfo[i].szLinkTarget) {
                continue; /* We are done. Old was real folder, as new shall be. */
            } else { 
                if (rename(szUnixPath, szBackupPath)) { /* Move folder out of the way. */
                    continue; /* Unable to move old folder. */
                }
            }
        }
    
        /* Create new link/folder. */
        if (*asfiInfo[i].szLinkTarget) {
            symlink(asfiInfo[i].szLinkTarget, szUnixPath);
        } else {
            /* If there's a backup folder, restore it. Else create new folder. */
            if (!lstat(szBackupPath, &statPath) && S_ISDIR(statPath.st_mode)) {
                rename(szBackupPath, szUnixPath);
            } else {
                mkdir(szUnixPath, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
            }
        }
    }
}

INT_PTR CALLBACK
ThemeDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_INITDIALOG:
            read_shell_folder_link_targets();
            init_shell_folder_listview_headers(hDlg);
            update_shell_folder_listview(hDlg);
            break;
        
        case WM_DESTROY:
            free_theme_files();
            break;

        case WM_SHOWWINDOW:
            set_window_title(hDlg);
            break;
            
        case WM_COMMAND:
            switch(HIWORD(wParam)) {
                case CBN_SELCHANGE: {
                    if (updating_ui) break;
                    SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
                    switch (LOWORD(wParam))
                    {
                        case IDC_THEME_THEMECOMBO: on_theme_changed(hDlg); break;
                        case IDC_THEME_COLORCOMBO: /* fall through */
                        case IDC_THEME_SIZECOMBO: theme_dirty = TRUE; break;
                    }
                    break;
                }
                case EN_CHANGE: {
                    if (LOWORD(wParam) == IDC_EDIT_SFPATH) 
                        on_shell_folder_edit_changed(hDlg);
                    break;
                }
                case BN_CLICKED:
                    switch (LOWORD(wParam))
                    {
                        case IDC_THEME_INSTALL:
                            on_theme_install (hDlg);
                            break;

                        case IDC_BROWSE_SFPATH:
                            if (browse_for_unix_folder(hDlg, psfiSelected->szLinkTarget)) {
                                update_shell_folder_listview(hDlg);
                                SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            }
                            break;

                        case IDC_LINK_SFPATH:
                            if (IsDlgButtonChecked(hDlg, IDC_LINK_SFPATH)) {
                                if (browse_for_unix_folder(hDlg, psfiSelected->szLinkTarget)) {
                                    update_shell_folder_listview(hDlg);
                                    SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
                                } else {
                                    CheckDlgButton(hDlg, IDC_LINK_SFPATH, BST_UNCHECKED);
                                }
                            } else {
                                psfiSelected->szLinkTarget[0] = '\0';
                                update_shell_folder_listview(hDlg);
                                SendMessage(GetParent(hDlg), PSM_CHANGED, 0, 0);
                            }
                            break;    
                    }
                    break;
            }
            break;
        
        case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code) {
                case PSN_KILLACTIVE: {
                    SetWindowLongPtr(hDlg, DWLP_MSGRESULT, FALSE);
                    break;
                }
                case PSN_APPLY: {
                    apply();
                    apply_theme(hDlg);
                    apply_shell_folder_changes();
                    read_shell_folder_link_targets();
                    update_shell_folder_listview(hDlg);
                    SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                    break;
                }
                case LVN_ITEMCHANGED: { 
                    if (wParam == IDC_LIST_SFPATHS)  
                        on_shell_folder_selection_changed(hDlg, (LPNMLISTVIEW)lParam);
                    break;
                }
                case PSN_SETACTIVE: {
                    init_dialog (hDlg);
                    break;
                }
            }
            break;

        default:
            break;
    }
    return FALSE;
}
