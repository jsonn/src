/*	$NetBSD: misc.c,v 1.7.2.1 1998/01/29 10:53:36 mellon Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
#if 0
static char sccsid[] = "@(#)misc.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: misc.c,v 1.7.2.1 1998/01/29 10:53:36 mellon Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"
#include "pathnames.h"

/*
 * find the index of second str in the first str.
 */
int
indx(s1, s2)
	char *s1;
	char *s2;
{
	char *t;

	t = strstr(s1, s2);
	if (t == NULL)
		return (-1);
	return (t - s1);
}

/*
 *  putback - push character back onto input
 */
void
putback(c)
	unsigned char c;
{
	if (bp < endpbb)
		*bp++ = c;
	else
		errx(1, "too many characters pushed back");
}

/*
 *  putbackeof - push EOF back onto input
 */
void
putbackeof()
{
	if (bp < endpbb)
		*bp++ = EOF;
	else
		errx(1, "too many characters pushed back");
}

/*
 *  pbstr - push string back onto input
 *          putback is replicated to improve
 *          performance.
 */
void
pbstr(s)
	char *s;
{
	char *es;
	pbent *zp;

	es = s;
	zp = bp;

	while (*es)
		es++;
	es--;
	while (es >= s)
		if (zp < endpbb)
			*zp++ = (unsigned char)*es--;
	if ((bp = zp) == endpbb)
		errx(1, "too many characters pushed back");
}

/*
 *  pbnum - convert number to string, push back on input.
 */
void
pbnum(n)
	int n;
{
	int num;

	num = (n < 0) ? -n : n;
	do {
		putback(num % 10 + '0');
	}
	while ((num /= 10) > 0);

	if (n < 0)
		putback('-');
}

/*
 *  chrsave - put single char on string space
 */
void
chrsave(c)
	char c;
{
	if (ep < endest)
		*ep++ = c;
	else
		errx(1, "string space overflow");
}

/*
 * read in a diversion file, and dispose it.
 */
void
getdiv(n)
	int n;
{
	int c;
	FILE *dfil;

	if (active == outfile[n])
		errx(1, "undivert: diversion still active");
	(void) fclose(outfile[n]);
	outfile[n] = NULL;
	m4temp[UNIQUE] = n + '0';
	if ((dfil = fopen(m4temp, "r")) == NULL)
		err(1, "%s: cannot undivert", m4temp);
	else
		while ((c = getc(dfil)) != EOF)
			putc(c, active);
	(void) fclose(dfil);

#ifdef vms
	if (remove(m4temp))
#else
	if (unlink(m4temp) == -1)
#endif
		err(1, "%s: cannot unlink", m4temp);
}

void
onintr(signo)
	int signo;
{
	errx(1, "interrupted");
}

/*
 * killdiv - get rid of the diversion files
 */
void
killdiv()
{
	int n;

	for (n = 0; n < MAXOUT; n++)
		if (outfile[n] != NULL) {
			(void) fclose(outfile[n]);
			m4temp[UNIQUE] = n + '0';
#ifdef vms
			(void) remove(m4temp);
#else
			(void) unlink(m4temp);
#endif
		}
}

char *
xalloc(n)
	unsigned long n;
{
	char *p = malloc(n);

	if (p == NULL)
		err(1, "malloc");
	return p;
}

char *
xstrdup(s)
	const char *s;
{
	char *p = strdup(s);
	if (p == NULL)
		err(1, "strdup");
	return p;
}

char *
basename(s)
	char *s;
{
	char *p;

	if ((p = strrchr(s, '/')) == NULL)
		return s;

	return ++p;
}

void
usage()
{
	fprintf(stderr, "usage: m4 [-Dname[=val]] [-Uname]\n");
	exit(1);
}
