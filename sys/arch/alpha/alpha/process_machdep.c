/* $NetBSD: process_machdep.c,v 1.7.2.1 1997/06/01 04:11:36 cgd Exp $ */

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <machine/options.h>		/* Config options headers */
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: process_machdep.c,v 1.7.2.1 1997/06/01 04:11:36 cgd Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>

#define	process_frame(p)	((p)->p_md.md_tf)
#define	process_pcb(p)		(&(p)->p_addr->u_pcb)
#define	process_fpframe(p)	(&(process_pcb(p)->pcb_fp))

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{

	frametoreg(process_frame(p), regs);
	regs->r_regs[R_ZERO] = process_frame(p)->tf_regs[FRAME_PC];
	regs->r_regs[R_SP] = process_pcb(p)->pcb_hw.apcb_usp;
	return (0);
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{

	regtoframe(regs, process_frame(p));
	process_frame(p)->tf_regs[FRAME_PC] = regs->r_regs[R_ZERO];
	process_pcb(p)->pcb_hw.apcb_usp = regs->r_regs[R_SP];
	return (0);
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{

	if (sstep)
		return (EINVAL);

	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	struct trapframe *frame = process_frame(p);

	frame->tf_regs[FRAME_PC] = (u_int64_t)addr;
	return (0);
}

int
process_read_fpregs(p, regs)
	struct proc *p;
	struct fpreg *regs;
{
	extern struct proc *fpcurproc;

	if (p == fpcurproc) {
		alpha_pal_wrfen(1);
		savefpstate(process_fpframe(p));
		alpha_pal_wrfen(0);
	}

	bcopy(process_fpframe(p), regs, sizeof(struct fpreg));
	return (0);
}

int
process_write_fpregs(p, regs)
	struct proc *p;
	struct fpreg *regs;
{
	extern struct proc *fpcurproc;

	if (p == fpcurproc)
		fpcurproc = NULL;

	bcopy(regs, process_fpframe(p), sizeof(struct fpreg));
	return (0);
}
