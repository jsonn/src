/*	$NetBSD: vm_machdep.c,v 1.41.4.10 2002/07/12 01:39:50 nathanw Exp $ */

/*
 * Copyright (c) 1996-2002 Eduardo Horvath.  All rights reserved.
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by Harvard University.
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
 *	@(#)vm_machdep.c	8.2 (Berkeley) 9/23/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/map.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/bus.h>

#include <sparc64/sparc64/cache.h>

#if 0
/* XXX These are in sbusvar.h, but including that would be problematical */
struct sbus_softc *sbus0;
void    sbus_enter __P((struct sbus_softc *, vaddr_t va, int64_t pa, int flags));
void    sbus_remove __P((struct sbus_softc *, vaddr_t va, int len));
#endif

/*
 * Move pages from one kernel virtual address to another.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	paddr_t pa;

	if (size & PGOFSET || (long)from & PGOFSET || (long)to & PGOFSET)
		panic("pagemove 1");

	while (size > 0) {
		if (pmap_extract(pmap_kernel(), (vaddr_t)from, &pa) == FALSE)
			panic("pagemove 2");
		pmap_kremove((vaddr_t)from, PAGE_SIZE);
		pmap_kenter_pa((vaddr_t)to, pa, VM_PROT_READ | VM_PROT_WRITE);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
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
	 * segment boundary to avoid VAC problems!
	 */
	bp->b_saveaddr = bp->b_data;
	uva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	len = round_page(off + len);
	kva = uvm_km_valloc_wait(kernel_map, len);
	bp->b_data = (caddr_t)(kva + off);

	/*
	 * We have to flush any write-back cache on the
	 * user-space mappings so our new mappings will
	 * have the correct contents.
	 */
	cache_flush(uva, len);

	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	kpmap = vm_map_pmap(kernel_map);
	do {
		if (pmap_extract(upmap, uva, &pa) == FALSE)
			panic("vmapbuf: null page frame");
		/* Now map the page into kernel space. */
		pmap_enter(pmap_kernel(), kva,
			pa /* | PMAP_NC */,
			VM_PROT_READ|VM_PROT_WRITE,
			VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);

		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	} while (len);
	pmap_update(pmap_kernel());
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

	kva = trunc_page((vaddr_t)bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	len = round_page(off + len);
	pmap_remove(pmap_kernel(), kva, kva + len);
	uvm_km_free_wakeup(kernel_map, kva, len);
	bp->b_data = bp->b_saveaddr;
	bp->b_saveaddr = NULL;

#if 0	/* XXX: The flush above is sufficient, right? */
	if (CACHEINFO.c_vactype != VAC_NONE)
		cpuinfo.cache_flush(bp->b_data, len);
#endif
}


/*
 * The offset of the topmost frame in the kernel stack.
 */
#ifdef __arch64__
#define	TOPFRAMEOFF (USPACE-sizeof(struct trapframe)-CC64FSZ)
#define	STACK_OFFSET	BIAS
#else
#undef	trapframe
#define	trapframe	trapframe64
#undef	rwindow
#define	rwindow		rwindow32
#define	TOPFRAMEOFF (USPACE-sizeof(struct trapframe)-CC64FSZ)
#define	STACK_OFFSET	0
#endif

#ifdef DEBUG
char cpu_forkname[] = "cpu_lwp_fork()";
#endif

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
	register struct lwp *l1, *l2;
	void *stack;
	size_t stacksize;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *opcb = &l1->l_addr->u_pcb;
	struct pcb *npcb = &l2->l_addr->u_pcb;
	struct trapframe *tf2;
	struct rwindow *rp;
	extern struct lwp lwp0;

	/*
	 * Save all user registers to l1's stack or, in the case of
	 * user registers and invalid stack pointers, to opcb.
	 * We then copy the whole pcb to l2; when switch() selects l2
	 * to run, it will run at the `proc_trampoline' stub, rather
	 * than returning at the copying code below.
	 *
	 * If process l1 has an FPU state, we must copy it.  If it is
	 * the FPU user, we must save the FPU state first.
	 */

#ifdef NOTDEF_DEBUG
	printf("cpu_lwp_fork()\n");
#endif
	if (l1 == curlwp) {
		write_user_windows();

		/*
		 * We're in the kernel, so we don't really care about
		 * %ccr or %asi.  We do want to duplicate %pstate and %cwp.
		 */
		opcb->pcb_pstate = getpstate();
		opcb->pcb_cwp = getcwp();
	}
#ifdef DIAGNOSTIC
	else if (l1 != &lwp0)
		panic("cpu_lwp_fork: curlwp");
#endif
#ifdef DEBUG
	/* prevent us from having NULL lastcall */
	opcb->lastcall = cpu_forkname;
#else
	opcb->lastcall = NULL;
