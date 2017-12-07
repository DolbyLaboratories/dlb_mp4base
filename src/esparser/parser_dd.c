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
    @file parser_dd.c
    @brief Implements a Dolby digital AC-3 and E-AC-3 parser
*/

#include "utils.h"
#include "io_base.h"
#include "registry.h"
#include "parser.h"
#include "parser_dd.h"

#define _REPORT(lvl,msg) parser_dd->reporter->report(parser_dd->reporter, lvl, msg)
static void parser_ec3_check_ccff_conformance(parser_dd_handle_t parser_dd);

/* on return, byte stream position is right after sync word
 * return EMA_MP4_MUXED_EOES, EMA_MP4_MUXED_OK
 */
static int
goto_next_syncword(bbio_handle_t ds, BOOL *isLE)
{
    uint8_t byte_read, last_read;
    uint32_t skiped; /* for debug only */

    if (ds->read(ds, &byte_read, 1) <= 0)
    {
        return EMA_MP4_MUXED_EOES;
    }

    /** handle the almost certain case outside of loop */
    if (byte_read == 0x0B)
    {
        if (ds->read(ds, &byte_read, 1) <= 0)
        {
            return EMA_MP4_MUXED_EOES;
        }
        if (byte_read == 0x77)
        {
            if (*isLE)
            {
                msglog(NULL, MSGLOG_INFO, "dd LE=>BE\n");
            }
            *isLE = FALSE;
            return EMA_MP4_MUXED_OK; /* already synced */
        }
    }
    else if (byte_read == 0x77)
    {
        if (ds->read(ds, &byte_read, 1) <= 0)
        {
            return EMA_MP4_MUXED_EOES;
        }
        if (byte_read == 0x0B)
        {
            if (!(*isLE))
            {
                msglog(NULL, MSGLOG_INFO, "dd BE=>LE\n");
            }
            *isLE = TRUE;
            return EMA_MP4_MUXED_OK; /* already synced */
        }
    }

    msglog(NULL, MSGLOG_ERR, "ERR: lost dd sync. resync\n");
    skiped = 1;
    while (1)
    {
        last_read = byte_read;

        if (ds->read(ds, &byte_read, 1) <= 0)
        {
            return EMA_MP4_MUXED_EOES;
        }
        if ((last_read == 0x0B && byte_read == 0x77) || (last_read == 0x77 && byte_read == 0x0B))
        {
            msglog(NULL, MSGLOG_INFO, "skip %u bytes\n", skiped);
            if (*isLE != (last_read == 0x77))
            {
                msglog(NULL, MSGLOG_INFO, "dd %s\n", (*isLE) ? "LE=>BE" : "BE=>LE");
                *isLE = !(*isLE);
            }
            break; /* got it */
        }
        skiped++;
    }

    return EMA_MP4_MUXED_OK;
}

static void
swap_byte_dd(uint8_t *buf, uint32_t data_len)
{
    uint8_t *pb  = buf, *p0;
    uint8_t *pbe = pb + data_len, b1;

    assert(!(data_len & 0x1));

    while (pb < pbe)
    {
        p0  = pb;
        b1  = *(++pb);
        *pb = *p0;
        *p0 = b1;

        pb++;
    }
}

static uint32_t
get_ind_subs_num(parser_dd_handle_t parser_dd)
{
    uint32_t u;

    for (u = 0; u < EC3_MAX_STREAMS && parser_dd->subs_ind[u].ddt != DD_TYPE_NONE; u++);
    return u;
}

static uint32_t
get_dep_subs_num(parser_dd_handle_t parser_dd, uint32_t streamID)
{
    uint32_t u;

    for (u = 0; u < EC3_MAX_SUBSTREAMS && parser_dd->subs[streamID][u].ddt != DD_TYPE_NONE; u++);
    return u;
}

static int
parse_ac3_substream(bbio_handle_t bs, parser_dd_handle_t parser_dd)
{
    uint8_t fscod, frmsizecod;
    dd_substream_t *substrm;
    uint32_t data_rate;

    /* to make it compatible with ec3 parser: generate a sample for every ac3 frame */
    if (parser_dd->last_indep < 0)
    {
        /* to expect and get an ac3 frame */
        parser_dd->last_indep = 0; parser_dd->last_dep = -1;
    }
    else
    {
        /* already got one */
        parser_dd->dd_frame_num++;
        parser_dd->last_indep = -1;
        return EMA_MP4_MUXED_OK;
    }
    substrm = &(parser_dd->subs_ind[AC3_SUBSTREAMID]);
    substrm->ddt = DD_TYPE_AC3;

    parser_dd->ddt     = DD_TYPE_AC3;
    parser_dd->numblks = 6;

    bs->skip_bytes(bs, 2);       /* crc1 */

    fscod      = (uint8_t)src_read_bits(bs, 2);
    frmsizecod = (uint8_t)src_read_bits(bs, 6);

    if (fscod >=3 || frmsizecod >= FRMSIZECOD_TOP)
    {
        msglog(NULL, MSGLOG_ERR, "ERR: fscod or frmsizecod\n");
        return EMA_MP4_MUXED_SYNC_ERR;
    }
    substrm->fscod         = fscod;
    substrm->bit_rate_code = frmsizecod >> 1;
    parser_dd->sample_rate = fscod_2_freq_tbl[fscod];
    parser_dd->frame_size  = ac3_frame_size_tbl[frmsizecod][fscod] << 1;  /* <<1: word=>byte */

    data_rate=ac3_bitrate_tbl[frmsizecod];
    /* test for data rate change */
    if (substrm->data_rate!=0 && substrm->data_rate!=data_rate)
    {
        msglog(NULL, MSGLOG_WARNING, "data rate change %d -> %d\n", substrm->data_rate, data_rate);
    }

    substrm->data_rate = data_rate;

    substrm->bsid          = (uint8_t)src_read_bits(bs, 5);
    substrm->bsmod         = (uint8_t)src_read_bits(bs, 3);
    substrm->acmod         = (uint8_t)src_read_bits(bs, 3);
    substrm->channel_flags = (uint8_t)acmod_tbl[substrm->acmod].channel_flags;

    if ((substrm->acmod & 0x01) && substrm->acmod != 0x01)
    {
        src_read_bits(bs, 2);
    }
    if (substrm->acmod & 0x04)
    {
        src_read_bits(bs, 2);
    }
    if (substrm->acmod == 0x02)
    {
        substrm->dsurmod = (uint8_t)src_read_bits(bs, 2);
    }
    else
    {
      substrm->dsurmod = 0;
    }

    substrm->lfeon = (uint8_t)src_read_bit(bs);
    if (substrm->lfeon)
    {
        substrm->channel_flags |= CHANMAP_LFE;
    }
    parser_dd->channel_flags_prg[AC3_SUBSTREAMID] = substrm->channel_flags;

    return EMA_MP4_MUXED_OK;
}

static void
get_channel_info(bbio_handle_t bs, dd_substream_t *substrm)
{
    int b;
    uint16_t chanmap = 0;
    for (b = 0; b < 16; b++)
    {
        /* CHANMAP_L is 1st bit */
        chanmap |= src_read_bit(bs) << b;
    }

    /* discard the reserved */
    substrm->channel_flags = chanmap & (~CHANMAP_reserved);
    /* no L, C, R, Ls, Rs, LFE for chan_loc */
    substrm->chan_loc = ((chanmap>>5) & 0xFF) | ((chanmap>>6) & 0x100);
}

static void
skip_ec3_mixmdate_2_infomdate(parser_dd_handle_t parser_dd, dd_substream_t *substrm,
                              uint16_t strmtyp, bbio_handle_t bs)
{
    /* mixing metadata */
    substrm->mixmdate = (uint8_t)src_read_bit(bs);
    if (substrm->mixmdate)
    {   /* mixmdate */
        if (substrm->acmod > 0x02)
        {
            src_skip_bits(bs, 2); /* dmixmod */
        }
        if ((substrm->acmod & 0x01) && (substrm->acmod > 0x2))
        {   /* if 3 front channels exist */
            src_skip_bits(bs, 3); /* ltrtcmixlev */
            src_skip_bits(bs, 3); /* lorocmixlev */
        }
        if (substrm->acmod & 0x04)
        {   /* if a surround channels exists */
            src_skip_bits(bs, 3); /* ltrtsurmixlev */
            src_skip_bits(bs, 3); /* lorosurmixlev */
        }
        if (substrm->lfeon)
        {   /* if LFE channel exists */
            if (src_read_bit(bs))
            {
                src_skip_bits(bs, 5); /* lfemixlevcod */
            }
        }
        if (strmtyp == 0x00)
        {   /* if independent stream */
            if (src_read_bit(bs))
            {
                src_skip_bits(bs, 6); /* pgmscl */
            }
            if (substrm->acmod == 0x0)
            {   /* 1+1 mono */
                if (src_read_bit(bs))
                {
                    src_skip_bits(bs, 6); /* pgmscl2 */
                }
            }
            if (src_read_bit(bs))
            {
                src_skip_bits(bs, 6); /* extpgmscl */
            }
            switch (src_read_bits(bs, 2)) {   /* mixdef */
            case 0x1:
                src_skip_bits(bs, 5);
                break;
            case 0x2:
                src_skip_bits(bs, 12);
                break;
            case 0x3:
                {
                    uint32_t mixdeflen = src_read_bits(bs, 5);
                    src_skip_bits(bs, 8 * (mixdeflen + 2));   /* mixdata */
                    break;
                }
            }

            if (substrm->acmod < 0x2)
            {   /* if mono or dual mono source */
                if (src_read_bit(bs))
                {   /* paninfoe */
                    src_skip_bits(bs, 8); /* panmean */
                    src_skip_bits(bs, 6); /* paninfo */
                }
                if (substrm->acmod == 0x0)
                {   /* 1+1 mode */
                    if (src_read_bit(bs))
                    {   /* paninfo2e */
                        src_skip_bits(bs, 8); /* panmean2 */
                        src_skip_bits(bs, 6); /* paninfo2 */
                    }
                }
            }
            if (src_read_bit(bs))
            {   /* frmmixcfginfoe */
                if (parser_dd->numblks == 0x1)
                {   /* numblkscod = 0 */
                    src_skip_bits(bs, 5); /* blkmixcfginfo[0] */
                }
                else
                {
                    int blk;
                    for (blk = 0; blk < parser_dd->numblks; blk++)
                    {
                        if (src_read_bit(bs))
                        {   /* blkmixcfginfoe */
                            src_skip_bits(bs, 5); /* blk,ixcfginfo[blk] */
                        }
                    }
                }
            }
        }
    }
}

