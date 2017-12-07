/******************************************************************************
 * This program is protected under international and U.S. copyright laws as
 * an unpublished work. This program is confidential and proprietary to the
 * copyright owners. Reproduction or disclosure, in whole or in part, or the
 * production of derivative works therefrom without the express permission of
 * the copyright owners is prohibited.
 *
 *                Copyright (C) 2008-2012 by Dolby Laboratories.
 *                            All rights reserved.
 ******************************************************************************/
/**
 *  @file  mp4_ctrl.h
 *  @brief Defines mp4 ctrl struct for mp4 muxer/demuxer
 */

#ifndef __MP4CTRL_H__
#define __MP4CTRL_H__

#include <stdio.h>        /* FILE */

#include "parser.h"       /* ext_timing_info_t */
#include "mp4_frag.h"     /* trex_t, tfhd_t, trun_t, tfra_t */
#include "mp4_encrypt.h"  /* mp4_encryptor_handle_t */

#define MAX_STREAMS 300   /**< internal stream number supported, allows max .uvu with 1 video, 32 audio, 255 subtitle tracks */

#define MAX_NUM_EDIT_LIST 16

#define ISOM_MUXCFG_ENCRYPTSTYLE_MASK   (0xff)
#define ISOM_MUXCFG_ENCRYPTSTYLE_CENC   0  /**< encryption boxes conform to Common Encryption */
#define ISOM_MUXCFG_ENCRYPTSTYLE_PIFF   1  /**< encryption boxes written according to PIFF */

#define ISOM_MUXCFG_TKHD_FLAG_MASK      0x000F
#define ISOM_MUXCFG_TRACK_ENABLED       0x0001
#define ISOM_MUXCFG_TRACK_IN_MOVIE      0x0002
#define ISOM_MUXCFG_TRACK_IN_PREVIEW    0x0004
#define ISOM_MUXCFG_TRACK_IN_POSTER     0x0008

#define ISOM_MUXCFG_BIT0            8
#define ISOM_MUXCFG_WRITE_IODS      (0x1<<(ISOM_MUXCFG_BIT0+0))
#define ISOM_MUXCFG_WRITE_PDIN      (0x1<<(ISOM_MUXCFG_BIT0+1))
#define ISOM_MUXCFG_WRITE_BLOC      (0x1<<(ISOM_MUXCFG_BIT0+2))
#define ISOM_MUXCFG_WRITE_AINF      (0x1<<(ISOM_MUXCFG_BIT0+3))
#define ISOM_MUXCFG_WRITE_FREE      (0x1<<(ISOM_MUXCFG_BIT0+4))
#define ISOM_MUXCFG_WRITE_CTTS_V1   (0x1<<(ISOM_MUXCFG_BIT0+5))
#define ISOM_MUXCFG_WRITE_SUBS_V1   (0x1<<(ISOM_MUXCFG_BIT0+6))
#define ISOM_MUXCFG_WRITE_STSS      (0x1<<(ISOM_MUXCFG_BIT0+7))

#define ISOM_MUXCFG_DEFAULT         (ISOM_MUXCFG_WRITE_IODS | ISOM_MUXCFG_WRITE_SUBS_V1 | ISOM_MUXCFG_WRITE_STSS)

#define ISOM_FRAGCFG_FRAGSTYLE_MASK        (0xff)
#define ISOM_FRAGCFG_FRAGSTYLE_DEFAULT     1  /**< Fragmentation must follow: each fragment starts with a sync sample. */
#define ISOM_FRAGCFG_FRAGSTYLE_CCFF        2  /**< Fragmentation must conform to the Common File Format as specified in the Ultraviolet spec */

