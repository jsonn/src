/*	$NetBSD: ioasicvar.h,v 1.10.8.1 1999/12/27 18:35:37 wrstuden Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

/*
 * IOASIC subdevice attachment information.
 */

/* motherboard-specific autoconfiguration tables of ioasic child devices. */
struct ioasic_dev {
	char		*iad_modname;
	tc_offset_t	iad_offset;
	void		*iad_cookie;
	u_int32_t	iad_intrbits;
};

/* Attachment arguments. */
struct ioasicdev_attach_args {
	char	iada_modname[TC_ROM_LLEN];
	tc_offset_t iada_offset;
	tc_addr_t iada_addr;
	void	*iada_cookie;
};

/* Device locators. */
#include "locators.h"
#define	ioasiccf_offset	cf_loc[IOASICCF_OFFSET]		/* offset */

#define	IOASIC_OFFSET_UNKNOWN	IOASICCF_OFFSET_DEFAULT

struct ioasic_softc {
	struct	device sc_dv;

	tc_addr_t sc_base;
	void	*sc_cookie;

	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_lance_dmam;
};

extern struct cfdriver ioasic_cd;

/*
 * XXX Some drivers need direct access to IOASIC registers.
 */
extern tc_addr_t ioasic_base;


/*
 * Interrupt establishment/disestablishment functions
 */
void    ioasic_intr_establish __P((struct device *, void *,
	    int, int (*)(void *), void *));
void    ioasic_intr_disestablish __P((struct device *, void *));


/*
 * Miscellaneous helper functions.
 */
int	ioasicprint __P((void *, const char *));
int	ioasic_submatch __P((struct cfdata *, struct ioasicdev_attach_args *));
void	ioasic_attach_devs __P((struct ioasic_softc *sc, 
	    struct ioasic_dev *ioasic_devs, int ioasic_ndevs));
