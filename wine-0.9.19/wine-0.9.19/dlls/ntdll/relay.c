/*
 * Win32 relay and snoop functions
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1998 Marcus Meissner
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

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winternl.h"
#include "excpt.h"
#include "wine/exception.h"
#include "ntdll_misc.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(relay);
WINE_DECLARE_DEBUG_CHANNEL(snoop);
WINE_DECLARE_DEBUG_CHANNEL(seh);

#ifdef __i386__

struct relay_descr  /* descriptor for a module */
{
    void               *magic;               /* signature */
    void               *relay_from_32;       /* functions to call from relay thunks */
    void               *relay_from_32_regs;
    void               *private;             /* reserved for the relay code private data */
    const char         *entry_point_base;    /* base address of entry point thunks */
    const unsigned int *entry_point_offsets; /* offsets of entry points thunks */
    const unsigned int *arg_types;           /* table of argument types for all entry points */
};

#define RELAY_DESCR_MAGIC  ((void *)0xdeb90001)

/* private data built at dll load time */

struct relay_entry_point
{
    void       *orig_func;    /* original entry point function */
    const char *name;         /* function name (if any) */
};

struct relay_private_data
{
    HMODULE                  module;            /* module handle of this dll */
    unsigned int             base;              /* ordinal base */
    char                     dllname[40];       /* dll name (without .dll extension) */
    struct relay_entry_point entry_points[1];   /* list of dll entry points */
};

static const WCHAR **debug_relay_excludelist;
static const WCHAR **debug_relay_includelist;
static const WCHAR **debug_snoop_excludelist;
static const WCHAR **debug_snoop_includelist;
static const WCHAR **debug_from_relay_excludelist;
static const WCHAR **debug_from_relay_includelist;
static const WCHAR **debug_from_snoop_excludelist;
static const WCHAR **debug_from_snoop_includelist;

static BOOL init_done;

/* compare an ASCII and a Unicode string without depending on the current codepage */
inline static int strcmpAW( const char *strA, const WCHAR *strW )
{
    while (*strA && ((unsigned char)*strA == *strW)) { strA++; strW++; }
    return (unsigned char)*strA - *strW;
}

/* compare an ASCII and a Unicode string without depending on the current codepage */
inline static int strncmpiAW( const char *strA, const WCHAR *strW, int n )
{
    int ret = 0;
    for ( ; n > 0; n--, strA++, strW++)
        if ((ret = toupperW((unsigned char)*strA) - toupperW(*strW)) || !*strA) break;
    return ret;
}

/***********************************************************************
 *           build_list
 *
 * Build a function list from a ';'-separated string.
 */
static const WCHAR **build_list( const WCHAR *buffer )
{
    int count = 1;
    const WCHAR *p = buffer;
    const WCHAR **ret;

    while ((p = strchrW( p, ';' )))
    {
        count++;
        p++;
    }
    /* allocate count+1 pointers, plus the space for a copy of the string */
    if ((ret = RtlAllocateHeap( GetProcessHeap(), 0,
                                (count+1) * sizeof(WCHAR*) + (strlenW(buffer)+1) * sizeof(WCHAR) )))
    {
        WCHAR *str = (WCHAR *)(ret + count + 1);
        WCHAR *p = str;

        strcpyW( str, buffer );
        count = 0;
        for (;;)
        {
            ret[count++] = p;
            if (!(p = strchrW( p, ';' ))) break;
            *p++ = 0;
        }
        ret[count++] = NULL;
    }
    return ret;
}


/***********************************************************************
 *           init_debug_lists
 *
 * Build the relay include/exclude function lists.
 */
