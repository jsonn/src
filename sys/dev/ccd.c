/*	$NetBSD: ccd.c,v 1.42.4.2 1997/10/14 10:22:06 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 */

/*
 * "Concatenated" disk driver.
 *
 * Dynamic configuration and disklabel support by:
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 *	Numerical Aerodynamic Simulation Facility
 *	Mail Stop 258-6
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 *
 * Mirroring support based on code written by Satoshi Asami
 * and Nisha Talagala.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/conf.h>

#include <dev/ccdvar.h>

#if defined(CCDDEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifdef DEBUG
#define CCDB_FOLLOW	0x01
#define CCDB_INIT	0x02
#define CCDB_IO		0x04
#define CCDB_LABEL	0x08
#define CCDB_VNODE	0x10
int ccddebug = 0x00;
#endif

#define	ccdunit(x)	DISKUNIT(x)

struct ccdbuf {
	struct buf	cb_buf;		/* new I/O buf */
	struct buf	*cb_obp;	/* ptr. to original I/O buf */
	int		cb_unit;	/* target unit */
	int		cb_comp;	/* target component */
	int		cb_flags;	/* misc. flags */
	LIST_ENTRY(ccdbuf) cb_list;	/* entry on freelist */
};

/* cb_flags */
#define CBF_MIRROR	0x01		/* we're for a mirror component */

/*
 * Number of freelist buffers per component.  Overridable in kernel
 * config file and patchable.
 */
#ifndef CCDNBUF
#define	CCDNBUF		8
#endif
int	ccdnbuf = CCDNBUF;

/*
 * XXX Is it OK to wait here?
 * XXX maybe set up a timeout when we hit some lowater?
 * XXX    --thorpej
 */
#define	CCDGETBUF(cs, cbp)	do {					\
		(cs)->sc_ngetbuf++;					\
		if (((cbp) = (cs)->sc_freelist.lh_first) != NULL) {	\
			LIST_REMOVE((cbp), cb_list);			\
			(cs)->sc_freecount--;				\
		} else {						\
			(cs)->sc_nmisses++;				\
			MALLOC((cbp), struct ccdbuf *,			\
			    sizeof(struct ccdbuf), M_DEVBUF, M_WAITOK);	\
		}							\
	} while (0)

#define	CCDPUTBUF(cs, cbp)	do {					\
		if ((cs)->sc_freecount == (cs)->sc_hiwat) {		\
			FREE((cbp), M_DEVBUF);				\
		} else {						\
			LIST_INSERT_HEAD(&(cs)->sc_freelist, (cbp), cb_list); \
			(cs)->sc_freecount++;				\
		}							\
	} while (0)

#define CCDLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), ccdunit((dev)), RAW_PART))

/* called by main() at boot time */
void	ccdattach __P((int));

/* called by biodone() at interrupt time */
void	ccdiodone __P((struct buf *));
int	ccdsize __P((dev_t));

static	void ccdstart __P((struct ccd_softc *, struct buf *));
static	void ccdinterleave __P((struct ccd_softc *, int));
static	void ccdintr __P((struct ccd_softc *, struct buf *));
static	int ccdinit __P((struct ccddevice *, char **, struct proc *));
static	int ccdlookup __P((char *, struct proc *p, struct vnode **));
static	void ccdbuffer __P((struct ccd_softc *, struct buf *,
		daddr_t, caddr_t, long, struct ccdbuf **));
static	void ccdgetdefaultlabel __P((struct ccd_softc *, struct disklabel *));
static	void ccdgetdisklabel __P((dev_t));
static	void ccdmakedisklabel __P((struct ccd_softc *));
static	int ccdlock __P((struct ccd_softc *));
static	void ccdunlock __P((struct ccd_softc *));

#ifdef DEBUG
static	void printiinfo __P((struct ccdiinfo *));
#endif

/* Non-private for the benefit of libkvm. */
struct	ccd_softc *ccd_softc;
struct	ccddevice *ccddevs;
int	numccd = 0;

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later.
 */
void
ccdattach(num)
	int num;
{
	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("ccdattach: count <= 0");
#endif
		return;
	}

	ccd_softc = (struct ccd_softc *)malloc(num * sizeof(struct ccd_softc),
	    M_DEVBUF, M_NOWAIT);
	ccddevs = (struct ccddevice *)malloc(num * sizeof(struct ccddevice),
	    M_DEVBUF, M_NOWAIT);
	if ((ccd_softc == NULL) || (ccddevs == NULL)) {
		printf("WARNING: no memory for concatenated disks\n");
		if (ccd_softc != NULL)
			free(ccd_softc, M_DEVBUF);
		if (ccddevs != NULL)
			free(ccddevs, M_DEVBUF);
		return;
	}
	numccd = num;
	bzero(ccd_softc, num * sizeof(struct ccd_softc));
	bzero(ccddevs, num * sizeof(struct ccddevice));
}

