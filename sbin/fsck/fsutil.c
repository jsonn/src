/*	$NetBSD: fsutil.c,v 1.4.2.1 1997/10/31 21:19:30 mellon Exp $	*/

/*
 * Copyright (c) 1990, 1993
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
__RCSID("$NetBSD: fsutil.c,v 1.4.2.1 1997/10/31 21:19:30 mellon Exp $");
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <errno.h>
#include <fstab.h>
#include <err.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "fsutil.h"

static const char *dev = NULL;
static int hot = 0;
static int preen = 0;

extern char *__progname;

static void vmsg __P((int, const char *, va_list));

void
setcdevname(cd, pr)
	const char *cd;
	int pr;
{
	dev = cd;
	preen = pr;
}

const char *
cdevname()
{
	return dev;
}

int
hotroot()
{
	return hot;
}

/*VARARGS*/
void
#if __STDC__
errexit(const char *fmt, ...)
#else
errexit(va_alist)
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	const char *fmt;

	va_start(ap);
	fmt = va_arg(ap, const char *);
#endif
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(8);
}

static void
vmsg(fatal, fmt, ap)
	int fatal;
	const char *fmt;
	va_list ap;
{
	if (!fatal && preen)
		(void) printf("%s: ", dev);

	(void) vprintf(fmt, ap);

	if (fatal && preen)
		(void) printf("\n");

	if (fatal && preen) {
		(void) printf(
		    "%s: UNEXPECTED INCONSISTENCY; RUN %s MANUALLY.\n",
		    dev, __progname);
		exit(8);
	}
}

/*VARARGS*/
void
#if __STDC__
pfatal(const char *fmt, ...)
#else
pfatal(va_alist)
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	const char *fmt;

	va_start(ap);
	fmt = va_arg(ap, const char *);
#endif
	vmsg(1, fmt, ap);
	va_end(ap);
}

/*VARARGS*/
void
#if __STDC__
pwarn(const char *fmt, ...)
#else
pwarn(va_alist)
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	const char *fmt;

	va_start(ap);
	fmt = va_arg(ap, const char *);
#endif
	vmsg(0, fmt, ap);
	va_end(ap);
}

void
perror(s)
	const char *s;
{
	pfatal("%s (%s)", s, strerror(errno));
}

void
#if __STDC__
panic(const char *fmt, ...)
#else
panic(va_alist)
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	const char *fmt;

	va_start(ap);
	fmt = va_arg(ap, const char *);
#endif
	vmsg(1, fmt, ap);
	va_end(ap);
	exit(8);
}

char *
unrawname(name)
	char *name;
{
	char *dp;
	struct stat stb;

	if ((dp = strrchr(name, '/')) == 0)
		return (name);
	if (stat(name, &stb) < 0)
		return (name);
	if (!S_ISCHR(stb.st_mode))
		return (name);
	if (dp[1] != 'r')
		return (name);
	(void)strcpy(&dp[1], &dp[2]);
	return (name);
}

char *
rawname(name)
	char *name;
{
	static char rawbuf[32];
	char *dp;

	if ((dp = strrchr(name, '/')) == 0)
		return (0);
	*dp = 0;
	(void)strcpy(rawbuf, name);
	*dp = '/';
	(void)strcat(rawbuf, "/r");
	(void)strcat(rawbuf, &dp[1]);
	return (rawbuf);
}

char *
blockcheck(origname)
	char *origname;
{
	struct stat stslash, stblock, stchar;
	char *newname, *raw;
	struct fstab *fsp;
	int retried = 0;

	hot = 0;
	if (stat("/", &stslash) < 0) {
		perror("/");
		printf("Can't stat root\n");
		return (origname);
	}
	newname = origname;
retry:
	if (stat(newname, &stblock) < 0) {
		perror(newname);
		printf("Can't stat %s\n", newname);
		return (origname);
	}
	if (S_ISBLK(stblock.st_mode)) {
		if (stslash.st_dev == stblock.st_rdev)
			hot++;
		raw = rawname(newname);
		if (stat(raw, &stchar) < 0) {
			perror(raw);
			printf("Can't stat %s\n", raw);
			return (origname);
		}
		if (S_ISCHR(stchar.st_mode)) {
			return (raw);
		} else {
			printf("%s is not a character device\n", raw);
			return (origname);
		}
	} else if (S_ISCHR(stblock.st_mode) && !retried) {
		newname = unrawname(newname);
		retried++;
		goto retry;
	} else if ((fsp = getfsfile(newname)) != 0 && !retried) {
		newname = fsp->fs_spec;
		retried++;
		goto retry;
	}
	/*
	 * Not a block or character device, just return name and
	 * let the user decide whether to use it.
	 */
	return (origname);
}


void *
emalloc(s)
	size_t s;
{
	void *p;

	p = malloc(s);
	if (p == NULL)
		err(1, "malloc failed");
	return (p);
}


void *
erealloc(p, s)
	void *p;
	size_t s;
{
	void *q;

	q = realloc(p, s);
	if (q == NULL)
		err(1, "realloc failed");
	return (q);
}


char *
estrdup(s)
	const char *s;
{
	char *p;

	p = strdup(s);
	if (p == NULL)
		err(1, "strdup failed");
	return (p);
}
