/*	$NetBSD: mcclock_ioasic.c,v 1.2.2.2 1996/12/08 00:31:41 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <alpha/alpha/clockvar.h>
#include <alpha/alpha/mcclockvar.h>
#include <dev/ic/mc146818reg.h>
#include <dev/tc/tcreg.h>
#include <dev/tc/tcvar.h> 
#include <dev/tc/ioasicvar.h>                   /* XXX */

struct mcclock_ioasic_clockdatum {
	u_char	datum;
	char	pad[3];
};

struct mcclock_ioasic_softc {
	struct mcclock_softc	sc_mcclock;

	struct mcclock_ioasic_clockdatum *sc_dp;
};

int	mcclock_ioasic_match __P((struct device *, struct cfdata *, void *));
void	mcclock_ioasic_attach __P((struct device *, struct device *, void *));

struct cfattach mcclock_ioasic_ca = {
	sizeof (struct mcclock_ioasic_softc), mcclock_ioasic_match,
	    mcclock_ioasic_attach, 
};

void	mcclock_ioasic_write __P((struct mcclock_softc *, u_int, u_int));
u_int	mcclock_ioasic_read __P((struct mcclock_softc *, u_int));

const struct mcclock_busfns mcclock_ioasic_busfns = {
	mcclock_ioasic_write, mcclock_ioasic_read,
};

int
mcclock_ioasic_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;

	if (strncmp("TOY_RTC ", d->iada_modname, TC_ROM_LLEN))
		return (0);

	return (1);
}

void
mcclock_ioasic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasicdev_attach_args *ioasicdev = aux;
	struct mcclock_ioasic_softc *sc = (struct mcclock_ioasic_softc *)self;

	sc->sc_dp = (struct mcclock_ioasic_clockdatum *)ioasicdev->iada_addr;

	mcclock_attach(&sc->sc_mcclock, &mcclock_ioasic_busfns);
}

void
mcclock_ioasic_write(dev, reg, datum)
	struct mcclock_softc *dev;
	u_int reg, datum;
{
	struct mcclock_ioasic_softc *sc = (struct mcclock_ioasic_softc *)dev;

	sc->sc_dp[reg].datum = datum;
}

u_int
mcclock_ioasic_read(dev, reg)
	struct mcclock_softc *dev;
	u_int reg;
{
	struct mcclock_ioasic_softc *sc = (struct mcclock_ioasic_softc *)dev;

	return (sc->sc_dp[reg].datum);
}
