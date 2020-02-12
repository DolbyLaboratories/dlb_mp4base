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
    @file parser.h
    @brief Defines basic functions for all parser supported

*/

#ifndef __PARSER_H__
#define __PARSER_H__

#include "c99_inttypes.h"  /** uint32_t      */
#include "io_base.h"       /** bbio_handle_t */
#include "dsi.h"           /** dsi_handle_t  */
#include "parser_defs.h"   /** SEsData_t     */
#include "return_codes.h"  /** return codes  */

typedef int64_t offset_t;

#ifdef __cplusplus
extern "C"
{
#endif

#define PARSE_DURATION_TEST      0 /** !0: test only PARSE_DURATION_TEST second es */

#define IS_FOURCC_EQUAL(a, b) ((uint8_t)((a)[0]) == (uint8_t)((b)[0]) && \
                               (uint8_t)((a)[1]) == (uint8_t)((b)[1]) && \
                               (uint8_t)((a)[2]) == (uint8_t)((b)[2]) && \
                               (uint8_t)((a)[3]) == (uint8_t)((b)[3]))
#define FOURCC_ASSIGN(a, b) \
do {                        \
    (a)[0] = (b)[0];        \
    (a)[1] = (b)[1];        \
    (a)[2] = (b)[2];        \
    (a)[3] = (b)[3];        \
} while (0)

/** Profile and level indication of H.264/AVC */
#define ADVANCED_VIDEO_CODING               0x7f

/** profile level value is 4 bytes into the parser codec_config */
#define MP4V_PROFILE_LEVEL_INDEX            4

/** These are the profile and level values for MPEG4 */
#define SIMPLE_PROFILE_LEVEL_1              0x01
#define SIMPLE_PROFILE_LEVEL_2              0x02
#define SIMPLE_PROFILE_LEVEL_3              0x03
#define SIMPLE_PROFILE_LEVEL_0              0x08

#define SIMPLE_SCALABLE_PROFILE_LEVEL_0     0x10
#define SIMPLE_SCALABLE_PROFILE_LEVEL_1     0x11
#define SIMPLE_SCALABLE_PROFILE_LEVEL_2     0x12

#define CORE_PROFILE_LEVEL_1                0x21
#define CORE_PROFILE_LEVEL_2                0x22

#define MAIN_PROFILE_LEVEL_2                0x32
#define MAIN_PROFILE_LEVEL_3                0x33
#define MAIN_PROFILE_LEVEL_4                0x34

#define N_BIT_PROFILE_LEVEL_2               0x42

#define SCALABLE_TEXTURE_PROFILE_LEVEL_1    0x51

#define SIMPLE_FACE_ANIM_LEVEL_1            0x61
#define SIMPLE_FACE_ANIM_LEVEL_2            0x62

#define SIMPLE_FBA_PROFILE_LEVEL_1          0x63
#define SIMPLE_FBA_PROFILE_LEVEL_2          0x64

#define BASIC_ANIM_TEXT_PROFILE_LEVEL_1     0x71
#define BASIC_ANIM_TEXT_PROFILE_LEVEL_2     0x72

#define H264AVC_PROFILE                     0x7F

#define HYBRID_PROFILE_LEVEL_1              0x81
#define HYBRID_PROFILE_LEVEL_2              0x82

#define ADVANCED_REAL_TIME_SIMPLE_PROFILE_LEVEL_1   0x91
#define ADVANCED_REAL_TIME_SIMPLE_PROFILE_LEVEL_2   0x92
#define ADVANCED_REAL_TIME_SIMPLE_PROFILE_LEVEL_3   0x93
#define ADVANCED_REAL_TIME_SIMPLE_PROFILE_LEVEL_4   0x94

#define CORE_SCALABLE_PROFILE_LEVEL_1       0xA1
#define CORE_SCALABLE_PROFILE_LEVEL_2       0xA2
#define CORE_SCALABLE_PROFILE_LEVEL_3       0xA3

#define ADVANCED_CODING_EFF_PROFILE_LEVEL_1 0xB1
#define ADVANCED_CODING_EFF_PROFILE_LEVEL_2 0xB2
#define ADVANCED_CODING_EFF_PROFILE_LEVEL_3 0xB3
#define ADVANCED_CODING_EFF_PROFILE_LEVEL_4 0xB4

