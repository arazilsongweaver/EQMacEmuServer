// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#if defined(_MSC_VER) && defined(RDP_DEAD_CODE)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rx.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#ifdef RDP_DEAD_CODE
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "arrival_fragid.h"
#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "fast.h"
#ifdef RDPLIB_SOURCE_FAITHFUL
#include "log.h"
#include "rdp.h"
#endif
#include "rdpstat.h"
#include "rdplib_platform.h"
#include "sequencer.h"
#include "utime.h"

#ifdef RDPLIB_DEBUG
// Names retained so the recovered assertions produce the May object strings.
typedef int16_t int16;
#ifndef FALSE
#define FALSE 0
#define RDP_RX_UNDEFINE_FALSE
#endif
#endif

#define has_stream(arrival) ((arrival)->options & RDP_FLAG_SEQUENCED)

void rx_init(connection_t *c)
{
    int i;

    memset(&c->stat, 0, sizeof(c->stat));
    c->rx_received_all_thru = 0;
    c->rx_highest_received = 0;
    bitarray_clear(&c->rx_others_received);
    c->rx_msgid_count = 0;
    c->rx_time_last_arrival = time_get_ms();
    c->rx_time_last_msgid_arrival = time_get_ms();
    arrival_fragid_init(&c->rx_reassembly_pool);
    c->rx_best_effort_stream_seqnum_reset = 0;
    c->rx_syn_recvd = 0;
    c->rx_fin_recvd = 0;
    c->rx_fin_storage = NULL;
    c->rx_highest_seqnum_received = UINT16_MAX;
    c->rx_recent_seqnum_history = UINT64_MAX;
    memset(c->rx_guaranteed_stream_seqnum, 0, sizeof(c->rx_guaranteed_stream_seqnum));
    memset(c->rx_best_effort_stream_seqnum, 0, sizeof(c->rx_best_effort_stream_seqnum));
    for (i = 0; i < RDP_STREAM_COUNT; ++i)
    {
        sequencer_init(&c->rx_sequencer[i]);
    }

    c->trace_probes = NULL;
    c->trace_en_route = 0;
    c->trace_time = time_get_ms();
    c->trace_next_ttl = 1;
    c->trace_max_ttl = 30;
    c->trace_pass = 0;
    c->trace_next_index = 0;

    c->rx_connect_trace = NULL;
    c->rx_connect_count = 0;
    c->rx_icmp_received = 0;
}

uint32_t rx_create(connection_t *c)
{
    int i;

    arrival_fragid_create(&c->rx_reassembly_pool);
    for (i = 0; i < RDP_STREAM_COUNT; ++i)
    {
        sequencer_create(&c->rx_sequencer[i]);
    }
    return 0;
}

void rx_destroy(connection_t *c)
{
    int i;

    rx_flush_input_buffers(c);
    arrival_fragid_destroy(&c->rx_reassembly_pool);
    for (i = 0; i < RDP_STREAM_COUNT; ++i)
    {
        sequencer_destroy(&c->rx_sequencer[i]);
    }

    if (c->trace_probes)
    {
        free(c->trace_probes);
        c->trace_probes = NULL;
    }
}

void rx_flush_input_buffers(connection_t *c)
{
    int i;
    msg_arrival_t *arrival;

    if (c->rx_fin_storage)
    {
        fast_free(c->rx_fin_storage);
        c->rx_fin_storage = NULL;
#ifdef RDPLIB_DEBUG
        dpf(0x4000u, "FIN message received, but never delivered\n");
#endif
    }

    while ((arrival = arrival_fragid_remove_head(&c->rx_reassembly_pool)) != NULL)
    {
#ifdef RDPLIB_DEBUG
        dpf(0x2000u, "[0x%08x] incomplete message arrival deleted\n", (uint32_t)(uintptr_t)arrival);
#endif
        fast_free(arrival);
    }

    for (i = 0; i < RDP_STREAM_COUNT; ++i)
    {
        while ((arrival = sequencer_remove_head(&c->rx_sequencer[i])) != NULL)
        {
#ifdef RDPLIB_DEBUG
            dpf(0x2000u, "[0x%08x] out of sequence message arrival deleted\n", (uint32_t)(uintptr_t)arrival);
#endif
            fast_free(arrival);
        }
    }
}

