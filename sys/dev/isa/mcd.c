/*	$NetBSD: mcd.c,v 1.57.2.1 1997/11/12 02:32:50 mellon Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
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
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
 * All rights reserved.
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
 *	This software was developed by Holger Veit and Brian Moore
 *      for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*static char COPYRIGHT[] = "mcd-driver (C)1993 by H.Veit & B.Moore";*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/mcdreg.h>

#ifndef MCDDEBUG
#define MCD_TRACE(fmt,a,b,c,d)
#else
#define MCD_TRACE(fmt,a,b,c,d)	{if (sc->debug) {printf("%s: st=%02x: ", sc->sc_dev.dv_xname, sc->status); printf(fmt,a,b,c,d);}}
#endif

#define	MCDPART(dev)	DISKPART(dev)
#define	MCDUNIT(dev)	DISKUNIT(dev)

/* toc */
#define MCD_MAXTOCS	104	/* from the Linux driver */

struct mcd_mbx {
	int		retry, count;
	struct buf	*bp;
	daddr_t		blkno;
	int		nblk;
	int		sz;
	u_long		skip;
	int		state;
#define	MCD_S_IDLE	0
#define MCD_S_BEGIN	1
#define MCD_S_WAITMODE	2
#define MCD_S_WAITREAD	3
	int		mode;
};

struct mcd_softc {
	struct	device sc_dev;
	struct	disk sc_dk;
	void *sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int	irq, drq;

	char	*type;
	int	flags;
#define	MCDF_LOCKED	0x01
#define	MCDF_WANTED	0x02
#define	MCDF_WLABEL	0x04	/* label is writable */
#define	MCDF_LABELLING	0x08	/* writing label */
#define	MCDF_LOADED	0x10	/* parameters loaded */
	short	status;
	short	audio_status;
	int	blksize;
	u_long	disksize;
	struct	mcd_volinfo volinfo;
	union	mcd_qchninfo toc[MCD_MAXTOCS];
	struct	mcd_command lastpb;
	struct	mcd_mbx mbx;
	int	lastmode;
#define	MCD_MD_UNKNOWN	-1
	int	lastupc;
#define	MCD_UPC_UNKNOWN	-1
	struct	buf buf_queue;
	u_char	readcmd;
	u_char	debug;
	u_char	probe;
};

/* prototypes */
/* XXX does not belong here */
cdev_decl(mcd);
bdev_decl(mcd);

static int bcd2bin __P((bcd_t));
static bcd_t bin2bcd __P((int));
static void hsg2msf __P((int, bcd_t *));
static daddr_t msf2hsg __P((bcd_t *, int));

int mcd_playtracks __P((struct mcd_softc *, struct ioc_play_track *));
int mcd_playmsf __P((struct mcd_softc *, struct ioc_play_msf *));
int mcd_playblocks __P((struct mcd_softc *, struct ioc_play_blocks *));
int mcd_stop __P((struct mcd_softc *));
int mcd_eject __P((struct mcd_softc *));
int mcd_read_subchannel __P((struct mcd_softc *, struct ioc_read_subchannel *));
int mcd_pause __P((struct mcd_softc *));
int mcd_resume __P((struct mcd_softc *));
int mcd_toc_header __P((struct mcd_softc *, struct ioc_toc_header *));
int mcd_toc_entries __P((struct mcd_softc *, struct ioc_read_toc_entry *));

int mcd_getreply __P((struct mcd_softc *));
int mcd_getstat __P((struct mcd_softc *));
int mcd_getresult __P((struct mcd_softc *, struct mcd_result *));
void mcd_setflags __P((struct mcd_softc *));
int mcd_get __P((struct mcd_softc *, char *, int));
int mcd_send __P((struct mcd_softc *, struct mcd_mbox *, int));
int mcdintr __P((void *));
void mcd_soft_reset __P((struct mcd_softc *));
int mcd_hard_reset __P((struct mcd_softc *));
int mcd_setmode __P((struct mcd_softc *, int));
int mcd_setupc __P((struct mcd_softc *, int));
int mcd_read_toc __P((struct mcd_softc *));
int mcd_getqchan __P((struct mcd_softc *, union mcd_qchninfo *, int));
int mcd_setlock __P((struct mcd_softc *, int));

int mcd_find __P((bus_space_tag_t, bus_space_handle_t, struct mcd_softc *));
int mcdprobe __P((struct device *, void *, void *));
void mcdattach __P((struct device *, struct device *, void *));

struct cfattach mcd_ca = {
	sizeof(struct mcd_softc), mcdprobe, mcdattach
};

struct cfdriver mcd_cd = {
	NULL, "mcd", DV_DISK
};

void	mcdgetdefaultlabel __P((struct mcd_softc *, struct disklabel *));
void	mcdgetdisklabel __P((struct mcd_softc *));
int	mcd_get_parms __P((struct mcd_softc *));
void	mcdstrategy __P((struct buf *));
void	mcdstart __P((struct mcd_softc *));
int	mcdlock __P((struct mcd_softc *));
void	mcdunlock __P((struct mcd_softc *));
void	mcd_pseudointr __P((void *));

struct dkdriver mcddkdriver = { mcdstrategy };

#define MCD_RETRIES	3
#define MCD_RDRETRIES	3

/* several delays */
#define RDELAY_WAITMODE	300
#define RDELAY_WAITREAD	800

#define	DELAY_GRANULARITY	25	/* 25us */
#define DELAY_GETREPLY		100000	/* 100000 * 25us */

