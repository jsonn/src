/*	$NetBSD: compat_13_machdep.c,v 1.2.8.2 2002/05/29 21:31:57 nathanw Exp $ */

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

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lwp.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>

#include <sys/sa.h>
#include <sys/syscallargs.h>

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
compat_13_sys_sigreturn(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct compat_13_sys_sigreturn_args /* {
		syscallarg(struct sigcontext13 *) sigcntxp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct sigcontext13 sc, *scp;
	sigset_t mask;
	struct trapframe *tf;
	int error;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(l))
		sigexit(l, SIGILL);

	if ((error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc)) != 0)
		return (error);
	scp = &sc;

	tf = l->l_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((scp->sc_pc | scp->sc_npc) & 3) != 0)
		return (EINVAL);
	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (scp->sc_psr & PSR_ICC);
	tf->tf_pc = scp->sc_pc;
	tf->tf_npc = scp->sc_npc;
	tf->tf_global[1] = scp->sc_g1;
	tf->tf_out[0] = scp->sc_o0;
	tf->tf_out[6] = scp->sc_sp;

	if (scp->sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask */
	native_sigset13_to_sigset(&scp->sc_mask, &mask);
	(void) sigprocmask1(p, SIG_SETMASK, &mask, 0);

	return (EJUSTRETURN);
}
