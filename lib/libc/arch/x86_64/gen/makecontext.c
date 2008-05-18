/*	$NetBSD: makecontext.c,v 1.2.30.1 2008/05/18 12:30:13 yamt Exp $	*/

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
 *
 * Modified from the i386 version for x86_64 by fvdl@wasabisystems.com.
 *
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: makecontext.c,v 1.2.30.1 2008/05/18 12:30:13 yamt Exp $");
#endif

#include <inttypes.h>
#include <stddef.h>
#include <ucontext.h>
#include "extern.h"

#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	uintptr_t *sp;
	va_list ap;
	int stackargs, i;

	stackargs = argc - 6;

	/* LINTED __greg_t is safe */
	gr[_REG_RIP] = (__greg_t)func;

	/* LINTED uintptr_t is safe */
	sp  = (uintptr_t *)
	    ((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);

	/* LINTED uintptr_t is safe */
	sp  = (uintptr_t *)(((uintptr_t)sp & ~15));
	sp--;
	if (stackargs > 0)
		sp -= stackargs;
	/* LINTED __greg_t is safe */
	gr[_REG_URSP] = (__greg_t)sp;
	gr[_REG_RBP] = (__greg_t)0;	/* Wipe out frame pointer. */

	/* Put return address on top of stack. */
	/* LINTED uintptr_t is safe */
	*sp++ = (uintptr_t)_resumecontext;

	/*
	 * Construct argument list.
	 * The registers used to pass the first 6 arguments
	 * (rdi, rsi, rdx, rcx, r8, r9) are the first 6 in gregs,
	 * in that order, so those arguments can just be copied to
	 * the gregs array.
	 */
	va_start(ap, argc);
	for (i = 0; i < 6 && argc > 0; i++) {
		argc--;
		/* LINTED __greg_t is safe */
		gr[i] = va_arg(ap, __greg_t);
	}

	while (stackargs-- > 0) {
		/* LINTED uintptr_t is safe */
		*sp++ = va_arg(ap, uintptr_t);
	}

	va_end(ap);
}
