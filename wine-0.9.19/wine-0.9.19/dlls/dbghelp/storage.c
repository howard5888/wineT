/*
 * Various storage structures (pool allocation, vector, hash table)
 *
 * Copyright (C) 1993, Eric Youngdale.
 *               2004, Eric Pouech
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
#include <assert.h>
#include <stdlib.h>
#include "wine/debug.h"

#include "dbghelp_private.h"
#ifdef USE_STATS
#include <math.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

struct pool_arena
{
    struct pool_arena*  next;
    char*               current;
};

void pool_init(struct pool* a, unsigned arena_size)
{
    a->arena_size = arena_size;
    a->first = NULL;
}

void pool_destroy(struct pool* pool)
{
    struct pool_arena*  arena;
    struct pool_arena*  next;

#ifdef USE_STATS
    unsigned    alloc, used, num;
    
    for (alloc = used = num = 0, arena = pool->first; arena; arena = arena->next)
    {
        alloc += pool->arena_size;
        used += arena->current - (char*)arena;
        num++;
    }
    FIXME("STATS: pool %p has allocated %u kbytes, used %u kbytes in %u arenas,\n"
          "\t\t\t\tnon-allocation ratio: %.2f%%\n",
          pool, alloc >> 10, used >> 10, num, 100.0 - (float)used / (float)alloc * 100.0);
#endif

    for (arena = pool->first; arena; arena = next)
    {
        next = arena->next;
        HeapFree(GetProcessHeap(), 0, arena);
    }
    pool_init(pool, 0);
}

void* pool_alloc(struct pool* pool, unsigned len)
{
    struct pool_arena** parena;
    struct pool_arena*  arena;
    void*               ret;

    len = (len + 3) & ~3; /* round up size on DWORD boundary */
    assert(sizeof(struct pool_arena) + len <= pool->arena_size && len);

    for (parena = &pool->first; *parena; parena = &(*parena)->next)
    {
        if ((char*)(*parena) + pool->arena_size - (*parena)->current >= len)
        {
            ret = (*parena)->current;
            (*parena)->current += len;
            return ret;
        }
    }
 
    arena = HeapAlloc(GetProcessHeap(), 0, pool->arena_size);
    if (!arena) {FIXME("OOM\n");return NULL;}

    *parena = arena;

    ret = (char*)arena + sizeof(*arena);
    arena->next = NULL;
    arena->current = (char*)ret + len;
    return ret;
}

static struct pool_arena* pool_is_last(struct pool* pool, void* p, unsigned old_size)
{
    struct pool_arena*  arena;

    for (arena = pool->first; arena; arena = arena->next)
    {
        if (arena->current == (char*)p + old_size) return arena;
    }
    return NULL;
}

static void* pool_realloc(struct pool* pool, void* p, unsigned old_size, unsigned new_size)
{
    struct pool_arena*  arena;
    void*               new;

    if ((arena = pool_is_last(pool, p, old_size)) && 
        (char*)p + new_size <= (char*)arena + pool->arena_size)
    {
        arena->current = (char*)p + new_size;
        return p;
    }
    if ((new = pool_alloc(pool, new_size)) && old_size)
        memcpy(new, p, min(old_size, new_size));
    return new;
}

char* pool_strdup(struct pool* pool, const char* str)
{
    char* ret;
    if ((ret = pool_alloc(pool, strlen(str) + 1))) strcpy(ret, str);
    return ret;
}

void vector_init(struct vector* v, unsigned esz, unsigned bucket_sz)
{
    v->buckets = NULL;
    /* align size on DWORD boundaries */
    v->elt_size = (esz + 3) & ~3;
    switch (bucket_sz)
    {
    case    2: v->shift =  1; break;
    case    4: v->shift =  2; break;
    case    8: v->shift =  3; break;
    case   16: v->shift =  4; break;
    case   32: v->shift =  5; break;
    case   64: v->shift =  6; break;
    case  128: v->shift =  7; break;
    case  256: v->shift =  8; break;
    case  512: v->shift =  9; break;
    case 1024: v->shift = 10; break;
    default: assert(0);
    }
    v->num_buckets = 0;
    v->num_elts = 0;
}

