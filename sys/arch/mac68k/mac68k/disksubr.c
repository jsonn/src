/*	$NetBSD: disksubr.c,v 1.19.2.1 1997/10/27 19:45:55 mellon Exp $	*/

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
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* rewritten, 2-5-93 MLF */
/* its alot cleaner now, and adding support for new partition types
 * isn't a bitch anymore
 * known bugs:
 * 1) when only an HFS_PART part exists on a drive it gets assigned to "B"
 * this is because of line 623 of sd.c, I think this line should go.
 * 2) /sbin/disklabel expects the whole disk to be in "D", we put it in
 * "C" (I think) and we don't set that position in the disklabel structure
 * as used.  Again, not my fault.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>

#include <mac68k/mac68k/dpme.h>	/* MF the structure of a mac partition entry */

#define	b_cylin	b_resid

#define NUM_PARTS_PROBED 32

#define ROOT_PART 1
#define UFS_PART 2
#define SWAP_PART 3
#define HFS_PART 4
#define SCRATCH_PART 5

static int getFreeLabelEntry __P((struct disklabel *));
static int whichType __P((struct partmapentry *));
static void fixPartTable __P((struct partmapentry *, long, char *, int *));
static void setRoot __P((struct partmapentry *, struct disklabel *, int));
static void setSwap __P((struct partmapentry *, struct disklabel *, int));
static void setUfs __P((struct partmapentry *, struct disklabel *, int));
static void setHfs __P((struct partmapentry *, struct disklabel *, int));
static void setScratch __P((struct partmapentry *, struct disklabel *, int));
static int getNamedType
__P((struct partmapentry *, int, struct disklabel *, int, int, int *));
static char *read_mac_label __P((dev_t, void (*)(struct buf *),
		register struct disklabel *, struct cpu_disklabel *));

/*
 * Find an entry in the disk label that is unused and return it
 * or -1 if no entry
 */
static int 
getFreeLabelEntry(lp)
	struct disklabel *lp;
{
	int i = 0;

	for (i = 0; i < NUM_PARTS_PROBED; i++) {
		if ((i != RAW_PART)
		    && (lp->d_partitions[i].p_fstype == FS_UNUSED))
			return i;
	}

	return -1;
}

/*
 * figure out what the type of the given part is and return it
 */
static int 
whichType(part)
	struct partmapentry *part;
{
	struct blockzeroblock *bzb;

	if (part->pmPartType[0] == '\0')
		return 0;

	if (strcmp(PART_DRIVER_TYPE, (char *)part->pmPartType) == 0)
		return 0;
	if (strcmp(PART_DRIVER43_TYPE, (char *)part->pmPartType) == 0)
		return 0;
	if (strcmp(PART_PARTMAP_TYPE, (char *)part->pmPartType) == 0)
		return 0;
	if (strcmp(PART_UNIX_TYPE, (char *)part->pmPartType) == 0) {
		/* unix part, swap, root, usr */
		bzb = (struct blockzeroblock *)(&part->pmBootArgs);
		if (bzb->bzbMagic != BZB_MAGIC)
			return 0;

		if (bzb->bzbFlags & BZB_ROOTFS)
			return ROOT_PART;

		if (bzb->bzbFlags & BZB_USRFS)
			return UFS_PART;

		if (bzb->bzbType == BZB_TYPESWAP)
			return SWAP_PART;

		return SCRATCH_PART;
	}
	if (strcmp(PART_MAC_TYPE, (char *)part->pmPartType) == 0)
		return HFS_PART;
/*
	if (strcmp(PART_SCRATCH, (char *)part->pmPartType) == 0)
		return SCRATCH_PART;
*/
	return SCRATCH_PART;	/* no known type, but label it, anyway */
}

/*
 * Take part table in crappy form, place it in a structure we can depend
 * upon.  Make sure names are NUL terminated.  Capitalize the names
 * of part types.
 */
