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
    @file parser_avc_dec.h
    @brief Defines avc parser needed lower level structures and APIs
*/

#ifndef __PARSER_AVC_DEC_H__
#define __PARSER_AVC_DEC_H__

#include "io_base.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define AVC_START_CODE              0x000001
#define AVC_PREVENT_3_BYTE          0x000003

#define AVC_PROFILE_BASELINE        66
#define AVC_PROFILE_MAIN            77
#define AVC_PROFILE_EXTENDED        88

typedef enum nal_type_t_
{
    NAL_TYPE_UNSPECIFIED0     = 0,
    NAL_TYPE_NON_IDR_SLICE    = 1,
    NAL_TYPE_DP_A_SLICE       = 2,
    NAL_TYPE_DP_B_SLICE       = 3,
    NAL_TYPE_DP_C_SLICE       = 4,
    NAL_TYPE_IDR_SLICE        = 5,
    NAL_TYPE_SEI              = 6,
    NAL_TYPE_SEQ_PARAM        = 7,
    NAL_TYPE_PIC_PARAM        = 8,
    NAL_TYPE_ACCESS_UNIT      = 9,
    NAL_TYPE_END_OF_SEQ       = 10,
    NAL_TYPE_END_OF_STREAM    = 11,
    NAL_TYPE_FILLER_DATA      = 12,
    NAL_TYPE_SEQ_PARAM_EXT    = 13,
    NAL_TYPE_PREFIX_NAL       = 14,
    NAL_TYPE_SUBSET_SEQ_PARAM = 15,
    NAL_TYPE_REV16            = 16,
    NAL_TYPE_REV18            = 18,
    NAL_TYPE_AUX_SLICE        = 19,
    NAL_TYPE_SLICE_EXT        = 20,
    NAL_TYPE_REV21            = 21,
    NAL_TYPE_REV23            = 23,
    NAL_TYPE_VDRD             = 24,
    NAL_TYPE_DOLBY_3D         = 25,
    NAL_TYPE_UNSPECIFIED26    = 26,
    NAL_TYPE_UNSPECIFIED27    = 27,
    NAL_TYPE_UNSPECIFIED28    = 28,
    NAL_TYPE_UNSPECIFIED29    = 29,
    NAL_TYPE_UNSPECIFIED30    = 30,
    NAL_TYPE_UNSPECIFIED31    = 31,
} nal_type_t;

typedef enum sei_msgType_t_
{
    SEI_BUFFERING_PERIOD                        = 0,
    SEI_PIC_TIMING                              = 1,
    SEI_PAN_SCAN_RECT                           = 2,
    SEI_FILLER_PAYLOAD                          = 3,
    SEI_USER_DATA_REGISTERED_ITU_T_T35          = 4,
    SEI_USER_DATA_UNREGISTERED                  = 5,
    SEI_RECOVERY_POINT                          = 6,
    SEI_DEC_REF_PIC_MARKING_REPETITION          = 7,
    SEI_SPARE_PIC                               = 8,
    SEI_SCENE_INFO                              = 9,
    SEI_SUB_SEQ_INFO                            = 10,
    SEI_SUB_SEQ_LAYER_CHARACTERISTICS           = 11,
    SEI_SUB_SEQ_CHARACTERISTICS                 = 12,
    SEI_FULL_FRAME_FREEZE                       = 13,
    SEI_FULL_FRAME_FREEZE_RELEASE               = 14,
    SEI_FULL_FRAME_SNAPSHOT                     = 15,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START    = 16,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END      = 17,
    SEI_MOTION_CONSTRAINED_SLICE_GROUP_SET      = 18,
    SEI_FILM_GRAIN_CHARACTERISTICS              = 19,
    SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE    = 20,
    SEI_STEREO_VIDEO_INFO                       = 21,
    SEI_POST_FILTER_HINT                        = 22,
    SEI_TONE_MAPPING_INFO                       = 23,
    SEI_SCALABILITY_INFO                        = 24,
    SEI_SUB_PIC_SCALABLE_LAYER                  = 25,
    SEI_NON_REQUIRED_LAYER_REP                  = 26,
    SEI_PRIORITY_LAYER_INFO                     = 27,
    SEI_LAYERS_NOT_PRESENT                      = 28,
    SEI_LAYER_DEPENDENCY_CHANGE                 = 29,
    SEI_SCALABLE_NESTING                        = 30,
    SEI_BASE_LAYER_TEMPORAL_HDR                 = 31,
    SEI_QUALITY_LAYER_INTEGRITY_CHECK           = 32,
    SEI_REDUNDANT_PIC_PROPERTY                  = 33,
    SEI_T10_PIC_INDEX                           = 34,
    SEI_T1_SWITCHING_POINT                      = 35,
    SEI_FRAME_PACKING                           = 45,
    /** all other valuse are for SEI_RESERVED msg */
} sei_msgType_t;

