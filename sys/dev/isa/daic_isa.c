/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@netbsd.org>.
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
__KERNEL_RCSID(0, "$NetBSD: daic_isa.c,v 1.1.2.5 2002/04/17 00:05:55 nathanw Exp $");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <sys/socket.h>
#include <net/if.h>
#include <dev/isa/isavar.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_l3l4.h>
#include <dev/ic/daicvar.h>

/* driver state */
struct daic_isa_softc {
	struct daic_softc sc_daic;	/* MI driver state */
	void *sc_ih;			/* interrupt handler */
};

/* local functions */
#ifdef __BROKEN_INDIRECT_CONFIG
static int daic_isa_probe __P((struct device *, void *, void *));
#else
static int daic_isa_probe __P((struct device *, struct cfdata *, void *));
#endif
static void daic_isa_attach __P((struct device *, struct device *, void *));
static int daic_isa_intr __P((void *));

struct cfattach daic_isa_ca = {
	sizeof(struct daic_isa_softc), daic_isa_probe, daic_isa_attach
};

static int
#ifdef __BROKEN_INDIRECT_CONFIG
daic_isa_probe(parent, match, aux)
#else
daic_isa_probe(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;
	int card, need_unmap = 0;

	/* We need some controller memory to comunicate! */
	if (ia->ia_iomem[0].ir_addr == 0 || ia->ia_iomem[0].ir_size == -1)
		goto bad;

	/* Map card RAM. */
	ia->ia_iomem[0].ir_size = DAIC_ISA_MEMSIZE;
	ia->ia_nio = 0;
	ia->ia_ndrq = 0;
	ia->ia_nirq = 1;
	ia->ia_niomem = 1;
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_size,
	    0, &memh))
		goto bad;
	need_unmap = 1;

	/* MI check for card at this location */
	card = daic_probe(memt, memh);
	if (card < 0)
		goto bad;
	if (card == DAIC_TYPE_QUAD)
		ia->ia_iomem[0].ir_size = DAIC_ISA_QUADSIZE;

	bus_space_unmap(memt, memh, DAIC_ISA_MEMSIZE);
	return 1;

bad:
	/* unmap card RAM if already mapped */
	if (need_unmap)
		bus_space_unmap(memt, memh, DAIC_ISA_MEMSIZE);
	return 0;
}

static void
daic_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct daic_isa_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t memh;

	/* Map card RAM. */
	if (bus_space_map(memt, ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_size,
	    0, &memh))
		return;

	sc->sc_daic.sc_iot = memt;
	sc->sc_daic.sc_ioh = memh;

	/* MI initialization of card */
	daic_attach(self, &sc->sc_daic);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_NET, daic_isa_intr, sc);
}

/*
 * Controller interrupt.
 */
static int
daic_isa_intr(arg)
	void *arg;
{
	struct daic_isa_softc *sc = arg;
	return daic_intr(&sc->sc_daic);
}
