/*	$NetBSD: pmap.c,v 1.113.4.1 2002/06/07 19:30:09 thorpej Exp $	   */
/*
 * Copyright (c) 1994, 1998, 1999 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "opt_ddb.h"
#include "opt_cputype.h"
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#include "opt_pipe.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <uvm/uvm_extern.h>

#ifdef PMAPDEBUG
#include <dev/cons.h>
#endif

#include <uvm/uvm.h>

#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/mtpr.h>
#include <machine/macros.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/scb.h>
#include <machine/rpb.h>

/* QDSS console mapping hack */
#include "qd.h"
void	qdearly(void);

#define ISTACK_SIZE NBPG
vaddr_t istack;
/* 
 * This code uses bitfield operators for most page table entries.  
 */
#define PROTSHIFT	27
#define PROT_KW		(PG_KW >> PROTSHIFT)
#define PROT_KR		(PG_KR >> PROTSHIFT) 
#define PROT_RW		(PG_RW >> PROTSHIFT)
#define PROT_RO		(PG_RO >> PROTSHIFT)
#define PROT_URKW	(PG_URKW >> PROTSHIFT)

/*
 * Scratch pages usage:
 * Page 1: initial frame pointer during autoconfig. Stack and pcb for
 *	   processes during exit on boot cpu only.
 * Page 2: cpu_info struct for any cpu.
 * Page 3: unused
 * Page 4: unused
 */
long	scratch;
#define SCRATCHPAGES	4


struct pmap kernel_pmap_store;

struct	pte *Sysmap;		/* System page table */
struct	pv_entry *pv_table;	/* array of entries, one per LOGICAL page */
int	pventries;
vaddr_t iospace;

vaddr_t ptemapstart, ptemapend;
struct	extent *ptemap;
#define PTMAPSZ EXTENT_FIXED_STORAGE_SIZE(100)
char	ptmapstorage[PTMAPSZ];

extern	caddr_t msgbufaddr;

#define IOSPACE(p)	(((u_long)(p)) & 0xe0000000)
#define NPTEPROCSPC	0x1000	/* # of virtual PTEs per process space */
#define NPTEPG		0x80	/* # of PTEs per page (logical or physical) */
#define PPTESZ		sizeof(struct pte)
#define NOVADDR		0xffffffff /* Illegal virtual address */
#define WAITOK		M_WAITOK
#define NOWAIT		M_NOWAIT
#define NPTEPERREG	0x200000

#ifdef PMAPDEBUG
volatile int recurse;
#define RECURSESTART {							\
	if (recurse)							\
		printf("enter at %d, previous %d\n", __LINE__, recurse);\
	recurse = __LINE__;						\
}
#define RECURSEEND {recurse = 0; }
#define PMDEBUG(x) if (startpmapdebug)printf x
#else
#define RECURSESTART
#define RECURSEEND
#define PMDEBUG(x)
#endif

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
static struct simplelock pvtable_lock;
#define PVTABLE_LOCK	simple_lock(&pvtable_lock);
#define PVTABLE_UNLOCK	simple_unlock(&pvtable_lock);
#else
#define PVTABLE_LOCK
#define PVTABLE_UNLOCK
#endif

#ifdef PMAPDEBUG
int	startpmapdebug = 0;
#endif

vaddr_t	  avail_start, avail_end;
vaddr_t	  virtual_avail, virtual_end; /* Available virtual memory	*/

struct pv_entry *get_pventry(void);
void free_pventry(struct pv_entry *);
void more_pventries(void);

/*
 * Calculation of the System Page Table is somewhat a pain, because it
 * must be in contiguous physical memory and all size calculations must
 * be done before memory management is turned on.
 * Arg is usrptsize in ptes.
 */
