// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Non owning connection event queue.
#ifndef RDPLIB_EVENTQ_H
#define RDPLIB_EVENTQ_H

#include <stdint.h>
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "event.h"
#include "connection.h"
#include "pqueue.h"
#include "umutex.h"

typedef struct _eventq_t
{
    pqueue_t q;
    umutex_t lock;
} eventq_t, *Peventq_t;

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t eventq_create(eventq_t *eq, uint32_t grow_size);
struct timeval *eventq_get_event_timeout(eventq_t *eq, struct timeval *timeout_ptr);

#ifdef __cplusplus
}
#endif

static void eventq_init(eventq_t *eq)
{
    memset(eq, 0, sizeof(*eq));
}

// The queue must be empty. Connections and their event links remain owned by the caller.
static void eventq_destroy(eventq_t *eq)
{
    pqueue_destroy(&eq->q);
    umutex_destroy(&eq->lock);
}

static connection_t *eventq_remove_head(eventq_t *eq)
{
    return (connection_t *)pqueue_remove_head(&eq->q);
}

static connection_t *eventq_peek_head(eventq_t *eq)
{
    return (connection_t *)pqueue_peek_head(&eq->q);
}

static uint32_t eventq_insert(eventq_t *eq, connection_t *c)
{
    return pqueue_insert(&eq->q, &c->cn_event_queue_link);
}

static connection_t *eventq_remove_by_ptr(eventq_t *eq, connection_t *c)
{
    return (connection_t *)pqueue_remove_by_link(&eq->q, &c->cn_event_queue_link);
}

static void eventq_resort_by_ptr(eventq_t *eq, connection_t *c)
{
    pqueue_resort_by_link(&eq->q, &c->cn_event_queue_link);
}

static void eventq_lock(eventq_t *eq)
{
    umutex_lock(&eq->lock);
}

static void eventq_unlock(eventq_t *eq)
{
    umutex_unlock(&eq->lock);
}

#endif /* RDPLIB_EVENTQ_H */
