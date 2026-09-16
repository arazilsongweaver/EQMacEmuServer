// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// The hash owns one membership reference per inserted connection, but not the
// connection allocation itself. Each bucket protects its sorted intrusive list.
#ifndef RDPLIB_CONNHASH_H
#define RDPLIB_CONNHASH_H

#include <stdint.h>

#include "connection.h"
#include "layout.h"
#include "list.h"
#include "umutex.h"

typedef struct _hashbin_t
{
    umutex_t lock;
    list_t list;
} hashbin_t, *Phashbin_t;

typedef struct _connhash_t
{
    uint16_t table_size;
    hashbin_t *table;
} connhash_t, *Pconnhash_t;

// Address keys use the Windows/Linux 16 byte sockaddr layout with a 16 bit family at offset 0. BSD/macOS native sockaddrs require normalization in the platform layer.
// Inserted endpoint keys must remain immutable and unique. connhash_lock returns
// a locked temporary reference; subref/unlock require a live reference. Every
// bucket must be empty before connhash_destroy.

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(hashbin_t, list, 0x18 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_STATIC_ASSERT(sizeof(hashbin_t) == 0x2C + RDP_WIN32_UMUTEX_OWNER_BYTES, "hashbin_t has the expected Win32 layout");
RDP_ASSERT_OFFSET(connhash_t, table, 0x04);
RDP_STATIC_ASSERT(sizeof(connhash_t) == 0x08, "connhash_t must be 0x08 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void connhash_init(connhash_t *ch);
int sockaddr_cmp(const void *sockaddr_1, const void *sockaddr_2);
uint32_t connhash_create(connhash_t *ch, uint16_t table_size);
void connhash_destroy(connhash_t *ch);
connection_t *connhash_lock(connhash_t *ch, struct sockaddr *sa);
void connhash_insert(connhash_t *ch, connection_t *c);
connection_t *connhash_subref(connhash_t *ch, connection_t *c);

#ifdef __cplusplus
}
#endif

static connection_t *connhash_unlock(connhash_t *ch, connection_t *c)
{
    umutex_unlock(&c->cn_lock);
    return connhash_subref(ch, c);
}

#endif /* RDPLIB_CONNHASH_H */
