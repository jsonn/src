/*	$NetBSD: disksubr.c,v 1.4.2.1 1999/10/19 16:49:10 he Exp $	*/

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
/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/disklabel_mbr.h>
#include <sys/syslog.h>

#include <machine/bswap.h>

#define	b_cylin	b_resid

#define NUM_PARTS 32

#define ROOT_PART 1
#define UFS_PART 2
#define SWAP_PART 3
#define HFS_PART 4
#define SCRATCH_PART 5

int fat_types[] = { MBR_PTYPE_FAT12, MBR_PTYPE_FAT16S,
		    MBR_PTYPE_FAT16B, MBR_PTYPE_FAT32,
		    MBR_PTYPE_FAT32L, MBR_PTYPE_FAT16L,
		    -1 };

static int getFreeLabelEntry __P((struct disklabel *));
static int whichType __P((struct part_map_entry *));
static void setpartition __P((struct part_map_entry *,
		struct partition *, int));
static int getNamedType __P((struct part_map_entry *, int,
		struct disklabel *, int, int, int *));
static char *read_mac_label __P((dev_t, void (*)(struct buf *),
		struct disklabel *, struct cpu_disklabel *));
static char *read_dos_label __P((dev_t, void (*)(struct buf *),
		struct disklabel *, struct cpu_disklabel *));
static int get_netbsd_label __P((dev_t dev, void (*strat)(struct buf *),
		struct disklabel *lp, daddr_t bno));

/*
 * Find an entry in the disk label that is unused and return it
 * or -1 if no entry
 */
static int
getFreeLabelEntry(lp)
	struct disklabel *lp;
{
	int i = 0;

