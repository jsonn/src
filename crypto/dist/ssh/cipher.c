/*	$NetBSD: cipher.c,v 1.1.1.1.2.4 2001/12/10 23:53:04 he Exp $	*/
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: cipher.c,v 1.47 2001/08/23 11:31:59 markus Exp $");

#include "xmalloc.h"
#include "log.h"
#include "cipher.h"

#include <openssl/md5.h>

/* no encryption */
static void
none_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
}
static void
none_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
}
static void
none_crypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	memcpy(dest, src, len);
}

/* DES */
static void
des_ssh1_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	static int dowarn = 1;
	if (dowarn) {
		error("Warning: use of DES is strongly discouraged "
		    "due to cryptographic weaknesses");
		dowarn = 0;
	}
	des_set_key((void *)key, cc->u.des.key);
}
static void
des_ssh1_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
	memset(cc->u.des.iv, 0, sizeof(cc->u.des.iv));
}
static void
des_ssh1_encrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	des_ncbc_encrypt(src, dest, len, cc->u.des.key, &cc->u.des.iv,
	    DES_ENCRYPT);
}
static void
des_ssh1_decrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	des_ncbc_encrypt(src, dest, len, cc->u.des.key, &cc->u.des.iv,
	    DES_DECRYPT);
}

/* 3DES */
static void
des3_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	des_set_key((void *) key, cc->u.des3.key1);
	des_set_key((void *) (key+8), cc->u.des3.key2);
	des_set_key((void *) (key+16), cc->u.des3.key3);
}
static void
des3_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
	memset(cc->u.des3.iv1, 0, sizeof(cc->u.des3.iv1));
	memset(cc->u.des3.iv2, 0, sizeof(cc->u.des3.iv2));
	memset(cc->u.des3.iv3, 0, sizeof(cc->u.des3.iv3));
	if (iv == NULL)
		return;
	memcpy(cc->u.des3.iv3, (char *)iv, 8);
}
static void
des3_cbc_encrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	des_ede3_cbc_encrypt(src, dest, len,
	    cc->u.des3.key1, cc->u.des3.key2, cc->u.des3.key3,
	    &cc->u.des3.iv3, DES_ENCRYPT);
}
static void
des3_cbc_decrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	des_ede3_cbc_encrypt(src, dest, len,
	    cc->u.des3.key1, cc->u.des3.key2, cc->u.des3.key3,
	    &cc->u.des3.iv3, DES_DECRYPT);
}

/*
 * This is used by SSH1:
 *
 * What kind of triple DES are these 2 routines?
 *
 * Why is there a redundant initialization vector?
 *
 * If only iv3 was used, then, this would till effect have been
 * outer-cbc. However, there is also a private iv1 == iv2 which
 * perhaps makes differential analysis easier. On the other hand, the
 * private iv1 probably makes the CRC-32 attack ineffective. This is a
 * result of that there is no longer any known iv1 to use when
 * choosing the X block.
 */
static void
des3_ssh1_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	des_set_key((void *) key, cc->u.des3.key1);
	des_set_key((void *) (key+8), cc->u.des3.key2);
	if (keylen <= 16)
		des_set_key((void *) key, cc->u.des3.key3);
	else
		des_set_key((void *) (key+16), cc->u.des3.key3);
}
static void
des3_ssh1_encrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	des_ncbc_encrypt(src,  dest, len, cc->u.des3.key1, &cc->u.des3.iv1,
	    DES_ENCRYPT);
	des_ncbc_encrypt(dest, dest, len, cc->u.des3.key2, &cc->u.des3.iv2,
	    DES_DECRYPT);
	des_ncbc_encrypt(dest, dest, len, cc->u.des3.key3, &cc->u.des3.iv3,
	    DES_ENCRYPT);
}
static void
des3_ssh1_decrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	des_ncbc_encrypt(src,  dest, len, cc->u.des3.key3, &cc->u.des3.iv3,
	    DES_DECRYPT);
	des_ncbc_encrypt(dest, dest, len, cc->u.des3.key2, &cc->u.des3.iv2,
	    DES_ENCRYPT);
	des_ncbc_encrypt(dest, dest, len, cc->u.des3.key1, &cc->u.des3.iv1,
	    DES_DECRYPT);
}

