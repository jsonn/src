/*	$NetBSD: kern_sig.c,v 1.66.6.1 1997/09/08 23:10:34 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

#define	SIGPROP		/* include signal properties table */
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
#include <sys/malloc.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/core.h>
#include <sys/ptrace.h>
#include <sys/filedesc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <machine/cpu.h>

#include <vm/vm.h>
#include <sys/user.h>		/* for coredump */

void stop __P((struct proc *p));
void killproc __P((struct proc *, char *));

/*
 * Can process p, with pcred pc, send the signal signum to process q?
 */
#define CANSIGNAL(p, pc, q, signum) \
	((pc)->pc_ucred->cr_uid == 0 || \
	    (pc)->p_ruid == (q)->p_cred->p_ruid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_cred->p_ruid || \
	    (pc)->p_ruid == (q)->p_ucred->cr_uid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_ucred->cr_uid || \
	    ((signum) == SIGCONT && (q)->p_session == (p)->p_session))

/* ARGSUSED */
int
sys_sigaction(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(const struct sigaction *) nsa;
		syscallarg(struct sigaction *) osa;
	} */ *uap = v;
	struct sigaction vec;
	register struct sigaction *sa;
	register struct sigacts *ps = p->p_sigacts;
	register int signum;
	int error;

	signum = SCARG(uap, signum);
	if (signum <= 0 || signum >= NSIG ||
	    ((signum == SIGKILL || signum == SIGSTOP) && SCARG(uap, nsa)))
		return (EINVAL);
	sa = &vec;
	if (SCARG(uap, osa)) {
		bcopy(&ps->ps_sigact[signum], sa, sizeof(vec));
		sa->sa_mask &= ~sigmask(signum);
		error = copyout(sa, SCARG(uap, osa), sizeof(vec));
		if (error)
			return (error);
	}
	if (SCARG(uap, nsa)) {
		error = copyin(SCARG(uap, nsa), sa, sizeof (vec));
		if (error)
			return (error);
		setsigvec(p, signum, sa);
	}
	return (0);
}

void
setsigvec(p, signum, nsa)
	register struct proc *p;
	int signum;
	register struct sigaction *nsa;
{
	register struct sigacts *ps = p->p_sigacts;
	register struct sigaction *sa;
	register int bit;

	bit = sigmask(signum);
	sa = &ps->ps_sigact[signum];

	/*
	 * Change setting atomically.
	 */
	(void) splhigh();
	bcopy(nsa, sa, sizeof(struct sigaction));
	if ((sa->sa_flags & SA_NODEFER) == 0)
		sa->sa_mask |= bit;
	sa->sa_mask &= ~sigcantmask;
	if (signum == SIGCHLD) {
		if (sa->sa_flags & SA_NOCLDSTOP)
			p->p_flag |= P_NOCLDSTOP;
		else
			p->p_flag &= ~P_NOCLDSTOP;
	} else
		sa->sa_flags &= ~SA_NOCLDSTOP;

	/*
	 * If signal is being ignored, removing it from the "pending" list.
	 */
	if (_SIGIGNORE(p, signum))
		p->p_siglist &= ~bit;		/* never to be seen again */

	(void) spl0();
}

/*
 * Initialize signal state for process 0.
 */
void
siginit(p)
	struct proc *p;
{

	/*
	 * Note: We don't need to set sa_mask for each signal
	 * since it only matters if the signal is being caught
	 * by the process, and that is handed in setsigvec().
	 */
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(p)
	register struct proc *p;
{
	register struct sigacts *ps = p->p_sigacts;
	register int nc;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	for (nc = 0; nc < NSIG; nc++) {
		if (SIGCATCH(p, nc)) {
			if (sigprop[nc] & SA_IGNORE)
				p->p_siglist &= ~sigmask(nc);
			SIGACTION(p, nc) = SIG_DFL;
		}
	}

	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	p->p_sigstk.ss_flags = SS_DISABLE;
	p->p_sigstk.ss_size = 0;
	p->p_sigstk.ss_sp = 0;

	ps->ps_flags = 0;
}

/*
 * Manipulate signal mask.
 * Note that we receive new mask, not pointer,
 * and return old mask as return value;
 * the library stub does the rest.
 */
int
sys_sigprocmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(sigset_t) mask;
	} */ *uap = v;
	int error = 0;

	*retval = p->p_sigmask;
	(void) splhigh();

	switch (SCARG(uap, how)) {
	case SIG_BLOCK:
		p->p_sigmask |= SCARG(uap, mask) &~ sigcantmask;
		break;

	case SIG_UNBLOCK:
		p->p_sigmask &= ~SCARG(uap, mask);
		break;

	case SIG_SETMASK:
		p->p_sigmask = SCARG(uap, mask) &~ sigcantmask;
		break;
	
	default:
		error = EINVAL;
		break;
	}
	(void) spl0();
	return (error);
}

