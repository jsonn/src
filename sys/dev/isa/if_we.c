/*	$NetBSD: if_we.c,v 1.1.2.2 1997/11/05 19:10:18 thorpej Exp $	*/

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
 * Device driver for National Semiconductor DS8390/WD83C690 based ethernet
 * adapters.
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * Device driver for the Western Digital/SMC 8003 and 8013 series,
 * and the SMC Elite Ultra (8216).
 */

#include "bpfilter.h"
#include "rnd.h" 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h> 
#include <netinet/ip.h>
#include <netinet/if_inarp.h> 
#endif 

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/isa/if_wereg.h>

struct we_softc {
	struct dp8390_softc sc_dp8390;

	bus_space_tag_t sc_asict;	/* space tag for ASIC */
	bus_space_handle_t sc_asich;	/* space handle for ASIC */

	bus_space_tag_t sc_delayt;	/* space tag for `delay port' */
	bus_space_handle_t sc_delayh;	/* space handle for `delay port' */

	u_int8_t sc_laar_proto;
	u_int8_t sc_msr_proto;

	u_int8_t sc_type;		/* our type */

	int sc_16bitp;			/* are we 16 bit? */

	void *sc_ih;			/* interrupt handle */
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	we_probe __P((struct device *, void *, void *));
#else
int	we_probe __P((struct device *, struct cfdata *, void *));
#endif
void	we_attach __P((struct device *, struct device *, void *));

struct cfattach we_ca = {
	sizeof(struct we_softc), we_probe, we_attach
};

struct cfdriver we_cd = {
	NULL, "we", DV_IFNET
};

const char *we_params __P((bus_space_tag_t, bus_space_handle_t, u_int8_t *,
	    bus_size_t *, int *, int *));
void	we_set_media __P((struct we_softc *, int));

int	we_mediachange __P((struct dp8390_softc *));
void	we_mediastatus __P((struct dp8390_softc *, struct ifmediareq *));

void	we_recv_int __P((struct dp8390_softc *));
void	we_init_card __P((struct dp8390_softc *));
int	we_write_mbuf __P((struct dp8390_softc *, struct mbuf *, int));
int	we_ring_copy __P((struct dp8390_softc *, int, caddr_t, u_short));
void	we_read_hdr __P((struct dp8390_softc *, int, struct dp8390_ring *));
int	we_test_mem __P((struct dp8390_softc *));

__inline void we_writemem __P((struct we_softc *, u_int8_t *, int, int));
__inline void we_readmem __P((struct we_softc *, int, u_int8_t *, int));

static const int we_584_irq[] = {
	9, 3, 5, 7, 10, 11, 15, 4,
};
#define	NWE_584_IRQ	(sizeof(we_584_irq) / sizeof(we_584_irq[0]))

static const int we_790_irq[] = {
	IRQUNK, 9, 3, 5, 7, 10, 11, 15,
};
#define	NWE_790_IRQ	(sizeof(we_790_irq) / sizeof(we_790_irq[0]))

int we_media[] = {
	IFM_ETHER|IFM_10_2,
	IFM_ETHER|IFM_10_5,
};
#define	NWE_MEDIA	(sizeof(we_media) / sizeof(we_media[0]))

/*
 * Delay needed when switching 16-bit access to shared memory.
 */
#define	WE_DELAY(wsc) \
do { \
	(void) bus_space_read_1((wsc)->sc_delayt, (wsc)->sc_delayh, 0); \
	(void) bus_space_read_1((wsc)->sc_delayt, (wsc)->sc_delayh, 0); \
} while (0)

/*
 * Enable card RAM, and 16-bit access.
 */
#define	WE_MEM_ENABLE(wsc) \
do { \
	if ((wsc)->sc_16bitp) \
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
		    WE_LAAR, (wsc)->sc_laar_proto | WE_LAAR_M16EN); \
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
	    WE_MSR, wsc->sc_msr_proto | WE_MSR_MENB); \
	WE_DELAY((wsc)); \
} while (0)

/*
 * Disable card RAM, and 16-bit access.
 */
#define	WE_MEM_DISABLE(wsc) \
do { \
	bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
	    WE_MSR, (wsc)->sc_msr_proto); \
	if ((wsc)->sc_16bitp) \
		bus_space_write_1((wsc)->sc_asict, (wsc)->sc_asich, \
		    WE_LAAR, (wsc)->sc_laar_proto); \
	WE_DELAY((wsc)); \
} while (0)

