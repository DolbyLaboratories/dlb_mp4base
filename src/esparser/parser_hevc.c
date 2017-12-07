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
    @file parser_hevc.c
    @brief Implements a HEVC parser

    Based on ISO/IEC 14496-15:2010 PDAM
*/

#include "utils.h"
#include "list_itr.h"
#include "registry.h"
#include "dsi.h"
#include "parser.h"
#include "parser_hevc_dec.h"

#include <stdarg.h>


#define FISRT_DTS_DTS_IS_0 1 /** first dts = 0 */
#define TEST_DTS          (1 || CAN_TEST_DELTA_POC)
#define TEST_CTS          (1 && CAN_TEST_DELTA_POC)

#define MAX_DUMP_LINE_LEN 64

/** dumps the hevc es into file test_es.hevc so we can do a binary comparision: keep the ES untouched */
#define TEST_NAL_ES_DUMP    0

#define NAL_IN_AU_MAX   128 /** to simplify code assume a static structure */
typedef struct hevc_nal_loc_t_
{
    int64_t  off;      /** offset of nal after sc at es file */
    size_t   size;     /** nal size exclude sc */
    size_t   sc_size;  /** nal sc size */

    uint8_t *buf_emb;  /** != NULL: the nal content is embedded */
} hevc_nal_loc_t;

typedef struct hevc_au_nals_t_
{
    int32_t   nal_idx;
    hevc_nal_loc_t nal_locs[NAL_IN_AU_MAX];
} hevc_au_nals_t;

typedef struct hevc_nal_t_
{
    bbio_handle_t ds;

    uint8_t *buffer;         /** buffer loaded with es for parsing */
    size_t   buf_size;       /** its size */
    size_t   data_size;      /** data in it */
    int32_t  sc_off;         /** start code offset */
    int32_t  sc_off_next;    /** next sc offset */

    offset_t  off_file;      /** offset of nal in file(ds) */
    uint8_t  *nal_buf;       /** point to the start of a nal defined by [sc_off, sc_off_next] */
    size_t    nal_size;      /** its size, including sc */
    size_t    sc_size;
    BOOL      nal_complete;  /** if get a complete nal */

    /** to aid parsing */
    uint8_t       *tmp_buf;
    uint32_t       tmp_buf_size;
    uint32_t       tmp_buf_data_size;
    bbio_handle_t  tmp_buf_bbi;
} hevc_nal_t;
typedef hevc_nal_t *hevc_nal_handle_t;

struct parser_hevc_t_
{
    PARSER_VIDEO_BASE;

    int keep_all_nalus;        /** 0: only keep that nalus in mdat box which are not defined in track header */
                               /** 1: keep all nalus in mdat box */

    dsi_handle_t dsi_enh;

    hevc_nal_t    nal;         /** nal buf and current nal info */
    hevc_au_nals_t  au_nals;   /** the composing nals of au */
    hevc_au_nals_t  dv_au_nals;/** dolby vision composing nals of au */
    bbio_handle_t tmp_bbo;     /** the output handle of file */
    bbio_handle_t tmp_bbi;     /** the input handle of file */

    hevc_decode_t dec;         /** current decoder status */
    hevc_decode_t dec_el;      /** dolby vision el decoder status */

    uint32_t sample_size;
    uint32_t au_num;
    uint32_t au_ticks;

    uint32_t vps_num, sps_num, pps_num, sps_ext_num;
    uint32_t sei_num;

    /** keep au timing info up to MinCts when SeiPicTiming is available */
    BOOL     bMinCtsKn;
    int32_t  i32PocMin;
    uint32_t u32MinCts;
#define CO_BUF_SIZE 4
    uint32_t au32CoTc[CO_BUF_SIZE];  /** cts offset in field# */

#if TEST_DTS
    int64_t delta_dts, dts_pre;
#endif

    list_handle_t hevc_cts_offset_lst;

    /** validation */
    uint32_t validation_flags;
    uint32_t last_idr_pos;
    uint32_t max_idr_dist;
};

typedef struct parser_hevc_t_ parser_hevc_t;
typedef parser_hevc_t  *parser_hevc_handle_t;

typedef struct idx_value_t_
{
    uint32_t idx;
    uint64_t value;
} idx_value_t;


static void
update_idx_value_lst(list_handle_t lst,
                   uint32_t      idx,
                   uint64_t      value)
{
    idx_value_t *idx_value = (idx_value_t *)list_alloc_entry(lst);

    idx_value->idx = idx;
    idx_value->value = value;
    list_add_entry(lst, idx_value);
}

/**  return the offset into buf where the sc is
 *  sc_next == TRUE: skip the starting sc
 *  return -1 for no sc found
 */
#define NAL_START_CODE              0x000001
static int32_t
find_sc_off(uint8_t *buf, size_t buf_size, BOOL sc_next)
{
    uint32_t val;
    uint8_t *buf0    = buf;
    uint8_t *buf_top = buf + buf_size;

    if (buf_size < 4)
    {
        /** 4: sc at least 3 bytes + 1 nal hdr */
        return -1;
    }

    /** skip current start code if search for next sc */
    if (sc_next)
    {
        if (*buf++ == 0 && *buf++ == 0 &&
            (*buf == 1 || (*buf++ == 0 && *buf == 1)))
        {
            buf++;
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, "sc miss-match\n");
            buf = buf0;  /** to keep going */
        }
    }

    /** get next current start code */
    val = 0xffffffff;
    while (buf < buf_top)
    {
        val <<= 8;
        val |= *buf++;
        if ((val & 0x00ffffff) == NAL_START_CODE)
        {
            if (val == NAL_START_CODE)
            {
                return (int32_t)(buf - buf0 - 4);
            }
            return (int32_t)(buf - buf0 - 3);
        }
    }

    return -1;
}

/** assuming sc_off_next point to next(now of interest) nal */
static BOOL
get_a_nal(hevc_nal_handle_t nal)
{
    int32_t sc_off_next, off0;
    size_t bytes_read, bytes_avail;

    /** next nal starts at where last one end */
    nal->sc_off = nal->sc_off_next;
    nal->off_file += nal->nal_size;

    bytes_avail = nal->data_size - nal->sc_off;
    sc_off_next = find_sc_off(nal->buffer + nal->sc_off, bytes_avail, TRUE);
      /** TRUE: skip the start code of current nal */
    if (sc_off_next >= 0)
    {
        /** already got a complet nal in buf */
        nal->sc_off_next = nal->sc_off + sc_off_next;

        nal->nal_buf = nal->buffer + nal->sc_off;
        nal->nal_size = sc_off_next;
        nal->sc_size = (nal->nal_buf[2] == 1) ? 3 : 4;
        nal->nal_complete = TRUE;
        return TRUE;
    }

    if (bytes_avail >= 2048)
    {
        /** get enough to parse */
        nal->nal_buf      = nal->buffer + nal->sc_off;
        nal->nal_size     = bytes_avail;                     /** data got so far */
        nal->sc_size      = (nal->nal_buf[2] == 1) ? 3 : 4;
        nal->nal_complete = FALSE;
        return TRUE;
    }

    /** need more data */
    /** discard data before sc_off. move data to offset 0, leave room to load more data */
    nal->data_size = bytes_avail;
    if (nal->data_size)
    {
        memmove(nal->buffer, nal->buffer + nal->sc_off, bytes_avail);
    }
    nal->sc_off = 0;
    nal->nal_buf = nal->buffer;
    /** search starts at right position to avoid double search and skip current nal sc */
    if (nal->data_size > 4)
    {
        /** already searched up to data_size. off0 > 1. -3: may got 3 0s */
        off0 = (int32_t)nal->data_size - 3;
    }
    else if (nal->data_size > 2)
    {
        /** skip 2 0s */
        off0 = 2;
    }
    else
    {
        off0 = 0;  /** only at the first or after last nal */
    }

    /** load */
    bytes_read = nal->ds->read(nal->ds, nal->buffer + nal->data_size, nal->buf_size - nal->data_size);
    nal->sc_size = (nal->nal_buf[2] == 1) ? 3 : 4;
    /** (1) init will report EOES if total data size < 4.
     *  (2) if reach EOES, retrun FALSE and sc_size does not matter
     */
    if (bytes_read == 0)
    {
        if (nal->data_size)
        {
            /** end of source and has last nal */
            nal->sc_off_next = (int32_t)nal->data_size;

            nal->nal_size     = nal->sc_off_next;
            nal->nal_complete = TRUE;
            return TRUE;
        }
        nal->nal_complete = TRUE;
        return FALSE;  /** nal->data_size == 0 and bytes_read == 0: done */
    }

    /** try search again */
    nal->data_size += bytes_read;
    sc_off_next = find_sc_off(nal->buffer + off0, nal->data_size - off0, off0 == 0);
    if (sc_off_next >= 0)
    {
        /** got it ! */
        nal->sc_off_next = off0 + sc_off_next;

        nal->nal_size     = nal->sc_off_next;
        nal->nal_complete = TRUE;
        return TRUE;
    }

    if (nal->data_size != nal->buf_size)
    {
        /** buf not full: end of source and has last nal */
        nal->sc_off_next = (int32_t)nal->data_size;

        nal->nal_size     = nal->sc_off_next;
        nal->nal_complete = TRUE;
        return TRUE;  /** done */
    }

    /** return TRUE when we get enough nal data to parse or close to end of file */
    nal->nal_size     = nal->data_size;  /** data got so far */
    nal->nal_complete = FALSE;
    return TRUE;
}

