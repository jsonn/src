/*
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
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
/* RCSID("$OpenBSD: compat.h,v 1.15 2001/02/19 09:53:31 markus Exp $"); */

#ifndef COMPAT_H
#define COMPAT_H

#define	SSH_PROTO_UNKNOWN 	0x00
#define	SSH_PROTO_1		0x01
#define	SSH_PROTO_1_PREFERRED	0x02
#define	SSH_PROTO_2		0x04

#define SSH_BUG_SIGBLOB		0x0001
#define SSH_BUG_PKSERVICE	0x0002
#define SSH_BUG_HMAC		0x0004
#define SSH_BUG_X11FWD		0x0008
#define SSH_OLD_SESSIONID	0x0010
#define SSH_BUG_PKAUTH		0x0020
#define SSH_BUG_DEBUG		0x0040
#define SSH_BUG_BANNER		0x0080
#define SSH_BUG_IGNOREMSG	0x0100

void    enable_compat13(void);
void    enable_compat20(void);
void    compat_datafellows(const char *s);
int	proto_spec(const char *spec);
extern int compat13;
extern int compat20;
extern int datafellows;
#endif
