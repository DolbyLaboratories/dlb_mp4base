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
    @file mp4_stream.c
    @brief Implements track specific demultiplex
*/

#include "utils.h"
#include "mp4_stream.h"
#include "parser.h"

static uint32_t
sample_cts_offset(stream_handle_t stream, uint32_t sample_idx)
{
    box_data_tbl_t *ctts = &(stream->ctts);

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (sample_idx < ctts->sample_idx0)
    {
        /* get backward: start from beginning */
        ctts->sample_idx0 = 0;
        ctts->entry_idx   = 0;
        /* ctts->acc_val no def in ctst */
    }

    for (; ctts->entry_idx < ctts->entry_count; ctts->entry_idx++)
    {
        uint32_t offset        = (ctts->entry_idx << 3);
        uint32_t sample_count  = 0;
        uint32_t sample_offset = 0;
        /*  Don't read off end of array */
        if (offset > ctts->size - 8)
        {
            break;
        }
        sample_count  = get_BE_u32(ctts->data + offset);
        sample_offset = get_BE_u32(ctts->data + 4 + offset);

        if (sample_idx < (ctts->sample_idx0 + sample_count))
        {
            /* requested sample is within this range */
            return sample_offset;
        }
        ctts->sample_idx0 += sample_count;
    }

    /* should not get here */
    return 0;
}

/********************* stream API ****************************/
/** Finds the sample with dts no later than start_time */
uint32_t
stream_get_sample_idx(stream_handle_t stream, uint64_t start_time)
{
    box_data_tbl_t *stts = &(stream->stts);

    if (start_time <= stream->dts_offset)
    {
        return 0;  /* the first one */
    }
    start_time -= stream->dts_offset;

    if (start_time < stts->acc_val)
    {
        /* get backward: start from beginning */
        stts->sample_idx0 = 0;
        stts->entry_idx   = 0;
        stts->acc_val     = 0;  /* the dts */
    }

    for (; stts->entry_idx < stts->entry_count; stts->entry_idx++)
    {
        uint32_t offset       = (stts->entry_idx << 3);
        uint32_t sample_count = 0;
        uint32_t sample_delta = 0;
        uint64_t val_next     = 0;
        /* Don't read off end of array */
        if (offset > stts->size - 8)
        {
            break;
        }
        sample_count = get_BE_u32(stts->data + offset);
        sample_delta = get_BE_u32(stts->data + 4 + offset);

        val_next = stts->acc_val + sample_count *(uint64_t)sample_delta;

        if (start_time < val_next)
        {
            return stts->sample_idx0 + (uint32_t)((start_time - stts->acc_val)/sample_delta);
        }
        stts->sample_idx0 += sample_count;
        stts->acc_val      = val_next;
    }

    /* just return the last one */
    return stream->sample_num - 1;
}

/** Gets the dts and cts-dts of a sample by adding durations */
uint64_t
stream_get_sample_timing(stream_handle_t stream, uint32_t sample_idx, uint32_t *cts_offset)
{
    box_data_tbl_t *stts = &(stream->stts);
    uint64_t        dts;

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (sample_idx < stts->sample_idx0)
    {
        /* get backward: start from beginning */
        stts->sample_idx0 = 0;
        stts->entry_idx   = 0;
        stts->acc_val     = 0;  /* the dts */
    }

    for (; stts->entry_idx < stts->entry_count; stts->entry_idx++)
    {
        uint32_t offset       = (stts->entry_idx << 3);
        uint32_t sample_count = 0;
        uint32_t sample_delta = 0;
        /* so we don't read off the array */
        if (offset > stts->size - 8)
        {
            break;
        }
        sample_count = get_BE_u32(stts->data + offset);
        sample_delta = get_BE_u32(stts->data + 4 + offset);

        if (sample_idx < (stts->sample_idx0 + sample_count))
        {
            dts         = stts->acc_val + (sample_idx - stts->sample_idx0)*(uint64_t)sample_delta;
            *cts_offset = sample_cts_offset(stream, sample_idx);

            return dts + stream->dts_offset;
        }

        stts->sample_idx0 += sample_count;
        stts->acc_val     += sample_count*(uint64_t)sample_delta;
    }

    /* should not get here */
    *cts_offset = 0;
    return (uint64_t)(-1);
}

