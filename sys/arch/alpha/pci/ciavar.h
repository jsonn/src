/* $NetBSD: ciavar.h,v 1.6.2.1 1997/06/01 04:13:10 cgd Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>

/*
 * A 21171 chipset's configuration.
 *
 * All of the information that the chipset-specific functions need to
 * do their dirty work (and more!).
 */
struct cia_config {
	int	cc_initted;

	bus_space_tag_t cc_iot, cc_memt;
	struct alpha_pci_chipset cc_pc;

	u_int32_t cc_hae_mem;
	u_int32_t cc_hae_io;

	struct extent *cc_io_ex, *cc_d_mem_ex, *cc_s_mem_ex;
	int	cc_mallocsafe;
};

struct cia_softc {
	struct	device sc_dev;

	struct	cia_config *sc_ccp;
	/* XXX SGMAP info */
};

void	cia_init __P((struct cia_config *, int));
void	cia_pci_init __P((pci_chipset_tag_t, void *));

bus_space_tag_t	cia_bus_io_init __P((void *));
bus_space_tag_t	cia_bus_mem_init __P((void *));
