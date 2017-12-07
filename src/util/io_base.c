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
    @file io_base.c
    @brief Implements base methods for I/O
*/

#include <assert.h>   /** assert() */
#include <stdio.h>    /** SEEK_CUR */

#include "io_base.h"

/*
 * helper functions to avoid direct function pointer usage
 */
void
bbio_call_destroy(bbio_handle_t bbio)
{
    bbio->destroy(bbio);
}

int32_t
bbio_call_open(bbio_handle_t bbio, const int8_t* dev_name)
{
    return bbio->open(bbio, dev_name);
}

/*************************** write "class method" ****************************/
void
sink_write_u8(bbio_handle_t sink, uint8_t u8)
{
    sink->write(sink, &u8, 1);
}

void
sink_write_u16(bbio_handle_t sink, uint16_t u16)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(u16 >> 8);
    buf[1] = (uint8_t)(u16);

    sink->write(sink, buf, 2);
}

void
sink_write_u32(bbio_handle_t sink, uint32_t u32)
{
    uint8_t buf[4];

    buf[0] = (uint8_t)(u32 >> 24);
    buf[1] = (uint8_t)(u32 >> 16);
    buf[2] = (uint8_t)(u32 >> 8);
    buf[3] = (uint8_t)(u32);

    sink->write(sink, buf, 4);
}

void
sink_write_u64(bbio_handle_t sink, uint64_t u64)
{
    uint8_t buf[8];

    buf[0] = (uint8_t)(u64 >> 56);
    buf[1] = (uint8_t)(u64 >> 48);
    buf[2] = (uint8_t)(u64 >> 40);
    buf[3] = (uint8_t)(u64 >> 32);
    buf[4] = (uint8_t)(u64 >> 24);
    buf[5] = (uint8_t)(u64 >> 16);
    buf[6] = (uint8_t)(u64 >> 8);
    buf[7] = (uint8_t)(u64);

    sink->write(sink, buf, 8);
}

void
sink_write_4CC(bbio_handle_t sink, const int8_t *cc)
{
    assert(strlen(cc) == 4);
    sink->write(sink, (uint8_t *)cc, 4);
}

/** A bit interface, designed for efficiency 
 * use byte interface when in byte aligned position
 * in 'w' op:
 * cached_bit_num is the number of bits that is cached to output to byte interface
 * when accumulated to 8 bits. The cached_bits is the bits cached in BE
 */
static const int32_t byte_bit_mask[] = {0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};
void
sink_write_bits(bbio_handle_t sink, uint32_t bit_num, uint32_t val)
{
    uint32_t output_bit_num;
    uint8_t u8;

    do
    {
        if (sink->cached_bit_num + bit_num <= 8)
        {
            sink->cached_bits = (sink->cached_bits << bit_num) | (val & byte_bit_mask[bit_num]);
            sink->cached_bit_num += bit_num;

            if (sink->cached_bit_num == 8)  /* => cached_bit_num < 8 */
            {
                u8 = (uint8_t)(sink->cached_bits);
                sink->write(sink, &u8, 1);

                sink->cached_bit_num = 0;
            }
            return;
        }

        /** consume as much input bit_num as possible(up to 8 bit_num a time) */
        output_bit_num = 8 - sink->cached_bit_num;
        sink->cached_bits = (sink->cached_bits << output_bit_num) |
                            ((val>>(bit_num - output_bit_num)) & byte_bit_mask[output_bit_num]);
        bit_num -= output_bit_num;  /** > 0 * since it failes if test */

        u8 = (uint8_t)(sink->cached_bits);
        sink->write(sink, &u8, 1);

        sink->cached_bit_num = 0;
    }
    while (1);
}

void
sink_write_bit(bbio_handle_t sink, uint32_t val)
{
    sink_write_bits(sink, 1, val);
}

/** flush with 0 padding if necessary */
void
sink_flush_bits(bbio_handle_t sink)
{
    if (sink->cached_bit_num)
    {
        sink_write_bits(sink, 8 - sink->cached_bit_num, 0);
    }
}
/*************************** End of write "class method" ****************************/


/*************************** read "class method" ****************************/
uint8_t src_read_u8(bbio_handle_t src)
{
    uint8_t u;
    (void) src_rd_u8(src, &u);
    return u;
}
uint16_t src_read_u16(bbio_handle_t src)
{
    uint16_t u;
    (void) src_rd_u16(src, &u);
    return u;
}
uint32_t src_read_u24(bbio_handle_t src)
{
    uint32_t u;
    (void) src_rd_u24(src, &u);
    return u;
}
uint32_t src_read_u32(bbio_handle_t src)
{
    uint32_t u;
    
    (void) src_rd_u32(src, &u);
    return u;
}
uint64_t src_read_u64(bbio_handle_t src)
{
    uint64_t u;
    (void) src_rd_u64(src, &u);
    return u;
}

int32_t
src_rd_u8(bbio_handle_t src, uint8_t *u8)
{
    assert(u8 != NULL);

    if (src->read(src, u8, 1) != 1)
    {
        return 1;
    }

    return 0;
}

int32_t
src_rd_u16(bbio_handle_t src, uint16_t *u16)
{
    uint8_t buf[2];

    assert(u16 != NULL);

    if (src->read(src, buf, 2) != 2)
    {
        *u16 = 0;

        return 1;
    }

    *u16 = (((uint16_t)buf[0])<<8) | buf[1];

    return 0;
}

int32_t
src_rd_u24(bbio_handle_t src, uint32_t *u32)
{
    uint8_t buf[3];

    assert(u32 != NULL);

    if (src->read(src, buf, 3) != 3)
    {
        *u32 = 0;

        return 1;
    }

    *u32 = (((uint32_t)buf[0])<<16) | (((uint32_t)buf[1])<< 8) | buf[2];

    return 0;
}

