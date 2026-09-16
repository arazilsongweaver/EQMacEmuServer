// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdpstat.h"

#ifdef RDP_DEAD_CODE
#include "ustrerror.h"

#include <stdio.h>

rdp_stat rdp_stat_struct;
rdp_stat *g_rdp_stat = &rdp_stat_struct;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4473 4996)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
uint32_t rdp_stat_format(rdp_stat *rdp_stat, char *buffer, uint32_t verbose)
{
    uint64_t i;
    char *position;

    position = buffer;
    position += sprintf(position, "%I64u packets sent (%I64u bytes)\n", rdp_stat->best_effort_packets_tx + rdp_stat->guaranteed_packets_tx + rdp_stat->guaranteed_packets_retx,
                        rdp_stat->best_effort_bytes_tx + rdp_stat->guaranteed_bytes_tx + rdp_stat->guaranteed_bytes_retx);
    position += sprintf(position, " %I64u best effort packets sent (%I64u bytes)\n", rdp_stat->best_effort_packets_tx, rdp_stat->best_effort_bytes_tx);
    position += sprintf(position, " %I64u guaranteed packets sent (%I64u bytes)\n", rdp_stat->guaranteed_packets_tx, rdp_stat->guaranteed_bytes_tx);
    position += sprintf(position, " %I64u guaranteed packets retransmitted (%I64u bytes)\n", rdp_stat->guaranteed_packets_retx, rdp_stat->guaranteed_bytes_retx);
    position += sprintf(position, " %I64u packets with ack\n", rdp_stat->ack_only_packets_tx + rdp_stat->ack_and_data_packets_tx);
    position += sprintf(position, "  %I64u ack-only packets (%I64u delayed)\n", rdp_stat->ack_only_packets_tx, rdp_stat->ack_only_packets_tx);
    position += sprintf(position, "  %I64u packets with ack and data\n", rdp_stat->ack_and_data_packets_tx);
    position += sprintf(position, " %I64u packets without ack\n", rdp_stat->ack_and_data_packets_tx);

    position += sprintf(position, "%I64u packets received (%I64u bytes)\n", rdp_stat->best_effort_packets_rx + rdp_stat->guaranteed_packets_rx + rdp_stat->duplicate_packets_rx,
                        rdp_stat->best_effort_bytes_rx + rdp_stat->guaranteed_bytes_rx + rdp_stat->duplicate_bytes_rx);
    position += sprintf(position, " %I64u best effort packets received (%I64u bytes)\n", rdp_stat->best_effort_packets_rx, rdp_stat->best_effort_bytes_rx);
    position += sprintf(position, " %I64u guaranteed packets received (%I64u bytes)\n", rdp_stat->guaranteed_packets_rx, rdp_stat->guaranteed_bytes_rx);
    position += sprintf(position, " %I64u duplicate packets received (%I64u bytes)\n", rdp_stat->duplicate_packets_rx, rdp_stat->duplicate_bytes_rx);
    position += sprintf(position, " %I64u acks (for %I64u messages)\n", rdp_stat->ack_only_packets_rx + rdp_stat->ack_and_data_packets_rx, rdp_stat->messages_acked);
    position += sprintf(position, "  %I64u ack only packets\n", rdp_stat->ack_only_packets_rx);
    position += sprintf(position, "  %I64u ack and data packets\n", rdp_stat->ack_and_data_packets_rx);
    position += sprintf(position, " %I64u duplicate acks (%I64u bytes)\n", rdp_stat->duplicate_acks, rdp_stat->bytes_in_duplicate_acks);
    position += sprintf(position, " %I64u acks for unsent messages (invalid))\n", rdp_stat->acks_for_unsent_messages);
    position += sprintf(position, " %I64u packets (%I64u bytes) received in-sequence\n", rdp_stat->packets_rx_in_sequence, rdp_stat->bytes_rx_in_sequence);
    position += sprintf(position, " %I64u packets (%I64u bytes) received out-of-sequence\n", rdp_stat->packets_rx_out_of_sequence, rdp_stat->bytes_rx_out_of_sequence);
    position += sprintf(position, " %I64u packets received after close\n", rdp_stat->packets_received_after_close);
    position += sprintf(position, " %I64u packets received after disconnect\n", rdp_stat->packets_received_after_disconnect);

    if (rdp_stat->discarded_bad_options || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad options\n", rdp_stat->discarded_bad_options);
    }
    if (rdp_stat->discarded_old_seqnum || verbose)
    {
        position += sprintf(position, " %I64u discarded because old seqnum\n", rdp_stat->discarded_old_seqnum);
    }
    if (rdp_stat->discarded_dup_seqnum || verbose)
    {
        position += sprintf(position, " %I64u discarded because duplicate seqnum\n", rdp_stat->discarded_dup_seqnum);
    }
    if (rdp_stat->discarded_old_msgid || verbose)
    {
        position += sprintf(position, " %I64u discarded because old msgid\n", rdp_stat->discarded_old_msgid);
    }
    if (rdp_stat->discarded_bad_fragment || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad fragment fields\n", rdp_stat->discarded_bad_fragment);
    }
    if (rdp_stat->discarded_bad_stream || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad stream\n", rdp_stat->discarded_bad_stream);
    }
    if (rdp_stat->discarded_too_short || verbose)
    {
        position += sprintf(position, " %I64u discarded because packet too short\n", rdp_stat->discarded_too_short);
    }
    if (rdp_stat->discarded_bad_fragment_size || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad fragment size\n", rdp_stat->discarded_bad_fragment_size);
    }
    if (rdp_stat->discarded_bad_ack_header || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad ack header\n", rdp_stat->discarded_bad_ack_header);
    }
    if (rdp_stat->discarded_bad_ackmask || verbose)
    {
        position += sprintf(position, " %I64u discarded because bad ackmask\n", rdp_stat->discarded_bad_ackmask);
    }
    if (rdp_stat->discarded_old_ack || verbose)
    {
        position += sprintf(position, " %I64u discarded because old ack\n", rdp_stat->discarded_old_ack);
    }
    if (rdp_stat->discarded_mask_wo_ack || verbose)
    {
        position += sprintf(position, " %I64u discarded because mask without ack\n", rdp_stat->discarded_mask_wo_ack);
    }
    if (rdp_stat->discarded_invalid_localhost || verbose)
    {
        position += sprintf(position, " %I64u discarded invalid message from localhost\n", rdp_stat->discarded_invalid_localhost);
    }
    if (rdp_stat->discarded_bad_size || verbose)
    {
        position += sprintf(position, " %I64u discarded bad size\n", rdp_stat->discarded_bad_size);
    }
    if (rdp_stat->discarded_bad_crc || verbose)
    {
        position += sprintf(position, " %I64u discarded bad crc\n", rdp_stat->discarded_bad_crc);
    }
    if (rdp_stat->connection_requests || verbose)
    {
        position += sprintf(position, "%I64u connection requests\n", rdp_stat->connection_requests);
    }
    if (rdp_stat->connection_accepts || verbose)
    {
        position += sprintf(position, "%I64u connection accepts\n", rdp_stat->connection_accepts);
    }
    if (rdp_stat->bad_connection_attempts || verbose)
    {
        position += sprintf(position, " %I64u bad connection attempts\n", rdp_stat->bad_connection_attempts);
    }
    if (rdp_stat->packets_without_connection || verbose)
    {
        position += sprintf(position, " %I64u packets without connection\n", rdp_stat->packets_without_connection);
    }
    if (rdp_stat->connections_established || verbose)
    {
        position += sprintf(position, "%I64u connections established (including accepts)\n", rdp_stat->connections_established);
    }

    if (rdp_stat->connections_closed || verbose)
    {
        // The May 2002 implementation supplies only this argument even though the format string asks for the number of drops as well.
        position += sprintf(position, "%I64u connections closed (including %I64u drops)\n", rdp_stat->connections_closed);
    }

    if (rdp_stat->embryonic_connections_dropped || verbose)
    {
        position += sprintf(position, "%I64u embryonic connections dropped\n", rdp_stat->embryonic_connections_dropped);
    }

    if (rdp_stat->packets_updated_rtt_attempts || verbose)
    {
        position += sprintf(position, "%I64u packets updated rtt (of %I64u attempts)\n", rdp_stat->packets_updated_rtt, rdp_stat->packets_updated_rtt_attempts);
    }

    if (rdp_stat->retx_timeouts || verbose)
    {
        position += sprintf(position, "%I64u retransmit timeouts\n", rdp_stat->retx_timeouts);
    }
    if (rdp_stat->connections_dropped_hacker || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: hacker\n", rdp_stat->connections_dropped_hacker);
    }
    if (rdp_stat->connections_dropped_rst || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: reset\n", rdp_stat->connections_dropped_rst);
    }
    if (rdp_stat->connections_dropped_no_response || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: no response\n", rdp_stat->connections_dropped_no_response);
    }
    if (rdp_stat->connections_dropped_msg_age || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: msg age\n", rdp_stat->connections_dropped_msg_age);
    }

    if (rdp_stat->connections_dropped_unreachable[0] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: network unreachable\n", rdp_stat->connections_dropped_unreachable[0]);
    }
    if (rdp_stat->connections_dropped_unreachable[1] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: host unreachable\n", rdp_stat->connections_dropped_unreachable[1]);
    }
    if (rdp_stat->connections_dropped_unreachable[2] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: protocol unreachable\n", rdp_stat->connections_dropped_unreachable[2]);
    }
    if (rdp_stat->connections_dropped_unreachable[3] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: port unreachable\n", rdp_stat->connections_dropped_unreachable[3]);
    }
    if (rdp_stat->connections_dropped_unreachable[4] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: fragmentation needed\n", rdp_stat->connections_dropped_unreachable[4]);
    }
    if (rdp_stat->connections_dropped_unreachable[5] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: source route failed\n", rdp_stat->connections_dropped_unreachable[5]);
    }
    if (rdp_stat->connections_dropped_unreachable[6] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: destination network unknown\n", rdp_stat->connections_dropped_unreachable[6]);
    }
    if (rdp_stat->connections_dropped_unreachable[7] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: destination host unknown\n", rdp_stat->connections_dropped_unreachable[7]);
    }
    if (rdp_stat->connections_dropped_unreachable[7] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: source host isolated (obsolete)\n", rdp_stat->connections_dropped_unreachable[7]);
    }
    if (rdp_stat->connections_dropped_unreachable[9] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: destination network prohibited\n", rdp_stat->connections_dropped_unreachable[9]);
    }
    if (rdp_stat->connections_dropped_unreachable[10] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: destination host prohibited\n", rdp_stat->connections_dropped_unreachable[10]);
    }
    if (rdp_stat->connections_dropped_unreachable[11] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: network unreachable TOS\n", rdp_stat->connections_dropped_unreachable[11]);
    }
    if (rdp_stat->connections_dropped_unreachable[12] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: host unreachable TOS\n", rdp_stat->connections_dropped_unreachable[12]);
    }
    if (rdp_stat->connections_dropped_unreachable[13] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: administrative filter\n", rdp_stat->connections_dropped_unreachable[13]);
    }
    if (rdp_stat->connections_dropped_unreachable[14] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: host precedence violation\n", rdp_stat->connections_dropped_unreachable[14]);
    }
    if (rdp_stat->connections_dropped_unreachable[15] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: host precedence cutoff in effect\n", rdp_stat->connections_dropped_unreachable[15]);
    }
    if (rdp_stat->connections_dropped_source_quench || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: source quench\n", rdp_stat->connections_dropped_source_quench);
    }
    if (rdp_stat->connections_dropped_ttl_expired[0] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: ttl expired in transit\n", rdp_stat->connections_dropped_ttl_expired[0]);
    }
    if (rdp_stat->connections_dropped_ttl_expired[1] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: ttl expired in reassembly\n", rdp_stat->connections_dropped_ttl_expired[1]);
    }
    if (rdp_stat->connections_dropped_parameter_problem[0] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: IP header bad\n", rdp_stat->connections_dropped_parameter_problem[0]);
    }
    if (rdp_stat->connections_dropped_parameter_problem[1] || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: required option missing\n", rdp_stat->connections_dropped_parameter_problem[1]);
    }
    if (rdp_stat->connections_dropped_icmp_unknown || verbose)
    {
        position += sprintf(position, " %I64u connections dropped: unknown icmp\n", rdp_stat->connections_dropped_icmp_unknown);
    }

    if (rdp_stat->icmp_unreachable[0] || verbose)
    {
        position += sprintf(position, " %I64u icmp network unreachable\n", rdp_stat->icmp_unreachable[0]);
    }
    if (rdp_stat->icmp_unreachable[1] || verbose)
    {
        position += sprintf(position, " %I64u icmp host unreachable\n", rdp_stat->icmp_unreachable[1]);
    }
    if (rdp_stat->icmp_unreachable[2] || verbose)
    {
        position += sprintf(position, " %I64u icmp protocol unreachable\n", rdp_stat->icmp_unreachable[2]);
    }
    if (rdp_stat->icmp_unreachable[3] || verbose)
    {
        position += sprintf(position, " %I64u icmp port unreachable\n", rdp_stat->icmp_unreachable[3]);
    }
    if (rdp_stat->icmp_unreachable[4] || verbose)
    {
        position += sprintf(position, " %I64u icmp fragmentation needed\n", rdp_stat->icmp_unreachable[4]);
    }
    if (rdp_stat->icmp_unreachable[5] || verbose)
    {
        position += sprintf(position, " %I64u icmp source route failed\n", rdp_stat->icmp_unreachable[5]);
    }
    if (rdp_stat->icmp_unreachable[6] || verbose)
    {
        position += sprintf(position, " %I64u icmp destination network unknown\n", rdp_stat->icmp_unreachable[6]);
    }
    if (rdp_stat->icmp_unreachable[7] || verbose)
    {
        position += sprintf(position, " %I64u icmp destination host unknown\n", rdp_stat->icmp_unreachable[7]);
    }
    if (rdp_stat->icmp_unreachable[7] || verbose)
    {
        position += sprintf(position, " %I64u icmp source host isolated (obsolete)\n", rdp_stat->icmp_unreachable[7]);
    }
    if (rdp_stat->icmp_unreachable[9] || verbose)
    {
        position += sprintf(position, " %I64u icmp destination network prohibited\n", rdp_stat->icmp_unreachable[9]);
    }
    if (rdp_stat->icmp_unreachable[10] || verbose)
    {
        position += sprintf(position, " %I64u icmp destination host prohibited\n", rdp_stat->icmp_unreachable[10]);
    }
    if (rdp_stat->icmp_unreachable[11] || verbose)
    {
        position += sprintf(position, " %I64u icmp network unreachable TOS\n", rdp_stat->icmp_unreachable[11]);
    }
    if (rdp_stat->icmp_unreachable[12] || verbose)
    {
        position += sprintf(position, " %I64u icmp host unreachable TOS\n", rdp_stat->icmp_unreachable[12]);
    }
    if (rdp_stat->icmp_unreachable[13] || verbose)
    {
        position += sprintf(position, " %I64u icmp administrative filter\n", rdp_stat->icmp_unreachable[13]);
    }
    if (rdp_stat->icmp_unreachable[14] || verbose)
    {
        position += sprintf(position, " %I64u icmp host precedence violation\n", rdp_stat->icmp_unreachable[14]);
    }
    if (rdp_stat->icmp_unreachable[15] || verbose)
    {
        position += sprintf(position, " %I64u icmp host precedence cutoff in effect\n", rdp_stat->icmp_unreachable[15]);
    }
    if (rdp_stat->icmp_source_quench || verbose)
    {
        position += sprintf(position, " %I64u icmp source quench\n", rdp_stat->icmp_source_quench);
    }
    if (rdp_stat->icmp_ttl_expired[0] || verbose)
    {
        position += sprintf(position, " %I64u icmp ttl expired in transit\n", rdp_stat->icmp_ttl_expired[0]);
    }
    if (rdp_stat->icmp_ttl_expired[1] || verbose)
    {
        position += sprintf(position, " %I64u icmp ttl expired in reassembly\n", rdp_stat->icmp_ttl_expired[1]);
    }
    if (rdp_stat->icmp_parameter_problem[0] || verbose)
    {
        position += sprintf(position, " %I64u icmp IP header bad\n", rdp_stat->icmp_parameter_problem[0]);
    }
    if (rdp_stat->icmp_parameter_problem[1] || verbose)
    {
        position += sprintf(position, " %I64u icmp required option missing\n", rdp_stat->icmp_parameter_problem[1]);
    }
    if (rdp_stat->icmp_unknown || verbose)
    {
        position += sprintf(position, " %I64u unknown icmp\n", rdp_stat->icmp_unknown);
    }

    position += sprintf(position, " %I64u sendto calls (%I64u failures, %I64u consecutive)\n", rdp_stat->sendto_calls, rdp_stat->sendto_failures, rdp_stat->sendto_consecutive_failures);

    for (i = 0; i < 152; ++i)
    {
        if (rdp_stat->sendto_errno[i])
        {
            position += sprintf(position, "  %I64u %s\n", rdp_stat->sendto_errno[i], net_strerror((uint32_t)i));
        }
    }

    if (rdp_stat->sendto_errno_big)
    {
        position +=
            sprintf(position, "  %I64u errno > %u (last big errno==%u)\n", rdp_stat->sendto_errno_big, (uint32_t)rdp_stat->sendto_errno_big_last, (uint32_t)(rdp_stat->sendto_errno_big_last >> 32));
    }

    position += sprintf(position, " %I64u shipping::tx() (%I64u useless, %I64u not ready)\n", rdp_stat->shipping_tx, rdp_stat->shipping_tx_useless, rdp_stat->shipping_tx_not_ready);


    return (uint32_t)(position - buffer);
}

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#else

rdp_stat *g_rdp_stat;

#endif
