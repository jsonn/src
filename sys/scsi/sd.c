/*	$NetBSD: sd.c,v 1.114.2.1 1997/08/23 07:14:29 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1997 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Originally written by Julian Elischer (julian@dialix.oz.au)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@dialix.oz.au) Sept 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#define	SDOUTSTANDING	4
#define	SDRETRIES	4

#define	SDUNIT(dev)			DISKUNIT(dev)
#define	SDPART(dev)			DISKPART(dev)
#define	MAKESDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	SDLABELDEV(dev)	(MAKESDDEV(major(dev), SDUNIT(dev), RAW_PART))

struct sd_softc {
	struct device sc_dev;
	struct disk sc_dk;

	int flags;
#define	SDF_LOCKED	0x01
#define	SDF_WANTED	0x02
#define	SDF_WLABEL	0x04		/* label is writable */
#define	SDF_LABELLING	0x08		/* writing label */
#define	SDF_ANCIENT	0x10		/* disk is ancient; for minphys */
	struct scsi_link *sc_link;	/* contains our targ, lun, etc. */
	struct disk_parms {
		u_char heads;		/* number of heads */
		u_short cyls;		/* number of cylinders */
		u_char sectors;		/* number of sectors/track */
		int blksize;		/* number of bytes/sector */
		u_long disksize;	/* total number sectors */
	} params;
	struct buf buf_queue;
	u_int8_t type;
};

