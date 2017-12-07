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
    @file parser_dd.h
    @brief Defines the structures for AC-3 and E-AC-3 parser 
*/

#ifndef __PARSER_DD_H
#define __PARSER_DD_H

#include "parser.h"   /** PARSER_AUDIO_BASE */
#include "msg_log.h"  /** ENABLE_MP4_MSGLOG */

#ifndef KEEP_LE_DD
#define KEEP_LE_DD    1 /** 1: Keep a little endian bit stream as LE
                            0: Convert LE to BE
                            LE is not supported by VLC */
#endif

#ifndef KEEP_LE_DD_TS
#define KEEP_LE_DD_TS 1 /** for ts case only */
#endif

/** EC3 dependent substream customer chanmap b0...b15 definition 
*  defined in the way to make mapping to chan_loc easy 
*  however, CHANMAP_L corresponding to the first bit in bitstream */
enum chanmap_t_
{
    CHANMAP_L       = 0x01,
    CHANMAP_C       = 0x02,
    CHANMAP_R       = 0x04,
    CHANMAP_Ls      = 0x08,
    CHANMAP_Rs      = 0x10,
    CHANMAP_LcRc    = 0x20,
    CHANMAP_LrsRrs  = 0x40,
    CHANMAP_Cs      = 0x80,
    CHANMAP_Ts      = 0x100,
    CHANMAP_LsdRsd  = 0x200,
    CHANMAP_LwRw    = 0x400,
    CHANMAP_LvhRvh  = 0x800,
    CHANMAP_Cvh     = 0x1000,
    CHANMAP_reserved= 0x2000,
    CHANMAP_LFE2    = 0x4000,
    CHANMAP_LFE     = 0x8000
};

/**  EC3 chanmap b0...b15 to channel num */
static const uint8_t channel_num_tbl[] = {
    1, 1, 1, 1, 
    1, 2, 2, 1, 
    1, 2, 2, 2,
    1, 0, 1, 1
};

#ifdef ENABLE_MP4_MSGLOG
/**  EC3 chanmap b0...b15 to channel description */
static const char* channel_desc_tbl[] = {
    "L",    "C",        "R",        "Ls",  
    "Rs",   "Lc/Rc",    "Lrs/Rrs",  "Cs", 
    "Ts",   "Lsd/Rsd",  "Lw/Rw",    "Lvh/Rvh",  
    "Cvh",  "reserved", "LFE2",     "LFE"
};
#endif

/** info acmod carries */
struct acmode_t_
{
    const int8_t *audio_coding_mode;      /** audio coding mode */
    uint32_t    nfchans;                  /** number of channels */
    const int8_t *channel_array_ordering; /** channel ordering */
    uint32_t    channel_flags;            /** channels bit field */
};
typedef struct acmode_t_ acmode_t;

/** acmod_tbl[acmod] => info */
static const acmode_t acmod_tbl[] = {
    { "1+1", 2, "Ch1+Ch2",      CHANMAP_L | CHANMAP_R },
    { "1/0", 1, "C",            CHANMAP_C },
    { "2/0", 2, "L R",          CHANMAP_L | CHANMAP_R },
    { "3/0", 3, "L C R",        CHANMAP_L | CHANMAP_R | CHANMAP_C },
    { "2/1", 3, "L R S",        CHANMAP_L | CHANMAP_R | CHANMAP_LrsRrs },
    { "3/1", 4, "L C R S",      CHANMAP_L | CHANMAP_R | CHANMAP_C | CHANMAP_LrsRrs },
    { "2/2", 4, "L R Ls Rs",    CHANMAP_L | CHANMAP_R | CHANMAP_Ls | CHANMAP_Rs },
    { "3/2", 5, "L C R Ls Rs",  CHANMAP_L | CHANMAP_R | CHANMAP_C | CHANMAP_Ls | CHANMAP_Rs }
};

#ifdef ENABLE_MP4_MSGLOG
/** bsmod_tbl[bsmod] => info */
static const int8_t *bsmod_tbl[] = {
    "main audio service: complete main(CM)",
    "main audio service: music and effects(ME)",
    "associated service: visually impaired(VI)",
    "associated service: hearing impaired(HI)",
    "associated service: dialogue(D)",
    "associated service: commentary(C)",
    "associated service: emergency(E)",
    "associated service: voice over(VO)",
    "main audio service: karaoke(K)"
};
#endif

/** ac3_bitrate_tbl[frmsizecod] => bitrate kbps */
static const uint32_t ac3_bitrate_tbl[] = {
    32,  32,  40,  40,  48,  48,   56,  56, 64,   64,  80,  80, 96,  96,  112, 112, 
    128, 128, 160, 160, 192, 192, 224, 224, 256, 256, 320, 320, 384, 384, 448, 448,
    512, 512, 576, 576, 640, 640
};

/** fscod_2_freq_tbl[fscod] => sample rate  */
static const uint32_t fscod_2_freq_tbl[] = { 48000, 44100, 32000 };
/** fscod2_2_freq_tbl[fscod2] => sample rate  */
static const uint32_t fscod2_2_freq_tbl[] = { 24000, 22050, 16000 };

