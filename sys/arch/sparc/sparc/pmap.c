/*	$NetBSD: pmap.c,v 1.141.2.3 1999/07/02 17:03:10 perry Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	@(#)pmap.c	8.4 (Berkeley) 2/5/94
 *
 */

/*
 * SPARC physical map management code.
 * Does not function on multiprocessors (yet).
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/pool.h>
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>

#include <uvm/uvm.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/kcore.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>

#ifdef DEBUG
#define PTE_BITS "\20\40V\37W\36S\35NC\33IO\32U\31M"
#define PTE_BITS4M "\20\10C\7M\6R\5ACC3\4ACC2\3ACC1\2TYP2\1TYP1"
#endif

/*
 * The SPARCstation offers us the following challenges:
 *
 *   1. A virtual address cache.  This is, strictly speaking, not
 *	part of the architecture, but the code below assumes one.
 *	This is a write-through cache on the 4c and a write-back cache
 *	on others.
 *
 *   2. (4/4c only) An MMU that acts like a cache.  There is not enough
 *	space in the MMU to map everything all the time.  Instead, we need
 *	to load MMU with the `working set' of translations for each
 *	process. The sun4m does not act like a cache; tables are maintained
 *	in physical memory.
 *
 *   3.	Segmented virtual and physical spaces.  The upper 12 bits of
 *	a virtual address (the virtual segment) index a segment table,
 *	giving a physical segment.  The physical segment selects a
 *	`Page Map Entry Group' (PMEG) and the virtual page number---the
 *	next 5 or 6 bits of the virtual address---select the particular
 *	`Page Map Entry' for the page.  We call the latter a PTE and
 *	call each Page Map Entry Group a pmeg (for want of a better name).
 *	Note that the sun4m has an unsegmented 36-bit physical space.
 *
 *	Since there are no valid bits in the segment table, the only way
 *	to have an invalid segment is to make one full pmeg of invalid PTEs.
 *	We use the last one (since the ROM does as well) (sun4/4c only)
 *
 *   4. Discontiguous physical pages.  The Mach VM expects physical pages
 *	to be in one sequential lump.
 *
 *   5. The MMU is always on: it is not possible to disable it.  This is
 *	mainly a startup hassle.
 */

struct pmap_stats {
	int	ps_unlink_pvfirst;	/* # of pv_unlinks on head */
	int	ps_unlink_pvsearch;	/* # of pv_unlink searches */
	int	ps_changeprots;		/* # of calls to changeprot */
	int	ps_useless_changeprots;	/* # of changeprots for wiring */
	int	ps_enter_firstpv;	/* pv heads entered */
	int	ps_enter_secondpv;	/* pv nonheads entered */
	int	ps_useless_changewire;	/* useless wiring changes */
	int	ps_npg_prot_all;	/* # of active pages protected */
	int	ps_npg_prot_actual;	/* # pages actually affected */
	int	ps_npmeg_free;		/* # of free pmegs */
	int	ps_npmeg_locked;	/* # of pmegs on locked list */
	int	ps_npmeg_lru;		/* # of pmegs on lru list */
} pmap_stats;

#ifdef DEBUG
#define	PDB_CREATE	0x0001
#define	PDB_DESTROY	0x0002
#define	PDB_REMOVE	0x0004
#define	PDB_CHANGEPROT	0x0008
#define	PDB_ENTER	0x0010
#define	PDB_FOLLOW	0x0020

#define	PDB_MMU_ALLOC	0x0100
#define	PDB_MMU_STEAL	0x0200
#define	PDB_CTX_ALLOC	0x0400
#define	PDB_CTX_STEAL	0x0800
#define	PDB_MMUREG_ALLOC	0x1000
#define	PDB_MMUREG_STEAL	0x2000
#define	PDB_CACHESTUFF	0x4000
#define	PDB_SWITCHMAP	0x8000
#define	PDB_SANITYCHK	0x10000
int	pmapdebug = 0;
#endif

#if 0
#define	splpmap() splimp()
#endif

/*
 * First and last managed physical addresses.
 */
paddr_t	vm_first_phys, vm_num_phys;

/*
 * For each managed physical page, there is a list of all currently
 * valid virtual mappings of that page.  Since there is usually one
 * (or zero) mapping per page, the table begins with an initial entry,
 * rather than a pointer; this head entry is empty iff its pv_pmap
 * field is NULL.
 *
 * Note that these are per machine independent page (so there may be
 * only one for every two hardware pages, e.g.).  Since the virtual
 * address is aligned on a page boundary, the low order bits are free
 * for storing flags.  Only the head of each list has flags.
 *
 * THIS SHOULD BE PART OF THE CORE MAP
 */
struct pvlist {
	struct		pvlist *pv_next;	/* next pvlist, if any */
	struct		pmap *pv_pmap;		/* pmap of this va */
	vaddr_t		pv_va;			/* virtual address */
	int		pv_flags;		/* flags (below) */
};

/*
 * Flags in pv_flags.  Note that PV_MOD must be 1 and PV_REF must be 2
 * since they must line up with the bits in the hardware PTEs (see pte.h).
 * SUN4M bits are at a slightly different location in the PTE.
 * Note: the REF, MOD and ANC flag bits occur only in the head of a pvlist.
 * The cacheable bit (either PV_NC or PV_C4M) is meaningful in each
 * individual pv entry.
 */
#define PV_MOD		1	/* page modified */
#define PV_REF		2	/* page referenced */
#define PV_NC		4	/* page cannot be cached */
#define PV_REF4M	1	/* page referenced (SRMMU) */
#define PV_MOD4M	2	/* page modified (SRMMU) */
#define PV_C4M		4	/* page _can_ be cached (SRMMU) */
#define PV_ANC		0x10	/* page has incongruent aliases */

struct pvlist *pv_table;	/* array of entries, one per physical page */

#define pvhead(pa)	(&pv_table[((pa) - vm_first_phys) >> PGSHIFT])

static vsize_t pv_table_map __P((paddr_t, int));
static paddr_t pv_physmem;
static struct pool pv_pool;


/*
 * Each virtual segment within each pmap is either valid or invalid.
 * It is valid if pm_npte[VA_VSEG(va)] is not 0.  This does not mean
 * it is in the MMU, however; that is true iff pm_segmap[VA_VSEG(va)]
 * does not point to the invalid PMEG.
 *
 * In the older SPARC architectures (pre-4m), page tables are cached in the
 * MMU. The following discussion applies to these architectures:
 *
 * If a virtual segment is valid and loaded, the correct PTEs appear
 * in the MMU only.  If it is valid and unloaded, the correct PTEs appear
 * in the pm_pte[VA_VSEG(va)] only.  However, some effort is made to keep
 * the software copies consistent enough with the MMU so that libkvm can
 * do user address translations.  In particular, pv_changepte() and
 * pmap_enu() maintain consistency, while less critical changes are
 * not maintained.  pm_pte[VA_VSEG(va)] always points to space for those
 * PTEs, unless this is the kernel pmap, in which case pm_pte[x] is not
 * used (sigh).
 *
 * Each PMEG in the MMU is either free or contains PTEs corresponding to
 * some pmap and virtual segment.  If it contains some PTEs, it also contains
 * reference and modify bits that belong in the pv_table.  If we need
 * to steal a PMEG from some process (if we need one and none are free)
 * we must copy the ref and mod bits, and update pm_segmap in the other
 * pmap to show that its virtual segment is no longer in the MMU.
 *
 * There are 128 PMEGs in a small Sun-4, of which only a few dozen are
 * tied down permanently, leaving `about' 100 to be spread among
 * running processes.  These are managed as an LRU cache.  Before
 * calling the VM paging code for a user page fault, the fault handler
 * calls mmu_load(pmap, va) to try to get a set of PTEs put into the
 * MMU.  mmu_load will check the validity of the segment and tell whether
 * it did something.
 *
 * Since I hate the name PMEG I call this data structure an `mmu entry'.
 * Each mmuentry is on exactly one of three `usage' lists: free, LRU,
 * or locked.  The LRU list is for user processes; the locked list is
 * for kernel entries; both are doubly linked queues headed by `mmuhd's.
 * The free list is a simple list, headed by a free list pointer.
 *
 * In the sun4m architecture using the SPARC Reference MMU (SRMMU), three
 * levels of page tables are maintained in physical memory. We use the same
 * structures as with the 3-level old-style MMU (pm_regmap, pm_segmap,
 * rg_segmap, sg_pte, etc) to maintain kernel-edible page tables; we also
 * build a parallel set of physical tables that can be used by the MMU.
 * (XXX: This seems redundant, but is it necessary for the unified kernel?)
 *
 * If a virtual segment is valid, its entries will be in both parallel lists.
 * If it is not valid, then its entry in the kernel tables will be zero, and
 * its entry in the MMU tables will either be nonexistent or zero as well.
 *
 * The Reference MMU generally uses a Translation Look-aside Buffer (TLB)
 * to cache the result of recently executed page table walks. When
 * manipulating page tables, we need to ensure consistency of the
 * in-memory and TLB copies of the page table entries. This is handled
 * by flushing (and invalidating) a TLB entry when appropriate before
 * altering an in-memory page table entry.
 */
struct mmuentry {
	TAILQ_ENTRY(mmuentry)	me_list;	/* usage list link */
	TAILQ_ENTRY(mmuentry)	me_pmchain;	/* pmap owner link */
	struct	pmap *me_pmap;		/* pmap, if in use */
	u_short	me_vreg;		/* associated virtual region/segment */
	u_short	me_vseg;		/* associated virtual region/segment */
	u_short	me_cookie;		/* hardware SMEG/PMEG number */
};
struct mmuentry *mmusegments;	/* allocated in pmap_bootstrap */
struct mmuentry *mmuregions;	/* allocated in pmap_bootstrap */

struct mmuhd segm_freelist, segm_lru, segm_locked;
struct mmuhd region_freelist, region_lru, region_locked;

int	seginval;		/* [4/4c] the invalid segment number */
int	reginval;		/* [4/3mmu] the invalid region number */

/*
 * (sun4/4c)
 * A context is simply a small number that dictates which set of 4096
 * segment map entries the MMU uses.  The Sun 4c has eight such sets.
 * These are alloted in an `almost MRU' fashion.
 * (sun4m)
 * A context is simply a small number that indexes the context table, the
 * root-level page table mapping 4G areas. Each entry in this table points
 * to a 1st-level region table. A SPARC reference MMU will usually use 16
 * such contexts, but some offer as many as 64k contexts; the theoretical
 * maximum is 2^32 - 1, but this would create overlarge context tables.
 *
 * Each context is either free or attached to a pmap.
 *
 * Since the virtual address cache is tagged by context, when we steal
 * a context we have to flush (that part of) the cache.
 */
union ctxinfo {
	union	ctxinfo *c_nextfree;	/* free list (if free) */
	struct	pmap *c_pmap;		/* pmap (if busy) */
};

#define ncontext	(cpuinfo.mmu_ncontext)
#define ctx_kick	(cpuinfo.ctx_kick)
#define ctx_kickdir	(cpuinfo.ctx_kickdir)
#define ctx_freelist	(cpuinfo.ctx_freelist)

void	ctx_alloc __P((struct pmap *));
void	ctx_free __P((struct pmap *));

#if 0
union ctxinfo *ctxinfo;		/* allocated at in pmap_bootstrap */

union	ctxinfo *ctx_freelist;	/* context free list */
int	ctx_kick;		/* allocation rover when none free */
int	ctx_kickdir;		/* ctx_kick roves both directions */

char	*ctxbusyvector;		/* [4m] tells what contexts are busy (XXX)*/
#endif

caddr_t	vpage[2];		/* two reserved MD virtual pages */
#if defined(SUN4M)
int	*vpage_pte[2];		/* pte location of vpage[] */
#endif
caddr_t	vmmap;			/* one reserved MI vpage for /dev/mem */
caddr_t	vdumppages;		/* 32KB worth of reserved dump pages */

smeg_t		tregion;	/* [4/3mmu] Region for temporary mappings */

struct pmap	kernel_pmap_store;		/* the kernel's pmap */
struct regmap	kernel_regmap_store[NKREG];	/* the kernel's regmap */
struct segmap	kernel_segmap_store[NKREG*NSEGRG];/* the kernel's segmaps */

#if defined(SUN4M)
u_int 	*kernel_regtable_store;		/* 1k of storage to map the kernel */
u_int	*kernel_segtable_store;		/* 2k of storage to map the kernel */
u_int	*kernel_pagtable_store;		/* 128k of storage to map the kernel */

u_int	*kernel_iopte_table;		/* 64k of storage for iommu */
u_int 	kernel_iopte_table_pa;

/*
 * Memory pools and back-end supplier for SRMMU page tables.
 * Share a pool between the level 2 and level 3 page tables,
 * since these are equal in size.
 */
static struct pool L1_pool;
static struct pool L23_pool;

static void *pgt_page_alloc __P((unsigned long, int, int));
static void  pgt_page_free __P((void *, unsigned long, int));

#endif

#define	MA_SIZE	32		/* size of memory descriptor arrays */
struct	memarr pmemarr[MA_SIZE];/* physical memory regions */
int	npmemarr;		/* number of entries in pmemarr */
/*static*/ paddr_t	avail_start;	/* first free physical page */
/*static*/ paddr_t	avail_end;	/* last free physical page */
/*static*/ paddr_t	unavail_gap_start;/* first stolen free phys page */
/*static*/ paddr_t	unavail_gap_end;/* last stolen free physical page */
/*static*/ vaddr_t	virtual_avail;	/* first free virtual page number */
/*static*/ vaddr_t	virtual_end;	/* last free virtual page number */

static void pmap_page_upload __P((void));
void pmap_pinit __P((pmap_t));
void pmap_release __P((pmap_t));

int mmu_has_hole;

vaddr_t prom_vstart;	/* For /dev/kmem */
vaddr_t prom_vend;

/*
 * Memory pool for pmap structures.
 */
static struct pool pmap_pmap_pool;

#if defined(SUN4)
/*
 * [sun4]: segfixmask: on some systems (4/110) "getsegmap()" returns a
 * partly invalid value. getsegmap returns a 16 bit value on the sun4,
 * but only the first 8 or so bits are valid (the rest are *supposed* to
 * be zero. On the 4/110 the bits that are supposed to be zero are
 * all one instead. e.g. KERNBASE is usually mapped by pmeg number zero.
 * On a 4/300 getsegmap(KERNBASE) == 0x0000, but
 * on a 4/100 getsegmap(KERNBASE) == 0xff00
 *
 * This confuses mmu_reservemon() and causes it to not reserve the PROM's
 * pmegs. Then the PROM's pmegs get used during autoconfig and everything
 * falls apart!  (not very fun to debug, BTW.)
 *
 * solution: mask the invalid bits in the getsetmap macro.
 */

static u_long segfixmask = 0xffffffff; /* all bits valid to start */
#else
#define segfixmask 0xffffffff	/* It's in getsegmap's scope */
#endif

/*
 * pseudo-functions for mnemonic value
 */
#define	getsegmap(va)		(CPU_ISSUN4C \
					? lduba(va, ASI_SEGMAP) \
					: (lduha(va, ASI_SEGMAP) & segfixmask))
#define	setsegmap(va, pmeg)	(CPU_ISSUN4C \
					? stba(va, ASI_SEGMAP, pmeg) \
					: stha(va, ASI_SEGMAP, pmeg))

/* 3-level sun4 MMU only: */
#define	getregmap(va)		((unsigned)lduha((va)+2, ASI_REGMAP) >> 8)
#define	setregmap(va, smeg)	stha((va)+2, ASI_REGMAP, (smeg << 8))

#if defined(SUN4M)
void		setpgt4m __P((int *ptep, int pte));
void		setpte4m __P((vaddr_t va, int pte));
void		setptesw4m __P((struct pmap *pm, vaddr_t va, int pte));
static u_int	getptesw4m __P((struct pmap *pm, vaddr_t va));
#endif

/* Function pointer messiness for supporting multiple sparc architectures
 * within a single kernel: notice that there are two versions of many of the
 * functions within this file/module, one for the sun4/sun4c and the other
 * for the sun4m. For performance reasons (since things like pte bits don't
 * map nicely between the two architectures), there are separate functions
 * rather than unified functions which test the cputyp variable. If only
 * one architecture is being used, then the non-suffixed function calls
 * are macro-translated into the appropriate xxx4_4c or xxx4m call. If
 * multiple architectures are defined, the calls translate to (*xxx_p),
 * i.e. they indirect through function pointers initialized as appropriate
 * to the run-time architecture in pmap_bootstrap. See also pmap.h.
 */

#if defined(SUN4M)
static void mmu_setup4m_L1 __P((int, struct pmap *));
static void mmu_setup4m_L2 __P((int, struct regmap *));
static void  mmu_setup4m_L3 __P((int, struct segmap *));
/*static*/ void	mmu_reservemon4m __P((struct pmap *));

/*static*/ void pmap_rmk4m __P((struct pmap *, vaddr_t, vaddr_t, int, int));
/*static*/ void pmap_rmu4m __P((struct pmap *, vaddr_t, vaddr_t, int, int));
/*static*/ void pmap_enk4m __P((struct pmap *, vaddr_t, vm_prot_t,
				int, struct pvlist *, int));
/*static*/ void pmap_enu4m __P((struct pmap *, vaddr_t, vm_prot_t,
				int, struct pvlist *, int));
/*static*/ void pv_changepte4m __P((struct pvlist *, int, int));
/*static*/ int  pv_syncflags4m __P((struct pvlist *));
/*static*/ int  pv_link4m __P((struct pvlist *, struct pmap *, vaddr_t, int));
/*static*/ void pv_unlink4m __P((struct pvlist *, struct pmap *, vaddr_t));
#endif

#if defined(SUN4) || defined(SUN4C)
/*static*/ void	mmu_reservemon4_4c __P((int *, int *));
/*static*/ void pmap_rmk4_4c __P((struct pmap *, vaddr_t, vaddr_t, int, int));
/*static*/ void pmap_rmu4_4c __P((struct pmap *, vaddr_t, vaddr_t, int, int));
/*static*/ void pmap_enk4_4c __P((struct pmap *, vaddr_t, vm_prot_t,
				  int, struct pvlist *, int));
/*static*/ void pmap_enu4_4c __P((struct pmap *, vaddr_t, vm_prot_t,
				  int, struct pvlist *, int));
/*static*/ void pv_changepte4_4c __P((struct pvlist *, int, int));
/*static*/ int  pv_syncflags4_4c __P((struct pvlist *));
/*static*/ int  pv_link4_4c __P((struct pvlist *, struct pmap *, vaddr_t, int));
/*static*/ void pv_unlink4_4c __P((struct pvlist *, struct pmap *, vaddr_t));
#endif

#if !defined(SUN4M) && (defined(SUN4) || defined(SUN4C))
#define		pmap_rmk	pmap_rmk4_4c
#define		pmap_rmu	pmap_rmu4_4c

#elif defined(SUN4M) && !(defined(SUN4) || defined(SUN4C))
#define		pmap_rmk	pmap_rmk4m
#define		pmap_rmu	pmap_rmu4m

#else  /* must use function pointers */

/* function pointer declarations */
/* from pmap.h: */
void		(*pmap_clear_modify_p) __P((paddr_t pa));
void		(*pmap_clear_reference_p) __P((paddr_t pa));
void		(*pmap_copy_page_p) __P((paddr_t, paddr_t));
void		(*pmap_enter_p) __P((pmap_t, vaddr_t, paddr_t, vm_prot_t,
		    boolean_t, vm_prot_t));
paddr_t		(*pmap_extract_p) __P((pmap_t, vaddr_t));
boolean_t	(*pmap_is_modified_p) __P((paddr_t pa));
boolean_t	(*pmap_is_referenced_p) __P((paddr_t pa));
void		(*pmap_page_protect_p) __P((paddr_t, vm_prot_t));
void		(*pmap_protect_p) __P((pmap_t, vaddr_t, vaddr_t, vm_prot_t));
void		(*pmap_zero_page_p) __P((paddr_t));
void		(*pmap_changeprot_p) __P((pmap_t, vaddr_t, vm_prot_t, int));
/* local: */
void 		(*pmap_rmk_p) __P((struct pmap *, vaddr_t, vaddr_t, int, int));
void 		(*pmap_rmu_p) __P((struct pmap *, vaddr_t, vaddr_t, int, int));

#define		pmap_rmk	(*pmap_rmk_p)
#define		pmap_rmu	(*pmap_rmu_p)

#endif

/* --------------------------------------------------------------*/

/*
 * Next we have some Sun4m-specific routines which have no 4/4c
 * counterparts, or which are 4/4c macros.
 */

#if defined(SUN4M)
/*
 * Macros which implement SRMMU TLB flushing/invalidation
 */
#define tlb_flush_page(va)    \
	sta(((vaddr_t)(va) & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP, 0)

#define tlb_flush_segment(vr, vs) \
	sta(((vr)<<RGSHIFT) | ((vs)<<SGSHIFT) | ASI_SRMMUFP_L2, ASI_SRMMUFP,0)

#define tlb_flush_region(vr) \
	sta(((vr) << RGSHIFT) | ASI_SRMMUFP_L1, ASI_SRMMUFP, 0)

#define tlb_flush_context()	sta(ASI_SRMMUFP_L0, ASI_SRMMUFP, 0)
#define tlb_flush_all()		sta(ASI_SRMMUFP_LN, ASI_SRMMUFP, 0)

static u_int	VA2PA __P((caddr_t));
static u_long	srmmu_bypass_read __P((u_long));

/*
 * VA2PA(addr) -- converts a virtual address to a physical address using
 * the MMU's currently-installed page tables. As a side effect, the address
 * translation used may cause the associated pte to be encached. The correct
 * context for VA must be set before this is called.
 *
 * This routine should work with any level of mapping, as it is used
 * during bootup to interact with the ROM's initial L1 mapping of the kernel.
 */
static __inline u_int
VA2PA(addr)
	caddr_t addr;
{
	u_int pte;

	/* we'll use that handy SRMMU flush/probe! %%%: make consts below! */
	/* Try each level in turn until we find a valid pte. Otherwise panic */

	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP);
	/* Unlock fault status; required on Hypersparc modules */
	(void)lda(SRMMU_SFSR, ASI_SRMMU);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xfff));

	/* A `TLB Flush Entire' is required before any L0, L1 or L2 probe */
	tlb_flush_all();

	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L2, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0x3ffff));
	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L1, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffff));
	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L0, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffffff));

	panic("VA2PA: Asked to translate unmapped VA %p", addr);
}

/*
 * Get the page table entry (PTE) for va by looking it up in the software
 * page tables. These are the same tables that are used by the MMU; this
 * routine allows easy access to the page tables even if the context
 * corresponding to the table is not loaded or selected.
 * This routine should NOT be used if there is any chance that the desired
 * pte is in the TLB cache, since it will return stale data in that case.
 * For that case, and for general use, use getpte4m, which is much faster
 * and avoids walking in-memory page tables if the page is in the cache.
 * Note also that this routine only works if a kernel mapping has been
 * installed for the given page!
 */
__inline u_int
getptesw4m(pm, va)		/* Assumes L3 mapping! */
	struct pmap *pm;
	vaddr_t va;
{
	struct regmap *rm;
	struct segmap *sm;

	rm = &pm->pm_regmap[VA_VREG(va)];
#ifdef DEBUG
	if (rm == NULL)
		panic("getptesw4m: no regmap entry");
#endif
	sm = &rm->rg_segmap[VA_VSEG(va)];
#ifdef DEBUG
	if (sm == NULL)
		panic("getptesw4m: no segmap");
#endif
	return (sm->sg_pte[VA_SUN4M_VPG(va)]); 	/* return pte */
}

__inline void
setpgt4m(ptep, pte)
	int *ptep;
	int pte;
{
	swap(ptep, pte);
#if 1
	/* XXX - uncaching in pgt_page_alloc() below is not yet quite Okay */
	if (cpuinfo.cpu_type == CPUTYP_SS1_MBUS_NOMXCC)
		cpuinfo.pcache_flush_line((int)ptep, VA2PA((caddr_t)ptep));
#endif
}


/*
 * Set the page table entry for va to pte. Only affects software MMU page-
 * tables (the in-core pagetables read by the MMU). Ignores TLB, and
 * thus should _not_ be called if the pte translation could be in the TLB.
 * In this case, use setpte4m().
 */
__inline void
setptesw4m(pm, va, pte)
	struct pmap *pm;
	vaddr_t va;
	int pte;
{
	struct regmap *rm;
	struct segmap *sm;

	rm = &pm->pm_regmap[VA_VREG(va)];

#ifdef DEBUG
	if (pm->pm_regmap == NULL || rm == NULL)
		panic("setptesw4m: no regmap entry");
#endif
	sm = &rm->rg_segmap[VA_VSEG(va)];

#ifdef DEBUG
	if (rm->rg_segmap == NULL || sm == NULL || sm->sg_pte == NULL)
		panic("setptesw4m: no segmap for va %p", (caddr_t)va);
#endif
	setpgt4m(sm->sg_pte + VA_SUN4M_VPG(va), pte);
}