/* ARGSUSED */
int
sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_siglist;
	return (0);
}

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 */
/* ARGSUSED */
int
sys_sigsuspend(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigsuspend_args /* {
		syscallarg(int) mask;
	} */ *uap = v;
	register struct sigacts *ps = p->p_sigacts;

	/*
	 * When returning from sigpause, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigacts structure
	 * to indicate this.
	 */
	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SAS_OLDMASK;
	p->p_sigmask = SCARG(uap, mask) &~ sigcantmask;
	while (tsleep((caddr_t) ps, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

/* ARGSUSED */
int
sys_sigaltstack(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_sigaltstack_args /* {
		syscallarg(const struct sigaltstack *) nss;
		syscallarg(struct sigaltstack *) oss;
	} */ *uap = v;
	struct sigacts *psp;
	struct sigaltstack ss;
	int error;

	psp = p->p_sigacts;
	if ((psp->ps_flags & SAS_ALTSTACK) == 0)
		p->p_sigstk.ss_flags |= SS_DISABLE;
	if (SCARG(uap, oss) && (error = copyout(&p->p_sigstk,
	    SCARG(uap, oss), sizeof (struct sigaltstack))))
		return (error);
	if (SCARG(uap, nss) == 0)
		return (0);
	error = copyin(SCARG(uap, nss), &ss, sizeof (ss));
	if (error)
		return (error);
	if (ss.ss_flags & SS_DISABLE) {
		if (p->p_sigstk.ss_flags & SS_ONSTACK)
			return (EINVAL);
		psp->ps_flags &= ~SAS_ALTSTACK;
		p->p_sigstk.ss_flags = ss.ss_flags;
		return (0);
	}
	if (ss.ss_size < MINSIGSTKSZ)
		return (ENOMEM);
	psp->ps_flags |= SAS_ALTSTACK;
	p->p_sigstk = ss;			/* struct copy */
	return (0);
}

/* ARGSUSED */
int
sys_kill(cp, v, retval)
	register struct proc *cp;
	void *v;
	register_t *retval;
{
	register struct sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	register struct proc *p;
	register struct pcred *pc = cp->p_cred;

	if ((u_int)SCARG(uap, signum) >= NSIG)
		return (EINVAL);
	if (SCARG(uap, pid) > 0) {
		/* kill single process */
		if ((p = pfind(SCARG(uap, pid))) == NULL)
			return (ESRCH);
		if (!CANSIGNAL(cp, pc, p, SCARG(uap, signum)))
			return (EPERM);
		if (SCARG(uap, signum))
			psignal(p, SCARG(uap, signum));
		return (0);
	}
	switch (SCARG(uap, pid)) {
	case -1:		/* broadcast signal */
		return (killpg1(cp, SCARG(uap, signum), 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(cp, SCARG(uap, signum), 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(cp, SCARG(uap, signum), -SCARG(uap, pid), 0));
	}
	/* NOTREACHED */
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
int
killpg1(cp, signum, pgid, all)
	register struct proc *cp;
	int signum, pgid, all;
{
	register struct proc *p;
	register struct pcred *pc = cp->p_cred;
	struct pgrp *pgrp;
	int nfound = 0;
	
	if (all)	
		/* 
		 * broadcast 
		 */
		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM || 
			    p == cp || !CANSIGNAL(cp, pc, p, signum))
				continue;
			nfound++;
			if (signum)
				psignal(p, signum);
		}
	else {
		if (pgid == 0)		
			/* 
			 * zero pgid means send to my process group.
			 */
			pgrp = cp->p_pgrp;
		else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL)
				return (ESRCH);
		}
		for (p = pgrp->pg_members.lh_first; p != 0;
		    p = p->p_pglist.le_next) {
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
			    !CANSIGNAL(cp, pc, p, signum))
				continue;
			nfound++;
			if (signum && p->p_stat != SZOMB)
				psignal(p, signum);
		}
	}
	return (nfound ? 0 : ESRCH);
}

/*
 * Send a signal to a process group.
 */
void
gsignal(pgid, signum)
	int pgid, signum;
{
	struct pgrp *pgrp;

	if (pgid && (pgrp = pgfind(pgid)))
		pgsignal(pgrp, signum, 0);
}

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
pgsignal(pgrp, signum, checkctty)
	struct pgrp *pgrp;
	int signum, checkctty;
{
	register struct proc *p;

	if (pgrp)
		for (p = pgrp->pg_members.lh_first; p != 0;
		    p = p->p_pglist.le_next)
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				psignal(p, signum);
}

/*
 * Send a signal caused by a trap to the current process.
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 */
void
trapsignal(p, signum, code)
	struct proc *p;
	register int signum;
	u_long code;
{
	register struct sigacts *ps = p->p_sigacts;
	struct sigaction *sa = &ps->ps_sigact[signum];
	int mask;

	/*
	 * If target process is exiting, don't bother posting
	 * the signal, because signals aren't noticed until
	 * the switch to user space, and that doesn't happen
	 * for a zombie.
	 */
	if ((ps = p->p_sigacts) == NULL)
		return;

	mask = sigmask(signum);
	if ((p->p_flag & P_TRACED) == 0 && SIGCATCH(p, signum) &&
	    (p->p_sigmask & mask) == 0) {
		p->p_stats->p_ru.ru_nsignals++;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_PSIG))
			ktrpsig(p->p_tracep, signum, sa->sa_handler, 
				p->p_sigmask, code);
#endif
		(*p->p_emul->e_sendsig)(sa->sa_handler, signum,
		    p->p_sigmask, code);
		p->p_sigmask |= sa->sa_mask;
		if (sa->sa_flags & SA_RESETHAND)
			SIGACTION(p, signum) = SIG_DFL;
	} else {
		ps->ps_code = code;	/* XXX for core dump/debugger */
		psignal(p, signum);
	}
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 */
void
psignal(p, signum)
	register struct proc *p;
	register int signum;
{
	register int s, prop;
	register sig_t action;
	struct sigacts *ps;
	int mask;

	if ((u_int)signum >= NSIG || signum == 0)
		panic("psignal signal number");

	/*
	 * If target process is exiting, don't bother posting
	 * the signal, because signals aren't noticed until
	 * the switch to user space, and that doesn't happen
	 * for a zombie.
	 */
	if ((ps = p->p_sigacts) == NULL)
		return;

	mask = sigmask(signum);
	prop = sigprop[signum];

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (p->p_flag & P_TRACED)
		action = SIG_DFL;
	else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 * (Note: we don't ignore SIGCONT 
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		if (SIGIGNORE(p, signum))
			return;
		if (p->p_sigmask & mask)
			action = SIG_HOLD;
		else if (SIGCATCH(p, signum))
			action = SIG_CATCH;
		else {
			action = SIG_DFL;

			if (prop & SA_KILL && p->p_nice > NZERO)
				p->p_nice = NZERO;

			/*
			 * If sending a tty stop signal to a member of an
			 * orphaned process group, discard the signal here if
			 * the action is default; don't stop the process below
			 * if sleeping, and don't clear any pending SIGCONT.
			 */
			if (prop & SA_TTYSTOP && p->p_pgrp->pg_jobc == 0)
				return;
		}
	}

	if (prop & SA_CONT)
		p->p_siglist &= ~stopsigmask;

	if (prop & SA_STOP)
		p->p_siglist &= ~contsigmask;

	p->p_siglist |= mask;

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD && ((prop & SA_CONT) == 0 || p->p_stat != SSTOP))
		return;
	s = splhigh();
	switch (p->p_stat) {

	case SSLEEP:
		/*
		 * If process is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((p->p_flag & P_SINTR) == 0)
			goto out;
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in issignal() and stop
		 * for the parent.
		 */
		if (p->p_flag & P_TRACED)
			goto run;
		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) && action == SIG_DFL) {
			p->p_siglist &= ~mask;
			goto out;
		}
		/*
		 * When a sleeping process receives a stop
		 * signal, process immediately if possible.
		 */
		if ((prop & SA_STOP) && action == SIG_DFL) {
			/*
			 * If a child holding parent blocked,
			 * stopping could cause deadlock.
			 */
			if (p->p_flag & P_PPWAIT)
				goto out;
			p->p_siglist &= ~mask;
			p->p_xstat = signum;
			if ((p->p_pptr->p_flag & P_NOCLDSTOP) == 0)
				psignal(p->p_pptr, SIGCHLD);
			stop(p);
			goto out;
		}
		/*
		 * All other (caught or default) signals
		 * cause the process to run.
		 */
		goto runfast;
		/*NOTREACHED*/

	case SSTOP:
		/*
		 * If traced process is already stopped,
		 * then no further action is necessary.
		 */
		if (p->p_flag & P_TRACED)
			goto out;

		/*
		 * Kill signal always sets processes running.
		 */
		if (signum == SIGKILL)
			goto runfast;

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in p_siglist, as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * p_siglist.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, then it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			if (action == SIG_DFL)
				p->p_siglist &= ~mask;
			if (action == SIG_CATCH)
				goto runfast;
			if (p->p_wchan == 0)
				goto run;
			p->p_stat = SSLEEP;
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again.
			 * (If we did the shell could get confused.)
			 */
			p->p_siglist &= ~mask;		/* take it away */
			goto out;
		}

		/*
		 * If process is sleeping interruptibly, then simulate a
		 * wakeup so that when it is continued, it will be made
		 * runnable and can look at the signal.  But don't make
		 * the process runnable, leave it stopped.
		 */
		if (p->p_wchan && p->p_flag & P_SINTR)
			unsleep(p);
		goto out;

	default:
		/*
		 * SRUN, SIDL, SZOMB do nothing with the signal,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		if (p == curproc)
			signotify(p);
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 */
	if (p->p_priority > PUSER)
		p->p_priority = PUSER;
