/*	$NetBSD: mkclock_sbdio.c,v 1.6.4.1 2008/05/16 02:22:19 yamt Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: mkclock_sbdio.c,v 1.6.4.1 2008/05/16 02:22:19 yamt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/sbdiovar.h>

#include <dev/clock_subr.h>

#include <dev/ic/mk48txxreg.h>
#include <dev/ic/mk48txxvar.h>

#include "ioconf.h"

#define MKCLOCK_SBD_STRIDE	2

int  mkclock_sbdio_match(device_t, cfdata_t, void *);
void mkclock_sbdio_attach(device_t, device_t, void *);
static uint8_t mkclock_sbdio_nvrd(struct mk48txx_softc *, int);
static void mkclock_sbdio_nvwr(struct mk48txx_softc *, int, uint8_t);

CFATTACH_DECL_NEW(mkclock_sbdio, sizeof(struct mk48txx_softc),
    mkclock_sbdio_match, mkclock_sbdio_attach, NULL, NULL);

int
mkclock_sbdio_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbdio_attach_args *sa = aux;

	if (strcmp("mkclock", sa->sa_name) != 0)
		return 0;

	return 1;
}

void
mkclock_sbdio_attach(device_t parent, device_t self, void *aux)
{
	struct mk48txx_softc *sc = device_private(self);
	struct sbdio_attach_args *sa = aux;
	bus_size_t size;

	sc->sc_dev = self;
	switch (sa->sa_flags) {
	case 0x0000:
		sc->sc_model = "mk48t08";
		size = MK48T08_CLKSZ;
		break;

	case 0x0001:
		sc->sc_model = "mk48t18";
		size = MK48T18_CLKSZ;
		break;

	default:
		/* assume MK48T18 */
		sc->sc_model = "mk48t18";
		size = MK48T18_CLKSZ;
		break;
	}

	sc->sc_bst = sa->sa_bust;
	if (bus_space_map(sc->sc_bst, sa->sa_addr1, size, 0,
	    &sc->sc_bsh) != 0) {
		aprint_error(": can't map device space\n");
		return;
	}

	sc->sc_year0 = 2000;	/* XXX Is this OK? */
	sc->sc_nvrd = mkclock_sbdio_nvrd;
	sc->sc_nvwr = mkclock_sbdio_nvwr;

	mk48txx_attach(sc);

	aprint_normal("\n");
}

static uint8_t
mkclock_sbdio_nvrd(struct mk48txx_softc *sc, int off)
{
	uint8_t rv;

	rv = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
	    off << MKCLOCK_SBD_STRIDE);
	return rv;
}

static void
mkclock_sbdio_nvwr(struct mk48txx_softc *sc, int off, uint8_t v)
{

	bus_space_write_1(sc->sc_bst, sc->sc_bsh, off << MKCLOCK_SBD_STRIDE, v);
}
