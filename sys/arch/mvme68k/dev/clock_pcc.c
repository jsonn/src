/*	$NetBSD: clock_pcc.c,v 1.5.24.2 2000/03/18 13:51:59 scw Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
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

/*
 * Glue for the Peripheral Channel Controller timers and the
 * Mostek clock chip found on the MVME-147.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/psl.h>
#include <machine/bus.h>

#include <mvme68k/mvme68k/clockreg.h>
#include <mvme68k/mvme68k/clockvar.h>
#include <mvme68k/dev/pccreg.h>
#include <mvme68k/dev/pccvar.h>

int clock_pcc_match __P((struct device *, struct cfdata *, void *));
void clock_pcc_attach __P((struct device *, struct device *, void *));

struct clock_pcc_softc {
	struct device sc_dev;
	struct clock_attach_args sc_clock_args;
	u_char sc_clock_lvl;
};

struct cfattach clock_pcc_ca = {
	sizeof(struct clock_pcc_softc), clock_pcc_match, clock_pcc_attach
};

extern struct cfdriver clock_cd;


static int clock_pcc_profintr __P((void *));
static int clock_pcc_statintr __P((void *));
static void clock_pcc_initclocks __P((void *, int, int));
static void clock_pcc_shutdown __P((void *));

static struct clock_pcc_softc *clock_pcc_sc;

/* ARGSUSED */
int
clock_pcc_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pcc_attach_args *pa;

	pa = aux;

	/* Only one clock, please. */
	if (clock_pcc_sc)
		return (0);

	if (strcmp(pa->pa_name, clock_cd.cd_name))
		return (0);

	pa->pa_ipl = cf->pcccf_ipl;

	return (1);
}

/* ARGSUSED */
void
clock_pcc_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pcc_attach_args *pa;
	struct clock_pcc_softc *sc;

	sc = (struct clock_pcc_softc *) self;
	pa = aux;

	if (pa->pa_ipl != CLOCK_LEVEL)
		panic("clock_pcc_attach: wrong interrupt level");

	clock_pcc_sc = sc;

	/* Map the RTC's registers */
	sc->sc_clock_args.ca_bust = pa->pa_bust;
	bus_space_map(pa->pa_bust, pa->pa_offset,
	    MK48T_REGSIZE, 0, &sc->sc_clock_args.ca_bush);

	sc->sc_clock_args.ca_arg = sc;
	sc->sc_clock_args.ca_initfunc = clock_pcc_initclocks;

	/* Do common portions of clock config. */
	clock_config(self, &sc->sc_clock_args);

	/* Ensure our interrupts get disabled at shutdown time. */
	(void) shutdownhook_establish(clock_pcc_shutdown, NULL);

	/* Attach the interrupt handlers. */
	pccintr_establish(PCCV_TIMER1, clock_pcc_profintr, pa->pa_ipl, NULL);
	pccintr_establish(PCCV_TIMER2, clock_pcc_statintr, pa->pa_ipl, NULL);
	sc->sc_clock_lvl = pa->pa_ipl | PCC_IENABLE | PCC_TIMERACK;
}

void
clock_pcc_initclocks(arg, proftick, stattick)
	void *arg;
	int proftick;
	int stattick;
{
	struct clock_pcc_softc *sc = arg;

	pcc_reg_write16(sys_pcc, PCCREG_TMR1_PRELOAD,
	    pcc_timer_us2lim(proftick));
	pcc_reg_write(sys_pcc, PCCREG_TMR1_CONTROL, PCC_TIMERCLEAR);
	pcc_reg_write(sys_pcc, PCCREG_TMR1_CONTROL, PCC_TIMERSTART);
	pcc_reg_write(sys_pcc, PCCREG_TMR1_INTR_CTRL, sc->sc_clock_lvl);

	pcc_reg_write16(sys_pcc, PCCREG_TMR2_PRELOAD,
	    pcc_timer_us2lim(stattick));
	pcc_reg_write(sys_pcc, PCCREG_TMR2_CONTROL, PCC_TIMERCLEAR);
	pcc_reg_write(sys_pcc, PCCREG_TMR2_CONTROL, PCC_TIMERSTART);
	pcc_reg_write(sys_pcc, PCCREG_TMR2_INTR_CTRL, sc->sc_clock_lvl);
}

int
clock_pcc_profintr(frame)
	void *frame;
{

	pcc_reg_write(sys_pcc, PCCREG_TMR1_INTR_CTRL,
	    clock_pcc_sc->sc_clock_lvl);
	hardclock(frame);
	clock_profcnt.ev_count++;
	return (1);
}

int
clock_pcc_statintr(frame)
	void *frame;
{

	/* Disable the timer interrupt while we handle it. */
	pcc_reg_write(sys_pcc, PCCREG_TMR2_INTR_CTRL, 0);

	statclock((struct clockframe *) frame);

	pcc_reg_write16(sys_pcc, PCCREG_TMR2_PRELOAD,
	    pcc_timer_us2lim(CLOCK_NEWINT(clock_statvar, clock_statmin)));
	pcc_reg_write(sys_pcc, PCCREG_TMR2_CONTROL, PCC_TIMERCLEAR);
	pcc_reg_write(sys_pcc, PCCREG_TMR2_CONTROL, PCC_TIMERSTART);

	pcc_reg_write(sys_pcc, PCCREG_TMR2_INTR_CTRL,
	    clock_pcc_sc->sc_clock_lvl);

	clock_statcnt.ev_count++;
	return (1);
}

/* ARGSUSED */
void
clock_pcc_shutdown(arg)
	void *arg;
{

	/* Make sure the timer interrupts are turned off. */
	pcc_reg_write(sys_pcc, PCCREG_TMR1_CONTROL, PCC_TIMERCLEAR);
	pcc_reg_write(sys_pcc, PCCREG_TMR1_INTR_CTRL, 0);
	pcc_reg_write(sys_pcc, PCCREG_TMR2_CONTROL, PCC_TIMERCLEAR);
	pcc_reg_write(sys_pcc, PCCREG_TMR2_INTR_CTRL, 0);
}