#define ADVANCED_CORE_PROFILE_LEVEL_1       0xC1
#define ADVANCED_CORE_PROFILE_LEVEL_2       0xC2

#define ADVANCED_SCALABLE_TEXT_LEVEL_1      0xD1
#define ADVANCED_SCALABLE_TEXT_LEVEL_2      0xD2
#define ADVANCED_SCALABLE_TEXT_LEVEL_3      0xD3

#define SIMPLE_STUDIO_PROFILE_LEVEL_1       0xE1
#define SIMPLE_STUDIO_PROFILE_LEVEL_2       0xE2
#define SIMPLE_STUDIO_PROFILE_LEVEL_3       0xE3
#define SIMPLE_STUDIO_PROFILE_LEVEL_4       0xE4

#define CORE_STUDIO_PROFILE_LEVEL_1         0xE5
#define CORE_STUDIO_PROFILE_LEVEL_2         0xE6
#define CORE_STUDIO_PROFILE_LEVEL_3         0xE7
#define CORE_STUDIO_PROFILE_LEVEL_4         0xE8

#define ADVANCED_SIMPLE_PROFILE_LEVEL_0     0xF0
#define ADVANCED_SIMPLE_PROFILE_LEVEL_1     0xF1
#define ADVANCED_SIMPLE_PROFILE_LEVEL_2     0xF2
#define ADVANCED_SIMPLE_PROFILE_LEVEL_3     0xF3
#define ADVANCED_SIMPLE_PROFILE_LEVEL_4     0xF4
#define ADVANCED_SIMPLE_PROFILE_LEVEL_5     0xF5
#define ADVANCED_SIMPLE_PROFILE_LEVEL_3B    0xF7

#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_0   0xF8
#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_1   0xF9
#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_2   0xFA
#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_3   0xFB
#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_4   0xFC
#define FINE_GRANULARITY_SCAL_PROFILE_LEVEL_5   0xFD
/** End of the profile and level values for MPEG4 */

/** Start of the profile and level values for Audio */
#define MAIN_AUDIO_PROFILE_LEVEL_1          0x01
#define MAIN_AUDIO_PROFILE_LEVEL_2          0x02
#define MAIN_AUDIO_PROFILE_LEVEL_3          0x03
#define MAIN_AUDIO_PROFILE_LEVEL_4          0x04

#define SCALABLE_AUDIO_PROFILE_LEVEL_1      0x05
#define SCALABLE_AUDIO_PROFILE_LEVEL_2      0x06
#define SCALABLE_AUDIO_PROFILE_LEVEL_3      0x07
#define SCALABLE_AUDIO_PROFILE_LEVEL_4      0x08

#define SPEECH_AUDIO_PROFILE_LEVEL_1        0x09
#define SPEECH_AUDIO_PROFILE_LEVEL_2        0x0A

#define SYNTHETIC_AUDIO_PROFILE_LEVEL_1     0x0B
#define SYNTHETIC_AUDIO_PROFILE_LEVEL_2     0x0C
#define SYNTHETIC_AUDIO_PROFILE_LEVEL_3     0x0D

#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_1  0x0E
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_2  0x0F
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_3  0x10
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_4  0x11
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_5  0x12
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_6  0x13
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_7  0x14
#define HIGH_QUALITY_AUDIO_PROFILE_LEVEL_8  0x15

#define LOW_DELAY_AUDIO_PROFILE_LEVEL_1     0x16
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_2     0x17
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_3     0x18
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_4     0x19
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_5     0x1A
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_6     0x1B
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_7     0x1C
#define LOW_DELAY_AUDIO_PROFILE_LEVEL_8     0x1D

#define NATURAL_AUDIO_PROFILE_LEVEL_1       0x1E
#define NATURAL_AUDIO_PROFILE_LEVEL_2       0x1F
#define NATURAL_AUDIO_PROFILE_LEVEL_3       0x20
#define NATURAL_AUDIO_PROFILE_LEVEL_4       0x21

#define MA_INTERNETWORKING_PROFILE_LEVEL_1  0x22
#define MA_INTERNETWORKING_PROFILE_LEVEL_2  0x23
#define MA_INTERNETWORKING_PROFILE_LEVEL_3  0x24
#define MA_INTERNETWORKING_PROFILE_LEVEL_4  0x25
#define MA_INTERNETWORKING_PROFILE_LEVEL_5  0x26
#define MA_INTERNETWORKING_PROFILE_LEVEL_6  0x27

