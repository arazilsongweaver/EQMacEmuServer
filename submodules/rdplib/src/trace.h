// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_TRACE_H
#define RDP_TRACE_H

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "layout.h"

typedef struct _trace_probe_t
{
    uint32_t time_sent;
    uint32_t reply_time;
    struct in_addr icmp_from;
    uint8_t ttl;
    uint8_t icmp_type;
    uint8_t icmp_code;
} trace_probe_t, *Ptrace_probe_t;

RDP_ASSERT_OFFSET(trace_probe_t, time_sent, 0x00);
RDP_ASSERT_OFFSET(trace_probe_t, reply_time, 0x04);
RDP_ASSERT_OFFSET(trace_probe_t, icmp_from, 0x08);
RDP_ASSERT_OFFSET(trace_probe_t, ttl, 0x0C);
RDP_ASSERT_OFFSET(trace_probe_t, icmp_type, 0x0D);
RDP_ASSERT_OFFSET(trace_probe_t, icmp_code, 0x0E);
RDP_STATIC_ASSERT(sizeof(trace_probe_t) == 0x10, "trace_probe_t must be 0x10 bytes");

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t format_trace(char *buffer, trace_probe_t *probes, uint32_t max_probe, uint32_t name_lookup);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RDP_TRACE_H */
