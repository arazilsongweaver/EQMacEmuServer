// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "uevent.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <string.h>

void uevent_init(uevent_t *event)
{
    memset(event, 0, sizeof(*event));
}

uint32_t uevent_create(uevent_t *event)
{
    event->event = CreateEventA(NULL, FALSE, FALSE, NULL);
    return event->event != NULL ? 0u : 3u;
}

void uevent_destroy(uevent_t *event)
{
    uint32_t closed;

    if (event->event != NULL)
    {
        closed = (uint32_t)CloseHandle(event->event);
#ifdef RDPLIB_DEBUG
        assert(closed);
#else
        (void)closed;
#endif
    }
}

void uevent_signal(uevent_t *event)
{
    uint32_t set;

#ifdef RDPLIB_DEBUG
    assert(event->event != NULL);
#endif
    set = (uint32_t)SetEvent(event->event);
#ifdef RDPLIB_DEBUG
    assert(set);
#else
    (void)set;
#endif
}

void uevent_wait(uevent_t *event)
{
    uint32_t wait;

#ifdef RDPLIB_DEBUG
    assert(event->event != NULL);
#endif
    wait = (uint32_t)WaitForSingleObjectEx(event->event, INFINITE, FALSE);
#ifdef RDPLIB_DEBUG
    assert(wait == WAIT_OBJECT_0);
#else
    (void)wait;
#endif
}
