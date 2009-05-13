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
#include "config.h"

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#include "create.h"
#include "keyring.h"
#include "packet.h"
#include "signature.h"
#include "writer.h"

#include "readerwriter.h"
#include "keyring_local.h"
#include "loccreate.h"
#include "memory.h"
#include "netpgpdefs.h"

#include <string.h>

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif

#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/**
 * \ingroup Core_Create
 * \param length
 * \param type
 * \param info
 * \return true if OK, otherwise false
 */

bool 
__ops_write_ss_header(unsigned length, __ops_content_tag_t type,
		    __ops_create_info_t * info)
{
	return __ops_write_length(length, info) &&
		__ops_write_scalar((unsigned)(type - OPS_PTAG_SIGNATURE_SUBPACKET_BASE), 1, info);
}

/*
 * XXX: the general idea of _fast_ is that it doesn't copy stuff the safe
 * (i.e. non _fast_) version will, and so will also need to be freed.
 */

/**
 * \ingroup Core_Create
 *
 * __ops_fast_create_user_id() sets id->user_id to the given user_id.
 * This is fast because it is only copying a char*. However, if user_id
 * is changed or freed in the future, this could have injurious results.
 * \param id
 * \param user_id
 */

void 
__ops_fast_create_user_id(__ops_user_id_t * id, unsigned char *user_id)
{
	id->user_id = user_id;
}

/**
 * \ingroup Core_WritePackets
 * \brief Writes a User Id packet
 * \param id
 * \param info
 * \return true if OK, otherwise false
 */
bool 
__ops_write_struct_user_id(__ops_user_id_t * id,
			 __ops_create_info_t * info)
{
	return __ops_write_ptag(OPS_PTAG_CT_USER_ID, info)
	&& __ops_write_length(strlen((char *) id->user_id), info)
	&& __ops_write(id->user_id, strlen((char *) id->user_id), info);
}

/**
 * \ingroup Core_WritePackets
 * \brief Write a User Id packet.
 * \param user_id
 * \param info
 *
 * \return return value from __ops_write_struct_user_id()
 */
bool 
__ops_write_user_id(const unsigned char *user_id, __ops_create_info_t * info)
{
	__ops_user_id_t   id;

	id.user_id = __UNCONST(user_id);
	return __ops_write_struct_user_id(&id, info);
}

/**
\ingroup Core_MPI
*/
static unsigned 
mpi_length(const BIGNUM * bn)
{
	return 2 + (BN_num_bits(bn) + 7) / 8;
}

static unsigned 
public_key_length(const __ops_public_key_t * key)
{
	switch (key->algorithm) {
	case OPS_PKA_RSA:
		return mpi_length(key->key.rsa.n) + mpi_length(key->key.rsa.e);

	default:
		assert(!"unknown key algorithm");
	}
	/* not reached */
	return 0;
}

static unsigned 
secret_key_length(const __ops_secret_key_t * key)
{
	int             l;

	l = 0;
	switch (key->public_key.algorithm) {
	case OPS_PKA_RSA:
		l = mpi_length(key->key.rsa.d) + mpi_length(key->key.rsa.p)
			+ mpi_length(key->key.rsa.q) + mpi_length(key->key.rsa.u);
		break;

	default:
		assert(!"unknown key algorithm");
	}

	return l + public_key_length(&key->public_key);
}

/**
 * \ingroup Core_Create
 * \param key
 * \param t
 * \param n
 * \param e
*/
void 
__ops_fast_create_rsa_public_key(__ops_public_key_t * key, time_t t,
			       BIGNUM * n, BIGNUM * e)
{
	key->version = 4;
	key->creation_time = t;
	key->algorithm = OPS_PKA_RSA;
	key->key.rsa.n = n;
	key->key.rsa.e = e;
}

/*
 * Note that we support v3 keys here because they're needed for for
 * verification - the writer doesn't allow them, though
 */
static bool 
write_public_key_body(const __ops_public_key_t * key,
		      __ops_create_info_t * info)
{
	if (!(__ops_write_scalar((unsigned)key->version, 1, info) &&
	      __ops_write_scalar((unsigned)key->creation_time, 4, info)))
		return false;

	if (key->version != 4 && !__ops_write_scalar(key->days_valid, 2, info))
		return false;

	if (!__ops_write_scalar((unsigned)key->algorithm, 1, info))
		return false;

	switch (key->algorithm) {
	case OPS_PKA_DSA:
		return __ops_write_mpi(key->key.dsa.p, info)
			&& __ops_write_mpi(key->key.dsa.q, info)
			&& __ops_write_mpi(key->key.dsa.g, info)
			&& __ops_write_mpi(key->key.dsa.y, info);

	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		return __ops_write_mpi(key->key.rsa.n, info)
			&& __ops_write_mpi(key->key.rsa.e, info);

	case OPS_PKA_ELGAMAL:
		return __ops_write_mpi(key->key.elgamal.p, info)
			&& __ops_write_mpi(key->key.elgamal.g, info)
			&& __ops_write_mpi(key->key.elgamal.y, info);

	default:
		assert( /* CONSTCOND */ 0);
		break;
	}

	/* not reached */
	return false;
}

