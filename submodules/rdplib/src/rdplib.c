// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "rdp.h"
#include "rdplib_rdp.h"

struct rdplib_runtime_t
{
    rdplib_platform_mutex_t lock;
    rdp_stat statistics;
    uint32_t endpoint_count;
    uint32_t outstanding_messages;
};

struct rdplib_message_t
{
    struct rdplib_message_t *next;
    rdplib_runtime_t *runtime;
    msg_arrival_t *arrival;
    uint8_t sender_address[16];
    uint32_t payload_bytes;
    uint16_t flags;
    uint8_t stream_id;
    uint8_t disconnect;
};

struct rdplib_connection_t
{
    struct rdplib_connection_t *accept_next;
    rdplib_endpoint_t *endpoint;
    connection_t *raw;
    rdplib_platform_mutex_t send_lock;
    rdplib_message_t *message_head;
    rdplib_message_t *message_tail;
    uint8_t remote_address[16];
    uint8_t application_owned;
    uint8_t close_started;
    uint8_t peer_fin;
    uint8_t disconnected;
};

struct rdplib_endpoint_t
{
    rdplib_runtime_t *runtime;
    rdp_t *raw;
    uint32_t application_connection_count;
    rdplib_connection_t *accept_head;
    rdplib_connection_t *accept_tail;
    rdplib_message_t *connectionless_head;
    rdplib_message_t *connectionless_tail;
};

static rdplib_runtime_t *rdplib_active_runtime;

static void rdplib_runtime_add_outstanding(rdplib_runtime_t *runtime)
{
    rdplib_platform_mutex_lock(&runtime->lock);
    ++runtime->outstanding_messages;
    rdplib_platform_mutex_unlock(&runtime->lock);
}

static void rdplib_runtime_remove_outstanding(rdplib_runtime_t *runtime)
{
    rdplib_platform_mutex_lock(&runtime->lock);
    assert(runtime->outstanding_messages != 0);
    --runtime->outstanding_messages;
    rdplib_platform_mutex_unlock(&runtime->lock);
}

static void rdplib_connection_destroy_handle(rdplib_connection_t *connection)
{
    assert(connection->raw == NULL);
    assert(connection->message_head == NULL);
    assert(connection->message_tail == NULL);
    assert(connection->accept_next == NULL);
    assert(connection->endpoint == NULL);
    assert(!connection->application_owned);
    rdplib_platform_mutex_destroy(&connection->send_lock);
    rdplib_platform_free(connection);
}

static void rdplib_connection_clear_packet_drop_callback(connection_t *raw)
{
#ifndef RDPLIB_SOURCE_FAITHFUL
    umutex_lock(&raw->cn_lock);
    raw->rdplib_packet_drop_callback = NULL;
    raw->rdplib_packet_drop_context = NULL;
    umutex_unlock(&raw->cn_lock);
#else
    (void)raw;
#endif
}

static rdplib_connection_t *rdplib_connection_from_raw(connection_t *raw)
{
    rdplib_connection_t *connection;

    assert(raw != NULL);
    connection = (rdplib_connection_t *)connection_app_ptr(raw)[0];
    assert(!connection || connection->raw == raw);
    return connection;
}

static void rdplib_connection_attach_raw(rdplib_connection_t *connection, connection_t *raw)
{
    assert(connection != NULL);
    assert(raw != NULL);
    assert(connection->raw == NULL);
    assert(connection_app_ptr(raw)[0] == NULL);

    connection->raw = raw;
    connection_app_ptr(raw)[0] = connection;
}

static connection_t *rdplib_connection_detach_raw(rdplib_connection_t *connection)
{
    connection_t *raw;

    assert(connection != NULL);
    raw = connection->raw;
    if (raw)
    {
        assert(connection_app_ptr(raw)[0] == connection);
        connection_app_ptr(raw)[0] = NULL;
        connection->raw = NULL;
    }
    return raw;
}

void rdplib_connection_release(rdplib_connection_t *connection)
{
    rdplib_endpoint_t *endpoint;
    connection_t *raw;

    if (!connection)
    {
        return;
    }

    assert(connection->application_owned);
    assert(connection->message_head == NULL);
    endpoint = connection->endpoint;
    assert(endpoint != NULL);
    assert(endpoint->application_connection_count != 0);

    rdplib_platform_mutex_lock(&connection->send_lock);
    assert(!connection->raw || connection->close_started || connection->peer_fin || connection->disconnected);
    raw = rdplib_connection_detach_raw(connection);
    if (raw)
    {
        // A terminal peer state permits release without a prior explicit close.
        assert(!connection->close_started);
        connection->close_started = 1;
        rdplib_connection_clear_packet_drop_callback(raw);
        (void)connection_close(raw, 0, NULL, NULL);
    }
    rdplib_platform_mutex_unlock(&connection->send_lock);

    --endpoint->application_connection_count;
    connection->application_owned = 0;
    connection->endpoint = NULL;
    rdplib_connection_destroy_handle(connection);
}