static int
ccdinit(ccd, cpaths, p)
	struct ccddevice *ccd;
	char **cpaths;
	struct proc *p;
{
	register struct ccd_softc *cs = &ccd_softc[ccd->ccd_unit];
	register struct ccdcinfo *ci = NULL;
	register size_t size;
	register int ix;
	struct vnode *vp;
	struct vattr va;
	size_t minsize;
	int maxsecsize;
	struct partinfo dpart;
	struct ccdgeom *ccg = &cs->sc_geom;
	char tmppath[MAXPATHLEN];
	struct ccdbuf *cbp;
	int error;

#ifdef DEBUG
	if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
		printf("ccdinit: unit %d\n", ccd->ccd_unit);
#endif

	cs->sc_size = 0;
	cs->sc_ileave = ccd->ccd_interleave;
	cs->sc_nccdisks = ccd->ccd_ndev;
	sprintf(cs->sc_xname, "ccd%d", ccd->ccd_unit);	/* XXX */

	/* Allocate space for the component info. */
	cs->sc_cinfo = malloc(cs->sc_nccdisks * sizeof(struct ccdcinfo),
	    M_DEVBUF, M_WAITOK);

	/*
	 * Verify that each component piece exists and record
	 * relevant information about it.
	 */
	maxsecsize = 0;
	minsize = 0;
	for (ix = 0; ix < cs->sc_nccdisks; ix++) {
		vp = ccd->ccd_vpp[ix];
		ci = &cs->sc_cinfo[ix];
		ci->ci_vp = vp;

		/*
		 * Copy in the pathname of the component.
		 */
		bzero(tmppath, sizeof(tmppath));	/* sanity */
		error = copyinstr(cpaths[ix], tmppath,
		    MAXPATHLEN, &ci->ci_pathlen);
		if (error) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: can't copy path, error = %d\n",
				    cs->sc_xname, error);
#endif
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_path = malloc(ci->ci_pathlen, M_DEVBUF, M_WAITOK);
		bcopy(tmppath, ci->ci_path, ci->ci_pathlen);

		/*
		 * XXX: Cache the component's dev_t.
		 */
		if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: %s: getattr failed %s = %d\n",
				    cs->sc_xname, ci->ci_path,
				    "error", error);
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_dev = va.va_rdev;

		/*
		 * Get partition information for the component.
		 */
		error = VOP_IOCTL(vp, DIOCGPART, (caddr_t)&dpart,
		    FREAD, p->p_ucred, p);
		if (error) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				 printf("%s: %s: ioctl failed, error = %d\n",
				     cs->sc_xname, ci->ci_path, error);
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		if (dpart.part->p_fstype == FS_BSDFFS) {
			maxsecsize =
			    ((dpart.disklab->d_secsize > maxsecsize) ?
			    dpart.disklab->d_secsize : maxsecsize);
			size = dpart.part->p_size;
		} else {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: %s: incorrect partition type\n",
				    cs->sc_xname, ci->ci_path);
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EFTYPE);
		}

		/*
		 * Calculate the size, truncating to an interleave
		 * boundary if necessary.
		 */
		if (cs->sc_ileave > 1)
			size -= size % cs->sc_ileave;

		if (size == 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("%s: %s: size == 0\n",
				    cs->sc_xname, ci->ci_path);
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (ENODEV);
		}

		if (minsize == 0 || size < minsize)
			minsize = size;
		ci->ci_size = size;
		cs->sc_size += size;
	}

	/*
	 * Don't allow the interleave to be smaller than
	 * the biggest component sector.
	 */
	if ((cs->sc_ileave > 0) &&
	    (cs->sc_ileave < (maxsecsize / DEV_BSIZE))) {
#ifdef DEBUG
		if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("%s: interleave must be at least %d\n",
			    cs->sc_xname, (maxsecsize / DEV_BSIZE));
#endif
		free(ci->ci_path, M_DEVBUF);
		free(cs->sc_cinfo, M_DEVBUF);
		return (EINVAL);
	}

	/*
	 * Mirroring support requires uniform interleave and
	 * and even number of components.
	 */
	if (ccd->ccd_flags & CCDF_MIRROR) {
		ccd->ccd_flags |= CCDF_UNIFORM;
		if (cs->sc_ileave == 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("%s: mirroring requires interleave\n",
			    cs->sc_xname);
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EINVAL);
		}
		if (cs->sc_nccdisks % 2) { 
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("%s: mirroring requires even # of components\n",
			    cs->sc_xname); 
#endif
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EINVAL);
		}
	}

	/*
	 * If uniform interleave is desired set all sizes to that of
	 * the smallest component.
	 */
	if (ccd->ccd_flags & CCDF_UNIFORM) {
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			ci->ci_size = minsize;

		if (ccd->ccd_flags & CCDF_MIRROR)
			cs->sc_size = (cs->sc_nccdisks / 2) * minsize;
		else
			cs->sc_size = cs->sc_nccdisks * minsize;
	}

	/*
	 * Construct the interleave table.
	 */
	ccdinterleave(cs, ccd->ccd_unit);

	/*
	 * Create pseudo-geometry based on 1MB cylinders.  It's
	 * pretty close.
	 */
	ccg->ccg_secsize = DEV_BSIZE;
	ccg->ccg_ntracks = 1;
	ccg->ccg_nsectors = 1024 * (1024 / ccg->ccg_secsize);
	ccg->ccg_ncylinders = cs->sc_size / ccg->ccg_nsectors;

	/*
	 * Allocate the component buffer header freelist.  We allocate
	 * ccdnbuf buffers per component.
	 */
	LIST_INIT(&cs->sc_freelist);
	cs->sc_hiwat = cs->sc_nccdisks * ccdnbuf;
	cs->sc_freecount = cs->sc_hiwat;
	for (ix = 0; ix < cs->sc_hiwat; ix++) {
		MALLOC(cbp, struct ccdbuf *, sizeof(struct ccdbuf),
		    M_DEVBUF, M_WAITOK);
		LIST_INSERT_HEAD(&cs->sc_freelist, cbp, cb_list);
	}

	/* Reset statistics. */
	cs->sc_nmisses = 0;
	cs->sc_ngetbuf = 0;

	cs->sc_flags |= CCDF_INITED;
	cs->sc_cflags = ccd->ccd_flags;	/* So we can find out later... */
	cs->sc_unit = ccd->ccd_unit;

	return (0);
}

