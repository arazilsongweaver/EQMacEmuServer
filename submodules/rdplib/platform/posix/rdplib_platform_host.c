// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#if defined(__linux__) && !defined(RDPLIB_SOURCE_FAITHFUL)
#include <linux/errqueue.h>
#include <sys/uio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t g_last_recorded_socket_error;
static pthread_mutex_t g_random_fallback_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_random_fallback_counter;

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
    uint32_t value;
    uint8_t *write_cursor = (uint8_t *)&value;
    size_t remaining = sizeof(value);
    int descriptor = open("/dev/urandom", O_RDONLY);

    if (descriptor >= 0)
    {
        while (remaining != 0)
        {
            ssize_t bytes_read = read(descriptor, write_cursor, remaining);

            if (bytes_read > 0)
            {
                write_cursor += bytes_read;
                remaining -= (size_t)bytes_read;
            }
            else if (bytes_read < 0 && errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        (void)close(descriptor);
        if (remaining == 0)
        {
            return value;
        }
    }

    (void)pthread_mutex_lock(&g_random_fallback_lock);
    value = (uint32_t)time(NULL) ^ (uint32_t)getpid() ^ ++g_random_fallback_counter;
    (void)pthread_mutex_unlock(&g_random_fallback_lock);
    return value;
}

void rdplib_platform_mutex_prepare(rdplib_platform_mutex_t *mutex)
{
    mutex->initialized = 0;
}

void rdplib_platform_mutex_init(rdplib_platform_mutex_t *mutex)
{
    (void)pthread_mutex_init(&mutex->value, NULL);
    mutex->initialized = 1;
}

void rdplib_platform_mutex_destroy(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        (void)pthread_mutex_destroy(&mutex->value);
        mutex->initialized = 0;
    }
}

void rdplib_platform_mutex_lock(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        (void)pthread_mutex_lock(&mutex->value);
    }
}

