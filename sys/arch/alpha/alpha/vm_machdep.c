/* $NetBSD: vm_machdep.c,v 1.50 1999/07/08 18:05:22 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: vm_machdep.c,v 1.50 1999/07/08 18:05:22 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/alpha.h>
#include <machine/pmap.h>
#include <machine/reg.h>


/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct md_coredump cpustate;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(cpustate);

	cpustate.md_tf = *p->p_md.md_tf;
	cpustate.md_tf.tf_regs[FRAME_SP] = alpha_pal_rdusp();	/* XXX */
	if (p->p_md.md_flags & MDP_FPUSED)
		if (p == fpcurproc) {
			alpha_pal_wrfen(1);
			savefpstate(&cpustate.md_fpstate);
			alpha_pal_wrfen(0);
		} else
			cpustate.md_fpstate = p->p_addr->u_pcb.pcb_fp;
	else
		bzero(&cpustate.md_fpstate, sizeof(cpustate.md_fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cpustate, sizeof(cpustate),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);

	if (!error)
		chdr->c_nseg++;

	return error;
}

/*
 * cpu_exit is called as the last action during exit.
 * We block interrupts and call switch_exit.  switch_exit switches
 * to proc0's PCB and stack, then jumps into the middle of cpu_switch,
 * as if it were switching from proc0.
 */
void
cpu_exit(p)
	struct proc *p;
{

	if (p == fpcurproc)
		fpcurproc = NULL;

	/*
	 * Deactivate the exiting address space before the vmspace
	 * is freed.  Note that we will continue to run on this
	 * vmspace's context until the switch to proc0 in switch_exit().
	 */
	pmap_deactivate(p);

	(void) splhigh();
	switch_exit(p);
	/* NOTREACHED */
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
void
cpu_fork(p1, p2, stack, stacksize)
	register struct proc *p1, *p2;
	void *stack;
	size_t stacksize;
{
	struct user *up = p2->p_addr;

	p2->p_md.md_tf = p1->p_md.md_tf;
	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FPUSED;

	/*
	 * Cache the physical address of the pcb, so we can
	 * swap to it easily.
	 */
	p2->p_md.md_pcbpaddr = (void *)vtophys((vaddr_t)&up->u_pcb);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	if (p1 == fpcurproc) {
		alpha_pal_wrfen(1);
		savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
		alpha_pal_wrfen(0);
	}

	/*
	 * Copy pcb and stack from proc p1 to p2.
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	p2->p_addr->u_pcb.pcb_hw.apcb_usp = alpha_pal_rdusp();

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	/*
	 * If p1 != curproc && p1 == &proc0, we are creating a kernel
	 * thread.
	 */
	if (p1 != curproc && p1 != &proc0)
		panic("cpu_fork: curproc");
	if ((up->u_pcb.pcb_hw.apcb_flags & ALPHA_PCB_FLAGS_FEN) != 0)
		printf("DANGER WILL ROBINSON: FEN SET IN cpu_fork!\n");
#endif

	/*
	 * create the child's kernel stack, from scratch.
	 */
	{
		struct trapframe *p2tf;

		/*
		 * Pick a stack pointer, leaving room for a trapframe;
		 * copy trapframe from parent so return to user mode
		 * will be to right address, with correct registers.
		 */
		p2tf = p2->p_md.md_tf = (struct trapframe *)
		    ((char *)p2->p_addr + USPACE - sizeof(struct trapframe));
		bcopy(p1->p_md.md_tf, p2->p_md.md_tf,
		    sizeof(struct trapframe));

		/*
		 * Set up return-value registers as fork() libc stub expects.
		 */
		p2tf->tf_regs[FRAME_V0] = p1->p_pid;	/* parent's pid */
		p2tf->tf_regs[FRAME_A3] = 0;		/* no error */
		p2tf->tf_regs[FRAME_A4] = 1;		/* is child */

		/*
		 * If specificed, give the child a different stack.
		 */
		if (stack != NULL)
			p2tf->tf_regs[FRAME_SP] = (u_long)stack + stacksize;

		/*
		 * Arrange for continuation at child_return(), which
		 * will return to exception_return().  Note that the child
		 * process doesn't stay in the kernel for long!
		 * 
		 * This is an inlined version of cpu_set_kpc.
		 */
		up->u_pcb.pcb_hw.apcb_ksp = (u_int64_t)p2tf;	
		up->u_pcb.pcb_context[0] =
		    (u_int64_t)child_return;		/* s0: pc */
		up->u_pcb.pcb_context[1] =
		    (u_int64_t)exception_return;	/* s1: ra */
		up->u_pcb.pcb_context[2] =
		    (u_int64_t)p2;			/* s2: arg */
		up->u_pcb.pcb_context[7] =
		    (u_int64_t)switch_trampoline;	/* ra: assembly magic */
	}
}

/*
 * cpu_set_kpc:
 *
 * Arrange for in-kernel execution of a process to continue at the
 * named pc, as if the code at that address were called as a function
 * with argument, the current process's process pointer.
 *
 * Note that it's assumed that when the named process returns,
 * exception_return() should be invoked, to return to user mode.
 *
 * (Note that cpu_fork(), above, uses an open-coded version of this.)
 */
void
cpu_set_kpc(p, pc, arg)
	struct proc *p;
	void (*pc) __P((void *));
	void *arg;
{
	struct pcb *pcbp;

	pcbp = &p->p_addr->u_pcb;
	pcbp->pcb_context[0] = (u_int64_t)pc;	/* s0 - pc to invoke */
	pcbp->pcb_context[1] =
	    (u_int64_t)exception_return;	/* s1 - return address */
	pcbp->pcb_context[2] = (u_int64_t)arg;	/* s2 - arg */
	pcbp->pcb_context[7] =
	    (u_int64_t)switch_trampoline;	/* ra - assembly magic */
}

/*
 * Finish a swapin operation.
 * We neded to update the cached PTEs for the user area in the
 * machine dependent part of the proc structure.
 */
void
cpu_swapin(p)
	register struct proc *p;
{
	struct user *up = p->p_addr;

	/*
	 * Cache the physical address of the pcb, so we can swap to
	 * it easily.
	 */
	p->p_md.md_pcbpaddr = (void *)vtophys((vaddr_t)&up->u_pcb);
}

/*
 * cpu_swapout is called immediately before a process's 'struct user'
 * and kernel stack are unwired (which are in turn done immediately
 * before it's P_INMEM flag is cleared).  If the process is the
 * current owner of the floating point unit, the FP state has to be
 * saved, so that it goes out with the pcb, which is in the user area.
 */
void
cpu_swapout(p)
	struct proc *p;
{

