/*	$NetBSD: tsdio.c,v 1.3.52.1 2007/10/25 22:38:26 bouyer Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jesse Off
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tsdio.c,v 1.3.52.1 2007/10/25 22:38:26 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/tsdiovar.h>

int	tsdio_probe(struct device *, struct cfdata *, void *);
void	tsdio_attach(struct device *, struct device *, void *);
int	tsdio_search(struct device *, struct cfdata *, const int *, void *);
int	tsdio_print(void *, const char *);

CFATTACH_DECL(tsdio, sizeof(struct tsdio_softc),
    tsdio_probe, tsdio_attach, NULL, NULL);

int
tsdio_probe(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t dioh;
	int rv = 0, have_io = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * Disallow wildcarded I/O base.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/*
	 * Map the I/O space.
	 */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 8,
	    0, &dioh))
		goto out;
	have_io = 1;

	if (bus_space_read_1(ia->ia_iot, dioh, 0) != 0x54) {
		goto out;
	}

	rv = 1;
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = 8;
	ia->ia_niomem = 0;
	ia->ia_nirq = 0;
	ia->ia_ndrq = 0;

 out:
	if (have_io)
		bus_space_unmap(iot, dioh, 8);

	return (rv);
}

void
tsdio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tsdio_softc *sc = (struct tsdio_softc *) self;
	struct isa_attach_args *ia = aux;

	sc->sc_iot = ia->ia_iot;

	aprint_normal(": Technologic Systems TS-DIO24\n");

	/*
	 * Map the device.
	 */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, 8,
	    0, &sc->sc_ioh)) {
		aprint_error("%s: unable to map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 *  Attach sub-devices
	 */
	config_search_ia(tsdio_search, self, "tsdio", NULL);
}

int
tsdio_search(parent, cf, l, aux)
	struct device *parent;
	struct cfdata *cf;
	const int *l;
	void *aux;
{
	struct tsdio_softc *sc = (struct tsdio_softc *)parent;
	struct tsdio_attach_args sa;

	sa.ta_iot = sc->sc_iot;
	sa.ta_ioh = sc->sc_ioh;

	if (config_match(parent, cf, &sa) > 0)
		config_attach(parent, cf, &sa, tsdio_print);

	return (0);
}

int
tsdio_print(aux, name)
	void *aux;
	const char *name;
{

	return (UNCONF);
}
