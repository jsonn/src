/*	$NetBSD: mkclock_hb.c,v 1.7.2.1 2008/05/18 12:32:32 yamt Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: mkclock_hb.c,v 1.7.2.1 2008/05/18 12:32:32 yamt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/clock_subr.h>
#include <dev/ic/mk48txxreg.h>
#include <dev/ic/mk48txxvar.h>

#include <newsmips/dev/hbvar.h>

#include "ioconf.h"

int  mkclock_hb_match(device_t, cfdata_t, void *);
void mkclock_hb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mkclock_hb, sizeof(struct mk48txx_softc),
    mkclock_hb_match, mkclock_hb_attach, NULL, NULL);

int
mkclock_hb_match(device_t parent, cfdata_t cf, void *aux)
{
	struct hb_attach_args *ha = aux;
	static int mkclock_hb_matched;

	/* Only one clock, please. */
	if (mkclock_hb_matched)
		return 0;

	if (strcmp(ha->ha_name, mkclock_cd.cd_name))
		return 0;

	mkclock_hb_matched = 1;

	return 1;
}

void
mkclock_hb_attach(device_t parent, device_t self, void *aux)
{
	struct mk48txx_softc *sc = device_private(self);
	struct hb_attach_args *ha = aux;

	sc->sc_dev = self;
	if (bus_space_map(sc->sc_bst, (bus_addr_t)ha->ha_addr, MK48T02_CLKSZ,
	    0, &sc->sc_bsh) != 0)
		aprint_error(": can't map device space\n");

	sc->sc_model = "mk48t02";
	sc->sc_year0 = 1900;
	mk48txx_attach(sc);

	aprint_normal("\n");
}
