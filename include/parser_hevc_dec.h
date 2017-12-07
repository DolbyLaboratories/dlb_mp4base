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
    @file parser_hevc_dec.h
    @brief Defines lower level hevc parser
*/

#ifndef __PARSER_HEVC_DEC_H__
#define __PARSER_HEVC_DEC_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

#define HEVC_MAX( a , b )       ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define HEVC_MIN( a , b )       ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define HEVC_ABS(a)             ( ( a ) < 0 ? -( a ) : ( a ) )
#define HEVC_CLIP(min,val,max)  ( HEVC_MIN( HEVC_MAX( ( min ), ( val ) ), ( max ) ) )
#define HEVC_LIMIT_L(val)       ( ( val ) > gi_max_val_luma ? gi_max_val_luma : HEVC_MAX( 0, (val) ) )
#define HEVC_LIMIT_C(val)       ( ( val ) > gi_max_val_chroma ? gi_max_val_chroma : HEVC_MAX( 0, (val) ) )
#define HEVC_INT32_SIGN(val)    ((((int32_t)(val)) >> 31) | ((int32_t)( ((uint32_t) -((int32_t)(val))) >> 31)))

#define Swap_t( a,b,type ) { type *p_tmp = a; a = b; b = p_tmp; }

#define MAX_VPS_OP_SETS_PLUS1                     1024

/** INTRA modes */
#define INTRA_MODE_PLANAR     0
#define INTRA_MODE_VER        26
#define INTRA_MODE_HOR        10
#define INTRA_MODE_DC         1

#define DM_CHROMA_IDX         36

#define NUM_CHROMA_MODE       5


#define MAX_CU_DEPTH          7                    /** log2(LCUSize) */
#define MAX_CU_SIZE           (1<<(MAX_CU_DEPTH))  /** maximum allowable size of CU */

#define MAX_NUM_REF_PICS      16
#define MAX_NUM_REF           16            /** max. value of multiple reference frames */

#define MAX_TLAYER            8             /** max number of temporal layer */

#define MIN_QP                0
#define MAX_QP                51

#define MLS_GRP_NUM           64       /**  Max number of coefficient groups, max(16, 64) */
#define MLS_CG_SIZE           4        /**  Coefficient group size of 4x4 */

#define QUANT_IQUANT_SHIFT    20      /** Q(QP%6) * IQ(QP%6) = 2^20 */
#define QUANT_SHIFT           14      /** Q(4) = 2^14 */
#define MAX_TR_DYNAMIC_RANGE  15      /** Maximum transform dynamic range (excluding sign bit) */

#define SHIFT_INV_1ST          7      /** Shift after first inverse transform stage */
#define SHIFT_INV_2ND         12      /** Shift after second inverse transform stage */

#define REGULAR_DCT           ((1<<16)-1)

#define MAX_CPB_CNT                 32          /**  Upper bound of (cpb_cnt_minus1 + 1) */

#define MAX_GOP                     64          /**  max. value of hierarchical GOP size */

#define MAX_TILE_COUNT              64           /**  que sais-je */

/** AMVP: advanced motion vector prediction */
#define AMVP_MAX_NUM_CANDS          2           /**  max number of final candidates */
#define AMVP_MAX_NUM_CANDS_MEM      3           /**  max number of candidates */
#define AMVP_DECIMATION_FACTOR      4           /**  motion vector subsampling */

#define MAX_VPS_NUM_HRD_PARAMETERS                1
#define MAX_VPS_NUM_HRD_PARAMETERS_ALLOWED_PLUS1  1024
#define MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1  1

/** MERGE */
#define MRG_MAX_NUM_CANDS           5

#define SCALING_LIST_NUM 6         /**  list number for quantization matrix */
#define SCALING_LIST_NUM_32x32 2   /**  list number for quantization matrix 32x32 */
#define SCALING_LIST_REM_NUM 6     /**  remainder of QP/6 */
#define SCALING_LIST_START_VALUE 8 /**  start value for dpcm mode */
#define MAX_MATRIX_COEF_NUM 64     /**  max coefficient number for quantization matrix */
#define MAX_MATRIX_SIZE_NUM 8      /**  max size number for quantization matrix */
#define SCALING_LIST_DC 16         /**  default DC value */

