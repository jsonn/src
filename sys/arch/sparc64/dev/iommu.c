/*	$NetBSD: iommu.c,v 1.3.2.2 2000/12/08 09:30:33 bouyer Exp $	*/

/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
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
 *	from: NetBSD: sbus.c,v 1.13 1999/05/23 07:24:02 mrg Exp
 *	from: @(#)sbus.c	8.1 (Berkeley) 6/11/93
 */

/*
 * UltraSPARC IOMMU support; used by both the sbus and pci code.
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <sparc64/sparc64/cache.h>
#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#ifdef DEBUG
#define IDB_BUSDMA	0x1
#define IDB_IOMMU	0x2
#define IDB_INFO	0x4
int iommudebug = 0x0;
#define DPRINTF(l, s)   do { if (iommudebug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

static	int iommu_strbuf_flush __P((struct iommu_state *));

/*
 * initialise the UltraSPARC IOMMU (SBUS or PCI):
 *	- allocate and setup the iotsb.
 *	- enable the IOMMU
 *	- initialise the streaming buffers (if they exist)
 *	- create a private DVMA map.
 */
void
iommu_init(name, is, tsbsize)
	char *name;
	struct iommu_state *is;
	int tsbsize;
{
	psize_t size;
	vaddr_t va;
	paddr_t pa;
	vm_page_t m;
	struct pglist mlist;

	/*
	 * Setup the iommu.
	 *
	 * The sun4u iommu is part of the SBUS or PCI controller so we
	 * will deal with it here..
	 *
	 * The IOMMU address space always ends at 0xffffe000, but the starting
	 * address depends on the size of the map.  The map size is 1024 * 2 ^
	 * is->is_tsbsize entries, where each entry is 8 bytes.  The start of
	 * the map can be calculated by (0xffffe000 << (8 + is->is_tsbsize)).
	 *
	 * Note: the stupid IOMMU ignores the high bits of an address, so a
	 * NULL DMA pointer will be translated by the first page of the IOTSB.
	 * To trap bugs we'll skip the first entry in the IOTSB.
	 */
	is->is_cr = (tsbsize << 16) | IOMMUCR_EN;
	is->is_tsbsize = tsbsize;
	is->is_dvmabase = IOTSB_VSTART(is->is_tsbsize) + NBPG;

	/*
	 * Allocate memory for I/O pagetables.  They need to be physically
	 * contiguous.
	 */

	size = NBPG<<(is->is_tsbsize);
	TAILQ_INIT(&mlist);
	if (uvm_pglistalloc((psize_t)size, (paddr_t)0, (paddr_t)-1,
		(paddr_t)NBPG, (paddr_t)0, &mlist, 1, 0) != 0)
		panic("iommu_init: no memory");

	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		panic("iommu_init: no memory");
	is->is_tsb = (int64_t *)va;

	m = TAILQ_FIRST(&mlist);
	is->is_ptsb = VM_PAGE_TO_PHYS(m);

	/* Map the pages */
	for (; m != NULL; m = TAILQ_NEXT(m,pageq)) {
		pa = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, pa | PMAP_NVC,
			VM_PROT_READ|VM_PROT_WRITE,
			VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
		va += NBPG;
	}
	bzero(is->is_tsb, size);

#ifdef DEBUG
	if (iommudebug & IDB_INFO)
	{
		/* Probe the iommu */
		struct iommureg *regs = is->is_iommu;

		printf("iommu regs at: cr=%lx tsb=%lx flush=%lx\n",
		    (u_long)&regs->iommu_cr,
		    (u_long)&regs->iommu_tsb,
		    (u_long)&regs->iommu_flush);
		printf("iommu cr=%llx tsb=%llx\n", (unsigned long long)regs->iommu_cr, (unsigned long long)regs->iommu_tsb);
		printf("TSB base %p phys %llx\n", (void *)is->is_tsb, (unsigned long long)is->is_ptsb);
		delay(1000000); /* 1 s */
	}
#endif

	/*
	 * Initialize streaming buffer, if it is there.
	 */
	if (is->is_sb)
		(void)pmap_extract(pmap_kernel(), (vaddr_t)&is->is_flush,
		    (paddr_t *)&is->is_flushpa);

	/*
	 * now actually start up the IOMMU
	 */
	iommu_reset(is);

	/*
	 * Now all the hardware's working we need to allocate a dvma map.
	 */
	printf("DVMA map: %x to %x\n", 
		(unsigned int)is->is_dvmabase,
		(unsigned int)IOTSB_VEND);
	is->is_dvmamap = extent_create(name,
				       is->is_dvmabase, (u_long)IOTSB_VEND,
				       M_DEVBUF, 0, 0, EX_NOWAIT);
}

/*
 * Streaming buffers don't exist on the UltraSPARC IIi; we should have
 * detected that already and disabled them.  If not, we will notice that
 * they aren't there when the STRBUF_EN bit does not remain.
 */
void
iommu_reset(is)
	struct iommu_state *is;
{

	/* Need to do 64-bit stores */
	bus_space_write_8(is->is_bustag, 
			  (bus_space_handle_t)(u_long)&is->is_iommu->iommu_tsb, 
			  0, is->is_ptsb);
	/* Enable IOMMU in diagnostic mode */
	bus_space_write_8(is->is_bustag, 
			  (bus_space_handle_t)(u_long)&is->is_iommu->iommu_cr, 0, 
			  is->is_cr|IOMMUCR_DE);


	if (!is->is_sb)
		return;

	/* Enable diagnostics mode? */
	bus_space_write_8(is->is_bustag, 
			  (bus_space_handle_t)(u_long)&is->is_sb->strbuf_ctl,
			  0, STRBUF_EN);

	/* No streaming buffers? Disable them */
	if (bus_space_read_8(is->is_bustag,
			     (bus_space_handle_t)(u_long)&is->is_sb->strbuf_ctl, 
			     0) == 0)
		is->is_sb = 0;
}

/*
 * Here are the iommu control routines. 
 */
void
iommu_enter(is, va, pa, flags)
	struct iommu_state *is;
	vaddr_t va;
	int64_t pa;
	int flags;
{
	int64_t tte;

#ifdef DIAGNOSTIC
	if (va < is->is_dvmabase)
		panic("iommu_enter: va %#lx not in DVMA space", va);
#endif

	tte = MAKEIOTTE(pa, !(flags&BUS_DMA_NOWRITE), !(flags&BUS_DMA_NOCACHE), 
			!(flags&BUS_DMA_COHERENT));
	
	/* Is the streamcache flush really needed? */
	if (is->is_sb) {
		bus_space_write_8(is->is_bustag, (bus_space_handle_t)(u_long)
				  &is->is_sb->strbuf_pgflush, 0, va);
		iommu_strbuf_flush(is);
	}
	DPRINTF(IDB_IOMMU, ("Clearing TSB slot %d for va %p\n", 
		       (int)IOTSBSLOT(va,is->is_tsbsize), (void *)(u_long)va));
	is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)] = tte;
	bus_space_write_8(is->is_bustag, (bus_space_handle_t)(u_long)
			  &is->is_iommu->iommu_flush, 0, va);
	DPRINTF(IDB_IOMMU, ("iommu_enter: va %lx pa %lx TSB[%lx]@%p=%lx\n",
		       va, (long)pa, (u_long)IOTSBSLOT(va,is->is_tsbsize), 
		       (void *)(u_long)&is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)],
		       (u_long)tte));
}

