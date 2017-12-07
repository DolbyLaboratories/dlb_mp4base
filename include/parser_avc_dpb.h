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
/**<
    @file parser_avc_dpb.h
    @brief Defines avc parser dpb to derive 0 based poc and minimum pic reordering.
*/

#ifndef __PARSER_AVC_DPB_H__
#define __PARSER_AVC_DPB_H__

#include "boolean.h"  /** BOOL */

#define USE_HRD_FOR_TS     1 /** use the hrd model for dts, cts*/
#define TEST_DELTA_POC     (0 && USE_HRD_FOR_TS) /** apoc is for what it mean for when USE_HRD_FOR_TS = 0 */

typedef struct avc_apoc_t_ avc_apoc_t;

avc_apoc_t* apoc_create(void);
void        apoc_destroy(avc_apoc_t *p);

void apoc_set_num_reorder_au(avc_apoc_t *p, int num_reorder_au);
void apoc_set_max_ref_au    (avc_apoc_t *p, int num_ref_frames);
void apoc_flush             (avc_apoc_t *p);
void apoc_add               (avc_apoc_t *p, int poc, int is_idr);
int  apoc_reorder_num       (avc_apoc_t *p, int doc);              /** return -1 for unknown */
int  apoc_min_cts           (avc_apoc_t *p);                       /** in au count */
BOOL apoc_need_adj_cts      (avc_apoc_t *p);                       /** always return true */

#if defined(_DEBUG) && TEST_DELTA_POC
    #define CAN_TEST_DELTA_POC  1
    int apoc_get_delta_poc(avc_apoc_t *p);
#else
    #define CAN_TEST_DELTA_POC  0
    #define apoc_get_delta_poc(p)  0
#endif

#ifdef __cplusplus
};
#endif

#endif /* !__PARSER_AVC_DPB_H__ */
