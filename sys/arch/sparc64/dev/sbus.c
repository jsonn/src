/*	$NetBSD: sbus.c,v 1.2.2.2 1998/08/02 00:06:48 eeh Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sbus.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Sbus stuff.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <sparc64/sparc64/vaddrs.h>
#include <sparc64/dev/sbusreg.h>
#include <sparc64/dev/sbusvar.h>
#include <sparc64/sparc64/asm.h>

#include <machine/autoconf.h>
#include <machine/ctlreg.h>
#include <machine/cpu.h>

/* XXXXX -- Needed to allow dvma_mapin to work -- need to switch to bus_dma_* */
struct sbus_softc *sbus0;

#ifdef DEBUG
#define SDB_DVMA	0x1
#define SDB_INTR	0x2
int sbusdebug = 0;
#endif

void sbusreset __P((int));
int sbus_flush __P((struct sbus_softc *));

static bus_space_tag_t sbus_alloc_bustag __P((struct sbus_softc *));
static bus_dma_tag_t sbus_alloc_dmatag __P((struct sbus_softc *));
static int sbus_get_intr __P((struct sbus_softc *, int,
			      struct sbus_intr **, int *));
static int sbus_bus_mmap __P((bus_space_tag_t, bus_type_t, bus_addr_t,
			      int, bus_space_handle_t *));
static int _sbus_bus_map __P((
		bus_space_tag_t,
		bus_type_t,
		bus_addr_t,		/*offset*/
		bus_size_t,		/*size*/
		int,			/*flags*/
		vaddr_t,		/*preferred virtual address */
		bus_space_handle_t *));
static void *sbus_intr_establish __P((
		bus_space_tag_t,
		int,			/*level*/
		int,			/*flags*/
		int (*) __P((void *)),	/*handler*/
		void *));		/*handler arg*/


/* autoconfiguration driver */
int	sbus_match __P((struct device *, struct cfdata *, void *));
void	sbus_attach __P((struct device *, struct device *, void *));


struct cfattach sbus_ca = {
	sizeof(struct sbus_softc), sbus_match, sbus_attach
};

extern struct cfdriver sbus_cd;

/*
 * DVMA routines
 */
void sbus_enter __P((struct sbus_softc *, vaddr_t, int64_t, int));
void sbus_remove __P((struct sbus_softc *, vaddr_t, int));
int sbus_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
			  bus_size_t, struct proc *, int));
void sbus_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void sbus_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
			   bus_size_t, int));
int sbus_dmamem_alloc __P((bus_dma_tag_t tag, bus_size_t size,
			   bus_size_t alignment, bus_size_t boundary,
			   bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags));
void sbus_dmamem_free __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
			   int nsegs));
int sbus_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
			 int nsegs, size_t size, caddr_t *kvap, int flags));
void sbus_dmamem_unmap __P((bus_dma_tag_t tag, caddr_t kva,
			    size_t size));


/*
 * Child devices receive the Sbus interrupt level in their attach
 * arguments. We translate these to CPU IPLs using the following
 * tables. Note: obio bus interrupt levels are identical to the
 * processor IPL.
 *
 * The second set of tables is used when the Sbus interrupt level
 * cannot be had from the PROM as an `interrupt' property. We then
 * fall back on the `intr' property which contains the CPU IPL.
 */

/* Translate Sbus interrupt level to processor IPL */
static int intr_sbus2ipl_4c[] = {
	0, 1, 2, 3, 5, 7, 8, 9
};
static int intr_sbus2ipl_4m[] = {
	0, 2, 3, 5, 7, 9, 11, 13
};

/*
 * This value is or'ed into the attach args' interrupt level cookie
 * if the interrupt level comes from an `intr' property, i.e. it is
 * not an Sbus interrupt level.
 */
#define SBUS_INTR_COMPAT	0x80000000


/*
 * Print the location of some sbus-attached device (called just
 * before attaching that device).  If `sbus' is not NULL, the
 * device was found but not configured; print the sbus as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
sbus_print(args, busname)
	void *args;
	const char *busname;
{
	struct sbus_attach_args *sa = args;
	int i;

	if (busname)
		printf("%s at %s", sa->sa_name, busname);
	printf(" slot %d offset 0x%x", sa->sa_slot, sa->sa_offset);
	for (i=0; i<sa->sa_nintr; i++) {
		struct sbus_intr *sbi = &sa->sa_intr[i];

		printf(" vector %x ipl %d", (int)sbi->sbi_vec, (int)INTLEV(sbi->sbi_pri));
	}
	return (UNCONF);
}

int
sbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * Attach an Sbus.
 */