/* Blowfish */
static void
blowfish_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	BF_set_key(&cc->u.bf.key, keylen, (u_char *)key);
}
static void
blowfish_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
	if (iv == NULL)
		memset(cc->u.bf.iv, 0, 8);
	else
		memcpy(cc->u.bf.iv, (char *)iv, 8);
}
static void
blowfish_cbc_encrypt(CipherContext *cc, u_char *dest, const u_char *src,
     u_int len)
{
	BF_cbc_encrypt((void *)src, dest, len, &cc->u.bf.key, cc->u.bf.iv,
	    BF_ENCRYPT);
}
static void
blowfish_cbc_decrypt(CipherContext *cc, u_char *dest, const u_char *src,
     u_int len)
{
	BF_cbc_encrypt((void *)src, dest, len, &cc->u.bf.key, cc->u.bf.iv,
	    BF_DECRYPT);
}

/*
 * SSH1 uses a variation on Blowfish, all bytes must be swapped before
 * and after encryption/decryption. Thus the swap_bytes stuff (yuk).
 */
static void
swap_bytes(const u_char *src, u_char *dst, int n)
{
	char c[4];

	/* Process 4 bytes every lap. */
	for (n = n / 4; n > 0; n--) {
		c[3] = *src++;
		c[2] = *src++;
		c[1] = *src++;
		c[0] = *src++;

		*dst++ = c[0];
		*dst++ = c[1];
		*dst++ = c[2];
		*dst++ = c[3];
	}
}

static void
blowfish_ssh1_encrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	swap_bytes(src, dest, len);
	BF_cbc_encrypt((void *)dest, dest, len, &cc->u.bf.key, cc->u.bf.iv,
	    BF_ENCRYPT);
	swap_bytes(dest, dest, len);
}
static void
blowfish_ssh1_decrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	swap_bytes(src, dest, len);
	BF_cbc_encrypt((void *)dest, dest, len, &cc->u.bf.key, cc->u.bf.iv,
	    BF_DECRYPT);
	swap_bytes(dest, dest, len);
}

/* alleged rc4 */
static void
arcfour_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	RC4_set_key(&cc->u.rc4, keylen, (u_char *)key);
}
static void
arcfour_crypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	RC4(&cc->u.rc4, len, (u_char *)src, dest);
}

/* CAST */
static void
cast_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	CAST_set_key(&cc->u.cast.key, keylen, (u_char *) key);
}
static void
cast_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
	if (iv == NULL)
		fatal("no IV for %s.", cc->cipher->name);
	memcpy(cc->u.cast.iv, (char *)iv, 8);
}
static void
cast_cbc_encrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	CAST_cbc_encrypt(src, dest, len, &cc->u.cast.key, cc->u.cast.iv,
	    CAST_ENCRYPT);
}
static void
cast_cbc_decrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	CAST_cbc_encrypt(src, dest, len, &cc->u.cast.key, cc->u.cast.iv,
	    CAST_DECRYPT);
}

/* RIJNDAEL */

