/*	$KAME: crypto_openssl.c,v 1.86 2004/06/16 11:55:35 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: crypto_openssl.c,v 1.12.2.1 2004/06/17 12:38:09 tron Exp $");

#include <sys/types.h>
#include <sys/param.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

/* get openssl/ssleay version number */
#ifdef HAVE_OPENSSL_OPENSSLV_H
# include <openssl/opensslv.h>
#else
# error no opensslv.h found.
#endif

#ifndef OPENSSL_VERSION_NUMBER
#error OPENSSL_VERSION_NUMBER is not defined. OpenSSL0.9.4 or later required.
#endif

#ifdef HAVE_OPENSSL_PEM_H
#include <openssl/pem.h>
#endif
#ifdef HAVE_OPENSSL_EVP_H
#include <openssl/evp.h>
#endif
#ifdef HAVE_OPENSSL_X509_H
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#endif
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/des.h>
#include <openssl/crypto.h>
#ifdef HAVE_OPENSSL_IDEA_H
#include <openssl/idea.h>
#endif
#include <openssl/blowfish.h>
#ifdef HAVE_OPENSSL_RC5_H
#include <openssl/rc5.h>
#endif
#include <openssl/cast.h>
#include <openssl/err.h>
#ifdef HAVE_OPENSSL_RIJNDAEL_H
#include <openssl/rijndael.h>
#else
#include "crypto/rijndael/rijndael-api-fst.h"
#endif
#ifdef HAVE_OPENSSL_SHA2_H
#include <openssl/sha2.h>
#else
#include "crypto/sha2/sha2.h"
#endif

/* 0.9.7 stuff? */
#if OPENSSL_VERSION_NUMBER < 0x0090700fL
typedef STACK_OF(GENERAL_NAME) GENERAL_NAMES;
#else
#define USE_NEW_DES_API
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "crypto_openssl.h"
#include "debug.h"
#include "gcmalloc.h"

/*
 * I hate to cast every parameter to des_xx into void *, but it is
 * necessary for SSLeay/OpenSSL portability.  It sucks.
 */

#ifdef HAVE_SIGNING_C
static int cb_check_cert_local __P((int, X509_STORE_CTX *));
static int cb_check_cert_remote __P((int, X509_STORE_CTX *));
static X509 *mem2x509 __P((vchar_t *));
#endif

static caddr_t eay_hmac_init __P((vchar_t *, const EVP_MD *));

#ifdef HAVE_SIGNING_C
/* X509 Certificate */
/*
 * convert the string of the subject name into DER
 * e.g. str = "C=JP, ST=Kanagawa";
 */
vchar_t *
eay_str2asn1dn(str, len)
	char *str;
	int len;
{
	X509_NAME *name;
	char *buf;
	char *field, *value;
	int i, j;
	vchar_t *ret;
	caddr_t p;

	buf = racoon_malloc(len + 1);
	if (!buf) {
		printf("failed to allocate buffer\n");
		return NULL;
	}
	memcpy(buf, str, len);

	name = X509_NAME_new();

	field = &buf[0];
	value = NULL;
	for (i = 0; i < len; i++) {
		if (!value && buf[i] == '=') {
			buf[i] = '\0';
			value = &buf[i + 1];
			continue;
		} else if (buf[i] == ',' || buf[i] == '/') {
			buf[i] = '\0';
#if 0
			printf("[%s][%s]\n", field, value);
#endif
			if (!X509_NAME_add_entry_by_txt(name, field,
					MBSTRING_ASC, value, -1, -1, 0))
				goto err;
			for (j = i + 1; j < len; j++) {
				if (buf[j] != ' ')
					break;
			}
			field = &buf[j];
			value = NULL;
			continue;
		}
	}
	buf[len] = '\0';
#if 0
	printf("[%s][%s]\n", field, value);
#endif
	if (!X509_NAME_add_entry_by_txt(name, field,
			MBSTRING_ASC, value, -1, -1, 0))
		goto err;

	i = i2d_X509_NAME(name, NULL);
	if (!i)
		goto err;
	ret = vmalloc(i);
	if (!ret)
		goto err;
	p = ret->v;
	i = i2d_X509_NAME(name, (unsigned char **)&p);
	if (!i)
		goto err;

	return ret;

    err:
	if (buf)
		racoon_free(buf);
	if (name)
		X509_NAME_free(name);
	return NULL;
}

/*
 * compare two subjectNames.
 * OUT:        0: equal
 *	positive:
 *	      -1: other error.
 */
int
eay_cmp_asn1dn(n1, n2)
	vchar_t *n1, *n2;
{
	X509_NAME *a = NULL, *b = NULL;
	caddr_t p;
	int i = -1;

	p = n1->v;
	if (!d2i_X509_NAME(&a, (unsigned char **)&p, n1->l))
		goto end;
	p = n2->v;
	if (!d2i_X509_NAME(&b, (unsigned char **)&p, n2->l))
		goto end;

	i = X509_NAME_cmp(a, b);

    end:
	if (a)
		X509_NAME_free(a);
	if (b)
		X509_NAME_free(b);
	return i;
}

/*
 * this functions is derived from apps/verify.c in OpenSSL0.9.5
 */
int
eay_check_x509cert(cert, CApath, local)
	vchar_t *cert;
	char *CApath;
	int local;
{
	X509_STORE *cert_ctx = NULL;
	X509_LOOKUP *lookup = NULL;
	X509 *x509 = NULL;
#if OPENSSL_VERSION_NUMBER >= 0x00905100L
	X509_STORE_CTX *csc;
#else
	X509_STORE_CTX csc;
#endif
	int error = -1;

	/* XXX define only functions required. */
#if OPENSSL_VERSION_NUMBER >= 0x00905100L
	OpenSSL_add_all_algorithms();
#else
	SSLeay_add_all_algorithms();
#endif

	cert_ctx = X509_STORE_new();
	if (cert_ctx == NULL)
		goto end;

	if (local)
		X509_STORE_set_verify_cb_func(cert_ctx, cb_check_cert_local);
	else
		X509_STORE_set_verify_cb_func(cert_ctx, cb_check_cert_remote);

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_file());
	if (lookup == NULL)
		goto end;
	X509_LOOKUP_load_file(lookup, NULL, X509_FILETYPE_DEFAULT); /* XXX */

	lookup = X509_STORE_add_lookup(cert_ctx, X509_LOOKUP_hash_dir());
	if (lookup == NULL)
		goto end;
	error = X509_LOOKUP_add_dir(lookup, CApath, X509_FILETYPE_PEM);
	if(!error) {
		error = -1;
		goto end;
	}
	error = -1;	/* initialized */

	/* read the certificate to be verified */
	x509 = mem2x509(cert);
	if (x509 == NULL)
		goto end;

