/*	$NetBSD: vm_machdep.c,v 1.85.2.10 2003/01/03 21:34:06 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: vm_machdep.c 1.21 91/04/06
 *
 *	@(#)vm_machdep.c	8.3 (Berkeley) 1/4/94
 */

#include "opt_ddb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: vm_machdep.c,v 1.85.2.10 2003/01/03 21:34:06 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/sa.h>
#include <sys/savar.h>

#include <uvm/uvm_extern.h>

#include <mips/cache.h>
#include <mips/regnum.h>
#include <mips/locore.h>
#include <mips/pte.h>
#include <mips/psl.h>
#include <machine/cpu.h>

paddr_t kvtophys(vaddr_t);	/* XXX */

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 *
 * Rig the child's kernel stack so that it will start out in
 * proc_trampoline() and call child_return() with p2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * p1 is the process being forked; if p1 == &proc0, we are creating
 * a kernel thread, and the return path and argument are specified with
 * `func' and `arg'.
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 */
void
cpu_lwp_fork(l1, l2, stack, stacksize, func, arg)
	struct lwp *l1, *l2;
	void *stack;
	size_t stacksize;
	void (*func)(void *);
	void *arg;
{
	struct pcb *pcb;
	struct frame *f;
	pt_entry_t *pte;
	int i, x;

#ifdef MIPS3_PLUS
	/*
	 * To eliminate virtual aliases created by pmap_zero_page(),
	 * this cache flush operation is necessary.
	 * VCED on kernel stack is not allowed.
	 * XXXJRT Confirm that this is necessry, and/or fix
	 * XXXJRT pmap_zero_page().
	 */
	if (CPUISMIPS3 && mips_sdcache_line_size)
		mips_dcache_wbinv_range((vaddr_t) l2->l_addr, USPACE);
#endif

#ifdef DIAGNOSTIC
	/*
	 * If l1 != curlwp && l1 == &lwp0, we're creating a kernel thread.
	 */
	if (l1 != curlwp && l1 != &lwp0)
		panic("cpu_lwp_fork: curlwp");
#endif
	if ((l1->l_md.md_flags & MDP_FPUSED) && l1 == fpcurlwp)
		savefpregs(l1);

	/*
	 * Copy pcb from proc p1 to p2.
	 * Copy p1 trapframe atop on p2 stack space, so return to user mode
	 * will be to right address, with correct registers.
	 */
	memcpy(&l2->l_addr->u_pcb, &l1->l_addr->u_pcb, sizeof(struct pcb));
	f = (struct frame *)((caddr_t)l2->l_addr + USPACE) - 1;
	memcpy(f, l1->l_md.md_regs, sizeof(struct frame));

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		f->f_regs[SP] = (u_int)stack + stacksize;

	l2->l_md.md_regs = (void *)f;
	l2->l_md.md_flags = l1->l_md.md_flags & MDP_FPUSED;
	x = (MIPS_HAS_R4K_MMU) ? (MIPS3_PG_G|MIPS3_PG_RO|MIPS3_PG_WIRED) : MIPS1_PG_G;
	pte = kvtopte(l2->l_addr);
	for (i = 0; i < UPAGES; i++)
		l2->l_md.md_upte[i] = pte[i].pt_entry &~ x;

	pcb = &l2->l_addr->u_pcb;
	pcb->pcb_context[10] = (int)proc_trampoline;	/* RA */
	pcb->pcb_context[8] = (int)f;			/* SP */
	pcb->pcb_context[0] = (int)func;		/* S0 */
	pcb->pcb_context[1] = (int)arg;			/* S1 */
	pcb->pcb_context[11] |= PSL_LOWIPL;		/* SR */
#ifdef IPL_ICU_MASK
	pcb->pcb_ppl = 0;	/* machine dependent interrupt mask */
#endif
}

/*
 * Set the given LWP to start at the given function via the
 * proc_trampoline.
 */
void
cpu_setfunc(l, func, arg)
	struct lwp *l;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *pcb;
	struct frame *f;

	f = (struct frame *)((caddr_t)l->l_addr + USPACE) - 1;
	KASSERT(l->l_md.md_regs == f);

	pcb = &l->l_addr->u_pcb;
	pcb->pcb_context[0] = (int)func;		/* S0 */
	pcb->pcb_context[1] = (int)arg;			/* S1 */
	pcb->pcb_context[8] = (int)f - 24;		/* SP */
	pcb->pcb_context[10] = (int)proc_trampoline;	/* RA */
	pcb->pcb_context[11] |= PSL_LOWIPL;		/* SR */
#ifdef IPL_ICU_MASK
	pcb->pcb_ppl = 0;	/* machine depenedend interrupt mask */
