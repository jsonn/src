/*	$NetBSD: pmap.c,v 1.1.2.17 2007/10/04 15:36:58 yamt Exp $	*/

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
 */

/*
 * Copyright 2001 (c) Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is the i386 pmap modified and generalized to support x86-64
 * as well. The idea is to hide the upper N levels of the page tables
 * inside pmap_get_ptp, pmap_free_ptp and pmap_growkernel. The rest
 * is mostly untouched, except that it uses some more generalized
 * macros and interfaces.
 *
 * This pmap has been tested on the i386 as well, and it can be easily
 * adapted to PAE.
 *
 * fvdl@wasabisystems.com 18-Jun-2001
 */

/*
 * pmap.c: i386 pmap module rewrite
 * Chuck Cranor <chuck@ccrc.wustl.edu>
 * 11-Aug-97
 *
 * history of this pmap module: in addition to my own input, i used
 *    the following references for this rewrite of the i386 pmap:
 *
 * [1] the NetBSD i386 pmap.   this pmap appears to be based on the
 *     BSD hp300 pmap done by Mike Hibler at University of Utah.
 *     it was then ported to the i386 by William Jolitz of UUNET
 *     Technologies, Inc.   Then Charles M. Hannum of the NetBSD
 *     project fixed some bugs and provided some speed ups.
 *
 * [2] the FreeBSD i386 pmap.   this pmap seems to be the
 *     Hibler/Jolitz pmap, as modified for FreeBSD by John S. Dyson
 *     and David Greenman.
 *
 * [3] the Mach pmap.   this pmap, from CMU, seems to have migrated
 *     between several processors.   the VAX version was done by
 *     Avadis Tevanian, Jr., and Michael Wayne Young.    the i386
 *     version was done by Lance Berc, Mike Kupfer, Bob Baron,
 *     David Golub, and Richard Draves.    the alpha version was
 *     done by Alessandro Forin (CMU/Mach) and Chris Demetriou
 *     (NetBSD/alpha).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pmap.c,v 1.1.2.17 2007/10/04 15:36:58 yamt Exp $");

#ifndef __x86_64__
#include "opt_cputype.h"
#endif
#include "opt_user_ldt.h"
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#if !defined(__x86_64__)
#include "opt_kstack_dr0.h"
#endif /* !defined(__x86_64__) */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/kernel.h>

#include <uvm/uvm.h>

#include <dev/isa/isareg.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/specialreg.h>
#include <machine/gdt.h>
#include <machine/intr.h>
#include <machine/isa_machdep.h>
#include <machine/cpuvar.h>

#include <x86/i82489reg.h>
#include <x86/i82489var.h>


/*
 * general info:
 *
 *  - for an explanation of how the i386 MMU hardware works see
 *    the comments in <machine/pte.h>.
 *
 *  - for an explanation of the general memory structure used by
 *    this pmap (including the recursive mapping), see the comments
 *    in <machine/pmap.h>.
 *
 * this file contains the code for the "pmap module."   the module's
 * job is to manage the hardware's virtual to physical address mappings.
 * note that there are two levels of mapping in the VM system:
 *
 *  [1] the upper layer of the VM system uses vm_map's and vm_map_entry's
 *      to map ranges of virtual address space to objects/files.  for
 *      example, the vm_map may say: "map VA 0x1000 to 0x22000 read-only
 *      to the file /bin/ls starting at offset zero."   note that
 *      the upper layer mapping is not concerned with how individual
 *      vm_pages are mapped.
 *
 *  [2] the lower layer of the VM system (the pmap) maintains the mappings
 *      from virtual addresses.   it is concerned with which vm_page is
 *      mapped where.   for example, when you run /bin/ls and start
 *      at page 0x1000 the fault routine may lookup the correct page
 *      of the /bin/ls file and then ask the pmap layer to establish
 *      a mapping for it.
 *
 * note that information in the lower layer of the VM system can be
 * thrown away since it can easily be reconstructed from the info
 * in the upper layer.
 *
 * data structures we use include:
 *
 *  - struct pmap: describes the address space of one thread
 *  - struct pv_entry: describes one <PMAP,VA> mapping of a PA
 *  - struct pv_head: there is one pv_head per managed page of
 *	physical memory.   the pv_head points to a list of pv_entry
 *	structures which describe all the <PMAP,VA> pairs that this
 *      page is mapped in.    this is critical for page based operations
 *      such as pmap_page_protect() [change protection on _all_ mappings
 *      of a page]
 *  - pv_page/pv_page_info: pv_entry's are allocated out of pv_page's.
 *      if we run out of pv_entry's we allocate a new pv_page and free
 *      its pv_entrys.
 */

/*
 * memory allocation
 *
 *  - there are three data structures that we must dynamically allocate:
 *
 * [A] new process' page directory page (PDP)
 *	- plan 1: done at pmap_create() we use
 *	  uvm_km_alloc(kernel_map, PAGE_SIZE)  [fka kmem_alloc] to do this
 *	  allocation.
 *
 * if we are low in free physical memory then we sleep in
 * uvm_km_alloc -- in this case this is ok since we are creating
 * a new pmap and should not be holding any locks.
 *
 * if the kernel is totally out of virtual space
 * (i.e. uvm_km_alloc returns NULL), then we panic.
 *
 * XXX: the fork code currently has no way to return an "out of
 * memory, try again" error code since uvm_fork [fka vm_fork]
 * is a void function.
 *
 * [B] new page tables pages (PTP)
 * 	- call uvm_pagealloc()
 * 		=> success: zero page, add to pm_pdir
 * 		=> failure: we are out of free vm_pages, let pmap_enter()
 *		   tell UVM about it.
 *
 * note: for kernel PTPs, we start with NKPTP of them.   as we map
 * kernel memory (at uvm_map time) we check to see if we've grown
 * the kernel pmap.   if so, we call the optional function
 * pmap_growkernel() to grow the kernel PTPs in advance.
 *
 * [C] pv_entry structures
 *	- plan 1: try to allocate one off the free list
 *		=> success: done!
 *		=> failure: no more free pv_entrys on the list
 *	- plan 2: try to allocate a new pv_page to add a chunk of
 *	pv_entrys to the free list
 *		[a] obtain a free, unmapped, VA in kmem_map.  either
 *		we have one saved from a previous call, or we allocate
 *		one now using a "vm_map_lock_try" in uvm_map
 *		=> success: we have an unmapped VA, continue to [b]
 *		=> failure: unable to lock kmem_map or out of VA in it.
 *			move on to plan 3.
 *		[b] allocate a page for the VA
 *		=> success: map it in, free the pv_entry's, DONE!
 *		=> failure: no free vm_pages, etc.
 *			save VA for later call to [a], go to plan 3.
 *	If we fail, we simply let pmap_enter() tell UVM about it.
 */

/*
 * locking
 *
 * we have the following locks that we must contend with:
 *
 * RW locks:
 *
 *  - pmap_main_lock
 *    this lock is used to prevent deadlock and/or provide mutex
 *    access to the pmap system.   most operations lock the pmap
 *    structure first, then they lock the pv_lists (if needed).
 *    however, some operations such as pmap_page_protect lock
 *    the pv_lists and then lock pmaps.   in order to prevent a
 *    cycle, we require a mutex lock when locking the pv_lists
 *    first.   thus, the "pmap = >pv_list" lockers must gain a
 *    read-lock on pmap_main_lock before locking the pmap.   and
 *    the "pv_list => pmap" lockers must gain a write-lock on
 *    pmap_main_lock before locking.    since only one thread
 *    can write-lock a lock at a time, this provides mutex.
 *
 * mutexes:
 *
 * - pmap lock (per pmap, part of uvm_object)
 *   this lock protects the fields in the pmap structure including
 *   the non-kernel PDEs in the PDP, and the PTEs.  it also locks
 *   in the alternate PTE space (since that is determined by the
 *   entry in the PDP).
 *
 * - pvh_lock (per pv_head)
 *   this lock protects the pv_entry list which is chained off the
 *   pv_head structure for a specific managed PA.   it is locked
 *   when traversing the list (e.g. adding/removing mappings,
 *   syncing R/M bits, etc.)
 *
 * - pmap_cpu::pc_pv_lock
 *   this lock protects the data structures which are used to manage
 *   the free list of pv_entry structures.
 *
 * - pmaps_lock
 *   this lock protects the list of active pmaps (headed by "pmaps").
 *   we lock it when adding or removing pmaps from this list.
 *
 * tlb shootdown
 *
 * tlb shootdowns are hard interrupts that operate outside the spl
 * framework: they don't need to be blocked provided that the pmap module
 * gets the order of events correct.  the calls are made by talking directly
 * to the lapic.  the stubs to handle the interrupts are quite short and do
 * one of the following: invalidate a single page, a range of pages, all
 * user tlb entries or the entire tlb.
 * 
 * the cpus synchronize with each other using pmap_mbox structures which are
 * aligned on 64-byte cache lines.  tlb shootdowns against the kernel pmap
 * use a global mailbox and are generated using a broadcast ipi (broadcast
 * to all but the sending cpu).  shootdowns against regular pmaps use
 * per-cpu mailboxes and are multicast.  kernel and user shootdowns can
 * execute simultaneously, as can shootdowns within different multithreaded
 * processes.  TODO:
 * 
 *   1. figure out which waitpoints can be deferered to pmap_update().
 *   2. see if there is a cheap way to batch some updates.
 */

vaddr_t ptp_masks[] = PTP_MASK_INITIALIZER;
int ptp_shifts[] = PTP_SHIFT_INITIALIZER;
long nkptp[] = NKPTP_INITIALIZER;
long nkptpmax[] = NKPTPMAX_INITIALIZER;
long nbpd[] = NBPD_INITIALIZER;
pd_entry_t *normal_pdes[] = PDES_INITIALIZER;
pd_entry_t *alternate_pdes[] = APDES_INITIALIZER;

/*
 * locking data structures.  to enable the locks, changes from the
 * 'vmlocking' cvs branch are required.  for now, just stub them out.
 */

#define rw_enter(a, b)		/* nothing */
#define	rw_exit(a)		/* nothing */
#define	mutex_enter(a)		simple_lock(a)
#define	mutex_exit(a)		simple_unlock(a)
#define	mutex_init(a, b, c)	simple_lock_init(a)
#define	mutex_owned(a)		(1)
#define	mutex_destroy(a)	/* nothing */
#define	crit_enter()		/* nothing */
#define	crit_exit()		/* nothing */
#define kmutex_t		struct simplelock

static kmutex_t pmaps_lock;
static krwlock_t pmap_main_lock;

static vaddr_t pmap_maxkvaddr;

#define COUNT(x)	/* nothing */

TAILQ_HEAD(pv_pagelist, pv_page);
typedef struct pv_pagelist pv_pagelist_t;

/*
 * Global TLB shootdown mailbox.
 */
struct evcnt pmap_tlb_evcnt __aligned(64);
struct pmap_mbox pmap_mbox __aligned(64);

/*
 * Per-CPU data.  The pmap mailbox is cache intensive so gets its
 * own line.  Note that the mailbox must be the first item.
 */
struct pmap_cpu {
	/* TLB shootdown */
	struct pmap_mbox pc_mbox;

	/* PV allocation */
	kmutex_t pc_pv_lock __aligned(64);	/* lock on structures */
	u_int pc_pv_nfpvents;			/* # of free pv entries */
	pv_pagelist_t pc_pv_freepages;		/* pv_pages with free entrys */
	pv_pagelist_t pc_pv_unusedpgs;		/* unused pv_pages */
};

union {
	struct pmap_cpu pc;
	uint8_t padding[128];
} pmap_cpu[X86_MAXPROCS] __aligned(64);

/*
 * global data structures
 */

struct pmap kernel_pmap_store;	/* the kernel's pmap (proc0) */

/*
 * pmap_pg_g: if our processor supports PG_G in the PTE then we
 * set pmap_pg_g to PG_G (otherwise it is zero).
 */

int pmap_pg_g = 0;

/*
 * pmap_largepages: if our processor supports PG_PS and we are
 * using it, this is set to true.
 */

int pmap_largepages;

/*
 * i386 physical memory comes in a big contig chunk with a small
 * hole toward the front of it...  the following two paddr_t's
 * (shared with machdep.c) describe the physical address space
 * of this machine.
 */
paddr_t avail_start;	/* PA of first available physical page */
paddr_t avail_end;	/* PA of last available physical page */

/*
 * other data structures
 */

static pt_entry_t protection_codes[8];	/* maps MI prot to i386 prot code */
static bool pmap_initialized = false;	/* pmap_init done yet? */

/*
 * the following two vaddr_t's are used during system startup
 * to keep track of how much of the kernel's VM space we have used.
 * once the system is started, the management of the remaining kernel
 * VM space is turned over to the kernel_map vm_map.
 */

static vaddr_t virtual_avail;	/* VA of first free KVA */
static vaddr_t virtual_end;	/* VA of last free KVA */

/*
 * pv_page management structures
 */

#define PVE_LOWAT (PVE_PER_PVPAGE / 2)	/* free pv_entry low water mark */
#define PVE_HIWAT (PVE_LOWAT + (PVE_PER_PVPAGE * 2))
					/* high water mark */

static inline int
pv_compare(struct pv_entry *a, struct pv_entry *b)
{

	if (a->pv_pmap < b->pv_pmap)
		return (-1);
	else if (a->pv_pmap > b->pv_pmap)
		return (1);
	else if (a->pv_va < b->pv_va)
		return (-1);
	else if (a->pv_va > b->pv_va)
		return (1);
	else
		return (0);
}

SPLAY_PROTOTYPE(pvtree, pv_entry, pv_node, pv_compare);
SPLAY_GENERATE(pvtree, pv_entry, pv_node, pv_compare);

/*
 * linked list of all non-kernel pmaps
 */

static struct pmap_head pmaps;

/*
 * pool that pmap structures are allocated from
 */

struct pool pmap_pmap_pool;

/*
 * MULTIPROCESSOR: special VA's/ PTE's are actually allocated inside a
 * X86_MAXPROCS*NPTECL array of PTE's, to avoid cache line thrashing
 * due to false sharing.
 */

#ifdef MULTIPROCESSOR
#define PTESLEW(pte, id) ((pte)+(id)*NPTECL)
#define VASLEW(va,id) ((va)+(id)*NPTECL*PAGE_SIZE)
#else
#define PTESLEW(pte, id) (pte)
#define VASLEW(va,id) (va)
#endif

/*
 * special VAs and the PTEs that map them
 */
static pt_entry_t *csrc_pte, *cdst_pte, *zero_pte, *ptp_pte, *early_zero_pte;
static char *csrcp, *cdstp, *zerop, *ptpp, *early_zerop;

/*
 * pool and cache that PDPs are allocated from
 */

struct pool pmap_pdp_pool;
struct pool_cache pmap_pdp_cache;
u_int pmap_pdp_cache_generation;

int	pmap_pdp_ctor(void *, void *, int);

void *vmmap; /* XXX: used by mem.c... it should really uvm_map_reserve it */

extern vaddr_t idt_vaddr;			/* we allocate IDT early */
extern paddr_t idt_paddr;

#ifdef _LP64
extern vaddr_t lo32_vaddr;
extern vaddr_t lo32_paddr;
#endif

extern int end;

#if defined(I586_CPU)
/* stuff to fix the pentium f00f bug */
extern vaddr_t pentium_idt_vaddr;
#endif


/*
 * local prototypes
 */

static struct pv_entry	*pmap_add_pvpage(struct pmap_cpu *, struct pv_page *,
					 bool);
