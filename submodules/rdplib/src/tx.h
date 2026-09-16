// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_TX_H
#define RDP_TX_H

#include <stdint.h>

#include "connection.h"
#include "iov.h"
#include "packet.h"

struct rdp_t;

enum rdp_transmit_address_family
{
    RDP_TRANSMIT_ADDRESS_IPV4 = 2,
    RDP_TRANSMIT_ADDRESS_IPX = 6,
    RDP_TRANSMIT_ADDRESS_SERIAL = 69
};

enum
{
    RDP_WIRE_HEADER_BASE_BYTES = 4,
    RDP_WIRE_HEADER_MAX_BYTES = RDP_WIRE_HEADER_BASE_BYTES + RDP_ACK_MAX_BYTES
};

#ifdef __cplusplus
extern "C"
{
#endif

void tx_init(connection_t *c, struct rdp_t *rdp, struct sockaddr *remote_addr);
uint32_t tx_create(connection_t *c);
void tx_destroy(connection_t *c);
void tx_flush_output_buffers(connection_t *c);
void tx_handle_ack(connection_t *c, uint16_t msgid);
uint32_t tx_validate_ack_arrival(connection_t *c, rdp_header_t *header, uint32_t *ack_size);
void tx_record_ack_arrival(connection_t *c, rdp_header_t *header);
uint32_t tx_send_ready(connection_t *c);
void tx_send_virgin(connection_t *c, msg_outgoing_t *msg_virgin);
uint16_t tx_reserve_msgid(connection_t *c);
uint32_t tx_outgoing_msg_in_outstanding_range(connection_t *c, msg_outgoing_t *msg_outgoing);
void tx_enqueue_outgoing(connection_t *c, msg_outgoing_t *msg_outgoing);
uint32_t tx_send_fin(connection_t *c);
uint32_t tx_send_alive(connection_t *c);
uint32_t trace_send(connection_t *c);
uint32_t connection_send(connection_t *to, const char *data, uint32_t size, uint32_t stream, uint32_t flags);
uint32_t connection_sendv(connection_t *c, iov_t *iov, uint32_t iov_len, uint32_t stream, uint32_t flags);
void tx_get_event_time(connection_t *c, timeout_data *timeout_data);
uint32_t tx_send_ready_virgins(connection_t *c);
void tx_abort_connection(connection_t *c, uint32_t disconnect_reason);
void tx_received_stopped(connection_t *c);
void tx_set_delayed_ack(connection_t *c);
void tx_tx(connection_t *c);
uint32_t tx_send_packet(connection_t *c, char *data, uint32_t size, uint16_t options_in_data);
uint32_t trace_start(connection_t *c);
uint32_t connection_set_max_data_rate(connection_t *c, uint32_t bytes_per_second);
uint32_t tx_get_stall_time(connection_t *c);

static uint32_t tx_needs_disconnect_msg(connection_t *c)
{
    return !c->tx_connected && !c->tx_enqueued_disconnect_msg;
}

static uint32_t tx_get_queue_size(connection_t *c)
{
    return txq_get_queue_size(&c->tx_outstanding_packets) + txq_get_queue_size(&c->tx_virgin_packets) + txq_get_queue_size(&c->tx_delayed_packets);
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_TX_H */
