/*	$NetBSD: pcb.h,v 1.8.8.4 2002/11/11 22:02:53 nathanw Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_POWERPC_PCB_H_
#define	_POWERPC_PCB_H_

#include <powerpc/reg.h>

typedef int faultbuf[23];
struct fpu {
	double fpr[32];
	double fpscr;	/* FPSCR stored as double for easier access */
};

struct pcb {
	struct pmap *pcb_pm;	/* pmap of our vmspace */
	struct pmap *pcb_pmreal; /* real address of above */
	register_t pcb_sp;	/* saved SP */
	int pcb_spl;		/* saved SPL */
	struct cpu_info * __volatile pcb_fpcpu; /* CPU with our FP state */
	struct cpu_info * __volatile pcb_veccpu;/* CPU with our VECTOR state */
	faultbuf *pcb_onfault;	/* For use during copyin/copyout */
	int pcb_flags;
#define	PCB_FPU		1	/* Process had FPU initialized */
#define PCB_ALTIVEC	2	/* Process had AltiVec initialized */
	struct fpu pcb_fpu;	/* Floating point processor */
	struct vreg *pcb_vr;
};

struct md_coredump {
	struct trapframe frame;
	struct fpu fpstate;
	struct vreg vstate;
};

#if defined(_KERNEL) && !defined(MULTIPROCESSOR)
extern struct pcb *curpcb;
extern struct pmap *curpm;
#endif

#endif	/* _POWERPC_PCB_H_ */