static vsize_t
calc_kvmsize(vsize_t usrptsize)
{
	extern int bufcache;
	vsize_t kvmsize;
	int n, s, bp, bc;

	/* All physical memory */
	kvmsize = avail_end;
	/* User Page table area. This may be large */
	kvmsize += (usrptsize * sizeof(struct pte));
	/* Kernel stacks per process */
	kvmsize += (USPACE * maxproc);
	/* kernel malloc arena */
	kvmsize += (NKMEMPAGES_MAX_DEFAULT * NBPG +
	    NKMEMPAGES_MAX_DEFAULT * sizeof(struct kmemusage));
	/* IO device register space */
	kvmsize += (IOSPSZ * VAX_NBPG);
	/* Pager allocations */
	kvmsize += (PAGER_MAP_SIZE + MAXBSIZE);
	/* Anon pool structures */
	kvmsize += (physmem * sizeof(struct vm_anon));

	/* allocated buffer space etc... This is a hack */
	n = nbuf; s = nswbuf; bp = bufpages; bc = bufcache;
	kvmsize += (int)allocsys(NULL, NULL);
	/* Buffer space */
	kvmsize += (MAXBSIZE * nbuf);
	nbuf = n; nswbuf = s; bufpages = bp; bufcache = bc;

	/* UBC submap space */
	kvmsize += (UBC_NWINS << UBC_WINSHIFT);

	/* Exec arg space */
	kvmsize += NCARGS;
#if VAX46 || VAX48 || VAX49 || VAX53 || VAXANY
	/* Physmap */
	kvmsize += VM_PHYS_SIZE;
#endif
#ifdef LKM
	/* LKMs are allocated out of kernel_map */
#define MAXLKMSIZ	0x100000	/* XXX */
	kvmsize += MAXLKMSIZ;
#endif

	/* The swapper uses many anon's, set an arbitrary size */
#ifndef SWAPSIZE
#define	SWAPSIZE (200*1024*1024)	/* Assume 200MB swap */
#endif
	kvmsize += ((SWAPSIZE/PAGE_SIZE)*sizeof(struct vm_anon));

	/* New pipes may steal some amount of memory. Calculate 10 pipes */
#ifndef PIPE_SOCKETPAIR
	kvmsize += PIPE_DIRECT_CHUNK*10;
#endif
	return kvmsize;
}

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */
void
pmap_bootstrap()
{
	unsigned int sysptsize, i;
	extern	unsigned int etext, proc0paddr;
	struct pcb *pcb = (struct pcb *)proc0paddr;
	pmap_t pmap = pmap_kernel();
	vsize_t kvmsize, usrptsize;

	/* Set logical page size */
	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	physmem = btoc(avail_end);

	usrptsize = PROCPTSIZE * maxproc;
	if (vax_btop(usrptsize)* PPTESZ > avail_end/20)
		usrptsize = (avail_end/(20 * PPTESZ)) * VAX_NBPG;
		
	kvmsize = calc_kvmsize(usrptsize);
	sysptsize = kvmsize >> VAX_PGSHIFT;
	/*
	 * Virtual_* and avail_* is used for mapping of system page table.
	 * The need for kernel virtual memory is linear dependent of the
	 * amount of physical memory also, therefore sysptsize is 
	 * a variable here that is changed dependent of the physical
	 * memory size.
	 */
	virtual_avail = avail_end + KERNBASE;
	virtual_end = KERNBASE + sysptsize * VAX_NBPG;
	memset(Sysmap, 0, sysptsize * 4); /* clear SPT before using it */

	/*
	 * The first part of Kernel Virtual memory is the physical
	 * memory mapped in. This makes some mm routines both simpler
	 * and faster, but takes ~0.75% more memory.
	 */
	pmap_map(KERNBASE, 0, avail_end, VM_PROT_READ|VM_PROT_WRITE);
	/*
	 * Kernel code is always readable for user, it must be because
	 * of the emulation code that is somewhere in there.
	 * And it doesn't hurt, /netbsd is also public readable.
	 * There are also a couple of other things that must be in
	 * physical memory and that isn't managed by the vm system.
	 */
	for (i = 0; i < ((unsigned)&etext ^ KERNBASE) >> VAX_PGSHIFT; i++)
		Sysmap[i].pg_prot = PROT_URKW;

	/* Map System Page Table and zero it,  Sysmap already set. */
	mtpr((unsigned)Sysmap - KERNBASE, PR_SBR);

	/* Map Interrupt stack and set red zone */
	istack = (unsigned)Sysmap + round_page(sysptsize * 4);
	mtpr(istack + ISTACK_SIZE, PR_ISP);
	kvtopte(istack)->pg_v = 0;

	/* Some scratch pages */
	scratch = istack + ISTACK_SIZE;

	/* Physical-to-virtual translation table */
	pv_table = (struct pv_entry *)(scratch + 4 * VAX_NBPG);

	avail_start = (vaddr_t)pv_table + (round_page(avail_end >> PGSHIFT)) *
	    sizeof(struct pv_entry) - KERNBASE;

	/* Kernel message buffer */
	avail_end -= MSGBUFSIZE;
	msgbufaddr = (void *)(avail_end + KERNBASE);

	/* zero all mapped physical memory from Sysmap to here */
	memset((void *)istack, 0, (avail_start + KERNBASE) - istack);

	/* QDSS console mapping hack */
#if NQD > 0
	qdearly();
#endif

	/* User page table map. This is big. */
	MAPVIRT(ptemapstart, vax_btoc(usrptsize * sizeof(struct pte)));
	ptemapend = virtual_avail;

	MAPVIRT(iospace, IOSPSZ); /* Device iospace mapping area */

	/* Init SCB and set up stray vectors. */
	avail_start = scb_init(avail_start);
	bcopy((caddr_t)proc0paddr + REDZONEADDR, 0, sizeof(struct rpb));

	if (dep_call->cpu_steal_pages)
		(*dep_call->cpu_steal_pages)();

	avail_start = round_page(avail_start);
	virtual_avail = round_page(virtual_avail);
	virtual_end = trunc_page(virtual_end);


#if 0 /* Breaks cninit() on some machines */
	cninit();
	printf("Sysmap %p, istack %lx, scratch %lx\n",Sysmap,istack,scratch);
	printf("etext %p, kvmsize %lx\n", &etext, kvmsize);
	printf("SYSPTSIZE %x PROCPTSIZE %x usrptsize %lx\n",
	    sysptsize, PROCPTSIZE, usrptsize * sizeof(struct pte));
	printf("pv_table %p, ptemapstart %lx ptemapend %lx\n",
	    pv_table, ptemapstart, ptemapend);
	printf("avail_start %lx, avail_end %lx\n",avail_start,avail_end);
	printf("virtual_avail %lx,virtual_end %lx\n",
	    virtual_avail, virtual_end);
	printf("startpmapdebug %p\n",&startpmapdebug);
#endif


	/* Init kernel pmap */
	pmap->pm_p1br = (struct pte *)KERNBASE;
	pmap->pm_p0br = (struct pte *)KERNBASE;
	pmap->pm_p1lr = NPTEPERREG;
	pmap->pm_p0lr = 0;
	pmap->pm_stats.wired_count = pmap->pm_stats.resident_count = 0;
	    /* btop(virtual_avail - KERNBASE); */

	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);

	/* Activate the kernel pmap. */
	mtpr(pcb->P1BR = pmap->pm_p1br, PR_P1BR);
	mtpr(pcb->P0BR = pmap->pm_p0br, PR_P0BR);
	mtpr(pcb->P1LR = pmap->pm_p1lr, PR_P1LR);
	mtpr(pcb->P0LR = (pmap->pm_p0lr|AST_PCB), PR_P0LR);

	/* cpu_info struct */
	pcb->SSP = scratch + VAX_NBPG;
	mtpr(pcb->SSP, PR_SSP);
	bzero((caddr_t)pcb->SSP,
	    sizeof(struct cpu_info) + sizeof(struct device));
	curcpu()->ci_exit = scratch;
	curcpu()->ci_dev = (void *)(pcb->SSP + sizeof(struct cpu_info));
#if defined(MULTIPROCESSOR)
	curcpu()->ci_flags = CI_MASTERCPU|CI_RUNNING;
#endif
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	simple_lock_init(&pvtable_lock);
#endif

	/*
	 * Now everything should be complete, start virtual memory.
	 */
	uvm_page_physload(avail_start >> PGSHIFT, avail_end >> PGSHIFT,
	    avail_start >> PGSHIFT, avail_end >> PGSHIFT,
	    VM_FREELIST_DEFAULT);
	mtpr(sysptsize, PR_SLR);
	rpb.sbr = mfpr(PR_SBR);
	rpb.slr = mfpr(PR_SLR);
	rpb.wait = 0;	/* DDB signal */
	mtpr(1, PR_MAPEN);
}

/*
 * Define the initial bounds of the kernel virtual address space.
 */
void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{

	*vstartp = virtual_avail;
	*vendp = virtual_end;
}

/*
 * Let the VM system do early memory allocation from the direct-mapped
 * physical memory instead.
 */
vaddr_t
pmap_steal_memory(size, vstartp, vendp)
	vsize_t size;
	vaddr_t *vstartp, *vendp;
{
	vaddr_t v;
	int npgs;

	PMDEBUG(("pmap_steal_memory: size 0x%lx start %p end %p\n",
		    size, vstartp, vendp));

	size = round_page(size);
	npgs = btoc(size);

#ifdef DIAGNOSTIC
	if (uvm.page_init_done == TRUE)
		panic("pmap_steal_memory: called _after_ bootstrap");
#endif

	/*
	 * A vax only have one segment of memory.
	 */

	v = (vm_physmem[0].avail_start << PGSHIFT) | KERNBASE;
	vm_physmem[0].avail_start += npgs;
	vm_physmem[0].start += npgs;
	bzero((caddr_t)v, size);
	return v;
}

/*
 * pmap_init() is called as part of vm init after memory management
 * is enabled. It is meant to do machine-specific allocations.
 * Here is the resource map for the user page tables inited.
 */
void 
pmap_init() 
{
	/*
	 * Create the extent map used to manage the page table space.
	 * XXX - M_HTABLE is bogus.
	 */
	ptemap = extent_create("ptemap", ptemapstart, ptemapend,
	    M_HTABLE, ptmapstorage, PTMAPSZ, EX_NOCOALESCE);
	if (ptemap == NULL)
		panic("pmap_init");
}

static u_long
pmap_extwrap(vsize_t nsize)
{
	int res;
	u_long rv;

	for (;;) {
		res = extent_alloc(ptemap, nsize, PAGE_SIZE, 0,
		    EX_WAITOK|EX_MALLOCOK, &rv);
		if (res == EAGAIN)
			return 0;
		if (res == 0)
			return rv;
	}
}

/*
 * Do a page removal from the pv table. A page is identified by its
 * virtual address combined with its struct pmap in the pv table.
 */