static struct pv_entry	*pmap_alloc_pv(struct pmap *, int); /* see codes below */
#define ALLOCPV_NEED	0	/* need PV now */
#define ALLOCPV_TRY	1	/* just try to allocate, don't steal */
#define ALLOCPV_NONEED	2	/* don't need PV, just growing cache */
static struct pv_entry	*pmap_alloc_pvpage(struct pmap_cpu *, struct pmap *, int);
static void		 pmap_enter_pv(struct pv_head *,
				       struct pv_entry *, struct pmap *,
				       vaddr_t, struct vm_page *);
static void		 pmap_free_pv(struct pmap *, struct pv_entry *);
static void		 pmap_free_pvs(struct pmap *, struct pv_entry *);
static void		 pmap_free_pv_doit(struct pmap *,
					   struct pv_entry *);
static void		 pmap_free_pvpage(struct pmap_cpu *);
static struct vm_page	*pmap_get_ptp(struct pmap *, vaddr_t, pd_entry_t **);
static struct vm_page	*pmap_find_ptp(struct pmap *, vaddr_t, paddr_t, int);
static void		 pmap_freepage(struct pmap *, struct vm_page *, int,
				       struct vm_page **);
static void		 pmap_free_ptp(struct pmap *, struct vm_page *,
				       vaddr_t, pt_entry_t *, pd_entry_t **,
				       struct vm_page **);
static bool		 pmap_is_curpmap(struct pmap *);
static bool		 pmap_is_active(struct pmap *, struct cpu_info *, bool);
static void		 pmap_map_ptes(struct pmap *, struct pmap **,
				       pt_entry_t **, pd_entry_t ***);
static struct pv_entry	*pmap_remove_pv(struct pv_head *, struct pmap *,
					vaddr_t);
static void		 pmap_do_remove(struct pmap *, vaddr_t, vaddr_t, int);
static bool		 pmap_remove_pte(struct pmap *, struct vm_page *,
					 pt_entry_t *, vaddr_t, int,
					 struct pv_entry **);
static pt_entry_t	 pmap_remove_ptes(struct pmap *, struct vm_page *,
					  vaddr_t, vaddr_t, vaddr_t, int,
					  struct pv_entry **);
#define PMAP_REMOVE_ALL		0	/* remove all mappings */
#define PMAP_REMOVE_SKIPWIRED	1	/* skip wired mappings */

static void		 pmap_unmap_ptes(struct pmap *, struct pmap *);
static bool		 pmap_get_physpage(vaddr_t, int, paddr_t *);
static int		 pmap_pdes_invalid(vaddr_t, pd_entry_t **,
					   pd_entry_t *);
#define	pmap_pdes_valid(va, pdes, lastpde)	\
	(pmap_pdes_invalid((va), (pdes), (lastpde)) == 0)
static void		 pmap_alloc_level(pd_entry_t **, vaddr_t, int, long *);

static bool		 pmap_reactivate(struct pmap *);

/*
 * p m a p   h e l p e r   f u n c t i o n s
 */

/*
 * pmap_is_curpmap: is this pmap the one currently loaded [in %cr3]?
 *		of course the kernel is always loaded
 */

inline static bool
pmap_is_curpmap(struct pmap *pmap)
{

	return((pmap == pmap_kernel()) ||
	       (pmap == curcpu()->ci_pmap));
}

/*
 * pmap_is_active: is this pmap loaded into the specified processor's %cr3?
 */

inline static bool
pmap_is_active(struct pmap *pmap, struct cpu_info *ci, bool kernel)
{

	return (pmap == pmap_kernel() ||
	    (pmap->pm_cpus & ci->ci_cpumask) != 0 ||
	    (kernel && (pmap->pm_kernel_cpus & ci->ci_cpumask) != 0));
}

static void
pmap_apte_flush(struct pmap *pmap)
{

	/*
	 * Flush the APTE mapping from all other CPUs that
	 * are using the pmap we are using (who's APTE space
	 * is the one we've just modified).
	 *
	 * XXXthorpej -- find a way to defer the IPI.
	 */
	pmap_tlb_shootdown(pmap, (vaddr_t)-1LL, 0, 0);
	pmap_tlb_shootwait();
}

/*
 * pmap_map_ptes: map a pmap's PTEs into KVM and lock them in
 *
 * => we lock enough pmaps to keep things locked in
 * => must be undone with pmap_unmap_ptes before returning
 */

static void
pmap_map_ptes(struct pmap *pmap, struct pmap **pmap2,
    pd_entry_t **ptepp, pd_entry_t ***pdeppp)
{
	pd_entry_t opde;
	struct pmap *ourpmap;
	struct cpu_info *ci;
	struct lwp *l;
	bool iscurrent;
	uint64_t ncsw;

	/* the kernel's pmap is always accessible */
	if (pmap == pmap_kernel()) {
		*pmap2 = NULL;
		*ptepp = PTE_BASE;
		*pdeppp = normal_pdes;
		return;
	}

 retry:
	crit_enter();
	l = curlwp;
	ncsw = l->l_ncsw;
 	ourpmap = NULL;
	ci = curcpu();
	if (ci->ci_want_pmapload &&
	    vm_map_pmap(&l->l_proc->p_vmspace->vm_map) == pmap) {
		pmap_load();
		if (l->l_ncsw != ncsw) {
			crit_exit();
			goto retry;
		}
	}
	iscurrent = pmap_is_curpmap(pmap);

	/* if curpmap then we are always mapped */
	if (iscurrent) {
		mutex_enter(&pmap->pm_lock);
		*pmap2 = NULL;
		*ptepp = PTE_BASE;
		*pdeppp = normal_pdes;
		goto out;
	}

	ourpmap = ci->ci_pmap;

	/* need to lock both curpmap and pmap: use ordered locking */
	if ((uintptr_t) pmap < (uintptr_t) ourpmap) {
		mutex_enter(&pmap->pm_lock);
		mutex_enter(&ourpmap->pm_lock);
	} else {
		mutex_enter(&ourpmap->pm_lock);
		mutex_enter(&pmap->pm_lock);
	}

	if (l->l_ncsw != ncsw)
		goto unlock_and_retry;

	/* need to load a new alternate pt space into curpmap? */
	COUNT(apdp_pde_map);
	opde = *APDP_PDE;
	if (!pmap_valid_entry(opde) || (opde & PG_FRAME) != pmap->pm_pdirpa) {
		*APDP_PDE = (pd_entry_t) (pmap->pm_pdirpa | PG_RW | PG_V);
		if (pmap_valid_entry(opde))
			pmap_apte_flush(ourpmap);
	}

	*pmap2 = ourpmap;
	*ptepp = APTE_BASE;
	*pdeppp = alternate_pdes;
	KASSERT(l->l_ncsw == ncsw);
 out:
 	/*
 	 * might have blocked, need to retry?
 	 */
	if (l->l_ncsw != ncsw) {
 unlock_and_retry:
		crit_exit();
	    	if (ourpmap != NULL)
			mutex_exit(&ourpmap->pm_lock);
		mutex_exit(&pmap->pm_lock);
		goto retry;
	}

	return;
}

/*
 * pmap_unmap_ptes: unlock the PTE mapping of "pmap"
 */

static void
pmap_unmap_ptes(struct pmap *pmap, struct pmap *pmap2)
{

	if (pmap == pmap_kernel()) {
		return;
	}
	if (pmap2 == NULL) {
		mutex_exit(&pmap->pm_lock);
	} else {
		KASSERT(curcpu()->ci_pmap == pmap2);
#if defined(MULTIPROCESSOR)
		*APDP_PDE = 0;
		pmap_apte_flush(pmap2);
#endif
		COUNT(apdp_pde_unmap);
		mutex_exit(&pmap->pm_lock);
		mutex_exit(&pmap2->pm_lock);
	}

	/* re-enable preemption */
	crit_exit();
}

inline static void
pmap_exec_account(struct pmap *pm, vaddr_t va, pt_entry_t opte, pt_entry_t npte)
{

#if !defined(__x86_64__)
	if (curproc == NULL || curproc->p_vmspace == NULL ||
	    pm != vm_map_pmap(&curproc->p_vmspace->vm_map))
		return;

	if ((opte ^ npte) & PG_X)
		pmap_update_pg(va);

	/*
	 * Executability was removed on the last executable change.
	 * Reset the code segment to something conservative and
	 * let the trap handler deal with setting the right limit.
	 * We can't do that because of locking constraints on the vm map.
	 */

	if ((opte & PG_X) && (npte & PG_X) == 0 && va == pm->pm_hiexec) {
		struct trapframe *tf = curlwp->l_md.md_regs;
		struct pcb *pcb = &curlwp->l_addr->u_pcb;

		pcb->pcb_cs = tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
		pm->pm_hiexec = I386_MAX_EXE_ADDR;
	}
#endif /* !defined(__x86_64__) */
}

#if !defined(__x86_64__)
/*
 * Fixup the code segment to cover all potential executable mappings.
 * returns 0 if no changes to the code segment were made.
 */

int
pmap_exec_fixup(struct vm_map *map, struct trapframe *tf, struct pcb *pcb)
{
	struct vm_map_entry *ent;
	struct pmap *pm = vm_map_pmap(map);
	vaddr_t va = 0;

	vm_map_lock_read(map);
	for (ent = (&map->header)->next; ent != &map->header; ent = ent->next) {

		/*
		 * This entry has greater va than the entries before.
		 * We need to make it point to the last page, not past it.
		 */

		if (ent->protection & VM_PROT_EXECUTE)
			va = trunc_page(ent->end) - PAGE_SIZE;
	}
	vm_map_unlock_read(map);
	if (va == pm->pm_hiexec && tf->tf_cs == GSEL(GUCODEBIG_SEL, SEL_UPL))
		return (0);

	pm->pm_hiexec = va;
	if (pm->pm_hiexec > I386_MAX_EXE_ADDR) {
		pcb->pcb_cs = tf->tf_cs = GSEL(GUCODEBIG_SEL, SEL_UPL);
	} else {
		pcb->pcb_cs = tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
		return (0);
	}
	return (1);
}
#endif /* !defined(__x86_64__) */

/*
 * p m a p   k e n t e r   f u n c t i o n s
 *
 * functions to quickly enter/remove pages from the kernel address
 * space.   pmap_kremove is exported to MI kernel.  we make use of
 * the recursive PTE mappings.
 */

/*
 * pmap_kenter_pa: enter a kernel mapping without R/M (pv_entry) tracking
 *
 * => no need to lock anything, assume va is already allocated
 * => should be faster than normal pmap enter function
 */

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte, opte, npte;

	if (va < VM_MIN_KERNEL_ADDRESS)
		pte = vtopte(va);
	else
		pte = kvtopte(va);

	npte = pa | protection_codes[prot] | PG_V | pmap_pg_g;
	opte = pmap_pte_set(pte, npte); /* zap! */
#if defined(DIAGNOSTIC)
	/* XXX For now... */
	if (opte & PG_PS)
		panic("pmap_kenter_pa: PG_PS");
#endif
	if ((opte & (PG_V | PG_U)) == (PG_V | PG_U)) {
		/* This should not happen, so no need to batch updates. */
		crit_enter();
		pmap_tlb_shootdown(pmap_kernel(), va, 0, opte);
		crit_exit();
	}
}

#if defined(__x86_64__)
/*
 * Change protection for a virtual address. Local for a CPU only, don't
 * care about TLB shootdowns.
 */
void
pmap_changeprot_local(vaddr_t va, vm_prot_t prot)
{
	pt_entry_t *pte, opte;

	if (va < VM_MIN_KERNEL_ADDRESS)
		pte = vtopte(va);
	else
		pte = kvtopte(va);

	opte = *pte;

	if ((prot & VM_PROT_WRITE) != 0)
		*pte |= PG_RW;
	else
		*pte &= ~PG_RW;

	if (opte != *pte)
		invlpg(va);
}
#endif /* defined(__x86_64__) */

/*
 * pmap_kremove: remove a kernel mapping(s) without R/M (pv_entry) tracking
 *
 * => no need to lock anything
 * => caller must dispose of any vm_page mapped in the va range
 * => note: not an inline function
 * => we assume the va is page aligned and the len is a multiple of PAGE_SIZE
 * => we assume kernel only unmaps valid addresses and thus don't bother
 *    checking the valid bit before doing TLB flushing
 * => must be followed by call to pmap_update() before reuse of page
 */

void
pmap_kremove(vaddr_t sva, vsize_t len)
{
	pt_entry_t *pte, xpte;
	vaddr_t va, eva;

	eva = sva + len;
	xpte = 0;

	for (va = sva; va < eva; va += PAGE_SIZE) {
		if (va < VM_MIN_KERNEL_ADDRESS)
			pte = vtopte(va);
		else
			pte = kvtopte(va);
		xpte |= pmap_pte_set(pte, 0); /* zap! */
#if defined(DIAGNOSTIC)
		/* XXX For now... */
		if (xpte & PG_PS)
			panic("pmap_kremove: PG_PS");
		if (xpte & PG_PVLIST)
			panic("pmap_kremove: PG_PVLIST mapping for 0x%lx",
			      va);
#endif
	}
	if ((xpte & (PG_V | PG_U)) == (PG_V | PG_U)) {
		crit_enter();
		pmap_tlb_shootdown(pmap_kernel(), sva, eva, xpte);
		crit_exit();
	}
}

/*
 * p m a p   i n i t   f u n c t i o n s
 *
 * pmap_bootstrap and pmap_init are called during system startup
 * to init the pmap module.   pmap_bootstrap() does a low level
 * init just to get things rolling.   pmap_init() finishes the job.
 */

/*
 * pmap_bootstrap: get the system in a state where it can run with VM
 *	properly enabled (called before main()).   the VM system is
 *      fully init'd later...
 *
 * => on i386, locore.s has already enabled the MMU by allocating
 *	a PDP for the kernel, and nkpde PTP's for the kernel.
 * => kva_start is the first free virtual address in kernel space
 */

