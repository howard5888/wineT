/*
 * File module.c - module handling for the wine debugger
 *
 * Copyright (C) 1993,      Eric Youngdale.
 * 		 2000-2004, Eric Pouech
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dbghelp_private.h"
#include "psapi.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/debug.h"
#include "winnls.h"

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

static const char* ext[] = {".acm", ".dll", ".drv", ".exe", ".ocx", ".vxd", NULL};

static int match_ext(const char* ptr, size_t len)
{
    const char**e;
    size_t      l;

    for (e = ext; *e; e++)
    {
        l = strlen(*e);
        if (l >= len) return FALSE;
        if (strncasecmp(&ptr[len - l], *e, l)) continue;
        return l;
    }
    return 0;
}
        
static void module_fill_module(const char* in, char* out, size_t size)
{
    const char  *ptr,*endptr;
    size_t      len, l;

    endptr = in + strlen(in);
    for (ptr = endptr - 1;
         ptr >= in && *ptr != '/' && *ptr != '\\';
         ptr--);
    ptr++;
    len = min(endptr-ptr,size-1);
    memcpy(out, ptr, len);
    out[len] = '\0';
    if (len > 4 && (l = match_ext(out, len)))
        out[len - l] = '\0';
    else if (len > 12 &&
             (!strcasecmp(out + len - 12, "wine-pthread") || 
              !strcasecmp(out + len - 12, "wine-kthread")))
        lstrcpynA(out, "<wine-loader>", size);
    else
    {
        if (len > 3 && !strcasecmp(&out[len - 3], ".so") &&
            (l = match_ext(out, len - 3)))
            strcpy(&out[len - l - 3], "<elf>");
    }
    while ((*out = tolower(*out))) out++;
}

static const char*      get_module_type(enum module_type type, BOOL virtual)
{
    switch (type)
    {
    case DMT_ELF: return virtual ? "Virtual ELF" : "ELF";
    case DMT_PE: return virtual ? "Virtual PE" : "PE";
    default: return "---";
    }
}

/***********************************************************************
 * Creates and links a new module to a process 
 */
struct module* module_new(struct process* pcs, const char* name, 
                          enum module_type type, BOOL virtual,
                          unsigned long mod_addr, unsigned long size,
                          unsigned long stamp, unsigned long checksum) 
{
    struct module*      module;

    assert(type == DMT_ELF || type == DMT_PE);
    if (!(module = HeapAlloc(GetProcessHeap(), 0, sizeof(*module))))
	return NULL;

    memset(module, 0, sizeof(*module));

    module->next = pcs->lmodules;
    pcs->lmodules = module;

    TRACE("=> %s %08lx-%08lx %s\n", 
          get_module_type(type, virtual), mod_addr, mod_addr + size, name);

    pool_init(&module->pool, 65536);
    
    module->module.SizeOfStruct = sizeof(module->module);
    module->module.BaseOfImage = mod_addr;
    module->module.ImageSize = size;
    module_fill_module(name, module->module.ModuleName,
                       sizeof(module->module.ModuleName));
    module->module.ImageName[0] = '\0';
    lstrcpynA(module->module.LoadedImageName, name, sizeof(module->module.LoadedImageName));
    module->module.SymType = SymNone;
    module->module.NumSyms = 0;
    module->module.TimeDateStamp = stamp;
    module->module.CheckSum = checksum;

    memset(module->module.LoadedPdbName, 0, sizeof(module->module.CVData));
    module->module.CVSig = 0;
    memset(module->module.CVData, 0, sizeof(module->module.CVData));
    module->module.PdbSig = 0;
    memset(&module->module.PdbSig70, 0, sizeof(module->module.PdbSig70));
    module->module.PdbAge = 0;
    module->module.PdbUnmatched = FALSE;
    module->module.DbgUnmatched = FALSE;
    module->module.LineNumbers = FALSE;
    module->module.GlobalSymbols = FALSE;
    module->module.TypeInfo = FALSE;
    module->module.SourceIndexed = 0;
    module->module.Publics = 0;

    module->type              = type;
    module->is_virtual        = virtual ? TRUE : FALSE;
    module->sortlist_valid    = FALSE;
    module->addr_sorttab      = NULL;
    /* FIXME: this seems a bit too high (on a per module basis)
     * need some statistics about this
     */
    hash_table_init(&module->pool, &module->ht_symbols, 4096);
    hash_table_init(&module->pool, &module->ht_types,   4096);
    vector_init(&module->vtypes, sizeof(struct symt*),  32);

    module->sources_used      = 0;
    module->sources_alloc     = 0;
    module->sources           = 0;

    return module;
}