static void
rmpage(pmap_t pm, int *br)
{
	struct pv_entry *pv, *pl, *pf;
	vaddr_t vaddr;
	int found = 0;

	if (pm == pmap_kernel())
		vaddr = (br - (int *)Sysmap) * VAX_NBPG + 0x80000000;
	else if ((br >= (int *)pm->pm_p0br) &&
	    (br < ((int *)pm->pm_p0br + pm->pm_p0lr)))
		vaddr = (br - (int *)pm->pm_p0br) * VAX_NBPG;
	else
		vaddr = (br - (int *)pm->pm_p1br) * VAX_NBPG + 0x40000000;

	pv = pv_table + ((br[0] & PG_FRAME) >> LTOHPS);
	if (((br[0] & PG_PROT) == PG_RW) && 
	    ((pv->pv_attr & PG_M) != PG_M))
		pv->pv_attr |= br[0]|br[1]|br[2]|br[3]|br[4]|br[5]|br[6]|br[7];
	simple_lock(&pm->pm_lock);
	pm->pm_stats.resident_count--;
	if (br[0] & PG_W)
		pm->pm_stats.wired_count--;
	simple_unlock(&pm->pm_lock);
	if (pv->pv_pmap == pm && pv->pv_vaddr == vaddr) {
		pv->pv_vaddr = NOVADDR;
		pv->pv_pmap = 0;
		found++;
	} else
		for (pl = pv; pl->pv_next; pl = pl->pv_next) {
			if (pl->pv_next->pv_pmap != pm ||
			    pl->pv_next->pv_vaddr != vaddr)
				continue;
			pf = pl->pv_next;
			pl->pv_next = pl->pv_next->pv_next;
			free_pventry(pf);
			found++;
			break;
		}
	if (found == 0)
		panic("rmpage: pm %p br %p", pm, br);
}
/*
 * Update the PCBs using this pmap after a change.
 */
static void
update_pcbs(struct pmap *pm)
{
	struct pm_share *ps;

	ps = pm->pm_share;
	while (ps != NULL) {
		ps->ps_pcb->P0BR = pm->pm_p0br;
		ps->ps_pcb->P0LR = pm->pm_p0lr|AST_PCB;
		ps->ps_pcb->P1BR = pm->pm_p1br;
		ps->ps_pcb->P1LR = pm->pm_p1lr;
		ps = ps->ps_next;
	}

	/* If curproc uses this pmap update the regs too */ 
	if (pm == curproc->p_vmspace->vm_map.pmap) {
		mtpr(pm->pm_p0br, PR_P0BR);
		mtpr(pm->pm_p0lr|AST_PCB, PR_P0LR);
		mtpr(pm->pm_p1br, PR_P1BR);
		mtpr(pm->pm_p1lr, PR_P1LR);
	}
#if defined(MULTIPROCESSOR) && defined(notyet)
	/* If someone else is using this pmap, be sure to reread */
	cpu_send_ipi(IPI_DEST_ALL, IPI_NEWPTE);
#endif
}

/*
 * Allocate a page through direct-mapped segment.
 */
static vaddr_t
getpage(int w)
{
	struct vm_page *pg;

	for (;;) {
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg != NULL)
			break;
		if (w == NOWAIT)
			return 0;
		uvm_wait("getpage");
	}
	return (VM_PAGE_TO_PHYS(pg)|KERNBASE);
}

/*
 * Free the page allocated above.
 */
static void
freepage(vaddr_t v)
{
	paddr_t paddr = (kvtopte(v)->pg_pfn << VAX_PGSHIFT);
	uvm_pagefree(PHYS_TO_VM_PAGE(paddr));
}

/*
 * Remove a full process space. Update all processes pcbs.
 */
static void
rmspace(struct pmap *pm)
{
	int lr, i, j, *br;

	if (pm->pm_p0lr == 0 && pm->pm_p1lr == NPTEPERREG)
		return; /* Already free */

	lr = pm->pm_p0lr/1024;
	for (i = 0; i < lr; i++) {
		if (pm->pm_pref[i] == 0)
			continue;
		br = (int *)&pm->pm_p0br[i*1024];
		for (j = 0; j < 1024; j+=8) {
			if (br[j] == 0)
				continue;
			rmpage(pm, &br[j]);
			if (--pm->pm_pref[i] == 0) {
				freepage((vaddr_t)br);
				break;
			}
		}
	}
	lr = pm->pm_p1lr/1024;
	for (i = lr; i < 0x800; i++) {
		if (pm->pm_pref[i+0x800] == 0)
			continue;
		br = (int *)&pm->pm_p1br[i*1024];
		for (j = 0; j < 1024; j+=8) {
			if (br[j] == 0)
				continue;
			rmpage(pm, &br[j]);
			if (--pm->pm_pref[i+0x800] == 0) {
				freepage((vaddr_t)br);
				break;
			}
		}
	}

	if (pm->pm_p0lr != 0)
		extent_free(ptemap, (u_long)pm->pm_p0br,
		    pm->pm_p0lr * PPTESZ, EX_WAITOK);
	if (pm->pm_p1lr != NPTEPERREG)
		extent_free(ptemap, (u_long)pm->pm_p1ap,
		    (NPTEPERREG - pm->pm_p1lr) * PPTESZ, EX_WAITOK);
	pm->pm_p0br = pm->pm_p1br = (struct pte *)KERNBASE;
	pm->pm_p0lr = 0;
	pm->pm_p1lr = NPTEPERREG;
	pm->pm_p1ap = NULL;
	update_pcbs(pm);
}

/*
 * Find a process to remove the process space for.
 * This is based on uvm_swapout_threads().
 * Avoid to remove ourselves.
 */

#define swappable(p, pm)						\
	(((p)->p_flag & (P_SYSTEM | P_INMEM | P_WEXIT)) == P_INMEM &&	\
	((p)->p_holdcnt == 0) && ((p)->p_vmspace->vm_map.pmap != pm))

static int
pmap_rmproc(struct pmap *pm)
{
	struct pmap *ppm;
	struct proc *p;
	struct proc *outp, *outp2;
	int outpri, outpri2;
	int didswap = 0;
	extern int maxslp;

	outp = outp2 = NULL;
	outpri = outpri2 = 0;
	proclist_lock_read();
	LIST_FOREACH(p, &allproc, p_list) {
		if (!swappable(p, pm))
			continue;
		ppm = p->p_vmspace->vm_map.pmap;
		if (ppm->pm_p0lr == 0 && ppm->pm_p1lr == NPTEPERREG)
			continue; /* Already swapped */
		switch (p->p_stat) {
		case SRUN:
		case SONPROC:
			if (p->p_swtime > outpri2) {
				outp2 = p;
				outpri2 = p->p_swtime;
			}
			continue;
		case SSLEEP:
		case SSTOP:
			if (p->p_slptime >= maxslp) {
				rmspace(p->p_vmspace->vm_map.pmap);
				didswap++;
			} else if (p->p_slptime > outpri) {
				outp = p;
				outpri = p->p_slptime;
			}
			continue;
		}
	}
	proclist_unlock_read();
	if (didswap == 0) {
		if ((p = outp) == NULL)
			p = outp2;
		if (p) {
			rmspace(p->p_vmspace->vm_map.pmap);
			didswap++;
		}
	}
	return didswap;
}