static void
ccdinterleave(cs, unit)
	register struct ccd_softc *cs;
	int unit;
{
	register struct ccdcinfo *ci, *smallci;
	register struct ccdiinfo *ii;
	register daddr_t bn, lbn;
	register int ix;
	u_long size;

#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printf("ccdinterleave(%p): ileave %d\n", cs, cs->sc_ileave);
#endif
	/*
	 * Allocate an interleave table.
	 * Chances are this is too big, but we don't care.
	 */
	size = (cs->sc_nccdisks + 1) * sizeof(struct ccdiinfo);
	cs->sc_itable = (struct ccdiinfo *)malloc(size, M_DEVBUF, M_WAITOK);
	bzero((caddr_t)cs->sc_itable, size);

	/*
	 * Trivial case: no interleave (actually interleave of disk size).
	 * Each table entry represents a single component in its entirety.
	 */
	if (cs->sc_ileave == 0) {
		bn = 0;
		ii = cs->sc_itable;

		for (ix = 0; ix < cs->sc_nccdisks; ix++) {
			/* Allocate space for ii_index. */
			ii->ii_index = malloc(sizeof(int), M_DEVBUF, M_WAITOK);
			ii->ii_ndisk = 1;
			ii->ii_startblk = bn;
			ii->ii_startoff = 0;
			ii->ii_index[0] = ix;
			bn += cs->sc_cinfo[ix].ci_size;
			ii++;
		}
		ii->ii_ndisk = 0;
#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			printiinfo(cs->sc_itable);
#endif
		return;
	}

	/*
	 * The following isn't fast or pretty; it doesn't have to be.
	 */
	size = 0;
	bn = lbn = 0;
	for (ii = cs->sc_itable; ; ii++) {
		/* Allocate space for ii_index. */
		ii->ii_index = malloc((sizeof(int) * cs->sc_nccdisks),
		    M_DEVBUF, M_WAITOK);

		/*
		 * Locate the smallest of the remaining components
		 */
		smallci = NULL;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size > size &&
			    (smallci == NULL ||
			     ci->ci_size < smallci->ci_size))
				smallci = ci;

		/*
		 * Nobody left, all done
		 */
		if (smallci == NULL) {
			ii->ii_ndisk = 0;
			break;
		}

		/*
		 * Record starting logical block and component offset
		 */
		ii->ii_startblk = bn / cs->sc_ileave;
		ii->ii_startoff = lbn;

		/*
		 * Determine how many disks take part in this interleave
		 * and record their indices.
		 */
		ix = 0;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size >= smallci->ci_size)
				ii->ii_index[ix++] = ci - cs->sc_cinfo;
		ii->ii_ndisk = ix;
		bn += ix * (smallci->ci_size - size);
		lbn = smallci->ci_size / cs->sc_ileave;
		size = smallci->ci_size;
	}
