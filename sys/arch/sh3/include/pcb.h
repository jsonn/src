/*	$NetBSD: pcb.h,v 1.2.16.1 2002/03/16 15:59:38 jdolecek Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pcb.h	5.10 (Berkeley) 5/12/91
 */

/*
 * SH3 process control block
 *     T.Horiuchi Brains Corp. 05/27/1998
 */

#ifndef _SH3_PCB_H_
#define _SH3_PCB_H_

#include <sys/signal.h>
#include <machine/psl.h>

struct pcb {
	int	r0;
	int	r1;
	int	r2;
	int	r3;
	int	r4;
	int	r5;
	int	r6;
	int	r7;
	int	r8;
	int	r9;
	int	r10;
	int	r11;
	int	r12;
	int	r13;
	int	r14;
	int	r15;
	int	sr;
	int	ssr;
	int	gbr;
	int	mach;
	int	macl;
	int	pr;
	int	vbr;
	int	pc;
	int	spc;
	int	pcb_tss_sel;
/*
 * Software pcb (extension)
 */
	int	kr15;		/* stack pointer in kernel mode */
	int	pageDirReg;	/* Page Directory of this process */
	int	pcb_flags;
	caddr_t	pcb_onfault;	/* copyin/out fault recovery */
	int	fusubail;
	struct pmap *pcb_pmap;	/* back pointer to our pmap */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the SH3, there is nothing to add.
 */
struct md_coredump {
	long	md_pad[8];
};

#ifdef _KERNEL
struct pcb *curpcb;		/* our current running pcb */
#endif

#endif /* _SH3_PCB_H_ */