/***********************************************************************
 *	module_find_by_name
 *
 */
struct module* module_find_by_name(const struct process* pcs, 
                                   const char* name, enum module_type type)
{
    struct module*      module;

    if (type == DMT_UNKNOWN)
    {
        if ((module = module_find_by_name(pcs, name, DMT_PE)) ||
            (module = module_find_by_name(pcs, name, DMT_ELF)))
            return module;
    }
    else
    {
        char                modname[MAX_PATH];

        for (module = pcs->lmodules; module; module = module->next)
        {
            if (type == module->type &&
                !strcasecmp(name, module->module.LoadedImageName)) 
                return module;
        }
        module_fill_module(name, modname, sizeof(modname));
        for (module = pcs->lmodules; module; module = module->next)
        {
            if (type == module->type &&
                !strcasecmp(modname, module->module.ModuleName)) 
                return module;
        }
    }
    SetLastError(ERROR_INVALID_NAME);
    return NULL;
}

/***********************************************************************
 *           module_get_container
 *
 */
struct module* module_get_container(const struct process* pcs, 
                                    const struct module* inner)
{
    struct module*      module;
     
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module != inner &&
            module->module.BaseOfImage <= inner->module.BaseOfImage &&
            module->module.BaseOfImage + module->module.ImageSize >=
            inner->module.BaseOfImage + inner->module.ImageSize)
            return module;
    }
    return NULL;
}

/***********************************************************************
 *           module_get_containee
 *
 */
struct module* module_get_containee(const struct process* pcs, 
                                    const struct module* outter)
{
    struct module*      module;
     
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module != outter &&
            outter->module.BaseOfImage <= module->module.BaseOfImage &&
            outter->module.BaseOfImage + outter->module.ImageSize >=
            module->module.BaseOfImage + module->module.ImageSize)
            return module;
    }
    return NULL;
}

/******************************************************************
 *		module_get_debug
 *
 * get the debug information from a module:
 * - if the module's type is deferred, then force loading of debug info (and return
 *   the module itself)
 * - if the module has no debug info and has an ELF container, then return the ELF
 *   container (and also force the ELF container's debug info loading if deferred)
 * - otherwise return the module itself if it has some debug info
 */
BOOL module_get_debug(const struct process* pcs, struct module_pair* pair)
{
    IMAGEHLP_DEFERRED_SYMBOL_LOAD64     idsl64;

    if (!pair->requested) return FALSE;
    /* for a PE builtin, always get info from container */
    if (!(pair->effective = module_get_container(pcs, pair->requested)))
        pair->effective = pair->requested;
    /* if deferred, force loading */
    if (pair->effective->module.SymType == SymDeferred)
    {
        BOOL ret;
        
        if (pair->effective->is_virtual) ret = FALSE;
        else switch (pair->effective->type)
        {
        case DMT_ELF:
            ret = elf_load_debug_info(pair->effective, NULL);
            break;
        case DMT_PE:
            idsl64.SizeOfStruct = sizeof(idsl64);
            idsl64.BaseOfImage = pair->effective->module.BaseOfImage;
            idsl64.CheckSum = pair->effective->module.CheckSum;
            idsl64.TimeDateStamp = pair->effective->module.TimeDateStamp;
            strcpy(idsl64.FileName, pair->effective->module.ImageName);
            idsl64.Reparse = FALSE;
            idsl64.hFile = INVALID_HANDLE_VALUE;

            pcs_callback(pcs, CBA_DEFERRED_SYMBOL_LOAD_START, &idsl64);
            ret = pe_load_debug_info(pcs, pair->effective);
            pcs_callback(pcs,
                         ret ? CBA_DEFERRED_SYMBOL_LOAD_COMPLETE : CBA_DEFERRED_SYMBOL_LOAD_FAILURE,
                         &idsl64);
            break;
        default:
            ret = FALSE;
            break;
        }
        if (!ret) pair->effective->module.SymType = SymNone;
        assert(pair->effective->module.SymType != SymDeferred);
        module_compute_num_syms(pair->effective);
    }
    return pair->effective->module.SymType != SymNone;
}