int
we_probe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = ((struct device *)match)->dv_cfdata;
#else
	struct cfdata *cf = match;
#endif
	bus_space_tag_t asict, memt;
	bus_space_handle_t asich, memh;
	bus_size_t memsize;
	int asich_valid, memh_valid;
	int i, is790, rv = 0;
	u_int8_t x, type;

	asict = ia->ia_iot;
	memt = ia->ia_memt;

	asich_valid = memh_valid = 0;

	/* Disallow wildcarded i/o addresses. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Disallow wildcarded mem address. */
	if (ia->ia_maddr == ISACF_IOMEM_DEFAULT)
		return (0);

	/* Attempt to map the device. */
	if (bus_space_map(asict, ia->ia_iobase, WE_NPORTS, 0, &asich))
		goto out;
	asich_valid = 1;

#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_POW);
#endif

	/*
	 * Attempt to do a checksum over the station address PROM.
	 * If it fails, it's probably not a WD/SMC board.  There is
	 * a problem with this, though.  Some clone WD8003E boards
	 * (e.g. Danpex) won't pass the checksum.  In this case,
	 * the checksum byte always seems to be 0.
	 */
	for (x = 0, i = 0; i < 8; i++)
		x += bus_space_read_1(asict, asich, WE_PROM + i);

	if (x != WE_ROM_CHECKSUM_TOTAL) {
		/* Make sure it's an 8003E clone... */
		if (bus_space_read_1(asict, asich, WE_CARD_ID) !=
		    WE_TYPE_WD8003E)
			goto out;

		/* Check the checksum byte. */
		if (bus_space_read_1(asict, asich, WE_PROM + 7) != 0)
			goto out;
	}

	/*
	 * Reset the card to force it into a known state.
	 */
#ifdef TOSH_ETHER
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST | WE_MSR_POW);
#else
	bus_space_write_1(asict, asich, WE_MSR, WE_MSR_RST);
#endif
	delay(100);

	bus_space_write_1(asict, asich, WE_MSR,
	    bus_space_read_1(asict, asich, WE_MSR) & ~WE_MSR_RST);

	/* Wait in case the card is reading it's EEPROM. */
	delay(5000);

	/*
	 * Get parameters.
	 */
	if (we_params(asict, asich, &type, &memsize, NULL, &is790) == NULL)
		goto out;

	/* Allow user to override probed value. */
	if (ia->ia_msize)
		memsize = ia->ia_msize;

	/* Attempt to map the memory space. */
	if (bus_space_map(memt, ia->ia_maddr, memsize, 0, &memh))
		goto out;
	memh_valid = 1;

	/*
	 * If possible, get the assigned interrupt number from the card
	 * and use it.
	 */
	if (is790) {
		u_int8_t hwr;

		/* Assemble together the encoded interrupt number. */
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);

		x = bus_space_read_1(asict, asich, WE790_GCR);
		i = ((x & WE790_GCR_IR2) >> 4) |
		    ((x & (WE790_GCR_IR1|WE790_GCR_IR0)) >> 2);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr & ~WE790_HWR_SWH);

		if (ia->ia_irq != IRQUNK && ia->ia_irq != we_790_irq[i])
			printf("%s%d: overriding IRQ %d to %d\n",
			    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
			    we_790_irq[i]);
		ia->ia_irq = we_790_irq[i];
	} else if (type & WE_SOFTCONFIG) {
		/* Assemble together the encoded interrupt number. */
		i = (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_IR2) |
		    ((bus_space_read_1(asict, asich, WE_IRR) &
		      (WE_IRR_IR0 | WE_IRR_IR1)) >> 5);

		if (ia->ia_irq != IRQUNK && ia->ia_irq != we_584_irq[i])
			printf("%s%d: overriding IRQ %d to %d\n",
			    we_cd.cd_name, cf->cf_unit, ia->ia_irq,
			    we_584_irq[i]);
		ia->ia_irq = we_584_irq[i];
	}

	/* So, we say we've found it! */
	ia->ia_iosize = WE_NPORTS;
	ia->ia_msize = memsize;
	rv = 1;

 out:
	if (asich_valid)
		bus_space_unmap(asict, asich, WE_NPORTS);
	if (memh_valid)
		bus_space_unmap(memt, memh, memsize);
	return (rv);
}