#define AAC_PROFILE_LEVEL_1                 0x28
#define AAC_PROFILE_LEVEL_2                 0x29
#define AAC_PROFILE_LEVEL_4                 0x2A
#define AAC_PROFILE_LEVEL_5                 0x2B

#define HEAAC_PROFILE_LEVEL_2               0x2C
#define HEAAC_PROFILE_LEVEL_3               0x2D
#define HEAAC_PROFILE_LEVEL_4               0x2E
#define HEAAC_PROFILE_LEVEL_5               0x2F

#define HEAACV2_PROFILE_LEVEL_2             0x30
#define HEAACV2_PROFILE_LEVEL_3             0x31
#define HEAACV2_PROFILE_LEVEL_4             0x32
#define HEAACV2_PROFILE_LEVEL_5             0x33

/** End of the profile and level values for Audio */

/** AAC Max Bitrate 1 sec window length correction factors */
#define AAC_1_SEC_WINDOW_DENOM  1024
#define AAC_1_SEC_WINDOW_16000  1000
#define AAC_1_SEC_WINDOW_22050  1002
#define AAC_1_SEC_WINDOW_24000  1043
#define AAC_1_SEC_WINDOW_32000  1032
#define AAC_1_SEC_WINDOW_44100  1026
#define AAC_1_SEC_WINDOW_48000  1021

/** Audio object types */
#define AOT_AAC_MAIN                    1
#define AOT_AAC_LC                      2
#define AOT_AAC_SSR                     3
#define AOT_AAC_LTP                     4
#define AOT_SBR                         5
#define AOT_AAC_SCALABLE                6
#define AOT_TWINVQ                      7
#define AOT_CELP                        8
#define AOT_HVXC                        9
#define AOT_TTSI                        12
#define AOT_MAIN_SYNTHETIC              13
#define AOT_WAVETABLE_SYNTHESIS         14
#define AOT_GENERAL_MIDI                15
#define AOT_ALGORITHMIC_SYNTH_AUDIO_FX  16
#define AOT_ER_AAC_LC                   17
#define AOT_ER_AAC_LTP                  19
#define AOT_ER_AAC_SCALABLE             20
#define AOT_ER_TWINVQ                   21
#define AOT_ER_BSAC                     22
#define AOT_ER_AAC_LD                   23
#define AOT_ER_CELP                     24
#define AOT_ER_HVXC                     25
#define AOT_ER_HILN                     26
#define AOT_ER_PARAMETRIC               27
#define AOT_SSC                         28
#define AOT_PS                          29
#define AOT_RESERVED_2                  30
#define AOT_ESCAPE                      31
#define AOT_LAYER_1                     32
#define AOT_LAYER_2                     33
#define AOT_LAYER_3                     34
#define AOT_DST                         35

/** objectTypeIndication values Table 5 ISO/IEC 14496-01 - 2004 */
#define MP4_OT_FORBIDDEN                                0x00
#define MP4_OT_SYSTEMS_A                                0x01
#define MP4_OT_SYSTEMS_B                                0x02
#define MP4_OT_INTERACTION_STREAM                       0x03
#define MP4_OT_SYSTEMS_C                                0x04
#define MP4_OT_SYSTEMS_D                                0x05
#define MP4_OT_FONT_DATA_STREAM                         0x06
#define MP4_OT_SYNTHESIZED_TEXTURE_STREAM               0x07
#define MP4_OT_STREAMING_TEXT_STREAM                    0x08
#define MP4_OT_VISUAL_14492_2                           0x20
#define MP4_OT_VISUAL_H264                              0x21
#define MP4_OT_PARAMETER_SETS_H264                      0x22
#define MP4_OT_AUDIO_14496_3                            0x40
#define MP4_OT_VISUAL_13818_2_SIMPLE_PROFILE            0x60
#define MP4_OT_VISUAL_13818_2_MAIN_PROFILE              0x61
#define MP4_OT_VISUAL_13818_2_SNR_PROFILE               0x62
#define MP4_OT_VISUAL_13818_2_SPATIAL_PROFILE           0x63
#define MP4_OT_VISUAL_13818_2_HIGH_PROFILE              0x64
#define MP4_OT_VISUAL_13818_2_422_PROFILE               0x65
#define MP4_OT_AUDIO_13818_7_MAIN_PROFILE               0x66
#define MP4_OT_AUDIO_13818_7_LOW_COMPLEXITY             0x67
#define MP4_OT_AUDIO_13818_7_SCALEABLE_SAMPLING_RATE    0x68
#define MP4_OT_AUDIO_13818_3                            0x69
#define MP4_OT_VISUAL_11172_2                           0x6A
#define MP4_OT_AUDIO_11172_3                            0x6B
#define MP4_OT_VISUAL_10918_1                           0x6C
#define MP4_OT_NO_TYPE_DEFINED                          0xFF

