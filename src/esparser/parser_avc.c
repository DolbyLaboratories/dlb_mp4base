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
 *   @file parser_avc.c
 *   @brief Implements an AVC parser

 *  Based on ISO/IEC 14496-15:2010 PDAM
 */

#include "utils.h"
#include "list_itr.h"
#include "registry.h"
#include "dsi.h"
#include "parser.h"
#include "parser_avc_dec.h"
#include "parser_avc_dpb.h"

#include <stdarg.h>

#define PROFILE_134_TO_128  0

#define FISRT_DTS_DTS_IS_0 1 /* first dts = 0 */
#define TEST_DTS          (1 || CAN_TEST_DELTA_POC)
#define TEST_CTS          (1 && CAN_TEST_DELTA_POC)

#define MAX_DUMP_LINE_LEN 64

/* Dumps the avc ES into file test_es.h264 so we can do a binary comparision: keep the ES untouched */
#define TEST_NAL_ES_DUMP    0

#define NAL_IN_AU_MAX   128 /* to simplify code assume a static structure */
typedef struct nal_loc_t_
{
    int64_t  off;      /* offset of nal after sc at es file */
    size_t   size;     /* nal size exclude sc */
    size_t   sc_size;  /* nal sc size */

    uint8_t *buf_emb;  /* != NULL: the nal content is embedded */
} nal_loc_t;

typedef struct au_nals_t_
{
    int32_t   nal_idx;
    nal_loc_t nal_locs[NAL_IN_AU_MAX];
} au_nals_t;

typedef struct nal_t_
{
    bbio_handle_t ds;

    uint8_t *buffer;         /* buffer loaded with es for parsing */
    size_t   buf_size;       /* its size */
    size_t   data_size;      /* data in it */
    int32_t  sc_off;         /* start code offset */
    int32_t  sc_off_next;    /* next sc offset */

    offset_t  off_file;      /* offset of nal in file(ds) */
    uint8_t  *nal_buf;       /* point to the start of a nal defined by [sc_off, sc_off_next] */
    size_t    nal_size;      /* its size, including sc */
    size_t    sc_size;
    BOOL      nal_complete;  /* if get a complete nal */

    /** to aid parsing */
    uint8_t       *tmp_buf;
    uint32_t       tmp_buf_size;
    uint32_t       tmp_buf_data_size;
    bbio_handle_t  tmp_buf_bbi;
} nal_t;
typedef nal_t *nal_handle_t;

struct parser_avc_t_
{
    PARSER_VIDEO_BASE;

    int32_t keep_all_nalus;    /* 0: only keep that nalus in mdat box which are not defined in track header */
                               /* 1: keep all nalus in mdat box */

    dsi_handle_t dsi_enh;

    nal_t         nal;         /* nal buf and current nal info */
    au_nals_t     au_nals;     /* the composing nals of au */
    bbio_handle_t tmp_bbo;     /* the output handle of file */
    bbio_handle_t tmp_bbi;     /* the input handle of file */

    avc_decode_t dec;          /* current decoder status */
    avc_decode_t dec_el;       /* dolby vision el decoder status */

    avc_apoc_t *p_apoc;        /* apoc for cts calculation */

    uint32_t sample_size;
    uint32_t au_num;
    uint32_t au_ticks;

    uint32_t sps_num, pps_num, sps_ext_num;
    uint32_t sei_num;

    /* keep au timing info up to MinCts when SeiPicTiming is available */
    BOOL     bMinCtsKn;
    int32_t  i32PocMin;
    uint32_t u32MinCts;
#define CO_BUF_SIZE 4
    uint32_t au32CoTc[CO_BUF_SIZE];  /* cts offset in field# */

#if TEST_DTS
    int64_t delta_dts, dts_pre;
#endif
#if TEST_CTS
    avc_apoc_t *p_cts_apoc;
#endif

    /** validation */
    uint32_t validation_flags;
    uint32_t last_idr_pos;
    uint32_t max_idr_dist;
};

typedef struct parser_avc_t_ parser_avc_t;
typedef parser_avc_t  *parser_avc_handle_t;


/* stream validation */
#define VALFLAGS_NO_AUD           0x1  /* stream doesn't contain access unit delimiters */

static void parser_avc_ccff_validate       (parser_avc_handle_t parser_avc, avc_decode_t *hdec);
static int32_t  parser_avc_ccff_post_validation(parser_avc_handle_t parser_avc);

static int
parser_avc_post_validation(parser_handle_t parser)
{
    if (parser->reporter) {
        parser_avc_handle_t parser_avc = (parser_avc_handle_t) parser;
        if (IS_FOURCC_EQUAL(parser->conformance_type, "cffh") ||
            IS_FOURCC_EQUAL(parser->conformance_type, "cffs")) 
        {
            return parser_avc_ccff_post_validation(parser_avc);
        }
    }
    return 0;
}

static void
dump_info(bbio_handle_t sink, const char *format, ...)
{
    char buf[256];

    va_list vl;

    if (sink == NULL)
    {
        return;
    }

    va_start(vl, format);
#ifdef _MSC_VER
    vsprintf_s(buf, sizeof(buf), format, vl);
#else
    vsprintf(buf, format, vl);
#endif
    va_end(vl);

    sink->write(sink, (uint8_t *)buf, strlen(buf));
}

static void
dump_binhex_raw(bbio_handle_t sink, char *pStr)
{
    int32_t i;
    char s[MAX_DUMP_LINE_LEN+1];
    int32_t size = (int)strlen(pStr);

    if (sink == NULL)
    {
        return;
    }

    i = 0;
    while (size > 0)
    {
        if (size > MAX_DUMP_LINE_LEN)
        {
            OSAL_STRNCPY(s, MAX_DUMP_LINE_LEN+1, pStr+MAX_DUMP_LINE_LEN*i, MAX_DUMP_LINE_LEN);
            s[MAX_DUMP_LINE_LEN] = '\0';
        }
        else
        {
            OSAL_STRCPY(s, MAX_DUMP_LINE_LEN+1, pStr+MAX_DUMP_LINE_LEN*i);
        }
        i++;
        size -= MAX_DUMP_LINE_LEN;
        dump_info(sink, "%s", s);
    }
}

static void
dump_binhex(bbio_handle_t sink, char* tag, char *pStr)
{
    dump_info(sink, "<%s dt:dt=\"binary.base16\">", tag);
    dump_binhex_raw(sink, pStr);
    dump_info(sink, "</%s>\n", tag);
}

/* nal info file format:
 *   # of entry
 *   entries ...
 *
 *   entry:
 *       nal offset at es file, nal size, sc_size, embedded data if nal offset == -1
 * NOTE: here nal means that after sc
*/

/* MACRO for debug nal info file */
#define ADD_NAL_INFO_FILE_DEBUG_INFO    0
#if ADD_NAL_INFO_FILE_DEBUG_INFO
static
int32_t wr_prfix(bbio_handle_t snk)
{
    static uint32_t wr_id = 0;

    sink_write_u32(snk, wr_id);
    wr_id++;

    return 0;
}

static
int32_t rd_prfix(bbio_handle_t src)
{
    static uint32_t rd_id = 0;
    uint32_t        rd;

    if (src_rd_u32(src, &rd) != 0)
    {
        return 1;
    }

    if (rd != rd_id)
    {
        msglog(NULL, MSGLOG_ERR, "Parser avc: ERR: rd %u != should be %u\n", rd, rd_id);
    }
    rd_id++;

    return 0;
}

#define WR_PREFIX(snk) wr_prfix(snk)
#define RD_PREFIX(src) rd_prfix(src)

#else

#define WR_PREFIX(snk) 0
#define RD_PREFIX(src) 0

#endif

#define CHK_NAL_INFO_FILE_OFFSET    0
#if CHK_NAL_INFO_FILE_OFFSET
static
int32_t chk_file_offset(nal_handle_t nal)
{
    int64_t       pos;
    bbio_handle_t ds = nal->ds;
    uint32_t      sc_rd;

    pos = ds->position(ds);

    ds->seek(ds, nal->off_file, SEEK_SET);
    if (src_rd_u32(ds, &sc_rd) != 0)
    {
        return 1;
    }
    if (nal->sc_size == 4)
    {
        if (sc_rd != 0x00000001)
        {
            msglog(NULL, MSGLOG_WARNING, "Parser avc: ERR: sc read 0x%x != 0x00000001\n", sc_rd);
        }
    }
    else
    {
        sc_rd >>= 8;
        if (sc_rd != 0x000001)
        {
            msglog(NULL, MSGLOG_WARNING, "Parser avc: ERR: sc read 0x%x != 0x000001\n", sc_rd);
        }
    }

    ds->seek(ds, pos, SEEK_SET);

    return 0;
}
#define CHK_FILE_OFF(nal) chk_file_offset(nal)

#else

#define CHK_FILE_OFF(nal) 0

#endif


/* Returns the offset into buf where the sc is
 * sc_next == TRUE: skip the starting sc
 * return -1 for no sc found
 */
static int32_t
find_sc_off(uint8_t *buf, size_t buf_size, BOOL sc_next)
{
    uint32_t val;
    uint8_t *buf0    = buf;
    uint8_t *buf_top = buf + buf_size;

    if (buf_size < 4)
    {
        /* 4: sc at least 3 bytes + 1 nal hdr */
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
            buf = buf0;  /* to keep going */
        }
    }

    /** get next current start code */
    val = 0xffffffff;
    while (buf < buf_top)
    {
        val <<= 8;
        val |= *buf++;
        if ((val & 0x00ffffff) == AVC_START_CODE)
        {
            if (val == AVC_START_CODE)
            {
                return (int32_t)((buf - buf0) - 4);
            }
            return (int32_t)((buf - buf0) - 3);
        }
    }

    return -1;
}

/* assuming sc_off_next point to next(now of interest) nal */
static BOOL
get_a_nal(nal_handle_t nal)
{
    int32_t sc_off_next, off0;
    size_t bytes_read, bytes_avail;

    /** next nal starts at where last one end */
    nal->sc_off = nal->sc_off_next;
    nal->off_file += nal->nal_size;

    bytes_avail = nal->data_size - nal->sc_off;
    sc_off_next = find_sc_off(nal->buffer + nal->sc_off, bytes_avail, TRUE);
      /* TRUE: skip the start code of current nal */
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
        /* get enough to parse */
        nal->nal_buf      = nal->buffer + nal->sc_off;
        nal->nal_size     = bytes_avail;                     /* data got so far */
        nal->sc_size      = (nal->nal_buf[2] == 1) ? 3 : 4;
        nal->nal_complete = FALSE;
        return TRUE;
    }

    /** need more data */
    /* discard data before sc_off. move data to offset 0, leave room to load more data */
    nal->data_size = bytes_avail;
    if (nal->data_size)
    {
        memmove(nal->buffer, nal->buffer + nal->sc_off, bytes_avail);
    }
    nal->sc_off = 0;
    nal->nal_buf = nal->buffer;
    /* search starts at right position to avoid double search and skip current nal sc */
    if (nal->data_size > 4)
    {
        /* already searched up to data_size. off0 > 1. -3: may got 3 0s */
        off0 = (int32_t)(nal->data_size - 3);
    }
    else if (nal->data_size > 2)
    {
        /* skip 2 0s */
        off0 = 2;
    }
    else
    {
        off0 = 0;  /* only at the first or after last nal */
    }

    /* load */
    bytes_read = nal->ds->read(nal->ds, nal->buffer + nal->data_size, nal->buf_size - nal->data_size);
    nal->sc_size = (nal->nal_buf[2] == 1) ? 3 : 4;
      /* (1) init will report EOES if total data size < 4.
       * (2) if reach EOES, retrun FALSE and sc_size does not matter
       */
    if (bytes_read == 0)
    {
        if (nal->data_size)
        {
            /* end of source and has last nal */
            nal->sc_off_next = (int32_t)nal->data_size;

            nal->nal_size     = nal->sc_off_next;
            nal->nal_complete = TRUE;
            return TRUE;
        }
        nal->nal_complete = TRUE;
        return FALSE;  /* nal->data_size == 0 and bytes_read == 0: done */
    }

    /** try to search again */
    nal->data_size += bytes_read;
    sc_off_next = find_sc_off(nal->buffer + off0, nal->data_size - off0, off0 == 0);
    if (sc_off_next >= 0)
    {
        /* got it ! */
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
        return TRUE;  /* done */
    }

    /** return TRUE when we get enough nal data to parse or close to end of file */
    nal->nal_size     = nal->data_size;  /* data got so far */
    nal->nal_complete = FALSE;
    return TRUE;
}

/* Finds out the end of nal and nal size if !nal_complete */
static BOOL
skip_the_nal(nal_handle_t nal)
{
    size_t  bytes_read;
    int32_t sc_off_next;

    if (nal->nal_complete)
    {
        return FALSE;  /* already done */
    }

    assert(nal->nal_size >= 2048);
    do {
        /* keep the last three byte and load more data */
        nal->buffer[0] = nal->buffer[nal->data_size - 3];
        nal->buffer[1] = nal->buffer[nal->data_size - 2];
        nal->buffer[2] = nal->buffer[nal->data_size - 1];
        bytes_read = nal->ds->read(nal->ds, nal->buffer + 3, nal->buf_size - 3);

        nal->data_size = 3 + bytes_read;  /* data in buffer */
        if (!bytes_read)
        {
            nal->sc_off_next = 3;  /* fake a sc at offset 3 */
            /* nal_size unchanged: up to end of file */
            return TRUE;
        }

        sc_off_next = find_sc_off(nal->buffer, bytes_read + 3, FALSE);
        if (sc_off_next >= 0)
        {
            nal->sc_off_next  = sc_off_next;
            nal->nal_size    += sc_off_next - 3;  /* -3 => each byte count once */
            return TRUE;
        }

        nal->nal_size += bytes_read;
    }
    while (TRUE);
}

/** Returns true if new SPS or PPS inside nal will trigger writing of new sample description box
    because there is already a SPS or PPS with same id but different content in plist. */
static BOOL
ps_list_is_there_collision(list_handle_t *plist, uint8_t id, nal_handle_t nal)
{
    it_list_handle_t it = NULL;
    buf_entry_t *    entry = NULL;
    BOOL             ret = FALSE;

    if (!*plist)
    {
        /* List does not have content at all */
        return FALSE;
    }

    it = it_create();
    it_init(it, *plist);
    while ((entry = it_get_entry(it)) && entry->id != id)
        continue;

    if (entry)
    {
        /* Do existing and new entry have the same content? */
        if (entry->size == nal->nal_size - nal->sc_size &&
           !memcmp(entry->data, nal->nal_buf + nal->sc_size, entry->size))
        {
            /* we get here if the NALs are identical */
            ret = FALSE;
        }
        else
        {
            /* same ID but different content (spliced stream) */
            ret = TRUE;
        }
    }

    it_destroy(it);
    return ret;
}

