/*	$NetBSD: getNAME.c,v 1.7.2.1 1997/11/10 19:54:46 thorpej Exp $	*/

/*-
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
#if 0
static char sccsid[] = "@(#)getNAME.c	8.1 (Berkeley) 6/30/93";
#else
__RCSID("$NetBSD: getNAME.c,v 1.7.2.1 1997/11/10 19:54:46 thorpej Exp $");
#endif
#endif /* not lint */

/*
 * Get name sections from manual pages.
 *	-t	for building toc
 *	-i	for building intro entries
 *	-w	for querying type of manual source
 *	other	apropos database
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int tocrc;
int intro;
int typeflag;

void doname __P((char *));
void dorefname __P((char *));
void getfrom __P((char *));
void split __P((char *, char *));
void trimln __P((char *));
void usage __P((void));
int main __P((int, char *[]));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;

	while ((ch = getopt(argc, argv, "itw")) != -1)
		switch (ch) {
		case 'i':
			intro = 1;
			break;
		case 't':
			tocrc = 1;
			break;
		case 'w':
			typeflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		usage();

	for (; *argv; ++argv)
		getfrom(*argv);
	exit(0);
}

void
getfrom(pathname)
	char *pathname;
{
	int i = 0;
	char *name, *loc, *s, *t;
	char headbuf[BUFSIZ];
	char linbuf[BUFSIZ];
	char savebuf[BUFSIZ];

	if (freopen(pathname, "r", stdin) == 0) {
		perror(pathname);
		return;
	}
	if ((name = strrchr(pathname, '/')))
		name++;
	else
		name = pathname;
	for (;;) {
		if (fgets(headbuf, sizeof headbuf, stdin) == NULL) {
			if (typeflag)
				printf("%-60s	UNKNOWN\n", pathname);
			return;
		}
		if (headbuf[0] != '.')
			continue;
		if ((headbuf[1] == 'T' && headbuf[2] == 'H') ||
		    (headbuf[1] == 't' && headbuf[2] == 'h'))
			break;
		if (headbuf[1] == 'D' && headbuf[2] == 't')
			goto newman;
	}
	if (typeflag) {
		printf("%-60s	OLD\n", pathname);
		return;
	}
	for (;;) {
		if (fgets(linbuf, sizeof linbuf, stdin) == NULL)
			return;
		if (linbuf[0] != '.')
			continue;
		if (linbuf[1] == 'S' && linbuf[2] == 'H')
			break;
		if (linbuf[1] == 's' && linbuf[2] == 'h')
			break;
	}
	trimln(headbuf);
	if (tocrc)
		doname(name);
	linbuf[0] = '\0';
	for (;;) {
		if (fgets(headbuf, sizeof headbuf, stdin) == NULL)
			break;
		if (headbuf[0] == '.') {
			if (headbuf[1] == 'S' && headbuf[2] == 'H')
				break;
			if (headbuf[1] == 's' && headbuf[2] == 'h')
				break;
		}
		if (i != 0)
			strcat(linbuf, " ");
		i++;
		trimln(headbuf);
		strcat(linbuf, headbuf);
		/* change the \- into (N) - */
		if ((s = strstr(linbuf, "\\-")) != NULL) {
			strncpy(savebuf, s+1, BUFSIZ);
			if ((t = strchr(name, '.')) != NULL) {
				t++;
				*s++ = '(';
				while (*t)
					*s++ = *t++;
				*s++ = ')';
				*s++ = ' ';
				*s++ = '\0';
			}
			strcat(linbuf, savebuf);
		}
	}
	if (intro)
		split(linbuf, name);
	else
		printf("%s\n", linbuf);
	return;