static rdplib_connection_t *rdplib_create_connection_handle(rdplib_endpoint_t *endpoint, connection_t *borrowed, int accept_pending)
{
    rdplib_connection_t *connection;

    if (!borrowed)
    {
        return NULL;
    }

    connection = (rdplib_connection_t *)rdplib_platform_malloc(sizeof(*connection));
    if (!connection)
    {
        return NULL;
    }

    memset(connection, 0, sizeof(*connection));
    rdplib_platform_mutex_prepare(&connection->send_lock);
    rdplib_platform_mutex_init(&connection->send_lock);
    connection->endpoint = endpoint;
    connection->application_owned = (uint8_t)(accept_pending == 0);
    memcpy(connection->remote_address, &borrowed->tx_remote_addr, sizeof(connection->remote_address));
    rdplib_connection_attach_raw(connection, borrowed);

    if (!accept_pending)
    {
        ++endpoint->application_connection_count;
    }

    if (accept_pending)
    {
        if (endpoint->accept_tail)
        {
            endpoint->accept_tail->accept_next = connection;
        }
        else
        {
            endpoint->accept_head = connection;
        }
        endpoint->accept_tail = connection;
    }
    return connection;
}

static rdplib_message_t *rdplib_wrap_message(rdplib_endpoint_t *endpoint, msg_arrival_t *arrival)
{
    rdplib_message_t *message = (rdplib_message_t *)rdplib_platform_malloc(sizeof(*message));

    if (!message)
    {
        fast_free(arrival);
        return NULL;
    }

    memset(message, 0, sizeof(*message));
    message->runtime = endpoint->runtime;
    // The caller owns the original fast allocation.  The payload is not copied.
    message->arrival = arrival;
    message->payload_bytes = arrival->size;
    message->flags = arrival->options;
    message->stream_id = arrival->stream;
    message->disconnect = (uint8_t)(arrival->sender != NULL && arrival->size == 0 && (arrival->options & RDP_FLAG_FIN) == 0);
    if (!arrival->sender)
    {
        memcpy(message->sender_address, &arrival->from, sizeof(message->sender_address));
    }

    rdplib_runtime_add_outstanding(endpoint->runtime);
    return message;
}

static void rdplib_queue_connection_message(rdplib_connection_t *connection, rdplib_message_t *message)
{
    int has_fin;
    int is_disconnect;

    has_fin = rdplib_message_has_fin(message);
    is_disconnect = rdplib_message_is_disconnect(message);
    if (connection->message_tail)
    {
        connection->message_tail->next = message;
    }
    else
    {
        connection->message_head = message;
    }
    connection->message_tail = message;

    if (has_fin || is_disconnect)
    {
        rdplib_platform_mutex_lock(&connection->send_lock);
        if (has_fin)
        {
            connection->peer_fin = 1;
        }
        if (is_disconnect)
        {
            connection->disconnected = 1;
        }
        rdplib_platform_mutex_unlock(&connection->send_lock);
    }
}

static void rdplib_queue_connectionless(rdplib_endpoint_t *endpoint, rdplib_message_t *message)
{
    if (endpoint->connectionless_tail)
    {
        endpoint->connectionless_tail->next = message;
    }
    else
    {
        endpoint->connectionless_head = message;
    }
    endpoint->connectionless_tail = message;
}

static int rdplib_process_arrival(rdplib_endpoint_t *endpoint, msg_arrival_t *arrival)
{
    connection_t *raw = arrival->sender;
    rdplib_connection_t *connection = NULL;
    rdplib_message_t *message;

    if (raw)
    {
        int create_for_arrival = arrival->size != 0 || (arrival->options & RDP_FLAG_FIN) != 0;

        connection = rdplib_connection_from_raw(raw);
        assert(!connection || connection->endpoint == endpoint);
        if (!connection && create_for_arrival)
        {
            connection = rdplib_create_connection_handle(endpoint, raw, 1);
        }
        if (!connection)
        {
            fast_free(arrival);
            return create_for_arrival ? RDPLIB_ERROR_OUT_OF_MEMORY : RDPLIB_OK;
        }
    }

    message = rdplib_wrap_message(endpoint, arrival);
    if (!message)
    {
        return RDPLIB_ERROR_OUT_OF_MEMORY;
    }

    if (connection)
    {
        rdplib_queue_connection_message(connection, message);
    }
    else
    {
        rdplib_queue_connectionless(endpoint, message);
    }
    return RDPLIB_OK;
}

