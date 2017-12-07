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
    @file mp4_isom.c
    @brief Defines stream type, stream ID, and language mapping as specified in ISO 639.

*/

#include "utils.h"
#include "mp4_isom.h"
#include "parser.h"


/** http://www.mp4ra.org */
/** ordered by muxing preference */
const stream_id_tag_map_t stream_id_objectTypeIndication_tbl[] = {
    {STREAM_ID_H264, MP4_OT_VISUAL_H264},                            /** 14496-10 */
    {STREAM_ID_MP4V, MP4_OT_VISUAL_14492_2},                         /** 14496-2 */
    {STREAM_ID_AAC,  MP4_OT_AUDIO_14496_3},                          /** 14496-3 */
    {STREAM_ID_AAC,  MP4_OT_AUDIO_13818_7_MAIN_PROFILE},             /** 13818-7 Main Profile */
    {STREAM_ID_AAC,  MP4_OT_AUDIO_13818_7_LOW_COMPLEXITY},           /** 13818-7 LowComplexity Profile */
    {STREAM_ID_AAC,  MP4_OT_AUDIO_13818_7_SCALEABLE_SAMPLING_RATE},  /** 13818-7 Scalable Sampling Rate Profile */
    {STREAM_ID_UNKNOWN, MP4_OT_FORBIDDEN},
};

const stream_id_codingname_map_t stream_id_video_codingname_tbl[] = {
    {STREAM_ID_H264,    "avc1"},     /** AVC-1/H.264 */
    {STREAM_ID_H264,    "AVC1"},     /** AVC-1/H.264 */
    {STREAM_ID_H264,    "h264"},     /** AVC-1/H.264 */
    {STREAM_ID_H264,    "264"},      /** AVC-1/H.264 */
    {STREAM_ID_H264,    "H264"},     /** AVC-1/H.264 */
    {STREAM_ID_VC1,     "vc-1"},     /** don't know yet vc1 */
    {STREAM_ID_VC1,     "VC-1"},     /** don't know yet vc1 */
    {STREAM_ID_H263,    "s263"},     /** s263/H.263 */
    {STREAM_ID_H263,    "S263"},     /** s263/H.263 */
    {STREAM_ID_H263,    "h263"},     /** s263/H.263 */
    {STREAM_ID_H263,    "H263"},     /** s263/H.263 */
    {STREAM_ID_H263,    "I263"},     /** s263/H.263 */
    {STREAM_ID_H263,    "263"},      /** s263/H.263 */
    {STREAM_ID_YUV420P, "I420"},     /** I420 - Intel Indeo 4 H.263 or YUV 4:2:0 */
    {STREAM_ID_YUV420P, "IYUV"},     /** YUV 4:2:0 */
    {STREAM_ID_MP4V,    "mp4v"},     /** mp4v */
    {STREAM_ID_MP4V,    "MP4V"},     /** mp4v */
    {STREAM_ID_MP4V,    "MP42"},     /** mp4v */
    {STREAM_ID_MP4V,    "MP43"},     /** mp4v */
    {STREAM_ID_MP4V,    "MP4S"},     /** mp4v */
    {STREAM_ID_MP4V,    "xvid"},     /** mp4v */
    {STREAM_ID_MP4V,    "XVID"},     /** mp4v */
    {STREAM_ID_MP4V,    "FMP4"},     /** mp4v */
    {STREAM_ID_MP4V,    "divx"},     /** mp4v */
    {STREAM_ID_MP4V,    "DIVX"},     /** mp4v */
    {STREAM_ID_MP4V,    "DX50"},     /** mp4v */
    {STREAM_ID_MP4V,    "cmp"},      /** mp4v */
    {STREAM_ID_QTVIDEO, "UYVY"},     /** UYVY */
    {STREAM_ID_QTVIDEO, "2vuy"},     /** UYVY */
    {STREAM_ID_QTVIDEO, "\0\0\0\0"}, /** RGB */
    {STREAM_ID_QTVIDEO, "j420"},     /** Apple YUV420 codec */
    {STREAM_ID_QTVIDEO, "yuv2"},     /** Apple YUV422 codec */
    {STREAM_ID_QTVIDEO, "8BPS"},
    {STREAM_ID_QTVIDEO, "8bps"},
    {STREAM_ID_QTVIDEO, "rle "},
    {STREAM_ID_QTVIDEO, "raw "},
    {STREAM_ID_QTVIDEO, "rpza"},
    {STREAM_ID_QTVIDEO, "dvsd"},
    {STREAM_ID_QTVIDEO, "dvcp"},
    {STREAM_ID_QTVIDEO, "210"},
    {STREAM_ID_QTVIDEO, "msvc"},     /** Microsoft Video One */
    {STREAM_ID_QTVIDEO, "CRAM"},
    {STREAM_ID_QTVIDEO, "WHAM"},
    {STREAM_ID_QTVIDEO, "v210"},     /** 32bpp 10-bit 4:2:2 YCrCb equivalent to the Quicktime format of the same name. */
    {STREAM_ID_QTVIDEO, "cvid"},
    {STREAM_ID_QTVIDEO, "mjpa"},     /** apple motion jpeg a */
    {STREAM_ID_QTVIDEO, "mjpb"},     /** apple motion jpeg b */
    {STREAM_ID_QTVIDEO, "mjp2"},     /** apple jpeg 2000 */
    {STREAM_ID_QTVIDEO, "jpeg"},     /** apple jpeg compressor */
    {STREAM_ID_QTVIDEO, "mjpg"},
    {STREAM_ID_QTVIDEO, "ap4h"},     /** Apple ProRes 4444 */
    {STREAM_ID_QTVIDEO, "apch"},     /** Apple ProRes 422 (HQ) */
    {STREAM_ID_QTVIDEO, "apcn"},     /** Apple ProRes 422 */
    {STREAM_ID_QTVIDEO, "apco"},     /** Apple ProRes 422 (Proxy) */
    {STREAM_ID_QTVIDEO, "apcs"},     /** Apple ProRes 422 (LT) */
    {STREAM_ID_QTVIDEO, "pxlt"},     /** Apple Pixlet */
    {STREAM_ID_QTVIDEO, "icod"},     /** Apple Intermediate Codec */
    {STREAM_ID_QTVIDEO, "AV1x"},     /** Avid 1:1 */
    {STREAM_ID_QTVIDEO, "AVDJ"},     /** Avid Meridien Compressed */
    {STREAM_ID_QTVIDEO, "AVUI"},     /** Avid Meridien Uncompressed */
    {STREAM_ID_QTVIDEO, "AVd1"},     /** Avid DV100 Codec */
    {STREAM_ID_QTVIDEO, "AVdn"},     /** Avid DNxHD Codec */
    {STREAM_ID_QTVIDEO, "AVdv"},     /** Avid DV Codec */
    {STREAM_ID_QTVIDEO, "AVup"},     /** Avid Packed Codec */
    {STREAM_ID_MPG2,    "mpg2"},
    {STREAM_ID_ENCRYPTED_VIDEO, "encv"}, /**< generic encrypted video */

    {STREAM_ID_UNKNOWN, 0},
};



