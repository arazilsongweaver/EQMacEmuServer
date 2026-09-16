// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "msg_outgoing.h"

#include <string.h>

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif

#include "rdplib_constants.h"
#include "rdplib_platform.h"
#ifdef RDPLIB_DEBUG
#include "utime.h"
#define RDP_HEADER_OPTION_MSGID RDP_FLAG_MSGID
#endif

void msg_outgoing_init(msg_outgoing_t *outgoing)
{
    uint16_t msgid;
    uint16_t fragid;
    uint16_t frag_total;
    uint16_t frag_number;

    outgoing->txq_link.item = outgoing;
    outgoing->txq_link.key.p = NULL;
    outgoing->size = 0;
    outgoing->attempts = 0;

    if (outgoing->options & RDP_FLAG_MSGID)
    {
        msgid = htons(outgoing->msgid);
        memcpy(msg_outgoing_get_data(outgoing) + outgoing->size, &msgid, sizeof(msgid));
        outgoing->size += sizeof(msgid);
    }

    if (outgoing->options & RDP_FLAG_FRAGMENT)
    {
#ifdef RDPLIB_DEBUG
        assert(outgoing->options & RDP_HEADER_OPTION_MSGID);
#endif

        fragid = htons(outgoing->fragid);
        memcpy(msg_outgoing_get_data(outgoing) + outgoing->size, &fragid, sizeof(fragid));
        outgoing->size += sizeof(fragid);

        frag_number = htons(outgoing->frag_number);
        memcpy(msg_outgoing_get_data(outgoing) + outgoing->size, &frag_number, sizeof(frag_number));
        outgoing->size += sizeof(frag_number);

        frag_total = htons(outgoing->frag_total);
        memcpy(msg_outgoing_get_data(outgoing) + outgoing->size, &frag_total, sizeof(frag_total));
        outgoing->size += sizeof(frag_total);
    }

    if (outgoing->options & RDP_FLAG_SEQUENCED)
    {
        msg_outgoing_get_data(outgoing)[outgoing->size] = (char)outgoing->stream;
        ++outgoing->size;
        if (outgoing->options & RDP_FLAG_MSGID)
        {
            msg_outgoing_get_data(outgoing)[outgoing->size] = (char)outgoing->stream_seqnum;
            ++outgoing->size;
        }
    }

#ifdef RDPLIB_DEBUG
    outgoing->enqueue_time = time_get_ms();
#else
    outgoing->enqueue_time = 0;
#endif
}
