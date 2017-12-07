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
 *  @file  dsi.h
 *  @brief Defines decoder specific structures
 */

#ifndef __DSI_H__
#define __DSI_H__

#include "boolean.h"   /* BOOL */
#include "list_itr.h"  /* list_handle_t */

#ifdef __cplusplus
extern "C"
{
#endif

/**** the dsi interface and common data member */
typedef enum dsi_type_t_
{
    DSI_TYPE_MP4FF,  /* that for mp4ff */
    DSI_TYPE_ASF,    /* that for asf */
    DSI_TYPE_MP2TS,  /* that for mp2ts */
    DSI_TYPE_CFF     /* that for uv */
} dsi_type_t;

#if !defined(dsi_t_)
struct dsi_t_;
#endif
typedef struct dsi_t_  dsi_t;
typedef dsi_t  *dsi_handle_t;

#define DSI_BASE                                    \
    uint32_t dsi_type;                              \
    uint32_t stream_id;                             \
    void     (*destroy) (struct dsi_t_ *dsi);       \
    uint32_t raw_data_size;                         \
    void *   raw_data

struct dsi_t_
{
    DSI_BASE;
};

/**** avc dsi */
/** the common part */
#define DSI_AVC_BASE                                            \
    DSI_BASE;                                                   \
                                                                \
    uint8_t NALUnitLength; /* bytes to represent NALUs length */\
    uint8_t AVCProfileIndication;                               \
    uint8_t profile_compatibility;                              \
    uint8_t AVCLevelIndication

struct dsi_avc_t_
{
    DSI_AVC_BASE;
};
typedef struct dsi_avc_t_  dsi_avc_t;
typedef dsi_avc_t  *dsi_avc_handle_t;

/** mp4ff version */
struct mp4_dsi_avc_t_
{
    DSI_AVC_BASE;

    uint8_t configurationVersion;

    list_handle_t sps_lst;
    list_handle_t pps_lst;

    uint8_t       chroma_format;
    uint8_t       bit_depth_luma;
    uint8_t       bit_depth_chroma;
    list_handle_t sps_ext_lst;

    uint8_t dsi_in_mdat; /* the indicator of dsi info (H264 PPS/SPS) in mdat (in case of PPS change,i.e. multiple stsd) */
};
typedef struct mp4_dsi_avc_t_  mp4_dsi_avc_t;
typedef mp4_dsi_avc_t  *mp4_dsi_avc_handle_t;

/**** hevc dsi */
/** the common part */
#define DSI_HEVC_BASE                                           \
    DSI_BASE;                                                   \
                                                                \
    uint8_t NALUnitLength /* bytes to represent NALUs length */

struct dsi_hevc_t_
{
    DSI_HEVC_BASE;
};
typedef struct dsi_hevc_t_  dsi_hevc_t;
typedef dsi_hevc_t  *dsi_hevc_handle_t;


/** mp4ff version */
struct mp4_dsi_hevc_t_
{
    DSI_HEVC_BASE;

    uint8_t configurationVersion;

    list_handle_t vps_lst;
    list_handle_t sps_lst;
    list_handle_t pps_lst;

    uint8_t      profile_space;
    uint8_t      tier_flag;
    uint8_t      profile_idc;
    uint32_t     profile_compatibility_indications;

    uint8_t      progressive_source_flag;
    uint8_t      interlaced_source_flag;
    uint8_t      non_packed_constraint_flag;
    uint8_t      frame_only_constraint_flag;

    uint64_t     constraint_indicator_flags;
    uint8_t      level_idc;
    uint8_t      min_spatial_segmentation_idc;
    uint8_t      parallelismType;
    uint8_t      chromaFormat;
    uint8_t      bitDepthLumaMinus8;
    uint8_t      bitDepthChromaMinus8;

    uint16_t     AvgFrameRate;
    uint8_t      constantFrameRate;
    uint8_t      numTemporalLayers;
    uint8_t      temporalIdNested;
    uint8_t      lengthSizeMinusOne;
    uint8_t      numOfArrays;

    uint8_t      dsi_in_mdat; /* the indicator of dsi info (H265 VPS/SPS/PPS) in mdat (means sample entry name is "hev1") */
};
typedef struct mp4_dsi_hevc_t_  mp4_dsi_hevc_t;
typedef mp4_dsi_hevc_t  *mp4_dsi_hevc_handle_t;


/**** esd dsi */
struct mp4_aac_esd_t_
{
    uint8_t  id;
    uint8_t  objectTypeIndication;
    uint32_t bufferSizeDB;
    uint32_t maxBitrate;
    uint32_t avgBitrate;
};
typedef struct mp4_aac_esd_t_  mp4_aac_esd_t;

/**** aac dsi */
struct mp4_dsi_aac_t_
{
    DSI_BASE;

    mp4_aac_esd_t esd;
    uint8_t       audioObjectType;          /* in fact audioObjectTypeId */
    uint8_t       audioObjectTypeExt;

    uint8_t audioObjectType2;               /* in fact audioObjectTypeId */
    uint8_t audioObjectTypeExt2;

    uint8_t  samplingFrequencyIndex;
    uint32_t samplingFrequency;
    uint8_t  channelConfiguration;

    uint8_t  sbr_sampling_frequency_index;  /* extensionSamplingFrequencyIndex */
    uint32_t sbr_sampling_frequency;        /* extensionSamplingFrequency */

    uint8_t extensionChannelConfiguration;

    BOOL has_sbr;
    BOOL has_ps;
    BOOL have_ps_ext;
    BOOL have_sbr_ext;

    /* GA Specific Config */
    BOOL     frameLengthFlag;
    BOOL     dependsOnCoreCoder;
    uint16_t coreCoderDelay;
    BOOL     extensionFlag;

    /* ProgramConfigElement */
    uint8_t element_instance_tag;
    uint8_t object_type;
    uint8_t pce_sampling_frequency_index;
    uint8_t num_front_channel_elements;
    uint8_t num_side_channel_elements;
    uint8_t num_back_channel_elements;
    uint8_t num_lfe_channel_elements;
    uint8_t num_assoc_data_elements;
    uint8_t num_valid_cc_elements;
    uint8_t mono_mixdown_present;

    uint8_t mono_mixdown_element_number;
    uint8_t stereo_mixdown_present;
    uint8_t stereo_mixdown_element_number;
    uint8_t matrix_mixdown_idx_present;
    uint8_t matrix_mixdown_idx;
    uint8_t pseudo_surround_enable;
    uint8_t *front_element_is_cpe;
    uint8_t *front_element_tag_select;

    uint8_t *side_element_is_cpe;
    uint8_t *side_element_tag_select;

    uint8_t *back_element_is_cpe;
    uint8_t *back_element_tag_select;

    uint8_t *lfe_element_tag_select;
    uint8_t *assoc_data_element_tag_select;
    uint8_t *cc_element_is_ind_sw;
    uint8_t *valid_cc_element_tag_select;
    uint8_t comment_field_bytes;
    uint8_t *comment_field_data;

    uint8_t layerNr;
    uint8_t numOfSubFrame;
    uint16_t layer_length;

    uint8_t aacSectionDataResilienceFlag;
    uint8_t aacScalefactorDataResilienceFlag;
    uint8_t aacSpectralDataResilienceFlag;

    uint8_t extensionFlag3;
    uint8_t extensionAudioObjectType;
    uint8_t extensionAudioObjectTypeExt;

    /* Helper vars */
    /* Use this instead of channel Configuration for channel count
     * as channel configuration can be 0 for MPEG-4 ADTS
     */
    uint8_t channel_count;
};
typedef struct mp4_dsi_aac_t_  mp4_dsi_aac_t;
typedef mp4_dsi_aac_t  *mp4_dsi_aac_handle_t;

/**** ac3 dsi */
struct mp4_dsi_ac3_t_
{
    DSI_BASE;

   int fscod, bsid, bsmod, acmod, lfeon;
   int bit_rate_code;
};
typedef struct mp4_dsi_ac3_t_  mp4_dsi_ac3_t;
typedef mp4_dsi_ac3_t  *mp4_dsi_ac3_handle_t;


struct ec3_substream_t_
{
    int fscod, bsid, bsmod, acmod, lfeon;
    int num_dep_sub, chan_loc;
};
typedef struct ec3_substream_t_  ec3_substream_t;

/**** ec3 dsi */
struct mp4_dsi_ec3_t_
{
    DSI_BASE;

    int data_rate, num_ind_sub;
    ec3_substream_t *substreams;
} ;
typedef struct mp4_dsi_ec3_t_  mp4_dsi_ec3_t;
typedef mp4_dsi_ec3_t  *mp4_dsi_ec3_handle_t;

/**** ac4 dsi */
struct mp4_dsi_ac4_t_
{
    DSI_BASE;
};
typedef struct mp4_dsi_ac4_t_  mp4_dsi_ac4_t;
typedef mp4_dsi_ac4_t  *mp4_dsi_ac4_handle_t;

/****** dsi */
dsi_handle_t dsi_hevc_create(uint32_t dsi_type);

dsi_handle_t mp4_dsi_avc_create(void);
dsi_handle_t mp2ts_dsi_avc_create(void);
dsi_handle_t dsi_avc_create(uint32_t dsi_type);

dsi_handle_t mp4_dsi_aac_create(void);
dsi_handle_t dsi_aac_create(uint32_t dsi_type);

dsi_handle_t mp4_dsi_ac3_create(void);
dsi_handle_t dsi_ac3_create(uint32_t dsi_type);

dsi_handle_t mp4_dsi_ec3_create(void);
dsi_handle_t dsi_ec3_create(uint32_t dsi_type);

dsi_handle_t mp4_dsi_ac4_create(void);
dsi_handle_t dsi_ac4_create(uint32_t dsi_type);

#ifdef __cplusplus
};
#endif

#endif /* !__DSI_H__ */