run:
	setrunnable(p);
out:
	splx(s);
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling issignal
 * by checking the pending signal masks in the CURSIG macro.) The normal call
 * sequence is
 *
 *	while (signum = CURSIG(curproc))
 *		postsig(signum);
 */
int
issignal(p)
	register struct proc *p;
{
	register int signum, mask, prop;

	for (;;) {
		mask = p->p_siglist & ~p->p_sigmask;
		if (p->p_flag & P_PPWAIT)
			mask &= ~stopsigmask;
		if (mask == 0)	 	/* no signal to send */
			return (0);
		signum = ffs((long)mask);
		mask = sigmask(signum);
		p->p_siglist &= ~mask;		/* take the signal! */

		/*
		 * We should see pending but ignored signals
		 * only if P_TRACED was on when they were posted.
		 */
		if (SIGIGNORE(p, signum) && (p->p_flag & P_TRACED) == 0)
			continue;

		if (p->p_flag & P_TRACED && (p->p_flag & P_PPWAIT) == 0) {
			/*
			 * If traced, always stop, and stay
			 * stopped until released by the debugger.
			 */
			p->p_xstat = signum;
			if ((p->p_flag & P_FSTRACE) == 0)
				psignal(p->p_pptr, SIGCHLD);
			do {
				stop(p);
				mi_switch();
			} while (!trace_req(p) && p->p_flag & P_TRACED);

			/*
			 * If we are no longer being traced, or the parent
			 * didn't give us a signal, look for more signals.
			 */
			if ((p->p_flag & P_TRACED) == 0 || p->p_xstat == 0)
				continue;

			/*
			 * If the new signal is being masked, look for other
			 * signals.
			 */
			signum = p->p_xstat;
			mask = sigmask(signum);
			if ((p->p_sigmask & mask) != 0)
				continue;
			p->p_siglist &= ~mask;		/* take the signal! */
		}

		prop = sigprop[signum];

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((long)SIGACTION(p, signum)) {

		case (long)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf("Process (pid %d) got signal %d\n",
				    p->p_pid, signum);
#endif
				break;		/* == ignore */
			}
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (p->p_flag & P_TRACED ||
		    		    (p->p_pgrp->pg_jobc == 0 &&
				    prop & SA_TTYSTOP))
					break;	/* == ignore */
				p->p_xstat = signum;
				if ((p->p_pptr->p_flag & P_NOCLDSTOP) == 0)
					psignal(p->p_pptr, SIGCHLD);
				stop(p);
				mi_switch();
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				goto keep;
			/*NOTREACHED*/

		case (long)SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (p->p_flag & P_TRACED) == 0)
				printf("issignal\n");
			break;		/* == ignore */

		default:
			/*
			 * This signal has an action, let
			 * postsig() process it.
			 */
			goto keep;
		}
	}
	/* NOTREACHED */