static void init_debug_lists(void)
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    char buffer[1024];
    HANDLE root, hkey;
    DWORD count;
    WCHAR *str;
    static const WCHAR configW[] = {'S','o','f','t','w','a','r','e','\\',
                                    'W','i','n','e','\\',
                                    'D','e','b','u','g',0};
    static const WCHAR RelayIncludeW[] = {'R','e','l','a','y','I','n','c','l','u','d','e',0};
    static const WCHAR RelayExcludeW[] = {'R','e','l','a','y','E','x','c','l','u','d','e',0};
    static const WCHAR SnoopIncludeW[] = {'S','n','o','o','p','I','n','c','l','u','d','e',0};
    static const WCHAR SnoopExcludeW[] = {'S','n','o','o','p','E','x','c','l','u','d','e',0};
    static const WCHAR RelayFromIncludeW[] = {'R','e','l','a','y','F','r','o','m','I','n','c','l','u','d','e',0};
    static const WCHAR RelayFromExcludeW[] = {'R','e','l','a','y','F','r','o','m','E','x','c','l','u','d','e',0};
    static const WCHAR SnoopFromIncludeW[] = {'S','n','o','o','p','F','r','o','m','I','n','c','l','u','d','e',0};
    static const WCHAR SnoopFromExcludeW[] = {'S','n','o','o','p','F','r','o','m','E','x','c','l','u','d','e',0};

    if (init_done) return;
    init_done = TRUE;

    RtlOpenCurrentUser( KEY_ALL_ACCESS, &root );
    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &name;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &name, configW );

    /* @@ Wine registry key: HKCU\Software\Wine\Debug */
    if (NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr )) hkey = 0;
    NtClose( root );
    if (!hkey) return;

    str = (WCHAR *)((KEY_VALUE_PARTIAL_INFORMATION *)buffer)->Data;
    RtlInitUnicodeString( &name, RelayIncludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE("RelayInclude = %s\n", debugstr_w(str) );
        debug_relay_includelist = build_list( str );
    }

    RtlInitUnicodeString( &name, RelayExcludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE( "RelayExclude = %s\n", debugstr_w(str) );
        debug_relay_excludelist = build_list( str );
    }

    RtlInitUnicodeString( &name, SnoopIncludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE_(snoop)( "SnoopInclude = %s\n", debugstr_w(str) );
        debug_snoop_includelist = build_list( str );
    }

    RtlInitUnicodeString( &name, SnoopExcludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE_(snoop)( "SnoopExclude = %s\n", debugstr_w(str) );
        debug_snoop_excludelist = build_list( str );
    }

    RtlInitUnicodeString( &name, RelayFromIncludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE("RelayFromInclude = %s\n", debugstr_w(str) );
        debug_from_relay_includelist = build_list( str );
    }

    RtlInitUnicodeString( &name, RelayFromExcludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE( "RelayFromExclude = %s\n", debugstr_w(str) );
        debug_from_relay_excludelist = build_list( str );
    }

    RtlInitUnicodeString( &name, SnoopFromIncludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE_(snoop)("SnoopFromInclude = %s\n", debugstr_w(str) );
        debug_from_snoop_includelist = build_list( str );
    }

    RtlInitUnicodeString( &name, SnoopFromExcludeW );
    if (!NtQueryValueKey( hkey, &name, KeyValuePartialInformation, buffer, sizeof(buffer), &count ))
    {
        TRACE_(snoop)( "SnoopFromExclude = %s\n", debugstr_w(str) );
        debug_from_snoop_excludelist = build_list( str );
    }

    NtClose( hkey );
}


/***********************************************************************
 *           check_list
 *
 * Check if a given module and function is in the list.
 */
static BOOL check_list( const char *module, int ordinal, const char *func, const WCHAR **list )
{
    char ord_str[10];

    sprintf( ord_str, "%d", ordinal );
    for(; *list; list++)
    {
        const WCHAR *p = strrchrW( *list, '.' );
        if (p && p > *list)  /* check module and function */
        {
            int len = p - *list;
            if (strncmpiAW( module, *list, len-1 ) || module[len]) continue;
            if (p[1] == '*' && !p[2]) return TRUE;
            if (!strcmpAW( ord_str, p + 1 )) return TRUE;
            if (func && !strcmpAW( func, p + 1 )) return TRUE;
        }
        else  /* function only */
        {
            if (func && !strcmpAW( func, *list )) return TRUE;
        }
    }
    return FALSE;
}


/***********************************************************************
 *           check_relay_include
 *
 * Check if a given function must be included in the relay output.
 */
static BOOL check_relay_include( const char *module, int ordinal, const char *func )
{
    if (debug_relay_excludelist && check_list( module, ordinal, func, debug_relay_excludelist ))
        return FALSE;
    if (debug_relay_includelist && !check_list( module, ordinal, func, debug_relay_includelist ))
        return FALSE;
    return TRUE;
}

/***********************************************************************
 *           check_from_module
 *
 * Check if calls from a given module must be included in the relay/snoop output,
 * given the exclusion and inclusion lists.
 */
