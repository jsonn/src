/*	$NetBSD: obmem.c,v 1.2.2.2 2001/04/21 17:54:57 bouyer Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Matthew Fredette.
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
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>

#include <sun2/sun2/control.h>
#include <sun2/sun2/machdep.h>

static int  obmem_match __P((struct device *, struct cfdata *, void *));
static void obmem_attach __P((struct device *, struct device *, void *));

struct obmem_softc {
	struct device	sc_dev;		/* base device */
	bus_space_tag_t	sc_bustag;	/* parent bus tag */
	bus_dma_tag_t	sc_dmatag;	/* parent bus dma tag */
};

struct cfattach obmem_ca = {
	sizeof(struct obmem_softc), obmem_match, obmem_attach
};

static	int obmem_bus_mmap __P((bus_space_tag_t, bus_type_t, bus_addr_t,
			       int, bus_space_handle_t *));
static	int _obmem_bus_map __P((bus_space_tag_t, bus_type_t, bus_addr_t,
			       bus_size_t, int,
			       vaddr_t, bus_space_handle_t *));

static struct sun2_bus_space_tag obmem_space_tag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	_obmem_bus_map,			/* bus_space_map */ 
	NULL,				/* bus_space_unmap */
	NULL,				/* bus_space_subregion */
	NULL,				/* bus_space_barrier */ 
	obmem_bus_mmap,			/* bus_space_mmap */ 
	NULL				/* bus_intr_establish */
}; 

static int
obmem_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	return (ca->ca_name == NULL || strcmp(cf->cf_driver->cd_name, ca->ca_name) == 0);
}

static void
obmem_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct obmem_softc *sc = (struct obmem_softc *)self;
	struct confargs sub_ca;
	const char *const *cpp;
	static const char *const special[] = {
		/* find these first */
		NULL
	};

	/*
	 * There is only one obmem bus
	 */
	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	sc->sc_bustag = ca->ca_bustag;
	sc->sc_dmatag = ca->ca_dmatag;

	obmem_space_tag.cookie = sc;
	obmem_space_tag.parent = sc->sc_bustag;

	/*
	 * Prepare the skeleton attach arguments for our devices.
	 * The values we give in the locators are indications to
	 * sun2_bus_search about which locators must and must not
	 * be defined.
	 */
	sub_ca = *ca;
	sub_ca.ca_bustag = &obmem_space_tag;
	sub_ca.ca_intpri = LOCATOR_OPTIONAL;
	sub_ca.ca_intvec = LOCATOR_FORBIDDEN;

	/* Find all `early' obmem devices */
	for (cpp = special; *cpp != NULL; cpp++) {
		sub_ca.ca_name = *cpp;
		(void)config_search(sun2_bus_search, self, &sub_ca);
	}

	/* Find all other obmem devices */
	sub_ca.ca_name = NULL;
	(void)config_search(sun2_bus_search, self, &sub_ca);
}

int
_obmem_bus_map(t, btype, paddr, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t paddr;
	bus_size_t size;
	int	flags;
	vaddr_t vaddr;
	bus_space_handle_t *hp;
{
	struct obmem_softc *sc = t->cookie;

	return (bus_space_map2(sc->sc_bustag, PMAP_OBMEM, paddr,
				size, flags, vaddr, hp));
}

int
obmem_bus_mmap(t, btype, paddr, flags, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t paddr;
	int flags;
	bus_space_handle_t *hp;
{
	struct obmem_softc *sc = t->cookie;

	return (bus_space_mmap(sc->sc_bustag, PMAP_OBMEM, paddr, flags, hp));
}