uint32_t
stream_get_sample_duration(stream_handle_t stream, uint32_t sample_idx)
{
    box_data_tbl_t *stts = &(stream->stts);

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (sample_idx < stts->sample_idx0)
    {
        /* get backward: start from beginning */
        stts->sample_idx0 = 0;
        stts->entry_idx   = 0;
        stts->acc_val     = 0;
    }

    for (; stts->entry_idx < stts->entry_count; stts->entry_idx++)
    {
        uint32_t offset       = (stts->entry_idx << 3);
        uint32_t sample_count = 0;
        uint32_t sample_delta = 0;
        /*  so we don't read off the array */
        if (offset > stts->size - 8)
        {
            break;
        }
        sample_count = get_BE_u32(stts->data + offset);
        sample_delta = get_BE_u32(stts->data + 4 + offset);

        if (sample_idx < (stts->sample_idx0 + sample_count))
        {
            return sample_delta;
        }
        stts->sample_idx0 += sample_count;
        stts->acc_val     += sample_count*(uint64_t)sample_delta;
    }

    /* should not get here */
    return (uint32_t)(-1);
}

uint32_t
stream_get_sample_size(stream_handle_t stream, uint32_t sample_idx)
{
    box_data_tbl_t *stsz = &(stream->stsz);
    uint32_t        size = 0;

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (stsz->variant)
    {
        /* stz2 case */
        if (stsz->add_info == 4)
        {
            size = stsz->data[sample_idx>>1];
            if (sample_idx % 2)
            {
                size >>= 4;
            }
            size &= 0x0F;
        }
        else if(stsz->add_info == 8)
        {
            size = stsz->data[sample_idx];
        }
        else if(stsz->add_info == 16)
        {
            uint32_t offset = (sample_idx<<1);
            if (offset <= stsz->size - 2)
            {
                size = get_BE_u16(stsz->data + offset);
            }
        }
        else if (stsz->add_info == 32)
        {
            uint32_t offset = (sample_idx<<2);
            if (offset <= stsz->size - 4)
            {
                size = get_BE_u32(stsz->data + offset);
            }
        }
    }
    else
    {
        size = stsz->add_info;
        if (size == 0)
        {
            uint32_t offset = (sample_idx<<2);
            if (offset <= stsz->size - 4)
            {
                size = get_BE_u32(stsz->data + offset);
            }
        }
    }

    return size;
}

uint64_t
stream_get_sample_offset(stream_handle_t stream, uint32_t sample_idx, uint32_t *sample_desc_index)
{
    box_data_tbl_t *stsc = &(stream->stsc);
    uint32_t        addr = 0;
    uint32_t        first_chunk = 0, first_chunk_next = 0, samples_per_chunk = 1;

    uint32_t chunk_idx = 0, chunk_idx_in_stco_entry = 0, sample_idx_in_chunk = 0;
    uint64_t chunk_byte_offset = 0, sample_byte_offset_in_chunk = 0;

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (sample_idx < stsc->sample_idx0)
    {
        /* get backward: start from beginning */
        stsc->sample_idx0 = 0;
        stsc->entry_idx   = 0;
        stsc->acc_val     = 0;  /* chunk */
    }

    /* -1 => chunk numbers are 1-based */
    addr             = 12*stsc->entry_idx;
    first_chunk_next = get_BE_u32(stsc->data + addr) - 1;
    for (; stsc->entry_idx < stsc->entry_count; stsc->entry_idx++)
    {
        uint32_t sample_idx0_next = 0;

        first_chunk = first_chunk_next;
        /*  we are going to read 4 bytes and we are adding 4 to the pointer */
        if (addr >= stsc->size - 8)
        {
            break;
        }
        samples_per_chunk = get_BE_u32(stsc->data + addr + 4);

        *sample_desc_index = get_BE_u32(stsc->data + addr + 8);

        if (stsc->entry_idx == stsc->entry_count-1)
        {
            /* stco is open ended, sample_idx must in last stsc */
            break;
        }

        addr += 12;  /* goto next entry */
        /*  we are going to read 4 bytes */
        if (addr >= stsc->size - 4)
        {
            break;
        }
        first_chunk_next = get_BE_u32(stsc->data + addr) - 1;

        sample_idx0_next = stsc->sample_idx0 + (first_chunk_next - first_chunk)*samples_per_chunk;
        if (sample_idx < sample_idx0_next || stsc->entry_idx == stsc->entry_count-1)
        {
            /* stco is open ended, must check if it is last entry or not */
            break;
        }
        else
        {
            stsc->sample_idx0 = sample_idx0_next;
        }
    }

    /* offset within the stco entry */
    if (samples_per_chunk)
    {
        chunk_idx_in_stco_entry = (sample_idx - stsc->sample_idx0)/samples_per_chunk;
    }
    else
    {
        chunk_idx_in_stco_entry = 0;
    }

    /* the global idx */
    chunk_idx = first_chunk + chunk_idx_in_stco_entry;
    /* chunk addr */
    if (stream->stco.variant)
    {
        uint32_t offset = (chunk_idx << 3);
        if (offset <= stream->stco.size - 8)
        {
            chunk_byte_offset = get_BE_u64(stream->stco.data + offset);
        }
    }
    else
    {
        uint32_t offset = (chunk_idx << 2);
        if (offset <= stream->stco.size - 4)
        {
            chunk_byte_offset = get_BE_u32(stream->stco.data + offset);
        }
    }

    sample_idx_in_chunk = (sample_idx - stsc->sample_idx0) -
                          (chunk_idx_in_stco_entry * samples_per_chunk);

    /* the offset to the begining of the chunk */
    sample_byte_offset_in_chunk = 0;
    if (!stream->stsz.variant && stream->stsz.add_info)
    {
        /* fix size */
        sample_byte_offset_in_chunk = (uint64_t)sample_idx_in_chunk*(uint64_t)stream->stsz.add_info;
    }
    else
    {
        uint32_t i;
        for (i = sample_idx - sample_idx_in_chunk; i < sample_idx; i++)
        {
            uint32_t size = stream_get_sample_size(stream, i);
            sample_byte_offset_in_chunk += size;
        }
    }

    return chunk_byte_offset + sample_byte_offset_in_chunk;
}

