/*	$NetBSD: vm_machdep.c,v 1.121.6.1.2.10 2010/02/05 07:36:50 matt Exp $	*/

/*
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
 * 3. Neither the name of the University nor the names of its contributors
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
/*
 * Copyright (c) 1988 University of Utah.
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
#include "opt_coredump.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: vm_machdep.c,v 1.121.6.1.2.10 2010/02/05 07:36:50 matt Exp $");

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
 * Finish a fork operation, with lwp l2 nearly set up.
 * Copy and update the pcb and trapframe, making the child ready to run.
 *
 * Rig the child's kernel stack so that it will start out in
 * lwp_trampoline() and call child_return() with l2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * l1 is the process being forked; if l1 == &lwp0, we are creating
 * a kernel thread, and the return path and argument are specified with
 * `func' and `arg'.
 *
 * If an alternate user-level stack is requested (with non-zero values
 * in both the stack and stacksize args), set up the user stack pointer
 * accordingly.
 */
void
cpu_lwp_fork(struct lwp *l1, struct lwp *l2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{
	struct pcb *pcb;
	struct trapframe *tf;
	pt_entry_t *pte;
	int i, x;

	l2->l_md.md_ss_addr = 0;
	l2->l_md.md_ss_instr = 0;
	l2->l_md.md_astpending = 0;

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
	 * Copy pcb from lwp l1 to l2.
	 * Copy l1 trapframe atop on l2 stack space, so return to user mode
	 * will be to right address, with correct registers.
	 */
	memcpy(&l2->l_addr->u_pcb, &l1->l_addr->u_pcb, sizeof(struct pcb));
	tf = (struct trapframe *)((char *)l2->l_addr + USPACE) - 1;
	*tf = *l1->l_md.md_utf;

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf->tf_regs[_R_SP] = (intptr_t)stack + stacksize;

	l2->l_md.md_utf = tf;
	l2->l_md.md_flags = l1->l_md.md_flags & MDP_FPUSED;
	x = (MIPS_HAS_R4K_MMU) ?
	    (MIPS3_PG_G | MIPS3_PG_RO | MIPS3_PG_WIRED) :
	    MIPS1_PG_G;
	pte = kvtopte(l2->l_addr);
	for (i = 0; i < UPAGES; i++)
		l2->l_md.md_upte[i] = pte[i].pt_entry &~ x;

	pcb = &l2->l_addr->u_pcb;
	pcb->pcb_context.val[_L_S0] = (intptr_t)func;			/* S0 */
	pcb->pcb_context.val[_L_S1] = (intptr_t)arg;			/* S1 */
	pcb->pcb_context.val[MIPS_CURLWP_LABEL] = (intptr_t)l2;		/* T8 */
	pcb->pcb_context.val[_L_SP] = (intptr_t)tf;			/* SP */
	pcb->pcb_context.val[_L_RA] = (intptr_t)lwp_trampoline;		/* RA */
#ifdef _LP64
	KASSERT(pcb->pcb_context.val[_L_SR] & MIPS_SR_KX);
#endif
#ifdef IPL_ICU_MASK
	pcb->pcb_ppl = 0;	/* machine dependent interrupt mask */
#endif
}

/*
 * Set the given LWP to start at the given function via the
 * lwp_trampoline.
 */
void
cpu_setfunc(struct lwp *l, void (*func)(void *), void *arg)
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct trapframe *tf = l->l_md.md_utf;

	KASSERT(tf == (struct trapframe *)((char *)l->l_addr + USPACE) - 1);

	pcb->pcb_context.val[_L_S0] = (intptr_t)func;			/* S0 */
	pcb->pcb_context.val[_L_S1] = (intptr_t)arg;			/* S1 */
	pcb->pcb_context.val[MIPS_CURLWP_LABEL] = (intptr_t)l;		/* T8 */
	pcb->pcb_context.val[_L_SP] = (intptr_t)tf;			/* SP */
	pcb->pcb_context.val[_L_RA] = (intptr_t)setfunc_trampoline;	/* RA */
#ifdef _LP64
	KASSERT(pcb->pcb_context.val[_L_SR] & MIPS_SR_KX);
#endif
#ifdef IPL_ICU_MASK
	pcb->pcb_ppl = 0;	/* machine depenedend interrupt mask */
#endif
}

