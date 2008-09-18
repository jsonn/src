/*	$NetBSD: colrm.c,v 1.7.28.1 2008/09/18 04:29:09 wrstuden Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)colrm.c	8.2 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: colrm.c,v 1.7.28.1 2008/09/18 04:29:09 wrstuden Exp $");
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	TAB	8

void	check(FILE *);
void	usage(void);

int
main(int argc, char *argv[])
{
	u_long column, start, stop;
	int ch;
	char *p;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	start = stop = 0;
	switch(argc) {
	case 2:
		stop = strtol(argv[1], &p, 10);
		if (stop <= 0 || *p)
			errx(1, "illegal column -- %s", argv[1]);
		/* FALLTHROUGH */
	case 1:
		start = strtol(argv[0], &p, 10);
		if (start <= 0 || *p)
			errx(1, "illegal column -- %s", argv[0]);
		break;
	case 0:
		break;
	default:
		usage();
	}

	if (stop && start > stop)
		err(1, "illegal start and stop columns");

	for (column = 0;;) {
		switch (ch = getchar()) {
		case EOF:
			check(stdin);
			break;
		case '\b':
			if (column)
				--column;
			break;
		case '\n':
			column = 0;
			break;
		case '\t':
			column = (column + TAB) & ~(TAB - 1);
			break;
		default:
			++column;
			break;
		}

		if ((!start || column < start || (stop && column > stop)) &&
		    putchar(ch) == EOF)
			check(stdout);
	}
}

void
check(FILE *stream)
{
	if (feof(stream))
		exit(0);
	if (ferror(stream))
		err(1, "%s", stream == stdin ? "stdin" : "stdout");
}

void
usage()
{
	(void)fprintf(stderr, "usage: colrm [start [stop]]\n");
	exit(1);
}
