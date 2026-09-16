// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "serial.h"

#include <string.h>

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "fast.h"
#include "utime.h"

#ifdef RDPLIB_DEBUG
typedef uint16_t uint16;
#ifndef _WIN32
#define ERROR_INVALID_USER_BUFFER PLATFORM_ERROR_INVALID_USER_BUFFER
#define ERROR_NOT_ENOUGH_MEMORY PLATFORM_ERROR_NOT_ENOUGH_MEMORY
#endif
#endif

enum
{
    SERIAL_RECEIVE_CAPACITY = 8000,
    SERIAL_MAX_PAYLOAD = 536,
    SERIAL_INVALID_ARGUMENT = 6,
    SERIAL_CAPACITY_EXCEEDED = 18,
    PLATFORM_ERROR_NOT_ENOUGH_MEMORY = 8,
    PLATFORM_ERROR_IO_INCOMPLETE = 996,
    PLATFORM_ERROR_IO_PENDING = 997,
    PLATFORM_ERROR_INVALID_USER_BUFFER = 1784
};

typedef struct serial_header_t
{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t size;
    uint16_t chksum;
} serial_header_t;

RDP_ASSERT_OFFSET(serial_header_t, src_port, 0x00);
RDP_ASSERT_OFFSET(serial_header_t, dst_port, 0x02);
RDP_ASSERT_OFFSET(serial_header_t, size, 0x04);
RDP_ASSERT_OFFSET(serial_header_t, chksum, 0x06);
RDP_STATIC_ASSERT(sizeof(serial_header_t) == 0x08, "serial_header_t must be 0x08 bytes");

static char s_sync_cookie[18] = "\n\rwho's yo daddy?";

#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)
static uint32_t s_last_retire;
#endif

uint16_t internet_checksum(uint16_t *data, uint16_t len)
{
    uint32_t sum;
    uint16_t checksum;

    sum = 0;
    while (len > 1)
    {
        sum += *data;
        ++data;
        len = (uint16_t)(len - 2);
    }

    if (len == 1)
    {
        sum += *(uint8_t *)data;
    }

    sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    sum += sum >> 16;
    checksum = (uint16_t)~sum;
    return checksum;
}

void serial_init(serial_t *serial)
{
#ifndef _WIN32
    // The host adapter retains rdplib's safe no op behavior for an invalid destroy before create lifecycle; the recovered Win32 record has no such state.
    rdplib_platform_mutex_prepare(&serial->lock.platform);
#ifdef RDPLIB_DEBUG
    serial->lock.owned = 0;
#endif
#endif
    serial->file = (void *)(intptr_t)-1;
    serial->rx_state = 0;
    tx_bufq_init(&serial->tx_in_progress);
    serial_rx_init(&serial->serial_rx);
    memset(&serial->stats, 0, sizeof(serial->stats));
}

uint32_t serial_create(serial_t *serial, uint16_t port)
{
    uint32_t result;

    tx_bufq_create(&serial->tx_in_progress);
    umutex_create(&serial->lock);
    serial->local_port = port;
    result = serial_rx_create(&serial->serial_rx, SERIAL_RECEIVE_CAPACITY);
    if (result != 0)
    {
        serial_destroy(serial);
    }
    return result;
}

static void serial_lock(serial_t *serial)
{
    umutex_lock(&serial->lock);
}

static void serial_unlock(serial_t *serial)
{
    umutex_unlock(&serial->lock);
}

void serial_destroy(serial_t *serial)
{
    serial_lock(serial);
#ifdef _WIN32
    serial_retire_overlapped_writes(serial, 1);
#endif
    serial_rx_destroy(&serial->serial_rx);
    tx_bufq_destroy(&serial->tx_in_progress);
    serial_unlock(serial);
    umutex_destroy(&serial->lock);
}