void
sbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct sbus_softc *sc = sbus0 = (struct sbus_softc *)self;
	struct mainbus_attach_args *ma = aux;
	int node = ma->ma_node;

	int node0, error;
	bus_space_tag_t sbt;
	struct sbus_attach_args sa;
	char *busname = "sbus";
	struct bootpath *bp = ma->ma_bp;


	sc->sc_bustag = ma->ma_bustag;
	sc->sc_dmatag = ma->ma_dmatag;
	sc->sc_sysio = (struct sysioreg*) ma->ma_address[0];	/* Use prom mapping for sysio. */
	sc->sc_ign = ma->ma_interrupts[0] & INTMAP_IGN;		/* Find interrupt group no */

	/* Setup interrupt translation tables */
	sc->sc_intr2ipl = CPU_ISSUN4C
				? intr_sbus2ipl_4c
				: intr_sbus2ipl_4m;

	/*
	 * Record clock frequency for synchronous SCSI.
	 * IS THIS THE CORRECT DEFAULT??
	 */
	sc->sc_clockfreq = getpropint(node, "clock-frequency", 25*1000*1000);
	printf(": clock = %s MHz\n", clockfreq(sc->sc_clockfreq));

	sbt = sbus_alloc_bustag(sc);
	sc->sc_dmatag = sbus_alloc_dmatag(sc);

	/*
	 * Get the SBus burst transfer size if burst transfers are supported
	 */
	sc->sc_burst = getpropint(node, "burst-sizes", 0);

	/* Propagate bootpath */
	if (bp != NULL && strcmp(bp->name, busname) == 0)
		bp++;
	else
		bp = NULL;

	/*
	 * Collect address translations from the OBP.
	 */
	error = getpropA(node, "ranges", sizeof(struct sbus_range),
			 &sc->sc_nrange, (void **)&sc->sc_range);
	switch (error) {
	case 0:
		break;
#if 0
	case ENOENT:
		/* Fall back to our own `range' construction */
		sc->sc_range = sbus_translations;
		sc->sc_nrange =
			sizeof(sbus_translations)/sizeof(sbus_translations[0]);
		break;
#endif
	default:
		panic("%s: error getting ranges property", sc->sc_dev.dv_xname);
	}


	/*
	 * Setup the iommu.
	 *
	 * The sun4u iommu is part of the SBUS controller so we will
	 * deal with it here.  We could try to fake a device node so
	 * we can eventually share it with the PCI bus run by psyco,
	 * but I don't want to get into that sort of cruft.
	 */

	/* 
	 * All IOMMUs will share the same TSB which is allocated in pmap_bootstrap.
	 *
	 * This makes device management easier.
	 */
	{
		extern int64_t		*iotsb;
		extern paddr_t		iotsbp;
		extern int		iotsbsize;
		
		sc->sc_tsbsize = iotsbsize;
		sc->sc_tsb = iotsb;
		sc->sc_ptsb = iotsbp;
	}
#if 0
	/* Need to do 64-bit stores */
	sc->sc_sysio->sys_iommu.iommu_cr = (IOMMUCR_TSB1K|IOMMUCR_8KPG|IOMMUCR_EN);
	sc->sc_sysio->sys_iommu.iommu_tsb = sc->sc_ptsb;
#else
	stxa(&sc->sc_sysio->sys_iommu.iommu_cr,ASI_NUCLEUS,(IOMMUCR_TSB1K|IOMMUCR_8KPG|IOMMUCR_EN));
	stxa(&sc->sc_sysio->sys_iommu.iommu_tsb,ASI_NUCLEUS,sc->sc_ptsb);
#endif
#ifdef DEBUG
	if (sbusdebug & SDB_DVMA)
	{
		/* Probe the iommu */
		int64_t cr, tsb;

		printf("iommu regs at: cr=%x tsb=%x flush=%x\n", &sc->sc_sysio->sys_iommu.iommu_cr,
		       &sc->sc_sysio->sys_iommu.iommu_tsb, &sc->sc_sysio->sys_iommu.iommu_flush);
		cr = sc->sc_sysio->sys_iommu.iommu_cr;
		tsb = sc->sc_sysio->sys_iommu.iommu_tsb;
		printf("iommu cr=%x:%x tsb=%x:%x\n", (long)(cr>>32), (long)cr, (long)(tsb>>32), (long)tsb);
		printf("sysio base %p phys %p TSB base %p phys %p", 
		       (long)sc->sc_sysio, (long)pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_sysio), 
		       (long)sc->sc_tsb, (long)sc->sc_ptsb);
		delay(1000000); /* 1 s */
	}
#endif

	/*
	 * Initialize streaming buffer.
	 */
	sc->sc_flushpa = pmap_extract(pmap_kernel(), (vaddr_t)&sc->sc_flush);