static struct evcnt uarea_remapped = 
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "uarea", "remapped");
static struct evcnt uarea_reallocated = 
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "uarea", "reallocated");
EVCNT_ATTACH_STATIC(uarea_remapped);
EVCNT_ATTACH_STATIC(uarea_reallocated);

void
cpu_uarea_remap(struct lwp *l)
{
	bool uarea_ok;
	vaddr_t va;
	paddr_t pa;

	/*
	 * Grab the starting physical address of the uarea.
	 */
	va = (vaddr_t)l->l_addr;
	if (!pmap_extract(pmap_kernel(), va, &pa))
		panic("%s: pmap_extract(%#"PRIxVADDR") failed", __func__, va);

	/*
	 * Check to see if the existing uarea is physically contiguous.
	 */
	uarea_ok = true;
	for (vaddr_t i = PAGE_SIZE; uarea_ok && i < USPACE; i += PAGE_SIZE) {
		paddr_t pa0;
		if (!pmap_extract(pmap_kernel(), va + i, &pa0))
			panic("%s: pmap_extract(%#"PRIxVADDR") failed",
			    __func__, va+1);
		uarea_ok = (pa0 - pa == i);
	}

#ifndef _LP64
	/*
	 * If this is a 32bit kernel, it needs to be mappedable via KSEG0
	 */
	uarea_ok = uarea_ok && (pa + USPACE - 1 <= MIPS_PHYS_MASK);
#endif
	printf("ctx=%#"PRIxVADDR" utf=%p\n", 
	    (vaddr_t)l->l_addr->u_pcb.pcb_context.val[_L_SP],
	    l->l_md.md_utf);
	KASSERT((vaddr_t)l->l_addr->u_pcb.pcb_context.val[_L_SP] == (vaddr_t)l->l_md.md_utf);
	vaddr_t sp = l->l_addr->u_pcb.pcb_context.val[_L_SP] - (vaddr_t)l->l_addr;

	if (!uarea_ok) {
		struct pglist pglist;
#ifdef _LP64
		const paddr_t high = mips_avail_end;
#else
		const paddr_t high = MIPS_KSEG1_START - MIPS_KSEG0_START;
#endif
		int error;

		/*
		 * Allocate a new physically contiguou uarea which can be
		 * direct-mapped.
		 */
		error = uvm_pglistalloc(USPACE, mips_avail_start, high,
		    USPACE_ALIGN, 0, &pglist, 1, 1);
		if (error)
			panic("softint_init_md: uvm_pglistalloc failed: %d",
			    error);

		/*
		 * Get the physical address from the first page.
		 */
		pa = VM_PAGE_TO_PHYS(TAILQ_FIRST(&pglist));
	}

	/*
	 * Now set the new uarea (if it's different). If l->l_addr was already
	 * direct mapped address then routine really change anything but that's
	 * not probably so don't micro optimize for it.
	 */
#ifdef _LP64
	va = MIPS_PHYS_TO_XKPHYS_CACHED(pa);
#else
	va = MIPS_PHYS_TO_KSEG0(pa);
#endif
	if (!uarea_ok) {
		((struct trapframe *)(va + USPACE))[-1] = *l->l_md.md_utf;
		*(struct pcb *)va = l->l_addr->u_pcb;
		/*
		 * Discard the old uarea.
		 */
		uvm_uarea_free(USER_TO_UAREA(l->l_addr), curcpu());
		uarea_reallocated.ev_count++;
	}

	l->l_addr = (struct user *)va;
	l->l_addr->u_pcb.pcb_context.val[_L_SP] = sp + va;
	l->l_md.md_utf = (struct trapframe *)((char *)l->l_addr + USPACE) - 1;
	uarea_remapped.ev_count++;
}

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */
void
cpu_swapin(struct lwp *l)
{
	pt_entry_t *pte;
	int i, x;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switchto() can quickly map
	 * in the user struct and kernel stack.
	 */
	x = (MIPS_HAS_R4K_MMU) ?
	    (MIPS3_PG_G | MIPS3_PG_RO | MIPS3_PG_WIRED) :
	    MIPS1_PG_G;
	pte = kvtopte(l->l_addr);
	for (i = 0; i < UPAGES; i++)
		l->l_md.md_upte[i] = pte[i].pt_entry &~ x;
}

void
cpu_lwp_free(struct lwp *l, int proc)
{

	if ((l->l_md.md_flags & MDP_FPUSED) && l == fpcurlwp)
		fpcurlwp = &lwp0;	/* save some NULL checks */
	KASSERT(fpcurlwp != l);
}

