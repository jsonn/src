/*	$NetBSD: oak.c,v 1.5.2.1 2002/01/10 19:57:30 thorpej Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mark Brinicombe of Causality Limited.
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
 * Oak Solutions SCSI 1 driver using the generic NCR5380 driver.
 *
 * From <URL:http://foldoc.doc.ic.ac.uk/acorn/doc/scsi>:
 * --------8<--------
 * From: Hugo Fiennes
 * [...]
 * The oak scsi plays some other tricks to get max around 2.2Mb/sec:
 * it is a 16- bit interface (using their own hardware and an 8-bit
 * scsi controller to 'double-up' the data). What it does is: every
 * 128 bytes it uses a polling loop (see above) to check data is
 * present and the drive has reported no errors, etc.  Inside each 128
 * byte block it just reads data as fast as it can: on a normal card
 * this would result in disaster if the drive wasn't fast enough to
 * feed the machine: on the oak card however, the hardware will not
 * assert IOGT (IO grant), so hanging the machine in a wait state
 * until data is ready. This can have problems: if the drive is to
 * slow (unlikely) the machine will completely stiff as the ARM3 can't
 * be kept in such a state for more than 10(?) us.
 * -------->8--------
 *
 * So far, my attempts at doing this have failed, though.
 *
 * This card has to be polled: it doesn't have anything connected to
 * PIRQ*.  This seems to be a common failing of Archimedes disc
 * controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: oak.c,v 1.5.2.1 2002/01/10 19:57:30 thorpej Exp $");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include <machine/bootconfig.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/powerromreg.h>

#include <dev/podulebus/oakreg.h>

void oak_attach (struct device *, struct device *, void *);
int  oak_match  (struct device *, struct cfdata *, void *);

#if 0
static int oak_pdma_in(struct ncr5380_softc *, int, int, u_char *);
static int oak_pdma_out(struct ncr5380_softc *, int, int, u_char *);
#endif

/*
 * Oak SCSI 1 softc structure.
 *
 * Contains the generic ncr5380 device node, podule information and
 * global information required by the driver.
 */

struct oak_softc {
	struct ncr5380_softc	sc_ncr5380;
	bus_space_tag_t		sc_pdmat;
	bus_space_handle_t	sc_pdmah;
};

struct cfattach oak_ca = {
	sizeof(struct oak_softc), oak_match, oak_attach
};

/*
 * Card probe function
 *
 * Just match the manufacturer and podule ID's
 */

int
oak_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	if (matchpodule(pa, MANUFACTURER_OAK, PODULE_OAK_SCSI, -1))
		return 1;

	/* PowerROM */
	if (pa->pa_product == PODULE_ALSYSTEMS_SCSI &&
	    podulebus_initloader(pa) == 0 &&
	    podloader_callloader(pa, 0, 0) == PRID_OAK_SCSI1)
		return 1;

	return 0;
}

/*
 * Card attach function
 *
 */

