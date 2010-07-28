/* $Id: imx31_space.c,v 1.3.2.5 2010/07/28 04:16:12 uebayasi Exp $ */

/* derived from: */
/*	$NetBSD: imx31_space.c,v 1.3.2.5 2010/07/28 04:16:12 uebayasi Exp $ */

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * bus_space(9) support for Freescale iMX31 processor
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <uvm/uvm_extern.h>
#include <machine/bus.h>

bs_protos(imx31);
bs_protos(generic);
bs_protos(generic_armv4);
bs_protos(bs_notimpl);

struct bus_space imx31_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	imx31_bs_map,
	imx31_bs_unmap,
	imx31_bs_subregion,

	/* allocation/deallocation */
	imx31_bs_alloc,	/* not implemented */
	imx31_bs_free,		/* not implemented */

	/* get kernel virtual address */
	imx31_bs_vaddr,

	/* mmap */
	imx31_bs_mmap,

	/* barrier */
	imx31_bs_barrier,

	/* read (single) */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	generic_bs_rm_1,
	generic_armv4_bs_rm_2,
	generic_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	generic_bs_rr_1,
	generic_armv4_bs_rr_2,
	generic_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	generic_bs_wm_1,
	generic_armv4_bs_wm_2,
	generic_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	generic_bs_wr_1,
	generic_armv4_bs_wr_2,
	generic_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	generic_bs_sr_1,
	generic_armv4_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	generic_armv4_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,

	/* physload */
	imx31_bs_physload,
	imx31_bs_physunload,
	imx31_bs_physload_device,
	imx31_bs_physunload_device,
};

int
imx31_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
	      int flag, bus_space_handle_t *bshp)
{
	u_long startpa, endpa, pa;
	vaddr_t va;
	pt_entry_t *pte;
	const struct pmap_devmap	*pd;

	if ((pd = pmap_devmap_find_pa(bpa, size)) != NULL) {
		/* Device was statically mapped. */
		*bshp = pd->pd_va + (bpa - pd->pd_pa);
		return 0;
	}

	startpa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	/* XXX use extent manager to check duplicate mapping */

	va = uvm_km_alloc(kernel_map, endpa - startpa, 0,
	    UVM_KMF_VAONLY | UVM_KMF_NOWAIT);
	if (! va)
		return(ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE, 0);
		if ((flag & BUS_SPACE_MAP_CACHEABLE) == 0) {
			pte = vtopte(va);
			*pte &= ~L2_S_CACHE_MASK;
			PTE_SYNC(pte);
			/* XXX: pmap_kenter_pa() also does PTE_SYNC(). a bit of
			 *      waste.
			 */
		}
	}
	pmap_update(pmap_kernel());

	return(0);
}

void
imx31_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t	va;
	vsize_t	sz;

	if (pmap_devmap_find_va(bsh, size) != NULL) {
		/* Device was statically mapped; nothing to do. */
		return;
	}

	va = trunc_page(bsh);
	sz = round_page(bsh + size) - va;

	pmap_kremove(va, sz);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, sz, UVM_KMF_VAONLY);
}


int
imx31_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void
imx31_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}

void *
imx31_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	return ((void *)bsh);
}


int
imx31_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("imx31_io_bs_alloc(): not implemented\n");
}

void    
imx31_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("imx31_io_bs_free(): not implemented\n");
}

paddr_t
imx31_bs_mmap(void *t, bus_addr_t addr, off_t off, int prot, int flags)
{

	/* XXX */
	return arm_btop(addr + off);
}

/* XXX generic */

void *
imx31_bs_physload(void *t, bus_addr_t addr, bus_size_t size, int freelist)
{
	const paddr_t start = imx31_bs_mmap(t, addr, 0, VM_PROT_ALL, 0);
	const paddr_t end = imx31_bs_mmap(t, addr + size, 0, VM_PROT_ALL, 0);

	return uvm_page_physload(start, end, start, end, freelist);
}

void
imx31_bs_physunload(void *t, void *phys)
{

	uvm_page_physunload(phys);
}

void *
imx31_bs_physload_device(void *t, bus_addr_t addr, bus_size_t size, int prot, int flags)
{
	const paddr_t start = imx31_bs_mmap(t, addr, 0, prot, flags);
	const paddr_t end = imx31_bs_mmap(t, addr + size, 0, prot, flags);

	return uvm_page_physload_device(start, end, start, end, prot, flags);
}

void
imx31_bs_physunload_device(void *t, void *phys)
{

	uvm_page_physunload_device(phys);
}
