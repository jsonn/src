/*      $NetBSD: if_ze_vsbus.c,v 1.8.6.1 2004/08/03 10:42:45 skrll Exp $ */
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ze_vsbus.c,v 1.8.6.1 2004/08/03 10:42:45 skrll Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/vsbus.h>
#include <machine/cpu.h>
#include <machine/scb.h>
#include <machine/sid.h>

#include <dev/ic/sgecreg.h>
#include <dev/ic/sgecvar.h>

#include "ioconf.h"
/*
 * Addresses.
 */
#define SGECADDR        0x20008000
#define NISA_ROM        0x27800000
#define	SGECVEC		0x108

static	int	zematch(struct device *, struct cfdata *, void *);
static	void	zeattach(struct device *, struct device *, void *);

CFATTACH_DECL(ze_vsbus, sizeof(struct ze_softc),
    zematch, zeattach, NULL, NULL);

/*
 * Check for present SGEC.
 */
int
zematch(parent, cf, aux)
	struct	device *parent;
	struct	cfdata *cf;
	void	*aux;
{
	/*
	 * Should some more intelligent checking be done???
	 * Should for sure force an interrupt instead...
	 */
	if (vax_boardtype != VAX_BTYP_49)
		return 0;

	/* Fool the interrupt system. */
	scb_fake(SGECVEC, 0x15);
	return 12;
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
zeattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct ze_softc *sc = (struct ze_softc *)self;
	struct vsbus_attach_args *va = aux;
	extern struct vax_bus_dma_tag vax_bus_dma_tag;
	int *ea, i;

	/*
	 * Map in SGEC registers.
	 */
	sc->sc_ioh = vax_map_physmem(SGECADDR, 1);
	sc->sc_iot = 0; /* :-) */
	sc->sc_dmat = &vax_bus_dma_tag;

	sc->sc_intvec = SGECVEC;
	scb_vecalloc(va->va_cvec, (void (*)(void *)) sgec_intr,
		sc, SCB_ISTACK, &sc->sc_intrcnt);

	/*
	 * Map in, read and release ethernet rom address.
	 */
	ea = (int *)vax_map_physmem(NISA_ROM, 1);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_enaddr[i] = ea[i] & 0377;
	vax_unmap_physmem((vaddr_t)ea, 1);

	sgec_attach(sc);
}