void rdplib_platform_mutex_unlock(rdplib_platform_mutex_t *mutex)
{
    if (mutex->initialized)
    {
        (void)pthread_mutex_unlock(&mutex->value);
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
    (void)version;
    return 0;
}

void rdplib_platform_network_cleanup(void)
{
}

int rdplib_platform_protocol_number(const char *name, int fallback)
{
    struct protoent *entry = getprotobyname(name);
    return entry ? entry->p_proto : fallback;
}

intptr_t rdplib_platform_socket_create(int family, int type, int protocol)
{
    int native_family;
    int endpoint;

    if (family == 2)
    {
        native_family = AF_INET;
    }
    else
    {
        // The clients' family 6 is historical IPX, not modern AF_INET6.
        // The standalone POSIX adapter does not emulate that transport.
        return -1;
    }

    endpoint = socket(native_family, type, protocol);
    return endpoint < 0 ? -1 : endpoint;
}

int rdplib_platform_socket_bind(intptr_t endpoint, const uint8_t *address, uint32_t address_bytes)
{
    return bind((int)endpoint, (const struct sockaddr *)address, (socklen_t)address_bytes);
}

int rdplib_platform_socket_get_name(intptr_t endpoint, uint8_t *address, uint32_t *address_bytes)
{
    socklen_t bytes = (socklen_t)*address_bytes;
    int result = getsockname((int)endpoint, (struct sockaddr *)address, &bytes);
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
    if (level == 0xFFFF && option == 0x1001)
    {
        return SO_SNDBUF;
    }
    if (level == 0xFFFF && option == 0x1002)
    {
        return SO_RCVBUF;
    }
    if (level == 0 && option == 7)
    {
        return IP_TTL;
    }
    return option;
}

int rdplib_platform_socket_set_option(intptr_t endpoint, int level, int option, const void *value, uint32_t value_bytes)
{
    return setsockopt((int)endpoint, rdplib_platform_socket_level(level), rdplib_platform_socket_option(level, option), value, (socklen_t)value_bytes);
}

int rdplib_platform_socket_get_option(intptr_t endpoint, int level, int option, void *value, uint32_t *value_bytes)
{
    socklen_t bytes = (socklen_t)*value_bytes;
    int result = getsockopt((int)endpoint, rdplib_platform_socket_level(level), rdplib_platform_socket_option(level, option), value, &bytes);
    *value_bytes = (uint32_t)bytes;
    return result;
}

static uint32_t rdplib_platform_normalize_socket_error(int error)
{
    if (error == EADDRINUSE)
    {
        return 10048;
    }
#ifdef ENOPROTOOPT
    if (error == ENOPROTOOPT)
    {
        return 10042;
    }
#endif
    if (error == EAGAIN || error == EWOULDBLOCK)
    {
        return 10035;
    }
#ifdef ENOBUFS
    if (error == ENOBUFS)
    {
        return 10055;
    }
#endif
    return (uint32_t)error;
}

uint32_t rdplib_platform_last_socket_error(void)
{
    return rdplib_platform_normalize_socket_error(errno);
}

void rdplib_platform_record_socket_error(uint32_t error)
{
    g_last_recorded_socket_error = error;
}

void rdplib_platform_socket_close(intptr_t endpoint)
{
    (void)close((int)endpoint);
}

int32_t rdplib_platform_send_datagram(intptr_t endpoint, const uint8_t *packet, uint32_t packet_bytes, const uint8_t destination[16])
{
    return rdplib_platform_send_datagram_to(endpoint, packet, packet_bytes, destination, 16);
}

int32_t rdplib_platform_send_datagram_to(intptr_t endpoint, const uint8_t *packet, uint32_t packet_bytes, const void *destination, uint32_t destination_bytes)
{
    ssize_t sent = sendto((int)endpoint, packet, packet_bytes, 0, (const struct sockaddr *)destination, (socklen_t)destination_bytes);
    return sent < 0 ? -1 : (int32_t)sent;
}

int32_t rdplib_platform_receive_datagram(intptr_t endpoint, uint8_t *packet, uint32_t packet_capacity, uint8_t source_address[16])
{
    socklen_t address_bytes = 16;
    ssize_t received = recvfrom((int)endpoint, packet, packet_capacity, 0, (struct sockaddr *)source_address, &address_bytes);
    return received < 0 ? -1 : (int32_t)received;
}

#if defined(__linux__) && !defined(RDPLIB_SOURCE_FAITHFUL)
int rdplib_platform_enable_icmp_errors(intptr_t endpoint)
{
    int enabled = 1;

    return setsockopt((int)endpoint, IPPROTO_IP, IP_RECVERR, &enabled, sizeof(enabled));
}

int rdplib_platform_receive_icmp_error(intptr_t endpoint, rdplib_platform_icmp_error_t *error)
{
    union
    {
        struct cmsghdr alignment;
        uint8_t bytes[CMSG_SPACE(sizeof(struct sock_extended_err) + sizeof(struct sockaddr_in))];
    } control;
    struct sockaddr_in destination;
    struct iovec vector;
    struct msghdr message;
    struct cmsghdr *control_message;
    struct sock_extended_err *extended_error;
    struct sockaddr *offender;
    uint8_t discarded_payload;
    ssize_t received;

    for (;;)
    {
        memset(&destination, 0, sizeof(destination));
        memset(&control, 0, sizeof(control));
        memset(&message, 0, sizeof(message));

        vector.iov_base = &discarded_payload;
        vector.iov_len = sizeof(discarded_payload);
        message.msg_name = &destination;
        message.msg_namelen = sizeof(destination);
        message.msg_iov = &vector;
        message.msg_iovlen = 1;
        message.msg_control = control.bytes;
        message.msg_controllen = sizeof(control.bytes);

        do
        {
            received = recvmsg((int)endpoint, &message, MSG_ERRQUEUE | MSG_DONTWAIT);
        } while (received < 0 && errno == EINTR);

        if (received < 0)
        {
            return 0;
        }
        if ((message.msg_flags & MSG_CTRUNC) != 0 || message.msg_namelen < sizeof(destination) || destination.sin_family != AF_INET)
        {
            continue;
        }

        for (control_message = CMSG_FIRSTHDR(&message); control_message; control_message = CMSG_NXTHDR(&message, control_message))
        {
            if (control_message->cmsg_level != IPPROTO_IP || control_message->cmsg_type != IP_RECVERR ||
                control_message->cmsg_len < CMSG_LEN(sizeof(struct sock_extended_err)))
            {
                continue;
            }

            extended_error = (struct sock_extended_err *)CMSG_DATA(control_message);
            if (extended_error->ee_origin != SO_EE_ORIGIN_ICMP)
            {
                continue;
            }

            memset(error, 0, sizeof(*error));
            memcpy(error->remote_address, &destination, sizeof(destination));
            error->type = extended_error->ee_type;
            error->code = extended_error->ee_code;

            if (control_message->cmsg_len >= CMSG_LEN(sizeof(*extended_error) + sizeof(struct sockaddr_in)))
            {
                offender = SO_EE_OFFENDER(extended_error);
                if (offender->sa_family == AF_INET)
                {
                    memcpy(error->source_address, offender, sizeof(struct sockaddr_in));
                }
            }

            return 1;
        }
    }
}
#endif

int rdplib_platform_wait(intptr_t ipv4_socket, intptr_t ipx_socket, intptr_t icmp_socket, uint32_t enabled_sources, int infinite, uint32_t timeout_seconds, uint32_t timeout_microseconds,
                         uint32_t *ready_sources)
{
    fd_set read_set;
    struct timeval timeout;
    struct timeval *timeout_ptr = NULL;
    int maximum = -1;
    int result;

    FD_ZERO(&read_set);
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPV4) != 0)
    {
        FD_SET((int)ipv4_socket, &read_set);
        maximum = (int)ipv4_socket > maximum ? (int)ipv4_socket : maximum;
    }
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPX) != 0)
    {
        FD_SET((int)ipx_socket, &read_set);
        maximum = (int)ipx_socket > maximum ? (int)ipx_socket : maximum;
    }
    if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_ICMP) != 0)
    {
        FD_SET((int)icmp_socket, &read_set);
        maximum = (int)icmp_socket > maximum ? (int)icmp_socket : maximum;
    }

    if (!infinite)
    {
        timeout.tv_sec = (time_t)timeout_seconds;
        timeout.tv_usec = (suseconds_t)timeout_microseconds;
        timeout_ptr = &timeout;
    }

    result = select(maximum + 1, &read_set, NULL, NULL, timeout_ptr);
    *ready_sources = 0;
    if (result > 0)
    {
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPV4) != 0 && FD_ISSET((int)ipv4_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_IPV4;
        }
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_IPX) != 0 && FD_ISSET((int)ipx_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_IPX;
        }
        if ((enabled_sources & RDPLIB_PLATFORM_IO_SOURCE_ICMP) != 0 && FD_ISSET((int)icmp_socket, &read_set))
        {
            *ready_sources |= RDPLIB_PLATFORM_IO_SOURCE_ICMP;
        }
    }
    return result;
}