#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printiinfo(cs->sc_itable);
#endif
}

/* ARGSUSED */
int
ccdopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	struct disklabel *lp;
	int error = 0, part, pmask;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdopen(0x%x, 0x%x)\n", dev, flags);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((error = ccdlock(cs)) != 0)
		return (error);

	lp = cs->sc_dkdev.dk_label;

	part = DISKPART(dev);
	pmask = (1 << part);

	/*
	 * If we're initialized, check to see if there are any other
	 * open partitions.  If not, then it's safe to update
	 * the in-core disklabel.
	 */
	if ((cs->sc_flags & CCDF_INITED) && (cs->sc_dkdev.dk_openmask == 0))
		ccdgetdisklabel(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART) {
		if (((cs->sc_flags & CCDF_INITED) == 0) ||
		    ((part >= lp->d_npartitions) ||
		     (lp->d_partitions[part].p_fstype == FS_UNUSED))) {
			error = ENXIO;
			goto done;
		}
	}

	/* Prevent our unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

 done:
	ccdunlock(cs);
	return (error);
}

/* ARGSUSED */
int
ccdclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	int error = 0, part;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdclose(0x%x, 0x%x)\n", dev, flags);
#endif

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((error = ccdlock(cs)) != 0)
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

	ccdunlock(cs);
	return (0);
}

void
ccdstrategy(bp)
	register struct buf *bp;
{
	register int unit = ccdunit(bp->b_dev);
	register struct ccd_softc *cs = &ccd_softc[unit];
	register int s;
	int wlabel;
	struct disklabel *lp;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdstrategy(%p): unit %d\n", bp, unit);
#endif
	if ((cs->sc_flags & CCDF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* If it's a nil transfer, wake up the top half now. */
	if (bp->b_bcount == 0)
		goto done;

	lp = cs->sc_dkdev.dk_label;

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.
	 */
	wlabel = cs->sc_flags & (CCDF_WLABEL|CCDF_LABELLING);
	if (DISKPART(bp->b_dev) != RAW_PART)
		if (bounds_check_with_label(bp, lp, wlabel) <= 0)
			goto done;

	bp->b_resid = bp->b_bcount;

	/*
	 * "Start" the unit.
	 */
	s = splbio();
	ccdstart(cs, bp);
	splx(s);
	return;
done:
	biodone(bp);
}

static void
ccdstart(cs, bp)
	register struct ccd_softc *cs;
	register struct buf *bp;
{
	register long bcount, rcount;
	struct ccdbuf *cbp[4];
	caddr_t addr;
	daddr_t bn;
	struct partition *pp;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdstart(%p, %p)\n", cs, bp);
#endif

	/* Instrumentation. */
	disk_busy(&cs->sc_dkdev);

	/*
	 * Translate the partition-relative block number to an absolute.
	 */
	bn = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &cs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)];
		bn += pp->p_offset;
	}

	/*
	 * Allocate component buffers and fire off the requests
	 */
	addr = bp->b_data;
	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		ccdbuffer(cs, bp, bn, addr, bcount, cbp);
		rcount = cbp[0]->cb_buf.b_bcount;
		if ((cbp[0]->cb_buf.b_flags & B_READ) == 0)
			cbp[0]->cb_buf.b_vp->v_numoutput++;
		VOP_STRATEGY(&cbp[0]->cb_buf);

		/*
		 * Mirror requires additional write.
		 */
		if ((cs->sc_cflags & CCDF_MIRROR) &&
		    ((cbp[0]->cb_buf.b_flags & B_READ) == 0)) {
			cbp[1]->cb_buf.b_vp->v_numoutput++;
			VOP_STRATEGY(&cbp[1]->cb_buf);
		}

		bn += btodb(rcount);
		addr += rcount;
	}
}

/*
 * Build a component buffer header.
 */
static void
ccdbuffer(cs, bp, bn, addr, bcount, cbpp)
	register struct ccd_softc *cs;
	struct buf *bp;
	daddr_t bn;
	caddr_t addr;
	long bcount;
	struct ccdbuf **cbpp;
{
	register struct ccdcinfo *ci, *ci2 = NULL;
	register struct ccdbuf *cbp;
	register daddr_t cbn, cboff;
	int ccdisk;

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf("ccdbuffer(%p, %p, %d, %p, %ld)\n",
		       cs, bp, bn, addr, bcount);
#endif
	/*
	 * Determine which component bn falls in.
	 */
	cbn = bn;
	cboff = 0;

