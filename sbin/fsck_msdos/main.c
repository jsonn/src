/*	$NetBSD: main.c,v 1.1.4.1 1996/05/31 18:41:54 jtc Exp $	*/

/*
 * Copyright (C) 1995 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef lint
static char rcsid[] = "$NetBSD: main.c,v 1.1.4.1 1996/05/31 18:41:54 jtc Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ext.h"

int alwaysno;		/* assume "no" for all questions */
int alwaysyes;		/* assume "yes" for all questions */
int preen;		/* set when preening */
int rdonly;		/* device is opened read only (supersedes above) */

char *fname;		/* filesystem currently checked */
	
static void
usage()
{
	errexit("Usage: fsck_msdos [-pny] filesystem ... \n");
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ret = 0, erg;
	int ch;

	while ((ch = getopt(argc, argv, "vpyn")) != EOF) {
		switch (ch) {
		case 'n':
			alwaysno = 1;
			alwaysyes = preen = 0;
			break;
		case 'y':
			alwaysyes = 1;
			alwaysno = preen = 0;
			break;

		case 'p':
			preen = 1;
			alwaysyes = alwaysno = 0;
			break;
			
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();
	
	while (argc-- > 0) {
		erg = checkfilesys(fname = *argv++);
		if (erg > ret)
			ret = erg;
	}
	exit(ret);
}

/*VARARGS*/
void
#if __STDC__
errexit(const char *fmt, ...)
#else
errexit(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vprintf(fmt, ap);
	va_end(ap);
	exit(8);
}

/*VARARGS*/
void
#if __STDC__
pfatal(const char *fmt, ...)
#else
pfatal(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	
	if (preen)
		printf("%s: ", fname);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
	if (preen)
		exit(8);
}

/*VARARGS*/
void
#if __STDC__
pwarn(const char *fmt, ...)
#else
pwarn(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	
	if (preen)
		printf("%s: ", fname);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vprintf(fmt, ap);
	va_end(ap);
}

void
perror(s)
	const char *s;
{
	pfatal("%s (%s)", s, strerror(errno));
}

/*VARARGS*/
int
#if __STDC__
ask(int def, const char *fmt, ...)
#else
ask(def, fmt, va_alist)
	int def;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	
	char prompt[256];
	int c;

	if (preen) {
		if (rdonly)
			def = 0;
		if (def)
			printf("FIXED\n");
		return def;
	}

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsnprintf(prompt, sizeof(prompt), fmt, ap);
	if (alwaysyes || rdonly) {
		printf("%s? %s\n", prompt, rdonly ? "no" : "yes");
		return !rdonly;
	}
	do {
		printf("%s? [yn] ", prompt);
		fflush(stdout);
		c = getchar();
		while (c != '\n' && getchar() != '\n')
			if (feof(stdin))
				return 0;
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
	return c == 'y' || c == 'Y';
}
