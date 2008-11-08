/*	$NetBSD: compat_utime.c,v 1.1.2.1 2008/11/08 21:49:34 christos Exp $	*/

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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)utime.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: compat_utime.c,v 1.1.2.1 2008/11/08 21:49:34 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */


#define __LIBC12_SOURCE__

#include "namespace.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <utime.h>
#include <compat/include/utime.h>
#include <sys/time.h>
#include <compat/sys/time.h>

__warn_references(utime,
    "warning: reference to compatibility utime(); include <utime.h> to generate correct reference")

#ifdef __weak_alias
__weak_alias(utime, _utime)
#endif

int
utime(const char *path, const struct utimbuf50 *times50)
{
	struct timeval50 tv50[2], *tvp50;

	_DIAGASSERT(path != NULL);

	if (times50 == NULL)
		tvp50 = NULL;
	else {
		tv50[0].tv_sec = times50->actime;
		tv50[1].tv_sec = times50->modtime;
		tv50[0].tv_usec = tv50[1].tv_usec = 0;
		tvp50 = tv50;
	}
	return utimes(path, tvp50);
}
