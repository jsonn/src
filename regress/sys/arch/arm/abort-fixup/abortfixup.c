/* $NetBSD: abortfixup.c,v 1.7.18.1 2008/05/18 12:30:47 yamt Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 *
 * Trying out if an unhandled instruction pattern in the late abort fixup
 * generates a panic or sysfaults like it ought to do.
 */

#include <sys/types.h>

__RCSID("$NetBSD: abortfixup.c,v 1.7.18.1 2008/05/18 12:30:47 yamt Exp $");

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

jmp_buf buf;

void
sighandler(int sig)
{

	/* Catching SIGSEGV means the test passed. */
	longjmp(buf, 1);
}

int
main(void)
{

	if (signal(SIGSEGV, sighandler) == SIG_ERR)
		err(1, "signal");

	printf("ARM6/7 abort fixup panic\n");

	/*
 	 * issue an instruction that for certain generates a page
	 * fault but _can't_ be fixed up by late abort fixup
	 * routines due to its structure.
	 */

	if (setjmp(buf) == 0) {
		__asm volatile (
		"	mov r0, #0			\n"
		"	mov r1, r0			\n"
		"	str r1, [r0], r1, ror #10");
		
		/* Should not be reached if OK */
		printf("!!! Regression test FAILED - no SEGV recieved\n");
		exit(1);
	}

	printf("ARM2/3 abort address panic\n");

	/* Similar but pre-indexed, to check ARM2/3 abort address function. */

	if (setjmp(buf) == 0) {
		__asm volatile (
		"	mov r0, #0			\n"
		"	mov r1, r0			\n"
		"	str r1, [r0, r1, ror #10]");
		
		/* Should not be reached if OK */
		printf("!!! Regression test FAILED - no SEGV recieved\n");
		exit(1);
	}

	printf("All OK\n");

	exit(0);
};
