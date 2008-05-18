/*	$NetBSD: sigstackalign.c,v 1.2.32.1 2008/05/18 12:30:47 yamt Exp $       */

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
 */

#include <sys/types.h>

__RCSID("$NetBSD: sigstackalign.c,v 1.2.32.1 2008/05/18 12:30:47 yamt Exp $");

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define RANGE 16
#define STACKALIGN 8
#define BLOCKSIZE (MINSIGSTKSZ + RANGE)

extern void getstackptr(int);

void *stackptr;

int
main(int argc, char **argv)
{
	char *stackblock;
	int i, ret;
	struct sigaction sa;
	stack_t ss;

	ret = 0;

	sa.sa_handler = getstackptr;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if (sigaction(SIGUSR1, &sa, NULL) != 0)
		err(1, "sigaction");

	stackblock = malloc(BLOCKSIZE);
	for (i = 0; i < RANGE; i++) {
		ss.ss_sp = stackblock;
		ss.ss_size = MINSIGSTKSZ + i;
		ss.ss_flags = 0;
		if (sigaltstack(&ss, NULL) != 0)
			err(1, "sigaltstack");
		kill(getpid(), SIGUSR1);
		if ((u_int)stackptr % STACKALIGN != 0) {
			fprintf(stderr, "Bad stack pointer %p\n", stackptr);
			ret = 1;
		}
#if 0
		printf("i = %d, stackptr = %p\n", i, stackptr);
#endif
	}

	return ret;
}
		