/* on return: parser_dd->last_indep > -1 => find a substream, else end of aud_frame or file */
static int
parse_ec3_substream(bbio_handle_t bs, parser_dd_handle_t parser_dd)
{
    uint8_t strmtyp;
    uint8_t fscod;
    uint8_t bsid;
    uint8_t bsmod;
    uint8_t acmod;
    uint8_t lfeon;
    uint16_t substreamid;
    dd_substream_t *substrm;
    uint32_t data_rate;
    const int bCheckForChange = (parser_dd->mp4_sample_num &&
        parser_dd->reporter &&
        (IS_FOURCC_EQUAL(parser_dd->conformance_type, "cffh") ||
        IS_FOURCC_EQUAL(parser_dd->conformance_type, "cffs"))) ? 1 : 0;

    strmtyp = (uint8_t)src_read_bits(bs, 2);
    substreamid = (uint8_t)src_read_bits(bs, 3);

    /* substreamid: 0..7
     *   - if strmtyp=0 or 2: this frame belongs to an independent stream
     *   - if strmtyp=1: this frame belongs to dependent stream and follows its dependent stream
     */
    if (strmtyp == EC3_STRMTYPE_0 || strmtyp == EC3_STRMTYPE_2)
    {
        if (substreamid > parser_dd->last_indep)
        {
            if (substreamid != parser_dd->last_indep + 1)
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            /* start a new program with same mp4_sample_num */
            substrm = &(parser_dd->subs_ind[substreamid]);
            parser_dd->last_indep = substreamid;
            parser_dd->last_dep = -1;
            if (bCheckForChange && substrm->ddt != DD_TYPE_EC3)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of num_ind_subs detected.");
            }
        }
        else
        {
            if (substreamid != 0)
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            /* just got one ec3 frame */
            parser_dd->dd_frame_num++;
            parser_dd->last_indep = -1;
            return EMA_MP4_MUXED_OK;
        }
    }
    else if (strmtyp == EC3_STRMTYPE_1)
    {
        if (parser_dd->last_indep >= 0)
        {
            if (substreamid != parser_dd->last_dep + 1)
            {
                return EMA_MP4_MUXED_ES_ERR;
            }
            /* start a new dependent substream for last_indep */
            substrm = &(parser_dd->subs[parser_dd->last_indep][substreamid]);
            parser_dd->last_dep = substreamid;
            if (bCheckForChange && substrm->ddt != DD_TYPE_EC3)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of num_dep_subs detected.");
            }
        }
        else
        {
            msglog(NULL, MSGLOG_ERR, "ERR: get dependent substream without independent substream\n");
            return EMA_MP4_MUXED_SYNC_ERR;
        }
    }
    else
    {
        msglog(NULL, MSGLOG_ERR, "ERR: get strmtype 3\n");
        return EMA_MP4_MUXED_SYNC_ERR;
    }

    substrm->ddt = DD_TYPE_EC3;

    if (bCheckForChange && strmtyp != substrm->strmtyp)
    {
        _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of strmtyp detected.");
    }
    substrm->strmtyp = strmtyp;

    parser_dd->ddt = DD_TYPE_EC3;
    parser_dd->frame_size = (1 + src_read_bits(bs, 11)) << 1; /* <<1: word=>byte */
    /* DPRINTF(NULL, "EC-3: frame %d: substream %d type %d frame_size %d\n",
              frameid++,substreamid,strmtyp,parser_dd->frame_size); */

    fscod = (uint8_t)src_read_bits(bs, 2);
    if (fscod == 0x3)
    {
        fscod                  = (uint8_t)src_read_bits(bs, 2);
        parser_dd->sample_rate = fscod2_2_freq_tbl[fscod];
        parser_dd->numblks     = 6;
    }
    else
    {
        parser_dd->sample_rate = fscod_2_freq_tbl[fscod];
        parser_dd->numblks     = (uint8_t)numblks_tbl[src_read_bits(bs, 2)];
    }

    if (bCheckForChange && fscod != substrm->fscod)
    {
        if (strmtyp == EC3_STRMTYPE_1)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of fscod detected for dependent substream.");
        }
        else
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of fscod detected for independent substream.");
        }
    }
    substrm->fscod = fscod;

    acmod = (uint8_t)src_read_bits(bs, 3);
    if (bCheckForChange && acmod != substrm->acmod)
    {
        if (strmtyp == EC3_STRMTYPE_1)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of acmod detected for dependent substream.");
        }
        else
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of acmod detected for independent substream.");
        }
    }
    substrm->acmod = acmod;

    lfeon = (uint8_t)src_read_bit(bs);
    if (bCheckForChange && lfeon != substrm->lfeon)
    {
        if (strmtyp == EC3_STRMTYPE_1)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of lfeon detected for dependent substream.");
        }
        else
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of lfeon detected for independent substream.");
        }
    }
    substrm->lfeon = lfeon;

    bsid = (uint8_t)src_read_bits(bs, 5);
    if (bCheckForChange && bsid != substrm->bsid)
    {
        if (strmtyp == EC3_STRMTYPE_1)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of bsid detected for dependent substream.");
        }
        else
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of bsid detected for independent substream.");
        }
    }
    substrm->bsid = bsid;

    data_rate = /* in kbps: derived from frame size and its duration */
        (uint32_t)((float)(parser_dd->frame_size * parser_dd->sample_rate) /
                   (float)(parser_dd->numblks * 32000));

    if (substrm->data_rate!=0 && substrm->data_rate!=data_rate)
    {
        msglog(NULL, MSGLOG_WARNING, "data rate change %d -> %d\n", substrm->data_rate, data_rate);
    }
    substrm->data_rate=data_rate;

    src_skip_bits(bs, 5);     /* dialnorm */
    if (src_read_bit(bs))
    {   /* compre */
        src_skip_bits(bs, 8); /* compr */
    }

    if (substrm->acmod == 0x00)
    {
        src_skip_bits(bs, 5); /* dialnorm2 */
        if (src_read_bit(bs))
        {   /* compr2e */
            src_skip_bits(bs, 8); /* compr2 */
        }
    }

    substrm->chan_loc = 0;
    /* if dependent stream */
    if (strmtyp == EC3_STRMTYPE_1 && src_read_bit(bs))
    {   /* chanmape == 1 */
        uint16_t last_channel_flags = substrm->channel_flags;
        get_channel_info(bs, substrm);
        if (bCheckForChange && last_channel_flags != substrm->channel_flags)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of channel_flags detected.");
        }
    }
    else
    {
        substrm->channel_flags = (uint8_t)acmod_tbl[substrm->acmod].channel_flags;
        if (substrm->lfeon)
        {
            substrm->channel_flags |= CHANMAP_LFE;
        }
    }
    parser_dd->channel_flags_prg[parser_dd->last_indep] |= substrm->channel_flags;

    skip_ec3_mixmdate_2_infomdate(parser_dd, substrm, strmtyp, bs);

    /* informational metadata */
    if (src_read_bit(bs))
    {   /* infomdate */
        bsmod = (uint8_t)src_read_bits(bs, 3);
        if (bCheckForChange && bsmod != substrm->bsmod && strmtyp==EC3_STRMTYPE_0)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Illegal change of bsmod detected.");
        }
        substrm->bsmod = bsmod;
        
        src_skip_bits(bs, 1); /* copyrightb */
        src_skip_bits(bs, 1); /* origbs */
        if (acmod == 0x2) /* if in 2/0 mode */
        {
            src_skip_bits(bs, 2); /* dsurmod */
            src_skip_bits(bs, 2); /* dheadphonmod */
        }
        else if(acmod >= 0x6) /* if both surround channels exist */ 
        {
            src_skip_bits(bs, 2); /* dsurexmod */
        }

        if (src_read_bits(bs, 1)) /* audprodie */
        {
            src_skip_bits(bs, 5); /* mixlevel */
            src_skip_bits(bs, 2); /* roomtyp */
            src_skip_bits(bs, 1); /* adconvtyp */
        }

        if(acmod == 0x0) /* if 1+1 mode (dual mono, so some items need a second value) */
        {
            if (src_read_bits(bs, 1)) /*audprodi2e */
            {
                src_skip_bits(bs, 5); /* mixlevel2 */
                src_skip_bits(bs, 2); /* roomtyp2 */
                src_skip_bits(bs, 1); /* adconvtyp2 */
            }
        }

        if(fscod < 0x3) /* if not half sample rate */ 
        {
            src_skip_bits(bs, 1); /* sourcefscod */
        }
    }

    if( (strmtyp == 0x0) && (parser_dd->numblks != numblks_tbl[0x3]) ) 
    {
        src_read_bits(bs, 1); /* convsync */
    }

    if(strmtyp == 0x2) /* if bit stream converted from AC-3 */
    {
        uint8_t blkid = 0;
        if(parser_dd->numblks == numblks_tbl[0x3]) /* 6 blocks per syncframe */ 
        {
            blkid = 1;
        }
        else
        {
            blkid = (uint8_t)src_read_bits(bs, 1);
        }

        if (blkid)
        {
            src_skip_bits(bs, 6); /* frmsizecod */
        }
    }

    substrm->addbsie = (uint8_t)src_read_bits(bs, 1);
    if (substrm->addbsie)
    {
        substrm->addbsil = (uint8_t)src_read_bits(bs, 6) + 1;
        
        if (substrm->addbsil < sizeof(substrm->addbsi))
        {
            uint8_t i = 0;
            for (i = 0; i < substrm->addbsil; i++)
            {
                substrm->addbsi[i] = (uint8_t)src_read_bits(bs, 8);
            }
        }
    }

    return EMA_MP4_MUXED_OK;
}

