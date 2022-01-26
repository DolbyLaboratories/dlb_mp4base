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
    @file ema_mp4_mux_api.c
    @brief Implements API for the mp4muxer
*/

#include <time.h>
#include "utils.h"
#include "io_base.h"
#include "registry.h"
#include "dsi.h"
#include "parser.h"
#include "mp4_muxer.h"
#include "ema_mp4_ifc.h" 


/**** Creates a data sink */
static int32_t
mux_data_sink_create(ema_mp4_ctrl_handle_t handle)
{
    bbio_handle_t snk = NULL;

    if (handle->usr_cfg_mux.output_mode & EMA_MP4_IO_FILE)
    {
        snk              = reg_bbio_get('f', 'w');
        handle->mp4_sink = snk;                     /** keep it in handle to be freed by ema_mp4_mux_destroy() */
        if (snk->open(snk, handle->usr_cfg_mux.output_fn))
        {
            msglog(NULL, MSGLOG_ERR, "ERROR! Can't open output file %s .\n", handle->usr_cfg_mux.output_fn);
            return EMA_MP4_MUXED_OPEN_FILE_ERR;
        }
    }

    if (handle->usr_cfg_mux.output_mode & EMA_MP4_IO_BUF)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Can't support Buffer mode output %s .\n", handle->usr_cfg_mux.output_fn);
        return EMA_MP4_MUXED_CLI_ERR;
    }

    return EMA_MP4_MUXED_OK;
}

static void
mux_data_sink_destroy(bbio_handle_t snk)
{
    if (snk)
    {
        snk->destroy(snk);
    }
}

static void
mux_muxer_destroy(mp4_ctrl_handle_t muxer)
{
    if (muxer)
    {
        int32_t track_ID;

        /** mp4_muxer_destroy() no longer destroys the parsers - we need to take care */
        for (track_ID = 1; track_ID <= MAX_STREAMS; track_ID++)
        {
            track_handle_t track = muxer->tracks[track_ID - 1];

            if (track)
            {
                if (track->parser)
                {
                    track->parser->destroy(track->parser);
                }
            }
        }
        mp4_muxer_destroy(muxer);
    }
}

/**
 * open the input source (file only for now)
 */
static int32_t
mux_data_src_create(ema_mp4_ctrl_handle_t handle, int32_t es_idx)
{
    usr_cfg_es_t *usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
    bbio_handle_t ds         = NULL;
    int32_t           err        = 0;

    if (usr_cfg_es->input_mode == EMA_MP4_IO_FILE)
    {
        /** file input source */
        ds = reg_bbio_get('f', 'r');
        assert(ds != NULL);
        handle->data_srcs[es_idx] = ds;  /** keep it in data_srcs to be freed by ema_mp4_mux_destroy() */
        err = ds->open(ds, usr_cfg_es->input_fn);
        if (err)
        {
#ifdef _MSC_VER
            int8_t err_msg[256];
            strerror_s(err_msg, 256, err);
#else
            int8_t *err_msg = strerror (err);
#endif
            msglog(NULL, MSGLOG_INFO, "ERROR! Can't open input file: %s error message: %s. \n", usr_cfg_es->input_fn, err_msg);
            return EMA_MP4_MUXED_OPEN_FILE_ERR;
        }
    }
    else
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Can't support Buffer mode input %s .\n", usr_cfg_es->input_fn);
        return EMA_MP4_MUXED_CLI_ERR;
    }

    return EMA_MP4_MUXED_OK;
}

static void
mux_data_src_destroy(bbio_handle_t data_srcs[])
{
    int32_t es_idx;

    for (es_idx = 0; es_idx < MAX_STREAMS; es_idx++)
    {
        if (data_srcs[es_idx])
        {
            data_srcs[es_idx]->destroy(data_srcs[es_idx]);
        }
    }
}

/**
 * find the right parser based on the input source file extension (currently, only support choosing parser from file name extension) 
 */
static int32_t
mux_es_parser_create(ema_mp4_ctrl_handle_t handle, uint32_t es_idx, parser_handle_t* p_parser, uint32_t dv_el_track_flag)
{
    usr_cfg_es_t *  usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
    int8_t *          es_type  = NULL;
    parser_handle_t parser     = NULL;
    int32_t             ret    = EMA_MP4_MUXED_OK;

    /**** get data source type */
    if (usr_cfg_es->input_mode == EMA_MP4_IO_FILE)
    {
        /** get es type based on file extension */
        es_type = strrchr(usr_cfg_es->input_fn, '.');
        if (!es_type)
        {
            msglog(NULL, MSGLOG_INFO, "ERROR! Input file %s: no file extension. Unknown ES type. \n", usr_cfg_es->input_fn);
            return EMA_MP4_MUXED_UNKNOW_ES;
        }
        es_type++;  /** skip '.' */
    }
    else
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Can't support Buffer mode input %s .\n", usr_cfg_es->input_fn);
        return EMA_MP4_MUXED_CLI_ERR;
    }

    /** get parser: dsi type is mp4 */
    if (es_type)
    {
        parser = reg_parser_get(es_type, DSI_TYPE_MP4FF);
    }

    if (!parser)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Input ES type: %s, extension not supported\n", es_type);
        return EMA_MP4_MUXED_UNKNOW_ES;
    }

    *p_parser = parser;

    if (dv_el_track_flag)
    {
        parser->dv_el_track_flag = 1;
    }

    msglog(NULL, MSGLOG_INFO, "Init %4s parser for stream %u\n", parser->stream_name, es_idx);
    ret = parser->init(parser, &(handle->usr_cfg_mux.ext_timing_info), es_idx, handle->data_srcs[es_idx]);

    if (handle->usr_cfg_mux.dv_bl_non_comp_flag)
    {
        parser->dv_bl_non_comp_flag = 1;
    }
    return ret;
}

