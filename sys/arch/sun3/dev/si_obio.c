/*	$NetBSD: si_obio.c,v 1.16.2.1 1997/10/22 01:06:59 mellon Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, David Jones, and Gordon W. Ross.
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
 * This file contains only the machine-dependent parts of the
 * Sun3 SCSI driver.  (Autoconfig stuff and DMA functions.)
 * The machine-independent parts are in ncr5380sbc.c
 *
 * Supported hardware includes:
 * Sun SCSI-3 on OBIO (Sun3/50,Sun3/60)
 * Sun SCSI-3 on VME (Sun3/160,Sun3/260)
 *
 * Could be made to support the Sun3/E if someone wanted to.
 *
 * Note:  Both supported variants of the Sun SCSI-3 adapter have
 * some really unusual "features" for this driver to deal with,
 * generally related to the DMA engine.  The OBIO variant will
 * ignore any attempt to write the FIFO count register while the
 * SCSI bus is in DATA_IN or DATA_OUT phase.  This is dealt with
 * by setting the FIFO count early in COMMAND or MSG_IN phase.
 *
 * The VME variant has a bit to enable or disable the DMA engine,
 * but that bit also gates the interrupt line from the NCR5380!
 * Therefore, in order to get any interrupt from the 5380, (i.e.
 * for reselect) one must clear the DMA engine transfer count and
 * then enable DMA.  This has the further complication that you
 * CAN NOT touch the NCR5380 while the DMA enable bit is set, so
 * we have to turn DMA back off before we even look at the 5380.
 *
 * What wonderfully whacky hardware this is!
 *
 * Credits, history:
 *
 * David Jones wrote the initial version of this module, which
 * included support for the VME adapter only. (no reselection).
 *
 * Gordon Ross added support for the OBIO adapter, and re-worked
 * both the VME and OBIO code to support disconnect/reselect.
 * (Required figuring out the hardware "features" noted above.)
 *
 * The autoconfiguration boilerplate came from Adam Glass.
 */

/*****************************************************************
 * OBIO functions for DMA
 ****************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_debug.h>
#include <dev/scsipi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/obio.h>
#include <machine/dvma.h>

#define DEBUG XXX

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include "sireg.h"
#include "sivar.h"
#include "am9516.h"

/*
 * How many uS. to delay after touching the am9516 UDC.
 */
#define UDC_WAIT_USEC 5

void si_obio_dma_setup __P((struct ncr5380_softc *));
void si_obio_dma_start __P((struct ncr5380_softc *));
void si_obio_dma_eop __P((struct ncr5380_softc *));
void si_obio_dma_stop __P((struct ncr5380_softc *));

static void si_obio_reset __P((struct ncr5380_softc *));

static __inline__ void si_obio_udc_write
 __P((volatile struct si_regs *si, int regnum, int value));
static __inline__ int si_obio_udc_read
 __P((volatile struct si_regs *si, int regnum));


/*
 * New-style autoconfig attachment
 */

static int	si_obio_match __P((struct device *, struct cfdata *, void *));
static void	si_obio_attach __P((struct device *, struct device *, void *));

struct cfattach si_obio_ca = {
	sizeof(struct si_softc), si_obio_match, si_obio_attach
};

/*
 * Options for disconnect/reselect, DMA, and interrupts.
 * By default, allow disconnect/reselect on targets 4-6.
 * Those are normally tapes that really need it enabled.
 */
int si_obio_options = 0x0f;


static int
si_obio_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	/* We use obio_mapin(), so require OBIO. */
	if (ca->ca_bustype != BUS_OBIO)
		return (0);

	/* Make sure something is there... */
	if (bus_peek(ca->ca_bustype, ca->ca_paddr + 1, 1) == -1)
		return (0);

	/* Default interrupt priority. */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 2;

	return (1);
}

static void
si_obio_attach(parent, self, args)
	struct device	*parent, *self;
	void		*args;
{
	struct si_softc *sc = (struct si_softc *) self;
	struct ncr5380_softc *ncr_sc = &sc->ncr_sc;
	struct cfdata *cf = self->dv_cfdata;
	struct confargs *ca = args;

	/* Get options from config flags if specified. */
	if (cf->cf_flags)
		sc->sc_options = cf->cf_flags;
	else
		sc->sc_options = si_obio_options;

	printf(": options=0x%x\n", sc->sc_options);

	sc->sc_adapter_type = ca->ca_bustype;
	sc->sc_regs = (struct si_regs *)
		obio_mapin(ca->ca_paddr, sizeof(struct si_regs));

	/*
	 * MD function pointers used by the MI code.
	 */
	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in =  ncr5380_pio_in;
	ncr_sc->sc_dma_alloc = si_dma_alloc;
	ncr_sc->sc_dma_free  = si_dma_free;
	ncr_sc->sc_dma_setup = si_obio_dma_setup;
	ncr_sc->sc_dma_start = si_obio_dma_start;
	ncr_sc->sc_dma_poll  = si_dma_poll;
	ncr_sc->sc_dma_eop   = si_obio_dma_eop;
	ncr_sc->sc_dma_stop  = si_obio_dma_stop;
	ncr_sc->sc_intr_on   = NULL;
	ncr_sc->sc_intr_off  = NULL;

	/* Need DVMA-capable memory for the UDC command block. */
	sc->sc_dmacmd = dvma_malloc(sizeof (struct udc_table));

	/* Attach interrupt handler. */
	isr_add_autovect(si_intr, (void *)sc, ca->ca_intpri);

	/* Reset the hardware. */
	si_obio_reset(ncr_sc);

	/* Do the common attach stuff. */
	si_attach(sc);
}

