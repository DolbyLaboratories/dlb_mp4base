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
    @file parser_aac.c
    @brief Implements an aac parser
*/

#include "utils.h"
#include "io_base.h"
#include "registry.h"
#include "dsi.h"
#include "parser.h"
#include "parser_aac.h"

/* 0 for reserved and escape value */
static const uint32_t sfi_2_freq_tbl[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025,  8000,  7350,     0,     0,     0
};

static int      parser_aac_read_pce_element_channel_count(bbio_handle_t   src
                                                         , int            num_elements
                                                         , uint8_t      **element_is_cpe
                                                         , uint8_t      **element_tag_select
                                                         );
static void     parser_aac_check_ccff_conformance(parser_aac_handle_t parser);
static uint32_t parser_aac_get_channel_count(parser_aac_handle_t parser_aac);

/* while performing resync */
static int /** @return -1: unsupported (multiple AAC frames), 0: no sync found, 1: sync found */
parser_aac_adts_hdr(parser_aac_handle_t parser_aac, bbio_handle_t bs)
{
    uint32_t val;
    offset_t pos_sync;
    int32_t  len_remain;

    while (!bs->is_EOD(bs))
    {
        pos_sync = bs->position(bs); /* for resync */

        /**** fixed header */
        /** syncword */
        val = src_read_u8(bs);
        if (val != 0xFF)
        {
            continue;
        }
        val = src_read_bits(bs, 4);
        if (val != 0x0F)
        {
            src_read_bits(bs, 4);
            continue;
        }

        parser_aac->ID = src_read_bit(bs);                            /* 1: 13818-7, 0: 14496-3 */

        /* test only */
        if (parser_aac->ID)
        {
            msglog(NULL, MSGLOG_DEBUG, "ID==1(MPEG2 profile) not fully supported\n");
        }

        src_skip_bits(bs, 2);                                         /* layer */
        parser_aac->protection_absent = src_read_bit(bs);             /* 0 => crc present */

        parser_aac->profile_ObjectType = src_read_bits(bs, 2);
        /* test only */
        if (parser_aac->profile_ObjectType == 2)
        {
            msglog(NULL, MSGLOG_DEBUG, "profile_ObjectType == 2(AAC SSR) not fully supported\n");
        }

        parser_aac->sampling_frequency_index = src_read_bits(bs, 4);
        src_skip_bits(bs, 1);                                         /* private_bit */
        parser_aac->channel_configuration = src_read_bits(bs, 3);
        src_skip_bits(bs, 2);                                         /* original_copy, home bits */

        if (parser_aac->channel_configuration == 0)
        {
            /*
             * The only time we are likely to see a channel_configuration of 0
             * is doing 6-channel encoding and mpeg metadata.  We should extract
             * the correct information from the PCE, but for the moment, we just
             * hard-code the channel configuration to 6.
             */
            msglog(NULL, MSGLOG_WARNING, "channel_configuration is 0: overriding to 6\n");
            parser_aac->channel_configuration = 6;
        }

        /** variable header */
        src_skip_bits(bs, 2);               /* copyright_identification_bit/start bits */
        len_remain /* = parser_aac->aac_frame_length_remain */ = src_read_bits(bs, 13);
        parser_aac->adts_buffer_fullness               = src_read_bits(bs, 11);
        parser_aac->number_of_raw_data_blocks_in_frame = src_read_bits(bs, 2);

        len_remain -= 7;  /* fixed + variable hdr done */
        if (!parser_aac->number_of_raw_data_blocks_in_frame)
        {
            if (!parser_aac->protection_absent)
            {
                bs->skip_bytes(bs, 2); /* the 2 bytes crc */
                len_remain -= 2;
            }
            /* we at raw_data_block now */
        }
        else
        {
            msglog(NULL, MSGLOG_INFO, "number_of_raw_data_blocks_in_frame=%u\n",
                   parser_aac->number_of_raw_data_blocks_in_frame);
            if (!parser_aac->protection_absent)
            {
                /* the adts_header_error_check. raw_data_block_position[0] == 0 */
                for (val = 1; val <= parser_aac->number_of_raw_data_blocks_in_frame; val++)
                {
                    parser_aac->raw_data_block_position[val] = src_read_u16(bs);
                }
                bs->skip_bytes(bs, 2); /* crc */
                len_remain -= 2*(parser_aac->number_of_raw_data_blocks_in_frame + 1);
            }
            else
            {
                /* assume same frame size ??? */ /* this is almost never the case! */
                parser_aac->frame_size = len_remain/(parser_aac->number_of_raw_data_blocks_in_frame + 1);
            }
            /* we at first raw_data_block now */
        }

        /** sync double check */
        if (len_remain < 0)
        {
            bs->seek(bs, pos_sync+1, SEEK_SET);
            continue;
        }

        if (bs->size(bs) - bs->position(bs) != len_remain)
        {
            offset_t pos_raw = bs->position(bs);
            bs->skip_bytes(bs, len_remain);
            val = src_read_u8(bs);
            if (val != 0xFF)
            {
                bs->seek(bs, pos_sync+1, SEEK_SET);
                continue;
            }
            val = src_read_bits(bs, 4);
            if (val != 0x0F)
            {
                src_byte_align(bs); /* will continue on align boundary */
                bs->seek(bs, pos_sync+2, SEEK_SET);
                continue;
            }

            bs->seek(bs, pos_raw, SEEK_SET);
            src_byte_align(bs); /* to align it */
        }

        /* second sync found - we now trust the ADTS header values */
        if (parser_aac->number_of_raw_data_blocks_in_frame)
        {
            /* multiple AAC frames per ADTS frame are not supported
             * - at least for protection_absent==1 we can't get the AAC frames without AAC decoder help
             */
            msglog(NULL, MSGLOG_ERR, "multiple AAC frames per ADTS frame are not supported\n");
            return -1;
        }

        parser_aac->aac_frame_length_remain = len_remain;
        parser_aac->raw_data_block_idx      = 0;
        return 1;
    }

    return 0;
}


