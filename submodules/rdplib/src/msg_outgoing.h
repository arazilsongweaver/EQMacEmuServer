// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Outgoing message storage. Serialized header and payload bytes follow the structure in the allocation.
#ifndef RDP_MSG_OUTGOING_H
#define RDP_MSG_OUTGOING_H

#include <stdint.h>
#include <string.h>

#include "layout.h"
#include "list.h"

typedef struct msg_outgoing_t
{
    rdp_link_t txq_link;
    uint32_t time_first_sent;
    uint32_t time_last_sent;
    uint16_t options;
    uint16_t msgid;
    uint16_t fragid;
    uint16_t frag_number;
    uint16_t frag_total;
    uint8_t stream;
    uint8_t stream_seqnum;
    uint16_t attempts;
    uint32_t size;
    uint32_t enqueue_time;
} msg_outgoing_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(msg_outgoing_t, txq_link, 0x00);
RDP_ASSERT_OFFSET(msg_outgoing_t, time_first_sent, 0x10);
RDP_ASSERT_OFFSET(msg_outgoing_t, time_last_sent, 0x14);
RDP_ASSERT_OFFSET(msg_outgoing_t, options, 0x18);
RDP_ASSERT_OFFSET(msg_outgoing_t, msgid, 0x1A);
RDP_ASSERT_OFFSET(msg_outgoing_t, fragid, 0x1C);
RDP_ASSERT_OFFSET(msg_outgoing_t, frag_number, 0x1E);
RDP_ASSERT_OFFSET(msg_outgoing_t, frag_total, 0x20);
RDP_ASSERT_OFFSET(msg_outgoing_t, stream, 0x22);
RDP_ASSERT_OFFSET(msg_outgoing_t, stream_seqnum, 0x23);
RDP_ASSERT_OFFSET(msg_outgoing_t, attempts, 0x24);
RDP_ASSERT_OFFSET(msg_outgoing_t, size, 0x28);
RDP_ASSERT_OFFSET(msg_outgoing_t, enqueue_time, 0x2C);
RDP_STATIC_ASSERT(sizeof(msg_outgoing_t) == 0x30, "msg_outgoing_t must be 0x30 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void msg_outgoing_init(msg_outgoing_t *outgoing);

static void msg_outgoing_set_syn_bit(msg_outgoing_t *outgoing)
{
    outgoing->options |= 0x2000;
}

static void msg_outgoing_set_time_last_sent(msg_outgoing_t *outgoing, uint32_t time_last_sent)
{
    outgoing->time_last_sent = time_last_sent;
    ++outgoing->attempts;
}

static void msg_outgoing_set_time_first_sent(msg_outgoing_t *outgoing, uint32_t time_first_sent)
{
    outgoing->time_last_sent = time_first_sent;
    outgoing->time_first_sent = time_first_sent;
    ++outgoing->attempts;
}

static char *msg_outgoing_get_data(msg_outgoing_t *outgoing)
{
    return (char *)(outgoing + 1);
}

static void msg_outgoing_append(msg_outgoing_t *outgoing, const void *data, uint32_t size)
{
    memcpy(msg_outgoing_get_data(outgoing) + outgoing->size, data, size);
    outgoing->size += size;
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_MSG_OUTGOING_H */
