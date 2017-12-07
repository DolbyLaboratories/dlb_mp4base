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
 *  @file  mp4_frag.h
 *  @brief Defines fragmented structures and Macros
 */

#ifndef __MP4_FRAG_H__
#define __MP4_FRAG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "c99_inttypes.h"  /* uint32_t */

enum tf_flags_t_
{
    TF_FLAGS_BASE_DATA_OFFSET           =    0x01,
    TF_FLAGS_SAMPLE_DESCRIPTION_INDEX   =    0x02,
    TF_FLAGS_DEFAULT_SAMPLE_DURATION    =    0x08,
    TF_FLAGS_DEFAULT_SAMPLE_SIZE        =    0x10,
    TF_FLAGS_DEFAULT_SAMPLE_FLAGS       =    0x20,
    TF_FLAGS_DURATION_IS_EMPTY          = 0x10000,
    TF_FLAGS_DEFAULT_BASE_IS_MOOF       = 0x20000,
};

#define TF_OPTIONAL_FIELDS  (0x01 | 0x02 | 0x08 | 0x10 | 0x20)

enum tr_flags_t_
{
    TR_FLAGS_DATA_OFFSET        =  0x01,
    TR_FLAGS_FIRST_FLAGS        =  0x04,
    TR_FLAGS_SAMPLE_DURATION    = 0x100,
    TR_FLAGS_SAMPLE_SIZE        = 0x200,
    TR_FLAGS_SAMPLE_FLAGS       = 0x400,
    TR_FLAGS_CTS_OFFSETS        = 0x800,
};

/* macros for setup and clear flags in sample flags */
#define sample_depends_on_BIT0      (24)            /* LSB */
#define sample_depends_on_MASK      (0x3 << sample_depends_on_BIT0)
#define sample_depends_on_UNKNOWN   (~sample_depends_on_MASK)           /* clear */
#define sample_depends_on_YES       (0x1 << sample_depends_on_BIT0)     /* set */
#define sample_depends_on_NO        (0x2 << sample_depends_on_BIT0)     /* set */

#define SET_sample_depends_on_UNKNOWN(flags)   (flags) &= sample_depends_on_UNKNOWN
#define SET_sample_depends_on_YES(flags)    \
    do { SET_sample_depends_on_UNKNOWN(flags); (flags) |= sample_depends_on_YES; } while(0)
#define SET_sample_depends_on_NO(flags)     \
    do { SET_sample_depends_on_UNKNOWN(flags); (flags) |= sample_depends_on_NO; } while(0)
    

#define sample_is_depended_on_BIT0      (22)
#define sample_is_depended_on_MASK      (0x3 << sample_is_depended_on_BIT0)
#define sample_is_depended_on_UNKNOWN   (~sample_is_depended_on_MASK)
#define sample_is_depended_on_YES       (0x1 << sample_is_depended_on_BIT0)
#define sample_is_depended_on_NO        (0x2 << sample_is_depended_on_BIT0)

#define SET_sample_is_depended_on_UNKNOWN(flags)   (flags) &= sample_is_depended_on_UNKNOWN
#define SET_sample_is_depended_on_YES(flags)    \
    do { SET_sample_is_depended_on_UNKNOWN(flags); (flags) |= sample_is_depended_on_YES; } while (0)
#define SET_sample_is_depended_on_NO(flags)     \
    do { SET_sample_is_depended_on_UNKNOWN(flags); (flags) |= sample_is_depended_on_NO; } while (0)


#define sample_has_redundancy_BIT0      (20)
#define sample_has_redundancy_MASK      (0x3 << sample_has_redundancy_BIT0)
#define sample_has_redundancy_UNKNOWN   (~sample_has_redundancy_MASK)
#define sample_has_redundancy_YES       (0x1 << sample_has_redundancy_BIT0)
#define sample_has_redundancy_NO        (0x2 << sample_has_redundancy_BIT0)

#define SET_sample_has_redundancy_UNKNOWN(flags)   (flags) &= sample_has_redundancy_UNKNOWN
#define SET_sample_has_redundancy_YES(flags)    \
    do { SET_sample_has_redundancy_UNKNOWN(flags); (flags) |= sample_has_redundancy_YES; } while (0)
#define SET_sample_has_redundancy_NO(flags)     \
    do { SET_sample_has_redundancy_UNKNOWN(flags); (flags) |= sample_has_redundancy_NO; } while (0)


