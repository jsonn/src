/*	$NetBSD: vm_machdep.c,v 1.7 1999/05/26 22:19:37 thorpej Exp $	*/

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
 *	from: @(#)vm_machdep.c	8.3 (Berkeley) 1/4/94
 */



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#if 0
#include <vm/vm_object.h>
#endif

#include <machine/pte.h>
#include <machine/cpu.h>

vm_offset_t kmem_alloc_wait_align();

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
	register pt_entry_t *pte;
	register int i;
	extern struct proc *machFPCurProcPtr;

	p2->p_md.md_regs = up->u_pcb.pcb_regs;
	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FPUSED;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switch() can quickly map in
	 * the user struct and kernel stack. Note: if the virtual address
	 * translation changes (e.g. swapout) we have to update this.
	 */
	pte = kvtopte(up);
	for (i = 0; i < UPAGES; i++) {
		p2->p_md.md_upte[i] = pte->pt_entry & ~(PG_G | PG_RO | PG_WIRED);
		pte++;
	}

	/*
	 * Copy floating point state from the FP chip if this process
	 * has state stored there.
	 */
	if (p1 == machFPCurProcPtr)
		MachSaveCurFPState(p1);

	/*
	 * Copy pcb and stack from proc p1 to p2. 
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	/* cache segtab for ULTBMiss() */
	p2->p_addr->u_pcb.pcb_segtab = (void *)p2->p_vmspace->vm_pmap.pm_segtab;

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	if (p1 != curproc && p1 != &proc0)
		panic("cpu_fork: curproc");
#endif
	if (copykstack(up)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
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
	register struct user *up = p->p_addr;
	register pt_entry_t *pte;
	register int i;

	/*
	 * Cache the PTEs for the user area in the machine dependent
	 * part of the proc struct so cpu_switch() can quickly map in
	 * the user struct and kernel stack.
	 */
	pte = kvtopte(up);
	for (i = 0; i < UPAGES; i++) {
		p->p_md.md_upte[i] = pte->pt_entry & ~(PG_G | PG_RO | PG_WIRED);
		pte++;
	}
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space and machine-dependent resources,
 * including the memory for the user structure and kernel stack.
 * Once finished, we call switch_exit, which switches to a temporary
 * pcb and stack and never returns.  We block memory allocation
 * until switch_exit has made things safe again.
 */
void cpu_exit(p)
	struct proc *p;
{
	extern struct proc *machFPCurProcPtr;

	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;

	vmspace_free(p->p_vmspace);

	(void) splhigh();
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
	switch_exit();
	/* NOTREACHED */
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
cpu_coredump(p, vp, cred)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
{
	extern struct proc *machFPCurProcPtr;

	/*
	 * Copy floating point state from the FP chip if this process
	 * has state stored there.
	 */
	if (p == machFPCurProcPtr)
		MachSaveCurFPState(p);

	return (vn_rdwr(UIO_WRITE, vp, (caddr_t)p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, NULL,
	    p));
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
	if(((int)from & machCacheAliasMask) != ((int)to & machCacheAliasMask)) {
		MachHitFlushDCache(from, size);
	}
	while (size > 0) {
		MachTLBFlushAddr(from);
		MachTLBUpdate(to, *fpte);
		*tpte++ = *fpte;
		fpte->pt_entry = PG_NV | PG_G;
		fpte++;
		size -= NBPG;
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
vmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr;
	register vm_size_t sz;
	struct proc *p;
	int off;
	vm_offset_t kva;
	register vm_offset_t pa;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");
	addr = bp->b_saveaddr = bp->b_un.b_addr;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	sz = round_page(bp->b_bcount + off);
	kva = kmem_alloc_wait_align(phys_map, sz, (vm_size_t)addr & machCacheAliasMask);
	bp->b_un.b_addr = (caddr_t) (kva + off);
	sz = atop(sz);
	while (sz--) {
		pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),
			(vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_enter(vm_map_pmap(phys_map), kva, trunc_page(pa),
		    VM_PROT_READ|VM_PROT_WRITE, TRUE, 0);
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
}

/*
 * Unmap a previously-mapped user I/O request.
 */
vunmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr = bp->b_un.b_addr;
	register vm_size_t sz;
	vm_offset_t kva;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	sz = round_page(bp->b_bcount + ((int)addr & PGOFSET));
	kva = (vm_offset_t)((int)addr & ~PGOFSET);
	kmem_free_wakeup(phys_map, kva, sz);
	bp->b_un.b_addr = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}


/*
 *	SAVE_HINT:
 *
 *	Saves the specified entry as the hint for
 *	future lookups.  Performs necessary interlocks.
 */
#define	SAVE_HINT(map,value) \
		simple_lock(&(map)->hint_lock); \
		(map)->hint = (value); \
		simple_unlock(&(map)->hint_lock);


/*
 *	kmem_alloc_upage:
 *
 *	Allocate pageable memory to the kernel's address map.
 *	map must be "kernel_map" below.
 *	(Currently only used when allocating U pages).
 */
vm_offset_t
kmem_alloc_upage(map, size)
	vm_map_t		map;
	register vm_size_t	size;
{
	vm_offset_t		addr;
	register int		result;


	size = round_page(size);

	addr = vm_map_min(map);
	result = vm_map_find_U(map, NULL, (vm_offset_t) 0,
				&addr, size, TRUE);
	if (result != KERN_SUCCESS) {
		return(0);
	}

	return(addr);
}

/*
 *	vm_map_find finds an unallocated region in the target address
 *	map with the given length aligned on U viritual address.
 *	The search is defined to be first-fit from the specified address;
 *	the region found is returned in the same parameter.
 *
 */
int
vm_map_find_U(map, object, offset, addr, length, find_space)
	vm_map_t	map;
	vm_object_t	object;
	vm_offset_t	offset;
	vm_offset_t	*addr;		/* IN/OUT */
	vm_size_t	length;
	boolean_t	find_space;
{
	register vm_offset_t	start;
	int			result;

	start = *addr;
	vm_map_lock(map);
	if (find_space) {
		if (vm_map_findspace_align(map, start, length, addr, 0)) {
			vm_map_unlock(map);
			return (KERN_NO_SPACE);
		}
		start = *addr;
	}
	result = vm_map_insert(map, object, offset, start, start + length);
	vm_map_unlock(map);
	return (result);
}

/*
 * Find sufficient space for `length' bytes in the given map, starting at
 * `start'.  The map must be locked.  Returns 0 on success, 1 on no space.
 */
int
vm_map_findspace_align(map, start, length, addr, align)
	register vm_map_t map;
	register vm_offset_t start;
	vm_size_t length;
	vm_offset_t *addr;
	vm_size_t align;
{
	register vm_map_entry_t entry, next;
	register vm_offset_t end;

	if (start < map->min_offset)
		start = map->min_offset;
	if (start > map->max_offset)
		return (1);

	/*
	 * Look for the first possible address; if there's already
	 * something at this address, we have to start after it.
	 */
	if (start == map->min_offset) {
		if ((entry = map->first_free) != &map->header)
			start = entry->end;
	} else {
		vm_map_entry_t tmp;
		if (vm_map_lookup_entry(map, start, &tmp))
			start = tmp->end;
		entry = tmp;
	}

	/*
	 * Look through the rest of the map, trying to fit a new region in
	 * the gap between existing regions, or after the very last region.
	 */
	for (;; start = (entry = next)->end) {
		/*
		 * Find the end of the proposed new region.  Be sure we didn't
		 * go beyond the end of the map, or wrap around the address;
		 * if so, we lose.  Otherwise, if this is the last entry, or
		 * if the proposed new region fits before the next entry, we
		 * win.
		 */
		start = ((start + NBPG -1) & ~(NBPG - 1)); /* Paranoia */
		if((start & machCacheAliasMask) <= align) {
			start += align - (start & machCacheAliasMask);
		}
		else {
			start = ((start + machCacheAliasMask) & ~machCacheAliasMask);
			start += align;
		}
			
		end = start + length;
		if (end > map->max_offset || end < start)
			return (1);
		next = entry->next;
		if (next == &map->header || next->start >= end)
			break;
	}
	SAVE_HINT(map, entry);
	*addr = start;
	return (0);
}

/*
 *	kmem_alloc_wait_align
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 */
vm_offset_t
kmem_alloc_wait_align(map, size, align)
	vm_map_t	map;
	vm_size_t	size;
	vm_size_t	align;
{
	vm_offset_t	addr;

	size = round_page(size);

	for (;;) {
		/*
		 * To make this work for more than one map,
		 * use the map's lock to lock out sleepers/wakers.
		 */
		vm_map_lock(map);
		if (vm_map_findspace_align(map, 0, size, &addr, align) == 0)
			break;
		/* no space now; see if we can ever get space */
		if (vm_map_max(map) - vm_map_min(map) < size) {
			vm_map_unlock(map);
			return (0);
		}
		assert_wait(map, TRUE);
		vm_map_unlock(map);
		thread_block();
	}
	vm_map_insert(map, NULL, (vm_offset_t)0, addr, addr + size);
	vm_map_unlock(map);
	return (addr);
}
