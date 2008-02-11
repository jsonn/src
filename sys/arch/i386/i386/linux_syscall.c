/*	$NetBSD: linux_syscall.c,v 1.31.2.5 2008/02/11 14:59:27 yamt Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: linux_syscall.c,v 1.31.2.5 2008/02/11 14:59:27 yamt Exp $");

#if defined(_KERNEL_OPT)
#include "opt_vm86.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/syscall.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/userret.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_errno.h>
#include <compat/linux/linux_syscall.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/arch/i386/linux_machdep.h>

static void linux_syscall(struct trapframe *);
extern struct sysent linux_sysent[];

void
linux_syscall_intern(struct proc *p)
{

	p->p_trace_enabled = trace_is_enabled(p);
	p->p_md.md_syscall = linux_syscall;
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
void
linux_syscall(struct trapframe *frame)
{
	register const struct sysent *callp;
	struct lwp *l;
	int error;
	register_t code, args[6], rval[2];

	uvmexp.syscalls++;
	l = curlwp;
	LWP_CACHE_CREDS(l, l->l_proc);

	code = frame->tf_eax & (LINUX_SYS_NSYSENT - 1);
	callp = linux_sysent;

	callp += code;
	/*
	 * Linux passes the args in ebx, ecx, edx, esi, edi, ebp, in
	 * increasing order.
	 */
	args[0] = frame->tf_ebx;
	args[1] = frame->tf_ecx;
	args[2] = frame->tf_edx;
	args[3] = frame->tf_esi;
	args[4] = frame->tf_edi;
	args[5] = frame->tf_ebp;

	rval[0] = 0;
	rval[1] = 0;

	KERNEL_LOCK(1, l);

	if (__predict_false(l->l_proc->p_trace_enabled)) {
		error = trace_enter(code, args, callp->sy_narg);
		if (__predict_true(error == 0)) {
			error = (*callp->sy_call)(l, args, rval);
			code = frame->tf_eax & (LINUX_SYS_NSYSENT - 1);
			trace_exit(code, rval, error);
		}
	} else
		error = (*callp->sy_call)(l, args, rval);

	KERNEL_UNLOCK_LAST(l);

	if (__predict_true(error == 0)) {
		frame->tf_eax = rval[0];
		/*
		 * XXX The linux libc code I (dsl) looked at doesn't use the
		 * carry bit.
		 * Values above 0xfffff000 are assumed to be errno values and
		 * not result codes!
		 */
		frame->tf_eflags &= ~PSL_C;	/* carry bit */
	} else {
		switch (error) {
		case ERESTART:
			/*
			 * The offset to adjust the PC by depends on whether
			 * we entered the kernel through the trap or call gate.
			 * We save the instruction size in tf_err on entry.
			 */
			frame->tf_eip -= frame->tf_err;
			break;
		case EJUSTRETURN:
			/* nothing to do */
			break;
		default:
			error = native_to_linux_errno[error];
			frame->tf_eax = error;
			frame->tf_eflags |= PSL_C;	/* carry bit */
			break;
		}
	}

	userret(l);
}
