/*	$NetBSD: makecontext.c,v 1.4.8.2 2008/04/28 20:22:58 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: makecontext.c,v 1.4.8.2 2008/04/28 20:22:58 martin Exp $");
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
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	int *sp;
	int i;
	va_list ap;

	/* LINTED uintptr_t is safe */
	sp  = (int *)
	    ((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	/* LINTED uintptr_t is safe */
	sp -= 2 + (argc > 8 ? argc - 8: 0); /* Make room for call frame. */
	sp  = (int *)
	    ((uintptr_t)sp & ~0xf);	/* Align on quad-word boundary. */

	/*
	 * Start executing at <func> -- when <func> completes, return to
	 * <_resumecontext>.
	 */
	gr[_REG_R1]  = (__greg_t)sp;
	gr[_REG_LR] = (__greg_t)_resumecontext;
	gr[_REG_PC] = (__greg_t)func;

	/* Wipe out stack frame backchain pointer. */
	*sp = 0;

	/* Construct argument list. */
	va_start(ap, argc);
	/* Up to the first eight arguments are passed in r3-10. */
	for (i = 0; i < argc && i < 8; i++)
		gr[_REG_R3 + i] = va_arg(ap, int);
	/* Pass remaining arguments on the stack above the backchain/lr gap. */
	for (sp += 2; i < argc; i++)
		*sp++ = va_arg(ap, int);
	va_end(ap);
}
