/*	$NetBSD: wd.c,v 1.212.2.7 2002/10/18 02:41:30 nathanw Exp $ */

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *  This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *	derived from this software without specific prior written permission.
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wd.c,v 1.212.2.7 2002/10/18 02:41:30 nathanw Exp $");

#ifndef WDCDEBUG
#define WDCDEBUG
#endif /* WDCDEBUG */

#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>
#include <dev/ic/wdcreg.h>
#include <sys/ataio.h>
#include "locators.h"

#define	WAITTIME	(4 * hz)	/* time to wait for a completion */
#define	WDIORETRIES_SINGLE 4	/* number of retries before single-sector */
#define	WDIORETRIES	5	/* number of retries before giving up */
#define	RECOVERYTIME hz/2	/* time to wait before retrying a cmd */

#define	WDUNIT(dev)		DISKUNIT(dev)
#define	WDPART(dev)		DISKPART(dev)
#define	WDMINOR(unit, part)	DISKMINOR(unit, part)
#define	MAKEWDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	WDLABELDEV(dev)	(MAKEWDDEV(major(dev), WDUNIT(dev), RAW_PART))

#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef WDCDEBUG
extern int wdcdebug_wd_mask; /* init'ed in ata_wdc.c */
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_wd_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

struct wd_softc {
	/* General disk infos */
	struct device sc_dev;
	struct disk sc_dk;
	struct bufq_state sc_q;
	struct callout sc_restart_ch;
	/* IDE disk soft states */
	struct ata_bio sc_wdc_bio; /* current transfer */
	struct buf *sc_bp; /* buf being transfered */
	void *wdc_softc;   /* pointer to our parent */
	struct ata_drive_datas *drvp; /* Our controller's infos */
	const struct ata_bustype *atabus;
	int openings;
	struct ataparams sc_params;/* drive characteistics found */
	int sc_flags;	  
#define	WDF_LOCKED	0x001
#define	WDF_WANTED	0x002
#define	WDF_WLABEL	0x004 /* label is writable */
#define	WDF_LABELLING	0x008 /* writing label */
/*
 * XXX Nothing resets this yet, but disk change sensing will when ATA-4 is
 * more fully implemented.
 */
#define WDF_LOADED	0x010 /* parameters loaded */
#define WDF_WAIT	0x020 /* waiting for resources */
#define WDF_LBA		0x040 /* using LBA mode */
#define WDF_KLABEL	0x080 /* retain label after 'full' close */
#define WDF_LBA48	0x100 /* using 48-bit LBA mode */
	int sc_capacity;
	int cyl; /* actual drive parameters */
	int heads;
	int sectors;
	int retries; /* number of xfer retry */

	void *sc_sdhook;		/* our shutdown hook */

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
};

#define sc_drive sc_wdc_bio.drive
#define sc_mode sc_wdc_bio.mode
#define sc_multi sc_wdc_bio.multi
#define sc_badsect sc_wdc_bio.badsect

int	wdprobe		__P((struct device *, struct cfdata *, void *));
void	wdattach	__P((struct device *, struct device *, void *));
int	wddetach __P((struct device *, int));
int	wdactivate __P((struct device *, enum devact));
int	wdprint	__P((void *, char *));
void	wdperror __P((const struct wd_softc *));

CFATTACH_DECL(wd, sizeof(struct wd_softc),
    wdprobe, wdattach, wddetach, wdactivate);

extern struct cfdriver wd_cd;

dev_type_open(wdopen);
dev_type_close(wdclose);
dev_type_read(wdread);
dev_type_write(wdwrite);
dev_type_ioctl(wdioctl);
dev_type_strategy(wdstrategy);
dev_type_dump(wddump);
dev_type_size(wdsize);

const struct bdevsw wd_bdevsw = {
	wdopen, wdclose, wdstrategy, wdioctl, wddump, wdsize, D_DISK
};

const struct cdevsw wd_cdevsw = {
	wdopen, wdclose, wdread, wdwrite, wdioctl,
	nostop, notty, nopoll, nommap, D_DISK
};

/*
 * Glue necessary to hook WDCIOCCOMMAND into physio
 */

struct wd_ioctl {
	LIST_ENTRY(wd_ioctl) wi_list;
	struct buf wi_bp;
	struct uio wi_uio;
	struct iovec wi_iov;
	atareq_t wi_atareq;
	struct wd_softc *wi_softc;
};

LIST_HEAD(, wd_ioctl) wi_head;

struct	wd_ioctl *wi_find __P((struct buf *));
void	wi_free __P((struct wd_ioctl *));
struct	wd_ioctl *wi_get __P((void));
void	wdioctlstrategy __P((struct buf *));

void  wdgetdefaultlabel __P((struct wd_softc *, struct disklabel *));
void  wdgetdisklabel	__P((struct wd_softc *));
void  wdstart	__P((void *));
void  __wdstart	__P((struct wd_softc*, struct buf *));
void  wdrestart __P((void*));
int   wd_get_params __P((struct wd_softc *, u_int8_t, struct ataparams *));
void  wd_flushcache __P((struct wd_softc *, int));
void  wd_shutdown __P((void*));

struct dkdriver wddkdriver = { wdstrategy };

#ifdef HAS_BAD144_HANDLING
static void bad144intern __P((struct wd_softc *));
#endif
int	wdlock	__P((struct wd_softc *));
void	wdunlock	__P((struct wd_softc *));

