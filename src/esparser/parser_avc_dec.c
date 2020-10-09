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
    @file parser_avc_dec.c
    @brief Implements avc parser needed lower level structures and APIs
*/

#include "utils.h"
#include "io_base.h"
#include "registry.h"
#include "parser_avc_dec.h"

static const char *nal_type_tbl[] =
{
    "Unspecified",                      /* 0 */
    "Coded slice of non-IDR picture",   /* 1 */
    "Coded slice data partition A",     /* 2 */
    "Coded slice data partition B",     /* 3 */
    "Coded slice data partition C",     /* 4 */
    "Coded slice of an IDR picture",    /* 5 */
    "SEI",                              /* 6 */
    "SPS",                              /* 7 */
    "PPS",                              /* 8 */
    "AUD",                              /* 9 */
    "End of Sequence",                  /* 10 */
    "End of Stream",                    /* 11 */
    "Filler data",                      /* 12 */
    "SPS extension",                    /* 13 */
    "Prefix NAL unit",                  /* 14 */
    "Subset SPS",                       /* 15 */
    "reserved",                         /* 16 */
    "reserved",                         /* 17 */
    "reserved",                         /* 18 */
    "Coded slice of aux coded pic",     /* 19 */
    "Coded slice extension",            /* 20 */
    "reserved",                         /* 21 */
    "reserved",                         /* 22 */
    "reserved",                         /* 23 */
    "VDRD",                             /* 24 */
    "Dolby 3D ext"                      /* 25 */
};

static const char *
get_nal_unit_type_dscr(uint8_t type)
{
    if (type < 26)
    {
        return nal_type_tbl[type];
    }

    return "Unspecified";
}

static const pd_nalType_t nal_delimier_type_tbl[32] =
{
    /** 0 */
    PD_NAL_TYPE_NO,
    /** 1-2 VCL */
    PD_NAL_TYPE_VCL, PD_NAL_TYPE_VCL,
    /** 3-4 VCL */
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO,
    /** 5 VCL */
    PD_NAL_TYPE_VCL,
    /** 6-8 SEI, SPS, PPS */
    PD_NAL_TYPE_NOT_VCL, PD_NAL_TYPE_NOT_VCL, PD_NAL_TYPE_NOT_VCL,
    /** 9 AUD */
    PD_NAL_TYPE_NOT_VCL,
    /** 10-13 EOS, EOStrm, Filler, SPS_EXT */
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO,
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO,
    /** 14 VCL prefix */
    PD_NAL_TYPE_VCL,
    /** 15 SUBSET_SPS */
    PD_NAL_TYPE_NOT_SLICE_EXT,
    /** 16-18: reserved, assuming not vcl */
    PD_NAL_TYPE_NOT_VCL, PD_NAL_TYPE_NOT_VCL, PD_NAL_TYPE_NOT_VCL,
    /** 19 aux pic slice */
    PD_NAL_TYPE_NO,
    /** 20: SVC, MVC slice extension */
    PD_NAL_TYPE_SLICE_EXT,
    /** 21-23 */
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO, PD_NAL_TYPE_NO,
    /** 24: dependency representation delimiter */
    PD_NAL_TYPE_NOT_SLICE_EXT,
    /** 25: dolby 3d */
    PD_NAL_TYPE_NOT_SLICE_EXT,
    /** 26-31: not defined */
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO, PD_NAL_TYPE_NO, PD_NAL_TYPE_NO,
    PD_NAL_TYPE_NO, PD_NAL_TYPE_NO
};

static const uint8_t aspect_ratio_tbl[17][2] =
{
    {  0,   0}, {  1,   1}, { 12,  11}, { 10,  11},
    { 16,  11}, { 40,  33}, { 24,  11}, { 20,  11},
    { 32,  11}, { 80,  33}, { 18,  11}, { 15,  11},
    { 64,  11}, {160,  99}, {  4,   3}, {  3,   2},
    {  2,   1}
};

#ifdef DEBUG
#define SLICE_TYPE_NUM  10
static const char *slice_type_tbl[SLICE_TYPE_NUM] =
{
    "P",
    "B",
    "I",
    "SP",
    "SI",
    "P",
    "B",
    "I",
    "SP",
    "SI",
};

static const char *
get_slice_type_dscr(uint8_t type)
{
    if (type < SLICE_TYPE_NUM)
    {
        return slice_type_tbl[type];
    }

    return "Invalid";
}
#endif

/* exp_golomb leading zero bits */
static uint8_t eg_lzb_tbl[256] =
{
    8, 7, 6, 6, 5, 5, 5, 5,
    /* 0x0000 1XXX */
    4, 4, 4, 4, 4, 4, 4, 4,

    /* 0x0001 XXXX */
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,

    /* 0x001X XXXX */
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,

    /* 0x01XX XXXX */
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,

    /* 0x1XXX XXXX */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t trailing_bits_tbl[9] =
{ 0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80 };

/* read in ue coded value */
static uint32_t
read_ue(bbio_handle_t bs)
{
    uint32_t lzbs, peek_value, zero_bits;

    lzbs = 0;
    do
    {
        if (bs->is_more_byte(bs))
        {
            peek_value = src_peek_bits(bs, 8, 0);

            if (peek_value) break;
            src_skip_bits(bs, 8);
            lzbs += 8;
        }
        else
        {
            uint32_t bits_left = (uint32_t)src_following_bit_num(bs);
            peek_value = src_peek_bits(bs, bits_left, 0) << (8 - bits_left);
            break;
        }
    }
    while (TRUE);

    if (peek_value == (uint32_t)-1)
    {
        assert(0); /* this is a major fail */
        return 0;
    }

    zero_bits = eg_lzb_tbl[peek_value];
    src_skip_bits(bs, zero_bits);
    lzbs += zero_bits;

    return src_read_bits(bs, lzbs + 1) - 1;
}

uint32_t
src_read_ue(bbio_handle_t bs)
{
    return read_ue(bs);
}

/* read in us coded value */
static int32_t
read_se(bbio_handle_t bs)
{
    uint32_t codeNum;

    codeNum = read_ue(bs);
    if ((codeNum & 0x1))
    {
        return (int32_t)((codeNum + 1) >> 1);
    }

    return -(int32_t)(codeNum >> 1);
}

void
parser_avc_remove_0x03(uint8_t *dst, size_t *dstlen, const uint8_t *src, const size_t srclen)
{
    uint8_t *dst_sav = dst;
    const uint8_t *end = (src + srclen) - 2;

    while (src < end)
    {
        if (src[0] == 0x00 && src[1] == 0x00 && src[2] == 0x03)
        {
            *dst++ = 0x00;
            *dst++ = 0x00;

            src += 3;
            continue;
        }
        *dst++ = *src++;
    }

    end += 2;
    while (src < end)
    {
        *dst++ = *src++;
    }

    *dstlen = (int) (dst - dst_sav);
}

static void
scaling_list(uint32_t ix, bbio_handle_t bs)
{
    uint32_t sizeOfScalingList = ix < 6 ? 16 : 64;
    uint32_t last_Scaler = 8, next_Scaler = 8;
    uint32_t index;
    uint32_t deltaScaler;

    for (index = 0; index < sizeOfScalingList; index++)
    {
        if (next_Scaler != 0)
        {
            deltaScaler = read_se(bs);
            next_Scaler  = (last_Scaler + deltaScaler + 0x100) % 0x100;
        }
        if (next_Scaler != 0)
        {
            last_Scaler = next_Scaler;
        }

    }
}

static void
parse_hrd_parameters(sps_t *p_sps, bbio_handle_t bs)
{
    /* to make it simple:
     * (1) use nal if possible(vcl come after nal)
     * (2) keep [0] and [cpb_cnt_minus1]: lowest bit rate and highest delay or highest bit rate and lowest dealy
     */
    uint32_t cpb_cnt_minus1, br_scl, sz_scl, ix,temp2;
    uint8_t temp1 = 0;
    BOOL     save_cpb = p_sps->nal_hrd_parameters_present_flag ^ p_sps->vcl_hrd_parameters_present_flag;

    cpb_cnt_minus1 = read_ue(bs);
    DPRINTF(NULL, "       cpb_cnt_minus1: %u\n", cpb_cnt_minus1);
    if (save_cpb)
    {
        p_sps->cpb_cnt_minus1 = cpb_cnt_minus1;
    }

    br_scl = src_read_bits(bs,4);
    DPRINTF(NULL, "       bit_rate_scale: %u\n", br_scl);
    sz_scl = src_read_bits(bs,4);
    DPRINTF(NULL, "       cpb_size_scale: %u\n", sz_scl);
    for (ix = 0; ix <= cpb_cnt_minus1; ix++)
    {
        /* we only interested in the 1st and last */
        temp1 = (uint8_t)read_ue(bs);
        temp2 = (temp1+1)<<(6+br_scl);
        DPRINTF(NULL, "         bit_rate_value_minus1[%u]: %u(%ukbps)\n", ix, temp1, temp2/1000);
        if (ix == 0 && save_cpb)
        {
            p_sps->bit_rate_1st  = temp2;
        }
        if (ix == cpb_cnt_minus1 && save_cpb)
        {
            p_sps->bit_rate_last = temp2;
        }

        temp1 = (uint8_t)read_ue(bs);
        temp2 = (temp1+1)<<(4+sz_scl);
        DPRINTF(NULL, "         cpb_size_value_minus1[%u]: %u(%ukbits)\n", ix, temp1, temp2/1000);
        if (ix == 0 && save_cpb)
        {
            p_sps->cpb_size_1st  = temp2;
        }
        if (ix == cpb_cnt_minus1 && save_cpb)
        {
            p_sps->cpb_size_last = temp2;
        }

        temp1 = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "         cbr_flag[%u]: %u\n", ix, temp1);
    }
    DPRINTF(NULL, "         cpb_size_depth(last) in ms %" PRIu64 "\n", (1000*(uint64_t)p_sps->cpb_size_last)/p_sps->bit_rate_last);

    temp1 = (uint8_t)src_read_bits(bs,5);
    DPRINTF(NULL, "       initial_cpb_removal_delay_length_minus1: %u\n", temp1);
    if (save_cpb)
    {
        p_sps->initial_cpb_removal_delay_length_minus1 = temp1;
    }

    temp1 = (uint8_t)src_read_bits(bs,5);
    DPRINTF(NULL, "       cpb_removal_delay_length_minus1: %u\n", temp1);
    if (save_cpb)
    {
        p_sps->cpb_removal_delay_length_minus1 = temp1;
    }

    temp1 = (uint8_t)src_read_bits(bs,5);
    DPRINTF(NULL, "       dpb_output_delay_length_minus1: %u\n", temp1);
    if (save_cpb)
    {
        p_sps->dpb_output_delay_length_minus1 = temp1;
    }

    temp1 = (uint8_t)src_read_bits(bs,5);
    DPRINTF(NULL, "       time_offset_length: %u\n", temp1);
    if (save_cpb)
    {
        p_sps->time_offset_length = temp1;
    }
}