/*
 * iommu_remove: removes mappings created by iommu_enter
 *
 * Only demap from IOMMU if flag is set.
 *
 * XXX: this function needs better internal error checking.
 */
void
iommu_remove(is, va, len)
	struct iommu_state *is;
	vaddr_t va;
	size_t len;
{

#ifdef DIAGNOSTIC
	if (va < is->is_dvmabase)
		panic("iommu_remove: va 0x%lx not in DVMA space", (u_long)va);
	if ((long)(va + len) < (long)va)
		panic("iommu_remove: va 0x%lx + len 0x%lx wraps", 
		      (long) va, (long) len);
	if (len & ~0xfffffff) 
		panic("iommu_remove: rediculous len 0x%lx", (u_long)len);
#endif

	va = trunc_page(va);
	DPRINTF(IDB_IOMMU, ("iommu_remove: va %lx TSB[%lx]@%p\n",
	    va, (u_long)IOTSBSLOT(va,is->is_tsbsize), 
	    &is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)]));
	while (len > 0) {
		DPRINTF(IDB_IOMMU, ("iommu_remove: clearing TSB slot %d for va %p size %lx\n", 
		    (int)IOTSBSLOT(va,is->is_tsbsize), (void *)(u_long)va, (u_long)len));
		if (is->is_sb) {
			DPRINTF(IDB_IOMMU, ("iommu_remove: flushing va %p TSB[%lx]@%p=%lx, %lu bytes left\n", 	       
			       (void *)(u_long)va, (long)IOTSBSLOT(va,is->is_tsbsize), 
			       (void *)(u_long)&is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)],
			       (long)(is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)]), 
			       (u_long)len));
			bus_space_write_8(is->is_bustag, (bus_space_handle_t)(u_long)
					  &is->is_sb->strbuf_pgflush, 0, va);
			if (len <= NBPG)
				iommu_strbuf_flush(is);
			DPRINTF(IDB_IOMMU, ("iommu_remove: flushed va %p TSB[%lx]@%p=%lx, %lu bytes left\n", 	       
			       (void *)(u_long)va, (long)IOTSBSLOT(va,is->is_tsbsize), 
			       (void *)(u_long)&is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)],
			       (long)(is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)]), 
			       (u_long)len));
		} else
			membar_sync();	/* XXX */

		if (len <= NBPG)
			len = 0;
		else
			len -= NBPG;

		is->is_tsb[IOTSBSLOT(va,is->is_tsbsize)] = 0;
		bus_space_write_8(is->is_bustag, (bus_space_handle_t)(u_long)
				  &is->is_iommu->iommu_flush, 0, va);
		va += NBPG;
	}
}

