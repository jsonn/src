/*	$NetBSD: vm_page.h,v 1.19.8.3 1997/05/17 00:06:27 thorpej Exp $	*/

/* 
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vm_page.h	7.3 (Berkeley) 4/21/91
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
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

/*
 *	Resident memory system definitions.
 */

#ifndef	_VM_PAGE_
#define	_VM_PAGE_

/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P).
 */

TAILQ_HEAD(pglist, vm_page);

struct vm_page {
	TAILQ_ENTRY(vm_page)	pageq;		/* queue info for FIFO
						 * queue or free list (P) */
	TAILQ_ENTRY(vm_page)	hashq;		/* hash table links (O)*/
	TAILQ_ENTRY(vm_page)	listq;		/* pages in same object (O)*/

	vm_object_t		object;		/* which object am I in (O,P)*/
	vm_offset_t		offset;		/* offset into object (O,P) */

	u_short			wire_count;	/* wired down maps refs (P) */
	u_short			flags;		/* see below */

	vm_offset_t		phys_addr;	/* physical address of page */
};

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_FILLED and PG_DIRTY are added for the filesystems.
 */
#define	PG_INACTIVE	0x0001		/* page is in inactive list (P) */
#define	PG_ACTIVE	0x0002		/* page is in active list (P) */
#define	PG_LAUNDRY	0x0004		/* page is being cleaned now (P)*/
#define	PG_CLEAN	0x0008		/* page has not been modified
					   There exists a case where this bit
					   will be cleared, although the page
					   is not physically dirty, which is
					   when a collapse operation moves
					   pages between two different pagers.
					   The bit is then used as a marker
					   for the pageout daemon to know it
					   should be paged out into the target
					   pager. */
#define	PG_BUSY		0x0010		/* page is in transit (O) */
#define	PG_WANTED	0x0020		/* someone is waiting for page (O) */
#define	PG_TABLED	0x0040		/* page is in VP table (O) */
#define	PG_COPYONWRITE	0x0080		/* must copy page before changing (O) */
#define	PG_FICTITIOUS	0x0100		/* physical page doesn't exist (O) */
#define	PG_FAKE		0x0200		/* page is placeholder for pagein (O) */
#define	PG_FILLED	0x0400		/* client flag to set when filled */
#define	PG_DIRTY	0x0800		/* client flag to set when dirty */
#define	PG_FREE		0x1000		/* XXX page is on free list */
#define	PG_FAULTING	0x2000		/* page is being faulted in */
#define	PG_PAGEROWNED	0x4000		/* DEBUG: async paging op in progress */
#define	PG_PTPAGE	0x8000		/* DEBUG: is a user page table page */

#if	VM_PAGE_DEBUG
#ifndef	MACHINE_NONCONTIG
#define	VM_PAGE_CHECK(mem) { \
	if ((((unsigned int) mem) < ((unsigned int) &vm_page_array[0])) || \
	    (((unsigned int) mem) > \
		((unsigned int) &vm_page_array[last_page-first_page])) || \
	    ((mem->flags & (PG_ACTIVE | PG_INACTIVE)) == \
		(PG_ACTIVE | PG_INACTIVE))) \
		panic("vm_page_check: not valid!"); \
}
#else	/* MACHINE_NONCONTIG */
#define	VM_PAGE_CHECK(mem) { \
	if ((((unsigned int) mem) < ((unsigned int) &vm_page_array[0])) || \
	    (((unsigned int) mem) > \
		((unsigned int) &vm_page_array[vm_page_count])) || \
	    ((mem->flags & (PG_ACTIVE | PG_INACTIVE)) == \
		(PG_ACTIVE | PG_INACTIVE))) \
		panic("vm_page_check: not valid!"); \
}
#endif	/* MACHINE_NONCONTIG */
#else /* VM_PAGE_DEBUG */
#define	VM_PAGE_CHECK(mem)
#endif /* VM_PAGE_DEBUG */

#ifdef _KERNEL
/*
 *	Each pageable resident page falls into one of three lists:
 *
 *	free	
 *		Available for allocation now.
 *	inactive
 *		Not referenced in any map, but still has an
 *		object/offset-page mapping, and may be dirty.
 *		This is the list of pages that should be
 *		paged out next.
 *	active
 *		A list of pages which have been placed in
 *		at least one physical map.  This list is
 *		ordered, in LRU-like fashion.
 */

extern
struct pglist	vm_page_queue_free;	/* memory free queue */
extern
struct pglist	vm_page_queue_active;	/* active memory queue */
extern
struct pglist	vm_page_queue_inactive;	/* inactive memory queue */

extern
vm_page_t	vm_page_array;		/* First resident page in table */

#ifndef MACHINE_NONCONTIG
extern
long		first_page;		/* first physical page number */
					/* ... represented in vm_page_array */
