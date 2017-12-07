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
    @file parser_hevc_dec.c
    @brief Implements a lower level hevc parser
*/

#include "utils.h"
#include "io_base.h"
#include "parser_hevc_dec.h"
#include <assert.h>
#include <stdlib.h>

#define long_jump( exception, what )

#define HEVCDEC_EXC_BITSTREAM_END 1
#define HEVCDEC_EXC_SYNTAX_ERROR  2
#define HEVCDEC_EXC_NOT_SUPPORTED 3
#define HEVCDEC_EXC_OUT_OF_MEMORY 4
#define HEVCDEC_EXC_INTERNAL      5

#define SWAP_ENDIAN32(x) ((((uint8_t *)&x)[0] << 24) | \
                         (((uint8_t *)&x)[1] << 16)  | \
                         (((uint8_t *)&x)[2] << 8)   | \
                         (( uint8_t *)&x)[3])  

int32_t gi_max_val_luma = 0;
int32_t gi_max_val_chroma = 0;

extern void parser_avc_remove_0x03(uint8_t *dst, size_t *dstlen, const uint8_t *src, const size_t srclen);

void 
hevcdec_create_context(hevc_decode_t *context)
{
    memset( context, 0, sizeof(hevc_decode_t) );

    context->as_protile[ 0 ].as_sublayer_ptl[ 0 ] = (struct profile_tier_level_t *)&context->as_protile[ 1 ];
    context->as_protile[ 0 ].as_sublayer_ptl[ 1 ] = (struct profile_tier_level_t *)&context->as_protile[ 2 ];
    context->as_protile[ 0 ].as_sublayer_ptl[ 2 ] = (struct profile_tier_level_t *)&context->as_protile[ 3 ];
    context->as_protile[ 0 ].as_sublayer_ptl[ 3 ] = (struct profile_tier_level_t *)&context->as_protile[ 4 ];
    context->as_protile[ 0 ].as_sublayer_ptl[ 4 ] = (struct profile_tier_level_t *)&context->as_protile[ 5 ];
    context->as_protile[ 0 ].as_sublayer_ptl[ 5 ] = (struct profile_tier_level_t *)&context->as_protile[ 6 ];

    context->s_vui.i_video_format = 5;
    context->s_vui.i_colour_primaries = 2;
    context->s_vui.i_transfer_characteristics = 2;
    context->s_vui.i_matrix_coefficients = 2;
    context->s_vui.b_motion_vectors_over_pic_bounds = true;
    context->s_vui.i_max_bytes_pp_denom = 2;
    context->s_vui.i_max_bits_pmcu_denom = 1;
    context->s_vui.i_log2_max_mv_lenh = 15;
    context->s_vui.i_log2_max_mv_lenv = 15;
    context->s_vui.i_num_units = 1001;
    context->s_vui.i_time_scale = 60000;

}

void
hevc_dec_init(hevc_decode_t *dec)
{
    hevcdec_create_context(dec);
}

void 
bitstream_init( bitstream_t *bitstream )
{
    bitstream->ui32_curr_bits = *((int32_t *)( bitstream->pui8_payload ));
    bitstream->ui32_next_bits = *((int32_t *)( bitstream->pui8_payload + 4 ));
    bitstream->ui_byte_position = 4;
    bitstream->ui_bit_idx = 0;
    bitstream->ui32_bits_read = 0;
    bitstream->i64_bits_available = bitstream->ui_length << 3;

    bitstream->ui32_curr_bits = SWAP_ENDIAN32( bitstream->ui32_curr_bits );
    bitstream->ui32_next_bits = SWAP_ENDIAN32( bitstream->ui32_next_bits );
}

uint32_t 
bitstream_read( bitstream_t *bitstream, uint32_t ui_num_bits )
{
    uint32_t ret_val;
    uint32_t ui_bit_pos_coming;

    if( (uint32_t)bitstream->i64_bits_available < ui_num_bits )
        long_jump( HEVCDEC_EXC_BITSTREAM_END, "" );

    ui_bit_pos_coming = bitstream->ui_bit_idx + ui_num_bits;

    if( ui_bit_pos_coming <= 32 )
    {
        uint32_t _fl = (1<<(ui_num_bits-1));
        ret_val = ( ( bitstream->ui32_curr_bits >> ( 32 - ui_num_bits - bitstream->ui_bit_idx ) ) & (_fl|(_fl-1)) );
    }
    else
    {
        uint32_t ui_1st = 32 - bitstream->ui_bit_idx, ui_2nd = ui_bit_pos_coming - 32;
        ret_val = ( bitstream->ui32_curr_bits >> ( 32 - ui_1st - bitstream->ui_bit_idx ) ) & (((1<<ui_1st)|((1<<ui_1st)-1))>>1);
        ret_val <<= ui_2nd;
        ret_val |= ( bitstream->ui32_next_bits >> ( 32 - ui_2nd ) ) & (((1<<ui_2nd)|((1<<ui_2nd)-1))>>1);
    }

    bitstream->ui_bit_idx += ui_num_bits;
    bitstream->ui32_bits_read += ui_num_bits;
    bitstream->i64_bits_available -= ui_num_bits;

    if( bitstream->ui_bit_idx >= 32 )
    {
        if( bitstream->ui_byte_position + 4 >= bitstream->ui_length )
        {
            int8_t i_jdx;

            int32_t i_bytes = (int32_t)bitstream->ui_length - bitstream->ui_byte_position;
            bitstream->ui32_curr_bits = 0x0;
            for( i_jdx=0; i_jdx<i_bytes; ++i_jdx )
            {
                bitstream->ui32_curr_bits |= ( (uint32_t)bitstream->pui8_payload[ bitstream->ui_byte_position++ ] )<<( (i_jdx<<3) );
            }

            bitstream->ui32_curr_bits = SWAP_ENDIAN32( bitstream->ui32_curr_bits );

            bitstream->ui_bit_idx &= 31;
            bitstream->ui32_next_bits = 0x0;
            return ret_val;
        }

        bitstream->ui32_curr_bits = bitstream->ui32_next_bits;
        bitstream->ui_byte_position += 4;
        bitstream->ui32_next_bits = *((int32_t *)( bitstream->pui8_payload + bitstream->ui_byte_position ));
        bitstream->ui_bit_idx &= 31;

        bitstream->ui32_next_bits = SWAP_ENDIAN32( bitstream->ui32_next_bits );
    }

    return ret_val;
}

uint32_t 
bitstream_peek( bitstream_t *bitstream, uint32_t ui_num_bits )
{
    uint32_t retVal = 0;
    
    uint32_t saved0, saved1, saved2, saved3, saved4;
    int64_t saved5;

    saved0 = bitstream->ui_byte_position;
    saved1 = bitstream->ui_bit_idx;
    saved2 = bitstream->ui32_curr_bits;
    saved3 = bitstream->ui32_next_bits;
    saved4 = bitstream->ui32_bits_read;
    saved5 = bitstream->i64_bits_available;

    if( bitstream->i64_bits_available <= 0 ) return 0;
    retVal = bitstream_read( bitstream, ui_num_bits );

    bitstream->ui_byte_position = saved0;
    bitstream->ui_bit_idx        = saved1;
    bitstream->ui32_curr_bits   = saved2;
    bitstream->ui32_next_bits   = saved3;
    bitstream->ui32_bits_read   = saved4;
    bitstream->i64_bits_available = saved5;

    return retVal;
}

bool 
more_rbsp_data( bitstream_t *bitstream )
{ 
    uint8_t ui_last_byte;
    int64_t i_bits_left = bitstream->i64_bits_available;

    if( i_bits_left > 8 )
    {
        return true;
    }

    ui_last_byte = (uint8_t)bitstream_peek( bitstream, (uint32_t)i_bits_left );

    while( i_bits_left > 0 && (ui_last_byte & 1) == 0 )
    {
        ui_last_byte >>= 1;
        i_bits_left--;
    }
    i_bits_left--;

    if( i_bits_left < 0 )
        long_jump( HEVCDEC_EXC_BITSTREAM_END, "" );

    return i_bits_left > 0;
}

bool 
bitstream_byte_aligned( bitstream_t *bitstream )
{
    return ( bitstream->ui_bit_idx & 7 ) == 0;
}

void 
bitstream_byte_align( bitstream_t *bitstream )
{
    if( ( bitstream->ui_bit_idx & 7 ) != 0 )
        bitstream_read( bitstream, 8 - (bitstream->ui_bit_idx & 7) );
}

uint32_t 
read_input_nalu( bitstream_t *bitstream, hevc_nalu_t *p_nalu)
{
    /* cf. B.1 */
    uint32_t ui_code;
    uint8_t *pui8_payload;
    uint32_t num_bytes, reserved_zero_6bits;
    uint32_t ui_consumed0 = bitstream->ui_byte_position - 4 + (bitstream->ui_bit_idx>>3);
    uint8_t *rbsp_bytes = p_nalu->rbsp_buff;
    size_t   rbsp_size = 0;

    p_nalu->b_incomplete = true;

    while( bitstream_peek( bitstream, 24 ) != 0x000001 && bitstream_peek( bitstream, 32 ) != 0x00000001 && bitstream->i64_bits_available > 0  )
    {
        uint8_t leading_zero_8bits;

        leading_zero_8bits = (uint8_t)bitstream_read( bitstream, 8 );
        if( leading_zero_8bits )
            return HEVCDEC_EXC_SYNTAX_ERROR;
    }


    if( 0x000001 != bitstream_peek( bitstream, 24 ) && bitstream->i64_bits_available > 0 )
    {
        uint8_t zero_byte = (uint8_t)bitstream_read( bitstream, 8 );
        if( zero_byte )
            return HEVCDEC_EXC_SYNTAX_ERROR;
    }

    if( bitstream->i64_bits_available <= 0 ) {
        p_nalu->read_nalu_consumed =  bitstream->ui_byte_position - 4 + (bitstream->ui_bit_idx>>3) - ui_consumed0;
        return HEVCDEC_EXC_BITSTREAM_END;
    }

    /* start code prefix */
    ui_code = bitstream_read( bitstream, 24 );
    if( ui_code != 0x000001 )
        return HEVCDEC_EXC_SYNTAX_ERROR;

    pui8_payload = bitstream->pui8_payload + (bitstream->ui32_bits_read >> 3);
    num_bytes = bitstream->ui_length - ( bitstream->ui_byte_position - 4 + ( bitstream->ui_bit_idx >> 3 ));

    p_nalu->ui_bytes_removed = 0;
    p_nalu->ui_num_bytes = num_bytes;

    parser_avc_remove_0x03(rbsp_bytes,&rbsp_size,pui8_payload, MIN2(num_bytes, RBSP_BYTE_NUM_MAX));
    /* for later parsing of RBSP */
    p_nalu->bitstream.pui8_payload = rbsp_bytes;
    p_nalu->bitstream.ui_length = (uint32_t)rbsp_size;
    bitstream_init( &p_nalu->bitstream );

    /* forbidden_zero_bit */
    ui_code = bitstream_read( &p_nalu->bitstream, 1 );
    if( ui_code )
        return HEVCDEC_EXC_SYNTAX_ERROR;

    p_nalu->e_nalu_type = (hevc_nalu_type_t)bitstream_read( &p_nalu->bitstream, 6 );
    reserved_zero_6bits = bitstream_read( &p_nalu->bitstream, 6 );
    if( reserved_zero_6bits )
        return HEVCDEC_EXC_SYNTAX_ERROR;
    p_nalu->i_temporal_id = bitstream_read( &p_nalu->bitstream, 3 ) - 1;

    p_nalu->read_nalu_consumed = bitstream->ui_byte_position - 4 + (bitstream->ui_bit_idx>>3) - ui_consumed0;
    return 0;
}

