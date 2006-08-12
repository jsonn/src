/*	$NetBSD: cut.c,v 1.16.4.1 2006/08/12 16:22:56 riz Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cut.c	8.3 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: cut.c,v 1.16.4.1 2006/08/12 16:22:56 riz Exp $");
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	cflag;
char	dchar;
int	dflag;
int	fflag;
int	sflag;

void	c_cut __P((FILE *, char *));
void	f_cut __P((FILE *, char *));
void	get_list __P((char *));
int	main __P((int, char **));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;
	void (*fcn) __P((FILE *, char *));
	int ch;

	fcn = NULL;
	setlocale (LC_ALL, "");

	dchar = '\t';			/* default delimiter is \t */

	/* Since we don't support multi-byte characters, the -c and -b 
	   options are equivalent, and the -n option is meaningless. */
	while ((ch = getopt(argc, argv, "b:c:d:f:sn")) != -1)
		switch(ch) {
		case 'b':
		case 'c':
			fcn = c_cut;
			get_list(optarg);
			cflag = 1;
			break;
		case 'd':
			dchar = *optarg;
			dflag = 1;
			break;
		case 'f':
			get_list(optarg);
			fcn = f_cut;
			fflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'n':
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		if (cflag)
			usage();
	} else if (!cflag || dflag || sflag)
		usage();

	if (*argv)
		for (; *argv; ++argv) {
			if (strcmp(*argv, "-") == 0)
				fcn(stdin, "stdin");
			else {
				if ((fp = fopen(*argv, "r")) == NULL)
					err(1, "%s", *argv);
				fcn(fp, *argv);
				(void)fclose(fp);
			}
		}
	else
		fcn(stdin, "stdin");
	exit(0);
}

int autostart, autostop, maxval;

char positions[_POSIX2_LINE_MAX + 1];

void
get_list(list)
	char *list;
{
	int setautostart, start, stop;
	char *pos;
	char *p;

	/*
	 * set a byte in the positions array to indicate if a field or
	 * column is to be selected; use +1, it's 1-based, not 0-based.
	 * This parser is less restrictive than the Draft 9 POSIX spec.
	 * POSIX doesn't allow lists that aren't in increasing order or
	 * overlapping lists.  We also handle "-3-5" although there's no
	 * real reason too.
	 */
	for (; (p = strtok(list, ", \t")) != NULL; list = NULL) {
		setautostart = start = stop = 0;
		if (*p == '-') {
			++p;
			setautostart = 1;
		}
		if (isdigit((unsigned char)*p)) {
			start = stop = strtol(p, &p, 10);
			if (setautostart && start > autostart)
				autostart = start;
		}
		if (*p == '-') {
			if (isdigit((unsigned char)p[1]))
				stop = strtol(p + 1, &p, 10);
			if (*p == '-') {
				++p;
				if (!autostop || autostop > stop)
					autostop = stop;
			}
		}
		if (*p)
			errx(1, "[-cf] list: illegal list value");
		if (!stop || !start)
			errx(1, "[-cf] list: values may not include zero");
		if (stop > _POSIX2_LINE_MAX)
			errx(1, "[-cf] list: %d too large (max %d)",
			    stop, _POSIX2_LINE_MAX);
		if (maxval < stop)
			maxval = stop;
		for (pos = positions + start; start++ <= stop; *pos++ = 1);
	}

	/* overlapping ranges */
	if (autostop && maxval > autostop)
		maxval = autostop;

	/* set autostart */
	if (autostart)
		memset(positions + 1, '1', autostart);
}

/* ARGSUSED */
void
c_cut(fp, fname)
	FILE *fp;
	char *fname;
{
	int ch, col;
	char *pos;

	ch = 0;
	for (;;) {
		pos = positions + 1;
		for (col = maxval; col; --col) {
			if ((ch = getc(fp)) == EOF)
				return;
			if (ch == '\n')
				break;
			if (*pos++)
				(void)putchar(ch);
		}
		if (ch != '\n') {
			if (autostop)
				while ((ch = getc(fp)) != EOF && ch != '\n')
					(void)putchar(ch);
			else
				while ((ch = getc(fp)) != EOF && ch != '\n');
		}
		(void)putchar('\n');
	}
}

void
f_cut(fp, fname)
	FILE *fp;
	char *fname;
{
	int ch, field, isdelim;
	char *pos, *p, sep;
	int output;
	char lbuf[_POSIX2_LINE_MAX + 1];

	for (sep = dchar; fgets(lbuf, sizeof(lbuf), fp);) {
		output = 0;
		for (isdelim = 0, p = lbuf;; ++p) {
			if (!(ch = *p))
				errx(1, "%s: line too long.", fname);
			/* this should work if newline is delimiter */
			if (ch == sep)
				isdelim = 1;
			if (ch == '\n') {
				if (!isdelim && !sflag)
					(void)printf("%s", lbuf);
				break;
			}
		}
		if (!isdelim)
			continue;

		pos = positions + 1;
		for (field = maxval, p = lbuf; field; --field, ++pos) {
			if (*pos) {
				if (output++)
					(void)putchar(sep);
				while ((ch = *p++) != '\n' && ch != sep)
					(void)putchar(ch);
			} else {
				while ((ch = *p++) != '\n' && ch != sep)
					continue;
			}
			if (ch == '\n')
				break;
		}
		if (ch != '\n') {
			if (autostop) {
				if (output)
					(void)putchar(sep);
				for (; (ch = *p) != '\n'; ++p)
					(void)putchar(ch);
			} else
				for (; (ch = *p) != '\n'; ++p);
		}
		(void)putchar('\n');
	}
}

void
usage()
{
	(void)fprintf(stderr, "usage:\tcut -b list [-n] [file ...]\n"
	    "\tcut -c list [file1 ...]\n"
	    "\tcut -f list [-d delim] [-s] [file ...]\n");
	exit(1);
}