newman:
	if (typeflag) {
		printf("%-60s	NEW\n", pathname);
		return;
	}
	for (;;) {
		if (fgets(linbuf, sizeof linbuf, stdin) == NULL)
			return;
		if (linbuf[0] != '.')
			continue;
		if (linbuf[1] == 'S' && linbuf[2] == 'h')
			break;
	}
	trimln(headbuf);
	if (tocrc)
		doname(name);
	linbuf[0] = '\0';
	for (;;) {
		if (fgets(headbuf, sizeof headbuf, stdin) == NULL)
			break;
		if (headbuf[0] == '.') {
			if (headbuf[1] == 'S' && headbuf[2] == 'h')
				break;
		}
		if (i != 0)
			strcat(linbuf, " ");
		i++;
		trimln(headbuf);
		for (loc = strchr(headbuf, ' '); loc; loc = strchr(loc, ' '))
			if (loc[1] == ',')
				strcpy(loc, &loc[1]);
			else
				loc++;
		if (headbuf[0] != '.') {
			strcat(linbuf, headbuf);
		} else {
			/*
			 * Get rid of quotes in macros.
			 */
			for (loc = strchr(&headbuf[4], '"'); loc; ) {
				strcpy(loc, &loc[1]);
				loc = strchr(loc, '"');
			}
			/*
			 * Handle cross references
			 */
			if (headbuf[1] == 'X' && headbuf[2] == 'r') {
				for (loc = &headbuf[4]; *loc != ' '; loc++)
					continue;
				loc[0] = '(';
				loc[2] = ')';
				loc[3] = '\0';
			}

			/*
			 * Put section and dash between names and description.
			 */
			if (headbuf[1] == 'N' && headbuf[2] == 'd') {
				if ((t = strchr(name, '.')) != NULL) {
					strcat(linbuf, "(");
					strcat(linbuf, t+1);
					strcat(linbuf, ") ");
				}
				strcat(linbuf, "- ");
			}
			/*
			 * Skip over macro names.
			 */
			strcat(linbuf, &headbuf[4]);
		}
	}
	if (intro)
		split(linbuf, name);
	else
		printf("%s\n", linbuf);
}

void
trimln(cp)
	char *cp;
{

	while (*cp)
		cp++;
	if (*--cp == '\n')
		*cp = 0;
}

void
doname(name)
	char *name;
{
	char *dp = name, *ep;

again:
	while (*dp && *dp != '.')
		putchar(*dp++);
	if (*dp)
		for (ep = dp+1; *ep; ep++)
			if (*ep == '.') {
				putchar(*dp++);
				goto again;
			}
	putchar('(');
	if (*dp)
		dp++;
	while (*dp)
		putchar (*dp++);
	putchar(')');
	putchar(' ');
}

void
split(line, name)
	char *line, *name;
{
	char *cp, *dp;
	char *sp, *sep;

	cp = strchr(line, '-');
	if (cp == 0)
		return;
	sp = cp + 1;
	for (--cp; *cp == ' ' || *cp == '\t' || *cp == '\\'; cp--)
		;
	*++cp = '\0';
	while (*sp && (*sp == ' ' || *sp == '\t'))
		sp++;
	for (sep = "", dp = line; dp && *dp; dp = cp, sep = "\n") {
		cp = strchr(dp, ',');
		if (cp) {
			char *tp;

			for (tp = cp - 1; *tp == ' ' || *tp == '\t'; tp--)
				;
			*++tp = '\0';
			for (++cp; *cp == ' ' || *cp == '\t'; cp++)
				;
		}
		printf("%s%s\t", sep, dp);
		dorefname(name);
		printf("\t%s", sp);
	}
}

void
dorefname(name)
	char *name;
{
	char *dp = name, *ep;

again:
	while (*dp && *dp != '.')
		putchar(*dp++);
	if (*dp)
		for (ep = dp+1; *ep; ep++)
			if (*ep == '.') {
				putchar(*dp++);
				goto again;
			}
	putchar('.');
	if (*dp)
		dp++;
	while (*dp)
		putchar (*dp++);
}

void
usage()
{
	(void)fprintf(stderr, "usage: getNAME [-itw] file ...\n");
	exit(1);
}