extern
long		last_page;		/* last physical page number */
					/* ... represented in vm_page_array */
					/* [INCLUSIVE] */
extern
vm_offset_t	first_phys_addr;	/* physical address for first_page */
extern
vm_offset_t	last_phys_addr;		/* physical address for last_page */
#else	/* MACHINE_NONCONTIG */
extern
u_long		first_page;		/* first physical page number */
extern
int		vm_page_count;		/* How many pages do we manage? */
#endif	/* MACHINE_NONCONTIG */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#ifndef MACHINE_NONCONTIG
#define IS_VM_PHYSADDR(pa) \
		((pa) >= first_phys_addr && (pa) <= last_phys_addr)

#define	VM_PAGE_INDEX(pa) \
		(atop((pa)) - first_page)
#else
#define	IS_VM_PHYSADDR(pa) \
({ \
	int __pmapidx = pmap_page_index(pa); \
	(__pmapidx >= 0 && __pmapidx >= first_page); \
})

#define	VM_PAGE_INDEX(pa) \
		(pmap_page_index((pa)) - first_page)
#endif /* MACHINE_NONCONTIG */

#define	PHYS_TO_VM_PAGE(pa) \
		(&vm_page_array[VM_PAGE_INDEX((pa))])

#define	VM_PAGE_IS_FREE(entry)	((entry)->flags & PG_FREE)

extern
simple_lock_data_t	vm_page_queue_lock;	/* lock on active and inactive
						   page queues */
extern						/* lock on free page queue */
simple_lock_data_t	vm_page_queue_free_lock;

/*
 *	Functions implemented as macros
 */

#define PAGE_ASSERT_WAIT(m, interruptible)	{ \
				(m)->flags |= PG_WANTED; \
				assert_wait((m), (interruptible)); \
			}

#define PAGE_WAKEUP(m)	{ \
				(m)->flags &= ~PG_BUSY; \
				if ((m)->flags & PG_WANTED) { \
					(m)->flags &= ~PG_WANTED; \
					thread_wakeup((m)); \
				} \
			}

#define	vm_page_lock_queues()	simple_lock(&vm_page_queue_lock)
#define	vm_page_unlock_queues()	simple_unlock(&vm_page_queue_lock)

#define vm_page_set_modified(m)	{ (m)->flags &= ~PG_CLEAN; }

#ifndef MACHINE_NONCONTIG
#define	VM_PAGE_INIT(mem, obj, offset) { \
	(mem)->flags = PG_BUSY | PG_CLEAN | PG_FAKE; \
	vm_page_insert((mem), (obj), (offset)); \
	(mem)->wire_count = 0; \
}
#else	/* MACHINE_NONCONTIG */
#define	VM_PAGE_INIT(mem, obj, offset) { \
	(mem)->flags = PG_BUSY | PG_CLEAN | PG_FAKE; \
	if (obj) \
		vm_page_insert((mem), (obj), (offset)); \
	else \
		(mem)->object = NULL; \
	(mem)->wire_count = 0; \
}
#endif	/* MACHINE_NONCONTIG */

/* XXX what is this here for? */
void		 vm_set_page_size __P((void));

/* XXX probably should be elsewhere. */
#ifdef MACHINE_NONCONTIG
vm_offset_t	 pmap_steal_memory __P((vm_size_t));
void		 pmap_startup __P((vm_offset_t *, vm_offset_t *));
#endif

void		 vm_page_activate __P((vm_page_t));
vm_page_t	 vm_page_alloc __P((vm_object_t, vm_offset_t));
int		 vm_page_alloc_memory __P((vm_size_t, vm_offset_t,
			vm_offset_t, vm_offset_t, vm_offset_t,
			struct pglist *, int, int));
void		 vm_page_free_memory __P((struct pglist *));
#ifdef MACHINE_NONCONTIG
void		 vm_page_bootstrap __P((vm_offset_t *, vm_offset_t *));
#endif
void		 vm_page_copy __P((vm_page_t, vm_page_t));
void		 vm_page_deactivate __P((vm_page_t));
void		 vm_page_free __P((vm_page_t));
void		 vm_page_insert __P((vm_page_t, vm_object_t, vm_offset_t));
vm_page_t	 vm_page_lookup __P((vm_object_t, vm_offset_t));
void		 vm_page_remove __P((vm_page_t));
void		 vm_page_rename __P((vm_page_t, vm_object_t, vm_offset_t));
#ifndef MACHINE_NONCONTIG
void		 vm_page_startup __P((vm_offset_t *, vm_offset_t *));
#endif
void		 vm_page_unwire __P((vm_page_t));
void		 vm_page_wire __P((vm_page_t));
boolean_t	 vm_page_zero_fill __P((vm_page_t));

#endif /* _KERNEL */
#endif /* !_VM_PAGE_ */
