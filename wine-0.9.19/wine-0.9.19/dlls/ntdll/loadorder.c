/*
 * Dlls load order support
 *
 * Copyright 1999 Bertho Stultiens
 * Copyright 2003 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "windef.h"
#include "winternl.h"
#include "ntdll_misc.h"

#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(module);

#define LOADORDER_ALLOC_CLUSTER	32	/* Allocate with 32 entries at a time */

typedef struct module_loadorder
{
    const WCHAR        *modulename;
    enum loadorder      loadorder;
} module_loadorder_t;

struct loadorder_list
{
    int                 count;
    int                 alloc;
    module_loadorder_t *order;
};

static const WCHAR separatorsW[] = {',',' ','\t',0};

static int init_done;
static struct loadorder_list env_list;


/***************************************************************************
 *	cmp_sort_func	(internal, static)
 *
 * Sorting and comparing function used in sort and search of loadorder
 * entries.
 */
static int cmp_sort_func(const void *s1, const void *s2)
{
    return strcmpiW(((const module_loadorder_t *)s1)->modulename, ((const module_loadorder_t *)s2)->modulename);
}


/***************************************************************************
 *	get_basename
 *
 * Return the base name of a file name (i.e. remove the path components).
 */
static const WCHAR *get_basename( const WCHAR *name )
{
    const WCHAR *ptr;

    if (name[0] && name[1] == ':') name += 2;  /* strip drive specification */
    if ((ptr = strrchrW( name, '\\' ))) name = ptr + 1;
    if ((ptr = strrchrW( name, '/' ))) name = ptr + 1;
    return name;
}

/***************************************************************************
 *	remove_dll_ext
 *
 * Remove extension if it is ".dll".
 */
static inline void remove_dll_ext( WCHAR *ext )
{
    if (ext[0] == '.' &&
        toupperW(ext[1]) == 'D' &&
        toupperW(ext[2]) == 'L' &&
        toupperW(ext[3]) == 'L' &&
        !ext[4]) ext[0] = 0;
}


/***************************************************************************
 *	debugstr_loadorder
 *
 * Return a loadorder in printable form.
 */
static const char *debugstr_loadorder( enum loadorder lo )
{
    switch(lo)
    {
    case LO_DISABLED: return "";
    case LO_NATIVE: return "n";
    case LO_BUILTIN: return "b";
    case LO_NATIVE_BUILTIN: return "n,b";
    case LO_BUILTIN_NATIVE: return "b,n";
    case LO_DEFAULT: return "default";
    default: return "??";
    }
}


/***************************************************************************
 *	parse_load_order
 *
 * Parses the loadorder options from the configuration and puts it into
 * a structure.
 */
static enum loadorder parse_load_order( const WCHAR *order )
{
    enum loadorder ret = LO_DISABLED;

    while (*order)
    {
        order += strspnW( order, separatorsW );
        switch(*order)
        {
        case 'N':  /* native */
        case 'n':
            if (ret == LO_DISABLED) ret = LO_NATIVE;
            else if (ret == LO_BUILTIN) return LO_BUILTIN_NATIVE;
            break;
        case 'B':  /* builtin */
        case 'b':
            if (ret == LO_DISABLED) ret = LO_BUILTIN;
            else if (ret == LO_NATIVE) return LO_NATIVE_BUILTIN;
            break;
        }
        order += strcspnW( order, separatorsW );
    }
    return ret;
}


/***************************************************************************
 *	add_load_order
 *
 * Adds an entry in the list of environment overrides.
 */
static void add_load_order( const module_loadorder_t *plo )
{
    int i;

    for(i = 0; i < env_list.count; i++)
    {
        if(!cmp_sort_func(plo, &env_list.order[i] ))
        {
            /* replace existing option */
            env_list.order[i].loadorder = plo->loadorder;
            return;
        }
    }

    if (i >= env_list.alloc)
    {
        /* No space in current array, make it larger */
        env_list.alloc += LOADORDER_ALLOC_CLUSTER;
        if (env_list.order)
            env_list.order = RtlReAllocateHeap(GetProcessHeap(), 0, env_list.order,
                                               env_list.alloc * sizeof(module_loadorder_t));
        else
            env_list.order = RtlAllocateHeap(GetProcessHeap(), 0,
                                             env_list.alloc * sizeof(module_loadorder_t));
        if(!env_list.order)
        {
            MESSAGE("Virtual memory exhausted\n");
            exit(1);
        }
    }
    env_list.order[i].loadorder  = plo->loadorder;
    env_list.order[i].modulename = plo->modulename;
    env_list.count++;
}


/***************************************************************************
 *	add_load_order_set
 *
 * Adds a set of entries in the list of command-line overrides from the key parameter.
 */
