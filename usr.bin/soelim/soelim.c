/*	$NetBSD: soelim.c,v 1.6 1998/02/03 04:12:24 perry Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)soelim.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: soelim.c,v 1.6 1998/02/03 04:12:24 perry Exp $");
#endif /* not lint */

/*
 * soelim - a filter to process n/troff input eliminating .so's
 *
 * Author: Bill Joy UCB July 8, 1977
 *
 * This program eliminates .so's from a n/troff input stream.
 * It can be used to prepare safe input for submission to the
 * phototypesetter since the software supporting the operator
 * doesn't let him do chdir.
 *
 * This is a kludge and the operator should be given the
 * ability to do chdir.
 *
 * This program is more generally useful, it turns out, because
 * the program tbl doesn't understand ".so" directives.
 */
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	STDIN_NAME	"-"

struct path {
	char **list;
	size_t n, c;
};

static int	 process __P((struct path *, char *));
static void	 initpath __P((struct path *));
static void	 addpath __P((struct path *,  const char *));
static FILE	*openpath __P((struct path *, const char *, const char *));

int	main __P((int, char **));


static void
initpath(p)
	struct path *p;
{
	p->list = NULL;
	p->n = p->c = 0;
}

static void
addpath(p, dir)
	struct path *p;
	const char *dir;
{
	if (p->list == NULL || p->n <= p->c - 2) {
		p->n += 10;
		p->list = realloc(p->list, p->n * sizeof(p->list[0]));
		if (p->list == NULL)
			err(1, "%s", "");
	}

	if ((p->list[p->c++] = strdup(dir)) == NULL)
		err(1, "%s", "");

	p->list[p->c] = NULL;
}

static FILE *
openpath(p, name, parm)
	struct path *p;
	const char *name;
	const char *parm;
{
	char filename[MAXPATHLEN];
	const char *f;
	FILE *fp;
	size_t i;

	if (*name == '/' || p->c == 0)
		return fopen(name, parm);

	for (i = 0; i < p->c; i++) {
		if (p->list[i][0] == '\0')
			f = name;
		else {
			(void)snprintf(filename, sizeof(filename), "%s/%s", 
			    p->list[i], name);
			f = filename;
		}
		if ((fp = fopen(f, parm)) != NULL)
			return fp;
	}
	return NULL;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *__progname;
	struct path p;
	int c;

	initpath(&p);
	addpath(&p, ".");

	while ((c = getopt(argc, argv, "I:")) != -1)
		switch (c) {
		case 'I':
			addpath(&p, optarg);
			break;
		default:
			(void)fprintf(stderr,
			    "Usage: %s [-I<dir>] [files...]\n", __progname);
			exit(1);
		}

	argc -= optind;
	argv += optind;
			
	if (argc == 0) {
		(void)process(&p, STDIN_NAME);
		exit(0);
	}
	do {
		(void)process(&p, argv[0]);
		argv++;
		argc--;
	} while (argc > 0);
	exit(0);
}

int
process(p, file)
	struct path *p;
	char *file;
{
	char *cp;
	int c;
	char fname[BUFSIZ];
	FILE *soee;
	int isfile;

	if (!strcmp(file, STDIN_NAME)) {
		soee = stdin;
	} else {
		soee = openpath(p, file, "r");
		if (soee == NULL) {
			warn("Cannot open `%s'", file);
			return(-1);
		}
	}
	for (;;) {
		c = getc(soee);
		if (c == EOF)
			break;
		if (c != '.')
			goto simple;
		c = getc(soee);
		if (c != 's') {
			putchar('.');
			goto simple;
		}
		c = getc(soee);
		if (c != 'o') {
			printf(".s");
			goto simple;
		}
		do
			c = getc(soee);
		while (c == ' ' || c == '\t');
		cp = fname;
		isfile = 0;
		for (;;) {
			switch (c) {

			case ' ':
			case '\t':
			case '\n':
			case EOF:
				goto donename;

			default:
				*cp++ = c;
				c = getc(soee);
				isfile++;
				continue;
			}
		}
donename:
		if (cp == fname) {
			printf(".so");
			goto simple;
		}
		*cp = 0;
		if (process(p, fname) < 0)
			if (isfile)
				printf(".so %s\n", fname);
		continue;
simple:
		if (c == EOF)
			break;
		putchar(c);
		if (c != '\n') {
			c = getc(soee);
			goto simple;
		}
	}
	if (soee != stdin) {
		fclose(soee);
	}
	return(0);
}
