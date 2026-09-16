// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDPLIB_CONNECTION_H
#define RDPLIB_CONNECTION_H

#include <stdint.h>

#include "arrival_fragid.h"
#include "bandwidth.h"
#include "bitops.h"
#include "event.h"
#include "layout.h"
#include "list.h"
#include "packet.h"
#include "pqueue.h"
#include "rdplib_constants.h"
#include "rdplib_platform.h"
#include "sequencer.h"
#include "stats.h"
#include "timeout.h"
#include "trace.h"
#include "txq.h"
#include "uevent.h"
#include "umutex.h"

typedef struct rdp_t rdp_t;

typedef struct connection_stat
{
    uint32_t best_effort_packets_tx;
    uint32_t best_effort_bytes_tx;
    uint32_t guaranteed_packets_tx;
    uint32_t guaranteed_bytes_tx;
    uint32_t guaranteed_packets_retx;
    uint32_t guaranteed_bytes_retx;
    uint32_t ack_only_packets_tx;
    uint32_t ack_and_data_packets_tx;
    uint32_t best_effort_packets_rx;
    uint32_t best_effort_bytes_rx;
    uint32_t guaranteed_packets_rx;
    uint32_t guaranteed_bytes_rx;
    uint32_t duplicate_packets_rx;
    uint32_t duplicate_bytes_rx;
    uint32_t header_bytes_rx;
    uint32_t ack_only_packets_rx;
    uint32_t ack_and_data_packets_rx;
    uint32_t messages_acked;
    uint32_t duplicate_acks;
    uint32_t bytes_in_duplicate_acks;
    uint32_t acks_for_unsent_messages;
    uint32_t packets_rx_in_sequence;
    uint32_t bytes_rx_in_sequence;
    uint32_t packets_rx_out_of_sequence;
    uint32_t bytes_rx_out_of_sequence;
    uint32_t discarded_bad_options;
    uint32_t discarded_old_seqnum;
    uint32_t discarded_dup_seqnum;
    uint32_t discarded_old_msgid;
    uint32_t discarded_bad_fragment;
    uint32_t discarded_bad_stream;
    uint32_t discarded_too_short;
    uint32_t discarded_bad_fragment_size;
    uint32_t discarded_bad_ack_header;
    uint32_t discarded_bad_ackmask;
    uint32_t discarded_mask_wo_ack;
    uint32_t discarded_old_ack;
    uint32_t packets_updated_rtt;
    uint32_t packets_updated_rtt_attempts;
    uint32_t icmp_unreachable[16];
    uint32_t icmp_source_quench;
    uint32_t icmp_ttl_expired[2];
    uint32_t icmp_parameter_problem[2];
    uint32_t icmp_unknown;
    uint32_t tqd_last_interval;
    uint32_t tqd_samples;
    uint32_t tqd_min;
    uint32_t tqd_max;
    uint32_t tqd_sum;
    uint32_t tqd_bytes;
} connection_stat;

#if defined(_MSC_VER)
#define RDPLIB_CONNECTION_ALIGN8 __declspec(align(8))
#else
#define RDPLIB_CONNECTION_ALIGN8 _Alignas(8)
#endif