#define RIJNDAEL_BLOCKSIZE 16
static void
rijndael_setkey(CipherContext *cc, const u_char *key, u_int keylen)
{
	rijndael_set_key(&cc->u.rijndael.enc, (char *)key, 8*keylen, 1);
	rijndael_set_key(&cc->u.rijndael.dec, (char *)key, 8*keylen, 0);
}
static void
rijndael_setiv(CipherContext *cc, const u_char *iv, u_int ivlen)
{
	if (iv == NULL || ivlen != RIJNDAEL_BLOCKSIZE) 
		fatal("bad/no IV for %s.", cc->cipher->name);
	memcpy(cc->u.rijndael.iv, iv, RIJNDAEL_BLOCKSIZE);
}
static void
rijndael_cbc_encrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	rijndael_ctx *ctx = &cc->u.rijndael.enc;
	u_char *iv = cc->u.rijndael.iv;
	u_char in[RIJNDAEL_BLOCKSIZE];
	u_char *cprev, *cnow, *plain;
	int i, j, blocks = len / RIJNDAEL_BLOCKSIZE;

	if (len == 0)
		return;
	if (len % RIJNDAEL_BLOCKSIZE)
		fatal("rijndael_cbc_encrypt: bad len %d", len);
	cnow  = dest;
	plain = (u_char *) src;
	cprev = iv;
	for (i = 0; i < blocks; i++, plain+=RIJNDAEL_BLOCKSIZE,
	    cnow+=RIJNDAEL_BLOCKSIZE) {
		for (j = 0; j < RIJNDAEL_BLOCKSIZE; j++)
			in[j] = plain[j] ^ cprev[j];
		rijndael_encrypt(ctx, in, cnow);
		cprev = cnow;
	}
	memcpy(iv, cprev, RIJNDAEL_BLOCKSIZE);
}
static void
rijndael_cbc_decrypt(CipherContext *cc, u_char *dest, const u_char *src,
    u_int len)
{
	rijndael_ctx *ctx = &cc->u.rijndael.dec;
	u_char *iv = cc->u.rijndael.iv;
	u_char ivsaved[RIJNDAEL_BLOCKSIZE];
	u_char *cnow  = (u_char *) (src+len-RIJNDAEL_BLOCKSIZE);
	u_char *plain = dest+len-RIJNDAEL_BLOCKSIZE;
	u_char *ivp;
	int i, j, blocks = len / RIJNDAEL_BLOCKSIZE;

	if (len == 0)
		return;
	if (len % RIJNDAEL_BLOCKSIZE)
		fatal("rijndael_cbc_decrypt: bad len %d", len);
	memcpy(ivsaved, cnow, RIJNDAEL_BLOCKSIZE);
	for (i = blocks; i > 0; i--, cnow-=RIJNDAEL_BLOCKSIZE,
	    plain-=RIJNDAEL_BLOCKSIZE) {
		rijndael_decrypt(ctx, cnow, plain);
		ivp = (i == 1) ? iv : cnow-RIJNDAEL_BLOCKSIZE;
		for (j = 0; j < RIJNDAEL_BLOCKSIZE; j++)
			plain[j] ^= ivp[j];
	}
	memcpy(iv, ivsaved, RIJNDAEL_BLOCKSIZE);
}

Cipher ciphers[] = {
	{ "none",
		SSH_CIPHER_NONE, 8, 0,
		none_setkey, none_setiv,
		none_crypt, none_crypt },
	{ "des",
		SSH_CIPHER_DES, 8, 8,
		des_ssh1_setkey, des_ssh1_setiv,
		des_ssh1_encrypt, des_ssh1_decrypt },
	{ "3des",
		SSH_CIPHER_3DES, 8, 16,
		des3_ssh1_setkey, des3_setiv,
		des3_ssh1_encrypt, des3_ssh1_decrypt },
	{ "blowfish",
		SSH_CIPHER_BLOWFISH, 8, 16,
		blowfish_setkey, blowfish_setiv,
		blowfish_ssh1_encrypt, blowfish_ssh1_decrypt },

	{ "3des-cbc",
		SSH_CIPHER_SSH2, 8, 24,
		des3_setkey, des3_setiv,
		des3_cbc_encrypt, des3_cbc_decrypt },
	{ "blowfish-cbc",
		SSH_CIPHER_SSH2, 8, 16,
		blowfish_setkey, blowfish_setiv,
		blowfish_cbc_encrypt, blowfish_cbc_decrypt },
	{ "cast128-cbc",
		SSH_CIPHER_SSH2, 8, 16,
		cast_setkey, cast_setiv,
		cast_cbc_encrypt, cast_cbc_decrypt },
	{ "arcfour",
		SSH_CIPHER_SSH2, 8, 16,
		arcfour_setkey, none_setiv,
		arcfour_crypt, arcfour_crypt },
	{ "aes128-cbc",
		SSH_CIPHER_SSH2, 16, 16,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "aes192-cbc",
		SSH_CIPHER_SSH2, 16, 24,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "aes256-cbc",
		SSH_CIPHER_SSH2, 16, 32,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "rijndael128-cbc",
		SSH_CIPHER_SSH2, 16, 16,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "rijndael192-cbc",
		SSH_CIPHER_SSH2, 16, 24,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "rijndael256-cbc",
		SSH_CIPHER_SSH2, 16, 32,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ "rijndael-cbc@lysator.liu.se",
		SSH_CIPHER_SSH2, 16, 32,
		rijndael_setkey, rijndael_setiv,
		rijndael_cbc_encrypt, rijndael_cbc_decrypt },
	{ NULL, SSH_CIPHER_ILLEGAL, 0, 0, NULL, NULL, NULL, NULL }
};

