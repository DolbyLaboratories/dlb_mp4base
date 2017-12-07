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
    @file utils.h
    @brief Defines types, OS abstration layer. Inlcudes common header file.
*/

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <assert.h>        /** assert()  */
#include <stdio.h>         /** sprintf() */
#include <string.h>        /** strncpy() */

#include "memory_chk.h"
#include "msg_log.h"
#include "c99_inttypes.h"  /** uint32_t */

#ifdef __GNUC__
#if defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2,1)
/** Note: ftello() and fseeko() are available since glibc 2.1. */
#define USE_STDIO_FOR_OSAL
#endif
#else
/** all current gnu C compiler should support ftello() and fseeko() */
#define USE_STDIO_FOR_OSAL
#endif
#endif

/**
  @brief Goto cleanup if the given error expression does not evaluate
  to zero.
*/
#define CHECK(err_expr)                                     \
do {                                                        \
    err = (err_expr);                                       \
    if (err != 0)                                           \
    {                                                       \
        goto cleanup;                                       \
    }                                                       \
} while(0)


/**************** read in value in BE *********************/
uint32_t get_BE_u16(const uint8_t* bytes);
uint32_t get_BE_u32(const uint8_t* bytes);
uint64_t get_BE_u64(const uint8_t* bytes);

#define FLIPENDIAN_INT32(x) ((((uint32_t) (x)) << 24) | (((uint32_t) (x)) >> 24) | \
    (( ((uint32_t) (x)) & 0x0000ff00) << 8) | (( ((uint32_t) (x)) & 0x00ff0000) >> 8))

/***************** Math operations *********************/
#define MAX2(a,b) ((a) > (b) ? (a) : (b))
#define MIN2(a,b) ((a) > (b) ? (b) : (a))

/** greatest common denominator */
uint32_t get_GCD(uint32_t a, uint32_t b);


/** seconds elapsed since 1970 in UTC time */
int64_t utc_sec_since_1970(void);

FILE* create_temp_file(void);
int8_t *get_temp_path(void);

/** Convert binary data in inbuf to ascii string in outbuf with binsize bytes in inbuf.
   outbuf size must be 2x of inbuf + 1. Note: no boundary check in this function */
void Bin2Hex(uint8_t* inbuf, int32_t insize, uint8_t* outbuf);

/** rescale: return u64*new_scale/old_scale */
uint64_t rescale_u64(uint64_t u64, uint32_t new_scale, uint32_t old_scale);


/***************** dump indicator to show progress *********************/
struct progress_t_
{
    int8_t *  caption;
    int64_t size_total;
    int32_t processed_ratio;

    void (*destroy)(struct progress_t_ *h);
    void (*show)(struct progress_t_ *h, int64_t size_done);
};
typedef struct progress_t_  progress_t;
typedef progress_t *progress_handle_t; 

progress_handle_t progress_create(const int8_t *title, int64_t size_total);

/***************** OSAL layer *********************/

/** buf or std I/O */
#ifdef _MSC_VER
    #define OSAL_SSCANF(buf, ...) sscanf_s(buf, __VA_ARGS__)
    #define OSAL_SNPRINTF(buf, buf_len, ...) sprintf_s(buf, buf_len, __VA_ARGS__)
    #define STRTOLL(s, p, b) _strtoi64(s, p, b)

    #define OSAL_STRNCPY(dest_buf, dest_buf_len, src_buf, src_len)      \
                    strncpy_s(dest_buf, dest_buf_len, src_buf, src_len)
    #define OSAL_STRCPY(dest_buf, dest_buf_len, src_buf)                \
                    strcpy_s(dest_buf, dest_buf_len, src_buf)

    #define OSAL_STRCASECMP _stricmp

    #define PATH_DELIMITER  '\\'
#else
    #define OSAL_SSCANF(buf, ...) sscanf(buf, __VA_ARGS__)
    #define OSAL_SNPRINTF(buf, buf_len, ...) sprintf(buf, __VA_ARGS__)
    #define STRTOLL(s, p, b) strtoll(s, p, b)

    #define OSAL_STRNCPY(dest_buf, dest_buf_len, src_buf, src_len)      \
                    strncpy(dest_buf, src_buf, src_len)
    #define OSAL_STRCPY(dest_buf, dest_buf_len, src_buf)                \
                    strcpy(dest_buf, src_buf)

    #define OSAL_STRCASECMP strcasecmp

    #define PATH_DELIMITER  '/'