void rx_record_packet_arrival(connection_t *c)
{
    c->rx_time_last_arrival = time_get_ms();
}

void rx_record_seqnum_arrival(connection_t *c, rdp_header_t *header)
{
    int16_t shift_size;

    shift_size = (int16_t)(header->seqnum - c->rx_highest_seqnum_received);
#ifdef RDPLIB_DEBUG
    assert(shift_size != 0);
#endif

    if (shift_size < 0)
    {
#ifdef RDPLIB_SOURCE_FAITHFUL
        uint32_t shift = (uint32_t)(-shift_size - 1) & 31u;
        uint32_t historical_mask = UINT32_C(1) << shift;
        uint64_t historical_result = historical_mask;

        if (historical_mask & UINT32_C(0x80000000))
        {
            historical_result |= UINT64_C(0xffffffff00000000);
        }

        // The original code made this 64 bit history mask from a signed 32 bit value. In the May build, TAKP Windows, and Intel Mac x86, shifts past bit 31 wrap back to bits 0..31;
        // bit 31 also fills the upper half with ones. PPC Mac fills the upper half at bit 31 too, but shifts past it produce zero. Either result can mark a new packet as a duplicate
        // or fail to mark a packet that was already received.
        //
        // Reliable data recovers: msgid tracking prevents duplicate delivery, and a falsely dropped message is retried with a new packet sequence. Ordered messages wait behind the
        // gap and drain when that retry arrives, so the stream does not stay out of sync. Sequenced unreliable data also rejects an exact replay, but a falsely dropped packet is lost.
        // Unsequenced unreliable data can be lost or delivered twice. The bad history bits eventually shift out as newer packets arrive.
        //
        // Reproduce the May/x86 behavior only in the source faithful build. Normal builds use the corrected 64 bit mask.
        c->rx_recent_seqnum_history |= historical_result;
#else
        c->rx_recent_seqnum_history |= UINT64_C(1) << (uint32_t)(-shift_size - 1);
#endif
        ++g_rdp_stat->packets_rx_out_of_sequence;
        g_rdp_stat->bytes_rx_out_of_sequence += (uint32_t)header->data_size + header->header_size;
        ++c->stat.packets_rx_out_of_sequence;
        c->stat.bytes_rx_out_of_sequence += (uint32_t)header->data_size + header->header_size;
    }
    else
    {
        ++g_rdp_stat->packets_rx_in_sequence;
        g_rdp_stat->bytes_rx_in_sequence += (uint32_t)header->data_size + header->header_size;
        ++c->stat.packets_rx_in_sequence;
        c->stat.bytes_rx_in_sequence += (uint32_t)header->data_size + header->header_size;

        if (shift_size > 64)
        {
            c->rx_recent_seqnum_history = 0;
        }
        else
        {
            c->rx_recent_seqnum_history <<= 1;
            c->rx_recent_seqnum_history |= 1;
            --shift_size;
            c->rx_recent_seqnum_history <<= shift_size;
        }
        c->rx_highest_seqnum_received = header->seqnum;
    }

    if ((int16_t)(header->seqnum - c->rx_best_effort_stream_seqnum_reset) > 16000)
    {
        uint32_t i;

        c->rx_best_effort_stream_seqnum_reset = (uint16_t)(header->seqnum - 1u);
        for (i = 0; i < RDP_STREAM_COUNT; ++i)
        {
            c->rx_best_effort_stream_seqnum[i] = c->rx_best_effort_stream_seqnum_reset;
        }
    }
}