/* Set the page table entry for va to pte. */
__inline void
setpte4m(va, pte)
	vaddr_t va;
	int pte;
{
	struct pmap *pm;
	struct regmap *rm;
	struct segmap *sm;

	if (getcontext4m() != 0)
		panic("setpte4m: user context");

	pm = pmap_kernel();

	/* Note: inline version of setptesw4m() */
#ifdef DEBUG
	if (pm->pm_regmap == NULL)
		panic("setpte4m: no regmap entry");
#endif
	rm = &pm->pm_regmap[VA_VREG(va)];
	sm = &rm->rg_segmap[VA_VSEG(va)];

#ifdef DEBUG
	if (rm->rg_segmap == NULL)
		panic("setpte4m: no segmap for va %p (rp=%p)",
			(caddr_t)va, (caddr_t)rm);

	if (sm->sg_pte == NULL)
		panic("setpte4m: no pte for va %p (rp=%p, sp=%p)",
		      (caddr_t)va, rm, sm);
#endif
	tlb_flush_page(va);
	setpgt4m(sm->sg_pte + VA_SUN4M_VPG(va), pte);
}

void	pcache_flush __P((caddr_t, caddr_t, int));
void
pcache_flush(va, pa, n)
	caddr_t	va, pa;
	int	n;
{
	void (*f)__P((int,int)) = cpuinfo.pcache_flush_line;

	while ((n -= 4) >= 0)
		(*f)((u_int)va+n, (u_int)pa+n);
}

/*
 * Page table pool back-end.
 */
void *
pgt_page_alloc(sz, flags, mtype)
	unsigned long sz;
	int flags;
	int mtype;
{
	caddr_t p;

	p = (caddr_t)uvm_km_kmemalloc(kernel_map, uvm.kernel_object,
				      (vsize_t)sz, UVM_KMF_NOWAIT);

	if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) == 0) {
		pcache_flush(p, (caddr_t)VA2PA(p), sz);
		kvm_uncache(p, sz/NBPG);
	}
	return (p);
}       
   
void
pgt_page_free(v, sz, mtype)
	void *v;
	unsigned long sz;
	int mtype;
{
	uvm_km_free(kernel_map, (vaddr_t)v, sz);
}
#endif /* 4m only */

/*----------------------------------------------------------------*/

/*
 * The following three macros are to be used in sun4/sun4c code only.
 */
#if defined(SUN4_MMU3L)
#define CTX_USABLE(pm,rp) (					\
		((pm)->pm_ctx != NULL &&			\
		 (!HASSUN4_MMU3L || (rp)->rg_smeg != reginval))	\
)
#else
#define CTX_USABLE(pm,rp)	((pm)->pm_ctx != NULL )
#endif

#define GAP_WIDEN(pm,vr) do if (CPU_ISSUN4OR4C) {	\
	if (vr + 1 == pm->pm_gap_start)			\
		pm->pm_gap_start = vr;			\
	if (vr == pm->pm_gap_end)			\
		pm->pm_gap_end = vr + 1;		\
} while (0)

#define GAP_SHRINK(pm,vr) do if (CPU_ISSUN4OR4C) {			\
	int x;								\
	x = pm->pm_gap_start + (pm->pm_gap_end - pm->pm_gap_start) / 2;	\
	if (vr > x) {							\
		if (vr < pm->pm_gap_end)				\
			pm->pm_gap_end = vr;				\
	} else {							\
		if (vr >= pm->pm_gap_start && x != pm->pm_gap_start)	\
			pm->pm_gap_start = vr + 1;			\
	}								\
} while (0)


static void get_phys_mem __P((void));
static void sortm __P((struct memarr *, int));
void	pv_flushcache __P((struct pvlist *));
void	kvm_iocache __P((caddr_t, int));

#ifdef DEBUG
void	pm_check __P((char *, struct pmap *));
void	pm_check_k __P((char *, struct pmap *));
void	pm_check_u __P((char *, struct pmap *));
#endif


/*
 * Grab physical memory list and use it to compute `physmem' and
 * `avail_end'. The latter is used in conjunction with
 * `avail_start' to dispatch left-over physical pages to the
 * VM system.
 */
void
get_phys_mem()
{
	struct memarr *mp;
	int i;

	npmemarr = makememarr(pmemarr, MA_SIZE, MEMARR_AVAILPHYS);
	sortm(pmemarr, npmemarr);
	if (pmemarr[0].addr != 0)
		panic("pmap_bootstrap: no memory?!");

	avail_end = pmemarr[npmemarr-1].addr + pmemarr[npmemarr-1].len;
	for (physmem = 0, mp = pmemarr, i = npmemarr; --i >= 0; mp++)
		physmem += btoc(mp->len);
}

/*
 * Sort a memory array by address.
 */
static void
sortm(mp, n)
	struct memarr *mp;
	int n;
{
	struct memarr *mpj;
	int i, j;
	paddr_t addr;
	psize_t len;

	/* Insertion sort.  This is O(n^2), but so what? */
	for (i = 1; i < n; i++) {
		/* save i'th entry */
		addr = mp[i].addr;
		len = mp[i].len;
		/* find j such that i'th entry goes before j'th */
		for (j = 0, mpj = mp; j < i; j++, mpj++)
			if (addr < mpj->addr)
				break;
		/* slide up any additional entries */
		memmove(mpj + 1, mpj, (i - j) * sizeof(*mp));
		mpj->addr = addr;
		mpj->len = len;
	}
}

/*
 * Support functions for vm_page_bootstrap().
 */

/*
 * How much virtual space does this kernel have?
 * (After mapping kernel text, data, etc.)
 */
void
pmap_virtual_space(v_start, v_end)
        vaddr_t *v_start;
        vaddr_t *v_end;
{
        *v_start = virtual_avail;
        *v_end   = virtual_end;
}

/*
 * Helper routine that hands off available physical pages to the VM system.
 */
static void
pmap_page_upload()
{
	int	n = 0;
	paddr_t	start, end, avail_next;

	avail_next = avail_start;
	if (unavail_gap_start != 0) {
		/* First, the gap we created in pmap_bootstrap() */
		if (avail_next != unavail_gap_start)
			/* Avoid empty ranges */
			uvm_page_physload(
				atop(avail_next),
				atop(unavail_gap_start),
				atop(avail_next),
				atop(unavail_gap_start),
				VM_FREELIST_DEFAULT);
		avail_next = unavail_gap_end;
	}

	for (n = 0; n < npmemarr; n++) {
		/*
		 * Assume `avail_next' is always in the first segment; we
		 * already made that assumption in pmap_bootstrap()..
		 */
		start = (n == 0) ? avail_next : pmemarr[n].addr;
		end = pmemarr[n].addr + pmemarr[n].len;
		if (start == end)
			continue;

		uvm_page_physload(
			atop(start),
			atop(end),
			atop(start),
			atop(end), VM_FREELIST_DEFAULT);
	}

}

#if 0
/*
 * pmap_page_index()
 *
 * Given a physical address, return a page index.
 *
 * There can be some values that we never return (i.e. a hole)
 * as long as the range of indices returned by this function
 * is smaller than the value returned by pmap_free_pages().
 * The returned index does NOT need to start at zero.
 *
 */
int
pmap_page_index(pa)
	paddr_t pa;
{
	paddr_t idx;
	int nmem;
	struct memarr *mp;

#ifdef  DIAGNOSTIC
	if (pa < avail_start || pa >= avail_end)
		panic("pmap_page_index: pa=0x%lx", pa);
#endif

	for (idx = 0, mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		if (pa >= mp->addr && pa < mp->addr + mp->len)
			break;
		idx += atop(mp->len);
	}

	return (int)(idx + atop(pa - mp->addr));
}
#endif

int
pmap_pa_exists(pa)
	paddr_t pa;
{
	int nmem;
	struct memarr *mp;

	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		if (pa >= mp->addr && pa < mp->addr + mp->len)
			return 1;
	}

	return 0;
}

/* update pv_flags given a valid pte */
#define	MR4_4C(pte) (((pte) >> PG_M_SHIFT) & (PV_MOD | PV_REF))
#define MR4M(pte) (((pte) >> PG_M_SHIFT4M) & (PV_MOD4M | PV_REF4M))

/*----------------------------------------------------------------*/

/*
 * Agree with the monitor ROM as to how many MMU entries are
 * to be reserved, and map all of its segments into all contexts.
 *
 * Unfortunately, while the Version 0 PROM had a nice linked list of
 * taken virtual memory, the Version 2 PROM provides instead a convoluted
 * description of *free* virtual memory.  Rather than invert this, we
 * resort to two magic constants from the PROM vector description file.
 */
#if defined(SUN4) || defined(SUN4C)
void
mmu_reservemon4_4c(nrp, nsp)
	int *nrp, *nsp;
{
	u_int va = 0, eva = 0;
	int mmuseg, i, nr, ns, vr, lastvr;
#if defined(SUN4_MMU3L)
	int mmureg;
#endif
	struct regmap *rp;

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		panic("mmu_reservemon4_4c called on Sun4M machine");
		return;
	}
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		prom_vstart = va = OLDMON_STARTVADDR;
		prom_vend = eva = OLDMON_ENDVADDR;
	}
#endif
#if defined(SUN4C)
	if (CPU_ISSUN4C) {
		prom_vstart = va = OPENPROM_STARTVADDR;
		prom_vend = eva = OPENPROM_ENDVADDR;
	}
#endif
	ns = *nsp;
	nr = *nrp;
	lastvr = 0;
	while (va < eva) {
		vr = VA_VREG(va);
		rp = &pmap_kernel()->pm_regmap[vr];

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L && vr != lastvr) {
			lastvr = vr;
			mmureg = getregmap(va);
			if (mmureg < nr)
				rp->rg_smeg = nr = mmureg;
			/*
			 * On 3-level MMU machines, we distribute regions,
			 * rather than segments, amongst the contexts.
			 */
			for (i = ncontext; --i > 0;)
				prom_setcontext(i, (caddr_t)va, mmureg);
		}
#endif
		mmuseg = getsegmap(va);
		if (mmuseg < ns)
			ns = mmuseg;

		if (!HASSUN4_MMU3L)
			for (i = ncontext; --i > 0;)
				prom_setcontext(i, (caddr_t)va, mmuseg);

		if (mmuseg == seginval) {
			va += NBPSG;
			continue;
		}
		/*
		 * Another PROM segment. Enter into region map.
		 * Assume the entire segment is valid.
		 */
		rp->rg_nsegmap += 1;
		rp->rg_segmap[VA_VSEG(va)].sg_pmeg = mmuseg;
		rp->rg_segmap[VA_VSEG(va)].sg_npte = NPTESG;

		/* PROM maps its memory user-accessible: fix it. */
		for (i = NPTESG; --i >= 0; va += NBPG)
			setpte4(va, getpte4(va) | PG_S);
	}
	*nsp = ns;
	*nrp = nr;
	return;
}
#endif

#if defined(SUN4M) /* Sun4M versions of above */

u_long
srmmu_bypass_read(paddr)
	u_long	paddr;
{
	unsigned long v;

	if (/*cpuinfo.cpu_impl == 4 && */cpuinfo.mxcc) {
		/*
		 * We're going to have to use MMU passthrough. If we're on
		 * a Viking MicroSparc without an mbus, we need to turn
		 * off traps and set the AC bit at 0x8000 in the MMU's
		 * control register.  Ugh.
		 */

		unsigned long s = lda(SRMMU_PCR,ASI_SRMMU);

		/* set MMU AC bit */
		sta(SRMMU_PCR, ASI_SRMMU, s | VIKING_PCR_AC);
		v = lda(paddr, ASI_BYPASS);
		sta(SRMMU_PCR, ASI_SRMMU, s);
	} else
		v = lda(paddr, ASI_BYPASS);

	return (v);
}


/*
 * Take the monitor's initial page table layout, convert it to 3rd-level pte's
 * (it starts out as a L1 mapping), and install it along with a set of kernel
 * mapping tables as the kernel's initial page table setup. Also create and
 * enable a context table. I suppose we also want to block user-mode access
 * to the new kernel/ROM mappings.
 */

/*
 * mmu_reservemon4m(): Copies the existing (ROM) page tables to kernel space,
 * converting any L1/L2 PTEs to L3 PTEs. Does *not* copy the L1 entry mapping
 * the kernel at KERNBASE since we don't want to map 16M of physical
 * memory for the kernel. Thus the kernel must be installed later!
 * Also installs ROM mappings into the kernel pmap.
 * NOTE: This also revokes all user-mode access to the mapped regions.
 */
void
mmu_reservemon4m(kpmap)
	struct pmap *kpmap;
{
	unsigned int rom_ctxtbl;
	int te;

	prom_vstart = OPENPROM_STARTVADDR;
	prom_vend = OPENPROM_ENDVADDR;

	/*
	 * XXX: although the Sun4M can handle 36 bits of physical
	 * address space, we assume that all these page tables, etc
	 * are in the lower 4G (32-bits) of address space, i.e. out of I/O
	 * space. Eventually this should be changed to support the 36 bit
	 * physical addressing, in case some crazed ROM designer decides to
	 * stick the pagetables up there. In that case, we should use MMU
	 * transparent mode, (i.e. ASI 0x20 to 0x2f) to access
	 * physical memory.
	 */

	rom_ctxtbl = (lda(SRMMU_CXTPTR,ASI_SRMMU) << SRMMU_PPNPASHIFT);

	te = srmmu_bypass_read(rom_ctxtbl);	/* i.e. context 0 */

	switch (te & SRMMU_TETYPE) {
	case SRMMU_TEINVALID:
		cpuinfo.ctx_tbl[0] = SRMMU_TEINVALID;
		panic("mmu_reservemon4m: no existing L0 mapping! "
		      "(How are we running?");
		break;
	case SRMMU_TEPTE:
#ifdef DEBUG
		printf("mmu_reservemon4m: trying to remap 4G segment!\n");
#endif
		panic("mmu_reservemon4m: can't handle ROM 4G page size");
		/* XXX: Should make this work, however stupid it is */
		break;
	case SRMMU_TEPTD:
		mmu_setup4m_L1(te, kpmap);
		break;
	default:
		panic("mmu_reservemon4m: unknown pagetable entry type");
	}
}

void
mmu_setup4m_L1(regtblptd, kpmap)
	int regtblptd;		/* PTD for region table to be remapped */
	struct pmap *kpmap;
{
	unsigned int regtblrover;
	int i;
	unsigned int te;
	struct regmap *rp;
	int j, k;

	/*
	 * Here we scan the region table to copy any entries which appear.
	 * We are only concerned with regions in kernel space and above
	 * (i.e. regions VA_VREG(KERNBASE)+1 to 0xff). We ignore the first
	 * region (at VA_VREG(KERNBASE)), since that is the 16MB L1 mapping
	 * that the ROM used to map the kernel in initially. Later, we will
	 * rebuild a new L3 mapping for the kernel and install it before
	 * switching to the new pagetables.
	 */
	regtblrover =
		((regtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT) +
		(VA_VREG(KERNBASE)+1) * sizeof(long);	/* kernel only */

	for (i = VA_VREG(KERNBASE) + 1; i < SRMMU_L1SIZE;
	     i++, regtblrover += sizeof(long)) {

		/* The region we're dealing with */
		rp = &kpmap->pm_regmap[i];

		te = srmmu_bypass_read(regtblrover);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;

		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_setup4m_L1: "
			       "converting region 0x%x from L1->L3\n", i);
#endif
			/*
			 * This region entry covers 64MB of memory -- or
			 * (NSEGRG * NPTESG) pages -- which we must convert
			 * into a 3-level description.
			 */

			for (j = 0; j < SRMMU_L2SIZE; j++) {
				struct segmap *sp = &rp->rg_segmap[j];

				for (k = 0; k < SRMMU_L3SIZE; k++) {
					sp->sg_npte++;
					setpgt4m(&sp->sg_pte[k],
						(te & SRMMU_L1PPNMASK) |
						(j << SRMMU_L2PPNSHFT) |
						(k << SRMMU_L3PPNSHFT) |
						(te & SRMMU_PGBITSMSK) |
						((te & SRMMU_PROT_MASK) |
						 PPROT_U2S_OMASK) |
						SRMMU_TEPTE);
				}
			}
			break;

		case SRMMU_TEPTD:
			mmu_setup4m_L2(te, rp);
			break;

		default:
			panic("mmu_setup4m_L1: unknown pagetable entry type");
		}
	}
}

void
mmu_setup4m_L2(segtblptd, rp)
	int segtblptd;
	struct regmap *rp;
{
	unsigned int segtblrover;
	int i, k;
	unsigned int te;
	struct segmap *sp;

	segtblrover = (segtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L2SIZE; i++, segtblrover += sizeof(long)) {

		sp = &rp->rg_segmap[i];

		te = srmmu_bypass_read(segtblrover);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;

		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_setup4m_L2: converting L2 entry at segment 0x%x to L3\n",i);
#endif
			/*
			 * This segment entry covers 256KB of memory -- or
			 * (NPTESG) pages -- which we must convert
			 * into a 3-level description.
			 */
			for (k = 0; k < SRMMU_L3SIZE; k++) {
				sp->sg_npte++;
				setpgt4m(&sp->sg_pte[k],
					(te & SRMMU_L1PPNMASK) |
					(te & SRMMU_L2PPNMASK) |
					(k << SRMMU_L3PPNSHFT) |
					(te & SRMMU_PGBITSMSK) |
					((te & SRMMU_PROT_MASK) |
					 PPROT_U2S_OMASK) |
					SRMMU_TEPTE);
			}
			break;

		case SRMMU_TEPTD:
			mmu_setup4m_L3(te, sp);
			break;

		default:
			panic("mmu_setup4m_L2: unknown pagetable entry type");
		}
	}
}

void
mmu_setup4m_L3(pagtblptd, sp)
	int pagtblptd;
	struct segmap *sp;
{
	unsigned int pagtblrover;
	int i;
	unsigned int te;

	pagtblrover = (pagtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L3SIZE; i++, pagtblrover += sizeof(long)) {
		te = srmmu_bypass_read(pagtblrover);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;
		case SRMMU_TEPTE:
			sp->sg_npte++;
			setpgt4m(&sp->sg_pte[i], te | PPROT_U2S_OMASK);
			break;
		case SRMMU_TEPTD:
			panic("mmu_setup4m_L3: PTD found in L3 page table");
		default:
			panic("mmu_setup4m_L3: unknown pagetable entry type");
		}
	}
}
#endif /* defined SUN4M */

/*----------------------------------------------------------------*/

/*
 * MMU management.
 */
struct mmuentry *me_alloc __P((struct mmuhd *, struct pmap *, int, int));
void		me_free __P((struct pmap *, u_int));
struct mmuentry	*region_alloc __P((struct mmuhd *, struct pmap *, int));
void		region_free __P((struct pmap *, u_int));

/*
 * Change contexts.  We need the old context number as well as the new
 * one.  If the context is changing, we must write all user windows
 * first, lest an interrupt cause them to be written to the (other)
 * user whose context we set here.
 */
#define	CHANGE_CONTEXTS(old, new) \
	if ((old) != (new)) { \
		write_user_windows(); \
		setcontext(new); \
	}

#if defined(SUN4) || defined(SUN4C) /* This is old sun MMU stuff */
/*
 * Allocate an MMU entry (i.e., a PMEG).
 * If necessary, steal one from someone else.
 * Put it on the tail of the given queue
 * (which is either the LRU list or the locked list).
 * The locked list is not actually ordered, but this is easiest.
 * Also put it on the given (new) pmap's chain,
 * enter its pmeg number into that pmap's segmap,
 * and store the pmeg's new virtual segment number (me->me_vseg).
 *
 * This routine is large and complicated, but it must be fast
 * since it implements the dynamic allocation of MMU entries.
 */