/** Sampling Frequency Index */
#define SFI_96000           0x0
#define SFI_88200           0x1
#define SFI_64000           0x2
#define SFI_48000           0x3
#define SFI_44100           0x4
#define SFI_32000           0x5
#define SFI_24000           0x6
#define SFI_22050           0x7
#define SFI_16000           0x8
#define SFI_12000           0x9
#define SFI_11025           0xA
#define SFI_8000            0xB
#define SFI_7350            0xC
#define SFI_RESERVED_1      0xD
#define SFI_RESERVED_2      0xE
#define SFI_ESCAPE          0xF

#define QTAUDIO_FLAG_ISFLOAT        0x01
#define QTAUDIO_FLAG_ISBIGENDIAN    0x02
#define QTAUDIO_FLAG_ISSIGNEDINT    0x04
#define QTAUDIO_FLAG_ISPACKED       0x08

#define USER_DEFINED_PROFILE                0xFE
#define UNKNOWN_PROFILE                     0xFF

/** descriptor tag we handled so far */
typedef enum DescrTag_t_
{
    ES_DescrTag             = 0x03,
    DecoderConfigDescrTag   = 0x04,
    DecSpecificInfoTag      = 0x05,
    SLConfigDescrTag        = 0x06
} DescrTag_t;

#define MEDIA_SAMPLE_BASE                                                               \
    /** timing in parser time_base */                                                   \
    uint64_t dts;                                                                       \
    uint64_t cts;                                                                       \
    uint32_t duration;                                                                  \
    /** data in buf and its offset in file */                                           \
    size_t   size;                                                                      \
    offset_t pos;                                                                       \
    uint8_t *data; /** in muxer only */                                                 \
    uint32_t flags;                                                                     \
    uint32_t sd_index;                                                                  \
    /** Sample dependencies. See 'sdtp' box. */                                         \
    uint8_t  is_leading;                                                                \
    uint8_t  sample_depends_on;                                                         \
    uint8_t  sample_is_depended_on;                                                     \
    uint8_t  sample_has_redundancy;                                                     \
    uint8_t  pic_type;                                                                  \
    uint8_t  frame_type;                                                                \
    uint8_t  dependency_level;                                                          \
    /** auxiliary data used in demuxer for decryption */                                \
    uint8_t  aux_data[256];                                                             \
    uint32_t aux_data_type;                                                             \
    uint8_t  aux_data_size;                                                             \
    /** Subsample information. */                                                       \
    uint32_t *subsample_sizes;                                                          \
    uint32_t num_subsamples;                                                            \


typedef struct mp4_sample_t_
{
    MEDIA_SAMPLE_BASE

    #define SAMPLE_SYNC             0x0001  /** sync frame */
    #define SAMPLE_PARTIAL          0x0010  /** partial sample: only data and size is defined(no timing) */
    #define SAMPLE_PARTIAL_AU       0x0020  /** partial sample: AU data not complete */
    #define SAMPLE_PARTIAL_TM       0x0040  /** partial sample: AU timeing not complete */
    #define SAMPLE_PARTIAL_SS       0x0080  /** partial sub-structure: sub-structure data not complete */

    #define SAMPLE_NEW_SD           0x0100  /** new sample desription */

    uint8_t nal_info;                       /** h.264 only */
    void (*destroy)(struct mp4_sample_t_ *);
} mp4_sample_t, avi_sample_t;
typedef mp4_sample_t  *mp4_sample_handle_t;
typedef avi_sample_t  *avi_sample_handle_t;

typedef enum stream_type_t_
{
    STREAM_TYPE_UNKNOWN = 0,

    STREAM_TYPE_VIDEO,

    STREAM_TYPE_AUDIO,

    STREAM_TYPE_DATA,

    STREAM_TYPE_META,

    STREAM_TYPE_TEXT,

    STREAM_TYPE_SUBTITLE,

    STREAM_TYPE_ODSM,

    STREAM_TYPE_HINT,

    STREAM_TYPE_SYSTEM
} stream_type_t;

