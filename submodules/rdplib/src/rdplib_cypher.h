// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Alignment safe little endian access used by the portable cypher implementation.
#ifndef RDPLIB_CYPHER_H
#define RDPLIB_CYPHER_H

#include <stdint.h>

static inline uint32_t rdplib_cypher_load_le32(const void *source)
{
    const uint8_t *bytes = (const uint8_t *)source;

    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static inline void rdplib_cypher_store_le32(void *destination, uint32_t value)
{
    uint8_t *bytes = (uint8_t *)destination;

    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

#endif /* RDPLIB_CYPHER_H */
