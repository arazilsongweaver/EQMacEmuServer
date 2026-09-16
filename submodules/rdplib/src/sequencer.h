// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_SEQUENCER_H
#define RDP_SEQUENCER_H

#include "cmp.h"
#include "layout.h"
#include "list.h"
#include "msg_arrival.h"

typedef struct _sequencer_t
{
    list_t list;
} sequencer_t, *Psequencer_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(sequencer_t, list, 0x00);
RDP_STATIC_ASSERT(sizeof(sequencer_t) == 0x14, "sequencer_t must be 0x14 bytes on Win32");
#endif

static void sequencer_init(sequencer_t *sequencer)
{
    list_init(&sequencer->list);
}

static void sequencer_create(sequencer_t *sequencer)
{
    list_create(&sequencer->list, 1, uint8_cmp);
}

static void sequencer_destroy(sequencer_t *sequencer)
{
    list_destroy(&sequencer->list);
}

static msg_arrival_t *sequencer_remove_head(sequencer_t *sequencer)
{
    return (msg_arrival_t *)list_remove_head(&sequencer->list);
}

static msg_arrival_t *sequencer_peek_head(sequencer_t *sequencer)
{
    return (msg_arrival_t *)list_peek_head(&sequencer->list);
}

static void sequencer_insert(sequencer_t *sequencer, msg_arrival_t *ptr)
{
    list_insert(&sequencer->list, &ptr->rxq_link);
}

#endif /* RDP_SEQUENCER_H */
