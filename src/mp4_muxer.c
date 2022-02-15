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
    @file mp4_muxer.c
    @brief Implements all of supported boxes and basic muxing logic
 */

#include "utils.h"
#include "registry.h"
#include "io_base.h"
#include "parser.h"
#include "mp4_isom.h"
#include "mp4_muxer.h"
#include "mp4_stream.h"

/** Macros to help write 32 bit size field
 *  when derive the size from position of the box in file
 */
#define SKIP_SIZE_FIELD(snk)                   \
    offset_t pos_size__ = snk->position(snk);  \
    sink_write_u32(snk, 0)

#define CURRENT_BOX_OFFSET()        pos_size__

#define WRITE_SIZE_FIELD(snk) write_size_field(snk, pos_size__)

#define WRITE_SIZE_FIELD_RETURN(snk) {         \
      return write_size_field(snk, pos_size__);\
  }


/** Writes the common part of sample entry. note: size field is already skipped */
#define MOV_WRITE_SAMPLE_ENTRY(snk, codingname, data_reference_index)   \
    snk->write(snk, codingname, 4);                                     \
    sink_write_u32(snk, 0);          /** reserved */                    \
    sink_write_u16(snk, 0);          /** reserved */                    \
    sink_write_u16(snk, data_reference_index)

/*
    Returns: the size written
 */
static uint32_t
write_size_field(bbio_handle_t snk, offset_t pos_size)
{
    offset_t pos_cur = snk->position(snk);
    uint32_t size = (uint32_t)(pos_cur - pos_size);

    snk->seek(snk, pos_size, SEEK_SET);
    sink_write_u32(snk, size);
    snk->seek(snk, pos_cur, SEEK_SET);

    return size;
}

/** Storage for fragment index information. */
typedef
struct frag_index_t_
{
    uint32_t frag_start_idx;
    uint32_t frag_end_idx;
}
frag_index_t;

/** Storage for sample dependency 'sdtp' information. */
typedef
struct sample_sdtp_t_
{
    uint8_t is_leading;
    uint8_t sample_depends_on;
    uint8_t sample_is_depended_on;
    uint8_t sample_has_redundancy;
    uint8_t sample_is_non_sync_sample;
}
sample_sdtp_t;

/** Storage for sample dependency 'trik' information. */
typedef
struct sample_trik_t_
{
    uint8_t pic_type;
    uint8_t dependency_level;
}
sample_trik_t;

/** Storage for sample type(h264 frame type: I(0),P(1),B(2) ); for 'ssix' level information. */
typedef
struct sample_frame_type_t_
{
    uint8_t frame_type;
}
sample_frame_type_t;

/** Storage for subsample 'subs' information. */
typedef
struct sample_subs_t_
{
    uint32_t subsample_size;
    uint32_t num_subs_left;
}
sample_subs_t;

#ifdef ENABLE_MP4_ENCRYPTION
/** Storage for encrypted sub sample information. */
typedef 
struct enc_subsample_info_t_
{
    enc_sample_info_t enc_info;
    uint32_t          subs_cnt;    /**< subsample count */
}
enc_subsample_info_t;
#endif

static int8_t*
get_codingname(parser_handle_t parser)
{
    int8_t *codingname;

    switch (parser->stream_id)
    {
    case STREAM_ID_HEVC: codingname = "hvc1"; break; 
    case STREAM_ID_H264: codingname = "avc1"; break;
    case STREAM_ID_H263: codingname = "s263"; break;
    case STREAM_ID_MP4V: codingname = "mp4v"; break;
    case STREAM_ID_VC1:  codingname = "vc-1"; break;
    case STREAM_ID_AC3:  codingname = "ac-3"; break;
    case STREAM_ID_EC3:  codingname = "ec-3"; break;
    case STREAM_ID_AC4:  codingname = "ac-4"; break;
    case STREAM_ID_MLP:  codingname = "mlpa"; break;
    case STREAM_ID_METX: codingname = "metx"; break;
    case STREAM_ID_METT: codingname = "mett"; break;
    case STREAM_ID_TX3G: codingname = "tx3g"; break;
    case STREAM_ID_STPP: codingname = "stpp"; break;
    default:
        if (parser->stream_type == STREAM_TYPE_VIDEO)
        {
            if (parser->dsi_FourCC)
                codingname = parser->dsi_FourCC;
            else
                codingname = "mp4v";
        }
        else if (parser->stream_type == STREAM_TYPE_AUDIO)
        {
            codingname = "mp4a";
        }
        else
        {
            codingname = parser->dsi_FourCC;
        }
        break;
    }

    return codingname;
}

static void
write_private_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer, int8_t* parent_box_type, uint32_t track_ID)
{
    if (muxer->moov_child_atom_lst != NULL)
    {
        it_list_handle_t   it;
        atom_data_handle_t atom;

        it = it_create();
        it_init(it, muxer->moov_child_atom_lst);
        while ((atom = it_get_entry(it)))
        {
            if (IS_FOURCC_EQUAL(atom->parent_box_type, parent_box_type) &&
                (atom->track_ID == track_ID))
            {
                snk->write(snk, (uint8_t *)atom->data, atom->size);
                muxer->moov_size_est += atom->size;
            }
        }
        it_destroy(it);
    }
}

static int32_t
write_ftyp_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    const int8_t *brand   = muxer->usr_cfg_mux_ref->major_brand;
    const int8_t *cbrands = muxer->usr_cfg_mux_ref->compatible_brands;
    uint32_t    version = muxer->usr_cfg_mux_ref->brand_version;
    uint32_t    i,j,k = 0;
    uint8_t     compatible_brands[256];
    uint32_t    len     = (uint32_t)strlen(cbrands);

    for(i = 0, j = 0; i < len; i++)
    {
        if (*(cbrands + i) != ',')
            compatible_brands[j++] = (uint8_t)(*(cbrands + i));
        k = j;
    }

    sink_write_u32(snk, 16 + k);   /** size */
    sink_write_4CC(snk, "ftyp");
    sink_write_4CC(snk, brand);
    sink_write_u32(snk, version); /** write version */
    snk->write(snk, (const uint8_t *)compatible_brands, j);

    return 16 + len;
}

static int32_t
write_styp_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    const int8_t *brand   = "mp42";
    const int8_t *cbrands = "mp42msdhiso5isom";
    uint32_t    version = 1;
    int32_t         len     = (int)strlen(cbrands);

    sink_write_u32(snk, 16 + len);   /** size */
    sink_write_4CC(snk, "styp");
    sink_write_4CC(snk, brand);
    sink_write_u32(snk, version); /** write version */
    snk->write(snk, (const uint8_t *)cbrands, len);

    return 16 + len;
    (void)muxer;  /** avoid compiler warning */
}

/** Progressive Download Information Box
Provide 3 sample points:
At the middle sample point the download rate matches about the data rate
of the movie. In this case the start-up delay is determined by the size
of the movie header, incl. 'moov' and all boxes prior to the first 'mdat'.

The last sample point assumes a higher download speed. Playback can start
immediately after the header is downloaded.

The first sample point uses a lower download speed. Hence the startup
delay is the sum of the time to download the movie header plus the
difference of download time and playback time.

Using these 3 sample points assures that linear interpolation and
extrapolation by a player also results in good estimates.
*/
static int32_t
write_pdin_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
#define NUM_PDIN_FIELDS 3

    uint32_t i;
    uint32_t rate[NUM_PDIN_FIELDS];  /** kB/s   */
    uint32_t initial_delay[NUM_PDIN_FIELDS];  /** ms   */
    uint64_t duration = rescale_u64(muxer->duration, 1000, muxer->timescale);  /** ms   */
    uint32_t baserate;
    uint64_t header_size = muxer->moov_size_est+320000;   /** add 320k for free box + bloc + ... */
    uint32_t num_fields = 0;
    uint32_t size;

    baserate = (uint32_t)((muxer->mdat_size + 1)/(duration + 1)) + 1;
    baserate += (baserate >> 3); /** add ~10% overhead */

    if (baserate >= 16)
    {
        rate[num_fields] = (baserate >> 4);  /** just an arbitrary sample point at baserate/16 */
        initial_delay[num_fields] = (uint32_t)(header_size/rate[num_fields] + (duration<<4) - duration);
        num_fields++;
    }
    else if (baserate >= 2)
    {
        rate[num_fields] = (baserate >> 1);  /** another arbitrary sample point at baserate/2 */
        initial_delay[num_fields] = (uint32_t)(header_size/rate[num_fields] + (duration<<1) - duration);
        num_fields++;
    }
    rate[num_fields] = baserate;         /** add baserate */
    initial_delay[num_fields] = (uint32_t)(header_size/rate[num_fields]);
    num_fields++;
    rate[num_fields] = (baserate << 4);  /** another sample point at 16 * baserate */
    initial_delay[num_fields] = (uint32_t)(header_size/rate[num_fields]);
    num_fields++;

    /** FullBox header */
    size = 12+num_fields*8;
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "pdin");
    sink_write_u32(snk, 0);           /** version & flags */

    for (i = 0; i < num_fields; i++)
    {
        sink_write_u32(snk, rate[i]);
        sink_write_u32(snk, initial_delay[i]);
    }
    return size;
}

#define EMPTY_BUF_SIZE 32
static void
write_empty(bbio_handle_t snk, int32_t cnt)
{
    uint8_t empty_buf[EMPTY_BUF_SIZE] = {'\0'};
    while (cnt > 0)
    {
        int32_t chunk = (cnt < EMPTY_BUF_SIZE) ? cnt : EMPTY_BUF_SIZE;
        snk->write(snk, empty_buf, chunk);
        cnt -= chunk;
    }
}

/**
 * @brief Writes (DECE) Subtitle Media Header Box
 * see [CFF] Section 2.2.10
 */
static int32_t
write_sthd_box(bbio_handle_t snk)
{
    /** FullBox header */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "sthd");
    sink_write_u32(snk, 0);       /** version & flags */

    WRITE_SIZE_FIELD_RETURN(snk);
}


static int32_t
write_free_box(bbio_handle_t snk, uint32_t size)
{
    /** Box header */
    sink_write_u32(snk, 8+size);
    sink_write_4CC(snk, "free");
    /** data */
    write_empty(snk, size);

    return 8+size;
}


/** Track Fragment Base Media Decode Time Box */
static int32_t
write_tfdt_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t   size;
    uint32_t   version = 1;
    uint32_t   versionflags = 0;
    idx_dts_t *dts_id;
    uint32_t   dts_u32 = 0;
    uint64_t   dts_u64;

    dts_id  = list_it_peek_entry(track->dts_lst);
    dts_u64 = dts_id->dts;
    if (dts_u64 < 0xffffffff)
    {
        version = 0;
        dts_u32 = (uint32_t)dts_u64;
    }

    /** get box size */
    size  = (version == 1) ? 8 : 4;

    /** Box header */
    sink_write_u32(snk, 12+size);
    sink_write_4CC(snk, "tfdt");
    sink_write_u8(snk, (uint8_t)version);     /** version */
    sink_write_bits(snk, 24, versionflags);  /** flags */

    if (version == 1)
    {
        sink_write_u64(snk, dts_u64); /** baseMediaDecodeTime */
    }
    else
    {
        sink_write_u32(snk, dts_u32); /** baseMediaDecodeTime */
    }

    return 12+size;
}

/**
 * @brief Writes (DECE) Trick Play Box
 * see [CFF] Section 2.2.7
 * - required for video tracks (such as AVC)
 */
static int32_t
write_trik_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t i;

    /** sample count is taken from 'trun' when part of 'traf' */
    uint32_t sample_count = track->trun.sample_count;
    uint32_t size         = 12 + sample_count;

    /** FullBox header */
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "trik");
    sink_write_u32(snk, 0);       /** version & flags */

    /** Access the trik samples list */
    for (i = 0; i < sample_count && list_get_entry_num(track->trik_lst); ++i)
    {
        sample_trik_t *entry = (sample_trik_t *)list_it_get_entry(track->trik_lst);
        sink_write_bits(snk, 2, entry->pic_type);
        sink_write_bits(snk, 6, entry->dependency_level);
    }

    return size;
}

/**
 * @brief Writes (DECE) AVC NAL Unit Storage Box
 * see [CFF] Section 2.2.2
 * - only for video tracks (AVC)
 * - required for Late Binding feature
 */
static int32_t
write_avcn_box(bbio_handle_t snk, track_handle_t track)
{
    it_list_handle_t it_ip  = it_create();
    it_list_handle_t it_dsi = it_create();
    idx_ptr_t *      ip;
    dsi_handle_t *   p_dsi;

    uint32_t frag_start_idx;
    uint32_t frag_end_idx;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "avcn");

    /** AVCDecoderConfigurationRecord - see [ISOAVC] Section 5.2.4.1 */

    /*
     * implementation detail:
     * we are expecting that write_trun_box() was called and therefore track->tfhd.sample_num
     * needs to be substracted from track->sample_num_to_fraged
     */
    frag_start_idx = track->sample_num_to_fraged - 1 - track->tfhd.sample_num;
    frag_end_idx   = track->sample_num_to_fraged - 1 - 1;

    /** check stsd_lst to determine which dsi / AVCDecoderConfigurationRecord to write */
    it_init(it_ip,  track->stsd_lst);
    it_init(it_dsi, track->parser->dsi_lst);
    while ((ip = it_get_entry(it_ip)))
    {
        idx_ptr_t *ip2;

        /** advance in dsi_lst to get the corresponding entry */
        p_dsi = (dsi_handle_t*)it_get_entry(it_dsi);

        if (ip->idx > frag_end_idx)
        {
            break;
        }
        ip2 = it_peek_entry(it_ip);
        if (ip2)
        {
            if (ip2->idx <= frag_start_idx)
            {
                continue;
            }
        }

        /** set current dsi */
        assert(p_dsi != NULL);
        track->parser->curr_dsi = *p_dsi;

        /** update dsi */
        if (track->parser->get_cfg)
        {
            size_t size = 0;
            track->parser->get_cfg(track->parser, &track->dsi_buf, &size);
            track->dsi_size = (uint32_t)size;
        }

        /** write dsi / AVCDecoderConfigurationRecord */
        snk->write(snk, (uint8_t *)track->dsi_buf, track->dsi_size);
    }
    it_destroy(it_ip);
    it_destroy(it_dsi);

    WRITE_SIZE_FIELD_RETURN(snk);
}

/** Independent and Disposable Samples Box */
static int32_t
write_sdtp_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t i;

    /** sample count is taken from 'trun' when part of 'traf' */
    uint32_t sample_count = track->trun.sample_count;
    uint32_t size         = 12 + sample_count;

    /** FullBox header */
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "sdtp");
    sink_write_u32(snk, 0);       /** version & flags */

    /** Access the sdtp samples list */
    for (i = 0; i < sample_count && list_get_entry_num(track->sdtp_lst) > 0; ++i)
    {
        sample_sdtp_t *entry = (sample_sdtp_t *)list_it_get_entry(track->sdtp_lst);
        sink_write_bits(snk, 2, entry->is_leading);
        sink_write_bits(snk, 2, entry->sample_depends_on);
        sink_write_bits(snk, 2, entry->sample_is_depended_on);
        sink_write_bits(snk, 2, entry->sample_has_redundancy);
    }

    return size;
}

/**
 * @brief Writes Sub-Sample Information Box
 * see [ISO] Section 8.7.7 and [CFF] Section 6.6.1.6
 * 
 * Function assumes that the 'subs' box is used only for subtitle tracks.
 */
static int32_t
write_subs_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t i;
    BOOL     first_subs;
    uint32_t entry_count        = track->trun.sample_count != 0 ? track->trun.sample_count : track->sample_num;
    uint32_t sample_delta       = 1; /** Write sparse entries for samples without subsamples. */
    uint16_t subsample_count    = 0;
    uint16_t subsamples_written = 0;
    uint8_t  version            = (track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_SUBS_V1) != 0 ? 1 : 0;

    /** FullBox header */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "subs");
    sink_write_u8(snk, version);    /** version */
    sink_write_bits(snk, 24, 0x0);  /** flags are all 0 */

    sink_write_u32(snk, entry_count);

    /** Write information for each entry. */
    for (i = 0; i < entry_count; ++i)
    {
        first_subs = TRUE;
        sink_write_u32(snk, sample_delta);

        /** Write information for each subsample. */
        while (1)
        {
            uint8_t  subsample_priority = 0;  /** Not used. */
            uint8_t  discardable        = 0;  /** Not used. */
            uint32_t reserved           = 0;

            sample_subs_t *subs = (sample_subs_t *)list_it_get_entry(track->subs_lst);
            if (!subs || (subsamples_written == 0 && subs->num_subs_left == 0))
            {
                sink_write_u16(snk, 0); /** There are zero subsamples. */
                break;
            }
            if (first_subs)
            {
                subsample_count = (uint16_t)subs->num_subs_left + 1;
                sink_write_u16(snk, subsample_count);
                first_subs = FALSE;
            }

            if (version & 0x01)
            {
                sink_write_u32(snk, subs->subsample_size);
            }
            else
            {
                sink_write_u16(snk, subs->subsample_size & 0xffff);
            }
            sink_write_u8(snk, subsample_priority);
            sink_write_u8(snk, discardable);
            sink_write_u32(snk, reserved);
            ++subsamples_written;

            if (subs->num_subs_left == 0)
            {
                break;
            }
        }
        assert(subsamples_written == subsample_count);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

#ifdef ENABLE_MP4_ENCRYPTION
static int32_t
write_saio_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t versionflags = 0x1;
    offset_t offset;
    uint32_t size = 12+4+4;

    if (versionflags & 0x1)
    {
        size += 8;
    }

    sink_write_u32(snk, size);
    sink_write_4CC(snk, "saio");
    sink_write_u32(snk, versionflags);

    if (versionflags & 0x1)
    {
        if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
        {
            sink_write_4CC(snk, "piff"); /** aux_info_type */
            sink_write_u32(snk, 0x0);    /** aux_info_type_parameter */
        }
        else
        {
            sink_write_4CC(snk, "cenc"); /** aux_info_type */
            sink_write_u32(snk, 0x0);    /** aux_info_type_parameter */
        }
    }
    sink_write_u32(snk, 1); /** entry_count */
    /** offset from 'moof' to 1st entry in 'senc': 
        + 4     (offset in 'saio')
        + 12    (full box 'senc')
        + 4     (sample_count in 'senc') */
    offset = snk->position(snk) - track->mp4_ctrl->moof_offset + 4 + 12 + 4;
    if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
    {
        offset += 16; /** add uuid box overhead */
    }
    sink_write_u32(snk, (uint32_t) offset);
    return 0;
}

static int32_t
write_saiz_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t              num_samples;
    uint32_t              num_subs;
    uint32_t              versionflags = 0x1;
    enc_subsample_info_t *enc_info_ptr;
    uint8_t               default_sample_info_size = 0;
    uint8_t               sample_info_size;
    const uint8_t         iv_bytes = (uint8_t)(track->encryptor->iv_size >> 3);
    uint32_t              sample_count = track->trun.sample_count != 0 ? track->trun.sample_count : track->sample_num;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "saiz");
    sink_write_u32(snk, versionflags);      /** version & flags */

    if (versionflags & 0x1)
    {
        if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
        {
            sink_write_4CC(snk, "piff"); /** aux_info_type */
            sink_write_u32(snk, 0x0);    /** aux_info_type_parameter */
        }
        else
        {
            sink_write_4CC(snk, "cenc"); /** aux_info_type */
            sink_write_u32(snk, 0x0);    /** aux_info_type_parameter */
        }
    }

    list_it_save_mark(track->enc_info_lst);
    num_samples = sample_count;

    while (num_samples)
    {
        enc_info_ptr     = list_it_get_entry(track->enc_info_lst);
        num_subs         = enc_info_ptr->subs_cnt;
        sample_info_size = iv_bytes;
        if (track->senc_flags & 0x2)
        {
            sample_info_size += (uint8_t)(6 * num_subs);
            while (num_subs--)
            {
                list_it_get_entry(track->enc_info_lst);
            }
        }
        if (num_samples == sample_count)
        {
            default_sample_info_size = sample_info_size;
        }
        else if (default_sample_info_size != sample_info_size)
        {
            default_sample_info_size = 0; /** -> break? */
        }
        num_samples--;
    }
    if (default_sample_info_size && (track->senc_flags & 0x2))
    {
        default_sample_info_size += 8;  /** subsample count + initial set */
    }
    list_it_goto_mark(track->enc_info_lst);

    sink_write_u8(snk, default_sample_info_size);
    sink_write_u32(snk, sample_count);  /** sample_count */

    if (default_sample_info_size == 0)
    {
        list_it_save_mark(track->enc_info_lst);
        num_samples = sample_count;
        while (num_samples)
        {
            enc_info_ptr     = list_it_get_entry(track->enc_info_lst);
            sample_info_size = (uint8_t)(track->encryptor->iv_size >> 3); /** IV size in bytes */
            if (track->senc_flags & 0x2)
            {
                num_subs = enc_info_ptr->subs_cnt;
                sample_info_size += (uint8_t)(8 + (6 * num_subs));
                while (num_subs--)
                {
                    list_it_get_entry(track->enc_info_lst);
                }
            }
            sink_write_u8(snk, sample_info_size);
            num_samples--;
        }
        list_it_goto_mark(track->enc_info_lst);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_senc_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t num_samples, num_subs;
    enc_subsample_info_t *enc_info_ptr;
    int32_t iv_bytes, i;
    uint32_t sample_count = track->trun.sample_count != 0 ? track->trun.sample_count : track->sample_num;

    SKIP_SIZE_FIELD(snk);
    if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
    {
        sink_write_4CC(snk, "uuid");
        snk->write(snk, (uint8_t *)"\xA2\x39\x4F\x52\x5A\x9B\x4f\x14\xA2\x44\x6C\x42\x7C\x64\x8D\xF4", 16);
    }
    else
    {
        sink_write_4CC(snk, "senc");
    }
    sink_write_u32(snk, track->senc_flags);         /** version & flags */
    sink_write_u32(snk, sample_count);              /** sample_count */

    num_samples = sample_count;
    iv_bytes    = (track->encryptor->iv_size >> 3); /** IV size in bytes */
    while (num_samples)
    {
        enc_info_ptr = list_it_get_entry(track->enc_info_lst);
        for (i = 0; i < iv_bytes; i++)
        {
            sink_write_u8(snk, enc_info_ptr->enc_info.initial_value[i]);
        }
        if (track->senc_flags & 0x2)
        {
            num_subs = enc_info_ptr->subs_cnt;
            sink_write_u16(snk, (uint16_t)num_subs+1);
            sink_write_u16(snk, (uint16_t)enc_info_ptr->enc_info.num_clear_bytes);
            sink_write_u32(snk, enc_info_ptr->enc_info.num_encrypted_bytes);
            while (num_subs--)
            {
                enc_info_ptr = list_it_get_entry(track->enc_info_lst);
                assert(enc_info_ptr->subs_cnt == num_subs);
                sink_write_u16(snk, (uint16_t)enc_info_ptr->enc_info.num_clear_bytes);
                sink_write_u32(snk, enc_info_ptr->enc_info.num_encrypted_bytes);
            }
        }
        num_samples--;
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_encryption_info_boxes(bbio_handle_t snk, track_handle_t track)
{
    if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) != ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
    {
        write_saiz_box(snk, track);
        write_saio_box(snk, track);
    }
    write_senc_box(snk, track);
    return 0;
}
#endif

static int32_t
write_hdlr2_box(bbio_handle_t snk, const int8_t* hdlr_type, const int8_t* name)
{
    int32_t i;

    /** FullBox header */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "hdlr");
    sink_write_u32(snk, 0);         /** version & flags */

    sink_write_u32(snk, 0);         /** pre_defined */
    sink_write_4CC(snk, hdlr_type); /** Common File Metadata */
    for (i = 0; i < 3; i++)
    {
        sink_write_u32(snk, 0);    /** reserved */
    }

    snk->write(snk, (uint8_t*)name, strlen(name));
    sink_write_u8(snk,0);          /** terminate name string */

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_xml_box(bbio_handle_t snk, const int8_t* xml)
{
    uint32_t size = (uint32_t)strlen(xml);

    /** FullBox header */
    sink_write_u32(snk, 12+1+size);
    sink_write_4CC(snk, "xml ");
    sink_write_u32(snk, 0);      /** version & flags */

    snk->write(snk, (uint8_t*)xml, strlen(xml));
    sink_write_u8(snk,0);        /** terminate name string */

    return 12+1+size;
}

#define ILOC_DEFAULT_OFFSET_SIZE 4         /** must be from {0, 4, 8} */

static int32_t
write_iloc_box(bbio_handle_t snk, const uint32_t *item_sizes, uint16_t item_count)
{
    uint16_t i;
    const uint8_t version = 1;
    const uint8_t index_size = 0;
    const uint16_t construction_method = 1;

    offset_t offset, idat_offset_abs;
    const offset_t k_limit32bit = ((1ULL<<32)-1);

    const uint32_t extent_count = 1;  /** Single extents will be used for all items */
    const uint32_t length_size = 4;
    const uint32_t base_offset_size = 0;
    uint32_t offset_size = ILOC_DEFAULT_OFFSET_SIZE;
    uint32_t base_offset = 0;

    /** FullBox header */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "iloc");
    sink_write_u8(snk, version); /** version */
    sink_write_bits(snk, 24, 0); /** flags */

    /** find largest offset */
    idat_offset_abs = snk->position(snk) + 2 + 2 + item_count * (2 + ((version==1)?2:0) + 2 + base_offset_size + 2 + offset_size + length_size) + 8;
    offset = (construction_method == 1) ? 0 : idat_offset_abs;
    for (i = 0; item_count > 1 && i < item_count-1; i++)
    {
        offset += item_sizes[i];
    }

    /** use 64-bit offsets if necessary */
    if (offset > k_limit32bit)
    {
        offset_size = 8;
    }
    idat_offset_abs += item_count * (offset_size - ILOC_DEFAULT_OFFSET_SIZE);

    sink_write_bits(snk, 4, offset_size);
    sink_write_bits(snk, 4, length_size);
    sink_write_bits(snk, 4, base_offset_size);
    if (version == 1)
    {
        sink_write_bits(snk, 4, index_size);
    }
    else
    {
        sink_write_bits(snk, 4, 0); /** reserved in version==0 */
    }

    sink_write_u16(snk, item_count);
    offset = (construction_method == 1) ? 0 : idat_offset_abs;
    for (i = 0; i < item_count; i++)
    {
        sink_write_u16(snk, i + 1); /** item_ID (one-based indexing) */
        if (version == 1)
        {
            sink_write_u16(snk, construction_method & 0xf);/** reserved = 0 (12 bits), construction_method (4 bits) */
        }
        sink_write_u16(snk, 0);                            /** data_reference_index (0 for this file only) */
        if (base_offset_size == 4)
        {
            sink_write_u32(snk, base_offset);              /** base offset   */
        }
        sink_write_u16(snk, (uint16_t)extent_count);       /** extent count  */
        if (offset_size==8)
        {
            sink_write_u64(snk, (uint64_t)offset);         /** extent offset as 64-bit */
        }
        else
        {
            sink_write_u32(snk, (uint32_t)offset);         /** extent offset as 32-bit */
        }
        sink_write_u32(snk, item_sizes[i]);                /** extent length */
        offset += item_sizes[i];
    }
    WRITE_SIZE_FIELD_RETURN(snk);
}

/**
 * @brief write Item Data Box
 * see [ISO] Section 8.11.11
 */
static int32_t
write_idat_box(bbio_handle_t snk, const int8_t **items, const uint32_t num_items, const uint32_t *item_sizes)
{
    uint32_t i = 0;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "idat");

    for (i = 0; i < num_items; ++i)
    {
        snk->write(snk, (const uint8_t *)items[i], item_sizes[i]);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_meta_box
(
    bbio_handle_t   snk,
    const int8_t*     xml,
    const int8_t*     hdlr_type,
    const int8_t*     name,
    const int8_t **   items,
    const uint32_t *item_sizes,
    const uint16_t  num_items
)
{
    if (xml)
    {
        /** FullBox header */
        SKIP_SIZE_FIELD(snk);
        sink_write_4CC(snk, "meta");
        sink_write_u8(snk, 0);       /** version */
        sink_write_bits(snk, 24, 0); /** flags */

        /** [ISO] Section 8.4.3: Handler Reference Box */
        /** [CFF] Section 2.3.3: Handler Reference Box for Common File Metadata */
        write_hdlr2_box(snk, hdlr_type, name);

        /** [ISO] Section 8.11.2: XML Box */
        /** [CFF] Section 2.3.4.1: XML Box for Required Metadata */
        write_xml_box(snk, xml);

        if (num_items > 0)
        {
            /** [ISO] Section 8.11.3: Item Location Box */
            write_iloc_box(snk, item_sizes, num_items);

            /** write binary items directly after the iloc box */
            write_idat_box(snk, items, num_items, item_sizes);
        }

        WRITE_SIZE_FIELD_RETURN(snk);
    }
    return 0;
}

static int32_t
write_mvhd_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint64_t duration;
    int32_t      version;
    uint32_t size;

    /** movie duration is already calculated in setup_muxer() with the correct information from the edit lists */
    if ((muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG))
    {
        duration = 0;
    }
    else
    {
        duration = muxer->duration;
    }

    assert(muxer->modification_time >= muxer->creation_time); /** assumption */
    if (muxer->duration > 0xffffffff|| muxer->modification_time > (uint32_t)(-1))
    {
        version = 1;    /** bit64 = TRUE; */
        size    = 120;
    }
    else
    {
        version = 0;    /** bit64 = FALSE; */
        size    = 108;
    }
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "mvhd");
    sink_write_u8(snk, (uint8_t)version); /** version */
    sink_write_bits(snk, 24, 0);          /** flags */

    if (version == 1)
    {
        sink_write_u64(snk, muxer->creation_time);
        sink_write_u64(snk, muxer->modification_time);
        sink_write_u32(snk, muxer->timescale);
        sink_write_u64(snk, duration);
    }
    else
    {
        sink_write_u32(snk, (uint32_t)muxer->creation_time);
        sink_write_u32(snk, (uint32_t)muxer->modification_time);
        sink_write_u32(snk, muxer->timescale);
        sink_write_u32(snk, (uint32_t)duration);
    }
    sink_write_u32(snk, 0x00010000); /** rate 1.0 */
    sink_write_u16(snk, 0x0100);     /** volume 1.0 */

    /** 10 bytes reserved */
    sink_write_u16(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);

    /** Matrix structure 9*4byte reserved */
    sink_write_u32(snk, 0x00010000);
    sink_write_u32(snk, 0x0);
    sink_write_u32(snk, 0x0);

    sink_write_u32(snk, 0x0);
    sink_write_u32(snk, 0x00010000);
    sink_write_u32(snk, 0x0);

    sink_write_u32(snk, 0x0);
    sink_write_u32(snk, 0x0);
    sink_write_u32(snk, 0x40000000);

    /** 6*4byte reserved */
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);

    sink_write_u32(snk, muxer->next_track_ID);
    return size;
}