/** Returns true if SPS/PPS should be copied in the stream */
static BOOL
ps_list_update(parser_avc_handle_t parser, list_handle_t *plist, uint8_t id, nal_handle_t nal, uint32_t *sample_flag)
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
        /* Do existing and new entry have the same content? */
        if (entry->size == nal->nal_size - nal->sc_size &&
           !memcmp(entry->data, nal->nal_buf + nal->sc_size, entry->size))
        {
            /* we get here if the NALs are identical */
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
            /* same ID but different content (spliced stream) */
            /* copy content in plist only */
            if (entry->size != (size_t)(nal->nal_size - nal->sc_size))
            {
                /* we don't have enough space in this entry */
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
                /* single sample description entry */
                msglog(NULL, MSGLOG_ERR, "Error: Multiple Sample Descriptions necessary but not allowed!\n");
                parser->sd_collision_flag = 1;
            }
            else if (parser->sd == 1)
            {
                /* multiple sample description entries */
                if (sample_flag)
                {
                    *sample_flag |= SAMPLE_NEW_SD;
                }
                ret = FALSE;
            }
        }
    }
    else
    {
        /* new entry in list */
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

#ifdef DEBUG
static const char *sei_payloadType_tbl[19] = {
    "buffering_period",
    "pic_timing",
    "pan_scan_rect",
    "filler_payload",
    "user_data_registered_itu_t_t35",
    "user_data_unregistered",
    "recovery_point",
    "dec_ref_pic_marking_repetition",
    "spare_pic",
    "scene_info",
    "sub_seq_info",
    "sub_seq-layer_characteristics",
    "full_frame_freeze",
    "full_frame_freeze_release",
    "full_frame_snapshot",
    "progressive_refinement_segment_start",
    "progressive_refinement_segment_end",
    "motioned_constrained_slice_group_set"
};

static const char *
get_sei_payloadType_dscr(uint8_t type)
{
    if (type < 19)
    {
        return sei_payloadType_tbl[type];
    }
    if (type > 35)
    {
        return "reserved_sei_message";
    }

    return "not care";
}
#endif

/* read the type or value of sei */
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
add_0x03(uint8_t *dst, size_t *dstlen, const uint8_t *src, const size_t srclen)
{
    const uint8_t *srcend = (src + srclen) - 2;
    uint8_t  * const dstsav = dst;

    *dstlen  = 0;
    while (src < srcend)
    {
        if (!src[0] && !src[1] && src[2] < 4)
        {
            *dst++ = 0;
            *dst++ = 0;
            *dst++ = 3;
            src += 2;
            continue;
        }
        *dst++ = *src++;
    }

    srcend += 2;
    while (src < srcend)
    {
        *dst++ = *src++;
    }

    *dstlen = dst - dstsav;
}

/* NumClockTS derived from pic_struct */
static const uint32_t num_clock_ts_from_pic_struct[16] = {1, 1, 1, 2, 2, 3, 3, 2, 3};

/* Returns size of sei nal to keep */
static uint32_t
parse_sei_messages(avc_decode_t *dec, nal_handle_t nal, BOOL keep_all)
{
    uint32_t msg_off, off, nal_hdr_size = (nal->nal_buf[2] == 1) ? 4 : 5;
    uint32_t nal_size_no_tz = (uint32_t)nal->nal_size, trailing_zero = 0;
    uint32_t payloadType, payloadSize, size, sei_keep_size;
    sps_t *  p_active_sps;
    BOOL     no_discard_sei = TRUE;
    size_t   tmp_buf_data_size;

    /****** get rid of trailing zero since "nal" here includes trailing zero  */
    while (nal_size_no_tz != 0 && nal->nal_buf[nal_size_no_tz - 1] == 0)
    {
        trailing_zero++;
        nal_size_no_tz--;
    }

    /****** remove 0x03 */
    if (nal->tmp_buf_size < nal_size_no_tz)
    {
        FREE_CHK(nal->tmp_buf);
        nal->tmp_buf = MALLOC_CHK(nal_size_no_tz);
        if (!nal->tmp_buf)
        {
            msglog(NULL, MSGLOG_ERR, "ERR: malloc fail\n");
            nal->tmp_buf_size = 0;
            /* just keep nal */
            return (uint32_t)nal->nal_size;
        }
        else
        {
            nal->tmp_buf_size = nal_size_no_tz;
        }
    }
    /* get rid of start code and nal hdr, remove 0x03 */
    parser_avc_remove_0x03(nal->tmp_buf, &tmp_buf_data_size,
                           nal->nal_buf + nal_hdr_size, nal_size_no_tz - nal_hdr_size);
    nal->tmp_buf_data_size = (uint32_t)tmp_buf_data_size;
    nal->tmp_buf_bbi->set_buffer(nal->tmp_buf_bbi, nal->tmp_buf, nal->tmp_buf_data_size, 0);

    /****** sei parsing and discarding */
    DPRINTF(NULL, "   total SEI msg RBSP len %u\n", nal->tmp_buf_data_size);
    sei_keep_size = nal->tmp_buf_data_size;
    off = 0;
    while (off + 2 < sei_keep_size)
    {
        /** loop on SEI msg one by one when there is minimum data available */
        msg_off = off;  /* SEI msg start offset */

        read_sei_tv(nal->tmp_buf_bbi, &size, &payloadType);
        DPRINTF(NULL, "   sei payloadType %u(%s)(field size %u) @offset %d\n",
                payloadType, get_sei_payloadType_dscr((uint8_t)payloadType), size, off);
        off += size;

        read_sei_tv(nal->tmp_buf_bbi, &size, &payloadSize);
        off += size;  /* at payload */
        DPRINTF(NULL, "   sei payloadSize %u(field size %u) %d bytes sei left\n",
                payloadSize, size, sei_keep_size - off - payloadSize);

        if (off + payloadSize >= sei_keep_size)
        {
            msglog(NULL, MSGLOG_WARNING, "Error decoding sei message\n");
            return (uint32_t)nal->nal_size;  /* to keep all */
        }

        switch (payloadType)
        {
        case SEI_FILLER_PAYLOAD:
        case SEI_SUB_SEQ_INFO:
        case SEI_SUB_SEQ_LAYER_CHARACTERISTICS:
        case SEI_SUB_SEQ_CHARACTERISTICS:
            /** keep every thing */
            if (keep_all)
            {
                break;
            }
#if TEST_NAL_ES_DUMP
            break;
#endif
            /** sei to discard */
            no_discard_sei = FALSE;
            off += payloadSize;  /* next sei msg to be */
            memmove(nal->tmp_buf + msg_off, nal->tmp_buf + off , sei_keep_size - off);
            sei_keep_size -= off - msg_off;

            off = msg_off;
            nal->tmp_buf_bbi->set_buffer(nal->tmp_buf_bbi, nal->tmp_buf, sei_keep_size, 0);
            nal->tmp_buf_bbi->seek(nal->tmp_buf_bbi, off, SEEK_SET);
            continue;

        case SEI_BUFFERING_PERIOD:
            if (msg_off != 0)
            {
                /* must be first SEI msg */
                msglog(NULL, MSGLOG_WARNING, "buffering period is not first SEI\n");
                return (uint32_t)nal->nal_size;  /* to keep all */
            }
            size = src_read_ue(nal->tmp_buf_bbi);  /* borrow size */
            if (size > 15 || dec->sps[size].isDefined == 0)
            {
                msglog(NULL, MSGLOG_ERR, "seq_parameter_set_id in SEI BP wrong\n");
                if (dec->sps[0].isDefined == 0)
                {
                    break;
                }
                msglog(NULL, MSGLOG_ERR, "Assume seq_parameter_set_id = 0\n");
                size = 0;
            }

            /* activation */
            dec->active_sps = dec->sps + size;

            p_active_sps = dec->active_sps;
            if (p_active_sps && p_active_sps->CpbDpbDelaysPresentFlag)
            {
                /* for now just get 1st and last one, prefer that of nal: refer to vui */
                uint32_t u, temp;
                for (u = 0; u <= p_active_sps->cpb_cnt_minus1; u ++)
                {
                    temp = src_read_bits(nal->tmp_buf_bbi, p_active_sps->initial_cpb_removal_delay_length_minus1+1);
                    if (u == 0)
                    {
                        dec->initial_cpb_removal_delay_1st = temp;
                    }
                    if (u == p_active_sps->cpb_cnt_minus1)
                    {
                        dec->initial_cpb_removal_delay_last = temp;
                    }

                    src_read_bits(nal->tmp_buf_bbi, p_active_sps->initial_cpb_removal_delay_length_minus1+1);
                }
                /* get enough and done */
                msglog(NULL, MSGLOG_DEBUG, "     initial_cpb_removal_delay_1st %u, last %u\n",
                       dec->initial_cpb_removal_delay_1st, dec->initial_cpb_removal_delay_last);

                dec->NewBpStart = 1;
            }
            else
            {
                msglog(NULL, MSGLOG_WARNING, "     get SEI_BUFFERING_PERIOD but Nal/VclHrdBpPresentFlag not on\n");
            }
            break;

        case SEI_PIC_TIMING:
            p_active_sps = dec->active_sps;
            if (p_active_sps && p_active_sps->CpbDpbDelaysPresentFlag)
            {
                dec->cpb_removal_delay = src_read_bits(nal->tmp_buf_bbi, p_active_sps->cpb_removal_delay_length_minus1+1);
                dec->dpb_output_delay  = src_read_bits(nal->tmp_buf_bbi, p_active_sps->dpb_output_delay_length_minus1+1);
                msglog(NULL, MSGLOG_DEBUG, "     cpb_removal_delay %u, dpb_output_delay %u\n",
                       dec->cpb_removal_delay, dec->dpb_output_delay);
            }
            if (p_active_sps && p_active_sps->pic_struct_present_flag)
            {
                uint32_t num_clock_ts, u, tmp;
                dec->pic_struct = (uint8_t)src_read_bits(nal->tmp_buf_bbi,4);

                msglog(NULL, MSGLOG_DEBUG, "    pic_struct %u\n", dec->pic_struct);
                num_clock_ts = num_clock_ts_from_pic_struct[dec->pic_struct];
                for (u = 0; u < num_clock_ts; u++)
                {
                    tmp = src_read_bit(nal->tmp_buf_bbi);
                    if (tmp)
                    {
                        uint32_t ct_type, nuit_field_based_flag, counting_type, full_timestamp_flag;
                        uint32_t discontinuity_flag, cnt_dropped_flag, n_frames;
                        uint32_t seconds_value, minutes_value, hours_value;

                        ct_type               = src_read_bits(nal->tmp_buf_bbi, 2);
                        nuit_field_based_flag = src_read_bits(nal->tmp_buf_bbi, 1);
                        counting_type         = src_read_bits(nal->tmp_buf_bbi, 5);
                        full_timestamp_flag   = src_read_bits(nal->tmp_buf_bbi, 1);
                        discontinuity_flag    = src_read_bits(nal->tmp_buf_bbi, 1);
                        cnt_dropped_flag      = src_read_bits(nal->tmp_buf_bbi, 1);
                        n_frames              = src_read_bits(nal->tmp_buf_bbi, 8);
                        msglog(NULL, MSGLOG_DEBUG, "      ct_type %u, nuit_field_based_flag %u, counting_type %u\n",
                               ct_type, nuit_field_based_flag, counting_type);
                        msglog(NULL, MSGLOG_DEBUG, "      full_timestamp_flag %u, discontinuity_flag %u, cnt_dropped_flag %u\n",
                               full_timestamp_flag, discontinuity_flag, cnt_dropped_flag);
                        msglog(NULL, MSGLOG_DEBUG, "      n_frames %u\n", n_frames);
                        if (full_timestamp_flag)
                        {
                            seconds_value = src_read_bits(nal->tmp_buf_bbi, 6);
                            msglog(NULL, MSGLOG_DEBUG, "        seconds_value %u\n", seconds_value);
                            minutes_value = src_read_bits(nal->tmp_buf_bbi, 6);
                            msglog(NULL, MSGLOG_DEBUG, "        minutes_value %u\n", minutes_value);
                            hours_value   = src_read_bits(nal->tmp_buf_bbi, 5);
                            msglog(NULL, MSGLOG_DEBUG, "        hours_value %u\n", hours_value);
                        }
                        else
                        {
                            if (src_read_bit(nal->tmp_buf_bbi))
                            {
                                seconds_value = src_read_bits(nal->tmp_buf_bbi, 6);
                                msglog(NULL, MSGLOG_DEBUG, "        seconds_value %u\n", seconds_value);
                                if (src_read_bit(nal->tmp_buf_bbi))
                                {
                                    minutes_value = src_read_bits(nal->tmp_buf_bbi, 6);
                                    msglog(NULL, MSGLOG_DEBUG, "        minutes_value %u\n", minutes_value);
                                    if (src_read_bit(nal->tmp_buf_bbi))
                                    {
                                        hours_value = src_read_bits(nal->tmp_buf_bbi, 5);
                                        msglog(NULL, MSGLOG_DEBUG, "        hours_value %u\n", hours_value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;

        case SEI_FRAME_PACKING:
            {
                uint32_t frame_packing_arrangement_cancel_flag;
                uint32_t i = 0;

                while (src_read_bits(nal->tmp_buf_bbi, 1) == 0)
                {
                    i++;
                }
                src_read_bits(nal->tmp_buf_bbi, i);                                /* frame_packing_arrangement_id */

                frame_packing_arrangement_cancel_flag = src_read_bits(nal->tmp_buf_bbi, 1);
                if (!frame_packing_arrangement_cancel_flag)
                {
                    uint32_t quincunx_sampling_flag;

                    dec->frame_packing_type = src_read_bits(nal->tmp_buf_bbi, 7);
                    quincunx_sampling_flag  = src_read_bits(nal->tmp_buf_bbi, 1);
                    src_read_bits(nal->tmp_buf_bbi, 6);                            /* content_interpretation_type */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* spatial_flipping_flag */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* frame0_flipped_flag */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* field_views_flag */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* current_frame_is_frame0_flag */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* frame0_self_contained_flag */
                    src_read_bits(nal->tmp_buf_bbi, 1);                            /* frame1_self_contained_flag */
                    if (!quincunx_sampling_flag && dec->frame_packing_type != 5)
                    {
                        src_read_bits(nal->tmp_buf_bbi, 4);                        /* frame0_grid_position_x */
                        src_read_bits(nal->tmp_buf_bbi, 4);                        /* frame0_grid_position_y */
                        src_read_bits(nal->tmp_buf_bbi, 4);                        /* frame1_grid_position_x */
                        src_read_bits(nal->tmp_buf_bbi, 4);                        /* frame1_grid_position_y */
                    }
                    src_read_bits(nal->tmp_buf_bbi, 8);                            /* frame_packing_arrangement_reserved_byte */

                    i = 0;
                    while (src_read_bits(nal->tmp_buf_bbi, 1) == 0)
                    {
                        i++;
                    }
                    src_read_bits(nal->tmp_buf_bbi, i);                            /* frame_packing_arrangement_repetition_period */
                }
                else
                {
                    msglog(NULL, MSGLOG_WARNING, "clearing SEI_FRAME_PACKING info is not supported\n");
                }
                src_read_bits(nal->tmp_buf_bbi, 1);                                /* frame_packing_arrangement_extension_flag */
            }
            break;

        default:
            /* keep */
            break;
        }

        /* keep the nal */
        off += payloadSize;
        src_byte_align(nal->tmp_buf_bbi);  /* empty the possible cached bits */
        nal->tmp_buf_bbi->seek(nal->tmp_buf_bbi, off, SEEK_SET);
    }

    assert(off < sei_keep_size);  /* there must be rbsp_trailing bits */

    if (off + 1 == sei_keep_size && nal->tmp_buf[off] == 0x80)
    {
        msglog(NULL, MSGLOG_DEBUG, "get trailing in SEI\n");
        if (!no_discard_sei)
        {
            /* put back 0x3 */
            add_0x03(nal->nal_buf + nal_hdr_size, (size_t*)&sei_keep_size,
                     nal->tmp_buf , sei_keep_size);
            /* include start code and nal hdr */
            sei_keep_size += nal_hdr_size;
            /* put back trailing zero if any */
            while (trailing_zero--)
            {
                nal->nal_buf[sei_keep_size++] = 0;
            }
            return sei_keep_size;
        }
        return (uint32_t)nal->nal_size;  /* no change to sei nal */
    }

    msglog(NULL, MSGLOG_WARNING, "Error decoding sei message\n");
    return (uint32_t)nal->nal_size;  
}

static void
get_colr_info(parser_avc_handle_t parser_avc, sps_t *p_sps)
{
    parser_avc->colour_primaries = p_sps->colour_primaries;
    parser_avc->transfer_characteristics = p_sps->transfer_characteristics;
    parser_avc->matrix_coefficients = p_sps->matrix_coefficients;
}

static void
timing_info_update(parser_avc_handle_t parser_avc, sps_t *p_sps)
{
    BOOL     frame_only = FALSE;

    if (!p_sps->timing_info_present_flag || parser_avc->ext_timing.override_timing)
    {
        assert(parser_avc->ext_timing.num_units_in_tick != 0);

        if (p_sps->num_units_in_tick != parser_avc->ext_timing.num_units_in_tick ||
            p_sps->time_scale != 2 * parser_avc->ext_timing.time_scale ||
            p_sps->fixed_frame_rate_flag == 0)
        {
            char *why = (p_sps->num_units_in_tick) ? "mismatch" : "miss";
            msglog(NULL, MSGLOG_WARNING, " Timing info %sing. use ext timing with fix frame rate %.2f\n",
                   why, ((float)parser_avc->ext_timing.time_scale)/parser_avc->ext_timing.num_units_in_tick); (void)why;

            p_sps->num_units_in_tick     = parser_avc->ext_timing.num_units_in_tick;
            p_sps->time_scale            = 2 * parser_avc->ext_timing.time_scale;     /* convert to field unit */
            p_sps->fixed_frame_rate_flag = 1;
        }
    }

    parser_avc->num_units_in_tick = p_sps->num_units_in_tick;
    parser_avc->time_scale        = p_sps->time_scale;

    if (p_sps->frame_mbs_only_flag || parser_avc->dec.slice->field_pic_flag == 0 || frame_only)
    {
        parser_avc->au_ticks    = parser_avc->num_units_in_tick;
        parser_avc->time_scale /= 2;
    }
    else
    {
        parser_avc->au_ticks = parser_avc->num_units_in_tick;
    }

    parser_avc->framerate = parser_avc->time_scale / parser_avc->num_units_in_tick;
    {
        uint32_t level = parser_avc->width * parser_avc->height * parser_avc->framerate;

        if ((parser_avc->dv_el_nal_flag== 0) 
            && (parser_avc->dv_rpu_nal_flag == 1) 
            && (parser_avc->ext_timing.ext_dv_profile != 9))
        {
            level = level * 4;
        }

        if (level <= 1280*720*24)
        {
            parser_avc->dv_level = 1;
        }
        else if (level <= 1280*720*30)
        {
            parser_avc->dv_level = 2;
        }
        else if (level <= 1920*1080*24)
        {
            parser_avc->dv_level = 3;
        }
        else if (level <= 1920*1080*30)
        {
            parser_avc->dv_level = 4;
        }
        else if (level <= 1920*1080*60)
        {
            parser_avc->dv_level = 5;
        }
        else if (level <= 3840*2160*24)
        {
            parser_avc->dv_level = 6;
        }
        else if (level <= 3840*2160*30)
        {
            parser_avc->dv_level = 7;
        }
        else if (level <= 3840*2160*48)
        {
            parser_avc->dv_level = 8;
        }
        else if (level <= 3840*2160*60)
        {
            parser_avc->dv_level = 9;
        }
    }
}

static int
save_au_nals_info(au_nals_t *au_nals, mp4_sample_handle_t sample, bbio_handle_t snk)
{
    nal_loc_t *nal_loc, *nal_loc_end;

    sample->pos = snk->position(snk);  /* into the nal info file */
    if (sample->data)
    {
        /* data=0 for nal info type sample data */
        FREE_CHK(sample->data);
        sample->data = 0;
    }

    assert(au_nals->nal_idx);
    /* save sample's au structure and location at es file */
    if (WR_PREFIX(snk) != 0)
    {
        return EMA_MP4_MUXED_WRITE_ERR;
    }

    sink_write_u32(snk, au_nals->nal_idx);  /* # of nal in au */

    nal_loc = au_nals->nal_locs;
    nal_loc_end = nal_loc + au_nals->nal_idx;
    while (nal_loc < nal_loc_end)
    {
        sink_write_u64(snk, nal_loc->off);                  /* nal body at es file. -1 embedded */
        sink_write_u32(snk, (uint32_t)nal_loc->size);       /* nal body size */
        sink_write_u8(snk,  (uint8_t)nal_loc->sc_size);     /* nal sc size */
        if (nal_loc->buf_emb)
        {
            /* save nal body only */
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
/* verify delta dts is a constant */
static void
verify_dts(parser_avc_handle_t parser_avc, mp4_sample_handle_t sample)
{
    if (parser_avc->au_num > 1)
    {
        int64_t d_d = sample->dts - parser_avc->dts_pre;
        if (parser_avc->delta_dts != d_d)
        {
            msglog(NULL, MSGLOG_WARNING, "delta dts changed %" PRIi64 "=>%" PRIu64 "\ndts %" PRIi64 "=>\n    %" PRIi64 "=>\n    %" PRIu64 "\n",
                   parser_avc->delta_dts, sample->dts - parser_avc->dts_pre,
                   parser_avc->dts_pre - parser_avc->delta_dts, parser_avc->dts_pre, sample->dts);

            if (d_d <= 0)
            {
                msglog(NULL, MSGLOG_WARNING, "force delta dts the same\n");
                sample->dts = parser_avc->dts_pre + parser_avc->delta_dts;
            }
            else
            {
                parser_avc->delta_dts = sample->dts - parser_avc->dts_pre;
            }
        }
    }
    else if (parser_avc->au_num)
    {
        parser_avc->delta_dts = sample->dts - parser_avc->dts_pre;
    }
    parser_avc->dts_pre = sample->dts;
}
#else
#define verify_dts(parser_avc, sample);
#endif

#if TEST_CTS
/* verify delta cts is a constant */
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
verify_ts_report(parser_avc_handle_t parser_avc)
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
dsi_update(dsi_avc_handle_t dsi_avc, sps_t *sps)
{
    dsi_avc->AVCProfileIndication  = sps->profile_idc;
    dsi_avc->profile_compatibility = sps->compatibility;
    dsi_avc->AVCLevelIndication    = sps->level_idc;

    switch (dsi_avc->dsi_type)
    {
    case DSI_TYPE_MP4FF:
        {
            mp4_dsi_avc_handle_t mp4ff_dsi = (mp4_dsi_avc_handle_t)dsi_avc;

            mp4ff_dsi->configurationVersion = 1;
            mp4ff_dsi->chroma_format        = (uint8_t)sps->chroma_format_idc;
            mp4ff_dsi->bit_depth_luma       = (uint8_t)sps->bit_depth_luma_minus8 + 8;
            mp4ff_dsi->bit_depth_chroma     = (uint8_t)sps->bit_depth_chroma_minus8 + 8;
            break;
        }

    default:
        break;
    }
}

static int32_t 
incr_nal_idx( au_nals_t * au_nals )
{
    au_nals->nal_idx++;
    if (au_nals->nal_idx >= NAL_IN_AU_MAX)
    {
        msglog(NULL, MSGLOG_DEBUG, "\ninvalid number of nal indexes\n");
        assert(NULL);
        return EMA_MP4_MUXED_BUGGY;
    }

    return EMA_MP4_MUXED_OK;
}

#if TEST_NAL_ES_DUMP  /*!TEST_NAL_ES_DUMP: embedded nal not supported yet */
/* return the actual added size */
static uint32_t
add_AUD(au_nals_t *au_nals, BOOL keep_scp)
{
    nal_loc_t *nal_loc =  au_nals->nal_locs + au_nals->nal_idx;

    nal_loc->off = -1;  /* embedded. not care */
    if (keep_scp)
    {
        nal_loc->size    = 6;
        nal_loc->sc_size = 0;
    }
    else
    {
        nal_loc->size    = 2;
        nal_loc->sc_size = 4;
    }

    nal_loc->buf_emb = (uint8_t *)MALLOC_CHK(nal_loc->size);
    if (keep_scp)
    {
        nal_loc->buf_emb[0] = 0;
        nal_loc->buf_emb[1] = 0;
        nal_loc->buf_emb[2] = 0;
        nal_loc->buf_emb[3] = 1;
    }
    nal_loc->buf_emb[nal_loc->size - 2] = NAL_TYPE_ACCESS_UNIT;
    nal_loc->buf_emb[nal_loc->size - 1] = 0xf0;  /* primary_pic_type = 0x7 << 5 | 0x10 */
                                                 /* 0x7: we do not know the exact type yet */
    msglog(NULL, MSGLOG_DEBUG, "\nAUD added\n");
    if (incr_nal_idx(au_nals) != EMA_MP4_MUXED_OK)
        return 0;

    return nal_loc->size;
}

static uint32_t
add_end_of_stram(au_nals_t *au_nals, BOOL keep_scp)
{
    nal_loc_t *nal_loc =  au_nals->nal_locs + au_nals->nal_idx;

    nal_loc->off = -1;  /* embedded. not care */
    if (keep_scp)
    {
        nal_loc->size    = 4;
        nal_loc->sc_size = 0;
    }
    else
    {
        nal_loc->size    = 1;
        nal_loc->sc_size = 3;
    }

    nal_loc->buf_emb = (uint8_t *)MALLOC_CHK(nal_loc->size);
    if (keep_scp)
    {
        nal_loc->buf_emb[0] = 0;
        nal_loc->buf_emb[1] = 0;
        nal_loc->buf_emb[2] = 1;
    }
    nal_loc->buf_emb[nal_loc->size - 1] = NAL_TYPE_END_OF_STREAM;
    msglog(NULL, MSGLOG_INFO, "  EOStrm added\n");
    if (incr_nal_idx(au_nals) != EMA_MP4_MUXED_OK)
        return 0;

    return nal_loc->size;
}
#endif

/* Create a new entry in parser->dsi_lst and copy content from current dsi in the new dsi entry.
 * After copying the former "new" dsi will be the "current" dsi to be worked with from there on.
 * Returns error code.
 */
static int
parser_avc_clone_dsi(parser_handle_t parser) 
{
    /* Create new entry in stsd list */
    dsi_handle_t    new_dsi = parser->dsi_create(parser->dsi_type);
    dsi_handle_t* p_new_dsi = NULL;

    mp4_dsi_avc_handle_t     mp4ff_dsi = (mp4_dsi_avc_handle_t)parser->curr_dsi;
    mp4_dsi_avc_handle_t new_mp4ff_dsi = (mp4_dsi_avc_handle_t)new_dsi;

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

    /* First copy content of dsi_avc_t struct itself (stream id, profile indications, etc.) */
    memcpy(new_dsi, parser->curr_dsi, sizeof(dsi_avc_t));

    /* Copy PPS list */
    if (mp4ff_dsi->pps_lst)
    {
        /* New list not existent yet */
        new_mp4ff_dsi->pps_lst = list_create(sizeof(buf_entry_t));

        /* Copy entries one by one
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
                return EMA_MP4_MUXED_NO_MEM;
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

    /* Copy SPS list */
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
                return EMA_MP4_MUXED_NO_MEM;
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

    /* Copy SPS ext list */
    if (mp4ff_dsi->sps_ext_lst)
    {
        new_mp4ff_dsi->sps_ext_lst = list_create(sizeof(buf_entry_t));

        for (i = 0; i < list_get_entry_num(mp4ff_dsi->sps_ext_lst); i++)
        {
            it_init(it, mp4ff_dsi->sps_ext_lst);
            entry = (buf_entry_t*)it_get_entry(it);
            if (!entry)
                continue;

            new_entry       = (buf_entry_t*)list_alloc_entry(new_mp4ff_dsi->sps_ext_lst);
            if (!new_entry)
            {
                list_free_entry(p_new_dsi);
                new_dsi->destroy(new_dsi);
                it_destroy(it);
                return EMA_MP4_MUXED_NO_MEM;
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

            list_add_entry(new_mp4ff_dsi->sps_ext_lst, new_entry);
        }
    }

    /* Copy rest of mp4_dsi_avc_t struct */
    new_mp4ff_dsi->configurationVersion = mp4ff_dsi->configurationVersion;
    new_mp4ff_dsi->chroma_format        = mp4ff_dsi->chroma_format;
    new_mp4ff_dsi->bit_depth_chroma     = mp4ff_dsi->bit_depth_chroma;
    new_mp4ff_dsi->bit_depth_luma       = mp4ff_dsi->bit_depth_luma;
    new_mp4ff_dsi->dsi_in_mdat          = mp4ff_dsi->dsi_in_mdat;

    /* Switch to new entry in stsd list */
    list_add_entry(parser->dsi_lst, p_new_dsi);
    parser->curr_dsi = new_dsi;

    it_destroy(it);

    return EMA_MP4_MUXED_OK;
}


/**
 * Parse Network Abstraction Layer Units (NALUs)
 */
static int
parser_avc_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    parser_avc_handle_t  parser_avc = (parser_avc_handle_t)parser;
    dsi_avc_handle_t     dsi_avc    = (dsi_avc_handle_t)parser->curr_dsi;
    mp4_dsi_avc_handle_t mp4ff_dsi  = (mp4_dsi_avc_handle_t)dsi_avc;
    nal_handle_t         nal        = &(parser_avc->nal);
    au_nals_t *          au_nals    = &(parser_avc->au_nals);
    avc_decode_t *       dec        = &(parser_avc->dec);
    avc_decode_t *       dec_el     = &(parser_avc->dec_el);

    sps_t *              p_active_sps;
    pps_t *              p_active_pps;

    uint32_t   sc_size;
    BOOL       new_au_start= FALSE, old_au_end = FALSE;
    BOOL       keep_nal= FALSE, keep_all= FALSE;
    int32_t    nal_in_au = 0;
    nal_loc_t *nal_loc = NULL;
    uint32_t   sei_size2keep = 0;  /* no sei to keep, or not a sei */

    uint8_t idr_nal_ref_idc = 0;
    uint8_t vcl_nal_ref_idc = 0;

    BOOL found_aud = FALSE;
    int32_t err = EMA_MP4_MUXED_OK;

    BOOL single_sps_flag = TRUE;

    sample->flags = 0;  /* reset flag */

    /* Initialization. */
    dec->sample_has_redundancy    = FALSE;
    sample->is_leading            = 0;
    sample->sample_depends_on     = 0;
    sample->sample_is_depended_on = 0;
    sample->sample_has_redundancy = 0;
    sample->dependency_level      = 0;
    sample->pic_type              = 0;
    sample->frame_type            = 0xff;
    
    if (IS_FOURCC_EQUAL(parser->dsi_name,"avc3") && parser->ext_timing.ps_present_flag != 2)
    {
        mp4ff_dsi->dsi_in_mdat = 1;
    }
    else
    {
        mp4ff_dsi->dsi_in_mdat = 0;
    }
#if PARSE_DURATION_TEST
    if (parser_avc->au_num && parser_avc->au_num*(uint64_t)(parser_avc->au_ticks) >=
        PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    parser_avc->sample_size = 0;
    dec->NewBpStart         = 0;
    keep_all                = (parser->dsi_type != DSI_TYPE_MP4FF);
#if TEST_NAL_ES_DUMP
    keep_all                = TRUE;  /* to keep all nal */
#endif

    /****** au are pushed out => always has a au start nal if not EOES */
    if (!nal->data_size)
    {
        /* push mode and 0 data mean end of file */
        return EMA_MP4_MUXED_EOES;
    }

    /****** very first nal is load but not touched */
    if (!parser_avc->au_num)
    {
        parser_avc_parse_nal_1(nal->nal_buf, nal->nal_size, dec);
    }

    /****** nal parsing and au boundary test */
    msglog(NULL, MSGLOG_DEBUG, "\nAu %u start with Nal type %u idc %u size avail %" PRIz "\n",
           parser_avc->au_num, dec->nal_unit_type, dec->nal_ref_idc, nal->nal_size);
    do
    {
        /**** phase 2 parse of the nal of current au */
        err = parser_avc_parse_nal_2(nal->nal_buf, nal->nal_size, dec);
        if (err != EMA_MP4_MUXED_OK)
        {
            return err;
        }

        keep_nal = TRUE;  /* default: to keep nal */
        sc_size  = (keep_all) ? 0 : (uint32_t)nal->sc_size;  /* only mp4ff replace start code */
        switch (dec->nal_unit_type)
        {
        case NAL_TYPE_SEI:
            /* parse SEI here since the parse result will not change if a au start or not
             *  and sei message will change(discard some)
             */
            sei_size2keep = parse_sei_messages(dec, nal, keep_all);
            keep_nal      = (sei_size2keep >= sc_size + 3);
            /* have SEI to keep. +3: nal hdr, sei type and size */
            parser_avc->sei_num++;
            break;

        case NAL_TYPE_SEQ_PARAM:
            DPRINTF(NULL, "Adding SPS %d\n", dec->sps_id);

            if (parser->dsi_type == DSI_TYPE_MP4FF)
            {
                /* Check if new sample description is necessary */
                if (ps_list_is_there_collision(&(mp4ff_dsi->sps_lst), dec->sps_id, nal) &&
                    !(sample->flags & SAMPLE_NEW_SD))   /* Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                {
                    /* New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                    err = parser_avc_clone_dsi(parser);
                    if (err != EMA_MP4_MUXED_OK)
                    {
                        return err;
                    }
                    /* Update avc handles */
                    dsi_avc   = (dsi_avc_handle_t)parser->curr_dsi;
                    mp4ff_dsi = (mp4_dsi_avc_handle_t)dsi_avc;

                    single_sps_flag = FALSE;
                }
                keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->sps_lst), dec->sps_id, nal, &sample->flags);
            }
#if TEST_NAL_ES_DUMP
            keep_nal = TRUE;
#endif
            if (mp4ff_dsi->dsi_in_mdat)
            {
                keep_nal = TRUE;
            }
            parser_avc->sps_num++;
            break;

        case NAL_TYPE_PIC_PARAM:
            DPRINTF(NULL, "Adding PPS %d\n", dec->pps_id);

            if (parser->dsi_type == DSI_TYPE_MP4FF)
            {
                /* Check if new sample description is necessary */
                if (ps_list_is_there_collision(&(mp4ff_dsi->pps_lst), dec->pps_id, nal) &&
                    !(sample->flags & SAMPLE_NEW_SD))   /* Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                {
                    /* New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                    err = parser_avc_clone_dsi(parser);
                    if (err != EMA_MP4_MUXED_OK)
                    {
                        return err;
                    }
                    /* Update avc handles */
                    dsi_avc   = (dsi_avc_handle_t)parser->curr_dsi;
                    mp4ff_dsi = (mp4_dsi_avc_handle_t)dsi_avc;
                    /** PPS could be kept in the bitstream if the SPS keep the same */
                    if (single_sps_flag) 
                    {
                        keep_nal = TRUE;
                    }
                }
                keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->pps_lst), dec->pps_id, nal, &sample->flags);
            }
#if TEST_NAL_ES_DUMP
            keep_nal = TRUE;
#endif
            if (mp4ff_dsi->dsi_in_mdat)
            {
                keep_nal = TRUE;
            }
            parser_avc->pps_num++;
            break;

        case NAL_TYPE_FILLER_DATA:
            /* For 'avc1/avc2', doesn't add filler data to sample buffer */
            keep_nal = keep_all;
            break;

        case NAL_TYPE_SEQ_PARAM_EXT:
            if (parser->dsi_type == DSI_TYPE_MP4FF)
            {
                /* Check if new sample description is necessary */
                if (ps_list_is_there_collision(&(mp4ff_dsi->sps_ext_lst), dec->sps_id, nal) &&
                    !(sample->flags & SAMPLE_NEW_SD))   /* Don't create new dsi list entry if
                                                           new sample entry was already triggered */
                {
                    /* New set could just be an update. In this case all other sets
                       would stay the same and just one set has to be updated.
                       So everything from the current dsi has to be copied into the new one.
                       The decision which sets are actually used has to be taken later in the code. */
                    err = parser_avc_clone_dsi(parser);
                    if (err != EMA_MP4_MUXED_OK)
                    {
                        return err;
                    }
                    /* Update avc handles */
                    dsi_avc   = (dsi_avc_handle_t)parser->curr_dsi;
                    mp4ff_dsi = (mp4_dsi_avc_handle_t)dsi_avc;
                }
                keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->sps_ext_lst), dec->sps_id, nal, &sample->flags);
            }
#if TEST_NAL_ES_DUMP
            keep_nal = TRUE;
#endif
            if (mp4ff_dsi->dsi_in_mdat)
            {
                keep_nal = TRUE;
            }
            parser_avc->sps_ext_num++;
            break;

        case NAL_TYPE_ACCESS_UNIT:
            found_aud = TRUE;
            keep_nal  = TRUE;
            break;

        /** DolbyVision RPU NALs */
        case NAL_TYPE_UNSPECIFIED28: 
            /** Dolby Vision RPU NALs found, but the user don't want to signal; Just mux it to comment mp4 */
            if (parser->ext_timing.ext_dv_profile == 0xff)
            {
                parser->dv_rpu_nal_flag = 0;
            }
            else
            {
                parser->dv_rpu_nal_flag = 1;
            }
            keep_nal  = TRUE;
            break;

        /** DolbyVision EL NALs */
        case NAL_TYPE_UNSPECIFIED30: 
            {
                uint8_t nal_unit_type = 0;
                parser->dv_el_nal_flag = 1;
                keep_nal  = TRUE;

                /** For single track, retrieve sps, pps at the first sample. */            
                if (!parser->dv_el_track_flag && parser_avc->au_num == 0) 
                {
                    mp4_dsi_avc_handle_t dsi = (mp4_dsi_avc_handle_t) (((parser_avc_handle_t) parser)->dsi_enh);
                    nal_t nal_temp;
                    err = parser_avc_parse_el_nal(nal->nal_buf+6, nal->nal_size-6, dec_el);
                    if (err != EMA_MP4_MUXED_OK)
                    {
                        return err;
                    }
                    
                    nal_unit_type = nal->nal_buf[6] & 0x1f;
                    nal_temp.nal_buf = nal->nal_buf + 2;
                    nal_temp.nal_size = nal->nal_size - 2;
                    nal_temp.sc_size = 4;
                    if (nal_unit_type == NAL_TYPE_SEQ_PARAM)
                    {
                        dsi->AVCProfileIndication = dec_el->active_sps->profile_idc;
                        dsi->AVCLevelIndication = dec_el->active_sps->level_idc;
                        dsi->profile_compatibility = dec_el->active_sps->compatibility;

                        dsi->bit_depth_chroma = (uint8_t)dec_el->active_sps->bit_depth_chroma_minus8 + 8;
                        dsi->bit_depth_luma = (uint8_t)dec_el->active_sps->bit_depth_luma_minus8 + 8;
                        dsi->chroma_format = (uint8_t)dec_el->active_sps->chroma_format_idc;
                        dsi->configurationVersion = 1;

                        ps_list_update(parser_avc, &(dsi->sps_lst), dec_el->sps_id, &nal_temp, NULL);
                    }
                    else if (nal_unit_type == NAL_TYPE_PIC_PARAM)
                    {
                        ps_list_update(parser_avc, &(dsi->pps_lst), dec_el->pps_id, &nal_temp, NULL);
                    }
                    else if (nal_unit_type == NAL_TYPE_SEQ_PARAM_EXT)
                    {
                        ps_list_update(parser_avc, &(dsi->sps_ext_lst), 0, &nal_temp, NULL);
                    }
                }
            break;
            }

        default:
            /* keep nal: vcl... */
            keep_nal = TRUE;
            break;
        }

        /* Abort when multiple sample descriptions would be necessary but forbidden */
        if (parser_avc->sd_collision_flag)
        {
            return EMA_MP4_MUXED_MULTI_SD_ERR;
        }

        /* to get nal_size and sc_off_next if havn't, reach next sc */
        skip_the_nal(nal);
        msglog(NULL, MSGLOG_DEBUG, "Nal size %" PRIz "\n", nal->nal_size);

        /******* book keep the nal */
        if (keep_nal)
        {
            nal_loc = au_nals->nal_locs + au_nals->nal_idx;

            nal_loc->sc_size = sc_size;

            if (CHK_FILE_OFF(nal) != 0)
            {
                return EMA_MP4_MUXED_READ_ERR;
            }
            /* !sei nal or all sei nal to keep */
            nal_loc->off  = nal->off_file + sc_size;
            nal_loc->size = nal->nal_size - sc_size;

            sei_size2keep = 0;

            if (incr_nal_idx(au_nals) != EMA_MP4_MUXED_OK)
                return EMA_MP4_MUXED_BUGGY;

            parser_avc->sample_size += dsi_avc->NALUnitLength + (uint32_t)nal_loc->size;
#if TEST_NAL_ES_DUMP
            parser_avc->sample_size -= dsi_avc->NALUnitLength;  /* no replacement */
#endif
        }
        nal_in_au++;  /* got a nal for au */

        /* Before we parse the next nal to look ahead, save current AU information. */
        if (dec->nal_unit_type == 5)
        {
            idr_nal_ref_idc          = dec->nal_ref_idc;
            sample->pic_type         = 1;
            sample->dependency_level = 0x01;
        }
        else if (dec->nal_unit_type > 0 && dec->nal_unit_type < 5)
        {
            vcl_nal_ref_idc = dec->nal_ref_idc;

            if (dec->slice->slice_type == 7)
            {
                /* Current slice and all slices in this AU are I slice type. */
                switch (dec->nal_unit_type)
                {
                case 1:
                    /* I slice */
                    sample->pic_type = 3;
                    break;

                case 5:
                    /* IDR slice */
                    sample->pic_type = 1;
                    break;

                default:
                    /* Unknown */
                    sample->pic_type = 0;
                }
                sample->dependency_level = 0x01;
            }
            else if (sample->dependency_level != 0x01)
            {
                /* All other slice types besides I slice. */
                sample->dependency_level = 0x02;
            }
        }
        /*** set frame type for level information in 'ssix' box*/
        if (dec->nal_unit_type > 0 && dec->nal_unit_type < 6)
        {
             switch (dec->slice->slice_type)
             {
             case 2:
             case 4:
             case 7:
             case 9:
                 /* I slice */
                 sample->frame_type = 0;
                 break;
        
             case 0:
             case 3:
             case 5:
             case 8:
                 /* P slice */
                 sample->frame_type = 1;
                 break;
        
             default:
                 /* B slice */
                 sample->frame_type = 2;
             }
        }

        if (sample->sample_has_redundancy == 0)
        {
            sample->sample_has_redundancy = 
                (dec->sample_has_redundancy == TRUE) ? 1 : 2;
        }    

        /**** done with current nal, load a new nal */
        if (!get_a_nal(nal))
        {
            break;
        }
        
        new_au_start = parser_avc_parse_nal_1(nal->nal_buf, nal->nal_size, dec);

        if (new_au_start && parser_avc->sample_size)
        {
            /* a next start with current nal and cuurent au has data  */
            old_au_end = TRUE;
            msglog(NULL, MSGLOG_DEBUG, "\nPrev au %u complete\n", parser_avc->au_num);
            break;
        }
        /* else, au not complete, or empty au (not possible ???) */
    } while (1);

    if (!old_au_end)
    {
        /* get_a_nal() fail: end of file */
        if (!parser_avc->sample_size)
        {
            return EMA_MP4_MUXED_EOES;
        }

        /* last sample: parser_avc->sample_size != 0 if source file has one valid nal */
        msglog(NULL, MSGLOG_DEBUG, "\nLast au %u complete\n", parser_avc->au_num);
    }

    /* come here:  conclude an au.  !old_au_end it is a last au */
    sample->flags |= (dec->IDR_pic) ? SAMPLE_SYNC : 0;
    if (!parser_avc->au_num)
    {
#ifdef FAKE_FIRST_SAMPLE_IS_SYNC
         sample->flags |= SAMPLE_SYNC;
#endif
    }
    p_active_sps = dec->active_sps;
    p_active_pps = dec->active_pps;
    /* Some general parsing error. Bail out to avoid accessing NULL pointers later on. */
    if (!p_active_sps || !p_active_pps)
    {
        return EMA_MP4_MUXED_ES_ERR;
    }
    /* SPS / PPS configuration missing prior to video payload */
    if (!p_active_sps->isDefined || !p_active_pps->isDefined)
    {
        err = EMA_MP4_MUXED_NO_CONFIG_ERR;
    }

    /** the maximum visual width and height of the stream described by this sample description*/
    if ((p_active_sps->pic_width_out > parser_avc->width) || (p_active_sps->pic_height_out > parser_avc->height))
    {
        parser_avc->width    = p_active_sps->pic_width_out;
        parser_avc->height   = p_active_sps->pic_height_out;
        parser_avc->hSpacing = p_active_sps->sar_width;
        parser_avc->vSpacing = p_active_sps->sar_height;
    }

    if (dec->IDR_pic || parser_avc->au_num == 0)
    {
        /* within a seq, active_sps remain the same */
        timing_info_update(parser_avc, p_active_sps);
        get_colr_info(parser_avc, p_active_sps);

        dsi_update(dsi_avc, p_active_sps);

        apoc_flush(parser_avc->p_apoc);
        if (p_active_sps->frame_mbs_only_flag)
        {
            apoc_set_num_reorder_au(parser_avc->p_apoc, p_active_sps->num_reorder_frames);
        }
        else
        {
            apoc_set_num_reorder_au(parser_avc->p_apoc, p_active_sps->num_reorder_frames<<1);
        }
    }

    /**** timing */
#if USE_HRD_FOR_TS
    if (!p_active_sps->UseSeiTiming)
    {
#endif
        sample->dts = parser_avc->au_num;

        apoc_add(parser_avc->p_apoc, dec->pic_order_cnt, FALSE);
        if (!old_au_end)
        {
            apoc_flush(parser_avc->p_apoc);
        }
        sample->dts *= parser_avc->au_ticks;
        sample->cts = sample->dts;            /* if cts != dts, cts have to be updated anyway */
#if USE_HRD_FOR_TS
    }
    else
    {
        if (parser_avc->au_num)
        {
            sample->dts = dec->DtsNb + dec->cpb_removal_delay*(uint64_t)parser_avc->num_units_in_tick;
        }
#if FISRT_DTS_DTS_IS_0
        else
        {
            sample->dts = 0;
        }
#else
        else if (dec->NewBpStart)
        {
            /* we have Bp, for now use last value: highest bit rate and lowest delay */
            sample->dts = (dec->initial_cpb_removal_delay_last*(uint64_t)parser_avc->time_scale)/90000;
        }
        else
        {
            sample->dts = (7*parser_avc->time_scale)/10;  /* assuming 0.7 sec */
        }
#endif
        /** verify and error correction code */
        verify_dts(parser_avc, sample);

        sample->cts = sample->dts + dec->dpb_output_delay*parser_avc->num_units_in_tick;

        /** debug code */
        verify_cts(parser_avc, sample);

        if (!old_au_end)
        {
            verify_ts_report(parser_avc);
        }
        /** end of debug code */

        if (dec->NewBpStart)
        {
            dec->DtsNb = sample->dts;
        }
    }
#endif
    sample->duration = parser_avc->au_ticks;

    /**** data */
    sample->size = parser_avc->sample_size;

    /* Save sample dependency information (see 'sdtp') */
    sample->sample_depends_on = (dec->IDR_pic) ? 2 : 1 ;
    if (dec->IDR_pic)
    {
        sample->sample_is_depended_on = (idr_nal_ref_idc == 0) ? 2 : 1;
    }
    else
    {
        sample->sample_is_depended_on = (vcl_nal_ref_idc == 0) ? 2 : 1;
    }
    msglog(NULL,
           MSGLOG_DEBUG,
           "s: %s, nal_ref_idc=%d, dep_on=%d, dep'd_on=%d, rdnt=%d, lvl=%d, pic=%d\n",
           (dec->IDR_pic ? "IDR" : "non-IDR"),
           (dec->IDR_pic ? idr_nal_ref_idc : vcl_nal_ref_idc),
           sample->sample_depends_on,
           sample->sample_is_depended_on,
           sample->sample_has_redundancy,
           sample->dependency_level,
           sample->pic_type);

    save_au_nals_info(au_nals, sample, parser_avc->tmp_bbo);

    msglog(NULL, MSGLOG_DEBUG, "Get frame %d: %" PRIz " bytes, dts %" PRIu64 ", cts %" PRIu64 ", dur %u, IDR %d\n",
           parser_avc->au_num, sample->size, sample->dts, sample->cts, sample->duration, dec->IDR_pic);
    msglog(NULL, MSGLOG_DEBUG, "  pic_order: dec %d, out %d\n",
           dec->pic_dec_order_cnt, dec->pic_order_cnt);

    {
        int64_t pos = parser->ds->position(parser->ds);
        parser->ds->seek(parser->ds, parser_avc->au_nals.nal_locs[0].off, SEEK_SET);
        parser->ds->read(parser->ds, &(sample->nal_info), 1);
        parser->ds->seek(parser->ds, pos, SEEK_SET);
    }

    /* validation of AU */
    if (IS_FOURCC_EQUAL(parser->conformance_type, "cffh") ||
        IS_FOURCC_EQUAL(parser->conformance_type, "cffs")) 
    {
        if (sample->flags & SAMPLE_NEW_SD)
        {
            parser_avc_ccff_validate(parser_avc, dec);
        }
    }

    if (!found_aud)
    {
        parser_avc->validation_flags |= VALFLAGS_NO_AUD;
    }
    if (dec->IDR_pic)
    {
        uint32_t dist = parser_avc->au_num - parser_avc->last_idr_pos;
        if (dist > parser_avc->max_idr_dist && parser_avc->au_num > parser_avc->last_idr_pos)
        {
            parser_avc->max_idr_dist = dist;
        }
        parser_avc->last_idr_pos = parser_avc->au_num;
    }

    parser_avc->au_num++;
    parser_avc->num_samples++;

    return err;
}

#ifdef WANT_GET_SAMPLE_PUSH
/* for now, pack the data into a linear buf and use corresponding pull mode version */
static void
esd_2_linear_buf(SEsData_t *asEsd, SSs_t *psNal, nal_handle_t nal)
{
    uint32_t off_esd;
    uint8_t  idx_esd;
    uint32_t data2cp, cp_size, data_seg_size;
    int32_t      idx = 0;

    /**** use nal->buffer as temp buf */
    nal->nal_buf = nal->buffer;

    nal->nal_size = psNal->u8ShSize + psNal->u32BodySize;
    if (nal->nal_size > nal->buf_size)
    {
        nal->nal_size = nal->buf_size;
    }
    nal->sc_size = psNal->u8ShSize;

    /**** copy to nal buf */
    /** build scp */
    nal->nal_buf[0] = 0x0;
    nal->nal_buf[1] = 0x0;
    nal->nal_buf[2] = 0x0;
    nal->nal_buf[nal->sc_size - 1] = 0x1;
    idx = nal->sc_size;

    /** cp data upto nal_size */
    data2cp = nal->nal_size - idx;
    idx_esd = psNal->u8BodyIdx;
    off_esd = psNal->u32BodyOff;
    do
    {
        data_seg_size = asEsd[idx_esd].u32DataInSize - off_esd;
        cp_size       = (data2cp < data_seg_size) ? data2cp : data_seg_size;
        memcpy(nal->nal_buf + idx, asEsd[idx_esd].pBufIn + off_esd, cp_size);
        data2cp -= cp_size;
        if (!data2cp)
        {
            break;
        }
        idx += cp_size;
        /* nal data continue at begining of next buf */
        idx_esd++;
        off_esd = 0;
    }
    while (1);
}

static int
build_sample(parser_avc_handle_t parser_avc, mp4_sample_handle_t sample)
{
    avc_decode_t *dec          = &(parser_avc->dec);
    sps_t        *p_active_sps = dec->active_sps;

    sample->flags = (dec->IDR_pic) ? SAMPLE_SYNC : 0;  /* set flags and clear SAMPLE_PARTIAL_AU */
    if (!parser_avc->au_num)
    {
         /* the first one should have all the new info and accessible(including MMCO5?) */
         sample->flags = SAMPLE_NEW_SD | SAMPLE_SYNC;
    }

    if (dec->IDR_pic || parser_avc->au_num == 0)
    {
        /* within a seq, active_sps remain the same */
        parser_avc->width    = p_active_sps->pic_width_out;
        parser_avc->height   = p_active_sps->pic_height_out;
        parser_avc->hSpacing = p_active_sps->sar_width;
        parser_avc->vSpacing = p_active_sps->sar_height;

        timing_info_update(parser_avc, p_active_sps);

        dsi_update((dsi_avc_handle_t)parser_avc->curr_dsi, p_active_sps);
        if (parser_avc->dec.active_sps_enh)
        {
            dsi_update((dsi_avc_handle_t)parser_avc->dsi_enh, parser_avc->dec.active_sps_enh);
        }

        apoc_flush(parser_avc->p_apoc);
        if (p_active_sps->frame_mbs_only_flag)
        {
            apoc_set_num_reorder_au(parser_avc->p_apoc, p_active_sps->num_reorder_frames);
            apoc_set_max_ref_au(parser_avc->p_apoc, p_active_sps->max_num_ref_frames);
        }
        else
        {
            apoc_set_num_reorder_au(parser_avc->p_apoc, p_active_sps->num_reorder_frames<<1);
            apoc_set_max_ref_au(parser_avc->p_apoc, p_active_sps->max_num_ref_frames<<1);
        }
    }

    /**** timing */
    /* parser_avc_compute_poc(dec); called in parsing vcl phase two for first slice of au */

#if USE_HRD_FOR_TS
    if (!p_active_sps->UseSeiTiming)
    {
#endif
        sample->dts = parser_avc->au_num;

        apoc_add(parser_avc->p_apoc, dec->pic_order_cnt, FALSE);
        if (dec->last_au)
        {
            apoc_flush(parser_avc->p_apoc);
        }
        sample->dts   *= parser_avc->au_ticks;
        sample->cts    = sample->dts;
        sample->flags |= SAMPLE_PARTIAL_TM;     /* in general, we don't know yet */
#if USE_HRD_FOR_TS
    }
    else
    {
        if (parser_avc->au_num)
        {
            sample->dts = dec->DtsNb + dec->cpb_removal_delay*(uint64_t)parser_avc->num_units_in_tick;
        }
#if FISRT_DTS_DTS_IS_0
        else
        {
            sample->dts = 0;
        }
#else
        else if (dec->NewBpStart)
        {
            /* we have Bp, for now use last value: highest bit rate and lowest delay */
            sample->dts = (dec->initial_cpb_removal_delay_last*(uint64_t)parser_avc->time_scale)/90000;
        }
        else 
        {
            sample->dts = (7*parser_avc->time_scale)/10;  /* assuming 0.7 sec */
        }
#endif
        /** verify and error correction code */
        verify_dts(parser_avc, sample);

        sample->cts = sample->dts + dec->dpb_output_delay*parser_avc->num_units_in_tick;

        /** debug code */
        verify_cts(parser_avc, sample);

        if (dec->last_au)
        {
            verify_ts_report(parser_avc);
        }
        /** end of debug code */

        if (dec->NewBpStart)
        {
            dec->DtsNb = sample->dts;
        }

        /* to hold the timing until MinCts is known */
        if (!parser_avc->bMinCtsKn)
        {
            sample->flags |= SAMPLE_PARTIAL_TM;
            /* so that co can be retrieved and timing resolved until minCts is known */
            if (parser_avc->au_num == 0 || parser_avc->i32PocMin > dec->pic_order_cnt)
            {
                parser_avc->i32PocMin = dec->pic_order_cnt;
                parser_avc->u32MinCts = (uint32_t)(sample->cts);
            }
            parser_avc->au32CoTc[parser_avc->au_num] = dec->dpb_output_delay;
            if (parser_avc->au_num == CO_BUF_SIZE - 1)
            {
                parser_avc->bMinCtsKn = TRUE;
            }
        }
    }
#endif
    sample->duration = parser_avc->au_ticks;

    /**** data */
    sample->size = parser_avc->sample_size;


    /* save_au_nals_info(&(parser_avc->au_nals), sample, parser_avc->tmp_bbo); */
    parser_avc->au_nals.nal_idx = 0;  /* push in case: no input file */

    msglog(NULL, MSGLOG_DEBUG, "\nAu %d end: %" PRIz " bytes, dts %" PRIu64 ", cts %" PRIu64 ", dur %u, IDR %d\n",
           parser_avc->au_num, sample->size, sample->dts, sample->cts, sample->duration, dec->IDR_pic);
    msglog(NULL, MSGLOG_DEBUG, "  pic_order: dec %d, out %d\n",
           dec->pic_dec_order_cnt, dec->pic_order_cnt);

    /* one more Au and mp4 sample got */
    parser_avc->au_num++;
    parser_avc->num_samples++;

    return EMA_MP4_MUXED_OK;
}

static int
start_new_sample(parser_avc_handle_t parser_avc)
{
    avc_decode_t *dec = &(parser_avc->dec);
    /**** first Nal in Au */

#if PARSE_DURATION_TEST
    if (parser_avc->au_num && parser_avc->au_num*(uint64_t)(parser_avc->au_ticks) >=
        PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    parser_avc->sample_size = 0;
    dec->NewBpStart         = 0;
    dec->keep_all           = (parser_avc->dsi_type != DSI_TYPE_MP4FF);
#if TEST_NAL_ES_DUMP
    dec->keep_all           = TRUE;  /* to keep all nal */
#endif

    dec->nal_idx_in_au = 0;

    return EMA_MP4_MUXED_OK;
}

/* parse the sequentially pushed in nal, build Au. sample->flags & SAMPLE_PARTIAL_AU mean Au
 * have not pushed out/not complete  upon input nal.
 * the code works well only if the current nal does not change the golbal params of the parser
 * ex. Au start with AUD
 */
static int
parser_avc_get_sample_push(parser_handle_t parser, SEsData_t *asEsd, SSs_t *psNal, mp4_sample_handle_t sample)
{
    parser_avc_handle_t  parser_avc = (parser_avc_handle_t)parser;
    dsi_avc_handle_t     dsi_avc    = (dsi_avc_handle_t)parser->curr_dsi;
    mp4_dsi_avc_handle_t mp4ff_dsi  = (mp4_dsi_avc_handle_t)dsi_avc;
    avc_decode_t *       dec        = &(parser_avc->dec);
    nal_handle_t         nal        = &(parser_avc->nal);      /* tmp: pack esd into nal buf upto 512 bytes */
    au_nals_t *          au_nals    = &(parser_avc->au_nals);
    nal_loc_t *          nal_loc;
    int32_t                  ret;

    uint32_t sc_size;
    BOOL     keep_nal;
    uint32_t sei_size2keep = 0;  /* no sei to keep, or not a sei */

    sample->flags = SAMPLE_PARTIAL_AU;  /* init to partial */
    /****** au are pushed out => always has a au start nal if not EOES */
    if (!psNal->u32BodySize)
    {
        /* push mode and 0 data mean end of file */
        if (parser_avc->sample_size)
        {
            /* last sample end: parser_avc->sample_size != 0 if source file has one valid nal */
            dec->last_au = 1;
            build_sample(parser_avc, sample);
            start_new_sample(parser_avc);  /* just to be consistent with normal AU */

            return EMA_MP4_MUXED_OK;
        }

        return EMA_MP4_MUXED_EOES;
    }

    esd_2_linear_buf(asEsd, psNal, nal);  /* tmp: use poll mode parsing for pull mode parsing */
    if (parser_avc_parse_nal_1(nal->nal_buf, nal->nal_size, dec))
    {
        /****** next AU start with input NAL  */
        if (parser_avc->sample_size)
        {
            /**** current AU is not the very first AU: conclude the previous AU */
            build_sample(parser_avc, sample);
        }

        start_new_sample(parser_avc);
        msglog(NULL, MSGLOG_DEBUG, "\nAu %u start with Nal type %u idc %u\n",
               parser_avc->au_num, dec->nal_unit_type, dec->nal_ref_idc);
    }

    /**** phase 2 parse of the nal of current au */
    ret = parser_avc_parse_nal_2(nal->nal_buf, nal->nal_size, dec);
    if (ret != EMA_MP4_MUXED_OK)
    {
        return ret;
    }

    keep_nal = TRUE;  /* default: to keep nal */
    sc_size  = (dec->keep_all) ? 0 : nal->sc_size;  /* only mp4ff replace start code: others: sc same as other es data */
    assert(nal->sc_size == psNal->u8ShSize);
    switch (dec->nal_unit_type)
    {
#if PROFILE_134_TO_128
    case NAL_TYPE_SUBSET_SEQ_PARAM:
        {
            /* hack: to change 134(D3D) into 128(MVC) */
            uint8_t *pu8tmp;
            if (asEsd[psNal->u8BodyIdx].u32DataInSize - psNal->u32BodyOff > 1)
            {
                pu8tmp = asEsd[psNal->u8BodyIdx].pBufIn + (psNal->u32BodyOff + 1);
            }
            else
            {
                assert(asEsd[psNal->u8BodyIdx].u32DataInSize != psNal->u32BodyOff);
                pu8tmp = asEsd[(uint8_t)(psNal->u8BodyIdx + 1)].pBufIn;
            }
            assert(*pu8tmp == 134);
            *pu8tmp = 128;

            break;
        }
#endif

    case NAL_TYPE_SEI:
        /* parse SEI here since the parse result will not change if a au start or not
         *  and sei message will change(discard some) */
        if (dec->mdNalType != PD_NAL_TYPE_NOT_SLICE_EXT)
        {
            sei_size2keep = parse_sei_messages(dec, nal, dec->keep_all);
            keep_nal      = (sei_size2keep >= sc_size + 3);
            /* have SEI to keep. +3: nal hdr, sei type and size */
        }
        else
        {
            DPRINTF(NULL, "SEI in MVC\n");
        }
        parser_avc->sei_num++;
        break;

    case NAL_TYPE_SEQ_PARAM:
        DPRINTF(NULL, "Adding SPS %d\n", dec->sps_id);

        if (parser->dsi_type == DSI_TYPE_MP4FF)
        {
            keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->sps_lst), dec->sps_id, nal, NULL);
        }
#if TEST_NAL_ES_DUMP
        keep_nal = TRUE;
#endif
        parser_avc->sps_num++;
        break;

    case NAL_TYPE_PIC_PARAM:
        DPRINTF(NULL, "Adding PPS %d\n", dec->pps_id);

        if (parser->dsi_type == DSI_TYPE_MP4FF && dec->mdNalType != PD_NAL_TYPE_NOT_SLICE_EXT)
        {
            keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->pps_lst), dec->pps_id, nal, NULL);
        }
#if TEST_NAL_ES_DUMP
        keep_nal = TRUE;
#endif
        parser_avc->pps_num++;
        break;

    case NAL_TYPE_FILLER_DATA:
        /* doesn't get added to sample buffer */
        keep_nal = dec->keep_all;
        break;

    case NAL_TYPE_SEQ_PARAM_EXT:
        if (parser->dsi_type == DSI_TYPE_MP4FF)
        {
            keep_nal = ps_list_update(parser_avc, &(mp4ff_dsi->sps_ext_lst), dec->sps_id, nal, NULL);
        }
#if TEST_NAL_ES_DUMP
        keep_nal = TRUE;
#endif
        parser_avc->sps_ext_num++;
        break;

    default:
        /* keep nal: vcl... */
        keep_nal = TRUE;
        break;
    }
    psNal->u8FlagsLidx = (psNal->u8FlagsLidx & (!LAYER_IDX_MASK)) | dec->layer_idx;
    msglog(NULL, MSGLOG_DEBUG, "Nal size %u\n", psNal->u8ShSize + psNal->u32BodySize);

    /* Abort when multiple sample descriptions would be necessary but forbidden */
    if (parser_avc->sd_collision_flag)
    {
        return EMA_MP4_MUXED_MULTI_SD_ERR;
    }

    /******* book keep the nal */
    if (keep_nal)
    {
        nal_loc =  au_nals->nal_locs + au_nals->nal_idx;

        nal_loc->sc_size = sc_size;  /* = 0 for !mp4ff */
        if (!sei_size2keep || sei_size2keep == nal->nal_size)
        {
            if (CHK_FILE_OFF(nal) != 0)
            {
                return EMA_MP4_MUXED_READ_ERR;
            }
            /* !sei nal or all sei nal to keep */
            nal_loc->off = nal->off_file + sc_size;
            /* nal_loc->size = nal->nal_size - sc_size; */
            nal_loc->size = psNal->u32BodySize + psNal->u8ShSize - sc_size;
        }
        else
        {
            /* sei changed. embed nal body */
            nal_loc->off  = -1;
            nal_loc->size = sei_size2keep - sc_size;

            nal_loc->buf_emb = (uint8_t *)MALLOC_CHK(nal_loc->size);
            memcpy(nal_loc->buf_emb, nal->nal_buf + sc_size, nal_loc->size);
        }
        sei_size2keep = 0;

        if (incr_nal_idx(au_nals) != EMA_MP4_MUXED_OK)
            return EMA_MP4_MUXED_BUGGY;

        parser_avc->sample_size += dsi_avc->NALUnitLength + nal_loc->size;
#if TEST_NAL_ES_DUMP
        parser_avc->sample_size -= dsi_avc->NALUnitLength;  /* no replacement */
#endif
    }
    dec->nal_idx_in_au++;  /* got a nal for au */

    return EMA_MP4_MUXED_OK;
}
#endif  /* WANT_GET_SAMPLE_PUSH */

static int
parser_avc_get_subsample(parser_handle_t parser, int64_t *pos, uint32_t subs_num_in, int32_t *more_subs_out, uint8_t *data, size_t *bufsize_ptr)
{
    uint32_t nal_num, size;
    int32_t  nals_left;
    uint8_t  sc_size;
    int64_t  off;
    static int32_t sample_count = 0;

    parser_avc_handle_t parser_avc   = (parser_avc_handle_t)parser;
    bbio_handle_t       src          = parser_avc->tmp_bbi, ds = parser->ds;
    const uint32_t      nal_unit_len = ((dsi_avc_handle_t)parser->curr_dsi)->NALUnitLength;
    const size_t        bufsize      = *bufsize_ptr;

    if (!src)
    {
        uint8_t* buffer;
        size_t   data_size, buf_size;
        /* give the output buffer to the input buffer */
        assert(parser_avc->tmp_bbo);
        src    = reg_bbio_get('b', 'r');
        buffer = parser_avc->tmp_bbo->get_buffer(parser_avc->tmp_bbo, &data_size, &buf_size);
        src->set_buffer(src, buffer, data_size, TRUE);
        parser_avc->tmp_bbi = src;
    }

    if (pos && *pos != -1)
    {
        src->seek(src, *pos, SEEK_SET);
    }

    if (RD_PREFIX(src) != 0)
    {
        return EMA_MP4_MUXED_READ_ERR;
    }

    if (src_rd_u32(src, &nal_num) != 0)   /* # of nal in au */
    {
        return EMA_MP4_MUXED_READ_ERR;
    }

    subs_num_in++;  /* start counting with 1 makes things easier */
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
    } 
    while (--subs_num_in);

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
            return 1;  /* buffer too small */
        }

#if !TEST_NAL_ES_DUMP
        while (n--)
        {
            *(data++) = (uint8_t)((size >> (n*8)) & 0xff);
        }
#endif

        if (off != -1)
        {
            /* not embedded: nal in ds */
            ds->seek(ds, off, SEEK_SET);
            ds->read(ds, data, size);
        }
        else
        {
            /* embedded: nal body right at current position */
            src->read(src, data, size);
        }
    }
    sample_count++;
    return EMA_MP4_MUXED_OK;
}

