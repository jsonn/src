/*	$NetBSD: tcflow.c,v 1.2.4.1 1996/09/17 17:53:36 jtc Exp $	*/

/*-
 * Copyright (c) 1989, 1993
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
static char sccsid[] = "@(#)termios.c	8.2 (Berkeley) 2/21/94";
#else
static char rcsid[] = "$NetBSD: tcflow.c,v 1.2.4.1 1996/09/17 17:53:36 jtc Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#ifdef __weak_alias
__weak_alias(tcflow,_tcflow);
#endif

int
tcflow(fd, action)
	int fd, action;
{
	struct termios term;
	u_char c;

	switch (action) {
	case TCOOFF:
		return (ioctl(fd, TIOCSTOP, 0));
	case TCOON:
		return (ioctl(fd, TIOCSTART, 0));
	case TCION:
	case TCIOFF:
		if (tcgetattr(fd, &term) == -1)
			return (-1);
		c = term.c_cc[action == TCIOFF ? VSTOP : VSTART];
		if (c != _POSIX_VDISABLE && write(fd, &c, sizeof(c)) == -1)
			return (-1);
		return (0);
	default:
		errno = EINVAL;
		return (-1);
	}
	/* NOTREACHED */
}
