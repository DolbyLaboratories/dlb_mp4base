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
    @file parser_defs.h
    @brief Defines types and structures shared by parser and app
*/

#ifndef __PARSER_DEFS_H__
#define __PARSER_DEFS_H__

#include "c99_inttypes.h"   /** uint8_t, uint32_t */
#ifdef __USE_RTS__
#include "rts/bufhandle.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*! \brief enum type EDateFmt_t defines input data format: ESI or raw */
typedef enum EEsFmt_t_
{
    ESFMT_RAW = 0,    /*!< raw input data format */
    ESFMT_ESI,            /*!< elementary stream interface input data format */
    ESFMT_NUM
} EEsFmt_t;

/** the ESI parameters about Au properties */
typedef struct SEsiArg_t_
{
    uint32_t u32AuSeqNum;
    uint32_t u32AuSize;
    uint32_t u32DtsTc; 
    uint32_t u32PtsOffTc; 

    uint8_t     fRap;
    uint8_t     fStart;
    uint8_t     fEnd;
    uint8_t     fIdle;
} SEsiArg_t;


/** to support input data buffer driven parsing */
typedef struct SEsData_t_
{
    uint8_t *pBufIn;           /** ES data input buffer addr */
    uint32_t u32DataInSize;    /** data available */
    SEsiArg_t *psDataDesc;     /** data description */
#ifdef __USE_RTS__
    buf_handle_t hBuf;
#endif
} SEsData_t;

/** to store Sub-Structure(Nal, substream sync frame, etc.) info collected */
#define LAYER_IDX_MASK    0x0f /** mask for layer idx */
#define EMBEDED_FLAG      0x80 /** embeded or not */
#define LE_FLAG           0x40 /** little endian or not */
typedef struct SSs_t_
{
    uint8_t  u8FlagsLidx;/** embeded flag and layer idx      */
    uint8_t  u8ShSize;   /** nal scp size                    */
    uint8_t  u8EmbValue; /** the embeded value, u8BodyIdx, u32BodyOff can be used for the purpose too */
    uint8_t  u8BodyIdx;  /** nal body in esd addr            */

    uint32_t  u32BodyOff;  /** nal bodyin esd addr             */
    uint32_t  u32BodySize; /** nal body(i.e. exclude scp) size */
} SSs_t;
/** end of input data buffer driven parsing */

/**** Ts protocol def */
typedef enum ETsPro_t_
{
    TS_PRO_ANY   = -1,
    TS_PRO_ATSC     = 0,
    TS_PRO_DTV   = 1,
    TS_PRO_CABLE = 2,
    TS_PRO_BD    = 3,
    TS_PRO_DVB   = 4,
    TS_PRO_NUM   = 5
} ETsPro_t;

/**** follow that of stream_type in 13818-1 if available */
typedef enum stream_id_t_
{
    STREAM_ID_UNKNOWN = 0, /** ITU-T|ISO/IEC Reserved */
   
    STREAM_ID_11172_2_Video     = 0x01,
    STREAM_ID_13818_2_Video     = 0x02,
    STREAM_ID_11172_3_Audio     = 0x03,
    STREAM_ID_13818_3_Audio     = 0x04,
    /*
    STREAM_ID_13818_1_private_sections  = 0x05,
    */
    STREAM_ID_13818_1_private_data_PES  = 0x06,

    STREAM_ID_13818_7_Audio_ADTS    = 0x0f, /** STREAM_ID_13818_7_Audio in ADTS */

    STREAM_ID_14496_2_Visual        = 0x10,
    STREAM_ID_14496_3_Audio_LATM    = 0x11, /** STREAM_ID_14496_3_Audio in LATM */

    /*
    STREAM_ID_Metadata_PES              = 0x15,
    STREAM_ID_Metadata_metadata_section = 0x16,
    */

    STREAM_ID_14496_10_AVC_sub          = 0x1b, /** including annex A, AVC sub bitstream, MVC sub bitstream and MVC base view */
    STREAM_ID_14496_3_Audio             = 0x1c,
    STREAM_ID_14496_10_SVC_sub          = 0x1f,
    STREAM_ID_14496_10_MVC_sub          = 0x20,

    STREAM_ID_HEVC                      = 0x27, /** hevc; ref: ISO/IEC 13818-1:201X/PDAM 3       7) Clause 2.4.4.9 */

    STREAM_ID_14496_ATSC_AC3            = 0x81,
    STREAM_ID_14496_ATSC_EC3            = 0x87,

    STREAM_ID_GENERAL                   = 0x100,
    
    /** pcr only pid */
    STREAM_ID_PCR_ONLY,

    /** video */
    STREAM_ID_D3D,
    STREAM_ID_H263,
    STREAM_ID_H264,
    STREAM_ID_MPG2,


    STREAM_ID_VC1,
    STREAM_ID_YUV420P,
    /** audio */
    STREAM_ID_AAC,
    STREAM_ID_AC3,
    STREAM_ID_EC3,
    STREAM_ID_AC4,
    STREAM_ID_MLP,
    STREAM_ID_MP3,
    STREAM_ID_MP2,
    STREAM_ID_DTS,
    /** metadata */
    STREAM_ID_METX,
    STREAM_ID_METT,
    STREAM_ID_HINT,
    /** text */
    STREAM_ID_TX3G,
    STREAM_ID_STPP,
    /** dolby's */
    STREAM_ID_EMAJ,
    STREAM_ID_VDRM,  /** VDR meta date */

    /** quicktime */
    STREAM_ID_QTVIDEO,
    STREAM_ID_QTAUDIO,
    /** microsoft */
    STREAM_ID_MSAUDIO,
    /** encryption */
    STREAM_ID_ENCRYPTED_VIDEO,
    STREAM_ID_ENCRYPTED_AUDIO,
    /** null */
    STREAM_ID_NULL  /** for null dsi  only */
} stream_id_t;

