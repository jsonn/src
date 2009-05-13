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
 * This file contains the base functions used by the writers.
 */
#include "config.h"

#include <sys/types.h>

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#include "create.h"
#include "writer.h"
#include "keyring.h"
#include "signature.h"
#include "packet.h"
#include "packet-parse.h"

#include "readerwriter.h"
#include "memory.h"
#include "netpgpdefs.h"
#include "keyring_local.h"
#include "version.h"

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif

#include <fcntl.h>

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif


/*
 * return true if OK, otherwise false
 */
static bool 
base_write(const void *src, unsigned length, __ops_create_info_t * info)
{
	return info->winfo.writer(src, length, &info->errors, &info->winfo);
}

/**
 * \ingroup Core_WritePackets
 *
 * \param src
 * \param length
 * \param info
 * \return 1 if OK, otherwise 0
 */

bool 
__ops_write(const void *src, unsigned length, __ops_create_info_t * info)
{
	return base_write(src, length, info);
}

/**
 * \ingroup Core_WritePackets
 * \param n
 * \param length
 * \param info
 * \return true if OK, otherwise false
 */

bool 
__ops_write_scalar(unsigned n, unsigned length, __ops_create_info_t *info)
{
	unsigned char   c[1];

	while (length-- > 0) {
		c[0] = n >> (length * 8);
		if (!base_write(c, 1, info)) {
			return false;
		}
	}
	return true;
}

/**
 * \ingroup Core_WritePackets
 * \param bn
 * \param info
 * \return 1 if OK, otherwise 0
 */

bool 
__ops_write_mpi(const BIGNUM * bn, __ops_create_info_t * info)
{
	unsigned char   buf[NETPGP_BUFSIZ];
	unsigned	bits = (unsigned)BN_num_bits(bn);

	assert(bits <= 65535);
	BN_bn2bin(bn, buf);
	return __ops_write_scalar(bits, 2, info) &&
		__ops_write(buf, (bits + 7) / 8, info);
}

/**
 * \ingroup Core_WritePackets
 * \param tag
 * \param info
 * \return 1 if OK, otherwise 0
 */

bool 
__ops_write_ptag(__ops_content_tag_t tag, __ops_create_info_t * info)
{
	unsigned char   c[1];

	c[0] = tag | OPS_PTAG_ALWAYS_SET | OPS_PTAG_NEW_FORMAT;
	return base_write(c, 1, info);
}

/**
 * \ingroup Core_WritePackets
 * \param length
 * \param info
 * \return 1 if OK, otherwise 0
 */

bool 
__ops_write_length(unsigned length, __ops_create_info_t * info)
{
	unsigned char   c[2];

	if (length < 192) {
		c[0] = length;
		return base_write(c, 1, info);
	}
	if (length < 8192 + 192) {
		c[0] = ((length - 192) >> 8) + 192;
		c[1] = (length - 192) % 256;
		return base_write(c, 2, info);
	}
	return __ops_write_scalar(0xff, 1, info) && __ops_write_scalar(length, 4, info);
}

/*
 * Note that we finalise from the top down, so we don't use writers below
 * that have already been finalised
 */
bool 
writer_info_finalise(__ops_error_t ** errors, __ops_writer_info_t * winfo)
{
	bool   ret = true;

	if (winfo->finaliser) {
		ret = winfo->finaliser(errors, winfo);
		winfo->finaliser = NULL;
	}
	if (winfo->next && !writer_info_finalise(errors, winfo->next)) {
		winfo->finaliser = NULL;
		return false;
	}
	return ret;
}

void 
writer_info_delete(__ops_writer_info_t * winfo)
{
	/* we should have finalised before deleting */
	assert(!winfo->finaliser);
	if (winfo->next) {
		writer_info_delete(winfo->next);
		free(winfo->next);
		winfo->next = NULL;
	}
	if (winfo->destroyer) {
		winfo->destroyer(winfo);
		winfo->destroyer = NULL;
	}
	winfo->writer = NULL;
}

/**
 * \ingroup Core_Writers
 *
 * Set a writer in info. There should not be another writer set.
 *
 * \param info The info structure
 * \param writer
 * \param finaliser
 * \param destroyer
 * \param arg The argument for the writer and destroyer
 */
void 
__ops_writer_set(__ops_create_info_t * info,
	       __ops_writer_t * writer,
	       __ops_writer_finaliser_t * finaliser,
	       __ops_writer_destroyer_t * destroyer,
	       void *arg)
{
	assert(!info->winfo.writer);
	info->winfo.writer = writer;
	info->winfo.finaliser = finaliser;
	info->winfo.destroyer = destroyer;
	info->winfo.arg = arg;
}

/**
 * \ingroup Core_Writers
 *
 * Push a writer in info. There must already be another writer set.
 *
 * \param info The info structure
 * \param writer
 * \param finaliser
 * \param destroyer
 * \param arg The argument for the writer and destroyer
 */
void 
__ops_writer_push(__ops_create_info_t * info,
		__ops_writer_t * writer,
		__ops_writer_finaliser_t * finaliser,
		__ops_writer_destroyer_t * destroyer,
		void *arg)
{
	__ops_writer_info_t *copy = calloc(1, sizeof(*copy));

	assert(info->winfo.writer);
	*copy = info->winfo;
	info->winfo.next = copy;

	info->winfo.writer = writer;
	info->winfo.finaliser = finaliser;
	info->winfo.destroyer = destroyer;
	info->winfo.arg = arg;
}

void 
__ops_writer_pop(__ops_create_info_t * info)
{
	__ops_writer_info_t *next;

	/* Make sure the finaliser has been called. */
	assert(!info->winfo.finaliser);
	/* Make sure this is a stacked writer */
	assert(info->winfo.next);
	if (info->winfo.destroyer) {
		info->winfo.destroyer(&info->winfo);
	}

	next = info->winfo.next;
	info->winfo = *next;

	free(next);
}

/**
 * \ingroup Core_Writers
 *
 * Close the writer currently set in info.
 *
 * \param info The info structure
 */
bool 
__ops_writer_close(__ops_create_info_t * info)
{
	bool   ret = writer_info_finalise(&info->errors, &info->winfo);

	writer_info_delete(&info->winfo);

	return ret;
}

