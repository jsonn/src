/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: md5.h,v 1.1.1.1.4.2 2000/06/16 18:45:40 thorpej Exp $ */

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#ifdef KRB5
#include <krb5-types.h>
#elif defined(KRB4)
#include <ktypes.h>
#endif

struct md5 {
  unsigned int offset;
  unsigned int sz;
  u_int32_t counter[4];
  unsigned char save[64];
};

void md5_init (struct md5 *m);
void md5_update (struct md5 *m, const void *p, size_t len);
void md5_finito (struct md5 *m, void *res); /* u_int32_t res[4] */

/*
 * Functions for compatibility that have never been tested.
 */
typedef struct {
  u_int32_t i[2];		/* number of _bits_ handled mod 2^64 */
  u_int32_t buf[4];		/* scratch buffer */
  unsigned char in[64];		/* input buffer */
} MD5_CTX_PREAMBLE;

typedef struct {
  union {
    MD5_CTX_PREAMBLE preamble_;
    struct md5 d5;
  } m;
} MD5_CTX;

void MD5Init (MD5_CTX *mdContext);
void MD5Update (MD5_CTX *mdContext,
		const unsigned char *inBuf,
		unsigned int inLen);
void MD5Final (unsigned char digest[16], MD5_CTX *mdContext);

#ifndef NO_MD5_MACROS
#define MD5Init(mdContext) md5_init(&(mdContext)->m.d5)
#define MD5Update(mdCtx, inBuf, inLen) md5_update(&(mdCtx)->m.d5, inBuf, inLen)
#define MD5Final(digest, mdCtx) md5_finito(&(mdCtx)->m.d5, (digest))
#endif
