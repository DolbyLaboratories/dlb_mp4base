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
 *  @file  mp4_muxer.h
 *  @brief Defines structures and functions of mp4muxer
 */

#ifndef __MP4_MUXER_H__
#define __MP4_MUXER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "mp4_ctrl.h"      /** mp4_ctrl_t_ */
#include "return_codes.h"  /** return codes */

#define DECE_FRAGFIX

/** I/O type */
enum
{
    EMA_MP4_IO_NONE = 0x00, /**< no I/O: invalid */
    EMA_MP4_IO_FILE = 0x01,
    EMA_MP4_IO_BUF  = 0x02,
    EMA_MP4_FRAG    = 0x04  /**< fragment output */
};

/** Fragment's sample flag mode */
enum
{
    SAMPLE_FLAG_IS_SAME_EXCEPT_FIRST = 0x0,
    SAMPLE_FLAG_IS_DIFFERENT         = 0x1,
    SAMPLE_FLAG_IS_SAME              = 0x2
};

/** Handle to mp4 muxer instance. */
typedef struct mp4_ctrl_t_ * mp4_muxer_handle_t;


/******************************************************************************
 * Initialization Calls
 ******************************************************************************/

/**
 *  @brief Creates a muxer instance.
 */
mp4_muxer_handle_t   /** @return The muxer instance handle. */
mp4_muxer_create (usr_cfg_mux_t *p_usr_cfg_mux    /** [in] Config info per muxing session. */
                 ,usr_cfg_es_t  *p_usr_cfg_ess    /** [in] Config info per elementary stream.
                                                           Should be NULL for non-editing uses. */
                 );

/**
 *  @brief Destroys a muxer instance.
 */
void
mp4_muxer_destroy (mp4_muxer_handle_t hmuxer  /** [in] Handle to muxer instance that will be destroyed. */
                  );

/**
 *  @brief Sets Audio Profile for mp4 file to be created.
 *
 *  Set the profile for specific audio codec to be muxed in the mp4 file.
 *  The profile is signaled in the IODS (initial object descriptor) to check on the decoder
 *  side whether the track can actually be played back.
 *  For instance in the case of AAC as codec the profile can define HE-AAC.
 */
void
mp4_muxer_set_audio_profile (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                            ,uint8_t            profile  /** [in] Audio Profile to set. */
                            );

/**
 *  @brief Sets Video Profile for mp4 file to be created.
 *
 *  Sets the type of video codec to be muxed in the mp4 file plus additional information.
 */
void
mp4_muxer_set_video_profile (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                            ,uint8_t            profile  /** [in] Video Profile to set. */
                            );

/**
 *  @brief Sets Object Descriptor Profile for mp4 file to be created.
 *
 *  Object Descriptor aggregates one or more elementary streams by means of their elementary
 *  stream descriptors and defines their logical dependencies.
 *  Object Descriptor Profile specifies the configurations of the object descriptor tool
 *  and the sync layer tool that are allowed.
 *  See ISO/IEC 14496-1 (MPEG-4 Systems).
 */
void
mp4_muxer_set_OD_profile (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                         ,uint8_t            profile  /** [in] OD Profile to set. */
                         );

/**
 *  @brief Sets Scene Profile for mp4 file to be created.
 */
void
mp4_muxer_set_scene_profile (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                            ,uint8_t            profile  /** [in] Scene Profile to set. */
                            );

/**
 *  @brief Sets Graphics Profile for mp4 file to be created.
 */
void
mp4_muxer_set_graphics_profile (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                               ,uint8_t            profile  /** [in] Graphics Profile to set. */
                               );

/**
 *  @brief Sets sample_description_index for tfhd box.
 *
 *  The sample_description_index used when writing the tfhd box is forced to the specified value.
 *  In addition, the flag TF_FLAGS_SAMPLE_DESCRIPTION_INDEX is set.
 *  If used, this function shall be called right after the call to mp4_muxer_add_track().
 */
void
mp4_muxer_set_tfhd_sample_description_index (mp4_muxer_handle_t hmuxer    /** [in] The muxer instance handle. */
                                            ,uint32_t           track_ID  /** [in] The track ID of the track to be modified. */
                                            ,uint32_t           sd_index  /** [in] The sample description index to set. */
                                            );

/**
 *  @brief Sets buffer to write data for mp4 file to.
 *
 *  The output buffer for the muxer is handled by the bbio interface.
 *  Before the muxing process can start you have to set a buffer instance which the muxer will use internally.
 *  Since data gets written into that buffer it is called sink.
 *  The sink must be set before anything can get written in or added to the mp4 file.
 */
void
mp4_muxer_set_sink (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                   ,bbio_handle_t      hsink    /** [in] Handle to data sink. Created by bbio interface. */
                   );


