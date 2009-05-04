/*	$NetBSD: vm_machdep.c,v 1.1.4.2 2009/05/04 08:12:11 yamt Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	@(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 */

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1989, 1990 William Jolitz
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	@(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 */

/*
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vm_machdep.c,v 1.1.4.2 2009/05/04 08:12:11 yamt Exp $");

#include "opt_mtrr.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#ifdef MTRR
#include <machine/mtrr.h>
#endif

#ifdef __x86_64__
#include <machine/fpu.h>
#else
#include "npx.h"
#if NNPX > 0
#define fpusave_lwp(x, y)	npxsave_lwp(x, y)
#else
#define fpusave_lwp(x, y)
#endif
#endif

static void setredzone(struct lwp *);

void
cpu_proc_fork(struct proc *p1, struct proc *p2)
{

	p2->p_md.md_flags = p1->p_md.md_flags;
	if (p1->p_flag & PK_32)
		p2->p_flag |= PK_32;
}

/*
 * Finish a new thread operation, with LWP l2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 *
 * Rig the child's kernel stack so that it will start out in
 * lwp_trampoline() and call child_return() with l2 as an
 * argument. This causes the newly-created child process to go
 * directly to user level with an apparent return value of 0 from
 * fork(), while the parent process returns normally.
 *
 * l1 is the thread being forked; if l1 == &lwp0, we are creating
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
	struct pcb *pcb = &l2->l_addr->u_pcb;
	struct trapframe *tf;

	/*
	 * If fpuproc != p1, then the fpu h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps.
	 *
	 * If fpuproc == p1, then we have to save the fpu h/w state to
	 * p1's pcb so that we can copy it.
	 */
	if (l1->l_addr->u_pcb.pcb_fpcpu != NULL) {
		fpusave_lwp(l1, true);
	}

	l2->l_md.md_flags = l1->l_md.md_flags;

	/* Copy pcb from proc p1 to p2. */
	if (l1 == curlwp) {
		/* Sync the PCB before we copy it. */
		savectx(curpcb);
	} else {
		KASSERT(l1 == &lwp0);
	}
	*pcb = l1->l_addr->u_pcb;
#if defined(XEN)
	pcb->pcb_iopl = SEL_KPL;
#endif /* defined(XEN) */

#ifdef __x86_64__
	/*
	 * Note: pcb_ldt_sel is handled in the pmap_activate() call when
	 * we run the new process.
	 */
	pcb->pcb_rsp0 = (USER_TO_UAREA(l2->l_addr) + KSTACK_SIZE - 16) & ~0xf;
	pcb->pcb_fs = l1->l_addr->u_pcb.pcb_fs;
	pcb->pcb_gs = l1->l_addr->u_pcb.pcb_gs;
#else
	pcb->pcb_esp0 = USER_TO_UAREA(l2->l_addr) + KSTACK_SIZE - 16;
	memcpy(&pcb->pcb_fsd, curpcb->pcb_fsd, sizeof(pcb->pcb_fsd));
	memcpy(&pcb->pcb_gsd, curpcb->pcb_gsd, sizeof(pcb->pcb_gsd));
	pcb->pcb_iomap = NULL;
#endif
	l2->l_md.md_astpending = 0;

	/*
	 * Copy the trapframe.
	 */
#ifdef __x86_64__
	tf = (struct trapframe *)pcb->pcb_rsp0 - 1;
#else
	tf = (struct trapframe *)pcb->pcb_esp0 - 1;
#endif
	l2->l_md.md_regs = tf;
	*tf = *l1->l_md.md_regs;
	tf->tf_trapno = T_ASTFLT;

	setredzone(l2);

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL) {
#ifdef __x86_64__
		tf->tf_rsp = (uint64_t)stack + stacksize;
#else
		tf->tf_esp = (u_int)stack + stacksize;
#endif
	}

	cpu_setfunc(l2, func, arg);
}