	/*
	 * Serially concatenated
	 */
	if (cs->sc_ileave == 0) {
		register daddr_t sblk;

		sblk = 0;
		for (ccdisk = 0, ci = &cs->sc_cinfo[ccdisk];
		    cbn >= sblk + ci->ci_size;
		    ccdisk++, ci = &cs->sc_cinfo[ccdisk])
			sblk += ci->ci_size;
		cbn -= sblk;
	}
	/*
	 * Interleaved
	 */
	else {
		register struct ccdiinfo *ii;
		int off;

		cboff = cbn % cs->sc_ileave;
		cbn /= cs->sc_ileave;
		for (ii = cs->sc_itable; ii->ii_ndisk; ii++)
			if (ii->ii_startblk > cbn)
				break;
		ii--;
		off = cbn - ii->ii_startblk;
		if (ii->ii_ndisk == 1) {
			ccdisk = ii->ii_index[0];
			cbn = ii->ii_startoff + off;
		} else {
			if (cs->sc_cflags & CCDF_MIRROR) {
				ccdisk =
				    ii->ii_index[off % (ii->ii_ndisk / 2)];
				cbn = ii->ii_startoff +
				    (off / (ii->ii_ndisk / 2));
				/* Mirrored data */
				ci2 =
				    &cs->sc_cinfo[ccdisk + (ii->ii_ndisk / 2)];
			} else {
				/* Normal case. */
				ccdisk = ii->ii_index[off % ii->ii_ndisk];
				cbn = ii->ii_startoff + off / ii->ii_ndisk;
			}
		}
		cbn *= cs->sc_ileave;
		ci = &cs->sc_cinfo[ccdisk];
	}

	/*
	 * Fill in the component buf structure.
	 */
	CCDGETBUF(cs, cbp);
	cbp->cb_flags = 0;
	cbp->cb_buf.b_flags = bp->b_flags | B_CALL;
	cbp->cb_buf.b_iodone = ccdiodone;
	cbp->cb_buf.b_proc = bp->b_proc;
	cbp->cb_buf.b_dev = ci->ci_dev;		/* XXX */
	cbp->cb_buf.b_blkno = cbn + cboff;
	cbp->cb_buf.b_data = addr;
	cbp->cb_buf.b_vp = ci->ci_vp;
	if (cs->sc_ileave == 0)
		cbp->cb_buf.b_bcount = dbtob(ci->ci_size - cbn);
	else
		cbp->cb_buf.b_bcount = dbtob(cs->sc_ileave - cboff);
	if (cbp->cb_buf.b_bcount > bcount)
		cbp->cb_buf.b_bcount = bcount;

	/*
	 * context for ccdiodone
	 */
	cbp->cb_obp = bp;
	cbp->cb_unit = cs->sc_unit;
	cbp->cb_comp = ccdisk;

	/* First buffer is dealt with. */
	cbpp[0] = cbp;

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf(" dev 0x%x(u%d): cbp %p bn %d addr %p bcnt %ld\n",
		    ci->ci_dev, ci-cs->sc_cinfo, cbp, cbp->cb_buf.b_blkno,
		    cbp->cb_buf.b_data, cbp->cb_buf.b_bcount);
#endif

	/*
	 * Mirrors have an additional write operation that is nearly
	 * identical to the first.
	 */
	if ((cs->sc_cflags & CCDF_MIRROR) &&
	    ((cbp->cb_buf.b_flags & B_READ) == 0)) {
		CCDGETBUF(cs, cbp);
		*cbp = *cbpp[0];
		cbp->cb_flags = CBF_MIRROR;
		cbp->cb_buf.b_dev = ci2->ci_dev;	/* XXX */
		cbp->cb_buf.b_vp = ci2->ci_vp;
		cbp->cb_comp = ci2 - cs->sc_cinfo;
		cbpp[1] = cbp;
	}
}

static void
ccdintr(cs, bp)
	register struct ccd_softc *cs;
	register struct buf *bp;
{

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdintr(%p, %p)\n", cs, bp);
#endif
	/*
	 * Request is done for better or worse, wakeup the top half.
	 */
	if (bp->b_flags & B_ERROR)
		bp->b_resid = bp->b_bcount;
	disk_unbusy(&cs->sc_dkdev, (bp->b_bcount - bp->b_resid));
	biodone(bp);
}

/*
 * Called at interrupt time.
 * Mark the component as done and if all components are done,
 * take a ccd interrupt.
 */
void
ccdiodone(vbp)
	struct buf *vbp;
{
	struct ccdbuf *cbp = (struct ccdbuf *) vbp;
	register struct buf *bp = cbp->cb_obp;
	register int unit = cbp->cb_unit;
	struct ccd_softc *cs = &ccd_softc[unit];
	int count, cbflags, s;
	char *comptype;