struct scsi_mode_sense_data {
	struct scsi_mode_header header;
	struct scsi_blk_desc blk_desc;
	union disk_pages pages;
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	sdmatch __P((struct device *, void *, void *));
#else
int	sdmatch __P((struct device *, struct cfdata *, void *));
#endif
void	sdattach __P((struct device *, struct device *, void *));
int	sdlock __P((struct sd_softc *));
void	sdunlock __P((struct sd_softc *));
void	sdminphys __P((struct buf *));
void	sdgetdisklabel __P((struct sd_softc *));
void	sdstart __P((void *));
void	sddone __P((struct scsi_xfer *));
int	sd_reassign_blocks __P((struct sd_softc *, u_long));
int	sd_get_optparms __P((struct sd_softc *, int, struct disk_parms *));
int	sd_get_parms __P((struct sd_softc *, int));
static int sd_mode_sense __P((struct sd_softc *, struct scsi_mode_sense_data *,
    int, int));

struct cfattach sd_ca = {
	sizeof(struct sd_softc), sdmatch, sdattach
};

struct cfdriver sd_cd = {
	NULL, "sd", DV_DISK
};

struct dkdriver sddkdriver = { sdstrategy };

struct scsi_device sd_switch = {
	NULL,			/* Use default error handler */
	sdstart,		/* have a queue, served by this */
	NULL,			/* have no async handler */
	sddone,			/* deal with stats at interrupt time */
};

struct scsi_inquiry_pattern sd_patterns[] = {
	{T_DIRECT, T_FIXED,
	 "",         "",                 ""},
	{T_DIRECT, T_REMOV,
	 "",         "",                 ""},
	{T_OPTICAL, T_FIXED,
	 "",         "",                 ""},
	{T_OPTICAL, T_REMOV,
	 "",         "",                 ""},
};

int
sdmatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)sd_patterns, sizeof(sd_patterns)/sizeof(sd_patterns[0]),
	    sizeof(sd_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
void
sdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	int error;
	struct sd_softc *sd = (void *)self;
	struct disk_parms *dp = &sd->params;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	sd->sc_link = sc_link;
	sd->type = (sa->sa_inqbuf->device & SID_TYPE);
	sc_link->device = &sd_switch;
	sc_link->device_softc = sd;
	if (sc_link->openings > SDOUTSTANDING)
		sc_link->openings = SDOUTSTANDING;

	/*
	 * Initialize and attach the disk structure.
	 */
	sd->sc_dk.dk_driver = &sddkdriver;
	sd->sc_dk.dk_name = sd->sc_dev.dv_xname;
	disk_attach(&sd->sc_dk);

#if !defined(i386)
	dk_establish(&sd->sc_dk, &sd->sc_dev);		/* XXX */
#endif

	/*
	 * Note if this device is ancient.  This is used in sdminphys().
	 */
	if ((sa->sa_inqbuf->version & SID_ANSII) == 0)
		sd->flags |= SDF_ANCIENT;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	printf("\n");
	printf("%s: ", sd->sc_dev.dv_xname);

	if ((sd->sc_link->quirks & SDEV_NOSTARTUNIT) == 0) {
		error = scsi_start(sd->sc_link, SSS_START,
				   SCSI_AUTOCONF | SCSI_IGNORE_ILLEGAL_REQUEST |
				   SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);
	} else
		error = 0;

	if (error || sd_get_parms(sd, SCSI_AUTOCONF) != 0)
		printf("drive offline\n");
	else
	        printf("%ldMB, %d cyl, %d head, %d sec, %d bytes/sect x %ld sectors\n",
		    dp->disksize / (1048576 / dp->blksize), dp->cyls,
		    dp->heads, dp->sectors, dp->blksize, dp->disksize);
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
sdlock(sd)
	struct sd_softc *sd;
{
	int error;

	while ((sd->flags & SDF_LOCKED) != 0) {
		sd->flags |= SDF_WANTED;
		if ((error = tsleep(sd, PRIBIO | PCATCH, "sdlck", 0)) != 0)
			return error;
	}
	sd->flags |= SDF_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
sdunlock(sd)
	struct sd_softc *sd;
{

	sd->flags &= ~SDF_LOCKED;
	if ((sd->flags & SDF_WANTED) != 0) {
		sd->flags &= ~SDF_WANTED;
		wakeup(sd);
	}
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int
sdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd;
	struct scsi_link *sc_link;
	int unit, part;
	int error;

	unit = SDUNIT(dev);
	if (unit >= sd_cd.cd_ndevs)
		return ENXIO;
	sd = sd_cd.cd_devs[unit];
	if (sd == NULL)
		return ENXIO;

	sc_link = sd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("sdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    sd_cd.cd_ndevs, SDPART(dev)));

	if ((error = sdlock(sd)) != 0)
		return error;

	if (sd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		/* Check that it is still responding and ok. */
		error = scsi_test_unit_ready(sc_link,
					     SCSI_IGNORE_ILLEGAL_REQUEST |
					     SCSI_IGNORE_MEDIA_CHANGE |
					     SCSI_IGNORE_NOT_READY);
		if (error)
			goto bad3;

		/* Start the pack spinning if necessary. */
		if ((sc_link->quirks & SDEV_NOSTARTUNIT) == 0) {
			error = scsi_start(sc_link, SSS_START,
					   SCSI_IGNORE_ILLEGAL_REQUEST |
					   SCSI_IGNORE_MEDIA_CHANGE |
					   SCSI_SILENT);
			if (error)
				goto bad3;
		}

		sc_link->flags |= SDEV_OPEN;

		/* Lock the pack in. */
		error = scsi_prevent(sc_link, PR_PREVENT,
				     SCSI_IGNORE_ILLEGAL_REQUEST |
				     SCSI_IGNORE_MEDIA_CHANGE);
		if (error)
			goto bad;

		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			sc_link->flags |= SDEV_MEDIA_LOADED;

			/* Load the physical device parameters. */
			if (sd_get_parms(sd, 0) != 0) {
				error = ENXIO;
				goto bad2;
			}
			SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

			/* Load the partition info if not already loaded. */
			sdgetdisklabel(sd);
			SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel loaded "));
		}
	}

	part = SDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sd->sc_dk.dk_label->d_npartitions ||
	     sd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sdunlock(sd);
	return 0;

bad2:
	sc_link->flags &= ~SDEV_MEDIA_LOADED;

bad:
	if (sd->sc_dk.dk_openmask == 0) {
		scsi_prevent(sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		sc_link->flags &= ~SDEV_OPEN;
	}

bad3:
	sdunlock(sd);
	return error;
}

/*
 * close the device.. only called if we are the LAST occurence of an open
 * device.  Convenient now but usually a pain.
 */
int 
sdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(dev)];
	int part = SDPART(dev);
	int error;

	if ((error = sdlock(sd)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	if (sd->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

		scsi_prevent(sd->sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
		sd->sc_link->flags &= ~(SDEV_OPEN|SDEV_MEDIA_LOADED);
	}

	sdunlock(sd);
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
sdstrategy(bp)
	struct buf *bp;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(bp->b_dev)];
	int s;

	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdstrategy "));
	SC_DEBUG(sd->sc_link, SDEV_DB1,
	    ("%ld bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));
	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % sd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * If the device has been made invalid, error out
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (SDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, sd->sc_dk.dk_label,
	    (sd->flags & (SDF_WLABEL|SDF_LABELLING)) != 0) <= 0)
		goto done;

	s = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(&sd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	sdstart(sd);

	splx(s);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * sdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (sdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * sdstart() is called at splbio from sdstrategy and scsi_done
 */
void 
sdstart(v)
	register void *v;
{
	register struct sd_softc *sd = v;
	register struct	scsi_link *sc_link = sd->sc_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd_big;
	struct scsi_rw cmd_small;
	struct scsi_generic *cmdp;
	int blkno, nblks, cmdlen, error;
	struct partition *p;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdstart "));
	/*
	 * Check if the device has room for another command
	 */
	while (sc_link->openings > 0) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (sc_link->flags & SDEV_WAITING) {
			sc_link->flags &= ~SDEV_WAITING;
			wakeup((caddr_t)sc_link);
			return;
		}

		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &sd->buf_queue;
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;
		dp->b_actf = bp->b_actf;

		/*
		 * If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-opened
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			continue;
		}

		/*
		 * We have a buf, now we should make a command
		 *
		 * First, translate the block to absolute and put it in terms
		 * of the logical blocksize of the device.
		 */
		blkno =
		    bp->b_blkno / (sd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
		if (SDPART(bp->b_dev) != RAW_PART) {
		     p = &sd->sc_dk.dk_label->d_partitions[SDPART(bp->b_dev)];
		     blkno += p->p_offset;
		}
		nblks = howmany(bp->b_bcount, sd->sc_dk.dk_label->d_secsize);

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (((blkno & 0x1fffff) == blkno) &&
		    ((nblks & 0xff) == nblks)) {
			/*
			 * We can fit in a small cdb.
			 */
			bzero(&cmd_small, sizeof(cmd_small));
			cmd_small.opcode = (bp->b_flags & B_READ) ?
			    READ_COMMAND : WRITE_COMMAND;
			_lto3b(blkno, cmd_small.addr);
			cmd_small.length = nblks & 0xff;
			cmdlen = sizeof(cmd_small);
			cmdp = (struct scsi_generic *)&cmd_small;
		} else {
			/*
			 * Need a large cdb.
			 */
			bzero(&cmd_big, sizeof(cmd_big));
			cmd_big.opcode = (bp->b_flags & B_READ) ?
			    READ_BIG : WRITE_BIG;
			_lto4b(blkno, cmd_big.addr);
			_lto2b(nblks, cmd_big.length);
			cmdlen = sizeof(cmd_big);
			cmdp = (struct scsi_generic *)&cmd_big;
		}

		/* Instrumentation. */
		disk_busy(&sd->sc_dk);

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		error = scsi_scsi_cmd(sc_link, cmdp, cmdlen,
		    (u_char *)bp->b_data, bp->b_bcount,
		    SDRETRIES, 60000, bp, SCSI_NOSLEEP |
		    ((bp->b_flags & B_READ) ? SCSI_DATA_IN : SCSI_DATA_OUT));
		if (error)
			printf("%s: not queued, error %d\n",
			    sd->sc_dev.dv_xname, error);
	}
}

void
sddone(xs)
	struct scsi_xfer *xs;
{
	struct sd_softc *sd = xs->sc_link->device_softc;

	if (xs->bp != NULL)
		disk_unbusy(&sd->sc_dk, xs->bp->b_bcount - xs->bp->b_resid);
}

void
sdminphys(bp)
	struct buf *bp;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(bp->b_dev)];
	long max;

	/*
	 * If the device is ancient, we want to make sure that
	 * the transfer fits into a 6-byte cdb.
	 *
	 * XXX Note that the SCSI-I spec says that 256-block transfers
	 * are allowed in a 6-byte read/write, and are specified
	 * by settng the "length" to 0.  However, we're conservative
	 * here, allowing only 255-block transfers in case an
	 * ancient device gets confused by length == 0.  A length of 0
	 * in a 10-byte read/write actually means 0 blocks.
	 */
	if (sd->flags & SDF_ANCIENT) {
		max = sd->sc_dk.dk_label->d_secsize * 0xff;

		if (bp->b_bcount > max)
			bp->b_bcount = max;
	}

	(*sd->sc_link->adapter->scsi_minphys)(bp);
}

int
sdread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_READ, sdminphys, uio));
}

int
sdwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_WRITE, sdminphys, uio));
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
int
sdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct sd_softc *sd = sd_cd.cd_devs[SDUNIT(dev)];
	int error;

	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0)
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *(sd->sc_dk.dk_label);
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = sd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &sd->sc_dk.dk_label->d_partitions[SDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = sdlock(sd)) != 0)
			return error;
		sd->flags |= SDF_LABELLING;

		error = setdisklabel(sd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*sd->sc_dk.dk_openmask : */0,
		    sd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(SDLABELDEV(dev),
				    sdstrategy, sd->sc_dk.dk_label,
				    sd->sc_dk.dk_cpulabel);
		}

		sd->flags &= ~SDF_LABELLING;
		sdunlock(sd);
		return error;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			sd->flags |= SDF_WLABEL;
		else
			sd->flags &= ~SDF_WLABEL;
		return 0;

	case DIOCLOCK:
		return scsi_prevent(sd->sc_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW, 0);

	case DIOCEJECT:
		return ((sd->sc_link->flags & SDEV_REMOVABLE) == 0 ? ENOTTY :
		    scsi_start(sd->sc_link, SSS_STOP|SSS_LOEJ, 0));

	default:
		if (SDPART(dev) != RAW_PART)
			return ENOTTY;
		return scsi_do_ioctl(sd->sc_link, dev, cmd, addr, flag, p);
	}