#define STREAM_ID_MP4V  STREAM_ID_14496_2_Visual

/** to support various get op */
typedef enum stream_param_id_t_
{
    /****** general */
    STREAM_PARAM_ID_MIN_BITRATE,
    STREAM_PARAM_ID_AVG_BITRATE,
    STREAM_PARAM_ID_MAX_BITRATE,

    STREAM_PARAM_ID_TIME_SCALE, /** the time ticks in one second */
    STREAM_PARAM_ID_NUM_UNITS_IN_TICK,
    STREAM_PARAM_ID_FRAME_DUR,  /** STREAM_PARAM_ID_TIME_SCALE/STREAM_PARAM_ID_FRAME_DUR = frame rate */
    STREAM_PARAM_ID_MIN_CTS,    /** min cts */
    STREAM_PARAM_ID_DLT_DTS_TC, /** delta dts in tc */

    /****** video */
    STREAM_PARAM_ID_PROFILE,
    STREAM_PARAM_ID_LEVEL,
    STREAM_PARAM_ID_PROFILE_ENH,
    STREAM_PARAM_ID_LEVEL_ENH,

    STREAM_PARAM_ID_MAX_FRAME_WIDTH,
    STREAM_PARAM_ID_MAX_FRAME_HEIGHT,

    STREAM_PARAM_ID_HRD0_BITRATE,       /** obsolete */
    STREAM_PARAM_ID_HRD_BITRATE,        
    STREAM_PARAM_ID_HRD_CPB_SIZE,       /** in byte */

    STREAM_PARAM_ID_HRD_BITRATE_ENH,        
    STREAM_PARAM_ID_HRD_CPB_SIZE_ENH,   /** in byte */
    /**** AVC specific */
    STREAM_PARAM_ID_CPB_CNT,            /** the base layer */
    STREAM_PARAM_ID_CPB_CNT_ENH,        /** the enhanced layer */
    /** ASF specific */
    STREAM_PARAM_ID_BUFFER_WINDOW,

    /** MP2TS specific */
    STREAM_PARAM_ID_RX,          /** for last one of HRD */
    STREAM_PARAM_ID_B_SIZE,      /** ..., in byte */
    STREAM_PARAM_ID_DEC_DELAY,   /** ...: initial_cpb_removal_delay_lst */

    STREAM_PARAM_ID_RX_ENH,      /** for last one of HRD: enhanced layer */
    STREAM_PARAM_ID_B_SIZE_ENH,  /** ..., in bits */

    STREAM_PARAM_ID_ASPECT_RATIO,/** = MSB16:LSB16 */
    STREAM_PARAM_ID_PROGRESSIVE, /** yes if != 0 */

    /** MLP specific */
    STREAM_PARAM_ID_TIME_OFFSET,

    /** AAC specific, may be generalized and used in audio */
    STREAM_PARAM_ID_CHANNELCOUNT,

    STREAM_PARAM_ID_NUM
} stream_param_id_t;


#ifdef __cplusplus
};
#endif

#endif /* !__PARSER_DEFS_H__ */
