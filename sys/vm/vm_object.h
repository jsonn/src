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
 *	from: @(#)vm_object.h	7.3 (Berkeley) 4/21/91
 *	$Id: vm_object.h,v 1.9.2.1 1994/03/18 05:46:24 cgd Exp $
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

#ifndef _VM_VM_OBJECT_H_
#define _VM_VM_OBJECT_H_

/*
 *	Virtual memory object module definitions.
 */

#include <vm/vm_page.h>
#include <vm/vm_pager.h>

/*
 *	Types defined:
 *
 *	vm_object_t		Virtual memory object.
 */

struct vm_object {
	struct pglist		memq;		/* Resident memory */
	TAILQ_ENTRY(vm_object)	object_list;	/* list of all objects */
	simple_lock_data_t	Lock;		/* Synchronization */
	int			ref_count;	/* How many refs?? */
	vm_size_t		size;		/* Object size */
	int			resident_page_count;
						/* number of resident pages */
	struct vm_object	*copy;		/* Object that holds copies of
						   my changed pages */
	vm_pager_t		pager;		/* Where to get data */
	vm_offset_t		paging_offset;	/* Offset into paging space */
	struct vm_object	*shadow;	/* My shadow */
	vm_offset_t		shadow_offset;	/* Offset in shadow */
	u_short			paging_in_progress;
						/* Paging (in or out) - don't
						   collapse or destroy */
	u_short			flags;		/* object flags; see below */
	TAILQ_ENTRY(vm_object)	cached_list;	/* for persistence */
};
/*
 * Flags
 */
#define OBJ_CANPERSIST		0x0001	/* allow to persist */
#define OBJ_INTERNAL		0x0002	/* internally created object */
#define OBJ_ACTIVE		0x0004	/* used to mark active objects */

TAILQ_HEAD(vm_object_hash_head, vm_object_hash_entry);

struct vm_object_hash_entry {
	TAILQ_ENTRY(vm_object_hash_entry)  hash_links;	/* hash chain links */
	vm_object_t			   object;	/* object represented */
};

typedef struct vm_object_hash_entry	*vm_object_hash_entry_t;

#ifdef	KERNEL
TAILQ_HEAD(object_q, vm_object);

struct object_q	vm_object_cached_list;	/* list of objects persisting */
int		vm_object_cached;	/* size of cached list */
simple_lock_data_t	vm_cache_lock;	/* lock for object cache */

struct object_q	vm_object_list;		/* list of allocated objects */
long		vm_object_count;	/* count of all objects */
simple_lock_data_t	vm_object_list_lock;
					/* lock for object list and count */

vm_object_t	kernel_object;		/* the single kernel object */
vm_object_t	kmem_object;

/*
 *	Declare procedures that operate on VM objects.
 */
void		vm_object_init __P((void));
vm_object_t	vm_object_allocate __P((vm_size_t));
void		vm_object_reference __P((vm_object_t));
void		vm_object_deallocate __P((vm_object_t));
void		vm_object_page_clean
		   __P((vm_object_t, vm_offset_t, vm_offset_t));
void		vm_object_pmap_copy
		   __P((vm_object_t, vm_offset_t, vm_offset_t));
void		vm_object_pmap_remove
		   __P((vm_object_t, vm_offset_t, vm_offset_t));
void		vm_object_copy
		   __P((vm_object_t, vm_offset_t, vm_size_t,
			vm_object_t *, vm_offset_t *, boolean_t *));
void		vm_object_shadow
		   __P((vm_object_t *, vm_offset_t *, vm_size_t));
void		vm_object_setpager
		   __P((vm_object_t, vm_pager_t, vm_offset_t, boolean_t));
vm_object_t	vm_object_lookup __P((vm_pager_t));
void		vm_object_enter __P((vm_object_t, vm_pager_t));
void		vm_object_collapse __P((vm_object_t));
void		vm_object_page_remove
		   __P((vm_object_t, vm_offset_t, vm_offset_t));
boolean_t	vm_object_coalesce
		   __P((vm_object_t, vm_object_t, vm_offset_t, vm_offset_t,
			vm_size_t, vm_size_t));
void		vm_object_print __P((vm_object_t, boolean_t));
void		_vm_object_print __P((vm_object_t, boolean_t, int (*)()));

/*
 *	Functions implemented as macros
 */
#define		vm_object_cache_lock()		simple_lock(&vm_cache_lock)
#define		vm_object_cache_unlock()	simple_unlock(&vm_cache_lock)

#define		vm_object_cache(pager)		pager_cache(vm_object_lookup(pager),TRUE)
#define		vm_object_uncache(pager)	pager_cache(vm_object_lookup(pager),FALSE)

#define	vm_object_lock_init(object) \
	simple_lock_init(&(object)->Lock)
#define	vm_object_lock(object) \
	simple_lock(&(object)->Lock)
#define	vm_object_unlock(object) \
	simple_unlock(&(object)->Lock)
#define	vm_object_lock_try(object) \
	simple_lock_try(&(object)->Lock)
#define	vm_object_sleep(event, object) \
	thread_sleep((event), &(object)->Lock)

#endif	/* KERNEL */

#endif /* !_VM_VM_OBJECT_H_ */