#if OPENSSL_VERSION_NUMBER >= 0x00905100L
	csc = X509_STORE_CTX_new();
	if (csc == NULL)
		goto end;
	X509_STORE_CTX_init(csc, cert_ctx, x509, NULL);
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
	X509_STORE_CTX_set_flags (csc, X509_V_FLAG_CRL_CHECK);
	X509_STORE_CTX_set_flags (csc, X509_V_FLAG_CRL_CHECK_ALL);
#endif
	error = X509_verify_cert(csc);
	X509_STORE_CTX_cleanup(csc);
#else
	X509_STORE_CTX_init(&csc, cert_ctx, x509, NULL);
	error = X509_verify_cert(&csc);
	X509_STORE_CTX_cleanup(&csc);
#endif

	/*
	 * if x509_verify_cert() is successful then the value of error is
	 * set non-zero.
	 */
	error = error ? 0 : -1;

end:
	if (error)
		printf("%s\n", eay_strerror());
	if (cert_ctx != NULL)
		X509_STORE_free(cert_ctx);
	if (x509 != NULL)
		X509_free(x509);

	return(error);
}

/*
 * Callback function for verifing certificate.
 * Derived from cb() in openssl/apps/s_server.c
 *
 * This one is called for certificates obtained from 
 * 'peers_certfile' directive.
 */
static int
cb_check_cert_local(ok, ctx)
	int ok;
	X509_STORE_CTX *ctx;
{
	char buf[256];
	int log_tag;

	if (!ok) {
		X509_NAME_oneline(
				X509_get_subject_name(ctx->current_cert),
				buf,
				256);
		/*
		 * since we are just checking the certificates, it is
		 * ok if they are self signed. But we should still warn
		 * the user.
 		 */
		switch (ctx->error) {
		case X509_V_ERR_CERT_HAS_EXPIRED:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
#if OPENSSL_VERSION_NUMBER >= 0x00905100L
		case X509_V_ERR_INVALID_PURPOSE:
		case X509_V_ERR_UNABLE_TO_GET_CRL:
#endif
			ok = 1;
			log_tag = LLV_WARNING;
			break;
		default:
			log_tag = LLV_ERROR;
		}
		plog(log_tag, LOCATION, NULL,
			"%s(%d) at depth:%d SubjectName:%s\n",
			X509_verify_cert_error_string(ctx->error),
			ctx->error,
			ctx->error_depth,
			buf);
	}
	ERR_clear_error();

	return ok;
}

/*
 * Similar to cb_check_cert_local() but this one is called 
 * for certificates obtained from the IKE payload.
 */
static int
cb_check_cert_remote(ok, ctx)
	int ok;
	X509_STORE_CTX *ctx;
{
	char buf[256];
	int log_tag;

	if (!ok) {
		X509_NAME_oneline(
				X509_get_subject_name(ctx->current_cert),
				buf,
				256);

		switch (ctx->error) {
		case X509_V_ERR_UNABLE_TO_GET_CRL:
			ok = 1;
			log_tag = LLV_WARNING;
			break;
		default:
			log_tag = LLV_ERROR;
		}
		plog(log_tag, LOCATION, NULL,
			"%s(%d) at depth:%d SubjectName:%s\n",
			X509_verify_cert_error_string(ctx->error),
			ctx->error,
			ctx->error_depth,
			buf);
	}
	ERR_clear_error();

	return ok;
}

/*
 * get a subjectAltName from X509 certificate.
 */
vchar_t *
eay_get_x509asn1subjectname(cert)
	vchar_t *cert;
{
	X509 *x509 = NULL;
	u_char *bp;
	vchar_t *name = NULL;
	int len;
	int error = -1;

	bp = cert->v;

	x509 = mem2x509(cert);
	if (x509 == NULL)
		goto end;

	/* get the length of the name */
	len = i2d_X509_NAME(x509->cert_info->subject, NULL);
	name = vmalloc(len);
	if (!name)
		goto end;
	/* get the name */
	bp = name->v;
	len = i2d_X509_NAME(x509->cert_info->subject, &bp);

	error = 0;

   end:
	if (error) {
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
		if (name) {
			vfree(name);
			name = NULL;
		}
	}
	if (x509)
		X509_free(x509);

	return name;
}

/*
 * get the subjectAltName from X509 certificate.
 * the name must be terminated by '\0'.
 */
int
eay_get_x509subjectaltname(cert, altname, type, pos)
	vchar_t *cert;
	char **altname;
	int *type;
	int pos;
{
	X509 *x509 = NULL;
        GENERAL_NAMES *gens;
	GENERAL_NAME *gen;
	int i, len;
	int error = -1;

	*altname = NULL;
	*type = GENT_OTHERNAME;

	x509 = mem2x509(cert);
	if (x509 == NULL)
		goto end;

        gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
	if (gens == NULL)
		goto end;

        for(i = 0; i < sk_GENERAL_NAME_num(gens); i++) {
		if (i + 1 != pos)
			continue;
		break;
	}

	/* there is no data at "pos" */
        if (i == sk_GENERAL_NAME_num(gens))
		goto end;

	gen = sk_GENERAL_NAME_value(gens, i);

	/* make sure if the data is terminated by '\0'. */
	if (gen->d.ia5->data[gen->d.ia5->length] != '\0') {
		plog(LLV_ERROR, LOCATION, NULL,
			"data is not terminated by '\0'.");
		hexdump(gen->d.ia5->data, gen->d.ia5->length + 1);
		goto end;
	}

	len = gen->d.ia5->length + 1;
	*altname = racoon_malloc(len);
	if (!*altname)
		goto end;

	strlcpy(*altname, gen->d.ia5->data, len);
	*type = gen->type;

	error = 0;

   end:
	if (error) {
		if (*altname) {
			racoon_free(*altname);
			*altname = NULL;
		}
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
	}
	if (x509)
		X509_free(x509);

	return error;
}

