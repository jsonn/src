/*	$NetBSD: opendisk.c,v 1.9.32.1 2008/05/18 12:30:43 yamt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
__RCSID("$NetBSD: opendisk.c,v 1.9.32.1 2008/05/18 12:30:43 yamt Exp $");
#endif

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <util.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>

int
opendisk(const char *path, int flags, char *buf, size_t buflen, int iscooked)
{
	int f, rawpart;

	if (buf == NULL) {
		errno = EFAULT;
		return (-1);
	}
	snprintf(buf, buflen, "%s", path);

	if ((flags & O_CREAT) != 0) {
		errno = EINVAL;
		return (-1);
	}

	rawpart = getrawpartition();
	if (rawpart < 0)
		return (-1);	/* sysctl(3) in getrawpartition sets errno */

	f = open(buf, flags);
	if (f != -1 || errno != ENOENT)
		return (f);

	snprintf(buf, buflen, "%s%c", path, 'a' + rawpart);
	f = open(buf, flags);
	if (f != -1 || errno != ENOENT)
		return (f);

	if (strchr(path, '/') != NULL)
		return (-1);

	snprintf(buf, buflen, "%s%s%s", _PATH_DEV, iscooked ? "" : "r", path);
	f = open(buf, flags);
	if (f != -1 || errno != ENOENT)
		return (f);

	snprintf(buf, buflen, "%s%s%s%c", _PATH_DEV, iscooked ? "" : "r", path,
	    'a' + rawpart);
	f = open(buf, flags);
	return (f);
}
