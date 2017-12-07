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
 *  @file  memory_chk.h
 *  @brief Defines Macros for Memeory options.
 */

#ifndef __MEMORY_CHK_H__
#define __MEMORY_CHK_H__


#ifdef __cplusplus
extern "C"
{
#endif
 /*
 * Memory (re)allocation, free and string duplication checking routines.
 *
 */
    #include    <stdlib.h>
    #include    <string.h>

    #define MEM_CHK_INIT()
    #ifdef _MSC_VER
        #define STRDUP_CHK(ptr)     _strdup(ptr)
    #else
        #define STRDUP_CHK(ptr)     strdup(ptr)
    #endif
    #define MALLOC_CHK(size)        malloc(size)
    #define REALLOC_CHK(ptr, size)  realloc(ptr, size)
    #define FREE_CHK(ptr)           if (ptr) free(ptr)

    #define MEM_LEAK_CHK_MALLOC(ptr) 
    #define MEM_LEAK_CHK_FREE(ptr)  
    #define MEM_LEAK_CHK_DEF(ptr)   

#define MEM_FREE_AND_NULL(ptr) do { FREE_CHK(ptr); (ptr) = NULL; } while (0)

#ifdef __cplusplus
};
#endif

#endif /* __MEMORY_CHK_H__ */