const stream_id_codingname_map_t stream_id_audio_codingname_tbl[] = {
    {STREAM_ID_AAC,     "mp4a"},  /** MPEG-4 AAC */
    {STREAM_ID_AC3,     "ac-3"},  /** Dolby AC3 */
    {STREAM_ID_EC3,     "ec-3"},  /** Dolby EC3 */
    {STREAM_ID_AAC,     "aac"},   /** MPEG-4 AAC */
    {STREAM_ID_EC3,     "ec3"},   /** Dolby EC3 */
    {STREAM_ID_AC3,     "ac3"},   /** Dolby AC3*/
    {STREAM_ID_MLP,     "mlpa"},  /** Dolby MLP */
    {STREAM_ID_MP3,     "mp3"},
    {STREAM_ID_MP2,     "mp2"},
    {STREAM_ID_DTS,     "dtsc"},  /** DTS */
    {STREAM_ID_DTS,     "dtsh"},  /** DTS */
    {STREAM_ID_DTS,     "dtse"},  /** DTS */
    {STREAM_ID_DTS,     "dtsl"},  /** DTS */
    {STREAM_ID_QTAUDIO, "lpcm"},
    {STREAM_ID_QTAUDIO, "raw "},
    {STREAM_ID_QTAUDIO, "twos"},
    {STREAM_ID_QTAUDIO, "sowt"},
    {STREAM_ID_QTAUDIO, "in24"},
    {STREAM_ID_QTAUDIO, "in32"},
    {STREAM_ID_QTAUDIO, "fl32"},
    {STREAM_ID_QTAUDIO, "fl64"},
    {STREAM_ID_QTAUDIO, "ulaw"},
    {STREAM_ID_QTAUDIO, "alaw"},
    {STREAM_ID_ENCRYPTED_AUDIO, "enca"}, /**< generic encrypted audio */

    {STREAM_ID_UNKNOWN, 0},
};


