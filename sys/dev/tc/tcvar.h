/* $NetBSD: tcvar.h,v 1.14.8.1 2000/11/20 11:43:17 bouyer Exp $ */

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

#ifndef __DEV_TC_TCVAR_H__
#define __DEV_TC_TCVAR_H__

/*
 * Definitions for TURBOchannel autoconfiguration.
 */

#include <machine/bus.h>
#include <dev/tc/tcreg.h>

/*
 * Machine-dependent definitions.
 */
#include <machine/tc_machdep.h>

/*
 * In the long run, the following block will go completely away.
 * For now, the MI TC code still uses the old TC_IPL_ names
 * and not the new IPL_ names.
 */
#if 1
/*
 * Map the new definitions to the old.
 */
#include <machine/intr.h>

#define tc_intrlevel_t	int

#define	TC_IPL_NONE	IPL_NONE
#define	TC_IPL_BIO	IPL_BIO
#define	TC_IPL_NET	IPL_NET
#define	TC_IPL_TTY	IPL_TTY
#define	TC_IPL_CLOCK	IPL_CLOCK
#endif /* 1 */

struct tc_softc {
	struct	device sc_dv;

	int	sc_speed;
	int	sc_nslots;
	struct tc_slotdesc *sc_slots;

	const struct evcnt *(*sc_intr_evcnt)(struct device *, void *);
	void	(*sc_intr_establish)(struct device *, void *,
			int, int (*)(void *), void *);
	void	(*sc_intr_disestablish)(struct device *, void *);
	bus_dma_tag_t (*sc_get_dma_tag)(int);
};

/*
 * Arguments used to attach TURBOchannel busses.
 */
struct tcbus_attach_args {
	char		*tba_busname;		/* XXX should be common */
	bus_space_tag_t tba_memt;

	/* Bus information */
	u_int		tba_speed;		/* see TC_SPEED_* below */
	u_int		tba_nslots;
	struct tc_slotdesc *tba_slots;
	u_int		tba_nbuiltins;
	const struct tc_builtin *tba_builtins;
	

	/* TC bus resource management; XXX will move elsewhere eventually. */
	const struct evcnt *(*tba_intr_evcnt)(struct device *, void *);
	void	(*tba_intr_establish)(struct device *, void *,
			int, int (*)(void *), void *);
	void	(*tba_intr_disestablish)(struct device *, void *);
	bus_dma_tag_t (*tba_get_dma_tag)(int);
};

/*
 * Arguments used to attach TURBOchannel devices.
 */
struct tc_attach_args {
	bus_space_tag_t ta_memt;
	bus_dma_tag_t	ta_dmat;

	char		ta_modname[TC_ROM_LLEN+1];
	u_int		ta_slot;
	tc_offset_t	ta_offset;
	tc_addr_t	ta_addr;
	void		*ta_cookie;
	u_int		ta_busspeed;		/* see TC_SPEED_* below */
};

/*
 * Description of TURBOchannel slots, provided by machine-dependent
 * code to the TURBOchannel bus driver.
 */
struct tc_slotdesc {
	tc_addr_t	tcs_addr;
	void		*tcs_cookie;
	int		tcs_used;
};

/*
 * Description of built-in TURBOchannel devices, provided by
 * machine-dependent code to the TURBOchannel bus driver.
 */
struct tc_builtin {
	char		*tcb_modname;
	u_int		tcb_slot;
	tc_offset_t	tcb_offset;
	void		*tcb_cookie;
};

/*
 * Interrupt establishment functions.
 */
int	tc_checkslot(tc_addr_t, char *);
void	tc_devinfo(const char *, char *);
void	tcattach(struct device *, struct device *, void *);
const struct evcnt *tc_intr_evcnt(struct device *, void *);
void	tc_intr_establish(struct device *, void *, int, int (*)(void *),
	    void *);
void	tc_intr_disestablish(struct device *, void *);

#include "locators.h"
/*
 * Easy to remember names for TURBOchannel device locators.
 */
#define	tccf_slot	cf_loc[TCCF_SLOT]		/* slot */
#define	tccf_offset	cf_loc[TCCF_OFFSET]		/* offset */

#define	TCCF_SLOT_UNKNOWN	TCCF_SLOT_DEFAULT
#define	TCCF_OFFSET_UNKNOWN	TCCF_OFFSET_DEFAULT

/*
 * Miscellaneous definitions.
 */
#define	TC_SPEED_12_5_MHZ	0		/* 12.5MHz TC bus */
#define	TC_SPEED_25_MHZ		1		/* 25MHz TC bus */

#endif /* __DEV_TC_TCVAR_H__ */