static BOOL check_from_module( const WCHAR **includelist, const WCHAR **excludelist, const WCHAR *module )
{
    static const WCHAR dllW[] = {'.','d','l','l',0 };
    const WCHAR **listitem;
    BOOL show;

    if (!module) return TRUE;
    if (!includelist && !excludelist) return TRUE;
    if (excludelist)
    {
        show = TRUE;
        listitem = excludelist;
    }
    else
    {
        show = FALSE;
        listitem = includelist;
    }
    for(; *listitem; listitem++)
    {
        int len;

        if (!strcmpiW( *listitem, module )) return !show;
        len = strlenW( *listitem );
        if (!strncmpiW( *listitem, module, len ) && !strcmpiW( module + len, dllW ))
            return !show;
    }
    return show;
}

/***********************************************************************
 *           RELAY_PrintArgs
 */
static inline void RELAY_PrintArgs( int *args, int nb_args, unsigned int typemask )
{
    while (nb_args--)
    {
	if ((typemask & 3) && HIWORD(*args))
        {
	    if (typemask & 2)
                DPRINTF( "%08x %s", *args, debugstr_w((LPWSTR)*args) );
            else
                DPRINTF( "%08x %s", *args, debugstr_a((LPCSTR)*args) );
	}
        else DPRINTF( "%08x", *args );
        if (nb_args) DPRINTF( "," );
        args++;
        typemask >>= 2;
    }
}

extern LONGLONG call_entry_point( void *func, int nb_args, const int *args );
__ASM_GLOBAL_FUNC( call_entry_point,
                   "\tpushl %ebp\n"
                   "\tmovl %esp,%ebp\n"
                   "\tpushl %esi\n"
                   "\tpushl %edi\n"
                   "\tmovl 12(%ebp),%edx\n"
                   "\tshll $2,%edx\n"
                   "\tjz 1f\n"
                   "\tsubl %edx,%esp\n"
                   "\tandl $~15,%esp\n"
                   "\tmovl 12(%ebp),%ecx\n"
                   "\tmovl 16(%ebp),%esi\n"
                   "\tmovl %esp,%edi\n"
                   "\tcld\n"
                   "\trep; movsl\n"
                   "1:\tcall *8(%ebp)\n"
                   "\tleal -8(%ebp),%esp\n"
                   "\tpopl %edi\n"
                   "\tpopl %esi\n"
                   "\tpopl %ebp\n"
                   "\tret" );


/***********************************************************************
 *           relay_call_from_32
 *
 * stack points to the return address, i.e. the first argument is stack[1].
 */
static LONGLONG WINAPI relay_call_from_32( struct relay_descr *descr, unsigned int idx, int *stack )
{
    LONGLONG ret;
    WORD ordinal = LOWORD(idx);
    BYTE nb_args = LOBYTE(HIWORD(idx));
    BYTE flags   = HIBYTE(HIWORD(idx));
    struct relay_private_data *data = descr->private;
    struct relay_entry_point *entry_point = data->entry_points + ordinal;

    if (!TRACE_ON(relay))
        ret = call_entry_point( entry_point->orig_func, nb_args, stack + 1 );
    else
    {
        if (entry_point->name)
            DPRINTF( "%04lx:Call %s.%s(", GetCurrentThreadId(), data->dllname, entry_point->name );
        else
            DPRINTF( "%04lx:Call %s.%u(", GetCurrentThreadId(), data->dllname, data->base + ordinal );
        RELAY_PrintArgs( stack + 1, nb_args, descr->arg_types[ordinal] );
        DPRINTF( ") ret=%08x\n", stack[0] );

        ret = call_entry_point( entry_point->orig_func, nb_args, stack + 1 );

        if (entry_point->name)
            DPRINTF( "%04lx:Ret  %s.%s()", GetCurrentThreadId(), data->dllname, entry_point->name );
        else
            DPRINTF( "%04lx:Ret  %s.%u()", GetCurrentThreadId(), data->dllname, data->base + ordinal );

        if (flags & 1)  /* 64-bit return value */
            DPRINTF( " retval=%08x%08x ret=%08x\n",
                     (UINT)(ret >> 32), (UINT)ret, stack[0] );
        else
            DPRINTF( " retval=%08x ret=%08x\n", (UINT)ret, stack[0] );
    }
    return ret;
}