/**
 *  @brief Gets handle to data sink.
 *
 *  The data sink is used to write data for mp4 file to.
 */
bbio_handle_t   /** @return Handle to data sink. */
mp4_muxer_get_sink (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                   );


/******************************************************************************
 * Write / Add operations per track
 ******************************************************************************/

/**
 *  @brief Adds anew track to mp4 structure.
 *
 *  New track gets created and its identifier gets returned.
 *  This is the first operation to perform before anything can be added to a track.
 *  To set the properties of the new track you have to pass the correct parser to this
 *  function as well as a configuration struct characterizing the elementary stream to mux.
 *
 *  hparser: The muxer stores a reference to the given parser. No copy is made of
 *           the parser object. Thus, any changes made to the parser after calling
 *           this function may affect the later behaviour of the muxer. The caller
 *           is responsible for deallocating the parser object.
 */
uint32_t    /** @return Track ID created identifying track. */
mp4_muxer_add_track (mp4_muxer_handle_t  hmuxer         /** [in] The muxer instance handle. */
                    ,parser_handle_t     hparser        /** [in] The parser instance handle. */
                    ,usr_cfg_es_t       *p_usr_cfg_es   /** [in] Config struct characterizing elementary stream. */
                    );

/**
 *  @brief Gets track handle to track identifier.
 *
 *  The track handle is mandatory for some functions of the API. The track ID gets returned by mp4_muxer_add_track().
 */
track_handle_t  /** @return The track instance handle. */
mp4_muxer_get_track (mp4_muxer_handle_t hmuxer      /** [in] The muxer instance handle. */
                    ,uint32_t           track_ID    /** [in] Track ID identifying track. */
                    );

/**
 *  @brief Adds samples to specific track.
 */
int32_t   /** @return Error code. */
mp4_muxer_input_sample (track_handle_t      htrack   /** [in] The track instance handle. */
                       ,mp4_sample_handle_t hsample  /** [in] Handle to sample struct that gets added.*/
                       );

/**
 *  @brief Adds child atom to parent moov box.
 *
 *  A new atom gets added to the moov box and filled with a block of data right away.
 *  In order to characterize the new atom you have to put the desired Four-Character Code
 *  of the atom in the first 4 characters of the data block (referenced by p_data).
 */
int32_t   /** @return Error code. */
mp4_muxer_add_moov_child_atom (mp4_muxer_handle_t  hmuxer               /** [in] The muxer instance handle. */
                              ,const int8_t       *p_data               /** [in] Block of data to add to the new atom. */ 
                              ,      uint32_t      size                 /** [in] Size of p_data (data to be added) in bytes. */
                              ,      int8_t       *p_parent_box_type    /** [in] Four-Character Code of parent box. */
                              ,      uint32_t      track_ID             /** [in] ID of track atom gets added to. */
                              );

/**
 *  @brief Sets meta atom data to parent moov box.
 *
 *  A meta atom contains human-readable data with meta information regarding the file.
 */
void
mp4_muxer_set_moov_meta_atom_data (mp4_muxer_handle_t    hmuxer          /** [in] The muxer instance handle. */
                                  ,const int8_t          *p_xml_data
                                  ,const int8_t          *p_hdlr_type
                                  ,const int8_t          *p_hdlr_name
                                  ,const int8_t * const  *pp_items        /** [in] Array of item data to set. */
                                  ,const uint32_t        *p_item_sizes    /** [in] Array containing item data sizes (in bytes). */
                                  ,uint16_t               num_items       /** [in] Number of items to set. */
                                  );

void
mp4_muxer_set_footer_meta_atom_data (mp4_muxer_handle_t    hmuxer          /** [in] The muxer instance handle. */
                                    ,const int8_t          *p_xml_data
                                    ,const int8_t          *p_hdlr_type
                                    ,const int8_t          *p_hdlr_name
                                    ,const int8_t * const  *pp_items       /** [in] Array of item data to set. */
                                    ,const uint32_t        *p_item_sizes   /** [in] Array containing item data sizes (in bytes). */
                                    ,uint16_t              num_items       /** [in] Number of items to set. */
                                    );

/**
 *  @brief Adds edit list to specific track.
 *
 *  Edit lists contain information about times and durations that pieces of a track are
 *  to be presented during playback.
 */
void
mp4_muxer_add_to_track_edit_list (track_handle_t htrack     /** [in] The track instance handle. */
                                 ,uint64_t       duration   /** [in] Duration of playback. */
                                 ,int64_t        media_time /** [in] Start time of playback. */
                                 );

/**
 *  @brief Adds base media decode time to specific track
 *
 *  duration contain information about decode offset that a track is
 *  to be presented during playback.
 */