	s = splbio();
#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdiodone(%p)\n", cbp);
	if (ccddebug & CCDB_IO) {
		if (cbp->cb_flags & CBF_MIRROR)
			printf("ccdiodone: mirror component\n");
		else
			printf("ccdiodone: bp %p bcount %ld resid %ld\n",
			       bp, bp->b_bcount, bp->b_resid);
		printf(" dev 0x%x(u%d), cbp %p bn %d addr %p bcnt %ld\n",
		       cbp->cb_buf.b_dev, cbp->cb_comp, cbp,
		       cbp->cb_buf.b_blkno, cbp->cb_buf.b_data,
		       cbp->cb_buf.b_bcount);
	}
#endif

	if (cbp->cb_buf.b_flags & B_ERROR) {
		if (cbp->cb_flags & CBF_MIRROR)
			comptype = " (mirror)";
		else {
			bp->b_flags |= B_ERROR;
			bp->b_error = cbp->cb_buf.b_error ?
			    cbp->cb_buf.b_error : EIO;
			comptype = "";
		}

		printf("%s: error %d on component %d%s\n",
		       cs->sc_xname, bp->b_error, cbp->cb_comp, comptype);
	}
	count = cbp->cb_buf.b_bcount;
	cbflags = cbp->cb_flags;
	CCDPUTBUF(cs, cbp);

	/*
	 * If all done, "interrupt".
	 *
	 * Note that mirror component buffers aren't counted against
	 * the original I/O buffer.
	 */
	if ((cbflags & CBF_MIRROR) == 0) {
		bp->b_resid -= count;
		if (bp->b_resid < 0)
			panic("ccdiodone: count");
		if (bp->b_resid == 0)
			ccdintr(&ccd_softc[unit], bp);
	}
	splx(s);
}

/* ARGSUSED */
int
ccdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdread(0x%x, %p)\n", dev, uio);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	/*
	 * XXX: It's not clear that using minphys() is completely safe,
	 * in particular, for raw I/O.  Underlying devices might have some
	 * non-obvious limits, because of the copy to user-space.
	 */
	return (physio(ccdstrategy, NULL, dev, B_READ, minphys, uio));
}

