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
    @file registry.h
    @brief Defines a registry for holding parser and I/O methods

*/

#ifndef __REGISTRY_H__
#define __REGISTRY_H__

#include "c99_inttypes.h"  /** uint32_t */

#ifdef __cplusplus
extern "C"
{
#endif

/** I/O */
struct bbio_t_;
void            reg_bbio_init(void);
void            reg_bbio_set(int8_t  dev_type, int8_t  io_mode, struct bbio_t_ *(*bbio_create)(int8_t));
struct bbio_t_ *reg_bbio_get(int8_t  dev_type, int8_t  io_mode);

/** parser */
struct parser_t_;
void              reg_parser_init(void);
void              reg_parser_set(int8_t  *parser_name, struct parser_t_ *(*parser_create)(uint32_t dsi_type));
struct parser_t_ *reg_parser_get(const int8_t  *parser_name, uint32_t dsi_type);

#ifdef __cplusplus
};
#endif
#endif /* __REGISTRY_H__ */