uint32_t rx_validate_seqnum_arrival(connection_t *c, uint16_t seqnum)
{
    uint32_t validation;
    int16_t shift_size;

    validation = RDP_RX_ACCEPT;
    shift_size = (int16_t)(seqnum - c->rx_highest_seqnum_received);

    if (shift_size <= 0)
    {
        uint64_t mask;

        mask = 1;
        // May forms this mask for every nonpositive distance. Restrict the C
        // shift to the only range in which its value is subsequently read;
        // the equality and outside window cases ignore the historical result.
        if (shift_size >= -64 && shift_size < 0)
        {
            mask <<= -shift_size - 1;
        }

        if (shift_size == 0 || (shift_size >= -64 && (c->rx_recent_seqnum_history & mask)))
        {
            validation = RDP_RX_DISCARD;
            ++g_rdp_stat->discarded_dup_seqnum;
            ++c->stat.discarded_dup_seqnum;
        }
    }

    if (!validation && ((int16_t)(seqnum + 64u - c->rx_highest_seqnum_received) < 0 ||
                        (int16_t)(seqnum - 4096u - c->rx_highest_seqnum_received) > 0))
    {
        validation = RDP_RX_DISCARD;
        ++g_rdp_stat->discarded_old_seqnum;
        ++c->stat.discarded_old_seqnum;
    }
    return validation;
}

uint32_t rx_validate_msgid_arrival(connection_t *c, rdp_header_t *header)
{
    uint32_t validation;

    validation = RDP_RX_ACCEPT;
    if (c->rx_syn_recvd &&
        ((int16_t)(header->msgid - c->rx_received_all_thru - 120u) >= 0 ||
         ((header->options & RDP_FLAG_SYN) && header->msgid != c->rx_syn_msgid)))
    {
        validation = RDP_RX_DISCARD;
#ifdef RDPLIB_SOURCE_FAITHFUL
        {
            char addr[64];

            format_sockaddr(addr, &c->tx_remote_addr);
            discard_log_append("%s bad msgid %i=%u-%u-%u, %s%u\n", addr, (int16_t)(header->msgid - c->rx_received_all_thru), header->msgid,
                               c->rx_received_all_thru, 120u, (header->options & RDP_FLAG_SYN) ? "SYN " : "", c->rx_syn_msgid);
        }
#endif
        ++g_rdp_stat->discarded_old_msgid;
        ++c->stat.discarded_old_msgid;
    }
    return validation;
}

uint32_t rx_validate_fragment_arrival(connection_t *c, rdp_header_t *header)
{
    uint32_t validation;
    msg_arrival_t *arrival;

    validation = RDP_RX_ACCEPT;
    if (header->frag_number >= header->frag_total || header->frag_total < 2 ||
        (header->frag_number + 1u != header->frag_total && header->data_size != RDP_FRAGMENT_PAYLOAD_BYTES))
    {
        validation = RDP_RX_ABORT;
#ifdef RDPLIB_SOURCE_FAITHFUL
        {
            char addr[64];

            // Unsafe historical diagnostic: its format has five integer conversions, but the object supplies only the following four arguments.
            format_sockaddr(addr, &c->tx_remote_addr);
            discard_log_append("%s bad fragment: %u %u %u %u %u\n", addr, header->frag_number, header->frag_total, RDP_FRAGMENT_PAYLOAD_BYTES,
                               header->data_size);
        }
#endif
        ++g_rdp_stat->discarded_bad_fragment;
        ++c->stat.discarded_bad_fragment;
    }
    else
    {
        arrival = arrival_fragid_lookup(&c->rx_reassembly_pool, &header->fragid);
        if (arrival)
        {
            validation = msg_arrival_validate_fragment_arrival(arrival, header);
            if (validation)
            {
#ifdef RDPLIB_SOURCE_FAITHFUL
                {
                    char addr[64];

                    format_sockaddr(addr, &c->tx_remote_addr);
                    discard_log_append("%s bad fragment size fragid:%u part:%u/%u msgid:%u a_total:%u a_msgid:%u\n", addr, header->fragid,
                                       header->frag_number, header->frag_total, header->msgid, arrival->frag_total, arrival->msgid);
                }
#endif
                ++g_rdp_stat->discarded_bad_fragment_size;
                ++c->stat.discarded_bad_fragment_size;
            }
        }
    }

    if (validation)
    {
#ifdef RDPLIB_DEBUG
        dpf(0x02000000u, "bad fragment received\n");
#endif
    }
    return validation;
}