typedef struct connection_t
{
    void *cn_app_ptr[3];
    rdp_t *cn_rdp;
    rdp_link_t cn_addr_map_link;
    uint32_t cn_ref_count;
    qlink cn_event_queue_link;
    timeout_data cn_event_time;
    int32_t cn_event_type;
    umutex_t cn_lock;
    uint32_t cn_closed;
    uint32_t cn_delete_time;
    uint32_t cn_accepted;
    uint32_t cn_abort;
    uint32_t cn_flags;
    connection_stat stat;
    uint8_t rx_icmp_type;
    uint8_t rx_icmp_code;
    uint32_t rx_icmp_time;
    uint32_t rx_icmp_received;
    struct in_addr rx_icmp_from;
    uint16_t rx_received_all_thru;
    uint16_t rx_highest_received;
    bitarray_t rx_others_received;
    uint16_t rx_msgid_count;
    uint16_t rx_msgid_lo;
    uint16_t rx_msgid_hi;
    uint32_t rx_time_last_arrival;
    uint32_t rx_time_last_msgid_arrival;
    arrival_fragid_t rx_reassembly_pool;
    uint16_t rx_best_effort_stream_seqnum_reset;
    uint16_t rx_best_effort_stream_seqnum[20];
    uint8_t rx_guaranteed_stream_seqnum[20];
    sequencer_t rx_sequencer[20];
    uint16_t rx_syn_recvd;
    uint16_t rx_syn_msgid;
    uint16_t rx_fin_recvd;
    uint16_t rx_fin_msgid;
    msg_arrival_t *rx_fin_storage;
    uint16_t rx_highest_seqnum_received;
    uint64_t rx_recent_seqnum_history;
    trace_probe_t *rx_connect_trace;
    uint32_t rx_connect_count;
    int32_t rx_connect_clock;
    intptr_t tx_socket;
    struct sockaddr tx_remote_addr;
    uint16_t tx_acked_thru;
    uint16_t tx_next_seqnum;
    uint16_t tx_next_msgid;
    uint16_t tx_next_fragid;
    uint32_t tx_time_last_guaranteed_send;
    bitarray_t tx_outstanding_packet_mask;
    txq_t tx_outstanding_packets;
    txq_t tx_virgin_packets;
    txq_t tx_delayed_packets;
    bandwidth_t tx_bandwidth;
    uint32_t tx_send_buffer_size;
    uint32_t tx_time_since_bandwidth_change;
    uint32_t tx_modem;
    timeout_t tx_rt_tracker;
    uint8_t tx_guaranteed_stream_seqnum[20];
    uint32_t tx_syn_sent;
    uint32_t tx_syn_acked;
    uint16_t tx_syn_msgid;
    uint32_t tx_fin_sent;
    uint32_t tx_fin_acked;
    uint16_t tx_fin_msgid;
    uint32_t tx_connected;
    uint32_t tx_stopped;
    uint32_t tx_disconnect_reason;
    uint32_t tx_enqueued_disconnect_msg;
    uint32_t tx_max_message_age;
    uint32_t tx_max_service_outage;
    uint32_t tx_delayed_ack;
    uint32_t tx_ack_time;
    uint16_t tx_last_rt_time;
    uevent_t *tx_all_acked_event;
    uint32_t *tx_all_acked;
    struct sockaddr_in trace_remote_addr;
    intptr_t trace_socket;
    uint32_t trace_udp_ttl;
    trace_probe_t *trace_probes;
    uint32_t trace_en_route;
    uint32_t trace_time;
    uint32_t trace_start;
    int32_t trace_clock;
    uint32_t trace_next_ttl;
    uint32_t trace_max_ttl;
    uint32_t trace_pass;
    uint32_t trace_next_index;

#ifndef RDPLIB_SOURCE_FAITHFUL
    // Configurable keepalive interval is an rdplib addition.
    // Raw header consumers must use the same RDPLIB_SOURCE_FAITHFUL setting as the library or the connection_t ABI breaks.
    RDPLIB_CONNECTION_ALIGN8 uint32_t rdplib_keepalive_interval_ms;
    int (*rdplib_packet_drop_callback)(void *context, rdplib_packet_drop_direction_t direction, const uint8_t *packet, uint32_t packet_bytes);
    void *rdplib_packet_drop_context;
#endif
} connection_t;

#undef RDPLIB_CONNECTION_ALIGN8

typedef enum rdp_rx_arrival_disposition_t
{
    RDP_RX_ACCEPT = 0,
    RDP_RX_DISCARD = 1,
    RDP_RX_ABORT = 2
} rdp_rx_arrival_disposition_t;

enum
{
    CONNECTION_EVENT_NONE = 0,
    CONNECTION_EVENT_DELETE = 1,
    CONNECTION_EVENT_TX = 2,
    CONNECTION_EVENT_ALIVE = 3,
    CONNECTION_EVENT_TRACE = 4
};

enum
{
    RDP_ACK_MAX_BYTES = 17
};

