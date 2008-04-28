/*	$NetBSD: unistd.h,v 1.4.10.2 2008/04/28 20:22:55 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
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
#ifndef _SSP_UNISTD_H_
#define _SSP_UNISTD_H_

#include <ssp/ssp.h>

#if __SSP_FORTIFY_LEVEL > 0
__BEGIN_DECLS

__ssp_redirect0(ssize_t, read, (int __fd, void *__buf, size_t __len), \
    (__fd, __buf, __len));

__ssp_redirect(int, readlink, (const char *__restrict __path, \
    char *__restrict __buf, size_t __len), (__path, __buf, __len));

__ssp_redirect(char *, getcwd, (char *__buf, size_t __len), (__buf, __len));

__END_DECLS

#define read(fd, buf, len)		__ssp_alias_func(read, (fd, buf, len))
#define readlink(path, buf, len)	__ssp_alias_func(readlink, (path, buf, len))
#define getcwd(buf, len)		__ssp_alias_func(getcwd, (buf, len))
#endif /* __SSP_FORTIFY_LEVEL > 0 */
#endif /* _SSP_UNISTD_H_ */
