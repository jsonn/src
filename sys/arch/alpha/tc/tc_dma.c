/* $NetBSD: tc_dma.c,v 1.1.2.1 1997/06/03 23:34:23 thorpej Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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

#include <machine/options.h>		/* Config options headers */
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: tc_dma.c,v 1.1.2.1 1997/06/03 23:34:23 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <vm/vm.h>

#define _ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/rpb.h>

#include <dev/tc/tcreg.h>
#include <dev/tc/tcvar.h>
#include <alpha/tc/tc_sgmap.h>

extern int cputype;

bus_dma_tag_t tc_dma_get_tag __P((int));

#ifdef DEC_3000_500
int	tc_bus_dmamap_create_sgmap __P((bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *));
void	tc_bus_dmamap_destroy_sgmap __P((bus_dma_tag_t, bus_dmamap_t));
int	tc_bus_dmamap_load_sgmap __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	tc_bus_dmamap_load_mbuf_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));
int	tc_bus_dmamap_load_uio_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	tc_bus_dmamap_load_raw_sgmap __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
void	tc_bus_dmamap_unload_sgmap __P((bus_dma_tag_t, bus_dmamap_t));
#endif /* DEC_3000_500 */

int	tc_bus_dmamap_load_direct __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	tc_bus_dmamap_load_mbuf_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));
int	tc_bus_dmamap_load_uio_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	tc_bus_dmamap_load_raw_direct __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));


