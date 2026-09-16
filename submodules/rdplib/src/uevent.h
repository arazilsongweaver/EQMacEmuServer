// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_UEVENT_H
#define RDP_UEVENT_H

#include <stdint.h>

#include "layout.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <semaphore.h>
#endif

typedef struct uevent_t
{
#ifdef _WIN32
    HANDLE event;
#else
    sem_t event;
    int created;
#endif
} uevent_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(uevent_t, event, 0x00);
RDP_STATIC_ASSERT(sizeof(uevent_t) == 0x04, "uevent_t must be 0x04 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void uevent_init(uevent_t *event);
uint32_t uevent_create(uevent_t *event);
void uevent_destroy(uevent_t *event);
void uevent_signal(uevent_t *event);
void uevent_wait(uevent_t *event);

#ifdef __cplusplus
}
#endif

#endif /* RDP_UEVENT_H */
