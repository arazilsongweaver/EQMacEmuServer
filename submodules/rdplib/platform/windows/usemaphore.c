// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "usemaphore.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif

void usemaphore_init(usemaphore_t *sem)
{
    sem->semid = NULL;
}

void usemaphore_destroy(usemaphore_t *sem)
{
    int bClosed;

    if (sem->semid != NULL)
    {
        bClosed = CloseHandle(sem->semid);
#ifdef RDPLIB_DEBUG
        assert(bClosed);
#else
        (void)bClosed;
#endif
    }
}

uint32_t usemaphore_create(usemaphore_t *sem)
{
    uint32_t result;

    result = 0;
    sem->semid = CreateSemaphoreA(NULL, 0, INT32_MAX, NULL);
#ifdef RDPLIB_DEBUG
    assert(sem->semid != NULL);
#endif
    if (sem->semid == NULL)
    {
        result = 3;
    }
    return result;
}

uint32_t usemaphore_decrement(usemaphore_t *sem, uint32_t timeout)
{
    uint32_t allocated;
    uint32_t result;

    allocated = 0;
    result = (uint32_t)WaitForSingleObjectEx(sem->semid, (DWORD)timeout, FALSE);
    if (result == WAIT_OBJECT_0)
    {
        allocated = 1;
    }
    return allocated;
}

void usemaphore_increment(usemaphore_t *sem)
{
    int bReleased;

    bReleased = ReleaseSemaphore(sem->semid, 1, NULL);
#ifdef RDPLIB_DEBUG
    assert(bReleased);
#else
    (void)bReleased;
#endif
}
