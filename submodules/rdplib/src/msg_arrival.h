// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Received message storage. Fragment payload bytes follow the structure in the allocation.
#ifndef RDP_MSG_ARRIVAL_H
#define RDP_MSG_ARRIVAL_H

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "layout.h"
#include "list.h"

typedef struct connection_t connection_t;
typedef struct _rdp_header_t rdp_header_t;

typedef struct msg_arrival_t
{
    rdp_link_t rxq_link; // Its key is repurposed as the message moves between receive lists.
    connection_t *sender;
    uint16_t options;
    uint16_t seqnum;
    uint16_t msgid;
    uint16_t fragid;
    uint16_t frag_total;
    uint8_t stream;
    uint8_t stream_seqnum;
    uint16_t fragments_collected;
    uint32_t size;
    uint32_t enqueue_time;
    struct sockaddr from;
} msg_arrival_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(msg_arrival_t, rxq_link, 0x00);
RDP_ASSERT_OFFSET(msg_arrival_t, sender, 0x10);
RDP_ASSERT_OFFSET(msg_arrival_t, options, 0x14);
RDP_ASSERT_OFFSET(msg_arrival_t, seqnum, 0x16);
RDP_ASSERT_OFFSET(msg_arrival_t, msgid, 0x18);
RDP_ASSERT_OFFSET(msg_arrival_t, fragid, 0x1A);
RDP_ASSERT_OFFSET(msg_arrival_t, frag_total, 0x1C);
RDP_ASSERT_OFFSET(msg_arrival_t, stream, 0x1E);
RDP_ASSERT_OFFSET(msg_arrival_t, stream_seqnum, 0x1F);
RDP_ASSERT_OFFSET(msg_arrival_t, fragments_collected, 0x20);
RDP_ASSERT_OFFSET(msg_arrival_t, size, 0x24);
RDP_ASSERT_OFFSET(msg_arrival_t, enqueue_time, 0x28);
RDP_ASSERT_OFFSET(msg_arrival_t, from, 0x2C);
RDP_STATIC_ASSERT(sizeof(msg_arrival_t) == 0x3C, "msg_arrival_t must be 0x3c bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void msg_arrival_init(msg_arrival_t *arrival, uint16_t fragid);
void msg_arrival_prepare_for_sequencer(msg_arrival_t *arrival);
void msg_arrival_prepare_for_rxq(msg_arrival_t *arrival);
uint32_t msg_arrival_assemble(msg_arrival_t *arrival, connection_t *sender, rdp_header_t *header, char *data);
void msg_arrival_init_disconnect_msg(msg_arrival_t *arrival, connection_t *sender);
uint32_t msg_arrival_validate_fragment_arrival(msg_arrival_t *arrival, rdp_header_t *header);
uint32_t msg_arrival_get_size(const msg_arrival_t *arrival);
char *msg_arrival_get_data(const msg_arrival_t *arrival);
connection_t *msg_arrival_get_sender(const msg_arrival_t *arrival);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
struct sockaddr *msg_arrival_get_sender_addr(msg_arrival_t *arrival);
#endif

uint32_t msg_arrival_has_fin(const msg_arrival_t *arrival);

#ifdef __cplusplus
}
#endif

#endif /* RDP_MSG_ARRIVAL_H */