void
pmap_bootstrap(vaddr_t kva_start)
{
	vaddr_t kva;
	vaddr_t kva_end;
	struct pmap *kpm;
	pt_entry_t *pte;
	int i;
	unsigned long p1i;
	pt_entry_t pg_nx = (cpu_feature & CPUID_NOX ? PG_NX : 0);

	/*
	 * set up our local static global vars that keep track of the
	 * usage of KVM before kernel_map is set up
	 */

	virtual_avail = kva_start;		/* first free KVA */
	virtual_end = VM_MAX_KERNEL_ADDRESS;	/* last KVA */

	/*
	 * set up protection_codes: we need to be able to convert from
	 * a MI protection code (some combo of VM_PROT...) to something
	 * we can jam into a i386 PTE.
	 */

	protection_codes[VM_PROT_NONE] = pg_nx;			/* --- */
	protection_codes[VM_PROT_EXECUTE] = PG_RO | PG_X;	/* --x */
	protection_codes[VM_PROT_READ] = PG_RO | pg_nx;		/* -r- */
	protection_codes[VM_PROT_READ|VM_PROT_EXECUTE] = PG_RO | PG_X;/* -rx */
	protection_codes[VM_PROT_WRITE] = PG_RW | pg_nx;	/* w-- */
	protection_codes[VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW | PG_X;/* w-x */
	protection_codes[VM_PROT_WRITE|VM_PROT_READ] = PG_RW | pg_nx;
								/* wr- */
	protection_codes[VM_PROT_ALL] = PG_RW | PG_X;		/* wrx */

	/*
	 * now we init the kernel's pmap
	 *
	 * the kernel pmap's pm_obj is not used for much.   however, in
	 * user pmaps the pm_obj contains the list of active PTPs.
	 * the pm_obj currently does not have a pager.   it might be possible
	 * to add a pager that would allow a process to read-only mmap its
	 * own page tables (fast user level vtophys?).   this may or may not
	 * be useful.
	 */

	kpm = pmap_kernel();
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		UVM_OBJ_INIT(&kpm->pm_obj[i], NULL, 1);
		kpm->pm_ptphint[i] = NULL;
	}
	memset(&kpm->pm_list, 0, sizeof(kpm->pm_list));  /* pm_list not used */
	kpm->pm_pdir = (pd_entry_t *)(lwp0.l_addr->u_pcb.pcb_cr3 + KERNBASE);
	kpm->pm_pdirpa = (paddr_t) lwp0.l_addr->u_pcb.pcb_cr3;
	kpm->pm_stats.wired_count = kpm->pm_stats.resident_count =
		x86_btop(kva_start - VM_MIN_KERNEL_ADDRESS);

	/*
	 * the above is just a rough estimate and not critical to the proper
	 * operation of the system.
	 */

	/*
	 * Begin to enable global TLB entries if they are supported.
	 * The G bit has no effect until the CR4_PGE bit is set in CR4,
	 * which happens in cpu_init(), which is run on each cpu
	 * (and happens later)
	 */

	if (cpu_feature & CPUID_PGE) {
		pmap_pg_g = PG_G;		/* enable software */

		/* add PG_G attribute to already mapped kernel pages */
		if (KERNBASE == VM_MIN_KERNEL_ADDRESS) {
			kva_end = virtual_avail;
		} else {
			kva_end = roundup((vaddr_t)&end, PAGE_SIZE);
		}
		for (kva = KERNBASE; kva < kva_end; kva += PAGE_SIZE) {
			p1i = pl1_i(kva);
			if (pmap_valid_entry(PTE_BASE[p1i]))
				PTE_BASE[p1i] |= PG_G;
		}
	}

	/*
	 * enable large pages if they are supported.
	 */

	if (cpu_feature & CPUID_PSE) {
		paddr_t pa;
		pd_entry_t *pde;
		extern char __data_start;

		lcr4(rcr4() | CR4_PSE);	/* enable hardware (via %cr4) */
		pmap_largepages = 1;	/* enable software */

		/*
		 * the TLB must be flushed after enabling large pages
		 * on Pentium CPUs, according to section 3.6.2.2 of
		 * "Intel Architecture Software Developer's Manual,
		 * Volume 3: System Programming".
		 */
		tlbflush();

		/*
		 * now, remap the kernel text using large pages.  we
		 * assume that the linker has properly aligned the
		 * .data segment to a NBPD_L2 boundary.
		 */
		kva_end = roundup((vaddr_t)&__data_start, NBPD_L1);
		for (pa = 0, kva = KERNBASE; kva + NBPD_L2 <= kva_end;
		     kva += NBPD_L2, pa += NBPD_L2) {
			pde = &L2_BASE[pl2_i(kva)];
			*pde = pa | pmap_pg_g | PG_PS |
			    PG_KR | PG_V;	/* zap! */
			tlbflush();
		}
#if defined(DEBUG)
		printf("kernel text is mapped with "
		    "%lu large pages and %lu normal pages\n",
		    (unsigned long)howmany(kva - KERNBASE, NBPD_L2),
		    (unsigned long)howmany((vaddr_t)&__data_start - kva,
		    NBPD_L1));
#endif /* defined(DEBUG) */
	}

	if (VM_MIN_KERNEL_ADDRESS != KERNBASE) {
		/*
		 * zero_pte is stuck at the end of mapped space for the kernel
		 * image (disjunct from kva space). This is done so that it
		 * can safely be used in pmap_growkernel (pmap_get_physpage),
		 * when it's called for the first time.
		 * XXXfvdl fix this for MULTIPROCESSOR later.
		 */

		early_zerop = (void *)(KERNBASE + NKL2_KIMG_ENTRIES * NBPD_L2);
		early_zero_pte = PTE_BASE + pl1_i((unsigned long)early_zerop);
	}

	/*
	 * now we allocate the "special" VAs which are used for tmp mappings
	 * by the pmap (and other modules).    we allocate the VAs by advancing
	 * virtual_avail (note that there are no pages mapped at these VAs).
	 * we find the PTE that maps the allocated VA via the linear PTE
	 * mapping.
	 */

	pte = PTE_BASE + pl1_i(virtual_avail);

#ifdef MULTIPROCESSOR
	/*
	 * Waste some VA space to avoid false sharing of cache lines
	 * for page table pages: Give each possible CPU a cache line
	 * of PTE's (8) to play with, though we only need 4.  We could
	 * recycle some of this waste by putting the idle stacks here
	 * as well; we could waste less space if we knew the largest
	 * CPU ID beforehand.
	 */
	csrcp = (char *) virtual_avail;  csrc_pte = pte;

	cdstp = (char *) virtual_avail+PAGE_SIZE;  cdst_pte = pte+1;

	zerop = (char *) virtual_avail+PAGE_SIZE*2;  zero_pte = pte+2;

	ptpp = (char *) virtual_avail+PAGE_SIZE*3;  ptp_pte = pte+3;

	virtual_avail += PAGE_SIZE * X86_MAXPROCS * NPTECL;
	pte += X86_MAXPROCS * NPTECL;
#else
	csrcp = (void *) virtual_avail;  csrc_pte = pte;	/* allocate */
	virtual_avail += PAGE_SIZE; pte++;			/* advance */

	cdstp = (void *) virtual_avail;  cdst_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	zerop = (void *) virtual_avail;  zero_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	ptpp = (void *) virtual_avail;  ptp_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;
#endif

	if (VM_MIN_KERNEL_ADDRESS == KERNBASE) {
		early_zerop = zerop;
		early_zero_pte = zero_pte;
	}

	/*
	 * Nothing after this point actually needs pte;
	 */
	pte = (void *)0xdeadbeef;

	/* XXX: vmmap used by mem.c... should be uvm_map_reserve */
	/* XXXfvdl PTEs not needed here */
	vmmap = (char *)virtual_avail;			/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;

	idt_vaddr = virtual_avail;			/* don't need pte */
	idt_paddr = avail_start;			/* steal a page */
#if defined(__x86_64__)
	virtual_avail += 2 * PAGE_SIZE; pte += 2;
	avail_start += 2 * PAGE_SIZE;
#else /* defined(__x86_64__) */
	virtual_avail += PAGE_SIZE; pte++;
	avail_start += PAGE_SIZE;
#endif /* defined(__x86_64__) */

#if defined(I586_CPU)
	/* pentium f00f bug stuff */
	pentium_idt_vaddr = virtual_avail;		/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;
#endif

#ifdef _LP64
	/*
	 * Grab a page below 4G for things that need it (i.e.
	 * having an initial %cr3 for the MP trampoline).
	 */
	lo32_vaddr = virtual_avail;
	virtual_avail += PAGE_SIZE; pte++;
	lo32_paddr = avail_start;
	avail_start += PAGE_SIZE;
#endif

	/*
	 * now we reserve some VM for mapping pages when doing a crash dump
	 */

	virtual_avail = reserve_dumppages(virtual_avail);

	/*
	 * init the static-global locks and global lists.
	 *
	 * => pventry::pvh_lock (initialized elsewhere) must also be
	 *      a spin lock, again at IPL_VM to prevent deadlock, and
	 *	again is never taken from interrupt context.
	 */

	rw_init(&pmap_main_lock);
	mutex_init(&pmaps_lock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&pmaps);
	pmap_cpu_init_early(curcpu());

	/*
	 * initialize the pmap pool.
	 */

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr, IPL_NONE);

	/*
	 * initialize the PDE pool and cache.
	 */

	pool_init(&pmap_pdp_pool, PAGE_SIZE, 0, 0, 0, "pdppl",
	    &pool_allocator_nointr, IPL_NONE);
	pool_cache_init(&pmap_pdp_cache, &pmap_pdp_pool,
	    pmap_pdp_ctor, NULL, NULL);

	/*
	 * ensure the TLB is sync'd with reality by flushing it...
	 */

	tlbflush();

	/*
	 * calculate pmap_maxkvaddr from nkptp[].
	 */

	kva = VM_MIN_KERNEL_ADDRESS;
	for (i = PTP_LEVELS - 1; i >= 1; i--) {
		kva += nkptp[i] * nbpd[i];
	}
	pmap_maxkvaddr = kva;
}

#if defined(__x86_64__)
/*
 * Pre-allocate PTPs for low memory, so that 1:1 mappings for various
 * trampoline code can be entered.
 */
void
pmap_prealloc_lowmem_ptps(void)
{
	pd_entry_t *pdes;
	int level;
	paddr_t newp;

	pdes = pmap_kernel()->pm_pdir;
	level = PTP_LEVELS;
	for (;;) {
		newp = avail_start;
		avail_start += PAGE_SIZE;
		*early_zero_pte = (newp & PG_FRAME) | PG_V | PG_RW;
		pmap_update_pg((vaddr_t)early_zerop);
		memset(early_zerop, 0, PAGE_SIZE);
		pdes[pl_i(0, level)] = (newp & PG_FRAME) | PG_V | PG_RW;
		level--;
		if (level <= 1)
			break;
		pdes = normal_pdes[level - 2];
	}
}
#endif /* defined(__x86_64__) */

/*
 * pmap_init: called from uvm_init, our job is to get the pmap
 * system ready to manage mappings...
 */

void
pmap_init(void)
{

	/*
	 * done: pmap module is up (and ready for business)
	 */

	pmap_initialized = true;
}

/*
 * pmap_cpu_init_early: perform early per-CPU initialization.
 */

void
pmap_cpu_init_early(struct cpu_info *ci)
{
	struct pmap_cpu *pc;
	static uint8_t pmap_cpu_alloc;

	pc = &pmap_cpu[pmap_cpu_alloc++].pc;
	ci->ci_pmap_cpu = pc;

 	/*
	 * Initalize the PV freelist.
	 */
	mutex_init(&pc->pc_pv_lock, MUTEX_DEFAULT, IPL_NONE);
	TAILQ_INIT(&pc->pc_pv_freepages);
	TAILQ_INIT(&pc->pc_pv_unusedpgs);
}

/*
 * pmap_cpu_init_late: perform late per-CPU initialization.
 */

void
pmap_cpu_init_late(struct cpu_info *ci)
{

	if (ci == &cpu_info_primary)
		evcnt_attach_dynamic(&pmap_tlb_evcnt, EVCNT_TYPE_INTR,
		    NULL, "global", "TLB IPI");
	evcnt_attach_dynamic(&ci->ci_tlb_evcnt, EVCNT_TYPE_INTR,
	    NULL, ci->ci_dev->dv_xname, "TLB IPI");
}

/*
 * p v _ e n t r y   f u n c t i o n s
 */

/*
 * pv_entry allocation functions:
 *   the main pv_entry allocation functions are:
 *     pmap_alloc_pv: allocate a pv_entry structure
 *     pmap_free_pv: free one pv_entry
 *     pmap_free_pvs: free a list of pv_entrys
 *
 * the rest are helper functions
 */

/*
 * pmap_alloc_pv: function to allocate a pv_entry structure
 * => we lock pc_pv_lock
 * => if we fail, we call out to pmap_alloc_pvpage
 * => 3 modes:
 *    ALLOCPV_NEED   = we really need a pv_entry, even if we have to steal it
 *    ALLOCPV_TRY    = we want a pv_entry, but not enough to steal
 *    ALLOCPV_NONEED = we are trying to grow our free list, don't really need
 *			one now
 *
 * "try" is for optional functions like pmap_copy().
 */

static struct pv_entry *
pmap_alloc_pv(struct pmap *pmap, int mode)
{
	struct pv_page *pvpage;
	struct pv_entry *pv;
	struct pmap_cpu *pc;

	pc = curcpu()->ci_pmap_cpu;
	mutex_enter(&pc->pc_pv_lock);

	pvpage = TAILQ_FIRST(&pc->pc_pv_freepages);
	if (pvpage != NULL) {
		pvpage->pvinfo.pvpi_nfree--;
		if (pvpage->pvinfo.pvpi_nfree == 0) {
			/* nothing left in this one? */
			TAILQ_REMOVE(&pc->pc_pv_freepages, pvpage,
			    pvinfo.pvpi_list);
		}
		pv = pvpage->pvinfo.pvpi_pvfree;
		KASSERT(pv);
		pvpage->pvinfo.pvpi_pvfree = SPLAY_RIGHT(pv, pv_node);
		pc->pc_pv_nfpvents--;  /* took one from pool */
	} else {
		pv = NULL;		/* need more of them */
	}

	/*
	 * if below low water mark or we didn't get a pv_entry we try and
	 * create more pv_entrys ...
	 */

	if (pc->pc_pv_nfpvents < PVE_LOWAT || pv == NULL) {
		if (pv == NULL)
			pv = pmap_alloc_pvpage(pc, pmap,
			    (mode == ALLOCPV_TRY) ? mode : ALLOCPV_NEED);
		else
			(void) pmap_alloc_pvpage(pc, pmap, ALLOCPV_NONEED);
	}
	mutex_exit(&pc->pc_pv_lock);

	if (pv != NULL)
		pv->pv_alloc_cpu = pc;

	return(pv);
}

/*
 * pmap_alloc_pvpage: maybe allocate a new pvpage
 *
 * if need_entry is false: try and allocate a new pv_page
 * if need_entry is true: try and allocate a new pv_page and return a
 *	new pv_entry from it.   if we are unable to allocate a pv_page
 *	we make a last ditch effort to steal a pv_page from some other
 *	mapping.    if that fails, we panic...
 *
 * => we assume that the caller holds pc_pv_lock
 */

static struct pv_entry *
pmap_alloc_pvpage(struct pmap_cpu *pc, struct pmap *pmap, int mode)
{
	struct pv_page *pvpage;
	struct pv_entry *pv;

	KASSERT(mutex_owned(&pc->pc_pv_lock));

	/*
	 * if we need_entry and we've got unused pv_pages, allocate from there
	 */

	pvpage = TAILQ_FIRST(&pc->pc_pv_unusedpgs);
	if (mode != ALLOCPV_NONEED && pvpage != NULL) {

		/* move it to pv_freepages list */
		TAILQ_REMOVE(&pc->pc_pv_unusedpgs, pvpage,
		    pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pc->pc_pv_freepages, pvpage,
		    pvinfo.pvpi_list);

		/* allocate a pv_entry */
		pvpage->pvinfo.pvpi_nfree--;	/* can't go to zero */
		pv = pvpage->pvinfo.pvpi_pvfree;
		KASSERT(pv);
		pvpage->pvinfo.pvpi_pvfree = SPLAY_RIGHT(pv, pv_node);
		pc->pc_pv_nfpvents--;  /* took one from pool */
		return(pv);
	}

	mutex_exit(&pc->pc_pv_lock);
	pvpage = (struct pv_page *)uvm_km_alloc(kmem_map, PAGE_SIZE, 0,
	    UVM_KMF_NOWAIT|UVM_KMF_WIRED);
	mutex_enter(&pc->pc_pv_lock);

	if (pvpage == NULL)
		return NULL;

	return (pmap_add_pvpage(pc, pvpage, mode != ALLOCPV_NONEED));
}

/*
 * pmap_add_pvpage: add a pv_page's pv_entrys to the free list
 *
 * => caller must hold pc_pv_lock
 * => if need_entry is true, we allocate and return one pv_entry
 */