/** find out the end of nal and nal size if !nal_complete */
static BOOL
skip_the_nal(hevc_nal_handle_t nal)
{
    size_t  bytes_read;
    int32_t sc_off_next;

    if (nal->nal_complete)
    {
        return FALSE;  /** already done */
    }

    assert(nal->nal_size >= 2048);
    do {
        /** keep the last three byte and load more data */
        nal->buffer[0] = nal->buffer[nal->data_size - 3];
        nal->buffer[1] = nal->buffer[nal->data_size - 2];
        nal->buffer[2] = nal->buffer[nal->data_size - 1];
        bytes_read = nal->ds->read(nal->ds, nal->buffer + 3, nal->buf_size - 3);

        nal->data_size = 3 + bytes_read;  /** data in buffer */
        if (!bytes_read)
        {
            nal->sc_off_next = 3;  /** fake a sc at offset 3 */
            /** nal_size unchanged: up to end of file */
            return TRUE;
        }

        sc_off_next = find_sc_off(nal->buffer, bytes_read + 3, FALSE);
        if (sc_off_next >= 0)
        {
            nal->sc_off_next  = sc_off_next;
            nal->nal_size    += sc_off_next - 3;  /** -3 => each byte count once */
            return TRUE;
        }

        nal->nal_size += bytes_read;
    }
    while (TRUE);
}

/** Return true if new SPS or PPS inside nal will trigger writing of new sample description box
    because there is already a SPS or PPS with same id but different content in plist. */
static BOOL
ps_list_is_there_collision(list_handle_t *plist, uint8_t id, hevc_nal_handle_t nal)
{
    it_list_handle_t it = NULL;
    buf_entry_t *    entry = NULL;
    BOOL             ret = FALSE;

    if (!*plist)
    {
        /** List does not have content at all */
        return FALSE;
    }

    it = it_create();
    it_init(it, *plist);
    while ((entry = it_get_entry(it)) && entry->id != id)
        continue;

    if (entry)
    {
        /** Do existing entries and the new one have the same content? */
        if (entry->size == nal->nal_size - nal->sc_size &&
           !memcmp(entry->data, nal->nal_buf + nal->sc_size, entry->size))
        {
            /** we get here if the NALs are identical */
            ret = FALSE;
        }
        else
        {
            /** same ID but different content (spliced stream) */
            ret = TRUE;
        }
    }

    it_destroy(it);
    return ret;
}

/** Returns true if SPS/PPS should be copied in the stream */
static BOOL
ps_list_update(parser_hevc_handle_t parser, list_handle_t *plist, uint8_t id, hevc_nal_handle_t nal, uint32_t *sample_flag)
{
    it_list_handle_t it = it_create();
    buf_entry_t *    entry;
    BOOL             ret = TRUE;

    if (!*plist)
    {
        *plist = list_create(sizeof(buf_entry_t));
    }

    it_init(it, *plist);
    while ((entry = it_get_entry(it)) && entry->id != id)
    {
        continue;
    }

    if (entry)
    {
        /** Do existing and new entry have the same content? */
        if (entry->size == nal->nal_size - nal->sc_size &&
           !memcmp(entry->data, nal->nal_buf + nal->sc_size, entry->size))
        {
            /** we get here if the NALs are identical */
            if (parser->keep_all_nalus)
            {
                ret = TRUE;
            }
            else
            {
                ret = FALSE;
            }
        }
        else
        {
            /** same ID but different content (spliced stream) */
            /** copy content in plist only */
            if (entry->size != (size_t)(nal->nal_size - nal->sc_size))
            {
                /** we don't have enough space in this entry */
                FREE_CHK(entry->data);
                entry->size = (size_t)(nal->nal_size - nal->sc_size);
                entry->data = (uint8_t *)MALLOC_CHK(entry->size);
            }
            memcpy(entry->data, nal->nal_buf + nal->sc_size, entry->size);
            if (parser->keep_all_nalus)
            {
                ret = TRUE;
            }
            else if (parser->sd == 0)
            {
                /** single sample description entry */
                msglog(NULL, MSGLOG_ERR, "Error: Multiple Sample Descriptions necessary but not allowed!\n");
                parser->sd_collision_flag = 1;
            }
            else if (parser->sd == 1)
            {
                /** multiple sample description entries */
                ret = FALSE;
            }
        }
    }
    else
    {
        /** new entry in list */
        entry       = list_alloc_entry(*plist);
        entry->id   = id;
        entry->size = nal->nal_size - nal->sc_size;
        entry->data = (uint8_t *)MALLOC_CHK(entry->size);
        memcpy(entry->data, nal->nal_buf + nal->sc_size, entry->size);

        list_add_entry(*plist, entry);

        if (sample_flag)
        {
            *sample_flag |= SAMPLE_NEW_SD;
        }

        if (parser->keep_all_nalus)
        {
            ret = TRUE;
        }
        else
        {
            ret = FALSE;
        }
    }

    it_destroy(it);
    return ret;
}


/** Reads the type or value of sei */
static int
read_sei_tv(bbio_handle_t ds, uint32_t *size, uint32_t *sei_value)
{
    uint8_t  u8;

    *sei_value = 0;
    *size = 0;
    if (src_rd_u8(ds, &u8) != 0)
    {
        return 1;
    }
    while ( u8 == 0xff)
    {
        *sei_value += 255;
        (*size)++;
        if (src_rd_u8(ds, &u8) != 0)
        {
            return 1;
        }
    }
    *sei_value += u8;
    (*size)++;

    return 0;
}

static void
get_colr_info(parser_hevc_handle_t parser_hevc, hevc_decode_t * context)
{
    parser_hevc->colour_primaries = (uint8_t)context->s_vui.i_colour_primaries;
    parser_hevc->transfer_characteristics = (uint8_t)context->s_vui.i_transfer_characteristics;
    parser_hevc->matrix_coefficients = (uint8_t)context->s_vui.i_matrix_coefficients;
}

static void
timing_info_update(parser_hevc_handle_t parser_hevc, hevc_decode_t * context)
{
    if (parser_hevc->ext_timing.override_timing)
    {
        parser_hevc->num_units_in_tick           = parser_hevc->ext_timing.num_units_in_tick;
        parser_hevc->time_scale                   = parser_hevc->ext_timing.time_scale;
        parser_hevc->au_ticks                   = parser_hevc->num_units_in_tick;

        if (!context->s_vui.b_timing_info_present_flag && !context->s_vps.b_vps_timing_info_present_flag)  
        {
            msglog(NULL, MSGLOG_NOTICE, "No timing info found in ES, so we just use user's setting! \n");
        }
        else
        {
            msglog(NULL, MSGLOG_NOTICE, 
                "Found timing info in ES and user want to set a new one, so we use user's setting! \n");
        }
    }
    else
    {
        if (context->s_vui.b_timing_info_present_flag ||context->s_vps.b_vps_timing_info_present_flag)  
        {
            if (context->s_vui.b_timing_info_present_flag)
            {
                parser_hevc->num_units_in_tick           = context->s_vui.i_num_units;
                parser_hevc->time_scale                   = context->s_vui.i_time_scale;
                parser_hevc->au_ticks                   = parser_hevc->num_units_in_tick;
            }
            if (context->s_vps.b_vps_timing_info_present_flag)
            {
                parser_hevc->num_units_in_tick           = context->s_vps.ui_vps_num_units_in_tick;
                parser_hevc->time_scale                   = context->s_vps.ui_vps_time_scale;
                parser_hevc->au_ticks                   = parser_hevc->num_units_in_tick;
            }
            msglog(NULL, MSGLOG_NOTICE, "Timing info found in ES, so we just use it! \n");
        }
        else
        {
            parser_hevc->num_units_in_tick           = parser_hevc->ext_timing.num_units_in_tick;
            parser_hevc->time_scale                   = parser_hevc->ext_timing.time_scale;
            parser_hevc->au_ticks                   = parser_hevc->num_units_in_tick;

            msglog(NULL, MSGLOG_NOTICE, 
                "No timing info found in ES and no user's setting, we just use a default timing(30 fps)! \n");
        }
    }

    parser_hevc->framerate = parser_hevc->time_scale / parser_hevc->num_units_in_tick;
    {
        unsigned actual_height = 0;
        uint32_t level = 0;
        if (parser_hevc->height == 544)
        {
            actual_height = 540;
        }
        else
        {
            actual_height = parser_hevc->height;
        }
        
        level  = parser_hevc->width * actual_height * parser_hevc->framerate;

        if ((parser_hevc->dv_el_nal_flag== 0) && (parser_hevc->dv_rpu_nal_flag == 1) 
            && (parser_hevc->ext_timing.ext_dv_profile != 5)
            && (parser_hevc->ext_timing.ext_dv_profile != 8)) 
        {
            level = level * 4;
        }

        if (level <= 1280*720*24)
        {
            parser_hevc->dv_level = 1;
        }
        else if (level<= 1280*720*30)
        {
            parser_hevc->dv_level = 2;
        }
        else if (level <= 1920*1080*24)
        {
            parser_hevc->dv_level = 3;
        }
        else if (level <= 1920*1080*30)
        {
            parser_hevc->dv_level = 4;
        }
        else if (level <= 1920*1080*60)
        {
            parser_hevc->dv_level = 5;
        }
        else if (level <= 3840*2160*24)
        {
            parser_hevc->dv_level = 6;
        }
        else if (level <= 3840*2160*30)
        {
            parser_hevc->dv_level = 7;
        }
        else if (level <= 3840*2160*48)
        {
            parser_hevc->dv_level = 8;
        }
        else if (level <= 3840*2160*60)
        {
            parser_hevc->dv_level = 9;
        }
    }

}
#define WR_PREFIX(snk) 0
#define RD_PREFIX(src) 0