static void
si_obio_reset(struct ncr5380_softc *ncr_sc)
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

#ifdef	DEBUG
	if (si_debug) {
		printf("si_obio_reset\n");
	}
#endif

	/*
	 * The SCSI3 controller has an 8K FIFO to buffer data between the
	 * 5380 and the DMA.  Make sure it starts out empty.
	 *
	 * The reset bits in the CSR are active low.
	 */
	si->si_csr = 0;
	delay(10);
	si->si_csr = SI_CSR_FIFO_RES | SI_CSR_SCSI_RES | SI_CSR_INTR_EN;
	delay(10);
	si->fifo_count = 0;
}

static __inline__ void
si_obio_udc_write(si, regnum, value)
	volatile struct si_regs *si;
	int regnum, value;
{
	si->udc_addr = regnum;
	delay(UDC_WAIT_USEC);
	si->udc_data = value;
	delay(UDC_WAIT_USEC);
}

static __inline__ int
si_obio_udc_read(si, regnum)
	volatile struct si_regs *si;
	int regnum;
{
	int value;

	si->udc_addr = regnum;
	delay(UDC_WAIT_USEC);
	value = si->udc_data;
	delay(UDC_WAIT_USEC);

	return (value);
}


/*
 * This function is called during the COMMAND or MSG_IN phase
 * that preceeds a DATA_IN or DATA_OUT phase, in case we need
 * to setup the DMA engine before the bus enters a DATA phase.
 *
 * The OBIO "si" IGNORES any attempt to set the FIFO count
 * register after the SCSI bus goes into any DATA phase, so
 * this function has to setup the evil FIFO logic.
 */
void
si_obio_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	struct udc_table *cmd;
	long data_pa, cmd_pa;
	int xlen;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = dvma_kvtopa(dh->dh_dvma, sc->sc_adapter_type);
	data_pa += (ncr_sc->sc_dataptr - dh->dh_addr);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%lx", data_pa);
	xlen = ncr_sc->sc_datalen;
	sc->sc_reqlen = xlen; 	/* XXX: or less? */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_setup: dh=%p, pa=0x%lx, xlen=0x%x\n",
			   dh, data_pa, xlen);
	}
#endif

	/* Reset the UDC. (In case not already reset?) */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_RESET);

	/* Reset the FIFO */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	/* Set direction (send/recv) */
	if (dh->dh_flags & SIDH_OUT) {
		si->si_csr |= SI_CSR_SEND;
	} else {
		si->si_csr &= ~SI_CSR_SEND;
	}

	/* Set the FIFO counter. */
	si->fifo_count = xlen;

	/* Reset the UDC. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_RESET);

	/*
	 * XXX: Reset the FIFO again!  Comment from Sprite:
	 * Go through reset again becuase of the bug on the 3/50
	 * where bytes occasionally linger in the DMA fifo.
	 */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

#ifdef	DEBUG
	/* Make sure the extra FIFO reset did not hit the count. */
	if (si->fifo_count != xlen) {
		printf("si_dma_setup: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, xlen);
		Debugger();
	}
#endif

	/*
	 * Set up the DMA controller.  The DMA controller on
	 * OBIO needs a command block in DVMA space.
	 */
	cmd = sc->sc_dmacmd;
	cmd->addrh = ((data_pa & 0xFF0000) >> 8) | UDC_ADDR_INFO;
	cmd->addrl = data_pa & 0xFFFF;
	cmd->count = xlen / 2;	/* bytes -> words */
	cmd->cmrh = UDC_CMR_HIGH;
	if (dh->dh_flags & SIDH_OUT) {
		if (xlen & 1)
			cmd->count++;
		cmd->cmrl = UDC_CMR_LSEND;
		cmd->rsel = UDC_RSEL_SEND;
	} else {
		cmd->cmrl = UDC_CMR_LRECV;
		cmd->rsel = UDC_RSEL_RECV;
	}

	/* Tell the DMA chip where the control block is. */
	cmd_pa = dvma_kvtopa(cmd, BUS_OBIO);
	si_obio_udc_write(si, UDC_ADR_CAR_HIGH,
					  (cmd_pa & 0xff0000) >> 8);
	si_obio_udc_write(si, UDC_ADR_CAR_LOW,
					  (cmd_pa & 0xffff));

	/* Tell the chip to be a DMA master. */
	si_obio_udc_write(si, UDC_ADR_MODE, UDC_MODE);

	/* Tell the chip to interrupt on error. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_CIE);

	/* Will do "start chain" command in _dma_start. */
}


