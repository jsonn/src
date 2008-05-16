/*	$NetBSD: fpu.c,v 1.37.20.1 2008/05/16 02:22:44 yamt Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
__KERNEL_RCSID(0, "$NetBSD: fpu.c,v 1.37.20.1 2008/05/16 02:22:44 yamt Exp $");

#include "opt_fpu_emulate.h"

/*
 * Floating Point Unit (MC68881/882/040)
 * Probe for the FPU at autoconfig time.
 */

#include <sys/param.h>

#include <machine/cpu.h>
#include <machine/frame.h>

/*
 * FPU type; emulator uses FPU_NONE
 */

extern label_t *nofault;

static int  fpu_probe(void);

static const char *fpu_descr[] = {
#ifdef	FPU_EMULATE
	"emulator", 		/* 0 */
#else
	"no math support",	/* 0 */
#endif
	"mc68881",			/* 1 */
	"mc68882",			/* 2 */
	"mc68040",			/* 3 */
	"mc68060",			/* 4 */
	"unknown" };

void
initfpu(void)
{
	const char *descr;

	fputype = fpu_probe();

	/* Generate a reference FPU idle frame. */
	if (fputype != FPU_NONE)
		m68k_make_fpu_idle_frame();

	if ((0 <= fputype) && (fputype <= 3))
		descr = fpu_descr[fputype];
	else
		descr = "unknown type";

	printf("fpu: %s\n", descr);
}

static int
fpu_probe(void)
{
	/*
	 * A 68881 idle frame is 28 bytes and a 68882's is 60 bytes.
	 * We, of course, need to have enough room for either.
	 */
	int fpframe[60 / sizeof(int)];
	label_t	faultbuf;
	u_char b;

	nofault = &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = (label_t *)0;
		return (FPU_NONE);
	}

	/*
	 * Synchronize FPU or cause a fault.
	 * This should leave the 881/882 in the IDLE state,
	 * state, so we can determine which we have by
	 * examining the size of the FP state frame
	 */
	__asm("fnop");

	nofault = 0;

	/*
	 * Presumably, if we're an 040 and did not take exception
	 * above, we have an FPU.  Don't bother probing.
	 */
	if (mmutype == MMU_68040)
		return (FPU_68040);

	/*
	 * Presumably, this will not cause a fault--the fnop should
	 * have if this will.  We save the state in order to get the
	 * size of the frame.
	 */
	__asm("movl %0,%%a0; fsave %%a0@" : : "a" (fpframe) : "a0");

	b = *((u_char *)fpframe + 1);

	/*
	 * Now, restore a NULL state to reset the FPU.
	 */
	fpframe[0] = fpframe[1] = 0;
	m68881_restore((struct fpframe *)fpframe);

	/*
	 * The size of a 68881 IDLE frame is 0x18
	 *         and a 68882 frame is 0x38
	 */
	if (b == 0x18)
		return (FPU_68881);
	if (b == 0x38)
		return (FPU_68882);

	/*
	 * If it's not one of the above, we have no clue what it is.
	 */
	return (FPU_UNKNOWN);
}
