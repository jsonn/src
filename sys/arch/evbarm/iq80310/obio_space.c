/*	$NetBSD: obio_space.c,v 1.1.4.2 2002/04/01 07:39:48 nathanw Exp $	*/

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
 * bus_space functions for IQ80310 on-board devices
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include "iq80310reg.h"

/* Prototypes for all the bus_space structure functions */
bs_protos(obio);
bs_protos(bs_notimpl);

/*
 * The obio bus space tag.  This is constant for all instances, so
 * we never have to explicitly "create" it.
 */
struct bus_space obio_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	obio_bs_map,
	obio_bs_unmap,
	obio_bs_subregion,

	/* allocation/deallocation */
	obio_bs_alloc,
	obio_bs_free,

	/* get kernel virtual address */
	obio_bs_vaddr,

	/* mmap */
	bs_notimpl_bs_mmap,

	/* barrier */
	obio_bs_barrier,

	/* read (single) */
	obio_bs_r_1,
	bs_notimpl_bs_r_2,
	obio_bs_r_4,
	bs_notimpl_bs_r_8,

	/* read multiple */
	obio_bs_rm_1,
	bs_notimpl_bs_rm_2,
	bs_notimpl_bs_rm_4,
	bs_notimpl_bs_rm_8,

	/* read region */
	obio_bs_rr_1,
	bs_notimpl_bs_rr_2,
	bs_notimpl_bs_rr_4,
	bs_notimpl_bs_rr_8,

	/* write (single) */
	obio_bs_w_1,
	bs_notimpl_bs_w_2,
	obio_bs_w_4,
	bs_notimpl_bs_w_8,

	/* write multiple */
	obio_bs_wm_1,
	bs_notimpl_bs_wm_2,
	bs_notimpl_bs_wm_4,
	bs_notimpl_bs_wm_8,

	/* write region */
	bs_notimpl_bs_wr_1,
	bs_notimpl_bs_wr_2,
	bs_notimpl_bs_wr_4,
	bs_notimpl_bs_wr_8,

	/* set multiple */
	bs_notimpl_bs_sm_1,
	bs_notimpl_bs_sm_2,
	bs_notimpl_bs_sm_4,
	bs_notimpl_bs_sm_8,

	/* set region */
	bs_notimpl_bs_sr_1,
	bs_notimpl_bs_sr_2,
	bs_notimpl_bs_sr_4,
	bs_notimpl_bs_sr_8,

	/* copy */
	bs_notimpl_bs_c_1,
	bs_notimpl_bs_c_2,
	bs_notimpl_bs_c_4,
	bs_notimpl_bs_c_8,
};

int
obio_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	u_long startpa, endpa, pa;
	vaddr_t va;
	pt_entry_t *pte;

	if (bpa > IQ80310_OBIO_BASE) {

		/*
		 * IQ80310 on-board devices are mapped VA==PA.  All addresses
		 * we're provided, therefore, don't need any additional mapping.
		 */
		*bshp = bpa;

	} else {

		/*
		 * Some devices actually lie outside the range above.
		 * Notably: flash.
		 */
		startpa = trunc_page(bpa);
		endpa = round_page(bpa + size);

		/* XXX use extent manager to check duplicate mapping */

		va = uvm_km_valloc(kernel_map, endpa - startpa);
		if (! va)
			return(ENOMEM);

		*bshp = (bus_space_handle_t)(va + (bpa - startpa));

		for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
			pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE);
			pte = vtopte(va);
			*pte &= ~PT_CACHEABLE;
		}
		pmap_update(pmap_kernel());

	}

	return 0;
}

int
obio_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend, bus_size_t size,
    bus_size_t alignment, bus_size_t boundary, int flags, bus_addr_t *bpap,
    bus_space_handle_t *bshp)
{

	panic("obio_bs_alloc(): not implemented\n");
}


void
obio_bs_unmap(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	/* Nothing to do. */
	/* XXX -- technically, if we alloc and map above, we should
	 * unmap and free here, but we bail on this for now.
	 */
}

void    
obio_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{

	panic("obio_bs_free(): not implemented\n");
}

int
obio_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}

void *
obio_bs_vaddr(void *t, bus_space_handle_t bsh)
{

	return ((void *)bsh);
}

void
obio_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{

	/* Nothing to do. */
}
