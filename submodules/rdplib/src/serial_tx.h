// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_SERIAL_TX_H
#define RDP_SERIAL_TX_H

#include <stdint.h>

#include "layout.h"
#include "list.h"
#include "rdplib_platform.h"

typedef struct serial_tx_buf_t
{
    rdp_link_t link;
    rdplib_platform_serial_async_t o; // Platform equivalent of the embedded 32 bit OVERLAPPED.
    uint32_t write_size;
    uint32_t start_time;
} serial_tx_buf_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(serial_tx_buf_t, link, 0x00);
RDP_ASSERT_OFFSET(serial_tx_buf_t, o, 0x10);
RDP_ASSERT_OFFSET(serial_tx_buf_t, write_size, 0x24);
RDP_ASSERT_OFFSET(serial_tx_buf_t, start_time, 0x28);
RDP_STATIC_ASSERT(sizeof(serial_tx_buf_t) == 0x2C, "serial_tx_buf_t must be 0x2c bytes on Win32");
#endif

#endif /* RDP_SERIAL_TX_H */