#define SBH_THRESHOLD         4  /**  value of the fixed SBH controlling threshold */
#define C1FLAG_NUMBER         8  /**  maximum number of largerThan1 flag coded in one chunk :  16 in HM5 */ 


#define COEF_REMAIN_BIN_REDUCTION        3 /** Maximum codeword length of coeff_abs_level_remaining reduced to 32. */
/**  COEF_REMAIN_BIN_REDUCTION is also used to indicate the level at which the VLC */
/**  transitions from Golomb-Rice to TU+EG(k) */

#define CU_DQP_TU_EG                     1 /** Bin reduction for delta QP coding */
#if (CU_DQP_TU_EG)
#define CU_DQP_TU_CMAX 5 /** max number bins for truncated unary */
#define CU_DQP_EG_k 0    /** expgolomb order */
#endif

#define DEPENDENT_SLICES                 1
#define NUM_WP_LIMIT                     1  /** number of total signalled weight flags <=24 */
#define B_PRINT_REFPIC_LIST              0
#define MAX_NUM_THREADS_TOTAL           64

typedef enum boolean_t { false = 0, true = 1 } bool;
typedef enum
{
    B_SLICE,
    P_SLICE,
    I_SLICE
} slice_type_t;

typedef enum
{
    SCALING_LIST_4x4 = 0,
    SCALING_LIST_8x8,
    SCALING_LIST_16x16,
    SCALING_LIST_32x32,
    SCALING_LIST_SIZE_NUM
} scaling_list_size_t;

#define SAO_BO_BITS                   5
#define SAO_LUMA_GROUP_NUM            (1<<SAO_BO_BITS)

typedef struct  
{
    int32_t *pi_bo_luma;
    int32_t *pi_bo_chroma;
    int32_t *pi_clip_luma;
    int32_t *pi_clip_chroma;
    int32_t *pi_bo_offsets;
    int32_t ai_eo_offsets[SAO_LUMA_GROUP_NUM];

    int32_t i_bits_luma;
    int32_t i_bits_chroma;
    int32_t i_bit_increase_luma;
    int32_t i_bit_increase_chroma;
    
    uint16_t *pui16_left1;
    uint16_t *pui16_left2;
    uint16_t *pui16_top1;
    uint16_t *pui16_top2;

    uint16_t *pui16_all_buffer;

    bool b_pcm_restoration;
    bool b_separation;

} sao_context_t;


typedef struct  
{
    uint32_t ui_length;
    int64_t i64_bits_available;

    uint32_t ui_byte_position;
    uint32_t ui_bit_idx;
    uint32_t ui32_curr_bits;
    uint32_t ui32_next_bits;
    uint32_t ui32_bits_read; 

    uint8_t *pui8_payload;

} bitstream_t;

