// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_LAYOUT_H
#define RDP_LAYOUT_H

#include <stddef.h>

#if defined(__cplusplus)
#define RDP_STATIC_ASSERT(condition, message) static_assert(condition, message)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define RDP_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#else
#define RDP_STATIC_ASSERT(condition, message) typedef char rdp_static_assert_##__LINE__[(condition) ? 1 : -1]
#endif

#define RDP_ASSERT_OFFSET(type, field, offset) RDP_STATIC_ASSERT(offsetof(type, field) == (offset), #type "::" #field " moved off " #offset)

#endif /* RDP_LAYOUT_H */