/*
 * Note that we support v3 keys here because they're needed for for
 * verification - the writer doesn't allow them, though
 */
static bool 
write_secret_key_body(const __ops_secret_key_t * key,
		      const unsigned char *passphrase,
		      const size_t pplen,
		      __ops_create_info_t * info)
{
	/* RFC4880 Section 5.5.3 Secret-Key Packet Formats */

	__ops_crypt_t     crypted;
	__ops_hash_t      hash;
	unsigned char   hashed[OPS_SHA1_HASH_SIZE];
	unsigned char   session_key[CAST_KEY_LENGTH];
	unsigned int    done = 0;
	unsigned int    i = 0;

	if (!write_public_key_body(&key->public_key, info))
		return false;

	assert(key->s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED);	/* = 254 */
	if (!__ops_write_scalar((unsigned)key->s2k_usage, 1, info))
		return false;

	assert(key->algorithm == OPS_SA_CAST5);
	if (!__ops_write_scalar((unsigned)key->algorithm, 1, info))
		return false;

	assert(key->s2k_specifier == OPS_S2KS_SIMPLE || key->s2k_specifier == OPS_S2KS_SALTED);	/* = 1 \todo could also
												 * be
												 * iterated-and-salted */
	if (!__ops_write_scalar((unsigned)key->s2k_specifier, 1, info))
		return false;

	assert(key->hash_algorithm == OPS_HASH_SHA1);
	if (!__ops_write_scalar((unsigned)key->hash_algorithm, 1, info))
		return false;

	switch (key->s2k_specifier) {
	case OPS_S2KS_SIMPLE:
		/* nothing more to do */
		break;

	case OPS_S2KS_SALTED:
		/* 8-octet salt value */
		__ops_random(__UNCONST(&key->salt[0]), OPS_SALT_SIZE);
		if (!__ops_write(key->salt, OPS_SALT_SIZE, info))
			return false;
		break;

		/*
		 * \todo case OPS_S2KS_ITERATED_AND_SALTED: // 8-octet salt
		 * value // 1-octet count break;
		 */

	default:
		fprintf(stderr, "invalid/unsupported s2k specifier %d\n", key->s2k_specifier);
		assert( /* CONSTCOND */ 0);
	}

	if (!__ops_write(&key->iv[0], __ops_block_size(key->algorithm), info))
		return false;

	/*
	 * create the session key for encrypting the algorithm-specific
	 * fields
	 */

	switch (key->s2k_specifier) {
	case OPS_S2KS_SIMPLE:
	case OPS_S2KS_SALTED:
		/* RFC4880: section 3.7.1.1 and 3.7.1.2 */

		done = 0;
		for (i = 0; done < CAST_KEY_LENGTH; i++) {
			unsigned int    j = 0;
			unsigned char   zero = 0;
			int             needed = CAST_KEY_LENGTH - done;
			int             use = needed < SHA_DIGEST_LENGTH ? needed : SHA_DIGEST_LENGTH;

			__ops_hash_any(&hash, key->hash_algorithm);
			hash.init(&hash);

			/* preload if iterating  */
			for (j = 0; j < i; j++) {
				/*
				 * Coverity shows a DEADCODE error on this
				 * line. This is expected since the hardcoded
				 * use of SHA1 and CAST5 means that it will
				 * not used. This will change however when
				 * other algorithms are supported.
				 */
				hash.add(&hash, &zero, 1);
			}

			if (key->s2k_specifier == OPS_S2KS_SALTED) {
				hash.add(&hash, key->salt, OPS_SALT_SIZE);
			}
			hash.add(&hash, passphrase, pplen);
			hash.finish(&hash, hashed);

			/*
			 * if more in hash than is needed by session key, use
			 * the leftmost octets
			 */
			(void) memcpy(session_key + (i * SHA_DIGEST_LENGTH), hashed, (unsigned)use);
			done += use;
			assert(done <= CAST_KEY_LENGTH);
		}

		break;

		/*
		 * \todo case OPS_S2KS_ITERATED_AND_SALTED: * 8-octet salt
		 * value * 1-octet count break;
		 */

	default:
		fprintf(stderr, "invalid/unsupported s2k specifier %d\n", key->s2k_specifier);
		assert( /* CONSTCOND */ 0);
	}

	/* use this session key to encrypt */

	__ops_crypt_any(&crypted, key->algorithm);
	crypted.set_iv(&crypted, key->iv);
	crypted.set_key(&crypted, session_key);
	__ops_encrypt_init(&crypted);

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i2 = 0;
		fprintf(stderr, "\nWRITING:\niv=");
		for (i2 = 0; i2 < __ops_block_size(key->algorithm); i2++) {
			fprintf(stderr, "%02x ", key->iv[i2]);
		}
		fprintf(stderr, "\n");

		fprintf(stderr, "key=");
		for (i2 = 0; i2 < CAST_KEY_LENGTH; i2++) {
			fprintf(stderr, "%02x ", session_key[i2]);
		}
		fprintf(stderr, "\n");

		/* __ops_print_secret_key(OPS_PTAG_CT_SECRET_KEY,key); */

		fprintf(stderr, "turning encryption on...\n");
	}
	__ops_writer_push_encrypt_crypt(info, &crypted);

	switch (key->public_key.algorithm) {
		/* case OPS_PKA_DSA: */
		/* return __ops_write_mpi(key->key.dsa.x,info); */

	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:

		if (!__ops_write_mpi(key->key.rsa.d, info)
		    || !__ops_write_mpi(key->key.rsa.p, info)
		    || !__ops_write_mpi(key->key.rsa.q, info)
		    || !__ops_write_mpi(key->key.rsa.u, info)) {
			if (__ops_get_debug_level(__FILE__)) {
				fprintf(stderr, "4 x mpi not written - problem\n");
			}
			return false;
		}
		break;

		/* case OPS_PKA_ELGAMAL: */
		/* return __ops_write_mpi(key->key.elgamal.x,info); */

	default:
		assert( /* CONSTCOND */ 0);
		break;
	}

	if (!__ops_write(key->checkhash, OPS_CHECKHASH_SIZE, info))
		return false;

	__ops_writer_pop(info);

	return true;
}