const stream_id_codingname_map_t stream_id_metadata_codingname_tbl[] = {

    { STREAM_ID_METX, "metx" },
    { STREAM_ID_METT, "mett" },

    {STREAM_ID_UNKNOWN, 0},
};


static const char *movie_mdhd_language_tab[] = {
    "eng", "fra", "ger", "ita", "dut", "sve", "spa", "dan", "por", "nor",
    "heb", "jpn", "ara", "fin", "gre", "ice", "mlt", "tur", "hr ", "chi",
    "urd", "hin", "tha", "kor", "lit", "pol", "hun", "est", "lav", NULL,
    "fo ", NULL, "rus", "chi",  NULL, "iri",   "alb", "ron", "ces", "slk",
    "slv", "yid", "sr ", "mac", "bul", "ukr", "bel", "uzb", "kaz", "aze",
    "aze", "arm", "geo", "mol", "kir", "tgk", "tuk", "mon", NULL, "pus",
    "kur", "kas", "snd", "tib", "nep", "san", "mar", "ben", "asm", "guj",
    "pa ", "ori", "mal", "kan", "tam", "tel", NULL,  "bur", "khm", "lao",
    "vie", "ind", "tgl", "may", "may", "amh", "tir", "orm", "som", "swa",
    NULL, "run",   NULL, "mlg", "epo",  NULL,  NULL,  NULL,  NULL,  NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,   NULL,  NULL,  NULL,
    NULL, NULL,  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL,  NULL, NULL, NULL, NULL, NULL, NULL, "wel", "baq",
    "cat", "lat", "que", "grn", "aym", "tat", "uig", "dzo", "jav"
};



uint16_t
movie_language_to_iso639 (uint16_t input, uint8_t *language)
{
    if (input > 0x8a)
    {
        language[0] = (uint8_t)((0x60 + (input & 0x7c00)) >> 10); 
        language[1] = (uint8_t)((0x60 + (input & 0x3e0)) >> 5) ; 
        language[2] = (uint8_t)(0x60 + (input & 0x1f)); 

        return 1;
    }

    if (!movie_mdhd_language_tab[input])
    {
        return 0;
    }

    OSAL_STRNCPY((char *)language, 4, movie_mdhd_language_tab[input], 4);

    return 1;
}

uint16_t
movie_iso639_to_language (const int8_t *language)
{
    int32_t i;
    uint16_t output = 0;

    /** if it's empty, it's undefined. */
    if (language[0] == '\0')
    {
        language = "und";
    }

    for (i = 0; i < 3; i++)
    {
        if (((uint8_t)language[i] < 0x60) || ((uint8_t)language[i] > 0x60 + 0x1f))
        {
            return 0;
        }
    }

    output |= ((language[0] - 0x60) << 10);
    output |= ((language[1] - 0x60) << 5);
    output |=  (language[2] - 0x60);

    return output;
}


stream_id_t
get_stream_id(const stream_id_codingname_map_t *map, const int8_t *codingname)
{
    while (map->stream_id != STREAM_ID_UNKNOWN && !IS_FOURCC_EQUAL(map->codingname, codingname))
    {
        map++;
    }

    return map->stream_id;
}



int8_t *
get_stream_codingname(const stream_id_codingname_map_t *map, const int8_t *codingname)
{
    while (map->stream_id != STREAM_ID_UNKNOWN && !IS_FOURCC_EQUAL(map->codingname, codingname))
    {
        map++;
    }

    return map->codingname;
}



uint32_t
get_objectTypeIndication(const stream_id_tag_map_t *map, uint32_t stream_id)
{
    while (map->stream_id != STREAM_ID_UNKNOWN && map->stream_id != stream_id)
    {
        map++;
    }

    return map->tag;
}