unsigned vector_length(const struct vector* v)
{
    return v->num_elts;
}

void* vector_at(const struct vector* v, unsigned pos)
{
    unsigned o;

    if (pos >= v->num_elts) return NULL;
    o = pos & ((1 << v->shift) - 1);
    return (char*)v->buckets[pos >> v->shift] + o * v->elt_size;
}

void* vector_add(struct vector* v, struct pool* pool)
{
    unsigned    ncurr = v->num_elts++;

    /* check that we don't wrap around */
    assert(v->num_elts > ncurr);
    if (ncurr == (v->num_buckets << v->shift))
    {
        v->buckets = pool_realloc(pool, v->buckets,
                                  v->num_buckets * sizeof(void*),
                                  (v->num_buckets + 1) * sizeof(void*));
        v->buckets[v->num_buckets] = pool_alloc(pool, v->elt_size << v->shift);
        return v->buckets[v->num_buckets++];
    }
    return vector_at(v, ncurr);
}

static unsigned vector_position(const struct vector* v, const void* elt)
{
    int i;

    for (i = 0; i < v->num_buckets; i++)
    {
        if (v->buckets[i] <= elt && 
            (const char*)elt < (const char*)v->buckets[i] + (v->elt_size << v->shift))
        {
            return (i << v->shift) + 
                ((const char*)elt - (const char*)v->buckets[i]) / v->elt_size;
        }
    }
    assert(0);
    return 0;
}

void* vector_iter_up(const struct vector* v, void* elt)
{
    unsigned    pos;

    if (!elt) return vector_at(v, 0);
    pos = vector_position(v, elt) + 1;
    if (pos >= vector_length(v)) return NULL;
    return vector_at(v, pos);
}

void* vector_iter_down(const struct vector* v, void* elt)
{
    unsigned    pos;
    if (!elt) return vector_at(v, vector_length(v) - 1);
    pos = vector_position(v, elt);
    if (pos == 0) return NULL;
    return vector_at(v, pos - 1);
}

/* We construct the sparse array as two vectors (of equal size)
 * The first vector (key2index) is the lookup table between the key and
 * an index in the second vector (elements)
 * When inserting an element, it's always appended in second vector (and
 * never moved in memory later on), only the first vector is reordered
 */
struct key2index
{
    unsigned long       key;
    unsigned            index;
};

void sparse_array_init(struct sparse_array* sa, unsigned elt_sz, unsigned bucket_sz)
{
    vector_init(&sa->key2index, sizeof(struct key2index), bucket_sz);
    vector_init(&sa->elements, elt_sz, bucket_sz);
}

/******************************************************************
 *		spare_array_lookup
 *
 * Returns the first index which key is >= at passed key
 */
static struct key2index* spare_array_lookup(const struct sparse_array* sa,
                                            unsigned long key, unsigned* idx)
{
    struct key2index*   pk2i;

    /* FIXME: should use bsearch here */
    for (*idx = 0; *idx < sa->elements.num_elts; (*idx)++)
    {
        pk2i = vector_at(&sa->key2index, *idx);
        if (pk2i && pk2i->key >= key) return pk2i;
    }
    return NULL;
}

void*   sparse_array_find(const struct sparse_array* sa, unsigned long key)
{
    unsigned            idx;
    struct key2index*   pk2i;

    if ((pk2i = spare_array_lookup(sa, key, &idx)) && pk2i->key == key)
        return vector_at(&sa->elements, pk2i->index);
    return NULL;
}

void*   sparse_array_add(struct sparse_array* sa, unsigned long key, 
                         struct pool* pool)
{
    unsigned            idx, i;
    struct key2index*   pk2i;
    struct key2index*   to;

