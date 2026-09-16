// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "bitops.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <string.h>


void bitarray_clear(bitarray_t *bitarray)
{
    memset(bitarray->bits, 0, sizeof(bitarray->bits));
    bitarray->high_byte = 0;
}

void bitarray_copy(bitarray_t *bitarray, uint8_t *dst, uint32_t start_bit, uint32_t num_bytes)
{
    uint8_t *src;
    uint8_t bit_offset;
    uint32_t whole_bytes_until_end_of_array;

    src = &bitarray->bits[start_bit >> 3];
#ifdef RDPLIB_DEBUG
    // In place shifts write toward lower addresses; a higher destination must begin beyond the copied source range.
    assert(( dst <= src ) || ( dst > src+num_bytes ));
    assert((BITARRAY_SIZE<<3) >= start_bit);
#endif
    whole_bytes_until_end_of_array = ((BITARRAY_SIZE << 3) - start_bit) >> 3;
    bit_offset = (uint8_t)(start_bit & 7u);
    if (whole_bytes_until_end_of_array > num_bytes)
    {
        whole_bytes_until_end_of_array = num_bytes;
    }
    num_bytes -= whole_bytes_until_end_of_array;

    while (whole_bytes_until_end_of_array--)
    {
        *dst++ = (uint8_t)((src[0] << bit_offset) | (src[1] >> (8u - bit_offset)));
        ++src;
    }

    if (num_bytes)
    {
        --num_bytes;
        *dst++ = (uint8_t)(*src << bit_offset);
    }
    while (num_bytes--)
    {
        *dst++ = 0;
    }
}

uint8_t bitarray_setbit(bitarray_t *bitarray, uint32_t array_index)
{
    uint8_t bit_index;
    uint8_t *byte_position;
    uint8_t result;

    byte_position = &bitarray->bits[array_index >> 3];
    bit_index = (uint8_t)(7u - (array_index & 7u));
    result = (uint8_t)((*byte_position >> bit_index) & 1u);
    *byte_position |= (uint8_t)(1u << bit_index);
    return result;
}

uint8_t bitarray_clearbit(bitarray_t *bitarray, uint32_t array_index)
{
    uint8_t bit_index;
    uint8_t *byte_position;
    uint8_t result;

    byte_position = &bitarray->bits[array_index >> 3];
    bit_index = (uint8_t)(7u - (array_index & 7u));
    result = (uint8_t)((*byte_position >> bit_index) & 1u);
    *byte_position &= (uint8_t)~(1u << bit_index);
    return result;
}

uint8_t getbit(uint8_t *byte_position, uint32_t array_index)
{
    uint8_t bit_index;
    uint8_t result;

    byte_position = &byte_position[array_index >> 3];
    bit_index = (uint8_t)(7u - (array_index & 7u));
    result = (uint8_t)((*byte_position >> bit_index) & 1u);
    return result;
}
