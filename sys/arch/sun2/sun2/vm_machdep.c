/*	$NetBSD: vm_machdep.c,v 1.5.4.5 2002/02/28 04:12:21 nathanw Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *	from: @(#)vm_machdep.c	8.6 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/pte.h>
#include <machine/pmap.h>

#include <sun2/sun2/machdep.h>

extern void proc_do_uret __P((void));
extern void proc_trampoline __P((void));

/* XXX MAKE THIS LIKE OTHER M68K PORTS! */
static void cpu_set_kpc __P((struct lwp *, void (*)(void *), void *));

/*
 * Finish a fork operation, with process l2 nearly set up.
 * Copy and update the pcb and trap frame, making the child ready to run.
 * 
 * Rig the child's kernel stack so that it will start out in
 * proc_do_uret() and call child_return() with l2 as an
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
cpu_lwp_fork(l1, l2, stack, stacksize, func, arg)
	struct lwp *l1, *l2;
	void *stack;
	size_t stacksize;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *p1pcb = &l1->l_addr->u_pcb;
	struct pcb *p2pcb = &l2->l_addr->u_pcb;
	struct trapframe *p2tf;
	struct switchframe *p2sf;

	/*
	 * Before copying the PCB from the current process,
	 * make sure it is up-to-date.  (l1 == curproc)
	 */
	if (l1 == curproc)
		savectx(p1pcb);
#ifdef DIAGNOSTIC
	else if (l1 != &lwp0)
		panic("cpu_lwp_fork: curproc");
#endif

	/* copy over the machdep part of struct proc */
	l2->l_md.md_flags = l1->l_md.md_flags;

	/* Copy pcb from proc p1 to p2. */
	memcpy(p2pcb, p1pcb, sizeof(*p2pcb));

	/* Child can start with low IPL (XXX - right?) */
	p2pcb->pcb_ps = PSL_LOWIPL;

	/*
	 * Our cpu_switch MUST always call PMAP_ACTIVATE on a
	 * process switch so there is no need to do it here.
	 * (Our PMAP_ACTIVATE call allocates an MMU context.)
	 */

	/*
	 * Create the child's kernel stack, from scratch.
	 * Pick a stack pointer, leaving room for a trapframe;
	 * copy trapframe from parent so return to user mode
	 * will be to right address, with correct registers.
	 * Leave one word unused at the end of the kernel stack
	 * so the system stack pointer stays within the page.
	 */
	p2tf = (struct trapframe *)((char*)p2pcb + USPACE-4) - 1;
	l2->l_md.md_regs = (int *)p2tf;
	memcpy(p2tf, l1->l_md.md_regs, sizeof(*p2tf));

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		p2tf->tf_regs[15] = (u_int)stack + stacksize;

	/*
	 * Create a "switch frame" such that when cpu_switch returns,
	 * this process will be in proc_do_uret() going to user mode.
	 */
	p2sf = (struct switchframe *)p2tf - 1;
	p2sf->sf_pc = (u_int)proc_do_uret;
	p2pcb->pcb_regs[11] = (int)p2sf;		/* SSP */

	/*
	 * This will "push a call" to an arbitrary kernel function
	 * onto the stack of l2, very much like signal delivery.
	 * When l2 runs, it will find itself in child_return().
	 */
	cpu_set_kpc(l2, func, arg);
}

void
cpu_setfunc(l, func, arg)
	struct lwp *l;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	struct trapframe *tf = (struct trapframe *)l->l_md.md_regs;
	struct switchframe *sf = (struct switchframe *)tf - 1;

	sf->sf_pc = (int)proc_do_uret;
	pcb->pcb_regs[11] = (int)sf;		/* SSP */
	cpu_set_kpc(l, func, arg);
}

/*
 * cpu_set_kpc:
 *
 * Arrange for in-kernel execution of an lwp to continue in the
 * named function, as if that function were called with the supplied
 * argument.
 *
 * Note that it's assumed that when the named lwp returns,
 * rei() should be invoked, to return to user mode.  That is
 * accomplished by having cpu_lwp_fork set the initial frame with a
 * return address pointing to proc_do_uret() which does the rte.
 *
 * The design allows this function to be implemented as a general
 * "kernel sendsig" utility, that can "push" a call to a kernel
 * function onto any other process kernel stack, in a way very
 * similar to how signal delivery works on a user stack.  When
 * the named process is switched to, it will call the function
 * we "pushed" and then proc_trampoline will pop the args that
 * were pushed here and return to where it would have returned
 * before we "pushed" this call.
 */