	for (i = 0; i < MAXPARTITIONS; i++) {
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
	struct part_map_entry *part;
{
	struct blockzeroblock *bzb;
	char typestr[32], *s;
	int type;

	if (part->pmSig != PART_ENTRY_MAGIC || part->pmPartType[0] == '\0')
		return 0;

	strncpy(typestr, (char *)part->pmPartType, sizeof(typestr));
	typestr[sizeof(typestr) - 1] = '\0';
	for (s = typestr; *s; s++)
		if ((*s >= 'a') && (*s <= 'z'))
			*s = (*s - 'a' + 'A');

	if (strcmp(PART_TYPE_DRIVER, typestr) == 0 ||
	    strcmp(PART_TYPE_DRIVER43, typestr) == 0 ||
	    strcmp(PART_TYPE_DRIVERATA, typestr) == 0 ||
	    strcmp(PART_TYPE_FWB_COMPONENT, typestr) == 0 ||
	    strcmp(PART_TYPE_PARTMAP, typestr) == 0)
		type = 0;
	else if (strcmp(PART_TYPE_UNIX, typestr) == 0) {
		/* unix part, swap, root, usr */
		bzb = (struct blockzeroblock *)(&part->pmBootArgs);
		if (bzb->bzbMagic != BZB_MAGIC)
			type = 0;
		else if (bzb->bzbFlags & BZB_ROOTFS)
			type = ROOT_PART;
		else if (bzb->bzbFlags & BZB_USRFS)
			type = UFS_PART;
		else if (bzb->bzbType == BZB_TYPESWAP)
			type = SWAP_PART;
		else
			type = SCRATCH_PART;
	} else if (strcmp(PART_TYPE_MAC, typestr) == 0)
		type = HFS_PART;
	else
		type = SCRATCH_PART;	/* no known type */

	return type;
}

static void
setpartition(part, pp, fstype)
	struct part_map_entry *part;
	struct partition *pp;
{
	pp->p_size = part->pmPartBlkCnt;
	pp->p_offset = part->pmPyPartStart;
	pp->p_fstype = fstype;

	part->pmPartType[0] = '\0';
}

static int
getNamedType(part, num_parts, lp, type, alt, maxslot)
	struct part_map_entry *part;
	int num_parts;
	struct disklabel *lp;
	int type, alt;
	int *maxslot;
{
	struct blockzeroblock *bzb;
	int i = 0;

	for (i = 0; i < num_parts; i++) {
		if (whichType(part + i) != type)
			continue;

		if (type == ROOT_PART) {
			bzb = (struct blockzeroblock *)
			    (&(part + i)->pmBootArgs);
			if (alt >= 0 && alt != bzb->bzbCluster)
				continue;
			setpartition(part + i, &lp->d_partitions[0], FS_BSDFFS);
		} else if (type == UFS_PART) {
			bzb = (struct blockzeroblock *)
			    (&(part + i)->pmBootArgs);
			if (alt >= 0 && alt != bzb->bzbCluster)
				continue;
			setpartition(part + i, &lp->d_partitions[6], FS_BSDFFS);
			if (*maxslot < 6)
				*maxslot = 6;
		} else if (type == SWAP_PART) {
			setpartition(part + i, &lp->d_partitions[1], FS_SWAP);
			if (*maxslot < 1)
				*maxslot = 1;
		} else
			printf("disksubr.c: can't do type %d\n", type);

		return 0;
	}

	return -1;
}

/*
 * MF --
 * here's what i'm gonna do:
 * read in the entire diskpartition table, it may be bigger or smaller
 * than NUM_PARTS but read that many entries.  Each entry has a magic
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
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct part_map_entry *part;
	struct partition *pp;
	struct buf *bp;
	char *msg = NULL;
	int i, slot, maxslot = 0;

	/* get buffer and initialize it */
	bp = geteblk((int)lp->d_secsize * NUM_PARTS);
	bp->b_dev = dev;

	/* read partition map */
	bp->b_blkno = 1;	/* partition map starts at blk 1 */
	bp->b_bcount = lp->d_secsize * NUM_PARTS;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 1 / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp)) {
		msg = "Macintosh partition map I/O error";
		goto done;
	}

	part = (struct part_map_entry *)bp->b_data;

	/* Fill in standard partitions */
	lp->d_npartitions = RAW_PART + 1;
	if (getNamedType(part, NUM_PARTS, lp, ROOT_PART, 0, &maxslot))
		getNamedType(part, NUM_PARTS, lp, ROOT_PART, -1, &maxslot);
	if (getNamedType(part, NUM_PARTS, lp, UFS_PART, 0, &maxslot))
		getNamedType(part, NUM_PARTS, lp, UFS_PART, -1, &maxslot);
	getNamedType(part, NUM_PARTS, lp, SWAP_PART, -1, &maxslot);

	/* Now get as many of the rest of the partitions as we can */
	for (i = 0; i < NUM_PARTS; i++) {
		slot = getFreeLabelEntry(lp);
		if (slot < 0)
			break;

		pp = &lp->d_partitions[slot];

		switch (whichType(part + i)) {
		case ROOT_PART:
		/*
		 * another root part will turn into a plain old
		 * UFS_PART partition, live with it.
		 */
		case UFS_PART:
			setpartition(part + i, pp, FS_BSDFFS);
			break;
		case SWAP_PART:
			setpartition(part + i, pp, FS_SWAP);
			break;
		case HFS_PART:
			setpartition(part + i, pp, FS_HFS);
			break;
		case SCRATCH_PART:
			setpartition(part + i, pp, FS_OTHER);
			break;
		default:
			slot = 0;
			break;
		}
		if (slot > maxslot)
			maxslot = slot;
	}
	lp->d_npartitions = ((maxslot >= RAW_PART) ? maxslot : RAW_PART) + 1;

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);

	return msg;
}

/* Read MS-DOS partition table.
 *
 * XXX -
 * Since FFS is endian sensitive, we pay no effort in attempting to
 * dig up *BSD/i386 disk labels that may be present on the disk.
 * Hence anything but DOS partitions is treated as unknown FS type, but
 * this should suffice to mount_msdos Zip and other removable media.
 */
static char *
read_dos_label(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct mbr_partition *dp;
	struct partition *pp;
	struct buf *bp;
	char *msg = NULL;
	int i, *ip, slot, maxslot = 0;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* read master boot record */
	bp->b_blkno = MBR_BBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = MBR_BBSECTOR / lp->d_secpercyl;
	(*strat)(bp);

	/* if successful, wander through dos partition table */
	if (biowait(bp)) {
		msg = "dos partition I/O error";
		goto done;
	} else {
		/* XXX */
		dp = (struct mbr_partition *)(bp->b_data + MBR_PARTOFF);
		for (i = 0; i < NMBRPART; i++, dp++) {
			if (dp->mbrp_typ != 0) {
				slot = getFreeLabelEntry(lp);
				if (slot > maxslot)
					maxslot = slot;

				pp = &lp->d_partitions[slot];
				pp->p_fstype = FS_OTHER;
				pp->p_offset = bswap32(dp->mbrp_start);
				pp->p_size = bswap32(dp->mbrp_size);

				for (ip = fat_types; *ip != -1; ip++) {
					if (dp->mbrp_typ == *ip) {
						pp->p_fstype = FS_MSDOS;
						break;
					}
				}
			}
		}
	}
	lp->d_npartitions = ((maxslot >= RAW_PART) ? maxslot : RAW_PART) + 1;

 done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (msg);
}

