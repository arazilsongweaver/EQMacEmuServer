// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_random.h"

#include <limits.h>

#include "rdplib_platform.h"

int rdplib_random_next(void)
{
    return (int)(rdplib_platform_random_u32() & (uint32_t)INT_MAX);
}
