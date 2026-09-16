// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Absolute event deadline used as the event queue key.
#ifndef RDP_EVENT_H
#define RDP_EVENT_H

#include <stdint.h>

typedef struct _timeout_data
{
    uint32_t infinite; // Nonzero sorts after every finite event.
    uint32_t time;     // Absolute wrapping millisecond clock value.
} timeout_data, *Ptimeout_data;

#endif /* RDP_EVENT_H */