static void
parse_vui_parameters(sps_t *p_sps, bbio_handle_t bs)
{
    uint8_t temp;

    DPRINTF(NULL, "     VUI:\n");

    temp = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     aspect_ratio_info_present_flag: %u\n", temp);
    if (temp)
    {
        p_sps->aspect_ratio_idc = (uint8_t)src_read_bits(bs, 8);
        DPRINTF(NULL, "       aspect_ratio_idc:%u\n", p_sps->aspect_ratio_idc);
        if (p_sps->aspect_ratio_idc == 0xff)
        {
            /* extended_SAR */
            p_sps->sar_width  = (uint16_t)src_read_bits(bs,16);
            p_sps->sar_height = (uint16_t)src_read_bits(bs,16);
        }
        else if (p_sps->aspect_ratio_idc < 17)
        {
            p_sps->sar_width  = aspect_ratio_tbl[p_sps->aspect_ratio_idc][0];
            p_sps->sar_height = aspect_ratio_tbl[p_sps->aspect_ratio_idc][1];
        }
        DPRINTF(NULL, "       sar_width, sar_height: %u %u\n", p_sps->sar_width, p_sps->sar_height);
    }

    temp = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     overscan_info_present_flag: %u\n", temp);
    if (temp)
    {
        temp = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "       overscan_appropriate_flag: %u\n", temp);
        p_sps->overscan_info = 0x2 | temp;
    }

    p_sps->video_signal_info_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     video_signal_info_present_flag: %u\n", p_sps->video_signal_info_present_flag);
    if (p_sps->video_signal_info_present_flag)
    {
        p_sps->video_format = (uint8_t)src_read_bits(bs,3);
        DPRINTF(NULL, "       video_format: %u\n", p_sps->video_format);
        p_sps->video_full_range_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "       video_full_range_flag: %u\n", p_sps->video_full_range_flag);
        p_sps->colour_description_present_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "       colour_description_present_flag: %u\n", p_sps->colour_description_present_flag);
        if (p_sps->colour_description_present_flag)
        {
            p_sps->colour_primaries = (uint8_t)src_read_bits(bs,8);
            DPRINTF(NULL, "         colour_primaries: %u\n", p_sps->colour_primaries);
            p_sps->transfer_characteristics = (uint8_t)src_read_bits(bs,8);
            DPRINTF(NULL, "         transfer_characteristics: %u\n", p_sps->transfer_characteristics);
            p_sps->matrix_coefficients = (uint8_t)src_read_bits(bs,8);
            DPRINTF(NULL, "         matrix_coefficients: %u\n", p_sps->matrix_coefficients);
        }
    }

    p_sps->chroma_loc_info_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     chroma_loc_info_present_flag: %u\n", p_sps->chroma_loc_info_present_flag);
    if (p_sps->chroma_loc_info_present_flag)
    {
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       chroma_sample_loc_type_top_field: %u\n", temp);
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       chroma_sample_loc_type_bottom_field: %u\n", temp);
    }

    p_sps->timing_info_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     timing_info_present_flag: %u\n", p_sps->timing_info_present_flag);
    if (p_sps->timing_info_present_flag)
    {
        p_sps->num_units_in_tick = src_read_bits(bs, 32);
        DPRINTF(NULL, "       num_units_in_tick: %u\n", p_sps->num_units_in_tick);
        p_sps->time_scale = src_read_bits(bs, 32);
        DPRINTF(NULL, "       time_scale: %u\n", p_sps->time_scale);
        p_sps->fixed_frame_rate_flag = src_read_bit(bs);
        DPRINTF(NULL, "       fixed_frame_rate_flag: %u\n", p_sps->fixed_frame_rate_flag);
        /* if values don't make sense, then just ignore them  - we may have run off the end of the SPS */
        if (p_sps->num_units_in_tick == 0 || p_sps->time_scale == 0)
        {
            p_sps->timing_info_present_flag = 0;
        }
    }

    p_sps->nal_hrd_parameters_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     nal_hrd_parameters_present_flag: %u\n", p_sps->nal_hrd_parameters_present_flag);
    if (p_sps->nal_hrd_parameters_present_flag)
    {
        parse_hrd_parameters(p_sps, bs);
    }

    p_sps->vcl_hrd_parameters_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     vcl_hrd_parameters_present_flag: %u\n", p_sps->vcl_hrd_parameters_present_flag);
    if (p_sps->vcl_hrd_parameters_present_flag)
    {
        parse_hrd_parameters(p_sps, bs);
    }

    if (p_sps->nal_hrd_parameters_present_flag || p_sps->vcl_hrd_parameters_present_flag)
    {
        p_sps->low_delay_hrd_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "       low_delay_hrd_flag: %u\n", p_sps->low_delay_hrd_flag);
    }

    p_sps->pic_struct_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     pic_struct_present_flag: %u\n", p_sps->pic_struct_present_flag);

    p_sps->bitstream_restriction_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "     bitstream_restriction_flag: %u\n", p_sps->bitstream_restriction_flag);
    if (p_sps->bitstream_restriction_flag)
    {
        temp = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "       motion_vectors_over_pic_boundaries_flag: %u\n", temp);
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       max_bytes_per_pic_denom: %u\n", temp);
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       max_bits_per_mb_denom: %u\n", temp);
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       log2_max_mv_length_horizontal: %u\n", temp);
        temp = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       log2_max_mv_length_vertical: %u\n", temp);
        p_sps->num_reorder_frames = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       num_reorder_frames: %u\n", p_sps->num_reorder_frames);
        p_sps->max_dec_frame_buffering = (uint8_t)read_ue(bs);
        DPRINTF(NULL, "       max_dec_frame_buffering: %u\n", p_sps->max_dec_frame_buffering);
    }
}

#define SUPPORTED_LEVEL 53
static uint32_t MaxBRTbl[SUPPORTED_LEVEL+1], MaxCPBTbl[SUPPORTED_LEVEL+1];
static uint16_t cpbBrNalfactorTbl[245];

