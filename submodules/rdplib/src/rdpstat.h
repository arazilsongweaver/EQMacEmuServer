// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_RDPSTAT_H
#define RDP_RDPSTAT_H

#include <stdint.h>

#include "layout.h"

typedef struct rdp_stat
{
    uint64_t best_effort_packets_tx;
    uint64_t best_effort_bytes_tx;
    uint64_t guaranteed_packets_tx;
    uint64_t guaranteed_bytes_tx;
    uint64_t guaranteed_packets_retx;
    uint64_t guaranteed_bytes_retx;
    uint64_t ack_only_packets_tx;
    uint64_t ack_and_data_packets_tx;
    uint64_t best_effort_packets_rx;
    uint64_t best_effort_bytes_rx;
    uint64_t guaranteed_packets_rx;
    uint64_t guaranteed_bytes_rx;
    uint64_t duplicate_packets_rx;
    uint64_t duplicate_bytes_rx;
    uint64_t header_bytes_rx;
    uint64_t ack_only_packets_rx;
    uint64_t ack_and_data_packets_rx;
    uint64_t messages_acked;          // ACK message IDs processed, including IDs that no longer identify a queued message.
    uint64_t duplicate_acks;          // ACK bearing packets that retire no new message IDs.
    uint64_t bytes_in_duplicate_acks; // Nominal ACK header bytes in those packets.
    uint64_t acks_for_unsent_messages;
    uint64_t packets_rx_in_sequence;
    uint64_t bytes_rx_in_sequence;
    uint64_t packets_rx_out_of_sequence;
    uint64_t bytes_rx_out_of_sequence;
    uint64_t packets_received_after_close;
    uint64_t packets_received_after_disconnect;
    uint64_t discarded_bad_options;
    uint64_t discarded_old_seqnum;
    uint64_t discarded_dup_seqnum;
    uint64_t discarded_old_msgid;
    uint64_t discarded_bad_fragment;
    uint64_t discarded_bad_stream;
    uint64_t discarded_too_short;
    uint64_t discarded_bad_fragment_size;
    uint64_t discarded_bad_ack_header;
    uint64_t discarded_bad_ackmask;
    uint64_t discarded_old_ack;
    uint64_t discarded_mask_wo_ack;
    uint64_t discarded_invalid_localhost;
    uint64_t discarded_bad_size;
    uint64_t discarded_bad_crc;
    uint64_t connection_requests;
    uint64_t connection_accepts;
    uint64_t bad_connection_attempts;
    uint64_t packets_without_connection;
    uint64_t connections_established; // First accepted ACK after this direction has sent SYN.
    uint64_t connections_closed;
    uint64_t connections_dropped;
    uint64_t embryonic_connections_dropped;
    uint64_t packets_updated_rtt;          // Acknowledged messages sent exactly once and accepted as RTT samples.
    uint64_t packets_updated_rtt_attempts; // Acknowledged messages still present in the sent message queue.
    uint64_t retx_timeouts;                // Nonpositive socket/event wait results in the I/O loop.
    uint64_t connections_dropped_hacker;
    uint64_t connections_dropped_rst;
    uint64_t connections_dropped_no_response;
    uint64_t connections_dropped_msg_age;
    uint64_t connections_dropped_unreachable[16];
    uint64_t connections_dropped_source_quench;
    uint64_t connections_dropped_ttl_expired[2];
    uint64_t connections_dropped_parameter_problem[2];
    uint64_t connections_dropped_icmp_unknown;
    uint64_t icmp_received_with_connection;
    uint64_t icmp_received_without_connection;
    uint64_t icmp_unreachable[16];
    uint64_t icmp_source_quench;
    uint64_t icmp_ttl_expired[2];
    uint64_t icmp_parameter_problem[2];
    uint64_t icmp_unknown;
    uint64_t sendto_calls; // Backend sends, including the maintained serial backend.
    uint64_t sendto_failures;
    uint64_t sendto_consecutive_failures;
    uint64_t last_sendto_failed;
    uint64_t sendto_errno[152];
    uint64_t sendto_errno_big;
    uint64_t sendto_errno_big_last;
    uint64_t rqd_last_interval;
    uint64_t rqd_samples;
    uint64_t rqd_min;
    uint64_t rqd_max;
    uint64_t rqd_sum;
    uint64_t rqd_bytes;
    uint64_t shipping_tx;           // Transmit scheduler passes.
    uint64_t shipping_tx_useless;   // Passes that found no work.
    uint64_t shipping_tx_not_ready; // Passes blocked by backend readiness.
} rdp_stat;

