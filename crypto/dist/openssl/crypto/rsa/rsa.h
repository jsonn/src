/* crypto/rsa/rsa.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef HEADER_RSA_H
#define HEADER_RSA_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <openssl/bn.h>
#include <openssl/crypto.h>

#ifdef NO_RSA
#error RSA is disabled.
#endif

typedef struct rsa_st RSA;

typedef struct rsa_meth_st
	{
	const char *name;
	int (*rsa_pub_enc)(int flen,unsigned char *from,unsigned char *to,
			   RSA *rsa,int padding);
	int (*rsa_pub_dec)(int flen,unsigned char *from,unsigned char *to,
			   RSA *rsa,int padding);
	int (*rsa_priv_enc)(int flen,unsigned char *from,unsigned char *to,
			    RSA *rsa,int padding);
	int (*rsa_priv_dec)(int flen,unsigned char *from,unsigned char *to,
			    RSA *rsa,int padding);
	int (*rsa_mod_exp)(BIGNUM *r0,BIGNUM *I,RSA *rsa); /* Can be null */
	int (*bn_mod_exp)(BIGNUM *r, BIGNUM *a, const BIGNUM *p,
			  const BIGNUM *m, BN_CTX *ctx,
			  BN_MONT_CTX *m_ctx); /* Can be null */
	int (*init)(RSA *rsa);		/* called at new */
	int (*finish)(RSA *rsa);	/* called at free */
	int flags;			/* RSA_METHOD_FLAG_* things */
	char *app_data;			/* may be needed! */
	} RSA_METHOD;

struct rsa_st
	{
	/* The first parameter is used to pickup errors where
	 * this is passed instead of aEVP_PKEY, it is set to 0 */
	int pad;
	int version;
	RSA_METHOD *meth;
	BIGNUM *n;
	BIGNUM *e;
	BIGNUM *d;
	BIGNUM *p;
	BIGNUM *q;
	BIGNUM *dmp1;
	BIGNUM *dmq1;
	BIGNUM *iqmp;
	/* be careful using this if the RSA structure is shared */
	CRYPTO_EX_DATA ex_data;
	int references;
	int flags;

	/* Used to cache montgomery values */
	BN_MONT_CTX *_method_mod_n;
	BN_MONT_CTX *_method_mod_p;
	BN_MONT_CTX *_method_mod_q;

	/* all BIGNUM values are actually in the following data, if it is not
	 * NULL */
	char *bignum_data;
	BN_BLINDING *blinding;
	};

#define RSA_3	0x3L
#define RSA_F4	0x10001L

#define RSA_METHOD_FLAG_NO_CHECK	0x01 /* don't check pub/private match */

#define RSA_FLAG_CACHE_PUBLIC		0x02
#define RSA_FLAG_CACHE_PRIVATE		0x04
#define RSA_FLAG_BLINDING		0x08
#define RSA_FLAG_THREAD_SAFE		0x10
/* This flag means the private key operations will be handled by rsa_mod_exp
 * and that they do not depend on the private key components being present:
 * for example a key stored in external hardware. Without this flag bn_mod_exp
 * gets called when private key components are absent.
 */
#define RSA_FLAG_EXT_PKEY		0x20

#define RSA_PKCS1_PADDING	1
#define RSA_SSLV23_PADDING	2
#define RSA_NO_PADDING		3
#define RSA_PKCS1_OAEP_PADDING	4

#define RSA_set_app_data(s,arg)         RSA_set_ex_data(s,0,(char *)arg)
#define RSA_get_app_data(s)             RSA_get_ex_data(s,0)

RSA *	RSA_new(void);
RSA *	RSA_new_method(RSA_METHOD *method);
int	RSA_size(RSA *);
RSA *	RSA_generate_key(int bits, unsigned long e,void
		(*callback)(int,int,void *),void *cb_arg);
int	RSA_check_key(RSA *);
	/* next 4 return -1 on error */