#ifdef DIAGNOSTIC
	panic("sdioctl: impossible");
#endif
}

/*
 * Load the label information on the named device
 */
void
sdgetdisklabel(sd)
	struct sd_softc *sd;
{
	struct disklabel *lp = sd->sc_dk.dk_label;
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(sd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = sd->params.blksize;
	lp->d_ntracks = sd->params.heads;
	lp->d_nsectors = sd->params.sectors;
	lp->d_ncylinders = sd->params.cyls;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	if (sd->type == T_OPTICAL)
		strncpy(lp->d_typename, "SCSI optical", 16);
	else
		strncpy(lp->d_typename, "SCSI disk", 16);
	lp->d_type = DTYPE_SCSI;
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = sd->params.disksize;
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

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(MAKESDDEV(0, sd->sc_dev.dv_unit, RAW_PART),
				  sdstrategy, lp, sd->sc_dk.dk_cpulabel);
	if (errstring) {
		printf("%s: %s\n", sd->sc_dev.dv_xname, errstring);
		return;
	}
}

/*
 * Tell the device to map out a defective block
 */
int
sd_reassign_blocks(sd, blkno)
	struct sd_softc *sd;
	u_long blkno;
{
	struct scsi_reassign_blocks scsi_cmd;
	struct scsi_reassign_blocks_data rbdata;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.opcode = REASSIGN_BLOCKS;

	_lto2b(sizeof(rbdata.defect_descriptor[0]), rbdata.length);
	_lto4b(blkno, rbdata.defect_descriptor[0].dlbaddr);

	return scsi_scsi_cmd(sd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&rbdata, sizeof(rbdata), SDRETRIES,
	    5000, NULL, SCSI_DATA_OUT);
}



static int
sd_mode_sense(sd, scsi_sense, page, flags)
	struct sd_softc *sd;
	struct scsi_mode_sense_data *scsi_sense;
	int page, flags;
{
	struct scsi_mode_sense scsi_cmd;

	/*
	 * Make sure the sense buffer is clean before we do
	 * the mode sense, so that checks for bogus values of
	 * 0 will work in case the mode sense fails.
	 */
	bzero(scsi_sense, sizeof(*scsi_sense));

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = 0x20;
	/*
	 * If the command worked, use the results to fill out
	 * the parameter structure
	 */
	return scsi_scsi_cmd(sd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)scsi_sense, sizeof(*scsi_sense),
	    SDRETRIES, 6000, NULL, flags | SCSI_DATA_IN | SCSI_SILENT);
}