/**
 * \ingroup Core_Writers
 *
 * Get the arg supplied to __ops_create_info_set_writer().
 *
 * \param winfo The writer_info structure
 * \return The arg
 */
void           *
__ops_writer_get_arg(__ops_writer_info_t * winfo)
{
	return winfo->arg;
}

/**
 * \ingroup Core_Writers
 *
 * Write to the next writer down in the stack.
 *
 * \param src The data to write.
 * \param length The length of src.
 * \param errors A place to store errors.
 * \param winfo The writer_info structure.
 * \return Success - if false, then errors should contain the error.
 */
bool 
__ops_stacked_write(const void *src, unsigned length,
		  __ops_error_t ** errors, __ops_writer_info_t * winfo)
{
	return winfo->next->writer(src, length, errors, winfo->next);
}

/**
 * \ingroup Core_Writers
 *
 * Free the arg. Many writers just have a calloc()ed lump of storage, this
 * function releases it.
 *
 * \param winfo the info structure.
 */
void 
__ops_writer_generic_destroyer(__ops_writer_info_t * winfo)
{
	free(__ops_writer_get_arg(winfo));
}

/**
 * \ingroup Core_Writers
 *
 * A writer that just writes to the next one down. Useful for when you
 * want to insert just a finaliser into the stack.
 */
bool 
__ops_writer_passthrough(const unsigned char *src,
		       unsigned length,
		       __ops_error_t ** errors,
		       __ops_writer_info_t * winfo)
{
	return __ops_stacked_write(src, length, errors, winfo);
}

/**************************************************************************/

/**
 * \struct dash_escaped_t
 */
typedef struct {
	unsigned   seen_nl:1;
	unsigned   seen_cr:1;
	__ops_create_signature_t *sig;
	__ops_memory_t   *trailing;
}               dash_escaped_t;

static bool 
dash_escaped_writer(const unsigned char *src,
		    unsigned length,
		    __ops_error_t ** errors,
		    __ops_writer_info_t * winfo)
{
	dash_escaped_t *dash = __ops_writer_get_arg(winfo);
	unsigned        n;

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;
		fprintf(stderr, "dash_escaped_writer writing %d:\n", length);
		for (i = 0; i < length; i++) {
			fprintf(stderr, "0x%02x ", src[i]);
			if (!((i + 1) % 16)) {
				fprintf(stderr, "\n");
			} else if (!((i + 1) % 8)) {
				fprintf(stderr, "  ");
			}
		}
		fprintf(stderr, "\n");
	}
	/* XXX: make this efficient */
	for (n = 0; n < length; ++n) {
		unsigned        l;

		if (dash->seen_nl) {
			if (src[n] == '-' && !__ops_stacked_write("- ", 2, errors, winfo)) {
				return false;
			}
			dash->seen_nl = false;
		}
		dash->seen_nl = src[n] == '\n';

		if (dash->seen_nl && !dash->seen_cr) {
			if (!__ops_stacked_write("\r", 1, errors, winfo)) {
				return false;
			}
			__ops_signature_add_data(dash->sig, "\r", 1);
		}
		dash->seen_cr = src[n] == '\r';

		if (!__ops_stacked_write(&src[n], 1, errors, winfo)) {
			return false;
		}

		/* trailing whitespace isn't included in the signature */
		if (src[n] == ' ' || src[n] == '\t') {
			__ops_memory_add(dash->trailing, &src[n], 1);
		} else {
			if ((l = __ops_memory_get_length(dash->trailing)) != 0) {
				if (!dash->seen_nl && !dash->seen_cr) {
					__ops_signature_add_data(dash->sig,
					 __ops_memory_get_data(dash->trailing),
							       l);
				}
				__ops_memory_clear(dash->trailing);
			}
			__ops_signature_add_data(dash->sig, &src[n], 1);
		}
	}

	return true;
}

/**
 * \param winfo
 */
static void 
dash_escaped_destroyer(__ops_writer_info_t * winfo)
{
	dash_escaped_t *dash = __ops_writer_get_arg(winfo);

	__ops_memory_free(dash->trailing);
	free(dash);
}

/**
 * \ingroup Core_WritersNext
 * \brief Push Clearsigned Writer onto stack
 * \param info
 * \param sig
 */
bool 
__ops_writer_push_clearsigned(__ops_create_info_t * info,
			    __ops_create_signature_t * sig)
{
	static char     header[] = "-----BEGIN PGP SIGNED MESSAGE-----\r\nHash: ";
	const char     *hash = __ops_text_from_hash(__ops_signature_get_hash(sig));
	dash_escaped_t *dash = calloc(1, sizeof(*dash));

	bool   rtn;

	rtn = (__ops_write(header, sizeof(header) - 1, info) &&
		__ops_write(hash, strlen(hash), info) &&
		__ops_write("\r\n\r\n", 4, info));

	if (rtn == false) {
		OPS_ERROR(&info->errors, OPS_E_W, "Error pushing clearsigned header");
		free(dash);
		return rtn;
	}
	dash->seen_nl = true;
	dash->sig = sig;
	dash->trailing = __ops_memory_new();
	__ops_writer_push(info, dash_escaped_writer, NULL, dash_escaped_destroyer, dash);
	return rtn;
}


/**
 * \struct base64_t
 */
typedef struct {
	unsigned        pos;
	unsigned char   t;
	unsigned        checksum;
}               base64_t;

