// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "connhash.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <stddef.h>
#include <string.h>

#include "rdplib_connhash.h"

enum
{
    SHIFT_SIZE = 12,
    RDP_AF_INET = 2,
    RDP_AF_IPX = 6,
    RDP_AF_COMPORT = 69
};

static uint16_t connhash_hash(connhash_t *ch, struct sockaddr *sa)
{
    uint16_t sum;
    const uint8_t *sin;
    const uint8_t *sipx;
    const uint8_t *address;

#ifdef RDPLIB_DEBUG
    assert(ch->table_size <= 1<<(SHIFT_SIZE-1));
#endif
    address = (const uint8_t *)sa;
    if (rdplib_connhash_load_u16(address) == RDP_AF_INET)
    {
        sin = address;
        sum = rdplib_connhash_load_u16(sin + 2);
        sum ^= (uint16_t)rdplib_connhash_load_u32(sin + 4);
        sum ^= (uint16_t)(rdplib_connhash_load_u32(sin + 4) >> SHIFT_SIZE);
        sum ^= (uint16_t)(rdplib_connhash_load_u32(sin + 4) >> (2 * SHIFT_SIZE));
        sum &= (uint16_t)(ch->table_size - 1);
    }
    else if (rdplib_connhash_load_u16(address) == RDP_AF_IPX)
    {
        sipx = address;
        sum = rdplib_connhash_load_u16(sipx + 12);
        sum = (uint16_t)(sum + (int8_t)sipx[2]);
        sum = (uint16_t)(sum + (int8_t)sipx[3]);
        sum = (uint16_t)(sum + (int8_t)sipx[4]);
        sum = (uint16_t)(sum + (int8_t)sipx[5]);
        sum = (uint16_t)(sum + (int8_t)sipx[6]);
        sum = (uint16_t)(sum + (int8_t)sipx[7]);
        sum = (uint16_t)(sum + (int8_t)sipx[8]);
        sum = (uint16_t)(sum + (int8_t)sipx[9]);
        sum = (uint16_t)(sum + (int8_t)sipx[10]);
        sum = (uint16_t)(sum + (int8_t)sipx[11]);
        sum &= (uint16_t)(ch->table_size - 1);
    }
    else
    {
        sum = 0;
    }
    return sum;
}

void connhash_init(connhash_t *ch)
{
    ch->table_size = 0;
    ch->table = NULL;
}

int sockaddr_cmp(const void *sockaddr_1, const void *sockaddr_2)
{
    const uint8_t *sa2;
    const uint8_t *sa1;
    int result;

    sa1 = (const uint8_t *)sockaddr_1;
    sa2 = (const uint8_t *)sockaddr_2;
    if (rdplib_connhash_load_u16(sa1) == rdplib_connhash_load_u16(sa2))
    {
        switch (rdplib_connhash_load_u16(sa1))
        {
        case RDP_AF_INET:
            result = memcmp(sa1, sa2, 8);
            break;
        case RDP_AF_IPX:
            result = memcmp(sa1, sa2, 14);
            break;
        case RDP_AF_COMPORT:
            result = memcmp(sa1, sa2, 4);
            break;
        default:
#ifdef RDPLIB_DEBUG
            assert(!"invalid sockaddr family");
#endif
#ifndef RDPLIB_SOURCE_FAITHFUL
            // Checked builds retain a deterministic ordering if assertions are disabled.
            result = memcmp(sa1, sa2, 16);
#endif
            break;
        }
    }
    else if (rdplib_connhash_load_u16(sa1) > rdplib_connhash_load_u16(sa2))
    {
        result = 1;
    }
    else
    {
        result = -1;
    }
    return result;
}

uint32_t connhash_create(connhash_t *ch, uint16_t table_size)
{
    uint32_t bin;

#ifdef RDPLIB_SOURCE_FAITHFUL
    ch->table_size = (uint16_t)(1 << (table_size - 1));
    ch->table = (hashbin_t *)rdplib_platform_malloc(sizeof(*ch->table) * ch->table_size);
    // The May 2002 code clears the allocation before checking it for failure.
    memset(ch->table, 0, sizeof(*ch->table) * ch->table_size);
    if (ch->table)
    {
        for (bin = 0; bin < ch->table_size; ++bin)
        {
            list_init(&ch->table[bin].list);
            list_create(&ch->table[bin].list, 1, sockaddr_cmp);
            umutex_create(&ch->table[bin].lock);
        }
    }
    return ch->table == NULL;
#else
    uint16_t expanded_table_size;
    hashbin_t *table;
    size_t allocation_bytes;

    if (!ch || table_size < 1 || table_size > SHIFT_SIZE)
    {
        return 1;
    }

    expanded_table_size = (uint16_t)(UINT32_C(1) << (table_size - 1u));
    allocation_bytes = (size_t)expanded_table_size * sizeof(*table);
    table = (hashbin_t *)rdplib_platform_malloc(allocation_bytes);
    if (!table)
    {
        return 1;
    }

    memset(table, 0, allocation_bytes);
    for (bin = 0; bin < expanded_table_size; ++bin)
    {
        list_init(&table[bin].list);
        list_create(&table[bin].list, 1, sockaddr_cmp);
        umutex_create(&table[bin].lock);
    }
    ch->table_size = expanded_table_size;
    ch->table = table;
    return 0;
#endif
}

void connhash_destroy(connhash_t *ch)
{
    uint32_t bin;

    if (ch->table)
    {
        for (bin = 0; bin < ch->table_size; ++bin)
        {
            list_destroy(&ch->table[bin].list);
            umutex_destroy(&ch->table[bin].lock);
        }
        rdplib_platform_free(ch->table);
        ch->table = NULL;
    }
}

connection_t *connhash_lock(connhash_t *ch, struct sockaddr *sa)
{
    uint16_t bin;
    connection_t *c;

    c = NULL;
    bin = connhash_hash(ch, sa);
    umutex_lock(&ch->table[bin].lock);
    c = (connection_t *)list_lookup(&ch->table[bin].list, sa);
    if (c)
    {
        ++c->cn_ref_count;
    }
    umutex_unlock(&ch->table[bin].lock);
    if (c)
    {
        umutex_lock(&c->cn_lock);
    }
    return c;
}

void connhash_insert(connhash_t *ch, connection_t *c)
{
    uint16_t bin;

#ifdef RDPLIB_DEBUG
    assert(c != NULL);
#endif
    bin = connhash_hash(ch, &c->tx_remote_addr);
    umutex_lock(&ch->table[bin].lock);
    c->cn_ref_count = 1;
    list_insert(&ch->table[bin].list, &c->cn_addr_map_link);
    umutex_unlock(&ch->table[bin].lock);
}

connection_t *connhash_subref(connhash_t *ch, connection_t *c)
{
    connection_t *removed;
    uint16_t bin;

    removed = NULL;
#ifdef RDPLIB_DEBUG
    assert(c != NULL);
#endif
    bin = connhash_hash(ch, &c->tx_remote_addr);
    umutex_lock(&ch->table[bin].lock);
    --c->cn_ref_count;
    if (c->cn_ref_count == 0)
    {
        removed = (connection_t *)list_remove_by_key(&ch->table[bin].list, &c->tx_remote_addr);
#ifdef RDPLIB_DEBUG
        assert(removed == c);
#endif
    }
    umutex_unlock(&ch->table[bin].lock);
    return removed;
}
