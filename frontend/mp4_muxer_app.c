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
    @file  mp4_mpuxer_app.c
    @brief Implements mp4muxer applicaion
*/

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"          /** OSAL_xyz() */
#include "mp4_muxer.h"      /** EMA_MP4_FRAG */
#include "ema_mp4_ifc.h"    /** ema_mp4_ctrl_handle_t */


static void
show_version(void)
{
    const mp4base_version_info* mp4base_version = mp4base_get_version();

    msglog(NULL, MSGLOG_CRIT, "%s\n", "Copyright (c) 2008-2017 Dolby Laboratories, Inc. All Rights Reserved\n");
    msglog(NULL, MSGLOG_CRIT, "MP4muxer version: %s (build: %s)\n",
           mp4base_version->text, __DATE__);
}

static void
mp4muxer_usage(void)
{
    msglog(NULL, MSGLOG_CRIT,
                "Usage: mp4muxer arg [options]\n\n");
    msglog(NULL, MSGLOG_CRIT,
                " Args:       [Options]              Descriptions: \n");
    msglog(NULL, MSGLOG_CRIT,
                " -----       --------------------   -------------------------------------------------------\n");
    msglog(NULL, MSGLOG_CRIT,
                " --help,-h                          = Shows the help information.\n"
                " --version,-v                       = Shows the version information.\n"
                " --input-file,-i <file.ext> [--media-lang <language>] \n" 
                "                            [--media-timescale <timescale>] \n"
                "                            [--input-video-frame-rate <framerate>]\n"
                "                                    = Adds elementary stream (ES) file.ext with\n"
                "                                      media language, timescale, and framerate(only for video,such as 23.97 or 30000/1001).\n"
                "                                      Supports H264, H265, AC3, EC3, and AC4.\n"
                " --output-file, -o <file.mp4>       = Sets the output file name.\n"
                " --overwrite                        = Overwrites the existing output .mp4 file if there is one.\n"
                " --mpeg4-timescale <arg>            = Overrides the timescale of the entire presentation.\n"
                " --mpeg4-brand <arg>                = Specifies the ISO base media file format brand in the format.\n"
                " --mpeg4-comp-brand <arg>           = Specifies the ISO base media file format compatible brand(s), \n" 
                "                                      in the format of a comma separated list,for example ABCD,EFGH.\n"
                " --output-format <arg>              = Sets the output file format or the specification to which the\n"
                "                                      output file must conform. Valid values include 'mp4' and 'frag-mp4'. \n" 
                "                                      'mp4' is the default value.\n"
                " --mpeg4-max-frag-duration <arg>    = Sets the maximum fragment duration in milliseconds. \n" 
                "                                      By default, the max duration is 2s.\n"
                " --dv-input-es-mode <arg>           = Specifies the Dolby Vision video elementary stream input mode:\n"
                "                                      'comb':  BL, EL, and RPU are combined into a single file;'comb' is the default mode. \n"
                "                                      'split': BL and EL+RPU are multiplexed into two separated elementary stream files.\n" 
                " --dv-profile <arg>                 = Sets the Dolby Vision profile. This option is MANDATORY for \n"
                "                                      DoVi elementary stream: Valid profile values are:\n"
                "                                      0 - dvav.per, BL codec: AVC;    EL codec: AVC;    BL compatibility: SDR/HDR.   \n"
                "                                      1 - dvav.pen, BL codec: AVC;    EL codec: AVC;    BL compatibility: None.      \n"
                "                                      2 - dvhe.der, BL codec: HEVC8;  EL codec: HEVC8;  BL compatibility: SDR/HDR.   \n"
                "                                      3 - dvhe.den, BL codec: HEVC8;  EL codec: HEVC8;  BL compatibility: None.      \n"
                "                                      4 - dvhe.dtr, BL codec: HEVC10; EL codec: HEVC10; BL compatibility: SDR/HDR.   \n"
                "                                      5 - dvhe.stn, BL codec: HEVC10; EL codec: N/A;    BL compatibility: None.      \n"
                "                                      6 - dvhe.dth, BL codec: HEVC10; EL codec: HEVC10; BL compatibility: CEA HDR10. \n"
                "                                      7 - dvhe.dtb, BL codec: HEVC10; EL codec: HEVC10; BL compatibility: Blue-ray HDR10. \n"
                "                                     >7 - Reserved \n"
                "\n\n");

    msglog(NULL, MSGLOG_CRIT, "mp4muxer usage examples: \n"
           "---------------------------------------------------\n"
           "To create an audio-only .mp4 file with EC-3 audio:\n"
           "   mp4muxer -o output.mp4 -i audio.ec3\n\n"
           "To multiplex AC-4 audio and H.264 video:\n"
           "   mp4muxer -o output.mp4 -i audio.ac4 -i video.h264\n\n"
           "To multiplex Dolby vision BL+EL+RPU into a general(non-fragmented) single-track mp4:\n"
           "   mp4muxer -i ves_bl_el_rpu.264 -o single_track_output.mp4 --dv-profile 0 --overwrite \n\n"

           "To multiplex Dolby vision BL file and EL+RPU file into a dual-track .mp4 file with EC-3 audio track:\n"
           "   mp4muxer -i ves_bl.265 -i ves_el_rpu.265 --dv-input-es-mode split -i audio.ec3 -o dual_track_output.mp4 \n" 
           "            -dv-profile 2 --overwrite \n"
           "   Note: The audio input must not separate the BL and EL input. \n\n"

           "To multiplex Dolby Vision into fragmented mp4 file, when the input is a single file containing \n"
           "combined BL, EL, and RPU, and the output is one .mp4 file:  \n"
           "   mp4muxer -i ves_bl_el_rpu.264 -o dash_output.mp4 --output-format frag-mp4 --mpeg4-max-frag-duration 2200 \n"
           "            --dv-profile 0 --overwrite \n"
           "   Note: For fragmented mp4, the default fragment duration is 2s. And each fragment must start with the Random \n"
           "         Access Point (for H.264/H.265, it must be an IDR). \n"
           "         If the distance between two contiguous RAPs is longer than 2s, it's impossible to create fragment\n"
           "         correctly by using the default max fragment duration(2s).\n"
           "         In this case, the command line must be used and the value be bigger than the max RAP distance. \n"

           "To multiplex Dolby vision into fragmented mp4 file, when the input contains two files with one file \n"
           "for BL and the other for EL+RPU, and the output is one .mp4 file:  \n"
           "   mp4muxer -i ves_bl.264 -i ves_el_rpu.264 --dv-input-es-mode split -o dash_output.mp4 --output-format frag-mp4 \n"
           "            --mpeg4-max-frag-duration 2500 --dv-profile 0 --overwrite \n"
           "   Note1: The BL elementary stream file must be put in the first place followed by the EL+RPU file. \n"
           "   Note2: The output file have two tracks: one BL track and one EL track.  \n\n"
           );
}

