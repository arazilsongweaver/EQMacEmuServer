// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_USEND_H
#define RDP_USEND_H

#include <stdint.h>

#include "iov.h"

struct connection_t;
struct sockaddr;

#ifdef __cplusplus
extern "C"
{
#endif

int32_t rdp_append_crc(char *buffer, int32_t buffer_size);
int32_t rdp_encode_data(char *buffer, int32_t buffer_size);
uint32_t usend(intptr_t socket, iov_t *iov, uint32_t iov_len, struct sockaddr *remote_addr, uint32_t encrypt, uint32_t crc);

#ifndef RDPLIB_SOURCE_FAITHFUL
uint32_t rdplib_usend_framed_size(uint32_t plaintext_bytes, uint32_t encrypt, uint32_t crc);
// Maintained connected send adapter. The callback sees the joined plaintext before CRC and encryption; when accepted, that same snapshot is framed and sent.
uint32_t rdplib_usend(struct connection_t *connection, intptr_t socket, iov_t *iov, uint32_t iov_len, struct sockaddr *remote_addr, uint32_t encrypt, uint32_t crc);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RDP_USEND_H */