/* ARGSUSED */
int
ccdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdwrite(0x%x, %p)\n", dev, uio);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	/*
	 * XXX: It's not clear that using minphys() is completely safe,
	 * in particular, for raw I/O.  Underlying devices might have some
	 * non-obvious limits, because of the copy to user-space.
	 */
	return (physio(ccdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
ccdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = ccdunit(dev);
	int i, j, lookedup = 0, error = 0;
	int part, pmask;
	struct ccd_softc *cs;
	struct ccd_ioctl *ccio = (struct ccd_ioctl *)data;
	struct ccddevice ccd;
	struct ccdbuf *cbp;
	char **cpp;
	struct vnode **vpp;

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	bzero(&ccd, sizeof(ccd));

	/* Must be open for writes for these commands... */
	switch (cmd) {
	case CCDIOCSET:
	case CCDIOCCLR:
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	/* Must be initialized for these... */
	switch (cmd) {
	case CCDIOCCLR:
	case DIOCGDINFO:
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCGPART:
	case DIOCWLABEL:
	case DIOCGDEFLABEL:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);
	}

	switch (cmd) {
	case CCDIOCSET:
		if (cs->sc_flags & CCDF_INITED)
			return (EBUSY);

		if ((error = ccdlock(cs)) != 0)
			return (error);

		/* Fill in some important bits. */
		ccd.ccd_unit = unit;
		ccd.ccd_interleave = ccio->ccio_ileave;
		ccd.ccd_flags = ccio->ccio_flags & CCDF_USERMASK;

		/*
		 * Allocate space for and copy in the array of
		 * componet pathnames and device numbers.
		 */
		cpp = malloc(ccio->ccio_ndisks * sizeof(char *),
		    M_DEVBUF, M_WAITOK);
		vpp = malloc(ccio->ccio_ndisks * sizeof(struct vnode *),
		    M_DEVBUF, M_WAITOK);

		error = copyin((caddr_t)ccio->ccio_disks, (caddr_t)cpp,
		    ccio->ccio_ndisks * sizeof(char **));
		if (error) {
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			for (i = 0; i < ccio->ccio_ndisks; ++i)
				printf("ccdioctl: component %d: 0x%p\n",
				    i, cpp[i]);
#endif

		for (i = 0; i < ccio->ccio_ndisks; ++i) {
#ifdef DEBUG
			if (ccddebug & CCDB_INIT)
				printf("ccdioctl: lookedup = %d\n", lookedup);
#endif
			if ((error = ccdlookup(cpp[i], p, &vpp[i])) != 0) {
				for (j = 0; j < lookedup; ++j)
					(void)vn_close(vpp[j], FREAD|FWRITE,
					    p->p_ucred, p);
				free(vpp, M_DEVBUF);
				free(cpp, M_DEVBUF);
				ccdunlock(cs);
				return (error);
			}
			++lookedup;
		}
		ccd.ccd_cpp = cpp;
		ccd.ccd_vpp = vpp;
		ccd.ccd_ndev = ccio->ccio_ndisks;

		/*
		 * Initialize the ccd.  Fills in the softc for us.
		 */
		if ((error = ccdinit(&ccd, cpp, p)) != 0) {
			for (j = 0; j < lookedup; ++j)
				(void)vn_close(vpp[j], FREAD|FWRITE,
				    p->p_ucred, p);
			bzero(&ccd_softc[unit], sizeof(struct ccd_softc));
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

		/*
		 * The ccd has been successfully initialized, so
		 * we can place it into the array.  Don't try to
		 * read the disklabel until the disk has been attached,
		 * because space for the disklabel is allocated
		 * in disk_attach();
		 */
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));
		ccio->ccio_unit = unit;
		ccio->ccio_size = cs->sc_size;

		/* Attach the disk. */
		cs->sc_dkdev.dk_name = cs->sc_xname;
		disk_attach(&cs->sc_dkdev);

		/* Try and read the disklabel. */
		ccdgetdisklabel(dev);

		ccdunlock(cs);

		break;

	case CCDIOCCLR:
		if ((error = ccdlock(cs)) != 0)
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((cs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((cs->sc_dkdev.dk_bopenmask & pmask) &&
		    (cs->sc_dkdev.dk_copenmask & pmask))) {
			ccdunlock(cs);
			return (EBUSY);
		}

		/*
		 * Free ccd_softc information and clear entry.
		 */

		/* Close the components and free their pathnames. */
		for (i = 0; i < cs->sc_nccdisks; ++i) {
			/*
			 * XXX: this close could potentially fail and
			 * cause Bad Things.  Maybe we need to force
			 * the close to happen?
			 */
#ifdef DEBUG
			if (ccddebug & CCDB_VNODE)
				vprint("CCDIOCCLR: vnode info",
				    cs->sc_cinfo[i].ci_vp);
#endif
			(void)vn_close(cs->sc_cinfo[i].ci_vp, FREAD|FWRITE,
			    p->p_ucred, p);
			free(cs->sc_cinfo[i].ci_path, M_DEVBUF);
		}

		/* Free component buffer freelist. */
		while ((cbp = cs->sc_freelist.lh_first) != NULL) {
			LIST_REMOVE(cbp, cb_list);
			FREE(cbp, M_DEVBUF);
		}

		/* Free interleave index. */
		for (i = 0; cs->sc_itable[i].ii_ndisk; ++i)
			free(cs->sc_itable[i].ii_index, M_DEVBUF);

		/* Free component info and interleave table. */
		free(cs->sc_cinfo, M_DEVBUF);
		free(cs->sc_itable, M_DEVBUF);
		cs->sc_flags &= ~CCDF_INITED;

		/*
		 * Free ccddevice information and clear entry.
		 */
		free(ccddevs[unit].ccd_cpp, M_DEVBUF);
		free(ccddevs[unit].ccd_vpp, M_DEVBUF);
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));

		/* Detatch the disk. */
		disk_detach(&cs->sc_dkdev);

		ccdunlock(cs);

		break;

	case DIOCGDINFO:
		*(struct disklabel *)data = *(cs->sc_dkdev.dk_label);
		break;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = cs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &cs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((error = ccdlock(cs)) != 0)
			return (error);

		cs->sc_flags |= CCDF_LABELLING;

		error = setdisklabel(cs->sc_dkdev.dk_label,
		    (struct disklabel *)data, 0, cs->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(CCDLABELDEV(dev),
				    ccdstrategy, cs->sc_dkdev.dk_label,
				    cs->sc_dkdev.dk_cpulabel);
		}

		cs->sc_flags &= ~CCDF_LABELLING;

		ccdunlock(cs);

		if (error)
			return (error);
		break;

	case DIOCWLABEL:
		if (*(int *)data != 0)
			cs->sc_flags |= CCDF_WLABEL;
		else
			cs->sc_flags &= ~CCDF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		ccdgetdefaultlabel(cs, (struct disklabel *)data);
		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

int
ccdsize(dev)
	dev_t dev;
{
	struct ccd_softc *cs;
	struct disklabel *lp;
	int part, unit, omask, size;

	unit = ccdunit(dev);
	if (unit >= numccd)
		return (-1);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (-1);

	part = DISKPART(dev);
	omask = cs->sc_dkdev.dk_openmask & (1 << part);
	lp = cs->sc_dkdev.dk_label;

	if (omask == 0 && ccdopen(dev, 0, S_IFBLK, curproc))
		return (-1);

	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (omask == 0 && ccdclose(dev, 0, S_IFBLK, curproc))
		return (-1);

	return (size);
}

int
ccddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 */
static int
ccdlookup(path, p, vpp)
	char *path;
	struct proc *p;
	struct vnode **vpp;	/* result */
{
	struct nameidata nd;
	struct vnode *vp;
	struct vattr va;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, path, p);
	if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
#ifdef DEBUG
		if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("ccdlookup: vn_open error = %d\n", error);
#endif
		return (error);
	}
	vp = nd.ni_vp;

	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}

	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
