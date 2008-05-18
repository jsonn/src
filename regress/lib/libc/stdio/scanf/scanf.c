/* $NetBSD: scanf.c,v 1.2.30.1 2008/05/18 12:30:46 yamt Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>

#define	NUM	-0x1234
#define	STRNUM	___STRING(NUM)

int
main(int argc, char **argv)
{
	int i;

	/*
	 * NetBSD PR lib/21691:
	 * sscanf(3) %i format does not work properly with -ve hex numbers
	 */
	sscanf(STRNUM, "%i", &i);
	if (i != NUM)
		errx(1, "want %d, got %d", NUM, i);
	assert(i == NUM);

	/*
	 * "%x" has the same problem as "%i" in lib/21691.
	 */
	sscanf(STRNUM, "%x", &i);
	if (i != NUM)
		errx(1, "want %d, got %d", NUM, i);
	assert(i == NUM);


	return (0);
}