int32_t serial_recv_from(serial_t *serial, char *buf, uint32_t buf_size, sockaddr_com *scom)
{
    int32_t char_recv;
    char sync[18];
    serial_header_t *header;
    // The May source reserved only 536 bytes including the 8 byte header. All later clients reserve the full header plus the valid 536 byte RDP datagram.
    char scratch[sizeof(serial_header_t) + SERIAL_MAX_PAYLOAD];

    char_recv = -1;
#ifdef RDPLIB_SOURCE_FAITHFUL
    (void)buf_size;
#endif
    serial_lock(serial);
#ifdef _WIN32
    serial_retire_overlapped_writes(serial, 0);
    serial_rx_fill(&serial->serial_rx, serial->file);
#else
#if 0
    // unused, retained for historical interest: both Mac fill variants stopped here.
    Debugger();
    DoWarningDialog("check this code", __FILE__, 300);
#endif
#endif

    while (1)
    {
#ifndef RDPLIB_SOURCE_FAITHFUL
        if (serial->rx_state != 0 && serial->rx_state != 1)
        {
            serial->rx_state = 0;
        }
#endif

        switch (serial->rx_state)
        {
        case 0:
            while (serial->serial_rx.size >= sizeof(sync))
            {
                serial_rx_peek(&serial->serial_rx, sync, (uint32_t)sizeof(sync));
                if (memcmp(sync, s_sync_cookie, sizeof(sync)) == 0)
                {
                    serial_rx_read(&serial->serial_rx, NULL, (uint32_t)sizeof(sync));
                    serial->rx_state = 1;
                    break;
                }

                serial_rx_read(&serial->serial_rx, NULL, 1);
                ++serial->stats.bytes_discarded;
            }

            if (serial->rx_state == 0)
            {
                goto exit;
            }
            /* fall through */

        case 1:
            header = (serial_header_t *)scratch;
            if (serial->serial_rx.size < sizeof(serial_header_t))
            {
                goto exit;
            }

            serial_rx_peek(&serial->serial_rx, header, (uint32_t)sizeof(serial_header_t));
            if (header->size > SERIAL_MAX_PAYLOAD || (header->dst_port != serial->local_port && header->dst_port != 0))
            {
                serial->rx_state = 0;
#ifdef RDPLIB_DEBUG
                dpf(UINT32_C(0x100), "lost sync size:%u, dst_port:%u\n", header->size, header->dst_port);
#endif
                if (header->size > SERIAL_MAX_PAYLOAD)
                {
                    ++serial->stats.bad_header_size;
                }
                else
                {
                    ++serial->stats.wrong_rdp;
                }
                break;
            }

            if (serial->serial_rx.size < (uint32_t)header->size + sizeof(serial_header_t))
            {
                goto exit;
            }

            serial_rx_peek(&serial->serial_rx, scratch, (uint32_t)header->size + sizeof(serial_header_t));
            if (internet_checksum((uint16_t *)scratch, (uint16_t)(header->size + sizeof(serial_header_t))) != 0)
            {
#ifdef RDPLIB_DEBUG
                dpf(UINT32_C(0x100), "lost sync invalid checksum\n");
#endif
                serial->rx_state = 0;
                ++serial->stats.bad_checksum;
                break;
            }

#ifdef RDPLIB_SOURCE_FAITHFUL
            serial->stats.bytes_accepted += (uint32_t)header->size + sizeof(s_sync_cookie) + sizeof(serial_header_t);
#endif
            serial_rx_read(&serial->serial_rx, NULL, (uint32_t)header->size + sizeof(serial_header_t));
#ifdef RDPLIB_DEBUG
            assert(header->size < buf_size);
#endif
#ifndef RDPLIB_SOURCE_FAITHFUL
            if (header->size > buf_size || (header->size != 0 && buf == NULL) || scom == NULL)
            {
                serial->rx_state = 0;
                goto exit;
            }
#endif
            char_recv = header->size;
#ifndef RDPLIB_SOURCE_FAITHFUL
            if (char_recv != 0)
            {
                memcpy(buf, scratch + sizeof(serial_header_t), (size_t)char_recv);
            }
#else
            memcpy(buf, scratch + sizeof(serial_header_t), (size_t)char_recv);
#endif
            memset(scom, 0, sizeof(*scom));
            scom->scom_family = 69;
            scom->scom_port = (int16_t)header->src_port;
#ifndef RDPLIB_SOURCE_FAITHFUL
            serial->stats.bytes_accepted += (uint32_t)header->size + sizeof(s_sync_cookie) + sizeof(serial_header_t);
#endif
            serial->rx_state = 0;
            goto exit;
        }
    }

exit:
    serial_unlock(serial);
    return char_recv;
}

