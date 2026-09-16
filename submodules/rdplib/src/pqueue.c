// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "pqueue.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif
#include <stdlib.h>

static void pqueue_resort_index(pqueue_t *q, uint32_t element);
static void *pqueue_remove_index(pqueue_t *q, uint32_t element);
static void pqueue_siftup(pqueue_t *q, uint32_t element);
static void pqueue_siftdown(pqueue_t *q, uint32_t parent);

uint32_t pqueue_create(pqueue_t *q, uint32_t grow_size, keycmp_f keycmp)
{
    uint32_t result;

    result = 0;
    q->grow_size = grow_size;
    q->next_element = 0;
    q->array_size = grow_size;
    q->keycmp = keycmp;
    q->array = (qlink **)malloc(sizeof(*q->array) * q->grow_size);
#ifdef RDPLIB_DEBUG
    assert(q->array != NULL);
#endif
    if (q->array == NULL)
    {
        result = 1;
    }
    return result;
}

void pqueue_destroy(pqueue_t *q)
{
#ifdef RDPLIB_DEBUG
    assert(q->next_element == 0);
#endif
    if (q->array)
    {
        free(q->array);
        q->array = NULL;
    }
}

#ifdef RDP_DEAD_CODE
// unused, retained for historical interest
uint32_t pqueue_get_size(pqueue_t *q)
{
    return q->next_element;
}
#endif

static void pqueue_resort_index(pqueue_t *q, uint32_t element)
{
#ifdef RDPLIB_DEBUG
    assert(element < q->next_element);
#endif
    pqueue_siftup(q, element);
    pqueue_siftdown(q, element);
}

static void *pqueue_remove_index(pqueue_t *q, uint32_t element)
{
    void *item;

#ifdef RDPLIB_DEBUG
    assert(element < q->next_element);
#endif
    item = q->array[element]->item;
    --q->next_element;
    if (q->next_element != element)
    {
        q->array[element] = q->array[q->next_element];
        q->array[element]->index = element;
        pqueue_siftdown(q, element);
        pqueue_siftup(q, element);
    }
    return item;
}

static void pqueue_siftup(pqueue_t *q, uint32_t element)
{
    uint32_t parent;
    qlink *tmp_link;

    while (element)
    {
        parent = (element - 1u) >> 1;
        if (q->keycmp(q->array[element]->key.p, q->array[parent]->key.p) >= 0)
        {
            break;
        }

        tmp_link = q->array[element];
        q->array[element] = q->array[parent];
        q->array[parent] = tmp_link;
        q->array[element]->index = element;
        q->array[parent]->index = parent;
        element = parent;
    }
}

static void pqueue_siftdown(pqueue_t *q, uint32_t parent)
{
    uint32_t swap_element;
    int right_cmp;
    qlink *tmp_link;
    int left_right_cmp;
    uint32_t right_child;
    uint32_t left_child;
    int left_cmp;

    for (;;)
    {
        left_child = 2u * parent + 1u;
        right_child = 2u * parent + 2u;
        if (left_child >= q->next_element)
        {
            break;
        }

        left_cmp = q->keycmp(q->array[parent]->key.p, q->array[left_child]->key.p);
        if (right_child >= q->next_element)
        {
            if (left_cmp > 0)
            {
                tmp_link = q->array[parent];
                q->array[parent] = q->array[left_child];
                q->array[left_child] = tmp_link;
                q->array[parent]->index = parent;
                q->array[left_child]->index = left_child;
            }
            return;
        }

        right_cmp = q->keycmp(q->array[parent]->key.p, q->array[right_child]->key.p);
        if (left_cmp <= 0 && right_cmp <= 0)
        {
            return;
        }

        left_right_cmp = q->keycmp(q->array[left_child]->key.p, q->array[right_child]->key.p);
        if (left_right_cmp < 0)
        {
            swap_element = left_child;
        }
        else
        {
            swap_element = right_child;
        }

        tmp_link = q->array[parent];
        q->array[parent] = q->array[swap_element];
        q->array[swap_element] = tmp_link;
        q->array[parent]->index = parent;
        q->array[swap_element]->index = swap_element;
        parent = swap_element;
    }
}

uint32_t pqueue_insert(pqueue_t *q, qlink *link)
{
    qlink **new_array;
    uint32_t result;

    result = 0;
    if (q->next_element == q->array_size)
    {
        new_array = (qlink **)realloc(q->array, sizeof(*q->array) * (q->array_size + q->grow_size));
#ifdef RDPLIB_DEBUG
        assert(new_array != NULL);
#endif
        if (new_array == NULL)
        {
            return 1;
        }
        q->array = new_array;
        q->array_size += q->grow_size;
    }

    q->array[q->next_element] = link;
    link->index = q->next_element;
    pqueue_siftup(q, q->next_element);
    ++q->next_element;
    return result;
}

void *pqueue_remove_by_link(pqueue_t *q, qlink *link)
{
    return pqueue_remove_index(q, link->index);
}

void pqueue_resort_by_link(pqueue_t *q, qlink *link)
{
    pqueue_resort_index(q, link->index);
}

void *pqueue_remove_head(pqueue_t *q)
{
    void *item;

    item = NULL;
    if (q->next_element)
    {
        item = pqueue_remove_index(q, 0);
    }
    return item;
}

void *pqueue_peek_head(pqueue_t *q)
{
    void *item;

    item = NULL;
    if (q->next_element)
    {
        item = q->array[0]->item;
    }
    return item;
}