/** slice types */
typedef enum avc_sliceType_t_
{
    AVC_SLICE_TYPE_P                 = 0,
    AVC_SLICE_TYPE_B                 = 1,
    AVC_SLICE_TYPE_I                 = 2,
    AVC_SLICE_TYPE_SP                = 3,
    AVC_SLICE_TYPE_SI                = 4,
    AVC_SLICE_TYPE2_P                = 5,
    AVC_SLICE_TYPE2_B                = 6,
    AVC_SLICE_TYPE2_I                = 7,
    AVC_SLICE_TYPE2_SP               = 8,
    AVC_SLICE_TYPE2_SI               = 9,
} avc_sliceType_t;

typedef enum sei_frame_packing_t_
{
    SEI_FRAME_PACKING_SIDE_BY_SIDE   = 3,
    SEI_FRAME_PACKING_TOP_BOTTOM     = 4,
} sei_frame_packing_t;

#define AVC_SLICE_TYPE_IS_P(t)            ((t) == AVC_SLICE_TYPE_P || (t) == AVC_SLICE_TYPE2_P)
#define AVC_SLICE_TYPE_IS_B(t)            ((t) == AVC_SLICE_TYPE_B || (t) == AVC_SLICE_TYPE2_B)
#define AVC_SLICE_TYPE_IS_I(t)            ((t) == AVC_SLICE_TYPE_I || (t) == AVC_SLICE_TYPE2_I)
#define AVC_SLICE_TYPE_IS_SP(t)           ((t) == AVC_SLICE_TYPE_SP || (t) == AVC_SLICE_TYPE2_SP)
#define AVC_SLICE_TYPE_IS_SI(t)           ((t) == AVC_SLICE_TYPE_SI || (t) == AVC_SLICE_TYPE2_SI)

#define HAVE_SLICE_I                0x1
#define HAVE_SLICE_P                0x2
#define HAVE_SLICE_B                0x4
#define HAVE_SLICE_SI               0x8
#define HAVE_SLICE_SP               0x10
#define HAVE_ALL_SLICES             0x1f
#define HAVE_ALL_BUT_B_SLICES       0x1b

typedef enum avc_picType_t_
{
    AVC_PIC_TYPE_FRAME = 0,
    AVC_PIC_TYPE_FIELD_TOP,
    AVC_PIC_TYPE_FIELD_BOTTOM,
} avc_picType_t;

typedef enum pd_nalType_t_
{
    PD_NAL_TYPE_NO = 0,         /** impossible picture delimiter */

    PD_NAL_TYPE_NOT_VCL,        /** not vcl */
    PD_NAL_TYPE_VCL,            /** vcl */

    PD_NAL_TYPE_NOT_SLICE_EXT,  /** in dependency but not slice extension */
    PD_NAL_TYPE_SLICE_EXT       /** slice extension */
} pd_nalType_t;

