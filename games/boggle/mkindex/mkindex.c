/*	$NetBSD: mkindex.c,v 1.6.4.1 1999/12/27 18:28:59 wrstuden Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Barry Brachman.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkindex.c	8.1 (Berkeley) 6/11/93";
#else
__RCSID("$NetBSD: mkindex.c,v 1.6.4.1 1999/12/27 18:28:59 wrstuden Exp $");
#endif
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "bog.h"

int main __P((void));
char *nextword __P((FILE *, char *, int *, int *));

int
main(void)
{
	int clen, rlen, prev, i;
	long off, start;
	char buf[MAXWORDLEN + 1];

	prev = '\0';
	off = start = 0L;
	while (nextword(stdin, buf, &clen, &rlen) != NULL) {
		if (*buf != prev) {
			/*
			 * Boggle expects a full index even if the dictionary
			 * had no words beginning with some letters.
			 * So we write out entries for every letter from prev
			 * to *buf.
			 */
			if (prev != '\0')
				printf("%c %6ld %6ld\n", prev, start, off - 1);
			for (i = (prev ? prev + 1 : 'a'); i < *buf; i++)
				printf("%c %6ld %6ld\n", i, off, off - 1);
			prev = *buf;
			start = off;
		}
		off += clen + 1;
	}
	printf("%c %6ld %6ld\n", prev, start, off - 1);
	for (i = prev + 1; i <= 'z'; i++)
		printf("%c %6ld %6ld\n", i, off, off - 1);
	fflush(stdout);
	if (ferror(stdout))
		err(1, "writing standard output");
	exit(0);
}

/*
 * Return the next word in the compressed dictionary in 'buffer' or
 * NULL on end-of-file
 * Also set clen to the length of the compressed word (for mkindex) and
 * rlen to the strlen() of the real word
 */
char *
nextword(fp, buffer, clen, rlen)
	FILE *fp;
	char *buffer;
	int *clen, *rlen;
{
	int ch, pcount;
	char *p, *q;
	static char buf[MAXWORDLEN + 1];
	static int first = 1;
	static int lastch = 0;

   	if (first) {
		if ((pcount = getc(fp)) == EOF)
			return (NULL);
		first = 0;
	}
	else if ((pcount = lastch) == EOF)
		return (NULL);

	p = buf + (*clen = pcount);
 
	while ((ch = getc(fp)) != EOF && ch >= 'a')
			*p++ = ch;
		lastch = ch;
	*p = '\0';

	*rlen = (int) (p - buf);
	*clen = *rlen - *clen;

	p = buf;
	q = buffer;
	while ((*q++ = *p) != '\0') {
		if (*p++ == 'q')
			*q++ = 'u';
	}
	return (buffer);
}
