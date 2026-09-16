// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "txq.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#include "dpf.h"
#endif
#ifdef RDP_DEAD_CODE
#include "fast.h"
#endif

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
void txq_destroy(txq_t *txq)
{
    msg_outgoing_t *msg;
    uint32_t item_count;

    item_count = list_get_size(&txq->list);
    while (item_count--)
    {
        msg = txq_remove_head(txq);
#ifdef RDPLIB_DEBUG
        dpf(0x20u, "message not acknowledged (%u)\n", msg->msgid);
#endif
        // The May 2002 routine subtracts the size a second time here. It is intentionally preserved only in this dead implementation.
        txq->queue_size -= msg->size;
        fast_free(msg);
    }
#ifdef RDPLIB_DEBUG
    assert(txq->queue_size == 0);
#endif
    list_destroy(&txq->list);
}
#endif

msg_outgoing_t *txq_remove_msgid(txq_t *txq, uint16_t msgid)
{
    msg_outgoing_t *msg;
    uint32_t item_count;
    msg_outgoing_t *msg_removed;

    msg_removed = NULL;
#ifdef RDPLIB_DEBUG
    dpf(2u, "removing msgid == %u\n", msgid);
#endif
    item_count = list_get_size(&txq->list);
    while (item_count--)
    {
        msg = (msg_outgoing_t *)list_remove_head(&txq->list);

        if (msg->msgid == msgid)
        {
#ifdef RDPLIB_DEBUG
            assert(msg_removed == NULL);
            dpf(2u, "removed msgid == %u\n", msg->msgid);
#endif
            txq->queue_size -= msg->size;
            msg_removed = msg;
        }
        else
        {
            list_add_tail(&txq->list, &msg->txq_link);
        }
    }

    return msg_removed;
}

uint32_t txq_get_oldest_time_sent(txq_t *txq)
{
    msg_outgoing_t *msg;
    uint32_t item_count;
    uint32_t oldest;

    item_count = list_get_size(&txq->list);
#ifdef RDPLIB_DEBUG
    assert(item_count != 0);
#endif
    msg = (msg_outgoing_t *)list_peek_head(&txq->list);
    oldest = msg->time_first_sent;
    while (item_count--)
    {
        msg = (msg_outgoing_t *)list_remove_head(&txq->list);
        if ((int32_t)(oldest - msg->time_first_sent) > 0)
        {
            oldest = msg->time_first_sent;
        }
        list_add_tail(&txq->list, &msg->txq_link);
    }
    return oldest;
}