void
oak_attach(struct device *parent, struct device *self, void *aux)
{
	struct oak_softc *sc = (struct oak_softc *)self;
	struct podulebus_attach_args *pa = aux;
	u_char *iobase;
	char hi_option[sizeof(sc->sc_ncr5380.sc_dev.dv_xname) + 8];

	sc->sc_ncr5380.sc_flags |= NCR5380_FORCE_POLLING;
	sc->sc_ncr5380.sc_min_dma_len = 0;
	sc->sc_ncr5380.sc_no_disconnect = 0xff;
	sc->sc_ncr5380.sc_parity_disable = 0;

	sc->sc_ncr5380.sc_dma_alloc = NULL;
	sc->sc_ncr5380.sc_dma_free = NULL;
	sc->sc_ncr5380.sc_dma_poll = NULL;
	sc->sc_ncr5380.sc_dma_setup = NULL;
	sc->sc_ncr5380.sc_dma_start = NULL;
	sc->sc_ncr5380.sc_dma_eop = NULL;
	sc->sc_ncr5380.sc_dma_stop = NULL;
	sc->sc_ncr5380.sc_intr_on = NULL;
	sc->sc_ncr5380.sc_intr_off = NULL;

#ifdef NCR5380_USE_BUS_SPACE
	sc->sc_ncr5380.sc_regt = pa->pa_mod_t;
	bus_space_map(sc->sc_ncr5380.sc_regt, pa->pa_mod_base, 8, 0,
	    &sc->sc_ncr5380.sc_regh);
	sc->sc_ncr5380.sci_r0 = 0;
	sc->sc_ncr5380.sci_r1 = 1;
	sc->sc_ncr5380.sci_r2 = 2;
	sc->sc_ncr5380.sci_r3 = 3;
	sc->sc_ncr5380.sci_r4 = 4;
	sc->sc_ncr5380.sci_r5 = 5;
	sc->sc_ncr5380.sci_r6 = 6;
	sc->sc_ncr5380.sci_r7 = 7;
#else
	iobase = (u_char *)pa->pa_mod_base;
	sc->sc_ncr5380.sci_r0 = iobase + 0;
	sc->sc_ncr5380.sci_r1 = iobase + 4;
	sc->sc_ncr5380.sci_r2 = iobase + 8;
	sc->sc_ncr5380.sci_r3 = iobase + 12;
	sc->sc_ncr5380.sci_r4 = iobase + 16;
	sc->sc_ncr5380.sci_r5 = iobase + 20;
	sc->sc_ncr5380.sci_r6 = iobase + 24;
	sc->sc_ncr5380.sci_r7 = iobase + 28;
#endif
	sc->sc_pdmat = pa->pa_mod_t;
	bus_space_map(sc->sc_pdmat, pa->pa_mod_base + OAK_PDMA_OFFSET, 0x20, 0,
	    &sc->sc_pdmah);

	sc->sc_ncr5380.sc_rev = NCR_VARIANT_NCR5380;

	sc->sc_ncr5380.sc_pio_in = ncr5380_pio_in;
	sc->sc_ncr5380.sc_pio_out = ncr5380_pio_out;

	/* Provide an override for the host id */
	sc->sc_ncr5380.sc_channel.chan_id = 7;
	sprintf(hi_option, "%s.hostid", sc->sc_ncr5380.sc_dev.dv_xname);
	(void)get_bootconf_option(boot_args, hi_option,
	    BOOTOPT_TYPE_INT, &sc->sc_ncr5380.sc_channel.chan_id);
	sc->sc_ncr5380.sc_adapter.adapt_minphys = minphys;

	printf(": host=%d, using 8 bit PIO\n",
	    sc->sc_ncr5380.sc_channel.chan_id);

	ncr5380_attach(&sc->sc_ncr5380);
}

/*
 * XXX The code below doesn't work correctly.  I probably need more
 * details on how the card works.  [bjh21 20011202]
 */
#if 0

#ifndef OAK_TSIZE_OUT
#define OAK_TSIZE_OUT	128
#endif

#ifndef OAK_TSIZE_IN
#define OAK_TSIZE_IN	128
#endif

#define TIMEOUT 1000000

static __inline int
oak_ready(struct ncr5380_softc *sc)
{
	int i;
	int status;

	for (i = TIMEOUT; i > 0; i--) {
		status = NCR5380_READ(sc, sci_csr);
		    if ((status & (SCI_CSR_DREQ | SCI_CSR_PHASE_MATCH)) ==
			(SCI_CSR_DREQ | SCI_CSR_PHASE_MATCH))
		    	return(1);

		if ((status & SCI_CSR_PHASE_MATCH) == 0 ||
		    SCI_BUSY(sc) == 0)
			return(0);
	}
	printf("%s: ready timeout\n", sc->sc_dev.dv_xname);
	return(0);

#if 0 /* The Linux driver does this: */
	struct oak_softc *sc = (void *)ncr_sc;
	bus_space_tag_t pdmat = sc->sc_pdmat;
	bus_space_handle_t pdmah = sc->sc_pdmah;
	int i, status;

	for (i = TIMEOUT; i > 0; i--) {
		status = bus_space_read_2(pdmat, pdmah, OAK_PDMA_STATUS);
		if (status & 0x200)
			return(0);
		if (status & 0x100)
			return(1);
	}
	printf("%s: ready timeout, status = 0x%x\n", ncr_sc->sc_dev.dv_xname,
	    status);
	return(0);
#endif
}



/* Return zero on success. */
static __inline void oak_wait_not_req(struct ncr5380_softc *sc)
{
	int timo;
	for (timo = TIMEOUT; timo; timo--) {
		if ((NCR5380_READ(sc, sci_bus_csr) & SCI_BUS_REQ) == 0 ||
		    (NCR5380_READ(sc, sci_csr) & SCI_CSR_PHASE_MATCH) == 0 ||
		    SCI_BUSY(sc) == 0) {
			return;
		}
	}
	printf("%s: pdma not_req timeout\n", sc->sc_dev.dv_xname);
}