static struct pv_entry *
pmap_add_pvpage(struct pmap_cpu *pc, struct pv_page *pvp, bool need_entry)
{
	int tofree, lcv;

	KASSERT(mutex_owned(&pc->pc_pv_lock));

	/* do we need to return one? */
	tofree = (need_entry) ? PVE_PER_PVPAGE - 1 : PVE_PER_PVPAGE;

	pvp->pvinfo.pvpi_pvfree = NULL;
	pvp->pvinfo.pvpi_nfree = tofree;
	for (lcv = 0 ; lcv < tofree ; lcv++) {
		SPLAY_RIGHT(&pvp->pvents[lcv], pv_node) =
			pvp->pvinfo.pvpi_pvfree;
		pvp->pvinfo.pvpi_pvfree = &pvp->pvents[lcv];
	}
	if (need_entry)
		TAILQ_INSERT_TAIL(&pc->pc_pv_freepages, pvp, pvinfo.pvpi_list);
	else
		TAILQ_INSERT_TAIL(&pc->pc_pv_unusedpgs, pvp, pvinfo.pvpi_list);
	pc->pc_pv_nfpvents += tofree;

	return ((need_entry) ? &pvp->pvents[lcv] : NULL);
}

/*
 * pmap_free_pv_doit: actually free a pv_entry
 *
 * => do not call this directly!  instead use either
 *    1. pmap_free_pv ==> free a single pv_entry
 *    2. pmap_free_pvs => free a list of pv_entrys
 * => we must be holding pc_pv_lock
 */

static void
pmap_free_pv_doit(struct pmap *pmap, struct pv_entry *pv)
{
	struct pv_page *pvp;
	struct pmap_cpu *pc;

	pvp = (struct pv_page *)x86_trunc_page(pv);
	pc = pv->pv_alloc_cpu;

	KASSERT(mutex_owned(&pc->pc_pv_lock));

	pc->pc_pv_nfpvents++;
	pvp->pvinfo.pvpi_nfree++;

	/* nfree == 1 => fully allocated page just became partly allocated */
	if (pvp->pvinfo.pvpi_nfree == 1) {
		TAILQ_INSERT_HEAD(&pc->pc_pv_freepages, pvp, pvinfo.pvpi_list);
	}

	/* free it */
	SPLAY_RIGHT(pv, pv_node) = pvp->pvinfo.pvpi_pvfree;
	pvp->pvinfo.pvpi_pvfree = pv;

	/*
	 * are all pv_page's pv_entry's free?  move it to unused queue.
	 */

	if (pvp->pvinfo.pvpi_nfree == PVE_PER_PVPAGE) {
		TAILQ_REMOVE(&pc->pc_pv_freepages, pvp, pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pc->pc_pv_unusedpgs, pvp, pvinfo.pvpi_list);
	}

	/*
	 * Can't free the PV page if the PV entries were associated with
	 * the kernel pmap; the pmap is already locked.
	 */
	if (pc->pc_pv_nfpvents > PVE_HIWAT &&
	    TAILQ_FIRST(&pc->pc_pv_unusedpgs) != NULL &&
	    pmap != pmap_kernel())
		pmap_free_pvpage(pc);
}

/*
 * pmap_free_pv: free a single pv_entry
 *
 * => we gain pc_pv_lock
 */

static void
pmap_free_pv(struct pmap *pmap, struct pv_entry *pv)
{
	struct pmap_cpu *pc;

	pc = pv->pv_alloc_cpu;

	mutex_enter(&pc->pc_pv_lock);
	pmap_free_pv_doit(pmap, pv);
	mutex_exit(&pc->pc_pv_lock);
}

/*
 * pmap_free_pvs: free a list of pv_entrys
 *
 * => we gain pc_pv_lock
 */

static void
pmap_free_pvs(struct pmap *pmap, struct pv_entry *pvs)
{
	struct pv_entry *nextpv;
	kmutex_t *lock, *lock2;

	if (pvs == NULL)
		return;

	lock = &pvs->pv_alloc_cpu->pc_pv_lock;
	mutex_enter(lock);
	for ( /* null */ ; pvs != NULL ; pvs = nextpv) {
		lock2 = &pvs->pv_alloc_cpu->pc_pv_lock;
		if (__predict_false(lock != lock2)) {
			mutex_exit(lock);
			lock = lock2;
			mutex_enter(lock);
		}
		nextpv = SPLAY_RIGHT(pvs, pv_node);
		pmap_free_pv_doit(pmap, pvs);
	}
	mutex_exit(lock);
}

/*
 * pmap_free_pvpage: try and free an unused pv_page structure
 *
 * => must be called with the correct lock held
 * => assume that there is a page on the pv_unusedpgs list
 */

static void
pmap_free_pvpage(struct pmap_cpu *pc)
{
	struct pv_page *pvp;

	KASSERT(mutex_owned(&pc->pc_pv_lock));

	pvp = TAILQ_FIRST(&pc->pc_pv_unusedpgs);
	/* remove pvp from pv_unusedpgs */
	TAILQ_REMOVE(&pc->pc_pv_unusedpgs, pvp, pvinfo.pvpi_list);

	pc->pc_pv_nfpvents -= PVE_PER_PVPAGE;  /* update free count */

	mutex_exit(&pc->pc_pv_lock);
	uvm_km_free(kmem_map, (vaddr_t)pvp, PAGE_SIZE, UVM_KMF_WIRED);
	mutex_enter(&pc->pc_pv_lock);
}

/*
 * pmap_lock_pvhs: Lock pvh1 and optional pvh2
 *                 Observe locking order when locking both pvhs
 */

static void
pmap_lock_pvhs(struct pv_head *pvh1, struct pv_head *pvh2)
{

	if (pvh2 == NULL) {
		mutex_spin_enter(&pvh1->pvh_lock);
		return;
	}

	if (pvh1 < pvh2) {
		mutex_spin_enter(&pvh1->pvh_lock);
		mutex_spin_enter(&pvh2->pvh_lock);
	} else {
		mutex_spin_enter(&pvh2->pvh_lock);
		mutex_spin_enter(&pvh1->pvh_lock);
	}
}


/*
 * main pv_entry manipulation functions:
 *   pmap_enter_pv: enter a mapping onto a pv_head list
 *   pmap_remove_pv: remove a mappiing from a pv_head list
 *
 * NOTE: Both pmap_enter_pv and pmap_remove_pv expect the caller to lock 
 *       the pvh before calling
 */

/*
 * pmap_enter_pv: enter a mapping onto a pv_head lst
 *
 * => caller should hold the proper lock on pmap_main_lock
 * => caller should have pmap locked
 * => caller should have the pv_head locked
 * => caller should adjust ptp's wire_count before calling
 */

static void
pmap_enter_pv(struct pv_head *pvh,
	      struct pv_entry *pve,	/* preallocated pve for us to use */
	      struct pmap *pmap,
	      vaddr_t va,
	      struct vm_page *ptp)	/* PTP in pmap that maps this VA */
{
	pve->pv_pmap = pmap;
	pve->pv_va = va;
	pve->pv_ptp = ptp;			/* NULL for kernel pmap */
	SPLAY_INSERT(pvtree, &pvh->pvh_root, pve); /* add to locked list */
}

/*
 * pmap_remove_pv: try to remove a mapping from a pv_list
 *
 * => caller should hold proper lock on pmap_main_lock
 * => pmap should be locked
 * => caller should hold lock on pv_head [so that attrs can be adjusted]
 * => caller should adjust ptp's wire_count and free PTP if needed
 * => we return the removed pve
 */

static struct pv_entry *
pmap_remove_pv(struct pv_head *pvh, struct pmap *pmap, vaddr_t va)
{
	struct pv_entry tmp, *pve;

	tmp.pv_pmap = pmap;
	tmp.pv_va = va;
	pve = SPLAY_FIND(pvtree, &pvh->pvh_root, &tmp);
	if (pve == NULL)
		return (NULL);
	SPLAY_REMOVE(pvtree, &pvh->pvh_root, pve);
	return(pve);				/* return removed pve */
}

/*
 * p t p   f u n c t i o n s
 */

static inline struct vm_page *
pmap_find_ptp(struct pmap *pmap, vaddr_t va, paddr_t pa, int level)
{
	int lidx = level - 1;
	struct vm_page *pg;

	if (pa != (paddr_t)-1 && pmap->pm_ptphint[lidx] &&
	    pa == VM_PAGE_TO_PHYS(pmap->pm_ptphint[lidx])) {
		return (pmap->pm_ptphint[lidx]);
	}
	if (lidx == 0)
		pg = uvm_pagelookup(&pmap->pm_obj[lidx], ptp_va2o(va, level));
	else {
		mutex_enter(&pmap->pm_obj[lidx].vmobjlock);
		pg = uvm_pagelookup(&pmap->pm_obj[lidx], ptp_va2o(va, level));
		mutex_exit(&pmap->pm_obj[lidx].vmobjlock);
	}
	return pg;
}

static inline void
pmap_freepage(struct pmap *pmap, struct vm_page *ptp, int level,
    struct vm_page **empty_ptps)
{
	int lidx;
	struct uvm_object *obj;

	lidx = level - 1;

	obj = &pmap->pm_obj[lidx];
	pmap->pm_stats.resident_count--;
	if (lidx != 0)
		mutex_enter(&obj->vmobjlock);
	if (pmap->pm_ptphint[lidx] == ptp)
		pmap->pm_ptphint[lidx] = TAILQ_FIRST(&obj->memq);
	ptp->wire_count = 0;
	uvm_pagerealloc(ptp, NULL, 0);
	ptp->flags |= PG_ZERO;
	ptp->mdpage.mp_link = *empty_ptps;
	*empty_ptps = ptp;
	if (lidx != 0)
		mutex_exit(&obj->vmobjlock);
}

static void
pmap_free_ptp(struct pmap *pmap, struct vm_page *ptp, vaddr_t va,
	      pt_entry_t *ptes, pd_entry_t **pdes, struct vm_page **empty_ptps)
{
	unsigned long index;
	int level;
	vaddr_t invaladdr;
	pd_entry_t opde;
	struct pmap *curpmap = vm_map_pmap(&curlwp->l_proc->p_vmspace->vm_map);

	level = 1;
	do {
		pmap_freepage(pmap, ptp, level, empty_ptps);
		index = pl_i(va, level + 1);
		opde = pmap_pte_set(&pdes[level - 1][index], 0);
		invaladdr = level == 1 ? (vaddr_t)ptes :
		    (vaddr_t)pdes[level - 2];
		pmap_tlb_shootdown(curpmap, invaladdr + index * PAGE_SIZE,
		    0, opde);
#if defined(MULTIPROCESSOR)
		invaladdr = level == 1 ? (vaddr_t)PTE_BASE :
		    (vaddr_t)normal_pdes[level - 2];
		pmap_tlb_shootdown(pmap, invaladdr + index * PAGE_SIZE,
		    0, opde);
#endif
		if (level < PTP_LEVELS - 1) {
			ptp = pmap_find_ptp(pmap, va, (paddr_t)-1, level + 1);
			ptp->wire_count--;
			if (ptp->wire_count > 1)
				break;
		}
	} while (++level < PTP_LEVELS);
}

/*
 * pmap_get_ptp: get a PTP (if there isn't one, allocate a new one)
 *
 * => pmap should NOT be pmap_kernel()
 * => pmap should be locked
 */

static struct vm_page *
pmap_get_ptp(struct pmap *pmap, vaddr_t va, pd_entry_t **pdes)
{
	struct vm_page *ptp, *pptp;
	int i;
	unsigned long index;
	pd_entry_t *pva;
	paddr_t ppa, pa;
	struct uvm_object *obj;

	ptp = NULL;
	pa = (paddr_t)-1;

	/*
	 * Loop through all page table levels seeing if we need to
	 * add a new page to that level.
	 */
	for (i = PTP_LEVELS; i > 1; i--) {
		/*
		 * Save values from previous round.
		 */
		pptp = ptp;
		ppa = pa;

		index = pl_i(va, i);
		pva = pdes[i - 2];

		if (pmap_valid_entry(pva[index])) {
			ppa = pva[index] & PG_FRAME;
			ptp = NULL;
			continue;
		}

		obj = &pmap->pm_obj[i-2];
		/*
		 * XXX pm_obj[0] is pm_lock, which is already locked.
		 */
		if (i != 2)
			mutex_enter(&obj->vmobjlock);
		ptp = uvm_pagealloc(obj, ptp_va2o(va, i - 1), NULL,
		    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
		if (i != 2)
			mutex_exit(&obj->vmobjlock);

		if (ptp == NULL)
			return NULL;

		ptp->flags &= ~PG_BUSY; /* never busy */
		ptp->wire_count = 1;
		pmap->pm_ptphint[i - 2] = ptp;
		pa = VM_PAGE_TO_PHYS(ptp);
		pva[index] = (pd_entry_t) (pa | PG_u | PG_RW | PG_V);
		pmap->pm_stats.resident_count++;
		/*
		 * If we're not in the top level, increase the
		 * wire count of the parent page.
		 */
		if (i < PTP_LEVELS) {
			if (pptp == NULL)
				pptp = pmap_find_ptp(pmap, va, ppa, i);
#ifdef DIAGNOSTIC
			if (pptp == NULL)
				panic("pde page disappeared");
#endif
			pptp->wire_count++;
		}
	}

	/*
	 * ptp is not NULL if we just allocated a new ptp. If it's
	 * still NULL, we must look up the existing one.
	 */
	if (ptp == NULL) {
		ptp = pmap_find_ptp(pmap, va, ppa, 1);
#ifdef DIAGNOSTIC
		if (ptp == NULL) {
			printf("va %lx ppa %lx\n", (unsigned long)va,
			    (unsigned long)ppa);
			panic("pmap_get_ptp: unmanaged user PTP");
		}
#endif
	}

	pmap->pm_ptphint[0] = ptp;
	return(ptp);
}

/*
 * p m a p  l i f e c y c l e   f u n c t i o n s
 */

/*
 * pmap_pdp_ctor: constructor for the PDP cache.
 */

int
pmap_pdp_ctor(void *arg, void *object, int flags)
{
	pd_entry_t *pdir = object;
	paddr_t pdirpa = 0;	/* XXX: GCC */
	int npde;

	/*
	 * NOTE: The `pmap_lock' is held when the PDP is allocated.
	 */

	/* fetch the physical address of the page directory. */
	(void) pmap_extract(pmap_kernel(), (vaddr_t) pdir, &pdirpa);

	/* zero init area */
	memset(pdir, 0, PDIR_SLOT_PTE * sizeof(pd_entry_t));

	/* put in recursive PDE to map the PTEs */
	pdir[PDIR_SLOT_PTE] = pdirpa | PG_V | PG_KW;

	npde = nkptp[PTP_LEVELS - 1];

	/* put in kernel VM PDEs */
	memcpy(&pdir[PDIR_SLOT_KERN], &PDP_BASE[PDIR_SLOT_KERN],
	    npde * sizeof(pd_entry_t));

	/* zero the rest */
	memset(&pdir[PDIR_SLOT_KERN + npde], 0,
	    (NTOPLEVEL_PDES - (PDIR_SLOT_KERN + npde)) * sizeof(pd_entry_t));

	if (VM_MIN_KERNEL_ADDRESS != KERNBASE) {
		int idx = pl_i(KERNBASE, PTP_LEVELS);

		pdir[idx] = PDP_BASE[idx];
	}

	return (0);
}

/*
 * pmap_create: create a pmap
 *
 * => note: old pmap interface took a "size" args which allowed for
 *	the creation of "software only" pmaps (not in bsd).
 */

struct pmap *
pmap_create(void)
{
	struct pmap *pmap;
	int i;
	u_int gen;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	/* init uvm_object */
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		UVM_OBJ_INIT(&pmap->pm_obj[i], NULL, 1);
		pmap->pm_ptphint[i] = NULL;
	}
	pmap->pm_stats.wired_count = 0;
	pmap->pm_stats.resident_count = 1;	/* count the PDP allocd below */
#if !defined(__x86_64__)
	pmap->pm_hiexec = 0;
#endif /* !defined(__x86_64__) */
	pmap->pm_flags = 0;
	pmap->pm_cpus = 0;
	pmap->pm_kernel_cpus = 0;

	/* init the LDT */
	pmap->pm_ldt = NULL;
	pmap->pm_ldt_len = 0;
#if defined(__x86_64__)
	pmap->pm_ldt_sel = GSYSSEL(GLDT_SEL, SEL_KPL);
#else /* defined(__x86_64__) */
	pmap->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
#endif /* defined(__x86_64__) */

	/* allocate PDP */

	/*
	 * we need to lock pmaps_lock to prevent nkpde from changing on
	 * us.  note that there is no need to splvm to protect us from
	 * malloc since malloc allocates out of a submap and we should
	 * have already allocated kernel PTPs to cover the range...
	 *
	 * NOTE: WE MUST NOT BLOCK WHILE HOLDING THE `pmap_lock', nor
	 * must we call pmap_growkernel() while holding it!
	 */

 try_again:
	gen = pmap_pdp_cache_generation;
	pmap->pm_pdir = pool_cache_get(&pmap_pdp_cache, PR_WAITOK);

	mutex_enter(&pmaps_lock);

	if (gen != pmap_pdp_cache_generation) {
		mutex_exit(&pmaps_lock);
		pool_cache_destruct_object(&pmap_pdp_cache, pmap->pm_pdir);
		goto try_again;
	}

	pmap->pm_pdirpa = pmap->pm_pdir[PDIR_SLOT_PTE] & PG_FRAME;

	LIST_INSERT_HEAD(&pmaps, pmap, pm_list);

	mutex_exit(&pmaps_lock);

	return (pmap);
}