int rdplib_runtime_create(rdplib_runtime_t **output, uint32_t fast_allocator_bytes)
{
    rdplib_runtime_t *runtime;

    if (!output || fast_allocator_bytes == 0 || rdplib_active_runtime)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;

    runtime = (rdplib_runtime_t *)rdplib_platform_malloc(sizeof(*runtime));
    if (!runtime)
    {
        return RDPLIB_ERROR_OUT_OF_MEMORY;
    }

    memset(runtime, 0, sizeof(*runtime));
    rdplib_platform_mutex_prepare(&runtime->lock);
    rdplib_platform_mutex_init(&runtime->lock);
    fast_malloc_init(fast_allocator_bytes);
    g_rdp_stat = &runtime->statistics;
    rdplib_discard_global_statistics(1);
    rdplib_active_runtime = runtime;
    *output = runtime;
    return RDPLIB_OK;
}

int rdplib_runtime_destroy(rdplib_runtime_t *runtime)
{
    uint32_t endpoint_count;
    uint32_t outstanding_messages;

    if (!runtime || runtime != rdplib_active_runtime)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }

    rdplib_platform_mutex_lock(&runtime->lock);
    endpoint_count = runtime->endpoint_count;
    outstanding_messages = runtime->outstanding_messages;
    rdplib_platform_mutex_unlock(&runtime->lock);
    assert(endpoint_count == 0 && outstanding_messages == 0);
    if (endpoint_count != 0 || outstanding_messages != 0)
    {
        return RDPLIB_ERROR_BUSY;
    }

    rdplib_active_runtime = NULL;
    rdplib_discard_global_statistics(0);
#ifdef RDP_DEAD_CODE
    g_rdp_stat = &rdp_stat_struct;
#else
    g_rdp_stat = NULL;
#endif
    fast_malloc_destroy();
    rdplib_platform_mutex_destroy(&runtime->lock);
    rdplib_platform_free(runtime);
    return RDPLIB_OK;
}

int rdplib_endpoint_create(rdplib_runtime_t *runtime, rdplib_endpoint_t **output, uint16_t local_port, uint32_t expected_connections, uint32_t flags)
{
    return rdplib_endpoint_create_ex(runtime, output, local_port, expected_connections, flags, NULL);
}

int rdplib_endpoint_create_ex(rdplib_runtime_t *runtime, rdplib_endpoint_t **output, uint16_t local_port, uint32_t expected_connections, uint32_t flags, const rdplib_endpoint_options_t *options)
{
    rdplib_endpoint_t *endpoint;
    uint32_t receive_socket_buffer_bytes = 0;
    uint32_t send_socket_buffer_bytes = 0;
    rdplib_arrival_ready_callback_t arrival_ready_callback = NULL;
    void *arrival_ready_context = NULL;
    int result;

    if (!runtime || runtime != rdplib_active_runtime || !output || (flags & ~(RDPLIB_USE_CRC | RDPLIB_USE_ENCRYPTION)) != 0)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;

    if (options)
    {
        if (options->structure_size < sizeof(*options) ||
            options->receive_socket_buffer_bytes > (uint32_t)INT_MAX ||
            options->send_socket_buffer_bytes > (uint32_t)INT_MAX)
        {
            return RDPLIB_ERROR_INVALID_ARGUMENT;
        }
        receive_socket_buffer_bytes = options->receive_socket_buffer_bytes;
        send_socket_buffer_bytes = options->send_socket_buffer_bytes;
        arrival_ready_callback = options->arrival_ready_callback;
        arrival_ready_context = options->arrival_ready_context;
    }

#ifdef RDPLIB_SOURCE_FAITHFUL
    if (arrival_ready_callback)
    {
        return RDPLIB_ERROR_NOT_SUPPORTED;
    }
#endif

    endpoint = (rdplib_endpoint_t *)rdplib_platform_malloc(sizeof(*endpoint));
    if (!endpoint)
    {
        return RDPLIB_ERROR_OUT_OF_MEMORY;
    }
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->runtime = runtime;

    result = (int)rdplib_rdp_create(&endpoint->raw, local_port, expected_connections, flags | RDP_CREATE_REQUIRE_IPV4,
                                  receive_socket_buffer_bytes, send_socket_buffer_bytes, arrival_ready_callback, arrival_ready_context);
    if (result != 0)
    {
        rdplib_platform_free(endpoint);
        return result;
    }

    rdplib_platform_mutex_lock(&runtime->lock);
    ++runtime->endpoint_count;
    rdplib_platform_mutex_unlock(&runtime->lock);
    *output = endpoint;
    return RDPLIB_OK;
}

