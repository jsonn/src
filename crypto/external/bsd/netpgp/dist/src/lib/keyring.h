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

#ifndef OPS_KEYRING_H
#define OPS_KEYRING_H

#include "packet.h"
#include "packet-parse.h"

typedef struct __ops_keydata __ops_keydata_t;

/** \struct __ops_keyring_t
 * A keyring
 */

typedef struct __ops_keyring_t {
	int             nkeys;
	              /*while we are constructing a key, this is the offset */
	int             nkeys_allocated;
	__ops_keydata_t  *keys;
}               __ops_keyring_t;

const __ops_keydata_t *__ops_keyring_find_key_by_id(const __ops_keyring_t *, const unsigned char *);
const __ops_keydata_t *__ops_keyring_find_key_by_userid(const __ops_keyring_t *, const char *);
void            __ops_keydata_free(__ops_keydata_t *);
void            __ops_keyring_free(__ops_keyring_t *);
void            __ops_dump_keyring(const __ops_keyring_t *);
const __ops_public_key_t *__ops_get_public_key_from_data(const __ops_keydata_t *);
bool   __ops_is_key_secret(const __ops_keydata_t *);
const __ops_secret_key_t *__ops_get_secret_key_from_data(const __ops_keydata_t *);
__ops_secret_key_t *__ops_get_writable_secret_key_from_data(__ops_keydata_t *);
__ops_secret_key_t *__ops_decrypt_secret_key_from_data(const __ops_keydata_t *, const char *);

bool   __ops_keyring_read_from_file(__ops_keyring_t *, const bool, const char *);

char           *__ops_malloc_passphrase(char *);

void            __ops_keyring_list(const __ops_keyring_t *);

void            __ops_set_secret_key(__ops_parser_content_union_t *, const __ops_keydata_t *);

const unsigned char *__ops_get_key_id(const __ops_keydata_t *);
unsigned        __ops_get_user_id_count(const __ops_keydata_t *);
const unsigned char *__ops_get_user_id(const __ops_keydata_t *, unsigned);
bool   __ops_is_key_supported(const __ops_keydata_t *);
const __ops_keydata_t *__ops_keyring_get_key_by_index(const __ops_keyring_t *, int);

__ops_user_id_t  *__ops_add_userid_to_keydata(__ops_keydata_t *, const __ops_user_id_t *);
__ops_packet_t   *__ops_add_packet_to_keydata(__ops_keydata_t *, const __ops_packet_t *);
void            __ops_add_signed_userid_to_keydata(__ops_keydata_t *, const __ops_user_id_t *, const __ops_packet_t *);

bool   __ops_add_selfsigned_userid_to_keydata(__ops_keydata_t *, __ops_user_id_t *);

__ops_keydata_t  *__ops_keydata_new(void);
void            __ops_keydata_init(__ops_keydata_t *, const __ops_content_tag_t);

void            __ops_copy_userid(__ops_user_id_t *, const __ops_user_id_t *);
void            __ops_copy_packet(__ops_packet_t *, const __ops_packet_t *);
unsigned        __ops_get_keydata_content_type(const __ops_keydata_t *);

int		__ops_parse_and_accumulate(__ops_keyring_t *, __ops_parse_info_t *);

void            __ops_print_public_keydata(const __ops_keydata_t *);
void            __ops_print_public_key(const __ops_public_key_t *);

void            __ops_print_secret_keydata(const __ops_keydata_t *);
void            __ops_list_packets(char *, bool, __ops_keyring_t *, __ops_parse_cb_t *);

int		__ops_export_key(const __ops_keydata_t *, unsigned char *);

#endif /* OPS_KEYRING_H */
