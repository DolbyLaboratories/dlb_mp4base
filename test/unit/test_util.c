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

#include "test_util.h"

#include <registry.h>
#include <utils.h>

bbio_handle_t bbio_to_file(const char *filename)
{
    bbio_handle_t mp4_src = reg_bbio_get('f', 'w');
    assure(mp4_src != NULL);
    assure(mp4_src->open(mp4_src, filename) == 0);

    return mp4_src;
}

bbio_handle_t bbio_from_file(const char *filename)
{
    bbio_handle_t mp4_src = reg_bbio_get('f', 'r');
    assure(mp4_src != NULL);
    assure(mp4_src->open(mp4_src, filename) == 0);

    return mp4_src;
}

char *string_cat(const char *s, const char *t)
{
    size_t length = strlen(s) + strlen(t) + 1;
    char * c = malloc(length);
    assure(c != NULL);

    assure(OSAL_SNPRINTF(c, length, "%s%s", s, t) == length - 1);

    return c;
}

