/*	$NetBSD: vm_machdep.c,v 1.3.2.1 1998/07/30 14:03:57 eeh Exp $ */

/*
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

#include "opt_uvm.h"

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

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/bus.h>

#include <sparc64/sparc64/cache.h>

/* XXX These are in sbusvar.h, but including that would be problematical */
struct sbus_softc *sbus0;
void    sbus_enter __P((struct sbus_softc *, vaddr_t va, int64_t pa, int flags));
void    sbus_remove __P((struct sbus_softc *, vaddr_t va, int len));

/*
 * Move pages from one kernel virtual address to another.
 */
void
pagemove(from, to, size)
	register caddr_t from, to;
	size_t size;
{
	register paddr_t pa;

	if (size & CLOFSET || (int)from & CLOFSET || (int)to & CLOFSET)
		panic("pagemove 1");
#if 1
	cache_flush((caddr_t)from, size);
#endif
	while (size > 0) {
		pa = pmap_extract(pmap_kernel(), (vaddr_t)from);
		if (pa == 0)
			panic("pagemove 2");
		pmap_remove(pmap_kernel(),
		    (vaddr_t)from, (vaddr_t)from + PAGE_SIZE);
		pmap_enter(pmap_kernel(),
		    (vaddr_t)to, pa, VM_PROT_READ|VM_PROT_WRITE, 1);
		from += PAGE_SIZE;
		to += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
#if 1
	cache_flush((caddr_t)to, size);
#endif
}

/*
 * Wrapper for dvma_mapin() in kernel space,
 * so drivers need not include VM goo to get at kernel_map.
 */
caddr_t
kdvma_mapin(va, len, canwait)
	caddr_t	va;
	int	len, canwait;
{
	return ((caddr_t)dvma_mapin(kernel_map, (vaddr_t)va, len, canwait));
}

caddr_t
dvma_malloc(len, kaddr, flags)
	size_t	len;
	void	*kaddr;
	int	flags;
{
	vaddr_t	kva;
	vaddr_t	dva;
#if defined(SUN4M)
	extern int has_iocache;
#endif

	len = round_page(len);
	kva = (vaddr_t)malloc(len, M_DEVBUF, flags);
	if (kva == NULL)
		return (NULL);

	*(vaddr_t *)kaddr = kva;
	dva = dvma_mapin(kernel_map, kva, len, (flags & M_NOWAIT) ? 0 : 1);
	if (dva == NULL) {
		free((void *)kva, M_DEVBUF);
		return (NULL);
	}
	return (caddr_t)dva;
}

void
dvma_free(dva, len, kaddr)
	caddr_t	dva;
	size_t	len;
	void	*kaddr;
{
	vaddr_t	kva = *(vaddr_t *)kaddr;

	dvma_mapout((vaddr_t)dva, kva, round_page(len));
	free((void *)kva, M_DEVBUF);
}

u_long dvma_cachealign = 0;

/*
 * Map a range [va, va+len] of wired virtual addresses in the given map
 * to a kernel address in DVMA space.
 */
vaddr_t
dvma_mapin(map, va, len, canwait)
	struct vm_map	*map;
	vaddr_t	va;
	int		len, canwait;
{
	vaddr_t	kva, tva;
	register int npf, s;
	register paddr_t pa;
	long off, pn;
	vaddr_t	ova;
	int		olen;

	ova = va;
	olen = len;

#ifdef DEBUG_1
	printf("dvma_mapin: va %x len %x\n", va, len);
#endif
	off = (int)va & PGOFSET;
	va -= off;
	len = round_page(len + off);
	npf = btoc(len);

	s = splimp();
	for (;;) {

		if (dvma_cachealign) {
			int m = (dvma_cachealign >> PGSHIFT);
			int palign = (va >> PGSHIFT) & (m - 1);
			long basepn;

			/*
			 * Find a DVMA address that's congruent to VA
			 * modulo the cache size.  This needs only be
			 * done on machines with virtually indexed
			 * caches capable of DVMA coherency (eg. Hypersparc)
			 *
			 * XXX - there should be a better way..
			 */
			basepn = pn = rmalloc(dvmamap, npf + m - 1);
			if (pn != 0) {
				pn += (palign + 1 + m - pn) & (m - 1);

				/* Free excess resources */
				if (pn != basepn)
					rmfree(dvmamap, pn - basepn, basepn);
				if (pn != basepn + (m - 1))
					rmfree(dvmamap, m - 1 - (pn - basepn),
						pn + npf);
				break;
			}

		} else {
			pn = rmalloc(dvmamap, npf);
			if (pn != 0)
				break;
		}
		if (canwait) {
			(void)tsleep(dvmamap, PRIBIO+1, "physio", 0);
			continue;
		}
		splx(s);
		return NULL;
	}
	splx(s);

#if 0
	/* Sync memory -- should not be needed */
	cache_flush((caddr_t)va, len);
#endif

	kva = tva = rctov(pn);

	while (npf--) {
		pa = pmap_extract(vm_map_pmap(map), va);
		if (pa == 0)
			panic("dvma_mapin: null page frame");
		pa = trunc_page(pa);

		/* Standard RAM is cached non-coherent writeable */
		sbus_enter(sbus0, tva, pa, BUS_DMA_WAITOK);

		tva += PAGE_SIZE;
		va += PAGE_SIZE;
	}
#if 0
	/*
	 * XXX Only have to do this on write.
	 */
	if (CACHEINFO.c_vactype == VAC_WRITEBACK)	/* XXX */
		cpuinfo.cache_flush((caddr_t)ova, olen);	/* XXX */
#endif
	return kva + off;
}

/*
 * Remove double map of `va' in DVMA space at `kva'.
 */
void
dvma_mapout(kva, va, len)
	vaddr_t	kva, va;
	int		len;
{
	register int s, off;

#ifdef DEBUG_1
	printf("dvma_mapout: kva %x va %x len %x\n", kva, va, len);
#endif

	off = (int)kva & PGOFSET;
	kva -= off;
	len = round_page(len + off);

	sbus_remove(sbus0, kva, len);
	
	s = splimp();
	rmfree(dvmamap, btoc(len), vtorc(kva));
	wakeup(dvmamap);
	splx(s);

	cache_flush((caddr_t)va, len);
}

/*
 * Map an IO request into kernel virtual address space.
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
	uva = trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - uva;
	len = round_page(off + len);
#if defined(UVM)
	kva = uvm_km_valloc_wait(kernel_map, len);
#else
	kva = kmem_alloc_wait(kernel_map, len);
#endif
	bp->b_data = (caddr_t)(kva + off);

	/*
	 * We have to flush any write-back cache on the
	 * user-space mappings so our new mappings will
	 * have the correct contents.
	 */
	cache_flush((caddr_t)uva, len);

	upmap = vm_map_pmap(&bp->b_proc->p_vmspace->vm_map);
	kpmap = vm_map_pmap(kernel_map);
	do {
		pa = pmap_extract(upmap, uva);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		/* Now map the page into kernel space. */
		pmap_enter(pmap_kernel(), kva,
			   pa /* | PMAP_NC */,
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);

		uva += PAGE_SIZE;
		kva += PAGE_SIZE;
		len -= PAGE_SIZE;
	} while (len);
}

