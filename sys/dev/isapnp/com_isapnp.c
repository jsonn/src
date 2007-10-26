/*	$NetBSD: com_isapnp.c,v 1.26.22.1 2007/10/26 15:45:34 joerg Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_isapnp.c,v 1.26.22.1 2007/10/26 15:45:34 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/comvar.h>
#include <dev/ic/comreg.h>

struct com_isapnp_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* ISAPnP-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

int	com_isapnp_match(struct device *, struct cfdata *, void *);
void	com_isapnp_attach(struct device *, struct device *, void *);

CFATTACH_DECL(com_isapnp, sizeof(struct com_isapnp_softc),
    com_isapnp_match, com_isapnp_attach, NULL, NULL);

int
com_isapnp_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_com_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

void
com_isapnp_attach(struct device *parent, struct device *self,
    void *aux)
{
	struct com_isapnp_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;
	struct isapnp_attach_args *ipa = aux;

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		printf("%s: error in region allocation\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	COM_INIT_REGS(sc->sc_regs, ipa->ipa_iot, ipa->ipa_io[0].h,
	    ipa->ipa_io[0].base);

	/*
	 * if the chip isn't something we recognise skip it.
	 */
	if (com_probe_subr(&sc->sc_regs) == 0)
		return;

	sc->sc_frequency = 115200 * 16; /* is that always right? */

	com_attach_subr(sc);

	isc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_SERIAL, comintr, sc);
}
