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
    @file parser.c
    @brief Implements basic functions for all parser supported

*/

#include "parser.h"
#include "registry.h"
#include "msg_log.h"     /* msglog() */
#include "memory_chk.h"  /* MALLOC_CHK(), FREE_CHK() */

/*
 * helper functions to avoid direct function pointer usage
 */
int32_t parser_call_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx, bbio_handle_t ds)
{
    return parser->init(parser, ext_timing, es_idx, ds);
}

void parser_call_destroy(parser_handle_t parser)
{
    parser->destroy(parser);
}

int32_t parser_call_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    return parser->get_sample(parser, sample);
}

void parser_call_sample_destroy(struct mp4_sample_t_ *sample)
{
    sample->destroy(sample);
}


/** "class member" */
int8_t
parser_get_type(parser_handle_t parser)
{
    switch (parser->stream_type)
    {
    case  STREAM_TYPE_VIDEO:
        return 'v';
    case  STREAM_TYPE_AUDIO:
        return 'a';
    case  STREAM_TYPE_DATA:
        return 'd';
    case  STREAM_TYPE_SUBTITLE:
        return 's';
    default:
        return 'u';
    }
}

size_t
get_codec_config_size(parser_handle_t parser)
{
    if (parser->curr_codec_config)
    {
        return parser->curr_codec_config->codec_config_size;
    }

    return 0;
}

/* Returns the offset into buf where the start_code is
*  return -1 for no start_code found */
int32_t
find_start_code_off(bbio_handle_t ds, uint64_t size, uint32_t start_code, uint32_t start_code_size, uint32_t mask)
{
    uint32_t val;
    uint64_t offset;

    if (size < (uint64_t)(start_code_size+1))
    {
        /* start_code at least start_code_size bytes */
        return -1;
    }

    offset = 0;

    /** get next current start code */
    val = 0xffffffff;
    while (offset < size)
    {
        val <<= 8;
        val |= src_read_u8(ds);
        offset++;
        if ((val & mask) == start_code)
        {
            return (int32_t)(offset - start_code_size);
        }
    }
    return -1;
}

void
parser_set_frame_size(parser_handle_t parser, uint32_t frame_size)
{
    parser->frame_size = frame_size;
}

dsi_handle_t
parser_get_curr_dsi(parser_handle_t parser)
{
    if (parser->curr_dsi)
    {
        return parser->curr_dsi;
    }

    if (parser->dsi_create)
    {
        (void)dsi_list_create(parser, parser->dsi_type);
        if (!parser->curr_dsi)
        {
            msglog(NULL, MSGLOG_ERR, "ERR: no dsi for %s\n", parser->stream_name);
        }
    }
    return parser->curr_dsi;
}

/** Sets a callback for reporting messages to higher layers of the application */
void parser_set_reporter(parser_handle_t parser, parser_reporter_t *reporter)
{
    parser->reporter = reporter;
}

/** Sets conformance checking */
int32_t parser_set_conformance(parser_handle_t parser, const int8_t* type)
{
    if (type) {
        strncpy(parser->conformance_type, type, 4);
    }
    else {
        parser->conformance_type[0] = '\0';
    }
    /* check supported types */
    if (parser->conformance_type[0] == '\0'
        || IS_FOURCC_EQUAL(parser->conformance_type, "ccff")
        ) 
    {
        return 0;
    }
    return 1;
}

void
parser_destroy(parser_handle_t parser)
{
    if (parser)
    {
        dsi_list_destroy(parser);

        if (parser->codec_config_lst)
        {
            codec_config_t*  p_config;
            it_list_handle_t it = it_create();
            it_init(it, parser->codec_config_lst);
            while ((p_config = (codec_config_t*)it_get_entry(it)))
            {
                FREE_CHK(p_config->codec_config_data);
            }
            it_destroy(it);
            list_destroy(parser->codec_config_lst);
            parser->codec_config_lst = 0;
        }

        FREE_CHK(parser);
    }
}

int32_t
dsi_list_create(parser_handle_t parser, uint32_t dsi_type)
{
    dsi_handle_t* p_curr_dsi;

    parser->dsi_lst = list_create(sizeof(dsi_handle_t));
    if (!parser->dsi_lst)
    {
        return 1;
    }
    /* create first entry for dsi list */
    parser->curr_dsi = parser->dsi_create(dsi_type);
    if (!parser->curr_dsi)
    {
        return 1;
    }
    p_curr_dsi  = (dsi_handle_t*)list_alloc_entry(parser->dsi_lst);
    *p_curr_dsi = parser->curr_dsi;
    parser->dsi_curr_index = 1;
    list_add_entry(parser->dsi_lst, p_curr_dsi);

    return 0;
}

void
dsi_list_destroy(parser_handle_t parser)
{
    if (parser->dsi_lst)
    {
        dsi_handle_t*    entry;
        dsi_handle_t     dsi;
        it_list_handle_t it = it_create();
        it_init(it, parser->dsi_lst);
        while ((entry = (dsi_handle_t*)it_get_entry(it)))
        {
            dsi = *entry;
            dsi->destroy(dsi);
        }
        it_destroy(it);

        list_destroy(parser->dsi_lst);
    }
    parser->dsi_lst = 0;
}

#if defined(sample_destroy)
#undef sample_destroy
#endif
static void
sample_destroy(mp4_sample_handle_t sample)
{
    if (sample)
    {
        if (sample->data)
        {
            FREE_CHK(sample->data);
        }
        FREE_CHK(sample);
    }
}

mp4_sample_handle_t
sample_create(void)
{
    mp4_sample_handle_t sample = (mp4_sample_handle_t)MALLOC_CHK(sizeof(mp4_sample_t));
    if (sample == NULL)
    {
        return sample;
    }

    memset(sample, 0, sizeof(mp4_sample_t));
    sample->destroy = sample_destroy;

    return sample;
}

avi_sample_handle_t
sample_create_avi()
{
    avi_sample_handle_t sample = (avi_sample_handle_t)MALLOC_CHK(sizeof(avi_sample_t));
    if (sample == NULL)
    {
        return sample;
    }

    memset(sample, 0, sizeof(avi_sample_t));
    sample->destroy = sample_destroy_avi;

    return sample;
}

void
sample_destroy_avi (avi_sample_handle_t sample)
{
    if (sample)
    {
        if (sample->data)
        {
            FREE_CHK(sample->data);
        }
        FREE_CHK(sample);
    }
}