/* get hdr of a substream */
static int
get_a_substream_frame_hdr(parser_dd_handle_t parser_dd, bbio_handle_t ds, uint64_t* pos)
{
    int       ret = EMA_MP4_MUXED_OK;
    uint8_t * hdr_buf;
    uint32_t  data_2_read = parser_dd->sample_pre_read_size - 2;

    bbio_handle_t memds;
    uint8_t       bsid;

    hdr_buf = parser_dd->sample_buf + parser_dd->sample_size;

    memds = reg_bbio_get('b', 'r'); /* to parse reset of hdr, need bit input module */
    memds->set_buffer(memds, hdr_buf+2, data_2_read, 0);

    do
    {   /* loop for resync */
        ret = goto_next_syncword(ds, &(parser_dd->isLE));  /* return: at frame offset 2 */
        if (ret != EMA_MP4_MUXED_OK)
        {
            /* only possibility: EOS */
            ret = EMA_MP4_MUXED_EOES;
            break;
        }

        if (pos)
        {
            *pos = ds->position(ds) - 2;
        }

        /** read data_2_read byte to header buf(additionally allocated already) */
        if (ds->read(ds, hdr_buf+2, data_2_read) != data_2_read)
        {
            ret = EMA_MP4_MUXED_EOES;
            break;
        }

        if (parser_dd->isLE)
        {
#if  KEEP_LE_DD
            hdr_buf[0] = 0x77; /* syncword: LE */
            hdr_buf[1] = 0x0B;
#else
            hdr_buf[0] = 0x0B; /* syncword: convert to BE */
            hdr_buf[1] = 0x77;
#endif

            swap_byte_dd(hdr_buf+2, data_2_read); /* LE = > BE */
        }
        else
        {
            hdr_buf[0] = 0x0B; /* syncword */
            hdr_buf[1] = 0x77;
        }

        bsid = (uint8_t)src_peek_bits(memds, 5, 3); /* bsid always at offset 5 */
        if (bsid <= 0x08)
        {
            ret = parse_ac3_substream(memds, parser_dd);
        }
        else if (bsid >= 0x0B && bsid <= 0x10)
        {
            ret = parse_ec3_substream(memds, parser_dd);
        }
        else
        {
            DPRINTF(NULL, "WARNING: got bsid 0x%02X. resync\n", bsid);
            ret = EMA_MP4_MUXED_SYNC_ERR;
        }

#if  KEEP_LE_DD
        if (parser_dd->isLE)
        {
            swap_byte_dd(hdr_buf+2, data_2_read); /* BE = > LE */
        }
#endif

        if (ret == EMA_MP4_MUXED_OK)
        {
            break;
        }

        msglog(NULL, MSGLOG_WARNING, "frame sync problem");

        memds->seek(memds, 0, SEEK_SET);
        /* just to be sure */
        src_byte_align(memds);
        src_byte_align(ds);
    } while (1);

    /** close bitstream */
    memds->destroy(memds);

    return ret;
}

static void
get_rest_of_substream_frame(parser_dd_handle_t parser_dd)
{
    uint8_t * buf       = parser_dd->sample_buf;
    uint32_t  data_read = parser_dd->sample_size + parser_dd->sample_pre_read_size;

    /** if need to expand sample buf */
    if (parser_dd->sample_buf_size < parser_dd->sample_size + parser_dd->frame_size)
    {
        parser_dd->sample_buf_size += parser_dd->frame_size;
        buf = MALLOC_CHK(parser_dd->sample_buf_size + parser_dd->sample_pre_read_size);
        memcpy(buf, parser_dd->sample_buf, data_read);
        FREE_CHK(parser_dd->sample_buf);
        parser_dd->sample_buf = buf;
    }

    /** loading the reset of substream frame */
    if (parser_dd->frame_size > parser_dd->sample_pre_read_size)
    {
        parser_dd->ds->read(parser_dd->ds, buf + data_read,
                            parser_dd->frame_size -  parser_dd->sample_pre_read_size);
#if !KEEP_LE_DD
        if (parser_dd->isLE)
        {
            /* LE = > BE */
            swap_byte_dd(buf + data_read, parser_dd->frame_size -  parser_dd->sample_pre_read_size);
        }
#endif
    }
    parser_dd->sample_size += parser_dd->frame_size;
}

static uint32_t
get_channel_num(uint16_t channel_flags)
{
    int      k;
    uint32_t channel_num = 0;

    for (k = 0; k < 16; k++)
    {
        if (channel_flags & (1 << k))
        {
            channel_num += channel_num_tbl[k];
        }
    }
    return channel_num;
}

/* get a new ac3 or eac3 frame of multiple program with dependent substreams */
static int
parser_dd_get_sample(parser_handle_t parser, mp4_sample_handle_t sample)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;
    bbio_handle_t      ds        = parser->ds;
    uint64_t           pos;
    int                ret       = EMA_MP4_MUXED_OK;
    int                loop      = 0;