static void rdplib_detach_connection(rdplib_connection_t *connection)
{
    rdplib_message_t *message;
    rdplib_message_t *next;

    assert(!connection->application_owned);
    (void)rdplib_connection_detach_raw(connection);
    connection->endpoint = NULL;
    message = connection->message_head;
    connection->message_head = NULL;
    connection->message_tail = NULL;

    for (; message; message = next)
    {
        next = message->next;
        message->next = NULL;
        rdplib_message_release(message);
    }
    rdplib_connection_destroy_handle(connection);
}

int rdplib_endpoint_destroy(rdplib_endpoint_t *endpoint)
{
    rdplib_connection_t *connection;
    rdplib_message_t *message;
    rdplib_message_t *next_message;
    rdplib_runtime_t *runtime;

    if (!endpoint)
    {
        return RDPLIB_OK;
    }
    if (endpoint->application_connection_count != 0)
    {
        return RDPLIB_ERROR_BUSY;
    }

    runtime = endpoint->runtime;
    assert((endpoint->accept_head == NULL) == (endpoint->accept_tail == NULL));
    while ((connection = endpoint->accept_head) != NULL)
    {
        endpoint->accept_head = connection->accept_next;
        connection->accept_next = NULL;
        rdplib_detach_connection(connection);
    }
    endpoint->accept_tail = NULL;

    for (message = endpoint->connectionless_head; message; message = next_message)
    {
        next_message = message->next;
        message->next = NULL;
        rdplib_message_release(message);
    }

    rdp_destroy(endpoint->raw, 1);
    rdplib_platform_mutex_lock(&runtime->lock);
    assert(runtime->endpoint_count != 0);
    --runtime->endpoint_count;
    rdplib_platform_mutex_unlock(&runtime->lock);
    rdplib_platform_free(endpoint);
    return RDPLIB_OK;
}

uint16_t rdplib_endpoint_local_port(const rdplib_endpoint_t *endpoint)
{
    return endpoint && endpoint->raw ? ntohs(endpoint->raw->local_udp_addr.sin_port) : 0;
}

int rdplib_endpoint_set_socket_receive_buffer_size(rdplib_endpoint_t *endpoint, uint32_t bytes)
{
    if (!endpoint || !endpoint->raw || bytes == 0 || bytes > (uint32_t)INT_MAX)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    return rdplib_rdp_set_socket_receive_buffer_size(endpoint->raw, bytes) == 0 ? RDPLIB_OK : RDPLIB_ERROR_PLATFORM;
}

int rdplib_endpoint_set_socket_send_buffer_size(rdplib_endpoint_t *endpoint, uint32_t bytes)
{
    if (!endpoint || !endpoint->raw || bytes == 0 || bytes > (uint32_t)INT_MAX)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    return rdplib_rdp_set_socket_send_buffer_size(endpoint->raw, bytes) == 0 ? RDPLIB_OK : RDPLIB_ERROR_PLATFORM;
}

int rdplib_endpoint_get_socket_receive_buffer_size(const rdplib_endpoint_t *endpoint, uint32_t *bytes)
{
    if (!endpoint || !endpoint->raw || !bytes)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    return rdplib_rdp_get_socket_receive_buffer_size(endpoint->raw, bytes) == 0 ? RDPLIB_OK : RDPLIB_ERROR_PLATFORM;
}

int rdplib_endpoint_get_socket_send_buffer_size(const rdplib_endpoint_t *endpoint, uint32_t *bytes)
{
    if (!endpoint || !endpoint->raw || !bytes)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    return rdplib_rdp_get_socket_send_buffer_size(endpoint->raw, bytes) == 0 ? RDPLIB_OK : RDPLIB_ERROR_PLATFORM;
}

int rdplib_endpoint_get_input_rate(const rdplib_endpoint_t *endpoint, rdplib_endpoint_input_rate_t *input_rate)
{
    rdplib_rdp_input_rate_t raw;

    if (!endpoint || !endpoint->raw || !input_rate)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    rdplib_rdp_get_input_rate(endpoint->raw, &raw);
    input_rate->bytes_per_second = raw.bytes_per_second;
    input_rate->duplicate_reliable_bytes_per_second = raw.duplicate_bytes_per_second;
    return RDPLIB_OK;
}