void
mcdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mcd_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct mcd_mbox mbx;

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_iobase, MCD_NPORT, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	sc->probe = 0;
	sc->debug = 0;

	if (!mcd_find(iot, ioh, sc)) {
		printf(": mcd_find failed\n");
		return;
	}

	/*
	 * Initialize and attach the disk structure.
	 */
	sc->sc_dk.dk_driver = &mcddkdriver;
	sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dk);

	printf(": model %s\n", sc->type != 0 ? sc->type : "unknown");

	(void) mcd_setlock(sc, MCD_LK_UNLOCK);

	mbx.cmd.opcode = MCD_CMDCONFIGDRIVE;
	mbx.cmd.length = sizeof(mbx.cmd.data.config) - 1;
	mbx.cmd.data.config.subcommand = MCD_CF_IRQENABLE;
	mbx.cmd.data.config.data1 = 0x01;
	mbx.res.length = 0;
	(void) mcd_send(sc, &mbx, 0);

	mcd_soft_reset(sc);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_BIO, mcdintr, sc);
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
mcdlock(sc)
	struct mcd_softc *sc;
{
	int error;

	while ((sc->flags & MCDF_LOCKED) != 0) {
		sc->flags |= MCDF_WANTED;
		if ((error = tsleep(sc, PRIBIO | PCATCH, "mcdlck", 0)) != 0)
			return error;
	}
	sc->flags |= MCDF_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
mcdunlock(sc)
	struct mcd_softc *sc;
{

	sc->flags &= ~MCDF_LOCKED;
	if ((sc->flags & MCDF_WANTED) != 0) {
		sc->flags &= ~MCDF_WANTED;
		wakeup(sc);
	}
}

int
mcdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	int error;
	int unit, part;
	struct mcd_softc *sc;

	unit = MCDUNIT(dev);
	if (unit >= mcd_cd.cd_ndevs)
		return ENXIO;
	sc = mcd_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if ((error = mcdlock(sc)) != 0)
		return error;

	if (sc->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc->flags & MCDF_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		/*
		 * Lock the drawer.  This will also notice any pending disk
		 * change or door open indicator and clear the MCDF_LOADED bit
		 * if necessary.
		 */
		(void) mcd_setlock(sc, MCD_LK_LOCK);

		if ((sc->flags & MCDF_LOADED) == 0) {
			/* Partially reset the state. */
			sc->lastmode = MCD_MD_UNKNOWN;
			sc->lastupc = MCD_UPC_UNKNOWN;

			sc->flags |= MCDF_LOADED;

			/* Set the mode, causing the disk to spin up. */
			if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
				goto bad2;

			/* Load the physical device parameters. */
			if (mcd_get_parms(sc) != 0) {
				error = ENXIO;
				goto bad2;
			}

			/* Read the table of contents. */
			if ((error = mcd_read_toc(sc)) != 0)
				goto bad2;

			/* Fabricate a disk label. */
			mcdgetdisklabel(sc);
		}
	}

	MCD_TRACE("open: partition=%d disksize=%d blksize=%d\n", part,
	    sc->disksize, sc->blksize, 0);

	part = MCDPART(dev);
	
	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sc->sc_dk.dk_label->d_npartitions ||
	     sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	mcdunlock(sc);
	return 0;

bad2:
	sc->flags &= ~MCDF_LOADED;

bad:
	if (sc->sc_dk.dk_openmask == 0) {
#if 0
		(void) mcd_setmode(sc, MCD_MD_SLEEP);
#endif
		(void) mcd_setlock(sc, MCD_LK_UNLOCK);
	}

bad3:
	mcdunlock(sc);
	return error;
}

int
mcdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct mcd_softc *sc = mcd_cd.cd_devs[MCDUNIT(dev)];
	int part = MCDPART(dev);
	int error;
	
	MCD_TRACE("close: partition=%d\n", part, 0, 0, 0);

	if ((error = mcdlock(sc)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	if (sc->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

#if 0
		(void) mcd_setmode(sc, MCD_MD_SLEEP);
#endif
		(void) mcd_setlock(sc, MCD_LK_UNLOCK);
	}

	mcdunlock(sc);
	return 0;
}

void
mcdstrategy(bp)
	struct buf *bp;
{
	struct mcd_softc *sc = mcd_cd.cd_devs[MCDUNIT(bp->b_dev)];
	int s;
	
	/* Test validity. */
	MCD_TRACE("strategy: buf=0x%lx blkno=%ld bcount=%ld\n", bp,
	    bp->b_blkno, bp->b_bcount, 0);
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % sc->blksize) != 0) {
		printf("%s: strategy: blkno = %d bcount = %ld\n",
		    sc->sc_dev.dv_xname, bp->b_blkno, bp->b_bcount);
		bp->b_error = EINVAL;
		goto bad;
	}

	/* If device invalidated (e.g. media change, door open), error. */
	if ((sc->flags & MCDF_LOADED) == 0) {
		MCD_TRACE("strategy: drive not valid\n", 0, 0, 0, 0);
		bp->b_error = EIO;
		goto bad;
	}

	/* No data to read. */
	if (bp->b_bcount == 0)
		goto done;
	
	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (MCDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, sc->sc_dk.dk_label,
	    (sc->flags & (MCDF_WLABEL|MCDF_LABELLING)) != 0) <= 0)
		goto done;
	
	/* Queue it. */
	s = splbio();
	disksort(&sc->buf_queue, bp);
	splx(s);
	if (!sc->buf_queue.b_active)
		mcdstart(sc);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

void
mcdstart(sc)
	struct mcd_softc *sc;
{
	struct buf *bp, *dp = &sc->buf_queue;
	int s;
	
loop:
	s = splbio();

	bp = dp->b_actf;
	if (bp == NULL) {
		/* Nothing to do. */
		dp->b_active = 0;
		splx(s);
		return;
	}

	/* Block found to process; dequeue. */
	MCD_TRACE("start: found block bp=0x%x\n", bp, 0, 0, 0);
	dp->b_actf = bp->b_actf;
	splx(s);