/*
 * pmap_destroy: drop reference count on pmap.   free pmap if
 *	reference count goes to zero.
 */

void
pmap_destroy(struct pmap *pmap)
{
	int refs;
	int i;
#ifdef DIAGNOSTIC
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
#endif /* DIAGNOSTIC */

	/*
	 * drop reference count
	 */

	mutex_enter(&pmap->pm_lock);
	refs = --pmap->pm_obj[0].uo_refs;
	mutex_exit(&pmap->pm_lock);
	if (refs > 0) {
		return;
	}

#ifdef DIAGNOSTIC
	for (CPU_INFO_FOREACH(cii, ci))
		if (ci->ci_pmap == pmap)
			panic("destroying pmap being used");
#endif /* DIAGNOSTIC */

	/*
	 * reference count is zero, free pmap resources and then free pmap.
	 */

	/*
	 * remove it from global list of pmaps
	 */

	mutex_enter(&pmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	mutex_exit(&pmaps_lock);

	/*
	 * destroyed pmap shouldn't have remaining PTPs
	 */

	for (i = 0; i < PTP_LEVELS - 1; i++) {
		KASSERT(pmap->pm_obj[i].uo_npages == 0);
		KASSERT(TAILQ_EMPTY(&pmap->pm_obj[i].memq));
	}

	/*
	 * MULTIPROCESSOR -- no need to flush out of other processors'
	 * APTE space because we do that in pmap_unmap_ptes().
	 */
	pool_cache_put(&pmap_pdp_cache, pmap->pm_pdir);

#ifdef USER_LDT
	if (pmap->pm_flags & PMF_USER_LDT) {
		/*
		 * no need to switch the LDT; this address space is gone,
		 * nothing is using it.
		 *
		 * No need to lock the pmap for ldt_free (or anything else),
		 * we're the last one to use it.
		 */
		ldt_free(pmap->pm_ldt_sel);
		uvm_km_free(kernel_map, (vaddr_t)pmap->pm_ldt,
		    pmap->pm_ldt_len * sizeof(union descriptor), UVM_KMF_WIRED);
	}
#endif

	for (i = 0; i < PTP_LEVELS - 1; i++)
		mutex_destroy(&pmap->pm_obj[i].vmobjlock);
	pool_put(&pmap_pmap_pool, pmap);
}

/*
 *	Add a reference to the specified pmap.
 */

void
pmap_reference(struct pmap *pmap)
{
	mutex_enter(&pmap->pm_lock);
	pmap->pm_obj[0].uo_refs++;
	mutex_exit(&pmap->pm_lock);
}

#if defined(PMAP_FORK)
/*
 * pmap_fork: perform any necessary data structure manipulation when
 * a VM space is forked.
 */

void
pmap_fork(struct pmap *pmap1, struct pmap *pmap2)
{
#ifdef USER_LDT
	union descriptor *new_ldt;
	size_t len;
	int sel;

 retry:
	if (pmap1->pm_flags & PMF_USER_LDT) {
		len = pmap1->pm_ldt_len * sizeof(union descriptor);
		new_ldt = (union descriptor *)uvm_km_alloc(kernel_map,
		    len, 0, UVM_KMF_WIRED);
		sel = ldt_alloc(new_ldt, len);
	} else {
		len = -1;
		new_ldt = NULL;
		sel = -1;
	}

	if ((uintptr_t) pmap1 < (uintptr_t) pmap2) {
		mutex_enter(&pmap1->pm_obj.vmobjlock);
		mutex_enter(&pmap2->pm_obj.vmobjlock);
	} else {
		mutex_enter(&pmap2->pm_obj.vmobjlock);
		mutex_enter(&pmap1->pm_obj.vmobjlock);
	}

 	/* Copy the LDT, if necessary. */
 	if (pmap1->pm_flags & PMF_USER_LDT) {
		if (len != pmap1->pm_ldt_len * sizeof(union descriptor)) {
			mutex_exit(&pmap2->pm_obj.vmobjlock);
			mutex_exit(&pmap1->pm_obj.vmobjlock);
			if (len != -1) {
				ldt_free(sel);
				uvm_km_free(kernel_map, (vaddr_t)new_ldt,
				    len, UVM_KMF_WIRED);
			}
			goto retry;
		}
  
		memcpy(new_ldt, pmap1->pm_ldt, len);
		pmap2->pm_ldt = new_ldt;
		pmap2->pm_ldt_len = pmap1->pm_ldt_len;
		pmap2->pm_flags |= PMF_USER_LDT;
		pmap2->pm_ldt_sel = sel;
		len = -1;
	}

	mutex_exit(&pmap2->pm_obj.vmobjlock);
	mutex_exit(&pmap1->pm_obj.vmobjlock);

	if (len != -1) {
		ldt_free(sel);
		uvm_km_free(kernel_map, (vaddr_t)new_ldt, len,
		    UVM_KMF_WIRED);
	}
#endif /* USER_LDT */
}
#endif /* PMAP_FORK */

#ifdef USER_LDT
/*
 * pmap_ldt_cleanup: if the pmap has a local LDT, deallocate it, and
 * restore the default.
 */

void
pmap_ldt_cleanup(struct lwp *l)
{
	struct pcb *pcb = &l->l_addr->u_pcb;
	pmap_t pmap = l->l_proc->p_vmspace->vm_map.pmap;
	union descriptor *old_ldt = NULL;
	size_t len = 0;
	int sel = -1;

	mutex_enter(&pmap->pm_lock);

	if (pmap->pm_flags & PMF_USER_LDT) {
		sel = pmap->pm_ldt_sel;
#if defined(__x86_64__)
		pmap->pm_ldt_sel = GSYSSEL(GLDT_SEL, SEL_KPL);
#else /* defined(__x86_64__) */
		pmap->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
#endif /* defined(__x86_64__) */
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
		if (l == curlwp)
			lldt(pcb->pcb_ldt_sel);
		old_ldt = pmap->pm_ldt;
		len = pmap->pm_ldt_len * sizeof(union descriptor);
		pmap->pm_ldt = NULL;
		pmap->pm_ldt_len = 0;
		pmap->pm_flags &= ~PMF_USER_LDT;
	}

	mutex_exit(&pmap->pm_lock);

	if (sel != -1)
		ldt_free(sel);
	if (old_ldt != NULL)
		uvm_km_free(kernel_map, (vaddr_t)old_ldt, len, UVM_KMF_WIRED);
}
#endif /* USER_LDT */

/*
 * pmap_activate: activate a process' pmap
 *
 * => must be called with kernel preemption disabled
 * => if lwp is the curlwp, then set ci_want_pmapload so that
 *    actual MMU context switch will be done by pmap_load() later
 */

void
pmap_activate(struct lwp *l)
{
	struct cpu_info *ci;
	struct pmap *pmap = vm_map_pmap(&l->l_proc->p_vmspace->vm_map);

	ci = curcpu();

	if (l == ci->ci_curlwp) {
		struct pcb *pcb;

		KASSERT(ci->ci_want_pmapload == 0);
		KASSERT(ci->ci_tlbstate != TLBSTATE_VALID);
#ifdef KSTACK_CHECK_DR0
		/*
		 * setup breakpoint on the top of stack
		 */
		if (l == &lwp0)
			dr0(0, 0, 0, 0);
		else
			dr0(KSTACK_LOWEST_ADDR(l), 1, 3, 1);
#endif

		/*
		 * no need to switch to kernel vmspace because
		 * it's a subset of any vmspace.
		 */

		if (pmap == pmap_kernel()) {
			ci->ci_want_pmapload = 0;
			return;
		}

		pcb = &l->l_addr->u_pcb;
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;

		ci->ci_want_pmapload = 1;

#if defined(__x86_64__)
		if (pcb->pcb_flags & PCB_GS64)
			wrmsr(MSR_KERNELGSBASE, pcb->pcb_gs);
		if (pcb->pcb_flags & PCB_FS64)
			wrmsr(MSR_FSBASE, pcb->pcb_fs);
#endif /* defined(__x86_64__) */
	}
}

/*
 * pmap_reactivate: try to regain reference to the pmap.
 *
 * => must be called with kernel preemption disabled
 */

static bool
pmap_reactivate(struct pmap *pmap)
{
	struct cpu_info *ci;
	uint32_t cpumask;
	bool result;	
	uint32_t oldcpus;

	ci = curcpu();
	cpumask = ci->ci_cpumask;

	KASSERT(pmap->pm_pdirpa == rcr3());

	/*
	 * if we still have a lazy reference to this pmap,
	 * we can assume that there was no tlb shootdown
	 * for this pmap in the meantime.
	 *
	 * the order of events here is important as we must
	 * synchronize with TLB shootdown interrupts.  declare
	 * interest in invalidations (TLBSTATE_VALID) and then
	 * check the cpumask, which the IPIs can change only
	 * when the state is !TLBSTATE_VALID.
	 */

	ci->ci_tlbstate = TLBSTATE_VALID;
	oldcpus = pmap->pm_cpus;
	x86_atomic_setbits_l(&pmap->pm_cpus, cpumask);
	KASSERT((pmap->pm_kernel_cpus & cpumask) != 0);
	if (oldcpus & cpumask) {
		/* got it */
		result = true;
	} else {
		result = false;
	}

	return result;
}

/*
 * pmap_load: actually switch pmap.  (fill in %cr3 and LDT info)
 */

void
pmap_load(void)
{
	struct cpu_info *ci;
	uint32_t cpumask;
	struct pmap *pmap;
	struct pmap *oldpmap;
	struct lwp *l;
	struct pcb *pcb;
	uint64_t ncsw;

	crit_enter();
	KASSERT(curcpu()->ci_want_pmapload);
 retry:
	ci = curcpu();
	cpumask = ci->ci_cpumask;

	/* should be able to take ipis. */
	KASSERT(ci->ci_ilevel < IPL_IPI); 
	KASSERT((read_psl() & PSL_I) != 0);

	l = ci->ci_curlwp;
	KASSERT(l != NULL);
	pmap = vm_map_pmap(&l->l_proc->p_vmspace->vm_map);
	KASSERT(pmap != pmap_kernel());
	oldpmap = ci->ci_pmap;

	pcb = &l->l_addr->u_pcb;
	/* loaded by pmap_activate */
	KASSERT(pcb->pcb_ldt_sel == pmap->pm_ldt_sel);

	if (pmap == oldpmap) {
		if (!pmap_reactivate(pmap)) {

			/*
			 * pmap has been changed during deactivated.
			 * our tlb may be stale.
			 */

			tlbflush();
		}

		ci->ci_want_pmapload = 0;
		crit_exit();
		return;
	}

	/*
	 * grab a reference to the new pmap.  this may block.  if state
	 * has changed out from underneath us, drop the reference and
	 * retry.
	 */

	KERNEL_LOCK(1, NULL);
	ncsw = l->l_ncsw;
	pmap_reference(pmap);
	if (l->l_ncsw != ncsw) {
		pmap_destroy(pmap);
		KERNEL_UNLOCK_ONE(NULL);
		goto retry;
	}

	/*
	 * actually switch pmap.
	 */

	x86_atomic_clearbits_l(&oldpmap->pm_cpus, cpumask);
	x86_atomic_clearbits_l(&oldpmap->pm_kernel_cpus, cpumask);

	KASSERT(oldpmap->pm_pdirpa == rcr3());
	KASSERT((pmap->pm_cpus & cpumask) == 0);
	KASSERT((pmap->pm_kernel_cpus & cpumask) == 0);

	/*
	 * mark the pmap in use by this processor.  again we must
	 * synchronize with TLB shootdown interrupts, so set the
	 * state VALID first, then register us for shootdown events
	 * on this pmap.
	 */

	ci->ci_tlbstate = TLBSTATE_VALID;
	x86_atomic_setbits_l(&pmap->pm_cpus, cpumask);
	x86_atomic_setbits_l(&pmap->pm_kernel_cpus, cpumask);
	ci->ci_pmap = pmap;

	/*
	 * update tss.  now that we have registered for invalidations
	 * from other CPUs, we're good to load the page tables.
	 */

	lldt(pcb->pcb_ldt_sel);
	pcb->pcb_cr3 = pmap->pm_pdirpa;
	lcr3(pcb->pcb_cr3);

	ci->ci_want_pmapload = 0;

	/*
	 * we're now running with the new pmap.  drop the reference
	 * to the old pmap.  if we block, we need to go around again.
	 */

	pmap_destroy(oldpmap);
	KERNEL_UNLOCK_ONE(NULL);
	if (l->l_ncsw != ncsw) {
		goto retry;
	}

	crit_exit();
}

/*
 * pmap_deactivate: deactivate a process' pmap
 *
 * => must be called with kernel preemption disabled (high SPL is enough)
 */

void
pmap_deactivate(struct lwp *l)
{
	struct pmap *pmap;
	struct cpu_info *ci;

	if (l != curlwp) {
		return;
	}

	/*
	 * wait for pending TLB shootdowns to complete.  necessary
	 * because TLB shootdown state is per-CPU, and the LWP may
	 * be coming off the CPU before it has a chance to call
	 * pmap_update().
	 */
	pmap_tlb_shootwait();

	ci = curcpu();

	if (ci->ci_want_pmapload) {
		KASSERT(vm_map_pmap(&l->l_proc->p_vmspace->vm_map)
		    != pmap_kernel());
		KASSERT(vm_map_pmap(&l->l_proc->p_vmspace->vm_map)
		    != ci->ci_pmap || ci->ci_tlbstate != TLBSTATE_VALID);

		/*
		 * userspace has not been touched.
		 * nothing to do here.
		 */

		ci->ci_want_pmapload = 0;
		return;
	}

	pmap = vm_map_pmap(&l->l_proc->p_vmspace->vm_map);

	if (pmap == pmap_kernel()) {
		return;
	}

	KASSERT(pmap->pm_pdirpa == rcr3());
	KASSERT(ci->ci_pmap == pmap);

	/*
	 * we aren't interested in TLB invalidations for this pmap,
	 * at least for the time being.
	 */

	KASSERT(ci->ci_tlbstate == TLBSTATE_VALID);
	ci->ci_tlbstate = TLBSTATE_LAZY;
}

/*
 * end of lifecycle functions
 */

/*
 * some misc. functions
 */

static int
pmap_pdes_invalid(vaddr_t va, pd_entry_t **pdes, pd_entry_t *lastpde)
{
	int i;
	unsigned long index;
	pd_entry_t pde;

	for (i = PTP_LEVELS; i > 1; i--) {
		index = pl_i(va, i);
		pde = pdes[i - 2][index];
		if ((pde & PG_V) == 0)
			return i;
	}
	if (lastpde != NULL)
		*lastpde = pde;
	return 0;
}

/*
 * pmap_extract: extract a PA for the given VA
 */

bool
pmap_extract(struct pmap *pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *ptes, pte;
	pd_entry_t pde, **pdes;
	struct pmap *pmap2;

	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);
	if (!pmap_pdes_valid(va, pdes, &pde)) {
		pmap_unmap_ptes(pmap, pmap2);
		return false;
	}
	pte = ptes[pl1_i(va)];
	pmap_unmap_ptes(pmap, pmap2);

	if (pde & PG_PS) {
		if (pap != NULL)
			*pap = (pde & PG_LGFRAME) | (va & (NBPD_L2 - 1));
		return (true);
	}

	if (__predict_true((pte & PG_V) != 0)) {
		if (pap != NULL)
			*pap = (pte & PG_FRAME) | (va & (NBPD_L1 - 1));
		return (true);
	}

	return false;
}


