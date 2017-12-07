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
    @file parser_avc_dpb.c
    @brief Implements avc parser dpb to derive 0 based poc and minimum pic reordering.

*/

#include <assert.h>          /* assert() */
#include <string.h>          /* memset() */

#include "memory_chk.h"
#include "parser_avc_dpb.h"

/* reorder buffer */
#define DP_CNT_MAX      33  /* 2*16 + 1 */
typedef struct dpb_t_ {
    int doc_next;           /* dec order cnt */
    int dp_cnt;             /* dec pic cnt in dpb */
    int dp_cnt_max;         /* the max */
    int docs[DP_CNT_MAX];   /* array for doc */
    int pocs[DP_CNT_MAX];   /* array for input poc */
} dpb_t;

#define USE_MAP2          1 /* use two layer matrix to avoid mem realloc */
#if !USE_MAP2
    #define DOC_2_POC_OUT(m, doc)  m[doc]    
#else
    #define MAP_SEC_LOG2_SIZE   10 /* secondary matrix size 1<< MAP_SEC_LOG2_SIZE */
    #define MAP_SEC_IDX_MASK    ((1<<MAP_SEC_LOG2_SIZE) - 1)
    #define MAP_PRIM_SIZE       4096 /* at 120 frame/sec, 4096*1024 frame > 9.7 hours */

    #define DOC_2_POC_OUT(m, doc)  (m)[(doc) >> MAP_SEC_LOG2_SIZE][(doc) & MAP_SEC_IDX_MASK]
#endif

struct avc_apoc_t_ {
    dpb_t dpb;

    int ref_au_max;
    int reorder_min_ready;
    int reorder_min;
    int doc_at_poc_min;
    int poc_out_next;
    /* doc=>poc output/abs poc map */
    int doc_poc_out_map_size;
#if !USE_MAP2
    int *doc_poc_out_map; 
#else
    int *doc_poc_out_map[MAP_PRIM_SIZE];
    int  map_sec_cnt;
#endif

#if CAN_TEST_DELTA_POC
    int delta_poc, poc_pre;
#endif
};

static void
apoc_init(avc_apoc_t *p)
{
    p->dpb.doc_next = 0;
    p->dpb.dp_cnt = 0;
    p->dpb.dp_cnt_max = DP_CNT_MAX;

    p->ref_au_max = 0;
    p->reorder_min_ready = 0;
    p->reorder_min = 0;
    p->doc_at_poc_min = 0;
    p->poc_out_next = 0;
    p->doc_poc_out_map_size = 0;
#if !USE_MAP2
    p->doc_poc_out_map = NULL;
#else
    p->map_sec_cnt = 0;
    memset(p->doc_poc_out_map, 0, sizeof(p->doc_poc_out_map));
#endif

#if CAN_TEST_DELTA_POC
    p->delta_poc = -1;
    p->poc_pre = -1;
#endif
}

void 
apoc_set_num_reorder_au(avc_apoc_t *p, int num_reorder_au)
{
    p->dpb.dp_cnt_max = num_reorder_au + 1;
}

void 
apoc_set_max_ref_au(avc_apoc_t *p, int max_ref_au)
{
    p->ref_au_max = max_ref_au;
}