static void
cpu_set_kpc(l, func, arg)
	struct lwp *l;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *pcbp;
	struct ksigframe {
		struct switchframe sf;
		void (*func) __P((void *));
		void *arg;
	} *ksfp;

	pcbp = &l->l_addr->u_pcb;

	/* Push a ksig frame onto the kernel stack. */
	ksfp = (struct ksigframe *)pcbp->pcb_regs[11] - 1;
	pcbp->pcb_regs[11] = (int)ksfp;

	/* Now fill it in for proc_trampoline. */
	ksfp->sf.sf_pc = (u_int)proc_trampoline;
	ksfp->func = func;
	ksfp->arg  = arg;
}

/*
 * cpu_exit is called as the last action during exit.
 *
 * Block context switches and then call switch_exit() which will
 * switch to another process thus we never return.
 */
void
cpu_exit(l, proc)
	struct lwp *l;
	int proc;
{

	(void) splhigh();
	uvmexp.swtch++;
	if (proc)
		switch_exit(l);
	else
		switch_lwp_exit(l);
	/* NOTREACHED */
}

/*
 * Do any additional state-saving necessary before swapout.
 */
void
cpu_swapout(p)
	struct proc *p;
{

	/*
	 * This will have real work to do when we implement the
	 * context-switch optimization of not switching FPU state
	 * until the new process actually uses FPU instructions.
	 */
}

/*
 * Do any additional state-restoration after swapin.
 * The pcb will be at the same KVA, but it may now
 * reside in different physical pages.
 */
void
cpu_swapin(p)
	struct proc *p;
{

	/*
	 * XXX - Just for debugging... (later).
	 */
}

/*
 * Dump the machine specific segment at the start of a core dump.
 * This means the CPU and FPU registers.  The format used here is
 * the same one ptrace uses, so gdb can be machine independent.
 *
 * XXX - Generate Sun format core dumps for Sun executables?
 */
struct md_core {
	struct reg intreg;
	struct fpreg freg;
};
int
cpu_coredump(l, vp, cred, chdr)
	struct lwp *l;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct proc *p = l->l_proc;
	struct md_core md_core;
	struct coreseg cseg;
	int error;

	/* XXX: Make sure savectx() was done? */

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Save integer registers. */
	error = process_read_regs(l, &md_core.intreg);
	if (error)
		return error;

	/* Save floating point registers. */
	error = process_read_fpregs(l, &md_core.freg);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
	if (error)
		return error;

	chdr->c_nseg++;
	return (0);
}

/*
 * Move pages from one kernel virtual address to another.
 * Both addresses are assumed to reside in the kernel map,
 * and size must be a multiple of NBPG.
 */
void
pagemove(from, to, len)
	caddr_t from, to;
	size_t len;
{
	struct pmap *kpmap = vm_map_pmap(kernel_map);
	vm_prot_t prot = VM_PROT_READ|VM_PROT_WRITE;
	vaddr_t fva = (vaddr_t)from;
	vaddr_t tva = (vaddr_t)to;
	paddr_t pa;
	boolean_t rv;

#ifdef DEBUG
	if (len & PGOFSET)
		panic("pagemove");
#endif
	while (len > 0) {
		rv = pmap_extract(kpmap, fva, &pa);
#ifdef DEBUG
		if (rv == FALSE)
			panic("pagemove 2");
		if (pmap_extract(kpmap, tva, NULL) == TRUE)
			panic("pagemove 3");
#endif
		/* pmap_kremove does the necessary cache flush.*/
		pmap_kremove(fva, NBPG);
		pmap_kenter_pa(tva, pa, prot);
		fva += NBPG;
		tva += NBPG;
		len -= NBPG;
	}
	pmap_update(pmap_kernel());
}

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
	struct pmap *upmap, *kpmap;
	vaddr_t uva;	/* User VA (map from) */
	vaddr_t kva;	/* Kernel VA (new to) */
	paddr_t pa; 	/* physical address */
	vsize_t off;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	/*
	 * XXX:  It might be better to round/trunc to a
	 * segment boundary to avoid VAC problems! -gwr
	 */
	bp->b_saveaddr = bp->b_data;
	uva = m68k_trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	len = m68k_round_page(off + len);
	kva = uvm_km_valloc_wait(kernel_map, len);
	bp->b_data = (caddr_t)(kva + off);

	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	kpmap = vm_map_pmap(kernel_map);
	do {
		if (pmap_extract(upmap, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
#ifdef	HAVECACHE
		/* Flush write-back cache on old mappings. */
		if (cache_size)
			cache_flush_page(uva);
#endif
		/* Now map the page into kernel space. */
		pmap_enter(kpmap, kva, pa | PMAP_NC,
		    VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
		uva += NBPG;
		kva += NBPG;
		len -= NBPG;
	} while (len);
	pmap_update(kpmap);
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

	kva = m68k_trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	len = m68k_round_page(off + len);
	pmap_remove(vm_map_pmap(kernel_map), kva, kva + len);
	pmap_update(vm_map_pmap(kernel_map));
	uvm_km_free_wakeup(kernel_map, kva, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

