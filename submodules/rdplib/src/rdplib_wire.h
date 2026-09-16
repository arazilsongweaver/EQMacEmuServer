// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Alignment safe wire value access.
#ifndef RDPLIB_WIRE_H
#define RDPLIB_WIRE_H

#include <stdint.h>
#include <string.h>

#include "rdplib_platform.h"

static inline uint16_t rdplib_load_network_u16(const void *source)
{
    uint16_t value;

    memcpy(&value, source, sizeof(value));
    return ntohs(value);
}

#endif // RDPLIB_WIRE_H