static char     b64map[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool 
base64_writer(const unsigned char *src,
	      unsigned length, __ops_error_t ** errors,
	      __ops_writer_info_t * winfo)
{
	base64_t   *base64 = __ops_writer_get_arg(winfo);
	unsigned        n;

	for (n = 0; n < length;) {
		base64->checksum = __ops_crc24(base64->checksum, src[n]);
		if (base64->pos == 0) {
			/* XXXXXX00 00000000 00000000 */
			if (!__ops_stacked_write(&b64map[(unsigned)src[n] >> 2], 1, errors, winfo)) {
				return false;
			}

			/* 000000XX xxxx0000 00000000 */
			base64->t = (src[n++] & 3) << 4;
			base64->pos = 1;
		} else if (base64->pos == 1) {
			/* 000000xx XXXX0000 00000000 */
			base64->t += (unsigned)src[n] >> 4;
			if (!__ops_stacked_write(&b64map[base64->t], 1, errors, winfo)) {
				return false;
			}

			/* 00000000 0000XXXX xx000000 */
			base64->t = (src[n++] & 0xf) << 2;
			base64->pos = 2;
		} else if (base64->pos == 2) {
			/* 00000000 0000xxxx XX000000 */
			base64->t += (unsigned)src[n] >> 6;
			if (!__ops_stacked_write(&b64map[base64->t], 1, errors, winfo)) {
				return false;
			}

			/* 00000000 00000000 00XXXXXX */
			if (!__ops_stacked_write(&b64map[src[n++] & 0x3f], 1, errors, winfo)) {
				return false;
			}

			base64->pos = 0;
		}
	}

	return true;
}

static bool 
signature_finaliser(__ops_error_t ** errors,
		    __ops_writer_info_t * winfo)
{
	base64_t   *base64 = __ops_writer_get_arg(winfo);
	static char     trailer[] = "\r\n-----END PGP SIGNATURE-----\r\n";
	unsigned char   c[3];

	if (base64->pos) {
		if (!__ops_stacked_write(&b64map[base64->t], 1, errors, winfo)) {
			return false;
		}
		if (base64->pos == 1 && !__ops_stacked_write("==", 2, errors, winfo)) {
			return false;
		}
		if (base64->pos == 2 && !__ops_stacked_write("=", 1, errors, winfo)) {
			return false;
		}
	}
	/* Ready for the checksum */
	if (!__ops_stacked_write("\r\n=", 3, errors, winfo)) {
		return false;
	}

	base64->pos = 0;		/* get ready to write the checksum */

	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, winfo)) {
		return false;
	}

	return __ops_stacked_write(trailer, sizeof(trailer) - 1, errors, winfo);
}

/**
 * \struct linebreak_t
 */
typedef struct {
	unsigned        pos;
}               linebreak_t;

#define BREAKPOS	76

static bool 
linebreak_writer(const unsigned char *src,
		 unsigned length,
		 __ops_error_t ** errors,
		 __ops_writer_info_t * winfo)
{
	linebreak_t *linebreak = __ops_writer_get_arg(winfo);
	unsigned        n;

	for (n = 0; n < length; ++n, ++linebreak->pos) {
		if (src[n] == '\r' || src[n] == '\n') {
			linebreak->pos = 0;
		}

		if (linebreak->pos == BREAKPOS) {
			if (!__ops_stacked_write("\r\n", 2, errors, winfo)) {
				return false;
			}
			linebreak->pos = 0;
		}
		if (!__ops_stacked_write(&src[n], 1, errors, winfo)) {
			return false;
		}
	}

	return true;
}

/**
 * \ingroup Core_WritersNext
 * \brief Push armoured signature on stack
 * \param info
 */
bool 
__ops_writer_switch_to_armoured_signature(__ops_create_info_t * info)
{
	static const char     header[] =
"\r\n-----BEGIN PGP SIGNATURE-----\r\nVersion: " NETPGP_VERSION_STRING "\r\n\r\n";
	base64_t   *base64;

	__ops_writer_pop(info);
	if (__ops_write(header, sizeof(header) - 1, info) == false) {
		OPS_ERROR(&info->errors, OPS_E_W, "Error switching to armoured signature");
		return false;
	}
	__ops_writer_push(info, linebreak_writer, NULL, __ops_writer_generic_destroyer,
			calloc(1, sizeof(linebreak_t)));

	base64 = calloc(1, sizeof(*base64));
	if (!base64) {
		OPS_MEMORY_ERROR(&info->errors);
		return false;
	}
	base64->checksum = CRC24_INIT;
	__ops_writer_push(info, base64_writer, signature_finaliser,
			__ops_writer_generic_destroyer, base64);
	return true;
}

static bool 
armoured_message_finaliser(__ops_error_t ** errors,
			   __ops_writer_info_t * winfo)
{
	/* TODO: This is same as signature_finaliser apart from trailer. */
	base64_t   *base64 = __ops_writer_get_arg(winfo);
	static char     trailer[] = "\r\n-----END PGP MESSAGE-----\r\n";
	unsigned char   c[3];

	if (base64->pos) {
		if (!__ops_stacked_write(&b64map[base64->t], 1, errors, winfo)) {
			return false;
		}
		if (base64->pos == 1 && !__ops_stacked_write("==", 2, errors, winfo)) {
			return false;
		}
		if (base64->pos == 2 && !__ops_stacked_write("=", 1, errors, winfo)) {
			return false;
		}
	}
	/* Ready for the checksum */
	if (!__ops_stacked_write("\r\n=", 3, errors, winfo)) {
		return false;
	}

	base64->pos = 0;		/* get ready to write the checksum */

	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, winfo)) {
		return false;
	}

	return __ops_stacked_write(trailer, sizeof(trailer) - 1, errors, winfo);
}

/**
 \ingroup Core_WritersNext
 \brief Write a PGP MESSAGE
 \todo replace with generic function
*/
void 
__ops_writer_push_armoured_message(__ops_create_info_t * info)
{
	static char     header[] = "-----BEGIN PGP MESSAGE-----\r\n";

	base64_t   *base64;

	__ops_write(header, sizeof(header) - 1, info);
	__ops_write("\r\n", 2, info);
	base64 = calloc(1, sizeof(*base64));
	base64->checksum = CRC24_INIT;
	__ops_writer_push(info, base64_writer, armoured_message_finaliser, __ops_writer_generic_destroyer, base64);
}

static bool 
armoured_finaliser(__ops_armor_type_t type, __ops_error_t ** errors,
		   __ops_writer_info_t * winfo)
{
	static char     tail_public_key[] = "\r\n-----END PGP PUBLIC KEY BLOCK-----\r\n";
	static char     tail_private_key[] = "\r\n-----END PGP PRIVATE KEY BLOCK-----\r\n";

	char           *tail = NULL;
	unsigned int    sz_tail = 0;
	base64_t   *base64;
	unsigned char   c[3];

	switch (type) {
	case OPS_PGP_PUBLIC_KEY_BLOCK:
		tail = tail_public_key;
		sz_tail = sizeof(tail_public_key) - 1;
		break;

	case OPS_PGP_PRIVATE_KEY_BLOCK:
		tail = tail_private_key;
		sz_tail = sizeof(tail_private_key) - 1;
		break;

	default:
		assert(/* CONSTCOND */0);
	}

	base64 = __ops_writer_get_arg(winfo);

	if (base64->pos) {
		if (!__ops_stacked_write(&b64map[base64->t], 1, errors, winfo)) {
			return false;
		}
		if (base64->pos == 1 && !__ops_stacked_write("==", 2, errors, winfo)) {
			return false;
		}
		if (base64->pos == 2 && !__ops_stacked_write("=", 1, errors, winfo)) {
			return false;
		}
	}
	/* Ready for the checksum */
	if (!__ops_stacked_write("\r\n=", 3, errors, winfo)) {
		return false;
	}

	base64->pos = 0;		/* get ready to write the checksum */

	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, winfo)) {
		return false;
	}

	return __ops_stacked_write(tail, sz_tail, errors, winfo);
}