/**
 * parses the ES, get the samples and send them to muxer
 */
static int32_t
mux_es_parsing(ema_mp4_ctrl_handle_t handle, uint32_t es_idx, uint32_t dv_el_flag)
{
    bbio_handle_t       ds     = handle->data_srcs[es_idx];
    track_handle_t      track  = NULL;
    parser_handle_t     parser = NULL;
    mp4_sample_handle_t sample;
    progress_handle_t   prgh;
    int32_t                 ret = EMA_MP4_MUXED_OK;

    track = mp4_muxer_get_track(handle->mp4_handle, handle->usr_cfg_ess[es_idx].track_ID);
    if (!track)
    {
        return EMA_MP4_MUXED_BUGGY; 
    }

    parser = track->parser;

    if (dv_el_flag)
    {
        track->BL_track = mp4_muxer_get_track(handle->mp4_handle, handle->usr_cfg_ess[es_idx -1].track_ID);
        if (!track->BL_track)
            return EMA_MP4_MUXED_BUGGY;
    }
    sample = sample_create();
    if (!sample)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }
    prgh = progress_create(parser->stream_name, ds->size(ds));
    if (!prgh)
    {
        sample->destroy(sample);
        return EMA_MP4_MUXED_NO_MEM;
    }

    /** just to be sure */
    src_byte_align(ds);
    /** read sample one by one, add each sample to muxer */
    while (!(ret = parser->get_sample(parser, sample)) || ret == EMA_MP4_MUXED_NO_CONFIG_ERR)
    {
        /** Parsing was successful, add sample to muxer */
        if (!ret)
        {
            /** REPORT_PARSING_PROGRESS */
            if (msglog_global_verbosity_get() >= MSGLOG_INFO)
            {
                if (msglog_global_verbosity_get() != MSGLOG_DEBUG)
                {
                    prgh->show(prgh, ds->position(ds));
                }
                else
                {
                    msglog(NULL, MSGLOG_INFO, "Add sample %d to stream %2d\n",
                           track->sample_num, es_idx);
                }
            }

            if (mp4_muxer_input_sample(track, sample))
            {
                msglog(NULL, MSGLOG_ERR, "ERROR! Parsing ES Error! \n");
                ret = EMA_MP4_MUXED_BUGGY;
                break;
            }
        }
    }
    /** CLOSE_REPORT_PARSING_PROGRESS */
    if (msglog_global_verbosity_get() >= MSGLOG_INFO)
    {
        if (parser->stream_id != STREAM_ID_EMAJ)
        {
            prgh->show(prgh, ds->position(ds));
        }
        else
        {
            msglog(NULL, MSGLOG_INFO, "EMAJ: done");
        }
    }
    msglog(NULL, MSGLOG_INFO, "\n");

    prgh->destroy(prgh);
    sample->destroy(sample);

    if (ret == EMA_MP4_MUXED_EOES)
    {
        return EMA_MP4_MUXED_OK;
    }
    else
    {
        return ret;
    }
}

/**
 * callback function for creating multiple fragmented mp4 files
 */
