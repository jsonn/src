/*	$NetBSD: genassym.c,v 1.5.2.1 1997/03/12 14:22:15 is Exp $	*/

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
 */

/*
 * This program is designed so that it can be both:
 * (1) Run on the native machine and generated output
 * (2) Converted to assembly and parsed by genassym.awk
 *     to produce the same output as (1) does.
 * The second method is done as follows:
 *   m68k-xxx-gcc [options] -S .../genassym.c
 *   awk -f genassym.awk < genassym.s > assym.h
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
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/mon.h>
#include <machine/vmparam.h>
#include <machine/dvma.h>
#include <machine/machdep.h>

#include "buserr.h"

/* Note: Avoid /usr/include for cross compilation! */
extern void printf __P((char *fmt, ...));
extern void exit __P((int));

#define def(name, value) { name, value }

#ifdef	__STDC__
#define	def1(name) def(#name, name)
#else
#define	def1(name) def("name", name)
#endif

#define	offsetof(type, member) ((size_t)(&((type *)0)->member))

/*
 * Note: genassym.awk cares about the form of this structure,
 * as well as the names and placement of the "asdefs" array
 * and the "nassefs" variable below.  Clever, but fragile.
 */
struct nv {
	char n[28];
	int v;
};

struct nv assyms[] = {

	/* bus error stuff */
	/* def1(BUSERR_REG), XXX */
	/* def1(BUSERR_MMU), XXX */

	/* XXX: for copy.s */
	def("M68030", 1),

	/* 68k isms */
	def1(PSL_LOWIPL),
	def1(PSL_HIGHIPL),
	def1(PSL_USER),
	def1(PSL_S),
	def1(FC_CONTROL),
	def1(FC_SUPERD),
	def1(FC_USERD),
	def1(IC_CLEAR),
	def1(DC_CLEAR),
	def1(CACHE_CLR),

	/* sun3 memory map */
	def1(DVMA_SPACE_START),
	def1(MONSTART),
	def1(PROM_BASE),
	def1(USRSTACK),

	/* kernel-isms */
	def1(KERNBASE),
	def1(USPACE),
	def1(NBPG),

	/* system calls */
	def1(SYS_sigreturn),

	/* errno-isms */
	def1(EFAULT),
	def1(ENAMETOOLONG),

	/* trap types: locore.s includes trap.h */

	/*
	 * unix structure-isms
	 */

	/* proc fields and values */
	def("P_FORW", offsetof(struct proc, p_forw)),
	def("P_BACK", offsetof(struct proc, p_back)),
	def("P_VMSPACE", offsetof(struct proc, p_vmspace)),
	def("P_ADDR", offsetof(struct proc, p_addr)),
	def("P_PRIORITY", offsetof(struct proc, p_priority)),
	def("P_STAT", offsetof(struct proc, p_stat)),
	def("P_WCHAN", offsetof(struct proc, p_wchan)),
	def("P_FLAG", offsetof(struct proc, p_flag)),
	def("P_MDFLAG", offsetof(struct proc, p_md.md_flags)),
	def("P_MDREGS", offsetof(struct proc, p_md.md_regs)),
	def1(SRUN),

	/* XXX: HP-UX trace bit? */

	/* VM/pmap structure fields */
	def("VM_PMAP", offsetof(struct vmspace, vm_pmap)),
	def("PM_A_PHYS", offsetof(struct pmap, pm_a_phys)),
	def("PM_A_TMGR", offsetof(struct pmap, pm_a_tmgr)),

	/* pcb offsets */
	def("PCB_FLAGS", offsetof(struct pcb, pcb_flags)),
	def("PCB_PS", offsetof(struct pcb, pcb_ps)),
	def("PCB_USP", offsetof(struct pcb, pcb_usp)),
	def("PCB_REGS", offsetof(struct pcb, pcb_regs[0])),
	def("PCB_ONFAULT", offsetof(struct pcb, pcb_onfault)),
	def("PCB_FPCTX", offsetof(struct pcb, pcb_fpregs)),
	def("SIZEOF_PCB", sizeof(struct pcb)),

	/* exception frame offset/sizes */
	def("FR_SP", offsetof(struct trapframe, tf_regs[15])),
	def("FR_ADJ", offsetof(struct trapframe, tf_stackadj)),
	def("FR_HW", offsetof(struct trapframe, tf_sr)),
	def("FR_SIZE", sizeof(struct trapframe)),

	/* FP frame offsets */
	def("FPF_REGS", offsetof(struct fpframe, fpf_regs[0])),
	def("FPF_FPCR", offsetof(struct fpframe, fpf_fpcr)),
};
int nassyms = sizeof(assyms)/sizeof(assyms[0]);

main()
{
	char *name;
	int i, val;

	for (i = 0; i < nassyms; i++) {
		name = assyms[i].n;
		val  = assyms[i].v;

		printf("#define\t%s\t", name);
		/* Hack to make the output easier to verify. */
		if ((val < 0) || (val > 999))
			printf("0x%x\n", val);
		else
			printf("%d\n", val);
	}

	exit(0);
}