static int
save_au_nals_info(hevc_au_nals_t *au_nals, mp4_sample_handle_t sample, bbio_handle_t snk)
{
    hevc_nal_loc_t *nal_loc, *nal_loc_end;

    sample->pos = snk->position(snk);  /** into the nal info file */
    if (sample->data)
    {
        /** data=0 for nal info type sample data */
        FREE_CHK(sample->data);
        sample->data = 0;
    }

    assert(au_nals->nal_idx);
    /** save sample's au structure and location at es file */
    if (WR_PREFIX(snk) != 0)
    {
        return EMA_MP4_MUXED_WRITE_ERR;
    }

    sink_write_u32(snk, au_nals->nal_idx);  /** # of nal in au */

    nal_loc = au_nals->nal_locs;
    nal_loc_end = nal_loc + au_nals->nal_idx;
    while (nal_loc < nal_loc_end)
    {
        sink_write_u64(snk, nal_loc->off);                   /** nal body at es file. -1 embedded */
        sink_write_u32(snk, (uint32_t)nal_loc->size);        /** nal body size */
        sink_write_u8(snk,  (uint8_t)nal_loc->sc_size);      /** nal sc size */
        if (nal_loc->buf_emb)
        {
            /** save nal body only */
            snk->write(snk, nal_loc->buf_emb, nal_loc->size);
            FREE_CHK(nal_loc->buf_emb);
            nal_loc->buf_emb = 0;
        }
        nal_loc++;
    }
    au_nals->nal_idx = 0;

    return EMA_MP4_MUXED_OK;
}



#if TEST_DTS
/** verify delta dts is a constant */
static void
verify_dts(parser_hevc_handle_t parser_hevc, mp4_sample_handle_t sample)
{
    sample;
    parser_hevc;
}
#else
#define verify_dts(parser_avc, sample);
#endif

#if TEST_CTS
/** verify delta cts is a constant */
static void
verify_cts(parser_avc_handle_t parser_avc, mp4_sample_handle_t sample)
{
    if (parser_avc->dec.IDR_pic || parser_avc->au_num == 0)
    {
        if (parser_avc->au_num == 0)
        {
            apoc_init(parser_avc->p_cts_apoc);
        }
        apoc_flush(parser_avc->p_cts_apoc);
        if ( parser_avc->dec.active_sps->frame_mbs_only_flag)
        {
          apoc_set_num_reorder_au(parser_avc->p_cts_apoc, parser_avc->dec.active_sps->num_reorder_frames);
        }
        else
        {
          apoc_set_num_reorder_au(parser_avc->p_cts_apoc, parser_avc->dec.active_sps->num_reorder_frames<<1);
        }
    }

    apoc_add(parser_avc->p_cts_apoc, (int)sample->cts, FALSE);
}
#else
#define verify_cts(parser_avc, sample);
#endif

#if TEST_DTS || TEST_CTS
static void
verify_ts_report(parser_hevc_handle_t parser_avc)
{
    msglog(NULL, MSGLOG_INFO, "\n");
#if TEST_DTS
    msglog(NULL, MSGLOG_INFO, "  delta_dts %" PRIi64 , parser_avc->delta_dts);
#endif

#if TEST_CTS
    apoc_flush(parser_avc->p_cts_apoc);
    msglog(NULL, MSGLOG_INFO, "  delta_cts %ld", apoc_get_delta_poc(parser_avc->p_cts_apoc));
#endif
    msglog(NULL, MSGLOG_INFO, "\n");
}
#else
#define verify_ts_report(parser_avc)
#endif

static void
dsi_update(dsi_hevc_handle_t dsi_hevc, hevc_decode_t * context)
{
    int i = 0;
    uint32_t temp = 0;

    mp4_dsi_hevc_handle_t mp4ff_dsi = (mp4_dsi_hevc_handle_t)dsi_hevc;

    mp4ff_dsi->configurationVersion = 1;
    mp4ff_dsi->profile_space = (uint8_t)context->as_protile[0].i_profile_space;
    mp4ff_dsi->tier_flag = context->as_protile[0].b_tier;
    mp4ff_dsi->profile_idc = (uint8_t)context->as_protile[0].i_profile;
    for (i = 0; i < 32; i++)
    {
        if (context->as_protile[0].b_profile_compat[i] == true)
            temp |= (1 << (31-i));
    }


    mp4ff_dsi->profile_compatibility_indications = temp;
    
    mp4ff_dsi->progressive_source_flag = context->as_protile[0].b_general_progressive_source;     
    mp4ff_dsi->interlaced_source_flag = context->as_protile[0].b_general_interlaced_source;      
    mp4ff_dsi->non_packed_constraint_flag = context->as_protile[0].b_general_non_packed_constraint; 
    mp4ff_dsi->frame_only_constraint_flag = context->as_protile[0].b_general_frame_only_constraint; 

    mp4ff_dsi->constraint_indicator_flags = 0; /** currently this info is just set to 0 */
    mp4ff_dsi->level_idc = (uint8_t)context->as_protile[0].i_level;
    mp4ff_dsi->min_spatial_segmentation_idc = (uint8_t)context->s_vui.i_min_spatial_segmentation_idc;
    mp4ff_dsi->parallelismType = 0;           /** currently spec don't mention how to set this value */
    mp4ff_dsi->chromaFormat = context->as_sps->i_chroma_format_idc;
    mp4ff_dsi->bitDepthLumaMinus8 = context->as_sps[0].i_bit_depth_luma - 8;
    mp4ff_dsi->bitDepthChromaMinus8 = context->as_sps[0].i_bit_depth_chroma - 8;

    mp4ff_dsi->AvgFrameRate = 0;      /** currently this info is just set to 0; */
    mp4ff_dsi->constantFrameRate = 0; /** currently this info is just set to 0; */
    mp4ff_dsi->numTemporalLayers = context->as_sps[0].i_max_temporal_layers;
    mp4ff_dsi->temporalIdNested = context->as_sps[0].b_temporal_id_nesting;
    mp4ff_dsi->lengthSizeMinusOne = 3;
    mp4ff_dsi->numOfArrays = 0;

}

static int 
incr_nal_idx( hevc_au_nals_t * au_nals )
{
    au_nals->nal_idx++;
    if (au_nals->nal_idx >= NAL_IN_AU_MAX)
    {
        msglog(NULL, MSGLOG_DEBUG, "\n Invalid number of nal indexes\n");
        assert(NULL);
        return EMA_MP4_MUXED_BUGGY;
    }

    return EMA_MP4_MUXED_OK;
}

/** Creates a new entry in parser->dsi_lst and copy content from current dsi in the new dsi entry.
 * After copying the former "new" dsi will be the "current" dsi to be worked with from there on.
 * Returns error code.
 */
