/*	$NetBSD: if_ie.c,v 1.3.10.2 2000/03/13 12:15:26 scw Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/bus.h>

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>

#include <mvme68k/dev/if_iereg.h>
#include <mvme68k/dev/pcctwovar.h>
#include <mvme68k/dev/pcctworeg.h>


int  ie_pcctwo_match  __P((struct device *, struct cfdata *, void *));
void ie_pcctwo_attach __P((struct device *, struct device *, void *));

struct cfattach ie_pcctwo_ca = {
	sizeof(struct ie_softc), ie_pcctwo_match, ie_pcctwo_attach
};

extern struct cfdriver ie_cd;


/* Functions required by the i82586 MI driver */
static void 	ie_reset __P((struct ie_softc *, int));
static int 	ie_intrhook __P((struct ie_softc *, int));
static void 	ie_hwinit __P((struct ie_softc *));
static void 	ie_atten __P((struct ie_softc *));

static void	ie_copyin __P((struct ie_softc *, void *, int, size_t));
static void	ie_copyout __P((struct ie_softc *, const void *, int, size_t));

static u_int16_t ie_read_16 __P((struct ie_softc *, int));
static void	ie_write_16 __P((struct ie_softc *, int, u_int16_t));
static void	ie_write_24 __P((struct ie_softc *, int, int));

/*
 * i82596 Support Routines for MVME1[67]7 Boards
 */
static void
ie_reset(sc, why)
	struct ie_softc *sc;
	int why;
{
	u_int32_t scp_addr;

	switch ( why ) {
	  case CHIP_PROBE:
	  case CARD_RESET:
		bus_space_write_2(sc->sc_bt, sc->sc_bh, IE_MPUREG_UPPER,
		    IE_MPU_RESET);
		bus_space_write_2(sc->sc_bt, sc->sc_bh, IE_MPUREG_LOWER, 0);
		delay(1000);

		/*
		 * Set the BUSY and BUS_USE bytes here, since the MI code
		 * incorrectly assumes it can use byte addressing to set it.
		 * (due to wrong-endianess of the chip)
		 */
		ie_write_16(sc, IE_ISCP_BUSY(sc->iscp), 1);
		ie_write_16(sc, IE_SCP_BUS_USE(sc->scp), 0x50);

		scp_addr = sc->scp + (u_int)sc->sc_iobase;
		scp_addr |= IE_MPU_SCP_ADDRESS;

		bus_space_write_2(sc->sc_bt, sc->sc_bh, IE_MPUREG_UPPER,
		    scp_addr & 0xffff);
		bus_space_write_2(sc->sc_bt, sc->sc_bh, IE_MPUREG_LOWER,
		    (scp_addr >> 16) & 0xffff;
		delay(1000);
		break;
	}
}

static int
ie_intrhook(sc, when)
	struct ie_softc	*sc;
	int when;
{
	u_int8_t reg;

	if ( when == INTR_EXIT ) {
		reg = pcc2_reg_read(sys_pcctwo, PCC2REG_ETH_ICSR);
		reg |= PCCTWO_ICR_ICLR;
		pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR, reg);
	}

	return 0;
}

static void
ie_hwinit(sc)
	struct ie_softc *sc;
{
	u_int8_t reg;

	reg = pcc2_reg_read(sys_pcctwo, PCC2REG_ETH_ICSR);
	reg |= PCCTWO_ICR_IEN | PCCTWO_ICR_ICLR;
	pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR, reg);
}

static void
ie_atten(sc)
	struct ie_softc *sc;
{
	bus_space_write_2(sc->sc_bt, sc->sc_bh, IE_MPUREG_CA, 0);
}

static void
ie_copyin(sc, dst, offset, size)
        struct ie_softc *sc;
        void *dst;
        int offset;
        size_t size;
{
	if ( size == 0 )	/* This *can* happen! */
		return;

	memcpy(dst, (void *)(((u_long)sc->sc_maddr) + offset), size);
}

static void
ie_copyout(sc, src, offset, size)
        struct ie_softc *sc;
        const void *src;
        int offset;
        size_t size;
{
	if ( size == 0 )	/* This *can* happen! */
		return;

	memcpy((void *)(((u_long)sc->sc_maddr) + offset), src, size);
}

static u_int16_t
ie_read_16(sc, offset)
        struct ie_softc *sc;
        int offset;
{
	return *((u_int16_t *)(((u_long)sc->sc_maddr) + offset));
}

static void
ie_write_16(sc, offset, value)
        struct ie_softc *sc;
        int offset;
        u_int16_t value;
{
	*((u_int16_t *)(((u_long)sc->sc_maddr) + offset)) = value;
}