struct mmuentry *
me_alloc(mh, newpm, newvreg, newvseg)
	struct mmuhd *mh;
	struct pmap *newpm;
	int newvreg, newvseg;
{
	struct mmuentry *me;
	struct pmap *pm;
	int i, va, pa, *pte, tpte;
	int ctx;
	struct regmap *rp;
	struct segmap *sp;

	/* try free list first */
	if ((me = segm_freelist.tqh_first) != NULL) {
		TAILQ_REMOVE(&segm_freelist, me, me_list);
#ifdef DEBUG
		if (me->me_pmap != NULL)
			panic("me_alloc: freelist entry has pmap");
		if (pmapdebug & PDB_MMU_ALLOC)
			printf("me_alloc: got pmeg %d\n", me->me_cookie);
#endif
		TAILQ_INSERT_TAIL(mh, me, me_list);

		/* onto on pmap chain; pmap is already locked, if needed */
		TAILQ_INSERT_TAIL(&newpm->pm_seglist, me, me_pmchain);
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_free--;
		if (mh == &segm_locked)
			pmap_stats.ps_npmeg_locked++;
		else
			pmap_stats.ps_npmeg_lru++;
#endif

		/* into pmap segment table, with backpointers */
		newpm->pm_regmap[newvreg].rg_segmap[newvseg].sg_pmeg = me->me_cookie;
		me->me_pmap = newpm;
		me->me_vseg = newvseg;
		me->me_vreg = newvreg;

		return (me);
	}

	/* no luck, take head of LRU list */
	if ((me = segm_lru.tqh_first) == NULL)
		panic("me_alloc: all pmegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("me_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("me_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMU_ALLOC | PDB_MMU_STEAL))
		printf("me_alloc: stealing pmeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&segm_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

#ifdef DIAGNOSTIC
	if (mh == &segm_locked) {
		pmap_stats.ps_npmeg_lru--;
		pmap_stats.ps_npmeg_locked++;
	}
#endif

	rp = &pm->pm_regmap[me->me_vreg];
	if (rp->rg_segmap == NULL)
		panic("me_alloc: LRU entry's pmap has no segments");
	sp = &rp->rg_segmap[me->me_vseg];
	pte = sp->sg_pte;
	if (pte == NULL)
		panic("me_alloc: LRU entry's pmap has no ptes");

	/*
	 * The PMEG must be mapped into some context so that we can
	 * read its PTEs.  Use its current context if it has one;
	 * if not, and since context 0 is reserved for the kernel,
	 * the simplest method is to switch to 0 and map the PMEG
	 * to virtual address 0---which, being a user space address,
	 * is by definition not in use.
	 *
	 * XXX for ncpus>1 must use per-cpu VA?
	 * XXX do not have to flush cache immediately
	 */
	ctx = getcontext4();
	if (CTX_USABLE(pm,rp)) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		cache_flush_segment(me->me_vreg, me->me_vseg);
		va = VSTOVA(me->me_vreg,me->me_vseg);
	} else {
		CHANGE_CONTEXTS(ctx, 0);
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
		setsegmap(0, me->me_cookie);
		/*
		 * No cache flush needed: it happened earlier when
		 * the old context was taken.
		 */
		va = 0;
	}

	/*
	 * Record reference and modify bits for each page,
	 * and copy PTEs into kernel memory so that they can
	 * be reloaded later.
	 */
	i = NPTESG;
	do {
		tpte = getpte4(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(tpte & PG_PFNUM);
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR4_4C(tpte);
		}
		*pte++ = tpte & ~(PG_U|PG_M);
		va += NBPG;
	} while (--i > 0);

	/* update segment tables */
	simple_lock(&pm->pm_lock); /* what if other cpu takes mmuentry ?? */
	if (CTX_USABLE(pm,rp))
		setsegmap(VSTOVA(me->me_vreg,me->me_vseg), seginval);
	sp->sg_pmeg = seginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_seglist, me, me_pmchain);
	simple_unlock(&pm->pm_lock);
	setcontext4(ctx);	/* done with old context */

	/* onto new pmap chain; new pmap is already locked, if needed */
	TAILQ_INSERT_TAIL(&newpm->pm_seglist, me, me_pmchain);

	/* into new segment table, with backpointers */
	newpm->pm_regmap[newvreg].rg_segmap[newvseg].sg_pmeg = me->me_cookie;
	me->me_pmap = newpm;
	me->me_vseg = newvseg;
	me->me_vreg = newvreg;

	return (me);
}

/*
 * Free an MMU entry.
 *
 * Assumes the corresponding pmap is already locked.
 * Does NOT flush cache, but does record ref and mod bits.
 * The rest of each PTE is discarded.
 * CALLER MUST SET CONTEXT to pm->pm_ctxnum (if pmap has
 * a context) or to 0 (if not).  Caller must also update
 * pm->pm_segmap and (possibly) the hardware.
 */
void
me_free(pm, pmeg)
	struct pmap *pm;
	u_int pmeg;
{
	struct mmuentry *me = &mmusegments[pmeg];
	int i, va, pa, tpte;
	int vr;
	struct regmap *rp;

	vr = me->me_vreg;

#ifdef DEBUG
	if (pmapdebug & PDB_MMU_ALLOC)
		printf("me_free: freeing pmeg %d from pmap %p\n",
		    me->me_cookie, pm);
	if (me->me_cookie != pmeg)
		panic("me_free: wrong mmuentry");
	if (pm != me->me_pmap)
		panic("me_free: pm != me_pmap");
#endif

	rp = &pm->pm_regmap[vr];

	/* just like me_alloc, but no cache flush, and context already set */
	if (CTX_USABLE(pm,rp)) {
		va = VSTOVA(vr,me->me_vseg);
	} else {
#ifdef DEBUG
if (getcontext4() != 0) panic("me_free: ctx != 0");
#endif
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
		setsegmap(0, me->me_cookie);
		va = 0;
	}
	i = NPTESG;
	do {
		tpte = getpte4(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(tpte & PG_PFNUM);
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR4_4C(tpte);
		}
		va += NBPG;
	} while (--i > 0);

	/* take mmu entry off pmap chain */
	TAILQ_REMOVE(&pm->pm_seglist, me, me_pmchain);
	/* ... and remove from segment map */
	if (rp->rg_segmap == NULL)
		panic("me_free: no segments in pmap");
	rp->rg_segmap[me->me_vseg].sg_pmeg = seginval;

	/* off LRU or lock chain */
	if (pm == pmap_kernel()) {
		TAILQ_REMOVE(&segm_locked, me, me_list);
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_locked--;
#endif
	} else {
		TAILQ_REMOVE(&segm_lru, me, me_list);
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_lru--;
#endif
	}

	/* no associated pmap; on free list */
	me->me_pmap = NULL;
	TAILQ_INSERT_TAIL(&segm_freelist, me, me_list);
#ifdef DIAGNOSTIC
	pmap_stats.ps_npmeg_free++;
#endif
}

#if defined(SUN4_MMU3L)

/* XXX - Merge with segm_alloc/segm_free ? */

struct mmuentry *
region_alloc(mh, newpm, newvr)
	struct mmuhd *mh;
	struct pmap *newpm;
	int newvr;
{
	struct mmuentry *me;
	struct pmap *pm;
	int ctx;
	struct regmap *rp;

	/* try free list first */
	if ((me = region_freelist.tqh_first) != NULL) {
		TAILQ_REMOVE(&region_freelist, me, me_list);
#ifdef DEBUG
		if (me->me_pmap != NULL)
			panic("region_alloc: freelist entry has pmap");
		if (pmapdebug & PDB_MMUREG_ALLOC)
			printf("region_alloc: got smeg 0x%x\n", me->me_cookie);
#endif
		TAILQ_INSERT_TAIL(mh, me, me_list);

		/* onto on pmap chain; pmap is already locked, if needed */
		TAILQ_INSERT_TAIL(&newpm->pm_reglist, me, me_pmchain);

		/* into pmap segment table, with backpointers */
		newpm->pm_regmap[newvr].rg_smeg = me->me_cookie;
		me->me_pmap = newpm;
		me->me_vreg = newvr;

		return (me);
	}

	/* no luck, take head of LRU list */
	if ((me = region_lru.tqh_first) == NULL)
		panic("region_alloc: all smegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("region_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("region_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMUREG_ALLOC | PDB_MMUREG_STEAL))
		printf("region_alloc: stealing smeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&region_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

	rp = &pm->pm_regmap[me->me_vreg];
	ctx = getcontext4();
	if (pm->pm_ctx) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		cache_flush_region(me->me_vreg);
	}

	/* update region tables */
	simple_lock(&pm->pm_lock); /* what if other cpu takes mmuentry ?? */
	if (pm->pm_ctx)
		setregmap(VRTOVA(me->me_vreg), reginval);
	rp->rg_smeg = reginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_reglist, me, me_pmchain);
	simple_unlock(&pm->pm_lock);
	setcontext4(ctx);	/* done with old context */

	/* onto new pmap chain; new pmap is already locked, if needed */
	TAILQ_INSERT_TAIL(&newpm->pm_reglist, me, me_pmchain);

	/* into new segment table, with backpointers */
	newpm->pm_regmap[newvr].rg_smeg = me->me_cookie;
	me->me_pmap = newpm;
	me->me_vreg = newvr;

	return (me);
}

/*
 * Free an MMU entry.
 *
 * Assumes the corresponding pmap is already locked.
 * Does NOT flush cache. ???
 * CALLER MUST SET CONTEXT to pm->pm_ctxnum (if pmap has
 * a context) or to 0 (if not).  Caller must also update
 * pm->pm_regmap and (possibly) the hardware.
 */
void
region_free(pm, smeg)
	struct pmap *pm;
	u_int smeg;
{
	struct mmuentry *me = &mmuregions[smeg];

#ifdef DEBUG
	if (pmapdebug & PDB_MMUREG_ALLOC)
		printf("region_free: freeing smeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
	if (me->me_cookie != smeg)
		panic("region_free: wrong mmuentry");
	if (pm != me->me_pmap)
		panic("region_free: pm != me_pmap");
#endif

	if (pm->pm_ctx)
		cache_flush_region(me->me_vreg);

	/* take mmu entry off pmap chain */
	TAILQ_REMOVE(&pm->pm_reglist, me, me_pmchain);
	/* ... and remove from segment map */
	pm->pm_regmap[smeg].rg_smeg = reginval;

	/* off LRU or lock chain */
	if (pm == pmap_kernel()) {
		TAILQ_REMOVE(&region_locked, me, me_list);
	} else {
		TAILQ_REMOVE(&region_lru, me, me_list);
	}

	/* no associated pmap; on free list */
	me->me_pmap = NULL;
	TAILQ_INSERT_TAIL(&region_freelist, me, me_list);
}
#endif

/*
 * `Page in' (load or inspect) an MMU entry; called on page faults.
 * Returns 1 if we reloaded the segment, -1 if the segment was
 * already loaded and the page was marked valid (in which case the
 * fault must be a bus error or something), or 0 (segment loaded but
 * PTE not valid, or segment not loaded at all).
 */
int
mmu_pagein(pm, va, prot)
	struct pmap *pm;
	vaddr_t va;
	int prot;
{
	int *pte;
	int vr, vs, pmeg, i, s, bits;
	struct regmap *rp;
	struct segmap *sp;

	if (va >= (unsigned long)KERNBASE)
		return (0);

	if (prot != VM_PROT_NONE)
		bits = PG_V | ((prot & VM_PROT_WRITE) ? PG_W : 0);
	else
		bits = 0;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	rp = &pm->pm_regmap[vr];
#ifdef DEBUG
if (pm == pmap_kernel())
printf("mmu_pagein: kernel wants map at va 0x%x, vr %d, vs %d\n", va, vr, vs);
#endif
#if 0
#if defined(SUN4_MMU3L)
printf("mmu_pagein: pm=%p, va 0x%x, vr %d, vs %d, rp=%p, segmap=%p\n", pm, va, vr, vs, rp, rp->rg_segmap);
#endif
#endif

	/* return 0 if we have no PMEGs to load */
	if (rp->rg_segmap == NULL)
		return (0);

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L && rp->rg_smeg == reginval) {
		smeg_t smeg;
		unsigned int tva = VA_ROUNDDOWNTOREG(va);
		struct segmap *sp = rp->rg_segmap;

		s = splpmap();		/* paranoid */
		smeg = region_alloc(&region_lru, pm, vr)->me_cookie;
		setregmap(tva, smeg);
		i = NSEGRG;
		do {
			setsegmap(tva, sp++->sg_pmeg);
			tva += NBPSG;
		} while (--i > 0);
		splx(s);
	}
#endif
	sp = &rp->rg_segmap[vs];

	/* return 0 if we have no PTEs to load */
	if ((pte = sp->sg_pte) == NULL)
		return (0);

	/* return -1 if the fault is `hard', 0 if not */
	if (sp->sg_pmeg != seginval)
		return (bits && (getpte4(va) & bits) == bits ? -1 : 0);

	/* reload segment: write PTEs into a new LRU entry */
	va = VA_ROUNDDOWNTOSEG(va);
	s = splpmap();		/* paranoid */
	pmeg = me_alloc(&segm_lru, pm, vr, vs)->me_cookie;
	setsegmap(va, pmeg);
	i = NPTESG;
	do {
		setpte4(va, *pte++);
		va += NBPG;
	} while (--i > 0);
	splx(s);
	return (1);
}
#endif /* defined SUN4 or SUN4C */

/*
 * Allocate a context.  If necessary, steal one from someone else.
 * Changes hardware context number and loads segment map.
 *
 * This routine is only ever called from locore.s just after it has
 * saved away the previous process, so there are no active user windows.
 */
void
ctx_alloc(pm)
	struct pmap *pm;
{
	union ctxinfo *c;
	int s, cnum, i, doflush;
	struct regmap *rp;
	int gap_start, gap_end;
	unsigned long va;

/*XXX-GCC!*/gap_start=gap_end=0;
#ifdef DEBUG
	if (pm->pm_ctx)
		panic("ctx_alloc pm_ctx");
	if (pmapdebug & PDB_CTX_ALLOC)
		printf("ctx_alloc(%p)\n", pm);
#endif
	if (CPU_ISSUN4OR4C) {
		gap_start = pm->pm_gap_start;
		gap_end = pm->pm_gap_end;
	}

	s = splpmap();
	if ((c = ctx_freelist) != NULL) {
		ctx_freelist = c->c_nextfree;
		cnum = c - cpuinfo.ctxinfo;
		doflush = 0;
	} else {
		if ((ctx_kick += ctx_kickdir) >= ncontext) {
			ctx_kick = ncontext - 1;
			ctx_kickdir = -1;
		} else if (ctx_kick < 1) {
			ctx_kick = 1;
			ctx_kickdir = 1;
		}
		c = &cpuinfo.ctxinfo[cnum = ctx_kick];
#ifdef DEBUG
		if (c->c_pmap == NULL)
			panic("ctx_alloc cu_pmap");
		if (pmapdebug & (PDB_CTX_ALLOC | PDB_CTX_STEAL))
			printf("ctx_alloc: steal context %d from %p\n",
			    cnum, c->c_pmap);
#endif
		c->c_pmap->pm_ctx = NULL;
		doflush = (CACHEINFO.c_vactype != VAC_NONE);
		if (CPU_ISSUN4OR4C) {
			if (gap_start < c->c_pmap->pm_gap_start)
				gap_start = c->c_pmap->pm_gap_start;
			if (gap_end > c->c_pmap->pm_gap_end)
				gap_end = c->c_pmap->pm_gap_end;
		}
	}

	c->c_pmap = pm;
	pm->pm_ctx = c;
	pm->pm_ctxnum = cnum;

	if (CPU_ISSUN4OR4C) {
		/*
		 * Write pmap's region (3-level MMU) or segment table into
		 * the MMU.
		 *
		 * Only write those entries that actually map something in
		 * this context by maintaining a pair of region numbers in
		 * between which the pmap has no valid mappings.
		 *
		 * If a context was just allocated from the free list, trust
		 * that all its pmeg numbers are `seginval'. We make sure this
		 * is the case initially in pmap_bootstrap(). Otherwise, the
		 * context was freed by calling ctx_free() in pmap_release(),
		 * which in turn is supposedly called only when all mappings
		 * have been removed.
		 *
		 * On the other hand, if the context had to be stolen from
		 * another pmap, we possibly shrink the gap to be the
		 * disjuction of the new and the previous map.
		 */

		setcontext4(cnum);
		splx(s);
		if (doflush)
			cache_flush_context();

		rp = pm->pm_regmap;
		for (va = 0, i = NUREG; --i >= 0; ) {
			if (VA_VREG(va) >= gap_start) {
				va = VRTOVA(gap_end);
				i -= gap_end - gap_start;
				rp += gap_end - gap_start;
				if (i < 0)
					break;
				/* mustn't re-enter this branch */
				gap_start = NUREG;
			}
			if (HASSUN4_MMU3L) {
				setregmap(va, rp++->rg_smeg);
				va += NBPRG;
			} else {
				int j;
				struct segmap *sp = rp->rg_segmap;
				for (j = NSEGRG; --j >= 0; va += NBPSG)
					setsegmap(va,
						  sp?sp++->sg_pmeg:seginval);
				rp++;
			}
		}

	} else if (CPU_ISSUN4M) {

#if defined(SUN4M)
		/*
		 * Reload page and context tables to activate the page tables
		 * for this context.
		 *
		 * The gap stuff isn't really needed in the Sun4m architecture,
		 * since we don't have to worry about excessive mappings (all
		 * mappings exist since the page tables must be complete for
		 * the mmu to be happy).
		 *
		 * If a context was just allocated from the free list, trust
		 * that all of its mmu-edible page tables are zeroed out
		 * (except for those associated with the kernel). We make
		 * sure this is the case initially in pmap_bootstrap() and
		 * pmap_init() (?).
		 * Otherwise, the context was freed by calling ctx_free() in
		 * pmap_release(), which in turn is supposedly called only
		 * when all mappings have been removed.
		 *
		 * XXX: Do we have to flush cache after reloading ctx tbl?
		 */

		/* Do any cache flush needed on context switch */
		(*cpuinfo.pure_vcache_flush)();
#ifdef DEBUG
		if (pm->pm_reg_ptps_pa == 0)
			panic("ctx_alloc: no region table in current pmap");
#endif
		/*setcontext(0); * paranoia? can we modify curr. ctx? */
#if defined(MULTIPROCESSOR)
		for (i = 0; i < ncpu; i++) {
			struct cpu_info *cpi = cpus[i];
			if (cpi == NULL)
				continue;

			setpgt4m(&cpi->ctx_tbl[cnum],
				 (pm->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) |
					SRMMU_TEPTD);
		}
		/* Fixup CPUINFO_VA region table entry for current CPU */
		setpgt4m(&pm->pm_reg_ptps[VA_VREG(CPUINFO_VA)],
			 cpuinfo.cpu_seg_ptd);
#else
		setpgt4m(&cpuinfo.ctx_tbl[cnum],
			(pm->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);
#endif

		setcontext4m(cnum);
		if (doflush)
			cache_flush_context();
		tlb_flush_context(); /* remove any remnant garbage from tlb */
#endif
		splx(s);
	}
}

/*
 * Give away a context.  Flushes cache and sets current context to 0.
 */
void
ctx_free(pm)
	struct pmap *pm;
{
	union ctxinfo *c;
	int newc, oldc;

	if ((c = pm->pm_ctx) == NULL)
		panic("ctx_free");
	pm->pm_ctx = NULL;
	oldc = getcontext();

	if (CACHEINFO.c_vactype != VAC_NONE) {
		/* Do any cache flush needed on context switch */
		(*cpuinfo.pure_vcache_flush)();

		newc = pm->pm_ctxnum;
		CHANGE_CONTEXTS(oldc, newc);
		cache_flush_context();
#if defined(SUN4M)
		if (CPU_ISSUN4M)
			tlb_flush_context();
#endif
		setcontext(0);
	} else {
#if defined(SUN4M)
		if (CPU_ISSUN4M) {
			/* Do any cache flush needed on context switch */
			(*cpuinfo.pure_vcache_flush)();
			newc = pm->pm_ctxnum;
			CHANGE_CONTEXTS(oldc, newc);
			tlb_flush_context();
		}
#endif
		CHANGE_CONTEXTS(oldc, 0);
	}
	c->c_nextfree = ctx_freelist;
	ctx_freelist = c;

#if 0
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/* Map kernel back into unused context */
		newc = pm->pm_ctxnum;
		cpuinfo.ctx_tbl[newc] = cpuinfo.ctx_tbl[0];
		if (newc)
			ctxbusyvector[newc] = 0; /* mark as free */
	}
#endif
#endif
}


/*----------------------------------------------------------------*/

/*
 * pvlist functions.
 */

/*
 * Walk the given pv list, and for each PTE, set or clear some bits
 * (e.g., PG_W or PG_NC).
 *
 * As a special case, this never clears PG_W on `pager' pages.
 * These, being kernel addresses, are always in hardware and have
 * a context.
 *
 * This routine flushes the cache for any page whose PTE changes,
 * as long as the process has a context; this is overly conservative.
 * It also copies ref and mod bits to the pvlist, on the theory that
 * this might save work later.  (XXX should test this theory)
 *
 * In addition, if the cacheable bit (PG_NC) is updated in the PTE
 * the corresponding PV_NC flag is also updated in each pv entry. This
 * is done so kvm_uncache() can use this routine and have the uncached
 * status stick.
 */

#if defined(SUN4) || defined(SUN4C)

void
pv_changepte4_4c(pv0, bis, bic)
	struct pvlist *pv0;
	int bis, bic;
{
	int *pte;
	struct pvlist *pv;
	struct pmap *pm;
	int va, vr, vs;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4();
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
#ifdef DIAGNOSTIC
		if(pm == NULL)
			panic("pv_changepte: pm == NULL");
#endif
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_changepte: no segments");

		sp = &rp->rg_segmap[vs];
		pte = sp->sg_pte;

		if (sp->sg_pmeg == seginval) {
			/* not in hardware: just fix software copy */
			if (pte == NULL)
				panic("pv_changepte: pte == NULL");
			pte += VA_VPG(va);
			*pte = (*pte | bis) & ~bic;
		} else {
			int tpte;

			/* in hardware: fix hardware copy */
			if (CTX_USABLE(pm,rp)) {
				/*
				 * Bizarreness:  we never clear PG_W on
				 * pager pages.
				 */
				if (bic == PG_W &&
				    va >= uvm.pager_sva && va < uvm.pager_eva)
					continue;

				setcontext4(pm->pm_ctxnum);
				/* XXX should flush only when necessary */
				tpte = getpte4(va);
				/*
				 * XXX: always flush cache; conservative, but
				 * needed to invalidate cache tag protection
				 * bits and when disabling caching.
				 */
				cache_flush_page(va);
			} else {
				/* XXX per-cpu va? */
				setcontext4(0);
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, sp->sg_pmeg);
				va = VA_VPG(va) << PGSHIFT;
				tpte = getpte4(va);
			}
			if (tpte & PG_V)
				pv0->pv_flags |= MR4_4C(tpte);
			tpte = (tpte | bis) & ~bic;
			setpte4(va, tpte);
			if (pte != NULL)	/* update software copy */
				pte[VA_VPG(va)] = tpte;

			/* Update PV_NC flag if required */
			if (bis & PG_NC)
				pv->pv_flags |= PV_NC;
			if (bic & PG_NC)
				pv->pv_flags &= ~PV_NC;
		}
	}
	setcontext4(ctx);
	splx(s);
}

/*
 * Sync ref and mod bits in pvlist (turns off same in hardware PTEs).
 * Returns the new flags.
 *
 * This is just like pv_changepte, but we never add or remove bits,
 * hence never need to adjust software copies.
 */
int
pv_syncflags4_4c(pv0)
	struct pvlist *pv0;
{
	struct pvlist *pv;
	struct pmap *pm;
	int tpte, va, vr, vs, pmeg, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext4();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_syncflags: no segments");
		sp = &rp->rg_segmap[vs];

		if ((pmeg = sp->sg_pmeg) == seginval)
			continue;

		if (CTX_USABLE(pm,rp)) {
			setcontext4(pm->pm_ctxnum);
			/* XXX should flush only when necessary */
			tpte = getpte4(va);
			if (tpte & PG_M)
				cache_flush_page(va);
		} else {
			/* XXX per-cpu va? */
			setcontext4(0);
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
		}
		if (tpte & (PG_M|PG_U) && tpte & PG_V) {
			flags |= MR4_4C(tpte);
			tpte &= ~(PG_M|PG_U);
			setpte4(va, tpte);
		}
	}
	pv0->pv_flags = flags;
	setcontext4(ctx);
	splx(s);
	return (flags);
}

/*
 * pv_unlink is a helper function for pmap_remove.
 * It takes a pointer to the pv_table head for some physical address
 * and removes the appropriate (pmap, va) entry.
 *
 * Once the entry is removed, if the pv_table head has the cache
 * inhibit bit set, see if we can turn that off; if so, walk the
 * pvlist and turn off PG_NC in each PTE.  (The pvlist is by
 * definition nonempty, since it must have at least two elements
 * in it to have PV_NC set, and we only remove one here.)
 */
/*static*/ void
pv_unlink4_4c(pv, pm, va)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
{
	struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv->pv_pmap == NULL)
		panic("pv_unlink0");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			/*
			 * Shift next entry into the head.
			 * Make sure to retain the REF, MOD and ANC flags.
			 */
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			pv->pv_flags &= ~PV_NC;
			pv->pv_flags |= npv->pv_flags & PV_NC;
			pool_put(&pv_pool, npv);
		} else {
			/*
			 * No mappings left; we still need to maintain
			 * the REF and MOD flags. since pmap_is_modified()
			 * can still be called for this page.
			 */
			pv->pv_pmap = NULL;
			pv->pv_flags &= ~(PV_NC|PV_ANC);
			return;
		}
	} else {
		struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		pool_put(&pv_pool, npv);
	}
	if (pv->pv_flags & PV_ANC && (pv->pv_flags & PV_NC) == 0) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va) || (npv->pv_flags & PV_NC))
				return;
		pv->pv_flags &= ~PV_ANC;
		pv_changepte4_4c(pv, 0, PG_NC);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns PG_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
/*static*/ int
pv_link4_4c(pv, pm, va, nc)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
	int nc;
{
	struct pvlist *npv;
	int ret;

	ret = nc ? PG_NC : 0;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		pv->pv_flags |= nc ? PV_NC : 0;
		return (ret);
	}
	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	pmap_stats.ps_enter_secondpv++;
	if (pv->pv_flags & (PV_NC|PV_ANC)) {
		/* already uncached, just stay that way */
		ret = PG_NC;
	} else {
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			if (npv->pv_flags & PV_NC) {
				ret = PG_NC;
				break;
			}
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug & PDB_CACHESTUFF)
					printf(
			"pv_link: badalias: pid %d, 0x%lx<=>0x%lx, pa 0x%lx\n",
					curproc ? curproc->p_pid : -1,
					va, npv->pv_va,
					vm_first_phys + (pv-pv_table)*NBPG);
#endif
				/* Mark list head `uncached due to aliases' */
				pv->pv_flags |= PV_ANC;
				pv_changepte4_4c(pv, ret = PG_NC, 0);
				break;
			}
		}
	}
	npv = pool_get(&pv_pool, PR_WAITOK);
	npv->pv_next = pv->pv_next;
	npv->pv_pmap = pm;
	npv->pv_va = va;
	npv->pv_flags = nc ? PV_NC : 0;
	pv->pv_next = npv;
	return (ret);
}

#endif /* sun4, sun4c code */

#if defined(SUN4M)		/* Sun4M versions of above */
/*
 * Walk the given pv list, and for each PTE, set or clear some bits
 * (e.g., PG_W or PG_NC).
 *
 * As a special case, this never clears PG_W on `pager' pages.
 * These, being kernel addresses, are always in hardware and have
 * a context.
 *
 * This routine flushes the cache for any page whose PTE changes,
 * as long as the process has a context; this is overly conservative.
 * It also copies ref and mod bits to the pvlist, on the theory that
 * this might save work later.  (XXX should test this theory)
 *
 * In addition, if the cacheable bit (SRMMU_PG_C) is updated in the PTE
 * the corresponding PV_C4M flag is also updated in each pv entry. This
 * is done so kvm_uncache() can use this routine and have the uncached
 * status stick.
 */
void
pv_changepte4m(pv0, bis, bic)
	struct pvlist *pv0;
	int bis, bic;
{
	struct pvlist *pv;
	struct pmap *pm;
	int va, vr;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4m();
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		int tpte;
		pm = pv->pv_pmap;
#ifdef DIAGNOSTIC
		if (pm == NULL)
			panic("pv_changepte: pm == NULL");
#endif
		va = pv->pv_va;
		vr = VA_VREG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_changepte: no segments");

		sp = &rp->rg_segmap[VA_VSEG(va)];

		if (pm->pm_ctx) {
			/*
			 * Bizarreness:  we never clear PG_W on
			 * pager pages.
			 */
			if ((bic & PPROT_WRITE) &&
			    va >= uvm.pager_sva && va < uvm.pager_eva)
				continue;

			setcontext4m(pm->pm_ctxnum);

			/*
			 * XXX: always flush cache; conservative, but
			 * needed to invalidate cache tag protection
			 * bits and when disabling caching.
			 */
			cache_flush_page(va);

			/* Flush TLB so memory copy is up-to-date */
			tlb_flush_page(va);

		}

		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
			printf("pv_changepte: invalid PTE for 0x%x\n", va);
			continue;
		}

		pv0->pv_flags |= MR4M(tpte);
		tpte = (tpte | bis) & ~bic;
		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], tpte);

		/* Update PV_C4M flag if required */
		if (bis & SRMMU_PG_C)
			pv->pv_flags |= PV_C4M;
		if (bic & SRMMU_PG_C)
			pv->pv_flags &= ~PV_C4M;

	}
	setcontext4m(ctx);
	splx(s);
}

/*
 * Sync ref and mod bits in pvlist. If page has been ref'd or modified,
 * update ref/mod bits in pvlist, and clear the hardware bits.
 *
 * Return the new flags.
 */
int
pv_syncflags4m(pv0)
	struct pvlist *pv0;
{
	struct pvlist *pv;
	struct pmap *pm;
	int tpte, va, vr, vs, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext4m();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_syncflags: no segments");
		sp = &rp->rg_segmap[vs];

		if (sp->sg_pte == NULL)	/* invalid */
			continue;

		/*
		 * We need the PTE from memory as the TLB version will
		 * always have the SRMMU_PG_R bit on.
		 */
		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);
			tlb_flush_page(va);
		}
		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE && /* if valid pte */
		    (tpte & (SRMMU_PG_M|SRMMU_PG_R))) {	  /* and mod/refd */

			flags |= MR4M(tpte);

			if (pm->pm_ctx && (tpte & SRMMU_PG_M)) {
				/* Only do this for write-back caches? */
				cache_flush_page(va);
				/*
				 * VIPT caches might use the TLB when
				 * flushing, so we flush the TLB again.
				 */
				tlb_flush_page(va);
			}

			/* Clear mod/ref bits from PTE and write it back */
			tpte &= ~(SRMMU_PG_M | SRMMU_PG_R);
			setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], tpte);
		}
	}
	pv0->pv_flags = flags;
	setcontext4m(ctx);
	splx(s);
	return (flags);
}