static int
parser_avc_copy_sample(parser_handle_t parser, bbio_handle_t snk, int64_t pos)
{
    uint32_t            nal_num, size;
    uint8_t             sc_size;
    int64_t             off;
    parser_avc_handle_t parser_avc   = (parser_avc_handle_t)parser;
    bbio_handle_t       src          = parser_avc->tmp_bbi;
    bbio_handle_t       ds           = parser->ds;
    const uint32_t      nal_unit_len = ((dsi_avc_handle_t)parser->curr_dsi)->NALUnitLength;

#if TEST_NAL_ES_DUMP
    if (parser->dsi_type == DSI_TYPE_MP2TS || parser->dsi_type == DSI_TYPE_MP4FF)
    {
        /* all NALU should consist a valid h264 stream */
        static bbio_handle_t es_snk = NULL;
        if (es_snk == NULL)
        {
            /* will be memory leak on this. but it is in test only code. we are fine */
            es_snk = reg_bbio_get('f', 'w');
            es_snk->open(es_snk, "test_es.h264");
        }
        snk = es_snk;
    }
#endif

    if (!src)
    {
        uint8_t* buffer;
        size_t data_size, buf_size;
        /* give the output buffer to the input buffer */
        assert(parser_avc->tmp_bbo);
        src = reg_bbio_get('b', 'r');
        buffer = parser_avc->tmp_bbo->get_buffer(parser_avc->tmp_bbo, &data_size, &buf_size);
        src->set_buffer(src, buffer, data_size, TRUE);
        parser_avc->tmp_bbi = src;
    }

    if (pos != -1)
    {
        src->seek(src, pos, SEEK_SET);
    }

    if (RD_PREFIX(src) != 0)
    {
        return EMA_MP4_MUXED_READ_ERR;
    }

    if (src_rd_u32(src, &nal_num) != 0) /* # of nal in au */
    {
        return EMA_MP4_MUXED_READ_ERR;
    }
    while (nal_num--)
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

#if !TEST_NAL_ES_DUMP
        switch (nal_unit_len)
        {
        case 1:
            sink_write_u8(snk, (uint8_t)size); break;
        case 2:
            sink_write_u16(snk, (uint16_t)size); break;
        case 4:
            sink_write_u32(snk, size); break;
        default:
            break;  /* must be 0: !mp4ff case */
        }
#endif
        if (off != -1)
        {
            /* not embedded: nal in ds */
            ds->seek(ds, off, SEEK_SET);
            bbio_copy(snk, ds, size);
        }
        else
        {
            /* embedded: nal body right at current position */
            bbio_copy(snk, src, size);
        }
    }

    return EMA_MP4_MUXED_OK;
}

