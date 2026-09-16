// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "timeout.h"

#include <math.h>
#include <string.h>

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif

void timeout_init(timeout_t *timeout, uint32_t time_rt, uint16_t attempts)
{
#ifndef RDPLIB_SOURCE_FAITHFUL
    if (!timeout)
    {
        return;
    }
#endif

#ifndef RDPLIB_DEBUG
    (void)attempts;
#endif
    memset(timeout, 0, sizeof(*timeout));
    timeout->sample[0].time_rt = time_rt < UINT16_MAX ? (uint16_t)time_rt : UINT16_MAX;
    timeout->sample[0].weight = 1;
    timeout->sum_weight = timeout->sample[0].weight;
    timeout->sum_weighted_times = (uint32_t)timeout->sum_weight * timeout->sample[0].time_rt;
#ifdef RDPLIB_SOURCE_FAITHFUL
    // The recovered implementations multiply at 32 bits before assigning the 64 bit accumulator.
    timeout->sum_weighted_squares = (uint32_t)(timeout->sum_weighted_times * (uint32_t)timeout->sample[0].time_rt);
#else
    timeout->sum_weighted_squares = (uint64_t)timeout->sum_weighted_times * timeout->sample[0].time_rt;
#endif
    timeout->weighted_avg = timeout->sample[0].time_rt;
    timeout->std_deviation = 0;
#ifdef RDPLIB_DEBUG
    dpf(0x8000u, "rt tracker: time==%u attempts==%u [%u-%u]\n", time_rt, attempts, timeout->weighted_avg, timeout->std_deviation);
#endif
}

void timeout_add_sample(timeout_t *timeout, uint32_t time_rt, uint16_t attempts)
{
    uint32_t weighted_time_rt;

#ifndef RDPLIB_SOURCE_FAITHFUL
    if (!timeout || !attempts)
    {
        return;
    }
#endif

    weighted_time_rt = (uint32_t)timeout->sample[timeout->oldest_sample].weight * timeout->sample[timeout->oldest_sample].time_rt;
    timeout->sum_weighted_times -= weighted_time_rt;
#ifdef RDPLIB_SOURCE_FAITHFUL
    // The recovered implementations multiply at 32 bits before updating the 64 bit accumulator.
    timeout->sum_weighted_squares -= (uint32_t)((uint32_t)timeout->sample[timeout->oldest_sample].time_rt * weighted_time_rt);
#else
    timeout->sum_weighted_squares -= (uint64_t)timeout->sample[timeout->oldest_sample].time_rt * weighted_time_rt;
#endif
    timeout->sum_weight -= timeout->sample[timeout->oldest_sample].weight;

    timeout->sample[timeout->oldest_sample].time_rt = time_rt < UINT16_MAX ? (uint16_t)time_rt : UINT16_MAX;
    timeout->sample[timeout->oldest_sample].weight = 1023 / attempts >= 1 ? (uint16_t)(1023 / attempts) : 1;

    weighted_time_rt = (uint32_t)timeout->sample[timeout->oldest_sample].weight * timeout->sample[timeout->oldest_sample].time_rt;
    timeout->sum_weighted_times += weighted_time_rt;
#ifdef RDPLIB_SOURCE_FAITHFUL
    timeout->sum_weighted_squares += (uint32_t)((uint32_t)timeout->sample[timeout->oldest_sample].time_rt * weighted_time_rt);
#else
    timeout->sum_weighted_squares += (uint64_t)timeout->sample[timeout->oldest_sample].time_rt * weighted_time_rt;
#endif
    timeout->sum_weight += timeout->sample[timeout->oldest_sample].weight;
    timeout->weighted_avg = (uint16_t)(timeout->sum_weighted_times / timeout->sum_weight);

    if (timeout->sum_weight > 1)
    {
#ifdef RDPLIB_SOURCE_FAITHFUL
        // The recovered implementations also evaluate this product at 32 bits.
        timeout->std_deviation = (uint16_t)sqrt(
            (double)(int64_t)(timeout->sum_weighted_squares - (uint32_t)((uint32_t)timeout->weighted_avg * timeout->sum_weighted_times)) / (timeout->sum_weight - 1u));
#else
        timeout->std_deviation = (uint16_t)sqrt((double)(timeout->sum_weighted_squares - (uint64_t)timeout->weighted_avg * timeout->sum_weighted_times) / (timeout->sum_weight - 1u));
#endif
    }
    else
    {
        timeout->std_deviation = 0;
    }

#ifdef RDPLIB_DEBUG
    dpf(0x8000u, "rt tracker: time==%u attempts==%u [%u-%u]\n", time_rt, attempts, timeout->weighted_avg, timeout->std_deviation);
#endif
    ++timeout->oldest_sample;
    timeout->oldest_sample %= 64;
}
