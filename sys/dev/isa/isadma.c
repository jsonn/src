/*	$NetBSD: isadma.c,v 1.23.2.4 1997/05/29 22:34:05 mycroft Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Device driver for the ISA on-board DMA controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/isadmareg.h>

/*
 * High byte of DMA address is stored in this DMAPG register for
 * the Nth DMA channel.
 */
static int dmapageport[2][4] = {
	{0x7, 0x3, 0x1, 0x2},
	{0xf, 0xb, 0x9, 0xa}
};

static u_int8_t dmamode[4] = {
	DMA37MD_READ | DMA37MD_SINGLE,
	DMA37MD_WRITE | DMA37MD_SINGLE,
	DMA37MD_READ | DMA37MD_SINGLE | DMA37MD_LOOP,
	DMA37MD_WRITE | DMA37MD_SINGLE | DMA37MD_LOOP
};

static inline void isa_dmaunmask __P((struct isa_softc *, int));
static inline void isa_dmamask __P((struct isa_softc *, int));

static inline void
isa_dmaunmask(sc, chan)
	struct isa_softc *sc;
	int chan;
{
	int ochan = chan & 3;

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0)
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h,
		    DMA1_SMSK, ochan | DMA37SM_CLEAR);
	else
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h,
		    DMA2_SMSK, ochan | DMA37SM_CLEAR);
}

static inline void
isa_dmamask(sc, chan)
	struct isa_softc *sc;
	int chan;
{
	int ochan = chan & 3;

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h,
		    DMA1_SMSK, ochan | DMA37SM_SET);
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h,
		    DMA1_FFC, 0);
	} else {
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h,
		    DMA2_SMSK, ochan | DMA37SM_SET);
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h,
		    DMA2_FFC, 0);
	}
}

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isa_dmacascade(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	int ochan = chan & 3;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	if (ISA_DRQ_ISFREE(sc, chan) == 0) {
		printf("%s: DRQ %d is not free\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	ISA_DRQ_ALLOC(sc, chan);

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0)
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h,
		    DMA1_MODE, ochan | DMA37MD_CASCADE);
	else
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h,
		    DMA2_MODE, ochan | DMA37MD_CASCADE);

	isa_dmaunmask(sc, chan);
	return;

 lose:
	panic("isa_dmacascade");
}

int
isa_dmamap_create(isadev, chan, size, flags)
	struct device *isadev;
	int chan;
	bus_size_t size;
	int flags;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_size_t maxsize;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	if (chan & 4)
		maxsize = (1 << 17);
	else
		maxsize = (1 << 16);

	if (size > maxsize)
		return (EINVAL);

	if (ISA_DRQ_ISFREE(sc, chan) == 0) {
		printf("%s: drq %d is not free\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	ISA_DRQ_ALLOC(sc, chan);

	return (bus_dmamap_create(sc->sc_dmat, size, 1, size, maxsize,
	    flags, &sc->sc_dmamaps[chan]));

 lose:
	panic("isa_dmamap_create");
}

void
isa_dmamap_destroy(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	if (ISA_DRQ_ISFREE(sc, chan)) {
		printf("%s: drq %d is already free\n",
		    sc->sc_dev.dv_xname, chan);
		goto lose;
	}

	ISA_DRQ_FREE(sc, chan);

	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamaps[chan]);
	return;

 lose:
	panic("isa_dmamap_destroy");
}

/*
 * isa_dmastart(): program 8237 DMA controller channel and set it
 * in motion.
 */
int
isa_dmastart(isadev, chan, addr, nbytes, p, flags, busdmaflags)
	struct device *isadev;
	int chan;
	void *addr;
	bus_size_t nbytes;
	struct proc *p;
	int flags;
	int busdmaflags;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dmamap_t dmam;
	bus_addr_t dmaaddr;
	int waport;
	int ochan = chan & 3;
	int error;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		goto lose;
	}

#ifdef ISADMA_DEBUG
	printf("isa_dmastart: drq %d, addr %p, nbytes 0x%lx, p %p, "
	    "flags 0x%x, dmaflags 0x%x\n",
	    chan, addr, nbytes, p, flags, busdmaflags);
