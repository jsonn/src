/*	$NetBSD: atapi_wdc.c,v 1.1.2.3 1998/06/19 21:57:56 leo Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 */

#define WDCDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/intr.h>
#include <machine/bus.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define    bus_space_write_multi_stream_2    bus_space_write_multi_2
#define    bus_space_write_multi_stream_4    bus_space_write_multi_4
#define    bus_space_read_multi_stream_2    bus_space_read_multi_2
#define    bus_space_read_multi_stream_4    bus_space_read_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/atapiconf.h>

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef WDCDEBUG
int wdcdebug_atapi_mask = DEBUG_PROBE;
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_atapi_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

int   atapi_print	__P((void *, const char *));
void  wdc_atapi_minphys  __P((struct buf *bp));
void  wdc_atapi_start	__P((struct channel_softc *,struct wdc_xfer *));
int   wdc_atapi_intr	 __P((struct channel_softc *, struct wdc_xfer *));
int   wdc_atapi_ctrl	 __P((struct channel_softc *, struct wdc_xfer *));
void  wdc_atapi_done	 __P((struct channel_softc *, struct wdc_xfer *));
int   wdc_atapi_send_cmd __P((struct scsipi_xfer *sc_xfer));

#define MAX_SIZE MAXPHYS

static struct scsipi_adapter wdc_switch  = {
	wdc_atapi_send_cmd,
	wdc_atapi_minphys,
	0,
	0
};

int atapi_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct ata_atapi_attach *aa_link = aux;
	if (pnp)
		printf("atapibus at %s", pnp);
	printf(" channel %d", aa_link->aa_channel);
	return (UNCONF);
}

void
wdc_atapibus_attach(chp)
	struct channel_softc *chp;
{
	struct wdc_softc *wdc = chp->wdc;
	int channel = chp->channel;
	struct ata_atapi_attach aa_link;

	bzero(&aa_link,sizeof(struct ata_atapi_attach));
	aa_link.aa_type = T_ATAPI;
	aa_link.aa_channel = channel;
	aa_link.aa_openings = 1;
	aa_link.aa_drv_data = chp->ch_drive; /* pass the whole array */
	aa_link.aa_bus_private = &wdc_switch;
	(void)config_found(&wdc->sc_dev, (void *)&aa_link, atapi_print);
}

void
wdc_atapi_minphys (struct buf *bp)
{
	if(bp->b_bcount > MAX_SIZE)
		bp->b_bcount = MAX_SIZE;
	minphys(bp);
}

int
wdc_atapi_get_params(ab_link, drive, flags, id)
	struct scsipi_link *ab_link;
	u_int8_t drive;
	int flags;
	struct ataparams *id;
{
	struct wdc_softc *wdc = (void*)ab_link->adapter_softc;
	struct channel_softc *chp =
	    &wdc->channels[ab_link->scsipi_atapi.channel];
	struct wdc_command wdc_c;

