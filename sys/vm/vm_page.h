/* 
 * Copyright (c) 1991 Regents of the University of California.
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
 *	from: @(#)vm_page.h	7.3 (Berkeley) 4/21/91
 *	$Id: vm_page.h,v 1.7.2.2 1993/12/20 12:40:21 cgd Exp $
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

#ifndef _VM_VM_PAGE_H_
#define _VM_VM_PAGE_H_

/*
 *	Resident memory system definitions.
 */

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

struct vm_page {
	queue_chain_t	pageq;		/* queue info for FIFO
					 * queue or free list (P) */
	queue_chain_t	hashq;		/* hash table links (O)*/
	queue_chain_t	listq;		/* all pages in same object (O)*/

	vm_object_t	object;		/* which object am I in (O,P)*/
	vm_offset_t	offset;		/* offset into that object (O,P) */

	u_short		wire_count;	/* number wired down maps use me? (P) */
	u_short		flags;		/* flags; see below */

	vm_offset_t	phys_addr;	/* physical address of page */
};

/*
 * These are the flags defined for vm_page.
 */
#define PG_INACTIVE	0x0001		/* page is in inactive list (P) */
#define PG_ACTIVE	0x0002		/* page is in active list (P) */
#define PG_LAUNDRY	0x0004		/* page is being cleaned now (P)*/
#define PG_CLEAN	0x0008		/* page has not been modified */
#define PG_BUSY		0x0010		/* page is in transit (O) */
#define PG_WANTED	0x0020		/* someone is waiting for page (O) */
#define PG_TABLED	0x0040		/* page is in VP table (O) */
#define PG_COW		0x0080		/* must copy page before changing (O) */
#define PG_FICTITIOUS	0x0100		/* physical page doesn't exist (O) */
#define PG_FAKE		0x0200		/* page is placeholder for pagein (O) */
#define PG_PAGEROWNED	0x4000		/* DEBUG: async paging op in progress */
#define PG_PTPAGE	0x8000		/* DEBUG: is a user page table page */

#if	VM_PAGE_DEBUG
#ifdef	MACHINE_NONCONTIG
#define	VM_PAGE_CHECK(mem) { \
	if ( (((unsigned int) mem) < ((unsigned int) &vm_page_array[0])) || \
	    (((unsigned int) mem) > \
		((unsigned int) &vm_page_array[vm_page_count])) || \
	    ((mem->flags & (PG_ACTIVE | PG_INACTIVE)) == \
		(PG_ACTIVE | PG_INACTIVE))) \
		panic("vm_page_check: not valid!"); \
	}
#else	/* MACHINE_NONCONTIG */
#define	VM_PAGE_CHECK(mem) { \
	if ( (((unsigned int) mem) < ((unsigned int) &vm_page_array[0])) || \
	    (((unsigned int) mem) > \
		((unsigned int) &vm_page_array[last_page-first_page])) || \
	    ((mem->flags & (PG_ACTIVE | PG_INACTIVE)) == \
		(PG_ACTIVE | PG_INACTIVE)) ) \
		panic("vm_page_check: not valid!"); \
	}
#endif	/* MACHINE_NONCONTIG */
#else	/* VM_PAGE_DEBUG */
#define	VM_PAGE_CHECK(mem)
#endif	/* VM_PAGE_DEBUG */

#ifdef	KERNEL
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
queue_head_t	vm_page_queue_free;	/* memory free queue */
extern
queue_head_t	vm_page_queue_active;	/* active memory queue */
extern
queue_head_t	vm_page_queue_inactive;	/* inactive memory queue */

extern
vm_page_t	vm_page_array;		/* First resident page in table */

#ifndef MACHINE_NONCONTIG
extern
long		first_page;		/* first physical page number */
					/* ... represented in vm_page_array */
extern
long		last_page;		/* last physical page number */

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
					/* ... represented in vm_page_array */

extern
int	vm_page_free_count;	/* How many pages are free? */
extern
int	vm_page_active_count;	/* How many pages are active? */
extern
int	vm_page_inactive_count;	/* How many pages are inactive? */
extern
int	vm_page_wire_count;	/* How many pages are wired? */
extern
int	vm_page_free_target;	/* How many do we want free? */
extern
int	vm_page_free_min;	/* When to wakeup pageout */
extern
int	vm_page_inactive_target;/* How many do we want inactive? */
extern
int	vm_page_free_reserved;	/* How many pages reserved to do pageout */
extern
int	vm_page_laundry_count;	/* How many pages being laundered? */

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

#ifndef MACHINE_NONCONTIG
#define IS_VM_PHYSADDR(pa) \
	((pa) >= first_phys_addr && (pa) <= last_phys_addr)

#define PHYS_TO_VM_PAGE(pa) \
	(&vm_page_array[atop(pa) - first_page ])
#else
#define	IS_VM_PHYSADDR(pa) \
	(pmap_page_index(pa) >= 0)
#define	PHYS_TO_VM_PAGE(pa) \
	(&vm_page_array[pmap_page_index(pa) - first_page])
#endif /* MACHINE_NONCONTIG */

extern
simple_lock_data_t	vm_page_queue_lock;	/* lock on active and inactive
						   page queues */
extern
simple_lock_data_t	vm_page_queue_free_lock;
						/* lock on free page queue */
vm_offset_t	vm_page_startup();
vm_page_t	vm_page_lookup();
vm_page_t	vm_page_alloc();
void		vm_page_free();
void		vm_page_activate();
void		vm_page_deactivate();
void		vm_page_rename();
void		vm_page_replace();
void		vm_page_insert();

boolean_t	vm_page_zero_fill();
void		vm_page_copy();

void		vm_page_wire();
void		vm_page_unwire();

void		vm_set_page_size();

/*
 *	Functions implemented as macros
 */

#define PAGE_ASSERT_WAIT(m, interruptible) { \
		(m)->flags |= PG_WANTED; \
		assert_wait((int) (m), (interruptible)); \
	}

#define PAGE_WAKEUP(m) { \
		(m)->flags &= ~PG_BUSY; \
		if ((m)->flags & PG_WANTED) { \
			(m)->flags &= ~PG_WANTED; \
			thread_wakeup((int) (m)); \
		} \
	}

#ifdef MACHINE_NONCONTIG
#define VM_PAGE_INIT(mem, obj, offset) { \
		(mem)->flags = PG_BUSY | PG_CLEAN | PG_FAKE; \
		if (obj) \
			vm_page_insert((mem), (obj), (offset)); \
		else \
			(mem)->object = NULL; \
		(mem)->wire_count = 0; \
	}
#else	/* MACHINE_NONCONTIG */
#define VM_PAGE_INIT(mem, obj, offset) { \
		(mem)->flags = PG_BUSY | PG_CLEAN | PG_FAKE; \
		vm_page_insert((mem), (obj), (offset)); \
		(mem)->wire_count = 0; \
	}
#endif	/* MACHINE_NONCONTIG */

#define VM_PAGE_LOCK_QUEUES()	simple_lock(&vm_page_queue_lock)
#define VM_PAGE_UNLOCK_QUEUES()	simple_unlock(&vm_page_queue_lock)

#define VM_PAGE_SET_MODIFIED(m)	{ (m)->flags &= ~PG_CLEAN; }

#endif	/* KERNEL */

#endif	/* !_VM_VM_PAGE_H_ */