static int32_t
onWriteNextFrag(void *handle_in)
{
    ema_mp4_ctrl_handle_t handle = (ema_mp4_ctrl_handle_t)handle_in;
    int8_t segment_name[256];
    uint8_t *output_name;
    int8_t *seg_name;
    uint32_t ret = 0;

    if (!handle->usr_cfg_mux.segment_output_flag)
    {
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    handle->mp4_sink->close(handle->mp4_sink);

    output_name = (uint8_t *)(handle->usr_cfg_mux.output_fn);
    seg_name = segment_name;
    while (*output_name != '.')
    {
        *seg_name++ = *output_name++;
    }
    sprintf(seg_name, "_%d.mp4", handle->usr_cfg_mux.SegmentCounter++);

    ret = handle->mp4_sink->open(handle->mp4_sink, (const int8_t *)segment_name);
    if (ret != 0)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Can't open output container");
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    ret = mp4_muxer_output_segment_hdrs(handle->mp4_handle);

    return ret;
}


/****** interface code starts from here */
/**
 * for each track,
 *  - parses input and delimit sample
 *  - adds track metadata and samples to muxer
*/
uint32_t
ema_mp4_mux_start(ema_mp4_ctrl_handle_t handle)
{
    int32_t      es_idx;
    int32_t      has_video = 0;
    int32_t      has_audio = 0;
    usr_cfg_es_t *usr_cfg_es;
    time_t       ltime_s, ltime_e;
    int32_t      ret = EMA_MP4_MUXED_OK;
    usr_cfg_mux_t *   usr_cfg_mux_ptr;

    usr_cfg_mux_ptr = &(handle->usr_cfg_mux);

    if (!handle->usr_cfg_mux.es_num)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! No valid input. \n");
        return EMA_MP4_MUXED_NO_ES;
    }

    if (handle->usr_cfg_mux.output_mode == EMA_MP4_IO_NONE)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! No valid output. \n");
        return EMA_MP4_MUXED_NO_OUTPUT;
    }

    if ( (handle->usr_cfg_mux.ext_timing_info.ext_dv_profile == 1) ||
         (handle->usr_cfg_mux.ext_timing_info.ext_dv_profile == 3) ||
         (handle->usr_cfg_mux.ext_timing_info.ext_dv_profile == 5) )
    {
        if (handle->usr_cfg_mux.dv_track_mode == DUAL)
        {
             msglog(NULL, MSGLOG_ERR, "ERROR! If the input dolby vision stream is Non SDR/HDR compatibility, setting dual track doesn't make sense. \n");
             return EMA_MP4_MUXED_PARAM_ERR;
        }

        handle->usr_cfg_mux.dv_bl_non_comp_flag = 1;
    }

	if((handle->usr_cfg_mux.ext_timing_info.ext_dv_profile == 8 ) && (handle->usr_cfg_mux.ext_timing_info.ext_dv_bl_compatible_id == 0))
	{
             msglog(NULL, MSGLOG_ERR, "Error: For Dolby vision profile 8, dv-bl-compatible-id should be set, value can be 1, 2 or 4.\n");
             return EMA_MP4_MUXED_PARAM_ERR;
	}

    if ( (handle->usr_cfg_mux.output_format == OUTPUT_FORMAT_DASH) ||
         (handle->usr_cfg_mux.output_format == OUTPUT_FORMAT_FRAG_MP4) )
    {
        usr_cfg_mux_ptr->mux_cfg_flags = (
            ISOM_MUXCFG_WRITE_IODS |
            ISOM_MUXCFG_WRITE_CTTS_V1 |
            ISOM_MUXCFG_WRITE_SUBS_V1 |
            ISOM_MUXCFG_WRITE_STSS |
            ISOM_MUXCFG_ENCRYPTSTYLE_CENC
        );

        usr_cfg_mux_ptr->frag_cfg_flags = (
            ISOM_FRAGCFG_FRAGSTYLE_DEFAULT |
            ISOM_FRAGCFG_EMPTY_TREX |
            ISOM_FRAGCFG_FORCE_TFRA |
            ISOM_FRAGCFG_WRITE_TFDT |
            ISOM_FRAGCFG_NO_BDO_IN_TFHD |
            ISOM_FRAGCFG_ONE_TFRA_ENTRY_PER_TRAF |
            ISOM_FRAGCFG_DEFAULT_BASE_IS_MOOF |
            ISOM_FRAGCFG_FORCE_TFHD_SAMPDESCIDX
        );

        usr_cfg_mux_ptr->output_mode |= EMA_MP4_FRAG;
        if (usr_cfg_mux_ptr->frag_range_max == 0)
        {
            /** DASH sepc suggested fragment duration is 2s. */
            usr_cfg_mux_ptr->frag_range_max = 2000; 
        }
        if (usr_cfg_mux_ptr->frag_range_min == 0)
        {
            usr_cfg_mux_ptr->frag_range_min = 1000;
        }

        FREE_CHK((int8_t *)usr_cfg_mux_ptr->major_brand);
        usr_cfg_mux_ptr->major_brand = STRDUP_CHK("mp42");
        usr_cfg_mux_ptr->brand_version = 1;

        if (handle->usr_cfg_mux.output_format == OUTPUT_FORMAT_DASH)
        {
            if (usr_cfg_mux_ptr->dash_profile == OnDemand) 
            {
                usr_cfg_mux_ptr->frag_cfg_flags |= ISOM_FRAGCFG_WRITE_SIDX;
                FREE_CHK((int8_t *)usr_cfg_mux_ptr->compatible_brands);
                usr_cfg_mux_ptr->compatible_brands = STRDUP_CHK("mp42dashdby1msdhmsixiso5isom");
            } 
            else if (usr_cfg_mux_ptr->dash_profile == Main) 
            {
                FREE_CHK((int8_t *)usr_cfg_mux_ptr->compatible_brands);
                usr_cfg_mux_ptr->compatible_brands = STRDUP_CHK("mp42dashdby1msdhiso5isom");
            } 
            else if (usr_cfg_mux_ptr->dash_profile == Live || usr_cfg_mux_ptr->dash_profile == HbbTV) 
            {
                FREE_CHK((int8_t *)usr_cfg_mux_ptr->compatible_brands);
                usr_cfg_mux_ptr->compatible_brands = STRDUP_CHK("mp42dashdby1iso5isom");
                usr_cfg_mux_ptr->segment_output_flag = 1;
                /**** set fragment callback */
                mp4_muxer_set_onwrite_next_frag_callback(handle->mp4_handle, onWriteNextFrag, (void *)(handle));
            }
        }
        else /** for frag-mp4, add the 'sidx' box */
        {
            usr_cfg_mux_ptr->frag_cfg_flags |= ISOM_FRAGCFG_WRITE_SIDX;
        }
        usr_cfg_mux_ptr->SegmentCounter = 1;
    }

    /**** get muxer sink */
    ret = mux_data_sink_create(handle);
    CHK_ERR_RET(ret);

    mp4_muxer_set_sink(handle->mp4_handle, handle->mp4_sink);

    /**** get data sources */
    if ((handle->usr_cfg_mux.dv_track_mode == DUAL) && (handle->usr_cfg_mux.dv_es_mode == COMB))
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Muxer can't support single VES(BL+EL+RPU) input and dual track output mode as RPU reorder needed! \n");
        return EMA_MP4_MUXED_CLI_ERR;
    }
    else if ((handle->usr_cfg_mux.dv_track_mode == SINGLE) && (handle->usr_cfg_mux.dv_es_mode == SPLIT))
    {
        msglog(NULL, MSGLOG_ERR, "ERROR! Muxer can't support dual VES(BL and EL+RPU) input and single track output mode as RPU reorder needed! \n");
        return EMA_MP4_MUXED_CLI_ERR;
    }

    for (es_idx = 0; es_idx < handle->usr_cfg_mux.es_num; es_idx++)
    {
        ret = mux_data_src_create(handle, es_idx);
        CHK_ERR_RET(ret);
    }
    
    /**** write all other boxes */

    /** parsing all ES source */
    for (es_idx = 0; es_idx < handle->usr_cfg_mux.es_num; es_idx++)
    {
        usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
        handle->mp4_handle->curr_usr_cfg_stream_index = es_idx;

        /** check if track to add, delete, replace does even exist */
        if (usr_cfg_es->mp4_tid > handle->usr_cfg_mux.es_num)
        {
            msglog(NULL, MSGLOG_ERR, "ERROR! Mp4 file does not contain track ID %i.\n", usr_cfg_es->mp4_tid);
            return EMA_MP4_MUXED_UNKNOW_ES;
        }

        {
            /** ES source */
            /** create parser */
            parser_handle_t parser = 0;

            ret = mux_es_parser_create(handle, es_idx, &parser, 0);
                
            /** When loop gets already aborted here the locally allocated parser does not get assigned
                to any track (aborting loop before calling mp4_muxer_add_track) and hence the parser
                can never get freed as it is only present in local scope. So it must be done here. */
            if (ret != EMA_MP4_MUXED_OK && parser)
            {
                parser->destroy(parser);
            }
            CHK_ERR_CNT(ret);

            /** set tkhd flags and alternative group */
            handle->usr_cfg_ess[es_idx].force_tkhd_flags = (handle->usr_cfg_mux.output_format == OUTPUT_FORMAT_MP4) ? 0xF : 0x7;
            if(parser->stream_type == STREAM_TYPE_VIDEO)
            {
                handle->usr_cfg_ess[es_idx].alternate_group = 1;
                if(has_video)
                    handle->usr_cfg_ess[es_idx].force_tkhd_flags &=  0xE;
                has_video = 1;
            }
            else if(parser->stream_type == STREAM_TYPE_AUDIO)
            {
                handle->usr_cfg_ess[es_idx].alternate_group = 2;
                if(has_audio)
                    handle->usr_cfg_ess[es_idx].force_tkhd_flags &=  0xE;
                has_audio = 1;            
            }

            /** add a track to muxer */
            handle->usr_cfg_ess[es_idx].track_ID =
                mp4_muxer_add_track(handle->mp4_handle, parser, &(handle->usr_cfg_ess[es_idx]));
            if (!handle->usr_cfg_ess[es_idx].track_ID)
            {
                ret = EMA_MP4_MUXED_NO_MEM;
            }
            CHK_ERR_CNT(ret);

            /** parse ES */
            msglog(NULL, MSGLOG_INFO, "\nParsing ES...\n");
            time(&ltime_s);

            ret = mux_es_parsing(handle, es_idx, 0);
            CHK_ERR_RET(ret);

            time(&ltime_e);
            msglog(NULL, MSGLOG_INFO, "Time lapse %lds\n", ltime_e - ltime_s);

            /** dolby vision el track parser*/
            if((((handle->usr_cfg_mux.dv_track_mode == DUAL) && (handle->usr_cfg_mux.dv_es_mode == SPLIT))) 
                && ((IS_FOURCC_EQUAL(parser->dsi_FourCC, "avcC")) || (IS_FOURCC_EQUAL(parser->dsi_FourCC, "hvcC"))))
            {
                es_idx ++;
                ret = mux_es_parser_create(handle, es_idx, &parser, 1);
                if (ret != EMA_MP4_MUXED_OK && parser)
                {
                    parser->destroy(parser);
                }
                CHK_ERR_CNT(ret);
                /** add a track to muxer */
                handle->usr_cfg_ess[es_idx].track_ID =
                    mp4_muxer_add_track(handle->mp4_handle, parser, &(handle->usr_cfg_ess[es_idx]));
                if (!handle->usr_cfg_ess[es_idx].track_ID)
                {
                    ret = EMA_MP4_MUXED_NO_MEM;
                }
                CHK_ERR_CNT(ret);

                ret = mux_es_parsing(handle, es_idx, 1);
                CHK_ERR_RET(ret);
            }
        }
    }

    /**** the summary part of mp4 file is ready and output */
    msglog(NULL, MSGLOG_INFO, "Output headers\n");
    ret = mp4_muxer_output_hdrs(handle->mp4_handle);  /** top level none media specific info */
    CHK_ERR_RET(ret);
    /** output mp4 tracks */
    msglog(NULL, MSGLOG_INFO, "\nOutput tracks\n");
    ret = mp4_muxer_output_tracks(handle->mp4_handle);
    CHK_ERR_RET(ret);

    msglog(NULL,MSGLOG_INFO,"\n");
    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_create(ema_mp4_ctrl_handle_t *handle)
{
    usr_cfg_mux_t *       usr_cfg_mux_ptr;
    int32_t               es_idx;
    ema_mp4_ctrl_handle_t handle_internal;

    /**** init system */
    MEM_CHK_INIT();
#ifdef DEBUG
    msglog_global_verbosity_set(MSGLOG_WARNING);   /** the level for msglog() messages defaults to warning in debug builds */
#else
    msglog_global_verbosity_set(MSGLOG_ERR);  /** the level for msglog() messages defaults to error in release builds */
#endif

    /**** init registry data base */
    reg_parser_init();

    /*** register video parser */
    parser_hevc_reg();   /** register hevc parser */
    parser_avc_reg();    /** register avc parser */

    /*** register audio parser */
    parser_aac_reg();    /** register aac parser */
    parser_ac3_reg();    /** register ac3 parser */
    parser_ec3_reg();    /** register ec3 parser */
    parser_ac4_reg();    /** register ac4 parser */

    /** I/O */
    reg_bbio_init();
    bbio_file_reg();
    bbio_buf_reg();

    /**** create and init ema_mp4_mux */
    handle_internal = (ema_mp4_ctrl_handle_t)MALLOC_CHK(sizeof(ema_mp4_ctrl_t));
    if (!handle_internal)
    {
        *handle = NULL;
        return EMA_MP4_MUXED_NO_MEM;
    }

    *handle = handle_internal;
    memset(handle_internal, 0, sizeof(ema_mp4_ctrl_t));

    /**** init usr_cfg_mux */
    usr_cfg_mux_ptr = &(handle_internal->usr_cfg_mux);
    memset(usr_cfg_mux_ptr, 0, sizeof(usr_cfg_mux_t));

    usr_cfg_mux_ptr->output_mode            = EMA_MP4_IO_FILE;        /** default to file output */
    usr_cfg_mux_ptr->output_fn              = STRDUP_CHK("test.mp4"); /** default output test.mp4 */
    usr_cfg_mux_ptr->output_format          = OUTPUT_FORMAT_MP4;      /** default output mp4 */
    usr_cfg_mux_ptr->timescale              = 600;                    /** default time scale in ms */
    usr_cfg_mux_ptr->mux_cfg_flags          = ISOM_MUXCFG_DEFAULT;    /** default to write iods */
    usr_cfg_mux_ptr->free_box_in_moov_size  = 0;                      /** no free box insertion at end of moov */
    usr_cfg_mux_ptr->ext_timing_info.override_timing   = 0;           /** not over ride timing info in sps */
    usr_cfg_mux_ptr->ext_timing_info.time_scale        = 30000;
    usr_cfg_mux_ptr->ext_timing_info.num_units_in_tick = 1000;        /** default time_scale/num_units_in_tick for video frame-rate: 30 fps */
    usr_cfg_mux_ptr->ext_timing_info.ext_dv_profile    = 0xff;
	usr_cfg_mux_ptr->ext_timing_info.ac4_bitrate = 0;
	usr_cfg_mux_ptr->ext_timing_info.ac4_bitrate_precision = 0xffffffff;
    usr_cfg_mux_ptr->fix_cm_time            = 0;
    usr_cfg_mux_ptr->chunk_span_time        = 250;                    /** default 250ms */
    usr_cfg_mux_ptr->frag_cfg_flags         = ISOM_FRAGCFG_DEFAULT;
    usr_cfg_mux_ptr->frag_range_max         = 0;
    usr_cfg_mux_ptr->frag_range_min         = 0;
    usr_cfg_mux_ptr->major_brand            = STRDUP_CHK("mp42");         /** default major brand: iso2 */
    usr_cfg_mux_ptr->compatible_brands      = STRDUP_CHK("mp42dby1isom"); /** default compatible brands: isom */
    usr_cfg_mux_ptr->brand_version          = 1;
    usr_cfg_mux_ptr->sd                     = 1;                          /** default to multiple stsd entry */
    usr_cfg_mux_ptr->withopt                = 0;
    usr_cfg_mux_ptr->max_pdu_size           = 0;
    usr_cfg_mux_ptr->es_num                 = 0;
    usr_cfg_mux_ptr->OD_profile_level       = UNKNOWN_PROFILE;
    usr_cfg_mux_ptr->scene_profile_level    = UNKNOWN_PROFILE;
    usr_cfg_mux_ptr->audio_profile_level    = UNKNOWN_PROFILE;
    usr_cfg_mux_ptr->video_profile_level    = UNKNOWN_PROFILE;
    usr_cfg_mux_ptr->graphics_profile_level = UNKNOWN_PROFILE;
    usr_cfg_mux_ptr->elst_track_id          = 0;
    usr_cfg_mux_ptr->dash_profile           = OnDemand;


    handle_internal->mp4_handle = mp4_muxer_create(usr_cfg_mux_ptr, handle_internal->usr_cfg_ess);
    if (!handle_internal->mp4_handle)
    {
        FREE_CHK(handle);
        return EMA_MP4_MUXED_NO_MEM;
    }

    /**** init usr_cfg_ess to 0 */
    memset(handle_internal->usr_cfg_ess, 0, sizeof(handle_internal->usr_cfg_ess));
    for (es_idx = 0; es_idx < MAX_INPUT_ES_NUM; es_idx++)
    {
        handle_internal->usr_cfg_ess[es_idx].es_idx = es_idx;
    }

    /**** init data sink to 0 */
    handle_internal->mp4_sink = 0;

    /**** init data_scrs  to 0 */
    memset(handle_internal->data_srcs, 0, sizeof(handle_internal->data_srcs));

    return 0;
}