/**
   \ingroup HighLevel_KeyWrite

   \brief Writes a transferable PGP public key to the given output stream.

   \param keydata Key to be written
   \param armoured Flag is set for armoured output
   \param info Output stream

   Example code:
   \code
   void example(const __ops_keydata_t* keydata)
   {
   bool armoured=true;
   char* filename="/tmp/testkey.asc";

   int fd;
   bool overwrite=true;
   __ops_create_info_t* cinfo;

   fd=__ops_setup_file_write(&cinfo, filename, overwrite);
   __ops_write_transferable_public_key(keydata,armoured,cinfo);
   __ops_teardown_file_write(cinfo,fd);
   }
   \endcode
*/

bool 
__ops_write_transferable_public_key(const __ops_keydata_t * keydata, bool armoured, __ops_create_info_t * info)
{
	bool   rtn;
	unsigned int    i = 0, j = 0;

	if (armoured) {
		__ops_writer_push_armoured(info, OPS_PGP_PUBLIC_KEY_BLOCK);
	}
	/* public key */
	rtn = __ops_write_struct_public_key(&keydata->key.skey.public_key, info);
	if (rtn != true)
		return rtn;

	/* TODO: revocation signatures go here */

	/* user ids and corresponding signatures */
	for (i = 0; i < keydata->nuids; i++) {
		__ops_user_id_t  *uid = &keydata->uids[i];

		rtn = __ops_write_struct_user_id(uid, info);

		if (!rtn)
			return rtn;

		/* find signature for this packet if it exists */
		for (j = 0; j < keydata->nsigs; j++) {
			sigpacket_t    *sig = &keydata->sigs[i];
			if (!strcmp((char *) sig->userid->user_id, (char *) uid->user_id)) {
				rtn = __ops_write(sig->packet->raw, sig->packet->length, info);
				if (!rtn)
					return !rtn;
			}
		}
	}

	/* TODO: user attributes and corresponding signatures */

	/*
	 * subkey packets and corresponding signatures and optional
	 * revocation
	 */

	if (armoured) {
		writer_info_finalise(&info->errors, &info->winfo);
		__ops_writer_pop(info);
	}
	return rtn;
}

/**
   \ingroup HighLevel_KeyWrite

   \brief Writes a transferable PGP secret key to the given output stream.

   \param keydata Key to be written
   \param passphrase
   \param pplen
   \param armoured Flag is set for armoured output
   \param info Output stream

   Example code:
   \code
   void example(const __ops_keydata_t* keydata)
   {
   const unsigned char* passphrase=NULL;
   const size_t passphraselen=0;
   bool armoured=true;

   int fd;
   char* filename="/tmp/testkey.asc";
   bool overwrite=true;
   __ops_create_info_t* cinfo;

   fd=__ops_setup_file_write(&cinfo, filename, overwrite);
   __ops_write_transferable_secret_key(keydata,passphrase,pplen,armoured,cinfo);
   __ops_teardown_file_write(cinfo,fd);
   }
   \endcode
*/

