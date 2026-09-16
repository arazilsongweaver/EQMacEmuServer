// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// Non owning intrusive priority queue used by RDP.
#ifndef RDP_PQUEUE_H
#define RDP_PQUEUE_H

#include <stdint.h>

#include "list.h"

typedef struct _qlink
{
    uint32_t index; // Trusted while the link is a member and deliberately left stale after removal.
    void *item;
    union _key key;
} qlink, *Pqlink;

typedef struct _pqueue_t
{
    qlink **array;
    uint32_t next_element;
    uint32_t array_size;
    uint32_t grow_size;
    keycmp_f keycmp;
} pqueue_t, *Ppqueue_t;

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t pqueue_create(pqueue_t *q, uint32_t grow_size, keycmp_f keycmp);
void pqueue_destroy(pqueue_t *q);

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t pqueue_get_size(pqueue_t *q);
#endif

uint32_t pqueue_insert(pqueue_t *q, qlink *link);
void *pqueue_remove_by_link(pqueue_t *q, qlink *link);
void pqueue_resort_by_link(pqueue_t *q, qlink *link);
void *pqueue_remove_head(pqueue_t *q);
void *pqueue_peek_head(pqueue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* RDP_PQUEUE_H */
