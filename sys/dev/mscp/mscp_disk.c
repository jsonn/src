/*	$NetBSD: mscp_disk.c,v 1.45.4.1 2005/02/12 18:17:46 yamt Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * RA disk device driver
 * RX MSCP floppy disk device driver
 */

/*
 * TODO
 *	write bad block forwarding code
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mscp_disk.c,v 1.45.4.1 2005/02/12 18:17:46 yamt Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>

#include "locators.h"
#include "ioconf.h"
#include "ra.h"

/*
 * Drive status, per drive
 */
struct ra_softc {
	struct	device ra_dev;	/* Autoconf struct */
	struct	disk ra_disk;
	int	ra_state;	/* open/closed state */
	u_long	ra_mediaid;	/* media id */
	int	ra_hwunit;	/* Hardware unit number */
	int	ra_havelabel;	/* true if we have a label */
	int	ra_wlabel;	/* label sector is currently writable */
};

#define rx_softc ra_softc

void	rxattach(struct device *, struct device *, void *);
int	rx_putonline(struct rx_softc *);
void	rrmakelabel(struct disklabel *, long);

#if NRA

int	ramatch(struct device *, struct cfdata *, void *);
void	raattach(struct device *, struct device *, void *);
int	ra_putonline(struct ra_softc *);

CFATTACH_DECL(ra, sizeof(struct ra_softc),
    ramatch, rxattach, NULL, NULL);

dev_type_open(raopen);
dev_type_close(raclose);
dev_type_read(raread);
dev_type_write(rawrite);
dev_type_ioctl(raioctl);
dev_type_strategy(rastrategy);
dev_type_dump(radump);
dev_type_size(rasize);

const struct bdevsw ra_bdevsw = {
	raopen, raclose, rastrategy, raioctl, radump, rasize, D_DISK
};

const struct cdevsw ra_cdevsw = {
	raopen, raclose, raread, rawrite, raioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static struct dkdriver radkdriver = {
	rastrategy, minphys
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
ramatch(parent, cf, aux)
	struct	device *parent;
	struct	cfdata *cf;
	void	*aux;
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[MSCPBUSCF_DRIVE] != MSCPBUSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[MSCPBUSCF_DRIVE] != mp->mscp_unit)
		return 0;
	/*
	 * Check if this disk is a floppy; then don't configure it.
	 * Seems to be a safe way to test it per Chris Torek.
	 */
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		return 0;
	return 1;
}

/* 
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it har fallen offline.
 */
int
ra_putonline(ra)
	struct ra_softc *ra;
{
	struct	disklabel *dl;
	const char *msg;
	int maj;

	if (rx_putonline(ra) != MSCP_DONE)
		return MSCP_FAILED;

	dl = ra->ra_disk.dk_label;

	ra->ra_state = DK_RDLABEL;
	printf("%s", ra->ra_dev.dv_xname);
	maj = cdevsw_lookup_major(&ra_cdevsw);
	if ((msg = readdisklabel(MAKEDISKDEV(maj, ra->ra_dev.dv_unit,
	    RAW_PART), rastrategy, dl, NULL)) != NULL)
		printf(": %s", msg);
	else {
		ra->ra_havelabel = 1;
		ra->ra_state = DK_OPEN;
	}

	printf(": size %d sectors\n", dl->d_secperunit);

	return MSCP_DONE;
}

/*
 * Open a drive.
 */