static int
get_vui_params(sps_t *p_sps, bbio_handle_t bs)
{
    uint32_t temp;

    /* default to 0 ts flags: VUI may set them if any */
    p_sps->sar_width                       = p_sps->sar_height = 0; /* 0: not specified */
    p_sps->timing_info_present_flag        = 0;
    p_sps->nal_hrd_parameters_present_flag = 0;
    p_sps->vcl_hrd_parameters_present_flag = 0;
    p_sps->low_delay_hrd_flag              = 0;
    p_sps->pic_struct_present_flag         = 0;
    p_sps->bitstream_restriction_flag      = 0;

    p_sps->colour_primaries         = 2;
    p_sps->matrix_coefficients      = 2;
    p_sps->transfer_characteristics = 2;

    temp = src_read_bit(bs);
    DPRINTF(NULL, "   vui_parameters_present_flag: %u\n", temp);
    if (temp)
    {
        parse_vui_parameters(p_sps, bs);
    }

    p_sps->NalHrdBpPresentFlag     = p_sps->nal_hrd_parameters_present_flag;
    p_sps->VclHrdBpPresentFlag     = p_sps->vcl_hrd_parameters_present_flag;
    p_sps->CpbDpbDelaysPresentFlag = p_sps->nal_hrd_parameters_present_flag | p_sps->vcl_hrd_parameters_present_flag;

    if (p_sps->NalHrdBpPresentFlag == 0 || p_sps->bit_rate_last < 100000 || p_sps->cpb_size_last < 100000)
    {
        /* no nal hrd: set up default nal hrd */

        /* we only interested in the 1st and last */
        if (((p_sps->compatibility & 0x10) && p_sps->level_idc == 11) || p_sps->level_idc == 9)
        {
            /* level 1b */
            p_sps->bit_rate_1st = cpbBrNalfactorTbl[p_sps->profile_idc] * 128;
            p_sps->cpb_size_1st = cpbBrNalfactorTbl[p_sps->profile_idc] * 350;
        }
        else
        {
            p_sps->bit_rate_1st = cpbBrNalfactorTbl[p_sps->profile_idc] * MaxBRTbl[p_sps->level_idc];
            p_sps->cpb_size_1st = cpbBrNalfactorTbl[p_sps->profile_idc] * MaxCPBTbl[p_sps->level_idc];
            if (p_sps->profile_idc == 128 || p_sps->profile_idc == 134)
            {
                /* 15Mbps case shall signalled by VUI */
                if (p_sps->level_idc == 40)
                {
                    p_sps->bit_rate_1st = 1200 * MaxBRTbl[p_sps->level_idc];
                }
                else if (p_sps->level_idc == 41)
                {
                    p_sps->bit_rate_1st = 800 * MaxBRTbl[p_sps->level_idc];
                }
                else
                {
                    msglog(NULL, MSGLOG_ERR, "MVHD and DB3d profile but level is not right");
                    return EMA_MP4_MUXED_ES_ERR;
                }
            }
        }

        p_sps->bit_rate_last = p_sps->bit_rate_1st;
        p_sps->cpb_size_last = p_sps->cpb_size_1st;

        DPRINTF(NULL, "     Use default for T-STD:\n");
        DPRINTF(NULL, "       bit_rate_value: %ukbps\n",  p_sps->bit_rate_last/1000);
        DPRINTF(NULL, "       cpb_size_value: %ukbits\n", p_sps->cpb_size_last/1000);
        DPRINTF(NULL, "       cpb_size_depth: %" PRIu64 "ms\n", (1000*(uint64_t)p_sps->cpb_size_last)/p_sps->bit_rate_last);
    }

    if (p_sps->CpbDpbDelaysPresentFlag == 0)
    {
        /* no nal or vcl */
        p_sps->cpb_cnt_minus1 = 0;

        p_sps->initial_cpb_removal_delay_length_minus1 = 23;
        p_sps->cpb_removal_delay_length_minus1         = 23;
        p_sps->dpb_output_delay_length_minus1          = 23;

        p_sps->time_offset_length = 24;
    }

    if (p_sps->bitstream_restriction_flag == 0)
    {
        p_sps->num_reorder_frames      = 16; /* max for now */
        p_sps->max_dec_frame_buffering = 16; /* max for now */
    }

    return EMA_MP4_MUXED_OK;
}


