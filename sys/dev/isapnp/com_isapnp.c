/*	$NetBSD: com_isapnp.c,v 1.6.2.1 1997/10/29 00:40:20 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>

#include <dev/ic/comvar.h>

struct com_isapnp_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* ISAPnP-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

int	com_isapnp_match __P((struct device *, void *, void *));
void	com_isapnp_attach __P((struct device *, struct device *, void *));

struct cfattach com_isapnp_ca = {
	sizeof(struct com_isapnp_softc), com_isapnp_match, com_isapnp_attach
};

int
com_isapnp_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isapnp_attach_args *ipa = aux;

	if (strcmp(ipa->ipa_devlogic, "ROK0010") &&
	    strcmp(ipa->ipa_devlogic, "BRI1400") && /* Boca 33.6 PnP */
	    strcmp(ipa->ipa_devcompat, "PNP0500") && /* generic 8250/16450 */
	    strcmp(ipa->ipa_devcompat, "PNP0501")) /* generic 16550A */
		return (0);

	return (1);
}

void
com_isapnp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_isapnp_softc *isc = (void *)self;
	struct com_softc *sc = &isc->sc_com;
	struct isapnp_attach_args *ipa = aux;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		printf("%s: error in region allocation\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = ipa->ipa_iot;
	sc->sc_iobase = ipa->ipa_io[0].base;
	sc->sc_ioh = ipa->ipa_io[0].h;

	/*
	 * if the chip isn't something we recognise skip it.
	 */
	if (comprobe1(sc->sc_iot, sc->sc_ioh, sc->sc_iobase) == 0)
		return;

	com_attach_subr(sc);

	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_SERIAL, comintr, sc);
}
