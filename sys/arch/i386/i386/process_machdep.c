/*	$NetBSD: process_machdep.c,v 1.32.2.2 2001/06/21 19:25:37 nathanw Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "opt_vm86.h"
#include "npx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/segments.h>

#ifdef VM86
#include <machine/vm86.h>
#endif

static __inline struct trapframe *process_frame (struct lwp *);
static __inline struct save87 *process_fpframe (struct lwp *);

static __inline struct trapframe *
process_frame(struct lwp *l)
{

	return (l->l_md.md_regs);
}

static __inline struct save87 *
process_fpframe(struct lwp *l)
{

	return (&l->l_addr->u_pcb.pcb_savefpu);
}

int
process_read_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		regs->r_gs = tf->tf_vm86_gs;
		regs->r_fs = tf->tf_vm86_fs;
		regs->r_es = tf->tf_vm86_es;
		regs->r_ds = tf->tf_vm86_ds;
		regs->r_eflags = get_vflags(l);
	} else
#endif
	{
		regs->r_gs = tf->tf_gs & 0xffff;
		regs->r_fs = tf->tf_fs & 0xffff;
		regs->r_es = tf->tf_es & 0xffff;
		regs->r_ds = tf->tf_ds & 0xffff;
		regs->r_eflags = tf->tf_eflags;
	}
	regs->r_edi = tf->tf_edi;
	regs->r_esi = tf->tf_esi;
	regs->r_ebp = tf->tf_ebp;
	regs->r_ebx = tf->tf_ebx;
	regs->r_edx = tf->tf_edx;
	regs->r_ecx = tf->tf_ecx;
	regs->r_eax = tf->tf_eax;
	regs->r_eip = tf->tf_eip;
	regs->r_cs = tf->tf_cs & 0xffff;
	regs->r_esp = tf->tf_esp;
	regs->r_ss = tf->tf_ss & 0xffff;

	return (0);
}

int
process_read_fpregs(struct lwp *l, struct fpreg *regs)
{
	struct save87 *frame = process_fpframe(l);

	if (l->l_md.md_flags & MDP_USEDFPU) {
#if NNPX > 0
		extern struct lwp *npxproc;

		if (npxproc == l)
			npxsave();
#endif
	} else {
		u_short cw;

		/*
		 * Fake a FNINIT.
		 * The initial control word was already set by setregs(), so
		 * save it temporarily.
		 */
		cw = frame->sv_env.en_cw;
		memset(frame, 0, sizeof(*regs));
		frame->sv_env.en_cw = cw;
		frame->sv_env.en_sw = 0x0000;
		frame->sv_env.en_tw = 0xffff;
		l->l_md.md_flags |= MDP_USEDFPU;
	}

	memcpy(regs, frame, sizeof(*regs));
	return (0);
}

int
process_write_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

#ifdef VM86
	if (regs->r_eflags & PSL_VM) {
		void syscall_vm86 __P((struct trapframe));

		tf->tf_vm86_gs = regs->r_gs;
		tf->tf_vm86_fs = regs->r_fs;
		tf->tf_vm86_es = regs->r_es;
		tf->tf_vm86_ds = regs->r_ds;
		set_vflags(l, regs->r_eflags);
		l->l_proc->p_md.md_syscall = syscall_vm86;
	} else
#endif
	{
		/*
		 * Check for security violations.
		 */
		if (((regs->r_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(regs->r_cs, regs->r_eflags))
			return (EINVAL);

		tf->tf_gs = regs->r_gs;
		tf->tf_fs = regs->r_fs;
		tf->tf_es = regs->r_es;
		tf->tf_ds = regs->r_ds;
#ifdef VM86
		if (tf->tf_eflags & PSL_VM)
			(*l->l_proc->p_emul->e_syscall_intern)(l->l_proc);
#endif
		tf->tf_eflags = regs->r_eflags;
	}
	tf->tf_edi = regs->r_edi;
	tf->tf_esi = regs->r_esi;
	tf->tf_ebp = regs->r_ebp;
	tf->tf_ebx = regs->r_ebx;
	tf->tf_edx = regs->r_edx;
	tf->tf_ecx = regs->r_ecx;
	tf->tf_eax = regs->r_eax;
	tf->tf_eip = regs->r_eip;
	tf->tf_cs = regs->r_cs;
	tf->tf_esp = regs->r_esp;
	tf->tf_ss = regs->r_ss;

	return (0);
}

int
process_write_fpregs(struct lwp *l, struct fpreg *regs)
{
	struct save87 *frame = process_fpframe(l);

	if (l->l_md.md_flags & MDP_USEDFPU) {
#if NNPX > 0
		extern struct lwp *npxproc;

		if (npxproc == l)
			npxdrop();
#endif
	} else {
		l->l_md.md_flags |= MDP_USEDFPU;
	}

	memcpy(frame, regs, sizeof(*regs));
	return (0);
}

int
process_sstep(struct lwp *l, int sstep)
{
	struct trapframe *tf = process_frame(l);

	if (sstep)
		tf->tf_eflags |= PSL_T;
	else
		tf->tf_eflags &= ~PSL_T;
	
	return (0);
}

int
process_set_pc(struct lwp *l, caddr_t addr)
{
	struct trapframe *tf = process_frame(l);

	tf->tf_eip = (int)addr;

	return (0);
}