void serial_tx_complete(serial_tx_buf_t *tx_buf)
{
    int ok;

    ok = rdplib_platform_serial_close_event(rdplib_platform_serial_async_get_event(&tx_buf->o));
#ifdef RDPLIB_DEBUG
    assert(ok);
#endif
    (void)ok;
    fast_free(tx_buf);
}

uint32_t serial_tx_write(serial_t *serial, serial_tx_buf_t *tx_buf)
{
    int ok;
    uint32_t bytes_written;
    char *buf;
    uint32_t result;
    uint32_t err;

    result = 0;
    buf = (char *)(tx_buf + 1);
#ifdef RDPLIB_DEBUG
    assert(umutex_owner( &serial->lock ));
#endif

    bytes_written = 0;
    err = 0;
    ok = rdplib_platform_serial_write((intptr_t)serial->file, &tx_buf->o, buf, tx_buf->write_size, &bytes_written, &err);
    if (ok)
    {
        if (bytes_written == tx_buf->write_size)
        {
            ++serial->stats.instant_writes;
        }
        else
        {
            ++serial->stats.partial_writes;
        }
        serial_tx_complete(tx_buf);
    }
    else if (err == PLATFORM_ERROR_IO_PENDING)
    {
        tx_buf->link.item = tx_buf;
        tx_buf->start_time = time_get_ms();
        tx_bufq_add_tail(&serial->tx_in_progress, tx_buf);
    }
    else
    {
        if (err == PLATFORM_ERROR_INVALID_USER_BUFFER || err == PLATFORM_ERROR_NOT_ENOUGH_MEMORY)
        {
            ++serial->stats.try_again;
#ifdef RDPLIB_DEBUG
            assert(err != ERROR_INVALID_USER_BUFFER);
            assert(err != ERROR_NOT_ENOUGH_MEMORY);
#endif
            result = 5;
        }
        else
        {
            ++serial->stats.failed_writes;
            serial->stats.fail_error = err;
            serial->file = (void *)(intptr_t)-1;
            result = 1;
        }
        serial_tx_complete(tx_buf);
    }
    return result;
}