static void 
apoc_update(avc_apoc_t *p, BOOL dpb_flush)
{
    int i;
    int poc_min_idx;
    dpb_t *p_dpb = &(p->dpb);

    /** to make sure reorder is right */
    if (!dpb_flush && p_dpb->dp_cnt < p_dpb->dp_cnt_max)
        return;

    /** find min poc */
    poc_min_idx = 0;
    for (i = 1; i < p_dpb->dp_cnt; i++) {
        if (p_dpb->pocs[i] < p_dpb->pocs[poc_min_idx])
            poc_min_idx = i;
    }

    /** test only code: check if delta_poc is constant */
#if CAN_TEST_DELTA_POC
    if (p->delta_poc >= 0) {
        if (p->delta_poc != p_dpb->pocs[poc_min_idx] - p->poc_pre) {
            printf("parser_avc_dpb: warning: delta poc changed %d =>%d\n", 
                p->delta_poc, p_dpb->pocs[poc_min_idx] - p->poc_pre);
            p->delta_poc = p_dpb->pocs[poc_min_idx] - p->poc_pre;
        }
    }
    else if (p->poc_pre >= 0) {
        p->delta_poc = p_dpb->pocs[poc_min_idx] - p->poc_pre;
    }
    p->poc_pre = p_dpb->pocs[poc_min_idx];
#endif

    /** output docs[poc_min_idx], build the map (doc, out_poc) */
    if (p_dpb->docs[poc_min_idx] >= p->doc_poc_out_map_size) {
#if !USE_MAP2
        int inc = 1000 + (p_dpb->docs[poc_min_idx]- p->doc_poc_out_map_size);

        p->doc_poc_out_map_size += inc;
        p->doc_poc_out_map = (int*)REALLOC_CHK(p->doc_poc_out_map, sizeof(int)*p->doc_poc_out_map_size );
        for (i = p->doc_poc_out_map_size - inc; i <  p->doc_poc_out_map_size; i++)
            p->doc_poc_out_map[i] = -1; /* To detect errors latter */
#else
        assert(p->map_sec_cnt < MAP_PRIM_SIZE);

        p->doc_poc_out_map[p->map_sec_cnt] = MALLOC_CHK(sizeof(int)<<MAP_SEC_LOG2_SIZE);
        memset(p->doc_poc_out_map[p->map_sec_cnt], 0xFF, sizeof(int)<<MAP_SEC_LOG2_SIZE);
        p->map_sec_cnt++;
        p->doc_poc_out_map_size += 1<<MAP_SEC_LOG2_SIZE;
#endif
    }
    i = p_dpb->docs[poc_min_idx];
    if (p->poc_out_next == 0) {
      p->doc_at_poc_min = i;
    }
    DOC_2_POC_OUT(p->doc_poc_out_map, i) = p->poc_out_next++;

    /** update reorder_min */
    if (p->reorder_min < poc_min_idx)
        p->reorder_min = poc_min_idx;

    /** update dpb after output poc_min_idx */
    p_dpb->dp_cnt--;
    for (i = poc_min_idx; i < p_dpb->dp_cnt; i++) {
        p_dpb->docs[i] = p_dpb->docs[i+1];
        p_dpb->pocs[i] = p_dpb->pocs[i+1];
    }

    if (!p->reorder_min_ready) {
      /* assume first ref_au_max + 1 AUs in decoding order resolve the reorder_min */
      p->reorder_min_ready = p->ref_au_max;
      while (p->reorder_min_ready >= 0 && DOC_2_POC_OUT(p->doc_poc_out_map, p->reorder_min_ready) >= 0) {
        p->reorder_min_ready--;
      }
      p->reorder_min_ready = (p->reorder_min_ready >= 0) ? 0 : 1;
    }
}

void 
apoc_flush(avc_apoc_t *p)
{
    while (p->dpb.dp_cnt > 0) {
        apoc_update(p, TRUE);
    }
}

void 
apoc_add(avc_apoc_t *p, int poc, int is_idr)
{
    dpb_t *p_dpb = &(p->dpb);

    if (is_idr)
        apoc_flush(p);

    p_dpb->docs[p_dpb->dp_cnt] = p_dpb->doc_next;
    p_dpb->pocs[p_dpb->dp_cnt] = poc;
    p_dpb->doc_next++;
    p_dpb->dp_cnt++;

    apoc_update(p, FALSE);
}

int 
apoc_reorder_num(avc_apoc_t *p, int doc)
{
    int poc_out;

    if (doc >= p->poc_out_next || !p->reorder_min_ready) {
        return -1;   /* have not done reordering or reorder_min not ready */
    }

    poc_out = DOC_2_POC_OUT(p->doc_poc_out_map, doc);
    if (poc_out < 0)
        return -1; /* the doc is never related to apoc_add() */

    return p->reorder_min + poc_out - doc;
}

int 
apoc_min_cts(avc_apoc_t *p)
{
    int rnum = apoc_reorder_num(p, p->doc_at_poc_min);

    if (rnum < 0)
    {
        /* shall not come here, just assume is 0 */
        rnum = 0;
    }
    return   rnum + p->doc_at_poc_min;
}

BOOL
apoc_need_adj_cts(avc_apoc_t *p)
{
    return p->reorder_min > 0;
}

void 
apoc_destroy(avc_apoc_t *p)
{   
#if !USE_MAP2
    if (p->doc_poc_out_map) {
        MEM_FREE_AND_NULL(p->doc_poc_out_map);
    }
#else
    int **pp = p->doc_poc_out_map;
    
    while (*pp != NULL) {
        MEM_FREE_AND_NULL(*pp);
        pp++;
    }
#endif
    FREE_CHK(p);
}

avc_apoc_t* 
apoc_create(void)
{
    avc_apoc_t *p;

    p = (avc_apoc_t *)MALLOC_CHK(sizeof(avc_apoc_t));
    if (p)
    {
        apoc_init(p);
    }
    return p;
}

#if CAN_TEST_DELTA_POC
int 
apoc_get_delta_poc(avc_apoc_t *p)
{
    return p->delta_poc;
}
#endif