uint32_t 
bitstream_read_uvlc( bitstream_t *bitstream )
{
    int32_t i_val = 0, i_length;
    int32_t i_code = bitstream_read( bitstream, 1 );
    
    if( 0 == i_code )
    {
        i_length = 0;

        while( !( i_code & 1 ) )
        {
            i_code = bitstream_read( bitstream, 1 );
            i_length++;
        }

        i_val = bitstream_read( bitstream, i_length );
        i_val += (1 << i_length) - 1;
    }

    return i_val;
}


int32_t 
bitstream_read_svlc( bitstream_t *bitstream )
{
    int32_t i_bits = bitstream_read( bitstream, 1 );
    
    if( 0 == i_bits )
    {
        int32_t i_length = 0;

        while( !( i_bits & 1 ) )
        {
            i_bits = bitstream_read( bitstream, 1 );
            i_length++;
        }

        i_bits = bitstream_read( bitstream, i_length );

        i_bits += 1 << i_length;
        return ( i_bits & 1 ) ? -(i_bits>>1) : (i_bits>>1);
    }

    return 0;
}

void 
parse_profile_tier( bitstream_t *p_bitstream, profile_tier_level_t *p_ptl )
{
    int32_t i_idx;

    p_ptl->i_profile_space = bitstream_read( p_bitstream, 2 );
    p_ptl->b_tier = bitstream_read( p_bitstream, 1 );
    p_ptl->i_profile = bitstream_read( p_bitstream, 5 );

    for( i_idx=0; i_idx<32; i_idx++ )
        p_ptl->b_profile_compat[ i_idx ] = bitstream_read( p_bitstream, 1 );

    p_ptl->b_general_progressive_source = bitstream_read( p_bitstream, 1 );
    p_ptl->b_general_interlaced_source = bitstream_read( p_bitstream, 1 );
    p_ptl->b_general_non_packed_constraint = bitstream_read( p_bitstream, 1 );
    p_ptl->b_general_frame_only_constraint = bitstream_read( p_bitstream, 1 );

    bitstream_read( p_bitstream, 16 ); /* "XXX_reserved_zero_44bits[0..15]");  */
    bitstream_read( p_bitstream, 16 ); /* "XXX_reserved_zero_44bits[16..31]"); */
    bitstream_read( p_bitstream, 12 ); /* "XXX_reserved_zero_44bits[32..43]"); */
}


void 
parse_ptl( bitstream_t *p_bitstream, profile_tier_level_t *p_ptl, bool b_profile_present, int32_t i_max_sublayers_minus1 )
{
    int32_t i_idx;

    if( b_profile_present )
        parse_profile_tier( p_bitstream, p_ptl );

    p_ptl->i_level = bitstream_read( p_bitstream, 8 );

    for( i_idx=0; i_idx < i_max_sublayers_minus1; i_idx++ )
    {
        if( b_profile_present )
            p_ptl->sub_layer_profile_present[ i_idx ] = bitstream_read( p_bitstream, 1 );
        p_ptl->sub_layer_level_present[ i_idx ] = bitstream_read( p_bitstream, 1 );
    }

    if( i_max_sublayers_minus1 > 0 )
    {
        for( i_idx=i_max_sublayers_minus1; i_idx < 8; i_idx++ )
        {
            int32_t x = bitstream_read( p_bitstream, 2 ); /* reserved_zero_2bits */
            if( x )
                long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "reserved_zero_2bits in PTL" );
        }
    }


    for( i_idx=0; i_idx < i_max_sublayers_minus1; i_idx++ )
    {
        if( b_profile_present && p_ptl->sub_layer_profile_present[ i_idx ] )
            parse_profile_tier( p_bitstream, (profile_tier_level_t *)p_ptl->as_sublayer_ptl[ i_idx ] );

        if( p_ptl->sub_layer_level_present[ i_idx ] )
            ((profile_tier_level_t *)p_ptl->as_sublayer_ptl[ i_idx ])->i_level = bitstream_read( p_bitstream, 8 );
    }
}

void 
parse_bitrate_picrate_info( bitstream_t *p_bitstream, bit_rate_picrate_info_t *info, int32_t tempLevelLow, int32_t tempLevelHigh )
{
    int32_t i_idx;
    
    for( i_idx = tempLevelLow; i_idx <= tempLevelHigh; i_idx++ )
    {
        info->m_bitRateInfoPresentFlag[ i_idx ] = bitstream_read( p_bitstream, 1 );
        info->m_picRateInfoPresentFlag[ i_idx ] = bitstream_read( p_bitstream, 1 );
        
        if( info->m_bitRateInfoPresentFlag[ i_idx ] )
        {
            info->m_avgBitRate[ i_idx ] = bitstream_read( p_bitstream, 16 );
            info->m_maxBitRate[ i_idx ] = bitstream_read( p_bitstream, 16 );
        }

        if( info->m_picRateInfoPresentFlag[ i_idx ] )
        {
            info->m_constantPicRateIdc[ i_idx ] = bitstream_read( p_bitstream, 2 );
            info->m_avgPicRate[ i_idx ] = bitstream_read( p_bitstream, 16 );
        }
    }
}

void 
decode_vps(hevc_decode_t  *context, hevc_nalu_t *p_nalu ) 
{
    int32_t i_idx, opIdx;
    bool b_subLayerOrderingInfoPresentFlag;
    video_parameter_set_t *p_vps = &context->s_vps;

    p_vps->i_id = bitstream_read( &p_nalu->bitstream, 4 );  /* video_parameter_set_id */
    bitstream_read( &p_nalu->bitstream, 2 );                /* vps_reserved_three_2bits */
    bitstream_read( &p_nalu->bitstream, 6 );                /* vps_reserved_zero_6bits */
    p_vps->i_max_temporal_layers = 1 + bitstream_read( &p_nalu->bitstream, 3 );
    p_vps->b_temporal_id_nesting = bitstream_read( &p_nalu->bitstream, 1 );

    i_idx = bitstream_read( &p_nalu->bitstream, 16 );       /* vps_reserved_ffff_16bits */ 
    assert( i_idx == 0xffff );

    parse_ptl( &p_nalu->bitstream, &context->as_protile[ 0 ], true, p_vps->i_max_temporal_layers - 1 );

    b_subLayerOrderingInfoPresentFlag = bitstream_read( &p_nalu->bitstream, 1 ); /* "vps_sub_layer_ordering_info_present_flag" */;

    for( i_idx=0; i_idx<p_vps->i_max_temporal_layers; i_idx++ )
    {
        p_vps->ai_max_dec_pic_buffering[ i_idx ] = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vps->ai_num_reorder_pics[ i_idx ] = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vps->ai_max_latency_increase[ i_idx ] = bitstream_read_uvlc( &p_nalu->bitstream );

        if( !b_subLayerOrderingInfoPresentFlag )
        {
            for( i_idx++; i_idx<p_vps->i_max_temporal_layers; ++i_idx )
            {
                p_vps->ai_max_dec_pic_buffering[ i_idx ] = 1 + p_vps->ai_max_dec_pic_buffering[ 0 ];
                p_vps->ai_num_reorder_pics[ i_idx ] = p_vps->ai_num_reorder_pics[ 0 ];
                p_vps->ai_max_latency_increase[ i_idx ] = p_vps->ai_max_latency_increase[ 0 ];
            }
            break;
        }
    }

    p_vps->i_vps_max_nuh_reserved_zero_layer_id = bitstream_read( &p_nalu->bitstream, 6 );
    p_vps->i_vps_max_op_sets = bitstream_read_uvlc( &p_nalu->bitstream );

    assert( p_vps->i_num_hrd_params < MAX_VPS_OP_SETS_PLUS1 );
    assert( p_vps->i_vps_max_nuh_reserved_zero_layer_id < MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1 );

    for( opIdx = 1; opIdx<p_vps->i_vps_max_op_sets; opIdx++ )
    {
        for( i_idx = 0; i_idx<=p_vps->i_vps_max_nuh_reserved_zero_layer_id; i_idx++ )
        {
            p_vps->ab_oplayer_id_included[ opIdx ][ i_idx ] = bitstream_read( &p_nalu->bitstream, 1 );
        }
    }

    p_vps->b_vps_timing_info_present_flag = bitstream_read( &p_nalu->bitstream, 1 );

    if( p_vps->b_vps_timing_info_present_flag )
    {
        p_vps->ui_vps_num_units_in_tick = bitstream_read( &p_nalu->bitstream, 32 );
        p_vps->ui_vps_time_scale = bitstream_read( &p_nalu->bitstream, 32 );
        p_vps->b_vps_poc_proportional_to_timing_flag = bitstream_read( &p_nalu->bitstream, 1 );

        if( p_vps->b_vps_poc_proportional_to_timing_flag )
        {
            p_vps->i_vps_num_ticks_poc_diff_one_minus1 = bitstream_read_uvlc( &p_nalu->bitstream );
        }
        else
        {
            p_vps->i_vps_num_ticks_poc_diff_one_minus1 = 0;
        }

        p_vps->i_num_hrd_params = bitstream_read_uvlc( &p_nalu->bitstream );

    }

    /* vps_extension_flag */
    if( (p_vps->b_extension = bitstream_read( &p_nalu->bitstream, 1 )) != false )
    {
        /* vps_extension_data_flag */
        while( more_rbsp_data( &p_nalu->bitstream ) )    
            bitstream_read( &p_nalu->bitstream, 1 );
    }
    p_vps->b_isDefined= true;
}