/*
 * decode a X509 certificate and make a readable text terminated '\n'.
 * return the buffer allocated, so must free it later.
 */
char *
eay_get_x509text(cert)
	vchar_t *cert;
{
	X509 *x509 = NULL;
	BIO *bio = NULL;
	char *text = NULL;
	u_char *bp = NULL;
	int len = 0;
	int error = -1;

	x509 = mem2x509(cert);
	if (x509 == NULL)
		goto end;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		goto end;

	error = X509_print(bio, x509);
	if (error != 1) {
		error = -1;
		goto end;
	}

	len = BIO_get_mem_data(bio, &bp);
	text = racoon_malloc(len + 1);
	if (text == NULL)
		goto end;
	memcpy(text, bp, len);
	text[len] = '\0';

	error = 0;

    end:
	if (error) {
		if (text) {
			racoon_free(text);
			text = NULL;
		}
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
	}
	if (bio)
		BIO_free(bio);
	if (x509)
		X509_free(x509);

	return text;
}

/* get X509 structure from buffer. */
static X509 *
mem2x509(cert)
	vchar_t *cert;
{
	X509 *x509;

#ifndef EAYDEBUG
    {
	u_char *bp;

	bp = cert->v;

	x509 = d2i_X509(NULL, &bp, cert->l);
    }
#else
    {
	BIO *bio;
	int len;

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		return NULL;
	len = BIO_write(bio, cert->v, cert->l);
	if (len == -1)
		return NULL;
	x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
    }
#endif
	return x509;
}

/*
 * get a X509 certificate from local file.
 * a certificate must be PEM format.
 * Input:
 *	path to a certificate.
 * Output:
 *	NULL if error occured
 *	other is the cert.
 */
vchar_t *
eay_get_x509cert(path)
	char *path;
{
	FILE *fp;
	X509 *x509;
	vchar_t *cert;
	u_char *bp;
	int len;
	int error;

	/* Read private key */
	fp = fopen(path, "r");
	if (fp == NULL)
		return NULL;
#if OPENSSL_VERSION_NUMBER >= 0x00904100L
	x509 = PEM_read_X509(fp, NULL, NULL, NULL);
#else
	x509 = PEM_read_X509(fp, NULL, NULL);
#endif
	fclose (fp);

	if (x509 == NULL)
		return NULL;

	len = i2d_X509(x509, NULL);
	cert = vmalloc(len);
	if (cert == NULL) {
		X509_free(x509);
		return NULL;
	}
	bp = cert->v;
	error = i2d_X509(x509, &bp);
	X509_free(x509);

	if (error == 0)
		return NULL;

	return cert;
}

/*
 * sign a souce by X509 signature.
 * XXX: to be get hash type from my cert ?
 *	to be handled EVP_dss().
 */
vchar_t *
eay_get_x509sign(source, privkey, cert)
	vchar_t *source;
	vchar_t *privkey;
	vchar_t *cert;
{
	vchar_t *sig = NULL;

	sig = eay_rsa_sign(source, privkey);

	return sig;
}

/*
 * check a X509 signature
 *	XXX: to be get hash type from my cert ?
 *		to be handled EVP_dss().
 * OUT: return -1 when error.
 *	0
 */
int
eay_check_x509sign(source, sig, cert)
	vchar_t *source;
	vchar_t *sig;
	vchar_t *cert;
{
	X509 *x509;
	u_char *bp;
	EVP_PKEY *evp;

	bp = cert->v;

	x509 = d2i_X509(NULL, &bp, cert->l);
	if (x509 == NULL) {
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
		return -1;
	}

	evp = X509_get_pubkey(x509);
	if (!evp) {
		plog(LLV_ERROR, LOCATION, NULL,
		    "X509_get_pubkey: %s\n", eay_strerror());
		return -1;
	}

	return eay_rsa_verify(source, sig, evp);
}

/*
 * check a signature by signed with PKCS7 certificate.
 *	XXX: to be get hash type from my cert ?
 *		to be handled EVP_dss().
 * OUT: return -1 when error.
 *	0
 */
int
eay_check_pkcs7sign(source, sig, cert)
	vchar_t *source;
	vchar_t *sig;
	vchar_t *cert;
{
	X509 *x509;
	EVP_MD_CTX md_ctx;
	EVP_PKEY *evp;
	int error;
	BIO *bio = BIO_new(BIO_s_mem());
	char *bp;

	if (bio == NULL)
		return -1;
	error = BIO_write(bio, cert->v, cert->l);
	if (error != cert->l)
		return -1;

	bp = cert->v;
	x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (x509 == NULL)
		return -1;

	evp = X509_get_pubkey(x509);
	X509_free(x509);
	if (evp == NULL)
		return -1;

	/* Verify the signature */
	/* XXX: to be handled EVP_dss() */
	EVP_VerifyInit(&md_ctx, EVP_sha1());
	EVP_VerifyUpdate(&md_ctx, source->v, source->l);
	error = EVP_VerifyFinal(&md_ctx, sig->v, sig->l, evp);

	EVP_PKEY_free(evp);

	if (error != 1)
		return -1;

	return 0;
}

/*
 * get PKCS#1 Private Key of PEM format from local file.
 */