/***********************************************************************
 *           relay_call_from_32_regs
 */
void WINAPI __regs_relay_call_from_32_regs( struct relay_descr *descr, unsigned int idx,
                                            unsigned int orig_eax, unsigned int ret_addr,
                                            CONTEXT86 *context )
{
    WORD ordinal = LOWORD(idx);
    BYTE nb_args = LOBYTE(HIWORD(idx));
    BYTE flags   = HIBYTE(HIWORD(idx));
    struct relay_private_data *data = descr->private;
    struct relay_entry_point *entry_point = data->entry_points + ordinal;
    BYTE *orig_func = entry_point->orig_func;
    int *args = (int *)context->Esp;
    int args_copy[32];

    /* restore the context to what it was before the relay thunk */
    context->Eax = orig_eax;
    context->Eip = ret_addr;
    if (flags & 2)  /* stdcall */
        context->Esp += nb_args * sizeof(int);

    if (TRACE_ON(relay))
    {
        if (entry_point->name)
            DPRINTF( "%04lx:Call %s.%s(", GetCurrentThreadId(), data->dllname, entry_point->name );
        else
            DPRINTF( "%04lx:Call %s.%u(", GetCurrentThreadId(), data->dllname, data->base + ordinal );
        RELAY_PrintArgs( args, nb_args, descr->arg_types[ordinal] );
        DPRINTF( ") ret=%08x\n", ret_addr );

        DPRINTF( "%04lx:  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx "
                 "ebp=%08lx esp=%08lx ds=%04lx es=%04lx fs=%04lx gs=%04lx flags=%08lx\n",
                 GetCurrentThreadId(), context->Eax, context->Ebx, context->Ecx,
                 context->Edx, context->Esi, context->Edi, context->Ebp, context->Esp,
                 context->SegDs, context->SegEs, context->SegFs, context->SegGs, context->EFlags );

        assert( orig_func[0] == 0x50 /* pushl %eax */ );
        assert( orig_func[1] == 0xe8 /* call */ );
    }

    /* now call the real function */

    memcpy( args_copy, args, nb_args * sizeof(args[0]) );
    args_copy[nb_args++] = (int)context;  /* append context argument */

    call_entry_point( orig_func + 6 + *(int *)(orig_func + 6), nb_args, args_copy );


    if (TRACE_ON(relay))
    {
        if (entry_point->name)
            DPRINTF( "%04lx:Ret  %s.%s() retval=%08lx ret=%08lx\n",
                     GetCurrentThreadId(), data->dllname, entry_point->name,
                     context->Eax, context->Eip );
        else
            DPRINTF( "%04lx:Ret  %s.%u() retval=%08lx ret=%08lx\n",
                     GetCurrentThreadId(), data->dllname, data->base + ordinal,
                     context->Eax, context->Eip );
        DPRINTF( "%04lx:  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx "
                 "ebp=%08lx esp=%08lx ds=%04lx es=%04lx fs=%04lx gs=%04lx flags=%08lx\n",
                 GetCurrentThreadId(), context->Eax, context->Ebx, context->Ecx,
                 context->Edx, context->Esi, context->Edi, context->Ebp, context->Esp,
                 context->SegDs, context->SegEs, context->SegFs, context->SegGs, context->EFlags );
    }
}
extern void WINAPI relay_call_from_32_regs(void);
DEFINE_REGS_ENTRYPOINT( relay_call_from_32_regs, 16, 16 );


/***********************************************************************
 *           RELAY_GetProcAddress
 *
 * Return the proc address to use for a given function.
 */
FARPROC RELAY_GetProcAddress( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                              DWORD exp_size, FARPROC proc, DWORD ordinal, const WCHAR *user )
{
    struct relay_private_data *data;
    const struct relay_descr *descr = (const struct relay_descr *)((const char *)exports + exp_size);

    if (descr->magic != RELAY_DESCR_MAGIC || !(data = descr->private)) return proc;  /* no relay data */
    if (!data->entry_points[ordinal].orig_func) return proc;  /* not a relayed function */
    if (check_from_module( debug_from_relay_includelist, debug_from_relay_excludelist, user ))
        return proc;  /* we want to relay it */
    return data->entry_points[ordinal].orig_func;
}


/***********************************************************************
 *           RELAY_SetupDLL
 *
 * Setup relay debugging for a built-in dll.
 */
