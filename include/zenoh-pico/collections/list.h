/*
 * Copyright (c) 2017, 2021 ADLINK Technology Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
 * which is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *
 * Contributors:
 *   ADLINK zenoh team, <zenoh@adlink-labs.tech>
 */

#ifndef ZENOH_PICO_COLLECTIONS_LIST_H
#define ZENOH_PICO_COLLECTIONS_LIST_H

#include "zenoh-pico/collections/element.h"

/*-------- Single-linked List --------*/
/**
 * A single-linked list.
 *
 *  Members:
 *   void *lal: The pointer to the inner value.
 *   struct z_list *tail: A pointer to the next element in the list.
 */
typedef struct _z_l_t
{
    void *val;
    struct _z_l_t *tail;
} _z_list_t;

_z_list_t *_z_list_of(void *x);

size_t _z_list_len(const _z_list_t *xs);
int _z_list_is_empty(const _z_list_t *xs);

void *_z_list_head(const _z_list_t *xs);
_z_list_t *_z_list_tail(const _z_list_t *xs);

_z_list_t *_z_list_push(_z_list_t *xs, void *x);
_z_list_t *_z_list_pop(_z_list_t *xs, z_element_free_f f_f);

_z_list_t *_z_list_find(const _z_list_t *xs, z_element_eq_f f_f, void *e);

_z_list_t *_z_list_drop_filter(_z_list_t *xs, z_element_free_f f_f, z_element_eq_f c_f, void *left);

_z_list_t *_z_list_clone(const _z_list_t *xs, z_element_clone_f d_f);
void _z_list_free(_z_list_t **xs, z_element_free_f f_f);

#define _Z_LIST_DEFINE(name, type)                                                                   \
    typedef _z_list_t name##_list_t;                                                                 \
    static inline name##_list_t *name##_list_new()                                                   \
    {                                                                                                \
        return NULL;                                                                                 \
    }                                                                                                \
    static inline size_t name##_list_len(const name##_list_t *l)                                     \
    {                                                                                                \
        return _z_list_len(l);                                                                       \
    }                                                                                                \
    static inline int name##_list_is_empty(const name##_list_t *l)                                   \
    {                                                                                                \
        return _z_list_is_empty(l);                                                                  \
    }                                                                                                \
    static inline type *name##_list_head(const name##_list_t *l)                                     \
    {                                                                                                \
        return (type *)_z_list_head(l);                                                              \
    }                                                                                                \
    static inline name##_list_t *name##_list_tail(const name##_list_t *l)                            \
    {                                                                                                \
        return _z_list_tail(l);                                                                      \
    }                                                                                                \
    static inline name##_list_t *name##_list_push(name##_list_t *l, type *e)                         \
    {                                                                                                \
        return _z_list_push(l, e);                                                                   \
    }                                                                                                \
    static inline name##_list_t *name##_list_pop(name##_list_t *l)                                   \
    {                                                                                                \
        return _z_list_pop(l, name##_elem_free);                                                     \
    }                                                                                                \
    static inline name##_list_t *name##_list_find(const name##_list_t *l, name##_eq_f c_f, type *e)  \
    {                                                                                                \
        return _z_list_find(l, (z_element_eq_f)c_f, e);                                              \
    }                                                                                                \
    static inline name##_list_t *name##_list_drop_filter(name##_list_t *l, name##_eq_f c_f, type *e) \
    {                                                                                                \
        return _z_list_drop_filter(l, name##_elem_free, (z_element_eq_f)c_f, e);                     \
    }                                                                                                \
    static inline name##_list_t *name##_list_clone(name##_list_t *l)                                 \
    {                                                                                                \
        return _z_list_clone(l, name##_elem_clone);                                                  \
    }                                                                                                \
    static inline void name##_list_free(name##_list_t **l)                                           \
    {                                                                                                \
        _z_list_free(l, name##_elem_free);                                                           \
    }

#endif /* ZENOH_PICO_COLLECTIONS_LIST_H */