void
cpu_setfunc(struct lwp *l, void (*func)(void *), void *arg)
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct trapframe *tf = l->l_md.md_regs;
	struct switchframe *sf = (struct switchframe *)tf - 1;

#ifdef __x86_64__
	sf->sf_r12 = (uint64_t)func;
	sf->sf_r13 = (uint64_t)arg;
	if (func == child_return && !(l->l_proc->p_flag & PK_32))
		sf->sf_rip = (uint64_t)child_trampoline;
	else
		sf->sf_rip = (uint64_t)lwp_trampoline;
	pcb->pcb_rsp = (uint64_t)sf;
	pcb->pcb_rbp = (uint64_t)l;
#else
	sf->sf_esi = (int)func;
	sf->sf_ebx = (int)arg;
	sf->sf_eip = (int)lwp_trampoline;
	pcb->pcb_esp = (int)sf;
	pcb->pcb_ebp = (int)l;
#endif
}

void
cpu_swapin(struct lwp *l)
{

	setredzone(l);
}

void
cpu_swapout(struct lwp *l)
{

	/*
	 * Make sure we save the FP state before the user area vanishes.
	 */
	fpusave_lwp(l, true);
}

/*
 * cpu_lwp_free is called from exit() to let machine-dependent
 * code free machine-dependent resources.  Note that this routine
 * must not block.
 */
void
cpu_lwp_free(struct lwp *l, int proc)
{

	/* If we were using the FPU, forget about it. */
	if (l->l_addr->u_pcb.pcb_fpcpu != NULL) {
		fpusave_lwp(l, false);
	}

#ifdef MTRR
	if (proc && l->l_proc->p_md.md_flags & MDP_USEDMTRR)
		mtrr_clean(l->l_proc);
#endif
}

/*
 * cpu_lwp_free2 is called when an LWP is being reaped.
 * This routine may block.
 */
void
cpu_lwp_free2(struct lwp *l)
{

	/* nothing */
}

/*
 * Set a red zone in the kernel stack after the u. area.
 */
static void
setredzone(struct lwp *l)
{

#ifdef DIAGNOSTIC
	vaddr_t addr;

	addr = USER_TO_UAREA(l->l_addr);
	pmap_remove(pmap_kernel(), addr, addr + PAGE_SIZE);
	pmap_update(pmap_kernel());
#endif
}

/*
 * Convert kernel VA to physical address
 */
paddr_t
kvtop(void *addr)
{
	paddr_t pa;
	bool ret;

	ret = pmap_extract(pmap_kernel(), (vaddr_t)addr, &pa);
	KASSERT(ret == true);
	return pa;
}

/*
 * Map a user I/O request into kernel virtual address space.
 * Note: the pages are already locked by uvm_vslock(), so we
 * do not need to pass an access_type to pmap_enter().
 */
void
vmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t faddr, taddr, off;
	paddr_t fpa;

	KASSERT((bp->b_flags & B_PHYS) != 0);

	bp->b_saveaddr = bp->b_data;
	faddr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_alloc(phys_map, len, 0, UVM_KMF_VAONLY | UVM_KMF_WAITVA);
	bp->b_data = (void *)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 * XXX: unwise to expect this in a multithreaded environment.
	 * anything can happen to a pmap between the time we lock a
	 * region, release the pmap lock, and then relock it for
	 * the pmap_extract().
	 *
	 * no need to flush TLB since we expect nothing to be mapped
	 * where we we just allocated (TLB will be flushed when our
	 * mapping is removed).
	 */
	while (len) {
		(void) pmap_extract(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map),
		    faddr, &fpa);
		pmap_kenter_pa(taddr, fpa, VM_PROT_READ|VM_PROT_WRITE);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
		len -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t addr, off;

	KASSERT((bp->b_flags & B_PHYS) != 0);

	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	pmap_kremove(addr, len);
	pmap_update(pmap_kernel());
	uvm_km_free(phys_map, addr, len, UVM_KMF_VAONLY);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}
