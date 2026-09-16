// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "eventq.h"

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "utime.h"

static int ascending_timeout_data_cmp(const void *t1, const void *t2)
{
    const timeout_data *time2;
    const timeout_data *time1;
    int result;

    time1 = (const timeout_data *)t1;
    time2 = (const timeout_data *)t2;
    result = (int32_t)(time1->infinite - time2->infinite);
    if (result == 0 && time1->infinite == 0)
    {
        result = (int32_t)(time1->time - time2->time);
        if (result > 0)
        {
            result = 1;
        }
        else if (result < 0)
        {
            result = -1;
        }
    }
    return result;
}
uint32_t eventq_create(eventq_t *eq, uint32_t grow_size)
{
    umutex_create(&eq->lock);
    return pqueue_create(&eq->q, grow_size, ascending_timeout_data_cmp);
}

struct timeval *eventq_get_event_timeout(eventq_t *eq, struct timeval *timeout_ptr)
{
    connection_t *c;
    uint32_t timeout;

    c = eventq_peek_head(eq);
    if (c && !c->cn_event_time.infinite)
    {
        timeout = c->cn_event_time.time - time_get_ms();
        if ((int32_t)timeout < 0)
        {
            timeout_ptr->tv_sec = 0;
            timeout_ptr->tv_usec = 0;
        }
        else
        {
            timeout_ptr->tv_sec = (long)(timeout / 1000u);
            timeout_ptr->tv_usec = (long)(1000u * (timeout % 1000u));
        }
#ifdef RDPLIB_DEBUG
        dpf(0x40u, "select timeout: %u secs, %u usecs\n", (uint32_t)timeout_ptr->tv_sec, (uint32_t)timeout_ptr->tv_usec);
#endif
    }
    else
    {
        timeout_ptr = NULL;
#ifdef RDPLIB_DEBUG
        dpf(0x40u, "select timeout: infinite\n");
#endif
    }
    return timeout_ptr;
}
