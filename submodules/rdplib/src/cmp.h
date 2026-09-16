// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Wrap aware fixed width comparators. Ordering is valid when keys are separated by less than half of their type's range.
#ifndef RDP_CMP_H
#define RDP_CMP_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
int uint64_cmp(const void *uint64_1, const void *uint64_2);
// Used only by the dead code DPC queue.
int uint32_cmp(const void *uint32_1, const void *uint32_2);
#endif

int uint16_cmp(const void *uint16_1, const void *uint16_2);
int uint8_cmp(const void *uint8_1, const void *uint8_2);

#ifdef __cplusplus
}
#endif

#endif /* RDP_CMP_H */
