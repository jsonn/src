/*	$NetBSD: uvm_page_i.h,v 1.16.2.1 2001/06/21 20:10:40 nathanw Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and
 *      its contributors.
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
 *	@(#)vm_page.c   8.3 (Berkeley) 3/21/94
 * from: Id: uvm_page_i.h,v 1.1.2.7 1998/01/05 00:26:02 chuck Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
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

#ifndef _UVM_UVM_PAGE_I_H_
#define _UVM_UVM_PAGE_I_H_

/*
 * uvm_page_i.h
 */

/*
 * inline functions [maybe]
 */

#if defined(UVM_PAGE_INLINE) || defined(UVM_PAGE)

/*
 * uvm_lock_fpageq: lock the free page queue
 *
 * => free page queue can be accessed in interrupt context, so this
 *	blocks all interrupts that can cause memory allocation, and
 *	returns the previous interrupt level.
 */

PAGE_INLINE int
uvm_lock_fpageq()
{
	int s;

	s = splvm();
	simple_lock(&uvm.fpageqlock);
	return (s);
}

/*
 * uvm_unlock_fpageq: unlock the free page queue
 *
 * => caller must supply interrupt level returned by uvm_lock_fpageq()
 *	so that it may be restored.
 */

PAGE_INLINE void
uvm_unlock_fpageq(s)
	int s;
{

	simple_unlock(&uvm.fpageqlock);
	splx(s);
}

/*
 * uvm_pagelookup: look up a page
 *
 * => caller should lock object to keep someone from pulling the page
 *	out from under it
 */

struct vm_page *
uvm_pagelookup(obj, off)
	struct uvm_object *obj;
	voff_t off;
{
	struct vm_page *pg;
	struct pglist *buck;
	int s;

	buck = &uvm.page_hash[uvm_pagehash(obj,off)];

	s = splvm();
	simple_lock(&uvm.hashlock);
	TAILQ_FOREACH(pg, buck, hashq) {
		if (pg->uobject == obj && pg->offset == off) {
			break;
		}
	}
	simple_unlock(&uvm.hashlock);
	splx(s);
	return(pg);
}

/*
 * uvm_pagewire: wire the page, thus removing it from the daemon's grasp
 *
 * => caller must lock page queues
 */

PAGE_INLINE void
uvm_pagewire(pg)
	struct vm_page *pg;
{
	if (pg->wire_count == 0) {
		if (pg->pqflags & PQ_ACTIVE) {
			TAILQ_REMOVE(&uvm.page_active, pg, pageq);
			pg->pqflags &= ~PQ_ACTIVE;
			uvmexp.active--;
		}
		if (pg->pqflags & PQ_INACTIVE) {
			TAILQ_REMOVE(&uvm.page_inactive, pg, pageq);
			pg->pqflags &= ~PQ_INACTIVE;
			uvmexp.inactive--;
		}
		uvmexp.wired++;
	}
	pg->wire_count++;
}

/*
 * uvm_pageunwire: unwire the page.
 *
 * => activate if wire count goes to zero.
 * => caller must lock page queues
 */

PAGE_INLINE void
uvm_pageunwire(pg)
	struct vm_page *pg;
{
	pg->wire_count--;
	if (pg->wire_count == 0) {
		TAILQ_INSERT_TAIL(&uvm.page_active, pg, pageq);
		uvmexp.active++;
		pg->pqflags |= PQ_ACTIVE;
		uvmexp.wired--;
	}
}

/*
 * uvm_pagedeactivate: deactivate page
 *
 * => caller must lock page queues
 * => caller must check to make sure page is not wired
 * => object that page belongs to must be locked (so we can adjust pg->flags)
 * => caller must clear the reference on the page before calling
 */

PAGE_INLINE void
uvm_pagedeactivate(pg)
	struct vm_page *pg;
{
	if (pg->pqflags & PQ_ACTIVE) {
		TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		pg->pqflags &= ~PQ_ACTIVE;
		uvmexp.active--;
	}
	if ((pg->pqflags & PQ_INACTIVE) == 0) {
		KASSERT(pg->wire_count == 0);
		TAILQ_INSERT_TAIL(&uvm.page_inactive, pg, pageq);
		pg->pqflags |= PQ_INACTIVE;
		uvmexp.inactive++;

		/*
		 * update the "clean" bit.  this isn't 100%
		 * accurate, and doesn't have to be.  we'll
		 * re-sync it after we zap all mappings when
		 * scanning the inactive list.
		 */
		if ((pg->flags & PG_CLEAN) != 0 &&
		    pmap_is_modified(pg))
			pg->flags &= ~PG_CLEAN;
	}
}

/*
 * uvm_pageactivate: activate page
 *
 * => caller must lock page queues
 */

PAGE_INLINE void
uvm_pageactivate(pg)
	struct vm_page *pg;
{
	if (pg->pqflags & PQ_INACTIVE) {
		TAILQ_REMOVE(&uvm.page_inactive, pg, pageq);
		pg->pqflags &= ~PQ_INACTIVE;
		uvmexp.inactive--;
	}
	if (pg->wire_count == 0) {

		/*
		 * if page is already active, remove it from list so we
		 * can put it at tail.  if it wasn't active, then mark
		 * it active and bump active count
		 */
		if (pg->pqflags & PQ_ACTIVE)
			TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		else {
			pg->pqflags |= PQ_ACTIVE;
			uvmexp.active++;
		}

		TAILQ_INSERT_TAIL(&uvm.page_active, pg, pageq);
	}
}

/*
 * uvm_pagezero: zero fill a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */

PAGE_INLINE void
uvm_pagezero(pg)
	struct vm_page *pg;
{

	pg->flags &= ~PG_CLEAN;
	pmap_zero_page(VM_PAGE_TO_PHYS(pg));
}

/*
 * uvm_pagecopy: copy a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */

PAGE_INLINE void
uvm_pagecopy(src, dst)
	struct vm_page *src, *dst;
{

	dst->flags &= ~PG_CLEAN;
	pmap_copy_page(VM_PAGE_TO_PHYS(src), VM_PAGE_TO_PHYS(dst));
}

/*
 * uvm_page_lookup_freelist: look up the free list for the specified page
 */

PAGE_INLINE int
uvm_page_lookup_freelist(pg)
	struct vm_page *pg;
{
	int lcv;

	lcv = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), NULL);
	KASSERT(lcv != -1);
	return (vm_physmem[lcv].free_list);
}

#endif /* defined(UVM_PAGE_INLINE) || defined(UVM_PAGE) */

#endif /* _UVM_UVM_PAGE_I_H_ */