int
sd_get_optparms(sd, flags, dp)
	struct sd_softc *sd;
	int flags;
	struct disk_parms *dp;
{
	struct scsi_mode_sense scsi_cmd;
	struct scsi_mode_sense_data {
		struct scsi_mode_header header;
		struct scsi_blk_desc blk_desc;
		union disk_pages pages;
	} scsi_sense;
	u_long sectors;
	int error;

	dp->blksize = 512;
	if ((sectors = scsi_size(sd->sc_link, flags)) == 0)
		return 1;

	/* XXX
	 * It is better to get the following params from the
	 * mode sense page 6 only (optical device parameter page).
	 * However, there are stupid optical devices which does NOT
	 * support the page 6. Ghaa....
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.page = 0x3f;	/* all pages */
	scsi_cmd.length = sizeof(struct scsi_mode_header) +
	    sizeof(struct scsi_blk_desc);

	if ((error = scsi_scsi_cmd(sd->sc_link,  
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),  
	    (u_char *)&scsi_sense, sizeof(scsi_sense), SDRETRIES,
	    6000, NULL, flags | SCSI_DATA_IN)) != 0)
		return error;

	dp->blksize = _3btol(scsi_sense.blk_desc.blklen);
	if (dp->blksize == 0) 
		dp->blksize = 512;

	/*
	 * Create a pseudo-geometry.
	 */
	dp->heads = 64;
	dp->sectors = 32;
	dp->cyls = sectors / (dp->heads * dp->sectors);
	dp->disksize = sectors;

	return 0;
}

