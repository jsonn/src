/*	$NetBSD: bufaux.h,v 1.1.1.1.2.4 2002/06/06 16:47:44 he Exp $	*/
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
 */

/* RCSID("$OpenBSD: bufaux.h,v 1.13 2001/06/26 17:27:22 markus Exp $"); */

#ifndef BUFAUX_H
#define BUFAUX_H

#include "buffer.h"
#include <openssl/bn.h>

void    buffer_put_bignum(Buffer *, BIGNUM *);
void    buffer_put_bignum2(Buffer *, BIGNUM *);
int     buffer_get_bignum(Buffer *, BIGNUM *);
int	buffer_get_bignum2(Buffer *, BIGNUM *);

u_short	buffer_get_short(Buffer *);
void	buffer_put_short(Buffer *, u_short);

u_int	buffer_get_int(Buffer *);
void    buffer_put_int(Buffer *, u_int);

u_int64_t buffer_get_int64(Buffer *);
void	buffer_put_int64(Buffer *, u_int64_t);

int     buffer_get_char(Buffer *);
void    buffer_put_char(Buffer *, int);

char   *buffer_get_string(Buffer *, u_int *);
void    buffer_put_string(Buffer *, const void *, u_int);
void	buffer_put_cstring(Buffer *, const char *);

#endif				/* BUFAUX_H */
