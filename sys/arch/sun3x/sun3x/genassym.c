/*	$NetBSD: genassym.c,v 1.1.1.1.2.2 1997/01/14 20:57:09 gwr Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)genassym.c	8.3 (Berkeley) 1/4/94
 *	from: genassym.c,v 1.9 1994/05/23 06:14:19 mycroft
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/syscall.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/mon.h>
#include <machine/vmparam.h>
#include <machine/dvma.h>

#include "buserr.h"
#include "machdep.h"

#if 1	/* XXX - Temporary hack... */
/*
 * Make this work correctly on a SPARC!
 * Should be able to fix this by adding:
 * __attribute__((packed)) where needed.
 */
struct mytrapframe {
	int 	tf_regs[16];
	short	tf_pad;
	short	tf_stackadj;
	u_short	tf_sr;
	u_short	tf_pc[2];	/* XXX was:  u_int tf_pc; */
	u_short	tf_format:4,
	        tf_vector:12;
};
#define trapframe mytrapframe
#endif	/* XXX */

#ifdef	__STDC__
#define	def1(name) def(#name, name)
#else
#define	def1(name) def("name", name)
#endif

extern void printf __P((char *fmt, ...));
extern void exit __P((int));

void
def(what, val)
	char *what;
	int val;
{
	printf("#define\t%s\t", what);
	/* Hack to make the output easier to verify. */
	if ((val < -99) || (val > 999))
		printf("0x%x\n", val);
	else
		printf("%d\n", val);
}

main()
{
	struct pcb *pcb = (struct pcb *) 0;
	struct proc *p = (struct proc *) 0;
	struct vmspace *vms = (struct vmspace *) 0;
	struct trapframe *tf = (struct trapframe *) 0;
	struct fpframe *fpf = (struct fpframe *) 0;

	/* bus error stuff */
	/* def1(BUSERR_REG); XXX */
	/* def1(BUSERR_MMU); XXX */

	/* 68k isms */
	def1(PSL_LOWIPL);
	def1(PSL_HIGHIPL);
	def1(PSL_USER);
	def1(PSL_S);
	def1(FC_CONTROL);
	def1(FC_SUPERD);
	def1(FC_USERD);
	def1(IC_CLEAR);
	def1(DC_CLEAR);
	def1(CACHE_CLR);

	/* sun3 memory map */
	def1(DVMA_SPACE_START);
	def1(MONSTART);
	def1(PROM_BASE);
	def1(USRSTACK);

	/* kernel-isms */
	def1(KERNBASE);
	def1(USPACE);

	/* system calls */
	def1(SYS_sigreturn);

	/* errno-isms */
	def1(EFAULT);
	def1(ENAMETOOLONG);

	/* trap types: locore.s includes trap.h */

	/*
	 * unix structure-isms
	 */

	/* proc fields and values */
	def("P_FORW", &p->p_forw);
	def("P_BACK", &p->p_back);
	def("P_VMSPACE", &p->p_vmspace);
	def("P_ADDR", &p->p_addr);
	def("P_PRIORITY", &p->p_priority);
	def("P_STAT", &p->p_stat);
	def("P_WCHAN", &p->p_wchan);
	def("P_FLAG", &p->p_flag);
	def("P_MDFLAG", &p->p_md.md_flags);
	def("P_MDREGS", &p->p_md.md_regs);
	def1(SRUN);

	/* HP-UX trace bit */
	def("MDP_TRCB", ffs(MDP_HPUXTRACE) - 1);

	/* VM structure fields */
	def("VM_PMAP", &vms->vm_pmap);

	/* pcb offsets */
	def("PCB_FLAGS", &pcb->pcb_flags);
	def("PCB_PS", &pcb->pcb_ps);
	def("PCB_MMUCTX", &pcb->pcb_mmuctx);
	def("PCB_USP", &pcb->pcb_usp);
	def("PCB_REGS", pcb->pcb_regs);
	def("PCB_ONFAULT", &pcb->pcb_onfault);
	def("PCB_FPCTX", &pcb->pcb_fpregs);
	def("SIZEOF_PCB", sizeof(*pcb));

	/* exception frame offset/sizes */
	def("FR_SP", &tf->tf_regs[15]);
	def("FR_ADJ", &tf->tf_stackadj);
	def("FR_HW", &tf->tf_sr);
	def("FR_SIZE", sizeof(*tf));

	/* FP frame offsets */
	def("FPF_REGS", &fpf->fpf_regs[0]);
	def("FPF_FPCR", &fpf->fpf_fpcr);

	exit(0);
}