typedef enum nalu_type_t_
{
    NAL_UNIT_CODED_SLICE_TRAIL_N = 0,
    NAL_UNIT_CODED_SLICE_TRAIL_R,     /** 1 */
                                      
    NAL_UNIT_CODED_SLICE_TSA_N,       /** 2 */
    NAL_UNIT_CODED_SLICE_TLA_R,       /** 3 */
                                      
    NAL_UNIT_CODED_SLICE_STSA_N,      /** 4 */
    NAL_UNIT_CODED_SLICE_STSA_R,      /** 5 */
                                      
    NAL_UNIT_CODED_SLICE_RADL_N,      /** 6 */
    NAL_UNIT_CODED_SLICE_RADL_R,      /** 7 */
                                      
    NAL_UNIT_CODED_SLICE_RASL_N,      /** 8 */
    NAL_UNIT_CODED_SLICE_RASL_R,      /** 9 */
                                      
    NAL_UNIT_RESERVED_VCL_N10,          
    NAL_UNIT_RESERVED_VCL_R11,          
    NAL_UNIT_RESERVED_VCL_N12,          
    NAL_UNIT_RESERVED_VCL_R13,          
    NAL_UNIT_RESERVED_VCL_N14,          
    NAL_UNIT_RESERVED_VCL_R15,          
                                      
    NAL_UNIT_CODED_SLICE_BLA_W_LP,    /** 16 */
    NAL_UNIT_CODED_SLICE_BLA_W_RADL,  /** 17 */
    NAL_UNIT_CODED_SLICE_BLA_N_LP,    /** 18 */
    NAL_UNIT_CODED_SLICE_IDR_W_RADL,  /** 19 */
    NAL_UNIT_CODED_SLICE_IDR_N_LP,    /** 20 */
    NAL_UNIT_CODED_SLICE_CRA,         /** 21 */
    NAL_UNIT_RESERVED_IRAP_VCL22,      
    NAL_UNIT_RESERVED_IRAP_VCL23,      
                                      
    NAL_UNIT_RESERVED_VCL24,          
    NAL_UNIT_RESERVED_VCL25,          
    NAL_UNIT_RESERVED_VCL26,          
    NAL_UNIT_RESERVED_VCL27,          
    NAL_UNIT_RESERVED_VCL28,          
    NAL_UNIT_RESERVED_VCL29,          
    NAL_UNIT_RESERVED_VCL30,          
    NAL_UNIT_RESERVED_VCL31,          
                                      
    NAL_UNIT_VPS,                     /** 32 */
    NAL_UNIT_SPS,                     /** 33 */
    NAL_UNIT_PPS,                     /** 34 */
    NAL_UNIT_ACCESS_UNIT_DELIMITER,   /** 35 */
    NAL_UNIT_EOS,                     /** 36 */
    NAL_UNIT_EOB,                     /** 37 */
    NAL_UNIT_FILLER_DATA,             /** 38 */
    NAL_UNIT_PREFIX_SEI,              /** 39 */
    NAL_UNIT_SUFFIX_SEI,              /** 40 */

    NAL_UNIT_RESERVED_NVCL41,
    NAL_UNIT_RESERVED_NVCL42,
    NAL_UNIT_RESERVED_NVCL43,
    NAL_UNIT_RESERVED_NVCL44,
    NAL_UNIT_RESERVED_NVCL45,
    NAL_UNIT_RESERVED_NVCL46,
    NAL_UNIT_RESERVED_NVCL47,
    NAL_UNIT_UNSPECIFIED_48,
    NAL_UNIT_UNSPECIFIED_49,
    NAL_UNIT_UNSPECIFIED_50,
    NAL_UNIT_UNSPECIFIED_51,
    NAL_UNIT_UNSPECIFIED_52,
    NAL_UNIT_UNSPECIFIED_53,
    NAL_UNIT_UNSPECIFIED_54,
    NAL_UNIT_UNSPECIFIED_55,
    NAL_UNIT_UNSPECIFIED_56,
    NAL_UNIT_UNSPECIFIED_57,
    NAL_UNIT_UNSPECIFIED_58,
    NAL_UNIT_UNSPECIFIED_59,
    NAL_UNIT_UNSPECIFIED_60,
    NAL_UNIT_UNSPECIFIED_61,
    NAL_UNIT_UNSPECIFIED_62,
    NAL_UNIT_UNSPECIFIED_63,
    NAL_UNIT_INVALID,
} hevc_nalu_type_t;

typedef struct  
{
    hevc_nalu_type_t e_nalu_type;
    uint32_t ui_num_bytes;
    int32_t i_temporal_id;
    bool b_incomplete;

    bitstream_t bitstream;

    uint32_t ui_bytes_removed;
    uint32_t aui_bytes_removed_positions[4096];

    uint32_t read_nalu_consumed;
#define RBSP_BYTE_NUM_MAX   1024
    uint8_t rbsp_buff[RBSP_BYTE_NUM_MAX];

} hevc_nalu_t;


