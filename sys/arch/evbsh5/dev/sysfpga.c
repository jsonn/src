/*	$NetBSD: sysfpga.c,v 1.3.2.3 2002/10/10 18:32:34 jdolecek Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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

/* Cayman's System FPGA Chip */

#include "sh5pci.h"
#include "superio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <sh5/dev/femivar.h>
#include <sh5/dev/intcreg.h>
#include <evbsh5/dev/sysfpgareg.h>
#include <evbsh5/dev/sysfpgavar.h>

struct sysfpga_ihandler {
	int (*ih_func)(void *);
	void *ih_arg;
	int ih_level;
	int ih_group;
	int ih_inum;
};

struct sysfpga_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_space_handle_t sc_bush;
	u_int8_t sc_intmr[SYSFPGA_NGROUPS];
	void *sc_ih[SYSFPGA_NGROUPS];
#if NSUPERIO > 0
	struct sysfpga_ihandler sc_ih_superio[SYSFPGA_SUPERIO_NINTR];
#endif
#if NSH5PCI > 0
	struct sysfpga_ihandler sc_ih_pci1[SYSFPGA_PCI1_NINTR];
	struct sysfpga_ihandler sc_ih_pci2[SYSFPGA_PCI1_NINTR];
#endif
};

static int sysfpgamatch(struct device *, struct cfdata *, void *);
static void sysfpgaattach(struct device *, struct device *, void *);
static int sysfpgaprint(void *, const char *);

CFATTACH_DECL(sysfpga, sizeof(struct sysfpga_softc),
    sysfpgamatch, sysfpgaattach, NULL, NULL);
extern struct cfdriver sysfpga_cd;

/*
 * Devices which hang off the System FPGA
 */
struct sysfpga_device {
	const char *sd_name;
	bus_addr_t sd_offset;
};

static struct sysfpga_device sysfpga_devices[] = {
	{"superio", SYSFPGA_OFFSET_SUPERIO},
	{NULL, 0}
};


#define	sysfpga_reg_read(s,r)	\
	    bus_space_read_4((s)->sc_bust, (s)->sc_bush, (r))
#define	sysfpga_reg_write(s,r,v)	\
	    bus_space_write_4((s)->sc_bust, (s)->sc_bush, (r), (v))

#if NSUPERIO > 0
static int sysfpga_intr_handler_irl1(void *);
#endif
#if NSH5PCI > 0
static int sysfpga_intr_handler_irl2(void *);
static int sysfpga_intr_handler_irl3(void *);
#endif

static int sysfpga_intr_dispatch(struct sysfpga_softc *,
	    struct sysfpga_ihandler *, int);


static const char *sysfpga_cpuclksel[] = {
	"400/200/100MHz", "400/200/66MHz", "400/200/50MHz", "<invalid>"
};

static struct sysfpga_softc *sysfpga_sc;


/*ARGSUSED*/
static int
sysfpgamatch(struct device *parent, struct cfdata *cf, void *args)
{

	if (sysfpga_sc)
		return (0);

	return (1);
}

