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
/**
 *  @file  parser_ac4.c
 *  @brief Implements an AC-4 parser
 * 
 *  Based on ETSI TS 103 190-2 V 1.1.1
 */

#include <assert.h>      /* assert()         */
#include <stdio.h>       /* SEEK_SET         */
#include <math.h>

#include "parser_ac4.h"
#include "memory_chk.h"  /* REALLOC_CHK()    */
#include "msg_log.h"     /* DPRINTF()        */
#include "registry.h"    /* reg_parser_set() */

/** Based on Table 79 and Table A.27 */
static const uint32_t chmode_2_channel_mask[16] = {
    0x00002, 0x00001, 0x00003, 0x00007, 0x00047, 0x0000f, 0x0004f, 0x20007,  /* 7.0:3/4/0 and 7.1:3/4/1  (Lrs, Rrs) == (Lb, Rb) */
    0x20047, 0x40007, 0x40047, 0x0003f, 0x0007f, 0x1003f, 0x1007f, 0x2ff7f   
};

static const uint8_t superset_channel_mode[16][16] = {
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {2,2,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {3,3,3,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {4,4,4,4,4,6,6,8,8,10,10,12,12,14,14,15},
    {5,5,5,5,6,5,6,7,8,9,10,11,12,13,14,15},
    {6,6,6,6,6,6,6,6,8,6,10,12,12,14,14,15},
    {7,7,7,7,8,7,6,7,8,9,10,12,12,13,14,15},
    {8,8,8,8,8,8,8,8,8,8,10,11,12,14,14,15},
    {9,9,9,9,10,9,10,9,9,9,10,11,12,13,14,15},
    {10,10,10,10,10,10,10,10,10,10,10,10,12,13,14,15},
    {11,11,11,11,12,11,12,11,12,11,12,11,13,13,14,15},
    {12,12,12,12,12,12,12,12,12,12,12,12,12,13,14,15},
    {13,13,13,13,14,13,14,13,14,13,14,13,14,13,14,15},
    {14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15},
    {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15}
};

static int32_t
superset(int32_t foo, int32_t bar)
{
    if ((foo == -1) || (foo > 15))
        return bar;

    if ((bar == -1) || (bar > 15))
        return foo;

    return superset_channel_mode[foo][bar];
}

static void
generate_presentation_ch_present(parser_ac4_handle_t parser_ac4, int32_t presentation_idx, int32_t *b_4_back_ch, int32_t *b_centre, int32_t *top_ch)
{
    int32_t sg, s;
    int32_t i;
    int32_t n_substreams; 

    for (sg = 0; sg < parser_ac4->total_n_substream_groups; sg++) { 
        for (i = 0; i < 3; i++)
        {
            if (sg == parser_ac4->group_index[presentation_idx][i])
                break;
            
        }
        
        n_substreams = (char)((char)(parser_ac4->n_lf_substreams_minus2[sg]) + 2);
        for (s = 0; s < n_substreams; s++) {
            if(*b_4_back_ch < parser_ac4->b_4_back_channels_present[sg][s]) {
                *b_4_back_ch = parser_ac4->b_4_back_channels_present[sg][s];
            }
            if(*b_centre < parser_ac4->b_centre_present[sg][s]) {
                *b_centre = parser_ac4->b_centre_present[sg][s];
            }
            if(*top_ch < parser_ac4->top_channels_present[sg][s]) {
                *top_ch = parser_ac4->top_channels_present[sg][s];
            }
        }
    }

}

static int32_t
generate_real_channel_mask(parser_ac4_handle_t parser_ac4, int32_t presentation_idx, int32_t sg_idx, int32_t substream_idx)
{
    int32_t b_4_back_channels = 0;
    int32_t b_centre = 0;
    int32_t top_channels = 0;
    int32_t real_mask = 0;
    int32_t need_change = 0;

    if (presentation_idx != -1)
    {
        generate_presentation_ch_present(parser_ac4, presentation_idx, &b_4_back_channels, &b_centre, &top_channels);

        if((parser_ac4->pres_ch_mode[presentation_idx] > 16) || (parser_ac4->pres_ch_mode[presentation_idx] < 0))
        {
            return -1;
        }

        real_mask = chmode_2_channel_mask[parser_ac4->pres_ch_mode[presentation_idx]];
        if (parser_ac4->pres_ch_mode[presentation_idx] == 11 || 
            parser_ac4->pres_ch_mode[presentation_idx] == 12 ||
            parser_ac4->pres_ch_mode[presentation_idx] == 13 ||
            parser_ac4->pres_ch_mode[presentation_idx] == 14 )
            {
                need_change = 1;
            }
    }
    else
    {
        b_4_back_channels = parser_ac4->b_4_back_channels_present[sg_idx][substream_idx];
        b_centre = parser_ac4->b_centre_present[sg_idx][substream_idx];
        top_channels = parser_ac4->top_channels_present[sg_idx][substream_idx];

        real_mask = chmode_2_channel_mask[parser_ac4->group_substream_ch_mode[sg_idx][substream_idx]];
        if (parser_ac4->group_substream_ch_mode[sg_idx][substream_idx] == 11 || 
            parser_ac4->group_substream_ch_mode[sg_idx][substream_idx] == 12 ||
            parser_ac4->group_substream_ch_mode[sg_idx][substream_idx] == 13 ||
            parser_ac4->group_substream_ch_mode[sg_idx][substream_idx] == 14 )
            {
                need_change = 1;
            }
    }

    if (need_change)
    {
        if (!b_centre)
        {
            real_mask = real_mask & 0xfffffffd;
        }
        if (!b_4_back_channels)
        {
            real_mask = real_mask & 0xfffffff7;
        }

        /* AC4 spe: Table 81: top_channels_present need to be update;
        Current logic follow: G.3.1 The AC-4 audio channel configuration scheme
        For a stream with an 3/2/2 (5.1.2) immersive audio channel configuration using loudspeakers L, R, C, Ls, Rs, TL, TR, LFE, the value is 0000C7*/
        if ((top_channels == 1) || (top_channels == 2))
        {
            real_mask = (real_mask & 0xffffff0f) | (0xc << 4) | (real_mask & 0xf);
        }
        else if (top_channels == 0)
        {
            real_mask = (real_mask & 0xffffff0f) | (0x4 << 4) | (real_mask & 0xf);
        }
    }

    return real_mask;
}


static int32_t
generate_presentation_ch_mode(parser_ac4_handle_t parser_ac4, int32_t presentation_idx)
{
    int32_t pres_ch_mode = -1;
    int32_t b_obj_or_ajoc = 0;
    int32_t is_ac4_substream = 1; 
    int32_t sg, s;
    int32_t i;
    int32_t n_substreams; 

    for (sg = 0; sg < parser_ac4->total_n_substream_groups; sg++) {
        for (i = 0; i < 3; i++)
        {
            if (sg == parser_ac4->group_index[presentation_idx][i])
                break;
            
        }
        if (i == 3)
            continue;
        
        n_substreams = (char)((char)(parser_ac4->n_lf_substreams_minus2[sg]) + 2);
        for (s = 0; s < n_substreams; s++) {
            if (is_ac4_substream) {
                if (parser_ac4->b_channel_coded[sg]) {
                    int32_t ch_mode = parser_ac4->group_substream_ch_mode[sg][s];
                    pres_ch_mode = superset(pres_ch_mode, ch_mode);
                } else {
                    b_obj_or_ajoc = 1;
                }
            }
        }
    }

    if (((pres_ch_mode == 5) || (pres_ch_mode == 6)) && (parser_ac4->presentation_version[presentation_idx] == 2))
    {
        pres_ch_mode = 1;
    }

    return (b_obj_or_ajoc ? -1 : pres_ch_mode);
}

static int32_t
generate_presentation_ch_mode_core(parser_ac4_handle_t parser_ac4, int32_t presentation_idx)
{
    int32_t pres_ch_mode = -1;
    int32_t b_obj_or_ajoc = 0;
    int32_t is_ac4_substream = 1;
    int32_t sg, s;
    int32_t i;
    int32_t n_substreams;
    int32_t ch_mode_core = -1;

    for (sg = 0; sg < parser_ac4->total_n_substream_groups; sg++) { 
        for (i = 0; i < 3; i++)
        {
            if (sg == parser_ac4->group_index[presentation_idx][i])
                break;
            
        }
        if (i == 3)
            continue;
        
        n_substreams = (char)((char)(parser_ac4->n_lf_substreams_minus2[sg]) + 2);
        for (s = 0; s < n_substreams; s++) {
            if (is_ac4_substream) {
                if (parser_ac4->b_channel_coded[sg]) {
                    if (parser_ac4->group_substream_ch_mode[sg][s] == 11 || parser_ac4->group_substream_ch_mode[sg][s] == 13)
                        ch_mode_core = 5;
                    else if (parser_ac4->group_substream_ch_mode[sg][s] == 12 || parser_ac4->group_substream_ch_mode[sg][s] == 14)
                        ch_mode_core = 6;
                    else 
                        ch_mode_core = -1;
                    pres_ch_mode = superset(pres_ch_mode, ch_mode_core);
                } else {
                    if((parser_ac4->b_ajoc[sg][s] == 1) && (parser_ac4->b_static_dmx[sg][s] == 1))
                    {
                        if (parser_ac4->b_lfe[sg][s])
                            ch_mode_core = 4;
                        else
                            ch_mode_core = 3;
                    } else {
                        ch_mode_core = -1;
                        b_obj_or_ajoc = 1;
                    }
                    pres_ch_mode = superset(pres_ch_mode, ch_mode_core);
                }
            }
        }
    }

    return (b_obj_or_ajoc ? -1 : pres_ch_mode);
}

static int32_t /** @return 0: no sync found, 1: sync found with CRC on, 2: sync found with CRC off */
parser_ac4_get_sync(parser_ac4_handle_t parser_ac4, bbio_handle_t bs)
{
    parser_ac4;
    while (!bs->is_EOD(bs))
    {
        uint32_t val;

        val = src_read_u8(bs);  /* 1st byte of sync word */
        if (val != 0xac)
        {
            continue;
        }

        val = src_read_u8(bs);  /* 2nd byte of sync word */
        if (val == 0x40)
        {
            return 2;
        }
        else if (val == 0x41)
        {
            return 1;
        }
        else
        {
            continue;
        }
    }

    /* NOTE: No second sync check is currently implemented as sanity check! */

    return 0;
}

static int32_t
variable_bits(uint32_t n_bits, bbio_handle_t bs)
{
    int32_t value = 0;
    int32_t b_read_more = 0;

    do {
        value += (uint8_t)src_read_bits(bs, n_bits);
        b_read_more = (uint8_t)src_read_bits(bs, 1);
        if (b_read_more) {
            value <<= n_bits;
            value += (1<<n_bits);
        }
    } while(b_read_more);

    return value;
}

static uint32_t
presentation_version(bbio_handle_t bs)
{
    uint32_t val = 0;
    while (src_read_bits(bs, 1) == 1) {
        val++;
    }
    return val;
}

static uint32_t /* return value is dsi_frame_rate_multiply_info as specified page 191 Table E8.6 */
frame_rate_multiply_info(parser_ac4_handle_t parser_ac4, bbio_handle_t bs, int32_t idx)
{
    uint32_t value = 0;

    switch (parser_ac4->frame_rate_index) {
    case 2:
    case 3:
    case 4:
        if (src_read_bits(bs, 1)) { /* b_multiplier */ 
            if (src_read_bits(bs, 1)) {/* multiplier_bit */
                parser_ac4->frame_rate_factor[idx] = 4;
                value = 2;
            }
            else
            {
                parser_ac4->frame_rate_factor[idx] = 2;
                value = 1;
            }
        }
        else
        {
            parser_ac4->frame_rate_factor[idx] = 1;
        }
        break;
    case 0:
    case 1:
    case 7:
    case 8:
    case 9:
        if (src_read_bits(bs, 1)) { /* b_multiplier */
            parser_ac4->frame_rate_factor[idx] = 2;
            value = 1;
        }
        else
        {
            parser_ac4->frame_rate_factor[idx] = 1;
        }
        break;
    default:
        parser_ac4->frame_rate_factor[idx] = 1;
        break;
    }

    return value;
}

static uint32_t /* return value is dsi_frame_rate_fractions_info as specified Table E.10.7 */
frame_rate_fractions_info(parser_ac4_handle_t parser_ac4, bbio_handle_t bs, int32_t idx) /* 4.3.3.5.3 Table 86: frame_rate_factor*/
{
    uint32_t value = 0;
    uint32_t frame_rate_fraction = 1;

    switch (parser_ac4->frame_rate_index) {
    case 10:
    case 11:
    case 12:
        if (src_read_bits(bs, 1)) { /* b_frame_rate_fraction */ 
            if (src_read_bits(bs, 1)) {/* b_frame_rate_fraction_is_4 */
                value = 2;
                frame_rate_fraction = 4;
            }
            else
            {
                frame_rate_fraction = 2;
                value = 1;
            }
        }
        else
        {
            value = 0;
        }
        break;

    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        if (parser_ac4->frame_rate_factor[idx]) {
            if (src_read_bits(bs, 1)) { /* b_frame_rate_fraction */
                value = 1;
                frame_rate_fraction = 2;
            }
        }
        break;

    default:
        break;
    }

    return value;
}

static void
emdf_payloads_substream_info(bbio_handle_t bs)
{
    uint32_t tmp;
    tmp = (uint8_t)src_read_bits(bs, 2);
    if (tmp == 3) { /* substream_index */
         tmp+= (uint8_t)variable_bits(2, bs);
    }
}
static void
emdf_protection(bbio_handle_t bs)
{
    uint32_t protection_length_primary;
    uint32_t protection_length_secondary;
    
    protection_length_primary = (uint8_t)src_read_bits(bs, 2);
    protection_length_secondary= (uint8_t)src_read_bits(bs, 2);

    switch (protection_length_primary) {
    case 1:
        src_read_bits(bs, 8);
        break;
    case 2:
        src_read_bits(bs, 32);
        break;
    case 3:
        src_read_bits(bs, 128);
        break;
    }

    switch (protection_length_secondary) {
    case 0:
        break;
    case 1:
        src_read_bits(bs, 8);
        break;
    case 2:
        src_read_bits(bs, 32);
        break;
    case 3:
        src_read_bits(bs, 128);
        break;
    }
}

static void
emdf_info(parser_ac4_handle_t parser_ac4, int32_t present_idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;

    parser_ac4->emdf_version[present_idx] = (uint8_t)src_read_bits(ds, 2);
    if (parser_ac4->emdf_version[present_idx] == 3) {
        parser_ac4->emdf_version[present_idx] += (uint8_t)variable_bits(2, ds);
    }
    parser_ac4->key_id[present_idx] = (uint8_t)src_read_bits(ds, 3);
    if (parser_ac4->key_id[present_idx] == 7) {
        parser_ac4->key_id[present_idx] += (uint8_t)variable_bits(3, ds);
    }
    if (src_read_bits(ds, 1)) { /* b_emdf_payloads_substream_info */
        emdf_payloads_substream_info(ds);
    }
    emdf_protection(ds);
}

static void
add_emdf_info(parser_ac4_handle_t parser_ac4, int32_t present_idx, int32_t idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;

    parser_ac4->add_emdf_version[present_idx][idx] = (uint8_t)src_read_bits(ds, 2);
    if (parser_ac4->add_emdf_version[present_idx][idx] == 3) {
        parser_ac4->add_emdf_version[present_idx][idx] += (uint8_t)variable_bits(2, ds);
    }
    parser_ac4->add_key_id[present_idx][idx] = (uint8_t)src_read_bits(ds, 3);
    if (parser_ac4->add_key_id[present_idx][idx] == 7) {
        parser_ac4->add_key_id[present_idx][idx] += (uint8_t)variable_bits(3, ds);
    }
    if (src_read_bits(ds, 1)) { /* b_emdf_payloads_substream_info */
        emdf_payloads_substream_info(ds);
    }
    emdf_protection(ds);
}

static void
content_type(parser_ac4_handle_t parser_ac4, int32_t present_idx, int32_t substream_idx )
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t i;

    if (present_idx != -1)
    {
        parser_ac4->content_classifier[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 3);
        parser_ac4->b_language_indicator[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_language_indicator[present_idx][substream_idx]) {
            if ( src_read_bits(ds, 1)) { /* b_serialized_language_tag */
                src_read_bits(ds, 1); /* b_start_tag */
                src_read_bits(ds, 16); /* language_tag_chunk */
            } else {
                parser_ac4->n_language_tag_bytes[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 6);
                for (i = 0; i < parser_ac4->n_language_tag_bytes[present_idx][substream_idx]; i++) {
                    parser_ac4->language_tag_bytes[present_idx][substream_idx][i] = (uint8_t)src_read_bits(ds, 8);
                }
            }
        }
    }
    else
    {
        parser_ac4->content_classifier_v2[substream_idx] = (uint8_t)src_read_bits(ds, 3);
        parser_ac4->b_language_indicator_v2[substream_idx] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_language_indicator_v2[substream_idx]) {
            if ( src_read_bits(ds, 1)) { /* b_serialized_language_tag */
                src_read_bits(ds, 1); /* b_start_tag */
                src_read_bits(ds, 16); /* language_tag_chunk */
            } else {
                parser_ac4->n_language_tag_bytes_v2[substream_idx] = (uint8_t)src_read_bits(ds, 6);
                for (i = 0; i < parser_ac4->n_language_tag_bytes_v2[substream_idx]; i++) {
                    parser_ac4->language_tag_bytes_v2[substream_idx][i] = (uint8_t)src_read_bits(ds, 8);
                }
            }
        }
    }
}

