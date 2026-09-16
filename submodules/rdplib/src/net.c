// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "net.h"

#include <stddef.h>

#include "rdp.h"

// This isn't really rdp code, it's a convenience wrapper used in the reference clients retained for historical interest.

enum
{
    NET_CREATE_FLAGS = RDP_CREATE_USE_CRC | RDP_CREATE_REQUIRE_IPV4,
    NET_CONNECTION_OPTIONS = RDP_CONNECTION_FEATURE_KEEPALIVE,
    NET_APPLICATION_VALUE = 0x200,
    NET_DATA_RATE = 5120,
    NET_SEND_BUFFER_BYTES = 0x40000,
    NET_CLOSE_TIMEOUT_MS = 10000
};

// This owner is application state in the clients, not an rdp_t field. The
// wrapper supports only a single active game transport instance.
static rdp_t *net_owner;

int net_connect(connection_t **connection, char *host, int16_t port)
{
    int result;

    if (net_owner)
    {
        rdp_destroy(net_owner, 0);
        net_owner = NULL;
    }

    result = rdp_create(&net_owner, 0, 1, NET_CREATE_FLAGS);
    if (result == 0)
    {
        result = rdp_connect(net_owner, connection, host, (uint16_t)port, NET_CONNECTION_OPTIONS);
        if (result == 0)
        {
            *(uint32_t *)connection_app_ptr(*connection) = NET_APPLICATION_VALUE;
            (void)connection_set_max_data_rate(*connection, NET_DATA_RATE);
            connection_set_send_buffer_size(*connection, NET_SEND_BUFFER_BYTES);
        }
    }

    if (result != 0 && net_owner)
    {
        rdp_destroy(net_owner, 0);
        net_owner = NULL;
    }
    return result;
}

void net_shutdown(connection_t *connection)
{
    if (connection)
    {
        (void)connection_close_wait(connection, NET_CLOSE_TIMEOUT_MS, NULL);
    }
    rdp_destroy(net_owner, 0);
    net_owner = NULL;
}