/* Fills dsi with properties in ADTS header */
static void
parser_aac_init_dsi(parser_handle_t parser) 
{
    parser_aac_handle_t  parser_aac = (parser_aac_handle_t)parser;
    mp4_dsi_aac_handle_t dsi        = (mp4_dsi_aac_handle_t)parser->curr_dsi;

    if (parser_aac->ID)
    {
        msglog(NULL, MSGLOG_WARNING, "\nWARNING: ID==1(MPEG2 profile) not fully supported\n");
    }

    /* test only */
    if (parser_aac->profile_ObjectType == 2)
    {
        msglog(NULL, MSGLOG_WARNING, "\nWARNING: profile_ObjectType == 2(AAC SSR) not fully supported\n");
    }

    /* assuming frameLengthFlag == 1, not AAC SSR */
    parser_aac->samples_per_frame = (parser_aac->number_of_raw_data_blocks_in_frame + 1)*1024;
    parser_aac->sample_rate       = sfi_2_freq_tbl[parser_aac->sampling_frequency_index];
    parser_aac->time_scale        = parser_aac->sample_rate;
    if (!parser_aac->number_of_raw_data_blocks_in_frame)
    {
        parser_aac->frame_size = parser_aac->aac_frame_length_remain;
    }
    else if (!parser_aac->protection_absent)
    {
        parser_aac->frame_size = parser_aac->raw_data_block_position[1] -
                                 parser_aac->raw_data_block_position[0] - 2;
    }
    /* else, fix size already set up in parser_aac_adts_hdr() */
    parser_aac->channelcount = parser_aac_get_channel_count(parser_aac);
    if ((parser_aac->channelcount == 6) || (parser_aac->channelcount == 8))
    {
        parser->buferSizeDB      = (parser_aac->channelcount - 1) * 768 * 8; /* for 5.1 and 7.1, remove the lfe */
    }
    else
    {
        parser->buferSizeDB      = parser_aac->channelcount * 768 * 8;    
    }

    dsi->audioObjectType        = (uint8_t)parser_aac->profile_ObjectType + 1;
    dsi->samplingFrequencyIndex = (uint8_t)parser_aac->sampling_frequency_index;
    dsi->samplingFrequency      = parser_aac->sample_rate;
    dsi->channelConfiguration   = (uint8_t)parser_aac->channel_configuration;
    dsi->channel_count          = (uint8_t)parser_aac->channelcount;
    dsi->esd.bufferSizeDB       = parser->buferSizeDB;

    dsi->element_instance_tag         = 0;
    dsi->object_type                  = 0;
    dsi->pce_sampling_frequency_index = 0;
    dsi->num_front_channel_elements   = 0;
    dsi->num_side_channel_elements    = 0;
    dsi->num_back_channel_elements    = 0;
    dsi->num_lfe_channel_elements     = 0;
    dsi->num_assoc_data_elements      = 0;
    dsi->num_valid_cc_elements        = 0;
    dsi->mono_mixdown_present         = 0;

    dsi->mono_mixdown_element_number   = 0;
    dsi->stereo_mixdown_present        = 0;
    dsi->stereo_mixdown_element_number = 0;
    dsi->matrix_mixdown_idx_present    = 0;
    dsi->matrix_mixdown_idx            = 0;
    dsi->pseudo_surround_enable        = 0;
    dsi->front_element_is_cpe          = 0;
    dsi->front_element_tag_select      = 0;

    dsi->side_element_is_cpe     = 0;
    dsi->side_element_tag_select = 0;

    dsi->back_element_is_cpe     = 0;
    dsi->back_element_tag_select = 0;

    dsi->lfe_element_tag_select        = 0;
    dsi->assoc_data_element_tag_select = 0;
    dsi->cc_element_is_ind_sw          = 0;
    dsi->valid_cc_element_tag_select   = 0;
    dsi->comment_field_bytes           = 0;
    dsi->comment_field_data            = 0;

    if (IS_FOURCC_EQUAL(parser->conformance_type, "cffh") ||
        IS_FOURCC_EQUAL(parser->conformance_type, "cffs")) 
    {
        parser_aac_check_ccff_conformance(parser_aac);
    }

    DPRINTF(NULL, "Audio audioObjectType %u, sample_rate %u, channel_configuration %u, frame size %d\n",
            dsi->audioObjectType, parser_aac->sample_rate, parser_aac->channel_configuration, parser_aac->frame_size);
}

static int
parser_aac_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx,  bbio_handle_t ds)
{
    parser_aac_handle_t  parser_aac = (parser_aac_handle_t)parser;

    parser->ext_timing = *ext_timing;
    parser->es_idx     = es_idx;
    parser->ds         = ds;

    if (parser_aac_adts_hdr(parser_aac, ds) <= 0)
    {
        return EMA_MP4_MUXED_EOES; /* no valid hdr found */
    }

    parser_aac_init_dsi(parser);

    /* let mux_es_parsing() start from very beginning */
    ds->seek(ds, 0, SEEK_SET);
    parser_aac->aac_frame_length_remain = 0;

    return EMA_MP4_MUXED_OK;
}


static int
parser_aac_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    parser_aac_handle_t  parser_aac = (parser_aac_handle_t)parser;
    mp4_dsi_aac_handle_t curr_dsi   = (mp4_dsi_aac_handle_t)parser->curr_dsi;
    bbio_handle_t        ds         = parser->ds;
    int                  sync_frame;

