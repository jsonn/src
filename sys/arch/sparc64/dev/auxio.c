/*	$NetBSD: auxio.c,v 1.1.10.1 2002/01/10 19:49:13 thorpej Exp $	*/

/*
 * Copyright (c) 2000, 2001 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * AUXIO registers support on the sbus & ebus2, used for the floppy driver
 * and to control the system LED, for the BLINK option.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/ebus/ebusreg.h>
#include <sparc64/dev/ebusvar.h>
#include <sparc64/dev/sbusvar.h>
#include <sparc64/dev/auxioreg.h>

/*
 * on sun4u, auxio exists with one register (LED) on the sbus, and 5
 * registers on the ebus2 (pci) (LED, PCIMODE, FREQUENCY, SCSI
 * OSCILLATOR, and TEMP SENSE.
 */

struct auxio_softc {
	struct device		sc_dev;

	/* parent's tag */
	bus_space_tag_t		sc_tag;

	/* handles to the various auxio regsiter sets */
	bus_space_handle_t	sc_led;
	bus_space_handle_t	sc_pci;
	bus_space_handle_t	sc_freq;
	bus_space_handle_t	sc_scsi;
	bus_space_handle_t	sc_temp;

	int			sc_flags;
#define	AUXIO_LEDONLY		0x1
#define	AUXIO_EBUS		0x2
#define	AUXIO_SBUS		0x4
};

#define	AUXIO_ROM_NAME		"auxio"

void	auxio_attach_common(struct auxio_softc *);
int	auxio_ebus_match(struct device *, struct cfdata *, void *);
void	auxio_ebus_attach(struct device *, struct device *, void *);
int	auxio_sbus_match(struct device *, struct cfdata *, void *);
void	auxio_sbus_attach(struct device *, struct device *, void *);

struct cfattach auxio_ebus_ca = {
	sizeof(struct auxio_softc), auxio_ebus_match, auxio_ebus_attach
};
struct cfattach auxio_sbus_ca = {
	sizeof(struct auxio_softc), auxio_sbus_match, auxio_sbus_attach
};

#ifdef BLINK
static struct callout blink_ch = CALLOUT_INITIALIZER;

static void auxio_blink(void *);

static void
auxio_blink(x)
	void *x;
{
	struct auxio_softc *sc = x;
	int s;
	u_int32_t led;

	s = splhigh();
	if (sc->sc_flags & AUXIO_EBUS)
		led = le32toh(bus_space_read_4(sc->sc_tag, sc->sc_led, 0));
	else
		led = bus_space_read_1(sc->sc_tag, sc->sc_led, 0);
	if (led & AUXIO_LED_LED)
		led = 0;
	else
		led = AUXIO_LED_LED;
	if (sc->sc_flags & AUXIO_EBUS)
		bus_space_write_4(sc->sc_tag, sc->sc_led, 0, htole32(led));
	else
		bus_space_write_1(sc->sc_tag, sc->sc_led, 0, led);
	splx(s);

	/*
	 * Blink rate is:
	 *	full cycle every second if completely idle (loadav = 0)
	 *	full cycle every 2 seconds if loadav = 1
	 *	full cycle every 3 seconds if loadav = 2
	 * etc.
	 */
	s = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 1));
	callout_reset(&blink_ch, s, auxio_blink, sc);
}
#endif

void
auxio_attach_common(sc)
	struct auxio_softc *sc;
{
#ifdef BLINK
	static int do_once = 1;

	/* only start one blinker */
	if (do_once) {
		auxio_blink(sc);
		do_once = 0;
	}
#endif
	printf("\n");
}

int
auxio_ebus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct ebus_attach_args *ea = aux;

	return (strcmp(AUXIO_ROM_NAME, ea->ea_name) == 0);
}

void
auxio_ebus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct auxio_softc *sc = (struct auxio_softc *)self;
	struct ebus_attach_args *ea = aux;

	if (ea->ea_nregs < 1 || ea->ea_nvaddrs < 1) {
		printf(": no registers??\n");
		return;
	}

	if (ea->ea_nregs != 5 || ea->ea_nvaddrs != 5) {
		printf(": not 5 (%d) registers, only setting led",
		    ea->ea_nregs);
		sc->sc_flags = AUXIO_LEDONLY|AUXIO_EBUS;
	} else {
		sc->sc_flags = AUXIO_EBUS;
		sc->sc_pci = (bus_space_handle_t)(u_long)ea->ea_vaddrs[1];
		sc->sc_freq = (bus_space_handle_t)(u_long)ea->ea_vaddrs[2];
		sc->sc_scsi = (bus_space_handle_t)(u_long)ea->ea_vaddrs[3];
		sc->sc_temp = (bus_space_handle_t)(u_long)ea->ea_vaddrs[4];
	}
	sc->sc_led = (bus_space_handle_t)(u_long)ea->ea_vaddrs[0];
	
	sc->sc_tag = ea->ea_bustag;

	auxio_attach_common(sc);
}

int
auxio_sbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(AUXIO_ROM_NAME, sa->sa_name) == 0);
}

void
auxio_sbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct auxio_softc *sc = (struct auxio_softc *)self;
	struct sbus_attach_args *sa = aux;

	if (sa->sa_nreg < 1 || sa->sa_npromvaddrs < 1) {
		printf(": no registers??\n");
		return;
	}

	if (sa->sa_nreg != 1 || sa->sa_npromvaddrs != 1) {
		printf(": not 1 (%d/%d) registers??", sa->sa_nreg,
		    sa->sa_npromvaddrs);
		return;
	}

	/* sbus auxio only has one set of registers */
	sc->sc_flags = AUXIO_LEDONLY|AUXIO_SBUS;
	sc->sc_led = (bus_space_handle_t)(u_long)sa->sa_promvaddr;

	sc->sc_tag = sa->sa_bustag;

	auxio_attach_common(sc);
}
