/* $NetBSD: pckbc_jazzio.c,v 1.1.4.2 2000/06/22 16:59:17 minoura Exp $ */
/* NetBSD: pckbc_isa.c,v 1.2 2000/03/23 07:01:35 thorpej Exp  */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h> 
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/lock.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <arc/pica/pica.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <arc/jazz/pckbc_jazzioreg.h>

#define PICA_KBCMDP	(PICA_SYS_KBD + JAZZIO_KBCMDP)

int	pckbc_jazzio_match __P((struct device *, struct cfdata *, void *));
void	pckbc_jazzio_attach __P((struct device *, struct device *, void *));
void	pckbc_jazzio_intr_establish __P((struct pckbc_softc *, pckbc_slot_t));

struct pckbc_jazzio_softc {
	struct pckbc_softc sc_pckbc;

	struct confargs *sc_ca[PCKBC_NSLOTS];
};

struct cfattach pckbc_jazzio_ca = {
	sizeof(struct pckbc_jazzio_softc),
	pckbc_jazzio_match, pckbc_jazzio_attach,
};

extern struct arc_bus_space pica_bus; /* XXX */

int
pckbc_jazzio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct confargs *ca = aux;
	bus_space_tag_t iot = &pica_bus;
	bus_space_handle_t ioh_d, ioh_c;
	bus_addr_t addr;
	int res, ok = 1;

	if(!BUS_MATCHNAME(ca, "pckbd"))
		return(0);

	addr = (bus_addr_t)BUS_CVTADDR(ca);
	if (pckbc_is_console(iot, addr) == 0) {
		if (bus_space_map(iot, addr + KBDATAP, 1, 0, &ioh_d))
			return (0);
		if (bus_space_map(iot, PICA_KBCMDP, 1, 0, &ioh_c)) {
			bus_space_unmap(iot, ioh_d, 1);
			return (0);
		}

		/* flush KBC */
		(void) pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);

		/* KBC selftest */
		if (pckbc_send_cmd(iot, ioh_c, KBC_SELFTEST) == 0) {
			ok = 0;
			goto out;
		}
		res = pckbc_poll_data1(iot, ioh_d, ioh_c, PCKBC_KBD_SLOT, 0);
		if (res != 0x55) {
			printf("kbc selftest: %x\n", res);
			ok = 0;
		}
 out:
		bus_space_unmap(iot, ioh_d, 1);
		bus_space_unmap(iot, ioh_c, 1);
	}

	return (ok);
}

void
pckbc_jazzio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct pckbc_jazzio_softc *jsc = (void *)self;
	struct pckbc_softc *sc = &jsc->sc_pckbc;
	struct pckbc_internal *t;
	bus_space_tag_t iot = &pica_bus;
	bus_space_handle_t ioh_d, ioh_c;
	bus_addr_t addr;
	static struct confargs pms_ca = { "pms", 8, NULL, }; /* XXX */

	sc->intr_establish = pckbc_jazzio_intr_establish;

	/*
	 * To establish interrupt handler
	 *
	 * XXX handcrafting "aux" slot...
	 */
	pms_ca.ca_bus = ca->ca_bus;
	jsc->sc_ca[PCKBC_KBD_SLOT] = ca;
	jsc->sc_ca[PCKBC_AUX_SLOT] = &pms_ca;

	addr = (bus_addr_t)BUS_CVTADDR(ca);
	if (pckbc_is_console(iot, addr)) {
		t = &pckbc_consdata;
		ioh_d = t->t_ioh_d;
		ioh_c = t->t_ioh_c;
		pckbc_console_attached = 1;
		/* t->t_cmdbyte was initialized by cnattach */
	} else {
		if (bus_space_map(iot, addr + KBDATAP, 1, 0, &ioh_d) ||
		    bus_space_map(iot, PICA_KBCMDP, 1, 0, &ioh_c))
			panic("pckbc_attach: couldn't map");

		t = malloc(sizeof(struct pckbc_internal), M_DEVBUF, M_WAITOK);
		bzero(t, sizeof(struct pckbc_internal));
		t->t_iot = iot;
		t->t_ioh_d = ioh_d;
		t->t_ioh_c = ioh_c;
		t->t_addr = addr;
		t->t_cmdbyte = KC8_CPU; /* Enable ports */
		callout_init(&t->t_cleanup);
	}

	t->t_sc = sc;
	sc->id = t;

	printf("\n");

	/* Finish off the attach. */
	pckbc_attach(sc);
}

void
pckbc_jazzio_intr_establish(sc, slot)
	struct pckbc_softc *sc;
	pckbc_slot_t slot;
{
	struct pckbc_jazzio_softc *jsc = (void *) sc;

	BUS_INTR_ESTABLISH(jsc->sc_ca[slot], pckbcintr, sc);
}
