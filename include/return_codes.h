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
    @file return_codes.h
    @brief Defines return codes Macros
*/

#ifndef RETURN_CODES_H
#define RETURN_CODES_H

/**************** Return codes *********************/
/** 0x0: OK */
#define EMA_MP4_MUXED_OK              0x0   /**< successful */

/** 0x1?: input config */
#define EMA_MP4_MUXED_PARAM_ERR       0x10  /**< parameter error */
#define EMA_MP4_MUXED_TOO_MANY_ES     0x11  /**< too many es to mux */
#define EMA_MP4_MUXED_NO_ES           0x12  /**< no es to mux */
#define EMA_MP4_MUXED_UNKNOW_ES       0x13  /**< es unknown */
#define EMA_MP4_MUXED_NO_OUTPUT       0x14  /**< no output */   
#define EMA_MP4_MUXED_OPEN_FILE_ERR   0x15  /**< file open err */
#define EMA_MP4_MUXED_EOES            0x16  /**< end of es */
#define EMA_MP4_MUXED_IO_ERR          0x17  /**< I/O err */
#define EMA_MP4_MUXED_CLI_ERR         0x18  /**< CLI err */
#define EMA_MP4_MUXED_EMPTY_ES        0x19  /**< empty es to mux */

/** 0x2?: I/O operation */
#define EMA_MP4_MUXED_WRITE_ERR       0x20  /**< write error */
#define EMA_MP4_MUXED_READ_ERR        0x21  /**< read error */

/** 0x4?: parsing */
#define EMA_MP4_MUXED_SYNC_ERR        0x40  /**< parsing ES error (sync) */
#define EMA_MP4_MUXED_ES_ERR          0x41  /**< parsing ES error */
#define EMA_MP4_MUXED_MP4_ERR         0x42  /**< parsing mp4 file err */
#define EMA_MP4_MUXED_NO_CONFIG_ERR   0x43  /**< no config found before payload starts */
#define EMA_MP4_MUXED_MULTI_SD_ERR    0x44  /**< multiple sample descriptions necessary but deactivated */
#define EMA_MP4_MUXED_CONFIG_ERR      0x45  /**< unallowable config change */
#define EMA_MP4_MUXED_NO_SUPPORT      0x49  /**< not supported syntax/semantics */

/** 0x8?: resource */
#define EMA_MP4_MUXED_NO_MEM          0x80  /**< no memory */

/** 0x10?: bugs */
#define EMA_MP4_MUXED_BUGGY           0x100 /**< unknown bug */

/** 0x11?: exit */
#define EMA_MP4_MUXED_EXIT            0x110 /**< exit by design */

#endif  /*  RETURN_CODES_H */