static int
parser_hevc_clone_dsi(parser_handle_t parser) 
{
    /** Create new entry in stsd list */
    dsi_handle_t    new_dsi = parser->dsi_create(parser->dsi_type);
    dsi_handle_t* p_new_dsi = NULL;

    mp4_dsi_hevc_handle_t     mp4ff_dsi = (mp4_dsi_hevc_handle_t)parser->curr_dsi;
    mp4_dsi_hevc_handle_t new_mp4ff_dsi = (mp4_dsi_hevc_handle_t)new_dsi;

    buf_entry_t*     entry = NULL;
    buf_entry_t* new_entry = NULL;
    it_list_handle_t it = NULL;
    uint32_t i = 0;
    
    if (!new_dsi)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }

    p_new_dsi = (dsi_handle_t*)list_alloc_entry(parser->dsi_lst);
    if (!p_new_dsi)
    {
        new_dsi->destroy(new_dsi);
        return EMA_MP4_MUXED_NO_MEM;
    }

    it = it_create();
    if (!it)
    {
        list_free_entry(p_new_dsi);
        new_dsi->destroy(new_dsi);
        return EMA_MP4_MUXED_NO_MEM;
    }

    *p_new_dsi = new_dsi;

    /** First copy content of dsi_hevc_t struct itself (stream id, profile indications, etc.) */
    memcpy(new_dsi, parser->curr_dsi, sizeof(dsi_hevc_t));
    /** Copy VPS list */
    if (mp4ff_dsi->vps_lst)      /** Is there even anything to copy? */
    {
        /** New list not existent yet */
        new_mp4ff_dsi->vps_lst = list_create(sizeof(buf_entry_t));

         /** Copy entries one by one
           (lists are not organized in one memory block. So no use of memcpy for whole list here) */

         /** Copy VPS list */
        for (i = 0; i < list_get_entry_num(mp4ff_dsi->vps_lst); i++)
        {
            it_init(it, mp4ff_dsi->vps_lst);
            entry = (buf_entry_t*)it_get_entry(it);
            if (!entry)
                continue;

            new_entry       = (buf_entry_t*)list_alloc_entry(new_mp4ff_dsi->vps_lst);
            if (!new_entry)
            {
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_READ_ERR;
            }
            new_entry->id   = entry->id;
            new_entry->size = entry->size;
            new_entry->data = (uint8_t *)MALLOC_CHK(entry->size);
            if (!new_entry->data)
            {
                list_free_entry(new_entry);
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_NO_MEM;
            }
            memcpy(new_entry->data, entry->data, new_entry->size);

            list_add_entry(new_mp4ff_dsi->vps_lst, new_entry);
        }
    }

    /** Copy PPS list */
    if (mp4ff_dsi->pps_lst)
    {
        /** New list not existent yet */
        new_mp4ff_dsi->pps_lst = list_create(sizeof(buf_entry_t));

        /** Copy entries one by one
           (lists are not organized in one memory block. So no use of memcpy for whole list here) */
        for (i = 0; i < list_get_entry_num(mp4ff_dsi->pps_lst); i++)
        {
            it_init(it, mp4ff_dsi->pps_lst);
            entry = (buf_entry_t*)it_get_entry(it);
            if (!entry)
                continue;

            new_entry       = (buf_entry_t*)list_alloc_entry(new_mp4ff_dsi->pps_lst);
            if (!new_entry)
            {
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_READ_ERR;
            }
            new_entry->id   = entry->id;
            new_entry->size = entry->size;
            new_entry->data = (uint8_t *)MALLOC_CHK(entry->size);
            if (!new_entry->data)
            {
                list_free_entry(new_entry);
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_NO_MEM;
            }
            memcpy(new_entry->data, entry->data, new_entry->size);

            list_add_entry(new_mp4ff_dsi->pps_lst, new_entry);
        }
    }

    /** Copy SPS list */
    if (mp4ff_dsi->sps_lst)
    {
        new_mp4ff_dsi->sps_lst = list_create(sizeof(buf_entry_t));

        for (i = 0; i < list_get_entry_num(mp4ff_dsi->sps_lst); i++)
        {
            it_init(it, mp4ff_dsi->sps_lst);
            entry = (buf_entry_t*)it_get_entry(it);
            if (!entry)
                continue;

            new_entry       = (buf_entry_t*)list_alloc_entry(new_mp4ff_dsi->sps_lst);
            if (!new_entry)
            {
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_READ_ERR;
            }
            new_entry->id   = entry->id;
            new_entry->size = entry->size;
            new_entry->data = (uint8_t *)MALLOC_CHK(entry->size);
            if (!new_entry->data)
            {
                list_free_entry(new_entry);
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_NO_MEM;
            }
            memcpy(new_entry->data, entry->data, new_entry->size);

            list_add_entry(new_mp4ff_dsi->sps_lst, new_entry);
        }
    }

    /** Copy rest of mp4_dsi_hevc_t struct */
    new_mp4ff_dsi->configurationVersion               = mp4ff_dsi->configurationVersion;
    new_mp4ff_dsi->profile_space                      = mp4ff_dsi->profile_space;
    new_mp4ff_dsi->tier_flag                          = mp4ff_dsi->tier_flag;
    new_mp4ff_dsi->profile_idc                        = mp4ff_dsi->profile_idc;
    new_mp4ff_dsi->profile_compatibility_indications  = mp4ff_dsi->profile_compatibility_indications;
    new_mp4ff_dsi->constraint_indicator_flags         = mp4ff_dsi->constraint_indicator_flags;
    new_mp4ff_dsi->level_idc                          = mp4ff_dsi->level_idc;
    new_mp4ff_dsi->min_spatial_segmentation_idc       = mp4ff_dsi->min_spatial_segmentation_idc;
    new_mp4ff_dsi->parallelismType                    = mp4ff_dsi->parallelismType;
    new_mp4ff_dsi->chromaFormat                       = mp4ff_dsi->chromaFormat;
    new_mp4ff_dsi->bitDepthChromaMinus8               = mp4ff_dsi->bitDepthChromaMinus8;
    new_mp4ff_dsi->bitDepthLumaMinus8                 = mp4ff_dsi->bitDepthLumaMinus8;
    new_mp4ff_dsi->AvgFrameRate                       = mp4ff_dsi->AvgFrameRate;
    new_mp4ff_dsi->constantFrameRate                  = mp4ff_dsi->constantFrameRate;
    new_mp4ff_dsi->numTemporalLayers                  = mp4ff_dsi->numTemporalLayers;
    new_mp4ff_dsi->temporalIdNested                   = mp4ff_dsi->temporalIdNested;
    new_mp4ff_dsi->lengthSizeMinusOne                 = mp4ff_dsi->lengthSizeMinusOne;
    new_mp4ff_dsi->numOfArrays                        = mp4ff_dsi->numOfArrays;

    new_mp4ff_dsi->dsi_in_mdat                        = mp4ff_dsi->dsi_in_mdat;

    /** Switch to new entry in stsd list */
    list_add_entry(parser->dsi_lst, p_new_dsi);
    parser->curr_dsi = new_dsi;

    it_destroy(it);

    return EMA_MP4_MUXED_OK;
}


/**
 * Parse Network Abstraction Layer Units (NALUs)
 */
static int
parser_hevc_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    parser_hevc_handle_t      parser_hevc = (parser_hevc_handle_t)parser;
    dsi_hevc_handle_t         dsi_hevc    = (dsi_hevc_handle_t)parser->curr_dsi;
    mp4_dsi_hevc_handle_t     mp4ff_dsi   = (mp4_dsi_hevc_handle_t)dsi_hevc;
    hevc_nal_handle_t         nal         = &(parser_hevc->nal);
    hevc_au_nals_t *          au_nals     = &(parser_hevc->au_nals);

    video_parameter_set_t *              p_active_vps;
    sequence_parameter_set_t *           p_active_sps;
    picture_parameter_set_t *            p_active_pps;
    
    BOOL       old_au_end = FALSE;
    BOOL       nal_vcl_flag = FALSE;
    BOOL       keep_nal= FALSE;
    BOOL       keep_all= FALSE;
    BOOL       found_aud = FALSE;
    BOOL       pic_type_setting_flag = FALSE;

    uint32_t   sc_size = 0;
    int32_t    nal_in_au = 0;
    hevc_nal_loc_t *nal_loc;
    uint32_t   sei_size2keep = 0;  /** no sei to keep, or not a sei */
    int err = EMA_MP4_MUXED_OK;

    hevc_nalu_t nalu, nalu_el;
    bitstream_t bitstream, bitstream_el;
    hevc_decode_t *_context;
    hevc_decode_t *_context_el;

    _context = (hevc_decode_t *)(&(parser_hevc->dec));
    assert(_context);

    _context_el = (hevc_decode_t *)(&(parser_hevc->dec_el));
    assert(_context_el);

    bitstream.pui8_payload = nal->nal_buf; 
    bitstream.ui_length = (uint32_t)nal->nal_size; 
    bitstream_init( &bitstream );

    sample->flags = 0;  /** reset flag */


    /** Initialization. */
    sample->is_leading            = 0;
    sample->sample_depends_on     = 0;
    sample->sample_is_depended_on = 0;
    sample->sample_has_redundancy = 0;
    sample->dependency_level      = 0;
    sample->pic_type              = 0;
    sample->frame_type            = 0xff;

