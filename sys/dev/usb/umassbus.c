/*	$NetBSD: umassbus.c,v 1.9.2.1 2001/11/12 21:18:33 thorpej Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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


#include "atapibus.h"
#include "scsibus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>


#include <dev/usb/umassbus.h>
#include <dev/usb/umassvar.h>


#define UMASS_SCSIID_HOST	0x00
#define UMASS_SCSIID_DEVICE	0x01

#define UMASS_ATAPI_DRIVE	0

Static void umass_scsipi_request(struct scsipi_channel *,
					scsipi_adapter_req_t, void *);
Static void umass_scsipi_minphys(struct buf *bp);
Static int umass_scsipi_ioctl(struct scsipi_channel *, u_long,
				   caddr_t, int, struct proc *);
Static int umass_scsipi_getgeom(struct scsipi_periph *periph,
			      struct disk_parms *, u_long sectors);

Static void umass_scsipi_cb(struct umass_softc *sc, void *priv,
				     int residue, int status);
Static void umass_scsipi_sense_cb(struct umass_softc *sc, void *priv,
				       int residue, int status);

Static int scsipiprint(void *aux, const char *pnp);
#if NATAPIBUS > 0
Static void umass_atapi_probe_device(struct atapibus_softc *, int);

const struct scsipi_bustype umass_atapi_bustype = {
	SCSIPI_BUSTYPE_ATAPI,
	atapi_scsipi_cmd,
	atapi_interpret_sense,
	atapi_print_addr,
	scsi_kill_pending,
};
#endif


int
umass_attach_bus(struct umass_softc *sc)
{
	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	/*
	 * Fill in the adapter.
	 */
	memset(&sc->bus.sc_adapter, 0, sizeof(sc->bus.sc_adapter));
	sc->bus.sc_adapter.adapt_dev = &sc->sc_dev;
	sc->bus.sc_adapter.adapt_nchannels = 1;
	sc->bus.sc_adapter.adapt_request = umass_scsipi_request;
	sc->bus.sc_adapter.adapt_minphys = umass_scsipi_minphys;
	sc->bus.sc_adapter.adapt_ioctl = umass_scsipi_ioctl;
	sc->bus.sc_adapter.adapt_getgeom = umass_scsipi_getgeom;
#if NATAPIBUS > 0
	sc->bus.sc_atapi_adapter.atapi_probe_device =  umass_atapi_probe_device;
#endif

	/* fill in the channel */
	memset(&sc->bus.sc_channel, 0, sizeof(sc->bus.sc_channel));
	sc->bus.sc_channel.chan_adapter = &sc->bus.sc_adapter;
	sc->bus.sc_channel.chan_channel = 0;
	sc->bus.sc_channel.chan_flags = SCSIPI_CHAN_OPENINGS;
	sc->bus.sc_channel.chan_openings = 1;
	sc->bus.sc_channel.chan_max_periph = 1;

	
	switch (sc->cmd_proto) {
	case CPROTO_RBC:
	case CPROTO_SCSI:
#if NSCSIBUS > 0
		sc->bus.sc_channel.chan_bustype = &scsi_bustype;
		sc->bus.sc_channel.chan_ntargets = UMASS_SCSIID_DEVICE + 1;
		sc->bus.sc_channel.chan_nluns = sc->maxlun + 1;
		sc->bus.sc_channel.chan_id = UMASS_SCSIID_HOST;
		sc->bus.sc_channel.type = BUS_SCSI;
		sc->bus.sc_child =
		    config_found(&sc->sc_dev, &sc->bus.sc_channel, scsipiprint);
#else
		printf("%s: scsibus not configured\n", USBDEVNAME(sc->sc_dev));
#endif
		break;

	case CPROTO_UFI:
	case CPROTO_ATAPI:
#if NATAPIBUS > 0
		sc->bus.sc_channel.chan_bustype = &umass_atapi_bustype;
		sc->bus.sc_channel.chan_ntargets = 2;
		sc->bus.sc_channel.chan_nluns = 1;

		sc->bus.aa.sc_aa.aa_type = T_ATAPI;
		sc->bus.aa.sc_aa.aa_channel = 0;
		sc->bus.aa.sc_aa.aa_openings = 1;
		sc->bus.aa.sc_aa.aa_drv_data = &sc->bus.aa.sc_aa_drive;
		sc->bus.aa.sc_aa.aa_bus_private = &sc->bus.sc_channel;
		if (sc->quirks & NO_TEST_UNIT_READY)
			sc->bus.sc_channel.chan_defquirks |= PQUIRK_NOTUR;
		sc->bus.sc_child =
		    config_found(&sc->sc_dev, &sc->bus.aa.sc_aa, scsipiprint);
#else
		printf("%s: atapibus not configured\n", USBDEVNAME(sc->sc_dev));
#endif
		break;

	default:
		printf("%s: cmd proto=0x%x not supported yet\n", 
		       USBDEVNAME(sc->sc_dev), sc->cmd_proto);
		return (1);
	}

	if (sc->bus.sc_child == NULL) {
		/* Not an error, just not a complete success. */
		return (1);
	}

	return (0);
}

