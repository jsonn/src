/*	$NetBSD: compat_utimes.c,v 1.1.2.1 2008/11/08 21:49:36 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_utimes.c,v 1.1.2.1 2008/11/08 21:49:36 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <compat/sys/time.h>

__warn_references(utimes,
    "warning: reference to compatibility utimes(); include <sys/time.h> to generate correct reference")
__warn_references(lutimes,
    "warning: reference to compatibility lutimes(); include <sys/time.h> to generate correct reference")
__warn_references(futimes,
    "warning: reference to compatibility futimes(); include <sys/time.h> to generate correct reference")

/*
 * libc12 compatible f,l,utimes routine.
 */
int
utimes(const char *path, const struct timeval50 *tv50)
{
	struct timeval tv[2];

	timeval50_to_timeval(&tv50[0], &tv[0]);
	timeval50_to_timeval(&tv50[1], &tv[1]);
	return __utimes50(path, tv);
}

int
lutimes(const char *path, const struct timeval50 *tv50)
{
	struct timeval tv[2];

	timeval50_to_timeval(&tv50[0], &tv[0]);
	timeval50_to_timeval(&tv50[1], &tv[1]);
	return __lutimes50(path, tv);
}

int
futimes(int fd, const struct timeval50 *tv50)
{
	struct timeval tv[2];

	timeval50_to_timeval(&tv50[0], &tv[0]);
	timeval50_to_timeval(&tv50[1], &tv[1]);
	return __futimes50(fd, tv);
}