int32_t
src_rd_u32(bbio_handle_t src, uint32_t *u32)
{
    uint8_t buf[4];

    assert(u32 != NULL);

    if (src->read(src, buf, 4) != 4)
    {
        *u32 = 0;

        return 1;
    }

    *u32 =
      (((uint32_t)buf[0])<<24) | (((uint32_t)buf[1])<<16) |
      (((uint32_t)buf[2])<< 8) | buf[3];

    return 0;
}

int32_t
src_rd_u64(bbio_handle_t src, uint64_t *u64)
{
    uint8_t buf[8];

    assert(u64 != NULL);

    if (src->read(src, buf, 8) != 8)
    {
        *u64 = 0;

        return 1;
    }

    *u64 = (((uint64_t)buf[0])<<56) | (((uint64_t)buf[1])<<48) |
           (((uint64_t)buf[2])<<40) | (((uint64_t)buf[3])<<32) |
           (((uint32_t)buf[4])<<24) | (((uint32_t)buf[5])<<16) |
           (((uint32_t)buf[6])<< 8) | buf[7];

    return 0;
}

/** A bit interface, designed for efficiency */

/** use byte interface when in byte aligned position
 * in 'r' op:
 * cached_bit_num is the number of bits that is cached and available to read
 * when the cached bits is less than needed, read a byte from byte interface
 * the cached byte never changes and is output in BE order
 */
uint32_t
src_read_bits(bbio_handle_t src, uint32_t bit_num)
{
    uint32_t u32;
    assert(src->cached_bit_num <= 8);

    if (src->cached_bit_num >= bit_num)
    {
        u32 = (src->cached_bits >> (src->cached_bit_num - bit_num)) & byte_bit_mask[bit_num];
        src->cached_bit_num -= bit_num;

        return u32;
    }

    u32 = 0;
    do
    {
        if (!src->cached_bit_num)
        {
            uint8_t u8 = 0;

            src->read(src, &u8, 1);
            src->cached_bits    = u8;
            src->cached_bit_num = 8;
        }

        if (src->cached_bit_num >= bit_num)
        {
            u32 = (u32 << bit_num) |
                  ((src->cached_bits >> (src->cached_bit_num - bit_num)) & byte_bit_mask[bit_num]);

            src->cached_bit_num -= bit_num;
            return u32;
        }

        /** get from all cached bits */
        u32 = (u32 << src->cached_bit_num) | (src->cached_bits & byte_bit_mask[src->cached_bit_num]);

        bit_num -= src->cached_bit_num;  /** >0 since it failes the if test: =>  cached_bit_num < 8 */
        src->cached_bit_num = 0;
    }
    while (1);
}

uint32_t
src_read_bit(bbio_handle_t src)
{
    return src_read_bits(src, 1);
}

/** to the next closest byte aligned position:
 * current byte position and ignore the cached bits
 */
void
src_byte_align(bbio_handle_t src)
{
    /** so the next read bits will read form byte interface => next byte position */
    src->cached_bit_num = 0;
}

/** if offset != 0 => peek at next 'offset' byte for bit_num,
 * else peek the bit_num immediately following current bit position
 */
uint32_t
src_peek_bits(bbio_handle_t src, uint32_t bit_num, int32_t offset)
{
    uint32_t u32;

    int64_t  pos_cur        = src->position(src);
    uint32_t cached_bit_num = src->cached_bit_num;
    uint32_t cached_bits    = src->cached_bits;


    if (bit_num > src_following_bit_num(src))
    {
        return (uint32_t)-1;
    }

    if (offset)
    {
        src->cached_bit_num = 0;           /** align */
        src->seek(src, offset, SEEK_CUR);
    }

    u32 = src_read_bits(src, bit_num);

    src->seek(src, pos_cur, SEEK_SET);
    src->cached_bit_num = cached_bit_num;
    src->cached_bits    = cached_bits;

    return u32;
}

uint32_t
src_bits_cached(bbio_handle_t src)
{
    return src->cached_bit_num;
}

void
src_skip_bits(bbio_handle_t src, uint32_t bit_num)
{
    if (bit_num <= src->cached_bit_num)
    {
        src->cached_bit_num -= bit_num;
    }
    else
    {
        /** try to use byte interface as much as possible */
        int32_t offset;

        /** dumps the cached bits: byte interface will not change */
        bit_num -= src->cached_bit_num;
        src->cached_bit_num = 0;

        /** dump the bytes */
        offset = (int32_t)(bit_num >> 3);
        if (offset)
        {
            src->seek(src, offset, SEEK_CUR);
            bit_num -= (offset << 3);
        }

        /** dump bits */
        if (bit_num)
        {
            src_read_bits(src, bit_num);
        }
    }
}

int64_t
src_following_bit_num(bbio_handle_t src)
{
    return src->cached_bit_num + ((src->size(src) - src->position(src))<<3);
}

/** Copies data of specified size from src to snk. */
void
bbio_copy(bbio_handle_t snk, bbio_handle_t src, uint64_t size)
{
#define CP_BUF_SIZE 4096
    uint8_t cp_buf[CP_BUF_SIZE];
    size_t  cp_size, ret_size;

    while (size)
    {
        cp_size  = (size > CP_BUF_SIZE) ? CP_BUF_SIZE : (size_t)size;
        ret_size = src->read(src, cp_buf, cp_size);
        assert(ret_size == cp_size);

        ret_size = snk->write(snk, cp_buf, cp_size);
        assert(ret_size == cp_size);

        size -= cp_size;
    }
#ifdef NDEBUG
    (void)ret_size;  /* avoid compiler warning */
#endif
}

/*************************** End of read "class method" ****************************/
