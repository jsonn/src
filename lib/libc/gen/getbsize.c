/*	$NetBSD: getbsize.c,v 1.8.2.1 1996/09/19 20:02:37 jtc Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getbsize.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] = "$NetBSD: getbsize.c,v 1.8.2.1 1996/09/19 20:02:37 jtc Exp $";
#endif
#endif /* not lint */

#include "namespace.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(getbsize,_getbsize);
#endif

char *
getbsize(headerlenp, blocksizep)
	int *headerlenp;
	long *blocksizep;
{
	static char header[20];
	long n, max, mul, blocksize;
	char *ep, *p, *form;

#define	KB	(1024L)
#define	MB	(1024L * 1024L)
#define	GB	(1024L * 1024L * 1024L)
#define	MAXB	GB		/* No tera, peta, nor exa. */
	form = "";
	if ((p = getenv("BLOCKSIZE")) != NULL && *p != '\0') {
		if ((n = strtol(p, &ep, 10)) < 0)
			goto underflow;
		if (n == 0)
			n = 1;
		if (*ep && ep[1])
			goto fmterr;
		switch (*ep) {
		case 'G': case 'g':
			form = "G";
			max = MAXB / GB;
			mul = GB;
			break;
		case 'K': case 'k':
			form = "K";
			max = MAXB / KB;
			mul = KB;
			break;
		case 'M': case 'm':
			form = "M";
			max = MAXB / MB;
			mul = MB;
			break;
		case '\0':
			max = MAXB;
			mul = 1;
			break;
		default:
fmterr:			_warnx("%s: unknown blocksize", p);
			n = 512;
			mul = 1;
			break;
		}
		if (n > max) {
			_warnx("maximum blocksize is %dG", MAXB / GB);
			n = max;
		}
		if ((blocksize = n * mul) < 512) {
underflow:		_warnx("%s: minimum blocksize is 512", p);
			form = "";
			blocksize = n = 512;
		}
	} else
		blocksize = n = 512;

	*headerlenp = snprintf(header, sizeof(header), "%d%s-blocks", n, form);
	*blocksizep = blocksize;
	return (header);
}