uint32_t rx_validate_stream_arrival(connection_t *c, rdp_header_t *header)
{
    uint32_t validation;

    validation = RDP_RX_ACCEPT;
    if (header->stream >= RDP_STREAM_COUNT)
    {
        validation = RDP_RX_ABORT;
#ifdef RDPLIB_SOURCE_FAITHFUL
        {
            char addr[64];

            format_sockaddr(addr, &c->tx_remote_addr);
            discard_log_append("%s bad stream id: %u >= %u\n", addr, header->stream, RDP_STREAM_COUNT);
        }
#endif
        ++g_rdp_stat->discarded_bad_stream;
        ++c->stat.discarded_bad_stream;
    }
    return validation;
}

uint32_t rx_record_msgid_arrival(connection_t *c, uint16_t msgid)
{
    uint32_t duplicate;

    duplicate = 0;
    c->rx_time_last_msgid_arrival = time_get_ms();

    if (!c->rx_syn_recvd)
    {
        c->rx_syn_recvd = 1;
        c->rx_syn_msgid = msgid;
        c->rx_received_all_thru = (uint16_t)(msgid - 1u);
        c->rx_highest_received = c->rx_received_all_thru;
#ifdef RDPLIB_DEBUG
        dpf(0x4000u, "[0x%08x] SYN received\n", (uint32_t)(uintptr_t)c);
#endif
    }

    if (c->rx_msgid_count)
    {
        if (msgid < c->rx_msgid_lo)
        {
            c->rx_msgid_lo = msgid;
        }
        else if (msgid > c->rx_msgid_hi)
        {
            c->rx_msgid_hi = msgid;
        }
    }
    else
    {
        c->rx_msgid_lo = msgid;
        c->rx_msgid_hi = msgid;
    }
    ++c->rx_msgid_count;

#ifdef RDPLIB_DEBUG
    dpf(4u, "receiving: recording msgid arrival (%u)\n", msgid);
#endif

    if ((int16_t)(msgid - c->rx_received_all_thru) > 0)
    {
        uint16_t shift_size;
        uint16_t was_set;
        uint16_t bit_number;

        if ((int16_t)(msgid - c->rx_highest_received) > 0)
        {
            c->rx_highest_received = msgid;
        }

        bit_number = (uint16_t)(msgid - c->rx_received_all_thru - 1u);
#ifdef RDPLIB_DEBUG
        dpf(4u, "receiving: bit_number==%u\n", bit_number);
#endif
        was_set = bitarray_setbit(&c->rx_others_received, bit_number);
        if (was_set)
        {
            duplicate = 1;
        }

        shift_size = 0;
        while (bitarray_getbit(&c->rx_others_received, shift_size))
        {
            ++shift_size;
        }

        if (shift_size > 0)
        {
            uint16_t bits_to_move;

            bits_to_move = (uint16_t)(c->rx_highest_received - c->rx_received_all_thru - shift_size);
#ifdef RDPLIB_DEBUG
            assert(( (int16)bits_to_move ) >= 0);
#else
            // Preserve the recovered calculation even when its debug only
            // assertion is compiled out.
            (void)bits_to_move;
#endif
            bitarray_left_shift(&c->rx_others_received, shift_size);
            c->rx_received_all_thru = (uint16_t)(c->rx_received_all_thru + shift_size);
        }
    }
    else
    {
        duplicate = 1;
    }
    return duplicate;
}

