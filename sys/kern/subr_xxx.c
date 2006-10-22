/*	$NetBSD: subr_xxx.c,v 1.15.22.1 2006/10/22 06:07:11 yamt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *
 *	@(#)subr_xxx.c	8.3 (Berkeley) 3/29/95
 */

/*
 * Miscellaneous trivial functions, including many
 * that are often inline-expanded or done in assembler.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_xxx.c,v 1.15.22.1 2006/10/22 06:07:11 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>

/*
 * Unsupported device function (e.g. writing to read-only device).
 */
int
enodev(void)
{

	return (ENODEV);
}

/*
 * Unconfigured device function; driver not configured.
 */
int
enxio(void)
{

	return (ENXIO);
}

/*
 * Unsupported ioctl function.
 */
int
enoioctl(void)
{

	return (ENOTTY);
}

/*
 * Unsupported system function.
 * This is used for an otherwise-reasonable operation
 * that is not supported by the current system binary.
 */
int
enosys(void)
{

	return (ENOSYS);
}

/*
 * Return error for operation not supported
 * on a specific object or file type.
 */
int
eopnotsupp(void)
{

	return (EOPNOTSUPP);
}

/*
 * Generic null operation, always returns success.
 */
/*ARGSUSED*/
int
nullop(void *v __unused)
{

	return (0);
}