static int32_t
write_iods_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint32_t track_idx;
    uint32_t total_ES_ID_Inc_size;

    total_ES_ID_Inc_size = 0;
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        if (muxer->tracks[track_idx]->sample_num)
        {
            total_ES_ID_Inc_size += 6;
        }
    }
    if (!total_ES_ID_Inc_size) 
    {
        return 0;
    }

    /** 12 B */
    sink_write_u32(snk, 21 + total_ES_ID_Inc_size);         /** size: 12 + 2 + 2 + 5 + ES_ID_Inc(s) */
    sink_write_4CC(snk, "iods");                            /** type */
    sink_write_u32(snk, 0);                                 /** version, flags */

    /** 2 B */
    sink_write_u8(snk, 0x10);                               /** mp4_iod_tag */
    sink_write_u8(snk, (uint8_t)(7 + total_ES_ID_Inc_size));/** size */

    /** 2 B */
    sink_write_bits(snk, 10, 1);                            /** ODID */
    sink_write_bits(snk, 1, 0);                             /** has URL String */
    sink_write_bits(snk, 1, 0);                             /** has inline profile */
    sink_write_bits(snk, 4, 0xf);                           /** reserved */

    /** 5 B:  profile, level indicators */
    sink_write_u8(snk, muxer->OD_profile_level);            /** OD  */
    sink_write_u8(snk, muxer->scene_profile_level);         /** scene  */
    sink_write_u8(snk, muxer->audio_profile_level);         /** Audio */
    sink_write_u8(snk, muxer->video_profile_level);         /** Video */
    sink_write_u8(snk, muxer->graphics_profile_level);      /** graphics */

    /** 6*x B */
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        track_handle_t track = muxer->tracks[track_idx];
        if (track->sample_num)
        {
            sink_write_u8(snk, 0x0e);                       /** ES_ID_IncTag  */
            sink_write_u8(snk, 0x04);                       /** payload size  */
            sink_write_u32(snk, track->track_ID);           /** Track_ID      */
        }
    }

    return 21 + total_ES_ID_Inc_size;
}

static offset_t
write_elst_box(bbio_handle_t snk, track_handle_t track)
{
    it_list_handle_t it = it_create();
    elst_entry_t *entry;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "elst");
    sink_write_u32(snk, track->elst_version<<24);
    sink_write_u32(snk, list_get_entry_num(track->edt_lst));

    /** dump elst */
    it_init(it, track->edt_lst);
    while ((entry = it_get_entry(it)))
    {
        if (track->elst_version == 1)
        {
            sink_write_u64(snk, entry->segment_duration);
            sink_write_u64(snk, (uint64_t)entry->media_time);
        }
        else
        {
            sink_write_u32(snk, (uint32_t)entry->segment_duration);
            sink_write_u32(snk, (uint32_t)entry->media_time);
        }
        sink_write_u32(snk, entry->media_rate << 16); /** write media_rate_integer and media_rate_fraction (here: 0) at once */
    }
    it_destroy(it);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_edts_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "edts");
    write_elst_box(snk,track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_smhd_box(bbio_handle_t snk)
{
    sink_write_u32(snk, 16);     /** size */
    sink_write_4CC(snk, "smhd");
    sink_write_u32(snk, 0);      /** version & flags */
    sink_write_u16(snk, 0);      /** reserved (balance, normally = 0) */
    sink_write_u16(snk, 0);      /** reserved */
    return 16;
}

static int32_t
write_vmhd_box(bbio_handle_t snk)
{
    sink_write_u32(snk, 0x14);   /** size (always 0x14) */
    sink_write_4CC(snk, "vmhd");
    sink_write_u32(snk, 0x01);   /** version & flags */
    sink_write_u16(snk, 0);      /** graphicmode 0 = copy */
    sink_write_u16(snk, 0);      /** opcolor: 0, 0, 0 */
    sink_write_u16(snk, 0);
    sink_write_u16(snk, 0);
    return 0x14;
}

static int32_t
write_hmhd_box(bbio_handle_t snk, track_handle_t track)
{
    sink_write_u32(snk, 28);     /** size */
    sink_write_4CC(snk, "hmhd");
    sink_write_u32(snk, 0);      /** version & flags */
    sink_write_u16(snk, (uint16_t)track->mp4_ctrl->usr_cfg_mux_ref->max_pdu_size);  /** max pdu size */
    sink_write_u16(snk, (uint16_t)track->mp4_ctrl->usr_cfg_mux_ref->max_pdu_size);  /** avg pdu size */
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    sink_write_u32(snk, 0);
    return 16;
}

static int32_t
write_nmhd_box(bbio_handle_t snk)
{
    sink_write_u32(snk, 12);     /** size */
    sink_write_4CC(snk, "nmhd");
    sink_write_u32(snk, 0);      /** version & flags */
    return 16;
}

static int32_t
write_dref_box(bbio_handle_t snk)
{
    sink_write_u32(snk, 28);     /** size */
    sink_write_4CC(snk, "dref");
    sink_write_u32(snk, 0);      /** version & flags */
    sink_write_u32(snk, 1);      /** entry count */

    sink_write_u32(snk, 0xc);    /** size */
    sink_write_4CC(snk, "url ");
    sink_write_u32(snk, 1);      /** version & flags: self-contained */

    return 28;
}

static offset_t
write_dinf_box(bbio_handle_t snk)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "dinf");
    write_dref_box(snk);
    WRITE_SIZE_FIELD_RETURN(snk);
}

/** protection specific boxes */
#define UUID_SIZE            16

static offset_t
write_frma_box(bbio_handle_t snk, track_handle_t track)
{
    const int8_t *codingname = track->codingname;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "frma");

    /** sample entry name should be */
    if (
        (track->parser->ext_timing.ext_dv_profile == 1)                                     /** non-bc dual layer, dual track */ 
        || (track->parser->ext_timing.ext_dv_profile == 3)                                  /**  non-bc dual layer, dual track */ 
        || ((track->parser->dv_rpu_nal_flag == 1) && (track->parser->dv_el_nal_flag == 0))  /** non-bc single layer,single track; dual layer, EL track */
        )
    {
        if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "avcC"))
            codingname = "dvav";
        else if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "hvcC"))
            codingname = "dvhe";    
    }

    sink_write_4CC(snk, codingname);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_schm_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t versionflags = 0;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "schm");
    sink_write_u32(snk, versionflags);     /** version & flags */

    if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
    {
        sink_write_4CC(snk, "piff");      /** scheme_type: PIFF */
        sink_write_u32(snk, 0x00010001);  /** version 1.1 */
    }
    else
    {
        sink_write_4CC(snk, "cenc");      /** scheme_type: Common Encryption */
        sink_write_u32(snk, 0x00010000);  /** version 1.0 */
    }

    /** this is dead code, assume it's a placeholder */
    if (versionflags & 0x1)
    {
        sink_write_u8(snk, '\0');
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_tenc_box(bbio_handle_t snk, track_handle_t track)
{
    int32_t i;
    uint32_t default_IV_size, default_AlgorithmID;

    SKIP_SIZE_FIELD(snk);
    if ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_ENCRYPTSTYLE_MASK) == ISOM_MUXCFG_ENCRYPTSTYLE_PIFF)
    {
        sink_write_4CC(snk, "uuid");
        snk->write(snk, (uint8_t *)"\x89\x74\xdb\xce\x7b\xe7\x4c\x51\x84\xf9\x71\x48\xf9\x88\x25\x54", 16);
    }
    else
    {
        sink_write_4CC(snk, "tenc");
    }
    sink_write_u32(snk, 0);     /** version & flags */

    default_AlgorithmID = 1;    /** 0: none, 1: AES-CTR */
    default_IV_size     = (track->encryptor->iv_size >> 3);     /** 8/16: 64/128 bit initialization vectors */ 

    sink_write_bits(snk, 24, default_AlgorithmID);
    sink_write_bits(snk,  8, default_IV_size);
    for (i = 0; i < UUID_SIZE; i++)
    {
        sink_write_bits(snk, 8, track->encryptor->keyId[i]);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_schi_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "schi");
    write_tenc_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}


static offset_t
write_sinf_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "sinf");
    write_frma_box(snk, track);
    write_schm_box(snk, track);
    write_schi_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_dsi_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t size = (uint32_t)track->dsi_size + 8;

    sink_write_u32(snk, size);
    snk->write(snk, (uint8_t *)track->parser->dsi_FourCC, 4);
    snk->write(snk, (uint8_t *)track->dsi_buf, track->dsi_size);
    
    /** add dolby vision dsi */
    if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "avcC") || (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "hvcC")))
    {
        if (track->parser->dv_dsi_size)
        {
            sink_write_u32(snk,track->parser->dv_dsi_size + 8);
            if (track->parser->ext_timing.ext_dv_profile > 7)
            {
                sink_write_4CC(snk, "dvvC");
            }
            else
            {
                sink_write_4CC(snk, "dvcC");
            }

            snk->write(snk, (uint8_t *)track->parser->dv_dsi_buf, track->parser->dv_dsi_size);
            size += track->parser->dv_dsi_size + 8;
        }

        /** add el config box: avcE or hvcE */
        if ((track->parser->dv_el_nal_flag == 1) && (track->parser->dv_rpu_nal_flag == 1))
        {
            sink_write_u32(snk,track->parser->dv_el_dsi_size + 8);
            if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "avcC"))
            {
                sink_write_4CC(snk,"avcE");
            }
            else
            {
                sink_write_4CC(snk,"hvcE");
            }
            snk->write(snk, (uint8_t *)track->parser->dv_el_dsi_buf, track->parser->dv_el_dsi_size);
            size += track->parser->dv_el_dsi_size + 8;

            /** clean up the el dsi buffer */
            if (track->parser->dv_el_dsi_buf)
            {
                FREE_CHK(track->parser->dv_el_dsi_buf);
                track->parser->dv_el_dsi_buf = NULL;
            }
        }
    }

    return size ;
}

static int32_t
write_dv_dsi_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t size = (uint32_t)track->parser->dv_dsi_size + 8;
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "dvcC");
    
    snk->write(snk, (uint8_t *)track->parser->dv_dsi_buf, track->parser->dv_dsi_size);
    
    return size ; 
}

static uint32_t
get_descriptor_size(uint32_t content_size)
{
    uint32_t u;

    if (content_size < 128)
    {
        /** the most likely case */
        return(1 + 1 + content_size);
    }

    for (u = 2; content_size >> (7 * u); u++);
    return u + 1 + content_size;
}

static void
write_descriptor_hdr(bbio_handle_t snk, int32_t tag, uint32_t content_size)
{
    uint32_t bytes_more = get_descriptor_size(content_size) - content_size - 2;

    sink_write_u8(snk, (uint8_t)tag);
    for (; bytes_more > 0; bytes_more--)
        sink_write_u8(snk, (uint8_t)((content_size >> (7 * bytes_more)) | 0x80));
    sink_write_u8(snk, (uint8_t)(content_size & 0x7F));
}

static offset_t
write_pasp_box(bbio_handle_t snk, track_handle_t track)
{
    parser_video_handle_t parser_video = (parser_video_handle_t)track->parser;
    /** pasp box hdr */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "pasp");

    sink_write_u32(snk, parser_video->hSpacing);
    sink_write_u32(snk, parser_video->vSpacing);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_colr_box(bbio_handle_t snk, track_handle_t track)
{
    parser_video_handle_t parser_video = (parser_video_handle_t)track->parser;
    /** color box hdr */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "colr");

    /**  color type */
    sink_write_4CC(snk, "nclc");

    sink_write_u16(snk, parser_video->colour_primaries);
    sink_write_u16(snk, parser_video->transfer_characteristics);
    sink_write_u16(snk, parser_video->matrix_coefficients);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_esds_box(bbio_handle_t snk, track_handle_t track)
{
    int32_t dsi_descriptor_size;

    /** esds box hdr */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "esds");
    sink_write_u32(snk, 0);

    dsi_descriptor_size = track->dsi_size ? get_descriptor_size(track->dsi_size) : 0;

    /** ES descriptor: ES_DescrTag 0x03  */
    write_descriptor_hdr(snk, ES_DescrTag, 3 + get_descriptor_size(13 + dsi_descriptor_size) + get_descriptor_size(1));

    /** the 3 bytes */
    sink_write_u16(snk, 0);                                    /** 0 for ES_ID */
    sink_write_u8(snk, 0x00);                                  /** flags  = 0 */

    /** decoder config descriptor : DecoderConfigDescrTag 0x04 */
    write_descriptor_hdr(snk, DecoderConfigDescrTag, 13 + dsi_descriptor_size);  /** the 13 + dsi_descriptor_size */

    /** 13 bytes */
    /** objectTypeIndication */
    sink_write_u8(snk, (uint8_t)get_objectTypeIndication(stream_id_objectTypeIndication_tbl, track->parser->stream_id));
    /** streamType: 6msb, upstream = 0, reserved = 1 lsb */
    if (track->parser->stream_type == STREAM_TYPE_AUDIO)
    {
        sink_write_u8(snk, (0x05 << 2) | 0x01);  /** audio */
    }
    else
    {
        sink_write_u8(snk, (0x04 << 2) | 0x01);  /** video */
    }
    /** buferSizeDB, min/max bitrate */
    sink_write_u8(snk, (uint8_t)(track->parser->buferSizeDB >> (3 + 16)));          /** >> 3 => bit to byte */
    sink_write_u16(snk, (track->parser->buferSizeDB >> 3) & 0xFFFF);
    sink_write_u32(snk, MAX2(track->parser->bit_rate, track->parser->maxBitrate));  /** maxBitrate */
    sink_write_u32(snk, track->parser->bit_rate);                                   /** avgBitrate */

    /** dsi: DecSpecificInfoTag 0x05 */
    if (track->dsi_size)
    {
        write_descriptor_hdr(snk, DecSpecificInfoTag, track->dsi_size);
        snk->write(snk, track->dsi_buf, track->dsi_size);
    }

    /** SLConfigDescriptor: 0x06 */
    write_descriptor_hdr(snk, SLConfigDescrTag, 1);
    sink_write_u8(snk, 0x02);                        /** 0x02: MP4 file */

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_video_box(bbio_handle_t snk, track_handle_t track)
{
    int8_t* codingname   = (track->encryptor) ? "encv" : track->codingname;
    parser_video_handle_t parser_video = (parser_video_handle_t)track->parser;
    int8_t compressor_name[32];
    int32_t dolby_vision_flag = 0;               

    /** Sample Entry */
    SKIP_SIZE_FIELD(snk);

    if (track->parser->ext_timing.ext_dv_profile == 5)
    {
        dolby_vision_flag = 1;
    }

    /** sample entry name */
    if(dolby_vision_flag && !track->encryptor)
    {
        if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "avcC"))
        {
            if(IS_FOURCC_EQUAL(codingname, "avc1"))
            {
                codingname = "dva1";
            } 
            else
            {
                codingname = "dvav";
            }
        }
        else if (IS_FOURCC_EQUAL(track->parser->dsi_FourCC, "hvcC"))
        {
            if(IS_FOURCC_EQUAL(codingname, "hev1"))
            {
                codingname = "dvhe";
            } 
            else if (IS_FOURCC_EQUAL(codingname, "hvc1"))
            {
                codingname = "dvh1";
            }
        }
        
        memcpy(track->codingname, codingname, 4);
    }
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)codingname, track->data_ref_index);


    /** VideoSampleEntry extension */
    sink_write_u16(snk, 0);           /** pre_defined */
    sink_write_u16(snk, 0);           /** reserved */
    sink_write_u32(snk, 0);           /** pre_defined */
    sink_write_u32(snk, 0);           /** pre_defined */
    sink_write_u32(snk, 0);           /** pre_defined */

    sink_write_u16(snk, (uint16_t)parser_video->width);
    sink_write_u16(snk, (uint16_t)parser_video->height);
    sink_write_u32(snk, 0x00480000);  /** horizontal resolution 72dpi */
    sink_write_u32(snk, 0x00480000);  /** vertical resolution 72dpi */
    sink_write_u32(snk, 0);           /** reserved */
    sink_write_u16(snk, 1);           /** frame_count = 0x1 */

    memset(compressor_name, 0, 32);

    if (track->codec_name[0] != '\0')
    {
        OSAL_STRNCPY(compressor_name, sizeof (compressor_name), track->codec_name, 32);
    }
    else if(parser_video->codec_name[0] != '\0')
    {
        memcpy(compressor_name, parser_video->codec_name, 32);
    }

    if(dolby_vision_flag)
    {
        memcpy(compressor_name, "\013DOVI Coding", 13);
    }
    
    snk->write(snk, (uint8_t *)compressor_name, 32);

    sink_write_u16(snk, 0x18);     /** depth = 0x18 */
    sink_write_u16(snk, 0xFFFF);   /** reserved */

    /** if both of hSpacing and vSpacing have valid value in ES */
    if ((parser_video->hSpacing != 0 ) && (parser_video->vSpacing != 0))
    {
        /** pixel aspect ratio box */
        write_pasp_box(snk, track);
    }

    /** stream specific extension */
    if (IS_FOURCC_EQUAL(track->codingname, "mp4v"))
    {
        /** ESDBox */
        write_esds_box(snk, track);
    }
    else if (track->dsi_size > 0)
    {
        write_dsi_box(snk, track);
    }

#ifdef ENABLE_MP4_ENCRYPTION
    if (track->encryptor)
    {
        write_sinf_box(snk, track);
    }
#endif

    WRITE_SIZE_FIELD_RETURN(snk);
}

/** Updates audio properties saved in track according to current dsi.
 * This has to happen differently for different codecs since they all have
 * their own specific dsi struct types.
 */
static void
update_audio_dsi(track_handle_t track)
{
    if (track->parser->stream_id == STREAM_ID_AAC)
    {
        parser_audio_handle_t parser  = (parser_audio_handle_t)track->parser;
        mp4_dsi_aac_handle_t  aac_dsi = (mp4_dsi_aac_handle_t)track->parser->curr_dsi;

        parser->bit_rate    = aac_dsi->esd.avgBitrate;
        parser->maxBitrate  = aac_dsi->esd.maxBitrate;
        parser->buferSizeDB = aac_dsi->esd.bufferSizeDB;
        parser->stream_id   = aac_dsi->stream_id;
        if (track->use_audio_channelcount)
        {
            track->audio_channel_count = aac_dsi->channel_count;
        }
    }
    else if (track->parser->stream_id == STREAM_ID_AC4)
    {
        parser_audio_handle_t parser  = (parser_audio_handle_t)track->parser;
        track->audio_channel_count = parser->channelcount;
    }
}

static offset_t
write_audio_box(bbio_handle_t snk, track_handle_t track)
{
    int8_t*    codingname = (track->encryptor) ? "enca" : track->codingname;
    uint32_t sample_rate;

    /** Sample Entry */
    SKIP_SIZE_FIELD(snk);
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)codingname, track->data_ref_index);

    update_audio_dsi(track);

    /** AudioSampleEntry extension */
    sink_write_u32(snk, 0);                                     /** reserved */
    sink_write_u32(snk, 0);                                     /** reserved */

    sink_write_u16(snk, (uint16_t)track->audio_channel_count);  /** channel count */
    sink_write_u16(snk, 16);                                    /** sample size   */
    sink_write_u16(snk, 0);                                     /** predefined    */

    sink_write_u16(snk, 0);                                     /** reserved */
    sample_rate = ((parser_audio_handle_t)(track->parser))->sample_rate;
    if (track->parser->stream_id != STREAM_ID_MLP)
    {
        if ((sample_rate >> 16) > 0)
        {
            sample_rate = 0;
        }
        sample_rate <<= 16;  /** 16.16 sample rate */
    }
    sink_write_u32(snk, sample_rate);

    /** stream specific extension */
    if (IS_FOURCC_EQUAL(track->codingname, "mp4a"))
    {
        /** ESDBox */
        write_esds_box(snk, track);
    }
    else if (track->dsi_size > 0)
    {
        write_dsi_box(snk, track);
    }

#ifdef ENABLE_MP4_ENCRYPTION
    if (track->encryptor)
    {
        write_sinf_box(snk, track);
    }
#endif

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_metadata_box(bbio_handle_t snk, track_handle_t track)
{
    parser_meta_handle_t parser_meta = (parser_meta_handle_t)track->parser;

    /** Sample Entry */
    SKIP_SIZE_FIELD(snk);
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)track->codingname, track->data_ref_index);

    /** MetaSampleEntry */
    snk->write(snk, (uint8_t *)parser_meta->content_encoding, strlen(parser_meta->content_encoding));
    sink_write_u8(snk, '\0');
    snk->write(snk, (uint8_t *)parser_meta->content_namespace, strlen(parser_meta->content_namespace));
    sink_write_u8(snk, '\0');
    snk->write(snk, (uint8_t *)parser_meta->schema_location, strlen(parser_meta->schema_location));
    sink_write_u8(snk, '\0');

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_ftab_box(bbio_handle_t snk, track_handle_t track)
{
    parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;
    uint32_t             nentries    = list_get_entry_num(parser_text->font_lst);
    it_list_handle_t     it;
    text_font_t *        font;

    /** Sample Entry */
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "ftab");

    sink_write_u16(snk, (uint16_t)nentries);
    it = it_create();
    it_init(it, parser_text->font_lst);
    while ((font = it_get_entry(it)))
    {
        uint8_t len = (uint8_t)strlen(font->font_name);
        sink_write_u16(snk, font->font_id);
        sink_write_u8(snk, len);
        snk->write(snk, (uint8_t *)font->font_name, len);
    }
    it_destroy(it);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_text_box(bbio_handle_t snk, track_handle_t track)
{
    parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;

    /** Sample Entry */
    SKIP_SIZE_FIELD(snk);
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)track->codingname, track->data_ref_index);

    /** TextSampleEntry */
    sink_write_u32(snk, parser_text->flags);
    sink_write_u8(snk, parser_text->horizontal_justification);
    sink_write_u8(snk, parser_text->vertical_justification);
    snk->write(snk, parser_text->bg_color, 4);

    /** BoxRecord */
    sink_write_u16(snk, parser_text->top);
    sink_write_u16(snk, parser_text->left);
    sink_write_u16(snk, parser_text->bottom);
    sink_write_u16(snk, parser_text->right);

    /** StyleRecord */
    sink_write_u16(snk, parser_text->start_char);
    sink_write_u16(snk, parser_text->end_char);
    sink_write_u16(snk, parser_text->font_id);
    sink_write_u8(snk, parser_text->font_flags);
    sink_write_u8(snk, parser_text->font_size);
    snk->write(snk, parser_text->fg_color, 4);

    write_ftab_box(snk, track);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_data_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)track->codingname, track->data_ref_index);

    /** stream specific extension */
    if (track->dsi_size > 0)
    {
        write_dsi_box(snk, track);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_tims_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "tims");
    sink_write_u32(snk, track->media_timescale); /** timescale */
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_tsro_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "tsro");
    sink_write_u32(snk, 0);  /** offset */
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_snro_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "snro");
    sink_write_u32(snk, 0);  /** offset */
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_rtp_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "rtp ");
    sink_write_u32(snk, 0);      /** reserved */
    sink_write_u16(snk, 0);      /** reserved */
    sink_write_u16(snk, 1);      /** dataReferenceIndex */
    sink_write_u16(snk, 1);      /** hintTrackVersion */
    sink_write_u16(snk, 1);      /** highestCompatibleVersion */
    sink_write_u32(snk, track->mp4_ctrl->usr_cfg_mux_ref->max_pdu_size);  /** maxPacketSize */

    write_tims_box(snk, track);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_subt_box(bbio_handle_t snk, track_handle_t track)
{
    parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;

    /** SampleEntry */
    SKIP_SIZE_FIELD(snk);
    MOV_WRITE_SAMPLE_ENTRY(snk, (uint8_t *)track->codingname, track->data_ref_index);

    /** SubtitleSampleEntry */
    snk->write(snk, (uint8_t *)parser_text->subt_namespace, strlen(parser_text->subt_namespace)+1);
    snk->write(snk, (uint8_t *)parser_text->subt_schema_location, strlen(parser_text->subt_schema_location)+1);
    snk->write(snk, (uint8_t *)parser_text->subt_image_mime_type, strlen(parser_text->subt_image_mime_type)+1);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_stsd_box(bbio_handle_t snk, track_handle_t track)
{
    it_list_handle_t  it = it_create();
    idx_ptr_t        *ip;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "stsd");

    sink_write_u32(snk, 0);      /** version & flags */

    /** entry count */
    sink_write_u32(snk, track->sample_descr_index);
    it_init(it, track->stsd_lst);
    /** write each entry already built */
    while ((ip = it_get_entry(it)))
    {
        snk->write(snk, ip->ptr, get_BE_u32(ip->ptr));
    }
    it_destroy(it);

    msglog(NULL, MSGLOG_INFO, "[stsd] entries %d\n", list_get_entry_num(track->stsd_lst));

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_stts_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t dts_entries, i;
    uint64_t dts0, dts1;
    uint32_t entry_count = 0, sample_count = 0, sample_delta_prev = 0, sample_delta;
    offset_t cur_pos;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "stts");
    sink_write_u32(snk, 0);     /** version & flags */

    sink_write_u32(snk, 0);     /** entry_count place holder. however, frag is 0 */

    /** initialize in case of only one sample */
    dts1 = track->media_duration;
    dts0 = 0;

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        it_list_handle_t it = it_create();

        dts_entries = list_get_entry_num(track->dts_lst);
        it_init(it, track->dts_lst);

        dts0 = ((idx_dts_t*)it_get_entry(it))->dts;       /** 1st dts */
        /** prepare the 1st stts entry */
        if (dts_entries > 1)
        {
            sample_count = 1;
            dts1 = ((idx_dts_t*)it_get_entry(it))->dts;   /** 2nd dts */
            sample_delta_prev = sample_delta = (uint32_t)(dts1 - dts0);
        }

        /** stts entry: for dts [2, dts_entries) */
        for (i = 2; i < dts_entries; i++)
        {
            idx_dts_t* temp_p = ((idx_dts_t*)it_get_entry(it));
            if (temp_p == NULL)
            {
                msglog(NULL, MSGLOG_ERR, "Missing entry");
                continue;
            }

            dts0 = dts1;
            dts1 = temp_p->dts;   /** 3rd dts and on */
            sample_delta = (uint32_t)(dts1 - dts0);
            if (sample_delta == sample_delta_prev)
            {
                sample_count++;
            }
            else
            {
                /** time to write previous entry */
                if (!entry_count)
                {
                    msglog(NULL, MSGLOG_INFO, "       delta dts changed %u => %u...\n",
                           sample_delta_prev, sample_delta);
                }
                sink_write_u32(snk, sample_count);
                sink_write_u32(snk, sample_delta_prev);
                entry_count++;

                /** and init a new entry */
                sample_count = 1;
                sample_delta_prev = sample_delta;
            }
        }
        it_destroy(it);

        if (sample_delta_prev != (track->media_duration - dts1) && ((int64_t)track->media_duration - (int64_t)dts1) >= 0)
        {
            uint32_t last_sample_delta;
            /** write out previous sample_delta_prev */
            sink_write_u32(snk, sample_count);
            sink_write_u32(snk, sample_delta_prev);
            /** write out last sample which is the remainder of the track */
            sink_write_u32(snk, 1);
            last_sample_delta = (uint32_t)(track->media_duration - dts1);
            sink_write_u32(snk, last_sample_delta);
            entry_count += 2;
        }
        else
        {
            /** sample_delta sames as previous so just
                add to previous sample_delta_prev */
            sample_count++;
            sink_write_u32(snk, sample_count);
            sink_write_u32(snk, sample_delta_prev);
            entry_count++;
        }

        msglog(NULL, MSGLOG_INFO, "[stts] entries %d\n", entry_count);
        msglog(NULL, MSGLOG_INFO, "       entry %u: sample_count %u, sample_delta %u\n",
               entry_count-1, sample_count, sample_delta_prev);

        cur_pos = snk->position(snk);
        snk->seek(snk, CURRENT_BOX_OFFSET()+12, SEEK_SET);
        sink_write_u32(snk, entry_count);
        snk->seek(snk, cur_pos, SEEK_SET);
    }
    else
    {
        msglog(NULL, MSGLOG_INFO, "[stts] entries 0\n");
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_ctts_box(bbio_handle_t snk, track_handle_t track)
{
    BOOL     is_v1 = (track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_CTTS_V1) != 0;
    uint32_t entries;
    uint32_t i, atom_size;

    it_list_handle_t it;
    count_value_t *  cv;

    /** hint tracks don't use ctts */
    if (track->parser && track->parser->stream_type == STREAM_TYPE_HINT)
    {
        return 0;
    }

    if (track->no_cts_offset)
    {
        msglog(NULL, MSGLOG_INFO, "[ctts] none\n");
        return 0; /** no ctts => DTS=CTS */
    }

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        entries = list_get_entry_num(track->cts_offset_lst);
        if (entries == 0)
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }

    atom_size = 16 + (entries * 8);
    sink_write_u32(snk, atom_size);
    sink_write_4CC(snk, "ctts");
    sink_write_u8(snk, is_v1 ? 1 : 0);  /** version */
    sink_write_bits(snk, 24, 0);        /** flags */
    sink_write_u32(snk, entries);
    msglog(NULL, MSGLOG_INFO, "[ctts] entries %d\n", entries);

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        it = it_create();
        it_init(it, track->cts_offset_lst);
        i = 0; /** i for test only */
        while ((cv = it_get_entry(it)))
        {
            sink_write_u32(snk, cv->count);
            sink_write_u32(snk, (uint32_t)cv->value);
            if (i < 2)
            {
                msglog(NULL, MSGLOG_INFO, "       entry %u: sample_count %u, sample_offset %u\n",
                    i, cv->count, (uint32_t)cv->value);
                i++;
            }
        }
        it_destroy(it);
    }

    return atom_size;
}

