/*	$NetBSD: syscall.c,v 1.42.2.2 2008/10/14 20:25:42 wrstuden Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: syscall.c,v 1.42.2.2 2008/10/14 20:25:42 wrstuden Exp $");

#include "opt_sa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/ktrace.h>
#include <sys/syscall.h>
#include <sys/syscall_stats.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/userret.h>

void syscall_intern(struct proc *);
static void syscall(struct trapframe *);

void
child_return(void *arg)
{
	struct lwp *l = arg;
	struct trapframe *tf = l->l_md.md_regs;

	tf->tf_rax = 0;
	tf->tf_rflags &= ~PSL_C;

	userret(l);
	ktrsysret(SYS_fork, 0, 0);
}

void
syscall_intern(struct proc *p)
{

	p->p_md.md_syscall = syscall;
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
static void
syscall(struct trapframe *frame)
{
	const struct sysent *callp;
	struct proc *p;
	struct lwp *l;
	int error;
	register_t code, rval[2];
	#define args (&frame->tf_rdi)
	/* Verify that the syscall args will fit in the trapframe space */
	typedef char foo[offsetof(struct trapframe, tf_arg9)
		>= sizeof (register_t) * (2 + SYS_MAXSYSARGS - 1) ? 1 : -1];

	l = curlwp;
	p = l->l_proc;

	code = frame->tf_rax & (SYS_NSYSENT - 1);

	LWP_CACHE_CREDS(l, p);

	callp = p->p_emul->e_sysent + code;

	SYSCALL_COUNT(syscall_counts, code);
	SYSCALL_TIME_SYS_ENTRY(l, syscall_times, code);

#ifdef KERN_SA
	if (__predict_false((l->l_savp)
            && (l->l_savp->savp_pflags & SAVP_FLAG_DELIVERING)))
		l->l_savp->savp_pflags &= ~SAVP_FLAG_DELIVERING;
#endif
	/*
	 * The first 6 syscall args are passed in rdi, rsi, rdx, r10, r8 and r9
	 * (rcx gets copied to r10 in the libc stub because the syscall
	 * instruction overwrites %cx) and are together in the trap frame
	 * with space following for 4 more entries.
	 */
	if (__predict_false(callp->sy_argsize > 6 * 8)) {
		error = copyin((register_t *)frame->tf_rsp + 1,
		    &frame->tf_arg6, callp->sy_argsize - 6 * 8);
		if (error != 0)
			goto bad;
		/* Refetch to avoid register spill to stack */
		code = frame->tf_rax & (SYS_NSYSENT - 1);
	}

	if (!__predict_false(p->p_trace_enabled)
	    || __predict_false(callp->sy_flags & SYCALL_INDIRECT)
	    || (error = trace_enter(code, args, callp->sy_narg)) == 0) {
		rval[0] = 0;
		rval[1] = 0;
		error = (*callp->sy_call)(l, args, rval);
	}

	if (__predict_false(p->p_trace_enabled)
	    && !__predict_false(callp->sy_flags & SYCALL_INDIRECT)) {
		code = frame->tf_rax & (SYS_NSYSENT - 1);
		trace_exit(code, rval, error);
	}

	if (__predict_true(error == 0)) {
		frame->tf_rax = rval[0];
		frame->tf_rdx = rval[1];
		frame->tf_rflags &= ~PSL_C;	/* carry bit */
	} else {
		switch (error) {
		case ERESTART:
			/*
			 * The offset to adjust the PC by depends on whether we
			 * entered the kernel through the trap or call gate.
			 * We saved the instruction size in tf_err on entry.
			 */
			frame->tf_rip -= frame->tf_err;
			break;
		case EJUSTRETURN:
			/* nothing to do */
			break;
		default:
		bad:
			frame->tf_rax = error;
			frame->tf_rflags |= PSL_C;	/* carry bit */
			break;
		}
	}

	SYSCALL_TIME_SYS_EXIT(l);
	userret(l);
}
