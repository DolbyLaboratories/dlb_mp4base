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
    @brief Unit test of utililities module.
*/

#include <utils.h>

#include <test_util.h>

void
static test_BE()
{
    uint8_t bytes[8];
    uint64_t r;
    bytes[0] = 1;
    bytes[1] = 0;
    bytes[2] = 0;
    bytes[3] = 0;
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 0;
    bytes[7] = 0;


    r = 1 << 8;
    assure( get_BE_u16(bytes) == r );

    r = r << 16;
    assure( get_BE_u32(bytes) == r );

    r = r << 32;
    assure( get_BE_u64(bytes) == r );
}

int main(void)
{
    test_BE();

    return 0;
}