typedef struct  
{
    int32_t i_width;
    int32_t i_height;
    int32_t i_right_edge_pos;
    int32_t i_bottom_edge_pos;
    int32_t i_1st_cu_addr;
    int32_t i_idx;

} tile_t;

typedef struct
{
    int32_t i_poc;
    bool b_reconstructed;
    bool b_referenced;
    bool b_output;
    bool b_longterm;

    int32_t i_width_in_cu;
    int32_t i_height_in_cu;

    int32_t i_width;
    int32_t i_height;

    int32_t i_stride;
    int32_t i_stride_chr;

    uint16_t *p_recon_l;
    uint16_t *p_recon_cb;
    uint16_t *p_recon_cr;

    int32_t i_padding;

    void *p_theLCUs;

    void *p_slice_map;
    int32_t i_num_slices;
    int32_t i_curr_slice;

    tile_t as_tiles[ MAX_TILE_COUNT ];

    uint8_t aui_digest[ 3 ][ 16 ];
    bool b_got_digest;

    void *p_decoder_context;

} reference_picture_t;

typedef struct  
{
    bool b_inter_rps_prediction;

    int32_t i_num_pictures;
    int32_t i_num_negativePictures;
    int32_t i_num_positivePictures;
    int32_t i_num_longtermPictures;
    int32_t ai_delta_poc[ MAX_NUM_REF_PICS ];
    int32_t ai_poc[ MAX_NUM_REF_PICS ];
    bool ab_used[ MAX_NUM_REF_PICS ];
    bool ab_ltmsb[ MAX_NUM_REF_PICS ];

    int32_t  i_num_ref_idc; 
    int32_t  ai_ref_idc[ MAX_NUM_REF_PICS+1 ];
} reference_picture_set_t;


typedef struct 
{
    bool b_l0;  
    bool b_l1;  
    int32_t ai_set_idx_l0[ 32 ];
    int32_t ai_set_idx_l1[ 32 ];
} rpl_modification_t;

typedef struct 
{
    int32_t ai_scaling_list_dc[ SCALING_LIST_SIZE_NUM ][ SCALING_LIST_NUM ];        /** the DC value of the matrix coefficient for 16x16 */
    int32_t ai_ref_matrix_idx[ SCALING_LIST_SIZE_NUM ][ SCALING_LIST_NUM ];
    int32_t ai_scaling_list_coeff[ SCALING_LIST_SIZE_NUM ][ SCALING_LIST_NUM ][ MAX_MATRIX_COEF_NUM ];     /** quantization matrix */
} scaling_list_t;

typedef struct 
{
    /** array of dequantization matrix coefficient 4x4 */
    int32_t *pi32_dequant_scales[ SCALING_LIST_SIZE_NUM ][ SCALING_LIST_NUM ][ SCALING_LIST_REM_NUM ];
} scaling_list_context_t;

typedef struct
{
    bool m_bitRateInfoPresentFlag[MAX_TLAYER];
    bool m_picRateInfoPresentFlag[MAX_TLAYER];
    int32_t m_avgBitRate[MAX_TLAYER];
    int32_t m_maxBitRate[MAX_TLAYER];
    int32_t m_constantPicRateIdc[MAX_TLAYER];
    int32_t m_avgPicRate[MAX_TLAYER];
} bit_rate_picrate_info_t;

typedef struct  
{
    int32_t i_max_temporal_layers;
    int32_t i_max_layers;
    int32_t i_id;
    bool b_temporal_id_nesting;

    int32_t ai_max_dec_pic_buffering[8];
    int32_t ai_num_reorder_pics[8];
    int32_t ai_max_latency_increase[8];
    
    bool b_extension;

    int32_t i_num_hrd_params;
    int32_t i_vps_max_nuh_reserved_zero_layer_id;
    int32_t i_vps_max_op_sets;
    
    bool b_vps_timing_info_present_flag;
    uint32_t ui_vps_num_units_in_tick;
    uint32_t ui_vps_time_scale;
    bool b_vps_poc_proportional_to_timing_flag;
    int32_t i_vps_num_ticks_poc_diff_one_minus1;

    bool ab_oplayer_id_included[MAX_VPS_NUM_HRD_PARAMETERS_ALLOWED_PLUS1][MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1];
    bit_rate_picrate_info_t s_bitrate_info;
    
    bool b_isDefined;
} video_parameter_set_t;

