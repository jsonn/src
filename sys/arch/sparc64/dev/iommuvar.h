/*	$NetBSD: iommuvar.h,v 1.6.4.1 2001/08/03 04:12:26 lukem Exp $	*/

/*
 * Copyright (c) 1999 Matthew R. Green
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

#ifndef _SPARC64_DEV_IOMMUVAR_H_
#define _SPARC64_DEV_IOMMUVAR_H_

/*
 * per-IOMMU state
 */
struct iommu_state {
	paddr_t			is_ptsb;	/* TSB physical address */
	int64_t			*is_tsb;	/* TSB virtual address */
	int			is_tsbsize;	/* 0 = 8K, ... */
	u_int			is_dvmabase;
	int64_t			is_cr;		/* IOMMU control regiter value */
	struct extent		*is_dvmamap;	/* DVMA map for this instance */

	paddr_t			is_flushpa;	/* used to flush the SBUS */
	/* Needs to be volatile or egcs optimizes away loads */
	volatile int64_t	is_flush;

	/* copies of our parents state, to allow us to be self contained */
	bus_space_tag_t		is_bustag;	/* our bus tag */
	struct iommureg		*is_iommu;	/* IOMMU registers */
	struct iommu_strbuf	*is_sb;		/* streaming buffer */
};

/* interfaces for PCI/SBUS code */
void	iommu_init __P((char *, struct iommu_state *, int, u_int32_t));
void	iommu_reset __P((struct iommu_state *));
void    iommu_enter __P((struct iommu_state *, vaddr_t, int64_t, int));
void    iommu_remove __P((struct iommu_state *, vaddr_t, size_t));

int	iommu_dvmamap_load __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, void *, bus_size_t, struct proc *, int));
void	iommu_dvmamap_unload __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t));
int	iommu_dvmamap_load_raw __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, bus_dma_segment_t *, int, int, bus_size_t));
void	iommu_dvmamap_sync __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dmamap_t, bus_addr_t, bus_size_t, int));
int	iommu_dvmamem_alloc __P((bus_dma_tag_t, struct iommu_state *,
	    bus_size_t, bus_size_t, bus_size_t, bus_dma_segment_t *,
	    int, int *, int));
void	iommu_dvmamem_free __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dma_segment_t *, int));
int	iommu_dvmamem_map __P((bus_dma_tag_t, struct iommu_state *,
	    bus_dma_segment_t *, int, size_t, caddr_t *, int));
void	iommu_dvmamem_unmap __P((bus_dma_tag_t, struct iommu_state *,
	    caddr_t, size_t));

#endif /* _SPARC64_DEV_IOMMUVAR_H_ */
