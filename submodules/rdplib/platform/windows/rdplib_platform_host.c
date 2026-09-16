// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#define _CRT_RAND_S
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "rdplib_platform.h"

#include <mswsock.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

static uint32_t g_last_recorded_socket_error;
static LONG g_random_fallback_counter;

void *rdplib_platform_malloc(size_t size)
{
    return malloc(size);
}

void rdplib_platform_free(void *allocation)
{
    free(allocation);
}

uint32_t rdplib_platform_random_u32(void)
{
    unsigned int value;

    if (rand_s(&value) == 0)
    {
        return (uint32_t)value;
    }
    return GetTickCount() ^ GetCurrentProcessId() ^ (uint32_t)InterlockedIncrement(&g_random_fallback_counter);
}

void rdplib_platform_mutex_prepare(rdplib_platform_mutex_t *mutex)
{
    mutex->initialized = 0;
}

void rdplib_platform_mutex_init(rdplib_platform_mutex_t *mutex)
{
    InitializeCriticalSection(&mutex->value);
    mutex->initialized = 1;
}

void rdplib_platform_mutex_destroy(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        DeleteCriticalSection(&mutex->value);
        mutex->initialized = 0;
    }
}

void rdplib_platform_mutex_lock(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        EnterCriticalSection(&mutex->value);
    }
}

void rdplib_platform_mutex_unlock(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        LeaveCriticalSection(&mutex->value);
    }
}

int rdplib_platform_resolve_ipv4(const char *host, uint16_t port, uint8_t endpoint[16])
{
    struct sockaddr_in address;
    struct hostent *host_entry;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(host);
    if (address.sin_addr.s_addr == INADDR_NONE)
    {
        host_entry = gethostbyname(host);
        if (!host_entry)
        {
            return 1;
        }
        memcpy(&address.sin_addr.s_addr, host_entry->h_addr_list[0], sizeof(address.sin_addr.s_addr));
    }

    memcpy(endpoint, &address, sizeof(address));
    return 0;
}

int rdplib_platform_network_startup(uint16_t version)
{
    WSADATA data;
    return WSAStartup(version, &data);
}

void rdplib_platform_network_cleanup(void)
{
    (void)WSACleanup();
}

int rdplib_platform_protocol_number(const char *name, int fallback)
{
    struct protoent *entry = getprotobyname(name);
    return entry ? entry->p_proto : fallback;
}

intptr_t rdplib_platform_socket_create(int family, int type, int protocol)
{
    SOCKET endpoint = socket(family, type, protocol);
    return endpoint == INVALID_SOCKET ? -1 : (intptr_t)endpoint;
}

int rdplib_platform_socket_bind(intptr_t endpoint, const uint8_t *address, uint32_t address_bytes)
{
    return bind((SOCKET)endpoint, (const struct sockaddr *)address, (int)address_bytes);
}

int rdplib_platform_socket_get_name(intptr_t endpoint, uint8_t *address, uint32_t *address_bytes)
{
    int bytes = (int)*address_bytes;
    int result = getsockname((SOCKET)endpoint, (struct sockaddr *)address, &bytes);
    *address_bytes = (uint32_t)bytes;
    return result;
}

static int rdplib_platform_socket_level(int level)
{
    return level == 0xFFFF ? SOL_SOCKET : level;
}

static int rdplib_platform_socket_option(int level, int option)
{
    if (level == 0xFFFF && option == 32)
    {
        return SO_BROADCAST;
    }
    if (level == 0 && option == 7)
    {
        return IP_TTL;
    }
    return option;
}

int rdplib_platform_socket_set_option(intptr_t endpoint, int level, int option, const void *value, uint32_t value_bytes)
{
    int native_level = rdplib_platform_socket_level(level);
    int native_option = rdplib_platform_socket_option(level, option);
    return setsockopt((SOCKET)endpoint, native_level, native_option, (const char *)value, (int)value_bytes);
}

int rdplib_platform_socket_get_option(intptr_t endpoint, int level, int option, void *value, uint32_t *value_bytes)
{
    int native_level = rdplib_platform_socket_level(level);
    int native_option = rdplib_platform_socket_option(level, option);
    int bytes = (int)*value_bytes;
    int result = getsockopt((SOCKET)endpoint, native_level, native_option, (char *)value, &bytes);
    *value_bytes = (uint32_t)bytes;
    return result;
}

uint32_t rdplib_platform_last_socket_error(void)
{
    return (uint32_t)WSAGetLastError();
}

void rdplib_platform_record_socket_error(uint32_t error)
{
    g_last_recorded_socket_error = error;
}

void rdplib_platform_socket_close(intptr_t endpoint)
{
    (void)closesocket((SOCKET)endpoint);
}

int32_t rdplib_platform_send_datagram(intptr_t endpoint, const uint8_t *packet, uint32_t packet_bytes, const uint8_t destination[16])
{
    return rdplib_platform_send_datagram_to(endpoint, packet, packet_bytes, destination, 16);
}

int32_t rdplib_platform_send_datagram_to(intptr_t endpoint, const uint8_t *packet, uint32_t packet_bytes, const void *destination, uint32_t destination_bytes)
{
    return sendto((SOCKET)endpoint, (const char *)packet, (int)packet_bytes, 0, (const struct sockaddr *)destination, (int)destination_bytes);
}

