/************************************************************************************************************
 * Copyright (c) 2017, Dolby Laboratories Inc.
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:

 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
 *    promote products derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 ************************************************************************************************************/
/*<
    @file list_itr.c
    @brief Implements list and iteration on the list methods
*/

#include "utils.h"
#include "list_itr.h"

typedef struct entry_t_
{
    struct entry_t_ *next;
    uint8_t          content[1];
} entry_t;

static const int PTR_SIZE = sizeof(entry_t *);

#define E_2_C_PTR(p_entry)      ((p_entry)->content)
#define C_2_E_PTR(p_content)    ((uint8_t *)(p_content) - PTR_SIZE)

struct list_t_
{
    entry_t *hdr, *tail;
    uint32_t entry_count;    /** total entries/list size */
    size_t   entry_size;

    entry_t *cur, *cur_mark; /** cur to support single thread iteration on list */
};

struct it_list_t_
{
    entry_t *p_entry;
};

list_handle_t
list_create(size_t content_size)
{
    list_handle_t lst;

    lst = MALLOC_CHK(sizeof(list_t));
    if (!lst)
    {
        return NULL;
    }

    lst->hdr         = lst->tail = lst->cur = lst->cur_mark = NULL;
    lst->entry_count = 0;
    lst->entry_size  = PTR_SIZE + content_size;

    return lst;
}

void
list_destroy(list_handle_t lst)
{
    entry_t *p, *pn;

    if (!lst) return;

    p = lst->hdr;
    while (p)
    {
        pn = p->next;
        FREE_CHK(p);
        p = pn;
    }

    FREE_CHK(lst);
}

void *
list_alloc_entry(list_handle_t lst)
{
    entry_t *p_entry;

    assert(lst);
    p_entry = (entry_t *)MALLOC_CHK(lst->entry_size);
    if (p_entry)
    {
        return E_2_C_PTR(p_entry);
    }
    return NULL;
}

void
list_free_entry(void *p_content)
{
    entry_t *p_entry;

    if (p_content)
    {
        p_entry = (entry_t *)C_2_E_PTR(p_content);
        assert(p_entry->content == p_content);

        FREE_CHK(p_entry);
    }
}

int
list_add_entry(list_handle_t lst, void *p_content)
{
    entry_t *p_entry;

    assert(p_content);
    if (!lst)
    {
        return EMA_MP4_MUXED_BUGGY;
    }

    p_entry = (entry_t *)C_2_E_PTR(p_content);
    assert(p_entry->content == p_content);
    p_entry->next = 0;

    if (lst->tail)
    {
        lst->tail->next = p_entry;
    }
    else
    {
        lst->hdr = p_entry;
    }
    lst->tail = p_entry;
    lst->entry_count++;

    return EMA_MP4_MUXED_OK;
}

int
list_remove_entry(list_handle_t lst, void *p_content)
{
    entry_t *pre = NULL, *p;

    if (!lst || !lst->hdr || !p_content)
    {
        return EMA_MP4_MUXED_BUGGY;
    }

    p = lst->hdr;
    while (p && E_2_C_PTR(p) != p_content)
    {
        pre = p;
        p = p->next;
    }

    if (p)
    {
        if (pre)
        {
            pre->next = p->next;
        }
        else
        {
            lst->hdr = p->next;
        }

        if (p == lst->tail)
        {
            lst->tail = pre;
        }
        if (lst->cur == p)
        {
            lst->cur = p->next;
        }
        lst->entry_count--;

        return EMA_MP4_MUXED_OK;
    }

    return EMA_MP4_MUXED_BUGGY;
}

uint32_t
list_get_entry_num(list_handle_t lst)
{
    if (!lst)
    {
        return 0;
    }

    return lst->entry_count;
}

void *
list_peek_first_entry(list_handle_t lst)
{
    if (!lst || !lst->hdr)
    {
        return NULL;
    }

    return E_2_C_PTR(lst->hdr);
}


