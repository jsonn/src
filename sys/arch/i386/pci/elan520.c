/*	$NetBSD: elan520.c,v 1.1.8.2 2002/09/06 08:36:31 jdolecek Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Device driver for the AMD Elan SC520 System Controller.  This attaches
 * where the "pchb" driver might normally attach, and provides support for
 * extra features on the SC520, such as the watchdog timer and GPIO.
 *
 * Information about the GP bus echo bug work-around is from code posted
 * to the "soekris-tech" mailing list by Jasper Wallace.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: elan520.c,v 1.1.8.2 2002/09/06 08:36:31 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/wdog.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>

#include <dev/pci/pcidevs.h>

#include <arch/i386/pci/elan520reg.h>

#include <dev/sysmon/sysmonvar.h>

struct elansc_softc {
	struct device sc_dev;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;
	int sc_echobug;

	struct sysmon_wdog sc_smw;
};

static void
elansc_wdogctl_write(struct elansc_softc *sc, uint16_t val)
{
	int s;
	uint8_t echo_mode;

	s = splhigh();

	/* Switch off GP bus echo mode if we need to. */
	if (sc->sc_echobug) {
		echo_mode = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    MMCR_GPECHO);
		bus_space_write_1(sc->sc_memt, sc->sc_memh,
		    MMCR_GPECHO, echo_mode & ~GPECHO_GP_ECHO_ENB);
	}

	/* Unlock the register. */
	bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
	    WDTMRCTL_UNLOCK1);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
	    WDTMRCTL_UNLOCK2);

	/* Write the value. */
	bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL, val);

	/* Switch GP bus echo mode back. */
	if (sc->sc_echobug)
		bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_GPECHO,
		    echo_mode);

	splx(s);
}

static void
elansc_wdogctl_reset(struct elansc_softc *sc)
{
	int s;
	uint8_t echo_mode;

	s = splhigh();

	/* Switch off GP bus echo mode if we need to. */
	if (sc->sc_echobug) {
		echo_mode = bus_space_read_1(sc->sc_memt, sc->sc_memh, 
		    MMCR_GPECHO); 
		bus_space_write_1(sc->sc_memt, sc->sc_memh,
		    MMCR_GPECHO, echo_mode & ~GPECHO_GP_ECHO_ENB); 
	}

	/* Reset the watchdog. */
	bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
	    WDTMRCTL_RESET1);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
	    WDTMRCTL_RESET2);

	/* Switch GP bus echo mode back. */
	if (sc->sc_echobug)
		bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_GPECHO,
		    echo_mode);

	splx(s);
}

static const struct {
	int	period;		/* whole seconds */
	uint16_t exp;		/* exponent select */
} elansc_wdog_periods[] = {
	{ 1,	WDTMRCTL_EXP_SEL25 },
	{ 2,	WDTMRCTL_EXP_SEL26 },
	{ 4,	WDTMRCTL_EXP_SEL27 },
	{ 8,	WDTMRCTL_EXP_SEL28 },
	{ 16,	WDTMRCTL_EXP_SEL29 },
	{ 32,	WDTMRCTL_EXP_SEL30 },
	{ 0,	0 },
};

static int
elansc_wdog_setmode(struct sysmon_wdog *smw)
{
	struct elansc_softc *sc = smw->smw_cookie;
	int i;
	uint16_t exp_sel;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		elansc_wdogctl_write(sc,
		    WDTMRCTL_WRST_ENB | WDTMRCTL_EXP_SEL30);
	} else {
		if (smw->smw_period == WDOG_PERIOD_DEFAULT) {
			smw->smw_period = 32;
			exp_sel = WDTMRCTL_EXP_SEL30;
		} else {
			for (i = 0; elansc_wdog_periods[i].period != 0; i++) {
				if (elansc_wdog_periods[i].period ==
				    smw->smw_period) {
					exp_sel = elansc_wdog_periods[i].exp;
					break;
				}
			}
			if (elansc_wdog_periods[i].period == 0)
				return (EINVAL);
		}
		elansc_wdogctl_write(sc, WDTMRCTL_ENB |
		    WDTMRCTL_WRST_ENB | exp_sel);
		elansc_wdogctl_reset(sc);
	}
	return (0);
}