// The linked TAKP clients publish application owned storage through this process global pointer. Counter updates are not synchronized between connections.
void rdplib_discard_global_statistics(int discard);
rdp_stat *rdplib_discarded_statistics(void);

#ifdef RDPLIB_DISCARD_GLOBAL_STATISTICS
#define g_rdp_stat (rdplib_discarded_statistics())
#else
extern rdp_stat *g_rdp_stat;
#endif

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
extern rdp_stat rdp_stat_struct;
uint32_t rdp_stat_format(rdp_stat *rdp_stat, char *buffer, uint32_t verbose);
#endif

RDP_ASSERT_OFFSET(rdp_stat, best_effort_packets_tx, 0x0000);
RDP_ASSERT_OFFSET(rdp_stat, best_effort_bytes_tx, 0x0008);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_packets_tx, 0x0010);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_bytes_tx, 0x0018);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_packets_retx, 0x0020);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_bytes_retx, 0x0028);
RDP_ASSERT_OFFSET(rdp_stat, ack_only_packets_tx, 0x0030);
RDP_ASSERT_OFFSET(rdp_stat, ack_and_data_packets_tx, 0x0038);
RDP_ASSERT_OFFSET(rdp_stat, best_effort_packets_rx, 0x0040);
RDP_ASSERT_OFFSET(rdp_stat, best_effort_bytes_rx, 0x0048);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_packets_rx, 0x0050);
RDP_ASSERT_OFFSET(rdp_stat, guaranteed_bytes_rx, 0x0058);
RDP_ASSERT_OFFSET(rdp_stat, duplicate_packets_rx, 0x0060);
RDP_ASSERT_OFFSET(rdp_stat, duplicate_bytes_rx, 0x0068);
RDP_ASSERT_OFFSET(rdp_stat, header_bytes_rx, 0x0070);
RDP_ASSERT_OFFSET(rdp_stat, ack_only_packets_rx, 0x0078);
RDP_ASSERT_OFFSET(rdp_stat, ack_and_data_packets_rx, 0x0080);
RDP_ASSERT_OFFSET(rdp_stat, messages_acked, 0x0088);
RDP_ASSERT_OFFSET(rdp_stat, duplicate_acks, 0x0090);
RDP_ASSERT_OFFSET(rdp_stat, bytes_in_duplicate_acks, 0x0098);
RDP_ASSERT_OFFSET(rdp_stat, acks_for_unsent_messages, 0x00A0);
RDP_ASSERT_OFFSET(rdp_stat, packets_rx_in_sequence, 0x00A8);
RDP_ASSERT_OFFSET(rdp_stat, bytes_rx_in_sequence, 0x00B0);
RDP_ASSERT_OFFSET(rdp_stat, packets_rx_out_of_sequence, 0x00B8);
RDP_ASSERT_OFFSET(rdp_stat, bytes_rx_out_of_sequence, 0x00C0);
RDP_ASSERT_OFFSET(rdp_stat, packets_received_after_close, 0x00C8);
RDP_ASSERT_OFFSET(rdp_stat, packets_received_after_disconnect, 0x00D0);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_options, 0x00D8);
RDP_ASSERT_OFFSET(rdp_stat, discarded_old_seqnum, 0x00E0);
RDP_ASSERT_OFFSET(rdp_stat, discarded_dup_seqnum, 0x00E8);
RDP_ASSERT_OFFSET(rdp_stat, discarded_old_msgid, 0x00F0);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_fragment, 0x00F8);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_stream, 0x0100);
RDP_ASSERT_OFFSET(rdp_stat, discarded_too_short, 0x0108);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_fragment_size, 0x0110);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_ack_header, 0x0118);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_ackmask, 0x0120);
RDP_ASSERT_OFFSET(rdp_stat, discarded_old_ack, 0x0128);
RDP_ASSERT_OFFSET(rdp_stat, discarded_mask_wo_ack, 0x0130);
RDP_ASSERT_OFFSET(rdp_stat, discarded_invalid_localhost, 0x0138);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_size, 0x0140);
RDP_ASSERT_OFFSET(rdp_stat, discarded_bad_crc, 0x0148);
RDP_ASSERT_OFFSET(rdp_stat, connection_requests, 0x0150);
RDP_ASSERT_OFFSET(rdp_stat, connection_accepts, 0x0158);
RDP_ASSERT_OFFSET(rdp_stat, bad_connection_attempts, 0x0160);
RDP_ASSERT_OFFSET(rdp_stat, packets_without_connection, 0x0168);
RDP_ASSERT_OFFSET(rdp_stat, connections_established, 0x0170);
RDP_ASSERT_OFFSET(rdp_stat, connections_closed, 0x0178);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped, 0x0180);
RDP_ASSERT_OFFSET(rdp_stat, embryonic_connections_dropped, 0x0188);
RDP_ASSERT_OFFSET(rdp_stat, packets_updated_rtt, 0x0190);
RDP_ASSERT_OFFSET(rdp_stat, packets_updated_rtt_attempts, 0x0198);
RDP_ASSERT_OFFSET(rdp_stat, retx_timeouts, 0x01A0);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_hacker, 0x01A8);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_rst, 0x01B0);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_no_response, 0x01B8);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_msg_age, 0x01C0);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_unreachable, 0x01C8);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_source_quench, 0x0248);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_ttl_expired, 0x0250);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_parameter_problem, 0x0260);
RDP_ASSERT_OFFSET(rdp_stat, connections_dropped_icmp_unknown, 0x0270);
RDP_ASSERT_OFFSET(rdp_stat, icmp_received_with_connection, 0x0278);
RDP_ASSERT_OFFSET(rdp_stat, icmp_received_without_connection, 0x0280);
RDP_ASSERT_OFFSET(rdp_stat, icmp_unreachable, 0x0288);
RDP_ASSERT_OFFSET(rdp_stat, icmp_source_quench, 0x0308);
RDP_ASSERT_OFFSET(rdp_stat, icmp_ttl_expired, 0x0310);
RDP_ASSERT_OFFSET(rdp_stat, icmp_parameter_problem, 0x0320);
RDP_ASSERT_OFFSET(rdp_stat, icmp_unknown, 0x0330);
RDP_ASSERT_OFFSET(rdp_stat, sendto_calls, 0x0338);
RDP_ASSERT_OFFSET(rdp_stat, sendto_failures, 0x0340);
RDP_ASSERT_OFFSET(rdp_stat, sendto_consecutive_failures, 0x0348);
RDP_ASSERT_OFFSET(rdp_stat, last_sendto_failed, 0x0350);
RDP_ASSERT_OFFSET(rdp_stat, sendto_errno, 0x0358);
RDP_ASSERT_OFFSET(rdp_stat, sendto_errno_big, 0x0818);
RDP_ASSERT_OFFSET(rdp_stat, sendto_errno_big_last, 0x0820);
RDP_ASSERT_OFFSET(rdp_stat, rqd_last_interval, 0x0828);
RDP_ASSERT_OFFSET(rdp_stat, rqd_samples, 0x0830);
RDP_ASSERT_OFFSET(rdp_stat, rqd_min, 0x0838);
RDP_ASSERT_OFFSET(rdp_stat, rqd_max, 0x0840);
RDP_ASSERT_OFFSET(rdp_stat, rqd_sum, 0x0848);
RDP_ASSERT_OFFSET(rdp_stat, rqd_bytes, 0x0850);
RDP_ASSERT_OFFSET(rdp_stat, shipping_tx, 0x0858);
RDP_ASSERT_OFFSET(rdp_stat, shipping_tx_useless, 0x0860);
RDP_ASSERT_OFFSET(rdp_stat, shipping_tx_not_ready, 0x0868);
RDP_STATIC_ASSERT(sizeof(rdp_stat) == 0x870, "rdp_stat must be 0x870 bytes");

#endif /* RDP_RDPSTAT_H */
