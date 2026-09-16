// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "uthread.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct rdplib_posix_thread_t
{
    pthread_t thread;
} rdplib_posix_thread_t;

static void *rdplib_posix_thread_start(void *data);

uint32_t uthread_wait_exit_code(uthread_t *thread, uint32_t *exit_code)
{
    rdplib_posix_thread_t *handle;
    void *result;
    int wait_result;

    if (thread->handle == NULL)
    {
        *exit_code = UINT32_MAX;
        return 0;
    }

    handle = (rdplib_posix_thread_t *)thread->handle;
    result = NULL;
    wait_result = pthread_join(handle->thread, &result);
    *exit_code = (uint32_t)(uintptr_t)result;
    return wait_result == 0 ? 0u : UINT32_MAX;
}

void uthread_init(uthread_t *thread)
{
    thread->handle = NULL;
}

void uthread_destroy(uthread_t *thread)
{
    rdplib_posix_thread_t *handle;

    handle = (rdplib_posix_thread_t *)thread->handle;
    if (handle != NULL)
    {
        if (pthread_equal(handle->thread, pthread_self()))
        {
            (void)pthread_detach(handle->thread);
        }
        free(handle);
    }
}

uint32_t RDP_STDCALL start_routine(void *data)
{
    uthread_t *thread;

    thread = (uthread_t *)data;
    thread->proc(thread->data);
    return 0;
}

static void *rdplib_posix_thread_start(void *data)
{
    return (void *)(uintptr_t)start_routine(data);
}

uint32_t uthread_create(uthread_t *thread, uthread_f proc, void *data)
{
    rdplib_posix_thread_t *handle;

    thread->proc = proc;
    thread->data = data;
    handle = (rdplib_posix_thread_t *)malloc(sizeof(*handle));
    if (handle == NULL)
    {
        thread->handle = NULL;
        return 1;
    }

    thread->handle = handle;
    if (pthread_create(&handle->thread, NULL, rdplib_posix_thread_start, thread) != 0)
    {
        free(handle);
        thread->handle = NULL;
        return 1;
    }
    thread->id = 0;
    return 0;
}