uint32_t serial_send(serial_t *serial, iov_t *iov, uint32_t iov_len, sockaddr_com *scom)
{
    uint32_t iov_size;
    uint32_t i;
    serial_header_t *header;
    char *buf;
    serial_tx_buf_t *tx_buf;
    uint32_t result;
    uint32_t buf_size;

    result = 0;
    iov_size = 0;

#ifndef RDPLIB_SOURCE_FAITHFUL
    if ((iov == NULL && iov_len != 0) || scom == NULL)
    {
        return SERIAL_INVALID_ARGUMENT;
    }
    for (i = 0; i < iov_len; ++i)
    {
        if (iov[i].data == NULL && iov[i].size != 0)
        {
            return SERIAL_INVALID_ARGUMENT;
        }
        if (iov[i].size > SERIAL_MAX_PAYLOAD - iov_size)
        {
            return SERIAL_CAPACITY_EXCEEDED;
        }
        iov_size += iov[i].size;
    }
#endif

    serial_lock(serial);
#ifdef _WIN32
    serial_retire_overlapped_writes(serial, 0);
#endif

#ifdef RDPLIB_SOURCE_FAITHFUL
    for (i = 0; i < iov_len; ++i)
    {
        iov_size += iov[i].size;
    }
#endif

    tx_buf = (serial_tx_buf_t *)fast_malloc(iov_size + (uint32_t)sizeof(serial_tx_buf_t) + (uint32_t)sizeof(s_sync_cookie) + (uint32_t)sizeof(serial_header_t));
#ifdef RDPLIB_DEBUG
    assert(tx_buf != NULL);
#endif
    if (tx_buf == NULL)
    {
        result = 2;
        goto exit;
    }

    memset(&tx_buf->o, 0, sizeof(tx_buf->o));
    rdplib_platform_serial_async_set_event(&tx_buf->o, rdplib_platform_serial_create_event());
#ifdef RDPLIB_DEBUG
#ifdef _WIN32
    assert(tx_buf->o.hEvent != NULL);
#else
    assert(rdplib_platform_serial_async_get_event(&tx_buf->o) != 0);
#endif
#endif
    if (rdplib_platform_serial_async_get_event(&tx_buf->o) == 0)
    {
        result = 3;
        fast_free(tx_buf);
        goto exit;
    }

    buf = (char *)(tx_buf + 1);
    buf_size = 0;
    memcpy(&buf[buf_size], s_sync_cookie, sizeof(s_sync_cookie));
    buf_size += (uint32_t)sizeof(s_sync_cookie);

    header = (serial_header_t *)&buf[buf_size];
    buf_size += (uint32_t)sizeof(serial_header_t);
    header->src_port = serial->local_port;
    header->dst_port = (uint16_t)scom->scom_port;
    header->size = (uint16_t)iov_size;
    header->chksum = 0;

    for (i = 0; i < iov_len; ++i)
    {
#ifndef RDPLIB_SOURCE_FAITHFUL
        if (iov[i].size == 0)
        {
            continue;
        }
#endif
        memcpy(&buf[buf_size], iov[i].data, iov[i].size);
        buf_size += iov[i].size;
    }

    header->chksum = internet_checksum((uint16_t *)header, (uint16_t)(sizeof(serial_header_t) + iov_size));
#ifdef RDPLIB_DEBUG
    assert(0 == internet_checksum( (uint16*)header, (uint16)(sizeof(serial_header_t)+iov_size) ));
#endif

    tx_buf->write_size = iov_size + (uint32_t)sizeof(s_sync_cookie) + (uint32_t)sizeof(serial_header_t);
    result = serial_tx_write(serial, tx_buf);

exit:
    serial_unlock(serial);
    return result;
}

void serial_retire_overlapped_writes(serial_t *serial, int wait)
{
    int ok;
    uint32_t bytes_transferred;
    serial_tx_buf_t *tx_buf;
    uint32_t err;

#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)
    s_last_retire = time_get_ms();
#endif
#ifdef RDPLIB_DEBUG
    assert(umutex_owner( &serial->lock ));
#endif

    while ((tx_buf = tx_bufq_peek_head(&serial->tx_in_progress)) != NULL)
    {
        bytes_transferred = 0;
        err = 0;
        ok = rdplib_platform_serial_get_write_result((intptr_t)serial->file, &tx_buf->o, wait, &bytes_transferred, &err);
        if (ok)
        {
            if (bytes_transferred == tx_buf->write_size)
            {
                ++serial->stats.complete_writes;
            }
            else
            {
                ++serial->stats.incomplete_writes;
            }

            tx_bufq_remove_head(&serial->tx_in_progress);
            serial_tx_complete(tx_buf);
        }
        else
        {
            if (err == PLATFORM_ERROR_IO_INCOMPLETE)
            {
                break;
            }

            ++serial->stats.failed_writes;
            tx_bufq_remove_head(&serial->tx_in_progress);
            serial_tx_complete(tx_buf);
        }
    }
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
int32_t serial_port_create(const char *filename)
{
#ifdef _WIN32
    void *file;
    int ok;
    DCB dcb;
    uint32_t err;

    file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        ok = GetCommState(file, &dcb) != FALSE;
        if (!ok)
        {
            err = rdplib_platform_last_system_error();
            (void)err;
#ifdef RDPLIB_DEBUG
            assert(ok);
#endif
            ok = CloseHandle(file) != FALSE;
            (void)ok;
            file = INVALID_HANDLE_VALUE;
        }
        else
        {
            dcb.BaudRate = CBR_57600;
            dcb.fParity = FALSE;
            dcb.Parity = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            dcb.ByteSize = 8;
            ok = SetCommState(file, &dcb) != FALSE;
#ifdef RDPLIB_DEBUG
            assert(ok);
#endif
            if (!ok)
            {
                ok = CloseHandle(file) != FALSE;
                (void)ok;
                file = INVALID_HANDLE_VALUE;
            }
        }
    }
    return (int32_t)(intptr_t)file;
#else
    (void)filename;
    return -1;
#endif
}
#endif

