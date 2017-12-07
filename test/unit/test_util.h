/******************************************************************************
 * This program is protected under international and U.S. copyright laws as
 * an unpublished work. This program is confidential and proprietary to the
 * copyright owners. Reproduction or disclosure, in whole or in part, or the
 * production of derivative works therefrom without the express permission of
 * the copyright owners is prohibited.
 *
 *                Copyright (C) 2012 by Dolby Laboratories.
 *                            All rights reserved.
 ******************************************************************************/
/*!
    @brief Testing tools
*/
#include <io_base.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define FUNC __FUNCTION__
#else
#define FUNC __func__
#endif

#define assure0(expr)  printf("hello\n");



#define assure(expr)                                                   \
do {                                                                   \
    printf("-------- %s:%d:%s(): %s ... ", __FILE__, __LINE__, FUNC, #expr); \
    fflush(stdout);                                                    \
    if (!(expr))                                                       \
    {                                                                  \
        fprintf(stderr,                                                \
               "-------- %s:%d:%s(): %s ... ", __FILE__, __LINE__, FUNC, #expr); \
        fprintf(stderr, "FAILED\n");                                   \
    }                                                                  \
    else                                                               \
    {                                                                  \
        printf("ok\n");                                                \
    }                                                                  \
} while (0)

bbio_handle_t bbio_to_file(const char *filename);
bbio_handle_t bbio_from_file(const char *filename);

/*
  Returns: concatenated string, which must be free()d by the caller
 */
char *string_cat(const char *s, const char *t);
