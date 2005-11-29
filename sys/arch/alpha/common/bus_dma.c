/* $NetBSD: bus_dma.c,v 1.59.6.1 2005/11/29 21:22:56 yamt Exp $ */

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

__KERNEL_RCSID(0, "$NetBSD: bus_dma.c,v 1.59.6.1 2005/11/29 21:22:56 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mbuf.h>

#include <uvm/uvm_extern.h>

#define _ALPHA_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>

int	_bus_dmamap_load_buffer_direct(bus_dma_tag_t,
	    bus_dmamap_t, void *, bus_size_t, struct proc *, int,
	    paddr_t *, int *, int);

extern paddr_t avail_start, avail_end;	/* from pmap.c */

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct alpha_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct alpha_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	if ((mapstore = malloc(mapsize, M_DMAMAP,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	memset(mapstore, 0, mapsize);
	map = (struct alpha_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxmaxsegsz = maxsegsz;
	if (t->_boundary != 0 && t->_boundary < boundary)
		map->_dm_boundary = t->_boundary;
	else
		map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_maxsegsz = maxsegsz;
	map->dm_mapsize = 0;		/* no valid mappings */
	map->dm_nsegs = 0;
	map->_dm_window = NULL;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{

	free(map, M_DMAMAP);
}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
int
_bus_dmamap_load_buffer_direct(bus_dma_tag_t t, bus_dmamap_t map,
    void *buf, size_t buflen, struct proc *p, int flags, paddr_t *lastaddrp,
    int *segp, int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vaddr_t vaddr = (vaddr_t)buf;
	int seg;

	lastaddr = *lastaddrp;
	bmask = ~(map->_dm_boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (p != NULL)
			(void) pmap_extract(p->p_vmspace->vm_map.pmap,
			    vaddr, &curaddr);
		else
			curaddr = vtophys(vaddr);

		/*
		 * If we're beyond the current DMA window, indicate
		 * that and try to fall back into SGMAPs.
		 */
		if (t->_wsize != 0 && curaddr >= t->_wsize)
			return (EINVAL);

		curaddr |= t->_wbase;

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;
		if (map->dm_maxsegsz < sgsize)
			sgsize = map->dm_maxsegsz;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = (curaddr + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * the previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = curaddr;
			map->dm_segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if ((map->_dm_flags & DMAMAP_NO_COALESCE) == 0 &&
			    curaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->dm_maxsegsz &&
			    (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     (curaddr & bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt)
					break;
				map->dm_segs[seg].ds_addr = curaddr;
				map->dm_segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		/*
		 * If there is a chained window, we will automatically
		 * fall back to it.
		 */
		return (EFBIG);		/* XXX better return value here? */
	}

	return (0);
}

/*
 * Common function for loading a direct-mapped DMA map with a linear
 * buffer.  Called by bus-specific DMA map load functions with the
 * OR value appropriate for indicating "direct-mapped" for that
 * chipset.
 */
int
_bus_dmamap_load_direct(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	paddr_t lastaddr;
	int seg, error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);
	KASSERT((map->_dm_flags & (BUS_DMA_READ|BUS_DMA_WRITE)) == 0);

	if (buflen > map->_dm_size)
		return (EINVAL);

	seg = 0;
	error = _bus_dmamap_load_buffer_direct(t, map, buf, buflen,
	    p, flags, &lastaddr, &seg, 1);
	if (error == 0) {
		map->dm_mapsize = buflen;
		map->dm_nsegs = seg + 1;
		map->_dm_window = t;
	} else if (t->_next_window != NULL) {
		/*
		 * Give the next window a chance.
		 */
		error = bus_dmamap_load(t->_next_window, map, buf, buflen,
		    p, flags);
	}
	return (error);
}

/*
 * Like _bus_dmamap_load_direct(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf_direct(bus_dma_tag_t t, bus_dmamap_t map,
    struct mbuf *m0, int flags)
{
	paddr_t lastaddr;
	int seg, error, first;
	struct mbuf *m;

	/*
	 * Make sure that on error condition we return "no valid mappings."
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);
	KASSERT((map->_dm_flags & (BUS_DMA_READ|BUS_DMA_WRITE)) == 0);

#ifdef DIAGNOSTIC
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("_bus_dmamap_load_mbuf_direct: no packet header");
#endif

	if (m0->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	first = 1;
	seg = 0;
	error = 0;
	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		/* XXX Could be better about coalescing. */
		/* XXX Doesn't check boundaries. */
		switch (m->m_flags & (M_EXT|M_EXT_CLUSTER)) {
		case M_EXT|M_EXT_CLUSTER:
			/* XXX KDASSERT */
			KASSERT(m->m_ext.ext_paddr != M_PADDR_INVALID);
			lastaddr = m->m_ext.ext_paddr +
			    (m->m_data - m->m_ext.ext_buf);
 have_addr:
			if (first == 0 &&
			    ++seg >= map->_dm_segcnt) {
				error = EFBIG;
				break;
			}

			/*
			 * If we're beyond the current DMA window, indicate
			 * that and try to fall back into SGMAPs.
			 */
			if (t->_wsize != 0 && lastaddr >= t->_wsize) {
				error = EINVAL;
				break;
			}
			lastaddr |= t->_wbase;

			map->dm_segs[seg].ds_addr = lastaddr;
			map->dm_segs[seg].ds_len = m->m_len;
			lastaddr += m->m_len;
			break;

		case 0:
			lastaddr = m->m_paddr + M_BUFOFFSET(m) +
			    (m->m_data - M_BUFADDR(m));
			goto have_addr;

		default:
			error = _bus_dmamap_load_buffer_direct(t, map,
			    m->m_data, m->m_len, NULL, flags, &lastaddr,
			    &seg, first);
		}
		first = 0;
	}
	if (error == 0) {
		map->dm_mapsize = m0->m_pkthdr.len;
		map->dm_nsegs = seg + 1;
		map->_dm_window = t;
	} else if (t->_next_window != NULL) {
		/*
		 * Give the next window a chance.
		 */
		error = bus_dmamap_load_mbuf(t->_next_window, map, m0, flags);
	}
	return (error);
}

/*
 * Like _bus_dmamap_load_direct(), but for uios.
 */
int
_bus_dmamap_load_uio_direct(bus_dma_tag_t t, bus_dmamap_t map,
    struct uio *uio, int flags)
{
	paddr_t lastaddr;
	int seg, i, error, first;
	bus_size_t minlen, resid;
	struct proc *p = NULL;
	struct iovec *iov;
	caddr_t addr;

	/*
	 * Make sure that on error condition we return "no valid mappings."
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);
	KASSERT((map->_dm_flags & (BUS_DMA_READ|BUS_DMA_WRITE)) == 0);

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		p = uio->uio_procp;
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("_bus_dmamap_load_uio_direct: "
			    "USERSPACE but no proc");
#endif
	}

	first = 1;
	seg = 0;
	error = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && error == 0; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		addr = (caddr_t)iov[i].iov_base;

		error = _bus_dmamap_load_buffer_direct(t, map,
		    addr, minlen, p, flags, &lastaddr, &seg, first);
		first = 0;

		resid -= minlen;
	}
	if (error == 0) {
		map->dm_mapsize = uio->uio_resid;
		map->dm_nsegs = seg + 1;
		map->_dm_window = t;
	} else if (t->_next_window != NULL) {
		/*
		 * Give the next window a chance.
		 */
		error = bus_dmamap_load_uio(t->_next_window, map, uio, flags);
	}
	return (error);
}

/*
 * Like _bus_dmamap_load_direct(), but for raw memory.
 */
int
_bus_dmamap_load_raw_direct(bus_dma_tag_t t, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{

	panic("_bus_dmamap_load_raw_direct: not implemented");
}

/*
 * Common function for unloading a DMA map.  May be called by
 * chipset-specific DMA map unload functions.
 */
void
_bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{

	/*
	 * No resources to free; just mark the mappings as
	 * invalid.
	 */
	map->dm_maxsegsz = map->_dm_maxmaxsegsz;
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	map->_dm_window = NULL;
	map->_dm_flags &= ~(BUS_DMA_READ|BUS_DMA_WRITE);
}

/*
 * Common function for DMA map synchronization.  May be called
 * by chipset-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t offset,
    bus_size_t len, int ops)
{

	/*
	 * Flush the store buffer.
	 */
	alpha_mb();
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_bus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{

	return (_bus_dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, 0, trunc_page(avail_end)));
}

/*
 * Allocate physical memory from the given physical address range.
 * Called by DMA-safe memory allocation methods.
 */
int
_bus_dmamem_alloc_range(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags, paddr_t low, paddr_t high)
{
	paddr_t curaddr, lastaddr;
	struct vm_page *m;    
	struct pglist mlist;
	int curseg, error;

	/* Always round the size. */
	size = round_page(size);

	/*
	 * Allocate pages from the VM system.
	 */
	error = uvm_pglistalloc(size, low, high, alignment, boundary,
	    &mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	m = mlist.tqh_first;
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = VM_PAGE_TO_PHYS(m);
	segs[curseg].ds_len = PAGE_SIZE;
	m = m->pageq.tqe_next;

	for (; m != NULL; m = m->pageq.tqe_next) {
		curaddr = VM_PAGE_TO_PHYS(m);
#ifdef DIAGNOSTIC
		if (curaddr < avail_start || curaddr >= high) {
			printf("uvm_pglistalloc returned non-sensical"
			    " address 0x%lx\n", curaddr);
			panic("_bus_dmamem_alloc");
		}
#endif
		if (curaddr == (lastaddr + PAGE_SIZE))
			segs[curseg].ds_len += PAGE_SIZE;
		else {
			curseg++;
			segs[curseg].ds_addr = curaddr;
			segs[curseg].ds_len = PAGE_SIZE;
		}
		lastaddr = curaddr;
	}

	*rsegs = curseg + 1;

	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_bus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	struct vm_page *m;
	bus_addr_t addr;
	struct pglist mlist;
	int curseg;

	/*
	 * Build a list of pages to free back to the VM system.
	 */
	TAILQ_INIT(&mlist);
	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += PAGE_SIZE) {
			m = PHYS_TO_VM_PAGE(addr);
			TAILQ_INSERT_TAIL(&mlist, m, pageq);
		}
	}

	uvm_pglistfree(&mlist);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
_bus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	vaddr_t va;
	bus_addr_t addr;
	int curseg;
	const uvm_flag_t kmflags =
	    (flags & BUS_DMA_NOWAIT) != 0 ? UVM_KMF_NOWAIT : 0;

	/*
	 * If we're only mapping 1 segment, use K0SEG, to avoid
	 * TLB thrashing.
	 */
	if (nsegs == 1) {
		*kvap = (caddr_t)ALPHA_PHYS_TO_K0SEG(segs[0].ds_addr);
		return (0);
	}

	size = round_page(size);

	va = uvm_km_alloc(kernel_map, size, 0, UVM_KMF_VAONLY | kmflags);

	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;

	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += PAGE_SIZE, va += PAGE_SIZE, size -= PAGE_SIZE) {
			if (size == 0)
				panic("_bus_dmamem_map: size botch");
			pmap_enter(pmap_kernel(), va, addr,
			    VM_PROT_READ | VM_PROT_WRITE,
			    PMAP_WIRED | VM_PROT_READ | VM_PROT_WRITE);
		}
	}
	pmap_update(pmap_kernel());

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_bus_dmamem_unmap");
#endif

	/*
	 * Nothing to do if we mapped it with K0SEG.
	 */
	if (kva >= (caddr_t)ALPHA_K0SEG_BASE &&
	    kva <= (caddr_t)ALPHA_K0SEG_END)
		return;

	size = round_page(size);
	pmap_remove(pmap_kernel(), (vaddr_t)kva, (vaddr_t)kva + size);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, (vaddr_t)kva, size, UVM_KMF_VAONLY);
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
paddr_t
_bus_dmamem_mmap(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
    off_t off, int prot, int flags)
{
	int i;

	for (i = 0; i < nsegs; i++) {
#ifdef DIAGNOSTIC
		if (off & PGOFSET)
			panic("_bus_dmamem_mmap: offset unaligned");
		if (segs[i].ds_addr & PGOFSET)
			panic("_bus_dmamem_mmap: segment unaligned");
		if (segs[i].ds_len & PGOFSET)
			panic("_bus_dmamem_mmap: segment size not multiple"
			    " of page size");
#endif
		if (off >= segs[i].ds_len) {
			off -= segs[i].ds_len;
			continue;
		}

		return (alpha_btop((caddr_t)segs[i].ds_addr + off));
	}

	/* Page not found. */
	return (-1);
}