static bool 
armoured_public_key_finaliser(__ops_error_t ** errors,
			      __ops_writer_info_t * winfo)
{
	return armoured_finaliser(OPS_PGP_PUBLIC_KEY_BLOCK, errors, winfo);
}

static bool 
armoured_private_key_finaliser(__ops_error_t ** errors,
			       __ops_writer_info_t * winfo)
{
	return armoured_finaliser(OPS_PGP_PRIVATE_KEY_BLOCK, errors, winfo);
}

/* \todo use this for other armoured types */
/**
 \ingroup Core_WritersNext
 \brief Push Armoured Writer on stack (generic)
*/
void 
__ops_writer_push_armoured(__ops_create_info_t * info, __ops_armor_type_t type)
{
	static char     hdr_public_key[] =
"-----BEGIN PGP PUBLIC KEY BLOCK-----\r\nVersion: " NETPGP_VERSION_STRING "\r\n\r\n";
	static char     hdr_private_key[] =
"-----BEGIN PGP PRIVATE KEY BLOCK-----\r\nVersion: " NETPGP_VERSION_STRING "\r\n\r\n";

	char           *header = NULL;
	unsigned int    sz_hdr = 0;
	bool(*finaliser) (__ops_error_t ** errors, __ops_writer_info_t * winfo);
	base64_t   *base64;

	finaliser = NULL;
	switch (type) {
	case OPS_PGP_PUBLIC_KEY_BLOCK:
		header = hdr_public_key;
		sz_hdr = sizeof(hdr_public_key) - 1;
		finaliser = armoured_public_key_finaliser;
		break;

	case OPS_PGP_PRIVATE_KEY_BLOCK:
		header = hdr_private_key;
		sz_hdr = sizeof(hdr_private_key) - 1;
		finaliser = armoured_private_key_finaliser;
		break;

	default:
		assert(/* CONSTCOND */0);
	}

	__ops_write(header, sz_hdr, info);

	__ops_writer_push(info, linebreak_writer, NULL, __ops_writer_generic_destroyer,
			calloc(1, sizeof(linebreak_t)));

	base64 = calloc(1, sizeof(*base64));
	base64->checksum = CRC24_INIT;
	__ops_writer_push(info, base64_writer, finaliser, __ops_writer_generic_destroyer, base64);
}

/**************************************************************************/

typedef struct {
	__ops_crypt_t    *crypt;
	int             free_crypt;
}               crypt_t;

/*
 * This writer simply takes plaintext as input,
 * encrypts it with the given key
 * and outputs the resulting encrypted text
 */
static bool 
encrypt_writer(const unsigned char *src,
	       unsigned length,
	       __ops_error_t ** errors,
	       __ops_writer_info_t * winfo)
{

#define BUFSZ 1024		/* arbitrary number */
	unsigned char   encbuf[BUFSZ];
	unsigned        remaining = length;
	unsigned        done = 0;

	crypt_t    *pgp_encrypt = (crypt_t *) __ops_writer_get_arg(winfo);

	if (!__ops_is_sa_supported(pgp_encrypt->crypt->algorithm)) {
		assert(/* CONSTCOND */0);/* \todo proper error handling */
	}

	while (remaining) {
		unsigned        len = remaining < BUFSZ ? remaining : BUFSZ;
		/* memcpy(buf,src,len); // \todo copy needed here? */

		pgp_encrypt->crypt->cfb_encrypt(pgp_encrypt->crypt, encbuf, src + done, len);

		if (__ops_get_debug_level(__FILE__)) {
			int             i = 0;

			fprintf(stderr, "WRITING:\nunencrypted: ");
			for (i = 0; i < 16; i++) {
				fprintf(stderr, "%2x ", src[done + i]);
			}
			fprintf(stderr, "\n");
			fprintf(stderr, "encrypted:   ");
			for (i = 0; i < 16; i++) {
				fprintf(stderr, "%2x ", encbuf[i]);
			}
			fprintf(stderr, "\n");
		}
		if (!__ops_stacked_write(encbuf, len, errors, winfo)) {
			if (__ops_get_debug_level(__FILE__)) {
				fprintf(stderr, "encrypted_writer got error from stacked write, returning\n");
			}
			return false;
		}
		remaining -= len;
		done += len;
	}

	return true;
}

static void 
encrypt_destroyer(__ops_writer_info_t * winfo)
{
	crypt_t    *pgp_encrypt = (crypt_t *) __ops_writer_get_arg(winfo);

	if (pgp_encrypt->free_crypt) {
		free(pgp_encrypt->crypt);
	}
	free(pgp_encrypt);
}

/**
\ingroup Core_WritersNext
\brief Push Encrypted Writer onto stack (create SE packets)
*/
void 
__ops_writer_push_encrypt_crypt(__ops_create_info_t * cinfo,
			      __ops_crypt_t * pgp_crypt)
{
	/* Create encrypt to be used with this writer */
	/* Remember to free this in the destroyer */

	crypt_t    *pgp_encrypt = calloc(1, sizeof(*pgp_encrypt));

	/* Setup the encrypt */

	pgp_encrypt->crypt = pgp_crypt;
	pgp_encrypt->free_crypt = 0;

	/* And push writer on stack */
	__ops_writer_push(cinfo, encrypt_writer, NULL, encrypt_destroyer, pgp_encrypt);

}

/**************************************************************************/

typedef struct {
	__ops_crypt_t    *crypt;
}               encrypt_se_ip_t;

static bool	encrypt_se_ip_writer(const unsigned char *,
		     unsigned,
		     __ops_error_t **,
		     __ops_writer_info_t *);
