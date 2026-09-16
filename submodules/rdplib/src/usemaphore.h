// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_USEMAPHORE_H
#define RDP_USEMAPHORE_H

#include <stdint.h>

#include "layout.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <semaphore.h>
#endif

typedef struct _usemaphore_t
{
#ifdef _WIN32
    HANDLE semid;
#else
    sem_t semid;
    int created;
#endif
} usemaphore_t, *Pusemaphore_t;

#if defined(_WIN32) && !defined(_WIN64)
RDP_ASSERT_OFFSET(usemaphore_t, semid, 0x00);
RDP_STATIC_ASSERT(sizeof(usemaphore_t) == 0x04, "usemaphore_t must be 0x04 bytes on Win32");
#endif

#ifdef __cplusplus
extern "C"
{
#endif

void usemaphore_init(usemaphore_t *sem);
void usemaphore_destroy(usemaphore_t *sem);
uint32_t usemaphore_create(usemaphore_t *sem);
uint32_t usemaphore_decrement(usemaphore_t *sem, uint32_t timeout);
void usemaphore_increment(usemaphore_t *sem);

#ifdef __cplusplus
}
#endif

#endif /* RDP_USEMAPHORE_H */