static BOOL
parser_avc_need_fix_cts(parser_handle_t parser)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;
    avc_apoc_t *p_apoc = parser_avc->p_apoc;

    if (parser_avc->dec.active_sps == NULL)
    {
        return FALSE;
    }

    if (parser_avc->dec.active_sps->UseSeiTiming)
    {
        return FALSE;
    }

    apoc_flush(p_apoc);

    return TRUE;
}

static int32_t
parser_avc_get_cts_offset(parser_handle_t parser, uint32_t sample_idx)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;
    if (!parser_avc->dec.active_sps->UseSeiTiming)
    {
        int32_t offset = apoc_reorder_num(parser_avc->p_apoc, (int)sample_idx);

        if (offset >= 0)
        {
            return offset*parser_avc->au_ticks;
        }

        return -1;  /* to signal it is not ready */
    }
    else
    {
        /* at and after CO_BUF_SIZE, all is known, shall not come here */
        assert(sample_idx < CO_BUF_SIZE);
        if (!parser_avc->bMinCtsKn)
        {
            return -1;  /* to signal it is not ready */
        }
        return parser_avc->au32CoTc[sample_idx]*parser_avc->num_units_in_tick;
    }
}

/* get dsi for avc (AVCDecoderConfigurationRecord) */
/* implements method get_cfg() of the (AVC) parser for the dsi_type DSI_TYPE_MP4FF
 */
