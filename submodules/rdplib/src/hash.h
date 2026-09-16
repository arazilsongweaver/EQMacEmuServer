// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDPLIB_HASH_H
#define RDPLIB_HASH_H

#ifdef RDP_DEAD_CODE

// unused, retained for historical interest

#include <stdint.h>

#include "layout.h"
#include "list.h"

typedef uint32_t (*keyhash_f)(void *);

typedef struct _hash_t
{
    list_t *table;
    uint32_t table_size;
    keyhash_f keyhash;
} hash_t, *Phash_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(hash_t, table, 0x00);
RDP_ASSERT_OFFSET(hash_t, table_size, 0x04);
RDP_ASSERT_OFFSET(hash_t, keyhash, 0x08);
RDP_STATIC_ASSERT(sizeof(hash_t) == 0x0C, "hash_t must be 0x0C bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void hash_init(hash_t *hash);
uint32_t hash_create(hash_t *hash, uint32_t table_size, keycmp_f keycmp);
void hash_destroy(hash_t *hash);
void *hash_remove_any(hash_t *hash);

#ifdef __cplusplus
}
#endif

#endif /* RDP_DEAD_CODE */

#endif /* RDPLIB_HASH_H */