static offset_t
write_stss_box(bbio_handle_t snk, track_handle_t track)
{
    if (list_get_entry_num(track->sync_lst) == 0)
    {
        return 0;
    }
    if (!(track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_STSS))
    {
        msglog(NULL, MSGLOG_INFO, "[stss] skipped writing stss box\n");
        return 0;
    }

    if (!track->all_rap_samples)
    {
        uint32_t entry_count = list_get_entry_num(track->sync_lst);
        idx_dts_t *idx_dts;

        SKIP_SIZE_FIELD(snk);
        sink_write_4CC(snk, "stss");
        sink_write_u32(snk, 0);          /** version, flags */
        if (!(track->output_mode & EMA_MP4_FRAG))
        {
            it_list_handle_t it = it_create();

            sink_write_u32(snk, entry_count);
            it_init(it, track->sync_lst);
            while ((idx_dts = it_get_entry(it)))
            {
                sink_write_u32(snk, 1 + idx_dts->idx); /** +1 => start from 1*/
            }
            it_destroy(it);
        }
        else
        {
            entry_count = 0;
            sink_write_u32(snk, entry_count);
        }

        msglog(NULL, MSGLOG_INFO, "[stss] entries %d\n",entry_count);
        WRITE_SIZE_FIELD_RETURN(snk);
    }
    /** no stss => all frames are sync sample */
    msglog(NULL, MSGLOG_INFO, "[stss] none\n");
    return 0;
}

static offset_t
write_stsc_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t num, i;
    uint32_t entry_count = 0, sample_num = 0, sample_description_index = 0;
    offset_t cur_pos;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "stsc");
    sink_write_u32(snk, 0);      /** version & flags */

    sink_write_u32(snk, 0);      /** entry_count place holder */

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        it_list_handle_t it = it_create();

        num = list_get_entry_num(track->chunk_lst);
        it_init(it, track->chunk_lst);
        for (i = 0; i < num; i++)
        {
            chunk_handle_t chunk;

            chunk = (chunk_handle_t)it_get_entry(it);
            if (chunk == NULL)
            {
                msglog(NULL, MSGLOG_ERR, "Missing entry");
                continue;
            }

            if (sample_num != chunk->sample_num ||
                sample_description_index != chunk->sample_description_index)
            {
                entry_count++;
                sample_num = chunk->sample_num;
                sample_description_index = chunk->sample_description_index;

                sink_write_u32(snk, i + 1);  /** first chunk start from 1 */
                sink_write_u32(snk, sample_num);
                sink_write_u32(snk, sample_description_index);
            }
        }
        it_destroy(it);

        cur_pos = snk->position (snk);
        snk->seek(snk, CURRENT_BOX_OFFSET()+12, SEEK_SET);
        sink_write_u32(snk, entry_count);
        snk->seek(snk, cur_pos, SEEK_SET);
    }
    msglog(NULL, MSGLOG_INFO, "[stsc] entries %d\n",entry_count);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_stsz_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t       cnt = 0;
    count_value_t *cv;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "stsz");
    sink_write_u32(snk, 0);      /** version & flags */

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        if (track->all_same_size_samples)
        {
            /** same size case */
            cv = list_peek_first_entry(track->size_lst);
            sink_write_u32(snk, (uint32_t)cv->value);  /** sample_size */
            sink_write_u32(snk, cv->count);            /** sample_count */
            assert(track->sample_num == cv->count);
        }
        else
        {
            it_list_handle_t it = it_create();
            uint32_t u;

            sink_write_u32(snk, 0);                      /** sample_size */
            sink_write_u32(snk, track->sample_num);      /** sample_count */
            it_init(it, track->size_lst);
            while ((cv = it_get_entry(it)))
            {
                for (u = 0; u < cv->count; u++)
                {
                    sink_write_u32(snk, (uint32_t)cv->value); /** entry_size: the actual sample size */
                }
                cnt += cv->count;
            }
            it_destroy(it);
            assert(cnt == track->sample_num);
        }
    }
    else
    {
        sink_write_u32(snk, 0);  /** sample_size */
        sink_write_u32(snk, 0);  /** sample_count */
    }

    msglog(NULL, MSGLOG_INFO, "[stsz] entries %d\n",cnt);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_stco_box(bbio_handle_t snk, track_handle_t track)
{
    int8_t *tag;
    uint32_t num=0;
    chunk_handle_t chunk;

    SKIP_SIZE_FIELD(snk);
    tag = (track->mp4_ctrl->co64_mode) ? "co64" : "stco";
    sink_write_4CC(snk, tag);
    sink_write_u32(snk, 0);      /** version & flags */

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        it_list_handle_t it = it_create();

        num = list_get_entry_num(track->chunk_lst);
        sink_write_u32(snk, num);
        it_init(it, track->chunk_lst);
        while ((chunk = it_get_entry(it)))
        {
            if (track->mp4_ctrl->co64_mode)
                sink_write_u64(snk, chunk->offset);
            else
                sink_write_u32(snk, (uint32_t)chunk->offset);
        }
        it_destroy(it);
    }
    else
    {
        num = 0;
        sink_write_u32(snk, num);
    }
    msglog(NULL, MSGLOG_INFO, "[%s] entries %d\n", tag, num);
    track->stco_offset = CURRENT_BOX_OFFSET();

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_sbgp_box(bbio_handle_t snk, track_handle_t track)
{
    int8_t *tag = "roll";

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "sbgp");
    sink_write_u32(snk, 0);                 /** version & flags */

    sink_write_4CC(snk, tag);               /** grouping_type */
    sink_write_u32(snk, 1 );                /** entry_count */
    sink_write_u32(snk, track->sample_num); /** sample_count */
    sink_write_u32(snk, 0 );                /** group_description_index */
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_sgpd_box(bbio_handle_t snk)
{
    int8_t *tag = "roll";

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "sgpd");
    sink_write_u32(snk, 0);         /** version & flags */
    sink_write_4CC(snk, tag);       /** grouping_type */
    sink_write_u32(snk, 1);         /** entry count */
    sink_write_u16(snk, 0xffff);    /** roll_distance */

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_stbl_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "stbl");
    if (list_get_entry_num(track->chunk_lst) > 0)
    {
        write_stsd_box(snk, track);

        write_stts_box(snk, track);
        write_ctts_box(snk, track);
        write_stss_box(snk, track);
        write_stsc_box(snk, track);
        write_stsz_box(snk, track);
        write_stco_box(snk, track);

#ifdef ENABLE_MP4_ENCRYPTION
        if (track->encryptor && (track->output_mode & EMA_MP4_FRAG) == 0)
        {
            write_encryption_info_boxes(snk, track);
        }
#endif

        if (track->parser->stream_type == STREAM_TYPE_SUBTITLE && track->subs_present && (track->output_mode & EMA_MP4_FRAG) == 0)
        {
            write_subs_box(snk, track);
        }

        if (track->write_pre_roll)
        {
            write_sbgp_box(snk, track);
            write_sgpd_box(snk);
        }
    }
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_mdhd_box(bbio_handle_t snk, track_handle_t track)
{
    int32_t      version  = 0;
    uint32_t size     = 32;
    uint64_t duration = 0;

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        duration = track->media_duration;
    }

    if (duration > UINT32_MAX || track->modification_time > UINT32_MAX)
    {
        version = 1;
        size    = 44;
    }

    sink_write_u32(snk, size);
    sink_write_4CC(snk, "mdhd");
    sink_write_u8(snk, (uint8_t)version);
    sink_write_bits (snk, 24, 0); /** flags */
    if (version == 1)
    {
        sink_write_u64(snk, track->media_creation_time);
        sink_write_u64(snk, track->media_modification_time);
    }
    else
    {
        sink_write_u32(snk, (uint32_t) track->media_creation_time);
        sink_write_u32(snk, (uint32_t) track->media_modification_time);
    }

    sink_write_u32(snk, track->media_timescale);             /** time scale (sample rate for audio) */
    (version == 1) ? sink_write_u64(snk, duration) : sink_write_u32(snk, (uint32_t)duration);
    sink_write_u16(snk, (uint16_t)track->language_code);     /** language_code */
    sink_write_u16(snk, 0);                                  /** reserved (quality) */

    return size;
}

static offset_t
write_hdlr_box(bbio_handle_t snk, track_handle_t track)
{
    const int8_t *name, *handler_type;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "hdlr");
    sink_write_u32(snk, 0);      /** Version & flags */

    switch (track->parser->stream_type)
    {
    case STREAM_TYPE_VIDEO:
        handler_type = "vide";
        name         = "video handler";
        break;
    case STREAM_TYPE_AUDIO:
        handler_type = "soun";
        name         = "sound handler";
        break;
    case STREAM_TYPE_META:
        handler_type = "meta";
        name         = "meta handler";
        break;
    case STREAM_TYPE_TEXT:
        /** use handler_type in text handler as handler type
            could be text or sbtl. */
        {
            parser_text_handle_t text_parser = (parser_text_handle_t)track->parser;
            if (text_parser->handler_type != NULL)
            {
                handler_type = text_parser->handler_type;
            }
            else
            {
                handler_type = "text";
            }
        }
        name = "streaming text handler";
        break;
    case STREAM_TYPE_SUBTITLE:
        handler_type = "subt";
        name         = "subtitle handler";
        break;
    case STREAM_TYPE_DATA:
        assert(track->parser->stream_id==STREAM_ID_EMAJ);
        handler_type = "emaj";
        name         = "EMAJ handler";
        break;
    case STREAM_TYPE_HINT:
        handler_type = "hint";
        name         = "hint";
        break;
    default:
        msglog(NULL, MSGLOG_ERR, "mp4_muxer: ERR: unknown stream type - skip writing hdlr box\n");
        assert(0);
        snk->seek(snk, CURRENT_BOX_OFFSET(), SEEK_SET);
        return 0;
    }
    /** If the value for 'name' field is provided, use it instead of the above defaults. */
    if (track->hdlr_name)
        name = track->hdlr_name;

    sink_write_u32(snk, 0);      /** pre-defined */
    sink_write_4CC(snk, handler_type);
    sink_write_u32(snk, 0);      /** reserved */
    sink_write_u32(snk, 0);      /** reserved */
    sink_write_u32(snk, 0);      /** reserved */
    snk->write(snk, (uint8_t*)name, strlen(name));
    sink_write_u8(snk,0);        /** to terminate the name string */

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_minf_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "minf");
    if (track->parser->stream_type == STREAM_TYPE_VIDEO)
    {
        write_vmhd_box(snk);
    }
    else if (track->parser->stream_type == STREAM_TYPE_AUDIO)
    {
        write_smhd_box(snk);
    }
    else if (track->parser->stream_type == STREAM_TYPE_SUBTITLE)
    {
        /** [CFF] Section 2.2.10: (DECE) Subtitle Media Header Box */
        write_sthd_box(snk);
    }
    else if (track->parser->stream_type == STREAM_TYPE_HINT)
    {
        write_hmhd_box(snk, track);
    }
    else
    {
        write_nmhd_box(snk);
    }

    write_dinf_box(snk);
    write_stbl_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_mdia_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "mdia");
    write_mdhd_box(snk, track);
    write_hdlr_box(snk, track);
    write_minf_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_tkhd_box(bbio_handle_t snk, track_handle_t track)
{
    int32_t      version  = 0;
    uint32_t size     = 92;
    uint64_t duration = 0;

    if (!(track->output_mode & EMA_MP4_FRAG))
    {
        duration = track->sum_track_edits;
    }

    if (duration > UINT32_MAX || track->modification_time > UINT32_MAX)
    {
        version = 1;
        size    = 104;
    }

    sink_write_u32(snk, size);
    sink_write_4CC(snk, "tkhd");
    sink_write_u8(snk, (uint8_t)version);
    if (track->parser->stream_type == STREAM_TYPE_HINT)
    {
        sink_write_bits (snk, 24, 0x0);  /** hint track has the flags of 0 */
    }
    else
    {
        sink_write_bits (snk, 24, track->flags);   /** flags (track in preview, in presentation, enabled) */
    }
    if (version == 1)
    {
        sink_write_u64(snk, track->creation_time);
        sink_write_u64(snk, track->modification_time);
    }
    else
    {
        sink_write_u32(snk, (uint32_t) track->creation_time);
        sink_write_u32(snk, (uint32_t) track->modification_time);
    }
    sink_write_u32(snk, track->track_ID);
    sink_write_u32(snk, 0);                  /** reserved */
    (version == 1) ? sink_write_u64(snk, duration) : sink_write_u32(snk, (uint32_t)duration);

    sink_write_u32(snk, 0);                  /** reserved */
    sink_write_u32(snk, 0);                  /** reserved */
    if (track->parser->stream_type == STREAM_TYPE_SUBTITLE)
    {
        /** see [CFF] Section 6.6.1.1 */
        sink_write_u16(snk, (uint16_t)(-1)); /** Layer. (In front of video plane). */
    }
    else
    {
        sink_write_u16(snk, 0x0);            /** reserved (Layer) */    
    }
     /** Alternate group is an integer that specifies a group or collection of tracks. If this field is 0 there is no
        information on possible relations to other tracks. If this field is not 0, it should be the same for tracks
        that contain alternate data for one another and different for tracks belonging to different such groups.
        Only one track within an alternate group should be played or streamed at any one time, and must be
        distinguishable from other tracks in the group via attributes such as bitrate, codec, language, packet
        size etc. A group may have only one member.*/
    sink_write_u16(snk, (uint16_t)(track->alternate_group));      
    /** volume, only for audio */
    if (track->parser->stream_type == STREAM_TYPE_AUDIO)
    {
        sink_write_u16(snk, 0x0100);
    }
    else
    {
        sink_write_u16(snk, 0);
    }
    sink_write_u16(snk, 0);           /** reserved */

    /** Matrix structure */
    sink_write_u32(snk, 0x00010000);  /** reserved */
    sink_write_u32(snk, 0x0);         /** reserved */
    sink_write_u32(snk, 0x0);         /** reserved */
    sink_write_u32(snk, 0x0);         /** reserved */
    sink_write_u32(snk, 0x00010000);  /** reserved */
    sink_write_u32(snk, 0x0);         /** reserved */
    if (track->parser->stream_type == STREAM_TYPE_TEXT)
    {
        parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;
        sink_write_u32(snk, parser_text->translation_x << 16);    /** x translation */
        sink_write_u32(snk, parser_text->translation_y << 16);    /** y translation */
    }
    else
    {
        sink_write_u32(snk, 0x0);     /** reserved */
        sink_write_u32(snk, 0x0);     /** reserved */
    }
    sink_write_u32(snk, 0x40000000);  /** reserved */

    /** track width and height, for video, text only and subtitles. */
    if (track->parser->stream_type == STREAM_TYPE_VIDEO)
    {
        parser_video_handle_t parser_video = (parser_video_handle_t)track->parser;
        double sample_aspect_ratio = 0.0;

        if (parser_video->vSpacing)
        {
            sample_aspect_ratio = (double)parser_video->hSpacing/(double)parser_video->vSpacing;
        }
        if (sample_aspect_ratio < 0.1)
        {
            sample_aspect_ratio = 1;
        }
        sink_write_u32(snk, (uint32_t)((uint32_t)(sample_aspect_ratio * parser_video->width + 0.5) * 0x10000));
        sink_write_u32(snk, parser_video->height<<16);
    }
    else if (track->parser->stream_type == STREAM_TYPE_TEXT)
    {
        parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;
        /** width / height in fixed-point 16.16 */
        sink_write_u32(snk, (parser_text->right - parser_text->left) << 16);
        sink_write_u32(snk, (parser_text->bottom - parser_text->top) << 16);
    }
    else if (track->parser->stream_type == STREAM_TYPE_SUBTITLE)
    {
        /** [CFF] Section 6.6.1.1: width and height of video track */
        parser_text_handle_t parser_text = (parser_text_handle_t)track->parser;
        double sample_aspect_ratio = 0.0;

        if (parser_text->video_vSpacing)
        {
            sample_aspect_ratio = parser_text->video_hSpacing / (double)parser_text->video_vSpacing;
        }
        if (sample_aspect_ratio < 0.1)
        {
            sample_aspect_ratio = 1.0;
        }

        sink_write_u32(snk, (uint32_t)((uint32_t)(sample_aspect_ratio * parser_text->video_width + 0.5) * 0x10000));
        sink_write_u32(snk, parser_text->video_height << 16);
    }
    else
    {
        sink_write_u32(snk, 0);
        sink_write_u32(snk, 0);
    }
    return size;
}

static offset_t
write_hint_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "hint"); /** reference type */
    sink_write_u32(snk, ((parser_hint_handle_t)(track->parser))->ref_ID); /** track this track references */
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_vdep_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "vdep"); /** reference type */
    /** For DoVi, we assume BL track id = EL track id - 1  */
    sink_write_u32(snk, track->track_ID - 1); 
    WRITE_SIZE_FIELD_RETURN(snk);
}

/** Track reference container box ISO/IEC 14496-12:2008(E) 8.3.3*/
static offset_t
write_tref_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "tref");
    if (track->parser->stream_type == STREAM_TYPE_HINT)
        write_hint_box(snk, track);

    write_vdep_box(snk, track);

    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_sdp_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "sdp ");
    snk->write(snk, (uint8_t *)((parser_hint_handle_t)(track->parser))->trackSDP, ((parser_hint_handle_t)(track->parser))->trackSDPSize);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_hnti_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "hnti");
    if (track->parser->stream_type == STREAM_TYPE_HINT)
        write_sdp_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

/** udta box inside the track as apposed to on the file itself */
static offset_t
write_udta_track_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "udta");
    if (track->parser->stream_type == STREAM_TYPE_HINT)
        write_hnti_box(snk, track);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_trak_box(bbio_handle_t snk, track_handle_t track, uint32_t tref_flag, uint32_t tkhd_flag)
{
    SKIP_SIZE_FIELD(snk);
    tkhd_flag;
    sink_write_4CC(snk, "trak");
    write_tkhd_box(snk, track);
    if (track->parser->stream_type == STREAM_TYPE_HINT)
    {
        write_tref_box(snk, track);
        write_udta_track_box(snk, track);
    }
    if (tref_flag && (track->parser->dv_el_nal_flag == 0) 
        && (track->parser->dv_rpu_nal_flag == 1) 
        && (track->parser->ext_timing.ext_dv_profile != 5)
        && (track->parser->ext_timing.ext_dv_profile != 8)
        && (track->track_ID > 1))
    {
        write_tref_box(snk, track);
    }
    if (list_get_entry_num(track->edt_lst))
    {
        write_edts_box(snk, track);
    }
    write_mdia_box(snk, track);
    write_private_box(snk, track->mp4_ctrl, "trak", track->track_ID);
    WRITE_SIZE_FIELD_RETURN(snk);
}

static offset_t
write_udta_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    if (muxer->udta_child_atom_lst == NULL)
    {
        return snk->position(snk);
    }
    else
    {
        it_list_handle_t it;
        atom_data_handle_t atom;

        SKIP_SIZE_FIELD(snk);
        sink_write_4CC(snk, "udta");

        /** write user data box if any */
        it = it_create();
        it_init(it, muxer->udta_child_atom_lst);
        while ((atom = it_get_entry(it)))
        {
            snk->write(snk, (uint8_t *)atom->data, atom->size);
        }
        it_destroy(it);
        WRITE_SIZE_FIELD_RETURN(snk);
    }
}


/*** fragment */
static void
write_mehd_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    if (muxer->duration > (uint32_t)(-1))
    {
        sink_write_u32(snk, 12+8);
        sink_write_4CC(snk, "mehd");
        sink_write_u32(snk, 1<<24);    /** version, flag */
        sink_write_u64(snk, muxer->duration);
    }
    else
    {
        sink_write_u32(snk, 12+4);
        sink_write_4CC(snk, "mehd");
        sink_write_u32(snk, 0);        /** version, flag */
        sink_write_u32(snk, (uint32_t)muxer->duration);
    }
}

static void
trex_get_sample_flag(track_t *track)
{
    /** Based on stdp list, we creates default sample flag which is the most common flag in sdtp list */
    typedef struct value_frequent_
    {
        uint32_t value;
        uint32_t freq;
    }value_frequent;
    value_frequent *p_content = NULL;

    uint32_t sample_flag_val;
    uint32_t i;
    sample_sdtp_t *sdtp;
    uint32_t num = list_get_entry_num(track->sdtp_lst);
    list_handle_t value_freq_lst = list_create(sizeof(value_frequent));

    while (num--)
    {
        sdtp = list_it_get_entry(track->sdtp_lst);
        sample_flag_val = (((sdtp->is_leading & 0x3) << 26) | 
                          ((sdtp->sample_depends_on & 0x3) << 24) | 
                          ((sdtp->sample_is_depended_on & 0x3) << 22) | 
                          ((sdtp->sample_has_redundancy & 0x3) << 20) | 
                          ((sdtp->sample_is_non_sync_sample & 0x1)<<16));
        if(list_get_entry_num(value_freq_lst) == 0)
        {
            p_content = (value_frequent *)list_alloc_entry(value_freq_lst);
            p_content->value = sample_flag_val;
            p_content->freq = 1;
            list_add_entry(value_freq_lst, p_content);
            list_it_init(value_freq_lst);
        }
        else
        {
            for (i = 0; i < list_get_entry_num(value_freq_lst); i++)
            {
                p_content = (value_frequent *)list_it_get_entry(value_freq_lst);
                if(p_content->value == sample_flag_val)
                {
                    p_content->freq++;
                    break;
                }
                else if (i == list_get_entry_num(value_freq_lst) - 1)
                {
                    p_content = (value_frequent *)list_alloc_entry(value_freq_lst);
                    p_content->value = sample_flag_val;
                    p_content->freq = 1;
                    list_add_entry(value_freq_lst, p_content);
                    break;
                }
            }
            list_it_init(value_freq_lst);
        }
    }
    /** value/frequence list was created, now we try to find the max frequence value*/
    {
        uint32_t max_freq = 0;
        for (i = 0; i < list_get_entry_num(value_freq_lst); i++)
        {
            p_content = (value_frequent *)list_it_get_entry(value_freq_lst);
            if(p_content->freq > max_freq)
            {
                max_freq = p_content->freq;
                track->trex.default_sample_flags = p_content->value;
            }
        }
    }

    list_destroy(value_freq_lst);
    list_it_init(track->sdtp_lst);
}

static void
write_trex_box(bbio_handle_t snk, track_t *track)
{
    trex_t *ptrex = &(track->trex);

    uint32_t default_sample_duration = 0;
    uint32_t default_sample_size     = 0;
    uint32_t default_sample_flags    = 0;

    trex_get_sample_flag(track);

    sink_write_u32(snk, 32);      /** size */
    sink_write_4CC(snk, "trex");
    sink_write_u32(snk, 0);       /** version, flag */

    if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TREX))
    {
        default_sample_duration = ptrex->default_sample_duration;
        default_sample_size     = ptrex->default_sample_size;
        default_sample_flags    = ptrex->default_sample_flags;
    }

    sink_write_u32(snk, ptrex->track_ID);
    sink_write_u32(snk, ptrex->default_sample_description_index);
    sink_write_u32(snk, default_sample_duration);
    sink_write_u32(snk, default_sample_size);
    sink_write_u32(snk, default_sample_flags);
}

static int32_t
write_mvex_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint32_t track_idx;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "mvex");

    write_mehd_box(snk, muxer);

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        track_handle_t track = muxer->tracks[track_idx];
        if (track->sample_num)
        {
            msglog(NULL, MSGLOG_INFO, "trex for track %d\n", track->track_ID);
            write_trex_box(snk, track);
        }
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_mvex_box_per_track(bbio_handle_t snk, mp4_ctrl_handle_t muxer, uint32_t index)
{
    track_handle_t track = muxer->tracks[index];

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "mvex");

    write_mehd_box(snk, muxer);

    if (track->sample_num)
    {
        msglog(NULL, MSGLOG_INFO, "trex for track %d\n", track->track_ID);
        write_trex_box(snk, track);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

/** Assuming there is at least one continuous track within the traf range */
static BOOL
more_moof(mp4_ctrl_handle_t muxer)
{
    uint32_t track_idx;

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        if (list_it_peek_entry(muxer->tracks[track_idx]->dts_lst))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/** Returns (uint64_t)-1 for not found */
static uint64_t
get_dts_from_idx(track_handle_t track, uint32_t idx)
{
    it_list_handle_t it = it_create();
    idx_dts_t *      id;

    it_init(it, track->dts_lst);
    while ((id = it_get_entry(it)))
    {
        if (id->idx == idx)
        {
            it_destroy(it);
            return id->dts;
        }
    }
    it_destroy(it);

    return (uint64_t)-1;
}

/**
 * @brief gets dts_max limit imposed by new sample description
 * a new sample description within the fragment range shall start a new fragment
 *
 * @return dts_max limit or (uint64_t)-1 for no SD limit
 */
static uint64_t
get_dts_max_sd(track_handle_t track, uint32_t idx_start)
{
    uint64_t dts_max_sd = (uint64_t)-1;

    idx_ptr_t *new_stsd = list_it_peek_entry(track->stsd_lst);
    if (new_stsd && new_stsd->idx == idx_start)
    {
        /** the new sample description is valid for the opening sample */
        list_it_get_entry(track->stsd_lst);  /** skip stsd list entry */
        new_stsd = list_it_peek_entry(track->stsd_lst);
    }
    if (new_stsd)
    {
        /** potential opening of next moof / fragment */
        dts_max_sd = get_dts_from_idx(track, new_stsd->idx);
    }

    return dts_max_sd;
}

static int32_t
get_dts_new_sd(track_handle_t track, uint32_t idx_start)
{
    int32_t dts_max_sd = 0;

    idx_ptr_t *new_stsd = list_it_peek_entry(track->stsd_lst);
    if (new_stsd && new_stsd->idx == idx_start)
    {
        /** the new sample description is valid for the opening sample */
        list_it_get_entry(track->stsd_lst);  /** skip stsd list entry */
        new_stsd = list_it_peek_entry(track->stsd_lst);
        dts_max_sd = 1;
    }
    if (new_stsd)
    {
        /** potential opening of next moof / fragment */
        get_dts_from_idx(track, new_stsd->idx);
    }

    return dts_max_sd;
}

#ifdef DECE_FRAGFIX

/** prepares next track fragment
    determine number of samples and size
*/
static int32_t
prepare_traf (mp4_ctrl_handle_t muxer, track_handle_t track)
{
    uint32_t       idx_start, idx_stop;
    it_list_handle_t  it_size;
    count_value_t     *cv;
    uint64_t          size = 0;
    uint64_t          pos  = 0;
    frag_index_t *frag_index,*next_frag_index;

    muxer; /** avoid compiler warning */

    frag_index = list_it_get_entry(track->segment_lst);
    if (frag_index)
    {
        next_frag_index = list_it_peek_entry(track->segment_lst);
        track->frag_dts = get_dts_from_idx(track, frag_index->frag_end_idx);
        /** if this is last fragment, adjust it's dts by this track's media duration */
        if (!next_frag_index)
        {
            track->frag_dts = track->media_duration;
        }

        track->frag_duration = (uint32_t)(track->frag_dts - get_dts_from_idx(track, frag_index->frag_start_idx));
        idx_start = frag_index->frag_start_idx;
        idx_stop  = frag_index->frag_end_idx;
    }
    else
    {
        /** there is no fragment left in this track */
        return -1;
    }

    it_size = it_create();
    it_init(it_size, track->size_lst);

    /** calculate size of fragment */
    cv = (count_value_t*)it_get_entry(it_size);
    pos += (cv) ? cv->count : 0;
    while (cv && pos < idx_start)
    {
        cv = (count_value_t*)it_get_entry(it_size);
        pos += (cv)?cv->count:0;
    }
    size += (cv) ? ((pos-idx_start)*cv->value) : 0;
    while (cv && pos < idx_stop)
    {
        cv = (count_value_t*)it_get_entry(it_size);
        if (!cv)
        {
            break;
        }
        size += cv->count * cv->value;
        pos  += cv->count;
    }
    size -= (cv) ? ((pos-idx_stop)*cv->value) : 0;
    track->frag_size = size;
    it_destroy(it_size);

    track->traf_is_prepared = TRUE;

    return 0;
}

static uint32_t
get_moof_ccff(mp4_ctrl_handle_t muxer)
{
    uint64_t dts_us;
    uint32_t track_idx;
    uint32_t next_track_ID = 0;

    track_handle_t *trk_entry = NULL;

    if (!muxer->next_track_lst)
    {
        muxer->next_track_lst = list_create(sizeof(track_handle_t*));
    }

    /** check if there are tracks already queued up to be send */
    if (list_get_entry_num(muxer->next_track_lst))
    {
        list_it_init(muxer->next_track_lst);
        trk_entry = list_it_get_entry(muxer->next_track_lst);
        if (list_get_entry_num(muxer->next_track_lst) > 1)
        {
            /** if more than one, find the smallest mdat */
            track_handle_t *e;
            while ((e = list_it_get_entry(muxer->next_track_lst)))
            {
                if(IS_FOURCC_EQUAL(muxer->usr_cfg_mux_ref->major_brand, "ccff")) 
                {
                    if ((*e)->frag_size < (*trk_entry)->frag_size) 
                    {
                        trk_entry = e;
                    }
                }
                else
                {
                     if ((*e)->track_ID < (*trk_entry)->track_ID)
                    {
                        trk_entry = e;
                    }
                }
            }
        }
        next_track_ID = (*trk_entry)->track_ID;
        list_remove_entry(muxer->next_track_lst, trk_entry);
        list_free_entry(trk_entry);
        return next_track_ID;
    }

    /** find the track with the lowest DTS */
    dts_us = (uint64_t)-1;
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        const track_handle_t track = muxer->tracks[track_idx];
        if (list_it_peek_entry(track->dts_lst))
        {
            const uint64_t dts2_us = rescale_u64(track->frag_dts, 1000000, track->media_timescale);
            if (dts2_us < dts_us)
            {
                dts_us = dts2_us;
            }
        }
    }

    /** prepare all tracks with same next DTS */
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        const track_handle_t track   = muxer->tracks[track_idx];
        const uint64_t       dts2_us = rescale_u64(track->frag_dts, 1000000, track->media_timescale);
        if (dts2_us == dts_us)
        {
            if (!prepare_traf(muxer, muxer->tracks[track_idx]))
            {
                trk_entry = (track_handle_t*)list_alloc_entry(muxer->next_track_lst);
                (*trk_entry) = muxer->tracks[track_idx];
                list_add_entry(muxer->next_track_lst, trk_entry);
            }
        }
    }

    /** check if there are samples left in any track */
    if (!list_get_entry_num(muxer->next_track_lst))
    {
        return 0;
    }

    /** return next track fragment */
    return get_moof_ccff(muxer);
}

