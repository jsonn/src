/*	$NetBSD: panic.c,v 1.6.10.1 2001/02/03 21:01:47 he Exp $	*/

/*
 * panic.c - terminate fast in case of error
 * Copyright (c) 1993 by Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* System Headers */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Local headers */

#include "panic.h"
#include "at.h"
#include "privs.h"

/* File scope variables */

#ifndef lint
#if 0
static char rcsid[] = "$OpenBSD: panic.c,v 1.4 1997/03/01 23:40:09 millert Exp $";
#else
__RCSID("$NetBSD: panic.c,v 1.6.10.1 2001/02/03 21:01:47 he Exp $");
#endif
#endif

/* External variables */

/* Global functions */

void
panic(a)
	char *a;
{

	/*
	 * Something fatal has happened, print error message and exit.
	 */
	(void)fprintf(stderr, "%s: %s\n", namep, a);
	if (fcreated) {
		PRIV_START
		(void)unlink(atfile);
		PRIV_END
	}

	exit(EXIT_FAILURE);
}

void
perr(a)
	char *a;
{

	/*
	 * Some operating system error; print error message and exit.
	 */
	perror(a);
	if (fcreated) {
		PRIV_START
		(void)unlink(atfile);
		PRIV_END
	}

	exit(EXIT_FAILURE);
}

void 
perr2(a, b)
	char *a, *b;
{

	(void)fputs(a, stderr);
	perr(b);
}

void
usage()
{

	/* Print usage and exit.  */
	(void)fprintf(stderr,   "Usage: at [-V] [-q x] [-f file] [-m] -t [[CC]YY]MMDDhhmm[.SS]\n"
				"       at [-V] [-q x] [-f file] [-m] timespec\n"
				"       at [-V] -c job [job ...]\n"
				"       atq [-V] [-q x] [-v]\n"
				"       atrm [-V] job [job ...]\n"
				"       batch [-V] [-f file] [-m] [-t [[CC]YY]MMDDhhmm[.SS]]\n"
				"       batch [-V] [-f file] [-m] [timespec]\n");
	exit(EXIT_FAILURE);
}