#define MAX_CTU_DEPTH          6 

typedef struct luts_s
{
#define MAX_CTU_SIZE            (1<<(MAX_CTU_DEPTH))         /** maximum allowable size of CU */
#define MIN_PU_SIZE             4
#define MAX_NUM_SPU_W           (MAX_CTU_SIZE/MIN_PU_SIZE)   /** maximum number of SPU in horizontal line */
#define LOG2_SCAN_SET_SIZE      4
#define SCAN_SET_SIZE           16

    int8_t au8_convert_to_bit[ MAX_CTU_SIZE + 1 ];

    int32_t ai32_zscan_2_raster[ MAX_NUM_SPU_W*MAX_NUM_SPU_W ];
    int32_t ai32_raster_2_zscan[ MAX_NUM_SPU_W*MAX_NUM_SPU_W ];
    int32_t aui_raster_to_pel_x [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ];
    int32_t aui_raster_to_pel_y [ MAX_NUM_SPU_W*MAX_NUM_SPU_W ];

    int32_t ai32_mocomp_map[ MAX_NUM_SPU_W*MAX_NUM_SPU_W ];

    uint32_t aui32_sig_last_scan_cg_32x32[ 64 ];
    uint32_t *apui32_sig_last_scan[ 3 ][ MAX_CTU_DEPTH ];
} luts_t;

typedef struct
{
    int32_t i_profile_space;
    int32_t i_profile;
    int32_t i_level_idc;
    int32_t i_profile_compat;

    int32_t i_tier_flag; 
    int32_t i_profile_idc; 

    int8_t i_id;
    int8_t i_vps_id;
    int8_t i_chroma_format_idc;             /** 1 for main profile */
    bool b_separate_colour_plane_flag;
    int8_t i_max_temporal_layers;
    int16_t i_pic_luma_width;
    int16_t i_pic_luma_height;
    
    int16_t i_pic_conf_win_left_offset;
    int16_t i_pic_conf_win_right_offset;
    int16_t i_pic_conf_win_top_offset;
    int16_t i_pic_conf_win_bottom_offset;

    int8_t i_bit_depth_luma;
    int8_t i_bit_depth_chroma;
    int8_t i_log2_max_pic_order_cnt_lsb;
    
    int32_t ai_max_dec_pic_buffering[ 8 ];
    int32_t ai_num_reorder_pics[ 8 ];
    int32_t max_latency_increase[ 8 ];

    bool b_restricted_ref_pic_lists;
    bool b_lists_modification_present;

    int8_t i_log2_min_coding_block_size;
    int8_t i_log2_min_transform_block_size;
    int8_t i_log2_max_transform_block_size;
    int8_t i_max_transform_block_size;

    bool b_pcm_enabled;    
    uint8_t i_pcm_bit_depth_luma;
    uint8_t i_pcm_bit_depth_chroma;
    int8_t i_min_pcm_cb_size;
    int8_t i_max_pcm_cb_size;
    
    int8_t i_max_transform_hierarchy_depth_inter;
    int8_t i_max_transform_hierarchy_depth_intra;
    bool b_scaling_list_enabled;
    bool b_scaling_list_present;
    bool b_chroma_pred_from_luma;
    bool b_transform_skip;
    bool b_deblocking_filter_in_aps;
    bool b_lf_across_slice;
    bool ab_amvp[ MAX_CU_DEPTH ];
    bool b_amp;
    bool b_sao;
    bool b_vui_params;
    
    bool b_pcm_loop_filter_disable;
    bool b_temporal_id_nesting;
    
    bool b_strong_intra_smoothing;

    int32_t i_num_short_term_ref_pic_sets;
    bool b_long_term_ref_pics_present;
    int32_t i_num_long_term_ref_pic_sets;
    int32_t ai_ltrefpic_poc_lsb[ 33 ];
    bool ab_ltusedbycurr[ 33 ];
    bool b_temporal_mvp;

    luts_t s_luts;
    scaling_list_t s_scaling_list;
    
    int8_t i_max_cu_depth;
    int16_t i_max_cu_width;
    int16_t i_max_cu_height;

    int32_t i_max_pic_order_cnt_lsb;
    
    int8_t i_add_depth; /** depth beyond CU (i.e. PU/TU) */

    reference_picture_set_t *pps_rps_list;
    reference_picture_t **ppas_ref_pics;
    int32_t i_curr_num_ref_pics;
    int32_t i_alloc_ref_pics;

    bool b_init;
    bool b_allocated;
} sequence_parameter_set_t;