/*
 * Allocate space for user page tables, from ptemap.
 * This routine should never fail; use the same algorithm as when processes
 * are swapped.
 * Argument is needed space, in bytes.
 * Returns a pointer to the newly allocated space.
 */
static vaddr_t
pmap_getusrptes(pmap_t pm, vsize_t nsize)
{
	u_long rv;

#ifdef DEBUG
	if (nsize & PAGE_MASK)
		panic("pmap_getusrptes: bad size %lx", nsize);
#endif
	while (((rv = pmap_extwrap(nsize)) == 0) && (pmap_rmproc(pm) != 0))
		;
	if (rv)
		return rv;
	panic("usrptmap space leakage");
}

/*
 * Remove a pte page when all references are gone.
 */
static void
rmptep(struct pte *pte)
{
	pte = (struct pte *)trunc_page((vaddr_t)pte);
#ifdef DEBUG
	{	int i, *ptr = (int *)pte;
		for (i = 0; i < 1024; i++)
			if (ptr[i] != 0)
				panic("rmptep: ptr[%d] != 0", i);
	}
#endif
	freepage((vaddr_t)pte);
	bzero(kvtopte(pte), sizeof(struct pte) * LTOHPN);
}

static void 
grow_p0(struct pmap *pm, int reqlen)
{
	vaddr_t nptespc;
	char *from, *to;
	int srclen, dstlen;
	int inuse, len, p0lr;
	u_long p0br;
 
	PMDEBUG(("grow_p0: pmap %p reqlen %d\n", pm, reqlen));

	/* Get new pte space */
	p0lr = pm->pm_p0lr;
	inuse = p0lr != 0;
	len = round_page((reqlen+1) * PPTESZ);
	RECURSEEND;
	nptespc = pmap_getusrptes(pm, len);
	RECURSESTART;
 
	/*
	 * Copy the old ptes to the new space.
	 * Done by moving on system page table.
	 */
	srclen = vax_btop(p0lr * PPTESZ) * PPTESZ;
	dstlen = vax_btoc(len)*PPTESZ;
	from = (char *)kvtopte(pm->pm_p0br);
	to = (char *)kvtopte(nptespc);

	PMDEBUG(("grow_p0: from %p to %p src %d dst %d\n",
	    from, to, srclen, dstlen));

	if (inuse)
		bcopy(from, to, srclen);
	bzero(to+srclen, dstlen-srclen);
	p0br = (u_long)pm->pm_p0br;
	pm->pm_p0br = (struct pte *)nptespc;
	pm->pm_p0lr = (len/PPTESZ);
	update_pcbs(pm);

	/* Remove the old after update_pcbs() (for multicpu propagation) */
	if (inuse)
		extent_free(ptemap, p0br, p0lr*PPTESZ, EX_WAITOK);
}


static void
grow_p1(struct pmap *pm, int len)
{
	vaddr_t nptespc, optespc;
	int nlen, olen;
 
	PMDEBUG(("grow_p1: pm %p len %x\n", pm, len));

	/* Get new pte space */
	nlen = 0x800000 - trunc_page(len * PPTESZ);
	RECURSEEND;
	nptespc = pmap_getusrptes(pm, nlen);
	RECURSESTART;
	olen = 0x800000 - (pm->pm_p1lr * PPTESZ);
	optespc = (vaddr_t)pm->pm_p1ap;

	/*
	 * Copy the old ptes to the new space.
	 * Done by moving on system page table.
	 */
	bzero(kvtopte(nptespc), vax_btop(nlen-olen) * PPTESZ);
	if (optespc)
		bcopy(kvtopte(optespc), kvtopte(nptespc+nlen-olen),
		    vax_btop(olen));

	pm->pm_p1ap = (struct pte *)nptespc;
	pm->pm_p1br = (struct pte *)(nptespc+nlen-0x800000);
	pm->pm_p1lr = NPTEPERREG - nlen/PPTESZ;
	update_pcbs(pm);

	if (optespc)
		extent_free(ptemap, optespc, olen, EX_WAITOK);
}

/*
 * Initialize a preallocated an zeroed pmap structure,
 */
static void
pmap_pinit(pmap_t pmap)
{

	/*
	 * Do not allocate any pte's here, we don't know the size and 
	 * we'll get a page pault anyway when some page is referenced,
	 * so do it then.
	 */
	pmap->pm_p0br = (struct pte *)KERNBASE;
	pmap->pm_p1br = (struct pte *)KERNBASE;
	pmap->pm_p0lr = 0;
	pmap->pm_p1lr = NPTEPERREG;
	pmap->pm_p1ap = NULL;

	PMDEBUG(("pmap_pinit(%p): p0br=%p p0lr=0x%lx p1br=%p p1lr=0x%lx\n",
	    pmap, pmap->pm_p0br, pmap->pm_p0lr, pmap->pm_p1br, pmap->pm_p1lr));

	pmap->pm_pref = (u_char *)getpage(WAITOK);
	pmap->pm_count = 1;
	pmap->pm_stats.resident_count = pmap->pm_stats.wired_count = 0;
}

/*
 * pmap_create() creates a pmap for a new task.
 * If not already allocated, malloc space for one.
 */