Static int
scsipiprint(void *aux, const char *pnp)
{
	struct scsipi_channel *chan = aux;

	if (chan->chan_bustype->bustype_type == SCSIPI_BUSTYPE_SCSI) {
#if NSCSIBUS > 0
		return (scsiprint(aux, pnp));
#else
		if (pnp)
			printf("scsibus at %s", pnp);
		return (UNCONF);
#endif
	} else {
#if NATAPIBUS > 0
		struct ata_atapi_attach *aa_link = aux;
#endif
		if (pnp)
			printf("atapibus at %s", pnp);
#if NATAPIBUS > 0
		printf(" channel %d", aa_link->aa_channel);
#endif
		return (UNCONF);
	}
}

int
umass_detach_bus(struct umass_softc *sc, int flags)
{
	if (sc->bus.sc_child != NULL)
		return config_detach(sc->bus.sc_child, flags);
	else
		return 0;
}

int
umass_activate(struct device *self, enum devact act)
{
	struct umass_softc *sc = (struct umass_softc *) self;
	int rv = 0;

	DPRINTF(UDMASS_USB, ("%s: umass_activate: %d\n",
	    USBDEVNAME(sc->sc_dev), act));

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if (sc->bus.sc_child == NULL)
			break;
		rv = config_deactivate(sc->bus.sc_child);
		DPRINTF(UDMASS_USB, ("%s: umass_activate: child "
		    "returned %d\n", USBDEVNAME(sc->sc_dev), rv));
		if (rv == 0)
			sc->sc_dying = 1;
		break;
	}
	return (rv);
}