/*
 * Free the mappings associated with this I/O operation.
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

	kva = trunc_page(bp->b_data);
	off = (vaddr_t)bp->b_data - kva;
	len = round_page(off + len);

	/* This will call pmap_remove() for us. */
#if defined(UVM)
	uvm_km_free_wakeup(kernel_map, kva, len);
#else
	kmem_free_wakeup(kernel_map, kva, len);
#endif
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
#define	TOPFRAMEOFF (USPACE-sizeof(struct trapframe)-sizeof(struct frame))

#ifdef DEBUG
char cpu_forkname[] = "cpu_fork()";
#endif

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, making the child ready to run, and marking
 * it so that it can return differently than the parent.
 *
 * This function relies on the fact that the pcb is
 * the first element in struct user.
 */
void
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct pcb *opcb = &p1->p_addr->u_pcb;
	register struct pcb *npcb = &p2->p_addr->u_pcb;
	register struct trapframe *tf2;
	register struct rwindow32 *rp;

	/*
	 * Save all user registers to p1's stack or, in the case of
	 * user registers and invalid stack pointers, to opcb.
	 * We then copy the whole pcb to p2; when switch() selects p2
	 * to run, it will run at the `proc_trampoline' stub, rather
	 * than returning at the copying code below.
	 *
	 * If process p1 has an FPU state, we must copy it.  If it is
	 * the FPU user, we must save the FPU state first.
	 */

