/*	$NetBSD: process_machdep.c,v 1.6.2.3 2004/09/21 13:21:36 skrll Exp $	*/

/*
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * Copyright (c) 1995, 1996, 1997
 *	Charles M. Hannum.  All rights reserved.
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
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: process_machdep.c,v 1.6.2.3 2004/09/21 13:21:36 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>

#include <machine/psl.h>
#include <machine/reg.h>

static __inline struct trapframe *
process_frame(struct lwp *l)
{

	return (l->l_md.md_regs);
}

int
process_read_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

	regs->r_spc = tf->tf_spc;
	regs->r_ssr = tf->tf_ssr;
	regs->r_macl = tf->tf_macl;
	regs->r_mach = tf->tf_mach;
	regs->r_pr = tf->tf_pr;
	regs->r_r14 = tf->tf_r14;
	regs->r_r13 = tf->tf_r13;
	regs->r_r12 = tf->tf_r12;
	regs->r_r11 = tf->tf_r11;
	regs->r_r10 = tf->tf_r10;
	regs->r_r9 = tf->tf_r9;
	regs->r_r8 = tf->tf_r8;
	regs->r_r7 = tf->tf_r7;
	regs->r_r6 = tf->tf_r6;
	regs->r_r5 = tf->tf_r5;
	regs->r_r4 = tf->tf_r4;
	regs->r_r3 = tf->tf_r3;
	regs->r_r2 = tf->tf_r2;
	regs->r_r1 = tf->tf_r1;
	regs->r_r0 = tf->tf_r0;
	regs->r_r15 = tf->tf_r15;

	return (0);
}

int
process_write_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

	/*
	 * Check for security violations.
	 */
	if (((regs->r_ssr ^ tf->tf_ssr) & PSL_USERSTATIC) != 0) {
		return (EINVAL);
	}

	tf->tf_spc = regs->r_spc;
	tf->tf_ssr = regs->r_ssr;
	tf->tf_pr = regs->r_pr;

	tf->tf_mach = regs->r_mach;
	tf->tf_macl = regs->r_macl;
	tf->tf_r14 = regs->r_r14;
	tf->tf_r13 = regs->r_r13;
	tf->tf_r12 = regs->r_r12;
	tf->tf_r11 = regs->r_r11;
	tf->tf_r10 = regs->r_r10;
	tf->tf_r9 = regs->r_r9;
	tf->tf_r8 = regs->r_r8;
	tf->tf_r7 = regs->r_r7;
	tf->tf_r6 = regs->r_r6;
	tf->tf_r5 = regs->r_r5;
	tf->tf_r4 = regs->r_r4;
	tf->tf_r3 = regs->r_r3;
	tf->tf_r2 = regs->r_r2;
	tf->tf_r1 = regs->r_r1;
	tf->tf_r0 = regs->r_r0;
	tf->tf_r15 = regs->r_r15;

	return (0);
}

int
process_sstep(l, sstep)
	struct lwp *l;
{

	if (sstep)
		return (EINVAL);

	return (0);
}

int
process_set_pc(struct lwp *l, caddr_t addr)
{
	struct trapframe *tf = process_frame(l);

	tf->tf_spc = (int)addr;

	return (0);
}