vchar_t *
eay_get_pkcs1privkey(path)
	char *path;
{
	FILE *fp;
	EVP_PKEY *evp = NULL;
	vchar_t *pkey = NULL;
	u_char *bp;
	int pkeylen;
	int error = -1;

	/* Read private key */
	fp = fopen(path, "r");
	if (fp == NULL)
		return NULL;

#if OPENSSL_VERSION_NUMBER >= 0x00904100L
	evp = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
#else
	evp = PEM_read_PrivateKey(fp, NULL, NULL);
#endif
	fclose (fp);

	if (evp == NULL)
		return NULL;

	pkeylen = i2d_PrivateKey(evp, NULL);
	if (pkeylen == 0)
		goto end;
	pkey = vmalloc(pkeylen);
	if (pkey == NULL)
		goto end;
	bp = pkey->v;
	pkeylen = i2d_PrivateKey(evp, &bp);
	if (pkeylen == 0)
		goto end;

	error = 0;

end:
	if (evp != NULL)
		EVP_PKEY_free(evp);
	if (error != 0 && pkey != NULL) {
		vfree(pkey);
		pkey = NULL;
	}

	return pkey;
}

/*
 * get PKCS#1 Public Key of PEM format from local file.
 */
vchar_t *
eay_get_pkcs1pubkey(path)
	char *path;
{
	FILE *fp;
	EVP_PKEY *evp = NULL;
	vchar_t *pkey = NULL;
	X509 *x509 = NULL;
	u_char *bp;
	int pkeylen;
	int error = -1;

	/* Read private key */
	fp = fopen(path, "r");
	if (fp == NULL)
		return NULL;

#if OPENSSL_VERSION_NUMBER >= 0x00904100L
	x509 = PEM_read_X509(fp, NULL, NULL, NULL);
#else
	x509 = PEM_read_X509(fp, NULL, NULL);
#endif
	fclose (fp);

	if (x509 == NULL)
		return NULL;
  
	/* Get public key - eay */
	evp = X509_get_pubkey(x509);
	if (evp == NULL)
		return NULL;

	pkeylen = i2d_PublicKey(evp, NULL);
	if (pkeylen == 0)
		goto end;
	pkey = vmalloc(pkeylen);
	if (pkey == NULL)
		goto end;
	bp = pkey->v;
	pkeylen = i2d_PublicKey(evp, &bp);
	if (pkeylen == 0)
		goto end;

	error = 0;
end:
	if (evp != NULL)
		EVP_PKEY_free(evp);
	if (error != 0 && pkey != NULL) {
		vfree(pkey);
		pkey = NULL;
	}

	return pkey;
}
#endif

vchar_t *
eay_rsa_sign(src, privkey)
	vchar_t *src, *privkey; 
{
	EVP_PKEY *evp;
	u_char *bp = privkey->v;
	vchar_t *sig = NULL;
	int len;
	int pad = RSA_PKCS1_PADDING;

	/* XXX to be handled EVP_PKEY_DSA */
	evp = d2i_PrivateKey(EVP_PKEY_RSA, NULL, &bp, privkey->l);
	if (evp == NULL)
		return NULL;

	/* XXX: to be handled EVP_dss() */
	/* XXX: Where can I get such parameters ?  From my cert ? */

	len = RSA_size(evp->pkey.rsa);

	sig = vmalloc(len);
	if (sig == NULL)
		return NULL;

	len = RSA_private_encrypt(src->l, src->v, sig->v, evp->pkey.rsa, pad);
	EVP_PKEY_free(evp);
	if (len == 0 || len != sig->l) {
		vfree(sig);
		sig = NULL;
	}

	return sig;
}

int
eay_rsa_verify(src, sig, evp)
	vchar_t *src, *sig;
	EVP_PKEY *evp;
{
	vchar_t *xbuf = NULL;
	int pad = RSA_PKCS1_PADDING;
	int len = 0;
	int error;

	len = RSA_size(evp->pkey.rsa);
	xbuf = vmalloc(len);
	if (xbuf == NULL) {
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
		EVP_PKEY_free(evp);
		return -1;
	}

	len = RSA_public_decrypt(sig->l, sig->v, xbuf->v, evp->pkey.rsa, pad);
	if (len == 0 || len != src->l)
		plog(LLV_ERROR, LOCATION, NULL, "%s\n", eay_strerror());
	EVP_PKEY_free(evp);
	if (len == 0 || len != src->l) {
		vfree(xbuf);
		return -1;
	}

	error = memcmp(src->v, xbuf->v, src->l);
	vfree(xbuf);
	if (error != 0)
		return -1;

	return 0;
}

/*
 * get error string
 * MUST load ERR_load_crypto_strings() first.
 */
char *
eay_strerror()
{
	static char ebuf[512];
	int len = 0, n;
	unsigned long l;
	char buf[200];
#if OPENSSL_VERSION_NUMBER >= 0x00904100L
	const char *file, *data;
#else
	char *file, *data;
#endif
	int line, flags;
	unsigned long es;

	es = CRYPTO_thread_id();

	while ((l = ERR_get_error_line_data(&file, &line, &data, &flags)) != 0){
		n = snprintf(ebuf + len, sizeof(ebuf) - len,
				"%lu:%s:%s:%d:%s ",
				es, ERR_error_string(l, buf), file, line,
				(flags & ERR_TXT_STRING) ? data : "");
		if (n < 0 || n >= sizeof(ebuf) - len)
			break;
		len += n;
		if (sizeof(ebuf) < len)
			break;
	}

	return ebuf;
}

void
eay_init_error()
{
	ERR_load_crypto_strings();
}

/*
 * DES-CBC
 */
vchar_t *
eay_des_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
#ifdef USE_NEW_DES_API
	DES_key_schedule ks;
#else
	des_key_schedule ks;
#endif

	if (data->l % 8)
		return NULL;

#ifdef USE_NEW_DES_API
	if (DES_key_sched((void *)key->v, &ks) != 0)
#else
	if (des_key_sched((void *)key->v, ks) != 0)
#endif
		return NULL;

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
#ifdef USE_NEW_DES_API
	DES_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                &ks, (void *)iv->v, DES_ENCRYPT);
#else
	des_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                ks, (void *)iv->v, DES_ENCRYPT);
#endif

	return res;
}

vchar_t *
eay_des_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
#ifdef USE_NEW_DES_API
	DES_key_schedule ks;
#else
	des_key_schedule ks;
#endif