#if PARSE_DURATION_TEST
    if (parser_aac->sample_num && sample->dts >= PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    sample->flags = 0;

    if (ds->is_EOD(ds))
    {
        return EMA_MP4_MUXED_EOES;
    }

    if (!parser_aac->aac_frame_length_remain)
    {
        /* get a new adts frame */
        sync_frame = parser_aac_adts_hdr(parser_aac, ds);
        if (sync_frame < 0)
        {
            return EMA_MP4_MUXED_NO_SUPPORT;
        }
        if (!parser_aac->sample_num && !sync_frame)
        {
            return EMA_MP4_MUXED_EOES;
        }
        /* last frame will come here too */
    }

    /* Check for configuration changes */
    if (curr_dsi->audioObjectType        != parser_aac->profile_ObjectType + 1   ||
        curr_dsi->samplingFrequencyIndex != parser_aac->sampling_frequency_index ||
        curr_dsi->channelConfiguration   != parser_aac->channel_configuration    ||
        curr_dsi->esd.bufferSizeDB       != parser->buferSizeDB)
    {
        dsi_handle_t  new_dsi;
        dsi_handle_t* p_new_dsi;

        if (curr_dsi->samplingFrequencyIndex != parser_aac->sampling_frequency_index)
        {
            msglog(NULL, MSGLOG_ERR, "change in AAC sampling rate is not allowed / supported\n");
            return EMA_MP4_MUXED_CONFIG_ERR;
        }

        /* Create new sample description for new configuration */
        new_dsi   = parser->dsi_create(parser->dsi_type);
        if (!new_dsi)
        {
            return EMA_MP4_MUXED_NO_MEM;
        }
        p_new_dsi = (dsi_handle_t*)list_alloc_entry(parser->dsi_lst);
        if (!p_new_dsi)
        {
            new_dsi->destroy(new_dsi);
            return EMA_MP4_MUXED_NO_MEM;
        }
        *p_new_dsi = new_dsi;

        /* Switch to new entry in dsi list */
        list_add_entry(parser->dsi_lst, p_new_dsi);
        parser->curr_dsi = new_dsi;

        /* Signal to muxer that new stsd entry has to be written */
        sample->flags |= SAMPLE_NEW_SD;

        /* Init new dsi */
        parser_aac_init_dsi(parser);
    }

    sample->flags |= SAMPLE_SYNC; /* all audio samples are sync frames */
    if (parser_aac->sample_num)
    {
        sample->dts += parser_aac->samples_per_frame;
    }
    else
    {
        sample->flags |= SAMPLE_NEW_SD; /* the first one should have all the new info */
        sample->dts = 0;
    }
    sample->cts      = sample->dts;
    sample->duration = parser_aac->samples_per_frame;

    /* sample->pos = bs->position(ds); */
    if (!parser_aac->number_of_raw_data_blocks_in_frame)
    {
        parser_aac->frame_size              = parser_aac->aac_frame_length_remain;
        parser_aac->aac_frame_length_remain = 0;
    }
    else if (!parser_aac->protection_absent)
    {
        if (parser_aac->raw_data_block_idx < parser_aac->number_of_raw_data_blocks_in_frame)
        {
            uint16_t len = parser_aac->raw_data_block_position[parser_aac->raw_data_block_idx+1] -
                           parser_aac->raw_data_block_position[parser_aac->raw_data_block_idx];

            parser_aac->frame_size               = len - 2;
            parser_aac->aac_frame_length_remain -= len;
            parser_aac->raw_data_block_idx++;
        }
        else
        {
            /* the last one */
            parser_aac->frame_size              = parser_aac->aac_frame_length_remain - 2;
            parser_aac->aac_frame_length_remain = 0;
        }
    }
    else
    {
        /* assume fixed size for each raw_data_block, already know when parsing the hdr */
        parser_aac->aac_frame_length_remain -= parser_aac->frame_size;
    }

    if (parser_aac->frame_size > parser_aac->sample_buf_size)
    {
        sample->data = REALLOC_CHK(sample->data, parser_aac->frame_size);
        parser_aac->sample_buf_size = parser_aac->frame_size;
    }
    sample->size = parser_aac->frame_size;
    ds->read(ds, sample->data, parser_aac->frame_size);

    if (parser_aac->number_of_raw_data_blocks_in_frame && !parser_aac->protection_absent)
    {
        ds->skip_bytes(ds, 2); /* crc */
    }

    parser_aac->sample_num++;

    DPRINTF(NULL, "frame size %d\n", parser_aac->frame_size);

    return EMA_MP4_MUXED_OK;
}

static int
parser_aac_read_pce_element_channel_count(bbio_handle_t src, int num_elements, uint8_t **element_is_cpe, uint8_t **element_tag_select)
{
    int channel_count = 0;
    int i;

    *element_is_cpe     = REALLOC_CHK(*element_is_cpe,     num_elements * sizeof(uint8_t));
    *element_tag_select = REALLOC_CHK(*element_tag_select, num_elements * sizeof(uint8_t));
    for (i = 0; i < num_elements; i++)
    {
        (*element_is_cpe)[i]      = (uint8_t)src_read_bits(src, 1);
        channel_count            += (*element_is_cpe)[i] == 1 ? 2 : 1;
        (*element_tag_select)[i]  = (uint8_t)src_read_bits(src, 4);
    }
    return channel_count;
}

static void
parser_aac_write_pce_channel_element(bbio_handle_t sink, int num_elements, uint8_t *element_is_cpe, uint8_t *element_tag_select)
{
    int i;
    for (i = 0; i < num_elements; i++)
    {
        sink_write_bit(sink, element_is_cpe[i]);
        sink_write_bits(sink, 4, element_tag_select[i]);
    }
}

static void
parser_aac_read_audio_object_type_data(bbio_handle_t src, uint8_t *aot, uint8_t *aotExt)
{
    *aot = (uint8_t)src_read_bits(src, 5);
    if (*aot == 31)
    {
        *aotExt = (uint8_t)src_read_bits(src, 6);
    }
}

static int
parser_aac_get_audio_object_type(uint8_t aot, uint8_t aotExt)
{
    int actualAot = aot;
    if (aot == 31)
    {
        actualAot = 32 + aotExt;
    }

    return actualAot;
}

static void
parser_aac_write_audio_object_type_data( bbio_handle_t sink, uint8_t aot, uint8_t aotExt)
{
    sink_write_bits(sink, 5, aot);
    if (aot == 31)
    {
        sink_write_bits(sink, 6, aotExt);
    }
}

static void
parser_aac_read_sampling_frequency(bbio_handle_t src, uint8_t *fs_index, uint32_t *fs)
{
    *fs_index = (uint8_t)src_read_bits(src, 4);
    if (*fs_index == 0xf)
    {
        *fs = src_read_bits(src, 24);
    }
}

static void
parser_aac_write_sampling_frequency(bbio_handle_t sink, uint8_t fs_index, uint32_t fs)
{
    sink_write_bits(sink, 4, fs_index);
    if (fs_index == 0xf)
    {
        sink_write_bits(sink, 24, fs);
    }
}

/**
 * @brief Converts curr_dsi to ASC and write to sink
 */