Static void
umass_scsipi_request(struct scsipi_channel *chan,
		scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_adapter *adapt = chan->chan_adapter;
	struct scsipi_periph *periph;
	struct scsipi_xfer *xs;
	struct umass_softc *sc = (void *)adapt->adapt_dev;
	struct scsipi_generic *cmd, trcmd;
	int cmdlen;
	int dir;
#ifdef UMASS_DEBUG
	microtime(&sc->tv);
#endif
	switch(req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		DIF(UDMASS_UPPER, periph->periph_dbflags |= SCSIPI_DEBUG_FLAGS);

		DPRINTF(UDMASS_CMD, ("%s: umass_scsi_cmd: at %lu.%06lu: %d:%d "
		    "xs=%p cmd=0x%02x datalen=%d (quirks=0x%x, poll=%d)\n",
		    USBDEVNAME(sc->sc_dev), sc->tv.tv_sec, sc->tv.tv_usec,
		    periph->periph_target, periph->periph_lun,
		    xs, xs->cmd->opcode, xs->datalen,
		    periph->periph_quirks, xs->xs_control & XS_CTL_POLL));
#if defined(USB_DEBUG) && defined(SCSIPI_DEBUG)
		if (umassdebug & UDMASS_SCSI)
			show_scsipi_xs(xs);
		else if (umassdebug & ~UDMASS_CMD)
			show_scsipi_cmd(xs);
#endif

		if (sc->sc_dying) {
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}

#ifdef UMASS_DEBUG
		if (chan->chan_bustype->bustype_type == SCSIPI_BUSTYPE_ATAPI ?
		    periph->periph_target != UMASS_ATAPI_DRIVE :
		    periph->periph_target != UMASS_SCSIID_DEVICE) {
			DPRINTF(UDMASS_SCSI, ("%s: wrong SCSI ID %d\n",
			    USBDEVNAME(sc->sc_dev),
			    periph->periph_target));
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}
#endif

		cmd = xs->cmd;
		cmdlen = xs->cmdlen;

		/* XXX should use transform */

		if (cmd->opcode == START_STOP &&
		    (sc->quirks & NO_START_STOP)) {
			/*printf("%s: START_STOP\n", USBDEVNAME(sc->sc_dev));*/
			xs->error = XS_NOERROR;
			goto done;
		}

		if (cmd->opcode == INQUIRY &&
		    (sc->quirks & FORCE_SHORT_INQUIRY)) {
			/*
			 * some drives wedge when asked for full inquiry
			 * information.
			 */
			memcpy(&trcmd, cmd, sizeof trcmd);
			trcmd.bytes[4] = SHORT_INQUIRY_LENGTH;
			cmd = &trcmd;
		}

		dir = DIR_NONE;
		if (xs->datalen) {
			switch (xs->xs_control &
			    (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			case XS_CTL_DATA_IN:
				dir = DIR_IN;
				break;
			case XS_CTL_DATA_OUT:
				dir = DIR_OUT;
				break;
			}
		}

		if (xs->datalen > UMASS_MAX_TRANSFER_SIZE) {
			printf("umass_cmd: large datalen, %d\n", xs->datalen);
			xs->error = XS_DRIVER_STUFFUP;
			goto done;
		}

		if (xs->xs_control & XS_CTL_POLL) {
			/* Use sync transfer. XXX Broken! */
			DPRINTF(UDMASS_SCSI,
			    ("umass_scsi_cmd: sync dir=%d\n", dir));
			sc->sc_xfer_flags = USBD_SYNCHRONOUS;
			sc->bus.sc_sync_status = USBD_INVAL;
			sc->transfer(sc, periph->periph_lun, cmd, cmdlen,
				     xs->data, xs->datalen, dir, 0, xs);
			sc->sc_xfer_flags = 0;
			DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: done err=%d\n", 
					      sc->bus.sc_sync_status));
			switch (sc->bus.sc_sync_status) {
			case USBD_NORMAL_COMPLETION:
				xs->error = XS_NOERROR;
				break;
			case USBD_TIMEOUT:
				xs->error = XS_TIMEOUT;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			goto done;
		} else {
			DPRINTF(UDMASS_SCSI,
			    ("umass_scsi_cmd: async dir=%d, cmdlen=%d"
				      " datalen=%d\n",
				      dir, cmdlen, xs->datalen));
			sc->transfer(sc, periph->periph_lun, cmd, cmdlen,
			    xs->data, xs->datalen, dir, umass_scsipi_cb, xs);
			return;
		}

		/* Return if command finishes early. */
 done:
		scsipi_done(xs);
		return;
	default:
		/* Not supported, nothing to do. */
		;
	}
}

Static void
umass_scsipi_minphys(struct buf *bp)
{
#ifdef DIAGNOSTIC
	if (bp->b_bcount <= 0) {
		printf("umass_scsipi_minphys count(%ld) <= 0\n",
		       bp->b_bcount);
		bp->b_bcount = UMASS_MAX_TRANSFER_SIZE;
	}
#endif
	if (bp->b_bcount > UMASS_MAX_TRANSFER_SIZE)
		bp->b_bcount = UMASS_MAX_TRANSFER_SIZE;
	minphys(bp);
}

