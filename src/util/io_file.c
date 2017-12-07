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
    @file io_file.c
    @brief Implemtents file I/O method
*/

#include <stdlib.h>    /** free() */

#include "utils.h"
#include "io_base.h"
#include "registry.h"
#ifdef _MSC_VER
#include <windows.h>
#endif



typedef struct bbio_file_t_
{
    BBIO;

    OSAL_FILE_HANDLE_T fp;
    
    int8_t*  dev_path;
    int64_t file_len;
} bbio_file_t;
typedef bbio_file_t *bbio_file_handle_t;


static int32_t
file_open(bbio_handle_t bbio, const int8_t *dev_name)
{
    bbio_file_handle_t f = (bbio_file_handle_t)bbio;
    int ret;

#ifdef _MSC_VER
    {
        wchar_t *wfilename;
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, dev_name, -1, NULL, 0);
        if (wlen == 0)
            return 1;
        wfilename = malloc(sizeof(wchar_t) * wlen);
        if (wfilename == NULL)
            return 1;
        MultiByteToWideChar(CP_UTF8, 0, dev_name, -1, wfilename, wlen);
        ret = _wfopen_s(&f->fp, wfilename, bbio->io_mode == 'r' ? L"rb" : L"wb");
        free(wfilename);
    }
#else
    OSAL_FOPEN(ret, f->fp, dev_name, bbio->io_mode);
#endif

    if (!ret) {
        char *pd;

        if (bbio->io_mode == 'r') {
            OSAL_FSEEK(f->fp, 0, SEEK_END);
            f->file_len = OSAL_FTELL(f->fp);
            OSAL_FSEEK(f->fp, 0, SEEK_SET);
        }

        pd = strrchr(dev_name, '/');
        if (!pd) {
            pd = strrchr(dev_name, '\\');
        }
        if(pd && (dev_name[0] == '/' || dev_name[1] == ':' || dev_name[0] == '\\')) {
            /** absolute path already */
            uint32_t path_len;

            path_len = (uint32_t)(pd - dev_name + 1); /** +1 => include PATH_DELIMITER */
            f->dev_path = MALLOC_CHK((uint32_t)path_len + 1); /** +1 => to put '\0' */
            OSAL_STRNCPY(f->dev_path, path_len + 1, dev_name, path_len);
            f->dev_path[path_len]='\0';
        }
        else {
            char *cwd;
            uint32_t cwd_len;

#ifdef _MSC_VER
            cwd = _getcwd(NULL, 0);
#else
            cwd = getcwd(NULL, 0);
#endif
            cwd_len = (uint32_t)strlen(cwd); /** to add PATH_DELIMITER */

            if (pd) {
                /** have relative path: absolute path = cwd PATH_DELIMITER relative path */
                uint32_t rp_len = (uint32_t)(pd - dev_name + 1); /** +1 => include PATH_DELIMITER */

                f->dev_path = MALLOC_CHK(cwd_len + rp_len + 2); /** +2: PATH_DELIMITER and '\0' */
                OSAL_STRNCPY(f->dev_path, cwd_len + rp_len + 2, cwd, cwd_len);
                f->dev_path[cwd_len++] = PATH_DELIMITER;
                OSAL_STRNCPY(f->dev_path + cwd_len, rp_len + 1, dev_name, rp_len);
                f->dev_path[cwd_len + rp_len] = '\0';
            }
            else {
                /** no relative path: absolute path = cwd PATH_DELIMITER */
                f->dev_path = MALLOC_CHK(cwd_len + 2); /** +2: PATH_DELIMITER and '\0' */

                OSAL_STRNCPY(f->dev_path, cwd_len + 2, cwd, cwd_len);
                f->dev_path[cwd_len++] = PATH_DELIMITER;
                f->dev_path[cwd_len] = '\0';
            }
            free(cwd);
        }
    }

    return ret;
}

static void
file_close(bbio_handle_t bbio)
{
    bbio_file_handle_t f = (bbio_file_handle_t)bbio;

    if (f->fp) OSAL_FCLOSE(f->fp);
    if (f->dev_path) FREE_CHK(f->dev_path);
    f->fp = 0;
    f->dev_path = 0;
}

static int64_t
file_position(bbio_handle_t bbio)
{
    return OSAL_FTELL(((bbio_file_handle_t)bbio)->fp);
}

static int32_t
file_seek(bbio_handle_t bbio, int64_t offset, int32_t origin)
{
    return OSAL_FSEEK(((bbio_file_handle_t)bbio)->fp, offset, origin);
}

static const int8_t* 
file_get_path(bbio_handle_t bbio)
{
    return ((bbio_file_handle_t)bbio)->dev_path;
}


static size_t
file_write(bbio_handle_t snk, const uint8_t *buf, size_t size)
{
    return OSAL_FWRITE(buf, size, ((bbio_file_handle_t)snk)->fp);
}

static size_t 
file_read(bbio_handle_t src, uint8_t *buf, size_t size)
{
    return OSAL_FREAD(buf, size, ((bbio_file_handle_t)src)->fp);
}

/** size of the data file or in buf */  
static int64_t
file_size(bbio_handle_t bbio)
{
    return ((bbio_file_handle_t)bbio)->file_len;
}

/** if at end of buffer('w') or data('r') */                    
static BOOL 
file_is_EOD(bbio_handle_t bbio)
{
    bbio_file_handle_t f = (bbio_file_handle_t)bbio;

    return (f->fp ? f->file_len <= file_position(bbio) : TRUE);
}

/**  if whole byte available */                
static BOOL 
file_is_more_byte(bbio_handle_t bbio)
{
    bbio_file_handle_t f = (bbio_file_handle_t)bbio;

    return f->file_len - file_position(bbio) > 0;
}

static BOOL
file_is_more_byte2(bbio_handle_t bbio)
{
    bbio_file_handle_t f = (bbio_file_handle_t)bbio;

    return f->file_len - file_position(bbio) > 1;
}

static int32_t 
file_skip_bytes(bbio_handle_t bbio, int64_t byte_num)
{
    return OSAL_FSEEK(((bbio_file_handle_t)bbio)->fp, byte_num, SEEK_CUR);
}


static void
file_destroy(bbio_handle_t bbio)
{
    file_close(bbio);
    FREE_CHK(bbio);
}



static bbio_handle_t
file_create(int8_t io_mode)
{
    bbio_file_handle_t f;

    f = (bbio_file_handle_t)MALLOC_CHK(sizeof(bbio_file_t));
    if (!f)
    {
        return 0;
    }
    memset(f, 0, (sizeof(bbio_file_t)));

    f->dev_type = 'f';
    f->io_mode  = io_mode;
    f->destroy  = file_destroy;
    f->open     = file_open;
    f->close    = file_close;
    f->position = file_position;
    f->seek     = file_seek;

    f->get_path = file_get_path;

    if (io_mode == 'w' || io_mode == 'e')
    {
        f->write = file_write;
    }
    if (io_mode == 'r' || io_mode == 'e')
    {
        f->read = file_read;
        f->size = file_size;

        f->is_EOD        = file_is_EOD;
        f->is_more_byte  = file_is_more_byte;
        f->is_more_byte2 = file_is_more_byte2;
        f->skip_bytes    = file_skip_bytes;
    }

    return (bbio_handle_t)f;
}


void
bbio_file_reg(void)
{
    reg_bbio_set('f', 'w', file_create);
    reg_bbio_set('f', 'r', file_create);
}