/* attention: ch_mode is not the value of channel_mode */
static uint32_t /* get ch_mode as Spec: 6.3.2.7.2 Table 79  */
get_ch_mode(bbio_handle_t bs)
{
    uint32_t value, tmp;

    value = (uint8_t)src_read_bits(bs, 1);

    if (value == 0)
        return 0;
    else
    {
        tmp = (uint8_t)src_read_bits(bs, 1);
        if (tmp == 0)
        {
            return 1;
        }
        else
        {
            tmp = (uint8_t)src_read_bits(bs, 2);
            if (tmp != 3)
            {
                return (tmp + 2);
            }
            else
            {
                tmp = (uint8_t)src_read_bits(bs, 3);
                if (tmp < 6)
                {
                    return (tmp + 5);
                }
                else if (tmp == 6)
                {
                    tmp = (uint8_t)src_read_bits(bs, 1);
                    return (tmp + 11);
                }
                else if (tmp > 6)
                {
                    tmp = (uint8_t)src_read_bits(bs, 2);
                    if (tmp < 3) {
                        return (tmp + 13);
                    }
                    else
                        return 16; 
                }
            }
        }
    }
    return 0xffffffff;
}

static void
ac4_substream_info_chan(parser_ac4_handle_t parser_ac4, int32_t sg_idx, int32_t stream_idx, int32_t b_substreams_present)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t tmp, i;

    parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] = (uint8_t)get_ch_mode(ds);
    if (parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 16) {
         parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] += (uint8_t)variable_bits(2, ds);
    }
    if (parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 11 ||
        parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 12 ||
        parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 13 ||
        parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 14 ) 
    {
        parser_ac4->b_4_back_channels_present[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 1);
        parser_ac4->b_centre_present[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 1);
        parser_ac4->top_channels_present[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 2);
    }
    if (parser_ac4->fs_index == 1) {  /* 48 kHz or above */
        if (src_read_bits(ds, 1)) {   /* b_sf_multiplier */
            parser_ac4->sf_multiplier[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 1) + 1;     /* sf_multiplier */
        }
        else
        {
            parser_ac4->sf_multiplier[sg_idx][stream_idx] = 0;
        }
    }
    parser_ac4->b_bitrate_info_v2[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_bitrate_info_v2[sg_idx][stream_idx]) { /* b_bitrate_info */
        tmp = (uint8_t)src_read_bits(ds, 3);
        if ((tmp == 0) || (tmp == 2) || (tmp == 4) || (tmp == 6)) /* 3 bit */
        { 
            parser_ac4->bitrate_indicator_v2[sg_idx][stream_idx] = (uint8_t)(tmp/2);
        }
        else /* 5 bit */
        {  
            if (tmp == 1)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][stream_idx] = 4 + (uint8_t)src_read_bits(ds, 2);
            }
            else if (tmp == 2)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][stream_idx] = 8 + (uint8_t)src_read_bits(ds, 2);
            }
            else
            {
                src_read_bits(ds, 2);
                parser_ac4->bitrate_indicator_v2[sg_idx][stream_idx] = 12; /* actually it should be 12...19; means unlimited bitrate; 12 is good enough */
            }
        }
    }

    if (parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 7 || parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 8 ||
        parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 9|| parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 10) 
    {
        parser_ac4->add_ch_base_v2[sg_idx][stream_idx] = (uint8_t)src_read_bits(ds, 1);
    }

    {
        int32_t k = 0;
        int32_t l = 0;
        int32_t presentation_idx = -1;
        
        for (k = 0; k < 32; k++) {
            for (l = 0; l < 16; l++) {
                if(parser_ac4->group_index[k][l] == sg_idx) {
                    presentation_idx = k;
                    break;
                }
            }
            if (presentation_idx != -1)
                break;
        }
        for (i = 0; i < parser_ac4->frame_rate_factor[k]; i++) {
            tmp = src_read_bits(ds, 1);/* b_audio_ndot */
        }
    }

    if (b_substreams_present == 1)
    {
        tmp =src_read_bits(ds, 2);  /* substream_index */
        if (tmp == 3) {
            tmp += (uint8_t)variable_bits(2, ds);
        }
    }

    /** IMS case */
    for (i = 0; i < parser_ac4->n_presentations; i++)
    {
        if (parser_ac4->presentation_version[i] == 2)
        {
            tmp = parser_ac4->group_index[i][0];
            if (tmp == sg_idx)
            {
                if (parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 6)
                {
                    parser_ac4->isAtmos[i] = 1;
                }

                if (((parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 5) 
                    || (parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] == 6)) )
                    {
                        parser_ac4->group_substream_ch_mode[sg_idx][stream_idx] = 1;
                    }
            }
        }
    }

}

static void
oamd_substream_info(bbio_handle_t bs, int32_t b_substreams_present)
{
    uint32_t tmp;
    src_read_bits(bs, 1);
    if (b_substreams_present == 1) {
        tmp = (uint8_t)src_read_bits(bs, 2);
        if (tmp == 3) { /* substream_index */
             tmp+= (uint8_t)variable_bits(2, bs);
        }
    }
}

/** Based on Table 83 */
static const uint8_t isf_config_objects_num[6] = {4, 8, 10, 14, 15, 30};
/** Based on Table 84 */
static const uint8_t bed_chan_assign_code_ch_num[8] = {2, 3, 6, 8, 10, 8, 10, 12};

