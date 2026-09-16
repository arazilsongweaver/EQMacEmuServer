// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Decoded RDP header. This is a source level structure with a host pointer, not a packed wire record.
#ifndef RDP_PACKET_H
#define RDP_PACKET_H

#include <stdint.h>

#include "layout.h"

typedef struct _rdp_header_t
{
    uint16_t options;
    uint16_t seqnum;
    uint16_t *ack; // Points into the datagram being parsed.
    uint16_t msgid;
    uint16_t fragid;
    uint16_t frag_number;
    uint16_t frag_total;
    uint8_t stream;
    uint8_t stream_seqnum;
    uint16_t header_size;
    uint16_t data_size;
} rdp_header_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(rdp_header_t, options, 0x00);
RDP_ASSERT_OFFSET(rdp_header_t, seqnum, 0x02);
RDP_ASSERT_OFFSET(rdp_header_t, ack, 0x04);
RDP_ASSERT_OFFSET(rdp_header_t, msgid, 0x08);
RDP_ASSERT_OFFSET(rdp_header_t, fragid, 0x0A);
RDP_ASSERT_OFFSET(rdp_header_t, frag_number, 0x0C);
RDP_ASSERT_OFFSET(rdp_header_t, frag_total, 0x0E);
RDP_ASSERT_OFFSET(rdp_header_t, stream, 0x10);
RDP_ASSERT_OFFSET(rdp_header_t, stream_seqnum, 0x11);
RDP_ASSERT_OFFSET(rdp_header_t, header_size, 0x12);
RDP_ASSERT_OFFSET(rdp_header_t, data_size, 0x14);
RDP_STATIC_ASSERT(sizeof(rdp_header_t) == 0x18, "rdp_header_t must be 0x18 bytes on Win32");
#endif

#endif /* RDP_PACKET_H */
