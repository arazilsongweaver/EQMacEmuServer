// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Non owning serial transmit queue. Removing a head assumes one exists and
// preserves the unchecked byte accounting.
#ifndef RDP_TX_BUFQ_H
#define RDP_TX_BUFQ_H

#include <stdint.h>

#include "layout.h"
#include "list.h"
#include "serial_tx.h"

typedef struct tx_bufq_t
{
    list_t list;
    uint32_t bytes;
} tx_bufq_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(tx_bufq_t, list, 0x00);
RDP_ASSERT_OFFSET(tx_bufq_t, bytes, 0x14);
RDP_STATIC_ASSERT(sizeof(tx_bufq_t) == 0x18, "tx_bufq_t must be 0x18 bytes on Win32");
#endif

static void tx_bufq_init(tx_bufq_t *tx_bufq)
{
    list_init(&tx_bufq->list);
    tx_bufq->bytes = 0;
}

static void tx_bufq_create(tx_bufq_t *tx_bufq)
{
    list_create(&tx_bufq->list, 0, NULL);
}

static void tx_bufq_destroy(tx_bufq_t *tx_bufq)
{
    list_destroy(&tx_bufq->list);
}

static serial_tx_buf_t *tx_bufq_remove_head(tx_bufq_t *tx_bufq)
{
    serial_tx_buf_t *o;

    o = (serial_tx_buf_t *)list_remove_head(&tx_bufq->list);
    tx_bufq->bytes -= o->write_size;
    return o;
}

static serial_tx_buf_t *tx_bufq_peek_head(tx_bufq_t *tx_bufq)
{
    return (serial_tx_buf_t *)list_peek_head(&tx_bufq->list);
}

static void tx_bufq_add_tail(tx_bufq_t *tx_bufq, serial_tx_buf_t *ptr)
{
    tx_bufq->bytes += ptr->write_size;
    list_add_tail(&tx_bufq->list, &ptr->link);
}

static uint32_t tx_bufq_get_bytes(tx_bufq_t *tx_bufq)
{
    return tx_bufq->bytes;
}

static uint32_t tx_bufq_get_size(tx_bufq_t *tx_bufq)
{
    return list_get_size(&tx_bufq->list);
}

#endif /* RDP_TX_BUFQ_H */