#if PARSE_DURATION_TEST
    if (parser_dd->dts >= PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    parser_dd->last_indep     = -1;
    parser_dd->sample_size    = 0;
    parser_dd->aud_sample_num = 0;
    do
    {   /* loop to accumulate numblks == 6 */
        pos = ds->position(ds); /* to rollback to ind start point */

        ret = get_a_substream_frame_hdr(parser_dd, ds, &pos);
        if (ret != EMA_MP4_MUXED_OK)
        {
            if (ret != EMA_MP4_MUXED_EOES || parser_dd->last_indep == -1)
            {
                /* if not eos nor a new frame: error */
                return ret;
            }

            /* got EOS and a frame */
            if ((parser_dd->last_indep == EC3_MAX_STREAMS - 1 ||
                 parser_dd->subs_ind[parser_dd->last_indep + 1].ddt == DD_TYPE_NONE)
                &&
                (parser_dd->last_dep == EC3_MAX_SUBSTREAMS - 1 ||
                 parser_dd->subs[parser_dd->last_indep][parser_dd->last_dep + 1].ddt == DD_TYPE_NONE))
            {
                /* got a valid dd frame */
                parser_dd->dd_frame_num++;
                parser_dd->aud_sample_num += parser_dd->numblks*SAMPLES_PER_BLOCK;

                parser_dd->last_indep = -1; /* make it consistent */
            }
            /* else partail dd frame => discard */
            break;
        }

        if (parser_dd->last_indep == -1)
        {
            /* already go to end of a dd frame: samples in dd_frame collected so far */
            parser_dd->aud_sample_num += parser_dd->numblks*SAMPLES_PER_BLOCK;

            if (parser_dd->aud_sample_num >= 1536)
            {
                /* got a complete mp4 sample */
                break;
            }

            loop = 1; /* need more */
            ds->seek(ds,pos,SEEK_SET);/*rollback to beginning of next frame */
            /* just to be sure */
            src_byte_align(ds);
        }
        else
        {
            if (parser_dd->ddt == DD_TYPE_AC3)
            {
                /* got a actual ac3 substream frame
                 * ac-3 always has numblks of 6 and 1536 samples and
                 * must be the first ind substream(AC3_SUBSTREAMID == 0)
                 */
                if (parser_dd->mp4_sample_num == 0)
                {
                    msglog(NULL, MSGLOG_INFO, "first AC3 frame is %s\n", (parser_dd->isLE) ? "LE" : "BE");
                    parser_dd->bit_rate = parser_dd->subs_ind[AC3_SUBSTREAMID].data_rate * 1000;
                    parser_dd->nfchans_prg[AC3_SUBSTREAMID] =
                        get_channel_num(parser_dd->channel_flags_prg[AC3_SUBSTREAMID]);
                }
                /*else, already know every thing */
            }
            else
            {
                /* got a actual ec3 substream frame
                 * only count once if numblks < 6 */
                if (parser_dd->mp4_sample_num == 0 && loop == 0)
                {
                   dd_substream_t *substrm;

                    if (parser_dd->last_dep < 0)
                    {
                        /* the independet substream */
                        substrm = &(parser_dd->subs_ind[parser_dd->last_indep]);
                        msglog(NULL, MSGLOG_INFO, "%dth EC3 independent frame is %s\n",
                               parser_dd->last_indep, (parser_dd->isLE) ? "LE" : "BE");
                    }
                    else
                    {
                        /* the dependet substream */
                        substrm = &(parser_dd->subs[parser_dd->last_indep][parser_dd->last_dep]);
                        msglog(NULL, MSGLOG_INFO, "%dth EC3 dependent frame is %s\n",
                               parser_dd->last_dep, (parser_dd->isLE) ? "LE" : "BE");
                    }

                    parser_dd->bit_rate += substrm->data_rate * 1000;
                    parser_dd->nfchans_prg[parser_dd->last_indep] =
                        get_channel_num(parser_dd->channel_flags_prg[parser_dd->last_indep]);
                        /* channel_flags_prg updated as each indep/dep substream parsed */
                }
                /*else, already know every thing */
            }
            get_rest_of_substream_frame(parser_dd);

            /* continue to get/confirm a complete dd frame */
       }
    } while(1);

    if (IS_FOURCC_EQUAL(parser->conformance_type, "cffh") ||
        IS_FOURCC_EQUAL(parser->conformance_type, "cffs"))
    {
        if (parser_dd->mp4_sample_num == 0)
        {
            parser_ec3_check_ccff_conformance(parser_dd);
        }
    }

    if (ret != EMA_MP4_MUXED_EOES)
    {
        /* must already get a mp4 sample */
        ds->seek(ds, pos, SEEK_SET); /* rollback to following ind frame */
        /* just to be sure */
        src_byte_align(ds);
    }
    else
    {
        if (parser_dd->aud_sample_num == 1536)
        {
            /* get the last complete mp4 sample. output it */
            ret = EMA_MP4_MUXED_OK;
        }
        else
        {
            msglog(NULL, MSGLOG_WARNING, "\ndiscard imcomplete mp4 sampes of %u EC3 frames\n",
                   parser_dd->aud_sample_num/(((uint16_t)parser_dd->numblks)*SAMPLES_PER_BLOCK));
            return ret;
        }
    }

    if (parser_dd->mp4_sample_num == 0)
    {
        /** update parser_dd context */
        parser_dd->num_units_in_tick = parser_dd->aud_sample_num;
        parser_dd->time_scale        = parser_dd->sample_rate;

        parser_dd->num_ind_sub = get_ind_subs_num(parser_dd);
    }
    else
    {
        parser_dd->dts += parser_dd->aud_sample_num;
    }

    DPRINTF(NULL, "mp4 sample %4u(dd frame %4u): %" PRIu64 "ms, size %u\n",
            parser_dd->mp4_sample_num, parser_dd->dd_frame_num-1,
            parser_dd->sample_rate?(((uint64_t)1000)*parser_dd->dts)/parser_dd->sample_rate:0,
            parser_dd->sample_size);
        /* dd_frame_num->dd_frame_num-1: dd_frame_num increased when got a frame, but
        *  mp4_sample_num increas after this print out */

    /** setup output sample */
    if (sample)
    {
        parser_dd->sample_buf_alloc_only = TRUE;

        sample->flags = SAMPLE_SYNC;
        if (!parser_dd->mp4_sample_num)
        {
            sample->flags |= SAMPLE_NEW_SD; /* the first one should have all the new info */
        }
        sample->dts      = parser_dd->dts;
        sample->cts      = sample->dts;
        sample->duration = parser_dd->aud_sample_num;
        sample->size     = parser_dd->sample_size;
        sample->data     = parser_dd->sample_buf;
    }
    parser_dd->mp4_sample_num++;

    return ret;
}

#ifdef WANT_GET_SAMPLE_PUSH
static void
acc_sync_hdr(parser_dd_handle_t parser_dd, SEsData_t *psEsd, uint32_t u32EsOff, int bLe)
{
    uint32_t data_seg_size, data2cp, cp_size;

    /** cp data upto nal_size */
    data_seg_size = psEsd->u32DataInSize - u32EsOff;

    data2cp = parser_dd->sf_pre_buf_num - parser_dd->sf_bufed_num;
    cp_size = (data2cp < data_seg_size) ? data2cp : data_seg_size;

    memcpy(parser_dd->sf_buf +  parser_dd->sf_bufed_num, psEsd->pBufIn + u32EsOff, cp_size);
    parser_dd->sf_bufed_num += cp_size;
    if (parser_dd->sf_bufed_num == parser_dd->sf_pre_buf_num)
    {
        if (bLe)
        {
            swap_byte_dd(parser_dd->sf_buf, parser_dd->sf_pre_buf_num);
        }
    }
}

static int
start_new_sample(parser_dd_handle_t parser_dd)
{
    parser_dd->last_indep     = -1;
    parser_dd->sample_size    = 0;
    parser_dd->aud_sample_num = 0;

    return EMA_MP4_MUXED_OK;
}

static int
build_sample(parser_dd_handle_t parser_dd)
{
    mp4_sample_handle_t sample;

    assert(!parser_dd->is1536AudSmplRdy);

    if (parser_dd->mp4_sample_num == 0)
    {
        /** update parser_dd context */
        parser_dd->num_units_in_tick = parser_dd->aud_sample_num;
        parser_dd->time_scale        = parser_dd->sample_rate;

        parser_dd->num_ind_sub = get_ind_subs_num(parser_dd);
    }
    else
    {
        parser_dd->dts += parser_dd->aud_sample_num;
    }

    DPRINTF(NULL, "mp4 sample %4u(dd frame %4u): %" PRIu64 "ms, size %u\n",
            parser_dd->mp4_sample_num, parser_dd->dd_frame_num-1,
            (((uint64_t)1000)*parser_dd->dts)/parser_dd->sample_rate,
            parser_dd->sample_size);
    /* dd_frame_num->dd_frame_num-1: dd_frame_num increased when got a frame, but
     *  mp4_sample_num increas after this print out */

    /** setup output sample */
    sample = &(parser_dd->sample_got);
    sample->flags = SAMPLE_SYNC;
    if (!parser_dd->mp4_sample_num)
    {
        sample->flags |= SAMPLE_NEW_SD; /* the first one should have all the new info */
    }
    sample->dts      = parser_dd->dts;
    sample->cts      = sample->dts;
    sample->duration = parser_dd->aud_sample_num;
    sample->size     = parser_dd->sample_size;
    parser_dd->mp4_sample_num++;

    parser_dd->is1536AudSmplRdy = TRUE;

    return EMA_MP4_MUXED_OK;
}

static void
get_last_mp4_sample(parser_dd_handle_t parser_dd, mp4_sample_handle_t sample)
{
    if (!parser_dd->is1536AudSmplRdy)
    {
        /* not ready */
        sample->flags = SAMPLE_PARTIAL_AU; /* not ready yet */
    }
    else
    {
        void (*destroy)(struct mp4_sample_t_ *) = sample->destroy;

        memcpy(sample, &(parser_dd->sample_got), sizeof(mp4_sample_t));
        sample->destroy             = destroy;
        parser_dd->is1536AudSmplRdy = FALSE;
    }
}

static void
start_new_sync_frame(parser_dd_handle_t parser_dd)
{
    parser_dd->frame_size   = 0;
    parser_dd->sf_bufed_num = 0;
    parser_dd->sf_data_got  = 2; /* parsing the sync frame only when sh is found */
}

#if !KEEP_LE_DD_TS
static void
SwapSf(parser_dd_handle_t parser_dd, uint8_t *pData, uint32_t u32DataSize)
{
    assert(u32DataSize);

    if (parser_dd->pu8Swap0)
    {
        uint8_t u8 = *(parser_dd->pu8Swap0);
        *(parser_dd->pu8Swap0) = *pData;
        *pData = u8;
        pData++;
        u32DataSize--;
    }

    if (u32DataSize & 0x1)
    {
        u32DataSize--;
        parser_dd->pu8Swap0 = pData + u32DataSize;
    }
    else
    {
        parser_dd->pu8Swap0 = 0;
    }

    swap_byte_dd(pData, u32DataSize);
}
#endif

/* if sample->flags && SAMPLE_PARTIAL_SS:
 *    parse psEsd to get a new ac3 or eac3 sync frame info. build mp4 sample info if such a sample reached
 *    sync frame body start at esd SSs_t::u8BodyIdx, u32BodyOff
 * else return mp4sample collected
 */