static uint32_t
get_moof_ccff_per_track(mp4_ctrl_handle_t muxer, uint32_t index)
{
    uint64_t dts_us;
    uint32_t next_track_ID = 0;

    track_handle_t *trk_entry = NULL;

    if (!muxer->next_track_lst)
    {
        muxer->next_track_lst = list_create(sizeof(track_handle_t*));
    }

    /** check if there are tracks already queued up to be sent */
    if (list_get_entry_num(muxer->next_track_lst))
    {
        list_it_init(muxer->next_track_lst);
        trk_entry = list_it_get_entry(muxer->next_track_lst);
        if (list_get_entry_num(muxer->next_track_lst) > 1)
        {
            /** if more than one, find the smallest mdat */
            track_handle_t *e;
            while ((e = list_it_get_entry(muxer->next_track_lst)))
            {
                if(IS_FOURCC_EQUAL(muxer->usr_cfg_mux_ref->major_brand, "ccff")) 
                {
                    if ((*e)->frag_size < (*trk_entry)->frag_size) 
                    {
                        trk_entry = e;
                    }
                }
                else
                {
                     if ((*e)->track_ID < (*trk_entry)->track_ID)
                    {
                        trk_entry = e;
                    }
                }
            }
        }
        next_track_ID = (*trk_entry)->track_ID;
        list_remove_entry(muxer->next_track_lst, trk_entry);
        list_free_entry(trk_entry);
        return next_track_ID;
    }

    /** find the track with the lowest DTS */
    dts_us = (uint64_t)-1;
    {
        const track_handle_t track = muxer->tracks[index];
        if (list_it_peek_entry(track->dts_lst))
        {
            const uint64_t dts2_us = rescale_u64(track->frag_dts, 1000000, track->media_timescale);
            if (dts2_us < dts_us)
            {
                dts_us = dts2_us;
            }
        }
    }

    /** prepare all tracks with same next DTS */
    {
        const track_handle_t track   = muxer->tracks[index];
        const uint64_t       dts2_us = rescale_u64(track->frag_dts, 1000000, track->media_timescale);
        if (dts2_us == dts_us)
        {
            if (!prepare_traf(muxer, muxer->tracks[index]))
            {
                trk_entry = (track_handle_t*)list_alloc_entry(muxer->next_track_lst);
                (*trk_entry) = muxer->tracks[index];
                list_add_entry(muxer->next_track_lst, trk_entry);
            }
        }
    }

    /** check if there are samples left in any track */
    if (!list_get_entry_num(muxer->next_track_lst))
    {
        return 0;
    }

    /** return next track fragment */
    return get_moof_ccff_per_track(muxer, index);
}

#endif  /** DECE_FRAGFIX */

/** returns (uint32_t)-1 for not founding */
static uint32_t
track_ID_2_track_idx(mp4_ctrl_handle_t muxer, uint32_t track_ID)
{
    uint32_t track_idx;

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        if (muxer->tracks[track_idx]->track_ID == track_ID)
        {
            return track_idx;
        }
    }

    return (uint32_t)-1;
}

/** criterias for fragment are max fragment range, sample description change and rap
 *
 * frag_ctrl_track_ID is the dominate track
 *
 * get closing dts(exclusive, in ms) of moof
 * limit dts if sample description changes
 * If frag_ctrl_track_ID is set up,
 *   dts will align with rap when the rap distance does not exceed max range.
 * If frag_ctrl_track_ID(==0) is not setup,
 *   all samples in all track are rap, moof length with be max range.
 */
static BOOL
get_moof(mp4_ctrl_handle_t muxer)
{
    track_handle_t  track;
    frag_index_t *frag_index;
    uint32_t index;
    BOOL frag_flag = FALSE;

    for (index = 0; index < muxer->stream_num; index++)
    {
        track    = muxer->tracks[index];
        frag_index = list_it_get_entry(track->segment_lst);
        if (frag_index)
        {
            track->frag_dts = get_dts_from_idx(track, frag_index->frag_end_idx);
            if (frag_index->frag_end_idx == (list_get_entry_num(track->dts_lst)))
            {
                track->frag_dts = get_dts_from_idx(track, frag_index->frag_end_idx - 1);
                track->frag_dts += get_dts_from_idx(track, 1);
            }

            track->frag_duration = (uint32_t)(track->frag_dts - get_dts_from_idx(track, frag_index->frag_start_idx));

            frag_flag = TRUE;
        }
    }

    return frag_flag;
}

static BOOL
get_moof_by_TrackIndex(mp4_ctrl_handle_t muxer, uint32_t index)
{
    track_handle_t  track;
    frag_index_t *frag_index;
    uint32_t frag_flag = 0;

    track    = muxer->tracks[index];
    frag_index = list_it_get_entry(track->segment_lst);
    if (frag_index)
    {
        track->frag_dts = get_dts_from_idx(track, frag_index->frag_end_idx);
        if (frag_index->frag_end_idx == (list_get_entry_num(track->dts_lst)))
        {
            track->frag_dts = get_dts_from_idx(track, frag_index->frag_end_idx - 1);
            track->frag_dts += get_dts_from_idx(track, 1);
        }

        track->frag_duration = (uint32_t)(track->frag_dts - get_dts_from_idx(track, frag_index->frag_start_idx));

        frag_flag = 1;
    }

    if (frag_flag)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }

}

static void
update_frag_index_lst(list_handle_t lst,
                uint32_t       frag_start_idx,
                uint32_t       frag_end_idx)
{
    frag_index_t *frag_index = (frag_index_t *)list_alloc_entry(lst);

    frag_index->frag_start_idx         = frag_start_idx;
    frag_index->frag_end_idx           = frag_end_idx;
    list_add_entry(lst, frag_index);

    if (list_get_entry_num(lst) == 1)
    {
        list_it_init(lst);
    }
}

/** Creates fragment list based on: sync list and multiple stsd box info.
 *  Fragment duration will not exceed  'usr_cfg_mux_ref->frag_range_max'
 *  For the input ES, we have the following assumptions:
 *  1) The first sample in a fragment must be sync sample
 *  2) The first sample referenced by stsd must be sync sample
 * 
 *  After calling this function successfully, we can get the fragment 
 *  number and each fragment's start/end sample index
 *
 */

static int32_t
create_fragment_lst(mp4_ctrl_handle_t muxer, uint32_t first_sample_is_sync)
{
    uint64_t frag_range_max_s;
    uint64_t frag_range_min_s;

    track_handle_t  track;
    idx_dts_t        *dts_id, *dts_id_1st;
    uint32_t          idx_start, idx_stop;
    uint64_t          dts;

    uint32_t          track_idx;
    uint32_t          one_sample_per_frag;
    uint32_t          stop_sample_is_sync_flag = 0;
    uint64_t          dts_max, dts_min;
    uint64_t          frag_dts, frag_duration;
    /** if the frist sample of the seg must be sync sample, we reset the min duration to a very little value (10 ms). */
    if (first_sample_is_sync)
    {
        muxer->usr_cfg_mux_ref->frag_range_min = 10;
    }

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        track = muxer->tracks[track_idx];    
        frag_range_max_s    = rescale_u64(muxer->usr_cfg_mux_ref->frag_range_max, track->media_timescale, 1000);
        frag_range_min_s    = rescale_u64(muxer->usr_cfg_mux_ref->frag_range_min, track->media_timescale, 1000);
        one_sample_per_frag = (IS_FOURCC_EQUAL(track->codingname, "stpp")) ? 1 : 0;
        frag_dts            = track->frag_dts;
        frag_duration       = track->frag_duration;
        
        if ((frag_range_max_s <= frag_range_min_s) || (frag_range_max_s <= 0))
        {
            msglog(NULL, MSGLOG_ERR, "\nError: max/min fragment duration setting error! \n");
            return -1;
        }
        
        while(frag_dts < track->media_duration)
        {
            /** initialize */
            dts                      = 0;
            dts_max                  = frag_dts + frag_range_max_s;
            dts_min                  = frag_dts + frag_range_min_s;
            stop_sample_is_sync_flag = 0;

            /** get first sample */
            dts_id_1st = (idx_dts_t*)list_it_peek_entry(track->dts_lst);
            if (!dts_id_1st)
            {
                /** no sample left in this track*/
                break; 
            }

            list_it_save_mark(track->dts_lst);
            idx_start = dts_id_1st->idx;
            idx_stop  = idx_start + 1;
            /** to check if there have 2 samples left in the track */
            dts_id    = list_it_peek2_entry(track->dts_lst);
            if (dts_id)
            {
                dts = dts_id->dts;
            }
            else
            {
                dts_id = list_peek_first_entry(track->dts_lst);
                if (dts_id)
                {
                    dts = track->media_duration + dts_id->dts;
                }
            }

            if (!one_sample_per_frag)
            {
                uint64_t dts_max_sd = get_dts_max_sd(track, idx_start);

                /** limit dts_max to limit imposed by new sample description */
                if (dts_max > dts_max_sd)
                {
                    dts_max = dts_max_sd;
                }

                /** potentially add more samples to fill the fragment */
                if (!track->all_rap_samples)
                {
                    /** check if first sample is sync */
                    dts_id = list_it_peek_entry(track->sync_lst);
                    if (!dts_id || dts_id->idx != idx_start)
                    {
                        track->warn_flags |= EMAMP4_WARNFLAG_FRAG_NO_SYNC;
                        /** if we require fragment at sync sample */
                        if (first_sample_is_sync)
                        {
                            msglog(NULL, MSGLOG_ERR, "\nError: rap distance larger than max fragment duration \n");
                            return -1;
                        }
                    }
                    /** try to start fragments on sync samples */
                    list_it_save_mark(track->sync_lst);
                    while ((dts_id = (idx_dts_t*)list_it_get_entry(track->sync_lst)) && dts_id->dts <= dts_max)
                    {
                        if (dts_id->idx > idx_stop)
                        {
                            idx_stop = dts_id->idx;
                            dts      = dts_id->dts;
                            stop_sample_is_sync_flag = 1;
                        }
                    }
                    list_it_goto_mark(track->sync_lst);
                }
                
                if (dts_id == NULL)
                {
                    if (track->media_duration < dts_max)
                    {
                        idx_stop = list_get_entry_num(track->dts_lst);
                        dts = track->media_duration;
                    }
                }

                if ((dts <= dts_min) || (!stop_sample_is_sync_flag))
                {
                    /** if all samples are sync samples or if there are no sync samples in range,
                       fill up with normal samples */
                    dts_id = (idx_dts_t*)list_it_get_entry(track->dts_lst);
                    while (dts_id && dts_id->dts <= dts_max)
                    {
                        if ((dts_id->idx > idx_stop) || (dts > dts_max))
                        {
                            idx_stop = dts_id->idx;
                            dts      = dts_id->dts;
                        }
                        dts_id = (idx_dts_t*)list_it_get_entry(track->dts_lst);
                    }
                    if (dts_id == NULL)
                    {
                        if (track->media_duration <= dts_max)
                        {
                            idx_stop = list_get_entry_num(track->dts_lst);
                            dts = track->media_duration;
                        }
                    }
                    if (!dts_id && track->media_duration <= dts_max)
                    {
                        dts_id = (idx_dts_t*)list_peek_first_entry(track->dts_lst);
                        dts    = track->media_duration + ((dts_id)?dts_id->dts:0);
                    }
                }
            }

            list_it_goto_mark(track->dts_lst);

            dts_id = (idx_dts_t*)list_it_peek_entry(track->dts_lst);
            while(dts_id && dts_id->idx <(idx_stop))
            {
                list_it_get_entry(track->dts_lst);
                dts_id = list_it_peek_entry(track->dts_lst);
            }

            dts_id = (idx_dts_t*)list_it_peek_entry(track->sync_lst);
            while(dts_id && dts_id->idx <(idx_stop))
            {
                list_it_get_entry(track->sync_lst);
                dts_id = list_it_peek_entry(track->sync_lst);
                if(!dts_id)
                    break;
            }

            /** add fragment's start/stop sample index to list*/
            update_frag_index_lst(track->segment_lst, idx_start, idx_stop);
            frag_dts      = dts;
        }
        
        track->sidx_reference_count = (uint16_t)list_get_entry_num(track->segment_lst);
        /** restore the dts and sync list */
        list_it_init(track->dts_lst);
        list_it_init(track->sync_lst);

        track->traf_is_prepared = TRUE;
    }

    return 0;
}

/** gets the smallest idx with dts No Less Than dts. if return == list_size: no such entry */
static uint32_t
get_min_sample_idx_nlt_dts(list_handle_t dts_lst, uint64_t dts)
{
    idx_dts_t *idx_dts;

    list_it_save_mark(dts_lst);
    while ( (idx_dts = list_it_get_entry(dts_lst)) && idx_dts->dts < dts );
    list_it_goto_mark(dts_lst);

    return (idx_dts) ? idx_dts->idx : list_get_entry_num(dts_lst);
}

/** fills in 'tfhd' for now only one traf per trak
 *  since no change in sample_description_index */
static BOOL
get_tfhd(track_handle_t track)
{
    idx_dts_t *dts_id;
    trex_t    *ptrex = &(track->trex);
    tfhd_t    *ptfhd = &(track->tfhd);
    uint32_t  idx_1st, idx_max;
    uint32_t  sample_count;

    dts_id = (idx_dts_t*)list_it_peek_entry(track->dts_lst);
    if (!dts_id || dts_id->dts >= track->frag_dts)
    {
        /** no sample in the dts range */
        return FALSE;
    }

    idx_1st = dts_id->idx; /** first sample idx in trun */
    idx_max = get_min_sample_idx_nlt_dts(track->dts_lst, track->frag_dts);
    sample_count = idx_max - idx_1st;
    
    /** build tf_flags and tfhd */
    ptfhd->tf_flags = ptfhd->tf_flags_override;
    if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TREX)
    {
        ptfhd->tf_flags |= TF_FLAGS_DEFAULT_SAMPLE_FLAGS;

        ptfhd->default_sample_flags = 0;
        if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TFHD))
        {
            ptfhd->tf_flags |= TF_FLAGS_DEFAULT_SAMPLE_DURATION | TF_FLAGS_DEFAULT_SAMPLE_SIZE;
            ptfhd->default_sample_duration = ptrex->default_sample_duration;
            ptfhd->default_sample_size     = ptrex->default_sample_size;
            if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FORCE_TFHD_SAMPDESCIDX)
                ptfhd->tf_flags |= TF_FLAGS_SAMPLE_DESCRIPTION_INDEX;
        }
        else
        {
            ptfhd->tf_flags &= ~TF_FLAGS_DEFAULT_SAMPLE_FLAGS;
        }
    }

    /** for each segment, check the mode of the samples  */
    if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TREX) && (list_get_entry_num(track->sdtp_lst)))
    {
        sample_sdtp_t *sdtp;
        uint32_t sdtp_first_val = (uint32_t)-1;
        uint32_t sdtp_cur_val = (uint32_t)-1;
        uint32_t sdtp_last_val = (uint32_t)-1;
        uint32_t sample_num = sample_count - 1;

        if(IS_FOURCC_EQUAL(track->codingname, "ac-4"))
        {
            ptfhd->tf_flags |= TF_FLAGS_DEFAULT_SAMPLE_DURATION;
            ptfhd->default_sample_duration = ptrex->default_sample_duration;
        }
        
        ptfhd->samples_same_mode = SAMPLE_FLAG_IS_DIFFERENT;
        /** save the current list item */
        list_it_save_mark(track->sdtp_lst);
        /** store the first sample flag in the fragment. */
        sdtp = list_it_get_entry(track->sdtp_lst);
        sdtp_first_val =    (((sdtp->is_leading & 0x3) << 26) | 
                            ((sdtp->sample_depends_on & 0x3) << 24) | 
                            ((sdtp->sample_is_depended_on & 0x3) << 22) | 
                            ((sdtp->sample_has_redundancy & 0x3) << 20) | 
                            ((sdtp->sample_is_non_sync_sample & 0x1)<<16));
        /** check the samples(except 1st sample) has the same mode or not */
        while(sample_num--) 
        {
            sdtp = list_it_get_entry(track->sdtp_lst);
            sdtp_cur_val = (((sdtp->is_leading & 0x3) << 26) | 
                            ((sdtp->sample_depends_on & 0x3) << 24) | 
                            ((sdtp->sample_is_depended_on & 0x3) << 22) | 
                            ((sdtp->sample_has_redundancy & 0x3) << 20) | 
                            ((sdtp->sample_is_non_sync_sample & 0x1)<<16));
            if ((sdtp_cur_val != sdtp_last_val) && (sdtp_last_val != -1))
            {
                ptfhd->samples_same_mode = SAMPLE_FLAG_IS_DIFFERENT;
                break;
            }

            sdtp_last_val = sdtp_cur_val;
        }

        if (sample_num == -1)
        {
            ptfhd->samples_same_mode = SAMPLE_FLAG_IS_SAME_EXCEPT_FIRST;
        }
        /** check if all samples in this fragment have the same flag */
        if((sdtp_first_val == sdtp_cur_val)&&(ptfhd->samples_same_mode == SAMPLE_FLAG_IS_SAME_EXCEPT_FIRST))
        {
            ptfhd->samples_same_mode = SAMPLE_FLAG_IS_SAME;
        }
        /** restore list item */
        list_it_goto_mark(track->sdtp_lst);

        /** if the flags don't match trex's flags, then set this flag in tfhd, it will over-ride trex's flag */
        if ((ptfhd->samples_same_mode != SAMPLE_FLAG_IS_DIFFERENT) && (sdtp_cur_val != track->trex.default_sample_flags) )
        {
            /** if permit to create tfhd */
            if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TFHD))
            {
                ptfhd->tf_flags |= TF_FLAGS_DEFAULT_SAMPLE_FLAGS;
                ptfhd->default_sample_flags = sdtp_cur_val;
            }
            /** if don't permit create tfhd, we must set each sample's flag in 'trun'*/
            else
            {
                ptfhd->samples_same_mode = SAMPLE_FLAG_IS_DIFFERENT;
            }
        }
    }

    /** base-data-offset may be forbidden in application standards, e.g. DECE */
    if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_NO_BDO_IN_TFHD)
    {
        if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_DEFAULT_BASE_IS_MOOF)
        {
           ptfhd->tf_flags |= TF_FLAGS_DEFAULT_BASE_IS_MOOF;
        }
    }
    else
    {
        ptfhd->tf_flags |= TF_FLAGS_BASE_DATA_OFFSET; /** value will be updated later */
    }

    track->traf_is_prepared = TRUE;

    /** only one sample_description_index supported so far */
    track->first_trun_in_traf = TRUE; /** expecting first trun in traf */
    return TRUE;
}

/** Examines the lst to build trun: for now
*   (1) rap always start a run if not all sample is a rap
*   (2) duration normally does not change, so if does change, list all duration
*   (3) we either have fixed size or changed size at random, so if size changes, list all size
*   (4) for the supported ES, since either the parser does not suppurt or it is fixed, we can
*      handle the flags like this:
*      (a) if all au is rap. i.e. aac, dd, emaj: all has the same flags
*      (b) else,  i.e. h264: flags for two catergories only:  rap and !rap
*/
static BOOL
get_trun(track_handle_t track)
{
#define CONTINUOUS_TRUN     1 /** we always have continuous trun in traf */

    trun_t        *ptrun = &(track->trun);
    idx_dts_t     *dts_id, *dts2_id, *sync_id;
    uint32_t       sample_count, dval, idx_1st, idx_max;
    count_value_t *cv;

    dts_id = list_it_peek_entry(track->dts_lst);
    if (!dts_id || dts_id->dts >= track->frag_dts)
    {
        return FALSE;
    }

    /** build tr_flags and tfhd */
    ptrun->tr_flags = ptrun->tr_flags_override;
    if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_NO_BDO_IN_TFHD)
    {
        ptrun->tr_flags |= TR_FLAGS_DATA_OFFSET;
    }

    if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_EMPTY_TFHD)
    {
        ptrun->tr_flags |= TR_FLAGS_SAMPLE_DURATION | TR_FLAGS_SAMPLE_SIZE;
    }

    idx_1st = dts_id->idx; /** first sample idx in trun */
    assert(idx_1st < track->sample_num);
    idx_max = get_min_sample_idx_nlt_dts(track->dts_lst, track->frag_dts); /** maximum idx in trun */
    assert(idx_max >= idx_1st);

    if (track->traf_is_prepared)
    {
        sample_count = idx_max - idx_1st;
        if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_SDTP) && 
            list_get_entry_num(track->sdtp_lst)) 
        {
            /** If the samples except the first one have the same mode in the fragment, and the first sample's flag doesn't equal the followings' flag */
            if(track->tfhd.samples_same_mode == SAMPLE_FLAG_IS_SAME_EXCEPT_FIRST)
            {
                const uint32_t clear_tr_sample_flags = TR_FLAGS_SAMPLE_FLAGS;
                ptrun->tr_flags |= TR_FLAGS_FIRST_FLAGS;
                ptrun->tr_flags &= (~clear_tr_sample_flags);       /** Required by ISO/IEC 14496-12, 8.8.8.1 */
            }
            else if(track->tfhd.samples_same_mode == SAMPLE_FLAG_IS_DIFFERENT)
            {
                const uint32_t clear_tr_first_sample_flags = TR_FLAGS_FIRST_FLAGS;
                ptrun->tr_flags |= TR_FLAGS_SAMPLE_FLAGS;
                ptrun->tr_flags &= (~clear_tr_first_sample_flags); /** Required by ISO/IEC 14496-12, 8.8.8.1 */
            }
        }
    }
    else if (track->all_rap_samples || !(sync_id = list_it_peek_entry(track->sync_lst)))
    {
        /** all samples are rap or no more rap:  one trun and [rap, rap] or [*, !rap] */
        /** sample_count */
        sample_count = idx_max - idx_1st;

        /** no data_offset: the 1st and the only trun in traf */
        /** no first_sample_flags: nothing special for 1st sample */
    }
    else if (sync_id->idx == idx_1st)
    {
        /** [rap, ... case */
        idx_dts_t *sync2_id;

        /** sample_count */
        sync2_id = list_it_peek2_entry(track->sync_lst);
        if (sync2_id && sync2_id->dts <= track->frag_dts)
        {
            /** this trun [rap, rap) */
            assert(sync2_id->idx <= idx_max);
            sample_count = sync2_id->idx - idx_1st;
        }
        else
        {
            /** this trun [rap, frag_dts) */
            sample_count = idx_max - idx_1st;
        }

        /** data_offset */
#if !CONTINUOUS_TRUN
        if (!track->first_trun_in_traf)
        {
            ptrun->tr_flags |= TR_FLAGS_DATA_OFFSET;
            /** data_offset should be updated when finish last trun */
        }
#endif

        /** first_sample_flags: must got prediction. need to setup rap sample */
        ptrun->tr_flags |= TR_FLAGS_FIRST_FLAGS;
        ptrun->first_sample_flags = SAMPLE_FLAGS_RAP;
    }
    else
    {
        /** [!rap, ... case */

        /** sample_count */
        if (sync_id->dts <= track->frag_dts)
        {
            /** this trun [!rap, rap:sync_id) */
            assert(sync_id->idx > idx_1st);
            sample_count = sync_id->idx - idx_1st;
        }
        else
        {
            /** this trun [!rap, !rap) */
            sample_count = idx_max - idx_1st;
        }

        /** data_offset */
#if !CONTINUOUS_TRUN
        if (!track->first_trun_in_traf)
        {
            ptrun->tr_flags |= TR_FLAGS_DATA_OFFSET;
            /** data_offset should be updated when finish last trun */
        }
#endif

        /** no first_sample_flags */
    }

    track->traf_is_prepared = FALSE;
    track->tfhd.sample_num += sample_count;
    ptrun->sample_count = sample_count;

    /** duration: actual work on sample_count+1 samples */
    dval = track->tfhd.default_sample_duration;
    list_it_save_mark(track->dts_lst);
    dts2_id = list_it_get_entry(track->dts_lst);
    while (sample_count--)
    {
        dts_id  = dts2_id;
        dts2_id = list_it_get_entry(track->dts_lst);
        if (dts2_id && (uint32_t)(dts2_id->dts - dts_id->dts) != dval)
        {
            ptrun->tr_flags |= TR_FLAGS_SAMPLE_DURATION; /** duration change within trun */
            break;
        }
    }
    list_it_goto_mark(track->dts_lst);

    /** size */
    dval = track->tfhd.default_sample_size;
    cv   = list_it_peek_entry(track->size_lst);
    if (cv->value != dval || track->size_cnt < ptrun->sample_count)
    {
        ptrun->tr_flags |= TR_FLAGS_SAMPLE_SIZE; /** size change within trun */
    }

    /** flags: we either have all default value or only the first is differnet:  already handled */

    /** cts_offset */
    if (!track->no_cts_offset)
    {
        ptrun->tr_flags |= TR_FLAGS_CTS_OFFSETS;
    }

    if (((track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FRAGSTYLE_MASK) == ISOM_FRAGCFG_FRAGSTYLE_CCFF))
    {
        /** [CFF v1.0.7] Section 6.7.1.7 */
        if (track->parser->stream_type == STREAM_TYPE_SUBTITLE || track->parser->stream_type == STREAM_TYPE_VIDEO)
        {
            ptrun->tr_flags |= TR_FLAGS_SAMPLE_DURATION | TR_FLAGS_SAMPLE_SIZE | TR_FLAGS_DATA_OFFSET;
        }

    }

    return TRUE;
}

static int32_t
write_tfhd_box(bbio_handle_t snk, track_handle_t track)
{
    tfhd_t   *ptfhd    = &(track->tfhd);
    uint32_t  size     = 4*4;              /** size, tag, flags, track_ID, base_data_offset */
    uint32_t  tf_flags = ptfhd->tf_flags;

    if (tf_flags & TF_OPTIONAL_FIELDS)
    {
        /** get more */
        if (tf_flags & TF_FLAGS_BASE_DATA_OFFSET)
        {
            size += 8;
        }
        if (tf_flags & TF_FLAGS_SAMPLE_DESCRIPTION_INDEX)
        {
            size += 4;
        }
        if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_DURATION)
        {
            size += 4;
        }
        if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_SIZE)
        {
            size += 4;
        }
        if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_FLAGS)
        {
            size += 4;
        }
    }

    sink_write_u32(snk, size);
    sink_write_4CC(snk, "tfhd");
    sink_write_u32(snk, tf_flags);

    msglog(NULL, MSGLOG_DEBUG, "    tfhd(traf idx %u)\n", track->mp4_ctrl->traf_idx);
    msglog(NULL, MSGLOG_DEBUG, "      tf_flags %#x, track_ID %u\n", tf_flags, ptfhd->track_ID);

    sink_write_u32(snk, ptfhd->track_ID);

    ptfhd->base_data_offset_pos = 0;
    if (tf_flags & TF_FLAGS_BASE_DATA_OFFSET)
    {
        /** since mdat come after moof, which is not known yet,
         * use it to save position for modification */
        ptfhd->base_data_offset_pos = snk->position(snk);
        ptfhd->base_data_offset     = 0;                  /** reference is first data in mdat */
        sink_write_u64(snk, ptfhd->base_data_offset_pos); /** position taker */
    }
    if (tf_flags & TF_FLAGS_SAMPLE_DESCRIPTION_INDEX)
    {
        sink_write_u32(snk, ptfhd->sample_description_index);
    }
    if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_DURATION)
    {
        sink_write_u32(snk, ptfhd->default_sample_duration);
    }
    if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_SIZE)
    {
        sink_write_u32(snk, ptfhd->default_sample_size);
    }
    if (tf_flags & TF_FLAGS_DEFAULT_SAMPLE_FLAGS)
    {
        sink_write_u32(snk, ptfhd->default_sample_flags);
    }

    return EMA_MP4_MUXED_OK;
}