bool 
__ops_write_transferable_secret_key(const __ops_keydata_t * keydata, const unsigned char *passphrase, const size_t pplen, bool armoured, __ops_create_info_t * info)
{
	bool   rtn;
	unsigned int    i = 0, j = 0;

	if (armoured) {
		__ops_writer_push_armoured(info, OPS_PGP_PRIVATE_KEY_BLOCK);
	}
	/* public key */
	rtn = __ops_write_struct_secret_key(&keydata->key.skey, passphrase, pplen, info);
	if (rtn != true)
		return rtn;

	/* TODO: revocation signatures go here */

	/* user ids and corresponding signatures */
	for (i = 0; i < keydata->nuids; i++) {
		__ops_user_id_t  *uid = &keydata->uids[i];

		rtn = __ops_write_struct_user_id(uid, info);

		if (!rtn)
			return rtn;

		/* find signature for this packet if it exists */
		for (j = 0; j < keydata->nsigs; j++) {
			sigpacket_t    *sig = &keydata->sigs[i];
			if (!strcmp((char *) sig->userid->user_id, (char *) uid->user_id)) {
				rtn = __ops_write(sig->packet->raw, sig->packet->length, info);
				if (!rtn)
					return !rtn;
			}
		}
	}

	/* TODO: user attributes and corresponding signatures */

	/*
	 * subkey packets and corresponding signatures and optional
	 * revocation
	 */

	if (armoured) {
		writer_info_finalise(&info->errors, &info->winfo);
		__ops_writer_pop(info);
	}
	return rtn;
}

/**
 * \ingroup Core_WritePackets
 * \brief Writes a Public Key packet
 * \param key
 * \param info
 * \return true if OK, otherwise false
 */
bool 
__ops_write_struct_public_key(const __ops_public_key_t * key,
			    __ops_create_info_t * info)
{
	assert(key->version == 4);

	return __ops_write_ptag(OPS_PTAG_CT_PUBLIC_KEY, info)
		&& __ops_write_length(1 + 4 + 1 + public_key_length(key), info)
		&& write_public_key_body(key, info);
}

/**
 * \ingroup Core_WritePackets
 * \brief Writes one RSA public key packet.
 * \param t Creation time
 * \param n RSA public modulus
 * \param e RSA public encryption exponent
 * \param info Writer settings
 *
 * \return true if OK, otherwise false
 */

bool 
__ops_write_rsa_public_key(time_t t, const BIGNUM * n,
			 const BIGNUM * e,
			 __ops_create_info_t * info)
{
	__ops_public_key_t key;

	__ops_fast_create_rsa_public_key(&key, t, __UNCONST(n), __UNCONST(e));
	return __ops_write_struct_public_key(&key, info);
}

/**
 * \ingroup Core_Create
 * \param out
 * \param key
 * \param make_packet
 */

void 
__ops_build_public_key(__ops_memory_t * out, const __ops_public_key_t * key,
		     bool make_packet)
{
	__ops_create_info_t *info;

	info = __ops_create_info_new();

	__ops_memory_init(out, 128);
	__ops_writer_set_memory(info, out);

	write_public_key_body(key, info);

	if (make_packet)
		__ops_memory_make_packet(out, OPS_PTAG_CT_PUBLIC_KEY);

	__ops_create_info_delete(info);
}

/**
 * \ingroup Core_Create
 *
 * Create an RSA secret key structure. If a parameter is marked as
 * [OPTIONAL], then it can be omitted and will be calculated from
 * other parameters - or, in the case of e, will default to 0x10001.
 *
 * Parameters are _not_ copied, so will be freed if the structure is
 * freed.
 *
 * \param key The key structure to be initialised.
 * \param t
 * \param d The RSA parameter d (=e^-1 mod (p-1)(q-1)) [OPTIONAL]
 * \param p The RSA parameter p
 * \param q The RSA parameter q (q > p)
 * \param u The RSA parameter u (=p^-1 mod q) [OPTIONAL]
 * \param n The RSA public parameter n (=p*q) [OPTIONAL]
 * \param e The RSA public parameter e */

void 
__ops_fast_create_rsa_secret_key(__ops_secret_key_t * key, time_t t,
			     BIGNUM * d, BIGNUM * p, BIGNUM * q, BIGNUM * u,
			       BIGNUM * n, BIGNUM * e)
{
	__ops_fast_create_rsa_public_key(&key->public_key, t, n, e);

	/* XXX: calculate optionals */
	key->key.rsa.d = d;
	key->key.rsa.p = p;
	key->key.rsa.q = q;
	key->key.rsa.u = u;

	key->s2k_usage = OPS_S2KU_NONE;

	/* XXX: sanity check and add errors... */
}

