/*	$NetBSD: process_machdep.c,v 1.6.4.1 1999/11/15 00:39:34 fvdl Exp $ */

/*
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1993 Jan-Simon Pendry
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 * from: Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <sys/ptrace.h>

/* Unfortunately we need to convert v9 trapframe to v8 regs */
int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
#ifdef __arch64__
	struct trapframe64* tf = p->p_md.md_tf;
	int i;

	/* 
	 * Um, we should only do this conversion for 32-bit emulation
	 * or when running 32-bit mode.  We really need to pass in a
	 * 32-bit emulation flag!
	 */

	regs->r_tstate = tf->tf_tstate;
	regs->r_pc = tf->tf_pc;
	regs->r_npc = tf->tf_npc;
	regs->r_y = tf->tf_y;
	for (i = 0; i < 8; i++) {
		regs->r_global[i] = tf->tf_global[i];
		regs->r_out[i] = tf->tf_out[i];
	}
	/* We should also write out the ins and locals.  See signal stuff */
	return (0);
#else
	struct reg32* regp = (struct reg32*)regs;
	struct trapframe64* tf = p->p_md.md_tf;
	int i;

	/* 
	 * Um, we should only do this conversion for 32-bit emulation
	 * or when running 32-bit mode.  We really need to pass in a
	 * 32-bit emulation flag!
	 */

	regp->r_psr = TSTATECCR_TO_PSR(tf->tf_tstate);
	regp->r_pc = tf->tf_pc;
	regp->r_npc = tf->tf_npc;
	regp->r_y = tf->tf_y;
	for (i = 0; i < 8; i++) {
		regp->r_global[i] = tf->tf_global[i];
		regp->r_out[i] = tf->tf_out[i];
	}
	/* We should also write out the ins and locals.  See signal stuff */
	return (0);
#endif
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
#ifdef __arch64__
	struct trapframe* tf = p->p_md.md_tf;
	int i;

	tf->tf_pc = regs->r_pc;
	tf->tf_npc = regs->r_npc;
	tf->tf_y = regs->r_pc;
	for (i = 0; i < 8; i++) {
		tf->tf_global[i] = regs->r_global[i];
		tf->tf_out[i] = regs->r_out[i];
	}
	/* We should also read in the ins and locals.  See signal stuff */
	tf->tf_tstate = (tf->tf_tstate & ~TSTATE_CCR) | (regs->r_tstate & TSTATE_CCR);
	return (0);
#else
	struct reg32* regp = (struct reg32*)regs;
	struct trapframe64* tf = p->p_md.md_tf;
	int i;

	tf->tf_pc = regp->r_pc;
	tf->tf_npc = regp->r_npc;
	tf->tf_y = regp->r_pc;
	for (i = 0; i < 8; i++) {
		tf->tf_global[i] = regp->r_global[i];
		tf->tf_out[i] = regp->r_out[i];
	}
	/* We should also read in the ins and locals.  See signal stuff */
	tf->tf_tstate = (int64_t)(tf->tf_tstate & ~TSTATE_CCR) | PSRCC_TO_TSTATE(regp->r_psr);
	return (0);
#endif
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	if (sstep)
		return EINVAL;
	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	p->p_md.md_tf->tf_pc = (vaddr_t)addr;
	p->p_md.md_tf->tf_npc = (vaddr_t)addr + 4;
	return (0);
}

int
process_read_fpregs(p, regs)
struct proc	*p;
struct fpreg	*regs;
{
#ifdef __arch64__
	extern struct fpstate64	initfpstate;
	struct fpstate64	*statep = &initfpstate;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate)
		statep = p->p_md.md_fpstate;
	bcopy(statep, regs, sizeof(struct fpreg64));
	return 0;
#else
	extern struct fpstate64	initfpstate;
	struct fpstate64	*statep = &initfpstate;
	struct fpreg32		*regp = (struct fpreg32 *)regs;
	int i;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate)
		statep = p->p_md.md_fpstate;
	for (i=0; i<32; i++)
		regp->fr_regs[i] = statep->fs_regs[i];
	regp->fr_fsr = statep->fs_fsr;
	regp->fr_qsize = statep->fs_qsize;
	for (i=0; i<statep->fs_qsize; i++)
		regp->fr_queue[i] = statep->fs_queue[i];

	return 0;
#endif
}

int
process_write_fpregs(p, regs)
struct proc	*p;
struct fpreg	*regs;
{
#ifdef __arch64__
	if (p->p_md.md_fpstate == NULL)
		return EINVAL;

	bcopy(regs, p->p_md.md_fpstate, sizeof(struct fpreg64));
	return 0;
#else
	extern struct fpstate64	initfpstate;
	struct fpstate64	*statep = &initfpstate;
	struct fpreg32		*regp = (struct fpreg32 *)regs;
	int i;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate)
		statep = p->p_md.md_fpstate;
	for (i=0; i<32; i++)
		statep->fs_regs[i] = regp->fr_regs[i];
	statep->fs_fsr = regp->fr_fsr;
	statep->fs_qsize = regp->fr_qsize;
	for (i=0; i<regp->fr_qsize; i++)
		statep->fs_queue[i] = regp->fr_queue[i];

	return 0;
#endif
}
