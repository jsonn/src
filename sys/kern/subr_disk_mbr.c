/*	$NetBSD: subr_disk_mbr.c,v 1.4.12.1 2005/02/12 18:17:52 yamt Exp $	*/

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
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

/*
 * Code to find a NetBSD label on a disk that contains an i386 style MBR.
 * The first NetBSD label found in the 2nd sector of a NetBSD partition
 * is used.
 * If we don't find a label searching the MBR, we look at the start of the
 * disk, if that fails then a label is faked up from the MBR.
 *
 * If there isn't a disklabel or anything in the MBR then partition a
 * is set to cover the whole disk.
 * Useful for files that contain single filesystems (etc).
 *
 * This code will read host endian netbsd labels from little endian MBR.
 *
 * Based on the i386 disksubr.c
 *
 * Since the mbr only has 32bit fields for sector addresses, we do the same.
 *
 * XXX There are potential problems writing labels to disks where there
 * is only space for 8 netbsd partitions but this code has been compiled
 * with MAXPARTITIONS=16.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_disk_mbr.c,v 1.4.12.1 2005/02/12 18:17:52 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>

#include "opt_mbr.h"

#define MBR_LABELSECTOR	1

#define SCAN_CONTINUE	0
#define SCAN_FOUND	1
#define SCAN_ERROR	2

typedef struct mbr_args {
	struct disklabel *lp;
	void		(*strat)(struct buf *);
	struct buf	*bp;
	const char	*msg;
	int		error;
	int		written;	/* number of times we wrote label */
	uint		label_sector;	/* where we found the label */
} mbr_args_t;

#define READ_LABEL	1
#define UPDATE_LABEL	2
#define WRITE_LABEL	3
static int validate_label(mbr_args_t *, uint, int);
static int look_netbsd_part(mbr_args_t *, struct mbr_partition *, int, uint);
static int write_netbsd_label(mbr_args_t *, struct mbr_partition *, int, uint);

static int
read_sector(mbr_args_t *a, uint sector)
{
	struct buf *bp = a->bp;
	int error;

	bp->b_blkno = sector;
	bp->b_bcount = a->lp->d_secsize;
	bp->b_flags = (bp->b_flags & ~(B_WRITE | B_DONE)) | B_READ;
	bp->b_cylinder = sector / a->lp->d_secpercyl;
	(*a->strat)(bp);
	error = biowait(bp);
	if (error != 0)
		a->error = error;
	return error;
}

/* 
 * Scan MBR for partitions, call 'action' routine for each.
 */

static int
scan_mbr(mbr_args_t *a, int (*actn)(mbr_args_t *, struct mbr_partition *, int, uint))
{
	struct mbr_partition ptns[MBR_PART_COUNT];
	struct mbr_partition *dp;
	struct mbr_sector *mbr;
	uint ext_base, this_ext, next_ext;
	int rval;
	int i;
#ifdef COMPAT_386BSD_MBRPART
	int dp_386bsd = -1;
#endif

	ext_base = 0;
	this_ext = 0;
	for (;;) {
		if (read_sector(a, this_ext)) {
			a->msg = "dos partition I/O error";
			return SCAN_ERROR;
		}

		/* Note: Magic number is little-endian. */
		mbr = (void *)a->bp->b_data;
		if (mbr->mbr_magic != htole16(MBR_MAGIC))
			return SCAN_CONTINUE;

		/* Copy data out of buffer so action can use bp */
		memcpy(ptns, &mbr->mbr_parts, sizeof ptns);

		/* look for NetBSD partition */
		next_ext = 0;
		dp = ptns;
		for (i = 0; i < MBR_PART_COUNT; i++, dp++) {
			if (dp->mbrp_type == 0)
				continue;
			if (MBR_IS_EXTENDED(dp->mbrp_type)) {
				next_ext = le32toh(dp->mbrp_start);
				continue;
			}
#ifdef COMPAT_386BSD_MBRPART
			if (dp->mbrp_type == MBR_PTYPE_386BSD) {
				/*
				 * If more than one matches, take last,
				 * as NetBSD install tool does.
				 */
				if (this_ext == 0)
					dp_386bsd = i;
				continue;
			}
#endif
			rval = (*actn)(a, dp, i, this_ext);
			if (rval != SCAN_CONTINUE)
				return rval;
		}
		if (next_ext == 0)
			break;
		if (ext_base == 0) {
			ext_base = next_ext;
			next_ext = 0;
		}
		next_ext += ext_base;
		if (next_ext <= this_ext)
			break;
		this_ext = next_ext;
	}
#ifdef COMPAT_386BSD_MBRPART
	if (this_ext == 0 && dp_386bsd != -1)
		return (*actn)(a, &ptns[dp_386bsd], dp_386bsd, 0);
#endif
	return SCAN_CONTINUE;
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
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
const char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dkbad *bdp;
	int rval;
	int i;
	mbr_args_t a;

	memset(&a, 0, sizeof a);
	a.lp = lp;
	a.strat = strat;

	/* minimal requirements for architypal disk label */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[RAW_PART].p_size == 0)
		lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
	lp->d_partitions[RAW_PART].p_offset = 0;

	/*
	 * Set partition 'a' to be the whole disk.
	 * Cleared if we find an mbr or a netbsd label.
	 */
	lp->d_partitions[0].p_size = lp->d_partitions[RAW_PART].p_size;
	lp->d_partitions[0].p_fstype = FS_BSDFFS;

	/* get a buffer and initialize it */
	a.bp = geteblk((int)lp->d_secsize);
	a.bp->b_dev = dev;

	if (osdep)
		/*
		 * Scan mbr searching for netbsd partition and saving
		 * bios partition information to use if the netbsd one
		 * is absent.
		 */
		rval = scan_mbr(&a, look_netbsd_part);
	else
		rval = SCAN_CONTINUE;

	if (rval == SCAN_CONTINUE) {
		/* Look at start of disk */
		rval = validate_label(&a, LABELSECTOR, READ_LABEL);
		if (LABELSECTOR != 0 && rval == SCAN_CONTINUE)
			rval = validate_label(&a, 0, READ_LABEL);
	}