/**
 * \ingroup Core_WritePackets
 * \brief Writes a Secret Key packet.
 * \param key The secret key
 * \param passphrase The passphrase
 * \param pplen Length of passphrase
 * \param info
 * \return true if OK; else false
 */
bool 
__ops_write_struct_secret_key(const __ops_secret_key_t * key,
			    const unsigned char *passphrase,
			    const size_t pplen,
			    __ops_create_info_t * info)
{
	int             length = 0;

	assert(key->public_key.version == 4);

	/* Ref: RFC4880 Section 5.5.3 */

	/* public_key, excluding MPIs */
	length += 1 + 4 + 1 + 1;

	/* s2k usage */
	length += 1;

	switch (key->s2k_usage) {
	case OPS_S2KU_NONE:
		/* nothing to add */
		break;

	case OPS_S2KU_ENCRYPTED_AND_HASHED:	/* 254 */
	case OPS_S2KU_ENCRYPTED:	/* 255 */

		/* Ref: RFC4880 Section 3.7 */
		length += 1;	/* s2k_specifier */

		switch (key->s2k_specifier) {
		case OPS_S2KS_SIMPLE:
			length += 1;	/* hash algorithm */
			break;

		case OPS_S2KS_SALTED:
			length += 1 + 8;	/* hash algorithm + salt */
			break;

		case OPS_S2KS_ITERATED_AND_SALTED:
			length += 1 + 8 + 1;	/* hash algorithm, salt +
						 * count */
			break;

		default:
			assert( /* CONSTCOND */ 0);
		}
		break;

	default:
		assert( /* CONSTCOND */ 0);
	}

	/* IV */
	if (key->s2k_usage != 0) {
		length += __ops_block_size(key->algorithm);
	}
	/* checksum or hash */
	switch (key->s2k_usage) {
	case 0:
	case 255:
		length += 2;
		break;

	case 254:
		length += 20;
		break;

	default:
		assert( /* CONSTCOND */ 0);
	}

	/* secret key and public key MPIs */
	length += secret_key_length(key);

	return __ops_write_ptag(OPS_PTAG_CT_SECRET_KEY, info)
	/* && __ops_write_length(1+4+1+1+secret_key_length(key)+2,info) */
		&& __ops_write_length((unsigned)length, info)
		&& write_secret_key_body(key, passphrase, pplen, info);
}

/**
 * \ingroup Core_Create
 *
 * \brief Create a new __ops_create_info_t structure.
 *
 * \return the new structure.
 * \note It is the responsiblity of the caller to call __ops_create_info_delete().
 * \sa __ops_create_info_delete()
 */
__ops_create_info_t *
__ops_create_info_new(void)
{
	return calloc(1, sizeof(__ops_create_info_t));
}

/**
 * \ingroup Core_Create
 * \brief Delete an __ops_create_info_t strucut and associated resources.
 *
 * Delete an __ops_create_info_t structure. If a writer is active, then
 * that is also deleted.
 *
 * \param info the structure to be deleted.
 */
void 
__ops_create_info_delete(__ops_create_info_t * info)
{
	writer_info_delete(&info->winfo);
	free(info);
}

/**
 \ingroup Core_Create
 \brief Calculate the checksum for a session key
 \param session_key Session Key to use
 \param cs Checksum to be written
 \return true if OK; else false
*/
bool 
__ops_calc_session_key_checksum(__ops_pk_session_key_t * session_key, unsigned char cs[2])
{
	unsigned int    i = 0;
	unsigned long   checksum = 0;

	if (!__ops_is_sa_supported(session_key->symmetric_algorithm))
		return false;

	for (i = 0; i < __ops_key_size(session_key->symmetric_algorithm); i++) {
		checksum += session_key->key[i];
	}
	checksum = checksum % 65536;

	cs[0] = (unsigned char)((checksum >> 8) & 0xff);
	cs[1] = (unsigned char)(checksum & 0xff);

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr,"\nm buf checksum: ");
		(void) fprintf(stderr," %2x",cs[0]);
		(void) fprintf(stderr," %2x\n",cs[1]);
	}
	return true;
}

static bool 
create_unencoded_m_buf(__ops_pk_session_key_t * session_key, unsigned char *m_buf)
{
	int             i = 0;
	/* unsigned long checksum=0; */

	/* m_buf is the buffer which will be encoded in PKCS#1 block */
	/* encoding to form the "m" value used in the  */
	/* Public Key Encrypted Session Key Packet */
	/*
	 * as defined in RFC Section 5.1 "Public-Key Encrypted Session Key
	 * Packet"
	 */

	m_buf[0] = session_key->symmetric_algorithm;

	assert(session_key->symmetric_algorithm == OPS_SA_CAST5);
	for (i = 0; i < CAST_KEY_LENGTH; i++) {
		m_buf[1 + i] = session_key->key[i];
	}

	return (__ops_calc_session_key_checksum(session_key, m_buf + 1 + CAST_KEY_LENGTH));
}

