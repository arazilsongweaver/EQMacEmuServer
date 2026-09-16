// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "uthread.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#include "dpf.h"
#endif

#include <process.h>
#include <windows.h>

uint32_t uthread_wait_exit_code(uthread_t *thread, uint32_t *exit_code)
{
    int32_t result;
    DWORD dwResult;
    DWORD long_code;

    if (thread->handle == NULL)
    {
        *exit_code = UINT32_MAX;
        result = 0;
    }
    else
    {
        dwResult = WaitForSingleObject((HANDLE)thread->handle, INFINITE);
#ifdef RDPLIB_DEBUG
        assert(dwResult != WAIT_FAILED);
#endif
        (void)dwResult;
        result = (int32_t)GetExitCodeThread((HANDLE)thread->handle, &long_code) - 1;
        *exit_code = (uint32_t)long_code;
    }

    if (result != 0)
    {
#ifdef RDPLIB_DEBUG
        if (GetLastError() == STILL_ACTIVE)
        {
            dpf(0x20u, "GetLastError() == STILL_ACTIVE\n");
        }
#elif defined(RDPLIB_SOURCE_FAITHFUL)
        (void)GetLastError();
#endif
    }
    else if (*exit_code == STILL_ACTIVE)
    {
#ifdef RDPLIB_DEBUG
        dpf(0x20u, "*exit_code == STILL_ACTIVE\n");
#endif
        result = -1;
    }

    return (uint32_t)result;
}

void uthread_init(uthread_t *thread)
{
    thread->handle = NULL;
}

void uthread_destroy(uthread_t *thread)
{
    uint32_t closed;

    if (thread->handle != NULL)
    {
        closed = (uint32_t)CloseHandle((HANDLE)thread->handle);
#ifdef RDPLIB_DEBUG
        assert(closed);
#endif
        (void)closed;
    }
}

uint32_t RDP_STDCALL start_routine(void *data)
{
    uthread_t *thread;

    thread = (uthread_t *)data;
    thread->proc(thread->data);
    return 0;
}

uint32_t uthread_create(uthread_t *thread, uthread_f proc, void *data)
{
    int bSet;

    thread->proc = proc;
    thread->data = data;
    thread->handle = (void *)_beginthreadex(NULL, 0, start_routine, thread, 0, (unsigned int *)&thread->id);
    if (thread->handle != NULL)
    {
        bSet = SetThreadPriority((HANDLE)thread->handle, THREAD_PRIORITY_HIGHEST);
#ifdef RDPLIB_DEBUG
        assert(bSet);
#endif
        (void)bSet;
    }
    return thread->handle == NULL;
}