int rdplib_endpoint_process(rdplib_endpoint_t *endpoint, int32_t timeout_ms)
{
    msg_arrival_t *arrival;
    int count = 0;
    int result;

    if (!endpoint || !endpoint->raw)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }

    arrival = rdp_receive(endpoint->raw, timeout_ms);
    while (arrival)
    {
        result = rdplib_process_arrival(endpoint, arrival);
        if (result != RDPLIB_OK)
        {
            return result;
        }
        ++count;
        arrival = rdp_receive(endpoint->raw, 0);
    }
    return count;
}

rdplib_connection_t *rdplib_endpoint_accept(rdplib_endpoint_t *endpoint)
{
    rdplib_connection_t *connection;

    if (!endpoint)
    {
        return NULL;
    }

    connection = endpoint->accept_head;
    if (!connection)
    {
        return NULL;
    }
    endpoint->accept_head = connection->accept_next;
    if (!endpoint->accept_head)
    {
        endpoint->accept_tail = NULL;
    }
    connection->accept_next = NULL;
    assert(!connection->application_owned);
    connection->application_owned = 1;
    ++endpoint->application_connection_count;
    return connection;
}

rdplib_message_t *rdplib_endpoint_pop_connectionless(rdplib_endpoint_t *endpoint)
{
    rdplib_message_t *message;

    if (!endpoint)
    {
        return NULL;
    }
    message = endpoint->connectionless_head;
    if (message)
    {
        endpoint->connectionless_head = message->next;
        if (!endpoint->connectionless_head)
        {
            endpoint->connectionless_tail = NULL;
        }
        message->next = NULL;
    }
    return message;
}

int rdplib_connect(rdplib_endpoint_t *endpoint, rdplib_connection_t **output, const char *host, uint16_t port)
{
    connection_t *raw = NULL;
    rdplib_connection_t *connection;
    int result;

    if (!endpoint || !output || !host)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;

    result = rdp_connect(endpoint->raw, &raw, (char *)host, port, 0);
    if (result != 0)
    {
        return result;
    }

    connection = rdplib_create_connection_handle(endpoint, raw, 0);
    if (!connection)
    {
        (void)connection_close(raw, 0, NULL, NULL);
        return RDPLIB_ERROR_OUT_OF_MEMORY;
    }
    *output = connection;
    return RDPLIB_OK;
}

int rdplib_connection_is_usable(rdplib_connection_t *connection)
{
    connection_t *raw;
    int usable;

    if (!connection)
    {
        return 0;
    }
    raw = connection->raw;
    usable = raw && !connection->close_started && !connection->peer_fin && !connection->disconnected;
    if (usable)
    {
        umutex_lock(&raw->cn_lock);
        usable = raw->tx_connected && !raw->tx_stopped && !raw->tx_fin_sent;
        umutex_unlock(&raw->cn_lock);
    }
    return usable;
}

static int rdplib_connection_enable_keepalive_internal(rdplib_connection_t *connection, uint32_t interval_ms, int replace_interval)
{
    connection_t *raw;
    int result = RDPLIB_OK;
    int resort = 0;

    if (!connection)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    raw = connection->raw;
    if (!raw || connection->close_started || connection->peer_fin || connection->disconnected)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }

    umutex_lock(&raw->cn_lock);
    if (!raw->tx_connected || raw->tx_fin_sent || raw->tx_stopped)
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
#ifndef RDPLIB_SOURCE_FAITHFUL
        if (replace_interval && raw->rdplib_keepalive_interval_ms != interval_ms)
        {
            raw->rdplib_keepalive_interval_ms = interval_ms;
            resort = 1;
        }
#else
        (void)interval_ms;
        (void)replace_interval;
        assert(!replace_interval);
#endif
        if ((raw->cn_flags & RDP_CONNECTION_FEATURE_KEEPALIVE) == 0)
        {
            raw->cn_flags |= RDP_CONNECTION_FEATURE_KEEPALIVE;
            resort = 1;
        }
        if (resort)
        {
            rdp_resort(raw, 1);
        }
    }
    umutex_unlock(&raw->cn_lock);
    return result;
}

int rdplib_connection_enable_keepalive(rdplib_connection_t *connection)
{
    return rdplib_connection_enable_keepalive_internal(connection, RDPLIB_DEFAULT_KEEPALIVE_INTERVAL_MS, 0);
}

