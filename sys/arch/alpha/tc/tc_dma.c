/* $NetBSD: tc_dma.c,v 1.2.6.1 1997/09/04 00:54:18 thorpej Exp $ */

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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: tc_dma.c,v 1.2.6.1 1997/09/04 00:54:18 thorpej Exp $");

#include <sys/param.h>
#include <sys/device.h>

#define _ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <dev/tc/tcvar.h>

bus_dma_tag_t (*tc_dma_get_tag_func) __P((int));

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

void
tc_dma_init()
{

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		extern vm_offset_t alpha_XXX_dmamap_or;		/* XXX */
		alpha_XXX_dmamap_or = 0;			/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */
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