#if 0
	/*
	 * Save sector where we found the label for the 'don't overwrite
	 * the label' check in bounds_check_with_label.
	 */
	if (rval == SCAN_FOUND)
		xxx->label_sector = a.label_sector;
#endif

	/* Obtain bad sector table if requested and present */
	if (rval == SCAN_FOUND && osdep && (lp->d_flags & D_BADSECT)) {
		struct dkbad *db;
		int blkno;

		bdp = &osdep->bad;
		i = 0;
		rval = SCAN_ERROR;
		do {
			/* read a bad sector table */
			blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				blkno *= lp->d_secsize / DEV_BSIZE;
			else
				blkno /= DEV_BSIZE / lp->d_secsize;
			/* if successful, validate, otherwise try another */
			if (read_sector(&a, blkno)) {
				a.msg = "bad sector table I/O error";
				continue;
			}
			db = (struct dkbad *)(a.bp->b_data);
#define DKBAD_MAGIC 0x4321
			if (db->bt_mbz != 0 || db->bt_flag != DKBAD_MAGIC) {
				a.msg = "bad sector table corrupted";
				continue;
			}
			rval = SCAN_FOUND;
			*bdp = *db;
			break;
		} while ((a.bp->b_flags & B_ERROR) && (i += 2) < 10 &&
			i < lp->d_nsectors);
	}

	brelse(a.bp);
	if (rval != SCAN_ERROR)
		return NULL;
	return a.msg;
}

static int
look_netbsd_part(mbr_args_t *a, struct mbr_partition *dp, int slot, uint ext_base)
{
	struct partition *pp;
	int ptn_base = ext_base + le32toh(dp->mbrp_start);
	int rval;

	if (
#ifdef COMPAT_386BSD_MBRPART
	    dp->mbrp_type == MBR_PTYPE_386BSD ||
#endif
	    dp->mbrp_type == MBR_PTYPE_NETBSD) {
		rval = validate_label(a, ptn_base + MBR_LABELSECTOR, READ_LABEL);

		/* Put actual location where we found the label into ptn 2 */
		if (RAW_PART != 2 && (rval == SCAN_FOUND ||
					a->lp->d_partitions[2].p_size == 0)) {
			a->lp->d_partitions[2].p_size = le32toh(dp->mbrp_size);
			a->lp->d_partitions[2].p_offset = ptn_base;
		}

		/* If we got a netbsd label look no further */
		if (rval == SCAN_FOUND)
			return rval;
	}

	/* Install main partitions into e..h and extended into i+ */
	if (ext_base == 0)
		slot += 4;
	else {
		slot = 4 + MBR_PART_COUNT;
		pp = &a->lp->d_partitions[slot];
		for (; slot < MAXPARTITIONS; pp++, slot++) {
			/* This gets called twice - avoid duplicates */
			if (pp->p_offset == ptn_base &&
			    pp->p_size == le32toh(dp->mbrp_size))
				break;
			if (pp->p_size == 0)
				break;
		}
	}

	if (slot < MAXPARTITIONS) {
		/* Stop 'a' being the entire disk */
		a->lp->d_partitions[0].p_size = 0;
		a->lp->d_partitions[0].p_fstype = 0;

		/* save partition info */
		pp = &a->lp->d_partitions[slot];
		pp->p_offset = ptn_base;
		pp->p_size = le32toh(dp->mbrp_size);
		pp->p_fstype = xlat_mbr_fstype(dp->mbrp_type);

		if (slot >= a->lp->d_npartitions)
			a->lp->d_npartitions = slot + 1;
	}

	return SCAN_CONTINUE;
}


