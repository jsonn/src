/* $NetBSD: atppc_pnpbios.c,v 1.7.2.1 2008/05/18 12:32:14 yamt Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
__KERNEL_RCSID(0, "$NetBSD: atppc_pnpbios.c,v 1.7.2.1 2008/05/18 12:32:14 yamt Exp $");

#include "opt_atppc.h"

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

#include <dev/ic/atppcvar.h>
#include <dev/isa/atppc_isadma.h>

static int	atppc_pnpbios_match(device_t, cfdata_t, void *);
static void	atppc_pnpbios_attach(device_t, device_t, void *);

struct atppc_pnpbios_softc {
	struct atppc_softc sc_atppc;

	isa_chipset_tag_t sc_ic;
	int sc_drq;
};

CFATTACH_DECL_NEW(atppc_pnpbios, sizeof(struct atppc_pnpbios_softc),
    atppc_pnpbios_match, atppc_pnpbios_attach, NULL, NULL);

static int atppc_pnpbios_dma_start(struct atppc_softc *, void *, u_int, 
	uint8_t);
static int atppc_pnpbios_dma_finish(struct atppc_softc *);
static int atppc_pnpbios_dma_abort(struct atppc_softc *);
static int atppc_pnpbios_dma_malloc(device_t, void **, bus_addr_t *, 
	bus_size_t);
static void atppc_pnpbios_dma_free(device_t, void **, bus_addr_t *, 
	bus_size_t);

/*
 * atppc_pnpbios_match: autoconf(9) match routine
 */
static int
atppc_pnpbios_match(device_t parent, cfdata_t match, void *aux)
{
	struct pnpbiosdev_attach_args *aa = aux;

	if (strcmp(aa->idstr, "PNP0400") == 0
	    || strcmp(aa->idstr, "PNP0401") == 0)
		return (1);

	return (0);
}

static void
atppc_pnpbios_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_softc *sc = device_private(self);
	struct atppc_pnpbios_softc *asc = device_private(self);
	struct pnpbiosdev_attach_args *aa = aux;

	sc->sc_dev_ok = ATPPC_NOATTACH;

	printf(": AT Parallel Port\n");

	if (pnpbios_io_map(aa->pbt, aa->resc, 0, &sc->sc_iot, &sc->sc_ioh)) { 	
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	/* find our DRQ */
	if (pnpbios_getdmachan(aa->pbt, aa->resc, 0, &asc->sc_drq)) {
		aprint_error_dev(sc->sc_dev, "unable to get DMA channel\n");
		return;
	}

	pnpbios_print_devres(self, aa);

	/* Attach */
	sc->sc_dev = self;
	sc->sc_has = 0;
	asc->sc_ic = aa->ic;
	sc->sc_dev_ok = ATPPC_ATTACHED;

	sc->sc_ieh = pnpbios_intr_establish(aa->pbt, aa->resc, 0, IPL_TTY,
					    atppcintr, sc);
	sc->sc_has |= ATPPC_HAS_INTR;

	/* setup DMA hooks */
	if (atppc_isadma_setup(sc, asc->sc_ic, asc->sc_drq) == 0) {
		sc->sc_has |= ATPPC_HAS_DMA;
		sc->sc_dma_start = atppc_pnpbios_dma_start;
		sc->sc_dma_finish = atppc_pnpbios_dma_finish;
		sc->sc_dma_abort = atppc_pnpbios_dma_abort;
		sc->sc_dma_malloc = atppc_pnpbios_dma_malloc;
		sc->sc_dma_free = atppc_pnpbios_dma_free;
	}

	/* Run soft configuration attach */
	atppc_sc_attach(sc);
}

/* Start DMA operation over ISA bus */
static int 
atppc_pnpbios_dma_start(struct atppc_softc *lsc, void *buf, u_int nbytes,
	uint8_t mode)
{
	struct atppc_pnpbios_softc * sc = (struct atppc_pnpbios_softc *) lsc;
	
	return atppc_isadma_start(sc->sc_ic, sc->sc_drq, buf, nbytes, mode);
}

/* Stop DMA operation over ISA bus */
static int 
atppc_pnpbios_dma_finish(struct atppc_softc * lsc)
{
	struct atppc_pnpbios_softc * sc = (struct atppc_pnpbios_softc *) lsc;
	
	return atppc_isadma_finish(sc->sc_ic, sc->sc_drq);
}

/* Abort DMA operation over ISA bus */
int 
atppc_pnpbios_dma_abort(struct atppc_softc * lsc)
{
	struct atppc_pnpbios_softc * sc = (struct atppc_pnpbios_softc *) lsc;
	
	return atppc_isadma_abort(sc->sc_ic, sc->sc_drq);
}

/* Allocate memory for DMA over ISA bus */ 
int
atppc_pnpbios_dma_malloc(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_pnpbios_softc * sc = device_private(dev);

	return atppc_isadma_malloc(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

/* Free memory allocated by atppc_isa_dma_malloc() */ 
void 
atppc_pnpbios_dma_free(device_t dev, void ** buf, bus_addr_t * bus_addr, 
	bus_size_t size)
{
	struct atppc_pnpbios_softc * sc = device_private(dev);

	return atppc_isadma_free(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}