/*
 * vtophys: virtual address to physical address.  For use by
 * machine-dependent code only.
 */

paddr_t
vtophys(vaddr_t va)
{
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == true)
		return (pa);
	return (0);
}


/*
 * pmap_virtual_space: used during bootup [pmap_steal_memory] to
 *	determine the bounds of the kernel virtual addess space.
 */

void
pmap_virtual_space(vaddr_t *startp, vaddr_t *endp)
{
	*startp = virtual_avail;
	*endp = virtual_end;
}

/*
 * pmap_map: map a range of PAs into kvm.
 *
 * => used during crash dump
 * => XXX: pmap_map() should be phased out?
 */

vaddr_t
pmap_map(vaddr_t va, paddr_t spa, paddr_t epa, vm_prot_t prot)
{
	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, 0);
		va += PAGE_SIZE;
		spa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
	return va;
}

/*
 * pmap_zero_page: zero a page
 */

void
pmap_zero_page(paddr_t pa)
{
#ifdef MULTIPROCESSOR
	int id = cpu_number();
#endif
	pt_entry_t *zpte = PTESLEW(zero_pte, id);
	void *zerova = VASLEW(zerop, id);

#ifdef DIAGNOSTIC
	if (*zpte)
		panic("pmap_zero_page: lock botch");
#endif

	*zpte = (pa & PG_FRAME) | PG_V | PG_RW | PG_M | PG_U; /* map in */
	pmap_update_pg((vaddr_t)zerova);		/* flush TLB */

	if (cpu_feature & CPUID_SSE2)
		sse2_zero_page(zerova);
	else
		memset(zerova, 0, PAGE_SIZE);

#ifdef DIAGNOSTIC
	*zpte = 0;					/* zap! */
#endif
}

/*
 * pmap_pagezeroidle: the same, for the idle loop page zero'er.
 * Returns true if the page was zero'd, false if we aborted for
 * some reason.
 */

bool
pmap_pageidlezero(paddr_t pa)
{

	pmap_zero_page(pa);
	return true;
}

/*
 * pmap_copy_page: copy a page
 */

void
pmap_copy_page(paddr_t srcpa, paddr_t dstpa)
{
#ifdef MULTIPROCESSOR
	int id = cpu_number();
#endif
	pt_entry_t *spte = PTESLEW(csrc_pte,id);
	pt_entry_t *dpte = PTESLEW(cdst_pte,id);
	void *csrcva = VASLEW(csrcp, id);
	void *cdstva = VASLEW(cdstp, id);

#ifdef DIAGNOSTIC
	if (*spte || *dpte)
		panic("pmap_copy_page: lock botch");
#endif

	*spte = (srcpa & PG_FRAME) | PG_V | PG_RW | PG_U;
	*dpte = (dstpa & PG_FRAME) | PG_V | PG_RW | PG_M | PG_U;
	pmap_update_2pg((vaddr_t)csrcva, (vaddr_t)cdstva);
	if (cpu_feature & CPUID_SSE2)
		sse2_copy_page(csrcva, cdstva);
	else
		memcpy(cdstva, csrcva, PAGE_SIZE);
#ifdef DIAGNOSTIC
	*spte = *dpte = 0;			/* zap! */
#endif
}

/*
 * p m a p   r e m o v e   f u n c t i o n s
 *
 * functions that remove mappings
 */

/*
 * pmap_remove_ptes: remove PTEs from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 * => must be called with kernel preemption disabled
 * => returns composite pte if at least one page should be shot down
 */

static pt_entry_t
pmap_remove_ptes(struct pmap *pmap, struct vm_page *ptp, vaddr_t ptpva,
		 vaddr_t startva, vaddr_t endva, int flags,
		 struct pv_entry **pv_tofree)
{
	struct pv_entry *pve;
	pt_entry_t *pte = (pt_entry_t *) ptpva;
	pt_entry_t opte, xpte = 0;

	/*
	 * note that ptpva points to the PTE that maps startva.   this may
	 * or may not be the first PTE in the PTP.
	 *
	 * we loop through the PTP while there are still PTEs to look at
	 * and the wire_count is greater than 1 (because we use the wire_count
	 * to keep track of the number of real PTEs in the PTP).
	 */

	for (/*null*/; startva < endva && (ptp == NULL || ptp->wire_count > 1)
			     ; pte++, startva += PAGE_SIZE) {
		struct vm_page *pg;
		struct vm_page_md *mdpg;

		if (!pmap_valid_entry(*pte))
			continue;			/* VA not mapped */
		if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
			continue;
		}

		/* atomically save the old PTE and zap! it */
		opte = pmap_pte_set(pte, 0);
		pmap_exec_account(pmap, startva, opte, 0);
		KASSERT(pmap_valid_entry(opte));

		if (opte & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;
		xpte |= opte;

		if (ptp) {
			ptp->wire_count--;		/* dropping a PTE */
			/* Make sure that the PDE is flushed */
			if (ptp->wire_count <= 1)
				xpte |= PG_U;
		}

		/*
		 * if we are not on a pv_head list we are done.
		 */

		if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
			if (PHYS_TO_VM_PAGE(opte & PG_FRAME) != NULL)
				panic("pmap_remove_ptes: managed page without "
				      "PG_PVLIST for 0x%lx", startva);
#endif
			continue;
		}

		pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);
#ifdef DIAGNOSTIC
		if (pg == NULL)
			panic("pmap_remove_ptes: unmanaged page marked "
			      "PG_PVLIST, va = 0x%lx, pa = 0x%lx",
			      startva, (u_long)(opte & PG_FRAME));
#endif
		mdpg = &pg->mdpage;

		/* sync R/M bits */
		mutex_spin_enter(&mdpg->mp_pvhead.pvh_lock);
		mdpg->mp_attrs |= (opte & (PG_U|PG_M));
		pve = pmap_remove_pv(&mdpg->mp_pvhead, pmap, startva);
		mutex_spin_exit(&mdpg->mp_pvhead.pvh_lock);

		if (pve) {
			SPLAY_RIGHT(pve, pv_node) = *pv_tofree;
			*pv_tofree = pve;
		}

		/* end of "for" loop: time for next pte */
	}

	return xpte;
}


/*
 * pmap_remove_pte: remove a single PTE from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 * => returns true if we removed a mapping
 * => must be called with kernel preemption disabled
 */

static bool
pmap_remove_pte(struct pmap *pmap, struct vm_page *ptp, pt_entry_t *pte,
		vaddr_t va, int flags, struct pv_entry **pv_tofree)
{
	pt_entry_t opte;
	struct pv_entry *pve;
	struct vm_page *pg;
	struct vm_page_md *mdpg;

	if (!pmap_valid_entry(*pte))
		return(false);		/* VA not mapped */
	if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
		return(false);
	}

	/* atomically save the old PTE and zap! it */
	opte = pmap_pte_set(pte, 0);
	pmap_exec_account(pmap, va, opte, 0);
	KASSERT(pmap_valid_entry(opte));

	if (opte & PG_W)
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	if (opte & PG_U)
		pmap_tlb_shootdown(pmap, va, 0, opte);

	if (ptp) {
		ptp->wire_count--;		/* dropping a PTE */
		/* Make sure that the PDE is flushed */
		if ((ptp->wire_count <= 1) && !(opte & PG_U))
			pmap_tlb_shootdown(pmap, va, 0, opte);
	}

	/*
	 * if we are not on a pv_head list we are done.
	 */

	if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
		if (PHYS_TO_VM_PAGE(opte & PG_FRAME) != NULL)
			panic("pmap_remove_pte: managed page without "
			      "PG_PVLIST for 0x%lx", va);
#endif
		return(true);
	}

	pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);
#ifdef DIAGNOSTIC
	if (pg == NULL)
		panic("pmap_remove_pte: unmanaged page marked "
		    "PG_PVLIST, va = 0x%lx, pa = 0x%lx", va,
		    (u_long)(opte & PG_FRAME));
#endif
	mdpg = &pg->mdpage;

	/* sync R/M bits */
	mutex_spin_enter(&mdpg->mp_pvhead.pvh_lock);
	mdpg->mp_attrs |= (opte & (PG_U|PG_M));
	pve = pmap_remove_pv(&mdpg->mp_pvhead, pmap, va);
	mutex_spin_exit(&mdpg->mp_pvhead.pvh_lock);

	if (pve) { 
		SPLAY_RIGHT(pve, pv_node) = *pv_tofree;
		*pv_tofree = pve;
	}

	return(true);
}

/*
 * pmap_remove: top level mapping removal function
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva)
{
	pmap_do_remove(pmap, sva, eva, PMAP_REMOVE_ALL);
}

/*
 * pmap_do_remove: mapping removal guts
 *
 * => caller should not be holding any pmap locks
 */

static void
pmap_do_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva, int flags)
{
	pt_entry_t *ptes, xpte = 0;
	pd_entry_t **pdes, pde;
	struct pv_entry *pv_tofree = NULL;
	bool result;
	paddr_t ptppa;
	vaddr_t blkendva, va = sva;
	struct vm_page *ptp, *empty_ptps = NULL;
	struct pmap *pmap2;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	rw_enter(&pmap_main_lock, RW_READER);

	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);	/* locks pmap */

	/*
	 * removing one page?  take shortcut function.
	 */

	if (va + PAGE_SIZE == eva) {
		if (pmap_pdes_valid(va, pdes, &pde)) {

			/* PA of the PTP */
			ptppa = pde & PG_FRAME;

			/* get PTP if non-kernel mapping */
			if (pmap == pmap_kernel()) {
				/* we never free kernel PTPs */
				ptp = NULL;
			} else {
				ptp = pmap_find_ptp(pmap, va, ptppa, 1);
#ifdef DIAGNOSTIC
				if (ptp == NULL)
					panic("pmap_remove: unmanaged "
					      "PTP detected");
#endif
			}

			/* do it! */
			result = pmap_remove_pte(pmap, ptp,
			    &ptes[pl1_i(va)], va, flags, &pv_tofree);

			/*
			 * if mapping removed and the PTP is no longer
			 * being used, free it!
			 */

			if (result && ptp && ptp->wire_count <= 1)
				pmap_free_ptp(pmap, ptp, va, ptes, pdes,
				    &empty_ptps);
		}
	} else for (/* null */ ; va < eva ; va = blkendva) {
		int lvl;

		/* determine range of block */
		blkendva = x86_round_pdr(va+1);
		if (blkendva > eva)
			blkendva = eva;

		/*
		 * XXXCDC: our PTE mappings should never be removed
		 * with pmap_remove!  if we allow this (and why would
		 * we?) then we end up freeing the pmap's page
		 * directory page (PDP) before we are finished using
		 * it when we hit in in the recursive mapping.  this
		 * is BAD.
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		if (pl_i(va, PTP_LEVELS) == PDIR_SLOT_PTE)
			/* XXXCDC: ugly hack to avoid freeing PDP here */
			continue;

		lvl = pmap_pdes_invalid(va, pdes, &pde);
		if (lvl != 0) {
			/*
			 * skip a range corresponding to an invalid pde.
			 */
			blkendva = (va & ptp_masks[lvl - 1]) + nbpd[lvl - 1]; 
 			continue;
		}

		/* PA of the PTP */
		ptppa = pde & PG_FRAME;

		/* get PTP if non-kernel mapping */
		if (pmap == pmap_kernel()) {
			/* we never free kernel PTPs */
			ptp = NULL;
		} else {
			ptp = pmap_find_ptp(pmap, va, ptppa, 1);
#ifdef DIAGNOSTIC
			if (ptp == NULL)
				panic("pmap_remove: unmanaged PTP "
				      "detected");
#endif
		}
		xpte |= pmap_remove_ptes(pmap, ptp,
		    (vaddr_t)&ptes[pl1_i(va)], va, blkendva,
		    flags, &pv_tofree);

		/* if PTP is no longer being used, free it! */
		if (ptp && ptp->wire_count <= 1) {
			pmap_free_ptp(pmap, ptp, va, ptes, pdes, &empty_ptps);
		}
		if ((xpte & PG_U) != 0)
			pmap_tlb_shootdown(pmap, sva, eva, xpte);
	}
	pmap_tlb_shootwait();
	pmap_unmap_ptes(pmap, pmap2);		/* unlock pmap */
	rw_exit(&pmap_main_lock);

	/* Now we can free unused PVs and ptps */
	if (pv_tofree)
		pmap_free_pvs(pmap, pv_tofree);
	for (ptp = empty_ptps; ptp != NULL; ptp = empty_ptps) {
		empty_ptps = ptp->mdpage.mp_link;
		uvm_pagefree(ptp);
	}
}

/*
 * pmap_page_remove: remove a managed vm_page from all pmaps that map it
 *
 * => we set pv_head => pmap locking
 * => R/M bits are sync'd back to attrs
 */

void
pmap_page_remove(struct vm_page *pg)
{
	struct pv_head *pvh;
	struct pv_entry *pve, *npve, *killlist = NULL;
	pt_entry_t *ptes, opte;
	pd_entry_t **pdes;
#ifdef DIAGNOSTIC
	pd_entry_t pde;
#endif
	struct vm_page *empty_ptps = NULL;
	struct vm_page *ptp;
	struct pmap *pmap2;

#ifdef DIAGNOSTIC
	int bank, off;

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1)
		panic("pmap_page_remove: unmanaged page?");
