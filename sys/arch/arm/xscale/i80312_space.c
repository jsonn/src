/*	$NetBSD: i80312_space.c,v 1.1.2.2 2001/11/12 21:16:35 thorpej Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
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
 * bus_space functions for i80312 Companion I/O chip.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <arm/xscale/i80312var.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(i80312);
bs_protos(i80312_io);
bs_protos(i80312_mem);
bs_protos(bs_notimpl);

/*
 * Template bus_space -- copied, and the bits that are NULL are
 * filled in.
 */
const struct bus_space i80312_bs_tag_template = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	NULL,
	NULL,
	i80312_bs_subregion,

	/* allocation/deallocation */
	NULL,
	NULL,

	/* get kernel virtual address */
	i80312_bs_vaddr,

	/* mmap */
	i80312_bs_mmap,

	/* barrier */
	i80312_bs_barrier,

	/* read (single) */
	i80312_bs_r_1,
	i80312_bs_r_2,
	i80312_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	i80312_bs_rm_1,
	i80312_bs_rm_2,
	i80312_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	bs_notimpl_bs_rr_1,
	i80312_bs_rr_2,
	i80312_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	i80312_bs_w_1,
	i80312_bs_w_2,
	i80312_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	i80312_bs_wm_1,
	i80312_bs_wm_2,
	i80312_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	bs_notimpl_bs_wr_1,
	i80312_bs_wr_2,
	i80312_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	i80312_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	i80312_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

void
i80312_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80312_bs_tag_template;
	bs->bs_cookie = cookie;
}

void
i80312_io_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80312_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i80312_io_bs_map;
	bs->bs_unmap = i80312_io_bs_unmap;
	bs->bs_alloc = i80312_io_bs_alloc;
	bs->bs_free = i80312_io_bs_free;

	bs->bs_vaddr = i80312_io_bs_vaddr;
}

void
i80312_mem_bs_init(bus_space_tag_t bs, void *cookie)
{

	*bs = i80312_bs_tag_template;
	bs->bs_cookie = cookie;

	bs->bs_map = i80312_mem_bs_map;
	bs->bs_unmap = i80312_mem_bs_unmap;
	bs->bs_alloc = i80312_mem_bs_alloc;
	bs->bs_free = i80312_mem_bs_free;

	bs->bs_mmap = i80312_mem_bs_mmap;
}

/* *** Routines shared by i80312, PCI IO, and PCI MEM. *** */

int
i80312_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void
i80312_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}

void *
i80312_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	return ((void *)bsh);
}

paddr_t
i80312_bs_mmap(void *t, bus_addr_t addr, off_t off, int prot, int flags)
{

	/* Not supported. */
	return (-1);
}

/* *** Routines for PCI IO. *** */

int
i80312_io_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	struct i80312_softc *sc = t;
	vaddr_t winvaddr;
	uint32_t busbase, bussize;

	if (bpa >= sc->sc_pioout_base &&
	    bpa < (sc->sc_pioout_base + sc->sc_pioout_size)) {
		busbase = sc->sc_pioout_base;
		bussize = sc->sc_pioout_size;
		winvaddr = sc->sc_piow_vaddr;
	} else if (bpa >= sc->sc_sioout_base &&
		   bpa < (sc->sc_sioout_base + sc->sc_sioout_size)) {
		busbase = sc->sc_sioout_base;
		bussize = sc->sc_sioout_size;
		winvaddr = sc->sc_siow_vaddr;
	} else
		return (EINVAL);

	if ((bpa + size) >= (busbase + bussize))
		return (EINVAL);

	/*
	 * Found the window -- PCI I/O space is mapped at a fixed
	 * virtual address by board-specific code.  Translate the
	 * bus address to the virtual address.
	 */
	*bshp = winvaddr + (bpa - busbase);

	return (0);
}

void
i80312_io_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	/* Nothing to do. */
}

int
i80312_io_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80312_io_bs_alloc(): not implemented\n");
}

void    
i80312_io_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80312_io_bs_free(): not implemented\n");
}

void *
i80312_io_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	/* Not supported. */
	return (NULL);
}

/* *** Routines for PCI MEM. *** */

int
i80312_mem_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{

	struct i80312_softc *sc = t;
	vaddr_t va;
	uint32_t busbase, bussize;
	paddr_t pa, endpa;

	if (bpa >= sc->sc_pmemout_base &&
	    bpa < (sc->sc_pmemout_base + sc->sc_pmemout_size)) {
		busbase = sc->sc_pmemout_base;
		bussize = sc->sc_pmemout_size;
	} else if (bpa >= sc->sc_smemout_base &&
		   bpa < (sc->sc_smemout_base + sc->sc_smemout_size)) {
		busbase = sc->sc_smemout_base;
		bussize = sc->sc_smemout_size;
	} else
		return (EINVAL);

	if ((bpa + size) >= (busbase + bussize))
		return (EINVAL);

	/*
	 * Found the window -- PCI MEM space is not mapped by allocating
	 * some kernel VA space and mapping the pages with pmap_enter().
	 * pmap_enter() will map unmanaged pages as non-cacheable.
	 */
	pa = trunc_page(bpa - busbase);
	endpa = round_page((bpa - busbase) + size);

	va = uvm_km_valloc(kernel_map, endpa - pa);
	if (va == 0)
		return (ENOMEM);

	*bshp = va + (bpa & PAGE_MASK);

	for (; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
	}
	pmap_update(pmap_kernel());

	return (0);
}

void
i80312_mem_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	vaddr_t va, endva;

	va = trunc_page(bsh);
	endva = round_page(bsh + size);

	/* Free the kernel virtual mapping. */
	uvm_km_free(kernel_map, va, endva - va);
}

int
i80312_mem_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	panic("i80312_mem_bs_alloc(): not implemented\n");
}

void    
i80312_mem_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("i80312_mem_bs_free(): not implemented\n");
}

paddr_t
i80312_mem_bs_mmap(void *t, bus_addr_t addr, off_t off, int prot, int flags)
{

	/* XXX */
	return (-1);
}