static void
fixPartTable(partTable, size, base, num)
	struct partmapentry *partTable;
	long size;
	char *base;
	int *num;
{
	int i = 0;
	struct partmapentry *pmap;
	char *s;

	for (i = 0; i < NUM_PARTS_PROBED; i++) {
		pmap = (struct partmapentry *)((i * size) + base);

		if (pmap->pmSig != DPME_MAGIC) { /* this is not valid */
			pmap->pmPartType[0] = '\0';
			break;
		}

		(*num)++;

		pmap->pmPartName[31] = '\0';
		pmap->pmPartType[31] = '\0';

		for (s = pmap->pmPartType; *s; s++)
			if ((*s >= 'a') && (*s <= 'z'))
				*s = (*s - 'a' + 'A');

		partTable[i] = *pmap;
	}
}

static void 
setRoot(part, lp, slot)
	struct partmapentry *part;
	struct disklabel *lp;
	int slot;
{
	lp->d_partitions[slot].p_size = part->pmPartBlkCnt;
	lp->d_partitions[slot].p_offset = part->pmPyPartStart;
	lp->d_partitions[slot].p_fstype = FS_BSDFFS;

#if PRINT_DISKLABELS
	printf("%c: Root '%s' at %d size %d\n", slot + 'a',
	    part->pmPartName,
	    part->pmPyPartStart,
	    part->pmPartBlkCnt);
#endif

	part->pmPartType[0] = '\0';
}

static void 
setSwap(part, lp, slot)
	struct partmapentry *part;
	struct disklabel *lp;
	int slot;
{
	lp->d_partitions[slot].p_size = part->pmPartBlkCnt;
	lp->d_partitions[slot].p_offset = part->pmPyPartStart;
	lp->d_partitions[slot].p_fstype = FS_SWAP;

#if PRINT_DISKLABELS
	printf("%c: Swap '%s' at %d size %d\n", slot + 'a',
	    part->pmPartName,
	    part->pmPyPartStart,
	    part->pmPartBlkCnt);
#endif

	part->pmPartType[0] = '\0';
}

static void 
setUfs(part, lp, slot)
	struct partmapentry *part;
	struct disklabel *lp;
	int slot;
{
	lp->d_partitions[slot].p_size = part->pmPartBlkCnt;
	lp->d_partitions[slot].p_offset = part->pmPyPartStart;
	lp->d_partitions[slot].p_fstype = FS_BSDFFS;

#if PRINT_DISKLABELS
	printf("%c: Usr '%s' at %d size %d\n", slot + 'a',
	    part->pmPartName,
	    part->pmPyPartStart,
	    part->pmPartBlkCnt);
#endif

	part->pmPartType[0] = '\0';
}

static void 
setHfs(part, lp, slot)
	struct partmapentry *part;
	struct disklabel *lp;
	int slot;
{
	lp->d_partitions[slot].p_size = part->pmPartBlkCnt;
	lp->d_partitions[slot].p_offset = part->pmPyPartStart;
	lp->d_partitions[slot].p_fstype = FS_HFS;

#if PRINT_DISKLABELS
	printf("%c: HFS_PART '%s' at %d size %d\n", slot + 'a',
	    part->pmPartName,
	    part->pmPyPartStart,
	    part->pmPartBlkCnt);
#endif

	part->pmPartType[0] = '\0';
}

static void 
setScratch(part, lp, slot)
	struct partmapentry *part;
	struct disklabel *lp;
	int slot;
{
	lp->d_partitions[slot].p_size = part->pmPartBlkCnt;
	lp->d_partitions[slot].p_offset = part->pmPyPartStart;
	lp->d_partitions[slot].p_fstype = FS_OTHER;

#if PRINT_DISKLABELS
	printf("%c: Other (%s) '%s' at %d size %d\n", slot + 'a',
	    part->pmPartType,
	    part->pmPartName,
	    part->pmPyPartStart,
	    part->pmPartBlkCnt);
#endif

	part->pmPartType[0] = '\0';
}

