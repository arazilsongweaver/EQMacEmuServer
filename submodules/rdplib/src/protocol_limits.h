// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_PROTOCOL_LIMITS_H
#define RDP_PROTOCOL_LIMITS_H

// These identifiers survive verbatim in recovered May assertion strings.
#define STREAMS_PER_CONNECTION 20
#define RDP_MAX_OUTSTANDING_IDS 4096
#define RDP_FRAGMENT_COUNT_MAX 100

// Original clients receive UDP datagrams into 536 bytes. A connected packet
// can contain 31 bytes of transport header and 512 bytes of fragment payload.
// CRC expands that to 547 bytes; encryption then pads it to at most 552.
#define RDP_LEGACY_DATAGRAM_BYTES 536u
#define RDP_MAX_CONNECTED_DATAGRAM_BYTES 552u

#endif /* RDP_PROTOCOL_LIMITS_H */