/**
 * general parser definition for mp4 iso file format
 */
/**** the parser interface and common data member */
struct parser_t_;
typedef struct parser_t_ parser_t;
typedef parser_t *parser_handle_t;

/** report levels */
#define REPORT_LEVEL_INFO  0
#define REPORT_LEVEL_WARN  1

/** used to report messages and warnings to higher layers in the application */
typedef struct parser_reporter_t_
{
    void (*report) (struct parser_reporter_t_ *self, int32_t level, const int8_t* msg);
    void (*destroy) (struct parser_reporter_t_ *self);
    void *data;
} parser_reporter_t;


/** external timing info: default value and if to overide avc or vc1 embededed timing */
typedef struct ext_timing_info_t_
{
    unsigned override_timing;    /** if over ride timing info in sps */
    /** externally time_scale/num_units_in_tick define frame rate in overiding */
    uint32_t time_scale, num_units_in_tick;
    uint8_t         ext_dv_profile;           /* dolby vision profile, overriding set by user */
    uint8_t         ext_dv_bl_compatible_id;  /* dolby vision profile, must provided by user if profile ID is 8 */
    uint8_t         ps_present_flag;          /* the indicator of dsi info (H264: SPS/PPS; H265: VPS/SPS/PPS) in sample entry box */
    uint32_t        ac4_bitrate;
    uint32_t        ac4_bitrate_precision;
    uint32_t        hls_flag;
} ext_timing_info_t;

#ifdef WANT_GET_SAMPLE_PUSH
#define GET_SAMPLE_PUSH int32_t (*get_sample_push)(parser_handle_t parser, SEsData_t *asEsd, SSs_t *psSs, mp4_sample_handle_t sample);
#else
#define GET_SAMPLE_PUSH
#endif

typedef struct codec_config_t_
{
    size_t codec_config_size;
    void * codec_config_data; 
} codec_config_t;
  

