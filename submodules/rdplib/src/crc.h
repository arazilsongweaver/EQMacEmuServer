// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Reflected CRC32 used by connected RDP datagrams.
#ifndef RDP_CRC_H
#define RDP_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t rdp_crc(uint32_t crc, const char *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* RDP_CRC_H */