int rdplib_connection_enable_keepalive_with_interval(rdplib_connection_t *connection, uint32_t interval_ms)
{
    if (!connection || interval_ms == 0 || interval_ms > INT32_MAX)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
#ifdef RDPLIB_SOURCE_FAITHFUL
    return RDPLIB_ERROR_NOT_SUPPORTED;
#else
    return rdplib_connection_enable_keepalive_internal(connection, interval_ms, 1);
#endif
}

int rdplib_connection_set_packet_drop_callback(rdplib_connection_t *connection, rdplib_packet_drop_callback_t callback, void *context)
{
#ifndef RDPLIB_SOURCE_FAITHFUL
    connection_t *raw;
    int result = RDPLIB_OK;
#endif

    if (!connection)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
#ifdef RDPLIB_SOURCE_FAITHFUL
    (void)callback;
    (void)context;
    return RDPLIB_ERROR_NOT_SUPPORTED;
#else
    raw = connection->raw;
    if (!raw || connection->close_started || (callback && (connection->peer_fin || connection->disconnected)))
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }

    umutex_lock(&raw->cn_lock);
    if (callback && (!raw->tx_connected || raw->tx_fin_sent || raw->tx_stopped))
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
        raw->rdplib_packet_drop_callback = callback;
        raw->rdplib_packet_drop_context = callback ? context : NULL;
    }
    umutex_unlock(&raw->cn_lock);
    return result;
#endif
}

const rdplib_message_t *rdplib_connection_peek_message(const rdplib_connection_t *connection)
{
    if (!connection)
    {
        return NULL;
    }
    return connection->message_head;
}

rdplib_message_t *rdplib_connection_pop_message(rdplib_connection_t *connection)
{
    rdplib_message_t *message = NULL;

    if (!connection)
    {
        return NULL;
    }
    message = connection->message_head;
    if (message)
    {
        connection->message_head = message->next;
        if (!connection->message_head)
        {
            connection->message_tail = NULL;
        }
        message->next = NULL;
    }
    return message;
}

int rdplib_connection_send(rdplib_connection_t *connection, const void *data, uint32_t bytes, uint32_t stream, uint32_t flags)
{
    int result;

    if (!connection)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }

    rdplib_platform_mutex_lock(&connection->send_lock);
    if (!connection->raw || connection->close_started || connection->peer_fin || connection->disconnected)
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
        result = connection_send(connection->raw, data, bytes, stream, flags);
    }
    rdplib_platform_mutex_unlock(&connection->send_lock);
    return result;
}

int rdplib_connection_begin_close(rdplib_connection_t *connection, uint32_t linger_timeout_ms)
{
    connection_t *raw;
    int result;

    if (!connection)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }

    rdplib_platform_mutex_lock(&connection->send_lock);
    if (connection->close_started)
    {
        result = RDPLIB_OK;
    }
    else if (!connection->raw)
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
        assert(connection->message_head == NULL);
        if (connection->message_head)
        {
            result = RDPLIB_ERROR_BUSY;
        }
        else
        {
            connection->close_started = 1;
            raw = rdplib_connection_detach_raw(connection);
            rdplib_connection_clear_packet_drop_callback(raw);
            result = connection_close(raw, linger_timeout_ms, NULL, NULL);
        }
    }
    rdplib_platform_mutex_unlock(&connection->send_lock);
    return result;
}

int rdplib_connection_set_data_rate(rdplib_connection_t *connection, uint32_t bytes_per_second)
{
    connection_t *raw;
    int result = RDPLIB_OK;

    if (!connection || bytes_per_second == 0)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    raw = connection->raw;
    if (!raw || connection->close_started || connection->peer_fin || connection->disconnected)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }

    umutex_lock(&raw->cn_lock);
    if (!raw->tx_connected || raw->tx_fin_sent || raw->tx_stopped)
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
        (void)connection_set_max_data_rate(raw, bytes_per_second);
    }
    umutex_unlock(&raw->cn_lock);
    return result;
}

int rdplib_connection_set_send_buffer_size(rdplib_connection_t *connection, uint32_t bytes)
{
    connection_t *raw;
    int result = RDPLIB_OK;

    if (!connection)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    raw = connection->raw;
    if (!raw || connection->close_started || connection->peer_fin || connection->disconnected)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }

    umutex_lock(&raw->cn_lock);
    if (!raw->tx_connected || raw->tx_fin_sent || raw->tx_stopped)
    {
        result = RDPLIB_ERROR_NOT_USABLE;
    }
    else
    {
        connection_set_send_buffer_size(raw, bytes);
    }
    umutex_unlock(&raw->cn_lock);
    return result;
}