static int 
iommu_strbuf_flush(is)
	struct iommu_state *is;
{
	struct timeval cur, flushtimeout;

#define BUMPTIME(t, usec) { \
	register volatile struct timeval *tp = (t); \
	register long us; \
 \
	tp->tv_usec = us = tp->tv_usec + (usec); \
	if (us >= 1000000) { \
		tp->tv_usec = us - 1000000; \
		tp->tv_sec++; \
	} \
}

	if (!is->is_sb)
		return (0);
				
	/*
	 * Streaming buffer flushes:
	 * 
	 *   1 Tell strbuf to flush by storing va to strbuf_pgflush.  If
	 *     we're not on a cache line boundary (64-bits):
	 *   2 Store 0 in flag
	 *   3 Store pointer to flag in flushsync
	 *   4 wait till flushsync becomes 0x1
	 *
	 * If it takes more than .5 sec, something
	 * went wrong.
	 */

	is->is_flush = 0;
	membar_sync();
	bus_space_write_8(is->is_bustag, (bus_space_handle_t)(u_long)
			  &is->is_sb->strbuf_flushsync, 0, is->is_flushpa);
	membar_sync();

	microtime(&flushtimeout); 
	cur = flushtimeout;
	BUMPTIME(&flushtimeout, 500000); /* 1/2 sec */
	
	DPRINTF(IDB_IOMMU, ("iommu_strbuf_flush: flush = %lx at va = %lx pa = %lx now=%lx:%lx until = %lx:%lx\n", 
		       (long)is->is_flush, (long)&is->is_flush, 
		       (long)is->is_flushpa, cur.tv_sec, cur.tv_usec, 
		       flushtimeout.tv_sec, flushtimeout.tv_usec));
	/* Bypass non-coherent D$ */
	while (!ldxa(is->is_flushpa, ASI_PHYS_CACHED) && 
	       ((cur.tv_sec <= flushtimeout.tv_sec) && 
		(cur.tv_usec <= flushtimeout.tv_usec)))
		microtime(&cur);