uint32_t rx_append_ack(connection_t *c, uint16_t *dst, uint16_t *options)
{
    uint32_t ack_size;

    ack_size = 0;
#ifndef RDPLIB_SOURCE_FAITHFUL
    if (!dst || !options)
    {
        return 0;
    }
#endif
    if (c->rx_msgid_count)
    {
        uint16_t mask_size_bits;
        uint16_t mask_size_bytes;

        if ((int16_t)c->rx_msgid_lo - (int32_t)c->rx_received_all_thru > 8)
        {
#ifdef RDPLIB_DEBUG
            dpf(0x30000u, "Appending MASKOFFSET (c->rx_msgid_count==%u)\n", c->rx_msgid_count);
#endif
            *dst++ = htons(c->rx_msgid_lo);
            ack_size += 2;
            *options |= RDP_FLAG_MASKOFFSET;
        }
        else
        {
#ifdef RDPLIB_DEBUG
            dpf(0x30000u, "Appending ACKTHRU (c->rx_msgid_count==%u)\n", c->rx_msgid_count);
#endif
            *dst++ = htons(c->rx_received_all_thru);
            ack_size += 2;
            *options |= RDP_FLAG_ACKTHRU;

            c->rx_msgid_lo = c->rx_received_all_thru;
            if ((int16_t)(c->rx_msgid_lo - c->rx_msgid_hi) > 0)
            {
                c->rx_msgid_hi = c->rx_msgid_lo;
            }
        }

        mask_size_bits = (uint16_t)(c->rx_msgid_hi - c->rx_msgid_lo);
        mask_size_bytes = (uint16_t)((mask_size_bits + 7u) >> 3);
        if (mask_size_bytes >= 15u)
        {
            mask_size_bytes = 15;
        }

        if (mask_size_bytes)
        {
            *options |= (uint16_t)(mask_size_bytes << 4);
            bitarray_copy(&c->rx_others_received, (uint8_t *)dst,
                          (uint16_t)(c->rx_msgid_lo - c->rx_received_all_thru), mask_size_bytes);
            ack_size += mask_size_bytes;
        }
        c->rx_msgid_count = 0;
    }
    return ack_size;
}

void rx_sort_into_sequence(connection_t *c, msg_arrival_t *arrival)
{
#ifdef RDPLIB_DEBUG
    dpf(0x1000u, "guaranteed stream: %u, expecting: %u, received: %u (%u)\n", arrival->stream,
        c->rx_guaranteed_stream_seqnum[arrival->stream], arrival->stream_seqnum, arrival->msgid);
#endif
    msg_arrival_prepare_for_sequencer(arrival);
    sequencer_insert(&c->rx_sequencer[arrival->stream], arrival);
}

msg_arrival_t *rx_get_next_in_sequence(connection_t *c, uint8_t stream)
{
    msg_arrival_t *next_in_sequence;

    next_in_sequence = sequencer_peek_head(&c->rx_sequencer[stream]);
    if (next_in_sequence)
    {
#ifdef RDPLIB_DEBUG
        assert(has_stream(next_in_sequence));
#endif
        if (c->rx_guaranteed_stream_seqnum[stream] == next_in_sequence->stream_seqnum)
        {
            ++c->rx_guaranteed_stream_seqnum[stream];
            next_in_sequence = sequencer_remove_head(&c->rx_sequencer[stream]);
        }
        else
        {
            next_in_sequence = NULL;
        }
    }
    return next_in_sequence;
}

uint32_t rx_in_sequence(connection_t *c, msg_arrival_t *arrival)
{
    uint32_t in_order;
    int16_t offset;
    uint16_t next_seqnum;

    in_order = 1;
    next_seqnum = c->rx_best_effort_stream_seqnum[arrival->stream];
    offset = (int16_t)(arrival->seqnum - next_seqnum);
    if (offset >= 0)
    {
        c->rx_best_effort_stream_seqnum[arrival->stream] = (uint16_t)(arrival->seqnum + 1u);
    }
    else
    {
#ifdef RDPLIB_DEBUG
        dpf(0x1000u, "best effort stream: %u, minimum: %u, received: %u\n", arrival->stream, next_seqnum, arrival->seqnum);
#endif
        in_order = 0;
    }
    return in_order;
}