void
pv_unlink4m(pv, pm, va)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
{
	struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv->pv_pmap == NULL)
		panic("pv_unlink0");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			/*
			 * Shift next entry into the head.
			 * Make sure to retain the REF, MOD and ANC flags.
			 */
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			pv->pv_flags &= ~PV_C4M;
			pv->pv_flags |= (npv->pv_flags & PV_C4M);
			pool_put(&pv_pool, npv);
		} else {
			/*
			 * No mappings left; we still need to maintain
			 * the REF and MOD flags. since pmap_is_modified()
			 * can still be called for this page.
			 */
			pv->pv_pmap = NULL;
			pv->pv_flags &= ~(PV_C4M|PV_ANC);
			return;
		}
	} else {
		struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		pool_put(&pv_pool, npv);
	}
	if ((pv->pv_flags & (PV_C4M|PV_ANC)) == (PV_C4M|PV_ANC)) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va) ||
			    (npv->pv_flags & PV_C4M) == 0)
				return;
		pv->pv_flags &= PV_ANC;
		pv_changepte4m(pv, SRMMU_PG_C, 0);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns SRMMU_PG_C if the (new) pvlist says that the address cannot
 * be cached (i.e. its results must be (& ~)'d in.
 */
/*static*/ int
pv_link4m(pv, pm, va, nc)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
	int nc;
{
	struct pvlist *npv;
	int ret;

	ret = nc ? SRMMU_PG_C : 0;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		pv->pv_flags |= nc ? 0 : PV_C4M;
		return (ret);
	}
	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	pmap_stats.ps_enter_secondpv++;
	if ((pv->pv_flags & PV_ANC) != 0 || (pv->pv_flags & PV_C4M) == 0) {
		/* already uncached, just stay that way */
		ret = SRMMU_PG_C;
	} else {
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			if ((npv->pv_flags & PV_C4M) == 0) {
				ret = SRMMU_PG_C;
				break;
			}
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug & PDB_CACHESTUFF)
					printf(
			"pv_link: badalias: pid %d, 0x%lx<=>0x%lx, pa 0x%lx\n",
					curproc ? curproc->p_pid : -1,
					va, npv->pv_va,
					vm_first_phys + (pv-pv_table)*NBPG);
#endif
				/* Mark list head `uncached due to aliases' */
				pv->pv_flags |= PV_ANC;
				pv_changepte4m(pv, 0, ret = SRMMU_PG_C);
				/* cache_flush_page(va); XXX: needed? */
				break;
			}
		}
	}
	npv = pool_get(&pv_pool, PR_WAITOK);
	npv->pv_next = pv->pv_next;
	npv->pv_pmap = pm;
	npv->pv_va = va;
	npv->pv_flags = nc ? 0 : PV_C4M;
	pv->pv_next = npv;
	return (ret);
}
#endif

/*
 * Walk the given list and flush the cache for each (MI) page that is
 * potentially in the cache. Called only if vactype != VAC_NONE.
 */
void
pv_flushcache(pv)
	struct pvlist *pv;
{
	struct pmap *pm;
	int s, ctx;

	write_user_windows();	/* paranoia? */

	s = splpmap();		/* XXX extreme paranoia */
	if ((pm = pv->pv_pmap) != NULL) {
		ctx = getcontext();
		for (;;) {
			if (pm->pm_ctx) {
				setcontext(pm->pm_ctxnum);
				cache_flush_page(pv->pv_va);
#if defined(SUN4M)
				if (CPU_ISSUN4M)
					tlb_flush_page(pv->pv_va);
#endif
			}
			pv = pv->pv_next;
			if (pv == NULL)
				break;
			pm = pv->pv_pmap;
		}
		setcontext(ctx);
	}
	splx(s);
}

vsize_t
pv_table_map(base, mapit)
	paddr_t base;
	int mapit;
{
	int nmem;
	struct memarr *mp;
	vsize_t s;
	vaddr_t sva, va, eva;
	paddr_t pa;

	/* 
	 * Map pv_table[] as a `sparse' array. pv_table_map() is called
	 * twice: the first time `mapit' is 0, and the number of
	 * physical pages needed to map the used pieces of pv_table[]
	 * is computed;  the second time those pages are used to
	 * actually map pv_table[].
	 * In both cases, this function returns the amount of physical
	 * memory needed.
	 */ 

	if (!mapit)
		/* Mark physical pages for pv_table[] */
		pv_physmem = base;

	pa = pv_physmem; /* XXX - always init `pa' to appease gcc */

	s = 0;
	sva = eva = 0;
	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		int len;
		paddr_t addr;

		len = mp->len;
		if ((addr = mp->addr) < base) {
			/*
			 * pv_table[] covers everything above `avail_start'.
			 */
			addr = base;
			len -= base;
		}

		/* Calculate stretch of pv_table */
		len = sizeof(struct pvlist) * btoc(len);
		va = (vaddr_t)&pv_table[btoc(addr - base)];
		sva = trunc_page(va);

		if (sva < eva) {
			/* This chunk overlaps the previous in pv_table[] */
			sva += NBPG;
			if (sva < eva)
				panic("pv_table_map: sva(0x%lx)<eva(0x%lx)",
				      sva, eva);
		}
		eva = roundup(va + len, NBPG);

		/* Add this range to the total */
		s += eva - sva;

		if (mapit) {
			/* Map this piece of pv_table[] */
			for (va = sva; va < eva; va += PAGE_SIZE) {
				pmap_enter(pmap_kernel(), va, pa,
				    VM_PROT_READ|VM_PROT_WRITE, 1,
				    VM_PROT_READ|VM_PROT_WRITE);
				pa += PAGE_SIZE;
			}
			bzero((caddr_t)sva, eva - sva);
		}
	}
	return (s);
}

/*----------------------------------------------------------------*/

/*
 * At last, pmap code.
 */

#if defined(SUN4) && (defined(SUN4C) || defined(SUN4M))
int nptesg;
#endif

#if defined(SUN4M)
static void pmap_bootstrap4m __P((void));
#endif
#if defined(SUN4) || defined(SUN4C)
static void pmap_bootstrap4_4c __P((int, int, int));
#endif

/*
 * Bootstrap the system enough to run with VM enabled.
 *
 * nsegment is the number of mmu segment entries (``PMEGs'');
 * nregion is the number of mmu region entries (``SMEGs'');
 * nctx is the number of contexts.
 */
void
pmap_bootstrap(nctx, nregion, nsegment)
	int nsegment, nctx, nregion;
{

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

#if defined(SUN4) && (defined(SUN4C) || defined(SUN4M))
	/* In this case NPTESG is not a #define */
	nptesg = (NBPSG >> pgshift);
#endif

#if 0
	ncontext = nctx;
#endif

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		pmap_bootstrap4m();
		return;
	}
#endif
#if defined(SUN4) || defined(SUN4C)
	if (CPU_ISSUN4OR4C) {
		pmap_bootstrap4_4c(nctx, nregion, nsegment);
		return;
	}
#endif
}

#if defined(SUN4) || defined(SUN4C)
void
pmap_bootstrap4_4c(nctx, nregion, nsegment)
	int nsegment, nctx, nregion;
{
	union ctxinfo *ci;
	struct mmuentry *mmuseg;
#if defined(SUN4_MMU3L)
	struct mmuentry *mmureg;
#endif
	struct   regmap *rp;
	int i, j;
	int npte, zseg, vr, vs;
	int rcookie, scookie;
	caddr_t p;
	int lastpage;
	vaddr_t va;
	extern char end[];
#ifdef DDB
	extern char *esym;
#endif

	switch (cputyp) {
	case CPU_SUN4C:
		mmu_has_hole = 1;
		break;
	case CPU_SUN4:
		if (cpuinfo.cpu_type != CPUTYP_4_400) {
			mmu_has_hole = 1;
			break;
		}
	}

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

#if defined(SUN4)
	/*
	 * set up the segfixmask to mask off invalid bits
	 */
	segfixmask =  nsegment - 1; /* assume nsegment is a power of 2 */
#ifdef DIAGNOSTIC
	if (((nsegment & segfixmask) | (nsegment & ~segfixmask)) != nsegment) {
		printf("pmap_bootstrap: unsuitable number of segments (%d)\n",
			nsegment);
		callrom();
	}
#endif
#endif

#if defined(SUN4M) /* We're in a dual-arch kernel. Setup 4/4c fn. ptrs */
	pmap_clear_modify_p 	=	pmap_clear_modify4_4c;
	pmap_clear_reference_p 	= 	pmap_clear_reference4_4c;
	pmap_copy_page_p 	=	pmap_copy_page4_4c;
	pmap_enter_p 		=	pmap_enter4_4c;
	pmap_extract_p 		=	pmap_extract4_4c;
	pmap_is_modified_p 	=	pmap_is_modified4_4c;
	pmap_is_referenced_p	=	pmap_is_referenced4_4c;
	pmap_page_protect_p	=	pmap_page_protect4_4c;
	pmap_protect_p		=	pmap_protect4_4c;
	pmap_zero_page_p	=	pmap_zero_page4_4c;
	pmap_changeprot_p	=	pmap_changeprot4_4c;
	pmap_rmk_p		=	pmap_rmk4_4c;
	pmap_rmu_p		=	pmap_rmu4_4c;
#endif /* defined SUN4M */

	/*
	 * Last segment is the `invalid' one (one PMEG of pte's with !pg_v).
	 * It will never be used for anything else.
	 */
	seginval = --nsegment;

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L)
		reginval = --nregion;
#endif

	/*
	 * Intialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	simple_lock_init(&kernel_pmap_store.pm_lock);
	kernel_pmap_store.pm_refcount = 1;
#if defined(SUN4_MMU3L)
	TAILQ_INIT(&kernel_pmap_store.pm_reglist);
#endif
	TAILQ_INIT(&kernel_pmap_store.pm_seglist);

	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG];
	for (i = NKREG; --i >= 0;) {
#if defined(SUN4_MMU3L)
		kernel_regmap_store[i].rg_smeg = reginval;
#endif
		kernel_regmap_store[i].rg_segmap =
			&kernel_segmap_store[i * NSEGRG];
		for (j = NSEGRG; --j >= 0;)
			kernel_segmap_store[i * NSEGRG + j].sg_pmeg = seginval;
	}

	/*
	 * Preserve the monitor ROM's reserved VM region, so that
	 * we can use L1-A or the monitor's debugger.  As a side
	 * effect we map the ROM's reserved VM into all contexts
	 * (otherwise L1-A crashes the machine!).
	 */

	mmu_reservemon4_4c(&nregion, &nsegment);

#if defined(SUN4_MMU3L)
	/* Reserve one region for temporary mappings */
	if (HASSUN4_MMU3L)
		tregion = --nregion;
#endif

	/*
	 * Allocate and clear mmu entries and context structures.
	 */
	p = end;
#ifdef DDB
	if (esym != 0)
		p = esym;
#endif
#if defined(SUN4_MMU3L)
	mmuregions = mmureg = (struct mmuentry *)p;
	p += nregion * sizeof(struct mmuentry);
	bzero(mmuregions, nregion * sizeof(struct mmuentry));
#endif
	mmusegments = mmuseg = (struct mmuentry *)p;
	p += nsegment * sizeof(struct mmuentry);
	bzero(mmusegments, nsegment * sizeof(struct mmuentry));

	pmap_kernel()->pm_ctx = cpuinfo.ctxinfo = ci = (union ctxinfo *)p;
	p += nctx * sizeof *ci;

	/* Initialize MMU resource queues */
#if defined(SUN4_MMU3L)
	TAILQ_INIT(&region_freelist);
	TAILQ_INIT(&region_lru);
	TAILQ_INIT(&region_locked);
#endif
	TAILQ_INIT(&segm_freelist);
	TAILQ_INIT(&segm_lru);
	TAILQ_INIT(&segm_locked);

	/*
	 * Set up the `constants' for the call to vm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free, but they start at a higher
	 * virtual address.  This gives us two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, and one MI page
	 * for /dev/mem, all with no associated physical memory.
	 */
	p = (caddr_t)(((u_int)p + NBPG - 1) & ~PGOFSET);

	/*
	 * Grab physical memory list.
	 */
	get_phys_mem();

	/* Allocate physical memory for pv_table[] */
	p += pv_table_map((paddr_t)p - KERNBASE, 0);
	avail_start = (paddr_t)p - KERNBASE;

	i = (int)p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	vmmap = p, p += NBPG;
	p = reserve_dumppages(p);

	/* Allocate virtual memory for pv_table[]. */
	pv_table = (struct pvlist *)p;
	p += round_page(sizeof(struct pvlist) * atop(avail_end - avail_start));

	virtual_avail = (vaddr_t)p;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	p = (caddr_t)i;			/* retract to first free phys */

	/*
	 * All contexts are free except the kernel's.
	 *
	 * XXX sun4c could use context 0 for users?
	 */
	ci->c_pmap = pmap_kernel();
	ctx_freelist = ci + 1;
	for (i = 1; i < ncontext; i++) {
		ci++;
		ci->c_nextfree = ci + 1;
	}
	ci->c_nextfree = NULL;
	ctx_kick = 0;
	ctx_kickdir = -1;

	/*
	 * Init mmu entries that map the kernel physical addresses.
	 *
	 * All the other MMU entries are free.
	 *
	 * THIS ASSUMES SEGMENT i IS MAPPED BY MMU ENTRY i DURING THE
	 * BOOT PROCESS
	 */

	zseg = ((((u_int)p + NBPSG - 1) & ~SGOFSET) - KERNBASE) >> SGSHIFT;
	lastpage = VA_VPG(p);
	if (lastpage == 0)
		/*
		 * If the page bits in p are 0, we filled the last segment
		 * exactly (now how did that happen?); if not, it is
		 * the last page filled in the last segment.
		 */
		lastpage = NPTESG;

	p = (caddr_t)KERNBASE;		/* first va */
	vs = VA_VSEG(KERNBASE);		/* first virtual segment */
	vr = VA_VREG(KERNBASE);		/* first virtual region */
	rp = &pmap_kernel()->pm_regmap[vr];

	for (rcookie = 0, scookie = 0;;) {

		/*
		 * Distribute each kernel region/segment into all contexts.
		 * This is done through the monitor ROM, rather than
		 * directly here: if we do a setcontext we will fault,
		 * as we are not (yet) mapped in any other context.
		 */

		if ((vs % NSEGRG) == 0) {
			/* Entering a new region */
			if (VA_VREG(p) > vr) {
#ifdef DEBUG
				printf("note: giant kernel!\n");
#endif
				vr++, rp++;
			}
#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L) {
				for (i = 1; i < nctx; i++)
					prom_setcontext(i, p, rcookie);

				TAILQ_INSERT_TAIL(&region_locked,
						  mmureg, me_list);
				TAILQ_INSERT_TAIL(&pmap_kernel()->pm_reglist,
						  mmureg, me_pmchain);
				mmureg->me_cookie = rcookie;
				mmureg->me_pmap = pmap_kernel();
				mmureg->me_vreg = vr;
				rp->rg_smeg = rcookie;
				mmureg++;
				rcookie++;
			}
#endif
		}

#if defined(SUN4_MMU3L)
		if (!HASSUN4_MMU3L)
#endif
			for (i = 1; i < nctx; i++)
				prom_setcontext(i, p, scookie);

		/* set up the mmu entry */
		TAILQ_INSERT_TAIL(&segm_locked, mmuseg, me_list);
		TAILQ_INSERT_TAIL(&pmap_kernel()->pm_seglist, mmuseg, me_pmchain);
		pmap_stats.ps_npmeg_locked++;
		mmuseg->me_cookie = scookie;
		mmuseg->me_pmap = pmap_kernel();
		mmuseg->me_vreg = vr;
		mmuseg->me_vseg = vs % NSEGRG;
		rp->rg_segmap[vs % NSEGRG].sg_pmeg = scookie;
		npte = ++scookie < zseg ? NPTESG : lastpage;
		rp->rg_segmap[vs % NSEGRG].sg_npte = npte;
		rp->rg_nsegmap += 1;
		mmuseg++;
		vs++;
		if (scookie < zseg) {
			p += NBPSG;
			continue;
		}

		/*
		 * Unmap the pages, if any, that are not part of
		 * the final segment.
		 */
		for (p += npte << PGSHIFT; npte < NPTESG; npte++, p += NBPG)
			setpte4(p, 0);

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L) {
			/*
			 * Unmap the segments, if any, that are not part of
			 * the final region.
			 */
			for (i = rp->rg_nsegmap; i < NSEGRG; i++, p += NBPSG)
				setsegmap(p, seginval);

			/*
			 * Unmap any kernel regions that we aren't using.
			 */
			for (i = 0; i < nctx; i++) {
				setcontext4(i);
				for (va = (vaddr_t)p;
				     va < (OPENPROM_STARTVADDR & ~(NBPRG - 1));
				     va += NBPRG)
					setregmap(va, reginval);
			}

		} else
#endif
		{
			/*
			 * Unmap any kernel segments that we aren't using.
			 */
			for (i = 0; i < nctx; i++) {
				setcontext4(i);
				for (va = (vaddr_t)p;
				     va < (OPENPROM_STARTVADDR & ~(NBPSG - 1));
				     va += NBPSG)
					setsegmap(va, seginval);
			}
		}
		break;
	}

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L)
		for (; rcookie < nregion; rcookie++, mmureg++) {
			mmureg->me_cookie = rcookie;
			TAILQ_INSERT_TAIL(&region_freelist, mmureg, me_list);
		}
#endif

	for (; scookie < nsegment; scookie++, mmuseg++) {
		mmuseg->me_cookie = scookie;
		TAILQ_INSERT_TAIL(&segm_freelist, mmuseg, me_list);
		pmap_stats.ps_npmeg_free++;
	}

	/* Erase all spurious user-space segmaps */
	for (i = 1; i < ncontext; i++) {
		setcontext4(i);
		if (HASSUN4_MMU3L)
			for (p = 0, j = NUREG; --j >= 0; p += NBPRG)
				setregmap(p, reginval);
		else
			for (p = 0, vr = 0; vr < NUREG; vr++) {
				if (VA_INHOLE(p)) {
					p = (caddr_t)MMU_HOLE_END;
					vr = VA_VREG(p);
				}
				for (j = NSEGRG; --j >= 0; p += NBPSG)
					setsegmap(p, seginval);
			}
	}
	setcontext4(0);

	/*
	 * write protect & encache kernel text;
	 * set red zone at kernel base; enable cache on message buffer.
	 */
	{
		extern char etext[];
#ifdef KGDB
		int mask = ~PG_NC;	/* XXX chgkprot is busted */
#else
		int mask = ~(PG_W | PG_NC);
#endif

		for (p = (caddr_t)trapbase; p < etext; p += NBPG)
			setpte4(p, getpte4(p) & mask);
	}
	pmap_page_upload();
}
#endif

#if defined(SUN4M)		/* Sun4M version of pmap_bootstrap */
/*
 * Bootstrap the system enough to run with VM enabled on a Sun4M machine.
 *
 * Switches from ROM to kernel page tables, and sets up initial mappings.
 */
static void
pmap_bootstrap4m(void)
{
	int i, j;
	caddr_t p, q;
	union ctxinfo *ci;
	int reg, seg;
	unsigned int ctxtblsize;
	caddr_t pagetables_start, pagetables_end;
	extern char end[];
	extern char etext[];
	extern caddr_t reserve_dumppages(caddr_t);
#ifdef DDB
	extern char *esym;
#endif

#if defined(SUN4) || defined(SUN4C) /* setup 4M fn. ptrs for dual-arch kernel */
	pmap_clear_modify_p 	=	pmap_clear_modify4m;
	pmap_clear_reference_p 	= 	pmap_clear_reference4m;
	pmap_copy_page_p 	=	pmap_copy_page4m;
	pmap_enter_p 		=	pmap_enter4m;
	pmap_extract_p 		=	pmap_extract4m;
	pmap_is_modified_p 	=	pmap_is_modified4m;
	pmap_is_referenced_p	=	pmap_is_referenced4m;
	pmap_page_protect_p	=	pmap_page_protect4m;
	pmap_protect_p		=	pmap_protect4m;
	pmap_zero_page_p	=	pmap_zero_page4m;
	pmap_changeprot_p	=	pmap_changeprot4m;
	pmap_rmk_p		=	pmap_rmk4m;
	pmap_rmu_p		=	pmap_rmu4m;
#endif /* defined Sun4/Sun4c */

	/*
	 * Intialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	simple_lock_init(&kernel_pmap_store.pm_lock);
	kernel_pmap_store.pm_refcount = 1;

	/*
	 * Set up pm_regmap for kernel to point NUREG *below* the beginning
	 * of kernel regmap storage. Since the kernel only uses regions
	 * above NUREG, we save storage space and can index kernel and
	 * user regions in the same way
	 */
	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG];
	kernel_pmap_store.pm_reg_ptps = NULL;
	kernel_pmap_store.pm_reg_ptps_pa = 0;
	bzero(kernel_regmap_store, NKREG * sizeof(struct regmap));
	bzero(kernel_segmap_store, NKREG * NSEGRG * sizeof(struct segmap));
	for (i = NKREG; --i >= 0;) {
		kernel_regmap_store[i].rg_segmap =
			&kernel_segmap_store[i * NSEGRG];
		kernel_regmap_store[i].rg_seg_ptps = NULL;
		for (j = NSEGRG; --j >= 0;)
			kernel_segmap_store[i * NSEGRG + j].sg_pte = NULL;
	}

	p = end;		/* p points to top of kernel mem */
#ifdef DDB
	if (esym != 0)
		p = esym;
#endif


	/* Allocate context administration */
	pmap_kernel()->pm_ctx = cpuinfo.ctxinfo = ci = (union ctxinfo *)p;
	p += ncontext * sizeof *ci;
	bzero((caddr_t)ci, (u_int)p - (u_int)ci);
#if 0
	ctxbusyvector = p;
	p += ncontext;
	bzero(ctxbusyvector, ncontext);
	ctxbusyvector[0] = 1;	/* context 0 is always in use */
#endif


	/*
	 * Set up the `constants' for the call to vm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free.
	 */
	p = (caddr_t)(((u_int)p + NBPG - 1) & ~PGOFSET);

	/*
	 * Grab physical memory list.
	 */
	get_phys_mem();

	/* Allocate physical memory for pv_table[] */
	p += pv_table_map((paddr_t)p - KERNBASE, 0);
	avail_start = (paddr_t)p - KERNBASE;

	/*
	 * Reserve memory for MMU pagetables. Some of these have severe
	 * alignment restrictions. We allocate in a sequence that
	 * minimizes alignment gaps.
	 * The amount of physical memory that becomes unavailable for
	 * general VM use is marked by [unavail_gap_start, unavail_gap_end>.
	 */

	/*
	 * Reserve memory for I/O pagetables. This takes 64k of memory
	 * since we want to have 64M of dvma space (this actually depends
	 * on the definition of IOMMU_DVMA_BASE...we may drop it back to 32M).
	 * The table must be aligned on a (-IOMMU_DVMA_BASE/NBPG) boundary
	 * (i.e. 64K for 64M of dvma space).
	 */
#ifdef DEBUG
	if ((0 - IOMMU_DVMA_BASE) % (16*1024*1024))
	    panic("pmap_bootstrap4m: invalid IOMMU_DVMA_BASE of 0x%x",
		  IOMMU_DVMA_BASE);
