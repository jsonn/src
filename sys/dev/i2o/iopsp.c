/*	$NetBSD: iopsp.c,v 1.10.6.3 2002/06/06 16:04:19 he Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Raw SCSI device support for I2O.  IOPs present SCSI devices individually;
 * we group them by controlling port.
 */

#include "opt_i2o.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/scsiio.h>
#include <sys/lock.h>

#include <machine/bswap.h>
#include <machine/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>
#include <dev/i2o/iopspvar.h>

static void	iopsp_adjqparam(struct device *, int);
static void	iopsp_attach(struct device *, struct device *, void *);
static void	iopsp_intr(struct device *, struct iop_msg *, void *);
static int	iopsp_ioctl(struct scsipi_link *, u_long,
			    caddr_t, int, struct proc *);
static int	iopsp_match(struct device *, struct cfdata *, void *);
static int	iopsp_rescan(struct iopsp_softc *);
static int	iopsp_reconfig(struct device *);
static int	iopsp_scsi_cmd(struct scsipi_xfer *);

struct cfattach iopsp_ca = {
	sizeof(struct iopsp_softc), iopsp_match, iopsp_attach
};

/* A default for our link struct */
static struct scsipi_device iopsp_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

/*
 * Match a supported device.
 */
static int
iopsp_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct iop_attach_args *ia;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_hba_ctlr_info ci;
	} __attribute__ ((__packed__)) param;

	ia = aux;

	if (ia->ia_class != I2O_CLASS_BUS_ADAPTER_PORT)
		return (0);

	if (iop_field_get_all((struct iop_softc *)parent, ia->ia_tid,
	    I2O_PARAM_HBA_CTLR_INFO, &param, sizeof(param), NULL) != 0)
		return (0);

	return (param.ci.bustype == I2O_HBA_BUS_SCSI ||
	    param.ci.bustype == I2O_HBA_BUS_FCA);
}

/*
 * Attach a supported device.
 */
static void
iopsp_attach(struct device *parent, struct device *self, void *aux)
{
	struct iop_attach_args *ia;
	struct iopsp_softc *sc;
	struct iop_softc *iop;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		union {
			struct	i2o_param_hba_ctlr_info ci;
			struct	i2o_param_hba_scsi_ctlr_info sci;
			struct	i2o_param_hba_scsi_port_info spi;
		} p;
	} __attribute__ ((__packed__)) param;
	int fc, rv;
#ifdef I2OVERBOSE
	int size;
#endif

	ia = (struct iop_attach_args *)aux;
	sc = (struct iopsp_softc *)self;
	iop = (struct iop_softc *)parent;

	/* Register us as an initiator. */
	sc->sc_ii.ii_dv = self;
	sc->sc_ii.ii_intr = iopsp_intr;
	sc->sc_ii.ii_flags = 0;
	sc->sc_ii.ii_tid = ia->ia_tid;
	sc->sc_ii.ii_reconfig = iopsp_reconfig;
	sc->sc_ii.ii_adjqparam = iopsp_adjqparam;
	iop_initiator_register(iop, &sc->sc_ii);

	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_HBA_CTLR_INFO,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

	fc = (param.p.ci.bustype == I2O_HBA_BUS_FCA);

	/* 
	 * Say what the device is.  If we can find out what the controling
	 * device is, say what that is too.
	 */
	printf(": SCSI port");
	iop_print_ident(iop, ia->ia_tid);
	printf("\n");

	rv = iop_field_get_all(iop, ia->ia_tid, I2O_PARAM_HBA_SCSI_CTLR_INFO,
	    &param, sizeof(param), NULL);
	if (rv != 0)
		goto bad;

#ifdef I2OVERBOSE
	printf("%s: ", sc->sc_dv.dv_xname);
	if (fc)
		printf("FC");
	else
		printf("%d-bit", param.p.sci.maxdatawidth);
	printf(", max sync rate %dMHz, initiator ID %d\n",
	    (u_int32_t)le64toh(param.p.sci.maxsyncrate) / 1000,
	    le32toh(param.p.sci.initiatorid));