#endif

	if (chan & 4) {
		if (nbytes > (1 << 17) || nbytes & 1 || (u_long)addr & 1) {
			printf("%s: drq %d, nbytes 0x%lx, addr %p\n",
			    sc->sc_dev.dv_xname, chan, nbytes, addr);
			goto lose;
		}
	} else {
		if (nbytes > (1 << 16)) {
			printf("%s: drq %d, nbytes 0x%lx\n",
			    sc->sc_dev.dv_xname, chan, nbytes);
			goto lose;
		}
	}

	dmam = sc->sc_dmamaps[chan];

	error = bus_dmamap_load(sc->sc_dmat, dmam, addr, nbytes,
	    p, busdmaflags);
	if (error)
		return (error);

#ifdef ISADMA_DEBUG
	__asm(".globl isa_dmastart_afterload ; isa_dmastart_afterload:");
#endif

	if (flags & DMAMODE_READ) {
		bus_dmamap_sync(sc->sc_dmat, dmam, BUS_DMASYNC_PREREAD);
		sc->sc_dmareads |= (1 << chan);
	} else {
		bus_dmamap_sync(sc->sc_dmat, dmam, BUS_DMASYNC_PREWRITE);
		sc->sc_dmareads &= ~(1 << chan);
	}

	dmaaddr = dmam->dm_segs[0].ds_addr;

#ifdef ISADMA_DEBUG
	printf("     dmaaddr 0x%lx\n", dmaaddr);

	__asm(".globl isa_dmastart_aftersync ; isa_dmastart_aftersync:");
#endif

	sc->sc_dmalength[chan] = nbytes;

	isa_dmamask(sc, chan);
	sc->sc_dmafinished &= ~(1 << chan);

	if ((chan & 4) == 0) {
		/* set dma channel mode */
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h, DMA1_MODE,
		    ochan | dmamode[flags]);

		/* send start address */
		waport = DMA1_CHN(ochan);
		bus_space_write_1(sc->sc_iot, sc->sc_dmapgh,
		    dmapageport[0][ochan], (dmaaddr >> 16) & 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h, waport,
		    dmaaddr & 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h, waport,
		    (dmaaddr >> 8) & 0xff);

		/* send count */
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h, waport + 1,
		    (--nbytes) & 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_dma1h, waport + 1,
		    (nbytes >> 8) & 0xff);
	} else {
		/* set dma channel mode */
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h, DMA2_MODE,
		    ochan | dmamode[flags]);

		/* send start address */
		waport = DMA2_CHN(ochan);
		bus_space_write_1(sc->sc_iot, sc->sc_dmapgh,
		    dmapageport[1][ochan], (dmaaddr >> 16) & 0xff);
		dmaaddr >>= 1;
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h, waport,
		    dmaaddr & 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h, waport,
		    (dmaaddr >> 8) & 0xff);

		/* send count */
		nbytes >>= 1;
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h, waport + 2,
		    (--nbytes) & 0xff);
		bus_space_write_1(sc->sc_iot, sc->sc_dma2h, waport + 2,
		    (nbytes >> 8) & 0xff);
	}

	isa_dmaunmask(sc, chan);
	return (0);

 lose:
	panic("isa_dmastart");
}

void
isa_dmaabort(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmaabort");
	}

	isa_dmamask(sc, chan);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamaps[chan]);
	sc->sc_dmareads &= ~(1 << chan);
}

bus_size_t
isa_dmacount(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	int waport;
	bus_size_t nbytes;
	int ochan = chan & 3;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmacount");
	}

	isa_dmamask(sc, chan);

	/*
	 * We have to shift the byte count by 1.  If we're in auto-initialize
	 * mode, the count may have wrapped around to the initial value.  We
	 * can't use the TC bit to check for this case, so instead we compare
	 * against the original byte count.
	 * If we're not in auto-initialize mode, then the count will wrap to
	 * -1, so we also handle that case.
	 */
	if ((chan & 4) == 0) {
		waport = DMA1_CHN(ochan);
		nbytes = bus_space_read_1(sc->sc_iot, sc->sc_dma1h,
		    waport + 1) + 1;
		nbytes += bus_space_read_1(sc->sc_iot, sc->sc_dma1h,
		    waport + 1) << 8;
		nbytes &= 0xffff;
	} else {
		waport = DMA2_CHN(ochan);
		nbytes = bus_space_read_1(sc->sc_iot, sc->sc_dma2h,
		    waport + 2) + 1;
		nbytes += bus_space_read_1(sc->sc_iot, sc->sc_dma2h,
		    waport + 2) << 8;
		nbytes <<= 1;
		nbytes &= 0x1ffff;
	}

	if (nbytes == sc->sc_dmalength[chan])
		nbytes = 0;

	isa_dmaunmask(sc, chan);
	return (nbytes);
}

