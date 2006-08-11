/*	$NetBSD: com_ebus.c,v 1.12.8.1 2006/08/11 15:42:47 yamt Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_ebus.c,v 1.12.8.1 2006/08/11 15:42:47 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/intr.h>

#include <dev/ebus/ebusreg.h>
#include <dev/ebus/ebusvar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_ebus_softc {
	struct com_softc ebsc_com;	/* real "com" softc */

	/* this space for rent */
};

static int com_ebus_match(struct device *, struct cfdata *, void *);
static void com_ebus_attach(struct device *, struct device *, void *);

CFATTACH_DECL(com_ebus, sizeof(struct com_ebus_softc),
    com_ebus_match, com_ebus_attach, NULL, NULL);

static int
com_ebus_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct ebus_attach_args *ea = aux;
	bus_space_handle_t ioh;
	int match;

	if (strcmp(ea->ea_name, "su") != 0)
		return (0);

	match = 0;
	if (bus_space_map(ea->ea_bustag, EBUS_ADDR_FROM_REG(&ea->ea_reg[0]),
			  ea->ea_reg[0].size, 0, &ioh) == 0)
	{
		match = comprobe1(ea->ea_bustag, ioh);
		bus_space_unmap(ea->ea_bustag, ioh, ea->ea_reg[0].size);
	}

	return (match);
}

static void
com_ebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_ebus_softc *ebsc = (void *)self;
	struct com_softc *sc = &ebsc->ebsc_com;
	struct ebus_attach_args *ea = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t iobase;

	iot = ea->ea_bustag;
	iobase = EBUS_ADDR_FROM_REG(&ea->ea_reg[0]);
	sc->sc_frequency = COM_FREQ;
	sc->sc_hwflags = COM_HW_NO_TXPRELOAD;

	/*
	 * XXX: It would be nice to be able to split console input and
	 * output to different devices.  For now switch to serial
	 * console if PROM stdin is on serial (so that we can use DDB).
	 */
	if (prom_instance_to_package(prom_stdin()) == ea->ea_node)
		comcnattach(iot, iobase, B9600, sc->sc_frequency,
		    COM_TYPE_NORMAL, (CLOCAL | CREAD | CS8));

	if (!com_is_console(iot, iobase, &ioh)
	    && bus_space_map(iot, iobase, ea->ea_reg[0].size,
			     0, &ioh) != 0)
	{
		printf(": unable to map device registers\n");
		return;
	}

	COM_INIT_REGS(sc->sc_regs, iot, ioh, iobase);

	com_attach_subr(sc);

	if (ea->ea_nintr != 0)
		(void)bus_intr_establish(iot, ea->ea_intr[0], IPL_SERIAL,
					 comintr, sc);
}
