// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#if defined(_MSC_VER) && defined(RDP_DEAD_CODE)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "serial_rx.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <string.h>

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "rdplib_platform.h"
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
#include "utime.h"
#endif

#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997u
#endif

#ifdef RDP_DEAD_CODE

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// unused, retained for historical interest
void __dpf_release(const char *fmt, ...)
{
    char temp[1024];

#ifdef _WIN32
    static HANDLE __dpf_file_handle = INVALID_HANDLE_VALUE;

    if (__dpf_file_handle == INVALID_HANDLE_VALUE)
    {
        DWORD len;
        DWORD written;
        time_t aclock;

        strcpy(temp, "vince.txt");
        __dpf_file_handle = CreateFileA(temp, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH | FILE_ATTRIBUTE_NORMAL, NULL);
        if (__dpf_file_handle != INVALID_HANDLE_VALUE)
        {
            time(&aclock);
            len = (DWORD)sprintf(temp, "%s", ctime(&aclock));
            len += (DWORD)sprintf(&temp[len], "\r");
            WriteFile(__dpf_file_handle, temp, len, &written, NULL);
            FlushFileBuffers(__dpf_file_handle);
        }
    }

    if (__dpf_file_handle != INVALID_HANDLE_VALUE)
    {
        DWORD len;
        DWORD written;
        va_list va;

        va_start(va, fmt);
        len = (DWORD)vsprintf(temp, fmt, va);
        va_end(va);
        WriteFile(__dpf_file_handle, temp, len, &written, NULL);
        FlushFileBuffers(__dpf_file_handle);
    }
#else
    static FILE *__dpf_file_handle;

    if (__dpf_file_handle == NULL)
    {
        uint32_t len;
        size_t written;
        time_t aclock;

        strcpy(temp, "vince.txt");
        __dpf_file_handle = fopen(temp, "r+b");
        if (__dpf_file_handle == NULL)
        {
            __dpf_file_handle = fopen(temp, "w+b");
        }
        if (__dpf_file_handle != NULL)
        {
            time(&aclock);
            len = (uint32_t)sprintf(temp, "%s", ctime(&aclock));
            len += (uint32_t)sprintf(&temp[len], "\r");
            written = fwrite(temp, 1, len, __dpf_file_handle);
            (void)written;
            fflush(__dpf_file_handle);
        }
    }

    if (__dpf_file_handle != NULL)
    {
        uint32_t len;
        size_t written;
        va_list va;

        va_start(va, fmt);
        len = (uint32_t)vsprintf(temp, fmt, va);
        va_end(va);
        written = fwrite(temp, 1, len, __dpf_file_handle);
        (void)written;
        fflush(__dpf_file_handle);
    }
#endif
}

#endif /* RDP_DEAD_CODE */

void serial_rx_init(serial_rx_t *serial_rx)
{
    memset(serial_rx, 0, sizeof(*serial_rx));
}

uint32_t serial_rx_create(serial_rx_t *serial_rx, uint32_t size)
{
    uint32_t result;

    result = 0;
#ifdef RDPLIB_DEBUG
    assert(serial_rx->head == NULL);
#endif

    serial_rx->head = (char *)rdplib_platform_malloc(size);
    if (serial_rx->head == NULL)
    {
        result = 2;
    }
    else
    {
        serial_rx->write_pos = serial_rx->head;
        serial_rx->read_pos = serial_rx->write_pos;
        serial_rx->max_size = size;
    }
    return result;
}

#ifdef RDP_DEAD_CODE

// unused, retained for historical interest
void serial_rx_clear(serial_rx_t *serial_rx)
{
    serial_rx->write_pos = serial_rx->head;
    serial_rx->read_pos = serial_rx->write_pos;
    serial_rx->size = 0;
}

#endif /* RDP_DEAD_CODE */

void serial_rx_destroy(serial_rx_t *serial_rx)
{
    if (serial_rx->head != NULL)
    {
        rdplib_platform_free(serial_rx->head);
        serial_rx->head = NULL;
    }
}

static void serial_rx_move_read_pos(serial_rx_t *serial_rx, uint32_t size)
{
    serial_rx->read_pos += size;
    if (serial_rx->read_pos == serial_rx->head + serial_rx->max_size)
    {
        serial_rx->read_pos = serial_rx->head;
    }

#ifdef RDPLIB_DEBUG
    assert(serial_rx->read_pos < serial_rx->head+serial_rx->max_size);
    assert(serial_rx->size >= size);
#endif
    serial_rx->size -= size;
}

static void serial_rx_move_write_pos(serial_rx_t *serial_rx, uint32_t size)
{
    serial_rx->write_pos += size;
    if (serial_rx->write_pos == serial_rx->head + serial_rx->max_size)
    {
        serial_rx->write_pos = serial_rx->head;
    }

#ifdef RDPLIB_DEBUG
    assert(serial_rx->write_pos < serial_rx->head+serial_rx->max_size);
#endif

    serial_rx->size += size;
#ifdef RDPLIB_DEBUG
    assert(serial_rx->size <= serial_rx->max_size);
#endif
}

static uint32_t serial_rx_bytes_at_write_pos(serial_rx_t *serial_rx)
{
    uint32_t bytes;

    if (serial_rx->write_pos < serial_rx->read_pos)
    {
        return (uint32_t)(serial_rx->read_pos - serial_rx->write_pos);
    }

    bytes = (uint32_t)(serial_rx->head + serial_rx->max_size - serial_rx->write_pos);
    if (serial_rx->size == serial_rx->max_size)
    {
#ifdef RDPLIB_DEBUG
        assert(serial_rx->write_pos == serial_rx->read_pos);
#endif
        bytes = 0;
    }
    return bytes;
}

