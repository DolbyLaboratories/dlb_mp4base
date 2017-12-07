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
 *  @file  mp4_isom.h
 *  @brief Defines structures and functions for geting stream ID, language mapping.
 */

#ifndef __MP4_ISOM_H__
#define __MP4_ISOM_H__

#include "c99_inttypes.h"  /** uint32_t */
#include "parser_defs.h"   /** stream_id_t */

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct stream_id_tag_map_t_
{
    uint32_t     stream_id;
    uint8_t tag;
} stream_id_tag_map_t;

typedef struct stream_id_codingname_map_t_
{
    int32_t   stream_id;
    int8_t *codingname;
} stream_id_codingname_map_t;

uint16_t movie_language_to_iso639 (uint16_t code, uint8_t *language);
uint16_t movie_iso639_to_language (const int8_t *language);

extern const stream_id_tag_map_t        stream_id_objectTypeIndication_tbl[];
extern const stream_id_codingname_map_t stream_id_video_codingname_tbl[];
extern const stream_id_codingname_map_t stream_id_audio_codingname_tbl[];
extern const stream_id_codingname_map_t stream_id_metadata_codingname_tbl[];

/** parser fourcc tag <-> stream_id */
stream_id_t get_stream_id           (const stream_id_codingname_map_t *map, const int8_t *codingname);
int8_t *    get_stream_codingname   (const stream_id_codingname_map_t *map, const int8_t *codingname);
uint32_t    get_objectTypeIndication(const stream_id_tag_map_t *map, uint32_t stream_id);

#ifdef __cplusplus
};
#endif

#endif /* __MP4_ISOM_H__ */
