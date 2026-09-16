// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Game application wrapper around the recovered RDP public operations.
#ifndef RDP_NET_H
#define RDP_NET_H

#include <stdint.h>

#include "connection.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Replaces the process global owner, creates an outgoing connection, and
// applies the game clients' application value, data rate, and send buffer.
// The recovered PPC signature uses signed short here; the wrapper converts
// its low 16 bits to the unsigned port accepted by rdp_connect.
//
// This isn't really rdp code, it's a convenience wrapper used in the reference clients retained for historical interest.
int net_connect(connection_t **connection, char *host, int16_t port);

// Closes an optional connection with the clients' 10 second timeout, then
// starts asynchronous destruction of the process global owner.
void net_shutdown(connection_t *connection);

#ifdef __cplusplus
}
#endif

#endif /* RDP_NET_H */