/**
\ingroup Core_Create
\brief implementation of EME-PKCS1-v1_5-ENCODE, as defined in OpenPGP RFC
\param M
\param mLen
\param pkey
\param EM
\return true if OK; else false
*/
bool 
encode_m_buf(const unsigned char *M, size_t mLen,
	     const __ops_public_key_t * pkey,
	     unsigned char *EM
)
{
	unsigned int    k;
	unsigned        i;

	/* implementation of EME-PKCS1-v1_5-ENCODE, as defined in OpenPGP RFC */

	assert(pkey->algorithm == OPS_PKA_RSA);

	k = BN_num_bytes(pkey->key.rsa.n);
	assert(mLen <= k - 11);
	if (mLen > k - 11) {
		fprintf(stderr, "message too long\n");
		return false;
	}
	/* these two bytes defined by RFC */
	EM[0] = 0x00;
	EM[1] = 0x02;

	/* add non-zero random bytes of length k - mLen -3 */
	for (i = 2; i < k - mLen - 1; ++i)
		do
			__ops_random(EM + i, 1);
		while (EM[i] == 0);

	assert(i >= 8 + 2);

	EM[i++] = 0;

	(void) memcpy(EM + i, M, mLen);


	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i2 = 0;
		fprintf(stderr, "Encoded Message: \n");
		for (i2 = 0; i2 < mLen; i2++)
			fprintf(stderr, "%2x ", EM[i2]);
		fprintf(stderr, "\n");
	}
	return true;
}

/**
 \ingroup Core_Create
\brief Creates an __ops_pk_session_key_t struct from keydata
\param key Keydata to use
\return __ops_pk_session_key_t struct
\note It is the caller's responsiblity to free the returned pointer
\note Currently hard-coded to use CAST5
\note Currently hard-coded to use RSA
*/
__ops_pk_session_key_t *
__ops_create_pk_session_key(const __ops_keydata_t * key)
{
	/*
         * Creates a random session key and encrypts it for the given key
         *
         * Session Key is for use with a SK algo,
         * can be any, we're hardcoding CAST5 for now
         *
         * Encryption used is PK,
         * can be any, we're hardcoding RSA for now
         */

	const __ops_public_key_t *pub_key = __ops_get_public_key_from_data(key);
#define SZ_UNENCODED_M_BUF CAST_KEY_LENGTH+1+2
	unsigned char   unencoded_m_buf[SZ_UNENCODED_M_BUF];

	const size_t    sz_encoded_m_buf = BN_num_bytes(pub_key->key.rsa.n);
	unsigned char  *encoded_m_buf = calloc(1, sz_encoded_m_buf);

	__ops_pk_session_key_t *session_key = calloc(1, sizeof(*session_key));

	assert(key->type == OPS_PTAG_CT_PUBLIC_KEY);
	session_key->version = OPS_PKSK_V3;
	(void) memcpy(session_key->key_id, key->key_id, sizeof(session_key->key_id));

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;

		fprintf(stderr, "Encrypting for RSA key id : ");
		for (i = 0; i < sizeof(session_key->key_id); i++)
			fprintf(stderr, "%2x ", key->key_id[i]);
		fprintf(stderr, "\n");
	}
	assert(key->key.pkey.algorithm == OPS_PKA_RSA);
	session_key->algorithm = key->key.pkey.algorithm;

	/* \todo allow user to specify other algorithm */
	session_key->symmetric_algorithm = OPS_SA_CAST5;
	__ops_random(session_key->key, CAST_KEY_LENGTH);

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;
		fprintf(stderr, "CAST5 session key created (len=%d):\n ", CAST_KEY_LENGTH);
		for (i = 0; i < CAST_KEY_LENGTH; i++)
			fprintf(stderr, "%2x ", session_key->key[i]);
		fprintf(stderr, "\n");
	}
	if (create_unencoded_m_buf(session_key, &unencoded_m_buf[0]) == false) {
		free(encoded_m_buf);
		return NULL;
	}
	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    i = 0;
		printf("unencoded m buf:\n");
		for (i = 0; i < SZ_UNENCODED_M_BUF; i++)
			printf("%2x ", unencoded_m_buf[i]);
		printf("\n");
	}
	encode_m_buf(&unencoded_m_buf[0], SZ_UNENCODED_M_BUF, pub_key, &encoded_m_buf[0]);

	/* and encrypt it */
	if (!__ops_rsa_encrypt_mpi(encoded_m_buf, sz_encoded_m_buf, pub_key, &session_key->parameters)) {
		free(encoded_m_buf);
		return NULL;
	}
	free(encoded_m_buf);
	return session_key;
}