static void     encrypt_se_ip_destroyer(__ops_writer_info_t *);

/* */

/**
\ingroup Core_WritersNext
\brief Push Encrypted SE IP Writer onto stack
*/
void 
__ops_writer_push_encrypt_se_ip(__ops_create_info_t * cinfo,
			      const __ops_keydata_t * pub_key)
{
	__ops_crypt_t    *encrypted;
	unsigned char  *iv = NULL;

	/* Create se_ip to be used with this writer */
	/* Remember to free this in the destroyer */
	encrypt_se_ip_t *se_ip = calloc(1, sizeof(*se_ip));

	/* Create and write encrypted PK session key */
	__ops_pk_session_key_t *encrypted_pk_session_key;
	encrypted_pk_session_key = __ops_create_pk_session_key(pub_key);
	__ops_write_pk_session_key(cinfo, encrypted_pk_session_key);

	/* Setup the se_ip */
	encrypted = calloc(1, sizeof(*encrypted));
	__ops_crypt_any(encrypted, encrypted_pk_session_key->symmetric_algorithm);
	iv = calloc(1, encrypted->blocksize);
	encrypted->set_iv(encrypted, iv);
	encrypted->set_key(encrypted, &encrypted_pk_session_key->key[0]);
	__ops_encrypt_init(encrypted);

	se_ip->crypt = encrypted;

	/* And push writer on stack */
	__ops_writer_push(cinfo, encrypt_se_ip_writer, NULL, encrypt_se_ip_destroyer, se_ip);
	/* tidy up */
	free(encrypted_pk_session_key);
	free(iv);
}

static bool 
encrypt_se_ip_writer(const unsigned char *src,
		     unsigned length,
		     __ops_error_t ** errors,
		     __ops_writer_info_t * winfo)
{
	encrypt_se_ip_t *se_ip = __ops_writer_get_arg(winfo);
	bool   rtn = true;
	__ops_memory_t   *mem_literal;
	__ops_create_info_t *cinfo_literal;
	__ops_memory_t   *mem_compressed;
	__ops_create_info_t *cinfo_compressed;
	__ops_memory_t   *my_mem;
	__ops_create_info_t *my_cinfo;
	const unsigned int bufsz = 128;	/* initial value; gets expanded as
					 * necessary */

	__ops_setup_memory_write(&cinfo_literal, &mem_literal, bufsz);
	__ops_setup_memory_write(&cinfo_compressed, &mem_compressed, bufsz);
	__ops_setup_memory_write(&my_cinfo, &my_mem, bufsz);

	/* create literal data packet from source data */
	__ops_write_literal_data_from_buf(src, (const int)length, OPS_LDT_BINARY, cinfo_literal);
	assert(__ops_memory_get_length(mem_literal) > length);

	/* create compressed packet from literal data packet */
	__ops_write_compressed(__ops_memory_get_data(mem_literal),
			     __ops_memory_get_length(mem_literal),
			     cinfo_compressed);

	/* create SE IP packet set from this compressed literal data */
	__ops_write_se_ip_pktset(__ops_memory_get_data(mem_compressed),
			       __ops_memory_get_length(mem_compressed),
			       se_ip->crypt, my_cinfo);
	assert(__ops_memory_get_length(my_mem) > __ops_memory_get_length(mem_compressed));

	/* now write memory to next writer */
	rtn = __ops_stacked_write(__ops_memory_get_data(my_mem),
				__ops_memory_get_length(my_mem),
				errors, winfo);

	__ops_memory_free(my_mem);
	__ops_memory_free(mem_compressed);
	__ops_memory_free(mem_literal);

	return rtn;
}

static void 
encrypt_se_ip_destroyer(__ops_writer_info_t * winfo)
{
	encrypt_se_ip_t *se_ip = __ops_writer_get_arg(winfo);

	free(se_ip->crypt);
	free(se_ip);
}

bool 
__ops_write_se_ip_pktset(const unsigned char *data,
		       const unsigned int len,
		       __ops_crypt_t * crypted,
		       __ops_create_info_t * cinfo)
{
	unsigned char   hashed[SHA_DIGEST_LENGTH];
	const size_t    sz_mdc = 1 + 1 + SHA_DIGEST_LENGTH;
	size_t          sz_preamble = crypted->blocksize + 2;
	unsigned char  *preamble = calloc(1, sz_preamble);
	size_t          sz_buf = sz_preamble + len + sz_mdc;
	__ops_memory_t   *mem_mdc;
	__ops_create_info_t *cinfo_mdc;

	if (!__ops_write_ptag(OPS_PTAG_CT_SE_IP_DATA, cinfo) ||
	    !__ops_write_length(1 + sz_buf, cinfo) ||
	    !__ops_write_scalar(SE_IP_DATA_VERSION, 1, cinfo)) {
		free(preamble);
		return 0;
	}
	__ops_random(preamble, crypted->blocksize);
	preamble[crypted->blocksize] = preamble[crypted->blocksize - 2];
	preamble[crypted->blocksize + 1] = preamble[crypted->blocksize - 1];

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;

		fprintf(stderr, "\npreamble: ");
		for (i = 0; i < sz_preamble; i++) {
			fprintf(stderr, " 0x%02x", preamble[i]);
		}
		fprintf(stderr, "\n");
	}
	/* now construct MDC packet and add to the end of the buffer */

	__ops_setup_memory_write(&cinfo_mdc, &mem_mdc, sz_mdc);

	__ops_calc_mdc_hash(preamble, sz_preamble, data, len, &hashed[0]);

	__ops_write_mdc(hashed, cinfo_mdc);

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;
		size_t          sz_plaintext = len;
		size_t          sz_mdc2 = 1 + 1 + OPS_SHA1_HASH_SIZE;
		unsigned char  *mdc = NULL;

		fprintf(stderr, "\nplaintext: ");
		for (i = 0; i < sz_plaintext; i++) {
			fprintf(stderr, " 0x%02x", data[i]);
		}
		fprintf(stderr, "\n");

		fprintf(stderr, "\nmdc: ");
		mdc = __ops_memory_get_data(mem_mdc);
		for (i = 0; i < sz_mdc2; i++) {
			fprintf(stderr, " 0x%02x", mdc[i]);
		}
		fprintf(stderr, "\n");
	}
	/* and write it out */

	__ops_writer_push_encrypt_crypt(cinfo, crypted);