#define CEILING_POS(X) ((X-(int32_t)(X)) > 0 ? (int32_t)(X+1) : (int32_t)(X))
static void
bed_dyn_obj_assignment(parser_ac4_handle_t parser_ac4, bbio_handle_t ds, uint32_t n_signals, int32_t sg_idx, int32_t substream_idx, int32_t flag) {

    uint32_t tmp = 0, i;
    uint32_t n_bed_signals = 0;
    uint8_t isf_config = 0;
    uint8_t b_ch_assign_code = 0;
    uint8_t b_chan_assign_mask = 0;
    uint8_t b_nonstd_bed_channel_assignment = 0;
    uint8_t bed_chan_assign_code = 0;
    uint32_t nonstd_bed_channel_assignment_mask = 0;
    uint32_t std_bed_channel_assignment_mask = 0;


    parser_ac4->b_dyn_objects_only[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1); /*  b_dyn_objects_only */
    if (parser_ac4->b_dyn_objects_only[sg_idx][substream_idx] == 0) { 
        parser_ac4->b_isf[sg_idx][substream_idx] =  (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_isf[sg_idx][substream_idx]) { /* b_isf */
            isf_config = (uint8_t)src_read_bits(ds, 3); /*  isf_config */
        }
        else {
            b_ch_assign_code = (uint8_t)src_read_bits(ds, 1); /* b_ch_assign_code */
            if (b_ch_assign_code) {
                bed_chan_assign_code = (uint8_t)src_read_bits(ds, 3); /* bed_chan_assign_code */
            }
            else {
                b_chan_assign_mask = (uint8_t)src_read_bits(ds, 1); /* b_chan_assign_mask */
                if (b_chan_assign_mask) {
                    b_nonstd_bed_channel_assignment = (uint8_t)src_read_bits(ds, 1); /* b_nonstd_bed_channel_assignment */
                    if (b_nonstd_bed_channel_assignment) {
                        nonstd_bed_channel_assignment_mask = src_read_bits(ds, 17);
                    }
                    else {
                        std_bed_channel_assignment_mask = src_read_bits(ds, 10);
                    }
                }
                else {
                    if (n_signals > 1) {
                        uint32_t bed_ch_bits = 0;
                        bed_ch_bits = (uint32_t)CEILING_POS(log((double)n_signals)/log((double)2));
                        n_bed_signals = (uint8_t)src_read_bits(ds, bed_ch_bits) + 1;
                    }
                    else {
                        n_bed_signals = 1;
                    }

                    for(i = 0; i < n_bed_signals; i++)
                        src_read_bits(ds, 4);
                }
            }
        }
    }

    if (flag && !parser_ac4->b_dyn_objects_only[sg_idx][substream_idx]) {
        if (parser_ac4->b_isf[sg_idx][substream_idx]) { /* b_isf */
            if (n_signals > isf_config_objects_num[isf_config])
                parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = 1;
        } else {
            if (b_ch_assign_code) {
                if (n_signals > bed_chan_assign_code_ch_num[bed_chan_assign_code])
                    parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = 1;
            }
            else {
                /* b_chan_assign_mask = (uint8_t)src_read_bits(ds, 1); b_chan_assign_mask */
                if (b_chan_assign_mask) {
                    /* b_nonstd_bed_channel_assignment = (uint8_t)src_read_bits(ds, 1); b_nonstd_bed_channel_assignment */
                    if (b_nonstd_bed_channel_assignment) {
                        /* nonstd_bed_channel_assignment_mask = src_read_bits(ds, 17); */
                        tmp = 0;
                        for (i = 0; i < 17; i++) {
                            if ((nonstd_bed_channel_assignment_mask >> i) & 1)
                                tmp++;
                        }
                        if (n_signals > tmp) 
                            parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = 1;
                    }
                    else {
                        /* std_bed_channel_assignment_mask = src_read_bits(ds, 10); */
                        tmp = 0;
                        for (i = 0; i < 10; i++) {
                            if ((std_bed_channel_assignment_mask >> i) & 1) {
                                if ((i == 1) || (i == 2) || (i == 9))
                                    tmp ++;
                                else
                                    tmp += 2;
                            }
                        }
                        if (n_signals > tmp)
                            parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = 1;
                    }
                }
                else {
                  if (n_signals > n_bed_signals)
                      parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = 1;
                }
            }
        }

    }

    if (flag) {
        if ((n_bed_signals > 0) || (tmp > 0))
            parser_ac4->b_bed_objects[sg_idx][substream_idx] = 1;
    }

}

static void
oamd_common_data(bbio_handle_t ds) {
    uint32_t tmp;

    if (src_read_bits(ds, 1) == 0) { /* b_default_screen_size_ratio */
        src_read_bits(ds, 5); /* master_screen_size_ratio_code */
    }
    src_read_bits(ds, 1); /* b_bed_object_chan_distribute */

    if (src_read_bits(ds, 1)) { /* b_additional_data */
        tmp = (uint8_t)src_read_bits(ds, 1) + 1;

        if (tmp == 2) {
            tmp += (uint8_t)variable_bits(2, ds);
        }

        src_read_bits(ds, tmp*8); /*  add_data */
    }

}

static void
ac4_substream_info_obj(parser_ac4_handle_t parser_ac4, int32_t sg_idx, int32_t substream_idx, int32_t b_substreams_present) 
{
    uint32_t tmp,i;
    bbio_handle_t ds = parser_ac4->ds;
    
    tmp = (uint8_t)src_read_bits(ds, 3); /* n_objects_code */

    parser_ac4->b_dynamic_objects[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_dynamic_objects[sg_idx][substream_idx]) { /* b_dynamic_objects */
        src_read_bits(ds, 1); /*  b_lfe */
    }
    else {
        parser_ac4->b_bed_objects[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
            if (parser_ac4->b_bed_objects[sg_idx][substream_idx]) { /* b_bed_objects */
                tmp = (uint8_t)src_read_bits(ds, 1); /* b_bed_start */
                if (tmp) {
                    tmp = (uint8_t)src_read_bits(ds, 1); /* b_ch_assign_code */
                    if (tmp) {
                        src_read_bits(ds, 3); /* bed_chan_assign_code */
                    }
                    else {
                        if (src_read_bits(ds, 1)) { /* b_nonstd_bed_channel_assignment */
                            src_read_bits(ds, 17); /* nonstd_bed_channel_assignment_mask */
                        }
                        else {
                            src_read_bits(ds, 10); /* std_bed_channel_assignment_mask */
                        }
                    }
                }
            }
            else {
                parser_ac4->b_isf[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
                if (parser_ac4->b_isf[sg_idx][substream_idx]) { /* b_isf */
                    tmp = (uint8_t)src_read_bits(ds, 1); /* b_isf_start */
                    if (tmp) {
                        src_read_bits(ds, 3); /* isf_config */
                    }
                } 
                else {
                    tmp = (uint8_t)src_read_bits(ds, 4); /* res_bytes */
                    src_read_bits(ds, 8*tmp);
                }
            }
        }

    if (parser_ac4->fs_index == 1) {  /* 48 kHz */
        if (src_read_bits(ds, 1)) {   /* b_sf_multiplier */
            parser_ac4->sf_multiplier[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1) + 1;     /* sf_multiplier */
        }
        else
        {
            parser_ac4->sf_multiplier[sg_idx][substream_idx] = 0;
        }
    }
    if (src_read_bits(ds, 1)) { /* b_bitrate_info */
        tmp = (uint8_t)src_read_bits(ds, 3);
        if ((tmp == 0) || (tmp == 2) || (tmp == 4) || (tmp == 6)) /* 3 bit */
        { 
            parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = (uint8_t)(tmp/2);
        }
        else /* 5 bit */
        {  
            if (tmp == 1)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 4 + (uint8_t)src_read_bits(ds, 2);
            }
            else if (tmp == 2)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 8 + (uint8_t)src_read_bits(ds, 2);
            }
            else
            {
                src_read_bits(ds, 2);
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 12; /* actually it should be 12...19; means unlimited bitrate; 12 is good enough */
            }
        }
    }

    {
        int32_t k = 0;
        int32_t l = 0;
        int32_t presentation_idx = -1;
        
        for (k = 0; k < 32; k++) {
            for (l = 0; l < 16; l++) {
                if(parser_ac4->group_index[k][l] == sg_idx) {
                    presentation_idx = k;
                    break;
                }
            }
            if (presentation_idx != -1)
                break;
        }
        for (i = 0; i < parser_ac4->frame_rate_factor[k]; i++) {
            src_read_bits(ds, 1);/* b_audio_ndot */
        }
    }


    if (b_substreams_present == 1) {
        tmp = (uint8_t)src_read_bits(ds, 2); /* substream_index */
        if (tmp == 3) { /* substream_index */
             tmp+= (uint8_t)variable_bits(2, ds);
        }
    }
}

static void
ac4_substream_info_ajoc(parser_ac4_handle_t parser_ac4, int32_t sg_idx, int32_t substream_idx, int32_t b_substreams_present)
{
    uint32_t tmp,i;
    bbio_handle_t ds = parser_ac4->ds;
    uint32_t n_fullband_dmx_signals,n_fullband_upmix_signals;

    parser_ac4->b_lfe[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    parser_ac4->b_static_dmx[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_static_dmx[sg_idx][substream_idx]) {
        n_fullband_dmx_signals = 5;
    }
    else {
        n_fullband_dmx_signals = (uint8_t)src_read_bits(ds, 4) + 1;
        parser_ac4->n_fullband_dmx_signals_minus1[sg_idx][substream_idx] = (uint8_t)(n_fullband_dmx_signals - 1);
        bed_dyn_obj_assignment(parser_ac4, parser_ac4->ds, n_fullband_dmx_signals, sg_idx, substream_idx, 0);
    }

    tmp = (uint8_t)src_read_bits(ds, 1);
    if (tmp) {
        oamd_common_data(parser_ac4->ds);
    }
    n_fullband_upmix_signals = (uint8_t)src_read_bits(ds, 4) + 1;
    parser_ac4->n_fullband_upmix_signals_minus1[sg_idx][substream_idx] = (uint8_t)(n_fullband_upmix_signals - 1);
    if (n_fullband_upmix_signals == 16) {
        n_fullband_upmix_signals += (uint8_t)variable_bits(3,ds);
    }

    bed_dyn_obj_assignment(parser_ac4, parser_ac4->ds, n_fullband_upmix_signals, sg_idx, substream_idx, 1);

        if (parser_ac4->fs_index == 1) {  /* 48 kHz */
        if (src_read_bits(ds, 1)) {   /* b_sf_multiplier */
            parser_ac4->sf_multiplier[sg_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1) + 1;     /* sf_multiplier */
        }
        else
        {
            parser_ac4->sf_multiplier[sg_idx][substream_idx] = 0;
        }
    }
    if (src_read_bits(ds, 1)) { /* b_bitrate_info */
        tmp = (uint8_t)src_read_bits(ds, 3);
        if ((tmp == 0) || (tmp == 2) || (tmp == 4) || (tmp == 6)) /* 3 bit */
        { 
            parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = (uint8_t)(tmp/2);
        }
        else /* 5 bit */
        {  
            if (tmp == 1)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 4 + (uint8_t)src_read_bits(ds, 2);
            }
            else if (tmp == 2)
            {
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 8 + (uint8_t)src_read_bits(ds, 2);
            }
            else
            {
                src_read_bits(ds, 2);
                parser_ac4->bitrate_indicator_v2[sg_idx][substream_idx] = 12; /* actually it should be 12...19; means unlimited bitrate; 12 is good enough */
            }
        }
    }

    {
        int32_t k = 0;
        int32_t l = 0;
        int32_t presentation_idx = -1;
        
        for (k = 0; k < 32; k++) {
            for (l = 0; l < 16; l++) {
                if(parser_ac4->group_index[k][l] == sg_idx) {
                    presentation_idx = k;
                    break;
                }
            }
            if (presentation_idx != -1)
                break;
        }
        for (i = 0; i < parser_ac4->frame_rate_factor[k]; i++) {
            src_read_bits(ds, 1);/* b_audio_ndot */
        }
    }

    if (b_substreams_present == 1) {
        tmp = (uint8_t)src_read_bits(ds, 2);
        if (tmp == 3) { /* substream_index */
             tmp+= (uint8_t)variable_bits(2, ds);
        }
    }
}

static void
ac4_hsf_ext_substream_info_v2(bbio_handle_t bs, uint8_t b_substreams_present)
{
    uint32_t tmp;
    if (b_substreams_present == 1 ) {
        tmp = (uint8_t)src_read_bits(bs, 2);
        if (tmp == 3) { /* substream_index */
             tmp+= (uint8_t)variable_bits(2, bs);
        }
    }
}

