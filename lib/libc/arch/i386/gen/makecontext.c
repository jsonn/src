/*	$NetBSD: makecontext.c,v 1.1.2.1 2001/03/05 23:34:37 nathanw Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__RCSID("$NetBSD: makecontext.c,v 1.1.2.1 2001/03/05 23:34:37 nathanw Exp $");
#endif

#include <inttypes.h>
#include <stddef.h>
#include <ucontext.h>
#include "extern.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


void
#if __STDC__
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
#else
makecontext(ucp, func, argc, va_alist)
	ucontext_t *ucp;
	void (*func)();
	int argc;
	va_dcl
#endif
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	unsigned int *sp;
	va_list ap;

	gr[_REG_EIP] = (__greg_t)func;

	/* LINTED uintptr_t is safe */
	sp  = (int *)((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	/* LINTED uintptr_t is safe */
	sp  = (int *)((uintptr_t)sp & ~0x3);	/* Align on word boundary. */
	sp -= argc + 1;			/* Make room for ret and args. */
	gr[_REG_UESP] = (__greg_t)sp;
	gr[_REG_EBP] = (__greg_t)0;	/* Wipe out frame pointer. */

	/* Put return address on top of stack. */
	/* LINTED uintptr_t is safe */
	*sp++ = (uintptr_t)_resumecontext;

	/* Construct argument list. */
#if __STDC__
	va_start(ap, argc);
#else
	va_start(ap);
#endif
	while (argc-- > 0) {
		/* LINTED uintptr_t is safe */
		*sp++ = va_arg(ap, uintptr_t);
	}
	va_end(ap);
}
