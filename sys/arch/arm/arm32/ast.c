/*	$NetBSD: ast.c,v 1.1.12.2 2001/11/15 06:39:21 thorpej Exp $	*/

/*
 * Copyright (c) 1994,1995 Mark Brinicombe
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTERS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * ast.c
 *
 * Code to handle ast's and returns to user mode
 *
 * Created      : 11/10/94
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/vmmeter.h>
#include <sys/savar.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>

#include <uvm/uvm_extern.h>

int want_resched = 0;

void
userret(l)
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	int sig;

	/* Take pending signals. */
	while ((sig = (CURSIG(l))) != 0)
		postsig(sig);

	/* If our process is on the way out, die. */
	if (p->p_flag & P_WEXIT)
		lwp_exit(l);

	/* Invoke any pending upcalls. */
	if (l->l_flag & L_SA_UPCALL)
		cpu_upcall(l);

	curcpu()->ci_schedstate.spc_curpriority = l->l_priority = l->l_usrpri;

#ifdef DEBUG
	if (current_spl_level != _SPL_0) {
		printf("userret: spl level=%d on exit\n", current_spl_level);
#ifdef DDB
		Debugger();
#endif	/* DDB */
	}
#endif	/* DEBUG */
}


/*
 * void ast(trapframe_t *frame)
 *
 * Handle asynchronous system traps.
 * This is called from the irq handler to deliver signals
 * and switch processes if required.
 * userret() does all the signal delivery and process switching work
 */

void
ast(frame)
	trapframe_t *frame;
{
	struct lwp *l = curproc;
	struct proc *p;

	uvmexp.traps++;
	uvmexp.softs++;

#ifdef DEBUG
	if (l == NULL)
		panic("ast: no curproc!");
	if (&l->l_addr->u_pcb == 0)
		panic("ast: no pcb!");
	if ((GetCPSR() & PSR_MODE) != PSR_SVC32_MODE)
		panic("ast: not in SVC32 mode");
#endif	

	p = l->l_proc;

	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	/* Allow a forced task switch. */
	if (want_resched)
		preempt(NULL);

	userret(l);
}

/* End of ast.c */