	/* Changed media? */
	if ((sc->flags & MCDF_LOADED) == 0) {
		MCD_TRACE("start: drive not valid\n", 0, 0, 0, 0);
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		goto loop;
	}

	dp->b_active = 1;

	/* Instrumentation. */
	s = splbio();
	disk_busy(&sc->sc_dk);
	splx(s);

	sc->mbx.retry = MCD_RDRETRIES;
	sc->mbx.bp = bp;
	sc->mbx.blkno = bp->b_blkno / (sc->blksize / DEV_BSIZE);
	if (MCDPART(bp->b_dev) != RAW_PART) {
		struct partition *p;
		p = &sc->sc_dk.dk_label->d_partitions[MCDPART(bp->b_dev)];
		sc->mbx.blkno += p->p_offset;
	}
	sc->mbx.nblk = bp->b_bcount / sc->blksize;
	sc->mbx.sz = sc->blksize;
	sc->mbx.skip = 0;
	sc->mbx.state = MCD_S_BEGIN;
	sc->mbx.mode = MCD_MD_COOKED;

	s = splbio();
	(void) mcdintr(sc);
	splx(s);
}

int
mcdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(mcdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
mcdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (physio(mcdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
mcdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct mcd_softc *sc = mcd_cd.cd_devs[MCDUNIT(dev)];
	int error;
	
	MCD_TRACE("ioctl: cmd=0x%x\n", cmd, 0, 0, 0);

	if ((sc->flags & MCDF_LOADED) == 0)
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *(sc->sc_dk.dk_label);
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = sc->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &sc->sc_dk.dk_label->d_partitions[MCDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = mcdlock(sc)) != 0)
			return error;
		sc->flags |= MCDF_LABELLING;

		error = setdisklabel(sc->sc_dk.dk_label,
		    (struct disklabel *)addr, /*sc->sc_dk.dk_openmask : */0,
		    sc->sc_dk.dk_cpulabel);
		if (error == 0) {
		}

		sc->flags &= ~MCDF_LABELLING;
		mcdunlock(sc);
		return error;

	case DIOCWLABEL:
		return EBADF;

	case DIOCGDEFLABEL:
		mcdgetdefaultlabel(sc, (struct disklabel *)addr);
		return 0;

	case CDIOCPLAYTRACKS:
		return mcd_playtracks(sc, (struct ioc_play_track *)addr);
	case CDIOCPLAYMSF:
		return mcd_playmsf(sc, (struct ioc_play_msf *)addr);
	case CDIOCPLAYBLOCKS:
		return mcd_playblocks(sc, (struct ioc_play_blocks *)addr);
	case CDIOCREADSUBCHANNEL:
		return mcd_read_subchannel(sc, (struct ioc_read_subchannel *)addr);
	case CDIOREADTOCHEADER:
		return mcd_toc_header(sc, (struct ioc_toc_header *)addr);
	case CDIOREADTOCENTRYS:
		return mcd_toc_entries(sc, (struct ioc_read_toc_entry *)addr);
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTEREO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return EINVAL;
	case CDIOCRESUME:
		return mcd_resume(sc);
	case CDIOCPAUSE:
		return mcd_pause(sc);
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return mcd_stop(sc);
	case CDIOCEJECT: /* FALLTHROUGH */
	case DIOCEJECT:
		return mcd_eject(sc);
	case CDIOCALLOW:
		return mcd_setlock(sc, MCD_LK_UNLOCK);
	case CDIOCPREVENT:
		return mcd_setlock(sc, MCD_LK_LOCK);
	case DIOCLOCK:
		return mcd_setlock(sc,
		    (*(int *)addr) ? MCD_LK_LOCK : MCD_LK_UNLOCK);
	case CDIOCSETDEBUG:
		sc->debug = 1;
		return 0;
	case CDIOCCLRDEBUG:
		sc->debug = 0;
		return 0;
	case CDIOCRESET:
		return mcd_hard_reset(sc);

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("mcdioctl: impossible");
#endif
}

void
mcdgetdefaultlabel(sc, lp)
	struct mcd_softc *sc;
	struct disklabel *lp;
{

	bzero(lp, sizeof(struct disklabel));

	lp->d_secsize = sc->blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (sc->disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "Mitsumi CD-ROM", 16);
	lp->d_type = 0;	/* XXX */
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = sc->disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	lp->d_partitions[0].p_offset = 0;
	lp->d_partitions[0].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[0].p_fstype = FS_ISO9660;
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_ISO9660;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * This could have been taken from scsi/cd.c, but it is not clear
 * whether the scsi cd driver is linked in.
 */
void
mcdgetdisklabel(sc)
	struct mcd_softc *sc;
{
	struct disklabel *lp = sc->sc_dk.dk_label;
	
	bzero(sc->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	mcdgetdefaultlabel(sc, lp);
}

int
mcd_get_parms(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;
	daddr_t size;
	int error;

	/* Send volume info command. */
	mbx.cmd.opcode = MCD_CMDGETVOLINFO;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.volinfo);
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	if (mbx.res.data.volinfo.trk_low == 0x00 &&
	    mbx.res.data.volinfo.trk_high == 0x00)
		return EINVAL;

	/* Volinfo is OK. */
	sc->volinfo = mbx.res.data.volinfo;
	sc->blksize = MCD_BLKSIZE_COOKED;
	size = msf2hsg(sc->volinfo.vol_msf, 0);
	sc->disksize = size * (MCD_BLKSIZE_COOKED / DEV_BSIZE);
	return 0;
}

int
mcdsize(dev)
	dev_t dev;
{

	/* CD-ROMs are read-only. */
	return -1;
}

int
mcddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Find the board and fill in the softc.
 */
int
mcd_find(iot, ioh, sc)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct mcd_softc *sc;
{
	int i;
	struct mcd_mbox mbx;

        sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	/* Send a reset. */
	bus_space_write_1(iot, ioh, MCD_RESET, 0);
	delay(1000000);
	/* Get any pending status and throw away. */
	for (i = 10; i; i--)
		bus_space_read_1(iot, ioh, MCD_STATUS);
	delay(1000);

	/* Send get status command. */
	mbx.cmd.opcode = MCD_CMDGETSTAT;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	if (mcd_send(sc, &mbx, 0) != 0)
		return 0;

	/* Get info about the drive. */
	mbx.cmd.opcode = MCD_CMDCONTINFO;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.continfo);
	if (mcd_send(sc, &mbx, 0) != 0)
		return 0;

	/*
	 * The following is code which is not guaranteed to work for all
	 * drives, because the meaning of the expected 'M' is not clear
	 * (M_itsumi is an obvious assumption, but I don't trust that).
	 * Also, the original hack had a bogus condition that always
	 * returned true.
	 *
	 * Note:  Which models support interrupts?  >=LU005S?
	 */
	sc->readcmd = MCD_CMDREADSINGLESPEED;
	switch (mbx.res.data.continfo.code) {
	case 'M':
		if (mbx.res.data.continfo.version <= 2)
			sc->type = "LU002S";
		else if (mbx.res.data.continfo.version <= 5)
			sc->type = "LU005S";
		else
			sc->type = "LU006S";
		break;
	case 'F':
		sc->type = "FX001";
		break;
	case 'D':
		sc->type = "FX001D";
		sc->readcmd = MCD_CMDREADDOUBLESPEED;
		break;
	default:
#ifdef MCDDEBUG
		printf("%s: unrecognized drive version %c%02x; will try to use it anyway\n",
		    sc->sc_dev.dv_xname,
		    mbx.res.data.continfo.code, mbx.res.data.continfo.version);
#endif
		sc->type = 0;
		break;
	}

	return 1;

}

int
mcdprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	struct mcd_softc sc;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_iobase, MCD_NPORT, 0, &ioh))
		return 0;

	sc.debug = 0;
	sc.probe = 1;

	rv = mcd_find(iot, ioh, &sc);

	bus_space_unmap(iot, ioh, MCD_NPORT);

	if (rv)	{
		ia->ia_iosize = MCD_NPORT;
		ia->ia_msize = 0;
	}

	return (rv);
}