static int32_t
write_sample_flags(bbio_handle_t snk, sample_sdtp_t *p_sdtp_entry, uint8_t sample_padding_value, uint16_t sample_degradation_priority)
{
    uint8_t reserved = 0;

    sink_write_bits(snk, 4, reserved);
    sink_write_bits(snk, 2, p_sdtp_entry->is_leading);
    sink_write_bits(snk, 2, p_sdtp_entry->sample_depends_on);
    sink_write_bits(snk, 2, p_sdtp_entry->sample_is_depended_on);
    sink_write_bits(snk, 2, p_sdtp_entry->sample_has_redundancy);
    sink_write_bits(snk, 3, sample_padding_value);
    sink_write_bits(snk, 1, p_sdtp_entry->sample_is_non_sync_sample);
    sink_write_u16(snk, sample_degradation_priority);

    return 0;
}

static int32_t
write_trun_box(bbio_handle_t snk, track_handle_t track)
{
    uint32_t       size;                                    /** size, tag, flags, track_ID, data-offset */
    trun_t *       ptrun          = &(track->trun);
    uint32_t       tr_flags       = ptrun->tr_flags;
    uint32_t       cnt;
    list_handle_t  sync_lst       = track->sync_lst;
    list_handle_t  dts_lst        = track->dts_lst;
    list_handle_t  size_lst       = track->size_lst;
    list_handle_t  cts_offset_lst = track->cts_offset_lst;
    BOOL           is_ctts_v1     = (track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_CTTS_V1) != 0;
    BOOL           force_v0       = (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FORCE_TRUN_V0) != 0;
    BOOL           is_v1          = is_ctts_v1 && !force_v0;
    idx_dts_t *    dts_id;
    count_value_t *cv;
    int32_t        is_first_sample;

    size = 0;
    if (tr_flags)
    {
        /** get more */
        if (tr_flags & TR_FLAGS_SAMPLE_DURATION)
        {
            size += 4;
        }
        if (tr_flags & TR_FLAGS_SAMPLE_SIZE)
        {
            size += 4;
        }
        if (tr_flags & TR_FLAGS_SAMPLE_FLAGS)
        {
            size += 4;
        }
        if (tr_flags & TR_FLAGS_CTS_OFFSETS)
        {
            size += 4;
        }
        size *= ptrun->sample_count; /** the above is per sample */

        if (tr_flags & TR_FLAGS_DATA_OFFSET)
        {
            size += 4;
        }
        if (tr_flags & TR_FLAGS_FIRST_FLAGS)
        {
            size += 4;
        }
    }

    size += 4*4; /** size, tag, tr_flags, sample_count */
    sink_write_u32(snk, size);
    sink_write_4CC(snk, "trun");
    sink_write_u8(snk, is_v1 ? 1 : 0);   /** version */
    sink_write_bits(snk, 24, tr_flags);  /** flags */

    msglog(NULL, MSGLOG_DEBUG, "    trun(trun idx %u)\n", track->trun_idx);
    msglog(NULL, MSGLOG_DEBUG, "      tr_flags %#x, sample_count %u\n", tr_flags, ptrun->sample_count);

    sink_write_u32(snk, ptrun->sample_count);

    if (tr_flags & TR_FLAGS_DATA_OFFSET)
    {
        ptrun->data_offset_pos = snk->position(snk);
        sink_write_u32(snk, ptrun->data_offset);
        msglog(NULL, MSGLOG_DEBUG, "      data_offset %u\n", ptrun->data_offset);
    }
    if (tr_flags & TR_FLAGS_FIRST_FLAGS)
    {
        sample_sdtp_t *entry;
        entry = (sample_sdtp_t *)list_it_peek_entry(track->sdtp_lst);
        if (entry)
        {
            ptrun->first_sample_flags =    (((entry->is_leading & 0x3) << 26) | 
                                        ((entry->sample_depends_on & 0x3) << 24) | 
                                        ((entry->sample_is_depended_on & 0x3) << 22) | 
                                        ((entry->sample_has_redundancy & 0x3) << 20) | 
                                        ((entry->sample_is_non_sync_sample & 0x1)<<16));
        }
        sink_write_u32(snk, ptrun->first_sample_flags);

        msglog(NULL, MSGLOG_DEBUG, "      first_sample_flags %u\n", ptrun->first_sample_flags);
    }

    if ((track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FORCE_TFRA)
        && track->all_rap_samples)
    {
        /** add first sample of a 'trun' to 'tfra' if all rap */
        tfra_entry_t *pent;

        /** mfra */
        pent = (tfra_entry_t *)list_alloc_entry(track->tfra_entry_lst);
        assert(pent != NULL);

        dts_id = list_it_peek_entry(dts_lst);
        pent->time = dts_id->dts;
        if (cts_offset_lst)
        {
            cv = list_it_peek_entry(cts_offset_lst);
            pent->time += cv->value;
        }

        pent->moof_offset   = track->mp4_ctrl->moof_offset;
        pent->traf_number   = track->mp4_ctrl->traf_idx;
        pent->trun_number   = track->trun_idx;
        pent->sample_number = 1;  /** 1 base */
        list_add_entry(track->tfra_entry_lst, pent);
    }

    /** Per sample stuff. consume list_it on
    *   (1) dts_lst
    *   (2) size_lst if !all_same_size_samples
    *   (3) sync_lst if !all_rap_samples
    *   (4) cts_offset_lst if !no_cts_offset
    */
    cnt                     = ptrun->sample_count;
    is_first_sample         = 1;
    ptrun->first_sample_pos = 0;

    /** save the current list item */
    list_it_save_mark(track->sdtp_lst);

    while (cnt--)
    {
        int64_t *pos = (int64_t *)list_it_get_entry(track->pos_lst);
        if (is_first_sample && pos)
        {
            ptrun->first_sample_pos = *pos;
        }

        /** duration, dts/sync_lst */
        dts_id = list_it_get_entry(dts_lst);  /** consume one entry */

        if (tr_flags & TR_FLAGS_SAMPLE_DURATION)
        {
            idx_dts_t *dts2_id = list_it_peek_entry(dts_lst);

            if (dts2_id)
            {
                ptrun->sample_duration = (uint32_t)(dts2_id->dts - dts_id->dts);
            }
            else
            {
                assert(track->sample_num == track->sample_num_to_fraged);
                /** same value as last one, or duration - output so far */
                ptrun->sample_duration = (uint32_t)(track->media_duration - dts_id->dts);
            }
            sink_write_u32(snk, ptrun->sample_duration);
        }

        /** size and size_lst */
        if (tr_flags & TR_FLAGS_SAMPLE_SIZE)
        {
            assert(track->size_cnt);

            cv = list_it_peek_entry(size_lst);
            ptrun->sample_size = (uint32_t)cv->value;
            sink_write_u32(snk, ptrun->sample_size);
        }

        track->size_cnt--;
        if (!track->size_cnt)
        {
            list_it_get_entry(size_lst);  /** consume one entry */
            cv = list_it_peek_entry(size_lst);
            if (cv)
            {
                track->size_cnt = cv->count;
            }
            else
            {
                assert(track->sample_num == track->sample_num_to_fraged);
            }
        }

        /** flags */
        if (tr_flags & TR_FLAGS_SAMPLE_FLAGS)
        {
            sample_sdtp_t *entry = (sample_sdtp_t *)list_it_get_entry(track->sdtp_lst);
            write_sample_flags(snk, entry, 0, 0);
        }

        /** cts_offset and cts_offset_lst */
        if (!track->no_cts_offset)
        {
            assert(track->cts_offset_cnt);
            if (tr_flags & TR_FLAGS_CTS_OFFSETS)
            {
                cv = list_it_peek_entry(cts_offset_lst);
                ptrun->sample_cts_offset = (uint32_t)cv->value;
                sink_write_u32(snk, ptrun->sample_cts_offset);
            }
            track->cts_offset_cnt--;
            if (!track->cts_offset_cnt)
            {
                list_it_get_entry(cts_offset_lst);  /** consume one entry */
                cv = list_it_peek_entry(cts_offset_lst);
                if (cv)
                {
                    track->cts_offset_cnt = cv->count;
                }
                else
                {
                    assert(track->sample_num == track->sample_num_to_fraged);
                }
            }
        }

        if (!track->all_rap_samples)
        {
            idx_dts_t *sync_id = list_it_peek_entry(sync_lst);

            if (sync_id && sync_id->idx == dts_id->idx)
            {
                tfra_entry_t *pent;

                list_it_get_entry(sync_lst);/** consume one entry */

                /** mfra */
                pent = (tfra_entry_t *)list_alloc_entry(track->tfra_entry_lst);
                assert(pent != NULL);

                pent->time          = sync_id->dts + ptrun->sample_cts_offset;
                pent->moof_offset   = track->mp4_ctrl->moof_offset;
                pent->traf_number   = track->mp4_ctrl->traf_idx;
                pent->trun_number   = track->trun_idx;
                pent->sample_number = ptrun->sample_count - cnt;  /** 1 base */
                list_add_entry(track->tfra_entry_lst, pent);
            }
        }

        track->sample_num_to_fraged++;
        is_first_sample = 0;
    }

    /** save the current list item */
    list_it_goto_mark(track->sdtp_lst);

    track->first_trun_in_traf = FALSE;
    return EMA_MP4_MUXED_OK;
}

static int32_t
write_traf_box(bbio_handle_t snk, track_handle_t track)
{
    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "traf");

    msglog(NULL, MSGLOG_DEBUG, "  traf\n");
    while (get_tfhd(track))
    {
        track->trun_idx = 1; /** reset within each traf */

        write_tfhd_box(snk, track);

        /** [ISO] Section 8.8.12: Track Fragment Base Media Decode Time Box */
        if (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_TFDT)
        {
            write_tfdt_box(snk, track);
        }

        while (get_trun(track))
        {
            write_trun_box(snk, track);
            track->trun_idx++;

            /** [CFF] Section 2.2.2: (DECE) AVC NAL Unit Storage Box (video only) */
            if (track->parser->stream_type == STREAM_TYPE_VIDEO &&
                track->parser->stream_id   == STREAM_ID_H264 &&
                (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_AVCN))
            {
                /*
                 * implementation details:
                 * we are using track->sample_num_to_fraged for the avcn box writing
                 * - write_trun_box() does the update of track->sample_num_to_fraged
                 * - depending on the order of writing of trun and avcn, this variable will have a different value
                 * - in case writing of avcn should come first, track->sample_num_to_fraged will have a by
                 *   track->tfhd.sample_num lower value
                 */
                write_avcn_box(snk, track);
            }

            /** [CFF] Section 2.2.7: (DECE) Trick Play Box (video only) */
            if (track->parser->stream_type == STREAM_TYPE_VIDEO &&
                (track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_TRIK))
            {
                write_trik_box(snk, track);
            }

#ifdef ENABLE_MP4_ENCRYPTION
            if (track->encryptor && track->trun.sample_count != 0)
            {
                write_encryption_info_boxes(snk, track);
            }
#endif

        }

        if ((track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_SDTP) && list_get_entry_num(track->sdtp_lst))
        {
            write_sdtp_box(snk, track);
        }
        else
        {
            uint32_t sample_count = track->trun.sample_count;
            if (list_get_entry_num(track->sdtp_lst))
            {
                while(sample_count--)
                {
                    list_it_get_entry(track->sdtp_lst);
                }
            }
        }

        if (track->parser->stream_type == STREAM_TYPE_SUBTITLE && track->subs_present)
        {
            write_subs_box(snk, track);
        }

        track->mp4_ctrl->traf_idx++;
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

static int32_t
write_moof_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer, uint32_t track_ID_requested)
{
    const uint32_t start_track_idx = (track_ID_requested > 0) ? track_ID_2_track_idx(muxer, track_ID_requested) : 0;
    const uint32_t end_track_idx_1 = (track_ID_requested > 0) ? start_track_idx + 1 : muxer->stream_num;  /** end track index + 1 */

    uint32_t       track_idx;
    track_handle_t track;
    idx_dts_t *    dts_id;
    int32_t new_stsd_flag = 0;

    uint64_t       total_frag_size = snk->position(snk);

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "moof");

    msglog(NULL, MSGLOG_INFO, "\nmoof\n");

    muxer->moof_offset = CURRENT_BOX_OFFSET();
    muxer->traf_idx    = 1;                     /** reset within each moof */

    /** mfhd */
    sink_write_u32(snk, 16);
    sink_write_4CC(snk, "mfhd");
    sink_write_u32(snk, 0);                       /** version, flags */
    sink_write_u32(snk, muxer->sequence_number);

    msglog(NULL, MSGLOG_INFO, "  mfhd\n");
    msglog(NULL, MSGLOG_INFO, "    moof seq#: %u\n", muxer->sequence_number);

    /** trafs */
    assert(start_track_idx < muxer->stream_num);
    for (track_idx = start_track_idx; track_idx < end_track_idx_1; track_idx++)
    {
        track = muxer->tracks[track_idx];
        track->tfhd.sample_num = 0;  /** the actual samples in a 'traf' to be updated */
        dts_id                 = list_it_peek_entry(track->dts_lst);
        if (dts_id)
        {
            new_stsd_flag = get_dts_new_sd(track, dts_id->idx);

            if(dts_id->idx)
            {
                track->tfhd.sample_description_index += new_stsd_flag;
            }
        }

        if (dts_id && dts_id->dts < track->frag_dts)
        {
            /** have samples for this 'moof' */
            write_traf_box(snk, track);
        }
    }

    total_frag_size = snk->position(snk) - total_frag_size;
    for (track_idx = start_track_idx; track_idx < end_track_idx_1; track_idx++)
    {
        uint64_t track_frag_size;

        track           = muxer->tracks[track_idx];
        track_frag_size = total_frag_size + track->frag_size;
        if (track_frag_size > track->max_total_frag_size)
        {
            track->max_total_frag_size = track_frag_size;
        }
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

#define MP4MUXER_SCRATCHBUF_GRAN 0x1000

static int32_t
realloc_scratch_buffer(mp4_ctrl_handle_t muxer, size_t size)
{
    if (size > muxer->scratchsize)
    {
        size += MP4MUXER_SCRATCHBUF_GRAN - (size % MP4MUXER_SCRATCHBUF_GRAN);
        muxer->scratchbuf = REALLOC_CHK(muxer->scratchbuf, size);
        if (!muxer->scratchbuf)
        {
            muxer->scratchsize = 0;
            return -1;
        }
        muxer->scratchsize = size;
    }
    return 0;
}

#ifdef ENABLE_MP4_ENCRYPTION
static void
encrypt_subframe(track_handle_t track, uint8_t* buf, size_t size)
{
    mp4_encryptor_handle_t encryptor = track->encryptor;
    enc_subsample_info_t * enc_info_ptr;

    if (encryptor && (enc_info_ptr=it_get_entry(track->enc_info_mdat_it)))
    {
        uint32_t num_clr  = enc_info_ptr->enc_info.num_clear_bytes;
        uint32_t num_encr = enc_info_ptr->enc_info.num_encrypted_bytes;
        memcpy(encryptor->initial_value, enc_info_ptr->enc_info.initial_value, ENC_ID_SIZE);
        assert(num_clr+num_encr == size);
        encryptor->encrypt(encryptor, buf+num_clr, buf+num_clr, num_encr, NULL);
    }
#ifdef NDEBUG
    (void)size;  /** avoid compiler warning */
#endif
}
#endif

static int32_t
write_chunk(track_handle_t track, chunk_handle_t chunk, bbio_handle_t snk);

/** Writes 'mdat' of 'moof', returns: error code */
static int32_t
write_mdat_box_frag(bbio_handle_t      snk,
                    mp4_ctrl_handle_t  muxer, 
                    uint32_t           track_ID_requested,
                    int32_t               *bytes_written)
{
    const uint32_t start_track_idx = (track_ID_requested > 0) ? track_ID_2_track_idx(muxer, track_ID_requested) : 0;
    const uint32_t end_track_idx_1 = (track_ID_requested > 0) ? start_track_idx + 1 : muxer->stream_num;  /** end track index + 1 */
    int32_t            ret             = EMA_MP4_MUXED_OK;
    uint32_t       track_idx;

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "mdat");

    assert(start_track_idx < muxer->stream_num);
    for (track_idx = start_track_idx; track_idx < end_track_idx_1; track_idx++)
    {
        track_handle_t track = muxer->tracks[track_idx];
        chunk_t        chunk;
        chunk.sample_num = track->tfhd.sample_num;
        chunk.offset     = track->trun.first_sample_pos;

        if (chunk.sample_num)
        {
            int32_t r;

            /** record actual position */
            track->tfhd.base_data_offset = snk->position(snk);
            r = write_chunk(track, &chunk, snk);
            if (r != EMA_MP4_MUXED_OK)
            {
                ret = r;
            }
        }
    }

    *bytes_written = WRITE_SIZE_FIELD(snk);

    return ret;
}

/** Updates base_data_offset */
static int32_t
modify_base_data_offset(bbio_handle_t snk, mp4_ctrl_handle_t muxer, uint32_t track_ID_requested)
{
    const uint32_t start_track_idx = (track_ID_requested > 0) ? track_ID_2_track_idx(muxer, track_ID_requested) : 0;
    const uint32_t end_track_idx_1 = (track_ID_requested > 0) ? start_track_idx + 1 : muxer->stream_num;  /** end track index + 1 */

    uint32_t       track_idx;
    track_handle_t track;
    int64_t        pos;

    pos = snk->position(snk);

    assert(start_track_idx < muxer->stream_num);
    for (track_idx = start_track_idx; track_idx < end_track_idx_1; track_idx++)
    {
        track = muxer->tracks[track_idx];

        if (track->tfhd.sample_num)
        {
            if (!(track->mp4_ctrl->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_NO_BDO_IN_TFHD))
            {
                snk->seek(snk, track->tfhd.base_data_offset_pos, SEEK_SET);
                sink_write_u64(snk, track->tfhd.base_data_offset);

                msglog(NULL, MSGLOG_DEBUG, "      moof seq# %u, track_ID %u, base_data_offset %" PRIi64 "\n",
                       muxer->sequence_number, track->track_ID, track->tfhd.base_data_offset);
            }
            else if (track->trun.tr_flags & TR_FLAGS_DATA_OFFSET)
            {
                uint32_t data_offset = (uint32_t) (track->tfhd.base_data_offset - track->mp4_ctrl->moof_offset);
                snk->seek(snk, track->trun.data_offset_pos, SEEK_SET);
                sink_write_u32(snk, data_offset);
            }
        }
    }

    snk->seek(snk, pos, SEEK_SET);
    return TRUE;
}

static int32_t
write_mfra_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint32_t track_idx;
    uint32_t size;

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        if (list_get_entry_num(muxer->tracks[track_idx]->tfra_entry_lst))
        {
            break;
        }
    }
    if (track_idx == muxer->stream_num)
    {
        return 0;
    }
    else
    {
        /** mfra */
        SKIP_SIZE_FIELD(snk);
        sink_write_4CC(snk, "mfra");

        msglog(NULL, MSGLOG_INFO, "\nmfra\n");

        /** tfra */
        for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
        {
            track_handle_t track = muxer->tracks[track_idx];
            uint32_t       number_of_entry;
            tfra_entry_t * pent;

            /** removes 'tfra' entries pointing to the same fragment */
            list_it_init(track->tfra_entry_lst);
            pent = list_it_get_entry(track->tfra_entry_lst);
            if (pent && (muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_ONE_TFRA_ENTRY_PER_TRAF))
            {
                uint64_t previous_moof_offset = pent->moof_offset;
                while ((pent = list_it_get_entry(track->tfra_entry_lst)))
                {
                    if (pent->moof_offset == previous_moof_offset)
                    {
                        list_remove_entry(track->tfra_entry_lst, pent);
                        list_free_entry(pent);
                    }
                    else
                    {
                        previous_moof_offset = pent->moof_offset;
                    }
                }
            }

            number_of_entry = list_get_entry_num(track->tfra_entry_lst);
            if (number_of_entry)
            {
                uint32_t version;

                pent = list_peek_last_entry(track->tfra_entry_lst);
                size = 12 + 12 + number_of_entry*(8 + 3);
                if (pent->time < UINT32_MAX && pent->moof_offset < UINT32_MAX)
                {
                    version = 0;
                }
                else
                {
                    version = 1;
                    size += number_of_entry<<3;
                }

                sink_write_u32(snk, size);
                sink_write_4CC(snk, "tfra");
                sink_write_u8(snk, (uint8_t)version);  /** version */
                sink_write_bits(snk, 24, 0);           /** flags */

                msglog(NULL, MSGLOG_INFO, "  tfra for track %u\n", track->track_ID);

                sink_write_u32(snk, track->track_ID);
                sink_write_u32(snk, 0);             /** reserved, length_size_of_*_num == 0 */
                sink_write_u32(snk, number_of_entry);

                list_it_init(track->tfra_entry_lst);
                while ((pent = list_it_get_entry(track->tfra_entry_lst)))
                {
                    if (version)
                    {
                        sink_write_u64(snk, pent->time);
                        sink_write_u64(snk, pent->moof_offset);
                    }
                    else
                    {
                        sink_write_u32(snk, (uint32_t)pent->time);
                        sink_write_u32(snk, (uint32_t)pent->moof_offset);
                    }

                    sink_write_u8(snk, (uint8_t)pent->traf_number);
                    sink_write_u8(snk, (uint8_t)pent->trun_number);
                    sink_write_u8(snk, (uint8_t)pent->sample_number);
                }
            }
        }

        /** mfro */
        sink_write_u32(snk, 16);
        sink_write_4CC(snk, "mfro");
        sink_write_u32(snk, 0);   /** version, flags */
        size = (uint32_t)(snk->position(snk) - CURRENT_BOX_OFFSET()) + 4;
        sink_write_u32(snk, size);

        WRITE_SIZE_FIELD_RETURN(snk);
    }
}

static offset_t
write_moov_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint32_t       track_idx;
    track_handle_t track;
    uint32_t       aac_flag = 0;

    SKIP_SIZE_FIELD(snk);

    msglog(NULL, MSGLOG_INFO, "\nWriting moov\n");
    sink_write_4CC(snk, "moov");

    if (muxer->stream_num > 0)
    {
        /** write what above tracks */
        muxer->moov_size_est += write_mvhd_box(snk, muxer);

        /** [CFF] Section 2.2.4: Asset Information Box */
        if (muxer->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_AINF)
        {
            snk->write(snk, (uint8_t *)muxer->moov_ainf_atom.data, muxer->moov_ainf_atom.size);
            muxer->moov_size_est += muxer->moov_ainf_atom.size;
        }

        if (muxer->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_IODS)
        {
            muxer->moov_size_est += write_iods_box(snk, muxer);
        }

        if (!(muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG))
        {
            msglog(NULL, MSGLOG_INFO, "\nworst case moov size %u\n", muxer->moov_size_est);

            if (!muxer->co64_mode && muxer->moov_size_est + (16 + muxer->mdat_size) > (uint32_t)(-1))
            {
                muxer->co64_mode = TRUE;
            }
            /** else cfg to co64_mode, always co64_mode */
        }

        /** [ISO] Section 8.11.1: Meta Box; [CFF]: DECE Required Metadata */
        if (muxer->moov_meta_xml_data)
        {
            write_meta_box
            (
                snk,
                muxer->moov_meta_xml_data,
                muxer->moov_meta_hdlr_type,
                muxer->moov_meta_hdlr_name,
                muxer->moov_meta_items,
                muxer->moov_meta_item_sizes,
                muxer->num_moov_meta_items
            );
        }

        /** write tracks */
        for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
        {
            track = muxer->tracks[track_idx];
            if (track->sample_num)
            {
                msglog(NULL, MSGLOG_INFO, "trak for track %d\n", track->track_ID);
                if (IS_FOURCC_EQUAL(track->codingname, "mp4a"))
                {
                    aac_flag = 1;
                }
                if ((aac_flag) && (IS_FOURCC_EQUAL(track->codingname, "ec-3")))
                {
                    write_trak_box(snk, track, 1, 0x6);
                } 
                else
                {
                    write_trak_box(snk, track, 1, 0x7);
                }
            }
        }
    }

    write_private_box(snk, muxer, "moov", 0);

    muxer->moov_size_est += (int)write_udta_box(snk, muxer);

    /** fragment */
    if (muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG)
    {
        write_mvex_box(snk, muxer);
    }

    if (muxer->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_FREE)
    {
        write_free_box(snk, muxer->usr_cfg_mux_ref->free_box_in_moov_size);
    }

    WRITE_SIZE_FIELD_RETURN(snk);
}

/** for dts/sync_lst */
static void
update_idx_dts_lst(list_handle_t lst,
                   uint32_t      idx,
                   uint64_t      dts)
{
    idx_dts_t *idx_dts = (idx_dts_t *)list_alloc_entry(lst);

    idx_dts->idx = idx;
    idx_dts->dts = dts;
    list_add_entry(lst, idx_dts);
}

static void
update_sdtp_lst(list_handle_t lst,
                uint8_t       is_leading,
                uint8_t       sample_depends_on,
                uint8_t       sample_is_depended_on,
                uint8_t       sample_has_redundancy,
                uint8_t       sample_is_non_sync_sample)
{
    sample_sdtp_t *sample_sdtp = (sample_sdtp_t *)list_alloc_entry(lst);

    sample_sdtp->is_leading                = is_leading;
    sample_sdtp->sample_depends_on         = sample_depends_on;
    sample_sdtp->sample_is_depended_on     = sample_is_depended_on;
    sample_sdtp->sample_has_redundancy     = sample_has_redundancy;
    sample_sdtp->sample_is_non_sync_sample = sample_is_non_sync_sample;
    list_add_entry(lst, sample_sdtp);
}

static void
update_trik_lst(list_handle_t lst,
                uint8_t       pic_type,
                uint8_t       dependency_level)
{
    sample_trik_t *sample_trik = (sample_trik_t *)list_alloc_entry(lst);

    sample_trik->pic_type         = pic_type;
    sample_trik->dependency_level = dependency_level;
    list_add_entry(lst, sample_trik);
}

static void
update_frame_type_lst(list_handle_t lst,
                uint8_t       frame_type)
{
    sample_frame_type_t *sample_frame_type = (sample_frame_type_t *)list_alloc_entry(lst);

    sample_frame_type->frame_type = frame_type;
    list_add_entry(lst, sample_frame_type);
}

static void
update_subs_lst(list_handle_t lst,
                uint32_t *    subsample_sizes,
                uint32_t      num_subsamples)
{
    uint32_t i = 0;
    sample_subs_t *sample_subs;

    if (num_subsamples <= 1)
    {
        /** Mark empty subsamples entry */
        sample_subs = (sample_subs_t *)list_alloc_entry(lst);        
        sample_subs->subsample_size = 0;
        sample_subs->num_subs_left  = 0;
        list_add_entry(lst, sample_subs);
        return;
    }
    for (i = 0; i < num_subsamples; ++i)
    {
        sample_subs = (sample_subs_t *)list_alloc_entry(lst);        
        sample_subs->subsample_size = subsample_sizes[i];
        sample_subs->num_subs_left = num_subsamples - 1 - i;
        list_add_entry(lst, sample_subs);
    }
}

static void
my_tmp_file_open(track_handle_t track)
{
#ifdef _MSC_VER
    int ret;
#endif

    if (track->es_tmp_fn[0] == '\0') {
        OSAL_SNPRINTF(track->es_tmp_fn, 256-1, "%sp%04x.x%02x.aud_tmp", get_temp_path(), OSAL_GETPID(), track->es_idx);
        track->es_tmp_fn[256-1] = '\0';
    }

#ifdef _MSC_VER
    ret = fopen_s(&(track->file), track->es_tmp_fn, "w+b");
#else
    track->file = fopen(track->es_tmp_fn, "w+b");
#endif
    if (track->file == NULL)
    {
        msglog(NULL, MSGLOG_CRIT, "Can't create tmp file. Error\n");
        track->es_tmp_fn[0] = '\0';
    }
    else
    {
        msglog(NULL, MSGLOG_WARNING, "Created tmp file %s\n", track->es_tmp_fn);
    }
}

static int32_t
build_stsd_entry(track_handle_t track, uint8_t **pbuf)
{
    bbio_handle_t snk;
    size_t        data_size;
    int32_t ret = 0;

    /** update dsi */
    if (track->parser->get_cfg)
    {
        size_t size = 0;
        ret = track->parser->get_cfg(track->parser, &track->dsi_buf, &size);
        if (ret)
        {
            return ret;
        }
        track->dsi_size = (uint32_t)size;
    }

    snk = reg_bbio_get('b', 'w');
    snk->set_buffer(snk, NULL, 512, 1);  /** pre-alloc to avoid realloc */

    /** build stsd entry into snk */
    switch (track->parser->stream_type)
    {
    case STREAM_TYPE_VIDEO:    write_video_box(snk, track);    break;
    case STREAM_TYPE_AUDIO:    write_audio_box(snk, track);    break;
    case STREAM_TYPE_META:     write_metadata_box(snk, track); break;
    case STREAM_TYPE_TEXT:     write_text_box(snk, track);     break;
    case STREAM_TYPE_DATA:     write_data_box(snk, track);     break;
    case STREAM_TYPE_HINT:     write_rtp_box(snk, track);      break;
    case STREAM_TYPE_SUBTITLE: write_subt_box(snk, track);     break;
    }

    sink_flush_bits(snk);
    *pbuf = snk->get_buffer(snk, &data_size, 0);
    snk->destroy(snk);

    if (IS_FOURCC_EQUAL(track->codingname, "dvav") || IS_FOURCC_EQUAL(track->codingname, "dvhe"))
    {
        if (track->BL_track)
        {
            FREE_CHK(((track_handle_t) (track->BL_track))->dsi_buf);
            ((track_handle_t) (track->BL_track))->dsi_buf = NULL;
        }
    }

    return 0;
}