static uint32_t serial_rx_bytes_at_read_pos(serial_rx_t *serial_rx)
{
    uint32_t bytes;

    if (serial_rx->read_pos < serial_rx->write_pos)
    {
        return (uint32_t)(serial_rx->write_pos - serial_rx->read_pos);
    }

    bytes = (uint32_t)(serial_rx->head + serial_rx->max_size - serial_rx->read_pos);
    if (serial_rx->size == 0)
    {
#ifdef RDPLIB_DEBUG
        assert(serial_rx->write_pos == serial_rx->read_pos);
#endif
        bytes = 0;
    }
    return bytes;
}

void serial_rx_fill(serial_rx_t *serial_rx, void *file)
{
    rdplib_platform_serial_async_t o;
    int ok;
    uint32_t bytes_read;
    uint32_t bytes_avail;
    uint32_t err;

    do
    {
#if !defined(RDPLIB_DEBUG) && !defined(RDPLIB_SOURCE_FAITHFUL)
        // Checked behavior: the recovered ReadFile body did not initialize
        // this output before a failure, so a backend that also leaves it
        // untouched could make the loop compare an indeterminate value.
        bytes_read = 0;
#endif
        bytes_avail = serial_rx_bytes_at_write_pos(serial_rx);
        if (bytes_avail == 0)
        {
            break;
        }

        memset(&o, 0, sizeof(o));
        err = 0;
        ok = rdplib_platform_serial_read((intptr_t)file, &o, serial_rx->write_pos, bytes_avail, &bytes_read, &err);
        if (!ok)
        {
            if (err == ERROR_IO_PENDING)
            {
                uint32_t completion_error = 0;

                ok = rdplib_platform_serial_get_read_result((intptr_t)file, &o, 1, &bytes_read, &completion_error);
                if (!ok)
                {
#ifdef RDPLIB_DEBUG
                    dpf(UINT32_MAX, "ReadFile, GetOverlappedResult returned ERROR: %08x\n", err);
#endif
                    bytes_read = 0;
                }
            }
            else
            {
#ifdef RDPLIB_DEBUG
                dpf(UINT32_MAX, "ReadFile returned ERROR: %08x\n", err);
#endif
            }
        }

        if (ok)
        {
            serial_rx_move_write_pos(serial_rx, bytes_read);
        }
        else
        {
#ifdef RDPLIB_DEBUG
            dpf(UINT32_MAX, "%u readfile %08x\n", time_get_ms(), rdplib_platform_last_system_error());
#elif defined(RDPLIB_SOURCE_FAITHFUL)
            (void)time_get_ms();
            (void)rdplib_platform_last_system_error();
#endif
        }
    }
    while (bytes_read >= bytes_avail);
}

void serial_rx_read(serial_rx_t *serial_rx, void *buffer, uint32_t size)
{
    char *dst;
    uint32_t bytes_avail;

    dst = (char *)buffer;
#ifdef RDPLIB_DEBUG
    assert(size <= serial_rx->size);
#endif

    while (size != 0)
    {
        bytes_avail = serial_rx_bytes_at_read_pos(serial_rx);
#ifdef RDPLIB_DEBUG
        assert(bytes_avail > 0);
#endif
        if (bytes_avail > size)
        {
            bytes_avail = size;
        }

        if (dst != NULL)
        {
            memcpy(dst, serial_rx->read_pos, bytes_avail);
            dst += bytes_avail;
        }
        size -= bytes_avail;
        serial_rx_move_read_pos(serial_rx, bytes_avail);
    }
}

#ifdef RDP_DEAD_CODE

// unused, retained for historical interest
void serial_rx_write(serial_rx_t *serial_rx, void *buffer, uint32_t size)
{
    char *src;
    uint32_t bytes_avail;

    src = (char *)buffer;
#ifdef RDPLIB_DEBUG
    assert(serial_rx->size+size <= serial_rx->max_size);
#endif

    while (size != 0)
    {
        bytes_avail = serial_rx_bytes_at_write_pos(serial_rx);
#ifdef RDPLIB_DEBUG
        assert(bytes_avail > 0);
#endif
        if (bytes_avail > size)
        {
            bytes_avail = size;
        }

        memcpy(serial_rx->write_pos, src, bytes_avail);
        src += bytes_avail;
        size -= bytes_avail;
        serial_rx_move_write_pos(serial_rx, bytes_avail);
    }
}

#endif /* RDP_DEAD_CODE */

void serial_rx_peek(serial_rx_t *serial_rx, void *buffer, uint32_t size)
{
    char *src;
    char *dst;
    uint32_t bytes_avail;

    dst = (char *)buffer;
#ifdef RDPLIB_DEBUG
    assert(size <= serial_rx->size);
#endif

    bytes_avail = serial_rx_bytes_at_read_pos(serial_rx);
#ifdef RDPLIB_DEBUG
    assert(bytes_avail > 0);
#endif

    if (bytes_avail < size)
    {
        memcpy(dst, serial_rx->read_pos, bytes_avail);
        dst += bytes_avail;
        size -= bytes_avail;
        src = serial_rx->head;
    }
    else
    {
        src = serial_rx->read_pos;
    }
    memcpy(dst, src, size);
}