static int
parser_aac_write_binary_dsis(parser_handle_t parser, bbio_handle_t sink)
{
    mp4_dsi_aac_handle_t dsi     = (mp4_dsi_aac_handle_t)parser->curr_dsi;
    int                  aot     = 0;
    int                  ext_aot = 0;

    aot = parser_aac_get_audio_object_type(dsi->audioObjectType, dsi->audioObjectTypeExt);
    parser_aac_write_audio_object_type_data(sink, dsi->audioObjectType, dsi->audioObjectTypeExt);

    parser_aac_write_sampling_frequency(sink, dsi->samplingFrequencyIndex, dsi->samplingFrequency);

    sink_write_bits(sink, 4, dsi->channelConfiguration);

    /* Non backward compatible signaling */
    if (aot == AOT_SBR || aot == AOT_PS)
    {
        ext_aot = 5;
        parser_aac_write_sampling_frequency(sink, dsi->sbr_sampling_frequency_index, dsi->sbr_sampling_frequency);
        parser_aac_write_audio_object_type_data(sink, dsi->audioObjectType2, dsi->audioObjectTypeExt2);
        aot = parser_aac_get_audio_object_type(dsi->audioObjectType2, dsi->audioObjectTypeExt2);
        if (aot == AOT_ER_BSAC)
        {
            sink_write_bits(sink, 4, dsi->extensionChannelConfiguration);
        }
    }
    else
    {
        ext_aot = 0;
    }

    sink_write_bit(sink, dsi->frameLengthFlag);
    sink_write_bit(sink, dsi->dependsOnCoreCoder);
    if (dsi->dependsOnCoreCoder)
    {
        sink_write_bits(sink, 14, dsi->coreCoderDelay);
    }
    sink_write_bit(sink, dsi->extensionFlag);
    if (dsi->channelConfiguration == 0)
    {
        int i;
        sink_write_bits(sink, 4, dsi->element_instance_tag);
        sink_write_bits(sink, 2, dsi->object_type);
        sink_write_bits(sink, 4, dsi->pce_sampling_frequency_index);
        sink_write_bits(sink, 4, dsi->num_front_channel_elements);
        sink_write_bits(sink, 4, dsi->num_side_channel_elements);
        sink_write_bits(sink, 4, dsi->num_back_channel_elements);
        sink_write_bits(sink, 2, dsi->num_lfe_channel_elements);
        sink_write_bits(sink, 3, dsi->num_assoc_data_elements);
        sink_write_bits(sink, 4, dsi->num_valid_cc_elements);

        sink_write_bit(sink, dsi->mono_mixdown_present);
        if (dsi->mono_mixdown_present)
        {
            sink_write_bits(sink, 4, dsi->mono_mixdown_element_number);
        }

        sink_write_bit(sink, dsi->stereo_mixdown_present);
        if (dsi->stereo_mixdown_present)
        {
            sink_write_bits(sink, 4, dsi->stereo_mixdown_element_number);
        }

        sink_write_bit(sink, dsi->matrix_mixdown_idx_present);
        if (dsi->matrix_mixdown_idx_present)
        {
            sink_write_bits(sink, 2, dsi->matrix_mixdown_idx);
            sink_write_bit(sink, dsi->pseudo_surround_enable);
        }
        parser_aac_write_pce_channel_element(sink, dsi->num_front_channel_elements, dsi->front_element_is_cpe, dsi->front_element_tag_select);
        parser_aac_write_pce_channel_element(sink, dsi->num_side_channel_elements, dsi->side_element_is_cpe, dsi->side_element_tag_select);
        parser_aac_write_pce_channel_element(sink, dsi->num_back_channel_elements, dsi->back_element_is_cpe, dsi->back_element_tag_select);

        for (i = 0; i < dsi->num_lfe_channel_elements; i++)
        {
            sink_write_bits(sink, 4, dsi->lfe_element_tag_select[i]);
        }

        for (i = 0; i < dsi->num_assoc_data_elements; i++)
        {
            sink_write_bits(sink, 4, dsi->assoc_data_element_tag_select[i]);
        }

        parser_aac_write_pce_channel_element(sink, dsi->num_valid_cc_elements, dsi->cc_element_is_ind_sw, dsi->valid_cc_element_tag_select);
        sink_flush_bits(sink);
        for (i = 0; i < dsi->comment_field_bytes; i++)
        {
            sink_write_bits(sink, 8, dsi->comment_field_data[i]);
        }
    }
    if (ext_aot != AOT_SBR && dsi->have_sbr_ext)
    {
        sink_write_bits(sink, 11, 0x2b7);
        parser_aac_write_audio_object_type_data(sink, dsi->extensionAudioObjectType, dsi->extensionAudioObjectTypeExt);
        ext_aot = parser_aac_get_audio_object_type(dsi->extensionAudioObjectType, dsi->extensionAudioObjectTypeExt);
        if (ext_aot == AOT_SBR)
        {
            sink_write_bit(sink, dsi->has_sbr);
            if (dsi->has_sbr)
            {
                parser_aac_write_sampling_frequency(sink, dsi->sbr_sampling_frequency_index, dsi->sbr_sampling_frequency);
            }
            if (dsi->have_ps_ext)
            {
                sink_write_bits(sink, 11, 0x548);
                sink_write_bit(sink, dsi->has_ps);
            }
        }
    }
    if (ext_aot == AOT_ER_BSAC)
    {
        sink_write_bit(sink, dsi->has_sbr);
        if (dsi->has_sbr)
        {
            parser_aac_write_sampling_frequency(sink, dsi->sbr_sampling_frequency_index, dsi->sbr_sampling_frequency);
        }
        sink_write_bits(sink, 4, dsi->extensionChannelConfiguration);
    }
    sink_flush_bits(sink);

    return 0;
}

/**
 * @brief Parses curr_codec_config (i.e. ASC) into curr_dsi
 *
 * curr_codec_config is expected to be set when this function / method is called
 * typically, curr_codec_config is set to one entry in codec_config_list
 */