#ifdef DIAGNOSTIC
	if (!is->is_flush) {
		printf("iommu_strbuf_flush: flush timeout %p at %p\n",
		    (void *)(u_long)is->is_flush, 
		    (void *)(u_long)is->is_flushpa); /* panic? */
#ifdef DDB
		Debugger();
#endif
	}
#endif
	DPRINTF(IDB_IOMMU, ("iommu_strbuf_flush: flushed\n"));
	return (is->is_flush);
}

/*
 * IOMMU DVMA operations, common to SBUS and PCI.
 */
int
iommu_dvmamap_load(t, is, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	int s;
	int err;
	bus_size_t sgsize;
	paddr_t curaddr;
	u_long dvmaddr;
	bus_size_t align, boundary;
	vaddr_t vaddr = (vaddr_t)buf;
	pmap_t pmap;

	if (map->dm_nsegs) {
		/* Already in use?? */
#ifdef DIAGNOSTIC
		printf("iommu_dvmamap_load: map still in use\n");
#endif
		bus_dmamap_unload(t, map);
	}
	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_load(): error %d > %d -- "
		     "map size exceeded!\n", (int)buflen, (int)map->_dm_size));
		return (EINVAL);
	}

	sgsize = round_page(buflen + ((int)vaddr & PGOFSET));

	/*
	 * A boundary presented to bus_dmamem_alloc() takes precedence
	 * over boundary in the map.
	 */
	if ((boundary = (map->dm_segs[0]._ds_boundary)) == 0)
		boundary = map->_dm_boundary;
	align = max(map->dm_segs[0]._ds_align, NBPG);
	s = splhigh();
	err = extent_alloc(is->is_dvmamap, sgsize, align,
	    boundary, EX_NOWAIT|EX_BOUNDZERO, (u_long *)&dvmaddr);
	splx(s);

#ifdef DEBUG
	if (err || (dvmaddr == (bus_addr_t)-1))	
	{ 
		printf("iommu_dvmamap_load(): extent_alloc(%d, %x) failed!\n",
		    (int)sgsize, flags);
		Debugger();
	}		
#endif	
	if (err != 0)
		return (err);

	if (dvmaddr == (bus_addr_t)-1)
		return (ENOMEM);

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	map->dm_segs[0].ds_addr = dvmaddr + (vaddr & PGOFSET);
	map->dm_segs[0].ds_len = buflen;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	dvmaddr = trunc_page(map->dm_segs[0].ds_addr);
	for (; buflen > 0; ) {
		/*
		 * Get the physical address for this page.
		 */
		if (pmap_extract(pmap, (vaddr_t)vaddr, &curaddr) == FALSE) {
			bus_dmamap_unload(t, map);
			return (-1);
		}

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = NBPG - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_load: map %p loading va %p dva %lx at pa %lx\n",
		    map, (void *)vaddr, (long)dvmaddr, (long)(curaddr&~(NBPG-1))));
		iommu_enter(is, trunc_page(dvmaddr), trunc_page(curaddr),
		    flags);
			
		dvmaddr += PAGE_SIZE;
		vaddr += sgsize;
		buflen -= sgsize;
	}
	return (0);
}


void
iommu_dvmamap_unload(t, is, map)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dmamap_t map;
{
	vaddr_t addr;
	size_t len;
	int error, s;
	bus_addr_t dvmaddr;
	bus_size_t sgsize;

	if (map->dm_nsegs != 1)
		panic("iommu_dvmamap_unload: nsegs = %d", map->dm_nsegs);

	addr = trunc_page(map->dm_segs[0].ds_addr);
	len = map->dm_segs[0].ds_len;

	DPRINTF(IDB_BUSDMA,
	    ("iommu_dvmamap_unload: map %p removing va %lx size %lx\n",
	    map, (long)addr, (long)len));
	iommu_remove(is, addr, len);
	dvmaddr = (map->dm_segs[0].ds_addr & ~PGOFSET);
	sgsize = round_page(map->dm_segs[0].ds_len + 
			    ((int)map->dm_segs[0].ds_addr & PGOFSET));

	/* Flush the caches */
	bus_dmamap_unload(t->_parent, map);

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	
	s = splhigh();
	error = extent_free(is->is_dvmamap, dvmaddr, sgsize, EX_NOWAIT);
	splx(s);
	if (error != 0)
		printf("warning: %qd of DVMA space lost\n", (long long)sgsize);
}