int
parse_sequence_parameter_set(avc_decode_t *dec, bbio_handle_t bs)
{
    sps_t    *p_sps;
    uint32_t  temp1, temp2, temp3, temp4;
    uint32_t  PicWidthInMbs, PicHeightInMapUnits;
    int       ret = EMA_MP4_MUXED_OK;

    temp1 = src_read_u8(bs);
    DPRINTF(NULL, "   profile_idc: %u\n", temp1);
    if (temp1 > 224 || cpbBrNalfactorTbl[temp1] == 0)
    {
        msglog(NULL, MSGLOG_ERR, "can't handle the profile\n");
        return EMA_MP4_MUXED_ES_ERR;
    }

    temp2 = src_read_u8(bs);
    DPRINTF(NULL, "   constaint_set[0-4]_flag: %1u, %1u, %1u, %1u, %1u\n",
            (temp2>>7)&0x1, (temp2>>6)&0x1, (temp2>>5)&0x1, (temp2>>4)&0x1, (temp2>>3)&0x1);

    temp3 = src_read_u8(bs);
    DPRINTF(NULL, "   level_idc: %u\n", temp3);
    if (temp3 > SUPPORTED_LEVEL || MaxBRTbl[temp3] == 0)
    {
        msglog(NULL, MSGLOG_ERR, "can't handle the level\n");
        return EMA_MP4_MUXED_ES_ERR;
    }

    temp4 = read_ue(bs);
    DPRINTF(NULL, "   seq_parameter_set_id: %u\n", temp4);
    if (temp4 > 31)
    {
        msglog(NULL, MSGLOG_ERR, "seq_parameter_set_id in sps wrong\n");
        if (dec->sps[0].isDefined)
        {
            return EMA_MP4_MUXED_ES_ERR;
        }
        msglog(NULL, MSGLOG_ERR, "Assume seq_parameter_set_id = 0\n");
        temp4 = 0;
    }
    dec->sps_id = (uint8_t)temp4;
    p_sps       = dec->sps + temp4;
    dec->active_sps = p_sps;

    p_sps->profile_idc   = (uint8_t)temp1;
    p_sps->compatibility = (uint8_t)temp2;
    p_sps->level_idc     = (uint8_t)temp3;
    p_sps->sps_id        = (uint8_t)temp4;

    /* default value */
    p_sps->chroma_format_idc                    = 1;
    p_sps->separate_colour_plane_flag           = 0;
    p_sps->bit_depth_luma_minus8                = 0;
    p_sps->bit_depth_chroma_minus8              = 0;
    p_sps->qpprime_y_zero_transform_bypass_flag = 0;
    p_sps->seq_scaling_matrix_present_flag      = 0;
    /** FRext stuff */
    if (p_sps->profile_idc == 100 || p_sps->profile_idc == 110 ||
        p_sps->profile_idc == 122 || p_sps->profile_idc == 244 || p_sps->profile_idc ==  44 ||
        p_sps->profile_idc ==  83 || p_sps->profile_idc ==  86 || p_sps->profile_idc == 118 ||
        p_sps->profile_idc == 128 || p_sps->profile_idc == 134)
    {
        p_sps->chroma_format_idc = read_ue(bs);
        DPRINTF(NULL, "   chroma_format_idc: %u\n", p_sps->chroma_format_idc);

        if (p_sps->chroma_format_idc == 3)
        {
            p_sps->separate_colour_plane_flag = (uint8_t)src_read_bit(bs);
            DPRINTF(NULL, "    separate_colour_plane_flag: %u\n", p_sps->separate_colour_plane_flag);
        }
        p_sps->bit_depth_luma_minus8 = read_ue(bs);
        DPRINTF(NULL, "   bit_depth_luma_minus8: %u\n", p_sps->bit_depth_luma_minus8);
        p_sps->bit_depth_chroma_minus8 = read_ue(bs);
        DPRINTF(NULL, "   bit_depth_chroma_minus8: %u\n", p_sps->bit_depth_chroma_minus8);
        p_sps->qpprime_y_zero_transform_bypass_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "   qpprime_y_zero_transform_bypass_flag: %u\n", p_sps->qpprime_y_zero_transform_bypass_flag);
        p_sps->seq_scaling_matrix_present_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "   seq_scaling_matrix_present_flag: %u\n", p_sps->seq_scaling_matrix_present_flag);
        if (p_sps->seq_scaling_matrix_present_flag)
        {
            temp1 = (p_sps->chroma_format_idc != 3) ? 8 : 12;
            for (temp2 = 0; temp2 < temp1; temp2++)
            {
                temp3 = src_read_bit(bs);
                DPRINTF(NULL, "   seq_scaling_list[%u]_present_flag: %u\n", temp2, temp3);
                if (temp3)
                {
                    scaling_list(temp2, bs);
                }
            }
        }
    }

    p_sps->log2_max_frame_num_minus4 = read_ue(bs);
    DPRINTF(NULL, "   log2_max_frame_num_minus4: %u\n", p_sps->log2_max_frame_num_minus4);
    p_sps->max_frame_num = 1 << (p_sps->log2_max_frame_num_minus4 + 4);

    p_sps->pic_order_cnt_type = read_ue(bs);
    DPRINTF(NULL, "   pic_order_cnt_type: %u\n", p_sps->pic_order_cnt_type);
    if (p_sps->pic_order_cnt_type == 0)
    {
        p_sps->log2_max_pic_order_cnt_lsb_minus4 = read_ue(bs);
        DPRINTF(NULL, "     log2_max_pic_order_cnt_lsb_minus4: %u\n", p_sps->log2_max_pic_order_cnt_lsb_minus4);
        p_sps->max_poc_lsb = 1 << (p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
    }
    else if (p_sps->pic_order_cnt_type == 1)
    {
        p_sps->delta_pic_order_always_zero_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "     delta_pic_order_always_zero_flag: %u\n", p_sps->delta_pic_order_always_zero_flag);
        p_sps->offset_for_non_ref_pic = read_se(bs);
        DPRINTF(NULL, "     offset_for_non_ref_pic: %d\n", p_sps->offset_for_non_ref_pic);
        p_sps->offset_for_top_to_bottom_field = read_se(bs);
        DPRINTF(NULL, "     offset_for_top_to_bottom_field: %d\n", p_sps->offset_for_top_to_bottom_field);
        p_sps->num_ref_frames_in_pic_order_cnt_cycle = (uint8_t)read_ue(bs);
        p_sps->expected_delta_per_poc_cycle = 0;
        for (temp1 = 0; temp1 < p_sps->num_ref_frames_in_pic_order_cnt_cycle; temp1++)
        {
            p_sps->offset_for_ref_frame[temp1] = (uint8_t)read_se(bs);
            DPRINTF(NULL, "       offset_for_ref_frame[%u]: %d\n", temp1, p_sps->offset_for_ref_frame[temp1]);

            p_sps->expected_delta_per_poc_cycle += p_sps->offset_for_ref_frame[temp1];
        }
    }

    p_sps->max_num_ref_frames = (uint8_t)read_ue(bs);
    DPRINTF(NULL, "   max_num_ref_frames: %u\n", p_sps->max_num_ref_frames);
    p_sps->gaps_in_frame_num_value_allowed_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "   gaps_in_frame_num_value_allowed_flag: %u\n", p_sps->gaps_in_frame_num_value_allowed_flag);

    PicWidthInMbs    = read_ue(bs) + 1;
    p_sps->pic_width = PicWidthInMbs * 16;
    DPRINTF(NULL, "   pic_width_in_mbs_minus1:  %u(%u)\n", PicWidthInMbs - 1, p_sps->pic_width);

    PicHeightInMapUnits        = read_ue(bs) + 1;
    p_sps->frame_mbs_only_flag = (uint8_t)src_read_bit(bs);
    p_sps->pic_height          = (2 - p_sps->frame_mbs_only_flag) * PicHeightInMapUnits * 16;
    DPRINTF(NULL, "   pic_height_in_map_minus1: %u(%u)\n", PicHeightInMapUnits - 1, p_sps->pic_height);
    DPRINTF(NULL, "   frame_mbs_only_flag: %u\n", p_sps->frame_mbs_only_flag);

    if (!p_sps->frame_mbs_only_flag)
    {
        temp1 = src_read_bit(bs);
        DPRINTF(NULL, "     mb_adaptive_frame_field_flag: %u\n", temp1);
    }
    temp1 = src_read_bit(bs);
    DPRINTF(NULL, "   direct_8x8_inference_flag: %u\n", temp1);

    p_sps->pic_width_out       = p_sps->pic_width;
    p_sps->pic_height_out      = p_sps->pic_height;
    p_sps->frame_cropping_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "   frame_cropping_flag: %u\n", p_sps->frame_cropping_flag);
    if (p_sps->frame_cropping_flag)
    {
        p_sps->frame_crop_left_offset = read_ue(bs);
        DPRINTF(NULL, "     frame_crop_left_offset: %u\n",   p_sps->frame_crop_left_offset);
        p_sps->frame_crop_right_offset = read_ue(bs);
        DPRINTF(NULL, "     frame_crop_right_offset: %u\n",  p_sps->frame_crop_right_offset);
        p_sps->frame_crop_top_offset = read_ue(bs);
        DPRINTF(NULL, "     frame_crop_top_offset: %u\n",    p_sps->frame_crop_top_offset);
        p_sps->frame_crop_bottom_offset = read_ue(bs);
        DPRINTF(NULL, "     frame_crop_bottom_offset: %u\n", p_sps->frame_crop_bottom_offset);

        /* get the output size */
        temp1 = (p_sps->chroma_format_idc == 1 || p_sps->chroma_format_idc == 2) ? 2 : 1; /* cropUnitX 2: 4:2:0, 4:2:2 */
        temp2 = (p_sps->chroma_format_idc == 1) ? 2 : 1; /* cropUnitY 2: 4:2:0 */
        temp2 *= (2 - p_sps->frame_mbs_only_flag);
        p_sps->pic_width_out  -= temp1*(p_sps->frame_crop_left_offset + p_sps->frame_crop_right_offset);
        p_sps->pic_height_out -= temp2*(p_sps->frame_crop_top_offset  + p_sps->frame_crop_bottom_offset);
    }
    DPRINTF(NULL, "   display pic size: %u by %u\n", p_sps->pic_width_out, p_sps->pic_height_out);

    /* vui and default value */
    ret = get_vui_params(p_sps,  bs);
    if (ret != EMA_MP4_MUXED_OK)
    {
        return ret;
    }

    /* sps_ext, if any, is effectively part of sps */
    p_sps->aux_format_id = 0;

    p_sps->isDefined = 1;

    if (dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM)
    {
        dec->sps_id_enh     = dec->sps_id;
        dec->active_sps_enh = p_sps;
    }

    return EMA_MP4_MUXED_OK;
}

static void parse_sequence_parameter_set_ext(avc_decode_t *dec, bbio_handle_t bs)
{
    uint32_t  temp;
    sps_t    *p_sps;

    temp = read_ue(bs);
    DPRINTF(NULL, "   seq_parameter_set_id: %u\n", temp);
    assert(dec->sps_id == temp);
    p_sps = dec->sps + temp;

    p_sps->aux_format_id = (uint8_t)read_ue(bs);
    DPRINTF(NULL, "   aux format idc: %u\n", p_sps->aux_format_id);
    if (p_sps->aux_format_id != 0)
    {
        temp = read_ue(bs);
        DPRINTF(NULL, "    bit depth aux minus8:%u\n",     temp);
        temp = src_read_bit(bs);
        DPRINTF(NULL, "    alpha incr flag:%u\n",          temp);
        temp = src_read_bits(bs,temp + 9);
        DPRINTF(NULL, "    alpha opaque value: %u\n",      temp);
        temp = src_read_bits(bs,temp + 9);
        DPRINTF(NULL, "    alpha transparent value: %u\n", temp);
    }
    temp = src_read_bit(bs);
    DPRINTF(NULL, "   additional extension flag: %u\n", temp);
}


static uint32_t
ceil_log2(uint32_t val)
{
    uint32_t l = 0, cval = 1;

    while (cval < val && l < 32)
    {
        cval <<= 1;
        l++;
    }
    return l;
}

