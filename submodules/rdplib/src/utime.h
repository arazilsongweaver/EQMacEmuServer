// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_UTIME_H
#define RDP_UTIME_H

#include <stdint.h>

#ifdef RDP_DEAD_CODE
struct timeval;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t time_get_ms(void);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void time_gettimeofday(struct timeval *tv);
#endif

void sleep_ms(uint32_t duration);

#ifdef __cplusplus
}
#endif

#endif /* RDP_UTIME_H */