#endif
	bcopy((caddr_t)opcb, (caddr_t)npcb, sizeof(struct pcb));
       	if (l1->l_md.md_fpstate) {
		if (l1 == fplwp) {
			savefpstate(l1->l_md.md_fpstate);
			fplwp = NULL;
		}
		l2->l_md.md_fpstate = malloc(sizeof(struct fpstate64),
		    M_SUBPROC, M_WAITOK);
		bcopy(l1->l_md.md_fpstate, l2->l_md.md_fpstate,
		    sizeof(struct fpstate64));
	} else
		l2->l_md.md_fpstate = NULL;

	if (p1->p_flag & P_32)
		p2->p_flag |= P_32;

	/*
	 * Setup (kernel) stack frame that will by-pass the child
	 * out of the kernel. (The trap frame invariably resides at
	 * the tippity-top of the u. area.)
	 */
	tf2 = l2->l_md.md_tf = (struct trapframe *)
			((long)npcb + USPACE - sizeof(*tf2));

	/* Copy parent's trapframe */
	*tf2 = *(struct trapframe *)((long)opcb + USPACE - sizeof(*tf2));

	/*
	 * If specified, give the child a different stack.
	 */
	if (stack != NULL)
		tf2->tf_out[6] = (u_int64_t)(u_long)stack + stacksize;

	/* Duplicate efforts of syscall(), but slightly differently */
	if (tf2->tf_global[1] & SYSCALL_G7RFLAG) {
		/* jmp %g2 (or %g7, deprecated) on success */
		tf2->tf_npc = tf2->tf_global[7];
	} else if (tf2->tf_global[1] & SYSCALL_G2RFLAG) {
		/* jmp %g2 (or %g7, deprecated) on success */
		tf2->tf_npc = tf2->tf_global[2];
	} else {
		/*
		 * old system call convention: clear C on success
		 * note: proc_trampoline() sets a fresh psr when
		 * returning to user mode.
		 */
		/*tf2->tf_psr &= ~PSR_C;   -* success */
	}

	/* Set return values in child mode */
	tf2->tf_out[0] = 0;
	tf2->tf_out[1] = 1;

	/* Construct kernel frame to return to in cpu_switch() */
	rp = (struct rwindow *)((u_long)npcb + TOPFRAMEOFF);
	*rp = *(struct rwindow *)((u_long)opcb + TOPFRAMEOFF);
	rp->rw_local[0] = (long)func;		/* Function to call */
	rp->rw_local[1] = (long)arg;		/* and its argument */

	npcb->pcb_pc = (long)proc_trampoline - 8;
	npcb->pcb_sp = (long)rp - STACK_OFFSET;
	/* Need to create a %tstate if we're forking from proc0 */
	if (l1 == &lwp0)
		tf2->tf_tstate = (ASI_PRIMARY_NO_FAULT<<TSTATE_ASI_SHIFT) |
			((PSTATE_USER)<<TSTATE_PSTATE_SHIFT);


#ifdef NOTDEF_DEBUG
	printf("cpu_lwp_fork: Copying over trapframe: otf=%p ntf=%p sp=%p opcb=%p npcb=%p\n", 
	       (struct trapframe *)((int)opcb + USPACE - sizeof(*tf2)), tf2, rp, opcb, npcb);
	printf("cpu_lwp_fork: tstate=%x:%x pc=%x:%x npc=%x:%x rsp=%x\n",
	       (long)(tf2->tf_tstate>>32), (long)tf2->tf_tstate, 
	       (long)(tf2->tf_pc>>32), (long)tf2->tf_pc,
	       (long)(tf2->tf_npc>>32), (long)tf2->tf_npc, 
	       (long)(tf2->tf_out[6]));
	Debugger();
#endif
}

void
cpu_setfunc(l, func, arg)
	struct lwp *l;
	void (*func) __P((void *));
	void *arg;
{
	struct pcb *npcb = &l->l_addr->u_pcb;
	struct rwindow *rp;


	/* Construct kernel frame to return to in cpu_switch() */
	rp = (struct rwindow *)((u_long)npcb + TOPFRAMEOFF);
	rp->rw_local[0] = (long)func;		/* Function to call */
	rp->rw_local[1] = (long)arg;		/* and its argument */

	npcb->pcb_pc = (long)proc_trampoline - 8;
	npcb->pcb_sp = (long)rp - STACK_OFFSET;
}	

/*
 * cpu_exit is called as the last action during exit.
 *
 * We clean up a little and then call switchexit() with the old proc
 * as an argument.  switchexit() switches to the idle context, schedules
 * the old vmspace and stack to be freed, then selects a new process to
 * run.
 */
void
cpu_exit(l, proc)
	struct lwp *l;
	int proc;
{
	register struct fpstate64 *fs;

	if ((fs = l->l_md.md_fpstate) != NULL) {
		if (l == fplwp) {
			savefpstate(fs);
			fplwp = NULL;
		}
		free((void *)fs, M_SUBPROC);
	}
	switchexit(l, proc);
	/* NOTREACHED */
}

