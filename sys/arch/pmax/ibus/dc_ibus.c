/* $NetBSD: dc_ibus.c,v 1.1.2.2 1999/04/17 13:45:53 nisimura Exp $ */

/*
 * Copyright (c) 1998, 1999 Tohru Nishimura.  All rights reserved.
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
 *      This product includes software developed by Tohru Nishimura
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: dc_ibus.c,v 1.1.2.2 1999/04/17 13:45:53 nisimura Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <pmax/ibus/ibusvar.h>
#include <pmax/ibus/dc7085reg.h>	/* XXX dev/ic/dc7085reg.h XXX */
#include <pmax/ibus/dc7085var.h>	/* XXX machine/dc7085var.h XXX */

int  dcibus_match	__P((struct device *, struct cfdata *, void *));
void dcibus_attach	__P((struct device *, struct device *, void *));
int  dcibus_print	__P((void *, const char *));

struct cfattach dc_ibus_ca = {
	sizeof(struct dc_softc), dcibus_match, dcibus_attach
};

int dc_major = 16;		/* EXPORT */
int dcintr __P((void *));	/* IMPORT */

extern int badaddr __P((void *, u_int)); /* XXX */

int
dcibus_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct ibus_attach_args *d = aux;

	if (strcmp(d->ia_name, "dc") != 0 &&
	    strcmp(d->ia_name, "mdc") != 0 &&
	    strcmp(d->ia_name, "dc7085") != 0)
		return 0;

	if (badaddr((caddr_t)d->ia_addr, 2))
		return 0;

	return 1;
}

void
dcibus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ibus_attach_args *d = aux;
	struct dc_softc *sc = (struct dc_softc*)self;
	struct dc_attach_args args;
	int line;

	sc->sc_bst = /* XXX */ 0;
	if (bus_space_map(sc->sc_bst, d->ia_addr, 0x32, 0, &sc->sc_bsh)) {
		printf("%s: unable to map device\n", sc->sc_dv.dv_xname);
		return;
	}
	sc->sc_unit = /* XXX */ 0;
	printf("\n");

	for (line = 0; line < 4; line++) {
		args.line = line;
		config_found(self, (void *)&args, dcibus_print);
	}

	ibus_intr_establish(parent, d->ia_cookie, IPL_TTY, dcintr, sc);

#if 0
	s = splhigh();
	/* XXX enable DC7085 circuit here XXX */
	splx(s);
#endif
}

int
dcibus_print(aux, name)
	void *aux;
	const char *name;
{
	struct dc_attach_args *args = aux;

	if (name != NULL)
		printf("%s:", name);
	
	if (args->line != -1)
		printf(" line %d", args->line);

	return (UNCONF);
}