void 
decode_short_term_rps( bitstream_t *bitstream, int32_t idx, reference_picture_set_t *p_rps, reference_picture_set_t *p_sets, sequence_parameter_set_t *p_sps )
{
    int32_t i_idx, i_jdx;

    memset( p_rps, 0x0, sizeof(reference_picture_set_t) );
    
    if( idx > 0 )
        p_rps->b_inter_rps_prediction = bitstream_read( bitstream, 1 );  /* inter_ref_pic_set_prediction_flag */
    else
        p_rps->b_inter_rps_prediction = false;

    if( p_rps->b_inter_rps_prediction )
    {
        int32_t i_ridx, i_bit, i_code;
        int32_t k = 0, k0 = 0, k1 = 0, i_delta_rps;
        reference_picture_set_t *p_rps_ref;
        
        if( idx == p_sps->i_num_short_term_ref_pic_sets )
            i_code = bitstream_read_uvlc( bitstream );      /* delta_idx_minus1 delta index of the Reference Picture Set used for prediction minus 1 */
        else
            i_code = 0;
        
        /** delta_idx_minus1 shall not be larger than idx-1, otherwise we will predict from a negative row 
            position that does not exist. When idx equals 0 there is no legal value and interRPSPred must be zero. See J0185-r2 */
        assert( i_code <= idx-1 );
        
        i_ridx = idx - 1 - i_code;
        assert( i_ridx <= idx-1 && i_ridx >= 0 ); /* if rIdx = idx then prediction is done from itself. rIdx must belong to range 0, idx-1, inclusive, see J0185-r2 */

        p_rps_ref = &p_sets[ i_ridx ];
        
        i_bit = bitstream_read( bitstream, 1 );        /* delta_rps_sign */
        i_code = bitstream_read_uvlc( bitstream );     /* abs_delta_rps_minus1 */
        i_delta_rps = (1 - (i_bit<<1)) * (i_code + 1);

        for( i_idx=0; i_idx <= p_rps_ref->i_num_pictures; i_idx++ )
        {
            int32_t i_ref_idc;
            i_bit = bitstream_read( bitstream, 1 ); /* used_by_curr_pic_flag */
            
            i_ref_idc = i_bit;
            if( !i_ref_idc )
            {
                i_bit = bitstream_read( bitstream, 1 ); /* use_delta_flag */
                i_ref_idc = i_bit << 1;
            }

            if( i_ref_idc == 1 || i_ref_idc == 2 )
            {
                int32_t i_delta_poc = i_delta_rps + ( i_idx < p_rps_ref->i_num_pictures ? p_rps_ref->ai_delta_poc[ i_idx ] : 0 );
                
                p_rps->ai_delta_poc[ k ] = i_delta_poc;
                p_rps->ab_used[ k ] = i_ref_idc == 1;
                
                if( i_delta_poc < 0 )
                    k0++;
                else 
                    k1++;
                k++;
            }
            p_rps->ai_ref_idc[ i_idx ] = i_ref_idc;
        }
        p_rps->i_num_ref_idc = p_rps_ref->i_num_pictures + 1;
        p_rps->i_num_pictures = k;
        p_rps->i_num_negativePictures = k0;
        p_rps->i_num_positivePictures = k1;

        for( i_jdx=1; i_jdx < p_rps->i_num_pictures; i_jdx++ )
        {
            int32_t i_delta_poc = p_rps->ai_delta_poc[ i_jdx ];
            bool b_used = p_rps->ab_used[ i_jdx ];
            for( k=i_jdx-1; k>=0; k-- )
            {
                int32_t i_temp = p_rps->ai_delta_poc[ k ];
                if( i_delta_poc < i_temp )
                {
                    p_rps->ai_delta_poc[ k+1 ] = i_temp;
                    p_rps->ab_used[ k+1 ] = p_rps->ab_used[ k ];
                    p_rps->ai_delta_poc[ k ] = i_delta_poc;
                    p_rps->ab_used[ k ] = b_used;
                }
            }
        }

        for( i_jdx=0, k=p_rps->i_num_negativePictures-1; i_jdx < p_rps->i_num_negativePictures>>1; i_jdx++, k-- )
        {
            int32_t i_delta_poc = p_rps->ai_delta_poc[ i_jdx ];
            bool b_used = p_rps->ab_used[ i_jdx ];

            p_rps->ai_delta_poc[ i_jdx ] = p_rps->ai_delta_poc[ k ];
            p_rps->ab_used[ i_jdx ] = p_rps->ab_used[ k ];
            p_rps->ai_delta_poc[ k ] = i_delta_poc;
            p_rps->ab_used[ k ] = b_used;
        }
    }
    else
    {
        int32_t i_prev = 0, i_poc;
        int32_t i_code;

        p_rps->i_num_negativePictures = bitstream_read_uvlc( bitstream );
        p_rps->i_num_positivePictures = bitstream_read_uvlc( bitstream );
        
        for( i_idx=0 ; i_idx < p_rps->i_num_negativePictures; i_idx++ )
        {
            i_code = bitstream_read_uvlc( bitstream );  /* delta_poc_s0_minus1 */
            i_poc = i_prev - i_code - 1;
            i_prev = i_poc;
            p_rps->ai_delta_poc[ i_idx ] = i_poc;
            p_rps->ab_used[ i_idx ] = bitstream_read( bitstream, 1 );   /* used_by_curr_pic_s0_flag */
        }

        i_prev = 0;
        for( i_idx=p_rps->i_num_negativePictures; i_idx < p_rps->i_num_negativePictures+p_rps->i_num_positivePictures; i_idx++ )
        {
            i_code = bitstream_read_uvlc( bitstream );    /* delta_poc_s1_minus1 */
            i_poc = i_prev + i_code + 1;
            i_prev = i_poc;
            p_rps->ai_delta_poc[ i_idx ] = i_poc;
            p_rps->ab_used[ i_idx ] = bitstream_read( bitstream, 1 );  /* used_by_curr_pic_s1_flag */
        }
        p_rps->i_num_pictures = p_rps->i_num_negativePictures + p_rps->i_num_positivePictures;
    }
}

void 
on_got_sps( sequence_parameter_set_t *p_sps_new, hevc_decode_t *p_context )
{
    sequence_parameter_set_t *p_sps = &p_context->as_sps[ p_sps_new->i_id ];
    
    if( p_sps->b_init )
    {
        return;
    }

    memcpy( p_sps, p_sps_new, sizeof(sequence_parameter_set_t) );

}

