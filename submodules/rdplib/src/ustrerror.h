// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_USTRERROR_H
#define RDP_USTRERROR_H

#include <stdint.h>

// net_strerror serves recovered debug and retail surviving diagnostic call
// sites. The unused system and later Mac comparison helpers remain dead code.
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef RDP_DEAD_CODE
char *sys_strerror(uint32_t error_number);
#endif
char *net_strerror(uint32_t error_number);

#ifdef RDP_DEAD_CODE
// Later Mac clients add eleven provider/service error names. This is kept as
// a separate comparison helper so the recovered May/Windows table stays exact.
char *net_strerror_mac(uint32_t error_number);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RDPLIB_DEBUG || RDPLIB_SOURCE_FAITHFUL || RDP_DEAD_CODE */

#endif /* RDP_USTRERROR_H */
