/*	$NetBSD: makekey.c,v 1.7.32.1 2008/09/18 04:39:18 wrstuden Exp $	*/

/*-
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
__COPYRIGHT("@(#) Copyright (c) 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "from: @(#)makekey.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: makekey.c,v 1.7.32.1 2008/09/18 04:39:18 wrstuden Exp $");
#endif
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void get(char *, int);
int main(void);

int
main(void)
{
	int len;
	char *r, key[9], salt[3];
	
	get(key, sizeof(key) - 1);
	get(salt, sizeof(salt) - 1);
	len = strlen(r = crypt(key, salt));
	if (write(STDOUT_FILENO, r, len) != len)
		err(1, "stdout");
	exit(0);
}

static void
get(char *bp, int len)
{
	int nr;

	bp[len] = '\0';
	if ((nr = read(STDIN_FILENO, bp, len)) == len)
		return;
	if (nr >= 0)
		errno = EFTYPE;
	err(1, "stdin");
}