void
ema_mp4_mux_destroy(ema_mp4_ctrl_handle_t handle)
{
    int32_t es_idx;
    usr_cfg_mux_t *usr_cfg_mux_ptr;

    usr_cfg_mux_ptr = &(handle->usr_cfg_mux);

    mux_data_src_destroy(handle->data_srcs);

    if (handle->mp4_handle)
    {
        mux_muxer_destroy(handle->mp4_handle);
        handle->mp4_handle = 0;
    }

    if (handle->mp4_sink)
    {
        mux_data_sink_destroy(handle->mp4_sink);
        handle->mp4_sink = 0;
    }

    for (es_idx = 0; es_idx < usr_cfg_mux_ptr->es_num; es_idx++)
    {
        usr_cfg_es_t *usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
        /**** free cfg space */
        FREE_CHK((int8_t *)usr_cfg_es->input_fn);
        FREE_CHK((int8_t *)usr_cfg_es->lang);
        FREE_CHK((int8_t *)usr_cfg_es->enc_name);
    }

    FREE_CHK((int8_t *)usr_cfg_mux_ptr->output_fn);
    FREE_CHK((int8_t *)usr_cfg_mux_ptr->output_fn_el);
    FREE_CHK((int8_t *)usr_cfg_mux_ptr->major_brand);
    FREE_CHK((int8_t *)usr_cfg_mux_ptr->compatible_brands);

    FREE_CHK(handle->fn_in);
    if ((handle->mp4_src) && (handle->demux_flag))
    {
        handle->mp4_src->destroy(handle->mp4_src);
    }
    FREE_CHK(handle->fn_out);

    FREE_CHK(handle);

    handle = 0;
}