int
mcd_getreply(sc)
	struct mcd_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	/* Wait until xfer port senses data ready. */
	for (i = DELAY_GETREPLY; i; i--) {
		if ((bus_space_read_1(iot, ioh, MCD_XFER) &
		    MCD_XF_STATUSUNAVAIL) == 0)
			break;
		delay(DELAY_GRANULARITY);
	}
	if (!i)
		return -1;

	/* Get the data. */
	return bus_space_read_1(iot, ioh, MCD_STATUS);
}

int
mcd_getstat(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;

	mbx.cmd.opcode = MCD_CMDGETSTAT;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_getresult(sc, res)
	struct mcd_softc *sc;
	struct mcd_result *res;
{
	int i, x;

	if (sc->debug)
		printf("%s: mcd_getresult: %d", sc->sc_dev.dv_xname,
		    res->length);

	if ((x = mcd_getreply(sc)) < 0) {
		if (sc->debug)
			printf(" timeout\n");
		else if (!sc->probe)
			printf("%s: timeout in getresult\n", sc->sc_dev.dv_xname);
		return EIO;
	}
	if (sc->debug)
		printf(" %02x", (u_int)x);
	sc->status = x;
	mcd_setflags(sc);

	if ((sc->status & MCD_ST_CMDCHECK) != 0)
		return EINVAL;

	for (i = 0; i < res->length; i++) {
		if ((x = mcd_getreply(sc)) < 0) {
			if (sc->debug)
				printf(" timeout\n");
			else
				printf("%s: timeout in getresult\n", sc->sc_dev.dv_xname);
			return EIO;
		}
		if (sc->debug)
			printf(" %02x", (u_int)x);
		res->data.raw.data[i] = x;
	}

	if (sc->debug)
		printf(" succeeded\n");

#ifdef MCDDEBUG
	delay(10);
	while ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, MCD_XFER) &
	    MCD_XF_STATUSUNAVAIL) == 0) {
		x = bus_space_read_1(sc->sc_iot, sc->sc_ioh, MCD_STATUS);
		printf("%s: got extra byte %02x during getstatus\n",
		    sc->sc_dev.dv_xname, (u_int)x);
		delay(10);
	}
#endif

	return 0;
}

void
mcd_setflags(sc)
	struct mcd_softc *sc;
{

	/* Check flags. */
	if ((sc->flags & MCDF_LOADED) != 0 &&
	    (sc->status & (MCD_ST_DSKCHNG | MCD_ST_DSKIN | MCD_ST_DOOROPEN)) !=
	    MCD_ST_DSKIN) {
		if ((sc->status & MCD_ST_DOOROPEN) != 0)
			printf("%s: door open\n", sc->sc_dev.dv_xname);
		else if ((sc->status & MCD_ST_DSKIN) == 0)
			printf("%s: no disk present\n", sc->sc_dev.dv_xname);
		else if ((sc->status & MCD_ST_DSKCHNG) != 0)
			printf("%s: media change\n", sc->sc_dev.dv_xname);
		sc->flags &= ~MCDF_LOADED;
	}

	if ((sc->status & MCD_ST_AUDIOBSY) != 0)
		sc->audio_status = CD_AS_PLAY_IN_PROGRESS;
	else if (sc->audio_status == CD_AS_PLAY_IN_PROGRESS ||
		 sc->audio_status == CD_AS_AUDIO_INVALID)
		sc->audio_status = CD_AS_PLAY_COMPLETED;
}