typedef struct {
    uint8_t i_pic_parameter_set_id;
    uint8_t i_seq_parameter_set_id;
    bool b_sign_data_hiding;
    bool b_cabac_init_present;
    int8_t i_ref_l0_default_active;
    int8_t i_ref_l1_default_active;
    int8_t i_pic_init_qp;
    bool b_constrained_intra_pred;
    bool b_transform_skip;
    
    int8_t i_min_dqp_size;
    bool b_use_dqp;

    int32_t i_cb_qp_offset;
    int32_t i_cr_qp_offset;
    bool b_slice_chroma_qp;
    
    bool b_weighted_pred;
    bool b_weighted_bipred;
    bool b_output_flag_present;
    bool b_dependent_slices;
    bool b_transquant_bypass;

    bool b_tiles_enabled;

    int8_t i_tile_columns;
    int8_t i_tile_rows;
    bool b_uniform_spacing;
    bool b_loop_filter_across_tiles;
    int32_t ai_tcol_widths[ MAX_TILE_COUNT ];
    int32_t ai_trow_heights[ MAX_TILE_COUNT ];

    bool b_entropy_coding_sync_enabled;

    bool b_loop_filter_across_slices;
    bool b_deblocking_ctrl;
    bool b_deblocking_override;
    bool b_disable_deblocking;
    int8_t i_lf_beta_offset;
    int8_t i_lf_tc_offset;

    bool b_scaling_list_data;
    scaling_list_t s_scaling_list;
    int8_t i_log2_parallel_merge_level;

    bool b_lists_modification_present;
    int32_t i_num_extra_slice_header_bits;

    bool b_slice_header_extension;
    bool b_extension;

    uint32_t ui_max_dqp_depth;

    uint8_t ui_num_of_sub_streams;

    bool b_isDefined;
} picture_parameter_set_t;

typedef struct 
{
    bool b_fixed_pic_rate_flag;
    bool b_fixed_pic_rate_within_cvs_flag;
    int32_t i_pic_duration_in_tc_minus1;
    int32_t i_elemental_duration_in_tc_minus1;
    bool b_low_delay_hrd;
    int32_t i_cpb_cnt_minus1;
    int32_t ai_bitrate_value[ MAX_CPB_CNT ][ 2 ];
    int32_t ai_cpb_size_value[ MAX_CPB_CNT ][ 2 ];
    int32_t ai_du_cpb_size_value[ MAX_CPB_CNT ][ 2 ];
    int32_t ai_du_bitrate_size_value[ MAX_CPB_CNT ][ 2 ];
    bool b_cbr_flag[ MAX_CPB_CNT ][ 2 ];
    
} hrd_slinfo_t;