#endif

	p = (caddr_t) roundup((u_int)p, (0 - IOMMU_DVMA_BASE) / 1024);
	unavail_gap_start = (paddr_t)p - KERNBASE;

	kernel_iopte_table = (u_int *)p;
	kernel_iopte_table_pa = VA2PA((caddr_t)kernel_iopte_table);
	p += (0 - IOMMU_DVMA_BASE) / 1024;

	pagetables_start = p;
	/*
	 * Allocate context table.
	 * To keep supersparc happy, minimum aligment is on a 4K boundary.
	 */
	ctxtblsize = max(ncontext,1024) * sizeof(int);
	cpuinfo.ctx_tbl = (int *)roundup((u_int)p, ctxtblsize);
	cpuinfo.ctx_tbl_pa = (paddr_t)cpuinfo.ctx_tbl - KERNBASE;
	p = (caddr_t)((u_int)cpuinfo.ctx_tbl + ctxtblsize);

	/*
	 * Reserve memory for segment and page tables needed to map the entire
	 * kernel. This takes (2k + NKREG * 16k) of space, but
	 * unfortunately is necessary since pmap_enk *must* be able to enter
	 * a kernel mapping without resorting to malloc, or else the
	 * possibility of deadlock arises (pmap_enk4m is called to enter a
	 * mapping; it needs to malloc a page table; malloc then calls
	 * pmap_enk4m to enter the new malloc'd page; pmap_enk4m needs to
	 * malloc a page table to enter _that_ mapping; malloc deadlocks since
	 * it is already allocating that object).
	 */
	p = (caddr_t) roundup((u_int)p, SRMMU_L1SIZE * sizeof(u_int));
	qzero(p, SRMMU_L1SIZE * sizeof(u_int));
	kernel_regtable_store = (u_int *)p;
	p += SRMMU_L1SIZE * sizeof(u_int);

	p = (caddr_t) roundup((u_int)p, SRMMU_L2SIZE * sizeof(u_int));
	qzero(p, (SRMMU_L2SIZE * sizeof(u_int)) * NKREG);
	kernel_segtable_store = (u_int *)p;
	p += (SRMMU_L2SIZE * sizeof(u_int)) * NKREG;

	p = (caddr_t) roundup((u_int)p, SRMMU_L3SIZE * sizeof(u_int));
	/* zero it: all will be SRMMU_TEINVALID */
	qzero(p, ((SRMMU_L3SIZE * sizeof(u_int)) * NKREG) * NSEGRG);
	kernel_pagtable_store = (u_int *)p;
	p += ((SRMMU_L3SIZE * sizeof(u_int)) * NKREG) * NSEGRG;

	/* Round to next page and mark end of stolen pages */
	p = (caddr_t)(((u_int)p + NBPG - 1) & ~PGOFSET);
	pagetables_end = p;
	unavail_gap_end = (paddr_t)p - KERNBASE;

	/*
	 * Since we've statically allocated space to map the entire kernel,
	 * we might as well pre-wire the mappings to save time in pmap_enter.
	 * This also gets around nasty problems with caching of L1/L2 ptp's.
	 *
	 * XXX WHY DO WE HAVE THIS CACHING PROBLEM WITH L1/L2 PTPS????? %%%
	 */

	pmap_kernel()->pm_reg_ptps = (int *) kernel_regtable_store;
	pmap_kernel()->pm_reg_ptps_pa =
		VA2PA((caddr_t)pmap_kernel()->pm_reg_ptps);

	/* Install L1 table in context 0 */
	setpgt4m(&cpuinfo.ctx_tbl[0],
	    (pmap_kernel()->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

	for (reg = 0; reg < NKREG; reg++) {
		struct regmap *rp;
		caddr_t kphyssegtbl;
		u_int ptd;

		/*
		 * Entering new region; install & build segtbl
		 */

		rp = &pmap_kernel()->pm_regmap[reg + VA_VREG(KERNBASE)];

		kphyssegtbl = (caddr_t)
		    &kernel_segtable_store[reg * SRMMU_L2SIZE];

		ptd = (VA2PA(kphyssegtbl) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;
		setpgt4m(&pmap_kernel()->pm_reg_ptps[reg + VA_VREG(KERNBASE)],
			 ptd);

		rp->rg_seg_ptps = (int *)kphyssegtbl;

		if (reg + VA_VREG(KERNBASE) == VA_VREG(CPUINFO_VA)) {
			/*
			 * Store the segment page table descriptor
			 * corresponding to CPUINFO_VA, so we can install
			 * this CPU-dependent translation into user pmaps
			 * at context switch time.
			 */
			cpuinfo.cpu_seg_ptd = ptd;
		}

		if (rp->rg_segmap == NULL) {
			printf("rp->rg_segmap == NULL!\n");
			rp->rg_segmap = &kernel_segmap_store[reg * NSEGRG];
		}

		for (seg = 0; seg < NSEGRG; seg++) {
			struct segmap *sp;
			caddr_t kphyspagtbl;

			rp->rg_nsegmap++;

			sp = &rp->rg_segmap[seg];
			kphyspagtbl = (caddr_t)
			    &kernel_pagtable_store
				[((reg * NSEGRG) + seg) * SRMMU_L3SIZE];

			setpgt4m(&rp->rg_seg_ptps[seg],
				 (VA2PA(kphyspagtbl) >> SRMMU_PPNPASHIFT) |
				 SRMMU_TEPTD);
			sp->sg_pte = (int *) kphyspagtbl;
		}
	}

	/*
	 * Preserve the monitor ROM's reserved VM region, so that
	 * we can use L1-A or the monitor's debugger.
	 */
	mmu_reservemon4m(&kernel_pmap_store);

	/*
	 * Reserve virtual address space for two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, one MI page
	 * for /dev/mem, and some more for dumpsys().
	 */
	q = p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	vmmap = p, p += NBPG;
	p = reserve_dumppages(p);

	/* Find PTE locations of vpage[] to optimize zero_fill() et.al. */
	for (i = 0; i < 2; i++) {
		struct regmap *rp;
		struct segmap *sp;
		rp = &pmap_kernel()->pm_regmap[VA_VREG(vpage[i])];
		sp = &rp->rg_segmap[VA_VSEG(vpage[i])];
		vpage_pte[i] = &sp->sg_pte[VA_SUN4M_VPG(vpage[i])];
	}

	/* Allocate virtual memory for pv_table[]. */
	pv_table = (struct pvlist *)p;
	p += round_page(sizeof(struct pvlist) * atop(avail_end - avail_start));

	virtual_avail = (vaddr_t)p;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	p = q;			/* retract to first free phys */

	/*
	 * Set up the ctxinfo structures (freelist of contexts)
	 */
	ci->c_pmap = pmap_kernel();
	ctx_freelist = ci + 1;
	for (i = 1; i < ncontext; i++) {
		ci++;
		ci->c_nextfree = ci + 1;
	}
	ci->c_nextfree = NULL;
	ctx_kick = 0;
	ctx_kickdir = -1;

	/*
	 * Now map the kernel into our new set of page tables, then
	 * (finally) switch over to our running page tables.
	 * We map from KERNBASE to p into context 0's page tables (and
	 * the kernel pmap).
	 */
#ifdef DEBUG			/* Sanity checks */
	if ((u_int)p % NBPG != 0)
		panic("pmap_bootstrap4m: p misaligned?!?");
	if (KERNBASE % NBPRG != 0)
		panic("pmap_bootstrap4m: KERNBASE not region-aligned");
#endif

	for (q = (caddr_t) KERNBASE; q < p; q += NBPG) {
		struct regmap *rp;
		struct segmap *sp;
		int pte;

		if ((int)q >= KERNBASE + avail_start &&
		    (int)q < KERNBASE + unavail_gap_start)
			/* This gap is part of VM-managed pages */
			continue;

		/*
		 * Now install entry for current page.
		 */
		rp = &pmap_kernel()->pm_regmap[VA_VREG(q)];
		sp = &rp->rg_segmap[VA_VSEG(q)];
		sp->sg_npte++;

		pte = ((int)q - KERNBASE) >> SRMMU_PPNPASHIFT;
		pte |= PPROT_N_RX | SRMMU_TEPTE;

		/* Deal with the cacheable bit for pagetable memory */
		if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) != 0 ||
		    q < pagetables_start || q >= pagetables_end)
			pte |= SRMMU_PG_C;

		/* write-protect kernel text */
		if (q < (caddr_t) trapbase || q >= etext)
			pte |= PPROT_WRITE;

		setpgt4m(&sp->sg_pte[VA_VPG(q)], pte);
	}

#if 0
	/*
	 * We also install the kernel mapping into all other contexts by
	 * copying the context 0 L1 PTP from cpuinfo.ctx_tbl[0] into the
	 * remainder of the context table (i.e. we share the kernel page-
	 * tables). Each user pmap automatically gets the kernel mapped
	 * into it when it is created, but we do this extra step early on
	 * in case some twit decides to switch to a context with no user
	 * pmap associated with it.
	 */
	for (i = 1; i < ncontext; i++)
		cpuinfo.ctx_tbl[i] = cpuinfo.ctx_tbl[0];
#endif

	if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) == 0)
		/* Flush page tables from cache */
		pcache_flush(pagetables_start, (caddr_t)VA2PA(pagetables_start),
			     pagetables_end - pagetables_start);

	/*
	 * Now switch to kernel pagetables (finally!)
	 */
	mmu_install_tables(&cpuinfo);

	pmap_page_upload();
}

static u_long prom_ctxreg;

void
mmu_install_tables(sc)
	struct cpu_info *sc;
{

#ifdef DEBUG
	printf("pmap_bootstrap: installing kernel page tables...");
#endif
	setcontext4m(0);	/* paranoia? %%%: Make 0x3 a define! below */

	/* Enable MMU tablewalk caching, flush TLB */
	if (sc->mmu_enable != 0)
		sc->mmu_enable();

	tlb_flush_all();
	prom_ctxreg = lda(SRMMU_CXTPTR, ASI_SRMMU);

	sta(SRMMU_CXTPTR, ASI_SRMMU,
		(sc->ctx_tbl_pa >> SRMMU_PPNPASHIFT) & ~0x3);

	tlb_flush_all();

#ifdef DEBUG
	printf("done.\n");
#endif
}

void srmmu_restore_prom_ctx __P((void));

void
srmmu_restore_prom_ctx()
{
	tlb_flush_all();
	sta(SRMMU_CXTPTR, ASI_SRMMU, prom_ctxreg);
	tlb_flush_all();
}

/*
 * Allocate per-CPU page tables. One region, segment and page table
 * is needed to map CPUINFO_VA to different physical addresses on
 * each CPU. Since the kernel region and segment tables are all
 * pre-wired (in bootstrap() above) and we also assume that the
 * first segment (256K) of kernel space is fully populated with
 * pages from the start, these per-CPU tables will never need
 * to be updated when mapping kernel virtual memory.
 *
 * Note: this routine is called in the context of the boot CPU
 * during autoconfig.
 */
void
pmap_alloc_cpu(sc)
	struct cpu_info *sc;
{
	vaddr_t va;
	u_int *ctxtable, *regtable, *segtable, *pagtable;
	int vr, vs, vpg;
	struct regmap *rp;
	struct segmap *sp;
	int ctxsize;
	struct pglist mlist;
	vm_page_t m;
	int cachebit;

	cachebit = (sc->flags & CPUFLG_CACHEPAGETABLES) != 0;

	/*
	 * Allocate properly aligned and contiguous physically memory
	 * for the context table.
	 */
	TAILQ_INIT(&mlist);
	ctxsize = sc->mmu_ncontext * sizeof(int);
	if (uvm_pglistalloc(ctxsize, vm_first_phys, vm_first_phys+vm_num_phys,
			    ctxsize, 0, &mlist, 1, 0) != 0)
		panic("pmap_alloc_cpu: no memory");

	va = uvm_km_valloc(kernel_map, ctxsize);
	if (va == 0)
		panic("pmap_alloc_cpu: no memory");

	ctxtable = (int *)va;

	m = TAILQ_FIRST(&mlist);
	sc->ctx_tbl_pa = VM_PAGE_TO_PHYS(m);

	/* Map the pages */
	for (; m != NULL; m = TAILQ_NEXT(m,pageq)) {
		paddr_t pa = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, pa | (cachebit ? 0 : PMAP_NC),
		    VM_PROT_READ|VM_PROT_WRITE, 1, VM_PROT_READ|VM_PROT_WRITE);
		va += NBPG;
	}

	/*
	 * Get memory for a region, segment and page table.
	 */
	va = uvm_km_alloc(kernel_map, NBPG);
	if (va == 0)
		panic("pmap_alloc_cpu: no memory");
	if (cachebit == 0)
		kvm_uncache((caddr_t)va, 1);

	regtable = (u_int *)va;
	segtable = regtable + SRMMU_L1SIZE;
	pagtable = segtable + SRMMU_L2SIZE;

	/*
	 * Store the segment page table descriptor corresponding
	 * to CPUINFO_VA, so we can install this CPU-dependent
	 * translation into user pmaps at context switch time.
	 * Note that the region table we allocate here (`regtable')
	 * is only used in context 0 on this CPU. Non-zero context
	 * numbers always have a user pmap associated with it.
	 * That pmap's region table is fixed up at context switch
	 * time so that the entry corresponding to CPUINFO_VA points
	 * at the correct CPU's segment table (and hence the per-CPU
	 * page table).
	 */
	sc->cpu_seg_ptd =
		(VA2PA((caddr_t)segtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;

	vr = VA_VREG(CPUINFO_VA);
	vs = VA_VSEG(CPUINFO_VA);
	vpg = VA_VPG(CPUINFO_VA);
	rp = &pmap_kernel()->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	/*
	 * Copy page tables, then modify entry for CPUINFO_VA so that
	 * it points at the per-CPU pages.
	 */

	setpgt4m(&ctxtable[0],
	    (VA2PA((caddr_t)regtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

	qcopy(pmap_kernel()->pm_reg_ptps, regtable, SRMMU_L1SIZE * sizeof(int));
	setpgt4m(&regtable[vr],
		(VA2PA((caddr_t)segtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

	qcopy(rp->rg_seg_ptps, segtable, SRMMU_L2SIZE * sizeof(int));
	setpgt4m(&segtable[vs],
		(VA2PA((caddr_t)pagtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

	qcopy(sp->sg_pte, pagtable, SRMMU_L3SIZE * sizeof(int));
	setpgt4m(&pagtable[vpg],
		(VA2PA((caddr_t)sc) >> SRMMU_PPNPASHIFT) |
		(SRMMU_TEPTE | PPROT_N_RWX | SRMMU_PG_C));

	sc->ctx_tbl = ctxtable;
}
#endif /* SUN4M */


void
pmap_init()
{

	if (PAGE_SIZE != NBPG)
		panic("pmap_init: CLSIZE!=1");

	/* Map pv_table[] */
	(void)pv_table_map(avail_start, 1);

	vm_first_phys = avail_start;
	vm_num_phys = avail_end - avail_start;

	/* Setup a pool for additional pvlist structures */
	pool_init(&pv_pool, sizeof(struct pvlist), 0, 0, 0, "pvtable", 0,
		  NULL, NULL, 0);

	/* Setup a pool for pmap structures. */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
		  0, pool_page_alloc_nointr, pool_page_free_nointr, M_VMPMAP);

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/*
		 * The SRMMU only ever needs chunks in one of two sizes:
		 * 1024 (for region level tables) and 256 (for segment
		 * and page level tables).
		 */
		int n;

		n = SRMMU_L1SIZE * sizeof(int);
		pool_init(&L1_pool, n, n, 0, 0, "L1 pagetable", 0,
			  pgt_page_alloc, pgt_page_free, 0);

		n = SRMMU_L2SIZE * sizeof(int);
		pool_init(&L23_pool, n, n, 0, 0, "L2/L3 pagetable", 0,
			  pgt_page_alloc, pgt_page_free, 0);
	}
#endif
}


/*
 * Map physical addresses into kernel VM.
 */
vaddr_t
pmap_map(va, pa, endpa, prot)
	vaddr_t va;
	paddr_t pa, endpa;
	int prot;
{
	int pgsize = PAGE_SIZE;

	while (pa < endpa) {
		pmap_enter(pmap_kernel(), va, pa, prot, TRUE, 0);
		va += pgsize;
		pa += pgsize;
	}
	return (va);
}

/*
 * Create and return a physical map.
 *
 * If size is nonzero, the map is useless. (ick)
 */
struct pmap *
pmap_create(size)
	vsize_t size;
{
	struct pmap *pm;

	if (size)
		return (NULL);
	pm = pool_get(&pmap_pmap_pool, PR_WAITOK);
#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_create: created %p\n", pm);
#endif
	bzero(pm, sizeof *pm);
	pmap_pinit(pm);
	return (pm);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pm)
	struct pmap *pm;
{
	int size;
	void *urp;

#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_pinit(%p)\n", pm);
#endif

	size = NUREG * sizeof(struct regmap);

	pm->pm_regstore = urp = malloc(size, M_VMPMAP, M_WAITOK);
	qzero((caddr_t)urp, size);
	/* pm->pm_ctx = NULL; */
	simple_lock_init(&pm->pm_lock);
	pm->pm_refcount = 1;
	pm->pm_regmap = urp;

	if (CPU_ISSUN4OR4C) {
		TAILQ_INIT(&pm->pm_seglist);
#if defined(SUN4_MMU3L)
		TAILQ_INIT(&pm->pm_reglist);
		if (HASSUN4_MMU3L) {
			int i;
			for (i = NUREG; --i >= 0;)
				pm->pm_regmap[i].rg_smeg = reginval;
		}
#endif
		pm->pm_gap_end = VA_VREG(VM_MAXUSER_ADDRESS);
	}
#if defined(SUN4M)
	else {
		int i;

		/*
		 * We must allocate and initialize hardware-readable (MMU)
		 * pagetables. We must also map the kernel regions into this
		 * pmap's pagetables, so that we can access the kernel from
		 * this user context.
		 *
		 * Note: pm->pm_regmap's have been zeroed already, so we don't
		 * need to explicitly mark them as invalid (a null
		 * rg_seg_ptps pointer indicates invalid for the 4m)
		 */
		urp = pool_get(&L1_pool, PR_WAITOK);
		pm->pm_reg_ptps = urp;
		pm->pm_reg_ptps_pa = VA2PA(urp);
		for (i = 0; i < NUREG; i++)
			setpgt4m(&pm->pm_reg_ptps[i], SRMMU_TEINVALID);

		/* Copy kernel regions */
		for (i = 0; i < NKREG; i++) {
			int *kpt, *upt;
			kpt = &pmap_kernel()->pm_reg_ptps[VA_VREG(KERNBASE)];
			upt = &pm->pm_reg_ptps[VA_VREG(KERNBASE)];
			setpgt4m(&upt[i], kpt[i]);
		}
	}
#endif

	return;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pm)
	struct pmap *pm;
{
	int count;

	if (pm == NULL)
		return;
#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_destroy(%p)\n", pm);
#endif
	simple_lock(&pm->pm_lock);
	count = --pm->pm_refcount;
	simple_unlock(&pm->pm_lock);
	if (count == 0) {
		pmap_release(pm);
		pool_put(&pmap_pmap_pool, pm);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pm)
	struct pmap *pm;
{
	union ctxinfo *c;
	int s = splpmap();	/* paranoia */

#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_release(%p)\n", pm);
#endif

	if (CPU_ISSUN4OR4C) {
#if defined(SUN4_MMU3L)
		if (pm->pm_reglist.tqh_first)
			panic("pmap_release: region list not empty");
#endif
		if (pm->pm_seglist.tqh_first)
			panic("pmap_release: segment list not empty");

		if ((c = pm->pm_ctx) != NULL) {
			if (pm->pm_ctxnum == 0)
				panic("pmap_release: releasing kernel");
			ctx_free(pm);
		}
	}

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		if ((c = pm->pm_ctx) != NULL) {
			if (pm->pm_ctxnum == 0)
				panic("pmap_release: releasing kernel");
			ctx_free(pm);
		}
		pool_put(&L1_pool, pm->pm_reg_ptps);
		pm->pm_reg_ptps = NULL;
		pm->pm_reg_ptps_pa = 0;
	}
#endif
	splx(s);

#ifdef DEBUG
if (pmapdebug) {
	int vs, vr;
	for (vr = 0; vr < NUREG; vr++) {
		struct regmap *rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap != 0)
			printf("pmap_release: %d segments remain in "
				"region %d\n", rp->rg_nsegmap, vr);
		if (rp->rg_segmap != NULL) {
			printf("pmap_release: segments still "
				"allocated in region %d\n", vr);
			for (vs = 0; vs < NSEGRG; vs++) {
				struct segmap *sp = &rp->rg_segmap[vs];
				if (sp->sg_npte != 0)
					printf("pmap_release: %d ptes "
					     "remain in segment %d\n",
						sp->sg_npte, vs);
				if (sp->sg_pte != NULL) {
					printf("pmap_release: ptes still "
					     "allocated in segment %d\n", vs);
				}
			}
		}
	}
}
#endif

	if (pm->pm_regstore)
		free(pm->pm_regstore, M_VMPMAP);
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pm)
	struct pmap *pm;
{

	if (pm != NULL) {
		simple_lock(&pm->pm_lock);
		pm->pm_refcount++;
		simple_unlock(&pm->pm_lock);
	}
}

/*
 * Remove the given range of mapping entries.
 * The starting and ending addresses are already rounded to pages.
 * Sheer lunacy: pmap_remove is often asked to remove nonexistent
 * mappings.
 */
void
pmap_remove(pm, va, endva)
	struct pmap *pm;
	vaddr_t va, endva;
{
	vaddr_t nva;
	int vr, vs, s, ctx;
	void (*rm)(struct pmap *, vaddr_t, vaddr_t, int, int);

	if (pm == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("pmap_remove(%p, 0x%lx, 0x%lx)\n", pm, va, endva);
#endif

	if (pm == pmap_kernel()) {
		/*
		 * Removing from kernel address space.
		 */
		rm = pmap_rmk;
	} else {
		/*
		 * Removing from user address space.
		 */
		write_user_windows();
		rm = pmap_rmu;
	}

	ctx = getcontext();
	s = splpmap();		/* XXX conservative */
	simple_lock(&pm->pm_lock);
	for (; va < endva; va = nva) {
		/* do one virtual segment at a time */
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		nva = VSTOVA(vr, vs + 1);
		if (nva == 0 || nva > endva)
			nva = endva;
		if (pm->pm_regmap[vr].rg_nsegmap != 0)
			(*rm)(pm, va, nva, vr, vs);
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext(ctx);
}

/*
 * The following magic number was chosen because:
 *	1. It is the same amount of work to cache_flush_page 4 pages
 *	   as to cache_flush_segment 1 segment (so at 4 the cost of
 *	   flush is the same).
 *	2. Flushing extra pages is bad (causes cache not to work).
 *	3. The current code, which malloc()s 5 pages for each process
 *	   for a user vmspace/pmap, almost never touches all 5 of those
 *	   pages.
 */
#if 0
#define	PMAP_RMK_MAGIC	(cacheinfo.c_hwflush?5:64)	/* if > magic, use cache_flush_segment */
#else
#define	PMAP_RMK_MAGIC	5	/* if > magic, use cache_flush_segment */
#endif

/*
 * Remove a range contained within a single segment.
 * These are egregiously complicated routines.
 */

#if defined(SUN4) || defined(SUN4C)

/* remove from kernel */
/*static*/ void
pmap_rmk4_4c(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int i, tpte, perpage, npg;
	struct pvlist *pv;
	int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	if (rp->rg_nsegmap == 0)
		return;

#ifdef DEBUG
	if (rp->rg_segmap == NULL)
		panic("pmap_rmk: no segments");
#endif

	if ((nleft = sp->sg_npte) == 0)
		return;

	pmeg = sp->sg_pmeg;

#ifdef DEBUG
	if (pmeg == seginval)
		panic("pmap_rmk: not loaded");
	if (pm->pm_ctx == NULL)
		panic("pmap_rmk: lost context");
#endif

	setcontext4(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (CACHEINFO.c_vactype != VAC_NONE);
	}
	while (va < endva) {
		tpte = getpte4(va);
		if ((tpte & PG_V) == 0) {
			va += NBPG;
			continue;
		}
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(tpte & PG_PFNUM);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4_4C(tpte);
				pv_unlink4_4c(pv, pm, va);
			}
		}
		nleft--;
#ifdef DIAGNOSTIC
		if (nleft < 0)
			panic("pmap_rmk: too many PTEs in segment; "
			      "va 0x%lx; endva 0x%lx", va, endva);
#endif
		setpte4(va, 0);
		va += NBPG;
	}

	/*
	 * If the segment is all gone, remove it from everyone and
	 * free the MMU entry.
	 */
	if ((sp->sg_npte = nleft) == 0) {
		va = VSTOVA(vr,vs);		/* retract */
#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L)
			setsegmap(va, seginval);
		else
#endif
			for (i = ncontext; --i >= 0;) {
				setcontext4(i);
				setsegmap(va, seginval);
			}
		me_free(pm, pmeg);
		if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L) {
				for (i = ncontext; --i >= 0;) {
					setcontext4(i);
					setregmap(va, reginval);
				}
				/* note: context is 0 */
				region_free(pm, rp->rg_smeg);
			}
#endif
		}
	}
}

#endif /* sun4, sun4c */

#if defined(SUN4M)		/* 4M version of pmap_rmk */
/* remove from kernel (4m)*/
/*static*/ void
pmap_rmk4m(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int i, tpte, perpage, npg;
	struct pvlist *pv;
	int nleft;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	if (rp->rg_nsegmap == 0)
		return;

#ifdef DEBUG
	if (rp->rg_segmap == NULL)
		panic("pmap_rmk: no segments");
#endif

	if ((nleft = sp->sg_npte) == 0)
		return;

#ifdef DEBUG
	if (sp->sg_pte == NULL || rp->rg_seg_ptps == NULL)
		panic("pmap_rmk: segment/region does not exist");
	if (pm->pm_ctx == NULL)
		panic("pmap_rmk: lost context");
#endif

	setcontext4m(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		if (CACHEINFO.c_vactype != VAC_NONE)
			cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (CACHEINFO.c_vactype != VAC_NONE);
	}
	while (va < endva) {
		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
			if ((pmapdebug & PDB_SANITYCHK) &&
			    (getpte4m(va) & SRMMU_TETYPE) == SRMMU_TEPTE)
				panic("pmap_rmk: Spurious kTLB entry for 0x%lx",
				      va);
#endif
			va += NBPG;
			continue;
		}
		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
				cache_flush_page(va);
			i = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		tlb_flush_page(va);
		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
		nleft--;
#ifdef DIAGNOSTIC
		if (nleft < 0)
			panic("pmap_rmk: too many PTEs in segment; "
			      "va 0x%lx; endva 0x%lx", va, endva);
#endif
		va += NBPG;
	}

	/*
	 * If the segment is all gone, remove it from everyone and
	 * flush the TLB.
	 */
	if ((sp->sg_npte = nleft) == 0) {
		va = VSTOVA(vr,vs);		/* retract */

		tlb_flush_segment(vr, vs); 	/* Paranoia? */

		/*
		 * We need to free the segment table. The problem is that
		 * we can't free the initial (bootstrap) mapping, so
		 * we have to explicitly check for this case (ugh).
		 */
		if (va < virtual_avail) {
#ifdef DEBUG
			printf("pmap_rmk4m: attempt to free base kernel alloc\n");
#endif
			/* sp->sg_pte = NULL; */
			sp->sg_npte = 0;
			return;
		}
		/* no need to free the table; it is statically allocated */
		qzero(sp->sg_pte, SRMMU_L3SIZE * sizeof(long));
	}
	/* if we're done with a region, leave it wired */
}
#endif /* SUN4M */
/*
 * Just like pmap_rmk_magic, but we have a different threshold.
 * Note that this may well deserve further tuning work.
 */
#if 0
#define	PMAP_RMU_MAGIC	(cacheinfo.c_hwflush?4:64)	/* if > magic, use cache_flush_segment */
#else
#define	PMAP_RMU_MAGIC	4	/* if > magic, use cache_flush_segment */
#endif

#if defined(SUN4) || defined(SUN4C)

/* remove from user */
/*static*/ void
pmap_rmu4_4c(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int *pte0, i, pteva, tpte, perpage, npg;
	struct pvlist *pv;
	int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0)
		return;
	if (rp->rg_segmap == NULL)
		panic("pmap_rmu: no segments");

	sp = &rp->rg_segmap[vs];
	if ((nleft = sp->sg_npte) == 0)
		return;
	if (sp->sg_pte == NULL)
		panic("pmap_rmu: no pages");


	pmeg = sp->sg_pmeg;
	pte0 = sp->sg_pte;

	if (pmeg == seginval) {
		int *pte = pte0 + VA_VPG(va);

		/*
		 * PTEs are not in MMU.  Just invalidate software copies.
		 */
		for (; va < endva; pte++, va += NBPG) {
			tpte = *pte;
			if ((tpte & PG_V) == 0) {
				/* nothing to remove (braindead VM layer) */
				continue;
			}
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				i = ptoa(tpte & PG_PFNUM);
				if (managed(i))
					pv_unlink4_4c(pvhead(i), pm, va);
			}
			nleft--;
#ifdef DIAGNOSTIC
			if (nleft < 0)
				panic("pmap_rmu: too many PTEs in segment; "
				      "va 0x%lx; endva 0x%lx", va, endva);
#endif
			*pte = 0;
		}
		if ((sp->sg_npte = nleft) == 0) {
			free(pte0, M_VMPMAP);
			sp->sg_pte = NULL;
			if (--rp->rg_nsegmap == 0) {
				free(rp->rg_segmap, M_VMPMAP);
				rp->rg_segmap = NULL;
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
					if (pm->pm_ctx) {
						setcontext4(pm->pm_ctxnum);
						setregmap(va, reginval);
					} else
						setcontext4(0);
					region_free(pm, rp->rg_smeg);
				}
#endif
			}
		}
		return;
	}

	/*
	 * PTEs are in MMU.  Invalidate in hardware, update ref &
	 * mod bits, and flush cache if required.
	 */
	if (CTX_USABLE(pm,rp)) {
		/* process has a context, must flush cache */
		npg = (endva - va) >> PGSHIFT;
		setcontext4(pm->pm_ctxnum);
		if (npg > PMAP_RMU_MAGIC) {
			perpage = 0; /* flush the whole segment */
			cache_flush_segment(vr, vs);
		} else
			perpage = (CACHEINFO.c_vactype != VAC_NONE);
		pteva = va;
	} else {
		/* no context, use context 0; cache flush unnecessary */
		setcontext4(0);
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
		/* XXX use per-cpu pteva? */
		setsegmap(0, pmeg);
		pteva = VA_VPG(va) << PGSHIFT;
		perpage = 0;
	}
	for (; va < endva; pteva += NBPG, va += NBPG) {
		tpte = getpte4(pteva);
		if ((tpte & PG_V) == 0)
			continue;
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(tpte & PG_PFNUM);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4_4C(tpte);
				pv_unlink4_4c(pv, pm, va);
			}
		}
		nleft--;
#ifdef DIAGNOSTIC
		if (nleft < 0)
			panic("pmap_rmu: too many PTEs in segment; "
			     "va 0x%lx; endva 0x%lx; pmeg %d", va, endva, pmeg);
#endif
		setpte4(pteva, 0);
		pte0[VA_VPG(pteva)] = 0;
	}

	/*
	 * If the segment is all gone, and the context is loaded, give
	 * the segment back.
	 */
	if ((sp->sg_npte = nleft) == 0 /* ??? && pm->pm_ctx != NULL*/) {
#ifdef DEBUG
if (pm->pm_ctx == NULL) {
	printf("pmap_rmu: no context here...");
}
#endif
		va = VSTOVA(vr,vs);		/* retract */
		if (CTX_USABLE(pm,rp))
			setsegmap(va, seginval);
		else if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
			/* note: context already set earlier */
			setregmap(0, rp->rg_smeg);
			setsegmap(vs << SGSHIFT, seginval);
		}
		free(pte0, M_VMPMAP);
		sp->sg_pte = NULL;
		me_free(pm, pmeg);

		if (--rp->rg_nsegmap == 0) {
			free(rp->rg_segmap, M_VMPMAP);
			rp->rg_segmap = NULL;
			GAP_WIDEN(pm,vr);

#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
				/* note: context already set */
				if (pm->pm_ctx)
					setregmap(va, reginval);
				region_free(pm, rp->rg_smeg);
			}