#ifdef DEBUG
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "writing %" PRIsize "u + %d + %" PRIsize "u\n", sz_preamble, len, __ops_memory_get_length(mem_mdc));
	}
#endif				/* DEBUG */

	if (!__ops_write(preamble, sz_preamble, cinfo) ||
	    !__ops_write(data, len, cinfo) ||
	    !__ops_write(__ops_memory_get_data(mem_mdc), __ops_memory_get_length(mem_mdc), cinfo))
		/* \todo fix cleanup here and in old code functions */
		return 0;

	__ops_writer_pop(cinfo);

	/* cleanup  */
	__ops_teardown_memory_write(cinfo_mdc, mem_mdc);
	free(preamble);

	return 1;
}

typedef struct {
	int             fd;
}               writer_fd_t;

static bool 
fd_writer(const unsigned char *src, unsigned length,
	  __ops_error_t ** errors,
	  __ops_writer_info_t * winfo)
{
	writer_fd_t *writer = __ops_writer_get_arg(winfo);
	int             n = write(writer->fd, src, length);

	if (n == -1) {
		OPS_SYSTEM_ERROR_1(errors, OPS_E_W_WRITE_FAILED, "write",
				   "file descriptor %d", writer->fd);
		return false;
	}
	if ((unsigned) n != length) {
		OPS_ERROR_1(errors, OPS_E_W_WRITE_TOO_SHORT,
			    "file descriptor %d", writer->fd);
		return false;
	}
	return true;
}

static void 
writer_fd_destroyer(__ops_writer_info_t * winfo)
{
	free(__ops_writer_get_arg(winfo));
}

/**
 * \ingroup Core_WritersFirst
 * \brief Write to a File
 *
 * Set the writer in info to be a stock writer that writes to a file
 * descriptor. If another writer has already been set, then that is
 * first destroyed.
 *
 * \param info The info structure
 * \param fd The file descriptor
 *
 */

void 
__ops_writer_set_fd(__ops_create_info_t * info, int fd)
{
	writer_fd_t *writer = calloc(1, sizeof(*writer));

	writer->fd = fd;
	__ops_writer_set(info, fd_writer, NULL, writer_fd_destroyer, writer);
}

static bool 
memory_writer(const unsigned char *src, unsigned length,
	      __ops_error_t ** errors,
	      __ops_writer_info_t * winfo)
{
	__ops_memory_t   *mem = __ops_writer_get_arg(winfo);

	OPS_USED(errors);
	__ops_memory_add(mem, src, length);
	return true;
}

/**
 * \ingroup Core_WritersFirst
 * \brief Write to memory
 *
 * Set a memory writer.
 *
 * \param info The info structure
 * \param mem The memory structure
 * \note It is the caller's responsiblity to call __ops_memory_free(mem)
 * \sa __ops_memory_free()
 */

void 
__ops_writer_set_memory(__ops_create_info_t * info, __ops_memory_t * mem)
{
	__ops_writer_set(info, memory_writer, NULL, NULL, mem);
}

/**************************************************************************/

typedef struct {
	__ops_hash_algorithm_t hash_algorithm;
	__ops_hash_t      hash;
	unsigned char  *hashed;
}               skey_checksum_t;

static bool 
skey_checksum_writer(const unsigned char *src, const unsigned length, __ops_error_t ** errors, __ops_writer_info_t * winfo)
{
	skey_checksum_t *sum = __ops_writer_get_arg(winfo);
	bool   rtn = true;

	/* add contents to hash */
	sum->hash.add(&sum->hash, src, length);

	/* write to next stacked writer */
	rtn = __ops_stacked_write(src, length, errors, winfo);

	/* tidy up and return */
	return rtn;
}

static bool
skey_checksum_finaliser(__ops_error_t **errors, __ops_writer_info_t * winfo)
{
	skey_checksum_t *sum = __ops_writer_get_arg(winfo);

	if (errors) {
		printf("errors in skey_checksum_finaliser\n");
	}
	sum->hash.finish(&sum->hash, sum->hashed);
	return true;
}

static void 
skey_checksum_destroyer(__ops_writer_info_t * winfo)
{
	skey_checksum_t *sum = __ops_writer_get_arg(winfo);

	free(sum);
}

/**
\ingroup Core_WritersNext
\param cinfo
\param skey
*/
void 
__ops_push_skey_checksum_writer(__ops_create_info_t * cinfo, __ops_secret_key_t * skey)
{
	/* OPS_USED(info); */
	/* XXX: push a SHA-1 checksum writer (and change s2k to 254). */
	skey_checksum_t *sum = calloc(1, sizeof(*sum));

	/* configure the arg */
	sum->hash_algorithm = skey->hash_algorithm;
	sum->hashed = &skey->checkhash[0];

	/* init the hash */
	__ops_hash_any(&sum->hash, sum->hash_algorithm);
	sum->hash.init(&sum->hash);

	__ops_writer_push(cinfo, skey_checksum_writer, skey_checksum_finaliser, skey_checksum_destroyer, sum);
}

/**************************************************************************/

#define MAX_PARTIAL_DATA_LENGTH 1073741824

typedef struct {
	__ops_crypt_t    *crypt;
	__ops_memory_t   *mem_data;
	__ops_memory_t   *mem_literal;
	__ops_create_info_t *cinfo_literal;
	__ops_memory_t   *mem_se_ip;
	__ops_create_info_t *cinfo_se_ip;
	__ops_hash_t      hash;
}               stream_encrypt_se_ip_t;


static bool 
stream_encrypt_se_ip_writer(const unsigned char *src,
			    unsigned length,
			    __ops_error_t ** errors,
			    __ops_writer_info_t * winfo);

static bool 
stream_encrypt_se_ip_finaliser(__ops_error_t ** errors,
			       __ops_writer_info_t * winfo);

static void     stream_encrypt_se_ip_destroyer(__ops_writer_info_t * winfo);

/* */