/**
\ingroup Core_WritePackets
\brief Writes Public Key Session Key packet
\param info Write settings
\param pksk Public Key Session Key to write out
\return true if OK; else false
*/
bool 
__ops_write_pk_session_key(__ops_create_info_t * info,
			 __ops_pk_session_key_t * pksk)
{
	assert(pksk);
	assert(pksk->algorithm == OPS_PKA_RSA);

	return __ops_write_ptag(OPS_PTAG_CT_PK_SESSION_KEY, info)
		&& __ops_write_length((unsigned)(1 + 8 + 1 + BN_num_bytes(pksk->parameters.rsa.encrypted_m) + 2), info)
		&& __ops_write_scalar((unsigned)pksk->version, 1, info)
		&& __ops_write(pksk->key_id, 8, info)
		&& __ops_write_scalar((unsigned)pksk->algorithm, 1, info)
		&& __ops_write_mpi(pksk->parameters.rsa.encrypted_m, info)
	/* ??	&& __ops_write_scalar(0, 2, info); */
		;
}

/**
\ingroup Core_WritePackets
\brief Writes MDC packet
\param hashed Hash for MDC
\param info Write settings
\return true if OK; else false
*/

bool 
__ops_write_mdc(const unsigned char *hashed,
	      __ops_create_info_t * info)
{
	/* write it out */
	return __ops_write_ptag(OPS_PTAG_CT_MDC, info)
	&& __ops_write_length(OPS_SHA1_HASH_SIZE, info)
	&& __ops_write(hashed, OPS_SHA1_HASH_SIZE, info);
}

/**
\ingroup Core_WritePackets
\brief Writes Literal Data packet from buffer
\param data Buffer to write out
\param maxlen Max length of buffer
\param type Literal Data Type
\param info Write settings
\return true if OK; else false
*/
bool 
__ops_write_literal_data_from_buf(const unsigned char *data,
				const int maxlen,
				const __ops_literal_data_type_t type,
				__ops_create_info_t * info)
{
	/*
         * RFC4880 does not specify a meaning for filename or date.
         * It is implementation-dependent.
         * We will not implement them.
         */
	/* \todo do we need to check text data for <cr><lf> line endings ? */
	return __ops_write_ptag(OPS_PTAG_CT_LITERAL_DATA, info)
	&& __ops_write_length((unsigned)(1 + 1 + 4 + maxlen), info)
	&& __ops_write_scalar((unsigned)type, 1, info)
	&& __ops_write_scalar(0, 1, info)
	&& __ops_write_scalar(0, 4, info)
	&& __ops_write(data, (unsigned)maxlen, info);
}

/**
\ingroup Core_WritePackets
\brief Writes Literal Data packet from contents of file
\param filename Name of file to read from
\param type Literal Data Type
\param info Write settings
\return true if OK; else false
*/

bool 
__ops_write_literal_data_from_file(const char *filename,
				 const __ops_literal_data_type_t type,
				 __ops_create_info_t * info)
{
	size_t          initial_size = 1024;
	int             fd = 0;
	bool   rtn;
	unsigned char   buf[1024];
	__ops_memory_t   *mem = NULL;
	size_t          len = 0;

#ifdef O_BINARY
	fd = open(filename, O_RDONLY | O_BINARY);
#else
	fd = open(filename, O_RDONLY);
#endif
	if (fd < 0)
		return false;

	mem = __ops_memory_new();
	__ops_memory_init(mem, initial_size);
	for (;;) {
		ssize_t         n = 0;
		n = read(fd, buf, 1024);
		if (!n)
			break;
		__ops_memory_add(mem, &buf[0], (unsigned)n);
	}
	close(fd);

	/* \todo do we need to check text data for <cr><lf> line endings ? */
	len = __ops_memory_get_length(mem);
	rtn = __ops_write_ptag(OPS_PTAG_CT_LITERAL_DATA, info)
		&& __ops_write_length(1 + 1 + 4 + len, info)
		&& __ops_write_scalar((unsigned)type, 1, info)
		&& __ops_write_scalar(0, 1, info)	/* filename */
		&&__ops_write_scalar(0, 4, info)	/* date */
		&&__ops_write(__ops_memory_get_data(mem), len, info);

	__ops_memory_free(mem);
	return rtn;
}

