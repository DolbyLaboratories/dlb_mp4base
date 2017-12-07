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
    @file io_buffer.c
    @brief Implements buffer I/O method
*/

#include <assert.h>      /** assert() */
#include <stdio.h>       /** SEEK_CUR */

#include "io_base.h"
#include "registry.h"
#include "msg_log.h"     /** msglog() */
#include "memory_chk.h"  /** MALLOC_CHK() */

/** since buf may be passed from out side, sink_buffer will not release buf
 *  it uses even though it may realloc or alloc the buf
 */
typedef struct bbio_buf_t_
{
    BBIO;

    /** current write status: with size and offset to support random access */
    size_t   data_size;   /**< data size in bytes. 'w': the data accumulated so far 'r': data available */
    int64_t  op_offset;   /**< next operation position */
    /** buf management */
    uint8_t *buf;        /**< the buffer */
    size_t   buf_size;   /**< 'w': the buf_size. 'r': == data_size */
    BOOL     re_al;      /**< 'w': if realloc allowed. 'r': if free buf */
} bbio_buf_t;
typedef bbio_buf_t *bbio_buf_handle_t;

static int
buf_open(bbio_handle_t bbio, const int8_t *dev_name)
{
    return EMA_MP4_MUXED_OK;
    (void)bbio;      /** avoid compiler warning */
    (void)dev_name;  /** avoid compiler warning */
}

static void
buf_close(bbio_handle_t bbio)
{
    (void)bbio;  /** avoid compiler warning */
}

static int64_t
buf_position(bbio_handle_t bbio)
{
    return ((bbio_buf_handle_t)bbio)->op_offset;
}

/** Returns offset after the seek. -1 if seek beyond buf range */
static int
buf_seek(bbio_handle_t bbio, int64_t offset, int origin)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    if (origin == SEEK_CUR)
    {
        offset += b->op_offset;
    }
    else if (origin == SEEK_END)
    {
        offset += b->data_size;
    }
    /** else offset is the one */

    if (offset < 0 || offset > (int64_t)b->buf_size)
    {
        return -1;
    }

    b->op_offset = offset;

    return 0;
}

static void
buf_set_buffer(bbio_handle_t bbio, uint8_t *buf, size_t buf_size, BOOL re_al)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    if (b->io_mode == 'w')
    {
        if (!buf)
        {
            if (buf_size)
            {
                /** know the maximum size of the buf and pre-alloc it to avoid realloc */
                buf = MALLOC_CHK(buf_size);
                if (!buf)
                {
                    /** alloc failed */
                    buf_size = 0;
                }
            }
        }
        else
        {
            if (!buf_size)
            {
                FREE_CHK(buf);
                buf = 0;
            }
        }
        /** come here: !buf && !buf_size || buf && buf_size */
        /** reset just for sure */
        b->data_size = 0;
    }
    else if (b->io_mode == 'r')
    {
        assert(buf && buf_size);
        b->data_size = buf_size;
    }

    b->buf      = buf;
    b->buf_size = buf_size;
    b->re_al    = re_al;

    b->op_offset = 0;
}

static uint8_t*
buf_get_buffer(bbio_handle_t bbio, size_t *data_size, size_t *buf_size)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;
    uint8_t *         buf;

    buf = b->buf;
    if (b->io_mode == 'w')
    {
        *data_size = b->data_size;
        if (buf_size)
        {
            *buf_size = b->buf_size;
        }

        b->buf      = 0;
        b->buf_size = 0;

        b->data_size = 0;
        b->op_offset = 0;
    }
    else if (b->io_mode == 'r')
    {
        *data_size = b->data_size - (size_t)b->op_offset;
    }

    return buf;
}

