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
    @file parser_aac.h
    @brief Defines aac parser interface
*/

#ifndef __PARSER_AAC_H__
#define __PARSER_AAC_H__

struct parser_aac_t_
{
    PARSER_AUDIO_BASE;

    uint32_t sample_num;
    uint32_t samples_per_frame;

    /** aac adts fix header */
    BOOL     ID;
    BOOL     protection_absent;
    uint32_t profile_ObjectType;
    uint32_t sampling_frequency_index;
    uint32_t channel_configuration;

    /** aac adts variable header */
    uint32_t aac_frame_length_remain;
    uint32_t adts_buffer_fullness;
    uint32_t number_of_raw_data_blocks_in_frame;

    /** to handle number_of_raw_data_blocks_in_frame > 0 */
    uint16_t raw_data_block_position[4];
    uint32_t raw_data_block_idx;

    /** to avoid ralloc buf */
    uint32_t sample_buf_size;

    /** the 7 byte adts header for demux aac dumping */
    uint8_t *adts_hdr_buf;
};
typedef struct parser_aac_t_ parser_aac_t;
typedef parser_aac_t *parser_aac_handle_t;

#endif  /* __PARSER_AAC_H__ */