keep:
	p->p_siglist |= mask;		/* leave the signal for later */
	return (signum);
}

/*
 * Put the argument process into the stopped state and notify the parent
 * via wakeup.  Signals are handled elsewhere.  The process must not be
 * on the run queue.
 */
void
stop(p)
	register struct proc *p;
{

	p->p_stat = SSTOP;
	p->p_flag &= ~P_WAITED;
	wakeup((caddr_t)p->p_pptr);
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
postsig(signum)
	register int signum;
{
	register struct proc *p = curproc;
	register struct sigacts *ps = p->p_sigacts;
	register struct sigaction *sa = &ps->ps_sigact[signum];
	register sig_t action;
	u_long code;
	int mask, returnmask;

#ifdef DIAGNOSTIC
	if (signum == 0)
		panic("postsig");
#endif
	mask = sigmask(signum);
	p->p_siglist &= ~mask;
	action = SIGACTION(p, signum);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG))
		ktrpsig(p->p_tracep,
		    signum, action, ps->ps_flags & SAS_OLDMASK ?
		    ps->ps_oldmask : p->p_sigmask, 0);
#endif
	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(p, signum);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
#ifdef DIAGNOSTIC
		if (action == SIG_IGN || (p->p_sigmask & mask))
			panic("postsig action");