#endif
		}

	}
}

#endif /* sun4,4c */

#if defined(SUN4M)		/* 4M version of pmap_rmu */
/* remove from user */
/*static*/ void
pmap_rmu4m(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int *pte0, i, perpage, npg;
	struct pvlist *pv;
	int nleft;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0)
		return;
	if (rp->rg_segmap == NULL)
		panic("pmap_rmu: no segments");

	sp = &rp->rg_segmap[vs];
	if ((nleft = sp->sg_npte) == 0)
		return;

	if (sp->sg_pte == NULL)
		panic("pmap_rmu: no pages");

	pte0 = sp->sg_pte;

	/*
	 * Invalidate PTE in MMU pagetables. Flush cache if necessary.
	 */
	if (pm->pm_ctx) {
		/* process has a context, must flush cache */
		setcontext4m(pm->pm_ctxnum);
		if (CACHEINFO.c_vactype != VAC_NONE) {
			npg = (endva - va) >> PGSHIFT;
			if (npg > PMAP_RMU_MAGIC) {
				perpage = 0; /* flush the whole segment */
				cache_flush_segment(vr, vs);
			} else
				perpage = 1;
		} else
			perpage = 0;
	} else {
		/* no context; cache flush unnecessary */
		perpage = 0;
	}
	for (; va < endva; va += NBPG) {
		int tpte;

		tpte = pte0[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
			if ((pmapdebug & PDB_SANITYCHK) &&
			    pm->pm_ctx &&
			    (getpte4m(va) & SRMMU_TEPTE) == SRMMU_TEPTE)
				panic("pmap_rmu: Spurious uTLB entry for 0x%lx",
				      va);
#endif
			continue;
		}

		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
				cache_flush_page(va);
			i = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		nleft--;
#ifdef DIAGNOSTIC
		if (nleft < 0)
			panic("pmap_rmu: too many PTEs in segment; "
			      "va 0x%lx; endva 0x%lx", va, endva);
#endif
		if (pm->pm_ctx)
			tlb_flush_page(va);

		setpgt4m(&pte0[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
	}

	/*
	 * If the segment is all gone, and the context is loaded, give
	 * the segment back.
	 */
	if ((sp->sg_npte = nleft) == 0) {
#ifdef DEBUG
		if (pm->pm_ctx == NULL) {
			printf("pmap_rmu: no context here...");
		}
#endif
		va = VSTOVA(vr,vs);		/* retract */

		if (pm->pm_ctx)
			tlb_flush_segment(vr, vs); 	/* Paranoia? */
		setpgt4m(&rp->rg_seg_ptps[vs], SRMMU_TEINVALID);
		pool_put(&L23_pool, pte0);
		sp->sg_pte = NULL;

		if (--rp->rg_nsegmap == 0) {
			if (pm->pm_ctx)
				tlb_flush_region(vr); 	/* Paranoia? */
			setpgt4m(&pm->pm_reg_ptps[vr], SRMMU_TEINVALID);
			free(rp->rg_segmap, M_VMPMAP);
			rp->rg_segmap = NULL;
			pool_put(&L23_pool, rp->rg_seg_ptps);
		}
	}
}
#endif /* SUN4M */

/*
 * Lower (make more strict) the protection on the specified
 * physical page.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we do the dirty work here), or it is going from
 * to read-only (in which case pv_changepte does the trick).
 */

#if defined(SUN4) || defined(SUN4C)
void
pmap_page_protect4_4c(pa, prot)
	paddr_t pa;
	vm_prot_t prot;
{
	struct pvlist *pv, *pv0, *npv;
	struct pmap *pm;
	int va, vr, vs, pteva, tpte;
	int flags, nleft, i, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == VM_PROT_NONE))
		printf("pmap_page_protect(0x%lx, 0x%x)\n", pa, prot);
#endif
	/*
	 * Skip unmanaged pages, or operations that do not take
	 * away write permission.
	 */
	if ((pa & (PMAP_TNC_4 & ~PMAP_NC)) ||
	     !managed(pa) || prot & VM_PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & VM_PROT_READ) {
		pv_changepte4_4c(pvhead(pa), 0, PG_W);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	pv = pvhead(pa);
	s = splpmap();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4();
	pv0 = pv;
	flags = pv->pv_flags & ~PV_NC;
	for (;; pm = pv->pv_pmap) {
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			panic("pmap_remove_all: empty vreg");
		sp = &rp->rg_segmap[vs];
		if ((nleft = sp->sg_npte) == 0)
			panic("pmap_remove_all: empty vseg");
		nleft--;
		sp->sg_npte = nleft;

		if (sp->sg_pmeg == seginval) {
			/* Definitely not a kernel map */
			if (nleft) {
				sp->sg_pte[VA_VPG(va)] = 0;
			} else {
				free(sp->sg_pte, M_VMPMAP);
				sp->sg_pte = NULL;
				if (--rp->rg_nsegmap == 0) {
					free(rp->rg_segmap, M_VMPMAP);
					rp->rg_segmap = NULL;
					GAP_WIDEN(pm,vr);
#if defined(SUN4_MMU3L)
					if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
						if (pm->pm_ctx) {
							setcontext4(pm->pm_ctxnum);
							setregmap(va, reginval);
						} else
							setcontext4(0);
						region_free(pm, rp->rg_smeg);
					}
#endif
				}
			}
			goto nextpv;
		}

		if (CTX_USABLE(pm,rp)) {
			setcontext4(pm->pm_ctxnum);
			pteva = va;
			cache_flush_page(va);
		} else {
			setcontext4(0);
			/* XXX use per-cpu pteva? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			pteva = VA_VPG(va) << PGSHIFT;
		}

		tpte = getpte4(pteva);
		if ((tpte & PG_V) == 0)
			panic("pmap_page_protect !PG_V: ctx %d, va 0x%x, pte 0x%x",
			      pm->pm_ctxnum, va, tpte);
		flags |= MR4_4C(tpte);

		if (nleft) {
			setpte4(pteva, 0);
			if (sp->sg_pte != NULL)
				sp->sg_pte[VA_VPG(pteva)] = 0;
			goto nextpv;
		}

		/* Entire segment is gone */
		if (pm == pmap_kernel()) {
#if defined(SUN4_MMU3L)
			if (!HASSUN4_MMU3L)
#endif
				for (i = ncontext; --i >= 0;) {
					setcontext4(i);
					setsegmap(va, seginval);
				}
			me_free(pm, sp->sg_pmeg);
			if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L) {
					for (i = ncontext; --i >= 0;) {
						setcontext4(i);
						setregmap(va, reginval);
					}
					region_free(pm, rp->rg_smeg);
				}
#endif
			}
		} else {
			if (CTX_USABLE(pm,rp))
				/* `pteva'; we might be using tregion */
				setsegmap(pteva, seginval);
#if defined(SUN4_MMU3L)
			else if (HASSUN4_MMU3L &&
				 rp->rg_smeg != reginval) {
				/* note: context already set earlier */
				setregmap(0, rp->rg_smeg);
				setsegmap(vs << SGSHIFT, seginval);
			}
#endif
			free(sp->sg_pte, M_VMPMAP);
			sp->sg_pte = NULL;
			me_free(pm, sp->sg_pmeg);

			if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L &&
				    rp->rg_smeg != reginval) {
					if (pm->pm_ctx)
						setregmap(va, reginval);
					region_free(pm, rp->rg_smeg);
				}
#endif
				free(rp->rg_segmap, M_VMPMAP);
				rp->rg_segmap = NULL;
				GAP_WIDEN(pm,vr);
			}
		}

	nextpv:
		npv = pv->pv_next;
		if (pv != pv0)
			pool_put(&pv_pool, pv);
		if ((pv = npv) == NULL)
			break;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL; /* ? */
	pv0->pv_flags = flags;
	setcontext4(ctx);
	splx(s);
}

/*
 * Lower (make more strict) the protection on the specified
 * range of this pmap.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we call pmap_remove to do the dirty work), or
 * it is going from read/write to read-only.  The latter is
 * fairly easy.
 */
void
pmap_protect4_4c(pm, sva, eva, prot)
	struct pmap *pm;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	int va, nva, vr, vs;
	int s, ctx;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL || prot & VM_PROT_WRITE)
		return;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

	write_user_windows();
	ctx = getcontext4();
	s = splpmap();
	simple_lock(&pm->pm_lock);

	for (va = sva; va < eva;) {
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		nva = VSTOVA(vr,vs + 1);
if (nva == 0) panic("pmap_protect: last segment");	/* cannot happen */
		if (nva > eva)
			nva = eva;
		if (rp->rg_nsegmap == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (rp->rg_segmap == NULL)
			panic("pmap_protect: no segments");
#endif
		sp = &rp->rg_segmap[vs];
		if (sp->sg_npte == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (pm != pmap_kernel() && sp->sg_pte == NULL)
			panic("pmap_protect: no pages");
#endif
		if (sp->sg_pmeg == seginval) {
			int *pte = &sp->sg_pte[VA_VPG(va)];

			/* not in MMU; just clear PG_W from core copies */
			for (; va < nva; va += NBPG)
				*pte++ &= ~PG_W;
		} else {
			/* in MMU: take away write bits from MMU PTEs */
			if (CTX_USABLE(pm,rp)) {
				int tpte;

				/*
				 * Flush cache so that any existing cache
				 * tags are updated.  This is really only
				 * needed for PTEs that lose PG_W.
				 */
				setcontext4(pm->pm_ctxnum);
				for (; va < nva; va += NBPG) {
					tpte = getpte4(va);
					pmap_stats.ps_npg_prot_all++;
					if ((tpte & (PG_W|PG_TYPE)) ==
					    (PG_W|PG_OBMEM)) {
						pmap_stats.ps_npg_prot_actual++;
						cache_flush_page(va);
						setpte4(va, tpte & ~PG_W);
					}
				}
			} else {
				int pteva;

				/*
				 * No context, hence not cached;
				 * just update PTEs.
				 */
				setcontext4(0);
				/* XXX use per-cpu pteva? */
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, sp->sg_pmeg);
				pteva = VA_VPG(va) << PGSHIFT;
				for (; va < nva; pteva += NBPG, va += NBPG)
					setpte4(pteva, getpte4(pteva) & ~PG_W);
			}
		}
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext4(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4_4c(pm, va, prot, wired)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
{
	int vr, vs, tpte, newprot, ctx, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va &= ~(NBPG-1);
	if (pm == pmap_kernel())
		newprot = prot & VM_PROT_WRITE ? PG_S|PG_W : PG_S;
	else
		newprot = prot & VM_PROT_WRITE ? PG_W : 0;
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	s = splpmap();		/* conservative */
	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0) {
		printf("pmap_changeprot: no segments in %d\n", vr);
		return;
	}
	if (rp->rg_segmap == NULL) {
		printf("pmap_changeprot: no segments in %d!\n", vr);
		return;
	}
	sp = &rp->rg_segmap[vs];

	pmap_stats.ps_changeprots++;

#ifdef DEBUG
	if (pm != pmap_kernel() && sp->sg_pte == NULL)
		panic("pmap_changeprot: no pages");
#endif

	/* update PTEs in software or hardware */
	if (sp->sg_pmeg == seginval) {
		int *pte = &sp->sg_pte[VA_VPG(va)];

		/* update in software */
		if ((*pte & PG_PROT) == newprot)
			goto useless;
		*pte = (*pte & ~PG_PROT) | newprot;
	} else {
		/* update in hardware */
		ctx = getcontext4();
		if (CTX_USABLE(pm,rp)) {
			/*
			 * Use current context.
			 * Flush cache if page has been referenced to
			 * avoid stale protection bits in the cache tags.
			 */
			setcontext4(pm->pm_ctxnum);
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext4(ctx);
				goto useless;
			}
			if ((tpte & (PG_U|PG_NC|PG_TYPE)) == (PG_U|PG_OBMEM))
				cache_flush_page((int)va);
		} else {
			setcontext4(0);
			/* XXX use per-cpu va? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext4(ctx);
				goto useless;
			}
		}
		tpte = (tpte & ~PG_PROT) | newprot;
		setpte4(va, tpte);
		setcontext4(ctx);
	}
	splx(s);
	return;

useless:
	/* only wiring changed, and we ignore wiring */
	pmap_stats.ps_useless_changeprots++;
	splx(s);
}

#endif /* sun4, 4c */

#if defined(SUN4M)		/* 4M version of protection routines above */
/*
 * Lower (make more strict) the protection on the specified
 * physical page.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we do the dirty work here), or it is going
 * to read-only (in which case pv_changepte does the trick).
 */
void
pmap_page_protect4m(pa, prot)
	paddr_t pa;
	vm_prot_t prot;
{
	struct pvlist *pv, *pv0, *npv;
	struct pmap *pm;
	int va, vr, vs, tpte;
	int flags, nleft, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == VM_PROT_NONE))
		printf("pmap_page_protect(0x%lx, 0x%x)\n", pa, prot);
#endif
	/*
	 * Skip unmanaged pages, or operations that do not take
	 * away write permission.
	 */
	if (!managed(pa) || prot & VM_PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & VM_PROT_READ) {
		pv_changepte4m(pvhead(pa), 0, PPROT_WRITE);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	pv = pvhead(pa);
	s = splpmap();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4m();
	pv0 = pv;
	flags = pv->pv_flags /*| PV_C4M*/;	/* %%%: ???? */
	for (;; pm = pv->pv_pmap) {
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			panic("pmap_remove_all: empty vreg");
		sp = &rp->rg_segmap[vs];
		if ((nleft = sp->sg_npte) == 0)
			panic("pmap_remove_all: empty vseg");
		nleft--;
		sp->sg_npte = nleft;

		/* Invalidate PTE in MMU pagetables. Flush cache if necessary */
		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);
			cache_flush_page(va);
			tlb_flush_page(va);
		}

		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE)
			panic("pmap_page_protect !PG_V");

		flags |= MR4M(tpte);

		if (nleft) {
			setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
			goto nextpv;
		}

		/* Entire segment is gone */
		if (pm == pmap_kernel()) {
			tlb_flush_segment(vr, vs); /* Paranoid? */
			setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
			if (va < virtual_avail) {
#ifdef DEBUG
				printf(
				 "pmap_page_protect: attempt to free"
				 " base kernel allocation\n");
#endif
				goto nextpv;
			}
#if 0 /* no need for this */
			/* no need to free the table; it is static */
			qzero(sp->sg_pte, SRMMU_L3SIZE * sizeof(int));
#endif

			/* if we're done with a region, leave it */

		} else { 	/* User mode mapping */
			if (pm->pm_ctx)
				tlb_flush_segment(vr, vs);
			setpgt4m(&rp->rg_seg_ptps[vs], SRMMU_TEINVALID);
			pool_put(&L23_pool, sp->sg_pte);
			sp->sg_pte = NULL;

			if (--rp->rg_nsegmap == 0) {
				if (pm->pm_ctx)
					tlb_flush_region(vr);
				setpgt4m(&pm->pm_reg_ptps[vr], SRMMU_TEINVALID);
				free(rp->rg_segmap, M_VMPMAP);
				rp->rg_segmap = NULL;
				pool_put(&L23_pool, rp->rg_seg_ptps);
			}
		}

	nextpv:
		npv = pv->pv_next;
		if (pv != pv0)
			pool_put(&pv_pool, pv);
		if ((pv = npv) == NULL)
			break;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL; /* ? */
	pv0->pv_flags = flags;
	setcontext4m(ctx);
	splx(s);
}

/*
 * Lower (make more strict) the protection on the specified
 * range of this pmap.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we call pmap_remove to do the dirty work), or
 * it is going from read/write to read-only.  The latter is
 * fairly easy.
 */
void
pmap_protect4m(pm, sva, eva, prot)
	struct pmap *pm;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	vaddr_t va, nva;
	int s, ctx, vr, vs;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL || prot & VM_PROT_WRITE)
		return;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_protect[curpid %d, ctx %d](%lx, %lx, %x)\n",
			curproc==NULL ? -1 : curproc->p_pid,
			pm->pm_ctx ? pm->pm_ctxnum : -1, sva, eva, prot);
#endif

	write_user_windows();
	ctx = getcontext4m();
	s = splpmap();
	simple_lock(&pm->pm_lock);

	for (va = sva; va < eva;) {
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		nva = VSTOVA(vr,vs + 1);
		if (nva == 0)	/* XXX */
			panic("pmap_protect: last segment"); /* cannot happen(why?)*/
		if (nva > eva)
			nva = eva;
		if (rp->rg_nsegmap == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (rp->rg_segmap == NULL)
			panic("pmap_protect: no segments");
#endif
		sp = &rp->rg_segmap[vs];
		if (sp->sg_npte == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (sp->sg_pte == NULL)
			panic("pmap_protect: no pages");
#endif
		/*
		 * pages loaded: take away write bits from MMU PTEs
		 */
		if (pm->pm_ctx)
			setcontext4m(pm->pm_ctxnum);

		pmap_stats.ps_npg_prot_all = (nva - va) >> PGSHIFT;
		for (; va < nva; va += NBPG) {
			int tpte;

			tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
			/*
			 * Flush cache so that any existing cache
			 * tags are updated.  This is really only
			 * needed for PTEs that lose PG_W.
			 */
			if ((tpte & (PPROT_WRITE|SRMMU_PGTYPE)) ==
			    (PPROT_WRITE|PG_SUN4M_OBMEM)) {
				pmap_stats.ps_npg_prot_actual++;
				if (pm->pm_ctx) {
					cache_flush_page(va);
					/* Flush TLB entry */
					tlb_flush_page(va);
				}
				setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)],
					 tpte & ~PPROT_WRITE);
			}
		}
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext4m(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4m(pm, va, prot, wired)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
{
	int pte, newprot, ctx, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va &= ~(NBPG-1);
	if (pm == pmap_kernel())
		newprot = prot & VM_PROT_WRITE ? PPROT_N_RWX : PPROT_N_RX;
	else
		newprot = prot & VM_PROT_WRITE ? PPROT_RWX_RWX : PPROT_RX_RX;

	pmap_stats.ps_changeprots++;

	s = splpmap();		/* conservative */

	rp = &pm->pm_regmap[VA_VREG(va)];
	sp = &rp->rg_segmap[VA_VSEG(va)];

	pte = sp->sg_pte[VA_SUN4M_VPG(va)];
	if ((pte & SRMMU_PROT_MASK) == newprot) {
		/* only wiring changed, and we ignore wiring */
		pmap_stats.ps_useless_changeprots++;
		goto out;
	}

	if (pm->pm_ctx) {
		/*
		 * Use current context.
		 * Flush cache if page has been referenced to
		 * avoid stale protection bits in the cache tags.
		 */

		ctx = getcontext4m();
		setcontext4m(pm->pm_ctxnum);
		if ((pte & (SRMMU_PG_C|SRMMU_PGTYPE)) ==
		    (SRMMU_PG_C|PG_SUN4M_OBMEM))
			cache_flush_page(va);

		tlb_flush_page(va);
		setcontext4m(ctx);
	}

	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)],
		 (pte & ~SRMMU_PROT_MASK) | newprot);

out:
	splx(s);
}
#endif /* SUN4M */

/*
 * Insert (MI) physical page pa at virtual address va in the given pmap.
 * NB: the pa parameter includes type bits PMAP_OBIO, PMAP_NC as necessary.
 *
 * If pa is not in the `managed' range it will not be `bank mapped'.
 * This works during bootstrap only because the first 4MB happens to
 * map one-to-one.
 *
 * There may already be something else there, or we might just be
 * changing protections and/or wiring on an existing mapping.
 *	XXX	should have different entry points for changing!
 */

#if defined(SUN4) || defined(SUN4C)