#ifdef USE_NEW_DES_API
	if (DES_key_sched((void *)key->v, &ks) != 0)
#else
	if (des_key_sched((void *)key->v, ks) != 0)
#endif
		return NULL;

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
#ifdef USE_NEW_DES_API
	DES_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                &ks, (void *)iv->v, DES_DECRYPT);
#else
	des_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                ks, (void *)iv->v, DES_DECRYPT);
#endif

	return res;
}

int
eay_des_weakkey(key)
	vchar_t *key;
{
#ifdef USE_NEW_DES_API
	return DES_is_weak_key((void *)key->v);
#else
	return des_is_weak_key((void *)key->v);
#endif
}

int
eay_des_keylen(len)
	int len;
{
	if (len != 0 && len != 64)
		return -1;
	return 64;
}

#ifdef HAVE_OPENSSL_IDEA_H
/*
 * IDEA-CBC
 */
vchar_t *
eay_idea_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	IDEA_KEY_SCHEDULE ks;

	idea_set_encrypt_key(key->v, &ks);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	idea_cbc_encrypt(data->v, res->v, data->l,
	                &ks, iv->v, IDEA_ENCRYPT);

	return res;
}

vchar_t *
eay_idea_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	IDEA_KEY_SCHEDULE ks, dks;

	idea_set_encrypt_key(key->v, &ks);
	idea_set_decrypt_key(&ks, &dks);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	idea_cbc_encrypt(data->v, res->v, data->l,
	                &dks, iv->v, IDEA_DECRYPT);

	return res;
}

int
eay_idea_weakkey(key)
	vchar_t *key;
{
	return 0;	/* XXX */
}

int
eay_idea_keylen(len)
	int len;
{
	if (len != 0 && len != 128)
		return -1;
	return 128;
}
#endif

/*
 * BLOWFISH-CBC
 */
vchar_t *
eay_bf_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	BF_KEY ks;

	BF_set_key(&ks, key->l, key->v);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	BF_cbc_encrypt(data->v, res->v, data->l,
		&ks, iv->v, BF_ENCRYPT);

	return res;
}

vchar_t *
eay_bf_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	BF_KEY ks;

	BF_set_key(&ks, key->l, key->v);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	BF_cbc_encrypt(data->v, res->v, data->l,
		&ks, iv->v, BF_DECRYPT);

	return res;
}

int
eay_bf_weakkey(key)
	vchar_t *key;
{
	return 0;	/* XXX to be done. refer to RFC 2451 */
}

int
eay_bf_keylen(len)
	int len;
{
	if (len == 0)
		return 448;
	if (len < 40 || len > 448)
		return -1;
	return len;
}

#ifdef HAVE_OPENSSL_RC5_H
/*
 * RC5-CBC
 */
vchar_t *
eay_rc5_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	RC5_32_KEY ks;

	/* in RFC 2451, there is information about the number of round. */
	RC5_32_set_key(&ks, key->l, key->v, 16);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	RC5_32_cbc_encrypt(data->v, res->v, data->l,
		&ks, iv->v, RC5_ENCRYPT);

	return res;
}

vchar_t *
eay_rc5_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	RC5_32_KEY ks;

	/* in RFC 2451, there is information about the number of round. */
	RC5_32_set_key(&ks, key->l, key->v, 16);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	RC5_32_cbc_encrypt(data->v, res->v, data->l,
		&ks, iv->v, RC5_DECRYPT);

	return res;
}

int
eay_rc5_weakkey(key)
	vchar_t *key;
{
	return 0;	/* No known weak keys when used with 16 rounds. */

}

int
eay_rc5_keylen(len)
	int len;
{
	if (len == 0)
		return 128;
	if (len < 40 || len > 2040)
		return -1;
	return len;
}
#endif

/*
 * 3DES-CBC
 */
vchar_t *
eay_3des_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
#ifdef USE_NEW_DES_API
	DES_key_schedule ks1, ks2, ks3;
#else
	des_key_schedule ks1, ks2, ks3;
#endif

	if (key->l < 24)
		return NULL;

#ifdef USE_NEW_DES_API
	if (DES_key_sched((void *)key->v, &ks1) != 0)
		return NULL;
	if (DES_key_sched((void *)(key->v + 8), &ks2) != 0)
		return NULL;
	if (DES_key_sched((void *)(key->v + 16), &ks3) != 0)
		return NULL;
#else
	if (des_key_sched((void *)key->v, ks1) != 0)
		return NULL;
	if (des_key_sched((void *)(key->v + 8), ks2) != 0)
		return NULL;
	if (des_key_sched((void *)(key->v + 16), ks3) != 0)
		return NULL;
#endif

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
#ifdef USE_NEW_DES_API
	DES_ede3_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                &ks1, &ks2, &ks3, (void *)iv->v, DES_ENCRYPT);
#else
	des_ede3_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                ks1, ks2, ks3, (void *)iv->v, DES_ENCRYPT);
#endif

	return res;
}

vchar_t *
eay_3des_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
#ifdef USE_NEW_DES_API
	DES_key_schedule ks1, ks2, ks3;
#else
	des_key_schedule ks1, ks2, ks3;
#endif

	if (key->l < 24)
		return NULL;

#ifdef USE_NEW_DES_API
	if (DES_key_sched((void *)key->v, &ks1) != 0)
		return NULL;
	if (DES_key_sched((void *)(key->v + 8), &ks2) != 0)
		return NULL;
	if (DES_key_sched((void *)(key->v + 16), &ks3) != 0)
		return NULL;
#else
	if (des_key_sched((void *)key->v, ks1) != 0)
		return NULL;
	if (des_key_sched((void *)(key->v + 8), ks2) != 0)
		return NULL;
	if (des_key_sched((void *)(key->v + 16), ks3) != 0)
		return NULL;
#endif

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
#ifdef USE_NEW_DES_API
	DES_ede3_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                &ks1, &ks2, &ks3, (void *)iv->v, DES_DECRYPT);
#else
	des_ede3_cbc_encrypt((void *)data->v, (void *)res->v, data->l,
	                ks1, ks2, ks3, (void *)iv->v, DES_DECRYPT);