RDP_ASSERT_OFFSET(connection_stat, best_effort_packets_tx, 0x000);
RDP_ASSERT_OFFSET(connection_stat, best_effort_bytes_tx, 0x004);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_packets_tx, 0x008);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_bytes_tx, 0x00C);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_packets_retx, 0x010);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_bytes_retx, 0x014);
RDP_ASSERT_OFFSET(connection_stat, ack_only_packets_tx, 0x018);
RDP_ASSERT_OFFSET(connection_stat, ack_and_data_packets_tx, 0x01C);
RDP_ASSERT_OFFSET(connection_stat, best_effort_packets_rx, 0x020);
RDP_ASSERT_OFFSET(connection_stat, best_effort_bytes_rx, 0x024);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_packets_rx, 0x028);
RDP_ASSERT_OFFSET(connection_stat, guaranteed_bytes_rx, 0x02C);
RDP_ASSERT_OFFSET(connection_stat, duplicate_packets_rx, 0x030);
RDP_ASSERT_OFFSET(connection_stat, duplicate_bytes_rx, 0x034);
RDP_ASSERT_OFFSET(connection_stat, header_bytes_rx, 0x038);
RDP_ASSERT_OFFSET(connection_stat, ack_only_packets_rx, 0x03C);
RDP_ASSERT_OFFSET(connection_stat, ack_and_data_packets_rx, 0x040);
RDP_ASSERT_OFFSET(connection_stat, messages_acked, 0x044);
RDP_ASSERT_OFFSET(connection_stat, duplicate_acks, 0x048);
RDP_ASSERT_OFFSET(connection_stat, bytes_in_duplicate_acks, 0x04C);
RDP_ASSERT_OFFSET(connection_stat, acks_for_unsent_messages, 0x050);
RDP_ASSERT_OFFSET(connection_stat, packets_rx_in_sequence, 0x054);
RDP_ASSERT_OFFSET(connection_stat, bytes_rx_in_sequence, 0x058);
RDP_ASSERT_OFFSET(connection_stat, packets_rx_out_of_sequence, 0x05C);
RDP_ASSERT_OFFSET(connection_stat, bytes_rx_out_of_sequence, 0x060);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_options, 0x064);
RDP_ASSERT_OFFSET(connection_stat, discarded_old_seqnum, 0x068);
RDP_ASSERT_OFFSET(connection_stat, discarded_dup_seqnum, 0x06C);
RDP_ASSERT_OFFSET(connection_stat, discarded_old_msgid, 0x070);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_fragment, 0x074);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_stream, 0x078);
RDP_ASSERT_OFFSET(connection_stat, discarded_too_short, 0x07C);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_fragment_size, 0x080);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_ack_header, 0x084);
RDP_ASSERT_OFFSET(connection_stat, discarded_bad_ackmask, 0x088);
RDP_ASSERT_OFFSET(connection_stat, discarded_mask_wo_ack, 0x08C);
RDP_ASSERT_OFFSET(connection_stat, discarded_old_ack, 0x090);
RDP_ASSERT_OFFSET(connection_stat, packets_updated_rtt, 0x094);
RDP_ASSERT_OFFSET(connection_stat, packets_updated_rtt_attempts, 0x098);
RDP_ASSERT_OFFSET(connection_stat, icmp_unreachable, 0x09C);
RDP_ASSERT_OFFSET(connection_stat, icmp_source_quench, 0x0DC);
RDP_ASSERT_OFFSET(connection_stat, icmp_ttl_expired, 0x0E0);
RDP_ASSERT_OFFSET(connection_stat, icmp_parameter_problem, 0x0E8);
RDP_ASSERT_OFFSET(connection_stat, icmp_unknown, 0x0F0);
RDP_ASSERT_OFFSET(connection_stat, tqd_last_interval, 0x0F4);
RDP_ASSERT_OFFSET(connection_stat, tqd_samples, 0x0F8);
RDP_ASSERT_OFFSET(connection_stat, tqd_min, 0x0FC);
RDP_ASSERT_OFFSET(connection_stat, tqd_max, 0x100);
RDP_ASSERT_OFFSET(connection_stat, tqd_sum, 0x104);
RDP_ASSERT_OFFSET(connection_stat, tqd_bytes, 0x108);
RDP_STATIC_ASSERT(sizeof(connection_stat) == 0x10C, "connection_stat must be 0x10C bytes");

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(connection_t, cn_app_ptr, 0x0000);
RDP_ASSERT_OFFSET(connection_t, cn_rdp, 0x000C);
RDP_ASSERT_OFFSET(connection_t, cn_addr_map_link, 0x0010);
RDP_ASSERT_OFFSET(connection_t, cn_ref_count, 0x0020);
RDP_ASSERT_OFFSET(connection_t, cn_event_queue_link, 0x0024);
RDP_ASSERT_OFFSET(connection_t, cn_event_time, 0x0030);
RDP_ASSERT_OFFSET(connection_t, cn_event_type, 0x0038);
RDP_ASSERT_OFFSET(connection_t, cn_lock, 0x003C);
RDP_ASSERT_OFFSET(connection_t, cn_closed, 0x0054 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, cn_delete_time, 0x0058 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, cn_accepted, 0x005C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, cn_abort, 0x0060 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, cn_flags, 0x0064 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, stat, 0x0068 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_icmp_type, 0x0174 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_icmp_code, 0x0175 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_icmp_time, 0x0178 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_icmp_received, 0x017C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_icmp_from, 0x0180 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_received_all_thru, 0x0184 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_highest_received, 0x0186 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_others_received, 0x0188 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_msgid_count, 0x038C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_msgid_lo, 0x038E + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_msgid_hi, 0x0390 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_time_last_arrival, 0x0394 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_time_last_msgid_arrival, 0x0398 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_reassembly_pool, 0x039C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_best_effort_stream_seqnum_reset, 0x03B0 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_best_effort_stream_seqnum, 0x03B2 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_guaranteed_stream_seqnum, 0x03DA + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_sequencer, 0x03F0 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_syn_recvd, 0x0580 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_syn_msgid, 0x0582 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_fin_recvd, 0x0584 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_fin_msgid, 0x0586 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_fin_storage, 0x0588 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_highest_seqnum_received, 0x058C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_recent_seqnum_history, 0x0590 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_connect_trace, 0x0598 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_connect_count, 0x059C + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, rx_connect_clock, 0x05A0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_socket, 0x05A4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_remote_addr, 0x05A8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_acked_thru, 0x05B8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_next_seqnum, 0x05BA + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_next_msgid, 0x05BC + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_next_fragid, 0x05BE + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_time_last_guaranteed_send, 0x05C0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_outstanding_packet_mask, 0x05C4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_outstanding_packets, 0x07C8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_virgin_packets, 0x07E0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_delayed_packets, 0x07F8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_bandwidth, 0x0810 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_send_buffer_size, 0x0820 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_time_since_bandwidth_change, 0x0824 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_modem, 0x0828 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_rt_tracker, 0x0830 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_guaranteed_stream_seqnum, 0x0950 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_syn_sent, 0x0964 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_syn_acked, 0x0968 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_syn_msgid, 0x096C + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_fin_sent, 0x0970 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_fin_acked, 0x0974 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_fin_msgid, 0x0978 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_connected, 0x097C + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_stopped, 0x0980 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_disconnect_reason, 0x0984 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_enqueued_disconnect_msg, 0x0988 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_max_message_age, 0x098C + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_max_service_outage, 0x0990 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_delayed_ack, 0x0994 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_ack_time, 0x0998 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_last_rt_time, 0x099C + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_all_acked_event, 0x09A0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, tx_all_acked, 0x09A4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_remote_addr, 0x09A8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_socket, 0x09B8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_udp_ttl, 0x09BC + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_probes, 0x09C0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_en_route, 0x09C4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_time, 0x09C8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_start, 0x09CC + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_clock, 0x09D0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_next_ttl, 0x09D4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_max_ttl, 0x09D8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_pass, 0x09DC + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(connection_t, trace_next_index, 0x09E0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
#ifdef RDPLIB_SOURCE_FAITHFUL
RDP_STATIC_ASSERT(sizeof(connection_t) == 0x09E8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES, "connection_t has the expected faithful Win32 layout");
#else
RDP_ASSERT_OFFSET(connection_t, rdplib_keepalive_interval_ms, 0x09E8 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void connection_init(connection_t *c, rdp_t *rdp, struct sockaddr *remote_addr, uint32_t flags);
uint32_t connection_create(connection_t *c);
void connection_destroy(connection_t *c);
void connection_recalc_event_timeout(connection_t *c, timeout_data *uevent_time);
uint32_t connection_parse_and_validate_arrival(connection_t *c, uint16_t *packet, uint16_t size, rdp_header_t *header);
void connection_record_arrival(connection_t *c, rdp_header_t *header, uint32_t *duplicate);
void connection_handle_icmp(connection_t *c, uint8_t type, uint8_t code, uint8_t trace_reply, uint8_t probe_index, struct sockaddr_in *from);
void connection_event_process(connection_t *c, uint32_t max_time, timeout_data *next_time);
uint32_t connection_close(connection_t *c, uint32_t linger_time, uint32_t *all_acked, uevent_t *all_acked_event);
uint32_t connection_linger_expired(connection_t *c);

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
// AK server was observed sending keepalives but it's not on by default.  It probably used this API to enable it after accepting.
uint32_t connection_keepalive(connection_t *c, uint32_t on);
#endif

void connection_set_send_buffer_size(connection_t *c, uint32_t send_buffer_size);

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
uint32_t connection_set_timeouts(connection_t *c, uint32_t max_message_age, uint32_t max_service_outage);
#endif

uint32_t connection_connected(connection_t *c);
void **connection_app_ptr(connection_t *c);
struct sockaddr *connection_get_remote_addr(connection_t *c);

// unused, retained for historical interest
#ifdef RDP_DEAD_CODE
uint32_t connection_get_last_rt_time(connection_t *c);
#endif

void connection_get_perf_stats(connection_t *c, perf_stats_t *stats);
void connection_get_disconnect_info(connection_t *c, disconnect_info_t *info, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* RDPLIB_CONNECTION_H */