int
umass_scsipi_ioctl(struct scsipi_channel *chan, u_long cmd, caddr_t arg,
		   int flag, struct proc *p)
{
	/*struct umass_softc *sc = link->adapter_softc;*/

	switch (cmd) {
#if 0
	case SCBUSIORESET:
		ccb->ccb_h.status = CAM_REQ_INPROG;
		umass_reset(sc, umass_cam_cb, (void *) ccb);
		return (0);
#endif
	default:
		return (ENOTTY);
	}
}

Static int
umass_scsipi_getgeom(struct scsipi_periph *periph, struct disk_parms *dp,
		     u_long sectors)
{
	struct umass_softc *sc =
	    (void *)periph->periph_channel->chan_adapter->adapt_dev;

	/* If it's not a floppy, we don't know what to do. */
	if (sc->cmd_proto != CPROTO_UFI)
		return (0);

	switch (sectors) {
	case 1440:
		/* Most likely a single density 3.5" floppy. */
		dp->heads = 2;
		dp->sectors = 9;
		dp->cyls = 80;
		return (1);
	case 2880:
		/* Most likely a double density 3.5" floppy. */
		dp->heads = 2;
		dp->sectors = 18;
		dp->cyls = 80;
		return (1);
	default:
		return (0);
	}
}

Static void
umass_scsipi_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	struct scsipi_xfer *xs = priv;
	struct scsipi_periph *periph = xs->xs_periph;
	int cmdlen;
	int s;
#ifdef UMASS_DEBUG
	struct timeval tv;
	u_int delta;
	microtime(&tv);
	delta = (tv.tv_sec - sc->tv.tv_sec) * 1000000 + tv.tv_usec - sc->tv.tv_usec;
#endif

	DPRINTF(UDMASS_CMD,("umass_scsipi_cb: at %lu.%06lu, delta=%u: xs=%p residue=%d"
	    " status=%d\n", tv.tv_sec, tv.tv_usec, delta, xs, residue, status));

	xs->resid = residue;

	switch (status) {
	case STATUS_CMD_OK:
		xs->error = XS_NOERROR;
		break;

	case STATUS_CMD_UNKNOWN:
	case STATUS_CMD_FAILED:
		/* fetch sense data */
		memset(&sc->bus.sc_sense_cmd, 0, sizeof(sc->bus.sc_sense_cmd));
		sc->bus.sc_sense_cmd.opcode = REQUEST_SENSE;
		sc->bus.sc_sense_cmd.byte2 = periph->periph_lun <<
		    SCSI_CMD_LUN_SHIFT;
		sc->bus.sc_sense_cmd.length = sizeof(xs->sense);

		cmdlen = sizeof(sc->bus.sc_sense_cmd);
		if (sc->cmd_proto == CPROTO_UFI) /* XXX */
			cmdlen = UFI_COMMAND_LENGTH;
		sc->transfer(sc, periph->periph_lun,
			     &sc->bus.sc_sense_cmd, cmdlen,
			     &xs->sense, sizeof(xs->sense), DIR_IN,
			     umass_scsipi_sense_cb, xs);
		return;

	case STATUS_WIRE_FAILED:
		xs->error = XS_RESET;
		break;

	default:
		panic("%s: Unknown status %d in umass_scsipi_cb\n",
			USBDEVNAME(sc->sc_dev), status);
	}

	DPRINTF(UDMASS_CMD,("umass_scsipi_cb: at %lu.%06lu: return xs->error="
            "%d, xs->xs_status=0x%x xs->resid=%d\n",
	     tv.tv_sec, tv.tv_usec,
	     xs->error, xs->xs_status, xs->resid));

	s = splbio();
	scsipi_done(xs);
	splx(s);
}

/* 
 * Finalise a completed autosense operation
 */
