/*	$NetBSD: disksubr.c,v 1.3.2.1 2001/10/10 11:56:29 fvdl Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.netbsd.org/ for
 *          information about NetBSD.
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

#include <sys/param.h> 
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/vnode.h>

#include <machine/disklabel.h>

/*
 * Attempt to read a disk label from a device using the indicated
 * stategy routine. The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines must be
 * filled in before calling us.
 *
 * Return buffer for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */

char *
readdisklabel(devvp, strat, lp, clp)
	struct vnode *devvp;
	void (*strat)(struct buf *);
	struct disklabel *lp; 
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	struct disklabel *dlp; 
	struct sgilabel *slp;
	char block[512];
	int error;
	int i;

	/* Minimal requirements for archetypal disk label. */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;

	/* Obtain buffer to probe drive with. */
	bp = geteblk((int)lp->d_secsize);

	/* Next, dig out the disk label. */
	bp->b_devvp = devvp;
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ | B_DKLABEL;
	(*strat)(bp);

	/* If successful, locate disk label within block and validate. */
	error = biowait(bp);
	if (error == 0) {
		/* Save the whole block in case it has info we need. */
		memcpy(block, bp->b_un.b_addr, sizeof(block));
	}
	bp->b_flags &= ~B_DKLABEL;
	brelse(bp);
	if (error != 0)
		return "error reading disklabel";

	/* Check for a NetBSD disk label. */
	dlp = (struct disklabel *) (block + LABELOFFSET);
	if (dlp->d_magic == DISKMAGIC) {
		if (dkcksum(dlp))
			return ("NetBSD disk label corrupted");
		*lp = *dlp;
		return NULL;
	}

	/* Check for a SGI label. */
	slp = (struct sgilabel *)block;
	if (be32toh(slp->magic) != SGILABEL_MAGIC)
		return "no disk label";
	/*
	 * XXX Calculate checksum.
	 */
	for (i = 0; i < MAXPARTITIONS; i++) {
	/* XXX be32toh */
		lp->d_partitions[i].p_offset = slp->partitions[i].first;
		lp->d_partitions[i].p_size = slp->partitions[i].blocks;
		lp->d_partitions[i].p_fstype = FS_BSDFFS;
		lp->d_partitions[i].p_fsize = 1024;
		lp->d_partitions[i].p_frag = 8;
		lp->d_partitions[i].p_cpg = 16;

		if (i == RAW_PART)
			lp->d_partitions[i].p_fstype = FS_OTHER;
	}

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_secsize = 512;
	lp->d_npartitions = 16;

	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

	return NULL;
}

int
setdisklabel(olp, nlp, openmask, clp)
	struct disklabel *olp;
	struct disklabel *nlp;
	unsigned long openmask;
	struct cpu_disklabel *clp;
{
	printf("SETDISKLABEL\n");

	return 0;
}

int     
writedisklabel(devvp, strat, lp, clp)
	struct vnode *devvp;
        void (*strat)(struct buf *);
        struct disklabel *lp;
        struct cpu_disklabel *clp;
{               
	printf("WRITEDISKLABEL\n");

	return ENODEV;
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
	struct partition *p;
	int part;
	int maxsz;
	int sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	if (bp->b_flags & B_DKLABEL)
		part = RAW_PART;
	else
		part = DISKPART(vdev_rdev(bp->b_devvp));
	p = lp->d_partitions + part;
	maxsz = p->p_size;

	/*
	 * Overwriting disk label?
	 * The label is always in sector LABELSECTOR.
	 */
	if (bp->b_blkno + p->p_offset <= LABELSECTOR &&
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

	/*
	 * Beyond partition?
	 */
	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		/* if exactly at end of disk, return an EOF */
		if (bp->b_blkno == maxsz) {
			bp->b_resid = bp->b_bcount;
			return(0);
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
	bp->b_resid = (bp->b_blkno + p->p_offset) / lp->d_secpercyl;
	return(1);
bad:
	bp->b_flags |= B_ERROR;
	return(-1);
}