static void
ac4_substream_group_info(parser_ac4_handle_t parser_ac4, int32_t substream_group_idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t i;
    uint32_t n_lf_substreams = 0;

    parser_ac4->b_substreams_present[substream_group_idx] =  (uint8_t)src_read_bits(ds, 1);
    parser_ac4->b_hsf_ext_v2[substream_group_idx] =  (uint8_t)src_read_bits(ds, 1);
    parser_ac4->b_single_substream_v2[substream_group_idx] =  (uint8_t)src_read_bits(ds, 1);

    if (parser_ac4->b_single_substream_v2[substream_group_idx]) {
        n_lf_substreams = 1;
        parser_ac4->n_lf_substreams_minus2[substream_group_idx] = 0xff;
    }
    else {
        parser_ac4->n_lf_substreams_minus2[substream_group_idx] =  (uint8_t)src_read_bits(ds, 2);
        n_lf_substreams = parser_ac4->n_lf_substreams_minus2[substream_group_idx] + 2;
        if (n_lf_substreams == 5) {
            n_lf_substreams += (uint8_t)variable_bits(2, ds);
            parser_ac4->n_lf_substreams_minus2[substream_group_idx] = (uint8_t)n_lf_substreams - 2;
        }
    }

    parser_ac4->b_channel_coded[substream_group_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_channel_coded[substream_group_idx]) {
        for (i = 0; i < n_lf_substreams; i++) {
            if (parser_ac4->bitstream_version == 1) {
                parser_ac4->sus_ver[substream_group_idx][i] = (uint8_t)src_read_bits(ds, 1);
            }
            else {
                parser_ac4->sus_ver[substream_group_idx][i] = 1;
            }
            ac4_substream_info_chan(parser_ac4, substream_group_idx, i, parser_ac4->b_substreams_present[substream_group_idx]);
            if (parser_ac4->b_hsf_ext_v2[substream_group_idx]) {
                ac4_hsf_ext_substream_info_v2(parser_ac4->ds, parser_ac4->b_substreams_present[substream_group_idx]);
            }
        }
    }
    else {
        parser_ac4->b_oamd_substream[substream_group_idx] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_oamd_substream[substream_group_idx]) {
            oamd_substream_info(parser_ac4->ds, parser_ac4->b_substreams_present[substream_group_idx]);
        }
        for (i = 0; i < n_lf_substreams; i++) {
            parser_ac4->b_ajoc[substream_group_idx][i] = (uint8_t)src_read_bits(ds, 1);
            if (parser_ac4->b_ajoc[substream_group_idx][i]) {
                ac4_substream_info_ajoc(parser_ac4, substream_group_idx, i, parser_ac4->b_substreams_present[substream_group_idx]);
                if (parser_ac4->b_hsf_ext_v2[substream_group_idx]) {
                    ac4_hsf_ext_substream_info_v2(parser_ac4->ds, parser_ac4->b_substreams_present[substream_group_idx]);
                }
            }
            else {
                ac4_substream_info_obj(parser_ac4, substream_group_idx, i, parser_ac4->b_substreams_present[substream_group_idx]);
                if (parser_ac4->b_hsf_ext_v2[substream_group_idx]) {
                    ac4_hsf_ext_substream_info_v2(parser_ac4->ds, parser_ac4->b_substreams_present[substream_group_idx]);
                }
            }
        }
    }
    parser_ac4->b_content_type_v2[substream_group_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_content_type_v2[substream_group_idx]) {
        content_type(parser_ac4, -1, substream_group_idx);
    }
}

static void
ac4_sgi_specifier(parser_ac4_handle_t parser_ac4,  int32_t presentation_idx, int32_t pres_conf, int32_t substream_group_idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    pres_conf;

    if ( parser_ac4->bitstream_version == 1) {
        ac4_substream_group_info(parser_ac4, substream_group_idx);
    }
    else {
        parser_ac4->group_index[presentation_idx][substream_group_idx] = (uint8_t)src_read_bits(ds, 3);
        if (parser_ac4->group_index[presentation_idx][substream_group_idx] == 7) {
            parser_ac4->group_index[presentation_idx][substream_group_idx] += (uint8_t)variable_bits(2, ds);
        }
    }
    if (parser_ac4->group_index[presentation_idx][substream_group_idx] > parser_ac4->max_group_index)
        parser_ac4->max_group_index = parser_ac4->group_index[presentation_idx][substream_group_idx];
}

static void
ac4_substream_info(parser_ac4_handle_t parser_ac4, int32_t present_idx, int32_t substream_idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t tmp, i;

    parser_ac4->ch_mode[present_idx][substream_idx] = (uint8_t)get_ch_mode(ds);
    if (parser_ac4->ch_mode[present_idx][substream_idx] >= 12) {
        parser_ac4->ch_mode[present_idx][substream_idx] += (uint8_t)variable_bits(2, ds);
    }
    if (parser_ac4->fs_index == 1) {  /* 48 kHz */
        if (src_read_bits(ds, 1)) {   /* b_sf_multiplier */
            src_read_bits(ds, 1);     /* sf_multiplier */
        }
    }
    parser_ac4->b_bitrate_info[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_bitrate_info[present_idx][substream_idx]) { /* b_bitrate_info */
        tmp = (uint8_t)src_read_bits(ds, 3);
        if ((tmp == 0) || (tmp == 2) || (tmp == 4) || (tmp == 6)) /* 3 bit */
        { 
            parser_ac4->bitrate_indicator[present_idx][substream_idx] = (uint8_t)(tmp/2);
        }
        else /* 5 bit */
        {  
            if (tmp == 1)
            {
                parser_ac4->bitrate_indicator[present_idx][substream_idx] = 4 + (uint8_t)src_read_bits(ds, 2);
            }
            else if (tmp == 2)
            {
                parser_ac4->bitrate_indicator[present_idx][substream_idx] = 8 + (uint8_t)src_read_bits(ds, 2);
            }
            else
            {
                src_read_bits(ds, 2);
                parser_ac4->bitrate_indicator[present_idx][substream_idx] = 12; /* actually it should be 12...19; means unlimited bitrate; 12 is good enough */
            }
        }
    }
    if (parser_ac4->ch_mode[present_idx][substream_idx] == 7 || parser_ac4->ch_mode[present_idx][substream_idx] == 8 ||
        parser_ac4->ch_mode[present_idx][substream_idx] == 9|| parser_ac4->ch_mode[present_idx][substream_idx] == 10) 
    {
        parser_ac4->add_ch_base[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    }
    parser_ac4->b_content_type[present_idx][substream_idx] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_content_type[present_idx][substream_idx]) {
        content_type(parser_ac4, present_idx, substream_idx);
    }
    for (i = 0; i < parser_ac4->frame_rate_factor[present_idx]; i++) {
        src_read_bits(ds, 1);/* b_iframe */
    }

    tmp =src_read_bits(ds, 2);  /* substream_index */
    if (tmp == 3) {
        tmp += (uint8_t)variable_bits(2, ds);
    }
}

static void
ac4_hsf_ext_substream_info(bbio_handle_t bs)
{
    uint32_t tmp;
    tmp = (uint8_t)src_read_bits(bs, 2);
    if (tmp == 3) { /* substream_index */
         tmp+= (uint8_t)variable_bits(2, bs);
    }
}

static void
presentation_config_ext_info(parser_ac4_handle_t parser_ac4, int32_t idx)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t i;

    parser_ac4->n_skip_bytes[idx] = (uint8_t)src_read_bits(ds, 5);
    if (src_read_bits(ds, 1)) { /* b_more_skip_bytes */
        parser_ac4->n_skip_bytes[idx] += (uint8_t)variable_bits(2, ds) << 5;
    }
    if (!parser_ac4->skip_bytes_address[idx]) {
        parser_ac4->skip_bytes_address[idx] = (uint8_t *)MALLOC_CHK( parser_ac4->n_skip_bytes[idx]);
    }
    for (i = 0; i < parser_ac4->n_skip_bytes[idx]; i++) {
        parser_ac4->skip_bytes_address[idx][i] = (uint8_t)src_read_bits(ds, 8); /* reserved */
    }
}


static void /* Parsing presentation info as: ETSI TS 103 190-2 V 1.1.1 part 6.2.1.2 */
ac4_presentation_info(parser_ac4_handle_t parser_ac4, int32_t index)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t i;

    parser_ac4->b_single_substream[index] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_single_substream[index] != 1) {
        parser_ac4->presentation_config[index] = (uint8_t)src_read_bits(ds, 3);
        if (parser_ac4->presentation_config[index] == 7) {
            parser_ac4->presentation_config[index] += (uint8_t)variable_bits(2, ds);
        }
    }
    parser_ac4->presentation_version[index] = (uint8_t)presentation_version(ds);
    if (parser_ac4->b_single_substream[index] != 1 && parser_ac4->presentation_config[index] == 6) {
        parser_ac4->b_add_emdf_substreams[index] = 1;
    } else {
        parser_ac4->mdcompat[index] = (uint8_t)src_read_bits(ds, 3);
        parser_ac4->b_presentation_id[index] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_presentation_id[index]) {
            parser_ac4->presentation_id[index] = (uint8_t)variable_bits(2, ds);
        }

        parser_ac4->dsi_frame_rate_multiply_info[index] = (uint8_t)frame_rate_multiply_info(parser_ac4, ds, index);
        emdf_info(parser_ac4, index);
        if (parser_ac4->b_single_substream[index] == 1) {
            ac4_substream_info(parser_ac4, index, 0);
        } else {
            parser_ac4->b_hsf_ext[index] = (uint8_t)src_read_bits(ds, 1);
            switch(parser_ac4->presentation_config[index]) {
            case 0:        
                ac4_substream_info(parser_ac4, index, 0);        
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);               
                }
                ac4_substream_info(parser_ac4, index, 1);        
                break;
            case 1:        
                ac4_substream_info(parser_ac4, index, 0);    
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);               
                }
                ac4_substream_info(parser_ac4, index, 1);    
                break;
            case 2:        
                ac4_substream_info(parser_ac4, index, 0);    
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);           
                }
                ac4_substream_info(parser_ac4, index, 1);    
                break;
            case 3:        
                ac4_substream_info(parser_ac4, index, 0);
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);            
                }
                ac4_substream_info(parser_ac4, index, 1);    
                ac4_substream_info(parser_ac4, index, 2);
                break;
            case 4:        
                ac4_substream_info(parser_ac4, index, 0);
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);          
                }
                ac4_substream_info(parser_ac4, index, 1);    
                ac4_substream_info(parser_ac4, index, 2);    
                break;
            case 5:        
                ac4_substream_info(parser_ac4, index, 0);    
                if (parser_ac4->b_hsf_ext[index] == 1) {
                    ac4_hsf_ext_substream_info(ds);           
                }
                break;
            default:
                presentation_config_ext_info(parser_ac4, index);
            }

        }
    
        parser_ac4->b_pre_virtualized[index] = (uint8_t)src_read_bits(ds, 1);
        parser_ac4->b_add_emdf_substreams[index] = (uint8_t)src_read_bits(ds, 1);
    }
    if (parser_ac4->b_add_emdf_substreams[index]) {
        parser_ac4->n_add_emdf_substreams[index] = (uint8_t)src_read_bits(ds, 2);
        if (parser_ac4->n_add_emdf_substreams[index] == 0) {
            parser_ac4->n_add_emdf_substreams[index] = (uint8_t)variable_bits(2, ds) + 4;
        }
        for (i = 0; i < parser_ac4->n_add_emdf_substreams[index]; i++) {
            add_emdf_info(parser_ac4, index, i);
        }
    }

}

static void
ac4_presentation_substream_info(bbio_handle_t ds)
{
    uint32_t tmp;

    src_read_bits(ds, 1); /* b_alternative */
    src_read_bits(ds, 1); /* b_pres_ndot */
    tmp = (uint8_t)src_read_bits(ds, 2); /* substream_index */

    if(tmp ==3) {
        tmp += (uint8_t)variable_bits(2, ds); /* substream_index */
    }
}

