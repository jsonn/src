/*	$NetBSD: prom.c,v 1.5.74.1 2008/06/02 13:22:25 mjf Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wayne Knowles
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
__KERNEL_RCSID(0, "$NetBSD: prom.c,v 1.5.74.1 2008/06/02 13:22:25 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/cpu.h>
#include <machine/prom.h>

static struct mips_prom callvec;

struct mips_prom *callv;

typedef void (*funcp_t)(void);

void
prom_init(void)
{
	int i;
	funcp_t *fp;

	fp = (void *)&callvec;
	for (i=0; i < sizeof(struct mips_prom)/sizeof(funcp_t); i++ ) {
		fp[i] = (funcp_t)MIPS_PROM_ENTRY(i);
	}
	callv = &callvec;
}

/*
 * Read the environment variable 'console' to determine which serial
 * port will be used for the console.
 */
int
prom_getconsole(void)
{
	char *cp;

	cp = MIPS_PROM(getenv)("console");
	if (cp == NULL) {
		MIPS_PROM(printf)("WARNING: defaulting to serial port 1\n");
		return 1;
	}
	return  (*cp == '0') ? 0 : 1;
}
