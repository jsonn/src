/*	$NetBSD: mcclock.c,v 1.1.6.2 2000/11/20 20:23:06 bouyer Exp $	*/

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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/clock_subr.h>

#include <dev/ic/mc146818reg.h>

#include <prep/prep/clockvar.h>
#include <prep/prep/mcclockvar.h>

/*
 * XXX rate is machine-dependent.
 */
#define	MC_DFEAULTRATE	MC_RATE_1024_Hz

void	mcclock_init __P((struct device *));
void	mcclock_get __P((struct device *, time_t, struct clocktime *));
void	mcclock_set __P((struct device *, struct clocktime *));

const struct clockfns mcclock_clockfns = {
	mcclock_init, mcclock_get, mcclock_set,
};

#define	mc146818_write(dev, reg, datum)					\
	    (*(dev)->sc_busfns->mc_bf_write)(dev, reg, datum)
#define	mc146818_read(dev, reg)						\
	    (*(dev)->sc_busfns->mc_bf_read)(dev, reg)

void
mcclock_attach(sc, busfns)
	struct mcclock_softc *sc;
	const struct mcclock_busfns *busfns;
{

	printf(": mc146818 or compatible");

	sc->sc_busfns = busfns;

	/* Turn interrupts off, just in case. */
	mc146818_write(sc, MC_REGB, MC_REGB_24HR);

	clockattach(&sc->sc_dev, &mcclock_clockfns);
}

void
mcclock_init(dev)
	struct device *dev;
{
	struct mcclock_softc *sc = (struct mcclock_softc *)dev;

	mc146818_write(sc, MC_REGA, MC_BASE_32_KHz | MC_DFEAULTRATE);
	mc146818_write(sc, MC_REGB, MC_REGB_24HR);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
mcclock_get(dev, base, ct)
	struct device *dev;
	time_t base;
	struct clocktime *ct;
{
	struct mcclock_softc *sc = (struct mcclock_softc *)dev;
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(sc, &regs)
	splx(s);

	ct->sec = FROMBCD(regs[MC_SEC]);
	ct->min = FROMBCD(regs[MC_MIN]);
	ct->hour = FROMBCD(regs[MC_HOUR]);
	ct->dow = FROMBCD(regs[MC_DOW]);
	ct->day = FROMBCD(regs[MC_DOM]);
	ct->mon = FROMBCD(regs[MC_MONTH]);
	ct->year = FROMBCD(regs[MC_YEAR]);
}

/*
 * Reset the TODR based on the time value.
 */
void
mcclock_set(dev, ct)
	struct device *dev;
	struct clocktime *ct;
{
	struct mcclock_softc *sc = (struct mcclock_softc *)dev;
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(sc, &regs);
	splx(s);

	regs[MC_SEC] = TOBCD(ct->sec);
	regs[MC_MIN] = TOBCD(ct->min);
	regs[MC_HOUR] = TOBCD(ct->hour);
	regs[MC_DOW] = TOBCD(ct->dow);
	regs[MC_DOM] = TOBCD(ct->day);
	regs[MC_MONTH] = TOBCD(ct->mon);
	regs[MC_YEAR] = TOBCD(ct->year);

	s = splclock();
	MC146818_PUTTOD(sc, &regs);
	splx(s);
}