void
si_obio_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int s;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: sr=%p\n", sr);
	}
#endif

	/* This MAY be time critical (not sure). */
	s = splhigh();

	/* Finally, give the UDC a "start chain" command. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_STRT_CHN);

	/*
	 * Acknowledge the phase change.  (After DMA setup!)
	 * Put the SBIC into DMA mode, and start the transfer.
	 */
	if (dh->dh_flags & SIDH_OUT) {
		*ncr_sc->sci_tcmd = PHASE_DATA_OUT;
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = SCI_ICMD_DATA;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_dma_send = 0;	/* start it */
	} else {
		*ncr_sc->sci_tcmd = PHASE_DATA_IN;
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = 0;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_irecv = 0;	/* start it */
	}

	splx(s);
	ncr_sc->sc_state |= NCR_DOINGDMA;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: started, flags=0x%x\n",
			   ncr_sc->sc_state);
	}
#endif
}


void
si_obio_dma_eop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{

	/* Not needed - DMA was stopped prior to examining sci_csr */
}


void
si_obio_dma_stop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int resid, ntrans, tmo, udc_cnt;

	if ((ncr_sc->sc_state & NCR_DOINGDMA) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	ncr_sc->sc_state &= ~NCR_DOINGDMA;

	NCR_TRACE("si_dma_stop: top, csr=0x%x\n", si->si_csr);

	/* OK, have either phase mis-match or end of DMA. */
	/* Set an impossible phase to prevent data movement? */
	*ncr_sc->sci_tcmd = PHASE_INVALID;

	/* Check for DMA errors. */
	if (si->si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si->si_csr);
		sr->sr_xs->error = XS_DRIVER_STUFFUP;
		ncr_sc->sc_state |= NCR_ABORTING;
		si_obio_reset(ncr_sc);
		goto out;
	}

	/* Note that timeout may have set the error flag. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		goto out;

	/*
	 * After a read, wait for the FIFO to empty.
	 * Note: this only works on the OBIO version.
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		tmo = 200000;	/* X10 = 2 sec. */
		for (;;) {
			if (si->si_csr & SI_CSR_FIFO_EMPTY)
				break;
			if (--tmo <= 0) {
				printf("si: dma fifo did not empty, reset\n");
				ncr_sc->sc_state |= NCR_ABORTING;
				/* si_obio_reset(ncr_sc); */
				goto out;
			}
			delay(10);
		}
	}

	/*
	 * Now try to figure out how much actually transferred.
	 * The fifo_count might not reflect how many bytes were
	 * actually transferred.
	 */
	resid = si->fifo_count & 0xFFFF;
	ntrans = sc->sc_reqlen - resid;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: resid=0x%x ntrans=0x%x\n",
		       resid, ntrans);
	}
#endif

	/* XXX: Treat (ntrans==0) as a special, non-error case? */
	if (ntrans < MIN_DMA_LEN) {
		printf("si: fifo count: 0x%x\n", resid);
		ncr_sc->sc_state |= NCR_ABORTING;
		goto out;
	}
	if (ntrans > ncr_sc->sc_datalen)
		panic("si_dma_stop: excess transfer");

	/* Adjust data pointer */
	ncr_sc->sc_dataptr += ntrans;
	ncr_sc->sc_datalen -= ntrans;

	/*
	 * After a read, we may need to clean-up
	 * "Left-over bytes" (yuck!)
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		/* If odd transfer count, grab last byte by hand. */
		if (ntrans & 1) {
			NCR_TRACE("si_dma_stop: leftover 1 at 0x%x\n",
				(int) ncr_sc->sc_dataptr - 1);
			ncr_sc->sc_dataptr[-1] =
				(si->fifo_data & 0xff00) >> 8;
			goto out;
		}
		/* UDC might not have transfered the last word. */
		udc_cnt = si_obio_udc_read(si, UDC_ADR_COUNT);
		if (((udc_cnt * 2) - resid) == 2) {
			NCR_TRACE("si_dma_stop: leftover 2 at 0x%x\n",
				(int) ncr_sc->sc_dataptr - 2);
			ncr_sc->sc_dataptr[-2] =
				(si->fifo_data & 0xff00) >> 8;
			ncr_sc->sc_dataptr[-1] =
				(si->fifo_data & 0x00ff);
		}
	}

out:
	/* Reset the UDC. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_RESET);
	si->fifo_count = 0;
	si->si_csr &= ~SI_CSR_SEND;

	/* Reset the FIFO */
	si->si_csr &= ~SI_CSR_FIFO_RES;     /* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	/* Put SBIC back in PIO mode. */
	/* XXX: set tcmd to PHASE_INVALID? */
	*ncr_sc->sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*ncr_sc->sci_icmd = 0;
}

