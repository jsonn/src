/* $NetBSD: vm_machdep.c,v 1.68.2.1 2001/08/03 04:10:41 lukem Exp $ */

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

__KERNEL_RCSID(0, "$NetBSD: vm_machdep.c,v 1.68.2.1 2001/08/03 04:10:41 lukem Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/alpha.h>
#include <machine/pmap.h>
#include <machine/reg.h>

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(struct proc *p, struct vnode *vp, struct ucred *cred,
    struct core *chdr)
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
	if (p->p_md.md_flags & MDP_FPUSED) {
		if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
			fpusave_proc(p, 1);
		cpustate.md_fpstate = p->p_addr->u_pcb.pcb_fp;
	} else
		memset(&cpustate.md_fpstate, 0, sizeof(cpustate.md_fpstate));

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
cpu_exit(struct proc *p)
{

	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 0);

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
cpu_fork(struct proc *p1, struct proc *p2, void *stack, size_t stacksize,
    void (*func)(void *), void *arg)
{
	struct user *up = p2->p_addr;

	p2->p_md.md_tf = p1->p_md.md_tf;

	p2->p_md.md_flags = p1->p_md.md_flags & (MDP_FPUSED | MDP_FP_C);

	/*
	 * Cache the physical address of the pcb, so we can
	 * swap to it easily.
	 */
	p2->p_md.md_pcbpaddr = (void *)vtophys((vaddr_t)&up->u_pcb);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	if (p1->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p1, 1);

	/*
	 * Copy pcb and user stack pointer from proc p1 to p2.
	 * If specificed, give the child a different stack.
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	if (stack != NULL)
		p2->p_addr->u_pcb.pcb_hw.apcb_usp = (u_long)stack + stacksize;
	else
		p2->p_addr->u_pcb.pcb_hw.apcb_usp = alpha_pal_rdusp();
	simple_lock_init(&p2->p_addr->u_pcb.pcb_fpcpu_slock);

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
		memcpy(p2->p_md.md_tf, p1->p_md.md_tf,
		    sizeof(struct trapframe));

		/*
		 * Set up return-value registers as fork() libc stub expects.
		 */
		p2tf->tf_regs[FRAME_V0] = p1->p_pid;	/* parent's pid */
		p2tf->tf_regs[FRAME_A3] = 0;		/* no error */
		p2tf->tf_regs[FRAME_A4] = 1;		/* is child */

		up->u_pcb.pcb_hw.apcb_ksp = (u_int64_t)p2tf;	
		up->u_pcb.pcb_context[0] =
		    (u_int64_t)func;			/* s0: pc */
		up->u_pcb.pcb_context[1] =
		    (u_int64_t)exception_return;	/* s1: ra */
		up->u_pcb.pcb_context[2] =
		    (u_int64_t)arg;			/* s2: arg */
		up->u_pcb.pcb_context[7] =
		    (u_int64_t)proc_trampoline;		/* ra: assembly magic */
		up->u_pcb.pcb_context[8] = ALPHA_PSL_IPL_0; /* ps: IPL */
	}
}

/*
 * Finish a swapin operation.
 *
 * We need to cache the physical address of the PCB, so we can
 * swap context to it easily.
 */
void
cpu_swapin(struct proc *p)
{
	struct user *up = p->p_addr;

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
cpu_swapout(struct proc *p)
{

	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 1);
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
pagemove(caddr_t from, caddr_t to, size_t size)
{
	long fidx, tidx;
	ssize_t todo;
	PMAP_TLB_SHOOTDOWN_CPUSET_DECL

	if (size % NBPG)
		panic("pagemove");

	todo = size;			/* if testing > 0, need sign... */
	while (todo > 0) {
		fidx = VPT_INDEX(from);
		tidx = VPT_INDEX(to);

		VPT[tidx] = VPT[fidx];
		VPT[fidx] = 0;

		ALPHA_TBIS((vaddr_t)from);
		ALPHA_TBIS((vaddr_t)to);

		PMAP_TLB_SHOOTDOWN(pmap_kernel(), (vaddr_t)from, PG_ASM);
		PMAP_TLB_SHOOTDOWN(pmap_kernel(), (vaddr_t)to, PG_ASM);

		todo -= NBPG;
		from += NBPG;
		to += NBPG;
	}

	PMAP_TLB_SHOOTNOW();
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
	paddr_t pa;
	struct proc *p;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	p = bp->b_proc;
	faddr = trunc_page((vaddr_t)bp->b_saveaddr = bp->b_data);
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
		    VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}
	pmap_update();
}

/*
 * Unmap a previously-mapped user I/O request.
 */
void
vunmapbuf(struct buf *bp, vsize_t len)
{
	vaddr_t addr, off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	addr = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - addr;
	len = round_page(off + len);
	uvm_km_free_wakeup(phys_map, addr, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}