int
wdprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;

	void *aux;
{
	struct ata_device *adev = aux;

	if (adev == NULL)
		return 0;
	if (adev->adev_bustype->bustype_type != SCSIPI_BUSTYPE_ATA)
		return 0;

	if (match->cf_loc[ATACF_CHANNEL] != ATACF_CHANNEL_DEFAULT &&
	    match->cf_loc[ATACF_CHANNEL] != adev->adev_channel)
		return 0;

	if (match->cf_loc[ATACF_DRIVE] != ATACF_DRIVE_DEFAULT &&
	    match->cf_loc[ATACF_DRIVE] != adev->adev_drv_data->drive)
		return 0;
	return 1;
}

void
wdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wd_softc *wd = (void *)self;
	struct ata_device *adev= aux;
	int i, blank;
	char buf[41], pbuf[9], c, *p, *q;
	WDCDEBUG_PRINT(("wdattach\n"), DEBUG_FUNCS | DEBUG_PROBE);

	callout_init(&wd->sc_restart_ch);
	bufq_alloc(&wd->sc_q, BUFQ_DISKSORT|BUFQ_SORT_RAWBLOCK);

	wd->atabus = adev->adev_bustype;
	wd->openings = adev->adev_openings;
	wd->drvp = adev->adev_drv_data;;
	wd->wdc_softc = parent;
	/* give back our softc to our caller */
	wd->drvp->drv_softc = &wd->sc_dev;

	/* read our drive info */
	if (wd_get_params(wd, AT_POLL, &wd->sc_params) != 0) {
		printf("%s: IDENTIFY failed\n", wd->sc_dev.dv_xname);
		return;
	}

	for (blank = 0, p = wd->sc_params.atap_model, q = buf, i = 0;
	    i < sizeof(wd->sc_params.atap_model); i++) {
		c = *p++;
		if (c == '\0')
			break;
		if (c != ' ') {
			if (blank) {
				*q++ = ' ';
				blank = 0;
			}
			*q++ = c;
		} else
			blank = 1;
	}
	*q++ = '\0';

	printf(": <%s>\n", buf);

	if ((wd->sc_params.atap_multi & 0xff) > 1) {
		wd->sc_multi = wd->sc_params.atap_multi & 0xff;
	} else {
		wd->sc_multi = 1;
	}

	printf("%s: drive supports %d-sector PIO transfers,",
	    wd->sc_dev.dv_xname, wd->sc_multi);

	/* 48-bit LBA addressing */
	if ((wd->sc_params.atap_cmd2_en & WDC_CAP_LBA48) != 0)
		wd->sc_flags |= WDF_LBA48;

	/* Prior to ATA-4, LBA was optional. */
	if ((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0)
		wd->sc_flags |= WDF_LBA;
#if 0
	/* ATA-4 requires LBA. */
	if (wd->sc_params.atap_ataversion != 0xffff &&
	    wd->sc_params.atap_ataversion >= WDC_VER_ATA4)
		wd->sc_flags |= WDF_LBA;
#endif

	if ((wd->sc_flags & WDF_LBA48) != 0) {
		printf(" LBA48 addressing\n");
		wd->sc_capacity =
		    ((u_int64_t) wd->sc_params.__reserved6[11] << 48) |
		    ((u_int64_t) wd->sc_params.__reserved6[10] << 32) |
		    ((u_int64_t) wd->sc_params.__reserved6[9]  << 16) |
		    ((u_int64_t) wd->sc_params.__reserved6[8]  << 0);
	} else if ((wd->sc_flags & WDF_LBA) != 0) {
		printf(" LBA addressing\n");
		wd->sc_capacity =
		    (wd->sc_params.atap_capacity[1] << 16) |
		    wd->sc_params.atap_capacity[0];
	} else {
		printf(" chs addressing\n");
		wd->sc_capacity =
		    wd->sc_params.atap_cylinders *
		    wd->sc_params.atap_heads *
		    wd->sc_params.atap_sectors;
	}
	format_bytes(pbuf, sizeof(pbuf),
	    (u_int64_t)wd->sc_capacity * DEV_BSIZE);
	printf("%s: %s, %d cyl, %d head, %d sec, "
	    "%d bytes/sect x %d sectors\n",
	    self->dv_xname, pbuf, wd->sc_params.atap_cylinders,
	    wd->sc_params.atap_heads, wd->sc_params.atap_sectors,
	    DEV_BSIZE, wd->sc_capacity);

	WDCDEBUG_PRINT(("%s: atap_dmatiming_mimi=%d, atap_dmatiming_recom=%d\n",
	    self->dv_xname, wd->sc_params.atap_dmatiming_mimi,
	    wd->sc_params.atap_dmatiming_recom), DEBUG_PROBE);
	/*
	 * Initialize and attach the disk structure.
	 */
	wd->sc_dk.dk_driver = &wddkdriver;
	wd->sc_dk.dk_name = wd->sc_dev.dv_xname;
	disk_attach(&wd->sc_dk);
	wd->sc_wdc_bio.lp = wd->sc_dk.dk_label;
	wd->sc_sdhook = shutdownhook_establish(wd_shutdown, wd);
	if (wd->sc_sdhook == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    wd->sc_dev.dv_xname); 
#if NRND > 0
	rnd_attach_source(&wd->rnd_source, wd->sc_dev.dv_xname,
			  RND_TYPE_DISK, 0);
#endif
}

int
wdactivate(self, act)
	struct device *self;
	enum devact act;
{
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		/*
		 * Nothing to do; we key off the device's DVF_ACTIVATE.
		 */
		break;
	}
	return (rv);
}

