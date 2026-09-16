// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Raw interface to the recovered RDP implementation. Use rdplib.h unless an
// application needs to manage the transport owner directly.
#ifndef RDPLIB_RAW_H
#define RDPLIB_RAW_H

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <stdint.h>

#include "arrival_fragid.h"
#include "bandwidth.h"
#include "bitops.h"
#include "cmp.h"
#include "connection.h"
#include "connhash.h"
#include "crc.h"
#include "cypher.h"
#include "event.h"
#include "eventq.h"
#include "fast.h"
#include "iov.h"
#include "layout.h"
#include "list.h"
#include "log.h"
#include "msg_arrival.h"
#include "msg_outgoing.h"
#include "packet.h"
#include "pqueue.h"
#include "rdplib_constants.h"
#include "rdplib_platform.h"
#include "rdpstat.h"
#include "rx.h"
#include "rxq.h"
#include "sequencer.h"
#include "serial.h"
#include "serial_rx.h"
#include "serial_tx.h"
#include "stats.h"
#include "timeout.h"
#include "trace.h"
#include "tx.h"
#include "tx_bufq.h"
#include "txq.h"
#include "uevent.h"
#include "umutex.h"
#include "usend.h"
#include "usemaphore.h"
#include "ustrerror.h"
#include "uthread.h"
#include "utime.h"

#ifdef _WIN32
#include <wsipx.h>
#else
struct sockaddr_ipx
{
    int16_t sa_family;
    uint8_t sa_netnum[4];
    uint8_t sa_nodenum[6];
    uint16_t sa_socket;
};
#endif