void RELAY_SetupDLL( HMODULE module )
{
    IMAGE_EXPORT_DIRECTORY *exports;
    DWORD *funcs;
    unsigned int i, len;
    DWORD size, entry_point_rva;
    struct relay_descr *descr;
    struct relay_private_data *data;
    const WORD *ordptr;

    if (!init_done) init_debug_lists();

    exports = RtlImageDirectoryEntryToData( module, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size );
    if (!exports) return;

    descr = (struct relay_descr *)((char *)exports + size);
    if (descr->magic != RELAY_DESCR_MAGIC) return;

    if (!(data = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data) +
                                  (exports->NumberOfFunctions-1) * sizeof(data->entry_points) )))
        return;

    descr->relay_from_32 = relay_call_from_32;
    descr->relay_from_32_regs = relay_call_from_32_regs;
    descr->private = data;

    data->module = module;
    data->base   = exports->Base;
    len = strlen( (char *)module + exports->Name );
    if (len > 4 && !strcasecmp( (char *)module + exports->Name + len - 4, ".dll" )) len -= 4;
    len = min( len, sizeof(data->dllname) - 1 );
    memcpy( data->dllname, (char *)module + exports->Name, len );
    data->dllname[len] = 0;

    /* fetch name pointer for all entry points and store them in the private structure */

    ordptr = (const WORD *)((char *)module + exports->AddressOfNameOrdinals);
    for (i = 0; i < exports->NumberOfNames; i++, ordptr++)
    {
        DWORD name_rva = ((DWORD*)((char *)module + exports->AddressOfNames))[i];
        data->entry_points[*ordptr].name = (const char *)module + name_rva;
    }

    /* patch the functions in the export table to point to the relay thunks */

    funcs = (DWORD *)((char *)module + exports->AddressOfFunctions);
    entry_point_rva = (const char *)descr->entry_point_base - (const char *)module;
    for (i = 0; i < exports->NumberOfFunctions; i++, funcs++)
    {
        if (!descr->entry_point_offsets[i]) continue;   /* not a normal function */
        if (!check_relay_include( data->dllname, i + exports->Base, data->entry_points[i].name ))
            continue;  /* don't include this entry point */

        data->entry_points[i].orig_func = (char *)module + *funcs;
        *funcs = entry_point_rva + descr->entry_point_offsets[i];
    }
}



/***********************************************************************/
/* snoop support */
/***********************************************************************/

#include "pshpack1.h"

typedef	struct
{
	/* code part */
	BYTE		lcall;		/* 0xe8 call snoopentry (relative) */
	/* NOTE: If you move snoopentry OR nrofargs fix the relative offset
	 * calculation!
	 */
	DWORD		snoopentry;	/* SNOOP_Entry relative */
	/* unreached */
	int		nrofargs;
	FARPROC	origfun;
	const char *name;
} SNOOP_FUN;

typedef struct tagSNOOP_DLL {
	HMODULE	hmod;
	SNOOP_FUN	*funs;
	DWORD		ordbase;
	DWORD		nrofordinals;
	struct tagSNOOP_DLL	*next;
	char name[1];
} SNOOP_DLL;

typedef struct
{
	/* code part */
	BYTE		lcall;		/* 0xe8 call snoopret relative*/
	/* NOTE: If you move snoopret OR origreturn fix the relative offset
	 * calculation!
	 */
	DWORD		snoopret;	/* SNOOP_Ret relative */
	/* unreached */
	FARPROC	origreturn;
	SNOOP_DLL	*dll;
	DWORD		ordinal;
	DWORD		origESP;
	DWORD		*args;		/* saved args across a stdcall */
} SNOOP_RETURNENTRY;

typedef struct tagSNOOP_RETURNENTRIES {
	SNOOP_RETURNENTRY entry[4092/sizeof(SNOOP_RETURNENTRY)];
	struct tagSNOOP_RETURNENTRIES	*next;
} SNOOP_RETURNENTRIES;

#include "poppack.h"

extern void WINAPI SNOOP_Entry(void);
extern void WINAPI SNOOP_Return(void);

static SNOOP_DLL *firstdll;
static SNOOP_RETURNENTRIES *firstrets;


/***********************************************************************
 *          SNOOP_ShowDebugmsgSnoop
 *
 * Simple function to decide if a particular debugging message is
 * wanted.
 */