#endif

	sc->sc_adapter.scsipi_cmd = iopsp_scsi_cmd;
	sc->sc_adapter.scsipi_ioctl = iopsp_ioctl;
	sc->sc_adapter.scsipi_minphys = minphys;

	sc->sc_link.adapter = &sc->sc_adapter;
	sc->sc_link.type = BUS_SCSI;
	sc->sc_link.device = &iopsp_dev;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.openings = 4;
	sc->sc_link.scsipi_scsi.channel = 0;
	sc->sc_link.scsipi_scsi.adapter_target =
	    le32toh(param.p.sci.initiatorid);;
	sc->sc_link.scsipi_scsi.max_lun = IOPSP_MAX_LUN - 1;
	sc->sc_link.scsipi_scsi.max_target = fc ?
	    IOPSP_MAX_FC_TARGET - 1 : param.p.sci.maxdatawidth - 1;

#ifdef I2OVERBOSE
	/*
	 * Allocate the target map.  Currently used for informational
	 * purposes only.
	 */
	size = (sc->sc_link.scsipi_scsi.max_target + 1) *
	    sizeof(struct iopsp_target);
	sc->sc_targetmap = malloc(size, M_DEVBUF, M_NOWAIT);
	memset(sc->sc_targetmap, 0, size);
#endif

 	/* Build the two maps, and attach to scsipi. */
	if (iopsp_reconfig(self) != 0) {
		printf("%s: configure failed\n", sc->sc_dv.dv_xname);
		goto bad;
	}
	config_found(self, &sc->sc_link, scsiprint);
	return;

 bad:
	iop_initiator_unregister(iop, &sc->sc_ii);
}

/*
 * Scan the LCT to determine which devices we control, and enter them into
 * the maps.
 */
static int
iopsp_reconfig(struct device *dv)
{
	struct iopsp_softc *sc;
	struct iop_softc *iop;
	struct i2o_lct_entry *le;
	struct scsipi_link *sc_link;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_param_scsi_device_info sdi;
	} __attribute__ ((__packed__)) param;
	u_int tid, nent, i, targ, lun, size, s, rv, bptid;
	u_short *tidmap;
#ifdef I2OVERBOSE
	struct iopsp_target *it;
	int syncrate;	
#endif

	sc = (struct iopsp_softc *)dv;
	iop = (struct iop_softc *)sc->sc_dv.dv_parent;
	sc_link = &sc->sc_link;

	/* Anything to do? */
	if (iop->sc_chgind == sc->sc_chgind)
		return (0);

	/*
	 * Allocate memory for the target/LUN -> TID map.  Use zero to
	 * denote absent targets (zero is the TID of the I2O executive,
	 * and we never address that here).
	 */
	size = (sc_link->scsipi_scsi.max_target + 1) * (IOPSP_MAX_LUN) *
	    sizeof(u_short);
	if ((tidmap = malloc(size, M_DEVBUF, M_WAITOK)) == NULL)
		return (ENOMEM);
	memset(tidmap, 0, size);

#ifdef I2OVERBOSE
	for (i = 0; i <= sc_link->scsipi_scsi.max_target; i++)
		sc->sc_targetmap[i].it_flags &= ~IT_PRESENT;