void decode_vui( hevc_decode_t *context, sequence_parameter_set_t *p_sps, hevc_nalu_t *p_nalu )
{
    vui_t *p_vui = &context->s_vui;

    p_vui->b_aspect_ratio_info = bitstream_read( &p_nalu->bitstream, 1 );  /* aspect_ratio_info_present_flag */

    if( p_vui->b_aspect_ratio_info )
    {
        p_vui->i_aspect_ratio_idc = bitstream_read( &p_nalu->bitstream, 8 );  /* aspect_ratio_idc */
        if( p_vui->i_aspect_ratio_idc == 255 )
        {
            p_vui->i_sar_width = bitstream_read( &p_nalu->bitstream, 16 );
            p_vui->i_sar_height = bitstream_read( &p_nalu->bitstream, 16 );
        }
    }

    p_vui->b_overscan_info = bitstream_read( &p_nalu->bitstream, 1 );  /* overscan_info_present_flag */

    if( p_vui->b_overscan_info )
        p_vui->b_overscan_appropriate = bitstream_read( &p_nalu->bitstream, 1 );

    p_vui->b_video_signal_type = bitstream_read( &p_nalu->bitstream, 1 );

    if( p_vui->b_video_signal_type )
    {
        p_vui->i_video_format = bitstream_read( &p_nalu->bitstream, 3 );
        p_vui->b_video_full_range = bitstream_read( &p_nalu->bitstream, 1 );
        p_vui->b_colour_description = bitstream_read( &p_nalu->bitstream, 1 );
        if( p_vui->b_colour_description )
        {
            p_vui->i_colour_primaries = bitstream_read( &p_nalu->bitstream, 8 );
            p_vui->i_transfer_characteristics = bitstream_read( &p_nalu->bitstream, 8 );
            p_vui->i_matrix_coefficients = bitstream_read( &p_nalu->bitstream, 8 );
        }
    }

    p_vui->b_chroma_location = bitstream_read( &p_nalu->bitstream, 1 );
    if( p_vui->b_chroma_location )
    {
        p_vui->i_chroma_sample_loc_top = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vui->i_chroma_sample_loc_bottom = bitstream_read_uvlc( &p_nalu->bitstream );
    }

    p_vui->b_neutral_chroma_indication = bitstream_read( &p_nalu->bitstream, 1 );
    p_vui->b_field_seq = bitstream_read( &p_nalu->bitstream, 1 );


    p_vui->b_frame_field_info = bitstream_read( &p_nalu->bitstream, 1 );    /* frame_field_info_present_flag */
    p_vui->b_defdisp_window = bitstream_read( &p_nalu->bitstream, 1 );      /* default_display_window_flag */

    if( p_vui->b_defdisp_window )
    {
        bitstream_read_uvlc( &p_nalu->bitstream ); /** l */
        bitstream_read_uvlc( &p_nalu->bitstream ); /** r */
        bitstream_read_uvlc( &p_nalu->bitstream ); /** t */
        bitstream_read_uvlc( &p_nalu->bitstream ); /** b */
    }

    p_vui->b_timing_info_present_flag = bitstream_read( &p_nalu->bitstream, 1 );

    if( p_vui->b_timing_info_present_flag )
    {
        p_vui->i_num_units = bitstream_read( &p_nalu->bitstream, 32 );
        p_vui->i_time_scale = bitstream_read( &p_nalu->bitstream, 32 );
        p_vui->b_vui_poc_proportional_to_timing_flag = bitstream_read( &p_nalu->bitstream, 1 );

        if( p_vui->b_vui_poc_proportional_to_timing_flag )
            p_vui->i_vui_num_ticks_poc_diff_one_minus1 = bitstream_read_uvlc( &p_nalu->bitstream );
        
        p_vui->b_hrd_parameters = bitstream_read( &p_nalu->bitstream, 1 );

        if( p_vui->b_hrd_parameters )
        {
            int32_t i_idx, i_jdx, i_nal_or_vcl;
            p_vui->b_nal_hrd_parameters = bitstream_read( &p_nalu->bitstream, 1 );
            p_vui->b_vcl_hrd_parameters = bitstream_read( &p_nalu->bitstream, 1 );


            if( p_vui->b_nal_hrd_parameters || p_vui->b_vcl_hrd_parameters )
            {
                p_vui->b_sub_pic_cpb_params = bitstream_read( &p_nalu->bitstream, 1 );
                if( p_vui->b_sub_pic_cpb_params )
                {
                    p_vui->i_tick_divisor_minus2 = bitstream_read( &p_nalu->bitstream, 8 );
                    p_vui->i_du_cpb_removal_delay_length_minus1 = bitstream_read( &p_nalu->bitstream, 5 );
                    p_vui->b_sub_pic_cpb_params_in_pic_timing_sei_flag = bitstream_read( &p_nalu->bitstream, 1 );
                    p_vui->i_dpb_output_delay_du_length_minus1 = bitstream_read( &p_nalu->bitstream, 5 );
                }

                p_vui->i_bitrate_scale = bitstream_read( &p_nalu->bitstream, 4 );
                p_vui->i_cpb_size_scale = bitstream_read( &p_nalu->bitstream, 4 );

                if( p_vui->b_sub_pic_cpb_params )
                    p_vui->i_du_cpb_size_scale = bitstream_read( &p_nalu->bitstream, 4 ); /* du_cpb_size_scale */
                
                p_vui->i_initial_cpb_removal_delay_length_minus1 = bitstream_read( &p_nalu->bitstream, 5 );
                p_vui->i_cpb_removal_delay_length_minus1 = bitstream_read( &p_nalu->bitstream, 5 );    
                p_vui->m_dpbOutputDelayLengthMinus1 = bitstream_read( &p_nalu->bitstream, 5 );
            }

            for( i_idx = 0; i_idx < p_sps->i_max_temporal_layers; i_idx++ )
            {
                p_vui->as_hrd[ i_idx ].b_fixed_pic_rate_flag = bitstream_read( &p_nalu->bitstream, 1 );

                if( !p_vui->as_hrd[ i_idx ].b_fixed_pic_rate_flag )
                    p_vui->as_hrd[ i_idx ].b_fixed_pic_rate_within_cvs_flag = bitstream_read( &p_nalu->bitstream, 1 );
                else
                    p_vui->as_hrd[ i_idx ].b_fixed_pic_rate_within_cvs_flag = true;
                
                p_vui->as_hrd[ i_idx ].b_low_delay_hrd = false;
                p_vui->as_hrd[ i_idx ].i_cpb_cnt_minus1 = 0;

                if( p_vui->as_hrd[ i_idx ].b_fixed_pic_rate_within_cvs_flag )
                    p_vui->as_hrd[ i_idx ].i_elemental_duration_in_tc_minus1 = bitstream_read_uvlc( &p_nalu->bitstream );
                else
                    p_vui->as_hrd[ i_idx ].b_low_delay_hrd = bitstream_read( &p_nalu->bitstream, 1 );
                
                if( !p_vui->as_hrd[ i_idx ].b_low_delay_hrd )
                    p_vui->as_hrd[ i_idx ].i_cpb_cnt_minus1 = bitstream_read_uvlc( &p_nalu->bitstream );
                
                for( i_nal_or_vcl = 0; i_nal_or_vcl < 2; i_nal_or_vcl++ )
                {
                    if( ( ( i_nal_or_vcl == 0 ) && ( p_vui->b_nal_hrd_parameters ) ) ||
                        ( ( i_nal_or_vcl == 1 ) && ( p_vui->b_vcl_hrd_parameters ) ) )
                    {
                        for( i_jdx = 0; i_jdx < ( p_vui->as_hrd[ i_idx ].i_cpb_cnt_minus1 + 1 ); i_jdx++ )
                        {
                            p_vui->as_hrd[ i_idx ].ai_bitrate_value[ i_jdx ][ i_nal_or_vcl ] = 1 + bitstream_read_uvlc( &p_nalu->bitstream );
                            p_vui->as_hrd[ i_idx ].ai_cpb_size_value[ i_jdx ][ i_nal_or_vcl ] = 1 + bitstream_read_uvlc( &p_nalu->bitstream );

                            if( p_vui->b_sub_pic_cpb_params )
                            {
                                p_vui->as_hrd[ i_idx ].ai_du_cpb_size_value[ i_jdx ][ i_nal_or_vcl ] = bitstream_read_uvlc( &p_nalu->bitstream ); /* du_cpb_size_value_minus1 */
                                p_vui->as_hrd[ i_idx ].ai_du_bitrate_size_value[ i_jdx ][ i_nal_or_vcl ] = bitstream_read_uvlc( &p_nalu->bitstream );
                            }

                            p_vui->as_hrd[ i_idx ].b_cbr_flag[ i_jdx ][ i_nal_or_vcl ] = bitstream_read( &p_nalu->bitstream, 1 );
                        }
                    }
                }
            }
        }
    } /* timing info present */

    p_vui->b_bitstream_restriction = bitstream_read( &p_nalu->bitstream, 1 );

    if( p_vui->b_bitstream_restriction )
    {
        p_vui->b_tiles_fixed_structure = bitstream_read( &p_nalu->bitstream, 1 );
        p_vui->b_motion_vectors_over_pic_bounds = bitstream_read( &p_nalu->bitstream, 1 );
        p_vui->b_restricted_ref_pic_lists = bitstream_read( &p_nalu->bitstream, 1 );
        p_vui->i_min_spatial_segmentation_idc = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vui->i_max_bytes_pp_denom = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vui->i_max_bits_pmcu_denom = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vui->i_log2_max_mv_lenh = bitstream_read_uvlc( &p_nalu->bitstream );
        p_vui->i_log2_max_mv_lenv = bitstream_read_uvlc( &p_nalu->bitstream );
    }
}

void hevcdecoder_free( void *ptr )
{
    if( !ptr ) return;
    free((void*)(*((uintptr_t*)((uintptr_t)ptr - sizeof(uintptr_t)))));
}

void sao_destroy_context( sao_context_t *p_sao )
{
    if( !p_sao->pi_clip_luma ) return;

    free( p_sao->pui16_top1 );
    free( p_sao->pui16_top2 );
    free( p_sao->pui16_left1 );
    free( p_sao->pui16_left2 );
    free( p_sao->pi_bo_luma );
    free( p_sao->pi_bo_chroma );
    free( p_sao->pi_clip_luma - (((1<<p_sao->i_bits_luma)-1)>>1) );
    free( p_sao->pi_clip_chroma - (((1<<p_sao->i_bits_chroma)-1)>>1) );
    free( p_sao->pi_bo_offsets );

    hevcdecoder_free( p_sao->pui16_all_buffer );

    memset( p_sao, 0x0, sizeof(sao_context_t) );
}



void sao_create_context( sao_context_t *p_sao, int32_t i_bits_luma, int32_t i_bits_chroma, int32_t i_picture_width )
{
    int32_t i_idx = 0;

    int32_t i_max_luma = (1<<i_bits_luma)-1;
    int32_t i_max_chroma = (1<<i_bits_chroma)-1;

    sao_destroy_context( p_sao );

    p_sao->i_bits_luma = i_bits_luma;
    p_sao->i_bits_chroma = i_bits_chroma;
    p_sao->i_bit_increase_luma = p_sao->i_bits_luma - HEVC_MIN( p_sao->i_bits_luma, 10 );
    p_sao->i_bit_increase_chroma = p_sao->i_bits_chroma - HEVC_MIN( p_sao->i_bits_chroma, 10 );

    p_sao->pi_bo_offsets = (int32_t *)malloc( (i_max_luma + ((i_max_luma>>1)<<1)) * sizeof(int32_t) );
    
    p_sao->pi_bo_luma = (int32_t *)malloc( sizeof(int32_t) * ((1LL<<p_sao->i_bits_luma) + 1) );
    for( i_idx=0; i_idx < 1<<p_sao->i_bits_luma; i_idx++ )
        p_sao->pi_bo_luma[ i_idx ] = 1 + ( i_idx >> (p_sao->i_bits_luma - SAO_BO_BITS) );

    p_sao->pi_bo_chroma = (int32_t *)malloc( sizeof(int32_t) * ((1LL<<p_sao->i_bits_chroma) + 1) );
    for( i_idx=0; i_idx < 1<<p_sao->i_bits_chroma; i_idx++ )
        p_sao->pi_bo_chroma[ i_idx ] = 1 + ( i_idx >> (p_sao->i_bits_chroma - SAO_BO_BITS) );

    p_sao->pui16_left1 = (uint16_t *)malloc( 65 * sizeof(uint16_t) );
    p_sao->pui16_left2 = (uint16_t *)malloc( 65 * sizeof(uint16_t) );
    p_sao->pui16_top1 = (uint16_t *)malloc( i_picture_width * sizeof(uint16_t) );
    p_sao->pui16_top2 = (uint16_t *)malloc( i_picture_width * sizeof(uint16_t) );

    i_idx = 0;
    p_sao->pi_clip_luma = (int32_t *)malloc( (i_max_luma + ((i_max_luma>>1)<<1)) * sizeof(int32_t) );
    for( ; i_idx<i_max_luma>>1; i_idx++)                    p_sao->pi_clip_luma[ i_idx ] = 0;
    for( ; i_idx<(i_max_luma + (i_max_luma>>1)); i_idx++ )  p_sao->pi_clip_luma[ i_idx ] = i_idx - ( i_max_luma>>1 );
    for( ; i_idx<i_max_luma+((i_max_luma>>1)<<1); i_idx++ ) p_sao->pi_clip_luma[ i_idx ] = i_max_luma;
    p_sao->pi_clip_luma += i_max_luma>>1;

    i_idx = 0;
    p_sao->pi_clip_chroma = (int32_t *)malloc( (i_max_chroma + ((i_max_chroma>>1)<<1)) * sizeof(int32_t) );
    for( ; i_idx<i_max_chroma>>1; i_idx++)                    p_sao->pi_clip_chroma[ i_idx ] = 0;
    for( ; i_idx<(i_max_chroma + (i_max_chroma>>1)); i_idx++ )  p_sao->pi_clip_chroma[ i_idx ] = i_idx - ( i_max_chroma>>1 );
    for( ; i_idx<i_max_chroma+((i_max_chroma>>1)<<1); i_idx++ ) p_sao->pi_clip_chroma[ i_idx ] = i_max_chroma;
    p_sao->pi_clip_chroma += i_max_chroma>>1;
}

