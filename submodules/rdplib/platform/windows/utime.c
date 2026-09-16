// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_platform.h"
#include "utime.h"

#include <mmsystem.h>
#include <time.h>

uint32_t time_get_ms(void)
{
    uint32_t t;

    t = (uint32_t)timeGetTime();
    return t;
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void time_gettimeofday(struct timeval *tv)
{
    uint32_t time;

    time = (uint32_t)timeGetTime();
    tv->tv_sec = (long)(time / 1000u);
    tv->tv_usec = (long)(1000u * (time % 1000u));
}
#endif

void sleep_ms(uint32_t duration)
{
    Sleep(duration);
}

uint32_t rdplib_platform_wall_time_seconds(void)
{
    return (uint32_t)time(NULL);
}