#endif

	/*
	 * A quick hack to handle Intel's stacked bus port arrangement.
	 */
	bptid = sc->sc_ii.ii_tid;
	nent = iop->sc_nlctent;
	for (le = iop->sc_lct->entry; nent != 0; nent--, le++)
		if ((le16toh(le->classid) & 4095) ==
		    I2O_CLASS_BUS_ADAPTER_PORT &&
		    (le32toh(le->usertid) & 4095) == bptid) {
			bptid = le16toh(le->localtid) & 4095;
			break;
		}

	nent = iop->sc_nlctent;
	for (i = 0, le = iop->sc_lct->entry; i < nent; i++, le++) {
		if ((le16toh(le->classid) & 4095) != I2O_CLASS_SCSI_PERIPHERAL)
			continue;
		if (((le32toh(le->usertid) >> 12) & 4095) != bptid)
			continue;
		tid = le16toh(le->localtid) & 4095;

		rv = iop_field_get_all(iop, tid, I2O_PARAM_SCSI_DEVICE_INFO,
		    &param, sizeof(param), NULL);
		if (rv != 0)
			continue;
		targ = le32toh(param.sdi.identifier);
		lun = param.sdi.luninfo[1];
#if defined(DIAGNOSTIC) || defined(I2ODEBUG)
		if (targ >= sc_link->scsipi_scsi.max_target ||
		    lun >= sc_link->scsipi_scsi.max_lun) {
			printf("%s: target %d,%d (tid %d): bad target/LUN\n",
			    sc->sc_dv.dv_xname, targ, lun, tid);
			continue;
		}
#endif

#ifdef I2OVERBOSE
		/*
		 * If we've already described this target, and nothing has
		 * changed, then don't describe it again.
		 */
		it = &sc->sc_targetmap[targ];
		it->it_flags |= IT_PRESENT;
		syncrate = ((int)le64toh(param.sdi.negsyncrate) + 500) / 1000;
		if (it->it_width == param.sdi.negdatawidth &&
		    it->it_offset == param.sdi.negoffset &&
		    it->it_syncrate == syncrate)
			continue;

		it->it_width = param.sdi.negdatawidth;
		it->it_offset = param.sdi.negoffset;
		it->it_syncrate = syncrate;

		printf("%s: target %d (tid %d): %d-bit, ", sc->sc_dv.dv_xname,
		    targ, tid, it->it_width);
		if (it->it_syncrate == 0)
			printf("asynchronous\n");
		else
			printf("synchronous at %dMHz, offset 0x%x\n",
			    it->it_syncrate, it->it_offset);
#endif

		/* Ignore the device if it's in use by somebody else. */
		if ((le32toh(le->usertid) & 4095) != I2O_TID_NONE) {
#ifdef I2OVERBOSE
			if (sc->sc_tidmap == NULL ||
			    IOPSP_TIDMAP(sc->sc_tidmap, targ, lun) !=
			    IOPSP_TID_INUSE)
				printf("%s: target %d,%d (tid %d): in use by"
				    " tid %d\n", sc->sc_dv.dv_xname,
				    targ, lun, tid,
				    le32toh(le->usertid) & 4095);
#endif
			IOPSP_TIDMAP(tidmap, targ, lun) = IOPSP_TID_INUSE;
		} else
			IOPSP_TIDMAP(tidmap, targ, lun) = (u_short)tid;
	}

#ifdef I2OVERBOSE
	for (i = 0; i <= sc_link->scsipi_scsi.max_target; i++)
		if ((sc->sc_targetmap[i].it_flags & IT_PRESENT) == 0)
			sc->sc_targetmap[i].it_width = 0;
#endif

	/* Swap in the new map and return. */
	s = splbio();
	if (sc->sc_tidmap != NULL)
		free(sc->sc_tidmap, M_DEVBUF);
	sc->sc_tidmap = tidmap;
	splx(s);
	sc->sc_chgind = iop->sc_chgind;
	return (0);
}

/*
 * Re-scan the bus; to be called from a higher level (e.g. scsipi).
 */
static int
iopsp_rescan(struct iopsp_softc *sc)
{
	struct iop_softc *iop;
	struct iop_msg *im;
	struct i2o_hba_bus_scan mf;
	int rv;

	iop = (struct iop_softc *)sc->sc_dv.dv_parent;

	rv = lockmgr(&iop->sc_conflock, LK_EXCLUSIVE, NULL);
	if (rv != 0) {
#ifdef I2ODEBUG
		printf("iopsp_rescan: unable to acquire lock\n");
#endif
		return (rv);
	}

	im = iop_msg_alloc(iop, IM_WAIT);

	mf.msgflags = I2O_MSGFLAGS(i2o_hba_bus_scan);
	mf.msgfunc = I2O_MSGFUNC(sc->sc_ii.ii_tid, I2O_HBA_BUS_SCAN);
	mf.msgictx = sc->sc_ii.ii_ictx;
	mf.msgtctx = im->im_tctx;

	rv = iop_msg_post(iop, im, &mf, 5*60*1000);
	iop_msg_free(iop, im);
	if (rv != 0)
		printf("%s: bus rescan failed (error %d)\n",
		    sc->sc_dv.dv_xname, rv);

	if ((rv = iop_lct_get(iop)) == 0)
		rv = iopsp_reconfig(&sc->sc_dv);

	lockmgr(&iop->sc_conflock, LK_RELEASE, NULL);
	return (rv);
}