/***********************************************************************
 *	module_find_by_addr
 *
 * either the addr where module is loaded, or any address inside the 
 * module
 */
struct module* module_find_by_addr(const struct process* pcs, unsigned long addr, 
                                   enum module_type type)
{
    struct module*      module;
    
    if (type == DMT_UNKNOWN)
    {
        if ((module = module_find_by_addr(pcs, addr, DMT_PE)) ||
            (module = module_find_by_addr(pcs, addr, DMT_ELF)))
            return module;
    }
    else
    {
        for (module = pcs->lmodules; module; module = module->next)
        {
            if (type == module->type && addr >= module->module.BaseOfImage &&
                addr < module->module.BaseOfImage + module->module.ImageSize) 
                return module;
        }
    }
    SetLastError(ERROR_INVALID_ADDRESS);
    return module;
}

static BOOL module_is_elf_container_loaded(struct process* pcs, const char* ImageName,
                                           const char* ModuleName)
{
    char                buffer[MAX_PATH];
    size_t              len;
    struct module*      module;

    if (!ModuleName)
    {
        module_fill_module(ImageName, buffer, sizeof(buffer));
        ModuleName = buffer;
    }
    len = strlen(ModuleName);
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!strncasecmp(module->module.ModuleName, ModuleName, len) &&
            module->type == DMT_ELF &&
            !strcmp(module->module.ModuleName + len, "<elf>"))
            return TRUE;
    }
    return FALSE;
}

/******************************************************************
 *		module_get_type_by_name
 *
 * Guesses a filename type from its extension
 */
enum module_type module_get_type_by_name(const char* name)
{
    const char* ptr;
    int         len = strlen(name);

    /* check for terminating .so or .so.[digit] */
    ptr = strrchr(name, '.');
    if (ptr)
    {
        if (!strcmp(ptr, ".so") ||
            (isdigit(ptr[1]) && !ptr[2] && ptr >= name + 3 && !memcmp(ptr - 3, ".so", 3)))
            return DMT_ELF;
        else if (!strcasecmp(ptr, ".pdb"))
            return DMT_PDB;
    }
    /* wine-[kp]thread is also an ELF module */
    else if (((len > 12 && name[len - 13] == '/') || len == 12) && 
             (!strcasecmp(name + len - 12, "wine-pthread") || 
              !strcasecmp(name + len - 12, "wine-kthread")))
    {
        return DMT_ELF;
    }
    return DMT_PE;
}

int module_compute_num_syms(struct module* module)
{
    struct hash_table_iter      hti;
    void*                       ptr;

    module->module.NumSyms = 0;
    hash_table_iter_init(&module->ht_symbols, &hti, NULL);
    while ((ptr = hash_table_iter_up(&hti)))
         module->module.NumSyms++;
    return module->module.NumSyms;
}

/***********************************************************************
 *			SymLoadModule (DBGHELP.@)
 */
DWORD WINAPI SymLoadModule(HANDLE hProcess, HANDLE hFile, const char* ImageName,
                           const char* ModuleName, DWORD BaseOfDll, DWORD SizeOfDll)
{
    struct process*     pcs;
    struct module*	module = NULL;

    TRACE("(%p %p %s %s %08lx %08lx)\n", 
          hProcess, hFile, debugstr_a(ImageName), debugstr_a(ModuleName), 
          BaseOfDll, SizeOfDll);

    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;

    /* force transparent ELF loading / unloading */
    elf_synchronize_module_list(pcs);

    /* this is a Wine extension to the API just to redo the synchronisation */
    if (!ImageName && !hFile) return 0;

    if (module_is_elf_container_loaded(pcs, ImageName, ModuleName))
    {
        /* force the loading of DLL as builtin */
        if ((module = pe_load_module_from_pcs(pcs, ImageName, ModuleName,
                                              BaseOfDll, SizeOfDll)))
            goto done;
        WARN("Couldn't locate %s\n", ImageName);
        return 0;
    }
    TRACE("Assuming %s as native DLL\n", ImageName);
    if (!(module = pe_load_module(pcs, ImageName, hFile, BaseOfDll, SizeOfDll)))
    {
        if (module_get_type_by_name(ImageName) == DMT_ELF &&
            (module = elf_load_module(pcs, ImageName, BaseOfDll)))
            goto done;
        FIXME("Should have successfully loaded debug information for image %s\n",
              ImageName);
        if ((module = pe_load_module_from_pcs(pcs, ImageName, ModuleName,
                                              BaseOfDll, SizeOfDll)))
            goto done;
        WARN("Couldn't locate %s\n", ImageName);
        return 0;
    }
    module_compute_num_syms(module);
done:
    /* by default pe_load_module fills module.ModuleName from a derivation 
     * of ImageName. Overwrite it, if we have better information
     */
    if (ModuleName)
        lstrcpynA(module->module.ModuleName, ModuleName, sizeof(module->module.ModuleName));
    lstrcpynA(module->module.ImageName, ImageName, sizeof(module->module.ImageName));

    return module->module.BaseOfImage;
}