void
pmap_enter4_4c(pm, va, pa, prot, wired, access_type)
	struct pmap *pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int wired;
	vm_prot_t access_type;
{
	struct pvlist *pv;
	int pteproto, ctx;

	if (pm == NULL)
		return;

	if (VA_INHOLE(va)) {
#ifdef DEBUG
		printf("pmap_enter: pm %p, va 0x%lx, pa 0x%lx: in MMU hole\n",
			pm, va, pa);
#endif
		return;
	}

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, pa, prot, wired);
#endif

	pteproto = PG_V | PMAP_T2PTE_4(pa);
	pa &= ~PMAP_TNC_4;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & PG_TYPE) == PG_OBMEM && managed(pa)) {
#ifdef DIAGNOSTIC
		if (!pmap_pa_exists(pa))
			panic("pmap_enter: no such address: 0x%lx", pa);
#endif
		pv = pvhead(pa);
	} else {
		pv = NULL;
	}
	pteproto |= atop(pa) & PG_PFNUM;
	if (prot & VM_PROT_WRITE)
		pteproto |= PG_W;

	ctx = getcontext4();
	if (pm == pmap_kernel())
		pmap_enk4_4c(pm, va, prot, wired, pv, pteproto | PG_S);
	else
		pmap_enu4_4c(pm, va, prot, wired, pv, pteproto);
	setcontext4(ctx);
}

/* enter new (or change existing) kernel mapping */
void
pmap_enk4_4c(pm, va, prot, wired, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, tpte, i, s;
	struct regmap *rp;
	struct segmap *sp;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];
	s = splpmap();		/* XXX way too conservative */

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L && rp->rg_smeg == reginval) {
		vaddr_t tva;
		rp->rg_smeg = region_alloc(&region_locked, pm, vr)->me_cookie;
		i = ncontext - 1;
		do {
			setcontext4(i);
			setregmap(va, rp->rg_smeg);
		} while (--i >= 0);

		/* set all PTEs to invalid, then overwrite one PTE below */
		tva = VA_ROUNDDOWNTOREG(va);
		for (i = 0; i < NSEGRG; i++) {
			setsegmap(tva, rp->rg_segmap[i].sg_pmeg);
			tva += NBPSG;
		};
	}
#endif
	if (sp->sg_pmeg != seginval && (tpte = getpte4(va)) & PG_V) {
		int addr;

		/* old mapping exists, and is of the same pa type */
		if ((tpte & (PG_PFNUM|PG_TYPE)) ==
		    (pteproto & (PG_PFNUM|PG_TYPE))) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot4_4c(pm, va, prot, wired);
			return;
		}

		if ((tpte & PG_TYPE) == PG_OBMEM) {
#ifdef DEBUG
printf("pmap_enk: changing existing va=>pa entry: va 0x%lx, pteproto 0x%x\n",
	va, pteproto);
#endif
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			addr = ptoa(tpte & PG_PFNUM);
			if (managed(addr))
				pv_unlink4_4c(pvhead(addr), pm, va);
			if ((tpte & PG_NC) == 0) {
				setcontext4(0);	/* ??? */
				cache_flush_page((int)va);
			}
		}
	} else {
		/* adding new entry */
		sp->sg_npte++;
	}

	/*
	 * If the new mapping is for a managed PA, enter into pvlist.
	 * Note that the mapping for a malloc page will always be
	 * unique (hence will never cause a second call to malloc).
	 */
	if (pv != NULL)
		pteproto |= pv_link4_4c(pv, pm, va, pteproto & PG_NC);

	if (sp->sg_pmeg == seginval) {
		int tva;

		/*
		 * Allocate an MMU entry now (on locked list),
		 * and map it into every context.  Set all its
		 * PTEs invalid (we will then overwrite one, but
		 * this is more efficient than looping twice).
		 */
#ifdef DEBUG
		if (pm->pm_ctx == NULL || pm->pm_ctxnum != 0)
			panic("pmap_enk: kern seg but no kern ctx");
#endif
		sp->sg_pmeg = me_alloc(&segm_locked, pm, vr, vs)->me_cookie;
		rp->rg_nsegmap++;

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L)
			setsegmap(va, sp->sg_pmeg);
		else
#endif
		{
			i = ncontext - 1;
			do {
				setcontext4(i);
				setsegmap(va, sp->sg_pmeg);
			} while (--i >= 0);
		}

		/* set all PTEs to invalid, then overwrite one PTE below */
		tva = VA_ROUNDDOWNTOSEG(va);
		i = NPTESG;
		do {
			setpte4(tva, 0);
			tva += NBPG;
		} while (--i > 0);
	}

	/* ptes kept in hardware only */
	setpte4(va, pteproto);
	splx(s);
}

/* enter new (or change existing) user mapping */
void
pmap_enu4_4c(pm, va, prot, wired, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, *pte, tpte, pmeg, s, doflush;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splpmap();			/* XXX conservative */

	/*
	 * If there is no space in which the PTEs can be written
	 * while they are not in the hardware, this must be a new
	 * virtual segment.  Get PTE space and count the segment.
	 *
	 * TO SPEED UP CTX ALLOC, PUT SEGMENT BOUNDS STUFF HERE
	 * AND IN pmap_rmu()
	 */

	GAP_SHRINK(pm,vr);

#ifdef DEBUG
	if (pm->pm_gap_end < pm->pm_gap_start) {
		printf("pmap_enu: gap_start 0x%x, gap_end 0x%x",
			pm->pm_gap_start, pm->pm_gap_end);
		panic("pmap_enu: gap botch");
	}
#endif

rretry:
	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		int i;
		int size = NSEGRG * sizeof (struct segmap);

		sp = (struct segmap *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (rp->rg_segmap != NULL) {
printf("pmap_enter: segment filled during sleep\n");	/* can this happen? */
			free(sp, M_VMPMAP);
			goto rretry;
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		for (i = NSEGRG; --i >= 0;)
			sp++->sg_pmeg = seginval;

#if defined(SUN4_MMU3L)
/*
 * XXX - preallocate the region MMU cookies.
 * XXX - Doing this keeps the machine running for a while
 * XXX - Remove or alter this after dealing with the bugs...
 */
		if (HASSUN4_MMU3L) {
			vaddr_t tva;
			rp->rg_smeg = region_alloc(&region_lru, pm, vr)->me_cookie;
			setregmap(va, rp->rg_smeg);

			tva = VA_ROUNDDOWNTOREG(va);
			for (i = 0; i < NSEGRG; i++) {
				setsegmap(tva, seginval);
				tva += NBPSG;
			};
		}
/* XXX  - end of work-around */
#endif
	}

	sp = &rp->rg_segmap[vs];

sretry:
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		int size = NPTESG * sizeof *pte;

		pte = (int *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (sp->sg_pte != NULL) {
printf("pmap_enter: pte filled during sleep\n");	/* can this happen? */
			free(pte, M_VMPMAP);
			goto sretry;
		}
#ifdef DEBUG
		if (sp->sg_pmeg != seginval)
			panic("pmap_enter: new ptes, but not seginval");
#endif
		qzero((caddr_t)pte, size);
		sp->sg_pte = pte;
		sp->sg_npte = 1;
		rp->rg_nsegmap++;
	} else {
		/* might be a change: fetch old pte */
		doflush = 0;
		if ((pmeg = sp->sg_pmeg) == seginval) {
			/* software pte */
			tpte = pte[VA_VPG(va)];
		} else {
			/* hardware pte */
			if (CTX_USABLE(pm,rp)) {
				setcontext4(pm->pm_ctxnum);
				tpte = getpte4(va);
				doflush = CACHEINFO.c_vactype != VAC_NONE;
			} else {
				setcontext4(0);
				/* XXX use per-cpu pteva? */
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, pmeg);
				tpte = getpte4(VA_VPG(va) << PGSHIFT);
			}
		}
		if (tpte & PG_V) {
			int addr;

			/* old mapping exists, and is of the same pa type */
			if ((tpte & (PG_PFNUM|PG_TYPE)) ==
			    (pteproto & (PG_PFNUM|PG_TYPE))) {
				/* just changing prot and/or wiring */
				splx(s);
				/* caller should call this directly: */
				pmap_changeprot4_4c(pm, va, prot, wired);
				if (wired)
					pm->pm_stats.wired_count++;
				else
					pm->pm_stats.wired_count--;
				return;
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
#if 0
printf("%s[%d]: pmap_enu: changing existing va(0x%x)=>pa entry\n",
	curproc->p_comm, curproc->p_pid, va);
#endif
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				addr = ptoa(tpte & PG_PFNUM);
				if (managed(addr))
					pv_unlink4_4c(pvhead(addr), pm, va);
				if (doflush && (tpte & PG_NC) == 0)
					cache_flush_page((int)va);
			}
		} else {
			/* adding new entry */
			sp->sg_npte++;

			/*
			 * Increment counters
			 */
			if (wired)
				pm->pm_stats.wired_count++;
		}
	}

	if (pv != NULL)
		pteproto |= pv_link4_4c(pv, pm, va, pteproto & PG_NC);

	/*
	 * Update hardware & software PTEs.
	 */
	if ((pmeg = sp->sg_pmeg) != seginval) {
		/* ptes are in hardware */
		if (CTX_USABLE(pm,rp))
			setcontext4(pm->pm_ctxnum);
		else {
			setcontext4(0);
			/* XXX use per-cpu pteva? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
		}
		setpte4(va, pteproto);
	}
	/* update software copy */
	pte += VA_VPG(va);
	*pte = pteproto;

	splx(s);
}

#endif /*sun4,4c*/

#if defined(SUN4M)		/* Sun4M versions of enter routines */
/*
 * Insert (MI) physical page pa at virtual address va in the given pmap.
 * NB: the pa parameter includes type bits PMAP_OBIO, PMAP_NC as necessary.
 *
 * If pa is not in the `managed' range it will not be `bank mapped'.
 * This works during bootstrap only because the first 4MB happens to
 * map one-to-one.
 *
 * There may already be something else there, or we might just be
 * changing protections and/or wiring on an existing mapping.
 *	XXX	should have different entry points for changing!
 */

void
pmap_enter4m(pm, va, pa, prot, wired, access_type)
	struct pmap *pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int wired;
	vm_prot_t access_type;
{
	struct pvlist *pv;
	int pteproto, ctx;

	if (pm == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter[curpid %d, ctx %d]"
			"(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
			curproc==NULL ? -1 : curproc->p_pid,
			pm->pm_ctx==NULL ? -1 : pm->pm_ctxnum,
			pm, va, pa, prot, wired);
#endif

	/* Initialise pteproto with cache bit */
	pteproto = (pa & PMAP_NC) == 0 ? SRMMU_PG_C : 0;

#ifdef DEBUG
	if (pa & PMAP_TYPE_SRMMU) {	/* this page goes in an iospace */
		if (cpuinfo.cpu_type == CPUTYP_MS1)
			panic("pmap_enter4m: attempt to use 36-bit iospace on"
			      " MicroSPARC");
	}
#endif
	pteproto |= PMAP_T2PTE_SRMMU(pa);

	/* Make sure we get a pte with appropriate perms! */
	pteproto |= SRMMU_TEPTE | PPROT_RX_RX;

	pa &= ~PMAP_TNC_SRMMU;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & SRMMU_PGTYPE) == PG_SUN4M_OBMEM && managed(pa)) {
#ifdef DIAGNOSTIC
		if (!pmap_pa_exists(pa))
			panic("pmap_enter: no such address: 0x%lx", pa);
#endif
		pv = pvhead(pa);
	} else {
		pv = NULL;
	}
	pteproto |= (atop(pa) << SRMMU_PPNSHIFT);

	if (prot & VM_PROT_WRITE)
		pteproto |= PPROT_WRITE;

	ctx = getcontext4m();

	if (pm == pmap_kernel())
		pmap_enk4m(pm, va, prot, wired, pv, pteproto | PPROT_S);
	else
		pmap_enu4m(pm, va, prot, wired, pv, pteproto);

	setcontext4m(ctx);
}

/* enter new (or change existing) kernel mapping */
void
pmap_enk4m(pm, va, prot, wired, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, tpte, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (va < KERNBASE)
		panic("pmap_enk4m: can't enter va 0x%lx below KERNBASE", va);
#endif
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	s = splpmap();		/* XXX way too conservative */

	if (rp->rg_seg_ptps == NULL) /* enter new region */
		panic("pmap_enk4m: missing kernel region table for va 0x%lx",va);

	tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
	if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		int addr;

		/* old mapping exists, and is of the same pa type */

		if ((tpte & SRMMU_PPNMASK) == (pteproto & SRMMU_PPNMASK)) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot4m(pm, va, prot, wired);
			return;
		}

		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
#ifdef DEBUG
printf("pmap_enk4m: changing existing va=>pa entry: va 0x%lx, pteproto 0x%x, "
       "oldpte 0x%x\n", va, pteproto, tpte);
#endif
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			addr = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(addr))
				pv_unlink4m(pvhead(addr), pm, va);
			if (tpte & SRMMU_PG_C) {
				setcontext4m(0);	/* ??? */
				cache_flush_page((int)va);
			}
		}
	} else {
		/* adding new entry */
		sp->sg_npte++;
	}

	/*
	 * If the new mapping is for a managed PA, enter into pvlist.
	 * Note that the mapping for a malloc page will always be
	 * unique (hence will never cause a second call to malloc).
	 */
	if (pv != NULL)
	        pteproto &= ~(pv_link4m(pv, pm, va, (pteproto & SRMMU_PG_C) == 0));

#ifdef DEBUG
	if (sp->sg_pte == NULL) /* If no existing pagetable */
		panic("pmap_enk4m: missing segment table for va 0x%lx",va);
#endif

	tlb_flush_page(va);
	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);

	splx(s);
}

/* enter new (or change existing) user mapping */
void
pmap_enu4m(pm, va, prot, wired, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, *pte, tpte, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (KERNBASE < va)
		panic("pmap_enu4m: can't enter va 0x%lx above KERNBASE", va);
#endif

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splpmap();			/* XXX conservative */

rretry:
	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		int size = NSEGRG * sizeof (struct segmap);

		sp = (struct segmap *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (rp->rg_segmap != NULL) {
#ifdef DEBUG
printf("pmap_enu4m: segment filled during sleep\n");	/* can this happen? */
#endif
			free(sp, M_VMPMAP);
			goto rretry;
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		rp->rg_seg_ptps = NULL;
	}
rgretry:
	if (rp->rg_seg_ptps == NULL) {
		/* Need a segment table */
		int i, *ptd;

		ptd = pool_get(&L23_pool, PR_WAITOK);
		if (rp->rg_seg_ptps != NULL) {
#ifdef DEBUG
printf("pmap_enu4m: bizarre segment table fill during sleep\n");
#endif
			pool_put(&L23_pool, ptd);
			goto rgretry;
		}

		rp->rg_seg_ptps = ptd;
		for (i = 0; i < SRMMU_L2SIZE; i++)
			setpgt4m(&ptd[i], SRMMU_TEINVALID);
		setpgt4m(&pm->pm_reg_ptps[vr],
			 (VA2PA((caddr_t)ptd) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);
	}

	sp = &rp->rg_segmap[vs];

sretry:
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		int i;

		pte = pool_get(&L23_pool, PR_WAITOK);
		if (sp->sg_pte != NULL) {
printf("pmap_enter: pte filled during sleep\n");	/* can this happen? */
			pool_put(&L23_pool, pte);
			goto sretry;
		}

		sp->sg_pte = pte;
		sp->sg_npte = 1;
		rp->rg_nsegmap++;
		for (i = 0; i < SRMMU_L3SIZE; i++)
			setpgt4m(&pte[i], SRMMU_TEINVALID);
		setpgt4m(&rp->rg_seg_ptps[vs],
			(VA2PA((caddr_t)pte) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);
	} else {
		/*
		 * Might be a change: fetch old pte
		 */
		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);
			tlb_flush_page(va);
		}
		tpte = pte[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
			int addr;

			/* old mapping exists, and is of the same pa type */
			if ((tpte & SRMMU_PPNMASK) ==
			    (pteproto & SRMMU_PPNMASK)) {
				/* just changing prot and/or wiring */
				splx(s);
				/* caller should call this directly: */
				pmap_changeprot4m(pm, va, prot, wired);
				if (wired)
					pm->pm_stats.wired_count++;
				else
					pm->pm_stats.wired_count--;
				return;
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
#ifdef DEBUG
if (pmapdebug & PDB_SWITCHMAP)
printf("%s[%d]: pmap_enu: changing existing va 0x%x: pte 0x%x=>0x%x\n",
	curproc->p_comm, curproc->p_pid, (int)va, tpte, pteproto);
#endif
			if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
				addr = ptoa( (tpte & SRMMU_PPNMASK) >>
					     SRMMU_PPNSHIFT);
				if (managed(addr)) {
					pvhead(addr)->pv_flags |= MR4M(tpte);
					pv_unlink4m(pvhead(addr), pm, va);
				}
				if (pm->pm_ctx && (tpte & SRMMU_PG_C))
					cache_flush_page((int)va);
			}
		} else {
			/* adding new entry */
			sp->sg_npte++;

			/*
			 * Increment counters
			 */
			if (wired)
				pm->pm_stats.wired_count++;
		}
	}
	if (pv != NULL)
	        pteproto &= ~(pv_link4m(pv, pm, va, (pteproto & SRMMU_PG_C) == 0));

	/*
	 * Update PTEs, flush TLB as necessary.
	 */
	if (pm->pm_ctx) {
		setcontext4m(pm->pm_ctxnum);
		tlb_flush_page(va);
	}
	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);

	splx(s);
}
#endif /* SUN4M */

/*
 * Change the wiring attribute for a map/virtual-address pair.
 */
/* ARGSUSED */
void
pmap_change_wiring(pm, va, wired)
	struct pmap *pm;
	vaddr_t va;
	int wired;
{

	pmap_stats.ps_useless_changewire++;
}

/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */

#if defined(SUN4) || defined(SUN4C)
paddr_t
pmap_extract4_4c(pm, va)
	struct pmap *pm;
	vaddr_t va;
{
	int tpte;
	int vr, vs;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: null pmap\n");
#endif
		return (0);
	}
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	if (rp->rg_segmap == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid segment (%d)\n", vr);
#endif
		return (0);
	}
	sp = &rp->rg_segmap[vs];

	if (sp->sg_pmeg != seginval) {
		int ctx = getcontext4();

		if (CTX_USABLE(pm,rp)) {
			CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
			tpte = getpte4(va);
		} else {
			CHANGE_CONTEXTS(ctx, 0);
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			tpte = getpte4(VA_VPG(va) << PGSHIFT);
		}
		setcontext4(ctx);
	} else {
		int *pte = sp->sg_pte;

		if (pte == NULL) {
#ifdef DEBUG
			if (pmapdebug & PDB_FOLLOW)
				printf("pmap_extract: invalid segment\n");
#endif
			return (0);
		}
		tpte = pte[VA_VPG(va)];
	}
	if ((tpte & PG_V) == 0) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid pte\n");
#endif
		return (0);
	}
	tpte &= PG_PFNUM;
	tpte = tpte;
	return ((tpte << PGSHIFT) | (va & PGOFSET));
}
#endif /*4,4c*/

#if defined(SUN4M)		/* 4m version of pmap_extract */
/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */
paddr_t
pmap_extract4m(pm, va)
	struct pmap *pm;
	vaddr_t va;
{
	struct regmap *rm;
	struct segmap *sm;
	int pte;

	if (pm == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: null pmap\n");
#endif
		return (0);
	}

	if ((rm = pm->pm_regmap) == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: no regmap entry\n");
#endif
		return (0);
	}

	rm += VA_VREG(va);
	if ((sm = rm->rg_segmap) == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: no segmap\n");
#endif
		return (0);
	}

	sm += VA_VSEG(va);
	if (sm->sg_pte == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: no ptes\n");
#endif
		return (0);
	}

	pte = sm->sg_pte[VA_SUN4M_VPG(va)];
	if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid pte of type %d\n",
			       pte & SRMMU_TETYPE);
#endif
		return (0);
	}

	return (ptoa((pte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT) | VA_OFF(va));
}
#endif /* sun4m */

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
/* ARGSUSED */
int pmap_copy_disabled=0;
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	struct pmap *dst_pmap, *src_pmap;
	vaddr_t dst_addr;
	vsize_t len;
	vaddr_t src_addr;
{
#if notyet
	struct regmap *rm;
	struct segmap *sm;

	if (pmap_copy_disabled)
		return;
#ifdef DIAGNOSTIC
	if (VA_OFF(src_addr) != 0)
		printf("pmap_copy: addr not page aligned: 0x%lx\n", src_addr);
	if ((len & (NBPG-1)) != 0)
		printf("pmap_copy: length not page aligned: 0x%lx\n", len);
#endif

	if (src_pmap == NULL)
		return;

	if (CPU_ISSUN4M) {
		int i, npg, pte;
		paddr_t pa;

		npg = len >> PGSHIFT;
		for (i = 0; i < npg; i++) {
			tlb_flush_page(src_addr);
			if ((rm = src_pmap->pm_regmap) == NULL)
				continue;
			rm += VA_VREG(src_addr);

			if ((sm = rm->rg_segmap) == NULL)
				continue;
			sm += VA_VSEG(src_addr);
			if (sm->sg_npte == 0)
				continue;

			pte = sm->sg_pte[VA_SUN4M_VPG(src_addr)];
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
				continue;

			pa = ptoa((pte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			pmap_enter(dst_pmap, dst_addr,
				   pa,
				   (pte & PPROT_WRITE)
					? (VM_PROT_WRITE | VM_PROT_READ)
					: VM_PROT_READ,
				   0, 0);
			src_addr += NBPG;
			dst_addr += NBPG;
		}
	}
#endif
}

/*
 * Require that all active physical maps contain no
 * incorrect entries NOW.  [This update includes
 * forcing updates of any address map caching.]
 */
void
pmap_update()
{
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		tlb_flush_all();	/* %%%: Extreme Paranoia?  */
#endif
}

/*
 * Garbage collects the physical map system for
 * pages which are no longer used.
 * Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but
 * others may be collected.
 * Called by the pageout daemon when pages are scarce.
 */
/* ARGSUSED */
void
pmap_collect(pm)
	struct pmap *pm;
{
}

#if defined(SUN4) || defined(SUN4C)

/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify4_4c(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_4 & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4_4c(pv);
		pv->pv_flags &= ~PV_MOD;
	}
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified4_4c(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_4 & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_MOD || pv_syncflags4_4c(pv) & PV_MOD)
			return (1);
	}
	return (0);
}

/*
 * Clear the reference bit for the given physical page.
 */
void
pmap_clear_reference4_4c(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_4 & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4_4c(pv);
		pv->pv_flags &= ~PV_REF;
	}
}

/*
 * Tell whether the given physical page has been referenced.
 */
int
pmap_is_referenced4_4c(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_4 & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_REF || pv_syncflags4_4c(pv) & PV_REF)
			return (1);
	}
	return (0);
}
#endif /*4,4c*/

#if defined(SUN4M)

/*
 * 4m versions of bit test/set routines
 *
 * Note that the 4m-specific routines should eventually service these
 * requests from their page tables, and the whole pvlist bit mess should
 * be dropped for the 4m (unless this causes a performance hit from
 * tracing down pagetables/regmap/segmaps).
 */

/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify4m(pa)	   /* XXX %%%: Should service from swpagetbl for 4m */
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_SRMMU & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4m(pv);
		pv->pv_flags &= ~PV_MOD4M;
	}
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified4m(pa) /* Test performance with SUN4M && SUN4/4C. XXX */
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_SRMMU & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_MOD4M || pv_syncflags4m(pv) & PV_MOD4M)
		        return(1);
	}
	return (0);
}

/*
 * Clear the reference bit for the given physical page.
 */
void
pmap_clear_reference4m(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_SRMMU & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4m(pv);
		pv->pv_flags &= ~PV_REF4M;
	}
}

/*
 * Tell whether the given physical page has been referenced.
 */
