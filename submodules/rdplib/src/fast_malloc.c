// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "fast.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <string.h>

#ifdef RDPLIB_DEBUG
#include "dpf.h"
#endif
#include "list.h"
#include "rdplib_platform.h"
#include "umutex.h"

#define FAST_MALLOC_POOL_COUNT 4u
#define FAST_MALLOC_BLOCK_HEADER_SIZE 8u
// The May 2002 Windows link occupied 16 bytes. A native link is required here so its pointers remain valid on 64 bit hosts.
#define LINK_SIZE ((uint32_t)sizeof(rdp_link_t))

struct _fast_malloc_block_header_t
{
    uint32_t pool;
    uint32_t size;
};

typedef struct _fast_malloc_pool_t
{
    uint32_t raw_size;
    uint32_t avail_size;
    uint32_t chunks;
    list_t list;
    umutex_t lock;
} fast_malloc_pool_t;

uint32_t g_fast_malloc_ready = 0;
list_t g_malloc_memory;
fast_malloc_pool_t g_fast_malloc_pool[FAST_MALLOC_POOL_COUNT];

static void fast_malloc_grow(uint32_t expected_usage);
static char *fast_malloc_raw(uint32_t pool);

#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)
uint32_t fast_malloc_usage(void)
{
    uint32_t chunks;
    uint32_t pool;

    chunks = 0;
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif
    for (pool = 0; pool < FAST_MALLOC_POOL_COUNT; ++pool)
    {
        chunks = g_fast_malloc_pool[pool].chunks + (chunks >> 1);
    }
#ifdef RDPLIB_DEBUG
    dpf(0x200u, "peak usage: %u used, %u expected\n", 584u * chunks, g_fast_malloc_ready);
#endif
    return 584u * chunks;
}
#endif

static void fast_malloc_grow(uint32_t expected_usage)
{
    char *buf;
    rdp_link_t *master_chunk;
    uint32_t size;

#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif
#ifdef RDPLIB_SOURCE_FAITHFUL
    expected_usage = 584u * (expected_usage / 584u + (expected_usage % 584u != 0));
#else
    // Retain the initial maintained implementation's rounding expression.
    expected_usage = ((expected_usage + 583u) / 584u) * 584u;
#endif
    master_chunk = (rdp_link_t *)rdplib_platform_malloc((size_t)expected_usage + LINK_SIZE);

    // clear backing storage - the mac clients did this but the windows clients did not.
    memset(master_chunk, 0, (size_t)expected_usage + LINK_SIZE);

    master_chunk->item = master_chunk;
    list_add_tail(&g_malloc_memory, master_chunk);

    buf = (char *)master_chunk;
    buf += LINK_SIZE;
    for (size = 0; size < expected_usage; size += 584u)
    {
        rdp_link_t *chunk;

        chunk = (rdp_link_t *)&buf[size];
        chunk->item = chunk;
        list_add_tail(&g_fast_malloc_pool[FAST_MALLOC_POOL_COUNT - 1u].list, chunk);
        ++g_fast_malloc_pool[FAST_MALLOC_POOL_COUNT - 1u].chunks;
    }

#ifdef RDPLIB_DEBUG
    dpf(0x200u, "%u large chunks allocated\n", expected_usage / 584u);
#endif
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
    (void)fast_malloc_usage();
#endif
}