typedef struct rdp_t
{
    uint32_t startup;
    intptr_t udp_socket;
    intptr_t icmp_socket;
    intptr_t trace_socket;
    intptr_t ipx_socket;
    struct sockaddr_in local_udp_addr;
    struct sockaddr_in trace_local_addr;
    struct sockaddr_ipx local_ipx_addr;
    uint32_t udp_socket_ttl;
    uint32_t ipx_broadcast;
    uint32_t udp_broadcast;
    connhash_t addr_map;
    eventq_t conn_eventq;
    uint32_t wake_sent;
    uint32_t app_is_waiting_for_exit;
    uint32_t io_thread_running;
    uthread_t io_thread;
    usemaphore_t receive_semaphore;
    rxq_t message_rxq;
    umutex_t message_rxq_mutex;
    rxq_t external_rxq;
    umutex_t external_rxq_mutex;
    uint32_t bytes_recvd;
    uint32_t duplicate_bytes_recvd;
    uint32_t last_sample;
    uint32_t bytes_per_second;
    uint32_t duplicate_bytes_per_second;
    uint32_t encrypt;
    uint32_t crc;
    serial_t serial;
#ifndef RDPLIB_SOURCE_FAITHFUL
    void (*rdplib_arrival_ready_callback)(void *context);
    void *rdplib_arrival_ready_context;
#endif
} rdp_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(rdp_t, udp_socket, 0x004);
RDP_ASSERT_OFFSET(rdp_t, icmp_socket, 0x008);
RDP_ASSERT_OFFSET(rdp_t, trace_socket, 0x00C);
RDP_ASSERT_OFFSET(rdp_t, ipx_socket, 0x010);
RDP_ASSERT_OFFSET(rdp_t, local_udp_addr, 0x014);
RDP_ASSERT_OFFSET(rdp_t, trace_local_addr, 0x024);
RDP_ASSERT_OFFSET(rdp_t, local_ipx_addr, 0x034);
RDP_ASSERT_OFFSET(rdp_t, udp_socket_ttl, 0x044);
RDP_ASSERT_OFFSET(rdp_t, ipx_broadcast, 0x048);
RDP_ASSERT_OFFSET(rdp_t, udp_broadcast, 0x04C);
RDP_ASSERT_OFFSET(rdp_t, addr_map, 0x050);
RDP_ASSERT_OFFSET(rdp_t, conn_eventq, 0x058);
RDP_ASSERT_OFFSET(rdp_t, wake_sent, 0x084 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, app_is_waiting_for_exit, 0x088 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, io_thread_running, 0x08C + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, io_thread, 0x090 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, receive_semaphore, 0x0A0 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, message_rxq, 0x0A4 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, message_rxq_mutex, 0x0B8 + RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, external_rxq, 0x0D0 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, external_rxq_mutex, 0x0E4 + 2u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, bytes_recvd, 0x0FC + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, duplicate_bytes_recvd, 0x100 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, last_sample, 0x104 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, bytes_per_second, 0x108 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, duplicate_bytes_per_second, 0x10C + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, encrypt, 0x110 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, crc, 0x114 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, serial, 0x118 + 3u * RDP_WIN32_UMUTEX_OWNER_BYTES);
#ifdef RDPLIB_SOURCE_FAITHFUL
RDP_STATIC_ASSERT(sizeof(rdp_t) == 0x1AC + 4u * RDP_WIN32_UMUTEX_OWNER_BYTES, "rdp_t has the expected Win32 layout");
#else
RDP_ASSERT_OFFSET(rdp_t, rdplib_arrival_ready_callback, 0x1AC + 4u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_ASSERT_OFFSET(rdp_t, rdplib_arrival_ready_context, 0x1B0 + 4u * RDP_WIN32_UMUTEX_OWNER_BYTES);
RDP_STATIC_ASSERT(sizeof(rdp_t) == 0x1B4 + 4u * RDP_WIN32_UMUTEX_OWNER_BYTES, "rdp_t has the expected maintained Win32 layout");
#endif
#endif

extern uint16_t g_next_local_port;

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t rdp_wake(rdp_t *rdp, uint32_t msg);
void rdp_resort(connection_t *c, uint32_t wakeup_iothread);
void rdp_unlock(connection_t *c);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void rdp_set_socket_rcvbuf(rdp_t *rdp, int size);
void rdp_set_socket_sndbuf(rdp_t *rdp, int size);
int rdp_get_socket_sndbuf(rdp_t *rdp);
#endif

void rdp_enqueue_arrival(rdp_t *rdp, msg_arrival_t *arrival);
void rdp_handle_complete_arrival(rdp_t *rdp, connection_t *c, msg_arrival_t *arrival);
void rdp_handle_connectionless(rdp_t *rdp, const char *data, uint32_t size, struct sockaddr *remote_addr);
int32_t rdp_verify_crc(char *buffer, int32_t buffer_size);
int32_t rdp_decode_data(char *scratch, int32_t char_recv);
uint32_t rdp_handle_data_recv(rdp_t *rdp, char *scratch, int32_t char_recv, struct sockaddr *remote_addr);
void rdp_handle_icmp_recv(rdp_t *rdp, char *scratch, int32_t char_recv, struct sockaddr_in *remote_addr);
void rdp_serial_drain(rdp_t *rdp);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t rdp_attach(rdp_t *rdp, int32_t file);
#endif

void rdp_destroy_internal(rdp_t *rdp);
void rdp_io_thread(void *data);
void rdp_enqueue_disconnect_msg(rdp_t *rdp, connection_t *c);
void rdp_init(rdp_t *rdp);
uint32_t rdp_create(rdp_t **out_rdp, uint16_t local_port, uint32_t connections, uint32_t flags);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void rdp_shutdown(uint32_t linger_time);
#endif

void rdp_destroy(rdp_t *rdp, int wait);
uint32_t rdp_connect_sa(rdp_t *rdp, connection_t **new_c, struct sockaddr *remote_addr, uint32_t flags);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t rdp_send(rdp_t *rdp, struct sockaddr *remote_addr, uint16_t port, const char *data, uint32_t size);
uint32_t rdp_send_oversized(rdp_t *rdp, struct sockaddr *remote_addr, uint16_t port, const char *data, uint32_t size);
#endif

#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
void format_sockaddr(char *buf, struct sockaddr *sa);
#endif
uint32_t rdp_connection_create_internal(rdp_t *rdp, connection_t **new_c, struct sockaddr *remote_addr, uint32_t flags);
uint32_t rdp_connect(rdp_t *rdp, connection_t **new_c, char *hostname, uint16_t port, uint32_t flags);
void rdp_connection_mark_for_delete(rdp_t *rdp, connection_t *c);
uint32_t connection_close_wait(connection_t *c, uint32_t linger_time, uint32_t *all_acked);
msg_arrival_t *rdp_receive(rdp_t *rdp, uint32_t timeout);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t rdp_trace_capable(rdp_t *rdp);
struct sockaddr *rdp_get_local_addr(rdp_t *rdp, int16_t family);
uint32_t rdp_get_transport_mask(void);
#endif

uint32_t rdp_get_input_rate(rdp_t *rdp);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t rdp_get_duplicate_input_rate(rdp_t *rdp);
#endif

uint32_t rdp_serial_tx_ready(rdp_t *rdp);
uint32_t rdp_serial_get_time_empty(rdp_t *rdp);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void rdp_get_serial_stats(rdp_t *rdp, serial_stat_t *stats);
#endif

uint32_t rdp_get_serial_stall_time(rdp_t *rdp);

#ifdef __cplusplus
}
#endif

static connection_t *rdp_lock_addr(rdp_t *rdp, struct sockaddr *sa)
{
    connection_t *c;

    c = connhash_lock(&rdp->addr_map, sa);
#ifndef RDPLIB_SOURCE_FAITHFUL
    if (c && c->cn_abort)
    {
        rdp_unlock(c);
        c = NULL;
    }
#endif
    return c;
}

static connection_t *rdp_lock_connection(connection_t *c)
{
    c = connhash_lock(&c->cn_rdp->addr_map, &c->tx_remote_addr);
#ifdef RDPLIB_DEBUG
    assert(c != NULL);
#endif
    return c;
}

#endif /* RDPLIB_RAW_H */
