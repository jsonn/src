/*	$NetBSD: npx_pnpbios.c,v 1.6.52.1 2006/11/18 21:29:20 ad Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: npx_pnpbios.c,v 1.6.52.1 2006/11/18 21:29:20 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>
#include <machine/specialreg.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <i386/pnpbios/pnpbiosvar.h>

#include <i386/isa/npxvar.h> 

int	npx_pnpbios_match(struct device *, struct cfdata *, void *);
void	npx_pnpbios_attach(struct device *, struct device *, void *);

CFATTACH_DECL(npx_pnpbios, sizeof(struct npx_softc),
    npx_pnpbios_match, npx_pnpbios_attach, NULL, NULL);

int
npx_pnpbios_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct pnpbiosdev_attach_args *aa = aux;

	if (strcmp(aa->idstr, "PNP0C04"))
		return (0);

	return (1);
}

void
npx_pnpbios_attach(struct device *parent, struct device *self,
    void *aux)
{
	struct npx_softc *sc = (void *)self;
	struct pnpbiosdev_attach_args *aa = aux;
	int irq, ist;

	if (pnpbios_io_map(aa->pbt, aa->resc, 0, &sc->sc_iot, &sc->sc_ioh)) { 	
		aprint_error(": can't map i/o space\n");
		return;
	}

	aprint_naive("\n");
	aprint_normal("\n");
	pnpbios_print_devres(self, aa);

	if (pnpbios_getirqnum(aa->pbt, aa->resc, 0, &irq, &ist) != 0) {
		aprint_error("%s: unable to get IRQ number or type\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_type = npxprobe1(sc->sc_iot, sc->sc_ioh, irq);

	switch (sc->sc_type) {
	case NPX_INTERRUPT:
		aprint_normal("%s: interrupting at irq %d\n",
		    sc->sc_dev.dv_xname, irq);
		lcr0(rcr0() & ~CR0_NE);
		sc->sc_ih = isa_intr_establish(0/*XXX*/, irq, ist, IPL_NONE,
		     (int (*)(void *))npxintr, NULL);
		break;
	case NPX_EXCEPTION:
		/*FALLTHROUGH*/
	case NPX_CPUID:
		aprint_verbose("%s:%s using exception 16\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_type == NPX_CPUID ? " reported by CPUID;" : "");
		sc->sc_type = NPX_EXCEPTION;
		break;
	case NPX_BROKEN:
		aprint_error("%s: error reporting broken; not using\n",
		    sc->sc_dev.dv_xname);
		sc->sc_type = NPX_NONE;
		return;
	case NPX_NONE:
		panic("npx_pnpbios_attach");
	}

	npxattach(sc);
}