void fast_malloc_init(uint32_t expected_usage)
{
    uint32_t raw_size;
    uint32_t pool;

#ifdef RDPLIB_DEBUG
    assert(!g_fast_malloc_ready);
#endif
    g_fast_malloc_ready = expected_usage;
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif

    list_init(&g_malloc_memory);
    list_create(&g_malloc_memory, 0, NULL);

    raw_size = 584u;
    pool = FAST_MALLOC_POOL_COUNT;
    do
    {
        --pool;
        list_init(&g_fast_malloc_pool[pool].list);
        list_create(&g_fast_malloc_pool[pool].list, 0, NULL);
        umutex_create(&g_fast_malloc_pool[pool].lock);

        g_fast_malloc_pool[pool].raw_size = raw_size;
#ifdef RDPLIB_DEBUG
        assert(g_fast_malloc_pool[pool].raw_size > FAST_MALLOC_BLOCK_HEADER_SIZE);
        assert(g_fast_malloc_pool[pool].raw_size > LINK_SIZE);
#endif
        g_fast_malloc_pool[pool].avail_size = (raw_size - FAST_MALLOC_BLOCK_HEADER_SIZE) & ~UINT32_C(7);
#ifdef RDPLIB_DEBUG
        assert(g_fast_malloc_pool[pool].avail_size > 0);
#endif

#if !defined(RDPLIB_DEBUG) && !defined(RDPLIB_SOURCE_FAITHFUL)
        // Reset diagnostic accounting when a maintained runtime is destroyed and initialized again.
        g_fast_malloc_pool[pool].chunks = 0;
#endif
        raw_size >>= 1;
        raw_size &= ~UINT32_C(7);
#ifdef RDPLIB_DEBUG
        dpf(0x220u, "pool %u raw_size: %u avail_size: %u\n", pool, g_fast_malloc_pool[pool].raw_size, g_fast_malloc_pool[pool].avail_size);
#endif
    }
    while (pool != 0);

    fast_malloc_grow(expected_usage);
}

void fast_malloc_destroy(void)
{
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
    uint32_t chunks;
#endif
    uint32_t pool;
    void *ptr;

#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
    chunks = 0;
#endif
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif
    while ((ptr = list_remove_head(&g_malloc_memory)) != NULL)
    {
        rdplib_platform_free(ptr);
    }

    for (pool = 0; pool < FAST_MALLOC_POOL_COUNT; ++pool)
    {
#ifdef RDPLIB_DEBUG
        if (g_fast_malloc_pool[pool].chunks == list_get_size(&g_fast_malloc_pool[pool].list))
        {
            dpf(0x220u, "pool %u chunk peak %u\n", pool, g_fast_malloc_pool[pool].chunks);
        }
        else
        {
            dpf(0x220u, "pool %u chunk leak %u of %u chunks returned!\n", pool, g_fast_malloc_pool[pool].chunks,
                list_get_size(&g_fast_malloc_pool[pool].list));
        }
#endif
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL)
        chunks = g_fast_malloc_pool[pool].chunks + (chunks >> 1);
#endif
        umutex_destroy(&g_fast_malloc_pool[pool].lock);
    }

#ifdef RDPLIB_DEBUG
    dpf(0x220u, "peak usage: %u used, %u expected\n", 584u * chunks, g_fast_malloc_ready);
#elif defined(RDPLIB_SOURCE_FAITHFUL)
    (void)chunks;
#endif
    g_fast_malloc_ready = 0;
}

static char *fast_malloc_raw(uint32_t pool)
{
    char *buf;

#ifdef RDPLIB_DEBUG
    assert(pool < FAST_MALLOC_POOL_COUNT);
#endif
    umutex_lock(&g_fast_malloc_pool[pool].lock);
    buf = (char *)list_remove_head(&g_fast_malloc_pool[pool].list);
    if (buf == NULL)
    {
        if (pool + 1u == FAST_MALLOC_POOL_COUNT)
        {
#ifdef RDPLIB_DEBUG
            dpf(0x200u, "pool %u empty, pulling from heap\n", pool);
#endif
            fast_malloc_grow(g_fast_malloc_pool[pool].raw_size);
            buf = (char *)list_remove_head(&g_fast_malloc_pool[pool].list);
        }
        else
        {
            rdp_link_t *chunk;

#ifdef RDPLIB_DEBUG
            dpf(0x200u, "pool %u empty, pulling from larger pool\n", pool);
#endif
            buf = fast_malloc_raw(pool + 1u);

            umutex_lock(&g_fast_malloc_pool[pool + 1u].lock);
            --g_fast_malloc_pool[pool + 1u].chunks;
            umutex_unlock(&g_fast_malloc_pool[pool + 1u].lock);

            g_fast_malloc_pool[pool].chunks += 2u;
#ifdef RDPLIB_DEBUG
            assert(g_fast_malloc_pool[pool].raw_size * 2 <= g_fast_malloc_pool[pool+1].raw_size);
#endif
            chunk = (rdp_link_t *)&buf[g_fast_malloc_pool[pool].raw_size];
            chunk->item = chunk;
            list_add_head(&g_fast_malloc_pool[pool].list, chunk);
        }
    }
    umutex_unlock(&g_fast_malloc_pool[pool].lock);
    return buf;
}

