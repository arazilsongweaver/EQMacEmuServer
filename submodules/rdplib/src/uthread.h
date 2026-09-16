// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_UTHREAD_H
#define RDP_UTHREAD_H

#include <stdint.h>

#include "layout.h"

#if defined(_MSC_VER)
#define RDP_CDECL __cdecl
#define RDP_STDCALL __stdcall
#elif defined(__GNUC__) && defined(_WIN32)
#define RDP_CDECL __attribute__((cdecl))
#define RDP_STDCALL __attribute__((stdcall))
#else
#define RDP_CDECL
#define RDP_STDCALL
#endif

typedef void(RDP_CDECL *uthread_f)(void *data);

typedef struct _uthread_t
{
    void *handle;
    uint32_t id;
    uthread_f proc;
    void *data;
} uthread_t, *Puthread_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(uthread_t, handle, 0x00);
RDP_ASSERT_OFFSET(uthread_t, id, 0x04);
RDP_ASSERT_OFFSET(uthread_t, proc, 0x08);
RDP_ASSERT_OFFSET(uthread_t, data, 0x0C);
RDP_STATIC_ASSERT(sizeof(uthread_t) == 0x10, "uthread_t must be 0x10 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t uthread_wait_exit_code(uthread_t *thread, uint32_t *exit_code);
void uthread_init(uthread_t *thread);
void uthread_destroy(uthread_t *thread);
uint32_t uthread_create(uthread_t *thread, uthread_f proc, void *data);

#ifdef __cplusplus
}
#endif

#endif /* RDP_UTHREAD_H */
