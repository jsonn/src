/*	$NetBSD: hpux_sig.c,v 1.16.6.1 1997/09/08 23:17:39 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_sig.c 1.4 92/01/20$
 *
 *	@(#)hpux_sig.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Signal related HPUX compatibility routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/hpux_syscallargs.h>

/* indexed by HPUX signal number - 1 */
char hpuxtobsdsigmap[NSIG] = {
/*01*/	SIGHUP,  SIGINT, SIGQUIT, SIGILL,   SIGTRAP, SIGIOT,  SIGEMT,   SIGFPE,
/*09*/  SIGKILL, SIGBUS, SIGSEGV, SIGSYS,   SIGPIPE, SIGALRM, SIGTERM,  SIGUSR1,
/*17*/  SIGUSR2, SIGCHLD, 0,      SIGVTALRM,SIGPROF, SIGIO,   SIGWINCH, SIGSTOP,
/*25*/	SIGTSTP, SIGCONT,SIGTTIN, SIGTTOU,  SIGURG,  0,       0,        0
};

/* indexed by BSD signal number - 1 */
char bsdtohpuxsigmap[NSIG] = {
/*01*/	 1,  2,  3,  4,  5,  6,  7,  8,
/*09*/   9, 10, 11, 12, 13, 14, 15, 29,
/*17*/  24, 25, 26, 18, 27, 28, 22,  0,
/*25*/	 0, 20, 21, 23,  0, 16, 17,  0
};

/*
 * XXX: In addition to mapping the signal number we also have
 * to see if the "old" style signal mechinism is needed.
 * If so, we set the OUSIG flag.  This is not really correct
 * as under HP-UX "old" style handling can be set on a per
 * signal basis and we are setting it for all signals in one
 * swell foop.  I suspect we can get away with this since I
 * doubt any program of interest mixes the two semantics.
 */
int
hpux_sys_sigvec(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigvec_args *uap = v;
	struct sigvec vec;
	struct sigacts *ps = p->p_sigacts;
	struct sigaction *sa;
	struct sigvec *sv;
	int sig;
	int bit, error;

	sig = hpuxtobsdsig(SCARG(uap, signo));
	if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
		return (EINVAL);
	sa = &ps->ps_sigact[sig];
	sv = &vec;
	if (SCARG(uap, osv)) {
		sv->sv_handler = sa->sa_handler;
		sv->sv_mask = sa->sa_mask & ~sigmask(sig);
		sv->sv_flags = 0;
		if (sa->sa_flags & SA_ONSTACK)
			sv->sv_flags |= SV_ONSTACK;
		if ((sa->sa_flags & SA_RESTART) == 0)
			sv->sv_flags |= SV_INTERRUPT;
		if (sa->sa_flags & SA_RESETHAND)
			sv->sv_flags |= HPUXSV_RESET;
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
		if (sig == SIGCONT && sv->sv_handler == SIG_IGN)
			return (EINVAL);
		sv->sv_flags ^= SA_RESTART;
		setsigvec(p, sig, (struct sigaction *)sv);
#if 0
/* XXX -- SOUSIG no longer exists, do something here */
		if (sv->sv_flags & HPUXSV_RESET)
			p->p_flag |= SOUSIG;		/* XXX */
#endif
	}
	return (0);
}

int
hpux_sys_sigblock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigblock_args *uap = v;

	(void) splhigh();
	*retval = bsdtohpuxmask(p->p_sigmask);
	p->p_sigmask |= hpuxtobsdmask(SCARG(uap, mask)) &~ sigcantmask;
	(void) spl0();
	return (0);
}

int
hpux_sys_sigsetmask(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigsetmask_args *uap = v;

	(void) splhigh();
	*retval = bsdtohpuxmask(p->p_sigmask);
	p->p_sigmask = hpuxtobsdmask(SCARG(uap, mask)) &~ sigcantmask;
	(void) spl0();
	return (0);
}

int
hpux_sys_sigpause(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigpause_args *uap = v;

	SCARG(uap, mask) = hpuxtobsdmask(SCARG(uap, mask));
	return (sys_sigsuspend(p, uap, retval));
}