/**
\ingroup Core_WritersNext
\param cinfo
\param pub_key
*/
void 
__ops_writer_push_stream_encrypt_se_ip(__ops_create_info_t * cinfo,
				     const __ops_keydata_t * pub_key)
{
	__ops_crypt_t    *encrypted;
	unsigned char  *iv = NULL;
	const unsigned int bufsz = 1024;
	/* Create arg to be used with this writer */
	/* Remember to free this in the destroyer */
	stream_encrypt_se_ip_t *se_ip = calloc(1, sizeof(*se_ip));
	/* Create and write encrypted PK session key */
	__ops_pk_session_key_t *encrypted_pk_session_key;

	encrypted_pk_session_key = __ops_create_pk_session_key(pub_key);
	__ops_write_pk_session_key(cinfo, encrypted_pk_session_key);

	/* Setup the se_ip */
	encrypted = calloc(1, sizeof(*encrypted));
	__ops_crypt_any(encrypted, encrypted_pk_session_key->symmetric_algorithm);
	iv = calloc(1, encrypted->blocksize);
	encrypted->set_iv(encrypted, iv);
	encrypted->set_key(encrypted, &encrypted_pk_session_key->key[0]);
	__ops_encrypt_init(encrypted);

	se_ip->crypt = encrypted;

	se_ip->mem_data = __ops_memory_new();
	__ops_memory_init(se_ip->mem_data, bufsz);

	se_ip->mem_literal = NULL;
	se_ip->cinfo_literal = NULL;

	__ops_setup_memory_write(&se_ip->cinfo_se_ip, &se_ip->mem_se_ip, bufsz);

	/* And push writer on stack */
	__ops_writer_push(cinfo,
			stream_encrypt_se_ip_writer,
			stream_encrypt_se_ip_finaliser,
			stream_encrypt_se_ip_destroyer, se_ip);
	/* tidy up */
	free(encrypted_pk_session_key);
	free(iv);
}


static unsigned int 
__ops_calc_partial_data_length(unsigned int len)
{
	int             i;
	unsigned int    mask = MAX_PARTIAL_DATA_LENGTH;

	assert(len > 0);
	if (len > MAX_PARTIAL_DATA_LENGTH) {
		return MAX_PARTIAL_DATA_LENGTH;
	}
	for (i = 0; i <= 30; i++) {
		if (mask & len) {
			break;
		}
		mask >>= 1;
	}

	return mask;
}

static bool 
__ops_write_partial_data_length(unsigned int len,
			      __ops_create_info_t * info)
{
	/* len must be a power of 2 from 0 to 30 */
	int             i;
	unsigned char   c[1];

	for (i = 0; i <= 30; i++) {
		if ((len >> i) & 1) {
			break;
		}
	}

	c[0] = 224 + i;
	return __ops_write(c, 1, info);
}

static bool 
__ops_stream_write_literal_data(const unsigned char *data,
			      unsigned int len,
			      __ops_create_info_t * info)
{
	while (len > 0) {
		size_t          pdlen = __ops_calc_partial_data_length(len);

		__ops_write_partial_data_length(pdlen, info);
		__ops_write(data, pdlen, info);
		data += pdlen;
		len -= pdlen;
	}
	return true;
}

static          bool
__ops_stream_write_literal_data_first(const unsigned char *data,
				    unsigned int len,
				    const __ops_literal_data_type_t type,
				    __ops_create_info_t * info)
{
	/* \todo add filename  */
	/* \todo add date */
	/* \todo do we need to check text data for <cr><lf> line endings ? */

	size_t          sz_towrite = 1 + 1 + 4 + len;
	size_t          sz_pd = __ops_calc_partial_data_length(sz_towrite);

	assert(sz_pd >= 512);
	__ops_write_ptag(OPS_PTAG_CT_LITERAL_DATA, info);
	__ops_write_partial_data_length(sz_pd, info);
	__ops_write_scalar((unsigned)type, 1, info);
	__ops_write_scalar(0, 1, info);
	__ops_write_scalar(0, 4, info);
	__ops_write(data, sz_pd - 6, info);

	data += (sz_pd - 6);
	sz_towrite -= sz_pd;

	__ops_stream_write_literal_data(data, sz_towrite, info);
	return true;
}

static          bool
__ops_stream_write_literal_data_last(const unsigned char *data,
				   unsigned int len,
				   __ops_create_info_t * info)
{
	__ops_write_length(len, info);
	__ops_write(data, len, info);
	return true;
}

static          bool
__ops_stream_write_se_ip(const unsigned char *data,
		       unsigned int len,
		       stream_encrypt_se_ip_t * se_ip,
		       __ops_create_info_t * cinfo)
{
	size_t          pdlen;

	while (len > 0) {
		pdlen = __ops_calc_partial_data_length(len);
		__ops_write_partial_data_length(pdlen, cinfo);

		__ops_writer_push_encrypt_crypt(cinfo, se_ip->crypt);
		__ops_write(data, pdlen, cinfo);
		__ops_writer_pop(cinfo);

		se_ip->hash.add(&se_ip->hash, data, pdlen);

		data += pdlen;
		len -= pdlen;
	}
	return true;
}

static          bool
__ops_stream_write_se_ip_first(const unsigned char *data,
			     unsigned int len,
			     stream_encrypt_se_ip_t * se_ip,
			     __ops_create_info_t * cinfo)
{
	size_t          sz_preamble = se_ip->crypt->blocksize + 2;
	size_t          sz_towrite = sz_preamble + 1 + len;
	unsigned char  *preamble = calloc(1, sz_preamble);
	size_t          sz_pd = __ops_calc_partial_data_length(sz_towrite);

	assert(sz_pd >= 512);

	__ops_write_ptag(OPS_PTAG_CT_SE_IP_DATA, cinfo);
	__ops_write_partial_data_length(sz_pd, cinfo);
	__ops_write_scalar(SE_IP_DATA_VERSION, 1, cinfo);

	__ops_writer_push_encrypt_crypt(cinfo, se_ip->crypt);

	__ops_random(preamble, se_ip->crypt->blocksize);
	preamble[se_ip->crypt->blocksize] = preamble[se_ip->crypt->blocksize - 2];
	preamble[se_ip->crypt->blocksize + 1] = preamble[se_ip->crypt->blocksize - 1];

	__ops_hash_any(&se_ip->hash, OPS_HASH_SHA1);
	se_ip->hash.init(&se_ip->hash);

	__ops_write(preamble, sz_preamble, cinfo);
	se_ip->hash.add(&se_ip->hash, preamble, sz_preamble);

	__ops_write(data, sz_pd - sz_preamble - 1, cinfo);
	se_ip->hash.add(&se_ip->hash, data, sz_pd - sz_preamble - 1);

	data += (sz_pd - sz_preamble - 1);
	sz_towrite -= sz_pd;

	__ops_writer_pop(cinfo);

	__ops_stream_write_se_ip(data, sz_towrite, se_ip, cinfo);

	free(preamble);

	return true;
}