static int32_t
ac4_presentation_v1_info(parser_ac4_handle_t parser_ac4, int32_t index)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t i;

    parser_ac4->b_single_substream_group[index] = (uint8_t)src_read_bits(ds, 1);
    if (parser_ac4->b_single_substream_group[index] != 1) {
        parser_ac4->presentation_config[index] = (uint8_t)src_read_bits(ds, 3);
        if (parser_ac4->presentation_config[index] == 7) {
            parser_ac4->presentation_config[index] += (uint8_t)variable_bits(2, ds);
        }
    }

    if (parser_ac4->bitstream_version != 1) {
        parser_ac4->presentation_version[index] = (uint8_t)presentation_version(ds);
    }

    if (parser_ac4->b_single_substream_group[index] != 1 && parser_ac4->presentation_config[index] == 6) {
        parser_ac4->b_add_emdf_substreams[index] = 1;
    } else {
        if (parser_ac4->bitstream_version != 1) {
            parser_ac4->mdcompat[index] = (uint8_t)src_read_bits(ds, 3);
        }
        parser_ac4->b_presentation_id[index] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_presentation_id[index]) {
            parser_ac4->presentation_id[index] = (uint16_t)variable_bits(2, ds);
        }
        else
        {
            if ( !((parser_ac4->n_presentations == 1) && (parser_ac4->presentation_version[index] != 2)))
            {
                printf("Error: AC4 Multiple presentation or IMS stream MUST have presentation id!\n");
                return 1;
            }
        }

        parser_ac4->dsi_frame_rate_multiply_info[index] = (uint8_t)frame_rate_multiply_info(parser_ac4, ds, index);
        parser_ac4->dsi_frame_rate_fractions_info[index] = (uint8_t)frame_rate_fractions_info(parser_ac4, ds, index);

        emdf_info(parser_ac4, index);

        parser_ac4->b_presentation_filter[index] = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_presentation_filter[index]) {
            parser_ac4->b_enable_presentation[index] = (uint8_t)src_read_bits(ds, 1);
        }

        if (parser_ac4->b_single_substream_group[index] == 1) {
            ac4_sgi_specifier(parser_ac4, index, 0, 0); 
            parser_ac4->n_substream_groups[index] = 1;
        } else {
            parser_ac4->b_multi_pid[index] = (uint8_t)src_read_bits(ds, 1);
            switch(parser_ac4->presentation_config[index]) {
            case 0:        
                ac4_sgi_specifier(parser_ac4, index, 1, 0);
                ac4_sgi_specifier(parser_ac4, index, 1, 1);
                parser_ac4->n_substream_groups[index] = 2;
                break;
            case 1:        
                ac4_sgi_specifier(parser_ac4, index, 2, 0);
                ac4_sgi_specifier(parser_ac4, index, 2, 1);
                parser_ac4->n_substream_groups[index] = 1;
                break;
            case 2:        
                ac4_sgi_specifier(parser_ac4, index, 3, 0);
                ac4_sgi_specifier(parser_ac4, index, 3, 1);
                parser_ac4->n_substream_groups[index] = 2;
                break;
            case 3:        
                ac4_sgi_specifier(parser_ac4, index, 4, 0);
                ac4_sgi_specifier(parser_ac4, index, 4, 1);
                ac4_sgi_specifier(parser_ac4, index, 4, 2);
                parser_ac4->n_substream_groups[index] = 3;
                break;
            case 4:        
                ac4_sgi_specifier(parser_ac4, index, 5, 0);
                ac4_sgi_specifier(parser_ac4, index, 5, 1);
                ac4_sgi_specifier(parser_ac4, index, 5, 2);
                parser_ac4->n_substream_groups[index] = 2;
                break;
            case 5:        
                parser_ac4->n_substream_groups[index] = (uint8_t)src_read_bits(ds, 2) + 2;
                if (parser_ac4->n_substream_groups[index] == 5) {
                    parser_ac4->n_substream_groups[index] += (uint8_t)variable_bits(2, ds);
                }
                for (i = 0; i < parser_ac4->n_substream_groups[index]; i++) {
                    ac4_sgi_specifier(parser_ac4, index, 6, i);
                }
                break;

            default: /* EMDF and other data */
                presentation_config_ext_info(parser_ac4, index);
                break;
            }
        }
    
        parser_ac4->b_pre_virtualized[index] = (uint8_t)src_read_bits(ds, 1);
        parser_ac4->b_add_emdf_substreams[index] = (uint8_t)src_read_bits(ds, 1);
        ac4_presentation_substream_info(parser_ac4->ds);
    }
    if (parser_ac4->b_add_emdf_substreams[index]) {
        parser_ac4->n_add_emdf_substreams[index] = (uint8_t)src_read_bits(ds, 2);
        if (parser_ac4->n_add_emdf_substreams[index] == 0) {
            parser_ac4->n_add_emdf_substreams[index] = (uint8_t)variable_bits(2, ds) + 4;
        }
        for (i = 0; i < parser_ac4->n_add_emdf_substreams[index]; i++) {
            add_emdf_info(parser_ac4, index, i);
        }
    }

    return 0;
}


/* Get time scale info as: ETSI TS 103 190 V 1.1.0 table E.1 Timescale for Media Header Box*/
static void
get_time_scale(parser_ac4_handle_t parser_ac4)
{
    if(parser_ac4->fs_index == 0)
    {
        if (parser_ac4->frame_rate_index == 13)
        {
            parser_ac4->time_scale = 44100;
            parser_ac4->num_units_in_tick = 2048;
        }
    }
    else if (parser_ac4->fs_index == 1)
    {
        switch (parser_ac4->frame_rate_index) {
            case 0:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 2002;
                break;
            case 1:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 2000;
                break;
            case 2:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 1920;
                break;
            case 3:
                parser_ac4->time_scale = 240000;
                parser_ac4->num_units_in_tick = 8008;
                break;
            case 4:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 1600;
                break;
            case 5:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 1001;
                break;
            case 6:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 1000;
                break;
            case 7:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 960;
                break;
            case 8:
                parser_ac4->time_scale = 240000;
                parser_ac4->num_units_in_tick = 4004;
                break;
            case 9:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 800;
                break;
            case 10:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 480;
                break;
            case 11:
                parser_ac4->time_scale = 240000;
                parser_ac4->num_units_in_tick = 2002;
                break;
            case 12:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 400;
                break;
            case 13:
                parser_ac4->time_scale = 48000;
                parser_ac4->num_units_in_tick = 2048;
                break;
            default:
                break;
        }
    }
}

/* Parsing toc as: ETSI TS 103 190-2 V 1.1.1 part 6.2.1 */
static int32_t 
parser_ac4_toc(parser_ac4_handle_t parser_ac4)
{
    bbio_handle_t       ds         = parser_ac4->ds;
    uint32_t tmp, payload_base, i,j;
    uint32_t ret = 0;

    src_byte_align(ds);
    parser_ac4->total_n_substream_groups = 0;
    memset(parser_ac4->group_index, -1, sizeof(parser_ac4->group_index));
    parser_ac4->bitstream_version = (uint8_t)src_read_bits(ds, 2);
    if (parser_ac4->bitstream_version == 3) 
    {
        parser_ac4->bitstream_version += (uint8_t)variable_bits(2, ds);
    }

    parser_ac4->sequence_counter = (uint8_t)src_read_bits(ds, 10);    /* sequence_counter, 10 bit*/

    tmp = (uint8_t)src_read_bits(ds, 1);    /*skip b_wait_frames, 1 bit*/
    if (tmp)
    {
        tmp = (uint8_t)src_read_bits(ds, 3); /*skip wait_frames, 3 bit*/
        if (tmp == 0)
        {
            parser_ac4->bit_rate_mode = 1;
        }
        else if ((tmp >0) && (tmp < 7))
        {
            parser_ac4->bit_rate_mode = 2;
        }
        else
        {
            parser_ac4->bit_rate_mode = 3;
        }

        if (tmp > 0)
        {
            tmp = (uint8_t)src_read_bits(ds, 2); /*skip br_code 2 bit*/
        }
    }

    parser_ac4->fs_index = (uint8_t)src_read_bits(ds, 1);
    parser_ac4->frame_rate_index = (uint8_t)src_read_bits(ds, 4);
    parser_ac4->b_iframe_global = (uint8_t)src_read_bits(ds, 1);

    tmp = (uint8_t)src_read_bits(ds, 1); /* b_single_presentation 1 bit */
    if (tmp) {
        parser_ac4->n_presentations = 1;
    }
    else {
        tmp = (uint8_t)src_read_bits(ds, 1); /* b_more_presentations 1 bit */
        if (tmp) {
            parser_ac4->n_presentations = variable_bits(2, ds) + 2;
        } else {
            parser_ac4->n_presentations = 0;
        }
    }

    payload_base = 0;
    tmp = (uint8_t)src_read_bits(ds, 1); /* b_payload_base 1 bit */
    if (tmp) {
        tmp = (uint8_t)src_read_bits(ds, 5); /* payload_base_minus1 5 bit */
        payload_base = tmp + 1;
        if (payload_base == 0x20) {
            payload_base += (uint8_t)variable_bits(3, ds);
        }
    }
    if (parser_ac4->bitstream_version <= 1) {
        printf("Error: AC4 ES with bitstream version 0 or 1 had been deprecated.\n ");
        return 1;
    }
    else
    {
        parser_ac4->b_program_id = (uint8_t)src_read_bits(ds, 1);
        if (parser_ac4->b_program_id) {
            parser_ac4->short_program_id = (uint16_t)src_read_bits(ds, 16);
            parser_ac4->b_program_uuid_present = (uint8_t)src_read_bits(ds, 1);
            if (parser_ac4->b_program_uuid_present) {
                for (i = 0; i < 8; i++)
                    parser_ac4->program_uuid[i] = (uint16_t)src_read_bits(ds, 16);
            }
        }
        for (i = 0; i < parser_ac4->n_presentations; i++) {
            ret = ac4_presentation_v1_info(parser_ac4, i);
            if(ret)
            {
                return 1;
            }
        }
        parser_ac4->total_n_substream_groups = 1 + parser_ac4->max_group_index;
        for (j = 0; j < parser_ac4->total_n_substream_groups; j++) {
            ac4_substream_group_info(parser_ac4, j);
        }
        for (i = 0; i < parser_ac4->n_presentations; i++) {
            parser_ac4->pres_ch_mode[i] = (uint8_t)generate_presentation_ch_mode(parser_ac4, i);
        }
    }

    return 0;
    /* ac4 dsi don't need info from the following table, just skip it */
    /*  substream_index_table(); */
    /*  byte_align; */

}

/* get channel count from ch_mode; Spec: 6.3.2.7.2 table 78*/
/* attention: ch_mode is not the value of channel_mode */
static int32_t
get_channel_count(int32_t ch_mode)
{
    switch (ch_mode) {
    case 0x0:
    case 0x1:
    case 0x2:
        return ch_mode + 1;
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
        return ch_mode + 2;
    case 0x7:
    case 0x9:
        return 7;
    case 0x8:
    case 0x10:
        return 8;
    case 11:
    case 12:
    case 13:
    case 14:
        return ch_mode;
    case 15:
        return 24;
    default:
        return 0;
    }
}

static int32_t
get_channel_count_new(parser_ac4_handle_t parser)
{
    int32_t channel_count = 0;
    int32_t channel_mask = 0;
    int32_t i = 0;

    channel_mask = generate_real_channel_mask(parser, 0, -1, -1);

    /* channel_mask equal to -1, means presentation 0 is not channel coded. */
    if(channel_mask == -1)
    {
        return 2;
    }

    for(i = 0; i < 19; i++)
    {
        if ((channel_mask >> i) & 1)
        {
            if ((i == 1) || (i == 6) || (i == 9) || (i == 10)
                || (i == 11) || (i == 12) || (i == 14) || (i == 15))
            {
                channel_count += 1;
            }
            else 
            {
                channel_count += 2;
            }
        }
    }

    return channel_count;
}