static int
parser_dd_get_sample_push(parser_handle_t parser, SEsData_t *psEsd, SSs_t *psSf, mp4_sample_handle_t sample)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;
    int                ret       = EMA_MP4_MUXED_OK;

#if PARSE_DURATION_TEST
    if (parser_dd->dts >= PARSE_DURATION_TEST*(uint64_t)parser->time_scale)
    {
        return EMA_MP4_MUXED_EOES;
    }
#endif

    if (sample->flags & SAMPLE_PARTIAL_SS)
    {
        uint32_t u32Offset = (uint32_t)sample->size;

        /****** parsing and get sync frame mode */
        if (parser_dd->sf_bufed_num < parser_dd->sf_pre_buf_num)
        {
            /**** to accumulate enough data */
            acc_sync_hdr(parser_dd, psEsd, u32Offset, psSf->u8FlagsLidx & LE_FLAG);
            if (parser_dd->sf_bufed_num == parser_dd->sf_pre_buf_num)
            {
                /** got enough data to parser hdr */
                bbio_handle_t memds;
                uint8_t       bsid;

                memds = reg_bbio_get('b', 'r'); /* to parse reset of hdr, need bit input module */
                memds->set_buffer(memds, parser_dd->sf_buf, parser_dd->sf_bufed_num, 0);

                bsid = src_peek_bits(memds, 5, 3); /* bsid always at offset 5 */
                if (bsid <= 0x08)
                {
                    ret = parse_ac3_substream(memds, parser_dd);
                }
                else if (bsid >= 0x0B && bsid <= 0x10)
                {
                    ret = parse_ec3_substream(memds, parser_dd);
                }
                else
                {
                    DPRINTF(NULL, "WARNING: got bsid 0x%02X. resync\n", bsid);
                    ret = EMA_MP4_MUXED_SYNC_ERR;
                }

                if (ret != EMA_MP4_MUXED_OK)
                {
                    assert(ret == EMA_MP4_MUXED_SYNC_ERR);
                    memds->destroy(memds);
                    return ret;
                }

                /** come here: got the sync frame hdr right */
                if (parser_dd->last_indep == -1)
                {
                    /* current frame is not parsed yet.
                     *  knew last dd frame end. samples in dd_frame collected so far: */
                    parser_dd->aud_sample_num += parser_dd->numblks*SAMPLES_PER_BLOCK;

                    if (parser_dd->aud_sample_num >= 1536)
                    {
                        /* got a complete mp4 sample */
                        build_sample(parser_dd);
                        start_new_sample(parser_dd);
                    }
                    /* parse current frame */
                    /* since bbio interfacer lack a rewind method, use the following equivalent */
                    src_byte_align(memds); /* byte align */
                    memds->set_buffer(memds, parser_dd->sf_buf, parser_dd->sf_bufed_num, 0); /* to very beginning */

                    if (bsid <= 0x08)
                    {
                        ret = parse_ac3_substream(memds, parser_dd);
                    }
                    else if (bsid >= 0x0B && bsid <= 0x10)
                    {
                        ret = parse_ec3_substream(memds, parser_dd);
                    }
                }
                else if (parser_dd->mp4_sample_num == 0 && parser_dd->aud_sample_num == 0)
                {
                    if (parser_dd->ddt == DD_TYPE_AC3)
                    {
                        /* got a actual ac3 substream frame
                         * ac-3 always has numblks of 6 and 1536 samples and
                         * must be the first ind substream(AC3_SUBSTREAMID == 0) */

                        msglog(NULL, MSGLOG_INFO, "first AC3 frame is %s\n", (psSf->u8FlagsLidx & LE_FLAG) ? "LE" : "BE");
                        parser_dd->bit_rate = parser_dd->subs_ind[AC3_SUBSTREAMID].data_rate * 1000;
                        parser_dd->nfchans_prg[AC3_SUBSTREAMID] =
                            get_channel_num(parser_dd->channel_flags_prg[AC3_SUBSTREAMID]);
                    } /* ac3 info */
                    else
                    {
                        /* got a actual ec3 substream frame
                         * only count once if numblks < 6 */
                        dd_substream_t *substrm;

                        if (parser_dd->last_dep < 0)
                        {
                            /* the independet substream */
                            substrm = &(parser_dd->subs_ind[parser_dd->last_indep]);
                            msglog(NULL, MSGLOG_INFO, "%dth EC3 independent frame is %s\n",
                                   parser_dd->last_indep, (psSf->u8FlagsLidx & LE_FLAG) ? "LE" : "BE");
                        }
                        else
                        {
                            /* the dependet substream */
                            substrm = &(parser_dd->subs[parser_dd->last_indep][parser_dd->last_dep]);
                            msglog(NULL, MSGLOG_INFO, "%dth EC3 dependent frame is %s\n",
                                   parser_dd->last_dep, (psSf->u8FlagsLidx & LE_FLAG) ? "LE" : "BE");
                        }

                        parser_dd->bit_rate += substrm->data_rate * 1000;
                        parser_dd->nfchans_prg[parser_dd->last_indep] =
                            get_channel_num(parser_dd->channel_flags_prg[parser_dd->last_indep]);
                        /* channel_flags_prg updated as each indep/dep substream parsed */
                    } /* ec3 info */
                } /* collecting per mp4 frame info */
                /* else, already know every thing */

                memds->destroy(memds);
            } /* get enough frame hdr data and parsing is done */
            /** else not enough data for sync hdr */
        }
        /**** else already got sync frame hdr */

        if (parser_dd->frame_size && parser_dd->sf_data_got + psEsd->u32DataInSize - u32Offset >= parser_dd->frame_size)
        {
            /**** current sync frame hdr known and data is complete */
            sample->size = (parser_dd->frame_size - parser_dd->sf_data_got) + u32Offset;/* next expecting sh */
            sample->flags &= ~SAMPLE_PARTIAL_SS;  /* sync frame complete */

            psSf->u8FlagsLidx &= ~LAYER_IDX_MASK;
            if (parser_dd->last_dep >= 0)
            {
                /* the dependent */
                psSf->u8FlagsLidx |= 0x08 | parser_dd->last_dep;
            }
            else
            {
                psSf->u8FlagsLidx |= parser_dd->last_indep;
            }

            parser_dd->sample_size += parser_dd->frame_size;
            start_new_sync_frame(parser_dd);

#if !KEEP_LE_DD_TS
            if (psSf->u8FlagsLidx & LE_FLAG)
            {
                SwapSf(parser_dd, psEsd->pBufIn + u32Offset, sample->size - u32Offset);
            }
#endif
        }
        else
        {
            parser_dd->sf_data_got += psEsd->u32DataInSize - u32Offset;

#if !KEEP_LE_DD_TS
            if (psSf->u8FlagsLidx & LE_FLAG)
            {
                SwapSf(parser_dd, psEsd->pBufIn + u32Offset, psEsd->u32DataInSize - u32Offset);
            }
#endif
        }

        return EMA_MP4_MUXED_OK;
    } /****** sync frame level parsing */

    /****** au are pushed out => simply get the Au build, except EOES */
    if (!psSf->u32BodySize)
    {
        /* push mode and 0 data mean end of file */
        /* got EOS and a frame */
        if ((parser_dd->sf_bufed_num == 0)
            &&
            (parser_dd->last_indep == EC3_MAX_STREAMS - 1 ||
             parser_dd->subs_ind[parser_dd->last_indep + 1].ddt == DD_TYPE_NONE)
            &&
            (parser_dd->last_dep == EC3_MAX_SUBSTREAMS - 1 ||
             parser_dd->subs[parser_dd->last_indep][parser_dd->last_dep + 1].ddt == DD_TYPE_NONE))
        {
            /* end of a sync frame and got a valid dd frame but not push out yet */
            parser_dd->dd_frame_num++;
            parser_dd->aud_sample_num += parser_dd->numblks*SAMPLES_PER_BLOCK;
            parser_dd->last_indep = -1; /* make it consistent */
        }

        if (parser_dd->aud_sample_num == 1536)
        {
            assert(parser_dd->sample_size);
            /* get the last complete mp4 sample. output it */

            build_sample(parser_dd);
            start_new_sample(parser_dd);  /* just to be consistent with normal AU */
        }
        else
        {
            msglog(NULL, MSGLOG_WARNING, "\ndiscard imcomplete dd frame of %u byte", parser_dd->sf_data_got);
            if (parser_dd->ddt == DD_TYPE_EC3)
            {
                msglog(NULL, MSGLOG_WARNING,". about %u sample of EC3 frames\n",
                       parser_dd->aud_sample_num/(((uint16_t)parser_dd->numblks)*SAMPLES_PER_BLOCK));
            }
            else
            {
                msglog(NULL, MSGLOG_WARNING, "\n");
            }
            /* return EMA_MP4_MUXED_EOES; end of stream. just keep it going */
        }
    } /* end of data */

    get_last_mp4_sample(parser_dd, sample);

    return EMA_MP4_MUXED_OK;
}
#endif  /* WANT_GET_SAMPLE_PUSH */