struct pmap * 
pmap_create()
{
	struct pmap *pmap;

	MALLOC(pmap, struct pmap *, sizeof(*pmap), M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(struct pmap));
	pmap_pinit(pmap);
	simple_lock_init(&pmap->pm_lock);
	return (pmap);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
static void
pmap_release(struct pmap *pmap)
{
#ifdef DEBUG
	vaddr_t saddr, eaddr;
	int i;
#endif

	PMDEBUG(("pmap_release: pmap %p\n",pmap));

	if (pmap->pm_p0br == 0)
		return;

#ifdef DEBUG
	for (i = 0; i < NPTEPROCSPC; i++)
		if (pmap->pm_pref[i])
			panic("pmap_release: refcnt %d index %d", 
			    pmap->pm_pref[i], i);

	saddr = (vaddr_t)pmap->pm_p0br;
	eaddr = saddr + pmap->pm_p0lr * PPTESZ;
	for (; saddr < eaddr; saddr += NBPG)
		if (kvtopte(saddr)->pg_pfn)
			panic("pmap_release: P0 page mapped");
	saddr = (vaddr_t)pmap->pm_p1br + pmap->pm_p1lr * PPTESZ;
	eaddr = KERNBASE;
	for (; saddr < eaddr; saddr += NBPG)
		if (kvtopte(saddr)->pg_pfn)
			panic("pmap_release: P1 page mapped");
#endif
	freepage((vaddr_t)pmap->pm_pref);

	if (pmap->pm_p0lr != 0)
		extent_free(ptemap, (u_long)pmap->pm_p0br,
		    pmap->pm_p0lr * PPTESZ, EX_WAITOK);
	if (pmap->pm_p1lr != NPTEPERREG)
		extent_free(ptemap, (u_long)pmap->pm_p1ap,
		    (NPTEPERREG - pmap->pm_p1lr) * PPTESZ, EX_WAITOK);
}

/*
 * pmap_destroy(pmap): Remove a reference from the pmap. 
 * If the pmap is NULL then just return else decrese pm_count.
 * If this was the last reference we call's pmap_relaese to release this pmap.
 * OBS! remember to set pm_lock
 */

void
pmap_destroy(pmap_t pmap)
{
	int count;
  
	PMDEBUG(("pmap_destroy: pmap %p\n",pmap));

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
  
	if (count == 0) {
#ifdef DEBUG
		if (pmap->pm_share)
			panic("pmap_destroy used pmap");
#endif
		pmap_release(pmap);
		FREE(pmap, M_VMPMAP);
	}
}

static struct pte *
vaddrtopte(struct pv_entry *pv)
{
	struct pmap *pm;
	if (pv->pv_pmap == NULL || pv->pv_vaddr == NOVADDR)
		return NULL;
	if (pv->pv_vaddr & KERNBASE)
		return &Sysmap[(pv->pv_vaddr & ~KERNBASE) >> VAX_PGSHIFT];
	pm = pv->pv_pmap;
	if (pv->pv_vaddr & 0x40000000)
		return &pm->pm_p1br[vax_btop(pv->pv_vaddr & ~0x40000000)];
	else
		return &pm->pm_p0br[vax_btop(pv->pv_vaddr)];
}

/*
 * New (real nice!) function that allocates memory in kernel space
 * without tracking it in the MD code.
 */
void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	int *ptp, opte;

	ptp = (int *)kvtopte(va);
	PMDEBUG(("pmap_kenter_pa: va: %lx, pa %lx, prot %x ptp %p\n",
	    va, pa, prot, ptp));
	opte = ptp[0];
	ptp[0] = PG_V | ((prot & VM_PROT_WRITE)? PG_KW : PG_KR) |
	    PG_PFNUM(pa) | PG_SREF;
	ptp[1] = ptp[0] + 1;
	ptp[2] = ptp[0] + 2;
	ptp[3] = ptp[0] + 3;
	ptp[4] = ptp[0] + 4;
	ptp[5] = ptp[0] + 5;
	ptp[6] = ptp[0] + 6;
	ptp[7] = ptp[0] + 7;
	if (opte & PG_V) {
#if defined(MULTIPROCESSOR)
		cpu_send_ipi(IPI_DEST_ALL, IPI_TBIA);
#endif
		mtpr(0, PR_TBIA);
	}
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	struct pte *pte;
#ifdef PMAPDEBUG
	int i;
#endif

	PMDEBUG(("pmap_kremove: va: %lx, len %lx, ptp %p\n",
		    va, len, kvtopte(va)));

	pte = kvtopte(va);

#ifdef PMAPDEBUG
	/*
	 * Check if any pages are on the pv list.
	 * This shouldn't happen anymore.
	 */
	len >>= PGSHIFT;
	for (i = 0; i < len; i++) {
		if (pte->pg_pfn == 0)
			continue;
		if (pte->pg_sref == 0)
			panic("pmap_kremove");
		bzero(pte, LTOHPN * sizeof(struct pte));
		pte += LTOHPN;
	}
#else
	len >>= VAX_PGSHIFT;
	bzero(pte, len * sizeof(struct pte));
#endif
#if defined(MULTIPROCESSOR)
	cpu_send_ipi(IPI_DEST_ALL, IPI_TBIA);
#endif
	mtpr(0, PR_TBIA);
}

/*
 * pmap_enter() is the main routine that puts in mappings for pages, or
 * upgrades mappings to more "rights".
 */
int
pmap_enter(pmap, v, p, prot, flags)
	pmap_t	pmap;
	vaddr_t v;
	paddr_t p;
	vm_prot_t prot;
	int flags;
{
	struct pv_entry *pv, *tmp;
	int i, s, newpte, oldpte, *patch, idx = 0; /* XXX gcc */
#ifdef PMAPDEBUG
	boolean_t wired = (flags & PMAP_WIRED) != 0;
#endif

	PMDEBUG(("pmap_enter: pmap %p v %lx p %lx prot %x wired %d access %x\n",
	    pmap, v, p, prot, wired, flags & VM_PROT_ALL));

	RECURSESTART;
	/* Find address of correct pte */
	if (v & KERNBASE) {
		patch = (int *)Sysmap;
		i = (v - KERNBASE) >> VAX_PGSHIFT;
		newpte = (p>>VAX_PGSHIFT)|(prot&VM_PROT_WRITE?PG_KW:PG_KR);
	} else {
#ifdef DIAGNOSTIC
		if (pmap == pmap_kernel())
			panic("pmap_enter in userspace");
#endif
		if (v < 0x40000000) {
			i = vax_btop(v);
			if (i >= pmap->pm_p0lr)
				grow_p0(pmap, i);
			patch = (int *)pmap->pm_p0br;
		} else {
			i = vax_btop(v - 0x40000000);
			if (i < pmap->pm_p1lr)
				grow_p1(pmap, i);
			patch = (int *)pmap->pm_p1br;
		}
		newpte = (p >> VAX_PGSHIFT) |
		    (prot & VM_PROT_WRITE ? PG_RW : PG_RO);

		/*
		 * Check if a pte page must be mapped in.
		 */
		if (pmap->pm_pref == NULL)
			panic("pmap->pm_pref");
		idx = (v >> PGSHIFT)/NPTEPG;

		if (pmap->pm_pref[idx] == 0) {
			vaddr_t ptaddr = trunc_page((vaddr_t)&patch[i]);
			paddr_t phys;
#ifdef DEBUG
			if (kvtopte(&patch[i])->pg_pfn)
				panic("pmap_enter: refcnt == 0");
#endif
			/*
			 * It seems to be legal to sleep here to wait for
			 * pages; at least some other ports do so.
			 */
			phys = getpage(NOWAIT);
			if ((phys == 0) && (flags & PMAP_CANFAIL)) {
				RECURSEEND;
				return ENOMEM;
			}
			pmap_kenter_pa(ptaddr,phys,VM_PROT_READ|VM_PROT_WRITE);
			pmap_update(pmap_kernel());
		}
	}
	/*
	 * Do not keep track of anything if mapping IO space.
	 */
	if (IOSPACE(p)) {
		patch[i] = newpte;
		patch[i+1] = newpte+1;
		patch[i+2] = newpte+2;
		patch[i+3] = newpte+3;
		patch[i+4] = newpte+4;
		patch[i+5] = newpte+5;
		patch[i+6] = newpte+6;
		patch[i+7] = newpte+7;
		if (pmap != pmap_kernel())
			pmap->pm_pref[idx]++; /* New mapping */
		RECURSEEND;
		return 0;
	}

	if (flags & PMAP_WIRED)
		newpte |= PG_W;

	oldpte = patch[i] & ~(PG_V|PG_M);
	pv = pv_table + (p >> PGSHIFT);

	/* just a wiring change? */
	if (newpte == (oldpte | PG_W)) {
		patch[i] |= PG_W;
		pmap->pm_stats.wired_count++;
		RECURSEEND;
		return 0;
	}

	/* mapping unchanged? just return. */
	if (newpte == oldpte) {
		RECURSEEND;
		return 0;
	}

	/* Changing mapping? */
	
	if ((newpte & PG_FRAME) == (oldpte & PG_FRAME)) {
		/* prot change. resident_count will be increased later */
		pmap->pm_stats.resident_count--;
		if (oldpte & PG_W) {
			pmap->pm_stats.wired_count--;
		}
	} else {

		/*
		 * Mapped before? Remove it then.
		 */

		if (oldpte & PG_FRAME) {
			RECURSEEND;
			if ((oldpte & PG_SREF) == 0)
				rmpage(pmap, &patch[i]);
			else
				panic("pmap_enter on PG_SREF page");
			RECURSESTART;
		} else if (pmap != pmap_kernel())
				pmap->pm_pref[idx]++; /* New mapping */

		s = splvm();
		PVTABLE_LOCK;
		if (pv->pv_pmap == NULL) {
			pv->pv_vaddr = v;
			pv->pv_pmap = pmap;
		} else {
			tmp = get_pventry();
			tmp->pv_vaddr = v;
			tmp->pv_pmap = pmap;
			tmp->pv_next = pv->pv_next;
			pv->pv_next = tmp;
		}
		PVTABLE_UNLOCK;
		splx(s);
	}
	pmap->pm_stats.resident_count++;
	if (flags & PMAP_WIRED) {
		pmap->pm_stats.wired_count++;
	}

	PVTABLE_LOCK;
	if (flags & (VM_PROT_READ|VM_PROT_WRITE)) {
		pv->pv_attr |= PG_V;
		newpte |= PG_V;
	}
	if (flags & VM_PROT_WRITE)
		pv->pv_attr |= PG_M;
	PVTABLE_UNLOCK;

	if (flags & PMAP_WIRED)
		newpte |= PG_V; /* Not allowed to be invalid */

	patch[i] = newpte;
	patch[i+1] = newpte+1;
	patch[i+2] = newpte+2;
	patch[i+3] = newpte+3;
	patch[i+4] = newpte+4;
	patch[i+5] = newpte+5;
	patch[i+6] = newpte+6;
	patch[i+7] = newpte+7;
	RECURSEEND;
#ifdef DEBUG
	if (pmap != pmap_kernel())
		if (pmap->pm_pref[idx] > VAX_NBPG/sizeof(struct pte))
			panic("pmap_enter: refcnt %d", pmap->pm_pref[idx]);
#endif
	if (pventries < 10)
		more_pventries();

	mtpr(0, PR_TBIA); /* Always; safety belt */
	return 0;
}