/** ac3_frame_size_tbl[frmsizecod][fscod] => fraame size */
static const uint32_t ac3_frame_size_tbl[][3] = {
    {  64,   69,   96},
    {  64,   70,   96},
    {  80,   87,  120},
    {  80,   88,  120},
    {  96,  104,  144},
    {  96,  105,  144},
    { 112,  121,  168},
    { 112,  122,  168},
    { 128,  139,  192},
    { 128,  140,  192},
    { 160,  174,  240},
    { 160,  175,  240},
    { 192,  208,  288},
    { 192,  209,  288},
    { 224,  243,  336},
    { 224,  244,  336},
    { 256,  278,  384},
    { 256,  279,  384},
    { 320,  348,  480},
    { 320,  349,  480},
    { 384,  417,  576},
    { 384,  418,  576},
    { 448,  487,  672},
    { 448,  488,  672},
    { 512,  557,  768},
    { 512,  558,  768},
    { 640,  696,  960},
    { 640,  697,  960},
    { 768,  835, 1152},
    { 768,  836, 1152},
    { 896,  975, 1344},
    { 896,  976, 1344},
    {1024, 1114, 1536},
    {1024, 1115, 1536},
    {1152, 1253, 1728},
    {1152, 1254, 1728},
    {1280, 1393, 1920},
    {1280, 1394, 1920},
};

/** frmsizecod max + 1 == FRMSIZECOD_TOP */
static const uint32_t FRMSIZECOD_TOP = sizeof(ac3_bitrate_tbl)/sizeof(uint32_t);

/** numblks_tbl[numblkscod] => blocks per frame */
static const uint32_t numblks_tbl[] = { 1, 2, 3, 6 };

#define SAMPLES_PER_BLOCK 256
static const uint32_t AC3_SAMPLES_PER_FRAME = (6*SAMPLES_PER_BLOCK);   /** 6 audio blocks */

#ifdef ENABLE_MP4_MSGLOG
/**  EC3 chan_loc b0...b8 description */
static const int8_t *mp4_chan_loc_tbl[] = {
    "Lc/Rc",
    "Lrs/Rrs",
    "Cs",
    "Ts",
    "Lsd/Rsd",
    "Lw/Rw",
    "Lvh/Rvh",
    "Cvh",
    "LFE2"
};
#endif

enum dd_type_t_
{
    DD_TYPE_NONE,   /** substream not exist */
    DD_TYPE_AC3,
    DD_TYPE_EC3
};
typedef enum dd_type_t_  dd_type_t;

enum ec3_strmtype_t_
{
    EC3_STRMTYPE_0 = 0x00,
    EC3_STRMTYPE_1,
    EC3_STRMTYPE_2,
    EC3_STRMTYPE_3
};

#define EC3_MAX_STREAMS     8
#define EC3_MAX_SUBSTREAMS  8
#define AC3_SUBSTREAMID     0

struct dd_substream_t_
{
    dd_type_t ddt;           /** ind_sub may be AC3 */
    uint32_t  data_rate;     /** in kbps */

    uint8_t strmtyp;
    uint8_t fscod;
    uint8_t bsid;
    uint8_t bsmod;
    uint8_t acmod;
    uint8_t lfeon;

    /** AC-3 only */
    uint8_t bit_rate_code;
    /** EC-3 with dependent substream only */
    uint16_t chan_loc;       /** mp4 channel location */

    uint16_t channel_flags;  /** for info output */
    uint8_t  dsurmod;        /** for ts descriptor only */
    uint8_t  mixmdate;       /** for ts descriptor only */

    uint8_t addbsie;
    uint8_t addbsil;
    uint8_t addbsi[64];
};
typedef struct dd_substream_t_  dd_substream_t;

typedef struct ec3_sp_t_  ec3_sp_t;

struct parser_dd_t_
{
    PARSER_AUDIO_BASE;

    /** AC3 or EC3 */
    dd_type_t ddt;

    /** per sub stream info enough to build dsi info, bit_rate in parser_t in kbps */
    uint32_t       num_ind_sub;
    dd_substream_t subs_ind[EC3_MAX_STREAMS];                  /** the independent substream. AC3 should only use [0] */
    dd_substream_t subs[EC3_MAX_STREAMS][EC3_MAX_SUBSTREAMS];  /** the dependent */

    /** parsing vars: aud_frame, mp4_sample */
    uint32_t mp4_sample_num, dd_frame_num;  /** mp4 sample # audio frame # got so far */
    uint32_t aud_sample_num;                /** audio sample(not frame) collected so for a mp4 sample */
    uint8_t  numblks;                       /** AC-3 always 6 */
    uint64_t dts;

    /** to track substream parsing: */
    int32_t last_indep, last_dep;   /** last_indep > -1, the independent substream last seen
                                      *            = -1 end of dd_frame_num
                                      * last_dep defined if last_indep > -1.
                                      * last_dep = -1 current frame is independent
                                      *          > -1 the dependent substream last seen */
    /** mp4 sample buf: its is shared across all mp4 streams if it is
    * use out side of parser context */
    BOOL     sample_buf_alloc_only;           
    uint8_t *sample_buf;                    
    uint32_t sample_buf_size, sample_size, sample_pre_read_size; 

    /** info per program */
    uint32_t nfchans_prg[EC3_MAX_STREAMS];          /** channel num */
    uint16_t channel_flags_prg[EC3_MAX_STREAMS];    /** channel bit map */

    BOOL isLE;  /** current dd frame is Little Endian */

    /** can with pull mode parser, but just to make it more indepenent */
#define SF_BUF_SIZE  32
    uint8_t      sf_buf[SF_BUF_SIZE];  /** since ec3 shall need less than 32 bytes, 8 - 2 byte is enough for ac3 */
    int32_t          sf_pre_buf_num;

    int32_t          sf_bufed_num;
    uint32_t     sf_data_got;
    BOOL         is1536AudSmplRdy;
    mp4_sample_t sample_got;     /** no memory alloc related so destroy does not matter */
#if !KEEP_LE_DD_TS
    uint8_t *    pu8Swap0;  /** to even byte to be swapped */
#endif
};
typedef struct parser_dd_t_ parser_dd_t;
typedef parser_dd_t *parser_dd_handle_t;

#endif /* __PARSER_DD_H */
