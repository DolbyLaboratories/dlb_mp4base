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
 *   @file dsi.c
 *   @brief Implements decoder specific information handling
 *   For different audio/video codec, there're different DSI definition for each of them. In this file, we 
 *   implements the DSI creation and destroy functions for AVC,HEVC,AAC,AC3,EC3 and AC4. For AVC and HEVC, 
 *   the DSI based on the spec: ISO/IEC 14496-15. For AAC, DSI based on the spec: ISO/IEC 14496-1,ISO/IEC 
 *   14496-3,ISO/IEC 14496-12 and ISO/IEC 14496-14. For AC3 and EC3, DSI based on the spec: ETSI TS 102 366.
 *   For AC4, DSI based on the spec: ETSI TS 103 190-2.
*/

#include "utils.h"
#include "list_itr.h"
#include "registry.h"
#include "dsi.h"
#include "parser.h"

static void
dsi_destroy(dsi_handle_t dsi)
{
    if (dsi->raw_data)
    {
        FREE_CHK(dsi->raw_data);
    }
    FREE_CHK(dsi);

    return;
}

/** avc specific */
static void
mp4_dsi_avc_destroy(dsi_handle_t dsi)
{
    mp4_dsi_avc_handle_t mp4ff_dsi_avc;
    it_list_handle_t     it;
    buf_entry_t *        nalu;

    if (!dsi)
    {
        return ;
    }
    mp4ff_dsi_avc = (mp4_dsi_avc_handle_t)dsi;
    it            = it_create();

    it_init(it, mp4ff_dsi_avc->sps_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_avc->sps_lst);

    it_init(it, mp4ff_dsi_avc->pps_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_avc->pps_lst);

    it_init(it, mp4ff_dsi_avc->sps_ext_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_avc->sps_ext_lst);

    it_destroy(it);

    if (dsi->raw_data)
    {
        FREE_CHK(dsi->raw_data);
    }

    FREE_CHK(dsi);
}

dsi_handle_t
mp4_dsi_avc_create(void)
{
    mp4_dsi_avc_handle_t dsi;

    dsi = (mp4_dsi_avc_handle_t)MALLOC_CHK(sizeof(mp4_dsi_avc_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_avc_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_H264;
        dsi->destroy   = mp4_dsi_avc_destroy;

        dsi->NALUnitLength = 4; /* to make demux robust */

        return (dsi_handle_t)dsi;
    }
    return NULL;
}

dsi_handle_t
dsi_avc_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
        return mp4_dsi_avc_create();
    default:
        return NULL;
    }
}

/** hevc specific */
static void
mp4_dsi_hevc_destroy(dsi_handle_t dsi)
{
    mp4_dsi_hevc_handle_t mp4ff_dsi_hevc;
    it_list_handle_t     it;
    buf_entry_t *        nalu;

    if (!dsi)
    {
        return ;
    }
    mp4ff_dsi_hevc = (mp4_dsi_hevc_handle_t)dsi;
    it            = it_create();

    it_init(it, mp4ff_dsi_hevc->vps_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_hevc->vps_lst);

    it_init(it, mp4ff_dsi_hevc->sps_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_hevc->sps_lst);

    it_init(it, mp4ff_dsi_hevc->pps_lst);
    while ((nalu = it_get_entry(it)))
    {
        FREE_CHK(nalu->data);
    }
    list_destroy(mp4ff_dsi_hevc->pps_lst);

    it_destroy(it);

    if (dsi->raw_data)
    {
        FREE_CHK(dsi->raw_data);
    }

    FREE_CHK(dsi);
}

dsi_handle_t
mp4_dsi_hevc_create(void)
{
    mp4_dsi_hevc_handle_t dsi;

    dsi = (mp4_dsi_hevc_handle_t)MALLOC_CHK(sizeof(mp4_dsi_hevc_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_hevc_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_HEVC;
        dsi->destroy   = mp4_dsi_hevc_destroy;

        dsi->NALUnitLength = 4; /* to make demux robust */

        return (dsi_handle_t)dsi;
    }
    return NULL;
}

dsi_handle_t
dsi_hevc_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
        return mp4_dsi_hevc_create();
    default:
        return NULL;
    }

}

/** aac specific */
dsi_handle_t
mp4_dsi_aac_create(void)
{
    mp4_dsi_aac_handle_t dsi;

    dsi = (mp4_dsi_aac_handle_t)MALLOC_CHK(sizeof(mp4_dsi_aac_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_aac_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_AAC;
        dsi->destroy   = dsi_destroy;

        return (dsi_handle_t)dsi;
    }
    return NULL;
}

dsi_handle_t
dsi_aac_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
        return mp4_dsi_aac_create();
    default:
        return NULL;
    }
}

/** ac3 specific */
dsi_handle_t
mp4_dsi_ac3_create(void)
{
    mp4_dsi_ac3_handle_t dsi;

    dsi = (mp4_dsi_ac3_handle_t)MALLOC_CHK(sizeof(mp4_dsi_ac3_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_ac3_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_AC3;
        dsi->destroy   = dsi_destroy;

        return (dsi_handle_t)dsi;
    }
    return NULL;
}

dsi_handle_t
dsi_ac3_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
        return mp4_dsi_ac3_create();
    default:
        return NULL;
    }
}

/** ec3 specific */
static void
mp4_dsi_ec3_destroy(dsi_handle_t dsi)
{
    mp4_dsi_ec3_handle_t dsi_ec3;

    if (dsi)
    {
        dsi_ec3 = (mp4_dsi_ec3_handle_t)dsi;

        if (dsi_ec3->substreams)
        {
            FREE_CHK(dsi_ec3->substreams);
        }
        dsi_destroy(dsi);
    }
}

dsi_handle_t
mp4_dsi_ec3_create(void)
{
    mp4_dsi_ec3_handle_t dsi;

    dsi = (mp4_dsi_ec3_handle_t)MALLOC_CHK(sizeof(mp4_dsi_ec3_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_ec3_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_EC3;
        dsi->destroy   = mp4_dsi_ec3_destroy;

        return (dsi_handle_t)dsi;
    }
    return NULL;
}

dsi_handle_t
dsi_ec3_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
    case DSI_TYPE_CFF:
        return mp4_dsi_ec3_create();
    default:
        return NULL;
    }
}

/** ac4 specific */
static void
mp4_dsi_ac4_destroy(dsi_handle_t dsi)
{
    FREE_CHK(dsi);
}

dsi_handle_t
mp4_dsi_ac4_create(void)
{
    mp4_dsi_ac4_handle_t dsi;

    dsi = (mp4_dsi_ac4_handle_t)MALLOC_CHK(sizeof(mp4_dsi_ac4_t));
    if (dsi)
    {
        memset(dsi, 0, sizeof(mp4_dsi_ac4_t));

        dsi->dsi_type  = DSI_TYPE_MP4FF;
        dsi->stream_id = STREAM_ID_AC4;
        dsi->destroy   = mp4_dsi_ac4_destroy;

        return (dsi_handle_t)dsi;
    }
    return NULL;

}

dsi_handle_t
dsi_ac4_create(uint32_t dsi_type)
{
    switch (dsi_type)
    {
    case DSI_TYPE_MP4FF:
        return mp4_dsi_ac4_create();
    default:
        return NULL;
    }
}