/**
   \ingroup HighLevel_General

   \brief Reads contents of file into new __ops_memory_t struct.

   \param filename Filename to read from
   \param errnum Pointer to error
   \return new __ops_memory_t pointer containing the contents of the file

   \note If there was an error opening the file or reading from it, errnum is set to the cause

   \note It is the caller's responsibility to call __ops_memory_free(mem)
*/

__ops_memory_t   *
__ops_write_mem_from_file(const char *filename, int *errnum)
{
	size_t          initial_size = 1024;
	int             fd = 0;
	unsigned char   buf[1024];
	__ops_memory_t   *mem = NULL;

	*errnum = 0;

#ifdef O_BINARY
	fd = open(filename, O_RDONLY | O_BINARY);
#else
	fd = open(filename, O_RDONLY);
#endif
	if (fd < 0) {
		*errnum = errno;
		return false;
	}
	mem = __ops_memory_new();
	__ops_memory_init(mem, initial_size);
	for (;;) {
		ssize_t         n = 0;
		n = read(fd, buf, 1024);
		if (n < 0) {
			*errnum = errno;
			break;
		}
		if (!n)
			break;
		__ops_memory_add(mem, &buf[0], (unsigned)n);
	}
	close(fd);
	return mem;
}

/**
   \ingroup HighLevel_General

   \brief Writes contents of buffer into file

   \param filename Filename to write to
   \param buf Buffer to write to file
   \param len Size of buffer
   \param overwrite Flag to set whether to overwrite an existing file
   \return 1 if OK; 0 if error
*/

int 
__ops_write_file_from_buf(const char *filename, const char *buf, const size_t len, const bool overwrite)
{
	int             fd = 0;
	size_t          n = 0;
	int             flags = 0;

	flags = O_WRONLY | O_CREAT;
	if (overwrite == true)
		flags |= O_TRUNC;
	else
		flags |= O_EXCL;
#ifdef O_BINARY
	flags |= O_BINARY;
#endif
	fd = open(filename, flags, 0600);
	if (fd < 0) {
		perror(NULL);
		return 0;
	}
	n = write(fd, buf, len);
	if (n != len)
		return 0;

	if (!close(fd))
		return 1;

	return 0;
}

/**
\ingroup Core_WritePackets
\brief Write Symmetrically Encrypted packet
\param data Data to encrypt
\param len Length of data
\param info Write settings
\return true if OK; else false
\note Hard-coded to use AES256
*/
bool 
__ops_write_symmetrically_encrypted_data(const unsigned char *data,
				       const int len,
				       __ops_create_info_t * info)
{
	unsigned char  *encrypted = (unsigned char *) NULL;
		/* buffer to write encrypted data to */
	__ops_crypt_t	crypt_info;
	size_t		encrypted_sz = 0;	/* size of encrypted data */
	int             done = 0;

	/* \todo assume AES256 for now */
	__ops_crypt_any(&crypt_info, OPS_SA_AES_256);
	__ops_encrypt_init(&crypt_info);

	encrypted_sz = len + crypt_info.blocksize + 2;
	encrypted = calloc(1, encrypted_sz);

	done = __ops_encrypt_se(&crypt_info, encrypted, data, (unsigned)len);
	assert(done == len);
	/* printf("len=%d, done: %d\n", len, done); */

	return __ops_write_ptag(OPS_PTAG_CT_SE_DATA, info)
		&& __ops_write_length(1 + encrypted_sz, info)
		&& __ops_write(data, (unsigned)len, info);
}

/**
\ingroup Core_WritePackets
\brief Write a One Pass Signature packet
\param skey Secret Key to use
\param hash_alg Hash Algorithm to use
\param sig_type Signature type
\param info Write settings
\return true if OK; else false
*/
bool 
__ops_write_one_pass_sig(const __ops_secret_key_t * skey,
		       const __ops_hash_algorithm_t hash_alg,
		       const __ops_sig_type_t sig_type,
		       __ops_create_info_t * info)
{
	unsigned char   keyid[OPS_KEY_ID_SIZE];
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "calling __ops_keyid in write_one_pass_sig: this calls sha1_init\n");
	}
	__ops_keyid(keyid, OPS_KEY_ID_SIZE, OPS_KEY_ID_SIZE, &skey->public_key);

	return __ops_write_ptag(OPS_PTAG_CT_ONE_PASS_SIGNATURE, info)
		&& __ops_write_length(1 + 1 + 1 + 1 + 8 + 1, info)
		&& __ops_write_scalar(3, 1, info)	/* version */
		&& __ops_write_scalar((unsigned)sig_type, 1, info)
		&& __ops_write_scalar((unsigned)hash_alg, 1, info)
		&& __ops_write_scalar((unsigned)skey->public_key.algorithm, 1, info)
		&& __ops_write(keyid, 8, info)
		&& __ops_write_scalar(1, 1, info);
}