int32_t g_quant_ts_default_4x4[16] =
{
    16,16,16,16,
    16,16,16,16,
    16,16,16,16,
    16,16,16,16
};

int32_t g_quant_intra_default_8x8[64] =
{
    16,16,16,16,17,18,21,24,
    16,16,16,16,17,19,22,25,
    16,16,17,18,20,22,25,29,
    16,16,18,21,24,27,31,36,
    17,17,20,24,30,35,41,47,
    18,19,22,27,35,44,54,65,
    21,22,25,31,41,54,70,88,
    24,25,29,36,47,65,88,115
};

int32_t g_quantInterDefault8x8[64] =
{
    16,16,16,16,17,18,20,24,
    16,16,16,17,18,20,24,25,
    16,16,17,18,20,24,25,28,
    16,17,18,20,24,25,28,33,
    17,18,20,24,25,28,33,41,
    18,20,24,25,28,33,41,54,
    20,24,25,28,33,41,54,71,
    24,25,28,33,41,54,71,91
};
uint32_t g_scanDiag4x4[ 16 ] = {
    0, 4, 1, 8, 
    5, 2, 12, 9, 
    6, 3, 13, 10, 
    7, 14, 11, 15, 
};

int32_t g_scalingListSize[ 4 ] = { 16, 64, 256, 1024 }; 
int32_t gai_scaling_list_size_x[ 4 ] = { 4, 8, 16, 32 };
int32_t gai_scaling_list_num[ SCALING_LIST_SIZE_NUM ] = { 6, 6, 6, 2 };
int32_t g_eTTable[ 4 ] = { 0, 3, 1, 2 };

int32_t *scaling_list_default_address( uint32_t ui_size_idx, uint32_t ui_list )
{
    switch( ui_size_idx )
    {
    case SCALING_LIST_4x4:   return g_quant_ts_default_4x4;
    case SCALING_LIST_8x8:   return ui_list < 3 ? g_quant_intra_default_8x8 : g_quantInterDefault8x8;
    case SCALING_LIST_16x16: return ui_list < 3 ? g_quant_intra_default_8x8 : g_quantInterDefault8x8;
    default:
    case SCALING_LIST_32x32: return ui_list < 1 ? g_quant_intra_default_8x8 : g_quantInterDefault8x8;
    }
}

void decode_scaling_list( scaling_list_t *p_scaling_list, bitstream_t *bitstream, luts_t *p_luts )
{
    uint32_t ui_size;
    int32_t i_code, i_list_idx;

    for( ui_size = SCALING_LIST_4x4; ui_size < SCALING_LIST_SIZE_NUM; ui_size++ )
    {
        for( i_list_idx = 0; i_list_idx <  gai_scaling_list_num[ui_size]; i_list_idx++ )
        {
            /* scaling_list_pred_mode_flag */
            if( !bitstream_read( bitstream, 1 ) )
            {
                i_code = bitstream_read_uvlc( bitstream ); /* scaling_list_pred_matrix_id_delta */
                p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] = (uint32_t)( i_list_idx - i_code );

                if( (uint32_t)p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] > SCALING_LIST_NUM )
                    long_jump( HEVCDEC_EXC_INTERNAL, "Invalid scaling list" );    
                
                if( ui_size > SCALING_LIST_8x8 )
                {
                    p_scaling_list->ai_scaling_list_dc[ ui_size ][ i_list_idx ] = i_list_idx == p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] ? 16
                        : p_scaling_list->ai_scaling_list_dc[ ui_size ][ p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] ];
                }

                memcpy( p_scaling_list->ai_scaling_list_coeff[ ui_size ][ i_list_idx ],
                    i_list_idx == p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ]
                ? scaling_list_default_address( ui_size, p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] )
                    : p_scaling_list->ai_scaling_list_coeff[ ui_size ][ p_scaling_list->ai_ref_matrix_idx[ ui_size ][ i_list_idx ] ],
                    sizeof(int32_t) * HEVC_MIN( MAX_MATRIX_COEF_NUM, g_scalingListSize[ ui_size ] )
                    );

            }
            else /* dpcm */
            {
                int32_t i_idx, i_data;
                int32_t i_coef_n = HEVC_MIN( MAX_MATRIX_COEF_NUM, g_scalingListSize[ ui_size ] );
                int32_t i_next = SCALING_LIST_START_VALUE;
                uint32_t *pui_scan = ui_size == SCALING_LIST_4x4 ? g_scanDiag4x4 : p_luts->aui32_sig_last_scan_cg_32x32;
                int32_t *pi_dst = p_scaling_list->ai_scaling_list_coeff[ ui_size ][ i_list_idx ];

                if( ui_size > SCALING_LIST_8x8 )
                {
                    /* scaling_list_dc_coef_minus8 */
                    i_next = p_scaling_list->ai_scaling_list_dc[ui_size][i_list_idx] = 8 + bitstream_read_svlc( bitstream );
                }

                for( i_idx = 0; i_idx < i_coef_n; i_idx++ )
                {
                    i_data = bitstream_read_svlc( bitstream ); /* scaling_list_delta_coef */
                    i_next = ( i_next + i_data + 256 ) & 0xff; 
                    pi_dst[ pui_scan[i_idx] ] = i_next;
                }
            }
        }
    }
}

