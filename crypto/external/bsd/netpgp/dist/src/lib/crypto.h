/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 */

#ifndef OPS_CRYPTO_H
#define OPS_CRYPTO_H

#include "keyring.h"
#include "packet.h"
#include "packet-parse.h"

#include <openssl/dsa.h>

#define OPS_MIN_HASH_SIZE	16

typedef void    __ops_hash_init_t(__ops_hash_t *);
typedef void __ops_hash_add_t(__ops_hash_t *, const unsigned char *, unsigned);
typedef unsigned __ops_hash_finish_t(__ops_hash_t *, unsigned char *);

/** _ops_hash_t */
struct _ops_hash_t {
	__ops_hash_algorithm_t algorithm;
	size_t          size;
	const char     *name;
	__ops_hash_init_t *init;
	__ops_hash_add_t *add;
	__ops_hash_finish_t *finish;
	void           *data;
};

typedef void __ops_crypt_set_iv_t(__ops_crypt_t *, const unsigned char *);
typedef void __ops_crypt_set_key_t(__ops_crypt_t *, const unsigned char *);
typedef void    __ops_crypt_init_t(__ops_crypt_t *);
typedef void    __ops_crypt_resync_t(__ops_crypt_t *);
typedef void __ops_crypt_block_encrypt_t(__ops_crypt_t *, void *, const void *);
typedef void __ops_crypt_block_decrypt_t(__ops_crypt_t *, void *, const void *);
typedef void __ops_crypt_cfb_encrypt_t(__ops_crypt_t *, void *, const void *, size_t);
typedef void __ops_crypt_cfb_decrypt_t(__ops_crypt_t *, void *, const void *, size_t);
typedef void    __ops_crypt_finish_t(__ops_crypt_t *);

/** _ops_crypt_t */
struct _ops_crypt_t {
	__ops_symmetric_algorithm_t algorithm;
	size_t          blocksize;
	size_t          keysize;
	__ops_crypt_set_iv_t *set_iv;	/* Call this before decrypt init! */
	__ops_crypt_set_key_t *set_key;	/* Call this before init! */
	__ops_crypt_init_t *base_init;
	__ops_crypt_resync_t *decrypt_resync;
	/* encrypt/decrypt one block  */
	__ops_crypt_block_encrypt_t *block_encrypt;
	__ops_crypt_block_decrypt_t *block_decrypt;

	/* Standard CFB encrypt/decrypt (as used by Sym Enc Int Prot packets) */
	__ops_crypt_cfb_encrypt_t *cfb_encrypt;
	__ops_crypt_cfb_decrypt_t *cfb_decrypt;

	__ops_crypt_finish_t *decrypt_finish;
	unsigned char   iv[OPS_MAX_BLOCK_SIZE];
	unsigned char   civ[OPS_MAX_BLOCK_SIZE];
	unsigned char   siv[OPS_MAX_BLOCK_SIZE];	/* Needed for weird v3
							 * resync */
	unsigned char   key[OPS_MAX_KEY_SIZE];
	int             num;	/* Offset - see openssl _encrypt doco */
	void           *encrypt_key;
	void           *decrypt_key;
};

void            __ops_crypto_init(void);
void            __ops_crypto_finish(void);
void            __ops_hash_md5(__ops_hash_t *);
void            __ops_hash_sha1(__ops_hash_t *);
void            __ops_hash_sha256(__ops_hash_t *);
void            __ops_hash_sha512(__ops_hash_t *);
void            __ops_hash_sha384(__ops_hash_t *);
void            __ops_hash_sha224(__ops_hash_t *);
void            __ops_hash_any(__ops_hash_t *, __ops_hash_algorithm_t);
__ops_hash_algorithm_t __ops_hash_algorithm_from_text(const char *);
const char     *__ops_text_from_hash(__ops_hash_t *);
unsigned        __ops_hash_size(__ops_hash_algorithm_t);
unsigned __ops_hash(unsigned char *, __ops_hash_algorithm_t, const void *, size_t);

void            __ops_hash_add_int(__ops_hash_t *, unsigned, unsigned);

bool __ops_dsa_verify(const unsigned char *, size_t, const __ops_dsa_signature_t *, const __ops_dsa_public_key_t *);

int __ops_rsa_public_decrypt(unsigned char *, const unsigned char *, size_t, const __ops_rsa_public_key_t *);
int __ops_rsa_public_encrypt(unsigned char *, const unsigned char *, size_t, const __ops_rsa_public_key_t *);

int __ops_rsa_private_encrypt(unsigned char *, const unsigned char *, size_t, const __ops_rsa_secret_key_t *, const __ops_rsa_public_key_t *);
int __ops_rsa_private_decrypt(unsigned char *, const unsigned char *, size_t, const __ops_rsa_secret_key_t *, const __ops_rsa_public_key_t *);

unsigned        __ops_block_size(__ops_symmetric_algorithm_t);
unsigned        __ops_key_size(__ops_symmetric_algorithm_t);

int __ops_decrypt_data(__ops_content_tag_t, __ops_region_t *, __ops_parse_info_t *);

int             __ops_crypt_any(__ops_crypt_t *, __ops_symmetric_algorithm_t);
void            __ops_decrypt_init(__ops_crypt_t *);
void            __ops_encrypt_init(__ops_crypt_t *);
size_t __ops_decrypt_se(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_encrypt_se(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_decrypt_se_ip(__ops_crypt_t *, void *, const void *, size_t);
size_t __ops_encrypt_se_ip(__ops_crypt_t *, void *, const void *, size_t);
bool   __ops_is_sa_supported(__ops_symmetric_algorithm_t);

void __ops_reader_push_decrypt(__ops_parse_info_t *, __ops_crypt_t *, __ops_region_t *);
void            __ops_reader_pop_decrypt(__ops_parse_info_t *);

/* Hash everything that's read */
void            __ops_reader_push_hash(__ops_parse_info_t *, __ops_hash_t *);
void            __ops_reader_pop_hash(__ops_parse_info_t *);

int __ops_decrypt_and_unencode_mpi(unsigned char *, unsigned, const BIGNUM *, const __ops_secret_key_t *);
bool __ops_rsa_encrypt_mpi(const unsigned char *, const size_t, const __ops_public_key_t *, __ops_pk_session_key_parameters_t *);


/* Encrypt everything that's written */
struct __ops_key_data;
void __ops_writer_push_encrypt(__ops_create_info_t *, const struct __ops_key_data *);

bool   __ops_encrypt_file(const char *, const char *, const __ops_keydata_t *, const bool, const bool);
bool   __ops_decrypt_file(const char *, const char *, __ops_keyring_t *, const bool, const bool, __ops_parse_cb_t *);

/* Keys */
bool   __ops_rsa_generate_keypair(const int, const unsigned long, __ops_keydata_t *);
__ops_keydata_t  *__ops_rsa_create_selfsigned_keypair(const int, const unsigned long, __ops_user_id_t *);

int             __ops_dsa_size(const __ops_dsa_public_key_t *);
DSA_SIG        *__ops_dsa_sign(unsigned char *, unsigned, const __ops_dsa_secret_key_t *, const __ops_dsa_public_key_t *);

#endif