static void
ie_write_24(sc, offset, addr)
        struct ie_softc *sc;
        int offset, addr;
{
	addr += (int)sc->sc_iobase;	/* XXXSCW: Is this right? */

	*((u_int16_t *)(((u_long)sc->sc_maddr) + offset)) = addr & 0xffff;

	addr >>= 16;

	*((u_int16_t *)(((u_long)sc->sc_maddr) + offset)) = addr & 0x00ff;
}

int
ie_pcctwo_match(parent, cf, args)
	struct device *parent;
	struct cfdata *cf;
	void *args;
{
	struct pcc_attach_args *pa = args;

	if ( strcmp(pa->pa_name, ie_cd.cd_name) )
		return 0;

	pa->pa_ipl = cf->pcctwocf_ipl;

	return 1;
}

void
ie_pcctwo_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void   *args;
{
	struct pcctwo_attach_args *pa;
	struct ie_softc *sc;
	u_int8_t ethaddr[ETHER_ADDR_LEN];
	bus_dma_segment_t seg;
	int rseg;

	pa = (struct pcctwo_attach_args *) args;
	sc = (struct ie_softc *) self;

	myetheraddr(ethaddr);

	/* Map the MPU controller registers in PCCTWO space */
	sc->bt = pa->pa_bust;
	bus_space_map(pa->pa_bust, pa->pa_offset, IE_MPUREG_SIZE,0, &sc->sc_bh);

	/* Get contiguous DMA-able memory for the IE chip */
	if ( bus_dmamem_alloc(pa->pa_dmat, ether_data_buff_size, NBPG, 0,
	    &seg, 1, &rseg,
	    BUS_DMA_NOWAIT | BUS_DMA_ONBOARD_RAM | BUS_DMA_24BIT) != 0 ) {
		printf("%s: Failed to allocate ether buffer\n", self->dv_xname);
		return;
	}

	if ( bus_dmamem_map(pa->pa_dmat, &seg, rseg,
	    ether_data_buff_size, (caddr_t *) &sc->sc_maddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0 ) {
		printf("%s: Failed to map ether buffer\n", self->dv_xname);
		bus_dmamem_free(pa->pa_dmat, &seg, rseg);
		return;
	}

	sc->sc_iobase = seg.ds_addr;
	sc->sc_msize = ether_data_buff_size;
	memset(sc->sc_maddr, 0, ether_data_buff_size);

	sc->hwreset = ie_reset;
	sc->hwinit = ie_hwinit;
	sc->chan_attn = ie_atten;
	sc->intrhook = ie_intrhook;
	sc->memcopyin = ie_copyin;
	sc->memcopyout = ie_copyout;
	sc->ie_bus_read16 = ie_read_16;
	sc->ie_bus_write16 = ie_write_16;
	sc->ie_bus_write24 = ie_write_24;
	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = NULL;

	sc->scp = 0;
	sc->iscp = sc->scp + ((IE_SCP_SZ + 15) & ~15);
	sc->scb = sc->iscp + IE_ISCP_SZ;
	sc->buf_area = sc->scb + IE_SCB_SZ;
	sc->buf_area_sz = sc->sc_msize - (sc->buf_area - sc->scp);

	/*
	 * BUS_USE -> Interrupt Active High (edge-triggered),
	 *            Lock function enabled,
	 *            Internal bus throttle timer triggering,
	 *            82586 operating mode.
	 */
	ie_write_16(sc, IE_SCP_BUS_USE(sc->scp), 0x50);
	ie_write_24(sc, IE_SCP_ISCP(sc->scp), sc->iscp);
	ie_write_16(sc, IE_ISCP_SCB(sc->iscp), sc->scb);
	ie_write_24(sc, IE_ISCP_BASE(sc->iscp), sc->scp);

	/* This has the side-effect of resetting the chip */
	i82586_proberam(sc);

	/* Attach the MI back-end */
	i82586_attach(sc, "onboard", ethaddr, NULL, 0, 0);

	/* Are we the boot device? */
	if ( PCCTWO_PADDR(pa->pa_offset) == bootaddr )
		booted_device = self;

	/* Finally, hook the hardware interrupt */
	pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR, 0);
	pcctwointr_establish(PCCTWOV_LANC_IRQ, i82586_intr, pa->pa_ipl, sc);
	pcc2_reg_write(sys_pcctwo, PCC2REG_ETH_ICSR,
	    pa->pa_ipl | PCCTWO_ICR_ICLR | PCCTWO_ICR_EDGE;
}
