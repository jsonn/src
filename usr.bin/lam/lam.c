/*	$NetBSD: lam.c,v 1.6.6.1 2009/05/13 19:19:54 jym Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lam.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: lam.c,v 1.6.6.1 2009/05/13 19:19:54 jym Exp $");
#endif /* not lint */

/*
 *	lam - laminate files
 *	Author:  John Kunze, UCB
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	MAXOFILES	20
#define	BIGBUFSIZ	5 * BUFSIZ

struct	openfile {		/* open file structure */
	FILE	*fp;		/* file pointer */
	short	eof;		/* eof flag */
	short	pad;		/* pad flag for missing columns */
	char	eol;		/* end of line character */
	const char *sepstring;	/* string to print before each line */
	const char *format;	/* printf(3) style string spec. */
}	input[MAXOFILES];

int	morefiles;		/* set by getargs(), changed by gatherline() */
int	nofinalnl;		/* normally append \n to each output line */
char	line[BIGBUFSIZ];
char	*linep;

void	 error __P((const char *, const char *));
char	*gatherline __P((struct openfile *));
void	 getargs __P((char *[]));
int	 main __P((int, char **));
char	*pad __P((struct openfile *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct	openfile *ip;

	getargs(argv);
	if (!morefiles)
		error("lam - laminate files", "");
	for (;;) {
		linep = line;
		for (ip = input; ip->fp != NULL; ip++)
			linep = gatherline(ip);
		if (!morefiles)
			exit(0);
		fputs(line, stdout);
		fputs(ip->sepstring, stdout);
		if (!nofinalnl)
			putchar('\n');
	}
}

void
getargs(av)
	char *av[];
{
	struct openfile *ip = input;
	char *p, *c;
	static char fmtbuf[BUFSIZ];
	char *fmtp = fmtbuf;
	int P, S, F, T;

	P = S = F = T = 0;		/* capitalized options */
	while ((p = *++av) != NULL) {
		if (*p != '-' || !p[1]) {
			if (++morefiles >= MAXOFILES)
				errx(1, "too many input files");
			if (*p == '-')
				ip->fp = stdin;
			else if ((ip->fp = fopen(p, "r")) == NULL)
				errx(1, "open %s", p);
			ip->pad = P;
			if (!ip->sepstring)
				ip->sepstring = (S ? (ip-1)->sepstring : "");
			if (!ip->format)
				ip->format = ((P || F) ? (ip-1)->format : "%s");
			if (!ip->eol)
				ip->eol = (T ? (ip-1)->eol : '\n');
			ip++;
			continue;
		}
		c = ++p;
		switch (tolower((unsigned char) *c)) {
		case 's':
			if (*++p || (p = *++av))
				ip->sepstring = p;
			else
				error("Need string after -%s", c);
			S = (*c == 'S' ? 1 : 0);
			break;
		case 't':
			if (*++p || (p = *++av))
				ip->eol = *p;
			else
				error("Need character after -%s", c);
			T = (*c == 'T' ? 1 : 0);
			nofinalnl = 1;
			break;
		case 'p':
			ip->pad = 1;
			P = (*c == 'P' ? 1 : 0);
			/* FALLTHROUGH */
		case 'f':
			F = (*c == 'F' ? 1 : 0);
			if (*++p || (p = *++av)) {
				fmtp += strlen(fmtp) + 1;
				if (fmtp >= fmtbuf + sizeof(fmtbuf))
					errx(1, "no more format space");
				/* restrict format string to only valid width formatters */
				if (strspn(p, "-.0123456789") != strlen(p))
					errx(1, "invalid format string `%s'", p);
				if (snprintf(fmtp, fmtbuf + sizeof(fmtbuf) - fmtp, "%%%ss", p)
					>= fmtbuf + sizeof(fmtbuf) - fmtp)
						errx(1, "no more format space");
				sprintf(fmtp, "%%%ss", p);
				ip->format = fmtp;
			}
			else
				error("Need string after -%s", c);
			break;
		default:
			error("What do you mean by -%s?", c);
			break;
		}
	}
	ip->fp = NULL;
	if (!ip->sepstring)
		ip->sepstring = "";
}

char *
pad(ip)
	struct openfile *ip;
{
	char *lp = linep;

	strlcpy(lp, ip->sepstring, line + sizeof(line) - lp);
	lp += strlen(lp);
	if (ip->pad) {
		snprintf(lp, line + sizeof(line) - lp, ip->format, "");
		lp += strlen(lp);
	}
	return (lp);
}

char *
gatherline(ip)
	struct openfile *ip;
{
	char s[BUFSIZ];
	int c;
	char *p;
	char *lp = linep;
	char *end = s + sizeof(s) - 1;

	if (ip->eof)
		return (pad(ip));
	for (p = s; (c = fgetc(ip->fp)) != EOF && p < end; p++)
		if ((*p = c) == ip->eol)
			break;
	*p = '\0';
	if (c == EOF) {
		ip->eof = 1;
		if (ip->fp == stdin)
			fclose(stdin);
		morefiles--;
		return (pad(ip));
	}
	strlcpy(lp, ip->sepstring, line + sizeof(line) - lp);
	lp += strlen(lp);
	snprintf(lp, line + sizeof(line) - lp, ip->format, s);
	lp += strlen(lp);
	return (lp);
}

void
error(const char *msg, const char *s)
{
	warnx(msg, s);
	fprintf(stderr,
"\nUsage:  lam [ -[fp] min.max ] [ -s sepstring ] [ -t c ] file ...\n");
	if (strncmp("lam - ", msg, 6) == 0)
		fprintf(stderr, "Options:\n\t%s\t%s\t%s\t%s\t%s",
		    "-f min.max	field widths for file fragments\n",
		    "-p min.max	like -f, but pad missing fragments\n",
		    "-s sepstring	fragment separator\n",
"-t c		input line terminator is c, no \\n after output lines\n",
		    "Capitalized options affect more than one file.\n");
	exit(1);
}
