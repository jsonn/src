/*	$NetBSD: extern.h,v 1.4.42.1 2009/05/13 19:20:01 jym Exp $	*/

/*-
 * Copyright (c) 1991 Keith Muller.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *      from: @(#)extern.h	8.1 (Berkeley) 6/6/93
 *	$NetBSD: extern.h,v 1.4.42.1 2009/05/13 19:20:01 jym Exp $
 */

extern int eoptind;
extern char *eoptarg;

void	 addnum __P((char *, int, int));
int	 egetopt __P((int, char * const *, const char *));
void	 flsh_errs __P((void));
int	 horzcol __P((int, char **));
int	 inln __P((FILE *, char *, int, int *, int, int *));
int	 inskip __P((FILE *, int, int));
void	 mfail __P((void));
int	 mulfile __P((int, char **));
FILE	*nxtfile __P((int, char **, const char **, char *, int));
int	 onecol __P((int, char **));
int	 otln __P((char *, int, int *, int *, int));
void	 pfail __P((void));
int	 prhead __P((char *, const char *, int));
int	 prtail __P((int, int));
int	 setup __P((int, char **));
void	 terminate __P((int));
void	 usage __P((void));
int	 vertcol __P((int, char **));