#define ISOM_FRAGCFG_BIT0                    8
#define ISOM_FRAGCFG_WRITE_TFDT              (0x1<<(ISOM_FRAGCFG_BIT0+0))
#define ISOM_FRAGCFG_WRITE_SDTP              (0x1<<(ISOM_FRAGCFG_BIT0+1))
#define ISOM_FRAGCFG_WRITE_SENC              (0x1<<(ISOM_FRAGCFG_BIT0+2))
#define ISOM_FRAGCFG_WRITE_TRIK              (0x1<<(ISOM_FRAGCFG_BIT0+3))
#define ISOM_FRAGCFG_WRITE_AVCN              (0x1<<(ISOM_FRAGCFG_BIT0+4))
#define ISOM_FRAGCFG_FORCE_TFRA              (0x1<<(ISOM_FRAGCFG_BIT0+5))   /**< force writing tfra for all tracks */
#define ISOM_FRAGCFG_NO_BDO_IN_TFHD          (0x1<<(ISOM_FRAGCFG_BIT0+6))   /**< do not write base-data-offset in 'tfhd' */
#define ISOM_FRAGCFG_EMPTY_TREX              (0x1<<(ISOM_FRAGCFG_BIT0+7))   /**< do not write defaults into 'trex' */
#define ISOM_FRAGCFG_EMPTY_TFHD              (0x1<<(ISOM_FRAGCFG_BIT0+8))   /**< do not write defaults into 'tfhd' */
#define ISOM_FRAGCFG_ONE_TFRA_ENTRY_PER_TRAF (0x1<<(ISOM_FRAGCFG_BIT0+9))   /**< do not write multiple 'tfra' entries that point to the same fragment. */
#define ISOM_FRAGCFG_WRITE_SIDX              (0x1<<(ISOM_FRAGCFG_BIT0+10))  /**< enable writing of 'sidx' box */
#define ISOM_FRAGCFG_DEFAULT_BASE_IS_MOOF    (0x1<<(ISOM_FRAGCFG_BIT0+11))
#define ISOM_FRAGCFG_WRITE_MFRA              (0x1<<(ISOM_FRAGCFG_BIT0+12))
#define ISOM_FRAGCFG_FORCE_TFHD_SAMPDESCIDX  (0x1<<(ISOM_FRAGCFG_BIT0+13))  /**< write sample description index into 'tfhd' */
#define ISOM_FRAGCFG_FORCE_TRUN_V0           (0x1<<(ISOM_FRAGCFG_BIT0+14))  /**< always write version 0 in TRUN, irrespective of CTTS version */

#define ISOM_FRAGCFG_DEFAULT     (ISOM_FRAGCFG_FRAGSTYLE_DEFAULT)

 /** fragment does not start with a sync sample */
#define EMAMP4_WARNFLAG_FRAG_NO_SYNC 0x1

/* editing action */
#define TRACK_EDIT_ACTION_NONE          0
#define TRACK_EDIT_ACTION_ADD           1
#define TRACK_EDIT_ACTION_DELETE        2
#define TRACK_EDIT_ACTION_REPLACE       3

/* sample entry name configure */
#define ISOM_MUXCFG_HEVC_SAMPLE_ENTRY_MASK  (0xff) 
#define ISOM_MUXCFG_HEVC_SAMPLE_ENTRY_HEV1  0 /**< 0: "hev1" */
#define ISOM_MUXCFG_HEVC_SAMPLE_ENTRY_HVC1  1 /**< 1: "hvc1" */

/* dolby vision muxing mode */
#define ISOM_DOLBY_VISION_MUXING_SINGLE_TRACK_MODE 0
#define ISOM_DOLBY_VISION_MUXING_DUAL_TRACK_MODE   1

#define MP4BASE_V_API  1
#define MP4BASE_V_FCT  5
#define MP4BASE_V_MTNC 1