/*ARGSUSED*/
int
raopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct	proc *p;
{
	struct ra_softc *ra;
	int error, part, unit, mask;
	/*
	 * Make sure this is a reasonable open request.
	 */
	unit = DISKUNIT(dev);
	if (unit >= ra_cd.cd_ndevs)
		return ENXIO;
	ra = ra_cd.cd_devs[unit];
	if (ra == 0)
		return ENXIO;

	part = DISKPART(dev);

	if ((error = lockmgr(&ra->ra_disk.dk_openlock, LK_EXCLUSIVE,
			     NULL)) != 0)
		return (error);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (ra->ra_disk.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad1;
	}

	/*
	 * If this is the first open; we must first try to put
	 * the disk online (and read the label).
	 */
	if (ra->ra_state == DK_CLOSED) {
		if (ra_putonline(ra) == MSCP_FAILED) {
			error = ENXIO;
			goto bad1;
		}
	}

	/* If the disk has no label; allow writing everywhere */
	if (ra->ra_havelabel == 0)
		ra->ra_wlabel = 1;

	if (part >= ra->ra_disk.dk_label->d_npartitions) {
		error = ENXIO;
		goto bad1;
	}

	/*
	 * Wait for the state to settle
	 */
#if notyet
	while (ra->ra_state != DK_OPEN)
		if ((error = tsleep((caddr_t)ra, (PZERO + 1) | PCATCH,
		    devopn, 0))) {
			splx(s);
			return (error);
		}
#endif

	mask = 1 << part;

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask |= mask;
		break;
	}
	ra->ra_disk.dk_openmask |= mask;
	(void) lockmgr(&ra->ra_disk.dk_openlock, LK_RELEASE, NULL);
	return 0;

 bad1:
	(void) lockmgr(&ra->ra_disk.dk_openlock, LK_RELEASE, NULL);
	return (error);
}

/* ARGSUSED */
int
raclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct	proc *p;
{
	int unit = DISKUNIT(dev);
	struct ra_softc *ra = ra_cd.cd_devs[unit];
	int error, mask = (1 << DISKPART(dev));

	if ((error = lockmgr(&ra->ra_disk.dk_openlock, LK_EXCLUSIVE,
			     NULL)) != 0)
		return (error);

	switch (fmt) {
	case S_IFCHR:
		ra->ra_disk.dk_copenmask &= ~mask;
		break;
	case S_IFBLK:
		ra->ra_disk.dk_bopenmask &= ~mask;
		break;
	}
	ra->ra_disk.dk_openmask =
	    ra->ra_disk.dk_copenmask | ra->ra_disk.dk_bopenmask;

	/*
	 * Should wait for I/O to complete on this partition even if
	 * others are open, but wait for work on blkflush().
	 */
#if notyet
	if (ra->ra_openpart == 0) {
		s = spluba();
		while (BUFQ_PEEK(&udautab[unit]) != NULL)
			(void) tsleep(&udautab[unit], PZERO - 1,
			    "raclose", 0);
		splx(s);
		ra->ra_state = CLOSED;
		ra->ra_wlabel = 0;
	}
#endif
	(void) lockmgr(&ra->ra_disk.dk_openlock, LK_RELEASE, NULL);
	return (0);
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 */
void
rastrategy(bp)
	struct buf *bp;
{
	int unit;
	struct ra_softc *ra;
	int b;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
	unit = DISKUNIT(bp->b_dev);
	if (unit > ra_cd.cd_ndevs || (ra = ra_cd.cd_devs[unit]) == NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}
	/*
	 * If drive is open `raw' or reading label, let it at it.
	 */
	if (ra->ra_state == DK_RDLABEL) {
	        /* Make some statistics... /bqt */
	        b = splbio();
	        disk_busy(&ra->ra_disk);
		splx(b);
		mscp_strategy(bp, ra->ra_dev.dv_parent);
		return;
	}

	/* If disk is not online, try to put it online */
	if (ra->ra_state == DK_CLOSED)
		if (ra_putonline(ra) == MSCP_FAILED) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			goto done;
		}

	/*
	 * Determine the size of the transfer, and make sure it is
	 * within the boundaries of the partition.
	 */
	if (bounds_check_with_label(&ra->ra_disk, bp, ra->ra_wlabel) <= 0)
		goto done;

	/* Make some statistics... /bqt */
	b = splbio();
	disk_busy(&ra->ra_disk);
	splx(b);
	mscp_strategy(bp, ra->ra_dev.dv_parent);
	return;

done:
	biodone(bp);
}