static void add_load_order_set( WCHAR *entry )
{
    module_loadorder_t ldo;
    WCHAR *end = strchrW( entry, '=' );

    if (!end) return;
    *end++ = 0;
    ldo.loadorder = parse_load_order( end );

    while (*entry)
    {
        entry += strspnW( entry, separatorsW );
        end = entry + strcspnW( entry, separatorsW );
        if (*end) *end++ = 0;
        if (*entry)
        {
            WCHAR *ext = strrchrW(entry, '.');
            if (ext) remove_dll_ext( ext );
            ldo.modulename = entry;
            add_load_order( &ldo );
            entry = end;
        }
    }
}


/***************************************************************************
 *	init_load_order
 */
static void init_load_order(void)
{
    const char *order = getenv( "WINEDLLOVERRIDES" );
    UNICODE_STRING strW;
    WCHAR *entry, *next;

    init_done = 1;
    if (!order) return;

    if (!strcmp( order, "help" ))
    {
        MESSAGE( "Syntax:\n"
                 "  WINEDLLOVERRIDES=\"entry;entry;entry...\"\n"
                 "    where each entry is of the form:\n"
                 "        module[,module...]={native|builtin}[,{b|n}]\n"
                 "\n"
                 "    Only the first letter of the override (native or builtin)\n"
                 "    is significant.\n\n"
                 "Example:\n"
                 "  WINEDLLOVERRIDES=\"comdlg32=n,b;shell32,shlwapi=b\"\n" );
        exit(0);
    }

    RtlCreateUnicodeStringFromAsciiz( &strW, order );
    entry = strW.Buffer;
    while (*entry)
    {
        while (*entry && *entry == ';') entry++;
        if (!*entry) break;
        next = strchrW( entry, ';' );
        if (next) *next++ = 0;
        else next = entry + strlenW(entry);
        add_load_order_set( entry );
        entry = next;
    }

    /* sort the array for quick lookup */
    if (env_list.count)
        qsort(env_list.order, env_list.count, sizeof(env_list.order[0]), cmp_sort_func);

    /* Note: we don't free the Unicode string because the
     * stored module names point inside it */
}


/***************************************************************************
 *	get_env_load_order
 *
 * Get the load order for a given module from the WINEDLLOVERRIDES environment variable.
 */
static inline enum loadorder get_env_load_order( const WCHAR *module )
{
    module_loadorder_t tmp, *res;

    tmp.modulename = module;
    /* some bsearch implementations (Solaris) are buggy when the number of items is 0 */
    if (env_list.count &&
        (res = bsearch(&tmp, env_list.order, env_list.count, sizeof(env_list.order[0]), cmp_sort_func)))
        return res->loadorder;
    return LO_INVALID;
}


/***************************************************************************
 *	get_standard_key
 *
 * Return a handle to the standard DllOverrides registry section.
 */
static HANDLE get_standard_key(void)
{
    static const WCHAR DllOverridesW[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\',
                                          'D','l','l','O','v','e','r','r','i','d','e','s',0};
    static HANDLE std_key = (HANDLE)-1;

    if (std_key == (HANDLE)-1)
    {
        OBJECT_ATTRIBUTES attr;
        UNICODE_STRING nameW;
        HANDLE root;

        RtlOpenCurrentUser( KEY_ALL_ACCESS, &root );
        attr.Length = sizeof(attr);
        attr.RootDirectory = root;
        attr.ObjectName = &nameW;
        attr.Attributes = 0;
        attr.SecurityDescriptor = NULL;
        attr.SecurityQualityOfService = NULL;
        RtlInitUnicodeString( &nameW, DllOverridesW );

        /* @@ Wine registry key: HKCU\Software\Wine\DllOverrides */
        if (NtOpenKey( &std_key, KEY_ALL_ACCESS, &attr )) std_key = 0;
        NtClose( root );
    }
    return std_key;
}


/***************************************************************************
 *	get_app_key
 *
 * Get the registry key for the app-specific DllOverrides list.
 */
static HANDLE get_app_key( const WCHAR *app_name )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HANDLE root;
    WCHAR *str;
    static const WCHAR AppDefaultsW[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\',
                                         'A','p','p','D','e','f','a','u','l','t','s','\\',0};
    static const WCHAR DllOverridesW[] = {'\\','D','l','l','O','v','e','r','r','i','d','e','s',0};
    static HANDLE app_key = (HANDLE)-1;

    if (app_key != (HANDLE)-1) return app_key;

    str = RtlAllocateHeap( GetProcessHeap(), 0,
                           sizeof(AppDefaultsW) + sizeof(DllOverridesW) +
                           strlenW(app_name) * sizeof(WCHAR) );
    if (!str) return 0;
    strcpyW( str, AppDefaultsW );
    strcatW( str, app_name );
    strcatW( str, DllOverridesW );

    RtlOpenCurrentUser( KEY_ALL_ACCESS, &root );
    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, str );

    /* @@ Wine registry key: HKCU\Software\Wine\AppDefaults\app.exe\DllOverrides */
    if (NtOpenKey( &app_key, KEY_ALL_ACCESS, &attr )) app_key = 0;
    NtClose( root );
    RtlFreeHeap( GetProcessHeap(), 0, str );
    return app_key;
}


