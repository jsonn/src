/*	$NetBSD: disksubr.c,v 1.9.14.1 1999/12/21 23:16:21 wrstuden Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

#define	b_cylin	b_resid

/* was this the boot device ? */
void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
}

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep, bshift)
	dev_t dev;
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int bshift;
{
	struct dos_partition *dp = osdep->dosparts;
	struct dkbad *bdp = &osdep->bad;
	struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;
	int dospartoff, cyl, i;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secsize == 0)
		lp->d_secsize = bsize;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_bshift = bshift;
	bp->b_bsize = blocksize(bp->b_bshift);

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (dp) {
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		(*strat)(bp);

		if (biowait(bp)) {
			msg = "dos boot record I/O error";
			goto done;
		}
		if (bcmp(bp->b_data, "X68SCSI1", 8) != 0) {
			msg = "no disk label";
			goto done;
		}

		/* read partition record */
		bp->b_blkno = DOSPARTOFF;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSPARTOFF / lp->d_secpercyl;
		(*strat)(bp);

		/* if successful, wander through dos partition table */
		if (biowait(bp)) {
			msg = "dos partition I/O error";
			goto done;
		}

		/* XXX how do we check veracity/bounds of this? */
		bcopy(bp->b_data + sizeof(*dp) /*DOSPARTOFF*/, dp,
		      NDOSPART * sizeof(*dp));

		lp->d_bbsize = 8192;
		lp->d_sbsize = 2048;
		for (i = 0; i < NDOSPART; i++, dp++)
			/* is this ours? */
			if (dp->dp_size) {
				u_char fstype;
				int part = i + (i < RAW_PART ? 0 : 1);
				int start = dp->dp_start * 2;
				int size = dp->dp_size * 2;

				/* need sector address for SCSI */
				dospartoff = start; /* XXX */

				/* update disklabel with details */
				lp->d_partitions[part].p_size = size;
				lp->d_partitions[part].p_offset =  start;
				/* get partition type */
#ifndef COMPAT_10
				if (dp->dp_flag == 1)
					fstype = FS_UNUSED;
				else
#endif
				if (!bcmp(dp->dp_typname, "Human68k", 8))
					fstype = FS_MSDOS;
				else if (part == 1)
					fstype = FS_SWAP;
				else
					fstype = FS_BSDFFS; /* XXX */
				lp->d_partitions[part].p_fstype = fstype; /* XXX */
				if (lp->d_npartitions <= part)
					lp->d_npartitions = part + 1;
			}
		goto done;
	}

	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylin = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_un.b_addr+DEV_BSIZE-sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			*lp = *dlp;
			msg = NULL;
			break;
		}
	}

	if (msg)
		goto done;

	/* obtain bad sector table if requested and present */
	if (bdp && (lp->d_flags & D_BADSECT)) {
		struct dkbad *db;

		i = 0;
		do {
			/* read a bad sector table */
			bp->b_flags = B_BUSY | B_READ;
			bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
			else
				bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
			bp->b_bcount = lp->d_secsize;
			bp->b_cylin = lp->d_ncylinders - 1;
			(*strat)(bp);

			/* if successful, validate, otherwise try another */
			if (biowait(bp)) {
				msg = "bad sector table I/O error";
			} else {
				db = (struct dkbad *)(bp->b_data);
#define DKBAD_MAGIC 0x4321
				if (db->bt_mbz == 0
					&& db->bt_flag == DKBAD_MAGIC) {
					msg = NULL;
					*bdp = *db;
					break;
				} else
					msg = "bad sector table corrupted";
			}
		} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
			i < lp->d_nsectors);
	}

done:
	bp->b_flags = B_INVAL;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	int i;
	struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
		|| (nlp->d_secsize % DEV_BSIZE) != 0)
			return(EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs(openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}
 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep, bshift)
	dev_t dev;
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int bshift;
{
	struct dos_partition *dp = osdep->dosparts;
	struct buf *bp;
	struct disklabel *dlp;
	int error, dospartoff, cyl, i;
	char *np;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_bshift = bshift;
	bp->b_bsize = blocksize(bp->b_bshift);

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (dp) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSBBSECTOR / lp->d_secpercyl;
		(*strat)(bp);
		if (biowait(bp))
			goto done;

#ifdef maybe
		if (bcmp(bp->b_data, "X68SCSI1", 8) != 0) {
			goto done;
		}
#endif

		/* read partition record */
		bp->b_blkno = DOSPARTOFF;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = DOSPARTOFF / lp->d_secpercyl;
		(*strat)(bp);

		if ((error = biowait(bp)) == 0) {
			/* XXX how do we check veracity/bounds of this? */
			dp = (struct dos_partition *)bp->b_data + 1;
			for (i = 0; i < NDOSPART; i++, dp++) {
				int part = i + (i < RAW_PART ? 0 : 1);
				int start, size;

				start = lp->d_partitions[part].p_offset >> 1;
				size = lp->d_partitions[part].p_size >> 1;

				switch (lp->d_partitions[part].p_fstype) {
				case FS_MSDOS:
					np = "Human68k";
					dp->dp_flag = 0; /* XXX $B<+F05/F0(B */
					break;

				case FS_SWAP:
					np = "BSD swap";
					dp->dp_flag = 2; /* $B;HMQ2DG=(B */
					break;

				case FS_BSDFFS:
					np = "BSD ffs ";
					if (part == 0)
						dp->dp_flag = 0; /* $B<+F05/F0(B */
					else
						dp->dp_flag = 2; /* $B;HMQ2DG=(B */
					break;

				case FS_UNUSED:
					np = "\0\0\0\0\0\0\0\0";
					start = size = 0;
					if (part < lp->d_npartitions) {
						dp->dp_flag = 1;
					} else {
						dp->dp_flag = 0;
					}
					break;

				default:
					/* XXX OS-9, MINIX etc. */
					continue;
				}
				bcopy(np, dp->dp_typname, 8);
				dp->dp_start = start;
				dp->dp_size = size;
			}
			bp->b_flags = B_BUSY | B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
		}
		goto done;
	}

#ifdef maybe
	/* disklabel in appropriate location? */
	if (lp->d_partitions[0].p_offset != 0
		&& lp->d_partitions[0].p_offset != dospartoff) {
		error = EXDEV;		
		goto done;
	}
#endif

	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylin = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	error = biowait(bp);
	if (error)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_BUSY | B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;
done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (error);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = lp->d_partitions[RAW_PART].p_offset + LABELSECTOR;
	int sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* Overwriting disk label? */
	if (bp->b_blkno + p->p_offset <= labelsector &&
#if LABELSECTOR != 0
	    bp->b_blkno + p->p_offset + sz > labelsector &&
#endif
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylin = (bp->b_blkno + p->p_offset) /
	    (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
done:
	return (0);
}