int
iommu_dvmamap_load_raw(t, is, map, segs, nsegs, flags, size)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	int flags;
	bus_size_t size;
{
	vm_page_t m;
	int s;
	int err;
	bus_size_t sgsize;
	paddr_t pa;
	bus_size_t boundary, align;
	u_long dvmaddr;
	struct pglist *mlist;
	int pagesz = PAGE_SIZE;

	if (map->dm_nsegs) {
		/* Already in use?? */
#ifdef DIAGNOSTIC
		printf("iommu_dvmamap_load_raw: map still in use\n");
#endif
		bus_dmamap_unload(t, map);
	}
	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;
#ifdef DIAGNOSTIC
	/* XXX - unhelpful since we can't reset these in map_unload() */
	if (segs[0].ds_addr != 0)
		panic("iommu_dvmamap_load_raw: segment already loaded: "
			"addr %#llx, size %#llx",
			(unsigned long long)segs[0].ds_addr,
			(unsigned long long)segs[0].ds_len);
	if (segs[0].ds_len != size)
		panic("iommu_dvmamap_load_raw: segment size changed: "
			"ds_len %#llx size %#llx",
			(unsigned long long)segs[0].ds_len,
			(unsigned long long)size);
#endif
	sgsize = round_page(size);

	/*
	 * A boundary presented to bus_dmamem_alloc() takes precedence
	 * over boundary in the map.
	 */
	if ((boundary = segs[0]._ds_boundary) == 0)
		boundary = map->_dm_boundary;

	align = max(segs[0]._ds_align, NBPG);
	s = splhigh();
	err = extent_alloc(is->is_dvmamap, sgsize, align, boundary, 
	   ((flags & BUS_DMA_NOWAIT) == 0 ? EX_WAITOK : EX_NOWAIT)|EX_BOUNDZERO, 
	    (u_long *)&dvmaddr);
	splx(s);

	if (err != 0)
		return (err);

#ifdef DEBUG
	if (dvmaddr == (bus_addr_t)-1)	
	{ 
		printf("iommu_dvmamap_load_raw(): extent_alloc(%d, %x) failed!\n",
		    (int)sgsize, flags);
		Debugger();
	}		
#endif	
	if (dvmaddr == (bus_addr_t)-1)
		return (ENOMEM);

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = size;
	map->dm_nsegs = 1;
	map->dm_segs[0].ds_addr = dvmaddr;
	map->dm_segs[0].ds_len = size;

	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {
		if (sgsize == 0)
			panic("iommu_dmamap_load_raw: size botch");
		pa = VM_PAGE_TO_PHYS(m);

		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_load_raw: map %p loading va %lx at pa %lx\n",
		    map, (long)dvmaddr, (long)(pa)));
		iommu_enter(is, dvmaddr, pa, flags);
			
		dvmaddr += pagesz;
		sgsize -= pagesz;
	}
	return (0);
}

