/*	$NetBSD: atexit.c,v 1.12.10.1 2001/08/08 16:27:44 nathanw Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)atexit.c	8.2 (Berkeley) 7/3/94";
#else
__RCSID("$NetBSD: atexit.c,v 1.12.10.1 2001/08/08 16:27:44 nathanw Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "reentrant.h"
#include "atexit.h"

struct atexit *__atexit;	/* points to head of LIFO stack */

#ifdef _REENTRANT
mutex_t __atexit_mutex = MUTEX_INITIALIZER;
#endif

/*
 * Register a function to be performed at exit.
 */
int
atexit(fn)
	void (*fn) __P((void));
{
	static struct atexit __atexit0;	/* one guaranteed table */
	struct atexit *p;

	_DIAGASSERT(fn != NULL);

	mutex_lock(&__atexit_mutex);
	if ((p = __atexit) == NULL)
		__atexit = p = &__atexit0;
	else if (p->ind >= ATEXIT_SIZE) {
		if ((p = malloc(sizeof(*p))) == NULL) {
			mutex_unlock(&__atexit_mutex);
			return (-1);
		}
		p->ind = 0;
		p->next = __atexit;
		__atexit = p;
	}
	p->fns[p->ind++] = fn;
	mutex_unlock(&__atexit_mutex);
	return (0);
}