typedef struct sps_t_
{
    uint8_t profile_idc;
    uint8_t compatibility;
    uint8_t level_idc;
    uint8_t sps_id;

    uint32_t chroma_format_idc;
    uint8_t  separate_colour_plane_flag;
    uint32_t bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;
    uint8_t  qpprime_y_zero_transform_bypass_flag;
    uint8_t  seq_scaling_matrix_present_flag;
    
    uint32_t log2_max_frame_num_minus4;

    uint32_t pic_order_cnt_type;
    uint32_t log2_max_pic_order_cnt_lsb_minus4;

    uint8_t delta_pic_order_always_zero_flag; 
    int32_t offset_for_non_ref_pic;          
    int32_t offset_for_top_to_bottom_field;   
    uint8_t num_ref_frames_in_pic_order_cnt_cycle;      
    int16_t offset_for_ref_frame[256];
     
    uint8_t max_num_ref_frames;
    uint8_t gaps_in_frame_num_value_allowed_flag;
    /** _minus1s */
    uint8_t frame_mbs_only_flag;
    /** uint8_t mb_adaptive_frame_field_flag, direct_8x8_inference_flag; */
    uint8_t  frame_cropping_flag;
    uint32_t frame_crop_left_offset;
    uint32_t frame_crop_right_offset;
    uint32_t frame_crop_top_offset;
    uint32_t frame_crop_bottom_offset;

    /** VUI */
    uint8_t vui_parameter_present_flag;

    uint8_t  aspect_ratio_idc;
    uint16_t sar_width;  /** default = 0=>unspecified */
    uint16_t sar_height; /** default = 0=>unspecified */

    uint8_t overscan_info; /** combines overscan_info_present_flag and overscan_appropriate_flag */

    uint8_t video_signal_info_present_flag;
    uint8_t video_format;
    uint8_t video_full_range_flag;
    uint8_t colour_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;
    
    uint8_t chroma_loc_info_present_flag;

    uint8_t  timing_info_present_flag;
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    BOOL     fixed_frame_rate_flag;

    /** HRD */
    uint8_t  nal_hrd_parameters_present_flag;
    uint8_t  vcl_hrd_parameters_present_flag;
    uint32_t cpb_cnt_minus1;

    uint8_t initial_cpb_removal_delay_length_minus1;
    uint8_t cpb_removal_delay_length_minus1;
    uint8_t dpb_output_delay_length_minus1;
    uint8_t time_offset_length;
    
    uint8_t low_delay_hrd_flag;
    uint8_t pic_struct_present_flag;
    uint8_t bitstream_restriction_flag;
    uint8_t num_reorder_frames;
    uint8_t max_dec_frame_buffering;
    
    /**** SPS ext: if exist, must follow sps and has same sps_id, so put inside sps */
    uint8_t spsext_id;
    uint8_t aux_format_id;

    /**** derived value */
    uint32_t pic_width_out, pic_height_out;
    uint32_t max_frame_num;
    uint32_t max_poc_lsb;
    int32_t  expected_delta_per_poc_cycle;
    uint32_t pic_width, pic_height; /** from _minus1 */
    uint32_t bit_rate_1st, cpb_size_1st, bit_rate_last, cpb_size_last; /** that for vcl or nal(if no vcl)*/
    /** those derived and may subject to external signaling */
    uint8_t NalHrdBpPresentFlag, VclHrdBpPresentFlag; /** nal/vcl_hrd_parameters_present_flag | external */
    uint8_t CpbDpbDelaysPresentFlag;/** nal_hrd_parameters_present_flag | vcl_hrd_parameters_present_flag | external */
    uint8_t UseSeiTiming; /** use SEI timing only if it make sense: we do have erroneous sei timing */

    /** add some error recovery: fallback to sps 0 if possible */
    uint8_t isDefined;
} sps_t;

typedef struct pps_t_
{
    uint8_t pps_id;
    uint8_t sps_id;
    uint8_t bottom_field_pic_order_in_frame_present_flag;
    uint8_t redundant_pic_cnt_present_flag;

    /** add some error recovery: fallback to pps 0 if possible */
    uint8_t isDefined;
} pps_t;