int
raread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(rastrategy, NULL, dev, B_READ, minphys, uio));
}

int
rawrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(rastrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * I/O controls.
 */
int
raioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DISKUNIT(dev);
	struct disklabel *lp, *tp;
	struct ra_softc *ra = ra_cd.cd_devs[unit];
	int error = 0;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif

	lp = ra->ra_disk.dk_label;

	switch (cmd) {

	case DIOCGDINFO:
		bcopy(lp, data, sizeof (struct disklabel));
		break;
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
		bcopy(lp, &newlabel, sizeof disklabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		bcopy(&newlabel, data, sizeof (struct olddisklabel));
		break;
#endif

	case DIOCGPART:
		((struct partinfo *)data)->disklab = lp;
		((struct partinfo *)data)->part =
		    &lp->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
		if (cmd == ODIOCSDINFO || xfer == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			tp = &newlabel;
		} else
#endif
		tp = (struct disklabel *)data;

		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			if ((error = lockmgr(&ra->ra_disk.dk_openlock,
					     LK_EXCLUSIVE, NULL)) != 0)
				break;
			error = setdisklabel(lp, tp, 0, 0);
			if ((error == 0) && (cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || cmd == ODIOCWDINFO
#else
			    )) {
#endif
				ra->ra_wlabel = 1;
				error = writedisklabel(dev, rastrategy, lp,0);
				ra->ra_wlabel = 0;
			}
			(void) lockmgr(&ra->ra_disk.dk_openlock,
				       LK_RELEASE, NULL);
		}
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			ra->ra_wlabel = 1;
		break;

	case DIOCGDEFLABEL:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		if (cmd == ODIOCGDEFLABEL)
			tp = &newlabel;
		else
#else
		tp = (struct disklabel *)data;
#endif
		bzero(tp, sizeof(struct disklabel));
		tp->d_secsize = lp->d_secsize;
		tp->d_nsectors = lp->d_nsectors;
		tp->d_ntracks = lp->d_ntracks;
		tp->d_ncylinders = lp->d_ncylinders;
		tp->d_secpercyl = lp->d_secpercyl;
		tp->d_secperunit = lp->d_secperunit;
		tp->d_type = DTYPE_MSCP;
		tp->d_rpm = 3600;
		rrmakelabel(tp, ra->ra_mediaid);
#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCGDEFLABEL) {
			if (tp->d_npartitions > OLDMAXPARTITIONS)
				return ENOTTY;
			memcpy(data, tp, sizeof (struct olddisklabel));
		}
#endif
		break;

	case DIOCAWEDGE:
	    {
	    	struct dkwedge_info *dkw = (void *) data;

		if ((flag & FWRITE) == 0)
			return (EBADF);

		/* If the ioctl happens here, the parent is us. */
		strcpy(dkw->dkw_parent, ra->ra_dev.dv_xname);
		return (dkwedge_add(dkw));
	    }
	
	case DIOCDWEDGE:
	    {
	    	struct dkwedge_info *dkw = (void *) data;

		if ((flag & FWRITE) == 0)
			return (EBADF);

		/* If the ioctl happens here, the parent is us. */
		strcpy(dkw->dkw_parent, ra->ra_dev.dv_xname);
		return (dkwedge_del(dkw));
	    }
	
	case DIOCLWEDGES:
	    {
	    	struct dkwedge_list *dkwl = (void *) data;

		return (dkwedge_list(&ra->ra_disk, dkwl, p));
	    }

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}


int
radump(dev, blkno, va, size)
	dev_t	dev;
	daddr_t blkno;
	caddr_t va;
	size_t	size;
{
	return ENXIO;
}

/*
 * Return the size of a partition, if known, or -1 if not.
 */