typedef struct
{
    bool b_aspect_ratio_info;
    int32_t i_aspect_ratio_idc;
    int32_t i_sar_width;
    int32_t i_sar_height;
    bool b_overscan_info;
    bool b_overscan_appropriate;
    bool b_video_signal_type;
    int32_t i_video_format;
    bool b_video_full_range;
    bool b_colour_description;
    int32_t i_colour_primaries;
    int32_t i_transfer_characteristics;
    int32_t i_matrix_coefficients;
    bool b_chroma_location;
    int32_t i_chroma_sample_loc_top;
    int32_t i_chroma_sample_loc_bottom;
    bool b_neutral_chroma_indication;
    bool b_field_seq;
    bool b_hrd_parameters;
    bool b_bitstream_restriction;
    bool b_tiles_fixed_structure;
    bool b_motion_vectors_over_pic_bounds;
    int32_t i_max_bytes_pp_denom;
    int32_t i_max_bits_pmcu_denom;
    int32_t i_log2_max_mv_lenh;
    int32_t i_log2_max_mv_lenv;
    bool b_timing_info_present_flag;
    bool b_vui_poc_proportional_to_timing_flag;
    int32_t i_vui_num_ticks_poc_diff_one_minus1;
    int32_t i_num_units;
    int32_t i_time_scale;
    bool b_nal_hrd_parameters;
    bool b_vcl_hrd_parameters;
    bool b_sub_pic_cpb_params;
    int32_t i_tick_divisor_minus2;
    int32_t i_du_cpb_removal_delay_length_minus1;
    int32_t i_bitrate_scale;
    int32_t i_cpb_size_scale;
    int32_t i_du_cpb_size_scale;
    int32_t i_initial_cpb_removal_delay_length_minus1;
    bool b_sub_pic_cpb_params_in_pic_timing_sei_flag;
    int32_t    i_dpb_output_delay_du_length_minus1;
    int32_t i_cpb_removal_delay_length_minus1;
    int32_t m_dpbOutputDelayLengthMinus1;
    int32_t i_num_du;
    hrd_slinfo_t as_hrd[ MAX_TLAYER ];

    bool b_frame_field_info;
    bool b_defdisp_window;
    
    int32_t i_min_spatial_segmentation_idc;
    bool b_restricted_ref_pic_lists;

} vui_t;

typedef struct
{
    /** Explicit weighted prediction parameters parsed in slice header, */
    /** or implicit weighted prediction parameters (8 bits depth values). */
    bool b_present;
    int32_t i_log2_weight_denom;
    int32_t i_weight;
    int32_t i_offset;

    /** weighted prediction scaling values built from above parameters (bitdepth scaled): */
    int32_t w, o, offset, shift, round;
} wp_scaling_t;

typedef struct
{
    slice_type_t e_type;
    bool b_dependent; /** aka lightweight_slice_flag aka entropy_slice_flag */
    int32_t i_poc;

    int32_t i_start_cu_addr;
    int32_t i_end_cu_addr;

    int32_t i_cu0;

    bool b_1st_slice;
    bool b_pic_output;
    
    int8_t i_pps_id;

    bool b_deblocking_override;
    bool b_lf_disabled;
    bool b_lf_across_slices;
    int8_t i_lf_beta_offset;
    int8_t i_lf_tc_offset;

    bool b_sao;
    bool b_sao_chroma;
    bool b_sao_interleaving;

    bool b_temporal_mvp;
    int32_t ai_ref_idx_active[ 2 ];   /** list 0,1 */

    bool b_cabac_init;

    bool b_mvd_l1_zero;              /** B-slices */
    bool b_collocated_from_l0;       /** B-slices */
    int8_t i_max_num_merge_cand;
    int32_t i_collocated_ref_idx;

    reference_picture_set_t s_rps_local;   /** needed? */
    reference_picture_set_t *p_rps;

    reference_picture_t *pp_refpic_list[ 2 ][ MAX_NUM_REF ];
    int32_t ai_ref_pocs[ 2 ][ MAX_NUM_REF+1 ];
    
    picture_parameter_set_t *p_pps;
    sequence_parameter_set_t *p_sps;
    scaling_list_t *p_scaling_list;

    int8_t i_qp;
    int8_t i_qp_delta_cb;
    int8_t i_qp_delta_cr;

    reference_picture_t *p_refpic;
    rpl_modification_t s_rpl_modification;
    bool b_used_as_lt[ 2 ][ MAX_NUM_REF+1 ];

    wp_scaling_t as_weight_pred_params[ 2 ][ MAX_NUM_REF ][ 3 ];
    
    bool b_lowdelay;
    int32_t i_temp_hier;

    int32_t i_num_entry_point_offsets;
    int32_t i_num_tile_locations;
    int32_t ai_tile_byte_locations[ MAX_TILE_COUNT ];

    int32_t ai_substream_sizes[ 64 ]; /** null bock for dyn alloc */
    hevc_nalu_type_t e_nalu_type;
} slice_t;