#if 0
	sc->sc_sysio->sys_strbuf.strbuf_ctl = STRBUF_EN; /* Enable diagnostics mode? */
#else
	stxa(&sc->sc_sysio->sys_strbuf.strbuf_ctl,ASI_NUCLEUS,STRBUF_EN);
#endif

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 * `specials' is an array of device names that are treated
	 * specially:
	 */
	node0 = firstchild(node);
	for (node = node0; node; node = nextsibling(node)) {
		char *name = getpropstring(node, "name");

		if (sbus_setup_attach_args(sc, sbt, sc->sc_dmatag,
					   node, bp, &sa) != 0) {
			printf("sbus_attach: %s: incomplete\n", name);
			continue;
		}
		(void) config_found(&sc->sc_dev, (void *)&sa, sbus_print);
		sbus_destroy_attach_args(&sa);
	}
}

int
sbus_setup_attach_args(sc, bustag, dmatag, node, bp, sa)
	struct sbus_softc	*sc;
	bus_space_tag_t		bustag;
	bus_dma_tag_t		dmatag;
	int			node;
	struct bootpath		*bp;
	struct sbus_attach_args	*sa;
{
	/*struct	sbus_reg sbusreg;*/
	/*int	base;*/
	int	error;
	int n;

	bzero(sa, sizeof(struct sbus_attach_args));
	error = getpropA(node, "name", 1, &n, (void **)&sa->sa_name);
	if (error != 0)
		return (error);
	sa->sa_name[n] = '\0';

	sa->sa_bustag = bustag;
	sa->sa_dmatag = dmatag;
	sa->sa_node = node;
	sa->sa_bp = bp;

	error = getpropA(node, "reg", sizeof(struct sbus_reg),
			 &sa->sa_nreg, (void **)&sa->sa_reg);
	if (error != 0) {
		char buf[32];
		if (error != ENOENT ||
		    !node_has_property(node, "device_type") ||
		    strcmp(getpropstringA(node, "device_type", buf),
			   "hierarchical") != 0)
			return (error);
	}
	for (n = 0; n < sa->sa_nreg; n++) {
		/* Convert to relative addressing, if necessary */
		u_int32_t base = sa->sa_reg[n].sbr_offset;
		if (SBUS_ABS(base)) {
			sa->sa_reg[n].sbr_slot = SBUS_ABS_TO_SLOT(base);
			sa->sa_reg[n].sbr_offset = SBUS_ABS_TO_OFFSET(base);
		}
	}

	if ((error = sbus_get_intr(sc, node, &sa->sa_intr, &sa->sa_nintr)) != 0)
		return (error);

	error = getpropA(node, "address", sizeof(u_int32_t),
			 &sa->sa_npromvaddrs, (void **)&sa->sa_promvaddrs);
	if (error != 0 && error != ENOENT)
		return (error);

	return (0);
}

void
sbus_destroy_attach_args(sa)
	struct sbus_attach_args	*sa;
{
	if (sa->sa_name != NULL)
		free(sa->sa_name, M_DEVBUF);

	if (sa->sa_nreg != 0)
		free(sa->sa_reg, M_DEVBUF);

	if (sa->sa_intr)
		free(sa->sa_intr, M_DEVBUF);

	if (sa->sa_promvaddrs)
		free(sa->sa_promvaddrs, M_DEVBUF);

	bzero(sa, sizeof(struct sbus_attach_args));/*DEBUG*/
}


int
_sbus_bus_map(t, btype, offset, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t offset;
	bus_size_t size;
	int	flags;
	vaddr_t vaddr;
	bus_space_handle_t *hp;
{
	struct sbus_softc *sc = t->cookie;
	int64_t slot = btype;
	int i;

	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t paddr;

		if (sc->sc_range[i].cspace != slot)
			continue;

		/* We've found the connection to the parent bus */
		paddr = sc->sc_range[i].poffset + offset;
		paddr |= ((bus_addr_t)sc->sc_range[i].pspace<<32);
#ifdef DEBUG
		if (sbusdebug & SDB_DVMA)
			printf("\n_sbus_bus_map: mapping paddr slot %x offset %x:%x poffset %x paddr %x:%x\n",
			       (int)slot, (int)(offset>>32), (int)offset, (int)sc->sc_range[i].poffset, (int)(paddr>>32), (int)paddr);
#endif
		return (bus_space_map2(sc->sc_bustag, 0, paddr,
					size, flags, vaddr, hp));
	}

	return (EINVAL);
}

int
sbus_bus_mmap(t, btype, paddr, flags, hp)
	bus_space_tag_t t;
	bus_type_t btype;
	bus_addr_t paddr;
	int flags;
	bus_space_handle_t *hp;
{
	bus_addr_t offset = paddr;
	int slot = (paddr>>32);
	struct sbus_softc *sc = t->cookie;
	int i;

	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t paddr;

		if (sc->sc_range[i].cspace != slot)
			continue;

		paddr = sc->sc_range[i].poffset + offset;
		paddr |= ((bus_addr_t)sc->sc_range[i].pspace<<32);
		return (bus_space_mmap(sc->sc_bustag, 0, paddr,
				       flags, hp));
	}

	return (-1);
}


/*
 * Each attached device calls sbus_establish after it initializes
 * its sbusdev portion.
 */
void
sbus_establish(sd, dev)
	register struct sbusdev *sd;
	register struct device *dev;
{
	register struct sbus_softc *sc;
	register struct device *curdev;

	/*
	 * We have to look for the sbus by name, since it is not necessarily
	 * our immediate parent (i.e. sun4m /iommu/sbus/espdma/esp)
	 * We don't just use the device structure of the above-attached
	 * sbus, since we might (in the future) support multiple sbus's.
	 */
	for (curdev = dev->dv_parent; ; curdev = curdev->dv_parent) {
		if (!curdev || !curdev->dv_xname)
			panic("sbus_establish: can't find sbus parent for %s",
			      sd->sd_dev->dv_xname
					? sd->sd_dev->dv_xname
					: "<unknown>" );

		if (strncmp(curdev->dv_xname, "sbus", 4) == 0)
			break;
	}
	sc = (struct sbus_softc *) curdev;

	sd->sd_dev = dev;
	sd->sd_bchain = sc->sc_sbdev;
	sc->sc_sbdev = sd;
}

/*
 * Reset the given sbus. (???)
 */
void
sbusreset(sbus)
	int sbus;
{
	register struct sbusdev *sd;
	struct sbus_softc *sc = sbus_cd.cd_devs[sbus];
	struct device *dev;

	printf("reset %s:", sc->sc_dev.dv_xname);
	for (sd = sc->sc_sbdev; sd != NULL; sd = sd->sd_bchain) {
		if (sd->sd_reset) {
			dev = sd->sd_dev;
			(*sd->sd_reset)(dev);
			printf(" %s", dev->dv_xname);
		}
	}
#if 0
	/* Reload iommu regs */
	sc->sc_sysio->sys_iommu.iommu_cr = (IOMMUCR_TSB1K|IOMMUCR_8KPG|IOMMUCR_EN);
	sc->sc_sysio->sys_iommu.iommu_tsb = sc->sc_ptsb;
	sc->sc_sysio->sys_strbuf.strbuf_ctl = STRBUF_EN; /* Enable diagnostics mode? */
#else
	/* Reload iommu regs */
	stxa(&sc->sc_sysio->sys_iommu.iommu_cr,ASI_NUCLEUS,(IOMMUCR_TSB1K|IOMMUCR_8KPG|IOMMUCR_EN));
	stxa(&sc->sc_sysio->sys_iommu.iommu_tsb,ASI_NUCLEUS,sc->sc_ptsb);
	stxa(&sc->sc_sysio->sys_strbuf.strbuf_ctl,ASI_NUCLEUS,STRBUF_EN);
#endif
}

/*
 * Here are the iommu control routines. 
 */
void
sbus_enter(sc, va, pa, flags)
	struct sbus_softc *sc;
	vaddr_t va;
	int64_t pa;
	int flags;
{
	int64_t tte;

#ifdef DIAGNOSTIC
	if (va < sc->sc_dvmabase)
		panic("sbus_enter: va 0x%x not in DVMA space",va);
#endif

	tte = MAKEIOTTE(pa, !(flags&BUS_DMA_NOWRITE), !(flags&BUS_DMA_NOCACHE), 
			!(flags&BUS_DMA_COHERENT));
	
	/* Is the streamcache flush really needed? */
#if 0
	sc->sc_sysio->sys_strbuf.strbuf_pgflush = va;
#else
	stxa(&(sc->sc_sysio->sys_strbuf.strbuf_pgflush), ASI_NUCLEUS, va);
#endif
	sbus_flush(sc);
	sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)] = tte;
#if 0
	sc->sc_sysio->sys_iommu.iommu_flush = va;
#else
	stxa(&sc->sc_sysio->sys_iommu.iommu_flush,ASI_NUCLEUS,va);
#endif
#ifdef DEBUG
	if (sbusdebug & SDB_DVMA)
		printf("sbus_enter: va %x pa %x:%x TSB[%x]@%p=%x:%x\n",
		       va, (int)(pa>>32), (int)pa, IOTSBSLOT(va,sc->sc_tsbsize), 
		       &sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)],
		       (int)(tte>>32), (int)tte);
#endif
}

/*
 * sbus_clear: clears mappings created by sbus_enter
 *
 * Only demap from IOMMU if flag is set.
 */
void
sbus_remove(sc, va, len)
	struct sbus_softc *sc;
	vaddr_t va;
	int len;
{

#ifdef DIAGNOSTIC
	if (va < sc->sc_dvmabase)
		panic("sbus_remove: va 0x%x not in DVMA space", (int)va);
	if ((int)(va + len) < (int)va)
		panic("sbus_remove: va 0x%x + len 0x%x wraps", 
		      (int) va, (int) len);
#endif

	va = trunc_page(va);
	while (len > 0) {

		/*
		 * Streaming buffer flushes:
		 * 
		 *   1 Tell strbuf to flush by storing va to strbuf_pgflush
		 * If we're not on a cache line boundary (64-bits):
		 *   2 Store 0 in flag
		 *   3 Store pointer to flag in flushsync
		 *   4 wait till flushsync becomes 0x1
		 *
		 * If it takes more than .5 sec, something went wrong.
		 */
#if 0
		sc->sc_sysio->sys_strbuf.strbuf_pgflush = va;
#else
		stxa(&(sc->sc_sysio->sys_strbuf.strbuf_pgflush), ASI_NUCLEUS, va);
#endif
		if (len <= NBPG) {
			sbus_flush(sc);
		}
#ifdef DEBUG
		if (sbusdebug & SDB_DVMA)
			printf("sbus_remove: flushed va %p TSB[%x]@%p=%x:%x\n", 	       
			       (int)va, (int)IOTSBSLOT(va,sc->sc_tsbsize), 
			       (int)&sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)],
			       (int)((sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)])>>32), 
			       (int)(sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)]));
#endif
		sc->sc_tsb[IOTSBSLOT(va,sc->sc_tsbsize)] = 0;
#if 0
		sc->sc_sysio->sys_iommu.iommu_flush = va;
#else
		stxa(&sc->sc_sysio->sys_iommu.iommu_flush, ASI_NUCLEUS, va);
#endif
		len -= NBPG;
		va += NBPG;
	}
}

int 
sbus_flush(sc)
	struct sbus_softc *sc;
{
	extern u_int64_t cpu_clockrate;
	u_int64_t flushtimeout;

	sc->sc_flush = 0;
	/*
	 * KLUGE ALERT KLUGE ALERT
	 *
	 * In order not to bother with pmap_extract() to do the vtop 
	 * translation, flushdone is a static variable that resides in 
	 * the kernel's 4MB locked TTE.  This means that this routine
	 * is NOT re-entrant.  Since we're single-threaded and poll
	 * on this value, this is currently not a problem.
	 */
#ifdef NOTDEF_DEBUG
	if (sbusdebug & SDB_DVMA)
		printf("sbus_remove: flush = %x at va = %x pa = %x:%x\n", 
		       (int)sc->sc_flush, (int)&sc->sc_flush, 
		       (int)(sc->sc_flushpa>>32), (int)sc->sc_flushpa);
#endif
#if 0
	sc->sc_sysio->sys_strbuf.strbuf_flushsync = sc->sc_flushpa;
#else
	stxa(&sc->sc_sysio->sys_strbuf.strbuf_flushsync, ASI_NUCLEUS, sc->sc_flushpa);
#endif
	membar_sync();
	flushtimeout = tick() + cpu_clockrate/2; /* .5 sec after *now* */
	while( !sc->sc_flush && flushtimeout > tick()) membar_sync();
#ifdef DIAGNOSTIC
	if( !sc->sc_flush )
		printf("sbus_remove: flush timeout %x at %x:%x\n", (int)sc->sc_flush, 
		       (int)(sc->sc_flushpa>>32), (int)sc->sc_flushpa); /* panic? */
#endif
	return (sc->sc_flush);
}
/*
 * Get interrupt attributes for an Sbus device.
 */
int
sbus_get_intr(sc, node, ipp, np)
	struct sbus_softc *sc;
	int node;
	struct sbus_intr **ipp;
	int *np;
{
	int *ipl;
	int i, n, error;
	char buf[32];

	/*
	 * The `interrupts' property contains the Sbus interrupt level.
	 */
	ipl = NULL;
	if (getpropA(node, "interrupts", sizeof(int), np, (void **)&ipl) == 0) {
		/* Change format to an `struct sbus_intr' array */
		struct sbus_intr *ip;
		int pri = 0;
		ip = malloc(*np * sizeof(struct sbus_intr), M_DEVBUF, M_NOWAIT);
		if (ip == NULL)
			return (ENOMEM);
		/* Now things get ugly.  We need to take this value which is
		 * the interrupt vector number and encode the IPL into it
		 * somehow. Luckily, the interrupt vector has lots of free
		 * space and we can easily stuff the IPL in there for a while.  
		 */
		getpropstringA(node, "device_type", buf);
		for (i=0; intrmap[i].in_class; i++) {
			if (strcmp(intrmap[i].in_class, buf) == 0) {
				pri = INTLEVENCODE(intrmap[i].in_lev);
				break;
			}
		}
		for (n = 0; n < *np; n++) {
			/* 
			 * We encode vector and priority into sbi_pri so we 
			 * can pass them as a unit.  This will go away if 
			 * sbus_establish ever takes an sbus_intr instead 
			 * of an integer level.
			 * Stuff the real vector in sbi_vec.
			 */
			ip[n].sbi_pri = pri|ipl[n];
			ip[n].sbi_vec = ipl[n];
		}
		free(ipl, M_DEVBUF);
		*ipp = ip;
		return (0);
	}
	
	/* We really don't support the following */