void 
mp4_muxer_add_to_track_tfdt(track_handle_t       htrack     /** [in] The track instance handle. */
                                 ,uint64_t       duration   /** [in] Duration of decode time offset. */
                                 );
/**
 *  @brief Encrypt specific track.
 */
int32_t   /** @return Error code. */
mp4_muxer_encrypt_track (track_handle_t         htrack      /** [in] Handle to track that gets encrypted. */
                        ,mp4_encryptor_handle_t hencryptor  /** [in] Handle to Encryptor holding the key for Encryption. */
                        );

/**
 *  @brief Returns average bitrate of specific track.
 */
uint32_t   /** @return Average bitrate of track. */
mp4_muxer_get_track_bitrate (track_handle_t htrack      /** [in] Handle to track whose bitrate gets returned. */
                            );


/******************************************************************************
 * Write / Add operations per muxing session
 ******************************************************************************/

/**
 *  @brief Writes top level, none media specific info.
 *
 *  This just writes the ftyp box for now.
 */
int32_t   /** @return Error code. */
mp4_muxer_output_hdrs (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                      );
/**
 *  @brief Writes the segment 'styp' header.
 */
int32_t  /** @return Error code. */
mp4_muxer_output_segment_hdrs (mp4_muxer_handle_t hmuxer  /** [in] The muxer instance handle. */
                              );


/**
 *  @brief Write common init segment (ftyp + moov).
 */
int32_t   /** @return Error code. */
mp4_muxer_output_init_segment (mp4_muxer_handle_t hmuxer          /** [in] The muxer instance handle. */
                              ,uint16_t *         p_video_width   /** [in] Optional array of video widths. */
                              ,uint16_t *         p_video_height  /** [in] Optional array of video heights. */
                              );


/**
 *  @brief Write moov and mdat boxes.
 *
 *  Should be called to finalize the mp4 file after all sub boxes have been added and written.
 */
int32_t   /** @return Error code. */
mp4_muxer_output_tracks (mp4_muxer_handle_t hmuxer   /** [in] The muxer instance handle. */
                        );

/**
 *  @brief Adds child atom to parent udta box.
 *
 *  A new atom gets added to the udta box and filled with a block of data right away.
 *  In order to characterize the new atom you have to put the desired Four-Character Code
 *  of the atom in the first 4 characters of the data block (referenced by p_data).
 */
int32_t   /** @return Error code. */
mp4_muxer_add_udta_child_atom (mp4_muxer_handle_t  hmuxer     /** [in] The muxer instance handle. */
                              ,const int8_t         *p_data   /** [in] Block of data to add to the new atom. */ 
                              ,      uint32_t      size       /** [in] Size of p_data (data to be added) in bytes. */
                              );

/**
 *  @brief Adds ainf atom to moov box.
 *
 *  A new ainf atom gets added to the moov box and filled with a block of data right away.
 */
void
mp4_muxer_add_moov_ainf_atom (mp4_muxer_handle_t  hmuxer     /** [in] The muxer instance handle. */
                             ,const int8_t         *p_data   /** [in] Block of data to add to the new atom. */ 
                             ,      uint32_t      size       /** [in] Size of p_data (data to be added) in bytes. */
                             );

/**
 *  @brief Adds a bloc atom.
 *
 *  A new bloc atom gets added and filled with a block of data right away.
 */
void
mp4_muxer_add_bloc_atom (mp4_muxer_handle_t  hmuxer     /** [in] The muxer instance handle. */
                        ,const int8_t         *p_data   /** [in] Block of data to add to the new atom. */ 
                        ,      uint32_t      size       /** [in] Size of p_data (data to be added) in bytes. */
                        );


/******************************************************************************
 * Misc
 ******************************************************************************/

/**
 *  @brief Sets callback function for progress updates.
 *
 *  See definition of progress_callback_t to see how progress notification callback function
 *  has to look like.
 */
void
mp4_muxer_set_progress_callback (mp4_muxer_handle_t   hmuxer      /** [in] The muxer instance handle. */
                                ,progress_callback_t  callback    /** [in] Progress notification callback. */
                                ,void                *p_instance  /** [in] User-defined data which is passed to the provided callback function. */
                                );

void
mp4_muxer_set_onwrite_next_frag_callback(mp4_muxer_handle_t hmuxer     /** [in] The muxer instance handle. */
                                        ,onwrite_callback_t callback   /** [in] onWrite notification callback. */
                                        ,void *p_instance              /** [in] User-defined data which is passed to the provided callback function. */
                                        );

#ifdef __cplusplus
};
#endif

#endif /* __MP4_MUXER_H__ */
