// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Maintained declarations for the recovered fixed class allocator.
// Original header ownership and filename are unproven.
#ifndef RDP_FAST_H
#define RDP_FAST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void fast_malloc_init(uint32_t expected_usage);
void fast_malloc_destroy(void);
void *fast_malloc(uint32_t size);
#if defined(RDPLIB_DEBUG) || defined(RDP_DEAD_CODE)
uint32_t fast_memory(void *ptr);
#endif
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)
uint32_t fast_malloc_usage(void);
#endif
#if defined(RDPLIB_DEBUG) || defined(RDP_DEAD_CODE)
uint32_t fast_size(void *ptr);
#endif
void fast_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* RDP_FAST_H */
