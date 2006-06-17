/*	$NetBSD: com_isa.c,v 1.24.16.3 2006/06/17 00:53:39 gdamore Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_isa.c,v 1.24.16.3 2006/06/17 00:53:39 gdamore Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#ifdef COM_HAYESP
#include <dev/ic/hayespreg.h>
#endif

#include <dev/isa/isavar.h>

struct com_isa_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* ISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

int com_isa_probe(struct device *, struct cfdata *, void *);
void com_isa_attach(struct device *, struct device *, void *);
#ifdef COM_HAYESP
int com_isa_isHAYESP(bus_space_handle_t, struct com_softc *);
#endif


CFATTACH_DECL(com_isa, sizeof(struct com_isa_softc),
    com_isa_probe, com_isa_attach, NULL, NULL);

int
com_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase;
	int rv = 1;
	struct isa_attach_args *ia = aux;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/* Don't allow wildcarded IRQ. */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	iot = ia->ia_iot;
	iobase = ia->ia_io[0].ir_addr;

	/* if it's in use as console, it's there. */
	if (!com_is_console(iot, iobase, 0)) {
		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			return 0;
		}
		rv = comprobe1(iot, ioh);
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = COM_NPORTS;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return (rv);
}

void
com_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_isa_softc *isc = (void *)self;
	struct com_softc *sc = &isc->sc_com;
	int iobase, irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
#ifdef COM_HAYESP
	int	hayesp_ports[] = { 0x140, 0x180, 0x280, 0x300, 0 };
	int	*hayespp;
#endif

	/*
	 * We're living on an isa.
	 */
	iobase = sc->sc_iobase = ia->ia_io[0].ir_addr;
	iot = sc->sc_iot = ia->ia_iot;

	if (!com_is_console(iot, iobase, &sc->sc_ioh) &&
	    bus_space_map(iot, iobase, COM_NPORTS, 0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_frequency = COM_FREQ;
	irq = ia->ia_irq[0].ir_irq;

#ifdef COM_HAYESP
	for (hayespp = hayesp_ports; *hayespp != 0; hayespp++) {
		bus_space_handle_t	hayespioh;
#define	HAYESP_NPORTS	8
		if (bus_space_map(iot, *hayespp, HAYESP_NPORTS, 0, &hayespioh))
			continue;
		if (com_isa_isHAYESP(hayespioh, sc)) {
			break;
		}
		bus_space_unmap(iot, hayespioh, HAYESP_NPORTS);
	}
#endif

	com_attach_subr(sc);

	isc->sc_ih = isa_intr_establish(ia->ia_ic, irq, IST_EDGE, IPL_SERIAL,
	    comintr, sc);

	/*
	 * Shutdown hook for buggy BIOSs that don't recognize the UART
	 * without a disabled FIFO.
	 */
	if (shutdownhook_establish(com_cleanup, sc) == NULL)
		panic("com_isa_attach: could not establish shutdown hook");
}

#ifdef COM_HAYESP
int
com_isa_isHAYESP(bus_space_handle_t hayespioh, struct com_softc *sc)
{
	char	val, dips;
	int	combaselist[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	bus_space_tag_t iot = sc->sc_iot;

	/*
	 * Hayes ESP cards have two iobases.  One is for compatibility with
	 * 16550 serial chips, and at the same ISA PC base addresses.  The
	 * other is for ESP-specific enhanced features, and lies at a
	 * different addressing range entirely (0x140, 0x180, 0x280, or 0x300).
	 */

	/* Test for ESP signature */
	if ((bus_space_read_1(iot, hayespioh, 0) & 0xf3) == 0)
		return (0);

	/*
	 * ESP is present at ESP enhanced base address; unknown com port
	 */

	/* Get the dip-switch configurations */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETDIPS);
	dips = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);

	/* Determine which com port this ESP card services: bits 0,1 of  */
	/*  dips is the port # (0-3); combaselist[val] is the com_iobase */
	if (sc->sc_regs.cr_iobase != combaselist[dips & 0x03])
		return (0);

	printf(": ESP");

 	/* Check ESP Self Test bits. */
	/* Check for ESP version 2.0: bits 4,5,6 == 010 */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETTEST);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1); /* Clear reg1 */
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		/* we do not support the necessary features */
		return (0);
	}

	/* Check for ability to emulate 16550: bit 8 == 1 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		/* XXX Does slave really mean no 16550 support?? */
		return (0);
	}

	/*
	 * If we made it this far, we are a full-featured ESP v2.0 (or
	 * better), at the correct com port address.
	 */
	sc->sc_type = COM_TYPE_HAYESP;
	sc->sc_hayespioh = hayespioh;
	sc->sc_fifolen = 1024;
	sc->sc_prescaler = 0;			/* set prescaler to x1. */
	printf(", 1024 byte fifo\n");
	return (1);
}
#endif