/***************************************************************************
 *	get_registry_value
 *
 * Load the registry loadorder value for a given module.
 */
static enum loadorder get_registry_value( HANDLE hkey, const WCHAR *module )
{
    UNICODE_STRING valueW;
    char buffer[80];
    DWORD count;
    BOOL ret;

    RtlInitUnicodeString( &valueW, module );

    if ((ret = !NtQueryValueKey( hkey, &valueW, KeyValuePartialInformation,
                                 buffer, sizeof(buffer), &count )))
    {
        WCHAR *str = (WCHAR *)((KEY_VALUE_PARTIAL_INFORMATION *)buffer)->Data;
        return parse_load_order( str );
    }
    return LO_INVALID;
}


/***************************************************************************
 *	get_load_order_value
 *
 * Get the load order for the exact specified module string, looking in:
 * 1. The WINEDLLOVERRIDES environment variable
 * 2. The per-application DllOverrides key
 * 3. The standard DllOverrides key
 */
static enum loadorder get_load_order_value( HANDLE std_key, HANDLE app_key, const WCHAR *module )
{
    enum loadorder ret;

    if ((ret = get_env_load_order( module )) != LO_INVALID)
    {
        TRACE( "got environment %s for %s\n", debugstr_loadorder(ret), debugstr_w(module) );
        return ret;
    }

    if (app_key && ((ret = get_registry_value( app_key, module )) != LO_INVALID))
    {
        TRACE( "got app defaults %s for %s\n", debugstr_loadorder(ret), debugstr_w(module) );
        return ret;
    }

    if (std_key && ((ret = get_registry_value( std_key, module )) != LO_INVALID))
    {
        TRACE( "got standard key %s for %s\n", debugstr_loadorder(ret), debugstr_w(module) );
        return ret;
    }

    return ret;
}


/***************************************************************************
 *	get_load_order   (internal)
 *
 * Return the loadorder of a module.
 * The system directory and '.dll' extension is stripped from the path.
 */
enum loadorder get_load_order( const WCHAR *app_name, const WCHAR *path )
{
    enum loadorder ret = LO_INVALID;
    HANDLE std_key, app_key = 0;
    WCHAR *module, *basename;
    UNICODE_STRING path_str;
    int len;

    if (!init_done) init_load_order();
    std_key = get_standard_key();
    if (app_name) app_key = get_app_key( app_name );

    TRACE("looking for %s\n", debugstr_w(path));

    /* Strip path information if the module resides in the system directory
     */
    RtlInitUnicodeString( &path_str, path );
    if (RtlPrefixUnicodeString( &system_dir, &path_str, TRUE ))
    {
        const WCHAR *p = path + system_dir.Length / sizeof(WCHAR);
        while (*p == '\\' || *p == '/') p++;
        if (!strchrW( p, '\\' ) && !strchrW( p, '/' )) path = p;
    }

    if (!(len = strlenW(path))) return ret;
    if (!(module = RtlAllocateHeap( GetProcessHeap(), 0, (len + 2) * sizeof(WCHAR) ))) return ret;
    strcpyW( module+1, path );  /* reserve module[0] for the wildcard char */
    basename = (WCHAR *)get_basename( module+1 );

    if (len >= 4) remove_dll_ext( module + 1 + len - 4 );

    /* first explicit module name */
    if ((ret = get_load_order_value( std_key, app_key, module+1 )) != LO_INVALID)
        goto done;

    /* then module basename preceded by '*' */
    basename[-1] = '*';
    if ((ret = get_load_order_value( std_key, app_key, basename-1 )) != LO_INVALID)
        goto done;

    /* then module basename without '*' (only if explicit path) */
    if (basename != module+1 && ((ret = get_load_order_value( std_key, app_key, basename )) != LO_INVALID))
        goto done;

    /* and last the hard-coded default */
    ret = LO_DEFAULT;
    TRACE( "got hardcoded default %s for %s\n", debugstr_loadorder(ret), debugstr_w(path) );

 done:
    RtlFreeHeap( GetProcessHeap(), 0, module );
    return ret;
}
