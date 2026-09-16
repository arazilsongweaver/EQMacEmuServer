// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDPLIB_CONNHASH_PORTABILITY_H
#define RDPLIB_CONNHASH_PORTABILITY_H

#include <stdint.h>
#include <string.h>

static inline uint16_t rdplib_connhash_load_u16(const void *source)
{
    uint16_t value;

    memcpy(&value, source, sizeof(value));
    return value;
}

static inline uint32_t rdplib_connhash_load_u32(const void *source)
{
    uint32_t value;

    memcpy(&value, source, sizeof(value));
    return value;
}

#endif /* RDPLIB_CONNHASH_PORTABILITY_H */