int	RSA_public_encrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
int	RSA_private_encrypt(int flen, unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
int	RSA_public_decrypt(int flen, unsigned char *from, 
		unsigned char *to, RSA *rsa,int padding);
int	RSA_private_decrypt(int flen, unsigned char *from, 
		unsigned char *to, RSA *rsa,int padding);
void	RSA_free (RSA *r);

int	RSA_flags(RSA *r);

void RSA_set_default_method(RSA_METHOD *meth);
RSA_METHOD *RSA_get_default_method(void);
RSA_METHOD *RSA_get_method(RSA *rsa);
RSA_METHOD *RSA_set_method(RSA *rsa, RSA_METHOD *meth);

/* This function needs the memory locking malloc callbacks to be installed */
int RSA_memory_lock(RSA *r);

/* If you have RSAref compiled in. */
RSA_METHOD *RSA_PKCS1_RSAref(void);

/* these are the actual SSLeay RSA functions */
RSA_METHOD *RSA_PKCS1_SSLeay(void);

void	ERR_load_RSA_strings(void );

RSA *	d2i_RSAPublicKey(RSA **a, unsigned char **pp, long length);
int	i2d_RSAPublicKey(RSA *a, unsigned char **pp);
RSA *	d2i_RSAPrivateKey(RSA **a, unsigned char **pp, long length);
int 	i2d_RSAPrivateKey(RSA *a, unsigned char **pp);
#ifndef NO_FP_API
int	RSA_print_fp(FILE *fp, RSA *r,int offset);
#endif

#ifdef HEADER_BIO_H
int	RSA_print(BIO *bp, RSA *r,int offset);
#endif

int i2d_Netscape_RSA(RSA *a, unsigned char **pp, int (*cb)());
RSA *d2i_Netscape_RSA(RSA **a, unsigned char **pp, long length, int (*cb)());
/* Naughty internal function required elsewhere, to handle a MS structure
 * that is the same as the netscape one :-) */
RSA *d2i_Netscape_RSA_2(RSA **a, unsigned char **pp, long length, int (*cb)());

/* The following 2 functions sign and verify a X509_SIG ASN1 object
 * inside PKCS#1 padded RSA encryption */
int RSA_sign(int type, unsigned char *m, unsigned int m_len,
	unsigned char *sigret, unsigned int *siglen, RSA *rsa);
int RSA_verify(int type, unsigned char *m, unsigned int m_len,
	unsigned char *sigbuf, unsigned int siglen, RSA *rsa);

/* The following 2 function sign and verify a ASN1_OCTET_STRING
 * object inside PKCS#1 padded RSA encryption */
int RSA_sign_ASN1_OCTET_STRING(int type, unsigned char *m, unsigned int m_len,
	unsigned char *sigret, unsigned int *siglen, RSA *rsa);
int RSA_verify_ASN1_OCTET_STRING(int type, unsigned char *m, unsigned int m_len,
	unsigned char *sigbuf, unsigned int siglen, RSA *rsa);

int RSA_blinding_on(RSA *rsa, BN_CTX *ctx);
void RSA_blinding_off(RSA *rsa);

int RSA_padding_add_PKCS1_type_1(unsigned char *to,int tlen,
	unsigned char *f,int fl);
int RSA_padding_check_PKCS1_type_1(unsigned char *to,int tlen,
	unsigned char *f,int fl,int rsa_len);
int RSA_padding_add_PKCS1_type_2(unsigned char *to,int tlen,
	unsigned char *f,int fl);
int RSA_padding_check_PKCS1_type_2(unsigned char *to,int tlen,
	unsigned char *f,int fl,int rsa_len);
int RSA_padding_add_PKCS1_OAEP(unsigned char *to,int tlen,
			       unsigned char *f,int fl,unsigned char *p,
			       int pl);
int RSA_padding_check_PKCS1_OAEP(unsigned char *to,int tlen,
				 unsigned char *f,int fl,int rsa_len,
				 unsigned char *p,int pl);
int RSA_padding_add_SSLv23(unsigned char *to,int tlen,
	unsigned char *f,int fl);
int RSA_padding_check_SSLv23(unsigned char *to,int tlen,
	unsigned char *f,int fl,int rsa_len);
int RSA_padding_add_none(unsigned char *to,int tlen,
	unsigned char *f,int fl);
int RSA_padding_check_none(unsigned char *to,int tlen,
	unsigned char *f,int fl,int rsa_len);

int RSA_get_ex_new_index(long argl, char *argp, int (*new_func)(),
	int (*dup_func)(), void (*free_func)());
int RSA_set_ex_data(RSA *r,int idx,char *arg);
char *RSA_get_ex_data(RSA *r, int idx);

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

/* Error codes for the RSA functions. */

/* Function codes. */
#define RSA_F_MEMORY_LOCK				 100
#define RSA_F_RSA_CHECK_KEY				 123
#define RSA_F_RSA_EAY_PRIVATE_DECRYPT			 101
#define RSA_F_RSA_EAY_PRIVATE_ENCRYPT			 102
#define RSA_F_RSA_EAY_PUBLIC_DECRYPT			 103
#define RSA_F_RSA_EAY_PUBLIC_ENCRYPT			 104
#define RSA_F_RSA_GENERATE_KEY				 105
#define RSA_F_RSA_NEW_METHOD				 106
#define RSA_F_RSA_PADDING_ADD_NONE			 107
#define RSA_F_RSA_PADDING_ADD_PKCS1_OAEP		 121
#define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_1		 108
#define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_2		 109
#define RSA_F_RSA_PADDING_ADD_SSLV23			 110
#define RSA_F_RSA_PADDING_CHECK_NONE			 111
#define RSA_F_RSA_PADDING_CHECK_PKCS1_OAEP		 122
#define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1		 112
#define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2		 113
#define RSA_F_RSA_PADDING_CHECK_SSLV23			 114
#define RSA_F_RSA_PRINT					 115
#define RSA_F_RSA_PRINT_FP				 116
#define RSA_F_RSA_SIGN					 117
#define RSA_F_RSA_SIGN_ASN1_OCTET_STRING		 118
#define RSA_F_RSA_VERIFY				 119
#define RSA_F_RSA_VERIFY_ASN1_OCTET_STRING		 120

/* Reason codes. */
#define RSA_R_ALGORITHM_MISMATCH			 100
#define RSA_R_BAD_E_VALUE				 101
#define RSA_R_BAD_FIXED_HEADER_DECRYPT			 102
#define RSA_R_BAD_PAD_BYTE_COUNT			 103
#define RSA_R_BAD_SIGNATURE				 104
#define RSA_R_BLOCK_TYPE_IS_NOT_01			 106
#define RSA_R_BLOCK_TYPE_IS_NOT_02			 107
#define RSA_R_DATA_GREATER_THAN_MOD_LEN			 108
#define RSA_R_DATA_TOO_LARGE				 109
#define RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE		 110
#define RSA_R_DATA_TOO_SMALL				 111
#define RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE		 122
#define RSA_R_D_E_NOT_CONGRUENT_TO_1			 123
#define RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY		 112
#define RSA_R_DMP1_NOT_CONGRUENT_TO_D			 124
#define RSA_R_DMQ1_NOT_CONGRUENT_TO_D			 125
#define RSA_R_IQMP_NOT_INVERSE_OF_Q			 126
#define RSA_R_KEY_SIZE_TOO_SMALL			 120
#define RSA_R_NULL_BEFORE_BLOCK_MISSING			 113
#define RSA_R_N_DOES_NOT_EQUAL_P_Q			 127
#define RSA_R_OAEP_DECODING_ERROR			 121
#define RSA_R_PADDING_CHECK_FAILED			 114
#define RSA_R_P_NOT_PRIME				 128
#define RSA_R_Q_NOT_PRIME				 129
#define RSA_R_SSLV3_ROLLBACK_ATTACK			 115
#define RSA_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD 116
#define RSA_R_UNKNOWN_ALGORITHM_TYPE			 117
#define RSA_R_UNKNOWN_PADDING_TYPE			 118
#define RSA_R_WRONG_SIGNATURE_LENGTH			 119

#ifdef  __cplusplus
}
#endif
#endif