int
mcd_send(sc, mbx, diskin)
	struct mcd_softc *sc;
	struct mcd_mbox *mbx;
	int diskin;
{
	int retry, i, error;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	
	if (sc->debug) {
		printf("%s: mcd_send: %d %02x", sc->sc_dev.dv_xname,
		    mbx->cmd.length, (u_int)mbx->cmd.opcode);
		for (i = 0; i < mbx->cmd.length; i++)
			printf(" %02x", (u_int)mbx->cmd.data.raw.data[i]);
		printf("\n");
	}

	for (retry = MCD_RETRIES; retry; retry--) {
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->cmd.opcode);
		for (i = 0; i < mbx->cmd.length; i++)
			bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->cmd.data.raw.data[i]);
		if ((error = mcd_getresult(sc, &mbx->res)) == 0)
			break;
		if (error == EINVAL)
			return error;
	}
	if (!retry)
		return error;
	if (diskin && (sc->flags & MCDF_LOADED) == 0)
		return EIO;

	return 0;
}

static int
bcd2bin(b)
	bcd_t b;
{

	return (b >> 4) * 10 + (b & 15);
}

static bcd_t
bin2bcd(b)
	int b;
{

	return ((b / 10) << 4) | (b % 10);
}

static void
hsg2msf(hsg, msf)
	int hsg;
	bcd_t *msf;
{

	hsg += 150;
	F_msf(msf) = bin2bcd(hsg % 75);
	hsg /= 75;
	S_msf(msf) = bin2bcd(hsg % 60);
	hsg /= 60;
	M_msf(msf) = bin2bcd(hsg);
}

static daddr_t
msf2hsg(msf, relative)
	bcd_t *msf;
	int relative;
{
	daddr_t blkno;

	blkno = bcd2bin(M_msf(msf)) * 75 * 60 +
		bcd2bin(S_msf(msf)) * 75 +
		bcd2bin(F_msf(msf));
	if (!relative)
		blkno -= 150;
	return blkno;
}

void
mcd_pseudointr(v)
	void *v;
{
	struct mcd_softc *sc = v;
	int s;

	s = splbio();
	(void) mcdintr(sc);
	splx(s);
}

/*
 * State machine to process read requests.
 * Initialize with MCD_S_BEGIN: calculate sizes, and set mode
 * MCD_S_WAITMODE: waits for status reply from set mode, set read command
 * MCD_S_WAITREAD: wait for read ready, read data.
 */
int
mcdintr(arg)
	void *arg;
{
	struct mcd_softc *sc = arg;
	struct mcd_mbx *mbx = &sc->mbx;
	struct buf *bp = mbx->bp;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	int i;
	u_char x;
	bcd_t msf[3];

	switch (mbx->state) {
	case MCD_S_IDLE:
		return 0;

	case MCD_S_BEGIN:
	tryagain:
		if (mbx->mode == sc->lastmode)
			goto firstblock;

		sc->lastmode = MCD_MD_UNKNOWN;
		bus_space_write_1(iot, ioh, MCD_COMMAND, MCD_CMDSETMODE);
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->mode);

		mbx->count = RDELAY_WAITMODE;
		mbx->state = MCD_S_WAITMODE;

	case MCD_S_WAITMODE:
		untimeout(mcd_pseudointr, sc);
		for (i = 20; i; i--) {
			x = bus_space_read_1(iot, ioh, MCD_XFER);
			if ((x & MCD_XF_STATUSUNAVAIL) == 0)
				break;
			delay(50);
		}
		if (i == 0)
			goto hold;
		sc->status = bus_space_read_1(iot, ioh, MCD_STATUS);
		mcd_setflags(sc);
		if ((sc->flags & MCDF_LOADED) == 0)
			goto changed;
		MCD_TRACE("doread: got WAITMODE delay=%d\n",
		    RDELAY_WAITMODE - mbx->count, 0, 0, 0);

		sc->lastmode = mbx->mode;

	firstblock:
		MCD_TRACE("doread: read blkno=%d for bp=0x%x\n", mbx->blkno,
		    bp, 0, 0);

		/* Build parameter block. */
		hsg2msf(mbx->blkno, msf);

		/* Send the read command. */
		bus_space_write_1(iot, ioh, MCD_COMMAND, sc->readcmd);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[0]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[1]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[2]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, 0);
		bus_space_write_1(iot, ioh, MCD_COMMAND, 0);
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->nblk);

		mbx->count = RDELAY_WAITREAD;
		mbx->state = MCD_S_WAITREAD;

	case MCD_S_WAITREAD:
		untimeout(mcd_pseudointr, sc);
	nextblock:
	loop:
		for (i = 20; i; i--) {
			x = bus_space_read_1(iot, ioh, MCD_XFER);
			if ((x & MCD_XF_DATAUNAVAIL) == 0)
				goto gotblock;
			if ((x & MCD_XF_STATUSUNAVAIL) == 0)
				break;
			delay(50);
		}
		if (i == 0)
			goto hold;
		sc->status = bus_space_read_1(iot, ioh, MCD_STATUS);
		mcd_setflags(sc);
		if ((sc->flags & MCDF_LOADED) == 0)
			goto changed;
#if 0
		printf("%s: got status byte %02x during read\n",
		    sc->sc_dev.dv_xname, (u_int)sc->status);