static int32_t
chunk_update(track_handle_t track, mp4_sample_handle_t sample)
{
    uint32_t       sz;
    uint32_t       last_idx, last_sample_num;
    chunk_handle_t chunk;

    last_idx = last_sample_num = 0;
    chunk = NULL;
    sz = 0;

    sz = list_get_entry_num(track->chunk_lst);
    if (sz == 0)
    {
        last_idx        = 0;
        last_sample_num = 1;
    }
    if (sz && !(sample->flags & SAMPLE_NEW_SD))
    {
        /**have chunk already and no new stsd: new chunk depends on span limit */
        chunk = list_peek_last_entry(track->chunk_lst);

        if (sample->dts < track->chunk_dts_top && chunk->size + sample->size <= track->max_chunk_size)
        {
            /** same chunk, including no interleave case */
            chunk->size += sample->size;
            chunk->sample_num++;
            return 0;
        }
    }
    else
    {
        chunk = list_peek_last_entry(track->chunk_lst);
    }

    if (chunk)
    {
        last_idx        = chunk->idx;
        last_sample_num = chunk->sample_num;
    }

    /** new chunk case */
    chunk = (chunk_handle_t)list_alloc_entry(track->chunk_lst);
    if (!chunk)
    {
        msglog(NULL, MSGLOG_ERR, "Not enough memory\n");
        return EMA_MP4_MUXED_NO_MEM;
    }

    chunk->dts    = rescale_u64(sample->dts, track->mp4_ctrl->timescale, track->media_timescale);
    chunk->offset = sample->pos;  /** to nal/sample info tmp file, es tmp file or es file itself */

    chunk->idx        = last_idx + last_sample_num;
    chunk->sample_num = 1;
    chunk->size       = sample->size;

    if (track->max_chunk_size < chunk->size)
    {
        msglog(NULL, MSGLOG_DEBUG, "Warning: chunk size %" PRIu64 " > limit %" PRIu64,
               chunk->size, track->max_chunk_size);
    }

    /** dref and stsd control */
    if (sample->flags & SAMPLE_NEW_SD)
    {
        /** new stsd */
        idx_ptr_t *ip;

        ip      = (idx_ptr_t *)list_alloc_entry(track->stsd_lst);
        ip->idx = track->sample_num; /** at which sample sd active */
        ip->ptr = NULL;
        /** update data_ref_index before build_stsd_entry() which uses it */
        track->data_ref_index = 1;

        list_add_entry(track->stsd_lst, ip);
        track->sample_descr_index++;
    }
    else
    {
        /** the first one sample must have the flags set */
        assert(track->sample_num>0);
    }
    chunk->data_reference_index     = track->data_ref_index;
    chunk->sample_description_index = track->sample_descr_index;

    /** chunk span control */
    if (track->chunk_span_time)
    {
        /** interleave case
           all chunks start time aligned at chunk_space_time boundary */
        if (sz)
        {
            if (sample->dts >= track->chunk_dts_top)
            {
                track->chunk_dts_top += track->chunk_span_time;
            }
        }
        else
        {
            track->chunk_dts_top = (sample->dts/track->chunk_span_time + 1)*track->chunk_span_time;
        }
    }
    /** else no interleave. track->chunk_dts_top = (uint64_t)-1 in mp4_muxer_add_track() */

    list_add_entry(track->chunk_lst, chunk);

    track->chunk_num++;

    return 0;
}

static int32_t
write_chunk(track_handle_t track, chunk_handle_t chunk, bbio_handle_t snk)
{
    int32_t             ret        = EMA_MP4_MUXED_OK;
    uint8_t * buf;
    parser_handle_t parser     = track->parser;
    uint32_t        sample_num = chunk->sample_num;
    int64_t         pos        = chunk->offset;   /** offset into sample structure file of first sample in chunk */
    uint32_t       calc_chunk_size = 0;
    size_t         write_count = 0;
    chunk->offset = snk->position(snk);

    while (sample_num--)
    {
        if (!track->size_cnt_4mdat)
        {
            count_value_t *cv = (count_value_t *)it_get_entry(track->size_it);
            if (cv == NULL)
                return EMA_MP4_MUXED_WRITE_ERR;

            track->size_cnt_4mdat = cv->count;
            track->size_4mdat     = (uint32_t)cv->value;
        }
        track->size_cnt_4mdat--;

        /** even if only subsamples are transferred, sample size is a good approx. */
        if (realloc_scratch_buffer(track->mp4_ctrl, track->size_4mdat))
        {
            return EMA_MP4_MUXED_NO_MEM;
        }
        buf = track->mp4_ctrl->scratchbuf;

        if (track->file)
        {
            /** tmp file for es used */
            size_t actual_read;
            actual_read = fread(buf, 1, track->size_4mdat, track->file);
            if (actual_read != track->size_4mdat)
            {
                msglog(NULL, MSGLOG_ERR, "read chunk from tmp file error\n");
                ret = EMA_MP4_MUXED_READ_ERR;
            }
#ifdef ENABLE_MP4_ENCRYPTION
            encrypt_subframe(track, buf, actual_read);
#endif
            write_count = snk->write(snk, buf, actual_read);
            if (write_count != actual_read)
            {
                return EMA_MP4_MUXED_WRITE_ERR;
            }
        }
        else if (track->parser->get_subsample)
        {
            /** sample structure file for ES used */
            int32_t  subs_left = 1;
            uint32_t subs_num  = 0;
            int64_t  subs_pos = 0;

            while (subs_left)
            {
                size_t subs_size = track->mp4_ctrl->scratchsize;
                subs_pos = pos;
                ret = parser->get_subsample(parser, &subs_pos, subs_num++, &subs_left, buf, &subs_size);
                if (ret == EMA_MP4_MUXED_OK)
                {
#ifdef ENABLE_MP4_ENCRYPTION
                    encrypt_subframe(track, buf, subs_size);
#endif
                    write_count = snk->write(snk, buf, subs_size);
                    if (write_count != subs_size)
                    {
                        return EMA_MP4_MUXED_WRITE_ERR;
                    }
                }
                else
                {
                    msglog(NULL, MSGLOG_ERR, "Not enough subsamples are available\n");
                    return ret;
                }
            }
            pos = subs_pos; /** sequential read follows */
        }
        else
        {
            /** else file itself is used */
            bbio_handle_t ds;

            if (track->frag_snk_file) /** if the source is fragment temp file */
            {
                ds = track->frag_snk_file;
            }
            else
            {
                ds = track->parser->ds;
            }

            if (sample_num == chunk->sample_num)
            {
                ds->seek(ds, pos, SEEK_SET); /** chunk->offset that of the first sample in chunk */
            }
            ds->read(ds, buf, track->size_4mdat);
#ifdef ENABLE_MP4_ENCRYPTION
            encrypt_subframe(track, buf, track->size_4mdat);
#endif
            write_count = snk->write(snk, buf, track->size_4mdat);
            if (write_count != track->size_4mdat)
            {
                return EMA_MP4_MUXED_WRITE_ERR;
            }
        }
        calc_chunk_size += track->size_4mdat;
    }

    return ret;
}

static void
show_chunk_output_progress(track_handle_t track, uint64_t dts, progress_handle_t prgh, uint32_t chunk_idx)
{
    if (msglog_global_verbosity_get() >= MSGLOG_DEBUG)
    {
        static uint32_t count=0;

        if (!((count++) & 0xF))
        {
            msglog(NULL, MSGLOG_DEBUG, "\n");
        }
        msglog(NULL, MSGLOG_DEBUG, "%2u", track->es_idx);
    }
    else if (prgh && msglog_global_verbosity_get() >= MSGLOG_INFO)
    {
        prgh->show(prgh, chunk_idx+1);
    }
    (void)dts;  /** avoid compiler warning */
}

/** export track */
track_handle_t
mp4_muxer_get_track (mp4_ctrl_handle_t hmuxer
                    ,uint32_t          track_ID
                    )
{
    uint32_t u;

    for (u = 0; u < hmuxer->stream_num; u++)
    {
        if (hmuxer->tracks[u]->track_ID == track_ID)
        {
            return hmuxer->tracks[u];
        }
    }
    return NULL;
}

/** Inputs samples to mp4muxer */
int
mp4_muxer_input_sample (track_handle_t      htrack
                       ,mp4_sample_handle_t hsample
                       )
{
    float           bitrate = 0.0f;
    parser_handle_t parser  = htrack->parser;
    mp4_sample_t    copied_sample;

    if (!hsample->size)
    {
        return EMA_MP4_MUXED_OK;               /** discard 0 sized packets */
    }
    if(!htrack->sample_num)
    {
        htrack->sample_duration = hsample->duration;
    }

    if (!htrack->media_timescale)
    {
        if ((parser->stream_type == STREAM_TYPE_AUDIO) && (parser->stream_id != STREAM_ID_AC4))
        {
            htrack->media_timescale = ((parser_audio_handle_t)parser)->sample_rate;
        }
        else
        {
            htrack->media_timescale = parser->time_scale;
        }

        if (!htrack->media_timescale) 
        {
            return EMA_MP4_MUXED_OK; /** parser should have the right value*/
        }

        if (htrack->warp_media_timestamps)
        {
            htrack->warp_parser_timescale = htrack->media_timescale;
            htrack->media_timescale       = htrack->warp_media_timescale;
        }

        if (htrack->mp4_ctrl->usr_cfg_mux_ref->chunk_span_time) 
        {
            /** ms => media_timescale */
            htrack->chunk_span_time =
                (uint32_t)rescale_u64((uint64_t)htrack->mp4_ctrl->usr_cfg_mux_ref->chunk_span_time,
                                      htrack->media_timescale, 1000);
        }
        /** else track->chunk_span_time = 0 in mp4_muxer_add_track() */
    }

    if (htrack->warp_media_timestamps)
    {
        copied_sample     = *hsample;
        hsample           = &copied_sample;
        hsample->dts      = rescale_u64(hsample->dts, htrack->warp_media_timescale, htrack->warp_parser_timescale);
        hsample->cts      = rescale_u64(hsample->cts, htrack->warp_media_timescale, htrack->warp_parser_timescale);
        hsample->duration = (uint32_t)rescale_u64(hsample->duration, htrack->warp_media_timescale, htrack->warp_parser_timescale);
    }

    if (!htrack->file && hsample->data)
    {
        msglog(NULL, MSGLOG_INFO, "Can't create tmp file. Try working dir.\n");
        my_tmp_file_open(htrack);
        if (!htrack->file)
        {
            return EMA_MP4_MUXED_OPEN_FILE_ERR;
        }
    }

    /** update location */
    /** save sample/record sample position */
    if (htrack->file && hsample->data)
    {
        /* tmp file for es is used  and mean to use it */
        hsample->pos = ftell(htrack->file); /* the actual sample position in tmp file */
        fwrite(hsample->data, hsample->size, 1, htrack->file);
    }
    else
    {
        /** pos is for sample info tmp file. */
        int64_t *pi64 = (int64_t *)list_alloc_entry(htrack->pos_lst);
        if (!pi64)
        {
            msglog(NULL, MSGLOG_ERR, "Not enough memory\n");
            return EMA_MP4_MUXED_NO_MEM;
        }
        *pi64 = hsample->pos;
        list_add_entry(htrack->pos_lst, pi64);
    }

    /** size */
    count_value_lst_update(htrack->size_lst, hsample->size);
    if (htrack->sample_max_size < hsample->size)
    {
        htrack->sample_max_size = (uint32_t)hsample->size;
    }
    htrack->mdat_size += (uint64_t)hsample->size;

    /** Update the 'sdtp' samples information for video */
    if (htrack->parser->stream_type == STREAM_TYPE_VIDEO)
    {
        update_sdtp_lst(htrack->sdtp_lst,
                        hsample->is_leading,
                        hsample->sample_depends_on,
                        hsample->sample_is_depended_on,
                        hsample->sample_has_redundancy,
                        (hsample->flags & SAMPLE_SYNC) ? 0 : 1);

        update_trik_lst(htrack->trik_lst,
                        hsample->pic_type,
                        hsample->dependency_level);

        update_frame_type_lst(htrack->frame_type_lst,
                        hsample->frame_type);
    }

    if (htrack->parser->stream_type == STREAM_TYPE_SUBTITLE)
    {
        update_subs_lst(htrack->subs_lst,
                        hsample->subsample_sizes,
                        hsample->num_subsamples);
        if (hsample->num_subsamples > 1)
        {
            htrack->subs_present = TRUE;
        }
    }

    /** update timing info */
    /** update rap table */
    if ((hsample->flags & SAMPLE_SYNC)) 
    {
        update_idx_dts_lst(htrack->sync_lst, htrack->sample_num, hsample->dts);
    }

        /** Update the 'sdtp' samples information for audio if needed*/
    if (htrack->parser->stream_type == STREAM_TYPE_AUDIO && list_get_entry_num(htrack->sync_lst))
    {
        update_sdtp_lst(htrack->sdtp_lst,
                                       0, /** hsample->is_leading */
                                       0, /** hsample->sample_depends_on */
                                       0, /** hsample->sample_is_depended_on */
                                       0, /** hsample->sample_has_redundancy */
                                      (hsample->flags & SAMPLE_SYNC) ? 0 : 1);
    }

    /** update dts table (not the delta dts) */
    update_idx_dts_lst(htrack->dts_lst, htrack->sample_num, hsample->dts);


    /** update cts-dts table */
    if (list_get_entry_num(htrack->cts_offset_lst) == 0)
    {
        htrack->cts_offset_v1_base = (uint32_t)(hsample->cts - hsample->dts);
    }
    count_value_lst_update(htrack->cts_offset_lst, hsample->cts - hsample->dts - htrack->cts_offset_v1_base);
    htrack->media_duration = hsample->dts + hsample->duration -
        ((idx_dts_t*)list_peek_first_entry(htrack->dts_lst))->dts;
    
    /** 'stsd', 'dref' and chunk */
    chunk_update(htrack, hsample); /** still use it for 'stsd' and 'dref' update */

    htrack->sample_num++;

    bitrate = ((float)(hsample->size)*8.0f*(float)(htrack->media_timescale))/(float)(hsample->duration);
    htrack->totalBitrate += bitrate;

    parser->bit_rate   = (uint32_t)(htrack->totalBitrate / htrack->sample_num);
    parser->maxBitrate = ((uint32_t)bitrate > parser->maxBitrate) ? (uint32_t)bitrate : parser->maxBitrate;

    return EMA_MP4_MUXED_OK;
}

static void
update_ctts(track_handle_t track, parser_handle_t parser)
{
    uint32_t cts_base = 0;
    uint32_t u, cts_offset;

    list_destroy(track->cts_offset_lst);
    track->cts_offset_lst = list_create(sizeof(count_value_t)); /** all track lists are pre-created */

    for (u = 0; u < track->sample_num; u++)
    {
        cts_offset = parser->get_cts_offset(parser, u);

        if (track->warp_media_timestamps)
        {
            cts_offset = (uint32_t)rescale_u64(cts_offset, track->warp_media_timescale, track->warp_parser_timescale);
        }

        if (u == 0 && ((track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_CTTS_V1) != 0))
        {
            cts_base = cts_offset;
        }

        count_value_lst_update(track->cts_offset_lst, cts_offset - cts_base);
    }
}

static int32_t
mp4_muxer_build_stsd_entries(track_handle_t track)
{
    uint32_t i = 0;
    uint32_t j = 0;
    int32_t ret = 0;

    /** init the it so we can go through them all one by one */
    list_it_init(track->stsd_lst);
    for (i = 0; i < list_get_entry_num(track->stsd_lst); i++)
    {
        idx_ptr_t* ptr = (idx_ptr_t*)list_it_get_entry(track->stsd_lst);
        /** note: stsd_lst might already contain valid entries (ptr->ptr != NULL) set via the demuxer
         *       - in this case keep the entry
         */
        if (ptr->ptr == NULL)
        {
            /** Set current dsi to be used inside build_stsd_entry() */
            dsi_handle_t *   p_dsi = NULL;
            it_list_handle_t it = it_create();
            it_init(it, track->parser->dsi_lst);
            for (j = 0; j <= i; j++)
            {
                p_dsi = (dsi_handle_t*)it_get_entry(it);
            }
            if (p_dsi)
            {
                track->parser->curr_dsi = *p_dsi;
            }
            it_destroy(it);

            ret = build_stsd_entry(track, &(ptr->ptr));
            if (ret)
            {
                return ret;
            }

            track->parser->dsi_curr_index++;
        }
    }

    return ret;
}

/**
 * @brief Finalizes bitrate calculation and stores avgBitrate and maxBitrate
 *
 * for AAC:  store avgBitrate and maxBitrate in DSI
 * for MP4V: store avgBitrate and maxBitrate in parser
 */
static void
calculate_bitrate_finalize(parser_handle_t parser,
                           uint32_t        media_timescale,
                           uint32_t        max_frame_size_total,
                           uint64_t        frame_size_sum,
                           uint64_t        media_duration,
                           dsi_handle_t *  p_dsi)
{
    uint32_t window_correction = 0;
    uint32_t avg_bitrate       = 0;
    uint32_t max_1sec_bitrate;

    if (parser->stream_id == STREAM_ID_AAC)
    {
        switch (media_timescale)
        {
        case 16000: window_correction = AAC_1_SEC_WINDOW_16000; break;
        case 22050: window_correction = AAC_1_SEC_WINDOW_22050; break;
        case 24000: window_correction = AAC_1_SEC_WINDOW_24000; break;
        case 32000: window_correction = AAC_1_SEC_WINDOW_32000; break;
        case 44100: window_correction = AAC_1_SEC_WINDOW_44100; break;
        case 48000: window_correction = AAC_1_SEC_WINDOW_48000; break;
        default:    window_correction = 0;                      break;
        }
    }

    /** Max Bitrate over 1 sec window */
    if (window_correction)
    {
        max_1sec_bitrate = (uint32_t)(8 * (uint64_t)max_frame_size_total * (uint64_t)window_correction)/AAC_1_SEC_WINDOW_DENOM;
    }
    else
    {
        max_1sec_bitrate = 8 * max_frame_size_total;
    }

    /** average bitrate */
    if (media_duration > 0)
    {
        avg_bitrate = 8 * (uint32_t)(frame_size_sum * (uint64_t)media_timescale / media_duration);
    }

    /** store bitrate in dsi */
    if (parser->stream_id == STREAM_ID_AAC)
    {
        mp4_dsi_aac_handle_t aac_dsi = (mp4_dsi_aac_handle_t)(*p_dsi);

        aac_dsi->esd.maxBitrate = max_1sec_bitrate;
        aac_dsi->esd.avgBitrate = avg_bitrate;
    }
    else
    {
        /** parser->stream_id == STREAM_ID_MP4V */
        parser->bit_rate   = avg_bitrate;
        parser->maxBitrate = max_1sec_bitrate;
    }
}

static uint64_t
get_dts(list_handle_t dts_lst, uint32_t sample_idx)
{
    it_list_handle_t it;
    idx_dts_t *      idx_dts;
    uint64_t         dts = 0;

    it = it_create();

    it_init(it, dts_lst);
    while ((idx_dts = it_get_entry(it)))
    {
        if (idx_dts->idx == sample_idx)
        {
            dts = idx_dts->dts;
        }
    }

    it_destroy(it);

    return dts;
}

static void
calculate_bitrate(track_handle_t track)
{
#define MAX_BITRATE_FILTER_LEN 48

    parser_handle_t  parser = track->parser;
    it_list_handle_t it_size;
    it_list_handle_t it_stsd;
    it_list_handle_t it_dsi;
    uint32_t         u, i;
    count_value_t *  cv;
    idx_ptr_t     *  ptr;
    dsi_handle_t  *  p_dsi;

    uint32_t max_frame_size_total = 0;
    uint32_t bitrate_filter[MAX_BITRATE_FILTER_LEN];
    uint32_t frame_size_total     = 0;
    uint32_t bitrate_filter_len   = 0;
    uint32_t curr_sample;
    uint32_t next_new_dsi_idx     = track->sample_num;
    uint64_t frame_size_sum;
    uint64_t media_duration;
    uint64_t first_dts;

    assert(parser != NULL);

    if ((parser->stream_id != STREAM_ID_AAC && parser->stream_id != STREAM_ID_MP4V) || track->media_duration == 0)
    {
        return;
    }

    memset(bitrate_filter, 0, sizeof(bitrate_filter));

    /** for AAC content calculate the peak bitrate over 1 sec
        with a moving average filter */

    if (parser->stream_id == STREAM_ID_AAC)
    {
        /** calculates filter length for 1 second of audio 
            calculations are based on core aac decoder with 1024 samples per frame */
        bitrate_filter_len = (track->media_timescale + 1023) / 1024;
    }
    else if (parser->stream_id == STREAM_ID_MP4V)
    {
        bitrate_filter_len = (uint32_t)((track->media_timescale * track->sample_num + track->media_duration -1 ) / track->media_duration);
    }
    else
    {
        assert(0);
    }

    if (bitrate_filter_len > MAX_BITRATE_FILTER_LEN)
    {
        bitrate_filter_len = MAX_BITRATE_FILTER_LEN;
    }

    msglog(NULL, MSGLOG_INFO, "\nbitrateFilterLen: %u  Num frames:%u\n", bitrate_filter_len, track->sample_num);

    it_size = it_create();
    it_stsd = it_create();
    it_dsi  = it_create();

    it_init(it_size, track->size_lst);
    it_init(it_stsd, track->stsd_lst);
    it_init(it_dsi,  parser->dsi_lst);

    curr_sample    = 0;
    frame_size_sum = 0;
    first_dts      = ((idx_dts_t*)list_peek_first_entry(track->dts_lst))->dts;
    ptr            = (idx_ptr_t *)it_get_entry(it_stsd);
    if (ptr)
    {
        assert(ptr->idx == 0);  /** index in first entry is expected to be 0 */
        ptr = (idx_ptr_t *)it_get_entry(it_stsd);
        if (ptr && ptr->idx)
        {
            /** we have a second and non-zero entry */
            /** Note: setting the DSI via parser_aac_set_asc() and having "multi-dsi" as ES input is not expected to work! */
            next_new_dsi_idx = ptr->idx;
        }
    }
    p_dsi = (dsi_handle_t*)it_get_entry(it_dsi);
    while ((cv = it_get_entry(it_size)))
    {
        for (u = 0; u < cv->count; u++, curr_sample++)
        {
            /** check for dsi change */
            if (curr_sample == next_new_dsi_idx)
            {
                uint64_t curr_dts;

                /** finalize bitrate calculation and start a new one */
                curr_dts       = get_dts(track->dts_lst, curr_sample);
                media_duration = curr_dts - first_dts;
                first_dts      = curr_dts;
                calculate_bitrate_finalize(parser, track->media_timescale, max_frame_size_total, frame_size_sum, media_duration,
                                           p_dsi);

                /** advance in stsd list and dsi list */
                ptr = (idx_ptr_t *)it_get_entry(it_stsd);
                if (ptr)
                {
                    next_new_dsi_idx = ptr->idx;
                }
                else
                {
                    next_new_dsi_idx = parser->num_samples;
                }
                p_dsi = (dsi_handle_t*)it_get_entry(it_dsi);

                /** clear bitrate_filter for new run */
                memset(bitrate_filter, 0, sizeof(bitrate_filter));
                max_frame_size_total = 0;
                frame_size_sum       = 0;
            }

            /** maintain filter window of frame size values */
            for (i = bitrate_filter_len - 1; i > 0; i--)
            {
                bitrate_filter[i] = bitrate_filter[i - 1];
            }
            /** add latest frame size value */
            bitrate_filter[0] = (uint32_t)cv->value;

            frame_size_sum += cv->value;

            /** calculate sum over window (not the actual mean as we want to preserve fixed point accuracy) */
            frame_size_total = 0;
            for (i = 0; i < bitrate_filter_len; i++)
            {
                frame_size_total += bitrate_filter[i];
            }

            /** track max value */
            if (frame_size_total > max_frame_size_total)
            {
                max_frame_size_total = frame_size_total;
            }
        }
    }
    it_destroy(it_size);
    it_destroy(it_stsd);
    it_destroy(it_dsi);

    media_duration = track->media_duration + ((idx_dts_t*)list_peek_first_entry(track->dts_lst))->dts - first_dts;
    calculate_bitrate_finalize(parser, track->media_timescale, max_frame_size_total, frame_size_sum, media_duration, p_dsi);

    parser->bit_rate = mp4_muxer_get_track_bitrate(track);

    if (parser->stream_id == STREAM_ID_AAC)
    {
        mp4_dsi_aac_handle_t  aac_dsi      = (mp4_dsi_aac_handle_t)parser->curr_dsi;
        parser_audio_handle_t parser_audio = (parser_audio_handle_t)parser;

        /** update_audio_dsi() expects bufferSizeDB and Sampling Frequency to be set up
         *  in case this has not been done (e.g. no ADTS input), we do this here.
         */
        if (!aac_dsi->esd.bufferSizeDB)
        {
            aac_dsi->esd.bufferSizeDB = parser_audio->buferSizeDB;
        }
        if (!aac_dsi->samplingFrequency)
        {
            aac_dsi->samplingFrequency = parser_audio->sample_rate;
        }
    }
}

