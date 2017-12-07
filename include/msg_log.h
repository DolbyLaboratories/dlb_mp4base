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
 *  @file msg_log.h
 *  @brief Defines the message log interface.
 *
 *  To use the message log interface, the define ENABLE_MP4_MSGLOG needs to be set during compilation.
 *  Alternatively, the define EMA_BUILD can be used for easier integration into projects originally set up this way.
 *  In case RTS is available, the RTS implementation takes precedence. This implementation was once compatible to the msglog
 *  interface defined by RTS. This could still be the case - although this is not guaranteed and might change in future verions.
 */

#ifndef __MSG_LOG_H__
#define __MSG_LOG_H__

#if defined(EMA_BUILD) && EMA_BUILD
#define ENABLE_MP4_MSGLOG
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __USE_RTS__
/**
 *  @brief Msglog levels and flags.
 */
typedef enum _msglog_level_s_
{
    MSGLOG_QUIET = -1,     /**< No output at all. */

    /** levels */
    MSGLOG_EMERG = 0,      /**< Msglog level for emergency messages. */
    MSGLOG_ALERT,          /**< Msglog level for alert messages. */
    MSGLOG_CRIT,           /**< Msglog level for critical messages. */
    MSGLOG_ERR,            /**< Msglog level for error messages. */
    MSGLOG_WARNING,        /**< Msglog level for warning messages. */
    MSGLOG_NOTICE,         /**< Msglog level for notice messages. */
    MSGLOG_PRINT,          /**< Msglog level for print messages. */
    MSGLOG_INFO,           /**< Msglog level for info messages. */
    MSGLOG_DEBUG,          /**< Msglog level for debug messages. */
    MSGLOG_LEVEL_MAX,      /**< Maximum msglog level. */

    /** flags */
    MSGLOG_BOX_TREE = 16,  /**< Msglog flag to enable BOX_TREE messages. */
    MSGLOG_COLOR    = 32   /**< Msglog flag to enable colorized messages. */
} msglog_level_t;

typedef struct sys_obj_t_  sys_obj_t;

/**
 *  @brief Use GCC format string checking if compiler is GCC 3 or above.
 *
 *  @param type        The check type, such as printf - see GCC manual.
 *  @param idx         The format string position.
 *  @param check_start The start of the variable args.
 */
#if defined(__GNUC__) && __GNUC__ >= 3
#define CHECK_FMT_STR(type, idx, check_start) __attribute__ ((format (type, idx, check_start)))
#else
#define CHECK_FMT_STR(type, idx, check_start) 
#endif

#ifdef ENABLE_MP4_MSGLOG
/**
 *  @brief Conditionally write a log message.
 *
 *  The log message will be written if the global log level (excluding log flags) is equal or higher than the specified log level.
 */
#include "c99_inttypes.h"  /* int8_t */
void
msglog(sys_obj_t *    p_obj,   /**< [in] Pointer to sys_obj_t for compatibility to RTS interface - unused in this implementation. */
       msglog_level_t level,   /**< [in] Log level. Should be in the range [#MSGLOG_EMERG, #MSGLOG_DEBUG]. */
       const char *   format,  /**< [in] Format string like format string for printf() */
       ...)
/** @cond */
       CHECK_FMT_STR(__printf__, 3, 4);
/** @endcond *

/**
 *  @brief Set the global log level.
 */
void
msglog_global_verbosity_set(msglog_level_t level  /**< [in] Log level. */
                           );
/**
 *  @brief Get the global log level.
 *
 *  @return The current global log level.
 */
msglog_level_t
msglog_global_verbosity_get(void);
#else
#define msglog(p_obj, level, ...)      do { /* no logging */ } while(0)
#define msglog_global_verbosity_set(x) do { /* no logging */ } while(0)
#define msglog_global_verbosity_get()  MSGLOG_QUIET
#endif

#else
/* use rts */
#include "rts/msglog.h"
#endif /* __USE_RTS__ */

/* let's map _DEBUG to DEBUG - the .vcxproj files define _DEBUG for debug builds whereas the Makefiles define DEBUG */
#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif

/* DPRINTF macro */
/**
 *  @brief Conditionally write a log message with a log level of MSGLOG_DEBUG.
 *
 *  @note Only available in DEBUG builds.
 */
#ifdef DEBUG

#define DPRINTF(p_obj, ...) msglog(p_obj, MSGLOG_DEBUG, __VA_ARGS__)

#else

#define DPRINTF(p_obj, ...)

#endif

#ifdef __cplusplus
};
#endif

#endif /* __MSG_LOG_H__ */