void
iommu_dvmamap_sync(t, is, map, offset, len, ops)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	vaddr_t va = map->dm_segs[0].ds_addr + offset;

	/*
	 * We only support one DMA segment; supporting more makes this code
         * too unweildy.
	 */

	if (ops & BUS_DMASYNC_PREREAD) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		     "BUS_DMASYNC_PREREAD\n", (void *)(u_long)va, (u_long)len));

		/* Nothing to do */;
	}
	if (ops & BUS_DMASYNC_POSTREAD) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		     "BUS_DMASYNC_POSTREAD\n", (void *)(u_long)va, (u_long)len));
		/* if we have a streaming buffer, flush it here first */
		if (is->is_sb)
			while (len > 0) {
				DPRINTF(IDB_BUSDMA,
				    ("iommu_dvmamap_sync: flushing va %p, %lu "
				     "bytes left\n", (void *)(u_long)va, (u_long)len));
				bus_space_write_8(is->is_bustag, 
						  (bus_space_handle_t)(u_long)
						  &is->is_sb->strbuf_pgflush, 0, va);
				if (len <= NBPG) {
					iommu_strbuf_flush(is);
					len = 0;
				} else
					len -= NBPG;
				va += NBPG;
			}
	}
	if (ops & BUS_DMASYNC_PREWRITE) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		     "BUS_DMASYNC_PREWRITE\n", (void *)(u_long)va, (u_long)len));
		/* Nothing to do */;
	}
	if (ops & BUS_DMASYNC_POSTWRITE) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		     "BUS_DMASYNC_POSTWRITE\n", (void *)(u_long)va, (u_long)len));
		/* Nothing to do */;
	}
}

int
iommu_dvmamem_alloc(t, is, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{

	DPRINTF(IDB_BUSDMA, ("iommu_dvmamem_alloc: sz %llx align %llx bound %llx "
	   "segp %p flags %d\n", (unsigned long long)size,
	   (unsigned long long)alignment, (unsigned long long)boundary,
	   segs, flags));
	return (bus_dmamem_alloc(t->_parent, size, alignment, boundary,
	    segs, nsegs, rsegs, flags|BUS_DMA_DVMA));
}

void
iommu_dvmamem_free(t, is, segs, nsegs)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dma_segment_t *segs;
	int nsegs;
{

	DPRINTF(IDB_BUSDMA, ("iommu_dvmamem_free: segp %p nsegs %d\n",
	    segs, nsegs));
	bus_dmamem_free(t->_parent, segs, nsegs);
}

/*
 * Map the DVMA mappings into the kernel pmap.
 * Check the flags to see whether we're streaming or coherent.
 */
int
iommu_dvmamem_map(t, is, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	struct iommu_state *is;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vm_page_t m;
	vaddr_t va;
	bus_addr_t addr;
	struct pglist *mlist;
	int cbit;

	DPRINTF(IDB_BUSDMA, ("iommu_dvmamem_map: segp %p nsegs %d size %lx\n",
	    segs, nsegs, size));

	/*
	 * Allocate some space in the kernel map, and then map these pages
	 * into this space.
	 */
	size = round_page(size);
	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;

	/* 
	 * digest flags:
	 */
	cbit = 0;
	if (flags & BUS_DMA_COHERENT)	/* Disable vcache */
		cbit |= PMAP_NVC;
	if (flags & BUS_DMA_NOCACHE)	/* sideffects */
		cbit |= PMAP_NC;

	/*
	 * Now take this and map it into the CPU.
	 */
	mlist = segs[0]._ds_mlist;
	for (m = mlist->tqh_first; m != NULL; m = m->pageq.tqe_next) {
#ifdef DIAGNOSTIC
		if (size == 0)
			panic("iommu_dvmamem_map: size botch");
#endif
		addr = VM_PAGE_TO_PHYS(m);
		DPRINTF(IDB_BUSDMA, ("iommu_dvmamem_map: "
		    "mapping va %lx at %llx\n", va, (unsigned long long)addr | cbit));
		pmap_enter(pmap_kernel(), va, addr | cbit,
		    VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return (0);
}

/*
 * Unmap DVMA mappings from kernel
 */
void
iommu_dvmamem_unmap(t, is, kva, size)
	bus_dma_tag_t t;
	struct iommu_state *is;
	caddr_t kva;
	size_t size;
{
	
	DPRINTF(IDB_BUSDMA, ("iommu_dvmamem_unmap: kvm %p size %lx\n",
	    kva, size));
	    
#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("iommu_dvmamem_unmap");
#endif
	
	size = round_page(size);
	pmap_remove(pmap_kernel(), (vaddr_t)kva, size);
#if 0
	/*
	 * XXX ? is this necessary? i think so and i think other
	 * implementations are missing it.
	 */
	uvm_km_free(kernel_map, (vaddr_t)kva, size);
#endif
}