vaddr_t
pmap_map(virtuell, pstart, pend, prot)
	vaddr_t virtuell;
	paddr_t pstart, pend;
	int prot;
{
	vaddr_t count;
	int *pentry;

	PMDEBUG(("pmap_map: virt %lx, pstart %lx, pend %lx, Sysmap %p\n",
	    virtuell, pstart, pend, Sysmap));

	pstart=(uint)pstart &0x7fffffff;
	pend=(uint)pend &0x7fffffff;
	virtuell=(uint)virtuell &0x7fffffff;
	(uint)pentry= (((uint)(virtuell)>>VAX_PGSHIFT)*4)+(uint)Sysmap;
	for(count=pstart;count<pend;count+=VAX_NBPG){
		*pentry++ = (count>>VAX_PGSHIFT)|PG_V|
		    (prot & VM_PROT_WRITE ? PG_KW : PG_KR);
	}
	return(virtuell+(count-pstart)+0x80000000);
}

#if 0
boolean_t 
pmap_extract(pmap, va, pap)
	pmap_t pmap;
	vaddr_t va;
	paddr_t *pap;
{
	paddr_t pa = 0;
	int	*pte, sva;

	PMDEBUG(("pmap_extract: pmap %p, va %lx\n",pmap, va));

	if (va & KERNBASE) {
		pa = kvtophys(va); /* Is 0 if not mapped */
		if (pap)
			*pap = pa;
		if (pa)
			return (TRUE);
		return (FALSE);
	}

	sva = PG_PFNUM(va);
	if (va < 0x40000000) {
		if (sva > pmap->pm_p0lr)
			return FALSE;
		pte = (int *)pmap->pm_p0br;
	} else {
		if (sva < pmap->pm_p1lr)
			return FALSE;
		pte = (int *)pmap->pm_p1br;
	}
	if (kvtopte(&pte[sva])->pg_pfn) {
		if (pap)
			*pap = (pte[sva] & PG_FRAME) << VAX_PGSHIFT;
		return (TRUE);
	}
	return (FALSE);
}
#endif
/*
 * Sets protection for a given region to prot. If prot == none then
 * unmap region. pmap_remove is implemented as pmap_protect with
 * protection none.
 */
void
pmap_protect_long(pmap_t pmap, vaddr_t start, vaddr_t end, vm_prot_t prot)
{
	struct	pte *pt, *pts, *ptd;
	int	pr, off, idx;

	PMDEBUG(("pmap_protect: pmap %p, start %lx, end %lx, prot %x\n",
	    pmap, start, end,prot));

	RECURSESTART;
	if (start & KERNBASE) { /* System space */
		pt = Sysmap;
#ifdef DIAGNOSTIC
		if (((end & 0x3fffffff) >> VAX_PGSHIFT) > mfpr(PR_SLR))
			panic("pmap_protect: outside SLR: %lx", end);
#endif
		start &= ~KERNBASE;
		end &= ~KERNBASE;
		pr = (prot & VM_PROT_WRITE ? PROT_KW : PROT_KR);
	} else {
		if (start & 0x40000000) { /* P1 space */
			if (vax_btop(end - 0x40000000) <= pmap->pm_p1lr) {
				RECURSEEND;
				return;
			}
			if (vax_btop(start - 0x40000000) < pmap->pm_p1lr)
				start = pmap->pm_p1lr * VAX_NBPG;
			pt = pmap->pm_p1br;
			start &= 0x3fffffff;
			end = (end == KERNBASE ? end >> 1 : end & 0x3fffffff);
			off = 0x800;
		} else { /* P0 space */
			int lr = pmap->pm_p0lr;

			/* Anything to care about at all? */
			if (vax_btop(start) > lr) {
				RECURSEEND;
				return;
			}
			if (vax_btop(end) > lr)
				end = lr * VAX_NBPG;
			pt = pmap->pm_p0br;
			off = 0;
		}
		pr = (prot & VM_PROT_WRITE ? PROT_RW : PROT_RO);
	}
	pts = &pt[start >> VAX_PGSHIFT];
	ptd = &pt[end >> VAX_PGSHIFT];
#ifdef DEBUG
	if (((int)pts - (int)pt) & 7)
		panic("pmap_remove: pts not even");
	if (((int)ptd - (int)pt) & 7)
		panic("pmap_remove: ptd not even");
#endif

	while (pts < ptd) {
		if (kvtopte(pts)->pg_pfn && *(int *)pts) {
			if (prot == VM_PROT_NONE) {
				RECURSEEND;
				if ((*(int *)pts & PG_SREF) == 0)
					rmpage(pmap, (u_int *)pts);
#ifdef DEBUG
				else
					panic("pmap_remove PG_SREF page");
#endif
				RECURSESTART;
				bzero(pts, sizeof(struct pte) * LTOHPN);
				if (pt != Sysmap) {
					idx = ((pts - pt) >> LTOHPS)/NPTEPG + off;
					if (--pmap->pm_pref[idx] == 0)
						rmptep(pts);
				}
			} else {
				pts[0].pg_prot = pr;
				pts[1].pg_prot = pr;
				pts[2].pg_prot = pr;
				pts[3].pg_prot = pr;
				pts[4].pg_prot = pr;
				pts[5].pg_prot = pr;
				pts[6].pg_prot = pr;
				pts[7].pg_prot = pr;
			}
		}
		pts += LTOHPN;
	}
	RECURSEEND;
#ifdef MULTIPROCESSOR
	cpu_send_ipi(IPI_DEST_ALL, IPI_TBIA);
#endif
	mtpr(0, PR_TBIA);
}

