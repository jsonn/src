/* $NetBSD: vsbus_dma.c,v 1.13.2.1 2008/03/24 07:15:09 keiichi Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vsbus_dma.c,v 1.13.2.1 2008/03/24 07:15:09 keiichi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>

#define _VAX_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/sgmap.h>
#include <machine/vsbus.h>

static int sgmap_bus_dmamap_create_sgmap(bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *);

static void sgmap_bus_dmamap_destroy_sgmap(bus_dma_tag_t, bus_dmamap_t);

static int sgmap_bus_dmamap_load_sgmap(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);

static int sgmap_bus_dmamap_load_mbuf_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);

static int sgmap_bus_dmamap_load_uio_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);

static int sgmap_bus_dmamap_load_raw_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);

static void sgmap_bus_dmamap_unload_sgmap(bus_dma_tag_t, bus_dmamap_t);

static void sgmap_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

void
vax_sgmap_dmatag_init(bus_dma_tag_t t, void *cookie, size_t ptecnt)
{
	/*
	 * Initialize the DMA tag used for sgmap-mapped DMA.
	 */
	t->_cookie = cookie;
	t->_wbase = 0;
	t->_wsize = ptecnt * VAX_NBPG;
	t->_boundary = 0;
	t->_dmamap_create = sgmap_bus_dmamap_create_sgmap;
	t->_dmamap_destroy = sgmap_bus_dmamap_destroy_sgmap;
	t->_dmamap_load = sgmap_bus_dmamap_load_sgmap;
	t->_dmamap_load_mbuf = sgmap_bus_dmamap_load_mbuf_sgmap;
	t->_dmamap_load_uio = sgmap_bus_dmamap_load_uio_sgmap;
	t->_dmamap_load_raw = sgmap_bus_dmamap_load_raw_sgmap;
	t->_dmamap_unload = sgmap_bus_dmamap_unload_sgmap;
	t->_dmamap_sync = sgmap_bus_dmamap_sync;

	t->_dmamem_alloc = _bus_dmamem_alloc;
	t->_dmamem_free = _bus_dmamem_free;
	t->_dmamem_map = _bus_dmamem_map;
	t->_dmamem_unmap = _bus_dmamem_unmap;
	t->_dmamem_mmap = _bus_dmamem_mmap;
}

void
vsbus_dma_init(struct vsbus_softc *sc, unsigned ptecnt)
{
	bus_dma_tag_t t = &sc->sc_dmatag;
	bus_dma_segment_t segs[1];
	struct pte *pte;
	int nsegs;
	int error;
	size_t mapsize = ptecnt * sizeof(struct pte);

	vax_sgmap_dmatag_init(t, sc, ptecnt);

	t->_sgmap = &sc->sc_sgmap;

	if (vax_boardtype == VAX_BTYP_46 || vax_boardtype == VAX_BTYP_48) {
		/*
		 * Allocate and map the VS4000 scatter gather map.
		 */
		error = bus_dmamem_alloc(t, mapsize, mapsize, mapsize,
		    segs, 1, &nsegs, BUS_DMA_NOWAIT);
		if (error) {
			panic("vsbus_dma_init: error allocating memory for "
			    "hw sgmap: error=%d", error);
		}

		error = bus_dmamem_map(t, segs, nsegs, mapsize, 
		   (void **)(void *) &pte, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
		if (error) {
			panic("vsbus_dma_init: error mapping memory for "
			    "hw sgmap: error=%d", error);
		}
		memset(pte, 0, mapsize);
		*(int *) (sc->sc_vsregs + 8) = segs->ds_addr;	/* set MAP BASE 0x2008008 */
	} else {
		pte = (struct pte *) vax_map_physmem(KA49_SCSIMAP, mapsize / VAX_NBPG);
		for (nsegs = ptecnt; nsegs > 0; ) {
			((uint32_t *) pte)[--nsegs] = 0;
		}
		segs->ds_addr = KA49_SCSIMAP;
	}
	aprint_normal_dev(sc->sc_dev,
	    "%uK entry DMA SGMAP at PA 0x%lx (VA %p)\n",
	    ptecnt / 1024, segs->ds_addr, pte);

	/*
	 * Initialize the SGMAP.
	 */
	vax_sgmap_init(t, &sc->sc_sgmap, "vsbus_sgmap", t->_wbase, t->_wsize, pte, 0);

}

/*
 * Create a VSBUS SGMAP-mapped DMA map.
 */
int
sgmap_bus_dmamap_create_sgmap(bus_dma_tag_t t, bus_size_t size,  int nsegments,
	bus_size_t maxsegsz, bus_size_t boundary, int flags,
	bus_dmamap_t *dmamp)
{
	bus_dmamap_t map;
	int error;

	error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
	    boundary, flags, dmamp);
	if (error)
		return (error);

	map = *dmamp;

	if (flags & BUS_DMA_ALLOCNOW) {
		error = vax_sgmap_alloc(map, vax_round_page(size),
		    t->_sgmap, flags);
		if (error)
			sgmap_bus_dmamap_destroy_sgmap(t, map);
	}

	return (error);
}

/*
 * Destroy a VSBUS SGMAP-mapped DMA map.
 */
static void
sgmap_bus_dmamap_destroy_sgmap(bus_dma_tag_t t, bus_dmamap_t map)
{

	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		vax_sgmap_free(map, t->_sgmap);

	_bus_dmamap_destroy(t, map);
}

/*
 * Load a VSBUS SGMAP-mapped DMA map with a linear buffer.
 */
static int
sgmap_bus_dmamap_load_sgmap(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
	bus_size_t buflen, struct proc *p, int flags)
{
	return vax_sgmap_load(t, map, buf, buflen, p, flags, t->_sgmap);
}

/*
 * Load a VSBUS SGMAP-mapped DMA map with an mbuf chain.
 */
static int
sgmap_bus_dmamap_load_mbuf_sgmap(bus_dma_tag_t t, bus_dmamap_t map,
	struct mbuf *m, int flags)
{
	return vax_sgmap_load_mbuf(t, map, m, flags, t->_sgmap);
}

/*
 * Load a VSBUS SGMAP-mapped DMA map with a uio.
 */
static int
sgmap_bus_dmamap_load_uio_sgmap(bus_dma_tag_t t, bus_dmamap_t map,
	struct uio *uio, int flags)
{
	return vax_sgmap_load_uio(t, map, uio, flags, t->_sgmap);
}

/*
 * Load a VSBUS SGMAP-mapped DMA map with raw memory.
 */
static int
sgmap_bus_dmamap_load_raw_sgmap(bus_dma_tag_t t, bus_dmamap_t map,
	bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	return vax_sgmap_load_raw(t, map, segs, nsegs, size, flags, t->_sgmap);
}

/*
 * Unload a VSBUS DMA map.
 */
static void
sgmap_bus_dmamap_unload_sgmap(bus_dma_tag_t t, bus_dmamap_t map)
{
	/*
	 * Invalidate any SGMAP page table entries used by this
	 * mapping.
	 */
	vax_sgmap_unload(t, map, t->_sgmap);

	/*
	 * Do the generic bits of the unload.
	 */
	_bus_dmamap_unload(t, map);
}

/*
 * Sync the bus map.
 */
static void
sgmap_bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset,
	bus_size_t len, int ops)
{
	/* not needed */
}