static int
parse_pic_parameter_set(avc_decode_t *dec, bbio_handle_t bs)
{
    pps_t    *p_pps;
    uint32_t  num_slice_groups, temp, temp2, iGroup;
    uint8_t   transform_8x8_mode_flag;

    temp = read_ue(bs);
    DPRINTF(NULL, "   pic_parameter_set_id: %u\n", temp);
    dec->pps_id   = (uint8_t)temp;
    p_pps         = dec->pps + temp;
    p_pps->pps_id = (uint8_t)temp;

    temp = read_ue(bs);
    DPRINTF(NULL, "   using seq_parameter_set_id: %u\n", temp);

    if (temp > 31)
    {
        msglog(NULL, MSGLOG_ERR, "seq_parameter_set_id in pps wrong\n");
        return EMA_MP4_MUXED_ES_ERR;
    }
    p_pps->sps_id   = (uint8_t)temp;
    dec->active_sps = dec->sps + temp;

    temp = src_read_bit(bs);
    DPRINTF(NULL, "   entropy_coding_mode_flag: %u\n", temp);
    p_pps->bottom_field_pic_order_in_frame_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "   bottom_field_pic_order_in_frame_present_flag: %u\n",
            p_pps->bottom_field_pic_order_in_frame_present_flag);

    num_slice_groups = read_ue(bs);
    DPRINTF(NULL, "   num_slice_groups_minus1: %u\n", num_slice_groups);
    if (num_slice_groups > 0)
    {
        temp = read_ue(bs);
        DPRINTF(NULL, "    slice_group_map_type: %u\n", temp);
        if (temp == 0)
        {
            for (iGroup = 0; iGroup <= num_slice_groups; iGroup++)
            {
                temp2 = read_ue(bs);
                DPRINTF(NULL, "     run_length_minus1[%u]: %u\n", iGroup, temp2);
            }
        }
        else if (temp == 2)
        {
            for (iGroup = 0; iGroup < num_slice_groups; iGroup++)
            {
                temp2 = read_ue(bs);
                DPRINTF(NULL, "     top_left[%u]: %u\n", iGroup, temp2);
                temp2 = read_ue(bs);
                DPRINTF(NULL, "     bottom_right[%u]: %u\n", iGroup, temp2);
            }
        }
        else if (temp == 3 || temp == 4 || temp == 5)
        {
            temp2 = src_read_bit(bs);
            DPRINTF(NULL, "     slice_group_change_direction_flag: %u\n", temp2);
            temp2 = read_ue(bs);
            DPRINTF(NULL, "     slice_group_change_rate_minus1: %u\n", temp2);
        }
        else if (temp == 6)
        {
            uint32_t bits;

            temp2 = read_ue(bs);
            DPRINTF(NULL, "     pic_size_in_map_units_minus1: %u\n", temp2);
            bits = ceil_log2(num_slice_groups + 1);
            DPRINTF(NULL, "     bits - %u\n", bits);
            for (iGroup = 0; iGroup <= temp; iGroup++)
            {
                temp2 = src_read_bits(bs,bits);
                DPRINTF(NULL, "      slice_group_id[%u]: %u\n", iGroup, temp2);
            }
        }
    }
    temp = read_ue(bs);
    DPRINTF(NULL, "   num_ref_idx_l0_active_minus1: %u\n", temp);
    temp = read_ue(bs);
    DPRINTF(NULL, "   num_ref_idx_l1_active_minus1: %u\n", temp);
    temp = src_read_bit(bs);
    DPRINTF(NULL, "   weighted_pred_flag: %u\n", temp);
    temp = src_read_bits(bs,2);
    DPRINTF(NULL, "   weighted_bipred_idc: %u\n", temp);
    temp = read_se(bs);
    DPRINTF(NULL, "   pic_init_qp_minus26: %d\n", temp);
    temp = read_se(bs);
    DPRINTF(NULL, "   pic_init_qs_minus26: %d\n", temp);
    temp = read_se(bs);
    DPRINTF(NULL, "   chroma_qp_index_offset: %d\n", temp);
    temp = src_read_bit(bs);
    DPRINTF(NULL, "   deblocking_filter_control_present_flag: %u\n", temp);
    temp = src_read_bit(bs);
    DPRINTF(NULL, "   constrained_intra_pred_flag: %u\n", temp);

    p_pps->redundant_pic_cnt_present_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "   redundant pic cnt present flag: %u\n", p_pps->redundant_pic_cnt_present_flag);

    if (!bs->is_more_byte2(bs))
    {
        uint32_t bits = (uint32_t)src_following_bit_num(bs);

        if (bits == 0)
        {
            /* shalln't come here, shall have trailing bits */
            p_pps->isDefined = 1;
            return EMA_MP4_MUXED_EXIT;
        }
        if (bits <= 8)
        {
            uint8_t trail_check = (uint8_t)src_peek_bits(bs, bits, 0);
            if (trail_check == trailing_bits_tbl[bits])
            {
                /* trailing only */
                p_pps->isDefined = 1;
                return EMA_MP4_MUXED_EXIT;
            }
        }
    }

    /* we have the extensions */
    transform_8x8_mode_flag = (uint8_t)src_read_bit(bs);
    DPRINTF(NULL, "   transform_8x8_mode_flag: %u\n", transform_8x8_mode_flag);
    temp = src_read_bit(bs);
    DPRINTF(NULL, "   pic_scaling_matrix_present_flag: %u\n", temp);
    if (temp)
    {
        uint32_t ix, max_count = 6 + (2 * transform_8x8_mode_flag);
        for (ix = 0; ix < max_count; ix++)
        {
            temp2 = src_read_bit(bs);
            DPRINTF(NULL, "     Pic Scaling list[%u] Present Flag: %u\n", ix, temp2);
            if (temp2)
            {
                scaling_list(ix, bs);
            }
        }
    }
    temp = read_se(bs);
    DPRINTF(NULL, "   second_chroma_qp_index_offset: %d\n", temp);

    p_pps->isDefined = 1;

    return EMA_MP4_MUXED_OK;
}