/*ARGSUSED*/
static void
sysfpgaattach(struct device *parent, struct device *self, void *args)
{
	struct sysfpga_softc *sc = (struct sysfpga_softc *)self;
	struct femi_attach_args *fa = args;
	struct sysfpga_attach_args sa;
	u_int32_t reg;
	int i;

	sysfpga_sc = sc;

	sc->sc_bust = fa->fa_bust;

	bus_space_map(sc->sc_bust, fa->fa_offset + SYSFPGA_OFFSET_REGS,
	    SYSFPGA_REG_SZ, 0, &sc->sc_bush);

	reg = sysfpga_reg_read(sc, SYSFPGA_REG_DATE);
	printf(
	    ": Cayman System FPGA, Revision: %d - %02x/%02x/%02x (yy/mm/dd)\n",
	    SYSFPGA_DATE_REV(reg), SYSFPGA_DATE_YEAR(reg),
	    SYSFPGA_DATE_MONTH(reg), SYSFPGA_DATE_DATE(reg));

	reg = sysfpga_reg_read(sc, SYSFPGA_REG_BDMR);
	printf("%s: CPUCLKSEL: %s, CPU Clock Mode: %d\n", sc->sc_dev.dv_xname,
	    sysfpga_cpuclksel[SYSFPGA_BDMR_CPUCLKSEL(reg)],
	    SYSFPGA_CPUMR_CLKMODE(sysfpga_reg_read(sc, SYSFPGA_REG_CPUMR)));

#if NSUPERIO > 0
	memset(sc->sc_ih_superio, 0, sizeof(sc->sc_ih_superio));
#endif
#if NSH5PCI > 0
	memset(sc->sc_ih_superio, 0, sizeof(sc->sc_ih_pci1));
	memset(sc->sc_ih_superio, 0, sizeof(sc->sc_ih_pci2));
#endif

	for (i = 0; i < SYSFPGA_NGROUPS; i++) {
		sysfpga_reg_write(sc, SYSFPGA_REG_INTMR(i), 0);
		sc->sc_intmr[i] = 0;
	}

#if NSUPERIO > 0
	/*
	 * Hook interrupts from the Super IO device
	 */
        sc->sc_ih[SYSFPGA_IGROUP_SUPERIO] =
            sh5_intr_establish(INTC_INTEVT_IRL1, IST_LEVEL, IPL_SUPERIO,
            sysfpga_intr_handler_irl1, sc);

	if (sc->sc_ih[SYSFPGA_IGROUP_SUPERIO] == NULL)
		panic("sysfpga: failed to register superio isr");
#endif

#if NSH5PCI > 0
	/*
	 * Hook interrupts from the PCI1 and PCI2 pins
	 */
        sc->sc_ih[SYSFPGA_IGROUP_PCI1] =
            sh5_intr_establish(INTC_INTEVT_IRL2, IST_LEVEL, IPL_SH5PCI,
            sysfpga_intr_handler_irl2, sc);
        sc->sc_ih[SYSFPGA_IGROUP_PCI2] =
            sh5_intr_establish(INTC_INTEVT_IRL3, IST_LEVEL, IPL_SH5PCI,
            sysfpga_intr_handler_irl3, sc);

	if (sc->sc_ih[SYSFPGA_IGROUP_PCI1] == NULL ||
	    sc->sc_ih[SYSFPGA_IGROUP_PCI2] == NULL)
		panic("sysfpga: failed to register pci isr");
#endif

#ifdef DEBUG
	sysfpga_reg_write(sc, SYSFPGA_REG_NMIMR, 1);
#endif

	/*
	 * Attach configured children
	 */
	sa._sa_base = fa->fa_offset;
	for (i = 0; sysfpga_devices[i].sd_name != NULL; i++) {
		sa.sa_name = sysfpga_devices[i].sd_name;
		sa.sa_bust = fa->fa_bust;
		sa.sa_dmat = fa->fa_dmat;
		sa.sa_offset = sysfpga_devices[i].sd_offset + sa._sa_base;

		(void) config_found(self, &sa, sysfpgaprint);
	}
}

static int
sysfpgaprint(void *arg, const char *cp)
{
	struct sysfpga_attach_args *sa = arg;

	if (cp)
		printf("%s at %s", sa->sa_name, cp);

	printf(" offset 0x%lx", sa->sa_offset - sa->_sa_base);

	return (UNCONF);
}

#if NSUPERIO > 0
static int
sysfpga_intr_handler_irl1(void *arg)
{
	struct sysfpga_softc *sc = arg;
	struct sysfpga_ihandler *ih;
	u_int8_t intsr, intmr;
	int sr_reg, h = 0;

	ih = sc->sc_ih_superio;
	sr_reg = SYSFPGA_REG_INTSR(SYSFPGA_IGROUP_SUPERIO);
	intmr = sc->sc_intmr[SYSFPGA_IGROUP_SUPERIO];

	for (intsr = sysfpga_reg_read(sc, sr_reg);
	    (intsr &= intmr) != 0;
	    intsr = sysfpga_reg_read(sc, sr_reg)) {

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_UART1))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_UART1);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_UART2))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_UART2);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_MOUSE))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_MOUSE);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_KBD))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_KBD);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_LAN))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_LAN);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_IDE))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_IDE);

		if (intsr & (1 << SYSFPGA_SUPERIO_INUM_LPT))
			h |= sysfpga_intr_dispatch(sc, ih,
			    SYSFPGA_SUPERIO_INUM_LPT);

		if (h == 0)
			panic("sysfpga: unclaimed IRL1 interrupt: 0x%02x",
			    intsr);
	}

	return (h);
}
#endif

#if NSH5PCI > 0
static int
sysfpga_intr_handler_irl2(void *arg)
{
	struct sysfpga_softc *sc = arg;
	struct sysfpga_ihandler *ih;
	u_int8_t intsr, intmr;
	int sr_reg, h = 0;

	ih = sc->sc_ih_pci1;
	sr_reg = SYSFPGA_REG_INTSR(SYSFPGA_IGROUP_PCI1);
	intmr = sc->sc_intmr[SYSFPGA_IGROUP_PCI1];

	for (intsr = sysfpga_reg_read(sc, sr_reg);
	    (intsr &= intmr) != 0;
	    intsr = sysfpga_reg_read(sc, sr_reg)) {

		if (intsr & (1 << SYSFPGA_PCI1_INTA))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI1_INTA);

		if (intsr & (1 << SYSFPGA_PCI1_INTB))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI1_INTB);

		if (intsr & (1 << SYSFPGA_PCI1_INTC))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI1_INTC);

		if (intsr & (1 << SYSFPGA_PCI1_INTD))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI1_INTD);

		if (h == 0)
			panic("sysfpga: unclaimed IRL2 interrupt: 0x%02x",
			    intsr);
	}

	return (h);
}

