// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Intrusive list used throughout RDP. The list owns neither its links nor their items.
#ifndef RDP_LIST_H
#define RDP_LIST_H

#include <stddef.h>
#include <stdint.h>

#include "layout.h"

typedef int (*keycmp_f)(const void *key_1, const void *key_2);

typedef union _key
{
    int32_t i;
    void *p;
} key, *Pkey;

// The May 2002 Windows typedef was named link, which conflicts with the POSIX link() function.
typedef struct _link
{
    struct _link *next;
    struct _link *prev;
    void *item;
    union _key key;
} rdp_link_t, *Plink;

typedef struct _list_t
{
    rdp_link_t *head;
    rdp_link_t *tail;
    uint32_t size;
    keycmp_f keycmp;
    uint32_t sorted;
} list_t, *Plist_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(rdp_link_t, next, 0x00);
RDP_ASSERT_OFFSET(rdp_link_t, prev, 0x04);
RDP_ASSERT_OFFSET(rdp_link_t, item, 0x08);
RDP_ASSERT_OFFSET(rdp_link_t, key, 0x0C);
RDP_STATIC_ASSERT(sizeof(rdp_link_t) == 0x10, "rdp_link_t must be 0x10 bytes on Win32");
RDP_ASSERT_OFFSET(list_t, head, 0x00);
RDP_ASSERT_OFFSET(list_t, tail, 0x04);
RDP_ASSERT_OFFSET(list_t, size, 0x08);
RDP_ASSERT_OFFSET(list_t, keycmp, 0x0C);
RDP_ASSERT_OFFSET(list_t, sorted, 0x10);
RDP_STATIC_ASSERT(sizeof(list_t) == 0x14, "list_t must be 0x14 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void list_init(list_t *list);
// Sets only sorted and keycmp; list_init or equivalent zero initialization must run first.
void list_create(list_t *list, uint32_t sorted, keycmp_f keycmp);
// The list must be empty. Links and their items remain owned by the caller.
void list_destroy(list_t *list);
void list_add_head(list_t *list, rdp_link_t *link);
void list_add_tail(list_t *list, rdp_link_t *link);
void *list_remove_head(list_t *list);
void *list_remove_tail(list_t *list);
void list_insert(list_t *list, rdp_link_t *link);
rdp_link_t *list_find_link_by_key(list_t *list, const void *key);
// The supplied link must be a live member. Removal deliberately leaves its next and prev fields unchanged.
void *list_remove_by_link(list_t *list, rdp_link_t *link);

static uint32_t list_get_size(list_t *l)
{
    return l->size;
}

static void *list_peek_head(list_t *l)
{
    if (l->head)
    {
        return l->head->item;
    }
    return NULL;
}

static void *list_lookup(list_t *l, const void *key)
{
    rdp_link_t *link;

    link = list_find_link_by_key(l, key);
    if (link)
    {
        return link->item;
    }
    return NULL;
}

static void *list_remove_by_key(list_t *l, const void *key)
{
    rdp_link_t *link;

    link = list_find_link_by_key(l, key);
    if (link)
    {
        return list_remove_by_link(l, link);
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_LIST_H */
