// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rxq.h"

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "fast.h"

uint32_t rxq_flush_all_messages(rxq_t *rxq, connection_t *c)
{
    msg_arrival_t *msg;
    int queue_size;
    uint32_t removed;

    removed = 0;
    queue_size = list_get_size(&rxq->list);
    while (queue_size--)
    {
        msg = rxq_remove_head(rxq);
        if (msg->sender == c)
        {
            ++removed;
#ifdef RDPLIB_DEBUG
            dpf(0x20u, "connection 0x%08x: message discarded on close\n", (uint32_t)(uintptr_t)c);
#endif
            fast_free(msg);
        }
        else
        {
            rxq_add_tail(rxq, msg);
        }
    }
    return removed;
}