#endif

	pvh = &pg->mdpage.mp_pvhead;
	if (SPLAY_ROOT(&pvh->pvh_root) == NULL) {
		return;
	}

	/* set pv_head => pmap locking */
	rw_enter(&pmap_main_lock, RW_WRITER);

	for (pve = SPLAY_MIN(pvtree, &pvh->pvh_root); pve != NULL; pve = npve) {
		npve = SPLAY_NEXT(pvtree, &pvh->pvh_root, pve);

		/* locks pmap */
		pmap_map_ptes(pve->pv_pmap, &pmap2, &ptes, &pdes);

#ifdef DIAGNOSTIC
		if (pve->pv_ptp && pmap_pdes_valid(pve->pv_va, pdes, &pde) &&
		   (pde & PG_FRAME) != VM_PAGE_TO_PHYS(pve->pv_ptp)) {
			printf("pmap_page_remove: pg=%p: va=%lx, pv_ptp=%p\n",
			       pg, pve->pv_va, pve->pv_ptp);
			printf("pmap_page_remove: PTP's phys addr: "
			       "actual=%lx, recorded=%lx\n",
			       (unsigned long)(pde & PG_FRAME),
			       (unsigned long)VM_PAGE_TO_PHYS(pve->pv_ptp));
			panic("pmap_page_remove: mapped managed page has "
			      "invalid pv_ptp field");
		}
#endif

		/* atomically save the old PTE and zap! it */
		opte = pmap_pte_set(&ptes[pl1_i(pve->pv_va)], 0);
		KASSERT(pmap_valid_entry(opte));
		KDASSERT((opte & PG_FRAME) == VM_PAGE_TO_PHYS(pg));

		if (opte & PG_W)
			pve->pv_pmap->pm_stats.wired_count--;
		pve->pv_pmap->pm_stats.resident_count--;

		/* Shootdown only if referenced */
		if (opte & PG_U)
			pmap_tlb_shootdown(pve->pv_pmap, pve->pv_va, 0, opte);

		/* sync R/M bits */
		pg->mdpage.mp_attrs |= (opte & (PG_U|PG_M));

		/* update the PTP reference count.  free if last reference. */
		if (pve->pv_ptp) {
			pve->pv_ptp->wire_count--;
			if (pve->pv_ptp->wire_count <= 1) {
				pmap_free_ptp(pve->pv_pmap, pve->pv_ptp,
				    pve->pv_va, ptes, pdes, &empty_ptps);
			}
		}
		
		pmap_unmap_ptes(pve->pv_pmap, pmap2);	/* unlocks pmap */
		SPLAY_REMOVE(pvtree, &pvh->pvh_root, pve); /* remove it */
		SPLAY_RIGHT(pve, pv_node) = killlist;	/* mark it for death */
		killlist = pve;
	}
	rw_exit(&pmap_main_lock);

	crit_enter();
	pmap_tlb_shootwait();
	crit_exit();

	/* Now we can free unused pvs and ptps. */
	pmap_free_pvs(NULL, killlist);
	for (ptp = empty_ptps; ptp != NULL; ptp = empty_ptps) {
		empty_ptps = ptp->mdpage.mp_link;
		uvm_pagefree(ptp);
	}
}

/*
 * p m a p   a t t r i b u t e  f u n c t i o n s
 * functions that test/change managed page's attributes
 * since a page can be mapped multiple times we must check each PTE that
 * maps it by going down the pv lists.
 */

/*
 * pmap_test_attrs: test a page's attributes
 *
 * => we set pv_head => pmap locking
 */

bool
pmap_test_attrs(struct vm_page *pg, unsigned testbits)
{
	struct vm_page_md *mdpg;
	int *myattrs;
	struct pv_head *pvh;
	struct pv_entry *pve;
	struct pmap *pmap2;
	pt_entry_t pte;

#if DIAGNOSTIC
	int bank, off;

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1)
		panic("pmap_test_attrs: unmanaged page?");
#endif
	mdpg = &pg->mdpage;

	/*
	 * before locking: see if attributes are already set and if so,
	 * return!
	 */

	myattrs = &mdpg->mp_attrs;
	if (*myattrs & testbits)
		return(true);

	/* test to see if there is a list before bothering to lock */
	pvh = &mdpg->mp_pvhead;
	if (SPLAY_ROOT(&pvh->pvh_root) == NULL) {
		return(false);
	}

	/* nope, gonna have to do it the hard way */
	rw_enter(&pmap_main_lock, RW_WRITER);

	for (pve = SPLAY_MIN(pvtree, &pvh->pvh_root);
	     pve != NULL && (*myattrs & testbits) == 0;
	     pve = SPLAY_NEXT(pvtree, &pvh->pvh_root, pve)) {
		pt_entry_t *ptes;
		pd_entry_t **pdes;

		pmap_map_ptes(pve->pv_pmap, &pmap2, &ptes, &pdes);
		pte = ptes[pl1_i(pve->pv_va)];
		pmap_unmap_ptes(pve->pv_pmap, pmap2);
		*myattrs |= pte;
	}

	/*
	 * note that we will exit the for loop with a non-null pve if
	 * we have found the bits we are testing for.
	 */

	rw_exit(&pmap_main_lock);
	return((*myattrs & testbits) != 0);
}

/*
 * pmap_clear_attrs: clear the specified attribute for a page.
 *
 * => we set pv_head => pmap locking
 * => we return true if we cleared one of the bits we were asked to
 */

bool
pmap_clear_attrs(struct vm_page *pg, unsigned clearbits)
{
	struct vm_page_md *mdpg;
	uint32_t result;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, opte;
	int *myattrs;
	struct pmap *pmap2;

#ifdef DIAGNOSTIC
	int bank, off;

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1)
		panic("pmap_change_attrs: unmanaged page?");
#endif
	mdpg = &pg->mdpage;

	rw_enter(&pmap_main_lock, RW_WRITER);
	pvh = &mdpg->mp_pvhead;

	myattrs = &mdpg->mp_attrs;
	result = *myattrs & clearbits;
	*myattrs &= ~clearbits;

	SPLAY_FOREACH(pve, pvtree, &pvh->pvh_root) {
		pt_entry_t *ptep;
		pd_entry_t **pdes;

		/* locks pmap */
		pmap_map_ptes(pve->pv_pmap, &pmap2, &ptes, &pdes);
#ifdef DIAGNOSTIC
		if (!pmap_pdes_valid(pve->pv_va, pdes, NULL))
			panic("pmap_change_attrs: mapping without PTP "
			      "detected");
#endif
		ptep = &ptes[pl1_i(pve->pv_va)];
		opte = *ptep;
		KASSERT(pmap_valid_entry(opte));
		KDASSERT((opte & PG_FRAME) == VM_PAGE_TO_PHYS(pg));
		if (opte & clearbits) {
			/* We need to do something */
			if (clearbits == PG_RW) {
				result |= PG_RW;

				/*
				 * On write protect we might not need to flush 
				 * the TLB
				 */

				/* First zap the RW bit! */
				pmap_pte_clearbits(ptep, PG_RW); 
				opte = *ptep;

				/*
				 * Then test if it is not cached as RW the TLB
				 */
				if (!(opte & PG_M))
					goto no_tlb_shootdown;
			}

			/*
			 * Since we need a shootdown we might as well
			 * always clear PG_U AND PG_M.
			 */

			/* zap! */
			opte = pmap_pte_set(ptep, (opte & ~(PG_U | PG_M)));

			result |= (opte & clearbits);
			*myattrs |= (opte & ~(clearbits));

			pmap_tlb_shootdown(pve->pv_pmap, pve->pv_va, 0, opte);
		}
no_tlb_shootdown:
		pmap_unmap_ptes(pve->pv_pmap, pmap2);	/* unlocks pmap */
	}

	rw_exit(&pmap_main_lock);

	crit_enter();
	pmap_tlb_shootwait();
	crit_exit();

	return(result != 0);
}


/*
 * p m a p   p r o t e c t i o n   f u n c t i o n s
 */

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_protect: set the protection in of the pages in a pmap
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_write_protect: write-protect pages in a pmap
 */

void
pmap_write_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	pt_entry_t *ptes, *epte, xpte;
	volatile pt_entry_t *spte;
	pd_entry_t **pdes;
	vaddr_t blockend, va, tva;
	pt_entry_t opte;
	struct pmap *pmap2;

	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);	/* locks pmap */

	/* should be ok, but just in case ... */
	sva &= PG_FRAME;
	eva &= PG_FRAME;
	xpte = 0;

	for (va = sva ; va < eva ; va = blockend) {

		blockend = (va & L2_FRAME) + NBPD_L2;
		if (blockend > eva)
			blockend = eva;

		/*
		 * XXXCDC: our PTE mappings should never be write-protected!
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		/* XXXCDC: ugly hack to avoid freeing PDP here */
		if (pl_i(va, PTP_LEVELS) == PDIR_SLOT_PTE)
			continue;

		/* empty block? */
		if (!pmap_pdes_valid(va, pdes, NULL))
			continue;

#ifdef DIAGNOSTIC
		if (va >= VM_MAXUSER_ADDRESS &&
		    va < VM_MAX_ADDRESS)
			panic("pmap_write_protect: PTE space");
#endif

		spte = &ptes[pl1_i(va)];
		epte = &ptes[pl1_i(blockend)];

		for (/*null */; spte < epte ; spte++) {
			opte = *spte;
			xpte |= opte;
			if ((opte & (PG_RW|PG_V)) == (PG_RW|PG_V)) {
				pmap_pte_clearbits(spte, PG_RW); /* zap! */
				if (*spte & PG_M) {
					tva = x86_ptob(spte - ptes);
					pmap_tlb_shootdown(pmap, tva, 0, opte);
				}
			}
		}
	}

	/*
	 * if we kept a removal record and removed some pages update the TLB
	 */
	pmap_tlb_shootdown(pmap, sva, eva, xpte);
	pmap_tlb_shootwait();
	pmap_unmap_ptes(pmap, pmap2);	/* unlocks pmap */
}

/*
 * end of protection functions
 */

/*
 * pmap_unwire: clear the wired bit in the PTE
 *
 * => mapping should already be in map
 */

void
pmap_unwire(struct pmap *pmap, vaddr_t va)
{
	pt_entry_t *ptes;
	pd_entry_t **pdes;
	struct pmap *pmap2;

	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);	/* locks pmap */

	if (pmap_pdes_valid(va, pdes, NULL)) {

#ifdef DIAGNOSTIC
		if (!pmap_valid_entry(ptes[pl1_i(va)]))
			panic("pmap_unwire: invalid (unmapped) va 0x%lx", va);
#endif
		if ((ptes[pl1_i(va)] & PG_W) != 0) {
			pmap_pte_clearbits(&ptes[pl1_i(va)], PG_W);
			pmap->pm_stats.wired_count--;
		}
#ifdef DIAGNOSTIC
		else {
			printf("pmap_unwire: wiring for pmap %p va 0x%lx "
			       "didn't change!\n", pmap, va);
		}
#endif
		pmap_unmap_ptes(pmap, pmap2);		/* unlocks map */
	}
#ifdef DIAGNOSTIC
	else {
		panic("pmap_unwire: invalid PDE");
	}
#endif
}

/*
 * pmap_collect: free resources held by a pmap
 *
 * => optional function.
 * => called when a process is swapped out to free memory.
 */

void
pmap_collect(struct pmap *pmap)
{
	/*
	 * free all of the pt pages by removing the physical mappings
	 * for its entire address space.
	 */

	pmap_do_remove(pmap, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS,
	    PMAP_REMOVE_SKIPWIRED);
}

/*
 * pmap_copy: copy mappings from one pmap to another
 *
 * => optional function
 * void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
 */

/*
 * defined as macro in pmap.h
 */

/*
 * pmap_enter: enter a mapping into a pmap
 *
 * => must be done "now" ... no lazy-evaluation
 * => we set pmap => pv_head locking
 */

int
pmap_enter(struct pmap *pmap, vaddr_t va, paddr_t pa, vm_prot_t prot,
	   int flags)
{
	pt_entry_t *ptes, opte, npte;
	pt_entry_t *ptep;
	pd_entry_t **pdes;
	struct vm_page *ptp, *pg;
	struct vm_page_md *mdpg;
	struct pv_head *old_pvh, *new_pvh;
	struct pv_entry *pve = NULL, *freepve, *freepve2 = NULL;
	int error;
	bool wired = (flags & PMAP_WIRED) != 0;
	struct pmap *pmap2;

	KASSERT(pmap_initialized);

#ifdef DIAGNOSTIC
	/* sanity check: totally out of range? */
	if (va >= VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: too big");

	if (va == (vaddr_t) PDP_BASE || va == (vaddr_t) APDP_BASE)
		panic("pmap_enter: trying to map over PDP/APDP!");

	/* sanity check: kernel PTPs should already have been pre-allocated */
	if (va >= VM_MIN_KERNEL_ADDRESS &&
	    !pmap_valid_entry(pmap->pm_pdir[pl_i(va, PTP_LEVELS)]))
		panic("pmap_enter: missing kernel PTP for va %lx!", va);
#endif

	npte = pa | protection_codes[prot] | PG_V;
	if (wired)
	        npte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		npte |= PG_u;
	else if (va < VM_MAX_ADDRESS)
		npte |= (PG_u | PG_RW);	/* XXXCDC: no longer needed? */
	if (pmap == pmap_kernel())
		npte |= pmap_pg_g;
	if (flags & VM_PROT_ALL) {
		npte |= PG_U;
		if (flags & VM_PROT_WRITE)
			npte |= PG_M;
	}

	/* get a pve. */
	freepve = pmap_alloc_pv(pmap, ALLOCPV_NEED);

	/* get lock */
	rw_enter(&pmap_main_lock, RW_READER);

	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);	/* locks pmap */
	if (pmap == pmap_kernel()) {
		ptp = NULL;
	} else {
		ptp = pmap_get_ptp(pmap, va, pdes);
		if (ptp == NULL) {
			if (flags & PMAP_CANFAIL) {
				error = ENOMEM;
				goto out;
			}
			panic("pmap_enter: get ptp failed");
		}
	}

	/*
	 * Get first view on old PTE 
	 * on SMP the PTE might gain PG_U and PG_M flags
	 * before we zap it later
	 */
	ptep = &ptes[pl1_i(va)];
	opte = *ptep;		/* old PTE */

	/*
	 * is there currently a valid mapping at our VA and does it
	 * map to the same PA as the one we want to map ?
	 */

	if (pmap_valid_entry(opte) && ((opte & PG_FRAME) == pa)) {

		/*
		 * first, calculate pm_stats updates.  resident count will not
		 * change since we are replacing/changing a valid mapping.
		 * wired count might change...
		 */
		pmap->pm_stats.wired_count +=
		    ((npte & PG_W) ? 1 : 0 - (opte & PG_W) ? 1 : 0);

		npte |= (opte & PG_PVLIST);

		/* zap! */
		opte = pmap_pte_set(ptep, npte);

		/*
		 * if this is on the PVLIST, sync R/M bit
		 */
		if (opte & PG_PVLIST) {
			pg = PHYS_TO_VM_PAGE(pa);
#ifdef DIAGNOSTIC
			if (pg == NULL)
				panic("pmap_enter: same pa PG_PVLIST "
				      "mapping with unmanaged page "
				      "pa = 0x%lx (0x%lx)", pa,
				      atop(pa));
#endif
			mdpg = &pg->mdpage;
			old_pvh = &mdpg->mp_pvhead;
			mutex_spin_enter(&old_pvh->pvh_lock);
			mdpg->mp_attrs |= opte;
			mutex_spin_exit(&old_pvh->pvh_lock);
		}
		goto shootdown_now;
	}

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL) {
		/* This is a managed page */
		npte |= PG_PVLIST;
		mdpg = &pg->mdpage;
		new_pvh = &mdpg->mp_pvhead;
		if ((opte & (PG_PVLIST | PG_V)) != (PG_PVLIST | PG_V)) {
			/* We can not steal a pve - allocate one */
			pve = freepve;
			freepve = NULL;
			if (pve == NULL) {
				if (!(flags & PMAP_CANFAIL))
					panic("pmap_enter: "
					    "no pv entries available");
				error = ENOMEM;
				goto out;
			}
  		}
	} else {
		new_pvh = NULL;
	}

	/*
	 * is there currently a valid mapping at our VA?
	 */

	if (pmap_valid_entry(opte)) {

		/*
		 * changing PAs: we must remove the old one first
		 */

		/*
		 * first, calculate pm_stats updates.  resident count will not
		 * change since we are replacing/changing a valid mapping.
		 * wired count might change...
		 */
		pmap->pm_stats.wired_count +=
		    ((npte & PG_W) ? 1 : 0 - (opte & PG_W) ? 1 : 0);

		if (opte & PG_PVLIST) {
			pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);
#ifdef DIAGNOSTIC
			if (pg == NULL)
				panic("pmap_enter: PG_PVLIST mapping with "
				      "unmanaged page "
				      "pa = 0x%lx (0x%lx)", pa, atop(pa));
#endif
			mdpg = &pg->mdpage;
			old_pvh = &mdpg->mp_pvhead;

			/* new_pvh is NULL if page will not be managed */
			pmap_lock_pvhs(old_pvh, new_pvh);

			/* zap! */
			opte = pmap_pte_set(ptep, npte);

			pve = pmap_remove_pv(old_pvh, pmap, va);
			KASSERT(pve != 0);
			mdpg->mp_attrs |= opte;

			if (new_pvh != NULL) {
				pmap_enter_pv(new_pvh, pve, pmap, va, ptp);
				mutex_spin_exit(&new_pvh->pvh_lock);
			}
			mutex_spin_exit(&old_pvh->pvh_lock);
			if (new_pvh == NULL)
				freepve2 = pve;
			goto shootdown_test;
		}
	} else {	/* opte not valid */
		pmap->pm_stats.resident_count++;
		if (wired) 
			pmap->pm_stats.wired_count++;
		if (ptp)
			ptp->wire_count++;
	}

	if (new_pvh) {
		mutex_spin_enter(&new_pvh->pvh_lock);
		pmap_enter_pv(new_pvh, pve, pmap, va, ptp);
		mutex_spin_exit(&new_pvh->pvh_lock);
	}

	opte = pmap_pte_set(ptep, npte);   /* zap! */