static int32_t
parser_ac4_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    parser_ac4_handle_t parser_ac4 = (parser_ac4_handle_t)parser;
    bbio_handle_t       ds         = parser->ds;
    int32_t                 sync;
    int64_t             file_offset;
    int32_t ret = 0;

    sample->flags = 0;

    if (ds->is_EOD(ds))
    {
        return EMA_MP4_MUXED_EOES;
    }

    /* get new syncframe */
    sync = parser_ac4_get_sync(parser_ac4, ds);
    if (!sync)
    {
        /* no sync header found */
        return EMA_MP4_MUXED_EOES;
    }

    parser_ac4->frame_size = src_read_u16(ds); 
    if (parser_ac4->frame_size == 0xffff)
    {
        parser_ac4->frame_size = src_read_u24(ds);
    }

    /* no check for config changes - needed? */

    /* save file offset as the sample data start here */
    file_offset = ds->position(ds);

    ret = parser_ac4_toc(parser_ac4);

    if(ret)
    {
        return EMA_MP4_MUXED_ES_ERR;
    }

    if (parser_ac4->sample_num)
    {
        sample->dts += parser_ac4->num_units_in_tick;
    }
    else
    {
        /* get channelcount info from the first sample's toc (the first present is the default present)*/
        /*  The ChannelCount field should be set to the total number of audio output channels of the default presentation of that
            track, if not defined differently by an application standard.*/
        parser_ac4->channelcount = get_channel_count_new(parser_ac4);

        /* the first one should have all the new info */
        sample->flags |= SAMPLE_NEW_SD;
        sample->dts = 0;
        get_time_scale(parser_ac4);
        /* get samplerate */
        if (parser_ac4->fs_index == 0)
        {
            parser_ac4->sample_rate = 44100;
        }
    }
    sample->cts      = sample->dts;
    sample->duration = parser_ac4->num_units_in_tick;

    if (parser_ac4->frame_size > parser_ac4->sample_buf_size)
    {
        sample->data = REALLOC_CHK(sample->data, parser_ac4->frame_size);
        parser_ac4->sample_buf_size = parser_ac4->frame_size;
    }
    sample->size = parser_ac4->frame_size;

    /*  check if this frame is I-frame; not all ac4 samples are sync frames */
    if (parser_ac4->b_iframe_global)
    {
        sample->flags |= SAMPLE_SYNC;
    }
    else
    {
        if (!parser_ac4->sample_num)
        {
             msglog(NULL, MSGLOG_WARNING, "Warning: The first AC-4 frame should be I frame !\n");
        }
    }

    /* Now the first sequence counter could not be 0! 
    if (!parser_ac4->sample_num && parser_ac4->sequence_counter)
    {
        msglog(NULL, MSGLOG_ERR, "The first AC-4 frame's sequence counter must be 0 !\n");
        return EMA_MP4_MUXED_ES_ERR;
    }*/

    /* the spec defines a ngc sample only include raw data,
       not including sync words/sample size/crc  */
    ds->seek(ds, file_offset, SEEK_SET); 
    ds->read(ds, sample->data, sample->size);

    /* removing 16-bit CRC words */
    if (sync == 1)
    {
        ds->skip_bytes(ds, 2);
    }

    parser_ac4->sample_num++;
    src_byte_align(ds);
    DPRINTF(NULL, "AC-4 frame size %d\n", parser_ac4->frame_size);

    return EMA_MP4_MUXED_OK;
}

/* currently just create dsi as spec: ETSI TS 103 190 V 1.1.0 */
static int32_t /* return number of written bits */
ac4_substream_dsi(parser_ac4_handle_t parser_ac4, bbio_handle_t snk, int32_t presentation_idx, int32_t substream_idx)
{
    int32_t      payloadbits = 0;
    uint32_t i;

    sink_write_bits(snk, 5, parser_ac4->ch_mode[presentation_idx][substream_idx]);
    sink_write_bits(snk, 2, parser_ac4->dsi_sf_multiplier[presentation_idx][substream_idx]);
    sink_write_bits(snk, 1, parser_ac4->b_bitrate_info[presentation_idx][substream_idx]);
    payloadbits += 8;

    if (parser_ac4->b_bitrate_info[presentation_idx][substream_idx]) {
        sink_write_bits(snk, 5, parser_ac4->bitrate_indicator[presentation_idx][substream_idx]);
        payloadbits += 5;
    }
    if (parser_ac4->ch_mode[presentation_idx][substream_idx] > 6) {  /* ch_mode == [7...10]  ref: page 68, table 87 */
        sink_write_bits(snk, 1, parser_ac4->add_ch_base[presentation_idx][substream_idx]);
        payloadbits += 1;
    }

    sink_write_bits(snk, 1, parser_ac4->b_content_type[presentation_idx][substream_idx]);
    payloadbits += 1;
    if (parser_ac4->b_content_type[presentation_idx][substream_idx]) {
        sink_write_bits(snk, 3, parser_ac4->content_classifier[presentation_idx][substream_idx]);
        sink_write_bits(snk, 1, parser_ac4->b_language_indicator[presentation_idx][substream_idx]);
        payloadbits += 4;

        if (parser_ac4->b_language_indicator[presentation_idx][substream_idx]) {
            sink_write_bits(snk, 6, parser_ac4->n_language_tag_bytes[presentation_idx][substream_idx]);
            payloadbits += 6;
            for (i = 0; i < parser_ac4->n_language_tag_bytes[presentation_idx][substream_idx]; i++) {
                sink_write_bits(snk, 8, parser_ac4->language_tag_bytes[presentation_idx][substream_idx][i]);
                payloadbits += 8;
            }
        }
    }
    return payloadbits;
}


static int32_t /* calc number of bits to be written */
calc_ac4_substream_dsi(parser_ac4_handle_t parser_ac4, int32_t presentation_idx, int32_t substream_idx)
{
    int32_t      payloadbits = 0;
    uint32_t i;

    payloadbits += 8;
    if (parser_ac4->b_bitrate_info[presentation_idx][substream_idx]) {
        payloadbits += 5;
    }
    if (parser_ac4->ch_mode[presentation_idx][substream_idx] > 0x79 ) {  /* ch_mode == [7...10]  ref: page 68, table 87 */
        payloadbits += 1;
    }

    payloadbits += 1;
    if (parser_ac4->b_content_type[presentation_idx][substream_idx]) {
        payloadbits += 4;
        if (parser_ac4->b_language_indicator[presentation_idx][substream_idx]) {
            payloadbits += 6;
            for (i = 0; i < parser_ac4->n_language_tag_bytes[presentation_idx][substream_idx]; i++) {
                payloadbits += 8;
            }
        }
    }
    return payloadbits;
}

static int32_t /* calc number of bits to be written */
calc_presentation_v0_dsi (parser_ac4_handle_t parser_ac4, int32_t presentation_idx)
{
    int32_t      payloadBits = 0;
    uint32_t i = presentation_idx;
    uint32_t j;

    payloadBits += 5;
    if (parser_ac4->presentation_config[i] != 6)
    {
        payloadBits += 4;

        if (parser_ac4->b_presentation_id[i]) 
        {
            payloadBits += 5;
        }

        payloadBits += 41;

        if (parser_ac4->b_single_substream[i] == 1)
        {
            payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 0);
        }
        else {
            payloadBits += 1;

            switch(parser_ac4->presentation_config[i]) {
            case 0:
            case 1:
            case 2:
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 0);
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 1);
                break;
            case 3:
            case 4:
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 0);
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 1);
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 2);
                break;
            case 5:
                payloadBits += calc_ac4_substream_dsi(parser_ac4 , i, 0);
                break;
            default:
                payloadBits += 7;
                payloadBits += 8 * parser_ac4->n_skip_bytes[i];
                break;
            }
            payloadBits += 2;
        }
    }
    if (parser_ac4->b_add_emdf_substreams[i])
    {
        payloadBits += 7;
        for (j = 0; j < parser_ac4->n_add_emdf_substreams[i]; j++) {
            payloadBits += 15;
        }
    }

    if (payloadBits % 8)
    {
        payloadBits += (8 - (payloadBits % 8));
    }

    return payloadBits;
}
static int32_t /* return number of writen bits */
presentation_v0_dsi (parser_ac4_handle_t parser_ac4, bbio_handle_t snk, int32_t presentation_idx)
{
    int32_t      payloadBits = 0;
    uint32_t i = presentation_idx;
    uint32_t j;

    sink_write_bits(snk, 5, parser_ac4->presentation_config[i]);
    payloadBits += 5;

    
    if (parser_ac4->presentation_config[i] == 6)
    {
        parser_ac4->b_add_emdf_substreams[i] = 1;
    }
    else
    {
        /* ETSI TS 103 190-1 V1.1.2 changed  */
        sink_write_bits(snk, 3, parser_ac4->mdcompat[i]);
        sink_write_bits(snk, 1, parser_ac4->b_presentation_id[i]);
        payloadBits += 4;

        if (parser_ac4->b_presentation_id[i]) 
        {
            sink_write_bits(snk, 5, parser_ac4->presentation_id[i]);
            payloadBits += 5;
        }

        sink_write_bits(snk, 2, parser_ac4->dsi_frame_rate_multiply_info[i]);
        sink_write_bits(snk, 5, parser_ac4->emdf_version[i]);
        sink_write_bits(snk, 10, parser_ac4->key_id[i]);
        
        {
            int32_t substreams_channel_mask = 0;
            int32_t substream_index = 0;
            
            for(substream_index = 0; substream_index < 3; substream_index++)
            {
                if (parser_ac4->ch_mode[presentation_idx][substream_index] != -1)
                {
                    substreams_channel_mask |= chmode_2_channel_mask[parser_ac4->ch_mode[presentation_idx][substream_index]];
                }
            }
            sink_write_bits(snk, 24, substreams_channel_mask);
        }

        payloadBits += 41;

        if (parser_ac4->b_single_substream[i] == 1)
        {
            payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 0);
        }
        else {
            sink_write_bits(snk, 1, parser_ac4->b_hsf_ext[i]);
            payloadBits += 1;

            switch(parser_ac4->presentation_config[i]) {
            case 0:
            case 1:
            case 2:
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 0);
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 1);
                break;
            case 3:
            case 4:
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 0);
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 1);
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 2);
                break;
            case 5:
                payloadBits += ac4_substream_dsi(parser_ac4 ,snk, i, 0);
                break;
            default:
                sink_write_bits(snk, 7, parser_ac4->n_skip_bytes[i]);
                payloadBits += 7;
                for (j = 0; j < parser_ac4->n_skip_bytes[i]; j++) {
                    sink_write_u8(snk, parser_ac4->skip_bytes_address[i][j]);
                }
                if (parser_ac4->skip_bytes_address[i]) {
                    FREE_CHK(parser_ac4->skip_bytes_address[i]);
                    parser_ac4->skip_bytes_address[i] = NULL;
                }
                payloadBits += 8 * parser_ac4->n_skip_bytes[i];
                break;
            }
            sink_write_bits(snk, 1, parser_ac4->b_pre_virtualized[i]);
            sink_write_bits(snk, 1, parser_ac4->b_add_emdf_substreams[i]);
            payloadBits += 2;
        }
    }
    if (parser_ac4->b_add_emdf_substreams[i])
    {
        sink_write_bits(snk, 7, parser_ac4->n_add_emdf_substreams[i]);
        payloadBits += 7;
        for (j = 0; j < parser_ac4->n_add_emdf_substreams[i]; j++) {
            sink_write_bits(snk, 5, parser_ac4->add_emdf_version[i][j]);
            sink_write_bits(snk, 10, parser_ac4->add_key_id[i][j]);
            payloadBits += 15;
        }
    }
    /* byte_align */
    if (payloadBits % 8)
    {
        sink_write_bits(snk, 8 - (payloadBits % 8), 0);
        payloadBits += (8 - (payloadBits % 8));
    }

    return payloadBits;
}