#endif
		/*
		 * Set the new mask value and also defer further
		 * occurences of this signal.
		 *
		 * Special case: user has done a sigpause.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpause is what we want
		 * restored after the signal processing is completed.
		 */
		(void) splhigh();
		if (ps->ps_flags & SAS_OLDMASK) {
			returnmask = ps->ps_oldmask;
			ps->ps_flags &= ~SAS_OLDMASK;
		} else
			returnmask = p->p_sigmask;
		p->p_sigmask |= sa->sa_mask;
		if (sa->sa_flags & SA_RESETHAND)
			SIGACTION(p, signum) = SIG_DFL;
		(void) spl0();
		p->p_stats->p_ru.ru_nsignals++;
		if (ps->ps_sig != signum) {
			code = 0;
		} else {
			code = ps->ps_code;
			ps->ps_code = 0;
		}
		(*p->p_emul->e_sendsig)(action, signum, returnmask, code);
	}
}

/*
 * Kill the current process for stated reason.
 */
void
killproc(p, why)
	struct proc *p;
	char *why;
{

	log(LOG_ERR, "pid %d was killed: %s\n", p->p_pid, why);
	uprintf("sorry, pid %d was killed: %s\n", p->p_pid, why);
	psignal(p, SIGKILL);
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 */
void
sigexit(p, signum)
	register struct proc *p;
	int signum;
{

	p->p_acflag |= AXSIG;
	if (sigprop[signum] & SA_CORE) {
		p->p_sigacts->ps_sig = signum;
		if (coredump(p) == 0)
			signum |= WCOREFLAG;
	}
	exit1(p, W_EXITCODE(0, signum));
	/* NOTREACHED */
}

/*
 * Dump core, into a file named "progname.core", unless the process was
 * setuid/setgid.
 */
int
coredump(p)
	register struct proc *p;
{
	register struct vnode *vp;
	register struct vmspace *vm = p->p_vmspace;
	register struct ucred *cred = p->p_cred->pc_ucred;
	struct nameidata nd;
	struct vattr vattr;
	int error, error1;
	char name[MAXCOMLEN+6];		/* progname.core */
	struct core core;

	/*
	 * Make sure the process has not set-id, to prevent data leaks.
	 */
	if (p->p_flag & P_SUGID)
		return (EPERM);

	/*
	 * Refuse to core if the data + stack + user size is larger than
	 * the core dump limit.  XXX THIS IS WRONG, because of mapped
	 * data.
	 */
	if (USPACE + ctob(vm->vm_dsize + vm->vm_ssize) >=
	    p->p_rlimit[RLIMIT_CORE].rlim_cur)
		return (EFBIG);		/* better error code? */

	/*
	 * The core dump will go in the current working directory.  Make
	 * sure that mount flags allow us to write core dumps there.
	 */
	if (p->p_fd->fd_cdir->v_mount->mnt_flag & MNT_NOCOREDUMP)
		return (EPERM);

	sprintf(name, "%s.core", p->p_comm);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, name, p);
	error = vn_open(&nd, O_CREAT | FWRITE, S_IRUSR | S_IWUSR);
	if (error)
		return (error);
	vp = nd.ni_vp;

	/* Don't dump to non-regular files or files with links. */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred, p) || vattr.va_nlink != 1) {
		error = EINVAL;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_LEASE(vp, p, cred, LEASE_WRITE);
	VOP_SETATTR(vp, &vattr, cred, p);
	p->p_acflag |= ACORE;
	bcopy(p, &p->p_addr->u_kproc.kp_proc, sizeof(struct proc));
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);

	core.c_midmag = 0;
	strncpy(core.c_name, p->p_comm, MAXCOMLEN);
	core.c_nseg = 0;
	core.c_signo = p->p_sigacts->ps_sig;
	core.c_ucode = p->p_sigacts->ps_code;
	core.c_cpusize = 0;
	core.c_tsize = (u_long)ctob(vm->vm_tsize);
	core.c_dsize = (u_long)ctob(vm->vm_dsize);
	core.c_ssize = (u_long)round_page(ctob(vm->vm_ssize));
	error = cpu_coredump(p, vp, cred, &core);
	if (error)
		goto out;
	if (core.c_midmag == 0) {
		/* XXX
		 * cpu_coredump() didn't bother to set the magic; assume
		 * this is a request to do a traditional dump. cpu_coredump()
		 * is still responsible for setting sensible values in
		 * the core header.
		 */
		if (core.c_cpusize == 0)
			core.c_cpusize = USPACE; /* Just in case */
		error = vn_rdwr(UIO_WRITE, vp, vm->vm_daddr,
		    (int)core.c_dsize,
		    (off_t)core.c_cpusize, UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
		if (error)
			goto out;
		error = vn_rdwr(UIO_WRITE, vp,
		    (caddr_t) trunc_page(USRSTACK - ctob(vm->vm_ssize)),
		    core.c_ssize,
		    (off_t)(core.c_cpusize + core.c_dsize), UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
	} else {
		/*
		 * vm_coredump() spits out all appropriate segments.
		 * All that's left to do is to write the core header.
		 */
		error = vm_coredump(p, vp, cred, &core);
		if (error)
			goto out;
		error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&core,
		    (int)core.c_hdrsize, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
	}
out:
	VOP_UNLOCK(vp);
	error1 = vn_close(vp, FWRITE, cred, p);
	if (error == 0)
		error = error1;
	return (error);
}

/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
/* ARGSUSED */
int
sys_nosys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	psignal(p, SIGSYS);
	return (ENOSYS);
}