intptr_t rdplib_platform_serial_create_event(void)
{
    return 1;
}

int rdplib_platform_serial_close_event(intptr_t event)
{
    (void)event;
    return event != 0;
}

int rdplib_platform_serial_write(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, const void *data, uint32_t bytes, uint32_t *bytes_written, uint32_t *error_code)
{
    (void)endpoint;
    (void)async_state;
    (void)data;
    (void)bytes;
    *bytes_written = 0;
    *error_code = 1;
    return 0;
}

int rdplib_platform_serial_get_write_result(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, int wait, uint32_t *bytes_written, uint32_t *error_code)
{
    (void)endpoint;
    (void)async_state;
    (void)wait;
    *bytes_written = 0;
    *error_code = 1;
    return 0;
}

int rdplib_platform_serial_read(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, void *data, uint32_t bytes, uint32_t *bytes_read, uint32_t *error_code)
{
    (void)endpoint;
    (void)async_state;
    (void)data;
    (void)bytes;
    *bytes_read = 0;
    *error_code = 1;
    return 0;
}

int rdplib_platform_serial_get_read_result(intptr_t endpoint, rdplib_platform_serial_async_t *async_state, int wait, uint32_t *bytes_read, uint32_t *error_code)
{
    (void)endpoint;
    (void)async_state;
    (void)wait;
    *bytes_read = 0;
    *error_code = 1;
    return 0;
}

uint32_t rdplib_platform_last_system_error(void)
{
    return (uint32_t)errno;
}