shootdown_test:
	/* Update page attributes if needed */
	if ((opte & (PG_V | PG_U)) == (PG_V | PG_U)) {
shootdown_now:
		pmap_tlb_shootdown(pmap, va, 0, opte);
		pmap_tlb_shootwait();
	}

	error = 0;

out:
	pmap_unmap_ptes(pmap, pmap2);
	rw_exit(&pmap_main_lock);

	if (freepve != NULL) {
		/* put back the pv, we don't need it. */
		pmap_free_pv(pmap, freepve);
	}
	if (freepve2 != NULL)
		pmap_free_pv(pmap, freepve2);

	return error;
}

static bool
pmap_get_physpage(vaddr_t va, int level, paddr_t *paddrp)
{
	struct vm_page *ptp;
	struct pmap *kpm = pmap_kernel();

	if (uvm.page_init_done == false) {
		/*
		 * we're growing the kernel pmap early (from
		 * uvm_pageboot_alloc()).  this case must be
		 * handled a little differently.
		 */

		if (uvm_page_physget(paddrp) == false)
			panic("pmap_get_physpage: out of memory");
		*early_zero_pte = (*paddrp & PG_FRAME) | PG_V | PG_RW;
		pmap_update_pg((vaddr_t)early_zerop);
		memset(early_zerop, 0, PAGE_SIZE);
#if defined(DIAGNOSTIC)
		*early_zero_pte = 0;
#endif /* defined(DIAGNOSTIC) */
	} else {
		/* XXX */
		if (level != 1)
			mutex_enter(&kpm->pm_obj[level - 1].vmobjlock);
		ptp = uvm_pagealloc(&kpm->pm_obj[level - 1],
				    ptp_va2o(va, level), NULL,
				    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
		if (level != 1)
			mutex_exit(&kpm->pm_obj[level - 1].vmobjlock);
		if (ptp == NULL)
			panic("pmap_get_physpage: out of memory");
		ptp->flags &= ~PG_BUSY;
		ptp->wire_count = 1;
		*paddrp = VM_PAGE_TO_PHYS(ptp);
	}
	kpm->pm_stats.resident_count++;
	return true;
}

#define	pl_i_roundup(va, lvl)	pl_i((va)+ ~ptp_masks[(lvl)-1], (lvl))
/*
 * Allocate the amount of specified ptps for a ptp level, and populate
 * all levels below accordingly, mapping virtual addresses starting at
 * kva.
 *
 * Used by pmap_growkernel.
 */
static void
pmap_alloc_level(pd_entry_t **pdes, vaddr_t kva, int lvl, long *needed_ptps)
{
	unsigned long i;
	vaddr_t va;
	paddr_t pa;
	unsigned long index, endindex;
	int level;
	pd_entry_t *pdep;

	for (level = lvl; level > 1; level--) {
		if (level == PTP_LEVELS)
			pdep = pmap_kernel()->pm_pdir;
		else
			pdep = pdes[level - 2];
		va = kva;
		index = pl_i_roundup(kva, level);
		endindex = index + needed_ptps[level - 1] - 1;

		for (i = index; i <= endindex; i++) {
			KASSERT(!pmap_valid_entry(pdep[i]));
			pmap_get_physpage(va, level - 1, &pa);
			pdep[i] = pa | PG_RW | PG_V;
			KASSERT(level != PTP_LEVELS || nkptp[level - 1] +
			    pl_i(VM_MIN_KERNEL_ADDRESS, level) == i);
			nkptp[level - 1]++;
			va += nbpd[level - 1];
		}
	}
}

/*
 * pmap_growkernel: increase usage of KVM space
 *
 * => we allocate new PTPs for the kernel and install them in all
 *	the pmaps on the system.
 */

vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
	struct pmap *kpm = pmap_kernel(), *pm;
	int s, i;
	unsigned newpdes;
	long needed_kptp[PTP_LEVELS], target_nptp, old;

	if (maxkvaddr <= pmap_maxkvaddr)
		return pmap_maxkvaddr;

	maxkvaddr = x86_round_pdr(maxkvaddr);
	old = nkptp[PTP_LEVELS - 1];
	/*
	 * This loop could be optimized more, but pmap_growkernel()
	 * is called infrequently.
	 */
	for (i = PTP_LEVELS - 1; i >= 1; i--) {
		target_nptp = pl_i_roundup(maxkvaddr, i + 1) -
		    pl_i_roundup(VM_MIN_KERNEL_ADDRESS, i + 1);
		/*
		 * XXX only need to check toplevel.
		 */
		if (target_nptp > nkptpmax[i])
			panic("out of KVA space");
		KASSERT(target_nptp >= nkptp[i]);
		needed_kptp[i] = target_nptp - nkptp[i];
	}

	s = splhigh();	/* to be safe */
	mutex_enter(&kpm->pm_lock);
	pmap_alloc_level(normal_pdes, pmap_maxkvaddr, PTP_LEVELS, needed_kptp);

	/*
	 * If the number of top level entries changed, update all
	 * pmaps.
	 */
	if (needed_kptp[PTP_LEVELS - 1] != 0) {
		newpdes = nkptp[PTP_LEVELS - 1] - old;
		mutex_enter(&pmaps_lock);
		LIST_FOREACH(pm, &pmaps, pm_list) {
			memcpy(&pm->pm_pdir[PDIR_SLOT_KERN + old],
			       &kpm->pm_pdir[PDIR_SLOT_KERN + old],
			       newpdes * sizeof (pd_entry_t));
		}

		/* Invalidate the PDP cache. */
		pool_cache_invalidate(&pmap_pdp_cache);
		pmap_pdp_cache_generation++;

		mutex_exit(&pmaps_lock);
	}
	pmap_maxkvaddr = maxkvaddr;
	mutex_exit(&kpm->pm_lock);
	splx(s);

	return maxkvaddr;
}

#ifdef DEBUG
void pmap_dump(struct pmap *, vaddr_t, vaddr_t);

/*
 * pmap_dump: dump all the mappings from a pmap
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_dump(struct pmap *pmap, vaddr_t sva, vaddr_t eva)
{
	pt_entry_t *ptes, *pte;
	pd_entry_t **pdes;
	struct pmap *pmap2;
	vaddr_t blkendva;

	/*
	 * if end is out of range truncate.
	 * if (end == start) update to max.
	 */

	if (eva > VM_MAXUSER_ADDRESS || eva <= sva)
		eva = VM_MAXUSER_ADDRESS;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	rw_enter(&pmap_main_lock, RW_READER);
	pmap_map_ptes(pmap, &pmap2, &ptes, &pdes);	/* locks pmap */

	/*
	 * dumping a range of pages: we dump in PTP sized blocks (4MB)
	 */

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = x86_round_pdr(sva+1);
		if (blkendva > eva)
			blkendva = eva;

		/* valid block? */
		if (!pmap_pdes_valid(sva, pdes, NULL))
			continue;

		pte = &ptes[pl1_i(sva)];
		for (/* null */; sva < blkendva ; sva += PAGE_SIZE, pte++) {
			if (!pmap_valid_entry(*pte))
				continue;
			printf("va %#lx -> pa %#lx (pte=%#lx)\n",
			       sva, (unsigned long)*pte,
			       (unsigned long)*pte & PG_FRAME);
		}
	}
	pmap_unmap_ptes(pmap, pmap2);
	rw_exit(&pmap_main_lock);
}
#endif

/*
 * pmap_tlb_shootdown: invalidate pages on all CPUs using pmap 'pm'
 *
 * => always invalidates locally before returning
 * => returns before remote CPUs have invalidated
 * => must be called with preemption disabled
 */

void
pmap_tlb_shootdown(struct pmap *pm, vaddr_t sva, vaddr_t eva, pt_entry_t pte)
{
#ifdef MULTIPROCESSOR
	extern int _lock_cas(volatile uintptr_t *, uintptr_t, uintptr_t);
	extern bool x86_mp_online;
	struct cpu_info *ci;
	struct pmap_mbox *mb, *selfmb;
	CPU_INFO_ITERATOR cii;
	uintptr_t head;
	u_int count;
	int s;
#endif	/* MULTIPROCESSOR */
	struct cpu_info *self;
	bool kernel;

	KASSERT(eva == 0 || eva >= sva);

	if (pte & PG_PS)
		sva &= PG_LGFRAME;
	pte &= PG_G;
	self = curcpu();

	if (sva == (vaddr_t)-1LL) {
		kernel = true;
	} else {
		if (eva == 0)
			eva = sva + PAGE_SIZE;
		kernel = sva >= VM_MAXUSER_ADDRESS;
		KASSERT(kernel == (eva > VM_MAXUSER_ADDRESS));
	}

	/*
	 * If the range is larger than 32 pages, then invalidate
	 * everything.
	 */
	if (sva != (vaddr_t)-1LL && eva - sva > (32 * PAGE_SIZE)) {
		sva = (vaddr_t)-1LL;
		eva = sva;
	}

#ifdef MULTIPROCESSOR
	if (ncpu > 1 && x86_mp_online) {
		selfmb = &self->ci_pmap_cpu->pc_mbox;

		/*
		 * If the CPUs have no notion of global pages then
		 * reload of %cr3 is sufficient.
		 */
		if (pte != 0 && (cpu_feature & CPUID_PGE) == 0)
			pte = 0;

		if (pm == pmap_kernel()) {
			/*
			 * Mapped on all CPUs: use the broadcast mechanism.
			 * Once we have the lock, increment the counter.
			 */
			s = splvm();
			mb = &pmap_mbox;
			count = SPINLOCK_BACKOFF_MIN;
			do {
				if ((head = mb->mb_head) != mb->mb_tail) {
					splx(s);
					while ((head = mb->mb_head) !=
					    mb->mb_tail)
						SPINLOCK_BACKOFF(count);
					s = splvm();
				}
			} while (!_lock_cas(&mb->mb_head, head,
			    head + ncpu - 1));

			/*
			 * Once underway we must stay at IPL_VM until the
			 * IPI is dispatched.  Otherwise interrupt handlers
			 * on this CPU can deadlock against us.
			 */
			pmap_tlb_evcnt.ev_count++;
			mb->mb_pointer = self;
			mb->mb_addr1 = sva;
			mb->mb_addr2 = eva;
			mb->mb_global = pte;
			x86_ipi(LAPIC_TLB_BCAST_VECTOR, LAPIC_DEST_ALLEXCL,
			    LAPIC_DLMODE_FIXED);
			self->ci_need_tlbwait = 1;
			splx(s);
		} else if ((pm->pm_cpus & ~self->ci_cpumask) != 0 ||
		    (kernel && (pm->pm_kernel_cpus & ~self->ci_cpumask) != 0)) {
			/*
			 * We don't bother traversing the CPU list if only
			 * used by this CPU.
			 *
			 * We can't do global flushes with the multicast
			 * mechanism.
			 */
			KASSERT(pte == 0);

			/*
			 * Take ownership of the shootdown mailbox on each
			 * CPU, fill the details and fire it off.
			 */
			s = splvm();
			for (CPU_INFO_FOREACH(cii, ci)) {
				if (ci == self ||
				    !pmap_is_active(pm, ci, kernel) ||
				    !(ci->ci_flags & CPUF_RUNNING))
					continue;
				selfmb->mb_head++;
				mb = &ci->ci_pmap_cpu->pc_mbox;
				count = SPINLOCK_BACKOFF_MIN;
				while (!_lock_cas((uintptr_t *)&mb->mb_pointer,
				    0, (uintptr_t)&selfmb->mb_tail)) {
				    	splx(s);
					while (mb->mb_pointer != 0)
						SPINLOCK_BACKOFF(count);
					s = splvm();
				}
				mb->mb_addr1 = sva;
				mb->mb_addr2 = eva;
				mb->mb_global = pte;
				if (x86_ipi(LAPIC_TLB_MCAST_VECTOR,
				    ci->ci_apicid,
				    LAPIC_DLMODE_FIXED))
					panic("pmap_tlb_shootdown: ipi failed");
			}
			self->ci_need_tlbwait = 1;
			splx(s);
		}
	}
#endif	/* MULTIPROCESSOR */

	/* Update the current CPU before waiting for others. */
	if (!pmap_is_active(pm, self, kernel))
		return;

	if (sva == (vaddr_t)-1LL) {
		if (pte != 0)
			tlbflushg();
		else
			tlbflush();
	} else {
		do {
			pmap_update_pg(sva);
			sva += PAGE_SIZE;
		} while (sva < eva);
	}
}

/*
 * pmap_tlb_shootwait: wait for pending TLB shootdowns to complete
 *
 * => only waits for operations generated by the current CPU
 * => must be called with preemption disabled
 */

void
pmap_tlb_shootwait(void)
{
	struct cpu_info *self;
	struct pmap_mbox *mb;

	/*
	 * Anything to do?  XXX Really we want to avoid touching the cache
	 * lines of the two mailboxes, but the processor may read ahead.
	 */
	self = curcpu();
	if (!self->ci_need_tlbwait)
		return;
	self->ci_need_tlbwait = 0;

	/* If we own the global mailbox, wait for it to drain. */
	mb = &pmap_mbox;
	while (mb->mb_pointer == self && mb->mb_head != mb->mb_tail)
		x86_pause();

	/* If we own other CPU's mailboxes, wait for them to drain. */
	mb = &self->ci_pmap_cpu->pc_mbox;
	KASSERT(mb->mb_pointer != &mb->mb_tail);
	while (mb->mb_head != mb->mb_tail)
		x86_pause();
}

/*
 * pmap_update: process deferred invalidations
 */

void
pmap_update(struct pmap *pm)
{

	crit_enter();
	pmap_tlb_shootwait();
	crit_exit();
}