#ifdef DEBUG
		if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("ccdlookup: getattr error = %d\n", error);
#endif
		VOP_UNLOCK(vp);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (error);
	}

	/* XXX: eventually we should handle VREG, too. */
	if (va.va_type != VBLK) {
		VOP_UNLOCK(vp);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (ENOTBLK);
	}

#ifdef DEBUG
	if (ccddebug & CCDB_VNODE)
		vprint("ccdlookup: vnode info", vp);
#endif

	VOP_UNLOCK(vp);
	*vpp = vp;
	return (0);
}

static void
ccdgetdefaultlabel(cs, lp)
	struct ccd_softc *cs;
	struct disklabel *lp;
{
	struct ccdgeom *ccg = &cs->sc_geom;

	bzero(lp, sizeof(*lp));

	lp->d_secperunit = cs->sc_size;
	lp->d_secsize = ccg->ccg_secsize;
	lp->d_nsectors = ccg->ccg_nsectors;
	lp->d_ntracks = ccg->ccg_ntracks;
	lp->d_ncylinders = ccg->ccg_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "ccd", sizeof(lp->d_typename));
	lp->d_type = DTYPE_CCD;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = cs->sc_size;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(cs->sc_dkdev.dk_label);
}

/*
 * Read the disklabel from the ccd.  If one is not present, fake one
 * up.
 */
static void
ccdgetdisklabel(dev)
	dev_t dev;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs = &ccd_softc[unit];
	char *errstring;
	struct disklabel *lp = cs->sc_dkdev.dk_label;
	struct cpu_disklabel *clp = cs->sc_dkdev.dk_cpulabel;

	bzero(clp, sizeof(*clp));

	ccdgetdefaultlabel(cs, lp);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	errstring = readdisklabel(CCDLABELDEV(dev), ccdstrategy,
	    cs->sc_dkdev.dk_label, cs->sc_dkdev.dk_cpulabel);
	if (errstring)
		ccdmakedisklabel(cs);

#ifdef DEBUG
	/* It's actually extremely common to have unlabeled ccds. */
	if (ccddebug & CCDB_LABEL)
		if (errstring != NULL)
			printf("%s: %s\n", cs->sc_xname, errstring);
#endif
}

/*
 * Take care of things one might want to take care of in the event
 * that a disklabel isn't present.
 */
static void
ccdmakedisklabel(cs)
	struct ccd_softc *cs;
{
	struct disklabel *lp = cs->sc_dkdev.dk_label;

	/*
	 * For historical reasons, if there's no disklabel present
	 * the raw partition must be marked FS_BSDFFS.
	 */
	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;

	strncpy(lp->d_packname, "default label", sizeof(lp->d_packname));

	lp->d_checksum = dkcksum(lp);
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
static int
ccdlock(cs)
	struct ccd_softc *cs;
{
	int error;

	while ((cs->sc_flags & CCDF_LOCKED) != 0) {
		cs->sc_flags |= CCDF_WANTED;
		if ((error = tsleep(cs, PRIBIO | PCATCH, "ccdlck", 0)) != 0)
			return (error);
	}
	cs->sc_flags |= CCDF_LOCKED;
	return (0);
}

/*
 * Unlock and wake up any waiters.
 */
static void
ccdunlock(cs)
	struct ccd_softc *cs;
{

	cs->sc_flags &= ~CCDF_LOCKED;
	if ((cs->sc_flags & CCDF_WANTED) != 0) {
		cs->sc_flags &= ~CCDF_WANTED;
		wakeup(cs);
	}
}

#ifdef DEBUG
static void
printiinfo(ii)
	struct ccdiinfo *ii;
{
	register int ix, i;

	for (ix = 0; ii->ii_ndisk; ix++, ii++) {
		printf(" itab[%d]: #dk %d sblk %d soff %d",
		    ix, ii->ii_ndisk, ii->ii_startblk, ii->ii_startoff);
		for (i = 0; i < ii->ii_ndisk; i++)
			printf(" %d", ii->ii_index[i]);
		printf("\n");
	}
}
#endif
