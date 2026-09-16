// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "cmp.h"

#include <stdint.h>

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
int uint64_cmp(const void *uint64_1, const void *uint64_2)
{
    int64_t diff;
    int result = 0;

    diff = (int64_t)(*(const uint64_t *)uint64_1 - *(const uint64_t *)uint64_2);
    if (diff > 0)
    {
        result = 1;
    }
    else if (diff < 0)
    {
        result = -1;
    }
    return result;
}

// Used only by the dead code DPC queue.
int uint32_cmp(const void *uint32_1, const void *uint32_2)
{
    int32_t result;

    result = (int32_t)(*(const uint32_t *)uint32_1 - *(const uint32_t *)uint32_2);
    if (result > 0)
    {
        result = 1;
    }
    else if (result < 0)
    {
        result = -1;
    }
    return result;
}
#endif

int uint16_cmp(const void *uint16_1, const void *uint16_2)
{
    int16_t result;

    result = (int16_t)(*(const uint16_t *)uint16_1 - *(const uint16_t *)uint16_2);
    if (result > 0)
    {
        result = 1;
    }
    else if (result < 0)
    {
        result = -1;
    }
    return result;
}

int uint8_cmp(const void *uint8_1, const void *uint8_2)
{
    int8_t result;

    result = (int8_t)(*(const uint8_t *)uint8_1 - *(const uint8_t *)uint8_2);
    if (result > 0)
    {
        result = 1;
    }
    else if (result < 0)
    {
        result = -1;
    }
    return result;
}