int
pmap_is_referenced4m(pa)
	paddr_t pa;
{
	struct pvlist *pv;

	if ((pa & (PMAP_TNC_SRMMU & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_REF4M || pv_syncflags4m(pv) & PV_REF4M)
		        return(1);
	}
	return (0);
}
#endif /* 4m */

/*
 * Make the specified pages (by pmap, offset) pageable (or not) as requested.
 *
 * A page which is not pageable may not take a fault; therefore, its page
 * table entry must remain valid for the duration (or at least, the trap
 * handler must not call vm_fault).
 *
 * This routine is merely advisory; pmap_enter will specify that these pages
 * are to be wired down (or not) as appropriate.
 */
/* ARGSUSED */
void
pmap_pageable(pm, start, end, pageable)
	struct pmap *pm;
	vaddr_t start, end;
	int pageable;
{
}

/*
 * Fill the given MI physical page with zero bytes.
 *
 * We avoid stomping on the cache.
 * XXX	might be faster to use destination's context and allow cache to fill?
 */

#if defined(SUN4) || defined(SUN4C)

void
pmap_zero_page4_4c(pa)
	paddr_t pa;
{
	caddr_t va;
	int pte;

	if (((pa & (PMAP_TNC_4 & ~PMAP_NC)) == 0) && managed(pa)) {
		/*
		 * The following might not be necessary since the page
		 * is being cleared because it is about to be allocated,
		 * i.e., is in use by no one.
		 */
		pv_flushcache(pvhead(pa));
	}
	pte = PG_V | PG_S | PG_W | PG_NC | (atop(pa) & PG_PFNUM);

	va = vpage[0];
	setpte4(va, pte);
	qzero(va, NBPG);
	setpte4(va, 0);
}

/*
 * Copy the given MI physical source page to its destination.
 *
 * We avoid stomping on the cache as above (with same `XXX' note).
 * We must first flush any write-back cache for the source page.
 * We go ahead and stomp on the kernel's virtual cache for the
 * source page, since the cache can read memory MUCH faster than
 * the processor.
 */
void
pmap_copy_page4_4c(src, dst)
	paddr_t src, dst;
{
	caddr_t sva, dva;
	int spte, dpte;

	if (managed(src)) {
		if (CACHEINFO.c_vactype == VAC_WRITEBACK)
			pv_flushcache(pvhead(src));
	}
	spte = PG_V | PG_S | (atop(src) & PG_PFNUM);

	if (managed(dst)) {
		/* similar `might not be necessary' comment applies */
		if (CACHEINFO.c_vactype != VAC_NONE)
			pv_flushcache(pvhead(dst));
	}
	dpte = PG_V | PG_S | PG_W | PG_NC | (atop(dst) & PG_PFNUM);

	sva = vpage[0];
	dva = vpage[1];
	setpte4(sva, spte);
	setpte4(dva, dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
	cache_flush_page((vaddr_t)sva);
	setpte4(sva, 0);
	setpte4(dva, 0);
}
#endif /* 4, 4c */

#if defined(SUN4M)		/* Sun4M version of copy/zero routines */
/*
 * Fill the given MI physical page with zero bytes.
 *
 * We avoid stomping on the cache.
 * XXX	might be faster to use destination's context and allow cache to fill?
 */
int xxxdebug = 0;
void
pmap_zero_page4m(pa)
	paddr_t pa;
{
	caddr_t va;
	int pte;

	if (((pa & (PMAP_TNC_SRMMU & ~PMAP_NC)) == 0) && managed(pa)) {
		/*
		 * The following might not be necessary since the page
		 * is being cleared because it is about to be allocated,
		 * i.e., is in use by no one.
		 */
		if (CACHEINFO.c_vactype != VAC_NONE)
			pv_flushcache(pvhead(pa));
	}
	pte = SRMMU_TEPTE | PPROT_N_RWX | (atop(pa) << SRMMU_PPNSHIFT);
	if (cpuinfo.flags & CPUFLG_CACHE_MANDATORY)
		pte |= SRMMU_PG_C;

	va = vpage[0];
	setpgt4m(vpage_pte[0], pte);
	qzero(va, NBPG);
	/* Remove temporary mapping */
	tlb_flush_page(va);
	setpgt4m(vpage_pte[0], SRMMU_TEINVALID);
}

/*
 * Copy the given MI physical source page to its destination.
 *
 * We avoid stomping on the cache as above (with same `XXX' note).
 * We must first flush any write-back cache for the source page.
 * We go ahead and stomp on the kernel's virtual cache for the
 * source page, since the cache can read memory MUCH faster than
 * the processor.
 */
void
pmap_copy_page4m(src, dst)
	paddr_t src, dst;
{
	caddr_t sva, dva;
	int spte, dpte;

	if (managed(src)) {
		if (CACHEINFO.c_vactype == VAC_WRITEBACK)
			pv_flushcache(pvhead(src));
	}

	spte = SRMMU_TEPTE | SRMMU_PG_C | PPROT_N_RX |
		(atop(src) << SRMMU_PPNSHIFT);

	if (managed(dst)) {
		/* similar `might not be necessary' comment applies */
		if (CACHEINFO.c_vactype != VAC_NONE)
			pv_flushcache(pvhead(dst));
	}

	dpte = SRMMU_TEPTE | PPROT_N_RWX | (atop(dst) << SRMMU_PPNSHIFT);
	if (cpuinfo.flags & CPUFLG_CACHE_MANDATORY)
		dpte |= SRMMU_PG_C;

	sva = vpage[0];
	dva = vpage[1];
	setpgt4m(vpage_pte[0], spte);
	setpgt4m(vpage_pte[1], dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
	cache_flush_page((vaddr_t)sva);
	tlb_flush_page(sva);
	setpgt4m(vpage_pte[0], SRMMU_TEINVALID);
	tlb_flush_page(dva);
	setpgt4m(vpage_pte[1], SRMMU_TEINVALID);
}
#endif /* SUN4M */

/*
 * Turn a cdevsw d_mmap value into a byte address for pmap_enter.
 * XXX	this should almost certainly be done differently, and
 *	elsewhere, or even not at all
 */
paddr_t
pmap_phys_address(x)
	int x;
{

	return ((paddr_t)x);
}

/*
 * Turn off cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
void
kvm_uncache(va, npages)
	caddr_t va;
	int npages;
{
	int pte;
	paddr_t pa;

	if (CPU_ISSUN4M) {
#if defined(SUN4M)
		int ctx = getcontext4m();

		setcontext4m(0);
		for (; --npages >= 0; va += NBPG) {
			pte = getpte4m((vaddr_t) va);
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
				panic("kvm_uncache: table entry not pte");

			if ((pte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM)
				cache_flush_page((int)va);

			pa = ptoa((pte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);

			if ((pte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM &&
			    managed(pa)) {
				pv_changepte4m(pvhead(pa), 0, SRMMU_PG_C);
			}
			pte &= ~SRMMU_PG_C;
			setpte4m((vaddr_t) va, pte);
		
		}
		setcontext4m(ctx);
#endif
	} else {
#if defined(SUN4) || defined(SUN4C)
		for (; --npages >= 0; va += NBPG) {
			pte = getpte4(va);
			if ((pte & PG_V) == 0)
				panic("kvm_uncache !pg_v");

			pa = ptoa(pte & PG_PFNUM);
			if ((pte & PG_TYPE) == PG_OBMEM &&
			    managed(pa)) {
				pv_changepte4_4c(pvhead(pa), PG_NC, 0);
			}
			pte |= PG_NC;
			setpte4(va, pte);
			if ((pte & PG_TYPE) == PG_OBMEM)
				cache_flush_page((int)va);
		}
#endif
	}
}

/*
 * Turn on IO cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
void
kvm_iocache(va, npages)
	caddr_t va;
	int npages;
{

#ifdef SUN4M
	if (CPU_ISSUN4M) /* %%%: Implement! */
		panic("kvm_iocache: 4m iocache not implemented");
#endif
#if defined(SUN4) || defined(SUN4C)
	for (; --npages >= 0; va += NBPG) {
		int pte = getpte4(va);
		if ((pte & PG_V) == 0)
			panic("kvm_iocache !pg_v");
		pte |= PG_IOC;
		setpte4(va, pte);
	}
#endif
}

int
pmap_count_ptes(pm)
	struct pmap *pm;
{
	int idx, total;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == pmap_kernel()) {
		rp = &pm->pm_regmap[NUREG];
		idx = NKREG;
	} else {
		rp = pm->pm_regmap;
		idx = NUREG;
	}
	for (total = 0; idx;)
		if ((sp = rp[--idx].rg_segmap) != NULL)
			total += sp->sg_npte;
	pm->pm_stats.resident_count = total;
	return (total);
}

/*
 * Find first virtual address >= *va that is
 * least likely to cause cache aliases.
 * (This will just seg-align mappings.)
 */
void
pmap_prefer(foff, vap)
	vaddr_t foff;
	vaddr_t *vap;
{
	vaddr_t va = *vap;
	long d, m;

	if (VA_INHOLE(va))
		va = MMU_HOLE_END;

	m = CACHE_ALIAS_DIST;
	if (m == 0)		/* m=0 => no cache aliasing */
		return;

	d = foff - va;
	d &= (m - 1);
	*vap = va + d;
}

void
pmap_redzone()
{
	pmap_remove(pmap_kernel(), KERNBASE, KERNBASE+NBPG);
}

/*
 * Activate the address space for the specified process.  If the
 * process is the current process, load the new MMU context.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int s;

	/*
	 * This is essentially the same thing that happens in cpu_switch()
	 * when the newly selected process is about to run, except that we
	 * have to make sure to clean the register windows before we set
	 * the new context.
	 */

	s = splpmap();
	if (p == curproc) {
		write_user_windows();
		if (pmap->pm_ctx == NULL) {
			ctx_alloc(pmap);	/* performs setcontext() */
		} else {
			/* Do any cache flush needed on context switch */
			(*cpuinfo.pure_vcache_flush)();
			setcontext(pmap->pm_ctxnum);
		}
	}
	splx(s);
}

/*
 * Deactivate the address space of the specified process.
 */
void
pmap_deactivate(p)
	struct proc *p;
{
}

#ifdef DEBUG
/*
 * Check consistency of a pmap (time consuming!).
 */
void
pm_check(s, pm)
	char *s;
	struct pmap *pm;
{
	if (pm == pmap_kernel())
		pm_check_k(s, pm);
	else
		pm_check_u(s, pm);
}

void
pm_check_u(s, pm)
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	struct segmap *sp;
	int n, vs, vr, j, m, *pte;

	if (pm->pm_regmap == NULL)
		panic("%s: CHK(pmap %p): no region mapping", s, pm);

#if defined(SUN4M)
	if (CPU_ISSUN4M &&
	    (pm->pm_reg_ptps == NULL ||
	     pm->pm_reg_ptps_pa != VA2PA((caddr_t)pm->pm_reg_ptps)))
		panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: "
		      "tblva=%p, tblpa=0x%x",
			s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M && pm->pm_ctx != NULL &&
	    (cpuinfo.ctx_tbl[pm->pm_ctxnum] != ((VA2PA((caddr_t)pm->pm_reg_ptps)
					      >> SRMMU_PPNPASHIFT) |
					     SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at 0x%x not installed "
		  "for context %d", s, pm, pm->pm_reg_ptps_pa, pm->pm_ctxnum);
#endif

	for (vr = 0; vr < NUREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			continue;
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
#if defined(SUN4M)
		if (CPU_ISSUN4M && rp->rg_seg_ptps == NULL)
		    panic("%s: CHK(vr %d): nsegmap=%d; no SRMMU segment table",
			  s, vr, rp->rg_nsegmap);
		if (CPU_ISSUN4M &&
		    pm->pm_reg_ptps[vr] != ((VA2PA((caddr_t)rp->rg_seg_ptps) >>
					    SRMMU_PPNPASHIFT) | SRMMU_TEPTD))
		    panic("%s: CHK(vr %d): SRMMU segtbl not installed",s,vr);
#endif
		if ((unsigned int)rp < KERNBASE)
			panic("%s: rp=%p", s, rp);
		n = 0;
		for (vs = 0; vs < NSEGRG; vs++) {
			sp = &rp->rg_segmap[vs];
			if ((unsigned int)sp < KERNBASE)
				panic("%s: sp=%p", s, sp);
			if (sp->sg_npte != 0) {
				n++;
				if (sp->sg_pte == NULL)
					panic("%s: CHK(vr %d, vs %d): npte=%d, "
					   "pte=NULL", s, vr, vs, sp->sg_npte);
#if defined(SUN4M)
				if (CPU_ISSUN4M &&
				    rp->rg_seg_ptps[vs] !=
				     ((VA2PA((caddr_t)sp->sg_pte)
					>> SRMMU_PPNPASHIFT) |
				       SRMMU_TEPTD))
				    panic("%s: CHK(vr %d, vs %d): SRMMU page "
					  "table not installed correctly",s,vr,
					  vs);
#endif
				pte=sp->sg_pte;
				m = 0;
				for (j=0; j<NPTESG; j++,pte++)
				    if ((CPU_ISSUN4M
					 ?((*pte & SRMMU_TETYPE) == SRMMU_TEPTE)
					 :(*pte & PG_V)))
					m++;
				if (m != sp->sg_npte)
				    /*if (pmapdebug & 0x10000)*/
					printf("%s: user CHK(vr %d, vs %d): "
					    "npte(%d) != # valid(%d)\n",
						s, vr, vs, sp->sg_npte, m);
			}
		}
		if (n != rp->rg_nsegmap)
			panic("%s: CHK(vr %d): inconsistent "
				"# of pte's: %d, should be %d",
				s, vr, rp->rg_nsegmap, n);
	}
	return;
}

void
pm_check_k(s, pm)		/* Note: not as extensive as pm_check_u. */
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	int vr, vs, n;

	if (pm->pm_regmap == NULL)
		panic("%s: CHK(pmap %p): no region mapping", s, pm);

#if defined(SUN4M)
	if (CPU_ISSUN4M &&
	    (pm->pm_reg_ptps == NULL ||
	     pm->pm_reg_ptps_pa != VA2PA((caddr_t)pm->pm_reg_ptps)))
	    panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: tblva=%p, tblpa=0x%x",
		  s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M &&
	    (cpuinfo.ctx_tbl[0] != ((VA2PA((caddr_t)pm->pm_reg_ptps) >>
					     SRMMU_PPNPASHIFT) | SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at 0x%x not installed "
		  "for context %d", s, pm, pm->pm_reg_ptps_pa, 0);
#endif
	for (vr = NUREG; vr < NUREG+NKREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
		if (rp->rg_nsegmap == 0)
			continue;
#if defined(SUN4M)
		if (CPU_ISSUN4M && rp->rg_seg_ptps == NULL)
		    panic("%s: CHK(vr %d): nsegmap=%d; no SRMMU segment table",
			  s, vr, rp->rg_nsegmap);
		if (CPU_ISSUN4M &&
		    pm->pm_reg_ptps[vr] != ((VA2PA((caddr_t)rp->rg_seg_ptps) >>
					    SRMMU_PPNPASHIFT) | SRMMU_TEPTD))
		    panic("%s: CHK(vr %d): SRMMU segtbl not installed",s,vr);
#endif
		if (CPU_ISSUN4M) {
			n = NSEGRG;
		} else {
			for (n = 0, vs = 0; vs < NSEGRG; vs++) {
				if (rp->rg_segmap[vs].sg_npte)
					n++;
			}
		}
		if (n != rp->rg_nsegmap)
			printf("%s: kernel CHK(vr %d): inconsistent "
				"# of pte's: %d, should be %d\n",
				s, vr, rp->rg_nsegmap, n);
	}
	return;
}
#endif

/*
 * Return the number of disk blocks that pmap_dumpmmu() will dump.
 */
int
pmap_dumpsize()
{
	int	sz;

	sz = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	sz += npmemarr * sizeof(phys_ram_seg_t);
	sz += sizeof(kernel_segmap_store);

	if (CPU_ISSUN4OR4C)
		/* For each pmeg in the MMU, we'll write NPTESG PTEs. */
		sz += (seginval + 1) * NPTESG * sizeof(int);

	return btodb(sz + DEV_BSIZE - 1);
}

/*
 * Write the core dump headers and MD data to the dump device.
 * We dump the following items:
 * 
 *	kcore_seg_t		 MI header defined in <sys/kcore.h>)
 *	cpu_kcore_hdr_t		 MD header defined in <machine/kcore.h>)
 *	phys_ram_seg_t[npmemarr] physical memory segments
 *	segmap_t[NKREG*NSEGRG]	 the kernel's segment map
 *	the MMU pmegs on sun4/sun4c
 */
int
pmap_dumpmmu(dump, blkno)
	daddr_t blkno;
	int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
{
	kcore_seg_t	*ksegp;
	cpu_kcore_hdr_t	*kcpup;
	phys_ram_seg_t	memseg;
	int		error = 0;
	int		i, memsegoffset, segmapoffset, pmegoffset;
	int		buffer[dbtob(1) / sizeof(int)];
	int		*bp, *ep;
#if defined(SUN4C) || defined(SUN4)
	int	pmeg;
#endif

#define EXPEDITE(p,n) do {						\
	int *sp = (int *)(p);						\
	int sz = (n);							\
	while (sz > 0) {						\
		*bp++ = *sp++;						\
		if (bp >= ep) {						\
			error = (*dump)(dumpdev, blkno,			\
					(caddr_t)buffer, dbtob(1));	\
			if (error != 0)					\
				return (error);				\
			++blkno;					\
			bp = buffer;					\
		}							\
		sz -= 4;						\
	}								\
} while (0)

	setcontext(0);

	/* Setup bookkeeping pointers */
	bp = buffer;
	ep = &buffer[sizeof(buffer) / sizeof(buffer[0])];

	/* Fill in MI segment header */
	ksegp = (kcore_seg_t *)bp;
	CORE_SETMAGIC(*ksegp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	ksegp->c_size = dbtob(pmap_dumpsize()) - ALIGN(sizeof(kcore_seg_t));

	/* Fill in MD segment header (interpreted by MD part of libkvm) */
	kcpup = (cpu_kcore_hdr_t *)((int)bp + ALIGN(sizeof(kcore_seg_t)));
	kcpup->cputype = cputyp;
	kcpup->kernbase = KERNBASE;
	kcpup->nmemseg = npmemarr;
	kcpup->memsegoffset = memsegoffset = ALIGN(sizeof(cpu_kcore_hdr_t));
	kcpup->nsegmap = NKREG*NSEGRG;
	kcpup->segmapoffset = segmapoffset =
		memsegoffset + npmemarr * sizeof(phys_ram_seg_t);

	kcpup->npmeg = (CPU_ISSUN4OR4C) ? seginval + 1 : 0; 
	kcpup->pmegoffset = pmegoffset =
		segmapoffset + kcpup->nsegmap * sizeof(struct segmap);

	/* Note: we have assumed everything fits in buffer[] so far... */
	bp = (int *)((int)kcpup + ALIGN(sizeof(cpu_kcore_hdr_t)));

#if 0
	/* Align storage for upcoming quad-aligned segment array */
	while (bp != (int *)ALIGN(bp)) {
		int dummy = 0;
		EXPEDITE(&dummy, 4);
	}
#endif

	for (i = 0; i < npmemarr; i++) {
		memseg.start = pmemarr[i].addr;
		memseg.size = pmemarr[i].len;
		EXPEDITE(&memseg, sizeof(phys_ram_seg_t));
	}

	EXPEDITE(&kernel_segmap_store, sizeof(kernel_segmap_store));

	if (CPU_ISSUN4M)
		goto out;

#if defined(SUN4C) || defined(SUN4)
	/*
	 * dump page table entries
	 *
	 * We dump each pmeg in order (by segment number).  Since the MMU
	 * automatically maps the given virtual segment to a pmeg we must
	 * iterate over the segments by incrementing an unused segment slot
	 * in the MMU.  This fixed segment number is used in the virtual
	 * address argument to getpte().
	 */

	/*
	 * Go through the pmegs and dump each one.
	 */
	for (pmeg = 0; pmeg <= seginval; ++pmeg) {
		int va = 0;

		setsegmap(va, pmeg);
		i = NPTESG;
		do {
			int pte = getpte4(va);
			EXPEDITE(&pte, sizeof(pte));
			va += NBPG;
		} while (--i > 0);
	}
	setsegmap(0, seginval);
#endif

out:
	if (bp != buffer)
		error = (*dump)(dumpdev, blkno++, (caddr_t)buffer, dbtob(1));

	return (error);
}

/*
 * Helper function for debuggers.
 */
void
pmap_writetext(dst, ch)
	unsigned char *dst;
	int ch;
{
	int s, pte0, pte, ctx;
	vaddr_t va;

	s = splpmap();
	va = (unsigned long)dst & (~PGOFSET);
	cpuinfo.cache_flush(dst, 1);

	ctx = getcontext();
	setcontext(0);

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		pte0 = getpte4m(va);
		if ((pte0 & SRMMU_TETYPE) != SRMMU_TEPTE) {
			splx(s);
			return;
		}
		pte = pte0 | PPROT_WRITE;
		setpte4m(va, pte);
		*dst = (unsigned char)ch;
		setpte4m(va, pte0);

	}
#endif
#if defined(SUN4) || defined(SUN4C)
	if (CPU_ISSUN4C || CPU_ISSUN4) {
		pte0 = getpte4(va);
		if ((pte0 & PG_V) == 0) {
			splx(s);
			return;
		}
		pte = pte0 | PG_W;
		setpte4(va, pte);
		*dst = (unsigned char)ch;
		setpte4(va, pte0);
	}
#endif
	cpuinfo.cache_flush(dst, 1);
	setcontext(ctx);
	splx(s);
}

#ifdef EXTREME_DEBUG

static void test_region __P((int, int, int));

void
debug_pagetables()
{
	int i;
	int *regtbl;
	int te;

	printf("\nncontext=%d. ",ncontext);
	printf("Context table is at va 0x%x. Level 0 PTP: 0x%x\n",
	       cpuinfo.ctx_tbl, cpuinfo.ctx_tbl[0]);
	printf("Context 0 region table is at va 0x%x, pa 0x%x. Contents:\n",
	       pmap_kernel()->pm_reg_ptps, pmap_kernel()->pm_reg_ptps_pa);

	regtbl = pmap_kernel()->pm_reg_ptps;

	printf("PROM vector is at 0x%x\n",promvec);
	printf("PROM reboot routine is at 0x%x\n",promvec->pv_reboot);
	printf("PROM abort routine is at 0x%x\n",promvec->pv_abort);
	printf("PROM halt routine is at 0x%x\n",promvec->pv_halt);

	printf("Testing region 0xfe: ");
	test_region(0xfe,0,16*1024*1024);
	printf("Testing region 0xff: ");
	test_region(0xff,0,16*1024*1024);
	printf("Testing kernel region 0x%x: ", VA_VREG(KERNBASE));
	test_region(VA_VREG(KERNBASE), 4096, avail_start);
	cngetc();

	for (i = 0; i < SRMMU_L1SIZE; i++) {
		te = regtbl[i];
		if ((te & SRMMU_TETYPE) == SRMMU_TEINVALID)
		    continue;
		printf("Region 0x%x: PTE=0x%x <%s> L2PA=0x%x kernL2VA=0x%x\n",
		       i, te, ((te & SRMMU_TETYPE) == SRMMU_TEPTE ? "pte" :
			       ((te & SRMMU_TETYPE) == SRMMU_TEPTD ? "ptd" :
				((te & SRMMU_TETYPE) == SRMMU_TEINVALID ?
				 "invalid" : "reserved"))),
		       (te & ~0x3) << SRMMU_PPNPASHIFT,
		       pmap_kernel()->pm_regmap[i].rg_seg_ptps);
	}
	printf("Press q to halt...\n");
	if (cngetc()=='q')
	    callrom();
}

static u_int
VA2PAsw(ctx, addr, pte)
	int ctx;
	caddr_t addr;
	int *pte;
{
	int *curtbl;
	int curpte;

#ifdef EXTREME_EXTREME_DEBUG
	printf("Looking up addr 0x%x in context 0x%x\n",addr,ctx);
#endif
	/* L0 */
	*pte = curpte = cpuinfo.ctx_tbl[ctx];
#ifdef EXTREME_EXTREME_DEBUG
	printf("Got L0 pte 0x%x\n",pte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
			((u_int)addr & 0xffffffff));
	}
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad context table entry 0x%x for context 0x%x\n",
		       curpte, ctx);
		return 0;
	}
	/* L1 */
	curtbl = ((curpte & ~0x3) << 4) | KERNBASE; /* correct for krn*/
	*pte = curpte = curtbl[VA_VREG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L1 table at 0x%x.\nGot L1 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffff));
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad region table entry 0x%x for region 0x%x\n",
		       curpte, VA_VREG(addr));
		return 0;
	}
	/* L2 */
	curtbl = ((curpte & ~0x3) << 4) | KERNBASE; /* correct for krn*/
	*pte = curpte = curtbl[VA_VSEG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L2 table at 0x%x.\nGot L2 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0x3ffff));
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad segment table entry 0x%x for reg 0x%x, seg 0x%x\n",
		       curpte, VA_VREG(addr), VA_VSEG(addr));
		return 0;
	}
	/* L3 */
	curtbl = ((curpte & ~0x3) << 4) | KERNBASE; /* correct for krn*/
	*pte = curpte = curtbl[VA_VPG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L3 table at 0x%x.\nGot L3 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xfff));
	else {
		printf("Bad L3 pte 0x%x for reg 0x%x, seg 0x%x, pg 0x%x\n",
		       curpte, VA_VREG(addr), VA_VSEG(addr), VA_VPG(addr));
		return 0;
	}
	printf("Bizarreness with address 0x%x!\n",addr);
}

void test_region(reg, start, stop)
	int reg;
	int start, stop;
{
	int i;
	int addr;
	int pte;
	int ptesw;
/*	int cnt=0;
*/

	for (i = start; i < stop; i+= NBPG) {
		addr = (reg << RGSHIFT) | i;
		pte=lda(((u_int)(addr)) | ASI_SRMMUFP_LN, ASI_SRMMUFP);
		if (pte) {
/*			printf("Valid address 0x%x\n",addr);
			if (++cnt == 20) {
				cngetc();
				cnt=0;
			}
*/
			if (VA2PA(addr) != VA2PAsw(0,addr,&ptesw)) {
				printf("Mismatch at address 0x%x.\n",addr);
				if (cngetc()=='q') break;
			}
			if (reg == VA_VREG(KERNBASE))
				/* kernel permissions are different */
				continue;
			if ((pte&SRMMU_PROT_MASK)!=(ptesw&SRMMU_PROT_MASK)) {
				printf("Mismatched protections at address "
				       "0x%x; pte=0x%x, ptesw=0x%x\n",
				       addr,pte,ptesw);
				if (cngetc()=='q') break;
			}
		}
	}
	printf("done.\n");
}


void print_fe_map(void)
{
	u_int i, pte;

	printf("map of region 0xfe:\n");
	for (i = 0xfe000000; i < 0xff000000; i+=4096) {
		if (((pte = getpte4m(i)) & SRMMU_TETYPE) != SRMMU_TEPTE)
		    continue;
		printf("0x%x -> 0x%x%x (pte 0x%x)\n", i, pte >> 28,
		       (pte & ~0xff) << 4, pte);
	}
	printf("done\n");
}

#endif