static int
parser_avc_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    bbio_handle_t        snk;
    mp4_dsi_avc_handle_t dsi = (mp4_dsi_avc_handle_t)parser->curr_dsi;
    buf_entry_t *        entry;
    it_list_handle_t     it  = it_create();

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 256, 1);
    }

    /* AVCDecoderConfigurationRecord - see [ISOAVC] Section 5.2.4.1 */
    sink_write_u8(snk, 1);                                         /* configurationVersion = 1 */
    sink_write_u8(snk, dsi->AVCProfileIndication);
    sink_write_u8(snk, dsi->profile_compatibility);
    sink_write_u8(snk, dsi->AVCLevelIndication);
    sink_write_bits(snk, 6, 0x3F);                                 /* reserved = '111111'b */
    sink_write_bits(snk, 2, (dsi->NALUnitLength-1) & 0x03);        /* lengthSizeMinusOne */

    sink_write_bits(snk, 3, 0x07);                                 /* reserved = '111'b; */

    if(dsi->dsi_in_mdat && (!parser->ext_timing.ps_present_flag))  /* sample entry name "avc3" */
    {
        sink_write_bits(snk, 5, 0);                                /* numOfSequenceParameterSets */
        sink_write_u8(snk, 0);                                     /* numOfPictureParameterSets */
    }
    else /** sample entry name "avc1"*/
    {
        sink_write_bits(snk, 5, list_get_entry_num(dsi->sps_lst));     /* numOfSequenceParameterSets */
        it_init(it, dsi->sps_lst);
        while ((entry = it_get_entry(it)))
        {
            sink_write_u16(snk, (uint16_t)entry->size);                /* sequenceParameterSetLength */
            snk->write(snk, entry->data, entry->size);                 /* sequenceParameterSetNALUnit */
        }

        sink_write_u8(snk, (uint8_t)list_get_entry_num(dsi->pps_lst)); /* numOfPictureParameterSets */
        it_init(it, dsi->pps_lst);
        while ((entry = it_get_entry(it)))
        {
            sink_write_u16(snk, (uint16_t)entry->size);                /* pictureParameterSetLength */
            snk->write(snk, entry->data, entry->size);                 /* pictureParameterSetNALUnit */
        }
    }

    if( dsi->AVCProfileIndication == 100 || dsi->AVCProfileIndication == 110 ||
        dsi->AVCProfileIndication == 122 || dsi->AVCProfileIndication == 144 )
    {
        sink_write_bits(snk, 6, 0x3F);                             /* reserved = '111111'b */
        sink_write_bits(snk, 2, dsi->chroma_format);
        sink_write_bits(snk, 5, 0x1F);                             /* reserved = '11111'b */
        sink_write_bits(snk, 3, dsi->bit_depth_luma - 8);          /* bit_depth_luma_minus8 */
        sink_write_bits(snk, 5, 0x1F);                             /* reserved = '11111'b */
        sink_write_bits(snk, 3, dsi->bit_depth_chroma - 8);        /* bit_depth_chroma_minus8 */

        if(dsi->dsi_in_mdat && (!parser->ext_timing.ps_present_flag)) /** sample entry name "avc3"*/
        {
            sink_write_u8(snk, 0);      /* numOfSequenceParameterSetExt */
        }
        else /** sample entry name "avc1"*/
        {
            sink_write_u8(snk, (uint8_t)list_get_entry_num(dsi->sps_ext_lst));  /* numOfSequenceParameterSetExt */
            it_init(it, dsi->sps_ext_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk,(uint16_t)entry->size);             /* sequenceParameterSetExtLength */
                snk->write(snk, entry->data, (int)entry->size);        /* sequenceParameterSetExtNALUnit */
            }
        }
    }
    it_destroy(it);

    *buf = snk->get_buffer(snk, buf_len, 0);  /* here buf_len is set to data_size */
    snk->destroy(snk);

    /** if it's dolby vision, we should add 'dvcC' to 'avcC' */
    if ((parser->ext_timing.ext_dv_profile == 1) || (parser->dv_rpu_nal_flag))
    {
        parser->dv_dsi_size = 24;
        memset(parser->dv_dsi_buf, 0, parser->dv_dsi_size);

        parser->dv_dsi_buf[0] = 1;
        if (parser->dv_el_nal_flag)
        {
            parser->dv_dsi_buf[3] = 7; /* BL+EL+RPU */
        }
        else
        {
            if ((parser->ext_timing.ext_dv_profile == 1) && (parser->dv_rpu_nal_flag == 0))
                parser->dv_dsi_buf[3] = 1; /*  BL */
            else
                parser->dv_dsi_buf[3] = 6; /*  EL+RPU */
        }

        if (parser->ext_timing.ext_dv_profile == 9)
        {
            parser->dv_dsi_buf[3] = 5; /*  BL+RPU */
        }

        /* user's setting override parser's value*/
        if (parser->ext_timing.ext_dv_profile != 0xff) 
        {
            if ((parser->ext_timing.ext_dv_profile == 0) 
                || (parser->ext_timing.ext_dv_profile == 1)
                || (parser->ext_timing.ext_dv_profile == 9))
            {
                parser->dv_dsi_buf[2] |= (parser->ext_timing.ext_dv_profile << 1);
            }
            else
            {
                msglog(NULL, MSGLOG_ERR, "Error: For Dolby vision 264 codec type, only setting profile to 9 makes sense!\n");
                return EMA_MP4_MUXED_BUGGY;
            }
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, "Error: For muxing Dolby vision stream, '--dv-profile' must be set by user!\n");
            return EMA_MP4_MUXED_BUGGY;
        }

        parser->dv_dsi_buf[2] |= (parser->dv_level & 0x80);
         parser->dv_dsi_buf[3] |= (parser->dv_level << 3);
    }
    if ((parser->ext_timing.ext_dv_profile == 0) || (parser->ext_timing.ext_dv_profile == 9))
    {
        parser->dv_dsi_buf[4] |= (2 << 4); 
    }

    /** If there's el nal, then extract the dsi info, which could be used to create avcE */
    if (parser->dv_el_nal_flag)
    {
        mp4_dsi_avc_handle_t dsi = (mp4_dsi_avc_handle_t) (((parser_avc_handle_t) parser)->dsi_enh);
        buf_entry_t *        entry;
        it_list_handle_t     it  = it_create();
        snk = reg_bbio_get('b', 'w');
        snk->set_buffer(snk, NULL, 256, 1);
        /* AVCDecoderConfigurationRecord - see [ISOAVC] Section 5.2.4.1 */
        sink_write_u8(snk, 1);                                                   /* configurationVersion = 1 */
        sink_write_u8(snk, dsi->AVCProfileIndication);
        sink_write_u8(snk, dsi->profile_compatibility);
        sink_write_u8(snk, dsi->AVCLevelIndication);
        sink_write_bits(snk, 6, 0x3F);                                           /* reserved = '111111'b */
        sink_write_bits(snk, 2, (dsi->NALUnitLength-1) & 0x03);                  /* lengthSizeMinusOne */

        sink_write_bits(snk, 3, 0x07);                                           /* reserved = '111'b; */
        sink_write_bits(snk, 5, list_get_entry_num(dsi->sps_lst));               /* numOfSequenceParameterSets */
        it_init(it, dsi->sps_lst);
        while ((entry = it_get_entry(it)))
        {
            sink_write_u16(snk, (uint16_t)entry->size);                          /* sequenceParameterSetLength */
            snk->write(snk, entry->data, entry->size);                           /* sequenceParameterSetNALUnit */
        }

        sink_write_u8(snk, (uint8_t)list_get_entry_num(dsi->pps_lst));           /* numOfPictureParameterSets */
        it_init(it, dsi->pps_lst);
        while ((entry = it_get_entry(it)))
        {
            sink_write_u16(snk, (uint16_t)entry->size);                          /* pictureParameterSetLength */
            snk->write(snk, entry->data, entry->size);                           /* pictureParameterSetNALUnit */
        }

        if( dsi->AVCProfileIndication == 100 || dsi->AVCProfileIndication == 110 ||
            dsi->AVCProfileIndication == 122 || dsi->AVCProfileIndication == 144 )
        {
            sink_write_bits(snk, 6, 0x3F);                                      /* reserved = '111111'b */
            sink_write_bits(snk, 2, dsi->chroma_format);
            sink_write_bits(snk, 5, 0x1F);                                      /* reserved = '11111'b */
            sink_write_bits(snk, 3, dsi->bit_depth_luma - 8);                   /* bit_depth_luma_minus8 */
            sink_write_bits(snk, 5, 0x1F);                                      /* reserved = '11111'b */
            sink_write_bits(snk, 3, dsi->bit_depth_chroma - 8);                 /* bit_depth_chroma_minus8 */

            sink_write_u8(snk, (uint8_t)list_get_entry_num(dsi->sps_ext_lst));  /* numOfSequenceParameterSetExt */
            it_init(it, dsi->sps_ext_lst);
            while ((entry = it_get_entry(it)))
            {
                sink_write_u16(snk,(uint16_t) entry->size);                     /* sequenceParameterSetExtLength */
                snk->write(snk, entry->data, (int)entry->size);                 /* sequenceParameterSetExtNALUnit */
            }
        }

        it_destroy(it);
        parser->dv_el_dsi_buf = snk->get_buffer(snk, (size_t *)(&parser->dv_el_dsi_size), 0);  /* here buf_len is set to data_size */
        snk->destroy(snk);
    }

    return 0;
}