/* not totally correct, but close enuf' */
int
hpux_sys_kill(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_kill_args *uap = v;

	if (SCARG(uap, signo)) {
		SCARG(uap, signo) = hpuxtobsdsig(SCARG(uap, signo));
		if (SCARG(uap, signo) == 0)
			SCARG(uap, signo) = NSIG;
	}
	return (sys_kill(p, uap, retval));
}

/*
 * The following (sigprocmask, sigpending, sigsuspend, sigaction are
 * POSIX calls.  Under BSD, the library routine dereferences the sigset_t
 * pointers before traping.  Not so under HP-UX.
 */

/*
 * Manipulate signal mask.
 * Note that we receive new mask, not pointer,
 * and return old mask as return value;
 * the library stub does the rest.
 */
int
hpux_sys_sigprocmask(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigprocmask_args *uap = v;
	int mask, error = 0;
	hpux_sigset_t sigset;

	/*
	 * Copy out old mask first to ensure no errors.
	 * (proc sigmask should not be changed if call fails for any reason)
	 */
	if (SCARG(uap, oset)) {
		bzero((caddr_t)&sigset, sizeof(sigset));
		sigset.sigset[0] = bsdtohpuxmask(p->p_sigmask);
		if (copyout((caddr_t)&sigset, (caddr_t)SCARG(uap, oset),
		    sizeof(sigset)))
			return (EFAULT);
	}
	if (SCARG(uap, set)) {
		if (copyin((caddr_t)SCARG(uap, set), (caddr_t)&sigset,
		    sizeof(sigset)))
			return (EFAULT);
		mask = hpuxtobsdmask(sigset.sigset[0]);
		(void) splhigh();
		switch (SCARG(uap, how)) {
		case HPUXSIG_BLOCK:
			p->p_sigmask |= mask &~ sigcantmask;
			break;
		case HPUXSIG_UNBLOCK:
			p->p_sigmask &= ~mask;
			break;
		case HPUXSIG_SETMASK:
			p->p_sigmask = mask &~ sigcantmask;
			break;
		default:
			error = EINVAL;
			break;
		}
		(void) spl0();
	}
	return (error);
}

int
hpux_sys_sigpending(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigpending_args *uap = v;
	hpux_sigset_t sigset;

	sigset.sigset[0] = bsdtohpuxmask(p->p_siglist);
	return (copyout((caddr_t)&sigset, (caddr_t)SCARG(uap, set),
	    sizeof(sigset)));
}