msg_arrival_t *rx_assemble(connection_t *c, rdp_header_t *header, char *data)
{
    msg_arrival_t *arrival;
    uint32_t total_size;
    uint32_t new_fragid;

    arrival = NULL;
    total_size = header->data_size;
    if (header->options & RDP_FLAG_FRAGMENT)
    {
        arrival = arrival_fragid_lookup(&c->rx_reassembly_pool, &header->fragid);
        total_size = (uint32_t)header->frag_total * RDP_FRAGMENT_PAYLOAD_BYTES;
    }

    if (!arrival)
    {
        arrival = (msg_arrival_t *)fast_malloc((uint32_t)sizeof(msg_arrival_t) + total_size);
#ifdef RDPLIB_SOURCE_FAITHFUL
        // The clients initialize this unchecked allocation before reaching their null branch.
        msg_arrival_init(arrival, header->fragid);
#else
        if (!arrival)
        {
#ifdef RDPLIB_DEBUG
            dpf(UINT32_MAX, "malloc failure, message arrival discarded\n");
#endif
            return NULL;
        }
        msg_arrival_init(arrival, header->fragid);
#endif
        new_fragid = 1;
    }
    else
    {
        new_fragid = 0;
    }

    if (arrival)
    {
        uint32_t complete;

        complete = msg_arrival_assemble(arrival, c, header, data);
        if (complete)
        {
            if (!new_fragid)
            {
                arrival = arrival_fragid_remove_by_ptr(&c->rx_reassembly_pool, arrival);
#ifdef RDPLIB_DEBUG
                assert(arrival != NULL);
#endif
            }
        }
        else
        {
            if (new_fragid)
            {
                arrival_fragid_insert(&c->rx_reassembly_pool, arrival);
            }
            arrival = NULL;
        }

        if (header->options & RDP_FLAG_FRAGMENT)
        {
            if (complete)
            {
#ifdef RDPLIB_DEBUG
                dpf(0x800u, "received last fragment [%u:%u/%u] (%u)\n", header->fragid, header->frag_number, header->frag_total, header->msgid);
#endif
            }
            else if (new_fragid)
            {
#ifdef RDPLIB_DEBUG
                dpf(0x800u, "receiving new fragmented message [%u:%u/%u] (%u)\n", header->fragid, header->frag_number, header->frag_total, header->msgid);
#endif
            }
            else
            {
#ifdef RDPLIB_DEBUG
                dpf(0x800u, "received fragment [%u:%u/%u] (%u)\n", header->fragid, header->frag_number, header->frag_total, header->msgid);
#endif
            }
        }
        else if (header->options & RDP_FLAG_MSGID)
        {
#ifdef RDPLIB_DEBUG
            dpf(0x800u, "receiving message (%u)\n", header->msgid);
#endif
        }
    }
    else
    {
#ifdef RDPLIB_DEBUG
        dpf(UINT32_MAX, "malloc failure, message arrival discarded\n");
#endif
    }
    return arrival;
}

msg_arrival_t *rx_load_fin_arrival(connection_t *c)
{
    msg_arrival_t *arrival;

    arrival = c->rx_fin_storage;
#ifdef RDPLIB_DEBUG
    assert(c->rx_fin_storage != NULL);
#endif
    c->rx_fin_storage = NULL;
    return arrival;
}

