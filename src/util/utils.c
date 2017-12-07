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
    @file utils.c
    @brief Implements types, OS abstration layer. Inlcudes common header file.

*/

#ifndef _MSC_VER
#include <sys/time.h>   /* for gettimeofday() */
#else
#include <sys/timeb.h>  /* for _ftime64_s() */
#include <windows.h>
#endif

#include "utils.h"
#include "memory_chk.h"

/**************** read in BE value *********************/
uint32_t get_BE_u16(const uint8_t* bytes)
{
    return (bytes[0] << 8) | bytes[1];
}

uint32_t get_BE_u32(const uint8_t* bytes)
{
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8)  |bytes[3];
}

uint64_t get_BE_u64(const uint8_t* bytes)
{
    return ((uint64_t)get_BE_u32(bytes))<< 32 | get_BE_u32(bytes + 4);
}
/**************** End of read in BE value *********************/

int64_t
utc_sec_since_1970(void)
{
#ifdef _MSC_VER
    struct __timeb64 tb;

    _ftime64_s(&tb);
    return tb.time;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec;
#endif
}

/** dump indicator to show progress */

static void
progress_destroy(progress_handle_t h)
{
    if (h)
    {
        if (h->caption)
        {
            FREE_CHK(h->caption);
        }
        FREE_CHK(h);
    }
}

static void
progress_show(progress_handle_t h, int64_t size_done)
{
    static const char *indicators[21] =
    {
        "                    ",
        "*                   ",
        "**                  ",
        "***                 ",
        "****                ",
        "*****               ",
        "******              ",
        "*******             ",
        "********            ",
        "*********           ",
        "**********          ",
        "***********         ",
        "************        ",
        "*************       ",
        "**************      ",
        "***************     ",
        "****************    ",
        "*****************   ",
        "******************  ",
        "******************* ",
        "********************",
    };

    if (size_done != h->size_total)
    {
        int32_t ratio;

        ratio = (int32_t)((float)(100*size_done)/(float)h->size_total);
        if (ratio > h->processed_ratio)
        {
            if (ratio > 100)
            {
                ratio = 100;
            }

            printf("\r%4s: %s %02d%%", h->caption, indicators[ratio/5], ratio);
            fflush(stdout);
            h->processed_ratio = ratio;
        }
        return;
    }

    /* so we will have a 100% */
    printf("\r%4s: %s 100%%", h->caption, indicators[20]);
    fflush(stdout);
}

progress_handle_t
progress_create(const int8_t *caption, int64_t size_total)
{
    progress_handle_t h;

    h = MALLOC_CHK(sizeof(progress_t));
    if (h)
    {
        h->caption         = (caption) ? STRDUP_CHK(caption) : STRDUP_CHK("");
        h->size_total      = size_total;
        h->processed_ratio = -1;          /* to show the first one */

        h->destroy = progress_destroy;
        h->show    = progress_show;
    }
    return h;
}

uint32_t
get_GCD(uint32_t a, uint32_t b)
{
    uint32_t r;

    if (a < b)
    {
        r = a;
        a = b;
        b = r;
    }

    while (b)
    {
        r = a % b;
        a = b;
        b = r;
    }
    return a;
}

FILE*
create_temp_file()
{
    FILE *fp = NULL;

#ifdef _MSC_VER
    CHAR szTempFileName[MAX_PATH];
    UINT uRetVal = GetTempFileName(get_temp_path(), "", 0, szTempFileName);
    if (uRetVal == 0)
        return NULL;
    fp = fopen(szTempFileName, "w+b");
    /* XXX likely to leak a file handle here */
#else
    fp = tmpfile();
#endif
    return fp;
}

int8_t *
get_temp_path()
{
#ifdef _MSC_VER
    static CHAR szTempPath[MAX_PATH];
    static int is_initialised = 0;
    if (!is_initialised) {
        DWORD dwRetVal = GetTempPathA(MAX_PATH, szTempPath);
        if (dwRetVal > MAX_PATH || dwRetVal == 0)
            return NULL;
        is_initialised = 1;
    }
    return szTempPath;
#else
    static char tmp_path[] = P_tmpdir "/";
    return tmp_path;
#endif
}

void
Bin2Hex(unsigned char* inbuf, int insize, unsigned char* outbuf)
{
    outbuf[insize * 2] = '\0';
    while(insize != 0)
    {
        outbuf[insize * 2 - 2] = inbuf[insize - 1];
        outbuf[insize * 2 - 1] = inbuf[insize - 1];
        outbuf[insize * 2 - 2] &= 0xf0;
        outbuf[insize * 2 - 1] &= 0x0f;
        outbuf[insize * 2 - 2] >>= 0x4;
        if (outbuf[insize * 2 - 2] <= 0x9)
        {
            outbuf[insize * 2 - 2] |= 0x30;
        }
        else
        {
            outbuf[insize * 2 - 2] |= 0x40;
            outbuf[insize * 2 - 2] -= 0x9;
        }
        if (outbuf[insize * 2 - 1] <= 0x9)
        {
            outbuf[insize * 2 - 1] |= 0x30;
        }
        else
        {
            outbuf[insize * 2 - 1] |= 0x40;
            outbuf[insize * 2 - 1] -= 0x9;
        }
        insize--;
    }
}

uint64_t
rescale_u64(uint64_t u64, uint32_t new_scale, uint32_t old_scale)
{
    assert(new_scale != 0);

    if (u64 <= 0xffffffff)
    {
        return (u64*new_scale + (old_scale>>1))/old_scale;
    }
    else
    {
        return (u64/old_scale)*new_scale + ((u64 % old_scale)*new_scale + (old_scale>>1))/old_scale;
    }
}