typedef struct slice_t_
{
    /** that of slice */
    uint8_t nal_ref_idc;
    uint8_t nal_unit_type;

    uint32_t slice_type;
    uint8_t  pps_id;
    uint32_t frame_num;
    uint8_t  field_pic_flag;
    uint8_t  bottom_field_flag;
    uint32_t idr_pic_id;

    /** dec->pic_order_cnt_type == 0 */
    uint32_t pic_order_cnt_lsb;
    int32_t  delta_pic_order_cnt_bottom;

    /** dec->pic_order_cnt_type == 1 */
    int32_t delta_pic_order_cnt[2];

    uint32_t redundant_pic_cnt;

    /** derived */
    uint8_t first_slice;
} avc_slice_t;

typedef struct avc_decode_t_
{
    /** NAL reference and type of current nal */
    uint8_t nal_ref_idc;
    uint8_t nal_unit_type;

    /** SPS */
    uint8_t sps_id, sps_id_enh;                     /** that of current nal */
    sps_t   sps[32], *active_sps, *active_sps_enh;  /** base layer and enhanced layer (assume one and no collision) */

    /** PPS */
    uint8_t pps_id;                                 /** that of current nal */
    pps_t   pps[256], *active_pps;

    /** SEI buffering and timing */
    uint32_t initial_cpb_removal_delay_1st;
    uint32_t initial_cpb_removal_delay_last;
    uint32_t cpb_removal_delay;
    uint32_t dpb_output_delay;
    uint8_t  pic_struct;

    /** slice */
    avc_slice_t  slices[2];
    avc_slice_t *slice;                  /** only the first vcl of au come here. but first_slice = 0 is 2nd... is parsed */
    avc_slice_t *slice_next;             /** to handle slice start AU case: the current parsing vcl(1, 2 or 5) */
    BOOL     sample_has_redundancy;      /** do all slices in AU have redundancy? See 'sdtp' box. */

    /** that for poc derivation */
    int32_t   pic_order_cnt;         /** can be < 0 */
    /** for poc = 0 */
    uint32_t  pic_order_cnt_msb;
    uint32_t  pic_order_cnt_msb_prev;
    uint32_t  pic_order_cnt_lsb_prev;
    /** for poc = 1 */
    int32_t   frame_num_offset;
    int32_t   frame_num_offset_prev;
    uint32_t  frame_num_prev;

    /** that of pic */
    BOOL          IDR_pic;
    avc_picType_t picType;
    /** au delimiter nal type */
    pd_nalType_t  pdNalType;

    /** to derive DTS and CTS */
    uint32_t first_vcl_cnt;      /** first vcl dectected so far */
    int32_t  pic_dec_order_cnt;  /** reset on each IDR as does pic_order_cnt */
    uint8_t  NewBpStart;
    uint64_t DtsNb;              /** dts of previous BP period, in time_scale */

    /** to support stereoscopic/frame compatible 3D */
    uint32_t frame_packing_type;

    /** to support push mode: parser work on one nal a time */
    uint8_t  nal_idx_in_au;      /** start from 0 */
    BOOL     last_au;
    BOOL     keep_all;

    /** to support MVC */
    pd_nalType_t mdNalType;     /** take care of mvc case, similar to pdNalType */
    uint8_t      layer_idx;
    /** hack for dolby 3D */
    uint8_t profile_idc_sub ;
    uint8_t compatibility_sub;
    uint8_t level_idc_sub;
} avc_decode_t;

struct parser_t_;

void     parser_avc_dec_init(avc_decode_t *dec);
uint32_t src_read_ue(bbio_handle_t bs); /** to export read_ue(); */
void     parser_avc_remove_0x03(uint8_t *dst, size_t *dstlen, const uint8_t *src, const size_t srclen);
BOOL     parser_avc_parse_nal_1(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec);

int32_t  /** @return Error code EMA_MP4_MUXED_... */
parser_avc_parse_nal_2(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec);

int32_t
parser_avc_parse_el_nal(const uint8_t *nal_buf, size_t nal_size, avc_decode_t *dec);

/**
 * @brief Parsing sequence parameter set.
 */
int32_t  /** @return Error code EMA_MP4_MUXED_... */
parse_sequence_parameter_set(avc_decode_t *dec, bbio_handle_t bs);

#ifdef __cplusplus
};
#endif

#endif /* !__PARSER_AVC_DEC_H__ */