/*--*/

u_int
cipher_mask_ssh1(int client)
{
	u_int mask = 0;
	mask |= 1 << SSH_CIPHER_3DES;           /* Mandatory */
	mask |= 1 << SSH_CIPHER_BLOWFISH;
	if (client) {
		mask |= 1 << SSH_CIPHER_DES;
	}
	return mask;
}

Cipher *
cipher_by_name(const char *name)
{
	Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (strcasecmp(c->name, name) == 0)
			return c;
	return NULL;
}

Cipher *
cipher_by_number(int id)
{
	Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (c->number == id)
			return c;
	return NULL;
}

#define	CIPHER_SEP	","
int
ciphers_valid(const char *names)
{
	Cipher *c;
	char *ciphers, *cp;
	char *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	ciphers = cp = xstrdup(names);
	for ((p = strsep(&cp, CIPHER_SEP)); p && *p != '\0';
	     (p = strsep(&cp, CIPHER_SEP))) {
		c = cipher_by_name(p);
		if (c == NULL || c->number != SSH_CIPHER_SSH2) {
			debug("bad cipher %s [%s]", p, names);
			xfree(ciphers);
			return 0;
		} else {
			debug3("cipher ok: %s [%s]", p, names);
		}
	}
	debug3("ciphers ok: [%s]", names);
	xfree(ciphers);
	return 1;
}

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */

int
cipher_number(const char *name)
{
	Cipher *c;
	if (name == NULL)
		return -1;
	c = cipher_by_name(name);
	return (c==NULL) ? -1 : c->number;
}

char *
cipher_name(int id)
{
	Cipher *c = cipher_by_number(id);
	return (c==NULL) ? "<unknown>" : c->name;
}

void
cipher_init(CipherContext *cc, Cipher *cipher,
    const u_char *key, u_int keylen, const u_char *iv, u_int ivlen)
{
	if (keylen < cipher->key_len)
		fatal("cipher_init: key length %d is insufficient for %s.",
		    keylen, cipher->name);
	if (iv != NULL && ivlen < cipher->block_size)
		fatal("cipher_init: iv length %d is insufficient for %s.",
		    ivlen, cipher->name);
	cc->cipher = cipher;
	cipher->setkey(cc, key, keylen);
	cipher->setiv(cc, iv, ivlen);
}

void
cipher_encrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	if (len % cc->cipher->block_size)
		fatal("cipher_encrypt: bad plaintext length %d", len);
	cc->cipher->encrypt(cc, dest, src, len);
}

void
cipher_decrypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	if (len % cc->cipher->block_size)
		fatal("cipher_decrypt: bad ciphertext length %d", len);
	cc->cipher->decrypt(cc, dest, src, len);
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */

void
cipher_set_key_string(CipherContext *cc, Cipher *cipher,
    const char *passphrase)
{
	MD5_CTX md;
	u_char digest[16];

	MD5_Init(&md);
	MD5_Update(&md, (const u_char *)passphrase, strlen(passphrase));
	MD5_Final(digest, &md);

	cipher_init(cc, cipher, digest, 16, NULL, 0);

	memset(digest, 0, sizeof(digest));
	memset(&md, 0, sizeof(md));
}
