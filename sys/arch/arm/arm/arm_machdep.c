/*	$NetBSD: arm_machdep.c,v 1.2.6.8 2001/12/17 21:31:25 nathanw Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_compat_netbsd.h"
#include "opt_progmode.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: arm_machdep.c,v 1.2.6.8 2001/12/17 21:31:25 nathanw Exp $");

#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/pool.h>
#include <sys/lwp.h>
#include <sys/ucontext.h>
#include <sys/savar.h>

#include <machine/pcb.h>
#include <machine/cpufunc.h>
#include <machine/vmparam.h>

static __inline struct trapframe *
process_frame(struct lwp *l)
{

	return (l->l_addr->u_pcb.pcb_tf);
}

/*
 * Clear registers on exec
 */

void
setregs(struct lwp *l, struct exec_package *pack, u_long stack)
{
	struct trapframe *tf;

	tf = l->l_addr->u_pcb.pcb_tf;

	memset(tf, 0, sizeof(*tf));
	tf->tf_r0 = (u_int)PS_STRINGS;
#ifdef COMPAT_13
	tf->tf_r12 = stack;			/* needed by pre 1.4 crt0.c */
#endif
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = pack->ep_entry;
	tf->tf_svc_lr = 0x77777777;		/* Something we can see */
	tf->tf_pc = pack->ep_entry;
#ifdef PROG32
	tf->tf_spsr = PSR_USR32_MODE;
#endif

	l->l_addr->u_pcb.pcb_flags = 0;
}

/*
 * startlwp:
 *
 *	Start a new LWP.
 */
void
startlwp(void *arg)
{
	int err;
	ucontext_t *uc = arg; 
	struct lwp *l = curproc;

	err = cpu_setmcontext(l, &uc->uc_mcontext, uc->uc_flags);
#ifdef DIAGNOSTIC
	if (err)
		printf("Error %d from cpu_setmcontext.", err);
#endif
	pool_put(&lwp_uc_pool, uc);

	userret(l);
}

/*
 * XXX This is a terrible name.
 */
void
upcallret(struct lwp *l)
{

	userret(l);
}

/*
 * cpu_stashcontext:
 *
 *	Save the user-level ucontext_t on the LWP's own stack.
 */
ucontext_t *
cpu_stashcontext(struct lwp *l)
{
	struct trapframe *tf;
	ucontext_t u, *up;
	void *stack;

	tf = process_frame(l);

	stack = (void *)(tf->tf_usr_sp - sizeof(ucontext_t));

	getucontext(l, &u);
	up = stack;

	if (copyout(&u, stack, sizeof(ucontext_t)) != 0) {
		/* Copying onto the stack didn't work.  Die. */
#ifdef DIAGNOSTIC
		printf("cpu_stashcontext: couldn't copyout context of %d.%d\n",
		    l->l_proc->p_pid, l->l_lid);
#endif
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	return (up);
}

/*
 * cpu_upcall:
 *
 *	Send an an upcall to userland.
 */

void 
cpu_upcall(struct lwp *l, int type, int nevents, int ninterrupted, void *sas, void *ap, void *sp, sa_upcall_t upcall)
{
	struct proc *p = l->l_proc;
	struct trapframe *tf;
	struct saframe *sf, frame;
	extern char sigcode[], upcallcode[];

	tf = process_frame(l);

	/* Finally, copy out the rest of the frame. */
#if 0 /* First 4 args in regs (see below). */
	frame.sa_type = type;
	frame.sa_sas = sas;
	frame.sa_events = nevents;
	frame.sa_interrupted = ninterrupted;
#endif
	frame.sa_arg = ap;
	frame.sa_upcall = upcall;

	sf = (struct saframe *)sp - 1;
	if (copyout(&frame, sf, sizeof(frame)) != 0) {
		/* Copying onto the stack didn't work. Die. */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	tf->tf_r0 = type;
	tf->tf_r1 = (int) sas;
	tf->tf_r2 = nevents;
	tf->tf_r3 = ninterrupted;
	tf->tf_usr_sp = (int) sf;
	tf->tf_pc = (int) ((caddr_t)p->p_sigctx.ps_sigcode + (
	    (caddr_t)upcallcode - (caddr_t)sigcode));
#ifndef arm26
	cpu_cache_syncI();	/* XXX really necessary? */
#endif
}
