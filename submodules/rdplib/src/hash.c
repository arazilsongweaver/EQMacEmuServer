// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifdef RDP_DEAD_CODE

#include "hash.h"

#include <stdlib.h>

// unused, retained for historical interest

void hash_init(hash_t *hash)
{
    hash->table = NULL;
    hash->table_size = 0;
}

uint32_t hash_create(hash_t *hash, uint32_t table_size, keycmp_f keycmp)
{
    uint32_t bin;
    int32_t result;

    result = 2;
    hash->table = (list_t *)malloc(sizeof(list_t) * table_size);
    if (hash->table)
    {
        for (bin = 0; bin < table_size; ++bin)
        {
            list_init(&hash->table[bin]);
            list_create(&hash->table[bin], 1, keycmp);
        }
        hash->table_size = table_size;
        result = 0;
    }
    return result;
}

void hash_destroy(hash_t *hash)
{
    uint32_t bin;

    if (hash->table)
    {
        for (bin = 0; bin < hash->table_size; ++bin)
        {
            list_destroy(&hash->table[bin]);
        }
        free(hash->table);
    }
}

void *hash_remove_any(hash_t *hash)
{
    uint32_t bin;
    void *item;

    item = NULL;
    for (bin = 0; bin < hash->table_size; ++bin)
    {
        item = list_remove_head(&hash->table[bin]);
        if (item)
        {
            break;
        }
    }
    return item;
}

#endif /* RDP_DEAD_CODE */

#ifndef RDP_DEAD_CODE
// suppress MSVC warning C4206 when this file becomes empty in a normal build
typedef int rdplib_hash_disabled_translation_unit;
#endif