void rx_save_fin_arrival(connection_t *c, msg_arrival_t *arrival)
{
#ifdef RDPLIB_DEBUG
    dpf(0x4000u, "[0x%08x] FIN received\n", (uint32_t)(uintptr_t)c);
    assert(c->rx_fin_storage == NULL);
    assert(c->rx_fin_recvd == FALSE);
#endif
    c->rx_fin_storage = arrival;
    c->rx_fin_recvd = 1;
    c->rx_fin_msgid = arrival->msgid;
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t c_stat_format(connection_stat *c_stat, char *buffer, uint32_t verbose)
{
    char *position;

    position = buffer;
    position += sprintf(position, "%u packets sent (%u bytes)\n", c_stat->guaranteed_packets_retx + c_stat->guaranteed_packets_tx + c_stat->best_effort_packets_tx,
                        c_stat->guaranteed_bytes_retx + c_stat->guaranteed_bytes_tx + c_stat->best_effort_bytes_tx);
    position += sprintf(position, " %u best effort packets sent (%u bytes)\n", c_stat->best_effort_packets_tx, c_stat->best_effort_bytes_tx);
    position += sprintf(position, " %u guaranteed packets sent (%u bytes)\n", c_stat->guaranteed_packets_tx, c_stat->guaranteed_bytes_tx);
    position += sprintf(position, " %u guaranteed packets retransmitted (%u bytes)\n", c_stat->guaranteed_packets_retx, c_stat->guaranteed_bytes_retx);
    position += sprintf(position, " %u packets with ack\n", c_stat->ack_and_data_packets_tx + c_stat->ack_only_packets_tx);
    position += sprintf(position, "  %u ack-only packets (%u delayed)\n", c_stat->ack_only_packets_tx, c_stat->ack_only_packets_tx);
    position += sprintf(position, "  %u packets with ack and data\n", c_stat->ack_and_data_packets_tx);
    position += sprintf(position, " %u packets without ack\n", c_stat->ack_and_data_packets_tx);
    position += sprintf(position, "%u packets received (%u bytes)\n", c_stat->duplicate_packets_rx + c_stat->guaranteed_packets_rx + c_stat->best_effort_packets_rx,
                        c_stat->duplicate_bytes_rx + c_stat->guaranteed_bytes_rx + c_stat->best_effort_bytes_rx);
    position += sprintf(position, " %u best effort packets received (%u bytes)\n", c_stat->best_effort_packets_rx, c_stat->best_effort_bytes_rx);
    position += sprintf(position, " %u guaranteed packets received (%u bytes)\n", c_stat->guaranteed_packets_rx, c_stat->guaranteed_bytes_rx);
    position += sprintf(position, " %u duplicate packets received (%u bytes)\n", c_stat->duplicate_packets_rx, c_stat->duplicate_bytes_rx);
    position += sprintf(position, " %u acks (for %u messages)\n", c_stat->ack_and_data_packets_rx + c_stat->ack_only_packets_rx, c_stat->messages_acked);
    position += sprintf(position, "  %u ack only packets\n", c_stat->ack_only_packets_rx);
    position += sprintf(position, "  %u ack and data packets\n", c_stat->ack_and_data_packets_rx);
    position += sprintf(position, " %u duplicate acks (%u bytes)\n", c_stat->duplicate_acks, c_stat->bytes_in_duplicate_acks);
    position += sprintf(position, " %u acks for unsent messages (invalid))\n", c_stat->acks_for_unsent_messages);
    position += sprintf(position, " %u packets (%u bytes) received in-sequence\n", c_stat->packets_rx_in_sequence, c_stat->bytes_rx_in_sequence);
    position += sprintf(position, " %u packets (%u bytes) received out-of-sequence\n", c_stat->packets_rx_out_of_sequence, c_stat->bytes_rx_out_of_sequence);

    if (c_stat->discarded_bad_options || verbose) position += sprintf(position, " %u discarded because bad options\n", c_stat->discarded_bad_options);
    if (c_stat->discarded_old_seqnum || verbose) position += sprintf(position, " %u discarded because old seqnum\n", c_stat->discarded_old_seqnum);
    if (c_stat->discarded_dup_seqnum || verbose) position += sprintf(position, " %u discarded because duplicate seqnum\n", c_stat->discarded_dup_seqnum);
    if (c_stat->discarded_old_msgid || verbose) position += sprintf(position, " %u discarded because old msgid\n", c_stat->discarded_old_msgid);
    if (c_stat->discarded_bad_fragment || verbose) position += sprintf(position, " %u discarded because bad fragment fields\n", c_stat->discarded_bad_fragment);
    if (c_stat->discarded_bad_stream || verbose) position += sprintf(position, " %u discarded because bad stream\n", c_stat->discarded_bad_stream);
    if (c_stat->discarded_too_short || verbose) position += sprintf(position, " %u discarded because packet too short\n", c_stat->discarded_too_short);
    if (c_stat->discarded_bad_fragment_size || verbose) position += sprintf(position, " %u discarded because bad fragment size\n", c_stat->discarded_bad_fragment_size);
    if (c_stat->discarded_bad_ack_header || verbose) position += sprintf(position, " %u discarded because bad ack header\n", c_stat->discarded_bad_ack_header);
    if (c_stat->discarded_bad_ackmask || verbose) position += sprintf(position, " %u discarded because bad ackmask\n", c_stat->discarded_bad_ackmask);
    if (c_stat->discarded_old_ack || verbose) position += sprintf(position, " %u discarded because old ack\n", c_stat->discarded_old_ack);
    if (c_stat->discarded_mask_wo_ack || verbose) position += sprintf(position, " %u discarded because mask without ack\n", c_stat->discarded_mask_wo_ack);
    if (c_stat->packets_updated_rtt_attempts || verbose)
    {
        position += sprintf(position, "%u packets updated rtt (of %u attempts)\n", c_stat->packets_updated_rtt, c_stat->packets_updated_rtt_attempts);
    }

    if (c_stat->icmp_unreachable[0] || verbose) position += sprintf(position, " %u icmp network unreachable\n", c_stat->icmp_unreachable[0]);
    if (c_stat->icmp_unreachable[1] || verbose) position += sprintf(position, " %u icmp host unreachable\n", c_stat->icmp_unreachable[1]);
    if (c_stat->icmp_unreachable[2] || verbose) position += sprintf(position, " %u icmp protocol unreachable\n", c_stat->icmp_unreachable[2]);
    if (c_stat->icmp_unreachable[3] || verbose) position += sprintf(position, " %u icmp port unreachable\n", c_stat->icmp_unreachable[3]);
    if (c_stat->icmp_unreachable[4] || verbose) position += sprintf(position, " %u icmp fragmentation needed\n", c_stat->icmp_unreachable[4]);
    if (c_stat->icmp_unreachable[5] || verbose) position += sprintf(position, " %u icmp source route failed\n", c_stat->icmp_unreachable[5]);
    if (c_stat->icmp_unreachable[6] || verbose) position += sprintf(position, " %u icmp destination network unknown\n", c_stat->icmp_unreachable[6]);
    if (c_stat->icmp_unreachable[7] || verbose) position += sprintf(position, " %u icmp destination host unknown\n", c_stat->icmp_unreachable[7]);
    if (c_stat->icmp_unreachable[7] || verbose) position += sprintf(position, " %u icmp source host isolated (obsolete)\n", c_stat->icmp_unreachable[7]);
    if (c_stat->icmp_unreachable[9] || verbose) position += sprintf(position, " %u icmp destination network prohibited\n", c_stat->icmp_unreachable[9]);
    if (c_stat->icmp_unreachable[10] || verbose) position += sprintf(position, " %u icmp destination host prohibited\n", c_stat->icmp_unreachable[10]);
    if (c_stat->icmp_unreachable[11] || verbose) position += sprintf(position, " %u icmp network unreachable TOS\n", c_stat->icmp_unreachable[11]);
    if (c_stat->icmp_unreachable[12] || verbose) position += sprintf(position, " %u icmp host unreachable TOS\n", c_stat->icmp_unreachable[12]);
    if (c_stat->icmp_unreachable[13] || verbose) position += sprintf(position, " %u icmp administrative filter\n", c_stat->icmp_unreachable[13]);
    if (c_stat->icmp_unreachable[14] || verbose) position += sprintf(position, " %u icmp host precedence violation\n", c_stat->icmp_unreachable[14]);
    if (c_stat->icmp_unreachable[15] || verbose) position += sprintf(position, " %u icmp host precedence cutoff in effect\n", c_stat->icmp_unreachable[15]);
    if (c_stat->icmp_source_quench || verbose) position += sprintf(position, " %u icmp source quench\n", c_stat->icmp_source_quench);
    if (c_stat->icmp_ttl_expired[0] || verbose) position += sprintf(position, " %u icmp ttl expired in transit\n", c_stat->icmp_ttl_expired[0]);
    if (c_stat->icmp_ttl_expired[1] || verbose) position += sprintf(position, " %u icmp ttl expired in reassembly\n", c_stat->icmp_ttl_expired[1]);
    if (c_stat->icmp_parameter_problem[0] || verbose) position += sprintf(position, " %u icmp IP header bad\n", c_stat->icmp_parameter_problem[0]);
    if (c_stat->icmp_parameter_problem[1] || verbose) position += sprintf(position, " %u icmp required option missing\n", c_stat->icmp_parameter_problem[1]);
    if (c_stat->icmp_unknown || verbose) position += sprintf(position, " %u unknown icmp\n", c_stat->icmp_unknown);

    return (uint32_t)(position - buffer);
}
#endif /* RDP_DEAD_CODE */

#ifdef RDP_RX_UNDEFINE_FALSE
#undef FALSE
#undef RDP_RX_UNDEFINE_FALSE
#endif

#undef has_stream