#define PARSER_BASE                                                                                                         \
    uint32_t        stream_type;      /** defined as STREAM_TYPE_...: AUDIO, VIDEO etc. */                                  \
    uint32_t        stream_id;        /** defined as STREAM_ID_... same as 13818 stream_type if possible */                 \
    const int8_t *    stream_name;    /** for hook up dsi, debug display, file name extension in demux */                   \
    int8_t            codec_name[32]; /** name for codec used, null terminated */                                           \
    /**** dsi properties (terms decoder specific info (dsi) and sample description (sd) are                                 \
          used for one and the same thing in most parts of the code) */                                                     \
    uint32_t        dsi_type;         /** parser flavor depends on dsi type. MP4FF, ASF, MP2TS */                           \
    int8_t *          dsi_FourCC;     /** mp4ff specific: FourCC type for dsi */                                            \
    int8_t            dsi_name[5];    /** the name of sample entry, such as: hev1, hvc1, avc1....    */                     \
    list_handle_t   dsi_lst;          /** handle to dsi list */                                                             \
    dsi_handle_t    curr_dsi;         /** handle to current dsi in dsi_lst */                                               \
    uint32_t        dsi_curr_index;   /** current dsi index in case of multiple stsd */                                     \
    uint32_t        sd;               /** sample description storing method; 1: allow multiple stsd entries */              \
    uint32_t        sd_collision_flag;/** indicating new sample description necessary but not allowed */                    \
    /** dolby vision only */                                                                                                \
    uint32_t        dv_el_nal_flag;   /** indicating the stream has el nal */                                               \
    uint32_t        dv_rpu_nal_flag;  /** indicating the stream has rpu nal */                                              \
    uint32_t        dv_el_track_flag;                                                                                       \
    uint32_t        dv_bl_non_comp_flag;  /** indicating input bl is non backward compatible  */                            \
    uint32_t        dv_dsi_size;          /** dolby vision dsi buff size */                                                 \
    uint8_t         dv_dsi_buf[24];       /** dolby vision dsi buff */                                                      \
    uint32_t        dv_el_dsi_size;       /** dolby vision el dsi buff size */                                              \
    uint8_t *       dv_el_dsi_buf;        /** dolby vision el dsi buff */                                                   \
    uint8_t         dv_level;             /** dolby vision level */                                                         \
    uint32_t        ac4_bitstream_version; /* ac-4 bitstream version */                                 \
    uint32_t        ac4_presentation_version; /* ac-4 main presentation version */                      \
    uint32_t        ac4_mdcompat; /* ac-4  decoder compatibility indication */                          \
    /** decoder only */                                                                                                     \
    list_handle_t   codec_config_lst;     /** handle to codec config list */                                                \
    codec_config_t* curr_codec_config;    /** handle to current codec config in codec_config_lst */                         \
    /**** stream data source: file, in a buf or streaming */                                                                \
    bbio_handle_t   ds;                                                                                                     \
    /**** stream property */                                                                                                \
    uint8_t           profile_levelID;                                                                                      \
    uint32_t          num_units_in_tick;                                                                                    \
    uint32_t          time_scale;         /** time_scale for parser world, timescale in mp4 world */                        \
    ext_timing_info_t ext_timing;         /** external timing info  */                                                      \
    /** mp4, dd only */                                                                                                     \
    uint32_t bit_rate;                                                                                                      \
    uint32_t buferSizeDB;                                                                                                   \
    uint32_t minBitrate, maxBitrate;                                                                                        \
    uint32_t isJoC; /* 1: if it's ddp joc */                                                            \
    uint32_t isReferencedEs;                                                                            \
                                                                                                                            \
    /**** for the convenience */                                                                                            \
    uint32_t frame_size;                                                                                                    \
    /** from stts */                                                                                                        \
    uint32_t num_samples;                                                                                                   \
    /**** cross reference */                                                                                                \
    uint32_t es_idx;            /** the es index this parser correponding to */                                             \
                                                                                                                            \
    /**** parser base method */                                                                                             \
    dsi_handle_t (*dsi_create)(uint32_t dsi_type); /** the companion dsi creator */                                         \
                                                                                                                            \
    int32_t    (*init)           (parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx, bbio_handle_t ds);\
    void   (*destroy)        (parser_handle_t parser);                                                                      \
    int32_t    (*get_sample)     (parser_handle_t parser, mp4_sample_handle_t sample);                                      \
    GET_SAMPLE_PUSH                                                                                                         \
    size_t (*get_cfg_len)    (parser_handle_t parser);                                                                      \
    int32_t    (*get_cfg)        (parser_handle_t parser, uint8_t **buf, size_t *buf_size);                                 \
    size_t (*get_cfg_len_ex) (parser_handle_t parser, int32_t layer_idx);                                                   \
    int32_t    (*get_cfg_ex)     (parser_handle_t parser, uint8_t **buf, size_t *buf_size, int32_t layer_idx);              \
    /**** uint32_t get, set method: get use a abnormal return val for error. set ret 0 for OK */                            \
    uint32_t (*get_param)(parser_handle_t parser, stream_param_id_t param_id);                                              \
    int32_t      (*set_param)(parser_handle_t parser, stream_param_id_t param_id, uint32_t param);                          \
    /**** general get, set method: for vector params, param_idx = -1 to get all. ret 0 for OK */                            \
    int32_t (*get_param_ex)(parser_handle_t parser, stream_param_id_t param_id, int32_t param_idx, void *param);            \
    int32_t (*set_param_ex)(parser_handle_t parser, stream_param_id_t param_id, int32_t param_idx, void *param);            \
                                                                                                                            \
    void (*show_info)(parser_handle_t parser);                                                                              \
    /**** avc only */                                                                                                       \
    int32_t     (*copy_sample)   (parser_handle_t parser, bbio_handle_t snk, int64_t pos);                                  \
    BOOL    (*need_fix_cts)  (parser_handle_t parser);                                                                      \
    int32_t (*get_cts_offset)(parser_handle_t parser, uint32_t sample_idx);                                                 \
                                                                                                                            \
    uint8_t *(*write_cfg)(parser_handle_t parser, bbio_handle_t sink);                                                      \
                                                                                                                            \
    /** Returns: error code */                                                                                              \
    int32_t     (*write_au) (parser_handle_t parser, uint8_t *data, size_t size, bbio_handle_t sink);                       \
                                                                                                                            \
    /**  info_sink: if non-NULL the mp4 box data must be dumped to this sink */                                             \
    int32_t  (*parse_codec_config)(parser_handle_t parser, bbio_handle_t info_sink);                                        \
    BOOL (*is_valid_chunk)    (parser_handle_t parser, bbio_handle_t data, size_t size);                                    \
    int32_t  (*get_subsample)     (parser_handle_t parser, int64_t *pos, uint32_t subs_num_in, int32_t *more_subs_out, uint8_t *data, size_t *size); \
                                                                                                                            \
    int8_t conformance_type[4];                                                                                             \
    int32_t (*post_validation)(parser_handle_t parser);                                                                     \
    parser_reporter_t *reporter