#ifdef __cplusplus
extern "C"
{
#endif

/** 
 * @brief output file format type 
 */
enum OutputFormat
{
    OUTPUT_FORMAT_UNKNOWN,
    OUTPUT_FORMAT_MP4,
    OUTPUT_FORMAT_FRAG_MP4,
    OUTPUT_FORMAT_DASH,
    OUTPUT_FORMAT_3GP,
    OUTPUT_FORMAT_PIFF,
    OUTPUT_FORMAT_UVU,
};



enum DolbyVisionTrackMode 
{
    SINGLE,
    DUAL,
};

enum DolbyVisionEsMode
{
    COMB,
    SPLIT,
};



/** 
 * @brief DASH profile 
 */
enum DashProfile { Main, OnDemand, Live, HbbTV };

/**
 *  @brief Version info struct.
 */
typedef struct mp4base_version_info_
{
    int32_t         v_api;   /**< API (interface) version number */
    int32_t         v_fct;   /**< Functional version number */
    int32_t         v_mtnc;  /**< Maintenance version number */
    const int8_t* text;      /**< version text */
} mp4base_version_info;

typedef struct elst_entry_t_
{
    uint64_t    segment_duration;   /* in movie timescale */
    int64_t     media_time;         /* -1: nothing to play */
    uint32_t    media_rate;         /* 0, this is a dwell */
} elst_entry_t;

/**** config info per es */
typedef struct usr_cfg_es_t_
{
    uint32_t input_mode;
    const int8_t * input_fn;                           /**< valid if has file input */
    const int8_t * lang;
    const int8_t * enc_name;
    const int8_t * hdlr_name;
    const int8_t * sample_entry_name;
    uint32_t     chunk_span_size;                      /**< chunk of this stream is controlled by size in byte */
    uint32_t     es_idx;                               /**< its idx in usr_cfg_ess[] */
    uint32_t     track_ID;                             /**< track_ID this es mapped into: assigned by muxer */
    uint16_t     alternate_group;                      /**< allow user set-able alternate_group in 'tkhd' */
    uint32_t     warp_media_timescale;                 /**< timescale to warp timestamps to */
    uint32_t     force_tkhd_flags;                     /**< override flags in the track header box */
    uint32_t     force_tfhd_flags;                     /**< override flags in the track fragment header */
    uint32_t     force_trun_flags;                     /**< override flags in the track run box */
    uint16_t     force_sidx_ref_count;                 /**< override reference_count in "sidx" box */
    int32_t          use_audio_channelcount;           /**< 1: Use actual number of main audio channels, 0: Always 2 like stated in the Dolby File Spec */
    int32_t          default_sample_description_index; /**< provide default sample description index */
    int32_t          mp4_tid;                          /**< track ID in mp4 file to extract */
    uint32_t     action;                               /**< track edit action flag */
    uint32_t     sample_entry_name_flag;               /**< flags for specific hevc sample entry name     0: "hvc1"; 1: "hev1"*/
} usr_cfg_es_t;

/**** config info per mux */
typedef struct usr_cfg_mux_t_
{
    uint32_t output_mode;                  /**< if is a valid sink */
    const int8_t * output_fn;              /**< valid if has file output */
    const int8_t * output_fn_el;
    uint32_t     output_file_num;
    uint32_t     timescale;                /**< movie time scale */
    uint32_t     mux_cfg_flags;            /**< flags for enabling specific features ISO media files */
    uint32_t     free_box_in_moov_size;    /**< if set anything but zero than a free box of that size (net) will be inserted at end of moov */

    ext_timing_info_t ext_timing_info;     /**< external def for h264 timeing in frame rate */

    uint64_t    fix_cm_time;               /**< if creation and modification time is a fixed value */
    uint32_t    chunk_span_time;           /**< chunk span in ms. 0 for non-interleave */
    uint32_t    frag_cfg_flags;            /**< flags for enabling different features of fragmented ISO media files */
    uint32_t    frag_range_max;            /**< max fragment duration in ms */
    uint32_t    frag_range_min;            /**< min fragment duration in ms */
    const int8_t *major_brand;             /**< major brand */
    const int8_t *compatible_brands;       /**< compatible brands */
    uint32_t    brand_version;             /**< the major brand version */
    uint32_t    sd;                        /**< 0: Only single sample description allowed. 1: Multiple sample descriptions allowed. */
    uint32_t    withopt;                   /**< additional options */
    uint32_t    max_pdu_size;              /**< max mtu size for network payload (hint track) */

    int32_t es_num;
    enum OutputFormat output_format;            
    enum DashProfile dash_profile;
    uint32_t segment_output_flag;
    uint32_t SegmentCounter;
    uint8_t OD_profile_level;              /**< the OD profile that goes in the initial object description */
    uint8_t scene_profile_level;           /**< the scene profile that goes in the initial object description */
    uint8_t audio_profile_level;           /**< the audio profile that goes in the initial object description */
    uint8_t video_profile_level;           /**< the video profile that goes in the initial object description */
    uint8_t graphics_profile_level;        /**< the graphics profile that goes in the initial object description */

    enum DolbyVisionTrackMode dv_track_mode;
    enum DolbyVisionEsMode    dv_es_mode;

    uint8_t dv_bl_non_comp_flag;

    uint8_t      elst_track_id;            /**< track ID for elst */
    elst_entry_t elst[MAX_NUM_EDIT_LIST];  /**< edit list apply to track elst_track_id */
} usr_cfg_mux_t;

typedef struct chunk_t_
{
    uint32_t idx;                   /* sample idx for easier indexing */
    uint64_t dts;                   /* dts of the first sample in this chunk, in movie_timescale */
    offset_t offset;                /* file offset of the chunk in temporary file or mp4 file */
    uint32_t data_reference_index;  /* thatfor all the samples in this chunk */

    uint32_t sample_num;           /* number of samples in this chunk */
    uint64_t size;                 /* chunk size(of all samples it contains) */

    uint32_t sample_description_index;
} chunk_t;
typedef chunk_t *chunk_handle_t;

/* dts_lst, sync_lst entry */
typedef struct idx_dts_t_
{
    uint32_t idx;
    uint64_t dts;
} idx_dts_t;

/* for stsd_lst entry */
typedef struct idx_ptr_t_
{
    uint32_t idx;
    uint8_t *ptr;
} idx_ptr_t;

typedef struct box_dref_t_
{
    int8_t type[4];
    int8_t *path;
} box_dref_t;

#define IS_VERSION_1(data_tbl)  ((data_tbl->version_flag & 0xFF000000) == 0x01000000)
typedef struct box_data_tbl_t_
{
    /** tbl from mp4 file */
    uint32_t version_flag;
    uint32_t entry_count;
    offset_t offset;
    size_t   size;
    uint8_t *data;
    uint32_t add_info;      /**< additional info for table box. defined for stsz(frame_size), stz2(field_size) only */
    BOOL     variant;       /**< if is a variation of a table. TRUE: co64, or stz2 */

    /** last search info */
    uint32_t sample_idx0;   /**< the sample index at the beginning of a entry in last search */
    uint32_t entry_idx;     /**< the entry index */
    uint64_t acc_val;       /**< accumulated value from first entry up to the entry: like dts, offset */
} box_data_tbl_t;

/**** info per stream for building mp4 file
* track_t collects info provided by parser in order to build track level and down
* mp4 file component. parser holds stream specific info */
typedef struct track_t_
{
    /**** cfg info */
    uint32_t track_ID;                          /**< this track id */
    int8_t     codingname[5];                   /**< fourCC type for stsd codingname including terminating zero */
    uint32_t output_mode;                       /**< to avoid cross reference, copied from usr_cfg_mux */

    uint32_t media_timescale;                   /**< media timescale */
    uint64_t media_duration;                    /**< track duration in media timescale */
    uint64_t sum_track_edits;                   /**< track duration in movie timescale; i.e. duration of all the track edits, used as duration in 'tkhd' */
    uint32_t elst_version;

    uint16_t alternate_group;                   /**< alternate_group field in 'tkhd' */

    BOOL     warp_media_timestamps;
    uint32_t warp_media_timescale;
    uint32_t warp_parser_timescale;

    uint16_t sidx_reference_count;              /**< reference_count used for sidx box creation */

    BOOL     write_pre_roll;

    uint32_t warn_flags;

    uint64_t creation_time, modification_time;  /**< for now same for track and media */
    int32_t      language_code;                 /**< ISO 639 3-letter language code (empty string if undefined) */
    int8_t     codec_name[32];
    uint32_t audio_channel_count;               /**< hold the channel count value for the audio sample entry,
                                                      this way it works only for the case were there is only a single sample entry per track. */
    int32_t      use_audio_channelcount;        /**< 1: Use actual number of main audio channels, 0: Always 2 like stated in the Dolby File Spec */

    double   totalBitrate;

    /**** raw info for generating mp4 file */
    uint32_t sample_num;
    uint32_t sample_duration;
    uint32_t sample_descr_index;                /**< current used value: 0 for none */
    uint32_t last_sample_descr_index;
    uint16_t data_ref_index;                    /**< track could be spread over several files.
                                                     A sample entry has one file associated with it, referenced by this id. */

    /* timing */
    list_handle_t dts_lst;                      /**< (idx, dts) */
    list_handle_t cts_offset_lst;               /**< (idx, count, value) */
    list_handle_t sync_lst;                     /**< (idx, dts) */
    list_handle_t edt_lst;                      /**< (segment_duration, media_time, media_rate) */
    /* location */
    list_handle_t size_lst;                     /**< (idx, count, value) */
    list_handle_t chunk_lst;                    /**< (idx, dts, offset, data_reference_index, sample_num, size, sample_description_index) */
    /* stsd */
    list_handle_t stsd_lst;                     /**< (idx, ptr) */
    list_handle_t sdtp_lst;                     /**< sample dependency information for 'sdtp' box */
    list_handle_t trik_lst;                     /**< sample dependency information for 'trik' box */
    list_handle_t frame_type_lst;               /**< sample frame type information; level for 'ssix' box*/
    list_handle_t subs_lst;                     /**< subsample information for 'subs' box */

    list_handle_t segment_lst;                  /**< segment index */

    /** chunk and data */
    /* build chunk */
    uint32_t chunk_span_time;                   /**< chunk span in media_timescale. 0 for non-interleave */
    uint64_t chunk_dts_top;                     /**< the top of dts allowed in current chunk, in media timescale */
    uint64_t max_chunk_size;                    /**< chunk size limit */

    uint32_t acc_size;                          /**< accumulate sample size in a chunk */
    uint32_t prev_sample_idx;                   /**< previous sample index */
    uint32_t prev_sync_num;                     /**< previous sync sample number */

    /* output chunk */
    uint32_t chunk_num, chunk_to_out;           /**< chunk number and next to output */

    uint64_t mdat_size;                         /**< contribution to mdata */
    /* help flags */
    BOOL     all_rap_samples;
    BOOL     all_same_size_samples;
    BOOL     no_cts_offset;


    uint32_t cts_offset_v1_base;

    /**** cross reference */
    struct mp4_ctrl_t_ *mp4_ctrl;               /**< to refer ready only cfg info, default fragment info */
    parser_handle_t     parser;                 /**< ref to parser to access info */
    uint32_t            es_idx;                 /**< the es index the track correponding to */

    /**** raw decoder specific config */
    uint32_t dsi_size;
    uint8_t *dsi_buf;

    int8_t     es_tmp_fn[256];                    /**< the name */
    FILE *   file;                              /**< tmp file */
    offset_t stco_offset;                       /**< where the stco offset in mp4 file */

    /**** fragment */
    uint32_t         frag_num;
    trex_t           trex;
    /* TODO: tfhd/trun only keeps current copy
        if keep all fragment data, it need to preallocate memory,
        however, unless searching all fragments, there is no total size info */
    tfhd_t           tfhd;
    trun_t           trun;
    tfra_t           tfra;
    uint64_t         frag_dts;                  /**< cut off dts in media_timescale */
    uint32_t         frag_duration;             /**< fragment duration in media_timescale */
    /* the number of sample available within an entry still available for next trun */
    BOOL             traf_is_prepared;          /**< indicates a track fragment run is already prepared and the get_trun() 
                                                     only returns the max. size as single trun */
    uint32_t         size_cnt, cts_offset_cnt;
    uint64_t         frag_size;                 /**< size of track fragments mdat in bytes */
    uint64_t         max_total_frag_size;       /**< max. size of track fragment incl. moof+mdat in bytes */
    BOOL             first_trun_in_traf;
    uint32_t         trun_samples_read;
    uint32_t         num_truns_read;
    offset_t         aux_data_offs;
    mp4_sample_t *   frag_samples;
    /* and to track the location of sample */
    list_handle_t    pos_lst;                   /**< (pos) */
    it_list_handle_t size_it;
    uint32_t         size_cnt_4mdat;            /**< the size_cnt used for write mdat */
    uint32_t         size_4mdat;                /**< the current size used for write mdat */
    /* to build tfra. for practical case, assuming length_size_of_... field is one byte */
    list_handle_t    tfra_entry_lst;            /**< with tfra_entry_t as content */
    uint32_t         trun_idx;                  /**< for tfra */
    /* test only */
    uint32_t         sample_num_to_fraged;
    /* end of fragment */

    mp4_encryptor_handle_t encryptor;           /**< encryptor handle */
    list_handle_t          enc_info_lst;        /**< encryption information per (sub-)sample (enc_subsample_info_t) */
    it_list_handle_t       enc_info_mdat_it;    /**< subsample info list iterator for mdat writing */
    uint32_t               senc_flags;

    BOOL subs_present;                          /**< Are subsamples present in this track? */

    const int8_t *hdlr_name;                    /**< Value of 'name' field in 'hdlr' box (muxer). */

    /* decryption */
    int8_t                   crypt_scheme_type[4];
    uint32_t               crypt_scheme_version;
    uint8_t                crypt_keyid[16];
    uint32_t               crypt_algid;
    uint32_t               crypt_iv_size;
    mp4_encryptor_handle_t decryptor;

    /***** Demux stuffs */
    uint32_t strm_idx;                          /**< stream index in demuxer */
    int8_t     orig_fmt[4];                     /**< original format (of encrypted stream) */

    stream_type_t stream_type;

    uint32_t flags;                             /**< the flags in track flag-version field */

    uint16_t translation_x;                     /**< translation offset for x axis (for subtitle support) */
    uint16_t translation_y;                     /**< translation offset for y axis (for subtitle support) */

    int64_t  start_time;                        /**< PTS of the first sample: TODO: make it the smallest PTS */
    uint64_t media_creation_time;
    uint64_t media_modification_time;
    int8_t   language[4];                       /**< language string converted from ISO 639-2/T in mdhd */
    int8_t * name;                              /**< human-readable name for tack type in hdlr */
    int16_t  volume;                            /**< volume in tkhd */
    uint32_t visual_width;                      /**< width and height in tkhd */
    uint32_t visual_height;

    /** edts */
    box_data_tbl_t elst;

    /** tref */
    uint32_t track_ref_ID;                      /**< ID of the track being referenced */

    /** sdp */
    uint8_t  *sdp_text;                         /**< SDP string */
    uint32_t  sdp_size;                         /**< SDP string length */

    /** dinf */
    uint32_t    drefs_count;
    box_dref_t *drefs;

    /** sample tables */
    box_data_tbl_t stsd;
    box_data_tbl_t stts;
    box_data_tbl_t ctts;
    box_data_tbl_t stss;
    box_data_tbl_t stsc;
    box_data_tbl_t stsz;
    box_data_tbl_t stco;

    /** derived value */
    uint32_t sample_max_size;

    /** provided by API call: first dts value */
    uint64_t dts_offset;

    /** fragment: for the purpose of verify the muxer, demuxer will dump out es on the fly
    * using the info in the following three boxes */
    uint64_t      dts;                          /**< sample dts */
    bbio_handle_t frag_snk_file;
    void *BL_track;
} track_t;
typedef track_t *       track_handle_t;
typedef struct track_t_ stream_t;         /* now stream is track */
typedef stream_t *      stream_handle_t;

/**** atom blobs to insert when building mp4 file */
typedef struct atom_data_t_
{
    int8_t *   data;
    uint32_t size;
    int8_t     parent_box_type[4];  /**< parent container box type */
    uint32_t track_ID;              /**< track id if under track box */
} atom_data_t;
typedef atom_data_t *atom_data_handle_t;

/**** progress notification callback */
typedef void (*progress_callback_t)(const float progress, void *instance);

/**** onwrite notifification callbacks */
typedef int32_t (*onwrite_callback_t)(void *instance);

struct mp4_ctrl_t_
{
    /* Demux */

    uint32_t        timescale;            /**< movie timescale */
    uint64_t        duration;             /**< movie duration in movie timescale */
    uint32_t        stream_num;
    stream_handle_t tracks[MAX_STREAMS];
    stream_handle_t stream_active;
    bbio_handle_t   mp4_src;

    BOOL isom;                            /**< 1 = ISO/IEC 14496-12 */
    BOOL moov_parsed;

    /* movie header */
    int32_t movie_rate;
    int16_t movie_volume;

    /** fragment */
    uint64_t fragment_duration;
    uint32_t sequence_number;           /**< sequence number for fragments */
    /* helper */
    BOOL           input_frag_file;     /**< if input file is a fragment file */
    int64_t        moof_offset;         /**< current moof start point offset */
    int64_t        mdat_offset;         /**< current mdat read offset */
    uint8_t       *cp_buf;              /**< copy buffer */
    size_t         cp_buf_size;
    bbio_handle_t  buf_snk;
    BOOL           first_traf_in_moof;  /**< if first traf in moof */

    /* dup that in ema_mp4... to avoid backward ref */
    int8_t * fn_out;
    size_t fn_out_base_len, fn_out_buf_size;

    int64_t first_moof_offset;
    BOOL    frag_second_pass;

    int8_t *   major_brand;               /**< major brand (from ftyp box) */
    int8_t *   compatible_brands;         /**< compatible brands (from ftyp box) */
    uint32_t   brand_version;             /**< the major brand version (from ftyp box) */

    /**** info */
    int8_t *        info_fn;
    int32_t       infoonly;
    bbio_handle_t info_sink;
    BOOL          info_brief;           /**< brief output is enabled if TRUE */

    /* Mux */

    /**** movie layer */
    uint32_t next_track_ID;             /**< next_track_ID value in mvhd box */

    uint64_t creation_time;
    uint64_t modification_time;

    uint32_t chunk_num;
    offset_t mdat_pos;
    uint64_t mdat_size;
    uint32_t moov_size_est;

    /**** OD profile level */
    uint8_t OD_profile_level;

    /**** scene profile level */
    uint8_t scene_profile_level;

    /**** video profile level */
    uint8_t video_profile_level;

    /**** audio profile level */
    uint8_t audio_profile_level;

    /**** graphics profile level */
    uint8_t graphics_profile_level;

    /**** track and output */
    BOOL          co64_mode;            /**< if 64 bit co or not */
    bbio_handle_t mp4_sink;             /**< output */ /* dup that in ema_mp4... to avoid backward ref  */
    bbio_handle_t mp4_sink_el;
    BOOL          track_ignored;        /**< ignore the track currently processing */

    /**** to help build the IOD */
    BOOL has_avc;
    BOOL has_mp4v;
    BOOL has_mp4a;

    /**** convenient reference to user cfg */
    usr_cfg_mux_t *usr_cfg_mux_ref;
    usr_cfg_es_t * usr_cfg_ess_ref;
    uint32_t       curr_usr_cfg_stream_index;

    /**** fragment */
    uint32_t      frag_ctrl_track_ID;   /**< whose rap starts a frag */
    uint32_t      frag_dts;             /**< the dts(in ms) cut off value for current moof */
    /* for mfra */
    uint32_t      traf_idx;             /**< for mfra */
    list_handle_t next_track_lst;       /**< list if prepared tracks for fragments (track_handle_t*) */


    /**** ID32, asset and iTunes atom blobs */
    list_handle_t moov_child_atom_lst;  /**< (atom_data_t) */
    list_handle_t udta_child_atom_lst;  /**< (atom_data_t) */

    /**** specific boxes */
    atom_data_t moov_ainf_atom;
    atom_data_t bloc_atom;

    /**** Items to be added to the meta box (memory managed externally) */
    const int8_t     *moov_meta_xml_data;
    const int8_t     *moov_meta_hdlr_type;
    const int8_t     *moov_meta_hdlr_name;
    const int8_t     **moov_meta_items;
    const uint32_t   *moov_meta_item_sizes;
    uint16_t         num_moov_meta_items;

    /**** Items to be added to the meta box (memory managed externally) */
    const int8_t     *footer_meta_xml_data;
    const int8_t     *footer_meta_hdlr_type; 
    const int8_t     *footer_meta_hdlr_name; 
    const int8_t     **footer_meta_items;
    const uint32_t   *footer_meta_item_sizes;
    uint16_t         num_footer_meta_items;

    /** scratch buffer */
    uint8_t        *scratchbuf;
    size_t         scratchsize;

    int32_t demux_flag;

    void (*destroy)(struct mp4_ctrl_t_ *ctrl);

    progress_callback_t progress_cb;
    void *              progress_cb_instance;

    onwrite_callback_t onwrite_next_frag_cb;
    void *             onwrite_next_frag_cb_instance;
};

typedef struct mp4_ctrl_t_  mp4_ctrl_t;
typedef mp4_ctrl_t *        mp4_ctrl_handle_t;

/**
 *  @brief Retrieves library version information.
 */
const mp4base_version_info* mp4base_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* !__MP4CTRL_H__ */
