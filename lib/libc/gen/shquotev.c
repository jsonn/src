/* $NetBSD: shquotev.c,v 1.2.2.2 2001/10/08 20:19:25 nathanw Exp $ */

/*
 * Copyright (c) 2001 Christopher G. Demetriou
 * All rights reserved.
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.netbsd.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include <stdlib.h>

/*
 * shquotev():
 *
 * Apply shquote() to a set of strings, separating the results by spaces.
 */

size_t
shquotev(int argc, char * const * argv, char *buf, size_t bufsize)
{
	size_t rv, callrv;
	int i;

	rv = 0;

	if (argc == 0) {
		if (bufsize != 0)
			*buf = '\0';
		return rv;
	}

	for (i = 0; i < argc; i++) {
		callrv = shquote(argv[i], buf, bufsize);
		if (callrv == (size_t)-1)
			goto bad;
		rv += callrv;
		buf += callrv;
		bufsize = (bufsize > callrv) ? (bufsize - callrv) : 0;

		if (i < (argc - 1)) {
			rv++;
			if (bufsize > 1) {
				*buf++ = ' ';
				bufsize--;
			}
		}
	}

	return rv;

bad:
	return (size_t)-1;
}