	/* if no ATAPI device detected at wdc attach time, skip */
	/*
	 * XXX this will break scsireprobe if this is of any interest for
	 * ATAPI devices one day.
	 */
	if ((chp->ch_drive[drive].drive_flags & DRIVE_ATAPI) == 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: drive %d not present\n",
		    drive), DEBUG_PROBE);
		return -1;
	}
	bzero(&wdc_c, sizeof(struct wdc_command));
	wdc_c.r_command = ATAPI_SOFT_RESET;
	wdc_c.r_st_bmask = 0;
	wdc_c.r_st_pmask = 0;
	wdc_c.flags = AT_POLL;
	if (wdc_exec_command(&chp->ch_drive[drive], &wdc_c) != WDC_COMPLETE) {
		printf("wdc_atapi_get_params: ATAPI_SOFT_RESET failed for"
		    " drive %s:%d:%d: driver failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive);
		panic("wdc_atapi_get_params");
	}
	if (wdc_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_SOFT_RESET "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive, 
		    wdc_c.r_error), DEBUG_PROBE);
		return -1;
	}
	chp->ch_drive[drive].state = 0;

	bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_status);
	
	/* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);
	if (ata_get_params(&chp->ch_drive[drive], AT_POLL, id) != 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_IDENTIFY_DEVICE "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive, 
		    wdc_c.r_error), DEBUG_PROBE);
		return -1;
	}
	return COMPLETE;
}

int
wdc_atapi_send_cmd(sc_xfer)
	struct scsipi_xfer *sc_xfer;
{
	struct scsipi_link *sc_link = sc_xfer->sc_link;
	struct wdc_softc *wdc = (void*)sc_link->adapter_softc;
	struct wdc_xfer *xfer;
	int flags = sc_xfer->flags;
	int s, ret;

	WDCDEBUG_PRINT(("wdc_atapi_send_cmd\n"), DEBUG_FUNCS);

	xfer = wdc_get_xfer(flags & SCSI_NOSLEEP ? WDC_NOSLEEP : WDC_CANSLEEP);
	if (xfer == NULL) {
		return TRY_AGAIN_LATER;
	}
	xfer->drive = sc_xfer->sc_link->scsipi_atapi.drive;
	xfer->channel = sc_xfer->sc_link->scsipi_atapi.channel;
	xfer->c_flags |= C_ATAPI;
	xfer->cmd = sc_xfer;
	xfer->databuf = sc_xfer->data;
	xfer->c_bcount = sc_xfer->datalen;
	xfer->c_start = wdc_atapi_start;
	xfer->c_intr = wdc_atapi_intr;
	s = splbio();
	wdc_exec_xfer(&wdc->channels[xfer->channel], xfer);
#ifdef DIAGNOSTIC
	if ((sc_xfer->flags & SCSI_POLL) != 0 &&
	    (sc_xfer->flags & ITSDONE) == 0)
		panic("wdc_atapi_send_cmd: polled command not done");
#endif
	ret = (sc_xfer->flags & ITSDONE) ? COMPLETE : SUCCESSFULLY_QUEUED;
	splx(s);
	return ret;
}

void
wdc_atapi_start(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsipi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	WDCDEBUG_PRINT(("wdc_atapi_start, scsi flags 0x%x \n",sc_xfer->flags),
	DEBUG_FUNCS);
	/* Do control operations specially. */
	if (drvp->state < READY) {
		if (drvp->state != PIOMODE) {
			printf("%s:%d:%d: bad state %d in wdc_atapi_start\n",
			    chp->wdc->sc_dev.dv_xname, chp->channel,
			    xfer->drive, drvp->state);
			panic("wdc_atapi_start: bad state");
		}
		wdc_atapi_ctrl(chp, xfer);
		return;
	}
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_sdh,
	    WDSD_IBM | (xfer->drive << 4));
	if (wait_for_unbusy(chp) < 0) {
		printf("wdc_atapi_start: not ready, st = %02x\n",
		    chp->ch_status);
		sc_xfer->error = XS_SELTIMEOUT;
		wdc_atapi_done(chp, xfer);
		return;
	}

	/*
	 * Even with WDCS_ERR, the device should accept a command packet
	 * Limit length to what can be stuffed into the cylinder register
	 * (16 bits).  Some CD-ROMs seem to interpret '0' as 65536,
	 * but not all devices do that and it's not obvious from the
	 * ATAPI spec that that behaviour should be expected.  If more
	 * data is necessary, multiple data transfer phases will be done.
	 */
	wdccommand(chp, xfer->drive, ATAPI_PKT_CMD, 
	    sc_xfer->datalen <= 0xffff ? sc_xfer->datalen : 0xffff,
	    0, 0, 0, 
	    (chp->ch_drive[xfer->drive].drive_flags & DRIVE_DMA) ? 
		ATAPI_PKT_CMD_FTRE_DMA : 0);
	
	/*
	 * If there is no interrupt for CMD input, busy-wait for it (done in 
	 * the interrupt routine. If it is a polled command, call the interrupt
	 * routine until command is done.
	 */
	if ((sc_xfer->sc_link->scsipi_atapi.cap  & 0x0300) != ACAP_DRQ_INTR || 
	    sc_xfer->flags & SCSI_POLL) {
		/* Wait for at last 400ns for status bit to be valid */
		delay(1);
		if (wdc_atapi_intr(chp, xfer) == 0) {
			sc_xfer->error = XS_SELTIMEOUT; /* do we know more ? */
			wdc_atapi_done(chp, xfer);
			return;
		}
	}
	if (sc_xfer->flags & SCSI_POLL) {
		while ((sc_xfer->flags & ITSDONE) == 0) {
			/* Wait for at last 400ns for status bit to be valid */
			delay(1);
			if (wdc_atapi_intr(chp, xfer) == 0) {
				sc_xfer->error = XS_SELTIMEOUT;
				    /* do we know more ? */
				wdc_atapi_done(chp, xfer);
				return;
			}
		}
	} else {
		chp->ch_flags |= WDCF_IRQ_WAIT; /* wait for interrupt */
		timeout(wdctimeout, chp, WAITTIME);
	}
}

