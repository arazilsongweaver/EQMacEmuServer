// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "list.h"

#ifdef RDPLIB_DEBUG
#include <assert.h>
#endif

void list_init(list_t *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->sorted = 0;
    list->keycmp = NULL;
}

void list_create(list_t *list, uint32_t sorted, keycmp_f keycmp)
{
    list->sorted = sorted;
    list->keycmp = keycmp;
#ifdef RDPLIB_DEBUG
    assert((!sorted) || (keycmp!=NULL));
#endif
}

void list_destroy(list_t *list)
{
#ifdef RDPLIB_DEBUG
    assert(list->head == NULL);
    assert(list->tail == NULL);
    assert(list->size == 0);
#endif
    (void)list;
}

void list_add_head(list_t *list, rdp_link_t *link)
{
#ifdef RDPLIB_DEBUG
    assert((!list->sorted) || (list->head==NULL) || (list->keycmp(link->key.p,list->head->key.p)<=0));
#endif

    link->next = list->head;
    link->prev = NULL;
    list->head = link;

    if (!list->tail)
    {
        list->tail = link;
    }
    else
    {
        link->next->prev = link;
    }

    ++list->size;
}

void list_add_tail(list_t *list, rdp_link_t *link)
{
#ifdef RDPLIB_DEBUG
    assert((!list->sorted) || (list->tail==NULL) || (list->keycmp(link->key.p,list->tail->key.p)>=0));
#endif

    link->next = NULL;
    link->prev = list->tail;
    list->tail = link;

    if (!list->head)
    {
        list->head = link;
    }
    else
    {
        link->prev->next = link;
    }

    ++list->size;
}

void *list_remove_head(list_t *list)
{
    void *item;

    item = NULL;
    if (list->head)
    {
        item = list->head->item;
        list->head = list->head->next;
        if (list->head)
        {
            list->head->prev = NULL;
        }
        else
        {
            list->tail = NULL;
        }
        --list->size;
    }
    return item;
}

void *list_remove_tail(list_t *list)
{
    void *item;

    item = NULL;
    if (list->tail)
    {
        item = list->tail->item;
        list->tail = list->tail->prev;
        if (list->tail)
        {
            list->tail->next = NULL;
        }
        else
        {
            list->head = NULL;
        }
        --list->size;
    }
    return item;
}

void list_insert(list_t *list, rdp_link_t *link)
{
    rdp_link_t *current;

    if ((list->head == NULL) || (list->keycmp(list->head->key.p, link->key.p) > 0))
    {
        list_add_head(list, link);
    }
    else if (list->keycmp(list->tail->key.p, link->key.p) <= 0)
    {
        list_add_tail(list, link);
    }
    else
    {
        current = list->head->next;
#ifdef RDPLIB_DEBUG
        assert(current != NULL);
#endif
        while (list->keycmp(current->key.p, link->key.p) <= 0)
        {
            current = current->next;
        }

        link->next = current;
        link->prev = current->prev;
        current->prev = link;
        link->prev->next = link;
#ifdef RDPLIB_DEBUG
        assert(current != NULL);
#endif
        ++list->size;
    }
}

rdp_link_t *list_find_link_by_key(list_t *list, const void *key)
{
    int diff;
    rdp_link_t *current;

    for (current = list->head; current; current = current->next)
    {
        diff = list->keycmp(current->key.p, key);
        if (diff == 0)
        {
            break;
        }
        if (list->sorted && diff > 0)
        {
            return NULL;
        }
    }

    return current;
}

void *list_remove_by_link(list_t *list, rdp_link_t *link)
{
    void *item;

    if (link->prev == NULL)
    {
#ifdef RDPLIB_DEBUG
        assert(list->head == link);
#endif
        return list_remove_head(list);
    }
    if (link->next == NULL)
    {
#ifdef RDPLIB_DEBUG
        assert(list->tail == link);
#endif
        return list_remove_tail(list);
    }

    link->prev->next = link->next;
    link->next->prev = link->prev;
    item = link->item;
    --list->size;
    return item;
}