static int
parser_aac_codec_config(parser_handle_t parser, bbio_handle_t info_sink)
{
    mp4_dsi_aac_handle_t dsi = (mp4_dsi_aac_handle_t)parser->curr_dsi;

    int extension_audio_object_type = 0;
    int aot;

    bbio_handle_t src;

    if (!parser->curr_codec_config || !parser->curr_codec_config->codec_config_size)
    {
        msglog(NULL, MSGLOG_WARNING, "parser_aac_codec_config: invalid curr_codec_config or empty codec_config\n");
        return EMA_MP4_MUXED_OK;
    }

    src = reg_bbio_get('b', 'r');
    src->set_buffer(src, (uint8_t *)parser->curr_codec_config->codec_config_data, parser->curr_codec_config->codec_config_size, 0);

    parser_aac_read_audio_object_type_data(src, &dsi->audioObjectType, &dsi->audioObjectTypeExt);
    aot = parser_aac_get_audio_object_type(dsi->audioObjectType, dsi->audioObjectTypeExt);

    parser_aac_read_sampling_frequency(src, &dsi->samplingFrequencyIndex, &dsi->samplingFrequency);

    dsi->channelConfiguration = (uint8_t)src_read_bits(src, 4);

    if (aot == AOT_SBR || aot == AOT_PS)
    {
        extension_audio_object_type = AOT_SBR;
        dsi->has_sbr = TRUE;
        if (dsi->audioObjectType == AOT_PS)
        {
            dsi->has_ps = TRUE;
        }
        parser_aac_read_sampling_frequency(src, &dsi->sbr_sampling_frequency_index, &dsi->sbr_sampling_frequency);
        parser_aac_read_audio_object_type_data(src, &dsi->audioObjectType2, &dsi->audioObjectTypeExt2);
        aot = parser_aac_get_audio_object_type(dsi->audioObjectType2, dsi->audioObjectTypeExt2);

        if (aot == AOT_ER_BSAC)
        {
            dsi->extensionChannelConfiguration = (uint8_t)src_read_bits(src, 4); /* extensionChannelConfiguration; */
        }
    }
    else
    {
        extension_audio_object_type = 0;
    }

    /* GASpecificConfig */
    dsi->frameLengthFlag = src_read_bits(src, 1);

    dsi->dependsOnCoreCoder = src_read_bits(src, 1);
    if (dsi->dependsOnCoreCoder) /* depends on core coder */
    {
        dsi->coreCoderDelay = (uint8_t)src_read_bits(src, 14);
    }

    dsi->extensionFlag = src_read_bits(src, 1);

    /* Read from ProgramConfigElement if channelConfiguration == 0 to get the channel config */
    if (dsi->channelConfiguration == 0)
    {
        int i;

        dsi->channel_count = 0;

        dsi->element_instance_tag = (uint8_t)src_read_bits(src, 4);
        dsi->object_type          = (uint8_t)src_read_bits(src, 2);

        dsi->pce_sampling_frequency_index = (uint8_t)src_read_bits(src, 4);

        dsi->num_front_channel_elements = (uint8_t)src_read_bits(src, 4);
        dsi->num_side_channel_elements  = (uint8_t)src_read_bits(src, 4);
        dsi->num_back_channel_elements  = (uint8_t)src_read_bits(src, 4);
        dsi->num_lfe_channel_elements   = (uint8_t)src_read_bits(src, 2);
        dsi->num_assoc_data_elements    = (uint8_t)src_read_bits(src, 3);
        dsi->num_valid_cc_elements      = (uint8_t)src_read_bits(src, 4);

        dsi->mono_mixdown_present = (uint8_t)src_read_bits(src, 1);
        if (dsi->mono_mixdown_present)
        {
            dsi->mono_mixdown_element_number = (uint8_t)src_read_bits(src, 4);
        }

        dsi->stereo_mixdown_present = (uint8_t)src_read_bits(src,1);
        if (dsi->stereo_mixdown_present)
        {
            dsi->stereo_mixdown_element_number = (uint8_t)src_read_bits(src, 4);
        }
        dsi->matrix_mixdown_idx_present = (uint8_t)src_read_bits(src,1);
        if (dsi->matrix_mixdown_idx_present)
        {
            /* This is the MPEG style downmix coefficient index. */
            dsi->matrix_mixdown_idx     = (uint8_t)src_read_bits(src, 2);
            dsi->pseudo_surround_enable = (uint8_t)src_read_bits(src, 1);
        }

        /* channel elements can be channel pairs. e.g L and R, Ls and Rs. This
         * functions gets the channel counts, determining if they are channel pairs
         * from the PCE data. */

        dsi->channel_count += (uint8_t)parser_aac_read_pce_element_channel_count(src,
                                                                        dsi->num_front_channel_elements,
                                                                        &dsi->front_element_is_cpe,
                                                                        &dsi->front_element_tag_select);

        dsi->channel_count += (uint8_t)parser_aac_read_pce_element_channel_count(src,
                                                                        dsi->num_side_channel_elements,
                                                                        &dsi->side_element_is_cpe,
                                                                        &dsi->side_element_tag_select);

        dsi->channel_count += (uint8_t)parser_aac_read_pce_element_channel_count(src,
                                                                        dsi->num_back_channel_elements,
                                                                        &dsi->back_element_is_cpe,
                                                                        &dsi->back_element_tag_select);

        dsi->channel_count += dsi->num_lfe_channel_elements;

        if (dsi->num_lfe_channel_elements)
        {
            if (dsi->lfe_element_tag_select)
            {
                FREE_CHK(dsi->lfe_element_tag_select);
            }
            dsi->lfe_element_tag_select = MALLOC_CHK(sizeof(uint8_t) * dsi->num_lfe_channel_elements);
        }

        for (i = 0; i < dsi->num_lfe_channel_elements; i++)
        {
            dsi->lfe_element_tag_select[i] = (uint8_t)src_read_bits(src, 4);
        }

        if (dsi->num_assoc_data_elements)
        {
            if (dsi->assoc_data_element_tag_select)
            {
                FREE_CHK(dsi->assoc_data_element_tag_select);
            }
            dsi->assoc_data_element_tag_select = MALLOC_CHK(sizeof(uint8_t) * dsi->num_assoc_data_elements);
        }

        for (i = 0; i < dsi->num_assoc_data_elements; i++)
        {
            dsi->assoc_data_element_tag_select[i] = (uint8_t)src_read_bits(src, 4);
        }

        /* not an element_channel but same binary format */
        parser_aac_read_pce_element_channel_count(src,
                                                  dsi->num_valid_cc_elements,
                                                  &dsi->cc_element_is_ind_sw,
                                                  &dsi->valid_cc_element_tag_select);

        src_byte_align(src);
        dsi->comment_field_bytes = (uint8_t)src_read_bits(src, 8);
        if (dsi->comment_field_bytes > 0)
        {
            dsi->comment_field_data = MALLOC_CHK(dsi->comment_field_bytes * sizeof(uint8_t));
        }
        if (dsi->comment_field_bytes > 0)
        {
            *dsi->comment_field_data = (uint8_t)src_read_bits(src, dsi->comment_field_bytes * 8);
        }
    }
    else
    {
        dsi->channel_count = dsi->channelConfiguration;
    }

    if (aot == AOT_AAC_SCALABLE || aot == AOT_ER_AAC_SCALABLE)
    {
        dsi->layerNr = (uint8_t)src_read_bits(src, 3); /* layerNr */
    }

    if (dsi->extensionFlag)
    {
        if (aot == AOT_ER_BSAC)
        {
            dsi->numOfSubFrame = (uint8_t)src_read_bits(src, 5);
            dsi->layer_length  = (uint16_t)src_read_bits(src, 11);
        }
        if(aot == AOT_ER_AAC_LC       ||
           aot == AOT_ER_AAC_LTP      ||
           aot == AOT_ER_AAC_SCALABLE ||
           aot == AOT_ER_AAC_LD)
        {
            dsi->aacSectionDataResilienceFlag     = (uint8_t)src_read_bits(src, 1);
            dsi->aacScalefactorDataResilienceFlag = (uint8_t)src_read_bits(src, 1);
            dsi->aacSpectralDataResilienceFlag    = (uint8_t)src_read_bits(src, 1);
        }
        dsi->extensionFlag3 = (uint8_t)src_read_bits(src, 1);
    }

    /* Back in AudioSpecificConfig so check for sbr and ps flags */

    if (extension_audio_object_type != AOT_SBR && !src->is_EOD(src))
    {
        int syncExtensionType = src_read_bits(src, 11);
        dsi->have_sbr_ext = TRUE;
        if (syncExtensionType == 0x2b7)
        {
            parser_aac_read_audio_object_type_data(src, &dsi->extensionAudioObjectType, &dsi->extensionAudioObjectTypeExt);
            extension_audio_object_type = parser_aac_get_audio_object_type(dsi->extensionAudioObjectType, dsi->extensionAudioObjectTypeExt);
            if (extension_audio_object_type == AOT_SBR)
            {
                dsi->has_sbr = src_read_bits(src, 1);
                if (dsi->has_sbr)
                {
                    parser_aac_read_sampling_frequency(src, &dsi->sbr_sampling_frequency_index, &dsi->sbr_sampling_frequency);
                    if (!src->is_EOD(src))
                    {
                        dsi->have_ps_ext  = TRUE;
                        syncExtensionType = src_read_bits(src, 11);
                        if (syncExtensionType == 0x548)
                        {
                            dsi->has_ps = src_read_bits(src, 1);
                        }
                    }
                }
            }
            if (extension_audio_object_type == AOT_ER_BSAC)
            {
                dsi->has_sbr = src_read_bits(src, 1);
                if (dsi->has_sbr)
                {
                    parser_aac_read_sampling_frequency(src, &dsi->sbr_sampling_frequency_index, &dsi->sbr_sampling_frequency);
                }
                dsi->extensionChannelConfiguration = (uint8_t)src_read_bits(src, 4); /* extensionChannelConfiguration */
            }
        }
    }

    src->destroy(src);

    return EMA_MP4_MUXED_OK;
    (void)info_sink;  /* avoid compiler warning */
}

