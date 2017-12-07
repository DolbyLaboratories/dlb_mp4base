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
 *  @file  io_base.h
 *  @brief Defines structures and functions to handle input/output opts
 */

#ifndef __IO_BASE_H__
#define __IO_BASE_H__

#include <string.h>        /* size_t       */

#include "c99_inttypes.h"  /* uint64_t     */
#include "boolean.h"       /* BOOL         */
#include "return_codes.h"  /* return codes */

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct bbio_t_  bbio_t;
typedef bbio_t *        bbio_handle_t;

#define BBIO                                                                          \
    int8_t dev_type;                                                                  \
    int8_t io_mode; /* 'r' read, 'w' write , 'e' edit an existing one(r & w)*/        \
    void (*destroy)(bbio_handle_t bbio);                                              \
                                                                                      \
    /* return native os dependedent error code. 0 for OK */                           \
    int32_t (*open)(bbio_handle_t bbio, const int8_t* dev_name);                      \
    void (*close)(bbio_handle_t bbio);                                                \
    /* return offset into the begining of the file */                                 \
    int64_t (*position)(bbio_handle_t bbio);                                          \
    /* return native os dependedent error code. 0 for OK. */                          \
    int32_t (*seek)(bbio_handle_t bbio, int64_t position, int32_t origin);            \
                                                                                      \
    /** file only */                                                                  \
    const int8_t* (*get_path)(bbio_handle_t bbio);                                    \
                                                                                      \
    /** buffer only                                                                 */\
    /* set a buf for 'w' op to write into or 'r' op to read from                    */\
    /* 'w' op:                                                                      */\
    /* re_al TRUE: realloc buffer if the actual written size is more than buf_size  */\
    /* !buf && buf_size: pre-alloc a buf of buf_size                                */\
    /* buf: pass a buf of buf_zise                                                  */\
    /* 'r' op:                                                                      */\
    /* re_al TRUE: release buffer if no longer used                                 */\
    void (*set_buffer)(bbio_handle_t bbio, uint8_t *buf, size_t buf_size, BOOL re_al);\
                                                                                      \
    /* get the buffer which 'w' op written into or 'r' op no longer read            */\
    /* 'w' op:                                                                      */\
    /* the return buffer point may differ from what set_buffer is given             */\
    /* when realloc happens                                                         */\
    /* 'r' op:                                                                      */\
    /* always return NULL if re_al == TRUE                                          */\
    /* if *data_size != 0: buffer is still read by 'r' op                           */\
    uint8_t *(*get_buffer)(bbio_handle_t bbio, size_t *data_size, size_t *buf_size);  \
                                                                                      \
    /** write/edit: return number of byte writen */                                   \
    size_t (*write)(bbio_handle_t snk, const uint8_t *buf, size_t size);    \
                                                                            \
    /** read/edit: return number of byte read */                            \
    size_t (*read)(bbio_handle_t src, uint8_t *buf, size_t size);           \
                                                                            \
    /* size of the file. buf. data size('w'), data left('r') */             \
    int64_t (*size)(bbio_handle_t bbio);                                    \
    /* if at end of buffer('w') or data('r') */                             \
    BOOL (*is_EOD)(bbio_handle_t bbio);                                     \
    /*  if whole byte available */                                          \
    BOOL (*is_more_byte)(bbio_handle_t bbio);                               \
    BOOL (*is_more_byte2)(bbio_handle_t bbio);                              \
    int32_t (*skip_bytes)(bbio_handle_t bbio, int64_t byte_num);            \
                                                                            \
    /** internal use for bit operation                                    */\
    /* 'w' op:                                                            */\
    /* when cached_bit_num == 8, write to byte intf => cached_bit_num < 8 */\
    /* 'r' op:                                                            */\
    /* when need more bit, read a byte from byte intf =>cached_bit_num > 1*/\
    uint32_t cached_bit_num, cached_bits


struct bbio_t_
{
    BBIO;
};

void bbio_file_reg(void);
void bbio_buf_reg(void);

/*
 * (some) alternatives to direct function pointer usage
 *
 * - function pointer access is sometimes not easily possible - e.g. from python scripts
 */
void bbio_call_destroy(bbio_handle_t bbio);
int32_t  bbio_call_open(bbio_handle_t bbio, const int8_t* dev_name);

/** For optimization, bit interface only handles bit operations. Byte-level operation must call byte interface directly. 
*   The 'r' operation (read): when no more bits is cached (already aligned) or after calling the src_byte_align() to go 
*   to the next closest byte-aligned position, alignment will be forced. The 'w' operation (write): call the sink_flush_bits() 
*   go to the next closest aligned position by padding output with 0 if not already aligned.
*/

/**** write "class method" */
void sink_write_u8(bbio_handle_t sink, uint8_t u8);                
void sink_write_u16(bbio_handle_t sink, uint16_t u16);              
void sink_write_u32(bbio_handle_t sink, uint32_t u32);      
void sink_write_u64(bbio_handle_t sink, uint64_t u64);  
void sink_write_4CC(bbio_handle_t sink, const int8_t *cc); 
                                                       
/** bit interfaces, designed for efficiency */        
/* use byte interface when in byte aligned position */ 
void sink_write_bit(bbio_handle_t sink, uint32_t val);  
void sink_write_bits(bbio_handle_t sink, uint32_t bit_num, uint32_t val);
void sink_flush_bits(bbio_handle_t sink);                               

/**** read "class method": Return: error code */
int32_t src_rd_u8(bbio_handle_t src, uint8_t *);
int32_t src_rd_u16(bbio_handle_t src, uint16_t *);
int32_t src_rd_u24(bbio_handle_t src, uint32_t *);
int32_t src_rd_u32(bbio_handle_t src, uint32_t *);
int32_t src_rd_u64(bbio_handle_t src, uint64_t *);
                                                       
/**** Deperecated read methods. Try not to use these methods as no error handling. */
uint8_t  src_read_u8(bbio_handle_t src);                
uint16_t src_read_u16(bbio_handle_t src);              
uint32_t src_read_u24(bbio_handle_t src);              
uint32_t src_read_u32(bbio_handle_t src);      
uint64_t src_read_u64(bbio_handle_t src);  
                                                       
/** bit interfaces, designed for efficiency */        
/* use byte interface when in byte aligned position */ 
uint32_t src_read_bit(bbio_handle_t src);  
uint32_t src_read_bits(bbio_handle_t src, uint32_t bit_num);
/* to the next closest byte aligned position */
void src_byte_align(bbio_handle_t src);                               

void src_skip_bits(bbio_handle_t src, uint32_t bit_num);
uint32_t src_peek_bits(bbio_handle_t src, uint32_t bit_num, int32_t offset);
uint32_t src_bits_cached(bbio_handle_t src);
int64_t src_following_bit_num(bbio_handle_t src);

/** Copies data of specified size from src to snk. */
void bbio_copy(bbio_handle_t snk, bbio_handle_t src, uint64_t size);

#ifdef __cplusplus
};
#endif

#endif /*__IO_BASE_H__*/