#define sample_padding_value_BIT0       (17)
#define sample_padding_value_MASK       (0x7 << sample_padding_value_BIT0)
#define sample_padding_value_ZERO       (~sample_padding_value_MASK)

#define SET_sample_padding_value_0(flags)   (flags) &= sample_padding_value_ZERO
#define SET_sample_padding_value(flags, val)  \
    do { SET_sample_padding_value_0(flags); (flags) |= ((val & 0x7) << sample_padding_value_BIT0); } while (0)

#define sample_is_difference_sample_BIT0   (16)
#define sample_is_difference_sample_MASK   (0x1 << sample_is_difference_sample_BIT0)
#define sample_is_difference_sample_YES    sample_is_difference_sample_MASK

#define SET_sample_is_difference_sample_YES(flags) (flags) |= sample_is_difference_sample_YES
#define SET_sample_is_difference_sample_NO(flags)  (flags) &= ~sample_is_difference_sample_YES

#define sample_degradation_priority_MASK   (0xFFFF)
#define sample_degradation_priority_ZERO   (~sample_degradation_priority_MASK)

#define SET_sample_degradation_priority_0(falgs)   (flags) &= sample_degradation_priority_ZERO
#define SET_sample_degradation_priority_value(flags, val) \
    do { SET_sample_degradation_priority_0(flags); (flags) |= (val & 0xFFFF; } while (0)


/* flags for all rap sequence */
#define SAMPLE_FLAGS_ALL_RAP (sample_depends_on_NO | sample_is_depended_on_NO  | sample_has_redundancy_NO)
/* flags for rap sequence in none all rap sequency */
#define SAMPLE_FLAGS_RAP     (sample_depends_on_NO | sample_is_depended_on_YES | sample_has_redundancy_NO)
/* flags for prediction sample */
#define SAMPLE_FLAGS_PREDICT (sample_depends_on_YES | sample_has_redundancy_NO |\
                              sample_is_difference_sample_YES)


typedef struct trex_t_
{
    uint32_t track_ID;
    uint32_t default_sample_description_index;
    uint32_t default_sample_duration;
    uint32_t default_sample_size;
    uint32_t default_sample_flags; /* setup by macros above */
} trex_t;

typedef struct tfhd_t_
{
    uint32_t tf_flags;
    uint32_t tf_flags_override;      /*!< override flags in the track fragment header */
    uint32_t track_ID;

    /* optional field pending on tf_flags */
    int64_t  base_data_offset;
    uint32_t sample_description_index;
    uint32_t default_sample_duration;
    uint32_t default_sample_size;
    uint32_t default_sample_flags;  /* setup by macros above */

    /** helpers */
    int64_t  base_data_offset_pos;  /* where the base_data_offset in snk is. */
    uint32_t sample_num;            /* assuming one traf a in moof and all run are continuous 
                                     * so sample_num here records a continuous run of  
                                     * a track samples in a moof */
    uint32_t samples_same_mode;
} tfhd_t;

typedef struct trun_t_
{
    uint32_t tr_flags;
    uint32_t tr_flags_override;      /*!< override flags in the track run box */
    uint32_t sample_count;

    /* optional field pending on tr_flags */
    int32_t  data_offset;
    uint64_t data_offset_pos;
    uint32_t first_sample_flags;

    /* all following are the place hold for now: the actual value is in
    *  separate list: dts_lst, size_lst, cts_offset_lst
    *  however, only the first sample_flags sre setup for now */
    uint32_t sample_duration;
    uint32_t sample_size;
    uint32_t sample_flags; /* setup by macros above */
    uint32_t sample_cts_offset;
    int64_t  first_sample_pos;  /* src position of 1st sample in trun */
} trun_t;

typedef struct tfra_entry_t_
{
    uint64_t time;
    uint64_t moof_offset;
    uint32_t traf_number; /* the size of type can be 8,16,24,32 */
    uint32_t trun_number;
    uint32_t sample_number;
} tfra_entry_t;

typedef struct tfra_t_
{
    uint32_t track_ID;
    uint8_t  length_size_of_traf_num;
    uint8_t  length_size_of_trun_num;
    uint8_t  length_size_of_sample_num;
    uint32_t number_of_entry;
    int8_t*  entry;
} tfra_t;

#ifdef __cplusplus
};
#endif

#endif /* __MP4_FRAG_H__ */