#endif

	return res;
}

int
eay_3des_weakkey(key)
	vchar_t *key;
{
	if (key->l < 24)
		return 0;

#ifdef USE_NEW_DES_API
	return (DES_is_weak_key((void *)key->v) ||
	    DES_is_weak_key((void *)(key->v + 8)) ||
	    DES_is_weak_key((void *)(key->v + 16)));
#else
	return (des_is_weak_key((void *)key->v) ||
	    des_is_weak_key((void *)(key->v + 8)) ||
	    des_is_weak_key((void *)(key->v + 16)));
#endif
}

int
eay_3des_keylen(len)
	int len;
{
	if (len != 0 && len != 192)
		return -1;
	return 192;
}

/*
 * CAST-CBC
 */
vchar_t *
eay_cast_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	CAST_KEY ks;

	CAST_set_key(&ks, key->l, key->v);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	CAST_cbc_encrypt(data->v, res->v, data->l,
	                &ks, iv->v, DES_ENCRYPT);

	return res;
}

vchar_t *
eay_cast_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	CAST_KEY ks;

	CAST_set_key(&ks, key->l, key->v);

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	CAST_cbc_encrypt(data->v, res->v, data->l,
	                &ks, iv->v, DES_DECRYPT);

	return res;
}

int
eay_cast_weakkey(key)
	vchar_t *key;
{
	return 0;	/* No known weak keys. */
}

int
eay_cast_keylen(len)
	int len;
{
	if (len == 0)
		return 128;
	if (len < 40 || len > 128)
		return -1;
	return len;
}

/*
 * AES(RIJNDAEL)-CBC
 */
vchar_t *
eay_aes_encrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	keyInstance k;
	cipherInstance c;

	memset(&k, 0, sizeof(k));
	if (rijndael_makeKey(&k, DIR_ENCRYPT, key->l << 3, key->v) < 0)
		return NULL;

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* encryption data */
	memset(&c, 0, sizeof(c));
	if (rijndael_cipherInit(&c, MODE_CBC, iv->v) < 0)
		return NULL;
	if (rijndael_blockEncrypt(&c, &k, data->v, data->l << 3, res->v) < 0)
		return NULL;

	return res;
}

vchar_t *
eay_aes_decrypt(data, key, iv)
	vchar_t *data, *key, *iv;
{
	vchar_t *res;
	keyInstance k;
	cipherInstance c;

	memset(&k, 0, sizeof(k));
	if (rijndael_makeKey(&k, DIR_DECRYPT, key->l << 3, key->v) < 0)
		return NULL;

	/* allocate buffer for result */
	if ((res = vmalloc(data->l)) == NULL)
		return NULL;

	/* decryption data */
	memset(&c, 0, sizeof(c));
	if (rijndael_cipherInit(&c, MODE_CBC, iv->v) < 0)
		return NULL;
	if (rijndael_blockDecrypt(&c, &k, data->v, data->l << 3, res->v) < 0)
		return NULL;

	return res;
}

int
eay_aes_weakkey(key)
	vchar_t *key;
{
	return 0;
}

int
eay_aes_keylen(len)
	int len;
{
	if (len == 0)
		return 128;
	if (len != 128 && len != 192 && len != 256)
		return -1;
	return len;
}

/* for ipsec part */
int
eay_null_hashlen()
{
	return 0;
}

int
eay_kpdk_hashlen()
{
	return 0;
}

int
eay_twofish_keylen(len)
	int len;
{
	if (len < 0 || len > 256)
		return -1;
	return len;
}

int
eay_null_keylen(len)
	int len;
{
	return 0;
}

/*
 * HMAC functions
 */
static caddr_t
eay_hmac_init(key, md)
	vchar_t *key;
	const EVP_MD *md;
{
	HMAC_CTX *c = racoon_malloc(sizeof(*c));

	HMAC_Init(c, key->v, key->l, md);

	return (caddr_t)c;
}

#ifdef WITH_SHA2
/*
 * HMAC SHA2-512
 */
vchar_t *
eay_hmacsha2_512_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_512_init(key);
	eay_hmacsha2_512_update(ctx, data);
	res = eay_hmacsha2_512_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_512_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, EVP_sha2_512());
}

void
eay_hmacsha2_512_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	HMAC_Update((HMAC_CTX *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_512_final(c)
	caddr_t c;
{
	vchar_t *res;
	unsigned int l;

	if ((res = vmalloc(SHA512_DIGEST_LENGTH)) == 0)
		return NULL;

	HMAC_Final((HMAC_CTX *)c, res->v, &l);
	res->l = l;
	(void)racoon_free(c);

	if (SHA512_DIGEST_LENGTH != res->l) {
		plog(LLV_ERROR, LOCATION, NULL,
			"hmac sha2_512 length mismatch %d.\n", res->l);
		vfree(res);
		return NULL;
	}

	return(res);
}

/*
 * HMAC SHA2-384
 */
vchar_t *
eay_hmacsha2_384_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_384_init(key);
	eay_hmacsha2_384_update(ctx, data);
	res = eay_hmacsha2_384_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_384_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, EVP_sha2_384());
}

void
eay_hmacsha2_384_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	HMAC_Update((HMAC_CTX *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_384_final(c)
	caddr_t c;
{
	vchar_t *res;
	unsigned int l;

	if ((res = vmalloc(SHA384_DIGEST_LENGTH)) == 0)
		return NULL;

	HMAC_Final((HMAC_CTX *)c, res->v, &l);
	res->l = l;
	(void)racoon_free(c);

	if (SHA384_DIGEST_LENGTH != res->l) {
		plog(LLV_ERROR, LOCATION, NULL,
			"hmac sha2_384 length mismatch %d.\n", res->l);
		vfree(res);
		return NULL;
	}

	return(res);
}

/*
 * HMAC SHA2-256
 */
vchar_t *
eay_hmacsha2_256_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha2_256_init(key);
	eay_hmacsha2_256_update(ctx, data);
	res = eay_hmacsha2_256_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha2_256_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, EVP_sha2_256());
}