#endif
		goto loop;

	gotblock:
		MCD_TRACE("doread: got data delay=%d\n",
		    RDELAY_WAITREAD - mbx->count, 0, 0, 0);

		/* Data is ready. */
		bus_space_write_1(iot, ioh, MCD_CTL2, 0x04);	/* XXX */
		bus_space_read_multi_1(iot, ioh, MCD_RDATA,
		    bp->b_data + mbx->skip, mbx->sz);
		bus_space_write_1(iot, ioh, MCD_CTL2, 0x0c);	/* XXX */
		mbx->blkno += 1;
		mbx->skip += mbx->sz;
		if (--mbx->nblk > 0)
			goto nextblock;

		mbx->state = MCD_S_IDLE;

		/* Return buffer. */
		bp->b_resid = 0;
		disk_unbusy(&sc->sc_dk, bp->b_bcount);
		biodone(bp);

		mcdstart(sc);
		return 1;

	hold:
		if (mbx->count-- < 0) {
			printf("%s: timeout in state %d",
			    sc->sc_dev.dv_xname, mbx->state);
			goto readerr;
		}

#if 0
		printf("%s: sleep in state %d\n", sc->sc_dev.dv_xname,
		    mbx->state);
#endif
		timeout(mcd_pseudointr, sc, hz / 100);
		return -1;
	}

readerr:
	if (mbx->retry-- > 0) {
		printf("; retrying\n");
		goto tryagain;
	} else
		printf("; giving up\n");

changed:
	/* Invalidate the buffer. */
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount - mbx->skip;
	disk_unbusy(&sc->sc_dk, (bp->b_bcount - bp->b_resid));
	biodone(bp);

	mcdstart(sc);
	return -1;

#ifdef notyet
	printf("%s: unit timeout; resetting\n", sc->sc_dev.dv_xname);
	bus_space_write_1(iot, ioh, MCD_RESET, MCD_CMDRESET);
	delay(300000);
	(void) mcd_getstat(sc, 1);
	(void) mcd_getstat(sc, 1);
	/*sc->status &= ~MCD_ST_DSKCHNG; */
	sc->debug = 1; /* preventive set debug mode */
#endif
}

void
mcd_soft_reset(sc)
	struct mcd_softc *sc;
{

	sc->debug = 0;
	sc->flags = 0;
	sc->lastmode = MCD_MD_UNKNOWN;
	sc->lastupc = MCD_UPC_UNKNOWN;
	sc->audio_status = CD_AS_AUDIO_INVALID;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MCD_CTL2, 0x0c); /* XXX */
}

int
mcd_hard_reset(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;

	mcd_soft_reset(sc);

	mbx.cmd.opcode = MCD_CMDRESET;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 0);
}
	
int
mcd_setmode(sc, mode)
	struct mcd_softc *sc;
	int mode;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->lastmode == mode)
		return 0;
	if (sc->debug)
		printf("%s: setting mode to %d\n", sc->sc_dev.dv_xname, mode);
	sc->lastmode = MCD_MD_UNKNOWN;

	mbx.cmd.opcode = MCD_CMDSETMODE;
	mbx.cmd.length = sizeof(mbx.cmd.data.datamode);
	mbx.cmd.data.datamode.mode = mode;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->lastmode = mode;
	return 0;
}

int
mcd_setupc(sc, upc)
	struct mcd_softc *sc;
	int upc;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->lastupc == upc)
		return 0;
	if (sc->debug)
		printf("%s: setting upc to %d\n", sc->sc_dev.dv_xname, upc);
	sc->lastupc = MCD_UPC_UNKNOWN;

	mbx.cmd.opcode = MCD_CMDCONFIGDRIVE;
	mbx.cmd.length = sizeof(mbx.cmd.data.config) - 1;
	mbx.cmd.data.config.subcommand = MCD_CF_READUPC;
	mbx.cmd.data.config.data1 = upc;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->lastupc = upc;
	return 0;
}

int
mcd_toc_header(sc, th)
	struct mcd_softc *sc;
	struct ioc_toc_header *th;
{

	if (sc->debug)
		printf("%s: mcd_toc_header: reading toc header\n",
		    sc->sc_dev.dv_xname);

	th->len = msf2hsg(sc->volinfo.vol_msf, 0);
	th->starting_track = bcd2bin(sc->volinfo.trk_low);
	th->ending_track = bcd2bin(sc->volinfo.trk_high);

	return 0;
}

int
mcd_read_toc(sc)
	struct mcd_softc *sc;
{
	struct ioc_toc_header th;
	union mcd_qchninfo q;
	int error, trk, idx, retry;

	if ((error = mcd_toc_header(sc, &th)) != 0)
		return error;

	if ((error = mcd_stop(sc)) != 0)
		return error;

	if (sc->debug)
		printf("%s: read_toc: reading qchannel info\n",
		    sc->sc_dev.dv_xname);

	for (trk = th.starting_track; trk <= th.ending_track; trk++)
		sc->toc[trk].toc.idx_no = 0x00;
	trk = th.ending_track - th.starting_track + 1;
	for (retry = 300; retry && trk > 0; retry--) {
		if (mcd_getqchan(sc, &q, CD_TRACK_INFO) != 0)
			break;
		if (q.toc.trk_no != 0x00 || q.toc.idx_no == 0x00)
			continue;
		idx = bcd2bin(q.toc.idx_no);
		if (idx < MCD_MAXTOCS &&
		    sc->toc[idx].toc.idx_no == 0x00) {
			sc->toc[idx] = q;
			trk--;
		}
	}

	/* Inform the drive that we're finished so it turns off the light. */
	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	if (trk != 0)
		return EINVAL;

	/* Add a fake last+1 for mcd_playtracks(). */
	idx = th.ending_track + 1;
	sc->toc[idx].toc.control = sc->toc[idx-1].toc.control;
	sc->toc[idx].toc.addr_type = sc->toc[idx-1].toc.addr_type;
	sc->toc[idx].toc.trk_no = 0x00;
	sc->toc[idx].toc.idx_no = 0xaa;
	sc->toc[idx].toc.absolute_pos[0] = sc->volinfo.vol_msf[0];
	sc->toc[idx].toc.absolute_pos[1] = sc->volinfo.vol_msf[1];
	sc->toc[idx].toc.absolute_pos[2] = sc->volinfo.vol_msf[2];