static BOOL SNOOP_ShowDebugmsgSnoop(const char *module, int ordinal, const char *func)
{
    if (debug_snoop_excludelist && check_list( module, ordinal, func, debug_snoop_excludelist ))
        return FALSE;
    if (debug_snoop_includelist && !check_list( module, ordinal, func, debug_snoop_includelist ))
        return FALSE;
    return TRUE;
}


/***********************************************************************
 *           SNOOP_SetupDLL
 *
 * Setup snoop debugging for a native dll.
 */
void SNOOP_SetupDLL(HMODULE hmod)
{
    SNOOP_DLL **dll = &firstdll;
    char *p, *name;
    void *addr;
    SIZE_T size;
    IMAGE_EXPORT_DIRECTORY *exports;

    if (!init_done) init_debug_lists();

    exports = RtlImageDirectoryEntryToData( hmod, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size );
    if (!exports) return;
    name = (char *)hmod + exports->Name;

    TRACE_(snoop)("hmod=%p, name=%s\n", hmod, name);

    while (*dll) {
        if ((*dll)->hmod == hmod)
        {
            /* another dll, loaded at the same address */
            addr = (*dll)->funs;
            size = (*dll)->nrofordinals * sizeof(SNOOP_FUN);
            NtFreeVirtualMemory(NtCurrentProcess(), &addr, &size, MEM_RELEASE);
            break;
        }
        dll = &((*dll)->next);
    }
    if (*dll)
        *dll = RtlReAllocateHeap(GetProcessHeap(),
                             HEAP_ZERO_MEMORY, *dll,
                             sizeof(SNOOP_DLL) + strlen(name));
    else
        *dll = RtlAllocateHeap(GetProcessHeap(),
                             HEAP_ZERO_MEMORY,
                             sizeof(SNOOP_DLL) + strlen(name));
    (*dll)->hmod	= hmod;
    (*dll)->ordbase = exports->Base;
    (*dll)->nrofordinals = exports->NumberOfFunctions;
    strcpy( (*dll)->name, name );
    p = (*dll)->name + strlen((*dll)->name) - 4;
    if (p > (*dll)->name && !strcasecmp( p, ".dll" )) *p = 0;

    size = exports->NumberOfFunctions * sizeof(SNOOP_FUN);
    addr = NULL;
    NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &size,
                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!addr) {
        RtlFreeHeap(GetProcessHeap(),0,*dll);
        FIXME("out of memory\n");
        return;
    }
    (*dll)->funs = addr;
    memset((*dll)->funs,0,size);
}


/***********************************************************************
 *           SNOOP_GetProcAddress
 *
 * Return the proc address to use for a given function.
 */
FARPROC SNOOP_GetProcAddress( HMODULE hmod, const IMAGE_EXPORT_DIRECTORY *exports,
                              DWORD exp_size, FARPROC origfun, DWORD ordinal,
                              const WCHAR *user)
{
    unsigned int i;
    const char *ename;
    const WORD *ordinals;
    const DWORD *names;
    SNOOP_DLL *dll = firstdll;
    SNOOP_FUN *fun;
    const IMAGE_SECTION_HEADER *sec;

    if (!TRACE_ON(snoop)) return origfun;
    if (!check_from_module( debug_from_snoop_includelist, debug_from_snoop_excludelist, user ))
        return origfun; /* the calling module was explicitly excluded */

    if (!*(LPBYTE)origfun) /* 0x00 is an imposs. opcode, poss. dataref. */
        return origfun;

    sec = RtlImageRvaToSection( RtlImageNtHeader(hmod), hmod, (char *)origfun - (char *)hmod );

    if (!sec || !(sec->Characteristics & IMAGE_SCN_CNT_CODE))
        return origfun;  /* most likely a data reference */

    while (dll) {
        if (hmod == dll->hmod)
            break;
        dll = dll->next;
    }
    if (!dll)	/* probably internal */
        return origfun;

    /* try to find a name for it */
    ename = NULL;
    names = (const DWORD *)((const char *)hmod + exports->AddressOfNames);
    ordinals = (const WORD *)((const char *)hmod + exports->AddressOfNameOrdinals);
    if (names) for (i = 0; i < exports->NumberOfNames; i++)
    {
        if (ordinals[i] == ordinal)
        {
            ename = (const char *)hmod + names[i];
            break;
        }
    }
    if (!SNOOP_ShowDebugmsgSnoop(dll->name,ordinal,ename))
        return origfun;
    assert(ordinal < dll->nrofordinals);
    fun = dll->funs + ordinal;
    if (!fun->name)
    {
        fun->name       = ename;
        fun->lcall	= 0xe8;
        /* NOTE: origreturn struct member MUST come directly after snoopentry */
        fun->snoopentry	= (char*)SNOOP_Entry-((char*)(&fun->nrofargs));
        fun->origfun	= origfun;
        fun->nrofargs	= -1;
    }
    return (FARPROC)&(fun->lcall);
}

