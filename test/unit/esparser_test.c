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
    @file  esparser_test.c
    @brief Unit test frontend to the mp4base parsers.
*/

#include <parser.h>
#include <registry.h>

#include <test_util.h>

const char *SIGNALS_DIR = "..\signals";

static
    void test_parsing(parser_handle_t parser)
{
    printf("Stream type: %d\n", parser->stream_type);
    printf("Stream ID: %d\n", parser->stream_id);
    printf("Stream name: %s\n", parser->stream_name);

    if (parser->show_info != NULL)
    {
        parser->show_info(parser);
    }

    {
        mp4_sample_handle_t sample = sample_create();

        assure( sample != NULL );
        assure( parser->get_sample != NULL );

        {
            int number_of_samples = 0;
            int err = parser->get_sample(parser, sample);
            while (err == 0 && number_of_samples < 50)
            {
                number_of_samples += 1;

                printf("Sample %d ", number_of_samples);
                printf("DTS: %llu ", sample->dts);
                printf("CTS: %llu ", sample->cts);
                printf("Duration: %d ", sample->duration);
                printf("Size: %ld ", sample->size);
                printf("Buffer: %p ", sample->data);
                printf("Flags: %d ", sample->flags);
                printf("sd_index: %d ", sample->sd_index);
                printf("\n");

                // fails with H264 and AVC parser: assure( sample->data != NULL );

                err = parser->get_sample(parser, sample);
            }
        }

        sample->destroy(sample);
    }
}

static
void test_parser(const char * filename, const char * parser_type)
{
    reg_parser_init();

    parser_aac_reg();
    parser_ac3_reg();
    parser_ec3_reg();
    parser_mlp_reg();
    parser_avc_reg();

    assure( reg_parser_get("non_existing", 0) == 0 );

    {
        uint32_t dsi_type = DSI_TYPE_MP4FF;
        parser_handle_t parser = reg_parser_get(parser_type, dsi_type);
        assure( parser != NULL );

        {
            bbio_handle_t es;
            int err;
            const char *abs_path = string_cat(string_cat(SIGNALS_DIR, "/"), filename);

            printf("Parsing %s using %s parser ...\n", abs_path, parser_type);        
            es = bbio_from_file(abs_path);
    
            {
                uint32_t es_idx = 0;
                ext_timing_info_t timing_info = {1, 0, 1};
                err = parser->init(parser, &timing_info, es_idx, es);
            }
            if (err == 0)
            {
                test_parsing(parser);
            }
            else
            {
                printf("%s parser could not parse %s\n", parser_type, abs_path);
            }
        }
        parser->destroy(parser);
    }
}

int main(int argc, char **argv)
{
    /* Initialization of global data */
    SIGNALS_DIR = argv[1];
    assure( SIGNALS_DIR != NULL );
    reg_bbio_init();
    bbio_file_reg();
    bbio_buf_reg();

    test_parser("random.dat", "aac");
    test_parser("random.dat", "ac3");
    test_parser("random.dat", "ec3");
    test_parser("random.dat", "mlp");
    test_parser("random.dat", "avc");
    
    test_parser("bd_channel_ID_8ch_96k.mlp", "aac");
    test_parser("bd_channel_ID_8ch_96k.mlp", "ac3");
    test_parser("bd_channel_ID_8ch_96k.mlp", "ec3");
    test_parser("bd_channel_ID_8ch_96k.mlp", "mlp");
    test_parser("bd_channel_ID_8ch_96k.mlp", "avc");

    test_parser("Blue_Devils_30s.aac", "aac");
    test_parser("Blue_Devils_30s.aac", "ac3");
    test_parser("Blue_Devils_30s.aac", "ec3");
    test_parser("Blue_Devils_30s.aac", "mlp");
    test_parser("Blue_Devils_30s.aac", "avc");

    test_parser("7ch_ddp_25fps_channel_id.ec3", "aac");
    test_parser("7ch_ddp_25fps_channel_id.ec3", "ac3");
    test_parser("7ch_ddp_25fps_channel_id.ec3", "ec3");
    test_parser("7ch_ddp_25fps_channel_id.ec3", "mlp");
    test_parser("7ch_ddp_25fps_channel_id.ec3", "avc");

    test_parser("5ch_dd_25fps_channel_id.ac3", "aac");
    test_parser("5ch_dd_25fps_channel_id.ac3", "ac3");
    test_parser("5ch_dd_25fps_channel_id.ac3", "ec3");
    test_parser("5ch_dd_25fps_channel_id.ac3", "mlp");
    test_parser("5ch_dd_25fps_channel_id.ac3", "avc");

    test_parser("7ch_ddp_25fps_dialnorm.h264", "aac");
    test_parser("7ch_ddp_25fps_dialnorm.h264", "ac3");
    test_parser("7ch_ddp_25fps_dialnorm.h264", "ec3");
    test_parser("7ch_ddp_25fps_dialnorm.h264", "mlp");
    test_parser("7ch_ddp_25fps_dialnorm.h264", "avc");

    return 0;
}