int pmap_simulref(int bits, int addr);
/*
 * Called from interrupt vector routines if we get a page invalid fault.
 * Note: the save mask must be or'ed with 0x3f for this function.
 * Returns 0 if normal call, 1 if CVAX bug detected.
 */
int
pmap_simulref(int bits, int addr)
{
	u_int	*pte;
	struct	pv_entry *pv;
	paddr_t pa;

	PMDEBUG(("pmap_simulref: bits %x addr %x\n", bits, addr));

#ifdef DEBUG
	if (bits & 1)
		panic("pte trans len");
#endif
	/* Set addess on logical page boundary */
	addr &= ~PGOFSET;
	/* First decode userspace addr */
	if (addr >= 0) {
		if ((addr << 1) < 0)
			pte = (u_int *)mfpr(PR_P1BR);
		else
			pte = (u_int *)mfpr(PR_P0BR);
		pte += PG_PFNUM(addr);
		if (bits & 2) { /* PTE reference */
			pte = (u_int *)kvtopte(trunc_page((vaddr_t)pte));
			if (pte[0] == 0) /* Check for CVAX bug */
				return 1;	
			pa = (u_int)pte & ~KERNBASE;
		} else
			pa = Sysmap[PG_PFNUM(pte)].pg_pfn << VAX_PGSHIFT;
	} else {
		pte = (u_int *)kvtopte(addr);
		pa = (u_int)pte & ~KERNBASE;
	}
	pte[0] |= PG_V;
	pte[1] |= PG_V;
	pte[2] |= PG_V;
	pte[3] |= PG_V;
	pte[4] |= PG_V;
	pte[5] |= PG_V;
	pte[6] |= PG_V;
	pte[7] |= PG_V;
	if (IOSPACE(pa) == 0) { /* No pv_table fiddling in iospace */
		PVTABLE_LOCK;
		pv = pv_table + (pa >> PGSHIFT);
		pv->pv_attr |= PG_V; /* Referenced */
		if (bits & 4) /* (will be) modified. XXX page tables  */
			pv->pv_attr |= PG_M;
		PVTABLE_UNLOCK;
	}
	return 0;
}

/*
 * Clears valid bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_reference_long(struct pv_entry *pv)
{
	struct pte *pte;
	int ref = 0;

	PMDEBUG(("pmap_clear_reference: pv_entry %p\n", pv));

	RECURSESTART;
	PVTABLE_LOCK;
	if (pv->pv_pmap != NULL) {
		pte = vaddrtopte(pv);
		if (pte->pg_w == 0) {
			pte[0].pg_v = pte[1].pg_v = pte[2].pg_v = 
			pte[3].pg_v = pte[4].pg_v = pte[5].pg_v =
			pte[6].pg_v = pte[7].pg_v = 0;
		}
	}

	while ((pv = pv->pv_next)) {
		pte = vaddrtopte(pv);
		if (pte[0].pg_w == 0) {
			pte[0].pg_v = pte[1].pg_v =
			    pte[2].pg_v = pte[3].pg_v = 
			    pte[4].pg_v = pte[5].pg_v = 
			    pte[6].pg_v = pte[7].pg_v = 0;
		}
	}
	PVTABLE_UNLOCK;
	RECURSEEND;
#ifdef MULTIPROCESSOR
	cpu_send_ipi(IPI_DEST_ALL, IPI_TBIA);
#endif	
	mtpr(0, PR_TBIA);
	return ref;
}

/*
 * Checks if page is modified; returns true or false depending on result.
 */
boolean_t
pmap_is_modified_long(struct pv_entry *pv)
{
	struct pte *pte;

	PMDEBUG(("pmap_is_modified: pv_entry %p ", pv));

	PVTABLE_LOCK;
	if (pv->pv_pmap != NULL) {
		pte = vaddrtopte(pv);
		if ((pte[0].pg_m | pte[1].pg_m | pte[2].pg_m | pte[3].pg_m |
		    pte[4].pg_m | pte[5].pg_m | pte[6].pg_m | pte[7].pg_m)) {
			PMDEBUG(("Yes: (1)\n"));
			PVTABLE_UNLOCK;
			return 1;
		}
	}

	while ((pv = pv->pv_next)) {
		pte = vaddrtopte(pv);
		if ((pte[0].pg_m | pte[1].pg_m | pte[2].pg_m | pte[3].pg_m
		    | pte[4].pg_m | pte[5].pg_m | pte[6].pg_m | pte[7].pg_m)) {
			PMDEBUG(("Yes: (2)\n"));
			PVTABLE_UNLOCK;
			return 1;
		}
	}
	PVTABLE_UNLOCK;
	PMDEBUG(("No\n"));
	return 0;
}

/*
 * Clears modify bit in all ptes referenced to this physical page.
 */
boolean_t
pmap_clear_modify_long(struct pv_entry *pv)
{
	struct pte *pte;
	boolean_t rv = FALSE;

	PMDEBUG(("pmap_clear_modify: pv_entry %p\n", pv));

	PVTABLE_LOCK;
	if (pv->pv_pmap != NULL) {
		pte = vaddrtopte(pv);
		if (pte[0].pg_m | pte[1].pg_m | pte[2].pg_m | pte[3].pg_m |
		    pte[4].pg_m | pte[5].pg_m | pte[6].pg_m | pte[7].pg_m) {
			rv = TRUE;
		}
		pte[0].pg_m = pte[1].pg_m = pte[2].pg_m = pte[3].pg_m = 
		    pte[4].pg_m = pte[5].pg_m = pte[6].pg_m = pte[7].pg_m = 0;
	}

	while ((pv = pv->pv_next)) {
		pte = vaddrtopte(pv);
		if (pte[0].pg_m | pte[1].pg_m | pte[2].pg_m | pte[3].pg_m |
		    pte[4].pg_m | pte[5].pg_m | pte[6].pg_m | pte[7].pg_m) {
			rv = TRUE;
		}
		pte[0].pg_m = pte[1].pg_m = pte[2].pg_m = pte[3].pg_m = 
		    pte[4].pg_m = pte[5].pg_m = pte[6].pg_m = pte[7].pg_m = 0;
	}
	PVTABLE_UNLOCK;
	return rv;
}

/*
 * Lower the permission for all mappings to a given page.
 * Lower permission can only mean setting protection to either read-only
 * or none; where none is unmapping of the page.
 */