int
rasize(dev)
	dev_t dev;
{
	int unit = DISKUNIT(dev);
	struct ra_softc *ra;

	if (unit >= ra_cd.cd_ndevs || ra_cd.cd_devs[unit] == 0)
		return -1;

	ra = ra_cd.cd_devs[unit];

	if (ra->ra_state == DK_CLOSED)
		if (ra_putonline(ra) == MSCP_FAILED)
			return -1;

	return ra->ra_disk.dk_label->d_partitions[DISKPART(dev)].p_size *
	    (ra->ra_disk.dk_label->d_secsize / DEV_BSIZE);
}

#endif /* NRA */

#if NRX

int	rxmatch(struct device *, struct cfdata *, void *);

CFATTACH_DECL(rx, sizeof(struct rx_softc),
    rxmatch, rxattach, NULL, NULL);

dev_type_open(rxopen);
dev_type_read(rxread);
dev_type_write(rxwrite);
dev_type_ioctl(rxioctl);
dev_type_strategy(rxstrategy);
dev_type_dump(rxdump);
dev_type_size(rxsize);

const struct bdevsw rx_bdevsw = {
	rxopen, nullclose, rxstrategy, rxioctl, rxdump, rxsize, D_DISK
};

const struct cdevsw rx_cdevsw = {
	rxopen, nullclose, rxread, rxwrite, rxioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_DISK
};

static struct dkdriver rxdkdriver = {
	rxstrategy, minphys
};

/*
 * More driver definitions, for generic MSCP code.
 */

int
rxmatch(parent, cf, aux)
	struct	device *parent;
	struct	cfdata *cf;
	void	*aux;
{
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;

	if ((da->da_typ & MSCPBUS_DISK) == 0)
		return 0;
	if (cf->cf_loc[MSCPBUSCF_DRIVE] != MSCPBUSCF_DRIVE_DEFAULT &&
	    cf->cf_loc[MSCPBUSCF_DRIVE] != mp->mscp_unit)
		return 0;
	/*
	 * Check if this disk is a floppy; then configure it.
	 * Seems to be a safe way to test it per Chris Torek.
	 */
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		return 1;
	return 0;
}

#endif /* NRX */

/*
 * The attach routine only checks and prints drive type.
 * Bringing the disk online is done when the disk is accessed
 * the first time. 
 */
void
rxattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux; 
{
	struct	rx_softc *rx = (void *)self;
	struct	drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;
	struct	mscp_softc *mi = (void *)parent;
	struct	disklabel *dl;

	rx->ra_mediaid = mp->mscp_guse.guse_mediaid;
	rx->ra_state = DK_CLOSED;
	rx->ra_hwunit = mp->mscp_unit;
	mi->mi_dp[mp->mscp_unit] = self;

	rx->ra_disk.dk_name = rx->ra_dev.dv_xname;
#if NRX
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) == 'X' - '@')
		rx->ra_disk.dk_driver = &rxdkdriver;
#endif
#if NRA
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) != 'X' - '@')
		rx->ra_disk.dk_driver = &radkdriver;
#endif
	disk_attach((struct disk *)&rx->ra_disk);

	/* Fill in what we know. The actual size is gotten later */
	dl = rx->ra_disk.dk_label;

	dl->d_secsize = DEV_BSIZE;
	dl->d_nsectors = mp->mscp_guse.guse_nspt;
	dl->d_ntracks = mp->mscp_guse.guse_ngpc * mp->mscp_guse.guse_group;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	disk_printtype(mp->mscp_unit, mp->mscp_guse.guse_mediaid);
#ifdef DEBUG
	printf("%s: nspt %d group %d ngpc %d rct %d nrpt %d nrct %d\n",
	    self->dv_xname, mp->mscp_guse.guse_nspt, mp->mscp_guse.guse_group,
	    mp->mscp_guse.guse_ngpc, mp->mscp_guse.guse_rctsize,
	    mp->mscp_guse.guse_nrpt, mp->mscp_guse.guse_nrct);