static int32_t
ac4_substream_group_dsi(parser_ac4_handle_t parser_ac4, bbio_handle_t snk, int32_t sg_idx)
{
    int32_t      payloadbits = 0;
    uint32_t i;
    int8_t temp = 0;
    uint8_t objects_assignment_mask = 0;

    sink_write_bits(snk, 1, parser_ac4->b_substreams_present[sg_idx]);
    sink_write_bits(snk, 1, parser_ac4->b_hsf_ext_v2[sg_idx]);
    sink_write_bits(snk, 1, parser_ac4->b_channel_coded[sg_idx]);
    temp = (int8_t)(parser_ac4->n_lf_substreams_minus2[sg_idx]+2);
    sink_write_bits(snk, 8, temp);
    payloadbits += 11;

    for(i = 0; i < (uint32_t)temp; i++) {
        sink_write_bits(snk, 2, parser_ac4->sf_multiplier[sg_idx][i]); /* dsi_sf_multiplier, 00: 48k */
        sink_write_bits(snk, 1, parser_ac4->bitrate_indicator_v2[sg_idx][i]); 
        payloadbits += 3;
        if (parser_ac4->b_channel_coded[sg_idx]) {
            int32_t real_ch_mode = 0;
            real_ch_mode = generate_real_channel_mask(parser_ac4, -1, sg_idx, i);
            sink_write_bits(snk, 24, real_ch_mode);
            payloadbits += 24;
        }
        else {
            sink_write_bits(snk, 1, parser_ac4->b_ajoc[sg_idx][i]); /* b_ajoc */
            payloadbits += 1;
            if (parser_ac4->b_ajoc[sg_idx][i]) {
                sink_write_bits(snk, 1, parser_ac4->b_static_dmx[sg_idx][i]); /* b_static_dmx */
                payloadbits += 1;
                if (parser_ac4->b_static_dmx[sg_idx][i] == 0)
                {
                    sink_write_bits(snk, 4, parser_ac4->n_fullband_dmx_signals_minus1[sg_idx][i]); /* n_dmx_objects_minus1 */
                    payloadbits += 4;
                }
                sink_write_bits(snk, 6, parser_ac4->n_fullband_upmix_signals_minus1[sg_idx][i]); /* n_umx_objects_minus1 */
                payloadbits += 6;
            }

            objects_assignment_mask = 0;
            if (parser_ac4->b_bed_objects[sg_idx][i])
                objects_assignment_mask |= 8;

            if (parser_ac4->b_dynamic_objects[sg_idx][i] | parser_ac4->b_dyn_objects_only[sg_idx][i])
                objects_assignment_mask |= 4;
            
            if (parser_ac4->b_isf[sg_idx][i])
                objects_assignment_mask |= 2;

            if (objects_assignment_mask == 0)
                objects_assignment_mask = 1; /* objects_assignment_mask = 1 -> reserved */

            sink_write_bits(snk, 4, objects_assignment_mask); 
            payloadbits += 4;
        }
    }

    sink_write_bits(snk, 1, parser_ac4->b_content_type_v2[sg_idx]);
    payloadbits += 1;
    if (parser_ac4->b_content_type_v2[sg_idx]) {
        sink_write_bits(snk, 3, parser_ac4->content_classifier_v2[sg_idx]);
        sink_write_bits(snk, 1, parser_ac4->b_language_indicator_v2[sg_idx]);
        payloadbits += 4;

        if (parser_ac4->b_language_indicator_v2[sg_idx]) {
            sink_write_bits(snk, 6, parser_ac4->n_language_tag_bytes_v2[sg_idx]);
            payloadbits += 6;
            for (i = 0; i < parser_ac4->n_language_tag_bytes_v2[sg_idx]; i++) {
                sink_write_bits(snk, 8, parser_ac4->language_tag_bytes_v2[sg_idx][i]);
                payloadbits += 8;
            }
        }
    }

    return payloadbits;
}

static int32_t
calc_ac4_substream_group_dsi(parser_ac4_handle_t parser_ac4, int32_t sg_idx)
{
    int32_t      payloadbits = 0;
    uint32_t i;
    int8_t temp = 0;

    temp = (int8_t)(parser_ac4->n_lf_substreams_minus2[sg_idx]+2);
    payloadbits += 11;

    for(i = 0; i < (uint32_t)temp; i++) {
        payloadbits += 3;
        if (parser_ac4->b_channel_coded[sg_idx]) {
            payloadbits += 24;
        }
        else {
            payloadbits += 1;
            if (parser_ac4->b_ajoc[sg_idx][i]) {
                payloadbits += 1;
                if (parser_ac4->b_static_dmx[sg_idx][i] == 0)
                {
                    payloadbits += 4;
                }
                payloadbits += 6;
            }
            payloadbits += 4;
        }
    }

    payloadbits += 1;
    if (parser_ac4->b_content_type_v2[sg_idx]) {
        payloadbits += 4;

        if (parser_ac4->b_language_indicator_v2[sg_idx]) {
            payloadbits += 6;
            for (i = 0; i < parser_ac4->n_language_tag_bytes_v2[sg_idx]; i++) {
                payloadbits += 8;
            }
        }
    }

    return payloadbits;
}


static int32_t /* return number of bits to be written */
calc_presentation_v1_dsi (parser_ac4_handle_t parser_ac4, int32_t presentation_idx)
{
    int32_t      payloadBits = 0;
    uint32_t i = presentation_idx;
    uint32_t j;
    uint32_t b_pres_channel_coded = 0;
    uint32_t substream_group_index = 0;
    uint8_t b_presentation_core_differs = 0;

    payloadBits += 5;

    
    if (parser_ac4->presentation_config[i] == 6)
    {
        parser_ac4->b_add_emdf_substreams[i] = 1;
    }
    else
    {
        payloadBits += 4;

        if (parser_ac4->b_presentation_id[i]) 
        {
            payloadBits += 5;
        }

        /* calc presentation channel mode */
        parser_ac4->pres_ch_mode[i] = (uint8_t)generate_presentation_ch_mode(parser_ac4, presentation_idx);
        parser_ac4->pres_ch_mode_core[i] = (uint8_t)generate_presentation_ch_mode_core(parser_ac4, presentation_idx);
        if (parser_ac4->pres_ch_mode[i] == 0xff)
        {
            b_pres_channel_coded = 0;
        }
        else
        {
            b_pres_channel_coded = 1;
        }
        payloadBits += 20;

        if (b_pres_channel_coded) {
            payloadBits += 5;
            if (parser_ac4->pres_ch_mode[i] == 11 || 
                parser_ac4->pres_ch_mode[i] == 12 ||
                parser_ac4->pres_ch_mode[i] == 13 ||
                parser_ac4->pres_ch_mode[i] == 14 )
            {
                payloadBits += 3;
            }
            payloadBits += 24;
        }
         /* b_presentation_core_differs */

        if (parser_ac4->pres_ch_mode_core[i] == 0xff)
        {
            b_presentation_core_differs = 0;
        }
        else
        {
            b_presentation_core_differs = 1;
        } 
        
        payloadBits += 1;

        if (b_presentation_core_differs) {
            payloadBits += 1;
            if (parser_ac4->pres_ch_mode_core[i] != 0xff) {
                payloadBits += 2;
            }
        }
        payloadBits += 1;

        if (parser_ac4->b_presentation_filter[i]) {
            payloadBits += 9;
        }

        if (parser_ac4->b_single_substream_group[i] == 1)
        {
            substream_group_index = parser_ac4->group_index[i][0];
            payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
        }
        else 
        {
            payloadBits += 1;

            switch(parser_ac4->presentation_config[i]) {
            case 0:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                break;
            case 1:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                break;
            case 2:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                break;
            case 3:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][2];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                break;
            case 4:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                substream_group_index = parser_ac4->group_index[i][2];
                payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);
                break;
            case 5:
                payloadBits += 3;
                for (j = 0; j < parser_ac4->n_substream_groups[i]; j++) {
                    substream_group_index = parser_ac4->group_index[i][j];
                    payloadBits += calc_ac4_substream_group_dsi(parser_ac4 , substream_group_index);;
                }
                break;
            default:
                payloadBits += 7;
                payloadBits += 8 * parser_ac4->n_skip_bytes[i];
                break;
            }
        }
        payloadBits += 2;
    }
    if (parser_ac4->b_add_emdf_substreams[i])
    {
        payloadBits += 7;
        for (j = 0; j < parser_ac4->n_add_emdf_substreams[i]; j++) {
            payloadBits += 15;
        }
    }

    payloadBits += 2;
    /*  byte_align */
    if (payloadBits % 8)
    {
        payloadBits += (8 - (payloadBits % 8));
    }

    payloadBits += 8;

    return payloadBits;
}

/** Based on spec: ETSI TS 103 190-2 V1.1.1 part E.10 */
static int32_t /* return number of written bits */
presentation_v1_dsi(parser_ac4_handle_t parser_ac4, bbio_handle_t snk, int32_t presentation_idx, int32_t is_IMS, int32_t is_duplicated)
{
    int32_t      payloadBits = 0;
    uint32_t i = presentation_idx;
    uint32_t j;
    uint32_t b_pres_channel_coded = 0;
    uint32_t substream_group_index = 0;
    uint8_t b_presentation_core_differs = 0;

    if (parser_ac4->b_single_substream_group[i])
    {
        sink_write_bits(snk, 5, 0x1f);
    }
    else
    {
        sink_write_bits(snk, 5, parser_ac4->presentation_config[i]);
    }
    payloadBits += 5;

    
    if (parser_ac4->presentation_config[i] == 6)
    {
        parser_ac4->b_add_emdf_substreams[i] = 1;
    }
    else
    {
        sink_write_bits(snk, 3, parser_ac4->mdcompat[i]);
        sink_write_bits(snk, 1, parser_ac4->b_presentation_id[i]);
        payloadBits += 4;

        if (parser_ac4->b_presentation_id[i]) 
        {
            sink_write_bits(snk, 5, parser_ac4->presentation_id[i]);
            payloadBits += 5;
        }

        sink_write_bits(snk, 2, parser_ac4->dsi_frame_rate_multiply_info[i]);
        sink_write_bits(snk, 2, parser_ac4->dsi_frame_rate_fractions_info[i]);
        sink_write_bits(snk, 5, parser_ac4->emdf_version[i]);
        sink_write_bits(snk, 10, parser_ac4->key_id[i]);

        /* calc presentation channel mode */
        parser_ac4->pres_ch_mode[i] = (uint8_t)generate_presentation_ch_mode(parser_ac4, presentation_idx);
        parser_ac4->pres_ch_mode_core[i] = (uint8_t)generate_presentation_ch_mode_core(parser_ac4, presentation_idx);
        if (parser_ac4->pres_ch_mode[i] == 0xff)
        {
            b_pres_channel_coded = 0;
        }
        else
        {
            b_pres_channel_coded = 1;
        }
        sink_write_bits(snk, 1, b_pres_channel_coded);
        payloadBits += 20;

        if (b_pres_channel_coded) {
            sink_write_bits(snk, 5, parser_ac4->pres_ch_mode[i]); /* dsi_presentation_ch_mode */
            payloadBits += 5;
            if (parser_ac4->pres_ch_mode[i] == 11 || 
                parser_ac4->pres_ch_mode[i] == 12 ||
                parser_ac4->pres_ch_mode[i] == 13 ||
                parser_ac4->pres_ch_mode[i] == 14 )
            {
                /* pres_b_4_back_channels_present 1bit */
                int32_t k = 0;
                int32_t temp = 0;
                
                for (k = 0; k < parser_ac4->n_substream_groups[i]; k++)
                {
                    temp = temp | parser_ac4->b_4_back_channels_present[parser_ac4->group_index[i][k]][0];
                }

                if (temp)
                    sink_write_bits(snk, 1, 1);
                else
                    sink_write_bits(snk, 1, 0);

                /** pres_top_channel_pairs 2bits */
                temp = 0;
                for (k = 0; k < parser_ac4->n_substream_groups[i]; k++)
                {
                    if (parser_ac4->top_channels_present[parser_ac4->group_index[i][k]][0] > temp)
                        temp = parser_ac4->top_channels_present[parser_ac4->group_index[i][k]][0];
                }
                if ((temp == 1) || (temp == 2))
                    sink_write_bits(snk, 2, 1);
                else if (temp == 3)
                    sink_write_bits(snk, 2, 2);
                else
                    sink_write_bits(snk, 2, 0);

                payloadBits += 3;
            }
            
            {
                int32_t real_mask = 0;

                real_mask = generate_real_channel_mask(parser_ac4, presentation_idx, -1, -1);
                sink_write_bits(snk, 24, real_mask);
                payloadBits += 24;
            }
        }
         /* b_presentation_core_differs */
        if (parser_ac4->pres_ch_mode_core[i] == 0xff)
        {
            b_presentation_core_differs = 0;
        }
        else
        {
            b_presentation_core_differs = 1;
        } 
        
        sink_write_bits(snk, 1, b_presentation_core_differs);
        payloadBits += 1;

        if (b_presentation_core_differs) {
            if (parser_ac4->pres_ch_mode_core[i] != 0xff) {
                sink_write_bits(snk, 1, 1);
                payloadBits += 1;
                sink_write_bits(snk, 2, parser_ac4->pres_ch_mode_core[i] - 3);
                payloadBits += 2;
            }
            else
            {
                sink_write_bits(snk, 1, 0);
                payloadBits += 1;
            }
        }

        sink_write_bits(snk, 1, parser_ac4->b_presentation_filter[i]);
        payloadBits += 1;

        if (parser_ac4->b_presentation_filter[i]) {
            sink_write_bits(snk, 1, parser_ac4->b_enable_presentation[i]);

            sink_write_bits(snk, 8, 0); /* parser_ac4->n_filter_bytes == 0; */
            payloadBits += 9;
        }

        if (parser_ac4->b_single_substream_group[i] == 1)
        {
            substream_group_index = parser_ac4->group_index[i][0];
            payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
        }
        else {
            sink_write_bits(snk, 1, parser_ac4->b_multi_pid[i]);
            payloadBits += 1;

            switch(parser_ac4->presentation_config[i]) {
            case 0:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                break;
            case 1:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                break;
            case 2:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk,substream_group_index);
                break;
            case 3:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                substream_group_index = parser_ac4->group_index[i][2];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                break;
            case 4:
                substream_group_index = parser_ac4->group_index[i][0];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                substream_group_index = parser_ac4->group_index[i][1];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                substream_group_index = parser_ac4->group_index[i][2];
                payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                break;
            case 5:
                sink_write_bits(snk, 3, parser_ac4->n_substream_groups[i] - 2);
                payloadBits += 3;
                for (j = 0; j < parser_ac4->n_substream_groups[i]; j++) {
                    substream_group_index = parser_ac4->group_index[i][j];
                    payloadBits += ac4_substream_group_dsi(parser_ac4 ,snk, substream_group_index);
                }
                break;
            default:
                sink_write_bits(snk, 7, parser_ac4->n_skip_bytes[i]);
                payloadBits += 7;
                for (j = 0; j < parser_ac4->n_skip_bytes[i]; j++) {
                    sink_write_u8(snk, parser_ac4->skip_bytes_address[i][j]);
                }
                if (parser_ac4->skip_bytes_address[i]) {
                    FREE_CHK(parser_ac4->skip_bytes_address[i]);
                    parser_ac4->skip_bytes_address[i] = NULL;
                }
                payloadBits += 8 * parser_ac4->n_skip_bytes[i];
                break;
            }
        }
        
        /** IMS Presentation */
        if (is_IMS && (!is_duplicated))
        {
             sink_write_bits(snk, 1, 1);
        }
        else
        {
            sink_write_bits(snk, 1, parser_ac4->b_pre_virtualized[i]);
        }
        
        sink_write_bits(snk, 1, parser_ac4->b_add_emdf_substreams[i]);
        payloadBits += 2;
    }
    if (parser_ac4->b_add_emdf_substreams[i])
    {
        sink_write_bits(snk, 7, parser_ac4->n_add_emdf_substreams[i]);
        payloadBits += 7;
        for (j = 0; j < parser_ac4->n_add_emdf_substreams[i]; j++) {
            sink_write_bits(snk, 5, parser_ac4->add_emdf_version[i][j]);
            sink_write_bits(snk, 10, parser_ac4->add_key_id[i][j]);
            payloadBits += 15;
        }
    }

    sink_write_bits(snk, 1, 0); /* b_presentation_bitrate_info */

    sink_write_bits(snk, 1, 0); /* b_alternative */

    payloadBits += 2;
    /* byte_align */
    /* *buf_len += (size_t)(payloadBits >> 3); */
    if (payloadBits % 8)
    {
        /* *buf_len += 1; */
        sink_write_bits(snk, 8 - (payloadBits % 8), 0);
        /* sink_flush_bits(snk); */
        payloadBits += (8 - (payloadBits % 8));
    }
    /* store DE indicator; atmos indicator */
    sink_write_bits(snk, 1, 1);

    /** IMS Presentation */
    if (parser_ac4->isAtmos[presentation_idx])
    {
        sink_write_bits(snk, 1, 1);
    }
    else
    {
        sink_write_bits(snk, 1, 0);
    }
    sink_write_bits(snk, 6, 0);
    payloadBits += 8;

    return payloadBits;
}