/***********************************************************************
 *			SymLoadModuleEx (DBGHELP.@)
 */
DWORD64 WINAPI  SymLoadModuleEx(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
                                PCSTR ModuleName, DWORD64 BaseOfDll, DWORD DllSize,
                                PMODLOAD_DATA Data, DWORD Flags)
{
    TRACE("(%p %p %s %s %s %08lx %p %08lx)\n", 
          hProcess, hFile, debugstr_a(ImageName), debugstr_a(ModuleName), 
          wine_dbgstr_longlong(BaseOfDll), DllSize, Data, Flags);

    if (Data)
        FIXME("Unsupported load data parameter %p for %s\n", Data, ImageName);
    if (!validate_addr64(BaseOfDll)) return FALSE;
    if (Flags & SLMFLAG_VIRTUAL)
    {
        struct process* pcs = process_find_by_handle(hProcess);
        struct module* module;
        if (!pcs) return FALSE;

        module = module_new(pcs, ImageName, module_get_type_by_name(ImageName), TRUE, 
                            (DWORD)BaseOfDll, DllSize, 0, 0);
        if (!module) return FALSE;
        if (ModuleName)
            lstrcpynA(module->module.ModuleName, ModuleName, sizeof(module->module.ModuleName));
        module->module.SymType = SymVirtual;

        return TRUE;
    }
    if (Flags & ~(SLMFLAG_VIRTUAL))
        FIXME("Unsupported Flags %08lx for %s\n", Flags, ImageName);

    return SymLoadModule(hProcess, hFile, (char*)ImageName, (char*)ModuleName,
                         (DWORD)BaseOfDll, DllSize);
}

/***********************************************************************
 *			SymLoadModuleExW (DBGHELP.@)
 */
DWORD64 WINAPI  SymLoadModuleExW(HANDLE hProcess, HANDLE hFile, PCWSTR wImageName,
                                 PCWSTR wModuleName, DWORD64 BaseOfDll, DWORD DllSize,
                                 PMODLOAD_DATA Data, DWORD Flags)
{
    LPSTR       ImageName, ModuleName;
    unsigned    len;
    BOOL        ret;

    if (wImageName)
    {
        len = WideCharToMultiByte(CP_ACP,0, wImageName, -1, NULL, 0, NULL, NULL);
        ImageName = HeapAlloc(GetProcessHeap(), 0, len);
        WideCharToMultiByte(CP_ACP,0, wImageName, -1, ImageName, len, NULL, NULL);
    }
    else ImageName = NULL;
    if (wModuleName)
    {
        len = WideCharToMultiByte(CP_ACP,0, wModuleName, -1, NULL, 0, NULL, NULL);
        ModuleName = HeapAlloc(GetProcessHeap(), 0, len);
        WideCharToMultiByte(CP_ACP,0, wModuleName, -1, ModuleName, len, NULL, NULL);
    }
    else ModuleName = NULL;

    ret = SymLoadModuleEx(hProcess, hFile, ImageName, ModuleName,
                          BaseOfDll, DllSize, Data, Flags);
    HeapFree(GetProcessHeap(), 0, ImageName);
    HeapFree(GetProcessHeap(), 0, ModuleName);
    return ret;
}

/***********************************************************************
 *                     SymLoadModule64 (DBGHELP.@)
 */
