/*	$NetBSD: kern_sig_43.c,v 1.7.14.1 1997/09/08 23:17:37 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/param.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/buf.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/core.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <machine/cpu.h>

#include <vm/vm.h>
#include <sys/user.h>		/* for coredump */

int
compat_43_sys_sigblock(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_sigblock_args /* {
		syscallarg(int) mask;
	} */ *uap = v;

	(void) splhigh();
	*retval = p->p_sigmask;
	p->p_sigmask |= SCARG(uap, mask) &~ sigcantmask;
	(void) spl0();
	return (0);
}


int
compat_43_sys_sigsetmask(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_sigsetmask_args /* {
		syscallarg(int) mask;
	} */ *uap = v;

	(void) splhigh();
	*retval = p->p_sigmask;
	p->p_sigmask = SCARG(uap, mask) &~ sigcantmask;
	(void) spl0();
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_sigstack(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_sigstack_args /* {
		syscallarg(struct sigstack *) nss;
		syscallarg(struct sigstack *) oss;
	} */ *uap = v;
	struct sigstack ss;
	struct sigacts *psp;
	int error = 0;

	psp = p->p_sigacts;
	ss.ss_sp = p->p_sigstk.ss_sp;
	ss.ss_onstack = p->p_sigstk.ss_flags & SS_ONSTACK;
	if (SCARG(uap, oss) && (error = copyout((caddr_t)&ss,
	    (caddr_t)SCARG(uap, oss), sizeof (struct sigstack))))
		return (error);
	if (SCARG(uap, nss) == 0)
		return (0);
	error = copyin((caddr_t)SCARG(uap, nss), (caddr_t)&ss,
	    sizeof (ss));
	if (error)
		return (error);
	psp->ps_flags |= SAS_ALTSTACK;
	p->p_sigstk.ss_sp = ss.ss_sp;
	p->p_sigstk.ss_size = 0;
	p->p_sigstk.ss_flags |= ss.ss_onstack & SS_ONSTACK;
	return (0);
}

/*
 * Generalized interface signal handler, 4.3-compatible.
 */
/* ARGSUSED */
int
compat_43_sys_sigvec(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_sigvec_args /* {
		syscallarg(int) signum;
		syscallarg(struct sigvec *) nsv;
		syscallarg(struct sigvec *) osv;
	} */ *uap = v;
	struct sigvec vec;
	register struct sigacts *ps = p->p_sigacts;
	register struct sigaction *sa;
	register struct sigvec *sv;
	register int signum;
	int error;

	signum = SCARG(uap, signum);
	if (signum <= 0 || signum >= NSIG ||
	    signum == SIGKILL || signum == SIGSTOP)
		return (EINVAL);
	sa = &ps->ps_sigact[signum];
	sv = &vec;
	if (SCARG(uap, osv)) {
		*(sig_t *)&sv->sv_handler = sa->sa_handler;
		sv->sv_mask = sa->sa_mask & ~sigmask(signum);
		sv->sv_flags = sa->sa_flags ^ SA_RESTART;
		error = copyout((caddr_t)sv, (caddr_t)SCARG(uap, osv),
		    sizeof (vec));
		if (error)
			return (error);
	}
	if (SCARG(uap, nsv)) {
		error = copyin((caddr_t)SCARG(uap, nsv), (caddr_t)sv,
		    sizeof (vec));
		if (error)
			return (error);
		sv->sv_flags ^= SA_RESTART;	/* opposite of SV_INTERRUPT */
		setsigvec(p, signum, (struct sigaction *)sv);
	}
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_killpg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_killpg_args /* {
		syscallarg(int) pgid;
		syscallarg(int) signum;
	} */ *uap = v;

#ifdef COMPAT_09
	SCARG(uap, pgid) = (short) SCARG(uap, pgid);
#endif

	if ((u_int)SCARG(uap, signum) >= NSIG)
		return (EINVAL);
	return (killpg1(p, SCARG(uap, signum), SCARG(uap, pgid), 0));
}