static int 
getNamedType(part, num_parts, lp, type, alt, maxslot)
	struct partmapentry *part;
	int num_parts;
	struct disklabel *lp;
	int type, alt;
	int *maxslot;
{
	struct blockzeroblock *bzb;
	int i = 0;

	for (i = 0; i < num_parts; i++) {
		if (whichType(&(part[i])) == type) {
			switch (type) {
			case ROOT_PART:
				bzb = (struct blockzeroblock *)
				    (&part[i].pmBootArgs);
				if (alt >= 0 && alt != bzb->bzbCluster)
					goto skip;
				setRoot(&(part[i]), lp, 0);
				break;
			case UFS_PART:
				bzb = (struct blockzeroblock *)
				    (&part[i].pmBootArgs);
				if (alt >= 0 && alt != bzb->bzbCluster)
					goto skip;
				setUfs(&(part[i]), lp, 6);
				if (*maxslot < 6) *maxslot = 6;
				break;
			case SWAP_PART:
				setSwap(&(part[i]), lp, 1);
				if (*maxslot < 1) *maxslot = 1;
				break;
			default:
				printf("disksubr.c: can't do type %d\n", type);
				break;
			}

			return 0;
		}
skip:
	}

	return -1;
}

/*
 * MF --
 * here's what i'm gonna do:
 * read in the entire diskpartition table, it may be bigger or smaller
 * than NUM_PARTS_PROBED but read that many entries.  Each entry has a magic
 * number so we'll know if an entry is crap.
 * next fill in the disklabel with info like this
 * next fill in the root, usr, and swap parts.
 * then look for anything else and fit it in.
 *	A: root
 *	B: Swap
 *	C: Whole disk
 *	G: Usr
 * 
 * 
 * I'm not entirely sure what netbsd386 wants in c & d
 * 386bsd wants other stuff, so i'll leave them alone
 * 
 * AKB -- I added to Mike's original algorithm by searching for a bzbCluster
 *	of zero for root, first.  This allows A/UX to live on cluster 1 and
 *	NetBSD to live on cluster 0--regardless of the actual order on the
 *	disk.  This whole algorithm should probably be changed in the future.
 */
static char *
read_mac_label(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	char *msg = NULL;
	int i = 0, num_parts = 0, maxslot = 0;
	struct partmapentry pmap[NUM_PARTS_PROBED];

	bp = geteblk((int) lp->d_secsize * NUM_PARTS_PROBED);
	bp->b_dev = dev;
	bp->b_blkno = 1;	/* partition map starts at blk 1 */
	bp->b_bcount = lp->d_secsize * NUM_PARTS_PROBED;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 1 / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "I/O error reading partition map.";
		goto done;
	}

	lp->d_npartitions = 1;	/* one for 'c' */
	fixPartTable(pmap, lp->d_secsize, bp->b_un.b_addr, &num_parts);
	if (getNamedType(pmap, num_parts, lp, ROOT_PART, 0, &maxslot))
		getNamedType(pmap, num_parts, lp, ROOT_PART, -1, &maxslot);
	if (getNamedType(pmap, num_parts, lp, UFS_PART, 0, &maxslot))
		getNamedType(pmap, num_parts, lp, UFS_PART, -1, &maxslot);
	getNamedType(pmap, num_parts, lp, SWAP_PART, -1, &maxslot);
	for (i = 0; i < num_parts; i++) {
		int partType;
		int slot;

		slot = getFreeLabelEntry(lp);
		if (slot < 0)
			break;

		partType = whichType(&(pmap[i]));

		switch (partType) {

		case ROOT_PART:
		/*
		 * another root part will turn into a plain old
		 * UFS_PART partition, live with it.
		 */
		case UFS_PART:
			setUfs(&(pmap[i]), lp, slot);
			if (slot > maxslot) maxslot = slot;
			break;
		case SWAP_PART:
			setSwap(&(pmap[i]), lp, slot);
			if (slot > maxslot) maxslot = slot;
			break;
		case HFS_PART:
			setHfs(&(pmap[i]), lp, slot);
			if (slot > maxslot) maxslot = slot;
			break;
		case SCRATCH_PART:
			setScratch(&(pmap[i]), lp, slot);
			if (slot > maxslot) maxslot = slot;
			break;
		default:
			break;
		}
	}
	lp->d_npartitions = maxslot+1;