static void SNOOP_PrintArg(DWORD x)
{
    int i,nostring;

    DPRINTF("%08lx",x);
    if (!HIWORD(x) || TRACE_ON(seh)) return; /* trivial reject to avoid faults */
    __TRY
    {
        LPBYTE s=(LPBYTE)x;
        i=0;nostring=0;
        while (i<80) {
            if (s[i]==0) break;
            if (s[i]<0x20) {nostring=1;break;}
            if (s[i]>=0x80) {nostring=1;break;}
            i++;
        }
        if (!nostring && i > 5)
            DPRINTF(" %s",debugstr_an((LPSTR)x,i));
        else  /* try unicode */
        {
            LPWSTR s=(LPWSTR)x;
            i=0;nostring=0;
            while (i<80) {
                if (s[i]==0) break;
                if (s[i]<0x20) {nostring=1;break;}
                if (s[i]>0x100) {nostring=1;break;}
                i++;
            }
            if (!nostring && i > 5) DPRINTF(" %s",debugstr_wn((LPWSTR)x,i));
        }
    }
    __EXCEPT_PAGE_FAULT
    {
    }
    __ENDTRY
}

#define CALLER1REF (*(DWORD*)context->Esp)

void WINAPI __regs_SNOOP_Entry( CONTEXT86 *context )
{
	DWORD		ordinal=0,entry = context->Eip - 5;
	SNOOP_DLL	*dll = firstdll;
	SNOOP_FUN	*fun = NULL;
	SNOOP_RETURNENTRIES	**rets = &firstrets;
	SNOOP_RETURNENTRY	*ret;
	int		i=0, max;

	while (dll) {
		if (	((char*)entry>=(char*)dll->funs)	&&
			((char*)entry<=(char*)(dll->funs+dll->nrofordinals))
		) {
			fun = (SNOOP_FUN*)entry;
			ordinal = fun-dll->funs;
			break;
		}
		dll=dll->next;
	}
	if (!dll) {
		FIXME("entrypoint 0x%08lx not found\n",entry);
		return; /* oops */
	}
	/* guess cdecl ... */
	if (fun->nrofargs<0) {
		/* Typical cdecl return frame is:
		 *     add esp, xxxxxxxx
		 * which has (for xxxxxxxx up to 255 the opcode "83 C4 xx".
		 * (after that 81 C2 xx xx xx xx)
		 */
		LPBYTE	reteip = (LPBYTE)CALLER1REF;

		if (reteip) {
			if ((reteip[0]==0x83)&&(reteip[1]==0xc4))
				fun->nrofargs=reteip[2]/4;
		}
	}


	while (*rets) {
		for (i=0;i<sizeof((*rets)->entry)/sizeof((*rets)->entry[0]);i++)
			if (!(*rets)->entry[i].origreturn)
				break;
		if (i!=sizeof((*rets)->entry)/sizeof((*rets)->entry[0]))
			break;
		rets = &((*rets)->next);
	}
	if (!*rets) {
                SIZE_T size = 4096;
                VOID* addr = NULL;

                NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &size, 
                                        MEM_COMMIT | MEM_RESERVE,
                                        PAGE_EXECUTE_READWRITE);
                if (!addr) return;
                *rets = addr;
		memset(*rets,0,4096);
		i = 0;	/* entry 0 is free */
	}
	ret = &((*rets)->entry[i]);
	ret->lcall	= 0xe8;
	/* NOTE: origreturn struct member MUST come directly after snoopret */
	ret->snoopret	= ((char*)SNOOP_Return)-(char*)(&ret->origreturn);
	ret->origreturn	= (FARPROC)CALLER1REF;
	CALLER1REF	= (DWORD)&ret->lcall;
	ret->dll	= dll;
	ret->args	= NULL;
	ret->ordinal	= ordinal;
	ret->origESP	= context->Esp;

	context->Eip = (DWORD)fun->origfun;

	if (fun->name) DPRINTF("%04lx:CALL %s.%s(",GetCurrentThreadId(),dll->name,fun->name);
	else DPRINTF("%04lx:CALL %s.%ld(",GetCurrentThreadId(),dll->name,dll->ordbase+ordinal);
	if (fun->nrofargs>0) {
		max = fun->nrofargs; if (max>16) max=16;
		for (i=0;i<max;i++)
                {
                    SNOOP_PrintArg(*(DWORD*)(context->Esp + 4 + sizeof(DWORD)*i));
                    if (i<fun->nrofargs-1) DPRINTF(",");
                }
		if (max!=fun->nrofargs)
			DPRINTF(" ...");
	} else if (fun->nrofargs<0) {
		DPRINTF("<unknown, check return>");
		ret->args = RtlAllocateHeap(GetProcessHeap(),
                                            0,16*sizeof(DWORD));
		memcpy(ret->args,(LPBYTE)(context->Esp + 4),sizeof(DWORD)*16);
	}
	DPRINTF(") ret=%08lx\n",(DWORD)ret->origreturn);
}


