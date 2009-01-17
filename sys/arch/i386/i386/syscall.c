/*	$NetBSD: syscall.c,v 1.50.6.3 2009/01/17 13:28:04 mjf Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: syscall.c,v 1.50.6.3 2009/01/17 13:28:04 mjf Exp $");

#include "opt_vm86.h"
#include "opt_sa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/ktrace.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/syscall_stats.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/userret.h>

void syscall(struct trapframe *);
int x86_copyargs(void *, void *, size_t);
#ifdef VM86
void syscall_vm86(struct trapframe *);
#endif

void
syscall_intern(p)
	struct proc *p;
{

	p->p_md.md_syscall = syscall;
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
void
syscall(struct trapframe *frame)
{
	const struct sysent *callp;
	struct lwp *l;
	int error;
	register_t code, args[2 + SYS_MAXSYSARGS], rval[2];

	l = curlwp;
	LWP_CACHE_CREDS(l, l->l_proc);

	code = frame->tf_eax & (SYS_NSYSENT - 1);
	callp = l->l_proc->p_emul->e_sysent + code;

	SYSCALL_COUNT(syscall_counts, code);
	SYSCALL_TIME_SYS_ENTRY(l, syscall_times, code);

#ifdef KERN_SA
	if (__predict_false((l->l_savp)
            && (l->l_savp->savp_pflags & SAVP_FLAG_DELIVERING)))
		l->l_savp->savp_pflags &= ~SAVP_FLAG_DELIVERING;
#endif

	if (callp->sy_argsize) {
		error = x86_copyargs((char *)frame->tf_esp + sizeof(int), args,
			    callp->sy_argsize);
		if (__predict_false(error != 0))
			goto bad;
	}

	if (!__predict_false(l->l_proc->p_trace_enabled)
	    || __predict_false(callp->sy_flags & SYCALL_INDIRECT)
	    || (error = trace_enter(frame->tf_eax & (SYS_NSYSENT - 1),
		    args, callp->sy_narg)) == 0) {
		rval[0] = 0;
		rval[1] = 0;
		KASSERT(l->l_holdcnt == 0);
		error = sy_call(callp, l, args, rval);
	}

	if (__predict_false(l->l_proc->p_trace_enabled)
	    && !__predict_false(callp->sy_flags & SYCALL_INDIRECT)) {
		code = frame->tf_eax & (SYS_NSYSENT - 1);
		trace_exit(code, rval, error);
	}

	if (__predict_true(error == 0)) {
		frame->tf_eax = rval[0];
		frame->tf_edx = rval[1];
		frame->tf_eflags &= ~PSL_C;	/* carry bit */
	} else {
		switch (error) {
		case ERESTART:
			/*
			 * The offset to adjust the PC by depends on whether we
			 * entered the kernel through the trap or call gate.
			 * We saved the instruction size in tf_err on entry.
			 */
			frame->tf_eip -= frame->tf_err;
			break;
		case EJUSTRETURN:
			/* nothing to do */
			break;
		default:
		bad:
			frame->tf_eax = error;
			frame->tf_eflags |= PSL_C;	/* carry bit */
			break;
		}
	}

	SYSCALL_TIME_SYS_EXIT(l);
	userret(l);
}

#ifdef VM86
void
syscall_vm86(frame)
	struct trapframe *frame;
{
	struct lwp *l;
	struct proc *p;
	ksiginfo_t ksi;

	KSI_INIT_TRAP(&ksi);
	ksi.ksi_signo = SIGBUS;
	ksi.ksi_code = BUS_OBJERR;
	ksi.ksi_trap = T_PROTFLT;
	ksi.ksi_addr = (void *)frame->tf_eip;

	l = curlwp;
	p = l->l_proc;

#ifdef KERN_SA
	/* While this is probably not needed, it's probably better to include than not */
	if (__predict_false((l->l_savp)
            && (l->l_savp->savp_pflags & SAVP_FLAG_DELIVERING)))
		l->l_savp->savp_pflags &= ~SAVP_FLAG_DELIVERING;
#endif

	(*p->p_emul->e_trapsignal)(l, &ksi);
	userret(l);
}
#endif

void
child_return(arg)
	void *arg;
{
	struct lwp *l = arg;
	struct trapframe *tf = l->l_md.md_regs;

	tf->tf_eax = 0;
	tf->tf_eflags &= ~PSL_C;

	userret(l);
	ktrsysret(SYS_fork, 0, 0);
}
