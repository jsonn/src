/*	$NetBSD: aic_isa.c,v 1.1.2.2 1997/10/14 10:23:15 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/ic/aic6360reg.h>
#include <dev/ic/aic6360var.h>

int	aic_isa_probe	__P((struct device *, void *, void *));

struct cfattach aic_isa_ca = {
	sizeof(struct aic_softc), aic_isa_probe, aic_isa_attach
};


/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * aic_isa_probe: probe for AIC6360 SCSI-controller
 * returns non-zero value if a controller is found.
 */
int
aic_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	if (bus_space_map(iot, ia->ia_iobase, AIC_ISA_IOSIZE, 0, &ioh))
		return (0);

	AIC_TRACE(("aic_isa_probe: port 0x%x\n", ia->ia_iobase));
	rv = aic_find(iot, ioh);

	bus_space_unmap(iot, ioh, AIC_ISA_IOSIZE);

	if (rv) {
		ia->ia_msize = 0;
		ia->ia_iosize = AIC_ISA_IOSIZE;
	}
	return rv;
}

void
aic_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct aic_softc *sc = (void *)self;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	isa_chipset_tag_t ic = ia->ia_ic;

	printf("\n");

	if (bus_space_map(iot, ia->ia_iobase, AIC_ISA_IOSIZE, 0, &ioh))
		panic("aic_isa_attach: bus_space_map failed");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	AIC_TRACE(("aic_isa_attach: port 0x%x\n", ia->ia_iobase));
	if (!aic_find(iot, ioh))
		panic("aic_isa_attach: aic_find failed");

	sc->sc_ih = isa_intr_establish(ic, ia->ia_irq, IST_EDGE, IPL_BIO,
	    aicintr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	aicattach(sc);
}