#if PARSE_DURATION_TEST
    if (parser_hevc->au_num && parser_hevc->au_num*(uint64_t)(parser_hevc->au_ticks) >=
        PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    if (IS_FOURCC_EQUAL(parser->dsi_name,"hev1"))
    {
        mp4ff_dsi->dsi_in_mdat = 1;
    }
    else
    {
        mp4ff_dsi->dsi_in_mdat = 0;
    }
    
    if (parser->dv_bl_non_comp_flag)
    {
        mp4ff_dsi->dsi_in_mdat = 1;
    }

    parser_hevc->sample_size = 0;
    keep_all                = (parser->dsi_type != DSI_TYPE_MP4FF);
#if TEST_NAL_ES_DUMP
    keep_all                = TRUE;  /** to keep all nal */
#endif

    /****** au are pushed out => always has a au start nal if not EOES */
    if (!nal->data_size)
    {
        /** push mode and 0 data mean end of file */
        return EMA_MP4_MUXED_EOES;
    }


    /****** nal parsing and au boundary test */
    do
    {
        /**** parse header of the nal of current au */
        err = read_input_nalu( &bitstream, &nalu );
        if(err)
        {
            return err;
        }

        keep_nal = TRUE;  /** default: to keep nal */
        sc_size  = (keep_all) ? 0 : (uint32_t)nal->sc_size;  /** only mp4ff replace start code */
        switch (nalu.e_nalu_type)
        {
            case NAL_UNIT_VPS:
                decode_vps( _context, &nalu );
                if (parser->dsi_type == DSI_TYPE_MP4FF)
                {
                    /** Check if new sample description is necessary */
                    if (ps_list_is_there_collision(&(mp4ff_dsi->vps_lst), 0, nal) &&
                        !(sample->flags & SAMPLE_NEW_SD))   /** Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                    {
                    /** New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                        err = parser_hevc_clone_dsi(parser);
                        if (err != EMA_MP4_MUXED_OK)
                        {
                            return err;
                        }
                        /** Update hevc handles */
                        dsi_hevc   = (dsi_hevc_handle_t)parser->curr_dsi;
                        mp4ff_dsi = (mp4_dsi_hevc_handle_t)dsi_hevc;
                    }

                    keep_nal = ps_list_update(parser_hevc, &(mp4ff_dsi->vps_lst), 0, nal, &sample->flags);
                    if (mp4ff_dsi->dsi_in_mdat)
                    {
                        keep_nal = TRUE;
                    }
                }

                parser_hevc->vps_num++;
                break;


               case NAL_UNIT_SPS:

                decode_sps( _context, &nalu );
                if (parser->dsi_type == DSI_TYPE_MP4FF)
                {
                    /** Check if new sample description is necessary */
                    if (ps_list_is_there_collision(&(mp4ff_dsi->sps_lst), _context->i_curr_sps_idx, nal) &&
                        !(sample->flags & SAMPLE_NEW_SD))   /** Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                    {
                    /** New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                        err = parser_hevc_clone_dsi(parser);
                        if (err != EMA_MP4_MUXED_OK)
                        {
                            return err;
                        }
                        /** Update hevc handles */
                        dsi_hevc   = (dsi_hevc_handle_t)parser->curr_dsi;
                        mp4ff_dsi = (mp4_dsi_hevc_handle_t)dsi_hevc;
                    }

                    keep_nal = ps_list_update(parser_hevc, &(mp4ff_dsi->sps_lst), _context->i_curr_sps_idx, nal, &sample->flags);
                    if (mp4ff_dsi->dsi_in_mdat)
                    {
                        keep_nal = TRUE;
                    }
                }

                parser_hevc->sps_num++;
                break;

            case NAL_UNIT_PPS:
                decode_pps( _context, &nalu );
                if (parser->dsi_type == DSI_TYPE_MP4FF)
                {
                /** Check if new sample description is necessary */
                    if (ps_list_is_there_collision(&(mp4ff_dsi->pps_lst), _context->i_curr_pps_idx, nal) &&
                        !(sample->flags & SAMPLE_NEW_SD))   /** Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                        {
                    /** New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                            err = parser_hevc_clone_dsi(parser);
                            if (err != EMA_MP4_MUXED_OK)
                            {
                                return err;
                               }
                        /** Update hevc handles */
                            dsi_hevc   = (dsi_hevc_handle_t)parser->curr_dsi;
                            mp4ff_dsi = (mp4_dsi_hevc_handle_t)dsi_hevc;
                        }

                        keep_nal = ps_list_update(parser_hevc, &(mp4ff_dsi->pps_lst), _context->i_curr_pps_idx, nal, &sample->flags);
                    if (mp4ff_dsi->dsi_in_mdat)
                    {
                        keep_nal = TRUE;
                    }
                }
                    parser_hevc->pps_num++;
                break;

            case NAL_UNIT_ACCESS_UNIT_DELIMITER:
                found_aud = TRUE;
                keep_nal  = TRUE;
                break;

            case NAL_UNIT_PREFIX_SEI:
                decode_sei_nalu( _context, &nalu );
                if (_context->rpu_flag)
                {
                    parser->dv_rpu_nal_flag = 1;
                }
                keep_nal  = TRUE;
                break;
            case NAL_UNIT_SUFFIX_SEI:
                keep_nal  = TRUE;
                break;
            /** DolbyVision RPU NALs */
            case NAL_UNIT_UNSPECIFIED_62: 
                parser->dv_rpu_nal_flag = 1;
                keep_nal  = TRUE;
                break;
            /** DolbyVision EL NALs */
            case NAL_UNIT_UNSPECIFIED_63: 
                if (!parser->dv_el_track_flag && parser_hevc->au_num == 0)
                {
                    uint32_t index = 0;
                    uint8_t temp_data[1024];
                    mp4_dsi_hevc_handle_t dsi = (mp4_dsi_hevc_handle_t) (((parser_hevc_handle_t) parser)->dsi_enh);

                    parser->dv_el_nal_flag = 1;
                    keep_nal  = TRUE;
                    if(((nal->nal_buf[6] >> 1) > 31) && (nal->nal_buf[6] >> 1) < 35)
                    {
                        assert (nal->nal_size < 1024);
                        for (index = 0; index < 4; index++)
                            temp_data[index] = nal->nal_buf[index];

                        for (index = 4; index < nal->nal_size - 2; index++)
                            temp_data[index] = nal->nal_buf[index+2];

                        bitstream_el.pui8_payload = temp_data; 
                        bitstream_el.ui_length = (uint32_t)nal->nal_size - 2; 
                        bitstream_init( &bitstream_el );

                        err = read_input_nalu( &bitstream_el, &nalu_el);
                        if(err)
                        {
                            return err;
                        }

                        nal->nal_buf += 2;
                        nal->nal_size -= 2;
                        if (nalu_el.e_nalu_type == NAL_UNIT_VPS)
                        {
                            decode_vps( _context_el, &nalu_el);
                            ps_list_update(parser_hevc, &(dsi->vps_lst), 0, nal, NULL);
                        }
                        else if (nalu_el.e_nalu_type == NAL_UNIT_SPS)
                        {
                            decode_sps( _context_el, &nalu_el);
                            ps_list_update(parser_hevc, &(dsi->sps_lst), 0, nal, NULL);
                        }
                        else if (nalu_el.e_nalu_type == NAL_UNIT_PPS)
                        {
                            decode_pps( _context_el, &nalu_el);
                            ps_list_update(parser_hevc, &(dsi->pps_lst), 0, nal, NULL);
                        }

                        nal->nal_buf -= 2;
                        nal->nal_size += 2;
                    }
                }
                break;

            case NAL_UNIT_CODED_SLICE_TRAIL_R:
            case NAL_UNIT_CODED_SLICE_TRAIL_N:
            case NAL_UNIT_CODED_SLICE_TLA_R:
            case NAL_UNIT_CODED_SLICE_TSA_N:
            case NAL_UNIT_CODED_SLICE_STSA_R:
            case NAL_UNIT_CODED_SLICE_STSA_N:
            case NAL_UNIT_CODED_SLICE_BLA_W_LP:
            case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
            case NAL_UNIT_CODED_SLICE_BLA_N_LP:
            case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
            case NAL_UNIT_CODED_SLICE_IDR_N_LP:
            case NAL_UNIT_CODED_SLICE_CRA:
            case NAL_UNIT_CODED_SLICE_RADL_R:
            case NAL_UNIT_CODED_SLICE_RADL_N:  
            case NAL_UNIT_CODED_SLICE_RASL_R:
            case NAL_UNIT_CODED_SLICE_RASL_N:
                nal_vcl_flag = TRUE;
                gop_decode_slice( _context, &nalu );
                break;

            default:
                /** Filler data and so on...doesn't get added to sample buffer */
                keep_nal = keep_all;
            break;
        }

        /** Abort when multiple sample descriptions would be necessary but forbidden */
        if (parser_hevc->sd_collision_flag)
        {
            return EMA_MP4_MUXED_MULTI_SD_ERR;
        }

        /** to get nal_size and sc_off_next if havn't, reach next sc */
        skip_the_nal(nal);
        

        /******* book keep the nal */
        if (keep_nal)
        {
            nal_loc = au_nals->nal_locs + au_nals->nal_idx;

            nal_loc->sc_size = sc_size;
            /** nothing ever sets sei_size2keep to anything other than 0 */
            assert(sei_size2keep == 0); /** if this is triggered, the dead code is no longer dead and someone has fixed this */
            if (!sei_size2keep)
            {
                /** !sei nal or all sei nal to keep */
                nal_loc->off  = nal->off_file + sc_size;
                nal_loc->size = nal->nal_size - sc_size;
            }

            sei_size2keep = 0;

            if (incr_nal_idx(au_nals) != EMA_MP4_MUXED_OK)
                return EMA_MP4_MUXED_BUGGY;

            parser_hevc->sample_size += (uint32_t)(dsi_hevc->NALUnitLength + nal_loc->size);
#if TEST_NAL_ES_DUMP
            parser_hevc->sample_size -= dsi_hevc->NALUnitLength;  /** no replacement */
#endif
        }
        nal_in_au++;  /** got a nal for au */

        /** Before we parse the next nal to look ahead, save current AU information. */
        /** The DVB DASH profile mentions the following about HEVC:

        5.2        DASH Specific Aspects for HEVC Video
        5.2.1      HEVC Specifics
        The encapsulation of HEVC video data in ISO BMFF is defined in ISO/IEC 14496-15 [5]. 
        Players which support HEVC shall support both sample entries using 'hvc1' and 'hev1' 
        (both storage for VPS/SPS/PPS within the initialisation segment or inband within the
        media segment). IDR pictures with nal_unit_type equal to IDR_N_LP and IDR_W_RADL are 
        mapped to SAP types 1 and 2, respectively. BLA pictures with nal_unit_type equal to 
        BLA_N_LP and BLA_W_RADL are mapped to SAP types 1 and 2, respectively. 

        Note: The mapping to SAP type 3 for ISO BMFF with HEVC deliberately remains undefined 
        until MPEG reaches a conclusion. This includes the mapping of all other types of HEVC 
        DVB_RAP pictures (including BLA pictures with nal_unit_type equal to BLA_W_LP, CRA 
        pictures with nal_unit_type equal to CRA_NUT and pictures with nal_unit_type equal to 
        TRAIL_R that contain only slices with slice_type equal to 2 (I slice), as specified in 
        ETSI TS 101 154 [4] clause 5.14.1.8). */
        if (nal_vcl_flag && !pic_type_setting_flag)
        {
            if (   nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_N_LP
                || nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_N_LP ) 
            {
                sample->pic_type         = 1;
                sample->frame_type       = 0;
                sample->dependency_level = 0x01;
                sample->flags |= SAMPLE_SYNC;
            }
            else if (   nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL
                     || nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_RADL )
            {
                sample->pic_type         = 2;
                sample->frame_type       = 0;
                sample->dependency_level = 0x01;
                sample->flags |= SAMPLE_SYNC;
            }
            else if (   nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_BLA_W_LP
                     || nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_CRA
                     || nalu.e_nalu_type == NAL_UNIT_CODED_SLICE_TRAIL_R)
            {
                sample->pic_type         = 3;
                sample->frame_type       = 1;
                sample->dependency_level = 0x02;
            }
            else
            {
                sample->pic_type         = 0;
                sample->frame_type       = 1;
                sample->dependency_level = 0x02;
            }
            pic_type_setting_flag = TRUE;
        }

        /**** done with current nal, load a new nal */
        if (!get_a_nal(nal))
        {
            break;
        }

        /*** reset the parser bitstream after every nal parsing finished */
        bitstream.pui8_payload = nal->nal_buf; 
        bitstream.ui_length = (uint32_t)nal->nal_size; 
        bitstream_init( &bitstream );

        if (nal_vcl_flag == 1)
        {
            /** we have got a VCL NAL, and check if the next NAL is associated or not */

            /** IDR_W_RADL, it may have associated nals*/
            uint8_t nal_type_data = *(nal->nal_buf + nal->sc_size);
            hevc_nalu_type_t nal_type = (hevc_nalu_type_t)(nal_type_data >> 1);  
            uint8_t first_slice_flag = 0;

            switch(nal_type) {
            case NAL_UNIT_CODED_SLICE_TRAIL_R:
            case NAL_UNIT_CODED_SLICE_TRAIL_N:
            case NAL_UNIT_CODED_SLICE_TLA_R:
            case NAL_UNIT_CODED_SLICE_TSA_N:
            case NAL_UNIT_CODED_SLICE_STSA_R:
            case NAL_UNIT_CODED_SLICE_STSA_N:
            case NAL_UNIT_CODED_SLICE_BLA_W_LP:
            case NAL_UNIT_CODED_SLICE_BLA_W_RADL:
            case NAL_UNIT_CODED_SLICE_BLA_N_LP:
            case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
            case NAL_UNIT_CODED_SLICE_IDR_N_LP:
            case NAL_UNIT_CODED_SLICE_CRA:
            case NAL_UNIT_CODED_SLICE_RADL_R:
            case NAL_UNIT_CODED_SLICE_RADL_N:  
            case NAL_UNIT_CODED_SLICE_RASL_R:
            case NAL_UNIT_CODED_SLICE_RASL_N:
                first_slice_flag =  *(nal->nal_buf + nal->sc_size + 2) & 0x80;
                break;
            case NAL_UNIT_ACCESS_UNIT_DELIMITER: 
                first_slice_flag = 1;
                break;
            default:
                first_slice_flag = 0;
                break;
            }

            if (nal_vcl_flag)
            {
                if (first_slice_flag)
                {
                    old_au_end = TRUE;
                    msglog(NULL, MSGLOG_DEBUG, "\nPrev au %u complete\n", parser_hevc->au_num);
                    break;
                }
                else
                {
                    if((nal_type == NAL_UNIT_PREFIX_SEI) || (nal_type == NAL_UNIT_ACCESS_UNIT_DELIMITER)) 
                    {
                        old_au_end = TRUE;
                        msglog(NULL, MSGLOG_DEBUG, "\nPrev au %u complete\n", parser_hevc->au_num);
                        break;
                    }
                }
            }
        }
    } while (1);

    if (!old_au_end)
    {
        /** get_a_nal() fail: end of file */
        if (!parser_hevc->sample_size)
        {
            return EMA_MP4_MUXED_EOES;
        }

        /** last sample: parser_avc->sample_size != 0 if source file has one valid nal */
        msglog(NULL, MSGLOG_DEBUG, "\nLast au %u complete\n", parser_hevc->au_num);
    }

    /** come here:  conclude an au.  !old_au_end it is a last au */
    if (!parser_hevc->au_num)
    {
#ifdef FAKE_FIRST_SAMPLE_IS_SYNC
         sample->flags |= SAMPLE_SYNC;
#endif
    }

    p_active_vps = &(_context->s_vps);
    p_active_sps = &(_context->as_sps[_context->i_curr_sps_idx]);
    p_active_pps = &(_context->as_pps[_context->i_curr_pps_idx]);
    /** Some general parsing error. Bail out to avoid accessing NULL pointers later on. */
    if (!p_active_vps || !p_active_sps || !p_active_pps)
    {
        return EMA_MP4_MUXED_ES_ERR;
    }
    /** VPS/ SPS / PPS configuration missing prior to video payload */
    if (!p_active_vps->b_isDefined || !p_active_sps->b_init || !p_active_pps->b_isDefined)
    {
        err = EMA_MP4_MUXED_NO_CONFIG_ERR;
    }
    if (parser_hevc->au_num == 0)
    {
        /** within a seq, active_sps remain the same */
        parser_hevc->width    = p_active_sps->i_pic_luma_width;
        parser_hevc->height   = p_active_sps->i_pic_luma_height;
        if(_context->s_vui.b_aspect_ratio_info == TRUE) 
        {
            parser_hevc->hSpacing = _context->s_vui.i_sar_width; 
            parser_hevc->vSpacing = _context->s_vui.i_sar_height;
        }
        else
        {  /*** if sar info not present in ES, the value should be set to 0 or 1?? */
            parser_hevc->vSpacing = 1; 
            parser_hevc->hSpacing = 1;
        }

        timing_info_update(parser_hevc, _context);
        get_colr_info(parser_hevc, _context);
        dsi_update(dsi_hevc, _context);
        if ((parser->dv_rpu_nal_flag == 1) && (parser->dv_el_nal_flag))
        {
            dsi_hevc_handle_t dsi = (dsi_hevc_handle_t) (((parser_hevc_handle_t) parser)->dsi_enh);
            dsi_update(dsi, _context_el);
        }

    }

    /**** timing */
    sample->dts = parser_hevc->au_num;
    sample->dts *= parser_hevc->au_ticks;

    if(_context->i_prev_poc == 0)
        _context->poc_offset = sample->dts;
    sample->cts = _context->poc_offset + _context->i_prev_poc*parser_hevc->au_ticks;

    update_idx_value_lst(parser_hevc->hevc_cts_offset_lst, parser_hevc->num_samples, sample->cts - sample->dts);
    
    sample->duration = parser_hevc->au_ticks;

    /**** data */
    sample->size = parser_hevc->sample_size;

    save_au_nals_info(au_nals, sample, parser_hevc->tmp_bbo);
    
    if (_context->IDR_pic_flag)
    {
        uint32_t dist = parser_hevc->au_num - parser_hevc->last_idr_pos;
        if (dist > parser_hevc->max_idr_dist && parser_hevc->au_num > parser_hevc->last_idr_pos)
        {
            parser_hevc->max_idr_dist = dist;
        }
        parser_hevc->last_idr_pos = parser_hevc->au_num;
    }

    parser_hevc->au_num++;
    parser_hevc->num_samples++;

    return err;
}