Static void
umass_scsipi_sense_cb(struct umass_softc *sc, void *priv, int residue,
		      int status)
{
	struct scsipi_xfer *xs = priv;
	int s;

	DPRINTF(UDMASS_CMD,("umass_scsipi_sense_cb: xs=%p residue=%d "
		"status=%d\n", xs, residue, status));

	switch (status) {
	case STATUS_CMD_OK:
	case STATUS_CMD_UNKNOWN:
		/* getting sense data succeeded */
		if (xs->cmd->opcode == INQUIRY && (xs->resid < xs->datalen
		    || ((sc->quirks & RS_NO_CLEAR_UA) /* XXX */) )) {
			/*
			 * Some drivers return SENSE errors even after INQUIRY.
			 * The upper layer doesn't like that.
			 */
			xs->error = XS_NOERROR;
			break;
		}
		/* XXX look at residue */
		if (residue == 0 || residue == 14)/* XXX */
			xs->error = XS_SENSE;
		else
			xs->error = XS_SHORTSENSE;
		break;
	default:
		DPRINTF(UDMASS_SCSI, ("%s: Autosense failed, status %d\n",
			USBDEVNAME(sc->sc_dev), status));
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	xs->xs_status |= XS_STS_DONE;

	DPRINTF(UDMASS_CMD,("umass_scsipi_sense_cb: return xs->error=%d, "
		"xs->xs_status=0x%x xs->resid=%d\n", xs->error, xs->xs_status,
		xs->resid));

	s = splbio();
	scsipi_done(xs);
	splx(s);
}

#if NATAPIBUS > 0
Static void
umass_atapi_probe_device(struct atapibus_softc *atapi, int target)
{
	struct scsipi_channel *chan = atapi->sc_channel;
	struct scsipi_periph *periph;
	struct scsipibus_attach_args sa;
	struct ata_drive_datas *drvp = &atapi->sc_drvs[target];
	char vendor[33], product[65], revision[17];
	struct scsipi_inquiry_data inqbuf;

	DPRINTF(UDMASS_SCSI,("umass_atapi_probe_device: atapi=%p target=%d\n",
			     atapi, target));

	if (target != UMASS_ATAPI_DRIVE)	/* only probe drive 0 */
		return;

	/* skip if already attached */
	if (scsipi_lookup_periph(chan, target, 0) != NULL)
		return;

	periph = scsipi_alloc_periph(M_NOWAIT);
	if (periph == NULL) {
		printf("%s: can't allocate link for drive %d\n",
		       atapi->sc_dev.dv_xname, target);
		return;       
	}

	DIF(UDMASS_UPPER, periph->periph_dbflags |= 1); /* XXX 1 */
	periph->periph_channel = chan;
	periph->periph_switch = &atapi_probe_periphsw;
	periph->periph_target = target;

	DPRINTF(UDMASS_SCSI, ("umass_atapi_probe_device: doing inquiry\n"));
	/* Now go ask the device all about itself. */
	memset(&inqbuf, 0, sizeof(inqbuf));
	if (scsipi_inquire(periph, &inqbuf,
	    XS_CTL_DISCOVERY | XS_CTL_DATA_ONSTACK) != 0) {
		DPRINTF(UDMASS_SCSI, ("umass_atapi_probe_device: "
		    "scsipi_inquire failed\n"));
		free(periph, M_DEVBUF);
		return;
	}

	scsipi_strvis(vendor, 33, inqbuf.vendor, 8);
	scsipi_strvis(product, 65, inqbuf.product, 16);
	scsipi_strvis(revision, 17, inqbuf.revision, 4);

	sa.sa_periph = periph;
	sa.sa_inqbuf.type = inqbuf.device;
	sa.sa_inqbuf.removable = inqbuf.dev_qual2 & SID_REMOVABLE ?
	    T_REMOV : T_FIXED;
	if (sa.sa_inqbuf.removable)
		periph->periph_flags |= PERIPH_REMOVABLE;
	sa.sa_inqbuf.vendor = vendor;
	sa.sa_inqbuf.product = product;
	sa.sa_inqbuf.revision = revision;
	sa.sa_inqptr = NULL;

	DPRINTF(UDMASS_SCSI, ("umass_atapi_probedev: doing atapi_probedev on "
			      "'%s' '%s' '%s'\n", vendor, product, revision));
	drvp->drv_softc = atapi_probe_device(atapi, target, periph, &sa);
	/* atapi_probe_device() frees the periph when there is no device.*/
}
#endif