void WINAPI __regs_SNOOP_Return( CONTEXT86 *context )
{
	SNOOP_RETURNENTRY	*ret = (SNOOP_RETURNENTRY*)(context->Eip - 5);
        SNOOP_FUN *fun = &ret->dll->funs[ret->ordinal];

	/* We haven't found out the nrofargs yet. If we called a cdecl
	 * function it is too late anyway and we can just set '0' (which
	 * will be the difference between orig and current ESP
	 * If stdcall -> everything ok.
	 */
	if (ret->dll->funs[ret->ordinal].nrofargs<0)
		ret->dll->funs[ret->ordinal].nrofargs=(context->Esp - ret->origESP-4)/4;
	context->Eip = (DWORD)ret->origreturn;
	if (ret->args) {
		int	i,max;

                if (fun->name)
                    DPRINTF("%04lx:RET  %s.%s(", GetCurrentThreadId(), ret->dll->name, fun->name);
                else
                    DPRINTF("%04lx:RET  %s.%ld(", GetCurrentThreadId(),
                            ret->dll->name,ret->dll->ordbase+ret->ordinal);

		max = fun->nrofargs;
		if (max>16) max=16;

		for (i=0;i<max;i++)
                {
                    SNOOP_PrintArg(ret->args[i]);
                    if (i<max-1) DPRINTF(",");
                }
		DPRINTF(") retval=%08lx ret=%08lx\n",
			context->Eax,(DWORD)ret->origreturn );
		RtlFreeHeap(GetProcessHeap(),0,ret->args);
		ret->args = NULL;
	}
        else
        {
            if (fun->name)
		DPRINTF("%04lx:RET  %s.%s() retval=%08lx ret=%08lx\n",
			GetCurrentThreadId(),
			ret->dll->name, fun->name, context->Eax, (DWORD)ret->origreturn);
            else
		DPRINTF("%04lx:RET  %s.%ld() retval=%08lx ret=%08lx\n",
			GetCurrentThreadId(),
			ret->dll->name,ret->dll->ordbase+ret->ordinal,
			context->Eax, (DWORD)ret->origreturn);
        }
	ret->origreturn = NULL; /* mark as empty */
}

/* assembly wrappers that save the context */
DEFINE_REGS_ENTRYPOINT( SNOOP_Entry, 0, 0 );
DEFINE_REGS_ENTRYPOINT( SNOOP_Return, 0, 0 );

#else  /* __i386__ */

FARPROC RELAY_GetProcAddress( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                              DWORD exp_size, FARPROC proc, DWORD ordinal, const WCHAR *user )
{
    return proc;
}

FARPROC SNOOP_GetProcAddress( HMODULE hmod, const IMAGE_EXPORT_DIRECTORY *exports, DWORD exp_size,
                              FARPROC origfun, DWORD ordinal, const WCHAR *user )
{
    return origfun;
}

void RELAY_SetupDLL( HMODULE module )
{
}

void SNOOP_SetupDLL( HMODULE hmod )
{
    FIXME("snooping works only on i386 for now.\n");
}

#endif /* __i386__ */
