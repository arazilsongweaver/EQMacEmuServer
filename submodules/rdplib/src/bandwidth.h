// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_BANDWIDTH_H
#define RDP_BANDWIDTH_H

#include <stdint.h>

#include "layout.h"

typedef struct _bandwidth_t
{
    uint32_t queue_size; // Virtual backlog drained at bandwidth bytes per second.
    uint32_t queue_time; // Millisecond clock sample of the last backlog update.
    uint32_t bandwidth;
    uint32_t autoadjust; // not used
} bandwidth_t, *Pbandwidth_t;

RDP_ASSERT_OFFSET(bandwidth_t, queue_size, 0x00);
RDP_ASSERT_OFFSET(bandwidth_t, queue_time, 0x04);
RDP_ASSERT_OFFSET(bandwidth_t, bandwidth, 0x08);
RDP_ASSERT_OFFSET(bandwidth_t, autoadjust, 0x0C);
RDP_STATIC_ASSERT(sizeof(bandwidth_t) == 0x10, "bandwidth_t must be 0x10 bytes");

#ifdef __cplusplus
extern "C"
{
#endif

void bandwidth_init(bandwidth_t *bandwidth);
void bandwidth_enqueue_bytes(bandwidth_t *bandwidth, uint32_t size);
uint32_t bandwidth_get_queue_size(bandwidth_t *bandwidth);
uint32_t bandwidth_get_time_empty(bandwidth_t *bandwidth);

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
void bandwidth_set_queue_size(bandwidth_t *bandwidth, uint32_t size);
uint32_t bandwidth_stepup(bandwidth_t *bandwidth);
uint32_t bandwidth_stepdown(bandwidth_t *bandwidth);
void bandwidth_set_send_speed(bandwidth_t *bandwidth, uint32_t speed);
#endif

static uint32_t bandwidth_get_send_speed(bandwidth_t *bandwidth)
{
    return bandwidth->bandwidth;
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_BANDWIDTH_H */