DWORD64 WINAPI SymLoadModule64(HANDLE hProcess, HANDLE hFile, PCSTR ImageName,
                               PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll)
{
    if (!validate_addr64(BaseOfDll)) return FALSE;
    return SymLoadModule(hProcess, hFile, ImageName, ModuleName, (DWORD)BaseOfDll, SizeOfDll);
}

/******************************************************************
 *		module_remove
 *
 */
BOOL module_remove(struct process* pcs, struct module* module)
{
    struct module**     p;

    TRACE("%s (%p)\n", module->module.ModuleName, module);
    hash_table_destroy(&module->ht_symbols);
    hash_table_destroy(&module->ht_types);
    HeapFree(GetProcessHeap(), 0, (char*)module->sources);
    HeapFree(GetProcessHeap(), 0, module->addr_sorttab);
    pool_destroy(&module->pool);
    /* native dbghelp doesn't invoke registered callback(,CBA_SYMBOLS_UNLOADED,) here
     * so do we
     */
    for (p = &pcs->lmodules; *p; p = &(*p)->next)
    {
        if (*p == module)
        {
            *p = module->next;
            HeapFree(GetProcessHeap(), 0, module);
            return TRUE;
        }
    }
    FIXME("This shouldn't happen\n");
    return FALSE;
}

/******************************************************************
 *		SymUnloadModule (DBGHELP.@)
 *
 */
BOOL WINAPI SymUnloadModule(HANDLE hProcess, DWORD BaseOfDll)
{
    struct process*     pcs;
    struct module*      module;

    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
    module = module_find_by_addr(pcs, BaseOfDll, DMT_UNKNOWN);
    if (!module) return FALSE;
    return module_remove(pcs, module);
}

/******************************************************************
 *		SymUnloadModule64 (DBGHELP.@)
 *
 */
BOOL WINAPI SymUnloadModule64(HANDLE hProcess, DWORD64 BaseOfDll)
{
    struct process*     pcs;
    struct module*      module;

    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
    if (!validate_addr64(BaseOfDll)) return FALSE;
    module = module_find_by_addr(pcs, (DWORD)BaseOfDll, DMT_UNKNOWN);
    if (!module) return FALSE;
    return module_remove(pcs, module);
}

/******************************************************************
 *		SymEnumerateModules (DBGHELP.@)
 *
 */
BOOL  WINAPI SymEnumerateModules(HANDLE hProcess,
                                 PSYM_ENUMMODULES_CALLBACK EnumModulesCallback,  
                                 PVOID UserContext)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;

    if (!pcs) return FALSE;
    
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!(dbghelp_options & SYMOPT_WINE_WITH_ELF_MODULES) && module->type == DMT_ELF)
            continue;
        if (!EnumModulesCallback(module->module.ModuleName, 
                                 module->module.BaseOfImage, UserContext))
            break;
    }
    return TRUE;
}

/******************************************************************
 *		SymEnumerateModules64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymEnumerateModules64(HANDLE hProcess,
                                   PSYM_ENUMMODULES_CALLBACK64 EnumModulesCallback,  
                                   PVOID UserContext)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;

    if (!pcs) return FALSE;
    
    for (module = pcs->lmodules; module; module = module->next)
    {
        if (!(dbghelp_options & SYMOPT_WINE_WITH_ELF_MODULES) && module->type == DMT_ELF)
            continue;
        if (!EnumModulesCallback(module->module.ModuleName, 
                                 module->module.BaseOfImage, UserContext))
            break;
    }
    return TRUE;
}

/******************************************************************
 *		EnumerateLoadedModules (DBGHELP.@)
 *
 */
