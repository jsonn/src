/*-
 * Copyright (c) 1993
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
 *
 *	$Id: strerror.c,v 1.3.2.2 1994/08/04 19:39:42 brezak Exp $
 */

#include "saerrno.h"


char *
strerror(err)
	int err;
{
	char *p;
	int length;
static	char ebuf[1024];

	switch (err) {
	case EADAPT:
		return "bad adaptor number";
	case ECTLR:
		return "bad controller number";
	case EUNIT:
		return "bad drive number";
	case EPART:
		return "bad partition";
	case ERDLAB:
		return "can't read disk label";
	case EUNLAB:
		return "unlabeled";
	case ENXIO:
		return "bad device specification";
	case EPERM:
		return "Permission Denied";
	case ENOENT:
		return "No such file or directory";
	case ESTALE:
		return "Stale NFS file handle";

	default:
		strcpy(ebuf, "Unknown error: code ");
		length = strlen(ebuf);
		p = ebuf+length;
		do {
			*p++ = "0123456789"[err % 10];
		} while (err /= 10);
		*p = '\0';
		return ebuf;
	}
}
