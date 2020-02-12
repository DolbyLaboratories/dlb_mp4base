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
    @file  ema_mp4_ifc.h
    @brief Defines EMA API for mp4muxer

 * ema_mp4_mux_api.h is the header file for the mp4muxer APIs.
 * In this header file, the APIs to create and destroy a multiplexer object
 * are defined. The API also defines methods to setup and start a
 * multiplexer object are also defined to multiplex input ES stream files 
 * into an output .mp4 file.
 *
 * To multiplex ES files using the APIs, first call the ema_mp4_mux_create() to 
 * create a multiplexer object, then feed the ES files to the multiplexer for 
 * multiplexing with the ema_mp4_mux_set_input().  This function can be called 
 * multiple times. Other optional parameters can be set with the ema_mp4_mux_set_*()
 * form method. Call ema_mp4_mux_start() to start multilplexing.
 * Finally, call the ema_mp4_mux_destroy() to destroy the multiplexer object after
 * the ema_mp4_mux_start() returns.
 *
 * ema_mp4_mux_main_cli() is provided to run a multiplexer in a command line prompt.
 *
 * NOTES: In this release
 * (1) the API is non-reentrant.
 * (2) Only Windows version has been tested.
 * (3) Only file based input/output stream is supported.
 * (4) HEVC parser does not support open GOP, so timing inforing such as CTS and PTS 
 *     may not be accurate for Open GOP ES.
 * (5) More clean up of the code is under way.The code is being optimized and more 
 *     funcitonalities will be added for the multiplexer.
 */

#ifndef __EMA_MP4IFC_H__
#define __EMA_MP4IFC_H__

#include "mp4_ctrl.h"  /** mp4_ctrl_handle_t */

#define MAX_INPUT_ES_NUM  16  /** supports up to 16 elementary streams for now */
#define CHK_ERR_RET(ret)  if ((ret) != EMA_MP4_MUXED_OK) return (ret);
#define CHK_ERR_CNT(ret)  if ((ret) != EMA_MP4_MUXED_OK) continue;