static int
parser_avc_get_param_ex(parser_handle_t parser, stream_param_id_t param_id, int32_t param_idx, void *param)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;
    uint32_t t;

    switch (param_id)
    {
    case STREAM_PARAM_ID_TIME_SCALE:
        t = parser_avc->time_scale;
        break;

    case STREAM_PARAM_ID_NUM_UNITS_IN_TICK:
        t = parser_avc->num_units_in_tick;
        break;

    case STREAM_PARAM_ID_FRAME_DUR:
        t = parser_avc->num_units_in_tick << 1;
        break;

    case STREAM_PARAM_ID_MIN_CTS:
        if (!parser_avc->dec.active_sps->UseSeiTiming)
        {
            t = apoc_min_cts(parser_avc->p_apoc)*parser_avc->au_ticks;
        }
        else
        {
            t = parser_avc->u32MinCts;
        }
        break;

    case STREAM_PARAM_ID_DLT_DTS_TC:
        t = (parser_avc->dec.active_sps->frame_mbs_only_flag ||
             parser_avc->dec.slice->field_pic_flag == 0 ) ? 2 : 1;
        break;

    case STREAM_PARAM_ID_PROFILE:
        t = parser_avc->dec.active_sps->profile_idc;
        break;

    case STREAM_PARAM_ID_LEVEL:
        t = parser_avc->dec.active_sps->level_idc;
        break;

    case STREAM_PARAM_ID_PROFILE_ENH:
        assert(parser_avc->dec.active_sps_enh);
        t = parser_avc->dec.active_sps_enh->profile_idc;
        break;

    case STREAM_PARAM_ID_LEVEL_ENH:
        assert(parser_avc->dec.active_sps_enh);
        t = parser_avc->dec.active_sps_enh->level_idc;
        break;

    case STREAM_PARAM_ID_MAX_FRAME_WIDTH:
        t = parser_avc->width;
        break;

    case STREAM_PARAM_ID_MAX_FRAME_HEIGHT:
        t = parser_avc->height;
        break;

    case STREAM_PARAM_ID_CPB_CNT:
        t = parser_avc->dec.active_sps->cpb_cnt_minus1 + 1;
        break;

    case STREAM_PARAM_ID_CPB_CNT_ENH:
        assert(parser_avc->dec.active_sps_enh);
        t = parser_avc->dec.active_sps_enh->cpb_cnt_minus1 + 1;
        break;

    case STREAM_PARAM_ID_HRD_BITRATE:
        if (param_idx == 0)
        {
            t = parser_avc->dec.active_sps->bit_rate_1st;
        }
        else
        {
            t = parser_avc->dec.active_sps->bit_rate_last;
        }
        break;

    case STREAM_PARAM_ID_HRD_CPB_SIZE:
        if (param_idx == 0)
        {
            t = (parser_avc->dec.active_sps->cpb_size_1st >> 3);
        }
        else
        {
            t = (parser_avc->dec.active_sps->cpb_size_last >> 3);
        }
        break;

    case STREAM_PARAM_ID_DEC_DELAY:
        if (param_idx == 0)
        {
            t = parser_avc->dec.initial_cpb_removal_delay_1st;
        }
        else
        {
            t = parser_avc->dec.initial_cpb_removal_delay_last;
        }
        break;

    case STREAM_PARAM_ID_HRD_BITRATE_ENH:
        assert(parser_avc->dec.active_sps_enh);
        if (param_idx == 0)
        {
            t = parser_avc->dec.active_sps_enh->bit_rate_1st;
        }
        else
        {
            t = parser_avc->dec.active_sps_enh->bit_rate_last;
        }
        break;

    case STREAM_PARAM_ID_HRD_CPB_SIZE_ENH:
        assert(parser_avc->dec.active_sps_enh);
        if (param_idx == 0)
        {
            t = (parser_avc->dec.active_sps_enh->cpb_size_1st >> 3);
        }
        else
        {
            t = (parser_avc->dec.active_sps_enh->cpb_size_last >> 3);
        }
        break;

    case STREAM_PARAM_ID_ASPECT_RATIO:
        t = (parser_avc->dec.active_sps->sar_width << 16) | parser_avc->dec.active_sps->sar_height;
        break;

    case STREAM_PARAM_ID_PROGRESSIVE:
        t = parser_avc->dec.active_sps->frame_mbs_only_flag;
        break;

    default:
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    *((uint32_t *)param) = t;
    return EMA_MP4_MUXED_OK;
}

static uint32_t
parser_avc_get_param(parser_handle_t parser, stream_param_id_t param_id)
{
    int32_t  param_idx = 0;
    uint32_t t;

    if (param_id == STREAM_PARAM_ID_RX ||
        param_id == STREAM_PARAM_ID_B_SIZE ||
        param_id == STREAM_PARAM_ID_DEC_DELAY)
    {
        param_idx = parser_avc_get_param(parser, STREAM_PARAM_ID_CPB_CNT) - 1;
        if (param_id == STREAM_PARAM_ID_RX)
        {
          param_id = STREAM_PARAM_ID_HRD_BITRATE;
        }
        else if (param_id == STREAM_PARAM_ID_B_SIZE)
        {
          param_id = STREAM_PARAM_ID_HRD_CPB_SIZE;
        }
    }
    else if (param_id == STREAM_PARAM_ID_RX_ENH ||
             param_id == STREAM_PARAM_ID_B_SIZE_ENH)
    {
        param_idx = parser_avc_get_param(parser, STREAM_PARAM_ID_CPB_CNT_ENH) - 1;
        if (param_id == STREAM_PARAM_ID_RX_ENH)
        {
          param_id = STREAM_PARAM_ID_HRD_BITRATE_ENH;
        }
        else if (param_id == STREAM_PARAM_ID_B_SIZE_ENH)
        {
          param_id = STREAM_PARAM_ID_HRD_CPB_SIZE_ENH;
        }
    }
    if (parser_avc_get_param_ex(parser, param_id, param_idx, &t))
    {
        return (uint32_t)-1;
    }

    return t;
}


