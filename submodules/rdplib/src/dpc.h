// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_DPC_H
#define RDP_DPC_H

#ifdef RDP_DEAD_CODE

#include <stdint.h>

#include "layout.h"
#include "pqueue.h"

#if defined(_MSC_VER)
#define DPC_CDECL __cdecl
#elif defined(__GNUC__) && defined(_WIN32)
#define DPC_CDECL __attribute__((cdecl))
#else
#define DPC_CDECL
#endif

typedef struct _dpc_t dpc_t;
typedef uint32_t(DPC_CDECL *dpc_f)(dpc_t *);

struct _dpc_t
{
    qlink dpcq_link;
    uint32_t time;
    uint32_t type;
    dpc_f func;
    void *data;
};
typedef dpc_t *Pdpc_t;

typedef struct _dpcq_t
{
    pqueue_t q;
} dpcq_t, *Pdpcq_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(dpc_t, dpcq_link, 0x00);
RDP_ASSERT_OFFSET(dpc_t, time, 0x0c);
RDP_ASSERT_OFFSET(dpc_t, type, 0x10);
RDP_ASSERT_OFFSET(dpc_t, func, 0x14);
RDP_ASSERT_OFFSET(dpc_t, data, 0x18);
RDP_STATIC_ASSERT(sizeof(dpc_t) == 0x1c, "dpc_t must be 0x1c bytes on Win32");
RDP_ASSERT_OFFSET(dpcq_t, q, 0x00);
RDP_STATIC_ASSERT(sizeof(dpcq_t) == 0x14, "dpcq_t must be 0x14 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

// unused, retained for historical interest
uint32_t DPC_CDECL dpcq_create(dpcq_t *dpcq);
uint32_t DPC_CDECL dpcq_get_next_timeout(dpcq_t *dpcq);

#ifdef __cplusplus
}
#endif

static dpc_t *DPC_CDECL dpcq_peek_head(dpcq_t *dpcq)
{
    return (dpc_t *)pqueue_peek_head(&dpcq->q);
}

#undef DPC_CDECL

#endif /* RDP_DEAD_CODE */

#endif /* RDP_DPC_H */