#endif
	if (MSCP_MID_ECH(1, mp->mscp_guse.guse_mediaid) != 'X' - '@') {
		/*
		 * XXX We should try to discover wedges here, but
		 * XXX that would mean being able to do I/O.  Should
		 * XXX use config_defer() here.
		 */
	}
}

/* 
 * (Try to) put the drive online. This is done the first time the
 * drive is opened, or if it har fallen offline.
 */
int
rx_putonline(rx)
	struct rx_softc *rx;
{
	struct	mscp *mp;
	struct	mscp_softc *mi = (struct mscp_softc *)rx->ra_dev.dv_parent;
	volatile int i;

	rx->ra_state = DK_CLOSED;
	mp = mscp_getcp(mi, MSCP_WAIT);
	mp->mscp_opcode = M_OP_ONLINE;
	mp->mscp_unit = rx->ra_hwunit;
	mp->mscp_cmdref = 1;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;

	/* Poll away */
	i = bus_space_read_2(mi->mi_iot, mi->mi_iph, 0);
	if (tsleep(&rx->ra_dev.dv_unit, PRIBIO, "rxonline", 100*100))
		rx->ra_state = DK_CLOSED;

	if (rx->ra_state == DK_CLOSED)
		return MSCP_FAILED;

	return MSCP_DONE;
}

#if NRX

/*
 * Open a drive.
 */
/*ARGSUSED*/
int
rxopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct	proc *p;
{
	struct rx_softc *rx;
	int unit;

	/*
	 * Make sure this is a reasonable open request.
	 */
	unit = DISKUNIT(dev);
	if (unit >= rx_cd.cd_ndevs)
		return ENXIO;
	rx = rx_cd.cd_devs[unit];
	if (rx == 0)
		return ENXIO;

	/*
	 * If this is the first open; we must first try to put
	 * the disk online (and read the label).
	 */
	if (rx->ra_state == DK_CLOSED)
		if (rx_putonline(rx) == MSCP_FAILED)
			return ENXIO;

	return 0;
}

/*
 * Queue a transfer request, and if possible, hand it to the controller.
 *
 * This routine is broken into two so that the internal version
 * udastrat1() can be called by the (nonexistent, as yet) bad block
 * revectoring routine.
 */
void
rxstrategy(bp)
	struct buf *bp;
{
	int unit;
	struct rx_softc *rx;
	int b;

	/*
	 * Make sure this is a reasonable drive to use.
	 */
	unit = DISKUNIT(bp->b_dev);
	if (unit > rx_cd.cd_ndevs || (rx = rx_cd.cd_devs[unit]) == NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* If disk is not online, try to put it online */
	if (rx->ra_state == DK_CLOSED)
		if (rx_putonline(rx) == MSCP_FAILED) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			goto done;
		}

	/*
	 * Determine the size of the transfer, and make sure it is
	 * within the boundaries of the partition.
	 */
	if (bp->b_blkno >= rx->ra_disk.dk_label->d_secperunit) {
		bp->b_resid = bp->b_bcount;
		goto done;
	}

	/* Make some statistics... /bqt */
	b = splbio();
	disk_busy(&rx->ra_disk);
	splx(b);
	mscp_strategy(bp, rx->ra_dev.dv_parent);
	return;

done:
	biodone(bp);
}

