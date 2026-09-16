// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "uevent.h"

#include <errno.h>
#include <string.h>

void uevent_init(uevent_t *event)
{
    memset(event, 0, sizeof(*event));
}

uint32_t uevent_create(uevent_t *event)
{
    if (sem_init(&event->event, 0, 0) != 0)
    {
#ifdef RDPLIB_SOURCE_FAITHFUL
        return 0;
#else
        return 3;
#endif
    }

    event->created = 1;
    return 0;
}

void uevent_destroy(uevent_t *event)
{
    if (event->created)
    {
        (void)sem_destroy(&event->event);
    }
}

void uevent_signal(uevent_t *event)
{
    if (event->created)
    {
        (void)sem_post(&event->event);
    }
}

void uevent_wait(uevent_t *event)
{
    int result;

    if (!event->created)
    {
        return;
    }

    do
    {
        result = sem_wait(&event->event);
    } while (result != 0 && errno == EINTR);
}
