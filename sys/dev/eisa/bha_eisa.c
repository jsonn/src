/*	$NetBSD: bha_eisa.c,v 1.9.4.1 1998/01/29 09:59:08 mellon Exp $	*/

/*
 * Copyright (c) 1994, 1996, 1997, 1998 Charles M. Hannum.  All rights reserved.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define	BHA_EISA_IOCONF	0xc8c
#define	BHA_ISA_IOSIZE	4

int	bha_eisa_address __P((bus_space_tag_t, bus_space_handle_t, int *));
#ifdef  __BROKEN_INDIRECT_CONFIG
int	bha_eisa_match __P((struct device *, void *, void *));
#else
int	bha_eisa_match __P((struct device *, struct cfdata *, void *));
#endif
void	bha_eisa_attach __P((struct device *, struct device *, void *));

struct cfattach bha_eisa_ca = {
	sizeof(struct bha_softc), bha_eisa_match, bha_eisa_attach
};

int
bha_eisa_address(iot, ioh, portp)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *portp;
{
	int port;

	switch (bus_space_read_1(iot, ioh, BHA_EISA_IOCONF) & 0x07) {
	case 0x00:
		port = 0x330;
		break;
	case 0x01:
		port = 0x334;
		break;
	case 0x02:
		port = 0x230;
		break;
	case 0x03:
		port = 0x234;
		break;
	case 0x04:
		port = 0x130;
		break;
	case 0x05:
		port = 0x134;
		break;
	default:
		return (1);
	}

	*portp = port;
	return (0);
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
bha_eisa_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh, ioh2;
	int port;
	int rv;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "BUS4201") &&
	    strcmp(ea->ea_idstring, "BUS4202"))
		return (0);

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, 0,
	    &ioh))
		return (0);

	if (bha_eisa_address(iot, ioh, &port) ||
	    bus_space_map(iot, port, BHA_ISA_IOSIZE, 0, &ioh2)) {
		bus_space_unmap(iot, ioh, EISA_SLOT_SIZE);
		return (0);
	}

	rv = bha_find(iot, ioh2, NULL);

	bus_space_unmap(iot, ioh2, BHA_ISA_IOSIZE);
	bus_space_unmap(iot, ioh, EISA_SLOT_SIZE);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_eisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eisa_attach_args *ea = aux;
	struct bha_softc *sc = (void *)self;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh, ioh2;
	int port;
	struct bha_probe_data bpd;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;

	if (!strcmp(ea->ea_idstring, "BUS4201"))
		model = EISA_PRODUCT_BUS4201;
	else if (!strcmp(ea->ea_idstring, "BUS4202"))
		model = EISA_PRODUCT_BUS4202;
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot), EISA_SLOT_SIZE, 0,
	    &ioh))
		panic("bha_eisa_attach: could not map EISA slot");

	if (bha_eisa_address(iot, ioh, &port) ||
	    bus_space_map(iot, port, BHA_ISA_IOSIZE, 0, &ioh2))
		panic("bha_eisa_attach: could not map ISA address");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh2;
	sc->sc_dmat = ea->ea_dmat;
	if (!bha_find(iot, ioh2, &bpd))
		panic("bha_eisa_attach: bha_find failed");

	sc->sc_dmaflags = 0;

	if (eisa_intr_map(ec, bpd.sc_irq, &ih)) {
		printf("%s: couldn't map interrupt (%d)\n",
		    sc->sc_dev.dv_xname, bpd.sc_irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih);
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    bha_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	bha_attach(sc, &bpd);
}
