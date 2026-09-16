// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "usemaphore.h"

#include <errno.h>
#include <string.h>
#include <time.h>

static uint32_t usemaphore_wait_without_timeout(usemaphore_t *sem);

void usemaphore_init(usemaphore_t *sem)
{
    memset(sem, 0, sizeof(*sem));
}

void usemaphore_destroy(usemaphore_t *sem)
{
    if (sem->created)
    {
        (void)sem_destroy(&sem->semid);
    }
}

uint32_t usemaphore_create(usemaphore_t *sem)
{
    uint32_t result;

    result = 0;
    if (sem_init(&sem->semid, 0, 0) != 0)
    {
        result = 3;
    }
    else
    {
        sem->created = 1;
    }
    return result;
}

uint32_t usemaphore_decrement(usemaphore_t *sem, uint32_t timeout)
{
    uint32_t allocated;
    int result;
    struct timespec deadline;

    allocated = 0;
    if (timeout == UINT32_MAX)
    {
        return usemaphore_wait_without_timeout(sem);
    }
    if (timeout == 0)
    {
        return sem_trywait(&sem->semid) == 0;
    }

    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t)(timeout / 1000u);
    deadline.tv_nsec += (long)(timeout % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L)
    {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }

    do
    {
        result = sem_timedwait(&sem->semid, &deadline);
    } while (result != 0 && errno == EINTR);
    if (result == 0)
    {
        allocated = 1;
    }
    return allocated;
}

void usemaphore_increment(usemaphore_t *sem)
{
    (void)sem_post(&sem->semid);
}

static uint32_t usemaphore_wait_without_timeout(usemaphore_t *sem)
{
    int result;

    do
    {
        result = sem_wait(&sem->semid);
    } while (result != 0 && errno == EINTR);

    return result == 0;
}