void
we_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct we_softc *wsc = (struct we_softc *)self;
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t nict, asict, memt;
	bus_space_handle_t nich, asich, memh;
	const char *typestr;
	u_int8_t x;
	int i;

	printf("\n");

	nict = asict = ia->ia_iot;
	memt = ia->ia_memt;

	wsc->sc_delayt = ia->ia_iot;
	wsc->sc_delayh = ia->ia_delaybah;

	/* Map the device. */
	if (bus_space_map(asict, ia->ia_iobase, WE_NPORTS, 0, &asich)) {
		printf("%s: can't map nic i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	if (bus_space_subregion(asict, asich, WE_NIC_OFFSET, WE_NIC_NPORTS,
	    &nich)) {
		printf("%s: can't subregion i/o space\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	typestr = we_params(asict, asich, &wsc->sc_type, NULL,
	    &wsc->sc_16bitp, &sc->is790);
	if (typestr == NULL) {
		printf("%s: where did the card go?\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Map memory space.  Note we use the size that might have
	 * been overridden by the user.
	 */
	if (bus_space_map(memt, ia->ia_maddr, ia->ia_msize, 0, &memh)) {
		printf("%s: can't map shared memory\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Allow user to override 16-bit mode.  8-bit takes precedence.
	 */
	if (self->dv_cfdata->cf_flags & WE_FLAGS_FORCE_16BIT_MODE)
		wsc->sc_16bitp = 1;
	if (self->dv_cfdata->cf_flags & WE_FLAGS_FORCE_8BIT_MODE)
		wsc->sc_16bitp = 0;

	wsc->sc_asict = asict;
	wsc->sc_asich = asich;

	sc->sc_regt = nict;
	sc->sc_regh = nich;

	sc->sc_buft = memt;
	sc->sc_bufh = memh;

	/* Interface is always enabled. */
	sc->sc_enabled = 1;

	/* Registers are linear. */
	for (i = 0; i < 16; i++)
		sc->sc_reg_map[i] = i;

	/* Now we can use the NIC_{GET,PUT}() macros. */

	printf("%s: %s Ethernet (%s-bit)\n", sc->sc_dev.dv_xname,
	    typestr, wsc->sc_16bitp ? "16" : "8");

	/* Get station address from EEPROM. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_enaddr[i] = bus_space_read_1(asict, asich, WE_PROM + i);

	/*
	 * Set upper address bits and 8/16 bit access to shared memory.
	 */
	if (sc->is790) {
		wsc->sc_laar_proto =
		    bus_space_read_1(asict, asich, WE_LAAR) &
		    ~WE_LAAR_M16EN;
		bus_space_write_1(asict, asich, WE_LAAR,
		    wsc->sc_laar_proto | (wsc->sc_16bitp ? WE_LAAR_M16EN : 0));
	} else if ((wsc->sc_type & WE_SOFTCONFIG) ||
#ifdef TOSH_ETHER
	    (wsc->sc_type == WE_TYPE_TOSHIBA1) ||
	    (wsc->sc_type == WE_TYPE_TOSHIBA4) ||
#endif
	    (wsc->sc_type == WE_TYPE_WD8013EBT)) {
		wsc->sc_laar_proto = (ia->ia_maddr >> 19) & WE_LAAR_ADDRHI;
		if (wsc->sc_16bitp)
			wsc->sc_laar_proto |= WE_LAAR_L16EN;
		bus_space_write_1(asict, asich, WE_LAAR,
		    wsc->sc_laar_proto | (wsc->sc_16bitp ? WE_LAAR_M16EN : 0));
	}

	/*
	 * Set address and enable interface shared memory.
	 */
	if (sc->is790) {
		/* XXX MAGIC CONSTANTS XXX */
		x = bus_space_read_1(asict, asich, 0x04);
		bus_space_write_1(asict, asich, 0x04, x | 0x80);
		bus_space_write_1(asict, asich, 0x0b,
		    ((ia->ia_maddr >> 13) & 0x0f) |
		    ((ia->ia_maddr >> 11) & 0x40) |
		    (bus_space_read_1(asict, asich, 0x0b) & 0xb0));
		bus_space_write_1(asict, asich, 0x04, x);
		wsc->sc_msr_proto = 0x00;
		sc->cr_proto = 0x00;
	} else {
#ifdef TOSH_ETHER
		if (wsc->sc_type == WE_TYPE_TOSHIBA1 ||
		    wsc->sc_type == WE_TYPE_TOSHIBA4) {
			bus_space_write_1(asict, asich, WE_MSR + 1,
			    ((ia->ia_maddr >> 8) & 0xe0) | 0x04);
			bus_space_write_1(asict, asich, WE_MSR + 2,
			    ((ia->ia_maddr >> 16) & 0x0f));
			wsc->sc_msr_proto = WE_MSR_POW;
		} else
#endif
			wsc->sc_msr_proto = (ia->ia_maddr >> 13) &
			    WE_MSR_ADDR;

		sc->cr_proto = ED_CR_RD2;
	}

	bus_space_write_1(asict, asich, WE_MSR,
	    wsc->sc_msr_proto | WE_MSR_MENB);
	WE_DELAY(wsc);

	/*
	 * DCR gets:
	 *
	 *	FIFO threshold to 8, No auto-init Remote DMA,
	 *	byte order=80x86.
	 *
	 * 16-bit cards also get word-wide DMA transfers.
	 */
	sc->dcr_reg = ED_DCR_FT1 | ED_DCR_LS |
	    (wsc->sc_16bitp ? ED_DCR_WTS : 0);

	sc->test_mem = we_test_mem;
	sc->ring_copy = we_ring_copy;
	sc->write_mbuf = we_write_mbuf;
	sc->read_hdr = we_read_hdr;
	sc->recv_int = we_recv_int;

	sc->sc_mediachange = we_mediachange;
	sc->sc_mediastatus = we_mediastatus;

	sc->mem_start = 0;
	sc->mem_size = ia->ia_msize;

	sc->sc_flags = self->dv_cfdata->cf_flags;

	/* Do generic parts of attach. */
	if (wsc->sc_type & WE_SOFTCONFIG) {
		int defmedia;

		x = bus_space_read_1(asict, asich, WE_IRR);
		defmedia = IFM_ETHER;
		if (x & WE_IRR_OUT2)
			defmedia |= IFM_10_2;
		else
			defmedia |= IFM_10_5;
		i = dp8390_config(sc, we_media, NWE_MEDIA, defmedia);
	} else
		i = dp8390_config(sc, NULL, 0, 0);
	if (i) {
		printf("%s: configuration failed\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Disable 16-bit access to shared memory - we leave it disabled
	 * so that:
	 *
	 *	(1) machines reboot properly when the board is set to
	 *	    16-bit mode and there are conflicting 8-bit devices
	 *	    within the same 128k address space as this board's
	 *	    shared memory, and
	 *
	 *	(2) so that other 8-bit devices with shared memory
	 *	    in this same 128k address space will work.
	 */
	WE_MEM_DISABLE(wsc);

	/*
	 * Enable the configured interrupt.
	 */
	if (sc->is790)
		bus_space_write_1(asict, asich, WE790_ICR,
		    bus_space_read_1(asict, asich, WE790_ICR) |
		    WE790_ICR_EIL);
	else if (wsc->sc_type & WE_SOFTCONFIG)
		bus_space_write_1(asict, asich, WE_IRR,
		    bus_space_read_1(asict, asich, WE_IRR) | WE_IRR_IEN);
	else if (ia->ia_irq == IRQUNK) {
		printf("%s: can't wildcard IRQ on a %s\n",
		    sc->sc_dev.dv_xname, typestr);
		return;
	}

	/* Establish interrupt handler. */
	wsc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, dp8390_intr, sc);
	if (wsc->sc_ih == NULL)
		printf("%s: can't establish interrupt\n", sc->sc_dev.dv_xname);
}

int
we_test_mem(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	bus_space_tag_t memt = sc->sc_buft;
	bus_space_handle_t memh = sc->sc_bufh;
	bus_size_t memsize = sc->mem_size;
	int i;

	if (wsc->sc_16bitp)
		bus_space_set_region_2(memt, memh, 0, 0, memsize >> 1);
	else
		bus_space_set_region_1(memt, memh, 0, 0, memsize);

	if (wsc->sc_16bitp) {
		for (i = 0; i < memsize; i += 2) {
			if (bus_space_read_2(memt, memh, i) != 0)
				goto fail;
		}
	} else {
		for (i = 0; i < memsize; i++) {
			if (bus_space_read_1(memt, memh, i) != 0)
				goto fail;
		}
	}

	return (0);

 fail:
	printf("%s: failed to clear shared memory at offset 0x%x\n",
	    sc->sc_dev.dv_xname, i);
	WE_MEM_DISABLE(wsc);
	return (1);
}

__inline void
we_writemem(wsc, from, to, len)
	struct we_softc *wsc;
	u_int8_t *from;
	int to, len;
{
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;

	if (wsc->sc_16bitp) {
		bus_space_write_region_2(memt, memh, to, (u_int16_t *)from,
		    len >> 1);
		if (len & 1)
			bus_space_write_2(memt, memh, to + (len & ~1),
			    (u_int16_t)(*(from + (len & ~1))));
	} else
		bus_space_write_region_1(memt, memh, to, from, len);
}

__inline void
we_readmem(wsc, from, to, len)
	struct we_softc *wsc;
	int from;
	u_int8_t *to;
	int len;
{
	bus_space_tag_t memt = wsc->sc_dp8390.sc_buft;
	bus_space_handle_t memh = wsc->sc_dp8390.sc_bufh;

	if (wsc->sc_16bitp) {
		bus_space_read_region_2(memt, memh, from, (u_int16_t *)to,
		    len >> 1);
		if (len & 1)
			*(to + (len & ~1)) = bus_space_read_2(memt,
			    memh, from + (len & ~1)) & 0xff;
	} else
		bus_space_read_region_1(memt, memh, from, to, len);
}

int
we_write_mbuf(sc, m, buf)
	struct dp8390_softc *sc;
	struct mbuf *m;
	int buf;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	int savelen;

	savelen = m->m_pkthdr.len;

	WE_MEM_ENABLE(wsc);

	for (; m != NULL; buf += m->m_len, m = m->m_next)
		we_writemem(wsc, mtod(m, u_int8_t *), buf, m->m_len);

	WE_MEM_DISABLE(wsc);

	return (savelen);
}

int
we_ring_copy(sc, src, dst, amount)
	struct dp8390_softc *sc;
	int src;
	caddr_t dst;
	u_short amount;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	u_short tmp_amount;

	/* Does copy wrap to lower addr in ring buffer? */
	if (src + amount > sc->mem_end) {
		tmp_amount = sc->mem_end - src;

		/* Copy amount up to end of NIC memory. */
		we_readmem(wsc, src, dst, tmp_amount);

		amount -= tmp_amount;
		src = sc->mem_ring;
		dst += tmp_amount;
	}

	we_readmem(wsc, src, dst, amount);

	return (src + amount);
}

void
we_read_hdr(sc, packet_ptr, packet_hdrp)
	struct dp8390_softc *sc;
	int packet_ptr;
	struct dp8390_ring *packet_hdrp;
{
	struct we_softc *wsc = (struct we_softc *)sc;

	we_readmem(wsc, packet_ptr, (u_int8_t *)packet_hdrp,
	    sizeof(struct dp8390_ring));
}

void
we_recv_int(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;

	WE_MEM_ENABLE(wsc);
	dp8390_rint(sc);
	WE_MEM_DISABLE(wsc);
}

int
we_mediachange(sc)
	struct dp8390_softc *sc;
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.  The new media will be
	 * set up in we_init_card() called via dp8390_init().
	 */
	dp8390_reset(sc);
	return (0);
}

void
we_mediastatus(sc, ifmr)
	struct dp8390_softc *sc;
	struct ifmediareq *ifmr;
{
	struct ifmedia *ifm = &sc->sc_media;

	/*
	 * The currently selected media is always the active media.
	 */
	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

void
we_init_card(sc)
	struct dp8390_softc *sc;
{
	struct we_softc *wsc = (struct we_softc *)sc;
	struct ifmedia *ifm = &sc->sc_media;

	we_set_media(wsc, ifm->ifm_cur->ifm_media);
}

void
we_set_media(wsc, media)
	struct we_softc *wsc;
	int media;
{
	struct dp8390_softc *sc = &wsc->sc_dp8390;
	bus_space_tag_t asict = wsc->sc_asict;
	bus_space_handle_t asich = wsc->sc_asich;
	u_int8_t hwr, gcr, irr;

	if (sc->is790) {
		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		gcr = bus_space_read_1(asict, asich, WE790_GCR);
		if (IFM_SUBTYPE(media) == IFM_10_2)
			gcr |= WE790_GCR_GPOUT;
		else
			gcr &= ~WE790_GCR_GPOUT;
		bus_space_write_1(asict, asich, WE790_GCR,
		    gcr | WE790_GCR_LIT);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr &= ~WE790_HWR_SWH);
		return;
	}

	irr = bus_space_read_1(wsc->sc_asict, wsc->sc_asich, WE_IRR);
	if (IFM_SUBTYPE(media) == IFM_10_2)
		irr |= WE_IRR_OUT2;
	else
		irr &= ~WE_IRR_OUT2;
	bus_space_write_1(wsc->sc_asict, wsc->sc_asich, WE_IRR, irr);
}

const char *
we_params(asict, asich, typep, memsizep, is16bitp, is790p)
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	u_int8_t *typep;
	bus_size_t *memsizep;
	int *is16bitp, *is790p;
{
	const char *typestr;
	bus_size_t memsize;
	int is16bit, is790;
	u_int8_t type;

	memsize = 8192;
	is16bit = is790 = 0;

	type = bus_space_read_1(asict, asich, WE_CARD_ID);
	switch (type) {
	case WE_TYPE_WD8003S: 
		typestr = "WD8003S"; 
		break;
	case WE_TYPE_WD8003E:
		typestr = "WD8003E";
		break;
	case WE_TYPE_WD8003EB: 
		typestr = "WD8003EB";
		break;
	case WE_TYPE_WD8003W:
		typestr = "WD8003W";
		break;
	case WE_TYPE_WD8013EBT: 
		typestr = "WD8013EBT";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013W:
		typestr = "WD8013W";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EP:		/* also WD8003EP */
		if (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) {
			is16bit = 1;
			memsize = 16384;
			typestr = "WD8013EP";
		} else
			typestr = "WD8003EP";
		break;
	case WE_TYPE_WD8013WC:
		typestr = "WD8013WC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EBP:
		typestr = "WD8013EBP";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_WD8013EPC:
		typestr = "WD8013EPC";
		memsize = 16384;
		is16bit = 1;
		break;
	case WE_TYPE_SMC8216C:
	case WE_TYPE_SMC8216T:
	    {
		u_int8_t hwr;

		typestr = (type == WE_TYPE_SMC8216C) ?
		    "SMC8216/SMC8216C" : "SMC8216T";

		hwr = bus_space_read_1(asict, asich, WE790_HWR);
		bus_space_write_1(asict, asich, WE790_HWR,
		    hwr | WE790_HWR_SWH);
		switch (bus_space_read_1(asict, asich, WE790_RAR) &
		    WE790_RAR_SZ64) {
		case WE790_RAR_SZ64:
			memsize = 65536;
			break;
		case WE790_RAR_SZ32:
			memsize = 32768;
			break;
		case WE790_RAR_SZ16:
			memsize = 16384;
			break;
		case WE790_RAR_SZ8:
			/* 8216 has 16K shared mem -- 8416 has 8K */
			typestr = (type == WE_TYPE_SMC8216C) ?
			    "SMC8416C/SMC8416BT" : "SMC8416T";
			memsize = 8192;
			break;
		}
		bus_space_write_1(asict, asich, WE790_HWR, hwr);

		is16bit = 1;
		is790 = 1;
		break;
	    }
#ifdef TOSH_ETHER
	case WE_TYPE_TOSHIBA1:
		typestr = "Toshiba1";
		memsize = 32768;
		is16bit = 1;
		break;
	case WE_TYPE_TOSHIBA4:
		typestr = "Toshiba4";
		memsize = 32768;
		is16bit = 1;
		break;
#endif
	default:
		/* Not one we recognize. */
		return (NULL);
	}

	/*
	 * Make some adjustments to initial values depending on what is
	 * found in the ICR.
	 */
	if (is16bit && (type != WE_TYPE_WD8013EBT) &&
#ifdef TOSH_ETHER
	    (type != WE_TYPE_TOSHIBA1 && type != WE_TYPE_TOSHIBA4) &&
#endif
	    (bus_space_read_1(asict, asich, WE_ICR) & WE_ICR_16BIT) == 0) {
		is16bit = 0;
		memsize = 8192;
	}

#ifdef WE_DEBUG
	{
		int i;

		printf("we_params: type = 0x%x, typestr = %s, is16bit = %d, "
		    "memsize = %d\n", type, typestr, is16bit, memsize);
		for (i = 0; i < 8; i++)
			printf("     %d -> 0x%x\n", i,
			    bus_space_read_1(asict, asich, i));
	}
#endif

	if (typep != NULL)
		*typep = type;
	if (memsizep != NULL)
		*memsizep = memsize;
	if (is16bitp != NULL)
		*is16bitp = is16bit;
	if (is790p != NULL)
		*is790p = is790;
	return (typestr);
}