static          bool
__ops_stream_write_se_ip_last(const unsigned char *data,
			    unsigned int len,
			    stream_encrypt_se_ip_t * se_ip,
			    __ops_create_info_t * cinfo)
{
	unsigned char   c[1];
	unsigned char   hashed[SHA_DIGEST_LENGTH];
	const size_t    sz_mdc = 1 + 1 + SHA_DIGEST_LENGTH;
	size_t          sz_buf = len + sz_mdc;
	__ops_memory_t   *mem_mdc;
	__ops_create_info_t *cinfo_mdc;

	se_ip->hash.add(&se_ip->hash, data, len);

	/* MDC packet tag */
	c[0] = 0xD3;
	se_ip->hash.add(&se_ip->hash, &c[0], 1);

	/* MDC packet len */
	c[0] = 0x14;
	se_ip->hash.add(&se_ip->hash, &c[0], 1);

	/* finish */
	se_ip->hash.finish(&se_ip->hash, hashed);

	__ops_setup_memory_write(&cinfo_mdc, &mem_mdc, sz_mdc);
	__ops_write_mdc(hashed, cinfo_mdc);

	/* write length of last se_ip chunk */
	__ops_write_length(sz_buf, cinfo);

	/* encode everting */
	__ops_writer_push_encrypt_crypt(cinfo, se_ip->crypt);

	__ops_write(data, len, cinfo);
	__ops_write(__ops_memory_get_data(mem_mdc), __ops_memory_get_length(mem_mdc), cinfo);

	__ops_writer_pop(cinfo);

	__ops_teardown_memory_write(cinfo_mdc, mem_mdc);

	return true;
}

static bool 
stream_encrypt_se_ip_writer(const unsigned char *src,
			    unsigned length,
			    __ops_error_t ** errors,
			    __ops_writer_info_t * winfo)
{
	stream_encrypt_se_ip_t *se_ip = __ops_writer_get_arg(winfo);
	bool   rtn = true;

	if (se_ip->cinfo_literal == NULL) {	/* first literal data chunk
						 * is not yet written */
		size_t          datalength;

		__ops_memory_add(se_ip->mem_data, src, length);
		datalength = __ops_memory_get_length(se_ip->mem_data);

		/* 4.2.2.4. Partial Body Lengths */
		/* The first partial length MUST be at least 512 octets long. */
		if (datalength < 512) {
			return true;	/* will wait for more data or
						 * end of stream             */
		}
		__ops_setup_memory_write(&se_ip->cinfo_literal, &se_ip->mem_literal, datalength + 32);
		__ops_stream_write_literal_data_first(__ops_memory_get_data(se_ip->mem_data),
						    datalength,
						    OPS_LDT_BINARY,
						    se_ip->cinfo_literal);

		__ops_stream_write_se_ip_first(__ops_memory_get_data(se_ip->mem_literal),
				    __ops_memory_get_length(se_ip->mem_literal),
					     se_ip, se_ip->cinfo_se_ip);
	} else {
		__ops_stream_write_literal_data(src, length, se_ip->cinfo_literal);
		__ops_stream_write_se_ip(__ops_memory_get_data(se_ip->mem_literal),
				    __ops_memory_get_length(se_ip->mem_literal),
				       se_ip, se_ip->cinfo_se_ip);
	}

	/* now write memory to next writer */
	rtn = __ops_stacked_write(__ops_memory_get_data(se_ip->mem_se_ip),
				__ops_memory_get_length(se_ip->mem_se_ip),
				errors, winfo);

	__ops_memory_clear(se_ip->mem_literal);
	__ops_memory_clear(se_ip->mem_se_ip);

	return rtn;
}

static bool 
stream_encrypt_se_ip_finaliser(__ops_error_t ** errors,
			       __ops_writer_info_t * winfo)
{
	stream_encrypt_se_ip_t *se_ip = __ops_writer_get_arg(winfo);
	/* write last chunk of data */

	if (se_ip->cinfo_literal == NULL) {
		/* first literal data chunk was not written */
		/* so we know the total length of data, write a simple packet */

		/* create literal data packet from buffered data */
		__ops_setup_memory_write(&se_ip->cinfo_literal,
				       &se_ip->mem_literal,
				 __ops_memory_get_length(se_ip->mem_data) + 32);

		__ops_write_literal_data_from_buf(__ops_memory_get_data(se_ip->mem_data),
			(const int)__ops_memory_get_length(se_ip->mem_data),
			OPS_LDT_BINARY,
			se_ip->cinfo_literal);

		/* create SE IP packet set from this literal data */
		__ops_write_se_ip_pktset(__ops_memory_get_data(se_ip->mem_literal),
				    __ops_memory_get_length(se_ip->mem_literal),
				       se_ip->crypt, se_ip->cinfo_se_ip);

	} else {
		/* finish writing */
		__ops_stream_write_literal_data_last(NULL, 0, se_ip->cinfo_literal);
		__ops_stream_write_se_ip_last(__ops_memory_get_data(se_ip->mem_literal),
				    __ops_memory_get_length(se_ip->mem_literal),
					    se_ip, se_ip->cinfo_se_ip);
	}

	/* now write memory to next writer */
	return __ops_stacked_write(__ops_memory_get_data(se_ip->mem_se_ip),
				 __ops_memory_get_length(se_ip->mem_se_ip),
				 errors, winfo);
}

static void 
stream_encrypt_se_ip_destroyer(__ops_writer_info_t * winfo)
{
	stream_encrypt_se_ip_t *se_ip = __ops_writer_get_arg(winfo);

	__ops_memory_free(se_ip->mem_data);
	__ops_teardown_memory_write(se_ip->cinfo_literal, se_ip->mem_literal);
	__ops_teardown_memory_write(se_ip->cinfo_se_ip, se_ip->mem_se_ip);

	se_ip->crypt->decrypt_finish(se_ip->crypt);

	free(se_ip->crypt);
	free(se_ip);
}
