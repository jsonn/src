/*	$NetBSD: sem.h,v 1.6.2.1 1997/01/14 21:29:01 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sem.h	8.1 (Berkeley) 6/6/93
 */

void		enddefs __P((void));

void		setdefmaxusers __P((int, int, int));
void		setmaxusers __P((int));
int		defattr __P((const char *, struct nvlist *));
void		defdev __P((struct devbase *, int, struct nvlist *,
			struct nvlist *));
void		defdevattach __P((struct deva *, struct devbase *,
			struct nvlist *, struct nvlist *));
struct devbase *getdevbase __P((const char *name));
struct deva    *getdevattach __P((const char *name));
struct attr    *getattr __P((const char *name));
void		setmajor __P((struct devbase *d, int n));
void		addconf __P((struct config *));
void		setconf __P((struct nvlist **, const char *, struct nvlist *));
void		setfstype __P((const char **, const char *));
void		adddev __P((const char *, const char *, struct nvlist *, int));
void		addpseudo __P((const char *name, int number));
const char     *ref __P((const char *name));
const char     *starref __P((const char *name));
const char     *wildref __P((const char *name));

extern const char *s_qmark;
extern const char *s_nfs;
