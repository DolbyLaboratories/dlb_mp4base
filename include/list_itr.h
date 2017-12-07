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
    @file list_itr.h
    @brief Defines list and iteration on the list methods
*/

#ifndef __LIST_ITR_H__
#define __LIST_ITR_H__

#include "c99_inttypes.h"  /* uint32_t */
#include "return_codes.h"  /* return codes */

#ifdef __cplusplus
extern "C"
{
#endif

/* a popular list entry:sps. pps. sps_ext */
typedef struct nalu_entry_t_
{
    uint32_t id;
    size_t   size;
    uint8_t* data;
} buf_entry_t;

/* count_value_t represents how many times a value occurs. cts_offset_lst, size_lst */
typedef struct count_value_t_
{
    uint32_t idx;    /* idx for easier indexing */
    uint32_t count;
    uint64_t value;
} count_value_t;

/** a listc FIFO whose management info use sampel memory block with content it stores */
typedef struct list_t_  list_t;
typedef list_t         *list_handle_t;

/** the iterator working on the list */
typedef struct it_list_t_  it_list_t;
typedef it_list_t         *it_list_handle_t;

/** op on the list */
list_handle_t list_create(size_t content_size);  /* create list stores content of content_size */
void          list_destroy(list_handle_t lst);   /* destroy */

void *list_alloc_entry(list_handle_t lst);      /* alloc memory of content_size */
void  list_free_entry(void *p_content);         /* free */

int list_add_entry(list_handle_t lst, void *p_content);     /* add the content into list */
int list_remove_entry(list_handle_t lst, void *p_content);  /* remove content from list */

uint32_t list_get_entry_num(list_handle_t lst); /* return the number of entries in list */

void *list_peek_first_entry(list_handle_t lst);  /* return the point of first entry */
void  list_delete_first_entry(list_handle_t lst);/* removed 1st entry from list, free memory */
void *list_peek_last_entry(list_handle_t lst);   /* return the point of last entry */

/* count/value list */
void count_value_lst_update(list_handle_t lst, uint64_t value);

/** op supported by the internal iterator of the list */
void  list_it_init(list_handle_t lst);          /* iterator point to 1st entry or null if empty */
void *list_it_get_entry(list_handle_t lst);     /* return the current point, move iterator to next */
void *list_it_peek_entry(list_handle_t lst);    /* peek the current entry */
void *list_it_peek2_entry(list_handle_t lst);   /* peek the next entry after current entry */
void  list_it_save_mark(list_handle_t lst);     /* mark the current position */
void  list_it_goto_mark(list_handle_t lst);     /* reset the current position to the mark */

/** op of external iterator */
it_list_handle_t it_create(void);
void             it_destroy(it_list_handle_t it);

void             it_init(it_list_handle_t it, list_handle_t lst);  /* set the iterator current position to 1st entry of lst */
it_list_handle_t it_create_on(list_handle_t lst);                  /* = create and init */
void *           it_get_entry(it_list_handle_t it);                /* return the current point, move iterator to next */
void *           it_peek_entry(it_list_handle_t it);               /* peek: no move of iterator */

#ifdef __cplusplus
};
#endif

#endif /* __LIST_ITR_H__ */