#ifndef RDPLIB_SOURCE_FAITHFUL
int rdplib_platform_enable_icmp_errors(intptr_t endpoint)
{
    BOOL enabled = TRUE;
    DWORD returned_bytes = 0;

    return WSAIoctl((SOCKET)endpoint, SIO_UDP_CONNRESET, &enabled, (DWORD)sizeof(enabled), NULL, 0, &returned_bytes, NULL, NULL) == SOCKET_ERROR ? -1 : 0;
}
#endif

int32_t rdplib_platform_receive_datagram(intptr_t endpoint, uint8_t *packet, uint32_t packet_capacity, uint8_t source_address[16])
{
    int address_bytes = 16;
    int received;
#ifndef RDPLIB_SOURCE_FAITHFUL
    uint16_t source_family;
    int socket_error;

    memset(source_address, 0, 16);
#endif

    received = recvfrom((SOCKET)endpoint, (char *)packet, (int)packet_capacity, 0, (struct sockaddr *)source_address, &address_bytes);
    if (received != SOCKET_ERROR)
    {
        return received;
    }

#ifndef RDPLIB_SOURCE_FAITHFUL
    socket_error = WSAGetLastError();
    memcpy(&source_family, source_address, sizeof(source_family));
    if (socket_error == WSAECONNRESET && address_bytes >= (int)sizeof(struct sockaddr_in) && source_family == AF_INET)
    {
        return RDPLIB_PLATFORM_RECEIVE_ICMP_PORT_UNREACHABLE;
    }
#endif

    return -1;
}

int rdplib_platform_wait(intptr_t ipv4_socket, intptr_t ipx_socket, intptr_t icmp_socket, uint32_t enabled_sources, int infinite, uint32_t timeout_seconds, uint32_t timeout_microseconds,
                         uint32_t *ready_sources)
{
    fd_set read_set;
    struct timeval timeout;
    struct timeval *timeout_ptr = NULL;
    int result;

    FD_ZERO(&read_set);
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPV4) != 0)
    {
        FD_SET((SOCKET)ipv4_socket, &read_set);
    }
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPX) != 0)
    {
        FD_SET((SOCKET)ipx_socket, &read_set);
    }
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_ICMP) != 0)
    {
        FD_SET((SOCKET)icmp_socket, &read_set);
    }

    if (!infinite)
    {
        timeout.tv_sec = (long)timeout_seconds;
        timeout.tv_usec = (long)timeout_microseconds;
        timeout_ptr = &timeout;
    }

    result = select(0, &read_set, NULL, NULL, timeout_ptr);
    *ready_sources = 0;
    if (result > 0)
    {
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPV4) != 0 && FD_ISSET((SOCKET)ipv4_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_IPV4;
        }
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPX) != 0 && FD_ISSET((SOCKET)ipx_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_IPX;
        }
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_ICMP) != 0 && FD_ISSET((SOCKET)icmp_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_ICMP;
        }
    }
    return result;
}

intptr_t rdplib_platform_serial_create_event(void)
{
    return (intptr_t)CreateEventA(NULL, TRUE, FALSE, NULL);
}

int rdplib_platform_serial_close_event(intptr_t event)
{
    if (event)
    {
        return CloseHandle((HANDLE)event) != FALSE;
    }
    return 0;
}

int rdplib_platform_serial_write(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, const void *data, uint32_t bytes, uint32_t *bytes_written, uint32_t *error_code)
{
    DWORD written = 0;
    BOOL result = WriteFile((HANDLE)endpoint, data, bytes, &written, async_state);
    *bytes_written = (uint32_t)written;
    *error_code = result ? 0 : (uint32_t)GetLastError();
    return result != FALSE;
}

int rdplib_platform_serial_get_write_result(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, int wait, uint32_t *bytes_written, uint32_t *error_code)
{
    DWORD written = 0;
    BOOL result = GetOverlappedResult((HANDLE)endpoint, async_state, &written, wait != 0);
    *bytes_written = (uint32_t)written;
    *error_code = result ? 0 : (uint32_t)GetLastError();
    return result != FALSE;
}

int rdplib_platform_serial_read(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, void *data, uint32_t bytes, uint32_t *bytes_read, uint32_t *error_code)
{
    DWORD read_bytes = 0;
    BOOL result = ReadFile((HANDLE)endpoint, data, bytes, &read_bytes, async_state);
    *bytes_read = (uint32_t)read_bytes;
    *error_code = result ? 0 : (uint32_t)GetLastError();
    return result != FALSE;
}

int rdplib_platform_serial_get_read_result(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, int wait, uint32_t *bytes_read, uint32_t *error_code)
{
    DWORD read_bytes = 0;
    BOOL result = GetOverlappedResult((HANDLE)endpoint, async_state, &read_bytes, wait != 0);
    *bytes_read = (uint32_t)read_bytes;
    *error_code = result ? 0 : (uint32_t)GetLastError();
    return result != FALSE;
}

uint32_t rdplib_platform_last_system_error(void)
{
    return (uint32_t)GetLastError();
}
