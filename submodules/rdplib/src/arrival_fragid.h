// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_ARRIVAL_FRAGID_H
#define RDP_ARRIVAL_FRAGID_H

#include <stdint.h>

#include "cmp.h"
#include "layout.h"
#include "list.h"
#include "msg_arrival.h"

typedef struct _arrival_fragid_t
{
    list_t list;
} arrival_fragid_t, *Parrival_fragid_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(arrival_fragid_t, list, 0x00);
RDP_STATIC_ASSERT(sizeof(arrival_fragid_t) == 0x14, "arrival_fragid_t must be 0x14 bytes on Win32");
#endif

static void arrival_fragid_init(arrival_fragid_t *arrival_fragid)
{
    list_init(&arrival_fragid->list);
}

static void arrival_fragid_create(arrival_fragid_t *arrival_fragid)
{
    list_create(&arrival_fragid->list, 1, uint16_cmp);
}

static void arrival_fragid_destroy(arrival_fragid_t *arrival_fragid)
{
    list_destroy(&arrival_fragid->list);
}

static msg_arrival_t *arrival_fragid_remove_head(arrival_fragid_t *arrival_fragid)
{
    return (msg_arrival_t *)list_remove_head(&arrival_fragid->list);
}

static msg_arrival_t *arrival_fragid_lookup(arrival_fragid_t *arrival_fragid, uint16_t *key)
{
    return (msg_arrival_t *)list_lookup(&arrival_fragid->list, key);
}

static msg_arrival_t *arrival_fragid_remove_by_ptr(arrival_fragid_t *arrival_fragid, msg_arrival_t *ptr)
{
    return (msg_arrival_t *)list_remove_by_link(&arrival_fragid->list, &ptr->rxq_link);
}

static void arrival_fragid_insert(arrival_fragid_t *arrival_fragid, msg_arrival_t *ptr)
{
    list_insert(&arrival_fragid->list, &ptr->rxq_link);
}

#endif /* RDP_ARRIVAL_FRAGID_H */