/*
 * Functions to manipulate `per-process' signal actions.  These may be
 * shared among multiple NetBSD processes, but are common across a POSIX
 * process, which may have multiple threads of execution.
 */

/*
 * Duplicate one NetBSD process's signal actions.  This is the common
 * operation done at fork() time.
 */
struct sigacts *
sigacts_copy(p)
	struct proc *p;
{
	struct sigacts *nsiga;

	MALLOC(nsiga, struct sigacts *, sizeof(struct sigacts), M_SUBPROC,
	    M_WAITOK);
	bcopy(p->p_sigacts, nsiga, sizeof(struct sigacts));
	nsiga->ps_refcnt = 1;
	return (nsiga);
}

/*
 * Make p2 share p1's signal actions.
 */
void
sigacts_share(p1, p2)
	struct proc *p1, *p2;
{

	p2->p_sigacts = p1->p_sigacts;
	p1->p_sigacts->ps_refcnt++;
}

/*
 * Make this process not share its signal actions.
 */
void
sigacts_unshare(p)
	struct proc *p;
{
	struct sigacts *nsiga;

	if (p->p_sigacts->ps_refcnt == 1)
		return;

	nsiga = sigacts_copy(p);
	p->p_sigacts->ps_refcnt--;
	p->p_sigacts = nsiga;
}

/*
 * Remove a reference to this process's sigacts.  If it
 * is the last reference, free the structure.
 */
void
sigacts_free(p)
	struct proc *p;
{

	if (--p->p_sigacts->ps_refcnt == 0)
		FREE(p->p_sigacts, M_SUBPROC);

	p->p_sigacts = NULL;
}