/*	printf("\nWARNING: sbus_get_intr() \"interrupts\" not found -- using \"intr\"\n"); */
/* And some devices don't even have interrupts */
	/*
	 * Fall back on `intr' property.
	 */
	*ipp = NULL;
	error = getpropA(node, "intr", sizeof(struct sbus_intr),
			 np, (void **)ipp);
	switch (error) {
	case 0:
		for (n = *np; n-- > 0;) {
			/* 
			 * Move the interrupt vector into place.
			 * We could remap the level, but the SBUS priorities 
			 * are probably good enough.
			 */
			(*ipp)[n].sbi_vec = (*ipp)[n].sbi_pri;
			(*ipp)[n].sbi_pri |= INTLEVENCODE((*ipp)[n].sbi_pri);
		}
		break;
	case ENOENT:
		error = 0;
		break;
	}

	return (error);
}


/*
 * Install an interrupt handler for an Sbus device.
 */
void *
sbus_intr_establish(t, level, flags, handler, arg)
	bus_space_tag_t t;
	int level;
	int flags;
	int (*handler) __P((void *));
	void *arg;
{
	struct sbus_softc *sc = t->cookie;
	struct intrhand *ih;
	int ipl;

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) != 0)
		ipl = level;
	else if ((level & SBUS_INTR_COMPAT) != 0)
		ipl = level & ~SBUS_INTR_COMPAT;
	else {
		/* Decode and remove IPL */
		ipl = INTLEV(level);
		level = INTVEC(level);
#ifdef DEBUG
		if (sbusdebug & SDB_INTR) {
			printf("\nsbus: intr[%d]%x: %x\n", ipl, level, 
			       intrlev[level]);
			printf("Hunting for IRQ...\n");
		}
#endif
		if ((level & INTMAP_OBIO) == 0) {
			/* We're in an SBUS slot */
			/* Register the map and clear intr registers */
#ifdef DEBUG
			if (sbusdebug & SDB_INTR) {
				int64_t *intrptr = &(&sc->sc_sysio->sbus_slot0_int)[INTSLOT(level)];
				int64_t intrmap = *intrptr;
				
				printf("Found SBUS %x IRQ as %x:%x in slot %d\n", 
				       level, (int)(intrmap>>32), (int)intrmap, 
				       INTSLOT(level));
			}
#endif
			ih->ih_map = &(&sc->sc_sysio->sbus_slot0_int)[INTSLOT(level)];
			ih->ih_clr = &sc->sc_sysio->sbus0_clr_int[INTVEC(level)];
			/* Enable the interrupt */
			level |= INTMAP_V;
			stxa(ih->ih_map, ASI_NUCLEUS, level);
		} else {
			int64_t *intrptr = &sc->sc_sysio->scsi_int_map;
			int64_t intrmap = 0;
			int i;

			/* Insert IGN */
			level |= sc->sc_ign;
			for (i=0;
			     &intrptr[i] <= (int64_t *)&sc->sc_sysio->reserved_int_map &&
				     INTVEC(intrmap=intrptr[i]) != INTVEC(level); 
			     i++);
			if (INTVEC(intrmap) == INTVEC(level)) {
#ifdef DEBUG
				if (sbusdebug & SDB_INTR)
					printf("Found OBIO %x IRQ as %x:%x in slot %d\n", 
					       level, (int)(intrmap>>32), (int)intrmap, 
					       i);
#endif
				/* Register the map and clear intr registers */
				ih->ih_map = &intrptr[i];
				intrptr = (int64_t *)&sc->sc_sysio->scsi_clr_int;
				ih->ih_clr = &intrptr[i];
				/* Enable the interrupt */
				intrmap |= INTMAP_V;
				stxa(ih->ih_map, ASI_NUCLEUS, intrmap);
			} else panic("IRQ not found!");
		}
	}