/* called right after first vcl of au is updated */
static void
parser_avc_compute_poc(avc_decode_t *dec)
{
    int            field_poc[2] = {0, 0};
    avc_picType_t  pic_type;
    sps_t         *p_active_sps;
    avc_slice_t       *p_slice;

    p_slice      = dec->slice;
    p_active_sps = dec->active_sps;

    /* picture type */
    if (p_active_sps->frame_mbs_only_flag || !p_slice->field_pic_flag)
    {
        pic_type = AVC_PIC_TYPE_FRAME;
    }
    else if (p_slice->bottom_field_flag)
    {
        pic_type = AVC_PIC_TYPE_FIELD_BOTTOM;
    }
    else
    {
        pic_type = AVC_PIC_TYPE_FIELD_TOP;
    }

    dec->picType = pic_type;

    dec->pic_dec_order_cnt++;
    if (dec->nal_unit_type == NAL_TYPE_IDR_SLICE)
    {
        dec->pic_dec_order_cnt = 0;
    }

    if (p_active_sps->pic_order_cnt_type == 0)
    {
        /** IDR reset */
        if (dec->nal_unit_type == NAL_TYPE_IDR_SLICE)
        {
            dec->pic_order_cnt_lsb_prev = 0;
            dec->pic_order_cnt_msb_prev = 0;
        }

        /** poc cal */
        if (p_slice->pic_order_cnt_lsb < dec->pic_order_cnt_lsb_prev &&
            dec->pic_order_cnt_lsb_prev - p_slice->pic_order_cnt_lsb >= p_active_sps->max_poc_lsb / 2)
        {
            dec->pic_order_cnt_msb = dec->pic_order_cnt_msb_prev + p_active_sps->max_poc_lsb;
        }
        else if (p_slice->pic_order_cnt_lsb > dec->pic_order_cnt_lsb_prev &&
                 p_slice->pic_order_cnt_lsb - dec->pic_order_cnt_lsb_prev > p_active_sps->max_poc_lsb / 2)
        {
            dec->pic_order_cnt_msb = dec->pic_order_cnt_msb_prev - p_active_sps->max_poc_lsb;
        }
        else
        {
            dec->pic_order_cnt_msb = dec->pic_order_cnt_msb_prev;
        }

        field_poc[0] = dec->pic_order_cnt_msb + p_slice->pic_order_cnt_lsb;
        field_poc[1] = field_poc[0];
        if (pic_type == AVC_PIC_TYPE_FRAME)
        {
            field_poc[1] += p_slice->delta_pic_order_cnt_bottom;
        }

        /** update for following pic poc cal */
        if (dec->nal_ref_idc != 0)
        {
            dec->pic_order_cnt_lsb_prev = p_slice->pic_order_cnt_lsb;
            dec->pic_order_cnt_msb_prev = dec->pic_order_cnt_msb;
        }

    }
    else
    {
        /** IDR reset, poc cal */
        if (dec->nal_unit_type == NAL_TYPE_IDR_SLICE)
        {
            dec->frame_num_offset = 0;
        }
        /** poc cal */
        else if (p_slice->frame_num < dec->frame_num_prev)
        {
            dec->frame_num_offset = dec->frame_num_offset_prev + p_active_sps->max_frame_num;
        }
        else
        {
            dec->frame_num_offset = dec->frame_num_offset_prev;
        }

        if (p_active_sps->pic_order_cnt_type == 1)
        {
            int abs_frame_num, expected_poc;

            if (p_active_sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
            {
                abs_frame_num = dec->frame_num_offset + p_slice->frame_num;
            }
            else
            {
                abs_frame_num = 0;
            }

            if (dec->nal_ref_idc == 0 && abs_frame_num > 0)
            {
                abs_frame_num--;
            }

            if (abs_frame_num > 0)
            {
                int i;
                int poc_cycle_cnt          = ( abs_frame_num - 1 ) / p_active_sps->num_ref_frames_in_pic_order_cnt_cycle;
                int frame_num_in_poc_cycle = ( abs_frame_num - 1 ) % p_active_sps->num_ref_frames_in_pic_order_cnt_cycle;

                expected_poc = poc_cycle_cnt * p_active_sps->expected_delta_per_poc_cycle;
                for (i = 0; i <= frame_num_in_poc_cycle; i++)
                {
                    expected_poc += p_active_sps->offset_for_ref_frame[i];
                }
            }
            else
            {
                expected_poc = 0;
            }

            if (dec->nal_ref_idc == 0)
            {
                expected_poc += p_active_sps->offset_for_non_ref_pic;
            }

            field_poc[0] = expected_poc + p_slice->delta_pic_order_cnt[0];
            field_poc[1] = field_poc[0] + p_active_sps->offset_for_top_to_bottom_field;

            if (pic_type == AVC_PIC_TYPE_FRAME)
            {
                field_poc[1] += p_slice->delta_pic_order_cnt[1];
            }
        }
        else if (p_active_sps->pic_order_cnt_type == 2)
        {
            int tmp_poc;

            if (dec->nal_unit_type == NAL_TYPE_IDR_SLICE)
            {
                tmp_poc = 0;
            }
            else
            {
                tmp_poc = (dec->frame_num_offset + p_slice->frame_num)<<1;
                if (dec->nal_ref_idc == 0)
                {
                    tmp_poc--;
                }
            }
            field_poc[0] = tmp_poc;
            field_poc[1] = tmp_poc;
        }

        /** update for following pic poc cal */
        dec->frame_num_prev        = p_slice->frame_num;
        dec->frame_num_offset_prev = dec->frame_num_offset;
    }

    if (pic_type == AVC_PIC_TYPE_FRAME)
    {
        dec->pic_order_cnt = MIN2(field_poc[0], field_poc[1] );
    }
    else if (pic_type == AVC_PIC_TYPE_FIELD_TOP)
    {
        dec->pic_order_cnt = field_poc[0];
    }
    else
    {
        dec->pic_order_cnt = field_poc[1];
    }

    DPRINTF(NULL, "   pic_order_cnt: %u\n", dec->pic_order_cnt);
}

/* no partitioning only and up to delta_pic_order_cnt_* */
static int
parse_slice(avc_decode_t *dec, bbio_handle_t bs)
{
    int      temp;
    avc_slice_t *p_slice_curr;
    pps_t   *p_pps;
    sps_t   *p_sps;

    p_slice_curr                = dec->slice_next;     /* the slice_next is the current one */
    p_slice_curr->nal_unit_type = dec->nal_unit_type;
    p_slice_curr->nal_ref_idc   = dec->nal_ref_idc;

    temp = read_ue(bs);
    DPRINTF(NULL, "   first_mb_in_slice: %u\n", temp);
    p_slice_curr->slice_type = read_ue(bs);
    DPRINTF(NULL, "   slice_type: %u(%s)\n", p_slice_curr->slice_type, get_slice_type_dscr((uint8_t)p_slice_curr->slice_type));
    p_slice_curr->pps_id = (uint8_t)read_ue(bs);
    DPRINTF(NULL, "   active pic_parameter_set_id: %u\n", p_slice_curr->pps_id);

    p_pps = dec->pps + p_slice_curr->pps_id;
    if (p_pps->isDefined == 0)
    {
        msglog(NULL, MSGLOG_ERR, "pic_parameter_set_id in slice wrong. pps not defined yet\n");
        if (dec->pps[0].isDefined == 0)
        {
            return EMA_MP4_MUXED_NO_CONFIG_ERR;
        }
        msglog(NULL, MSGLOG_ERR, "Assume pic_parameter_set_id = 0\n");
        p_slice_curr->pps_id = 0;
        p_pps                = dec->pps;
    }

    p_sps                   = dec->sps + p_pps->sps_id;
    p_slice_curr->frame_num = src_read_bits(bs, p_sps->log2_max_frame_num_minus4 + 4);
    DPRINTF(NULL, "   frame_num: %u (%u bits)\n", p_slice_curr->frame_num, p_sps->log2_max_frame_num_minus4 + 4);

    p_slice_curr->field_pic_flag    = 0;
    p_slice_curr->bottom_field_flag = 0;
    if (!p_sps->frame_mbs_only_flag)
    {
        p_slice_curr->field_pic_flag = (uint8_t)src_read_bit(bs);
        DPRINTF(NULL, "   field_pic_flag: %u\n", p_slice_curr->field_pic_flag);
        if (p_slice_curr->field_pic_flag)
        {
            p_slice_curr->bottom_field_flag = (uint8_t)src_read_bit(bs);
            DPRINTF(NULL, "    bottom_field_flag: %u\n", p_slice_curr->bottom_field_flag);
        }
    }
    if (p_slice_curr->nal_unit_type == NAL_TYPE_IDR_SLICE)
    {
        p_slice_curr->idr_pic_id = read_ue(bs);
        DPRINTF(NULL, "   idr_pic_id: %u\n", p_slice_curr->idr_pic_id);
    }

    if (p_sps->pic_order_cnt_type == 0)
    {
        p_slice_curr->delta_pic_order_cnt_bottom = 0;

        p_slice_curr->pic_order_cnt_lsb = src_read_bits(bs, p_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
        DPRINTF(NULL, "   pic_order_cnt_lsb: %u\n", p_slice_curr->pic_order_cnt_lsb);
        if (p_pps->bottom_field_pic_order_in_frame_present_flag && !p_slice_curr->field_pic_flag)
        {
            p_slice_curr->delta_pic_order_cnt_bottom = read_se(bs);
            DPRINTF(NULL, "   delta_pic_order_cnt_bottom: %d\n", p_slice_curr->delta_pic_order_cnt_bottom);
        }
    }
    else if (p_sps->pic_order_cnt_type == 1)
    {
        p_slice_curr->delta_pic_order_cnt[0] = 0;
        p_slice_curr->delta_pic_order_cnt[1] = 0;

        if (!p_sps->delta_pic_order_always_zero_flag)
        {
            p_slice_curr->delta_pic_order_cnt[0] = read_se(bs);
            DPRINTF(NULL, "   delta_pic_order_cnt[0]: %d\n", p_slice_curr->delta_pic_order_cnt[0]);
        }
        if (p_pps->bottom_field_pic_order_in_frame_present_flag && !p_slice_curr->field_pic_flag)
        {
            p_slice_curr->delta_pic_order_cnt[1] = read_se(bs);
            DPRINTF(NULL, "   delta_pic_order_cnt[1]: %d\n", p_slice_curr->delta_pic_order_cnt[1]);
        }
    }

    p_slice_curr->redundant_pic_cnt = 0;
    if (p_pps->redundant_pic_cnt_present_flag)
    {
        p_slice_curr->redundant_pic_cnt = read_ue(bs);
        DPRINTF(NULL, "     redundant_pic_cnt: %u\n", p_slice_curr->redundant_pic_cnt);
    }
    /* Mark whether there is redundancy in the sample based on redundancy in this slice */
    if (dec->sample_has_redundancy == FALSE)
    {
        dec->sample_has_redundancy =
            (p_pps->redundant_pic_cnt_present_flag && p_slice_curr->redundant_pic_cnt > 0) ? TRUE : FALSE;
    }

    return EMA_MP4_MUXED_OK;
#ifndef DEBUG
    (void)temp;  /* avoid compiler warning */
#endif
}

/* assuming no aux and redundent pic */
static BOOL
is_first_slice(const avc_decode_t *dec)
{
    const avc_slice_t *p_slice, *p_slice_next;
    const pps_t   *p_pps;
    const sps_t   *p_sps;

    p_slice      = dec->slice;
    p_slice_next = dec->slice_next;

    if (p_slice_next->redundant_pic_cnt)
    {
        return FALSE; /* redundant pic make no diffrence */
    }

    if (p_slice_next->frame_num != p_slice->frame_num)
    {
        return TRUE;
    }
    if (p_slice_next->pps_id != p_slice->pps_id)
    {
        return  TRUE;
    }
    if (p_slice_next->field_pic_flag != p_slice->field_pic_flag)
    {
        return TRUE;
    }
    /* come here p_slice_next->field_pic_flag == p_slice->field_pic_flag */
    if (p_slice_next->field_pic_flag && p_slice_next->bottom_field_flag != p_slice->bottom_field_flag)
    {
        return TRUE;
    }
    if (p_slice_next->nal_ref_idc != p_slice->nal_ref_idc &&
        (p_slice_next->nal_ref_idc == 0 || p_slice->nal_ref_idc == 0))
    {
        return TRUE;
    }

    p_pps = dec->pps + p_slice_next->pps_id;
    p_sps = dec->sps + p_pps->sps_id;

    if (p_sps->pic_order_cnt_type == 0)
    {
        if (p_slice_next->pic_order_cnt_lsb != p_slice->pic_order_cnt_lsb ||
            p_slice_next->delta_pic_order_cnt_bottom != p_slice->delta_pic_order_cnt_bottom)
        {
            return TRUE;
        }
    }
    else if (p_sps->pic_order_cnt_type == 1)
    {
        if (p_slice_next->delta_pic_order_cnt[0] != p_slice->delta_pic_order_cnt[0] ||
            p_slice_next->delta_pic_order_cnt[1] != p_slice->delta_pic_order_cnt[1])
        {
            return TRUE;
        }
    }

    if (p_slice_next->nal_unit_type != p_slice->nal_unit_type)
    {
        if (p_slice_next->nal_unit_type == NAL_TYPE_IDR_SLICE ||
            p_slice->nal_unit_type      == NAL_TYPE_IDR_SLICE)
        {
            return TRUE;
        }
    }
    else if (p_slice_next->nal_unit_type == NAL_TYPE_IDR_SLICE &&
             p_slice_next->idr_pic_id != p_slice->idr_pic_id)
    {
        return TRUE;
    }
    return FALSE;
}

BOOL
parser_avc_parse_nal_1(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec)
{
#define RBSP_BYTE_NUM_MAX   512
    /* should be enough for nal data of interests
    *  SPS worst 13 bytes and 496 se/ue in frext */
    uint32_t      hdr_size;
    uint8_t       rbsp_bytes[RBSP_BYTE_NUM_MAX];
    size_t        rbsp_size;
    bbio_handle_t dsb = 0;

    hdr_size           = (nal_buf[2] == 1) ? 3 : 4;
    dec->nal_unit_type = nal_buf[hdr_size] & 0x1f;
    dec->nal_ref_idc   = (nal_buf[hdr_size] >> 5) & 0x3;
    hdr_size++;
    msglog(NULL, MSGLOG_DEBUG, "\nGet Nal type %u(%s) idc %u size avail %zu\n",
           dec->nal_unit_type, get_nal_unit_type_dscr(dec->nal_unit_type),
           dec->nal_ref_idc, nal_size);

    /***** if the nal is end of stream, we think the nals within this sample is complete. */
    if (dec->nal_unit_type == 10)
    {
        return TRUE;
    }

    /****** AUD(9): must start AU, but have seen au after PPs */
    if (dec->nal_unit_type == NAL_TYPE_ACCESS_UNIT)
    {
        if (dec->pdNalType != PD_NAL_TYPE_NOT_VCL)
        {
            return TRUE;
        }
        msglog(NULL, MSGLOG_WARNING,"WARNING: AUD is not the first NAL in AU\n");
        return FALSE;
    }

    /****** special test for BD MVC: skip PPS and SEI within it */
    if (dec->mdNalType == PD_NAL_TYPE_NOT_SLICE_EXT)
    {
        /* in dependency */
        if (dec->nal_unit_type == NAL_TYPE_PIC_PARAM || dec->nal_unit_type == NAL_TYPE_SEI)
        {
            return FALSE;
        }
    }

    if (dec->nal_unit_type == NAL_TYPE_PREFIX_NAL)
    {
        return FALSE; /* fine as long as the suffix Nal doesn't start Au */
    }

    /****** VCL and 1,2,5: parsing to get the params, may check if start an au */
    if (nal_delimier_type_tbl[dec->nal_unit_type] ==  PD_NAL_TYPE_VCL)
    {
        parser_avc_remove_0x03(rbsp_bytes, &rbsp_size, nal_buf + hdr_size, MIN2(nal_size-hdr_size, RBSP_BYTE_NUM_MAX));
        dsb = reg_bbio_get('b', 'r');
        dsb->set_buffer(dsb, rbsp_bytes, rbsp_size, 0);
        parse_slice(dec, dsb);

        /* guarantee to be consistant within an au */
        dsb->destroy(dsb);

        /**** this is the first VCL but AU already started by non VCL */
        if (dec->pdNalType == PD_NAL_TYPE_NOT_VCL)
        {
            dec->slice_next->first_slice = 1;
            dec->first_vcl_cnt++;
            return FALSE;
        }

        /**** check if it is the first of new pic */
        if (is_first_slice(dec))
        {
            dec->slice_next->first_slice = 1;
            dec->first_vcl_cnt++;
            return TRUE;
        }
        else
        {
            dec->slice_next->first_slice = 0;
            return FALSE;
        }
    }

    /****** non-VCL(>5: 6-8, 16-18): may start AU */
    if (nal_delimier_type_tbl[dec->nal_unit_type] ==  PD_NAL_TYPE_NOT_VCL)
    {
        return (dec->pdNalType == PD_NAL_TYPE_VCL);
    }

    /****** can't start AU(0, 3-4, 10-13, 19-31) */
    /* just for test: special for MVC: also 15, 20, 24-25 */
    if (nal_delimier_type_tbl[dec->nal_unit_type] == PD_NAL_TYPE_SLICE_EXT ||
        nal_delimier_type_tbl[dec->nal_unit_type] == PD_NAL_TYPE_NOT_SLICE_EXT)
    {
        /**  NAL_TYPE_SLICE_EXT(20), NAL_TYPE_SUBSET_SEQ_PARAM(15),
         *   NAL_TYPE_VDRD(24), NAL_TYPE_DOLBY_3D(25)
         */
        return FALSE;
    }

    return FALSE;
}

int
parser_avc_parse_nal_2(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec)
{
#define RBSP_BYTE_NUM_MAX   512
    /* should be enough for nal data of interests
    *  SPS worst 13 bytes and 496 se/ue in frext */
    uint32_t      hdr_size;
    uint8_t       rbsp_bytes[RBSP_BYTE_NUM_MAX];
    size_t        rbsp_size;
    bbio_handle_t dsb = 0;
    int           ret = EMA_MP4_MUXED_OK;

    hdr_size = (nal_buf[2] == 1) ? 4 : 5;
    /****** VCL 1, 2, 5: already parsed */
    if (nal_delimier_type_tbl[dec->nal_unit_type] ==  PD_NAL_TYPE_VCL)
    {
        /* already parsed */
        if (dec->slice_next->first_slice)
        {
            avc_slice_t *p_slice = dec->slice;
            dec->slice      = dec->slice_next;
            dec->slice_next = p_slice;

            dec->IDR_pic = (dec->nal_unit_type == NAL_TYPE_IDR_SLICE);
            /* activation */
            dec->active_pps = dec->pps + dec->slice->pps_id;
            dec->active_sps = dec->sps + dec->active_pps->sps_id;

            if (!dec->active_sps->CpbDpbDelaysPresentFlag &&
                dec->first_vcl_cnt > 1 &&
                dec->slice->field_pic_flag != dec->slice_next->field_pic_flag)
            {
                msglog(NULL, MSGLOG_WARNING,"WARNING: timing info for PAFF is not fully supported\n");
            }
            /* poc */
            parser_avc_compute_poc(dec);
        }
        else
        {
            /* all in slice and slice_next should be same, except */
            dec->slice->first_slice = 0;
        }
    }
    /****** sps, pps, seq_ext */
    else if ((dec->nal_unit_type == NAL_TYPE_PIC_PARAM && dec->mdNalType != PD_NAL_TYPE_NOT_SLICE_EXT) ||
             dec->nal_unit_type == NAL_TYPE_SEQ_PARAM ||
             dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM ||
             dec->nal_unit_type == NAL_TYPE_SEQ_PARAM_EXT)
    {
        /* need futher parsing */
        parser_avc_remove_0x03(rbsp_bytes, &rbsp_size, nal_buf + hdr_size, MIN2(nal_size-hdr_size, RBSP_BYTE_NUM_MAX));
        dsb = reg_bbio_get('b', 'r');
        dsb->set_buffer(dsb, rbsp_bytes, rbsp_size, 0);

        if (dec->nal_unit_type == NAL_TYPE_SEQ_PARAM ||
            dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM)
        {
            ret = parse_sequence_parameter_set(dec, dsb);
            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
        }
        else if (dec->nal_unit_type == NAL_TYPE_PIC_PARAM)
        {
            ret = parse_pic_parameter_set(dec, dsb);
            if (ret != EMA_MP4_MUXED_OK &&
                ret != EMA_MP4_MUXED_EXIT)
            {
                return ret;
            }
        }
        else
        {
            parse_sequence_parameter_set_ext(dec, dsb);
        }
        dsb->destroy(dsb);
    }

    if (dec->mdNalType == PD_NAL_TYPE_NOT_SLICE_EXT)
    {
        if (dec->layer_idx != 1)
        {
            return EMA_MP4_MUXED_ES_ERR;
        }
        if (dec->nal_unit_type == NAL_TYPE_SLICE_EXT)
        {
            dec->mdNalType = PD_NAL_TYPE_SLICE_EXT;
        }
        else
        {
            /* mdNalType not changed and pdNalType not care about MVC status */
            if (!(dec->nal_unit_type == NAL_TYPE_PIC_PARAM ||
                  dec->nal_unit_type == NAL_TYPE_SEI ||
                  dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM ||
                  dec->nal_unit_type == NAL_TYPE_DOLBY_3D))
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
        }
    }
    else if (dec->mdNalType == PD_NAL_TYPE_SLICE_EXT)
    {
        if (dec->layer_idx != 1)
        {
            return EMA_MP4_MUXED_ES_ERR;
        }
        if (dec->nal_unit_type == NAL_TYPE_SLICE_EXT ||
            dec->nal_unit_type == NAL_TYPE_FILLER_DATA ||
            dec->nal_unit_type == NAL_TYPE_END_OF_SEQ ||
            dec->nal_unit_type == NAL_TYPE_END_OF_STREAM )
        {
            /* to make a continuous sub stream, nothing change */
        }
        else
        {
            /* move out of layer_idx = 1 */
            dec->mdNalType = nal_delimier_type_tbl[dec->nal_unit_type];
            if ((dec->mdNalType == PD_NAL_TYPE_NOT_SLICE_EXT) || (dec->mdNalType == PD_NAL_TYPE_SLICE_EXT))
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            dec->pdNalType = dec->mdNalType;
            dec->layer_idx = 0;
        }
    }
    else
    {
        dec->mdNalType = nal_delimier_type_tbl[dec->nal_unit_type];

        /* pdNalType care about !MVC NAL only */
        if (dec->mdNalType != PD_NAL_TYPE_NOT_SLICE_EXT && dec->mdNalType != PD_NAL_TYPE_SLICE_EXT)
        {
            if (dec->layer_idx)
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            dec->pdNalType = dec->mdNalType;
        }
        else
        {
            /* in MVC nal */
            if (dec->nal_unit_type != NAL_TYPE_VDRD || dec->layer_idx)
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            dec->pdNalType = PD_NAL_TYPE_NO;
            dec->layer_idx = 1;
        }
    }

    return EMA_MP4_MUXED_OK;
}


int
parser_avc_parse_el_nal(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec)
{
#define RBSP_BYTE_NUM_MAX   512
    /* should be enough for nal data of interests
    *  SPS worst 13 bytes and 496 se/ue in frext */
    uint8_t       rbsp_bytes[RBSP_BYTE_NUM_MAX];
    size_t        rbsp_size;
    bbio_handle_t dsb = 0;
    int           ret = EMA_MP4_MUXED_OK;

    dec->nal_unit_type = nal_buf[0] & 0x1f;
    /****** sps, pps, seq_ext */
    if ((dec->nal_unit_type == NAL_TYPE_PIC_PARAM && dec->mdNalType != PD_NAL_TYPE_NOT_SLICE_EXT) ||
             dec->nal_unit_type == NAL_TYPE_SEQ_PARAM ||
             dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM ||
             dec->nal_unit_type == NAL_TYPE_SEQ_PARAM_EXT)
    {
        /* need futher parsing */
        parser_avc_remove_0x03(rbsp_bytes, &rbsp_size, nal_buf+1, MIN2(nal_size-1, RBSP_BYTE_NUM_MAX));
        dsb = reg_bbio_get('b', 'r');
        dsb->set_buffer(dsb, rbsp_bytes, rbsp_size, 0);

        if (dec->nal_unit_type == NAL_TYPE_SEQ_PARAM ||
            dec->nal_unit_type == NAL_TYPE_SUBSET_SEQ_PARAM)
        {
            ret = parse_sequence_parameter_set(dec, dsb);

            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
        }
        else if (dec->nal_unit_type == NAL_TYPE_PIC_PARAM)
        {
            ret = parse_pic_parameter_set(dec, dsb);
            if (ret != EMA_MP4_MUXED_OK &&
                ret != EMA_MP4_MUXED_EXIT)
            {
                return ret;
            }
        }
        else
        {
            parse_sequence_parameter_set_ext(dec, dsb);
        }
        dsb->destroy(dsb);
    }


    return EMA_MP4_MUXED_OK;
}


void
parser_avc_dec_init(avc_decode_t *dec)
{
    /* pingpong pointer */
    dec->slice      = dec->slices;
    dec->slice_next = dec->slices + 1;

    /* tables */
    memset(MaxBRTbl, 0, sizeof(MaxBRTbl));
    memset(MaxCPBTbl, 0, sizeof(MaxCPBTbl));
    MaxBRTbl[10] = 64;         MaxCPBTbl[10] = 175;
    MaxBRTbl[11] = 192;        MaxCPBTbl[11] = 500;
    MaxBRTbl[12] = 384;        MaxCPBTbl[12] = 1000;
    MaxBRTbl[13] = 768;        MaxCPBTbl[13] = 2000;
    MaxBRTbl[20] = 2000;       MaxCPBTbl[20] = 2000;
    MaxBRTbl[21] = 4000;       MaxCPBTbl[21] = 4000;
    MaxBRTbl[22] = 4000;       MaxCPBTbl[22] = 4000;
    MaxBRTbl[30] = 10000;      MaxCPBTbl[30] = 10000;
    MaxBRTbl[31] = 14000;      MaxCPBTbl[31] = 14000;
    MaxBRTbl[32] = 20000;      MaxCPBTbl[32] = 20000;
    MaxBRTbl[40] = 20000;      MaxCPBTbl[40] = 25000;
    MaxBRTbl[41] = 50000;      MaxCPBTbl[41] = 62500;
    MaxBRTbl[42] = 50000;      MaxCPBTbl[42] = 62500;
    MaxBRTbl[50] = 135000;     MaxCPBTbl[50] = 135000;
    MaxBRTbl[51] = 240000;     MaxCPBTbl[51] = 240000;
    MaxBRTbl[52] = 240000;     MaxCPBTbl[52] = 240000;

    memset(cpbBrNalfactorTbl, 0, sizeof(cpbBrNalfactorTbl));
    cpbBrNalfactorTbl[66] = 1200;
    cpbBrNalfactorTbl[77] = 1200;
    cpbBrNalfactorTbl[88] = 1200;

    cpbBrNalfactorTbl[100] = 1500;
    cpbBrNalfactorTbl[110] = 3600;
    cpbBrNalfactorTbl[122] = 4800;
    cpbBrNalfactorTbl[244] = 4800;

    cpbBrNalfactorTbl[44]  = 4800;

    cpbBrNalfactorTbl[118] = 1500;  /* MVC */
    cpbBrNalfactorTbl[128] = 1500;  /* HDMV: for CPB only */
    cpbBrNalfactorTbl[134] = 1500;  /* DB3D: for CPB only */
}
