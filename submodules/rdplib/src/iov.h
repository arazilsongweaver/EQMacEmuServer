// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_IOV_H
#define RDP_IOV_H

#include <stdint.h>

#include "layout.h"

typedef struct iov_t
{
    void *data;
    uint32_t size;
} iov_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(iov_t, data, 0x00);
RDP_ASSERT_OFFSET(iov_t, size, 0x04);
RDP_STATIC_ASSERT(sizeof(iov_t) == 0x08, "iov_t must be 0x08 bytes on Win32");
#endif

#endif /* RDP_IOV_H */
