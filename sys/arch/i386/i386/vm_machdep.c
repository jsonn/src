/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	$Id: vm_machdep.c,v 1.29.2.1 1994/08/15 14:47:28 mycroft Exp $
 */

/*
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/reg.h>

#include "npx.h"
#if NNPX > 0
extern struct proc *npxproc;
#endif

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct user *up = p2->p_addr;
	int addr, i;
	extern char kstack[];

	/* Copy the pcb. */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	p2->p_md.md_regs = p1->p_md.md_regs;

	/*
	 * Wire top of address space of child to it's kstack.
	 * First, fault in a page of pte's to map it.
	 */
	addr = trunc_page((u_int)vtopte(kstack));
	vm_map_pageable(&p2->p_vmspace->vm_map, addr, addr+NBPG, FALSE);
	for (i = 0; i < UPAGES; i++)
		pmap_enter(&p2->p_vmspace->vm_pmap, kstack + i * NBPG,
		    pmap_extract(kernel_pmap,
		        (vm_offset_t)(((int)p2->p_addr) + i * NBPG)),
		    VM_PROT_READ | VM_PROT_WRITE, TRUE);

	pmap_activate(&p2->p_vmspace->vm_pmap, &up->u_pcb);

	/*
	 * Copy the stack.
	 *
	 * When we first switch to the child, this will return from cpu_switch()
	 * rather than savectx().  cpu_switch returns a pointer to the current
	 * process; savectx() returns 0.  Thus we can look for a non-zero
	 * return value to indicate that we're in the child.
	 */
	if (savectx(up, 1)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
}

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switch_exit() with the old proc as an
 * argument.  switch_exit() first switches to proc0's context, then does the
 * vmspace_free() and kmem_free() that we don't do here, and finally jumps
 * into switch() to wait for another process to wake up.
 */
void
cpu_exit(p)
	register struct proc *p;
{
	extern int _default_ldt, currentldt;
	struct vmspace *vm;

#if NNPX > 0
	if (npxproc == p)
		npxexit();
#endif

#ifdef USER_LDT
	if (p->p_addr->u_pcb.pcb_ldt) {
		lldt(currentldt = _default_ldt);	/* XXX necessary? */
		kmem_free(kernel_map, (vm_offset_t)p->p_addr->u_pcb.pcb_ldt,
		    (p->p_addr->u_pcb.pcb_ldt_len * sizeof(union descriptor)));
	}
#endif

	vm = p->p_vmspace;
	if (vm->vm_refcnt == 1)
		vm_map_remove(&vm->vm_map, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);

	cnt.v_swtch++;
	switch_exit(p);
}

/*
 * Dump the machine specific segment at the start of a core dump.
 */     
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	register struct user *up = p->p_addr;
	struct cpustate {
		struct trapframe regs;
		struct save87 fpstate;
	} cpustate;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_I386, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(cpustate);

	cpustate.regs = *(struct trapframe *)p->p_md.md_regs;
#if NNPX > 0
	if (p == npxproc)
		npxsave(&cpustate.fpstate); /* ??? */
	else