#endif
}	

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */
void
cpu_swapin(l)
	struct lwp *l;
{
	pt_entry_t *pte;
	int i, x;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switch() can quickly map in
	 * the user struct and kernel stack.
	 */
	x = (MIPS_HAS_R4K_MMU) ? (MIPS3_PG_G|MIPS3_PG_RO|MIPS3_PG_WIRED) : MIPS1_PG_G;
	pte = kvtopte(l->l_addr);
	for (i = 0; i < UPAGES; i++)
		l->l_md.md_upte[i] = pte[i].pt_entry &~ x;
}

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switch_exit() with the old proc as an
 * argument.  switch_exit() first switches to proc0's PCB and stack,
 * schedules the dead proc's vmspace and stack to be freed, then jumps
 * into the middle of cpu_switch(), as if it were switching from proc0.
 */
void
cpu_exit(l, proc)
	struct lwp *l;
	int proc;
{
	void switch_exit(struct lwp *, void (*)(struct lwp *));

	if ((l->l_md.md_flags & MDP_FPUSED) && l == fpcurlwp)
		fpcurlwp = (struct lwp *)0;

	uvmexp.swtch++;
	(void)splhigh();
	switch_exit(l, proc ? exit2 : lwp_exit2);
	/* NOTREACHED */
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */
int
cpu_coredump(l, vp, cred, chdr)
	struct lwp *l;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct coreseg cseg;
	struct cpustate {
		struct frame frame;
		struct fpreg fpregs;
	} cpustate;
	struct proc *p = l->l_proc;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(struct core));
	chdr->c_seghdrsize = ALIGN(sizeof(struct coreseg));
	chdr->c_cpusize = sizeof(struct cpustate);

	if ((l->l_md.md_flags & MDP_FPUSED) && l == fpcurlwp)
		savefpregs(l);
	cpustate.frame = *(struct frame *)l->l_md.md_regs;
	cpustate.fpregs = l->l_addr->u_pcb.pcb_fpregs;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cpustate,
			(off_t)chdr->c_cpusize,
			(off_t)(chdr->c_hdrsize + chdr->c_seghdrsize),
			UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT,
			cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of NBPG.
 */
void
pagemove(from, to, size)
	caddr_t from, to;
	size_t size;
{
	pt_entry_t *fpte, *tpte;
	paddr_t invalid;

	if (size % NBPG)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
#ifdef MIPS3_PLUS
	if (CPUISMIPS3 &&
	    (mips_cache_indexof(from) != mips_cache_indexof(to)))
		mips_dcache_wbinv_range((vaddr_t) from, size);
#endif
	invalid = (MIPS_HAS_R4K_MMU) ? MIPS3_PG_NV | MIPS3_PG_G : MIPS1_PG_NV;
	while (size > 0) {
		tpte->pt_entry = fpte->pt_entry;
		fpte->pt_entry = invalid;
		MIPS_TBIS((vaddr_t)from);
		MIPS_TBIS((vaddr_t)to);
		fpte++; tpte++;
		size -= PAGE_SIZE;
		from += PAGE_SIZE;
		to += NBPG;
	}
}

/*
 * Map a user I/O request into kernel virtual address space.
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	struct pmap *upmap;
	vaddr_t uva;	/* User VA (map from) */
	vaddr_t kva;	/* Kernel VA (new to) */
	paddr_t pa;	/* physical address */
	vsize_t off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	uva = mips_trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	len = mips_round_page(off + len);
	kva = uvm_km_valloc_prefer_wait(phys_map, len, uva);
	bp->b_data = (caddr_t)(kva + off);

	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	do {
		if (pmap_extract(upmap, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, pa,
		    VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	} while (len);
	pmap_update(vm_map_pmap(phys_map));
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t kva;
	vsize_t off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	kva = mips_trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	len = mips_round_page(off + len);
	pmap_remove(vm_map_pmap(phys_map), kva, kva + len);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, kva, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

/*
 * Map a (kernel) virtual address to a physical address.
 *
 * MIPS processor has 3 distinct kernel address ranges:
 *
 * - kseg0 kernel "virtual address" for the   cached physical address space.
 * - kseg1 kernel "virtual address" for the uncached physical address space.
 * - kseg2 normal kernel "virtual address" mapped via the TLB.
 */
paddr_t
kvtophys(kva)
	vaddr_t kva;
{
	pt_entry_t *pte;
	paddr_t phys;

	if (kva >= MIPS_KSEG2_START) {
		if (kva >= VM_MAX_KERNEL_ADDRESS)
			goto overrun;

		pte = kvtopte(kva);
		if ((size_t) (pte - Sysmap) > Sysmapsize)  {
			printf("oops: Sysmap overrun, max %d index %d\n",
			       Sysmapsize, pte - Sysmap);
		}
		if (!mips_pg_v(pte->pt_entry)) {
			printf("kvtophys: pte not valid for %lx\n", kva);
		}
		phys = mips_tlbpfn_to_paddr(pte->pt_entry) | (kva & PGOFSET);
		return phys;
	}
	if (kva >= MIPS_KSEG1_START)
		return MIPS_KSEG1_TO_PHYS(kva);

	if (kva >= MIPS_KSEG0_START)
		return MIPS_KSEG0_TO_PHYS(kva);

overrun:
	printf("Virtual address %lx: cannot map to physical\n", kva);
#ifdef DDB
	Debugger();
	return 0;	/* XXX */
#endif
	panic("kvtophys");
}