#ifdef DEBUG
	if (sbusdebug & SDB_INTR) { int i; for (i=0; i<140000000; i++); }
#endif

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_number = level;
	ih->ih_pil = (1<<ipl);
	if ((flags & BUS_INTR_ESTABLISH_FASTTRAP) != 0)
		intr_fasttrap(ipl, (void (*)__P((void)))handler);
	else
		intr_establish(ipl, ih);
	return (ih);
}

static bus_space_tag_t
sbus_alloc_bustag(sc)
	struct sbus_softc *sc;
{
	bus_space_tag_t sbt;

	sbt = (bus_space_tag_t)
		malloc(sizeof(struct sparc_bus_space_tag), M_DEVBUF, M_NOWAIT);
	if (sbt == NULL)
		return (NULL);

	bzero(sbt, sizeof *sbt);
	sbt->cookie = sc;
	sbt->parent = sc->sc_bustag;
	sbt->type = SBUS_BUS_SPACE;
	sbt->sparc_bus_map = _sbus_bus_map;
	sbt->sparc_bus_mmap = sbus_bus_mmap;
	sbt->sparc_intr_establish = sbus_intr_establish;
	return (sbt);
}


static bus_dma_tag_t
sbus_alloc_dmatag(sc)
	struct sbus_softc *sc;
{
	bus_dma_tag_t sdt, psdt = sc->sc_dmatag;