static int
parser_hevc_get_subsample(parser_handle_t parser, int64_t *pos, uint32_t subs_num_in, int32_t *more_subs_out, uint8_t *data, size_t *bufsize_ptr)
{
    uint32_t nal_num, size;
    int32_t  nals_left;
    uint8_t  sc_size;
    int64_t  off;

    parser_hevc_handle_t parser_hevc     = (parser_hevc_handle_t)parser;
    bbio_handle_t        src          = parser_hevc->tmp_bbi, ds = parser->ds;
    const uint32_t        nal_unit_len = ((dsi_hevc_handle_t)parser->curr_dsi)->NALUnitLength;
    const size_t        bufsize      = *bufsize_ptr;

    if (!src)
    {
        uint8_t* buffer;
        size_t     data_size, buf_size;
        /** give the output buffer to the input buffer */
        assert(parser_hevc->tmp_bbo);
        src    = reg_bbio_get('b', 'r');
        buffer = parser_hevc->tmp_bbo->get_buffer(parser_hevc->tmp_bbo, &data_size, &buf_size);
        src->set_buffer(src, buffer, data_size, TRUE);
        parser_hevc->tmp_bbi = src;
    }

    if (pos && *pos != -1)
    {
        src->seek(src, *pos, SEEK_SET);
    }

    if (RD_PREFIX(src) != 0)
    {
        return EMA_MP4_MUXED_READ_ERR;
    }

    if (src_rd_u32(src, &nal_num) != 0)   /** # of nal in au */
    {
        return EMA_MP4_MUXED_READ_ERR;
    }

    subs_num_in++;    /** start counting with 1 makes things easier */
    nals_left = nal_num - subs_num_in;
    if (more_subs_out)
    {
        *more_subs_out = nals_left;
    }

    if (nals_left < 0)
    {
        if (more_subs_out)
            *more_subs_out = 0;
        return nals_left;
    }

    do
    {
        uint64_t u;
        if (src_rd_u64(src, &u) != 0) 
        {
            return EMA_MP4_MUXED_READ_ERR;
        }
        off = u;
        if (src_rd_u32(src, &size) != 0)
        {
            return EMA_MP4_MUXED_READ_ERR;
        }
        if (src_rd_u8(src, &sc_size) != 0)
        {
            return EMA_MP4_MUXED_READ_ERR;
        }
    } while (--subs_num_in);

    *bufsize_ptr = nal_unit_len + size;
    if (pos)
    {
        *pos = src->position(src);
    }

    if (data)
    {
        uint32_t n = nal_unit_len;

        if (*bufsize_ptr > bufsize)
        {
            return 1;  /** buffer too small */
        }

#if !TEST_NAL_ES_DUMP
        while (n--)
        {
            *(data++) = (uint8_t)((size >> (n*8)) & 0xff);
        }
#endif

        if (off != -1)
        {
            /** not embedded: nal in ds */
            ds->seek(ds, off, SEEK_SET);
            ds->read(ds, data, size);
        }
        else
        {
            /** embedded: nal body right at current position */
            src->read(src, data, size);
        }
    }

    return EMA_MP4_MUXED_OK;
}


static int
parser_hevc_copy_sample(parser_handle_t parser, bbio_handle_t snk, int64_t pos)
{
    pos;
    snk;
    parser;
    return EMA_MP4_MUXED_OK;
}

static BOOL
parser_hevc_need_fix_ctts(parser_handle_t parser)
{
    parser;
    return TRUE;
}


static int32_t
parser_hevc_get_cts_offset(parser_handle_t parser, uint32_t sample_idx)
{
    idx_value_t *  cv;
    uint64_t ctts = 0;
    static int32_t ctts_offset = 0;

    it_list_handle_t   it  = it_create();
    parser_hevc_handle_t parser_hevc = (parser_hevc_handle_t)parser;
    
    if(sample_idx == 0)
    {

        it_init(it,parser_hevc->hevc_cts_offset_lst);
        while ((cv = (idx_value_t *)it_get_entry(it)))
        {
            if((int32_t)cv->value < ctts_offset)
                ctts_offset = (int32_t)cv->value;
            
        }
        it_destroy(it);
        return (-ctts_offset);
    }
    else
    {
        it_init(it, parser_hevc->hevc_cts_offset_lst);
        while ((cv = (idx_value_t *)it_get_entry(it)))
        {
            if (cv->idx == sample_idx)
            {
                ctts = cv->value;
                break;
            }
        }
        it_destroy(it);
    }

    return (int32_t)(ctts + (-ctts_offset));
}

/** get dsi for hevc (HEVCDecoderConfigurationRecord) */
/* implements method get_cfg() of the (HEVC) parser for the dsi_type DSI_TYPE_MP4FF
 */