#endif
		bzero((caddr_t)&cpustate.fpstate, sizeof(cpustate.fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_I386, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cpustate, sizeof(cpustate),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * Set a red zone in the kernel stack after the u. area.
 */
setredzone(pte, vaddr)
	u_short *pte;
	caddr_t vaddr;
{
/* eventually do this by setting up an expand-down stack segment
   for ss0: selector, allowing stack access down to top of u.
   this means though that protection violations need to be handled
   thru a double fault exception that must do an integral task
   switch to a known good context, within which a dump can be
   taken. a sensible scheme might be to save the initial context
   used by sched (that has physical memory mapped 1:1 at bottom)
   and take the dump while still in mapped mode */
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the Sysmap,
 * and size must be a multiple of CLSIZE.
 */
pagemove(from, to, size)
	register caddr_t from, to;
	int size;
{
	register pt_entry_t *fpte, *tpte;

	if (size % CLBYTES)
		panic("pagemove");
	fpte = kvtopte(from);
	tpte = kvtopte(to);
	while (size > 0) {
		*tpte++ = *fpte;
		*fpte++ = 0;
		from += NBPG;
		to += NBPG;
		size -= NBPG;
	}
	tlbflush();
}

/*
 * Convert kernel VA to physical address
 */
kvtop(addr)
	register caddr_t addr;
{
	vm_offset_t va;

	va = pmap_extract(kernel_pmap, (vm_offset_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
}

#ifdef notdef
/*
 * The probe[rw] routines should probably be redone in assembler
 * for efficiency.
 */
prober(addr)
	register u_int addr;
{
	register int page;
	register struct proc *p;

	if (addr >= USRSTACK)
		return(0);
	p = u.u_procp;
	page = btop(addr);
	if (page < dptov(p, p->p_dsize) || page > sptov(p, p->p_ssize))
		return(1);
	return(0);
}

probew(addr)
	register u_int addr;
{
	register int page;
	register struct proc *p;

	if (addr >= USRSTACK)
		return(0);
	p = u.u_procp;
	page = btop(addr);
	if (page < dptov(p, p->p_dsize) || page > sptov(p, p->p_ssize))
		return((*(int *)vtopte(p, page) & PG_PROT) == PG_UW);
	return(0);
}

/*
 * NB: assumes a physically contiguous kernel page table
 *     (makes life a LOT simpler).
 */
kernacc(addr, count, rw)
	register u_int addr;
	int count, rw;
{
	register pd_entry_t *pde;
	register pt_entry_t *pte;
	register int ix, cnt;
	extern long Syssize;

	if (count <= 0)
		return(0);
	pde = (pd_entry_t *)((u_int)u.u_procp->p_p0br + u.u_procp->p_szpt * NBPG);
	ix = (addr & PD_MASK) >> PDSHIFT;
	cnt = ((addr + count + (1 << PDSHIFT) - 1) & PD_MASK) >> PDSHIFT;
	cnt -= ix;
	for (pde += ix; cnt; cnt--, pde++)
		if (pde->pd_v == 0)
			return(0);
	ix = btop(addr-KERNBASE);
	cnt = btop(addr-KERNBASE+count+NBPG-1);
	if (cnt > (int)&Syssize)
		return(0);
	cnt -= ix;
	for (pte = &Sysmap[ix]; cnt; cnt--, pte++)
		if (pte->pg_v == 0 /*|| (rw == B_WRITE && pte->pg_prot == 1)*/) 
			return(0);
	return(1);
}

useracc(addr, count, rw)
	register u_int addr;
	int count, rw;
{
	register int (*func)();
	register u_int addr2;
	extern int prober(), probew();

	if (count <= 0)
		return(0);
	addr2 = addr;
	addr += count;
	func = (rw == B_READ) ? prober : probew;
	do {
		if ((*func)(addr2) == 0)
			return(0);
		addr2 = (addr2 + NBPG) & ~PGOFSET;
	} while (addr2 < addr);
	return(1);
}
#endif

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.  Requests fall into
 * one of five catagories:
 *
 *	B_PHYS|B_UAREA:	User u-area swap.
 *			Address is relative to start of u-area (p_addr).
 *	B_PHYS|B_PAGET:	User page table swap.
 *			Address is a kernel VA in usrpt (Usrptmap).
 *	B_PHYS|B_DIRTY:	Dirty page push.
 *			Address is a VA in proc2's address space.
 *	B_PHYS|B_PGIN:	Kernel pagein of user pages.
 *			Address is VA in user's address space.
 *	B_PHYS:		User "raw" IO request.
 *			Address is VA in user's address space.
 *
 * All requests are (re)mapped into kernel VA space via the useriomap
 * (a name with only slightly more meaning than "kernelmap")
 */
vmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t faddr, taddr, off;
	pt_entry_t *fpte, *tpte;
	pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vm_offset_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = kmem_alloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	/*
	 * The region is locked, so we expect that pmap_pte() will return
	 * non-NULL.
	 */
	fpte = pmap_pte(vm_map_pmap(&bp->b_proc->p_vmspace->vm_map), faddr);
	tpte = pmap_pte(vm_map_pmap(phys_map), taddr);
	do {
		*tpte++ = *fpte++;
		len -= PAGE_SIZE;
	} while (len);
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
vunmapbuf(bp, len)
	struct buf *bp;
	vm_size_t len;
{
	vm_offset_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vm_offset_t)bp->b_data - addr;
	len = round_page(off + len);
	kmem_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = 0;
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
cpu_reset() {

	/* force a shutdown by unmapping entire address space ! */
	bzero((caddr_t) PTD, NBPG);

	/* "good night, sweet prince .... <THUNK!>" */
	tlbflush(); 
	/* just in case */
	for (;;);
}
