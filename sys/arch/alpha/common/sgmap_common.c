/* $NetBSD: sgmap_common.c,v 1.12.8.2 2001/01/05 17:33:42 bouyer Exp $ */

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sgmap_common.c,v 1.12.8.2 2001/01/05 17:33:42 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#define	_ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <alpha/common/sgmapvar.h>

/*
 * Some systems will prefetch the next page during a memory -> device DMA.
 * This can cause machine checks if there is not a spill page after the
 * last page of the DMA (thus avoiding hitting an invalid SGMAP PTE).
 */
vaddr_t		alpha_sgmap_prefetch_spill_page_va;
bus_addr_t	alpha_sgmap_prefetch_spill_page_pa;

void
alpha_sgmap_init(bus_dma_tag_t t, struct alpha_sgmap *sgmap, const char *name,
    bus_addr_t wbase, bus_addr_t sgvabase, bus_size_t sgvasize, size_t ptesize,
    void *ptva, bus_size_t minptalign)
{
	bus_dma_segment_t seg;
	size_t ptsize;
	int rseg;

	if (sgvasize & PGOFSET) {
		printf("size botch for sgmap `%s'\n", name);
		goto die;
	}

	sgmap->aps_wbase = wbase;
	sgmap->aps_sgvabase = sgvabase;
	sgmap->aps_sgvasize = sgvasize;

	if (ptva != NULL) {
		/*
		 * We already have a page table; this may be a system
		 * where the page table resides in bridge-resident SRAM.
		 */
		sgmap->aps_pt = ptva;
		sgmap->aps_ptpa = 0;
	} else {
		/*
		 * Compute the page table size and allocate it.  At minimum,
		 * this must be aligned to the page table size.  However,
		 * some platforms have more strict alignment reqirements.
		 */
		ptsize = (sgvasize / NBPG) * ptesize;
		if (minptalign != 0) {
			if (minptalign < ptsize)
				minptalign = ptsize;
		} else
			minptalign = ptsize;
		if (bus_dmamem_alloc(t, ptsize, minptalign, 0, &seg, 1, &rseg,
		    BUS_DMA_NOWAIT)) {
			panic("unable to allocate page table for sgmap `%s'\n",
			    name);
			goto die;
		}
		sgmap->aps_ptpa = seg.ds_addr;
		sgmap->aps_pt = (caddr_t)ALPHA_PHYS_TO_K0SEG(sgmap->aps_ptpa);
	}

	/*
	 * Create the extent map used to manage the virtual address
	 * space.
	 */
	sgmap->aps_ex = extent_create((char *)name, sgvabase, sgvasize - 1,
	    M_DMAMAP, NULL, 0, EX_NOWAIT|EX_NOCOALESCE);
	if (sgmap->aps_ex == NULL) {
		printf("unable to create extent map for sgmap `%s'\n",
		    name);
		goto die;
	}

	/*
	 * Allocate a spill page if that hasn't already been done.
	 */
	if (alpha_sgmap_prefetch_spill_page_va == 0) {
		if (bus_dmamem_alloc(t, NBPG, 0, 0, &seg, 1, &rseg,
		    BUS_DMA_NOWAIT)) {
			printf("unable to allocate spill page for sgmap `%s'\n",
			    name);
			goto die;
		}
		alpha_sgmap_prefetch_spill_page_pa = seg.ds_addr;
		alpha_sgmap_prefetch_spill_page_va =
		    ALPHA_PHYS_TO_K0SEG(alpha_sgmap_prefetch_spill_page_pa);
		bzero((caddr_t)alpha_sgmap_prefetch_spill_page_va, NBPG);
	}
	
	return;
 die:
	panic("alpha_sgmap_init");
}

int
alpha_sgmap_alloc(bus_dmamap_t map, bus_size_t origlen,
    struct alpha_sgmap *sgmap, int flags)
{
	int error;
	bus_size_t len = origlen, boundary, alignment;

#ifdef DIAGNOSTIC
	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		panic("alpha_sgmap_alloc: already have sgva space");
#endif
	/*
	 * Add a range for spill page.
	 */
	len += NBPG;

	/*
	 * And add an additional amount in case of ALLOCNOW.
	 */
	if (flags & BUS_DMA_ALLOCNOW)
		len += NBPG;

	map->_dm_sgvalen = round_page(len);

	/*
	 * ARGH! If the addition of spill pages bumped us over our
	 * boundary, we have to 2x the boundary limit.
	 */
	boundary = map->_dm_boundary;
	if (boundary && boundary < map->_dm_sgvalen) {
		alignment = boundary;
		do {
			boundary <<= 1;
		} while (boundary < map->_dm_sgvalen);
	} else
		alignment = NBPG;
#if 0
	printf("len %x -> %x, _dm_sgvalen %x _dm_boundary %x boundary %x -> ",
	    origlen, len, map->_dm_sgvalen, map->_dm_boundary, boundary);
#endif

	error = extent_alloc(sgmap->aps_ex, map->_dm_sgvalen, alignment,
	    boundary, (flags & BUS_DMA_NOWAIT) ? EX_NOWAIT : EX_WAITOK,
	    &map->_dm_sgva);
#if 0
	printf("error %d _dm_sgva %x\n", error, map->_dm_sgva);
#endif

	if (error == 0)
		map->_dm_flags |= DMAMAP_HAS_SGMAP;
	else
		map->_dm_flags &= ~DMAMAP_HAS_SGMAP;
	
	return (error);
}

void
alpha_sgmap_free(bus_dmamap_t map, struct alpha_sgmap *sgmap)
{

#ifdef DIAGNOSTIC
	if ((map->_dm_flags & DMAMAP_HAS_SGMAP) == 0)
		panic("alpha_sgmap_free: no sgva space to free");
#endif

	if (extent_free(sgmap->aps_ex, map->_dm_sgva, map->_dm_sgvalen,
	    EX_NOWAIT))
		panic("alpha_sgmap_free");

	map->_dm_flags &= ~DMAMAP_HAS_SGMAP;
}

int
alpha_sgmap_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	bus_dmamap_t map;
	int error;

	error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
	    boundary, flags, &map);
	if (error)
		return (error);

	if (flags & BUS_DMA_ALLOCNOW)
		error = alpha_sgmap_alloc(map, round_page(size),
		    t->_sgmap, flags);

	if (error == 0)
		*dmamp = map;
	else
		alpha_sgmap_dmamap_destroy(t, map);

	return (error);
}

void
alpha_sgmap_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{

	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		alpha_sgmap_free(map, t->_sgmap);

	_bus_dmamap_destroy(t, map);
}
