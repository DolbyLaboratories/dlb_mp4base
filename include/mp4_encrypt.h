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
 *  @file  mp4_encrypt.h
 *  @brief Defines structures and functions for encryption.
 */

#ifndef __MP4_ENCRYPT_H__
#define __MP4_ENCRYPT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "c99_inttypes.h"  /* uint8_t */

#define ENC_ID_SIZE 16

/** status snapshot for each subsample */
typedef struct enc_subsample_info_s
{
    uint8_t  initial_value[ENC_ID_SIZE]; 
    uint32_t num_clear_bytes;      /**< set to the number of clear bytes in the beginning of a subsample */
    uint32_t num_encrypted_bytes;  /**< set to the number of encrypted bytes in the end of a subsample */
} enc_sample_info_t, *enc_sample_info_handle_t;

/** encryption object */
typedef struct mp4_encryptor_s
{
    int32_t (*encrypt) (struct mp4_encryptor_s *self, uint8_t *inbuf, uint8_t *outbuf, uint32_t len, enc_sample_info_handle_t info);
    int32_t (*update_iv) (struct mp4_encryptor_s *self);
    void (*destroy) (struct mp4_encryptor_s *self);
    uint8_t keyId[ENC_ID_SIZE];
    uint8_t key[ENC_ID_SIZE];
    uint8_t initial_value[ENC_ID_SIZE];
    uint32_t iv_size;
    void *data;      /**< opaque data of the encryption algorithm */
} * mp4_encryptor_handle_t;

/** encryption algorithm ID */
typedef enum encryption_alg_id_t_
{
    NO_ENCRYPTION = 0,
    AES_CTR_128   = 1
} encryption_alg_id_t;

/** generic destructor */
void
destroy_encryptor (mp4_encryptor_handle_t enc_ptr);

/** generic constructor */
mp4_encryptor_handle_t
create_encryptor (const uint8_t keyId[ENC_ID_SIZE], const uint8_t key[ENC_ID_SIZE], int32_t alg_id, uint32_t iv_size);

#ifdef __cplusplus
};
#endif

#endif /* __MP4_ENCRYPT_H__ */
