/*	$NetBSD: userret.h,v 1.3.4.2 2002/09/06 08:39:18 jdolecek Exp $	*/

/*
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

#include <powerpc/fpu.h>

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
static __inline void
userret(struct proc *p, struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb;
	int sig;

	/* Take pending signals. */
	while ((sig = CURSIG(p)) != 0) {
		postsig(sig);
	}

	pcb = &p->p_addr->u_pcb;

	/*
	 * If someone stole the fp or vector unit while we were away,
	 * disable it
	 */
#ifdef PPC_HAVE_FPU
	if ((pcb->pcb_flags & PCB_FPU) &&
	    (p != ci->ci_fpuproc || pcb->pcb_fpcpu != ci)) {
		frame->srr1 &= ~PSL_FP;
	}
#endif
#ifdef ALTIVEC
	if ((pcb->pcb_flags & PCB_ALTIVEC) &&
	    (p != ci->ci_vecproc || pcb->pcb_veccpu != ci)) {
		frame->srr1 &= ~PSL_VEC;
	}

	/*
	 * If the new process isn't the current AltiVec process on this
	 * cpu, we need to stop any data streams that are active (since
	 * it will be a different address space).
	 */
	if (ci->ci_vecproc != NULL && ci->ci_vecproc != p) {
		__asm __volatile("dssall;sync");
	}
#endif

	ci->ci_schedstate.spc_curpriority = p->p_priority = p->p_usrpri;
}
