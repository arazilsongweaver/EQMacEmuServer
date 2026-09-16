// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// timeout_t -- weighted 64 sample RTT estimator.
#ifndef RDP_TIMEOUT_H
#define RDP_TIMEOUT_H

#include <stdint.h>

#include "layout.h"

typedef struct _rt_sample_t
{
    uint16_t time_rt;
    uint16_t weight; // max(1023 / attempts, 1)
} rt_sample_t, *Prt_sample_t;

// The accumulator follows the native 64 bit alignment of the target compiler.
typedef struct _timeout_t
{
    rt_sample_t sample[64];
    uint32_t oldest_sample;
    uint64_t sum_weighted_squares;
    uint32_t sum_weighted_times;
    uint16_t sum_weight;
    uint16_t weighted_avg;
    uint16_t std_deviation;
} timeout_t, *Ptimeout_t;

RDP_ASSERT_OFFSET(rt_sample_t, time_rt, 0x00);
RDP_ASSERT_OFFSET(rt_sample_t, weight, 0x02);
RDP_STATIC_ASSERT(sizeof(rt_sample_t) == 0x04, "rt_sample_t must be 0x04 bytes");

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(timeout_t, sample, 0x000);
RDP_ASSERT_OFFSET(timeout_t, oldest_sample, 0x100);
RDP_ASSERT_OFFSET(timeout_t, sum_weighted_squares, 0x108);
RDP_ASSERT_OFFSET(timeout_t, sum_weighted_times, 0x110);
RDP_ASSERT_OFFSET(timeout_t, sum_weight, 0x114);
RDP_ASSERT_OFFSET(timeout_t, weighted_avg, 0x116);
RDP_ASSERT_OFFSET(timeout_t, std_deviation, 0x118);
RDP_STATIC_ASSERT(sizeof(timeout_t) == 0x120, "timeout_t must be 0x120 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

// attempts is used only by the recovered debug log in this initializer.
void timeout_init(timeout_t *timeout, uint32_t time_rt, uint16_t attempts);

// attempts must be nonzero; the live ACK path submits only 1.
void timeout_add_sample(timeout_t *timeout, uint32_t time_rt, uint16_t attempts);

static uint16_t timeout_get_timeout(timeout_t *timeout)
{
    if (timeout->weighted_avg + 2u * (uint32_t)timeout->std_deviation >= UINT16_MAX)
    {
        return UINT16_MAX;
    }
    if (timeout->weighted_avg + 2u * (uint32_t)timeout->std_deviation <= 50u)
    {
        return 50u;
    }
    return (uint16_t)(timeout->weighted_avg + 2u * timeout->std_deviation);
}

static uint16_t timeout_get_ancient(timeout_t *timeout)
{
    if (timeout->weighted_avg + 3u * (uint32_t)timeout->std_deviation >= UINT16_MAX)
    {
        return UINT16_MAX;
    }
    if (timeout->weighted_avg + 3u * (uint32_t)timeout->std_deviation <= 50u)
    {
        return 50u;
    }
    return (uint16_t)(timeout->weighted_avg + 3u * timeout->std_deviation);
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_TIMEOUT_H */
