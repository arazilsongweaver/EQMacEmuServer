// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_UMUTEX_H
#define RDP_UMUTEX_H

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif

#include "layout.h"
#include "rdplib_platform.h"

#ifdef _WIN32

#ifdef RDPLIB_DEBUG
#define RDP_WIN32_UMUTEX_OWNER_BYTES ((size_t)sizeof(void *))
#else
#define RDP_WIN32_UMUTEX_OWNER_BYTES ((size_t)0)
#endif

typedef struct _umutex_t
{
    CRITICAL_SECTION cs;
#ifdef RDPLIB_DEBUG
    void *owner;
#endif
} umutex_t, *Pumutex_t;

#if !defined(_WIN64)
#ifdef RDPLIB_DEBUG
RDP_ASSERT_OFFSET(umutex_t, owner, 0x18);
RDP_STATIC_ASSERT(sizeof(umutex_t) == 0x1c, "debug umutex_t must be 0x1c bytes on Win32");
#else
RDP_STATIC_ASSERT(sizeof(umutex_t) == 0x18, "retail umutex_t must be 0x18 bytes on Win32");
#endif
#endif

static void umutex_create(umutex_t *l)
{
    InitializeCriticalSection(&l->cs);
#ifdef RDPLIB_DEBUG
    l->owner = NULL;
#endif
}

#ifdef RDPLIB_DEBUG
static unsigned long umutex_owner(umutex_t *l)
{
    return l->owner == GetCurrentThread();
}
#endif

static void umutex_destroy(umutex_t *l)
{
#ifdef RDPLIB_DEBUG
    assert(l->owner == NULL);
#endif
    DeleteCriticalSection(&l->cs);
}

static void umutex_lock(umutex_t *l)
{
    EnterCriticalSection(&l->cs);
#ifdef RDPLIB_DEBUG
    assert(l->owner == NULL);
    l->owner = GetCurrentThread();
#endif
}

static void umutex_unlock(umutex_t *l)
{
#ifdef RDPLIB_DEBUG
    assert(l->owner == GetCurrentThread());
    l->owner = NULL;
#endif
    LeaveCriticalSection(&l->cs);
}

#else

// The Mac transport used native pthread mutexes. This adapter keeps the
// maintained POSIX initialization guards and records ownership only when the
// recovered debug precondition checks are enabled.
typedef struct _umutex_t
{
    rdplib_platform_mutex_t platform;
#ifdef RDPLIB_DEBUG
    pthread_t owner;
    int owned;
#endif
} umutex_t, *Pumutex_t;

static inline void umutex_create(umutex_t *l)
{
    rdplib_platform_mutex_init(&l->platform);
#ifdef RDPLIB_DEBUG
    l->owned = 0;
#endif
}

#ifdef RDPLIB_DEBUG
static inline unsigned long umutex_owner(umutex_t *l)
{
    return l->platform.initialized && l->owned && pthread_equal(l->owner, pthread_self());
}
#endif

static inline void umutex_destroy(umutex_t *l)
{
    if (!l->platform.initialized)
    {
        return;
    }
#ifdef RDPLIB_DEBUG
    assert(!l->owned);
#endif
    rdplib_platform_mutex_destroy(&l->platform);
#ifdef RDPLIB_DEBUG
    l->owned = 0;
#endif
}

static inline void umutex_lock(umutex_t *l)
{
    if (!l->platform.initialized)
    {
        return;
    }
    rdplib_platform_mutex_lock(&l->platform);
#ifdef RDPLIB_DEBUG
    assert(!l->owned);
    l->owner = pthread_self();
    l->owned = 1;
#endif
}

static inline void umutex_unlock(umutex_t *l)
{
    if (!l->platform.initialized)
    {
        return;
    }
#ifdef RDPLIB_DEBUG
    assert(l->owned && pthread_equal(l->owner, pthread_self()));
    l->owned = 0;
#endif
    rdplib_platform_mutex_unlock(&l->platform);
}

#endif

#endif /* RDP_UMUTEX_H */
