/* crypto/evp/e_ecb_3d.c */
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

#ifndef NO_DES
#include <stdio.h>
#include "cryptlib.h"
#include <openssl/evp.h>
#include <openssl/objects.h>

static void des_ede_init_key(EVP_CIPHER_CTX *ctx, unsigned char *key,
	unsigned char *iv,int enc);
static void des_ede3_init_key(EVP_CIPHER_CTX *ctx, unsigned char *key,
	unsigned char *iv,int enc);
static void des_ede_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
	unsigned char *in, unsigned int inl);
static EVP_CIPHER d_ede_cipher2=
	{
	NID_des_ede,
	8,16,0,
	des_ede_init_key,
	des_ede_cipher,
	NULL,
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.des_ede)),
	NULL,
	NULL,
	};

static EVP_CIPHER d_ede3_cipher3=
	{
	NID_des_ede3,
	8,24,0,
	des_ede3_init_key,
	des_ede_cipher,
	NULL,
	sizeof(EVP_CIPHER_CTX)-sizeof((((EVP_CIPHER_CTX *)NULL)->c))+
		sizeof((((EVP_CIPHER_CTX *)NULL)->c.des_ede)),
	NULL,
	};

EVP_CIPHER *EVP_des_ede(void)
	{
	return(&d_ede_cipher2);
	}

EVP_CIPHER *EVP_des_ede3(void)
	{
	return(&d_ede3_cipher3);
	}

static void des_ede_init_key(EVP_CIPHER_CTX *ctx, unsigned char *key,
	     unsigned char *iv, int enc)
	{
	des_cblock *deskey = (des_cblock *)key;

	if (deskey != NULL)
		{
		des_set_key(&deskey[0],ctx->c.des_ede.ks1);
		des_set_key(&deskey[1],ctx->c.des_ede.ks2);
		memcpy( (char *)ctx->c.des_ede.ks3,
			(char *)ctx->c.des_ede.ks1,
			sizeof(ctx->c.des_ede.ks1));
		}
	}

static void des_ede3_init_key(EVP_CIPHER_CTX *ctx, unsigned char *key,
	     unsigned char *iv, int enc)
	{
	des_cblock *deskey = (des_cblock *)key;

	if (deskey != NULL)
		{
		des_set_key(&deskey[0],ctx->c.des_ede.ks1);
		des_set_key(&deskey[1],ctx->c.des_ede.ks2);
		des_set_key(&deskey[2],ctx->c.des_ede.ks3);
		}
	}

static void des_ede_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
	     unsigned char *in, unsigned int inl)
	{
	unsigned int i;
	des_cblock *output   /* = (des_cblock *)out */;
	des_cblock *input    /* = (des_cblock *)in */;

	if (inl < 8) return;
	inl-=8;
	for (i=0; i<=inl; i+=8)
		{
		output = (des_cblock *)(out + i);
		input = (des_cblock *)(in + i);

		des_ecb3_encrypt(input,output,
			ctx->c.des_ede.ks1,
			ctx->c.des_ede.ks2,
			ctx->c.des_ede.ks3,
			ctx->encrypt);

		/* output++; */
		/* input++; */
		}
	}
#endif
