/*	$NetBSD: biff.c,v 1.9.28.1 2008/09/18 04:29:07 wrstuden Exp $	*/

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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)biff.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: biff.c,v 1.9.28.1 2008/09/18 04:29:07 wrstuden Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb;
	int ch;
	const char *name;


	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((name = ttyname(STDERR_FILENO)) == NULL)
		err(2, "tty");

	if (stat(name, &sb))
		err(2, "stat");

	if (*argv == NULL) {
		(void)printf("is %s\n", sb.st_mode&0100 ? "y" : "n");
		exit(sb.st_mode & 0100 ? 0 : 1);
	}

	switch(argv[0][0]) {
	case 'n':
		if (chmod(name, sb.st_mode & ~0100) < 0)
			err(2, "%s", name);
		break;
	case 'y':
		if (chmod(name, sb.st_mode | 0100) < 0)
			err(2, "%s", name);
		break;
	default:
		usage();
	}
	exit(sb.st_mode & 0100 ? 0 : 1);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: biff [y | n]\n");
	exit(2);
}