/*
 * Start a SCSI command.
 */
static int
iopsp_scsi_cmd(struct scsipi_xfer *xs)
{
	struct scsipi_link *sc_link;
	struct iopsp_softc *sc;
	struct iop_msg *im;
	struct iop_softc *iop;
	struct i2o_scsi_scb_exec *mf;
	int error, flags, tid;
	u_int32_t mb[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];

	sc_link = (void *)xs->sc_link;
	flags = xs->xs_control;
	sc = sc_link->adapter_softc;
	iop = (struct iop_softc *)sc->sc_dv.dv_parent;

	SC_DEBUG(sc_link, SDEV_DB2, ("iopsp_scsi_cmd\n"));

	tid = IOPSP_TIDMAP(sc->sc_tidmap, sc_link->scsipi_scsi.target,
	    sc_link->scsipi_scsi.lun);
	if (tid == IOPSP_TID_ABSENT || tid == IOPSP_TID_INUSE) {
		xs->error = XS_SELTIMEOUT;
		return (COMPLETE);
	}

	/* Need to reset the target? */
	if ((flags & XS_CTL_RESET) != 0) {
		if (iop_simple_cmd(iop, tid, I2O_SCSI_DEVICE_RESET,
		    sc->sc_ii.ii_ictx, 1, 30*1000) != 0) {
#ifdef I2ODEBUG
			printf("%s: reset failed\n", sc->sc_dv.dv_xname);
#endif
			xs->error = XS_DRIVER_STUFFUP;
		} else
			xs->error = XS_NOERROR;

		scsipi_done(xs);
		return (COMPLETE);
	}

#if defined(I2ODEBUG) || defined(SCSIDEBUG)
	if (xs->cmdlen > sizeof(mf->cdb))
		panic("%s: CDB too large\n", sc->sc_dv.dv_xname);
#endif

	im = iop_msg_alloc(iop, IM_POLL_INTR |
	    IM_NOSTATUS | ((flags & XS_CTL_POLL) != 0 ? IM_POLL : 0));
	im->im_dvcontext = xs;

	mf = (struct i2o_scsi_scb_exec *)mb;
	mf->msgflags = I2O_MSGFLAGS(i2o_scsi_scb_exec);
	mf->msgfunc = I2O_MSGFUNC(tid, I2O_SCSI_SCB_EXEC);
	mf->msgictx = sc->sc_ii.ii_ictx;
	mf->msgtctx = im->im_tctx;
	mf->flags = xs->cmdlen | I2O_SCB_FLAG_ENABLE_DISCONNECT |
	    I2O_SCB_FLAG_SENSE_DATA_IN_MESSAGE;
	mf->datalen = xs->datalen;
	memcpy(mf->cdb, xs->cmd, xs->cmdlen);

	if (xs->bp != NULL) {
		if ((xs->bp->b_flags & (B_ASYNC | B_READ)) != 0)
			mf->flags |= I2O_SCB_FLAG_SIMPLE_QUEUE_TAG;
		else
			mf->flags |= I2O_SCB_FLAG_ORDERED_QUEUE_TAG;
	}

	if (xs->datalen != 0) {
		error = iop_msg_map_bio(iop, im, mb, xs->data,
		    xs->datalen, (flags & XS_CTL_DATA_OUT) == 0);
		if (error) {
			xs->error = XS_DRIVER_STUFFUP;
			iop_msg_free(iop, im);
			scsipi_done(xs);
			return (COMPLETE);
		}
	}

	if ((flags & XS_CTL_DATA_IN) == 0)
		mf->flags |= I2O_SCB_FLAG_XFER_TO_DEVICE;
	else
		mf->flags |= I2O_SCB_FLAG_XFER_FROM_DEVICE;

	if (iop_msg_post(iop, im, mb, xs->timeout)) {
		if (xs->datalen != 0)
			iop_msg_unmap(iop, im);
		iop_msg_free(iop, im);
		xs->error = XS_DRIVER_STUFFUP;
		scsipi_done(xs);
		return (COMPLETE);
	}

	return (SUCCESSFULLY_QUEUED);
}

#ifdef notyet
/*
 * Abort the specified I2O_SCSI_SCB_EXEC message and its associated SCB.
 */
