/*	$NetBSD: term.c,v 1.13.2.1 2000/06/23 16:40:06 minoura Exp $	*/

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
static char sccsid[] = "@(#)term.c	8.1 (Berkeley) 6/9/93";
#endif
__RCSID("$NetBSD: term.c,v 1.13.2.1 2000/06/23 16:40:06 minoura Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <ttyent.h>
#include <unistd.h>
#include "extern.h"

char    *tbuf;      		/* Termcap entry. */

const	char *askuser __P((const char *));
char	*ttys __P((char *));

/*
 * Figure out what kind of terminal we're dealing with, and then read in
 * its termcap entry.
 */
const char *
get_termcap_entry(userarg, tcapbufp, extended)
	const char *userarg;
	char **tcapbufp;
	int extended;
{
	struct ttyent *t;
	int rval;
	char *p, *ttypath;
	const char *ttype;
	char zz[1024], *zz_ptr;
	char *ext_tc, *newptr;

	if (userarg) {
		ttype = userarg;
		goto found;
	}

	/* Try the environment. */
	if ((ttype = getenv("TERM")) != NULL)
		goto map;

	/* Try ttyname(3); check for dialup or other mapping. */
	if ((ttypath = ttyname(STDERR_FILENO)) != NULL) {
		if ((p = strrchr(ttypath, '/')) != NULL)
			++p;
		else
			p = ttypath;
		if ((t = getttynam(p))) {
			ttype = t->ty_type;
			goto map;
		}
	}

	/* If still undefined, use "unknown". */
	ttype = "unknown";

map:	ttype = mapped(ttype);

	/*
	 * If not a path, remove TERMCAP from the environment so we get a
	 * real entry from /etc/termcap.  This prevents us from being fooled
	 * by out of date stuff in the environment.
	 */
found:	if ((p = getenv("TERMCAP")) != NULL && *p != '/')
		unsetenv("TERMCAP");

	/*
	 * ttype now contains a pointer to the type of the terminal.
	 * If the first character is '?', ask the user.
	 */
	if (ttype[0] == '?') {
		if (ttype[1] != '\0')
			ttype = askuser(ttype + 1);
		else
			ttype = askuser(NULL);
	}
	/* Find the termcap entry.  If it doesn't exist, ask the user. */
	if ((tbuf = (char *) malloc(1024)) == NULL) {
		fprintf(stderr, "Could not malloc termcap buffer\n");
		exit(1);
	}
	
	while ((rval = tgetent(tbuf, ttype)) == 0) {
		warnx("terminal type %s is unknown", ttype);
		ttype = askuser(NULL);
	}
	if (rval == -1) {
		if (!errno)
			errno = ENOENT;
		err(1, NULL);
	}

	  /* check if we get a truncated termcap entry, fish back the full
	   * one if need be and the user has asked for it.
	   */
	zz_ptr = zz;
	if ((extended == 1) && (tgetstr("ZZ", &zz_ptr) != NULL)) {
			  /* it was, fish back the full termcap */
		sscanf(zz, "%p", &ext_tc);
		if ((newptr = (char *) realloc(tbuf, strlen(ext_tc) + 1))
		    == NULL) {
			fprintf(stderr,
				"reallocate of termcap falied\n");
			exit (1);
		}

		strcpy(newptr, ext_tc);
		tbuf = newptr;
	}
	
	*tcapbufp = tbuf;
	return (ttype);
}

/* Prompt the user for a terminal type. */
const char *
askuser(dflt)
	const char *dflt;
{
	static char answer[256];
	char *p;

	/* We can get recalled; if so, don't continue uselessly. */
	if (feof(stdin) || ferror(stdin)) {
		(void)fprintf(stderr, "\n");
		exit(1);
	}
	for (;;) {
		if (dflt)
			(void)fprintf(stderr, "Terminal type? [%s] ", dflt);
		else
			(void)fprintf(stderr, "Terminal type? ");
		(void)fflush(stderr);

		if (fgets(answer, sizeof(answer), stdin) == NULL) {
			if (dflt == NULL) {
				(void)fprintf(stderr, "\n");
				exit(1);
			}
			return (dflt);
		}

		if ((p = strchr(answer, '\n')) != NULL)
			*p = '\0';
		if (answer[0])
			return (answer);
		if (dflt != NULL)
			return (dflt);
	}
}