#ifdef __cplusplus
extern "C"
{
#endif

struct ema_mp4_ctrl_t_
{
    /**** user input info */
    usr_cfg_mux_t usr_cfg_mux;                    /**< config info per mux */
    usr_cfg_es_t  usr_cfg_ess[MAX_INPUT_ES_NUM];  /**< config info per es */

    /**** muxer */
    mp4_ctrl_handle_t mp4_handle;
    mp4_ctrl_handle_t mp4_handle_el;

    /**** mux coresponding data sink, assume file only for now */
    bbio_handle_t mp4_sink;
    bbio_handle_t mp4_sink_el;

    /**** demux output base name */
    int8_t  * fn_out;
    size_t fn_out_base_len, fn_out_buf_size;

    /**** mux coresponding data sources, assume file only for now */
    bbio_handle_t data_srcs[MAX_STREAMS];

    /**** demux input */
    int8_t  *        fn_in;
    bbio_handle_t mp4_src;

    int32_t demux_flag;
};

/** \brief Opaque mux handle for the API */
typedef struct ema_mp4_ctrl_t_  ema_mp4_ctrl_t;
typedef ema_mp4_ctrl_t *        ema_mp4_ctrl_handle_t;

/** \brief Creates a multiplexer.
 *
 * ema_mp4_mux_create() should be called first.
 * \param handle: the pointer of the multiplexer handle
 * \return sucess/error codes:EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_create(ema_mp4_ctrl_handle_t *handle);

/** \brief Destroys a multiplexer.
 *
 * ema_mp4_mux_destroy() must be called last.
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \return none
 */
void ema_mp4_mux_destroy(ema_mp4_ctrl_handle_t handle);

/** \brief Starts a multiplexer.
 *
 * ema_mp4_mux_start() should be called after all options are set. The mp4
 * file generated is saved in the file provided by ema_mp4_mux_set_mp4().
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_start(ema_mp4_ctrl_handle_t handle);

/** \brief Supplies the file that contains elementary streams to the multiplexer 
 *         for multiplexing. This function can be called multiple times to supply 
 *         the ES one by one. Up to 100 streams are supported, subject to the 
 *         resource limits.
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param fn: the file containing the ES to be multiplexed. Multiplex
 *        relies on the file name extension to derived the type of ES.
 *        Currently, following type of stream are fully supported:
 * \verbatim
         type                    file name extension
         avc                     avc, h264, 264
         ac3                     ac3
         e-ac3                   ec3
         aac                     aac
         interactive track       emaj \endverbatim
 *        At least one audio or video stream must be provide to multiplexer.
 * \param lang a 3-letter string defining the language defined according to ISO 639.
 *        For example,
 * \verbatim
         ISO 639 definition      language
         eng                     English
         fra                     French
         spa                     Spanish \endverbatim
 *        NULL for unknown. Empty string is not allowed.
 * \param enc_name string up to 31 character long defining the encoder used to generated
 *        the ES. For example, "dlby" for Dolby AC3 or E-AC3.
 *        NULL for unknown. Empty string is not allowed.
 * \param chunk_span_size specify the maximum chunk size in byte. Default chunk span is
 *        controlled globally by chunk duration time. Chunk span is irrelevant if fragment is enabled.
 * \param tid specify the track id to be selected. Zero means no selection (selected all).
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_input(ema_mp4_ctrl_handle_t handle, 
                          int8_t  *fn, 
                          int8_t  *lang, 
                          int8_t  *enc_name, 
                          uint32_t time_scale, 
                          uint32_t chunk_span_size, 
                          uint32_t tid);

/** \brief Defines the file name that contains the output mp4 file. The default name
 *         is test.mp4
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param buf_out if none zero, provide a buffer interface output(TBD)
 * \param fn the file name
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_output(ema_mp4_ctrl_handle_t handle, int32_t buf_out, const int8_t  *fn);

/** \brief Sets the movie timescale
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param timescale ticks in one second. The default value is 600.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_moov_timescale(ema_mp4_ctrl_handle_t handle, uint32_t timescale);

/** \brief Sets the creation and modification time
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param cmtimeh:
 * \param cmtimel:cmtimeh and cmtimel define a 64-bit number of seconds since 
 *                12:00 AM 01-01-1904. For eample, 0x0 and 0xc55b41a1 is 12-17-08 
 *                10:29 AM. Without this option, the multiplexer uses the current time.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_cm_time(ema_mp4_ctrl_handle_t handle, uint32_t cmtimeh, uint32_t cmtimel);

/** \brief Sets a global chunk size in time
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param chunk_span_time: The chunk span in millisecond. A value of 0 indicates
 *                          no chunk interleave. The default value is 250 ms. Chunk 
 *                          span is irrelevant when fragment is enabled.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_chunk_span_time(ema_mp4_ctrl_handle_t handle, uint32_t chunk_span_time);

/** \brief Sets the main brand
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param mbrand: the name of a main brand: isom, avc1, iso2 or 3gp6.
 *        The default is isom.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_mbrand(ema_mp4_ctrl_handle_t handle, const int8_t  *mbrand);

/** \brief Sets a compatable brand. One call adds one compatable brand
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param cbrand: the name of a compatable brand: isom, mp41, mp42, avc1  and iso2.
 *        The default is mp42.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_cbrand(ema_mp4_ctrl_handle_t handle, const int8_t  *cbrand);

/** \brief Provides additional options
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param opt: option string. For now, (opt & 0x1) means use 64 bit for chuck offset.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_withopt(ema_mp4_ctrl_handle_t handle, const int8_t  *opt);

/** \brief Sets multiple sample description support
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param sd: string. "single": Only single sample description allowed.
                     "multiple": Multiple sample descriptions allowed.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_sd(ema_mp4_ctrl_handle_t handle, const int8_t  *sd);

/** \brief Sets the debug output level
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param lvl: the debug output level. The default debug level is warning.
 *        Supported levels include error, warning, info, verbose, and debug.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_db_level(ema_mp4_ctrl_handle_t handle, int8_t  *lvl);


/** \brief  Sets the output format( mp4 or frag-mp4)
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param outfm: string to store "mp4" or "frag-mp4".
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_output_format(ema_mp4_ctrl_handle_t handle, const int8_t  *outfm);


/** \brief  Sets the maximum segment duration for fragmented mp4
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param max_duration: max segment duration in millisecond.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_max_duration(ema_mp4_ctrl_handle_t handle, uint32_t max_duration);

/** \brief  Sets the video framerate value
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param nome/deno: video framerate value, for 23.976, nome = 24000 deno = 1001
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_video_framerate(ema_mp4_ctrl_handle_t handle, uint32_t nome, uint32_t deno);

/** \brief  Sets the DoVi ES mode 
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param mode: DoVi ES can be: 
 *             'comb':  BL, EL, and RPU are combined into a single file(Default)
 *             'split': BL and EL+RPU are separated as two elementary stream files
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_dv_es_mode(ema_mp4_ctrl_handle_t handle, const int8_t *mode);

/** \brief  Sets the DoVi profile value 
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param profile: A value of 0-9. Refer to the Dolby Vision Profiles Levels 
 *                 specification for detailed information.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_dv_profile(ema_mp4_ctrl_handle_t handle, int32_t profile);

/** \brief  Sets the DoVi compatible id value
 *
 * \param handle: the multiplexer handle returned by the ema_mp4_mux_create()
 * \param compatid: A value of 0-9. Refer to the Dolby Vision Profiles Levels 
 *                 specification for detailed information.
 * \return EMA_MP4_MUXED_...
 */
uint32_t ema_mp4_mux_set_dv_compatible_id(ema_mp4_ctrl_handle_t handle, int32_t compatible_id);

#ifdef __cplusplus
}
#endif

#endif /* !__EMA_MP4IFC_H__ */