static int32_t
setup_muxer(mp4_ctrl_handle_t muxer)
{
    uint32_t        track_idx;
    track_handle_t  track;
    parser_handle_t parser;
    int32_t ret = 0;

    muxer->chunk_num = 0;
    muxer->mdat_size = 0;
    if (muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG)
    {
        muxer->sequence_number = 1;
    }

    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        track = muxer->tracks[track_idx];
        track->parser->dsi_curr_index = 1;

        calculate_bitrate(track);

        ret = mp4_muxer_build_stsd_entries(track);
        if (ret)
        {
            return EMA_MP4_MUXED_BUGGY;
        }

        parser = track->parser;

        if ((parser->dv_rpu_nal_flag) && (parser->dv_el_track_flag))
        {
            if(track_idx >= 1)
            {
                if (track->sample_num != muxer->tracks[track_idx-1]->sample_num)
                {
                     msglog(NULL, MSGLOG_ERR, "ERROR: BL and EL sample number is not equal!\n");
                    return EMA_MP4_MUXED_BUGGY;
                }
                track->media_timescale = muxer->tracks[track_idx-1]->media_timescale;
                track->media_duration = muxer->tracks[track_idx-1]->media_duration;
                track->no_cts_offset = muxer->tracks[track_idx-1]->no_cts_offset;
            }
            else
            {
                return EMA_MP4_MUXED_BUGGY;
            }
        }

        /** debug only */
        msglog(NULL, MSGLOG_INFO, "\nstream %u:\n  %d samples: \n", track->es_idx, track->sample_num);
        if (parser->show_info)
        {
            parser->show_info(parser);
        }
        msglog(NULL, MSGLOG_INFO, "  tmp table size: dts: %d, cts_offset %d, size %d, rap %d\n",
               list_get_entry_num(track->dts_lst), list_get_entry_num(track->cts_offset_lst),
               list_get_entry_num(track->size_lst),list_get_entry_num(track->sync_lst));
        msglog(NULL, MSGLOG_INFO, "              chunks: %d\n", list_get_entry_num(track->chunk_lst));
        /** end of debug */

        /** Check if there is even any sample available in that track.
            If there is no sample present, stop muxing process as it can lead
            to confusion when muxing empty tracks. Moreover, empty tracks are no use case. */
        if (!track->sample_num)
        {
            msglog(NULL, MSGLOG_ERR, "Aborting muxing process, stream %u is empty or corrupted.\n", track->es_idx);
            return EMA_MP4_MUXED_EMPTY_ES;
        }

        /** fix CTS if supported (avc only and with reordering) */
        if (parser->get_cts_offset && parser->need_fix_cts(parser))
        {
            update_ctts(track, parser);
            msglog(NULL, MSGLOG_INFO, "  final table size: cts %d\n", list_get_entry_num(track->cts_offset_lst));
        }

        if (track->parser->dv_el_track_flag)
        {
            uint32_t count_bl = 0;
            uint32_t count_el = 0;
            uint32_t index = 0;
            idx_dts_t *idx_dts = NULL;

            /** get sync list frome BL */
            count_bl = list_get_entry_num(((track_handle_t)(track->BL_track))->sync_lst);
            count_el = list_get_entry_num(track->sync_lst);

            if (count_el < count_bl)
            {
                msglog(NULL, MSGLOG_ERR, "Error: Dolby Vision EL track has less IDR frame than BL's! \n");
                return EMA_MP4_MUXED_READ_ERR;
            }
            list_destroy(track->sync_lst);
            track->sync_lst = list_create(sizeof(idx_dts_t));

            list_it_init(((track_handle_t)(track->BL_track))->sync_lst);
            for(index = 0; index < count_bl; index++)
            {
                idx_dts = list_it_get_entry(((track_handle_t)(track->BL_track))->sync_lst);
                update_idx_dts_lst(track->sync_lst, idx_dts->idx, idx_dts->dts);
            }
            list_it_init(((track_handle_t)(track->BL_track))->sync_lst);
            list_it_init(track->sync_lst);

        }

        /** help flags */
        track->all_rap_samples       = (list_get_entry_num(track->sync_lst) == track->sample_num);
        track->all_same_size_samples = (list_get_entry_num(track->size_lst) == 1);
        track->no_cts_offset         = (list_get_entry_num(track->cts_offset_lst) == 1 &&
                                        ((count_value_t*)list_peek_first_entry(track->cts_offset_lst))->value == 0 );


        /** build edit list, if necessary */
        if (!track->no_cts_offset && !list_get_entry_num(track->edt_lst) && list_get_entry_num(track->cts_offset_lst))
        {
            uint32_t cts_offset = (uint32_t)((count_value_t*)list_peek_first_entry(track->cts_offset_lst))->value;
            if (cts_offset)
            {
                mp4_muxer_add_to_track_edit_list(track, track->media_duration, cts_offset);
                msglog(NULL, MSGLOG_INFO, "adding edit list to compensate for cts offset (%d)\n", cts_offset);
            }
        }

        /** reset tmp for read back */
        if (track->file)
        {
            fseek(track->file, 0, SEEK_SET);
        }

        /** if source is fragment temp file */
        if (track->frag_snk_file)
        {
            int8_t fn[64];

            track->frag_snk_file->destroy(track->frag_snk_file);

            OSAL_SNPRINTF(fn, 64, "temp_dump.%u.%s.mp4dat", track->strm_idx, track->parser->stream_name);
            track->frag_snk_file = reg_bbio_get('f', 'r');
            if (track->frag_snk_file->open(track->frag_snk_file, fn))
            {
                msglog(NULL, MSGLOG_ERR, "\nfail to open fragment temp file %s!\n", fn);
                return EMA_MP4_MUXED_READ_ERR;
            }
        }

        /** collect track info into muxer */
        muxer->chunk_num += track->chunk_num;
        muxer->mdat_size += track->mdat_size;

        it_init(track->size_it, track->size_lst);
#ifdef ENABLE_MP4_ENCRYPTION
        if (track->enc_info_lst == NULL)
        {
            /** The track structure might not have been initialized by mp4_muxer_add_track(),
               but created somewhere externally (by the demuxer). Therefore, initialize
               data as needed. 
            */
            track->enc_info_lst = list_create(sizeof(enc_subsample_info_t));
        }
        if (track->enc_info_mdat_it == NULL)
        {
            track->enc_info_mdat_it = it_create();
        }

        list_it_init(track->enc_info_lst);
        it_init(track->enc_info_mdat_it, track->enc_info_lst);
#endif

        /** movie duration is already needed for 'pdin' prior to writing 'mvhd' */
        if (track->sample_num)
        {
            assert(track->media_timescale > 0);
            if (0 == list_get_entry_num(track->edt_lst))
            {
                track->sum_track_edits = rescale_u64(track->media_duration, muxer->timescale, track->media_timescale);
            }
            if (muxer->duration < track->sum_track_edits)
            {
                muxer->duration = track->sum_track_edits;
            }
        }

        if (!(muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG))
        {
            /**estimate moov size */
            muxer->moov_size_est +=
                120 /** mvhd */
                + 8   /** trak: s, t */
                    + 104 /** tkhd */
                    + 8 + 4 + 12 * (1 + list_get_entry_num(track->edt_lst)) /** edts, elst */
                    + 8 /** mdia: s, t */
                        + 44 /** mdhd */
                        + 12 + 4 + 4 + 12 + 32 /** hdlr: assuming name < 32 */
                        + 8 /** minf: s, t */
                            + 20 /** max of vmhd, smhd, nmhd. hndh not counted */
                            + 8 + 28 /** dinf, dref(self contained) */
                                + 8 /** stbl: s, t */
                                + 12 + 4 + 8 * list_get_entry_num(track->dts_lst) /** stts: worst case */
                                + 12 + 4 + 8 * list_get_entry_num(track->cts_offset_lst) /** ctts: worst case */
                                + 12 + 4 + 4 * list_get_entry_num(track->sync_lst) /** stss */
                                + 12 + 4 + 12 * list_get_entry_num(track->chunk_lst)  /** stsc: worst case */
                                + 12 + (4 + 4) + 4 * ((list_get_entry_num(track->size_lst) == 1) ? 0 : track->sample_num) /** stsz */
                                + 12 + 4 + 8 * list_get_entry_num(track->chunk_lst) /** stco: assuming 8 byte size */
                ;
            /** stsd */
            muxer->moov_size_est += 12 + 4; /** stsd: s,t,vf, entries */
            /** vide, soun, hint, meta: assuming just one. 8 + 6 + 2 = 16: SampleEntry */
            if (parser->stream_type == STREAM_TYPE_VIDEO)
            {
                muxer->moov_size_est += (16 + 70 + (track->dsi_size + 8))*1;
            }
            else if (parser->stream_type == STREAM_TYPE_AUDIO)
            {
                muxer->moov_size_est += (16 + 20 + (track->dsi_size + 8))*1;
            }
            else if (parser->stream_type == STREAM_TYPE_DATA)
            {
                muxer->moov_size_est += (16 + (track->dsi_size + 8))*1;
            }
            /** end of estimate moov size */
        }
        else
        {
            /** init the iterate on the lst so we can get them one by one */
            list_it_init(track->dts_lst);
            list_it_init(track->cts_offset_lst);
            list_it_init(track->sync_lst);
            list_it_init(track->size_lst);
            list_it_init(track->stsd_lst);
            list_it_init(track->sdtp_lst);
            list_it_init(track->trik_lst);
            list_it_init(track->frame_type_lst);
            list_it_init(track->subs_lst);
            list_it_init(track->segment_lst);

            if (track->track_ID == muxer->frag_ctrl_track_ID)
            {
                if (track->all_rap_samples)
                {
                    /** no need to align trun with rap */
                    muxer->frag_ctrl_track_ID = 0;
                }

                if (((idx_dts_t *)list_peek_first_entry(track->sync_lst))->idx != 0)
                {
                    msglog(NULL, MSGLOG_WARNING, "WARNING: rap track's first sample is not a rap.\n");
                }
            }

            /** set up default_sample_size */
            if (list_it_peek_entry(track->size_lst))
            {
                track->trex.default_sample_size = (uint32_t)(
                    ((count_value_t *)list_it_peek_entry(track->size_lst))->value);
            }
            else
            {
                track->trex.default_sample_size = 0;
            }
            track->tfhd.default_sample_size = track->trex.default_sample_size;

            /** set up default_sample_duration */
            if (list_get_entry_num(track->dts_lst) > 1)
            {
                track->trex.default_sample_duration = (uint32_t)(
                    ((idx_dts_t *)list_it_peek2_entry(track->dts_lst))->dts -
                    ((idx_dts_t *)list_it_peek_entry(track->dts_lst))->dts);
            }
            else
            {
                track->trex.default_sample_duration = (uint32_t)(track->media_duration);
            }
            track->tfhd.default_sample_duration = track->trex.default_sample_duration;

            /** set up default_sample_flags */
            if (track->all_rap_samples)
            {
                /** RAP only */
                track->trex.default_sample_flags = SAMPLE_FLAGS_ALL_RAP;
            }
            else
            {
                track->trex.default_sample_flags = SAMPLE_FLAGS_PREDICT;
            }
            track->tfhd.default_sample_flags = track->trex.default_sample_flags;

            /** specific for trun build */
            if (list_it_peek_entry(track->size_lst))
            {
                track->size_cnt       = ((count_value_t *)list_it_peek_entry(track->size_lst))->count;
                track->cts_offset_cnt = ((count_value_t*)list_it_peek_entry(track->cts_offset_lst))->count;
            }
            else
            {
                track->size_cnt       = 0;
                track->cts_offset_cnt = 0;
            }

            list_it_init(track->pos_lst);

            /** in case input source is fragment, got to clear tfra_entry_lst */
            list_destroy(track->tfra_entry_lst);
            track->tfra_entry_lst = list_create(sizeof(tfra_entry_t));

            track->sample_num_to_fraged = 1;
        }
    }

    return EMA_MP4_MUXED_OK;
}

/** Like setup_muxer() - to be used when creating init segment
 *  since we do not intend to do a real muxing, setup can be shorter.
 *
 * Assumptions:
 * - (muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG)
 * - (muxer->stream_num == 1)
 * - (track->sample_num == 0) # at least allowed
 * - (!track->file)
 * - (!track->frag_snk_file)
 */
static int32_t
setup_muxer_short(mp4_ctrl_handle_t muxer, uint16_t *p_video_width, uint16_t *p_video_height)
{
    track_handle_t  track  = muxer->tracks[0];  /** we have just one track */
    parser_handle_t parser = track->parser;

    assert(muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG);
    assert(muxer->stream_num == 1);
    assert(!track->file);
    assert(!track->frag_snk_file);

    muxer->chunk_num       = 0;
    muxer->mdat_size       = 0;
    muxer->sequence_number = 1;

    parser->dsi_curr_index = 1;

    if (parser->stream_type == STREAM_TYPE_AUDIO)
    {
        track->media_timescale = ((parser_audio_handle_t)parser)->sample_rate;
    }
    else
    {
        track->media_timescale = parser->time_scale;
    }

    /** use track->parser->codec_config_lst for building of track->stsd_lst */
    {
        it_list_handle_t it = it_create();
        codec_config_t * p_codec_config;

        int32_t i = 0;

        it_init(it, parser->codec_config_lst);
        while ((p_codec_config = (codec_config_t*)it_get_entry(it)))
        {
            idx_ptr_t *   ip;
            bbio_handle_t snk;
            size_t        data_size;

            track->dsi_buf  = p_codec_config->codec_config_data;
            track->dsi_size = (uint32_t)p_codec_config->codec_config_size;

            track->data_ref_index = 1;

            snk = reg_bbio_get('b', 'w');
            snk->set_buffer(snk, NULL, 512, 1);

            if (parser->stream_type == STREAM_TYPE_VIDEO)
            {
                if (p_video_width && p_video_height)
                {
                    parser_video_handle_t parser_video = (parser_video_handle_t)parser;

                    parser_video->width  = p_video_width[i];
                    parser_video->height = p_video_height[i];
                }

                write_video_box(snk, track);
            }
            else if (parser->stream_type == STREAM_TYPE_AUDIO)
            {
                write_audio_box(snk, track);
            }
            else
            {
                assert(0);
            }

            sink_flush_bits(snk);

            /** create entry in stsd_lst */
            ip      = (idx_ptr_t *)list_alloc_entry(track->stsd_lst);
            ip->idx = 0;
            ip->ptr = snk->get_buffer(snk, &data_size, 0);
            list_add_entry(track->stsd_lst, ip);
            track->sample_descr_index++;

            snk->destroy(snk);

            track->dsi_buf = NULL;
            i++;
        }
        it_destroy(it);
    }

    track->trex.default_sample_size     = 0;
    track->trex.default_sample_duration = (uint32_t)(track->media_duration);

    /** specific for trun build */
    track->size_cnt       = 0;
    track->cts_offset_cnt = 0;

    /** in case input source is fragment, got to clear tfra_entry_lst */
    list_destroy(track->tfra_entry_lst);
    track->tfra_entry_lst = list_create(sizeof(tfra_entry_t));

    track->sample_num_to_fraged = 1;

    if (!track->sample_num)
    {
        /** set sample_num to force writing of [trak] box */
        track->sample_num = 1;
    }

    /** add fake entry to chunk_lst to force writing of [stsd] box */
    {
        chunk_handle_t chunk = (chunk_handle_t)list_alloc_entry(track->chunk_lst);

        if (!chunk)
        {
            msglog(NULL, MSGLOG_ERR, "Not enough memory\n");
            return EMA_MP4_MUXED_NO_MEM;
        }

        chunk->idx                      = 1;
        chunk->dts                      = 0;
        chunk->offset                   = 0;
        chunk->data_reference_index     = 1;
        chunk->sample_num               = 1;
        chunk->size                     = 0;
        chunk->sample_description_index = 1;

        list_add_entry(track->chunk_lst, chunk);
    }

    return EMA_MP4_MUXED_OK;
}

static int32_t
write_mdat_box(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    int32_t               ret = EMA_MP4_MUXED_OK;
    uint32_t          track_idx;
    track_handle_t    track;
    uint32_t          chunk_idx;
    uint64_t          dts_out;
    progress_handle_t prgh;

    msglog(NULL, MSGLOG_INFO, "Writing mdat %" PRIu64 " bytes", muxer->mdat_size);
    /** size */
    if (muxer->mdat_size+8 <= (uint32_t)(-1))
    {
        sink_write_u32(snk, (uint32_t)muxer->mdat_size + 8);
        sink_write_4CC(snk, "mdat");
    }
    else
    {
        sink_write_u32(snk, 1);
        sink_write_4CC(snk, "mdat");
        sink_write_u64(snk, muxer->mdat_size + 16);
    }

    /** write out chunks in interleave mode */
    msglog(NULL, MSGLOG_INFO, ", %u chunks:\n", muxer->chunk_num);
    prgh    = progress_create("  written", muxer->chunk_num);
    dts_out = 0;

    /** init chunk list iterator */
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        list_it_init(muxer->tracks[track_idx]->chunk_lst);
    }

    for (chunk_idx = 0; chunk_idx < muxer->chunk_num; chunk_idx++)
    {
        chunk_handle_t chunk;
        track_handle_t track_out = NULL;

        /** To find the chunk with dts no larger than current output one.
         *  This process should speed up the search since all chunk have the similar
         *  size.
         */
        for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
        {
            track = muxer->tracks[track_idx];
            if (track->chunk_to_out == track->chunk_num)
            {
                continue;
            }
            chunk = list_it_peek_entry(track->chunk_lst);
            if (chunk->dts <= dts_out)
            {
                track_out = track;
                break;
            }
        }

        /** find the track smallest dts if not yet */
        if (!track_out)
        {
            dts_out = (uint64_t)(-1);
            for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
            {
                track = muxer->tracks[track_idx];
                if (track->chunk_to_out == track->chunk_num)
                {
                    continue;
                }
                chunk = list_it_peek_entry(track->chunk_lst);
                if (chunk->dts < dts_out)
                {
                    dts_out = chunk->dts;
                    track_out = track;
                }
            }
        }

        if (track_out)
        {
            chunk = list_it_get_entry(track_out->chunk_lst);
            show_chunk_output_progress(track_out, chunk->dts, prgh, chunk_idx);

            if (muxer->progress_cb != NULL)
            {
                if (muxer->chunk_num > 0)
                {
                    muxer->progress_cb((float)(100.0 * (float)(chunk_idx+1)/(float)muxer->chunk_num ), muxer->progress_cb_instance);
                }
            }

            ret = write_chunk(track_out, chunk, snk);
            /** side effect: chunk offset id set to actual value */
            if (ret != EMA_MP4_MUXED_OK)
            {
                break;
            }
            track_out->chunk_to_out++;
        }
        else
        {
            /** should not come here */
            msglog(NULL, MSGLOG_ERR, "chunk number not match");
            ret = EMA_MP4_MUXED_BUGGY;
            break;
        }
    }

    prgh->destroy(prgh);
    msglog(NULL, MSGLOG_INFO, "\n");

    return ret;
}

static void
modify_stco_boxes(bbio_handle_t snk, mp4_ctrl_handle_t muxer)
{
    uint32_t       track_idx;
    track_handle_t track;

    msglog(NULL, MSGLOG_INFO, "Modifying stco\n");
    for (track_idx = 0; track_idx < muxer->stream_num; track_idx++)
    {
        track = muxer->tracks[track_idx];
        /** debug */
        assert(track->chunk_to_out == track->chunk_num);
        /** end of debug */

        if (track->chunk_num)
        {
            snk->seek(snk, track->stco_offset, SEEK_SET);
            write_stco_box(snk, track);
        }
    }
}

/** Writes incomplete Segment Index Box (sidx)
 *
 *  referenced_size and subsegment_duration fields need to be updated for each (sub)segment
 *  using update_sidx_box()
 *
 *  size: (out) of sidx box written, must not be NULL
 *
 *  returns: error code
 *
 *  assumptions:
 *  - each fragment forms one (sub)segment
 *  - only one track will be muxed when writing of sidx boxes is enabled
 *  - each fragment starts with a sync sample
 */
static int32_t
write_sidx_box(bbio_handle_t snk,     /**< mp4 sink */
               track_handle_t track,  /**< track */
               uint32_t *size         /**< length in bytes of the written sidx box */
               )
{
    BOOL     is_ctts_v1 = (track->mp4_ctrl->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_CTTS_V1) != 0;
    uint32_t timescale  = track->media_timescale;
    uint16_t i;
    uint32_t earliest_presentation_time = 0;

    SKIP_SIZE_FIELD(snk);

    assert(size != NULL);

    if (track->cts_offset_lst && list_get_entry_num(track->cts_offset_lst) && !is_ctts_v1)
    {
        earliest_presentation_time = (uint32_t)(((count_value_t*)list_peek_first_entry(track->cts_offset_lst))->value);
    }

    msglog(NULL, MSGLOG_INFO, "\nWriting sidx dummy box\n");

    sink_write_4CC(snk, "sidx");

    sink_write_u32(snk, 0);                           /** version, flags */
    sink_write_u32(snk, track->track_ID);             /** reference_ID */
    sink_write_u32(snk, timescale);
    sink_write_u32(snk, earliest_presentation_time);
    sink_write_u32(snk, 0);                           /** first_offset */
    sink_write_u16(snk, 0);                           /** reserved */

    sink_write_u16(snk, track->sidx_reference_count);
    for (i = 0; i < track->sidx_reference_count; i++)
    {
        sink_write_u32(snk, 0);                       /** reference_type + referenced_size - will be updated */
        sink_write_u32(snk, 0);                       /** subsegment_duration - will be updated */
        sink_write_u32(snk, 0x90000000);              /** starts_with_SAP = 1, SAP_type = 1, SAP_delta_time = 0 */
    }

    msglog(NULL, MSGLOG_INFO, "sidx: timescale:                  %u\n", timescale);
    msglog(NULL, MSGLOG_INFO, "sidx: earliest_presentation_time: %u\n", earliest_presentation_time);
    msglog(NULL, MSGLOG_INFO, "sidx: reference_count:            %d\n", track->sidx_reference_count);

    *size = WRITE_SIZE_FIELD(snk);

    return EMA_MP4_MUXED_OK;
}

/** Updates referenced_size and subsegment_duration in sidx box */
static void
update_sidx_box(bbio_handle_t     snk,              /**< mp4 sink */
                track_handle_t    track,            /**< track */
                offset_t          sidx_pos,         /**< start position of the written sidx box */
                offset_t          sidx_size,        /**< length in bytes of the written sidx box */
                int32_t           referenced_size   /**< size of the referenced (sub)segment */
                )
{
    uint32_t frag_num = track->frag_num;

    /** safety check: requested update must occur within the limit of the sidx box */
    if (32 + frag_num * 12 + 8 <= sidx_size)
    {
        offset_t cur_pos             = snk->position(snk);
        uint32_t subsegment_duration = track->frag_duration;

        msglog(NULL, MSGLOG_INFO, "sidx-update: referenced_size: %u, subsegment_duration: %u\n",
               referenced_size, subsegment_duration);

        snk->seek(snk, sidx_pos + 32 + frag_num * 12, SEEK_SET);
        sink_write_u32(snk, referenced_size);
        sink_write_u32(snk, subsegment_duration);
        snk->seek(snk, cur_pos, SEEK_SET);
    }
    else
    {
        msglog(NULL, MSGLOG_WARNING, "NO update in sidx box for frag#: %u\n", frag_num);
        assert(0);
    }
}

/** Update first_offset in sidx box */
static void
update_sidx_box_offset(bbio_handle_t     snk,          /**< mp4 sink */
                       offset_t          sidx_pos,     /**< start position of the written sidx box */
                       offset_t          sidx_size,    /**< length in bytes of the written sidx box */
                       offset_t          moof_offset   /**< moof offset of data */
                       )
{
    offset_t cur_pos      = snk->position(snk);
    uint32_t first_offset = (uint32_t)(moof_offset - sidx_pos - sidx_size);

    snk->seek(snk, sidx_pos+24, SEEK_SET);
    sink_write_u32(snk, first_offset);
    snk->seek(snk, cur_pos, SEEK_SET);
}


static uint32_t
write_ssix_box(bbio_handle_t snk, track_handle_t track)
{
    uint16_t i,j;
    uint32_t ranges_count;
    uint32_t range_size = 0;
    uint32_t ret = 0;
    uint32_t segment_sample_count;
    count_value_t *cv = NULL;
    uint32_t sample_count = 1;

    it_list_handle_t it = it_create();

    SKIP_SIZE_FIELD(snk);
    sink_write_4CC(snk, "ssix");

    sink_write_u32(snk, 0);                           /** version, flags */
    sink_write_u32(snk, track->sidx_reference_count); /** subsegment_count */
    
    it_init(it, track->size_lst);
    list_it_init(track->frame_type_lst);
    list_it_save_mark(track->segment_lst);

    for (i = 0; i < track->sidx_reference_count; i++)
    {
        sample_frame_type_t *entry_cur,*entry_next;
        frag_index_t *frag_index;
        offset_t ranges_count_pos     = snk->position(snk);
        ranges_count = 0;
        frag_index = list_it_get_entry(track->segment_lst);

        if(frag_index->frag_end_idx != list_get_entry_num(track->dts_lst)-1)
        {
            segment_sample_count = frag_index->frag_end_idx - frag_index->frag_start_idx;
        }
        else
        {
            segment_sample_count = frag_index->frag_end_idx  - frag_index->frag_start_idx + 1;
        }

        /** Currently ranges_count is 0, we'll update it when we know it's real value.
          * Based on ISO/IEC 14496-12 2012 8.16.4.2: ranges_count should be 32 bit. */
        sink_write_u32(snk, 0); 
    
        entry_cur = (sample_frame_type_t *)list_it_get_entry(track->frame_type_lst);

        if(sample_count == 1)
        {
            cv = (count_value_t *)it_get_entry(it);
            if (cv == NULL)
            {
                it_destroy(it);
                return EMA_MP4_MUXED_WRITE_ERR;
            }
            sample_count = cv->count;
        }
        else 
        {
            sample_count --;
        }

        range_size = (uint32_t)cv->value;
        for (j = 0; j < segment_sample_count; j++)
        {
            entry_next = (sample_frame_type_t *)list_it_peek_entry(track->frame_type_lst);
            if (j < (segment_sample_count -1) )
            {
                if(sample_count == 1)
                {
                    cv = (count_value_t *)it_get_entry(it);
                    if (cv == NULL)
                    {
                        it_destroy(it);
                        return EMA_MP4_MUXED_WRITE_ERR;
                    }
                    sample_count = cv->count;
                }
                else 
                {
                    sample_count --;
                }
            }

            if ((entry_next != 0) && (entry_cur->frame_type == entry_next->frame_type) && (j < (segment_sample_count -1) ))
            {
                list_it_get_entry(track->frame_type_lst); /** consume an entry*/
                range_size += (uint32_t)cv->value;
                if (j == (segment_sample_count - 1))
                {
                    sink_write_u8(snk, entry_cur->frame_type);
                    sink_write_bits(snk, 24, range_size); 
                    ranges_count++;
                }
            }
            else 
            {
                assert(entry_cur != NULL);
                sink_write_u8(snk, entry_cur->frame_type);
                sink_write_bits(snk, 24, range_size);
                ranges_count++;
                if (j < (segment_sample_count - 1))
                {
                    entry_cur = (sample_frame_type_t *)list_it_get_entry(track->frame_type_lst);
                    range_size = (uint32_t)cv->value;
                }
            }
        }
        /** update ranges_count real value to sink */
        {
            offset_t cur_pos   = snk->position(snk);
            snk->seek(snk, ranges_count_pos, SEEK_SET);
            sink_write_u32(snk, ranges_count);
            snk->seek(snk, cur_pos, SEEK_SET);
        }

    }
    list_it_goto_mark(track->segment_lst);
    it_destroy(it);

    ret = WRITE_SIZE_FIELD(snk);
    return ret;
}

int
mp4_muxer_output_tracks(mp4_ctrl_handle_t muxer)
{
    int32_t           ret = EMA_MP4_MUXED_OK;
    bbio_handle_t snk = muxer->mp4_sink;

    offset_t sidx_pos[MAX_STREAMS];
    offset_t sidx_size[MAX_STREAMS];
    int32_t  sidx_first_offset_written[MAX_STREAMS];

    uint64_t data_written = 0ULL;  /** 'mdat' data written */

    memset(sidx_pos, 0, sizeof(offset_t)*MAX_STREAMS);
    memset(sidx_size, 0, sizeof(offset_t)*MAX_STREAMS);
    memset(sidx_first_offset_written, 0, sizeof(int)*MAX_STREAMS);

    /** final preparation for write out 'moov' and 'mdat' */
    ret = setup_muxer(muxer);
    if (ret != EMA_MP4_MUXED_OK)
    {
        return ret;
    }

    /** [ISO] Section 8.1.3: Progressive Download Information */
    if (muxer->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_PDIN)
    {
        muxer->moov_size_est += write_pdin_box(muxer->mp4_sink, muxer);
    }

    /** [CFF] Section 2.2.3: Base Location Box */
    if (muxer->usr_cfg_mux_ref->mux_cfg_flags & ISOM_MUXCFG_WRITE_BLOC)
    {
        snk->write(snk, (uint8_t *)muxer->bloc_atom.data, muxer->bloc_atom.size);
        muxer->moov_size_est += muxer->bloc_atom.size;
    }

    /** Create fragment info if needed */
    if (muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG)
    {
        if ((muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FRAGSTYLE_MASK) != ISOM_FRAGCFG_FRAGSTYLE_CCFF)
        {
            ret = create_fragment_lst(muxer, 1);
            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
        }
        else
        {
            ret = create_fragment_lst(muxer, 0);
            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
        }
        /** reset stsd-lst */
        list_it_init(muxer->tracks[0]->stsd_lst);
    }

    /** write 'moov' */
    write_moov_box(snk, muxer);
    msglog(NULL, MSGLOG_INFO, "moov end @ offset %" PRIi64 "\n", snk->position(snk)-1);

    /** [ISO] Section 8.16.3: Segment Index Box */
    if (muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_SIDX)
    {
        uint32_t track_idx;
        if (muxer->stream_num > 1)
        {
            msglog(NULL, MSGLOG_WARNING, "\nWARNING: writing of sidx boxes requested while muxing more than one track!"
                   " This is unsupported and might not work as expected!\n");
        }
        for (track_idx = 0; track_idx < 1; track_idx++)
        {
            uint32_t size;
            /** remember the start position of sidx box */
            sidx_pos[track_idx] = snk->position(snk);
            /** write sidx dummy box */
            ret = write_sidx_box(snk, muxer->tracks[track_idx], &size);
            sidx_size[track_idx] = size;

            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
            sidx_first_offset_written[track_idx] = 0;
        }
    }

    if (!(muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG))
    {
        /**to check if the 'stco'/'co64' selection is good or not */
        if (muxer->moov_size_est < snk->position(snk))
        {
            msglog(NULL, MSGLOG_WARNING, "\nWARNING: estimated moov size is too small\n");
            if (!muxer->co64_mode && snk->position(snk) + (16 + muxer->mdat_size) > (uint32_t)(-1))
            {
                msglog(NULL, MSGLOG_ERR, "ERROR: must use co64: use option -with 64 please\n");
                return EMA_MP4_MUXED_PARAM_ERR;
            }
        }

        /** write 'mdat' */
        ret = write_mdat_box(snk, muxer);

        /** rewrite chunk offsets */
        modify_stco_boxes(snk, muxer);
    }
    else if ((muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FRAGSTYLE_MASK) == ISOM_FRAGCFG_FRAGSTYLE_CCFF)
    {
        uint32_t track_ID;
        int32_t      referenced_size = 0;

        /** [DECE] CCFF specification type fragmented stream
         * for single thread only: write out all fragment after collected all es data
         * fragment or chunks
         */
        while ((track_ID = get_moof_ccff(muxer)))
        {
            offset_t moof_offset;
            
            int32_t      bytes_written;

            if (muxer->onwrite_next_frag_cb != NULL)
            {
                (*muxer->onwrite_next_frag_cb)(muxer->onwrite_next_frag_cb_instance);
            }

            moof_offset     = snk->position(snk);
            referenced_size += write_moof_box(snk, muxer, track_ID);

            ret = write_mdat_box_frag(snk, muxer, track_ID, &bytes_written);
            if (ret != EMA_MP4_MUXED_OK)
            {
                goto cleanup;
            }

            referenced_size += bytes_written;
            data_written    += bytes_written - 8;  /** only 'mdat' payload (without overhead for length and 4CC) */

            modify_base_data_offset(snk, muxer, track_ID);

            /** update referenced_size in 'sidx' box */
            if(muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_SIDX)
            {
                uint32_t track_idx = track_ID_2_track_idx(muxer, track_ID);
                if ((muxer->usr_cfg_mux_ref->dv_track_mode == SINGLE) || (track_idx))
                {
                    track_idx = 0;
                    if (!sidx_first_offset_written[track_idx])
                    {
                        update_sidx_box_offset(snk, sidx_pos[track_idx], sidx_size[track_idx], moof_offset);
                        sidx_first_offset_written[track_idx] = 1;
                    }
                    update_sidx_box(snk, muxer->tracks[track_idx], sidx_pos[track_idx], sidx_size[track_idx], referenced_size);
                    referenced_size = 0;
                    muxer->tracks[0]->frag_num++;
                }
            }

            if (muxer->progress_cb != NULL)
            {
                if (muxer->mdat_size > 0)
                {
                    muxer->progress_cb((float)(100.0 * data_written/(float)muxer->mdat_size ), muxer->progress_cb_instance);
                }
            }

            msglog(NULL, MSGLOG_INFO, "    seq#: %u\n", muxer->sequence_number);
            muxer->sequence_number++;
            
        }

        /** [ISO] Section 8.11.1: Meta Box; [CFF] DECE Optional Metadata */
        if (muxer->footer_meta_xml_data)
        {
            write_meta_box
            (
                snk,
                muxer->footer_meta_xml_data,
                muxer->footer_meta_hdlr_type,
                muxer->footer_meta_hdlr_name,
                muxer->footer_meta_items,
                muxer->footer_meta_item_sizes,
                muxer->num_footer_meta_items
            );
        }

        if (muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_MFRA)
        {
            write_mfra_box(snk, muxer);
        }
    }
    else if ((muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_FRAGSTYLE_MASK) == ISOM_FRAGCFG_FRAGSTYLE_DEFAULT)
    {
        /** fragment
         *  for single thread only: write out all fragment after collect all ES data
         *  fragment or chunks
         */

        uint32_t track_index = 0;
        uint32_t fragment_number = 0;
        
        for(track_index = 0; track_index <muxer->stream_num; track_index++)
        {
            if (!fragment_number)
            {
                fragment_number = muxer->tracks[track_index]->sidx_reference_count;
            }
            else
            {
                if (fragment_number != muxer->tracks[track_index]->sidx_reference_count)
                {
                    return EMA_MP4_MUXED_NO_SUPPORT;
                }
            }
        }
    
        while (fragment_number)
        {
            for(track_index = 0; track_index <muxer->stream_num; track_index++)
            {
                if(get_moof_by_TrackIndex(muxer, track_index))
                {
                    offset_t moof_offset;
                    int32_t      referenced_size;
                    int32_t      bytes_written;
                    uint32_t  trackID;

                    trackID = muxer->tracks[track_index]->track_ID;
                    if (muxer->onwrite_next_frag_cb != NULL)
                    {
                        (*muxer->onwrite_next_frag_cb)(muxer->onwrite_next_frag_cb_instance);
                    }

                    moof_offset     = snk->position(snk);
                    referenced_size = write_moof_box(snk, muxer, trackID);

                    ret = write_mdat_box_frag(snk, muxer, trackID, &bytes_written);
                    if (ret != EMA_MP4_MUXED_OK)
                    {
                        goto cleanup;
                    }

                    referenced_size += bytes_written;
                    data_written    += bytes_written - 8;  /** only 'mdat' payload (without overhead for length and 4CC) */

                    modify_base_data_offset(snk, muxer, trackID);

                    /** only update the first track's referenced_size in 'sidx' box */
                    if ((muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_SIDX) && (track_index == 0))
                    {
                        if (!sidx_first_offset_written[track_index])
                        {
                            update_sidx_box_offset(snk, sidx_pos[track_index], sidx_size[track_index], moof_offset);
                            sidx_first_offset_written[track_index] = 1;
                        }
                        update_sidx_box(snk, muxer->tracks[track_index], sidx_pos[track_index], sidx_size[track_index], referenced_size);       
                    }

                    if (muxer->progress_cb != NULL)
                    {
                        if (muxer->mdat_size > 0)
                        {
                            muxer->progress_cb((float)(100.0 * data_written/(float)muxer->mdat_size ), muxer->progress_cb_instance);
                        }
                    }
                    muxer->tracks[track_index]->frag_num++;
                    muxer->sequence_number++;
                }
            }

            fragment_number --;
        }

        if (muxer->usr_cfg_mux_ref->frag_cfg_flags & ISOM_FRAGCFG_WRITE_MFRA)
        {
            write_mfra_box(snk, muxer);
        }


    }
    else
    {
        /** unsupported fragmentation style */
        ret = EMA_MP4_MUXED_PARAM_ERR;
    }

    sink_flush_bits(snk);

 cleanup:
#ifdef _MSC_VER
    _rmtmp();
#endif

    return ret;
}