	sdt = (bus_dma_tag_t)
		malloc(sizeof(struct sparc_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	if (sdt == NULL)
		/* Panic? */
		return (psdt);

	sdt->_cookie = sc;
	sdt->_parent = psdt;
#define PCOPY(x)	sdt->x = psdt->x
	PCOPY(_dmamap_create);
	PCOPY(_dmamap_destroy);
	sdt->_dmamap_load = sbus_dmamap_load;
	PCOPY(_dmamap_load_mbuf);
	PCOPY(_dmamap_load_uio);
	PCOPY(_dmamap_load_raw);
	sdt->_dmamap_unload = sbus_dmamap_unload;
	sdt->_dmamap_sync = sbus_dmamap_sync;
	sdt->_dmamem_alloc = sbus_dmamem_alloc;
	sdt->_dmamem_free = sbus_dmamem_free;
	sdt->_dmamem_map = sbus_dmamem_map;
	sdt->_dmamem_unmap = sbus_dmamem_unmap;
	PCOPY(_dmamem_mmap);
#undef	PCOPY
	sc->sc_dmatag = sdt;
	return (sdt);
}

int
sbus_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	int err;
	bus_size_t sgsize;
	paddr_t curaddr;
	vaddr_t  dvmaddr, vaddr = (vaddr_t)buf;
	pmap_t pmap;
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;

	if (map->dm_nsegs) {
		/* Already in use?? */
#ifdef DIAGNOSTIC
		printf("sbus_dmamap_load: map still in use\n");
#endif
		bus_dmamap_unload(t, map);
	}
	if ((err = bus_dmamap_load(t->_parent, map, buf, buflen, p, flags)))
		return (err);

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	dvmaddr = trunc_page(map->dm_segs[0].ds_addr);
	sgsize = round_page(buflen + ((int)vaddr & PGOFSET));
	for (; buflen > 0; ) {
		/*
		 * Get the physical address for this page.
		 */
		if ((curaddr = (bus_addr_t)pmap_extract(pmap, (vaddr_t)vaddr)) == NULL) {
			bus_dmamap_unload(t, map);
			return (-1);
		}

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = NBPG - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

#ifdef DEBUG
		if (sbusdebug & SDB_DVMA)
			printf("sbus_dmamap_load: map %p loading va %x at pa %x\n",
			       map, (int)dvmaddr, (int)(curaddr & ~(NBPG-1)));
#endif
		sbus_enter(sc, trunc_page(dvmaddr), trunc_page(curaddr), flags);
			
		dvmaddr += PAGE_SIZE;
		vaddr += sgsize;
		buflen -= sgsize;
	}
	return (0);
}

void
sbus_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	vaddr_t addr;
	int len;
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;