struct parser_t_
{
    PARSER_BASE;
};

/**** the video parser interface and common data member */
#define PARSER_VIDEO_BASE                   \
    PARSER_BASE;                            \
                                            \
    uint32_t width, height;                 \
    uint32_t depth; /** reserved as 0x18 */ \
    uint32_t hSpacing;                      \
    uint32_t vSpacing;                      \
    uint32_t framerate;                     \
    uint8_t colour_primaries;               \
    uint8_t transfer_characteristics;       \
    uint8_t matrix_coefficients           

typedef struct parser_video_t_
{
    PARSER_VIDEO_BASE;
} parser_video_t;
typedef parser_video_t *parser_video_handle_t;


/**** the audio parser interface and common data member */
#define PARSER_AUDIO_BASE                           \
    PARSER_BASE;                                    \
                                                    \
    int32_t      channelcount;/** reserved as 2 */  \
    int32_t      samplesize;  /** reserved as 16 */ \
    int32_t      sample_rate;                       \
    uint32_t qtflags;                               \
    uint32_t wave_format

typedef struct parser_audio_t_
{
    PARSER_AUDIO_BASE;
}parser_audio_t ;
typedef parser_audio_t *parser_audio_handle_t;

typedef struct parser_meta_t_
{
    PARSER_BASE;

    int8_t *content_encoding;
    int8_t *content_namespace;
    int8_t *schema_location;
} parser_meta_t;
typedef parser_meta_t *parser_meta_handle_t;

typedef struct text_font_t_
{
    uint16_t font_id;
    int8_t *   font_name;
} text_font_t;

typedef struct text_frame_t_
{
    uint64_t dts;
    uint64_t cts;
    uint32_t duration;

    uint8_t *data;
    size_t   size;

    uint32_t *subsample_sizes;
    uint32_t  num_subsamples;
} text_frame_t;

typedef struct parser_text_t_
{
    PARSER_BASE;

    uint32_t flags;
    uint8_t  horizontal_justification;
    uint8_t  vertical_justification;
    uint8_t  bg_color[4];
    uint16_t top;
    uint16_t left;
    uint16_t bottom;
    uint16_t right;
    uint16_t translation_y;
    uint16_t translation_x;
    uint16_t start_char;
    uint16_t end_char;
    uint16_t font_id;
    uint8_t  font_flags;
    uint8_t  font_size;
    uint8_t  fg_color[4];

    const int8_t* subt_namespace;       /**< CFF: namespace of the schema for the subtitle document */
    const int8_t* subt_schema_location; /**< CFF: URL to find the schema corresponding to the namespace */
    const int8_t* subt_image_mime_type; /**< CFF: media type of any images present in subtitle samples */

    uint32_t video_width;       /**< CFF: Subtitle track inherits video width.  */
    uint32_t video_height;      /**< CFF: Subtitle track inherits video height. */
    uint32_t video_hSpacing;    /**< CFF: Horizontal aspect for calculating video width. */
    uint32_t video_vSpacing;    /**< CFF: Vertical aspect for calculating video width. */

    uint8_t mixed_subtitles;    /**< CFF: Does this track contain mixed image and text subtitle samples? */

    int8_t *handler_type;

    list_handle_t font_lst;

    uint32_t  number_of_frames;
    list_handle_t frame_lst;
} parser_text_t;
typedef parser_text_t *parser_text_handle_t;

typedef struct hint_frame_t_
{
    uint64_t dts;
    uint64_t cts;
    uint32_t duration;
    uint32_t size;
    uint8_t *data;
} hint_frame_t;

typedef struct parser_hint_t_
{
    PARSER_BASE;

    uint32_t ref_ID;            /** the track this hint track references */
    size_t   sample_buf_size;
    uint32_t number_of_frames;
    uint32_t trackSDPSize;
    int8_t *trackSDP;
} parser_hint_t;
typedef parser_hint_t *parser_hint_handle_t;