static int
elansc_wdog_tickle(struct sysmon_wdog *smw)
{
	struct elansc_softc *sc = smw->smw_cookie;

	elansc_wdogctl_reset(sc);
	return (0);
}

static int
elansc_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_SC520_SC)
		return (10);	/* beat pchb */

	return (0);
}

static const char *elansc_speeds[] = {
	"(reserved 00)",
	"100MHz",
	"133MHz",
	"(reserved 11)",
};

static void
elansc_attach(struct device *parent, struct device *self, void *aux)
{
	struct elansc_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	uint16_t rev;
	uint8_t ressta, cpuctl;

	printf(": AMD Elan SC520 System Controller\n");

	sc->sc_memt = pa->pa_memt;
	if (bus_space_map(sc->sc_memt, MMCR_BASE_ADDR, NBPG, 0,
	    &sc->sc_memh) != 0) {
		printf("%s: unable to map registers\n", sc->sc_dev.dv_xname);
		return;
	}

	rev = bus_space_read_2(sc->sc_memt, sc->sc_memh, MMCR_REVID);
	cpuctl = bus_space_read_1(sc->sc_memt, sc->sc_memh, MMCR_CPUCTL);

	printf("%s: product %d stepping %d.%d, CPU clock %s\n",
	    sc->sc_dev.dv_xname,
	    (rev & REVID_PRODID) >> REVID_PRODID_SHIFT,
	    (rev & REVID_MAJSTEP) >> REVID_MAJSTEP_SHIFT,
	    (rev & REVID_MINSTEP),
	    elansc_speeds[cpuctl & CPUCTL_CPU_CLK_SPD_MASK]);

	/*
	 * SC520 rev A1 has a bug that affects the watchdog timer.  If
	 * the GP bus echo mode is enabled, writing to the watchdog control
	 * register is blocked.
	 *
	 * The BIOS in some systems (e.g. the Soekris net4501) enables
	 * GP bus echo for various reasons, so we need to switch it off
	 * when we talk to the watchdog timer.
	 *
	 * XXX The step 1.1 (B1?) in my Soekris net4501 also has this
	 * XXX problem, so we'll just enable it for all Elan SC520s
	 * XXX for now.  --thorpej@netbsd.org
	 */
	if (1 || rev == ((PRODID_ELAN_SC520 << REVID_PRODID_SHIFT) |
		    (0 << REVID_MAJSTEP_SHIFT) | (1)))
		sc->sc_echobug = 1;

	/*
	 * Determine cause of the last reset, and issue a warning if it
	 * was due to watchdog expiry.
	 */
	ressta = bus_space_read_1(sc->sc_memt, sc->sc_memh, MMCR_RESSTA);
	if (ressta & RESSTA_WDT_RST_DET)
		printf("%s: WARNING: LAST RESET DUE TO WATCHDOG EXPIRATION!\n",
		    sc->sc_dev.dv_xname);
	bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_RESSTA, ressta);

	/*
	 * Hook up the watchdog timer.
	 */
	sc->sc_smw.smw_name = sc->sc_dev.dv_xname;
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = elansc_wdog_setmode;
	sc->sc_smw.smw_tickle = elansc_wdog_tickle;
	sc->sc_smw.smw_period = 32;	/* actually 32.54 */
	if (sysmon_wdog_register(&sc->sc_smw) != 0)
		printf("%s: unable to register watchdog with sysmon\n",
		    sc->sc_dev.dv_xname);

	/* Set up the watchdog registers with some defaults. */
	elansc_wdogctl_write(sc, WDTMRCTL_WRST_ENB | WDTMRCTL_EXP_SEL30);

	/* ...and clear it. */
	elansc_wdogctl_reset(sc);
}

struct cfattach elansc_ca = {
	sizeof(struct elansc_softc), elansc_match, elansc_attach,
};