static int
parser_hevc_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    bbio_handle_t        snk;
    mp4_dsi_hevc_handle_t dsi = (mp4_dsi_hevc_handle_t)parser->curr_dsi;
    buf_entry_t *        entry;
    it_list_handle_t     it  = it_create();

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 1024, 1);
    }

    /** HEVCDecoderConfigurationRecord - see [ISOAVC/PDAM] Section 8.3.3.1.1 */
    sink_write_u8(snk, 1);                    /** configurationVersion = 1 */
    
    sink_write_bits(snk, 2, dsi->profile_space);
    sink_write_bits(snk, 1, dsi->tier_flag);
    sink_write_bits(snk, 5, dsi->profile_idc);
    
    sink_write_u32(snk, dsi->profile_compatibility_indications);

    sink_write_bits(snk, 1, dsi->progressive_source_flag);
    sink_write_bits(snk, 1, dsi->interlaced_source_flag);
    sink_write_bits(snk, 1, dsi->non_packed_constraint_flag);
    sink_write_bits(snk, 1, dsi->frame_only_constraint_flag);

    sink_write_bits(snk, 44, 0); /** just set constraint_indicator_flags = 0 */
    
    sink_write_u8(snk, dsi->level_idc);

    sink_write_bits(snk, 4, 0xf);
    sink_write_bits(snk, 12, dsi->min_spatial_segmentation_idc);

    sink_write_bits(snk, 6, 0x3F);
    sink_write_bits(snk, 2,dsi->parallelismType);

    sink_write_bits(snk, 6, 0x3F);
    sink_write_bits(snk, 2,dsi->chromaFormat);
    
    sink_write_bits(snk, 5, 0x1F);
    sink_write_bits(snk, 3, dsi->bitDepthLumaMinus8);

    sink_write_bits(snk, 5, 0x1F);
    sink_write_bits(snk, 3, dsi->bitDepthChromaMinus8);

    sink_write_u16(snk, dsi->AvgFrameRate);                         /**  frames/(256 seconds) */

    sink_write_bits(snk, 2, dsi->constantFrameRate);                /**  assume the frame rate is constant */
    sink_write_bits(snk, 3, dsi->numTemporalLayers);
    sink_write_bits(snk, 1, dsi->temporalIdNested);
    sink_write_bits(snk, 2, dsi->lengthSizeMinusOne);

    if(list_get_entry_num(dsi->vps_lst))
        dsi->numOfArrays++;
    if(list_get_entry_num(dsi->sps_lst))
        dsi->numOfArrays++;
    if(list_get_entry_num(dsi->pps_lst))
        dsi->numOfArrays++;

    if(dsi->dsi_in_mdat) /** sample entry name "hev1"*/
    {
        sink_write_u8(snk, 0); /** set numOfArrays = 0; */
    }
    else /** sample entry name "hvc1"*/
    {
        sink_write_u8(snk, dsi->numOfArrays);
        if(list_get_entry_num(dsi->vps_lst)) {
            sink_write_bits(snk, 1, 1);                                       /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                       /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_VPS);                            /** VideoParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->vps_lst));  /** numOfVideoParameterSets */
            it_init(it, dsi->vps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                   /** VideoParameterSetLength */
                snk->write(snk, entry->data, entry->size);                    /** VideoParameterSetNALUnit */
            }
        }

        if(list_get_entry_num(dsi->sps_lst)) {
            sink_write_bits(snk, 1, 1);                                       /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                       /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_SPS);                            /** SequenceParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->sps_lst));  /** numOfSequenceParameterSets */
            it_init(it, dsi->sps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                   /** sequenceParameterSetLength */
                snk->write(snk, entry->data, entry->size);                    /** sequenceParameterSetNALUnit */
            }
        }

        if(list_get_entry_num(dsi->pps_lst)) {
            sink_write_bits(snk, 1, 1);                                       /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                       /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_PPS);                            /** PictureParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->pps_lst));  /** numOfPictureParameterSets */
            it_init(it, dsi->pps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                   /** PictureParameterSetLength */
                snk->write(snk, entry->data, entry->size);                    /** PictureParameterSetNALUnit */
            }
        }
    }

    /** if it's dolby vision, we should add 'dvcC' to 'hvcC' */
    if (parser->dv_rpu_nal_flag)
    {
        parser->dv_dsi_size = 24;
        memset(parser->dv_dsi_buf, 0, parser->dv_dsi_size);

        parser->dv_dsi_buf[0] = 1;
        if (parser->dv_el_nal_flag)
        {
            parser->dv_dsi_buf[3] = 7; /** BL+EL+RPU */
        }
        else
        {
            if ((parser->ext_timing.ext_dv_profile == 5) || (parser->ext_timing.ext_dv_profile == 8))
                parser->dv_dsi_buf[3] = 5; /** BL+RPU */
            else
                parser->dv_dsi_buf[3] = 6; /** EL+RPU */
        }

        if (parser->ext_timing.ext_dv_profile != 0xff) 
        {
            if ((parser->ext_timing.ext_dv_profile > 1) && (parser->ext_timing.ext_dv_profile < 9))
            {
                parser->dv_dsi_buf[2] |= (parser->ext_timing.ext_dv_profile << 1);
            }
            else
            {
                msglog(NULL, MSGLOG_ERR,"Error: For Dolby vision hevc codec type, only setting profile to 2-8 makes sense!\n");
                it_destroy(it);
                return EMA_MP4_MUXED_BUGGY;
            }
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, "Error: For muxing Dolby vision stream, '--dv-profile' must be set by user!\n");
            it_destroy(it);
            return EMA_MP4_MUXED_BUGGY;
        }

        parser->dv_dsi_buf[2] |= (parser->dv_level & 0x80);
         parser->dv_dsi_buf[3] |= (parser->dv_level << 3);
    }
    else /** dolby vision profile 3(Non backward compatible Base Layer track) */
    {
        if (parser->ext_timing.ext_dv_profile == 3)
        {
            parser->dv_dsi_size = 24;
            memset(parser->dv_dsi_buf, 0, parser->dv_dsi_size);

            parser->dv_dsi_buf[0] = 1;

            parser->dv_dsi_buf[2] |= (3 << 1); /** setting profile */
            parser->dv_dsi_buf[2] |= (parser->dv_level & 0x80); /** set level */
             parser->dv_dsi_buf[3] |= (parser->dv_level << 3);   /** set level */
            parser->dv_dsi_buf[3] |= 1; /** Setting flags, only BL flag is true; */
        }
    }

    if ((parser->ext_timing.ext_dv_profile == 2) || (parser->ext_timing.ext_dv_profile == 4))
    {
        parser->dv_dsi_buf[4] |= (2 << 4); 
    }
    else if (parser->ext_timing.ext_dv_profile == 6)
    {
        parser->dv_dsi_buf[4] |= (1 << 4);
    }
    else if (parser->ext_timing.ext_dv_profile == 7)
    {
        parser->dv_dsi_buf[4] |= (6 << 4);
    }
    else if (parser->ext_timing.ext_dv_profile == 8)
    {
        parser->dv_dsi_buf[4] |= (parser->ext_timing.ext_dv_bl_compatible_id << 4); 
    } 

    it_destroy(it);

    *buf = snk->get_buffer(snk, buf_len, 0);  /** here buf_len is set to data_size */
    snk->destroy(snk);

    /** If there's el nal, then extract the dsi info, which could be used to create hvcE */
    if (parser->dv_el_nal_flag)
    {
        mp4_dsi_hevc_handle_t dsi = (mp4_dsi_hevc_handle_t) (((parser_hevc_handle_t) parser)->dsi_enh);
        buf_entry_t *        entry;
        it_list_handle_t     it  = it_create();
        snk = reg_bbio_get('b', 'w');
        snk->set_buffer(snk, NULL, 1024, 1);
        
        sink_write_u8(snk, 1);                    /** configurationVersion = 1 */
    
        sink_write_bits(snk, 2, dsi->profile_space);
        sink_write_bits(snk, 1, dsi->tier_flag);
        sink_write_bits(snk, 5, dsi->profile_idc);
    
        sink_write_u32(snk, dsi->profile_compatibility_indications);

        sink_write_bits(snk, 1, dsi->progressive_source_flag);
        sink_write_bits(snk, 1, dsi->interlaced_source_flag);
        sink_write_bits(snk, 1, dsi->non_packed_constraint_flag);
        sink_write_bits(snk, 1, dsi->frame_only_constraint_flag);

        sink_write_bits(snk, 44, 0); /** just set constraint_indicator_flags = 0 */
    
        sink_write_u8(snk, dsi->level_idc);

        sink_write_bits(snk, 4, 0xf);
        sink_write_bits(snk, 12, dsi->min_spatial_segmentation_idc);

        sink_write_bits(snk, 6, 0x3F);
        sink_write_bits(snk, 2,dsi->parallelismType);

        sink_write_bits(snk, 6, 0x3F);
        sink_write_bits(snk, 2,dsi->chromaFormat);
    
        sink_write_bits(snk, 5, 0x1F);
        sink_write_bits(snk, 3, dsi->bitDepthLumaMinus8);

        sink_write_bits(snk, 5, 0x1F);
        sink_write_bits(snk, 3, dsi->bitDepthChromaMinus8);

        sink_write_u16(snk, dsi->AvgFrameRate);                         /**  frames/(256 seconds) */

        sink_write_bits(snk, 2, dsi->constantFrameRate);                /**  assume the frame rate is constant */
        sink_write_bits(snk, 3, dsi->numTemporalLayers);
        sink_write_bits(snk, 1, dsi->temporalIdNested);
        sink_write_bits(snk, 2, dsi->lengthSizeMinusOne);

        if(list_get_entry_num(dsi->vps_lst))
            dsi->numOfArrays++;
        if(list_get_entry_num(dsi->sps_lst))
            dsi->numOfArrays++;
        if(list_get_entry_num(dsi->pps_lst))
            dsi->numOfArrays++;

        sink_write_u8(snk, dsi->numOfArrays);
        if(list_get_entry_num(dsi->vps_lst)) {
            sink_write_bits(snk, 1, 1);                                         /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                         /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_VPS);                              /** VideoParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->vps_lst));    /** numOfVideoParameterSets */
            it_init(it, dsi->vps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                    /** VideoParameterSetLength */
                snk->write(snk, entry->data, entry->size);                     /** VideoParameterSetNALUnit */
            }
        }

        if(list_get_entry_num(dsi->sps_lst)) {
            sink_write_bits(snk, 1, 1);                                        /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                        /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_SPS);                             /** SequenceParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->sps_lst));   /** numOfSequenceParameterSets */
            it_init(it, dsi->sps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                    /** sequenceParameterSetLength */
                snk->write(snk, entry->data, entry->size);                     /** sequenceParameterSetNALUnit */
            }
        }

        if(list_get_entry_num(dsi->pps_lst)) {
            sink_write_bits(snk, 1, 1);                                        /** array_completeness = 1; because our name "hvc1" */
            sink_write_bits(snk, 1, 0);                                        /** reserved = 0; */
            sink_write_bits(snk, 6, NAL_UNIT_PPS);                             /** PictureParameterSet type */
            sink_write_u16(snk, (uint16_t)list_get_entry_num(dsi->pps_lst));   /** numOfPictureParameterSets */
            it_init(it, dsi->pps_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk, (uint16_t)entry->size);                    /** PictureParameterSetLength */
                snk->write(snk, entry->data, entry->size);                     /** PictureParameterSetNALUnit */
            }
        }

        it_destroy(it);
        parser->dv_el_dsi_buf = snk->get_buffer(snk, (size_t *)(&parser->dv_el_dsi_size), 0);  /** here buf_len is set to data_size */
        snk->destroy(snk);
    }

    return 0;
}




