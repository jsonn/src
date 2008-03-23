/*	main.c,v 1.18 2007/03/10 00:30:36 hubertf Exp	*/

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


#include <sys/cdefs.h>
#ifndef lint
__RCSID("main.c,v 1.18 2007/03/10 00:30:36 hubertf Exp");
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>

#include "fsutil.h"
#include "ext.h"
#include "exitvalues.h"

int alwaysno;		/* assume "no" for all questions */
int alwaysyes;		/* assume "yes" for all questions */
int preen;		/* set when preening */
int rdonly;		/* device is opened read only (supersedes above) */

static void usage(void) __dead;

static void
usage(void)
{
    	(void)fprintf(stderr, "Usage: %s [-fnpy] filesystem ... \n",
	    getprogname());
	exit(FSCK_EXIT_USAGE);
}

static void
catch(int n)
{
	exit(FSCK_EXIT_SIGNALLED);
}

int
main(int argc, char **argv)
{
	int ret = FSCK_EXIT_OK, erg;
	int ch;

	while ((ch = getopt(argc, argv, "pPqynf")) != -1) {
		switch (ch) {
		case 'f':
			/*
			 * We are always forced, since we don't
			 * have a clean flag
			 */
			break;
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

		case 'P':		/* Progress meter not implemented. */
			break;

		case 'q':		/* Quiet not implemented. */
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

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void) signal(SIGINT, catch);
	if (preen)
		(void) signal(SIGQUIT, catch);

	while (--argc >= 0) {
		setcdevname(*argv, preen);
		erg = checkfilesys(*argv++);
		if (erg > ret)
			ret = erg;
	}

	return ret;
}


/*VARARGS*/
int
ask(int def, const char *fmt, ...)
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

	va_start(ap, fmt);
	vsnprintf(prompt, sizeof(prompt), fmt, ap);
	va_end(ap);
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