int
wddetach(self, flags)
	struct device *self;
	int flags;
{
	struct wd_softc *sc = (struct wd_softc *)self;
	struct buf *bp;
	int s, bmaj, cmaj, i, mn;

	/* locate the major number */
	bmaj = bdevsw_lookup_major(&wd_bdevsw);
	cmaj = cdevsw_lookup_major(&wd_cdevsw);

	s = splbio();

	/* Kill off any queued buffers. */ 
	while ((bp = BUFQ_GET(&sc->sc_q)) != NULL) {
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR; 
		bp->b_resid = bp->b_bcount;
		biodone(bp);
	}

	bufq_free(&sc->sc_q);

	splx(s);

	/* Nuke the vnodes for any open instances. */
	for (i = 0; i < MAXPARTITIONS; i++) {
		mn = WDMINOR(self->dv_unit, i);
		vdevgone(bmaj, mn, mn, VBLK);
		vdevgone(cmaj, mn, mn, VCHR);
	}

	/* Detach disk. */
	disk_detach(&sc->sc_dk);

	/* Get rid of the shutdown hook. */
	if (sc->sc_sdhook != NULL)
		shutdownhook_disestablish(sc->sc_sdhook);

#if NRND > 0
	/* Unhook the entropy source. */
	rnd_detach_source(&sc->rnd_source);
#endif

	return (0);
}

/*
 * Read/write routine for a buffer.  Validates the arguments and schedules the
 * transfer.  Does not wait for the transfer to complete.
 */
void
wdstrategy(bp)
	struct buf *bp;
{
	struct wd_softc *wd = device_lookup(&wd_cd, WDUNIT(bp->b_dev));
	struct disklabel *lp = wd->sc_dk.dk_label;
	daddr_t blkno;
	int s;

	WDCDEBUG_PRINT(("wdstrategy (%s)\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);
	
	/* Valid request?  */
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % lp->d_secsize) != 0 ||
	    (bp->b_bcount / lp->d_secsize) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		goto bad;
	}
	
	/* If device invalidated (e.g. media change, door open), error. */
	if ((wd->sc_flags & WDF_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (WDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, wd->sc_dk.dk_label,
	    (wd->sc_flags & (WDF_WLABEL|WDF_LABELLING)) != 0) <= 0)
		goto done;

	/*
	 * Now convert the block number to absolute and put it in
	 * terms of the device's logical block size.
	 */
	if (lp->d_secsize >= DEV_BSIZE)
		blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE);
	else
		blkno = bp->b_blkno * (DEV_BSIZE / lp->d_secsize);

	if (WDPART(bp->b_dev) != RAW_PART)
		blkno += lp->d_partitions[WDPART(bp->b_dev)].p_offset;