static size_t
buf_write(bbio_handle_t snk, const uint8_t *buf, size_t size)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)snk;
    int64_t           offset_new;

    offset_new = b->op_offset + size;

    /** reallocate buffer if needed */
    if ((int64_t)b->buf_size < offset_new)
    {
        if (b->re_al)
        {
            b->buf_size += 4;  /** in case buf_size = 1, 0 */
            do
            {
                int tmp = (int)b->buf_size;
                b->buf_size += (tmp>>1);
            }
            while ((int64_t)b->buf_size < offset_new);

            if (b->buf)
            {
                b->buf = REALLOC_CHK(b->buf, b->buf_size);
            }
            else
            {
                b->buf = MALLOC_CHK(b->buf_size);
            }
            if (!b->buf)
            {
                return 0;  /** EMA_MP4_MUXED_NO_MEM; */
            }
        }
        else
        {
            return 0;  /** EMA_MP4_MUXED_WRITE_ERR; */
        }
    }

    memcpy(b->buf + b->op_offset, buf, size);
    b->op_offset = offset_new;
    if ((int64_t)b->data_size < b->op_offset)
    {
        b->data_size = (size_t)b->op_offset;
    }
    return size;
}


static size_t
buf_read(bbio_handle_t src, uint8_t *buf, size_t size)
{
    bbio_buf_handle_t b       = (bbio_buf_handle_t)src;
    size_t            size2rd = size;

    if (!buf)
    {
        return 0;
    }

    if (b->op_offset + size2rd > b->data_size)
    {
        size2rd = (size_t)(b->data_size - b->op_offset);
        msglog(NULL, MSGLOG_ERR, "io_buffer: ERR: read beyond buffer limit requested. wanted: %" PRIz ", will read: %" PRIz "\n", size, size2rd);
    }

    if (!size2rd)
    {
        return 0;
    }

    memcpy(buf, b->buf + b->op_offset, size2rd);
    b->op_offset += size2rd;

    return size2rd;
}

static int64_t
data_size(bbio_handle_t bbio)
{
    return ((bbio_buf_handle_t)bbio)->data_size;
}

static BOOL
buf_is_EOD(bbio_handle_t bbio)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    return b->op_offset == (int64_t)b->data_size;
}

/** if whole byte available */
static BOOL
buf_is_more_byte(bbio_handle_t bbio)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    return (int64_t)b->data_size - b->op_offset > 0;
}

static BOOL
buf_is_more_byte2(bbio_handle_t bbio)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    return (int64_t)b->data_size - b->op_offset > 1;
}

static int
buf_skip_bytes(bbio_handle_t bbio, int64_t byte_num)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    b->op_offset += byte_num;
    if (b->op_offset > (int64_t)b->data_size)
    {
        b->op_offset = b->data_size;
    }
    return 0;
}

static void
buf_destroy(bbio_handle_t bbio)
{
    bbio_buf_handle_t b = (bbio_buf_handle_t)bbio;

    if (b->io_mode == 'w')
    {
        if (b->buf)
        {
            FREE_CHK(b->buf);
        }
    }
    else if (b->io_mode == 'r')
    {
        if (b->re_al && b->buf)
        {
            FREE_CHK(b->buf);
        }
    }

    FREE_CHK(b);
}

static bbio_handle_t
buf_create(int8_t io_mode)
{
    bbio_buf_handle_t b;

    b = (bbio_buf_handle_t)MALLOC_CHK(sizeof(bbio_buf_t));
    if (!b)
    {
        return 0;
    }
    memset(b, 0, (sizeof(bbio_buf_t)));

    b->dev_type   = 'b';
    b->io_mode    = io_mode;
    b->destroy    = buf_destroy;
    b->open       = buf_open;
    b->close      = buf_close;
    b->position   = buf_position;
    b->seek       = buf_seek;
    b->set_buffer = buf_set_buffer;
    b->get_buffer = buf_get_buffer;

    if (io_mode == 'w' || io_mode == 'e')
    {
        b->write = buf_write;
    }
    if (io_mode == 'r' || io_mode == 'e')
    {
        b->read = buf_read;
        b->size = data_size;

        b->is_EOD        = buf_is_EOD;
        b->is_more_byte  = buf_is_more_byte;
        b->is_more_byte2 = buf_is_more_byte2;
        b->skip_bytes    = buf_skip_bytes;
    }

    return (bbio_handle_t)b;
}

void
bbio_buf_reg(void)
{
    reg_bbio_set('b', 'w', buf_create);
    reg_bbio_set('b', 'r', buf_create);
}