int
isa_dmafinished(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmafinished");
	}

	/* check that the terminal count was reached */
	if ((chan & 4) == 0)
		sc->sc_dmafinished |= bus_space_read_1(sc->sc_iot,
		    sc->sc_dma1h, DMA1_SR) & 0x0f;
	else
		sc->sc_dmafinished |= (bus_space_read_1(sc->sc_iot,
		    sc->sc_dma2h, DMA2_SR) & 0x0f) << 4;

	return ((sc->sc_dmafinished & (1 << chan)) != 0);
}

void
isa_dmadone(isadev, chan)
	struct device *isadev;
	int chan;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dmamap_t dmam;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmadone");
	}

	dmam = sc->sc_dmamaps[chan];

	isa_dmamask(sc, chan);

	if (isa_dmafinished(isadev, chan) == 0)
		printf("%s: isa_dmadone: channel %d not finished\n",
		    sc->sc_dev.dv_xname, chan);

	bus_dmamap_sync(sc->sc_dmat, dmam,
	    (sc->sc_dmareads & (1 << chan)) ? BUS_DMASYNC_POSTREAD :
	    BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->sc_dmat, dmam);
	sc->sc_dmareads &= ~(1 << chan);
}

int
isa_dmamem_alloc(isadev, chan, size, addrp, flags)
	struct device *isadev;
	int chan;
	bus_size_t size;
	bus_addr_t *addrp;
	int flags;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dma_segment_t seg;
	int error, boundary, rsegs;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmamem_alloc");
	}

	boundary = (chan & 4) ? (1 << 17) : (1 << 16);

	size = round_page(size);

	error = bus_dmamem_alloc(sc->sc_dmat, size, NBPG, boundary,
	    &seg, 1, &rsegs, flags);
	if (error)
		return (error);

	*addrp = seg.ds_addr;
	return (0);
}

void
isa_dmamem_free(isadev, chan, addr, size)
	struct device *isadev;
	int chan;
	bus_addr_t addr;
	bus_size_t size;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dma_segment_t seg;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmamem_free");
	}

	seg.ds_addr = addr;
	seg.ds_len = size;

	bus_dmamem_free(sc->sc_dmat, &seg, 1);
}

int
isa_dmamem_map(isadev, chan, addr, size, kvap, flags)
	struct device *isadev;
	int chan;
	bus_addr_t addr;
	bus_size_t size;
	caddr_t *kvap;
	int flags;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dma_segment_t seg;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmamem_map");
	}

	seg.ds_addr = addr;
	seg.ds_len = size;

	return (bus_dmamem_map(sc->sc_dmat, &seg, 1, size, kvap, flags));
}

void
isa_dmamem_unmap(isadev, chan, kva, size)
	struct device *isadev;
	int chan;
	caddr_t kva;
	size_t size;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmamem_unmap");
	}

	bus_dmamem_unmap(sc->sc_dmat, kva, size);
}

int
isa_dmamem_mmap(isadev, chan, addr, size, off, prot, flags)
	struct device *isadev;
	int chan;
	bus_addr_t addr;
	bus_size_t size;
	int off, prot, flags;
{
	struct isa_softc *sc = (struct isa_softc *)isadev;
	bus_dma_segment_t seg;

	if (chan < 0 || chan > 7) {
		printf("%s: bogus drq %d\n", sc->sc_dev.dv_xname, chan);
		panic("isa_dmamem_mmap");
	}

	seg.ds_addr = addr;
	seg.ds_len = size;

	return (bus_dmamem_mmap(sc->sc_dmat, &seg, 1, off, prot, flags));
}