void *
list_peek_last_entry(list_handle_t lst)
{
    if (!lst || !lst->tail)
    {
        return NULL;
    }

    return E_2_C_PTR(lst->tail);
}

void
list_delete_first_entry(list_handle_t lst)
{
    entry_t *p_entry;

    if (!lst || !lst->hdr)
    {
        return;
    }

    p_entry = lst->hdr;
    /** removed from list */
    lst->hdr = lst->hdr->next;
    if (!lst->hdr)
    {
        lst->tail = NULL;
    }
    if (lst->cur == p_entry)
    {
        lst->cur = lst->hdr;
    }
    lst->entry_count--;

    FREE_CHK(p_entry);
}

/** count/value list */
void
count_value_lst_update(list_handle_t lst,
                       uint64_t      value)
{
    uint32_t       entry_count;
    count_value_t *cv;
    uint32_t       last_idx, last_count;

    entry_count = list_get_entry_num(lst);
    if (entry_count == 0)
    {
        last_idx   = 0;
        last_count = 1;
    }
    else
    {
        cv = list_peek_last_entry(lst);
        if (cv->value == value)
        {
            cv->count++;
            return;
        }
        last_idx   = cv->idx;
        last_count = cv->count;
    }

    cv = list_alloc_entry(lst);
    cv->idx   = last_idx + last_count;
    cv->count = 1;
    cv->value = value;
    list_add_entry(lst, cv);
}


void
list_it_init(list_handle_t lst)
{
    lst->cur      = lst->hdr;
    lst->cur_mark = NULL;
}

void *
list_it_get_entry(list_handle_t lst)
{
    entry_t *p_entry;

    if (!lst || !lst->cur)
    {
        return NULL;
    }

    p_entry  = lst->cur;
    lst->cur = p_entry->next;

    return E_2_C_PTR(p_entry);
}

void *
list_it_peek_entry(list_handle_t lst)
{
    if (!lst || !lst->cur)
    {
        return NULL;
    }

    return E_2_C_PTR(lst->cur);
}

void *
list_it_peek2_entry(list_handle_t lst)
{
    if (!lst || !lst->cur || !lst->cur->next)
    {
        return NULL;
    }
    return E_2_C_PTR(lst->cur->next);
}

void
list_it_save_mark(list_handle_t lst)
{
    assert(lst->cur_mark == NULL); /** support only one mark */

    lst->cur_mark = lst->cur;
}

void
list_it_goto_mark(list_handle_t lst)
{
    assert(lst->cur_mark != NULL || lst->cur == NULL); /** mark must be well defined */

    lst->cur = lst->cur_mark;

    lst->cur_mark = NULL;
}


it_list_handle_t
it_create(void)
{
    it_list_handle_t it;

    it = (it_list_handle_t)MALLOC_CHK(sizeof(it_list_t));
    if (it)
    {
        it->p_entry = NULL;
    }

    return it;
}

it_list_handle_t
it_create_on(list_handle_t lst)
{
    it_list_handle_t it;

    it = (it_list_handle_t)MALLOC_CHK(sizeof(it_list_t));
    if (it)
    {
        if (lst)
        {
            it->p_entry = lst->hdr;
        }
        else
        {
            it->p_entry = NULL;
        }
    }

    return it;
}

void
it_init(it_list_handle_t it, list_handle_t lst)
{
    assert(it);
    if (lst)
    {
        it->p_entry = lst->hdr;
    }
    else
    {
        it->p_entry = NULL;
    }
}

void *
it_peek_entry(it_list_handle_t it)
{
    if (it && it->p_entry)
    {
        return E_2_C_PTR(it->p_entry);
    }

    return NULL;
}

void *
it_get_entry(it_list_handle_t it)
{
    void *p_content;

    if (it && it->p_entry)
    {
        p_content   = E_2_C_PTR(it->p_entry);
        it->p_entry = it->p_entry->next;

        return p_content;
    }

    return NULL;
}

void
it_destroy(it_list_handle_t it)
{
    FREE_CHK(it);
}
