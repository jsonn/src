/*	$NetBSD: mach_machdep.h,v 1.4.76.1 2008/05/18 12:32:36 yamt Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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

#ifndef	_MACH_MACHDEP_H_
#define	_MACH_MACHDEP_H_

/* process and thread state */
#define MACH_PPC_THREAD_STATE		1
#define MACH_PPC_FLOAT_STATE		2
#define MACH_PPC_EXCEPTION_STATE	3
#define MACH_PPC_VECTOR_STATE		4
#define MACH_THREAD_STATE_NONE		7

struct mach_ppc_exception_state {
	unsigned long dar;
	unsigned long dsisr;
	unsigned long exception;
	unsigned long pad[5];
};

struct mach_ppc_thread_state {
	unsigned int srr0;
	unsigned int srr1;
	unsigned int gpreg[32];
	unsigned int cr;
	unsigned int xer;
	unsigned int lr;
	unsigned int ctr;
	unsigned int mq;
	unsigned int vrsave;
};

struct mach_ppc_float_state {
	double  fpregs[32];
	unsigned int fpscr_pad;
	unsigned int fpscr;
};

struct mach_ppc_vector_state {
	unsigned long vr[32][4];
	unsigned long vscr[4];
	unsigned int pad1[4];
	unsigned int vrvalid;
	unsigned int pad2[7];
};

#endif /* _MACH_MACHDEP_H_ */
