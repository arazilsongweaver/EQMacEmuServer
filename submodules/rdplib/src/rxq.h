// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_RXQ_H
#define RDP_RXQ_H

#include <stdint.h>

#include "layout.h"
#include "list.h"
#include "msg_arrival.h"

typedef struct _rxq_t
{
    list_t list;
} rxq_t, *Prxq_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(rxq_t, list, 0x00);
RDP_STATIC_ASSERT(sizeof(rxq_t) == 0x14, "rxq_t must be 0x14 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t rxq_flush_all_messages(rxq_t *rxq, connection_t *c);

static void rxq_init(rxq_t *rxq)
{
    list_init(&rxq->list);
}

static void rxq_destroy(rxq_t *rxq)
{
    list_destroy(&rxq->list);
}

static msg_arrival_t *rxq_remove_head(rxq_t *rxq)
{
    return (msg_arrival_t *)list_remove_head(&rxq->list);
}

static msg_arrival_t *rxq_peek_head(rxq_t *rxq)
{
    return (msg_arrival_t *)list_peek_head(&rxq->list);
}

static void rxq_add_tail(rxq_t *rxq, msg_arrival_t *ptr)
{
    list_add_tail(&rxq->list, &ptr->rxq_link);
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_RXQ_H */