/**
 * @brief Writes "curr_codec_config" using curr_dsi to buf
 */
static int
parser_aac_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    bbio_handle_t sink = NULL;

    /* Write it out to the buffer */
    sink = reg_bbio_get('b', 'w');
    sink->set_buffer(sink, NULL, 32, 0);           /* 32 bytes is more then enough */
    parser_aac_write_binary_dsis(parser, sink);
    *buf = sink->get_buffer(sink, buf_len, NULL);  /* here buf_len is set to data_size */
    sink->destroy(sink);

    return 0;
}

/**
 * @brief Builds ADTS header and write to sink
 *
 * for the first frame or on config changes:
 * - curr_codec_config is set to the dsi_curr_index entry of the codec_config_list
 * - curr_dsi is set up using curr_codec_config
 * the ADTS header is set up using values from curr_dsi
 */
static uint8_t *
parser_aac_write_mp4_cfg(parser_handle_t parser, bbio_handle_t sink)
{
    parser_aac_handle_t parser_aac = (parser_aac_handle_t)parser;
    uint16_t            aac_frame_size;

    if (!list_get_entry_num(parser_aac->codec_config_lst))
    {
        /* the dsi is missing: stsd is not right */
        return NULL;
    }

    aac_frame_size = (uint16_t)parser_aac->frame_size + 7;
    if (!parser_aac->adts_hdr_buf ||    /* first frame */
        !parser_aac->curr_dsi)          /* config changes */
    {
        /**** build the 7 byte adts hdr */
        mp4_dsi_aac_handle_t dsi = NULL;

        bbio_handle_t srcb;
        bbio_handle_t snk;
        size_t        size;
        uint32_t      i = 0;

        /* Get current codec config */
        it_list_handle_t it = it_create();
        it_init(it, parser->codec_config_lst);
        for (i = 0; i < parser->dsi_curr_index; i++)
        {
            parser_aac->curr_codec_config = (codec_config_t*)it_get_entry(it);
        }
        it_destroy(it);

        if (!parser_aac->curr_codec_config || !parser_aac->curr_codec_config->codec_config_data)
        {
            /* the dsi is missing: stds is not right */
            return NULL;
        }

        /* Create new entry in dsi list if necessary */
        if (!parser->curr_dsi)
        {
            dsi_handle_t* p_new_dsi = (dsi_handle_t*)list_alloc_entry(parser->dsi_lst);
            parser->curr_dsi = parser->dsi_create(parser->dsi_type);
            *p_new_dsi = parser->curr_dsi;
            list_add_entry(parser->dsi_lst, p_new_dsi);
        }
        dsi = (mp4_dsi_aac_handle_t)parser->curr_dsi;

        /* Delete buffer from last run */
        FREE_CHK(parser_aac->adts_hdr_buf);

        /** get dsi from mp4 file(codec_config) */
        srcb = reg_bbio_get('b', 'r');
        srcb->set_buffer(srcb, parser_aac->curr_codec_config->codec_config_data, parser_aac->curr_codec_config->codec_config_size, 0);

        dsi->audioObjectType        = (uint8_t)src_read_bits(srcb, 5);
        dsi->samplingFrequencyIndex = (uint8_t)src_read_bits(srcb, 4);
        assert(dsi->samplingFrequencyIndex != 0xF); /* adts case */
        dsi->channelConfiguration = (uint8_t)src_read_bits(srcb, 4);

        srcb->destroy(srcb);

        /** build the 7 byte adts hdr */
        snk = reg_bbio_get('b', 'w');
        snk->set_buffer(snk, 0, 7, 0);                          /* pre-alloc 7 byte buf */

        sink_write_u8(snk, 0xFF);
        sink_write_bits(snk, 4, 0xF);
        sink_write_bits(snk, 4, 0x1);                           /* ID = 0, layer = 0, protection_absent = 1 */

        sink_write_bits(snk, 2,  dsi->audioObjectType-1);
        sink_write_bits(snk, 4,  dsi->samplingFrequencyIndex);
        sink_write_bits(snk, 1,  0x0);                          /* private_bit = 0 */
        sink_write_bits(snk, 3,  dsi->channelConfiguration);
        sink_write_bits(snk, 4,  0x0);                          /* original_copy, home, copyright_* */
        sink_write_bits(snk, 13, aac_frame_size);
        sink_write_bits(snk, 11, 0x7FF);                        /* adts_buffer_fullness: vbr */
        sink_write_bits(snk, 2,  0);                            /* number_of_raw_data_block_in_frame */
        sink_flush_bits(snk);

        parser_aac->adts_hdr_buf = snk->get_buffer(snk, &size, 0);
        assert(size == 7);

        snk->destroy(snk);
    }

    /* update aac_frame_size */
    parser_aac->adts_hdr_buf[3] = (parser_aac->adts_hdr_buf[3] & 0xFC) | (aac_frame_size >> 11);
    parser_aac->adts_hdr_buf[4] = (aac_frame_size >> 3) & 0xFF;
    parser_aac->adts_hdr_buf[5] = (uint8_t)((parser_aac->adts_hdr_buf[5] & 0x1F) | (aac_frame_size << 5));

    if (sink)
    {
        sink->write(sink, parser_aac->adts_hdr_buf, 7);
    }

    return parser_aac->adts_hdr_buf;
}