BOOL  WINAPI EnumerateLoadedModules(HANDLE hProcess,
                                    PENUMLOADED_MODULES_CALLBACK EnumLoadedModulesCallback,
                                    PVOID UserContext)
{
    HMODULE*    hMods;
    char        base[256], mod[256];
    DWORD       i, sz;
    MODULEINFO  mi;

    hMods = HeapAlloc(GetProcessHeap(), 0, 256 * sizeof(hMods[0]));
    if (!hMods) return FALSE;

    if (!EnumProcessModules(hProcess, hMods, 256 * sizeof(hMods[0]), &sz))
    {
        /* hProcess should also be a valid process handle !! */
        FIXME("If this happens, bump the number in mod\n");
        HeapFree(GetProcessHeap(), 0, hMods);
        return FALSE;
    }
    sz /= sizeof(HMODULE);
    for (i = 0; i < sz; i++)
    {
        if (!GetModuleInformation(hProcess, hMods[i], &mi, sizeof(mi)) ||
            !GetModuleBaseNameA(hProcess, hMods[i], base, sizeof(base)))
            continue;
        module_fill_module(base, mod, sizeof(mod));
        EnumLoadedModulesCallback(mod, (DWORD)mi.lpBaseOfDll, mi.SizeOfImage, 
                                  UserContext);
    }
    HeapFree(GetProcessHeap(), 0, hMods);

    return sz != 0 && i == sz;
}

/******************************************************************
 *		SymGetModuleInfo (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfo(HANDLE hProcess, DWORD dwAddr, 
                              PIMAGEHLP_MODULE ModuleInfo)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;
    IMAGEHLP_MODULE     mod;

    if (!pcs) return FALSE;
    if (ModuleInfo->SizeOfStruct < sizeof(*ModuleInfo)) return FALSE;
    module = module_find_by_addr(pcs, dwAddr, DMT_UNKNOWN);
    if (!module) return FALSE;

    mod.SizeOfStruct = ModuleInfo->SizeOfStruct;
    mod.BaseOfImage = module->module.BaseOfImage;
    mod.ImageSize = module->module.ImageSize;
    mod.TimeDateStamp = module->module.TimeDateStamp;
    mod.CheckSum = module->module.CheckSum;
    mod.NumSyms = module->module.NumSyms;
    mod.SymType = module->module.SymType;
    strcpy(mod.ModuleName, module->module.ModuleName);
    strcpy(mod.ImageName, module->module.ImageName);
    strcpy(mod.LoadedImageName, module->module.LoadedImageName);

    if (module->module.SymType == SymNone)
    {
        module = module_get_container(pcs, module);
        if (module && module->module.SymType != SymNone)
        {
            mod.SymType = module->module.SymType;
            mod.NumSyms = module->module.NumSyms;
        }
    }
    memcpy(ModuleInfo, &mod, ModuleInfo->SizeOfStruct);
    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfoW (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfoW(HANDLE hProcess, DWORD dwAddr, 
                               PIMAGEHLP_MODULEW ModuleInfo)
{
    IMAGEHLP_MODULE     mi;
    IMAGEHLP_MODULEW    miw;

    if (sizeof(miw) < ModuleInfo->SizeOfStruct) FIXME("Wrong size\n");

    mi.SizeOfStruct = sizeof(mi);
    if (!SymGetModuleInfo(hProcess, dwAddr, &mi)) return FALSE;

    miw.SizeOfStruct  = mi.SizeOfStruct;
    miw.BaseOfImage   = mi.BaseOfImage;
    miw.ImageSize     = mi.ImageSize;
    miw.TimeDateStamp = mi.TimeDateStamp;
    miw.CheckSum      = mi.CheckSum;
    miw.NumSyms       = mi.NumSyms;
    miw.SymType       = mi.SymType;
    MultiByteToWideChar(CP_ACP, 0, mi.ModuleName, -1,   
                        miw.ModuleName, sizeof(miw.ModuleName) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, mi.ImageName, -1,   
                        miw.ImageName, sizeof(miw.ImageName) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, mi.LoadedImageName, -1,   
                        miw.LoadedImageName, sizeof(miw.LoadedImageName) / sizeof(WCHAR));
    memcpy(ModuleInfo, &miw, ModuleInfo->SizeOfStruct);

    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfo64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfo64(HANDLE hProcess, DWORD64 dwAddr, 
                                PIMAGEHLP_MODULE64 ModuleInfo)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;

    TRACE("%p %s %p\n", hProcess, wine_dbgstr_longlong(dwAddr), ModuleInfo);

    if (!pcs) return FALSE;
    if (ModuleInfo->SizeOfStruct > sizeof(*ModuleInfo)) return FALSE;
    module = module_find_by_addr(pcs, dwAddr, DMT_UNKNOWN);
    if (!module) return FALSE;

    memcpy(ModuleInfo, &module->module, ModuleInfo->SizeOfStruct);

    if (module->module.SymType == SymNone)
    {
        module = module_get_container(pcs, module);
        if (module && module->module.SymType != SymNone)
        {
            ModuleInfo->SymType = module->module.SymType;
            ModuleInfo->NumSyms = module->module.NumSyms;
        }
    }
    return TRUE;
}

/******************************************************************
 *		SymGetModuleInfoW64 (DBGHELP.@)
 *
 */
