// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDPLIB_LOG_H
#define RDPLIB_LOG_H

#ifndef _WIN32

#include <stdint.h>
#include <time.h>

typedef struct rdplib_log_system_time_t
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
} rdplib_log_system_time_t;

static inline void rdplib_log_get_system_time(rdplib_log_system_time_t *system_time)
{
    struct tm utc_time;
    time_t now;

    now = time(NULL);
    (void)gmtime_r(&now, &utc_time);
    system_time->year = (uint32_t)(utc_time.tm_year + 1900);
    system_time->month = (uint32_t)(utc_time.tm_mon + 1);
    system_time->day = (uint32_t)utc_time.tm_mday;
    system_time->hour = (uint32_t)utc_time.tm_hour;
    system_time->minute = (uint32_t)utc_time.tm_min;
    system_time->second = (uint32_t)utc_time.tm_sec;
}

#endif /* !_WIN32 */

#endif /* RDPLIB_LOG_H */
