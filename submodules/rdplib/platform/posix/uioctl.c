// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_platform.h"

#include <fcntl.h>

uint32_t disable_blocking(intptr_t s)
{
    int flags;
    int result;

    flags = fcntl((int)s, F_GETFL, 0);
    result = flags < 0 ? -1 : fcntl((int)s, F_SETFL, flags | O_NONBLOCK);
    return result != 0;
}