void
pmap_page_protect_long(struct pv_entry *pv, vm_prot_t prot)
{
	struct	pte *pt;
	struct	pv_entry *opv, *pl;
	int	s, *g;

	PMDEBUG(("pmap_page_protect: pv %p, prot %x\n", pv, prot));


	if (prot == VM_PROT_ALL) /* 'cannot happen' */
		return;

	RECURSESTART;
	PVTABLE_LOCK;
	if (prot == VM_PROT_NONE) {
		s = splvm();
		g = (int *)vaddrtopte(pv);
		if (g) {
			int idx;

			simple_lock(&pv->pv_pmap->pm_lock);
			pv->pv_pmap->pm_stats.resident_count--;
			if (g[0] & PG_W) {
				pv->pv_pmap->pm_stats.wired_count--;
			}
			simple_unlock(&pv->pv_pmap->pm_lock);
			if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pv->pv_attr |= 
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			bzero(g, sizeof(struct pte) * LTOHPN);
			if (pv->pv_pmap != pmap_kernel()) {
				idx = pv->pv_vaddr/(VAX_NBPG*NPTEPG*LTOHPN);
				if (--pv->pv_pmap->pm_pref[idx] == 0)
					rmptep((void *)g);
			}
			pv->pv_vaddr = NOVADDR;
			pv->pv_pmap = NULL;
		}
		pl = pv->pv_next;
		pv->pv_pmap = 0;
		pv->pv_next = 0;
		while (pl) {
			int idx;

			g = (int *)vaddrtopte(pl);
			simple_lock(&pl->pv_pmap->pm_lock);
			pl->pv_pmap->pm_stats.resident_count--;
			if (g[0] & PG_W) {
				pl->pv_pmap->pm_stats.wired_count--;
			}
			simple_unlock(&pl->pv_pmap->pm_lock);
			if ((pv->pv_attr & (PG_V|PG_M)) != (PG_V|PG_M))
				pv->pv_attr |=
				    g[0]|g[1]|g[2]|g[3]|g[4]|g[5]|g[6]|g[7];
			bzero(g, sizeof(struct pte) * LTOHPN);
			if (pl->pv_pmap != pmap_kernel()) {
				idx = pl->pv_vaddr/(VAX_NBPG*NPTEPG*LTOHPN);
				if (--pl->pv_pmap->pm_pref[idx] == 0)
					rmptep((void *)g);
			}
			opv = pl;
			pl = pl->pv_next;
			free_pventry(opv);
		} 
		splx(s);
	} else { /* read-only */
		do {
			pt = vaddrtopte(pv);
			if (pt == 0)
				continue;
			pt[0].pg_prot = pt[1].pg_prot = 
			    pt[2].pg_prot = pt[3].pg_prot = 
			    pt[4].pg_prot = pt[5].pg_prot = 
			    pt[6].pg_prot = pt[7].pg_prot = 
			    ((vaddr_t)pt < ptemapstart ? PROT_KR : PROT_RO);
		} while ((pv = pv->pv_next));
	}
	PVTABLE_UNLOCK;
	RECURSEEND;
#ifdef MULTIPROCESSOR
	cpu_send_ipi(IPI_DEST_ALL, IPI_TBIA);
#endif
	mtpr(0, PR_TBIA);
}

/*
 * Activate the address space for the specified process.
 * Note that if the process to activate is the current process, then
 * the processor internal registers must also be loaded; otherwise
 * the current process will have wrong pagetables.
 */
void
pmap_activate(struct proc *p)
{
	struct pm_share *ps;
	pmap_t pmap;
	struct pcb *pcb;

	PMDEBUG(("pmap_activate: p %p\n", p));

	pmap = p->p_vmspace->vm_map.pmap;
	pcb = &p->p_addr->u_pcb;

	pcb->P0BR = pmap->pm_p0br;
	pcb->P0LR = pmap->pm_p0lr|AST_PCB;
	pcb->P1BR = pmap->pm_p1br;
	pcb->P1LR = pmap->pm_p1lr;

	ps = (struct pm_share *)get_pventry();
	ps->ps_next = pmap->pm_share;
	pmap->pm_share = ps;
	ps->ps_pcb = pcb;

	if (p == curproc) {
		mtpr(pmap->pm_p0br, PR_P0BR);
		mtpr(pmap->pm_p0lr|AST_PCB, PR_P0LR);
		mtpr(pmap->pm_p1br, PR_P1BR);
		mtpr(pmap->pm_p1lr, PR_P1LR);
		mtpr(0, PR_TBIA);
	}
}

void	
pmap_deactivate(struct proc *p)
{
	struct pm_share *ps, *ops;
	pmap_t pmap;
	struct pcb *pcb;

	PMDEBUG(("pmap_deactivate: p %p\n", p));

	pmap = p->p_vmspace->vm_map.pmap;
	pcb = &p->p_addr->u_pcb;

	ps = pmap->pm_share;
	if (ps->ps_pcb == pcb) {
		pmap->pm_share = ps->ps_next;
		free_pventry((struct pv_entry *)ps);
		return;
	}
	ops = ps;
	ps = ps->ps_next;
	while (ps != NULL) {
		if (ps->ps_pcb == pcb) {
			ops->ps_next = ps->ps_next;
			free_pventry((struct pv_entry *)ps);
			return;
		}
		ops = ps;
		ps = ps->ps_next;
	}
#ifdef DEBUG
	panic("pmap_deactivate: not in list");
#endif
}

/*
 * removes the wired bit from a bunch of PTE's.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t v)
{
	int *pte;

	PMDEBUG(("pmap_unwire: pmap %p v %lx\n", pmap, v));

	RECURSESTART;
	if (v & KERNBASE) {
		pte = (int *)kvtopte(v);
	} else {
		if (v < 0x40000000)
			pte = (int *)&pmap->pm_p0br[PG_PFNUM(v)];
		else
			pte = (int *)&pmap->pm_p1br[PG_PFNUM(v)];
	}
	pte[0] &= ~PG_W;
	RECURSEEND;
	pmap->pm_stats.wired_count--;
}

/*
 * pv_entry functions.
 */
struct pv_entry *pv_list;

/*
 * get_pventry().
 * The pv_table lock must be held before calling this.
 */
struct pv_entry *
get_pventry()
{
	struct pv_entry *tmp;

	if (pventries == 0)
		panic("get_pventry");

	tmp = pv_list;
	pv_list = tmp->pv_next;
	pventries--;
	return tmp;
}

/*
 * free_pventry().
 * The pv_table lock must be held before calling this.
 */
void
free_pventry(pv)
	struct pv_entry *pv;
{
	pv->pv_next = pv_list;
	pv_list = pv;
	pventries++;
}

/*
 * more_pventries().
 * The pv_table lock must _not_ be held before calling this.
 */
void
more_pventries()
{
	struct pv_entry *pv;
	int s, i, count;

	pv = (struct pv_entry *)getpage(NOWAIT);
	if (pv == NULL)
		return;
	count = NBPG/sizeof(struct pv_entry);

	for (i = 0; i < count; i++)
		pv[i].pv_next = &pv[i + 1];

	s = splvm();
	PVTABLE_LOCK;
	pv[count - 1].pv_next = pv_list;
	pv_list = pv;
	pventries += count;
	PVTABLE_UNLOCK;
	splx(s);
}

	
/*
 * Called when a process is about to be swapped, to remove the page tables.
 */
void
cpu_swapout(struct proc *p)
{
	pmap_t pm;

	PMDEBUG(("Swapout pid %d\n", p->p_pid));

	pm = p->p_vmspace->vm_map.pmap;
	rmspace(pm);
	pmap_deactivate(p);
}

/*
 * Kernel stack red zone need to be set when a process is swapped in.
 * Be sure that all pages are valid.
 */
void
cpu_swapin(struct proc *p)
{
	struct pte *pte;
	int i;

	PMDEBUG(("Swapin pid %d\n", p->p_pid));

	pte = kvtopte((vaddr_t)p->p_addr);
	for (i = 0; i < (USPACE/VAX_NBPG); i ++)
		pte[i].pg_v = 1;
	kvtopte((vaddr_t)p->p_addr + REDZONEADDR)->pg_v = 0;
	pmap_activate(p);
}

