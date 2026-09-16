// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// bitarray_t stores 4096 bits followed by a zero dword; copies may read the following byte, and scans use it as a zero sentinel.
// Mutation indices must be below 4096; copy starts and contiguous reads may use 4096 for the sentinel.
// Callers must provide valid storage; copies may write in place or toward lower addresses, but not into a higher overlapping range.
#ifndef RDP_BITOPS_H
#define RDP_BITOPS_H

#include <stdint.h>

#define BITARRAY_SIZE 512u
#define RDP_BITARRAY_BITS (BITARRAY_SIZE * 8u)

typedef struct _bitarray_t
{
    uint8_t bits[BITARRAY_SIZE];
    uint32_t high_byte;
} bitarray_t, *Pbitarray_t;

#ifdef __cplusplus
extern "C"
{
#endif

void bitarray_clear(bitarray_t *bitarray);
void bitarray_copy(bitarray_t *bitarray, uint8_t *dst, uint32_t start_bit, uint32_t num_bytes);
uint8_t bitarray_setbit(bitarray_t *bitarray, uint32_t array_index);
uint8_t bitarray_clearbit(bitarray_t *bitarray, uint32_t array_index);
uint8_t getbit(uint8_t *byte_position, uint32_t array_index);

static void bitarray_left_shift(bitarray_t *bitarray, uint32_t distance)
{
    bitarray_copy(bitarray, bitarray->bits, distance, BITARRAY_SIZE);
}

static uint8_t bitarray_getbit(bitarray_t *bitarray, uint32_t array_index)
{
    return getbit(bitarray->bits, array_index);
}

#ifdef __cplusplus
}
#endif

#endif /* RDP_BITOPS_H */
