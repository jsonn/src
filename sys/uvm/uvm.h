/*	$NetBSD: uvm.h,v 1.21.2.1 2000/06/22 17:10:42 minoura Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm.h,v 1.1.2.14 1998/02/02 20:07:19 chuck Exp
 */

#ifndef _UVM_UVM_H_
#define _UVM_UVM_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#include "opt_uvmhist.h"
#endif

#include <uvm/uvm_extern.h>

#include <uvm/uvm_stat.h>

/*
 * pull in prototypes
 */

#include <uvm/uvm_amap.h>
#include <uvm/uvm_aobj.h>
#include <uvm/uvm_fault.h>
#include <uvm/uvm_glue.h>
#include <uvm/uvm_km.h>
#include <uvm/uvm_loan.h>
#include <uvm/uvm_map.h>
#include <uvm/uvm_object.h>
#include <uvm/uvm_page.h>
#include <uvm/uvm_pager.h>
#include <uvm/uvm_pdaemon.h>
#include <uvm/uvm_swap.h>

/*
 * pull in VM_NFREELIST
 */
#include <machine/vmparam.h>

/*
 * uvm structure (vm global state: collected in one structure for ease
 * of reference...)
 */

struct uvm {
	/* vm_page related parameters */
		/* vm_page queues */
	struct pgfreelist page_free[VM_NFREELIST]; /* unallocated pages */
	struct pglist page_active;	/* allocated pages, in use */
	struct pglist page_inactive_swp;/* pages inactive (reclaim or free) */
	struct pglist page_inactive_obj;/* pages inactive (reclaim or free) */
	simple_lock_data_t pageqlock;	/* lock for active/inactive page q */
	simple_lock_data_t fpageqlock;	/* lock for free page q */
	boolean_t page_init_done;	/* TRUE if uvm_page_init() finished */
	boolean_t page_idle_zero;	/* TRUE if we should try to zero
					   pages in the idle loop */
		/* page daemon trigger */
	int pagedaemon;			/* daemon sleeps on this */
	struct proc *pagedaemon_proc;	/* daemon's pid */
	simple_lock_data_t pagedaemon_lock;
		/* page hash */
	struct pglist *page_hash;	/* page hash table (vp/off->page) */
	int page_nhash;			/* number of buckets */
	int page_hashmask;		/* hash mask */
	simple_lock_data_t hashlock;	/* lock on page_hash array */

	/* anon stuff */
	struct vm_anon *afree;		/* anon free list */
	simple_lock_data_t afreelock; 	/* lock on anon free list */

	/* static kernel map entry pool */
	vm_map_entry_t kentry_free;	/* free page pool */
	simple_lock_data_t kentry_lock;

	/* aio_done is locked by uvm.pagedaemon_lock and splbio! */
	struct uvm_aiohead aio_done;	/* done async i/o reqs */

	/* pager VM area bounds */
	vaddr_t pager_sva;		/* start of pager VA area */
	vaddr_t pager_eva;		/* end of pager VA area */

	/* swap-related items */
	simple_lock_data_t swap_data_lock;

	/* kernel object: to support anonymous pageable kernel memory */
	struct uvm_object *kernel_object;
};

/*
 * vm_map_entry etype bits:
 */

#define UVM_ET_OBJ		0x01	/* it is a uvm_object */
#define UVM_ET_SUBMAP		0x02	/* it is a vm_map submap */
#define UVM_ET_COPYONWRITE 	0x04	/* copy_on_write */
#define UVM_ET_NEEDSCOPY	0x08	/* needs_copy */

#define UVM_ET_ISOBJ(E)		(((E)->etype & UVM_ET_OBJ) != 0)
#define UVM_ET_ISSUBMAP(E)	(((E)->etype & UVM_ET_SUBMAP) != 0)
#define UVM_ET_ISCOPYONWRITE(E)	(((E)->etype & UVM_ET_COPYONWRITE) != 0)
#define UVM_ET_ISNEEDSCOPY(E)	(((E)->etype & UVM_ET_NEEDSCOPY) != 0)

/*
 * macros
 */

#ifdef _KERNEL

extern struct uvm uvm;

/*
 * historys
 */

UVMHIST_DECL(maphist);
UVMHIST_DECL(pdhist);

/*
 * UVM_UNLOCK_AND_WAIT: atomic unlock+wait... wrapper around the
 * interlocked tsleep() function.
 */

#define	UVM_UNLOCK_AND_WAIT(event, slock, intr, msg, timo)		\
do {									\
	(void) ltsleep(event, PVM | PNORELOCK | (intr ? PCATCH : 0),	\
	    msg, timo, slock);						\
} while (0)

/*
 * UVM_PAGE_OWN: track page ownership (only if UVM_PAGE_TRKOWN)
 */

#if defined(UVM_PAGE_TRKOWN)
#define UVM_PAGE_OWN(PG, TAG) uvm_page_own(PG, TAG)
#else
#define UVM_PAGE_OWN(PG, TAG) /* nothing */
#endif /* UVM_PAGE_TRKOWN */

/*
 * pull in inlines
 */

#include <uvm/uvm_amap_i.h>
#include <uvm/uvm_fault_i.h>
#include <uvm/uvm_map_i.h>
#include <uvm/uvm_page_i.h>
#include <uvm/uvm_pager_i.h>

#endif /* _KERNEL */

#endif /* _UVM_UVM_H_ */