/*
 * cpu_coredump is called to write a core dump header.
 * (should this be defined elsewhere?  machdep.c?)
 */
int
cpu_coredump(l, vp, cred, chdr)
	struct lwp *l;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int error;
	struct md_coredump md_core;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

	/* Copy important fields over. */
	md_core.md_tf.tf_tstate = p->p_md.md_tf->tf_tstate;
	md_core.md_tf.tf_pc = p->p_md.md_tf->tf_pc;
	md_core.md_tf.tf_npc = p->p_md.md_tf->tf_npc;
	md_core.md_tf.tf_y = p->p_md.md_tf->tf_y;
	md_core.md_tf.tf_tt = p->p_md.md_tf->tf_tt;
	md_core.md_tf.tf_pil = p->p_md.md_tf->tf_pil;
	md_core.md_tf.tf_oldpil = p->p_md.md_tf->tf_oldpil;

	md_core.md_tf.tf_global[0] = p->p_md.md_tf->tf_global[0];
	md_core.md_tf.tf_global[1] = p->p_md.md_tf->tf_global[1];
	md_core.md_tf.tf_global[2] = p->p_md.md_tf->tf_global[2];
	md_core.md_tf.tf_global[3] = p->p_md.md_tf->tf_global[3];
	md_core.md_tf.tf_global[4] = p->p_md.md_tf->tf_global[4];
	md_core.md_tf.tf_global[5] = p->p_md.md_tf->tf_global[5];
	md_core.md_tf.tf_global[6] = p->p_md.md_tf->tf_global[6];
	md_core.md_tf.tf_global[7] = p->p_md.md_tf->tf_global[7];

	md_core.md_tf.tf_out[0] = p->p_md.md_tf->tf_out[0];
	md_core.md_tf.tf_out[1] = p->p_md.md_tf->tf_out[1];
	md_core.md_tf.tf_out[2] = p->p_md.md_tf->tf_out[2];
	md_core.md_tf.tf_out[3] = p->p_md.md_tf->tf_out[3];
	md_core.md_tf.tf_out[4] = p->p_md.md_tf->tf_out[4];
	md_core.md_tf.tf_out[5] = p->p_md.md_tf->tf_out[5];
	md_core.md_tf.tf_out[6] = p->p_md.md_tf->tf_out[6];
	md_core.md_tf.tf_out[7] = p->p_md.md_tf->tf_out[7];

#ifdef DEBUG
	md_core.md_tf.tf_local[0] = p->p_md.md_tf->tf_local[0];
	md_core.md_tf.tf_local[1] = p->p_md.md_tf->tf_local[1];
	md_core.md_tf.tf_local[2] = p->p_md.md_tf->tf_local[2];
	md_core.md_tf.tf_local[3] = p->p_md.md_tf->tf_local[3];
	md_core.md_tf.tf_local[4] = p->p_md.md_tf->tf_local[4];
	md_core.md_tf.tf_local[5] = p->p_md.md_tf->tf_local[5];
	md_core.md_tf.tf_local[6] = p->p_md.md_tf->tf_local[6];
	md_core.md_tf.tf_local[7] = p->p_md.md_tf->tf_local[7];

	md_core.md_tf.tf_in[0] = p->p_md.md_tf->tf_in[0];
	md_core.md_tf.tf_in[1] = p->p_md.md_tf->tf_in[1];
	md_core.md_tf.tf_in[2] = p->p_md.md_tf->tf_in[2];
	md_core.md_tf.tf_in[3] = p->p_md.md_tf->tf_in[3];
	md_core.md_tf.tf_in[4] = p->p_md.md_tf->tf_in[4];
	md_core.md_tf.tf_in[5] = p->p_md.md_tf->tf_in[5];
	md_core.md_tf.tf_in[6] = p->p_md.md_tf->tf_in[6];
	md_core.md_tf.tf_in[7] = p->p_md.md_tf->tf_in[7];
#endif
	if (l->l_md.md_fpstate) {
		if (l == fplwp) {
			savefpstate(l->l_md.md_fpstate);
			fplwp = NULL;
		}
		md_core.md_fpstate = *l->l_md.md_fpstate;
	} else
		bzero((caddr_t)&md_core.md_fpstate, 
		      sizeof(md_core.md_fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&cseg, chdr->c_seghdrsize,
	    (off_t)chdr->c_hdrsize, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, l->l_proc);
	if (error)
		return error;

	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)&md_core, sizeof(md_core),
	    (off_t)(chdr->c_hdrsize + chdr->c_seghdrsize), UIO_SYSSPACE,
	    IO_NODELOCKED|IO_UNIT, cred, NULL, l->l_proc);
	if (!error)
		chdr->c_nseg++;

	return error;
}