static void parser_aac_free_channel_element_arrays(uint8_t **is_cpe, uint8_t **tag_select)
{
    FREE_CHK(*is_cpe);
    FREE_CHK(*tag_select);
    *is_cpe     = NULL;
    *tag_select = NULL;
}

static void
parser_aac_destroy(parser_handle_t parser)
{
    parser_aac_handle_t   parser_aac = (parser_aac_handle_t)parser;
    mp4_dsi_aac_handle_t  dsi        = (mp4_dsi_aac_handle_t)parser->curr_dsi;
    mp4_dsi_avc_handle_t* entry = NULL;
    it_list_handle_t      it = it_create();

    if (parser_aac->adts_hdr_buf)
    {
        FREE_CHK(parser_aac->adts_hdr_buf);
    }

    it_init(it, parser->dsi_lst);
    while ((entry = it_get_entry(it)))
    {
        dsi = (mp4_dsi_aac_handle_t)*entry;

        /* free all the dsi arrays we may have created */
        parser_aac_free_channel_element_arrays(&dsi->front_element_is_cpe, &dsi->front_element_tag_select);
        parser_aac_free_channel_element_arrays(&dsi->side_element_is_cpe,  &dsi->side_element_tag_select);
        parser_aac_free_channel_element_arrays(&dsi->back_element_is_cpe,  &dsi->back_element_tag_select);

        if (dsi->num_lfe_channel_elements > 0)
        {
            FREE_CHK(dsi->lfe_element_tag_select);
        }
        if (dsi->num_assoc_data_elements > 0)
        {
            FREE_CHK(dsi->assoc_data_element_tag_select);
        }

        parser_aac_free_channel_element_arrays(&dsi->cc_element_is_ind_sw, &dsi->valid_cc_element_tag_select);
        if (dsi->comment_field_bytes > 0)
        {
            FREE_CHK(dsi->comment_field_data);
        }
    }
    it_destroy(it);

    parser_destroy(parser);
}

static uint32_t
parser_aac_get_channel_count(parser_aac_handle_t parser_aac)
{
    return (uint32_t) (parser_aac->channel_configuration == 7) ? 8 : parser_aac->channel_configuration;
}


static uint32_t
parser_aac_get_param(parser_handle_t parser, stream_param_id_t param_id)
{
    parser_aac_handle_t parser_aac = (parser_aac_handle_t)parser;

    uint32_t t = 0;

    if (param_id == STREAM_PARAM_ID_CHANNELCOUNT)
    {
        t = parser_aac_get_channel_count(parser_aac);
    }

    return t;
}

static parser_handle_t
parser_aac_create(uint32_t dsi_type)
{
    parser_aac_handle_t parser;

    assert(dsi_type == DSI_TYPE_MP4FF);
    parser = (parser_aac_handle_t)MALLOC_CHK(sizeof(parser_aac_t));
    if (!parser)
    {
        return 0;
    }
    memset(parser, 0, sizeof(parser_aac_t));

    /**** build the interface, base for the instance */
    parser->stream_type = STREAM_TYPE_AUDIO;
    parser->stream_id   = STREAM_ID_AAC;
    parser->stream_name = "aac";
    parser->dsi_FourCC  = "esds";

    parser->dsi_type   = dsi_type;
    parser->dsi_create = dsi_aac_create;

    parser->init       = parser_aac_init;
    parser->destroy    = parser_aac_destroy;
    parser->get_sample = parser_aac_get_sample;
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->get_cfg   = parser_aac_get_mp4_cfg;
        parser->write_cfg = parser_aac_write_mp4_cfg;
    }

    parser->get_param          = parser_aac_get_param;
    parser->parse_codec_config = parser_aac_codec_config;

    /* use dsi list for the sake of multiple entries of stsd */
    if (dsi_list_create((parser_handle_t)parser, dsi_type))
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }
    parser->codec_config_lst  = list_create(sizeof(codec_config_t));
    parser->curr_codec_config = NULL;
    if (!parser->codec_config_lst)
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }

    /**** aac specifics */

    /**** cast to base */
    return (parser_handle_t)parser;
}

void
parser_aac_reg(void)
{
    reg_parser_set("aac", parser_aac_create);
}

/*
 * User Interface for out-of-band configuration
 */

/**
 * @brief Adjusts values in curr_dsi
 *
 * Fiddles with the DSI so that the signaling is correct
 */