	bp->b_rawblkno = blkno;

	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	BUFQ_PUT(&wd->sc_q, bp);
	wdstart(wd);
	splx(s);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Queue a drive for I/O.
 */
void
wdstart(arg)
	void *arg;
{
	struct wd_softc *wd = arg;
	struct buf *bp = NULL;

	WDCDEBUG_PRINT(("wdstart %s\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);
	while (wd->openings > 0) {

		/* Is there a buf for us ? */
		if ((bp = BUFQ_GET(&wd->sc_q)) == NULL)
			return;
	
		/* 
		 * Make the command. First lock the device
		 */
		wd->openings--;

		wd->retries = 0;
		__wdstart(wd, bp);
	}
}

void
__wdstart(wd, bp)
	struct wd_softc *wd;
	struct buf *bp;
{

	wd->sc_wdc_bio.blkno = bp->b_rawblkno;
	wd->sc_wdc_bio.blkdone =0;
	wd->sc_bp = bp;
	/*
	 * If we're retrying, retry in single-sector mode. This will give us
	 * the sector number of the problem, and will eventually allow the
	 * transfer to succeed.
	 */
	if (wd->sc_multi == 1 || wd->retries >= WDIORETRIES_SINGLE)
		wd->sc_wdc_bio.flags = ATA_SINGLE;
	else
		wd->sc_wdc_bio.flags = 0;
	if (wd->sc_flags & WDF_LBA48)
		wd->sc_wdc_bio.flags |= ATA_LBA48;
	if (wd->sc_flags & WDF_LBA)
		wd->sc_wdc_bio.flags |= ATA_LBA;
	if (bp->b_flags & B_READ)
		wd->sc_wdc_bio.flags |= ATA_READ;
	wd->sc_wdc_bio.bcount = bp->b_bcount;
	wd->sc_wdc_bio.databuf = bp->b_data;
	/* Instrumentation. */
	disk_busy(&wd->sc_dk);
	switch (wd->atabus->ata_bio(wd->drvp, &wd->sc_wdc_bio)) {
	case WDC_TRY_AGAIN:
		callout_reset(&wd->sc_restart_ch, hz, wdrestart, wd);
		break;
	case WDC_QUEUED:
	case WDC_COMPLETE:
		break;
	default:
		panic("__wdstart: bad return code from ata_bio()");
	}
}

void
wddone(v)
	void *v;
{
	struct wd_softc *wd = v;
	struct buf *bp = wd->sc_bp;
	const char *errmsg;
	int do_perror = 0;
	WDCDEBUG_PRINT(("wddone %s\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);

	if (bp == NULL)
		return;
	bp->b_resid = wd->sc_wdc_bio.bcount;
	switch (wd->sc_wdc_bio.error) {
	case ERR_DMA:
		errmsg = "DMA error";
		goto retry;
	case ERR_DF:
		errmsg = "device fault";
		goto retry;
	case TIMEOUT:
		errmsg = "device timeout";
		goto retry;
	case ERROR:
		/* Don't care about media change bits */
		if (wd->sc_wdc_bio.r_error != 0 &&
		    (wd->sc_wdc_bio.r_error & ~(WDCE_MC | WDCE_MCR)) == 0)
			goto noerror;
		errmsg = "error";
		do_perror = 1;
retry:		/* Just reset and retry. Can we do more ? */
		wd->atabus->ata_reset_channel(wd->drvp);
		diskerr(bp, "wd", errmsg, LOG_PRINTF,
		    wd->sc_wdc_bio.blkdone, wd->sc_dk.dk_label);
		if (wd->retries < WDIORETRIES)
			printf(", retrying\n");
		if (do_perror)
			wdperror(wd);
		if (wd->retries < WDIORETRIES) {
			wd->retries++;
			callout_reset(&wd->sc_restart_ch, RECOVERYTIME,
			    wdrestart, wd);
			return;
		}
		printf("\n");
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		break;
	case NOERROR:
noerror:	if ((wd->sc_wdc_bio.flags & ATA_CORR) || wd->retries > 0)
			printf("%s: soft error (corrected)\n",
			    wd->sc_dev.dv_xname);
		break;
	case ERR_NODEV:
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		break;
	}
	disk_unbusy(&wd->sc_dk, (bp->b_bcount - bp->b_resid));
#if NRND > 0
	rnd_add_uint32(&wd->rnd_source, bp->b_blkno);
#endif
	biodone(bp);
	wd->openings++;
	wdstart(wd);
}

void
wdrestart(v)
	void *v;
{
	struct wd_softc *wd = v;
	struct buf *bp = wd->sc_bp;
	int s;
	WDCDEBUG_PRINT(("wdrestart %s\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);

	s = splbio();
	__wdstart(v, bp);
	splx(s);
}

int
wdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	WDCDEBUG_PRINT(("wdread\n"), DEBUG_XFERS);
	return (physio(wdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
wdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	WDCDEBUG_PRINT(("wdwrite\n"), DEBUG_XFERS);
	return (physio(wdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
wdlock(wd)
	struct wd_softc *wd;
{
	int error;
	int s;

	WDCDEBUG_PRINT(("wdlock\n"), DEBUG_FUNCS);

	s = splbio();

	while ((wd->sc_flags & WDF_LOCKED) != 0) {
		wd->sc_flags |= WDF_WANTED;
		if ((error = tsleep(wd, PRIBIO | PCATCH,
		    "wdlck", 0)) != 0) {
			splx(s);
			return error;
		}
	}
	wd->sc_flags |= WDF_LOCKED;
	splx(s);
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
wdunlock(wd)
	struct wd_softc *wd;
{

	WDCDEBUG_PRINT(("wdunlock\n"), DEBUG_FUNCS);

	wd->sc_flags &= ~WDF_LOCKED;
	if ((wd->sc_flags & WDF_WANTED) != 0) {
		wd->sc_flags &= ~WDF_WANTED;
		wakeup(wd);
	}
}

int
wdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd;
	int part, error;

	WDCDEBUG_PRINT(("wdopen\n"), DEBUG_FUNCS);
	wd = device_lookup(&wd_cd, WDUNIT(dev));
	if (wd == NULL)
		return (ENXIO);

	/*
	 * If this is the first open of this device, add a reference
	 * to the adapter.
	 */
	if (wd->sc_dk.dk_openmask == 0 &&
	    (error = wd->atabus->ata_addref(wd->drvp)) != 0)
		return (error);

	if ((error = wdlock(wd)) != 0)
		goto bad4;

	if (wd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((wd->sc_flags & WDF_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		if ((wd->sc_flags & WDF_LOADED) == 0) {
			wd->sc_flags |= WDF_LOADED;

			/* Load the physical device parameters. */
			wd_get_params(wd, AT_WAIT, &wd->sc_params);

			/* Load the partition info if not already loaded. */
			wdgetdisklabel(wd);
		}
	}

	part = WDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= wd->sc_dk.dk_label->d_npartitions ||
	     wd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}
	
	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	wdunlock(wd);
	return 0;

bad:
	if (wd->sc_dk.dk_openmask == 0) {
	}

bad3:
	wdunlock(wd);
bad4:
	if (wd->sc_dk.dk_openmask == 0)
		wd->atabus->ata_delref(wd->drvp);
	return error;
}

int
wdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd = device_lookup(&wd_cd, WDUNIT(dev));
	int part = WDPART(dev);
	int error;
	
	WDCDEBUG_PRINT(("wdclose\n"), DEBUG_FUNCS);
	if ((error = wdlock(wd)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	if (wd->sc_dk.dk_openmask == 0) {
		wd_flushcache(wd, AT_WAIT);
		/* XXXX Must wait for I/O to complete! */

		if (! (wd->sc_flags & WDF_KLABEL))
			wd->sc_flags &= ~WDF_LOADED;

		wd->atabus->ata_delref(wd->drvp);
	}

	wdunlock(wd);
	return 0;
}

void
wdgetdefaultlabel(wd, lp)
	struct wd_softc *wd;
	struct disklabel *lp;
{

	WDCDEBUG_PRINT(("wdgetdefaultlabel\n"), DEBUG_FUNCS);
	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = wd->sc_params.atap_heads;
	lp->d_nsectors = wd->sc_params.atap_sectors;
	lp->d_ncylinders = wd->sc_params.atap_cylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	if (strcmp(wd->sc_params.atap_model, "ST506") == 0)
		lp->d_type = DTYPE_ST506;
	else
		lp->d_type = DTYPE_ESDI;

	strncpy(lp->d_typename, wd->sc_params.atap_model, 16);
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = wd->sc_capacity;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Fabricate a default disk label, and try to read the correct one.
 */
void
wdgetdisklabel(wd)
	struct wd_softc *wd;
{
	struct disklabel *lp = wd->sc_dk.dk_label;
	char *errstring;

	WDCDEBUG_PRINT(("wdgetdisklabel\n"), DEBUG_FUNCS);

	memset(wd->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	wdgetdefaultlabel(wd, lp);

	wd->sc_badsect[0] = -1;

	if (wd->drvp->state > RECAL)
		wd->drvp->drive_flags |= DRIVE_RESET;
	errstring = readdisklabel(MAKEWDDEV(0, wd->sc_dev.dv_unit, RAW_PART),
	    wdstrategy, lp, wd->sc_dk.dk_cpulabel);
	if (errstring) {
		/*
		 * This probably happened because the drive's default
		 * geometry doesn't match the DOS geometry.  We
		 * assume the DOS geometry is now in the label and try
		 * again.  XXX This is a kluge.
		 */
		if (wd->drvp->state > RECAL)
			wd->drvp->drive_flags |= DRIVE_RESET;
		errstring = readdisklabel(MAKEWDDEV(0, wd->sc_dev.dv_unit,
		    RAW_PART), wdstrategy, lp, wd->sc_dk.dk_cpulabel);
	}
	if (errstring) {
		printf("%s: %s\n", wd->sc_dev.dv_xname, errstring);
		return;
	}

	if (wd->drvp->state > RECAL)
		wd->drvp->drive_flags |= DRIVE_RESET;
#ifdef HAS_BAD144_HANDLING
	if ((lp->d_flags & D_BADSECT) != 0)
		bad144intern(wd);
#endif
}

void
wdperror(wd)
	const struct wd_softc *wd;
{
	static const char *const errstr0_3[] = {"address mark not found",
	    "track 0 not found", "aborted command", "media change requested",
	    "id not found", "media changed", "uncorrectable data error",
	    "bad block detected"};
	static const char *const errstr4_5[] = {
	    "obsolete (address mark not found)",
	    "no media/write protected", "aborted command",
	    "media change requested", "id not found", "media changed",
	    "uncorrectable data error", "interface CRC error"};
	const char *const *errstr;  
	int i;
	char *sep = "";

	const char *devname = wd->sc_dev.dv_xname;
	struct ata_drive_datas *drvp = wd->drvp;
	int errno = wd->sc_wdc_bio.r_error;

	if (drvp->ata_vers >= 4)
		errstr = errstr4_5;
	else
		errstr = errstr0_3;

	printf("%s: (", devname);

	if (errno == 0)
		printf("error not notified");

	for (i = 0; i < 8; i++) {
		if (errno & (1 << i)) {
			printf("%s%s", sep, errstr[i]);
			sep = ", ";
		}
	}
	printf(")\n");
}

int
wdioctl(dev, xfer, addr, flag, p)
	dev_t dev;
	u_long xfer;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct wd_softc *wd = device_lookup(&wd_cd, WDUNIT(dev));
	int error;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	WDCDEBUG_PRINT(("wdioctl\n"), DEBUG_FUNCS);

	if ((wd->sc_flags & WDF_LOADED) == 0)
		return EIO;

	switch (xfer) {
#ifdef HAS_BAD144_HANDLING
	case DIOCSBAD:
		if ((flag & FWRITE) == 0)
			return EBADF;
		wd->sc_dk.dk_cpulabel->bad = *(struct dkbad *)addr;
		wd->sc_dk.dk_label->d_flags |= D_BADSECT;
		bad144intern(wd);
		return 0;
#endif

	case DIOCGDINFO:
		*(struct disklabel *)addr = *(wd->sc_dk.dk_label);
		return 0;
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
		newlabel = *(wd->sc_dk.dk_label);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(addr, &newlabel, sizeof (struct olddisklabel));
		return 0;
#endif

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = wd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &wd->sc_dk.dk_label->d_partitions[WDPART(dev)];
		return 0;
	
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *lp;

#ifdef __HAVE_OLD_DISKLABEL
		if (xfer == ODIOCSDINFO || xfer == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, addr, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)addr;

		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = wdlock(wd)) != 0)
			return error;
		wd->sc_flags |= WDF_LABELLING;

		error = setdisklabel(wd->sc_dk.dk_label,
		    lp, /*wd->sc_dk.dk_openmask : */0,
		    wd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (wd->drvp->state > RECAL)
				wd->drvp->drive_flags |= DRIVE_RESET;
			if (xfer == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || xfer == ODIOCWDINFO
#endif
			    )
				error = writedisklabel(WDLABELDEV(dev),
				    wdstrategy, wd->sc_dk.dk_label,
				    wd->sc_dk.dk_cpulabel);
		}

		wd->sc_flags &= ~WDF_LABELLING;
		wdunlock(wd);
		return error;
	}

	case DIOCKLABEL:
		if (*(int *)addr)
			wd->sc_flags |= WDF_KLABEL;
		else
			wd->sc_flags &= ~WDF_KLABEL;
		return 0;
	
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			wd->sc_flags |= WDF_WLABEL;
		else
			wd->sc_flags &= ~WDF_WLABEL;
		return 0;

	case DIOCGDEFLABEL:
		wdgetdefaultlabel(wd, (struct disklabel *)addr);
		return 0;
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		wdgetdefaultlabel(wd, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(addr, &newlabel, sizeof (struct olddisklabel));
		return 0;
#endif

#ifdef notyet
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			return EBADF;
		{
		register struct format_op *fop;
		struct iovec aiov;
		struct uio auio;
	
		fop = (struct format_op *)addr;
		aiov.iov_base = fop->df_buf;
		aiov.iov_len = fop->df_count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = fop->df_count;
		auio.uio_segflg = 0;
		auio.uio_offset =
			fop->df_startblk * wd->sc_dk.dk_label->d_secsize;
		auio.uio_procp = p;
		error = physio(wdformat, NULL, dev, B_WRITE, minphys,
		    &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = wdc->sc_status;
		fop->df_reg[1] = wdc->sc_error;
		return error;
		}
#endif

	case ATAIOCCOMMAND:
		/*
		 * Make sure this command is (relatively) safe first
		 */
		if ((((atareq_t *) addr)->flags & ATACMD_READ) == 0 &&
		    (flag & FWRITE) == 0)
			return (EBADF);
		{
		struct wd_ioctl *wi;
		atareq_t *atareq = (atareq_t *) addr;
		int error;

		wi = wi_get();
		wi->wi_softc = wd;
		wi->wi_atareq = *atareq;

		if (atareq->datalen && atareq->flags &
		    (ATACMD_READ | ATACMD_WRITE)) {
			wi->wi_iov.iov_base = atareq->databuf;
			wi->wi_iov.iov_len = atareq->datalen;
			wi->wi_uio.uio_iov = &wi->wi_iov;
			wi->wi_uio.uio_iovcnt = 1;
			wi->wi_uio.uio_resid = atareq->datalen;
			wi->wi_uio.uio_offset = 0;
			wi->wi_uio.uio_segflg = UIO_USERSPACE;
			wi->wi_uio.uio_rw =
			    (atareq->flags & ATACMD_READ) ? B_READ : B_WRITE;
			wi->wi_uio.uio_procp = p;
			error = physio(wdioctlstrategy, &wi->wi_bp, dev,
			    (atareq->flags & ATACMD_READ) ? B_READ : B_WRITE,
			    minphys, &wi->wi_uio);
		} else {
			/* No need to call physio if we don't have any
			   user data */
			wi->wi_bp.b_flags = 0;
			wi->wi_bp.b_data = 0;
			wi->wi_bp.b_bcount = 0;
			wi->wi_bp.b_dev = 0;
			wi->wi_bp.b_proc = p;
			wdioctlstrategy(&wi->wi_bp);
			error = wi->wi_bp.b_error;
		}
		*atareq = wi->wi_atareq;
		wi_free(wi);
		return(error);
		}

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("wdioctl: impossible");
#endif
}

#ifdef B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return wdstrategy(bp);
}
#endif

int
wdsize(dev)
	dev_t dev;
{
	struct wd_softc *wd;
	int part, omask;
	int size;

	WDCDEBUG_PRINT(("wdsize\n"), DEBUG_FUNCS);

	wd = device_lookup(&wd_cd, WDUNIT(dev));
	if (wd == NULL)
		return (-1);

	part = WDPART(dev);
	omask = wd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && wdopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	if (wd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = wd->sc_dk.dk_label->d_partitions[part].p_size *
		    (wd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && wdclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}

/* #define WD_DUMP_NOT_TRUSTED if you just want to watch */
static int wddoingadump = 0;
static int wddumprecalibrated = 0;
static int wddumpmulti = 1;

/*
 * Dump core after a system crash.
 */
int
wddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	struct wd_softc *wd;	/* disk unit to do the I/O */
	struct disklabel *lp;   /* disk's disklabel */
	int part, err;
	int nblks;	/* total number of sectors left to write */

	/* Check if recursive dump; if so, punt. */
	if (wddoingadump)
		return EFAULT;
	wddoingadump = 1;

	wd = device_lookup(&wd_cd, WDUNIT(dev));
	if (wd == NULL)
		return (ENXIO);

	part = WDPART(dev);

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = wd->sc_dk.dk_label;
	if ((size % lp->d_secsize) != 0)
		return EFAULT;
	nblks = size / lp->d_secsize;
	blkno = blkno / (lp->d_secsize / DEV_BSIZE);

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + nblks) > lp->d_partitions[part].p_size))
		return EINVAL;  

	/* Offset block number to start of partition. */
	blkno += lp->d_partitions[part].p_offset;

	/* Recalibrate, if first dump transfer. */
	if (wddumprecalibrated == 0) {
		wddumpmulti = wd->sc_multi;
		wddumprecalibrated = 1;
		wd->drvp->state = RESET;
	}
  
	while (nblks > 0) {
again:
		wd->sc_bp = NULL;
		wd->sc_wdc_bio.blkno = blkno;
		wd->sc_wdc_bio.flags = ATA_POLL;
		if (wddumpmulti == 1)
			wd->sc_wdc_bio.flags |= ATA_SINGLE;
		if (wd->sc_flags & WDF_LBA48)
			wd->sc_wdc_bio.flags |= ATA_LBA48;
		if (wd->sc_flags & WDF_LBA)
			wd->sc_wdc_bio.flags |= ATA_LBA;
		wd->sc_wdc_bio.bcount =
			min(nblks, wddumpmulti) * lp->d_secsize;
		wd->sc_wdc_bio.databuf = va;
#ifndef WD_DUMP_NOT_TRUSTED
		switch (wd->atabus->ata_bio(wd->drvp, &wd->sc_wdc_bio)) {
		case WDC_TRY_AGAIN:
			panic("wddump: try again");
			break;
		case WDC_QUEUED:
			panic("wddump: polled command has been queued");
			break;
		case WDC_COMPLETE:
			break;
		}
		switch(wd->sc_wdc_bio.error) {
		case TIMEOUT:
			printf("wddump: device timed out");
			err = EIO;
			break;
		case ERR_DF:
			printf("wddump: drive fault");
			err = EIO;
			break;
		case ERR_DMA:
			printf("wddump: DMA error");
			err = EIO;
			break;
		case ERROR:
			printf("wddump: ");
			wdperror(wd);
			err = EIO;
			break;
		case NOERROR: 
			err = 0;
			break;
		default:
			panic("wddump: unknown error type");
		}
		if (err != 0) {
			if (wddumpmulti != 1) {
				wddumpmulti = 1; /* retry in single-sector */
				printf(", retrying\n");
				goto again;
			}
			printf("\n");
			return err;
		}
#else	/* WD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("wd%d: dump addr 0x%x, cylin %d, head %d, sector %d\n",
		    unit, va, cylin, head, sector);
		delay(500 * 1000);	/* half a second */
#endif

		/* update block count */
		nblks -= min(nblks, wddumpmulti);
		blkno += min(nblks, wddumpmulti);
		va += min(nblks, wddumpmulti) * lp->d_secsize;
	}

	wddoingadump = 0;
	return 0;
}

#ifdef HAS_BAD144_HANDLING
/*
 * Internalize the bad sector table.
 */
void
bad144intern(wd)
	struct wd_softc *wd;
{
	struct dkbad *bt = &wd->sc_dk.dk_cpulabel->bad;
	struct disklabel *lp = wd->sc_dk.dk_label;
	int i = 0;

	WDCDEBUG_PRINT(("bad144intern\n"), DEBUG_XFERS);

	for (; i < NBT_BAD; i++) {
		if (bt->bt_bad[i].bt_cyl == 0xffff)
			break;
		wd->sc_badsect[i] =
		    bt->bt_bad[i].bt_cyl * lp->d_secpercyl +
		    (bt->bt_bad[i].bt_trksec >> 8) * lp->d_nsectors +
		    (bt->bt_bad[i].bt_trksec & 0xff);
	}
	for (; i < NBT_BAD+1; i++)
		wd->sc_badsect[i] = -1;
}
#endif

int
wd_get_params(wd, flags, params)
	struct wd_softc *wd;
	u_int8_t flags;
	struct ataparams *params;
{
	switch (wd->atabus->ata_get_params(wd->drvp, flags, params)) {
	case CMD_AGAIN:
		return 1;
	case CMD_ERR:
		/*
		 * We `know' there's a drive here; just assume it's old.
		 * This geometry is only used to read the MBR and print a
		 * (false) attach message.
		 */
		strncpy(params->atap_model, "ST506",
		    sizeof params->atap_model);
		params->atap_config = ATA_CFG_FIXED;
		params->atap_cylinders = 1024;
		params->atap_heads = 8;
		params->atap_sectors = 17;
		params->atap_multi = 1;
		params->atap_capabilities1 = params->atap_capabilities2 = 0;
		wd->drvp->ata_vers = -1; /* Mark it as pre-ATA */
		return 0;
	case CMD_OK:
		return 0;
	default:
		panic("wd_get_params: bad return code from ata_get_params");
		/* NOTREACHED */
	}
}

void
wd_flushcache(wd, flags)
	struct wd_softc *wd;
	int flags;
{
	struct wdc_command wdc_c;

	if (wd->drvp->ata_vers < 4) /* WDCC_FLUSHCACHE is here since ATA-4 */
		return;
	memset(&wdc_c, 0, sizeof(struct wdc_command));
	wdc_c.r_command = WDCC_FLUSHCACHE;
	wdc_c.r_st_bmask = WDCS_DRDY;
	wdc_c.r_st_pmask = WDCS_DRDY;
	wdc_c.flags = flags;
	wdc_c.timeout = 30000; /* 30s timeout */
	if (wd->atabus->ata_exec_command(wd->drvp, &wdc_c) != WDC_COMPLETE) {
		printf("%s: flush cache command didn't complete\n",
		    wd->sc_dev.dv_xname);
	}
	if (wdc_c.flags & AT_TIMEOU) {
		printf("%s: flush cache command timeout\n",
		    wd->sc_dev.dv_xname);
	}
	if (wdc_c.flags & AT_DF) {
		printf("%s: flush cache command: drive fault\n",
		    wd->sc_dev.dv_xname);
	}
	/*
	 * Ignore error register, it shouldn't report anything else
	 * than COMMAND ABORTED, which means the device doesn't support
	 * flush cache
	 */
}

void
wd_shutdown(arg)
	void *arg;
{
	struct wd_softc *wd = arg;
	wd_flushcache(wd, AT_POLL);
}

/*
 * Allocate space for a ioctl queue structure.  Mostly taken from
 * scsipi_ioctl.c
 */
struct wd_ioctl *
wi_get()
{
	struct wd_ioctl *wi;
	int s;

	wi = malloc(sizeof(struct wd_ioctl), M_TEMP, M_WAITOK|M_ZERO);
	s = splbio();
	LIST_INSERT_HEAD(&wi_head, wi, wi_list);
	splx(s);
	return (wi);
}

/*
 * Free an ioctl structure and remove it from our list
 */

void
wi_free(wi)
	struct wd_ioctl *wi;
{
	int s;

	s = splbio();
	LIST_REMOVE(wi, wi_list);
	splx(s);
	free(wi, M_TEMP);
}

/*
 * Find a wd_ioctl structure based on the struct buf.
 */

struct wd_ioctl *
wi_find(bp)
	struct buf *bp;
{
	struct wd_ioctl *wi;
	int s;

	s = splbio();
	for (wi = wi_head.lh_first; wi != 0; wi = wi->wi_list.le_next)
		if (bp == &wi->wi_bp)
			break;
	splx(s);
	return (wi);
}

/*
 * Ioctl pseudo strategy routine
 *
 * This is mostly stolen from scsipi_ioctl.c:scsistrategy().  What
 * happens here is:
 *
 * - wdioctl() queues a wd_ioctl structure.
 *
 * - wdioctl() calls physio/wdioctlstrategy based on whether or not
 *   user space I/O is required.  If physio() is called, physio() eventually
 *   calls wdioctlstrategy().
 *
 * - In either case, wdioctlstrategy() calls wd->atabus->ata_exec_command()
 *   to perform the actual command
 *
 * The reason for the use of the pseudo strategy routine is because
 * when doing I/O to/from user space, physio _really_ wants to be in
 * the loop.  We could put the entire buffer into the ioctl request
 * structure, but that won't scale if we want to do things like download
 * microcode.
 */

void
wdioctlstrategy(bp)
	struct buf *bp;
{
	struct wd_ioctl *wi;
	struct wdc_command wdc_c;
	int error = 0;

	wi = wi_find(bp);
	if (wi == NULL) {
		printf("user_strat: No ioctl\n");
		error = EINVAL;
		goto bad;
	}

	memset(&wdc_c, 0, sizeof(wdc_c));

	/*
	 * Abort if physio broke up the transfer
	 */

	if (bp->b_bcount != wi->wi_atareq.datalen) {
		printf("physio split wd ioctl request... cannot proceed\n");
		error = EIO;
		goto bad;
	}

	/*
	 * Abort if we didn't get a buffer size that was a multiple of
	 * our sector size (or was larger than NBBY)
	 */

	if ((bp->b_bcount % wi->wi_softc->sc_dk.dk_label->d_secsize) != 0 ||
	    (bp->b_bcount / wi->wi_softc->sc_dk.dk_label->d_secsize) >=
	     (1 << NBBY)) {
		error = EINVAL;
		goto bad;
	}

	/*
	 * Make sure a timeout was supplied in the ioctl request
	 */

	if (wi->wi_atareq.timeout == 0) {
		error = EINVAL;
		goto bad;
	}

	if (wi->wi_atareq.flags & ATACMD_READ)
		wdc_c.flags |= AT_READ;
	else if (wi->wi_atareq.flags & ATACMD_WRITE)
		wdc_c.flags |= AT_WRITE;

	if (wi->wi_atareq.flags & ATACMD_READREG)
		wdc_c.flags |= AT_READREG;

	wdc_c.flags |= AT_WAIT;

	wdc_c.timeout = wi->wi_atareq.timeout;
	wdc_c.r_command = wi->wi_atareq.command;
	wdc_c.r_head = wi->wi_atareq.head & 0x0f;
	wdc_c.r_cyl = wi->wi_atareq.cylinder;
	wdc_c.r_sector = wi->wi_atareq.sec_num;
	wdc_c.r_count = wi->wi_atareq.sec_count;
	wdc_c.r_precomp = wi->wi_atareq.features;
	wdc_c.r_st_bmask = WDCS_DRDY;
	wdc_c.r_st_pmask = WDCS_DRDY;
	wdc_c.data = wi->wi_bp.b_data;
	wdc_c.bcount = wi->wi_bp.b_bcount;

	if (wi->wi_softc->atabus->ata_exec_command(wi->wi_softc->drvp, &wdc_c)
	    != WDC_COMPLETE) {
		wi->wi_atareq.retsts = ATACMD_ERROR;
		goto bad;
	}

	if (wdc_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		if (wdc_c.flags & AT_ERROR) {
			wi->wi_atareq.retsts = ATACMD_ERROR;
			wi->wi_atareq.error = wdc_c.r_error;
		} else if (wdc_c.flags & AT_DF)
			wi->wi_atareq.retsts = ATACMD_DF;
		else
			wi->wi_atareq.retsts = ATACMD_TIMEOUT;
	} else {
		wi->wi_atareq.retsts = ATACMD_OK;
		if (wi->wi_atareq.flags & ATACMD_READREG) {
			wi->wi_atareq.head = wdc_c.r_head ;
			wi->wi_atareq.cylinder = wdc_c.r_cyl;
			wi->wi_atareq.sec_num = wdc_c.r_sector;
			wi->wi_atareq.sec_count = wdc_c.r_count; 
			wi->wi_atareq.features = wdc_c.r_precomp; 
			wi->wi_atareq.error = wdc_c.r_error; 
		}
	}

	bp->b_error = 0;
	biodone(bp);
	return;
bad:
	bp->b_flags |= B_ERROR;
	bp->b_error = error;
	biodone(bp);
}
