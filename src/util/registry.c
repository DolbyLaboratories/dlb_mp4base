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
    @file registry.c
    @brief Implements a registry for holding parser, dsi and I/O methods

*/

#include "utils.h"
#include "registry.h"

/**** I/O = reg_bbio_get('f', 'r'); registry */
typedef struct reg_bbio_t_ {
    int8_t dev_type;
    int8_t io_mode;
    struct bbio_t_ *(*bbio_create)(int8_t io_mode);
} reg_bbio_t;

#define BBIO_MAX_NUM    6
static reg_bbio_t reg_bbios[BBIO_MAX_NUM + 1];
static uint32_t reg_bbio_num;

void
reg_bbio_init(void)
{
    reg_bbio_num = 0;
    /* 
    reg_bbios[PARSER_NUM_MAX].dev_type = "null";
    reg_bbios[PARSER_NUM_MAX].bbio_create = null_bbio_creater;
    */
}

struct bbio_t_ *
reg_bbio_get(int8_t dev_type, int8_t io_mode)
{
    uint32_t u;

    for (u = 0; u < reg_bbio_num; u++) {
        if (dev_type == reg_bbios[u].dev_type && io_mode == reg_bbios[u].io_mode) {
            return reg_bbios[u].bbio_create(io_mode);
        }
    }
    return 0;
}

void 
reg_bbio_set(int8_t dev_type, int8_t io_mode, struct bbio_t_ *(*bbio_create)(int8_t))
{
    /* since the sink supported is knid of static, assert is good enough */
    assert(reg_bbio_num < BBIO_MAX_NUM); 

    reg_bbios[reg_bbio_num].dev_type = dev_type;
    reg_bbios[reg_bbio_num].io_mode = io_mode;
    reg_bbios[reg_bbio_num].bbio_create = bbio_create;
    reg_bbio_num++;
}

/**** parser registry */
struct reg_parser_t_ {
    int8_t *parser_name;
    struct parser_t_ *(*parser_create)(uint32_t dsi_type);
};
typedef struct reg_parser_t_ reg_parser_t;

#define PARSER_NUM_MAX  50
static reg_parser_t reg_parsers[PARSER_NUM_MAX + 1];
static uint32_t reg_parser_num;

void
reg_parser_init(void)
{
    reg_parser_num = 0;
    /* 
    reg_parsers[PARSER_NUM_MAX].parser_name = "null";
    reg_parsers[PARSER_NUM_MAX].parser_create = null_parser_creater;
    */
}

struct parser_t_ *
reg_parser_get(const int8_t *parser_name, uint32_t dsi_type)
{
    uint32_t u;

    for (u = 0; u < reg_parser_num; u++) {
        if (!OSAL_STRCASECMP(parser_name, reg_parsers[u].parser_name)) {
            return reg_parsers[u].parser_create(dsi_type);
        }
    }
    return 0;
}

void 
reg_parser_set(int8_t *parser_name, struct parser_t_ *(*parser_create)(uint32_t dsi_type))
{
    /* since the parser supported is knid of static, assert is good enough */
    assert(reg_parser_num < PARSER_NUM_MAX); 

    reg_parsers[reg_parser_num].parser_name = parser_name;
    reg_parsers[reg_parser_num].parser_create = parser_create;
    reg_parser_num++;
}