static int
parser_hevc_get_param_ex(parser_handle_t parser, stream_param_id_t param_id, int32_t param_idx, void *param)
{
    /** Currently we have not implement it. */
    param;
    param_idx;
    param_id;
    parser;
    return EMA_MP4_MUXED_OK;
}

static uint32_t
parser_hevc_get_param(parser_handle_t parser, stream_param_id_t param_id)
{
    /** Currently we have not implement it. */
    param_id;
    parser;
    return 0;
}


static void
parser_hevc_show_info(parser_handle_t parser)
{
    /** Currently we have not implement it. */
    parser;
}


/** Converts hevc mp4 VPS, SPS, PPS into hevc format: NALLength => start code
 *  implements method write_cfg() of the (HEVC) parser for the dsi_type DSI_TYPE_MP4FF
 */
static uint8_t *
parser_hevc_write_mp4_cfg(parser_handle_t parser, bbio_handle_t sink)
{
    /** Currently we have not implement it. */
    sink;
    parser;
    return NULL;
}

/** convert hevc mp4 into hevc format: NALLength => start code */
static int
parser_hevc_write_au(parser_handle_t parser, uint8_t *data, size_t size, bbio_handle_t sink)
{
    /** Currently we don't use this interface. */
    sink;
    size;
    data;
    parser;
    return 0;
}

static void
hevc_close(parser_handle_t parser)
{
    parser_hevc_handle_t parser_hevc = (parser_hevc_handle_t)parser;

    FREE_CHK(parser_hevc->nal.tmp_buf);
    FREE_CHK(parser_hevc->nal.buffer);
    if (parser_hevc->hevc_cts_offset_lst)
    {
        list_destroy(parser_hevc->hevc_cts_offset_lst);
    }

    if (parser_hevc->nal.tmp_buf_bbi)
    {
        parser_hevc->nal.tmp_buf_bbi->destroy(parser_hevc->nal.tmp_buf_bbi);
    }

    if (parser_hevc->dsi_enh)
    {
        parser_hevc->dsi_enh->destroy(parser_hevc->dsi_enh);
    }

    /** release nal related stuff */
    if (parser_hevc->tmp_bbo)
    {
        parser_hevc->tmp_bbo->destroy(parser_hevc->tmp_bbo);
    }
    if (parser_hevc->tmp_bbi)
    {
        parser_hevc->tmp_bbi->destroy(parser_hevc->tmp_bbi);
    }
    if (parser_hevc->au_nals.nal_idx)
    {
        hevc_au_nals_t *au_nals = &(parser_hevc->au_nals);

        while (au_nals->nal_idx--)
        {
            hevc_nal_loc_t *nal_loc = au_nals->nal_locs + au_nals->nal_idx;
            if (nal_loc->buf_emb)
            {
                FREE_CHK(nal_loc->buf_emb);
                nal_loc->buf_emb = 0;
            }
        }
    }


}

static void
parser_hevc_destroy(parser_handle_t parser)
{
    hevc_close(parser);
    parser_destroy(parser);
}

static int
parser_hevc_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx, bbio_handle_t ds)
{
    parser_hevc_handle_t parser_hevc = (parser_hevc_handle_t)parser;
    hevc_nal_handle_t        nal        = &parser_hevc->nal;
    hevc_decode_t *      dec        = &parser_hevc->dec;

    parser->ext_timing = *ext_timing;
    parser->es_idx     = es_idx;
    parser->ds         = ds;

    /** nal parser buffer */
    nal->ds       = ds;
    nal->buf_size = 4096;
    nal->buffer   = (uint8_t *) MALLOC_CHK(nal->buf_size);

    if (!nal->buffer)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }

    nal->tmp_buf_size = 4096;
    nal->tmp_buf      = (uint8_t *)MALLOC_CHK(nal->tmp_buf_size);
    if (!nal->tmp_buf)
    {
        return EMA_MP4_MUXED_NO_MEM;
    }
    nal->tmp_buf_bbi = reg_bbio_get('b', 'r');

    if (parser->dsi_type != DSI_TYPE_MP2TS)
    {
        if (!get_a_nal(nal) || nal->data_size < 4)
        {
            /** no data at all or too less and cause get_a_nal() malfunction */
            return EMA_MP4_MUXED_EOES;
        }
    }

    /** create a memory buffer as file i/o can cause issues with system rights */
    parser_hevc->tmp_bbo = reg_bbio_get('b', 'w');
    parser_hevc->tmp_bbo->set_buffer(parser_hevc->tmp_bbo, NULL, 0, TRUE);

    hevc_dec_init(dec);

    /** validation */
    parser_hevc->last_idr_pos    = (uint32_t)(-1);
    parser_hevc->post_validation = NULL; 

    parser_hevc->hevc_cts_offset_lst = list_create(sizeof(idx_value_t));

    /** reset HEVC sample buffer */
    return EMA_MP4_MUXED_OK;

}

/**
 * @brief Parses curr_codec_config into curr_dsi
 *
 * curr_codec_config is expected to be set when this function / method is called
 * typically, curr_codec_config is set to one entry in codec_config_list
 */
static int
parser_hevc_codec_config(parser_handle_t parser, bbio_handle_t info_sink)
{
    info_sink;
    parser;
    return 0;
}

/** Creates and build interface, base */
static parser_handle_t
parser_hevc_create(uint32_t dsi_type)
{
    parser_hevc_handle_t parser;

    /** dsi_type = DSI_TYPE_MP2TS;  force to a mp2ts parser test */

    parser = (parser_hevc_handle_t)MALLOC_CHK(sizeof(parser_hevc_t));
    if (!parser)
    {
        return 0;
    }
    memset(parser, 0, sizeof(parser_hevc_t));
    memset(parser->codec_name, 0, sizeof(parser->codec_name));

    /**** build the interface, base for the instance */
    parser->stream_type     = STREAM_TYPE_VIDEO;
    parser->stream_id       = STREAM_ID_HEVC;
    parser->stream_name     = "hevc";
    parser->dsi_FourCC      = "hvcC";
    parser->profile_levelID = 0;

    parser->dsi_type        = dsi_type;
    parser->dsi_create      = dsi_hevc_create;

    parser->init            = parser_hevc_init;
    parser->destroy         = parser_hevc_destroy;
    parser->get_sample      = parser_hevc_get_sample;

    parser->get_subsample   = parser_hevc_get_subsample;
    parser->copy_sample     = parser_hevc_copy_sample;

    OSAL_STRNCPY(parser->codec_name, 13, "\013HEVC Coding", 13);

    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->get_cfg = parser_hevc_get_mp4_cfg;
    }

    parser->get_param    = NULL; 
    parser->get_param_ex = NULL;

    /**** demux related api, currently hevc muxer don't need them */
    parser->show_info          = NULL; 
    parser->parse_codec_config = NULL; 

    /**** avc only,hevc don't need them */
    parser->need_fix_cts   = parser_hevc_need_fix_ctts;
    parser->get_cts_offset = parser_hevc_get_cts_offset;

    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->write_cfg = parser_hevc_write_mp4_cfg;
        parser->write_au  = parser_hevc_write_au;
    }

    /** use dsi list for the sake of multiple entries of stsd */
    if (dsi_list_create((parser_handle_t)parser, dsi_type))
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }
    parser->codec_config_lst  = list_create(sizeof(codec_config_t));
    parser->curr_codec_config = NULL;
    if (!parser->codec_config_lst)
    {
        parser_destroy((parser_handle_t)parser);
        return 0;
    }

    parser->dsi_enh = parser->dsi_create(dsi_type);
    if (!parser->dsi_enh)
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }

    parser->keep_all_nalus = 0;

    /***** cast to base */
    return (parser_handle_t)parser;
}

void
parser_hevc_reg(void)
{
    /** register all alias to make reg_parser_get easier */
    reg_parser_set("hevc", parser_hevc_create);
    reg_parser_set("hvc", parser_hevc_create);
    reg_parser_set("h265", parser_hevc_create);
    reg_parser_set("265",  parser_hevc_create);
}

