// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "msg_arrival.h"

#include <string.h>

#include "packet.h"
#include "rdplib_constants.h"

void msg_arrival_init(msg_arrival_t *arrival, uint16_t fragid)
{
    arrival->rxq_link.item = arrival;
    arrival->rxq_link.key.p = &arrival->fragid;
    arrival->fragments_collected = 0;
    arrival->fragid = fragid;
    arrival->sender = NULL;
}

void msg_arrival_prepare_for_sequencer(msg_arrival_t *arrival)
{
    arrival->rxq_link.key.p = &arrival->stream_seqnum;
}

void msg_arrival_prepare_for_rxq(msg_arrival_t *arrival)
{
    arrival->rxq_link.key.p = NULL;
}

uint32_t msg_arrival_assemble(msg_arrival_t *arrival, connection_t *sender, rdp_header_t *header, char *data)
{
    char *dst;

#ifndef RDPLIB_SOURCE_FAITHFUL
    // The original waits for fragment zero before recording the group geometry. Binding it to the first fragment prevents a later fragment zero from enlarging the allocation's declared extent.
    if (arrival->fragments_collected == 0 && header->frag_number != 0)
    {
        arrival->frag_total = header->frag_total;
        arrival->msgid = (uint16_t)(header->msgid - header->frag_number);
    }
#endif

    if (header->frag_number == 0)
    {
        arrival->options = header->options;
        arrival->seqnum = header->seqnum;
        arrival->msgid = header->msgid;
        arrival->frag_total = header->frag_total;
        arrival->stream = header->stream;
        arrival->stream_seqnum = header->stream_seqnum;
        arrival->sender = sender;
    }

    ++arrival->fragments_collected;
    dst = (char *)(arrival + 1) + RDP_FRAGMENT_PAYLOAD_BYTES * header->frag_number;
    memcpy(dst, data, header->data_size);

    if (header->frag_number + 1u == header->frag_total)
    {
        arrival->size = RDP_FRAGMENT_PAYLOAD_BYTES * header->frag_number + header->data_size;
    }

    return header->frag_total == arrival->fragments_collected;
}

void msg_arrival_init_disconnect_msg(msg_arrival_t *arrival, connection_t *sender)
{
    memset(arrival, 0, sizeof(*arrival));
    arrival->rxq_link.item = arrival;
    arrival->rxq_link.key.p = NULL;
    arrival->sender = sender;
}

uint32_t msg_arrival_validate_fragment_arrival(msg_arrival_t *arrival, rdp_header_t *header)
{
    uint32_t validate;
    uint16_t msgid;

    validate = 0;
#ifdef RDPLIB_SOURCE_FAITHFUL
    msgid = (uint16_t)(arrival->msgid + header->frag_number);
    if (arrival->sender && (arrival->frag_total != header->frag_total || msgid != header->msgid))
    {
        validate = 2;
    }
#else
    if (arrival->fragments_collected != 0)
    {
        msgid = (uint16_t)(arrival->msgid + header->frag_number);
        if (arrival->frag_total != header->frag_total || msgid != header->msgid)
        {
            validate = 2;
        }
    }
#endif
    return validate;
}

uint32_t msg_arrival_get_size(const msg_arrival_t *arrival)
{
    return arrival->size;
}

char *msg_arrival_get_data(const msg_arrival_t *arrival)
{
    return arrival->size ? (char *)(arrival + 1) : NULL;
}

connection_t *msg_arrival_get_sender(const msg_arrival_t *arrival)
{
    return arrival->sender;
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
struct sockaddr *msg_arrival_get_sender_addr(msg_arrival_t *arrival)
{
    return arrival->sender ? NULL : &arrival->from;
}
#endif

uint32_t msg_arrival_has_fin(const msg_arrival_t *arrival)
{
    return (arrival->options & RDP_FLAG_FIN) != 0;
}