typedef struct {
    int32_t i_level;
    int32_t i_profile_space;
    int32_t i_profile;
    bool b_tier;
    bool b_profile_compat[ 32 ];
    bool sub_layer_profile_present[ 6 ];
    bool sub_layer_level_present[ 6 ];
    struct profile_tier_level_t *as_sublayer_ptl[ 6 ];

    bool b_general_progressive_source;
    bool b_general_interlaced_source;
    bool b_general_non_packed_constraint;
    bool b_general_frame_only_constraint;
} profile_tier_level_t;

typedef struct  
{
    int32_t i_tile_idx;
    int32_t i_offset_idx;
    slice_t *p_slice;
    reference_picture_t *p_rpic;
    bitstream_t *p_bitstream;
    int32_t i_cu_num_partitions;
    int32_t i_cu_start_idx;
    int32_t i_cu_stop_idx;

    bool b_sig_tile_exit;
    bool b_sig_tile_available;

    int32_t i_error;
    int32_t i_cum_cu_idx;

} tile_control_t;


typedef struct  
{
    bool b_error;
    hevc_nalu_t nalu;

#define NUM_MAX_SEQ_PARAM_SETS 32 /** 7.4.2.1 */
    sequence_parameter_set_t as_sps[ NUM_MAX_SEQ_PARAM_SETS ];  /** sequence parameter set */
#define NUM_MAX_PIC_PARAM_SETS 64
    picture_parameter_set_t as_pps[ NUM_MAX_PIC_PARAM_SETS ]; /** picture parameter set */

    video_parameter_set_t s_vps;

    int8_t i_curr_sps_idx;
    int8_t i_curr_pps_idx;

    uint32_t ui_input_size;


    int32_t i_prev_poc;
    int32_t i_prev_tid0_poc;
    int32_t i_poc;
    int32_t i_last_display_poc;

    vui_t s_vui;

    bool b_no_simd;
    bool b_no_threads;

    int8_t i_output_bits;

    bool b_digest_check;
    
    sao_context_t s_sao;
    
    profile_tier_level_t as_protile[ 7 ];

    tile_control_t as_tile_control[ MAX_TILE_COUNT ];

    uint32_t ui_cum_cu;
    uint32_t ui_tgt_cu_cum;
    bool IDR_pic_flag;

    uint64_t poc_offset;
    uint32_t rpu_flag;

    scaling_list_t as_pps_scaling_lists[ NUM_MAX_PIC_PARAM_SETS ];   /** pps scaling lists */

} hevc_decode_t;

void hevc_dec_init(hevc_decode_t *dec);
void decode_vps(hevc_decode_t  *context, hevc_nalu_t *p_nalu );
void decode_sps(hevc_decode_t  *context, hevc_nalu_t *p_nalu );
void decode_pps(hevc_decode_t  *context, hevc_nalu_t *p_nalu );
void decode_vui( hevc_decode_t *context, sequence_parameter_set_t *p_sps, hevc_nalu_t *p_nalu );
bool gop_decode_slice(hevc_decode_t  *context, hevc_nalu_t *p_nalu );
void decode_sei_nalu( hevc_decode_t *p_context, hevc_nalu_t *p_nalu );


void bitstream_init( bitstream_t *bitstream );
uint32_t bitstream_read( bitstream_t *bitstream, uint32_t ui_num_bits );
uint32_t read_input_nalu( bitstream_t *bitstream, hevc_nalu_t *p_nalu);

#ifdef __cplusplus
};
#endif

#endif /* !__PARSER_AVC_DEC_H__ */