/*
 * Get the scsi driver to send a full inquiry to the * device and use the
 * results to fill out the disk parameter structure.
 */
int
sd_get_parms(sd, flags)
	struct sd_softc *sd;
	int flags;
{
	struct disk_parms *dp = &sd->params;
	struct scsi_mode_sense_data scsi_sense;
	u_long sectors;
	int page;
	int error;

	if (sd->type == T_OPTICAL) {
		if ((error = sd_get_optparms(sd, flags, dp)) != 0)
			sd->sc_link->flags &= ~SDEV_MEDIA_LOADED;
		return error;
	}

	if ((error = sd_mode_sense(sd, &scsi_sense, page = 4, flags)) == 0) {
		SC_DEBUG(sd->sc_link, SDEV_DB3,
		    ("%d cyls, %d heads, %d precomp, %d red_write, %d land_zone\n",
		    _3btol(scsi_sense.pages.rigid_geometry.ncyl),
		    scsi_sense.pages.rigid_geometry.nheads,
		    _2btol(scsi_sense.pages.rigid_geometry.st_cyl_wp),
		    _2btol(scsi_sense.pages.rigid_geometry.st_cyl_rwc),
		    _2btol(scsi_sense.pages.rigid_geometry.land_zone)));

		/*
		 * KLUDGE!! (for zone recorded disks)
		 * give a number of sectors so that sec * trks * cyls
		 * is <= disk_size
		 * can lead to wasted space! THINK ABOUT THIS !
		 */
		dp->heads = scsi_sense.pages.rigid_geometry.nheads;
		dp->cyls = _3btol(scsi_sense.pages.rigid_geometry.ncyl);
		dp->blksize = _3btol(scsi_sense.blk_desc.blklen);

		if (dp->heads == 0 || dp->cyls == 0)
			goto fake_it;

		if (dp->blksize == 0)
			dp->blksize = 512;

		sectors = scsi_size(sd->sc_link, flags);
		dp->disksize = sectors;
		sectors /= (dp->heads * dp->cyls);
		dp->sectors = sectors;	/* XXX dubious on SCSI */

		return 0;
	}

	if ((error = sd_mode_sense(sd, &scsi_sense, page = 5, flags)) == 0) {
		dp->heads = scsi_sense.pages.flex_geometry.nheads;
		dp->cyls = _2btol(scsi_sense.pages.flex_geometry.ncyl);
		dp->blksize = _3btol(scsi_sense.blk_desc.blklen);
		dp->sectors = scsi_sense.pages.flex_geometry.ph_sec_tr;
		dp->disksize = dp->heads * dp->cyls * dp->sectors;
		if (dp->disksize == 0)
			goto fake_it;

		if (dp->blksize == 0)
			dp->blksize = 512;

		return 0;
	}

fake_it:
	if ((sd->sc_link->quirks & SDEV_NOMODESENSE) == 0) {
		if (error == 0)
			printf("%s: mode sense (%d) returned nonsense",
			    sd->sc_dev.dv_xname, page);
		else
			printf("%s: could not mode sense (4/5)",
			    sd->sc_dev.dv_xname);
		printf("; using fictitious geometry\n");
	}
	/*
	 * use adaptec standard fictitious geometry
	 * this depends on which controller (e.g. 1542C is
	 * different. but we have to put SOMETHING here..)
	 */
	sectors = scsi_size(sd->sc_link, flags);
	dp->heads = 64;
	dp->sectors = 32;
	dp->cyls = sectors / (64 * 32);
	dp->blksize = 512;
	dp->disksize = sectors;
	return 0;
}