void *fast_malloc(uint32_t size)
{
    uint32_t pool;
    // Every historical path assigns buf, but initialize it to suppress static analysis warnings
    char *buf = NULL;

#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif
    for (pool = 0; pool < FAST_MALLOC_POOL_COUNT; ++pool)
    {
        if (size <= g_fast_malloc_pool[pool].avail_size)
        {
#ifdef RDPLIB_DEBUG
            dpf(0x200u, "allocating chunk from pool %u\n", pool);
#endif
            buf = fast_malloc_raw(pool);
            break;
        }
    }

    if (pool == FAST_MALLOC_POOL_COUNT)
    {
#if SIZE_MAX <= UINT32_MAX && !defined(RDPLIB_SOURCE_FAITHFUL)
        if (size > SIZE_MAX - FAST_MALLOC_BLOCK_HEADER_SIZE)
        {
            return NULL;
        }
#endif
        buf = (char *)rdplib_platform_malloc((size_t)size + FAST_MALLOC_BLOCK_HEADER_SIZE);
    }

    if (buf != NULL)
    {
        struct _fast_malloc_block_header_t *header;

        header = (struct _fast_malloc_block_header_t *)buf;
        header->pool = pool;
        if (pool == FAST_MALLOC_POOL_COUNT)
        {
            header->size = size;
        }
        else
        {
            header->size = g_fast_malloc_pool[pool].avail_size;
        }
        buf += FAST_MALLOC_BLOCK_HEADER_SIZE;
    }
    return buf;
}

#if defined(RDPLIB_DEBUG) || defined(RDP_DEAD_CODE)
uint32_t fast_memory(void *ptr)
{
    char *buf;
    struct _fast_malloc_block_header_t *header;

    buf = (char *)ptr;
    header = (struct _fast_malloc_block_header_t *)(buf - FAST_MALLOC_BLOCK_HEADER_SIZE);
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
#endif

    return (header->pool == FAST_MALLOC_POOL_COUNT && header->size > g_fast_malloc_pool[FAST_MALLOC_POOL_COUNT - 1u].avail_size) ||
           (header->pool < FAST_MALLOC_POOL_COUNT && header->size == g_fast_malloc_pool[header->pool].avail_size);
}
#endif

#if defined(RDPLIB_DEBUG) || defined(RDP_DEAD_CODE)
// Present in the recovered May debug object and retained with dead code.
uint32_t fast_size(void *ptr)
{
    char *buf;
    struct _fast_malloc_block_header_t *header;

    buf = (char *)ptr;
    header = (struct _fast_malloc_block_header_t *)(buf - FAST_MALLOC_BLOCK_HEADER_SIZE);
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
    assert(fast_memory(ptr));
#endif
    return header->size;
}
#endif

void fast_free(void *ptr)
{
    rdp_link_t *chunk;
    uint32_t pool;
    char *buf;
    struct _fast_malloc_block_header_t *header;

    buf = (char *)ptr;
    header = (struct _fast_malloc_block_header_t *)(buf - FAST_MALLOC_BLOCK_HEADER_SIZE);
#ifdef RDPLIB_DEBUG
    assert(g_fast_malloc_ready);
    assert(fast_memory(ptr));
#endif

    pool = header->pool;
    if (pool == FAST_MALLOC_POOL_COUNT)
    {
        rdplib_platform_free(header);
    }
    else
    {
        chunk = (rdp_link_t *)header;
        chunk->item = chunk;
        umutex_lock(&g_fast_malloc_pool[pool].lock);
        list_add_head(&g_fast_malloc_pool[pool].list, chunk);
        umutex_unlock(&g_fast_malloc_pool[pool].lock);
    }
}
