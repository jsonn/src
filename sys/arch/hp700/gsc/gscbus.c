/*	$NetBSD: gscbus.c,v 1.1.4.2 2002/07/14 17:46:23 gehenna Exp $	*/

/*	$OpenBSD: gscbus.c,v 1.13 2001/08/01 20:32:04 miod Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
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

/*
 * Sample IO layouts:
 * 712:
 *
 * f0100000 -- lasi0
 * f0102000 -- lpt0
 * f0104000 -- audio0
 * f0105000 -- com0
 * f0106000 -- siop0
 * f0107000 -- ie0
 * f0108000 -- kbd0
 * f0108100 -- pms0
 * f010a000 -- fdc0
 * f010c000 -- *lasi0
 * f0200000 -- wax0
 * f8000000 -- sti0
 * fffbe000 -- cpu0
 * fffbf000 -- mem0
 *
 * 725/50:
 *
 * f0820000 -- dma
 * f0821000 -- hil
 * f0822000 -- com1
 * f0823000 -- com0
 * f0824000 -- lpt0
 * f0825000 -- siop0
 * f0826000 -- ie0
 * f0827000 -- dma reset
 * f0828000 -- timers
 * f0829000 -- domain kbd
 * f082f000 -- asp0
 * f1000000 -- audio0
 * fc000000 -- eisa0
 * fffbe000 -- cpu0
 * fffbf000 -- mem0
 *
 */

#define GSCDEBUG

#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>

#ifdef	KGDB
#include <sys/kgdb.h>
#include "com.h"
#endif

#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/cpufunc.h>

#include <hp700/gsc/gscbusvar.h>
#include <hp700/hp700/machdep.h>

int	gscmatch __P((struct device *, struct cfdata *, void *));
void	gscattach __P((struct device *, struct device *, void *));

struct cfattach gsc_ca = {
	sizeof(struct gsc_softc), gscmatch, gscattach
};

int	gsc_dmamap_create __P((void *, bus_size_t, int,
			       bus_size_t, bus_size_t, int, bus_dmamap_t *));
void	gsc_dmamap_destroy __P((void *, bus_dmamap_t));
int	gsc_dmamap_load __P((void *, bus_dmamap_t, void *,
			     bus_size_t, struct proc *, int));
int	gsc_dmamap_load_mbuf __P((void *, bus_dmamap_t, struct mbuf *, int));
int	gsc_dmamap_load_uio __P((void *, bus_dmamap_t, struct uio *, int));
int	gsc_dmamap_load_raw __P((void *, bus_dmamap_t,
				 bus_dma_segment_t *, int, bus_size_t, int));
void	gsc_dmamap_unload __P((void *, bus_dmamap_t));
void	gsc_dmamap_sync __P((void *, bus_dmamap_t, bus_addr_t, bus_size_t, bus_dmasync_op_t));

int	gsc_dmamem_alloc __P((void *, bus_size_t, bus_size_t,
			      bus_size_t, bus_dma_segment_t *, int, int *, int));
void	gsc_dmamem_free __P((void *, bus_dma_segment_t *, int));
int	gsc_dmamem_map __P((void *, bus_dma_segment_t *,
			    int, size_t, caddr_t *, int));
void	gsc_dmamem_unmap __P((void *, caddr_t, size_t));
paddr_t	gsc_dmamem_mmap __P((void *, bus_dma_segment_t *, int, off_t, int, int));

int
gscmatch(parent, cf, aux)   
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	return !strcmp(ca->ca_name, "gsc");
}

