/*	$NetBSD: ssh-dss.c,v 1.3.2.3 2001/12/10 23:54:01 he Exp $	*/
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: ssh-dss.c,v 1.10 2001/12/05 10:06:12 deraadt Exp $");

#include <openssl/bn.h>
#include <openssl/evp.h>

#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "compat.h"
#include "log.h"
#include "key.h"
#include "ssh-dss.h"

#define INTBLOB_LEN	20
#define SIGBLOB_LEN	(2*INTBLOB_LEN)

int
ssh_dss_sign(
    Key *key,
    u_char **sigp, int *lenp,
    u_char *data, int datalen)
{
	DSA_SIG *sig;
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	u_char *digest, *ret, sigblob[SIGBLOB_LEN];
	u_int rlen, slen, len, dlen;
	Buffer b;

	if (key == NULL || key->type != KEY_DSA || key->dsa == NULL) {
		error("ssh_dss_sign: no DSA key");
		return -1;
	}
	dlen = evp_md->md_size;
	digest = xmalloc(dlen);
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, NULL);

	sig = DSA_do_sign(digest, dlen, key->dsa);

	memset(digest, 0, dlen);
	xfree(digest);
	if (sig == NULL) {
		error("ssh_dss_sign: sign failed");
		return -1;
	}

	rlen = BN_num_bytes(sig->r);
	slen = BN_num_bytes(sig->s);
	if (rlen > INTBLOB_LEN || slen > INTBLOB_LEN) {
		error("bad sig size %d %d", rlen, slen);
		DSA_SIG_free(sig);
		return -1;
	}
	memset(sigblob, 0, SIGBLOB_LEN);
	BN_bn2bin(sig->r, sigblob+ SIGBLOB_LEN - INTBLOB_LEN - rlen);
	BN_bn2bin(sig->s, sigblob+ SIGBLOB_LEN - slen);
	DSA_SIG_free(sig);

	if (datafellows & SSH_BUG_SIGBLOB) {
		ret = xmalloc(SIGBLOB_LEN);
		memcpy(ret, sigblob, SIGBLOB_LEN);
		if (lenp != NULL)
			*lenp = SIGBLOB_LEN;
		if (sigp != NULL)
			*sigp = ret;
	} else {
		/* ietf-drafts */
		buffer_init(&b);
		buffer_put_cstring(&b, "ssh-dss");
		buffer_put_string(&b, sigblob, SIGBLOB_LEN);
		len = buffer_len(&b);
		ret = xmalloc(len);
		memcpy(ret, buffer_ptr(&b), len);
		buffer_free(&b);
		if (lenp != NULL)
			*lenp = len;
		if (sigp != NULL)
			*sigp = ret;
	}
	return 0;
}
int
ssh_dss_verify(
    Key *key,
    u_char *signature, int signaturelen,
    u_char *data, int datalen)
{
	DSA_SIG *sig;
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	u_char *digest, *sigblob;
	u_int len, dlen;
	int rlen, ret;
	Buffer b;

	if (key == NULL || key->type != KEY_DSA || key->dsa == NULL) {
		error("ssh_dss_verify: no DSA key");
		return -1;
	}

	/* fetch signature */
	if (datafellows & SSH_BUG_SIGBLOB) {
		sigblob = signature;
		len = signaturelen;
	} else {
		/* ietf-drafts */
		char *ktype;
		buffer_init(&b);
		buffer_append(&b, signature, signaturelen);
		ktype = buffer_get_string(&b, NULL);
		if (strcmp("ssh-dss", ktype) != 0) {
			error("ssh_dss_verify: cannot handle type %s", ktype);
			buffer_free(&b);
			xfree(ktype);
			return -1;
		}
		xfree(ktype);
		sigblob = buffer_get_string(&b, &len);
		rlen = buffer_len(&b);
		buffer_free(&b);
		if (rlen != 0) {
			error("ssh_dss_verify: "
			    "remaining bytes in signature %d", rlen);
			xfree(sigblob);
			return -1;
		}
	}

	if (len != SIGBLOB_LEN) {
		fatal("bad sigbloblen %d != SIGBLOB_LEN", len);
	}

	/* parse signature */
	sig = DSA_SIG_new();
	sig->r = BN_new();
	sig->s = BN_new();
	BN_bin2bn(sigblob, INTBLOB_LEN, sig->r);
	BN_bin2bn(sigblob+ INTBLOB_LEN, INTBLOB_LEN, sig->s);

	if (!(datafellows & SSH_BUG_SIGBLOB)) {
		memset(sigblob, 0, len);
		xfree(sigblob);
	}

	/* sha1 the data */
	dlen = evp_md->md_size;
	digest = xmalloc(dlen);
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, NULL);

	ret = DSA_do_verify(digest, dlen, sig, key->dsa);

	memset(digest, 0, dlen);
	xfree(digest);
	DSA_SIG_free(sig);

	debug("ssh_dss_verify: signature %s",
	    ret == 1 ? "correct" : ret == 0 ? "incorrect" : "error");
	return ret;
}