uint32_t
stream_get_sample_max_size(stream_handle_t stream, uint32_t *fix_size)
{
    box_data_tbl_t *stsz = &(stream->stsz);

    if (fix_size)
    {
        if (!stsz->variant && stsz->add_info)
        {
            *fix_size = stsz->add_info;
        }
        else
        {
            *fix_size = 0;
        }
    }
    return stream->sample_max_size;
}

/** Finds the preceding keyframe of this sample or the sample itself if it's a sync */
uint32_t
stream_get_prev_sync_sample_idx(stream_handle_t stream, uint32_t sample_idx)
{
    box_data_tbl_t *stss = &(stream->stss);

    if (sample_idx >= stream->sample_num)
    {
        sample_idx = stream->sample_num - 1;  /* the last one */
    }

    if (!stss->entry_count)
    {
        /* no table => all samples are key samples */
        return sample_idx;
    }

    if (sample_idx < stss->sample_idx0)
    {
        /* get backward: start from beginning */
        stss->sample_idx0 = get_BE_u32(stss->data) - 1;  /* first sync */
        stss->entry_idx   = 0;
    }

    for (; stss->entry_idx < stss->entry_count - 1; stss->entry_idx++)
    {
        /* get the next sync */
        uint32_t sample_number = 0;
        uint32_t offset        = ((stss->entry_idx+1) << 2);
        if (offset > stss->size - 4)
        {
            break;
        }
        sample_number = get_BE_u32(stss->data + offset) - 1;
        if (sample_idx < sample_number)
        {
            break;
        }
        stss->sample_idx0 = sample_number;
    }

    return (sample_idx < stss->sample_idx0) ? (uint32_t)-1 : stss->sample_idx0;
}

uint32_t
stream_get_track_media_timescale(stream_handle_t stream)
{
    return stream->media_timescale;
}

uint64_t
stream_get_track_duration(stream_handle_t stream)
{
    assert(stream);

    return stream->sum_track_edits;
}

uint64_t
stream_get_track_media_duration(stream_handle_t stream)
{
    assert(stream);

    return stream->media_duration;
}

uint32_t
stream_get_track_frame_num(stream_handle_t stream)
{
    assert(stream);

    return stream->sample_num;
}

uint32_t
stream_get_track_flags(stream_handle_t stream)
{
    assert(stream);

    return stream->flags;
}

uint32_t
stream_get_track_id(stream_handle_t stream)
{
    assert(stream);

    return stream->track_ID;
}

stream_type_t
stream_get_track_stream_type(stream_handle_t stream)
{
    assert(stream);

    return stream->stream_type;
}