static int
parser_ac3_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;
    dd_substream_t *   sub       = &parser_dd->subs_ind[AC3_SUBSTREAMID];
    bbio_handle_t      snk;

    DPRINTF(NULL,
            "[AC3] fscod %d, bsid %d, bsmod %d, acmod %d, lfeon %d, bit_rate_code %d\n",
            sub->fscod, sub->bsid, sub->bsmod, sub->acmod, sub->lfeon,
            sub->bit_rate_code);

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 4, 0); /* in fact 3 is enough */
    }

    sink_write_bits(snk, 2, sub->fscod);
    sink_write_bits(snk, 5, sub->bsid);
    sink_write_bits(snk, 3, sub->bsmod);
    sink_write_bits(snk, 3, sub->acmod);
    sink_write_bits(snk, 1, sub->lfeon);
    sink_write_bits(snk, 5, sub->bit_rate_code);
    sink_write_bits(snk, 5, 0);

    /* sink_flush_bits(snk); a;ready aligned */

    *buf = snk->get_buffer(snk, buf_len, 0);/* here buf_len is set to data_size */
    snk->destroy(snk);

    return 0;
}

int
parser_ec3_get_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len, BOOL dump_joc_flag)
{
    parser_dd_handle_t parser_dd   = (parser_dd_handle_t)parser;
    uint32_t           i, j;
    uint32_t           num_indep_sub = 0;
    uint32_t           num_dep_sub = 0;
    bbio_handle_t      snk;

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 8, 1); /* in fact 6 is enough */
    }

    sink_write_bits(snk, 5, (parser_dd->bit_rate / 1000) >> 8);
    sink_write_bits(snk, 8, (parser_dd->bit_rate / 1000) & 0xff);

    num_indep_sub = parser_dd->num_ind_sub;
    sink_write_bits(snk, 3, num_indep_sub-1);

    for (i = 0;i < num_indep_sub; i++)
    {
        uint32_t           num_dep_sub_tmp = 0;
        dd_substream_t *ss = &parser_dd->subs_ind[i];
        sink_write_bits(snk, 2, ss->fscod);
        sink_write_bits(snk, 5, ss->bsid);
        sink_write_bits(snk, 2, 0);         /* 2 bits reserved */
        sink_write_bits(snk, 3, ss->bsmod); /* bsmod take 3 bits */
        sink_write_bits(snk, 3, ss->acmod);
        sink_write_bits(snk, 1, ss->lfeon);
        sink_write_bits(snk, 3, 0);

        num_dep_sub_tmp = get_dep_subs_num(parser_dd, i);
        sink_write_bits(snk, 4, num_dep_sub_tmp);
        if (num_dep_sub_tmp)
        {
            uint16_t chan_loc = 0;
            for (j = 0; j < EC3_MAX_SUBSTREAMS; j++)
            {
                dd_substream_t *psub = &(parser_dd->subs[i][j]);
                if (psub->ddt == DD_TYPE_NONE)
                {
                    break;
                }
                chan_loc |= psub->chan_loc;
            }
            sink_write_bits(snk, 1, chan_loc >> 8 );
            sink_write_bits(snk, 8, chan_loc & 0xff);
        }
        else
        {
            sink_write_bits(snk, 1, 0); /* reserved */
        }
    }

    if (dump_joc_flag)
    {
        dd_substream_t * pactive_stream = NULL;
        if (num_indep_sub)
        {
            num_dep_sub = get_dep_subs_num(parser_dd, 0); /* foucs on 0 stream only */
            if (num_dep_sub)
            {
                pactive_stream = &(parser_dd->subs[0][0]);
            }
            else 
            {
                pactive_stream = &parser_dd->subs_ind[0];
            }
        }

        /* trigger JOC */
        if (pactive_stream && pactive_stream->addbsie && pactive_stream->addbsil >= 1)
        {
            sink_write_u8(snk, pactive_stream->addbsi[0]);
            if (pactive_stream->addbsi[0] && pactive_stream->addbsil >= 2)
            {
                sink_write_u8(snk, pactive_stream->addbsi[1]);
            }
        }
    }

    /* sink_flush_bits(snk); already aligned */
    *buf = snk->get_buffer(snk, buf_len, 0);  /* here buf_len is set to data_size */
    snk->destroy(snk);

    return 0;
}

static int
parser_ec3_get_mp4_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    return parser_ec3_get_cfg(parser, buf, buf_len, TRUE);
}

static int
parser_ec3_get_uv_cfg(parser_handle_t parser, uint8_t **buf, size_t *buf_len)
{
    return parser_ec3_get_cfg(parser, buf, buf_len, FALSE);
}

static size_t
parser_ac3_get_mp2_cfg_len_ex(parser_handle_t parser, int ts_pro)
{
    return (ts_pro != TS_PRO_DVB) ? 6 + 5 : 3;
    (void)parser;  /* avoid compiler warning */
}

static int
parser_ac3_get_mp2_cfg_ex(parser_handle_t parser, uint8_t **buf, size_t *buf_len, int ts_pro)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;
    dd_substream_t *   sub       = &parser_dd->subs_ind[AC3_SUBSTREAMID];
    bbio_handle_t      snk;

    DPRINTF(NULL,
            "[AC3, EC3] fscod %d, bsid %d, bsmod %d, acmod %d, lfeon %d, bit_rate_code %d\n",
            sub->fscod, sub->bsid, sub->bsmod, sub->acmod, sub->lfeon,
            sub->bit_rate_code);

    snk = reg_bbio_get('b', 'w');
    if (*buf)
    {
        snk->set_buffer(snk, *buf, *buf_len, 1);
    }
    else
    {
        snk->set_buffer(snk, NULL, 6 + 5, 0);
    }

    if (ts_pro != TS_PRO_DVB)
    {
        /** AC-3 register_descriptor */
        sink_write_u8(snk, 0x05);  /* tag */
        sink_write_u8(snk, 4);     /* len */

        sink_write_4CC(snk, "AC-3");

        /** AC-3_audio_stream_descriptor */
        sink_write_u8(snk, 0x81);  /* tag */
        sink_write_u8(snk, 3);     /* len */

        sink_write_bits(snk, 3, sub->fscod);
        sink_write_bits(snk, 5, sub->bsid);

        sink_write_bits(snk, 6, sub->bit_rate_code);
        sink_write_bits(snk, 2, sub->dsurmod);

        sink_write_bits(snk, 3, sub->bsmod);
        sink_write_bits(snk, 4,  parser_dd->nfchans_prg[AC3_SUBSTREAMID]);
        sink_write_bits(snk, 1, 1);
    }
    else
    {
        /* AC-3/EC-3 descriptor: build a simplist one for now */
        /* tag */
        if (parser_dd->stream_id == STREAM_ID_AC3)
        {
            sink_write_u8(snk, 0x6A);
        }
        else
        {
            sink_write_u8(snk, 0x7A);
        }
        sink_write_u8(snk, 1);                       /* len */
        if (parser_dd->stream_id == STREAM_ID_AC3)
        {
           sink_write_u8(snk, 0);                    /* no optional field */
        }
        else
        {
            /* ec3: for now assume only one indep sunstream */
            sink_write_u8(snk, sub->mixmdate << 3);  /* set only mixmdate */
        }
    }

    /* sink_flush_bits(snk); already aligned */

    *buf = snk->get_buffer(snk, buf_len, 0);  /* here buf_len is set to data_size */
    snk->destroy(snk);

    return 0;

}

static int
parser_dd_get_param_ex(parser_handle_t parser, stream_param_id_t param_id, int32_t param_idx, void *param)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;

    uint32_t t;

    switch (param_id)
    {
    case STREAM_PARAM_ID_TIME_SCALE:
        t = parser->time_scale;
        break;

    case STREAM_PARAM_ID_NUM_UNITS_IN_TICK:
        t = parser->num_units_in_tick;
        break;

    case STREAM_PARAM_ID_FRAME_DUR:
        t = parser->num_units_in_tick;
        break;

    case STREAM_PARAM_ID_MIN_CTS:
        t = 0;
        break;

    case STREAM_PARAM_ID_DLT_DTS_TC:
        t = 1;
        break;

    case STREAM_PARAM_ID_B_SIZE:
        t = 2592; /* Ac3 in byte */
        if (parser_dd->stream_id == STREAM_ID_EC3)
        {
            t <<= 1;  /* EC3 */
        }
        break;

    case STREAM_PARAM_ID_RX:
        t = 2000000;
        break;

    case STREAM_PARAM_ID_DEC_DELAY:
        t = 0;
        break;

    default:
        assert(0);
        return EMA_MP4_MUXED_PARAM_ERR;
    }

    *((uint32_t *)param) = t;
    return EMA_MP4_MUXED_OK;
    (void)param_idx;  /* avoid compiler warning */
}

/* use ATSC cfg */
static uint32_t
parser_dd_get_param(parser_handle_t parser, stream_param_id_t param_id)
{
    uint32_t t;

    if (parser_dd_get_param_ex(parser, param_id, TS_PRO_ATSC, &t))
    {
        return (uint32_t)-1;
    }

    return t;
}