static int32_t
parser_ac4_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    parser_ac4_handle_t parser_ac4   = (parser_ac4_handle_t)parser;
    uint32_t           i,j;
    uint32_t           payloadBits = 0;
    uint32_t           pre_calc_bytes = 0, tmp;
    uint32_t           is_duplicate_dsi = 0;
    bbio_handle_t      snk;
    uint32_t imsPresentationNum = 0;

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 80, 1); /* just set to 80 now */
    }

    parser->ac4_bitstream_version = parser_ac4->bitstream_version;
    parser->ac4_presentation_version = parser_ac4->presentation_version[0];
    parser->ac4_mdcompat = parser_ac4->mdcompat[0];

    sink_flush_bits(snk);
    sink_write_bits(snk, 3, 1);  /* ac4_dsi_version field shall be set to '001' */
    sink_write_bits(snk, 7, parser_ac4->bitstream_version);
    sink_write_bits(snk, 1, parser_ac4->fs_index);
    sink_write_bits(snk, 4, parser_ac4->frame_rate_index);
    
    /** single presentation and it's IMS, we'll add more presentation */
    for(i = 0; i < parser_ac4->n_presentations; i++)
    {
        if(parser_ac4->presentation_version[i] == 2)
            imsPresentationNum++;
    }

    sink_write_bits(snk, 9, parser_ac4->n_presentations + imsPresentationNum);

    payloadBits += 24;

    if(parser_ac4->bitstream_version > 1) {
        sink_write_bits(snk, 1, parser_ac4->b_program_id);
        payloadBits +=1;
        if (parser_ac4->b_program_id) {
            sink_write_bits(snk, 16, parser_ac4->short_program_id);
            sink_write_bits(snk, 1, parser_ac4->b_program_uuid_present);
            payloadBits += 17;
            if (parser_ac4->b_program_uuid_present) {
                for(i = 0; i < 8; i++)
                    sink_write_bits(snk, 16, parser_ac4->program_uuid[i]);
                payloadBits += 128;
            }
        }
    }

    /* ac4_bitrate_dsi structure */
    sink_write_bits(snk, 2, parser_ac4->bit_rate_mode);
    sink_write_bits(snk, 32, parser->ext_timing.ac4_bitrate);
    sink_write_bits(snk, 32, parser->ext_timing.ac4_bitrate_precision);
    payloadBits += 66;

    /* byte_align */
    /* *buf_len += (size_t)(payloadBits >> 3); */
    if (payloadBits % 8)
    {
        /* *buf_len += 1; */
        sink_write_bits(snk, 8 - (payloadBits % 8), 0);
        payloadBits += (8 - payloadBits % 8);
    }

    for (i = 0; i < parser_ac4->n_presentations; i++)
    {
        uint32_t presentation_bytes = 0, presentation_bits = 0;
        sink_write_bits(snk, 8, parser_ac4->presentation_version[i]); /* presentation_version */

        /* pre calc this presenation bytes  */
        if (parser_ac4->presentation_version[i] == 0) {
            tmp = calc_presentation_v0_dsi(parser_ac4 , i);    
            pre_calc_bytes = tmp >> 3;
        }
        else {
            if (parser_ac4->presentation_version[i] > 0) {
                tmp = calc_presentation_v1_dsi(parser_ac4 , i);
                pre_calc_bytes = tmp >> 3;
            }
        }

        if (pre_calc_bytes > 255) {
            sink_write_bits(snk, 8, 0xff);
            sink_write_bits(snk, 16, pre_calc_bytes - 255);
        }
        else {
            sink_write_bits(snk, 8, pre_calc_bytes);
        }
        
        if (parser_ac4->presentation_version[i] == 0) {
            presentation_bits = presentation_v0_dsi(parser_ac4 ,snk, i);    
            presentation_bytes = presentation_bits >> 3;
            payloadBits += presentation_bits;
        }
        else {
            if (parser_ac4->presentation_version[i] == 1) {
                presentation_bits = presentation_v1_dsi(parser_ac4 ,snk, i, 0, 0);
                presentation_bytes = presentation_bits >> 3;
                payloadBits += presentation_bits;
            }
            else if (parser_ac4->presentation_version[i] == 2) {
                presentation_bits = presentation_v1_dsi(parser_ac4 ,snk, i, 1, 0);
                presentation_bytes = presentation_bits >> 3;
                payloadBits += presentation_bits;
            }
            else {
                presentation_bytes = 0;
            }
        }
        assert(presentation_bytes == pre_calc_bytes);
        /* as pre_bytes could be bigger, skip area needed. */
        for(j = 0; j < pre_calc_bytes - presentation_bytes; j ++)
        {
            sink_write_bits(snk, 8, 0);
        }

        /** IMS duplicated presentation DSI */
        if (parser_ac4->presentation_version[i] == 2)
        {
            uint32_t presentation_bytes = 0, presentation_bits = 0;
            sink_write_bits(snk, 8, 1);
            if (pre_calc_bytes > 255) 
            {
                sink_write_bits(snk, 8, 0xff);
                sink_write_bits(snk, 16, pre_calc_bytes - 255);
            }
            else 
            {
                sink_write_bits(snk, 8, pre_calc_bytes);
            }

            presentation_bits = presentation_v1_dsi(parser_ac4 ,snk, i, 1, 1);
            presentation_bytes = presentation_bits >> 3;
            payloadBits += presentation_bits;

            assert(presentation_bytes == pre_calc_bytes);
            /** as pre_bytes could be bigger, skip area needed. */
            for(j = 0; j < pre_calc_bytes - presentation_bytes; j++)
            {
                sink_write_bits(snk, 8, 0);
            }
        }
    }
    /* sink_flush_bits(snk); already aligned */
    *buf = snk->get_buffer(snk, buf_len, 0);  /* here buf_len is set to data_size */
    snk->destroy(snk);
    return 0;
}

static void
parser_ac4_show_info(parser_handle_t parser)
{
    msglog(NULL, MSGLOG_INFO, "AC-4 Parser\n");
    (void)parser;  /* avoid compiler warning */
}

static int32_t
parser_ac4_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx,  bbio_handle_t ds)
{
    parser_ac4_handle_t parser_ac4 = (parser_ac4_handle_t)parser;

    parser->ext_timing = *ext_timing;
    parser->es_idx     = es_idx;
    parser->ds         = ds;

    if (!parser_ac4_get_sync(parser_ac4, ds))
    {
        return EMA_MP4_MUXED_EOES;  /* no sync header found */
    }

    /* NOTE: no parser_ac4_init_dsi(parser); call - just fixed values for the moment! */
    parser_ac4->samples_per_frame = 1920;
    parser_ac4->sample_rate       = 48000;

    /* reset data source to the beginning */
    ds->seek(ds, 0, SEEK_SET);

    return EMA_MP4_MUXED_OK;
}

static void
parser_ac4_destroy(parser_handle_t parser)
{
    parser_ac4_handle_t parser_ac4 = (parser_ac4_handle_t)parser;

    if (parser_ac4)
    {
        /* TBA */
    }
    parser_destroy(parser);
}

static parser_handle_t
parser_ac4_create(uint32_t dsi_type)
{
    parser_ac4_handle_t parser;

    assert(dsi_type == DSI_TYPE_MP4FF);
    parser = (parser_ac4_handle_t)MALLOC_CHK(sizeof(parser_ac4_t));
    if (!parser)
    {
        return 0;
    }
    memset(parser, 0, sizeof(parser_ac4_t));

    /* because channel_mode = 0 means mono, we just set -1 as inital channel_mode value*/
    memset(parser->ch_mode, -1, sizeof(parser->ch_mode));
    /**** build the interface, base for the instance */
    parser->stream_type = STREAM_TYPE_AUDIO;
    parser->stream_id   = STREAM_ID_AC4;
    parser->stream_name = "ac4";
    parser->dsi_FourCC  = "dac4";

    parser->dsi_type   = dsi_type;
    parser->dsi_create = dsi_ac4_create;

    parser->init       = parser_ac4_init;
    parser->destroy    = parser_ac4_destroy;
    parser->get_sample = parser_ac4_get_sample;
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->get_cfg = parser_ac4_get_mp4_cfg;
    }

    parser->show_info = parser_ac4_show_info;

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

    /**** ac4 specifics */

    /**** cast to base */
    return (parser_handle_t)parser;
}

void
parser_ac4_reg(void)
{
    reg_parser_set("ac4", parser_ac4_create);
}