int
rxread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	return (physio(rxstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rxwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	return (physio(rxstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * I/O controls.
 */
int
rxioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DISKUNIT(dev);
	struct disklabel *lp;
	struct rx_softc *rx = rx_cd.cd_devs[unit];
	int error = 0;

	lp = rx->ra_disk.dk_label;

	switch (cmd) {

	case DIOCGDINFO:
		bcopy(lp, data, sizeof (struct disklabel));
		break;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = lp;
		((struct partinfo *)data)->part =
		    &lp->d_partitions[DISKPART(dev)];
		break;


	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCWLABEL:
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
rxdump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not likely. */
	return ENXIO;
}

int
rxsize(dev)
	dev_t dev;
{

	return -1;
}

#endif /* NRX */

void	rrdgram(struct device *, struct mscp *, struct mscp_softc *);
void	rriodone(struct device *, struct buf *);
int	rronline(struct device *, struct mscp *);
int	rrgotstatus(struct device *, struct mscp *);
void	rrreplace(struct device *, struct mscp *);
int	rrioerror(struct device *, struct mscp *, struct buf *);
void	rrfillin(struct buf *, struct mscp *);
void	rrbb(struct device *, struct mscp *, struct buf *);


struct	mscp_device ra_device = {
	rrdgram,
	rriodone,
	rronline,
	rrgotstatus,
	rrreplace,
	rrioerror,
	rrbb,
	rrfillin,
};

/*
 * Handle an error datagram.
 * This can come from an unconfigured drive as well.
 */	
void	    
rrdgram(usc, mp, mi)
	struct device *usc;
	struct mscp *mp; 
	struct mscp_softc *mi;
{	 
	if (mscp_decodeerror(usc == NULL?"unconf disk" : usc->dv_xname, mp, mi))
		return;	 
	/*
	 * SDI status information bytes 10 and 11 are the microprocessor
	 * error code and front panel code respectively.  These vary per
	 * drive type and are printed purely for field service information.
	 */
	if (mp->mscp_format == M_FM_SDI) 
		printf("\tsdi uproc error code 0x%x, front panel code 0x%x\n",
			mp->mscp_erd.erd_sdistat[10],
			mp->mscp_erd.erd_sdistat[11]);
}

void	
rriodone(usc, bp)
	struct device *usc;
	struct buf *bp;
{
	struct ra_softc *ra;
	int unit;

	/* We assume that this is a reasonable drive. ra_strategy should
	   already have verified it. Thus, no checks here... /bqt */
	unit = DISKUNIT(bp->b_dev);
#if NRA
	if (cdevsw_lookup(bp->b_dev) == &ra_cdevsw)
		ra = ra_cd.cd_devs[unit];
	else
#endif
#if NRX
	if (cdevsw_lookup(bp->b_dev) == &rx_cdevsw)
		ra = rx_cd.cd_devs[unit];
	else
#endif
		panic("rriodone: unexpected major %d unit %d",
		    major(bp->b_dev), unit);
	disk_unbusy(&ra->ra_disk, bp->b_bcount, (bp->b_flags & B_READ));

	biodone(bp);
}

/*
 * A drive came on line.  Check its type and size.  Return DONE if
 * we think the drive is truly on line.	 In any case, awaken anyone
 * sleeping on the drive on-line-ness.
 */
int
rronline(usc, mp)
	struct device *usc;
	struct mscp *mp;
{
	struct rx_softc *rx = (struct rx_softc *)usc;
	struct disklabel *dl;

	wakeup((caddr_t)&usc->dv_unit);
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		printf("%s: attempt to bring on line failed: ", usc->dv_xname);
		mscp_printevent(mp);
		return (MSCP_FAILED);
	}

	rx->ra_state = DK_OPEN;
 
	dl = rx->ra_disk.dk_label;
	dl->d_secperunit = (daddr_t)mp->mscp_onle.onle_unitsize;

	if (dl->d_secpercyl) {
		dl->d_ncylinders = dl->d_secperunit/dl->d_secpercyl;
		dl->d_type = DTYPE_MSCP;
		dl->d_rpm = 3600;
	} else {
		dl->d_type = DTYPE_FLOPPY;
		dl->d_rpm = 300;
	}
	rrmakelabel(dl, rx->ra_mediaid);

	return (MSCP_DONE);
}

void
rrmakelabel(dl, type)
	struct disklabel *dl;
	long type;
{
	int n, p = 0;

	dl->d_bbsize = BBSIZE;
	dl->d_sbsize = SBLOCKSIZE;

	/* Create the disk name for disklabel. Phew... */
	dl->d_typename[p++] = MSCP_MID_CHAR(2, type);
	dl->d_typename[p++] = MSCP_MID_CHAR(1, type);
	if (MSCP_MID_ECH(0, type))
		dl->d_typename[p++] = MSCP_MID_CHAR(0, type);
	n = MSCP_MID_NUM(type);
	if (n > 99) {
		dl->d_typename[p++] = '1';
		n -= 100;
	}
	if (n > 9) {
		dl->d_typename[p++] = (n / 10) + '0';
		n %= 10;
	}
	dl->d_typename[p++] = n + '0';
	dl->d_typename[p] = 0;
	dl->d_npartitions = MAXPARTITIONS;
	dl->d_partitions[0].p_size = dl->d_partitions[2].p_size =
	    dl->d_secperunit;
	dl->d_partitions[0].p_offset = dl->d_partitions[2].p_offset = 0;
	dl->d_interleave = dl->d_headswitch = 1;
	dl->d_magic = dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);
}

/*	
 * We got some (configured) unit's status.  Return DONE if it succeeded.
 */
int
rrgotstatus(usc, mp)
	struct device *usc;
	struct mscp *mp;
{	
	if ((mp->mscp_status & M_ST_MASK) != M_ST_SUCCESS) {
		printf("%s: attempt to get status failed: ", usc->dv_xname);
		mscp_printevent(mp);
		return (MSCP_FAILED);
	}
	/* record for (future) bad block forwarding and whatever else */
#ifdef notyet
	uda_rasave(ui->ui_unit, mp, 1);
#endif
	return (MSCP_DONE);
}

/*	
 * A replace operation finished.
 */
/*ARGSUSED*/
void	
rrreplace(usc, mp)
	struct device *usc;
	struct mscp *mp;
{

	panic("udareplace");
}

/*
 * A transfer failed.  We get a chance to fix or restart it.
 * Need to write the bad block forwaring code first....
 */
/*ARGSUSED*/
int 
rrioerror(usc, mp, bp)
	struct device *usc;
	struct mscp *mp;
	struct buf *bp;
{
	struct ra_softc *ra = (void *)usc;
	int code = mp->mscp_event;

	switch (code & M_ST_MASK) {
	/* The unit has fallen offline. Try to figure out why. */
	case M_ST_OFFLINE:
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		ra->ra_state = DK_CLOSED;
		if (code & M_OFFLINE_UNMOUNTED)
			printf("%s: not mounted/spun down\n", usc->dv_xname);
		if (code & M_OFFLINE_DUPLICATE)
			printf("%s: duplicate unit number!!!\n", usc->dv_xname);
		return MSCP_DONE;

	case M_ST_AVAILABLE:
		ra->ra_state = DK_CLOSED; /* Force another online */
		return MSCP_DONE;

	default:
		printf("%s:", usc->dv_xname);
		break;
	}
	return (MSCP_FAILED);
}

/*
 * Fill in disk addresses in a mscp packet waiting for transfer.
 */
void
rrfillin(bp, mp)
	struct buf *bp;
	struct mscp *mp;
{
	struct rx_softc *rx = 0; /* Wall */
	struct disklabel *lp;
	int unit = DISKUNIT(bp->b_dev);
	int part = DISKPART(bp->b_dev);

#if NRA
	if (cdevsw_lookup(bp->b_dev) == &ra_cdevsw)
		rx = ra_cd.cd_devs[unit];
#endif
#if NRX
	if (cdevsw_lookup(bp->b_dev) == &rx_cdevsw)
		rx = rx_cd.cd_devs[unit];
#endif
	lp = rx->ra_disk.dk_label;

	mp->mscp_seq.seq_lbn = lp->d_partitions[part].p_offset + bp->b_blkno;
	mp->mscp_unit = rx->ra_hwunit;
	mp->mscp_seq.seq_bytecount = bp->b_bcount;
}

/*
 * A bad block related operation finished.
 */
/*ARGSUSED*/
void
rrbb(usc, mp, bp)
	struct device *usc;
	struct mscp *mp;
	struct buf *bp;
{

	panic("udabb");
}