uint32_t
ema_mp4_mux_set_input(ema_mp4_ctrl_handle_t handle, 
                      int8_t *fn, 
                      int8_t *lang, 
                      int8_t *enc_name, 
                      uint32_t time_scale, 
                      uint32_t chunk_span_size, 
                      uint32_t tid)
{
    usr_cfg_es_t *usr_cfg_es;

    if (handle->usr_cfg_mux.es_num == MAX_INPUT_ES_NUM)
    {
        return EMA_MP4_MUXED_TOO_MANY_ES;
    }

    usr_cfg_es = &(handle->usr_cfg_ess[handle->usr_cfg_mux.es_num]);
    if (fn)
    {
        usr_cfg_es->input_mode = EMA_MP4_IO_FILE;
        usr_cfg_es->input_fn   = STRDUP_CHK(fn);
    }
    else
    {
        /** not file in => buf in */
        usr_cfg_es->input_mode = EMA_MP4_IO_BUF;
        usr_cfg_es->input_fn   = 0;
    }
    /** Check input file exist or not */
    {
        FILE *input_check = NULL;

        if (usr_cfg_es->input_fn)
        {
            input_check = fopen(usr_cfg_es->input_fn, "r");
        }

        if (input_check)
        {
            fclose(input_check);
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, "ERROR! Can't open input file: %s\n", usr_cfg_es->input_fn);
            return EMA_MP4_MUXED_PARAM_ERR;
        }
    }
    /** check input lang length */
    if (lang)
    {
        if (strlen(lang) != 3)
        {
           msglog(NULL, MSGLOG_ERR, "ERROR! Input lang code:%s is not correct! \n", lang);
           return EMA_MP4_MUXED_PARAM_ERR;
        }

        usr_cfg_es->lang            = (lang) ? STRDUP_CHK(lang) : 0;
    }
    
    usr_cfg_es->enc_name        = (enc_name) ? STRDUP_CHK(enc_name) : 0;
    /** chunk_span_size: 0 means no chunk span control by size */
    usr_cfg_es->chunk_span_size = 0;
    usr_cfg_es->mp4_tid         = tid;
    usr_cfg_es->warp_media_timescale = time_scale;
    /** mark for add */
    usr_cfg_es->action = TRACK_EDIT_ACTION_ADD;  
    handle->usr_cfg_mux.es_num++;

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_output(ema_mp4_ctrl_handle_t handle, int32_t buf_out, const int8_t *fn)
{
    if (!buf_out && !fn)
    {
        return EMA_MP4_MUXED_PARAM_ERR;  /** should have output */
    }

    if (buf_out)
    {
        handle->usr_cfg_mux.output_mode |= EMA_MP4_IO_BUF;
    }
    else
    {
        handle->usr_cfg_mux.output_mode &= ~EMA_MP4_IO_BUF;
    }

    if (handle->usr_cfg_mux.output_file_num == 1)
    {
        FREE_CHK((int8_t *)handle->usr_cfg_mux.output_fn_el);
        handle->usr_cfg_mux.output_mode &= ~EMA_MP4_IO_FILE;
        if (fn)
        {
            handle->usr_cfg_mux.output_fn_el    = STRDUP_CHK(fn);
            handle->usr_cfg_mux.output_mode |= EMA_MP4_IO_FILE;
        }

        handle->usr_cfg_mux.output_file_num = 2;
        return EMA_MP4_MUXED_OK;
    }

    FREE_CHK((int8_t *)handle->usr_cfg_mux.output_fn);
    handle->usr_cfg_mux.output_mode &= ~EMA_MP4_IO_FILE;
    if (fn)
    {
        handle->usr_cfg_mux.output_fn    = STRDUP_CHK(fn);
        handle->usr_cfg_mux.output_mode |= EMA_MP4_IO_FILE;
    }

    handle->usr_cfg_mux.output_file_num = 1;
    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_moov_timescale(ema_mp4_ctrl_handle_t handle, uint32_t timescale)
{
    handle->usr_cfg_mux.timescale = timescale;
    handle->mp4_handle->timescale = timescale;

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_cm_time(ema_mp4_ctrl_handle_t handle, uint32_t cmtimeh, uint32_t cmtimel)
{
    handle->usr_cfg_mux.fix_cm_time = (((uint64_t)cmtimeh) << 32) | cmtimel;

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_chunk_span_time(ema_mp4_ctrl_handle_t handle, uint32_t chunk_span_time)
{
    handle->usr_cfg_mux.chunk_span_time = chunk_span_time;

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_mbrand(ema_mp4_ctrl_handle_t handle, const int8_t *mbrand)
{
    if (!mbrand)
    {
        return EMA_MP4_MUXED_PARAM_ERR;  /** must have mbrand */
    }

    FREE_CHK((int8_t *)handle->usr_cfg_mux.major_brand);
    handle->usr_cfg_mux.major_brand = STRDUP_CHK(mbrand);

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_cbrand(ema_mp4_ctrl_handle_t handle, const int8_t *cbrand)
{
    if (!cbrand)
    {
        return EMA_MP4_MUXED_PARAM_ERR;  /** must have cbrand */
    }

    FREE_CHK((int8_t *)handle->usr_cfg_mux.compatible_brands);
    handle->usr_cfg_mux.compatible_brands = STRDUP_CHK(cbrand);

    return EMA_MP4_MUXED_OK;
}


uint32_t
ema_mp4_mux_set_withopt(ema_mp4_ctrl_handle_t handle, const int8_t *opt)
{
    if (!OSAL_STRCASECMP(opt, "64")) 
    {
        handle->usr_cfg_mux.withopt |= 0x1;  /** the only withopt for now */
    }

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_sd(ema_mp4_ctrl_handle_t handle, const int8_t *sd)
{
    if (!OSAL_STRCASECMP(sd,"single"))        
    {
        handle->usr_cfg_mux.sd = 0x0;
    }
    else if (!OSAL_STRCASECMP(sd,"multiple")) 
    {
        handle->usr_cfg_mux.sd = 0x1;
    }
    else 
    {
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_db_level(ema_mp4_ctrl_handle_t handle, int8_t *lvl)
{
    msglog_level_t level;

    (void)handle;

    if (!OSAL_STRCASECMP(lvl, "quiet"))        
    {
        level = MSGLOG_QUIET;
    }
    else if (!OSAL_STRCASECMP(lvl, "panic"))   
    {
        level = MSGLOG_EMERG;
    }
    else if (!OSAL_STRCASECMP(lvl, "fatal"))   
    {
        level = MSGLOG_CRIT;
    }
    else if (!OSAL_STRCASECMP(lvl, "error"))   
    {
        level = MSGLOG_ERR;
    }
    else if (!OSAL_STRCASECMP(lvl, "warning")) 
    {
        level = MSGLOG_WARNING;
    }
    else if (!OSAL_STRCASECMP(lvl, "info"))    
    {
        level = MSGLOG_INFO;
    }
    else if (!OSAL_STRCASECMP(lvl, "verbose")) 
    {
        level = MSGLOG_DEBUG;  /** verbose no longer supported */
    }
    else if (!OSAL_STRCASECMP(lvl, "debug"))   
    {
        level = MSGLOG_DEBUG;
    }
    else                                       
    {
        level = msglog_global_verbosity_get();  /** unchanged */
    }

    msglog_global_verbosity_set(level);

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_output_format(ema_mp4_ctrl_handle_t handle, const int8_t *outfm)
{
    if (!outfm)
    {
        return EMA_MP4_MUXED_PARAM_ERR; 
    }

    if (OSAL_STRCASECMP(outfm, "frag-mp4") && OSAL_STRCASECMP(outfm, "mp4")) 
    {
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    if (!OSAL_STRCASECMP(outfm, "frag-mp4"))
    {
        handle->usr_cfg_mux.output_format = OUTPUT_FORMAT_FRAG_MP4;
    }
    else
    {
        handle->usr_cfg_mux.output_format = OUTPUT_FORMAT_MP4;
    }

    return EMA_MP4_MUXED_OK;
}

uint32_t 
ema_mp4_mux_set_max_duration(ema_mp4_ctrl_handle_t handle, uint32_t max_duration)
{
    handle->usr_cfg_mux.frag_range_max = max_duration;

    return EMA_MP4_MUXED_OK;
}


uint32_t
ema_mp4_mux_set_video_framerate(ema_mp4_ctrl_handle_t handle, uint32_t nome, uint32_t deno)
{
    handle->usr_cfg_mux.ext_timing_info.override_timing   = 1;       
    handle->usr_cfg_mux.ext_timing_info.time_scale        = nome;
    handle->usr_cfg_mux.ext_timing_info.num_units_in_tick = deno;     

    return EMA_MP4_MUXED_OK;
}

uint32_t
ema_mp4_mux_set_dv_es_mode(ema_mp4_ctrl_handle_t handle, const int8_t *mode)
{
    if (!mode)
    {
        return EMA_MP4_MUXED_PARAM_ERR;  
    }

    if (!OSAL_STRCASECMP(mode, "split"))
    {
        handle->usr_cfg_mux.dv_es_mode = SPLIT;
        handle->usr_cfg_mux.dv_track_mode = DUAL;
    }
    else if (!OSAL_STRCASECMP(mode, "comb"))
    {
        handle->usr_cfg_mux.dv_es_mode = COMB;
        handle->usr_cfg_mux.dv_track_mode = SINGLE;
    }
    else
    {
        return EMA_MP4_MUXED_PARAM_ERR; 
    }

    return EMA_MP4_MUXED_OK;
}

uint32_t 
ema_mp4_mux_set_dv_profile(ema_mp4_ctrl_handle_t handle, int32_t profile)
{
    if ((profile == 4) || (profile == 5) || (profile >= 7) && (profile <= 9)) 
    {
		handle->usr_cfg_mux.ext_timing_info.ext_dv_profile = (uint8_t)profile;
        return EMA_MP4_MUXED_OK;
    }
    else
    {
        return EMA_MP4_MUXED_PARAM_ERR;
    }
}

uint32_t
ema_mp4_mux_set_dv_bl_compatible_id(ema_mp4_ctrl_handle_t handle, int32_t compatible_id)
{
	if ((compatible_id >= 0) && (compatible_id <= 6)) 
    {
        handle->usr_cfg_mux.ext_timing_info.ext_dv_bl_compatible_id = (uint8_t)compatible_id;
        return EMA_MP4_MUXED_OK;
    }
    else
    {
        return EMA_MP4_MUXED_PARAM_ERR;
    }
}

uint32_t
ema_mp4_mux_set_sampleentry_dvh1(ema_mp4_ctrl_handle_t handle, int32_t es_idx)
{
    if (es_idx >= 0 && es_idx < handle->usr_cfg_mux.es_num)
    {
        usr_cfg_es_t *usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
        usr_cfg_es->sample_entry_name = "dvh1";
        return EMA_MP4_MUXED_OK;
    }
    else
    {
        msglog(NULL, MSGLOG_ERR,
                "Error parsing command line: Unknown es index for --dvh1flag.\n");
        return EMA_MP4_MUXED_PARAM_ERR;
    }
}

uint32_t
ema_mp4_mux_set_sampleentry_hvc1(ema_mp4_ctrl_handle_t handle, int32_t es_idx)
{
    if (es_idx >= 0 && es_idx < handle->usr_cfg_mux.es_num)
    {
        usr_cfg_es_t *usr_cfg_es = &(handle->usr_cfg_ess[es_idx]);
        usr_cfg_es->sample_entry_name = "hvc1";
        return EMA_MP4_MUXED_OK;
    }
    else
    {
        msglog(NULL, MSGLOG_ERR,
                "Error parsing command line: Unknown es index for --hvc1flag.\n");
        return EMA_MP4_MUXED_PARAM_ERR;
    }
}