struct alpha_bus_dma_tag tc_dmat_direct = {
	NULL,				/* _cookie */
	NULL,				/* _get_tag */
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	tc_bus_dmamap_load_direct,
	tc_bus_dmamap_load_mbuf_direct,
	tc_bus_dmamap_load_uio_direct,
	tc_bus_dmamap_load_raw_direct,
	_bus_dmamap_unload,
	NULL,				/* _dmamap_sync */
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

#ifdef DEC_3000_500
struct alpha_bus_dma_tag tc_dmat_sgmap = {
	NULL,				/* _cookie */
	NULL,				/* _get_tag */
	tc_bus_dmamap_create_sgmap,
	tc_bus_dmamap_destroy_sgmap,
	tc_bus_dmamap_load_sgmap,
	tc_bus_dmamap_load_mbuf_sgmap,
	tc_bus_dmamap_load_uio_sgmap,
	tc_bus_dmamap_load_raw_sgmap,
	tc_bus_dmamap_unload_sgmap,
	NULL,				/* _dmamap_sync */
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

struct tc_dma_slot_info {
	struct alpha_sgmap tdsi_sgmap;		/* sgmap for slot */
	struct alpha_bus_dma_tag tdsi_dmat;	/* dma tag for slot */
};
struct tc_dma_slot_info *tc_dma_slot_info;
#endif /* DEC_3000_500 */

void
tc_dma_init(nslots)
	int nslots;
{

#ifdef DEC_3000_500
	if (cputype == ST_DEC_3000_500) {
		size_t sisize;
		int i;

		/* Allocate per-slot DMA info. */
		sisize = nslots * sizeof(struct tc_dma_slot_info);
		tc_dma_slot_info = malloc(sisize, M_DEVBUF, M_NOWAIT);
		if (tc_dma_slot_info == NULL)
			panic("tc_dma_init: can't allocate per-slot DMA info");
		bzero(tc_dma_slot_info, sisize);

		/* Default all slots to direct-mapped. */
		for (i = 0; i < nslots; i++)
			bcopy(&tc_dmat_direct, &tc_dma_slot_info[i].tdsi_dmat,
			    sizeof(tc_dma_slot_info[i].tdsi_dmat));
	}
#endif /* DEC_3000_500 */

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		extern vm_offset_t alpha_XXX_dmamap_or;		/* XXX */
		alpha_XXX_dmamap_or = 0;			/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */
}

/*
 * Return the DMA tag for the given slot.
 */
bus_dma_tag_t
tc_dma_get_tag(slot)
	int slot;
{
	bus_dma_tag_t t;

	switch (cputype) {
#ifdef DEC_3000_500
	case ST_DEC_3000_500:
		t = &tc_dma_slot_info[slot].tdsi_dmat;
		break;
#endif /* DEC_3000_500 */

#ifdef DEC_3000_300
	case ST_DEC_3000_300:
		/* Pelican doesn't have SGMAPs. */
		t = &tc_dmat_direct;
		break;
#endif /* ST_DEC_3000_300 */

	default:
		panic("tc_dma_get_tag: bad cputype");
	}

	return (t);
}

#ifdef DEC_3000_500
/*
 * Create a TurboChannel SGMAP-mapped DMA map.
 */
int
tc_bus_dmamap_create_sgmap(t, size, nsegments, maxsegsz, boundary,
    flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;
	struct alpha_sgmap_cookie *a;
	bus_dmamap_t map;
	int error;

	error = _bus_dmamap_create(t, size, nsegments, maxsegsz,
	    boundary, flags, dmamp);
	if (error)
		return (error);

	map = *dmamp;

	a = malloc(sizeof(struct alpha_sgmap_cookie), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	if (a == NULL) {
		_bus_dmamap_destroy(t, map);
		return (ENOMEM);
	}
	bzero(a, sizeof(struct alpha_sgmap_cookie));
	map->_dm_sgcookie = a;

	if (flags & BUS_DMA_ALLOCNOW) {
		error = alpha_sgmap_alloc(map, round_page(size),
		    &tdsi->tdsi_sgmap, flags);
		if (error)
			tc_bus_dmamap_destroy_sgmap(t, map);
	}

	return (error);
}

/*
 * Destroy a TurboChannel SGMAP-mapped DMA map.
 */
void
tc_bus_dmamap_destroy_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;
	struct alpha_sgmap_cookie *a = map->_dm_sgcookie;

	if (a->apdc_flags & APDC_HAS_SGMAP)
		alpha_sgmap_free(&tdsi->tdsi_sgmap, a);

	free(a, M_DEVBUF);
	_bus_dmamap_destroy(t, map);
}

/*
 * Load a TurboChannel direct-mapped DMA map with a linear buffer.
 */
int
tc_bus_dmamap_load_direct(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{

	return (_bus_dmamap_load_direct_common(t, map, buf, buflen, p,
	    flags, 0));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with a linear buffer.
 */
int
tc_bus_dmamap_load_sgmap(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load(t, map, buf, buflen, p, flags,
	    &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel direct-mapped DMA map with an mbuf chain.
 */
int
tc_bus_dmamap_load_mbuf_direct(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	return (_bus_dmamap_load_mbuf_direct_common(t, map, m,
	    flags, 0));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with an mbuf chain.
 */
int
tc_bus_dmamap_load_mbuf_sgmap(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_mbuf(t, map, m, flags, &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel direct-mapped DMA map with a uio.
 */
int
tc_bus_dmamap_load_uio_direct(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	return (_bus_dmamap_load_uio_direct_common(t, map, uio,
	    flags, 0));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with a uio.
 */
int
tc_bus_dmamap_load_uio_sgmap(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_uio(t, map, uio, flags, &tdsi->tdsi_sgmap));
}

/*
 * Load a TurboChannel direct-mapped DMA map with raw memory.
 */
int
tc_bus_dmamap_load_raw_direct(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	return (_bus_dmamap_load_raw_direct_common(t, map, segs, nsegs,
	    size, flags, 0));
}

/*
 * Load a TurboChannel SGMAP-mapped DMA map with raw memory.
 */
int
tc_bus_dmamap_load_raw_sgmap(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	return (tc_sgmap_load_raw(t, map, segs, nsegs, size, flags,
	    &tdsi->tdsi_sgmap));
}

/*
 * Unload a TurboChannel DMA map.
 */
void
tc_bus_dmamap_unload_sgmap(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	struct tc_dma_slot_info *tdsi = t->_cookie;

	/*
	 * Invalidate any SGMAP page table entries used by this
	 * mapping.
	 */
	tc_sgmap_unload(t, map, &tdsi->tdsi_sgmap);

	/*
	 * Do the generic bits of the unload.
	 */
	_bus_dmamap_unload(t, map);
}
#endif /* DEC_3000_500 */