	if (p != fpcurproc)
		return;

	alpha_pal_wrfen(1);
	savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
	alpha_pal_wrfen(0);
	fpcurproc = NULL;
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to have valid page table pages
 * and size must be a multiple of CLSIZE.
 *
 * Note that since all kernel page table pages are pre-allocated
 * and mapped in, we can use the Virtual Page Table.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	long fidx, tidx;
	ssize_t todo;

	if (size % CLBYTES)
		panic("pagemove");

	todo = size;			/* if testing > 0, need sign... */
	while (todo > 0) {
		fidx = VPT_INDEX(from);
		tidx = VPT_INDEX(to);

		VPT[tidx] = VPT[fidx];
		VPT[fidx] = 0;

		ALPHA_TBIS((vaddr_t)from);
		ALPHA_TBIS((vaddr_t)to);

#if defined(MULTIPROCESSOR) && 0
		pmap_tlb_shootdown(pmap_kernel(), (vaddr_t)from, PG_ASM);
		pmap_tlb_shootdown(pmap_kernel(), (vaddr_t)to, PG_ASM);
#endif

		todo -= NBPG;
		from += NBPG;
		to += NBPG;
	}
}

extern vm_map_t phys_map;

/*
 * Map a user I/O request into kernel virtual address space.
 * Note: the pages are already locked by uvm_vslock(), so we
 * do not need to pass an access_type to pmap_enter().
 */
void
vmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t faddr, taddr, off;
	paddr_t pa;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	faddr = trunc_page(bp->b_saveaddr = bp->b_data);
	off = (vaddr_t)bp->b_data - faddr;
	len = round_page(off + len);
	taddr = uvm_km_valloc_wait(phys_map, len);
	bp->b_data = (caddr_t)(taddr + off);
	len = atop(len);
	while (len--) {
		if (pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map), faddr,
		    &pa) == FALSE)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), taddr, trunc_page(pa),
		    VM_PROT_READ|VM_PROT_WRITE, TRUE, 0);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(bp, len)
	struct buf *bp;
	vsize_t len;
{
	vaddr_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}