void 
decode_sps( hevc_decode_t *context, hevc_nalu_t *p_nalu ) 
{
    int32_t i_idx;
    bool b_subLayerOrderingInfoPresentFlag;

    sequence_parameter_set_t s_sps;
    sequence_parameter_set_t *sps = &s_sps;
    memset( sps, 0x0, sizeof(sequence_parameter_set_t) );

    sps->i_vps_id = (int8_t)bitstream_read( &p_nalu->bitstream, 4 );                    /* video_parameter_set_id */
    sps->i_max_temporal_layers = (int8_t)(1 + bitstream_read( &p_nalu->bitstream, 3 )); /* sps_max_sub_layers_minus1 */
    sps->b_temporal_id_nesting = 0 != bitstream_read( &p_nalu->bitstream, 1 );

    parse_ptl( &p_nalu->bitstream, &context->as_protile[ 0 ], true, sps->i_max_temporal_layers - 1 );

    sps->i_id = (int8_t)bitstream_read_uvlc( &p_nalu->bitstream ); /* seq_parameter_set_id */
    sps->i_chroma_format_idc = (int8_t)bitstream_read_uvlc( &p_nalu->bitstream ); /* chroma_format_idc */
    /** in the first version we only support chroma_format_idc equal to 1 (4:2:0), so separate_colour_plane_flag cannot appear in the bitstream */
    if( sps->i_chroma_format_idc == 3 )
    {
        sps->b_separate_colour_plane_flag = bitstream_read( &p_nalu->bitstream, 1 );
        assert( sps->b_separate_colour_plane_flag == false );
    }

    if( sps->i_max_temporal_layers > 8 )
        long_jump( HEVCDEC_EXC_NOT_SUPPORTED, "too many temporal layers" );

    sps->i_pic_luma_width =  (int16_t)bitstream_read_uvlc( &p_nalu->bitstream );     /* i_pic_luma_width */
    sps->i_pic_luma_height = (int16_t)bitstream_read_uvlc( &p_nalu->bitstream );    /* pic_height_in_luma_samples */

    if( bitstream_read( &p_nalu->bitstream, 1 ) ) /* conformance_window_flag */
    {
        int32_t i_chroma_ss_factor = 2;
        sps->i_pic_conf_win_left_offset   = (int16_t)( i_chroma_ss_factor * bitstream_read_uvlc( &p_nalu->bitstream ) );
        sps->i_pic_conf_win_right_offset  = (int16_t)( i_chroma_ss_factor * bitstream_read_uvlc( &p_nalu->bitstream ) );
        sps->i_pic_conf_win_top_offset    = (int16_t)( i_chroma_ss_factor * bitstream_read_uvlc( &p_nalu->bitstream ) );
        sps->i_pic_conf_win_bottom_offset = (int16_t)( i_chroma_ss_factor * bitstream_read_uvlc( &p_nalu->bitstream ) );


        if( 0 >= sps->i_pic_luma_width - sps->i_pic_conf_win_left_offset - sps->i_pic_conf_win_right_offset 
            || 0 >= sps->i_pic_luma_height - sps->i_pic_conf_win_top_offset - sps->i_pic_conf_win_bottom_offset )
            long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "bad cropping values" );
    }
    else
    {
        sps->i_pic_conf_win_left_offset = sps->i_pic_conf_win_right_offset = sps->i_pic_conf_win_top_offset = sps->i_pic_conf_win_bottom_offset = 0;
    }

    sps->i_bit_depth_luma = (int8_t)(8 + bitstream_read_uvlc( &p_nalu->bitstream ));       /* bit_depth_luma_minus8   */
    sps->i_bit_depth_chroma = (int8_t)(8 + bitstream_read_uvlc( &p_nalu->bitstream ));     /* bit_depth_chroma_minus8 */

    sps->i_log2_max_pic_order_cnt_lsb = (int8_t)(4 + bitstream_read_uvlc( &p_nalu->bitstream ));    /* log2_max_pic_order_cnt_lsb_minus4 (0...12) */
    sps->i_max_pic_order_cnt_lsb = 1 << sps->i_log2_max_pic_order_cnt_lsb;

    b_subLayerOrderingInfoPresentFlag = bitstream_read( &p_nalu->bitstream, 1 );    /* sps_sub_layer_ordering_info_present_flag */

    for( i_idx = 0; i_idx < sps->i_max_temporal_layers; i_idx++ )
    {    
        sps->ai_max_dec_pic_buffering[ i_idx ] = 1 + bitstream_read_uvlc( &p_nalu->bitstream );
        sps->ai_num_reorder_pics[ i_idx ] = bitstream_read_uvlc( &p_nalu->bitstream );
        sps->max_latency_increase[ i_idx ] = bitstream_read_uvlc( &p_nalu->bitstream );

        if( !b_subLayerOrderingInfoPresentFlag )
        {
            for( i_idx++; i_idx < sps->i_max_temporal_layers; ++i_idx )
            {
                sps->ai_max_dec_pic_buffering[ i_idx ] = sps->ai_max_dec_pic_buffering[ 0 ];
                sps->ai_num_reorder_pics[ i_idx ] = sps->ai_num_reorder_pics[ 0 ];
                sps->max_latency_increase[ i_idx ] = sps->max_latency_increase[ 0 ];
            }
            break;
        }
    }

    sps->i_log2_min_coding_block_size = (int8_t)( 3 + bitstream_read_uvlc( &p_nalu->bitstream ) );
    sps->i_max_cu_depth = (int8_t)bitstream_read_uvlc( &p_nalu->bitstream );
    sps->i_max_cu_width = (int8_t)( sps->i_max_cu_height = 1<<(sps->i_log2_min_coding_block_size + sps->i_max_cu_depth) );

    sps->i_log2_min_transform_block_size = (int8_t)( 2 + bitstream_read_uvlc( &p_nalu->bitstream ) ); /* log2_min_transform_block_size_minus2 */
    sps->i_log2_max_transform_block_size = (int8_t)( sps->i_log2_min_transform_block_size + bitstream_read_uvlc( &p_nalu->bitstream ) );
    sps->i_max_transform_block_size = (int8_t)( 1 << sps->i_log2_max_transform_block_size );

    sps->i_max_transform_hierarchy_depth_inter = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );
    sps->i_max_transform_hierarchy_depth_intra = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );

    sps->i_add_depth = HEVC_MAX( 0, sps->i_log2_min_coding_block_size - sps->i_log2_min_transform_block_size );
    sps->i_max_cu_depth = sps->i_max_cu_depth + sps->i_add_depth;

    sps->b_scaling_list_enabled = bitstream_read( &p_nalu->bitstream, 1 );
    if( sps->b_scaling_list_enabled )
    {
        sps->b_scaling_list_present = bitstream_read( &p_nalu->bitstream, 1 );
        if( sps->b_scaling_list_present )
        {
            decode_scaling_list( &sps->s_scaling_list, &p_nalu->bitstream, &sps->s_luts );
        }
    }

    sps->b_amp = bitstream_read( &p_nalu->bitstream, 1 ); /* amp_enabled_flag */
    sps->b_sao = bitstream_read( &p_nalu->bitstream, 1 ); /* sample_adaptive_offset_enabled_flag */

    sps->b_pcm_enabled = bitstream_read( &p_nalu->bitstream, 1 );
    if( sps->b_pcm_enabled )
    {
        sps->i_pcm_bit_depth_luma = (int8_t)( 1 + bitstream_read( &p_nalu->bitstream, 4 ) );
        sps->i_pcm_bit_depth_chroma = (int8_t)( 1 + bitstream_read( &p_nalu->bitstream, 4 ) );
        sps->i_min_pcm_cb_size = (int8_t)( 3 + bitstream_read_uvlc( &p_nalu->bitstream ) );                    /* log2_min_pcm_coding_block_size_minus3 */
        sps->i_max_pcm_cb_size = 1 << (sps->i_min_pcm_cb_size + bitstream_read_uvlc( &p_nalu->bitstream ) );   /* log2_diff_max_min_pcm_coding_block_size */
        sps->i_min_pcm_cb_size = 1 << sps->i_min_pcm_cb_size;
        sps->b_pcm_loop_filter_disable = bitstream_read( &p_nalu->bitstream, 1 );
    }

    sps->i_num_short_term_ref_pic_sets = bitstream_read_uvlc( &p_nalu->bitstream );

    if( sps->i_num_short_term_ref_pic_sets )
    {
        sps->pps_rps_list = (reference_picture_set_t *)malloc( sizeof(reference_picture_set_t)*sps->i_num_short_term_ref_pic_sets );
        memset( sps->pps_rps_list, 0x0, sizeof(reference_picture_set_t)*sps->i_num_short_term_ref_pic_sets );
        for( i_idx = 0; i_idx < sps->i_num_short_term_ref_pic_sets; i_idx++ )
            decode_short_term_rps( &p_nalu->bitstream, i_idx, &sps->pps_rps_list[i_idx], sps->pps_rps_list, sps );
    }

    sps->b_long_term_ref_pics_present = bitstream_read( &p_nalu->bitstream, 1 );
    if( sps->b_long_term_ref_pics_present )
    {
        sps->i_num_long_term_ref_pic_sets = bitstream_read_uvlc( &p_nalu->bitstream ); /* num_long_term_ref_pic_sps */
        for( i_idx = 0; i_idx < sps->i_num_long_term_ref_pic_sets; i_idx++ )
        {
            sps->ai_ltrefpic_poc_lsb[ i_idx ] = bitstream_read( &p_nalu->bitstream, sps->i_log2_max_pic_order_cnt_lsb ); /* lt_ref_pic_poc_lsb_sps */
            sps->ab_ltusedbycurr[ i_idx ] = bitstream_read( &p_nalu->bitstream, 1 ); /* used_by_curr_pic_lt_sps_flag */
        }
    }

    sps->b_temporal_mvp = bitstream_read( &p_nalu->bitstream, 1 ); /* sps_temporal_mvp_enable_flag */
    sps->b_strong_intra_smoothing = bitstream_read( &p_nalu->bitstream, 1 );


    sps->b_vui_params = bitstream_read( &p_nalu->bitstream, 1 );  /* vui_parameters_present_flag */
    if( sps->b_vui_params )
        decode_vui( context, sps, p_nalu );

    /* sps_extension_flag */
    if( bitstream_read( &p_nalu->bitstream, 1 ) )
    {
        /* sps_extension_data_flag */
        while( more_rbsp_data( &p_nalu->bitstream ) )    
            bitstream_read( &p_nalu->bitstream, 1 );
    }


    gi_max_val_luma = ((1<<(sps->i_bit_depth_luma))-1);
    gi_max_val_chroma = ((1<<(sps->i_bit_depth_chroma))-1);

    if( sps->b_sao )
    {
        sao_create_context( &context->s_sao, sps->i_bit_depth_luma, sps->i_bit_depth_chroma, sps->i_pic_luma_width );
    }



    for( i_idx=0; i_idx<sps->i_max_cu_depth - sps->i_add_depth; i_idx++ )
        sps->ab_amvp[ i_idx ] = sps->b_amp;
    for(        ; i_idx<sps->i_max_cu_depth; i_idx++ )
        sps->ab_amvp[ i_idx ] = false;

    sps->b_init = true;
    sps->b_allocated = false;
    on_got_sps( &s_sps, context );
}


void 
decode_pps( hevc_decode_t *context, hevc_nalu_t *p_nalu )
{
    int32_t i_idx;
    picture_parameter_set_t *pps;
    sequence_parameter_set_t *sps;

    context->i_curr_pps_idx = (int8_t)bitstream_read_uvlc( &p_nalu->bitstream );
    pps = &context->as_pps[ context->i_curr_pps_idx ];
    pps->i_pic_parameter_set_id = context->i_curr_pps_idx;
    pps->i_seq_parameter_set_id = (int8_t)bitstream_read_uvlc( &p_nalu->bitstream );

    sps = &context->as_sps[ pps->i_seq_parameter_set_id ];

    pps->b_dependent_slices = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_output_flag_present = bitstream_read( &p_nalu->bitstream, 1 );
    pps->i_num_extra_slice_header_bits = bitstream_read( &p_nalu->bitstream, 3 );
    pps->b_sign_data_hiding = bitstream_read( &p_nalu->bitstream, 1 );

    pps->b_cabac_init_present = bitstream_read( &p_nalu->bitstream, 1 );

    pps->i_ref_l0_default_active = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );
    if( pps->i_ref_l0_default_active > 15 ) long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "bad ref_idx_active0 count" );
    pps->i_ref_l1_default_active = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );
    if( pps->i_ref_l1_default_active > 15 ) long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "bad ref_idx_active1 count" );


    pps->i_pic_init_qp = (int8_t)( 26 + bitstream_read_svlc( &p_nalu->bitstream ) );
    pps->b_constrained_intra_pred = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_transform_skip = bitstream_read( &p_nalu->bitstream, 1 );

    pps->b_use_dqp = bitstream_read( &p_nalu->bitstream, 1 );
    if( pps->b_use_dqp )
    {
        pps->ui_max_dqp_depth = bitstream_read_uvlc( &p_nalu->bitstream );
        pps->i_min_dqp_size = (int8_t)( context->as_sps[ 0 ].i_max_cu_width >> pps->ui_max_dqp_depth );
    }
    else
    {
        pps->i_min_dqp_size = (int8_t)(context->as_sps[ 0 ].i_max_cu_width);
        pps->ui_max_dqp_depth = 0;
    }

    pps->i_cb_qp_offset = bitstream_read_svlc( &p_nalu->bitstream );
    pps->i_cr_qp_offset = bitstream_read_svlc( &p_nalu->bitstream );
    pps->b_slice_chroma_qp = bitstream_read( &p_nalu->bitstream, 1 );

    pps->b_weighted_pred = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_weighted_bipred = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_transquant_bypass = bitstream_read( &p_nalu->bitstream, 1 );

    pps->b_tiles_enabled = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_entropy_coding_sync_enabled = bitstream_read( &p_nalu->bitstream, 1 );

    if( pps->b_tiles_enabled )
    {
        pps->i_tile_columns = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );
        pps->i_tile_rows = (int8_t)( 1 + bitstream_read_uvlc( &p_nalu->bitstream ) );
        pps->b_uniform_spacing = bitstream_read( &p_nalu->bitstream, 1 );
        if( !pps->b_uniform_spacing  )
        {
            for( i_idx=0; i_idx<pps->i_tile_columns-1; i_idx++)
                pps->ai_tcol_widths[ i_idx ] = 1 + bitstream_read_uvlc( &p_nalu->bitstream );

            for( i_idx=0; i_idx<pps->i_tile_rows-1; i_idx++)
                pps->ai_trow_heights[ i_idx ] = 1 + bitstream_read_uvlc( &p_nalu->bitstream );
        }

        if( pps->i_tile_columns>1 || pps->i_tile_rows>1 )
            pps->b_loop_filter_across_tiles = bitstream_read( &p_nalu->bitstream, 1 );
    }
    else
    {
        pps->i_tile_columns = pps->i_tile_rows = 0;        
    }

    pps->b_loop_filter_across_slices = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_deblocking_ctrl = bitstream_read( &p_nalu->bitstream, 1 ); /* deblocking_filter_control_present_flag */

    if( pps->b_deblocking_ctrl )
    {
        pps->b_deblocking_override = bitstream_read( &p_nalu->bitstream, 1 );
        pps->b_disable_deblocking = bitstream_read( &p_nalu->bitstream, 1 );  /* disable_deblocking_filter_flag */
        if( !pps->b_disable_deblocking )
        {
            pps->i_lf_beta_offset = (int8_t)bitstream_read_svlc( &p_nalu->bitstream ) << 1;
            pps->i_lf_tc_offset = (int8_t)bitstream_read_svlc( &p_nalu->bitstream ) << 1;
        }
    }

    pps->b_scaling_list_data = bitstream_read( &p_nalu->bitstream, 1 );
    if( pps->b_scaling_list_data )
    {
        decode_scaling_list( &context->as_pps_scaling_lists[ context->i_curr_pps_idx ], &p_nalu->bitstream, &sps->s_luts );
    }

    pps->b_lists_modification_present = bitstream_read( &p_nalu->bitstream, 1 ); /* lists_modification_present_flag */
    pps->i_log2_parallel_merge_level = (int8_t)( 2 + bitstream_read_uvlc( &p_nalu->bitstream ) );

    pps->b_slice_header_extension = bitstream_read( &p_nalu->bitstream, 1 );
    pps->b_extension = bitstream_read( &p_nalu->bitstream, 1 );
    if( pps->b_extension )
    {
        while( more_rbsp_data( &p_nalu->bitstream ) )
        {
            bitstream_read( &p_nalu->bitstream, 1 ); /* pps_extension_data_flag */
        }
    }

    pps->b_isDefined= true;
}