#endif
/** End of buf or std I/O */

/** file I/O */
#ifdef _MSC_VER
    #include <io.h>
    #include <share.h>
    #include <direct.h>
    #include <fcntl.h>

    #define OSAL_FOPEN(err, fp, fn, mode)                       \
        if (mode == 'w') (err) = fopen_s(&(fp), fn, "wb");      \
        else if (mode == 'r') (err) = fopen_s(&(fp), fn, "rb"); \
        else (err) = fopen_s(&(fp), fn, "r+b")

    #define OSAL_FCLOSE                     fclose
    #define OSAL_FTELL                      _ftelli64
    #define OSAL_FSEEK                      _fseeki64
    #define OSAL_FEOF                       feof
    #define OSAL_FREAD(buf, size, fp)       fread(buf, 1, size, fp)
    #define OSAL_FWRITE(buf, size, fp)      fwrite(buf, 1, size, fp);

    #define OSAL_FILE_HANDLE_T              FILE *
    #define OSAL_DEL_FILE                   _unlink

#elif defined(USE_STDIO_FOR_OSAL)

    /** Standard I/O */
    #include <unistd.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>

    #define OSAL_FOPEN(err, fp, fn, mode)                   \
        do {                                                \
            if (mode == 'w') (fp) = fopen(fn, "wb");        \
            else if (mode == 'r') (fp) = fopen(fn, "rb");   \
            else (fp) = fopen(fn, "r+b");                   \
            if ((fp)) (err) = 0;                            \
            else (err)=errno;                               \
        } while (0)

    #define OSAL_FCLOSE                     fclose
    /** Note: to support file sizes > 3.75 GB - even on 32-bit systems, ftello() and fseeko() are used.
     * In contrast to ftell() and fseek(), they are using off_t instead of long for the file position / offset.
     * To turn off_t into a 64-bit type on a 32-bit system, _FILE_OFFSET_BITS needs to be defined (via the Makefiles) to be 64.
     */
#if defined(__linux__) && !defined(__x86_64__) && !defined( __USE_FILE_OFFSET64)
#pragma message(__FILE__": __USE_FILE_OFFSET64 is not defined! Do the Makefiles define _FILE_OFFSET_BITS=64?")
#endif
    #define OSAL_FTELL                      ftello
    #define OSAL_FSEEK                      fseeko
    #define OSAL_FEOF                       feof
    #define OSAL_FREAD(buf, size, fp)       fread(buf, 1, size, fp)
    #define OSAL_FWRITE(buf, size, fp)      fwrite(buf, 1, size, fp)

    #define OSAL_FILE_HANDLE_T              FILE *
    #define OSAL_DEL_FILE                   unlink

#else
#pragma message(__FILE__": OSAL using File I/O")
    /** File I/O */
    #include <unistd.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>

    #define OSAL_FOPEN(err, fd, fn, mode)                       \
        do {                                                    \
            if (mode == 'w') (fd) = open(fn, O_WRONLY | O_CREAT \
            | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);  \
            else if (mode == 'r') (fd) = open(fn, O_RDONLY);    \
            else (fd) = open(fn, O_RDWR);                       \
            if ((fd)>=0) (err) = 0;                             \
            else (err)=errno;                                   \
        } while (0)

    #define OSAL_FCLOSE                             close
    #define OSAL_FTELL(fd)                          lseek(fd, 0, SEEK_CUR)
    #define OSAL_FSEEK                              lseek
    #define OSAL_FREAD(buf, size, fd)               read(fd, buf, size)
    #define OSAL_FWRITE(buf, size, fd)              write(fd, buf, size)
    #define OSAL_FEOF                               eof

    #define OSAL_FILE_HANDLE_T                      long
    #define OSAL_DEL_FILE                           unlink

#endif
/** End of file I/O */

/** thread, process */
#ifdef _MSC_VER
    #include <process.h>
    #define OSAL_GETPID                             _getpid
#else
    #include <sys/types.h>
    /** #include <unistd.h> already included */
    #define OSAL_GETPID                             getpid
#endif
/** End of thread, process */

/***************** End of OSAL layer *********************/



#ifdef __cplusplus
};
#endif /* C++ */

#endif /* __UTILS_H__ */