done:
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return msg;
}

/*
 * Attempt to read a disk label from a device using the indicated stategy
 * routine.  The label must be partly set up before this: secpercyl and
 * anything required in the strategy routine (e.g., sector size) must be
 * filled in before calling us.  Returns null on success and an error
 * string on failure. 
 *
 * This will read sector zero.  If this contains what looks like a valid
 * Macintosh boot sector, we attempt to fill in the disklabel structure.
 * If the first longword of the disk is a NetBSD disk label magic number,
 * then we assume that it's a real disklabel and return it.
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	register struct buf *bp;
	char *msg = NULL;
	struct disklabel *dlp;

	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;

	if (lp->d_secpercyl == 0) {
		return msg = "Zero secpercyl";
	}
	bp = geteblk((int)lp->d_secsize * MAXPARTITIONS);

	bp->b_dev = dev;
	bp->b_blkno = 0;
	bp->b_resid = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 1 / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "I/O error reading block zero";
	} else {
		u_int16_t	*sbSigp;

		sbSigp = (u_int16_t *) bp->b_un.b_addr;
		if (*sbSigp == 0x4552) {
			msg = read_mac_label(dev, strat, lp, osdep);
		} else {
			dlp = (struct disklabel *)(bp->b_un.b_addr + 0);
			if (dlp->d_magic == DISKMAGIC) {
				*lp = *dlp;
			} else {
				msg = "no disk label--NetBSD or Macintosh";
			}
		}
	}

	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
#if 0
	register i;
	register struct partition *opp, *npp;

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);
	while ((i = ffs((long)openmask)) != 0) {
		i--;
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
#endif
	return (0);
}

/*
 * Write disk label back to device after modification.
 *
 *  MF - 8-14-93 This function is never called.  It is here just in case
 *  we want to write dos disklabels some day. Really!
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
#if 0
	struct buf *bp;
	struct disklabel *dlp;
	int labelpart;
	int error = 0;

	labelpart = DISKPART(dev);
	if (lp->d_partitions[labelpart].p_offset != 0) {
		if (lp->d_partitions[0].p_offset != 0)
			return (EXDEV);	/* not quite right */
		labelpart = 0;
	}
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), labelpart);
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;
	(*strat)(bp);
	if (error = biowait(bp))
		goto done;
	for (dlp = (struct disklabel *)bp->b_un.b_addr;
	    dlp <= (struct disklabel *)
	    (bp->b_un.b_addr + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;
done:
	brelse(bp);
	return (error);
#else
	return 0;
#endif
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
#if 0
	int labelsect = lp->d_partitions[0].p_offset;
#endif
	int maxsz = p->p_size;
	int sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
#if 0				/* MF this is crap, especially on swap !! */
	if (bp->b_blkno + p->p_offset <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
	    bp->b_blkno + p->p_offset + sz > LABELSECTOR + labelsect &&
#endif
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
#endif

#if defined(DOSBBSECTOR) && defined(notyet)
	/* overwriting master boot record? */
	if (bp->b_blkno + p->p_offset <= DOSBBSECTOR &&
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
#endif

	/* beyond partition? */
	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		/* if exactly at end of disk, return an EOF */
		if (bp->b_blkno == maxsz) {
			bp->b_resid = bp->b_bcount;
			return (0);
		}
		/* or truncate if part of it fits */
		sz = maxsz - bp->b_blkno;
		if (sz <= 0) {
			bp->b_error = EINVAL;
			goto bad;
		}
		bp->b_bcount = sz << DEV_BSHIFT;
	}
	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylin = (bp->b_blkno + p->p_offset) / lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
	return (-1);
}

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	/* Empty for now. -- XXX */
}