	if (map->dm_nsegs != 1)
		panic("_sbus_dmamap_unload: nsegs = %d", map->dm_nsegs);

	addr = trunc_page(map->dm_segs[0].ds_addr);
	len = map->dm_segs[0].ds_len;

#ifdef DEBUG
	if (sbusdebug & SDB_DVMA)
		printf("sbus_dmamap_unload: map %p removing va %x size %x\n",
		       map, (int)addr, (int)len);
#endif
	sbus_remove(sc, addr, len);
	bus_dmamap_unload(t->_parent, map);
}


void
sbus_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;
	vaddr_t va = map->dm_segs[0].ds_addr + offset;


	/*
	 * We only support one DMA segment; supporting more makes this code
         * too unweildy.
	 */

	if (ops&BUS_DMASYNC_PREREAD) 
		/* Nothing to do */;
	if (ops&BUS_DMASYNC_POSTREAD) {
		/*
		 * We should sync the IOMMU streaming caches here first.
		 */
		while (len > 0) {
			
			/*
			 * Streaming buffer flushes:
			 * 
			 *   1 Tell strbuf to flush by storing va to strbuf_pgflush
			 * If we're not on a cache line boundary (64-bits):
			 *   2 Store 0 in flag
			 *   3 Store pointer to flag in flushsync
			 *   4 wait till flushsync becomes 0x1
			 *
			 * If it takes more than .5 sec, something went wrong.
			 */
#if 0
			sc->sc_sysio->sys_strbuf.strbuf_pgflush = va;
#else
			stxa(&(sc->sc_sysio->sys_strbuf.strbuf_pgflush), ASI_NUCLEUS, va);
#endif
			if (len <= NBPG) {
				sbus_flush(sc);
			}
			len -= NBPG;
			va += NBPG;
		}
	}
	if (ops&BUS_DMASYNC_PREWRITE) 
		/* Nothing to do */;
	if (ops&BUS_DMASYNC_POSTWRITE) 
		/* Nothing to do */;	
	bus_dmamap_sync(t->_parent, map, offset, len, ops);
}