static int
iopsp_scsi_abort(struct iopsp_softc *sc, int atid, struct iop_msg *aim)
{
	struct iop_msg *im;
	struct i2o_scsi_scb_abort mf;
	struct iop_softc *iop;
	int rv, s;

	iop = (struct iop_softc *)sc->sc_dv.dv_parent;
	im = iop_msg_alloc(iop, IM_POLL);

	mf.msgflags = I2O_MSGFLAGS(i2o_scsi_scb_abort);
	mf.msgfunc = I2O_MSGFUNC(atid, I2O_SCSI_SCB_ABORT);
	mf.msgictx = sc->sc_ii.ii_ictx;
	mf.msgtctx = im->im_tctx;
	mf.tctxabort = aim->im_tctx;

	s = splbio();
	rv = iop_msg_post(iop, im, &mf, 30000);
	splx(s);
	iop_msg_free(iop, im);
	return (rv);
}
#endif

/*
 * We have a message which has been processed and replied to by the IOP -
 * deal with it.
 */
static void
iopsp_intr(struct device *dv, struct iop_msg *im, void *reply)
{
	struct scsipi_xfer *xs;
	struct iopsp_softc *sc;
	struct i2o_scsi_reply *rb;
 	struct iop_softc *iop;
	u_int sl;

	sc = (struct iopsp_softc *)dv;
	xs = (struct scsipi_xfer *)im->im_dvcontext;
	iop = (struct iop_softc *)dv->dv_parent;
	rb = reply;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("iopsp_intr\n"));

	if ((rb->msgflags & I2O_MSGFLAGS_FAIL) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		xs->resid = xs->datalen;
	} else {
		if (rb->hbastatus != I2O_SCSI_DSC_SUCCESS) {
			switch (rb->hbastatus) {
			case I2O_SCSI_DSC_ADAPTER_BUSY:
			case I2O_SCSI_DSC_SCSI_BUS_RESET:
			case I2O_SCSI_DSC_BUS_BUSY:
				xs->error = XS_BUSY;
				break;
			case I2O_SCSI_DSC_SELECTION_TIMEOUT:
				xs->error = XS_SELTIMEOUT;
				break;
			case I2O_SCSI_DSC_COMMAND_TIMEOUT:
			case I2O_SCSI_DSC_DEVICE_NOT_PRESENT:
			case I2O_SCSI_DSC_LUN_INVALID:
			case I2O_SCSI_DSC_SCSI_TID_INVALID:
				xs->error = XS_TIMEOUT;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
			printf("%s: HBA status 0x%02x\n", sc->sc_dv.dv_xname,
			   rb->hbastatus);
		} else if (rb->scsistatus != SCSI_OK) {
			switch (rb->scsistatus) {
			case SCSI_CHECK:
				xs->error = XS_SENSE;
				sl = le32toh(rb->senselen);
				if (sl > sizeof(xs->sense.scsi_sense))
					sl = sizeof(xs->sense.scsi_sense);
				memcpy(&xs->sense.scsi_sense, rb->sense, sl);
				break;
			case SCSI_QUEUE_FULL:
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->error = XS_NOERROR;

		xs->resid = xs->datalen - le32toh(rb->datalen);
		xs->status = rb->scsistatus;
	}

	/* Free the message wrapper and pass the news to scsipi. */
	if (xs->datalen != 0)
		iop_msg_unmap(iop, im);
	iop_msg_free(iop, im);

	scsipi_done(xs);
}

/*
 * ioctl hook; used here only to initiate low-level rescans.
 */
static int
iopsp_ioctl(struct scsipi_link *sc_link, u_long cmd, caddr_t data, int flag,
	    struct proc *p)
{
	int rv;

	switch (cmd) {
	case SCBUSIOLLSCAN:
		/*
		 * If it's boot time, the bus will have been scanned and the
		 * maps built.  Locking would stop re-configuration, but we
		 * want to fake success.
		 */
		if (p != &proc0)
			rv = iopsp_rescan(sc_link->adapter_softc);
		else
			rv = 0;
		break;

	default:
		rv = ENOTTY;
		break;
	}

	return (rv);
}

/*
 * The number of openings available to us has changed, so inform scsipi.
 */
static void
iopsp_adjqparam(struct device *dv, int mpi)
{

	/* XXX */
}