int32_t 
get_num_rps_curr_temp_list( slice_t *p_slice )
{
    int32_t i_num_rps_curr_temp_list = 0, i_idx;

    if( p_slice->e_type == I_SLICE )
        return 0;
    
    for( i_idx=0; i_idx < p_slice->p_rps->i_num_negativePictures + p_slice->p_rps->i_num_positivePictures + p_slice->p_rps->i_num_longtermPictures; i_idx++ )
    {
        if( p_slice->p_rps->ab_used[ i_idx ] )
            i_num_rps_curr_temp_list++;
    }

    return i_num_rps_curr_temp_list;
}

bool reference_nalu( hevc_nalu_t *p_nalu )
{
    return (p_nalu->e_nalu_type <= NAL_UNIT_RESERVED_VCL_R15 && (p_nalu->e_nalu_type & 1))   ||
           (p_nalu->e_nalu_type >= NAL_UNIT_CODED_SLICE_BLA_W_LP && p_nalu->e_nalu_type <= NAL_UNIT_RESERVED_IRAP_VCL23 );
}

bool parse_slice_header( hevc_decode_t *context, hevc_nalu_t *p_nalu, slice_t *p_slice )
{
    int32_t i_idx;
    int32_t i_address;
    int32_t i_inner_address = 0;
    int32_t i_cu_address = 0;
    int32_t i_req_bits_outer = 0, i_req_bits_inner = 0;
    int32_t i_max_parts, i_num_parts, i_num_ctus;
    bitstream_t *bitstream = &p_nalu->bitstream;

    p_slice->e_nalu_type = p_nalu->e_nalu_type;
    p_slice->b_1st_slice = bitstream_read( bitstream, 1 ); /* first_slice_segment_in_pic_flag */
    if( p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
        p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP   ||
        p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_N_LP   ||
        p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL ||
        p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_LP   ||
        p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_CRA )
    {
        bitstream_read( bitstream, 1 ); /* no_output_of_prior_pics_flag, ignored */
    }

    p_slice->i_pps_id = (int8_t)bitstream_read_uvlc( bitstream );  /* pic_parameter_set_id */
    if( p_slice->i_pps_id > context->i_curr_pps_idx || p_slice->i_pps_id >= NUM_MAX_PIC_PARAM_SETS )
        long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "slice w/ invalid PPS" );

    p_slice->p_pps = &context->as_pps[ p_slice->i_pps_id ];
    p_slice->p_sps = &context->as_sps[ p_slice->p_pps->i_seq_parameter_set_id ];

    if( !p_slice->p_sps->i_max_cu_width || !p_slice->p_sps->i_max_cu_height )
        long_jump( HEVCDEC_EXC_SYNTAX_ERROR, "slice w/ invalid SPS" );

    if( p_slice->p_pps->b_dependent_slices && !p_slice->b_1st_slice )
        p_slice->b_dependent = bitstream_read( &p_nalu->bitstream, 1 ) == 1;
    else
        p_slice->b_dependent = false;
    
    p_slice->i_temp_hier = p_nalu->i_temporal_id;

    if ((p_slice->p_sps->i_max_cu_width == 0) || (p_slice->p_sps->i_max_cu_height == 0))
    {
        msglog(NULL, MSGLOG_ERR, "parsing slice header error! \n");
        return 1;
    }

    i_num_ctus = ( p_slice->p_sps->i_pic_luma_width + p_slice->p_sps->i_max_cu_width - 1 ) / p_slice->p_sps->i_max_cu_width
        * ( ( p_slice->p_sps->i_pic_luma_height + p_slice->p_sps->i_max_cu_height - 1 ) / p_slice->p_sps->i_max_cu_height );
    i_max_parts = 1<<( p_slice->p_sps->i_max_cu_depth<<1 );
    i_num_parts = 0;
    
    while( i_num_ctus>(1<<i_req_bits_outer) ) i_req_bits_outer++;

    if( !p_slice->b_1st_slice )
    {
        i_address = bitstream_read( bitstream, i_req_bits_outer+i_req_bits_inner );
        i_cu_address = i_address >> i_req_bits_inner;
        i_inner_address = i_address - ( i_cu_address<<i_req_bits_inner );
    }

    p_slice->i_start_cu_addr = (i_max_parts*i_cu_address + i_inner_address * i_max_parts);
    p_slice->i_end_cu_addr = i_num_ctus * i_max_parts;

    if( !p_slice->b_dependent )
    {    
        if( p_slice->p_pps->i_num_extra_slice_header_bits )
            bitstream_read( bitstream, p_slice->p_pps->i_num_extra_slice_header_bits );
        
        p_slice->e_type = (slice_type_t)bitstream_read_uvlc( bitstream ); /* slice_type */

        if( p_slice->p_pps->b_output_flag_present )
            p_slice->b_pic_output = 0 != bitstream_read( bitstream, 1 ); /* pic_output_flag */
        else
            p_slice->b_pic_output = true;
    
    
        if( p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP )
        {
            context->i_prev_poc = p_slice->i_poc;
            if( 0 == p_slice->i_temp_hier && reference_nalu( p_nalu ) && p_nalu->e_nalu_type != NAL_UNIT_CODED_SLICE_RASL_R && p_nalu->e_nalu_type != NAL_UNIT_CODED_SLICE_RADL_R )
                context->i_prev_tid0_poc = context->i_prev_poc;
        
            p_slice->i_poc = 0;
            p_slice->s_rps_local.i_num_negativePictures = 0;
            p_slice->s_rps_local.i_num_positivePictures = 0;
            p_slice->s_rps_local.i_num_longtermPictures = 0;
            p_slice->s_rps_local.i_num_pictures = 0;
            p_slice->p_rps = &p_slice->s_rps_local;
        }
        else
        {
            reference_picture_set_t *p_rps;

            /* pic_order_cnt_lsb */
            int32_t i_poc_lsb = bitstream_read( bitstream, p_slice->p_sps->i_log2_max_pic_order_cnt_lsb );
            int32_t i_prev_poc = context->i_prev_tid0_poc;
            int32_t i_max_poc_lsb = 1 << p_slice->p_sps->i_log2_max_pic_order_cnt_lsb;
            int32_t i_prev_poc_lsb = i_prev_poc & (i_max_poc_lsb - 1);
            int32_t i_prev_poc_msb = i_prev_poc - i_prev_poc_lsb;
            int32_t i_poc_msb;

            if( i_poc_lsb < i_prev_poc_lsb && (i_prev_poc_lsb - i_poc_lsb) >= i_max_poc_lsb / 2 )
            {
                i_poc_msb = i_prev_poc_msb + i_max_poc_lsb;
            }
            else if( i_poc_lsb > i_prev_poc_lsb && (i_poc_lsb - i_prev_poc_lsb) > i_max_poc_lsb / 2 ) 
            {
                i_poc_msb = i_prev_poc_msb - i_max_poc_lsb;
            }
            else
            {
                i_poc_msb = i_prev_poc_msb;
            }


            if( p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_LP   ||
                p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL ||
                p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_N_LP )
            {
                /** For BLA/BLANT, POCmsb is set to 0. Na denn macht mal. */
                i_poc_msb = 0;
            }
            
            p_slice->i_poc = i_poc_msb + i_poc_lsb;
            if( 0 == p_slice->i_temp_hier ) context->i_prev_poc = p_slice->i_poc;
            if( 0 == p_slice->i_temp_hier && reference_nalu( p_nalu ) && p_nalu->e_nalu_type != NAL_UNIT_CODED_SLICE_RASL_R && p_nalu->e_nalu_type != NAL_UNIT_CODED_SLICE_RADL_R )
                context->i_prev_tid0_poc = context->i_prev_poc;

            if( !bitstream_read( bitstream, 1 ) )   /* short_term_ref_pic_set_sps_flag */
            {
                /** use short-term reference picture set explicitly signalled in slice header */
                p_rps = &p_slice->s_rps_local;
                decode_short_term_rps( bitstream, p_slice->p_sps->i_num_short_term_ref_pic_sets, p_rps, p_slice->p_sps->pps_rps_list, p_slice->p_sps );
                p_slice->p_rps = p_rps;
            }
            else /** use reference to short-term reference picture set in PPS */
            {
                int32_t numBits = 0, i_short_term_ref_pic_set_idx;
                while( (1 << numBits) < p_slice->p_sps->i_num_short_term_ref_pic_sets )
                    numBits++;
                
                if( numBits )
                    i_short_term_ref_pic_set_idx = bitstream_read( bitstream, numBits );
                else
                    i_short_term_ref_pic_set_idx = 0;
                
                p_slice->p_rps = &p_slice->p_sps->pps_rps_list[ i_short_term_ref_pic_set_idx ];
                p_rps = p_slice->p_rps;
            }


            if( p_slice->p_sps->b_long_term_ref_pics_present )
            {
                int32_t i_offset = p_slice->p_rps->i_num_negativePictures + p_slice->p_rps->i_num_positivePictures;
                int32_t i_num_ltrp = 0;
                int32_t i_num_ltrp_sps = 0;
                int32_t i_bits_for_ltrp_sps = 0,k;
                int32_t i_max_poc_lsb = 1 << p_slice->p_sps->i_log2_max_pic_order_cnt_lsb;
                int32_t i_prev_delta_msb = 0, i_delta_poc_msb_cycle_lt = 0;

                if( p_slice->p_sps->i_num_long_term_ref_pic_sets > 0 )
                {
                    i_num_ltrp_sps = bitstream_read_uvlc( bitstream );  /* num_long_term_sps */
                    i_num_ltrp += i_num_ltrp_sps;
                    p_slice->p_rps->i_num_longtermPictures = i_num_ltrp;
                }

                while( p_slice->p_sps->i_num_long_term_ref_pic_sets > (1 << i_bits_for_ltrp_sps) )
                    i_bits_for_ltrp_sps++;
            
                p_slice->p_rps->i_num_longtermPictures = bitstream_read_uvlc( bitstream ); /* num_long_term_pics */
                i_num_ltrp += p_slice->p_rps->i_num_longtermPictures;
            
                for( i_idx=i_offset+p_slice->p_rps->i_num_longtermPictures-1, k = 0; k < i_num_ltrp; i_idx--, k++ )
                {
                    int32_t pocLsbLt;

                    if( k < i_num_ltrp_sps )
                    {
                        bool b_used_by_curr_from_SPS;
                        int32_t i_code = 0;
                        if( i_bits_for_ltrp_sps > 0 )
                            i_code = bitstream_read( bitstream, i_bits_for_ltrp_sps ); /* lt_idx_sps[i] */

                        b_used_by_curr_from_SPS = p_slice->p_sps->ab_ltusedbycurr[ i_code ];

                        pocLsbLt = p_slice->p_sps->ai_ltrefpic_poc_lsb[ i_code ];
                        p_slice->p_rps->ab_used[ i_idx ] = b_used_by_curr_from_SPS;
                    }
                    else
                    {
                        pocLsbLt = bitstream_read( bitstream, p_slice->p_sps->i_log2_max_pic_order_cnt_lsb ); /* poc_lsb_lt */
                        p_slice->p_rps->ab_used[ i_idx ] = bitstream_read( bitstream, 1 ); /* used_by_curr_pic_lt_flag */
                    }
                
                    /* delta_poc_msb_present_flag */
                    if( bitstream_read( bitstream, 1 ) )                  
                    {
                        int32_t i_pocLTCurr;
                        int32_t i_code = bitstream_read_uvlc( bitstream ); /* delta_poc_msb_cycle_lt[i] */
                        bool deltaFlag = false;
                    
                        if( i_idx == i_offset+p_slice->p_rps->i_num_longtermPictures-1 || i_idx == i_offset+i_num_ltrp-i_num_ltrp_sps-1 )
                            deltaFlag = true;
                    
                        if( deltaFlag )
                            i_delta_poc_msb_cycle_lt = i_code;
                        else
                            i_delta_poc_msb_cycle_lt = i_code + i_prev_delta_msb;              
                    
                        i_pocLTCurr = p_slice->i_poc - i_delta_poc_msb_cycle_lt * i_max_poc_lsb - i_poc_lsb + pocLsbLt;
                        p_slice->p_rps->ai_poc[ i_idx ] = i_pocLTCurr; 
                        p_slice->p_rps->ai_delta_poc[ i_idx ] = -p_slice->i_poc + i_pocLTCurr;
                        p_slice->p_rps->ab_ltmsb[ i_idx ] = true;
                    }
                    else
                    {
                        p_slice->p_rps->ai_poc[ i_idx ] = pocLsbLt;
                        p_slice->p_rps->ai_delta_poc[ i_idx ] = -p_slice->i_poc + pocLsbLt;
                        p_slice->p_rps->ab_ltmsb[ i_idx ] = false;

                        if( i_idx == i_offset + (i_num_ltrp - i_num_ltrp_sps)-1 )
                            i_delta_poc_msb_cycle_lt = 0;
                    }

                    i_prev_delta_msb = i_delta_poc_msb_cycle_lt;
                }

                i_offset += p_slice->p_rps->i_num_longtermPictures;
                p_slice->p_rps->i_num_pictures = i_offset;
            }


            if( p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_LP   ||
                p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL ||
                p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_N_LP )
            {
                /** In the case of BLA/BLANT, rps data is read from slice header but ignored */
                p_slice->s_rps_local.i_num_negativePictures = 0;
                p_slice->s_rps_local.i_num_positivePictures = 0;
                p_slice->s_rps_local.i_num_longtermPictures = 0;
                p_slice->s_rps_local.i_num_pictures = 0;
                p_slice->p_rps = &p_slice->s_rps_local;
            }

            if( p_slice->p_sps->b_temporal_mvp )
                p_slice->b_temporal_mvp = bitstream_read( bitstream, 1 ); /* slice_temporal_mvp_enable_flag */
        }
    } 

    return true;
}