static int
oak_pdma_in(struct ncr5380_softc *ncr_sc, int phase, int datalen,
    u_char *data)
{
	struct oak_softc *sc = (void *)ncr_sc;
	bus_space_tag_t pdmat = sc->sc_pdmat;
	bus_space_handle_t pdmah = sc->sc_pdmah;
	int s, resid, len;

	s = splbio();

	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) | SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_irecv, 0);

	resid = datalen;
	while (resid > 0) {
		len = min(resid, OAK_TSIZE_IN);
		if (oak_ready(ncr_sc) == 0)
			goto interrupt;
		KASSERT(BUS_SPACE_ALIGNED_POINTER(data, u_int16_t));
		bus_space_read_multi_2(pdmat, pdmah, OAK_PDMA_READ,
		    (u_int16_t *)data, len/2);
		data += len;
		resid -= len;
	}

	oak_wait_not_req(ncr_sc);

interrupt:
	SCI_CLR_INTR(ncr_sc);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) & ~SCI_MODE_DMA);
	splx(s);
	return datalen - resid;
}

static int
oak_pdma_out(struct ncr5380_softc *ncr_sc, int phase, int datalen,
    u_char *data)
{
	struct oak_softc *sc = (void *)ncr_sc;
	bus_space_tag_t pdmat = sc->sc_pdmat;
	bus_space_handle_t pdmah = sc->sc_pdmah;
	int i, s, icmd, resid;

	s = splbio();
	icmd = NCR5380_READ(ncr_sc, sci_icmd) & SCI_ICMD_RMASK;
	NCR5380_WRITE(ncr_sc, sci_icmd, icmd | SCI_ICMD_DATA);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) | SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_dma_send, 0);

	resid = datalen;
	if (oak_ready(ncr_sc) == 0)
		goto interrupt;

	if (resid > OAK_TSIZE_OUT) {
		/*
		 * Because of the chips DMA prefetch, phase changes
		 * etc, won't be detected until we have written at
		 * least one byte more. We pre-write 4 bytes so
		 * subsequent transfers will be aligned to a 4 byte
		 * boundary. Assuming disconects will only occur on
		 * block boundaries, we then correct for the pre-write
		 * when and if we get a phase change. If the chip had
		 * DMA byte counting hardware, the assumption would not
		 * be necessary.
		 */
		KASSERT(BUS_SPACE_ALIGNED_POINTER(data, u_int16_t));
		bus_space_write_multi_2(pdmat, pdmah, OAK_PDMA_WRITE,
		    (u_int16_t *)data, 4/2);
		data += 4;
		resid -= 4;
		
		for (; resid >= OAK_TSIZE_OUT; resid -= OAK_TSIZE_OUT) {
			if (oak_ready(ncr_sc) == 0) {
				resid += 4; /* Overshot */
				goto interrupt;
			}
			bus_space_write_multi_2(pdmat, pdmah, OAK_PDMA_WRITE,
			    (u_int16_t *)data, OAK_TSIZE_OUT/2);
			data += OAK_TSIZE_OUT;
		}
		if (oak_ready(ncr_sc) == 0) {
			resid += 4; /* Overshot */
			goto interrupt;
		}
	}

	if (resid) {
		bus_space_write_multi_2(pdmat, pdmah, OAK_PDMA_WRITE,
		    (u_int16_t *)data, resid/2);
		resid = 0;
	}
	for (i = TIMEOUT; i > 0; i--) {
		if ((NCR5380_READ(ncr_sc, sci_csr)
		    & (SCI_CSR_DREQ|SCI_CSR_PHASE_MATCH))
		    != SCI_CSR_DREQ)
			break;
	}
	if (i != 0)
		bus_space_write_2(pdmat, pdmah, OAK_PDMA_WRITE, 0);
	else
		printf("%s: timeout waiting for final SCI_DSR_DREQ.\n",
			ncr_sc->sc_dev.dv_xname);

	oak_wait_not_req(ncr_sc);
interrupt:
	SCI_CLR_INTR(ncr_sc);
	NCR5380_WRITE(ncr_sc, sci_mode,
	    NCR5380_READ(ncr_sc, sci_mode) & ~SCI_MODE_DMA);
	NCR5380_WRITE(ncr_sc, sci_icmd, icmd);
	splx(s);
	return(datalen - resid);
}
#endif
