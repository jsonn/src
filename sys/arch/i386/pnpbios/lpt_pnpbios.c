/* $NetBSD: lpt_pnpbios.c,v 1.6.14.1 2005/04/29 11:28:13 kent Exp $ */
/*
 * Copyright (c) 1999
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lpt_pnpbios.c,v 1.6.14.1 2005/04/29 11:28:13 kent Exp $");

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

#include <i386/pnpbios/pnpbiosvar.h>

#include <dev/ic/lptvar.h>

struct lpt_pnpbios_softc {
	struct	lpt_softc sc_lpt;
};

int lpt_pnpbios_match(struct device *, struct cfdata *, void *);
void lpt_pnpbios_attach(struct device *, struct device *, void *);

CFATTACH_DECL(lpt_pnpbios, sizeof(struct lpt_pnpbios_softc),
    lpt_pnpbios_match, lpt_pnpbios_attach, NULL, NULL);

int
lpt_pnpbios_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pnpbiosdev_attach_args *aa = aux;

	if (strcmp(aa->idstr, "PNP0400") &&
	    strcmp(aa->idstr, "PNP0401"))
		return (0);

	return (1);
}

void
lpt_pnpbios_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpt_pnpbios_softc *psc = (void *)self;
	struct lpt_softc *sc = &psc->sc_lpt;
	struct pnpbiosdev_attach_args *aa = aux;

	if (pnpbios_io_map(aa->pbt, aa->resc, 0, &sc->sc_iot, &sc->sc_ioh)) { 	
		printf(": can't map i/o space\n");
		return;
	}

	printf("\n");
	pnpbios_print_devres(self, aa);

	lpt_attach_subr(sc);

	sc->sc_ih = pnpbios_intr_establish(aa->pbt, aa->resc, 0, IPL_TTY,
					    lptintr, sc);
}
