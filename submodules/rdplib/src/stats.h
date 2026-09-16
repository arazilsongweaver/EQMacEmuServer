// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Raw connection snapshots filled by connection_get_perf_stats and connection_get_disconnect_info.
#ifndef RDP_STATS_H
#define RDP_STATS_H

#include <stdint.h>

#include "layout.h"

typedef struct perf_stats_t
{
    uint32_t time_last_arrival;
    uint64_t recent_seqnum_history;
    uint16_t highest_seqnum_received;
    uint32_t average_rt_time;
    uint32_t std_deviation;
    uint32_t last_rt_time;
    uint32_t queue_size;
    uint32_t stall_time;
} perf_stats_t;

typedef struct disconnect_info_t
{
    uint32_t disconnect_reason;
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint32_t icmp_from;
} disconnect_info_t;

RDP_ASSERT_OFFSET(disconnect_info_t, disconnect_reason, 0x00);
RDP_ASSERT_OFFSET(disconnect_info_t, icmp_type, 0x04);
RDP_ASSERT_OFFSET(disconnect_info_t, icmp_code, 0x05);
RDP_ASSERT_OFFSET(disconnect_info_t, icmp_from, 0x08);
RDP_STATIC_ASSERT(sizeof(disconnect_info_t) == 0x0C, "disconnect_info_t must be 0x0C bytes");

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(perf_stats_t, time_last_arrival, 0x00);
RDP_ASSERT_OFFSET(perf_stats_t, recent_seqnum_history, 0x08);
RDP_ASSERT_OFFSET(perf_stats_t, highest_seqnum_received, 0x10);
RDP_ASSERT_OFFSET(perf_stats_t, average_rt_time, 0x14);
RDP_ASSERT_OFFSET(perf_stats_t, std_deviation, 0x18);
RDP_ASSERT_OFFSET(perf_stats_t, last_rt_time, 0x1C);
RDP_ASSERT_OFFSET(perf_stats_t, queue_size, 0x20);
RDP_ASSERT_OFFSET(perf_stats_t, stall_time, 0x24);
RDP_STATIC_ASSERT(sizeof(perf_stats_t) == 0x28, "perf_stats_t must be 0x28 bytes on Win32");
#endif

#endif /* RDP_STATS_H */