BOOL  WINAPI SymGetModuleInfoW64(HANDLE hProcess, DWORD64 dwAddr, 
                                 PIMAGEHLP_MODULEW64 ModuleInfo)
{
    IMAGEHLP_MODULE64   mi;
    IMAGEHLP_MODULEW64  miw;

    if (sizeof(miw) < ModuleInfo->SizeOfStruct) FIXME("Wrong size\n");

    mi.SizeOfStruct = sizeof(mi);
    if (!SymGetModuleInfo64(hProcess, dwAddr, &mi)) return FALSE;

    miw.SizeOfStruct  = mi.SizeOfStruct;
    miw.BaseOfImage   = mi.BaseOfImage;
    miw.ImageSize     = mi.ImageSize;
    miw.TimeDateStamp = mi.TimeDateStamp;
    miw.CheckSum      = mi.CheckSum;
    miw.NumSyms       = mi.NumSyms;
    miw.SymType       = mi.SymType;
    MultiByteToWideChar(CP_ACP, 0, mi.ModuleName, -1,   
                        miw.ModuleName, sizeof(miw.ModuleName) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, mi.ImageName, -1,   
                        miw.ImageName, sizeof(miw.ImageName) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, mi.LoadedImageName, -1,   
                        miw.LoadedImageName, sizeof(miw.LoadedImageName) / sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, mi.LoadedPdbName, -1,   
                        miw.LoadedPdbName, sizeof(miw.LoadedPdbName) / sizeof(WCHAR));

    miw.CVSig         = mi.CVSig;
    MultiByteToWideChar(CP_ACP, 0, mi.CVData, -1,   
                        miw.CVData, sizeof(miw.CVData) / sizeof(WCHAR));
    miw.PdbSig        = mi.PdbSig;
    miw.PdbSig70      = mi.PdbSig70;
    miw.PdbAge        = mi.PdbAge;
    miw.PdbUnmatched  = mi.PdbUnmatched;
    miw.DbgUnmatched  = mi.DbgUnmatched;
    miw.LineNumbers   = mi.LineNumbers;
    miw.GlobalSymbols = mi.GlobalSymbols;
    miw.TypeInfo      = mi.TypeInfo;
    miw.SourceIndexed = mi.SourceIndexed;
    miw.Publics       = mi.Publics;

    memcpy(ModuleInfo, &miw, ModuleInfo->SizeOfStruct);

    return TRUE;
}

/***********************************************************************
 *		SymGetModuleBase (DBGHELP.@)
 */
DWORD WINAPI SymGetModuleBase(HANDLE hProcess, DWORD dwAddr)
{
    struct process*     pcs = process_find_by_handle(hProcess);
    struct module*      module;

    if (!pcs) return 0;
    module = module_find_by_addr(pcs, dwAddr, DMT_UNKNOWN);
    if (!module) return 0;
    return module->module.BaseOfImage;
}

/***********************************************************************
 *		SymGetModuleBase64 (DBGHELP.@)
 */
DWORD64 WINAPI SymGetModuleBase64(HANDLE hProcess, DWORD64 dwAddr)
{
    if (!validate_addr64(dwAddr)) return 0;
    return SymGetModuleBase(hProcess, (DWORD)dwAddr);
}

/******************************************************************
 *		module_reset_debug_info
 * Removes any debug information linked to a given module.
 */
void module_reset_debug_info(struct module* module)
{
    module->sortlist_valid = TRUE;
    module->addr_sorttab = NULL;
    hash_table_destroy(&module->ht_symbols);
    module->ht_symbols.num_buckets = 0;
    module->ht_symbols.buckets = NULL;
    hash_table_destroy(&module->ht_types);
    module->ht_types.num_buckets = 0;
    module->ht_types.buckets = NULL;
    module->vtypes.num_elts = 0;
    hash_table_destroy(&module->ht_symbols);
    module->sources_used = module->sources_alloc = 0;
    module->sources = NULL;
}
