/* $NetBSD: a12cvar.h,v 1.2.48.2 2004/09/18 14:31:12 skrll Exp $ */

/* [Notice revision 2.0]
 * Copyright (c) 1997 Avalon Computer Systems, Inc.
 * All rights reserved.
 *
 * Author: Ross Harvey
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright and
 *    author notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Avalon Computer Systems, Inc. nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. This copyright will be assigned to The NetBSD Foundation on
 *    1/1/2000 unless these terms (including possibly the assignment
 *    date) are updated in writing by Avalon prior to the latest specified
 *    assignment date.
 *
 * THIS SOFTWARE IS PROVIDED BY AVALON COMPUTER SYSTEMS, INC. AND CONTRIBUTORS
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

#ifndef	_ALPHA_PCI_A12CVAR_H_
#define	_ALPHA_PCI_A12CVAR_H_

#define	A12CVAR() 	/* generate ctags(1) key */

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/pci_sgmap_pte64.h>
/*
 * A12 Core Logic -a12c- configuration.
 */
struct a12c_config {
	int	ac_initted;

	bus_space_tag_t ac_iot, ac_memt;
	struct alpha_pci_chipset ac_pc;

	struct alpha_bus_dma_tag ac_dmat_direct;
	struct alpha_bus_dma_tag ac_dmat_sgmap;

	struct alpha_sgmap ac_sgmap;

	u_int32_t ac_hae_mem;
	u_int32_t ac_hae_io;

	struct extent *ac_io_ex, *ac_d_mem_ex, *ac_s_mem_ex;
	int	ac_mallocsafe;
};

struct a12c_softc {
	struct	device sc_dev;

	struct	a12c_config *sc_ccp;
};

void	a12c_init __P((struct a12c_config *, int));
void	a12c_pci_init __P((pci_chipset_tag_t, void *));
void	a12c_dma_init __P((struct a12c_config *));

bus_space_tag_t	a12c_bus_io_init __P((void *));
bus_space_tag_t	a12c_bus_mem_init __P((void *));
/*
 * The following two CPU numbers are relative to the local switch;
 * in a multistage system these numbers will not be unique. a12_ethercpu
 * is used to route bootp packets and to mount the root FS.
 */
int	a12_cpu_local;	/* our CPU number relative to the local switch */
int	a12_cpu_ether;	/* relative CPU number with outside world access */
int	a12_cpu_global;	/* XXX used to construct MSN routing tables XXX */

void	a12_intr_register_xb	__P((int (*)(void *)));
void	a12_intr_register_icw	__P((int (*)(void *)));
void	configure_xb		__P((void));
void	configure_a12dc		__P((void));

#endif
