/* $NetBSD: uba_dma.c,v 1.10.22.1 2011/03/05 20:52:17 rmind Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: uba_dma.c,v 1.10.22.1 2011/03/05 20:52:17 rmind Exp $");

#define _VAX_BUS_DMA_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/sgmap.h>

#include <dev/qbus/ubavar.h>

#include <arch/vax/uba/uba_common.h>

int	uba_bus_dmamap_create_sgmap(bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *);

void	uba_bus_dmamap_destroy_sgmap(bus_dma_tag_t, bus_dmamap_t);

int	uba_bus_dmamap_load_sgmap(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);

int	uba_bus_dmamap_load_mbuf_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);

int	uba_bus_dmamap_load_uio_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);

int	uba_bus_dmamap_load_raw_sgmap(bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);

void	uba_bus_dmamap_unload_sgmap(bus_dma_tag_t, bus_dmamap_t);

void	uba_bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);

void
uba_dma_init(struct uba_vsoftc *sc)
{
	bus_dma_tag_t t;
	struct pte *pte;

	/*
	 * Initialize the DMA tag used for sgmap-mapped DMA.
	 */
	t = &sc->uv_dmat;
	t->_cookie = sc;
	t->_wbase = 0;
	t->_wsize = sc->uv_size;
	t->_boundary = 0;
	t->_sgmap = &sc->uv_sgmap;
	t->_dmamap_create = uba_bus_dmamap_create_sgmap;
	t->_dmamap_destroy = uba_bus_dmamap_destroy_sgmap;
	t->_dmamap_load = uba_bus_dmamap_load_sgmap;
	t->_dmamap_load_mbuf = uba_bus_dmamap_load_mbuf_sgmap;
	t->_dmamap_load_uio = uba_bus_dmamap_load_uio_sgmap;
	t->_dmamap_load_raw = uba_bus_dmamap_load_raw_sgmap;
	t->_dmamap_unload = uba_bus_dmamap_unload_sgmap;
	t->_dmamap_sync = uba_bus_dmamap_sync;

	t->_dmamem_alloc = _bus_dmamem_alloc;
	t->_dmamem_free = _bus_dmamem_free;
	t->_dmamem_map = _bus_dmamem_map;
	t->_dmamem_unmap = _bus_dmamem_unmap;
	t->_dmamem_mmap = _bus_dmamem_mmap;

	/*
	 * Map in Unibus map registers, if not mapped in already.
	 */
	if (sc->uv_uba) {
		pte = sc->uv_uba->uba_map;
	} else {
		pte = (struct pte *)vax_map_physmem(sc->uv_addr,
		    vax_btoc(vax_btoc(sc->uv_size) * sizeof(struct pte)));
		if (pte == 0)
			panic("uba_dma_init");
	}
	/*
	 * Initialize the SGMAP.
	 */
	vax_sgmap_init(t, &sc->uv_sgmap, "uba_sgmap", 0, sc->uv_size, pte, 0);

}

/*
 * Create a UBA SGMAP-mapped DMA map.
 */
int
uba_bus_dmamap_create_sgmap(bus_dma_tag_t t, bus_size_t size,  int nsegments,
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
			uba_bus_dmamap_destroy_sgmap(t, map);
	}

	return (error);
}

/*
 * Destroy a UBA SGMAP-mapped DMA map.
 */
void
uba_bus_dmamap_destroy_sgmap(bus_dma_tag_t t, bus_dmamap_t map)
{

	if (map->_dm_flags & DMAMAP_HAS_SGMAP)
		vax_sgmap_free(map, t->_sgmap);

	_bus_dmamap_destroy(t, map);
}

/*
 * Load a UBA SGMAP-mapped DMA map with a linear buffer.
 */
int
uba_bus_dmamap_load_sgmap(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
	bus_size_t buflen, struct proc *p, int flags)
{
	int error;

	error = vax_sgmap_load(t, map, buf, buflen, p, flags, t->_sgmap);
	/*
	 * XXX - Set up BDPs.
	 */

	return (error);
}

/*
 * Load a UBA SGMAP-mapped DMA map with an mbuf chain.
 */
int
uba_bus_dmamap_load_mbuf_sgmap( bus_dma_tag_t t, bus_dmamap_t map,
	struct mbuf *m, int flags)
{
	int error;

	error = vax_sgmap_load_mbuf(t, map, m, flags, t->_sgmap);

	return (error);
}

/*
 * Load a UBA SGMAP-mapped DMA map with a uio.
 */
int
uba_bus_dmamap_load_uio_sgmap(bus_dma_tag_t t, bus_dmamap_t map,
	struct uio *uio, int flags)
{
	int error;

	error = vax_sgmap_load_uio(t, map, uio, flags, t->_sgmap);

	return (error);
}

/*
 * Load a UBA SGMAP-mapped DMA map with raw memory.
 */
int
uba_bus_dmamap_load_raw_sgmap(bus_dma_tag_t t, bus_dmamap_t map,
	bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	int error;

	error = vax_sgmap_load_raw(t, map, segs, nsegs, size, flags,
	    t->_sgmap);

	return (error);
}

/*
 * Unload a UBA DMA map.
 */
void
uba_bus_dmamap_unload_sgmap(bus_dma_tag_t t, bus_dmamap_t map)
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
 * Sync the bus map. This is only needed if BDP's are used.
 */
void
uba_bus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t dmam, bus_addr_t offset,
	bus_size_t len, int ops)
{
	/* Only BDP handling, but not yet. */
}