/*
 * Get real NetBSD disk label
 */
static int
get_netbsd_label(dev, strat, lp, bno)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	daddr_t bno;
{
	struct buf *bp;
	struct disklabel *dlp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Now get the label block */
	bp->b_blkno = bno + LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp))
		goto done;

	for (dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
	     dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof (*dlp));
	     dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC
		    && dlp->d_magic2 == DISKMAGIC
		    && dlp->d_npartitions <= MAXPARTITIONS
		    && dkcksum(dlp) == 0) {
			*lp = *dlp;
			brelse(bp);
			return 1;
		}
	}
done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return 0;
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
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	char *msg = NULL;
	struct disklabel *dlp;

	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;

	if (lp->d_secpercyl == 0) {
		return msg = "Zero secpercyl";
	}
	bp = geteblk((int)lp->d_secsize);

	bp->b_dev = dev;
	bp->b_blkno = 0;
	bp->b_resid = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 1 / lp->d_secpercyl;
	(*strat)(bp);

	osdep->cd_start = -1;

	if (biowait(bp)) {
		msg = "I/O error reading block zero";
	} if (get_netbsd_label(dev, strat, lp, 0)) {
		osdep->cd_start = 0;
		msg = "NetBSD disklabel";
	} else {
		u_int16_t *sbSigp;

		sbSigp = (u_int16_t *)bp->b_un.b_addr;
		if (*sbSigp == 0x4552) {
			msg = read_mac_label(dev, strat, lp, osdep);
		} else if (bswap16(*(u_int16_t *)(bp->b_data + MBR_MAGICOFF))
			   == MBR_MAGIC) {
			msg = read_dos_label(dev, strat, lp, osdep);
		} else {
			msg = "no disk label -- NetBSD or Macintosh";
			osdep->cd_start = 0;	/* XXX for now */
		}
	}

	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
	    || (nlp->d_secsize % DEV_BSIZE) != 0)
		return EINVAL;

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return 0;
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC
	    || dkcksum(nlp) != 0)
		return EINVAL;

	/* openmask parameter ignored */

	*olp = *nlp;
	return 0;
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	int error;
	struct disklabel label;

	/*
	 * Try to re-read a disklabel, in case he changed the MBR.
	 */
	label = *lp;
	readdisklabel(dev, strat, &label, osdep);
	if (osdep->cd_start < 0)
		return EINVAL;

	/* get a buffer and initialize it */
	bp = geteblk(lp->d_secsize);
	bp->b_dev = dev;

	bp->b_blkno = osdep->cd_start + LABELSECTOR;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;

	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	error = biowait(bp);
	if (error != 0)
		goto done;

	bp->b_flags = B_BUSY | B_WRITE;

	bcopy((caddr_t)lp, (caddr_t)bp->b_data + LABELOFFSET, sizeof *lp);

	(*strat)(bp);
	error = biowait(bp);

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);

	return error;
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaris of the partition.  Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If axactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise truncate request. */
		bp->b_bcount = sz * lp->d_secsize;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset)
			 / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;

	return 1;

bad:
	bp->b_flags |= B_ERROR;
done:
	return 0;
}

/*
 * This is called by main to set dumplo and dumpsize.
 */
void
cpu_dumpconf()
{
	int nblks;		/* size of dump device */
	int skip;
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = physmem;

	/* Skip enough blocks at start of disk to preserve an eventual disklabel. */
	skip = LABELSECTOR + 1;
	skip += ctod(1) - 1;
	skip = ctod(dtoc(skip));
	if (dumplo < skip)
		dumplo = skip;

	/* Put dump at end of partition */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}