static void
parser_avc_show_info(parser_handle_t parser)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;
    dsi_avc_handle_t    dsi_avc    = (dsi_avc_handle_t)parser->curr_dsi;

    msglog(NULL, MSGLOG_INFO, "H264/AVC stream\n");
    if (!parser_avc->au_num)
    {
        msglog(NULL, MSGLOG_INFO, "  No AU found\n");
        return;
    }

    msglog(NULL, MSGLOG_INFO, "  profile idc %u, level idc %u\n", dsi_avc->AVCProfileIndication, dsi_avc->AVCLevelIndication);
    if (dsi_avc->dsi_type == DSI_TYPE_MP4FF)
    {
        msglog(NULL, MSGLOG_INFO, "  NALU size %u\n", dsi_avc->NALUnitLength);
    }
    if (parser_avc->dec.active_sps_enh)
    {
        dsi_avc_handle_t dsi_avc_enh = (dsi_avc_handle_t)parser_avc->dsi_enh;
        msglog(NULL, MSGLOG_INFO, "  enhanced layer:\n");
        msglog(NULL, MSGLOG_INFO, "  profile idc %u, level idc %u\n", dsi_avc_enh->AVCProfileIndication, dsi_avc_enh->AVCLevelIndication);
        (void)dsi_avc_enh;
    }

    msglog(NULL, MSGLOG_INFO, "  Picture size: %dx%d\n", parser_avc->width, parser_avc->height);
    /* external is frame based */
    if (parser_avc->num_units_in_tick != 0)
    {
        msglog(NULL, MSGLOG_INFO, "  timebase %u %u(/2)(frame rate %.2f)\n",
               parser_avc->time_scale, parser_avc->num_units_in_tick<<1,
               (parser_avc->time_scale*50)/parser_avc->num_units_in_tick/100.0);
    }
    msglog(NULL, MSGLOG_INFO, "  frames %u\n", parser_avc->au_num);

    msglog(NULL, MSGLOG_INFO, "  Num of: %6s, %6s, %6s, %7s\n", "SPS", "PPS", "SPSext", "SEI");
    msglog(NULL, MSGLOG_INFO, "          %6d, %6d, %6d, %7d\n",
           parser_avc->sps_num,     parser_avc->pps_num,
           parser_avc->sps_ext_num, parser_avc->sei_num);

    if (dsi_avc->dsi_type != DSI_TYPE_MP4FF)
    {
        msglog(NULL, MSGLOG_INFO, "  Last HRD Param: Rate(bps), cpb size(bits)\n");
        msglog(NULL, MSGLOG_INFO, "  Base %8d, %8d\n",
               parser_avc->dec.active_sps->bit_rate_last, parser_avc->dec.active_sps->cpb_size_last);

        if (parser_avc->dec.active_sps_enh)
        {
            msglog(NULL, MSGLOG_INFO, "  Enh  %8d, %8d\n",
                   parser_avc->dec.active_sps_enh->bit_rate_last, parser_avc->dec.active_sps_enh->cpb_size_last);
        }

        msglog(NULL, MSGLOG_INFO, "  Initial cpb removal delay %d(in 90KHz clk)\n",
               parser_avc->dec.initial_cpb_removal_delay_last);
    }
}

static const uint8_t avc_nal_buf[4] = { 0, 0, 0, 1 };

/* Converts avc mp4 SPS, PPS into avc format: NALLength => start code */
/* implements method write_cfg() of the (AVC) parser for the dsi_type DSI_TYPE_MP4FF
 */
static uint8_t *
parser_avc_write_mp4_cfg(parser_handle_t parser, bbio_handle_t sink)
{
    mp4_dsi_avc_handle_t dsi = (mp4_dsi_avc_handle_t)parser->curr_dsi;
    buf_entry_t *        entry;
    it_list_handle_t     it = it_create();

    /* multi stsd */
    mp4_dsi_avc_handle_t *p_dsi;
    uint32_t              i;
    it_init(it, parser->dsi_lst);
    for (i = 0; i < parser->dsi_curr_index; i++)
    {
        /* Protection against corrupted bitstreams */
        if (i >= list_get_entry_num(parser->dsi_lst))
        {
            break;
        }
        p_dsi = (mp4_dsi_avc_handle_t *)it_get_entry(it);
        if (!p_dsi)
            continue;
        dsi   = *p_dsi;
    }
    if (!dsi)
    {
        it_destroy(it);
        return NULL;
    }

    it_init(it, dsi->sps_lst);
    while ((entry = it_get_entry(it)))
    {
        sink->write(sink, (uint8_t*)avc_nal_buf, 4);
        sink->write(sink, entry->data, entry->size);
    }

    it_init(it, dsi->pps_lst);
    while ((entry = it_get_entry(it)))
    {
        sink->write(sink, (uint8_t*)avc_nal_buf, 4);
        sink->write(sink, entry->data, entry->size);
    }

    it_init(it, dsi->sps_ext_lst);
    while ((entry = it_get_entry(it)))
    {
        sink->write(sink, (uint8_t*)avc_nal_buf, 4);
        sink->write(sink, entry->data, entry->size);
    }

    it_destroy(it);
    return NULL;
}

/* Converts avc mp4 into avc format: NALLength => start code */
static int
parser_avc_write_au(parser_handle_t parser, uint8_t *data, size_t size, bbio_handle_t sink)
{
    uint8_t nal_unit_type;
    BOOL    first_nal = TRUE;

    mp4_dsi_avc_handle_t dsi    = (mp4_dsi_avc_handle_t)parser->curr_dsi;
    size_t               remain = size;
    uint8_t *            ptr    = data;

    while (remain)
    {
        uint32_t j, nal_size = 0;
        for (j = dsi->NALUnitLength; j; j--)
        {
            nal_size <<= 8;
            nal_size |= *ptr;

            remain--;
            ptr++;
        }

        if (remain < nal_size)
        {
            msglog(NULL, MSGLOG_ERR, 
                   "Advertised NAL size is %u, but only %" PRIz " bytes remaining, illegal data\n",
                   nal_size, remain);

            return EMA_MP4_MUXED_ES_ERR;
        }

        if (nal_size < 1)
        {
            msglog(NULL, MSGLOG_ERR, "get nal size < 1, skip the au\n");
            return EMA_MP4_MUXED_OK;
        }

        nal_unit_type = (*ptr) & 0x1f;
        if (!first_nal && (nal_unit_type < NAL_TYPE_SEQ_PARAM || nal_unit_type > NAL_TYPE_PIC_PARAM))
        {
            sink->write(sink, avc_nal_buf+1, 3);
        }
        else
        {
            /* get zero_byte */
            sink->write(sink, (uint8_t*)avc_nal_buf, 4);
            first_nal = FALSE;

            if ((nal_unit_type == NAL_TYPE_SEQ_PARAM) || (nal_unit_type == NAL_TYPE_PIC_PARAM))
            {
                if (((mp4_dsi_avc_handle_t)parser->curr_dsi)->dsi_in_mdat == 0)
                {
                    msglog(NULL, MSGLOG_WARNING, "Found SPS or PPS in mdat, stopping inserting SPS/PPS\n");
                }
                ((mp4_dsi_avc_handle_t)parser->curr_dsi)->dsi_in_mdat = 1;
            }
        }
        sink->write(sink, ptr, nal_size);
        ptr    += nal_size;
        remain -= nal_size;
    }

    return EMA_MP4_MUXED_OK;
}

static void
avc_close(parser_handle_t parser)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;

    FREE_CHK(parser_avc->nal.buffer);
    FREE_CHK(parser_avc->nal.tmp_buf);
    if (parser_avc->nal.tmp_buf_bbi)
    {
        parser_avc->nal.tmp_buf_bbi->destroy(parser_avc->nal.tmp_buf_bbi);
    }

    if (parser_avc->dsi_enh)
    {
        parser_avc->dsi_enh->destroy(parser_avc->dsi_enh);
    }

    /* release nal related stuff */
    if (parser_avc->tmp_bbo)
    {
        parser_avc->tmp_bbo->destroy(parser_avc->tmp_bbo);
    }
    if (parser_avc->tmp_bbi)
    {
        parser_avc->tmp_bbi->destroy(parser_avc->tmp_bbi);
    }
    if (parser_avc->au_nals.nal_idx)
    {
        au_nals_t *au_nals = &(parser_avc->au_nals);

        while (au_nals->nal_idx--)
        {
            nal_loc_t *nal_loc = au_nals->nal_locs + au_nals->nal_idx;
            if (nal_loc->buf_emb)
            {
                FREE_CHK(nal_loc->buf_emb);
                nal_loc->buf_emb = 0;
            }
        }
    }

    if (parser_avc->p_apoc)
    {
        apoc_destroy(parser_avc->p_apoc);
    }
#if TEST_CTS
    if (parser_avc->p_cts_apoc)
    {
        apoc_destroy(parser_avc->p_cts_apoc);
    }
#endif
}

static void
parser_avc_destroy(parser_handle_t parser)
{
    avc_close(parser);
    parser_destroy(parser);
}

static int
parser_avc_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx, bbio_handle_t ds)
{
    parser_avc_handle_t parser_avc = (parser_avc_handle_t)parser;
    nal_handle_t        nal        = &parser_avc->nal;
    avc_decode_t *      dec        = &parser_avc->dec;
    avc_decode_t *      dec_el     = &parser_avc->dec_el;

    parser->ext_timing = *ext_timing;
    parser->es_idx     = es_idx;
    parser->ds         = ds;

    /* nal parser buffer */
    nal->ds       = ds;
    nal->buf_size = 4096;
    nal->buffer   = (uint8_t *)MALLOC_CHK(nal->buf_size);
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
            /* no data at all or to less and cause get_a_nal() malfunction */
            return EMA_MP4_MUXED_EOES;
        }
    }

    /* create a memory buffer as file io can cause issues with system rights */
    parser_avc->tmp_bbo = reg_bbio_get('b', 'w');
    parser_avc->tmp_bbo->set_buffer(parser_avc->tmp_bbo, NULL, 0, TRUE);

    parser_avc_dec_init(dec);
    parser_avc_dec_init(dec_el);

    /* validation */
    parser_avc->last_idr_pos    = (uint32_t)(-1);
    parser_avc->post_validation = &parser_avc_post_validation;

    return EMA_MP4_MUXED_OK;
}

/**
 * @brief Parses curr_codec_config into curr_dsi
 *
 * curr_codec_config is expected to be set when this function / method is called
 * typically, curr_codec_config is set to one entry in codec_config_list
 */
static int
parser_avc_codec_config(parser_handle_t parser, bbio_handle_t info_sink)
{
    parser_avc_handle_t   parser_avc = (parser_avc_handle_t)parser;
    mp4_dsi_avc_handle_t  dsi        = (mp4_dsi_avc_handle_t)(parser_get_curr_dsi(parser));
    bbio_handle_t         pb;
    offset_t              curr_pos;
    int64_t               left;
    uint32_t              i;
    uint32_t              num;
    buf_entry_t *         nalu;
    avc_decode_t          avc_decode;
    avc_decode_t          avc_decode_el;

    if (!parser->curr_codec_config || !parser->curr_codec_config->codec_config_size)
    {
        msglog(NULL, MSGLOG_WARNING, "parser_avc_codec_config: invalid curr_codec_config or empty codec_config\n");
        return EMA_MP4_MUXED_OK;
    }

    pb = reg_bbio_get('b', 'r');
    pb->set_buffer(pb, (uint8_t *)parser->curr_codec_config->codec_config_data, parser->curr_codec_config->codec_config_size, 0);

    memset(&avc_decode, 0, sizeof(avc_decode_t));
    parser_avc_dec_init(&avc_decode);
    parser_avc_dec_init(&avc_decode_el);

    dsi->configurationVersion  = src_read_u8(pb);
    dsi->AVCProfileIndication  = src_read_u8(pb);
    dsi->profile_compatibility = src_read_u8(pb);
    dsi->AVCLevelIndication    = src_read_u8(pb);
    src_read_bits(pb, 6);
    dsi->NALUnitLength = 1 + (uint8_t)src_read_bits(pb, 2);
    msglog(NULL, MSGLOG_DEBUG, "nal unit length %u bytes\n", dsi->NALUnitLength);

    src_skip_bits(pb, 3);
    num = src_read_bits(pb, 5);
    msglog(NULL, MSGLOG_DEBUG, "numOfSequenceParameterSets %u\n", num);

    dump_info(info_sink, "<configurationVersion>%u</configurationVersion>\n",             dsi->configurationVersion);
    dump_info(info_sink, "<AVCProfileIndication>%u</AVCProfileIndication>\n",             dsi->AVCProfileIndication);
    dump_info(info_sink, "<profile_compatibility>%u</profile_compatibility>\n",           dsi->profile_compatibility);
    dump_info(info_sink, "<AVCLevelIndication>%u</AVCLevelIndication>\n",                 dsi->AVCLevelIndication);
    dump_info(info_sink, "<lengthSizeMinusOne>%u</lengthSizeMinusOne>\n",                 dsi->NALUnitLength - 1);
    dump_info(info_sink, "<numOfSequenceParameterSets>%u</numOfSequenceParameterSets>\n", num);

    if (!dsi->sps_lst)
    {
        dsi->sps_lst = list_create(sizeof(buf_entry_t));
    }
    for (i = 0; i < num; i++)
    {
        uint8_t* pNALStr;
        nalu       = list_alloc_entry(dsi->sps_lst);
        nalu->size = src_read_u16(pb);
        dump_info(info_sink, "<sequenceParameterSetLength>%" PRIz "</sequenceParameterSetLength>\n", nalu->size);
        nalu->data = MALLOC_CHK(nalu->size * sizeof (uint8_t));
        curr_pos   = pb->position(pb);
        pb->read(pb, nalu->data, nalu->size);
        pNALStr    = MALLOC_CHK(nalu->size * sizeof (uint8_t) * 2 + 1);
        if (pNALStr)
        {
            Bin2Hex(nalu->data, (int)nalu->size, pNALStr);
            dump_binhex(info_sink, "sequenceParameterSetNALUnit", (char *)pNALStr);
            FREE_CHK(pNALStr);
        }
        list_add_entry(dsi->sps_lst, nalu);
        if (i == 0)
        {
            uint8_t       rbsp_bytes[128];  
            size_t        rbsp_size;
            bbio_handle_t dsb = 0;
            int32_t           ret;

            /* Use SAR from the first SPS read since 'pasp' atoms don't exist for h.264 streams
             * must remove the 0x03 nal protection byte if present
             */
            parser_avc_remove_0x03(rbsp_bytes, &rbsp_size, nalu->data+1, MIN2((nalu->size-1), 128));
            dsb = reg_bbio_get('b', 'r');
            dsb->set_buffer(dsb, rbsp_bytes, rbsp_size, 0);
            ret = parse_sequence_parameter_set(&avc_decode, dsb);
            if (ret != EMA_MP4_MUXED_OK)
            {
                return ret;
            }
            dsb->destroy(dsb);
            parser_avc->vSpacing = avc_decode.active_sps->sar_height;
            parser_avc->hSpacing = avc_decode.active_sps->sar_width;
            pb->seek(pb, curr_pos + nalu->size, SEEK_SET);  /* Parse function goes beyond SPS! Reset. */
        }
    }

    num = src_read_u8(pb);
    msglog(NULL, MSGLOG_DEBUG, "numOfPictureParameterSets %u\n", num);
    dump_info(info_sink, "<numOfPictureParameterSets>%u</numOfPictureParameterSets>\n", num);

    if (!dsi->pps_lst)
    {
        dsi->pps_lst = list_create(sizeof(buf_entry_t));
    }
    for (i = 0; i < num; i++)
    {
        uint8_t* pNALStr;
        nalu       = list_alloc_entry(dsi->pps_lst);
        nalu->size = src_read_u16(pb);
        dump_info(info_sink, "<pictureParameterSetLength>%" PRIz "</pictureParameterSetLength>\n", nalu->size);
        nalu->data = MALLOC_CHK(nalu->size * sizeof (uint8_t));
        pb->read(pb, nalu->data, nalu->size);
        pNALStr = MALLOC_CHK(nalu->size * sizeof (uint8_t) * 2 + 1);
        if (pNALStr)
        {
            Bin2Hex(nalu->data, (int)nalu->size, pNALStr);
            dump_binhex(info_sink, "pictureParameterSetNALUnit", (char *)pNALStr);
            FREE_CHK(pNALStr);
        }
        list_add_entry(dsi->pps_lst, nalu);
    }

    left = parser->curr_codec_config->codec_config_size - pb->position(pb);
    if (left>=4 && (dsi->AVCProfileIndication == 100 || dsi->AVCProfileIndication == 110 ||
                    dsi->AVCProfileIndication == 122 || dsi->AVCProfileIndication == 144))
    {
        msglog(NULL, MSGLOG_DEBUG, "Have -15 Amendment\n");
        src_read_bits(pb, 6);
        dsi->chroma_format = (uint8_t)src_read_bits(pb, 2);
        src_read_bits(pb, 5);
        dsi->bit_depth_luma = 8 + (uint8_t)src_read_bits(pb, 3);
        src_read_bits(pb, 5);
        dsi->bit_depth_chroma = 8 + (uint8_t)src_read_bits(pb, 3);

        num = src_read_u8(pb);
        msglog(NULL, MSGLOG_DEBUG, "numOfSequenceParameterSetExt %u\n", num);

        dump_info(info_sink, "<chroma_format>%u</chroma_format>\n",                               dsi->chroma_format);
        dump_info(info_sink, "<bit_depth_luma_minus8>%u</bit_depth_luma_minus8>\n",               dsi->bit_depth_luma);
        dump_info(info_sink, "<bit_depth_chroma_minus8>%u</bit_depth_chroma_minus8>\n",           dsi->bit_depth_chroma);
        dump_info(info_sink, "<numOfSequenceParameterSetExt>%u</numOfSequenceParameterSetExt>\n", num);

        if (!dsi->sps_ext_lst)
        {
            dsi->sps_ext_lst = list_create(sizeof(buf_entry_t));
        }
        for (i = 0; i < num; i++)
        {
            uint8_t* pNALStr;
            nalu       = list_alloc_entry(dsi->sps_ext_lst);
            nalu->size = src_read_u16(pb);
            dump_info(info_sink, "<sequenceParameterSetExtLength>%" PRIz "</sequenceParameterSetExtLength>\n", nalu->size);
            nalu->data = MALLOC_CHK(nalu->size * sizeof (uint8_t));
            pb->read(pb, nalu->data, nalu->size);
            pNALStr = MALLOC_CHK(nalu->size * sizeof (uint8_t) * 2 + 1);
            if (pNALStr)
            {
                Bin2Hex(nalu->data, (int)nalu->size, pNALStr);
                dump_binhex(info_sink, "sequenceParameterSetExtNALUnit", (char *)pNALStr);
                FREE_CHK(pNALStr);
            }
            list_add_entry(dsi->sps_ext_lst, nalu);
        }
        left = parser->curr_codec_config->codec_config_size - pb->position(pb);
    }
    /* debug here since save_dsi_raw_data will consume the right amount of data */
    if (left > 0)
    {
        msglog(NULL, MSGLOG_DEBUG, "Payload of %" PRIi64 " bytes not parsed\n", left);
    }
    else if (left < 0)
    {
        msglog(NULL, MSGLOG_DEBUG, "WARNING: box of wrong size. at least short of %" PRIi64 " bytes\n", -left);
    }

    pb->destroy(pb);

    return EMA_MP4_MUXED_OK;
}