#ifdef NOTDEF_DEBUG
	printf("cpu_fork()\n");
#endif
	write_user_windows();
#if 0
	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();
	/* 
	 * We should not need to copy this out cause we should be able to
	 * directly reload our windows from the pcb.
	 */
	rwindow_save(p1);
#endif

	/*
	 * We're in the kernel, so we don't really care about %ccr or %asi.
	 * We do want to duplicate %pstate and %cwp.  */
	opcb->pcb_pstate = getpstate();
	opcb->pcb_cwp = getcwp();
#ifdef DEBUG
	/* prevent us from having NULL lastcall */
	opcb->lastcall = cpu_forkname;
#endif
	bcopy((caddr_t)opcb, (caddr_t)npcb, sizeof(struct pcb));
       	if (p1->p_md.md_fpstate) {
		if (p1 == fpproc)
			savefpstate(p1->p_md.md_fpstate);
		p2->p_md.md_fpstate = malloc(sizeof(struct fpstate),
		    M_SUBPROC, M_WAITOK);
		bcopy(p1->p_md.md_fpstate, p2->p_md.md_fpstate,
		    sizeof(struct fpstate));
	} else
		p2->p_md.md_fpstate = NULL;

	/*
	 * Setup (kernel) stack frame that will by-pass the child
	 * out of the kernel. (The trap frame invariably resides at
	 * the tippity-top of the u. area.)
	 */
	tf2 = p2->p_md.md_tf = (struct trapframe *)
			((int)npcb + USPACE - sizeof(*tf2));

	/* Copy parent's trapframe */
	*tf2 = *(struct trapframe *)((int)opcb + USPACE - sizeof(*tf2));

	/* Duplicate efforts of syscall(), but slightly differently */
	if (tf2->tf_global[1] & SYSCALL_G2RFLAG) {
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

#ifdef DEBUG
	/* Need to sync tf locals and ins with stack to prevent panic */
	{
		int i;

		rp = (struct rwindow32 *)tf2->tf_out[6];
		for (i=0; i<8; i++) {
			tf2->tf_local[i] = fuword(&rp->rw_local[i]);
			tf2->tf_in[i] = fuword(&rp->rw_in[i]);
		}
	}
#endif
	/* Construct kernel frame to return to in cpu_switch() */
	rp = (struct rwindow32 *)((u_int)npcb + TOPFRAMEOFF);
	*rp = *(struct rwindow32 *)((u_int)opcb + TOPFRAMEOFF);
	rp->rw_local[0] = (int)child_return;	/* Function to call */
	rp->rw_local[1] = (int)p2;		/* and its argument */

	npcb->pcb_pc = (int)proc_trampoline - 8;
	npcb->pcb_sp = (int)rp;

#ifdef NOTDEF_DEBUG
	printf("cpu_fork: Copying over trapframe: otf=%p ntf=%p sp=%p opcb=%p npcb=%p\n", 
	       (struct trapframe *)((int)opcb + USPACE - sizeof(*tf2)), tf2, rp, opcb, npcb);
	printf("cpu_fork: tstate=%x:%x pc=%x:%x npc=%x:%x rsp=%x\n",
	       (long)(tf2->tf_tstate>>32), (long)tf2->tf_tstate, 
	       (long)(tf2->tf_pc>>32), (long)tf2->tf_pc,
	       (long)(tf2->tf_npc>>32), (long)tf2->tf_npc, 
	       (long)(tf2->tf_out[6]));
	Debugger();
#endif
}