int
wdc_atapi_intr(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsipi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	int len, phase, i, retries=0;
	int ire, dma_err = 0;

	WDCDEBUG_PRINT(("wdc_atapi_intr\n"), DEBUG_INTR);

	/* Is it not a transfer, but a control operation? */
	if (drvp->state < READY) {
		printf("%s:%d:%d: bad state %d in wdc_atapi_intr\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		    drvp->state);
		panic("wdc_atapi_intr: bad state\n");
	}
	/* Ack interrupt done in wait_for_unbusy */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_sdh,
	    WDSD_IBM | (xfer->drive << 4));
	if (wait_for_unbusy(chp) != 0) {
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip%d\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		    xfer->c_bcount, xfer->c_skip);
		wdccommandshort(chp, xfer->drive, ATAPI_SOFT_RESET);
		drvp->state = 0;
		if (wait_for_unbusy(chp) != 0)
			printf("%s:%d:%d: reset failed\n",
			    chp->wdc->sc_dev.dv_xname, chp->channel,
			    xfer->drive);
		sc_xfer->error = XS_SELTIMEOUT;
		wdc_atapi_done(chp, xfer);
		return 1;
	}
	/* Error here only if the command is aborted */
	if ((chp->ch_status & WDCS_ERR) != 0 &&
	    (chp->ch_error & WDCE_ABRT) != 0) {
		sc_xfer->error = XS_SENSE;
		sc_xfer->sense.atapi_sense = chp->ch_error;
		WDCDEBUG_PRINT(("wdc_atapi_intr: wdc_atapi_done(), "
		    "sense 0x%x\n", sc_xfer->sense.atapi_sense), DEBUG_INTR);
		wdc_atapi_done(chp, xfer);
		return 1;
	}