void
parser_aac_set_signaling_mode(parser_handle_t parser, uint32_t signaling_mode)
{
    mp4_dsi_aac_handle_t dsi = (mp4_dsi_aac_handle_t)parser->curr_dsi;

    /* Set up the dsi for correct signaling */
    switch (signaling_mode)
    {
    case PARSER_AAC_SIGNALING_MODE_SBR_NBC:
        dsi->audioObjectType     = (dsi->has_sbr ? AOT_SBR : AOT_AAC_LC);
        dsi->audioObjectType2    = AOT_AAC_LC;
        dsi->audioObjectTypeExt2 = 0;
        break;
    case PARSER_AAC_SIGNALING_MODE_PS_NBC:
        if (dsi->has_ps)
        {
            dsi->audioObjectType = AOT_PS;
        }
        else if (dsi->has_sbr)
        {
            dsi->audioObjectType = AOT_SBR;
        }
        else
        {
            dsi->audioObjectType = AOT_AAC_LC;
        }
        dsi->audioObjectType2    = AOT_AAC_LC;
        dsi->audioObjectTypeExt2 = 0;
        break;
    case PARSER_AAC_SIGNALING_MODE_SBR_BC:
    case PARSER_AAC_SIGNALING_MODE_PS_BC:
        dsi->audioObjectType             = AOT_AAC_LC;
        dsi->audioObjectTypeExt          = 0;
        dsi->extensionAudioObjectType    = AOT_SBR;
        dsi->extensionAudioObjectTypeExt = 0;
        dsi->have_sbr_ext                = TRUE;
        dsi->have_ps_ext                 = signaling_mode == PARSER_AAC_SIGNALING_MODE_PS_BC ? TRUE : FALSE;
    break;
    default:
        dsi->audioObjectType    = AOT_AAC_LC;
        dsi->have_sbr_ext       = FALSE;
        dsi->have_ps_ext        = FALSE;
        dsi->audioObjectTypeExt = 0;
    }
}

/**
 * @brief store codec_config (i.e. ASC) in codec_config_lst and setup curr_dsi
 */
void
parser_aac_set_asc(parser_handle_t parser, unsigned char *asc, uint32_t size)
{
    /* Create new entry for codec config list */
    parser->curr_codec_config = (codec_config_t*)list_alloc_entry(parser->codec_config_lst);

    parser->curr_codec_config->codec_config_data = MALLOC_CHK(size);
    memcpy(parser->curr_codec_config->codec_config_data, asc, size);
    parser->curr_codec_config->codec_config_size = size;
    list_add_entry(parser->codec_config_lst, parser->curr_codec_config);

    /* parse codec config */
    parser_aac_codec_config(parser, NULL);
}

/**
 * @brief Adjusts values in curr_dsi
 *
 * will override the information in the DSI - creates explicit compatible signaling DSI
 */
void
parser_aac_set_config(parser_handle_t parser, uint32_t frequency, BOOL has_sbr, BOOL has_ps, BOOL is_oversampled_sbr)
{
    parser_aac_handle_t  parser_aac = (parser_aac_handle_t)parser;
    mp4_dsi_aac_handle_t dsi        = (mp4_dsi_aac_handle_t)parser->curr_dsi;
    int                  idx;

    if (is_oversampled_sbr || has_sbr)
    {
        frequency /= 2;
    }

    for (idx = 0; idx < 15; idx++)
    {
        if (sfi_2_freq_tbl[idx] == frequency)
        {
            break;
        }
    }
    /* 15 indicates that the frequency is a 24-bit value */

    dsi->extensionAudioObjectTypeExt = 0;

    dsi->has_sbr      = has_sbr;
    dsi->have_sbr_ext = FALSE;
    if (has_sbr)
    {
        dsi->sbr_sampling_frequency_index = (uint8_t)idx - 3;
        dsi->pce_sampling_frequency_index = (uint8_t)idx - 3;

        /* SBR sampling frequency is the actual timescale if SBR is used. So set it here. */
        parser_aac->time_scale = sfi_2_freq_tbl[idx];

        dsi->have_sbr_ext                = TRUE;
        dsi->extensionAudioObjectType    = AOT_SBR;
        dsi->extensionAudioObjectTypeExt = 0;
    }

    dsi->has_ps      = has_ps;
    dsi->have_ps_ext = FALSE;
    if (has_ps)
    {
        dsi->have_ps_ext              = TRUE;
        dsi->extensionAudioObjectType = AOT_PS;
    }
}

/*
 * Prior to this being called the audioObjectType has to be setup.
 * parser_aac_get_mp4_cfg() needs to be called which uses the signaling mode to set dsi->audioObjectType
 * Call parser_aac_set_signaling_mode() to set the signaling mode or it can be read during mp4 file demuxing when
 * parser_aac_codec_config() is called
 */
uint8_t
parser_aac_get_profile_level_id(parser_handle_t parser)
{
    parser_aac_handle_t  parser_aac = (parser_aac_handle_t)parser;
    mp4_dsi_aac_handle_t dsi        = (mp4_dsi_aac_handle_t)parser->curr_dsi;

    BOOL has_sbr     = dsi->has_sbr;
    BOOL has_ps      = dsi->has_ps;
    int sample_rate  = parser_aac->sample_rate;
    int num_channels = dsi->channelConfiguration;
    int apl          = 0xff;                       /* no capability required */

    if (has_sbr)
    {
        if (!has_ps)
        {
            if (num_channels <= 2)
            {
                apl = HEAAC_PROFILE_LEVEL_2;
            }
            else
            {
                apl = HEAAC_PROFILE_LEVEL_4;
            }
            if (sample_rate > 48000)
            {
                apl = HEAAC_PROFILE_LEVEL_5;
            }
        }
        else
        {
            apl = HEAACV2_PROFILE_LEVEL_2;
        }
    }
    else
    {
        if (num_channels <= 2)
        {
            apl = AAC_PROFILE_LEVEL_2;
        }
        else
        {
            apl = AAC_PROFILE_LEVEL_4;
        }
        if (sample_rate > 48000)
        {
            apl = AAC_PROFILE_LEVEL_5;
        }
    }

    return (uint8_t)apl;
}

static void parser_aac_check_ccff_conformance(parser_aac_handle_t parser_aac)
{
#define _REPORT(lvl,msg) parser_aac->reporter->report(parser_aac->reporter, lvl, msg)

    mp4_dsi_aac_handle_t dsi = (mp4_dsi_aac_handle_t)parser_aac->curr_dsi;

    if (!parser_aac->reporter) return;

    _REPORT(REPORT_LEVEL_INFO, "AAC: Validating audio object type. Expecting AOT=2.");
    if (dsi->audioObjectType != 2)
    {
        _REPORT(REPORT_LEVEL_WARN, "AAC: Wrong audio object type detected.");
    }

    _REPORT(REPORT_LEVEL_INFO, "AAC: Validating sample rate. Expecting 48000.");
    if (parser_aac->sample_rate != 48000)
    {
        _REPORT(REPORT_LEVEL_WARN, "AAC: Wrong sample rate.");
    }
}