static int rdplib_decode_ipv4_address(const uint8_t endpoint[16], uint8_t address[4], uint16_t *port)
{
    if (!endpoint || !address || !port || endpoint[0] != 2 || endpoint[1] != 0)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    memcpy(address, endpoint + 4, 4);
    *port = (uint16_t)(((uint16_t)endpoint[2] << 8) | endpoint[3]);
    return RDPLIB_OK;
}

int rdplib_connection_get_remote_ipv4(rdplib_connection_t *connection, uint8_t address[4], uint16_t *port)
{
    uint8_t endpoint[16];

    if (!connection || !address || !port)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    memcpy(endpoint, connection->remote_address, sizeof(endpoint));
    return rdplib_decode_ipv4_address(endpoint, address, port);
}

int rdplib_connection_get_counters(rdplib_connection_t *connection, rdplib_connection_counters_t *counters)
{
    connection_stat raw;
    rdplib_connection_counters_t result;

    if (!connection || !counters)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    if (!connection->raw)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }

    umutex_lock(&connection->raw->cn_lock);
    memcpy(&raw, &connection->raw->stat, sizeof(raw));
    umutex_unlock(&connection->raw->cn_lock);
    memset(&result, 0, sizeof(result));
#define COPY_COUNTER(public_name, raw_name) result.public_name = raw.raw_name
    COPY_COUNTER(unreliable_packets_tx, best_effort_packets_tx);
    COPY_COUNTER(unreliable_bytes_tx, best_effort_bytes_tx);
    COPY_COUNTER(reliable_packets_tx, guaranteed_packets_tx);
    COPY_COUNTER(reliable_bytes_tx, guaranteed_bytes_tx);
    COPY_COUNTER(reliable_packets_retransmitted, guaranteed_packets_retx);
    COPY_COUNTER(reliable_bytes_retransmitted, guaranteed_bytes_retx);
    COPY_COUNTER(ack_only_packets_tx, ack_only_packets_tx);
    COPY_COUNTER(ack_and_data_packets_tx, ack_and_data_packets_tx);
    COPY_COUNTER(unreliable_packets_rx, best_effort_packets_rx);
    COPY_COUNTER(unreliable_bytes_rx, best_effort_bytes_rx);
    COPY_COUNTER(reliable_packets_rx, guaranteed_packets_rx);
    COPY_COUNTER(reliable_bytes_rx, guaranteed_bytes_rx);
    COPY_COUNTER(duplicate_reliable_packets_rx, duplicate_packets_rx);
    COPY_COUNTER(duplicate_reliable_bytes_rx, duplicate_bytes_rx);
    COPY_COUNTER(header_bytes_rx, header_bytes_rx);
    COPY_COUNTER(ack_only_packets_rx, ack_only_packets_rx);
    COPY_COUNTER(ack_and_data_packets_rx, ack_and_data_packets_rx);
    COPY_COUNTER(messages_acked, messages_acked);
    COPY_COUNTER(duplicate_acks, duplicate_acks);
    COPY_COUNTER(bytes_in_duplicate_acks, bytes_in_duplicate_acks);
    COPY_COUNTER(acks_for_unsent_messages, acks_for_unsent_messages);
    COPY_COUNTER(packets_rx_in_sequence, packets_rx_in_sequence);
    COPY_COUNTER(bytes_rx_in_sequence, bytes_rx_in_sequence);
    COPY_COUNTER(packets_rx_out_of_sequence, packets_rx_out_of_sequence);
    COPY_COUNTER(bytes_rx_out_of_sequence, bytes_rx_out_of_sequence);
    COPY_COUNTER(discarded_bad_options, discarded_bad_options);
    COPY_COUNTER(discarded_old_seqnum, discarded_old_seqnum);
    COPY_COUNTER(discarded_dup_seqnum, discarded_dup_seqnum);
    COPY_COUNTER(discarded_old_msgid, discarded_old_msgid);
    COPY_COUNTER(discarded_bad_fragment, discarded_bad_fragment);
    COPY_COUNTER(discarded_bad_stream, discarded_bad_stream);
    COPY_COUNTER(discarded_too_short, discarded_too_short);
    COPY_COUNTER(discarded_bad_fragment_size, discarded_bad_fragment_size);
    COPY_COUNTER(discarded_bad_ack_header, discarded_bad_ack_header);
    COPY_COUNTER(discarded_bad_ackmask, discarded_bad_ackmask);
    COPY_COUNTER(discarded_mask_wo_ack, discarded_mask_wo_ack);
    COPY_COUNTER(discarded_old_ack, discarded_old_ack);
    COPY_COUNTER(packets_updated_rtt, packets_updated_rtt);
    COPY_COUNTER(packets_updated_rtt_attempts, packets_updated_rtt_attempts);
    COPY_COUNTER(icmp_source_quench, icmp_source_quench);
    COPY_COUNTER(icmp_unknown, icmp_unknown);