again:
	len = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_lo) +
	    256 * bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_hi);
	ire = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_ireason);
	phase = (ire & (WDCI_CMD | WDCI_IN)) | (chp->ch_status & WDCS_DRQ);
	WDCDEBUG_PRINT(("wdc_atapi_intr: c_bcount %d len %d st 0x%x err 0x%x "
	    "ire 0x%x :", xfer->c_bcount,
	    len, chp->ch_status, chp->ch_error, ire), DEBUG_INTR);

	switch (phase) {
	case PHASE_CMDOUT:
		WDCDEBUG_PRINT(("PHASE_CMDOUT\n"), DEBUG_INTR);
		/* Init the DMA channel if necessary */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    sc_xfer->datalen > 0) {
			if ((*chp->wdc->dma_init)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive,
			    xfer->databuf, sc_xfer->datalen,
			    sc_xfer->flags & SCSI_DATA_IN) != 0) {
				sc_xfer->error = XS_DRIVER_STUFFUP;
				break;
			}
		}
		/* send packet command */
		/* Commands are 12 or 16 bytes long. It's 32-bit aligned */
		if (drvp->drive_flags & DRIVE_CAP32) {
			bus_space_write_multi_4(chp->cmd_iot, chp->cmd_ioh,
			    wd_data, (u_int32_t *)sc_xfer->cmd,
			    sc_xfer->cmdlen >> 2);
		} else {
			bus_space_write_multi_2(chp->cmd_iot, chp->cmd_ioh,
			    wd_data, (u_int16_t *)sc_xfer->cmd,
			    sc_xfer->cmdlen >> 1);
		}
		/* Start the DMA channel if necessary */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    sc_xfer->datalen > 0) {
			(*chp->wdc->dma_start)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive,
			    sc_xfer->flags & SCSI_DATA_IN);
		}

		if ((sc_xfer->sc_link->scsipi_atapi.cap  & 0x0300) ==
		    ACAP_DRQ_INTR &&
		    (sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, WAITTIME);
		}
		return 1;

	 case PHASE_DATAOUT:
		/* write data */
		WDCDEBUG_PRINT(("PHASE_DATAOUT\n"), DEBUG_INTR);
		if ((sc_xfer->flags & SCSI_DATA_OUT) == 0) {
			printf("wdc_atapi_intr: bad data phase\n");
			sc_xfer->error = XS_DRIVER_STUFFUP;
			break;
		}
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: write only "
			    "%d of %d requested bytes\n", xfer->c_bcount, len);
			bus_space_write_multi_2(chp->cmd_iot, chp->cmd_ioh,
			    wd_data, xfer->databuf + xfer->c_skip,
			    xfer->c_bcount >> 1);
			for (i = xfer->c_bcount; i < len; i += 2)
				bus_space_write_2(chp->cmd_iot, chp->cmd_ioh,
				    wd_data, 0);
			xfer->c_skip += xfer->c_bcount;
			xfer->c_bcount = 0;
		} else {
			if (drvp->drive_flags & DRIVE_CAP32) {
				bus_space_write_multi_4(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    xfer->databuf + xfer->c_skip, len >> 2);
				xfer->c_skip += len & 0xfffffffc;
				xfer->c_bcount -= len & 0xfffffffc;
				len = len & 0x03;
			}
			if (len > 0) {
				bus_space_write_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    xfer->databuf + xfer->c_skip, len >> 1);
				xfer->c_skip += len;
				xfer->c_bcount -= len;
			}
		}
		if ((sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, WAITTIME);
		}
		return 1;

	case PHASE_DATAIN:
		/* Read data */
		WDCDEBUG_PRINT(("PHASE_DATAIN\n"), DEBUG_INTR);
		if ((sc_xfer->flags & SCSI_DATA_IN) == 0) {
			printf("wdc_atapi_intr: bad data phase\n");
			sc_xfer->error = XS_DRIVER_STUFFUP;
			break;
		}
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: reading only "
			    "%d of %d bytes\n", xfer->c_bcount, len);
			bus_space_read_multi_2(chp->cmd_iot, chp->cmd_ioh,
			    wd_data, xfer->databuf + xfer->c_skip,
			    xfer->c_bcount >> 1);
			wdcbit_bucket(chp, len - xfer->c_bcount);
			xfer->c_skip += xfer->c_bcount;
			xfer->c_bcount = 0;
		} else {
			if (drvp->drive_flags & DRIVE_CAP32) {
				bus_space_read_multi_4(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    xfer->databuf + xfer->c_skip, len >> 2);
				xfer->c_skip += len & 0xfffffffc;
				xfer->c_bcount -= len & 0xfffffffc;
				len = len & 0x03;
			}
			if (len > 0) {
				bus_space_read_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    xfer->databuf + xfer->c_skip, len >> 1);
				xfer->c_skip += len;
				xfer->c_bcount -=len;
			}
		}
		if ((sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, WAITTIME);
		}
		return 1;

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
		WDCDEBUG_PRINT(("PHASE_COMPLETED\n"), DEBUG_INTR);
		/* turn off DMA channel */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    sc_xfer->datalen > 0) {
			dma_err = (*chp->wdc->dma_finish)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive,
			    sc_xfer->flags & SCSI_DATA_IN);
			xfer->c_bcount -= sc_xfer->datalen;
		}
			
		if (chp->ch_status & WDCS_ERR) {
			sc_xfer->error = XS_SENSE;
			sc_xfer->sense.atapi_sense = chp->ch_error;
		} else if (dma_err) {
			sc_xfer->error = XS_DRIVER_STUFFUP;
		}
#ifdef DIAGNOSTIC
		if (xfer->c_bcount != 0) {
			printf("wdc_atapi_intr warning: bcount value "
			    "is %d after io\n", xfer->c_bcount);
		}
#endif
		break;

	default:
		if (++retries<500) {
			DELAY(100);
			chp->ch_status = bus_space_read_1(chp->cmd_iot,
			    chp->cmd_ioh, wd_status);
			chp->ch_error = bus_space_read_1(chp->cmd_iot,
			    chp->cmd_ioh, wd_error);
			goto again;
		}
		printf("wdc_atapi_intr: unknown phase 0x%x\n", phase);
		if (chp->ch_status & WDCS_ERR) {
			sc_xfer->error = XS_SENSE;
			sc_xfer->sense.atapi_sense = chp->ch_error;
		} else {
			sc_xfer->error = XS_DRIVER_STUFFUP;
		}
	}
	WDCDEBUG_PRINT(("wdc_atapi_intr: wdc_atapi_done() (end), sense 0x%x\n",
	    sc_xfer->sense.atapi_sense), DEBUG_INTR);
	wdc_atapi_done(chp, xfer);
	return (1);
}

