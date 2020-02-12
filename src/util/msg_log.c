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
 *  @file msg_log.c
 *  @brief Implements the message log interface.
 */

#ifndef __USE_RTS__

#include <stdio.h>
#include <stdarg.h>
#ifdef _MSC_VER
#include <windows.h>  /* WinApi */
#include <io.h>       /* _isatty() */
#else
#include <unistd.h>   /* isatty()  */
#define _isatty isatty
#endif

#include "msg_log.h"

#ifdef ENABLE_MP4_MSGLOG

#ifdef DEBUG
static msglog_level_t msg_log_level = MSGLOG_ERR;     /**< The global msglog level. */
#else
static msglog_level_t msg_log_level = MSGLOG_ERR;  /**< The global msglog level. */
#endif

static int            msg_color_out = 0;               /**< The global msglog flag for colorized messages. */
#ifdef _MSC_VER
static HANDLE         msg_h_console = NULL;
static WORD           msg_defattrib = 0;
#endif

#ifdef _MSC_VER
/**
 * @brief Converts a msglog level into a color character attribute for use with SetConsoleTextAttribute()
 */
static WORD level2color[MSGLOG_LEVEL_MAX+1+32] =
    {
        BACKGROUND_RED,                                        /* MSGLOG_EMERG:     back_red */
        BACKGROUND_RED,                                        /* MSGLOG_ALERT:     back_red */
        BACKGROUND_RED,                                        /* MSGLOG_CRIT:      back_red */
        FOREGROUND_RED,                                        /* MSGLOG_ERR:       red      */
        FOREGROUND_BLUE  | FOREGROUND_RED,                     /* MSGLOG_WARNING:   magenta  */
        FOREGROUND_BLUE  | FOREGROUND_GREEN,                   /* MSGLOG_NOTICE:    cyan     */
        FOREGROUND_BLUE,                                       /* MSGLOG_PRINT:     blue     */
        FOREGROUND_GREEN,                                      /* MSGLOG_INFO:      green    */
        FOREGROUND_GREEN | FOREGROUND_RED,                     /* MSGLOG_DEBUG:     yellow   */
        FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED   /* MSGLOG_LEVEL_MAX: white    */
    };
#else
/**
 * @brief Converts a msglog level into a color attribute code for use with ANSI escape character
 */
static int level2color[MSGLOG_LEVEL_MAX+1] =
    {
        41,  /* MSGLOG_EMERG:     back_red */
        41,  /* MSGLOG_ALERT:     back_red */
        41,  /* MSGLOG_CRIT:      back_red */
        31,  /* MSGLOG_ERR:       red      */
        35,  /* MSGLOG_WARNING:   magenta  */
        36,  /* MSGLOG_NOTICE:    cyan     */
        34,  /* MSGLOG_PRINT:     blue     */
        32,  /* MSGLOG_INFO:      green    */
        33,  /* MSGLOG_DEBUG:     yellow   */
        37   /* MSGLOG_LEVEL_MAX: white    */
    };
#endif

void
msglog(sys_obj_t *p_obj, msglog_level_t level, const char *format, ...)
{
    va_list vl;

    if (msg_log_level == MSGLOG_QUIET)
    {
        return;
    }

    if (level > MSGLOG_LEVEL_MAX)
    {
        /* level is a flag: specific messages can be selected using flags */
        if (level & msg_log_level)
        {
            va_start(vl, format);
            vfprintf(stdout, format, vl);
            va_end(vl);
            return;
        }
    }

    if ((level > (msg_log_level & 0x0F)) || (level < 0))
    {
        return;
    }

    va_start(vl, format);

    if (msg_color_out)
    {
#ifdef _MSC_VER
        SetConsoleTextAttribute(msg_h_console, level2color[level]);
#else
        fprintf(stdout, "\033[%dm", level2color[level]);
#endif
    }

    vfprintf(stdout, format, vl);

    if (msg_color_out)
    {
#ifdef _MSC_VER
        SetConsoleTextAttribute(msg_h_console, msg_defattrib);
#else
        fprintf(stdout, "\033[0m");  /* color off */
#endif
    }

    va_end(vl);

    (void)p_obj;  /* avoid compiler warning */
}

msglog_level_t
msglog_global_verbosity_get (void)
{
    return msg_log_level;
}

void
msglog_global_verbosity_set (msglog_level_t level)
{
    msg_log_level = level;

    if ((level != MSGLOG_QUIET) && (level & MSGLOG_COLOR))
    {
        /* enable colorized messages if stdout is a tty */
        msg_color_out = _isatty(1);
#ifdef _MSC_VER
        if (msg_color_out)
        {
            CONSOLE_SCREEN_BUFFER_INFO csb_info;

            msg_h_console = GetStdHandle(STD_OUTPUT_HANDLE);
            GetConsoleScreenBufferInfo(msg_h_console, &csb_info);
            msg_defattrib = csb_info.wAttributes;
        }
#endif
    }
    else
    {
        msg_color_out = 0;
    }
}

#endif  /*  ENABLE_MP4_MSGLOG */
#endif  /*  __USE_RTS__ */
