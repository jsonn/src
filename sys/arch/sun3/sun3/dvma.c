/*	$NetBSD: dvma.c,v 1.24.6.1 2004/08/03 10:42:11 skrll Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dvma.c,v 1.24.6.1 2004/08/03 10:42:11 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <sys/core.h>
#include <sys/exec.h>

#include <uvm/uvm.h> /* XXX: not _extern ... need uvm_map_create */

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <sun3/sun3/control.h>
#include <sun3/sun3/machdep.h>

/* DVMA is the last 1MB, but the PROM owns the last page. */
#define DVMA_MAP_END	(DVMA_MAP_BASE + DVMA_MAP_AVAIL)

/* Extent map used by dvma_mapin/dvma_mapout */
struct extent *dvma_extent;

/* XXX: Might need to tune this... */
vsize_t dvma_segmap_size = 6 * NBSG;

/* Using phys_map to manage DVMA scratch-memory pages. */
/* Note: Could use separate pagemap for obio if needed. */

void
dvma_init()
{
	vaddr_t segmap_addr;

	/*
	 * Create phys_map covering the entire DVMA space,
	 * then allocate the segment pool from that.  The
	 * remainder will be used as the DVMA page pool.
	 *
	 * Note that no INTRSAFE is needed here because the
	 * dvma_extent manages things handled in interrupt
	 * context.
	 */
	phys_map = uvm_map_create(pmap_kernel(),
		DVMA_MAP_BASE, DVMA_MAP_END, 0);
	if (phys_map == NULL)
		panic("unable to create DVMA map");

	/*
	 * Reserve the DVMA space used for segment remapping.
	 * The remainder of phys_map is used for DVMA scratch
	 * memory pages (i.e. driver control blocks, etc.)
	 */
	segmap_addr = uvm_km_valloc_wait(phys_map, dvma_segmap_size);
	if (segmap_addr != DVMA_MAP_BASE)
		panic("dvma_init: unable to allocate DVMA segments");

	/*
	 * Create the VM pool used for mapping whole segments
	 * into DVMA space for the purpose of data transfer.
	 */
	dvma_extent = extent_create("dvma", segmap_addr,
	    segmap_addr + (dvma_segmap_size - 1), M_DEVBUF,
	    NULL, 0, EX_NOCOALESCE|EX_NOWAIT);
}

/*
 * Allocate actual memory pages in DVMA space.
 * (idea for implementation borrowed from Chris Torek.)
 */
void *
dvma_malloc(bytes)
	size_t bytes;
{
    caddr_t new_mem;
    vsize_t new_size;

    if (!bytes)
		return NULL;
    new_size = m68k_round_page(bytes);
    new_mem = (caddr_t) uvm_km_alloc(phys_map, new_size);
    if (!new_mem)
		panic("dvma_malloc: no space in phys_map");
    /* The pmap code always makes DVMA pages non-cached. */
    return new_mem;
}

/*
 * Free pages from dvma_malloc()
 */
void
dvma_free(addr, size)
	void *addr;
	size_t size;
{
	vsize_t sz = m68k_round_page(size);

	uvm_km_free(phys_map, (vaddr_t)addr, sz);
}

/*
 * Given a DVMA address, return the physical address that
 * would be used by some OTHER bus-master besides the CPU.
 * (Examples: on-board ie/le, VME xy board).
 */
u_long
dvma_kvtopa(kva, bustype)
	void *kva;
	int bustype;
{
	u_long addr, mask;

	addr = (u_long)kva;
	if ((addr & DVMA_MAP_BASE) != DVMA_MAP_BASE)
		panic("dvma_kvtopa: bad dmva addr=0x%lx", addr);

	switch (bustype) {
	case BUS_OBIO:
	case BUS_OBMEM:
		mask = DVMA_OBIO_SLAVE_MASK;
		break;
	default:	/* VME bus device. */
		mask = DVMA_VME_SLAVE_MASK;
		break;
	}

	return(addr & mask);
}

/*
 * Given a range of kernel virtual space, remap all the
 * pages found there into the DVMA space (dup mappings).
 * This IS safe to call at interrupt time.
 * (Typically called at SPLBIO)
 */
void *
dvma_mapin(kva, len, canwait)
	void *kva;
	int len;
	int canwait; /* ignored */
{
	vaddr_t seg_kva, seg_dma;
	vsize_t seg_len, seg_off;
	vaddr_t v, x;
	int s, sme, error;

	/* Get seg-aligned address and length. */
	seg_kva = (vaddr_t)kva;
	seg_len = (vsize_t)len;
	seg_off = seg_kva & SEGOFSET;
	seg_kva -= seg_off;
	seg_len = m68k_round_seg(seg_len + seg_off);

	s = splvm();

	/* Allocate the DVMA segment(s) */

	error = extent_alloc(dvma_extent, seg_len, NBSG, 0,
	    EX_FAST | EX_NOWAIT | EX_MALLOCOK, &seg_dma);
	if (error) {
		splx(s);
		return (NULL);
	}

#ifdef	DIAGNOSTIC
	if (seg_dma & SEGOFSET)
		panic("dvma_mapin: seg not aligned");
#endif

	/* Duplicate the mappings into DMA space. */
	v = seg_kva;
	x = seg_dma;
	while (seg_len > 0) {
		sme = get_segmap(v);
#ifdef	DIAGNOSTIC
		if (sme == SEGINV)
			panic("dvma_mapin: seg not mapped");
#endif
#ifdef	HAVECACHE
		/* flush write-back on old mappings */
		if (cache_size)
			cache_flush_segment(v);
#endif
		set_segmap_allctx(x, sme);
		v += NBSG;
		x += NBSG;
		seg_len -= NBSG;
	}
	seg_dma += seg_off;

	splx(s);
	return ((caddr_t)seg_dma);
}

/*
 * Free some DVMA space allocated by the above.
 * This IS safe to call at interrupt time.
 * (Typically called at SPLBIO)
 */
void
dvma_mapout(dma, len)
	void *dma;
	int len;
{
	vaddr_t seg_dma;
	vsize_t seg_len, seg_off;
	vaddr_t v, x;
	int sme;
	int s;

	/* Get seg-aligned address and length. */
	seg_dma = (vaddr_t)dma;
	seg_len = (vsize_t)len;
	seg_off = seg_dma & SEGOFSET;
	seg_dma -= seg_off;
	seg_len = m68k_round_seg(seg_len + seg_off);

	s = splvm();

	/* Flush cache and remove DVMA mappings. */
	v = seg_dma;
	x = v + seg_len;
	while (v < x) {
		sme = get_segmap(v);
#ifdef	DIAGNOSTIC
		if (sme == SEGINV)
			panic("dvma_mapout: seg not mapped");
#endif
#ifdef	HAVECACHE
		/* flush write-back on the DVMA mappings */
		if (cache_size)
			cache_flush_segment(v);
#endif
		set_segmap_allctx(v, SEGINV);
		v += NBSG;
	}

	if (extent_free(dvma_extent, seg_dma, seg_len,
	    EX_NOWAIT | EX_MALLOCOK))
		panic("dvma_mapout: unable to free 0x%lx,0x%lx",
		    seg_dma, seg_len);
	splx(s);
}
