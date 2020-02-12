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
 *  @file  parser_ac4.h
 *  @brief Defines AC-4 parser interface
 */

#ifndef __PARSER_AC4_H__
#define __PARSER_AC4_H__

#include "parser.h"  /** PARSER_AUDIO_BASE */

struct parser_ac4_t_
{
    PARSER_AUDIO_BASE;

    uint32_t sample_num;
    uint32_t samples_per_frame;

    uint32_t sample_buf_size;

    uint32_t sequence_counter;
    uint32_t b_iframe_global;

    /*ac4_dsi related varibles */
    uint32_t bitstream_version;
    uint32_t fs_index;
    uint32_t frame_rate_index;
    uint32_t n_presentations;

    /** currently we just set max presentation number as 32; actually this value should be set to 2^9 = 512 */
#define PRESENTATION_NUM 512
    uint8_t b_single_substream[PRESENTATION_NUM];
    uint8_t presentation_config[PRESENTATION_NUM];
    uint8_t presentation_version[PRESENTATION_NUM];
    uint8_t b_add_emdf_substreams[PRESENTATION_NUM];
    uint8_t mdcompat[PRESENTATION_NUM];
    uint8_t b_presentation_id[PRESENTATION_NUM];
    uint16_t presentation_id[PRESENTATION_NUM];
    uint8_t frame_rate_factor[PRESENTATION_NUM];
    uint8_t dsi_frame_rate_multiply_info[PRESENTATION_NUM];
    uint8_t emdf_version[PRESENTATION_NUM];
    uint8_t key_id[PRESENTATION_NUM];
    uint8_t b_hsf_ext[PRESENTATION_NUM];
    uint8_t n_skip_bytes[PRESENTATION_NUM];
    uint8_t* skip_bytes_address[PRESENTATION_NUM];
    uint8_t b_pre_virtualized[PRESENTATION_NUM];
    uint8_t n_add_emdf_substreams[PRESENTATION_NUM];

    /** emdf added substream  */
    /** currently we just set max emdf substreams number as 16; actually this value should be set to 2^7 = 128 */
#define EMDF_SUBSTREAM_NUM 32
    uint8_t add_emdf_version[PRESENTATION_NUM][EMDF_SUBSTREAM_NUM];
    uint8_t add_key_id[PRESENTATION_NUM][EMDF_SUBSTREAM_NUM];

    /** ac4_substeam_dsi related varibles */
    /** for each presentation, the max substream num is: 3 (case 3,4)*/
#define SUBSTREAM_NUM 3
    uint8_t ch_mode[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t dsi_sf_multiplier[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t b_bitrate_info[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t bitrate_indicator[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t add_ch_base[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t b_content_type[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t content_classifier[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t b_language_indicator[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t n_language_tag_bytes[PRESENTATION_NUM][SUBSTREAM_NUM];
    uint8_t language_tag_bytes[PRESENTATION_NUM][SUBSTREAM_NUM][64];

    /** v2 specific syntax  */
    uint8_t b_program_id;
    uint16_t short_program_id;
    uint8_t b_program_uuid_present;
    uint16_t program_uuid[8];

    uint8_t b_single_substream_group[PRESENTATION_NUM];
    uint8_t dsi_frame_rate_fractions_info[PRESENTATION_NUM];

    uint8_t b_presentation_filter[PRESENTATION_NUM];
    uint8_t b_enable_presentation[PRESENTATION_NUM];

    uint8_t n_substream_groups[PRESENTATION_NUM];
    uint8_t b_multi_pid[PRESENTATION_NUM];
    uint8_t isAtmos[PRESENTATION_NUM];

    uint8_t total_n_substream_groups;
    uint8_t max_group_index;

#define SUBSTREAM_COUNT 128
#define SUBSTREAM_GROUP 128
    uint8_t group_index[PRESENTATION_NUM][SUBSTREAM_GROUP];

    uint8_t b_4_back_channels_present[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_centre_present[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t top_channels_present[SUBSTREAM_GROUP][SUBSTREAM_COUNT];

    uint8_t b_substreams_present[SUBSTREAM_GROUP];
    uint8_t b_hsf_ext_v2[SUBSTREAM_GROUP];
    uint8_t b_single_substream_v2[SUBSTREAM_GROUP];
    uint8_t n_lf_substreams_minus2[SUBSTREAM_GROUP];
    uint8_t b_channel_coded[SUBSTREAM_GROUP];

    uint8_t b_oamd_substream[SUBSTREAM_GROUP];
    uint8_t b_ajoc[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_content_type_v2[SUBSTREAM_GROUP];
    uint8_t content_classifier_v2[SUBSTREAM_GROUP];
    uint8_t b_language_indicator_v2[SUBSTREAM_GROUP];
    uint8_t n_language_tag_bytes_v2[SUBSTREAM_GROUP];
    uint8_t language_tag_bytes_v2[SUBSTREAM_GROUP][64];

    uint8_t sus_ver[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t sf_multiplier[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t group_substream_ch_mode[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_bitrate_info_v2[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t bitrate_indicator_v2[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t add_ch_base_v2[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t frame_rate_factor_v2[SUBSTREAM_GROUP][SUBSTREAM_COUNT];

    uint8_t pres_ch_mode[PRESENTATION_NUM];
    uint8_t pres_ch_mode_core[PRESENTATION_NUM];

/*ajoc related structs */
    uint8_t b_lfe[SUBSTREAM_GROUP][SUBSTREAM_COUNT];

    uint8_t b_isf[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_dynamic_objects[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_dyn_objects_only[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t b_bed_objects[SUBSTREAM_GROUP][SUBSTREAM_COUNT];

    uint8_t b_static_dmx[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t n_fullband_dmx_signals_minus1[SUBSTREAM_GROUP][SUBSTREAM_COUNT];
    uint8_t n_fullband_upmix_signals_minus1[SUBSTREAM_GROUP][SUBSTREAM_COUNT];

    uint32_t bit_rate_mode;
};
typedef struct parser_ac4_t_ parser_ac4_t;
typedef parser_ac4_t *parser_ac4_handle_t;

#endif  /* __PARSER_AC4_H__ */