void serial_set_time_next_recv(serial_t *serial, uint32_t time_next_recv)
{
    serial_lock(serial);
    serial->time_next_recv = time_next_recv;
    serial_unlock(serial);
}

uint32_t serial_get_time_next_recv(serial_t *serial)
{
    uint32_t time_next_recv;

    serial_lock(serial);
    time_next_recv = serial->time_next_recv;
    serial_unlock(serial);
    return time_next_recv;
}

uint32_t serial_tx_ready(serial_t *serial)
{
    uint32_t overlapped_writes_in_progress;

    serial_lock(serial);
    overlapped_writes_in_progress = tx_bufq_get_size(&serial->tx_in_progress);
    serial_unlock(serial);
#ifdef RDPLIB_DEBUG
    dpf(UINT32_C(0x100), "in progress: %u\n", overlapped_writes_in_progress);
#endif
    return overlapped_writes_in_progress < 2;
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void serial_get_stats(serial_t *serial, serial_stat_t *stats)
{
    serial_tx_buf_t *head;

    serial_lock(serial);
    memcpy(stats, &serial->stats, sizeof(*stats));
    stats->tx_operations = tx_bufq_get_size(&serial->tx_in_progress);
    stats->tx_bytes = tx_bufq_get_bytes(&serial->tx_in_progress);
    if (stats->tx_operations)
    {
        head = tx_bufq_peek_head(&serial->tx_in_progress);
#ifdef RDPLIB_DEBUG
        assert(head != NULL);
#endif
        stats->tx_age = time_get_ms() - head->start_time;
    }
    else
    {
        stats->tx_age = 0;
    }
    stats->time_since_retire = time_get_ms() - s_last_retire;
    serial_unlock(serial);
}
#endif

uint32_t serial_get_time_empty(serial_t *serial)
{
    uint32_t bytes;
    uint32_t empty;
    serial_tx_buf_t *head;

    serial_lock(serial);
    bytes = tx_bufq_get_bytes(&serial->tx_in_progress);
    if (bytes)
    {
        head = tx_bufq_peek_head(&serial->tx_in_progress);
#ifdef RDPLIB_DEBUG
        assert(head != NULL);
#endif
#ifdef RDPLIB_SOURCE_FAITHFUL
        bytes -= head->write_size;
#else
        if (head != NULL && bytes >= head->write_size)
        {
            bytes -= head->write_size;
        }
        else
        {
            bytes = 0;
        }
#endif
#ifdef RDPLIB_DEBUG
        assert(bytes < 65000);
#endif
    }
    empty = (bytes * UINT32_C(9000)) / UINT32_C(1000000);
    serial_unlock(serial);

#ifdef RDPLIB_DEBUG
    dpf(UINT32_C(0x100), "[%u] in progress: %u bytes: %u empty: %ums\n", time_get_ms(), tx_bufq_get_size(&serial->tx_in_progress), bytes, empty);
#elif defined(RDPLIB_SOURCE_FAITHFUL)
    (void)time_get_ms();
#endif
    return time_get_ms() + empty;
}

uint32_t serial_get_stall_time(serial_t *serial)
{
    uint32_t stall;
    uint32_t tx_time;
    serial_tx_buf_t *head;

    stall = 0;
    tx_time = 0;
    serial_lock(serial);
    head = tx_bufq_peek_head(&serial->tx_in_progress);
    if (head)
    {
        tx_time = 2u * ((head->write_size * UINT32_C(9000)) / UINT32_C(1000000));
        stall = time_get_ms() - head->start_time;
        if (stall <= tx_time)
        {
            stall = 0;
        }
        else
        {
            stall -= tx_time;
        }
    }
    serial_unlock(serial);
    return stall;
}