/** set a callback for reporting messages to higher layers of the application */
void parser_set_reporter(parser_handle_t parser, parser_reporter_t *reporter);

/** set conformance checking */
int32_t parser_set_conformance(parser_handle_t parser, const int8_t *type);

/**
 * (some) alternatives to direct function pointer usage
 *
 * - function pointer access is sometimes not easily possible - e.g. from python scripts
 */
int32_t  parser_call_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx, bbio_handle_t ds);
void     parser_call_destroy(parser_handle_t parser);
int32_t  parser_call_get_sample(parser_handle_t parser, mp4_sample_handle_t sample);
void     parser_call_sample_destroy(struct mp4_sample_t_ *);


/**** for registry function per every parser defined */
void parser_hevc_reg (void);
void parser_avc_reg  (void);
void parser_aac_reg  (void);
void parser_ac3_reg  (void);
void parser_ec3_reg  (void);
void parser_ac4_reg  (void);
void parser_video_reg(void);
void parser_audio_reg(void);


size_t       get_codec_config_size(parser_handle_t parser);
void         parser_destroy       (parser_handle_t parser);
int8_t         parser_get_type      (parser_handle_t parser);
dsi_handle_t parser_get_curr_dsi  (parser_handle_t parser);
void         parser_set_frame_size(parser_handle_t parser, uint32_t frame_size);

int32_t  dsi_list_create(parser_handle_t parser, uint32_t dsi_type);
void dsi_list_destroy(parser_handle_t parser);

mp4_sample_handle_t sample_create(void);
#define sample_destroy(sample)  sample->destroy(sample);
avi_sample_handle_t sample_create_avi(void);
void                sample_destroy_avi(avi_sample_handle_t sample);

/** interface to user parameters to aac parser */
#define PARSER_AAC_SIGNALING_MODE_IMPLICIT      0
#define PARSER_AAC_SIGNALING_MODE_SBR_BC        1
#define PARSER_AAC_SIGNALING_MODE_SBR_NBC       2
#define PARSER_AAC_SIGNALING_MODE_PS_BC         3
#define PARSER_AAC_SIGNALING_MODE_PS_NBC        4
void    parser_aac_set_signaling_mode  (parser_handle_t parser, uint32_t signaling_mode);
void    parser_aac_set_asc             (parser_handle_t parser, uint8_t *asc, uint32_t size);
void    parser_aac_set_config          (parser_handle_t parser, uint32_t frequency, BOOL has_sbr, BOOL has_ps, BOOL is_oversampled_sbr);
uint8_t parser_aac_get_profile_level_id(parser_handle_t parser);

void parser_lrc_add_text_sample     (parser_handle_t parser, uint64_t cts, uint32_t duration, uint8_t *data, uint32_t data_size);
void parser_lrc_set_dimensions      (parser_handle_t parser, uint16_t height, uint16_t width, uint16_t translation_y);
void parser_lrc_set_foreground_color(parser_handle_t parser, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void parser_lrc_set_background_color(parser_handle_t parser, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void parser_lrc_set_handler_type    (parser_handle_t parser, const int8_t *handler_type);

/** interface to user parameters to h263 parser */
void parser_h263_set_level          (parser_handle_t parser, uint8_t level);
void parser_h263_set_profile        (parser_handle_t parser, uint8_t profile);
void parser_h263_set_decoder_version(parser_handle_t parser, uint8_t version);
void parser_h263_set_vendor         (parser_handle_t parser, uint32_t vendor);

void parser_hint_set_ref     (parser_handle_t parser, uint32_t ref);
void parser_hint_set_trackSDP(parser_handle_t parser, uint32_t size, const int8_t *text);

void parser_mlp_set_fixed_timing(parser_handle_t parser, int32_t enable);

void parser_text_add_text_sample(parser_handle_t parser, uint64_t dts, uint64_t duration, const uint8_t *data, uint32_t data_size, const uint32_t *subsample_offsets, uint32_t num_subsamples);

int32_t find_start_code_off(bbio_handle_t ds, uint64_t size, uint32_t start_code, uint32_t start_code_size, uint32_t mask);


#ifdef __cplusplus
};
#endif

#endif /* !__PARSER_H__ */
