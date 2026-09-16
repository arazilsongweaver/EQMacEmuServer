// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Block transformation used around connected RDP datagrams.
#ifndef RDP_CYPHER_H
#define RDP_CYPHER_H

#ifdef __cplusplus
extern "C"
{
#endif

// Applies the four round Feistel core to one eight byte block. Keys points to the 32 byte little endian round schedule.
void rdp_cypher(void *block, const char *keys);

// Encodes one or more backward chained eight byte blocks in place.
void rdp_encode(void *data, int blocks);

// Decodes one or more backward chained eight byte blocks in place.
void rdp_decode(void *data, int blocks);

#ifdef __cplusplus
}
#endif

#endif /* RDP_CYPHER_H */
