// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifdef RDP_DEAD_CODE

#include "cmp.h"
#include "dpc.h"
#include "utime.h"

// unused, retained for historical interest
uint32_t dpcq_create(dpcq_t *dpcq)
{
    return pqueue_create(&dpcq->q, 0x800u, uint32_cmp);
}

uint32_t dpcq_get_next_timeout(dpcq_t *dpcq)
{
    uint32_t timeout;
    dpc_t *dpc;

    timeout = (uint32_t)-1;
    dpc = dpcq_peek_head(dpcq);
    if (dpc)
    {
        timeout = dpc->time - time_get_ms();
        if ((int32_t)timeout < 0)
        {
            timeout = 0;
        }
    }

    return timeout;
}

#endif /* RDP_DEAD_CODE */

#ifndef RDP_DEAD_CODE
// suppress MSVC warning C4206 when this file becomes empty in a normal build
typedef int rdplib_dpc_disabled_translation_unit;
#endif