int
wdc_atapi_ctrl(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsipi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	char *errstring = NULL;

	WDCDEBUG_PRINT(("wdc_atapi_ctrl state %d\n", drvp->state), DEBUG_INTR);
	/* Ack interrupt done in wait_for_unbusy */
again:
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_sdh,
	    WDSD_IBM | (xfer->drive << 4));
	switch (drvp->state) {
	case PIOMODE:
		/* Don't try to set mode if controller can't be adjusted */
		if ((chp->wdc->cap & WDC_CAPABILITY_PIO) == 0)
			goto dmamode;
		/*
		 * if mode is < 3, it is unknown. Assume the defaults are
		 * good.
		 */
		if (drvp->PIO_mode < 3)
			goto dmamode;
		wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
		    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
		drvp->state = PIOMODE_WAIT;
		break;
	case PIOMODE_WAIT:
		errstring = "piomode";
		if (wait_for_unbusy(chp))
			goto timeout;
		if (chp->ch_status & WDCS_ERR)
			goto error;
	/* fall through */

	case DMAMODE:
	dmamode:
		if (drvp->drive_flags & DRIVE_UDMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x40 | drvp->UDMA_mode, WDSF_SET_MODE);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x20 | drvp->DMA_mode, WDSF_SET_MODE);
		} else {
			goto ready;
		}
		drvp->state = DMAMODE_WAIT;
		break;
	case DMAMODE_WAIT:
		errstring = "dmamode";
		if (wait_for_unbusy(chp))
			goto timeout;
		if (chp->ch_status & WDCS_ERR)
			goto error;
	/* fall through */

	case READY:
	ready:
		drvp->state = READY;
		xfer->c_intr = wdc_atapi_intr;
		wdc_atapi_start(chp, xfer);
		return 1;
	}
	if ((sc_xfer->flags & SCSI_POLL) == 0) {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		xfer->c_intr = wdc_atapi_ctrl;
		timeout(wdctimeout, chp, WAITTIME);
	} else {
		goto again;
	}
	return 1;

timeout:
	if ((xfer->c_flags & C_TIMEOU) == 0 ) {
		return 0; /* IRQ was not for us */
	}
	printf("%s:%d:%d: %s timed out\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive, errstring);
	wdccommandshort(chp, xfer->drive, ATAPI_SOFT_RESET);
	drvp->state = 0;
	if (wait_for_unbusy(chp) != 0)
		printf("%s:%d:%d: reset failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel,
		    xfer->drive);
	sc_xfer->error = XS_SELTIMEOUT;
	wdc_atapi_done(chp, xfer);
	return 0;
error:
	printf("%s:%d:%d: %s ",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    errstring);
	printf("error (%x)\n", chp->ch_error);
	sc_xfer->error = XS_SENSE;
	sc_xfer->sense.atapi_sense = chp->ch_error;
	drvp->state = 0;
	wdc_atapi_done(chp, xfer);
	return 0;
}

void
wdc_atapi_done(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsipi_xfer *sc_xfer = xfer->cmd;
	struct wdc_softc *wdc = chp->wdc;
	int need_done =  xfer->c_flags & C_NEEDDONE;

	WDCDEBUG_PRINT(("wdc_atapi_done: flags 0x%x\n", (u_int)xfer->c_flags),
	    DEBUG_FUNCS);
	sc_xfer->resid = xfer->c_bcount;
	/* remove this command from xfer queue */
	xfer->c_skip = 0;
	wdc_free_xfer(chp, xfer);
	sc_xfer->flags |= ITSDONE;
	if (need_done) {
		WDCDEBUG_PRINT(("wdc_atapi_done: scsipi_done\n"), DEBUG_FUNCS);
		scsipi_done(sc_xfer);
	}
	WDCDEBUG_PRINT(("wdcstart from wdc_atapi_done, flags 0x%x\n",
	    chp->ch_flags), DEBUG_FUNCS);
	wdcstart(wdc, chp->channel);
}