/* print out the a ac3/eac3 sub stream info */
static void
show_substream_info(dd_substream_t *psub)
{
    int k;

    msglog(NULL, MSGLOG_INFO, "          bsid         %u\n", psub->bsid);
    msglog(NULL, MSGLOG_INFO, "          fscod        %u\n", psub->fscod);
    k = (psub->bsmod < 7) ? psub->bsmod : ((psub->acmod == 1) ? 7 : 8);
    msglog(NULL, MSGLOG_INFO, "          bsmod        %u (%s)\n", psub->bsmod, bsmod_tbl[k]);
    msglog(NULL, MSGLOG_INFO, "          acmod        %u (%s)\n", psub->acmod, acmod_tbl[psub->acmod].audio_coding_mode);
    msglog(NULL, MSGLOG_INFO, "          lfeon        %u\n",      psub->lfeon);
    msglog(NULL, MSGLOG_INFO, "          data rate    %u kbps\n", psub->data_rate);
    msglog(NULL, MSGLOG_INFO, "          channels     [ ");
    for (k = 0; k < 16; k++)
    {
        if (psub->channel_flags & (1 << k))
        {
            msglog(NULL, MSGLOG_INFO, "%s ", channel_desc_tbl[k]);
        }
    }
    msglog(NULL, MSGLOG_INFO, "]\n");

    if (psub->ddt == DD_TYPE_AC3)
    {
        msglog(NULL, MSGLOG_INFO, "          bitrate code %u\n", psub->bit_rate_code);
    }
    else
    {
        msglog(NULL, MSGLOG_INFO, "          mp4 chan_loc 0x%02X", psub->chan_loc);
        if (psub->chan_loc)
        {
            msglog(NULL, MSGLOG_INFO, " [ ");
            for (k = 0; k < 9; k++)
            {
                if (psub->chan_loc & (1 << k))
                {
                    msglog(NULL, MSGLOG_INFO, "%s ", mp4_chan_loc_tbl[k]);
                }
            }
            msglog(NULL, MSGLOG_INFO, "]");
        }
        msglog(NULL, MSGLOG_INFO, "\n");
    }
    msglog(NULL, MSGLOG_INFO, "          dsurmod      %u\n", psub->dsurmod);
}

static void
parser_dd_show_info(parser_handle_t parser)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;
    uint32_t prg, j, channel_flags;
    dd_substream_t *psub;

    if (parser_dd == NULL)
    {
        return;
    }
    if (!parser_dd->dd_frame_num)
    {
        msglog(NULL, MSGLOG_INFO, "  No AU found\n");
        return;
    }

    msglog(NULL, MSGLOG_INFO, "Dolby stream:\n");
    msglog(NULL, MSGLOG_INFO, "  data rate   %u bps\n", parser_dd->bit_rate);
    msglog(NULL, MSGLOG_INFO, "  sample rate %u Hz\n",  parser_dd->sample_rate);
    msglog(NULL, MSGLOG_INFO, "  numblks     %u\n",     parser_dd->numblks);
    msglog(NULL, MSGLOG_INFO, "  %u dd frames\n",       parser_dd->dd_frame_num);
    msglog(NULL, MSGLOG_INFO, "  %u mp4 samples\n",     parser_dd->mp4_sample_num);
    msglog(NULL, MSGLOG_INFO, "  %u indep streams:\n",  get_ind_subs_num(parser_dd));

    for (prg = 0; prg < EC3_MAX_STREAMS; prg++)
    {
        psub = &(parser_dd->subs_ind[prg]);
        if (psub->ddt == DD_TYPE_NONE)
        {
            break;
        }

        msglog(NULL, MSGLOG_INFO, "  program %u\n", prg);
        msglog(NULL, MSGLOG_INFO, psub->ddt == DD_TYPE_AC3 ?
                                "  Dolby Digital stream:\n" : "  Dolby Digital PLUS stream:\n");

        msglog(NULL, MSGLOG_INFO, "    %u channels [ ", parser_dd->nfchans_prg[prg]);
        channel_flags = parser_dd->channel_flags_prg[prg];
        for (j = 0; j < 16; j++)
        {
            if (channel_flags & (1 << j))
            {
                msglog(NULL, MSGLOG_INFO, "%s ", channel_desc_tbl[j]);
            }
        }
        msglog(NULL, MSGLOG_INFO, "]\n");

        msglog(NULL, MSGLOG_INFO, "    + Indep stream %u\n", prg);
        show_substream_info(psub);

        msglog(NULL, MSGLOG_INFO, "      %u dep stream\n", get_dep_subs_num(parser_dd, prg));
        for (j = 0; j < EC3_MAX_SUBSTREAMS; j++)
        {
            psub = &(parser_dd->subs[prg][j]);
            if (psub->ddt == DD_TYPE_NONE)
            {
                break;
            }
            msglog(NULL, MSGLOG_INFO, "      + substream      %u\n", j);
            show_substream_info(psub);
        }
    }
}

static void
dd_close(parser_handle_t parser)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;

    if (!parser_dd->sample_buf_alloc_only && parser_dd->sample_buf)
    {
        MEM_FREE_AND_NULL(parser_dd->sample_buf);
    }

    /* dd has a static ec3_specific static dsi used for parsing */
}

static void
parser_dd_destroy(parser_handle_t parser)
{
    dd_close(parser);
    parser_destroy(parser);
}

static int
parser_dd_init(parser_handle_t parser, ext_timing_info_t *ext_timing, uint32_t es_idx,  bbio_handle_t ds)
{
    parser_dd_handle_t parser_dd = (parser_dd_handle_t)parser;

    parser->ext_timing = *ext_timing;
    parser->es_idx     = es_idx;
    parser->ds         = ds;

    /* pre-alloc max substream frame size + max header size upto bsmod */
    parser_dd->sample_buf_size = 4096; /* not include that for the next header */
    parser_dd->sample_buf      = MALLOC_CHK(parser_dd->sample_buf_size +
                                            parser_dd->sample_pre_read_size);
    if (parser_dd->sample_buf)
    {
        return EMA_MP4_MUXED_OK;
    }
    return EMA_MP4_MUXED_NO_MEM;
}

static parser_handle_t
parser_ac3_create(uint32_t dsi_type)
{
    parser_dd_handle_t parser_dd;

    parser_dd = (parser_dd_handle_t)MALLOC_CHK(sizeof(parser_dd_t));
    if (!parser_dd)
    {
        return 0;
    }
    memset(parser_dd, 0, sizeof(parser_dd_t));
    parser_dd->last_indep  = -1;
    parser_dd->sf_data_got = 2; /* parsing the sync frame only when sh is found */

    /**** build the interface, base for the instance */
    parser_dd->stream_type = STREAM_TYPE_AUDIO;
    parser_dd->stream_id   = STREAM_ID_AC3;
    parser_dd->stream_name = "ac3";
    parser_dd->dsi_FourCC  = "dac3";

    parser_dd->dsi_type   = dsi_type;
    parser_dd->dsi_create = dsi_ac3_create;

    parser_dd->init       = parser_dd_init;
    parser_dd->destroy    = parser_dd_destroy;
    parser_dd->get_sample = parser_dd_get_sample;
#ifdef WANT_GET_SAMPLE_PUSH
    parser_dd->get_sample_push = parser_dd_get_sample_push;
#endif
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser_dd->get_cfg = parser_ac3_get_mp4_cfg;
    }
    else if (dsi_type == DSI_TYPE_MP2TS)
    {
        parser_dd->get_cfg_len_ex = parser_ac3_get_mp2_cfg_len_ex;
        parser_dd->get_cfg_ex     = parser_ac3_get_mp2_cfg_ex;
    }
    parser_dd->get_param    = parser_dd_get_param;
    parser_dd->get_param_ex = parser_dd_get_param_ex;
    parser_dd->show_info    = parser_dd_show_info;

    /* instead of mp4_dsi_ac3_t, ac3 use ec3_sp_t in parsing ac3 file
     * ac3 dsi will be generated on demand
     */
    /* use dsi list for the sake of multiple entries of stsd */
    if (dsi_list_create((parser_handle_t)parser_dd, dsi_type))
    {
        parser_dd->destroy((parser_handle_t)parser_dd);
        return 0;
    }
    parser_dd->codec_config_lst  = list_create(sizeof(codec_config_t));
    parser_dd->curr_codec_config = NULL;
    if (!parser_dd->codec_config_lst)
    {
        parser_dd->destroy((parser_handle_t)parser_dd);
        return 0;
    }

    /**** ac3 only */
    parser_dd->sample_pre_read_size = 16;
    /****** to support push mode parser */
    parser_dd->sf_pre_buf_num       = 6;

    /**** cast to base */
    return (parser_handle_t)parser_dd;
}