int
sbus_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	paddr_t curaddr;
	bus_addr_t dvmaddr;
	vm_page_t m;
	struct pglist *mlist;
	int error;
	int n;
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;

	if ((error = bus_dmamem_alloc(t->_parent, size, alignment, 
				     boundary, segs, nsegs, rsegs, flags)))
		return (error);

	for (n=0; n<*rsegs; n++) {
		dvmaddr = segs[n].ds_addr;
		size = segs[n].ds_len;
		mlist = segs[n]._ds_mlist;

		/* Map memory into DVMA space */
		for (m = mlist->tqh_first; m != NULL; m = m->pageq.tqe_next) {
			curaddr = VM_PAGE_TO_PHYS(m);
			sbus_enter(sc, dvmaddr, curaddr, flags);
			dvmaddr += PAGE_SIZE;
		}
	}
	return (0);
}

void
sbus_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	vaddr_t addr;
	int len;
	int n;
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;


	for (n=0; n<nsegs; n++) {
		addr = segs[n].ds_addr;
		len = segs[n].ds_len;
		sbus_remove(sc, addr, len);
	}
	bus_dmamem_free(t->_parent, segs, nsegs);
}

/*
 * Call bus_dmamem_map() to map it into the kernel, then map it into the IOTSB.
 * Check the flags to see whether we're streaming or coherent.
 */
int
sbus_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vm_page_t m;
	vaddr_t va;
	bus_addr_t addr;
	struct pglist *mlist;
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;
	int cbit;
	int rval;

	/* 
	 * First have the parent driver allocate some address space in DVMA space.
	 */
	if ((rval = bus_dmamem_map(t->_parent, segs, nsegs, size, kvap, flags)))
		return (rval);

	/* 
	 * digest flags:
	 */
	cbit = 0;
	if (flags & BUS_DMA_COHERENT)	/* Disable vcache */
		cbit |= PMAP_NVC;
	if (flags & BUS_DMA_NOCACHE)	/* sideffects */
		cbit |= PMAP_NC;
	/*
	 * Now take this and map it both into the CPU and into the IOMMU.
	 */
	va = (vaddr_t)*kvap;
	mlist = segs[0]._ds_mlist;
	for (m = mlist->tqh_first; m != NULL; m = m->pageq.tqe_next) {

		if (size == 0)
			panic("_bus_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, addr | cbit,
			   VM_PROT_READ | VM_PROT_WRITE, TRUE);
		sbus_enter(sc, va, addr, flags);
		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
sbus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{
	struct sbus_softc *sc = (struct sbus_softc *)t->_cookie;
	
#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_bus_dmamem_unmap");
#endif
	
	size = round_page(size);
	sbus_remove(sc, (vaddr_t)kva, size);
	bus_dmamem_unmap(t->_parent, kva, size);
}
