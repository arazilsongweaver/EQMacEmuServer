// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdplib_platform.h"

uint32_t disable_blocking(intptr_t s)
{
    int result;
    u_long on;

    on = 1;
    result = ioctlsocket((SOCKET)s, FIONBIO, &on);
    return result != 0;
}