void
gscattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct gsc_softc *sc = (struct gsc_softc *)self;
	register struct gsc_attach_args *ga = aux;

	sc->sc_iot = ga->ga_iot;
	sc->sc_ic = ga->ga_ic;
	sc->sc_intrmask = 0;
	bzero(sc->sc_intrvs, sizeof(sc->sc_intrvs));

	if (machine_ledaddr)
		printf(": %sleds", machine_ledword? "word" : "");

	printf ("\n");

	/* Add the I/O subsystem's interrupt register. */
	sc->sc_ic->gsc_int_reg.int_reg_dev = parent->dv_xname;
	sc->sc_ih = hp700_intr_establish(&sc->sc_dev, IPL_NONE,
					 NULL, &sc->sc_ic->gsc_int_reg,
					 &int_reg_cpu, ga->ga_irq);

	/* DMA guts */
	sc->sc_dmatag._cookie = sc;
	sc->sc_dmatag._dmamap_create = gsc_dmamap_create;
	sc->sc_dmatag._dmamap_destroy = gsc_dmamap_destroy;
	sc->sc_dmatag._dmamap_load = gsc_dmamap_load;
	sc->sc_dmatag._dmamap_load_mbuf = gsc_dmamap_load_mbuf;
	sc->sc_dmatag._dmamap_load_uio = gsc_dmamap_load_uio;
	sc->sc_dmatag._dmamap_load_raw = gsc_dmamap_load_raw;
	sc->sc_dmatag._dmamap_unload = gsc_dmamap_unload;
	sc->sc_dmatag._dmamap_sync = gsc_dmamap_sync;

	sc->sc_dmatag._dmamem_alloc = gsc_dmamem_alloc;
	sc->sc_dmatag._dmamem_free = gsc_dmamem_free;
	sc->sc_dmatag._dmamem_map = gsc_dmamem_map;
	sc->sc_dmatag._dmamem_unmap = gsc_dmamem_unmap;
	sc->sc_dmatag._dmamem_mmap = gsc_dmamem_mmap;

#ifdef	KGDB
#if NCOM > 0
	if(!strcmp(KGDB_DEVNAME, "com")) {
		int com_gsc_kgdb_attach __P((void));
		if (com_gsc_kgdb_attach() == 0) {
			if (boothowto & RB_KDB) {
				kgdb_connect(1);
			}
		}
        }
#endif
#endif

	pdc_scanbus(self, &ga->ga_ca, ga->ga_mod, MAXMODBUS);
}

int
gscprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	return (UNCONF);
}


void *
gsc_intr_establish(sc, pri, irq, handler, arg, dv)
	struct gsc_softc *sc;
	int pri;
	int irq;
	int (*handler) __P((void *v));
	void *arg;
	struct device *dv;
{
	return (hp700_intr_establish(dv, pri, handler, arg, 
				     &sc->sc_ic->gsc_int_reg, irq));
}

void
gsc_intr_disestablish(sc, v)
	struct gsc_softc *sc;
	void *v;
{
	panic("gsc_intr_disestablish: unimplemented");
}

int
gsc_dmamap_create(v, size, nseg, maxsegsz, boundary, flags, dmamp)
	void *v;
	bus_size_t size;
	int nseg;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	return 0;
}

void
gsc_dmamap_destroy(v, map)
	void *v;
	bus_dmamap_t map;
{
}

int
gsc_dmamap_load(v, map, buf, buflen, p, flags)
	void *v;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_mbuf(v, map, mbuf, flags)
	void *v;
	bus_dmamap_t map;
	struct mbuf *mbuf;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_uio(v, map, uio, flags)
	void *v;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	return 0;
}

int
gsc_dmamap_load_raw(v, map, segs, nsegs, size, flags)
	void *v;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	return 0;
}

void
gsc_dmamap_unload(v, map)
	void *v;
	bus_dmamap_t map;
{

}

void
gsc_dmamap_sync(v, map, addr, size, op)
	void *v;
	bus_dmamap_t map;
	bus_addr_t addr;
	bus_size_t size;
	bus_dmasync_op_t op;
{

}

int
gsc_dmamem_alloc(v, size, alignment, boundary, segs, nsegs, rsegs, flags)
	void *v;
	bus_size_t size;
	bus_size_t alignment;
	bus_size_t boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	return 0;
}

void
gsc_dmamem_free(v, segs, nsegs)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
{

}

int
gsc_dmamem_map(v, segs, nsegs, size, kvap, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	return 0;
}

void
gsc_dmamem_unmap(v, kva, size)
	void *v;
	caddr_t kva;
	size_t size;
{

}

paddr_t
gsc_dmamem_mmap(v, segs, nsegs, off, prot, flags)
	void *v;
	bus_dma_segment_t *segs;
	int nsegs;
	off_t off;
	int prot;
	int flags;
{
	return (-1);
}