int
hpux_sys_sigsuspend(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigsuspend_args *uap = v;
	struct sigacts *ps = p->p_sigacts;
	hpux_sigset_t sigset;
	int mask;

	if (copyin((caddr_t)SCARG(uap, set), (caddr_t)&sigset, sizeof(sigset)))
		return (EFAULT);
	mask = hpuxtobsdmask(sigset.sigset[0]);
	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SAS_OLDMASK;
	p->p_sigmask = mask &~ sigcantmask;
	(void) tsleep((caddr_t)ps, PPAUSE | PCATCH, "pause", 0);
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

int
hpux_sys_sigaction(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigaction_args *uap = v;
	struct hpux_sigaction action;
	struct sigacts *ps = p->p_sigacts;
	struct sigaction *sa;
	struct hpux_sigaction *hsa;
	int sig;
	int bit;

	sig = hpuxtobsdsig(SCARG(uap, signo));
	if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
		return (EINVAL);

	sa = &ps->ps_sigact[sig];
	hsa = &action;
	if (SCARG(uap, osa)) {
		hsa->sa_handler = sa->sa_handler;
		bzero(&hsa->sa_mask, sizeof(hsa->sa_mask));
		hsa->sa_mask.sigset[0] = bsdtohpuxmask(sa->sa_mask);
		hsa->sa_flags = 0;
		if (sa->sa_flags & SA_ONSTACK)
			hsa->sa_flags |= HPUXSA_ONSTACK;
		if (sa->sa_flags & SA_RESETHAND)
			hsa->sa_flags |= HPUXSA_RESETHAND;
		if (sa->sa_flags & SA_NOCLDSTOP)
			hsa->sa_flags |= HPUXSA_NOCLDSTOP;
		if (copyout((caddr_t)sa, (caddr_t)SCARG(uap, osa),
		    sizeof (action)))
			return (EFAULT);
	}
	if (SCARG(uap, nsa)) {
		struct sigaction act;

		if (copyin((caddr_t)SCARG(uap, nsa), (caddr_t)hsa,
		    sizeof (action)))
			return (EFAULT);
		if (sig == SIGCONT && hsa->sa_handler == SIG_IGN)
			return (EINVAL);
		/*
		 * Create a sigaction struct for setsigvec
		 */
		act.sa_handler = sa->sa_handler;
		act.sa_mask = hpuxtobsdmask(sa->sa_mask.sigset[0]);
		act.sa_flags = SA_RESTART;
		if (sa->sa_flags & HPUXSA_ONSTACK)
			act.sa_flags |= SA_ONSTACK;
		if (sa->sa_flags & HPUXSA_NOCLDSTOP)
			act.sa_flags |= SA_NOCLDSTOP;
		setsigvec(p, sig, &act);
#if 0
/* XXX -- SOUSIG no longer exists, do something here */
		if (sa->sa_flags & HPUXSA_RESETHAND)
			p->p_flag |= SOUSIG;		/* XXX */
#endif
	}
	return (0);
}

int
hpux_sys_ssig_6x(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_ssig_6x_args /* {
		syscallarg(int) signo;
		syscallarg(sig_t) fun;
	} */ *uap = v;
	int a;
	struct sigaction vec;
	struct sigaction *sa = &vec;

	a = hpuxtobsdsig(SCARG(uap, signo));
	sa->sa_handler = SCARG(uap, fun);
	/*
	 * Kill processes trying to use job control facilities
	 * (this'll help us find any vestiges of the old stuff).
	 */
	if ((a &~ 0377) ||
	    (sa->sa_handler != SIG_DFL && sa->sa_handler != SIG_IGN &&
	     ((int)sa->sa_handler) & 1)) {
		psignal(p, SIGSYS);
		return (0);
	}
	if (a <= 0 || a >= NSIG || a == SIGKILL || a == SIGSTOP ||
	    (a == SIGCONT && sa->sa_handler == SIG_IGN))
		return (EINVAL);
	sa->sa_mask = 0;
	sa->sa_flags = 0;
	*retval = (int)SIGACTION(p, a);
	setsigvec(p, a, sa);
#if 0
	p->p_flag |= SOUSIG;		/* mark as simulating old stuff */
#endif
	return (0);
}

/* signal numbers: convert from HPUX to BSD */
int
hpuxtobsdsig(sig)
	int sig;
{
	if (--sig < 0 || sig >= NSIG)
		return(0);
	return((int)hpuxtobsdsigmap[sig]);
}

/* signal numbers: convert from BSD to HPUX */
int
bsdtohpuxsig(sig)
	int sig;
{
	if (--sig < 0 || sig >= NSIG)
		return(0);
	return((int)bsdtohpuxsigmap[sig]);
}

/* signal masks: convert from HPUX to BSD (not pretty or fast) */
int
hpuxtobsdmask(mask)
	int mask;
{
	int nmask, sig, nsig;

	if (mask == 0 || mask == -1)
		return(mask);
	nmask = 0;
	for (sig = 1; sig < NSIG; sig++)
		if ((mask & sigmask(sig)) && (nsig = hpuxtobsdsig(sig)))
			nmask |= sigmask(nsig);
	return(nmask);
}

int
bsdtohpuxmask(mask)
	int mask;
{
	int nmask, sig, nsig;

	if (mask == 0 || mask == -1)
		return(mask);
	nmask = 0;
	for (sig = 1; sig < NSIG; sig++)
		if ((mask & sigmask(sig)) && (nsig = bsdtohpuxsig(sig)))
			nmask |= sigmask(nsig);
	return(nmask);
}
