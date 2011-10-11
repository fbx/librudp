/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_LIST_IMPL_H_
#define RUDP_LIST_IMPL_H_

#include <rudp/list.h>

#define __container_of(ptr, sample, member)                 \
    (void *)((char *)(ptr)  -                   \
         ((char *)&(sample)->member - (char *)(sample)))

#define rudp_list_for_each(pos, head, member)               \
    for (pos = 0, pos = __container_of((head)->next, pos, member);  \
         &pos->member != (head);                    \
         pos = __container_of(pos->member.next, pos, member))

#define rudp_list_for_each_safe(pos, tmp, head, member)             \
    for (pos = 0, tmp = 0,                          \
         pos = __container_of((head)->next, pos, member),       \
         tmp = __container_of((pos)->member.next, tmp, member);     \
         &pos->member != (head);                    \
         pos = tmp,                             \
         tmp = __container_of(pos->member.next, tmp, member))

#define rudp_list_for_each_reverse(pos, head, member)           \
    for (pos = 0, pos = __container_of((head)->prev, pos, member);  \
         &pos->member != (head);                    \
         pos = __container_of(pos->member.prev, pos, member))

static inline void
rudp_list_init(struct rudp_list *list)
{
    list->prev = list;
    list->next = list;
}

static inline void
rudp_list_insert(struct rudp_list *list, struct rudp_list *elm)
{
    elm->prev = list;
    elm->next = list->next;
    list->next = elm;
    elm->next->prev = elm;
}

static inline void
rudp_list_append(struct rudp_list *list, struct rudp_list *elm)
{
    elm->next = list;
    elm->prev = list->prev;
    list->prev = elm;
    elm->prev->next = elm;
}

static inline void
rudp_list_remove(struct rudp_list *elm)
{
    elm->prev->next = elm->next;
    elm->next->prev = elm->prev;
}

static inline int
rudp_list_length(struct rudp_list *list)
{
    struct rudp_list *e;
    int count;

    count = 0;
    e = list->next;
    while (e != list) {
        e = e->next;
        count++;
    }

    return count;
}

static inline int
rudp_list_empty(struct rudp_list *elm)
{
    return elm->next == elm;
}

#define rudp_list_head(head, type, member)      \
    (type*)__container_of((head)->next, (type*)0, member);

#endif
