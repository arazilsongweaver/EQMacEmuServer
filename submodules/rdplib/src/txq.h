// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_TXQ_H
#define RDP_TXQ_H

#include <stdint.h>

#include "layout.h"
#include "list.h"
#include "msg_outgoing.h"

typedef struct _txq_t
{
    list_t list;
    uint32_t queue_size;
} txq_t, *Ptxq_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(txq_t, list, 0x00);
RDP_ASSERT_OFFSET(txq_t, queue_size, 0x14);
RDP_STATIC_ASSERT(sizeof(txq_t) == 0x18, "txq_t must be 0x18 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
void txq_destroy(txq_t *txq);
#endif

msg_outgoing_t *txq_remove_msgid(txq_t *txq, uint16_t msgid);
uint32_t txq_get_oldest_time_sent(txq_t *txq);

static void txq_init(txq_t *txq)
{
    list_init(&txq->list);
}

static void txq_create(txq_t *txq)
{
    txq->queue_size = 0;
    list_create(&txq->list, 0, NULL);
}

static msg_outgoing_t *txq_remove_head(txq_t *txq)
{
    msg_outgoing_t *outgoing;

    outgoing = (msg_outgoing_t *)list_remove_head(&txq->list);
    if (outgoing)
    {
        txq->queue_size -= outgoing->size;
    }
    return outgoing;
}

static msg_outgoing_t *txq_peek_head(txq_t *txq)
{
    return (msg_outgoing_t *)list_peek_head(&txq->list);
}

static void txq_add_tail(txq_t *txq, msg_outgoing_t *outgoing)
{
    txq->queue_size += outgoing->size;
    list_add_tail(&txq->list, &outgoing->txq_link);
}

static uint32_t txq_get_queue_size(txq_t *txq)
{
    return txq->queue_size;
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_TXQ_H */