const int8_t *
stream_get_track_stream_name(stream_handle_t stream)
{
    if (!stream)
    {
        return NULL;
    }

    if (!stream->parser)
    {
        return NULL;
    }

    return stream->parser->stream_name;
}

stream_id_t
stream_get_track_stream_id(stream_handle_t stream)
{
    if (!stream)
    {
        return STREAM_ID_UNKNOWN;
    }

    if (!stream->parser)
    {
        return STREAM_ID_UNKNOWN;
    }

    return stream->parser->stream_id;
}

int32_t
stream_get_video_track_image_info(stream_handle_t stream,
                                  uint32_t *      video_width,
                                  uint32_t *      video_height,
                                  uint32_t *      video_pixel_depth)
{
    parser_video_handle_t parser_video;

    if (!stream)
    {
        return -1;
    }

    if (stream->stream_type != STREAM_TYPE_VIDEO)
    {
        return -2;
    }

    parser_video = (parser_video_handle_t)stream->parser;
    if (video_width)
    {
        *video_width = parser_video->width;
    }
    if (video_height)
    {
        *video_height = parser_video->height;
    }
    if (video_pixel_depth)
    {
        *video_pixel_depth = parser_video->depth;
    }

    return 0;
}

int32_t
stream_get_audio_track_channelcount(stream_handle_t stream)
{
    parser_audio_handle_t parser_audio;

    if (!stream)
    {
        return -1;
    }

    if (stream->stream_type != STREAM_TYPE_AUDIO)
    {
        return -2;
    }

    parser_audio = (parser_audio_handle_t)stream->parser;

    return parser_audio->channelcount;
}

int32_t
stream_get_audio_track_sample_rate(stream_handle_t stream)
{
    parser_audio_handle_t parser_audio;

    if (!stream)
    {
        return -1;
    }

    if (stream->stream_type != STREAM_TYPE_AUDIO)
    {
        return -2;
    }

    parser_audio = (parser_audio_handle_t)stream->parser;

    return parser_audio->sample_rate;
}

void
stream_destroy (stream_handle_t stream)
{
    idx_ptr_t * idx_ptr;

    if (stream)
    {
        FREE_CHK(stream->dsi_buf);

        list_destroy(stream->dts_lst);
        list_destroy(stream->cts_offset_lst);
        list_destroy(stream->sync_lst);
        list_destroy(stream->edt_lst);

        list_destroy(stream->size_lst);
        list_destroy(stream->chunk_lst);

        if (stream->stsd_lst)
        {
            list_it_init(stream->stsd_lst);
            while ((idx_ptr = list_it_get_entry(stream->stsd_lst)))
            {
                FREE_CHK(idx_ptr->ptr);
            }
            list_destroy(stream->stsd_lst);
        }
        list_destroy(stream->sdtp_lst);
        list_destroy(stream->trik_lst);
        list_destroy(stream->frame_type_lst);
        list_destroy(stream->subs_lst);
        list_destroy(stream->segment_lst);
#ifdef ENABLE_MP4_ENCRYPTION
        list_destroy(stream->enc_info_lst);
        it_destroy(stream->enc_info_mdat_it);
#endif

        if (stream->file)
        {
            fclose(stream->file);
            if (stream->es_tmp_fn[0] != '\0')
            {
                /* my tmp file */
                OSAL_DEL_FILE(stream->es_tmp_fn);
            }
        }

        /** fragment */
        list_destroy(stream->pos_lst);
        it_destroy(stream->size_it);
        list_destroy(stream->tfra_entry_lst);

        /* for demux */

        FREE_CHK(stream->name);

        FREE_CHK(stream->elst.data);
        FREE_CHK(stream->sdp_text);
        FREE_CHK(stream->drefs);

        /* free sample table data */
        FREE_CHK(stream->stco.data);
        FREE_CHK(stream->stts.data);
        FREE_CHK(stream->ctts.data);
        FREE_CHK(stream->stsc.data);
        FREE_CHK(stream->stsz.data);
        FREE_CHK(stream->stss.data);
        FREE_CHK(stream->stsd.data);

        /** fragment */
        if (stream->frag_snk_file)
        {
            stream->frag_snk_file->destroy(stream->frag_snk_file);
        }

        if (stream->parser)
        {
            stream->parser->destroy(stream->parser);
        }

        if (stream->frag_samples)
        {
            FREE_CHK(stream->frag_samples);
        }

        FREE_CHK(stream);
    }
}