void
eay_hmacsha2_256_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	HMAC_Update((HMAC_CTX *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha2_256_final(c)
	caddr_t c;
{
	vchar_t *res;
	unsigned int l;

	if ((res = vmalloc(SHA256_DIGEST_LENGTH)) == 0)
		return NULL;

	HMAC_Final((HMAC_CTX *)c, res->v, &l);
	res->l = l;
	(void)racoon_free(c);

	if (SHA256_DIGEST_LENGTH != res->l) {
		plog(LLV_ERROR, LOCATION, NULL,
			"hmac sha2_256 length mismatch %d.\n", res->l);
		vfree(res);
		return NULL;
	}

	return(res);
}
#endif	/* WITH_SHA2 */

/*
 * HMAC SHA1
 */
vchar_t *
eay_hmacsha1_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacsha1_init(key);
	eay_hmacsha1_update(ctx, data);
	res = eay_hmacsha1_final(ctx);

	return(res);
}

caddr_t
eay_hmacsha1_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, EVP_sha1());
}

void
eay_hmacsha1_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	HMAC_Update((HMAC_CTX *)c, data->v, data->l);
}

vchar_t *
eay_hmacsha1_final(c)
	caddr_t c;
{
	vchar_t *res;
	unsigned int l;

	if ((res = vmalloc(SHA_DIGEST_LENGTH)) == 0)
		return NULL;

	HMAC_Final((HMAC_CTX *)c, res->v, &l);
	res->l = l;
	(void)racoon_free(c);

	if (SHA_DIGEST_LENGTH != res->l) {
		plog(LLV_ERROR, LOCATION, NULL,
			"hmac sha1 length mismatch %d.\n", res->l);
		vfree(res);
		return NULL;
	}

	return(res);
}

/*
 * HMAC MD5
 */
vchar_t *
eay_hmacmd5_one(key, data)
	vchar_t *key, *data;
{
	vchar_t *res;
	caddr_t ctx;

	ctx = eay_hmacmd5_init(key);
	eay_hmacmd5_update(ctx, data);
	res = eay_hmacmd5_final(ctx);

	return(res);
}

caddr_t
eay_hmacmd5_init(key)
	vchar_t *key;
{
	return eay_hmac_init(key, EVP_md5());
}

void
eay_hmacmd5_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	HMAC_Update((HMAC_CTX *)c, data->v, data->l);
}

vchar_t *
eay_hmacmd5_final(c)
	caddr_t c;
{
	vchar_t *res;
	unsigned int l;

	if ((res = vmalloc(MD5_DIGEST_LENGTH)) == 0)
		return NULL;

	HMAC_Final((HMAC_CTX *)c, res->v, &l);
	res->l = l;
	(void)racoon_free(c);

	if (MD5_DIGEST_LENGTH != res->l) {
		plog(LLV_ERROR, LOCATION, NULL,
			"hmac md5 length mismatch %d.\n", res->l);
		vfree(res);
		return NULL;
	}

	return(res);
}

#ifdef WITH_SHA2
/*
 * SHA2-512 functions
 */