static int
sysfpga_intr_handler_irl3(void *arg)
{
	struct sysfpga_softc *sc = arg;
	struct sysfpga_ihandler *ih;
	u_int8_t intsr, intmr;
	int sr_reg, h = 0;

	ih = sc->sc_ih_pci2;
	sr_reg = SYSFPGA_REG_INTSR(SYSFPGA_IGROUP_PCI2);
	intmr = sc->sc_intmr[SYSFPGA_IGROUP_PCI2];

	for (intsr = sysfpga_reg_read(sc, sr_reg);
	    (intsr &= intmr) != 0;
	    intsr = sysfpga_reg_read(sc, sr_reg)) {

		if (intsr & (1 << SYSFPGA_PCI2_INTA))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTA);

		if (intsr & (1 << SYSFPGA_PCI2_INTB))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTB);

		if (intsr & (1 << SYSFPGA_PCI2_INTC))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTC);

		if (intsr & (1 << SYSFPGA_PCI2_INTD))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTD);

		if (intsr & (1 << SYSFPGA_PCI2_FAL))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_FAL);

		if (intsr & (1 << SYSFPGA_PCI2_DEG))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_DEG);

		if (intsr & (1 << SYSFPGA_PCI2_INTP))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTP);

		if (intsr & (1 << SYSFPGA_PCI2_INTS))
			h |= sysfpga_intr_dispatch(sc, ih, SYSFPGA_PCI2_INTS);

		if (h == 0)
			panic("sysfpga: unclaimed IRL3 interrupt: 0x%02x",
			    intsr);
	}

	return (h);
}
#endif

static int
sysfpga_intr_dispatch(struct sysfpga_softc *sc, struct sysfpga_ihandler *ih,
    int hnum)
{
	int h, s;

	ih += hnum;

#ifdef DEBUG
	if (ih->ih_func == NULL)
		panic("sysfpga_intr_dispatch: NULL handler for isr %d", hnum);
#endif

	/*
	 * This splraise() is fine since sysfpga's interrupt handler
	 * runs at a lower ipl than anything the child drivers could request.
	 */
	s = splraise(ih->ih_level);
	h = (*ih->ih_func)(ih->ih_arg);
	splx(s);

	return (h);
}

struct evcnt *
sysfpga_intr_evcnt(int group)
{
	struct sysfpga_softc *sc = sysfpga_sc;

	KDASSERT(group < SYSFPGA_NGROUPS);
	KDASSERT(sc->sc_ih[group] != NULL);

	return (sh5_intr_evcnt(sc->sc_ih[group]));
}

void *
sysfpga_intr_establish(int group, int level, int inum,
    int (*func)(void *), void *arg)
{
	struct sysfpga_softc *sc = sysfpga_sc;
	struct sysfpga_ihandler *ih;
	int s;

	switch (group) {
#if NSUPERIO > 0
	case SYSFPGA_IGROUP_SUPERIO:
		KDASSERT(inum < SYSFPGA_SUPERIO_NINTR);
		KDASSERT(level >= IPL_SUPERIO);
		ih = sc->sc_ih_superio;
		break;
#endif
#if NSH5PCI > 0
	case SYSFPGA_IGROUP_PCI1:
		KDASSERT(inum < SYSFPGA_PCI1_NINTR);
		KDASSERT(level >= IPL_SH5PCI);
		ih = sc->sc_ih_pci1;
		break;

	case SYSFPGA_IGROUP_PCI2:
		KDASSERT(inum < SYSFPGA_PCI2_NINTR);
		KDASSERT(level >= IPL_SH5PCI);
		ih = sc->sc_ih_pci2;
		break;
#endif
	default:
		return (NULL);
	}

	ih += inum;

	KDASSERT(ih->ih_func == NULL);

	ih->ih_level = level;
	ih->ih_group = group;
	ih->ih_inum = inum;
	ih->ih_arg = arg;
	ih->ih_func = func;

	s = splhigh();
	sc->sc_intmr[group] |= 1 << inum;
	sysfpga_reg_write(sc, SYSFPGA_REG_INTMR(group), sc->sc_intmr[group]);
	splx(s);

	return ((void *)ih);
}

void
sysfpga_intr_disestablish(void *cookie)
{
	struct sysfpga_softc *sc = sysfpga_sc;
	struct sysfpga_ihandler *ih = cookie;
	int s;

	s = splhigh();
	sc->sc_intmr[ih->ih_group] &= ~(1 << ih->ih_inum);
	sysfpga_reg_write(sc, SYSFPGA_REG_INTMR(ih->ih_group),
	    sc->sc_intmr[ih->ih_group]);
	splx(s);

	ih->ih_func = NULL;
}

void
sysfpga_nmi_clear(void)
{

	sysfpga_reg_write(sysfpga_sc, SYSFPGA_REG_NMISR, 0);
}