bool 
gop_decode_slice( hevc_decode_t *context, hevc_nalu_t *p_nalu )
{
    slice_t s_slice;
    bool ret = 0;
    
    if( context->i_curr_sps_idx < 0 )
        assert( !"Slice w/a SPS" );

    if( context->i_curr_pps_idx < 0 )
        assert( !"Slice w/a PPS" );


    memset( &s_slice, 0, sizeof(slice_t) );

    s_slice.p_pps = &context->as_pps[ context->i_curr_pps_idx ];
    s_slice.p_sps = &context->as_sps[ context->i_curr_sps_idx ];

    ret = parse_slice_header( context, p_nalu, &s_slice );

    if (   p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL
    || p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP
    || p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_N_LP
    || p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL
    || p_nalu->e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_LP )
    {
        context->IDR_pic_flag = 1;
    }
    else
        context->IDR_pic_flag = 0;

    return ret;
}

typedef enum
{
    SEI_BUFFERING_PERIOD                     = 0,
    SEI_PICTURE_TIMING                       = 1,
    SEI_PAN_SCAN_RECT                        = 2,
    SEI_FILLER_PAYLOAD                       = 3,
    SEI_USER_DATA_REGISTERED_ITU_T_T35       = 4,
    SEI_USER_DATA_UNREGISTERED               = 5,
    SEI_RECOVERY_POINT                       = 6,
    SEI_SCENE_INFO                           = 9,
    SEI_FULL_FRAME_SNAPSHOT                  = 15,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
    SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
    SEI_FILM_GRAIN_CHARACTERISTICS           = 19,
    SEI_POST_FILTER_HINT                     = 22,
    SEI_TONE_MAPPING_INFO                    = 23,
    SEI_FRAME_PACKING                        = 45,
    SEI_DISPLAY_ORIENTATION                  = 47,
    SEI_SOP_DESCRIPTION                      = 128,
    SEI_ACTIVE_PARAMETER_SETS                = 129,
    SEI_DECODING_UNIT_INFO                   = 130,
    SEI_TEMPORAL_LEVEL0_INDEX                = 131,
    SEI_DECODED_PICTURE_HASH                 = 132,
    SEI_SCALABLE_NESTING                     = 133,
    SEI_REGION_REFRESH_INFO                  = 134,
    SEI_MASTERING_DISPLAY_COLOR_VOLUME       = 137,
    SEI_LIGHT_LEVEL_INFORMATION              = 144
} SEI_PayloadType_t;

void decode_sei_nalu( hevc_decode_t *p_context, hevc_nalu_t *p_nalu )
{
    do
    {
        SEI_PayloadType_t e_payload_type;
        uint32_t payloadSize;
        uint8_t byte;

        *((int32_t *)&e_payload_type) = 0;
        for( byte = 0xff; 0xff == byte; )
            *((int32_t *)&e_payload_type) += byte = (uint8_t)bitstream_read( &p_nalu->bitstream, 8 );

        payloadSize = 0;
        for( byte = 0xff; 0xff == byte; )
            payloadSize += byte = (uint8_t)bitstream_read( &p_nalu->bitstream, 8 );
                
        switch( e_payload_type )
        {
            case SEI_USER_DATA_REGISTERED_ITU_T_T35:
            {
                uint8_t country_code = 0;
                uint16_t provider_code = 0;
                uint32_t user_id = 0;
                uint8_t data_type_code = 0;

                country_code = (uint8_t)bitstream_read( &p_nalu->bitstream, 8 );
                provider_code = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                user_id = bitstream_read( &p_nalu->bitstream, 32 );
                data_type_code = (uint8_t)bitstream_read( &p_nalu->bitstream, 8 );

                if (country_code  == 0xb5 &&
                    provider_code == 0x31 &&
                    user_id       == 0x47413934 &&
                    (data_type_code == 0x8 || data_type_code == 0x9))
                {
                    p_context->rpu_flag = 1;
                }

                break;
            }
            case SEI_MASTERING_DISPLAY_COLOR_VOLUME:
                {
                    uint16_t temp0 = 0;
                    uint32_t temp1 = 0;
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Green primary  x: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Green primary  y: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Blue primary   x: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Blue primary   y: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Red primary    x: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Red primary    y: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering White primary  x: %f\n", temp0*0.00002);
                     temp0 = (uint16_t)bitstream_read( &p_nalu->bitstream, 16 );
                      msglog(NULL, MSGLOG_INFO, "Mastering White primary  y: %f\n", temp0*0.00002);

                     temp1 = bitstream_read( &p_nalu->bitstream, 32 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Luminance Man: %f\n", temp1*0.0001);
                     temp1 = bitstream_read( &p_nalu->bitstream, 32 );
                      msglog(NULL, MSGLOG_INFO, "Mastering Luminance Min: %f\n", temp1*0.0001);
                }
            default:
                break;
        }
    }while( p_nalu->bitstream.i64_bits_available > 2 );
}


