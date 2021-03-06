/*
 * File source.c - source files management
 *
 * Copyright (C) 2004,      Eric Pouech.
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
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dbghelp_private.h"
#include "wine/debug.h"
#ifdef HAVE_REGEX_H
# include <regex.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

/******************************************************************
 *		source_find
 *
 * check whether a source file has already been stored
 */
static unsigned source_find(const struct module* module, const char* name)
{
    char*       ptr = module->sources;

    while (*ptr)
    {
        if (strcmp(ptr, name) == 0) return ptr - module->sources;
        ptr += strlen(ptr) + 1;
    }
    return (unsigned)-1;
}

/******************************************************************
 *		source_new
 *
 * checks if source exists. if not, add it
 */
unsigned source_new(struct module* module, const char* base, const char* name)
{
    int         len;
    unsigned    ret;
    const char* full;

    if (!name) return (unsigned)-1;
    if (!base || *name == '/')
        full = name;
    else
    {
        unsigned bsz = strlen(base);
        char* tmp = HeapAlloc(GetProcessHeap(), 0, bsz + 1 + strlen(name) + 1);

        if (!tmp) return (unsigned)-1;
        full = tmp;
        strcpy(tmp, base);
        if (tmp[bsz - 1] != '/') tmp[bsz++] = '/';
        strcpy(&tmp[bsz], name);
    }
    if (module->sources && (ret = source_find(module, full)) != (unsigned)-1)
        return ret;

    len = strlen(full) + 1;
    if (module->sources_used + len + 1 > module->sources_alloc)
    {
        /* Alloc by block of 256 bytes */
        module->sources_alloc = (module->sources_used + len + 1 + 255) & ~255;
        if (!module->sources)
            module->sources = HeapAlloc(GetProcessHeap(), 0, module->sources_alloc);
        else
            module->sources = HeapReAlloc(GetProcessHeap(), 0, module->sources,
                                          module->sources_alloc);
    }
    ret = module->sources_used;
    strcpy(module->sources + module->sources_used, full);
    module->sources_used += len;
    module->sources[module->sources_used] = '\0';
    if (full != name) HeapFree(GetProcessHeap(), 0, (char*)full);
    return ret;
}

/******************************************************************
 *		source_get
 *
 * returns a stored source file name
 */
const char* source_get(const struct module* module, unsigned idx)
{
    if (idx == -1) return "";
    assert(module->sources);
    return module->sources + idx;
}

/******************************************************************
 *		SymEnumSourceFiles (DBGHELP.@)
 *
 */
BOOL WINAPI SymEnumSourceFiles(HANDLE hProcess, ULONG64 ModBase, PCSTR Mask,
                               PSYM_ENUMSOURCEFILES_CALLBACK cbSrcFiles,
                               PVOID UserContext)
{
    struct process*     pcs;
    struct module_pair  pair;
    SOURCEFILE          sf;
    char*               ptr;
    
    if (!cbSrcFiles) return FALSE;
    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
         
    if (ModBase)
    {
        pair.requested = module_find_by_addr(pcs, ModBase, DMT_UNKNOWN);
        if (!module_get_debug(pcs, &pair)) return FALSE;
    }
    else
    {
        if (Mask[0] == '!')
        {
            pair.requested = module_find_by_name(pcs, Mask + 1, DMT_UNKNOWN);
            if (!module_get_debug(pcs, &pair)) return FALSE;
        }
        else
        {
            FIXME("Unsupported yet (should get info from current context)\n");
            return FALSE;
        }
    }
    if (!pair.effective->sources) return FALSE;
    for (ptr = pair.effective->sources; *ptr; ptr += strlen(ptr) + 1)
    {
        /* FIXME: not using Mask */
        sf.ModBase = ModBase;
        sf.FileName = ptr;
        if (!cbSrcFiles(&sf, UserContext)) break;
    }

    return TRUE;
}

/******************************************************************
 *		SymEnumLines (DBGHELP.@)
 *
 */
BOOL WINAPI SymEnumLines(HANDLE hProcess, ULONG64 base, PCSTR compiland,
                         PCSTR srcfile, PSYM_ENUMLINES_CALLBACK cb, PVOID user)
{
    struct process*             pcs;
    struct module_pair          pair;
    struct hash_table_iter      hti;
    struct symt_ht*             sym;
    regex_t                     re;
    struct line_info*           dli;
    void*                       ptr;
    SRCCODEINFO                 sci;
    const char*                 file;

    if (!cb) return FALSE;
    pcs = process_find_by_handle(hProcess);
    if (!pcs) return FALSE;
    if (!(dbghelp_options & SYMOPT_LOAD_LINES)) return TRUE;
    if (regcomp(&re, srcfile, REG_NOSUB))
    {
        FIXME("Couldn't compile %s\n", srcfile);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (compiland) FIXME("Unsupported yet (filtering on compiland %s)\n", compiland);
    pair.requested = module_find_by_addr(pcs, base, DMT_UNKNOWN);
    if (!module_get_debug(pcs, &pair)) return FALSE;

    sci.SizeOfStruct = sizeof(sci);
    sci.ModBase      = base;

    hash_table_iter_init(&pair.effective->ht_symbols, &hti, NULL);
    while ((ptr = hash_table_iter_up(&hti)))
    {
        sym = GET_ENTRY(ptr, struct symt_ht, hash_elt);
        if (sym->symt.tag != SymTagFunction) continue;

        dli = NULL;
        sci.FileName[0] = '\0';
        while ((dli = vector_iter_up(&((struct symt_function*)sym)->vlines, dli)))
        {
            if (dli->is_source_file)
            {
                file = source_get(pair.effective, dli->u.source_file);
                if (regexec(&re, file, 0, NULL, 0) != 0) file = "";
                strcpy(sci.FileName, file);
            }
            else if (sci.FileName[0])
            {
                sci.Key = dli;
                sci.Obj[0] = '\0'; /* FIXME */
                sci.LineNumber = dli->line_number;
                sci.Address = dli->u.pc_offset;
                if (!cb(&sci, user)) break;
            }
        }
    }
    return TRUE;
}

/******************************************************************
 *		SymGetSourceFileToken (DBGHELP.@)
 *
 */
BOOL WINAPI SymGetSourceFileToken(HANDLE hProcess, ULONG64 base,
                                  PCSTR src, PVOID* token, DWORD* size)
{
    FIXME("%p %s %s %p %p: stub!\n",
          hProcess, wine_dbgstr_longlong(base), debugstr_a(src), token, size);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

/******************************************************************
 *		SymGetSourceFileTokenW (DBGHELP.@)
 *
 */
BOOL WINAPI SymGetSourceFileTokenW(HANDLE hProcess, ULONG64 base,
                                   PCWSTR src, PVOID* token, DWORD* size)
{
    FIXME("%p %s %s %p %p: stub!\n",
          hProcess, wine_dbgstr_longlong(base), debugstr_w(src), token, size);
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