int
mp4_muxer_output_init_segment(mp4_ctrl_handle_t muxer, uint16_t *p_video_width, uint16_t *p_video_height)
{
    int32_t           ret = EMA_MP4_MUXED_OK;
    bbio_handle_t snk = muxer->mp4_sink;

    assert(muxer->usr_cfg_mux_ref->output_mode & EMA_MP4_FRAG);

    /** write 'ftyp' box */
    ret = mp4_muxer_output_hdrs(muxer);
    if (ret != EMA_MP4_MUXED_OK)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: call to mp4_muxer_output_hdrs() failed (%d)\n", ret);
        return ret;
    }

    /** final preparation for write out of moov */
    ret = setup_muxer_short(muxer, p_video_width, p_video_height);
    if (ret != EMA_MP4_MUXED_OK)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: call to setup_muxer_short() failed (%d)\n", ret);
        return ret;
    }

    /** write moov */
    write_moov_box(snk, muxer);
    msglog(NULL, MSGLOG_INFO, "moov end @ offset %" PRIi64 "\n", snk->position(snk)-1);

    sink_flush_bits(snk);

    return ret;
}

/** top level none media specific info, just ftyp for now */
int
mp4_muxer_output_hdrs (mp4_ctrl_handle_t hmuxer
                      )
{
    if (!hmuxer->mp4_sink)
    {
        return EMA_MP4_MUXED_IO_ERR;
    }

    hmuxer->moov_size_est = write_ftyp_box(hmuxer->mp4_sink, hmuxer); /** assuming the first box */

    return EMA_MP4_MUXED_OK;
}

int
mp4_muxer_output_segment_hdrs(mp4_ctrl_handle_t hmuxer)
{
    if (!hmuxer->mp4_sink)
    {
        return EMA_MP4_MUXED_IO_ERR;
    }

    write_styp_box(hmuxer->mp4_sink, hmuxer);

    return EMA_MP4_MUXED_OK;
}

void
mp4_muxer_destroy (mp4_ctrl_handle_t hmuxer
                  )
{
    uint32_t           track_idx;
    it_list_handle_t   it;
    atom_data_handle_t atom;

    if (!hmuxer)
    {
        return;
    }

    /** for mux */

    /** destory user data */
    it = it_create();
    it_init(it, hmuxer->moov_child_atom_lst);
    while ((atom = it_get_entry(it)))
    {
        FREE_CHK((void *)atom->data);
    }
    it_init(it, hmuxer->udta_child_atom_lst);
    while ((atom = it_get_entry(it)))
    {
        FREE_CHK((void *)atom->data);
    }
    it_destroy(it);
    list_destroy(hmuxer->moov_child_atom_lst);
    list_destroy(hmuxer->udta_child_atom_lst);
    list_destroy(hmuxer->next_track_lst);
    if (hmuxer->scratchbuf)
    {
        FREE_CHK(hmuxer->scratchbuf);
    }

    for (track_idx = 0; track_idx < hmuxer->stream_num; track_idx++)
    {
        track_handle_t track = hmuxer->tracks[track_idx];

        /** The parser is a reference to external memory. Do not destroy it. */
        track->parser = NULL;
        stream_destroy(track);
    }

    /** fragment */
    FREE_CHK(hmuxer->fn_out);
    FREE_CHK(hmuxer->cp_buf);
    if (hmuxer->buf_snk)
    {
        hmuxer->buf_snk->destroy(hmuxer->buf_snk);
    }

    FREE_CHK(hmuxer->info_fn);
    if (hmuxer->info_sink)
    {
        hmuxer->info_sink->destroy(hmuxer->info_sink);
    }

    FREE_CHK(hmuxer->major_brand);
    FREE_CHK(hmuxer->compatible_brands);

    FREE_CHK(hmuxer);
}

mp4_ctrl_handle_t
mp4_muxer_create (usr_cfg_mux_t *p_usr_cfg_mux
                 ,usr_cfg_es_t  *p_usr_cfg_ess
                 )
{
    mp4_ctrl_handle_t muxer;

    if (!p_usr_cfg_mux)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: no muxer config given to mp4_muxer_create()\n");
        return NULL;
    }

    muxer = (mp4_ctrl_handle_t)MALLOC_CHK(sizeof(mp4_ctrl_t));
    if (!muxer)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: no memory\n");
        return NULL;
    }
    memset(muxer, 0, sizeof(mp4_ctrl_t));

    muxer->destroy = mp4_muxer_destroy;

    muxer->timescale     = p_usr_cfg_mux->timescale;
    muxer->next_track_ID = 1;

    if (p_usr_cfg_mux->fix_cm_time)
    {
        muxer->creation_time = p_usr_cfg_mux->fix_cm_time;
    }
    else
    {
        muxer->creation_time = utc_sec_since_1970();
        muxer->creation_time += 0x7C25B080;    /** => since 1904  */
    }
    muxer->modification_time = muxer->creation_time;

    muxer->OD_profile_level       = p_usr_cfg_mux->OD_profile_level;
    muxer->scene_profile_level    = p_usr_cfg_mux->scene_profile_level;
    muxer->video_profile_level    = p_usr_cfg_mux->video_profile_level;
    muxer->audio_profile_level    = p_usr_cfg_mux->audio_profile_level;
    muxer->graphics_profile_level = p_usr_cfg_mux->graphics_profile_level;

    muxer->co64_mode = ((p_usr_cfg_mux->withopt & 0x1) == 0x1);
    muxer->usr_cfg_mux_ref = p_usr_cfg_mux;
    muxer->usr_cfg_ess_ref = p_usr_cfg_ess;

    /** fragment */
    muxer->sequence_number = 1; /** Based on ISO/IEC BMFF: start from 1 */

    muxer->progress_cb          = NULL;
    muxer->progress_cb_instance = NULL;

    muxer->onwrite_next_frag_cb      = NULL;
    muxer->onwrite_next_frag_cb_instance = NULL;

    return muxer;
}

void
mp4_muxer_set_progress_callback (mp4_muxer_handle_t   hmuxer
                                ,progress_callback_t  callback
                                ,void                *p_instance
                                )
{
    hmuxer->progress_cb          = callback;
    hmuxer->progress_cb_instance = p_instance;
}

void
mp4_muxer_set_onwrite_next_frag_callback(mp4_muxer_handle_t hmuxer
                                   ,onwrite_callback_t callback
                                   ,void *p_instance
                                  )
{
    hmuxer->onwrite_next_frag_cb = callback;
    hmuxer->onwrite_next_frag_cb_instance = p_instance;
}

void
mp4_muxer_set_sink (mp4_ctrl_handle_t hmuxer
                   ,bbio_handle_t     hsink
                   )
{
    hmuxer->mp4_sink = hsink;
}


bbio_handle_t
mp4_muxer_get_sink (mp4_ctrl_handle_t hmuxer
                   )
{
    return hmuxer->mp4_sink;
}

/** return track_ID */
uint32_t
mp4_muxer_add_track (mp4_ctrl_handle_t  hmuxer
                    ,parser_handle_t    hparser
                    ,usr_cfg_es_t      *p_usr_cfg_es
                    )
{
    char           *codingname;
    track_handle_t  track;
    trex_t         *ptrex;
    tfhd_t         *ptfhd;
    trun_t         *ptrun;

    if (!hmuxer || !hparser || !p_usr_cfg_es)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: NULL input to mp4_muxer_add_track()\n");
        return 0;
    }

    if (hmuxer->stream_num + 1 > MAX_STREAMS)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: no more track available\n");
        return 0;
    }

    codingname = get_codingname(hparser);
    if (!codingname)
    {
        msglog(NULL, MSGLOG_ERR, "stream %d: could not find codingname for parser\n", p_usr_cfg_es->es_idx);
        return EMA_MP4_MUXED_UNKNOW_ES;
    }

    track = (track_handle_t)MALLOC_CHK(sizeof(track_t));
    if (!track)
    {
        msglog(NULL, MSGLOG_ERR, "ERROR: no memory\n");
        return 0;
    }
    memset(track, 0, sizeof(track_t));

    if (p_usr_cfg_es->track_ID)
    {
        track->track_ID = p_usr_cfg_es->track_ID;
    }
    else
    {
        track->track_ID = hmuxer->next_track_ID;
    }
    /** check if track ID conflicting, track ID can be used by mp4 source */
    if (mp4_muxer_get_track(hmuxer, track->track_ID))
    {
        /** find unused track ID */
        uint32_t i;
        for (i = 1; i <= MAX_STREAMS; i++)
        {
            if (!mp4_muxer_get_track(hmuxer, i))
            {
                track->track_ID = i;
                break;
            }
        }
    }

    track->alternate_group = p_usr_cfg_es->alternate_group;
    track->flags = p_usr_cfg_es->force_tkhd_flags;
    FOURCC_ASSIGN(track->codingname, codingname);

    if (IS_FOURCC_EQUAL(codingname,"hvc1")) 
    {
        if (p_usr_cfg_es->sample_entry_name && IS_FOURCC_EQUAL(p_usr_cfg_es->sample_entry_name, "hvc1"))
        {
            FOURCC_ASSIGN(track->codingname, "hvc1");
            FOURCC_ASSIGN(hparser->dsi_name, "hvc1");
        }
        else
        {
            FOURCC_ASSIGN(track->codingname, "hev1");
            FOURCC_ASSIGN(hparser->dsi_name, "hev1");
        }
    }


    if (hmuxer->usr_cfg_mux_ref->dv_bl_non_comp_flag && hparser->stream_type == STREAM_TYPE_VIDEO)
    {
        if (IS_FOURCC_EQUAL(codingname,"avc1") || IS_FOURCC_EQUAL(codingname,"avc3"))
        {
            FOURCC_ASSIGN(track->codingname, "dvav");
            FOURCC_ASSIGN(hparser->dsi_name, "dvav");
        }
        if (p_usr_cfg_es->sample_entry_name && IS_FOURCC_EQUAL(p_usr_cfg_es->sample_entry_name, "dvh1")) {
            FOURCC_ASSIGN(track->codingname, "dvh1");
            FOURCC_ASSIGN(hparser->dsi_name, "dvh1");
        }
        else
        {
            FOURCC_ASSIGN(track->codingname, "dvhe");
            FOURCC_ASSIGN(hparser->dsi_name, "dvhe");
        }
    }


    track->codingname[4] = '\0';
    track->output_mode = hmuxer->usr_cfg_mux_ref->output_mode;
    track->hdlr_name   = p_usr_cfg_es->hdlr_name;

    /** track->edits to set from usr cfg */

    track->creation_time           = hmuxer->creation_time;
    track->modification_time       = hmuxer->modification_time;
    track->media_creation_time     = track->creation_time;
    track->media_modification_time = track->modification_time;
    if (p_usr_cfg_es->lang)
    {
        OSAL_STRNCPY(track->language, 4, p_usr_cfg_es->lang, 3);
        track->language[3] = '\0';
    }
    else
    {
        track->language[0] = '\0';
    }
    track->language_code = movie_iso639_to_language(track->language);

    if (hparser->stream_type == STREAM_TYPE_VIDEO && p_usr_cfg_es->enc_name)
    {
        OSAL_STRNCPY(track->codec_name, 32, p_usr_cfg_es->enc_name, 31);
        track->codec_name[31] = '\0';
    }
    else
    {
        track->codec_name[0] = '\0';
    }
    hparser->sd                = hmuxer->usr_cfg_mux_ref->sd; /** set sd from user config */
    hparser->sd_collision_flag = 0;                           /** reset sd collision flag */

    if (p_usr_cfg_es->warp_media_timescale != 0)
    {
        track->warp_media_timescale  = p_usr_cfg_es->warp_media_timescale;
        track->warp_media_timestamps = 1;
    }

    track->max_chunk_size = p_usr_cfg_es->chunk_span_size;
    if (!hmuxer->usr_cfg_mux_ref->chunk_span_time)
    {
        assert(track->max_chunk_size == 0); /** by consistency check */
        track->chunk_span_time = 0; /** no interleave */
        /** so !track->max_chunk_size && sample->dts < track->chunk_dts_top*/
        track->chunk_dts_top = (uint64_t)-1;
    }

    /** else since it will in media domain, wait until we know the scale */
    if (p_usr_cfg_es->chunk_span_size)
    {
        track->max_chunk_size = p_usr_cfg_es->chunk_span_size;
    }
    else
    {
        track->max_chunk_size = (uint64_t)-1;
    }

    if (hparser->stream_id == STREAM_ID_H264)
    {
        hmuxer->has_avc = TRUE;
    }
    else if (hparser->stream_id == STREAM_ID_MP4V)
    {
        hmuxer->has_mp4v = TRUE;
    }
    else if (hparser->stream_id == STREAM_ID_AAC)
    {
        hmuxer->has_mp4a = TRUE;
    }

    track->audio_channel_count    = 2;     /** Always 2 like stated in the Dolby File Spec */
    track->use_audio_channelcount = p_usr_cfg_es->use_audio_channelcount;
    if (p_usr_cfg_es->use_audio_channelcount && hparser->stream_type == STREAM_TYPE_AUDIO)
    {
        parser_audio_handle_t parser_audio = (parser_audio_handle_t)hparser;
        track->audio_channel_count = parser_audio->channelcount;
    }

    track->sidx_reference_count = p_usr_cfg_es->force_sidx_ref_count;

    /** pre alloc lst */
    track->dts_lst        = list_create(sizeof(idx_dts_t));
    track->cts_offset_lst = list_create(sizeof(count_value_t));
    track->sync_lst       = list_create(sizeof(idx_dts_t));

    track->edt_lst = list_create(sizeof(elst_entry_t));

    track->size_lst  = list_create(sizeof(count_value_t));
    track->chunk_lst = list_create(sizeof(chunk_t));

    track->stsd_lst = list_create(sizeof(idx_ptr_t));
    track->sdtp_lst = list_create(sizeof(sample_sdtp_t));
    track->trik_lst = list_create(sizeof(sample_trik_t));
    track->frame_type_lst = list_create(sizeof(sample_frame_type_t));
    track->subs_lst = list_create(sizeof(sample_subs_t));
    track->segment_lst = list_create(sizeof(frag_index_t));

#ifdef ENABLE_MP4_ENCRYPTION
    track->enc_info_lst     = list_create(sizeof(enc_subsample_info_t));
    track->enc_info_mdat_it = it_create();
#endif

    /** fragment: init of parser providing stream specific info */
    if (hparser->stream_type == STREAM_TYPE_VIDEO)
    {
        hmuxer->frag_ctrl_track_ID = track->track_ID;
    }
    ptrex                                   = &(track->trex);
    ptrex->track_ID                         = track->track_ID;
    ptrex->default_sample_description_index = p_usr_cfg_es->default_sample_description_index ? p_usr_cfg_es->default_sample_description_index : 1;

    ptfhd                           = &(track->tfhd);
    ptfhd->track_ID                 = track->track_ID;
    ptfhd->sample_description_index = ptrex->default_sample_description_index;
    ptfhd->tf_flags_override        = p_usr_cfg_es->force_tfhd_flags;

    ptrun = &(track->trun);
    ptrun->tr_flags_override = p_usr_cfg_es->force_trun_flags;

    track->first_trun_in_traf = TRUE;
    track->pos_lst            = list_create(sizeof(int64_t));       /** for no data tmp file case */
    track->size_it            = it_create();                        /** for tmp file case */
    track->tfra_entry_lst     = list_create(sizeof(tfra_entry_t));
    /** end of fragment */

    track->mp4_ctrl = hmuxer;
    track->parser   = hparser;
    track->es_idx   = p_usr_cfg_es->es_idx;

    hmuxer->tracks[hmuxer->stream_num] = track;
    hmuxer->stream_num++;
    hmuxer->next_track_ID++;
    if (track->track_ID + 1 > hmuxer->next_track_ID)
    {
        hmuxer->next_track_ID = track->track_ID + 1;
    }

    return track->track_ID;
}

int32_t
mp4_muxer_add_moov_child_atom (mp4_muxer_handle_t  hmuxer
                              ,const int8_t         *p_data
                              ,      uint32_t      size
                              ,      int8_t         *p_parent_box_type
                              ,      uint32_t      track_ID
                              )
{
    atom_data_handle_t atom;

    if (hmuxer->moov_child_atom_lst == NULL)
    {
        hmuxer->moov_child_atom_lst = list_create(sizeof(atom_data_t));
    }

    atom = (atom_data_handle_t)list_alloc_entry(hmuxer->moov_child_atom_lst);
    if (!atom)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }
    atom->data = (int8_t*)MALLOC_CHK(size);
    if (!atom->data)
    {
        list_free_entry(atom);
        return EMA_MP4_MUXED_NO_MEM;
    }
    memcpy((void *)atom->data, p_data, (size_t)size);
    atom->size = size;
    atom->parent_box_type[0] = p_parent_box_type[0];
    atom->parent_box_type[1] = p_parent_box_type[1];
    atom->parent_box_type[2] = p_parent_box_type[2];
    atom->parent_box_type[3] = p_parent_box_type[3];
    atom->track_ID = track_ID;
    list_add_entry(hmuxer->moov_child_atom_lst, atom);

    return EMA_MP4_MUXED_OK;
}

void
mp4_muxer_add_moov_ainf_atom (mp4_ctrl_handle_t  hmuxer
                             ,const int8_t        *p_data
                             ,      uint32_t     size
                             )
{
    hmuxer->moov_ainf_atom.data = (int8_t *)p_data;
    hmuxer->moov_ainf_atom.size = size;
}

void
mp4_muxer_add_bloc_atom (mp4_muxer_handle_t  hmuxer
                        ,const int8_t         *p_data
                        ,      uint32_t      size
                        )
{
    hmuxer->bloc_atom.data = (int8_t *)p_data;
    hmuxer->bloc_atom.size = size;
}

void
mp4_muxer_set_moov_meta_atom_data (mp4_muxer_handle_t   hmuxer
                                  ,const int8_t          *p_xml_data
                                  ,const int8_t          *p_hdlr_type
                                  ,const int8_t          *p_hdlr_name
                                  ,const int8_t * const  *pp_items
                                  ,const uint32_t      *p_item_sizes
                                  ,uint16_t             num_items
                                  )
{
    hmuxer->moov_meta_xml_data   = p_xml_data;
    hmuxer->moov_meta_hdlr_type  = p_hdlr_type;
    hmuxer->moov_meta_hdlr_name  = p_hdlr_name;
    hmuxer->moov_meta_items      = (const int8_t **)pp_items;
    hmuxer->moov_meta_item_sizes = p_item_sizes;
    hmuxer->num_moov_meta_items  = num_items;
}

void
mp4_muxer_set_footer_meta_atom_data (mp4_muxer_handle_t   hmuxer
                                    ,const int8_t          *p_xml_data
                                    ,const int8_t          *p_hdlr_type
                                    ,const int8_t          *p_hdlr_name
                                    ,const int8_t * const  *pp_items
                                    ,const uint32_t      *p_item_sizes
                                    ,uint16_t             num_items
                                    )
{
    hmuxer->footer_meta_xml_data   = p_xml_data;
    hmuxer->footer_meta_hdlr_type  = p_hdlr_type;
    hmuxer->footer_meta_hdlr_name  = p_hdlr_name;
    hmuxer->footer_meta_items      = (const int8_t **)pp_items;
    hmuxer->footer_meta_item_sizes = p_item_sizes;
    hmuxer->num_footer_meta_items  = num_items;
}

int32_t
mp4_muxer_add_udta_child_atom (mp4_muxer_handle_t  hmuxer
                              ,const int8_t         *p_data
                              ,      uint32_t      size
                              )
{
    atom_data_handle_t atom;

    if (hmuxer->udta_child_atom_lst == NULL)
    {
        hmuxer->udta_child_atom_lst = list_create(sizeof(atom_data_t));
    }

    atom       = (atom_data_handle_t)list_alloc_entry(hmuxer->udta_child_atom_lst);
    if (!atom)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }
    atom->data = (int8_t*)MALLOC_CHK(size);
    if (!atom->data)
    {
        list_free_entry(atom);
        return EMA_MP4_MUXED_NO_MEM;
    }
    memcpy((void *)atom->data, p_data, (size_t)size);
    atom->size = size;
    list_add_entry(hmuxer->udta_child_atom_lst, atom);
    return EMA_MP4_MUXED_OK;
}

void
mp4_muxer_set_OD_profile (mp4_ctrl_handle_t hmuxer
                         ,uint8_t           profile
                         )
{
    hmuxer->OD_profile_level = profile;
}

void
mp4_muxer_set_scene_profile (mp4_ctrl_handle_t hmuxer
                            ,uint8_t           profile
                            )
{
    hmuxer->scene_profile_level = profile;
}

void
mp4_muxer_set_audio_profile (mp4_ctrl_handle_t hmuxer
                            ,uint8_t           profile
                            )
{
    hmuxer->audio_profile_level = profile;
}

void
mp4_muxer_set_video_profile (mp4_ctrl_handle_t hmuxer
                            ,uint8_t           profile
                            )
{
    hmuxer->video_profile_level = profile;
}

void
mp4_muxer_set_graphics_profile (mp4_ctrl_handle_t hmuxer
                               ,uint8_t           profile
                               )
{
    hmuxer->graphics_profile_level = profile;
}

void
mp4_muxer_set_tfhd_sample_description_index (mp4_ctrl_handle_t hmuxer
                                            ,uint32_t          track_ID
                                            ,uint32_t          sample_description_index
                                            )
{
    track_handle_t track = mp4_muxer_get_track(hmuxer, track_ID);

    if (track)
    {
        track->tfhd.tf_flags_override        |= TF_FLAGS_SAMPLE_DESCRIPTION_INDEX;
        track->tfhd.sample_description_index  = sample_description_index;
    }
}

void
mp4_muxer_add_to_track_edit_list (track_handle_t htrack
                                 ,uint64_t       duration
                                 ,int64_t        media_time
                                 )
{
    const uint32_t movie_timescale   = htrack->mp4_ctrl->timescale;
    uint64_t       duration_movie_ts = (uint32_t)rescale_u64(duration, movie_timescale, htrack->media_timescale);
    elst_entry_t * entry;

    entry = (elst_entry_t *)list_alloc_entry(htrack->edt_lst);
    entry->segment_duration = duration_movie_ts; /** already converted to movie timescale */
    entry->media_time       = media_time;
    entry->media_rate       = 1;
    list_add_entry(htrack->edt_lst, entry);

    htrack->sum_track_edits += duration_movie_ts;
    if (duration_movie_ts > 0xFFFFFFFF ||
        media_time > 0x7FFFFFFF)
    {
        htrack->elst_version = 1;
    }
}

void
mp4_muxer_add_to_track_tfdt (track_handle_t  htrack
                             ,uint64_t       duration
                                 )
{
    htrack->dts_offset = duration;
}

uint32_t
mp4_muxer_get_track_bitrate (track_handle_t htrack
                            )
{
    uint32_t bitrate = 0;

    if (htrack->media_duration > 0)
    {
        bitrate = 8 * (uint32_t)((uint64_t)htrack->mdat_size * (uint64_t)htrack->media_timescale / (uint64_t)htrack->media_duration);
    }

    return bitrate;
}

#ifdef ENABLE_MP4_ENCRYPTION
static int32_t
update_enc_sample_info(track_handle_t track, uint32_t sample_size)
{
    enc_subsample_info_t *entry = (enc_subsample_info_t *)list_alloc_entry(track->enc_info_lst);
    track->encryptor->encrypt(track->encryptor, NULL, NULL, sample_size, &entry->enc_info);
    entry->subs_cnt = 0;
    list_add_entry(track->enc_info_lst, entry);
    track->encryptor->update_iv(track->encryptor);
    return 0;
}

static int32_t
update_enc_sample_info_video(track_handle_t track, uint32_t sample_size, int64_t pos)
{
    int32_t                subs_left = 1;
    uint32_t               subs_num  = 0;
    mp4_encryptor_handle_t encryptor = track->encryptor;
    parser_handle_t        parser    = track->parser;

    DPRINTF(NULL, "update_enc_sample_info_video(sample_size=%u, pos=%lu)\n", sample_size, pos);

    while (subs_left)
    {
        size_t                subs_size = sample_size;
        uint32_t              size;
        enc_subsample_info_t *entry     = (enc_subsample_info_t *)list_alloc_entry(track->enc_info_lst);
        int64_t               subs_pos  = pos;
        parser->get_subsample(parser, &subs_pos, subs_num++, &subs_left, NULL, &subs_size);
        size = (uint32_t)subs_size;
        /** only encrypt NALUs that are larger than 112 bytes, round to divisible by 16 block size */
        if (size >= 112)
        {
            int32_t is_dovi = 0;
            if (size < 1024) {
                uint8_t buf[1024];
                int32_t nalu_type = 0;
                int64_t pos2 = pos;
                int32_t nleft = 1;
                size_t sz = 1024;
                parser->get_subsample(parser, &pos2, subs_num-1, &nleft, buf, &sz);
                nalu_type = (buf[4] & 0x7e) >> 1;
                DPRINTF(NULL, "nalu type = 0x%02x\n", nalu_type);
                is_dovi = (nalu_type == 0x3e);
            }
            if (!is_dovi)
                size = 96 + (size & 0xf);
        }
        entry->enc_info.num_encrypted_bytes = (uint32_t)subs_size - size;
        encryptor->encrypt(encryptor, NULL, NULL, entry->enc_info.num_encrypted_bytes, &entry->enc_info);
        entry->enc_info.num_clear_bytes = size;
        entry->subs_cnt = subs_left;
        DPRINTF(NULL, "encrypting %u bytes, leaving %u bytes clear\n", entry->enc_info.num_encrypted_bytes, entry->enc_info.num_clear_bytes);
        list_add_entry(track->enc_info_lst, entry);
    }
    encryptor->update_iv(encryptor);
    return 0;
}

int
mp4_muxer_encrypt_track (track_handle_t         htrack
                        ,mp4_encryptor_handle_t hencryptor
                        )
{
    count_value_t *cv = NULL;

    htrack->encryptor  = hencryptor;
    htrack->senc_flags = 0;

    list_it_init(htrack->size_lst);
    list_it_init(htrack->pos_lst);

    while ((cv = list_it_get_entry(htrack->size_lst)))
    {
        uint32_t cnt = cv->count;
        while (cnt--)
        {
            if ((IS_FOURCC_EQUAL(htrack->codingname, "avc1"))
			 || (IS_FOURCC_EQUAL(codingname,"avc3"))
			 || (IS_FOURCC_EQUAL(htrack->codingname, "hvc1")) 
			 || (IS_FOURCC_EQUAL(htrack->codingname, "hev1")))
            {
                int64_t *pos = (int64_t *)list_it_get_entry(htrack->pos_lst);
                DPRINTF(NULL, "encrypting subsample\n");
                update_enc_sample_info_video(htrack, (uint32_t)cv->value, *pos);
                htrack->senc_flags = 0x2; /** use subsample encryption */
            }
            else
            {
                DPRINTF(NULL, "encrypting full sample\n");
                update_enc_sample_info(htrack, (uint32_t)cv->value);
            }
        }
    }

    return 0;
}
#endif

/** library version info */
static const mp4base_version_info mp4base_lib_version =
{
    MP4BASE_V_API,  /** API */
    MP4BASE_V_FCT,  /** Functionality */
    MP4BASE_V_MTNC, /** Maintenance  */
    "v1.1.0"
};

const mp4base_version_info*
mp4base_get_version (void)
{
    return &mp4base_lib_version;
}

