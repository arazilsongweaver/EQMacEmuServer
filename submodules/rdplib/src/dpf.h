// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_DPF_H
#define RDP_DPF_H

#include <stdint.h>

#ifdef RDPLIB_DEBUG

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t data_format(char *dst, const uint8_t *data, uint32_t size);
void dpf(uint32_t filter, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* RDPLIB_DEBUG */

#endif /* RDP_DPF_H */
