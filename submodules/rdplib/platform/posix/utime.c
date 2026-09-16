// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_platform.h"
#include "utime.h"

#include <errno.h>
#ifdef RDP_DEAD_CODE
#include <sys/time.h>
#endif
#include <time.h>

uint32_t time_get_ms(void)
{
    struct timespec now = {0, 0};

    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * 1000u + (uint32_t)now.tv_nsec / 1000000u);
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void time_gettimeofday(struct timeval *tv)
{
    uint32_t time;

    time = time_get_ms();
    tv->tv_sec = (time_t)(time / 1000u);
    tv->tv_usec = (suseconds_t)(1000u * (time % 1000u));
}
#endif

void sleep_ms(uint32_t duration)
{
    struct timespec request;

    request.tv_sec = duration / 1000u;
    request.tv_nsec = (long)(duration % 1000u) * 1000000L;
    while (nanosleep(&request, &request) != 0 && errno == EINTR)
    {
    }
}

uint32_t rdplib_platform_wall_time_seconds(void)
{
    return (uint32_t)time(NULL);
}
