// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_SERIAL_H
#define RDP_SERIAL_H

#include <stdint.h>

#include "iov.h"
#include "layout.h"
#include "serial_rx.h"
#include "tx_bufq.h"
#include "umutex.h"

typedef struct sockaddr_com
{
    int16_t scom_family;
    int16_t scom_port;
    char scom_zero[12];
} sockaddr_com;

typedef struct serial_stat_t
{
    uint32_t time_since_retire;
    uint32_t tx_operations;
    uint32_t tx_bytes;
    uint32_t tx_age;
    uint32_t bad_header_size;
    uint32_t bad_checksum;
    uint32_t wrong_rdp;
    uint32_t bytes_accepted;
    uint32_t bytes_discarded;
    uint32_t incomplete_writes;
    uint32_t complete_writes;
    uint32_t instant_writes;
    uint32_t failed_writes;
    uint32_t fail_error;
    uint32_t partial_writes;
    uint32_t try_again;
} serial_stat_t;

typedef struct serial_t
{
    umutex_t lock;
    uint32_t time_next_recv;
    void *file;
    uint16_t local_port;
    serial_rx_t serial_rx;
    tx_bufq_t tx_in_progress;
    serial_stat_t stats;
    int rx_state;
} serial_t;

RDP_ASSERT_OFFSET(sockaddr_com, scom_family, 0x00);
RDP_ASSERT_OFFSET(sockaddr_com, scom_port, 0x02);
RDP_ASSERT_OFFSET(sockaddr_com, scom_zero, 0x04);
RDP_STATIC_ASSERT(sizeof(sockaddr_com) == 0x10, "sockaddr_com must be 0x10 bytes");

RDP_ASSERT_OFFSET(serial_stat_t, time_since_retire, 0x00);
RDP_ASSERT_OFFSET(serial_stat_t, tx_operations, 0x04);
RDP_ASSERT_OFFSET(serial_stat_t, tx_bytes, 0x08);
RDP_ASSERT_OFFSET(serial_stat_t, tx_age, 0x0C);
RDP_ASSERT_OFFSET(serial_stat_t, bad_header_size, 0x10);
RDP_ASSERT_OFFSET(serial_stat_t, bad_checksum, 0x14);
RDP_ASSERT_OFFSET(serial_stat_t, wrong_rdp, 0x18);
RDP_ASSERT_OFFSET(serial_stat_t, bytes_accepted, 0x1C);
RDP_ASSERT_OFFSET(serial_stat_t, bytes_discarded, 0x20);
RDP_ASSERT_OFFSET(serial_stat_t, incomplete_writes, 0x24);
RDP_ASSERT_OFFSET(serial_stat_t, complete_writes, 0x28);
RDP_ASSERT_OFFSET(serial_stat_t, instant_writes, 0x2C);
RDP_ASSERT_OFFSET(serial_stat_t, failed_writes, 0x30);
RDP_ASSERT_OFFSET(serial_stat_t, fail_error, 0x34);
RDP_ASSERT_OFFSET(serial_stat_t, partial_writes, 0x38);
RDP_ASSERT_OFFSET(serial_stat_t, try_again, 0x3C);
RDP_STATIC_ASSERT(sizeof(serial_stat_t) == 0x40, "serial_stat_t must be 0x40 bytes");

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(serial_t, lock, 0x00);
RDP_ASSERT_OFFSET(serial_t, time_next_recv, 0x18 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, file, 0x1C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, local_port, 0x20 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, serial_rx, 0x24 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, tx_in_progress, 0x38 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, stats, 0x50 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(serial_t, rx_state, 0x90 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_STATIC_ASSERT(sizeof(serial_t) == 0x94 + RDP_WIN32_UMUTEX_OWNER_BYTES, "serial_t has the expected Win32 layout");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

uint16_t internet_checksum(uint16_t *data, uint16_t len);

// Initializes only source owned fields. serial_create owns lock/list creation and publishes local_port; the scheduler separately initializes time_next_recv.
void serial_init(serial_t *serial);

// Creates the transmit queue and lock and allocates the fixed receive ring. Returns 2 only when receive allocation fails, after unwinding partial ownership.
uint32_t serial_create(serial_t *serial, uint16_t port);

// On Windows, drains pending writes before releasing the receive ring, non owning transmit queue, and lock. The Mac transport variant has no completion drain.
void serial_destroy(serial_t *serial);

// Parses at most one complete frame and returns its payload size, or -1 when no deliverable frame remains. The checked build consumes a complete frame that exceeds buf_size;
// source faithful mode retains the valid caller precondition.
int32_t serial_recv_from(serial_t *serial, char *buf, uint32_t buf_size, sockaddr_com *scom);

// Closes the write event and consumes the fast allocation.
void serial_tx_complete(serial_tx_buf_t *tx_buf);

// Starts an asynchronous write and always consumes tx_buf ownership: immediate completion/error frees it, while ERROR_IO_PENDING transfers it to tx_in_progress.
// The caller must hold serial->lock. Returns 0, 1, or 5.
uint32_t serial_tx_write(serial_t *serial, serial_tx_buf_t *tx_buf);

// Builds the native endian cookie/header/vector frame and passes its allocation to serial_tx_write. The checked build validates pointers and the 536 byte protocol ceiling;
// source faithful mode retains wrapping and truncating caller preconditions. Returns 0, 1, 2, 3, or 5, plus checked only 6 and 18.
uint32_t serial_send(serial_t *serial, iov_t *iov, uint32_t iov_len, sockaddr_com *scom);

// Retires only the completed queue prefix. ERROR_IO_INCOMPLETE leaves the head in place; every other completion failure consumes it without changing fail_error. The caller must hold serial->lock.
void serial_retire_overlapped_writes(serial_t *serial, int wait);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
int32_t serial_port_create(const char *filename);
#endif

void serial_set_time_next_recv(serial_t *serial, uint32_t time_next_recv);
uint32_t serial_get_time_next_recv(serial_t *serial);
uint32_t serial_tx_ready(serial_t *serial);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void serial_get_stats(serial_t *serial, serial_stat_t *stats);
#endif

// Predicts when bytes queued behind the active write will empty. Queue membership and byte accounting must agree; the checked build safely handles a broken invariant.
uint32_t serial_get_time_empty(serial_t *serial);

// Returns the active write's age beyond twice its estimated transmit duration, or 0.
uint32_t serial_get_stall_time(serial_t *serial);

#ifdef __cplusplus
}
#endif

#endif /* RDP_SERIAL_H */
