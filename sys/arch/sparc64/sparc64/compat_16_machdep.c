/*	$NetBSD: compat_16_machdep.c,v 1.4.2.2 2004/08/03 10:41:35 skrll Exp $ */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: compat_16_machdep.c,v 1.4.2.2 2004/08/03 10:41:35 skrll Exp $");

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <machine/signal.h>
#include <machine/frame.h>

#if defined(COMPAT_16)

#ifdef DEBUG
/* See sigdebug.h */
#include <sparc64/sparc64/sigdebug.h>
#endif

#ifdef __arch64__
#define STACK_OFFSET	BIAS
#define CPOUTREG(l,v)	copyout(&(v), (l), sizeof(v))
#undef CCFSZ
#define CCFSZ	CC64FSZ
#else
#define STACK_OFFSET	0
#define CPOUTREG(l,v)	copyout(&(v), (l), sizeof(v))
#endif

struct sigframe_sigcontext {
	int	sf_signo;		/* signal number */
	int	sf_code;		/* code */
#ifndef __arch64__
	struct	sigcontext *sf_scp;	/* SunOS user addr of sigcontext */
	int	sf_addr;		/* SunOS compat, always 0 for now */
#endif
	struct	sigcontext sf_sc;	/* actual sigcontext */
};

/*
 * Send an interrupt to process.
 */
void
sendsig_sigcontext(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct sigacts *ps = p->p_sigacts;
	void *addr; 
	struct rwindow *newsp;
#ifdef NOT_DEBUG
	struct rwindow tmpwin;
#endif
	int onstack;
	int sig = ksi->ksi_signo;
	struct sigframe_sigcontext *fp = getframe(l, sig, &onstack);
	struct sigframe_sigcontext sf;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct trapframe64 *tf = l->l_md.md_tf;
	/* Allocate an aligned sigframe */
	fp = (void *)((u_long)(fp - 1) & ~0x0f);

#ifdef DEBUG
	sigpid = p->p_pid;
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig_sigcontext: %s[%d] sig %d newusp %p scp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
	}
#endif

	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_code = ksi->ksi_trap;
#ifndef __arch64__
	sf.sf_scp = 0;
	sf.sf_addr = 0;			/* XXX */
#endif

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK;
	sf.sf_sc.sc_mask = *mask;
#ifdef COMPAT_13
	/*
	 * XXX We always have to save an old style signal mask because
	 * XXX we might be delivering a signal to a process which will
	 * XXX escape from the signal in a non-standard way and invoke
	 * XXX sigreturn() directly.
	 */
	native_sigset_to_sigset13(mask, &sf.sf_sc.__sc_mask13);
#endif
	/* Save register context. */
	sf.sf_sc.sc_sp = (long)tf->tf_out[6];
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
#ifdef __arch64__
	sf.sf_sc.sc_tstate = tf->tf_tstate; /* XXX */
#else
	sf.sf_sc.sc_psr = TSTATECCR_TO_PSR(tf->tf_tstate); /* XXX */
#endif
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];

	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (struct rwindow *)((vaddr_t)fp - sizeof(struct rwindow));
	write_user_windows();
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK))
	    printf("sendsig: saving sf to %p, setting stack pointer %p to %p\n",
		   fp, &(((struct rwindow *)newsp)->rw_in[6]),
		   (void *)(unsigned long)tf->tf_out[6]);
#endif
	if (rwindow_save(l) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) || 
#ifdef NOT_DEBUG
	    copyin(oldsp, &tmpwin, sizeof(tmpwin)) || copyout(&tmpwin, newsp, sizeof(tmpwin)) ||
#endif
	    CPOUTREG(&(((struct rwindow *)newsp)->rw_in[6]), tf->tf_out[6])) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
		printf("sendsig: stack was trashed trying to send sig %d, sending SIGILL\n", sig);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
#endif
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
	}
#endif

	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
	switch (ps->sa_sigdesc[sig].sd_vers) {
	case 0:		/* legacy on-stack sigtramp */
		addr = (void *)p->p_sigctx.ps_sigcode;
		break;

	case 1:
		addr = (void *)ps->sa_sigdesc[sig].sd_tramp;
		break;

	default:
		addr = NULL; /* XXX: gcc */
		/* Don't know what trampoline version; kill it. */
		sigexit(l, SIGILL);
	}

	tf->tf_global[1] = (vaddr_t)catcher;
	tf->tf_pc = (const vaddr_t)addr;
	tf->tf_npc = (const vaddr_t)addr + 4;
	tf->tf_out[6] = (vaddr_t)newsp - STACK_OFFSET;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid) {
		printf("sendsig: about to return to catcher %p thru %p\n", 
		       catcher, (void *)(unsigned long)addr);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
	}
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
int compat_16_sys___sigreturn14(struct lwp *, void *, register_t *);

/* ARGSUSED */
int
compat_16_sys___sigreturn14(l, v, retval)
	register struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct compat_16_sys___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc, *scp;
	register struct trapframe64 *tf;
	int error = EINVAL;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(l)) {
#ifdef DEBUG
		printf("sigreturn14: rwindow_save(%p) failed, sending SIGILL\n", p);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
#endif
		sigexit(l, SIGILL);
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sigreturn14: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
	}
#endif
	scp = SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 || (error = copyin((caddr_t)scp, &sc, sizeof sc) != 0))
#ifdef DEBUG
	{
		printf("sigreturn14: copyin failed: scp=%p\n", scp);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
		return (error);
	}
#else
		return (error);
#endif
	scp = &sc;

	tf = l->l_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((sc.sc_pc | sc.sc_npc) & 3) != 0 || (sc.sc_pc == 0) || (sc.sc_npc == 0))
#ifdef DEBUG
	{
		printf("sigreturn14: pc %p or npc %p invalid\n",
		   (void *)(unsigned long)sc.sc_pc,
		   (void *)(unsigned long)sc.sc_npc);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
		return (EINVAL);
	}
#else
		return (EINVAL);
#endif
	/* take only psr ICC field */
#ifdef __arch64__
	tf->tf_tstate = (u_int64_t)(tf->tf_tstate & ~TSTATE_CCR) | (scp->sc_tstate & TSTATE_CCR);
#else
	tf->tf_tstate = (u_int64_t)(tf->tf_tstate & ~TSTATE_CCR) | PSRCC_TO_TSTATE(scp->sc_psr);
#endif
	tf->tf_pc = (u_int64_t)scp->sc_pc;
	tf->tf_npc = (u_int64_t)scp->sc_npc;
	tf->tf_global[1] = (u_int64_t)scp->sc_g1;
	tf->tf_out[0] = (u_int64_t)scp->sc_o0;
	tf->tf_out[6] = (u_int64_t)scp->sc_sp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sigreturn14: return trapframe pc=%p sp=%p tstate=%llx\n",
		       (void *)(unsigned long)tf->tf_pc,
		       (void *)(unsigned long)tf->tf_out[6],
		       (unsigned long long)tf->tf_tstate);
#ifdef DDB
		if (sigdebug & SDB_DDB) Debugger();
#endif
	}
#endif

	/* Restore signal stack. */
	if (sc.sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &sc.sc_mask, 0);

	return (EJUSTRETURN);
}
#endif /* COMPAT_16 */
