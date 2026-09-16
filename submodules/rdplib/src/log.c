// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#if defined(_MSC_VER) && defined(RDPLIB_SOURCE_FAITHFUL)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "log.h"

#ifdef RDPLIB_SOURCE_FAITHFUL

#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include "rdplib_log.h"
#endif

void time_format(char *time_text)
{
#ifdef _WIN32
    SYSTEMTIME t;

    GetSystemTime(&t);
    sprintf(time_text, "[%02u/%02u/%02u %02u:%02u:%02u] ", t.wMonth, t.wDay, t.wYear % 100, t.wHour, t.wMinute, t.wSecond);
#else
    rdplib_log_system_time_t t;

    rdplib_log_get_system_time(&t);
    sprintf(time_text, "[%02u/%02u/%02u %02u:%02u:%02u] ", (unsigned)t.month, (unsigned)t.day, (unsigned)(t.year % 100), (unsigned)t.hour, (unsigned)t.minute, (unsigned)t.second);
#endif
}

void ftimeprint(FILE *file)
{
    char time_text[21];

    time_format(time_text);
    fprintf(file, time_text);
}

void discard_log_append(char *fmt, ...)
{
    static FILE *s_discard_log;

#ifdef _WIN32
    if (!s_discard_log)
        s_discard_log = fopen("discard.log", "a");
#else
    // Mac clients use the HFS path below.
    if (!s_discard_log)
        s_discard_log = fopen("Logs:discard.log", "a");
#endif
    if (s_discard_log)
    {
        va_list varg;

        ftimeprint(s_discard_log);
        va_start(varg, fmt);
        vfprintf(s_discard_log, fmt, varg);
        va_end(varg);
        fflush(s_discard_log);
    }
}

#endif /* RDPLIB_SOURCE_FAITHFUL */

#ifndef RDPLIB_SOURCE_FAITHFUL
// suppress MSVC warning C4206 when this file becomes empty in a normal build
typedef int rdplib_log_disabled_translation_unit;
#endif