void
cpu_lwp_free2(struct lwp *l)
{

	(void)l;
}

#ifdef COREDUMP
/*
 * Dump the machine specific segment at the start of a core dump.
 */
int
cpu_coredump(struct lwp *l, void *iocookie, struct core *chdr)
{
	int error;
	struct coreseg cseg;
	struct cpustate {
		struct trapframe frame;
		struct fpreg fpregs;
	} cpustate;

	if (iocookie == NULL) {
		CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
		chdr->c_hdrsize = ALIGN(sizeof(struct core));
		chdr->c_seghdrsize = ALIGN(sizeof(struct coreseg));
		chdr->c_cpusize = sizeof(struct cpustate);
		chdr->c_nseg++;
		return 0;
	}

	if ((l->l_md.md_flags & MDP_FPUSED) && l == fpcurlwp)
		savefpregs(l);
	cpustate.frame = *l->l_md.md_utf;
	cpustate.fpregs = l->l_addr->u_pcb.pcb_fpregs;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = coredump_write(iocookie, UIO_SYSSPACE, &cseg,
	    chdr->c_seghdrsize);
	if (error)
		return error;

	return coredump_write(iocookie, UIO_SYSSPACE, &cpustate,
	    chdr->c_cpusize);
}
#endif

static struct evcnt evcnt_vmapbuf =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "vmapbuf", "calls");
static struct evcnt evcnt_vmapbuf_adjustments =
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, &evcnt_vmapbuf,
	"vmapbuf", "adjustments");

/*
 * Map a user I/O request into kernel virtual address space.
 */
void
vmapbuf(struct buf *bp, vsize_t len)
{
	struct pmap *upmap;
	vaddr_t uva;	/* User VA (map from) */
	vaddr_t kva;	/* Kernel VA (new to) */
	paddr_t pa;	/* physical address */
	vsize_t off;
	vsize_t coloroff;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	evcnt_vmapbuf.ev_count++;
	uva = mips_trunc_page(bp->b_saveaddr = bp->b_data);
	coloroff = uva & ptoa(uvmexp.colormask);
	if (coloroff)
		evcnt_vmapbuf_adjustments.ev_count++;
	off = (vaddr_t)bp->b_data - uva;
	len = mips_round_page(off + len);
	kva = uvm_km_alloc(phys_map, len + coloroff, ptoa(uvmexp.ncolors),
	    UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	kva += coloroff;
	bp->b_data = (void *)(kva + off);
	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	do {
		if (pmap_extract(upmap, uva, &pa) == false)
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
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t kva;
	vsize_t off;
	vsize_t coloroff;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	kva = mips_trunc_page(bp->b_data);
	coloroff = kva & ptoa(uvmexp.colormask);
	off = (vaddr_t)bp->b_data - kva;
	len = mips_round_page(off + len);
	pmap_remove(vm_map_pmap(phys_map), kva, kva + len);
	pmap_update(pmap_kernel());
	uvm_km_free(phys_map, kva - coloroff, len + coloroff, UVM_KMF_VAONLY);
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
kvtophys(vaddr_t kva)
{
	pt_entry_t *pte;
	paddr_t phys;

	if (kva >= VM_MIN_KERNEL_ADDRESS) {
		if (kva >= VM_MAX_KERNEL_ADDRESS)
			goto overrun;

		pte = kvtopte(kva);
		if ((size_t) (pte - Sysmap) >= Sysmapsize)  {
			printf("oops: Sysmap overrun, max %d index %zd\n",
			       Sysmapsize, pte - Sysmap);
		}
		if (!mips_pg_v(pte->pt_entry)) {
			printf("kvtophys: pte not valid for %#"PRIxVADDR"\n",
			    kva);
		}
		phys = mips_tlbpfn_to_paddr(pte->pt_entry) | (kva & PGOFSET);
		return phys;
	}
	if (MIPS_KSEG1_P(kva))
		return MIPS_KSEG1_TO_PHYS(kva);

	if (MIPS_KSEG0_P(kva))
		return MIPS_KSEG0_TO_PHYS(kva);
#ifdef _LP64
	if (MIPS_XKPHYS_P(kva))
		return MIPS_XKPHYS_TO_PHYS(kva);
#endif
overrun:
	printf("Virtual address %#"PRIxVADDR": cannot map to physical\n", kva);
#ifdef DDB
	Debugger();
	return 0;	/* XXX */
#endif
	panic("kvtophys");
}