	return 0;
}

int
mcd_toc_entries(sc, te)
	struct mcd_softc *sc;
	struct ioc_read_toc_entry *te;
{
	int len = te->data_len;
	struct ret_toc {
		struct ioc_toc_header header;
		struct cd_toc_entry entries[MCD_MAXTOCS];
	} data;
	u_char trk;
	daddr_t lba;
	int error, n;

	if (len > sizeof(data.entries) ||
	    len < sizeof(struct cd_toc_entry))
		return EINVAL;
	if (te->address_format != CD_MSF_FORMAT &&
	    te->address_format != CD_LBA_FORMAT)
		return EINVAL;

	/* Copy the TOC header. */
	if ((error = mcd_toc_header(sc, &data.header)) != 0)
		return error;

	/* Verify starting track. */
	trk = te->starting_track;
	if (trk == 0x00)
		trk = data.header.starting_track;
	else if (trk == 0xaa)
		trk = data.header.ending_track + 1;
	else if (trk < data.header.starting_track ||
		 trk > data.header.ending_track + 1)
		return EINVAL;

	/* Copy the TOC data. */
	for (n = 0; trk <= data.header.ending_track + 1; trk++) {
		if (sc->toc[trk].toc.idx_no == 0x00)
			continue;
		data.entries[n].control = sc->toc[trk].toc.control;
		data.entries[n].addr_type = sc->toc[trk].toc.addr_type;
		data.entries[n].track = bcd2bin(sc->toc[trk].toc.idx_no);
		switch (te->address_format) {
		case CD_MSF_FORMAT:
			data.entries[n].addr.addr[0] = 0;
			data.entries[n].addr.addr[1] = bcd2bin(sc->toc[trk].toc.absolute_pos[0]);
			data.entries[n].addr.addr[2] = bcd2bin(sc->toc[trk].toc.absolute_pos[1]);
			data.entries[n].addr.addr[3] = bcd2bin(sc->toc[trk].toc.absolute_pos[2]);
			break;
		case CD_LBA_FORMAT:
			lba = msf2hsg(sc->toc[trk].toc.absolute_pos, 0);
			data.entries[n].addr.addr[0] = lba >> 24;
			data.entries[n].addr.addr[1] = lba >> 16;
			data.entries[n].addr.addr[2] = lba >> 8;
			data.entries[n].addr.addr[3] = lba;
			break;
		}
		n++;
	}

	len = min(len, n * sizeof(struct cd_toc_entry));

	/* Copy the data back. */
	return copyout(&data.entries[0], te->data, len);
}

int
mcd_stop(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: mcd_stop: stopping play\n", sc->sc_dev.dv_xname);

	mbx.cmd.opcode = MCD_CMDSTOPAUDIO;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

int
mcd_getqchan(sc, q, qchn)
	struct mcd_softc *sc;
	union mcd_qchninfo *q;
	int qchn;
{
	struct mcd_mbox mbx;
	int error;

	if (qchn == CD_TRACK_INFO) {
		if ((error = mcd_setmode(sc, MCD_MD_TOC)) != 0)
			return error;
	} else {
		if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
			return error;
	}
	if (qchn == CD_MEDIA_CATALOG) {
		if ((error = mcd_setupc(sc, MCD_UPC_ENABLE)) != 0)
			return error;
	} else {
		if ((error = mcd_setupc(sc, MCD_UPC_DISABLE)) != 0)
			return error;
	}

	mbx.cmd.opcode = MCD_CMDGETQCHN;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.qchninfo);
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	*q = mbx.res.data.qchninfo;
	return 0;
}

int
mcd_read_subchannel(sc, ch)
	struct mcd_softc *sc;
	struct ioc_read_subchannel *ch;
{
	int len = ch->data_len;
	union mcd_qchninfo q;
	struct cd_sub_channel_info data;
	daddr_t lba;
	int error;

	if (sc->debug)
		printf("%s: subchan: af=%d df=%d\n", sc->sc_dev.dv_xname,
		    ch->address_format, ch->data_format);

	if (len > sizeof(data) ||
	    len < sizeof(struct cd_sub_channel_header))
		return EINVAL;
	if (ch->address_format != CD_MSF_FORMAT &&
	    ch->address_format != CD_LBA_FORMAT)
		return EINVAL;
	if (ch->data_format != CD_CURRENT_POSITION &&
	    ch->data_format != CD_MEDIA_CATALOG)
		return EINVAL;

	if ((error = mcd_getqchan(sc, &q, ch->data_format)) != 0)
		return error;

	data.header.audio_status = sc->audio_status;
	data.what.media_catalog.data_format = ch->data_format;

