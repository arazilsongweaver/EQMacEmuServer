// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_SERIAL_RX_H
#define RDP_SERIAL_RX_H

#include <stdint.h>

#include "layout.h"

typedef struct serial_rx_t
{
    char *head;
    uint32_t max_size;
    char *read_pos;
    char *write_pos;
    uint32_t size;
} serial_rx_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(serial_rx_t, head, 0x00);
RDP_ASSERT_OFFSET(serial_rx_t, max_size, 0x04);
RDP_ASSERT_OFFSET(serial_rx_t, read_pos, 0x08);
RDP_ASSERT_OFFSET(serial_rx_t, write_pos, 0x0C);
RDP_ASSERT_OFFSET(serial_rx_t, size, 0x10);
RDP_STATIC_ASSERT(sizeof(serial_rx_t) == 0x14, "serial_rx_t must be 0x14 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void __dpf_release(const char *fmt, ...);
#endif

void serial_rx_init(serial_rx_t *serial_rx);

// Allocates the circular store. serial_rx_init must run first because allocation failure leaves every field except head unchanged. The original result is 2 on failure.
uint32_t serial_rx_create(serial_rx_t *serial_rx, uint32_t size);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void serial_rx_clear(serial_rx_t *serial_rx);
#endif

// Releases only the byte store and clears only head. The remaining cursor, capacity, and byte count fields retain their old values.
void serial_rx_destroy(serial_rx_t *serial_rx);

// Fills free spans while the caller holds the serial lock. A pending read is waited synchronously; a short read stops the pass, while an exact span read continues after wrap.
void serial_rx_fill(serial_rx_t *serial_rx, void *file);

// Consumes exactly size bytes. A null buffer discards them. The caller must prove availability; an empty buffer with a nonzero request cannot make progress.
void serial_rx_read(serial_rx_t *serial_rx, void *buffer, uint32_t size);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
void serial_rx_write(serial_rx_t *serial_rx, void *buffer, uint32_t size);
#endif

// Copies exactly size bytes without consuming them. The caller must prove availability and provide a valid destination.
void serial_rx_peek(serial_rx_t *serial_rx, void *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* RDP_SERIAL_RX_H */
