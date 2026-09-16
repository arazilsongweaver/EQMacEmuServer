// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "bandwidth.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>

#include "dpf.h"
#endif
#include "utime.h"

void bandwidth_init(bandwidth_t *bandwidth)
{
    bandwidth->queue_size = 0;
    bandwidth->queue_time = time_get_ms();
    bandwidth->bandwidth = 3000;
}

void bandwidth_enqueue_bytes(bandwidth_t *bandwidth, uint32_t size)
{
    bandwidth->queue_size = bandwidth_get_queue_size(bandwidth) + size;
    bandwidth->queue_time = time_get_ms();
#ifdef RDPLIB_DEBUG
    dpf(1u, "enqueue_bytes( %u ) [%u]\n", size, bandwidth->queue_size);
#endif
}

uint32_t bandwidth_get_queue_size(bandwidth_t *bandwidth)
{
    uint64_t burn;
    uint32_t time;

    if (bandwidth->queue_size)
    {
        time = time_get_ms();
        burn = ((uint64_t)bandwidth->bandwidth * (time - bandwidth->queue_time)) / 1000u;

        if (burn < bandwidth->queue_size)
        {
#if defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDPLIB_DEBUG)
            // this incorrectly recomputes the subtraction amount with a 32 bit multiply/divide
            bandwidth->queue_size -= bandwidth->bandwidth * (time - bandwidth->queue_time) / 1000u;
#else
            bandwidth->queue_size -= (uint32_t)burn;
#endif
        }
        else
        {
            bandwidth->queue_size = 0;
        }
        bandwidth->queue_time = time;
    }
    return bandwidth->queue_size;
}

uint32_t bandwidth_get_time_empty(bandwidth_t *bandwidth)
{
#ifdef RDPLIB_DEBUG
    assert(bandwidth->queue_size < 2000000);
#endif
    return bandwidth->queue_time + (1000u * bandwidth->queue_size) / bandwidth->bandwidth;
}

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
void bandwidth_set_queue_size(bandwidth_t *bandwidth, uint32_t size)
{
    bandwidth->queue_size = size;
    bandwidth->queue_time = time_get_ms();
}

uint32_t bandwidth_stepup(bandwidth_t *bandwidth)
{
    bandwidth->bandwidth = 100u * bandwidth->bandwidth / 99u <= 6000u
                               ? 100u * bandwidth->bandwidth / 99u
                               : 6000u;
    return bandwidth->bandwidth;
}

uint32_t bandwidth_stepdown(bandwidth_t *bandwidth)
{
    bandwidth->bandwidth = 49u * bandwidth->bandwidth / 50u >= 1000u
                               ? 49u * bandwidth->bandwidth / 50u
                               : 1000u;
    return bandwidth->bandwidth;
}

void bandwidth_set_send_speed(bandwidth_t *bandwidth, uint32_t speed)
{
    if (speed)
    {
        bandwidth->bandwidth = speed;
        bandwidth->autoadjust = 0;
    }
    else
    {
        bandwidth->autoadjust = 1;
    }
}
#endif