int
sdsize(dev)
	dev_t dev;
{
	struct sd_softc *sd;
	int part, unit, omask;
	int size;

	unit = SDUNIT(dev);
	if (unit >= sd_cd.cd_ndevs)
		return (-1);
	sd = sd_cd.cd_devs[unit];
	if (sd == NULL)
		return (-1);

	part = SDPART(dev);
	omask = sd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && sdopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	if (sd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = sd->sc_dk.dk_label->d_partitions[part].p_size *
		    (sd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && sdclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}

#ifndef __BDEVSW_DUMP_OLD_TYPE
/* #define SD_DUMP_NOT_TRUSTED if you just want to watch */
static struct scsi_xfer sx;
static int sddoingadump;

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	struct sd_softc *sd;	/* disk unit to do the I/O */
	struct disklabel *lp;	/* disk's disklabel */
	int	unit, part;
	int	sectorsize;	/* size of a disk sector */
	int	nsects;		/* number of sectors in partition */
	int	sectoff;	/* sector offset of partition */
	int	totwrt;		/* total number of sectors left to write */
	int	nwrt;		/* current number of sectors to write */
	struct scsi_rw_big cmd;	/* write command */
	struct scsi_xfer *xs;	/* ... convenience */
	int	retval;

	/* Check if recursive dump; if so, punt. */
	if (sddoingadump)
		return EFAULT;

	/* Mark as active early. */
	sddoingadump = 1;

	unit = SDUNIT(dev);	/* Decompose unit & partition. */
	part = SDPART(dev);

	/* Check for acceptable drive number. */
	if (unit >= sd_cd.cd_ndevs || (sd = sd_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	/*
	 * XXX Can't do this check, since the media might have been
	 * XXX marked `invalid' by successful unmounting of all
	 * XXX filesystems.
	 */
#if 0
	/* Make sure it was initialized. */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) != SDEV_MEDIA_LOADED)
		return ENXIO;
#endif

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = sd->sc_dk.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return EFAULT;
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = lp->d_partitions[part].p_size;
	sectoff = lp->d_partitions[part].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + totwrt) > nsects))
		return EINVAL;

	/* Offset block number to start of partition. */
	blkno += sectoff;

	xs = &sx;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef	SD_DUMP_NOT_TRUSTED
		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = WRITE_BIG;
		_lto4b(blkno, cmd.addr);
		_lto2b(nwrt, cmd.length);
		/*
		 * Fill out the scsi_xfer structure
		 *    Note: we cannot sleep as we may be an interrupt
		 * don't use scsi_scsi_cmd() as it may want
		 * to wait for an xs.
		 */
		bzero(xs, sizeof(sx));
		xs->flags |= SCSI_AUTOCONF | INUSE | SCSI_DATA_OUT;
		xs->sc_link = sd->sc_link;
		xs->retries = SDRETRIES;
		xs->timeout = 10000;	/* 10000 millisecs for a disk ! */
		xs->cmd = (struct scsi_generic *)&cmd;
		xs->cmdlen = sizeof(cmd);
		xs->resid = nwrt * sectorsize;
		xs->error = XS_NOERROR;
		xs->bp = 0;
		xs->data = va;
		xs->datalen = nwrt * sectorsize;

		/*
		 * Pass all this info to the scsi driver.
		 */
		retval = (*(sd->sc_link->adapter->scsi_cmd)) (xs);
		if (retval != COMPLETE)
			return ENXIO;
#else	/* SD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("sd%d: dump addr 0x%x, blk %d\n", unit, va, blkno);
		delay(500 * 1000);	/* half a second */
#endif	/* SD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va += sectorsize * nwrt;
	}
	sddoingadump = 0;
	return 0;
}
#else	/* __BDEVSW_DUMP_NEW_TYPE */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}
#endif	/* __BDEVSW_DUMP_NEW_TYPE */
