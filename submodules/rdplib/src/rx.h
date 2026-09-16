// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_RX_H
#define RDP_RX_H

#include <stdint.h>

#include "connection.h"
#include "msg_arrival.h"
#include "packet.h"

#ifdef __cplusplus
extern "C"
{
#endif

void rx_init(connection_t *c);
uint32_t rx_create(connection_t *c);
void rx_destroy(connection_t *c);
void rx_flush_input_buffers(connection_t *c);
void rx_record_packet_arrival(connection_t *c);
void rx_record_seqnum_arrival(connection_t *c, rdp_header_t *header);
uint32_t rx_validate_seqnum_arrival(connection_t *c, uint16_t seqnum);
uint32_t rx_validate_msgid_arrival(connection_t *c, rdp_header_t *header);
uint32_t rx_validate_fragment_arrival(connection_t *c, rdp_header_t *header);
uint32_t rx_validate_stream_arrival(connection_t *c, rdp_header_t *header);
uint32_t rx_record_msgid_arrival(connection_t *c, uint16_t msgid);
uint32_t rx_append_ack(connection_t *c, uint16_t *dst, uint16_t *options);
void rx_sort_into_sequence(connection_t *c, msg_arrival_t *arrival);
msg_arrival_t *rx_get_next_in_sequence(connection_t *c, uint8_t stream);
uint32_t rx_in_sequence(connection_t *c, msg_arrival_t *arrival);
msg_arrival_t *rx_assemble(connection_t *c, rdp_header_t *header, char *data);
msg_arrival_t *rx_load_fin_arrival(connection_t *c);
void rx_save_fin_arrival(connection_t *c, msg_arrival_t *arrival);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t c_stat_format(connection_stat *c_stat, char *buffer, uint32_t verbose);
#endif

static uint32_t rx_rcvd_all_msgids(connection_t *c)
{
    return c->rx_fin_recvd && c->rx_received_all_thru == c->rx_fin_msgid;
}

static uint32_t rx_fin_waiting(connection_t *c)
{
    return c->rx_fin_storage != NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_RX_H */