caddr_t
eay_sha2_512_init()
{
	SHA512_CTX *c = racoon_malloc(sizeof(*c));

	SHA512_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_512_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA512_Update((SHA512_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_sha2_512_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA512_DIGEST_LENGTH)) == 0)
		return(0);

	SHA512_Final(res->v, (SHA512_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_512_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_512_init();
	eay_sha2_512_update(ctx, data);
	res = eay_sha2_512_final(ctx);

	return(res);
}
#endif

int
eay_sha2_512_hashlen()
{
	return SHA512_DIGEST_LENGTH << 3;
}

#ifdef WITH_SHA2
/*
 * SHA2-384 functions
 */
caddr_t
eay_sha2_384_init()
{
	SHA384_CTX *c = racoon_malloc(sizeof(*c));

	SHA384_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_384_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA384_Update((SHA384_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_sha2_384_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA384_DIGEST_LENGTH)) == 0)
		return(0);

	SHA384_Final(res->v, (SHA384_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_384_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_384_init();
	eay_sha2_384_update(ctx, data);
	res = eay_sha2_384_final(ctx);

	return(res);
}
#endif

int
eay_sha2_384_hashlen()
{
	return SHA384_DIGEST_LENGTH << 3;
}

#ifdef WITH_SHA2
/*
 * SHA2-256 functions
 */
caddr_t
eay_sha2_256_init()
{
	SHA256_CTX *c = racoon_malloc(sizeof(*c));

	SHA256_Init(c);

	return((caddr_t)c);
}

void
eay_sha2_256_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA256_Update((SHA256_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_sha2_256_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA256_DIGEST_LENGTH)) == 0)
		return(0);

	SHA256_Final(res->v, (SHA256_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha2_256_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha2_256_init();
	eay_sha2_256_update(ctx, data);
	res = eay_sha2_256_final(ctx);

	return(res);
}
#endif

int
eay_sha2_256_hashlen()
{
	return SHA256_DIGEST_LENGTH << 3;
}

/*
 * SHA functions
 */
caddr_t
eay_sha1_init()
{
	SHA_CTX *c = racoon_malloc(sizeof(*c));

	SHA1_Init(c);

	return((caddr_t)c);
}

void
eay_sha1_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	SHA1_Update((SHA_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_sha1_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(SHA_DIGEST_LENGTH)) == 0)
		return(0);

	SHA1_Final(res->v, (SHA_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_sha1_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_sha1_init();
	eay_sha1_update(ctx, data);
	res = eay_sha1_final(ctx);

	return(res);
}

int
eay_sha1_hashlen()
{
	return SHA_DIGEST_LENGTH << 3;
}

/*
 * MD5 functions
 */
caddr_t
eay_md5_init()
{
	MD5_CTX *c = racoon_malloc(sizeof(*c));

	MD5_Init(c);

	return((caddr_t)c);
}

void
eay_md5_update(c, data)
	caddr_t c;
	vchar_t *data;
{
	MD5_Update((MD5_CTX *)c, data->v, data->l);

	return;
}

vchar_t *
eay_md5_final(c)
	caddr_t c;
{
	vchar_t *res;

	if ((res = vmalloc(MD5_DIGEST_LENGTH)) == 0)
		return(0);

	MD5_Final(res->v, (MD5_CTX *)c);
	(void)racoon_free(c);

	return(res);
}

vchar_t *
eay_md5_one(data)
	vchar_t *data;
{
	caddr_t ctx;
	vchar_t *res;

	ctx = eay_md5_init();
	eay_md5_update(ctx, data);
	res = eay_md5_final(ctx);

	return(res);
}

int
eay_md5_hashlen()
{
	return MD5_DIGEST_LENGTH << 3;
}

/*
 * eay_set_random
 *   size: number of bytes.
 */
vchar_t *
eay_set_random(size)
	u_int32_t size;
{
	BIGNUM *r = NULL;
	vchar_t *res = 0;

	if ((r = BN_new()) == NULL)
		goto end;
	BN_rand(r, size * 8, 0, 0);
	eay_bn2v(&res, r);

end:
	if (r)
		BN_free(r);
	return(res);
}

/* DH */
int
eay_dh_generate(prime, g, publen, pub, priv)
	vchar_t *prime, **pub, **priv;
	u_int publen;
	u_int32_t g;
{
	BIGNUM *p = NULL;
	DH *dh = NULL;
	int error = -1;

	/* initialize */
	/* pre-process to generate number */
	if (eay_v2bn(&p, prime) < 0)
		goto end;

	if ((dh = DH_new()) == NULL)
		goto end;
	dh->p = p;
	p = NULL;	/* p is now part of dh structure */
	dh->g = NULL;
	if ((dh->g = BN_new()) == NULL)
		goto end;
	if (!BN_set_word(dh->g, g))
		goto end;

	if (publen != 0)
		dh->length = publen;

	/* generate public and private number */
	if (!DH_generate_key(dh))
		goto end;

	/* copy results to buffers */
	if (eay_bn2v(pub, dh->pub_key) < 0)
		goto end;
	if (eay_bn2v(priv, dh->priv_key) < 0) {
		vfree(*pub);
		goto end;
	}

	error = 0;

end:
	if (dh != NULL)
		DH_free(dh);
	if (p != 0)
		BN_free(p);
	return(error);
}

int
eay_dh_compute(prime, g, pub, priv, pub2, key)
	vchar_t *prime, *pub, *priv, *pub2, **key;
	u_int32_t g;
{
	BIGNUM *dh_pub = NULL;
	DH *dh = NULL;
	int l;
	caddr_t v = NULL;
	int error = -1;

	/* make public number to compute */
	if (eay_v2bn(&dh_pub, pub2) < 0)
		goto end;

	/* make DH structure */
	if ((dh = DH_new()) == NULL)
		goto end;
	if (eay_v2bn(&dh->p, prime) < 0)
		goto end;
	if (eay_v2bn(&dh->pub_key, pub) < 0)
		goto end;
	if (eay_v2bn(&dh->priv_key, priv) < 0)
		goto end;
	dh->length = pub2->l * 8;

	dh->g = NULL;
	if ((dh->g = BN_new()) == NULL)
		goto end;
	if (!BN_set_word(dh->g, g))
		goto end;

	if ((v = (caddr_t)racoon_calloc(prime->l, sizeof(u_char))) == NULL)
		goto end;
	if ((l = DH_compute_key(v, dh_pub, dh)) == -1)
		goto end;
	memcpy((*key)->v + (prime->l - l), v, l);

	error = 0;

end:
	if (dh_pub != NULL)
		BN_free(dh_pub);
	if (dh != NULL)
		DH_free(dh);
	if (v != NULL)
		racoon_free(v);
	return(error);
}

#if 1
int
eay_v2bn(bn, var)
	BIGNUM **bn;
	vchar_t *var;
{
	if ((*bn = BN_bin2bn(var->v, var->l, NULL)) == NULL)
		return -1;

	return 0;
}
#else
/*
 * convert vchar_t <-> BIGNUM.
 *
 * vchar_t: unit is u_char, network endian, most significant byte first.
 * BIGNUM: unit is BN_ULONG, each of BN_ULONG is in host endian,
 *	least significant BN_ULONG must come first.
 *
 * hex value of "0x3ffe050104" is represented as follows:
 *	vchar_t: 3f fe 05 01 04
 *	BIGNUM (BN_ULONG = u_int8_t): 04 01 05 fe 3f
 *	BIGNUM (BN_ULONG = u_int16_t): 0x0104 0xfe05 0x003f
 *	BIGNUM (BN_ULONG = u_int32_t_t): 0xfe050104 0x0000003f
 */
int
eay_v2bn(bn, var)
	BIGNUM **bn;
	vchar_t *var;
{
	u_char *p;
	u_char *q;
	BN_ULONG *r;
	int l;
	BN_ULONG num;

	*bn = BN_new();
	if (*bn == NULL)
		goto err;
	l = (var->l * 8 + BN_BITS2 - 1) / BN_BITS2;
	if (bn_expand(*bn, l * BN_BITS2) == NULL)
		goto err;
	(*bn)->top = l;

	/* scan from least significant byte */
	p = (u_char *)var->v;
	q = (u_char *)(var->v + var->l);
	r = (*bn)->d;
	num = 0;
	l = 0;
	do {
		q--;
		num = num | ((BN_ULONG)*q << (l++ * 8));
		if (l == BN_BYTES) {
			*r++ = num;
			num = 0;
			l = 0;
		}
	} while (p < q);
	if (l)
		*r = num;
	return 0;

err:
	if (*bn)
		BN_free(*bn);
	return -1;
}
#endif

int
eay_bn2v(var, bn)
	vchar_t **var;
	BIGNUM *bn;
{
	*var = vmalloc(bn->top * BN_BYTES);
	if (*var == NULL)
		return(-1);

	(*var)->l = BN_bn2bin(bn, (*var)->v);

	return 0;
}

const char *
eay_version()
{
	return SSLeay_version(SSLEAY_VERSION);
}