static int
validate_label(mbr_args_t *a, uint label_sector, int action)
{
	struct disklabel *dlp;
	char *dlp_lim;
	int error;

	/* Next, dig out disk label */
	if (read_sector(a, label_sector)) {
		a->msg = "disk label read failed";
		return SCAN_ERROR;
	}

	/* Locate disk label within block and validate */
	/*
	 * XXX (dsl) This search may be a waste of time, a lot of other i386
	 * code assumes the label is at offset LABELOFFSET (=0) in the sector.
	 *
	 * If we want to support disks from other netbsd ports, then the
	 * code should also allow for a shorter label nearer the end of
	 * the disk sector, and (IIRC) labels within 8k of the disk start.
	 */
	dlp = (void *)a->bp->b_data;
	if (action != WRITE_LABEL) {
		dlp_lim = a->bp->b_data + a->lp->d_secsize - sizeof(*dlp);
		for (;; dlp = (void *)((char *)dlp + sizeof(long))) {
			if ((char *)dlp > dlp_lim)
				return SCAN_CONTINUE;
			if (dlp->d_magic != DISKMAGIC
			    || dlp->d_magic2 != DISKMAGIC)
				continue;
			if (dlp->d_npartitions > MAXPARTITIONS
			    || dkcksum(dlp) != 0) {
				a->msg = "disk label corrupted";
				continue;
			}
			break;
		}
	}

	switch (action) {
	case READ_LABEL:
		*a->lp = *dlp;
		a->label_sector = label_sector;
		return SCAN_FOUND;
	case UPDATE_LABEL:
	case WRITE_LABEL:
		*dlp = *a->lp;
		a->bp->b_flags &= ~(B_READ|B_DONE);
		a->bp->b_flags |= B_WRITE;
		(*a->strat)(a->bp);
		error = biowait(a->bp);
		if (error != 0) {
			a->error = error;
			a->msg = "disk label write failed";
			return SCAN_ERROR;
		}
		a->written++;
		/* Write label to all mbr partitions */
		return SCAN_CONTINUE;
	default:
		return SCAN_ERROR;
	}
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
			return (EINVAL);

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
		if (i > nlp->d_npartitions)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			*npp = *opp;
			continue;
		}
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
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
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	mbr_args_t a;

	memset(&a, 0, sizeof a);
	a.lp = lp;
	a.strat = strat;

	/* get a buffer and initialize it */
	a.bp = geteblk((int)lp->d_secsize);
	a.bp->b_dev = dev;

	if (osdep)
		/* Write the label to every netbsd mbr partition */
		scan_mbr(&a, write_netbsd_label);

	/* and overwrite any label at the start of the volume */
	validate_label(&a, LABELSECTOR, UPDATE_LABEL);
	if (LABELSECTOR != 0)
		validate_label(&a, 0, UPDATE_LABEL);

	if (a.written == 0 && a.error == 0)
		a.error = ESRCH;

	brelse(a.bp);
	return a.error;
}

static int
write_netbsd_label(mbr_args_t *a, struct mbr_partition *dp, int slot, uint ext_base)
{
	int ptn_base = ext_base + le32toh(dp->mbrp_start);

	if (dp->mbrp_type != MBR_PTYPE_NETBSD)
		return SCAN_CONTINUE;

	return validate_label(a, ptn_base + MBR_LABELSECTOR, WRITE_LABEL);
}


/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(dk, bp, wlabel)
	struct disk *dk;
	struct buf *bp;
	int wlabel;
{
	struct disklabel *lp = dk->dk_label;
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = lp->d_partitions[2].p_offset + LABELSECTOR;
	int64_t sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			return (0);
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
	    bp->b_blkno + p->p_offset + sz > labelsector &&
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset) /
	    (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
	return (-1);
}
