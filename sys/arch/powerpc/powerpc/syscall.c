/*	$NetBSD: syscall.c,v 1.3.2.5 2002/08/06 23:00:09 nathanw Exp $	*/

/*
 * Copyright (C) 2002 Matt Thomas
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

#include "opt_altivec.h"
#include "opt_ktrace.h"
#include "opt_systrace.h"
#include "opt_compat_linux.h"
#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#ifdef SYSTRACE
#include <sys/systrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <powerpc/userret.h>
#include <machine/cpu.h>
#include <machine/frame.h>

#ifdef COMPAT_LINUX
#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_errno.h>
#include <compat/linux/linux_syscall.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_siginfo.h>
#include <compat/linux/arch/powerpc/linux_siginfo.h>
#include <compat/linux/arch/powerpc/linux_machdep.h>
#endif

#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((uintptr_t)(sp) + 8)) /* more args go here */

void syscall_plain(struct trapframe *frame);
void syscall_fancy(struct trapframe *frame);

void
syscall_plain(struct trapframe *frame)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	const struct sysent *callp;
	size_t argsize;
	register_t code;
	register_t *params, rval[2];
	register_t args[10];
	int error;
	int n;

	curcpu()->ci_ev_scalls.ev_count++;

	code = frame->fixreg[0];
	callp = p->p_emul->e_sysent;
	params = frame->fixreg + FIRSTARG;
	n = NARGREG;

	switch (code) {
	case SYS_syscall:
		/*
		 * code is first argument,
		 * followed by actual args.
		 */
		code = *params++;
		n -= 1;
		break;
	case SYS___syscall:
		params++;
		code = *params++;
		n -= 2;
		break;
	default:
		break;
	}

	code &= (SYS_NSYSENT - 1);
	callp += code;
	argsize = callp->sy_argsize;

	if (argsize > n * sizeof(register_t)) {
		memcpy(args, params, n * sizeof(register_t));
		KERNEL_PROC_LOCK(l);
		error = copyin(MOREARGS(frame->fixreg[1]),
		       args + n,
		       argsize - n * sizeof(register_t));
		KERNEL_PROC_UNLOCK(l);
		if (error)
			goto syscall_bad;
		params = args;
	}

	rval[0] = 0;
	rval[1] = 0;

	if ((callp->sy_flags & SYCALL_MPSAFE) == 0) {
		KERNEL_PROC_LOCK(l);
	}

	error = (*callp->sy_call)(l, params, rval);

	if ((callp->sy_flags & SYCALL_MPSAFE) == 0) {
		KERNEL_PROC_UNLOCK(l);
	}

	switch (error) {
	case 0:
		frame->fixreg[FIRSTARG] = rval[0];
		frame->fixreg[FIRSTARG + 1] = rval[1];
		frame->cr &= ~0x10000000;
		break;
	case ERESTART:
		/*
		 * Set user's pc back to redo the system call.
		 */
		frame->srr0 -= 4;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
syscall_bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame->fixreg[FIRSTARG] = error;
		frame->cr |= 0x10000000;
		break;
	}
	userret(l, frame);
}

void
syscall_fancy(struct trapframe *frame)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	const struct sysent *callp;
	size_t argsize;
	register_t code;
	register_t *params, rval[2];
	register_t args[10];
	int error;
	int n;

	KERNEL_PROC_LOCK(l);
	curcpu()->ci_ev_scalls.ev_count++;

	code = frame->fixreg[0];
	callp = p->p_emul->e_sysent;
	params = frame->fixreg + FIRSTARG;
	n = NARGREG;

	switch (code) {
	case SYS_syscall:
		/*
		 * code is first argument,
		 * followed by actual args.
		 */
		code = *params++;
		n -= 1;
		break;
	case SYS___syscall:
		params++;
		code = *params++;
		n -= 2;
		break;
	default:
		break;
	}

	code &= (SYS_NSYSENT - 1);
	callp += code;
	argsize = callp->sy_argsize;

	if (argsize > n * sizeof(register_t)) {
		memcpy(args, params, n * sizeof(register_t));
		error = copyin(MOREARGS(frame->fixreg[1]),
		       args + n,
		       argsize - n * sizeof(register_t));
		if (error)
			goto syscall_bad;
		params = args;
	}

	if ((error = trace_enter(l, code, params, rval)) != 0)
		goto syscall_bad;

	rval[0] = 0;
	rval[1] = 0;

	error = (*callp->sy_call)(l, params, rval);
	switch (error) {
	case 0:
		frame->fixreg[FIRSTARG] = rval[0];
		frame->fixreg[FIRSTARG + 1] = rval[1];
		frame->cr &= ~0x10000000;
		break;
	case ERESTART:
		/*
		 * Set user's pc back to redo the system call.
		 */
		frame->srr0 -= 4;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
syscall_bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame->fixreg[FIRSTARG] = error;
		frame->cr |= 0x10000000;
		break;
	}
	KERNEL_PROC_UNLOCK(l);
	trace_exit(l, code, params, rval, error);
	userret(l, frame);
}

void
syscall_intern(struct proc *p)
{
#ifdef KTRACE
	if (p->p_traceflag & (KTRFAC_SYSCALL | KTRFAC_SYSRET)) {
		p->p_md.md_syscall = syscall_fancy;
		return;
	}
#endif
#ifdef SYSTRACE
	if (ISSET(p->p_flag, P_SYSTRACE)) {
		p->p_md.md_syscall = syscall_fancy;
		return;
	} 
#endif
	p->p_md.md_syscall = syscall_plain;
}

#ifdef COMPAT_LINUX
void
linux_syscall_intern(struct proc *p)
{
	p->p_md.md_syscall = syscall_fancy;
}
#endif

void
child_return(void *arg)
{
	struct lwp * const l = arg;
	struct proc *p = l->l_proc;
	struct trapframe * const tf = trapframe(l);

	KERNEL_PROC_UNLOCK(l);

	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
	tf->srr1 &= ~(PSL_FP|PSL_VEC);	/* Disable FP & AltiVec, as we can't
					   be them. */
	l->l_addr->u_pcb.pcb_fpcpu = NULL;
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		KERNEL_PROC_LOCK(l);
		ktrsysret(p, SYS_fork, 0, 0);
		KERNEL_PROC_UNLOCK(l);
	}
#endif
	/* Profiling?							XXX */
	curcpu()->ci_schedstate.spc_curpriority = l->l_priority;
}