/*
 * cpu_set_kpc:
 *
 * Arrange for in-kernel execution of a process to continue at the
 * named pc, as if the code at that address were called as a function
 * with the current process's process pointer as an argument.
 *
 * Note that it's assumed that when the named process returns,
 * we immediately return to user mode.
 *
 * (Note that cpu_fork(), above, uses an open-coded version of this.)
 */
void
cpu_set_kpc(p, pc)
	struct proc *p;
	void (*pc) __P((struct proc *));
{
	struct pcb *pcb;
	struct rwindow32 *rp;

#if 0
	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();
#endif

	pcb = &p->p_addr->u_pcb;

	rp = (struct rwindow32 *)((u_int)pcb + TOPFRAMEOFF);
	rp->rw_local[0] = (int)pc;		/* Function to call */
	rp->rw_local[1] = (int)p;		/* and its argument */

#ifdef NOTDEF_DEBUG
	/* Let's see if this is ever called */
	{ int s=splhigh();
	extern int pmapdebug;
	pmapdebug = 0;
	printf("cpu_set_kpc: p=%x pc=%x, sp=%x rsp=%x\n", p, pc, rp, rp->rw_in[6]);
	splx(s);
	delay(2000000);
	}
#endif
	/*
	 * Frob PCB:
	 *	- arrange to return to proc_trampoline() from cpu_switch()
	 *	- point it at the stack frame constructed above
	 *	- make it run in a clear set of register windows
	 */
	pcb->pcb_pc = (int)proc_trampoline - 8 ;
	pcb->pcb_sp = (int)rp;
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Since the latter is also the interrupt stack, we release it
 * from assembly code after switching to a temporary pcb+stack.
 */
void
cpu_exit(p)
	struct proc *p;
{
	register struct fpstate *fs;

	if ((fs = p->p_md.md_fpstate) != NULL) {
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
	}
#if defined(UVM)
	uvmspace_free(p->p_vmspace);
#else
	vmspace_free(p->p_vmspace);
#endif
	switchexit(kernel_map, p->p_addr, USPACE);
	/* NOTREACHED */
}

/*
 * cpu_coredump is called to write a core dump header.
 * (should this be defined elsewhere?  machdep.c?)
 */
int
cpu_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	int i, error;
	struct md_coredump md_core;
	struct coreseg cseg;

	CORE_SETMAGIC(*chdr, COREMAGIC, MID_SPARC, 0);
	chdr->c_hdrsize = ALIGN(sizeof(*chdr));
	chdr->c_seghdrsize = ALIGN(sizeof(cseg));
	chdr->c_cpusize = sizeof(md_core);

#if 0
	md_core.md_tf = *p->p_md.md_tf;
#else
	/* Until we get 64-bit executables we need to fake a v8 trapframe */
	md_core.md_tf.tf_psr = TSTATECCR_TO_PSR(p->p_md.md_tf->tf_tstate);
	md_core.md_tf.tf_pc = p->p_md.md_tf->tf_pc;
	md_core.md_tf.tf_npc = p->p_md.md_tf->tf_npc;
	md_core.md_tf.tf_y = p->p_md.md_tf->tf_y;
	for (i=0; i<8; i++) {
		struct rwindow32 *rw = (struct rwindow32 *)p->p_md.md_tf->tf_out[6];

		md_core.md_tf.tf_global[i] = p->p_md.md_tf->tf_global[i];
		md_core.md_tf.tf_out[i] = p->p_md.md_tf->tf_out[i];
	}
#endif
	if (p->p_md.md_fpstate) {
		if (p == fpproc)
			savefpstate(p->p_md.md_fpstate);
		md_core.md_fpstate = *p->p_md.md_fpstate;
	} else
		bzero((caddr_t)&md_core.md_fpstate, sizeof(struct fpstate));

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_SPARC, CORE_CPU);
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
	if (!error)
		chdr->c_nseg++;

	return error;
}
