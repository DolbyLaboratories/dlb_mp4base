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
 *  @file  c99_inttypes.h
 *  @brief Defines C99 integer types and format Macros
 */

#ifndef C99_INTTYPES_H
#define C99_INTTYPES_H

#ifndef _MSC_VER
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/** explicitely request the format macros (PRIu64 etc.) form <inttypes.h> */
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
#else
/** Visual C++ does not fully support C99 */

#if (_MSC_VER >= 1600)
/** Visual Studio 10 has at least <stdint.h> */
#include <stdint.h>
#else
/** Integer types */
typedef __int8           int8_t;
typedef __int16          int16_t;
typedef __int32          int32_t;
typedef __int64          int64_t;
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#endif

/** some macros from (non-existing in MSVC) inttypes.h would look like this */

/** Macros for scanning format specifiers */
#define SCNu64 "I64u"
#define SCNx64 "I64x"

/** Macros for printing format specifiers */
#define PRIi64 "I64i"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#endif

/** not specified by C99 but useful for portable printing of size_t variables */
#ifdef _MSC_VER
#define PRIz "Iu"
#else
#define PRIz "zu"
#endif

#endif  /* C99_INTTYPES_H */