static parser_handle_t
parser_ec3_create(uint32_t dsi_type)
{
    parser_dd_handle_t parser_dd;

    parser_dd = (parser_dd_handle_t)MALLOC_CHK(sizeof(parser_dd_t));
    if (!parser_dd)
    {
        return 0;
    }
    memset(parser_dd, 0, sizeof(parser_dd_t));
    parser_dd->last_indep  = -1;
    parser_dd->sf_data_got = 2; /* parsing the sync frame only when sh is found */

    /**** build the interface, base for the instance */
    parser_dd->stream_type = STREAM_TYPE_AUDIO;
    parser_dd->stream_id   = STREAM_ID_EC3;
    parser_dd->stream_name = "ec3";
    parser_dd->dsi_FourCC  = "dec3";

    parser_dd->dsi_type   = dsi_type;
    parser_dd->dsi_create = dsi_ec3_create;

    parser_dd->init       = parser_dd_init;
    parser_dd->destroy    = parser_dd_destroy;
    parser_dd->get_sample = parser_dd_get_sample;
#ifdef WANT_GET_SAMPLE_PUSH
    parser_dd->get_sample_push = parser_dd_get_sample_push;
#endif
    if (dsi_type == DSI_TYPE_MP4FF)
    {
        parser_dd->get_cfg = parser_ec3_get_mp4_cfg;
    }
    else if (dsi_type == DSI_TYPE_MP2TS)
    {
        parser_dd->get_cfg_len_ex = parser_ac3_get_mp2_cfg_len_ex;
        parser_dd->get_cfg_ex     = parser_ac3_get_mp2_cfg_ex;
    }
    else if (dsi_type == DSI_TYPE_CFF)
    {
        parser_dd->get_cfg = parser_ec3_get_uv_cfg;
    }

    parser_dd->get_param    = parser_dd_get_param;
    parser_dd->get_param_ex = parser_dd_get_param_ex;
    parser_dd->show_info    = parser_dd_show_info;

    /* instead of mp4_dsi_ec3_t, ec3 use ec3_sp_t in parsing ec3 file
     * ec3 dsi will be generated on demand
     */
    /* use dsi list for the sake of multiple entries of stsd */
    if (dsi_list_create((parser_handle_t)parser_dd, dsi_type))
    {
        parser_dd->destroy((parser_handle_t)parser_dd);
        return 0;
    }
    parser_dd->codec_config_lst  = list_create(sizeof(codec_config_t));
    parser_dd->curr_codec_config = NULL;
    if (!parser_dd->codec_config_lst)
    {
        parser_dd->destroy((parser_handle_t)parser_dd);
        return 0;
    }

    /**** ec3 only */
    parser_dd->sample_pre_read_size = 64;
    /****** to support push mode parser */
    parser_dd->sf_pre_buf_num       = 32;

    /**** cast to base */
    return (parser_handle_t)parser_dd;
}

void
parser_ac3_reg(void)
{
    reg_parser_set("ac3", parser_ac3_create);
}

void
parser_ec3_reg(void)
{
    reg_parser_set("ec3", parser_ec3_create);
}

static void parser_ec3_check_ccff_conformance(parser_dd_handle_t parser_dd)
{
    if (!parser_dd->reporter)
    {
        return;
    }

    _REPORT(REPORT_LEVEL_INFO, "EC-3: Validating number of independent substreams. Expecting 1.");
    if (parser_dd->num_ind_sub == 0 && parser_dd->subs_ind[0].ddt == DD_TYPE_EC3)
    {
        uint32_t datarate = parser_dd->subs_ind[0].data_rate;

        _REPORT(REPORT_LEVEL_INFO, "EC-3 (ind_subs=0): Validating substreamid. Expecting 0.");
        /* implementation basically prohibits anything else than substreamid=0 for first independent substream, no test needed */

        _REPORT(REPORT_LEVEL_INFO, "EC-3 (ind_subs=0): Validating sample rate. Expecting 48000.");
        if (parser_dd->subs_ind[0].fscod != 0)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3 (ind_subs=0): Wrong sample rate.");
        }

        _REPORT(REPORT_LEVEL_INFO, "EC-3 (ind_subs=0): Validating acmod is not 0x0 (dual-mono).");
        if (parser_dd->subs_ind[0].acmod == 0x0)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3 (ind_subs=0): Wrong acmod. Dual-mono not supported.");
        }

        _REPORT(REPORT_LEVEL_INFO, "EC-3 (ind_subs=0): Validating bsid. Expecting 16.");
        if (parser_dd->subs_ind[0].bsid != 16)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3 (ind_subs=0): Wrong bsid.");
        }

        _REPORT(REPORT_LEVEL_INFO, "EC-3 (ind_subs=0): Validating strmtyp. Expecting 0x0.");
        if (parser_dd->subs_ind[0].strmtyp != 0x0)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3 (ind_subs=0): Wrong strmtyp.");
        }

        _REPORT(REPORT_LEVEL_INFO, "EC-3: Validating number of dependent substreams. Expecting 0 or 1.");
        if (parser_dd->last_dep == 0 && parser_dd->subs[0][0].ddt == DD_TYPE_EC3)
        {
            _REPORT(REPORT_LEVEL_INFO, "EC-3: Found 1 dependent substream.");
            datarate += parser_dd->subs[0][0].data_rate;

            _REPORT(REPORT_LEVEL_INFO, "EC-3 (dep_subs=0): Validating substreamid. Expecting 0.");
            /* implementation prohibits anything else than substreamid=0 for first dependent substream, no test needed */

            _REPORT(REPORT_LEVEL_INFO, "EC-3 (dep_subs=0): Validating sample rate. Expecting 48000.");
            if (parser_dd->subs[0][0].fscod != 0)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3 (dep_subs=0): Wrong sample rate.");
            }

            _REPORT(REPORT_LEVEL_INFO, "EC-3 (dep_subs=0): Validating acmod is not 0x0 (dual-mono).");
            if (parser_dd->subs[0][0].acmod == 0x0)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3 (dep_subs=0): Wrong acmod. Dual-mono not supported.");
            }

            _REPORT(REPORT_LEVEL_INFO, "EC-3 (dep_subs=0): Validating bsid. Expecting 16.");
            if (parser_dd->subs[0][0].bsid != 16)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3 (dep_subs=0): Wrong bsid.");
            }

            _REPORT(REPORT_LEVEL_INFO, "EC-3 (dep_subs=0): Validating strmtyp. Expecting 0x1.");
            if (parser_dd->subs[0][0].strmtyp != 0x1)
            {
                _REPORT(REPORT_LEVEL_WARN, "EC-3 (dep_subs=0): Wrong strmtyp.");
            }

            if (IS_FOURCC_EQUAL(parser_dd->conformance_type, "cffs"))
            {
                _REPORT(REPORT_LEVEL_INFO, "EC-3: Validating channel mode. Expecting max 5.1.");
                if (parser_dd->nfchans_prg[0] > 6)
                {
                    _REPORT(REPORT_LEVEL_WARN, "EC-3: Wrong channel mode.");
                }
            }
            else if (IS_FOURCC_EQUAL(parser_dd->conformance_type, "cffh"))
            {
                _REPORT(REPORT_LEVEL_INFO, "EC-3: Validating channel mode. Expecting max 7.1.");
                if (parser_dd->nfchans_prg[0] > 8)
                {
                    _REPORT(REPORT_LEVEL_WARN, "EC-3: Wrong channel mode.");
                }
            }
        }
        else if (parser_dd->last_dep > 0)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Too many dependent substreams found.");
        }

        _REPORT(REPORT_LEVEL_INFO, "EC-3: Validating data rate. Expecting value between 32 and 3024 kbps.");
        if (datarate < 32)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Data rate below min limit.");
        }
        else if (datarate > 3024)
        {
            _REPORT(REPORT_LEVEL_WARN, "EC-3: Data rate above max limit.");
        }
    }
    else if (parser_dd->num_ind_sub > 0)
    {
        _REPORT(REPORT_LEVEL_WARN, "EC-3: Too many independent substreams found.");
    }
    else if (parser_dd->subs_ind[0].ddt == DD_TYPE_AC3)
    {
        _REPORT(REPORT_LEVEL_INFO, "AC-3: Validating sample rate. Expecting 48000.");
        if (parser_dd->subs_ind[AC3_SUBSTREAMID].fscod != 0)
        {
            _REPORT(REPORT_LEVEL_WARN, "AC-3: Wrong sample rate.");
        }

        _REPORT(REPORT_LEVEL_INFO, "AC-3: Validating acmod is not 0x0 (dual-mono).");
        if (parser_dd->subs_ind[AC3_SUBSTREAMID].acmod == 0x0)
        {
            _REPORT(REPORT_LEVEL_WARN, "AC-3: Wrong acmod. Dual-mono not supported.");
        }

        _REPORT(REPORT_LEVEL_INFO, "AC-3: Validating bsid. Expecting 8 or 6.");
        if (parser_dd->subs_ind[AC3_SUBSTREAMID].bsid != 8 && parser_dd->subs_ind[AC3_SUBSTREAMID].bsid != 6)
        {
            _REPORT(REPORT_LEVEL_WARN, "AC-3: Wrong bsid.");
        }

        _REPORT(REPORT_LEVEL_INFO, "AC-3: Validating frmsizecod. Expecting between 64 and 640kbps.");
        if (parser_dd->subs_ind[AC3_SUBSTREAMID].data_rate < 64 || parser_dd->subs_ind[AC3_SUBSTREAMID].data_rate > 640)
        {
            _REPORT(REPORT_LEVEL_WARN, "AC-3: Bad frmsizecod.");
        }
    }
}
