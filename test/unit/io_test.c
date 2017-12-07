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
    @brief Unit test of I/O module
*/

#include <io_base.h>
#include <registry.h>
#include <test_util.h>

static
void test_bbio()
{
    const char filename[] = "bbio_test.mp4";
    const uint8_t buffer[] = "contents of mp4 file";
    const size_t write_size = 7;
    const size_t read_size = 3;
    bbio_handle_t b;

    assure( sizeof(buffer) % write_size == 0 );
    assure( sizeof(buffer) % read_size == 0 );

    /* Write file */
    b = reg_bbio_get('f', 'w');
    assure( b != NULL );
    assure( b->open     != NULL );
    assure( b->close    != NULL );
    assure( b->position != NULL );
    assure( b->seek     != NULL );
    assure( b->get_path != NULL );
    assure( b->read     == NULL );
    assure( b->size     == NULL );
    assure( b->is_EOD   == NULL );
    assure( b->is_more_byte  == NULL );
    assure( b->is_more_byte2 == NULL );
    assure( b->skip_bytes == NULL );
    assure( b->write  != NULL );

    assure( b->open(b, filename) == 0 );

    {
        size_t bytes_written = 0;
        while (bytes_written < sizeof(buffer))
        {
            assure( b->write(b, buffer + bytes_written, write_size) == write_size);
            bytes_written += write_size;
        }
        printf("Wrote %lu bytes to %s\n", bytes_written, filename);
    }

    b->close(b);
    b->destroy(b);

    /* Read file and verify its contents */
    b = reg_bbio_get('f', 'r');
    assure( b != NULL );
    assure( b->open     != NULL );
    assure( b->close    != NULL );
    assure( b->position != NULL );
    assure( b->seek     != NULL );
    assure( b->get_path != NULL );
    assure( b->read     != NULL );
    assure( b->size     != NULL );
    assure( b->is_EOD   != NULL );
    assure( b->is_more_byte  != NULL );
    assure( b->is_more_byte2 != NULL );
    assure( b->skip_bytes != NULL );
    assure( b->write  == NULL );

    assure( b->open(b, filename) == 0 );

    printf("File path is %s\n", b->get_path(b));

    {
        uint8_t * read_buffer = malloc(read_size * sizeof(*read_buffer));

        size_t total_bytes_read = 0;
        size_t bytes_read = 0;
        do
        {
            bytes_read = b->read(b, read_buffer, sizeof(*read_buffer));
            printf("Read %lu bytes\n", bytes_read);

            {
                size_t i;
                for (i = 0; i < bytes_read; i++)
                {
                    assure(read_buffer[i] == buffer[total_bytes_read + i]);
                    printf("%c", read_buffer[i]);
                }
            }
            total_bytes_read += bytes_read;
        } 
        while (bytes_read > 0);

        free(read_buffer);
    }

    b->close(b);
    b->destroy(b);
}

int main(void)
{
    /* Initialization of global data */
    reg_bbio_init();
    bbio_file_reg();
    bbio_buf_reg();

    /* Run tests */
    test_bbio();

    return 0;
}