	switch (ch->data_format) {
	case CD_MEDIA_CATALOG:
		data.what.media_catalog.mc_valid = 1;
#if 0
		data.what.media_catalog.mc_number = 
#endif
		break;

	case CD_CURRENT_POSITION:
		data.what.position.track_number = bcd2bin(q.current.trk_no);
		data.what.position.index_number = bcd2bin(q.current.idx_no);
		switch (ch->address_format) {
		case CD_MSF_FORMAT:
			data.what.position.reladdr.addr[0] = 0;
			data.what.position.reladdr.addr[1] = bcd2bin(q.current.relative_pos[0]);
			data.what.position.reladdr.addr[2] = bcd2bin(q.current.relative_pos[1]);
			data.what.position.reladdr.addr[3] = bcd2bin(q.current.relative_pos[2]);
			data.what.position.absaddr.addr[0] = 0;
			data.what.position.absaddr.addr[1] = bcd2bin(q.current.absolute_pos[0]);
			data.what.position.absaddr.addr[2] = bcd2bin(q.current.absolute_pos[1]);
			data.what.position.absaddr.addr[3] = bcd2bin(q.current.absolute_pos[2]);
			break;
		case CD_LBA_FORMAT:
			lba = msf2hsg(q.current.relative_pos, 1);
			/*
			 * Pre-gap has index number of 0, and decreasing MSF
			 * address.  Must be converted to negative LBA, per
			 * SCSI spec.
			 */
			if (data.what.position.index_number == 0x00)
				lba = -lba;
			data.what.position.reladdr.addr[0] = lba >> 24;
			data.what.position.reladdr.addr[1] = lba >> 16;
			data.what.position.reladdr.addr[2] = lba >> 8;
			data.what.position.reladdr.addr[3] = lba;
			lba = msf2hsg(q.current.absolute_pos, 0);
			data.what.position.absaddr.addr[0] = lba >> 24;
			data.what.position.absaddr.addr[1] = lba >> 16;
			data.what.position.absaddr.addr[2] = lba >> 8;
			data.what.position.absaddr.addr[3] = lba;
			break;
		}
		break;
	}

	return copyout(&data, ch->data, len);
}

int
mcd_playtracks(sc, p)
	struct mcd_softc *sc;
	struct ioc_play_track *p;
{
	struct mcd_mbox mbx;
	int a = p->start_track;
	int z = p->end_track;
	int error;

	if (sc->debug)
		printf("%s: playtracks: from %d:%d to %d:%d\n",
		    sc->sc_dev.dv_xname,
		    a, p->start_index, z, p->end_index);

	if (a < bcd2bin(sc->volinfo.trk_low) ||
	    a > bcd2bin(sc->volinfo.trk_high) ||
	    a > z ||
	    z < bcd2bin(sc->volinfo.trk_low) ||
	    z > bcd2bin(sc->volinfo.trk_high))
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	mbx.cmd.data.play.start_msf[0] = sc->toc[a].toc.absolute_pos[0];
	mbx.cmd.data.play.start_msf[1] = sc->toc[a].toc.absolute_pos[1];
	mbx.cmd.data.play.start_msf[2] = sc->toc[a].toc.absolute_pos[2];
	mbx.cmd.data.play.end_msf[0] = sc->toc[z+1].toc.absolute_pos[0];
	mbx.cmd.data.play.end_msf[1] = sc->toc[z+1].toc.absolute_pos[1];
	mbx.cmd.data.play.end_msf[2] = sc->toc[z+1].toc.absolute_pos[2];
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_playmsf(sc, p)
	struct mcd_softc *sc;
	struct ioc_play_msf *p;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: playmsf: from %d:%d.%d to %d:%d.%d\n",
		    sc->sc_dev.dv_xname,
		    p->start_m, p->start_s, p->start_f,
		    p->end_m, p->end_s, p->end_f);

	if ((p->start_m * 60 * 75 + p->start_s * 75 + p->start_f) >=
	    (p->end_m * 60 * 75 + p->end_s * 75 + p->end_f))
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	mbx.cmd.data.play.start_msf[0] = bin2bcd(p->start_m);
	mbx.cmd.data.play.start_msf[1] = bin2bcd(p->start_s);
	mbx.cmd.data.play.start_msf[2] = bin2bcd(p->start_f);
	mbx.cmd.data.play.end_msf[0] = bin2bcd(p->end_m);
	mbx.cmd.data.play.end_msf[1] = bin2bcd(p->end_s);
	mbx.cmd.data.play.end_msf[2] = bin2bcd(p->end_f);
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_playblocks(sc, p)
	struct mcd_softc *sc;
	struct ioc_play_blocks *p;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: playblocks: blkno %d length %d\n",
		    sc->sc_dev.dv_xname, p->blk, p->len);

	if (p->blk > sc->disksize || p->len > sc->disksize ||
	    (p->blk + p->len) > sc->disksize)
		return 0;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	hsg2msf(p->blk, mbx.cmd.data.play.start_msf);
	hsg2msf(p->blk + p->len, mbx.cmd.data.play.end_msf);
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_pause(sc)
	struct mcd_softc *sc;
{
	union mcd_qchninfo q;
	int error;

	/* Verify current status. */
	if (sc->audio_status != CD_AS_PLAY_IN_PROGRESS)	{
		printf("%s: pause: attempted when not playing\n",
		    sc->sc_dev.dv_xname);
		return EINVAL;
	}

	/* Get the current position. */
	if ((error = mcd_getqchan(sc, &q, CD_CURRENT_POSITION)) != 0)
		return error;

	/* Copy it into lastpb. */
	sc->lastpb.data.seek.start_msf[0] = q.current.absolute_pos[0];
	sc->lastpb.data.seek.start_msf[1] = q.current.absolute_pos[1];
	sc->lastpb.data.seek.start_msf[2] = q.current.absolute_pos[2];

	/* Stop playing. */
	if ((error = mcd_stop(sc)) != 0)
		return error;

	/* Set the proper status and exit. */
	sc->audio_status = CD_AS_PLAY_PAUSED;
	return 0;
}

int
mcd_resume(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;
	int error;

	if (sc->audio_status != CD_AS_PLAY_PAUSED)
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd = sc->lastpb;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_eject(sc)
	struct mcd_softc *sc;
{
	struct mcd_mbox mbx;

	mbx.cmd.opcode = MCD_CMDEJECTDISK;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 0);
}

int
mcd_setlock(sc, mode)
	struct mcd_softc *sc;
	int mode;
{
	struct mcd_mbox mbx;
	
	mbx.cmd.opcode = MCD_CMDSETLOCK;
	mbx.cmd.length = sizeof(mbx.cmd.data.lockmode);
	mbx.cmd.data.lockmode.mode = mode;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}
