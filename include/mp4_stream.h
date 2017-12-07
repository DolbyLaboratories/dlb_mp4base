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
 *  @file  mp4_stream.h
 *  @brief Defines structures to handle demultiplex result for each tracks
 */

#ifndef __MP4_STREAM_H__
#define __MP4_STREAM_H__

#include "mp4_ctrl.h"  /** stream_handle_t */

#ifdef __cplusplus
extern "C"
{
#endif

/************* stream API ****************************/
void     stream_reset_last_get_sample_info(stream_handle_t stream, uint64_t offset);
uint32_t stream_get_sample_idx            (stream_handle_t stream, uint64_t start_time);
uint64_t stream_get_sample_timing         (stream_handle_t stream, uint32_t nSample, uint32_t *cts_offset);
uint32_t stream_get_sample_duration       (stream_handle_t stream, uint32_t nSample);
uint64_t stream_get_sample_offset         (stream_handle_t stream, uint32_t nSample, uint32_t *sample_desc_index);
uint32_t stream_get_sample_size           (stream_handle_t stream, uint32_t nSample);
uint32_t stream_get_sample_max_size       (stream_handle_t stream, uint32_t *fix_size);
uint32_t stream_get_prev_sync_sample_idx  (stream_handle_t stream, uint32_t nSample);

uint32_t       stream_get_track_media_timescale   (stream_handle_t stream);
uint64_t       stream_get_track_duration          (stream_handle_t stream);
uint64_t       stream_get_track_media_duration    (stream_handle_t stream);
uint32_t       stream_get_track_frame_num         (stream_handle_t stream);
uint32_t       stream_get_track_flags             (stream_handle_t stream);
uint32_t       stream_get_track_id                (stream_handle_t stream);
stream_type_t  stream_get_track_stream_type       (stream_handle_t stream);
const int8_t * stream_get_track_stream_name       (stream_handle_t stream);
stream_id_t    stream_get_track_stream_id         (stream_handle_t stream);

int32_t stream_get_video_track_image_info(stream_handle_t stream,
                                      uint32_t *      video_width,
                                      uint32_t *      video_height,
                                      uint32_t *      video_pixel_depth);

int32_t stream_get_audio_track_channelcount(stream_handle_t stream);
int32_t stream_get_audio_track_sample_rate (stream_handle_t stream);

void stream_destroy(stream_handle_t s);

#ifdef __cplusplus
};
#endif

#endif /* __MP4_STREAM_H__ */