/* Creates and builds interface, base */
static parser_handle_t
parser_avc_create(uint32_t dsi_type)
{
    parser_avc_handle_t parser;

    parser = (parser_avc_handle_t)MALLOC_CHK(sizeof(parser_avc_t));
    if (!parser)
    {
        return 0;
    }
    memset(parser, 0, sizeof(parser_avc_t));

    /**** build the interface, base for the instance */
    parser->stream_type     = STREAM_TYPE_VIDEO;
    parser->stream_id       = STREAM_ID_H264;
    parser->stream_name     = "h264";
    parser->dsi_FourCC      = "avcC";
    parser->profile_levelID = H264AVC_PROFILE;  /* for IODS */

    parser->dsi_type        = dsi_type;
    parser->dsi_create      = dsi_avc_create;

    parser->init            = parser_avc_init;
    parser->destroy         = parser_avc_destroy;
    parser->get_sample      = parser_avc_get_sample;
#ifdef WANT_GET_SAMPLE_PUSH
    parser->get_sample_push = parser_avc_get_sample_push;
#endif
    parser->get_subsample   = parser_avc_get_subsample;
    parser->copy_sample     = parser_avc_copy_sample;
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->get_cfg = parser_avc_get_mp4_cfg;
    }
    
    OSAL_STRNCPY(parser->codec_name, 12, "\012AVC Coding", 12);

    parser->get_param    = parser_avc_get_param;
    parser->get_param_ex = parser_avc_get_param_ex;

    parser->show_info          = parser_avc_show_info;
    parser->parse_codec_config = parser_avc_codec_config;

    /**** avc only */
    parser->need_fix_cts   = parser_avc_need_fix_cts;
    parser->get_cts_offset = parser_avc_get_cts_offset;
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser->write_cfg = parser_avc_write_mp4_cfg;
        parser->write_au  = parser_avc_write_au;
    }

    /* use dsi list for the sake of multiple entries of stsd */
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

    parser->p_apoc = apoc_create();
    if (!parser->p_apoc)
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }

    parser->dec.keep_all   = (parser->dsi_type != DSI_TYPE_MP4FF);
    parser->keep_all_nalus = 0;

#if TEST_CTS
    parser->p_cts_apoc = apoc_create();
    if (!parser->p_cts_apoc)
    {
        parser->destroy((parser_handle_t)parser);
        return 0;
    }
#endif

    /***** cast to base */
    return (parser_handle_t)parser;
}

void
parser_avc_reg(void)
{
    /* register all alias to make reg_parser_get easier */
    reg_parser_set("avc", parser_avc_create);
    reg_parser_set("h264", parser_avc_create);
    reg_parser_set("264", parser_avc_create);
}

/* CFF Stream Validation */

#define _REPORT(lvl,msg) parser_avc->reporter->report(parser_avc->reporter, lvl, msg)

static void 
parser_avc_ccff_validate(parser_avc_handle_t parser_avc, avc_decode_t *hdec)   
{
    char     str[256];
    uint32_t frameRate1000 = (parser_avc->num_units_in_tick==0) ? 0 : ((parser_avc->time_scale * 1000) / parser_avc->num_units_in_tick);

    if (!parser_avc->reporter)
    {
        return;
    }

    if (hdec->active_sps)
    {
        sps_t *p_sps = hdec->active_sps;

        /* output frame rate and resolution */
        sprintf(str, "AVC: Video resolution %dx%d.", parser_avc->width, parser_avc->height);
        _REPORT(REPORT_LEVEL_INFO, str);
        sprintf(str, "AVC: Video frame rate %f.", (float)frameRate1000/1000.0f);
        _REPORT(REPORT_LEVEL_INFO, str);

        if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffs")) 
        {
            _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing video profile_idc.");
            if (p_sps->profile_idc != 66)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC SD: profile_idc not 66. Expecting Constrained Baseline Profile.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing video level_idc.");
            if (p_sps->level_idc > 30)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC SD: level_idc larger than 30. Expecting Level 3.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing video resolution.");
            if ((parser_avc->width == 640 && parser_avc->height <= 480) ||
                (parser_avc->width <= 640 && parser_avc->height == 480))
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing video frame rate for 640x480 picture format.");
                if (frameRate1000 != 23976 && 
                    frameRate1000 != 25000 &&
                    frameRate1000 != 29970)
                {
                    sprintf(str, "AVC SD: %f is invalid video frame rate for 640x480 picture format.", (float)frameRate1000/1000.0f);
                    _REPORT(REPORT_LEVEL_WARN, str);
                }
            }
            else if ((parser_avc->width == 854 && parser_avc->height <= 480) ||
                     (parser_avc->width <= 854 && parser_avc->height == 480))
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing video frame rate for 854x480 picture format.");
                if (frameRate1000 != 23976 && 
                    frameRate1000 != 25000)
                {
                    sprintf(str, "AVC SD: %f is invalid video frame rate for 854x480 picture format.", (float)frameRate1000/1000.0f);
                    _REPORT(REPORT_LEVEL_WARN, str);
                }
            }
            else
            {
                sprintf(str, "AVC SD: %dx%d is invalid video resolution.", parser_avc->width, parser_avc->height);
                _REPORT(REPORT_LEVEL_WARN, str);
            }
        }

        if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffh"))
        {
            _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing video profile_idc.");
            if (p_sps->profile_idc != 100)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC HD: profile_idc not 100. Expecting High Profile.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing video level_idc.");
            if (p_sps->level_idc > 40)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC HD: level_idc larger than 40. Expecting Level 4.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing video resolution.");
            if ((parser_avc->width == 1280 && parser_avc->height <= 720) ||
                (parser_avc->width <= 1280 && parser_avc->height == 720))
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing video frame rate for 1280x720 picture format.");
                if (frameRate1000 != 23976 && 
                    frameRate1000 != 25000 &&
                    frameRate1000 != 29970 &&
                    frameRate1000 != 50000 &&
                    frameRate1000 != 59940)
                {
                        sprintf(str, "AVC HD: %f is invalid video frame rate for 1280x720 picture format.", (float)frameRate1000/1000.0f);
                        _REPORT(REPORT_LEVEL_WARN, str);
                }
            }
            else if ((parser_avc->width == 1920 && parser_avc->height <= 1080) ||
                     (parser_avc->width <= 1920 && parser_avc->height == 1080))
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing video frame rate for 1920x1080 picture format.");
                if (frameRate1000 != 23976 && 
                    frameRate1000 != 25000 &&
                    frameRate1000 != 29970)
                {
                        sprintf(str, "AVC HD: %f is invalid video frame rate for 1920x1080 picture format.", (float)frameRate1000/1000.0f);
                        _REPORT(REPORT_LEVEL_WARN, str);
                }
            }
            else
            {
                sprintf(str, "AVC HD: %dx%d is invalid video resolution.", parser_avc->width, parser_avc->height);
                _REPORT(REPORT_LEVEL_WARN, str);
            }
        }

        _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for frame_mbs_only_flag == 1.");
        if (p_sps->frame_mbs_only_flag != 1)
        {
            _REPORT(REPORT_LEVEL_WARN, "AVC: frame_mbs_only_flag != 1.");
        }

        _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for gaps_in_frame_num_value_allowed_flag == 0.");
        if (p_sps->gaps_in_frame_num_value_allowed_flag != 0)
        {
            _REPORT(REPORT_LEVEL_WARN, "AVC: gaps_in_frame_num_value_allowed_flag != 0.");
        }

        _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for vui_parameter_present_flag == 1.");
        if (!p_sps->vui_parameter_present_flag)
        {
            /* test VUI parameter set*/

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for aspect_ratio_info_present_flag == 1.");
            if (p_sps->aspect_ratio_idc)
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for aspect_ratio_idc.");
                if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffh"))
                {
                    uint32_t i = 0;
                    int32_t conformant = 0;
                    uint8_t valid_values[] = {1u, 14u, 15u, 16u};
                    for (i = 0; i < sizeof(valid_values) / sizeof(valid_values[0]); ++i)
                    {
                        if (p_sps->aspect_ratio_idc == valid_values[i])
                        {
                            conformant = 1;
                            break;
                        }
                    }
                    if (!conformant)
                    {
                        sprintf(str, "AVC HD: aspect_ratio_idc (0x%02x) not supported.", p_sps->aspect_ratio_idc);
                        _REPORT(REPORT_LEVEL_WARN, str);
                    }
                }
                if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffs"))
                {
                    uint32_t i = 0;
                    int32_t conformant = 0;
                    uint8_t valid_values[] = {1u, 2u, 3u, 4u, 5u, 14u, 15u, 255u};
                    for (i = 0; i < sizeof(valid_values) / sizeof(valid_values[0]); ++i)
                    {
                        if (p_sps->aspect_ratio_idc == valid_values[i])
                        {
                            conformant = 1;
                            break;
                        }
                    }
                    if (!conformant)
                    {
                        sprintf(str, "AVC SD: aspect_ratio_idc (0x%02x) not supported.", p_sps->aspect_ratio_idc);
                        _REPORT(REPORT_LEVEL_WARN, str);
                    }
                }
            }
            else
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: aspect_ratio_info_present_flag not set.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for chroma_loc_info_present_flag == 0.");
            if (p_sps->chroma_loc_info_present_flag)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: chroma_loc_info_present_flag != 0.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for overscan_appropriate == 0 (if present).");
            if (p_sps->overscan_info == 0x11)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: overscan_appropriate != 0.");
            }

            if (p_sps->video_signal_info_present_flag)
            {
                /* video signal info */

                _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for video_full_range_flag == 0.");
                if (p_sps->video_full_range_flag != 0)
                {
                    _REPORT(REPORT_LEVEL_WARN, "AVC: video_full_range_flag != 0.");
                }

                if (p_sps->colour_description_present_flag)
                {
                    /* color description */

                    _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for transfer_characteristics == 1.");
                    if (p_sps->transfer_characteristics != 1)
                    {
                        _REPORT(REPORT_LEVEL_WARN, "AVC: transfer_characteristics != 1.");
                    }

                    if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffh"))
                    {
                        _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing for colour_primaries == 1.");
                        if (p_sps->colour_primaries != 1)
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC HD: colour_primaries != 1.");
                        }

                        _REPORT(REPORT_LEVEL_INFO, "AVC HD: Testing for matrix_coefficients == 1.");
                        if (p_sps->matrix_coefficients != 1)
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC HD: matrix_coefficients != 1.");
                        }
                    }
                    if (IS_FOURCC_EQUAL(parser_avc->conformance_type, "cffs"))
                    {
                        /* colour_primaries */
                        _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing for colour_primaries == [1,5,6] depending on aspect_ratio_idc.");
                        if (p_sps->colour_primaries == 5 && (p_sps->aspect_ratio_idc != 2 && p_sps->aspect_ratio_idc != 4))
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: colour_primaries == 5 but aspect_ratio_idc not 2 or 4.");
                        }
                        else if (p_sps->colour_primaries == 6 && (p_sps->aspect_ratio_idc != 3 && p_sps->aspect_ratio_idc != 5))
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: colour_primaries == 6 but aspect_ratio_idc not 3 or 5.");
                        }
                        else if (p_sps->colour_primaries != 1)
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: colour_primaries != 1.");
                        }

                        /* matrix_coefficients */
                        _REPORT(REPORT_LEVEL_INFO, "AVC SD: Testing for matrix_coefficients == [1,5,6] depending on aspect_ratio_idc.");
                        if (p_sps->matrix_coefficients == 5 && (p_sps->aspect_ratio_idc != 2 && p_sps->aspect_ratio_idc != 4))
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: matrix_coefficients == 5 but aspect_ratio_idc not 2 or 4.");
                        }
                        else if (p_sps->matrix_coefficients == 6 && (p_sps->aspect_ratio_idc != 3 && p_sps->aspect_ratio_idc != 5))
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: matrix_coefficients == 6 but aspect_ratio_idc not 3 or 5.");
                        }
                        else if (p_sps->matrix_coefficients != 1)
                        {
                            _REPORT(REPORT_LEVEL_WARN, "AVC SD: matrix_coefficients != 1.");
                        }
                    }
                }
                else
                {
                    _REPORT(REPORT_LEVEL_INFO, "AVC: transfer_characteristics, colour_primaries, and matrix_coefficients not present.");
                }
            }
            else
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC: video_full_range_flag, transfer_characteristics, colour_primaries, and matrix_coefficients not present.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for timing_info_present_flag == 1.");
            if (p_sps->timing_info_present_flag)
            {
                _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for fixed_frame_rate_flag == 1.");
                if (!p_sps->fixed_frame_rate_flag)
                {
                    _REPORT(REPORT_LEVEL_WARN, "AVC: fixed_frame_rate_flag != 1.");
                }
            }
            else
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: timing_info_present_flag != 1.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for chroma_loc_info_present_flag == 0.");
            if (p_sps->chroma_loc_info_present_flag)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: chroma_loc_info_present_flag != 0.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for pic_struct_present_flag == 1.");
            if (!p_sps->pic_struct_present_flag)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: pic_struct_present_flag != 1.");
            }

            _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for low_delay_hrd_flag == 0.");
            if (p_sps->low_delay_hrd_flag)
            {
                _REPORT(REPORT_LEVEL_WARN, "AVC: low_delay_hrd_flag != 0.");
            }
        }
        else
        {
            _REPORT(REPORT_LEVEL_WARN, "AVC: vui_parameter_present_flag != 1.");
        }
    }
    else
    {
        _REPORT(REPORT_LEVEL_WARN, "AVC: Validation failed. SPS not accessible.");
    }
}


static int32_t 
parser_avc_ccff_post_validation(parser_avc_handle_t parser_avc)   
{
    char str[256];
    uint32_t max_dist_frames = (parser_avc->num_units_in_tick==0) ? 0 : ((parser_avc->time_scale * 3004) / parser_avc->num_units_in_tick);

    if (parser_avc->sd_collision_flag)
    {
        _REPORT(REPORT_LEVEL_WARN, "AVC: Multiple SPS or PPS unsupported. Keeping them in stream.");
    }

    _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for AUDs in all access units.");
    if (parser_avc->validation_flags & VALFLAGS_NO_AUD)
    {
        _REPORT(REPORT_LEVEL_WARN, "AVC: AUD missing.");
    }

    _REPORT(REPORT_LEVEL_INFO, "AVC: Testing for max IDR distance.");
    if (parser_avc->last_idr_pos == (uint32_t)(-1))
    {
        _REPORT(REPORT_LEVEL_WARN, "AVC: No IDR found");
    }
    else
    {
        uint32_t dist = parser_avc->au_num - parser_avc->last_idr_pos;
        if (dist > parser_avc->max_idr_dist && parser_avc->au_num > parser_avc->last_idr_pos)
        {
            parser_avc->max_idr_dist = dist;
        }

        sprintf(str, "AVC: Found max IDR distance of %d frames.", parser_avc->max_idr_dist);
        _REPORT(REPORT_LEVEL_INFO, str);
        if (max_dist_frames && parser_avc->max_idr_dist*1000 > max_dist_frames)
        {
            _REPORT(REPORT_LEVEL_WARN, "AVC: Max IDR distance larger than 3s.");
        }
    }

    return 0;
}