    pk2i = spare_array_lookup(sa, key, &idx);
    if (pk2i && pk2i->key == key)
    {
        FIXME("re adding an existing key\n");
        return NULL;
    }
    to = vector_add(&sa->key2index, pool);
    if (pk2i)
    {
        /* we need to shift vector's content... */
        /* let's do it brute force... (FIXME) */
        assert(sa->key2index.num_elts >= 2);
        for (i = sa->key2index.num_elts - 1; i > idx; i--)
        {
            pk2i = vector_at(&sa->key2index, i - 1);
            *to = *pk2i;
            to = pk2i;
        }
    }

    to->key = key;
    to->index = sa->elements.num_elts;

    return vector_add(&sa->elements, pool);
}

unsigned sparse_array_length(const struct sparse_array* sa)
{
    return sa->elements.num_elts;
}

unsigned hash_table_hash(const char* name, unsigned num_buckets)
{
    unsigned    hash = 0;
    while (*name)
    {
        hash += *name++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % num_buckets;
}

void hash_table_init(struct pool* pool, struct hash_table* ht, unsigned num_buckets)
{
    ht->buckets = pool_alloc(pool, num_buckets * sizeof(struct hash_table_elt*));
    assert(ht->buckets);
    ht->num_buckets = num_buckets;
    memset(ht->buckets, 0, num_buckets * sizeof(struct hash_table_elt*));
}

void hash_table_destroy(struct hash_table* ht)
{
#if defined(USE_STATS)
    int                         i;
    unsigned                    len;
    unsigned                    num = 0, min = 0xffffffff, max = 0, sq = 0;
    struct hash_table_elt*      elt;
    double                      mean, variance;

    for (i = 0; i < ht->num_buckets; i++)
    {
        for (len = 0, elt = ht->buckets[i]; elt; elt = elt->next) len++;
        if (len < min) min = len;
        if (len > max) max = len;
        num += len;
        sq += len * len;
    }
    mean = (double)num / ht->num_buckets;
    variance = (double)sq / ht->num_buckets - mean * mean;
    FIXME("STATS: elts[num:%-4u size:%u mean:%f] buckets[min:%-4u variance:%+f max:%-4u]\n",
          num, ht->num_buckets, mean, min, variance, max);
#if 1
    for (i = 0; i < ht->num_buckets; i++)
    {
        for (len = 0, elt = ht->buckets[i]; elt; elt = elt->next) len++;
        if (len == max)
        {
            FIXME("Longuest bucket:\n");
            for (elt = ht->buckets[i]; elt; elt = elt->next)
                FIXME("\t%s\n", elt->name);
            break;
        }

    }
#endif
#endif
}

void hash_table_add(struct hash_table* ht, struct hash_table_elt* elt)
{
    unsigned                    hash = hash_table_hash(elt->name, ht->num_buckets);
    struct hash_table_elt**     p;

    /* in some cases, we need to get back the symbols of same name in the order
     * in which they've been inserted. So insert new elements at the end of the list.
     */
    for (p = &ht->buckets[hash]; *p; p = &((*p)->next));
    *p = elt;
    elt->next = NULL;
}

void* hash_table_find(const struct hash_table* ht, const char* name)
{
    unsigned                    hash = hash_table_hash(name, ht->num_buckets);
    struct hash_table_elt*      elt;

    for (elt = ht->buckets[hash]; elt; elt = elt->next)
        if (!strcmp(name, elt->name)) return elt;
    return NULL;
}

void hash_table_iter_init(const struct hash_table* ht, 
                          struct hash_table_iter* hti, const char* name)
{
    hti->ht = ht;
    if (name)
    {
        hti->last = hash_table_hash(name, ht->num_buckets);
        hti->index = hti->last - 1;
    }
    else
    {
        hti->last = ht->num_buckets - 1;
        hti->index = -1;
    }
    hti->element = NULL;
}

void* hash_table_iter_up(struct hash_table_iter* hti)
{
    if (hti->element) hti->element = hti->element->next;
    while (!hti->element && hti->index < hti->last) 
        hti->element = hti->ht->buckets[++hti->index];
    return hti->element;
}