static int32_t
parse_cli(ema_mp4_ctrl_handle_t handle, int32_t argc, int8_t **argv)
{
    int32_t       ret = EMA_MP4_MUXED_OK;
    int32_t       ua = 0, ub = 0, ts = 0;
    int32_t       overwrite_flag = 0;
    int32_t       output_file_exist_flag = 0;

    /*** input summary(default log level is info) */
    msglog(NULL, MSGLOG_DEBUG, "CLI input: ");
    for (ua = 0; ua < argc; ua++)
    {
        msglog(NULL, MSGLOG_DEBUG, "%s ", argv[ua]);
    }
    msglog(NULL, MSGLOG_DEBUG, "\n");

    if (argc == 1)
    {
        msglog(NULL, MSGLOG_ERR,"Error parsing command line, using '-h' for more info.\n");
        return EMA_MP4_MUXED_CLI_ERR;
    }

    /**** CLI parser */
    for(--argc,++argv; argc && ret == EMA_MP4_MUXED_OK; argc--,argv++)
    {
        const int8_t* opt = *argv;
        
        if (OSAL_STRCASECMP(opt, "--overwrite"))
        {
            argv++;
            argc--;
        }

        if (!OSAL_STRCASECMP(opt, "-h") || !OSAL_STRCASECMP(opt, "--help"))
        {
            mp4muxer_usage();
            ret = EMA_MP4_MUXED_EXIT;
            break;
        }
        else if (!OSAL_STRCASECMP(opt, "-v") || !OSAL_STRCASECMP(opt, "--version"))
        {
            show_version();
            ret = EMA_MP4_MUXED_EXIT;
            break;
        }
        else if (!OSAL_STRCASECMP(opt, "--overwrite"))
        {
            overwrite_flag = 1;
        }
        else if (!argc)
        {
            /** since we follow (opt, val) pair rule except for help, info and version */
            ret = EMA_MP4_MUXED_PARAM_ERR;
            msglog(NULL, MSGLOG_ERR, "Error parsing command line: Unknown option: %s \n\n",opt);
            break;
        } /** we have at least one opt value pair afterward */
        else if (!OSAL_STRCASECMP(opt, "--input-file") || !OSAL_STRCASECMP(opt, "-i"))
        {
            int8_t *fn = *argv, *lang = NULL, *enc_name = NULL;
            ua = 0;
            ub = 0;
            ts = 0;
            /** to probe if have optional input */
            ret = EMA_MP4_MUXED_PARAM_ERR;
            while (argc > 2)
            {
                opt = argv[1];
                if (!OSAL_STRCASECMP(opt, "--media-lang"))
                {
                    lang  = argv[2];
                    argc -= 2;
                    argv += 2;
                }
                else if (!OSAL_STRCASECMP(opt, "--media-timescale"))
                {
                    OSAL_SSCANF(argv[2], "%u", &ts);
                    argc -= 2;
                    argv += 2;
                }
                else if (!OSAL_STRCASECMP(opt, "--input-video-frame-rate"))
                {
                    int8_t *fn = argv[2];
                    uint32_t float_flag = 0, div_flag = 0;
                    uint32_t framerate_nome = 0, framerate_deno = 0;

                    while (*fn)
                    {
                        if (*fn == '.')
                        {
                            float_flag = 1;
                            break;
                        }
                        else if (*fn == '/')
                        {
                            div_flag = 1;
                            break;
                        }
                        else
                        {
                            fn++;
                        }
                    }
                    if (float_flag)
                    {
                        framerate_nome = (uint32_t)(atof(argv[2]) * 1000);
                        framerate_deno = 1000;
                    }
                    else if (div_flag)
                    {
                        framerate_nome = atoi(argv[2]);
                        framerate_deno = atoi((++fn));
                    }
                    else
                    {
                        /** input framerate is integer */
                        framerate_nome = atoi(argv[2]);
                        framerate_deno = 1;
                        
                        if (!framerate_nome)
                        {
                            ret = EMA_MP4_MUXED_PARAM_ERR;
                            msglog(NULL, MSGLOG_ERR, "Error parsing command line: unsupported frame-rate format %s \n\n",argv[2]);
                            break;
                        }
                    }
                    ret = ema_mp4_mux_set_video_framerate(handle, framerate_nome, framerate_deno);
                    
                    argc -= 2;
                    argv += 2;
                }
                else
                {
                    break;
                }
            }
            ret = ema_mp4_mux_set_input(handle, fn, lang, enc_name, ts, ua, ub);
        }
        else if (!OSAL_STRCASECMP(opt, "--output-file") || !OSAL_STRCASECMP(opt, "-o"))
        {
            int8_t *fn = *argv;
            FILE * test_input = NULL;
            ua = 0;
            ub = 0;
            /** to probe if have optional input */
            ret = EMA_MP4_MUXED_PARAM_ERR;
            /** check output file exist or not */
            test_input = fopen(fn, "r");
            if (test_input)
            {
                fclose(test_input);
                output_file_exist_flag = 1;
            }

            ret = ema_mp4_mux_set_output(handle, 0, fn);
        }
        else if (!OSAL_STRCASECMP(opt, "--mpeg4-timescale"))
        {
            OSAL_SSCANF(*argv, "%u", &ua);
            ret = ema_mp4_mux_set_moov_timescale(handle, ua);
        }
        else if (!OSAL_STRCASECMP(opt, "--mpeg4-brand"))
        {
            ret = ema_mp4_mux_set_mbrand(handle, *argv);
        }
        else if (!OSAL_STRCASECMP(opt, "--mpeg4-comp-brand"))
        {
            ret = ema_mp4_mux_set_cbrand(handle, *argv);
        }
        else if (!OSAL_STRCASECMP(opt, "--output-format"))
        {
            ret = ema_mp4_mux_set_output_format(handle, *argv);
            if (ret != EMA_MP4_MUXED_OK)
            {
                msglog(NULL, MSGLOG_ERR, 
                       "Error parsing command line: Unknown output format: %s \n\n",*argv);
            }
        }
        else if (!OSAL_STRCASECMP(opt, "--mpeg4-max-frag-duration"))
        {
            OSAL_SSCANF(*argv, "%u", &ua);
            ret = ema_mp4_mux_set_max_duration(handle, ua);
        }
        else if (!OSAL_STRCASECMP(opt, "--dv-input-es-mode"))
        {
            ret = ema_mp4_mux_set_dv_es_mode(handle, *argv);
        }
        else if (!OSAL_STRCASECMP(opt, "--dv-profile"))
        {
            OSAL_SSCANF(*argv, "%u", &ua);
            ret = ema_mp4_mux_set_dv_profile(handle, (int)ua);
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, 
                "Error parsing command line: Unknown option: %s \n\n",opt);
            ret = EMA_MP4_MUXED_PARAM_ERR;
        }
    }

    if (ret == EMA_MP4_MUXED_OK)
    {

        /** output file overwrite check */
        /** if no "--overwrite" option, if the output file had been exist, return error and exit.*/
        /** if providing "--overwrite" option, always create output file */
        if ((!overwrite_flag) && (output_file_exist_flag))
        {
            msglog(NULL, MSGLOG_ERR,
                   "Output file had been existed, please using '--overwrite' if you want to overwrite it\n\n");
            ret = EMA_MP4_MUXED_PARAM_ERR;
        }

        /** consistency check */
        if ((handle->usr_cfg_mux.output_mode & EMA_MP4_FRAG) || !handle->usr_cfg_mux.chunk_span_time)
        {
           int32_t ua;
            /** no interleave by size */
            for (ua = 0; ua < handle->usr_cfg_mux.es_num; ua++)
            {
                if (handle->usr_cfg_ess[ua].chunk_span_size)
                {
                    handle->usr_cfg_ess[ua].chunk_span_size = 0;
                }
            }

            /** no interleave by time */
            if (handle->usr_cfg_mux.output_mode & EMA_MP4_FRAG)
            {
                handle->usr_cfg_mux.chunk_span_time = 0;
                /** sp chunk op basically just take care of sample description and dref */
            }
        }
    }
    else if (ret != EMA_MP4_MUXED_EXIT)
    {
        msglog(NULL, MSGLOG_ERR, "Error parsing command line! \n");
    }

    return ret;
}


int
main(int argc, char **argv)
{
    int32_t err = EMA_MP4_MUXED_OK;
    ema_mp4_ctrl_handle_t ema_handle = NULL;

    /**** create muxer handle */
    CHECK( ema_mp4_mux_create(&ema_handle) );

    /**** argc/argv to usr_cfg info */
    CHECK( parse_cli(ema_handle, argc, (int8_t **)argv) );

    /**** go */
    CHECK( ema_mp4_mux_start(ema_handle) );

cleanup:
    /**** clean up. parser and mux already done and their resource released */
    if (ema_handle)
    {
        ema_mp4_mux_destroy(ema_handle);
    }

    return err;
}