#undef COPY_COUNTER
    memcpy(result.icmp_unreachable, raw.icmp_unreachable, sizeof(result.icmp_unreachable));
    memcpy(result.icmp_ttl_expired, raw.icmp_ttl_expired, sizeof(result.icmp_ttl_expired));
    memcpy(result.icmp_parameter_problem, raw.icmp_parameter_problem, sizeof(result.icmp_parameter_problem));

    memcpy(counters, &result, sizeof(result));
    return RDPLIB_OK;
}

int rdplib_connection_get_perf_stats(rdplib_connection_t *connection, rdplib_connection_perf_stats_t *statistics)
{
    perf_stats_t raw_statistics;

    if (!connection || !statistics)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    if (!connection->raw)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }
    connection_get_perf_stats(connection->raw, &raw_statistics);
    statistics->last_packet_receive_time_ms = raw_statistics.time_last_arrival;
    statistics->received_packet_sequence_history = raw_statistics.recent_seqnum_history;
    statistics->last_received_packet_sequence = raw_statistics.highest_seqnum_received;
    statistics->rtt_mean_ms = raw_statistics.average_rt_time;
    statistics->rtt_deviation_ms = raw_statistics.std_deviation;
    statistics->last_ping_sample_ms = raw_statistics.last_rt_time;
    statistics->queued_reliable_bytes = raw_statistics.queue_size;
    statistics->transmit_stall_time_ms = raw_statistics.stall_time;
    return RDPLIB_OK;
}

int rdplib_connection_get_disconnect_info(rdplib_connection_t *connection, rdplib_disconnect_info_t *information)
{
    disconnect_info_t raw_information;

    if (!connection || !information)
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    if (!connection->raw)
    {
        return RDPLIB_ERROR_NOT_USABLE;
    }
    umutex_lock(&connection->raw->cn_lock);
    connection_get_disconnect_info(connection->raw, &raw_information, sizeof(raw_information));
    umutex_unlock(&connection->raw->cn_lock);
    information->reason = raw_information.disconnect_reason;
    if (raw_information.disconnect_reason == RDP_DISCONNECT_REASON_ICMP)
    {
        information->icmp_type = raw_information.icmp_type;
        information->icmp_code = raw_information.icmp_code;
        memcpy(information->icmp_source_ipv4, &raw_information.icmp_from, sizeof(information->icmp_source_ipv4));
    }
    else
    {
        information->icmp_type = 0;
        information->icmp_code = 0;
        memset(information->icmp_source_ipv4, 0, sizeof(information->icmp_source_ipv4));
    }
    return RDPLIB_OK;
}

uint16_t rdplib_message_flags(const rdplib_message_t *message)
{
    return message ? message->flags : 0;
}

uint8_t rdplib_message_stream(const rdplib_message_t *message)
{
    return message ? message->stream_id : 0;
}

uint32_t rdplib_message_size(const rdplib_message_t *message)
{
    return message ? message->payload_bytes : 0;
}

const void *rdplib_message_data(const rdplib_message_t *message)
{
    if (!message || message->payload_bytes == 0)
    {
        return NULL;
    }
    return msg_arrival_get_data(message->arrival);
}

int rdplib_message_is_connectionless(const rdplib_message_t *message)
{
    return message && message->flags == UINT16_C(0xFFFF);
}

int rdplib_message_is_disconnect(const rdplib_message_t *message)
{
    return message && message->disconnect;
}

int rdplib_message_has_fin(const rdplib_message_t *message)
{
    return message && (message->flags & RDP_FLAG_FIN) != 0;
}

int rdplib_message_get_sender_ipv4(const rdplib_message_t *message, uint8_t address[4], uint16_t *port)
{
    if (!rdplib_message_is_connectionless(message))
    {
        return RDPLIB_ERROR_INVALID_ARGUMENT;
    }
    return rdplib_decode_ipv4_address(message->sender_address, address, port);
}

void rdplib_message_release(rdplib_message_t *message)
{
    if (!message)
    {
        return;
    }
    assert(message->arrival != NULL);
    assert(message->runtime != NULL);
    fast_free(message->arrival);
    rdplib_runtime_remove_outstanding(message->runtime);
    rdplib_platform_free(message);
}
