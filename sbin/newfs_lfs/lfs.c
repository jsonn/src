/*	$NetBSD: lfs.c,v 1.12.4.4 2001/02/03 21:47:12 he Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)lfs.c	8.5 (Berkeley) 5/24/95";
#else
__RCSID("$NetBSD: lfs.c,v 1.12.4.4 2001/02/03 21:47:12 he Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#define FSTYPENAMES
#include <sys/disklabel.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "extern.h"

extern int Nflag; /* Don't write anything */
daddr_t *ifib = NULL; /* Ifile single indirect block (lbn -NDADDR) */

/*
 * This table is indexed by the log base 2 of the block size.
 * It returns the maximum file size allowed in a file system
 * with the specified block size.  For block sizes smaller than
 * 8K, the size is limited by tha maximum number of blocks that
 * can be reached by triply indirect blocks:
 *	NDADDR + INOPB(bsize) + INOPB(bsize)^2 + INOPB(bsize)^3
 * For block size of 8K or larger, the file size is limited by the
 * number of blocks that can be represented in the file system.  Since
 * we use negative block numbers to represent indirect blocks, we can
 * have a maximum of 2^31 blocks.
 */

u_quad_t maxtable[] = {
	/*    1 */ -1,
	/*    2 */ -1,
	/*    4 */ -1,
	/*    8 */ -1,
	/*   16 */ -1,
	/*   32 */ -1,
	/*   64 */ -1,
	/*  128 */ -1,
	/*  256 */ -1,
	/*  512 */ NDADDR + 128 + 128 * 128 + 128 * 128 * 128,
	/* 1024 */ NDADDR + 256 + 256 * 256 + 256 * 256 * 256,
	/* 2048 */ NDADDR + 512 + 512 * 512 + 512 * 512 * 512,
	/* 4096 */ NDADDR + 1024 + 1024 * 1024 + 1024 * 1024 * 1024,
	/* 8192 */ 1 << 31,
	/* 16 K */ 1 << 31,
	/* 32 K */ 1 << 31,
};

static struct lfs lfs_default =  {
	{ /* lfs_dlfs */
		/* dlfs_magic */	LFS_MAGIC,
		/* dlfs_version */	LFS_VERSION,
		/* dlfs_size */		0,
		/* dlfs_ssize */	DFL_LFSSEG/DFL_LFSBLOCK,
		/* dlfs_dsize */	0,
		/* dlfs_bsize */	DFL_LFSBLOCK,
		/* dlfs_fsize */	DFL_LFSFRAG,
		/* dlfs_frag */		DFL_LFSBLOCK/DFL_LFSFRAG,
		/* dlfs_free */		LFS_FIRST_INUM,
		/* dlfs_bfree */	0,
		/* dlfs_nfiles */	0,
		/* dlfs_avail */	0,
		/* dlfs_uinodes */	0,
		/* dlfs_idaddr */	0,
		/* dlfs_ifile */	LFS_IFILE_INUM,
		/* dlfs_lastseg */	0,
		/* dlfs_nextseg */	0,
		/* dlfs_curseg */	0,
		/* dlfs_offset */	0,
		/* dlfs_lastpseg */	0,
		/* dlfs_tstamp */	0,
		/* dlfs_minfree */	MINFREE,
		/* dlfs_maxfilesize */	0,
		/* dlfs_dbpseg */	DFL_LFSSEG/DEV_BSIZE,
		/* dlfs_inopb */	DFL_LFSBLOCK/sizeof(struct dinode),
		/* dlfs_ifpb */		DFL_LFSBLOCK/sizeof(IFILE),
		/* dlfs_sepb */		DFL_LFSBLOCK/sizeof(SEGUSE),
		/* dlfs_nindir */	DFL_LFSBLOCK/sizeof(daddr_t),
		/* dlfs_nseg */		0,
		/* dlfs_nspf */		0,
		/* dlfs_cleansz */	0,
		/* dlfs_segtabsz */	0,
		/* dlfs_segmask */	DFL_LFSSEG_MASK,
		/* dlfs_segshift */	DFL_LFSSEG_SHIFT,
		/* dlfs_bshift */	DFL_LFSBLOCK_SHIFT,
		/* dlfs_ffshift */	DFL_LFS_FFSHIFT,
		/* dlfs_fbshift */	DFL_LFS_FBSHIFT,
		/* dlfs_bmask */	DFL_LFSBLOCK_MASK,
		/* dlfs_ffmask */	DFL_LFS_FFMASK,
		/* dlfs_fbmask */	DFL_LFS_FBMASK,
		/* dlfs_fsbtodb */	0,
		/* dlfs_sushift */	0,
		/* dlfs_maxsymlinklen */	MAXSYMLINKLEN,
		/* dlfs_sboffs */	{ 0 },
		/* dlfs_nclean */       0,
		/* dlfs_fsmnt */        { 0 },
		/* dlfs_clean */        0,
		/* dlfs_dmeta */	0,
		/* dlfs_minfreeseg */   0,

		/* dlfs_pad */ 		{ 0 },
		/* dlfs_cksum */	0
	},
	/* lfs_sp */		NULL,
	/* lfs_ivnode */	NULL,
	/* lfs_seglock */	0,
	/* lfs_lockpid */	0,
	/* lfs_iocount */	0,
	/* lfs_writer */	0,
	/* lfs_dirops */	0,
	/* lfs_doifile */	0,
	/* lfs_nactive */	0,
	/* lfs_fmod */		0,
	/* lfs_ronly */		0,
	/* lfs_flags */		0
};

#define	UMASK	0755

struct direct lfs_root_dir[] = {
	{ ROOTINO, sizeof(struct direct), DT_DIR, 1, "."},
	{ ROOTINO, sizeof(struct direct), DT_DIR, 2, ".."},
	{ LFS_IFILE_INUM, sizeof(struct direct), DT_REG, 5, "ifile"},
	{ LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 10, "lost+found"},
};

struct direct lfs_lf_dir[] = {
        { LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 1, "." },
        { ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
};

static daddr_t make_dinode 
	__P((ino_t, struct dinode *, int, daddr_t, struct lfs *));
static void make_dir __P(( void *, struct direct *, int));
static void put __P((int, off_t, void *, size_t));

int
make_lfs(fd, lp, partp, minfree, block_size, frag_size, seg_size, minfreeseg)
	int fd;
	struct disklabel *lp;
	struct partition *partp;
	int minfree;
	int block_size;
	int frag_size;
	int seg_size;
	int minfreeseg;
{
	struct dinode *dip;	/* Pointer to a disk inode */
	struct dinode *dpagep;	/* Pointer to page of disk inodes */
	CLEANERINFO *cleaninfo;	/* Segment cleaner information table */
	FINFO file_info;	/* File info structure in summary blocks */
	IFILE *ifile;		/* Pointer to array of ifile structures */
	IFILE *ip;		/* Pointer to array of ifile structures */
	struct lfs *lfsp;	/* Superblock */
	SEGUSE *segp;		/* Segment usage table */
	SEGUSE *segtable;	/* Segment usage table */
	SEGSUM summary;		/* Segment summary structure */
	SEGSUM *sp;		/* Segment summary pointer */
	daddr_t	last_sb_addr;	/* Address of superblocks */
	daddr_t	sb_addr;	/* Address of superblocks */
	daddr_t	seg_addr;	/* Address of current segment */
	char *ipagep;		/* Pointer to the page we use to write stuff */
	char *sump;		/* Used to copy stuff into segment buffer */
	ufs_daddr_t *block_array; /* Array of logical block nos to put in sum */
	u_long blocks_used;	/* Number of blocks in first segment */
	u_long *dp;		/* Used to computed checksum on data */
	u_long *datasump;	/* Used to computed checksum on data */
	int block_array_size;	/* How many entries in block array */
	int bsize;		/* Block size */
	int fsize;		/* Fragment size */
	int db_per_fb;		/* Disk blocks per file block */
	int i, j;
	off_t off;		/* Offset at which to write */
	int sb_interval;	/* number of segs between super blocks */
	off_t seg_seek;		/* Seek offset for a segment */
	int ssize;		/* Segment size */
	int sum_size;		/* Size of the summary block */
	int warned_segtoobig=0;

	lfsp = &lfs_default;

	/* If partition is not an LFS partition, warn that that is the case */
	if(partp->p_fstype != FS_BSDLFS) {
		fatal("partition label indicated fs type \"%s\", expected \"%s\"",
		      fstypenames[partp->p_fstype], fstypenames[FS_BSDLFS]);
	}

	if (!(bsize = block_size))
		if (!(bsize = partp->p_fsize * partp->p_frag))
			bsize = DFL_LFSBLOCK;
	if (!(fsize = frag_size))
		if (!(fsize = partp->p_fsize))
			fsize = DFL_LFSFRAG;
	if (!(ssize = seg_size))
		if (partp->p_sgs == 0 ||
		    !(ssize = (partp->p_fsize * partp->p_frag) << partp->p_sgs))
		{
			ssize = DFL_LFSSEG;
		}

	/* Sanity check: fsize<=bsize<ssize */
	if (fsize > bsize) {
		/* Only complain if fsize was explicitly set */
		if(frag_size)
			fatal("fragment size must be <= block size %d", bsize);
		fsize = bsize;
	}
	if (bsize >= ssize) {
		/* Only fatal if ssize was explicitly set */
		if(seg_size)
			fatal("block size must be < segment size");
		warnx("%s: disklabel segment size (%d) too small, using default (%d)",
		      progname, ssize, DFL_LFSSEG);
		ssize = DFL_LFSSEG;
	}

    tryagain:
	/* Modify parts of superblock overridden by command line arguments */
	if (bsize != DFL_LFSBLOCK || fsize != DFL_LFSFRAG) {
		lfsp->lfs_bshift = log2(bsize);
		if (1 << lfsp->lfs_bshift != bsize)
			fatal("%d: block size not a power of 2", bsize);
		lfsp->lfs_bsize = bsize;
		lfsp->lfs_fsize = fsize;
		lfsp->lfs_bmask = bsize - 1;
		lfsp->lfs_inopb = bsize / sizeof(struct dinode);
		lfsp->lfs_ffmask = fsize - 1;
		lfsp->lfs_ffshift = log2(fsize);
		if (1 << lfsp->lfs_ffshift != fsize)
			fatal("%d: frag size not a power of 2", fsize);
		lfsp->lfs_frag = numfrags(lfsp, bsize);
		lfsp->lfs_fbmask = lfsp->lfs_frag - 1;
		lfsp->lfs_fbshift = log2(lfsp->lfs_frag);
/* MIS -- should I round to power of 2 */
		lfsp->lfs_ifpb = bsize / sizeof(IFILE);
		lfsp->lfs_sepb = bsize / sizeof(SEGUSE);
		lfsp->lfs_nindir = bsize / sizeof(daddr_t);
	}

	if (ssize != DFL_LFSSEG) {
		lfsp->lfs_segshift = log2(ssize);
		if (1 << lfsp->lfs_segshift != ssize)
			fatal("%d: segment size not power of 2", ssize);
		lfsp->lfs_ssize = ssize;
		lfsp->lfs_segmask = ssize - 1;
		lfsp->lfs_dbpseg = ssize / DEV_BSIZE;
	}
	lfsp->lfs_ssize = ssize >> lfsp->lfs_bshift;
	lfsp->lfs_minfree = minfree;

	/*
	 * Fill in parts of superblock that can be computed from file system
	 * size, disk geometry and current time.
	 */
	db_per_fb = bsize/lp->d_secsize;
	lfsp->lfs_fsbtodb = log2(db_per_fb);
	lfsp->lfs_sushift = log2(lfsp->lfs_sepb);
	lfsp->lfs_size = partp->p_size >> lfsp->lfs_fsbtodb;
	lfsp->lfs_dsize = lfsp->lfs_size - (LFS_LABELPAD >> lfsp->lfs_bshift);
	lfsp->lfs_nseg = lfsp->lfs_dsize / lfsp->lfs_ssize;

	lfsp->lfs_nclean = lfsp->lfs_nseg - 1;
	lfsp->lfs_maxfilesize = maxtable[lfsp->lfs_bshift] << lfsp->lfs_bshift;
	if (minfreeseg == 0)
		lfsp->lfs_minfreeseg = lfsp->lfs_nseg / DFL_MIN_FREE_SEGS;
	else
		lfsp->lfs_minfreeseg = minfreeseg;
	if (lfsp->lfs_minfreeseg < MIN_FREE_SEGS)
		lfsp->lfs_minfreeseg = MIN_FREE_SEGS;

	if(lfsp->lfs_nseg < lfsp->lfs_minfreeseg + 1
	   || lfsp->lfs_nseg < LFS_MIN_SBINTERVAL + 1)
	{
		if(seg_size == 0 && ssize > (bsize<<1)) {
			if(!warned_segtoobig) {
				fprintf(stderr,"Segment size %d is too large; trying smaller sizes.\n", ssize);
				if (ssize == (bsize << 16)) {
					fprintf(stderr, "(Did you perhaps accidentally leave \"16\" in the disklabel's sgs field?)\n");
				}
			}
			++warned_segtoobig;
			ssize >>= 1;
			goto tryagain;
		}
		fatal("Could not allocate enough segments with segment size %d and block size %d;\nplease decrease the segment size.\n",
			ssize, lfsp->lfs_bsize);
	}

	printf("%.1fMB in %d segments of size %d\n", 
	       (lfsp->lfs_nseg * (double)ssize) / 1048576.0,
	       lfsp->lfs_nseg, ssize);

	/* 
	 * The number of free blocks is set from the number of segments
	 * times the segment size - lfs_minfreesegs (that we never write
	 * because we need to make sure the cleaner can run).  Then
	 * we'll subtract off the room for the superblocks ifile entries
	 * and segment usage table, and half a block per segment that can't
	 * be written due to fragmentation.
	 */
	lfsp->lfs_dsize = (lfsp->lfs_nseg - lfsp->lfs_minfreeseg) *
		fsbtodb(lfsp, lfsp->lfs_ssize);

	lfsp->lfs_bfree = lfsp->lfs_dsize;
	lfsp->lfs_bfree -= fsbtodb(lfsp, lfsp->lfs_nseg / 2);
	lfsp->lfs_segtabsz = SEGTABSIZE_SU(lfsp);
	lfsp->lfs_cleansz = CLEANSIZE_SU(lfsp);
	if ((lfsp->lfs_tstamp = time(NULL)) == -1)
		fatal("time: %s", strerror(errno));
	if ((sb_interval = lfsp->lfs_nseg / LFS_MAXNUMSB) < LFS_MIN_SBINTERVAL)
		sb_interval = LFS_MIN_SBINTERVAL;

	/* To start, one inode block and one segsum are dirty metadata */
	lfsp->lfs_dmeta = 1 + fsbtodb(lfsp, 1);
	/*
	 * Now, lay out the file system.  We need to figure out where
	 * the superblocks go, initialize the checkpoint information
	 * for the first two superblocks, initialize the segment usage
	 * information, put the segusage information in the ifile, create
	 * the first block of IFILE structures, and link all the IFILE
	 * structures into a free list.
	 */

	/* Figure out where the superblocks are going to live */
	lfsp->lfs_sboffs[0] = btodb(LFS_LABELPAD);
	lfsp->lfs_dsize -= btodb(LFS_SBPAD);
	for (i = 1; i < LFS_MAXNUMSB; i++) {
		sb_addr = ((i * sb_interval) << 
		    (lfsp->lfs_segshift - lfsp->lfs_bshift + lfsp->lfs_fsbtodb))
		    + lfsp->lfs_sboffs[0];
		if (sb_addr > partp->p_size)
			break;
		lfsp->lfs_sboffs[i] = sb_addr;
		lfsp->lfs_dsize -= btodb(LFS_SBPAD);
	}

	/* We need >= 2 superblocks */
	if(lfsp->lfs_sboffs[1] == 0x0) {
		fatal("Could not assign a disk adress for the second superblock.\nPlease decrease the segment size.\n");
	}

	last_sb_addr = lfsp->lfs_sboffs[i - 1];
	lfsp->lfs_lastseg = lfsp->lfs_sboffs[0];
	lfsp->lfs_nextseg = 
	    lfsp->lfs_sboffs[1] ? lfsp->lfs_sboffs[1] : lfsp->lfs_sboffs[0];
	lfsp->lfs_curseg = lfsp->lfs_lastseg;

	/*
	 * Initialize the segment usage table.  The first segment will
	 * contain the superblock, the cleanerinfo (cleansz), the segusage 
	 * table (segtabsz), 1 block's worth of IFILE entries, the root 
	 * directory, the lost+found directory and one block's worth of 
	 * inodes (containing the ifile, root, and l+f inodes).
	 */
	if (!(cleaninfo = malloc(lfsp->lfs_cleansz << lfsp->lfs_bshift)))
		fatal("%s", strerror(errno));
	cleaninfo->clean = lfsp->lfs_nseg - 1;
	cleaninfo->dirty = 1;

	if (!(segtable = malloc(lfsp->lfs_segtabsz << lfsp->lfs_bshift)))
		fatal("%s", strerror(errno));
	segp = segtable;
	blocks_used = lfsp->lfs_segtabsz + lfsp->lfs_cleansz + 4;
	segp->su_nbytes = ((lfsp->lfs_segtabsz + lfsp->lfs_cleansz + 1) <<
			   lfsp->lfs_bshift) +
		2 * roundup(DIRBLKSIZ,lfsp->lfs_fsize) +
		3 * DINODE_SIZE;
	segp->su_lastmod = lfsp->lfs_tstamp;
	segp->su_nsums = 1;	/* 1 summary blocks */
	segp->su_ninos = 1;	/* 1 inode block */
	segp->su_flags = SEGUSE_SUPERBLOCK | SEGUSE_DIRTY;

	lfsp->lfs_bfree -= btodb(LFS_SUMMARY_SIZE);
	lfsp->lfs_bfree -=
	     fsbtodb(lfsp, lfsp->lfs_cleansz + lfsp->lfs_segtabsz + 4);

	/* 
	 * Now figure out the address of the ifile inode. The inode block
	 * appears immediately after the segment summary.
	 */
	lfsp->lfs_idaddr = (LFS_LABELPAD + LFS_SBPAD + LFS_SUMMARY_SIZE) /
	    lp->d_secsize;

	j = 1;
	for (segp = segtable + 1, i = 1; i < lfsp->lfs_nseg; i++, segp++) {
		if ((i % sb_interval) == 0 && j < LFS_MAXNUMSB) {
			segp->su_flags = SEGUSE_SUPERBLOCK;
			lfsp->lfs_bfree -= (LFS_SBPAD / lp->d_secsize);
			++j;
		} else
			segp->su_flags = 0;
		segp->su_lastmod = 0;
		segp->su_nbytes = 0;
		segp->su_ninos = 0;
		segp->su_nsums = 0;
	}

	/* 
	 * Initialize dynamic accounting.
	 */
	lfsp->lfs_uinodes = 0;

	/*
	 * Ready to start writing segments.  The first segment is different
	 * because it contains the segment usage table and the ifile inode
	 * as well as a superblock.  For the rest of the segments, set the 
	 * time stamp to be 0 so that the first segment is the most recent.
	 * For each segment that is supposed to contain a copy of the super
	 * block, initialize its first few blocks and its segment summary 
	 * to indicate this.
	 */
	lfsp->lfs_nfiles = LFS_FIRST_INUM - 1;

	/* Now create a block of disk inodes */
	if (!(dpagep = malloc(lfsp->lfs_bsize)))
		fatal("%s", strerror(errno));
	dip = (struct dinode *)dpagep;
	memset(dip, 0, lfsp->lfs_bsize);

	/* Create a block of IFILE structures. */
	if (!(ipagep = (char *)malloc(lfsp->lfs_bsize)))
		fatal("%s", strerror(errno));
	ifile = (IFILE *)ipagep;

	/* 
	 * Initialize IFILE.  It is the next block following the
	 * block of inodes (whose address has been calculated in
	 * lfsp->lfs_idaddr;
	 */
	sb_addr = lfsp->lfs_idaddr + lfsp->lfs_bsize / lp->d_secsize;
	sb_addr = make_dinode(LFS_IFILE_INUM, dip, 
		(lfsp->lfs_cleansz + lfsp->lfs_segtabsz+1)<<lfsp->lfs_fbshift,
		sb_addr, lfsp);
	dip->di_mode = IFREG|IREAD|IWRITE;
	dip->di_flags = SF_IMMUTABLE; /* XXX KS */
	ip = &ifile[LFS_IFILE_INUM];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Initialize the ROOT Directory */
	sb_addr = make_dinode(ROOTINO, ++dip, howmany(DIRBLKSIZ,lfsp->lfs_fsize), sb_addr, lfsp);
	dip->di_mode = IFDIR | UMASK;
	dip->di_size = DIRBLKSIZ;
	dip->di_blocks = howmany(roundup(DIRBLKSIZ,lfsp->lfs_fsize),DEV_BSIZE);
	dip->di_nlink = 3;
	ip = &ifile[ROOTINO];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Initialize the lost+found Directory */
	sb_addr = make_dinode(LOSTFOUNDINO, ++dip, howmany(DIRBLKSIZ,lfsp->lfs_fsize), sb_addr, lfsp);
	dip->di_mode = IFDIR | UMASK;
	dip->di_size = DIRBLKSIZ;
	dip->di_blocks = howmany(roundup(DIRBLKSIZ,lfsp->lfs_fsize),DEV_BSIZE);
	dip->di_nlink = 2;
	ip = &ifile[LOSTFOUNDINO];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Make all the other dinodes invalid */
	for (i = INOPB(lfsp)-3, dip++; i; i--, dip++)
		dip->di_inumber = LFS_UNUSED_INUM;
	

	/* Link remaining IFILE entries in free list */
	for (ip = &ifile[LFS_FIRST_INUM], i = LFS_FIRST_INUM; 
	    i < lfsp->lfs_ifpb; ++ip) {
		ip->if_version = 1;
		ip->if_daddr = LFS_UNUSED_DADDR;
		ip->if_nextfree = ++i;
	}
	ifile[lfsp->lfs_ifpb - 1].if_nextfree = LFS_UNUSED_INUM;

	/* Now, write the segment */

	/* Adjust blocks_used to take indirect block into account */
	if(ifib) {
		++blocks_used;
		segtable[0].su_nbytes += lfsp->lfs_bsize;
	}

	/* Compute a checksum across all the data you're writing */
	dp = datasump = malloc (blocks_used * sizeof(u_long));
	*dp++ = ((u_long *)dpagep)[0];		/* inode block */
	for (i = 0; i < lfsp->lfs_cleansz; i++)
		*dp++ = ((u_long *)cleaninfo)[(i << lfsp->lfs_bshift) / 
		    sizeof(u_long)];		/* Cleaner info */
	for (i = 0; i < lfsp->lfs_segtabsz; i++)
		*dp++ = ((u_long *)segtable)[(i << lfsp->lfs_bshift) / 
		    sizeof(u_long)];		/* Segusage table */
	*dp++ = ((u_long *)ifile)[0];		/* Ifile */

	/* Still need the root and l+f bytes; get them later */

	/* Write out the inode block */
	off = LFS_LABELPAD + LFS_SBPAD + LFS_SUMMARY_SIZE;
	put(fd, off, dpagep, lfsp->lfs_bsize);
	off += lfsp->lfs_bsize;

	/* Write out the ifile */

	put(fd, off, cleaninfo, lfsp->lfs_cleansz << lfsp->lfs_bshift);
	off += (lfsp->lfs_cleansz << lfsp->lfs_bshift);
	(void)free(cleaninfo);

	put(fd, off, segtable, lfsp->lfs_segtabsz << lfsp->lfs_bshift);
	off += (lfsp->lfs_segtabsz << lfsp->lfs_bshift);
	(void)free(segtable);

	put(fd, off, ifile, lfsp->lfs_bsize);
	off += lfsp->lfs_bsize;

	/* XXX KS - write the single indirect block */
	if(ifib) {
		*dp++ = ((u_long *)ifib)[0];
		put(fd, off, ifib, lfsp->lfs_bsize);
		off += lfsp->lfs_bsize;
	}

	/*
	 * use ipagep for space for writing out other stuff.  It used to 
	 * contain the ifile, but we're done with it.
	 */

	/* Write out the root and lost and found directories */
	memset(ipagep, 0, lfsp->lfs_bsize);
	make_dir(ipagep, lfs_root_dir, 
	    sizeof(lfs_root_dir) / sizeof(struct direct));
	*dp++ = ((u_long *)ipagep)[0];
	dip = ((struct dinode *)dpagep) + 1;
	put(fd, off, ipagep, dblksize(lfsp,dip,0));
	off += dblksize(lfsp,dip,0);

	memset(ipagep, 0, lfsp->lfs_bsize);
	make_dir(ipagep, lfs_lf_dir, 
		sizeof(lfs_lf_dir) / sizeof(struct direct));
	*dp++ = ((u_long *)ipagep)[0];
	dip = ((struct dinode *)dpagep) + 2;
	put(fd, off, ipagep, dblksize(lfsp,dip,0));
	off += dblksize(lfsp,dip,0);

	/* Write Superblock */
	lfsp->lfs_offset = off / lp->d_secsize;
	lfsp->lfs_avail = lfsp->lfs_dsize -
		(fsbtodb(lfsp, lfsp->lfs_ssize) - btodb(LFS_SBPAD) -
		 (sntoda(lfsp, 1) - lfsp->lfs_offset));

	lfsp->lfs_bfree = lfsp->lfs_avail; /* XXX */
	/* Slop for an imperfect cleaner */
	lfsp->lfs_avail += (lfsp->lfs_minfreeseg / 2) *
		fsbtodb(lfsp, lfsp->lfs_ssize);

	lfsp->lfs_cksum = lfs_sb_cksum(&(lfsp->lfs_dlfs));

	put(fd, (off_t)LFS_LABELPAD, &(lfsp->lfs_dlfs), sizeof(struct dlfs));

	/* 
	 * Finally, calculate all the fields for the summary structure
	 * and write it.
	 */

	memset(&summary,0,sizeof(summary));
	summary.ss_next = lfsp->lfs_nextseg;
	summary.ss_create = lfsp->lfs_tstamp;
	summary.ss_nfinfo = 3;
	summary.ss_ninos = 3;
	summary.ss_magic = SS_MAGIC;
	summary.ss_datasum = cksum(datasump, sizeof(u_long) * blocks_used);

	/*
	 * Make sure that we don't overflow a summary block. We have to
	 * record: FINFO structures for ifile, root, and l+f.  The number
	 * of blocks recorded for the ifile is determined by the size of
	 * the cleaner info and the segments usage table.  There is room
	 * for one block included in sizeof(FINFO) so we don't need to add
	 * any extra space for the ROOT and L+F, and one block of the ifile
	 * is already counted.  Finally, we leave room for 1 inode block
	 * address.
	 */
	sum_size = 3*sizeof(FINFO) + sizeof(SEGSUM) + 2*sizeof(daddr_t) +
	    (lfsp->lfs_cleansz + lfsp->lfs_segtabsz) * sizeof(ufs_daddr_t);
#define	SUMERR \
"Multiple summary blocks in segment 1 not yet implemented\nsummary is %d bytes."
	if (sum_size > LFS_SUMMARY_SIZE)
		fatal(SUMERR, sum_size);

	block_array_size = lfsp->lfs_cleansz + lfsp->lfs_segtabsz + 1;
	if (block_array_size > NDADDR)
		block_array_size++;

	if (!(block_array = malloc(block_array_size *sizeof(int))))
		fatal("%s: %s", special, strerror(errno));

	/* fill in the array */
	for (i = 0; i < block_array_size; i++)
		block_array[i] = i;
	if (block_array_size > NDADDR)
		block_array[block_array_size-1] = -NDADDR;

	/* copy into segment */
	sump = ipagep;
	memcpy(sump, &summary, sizeof(SEGSUM));
	sump += sizeof(SEGSUM);

	/* Now, add the ifile */
	file_info.fi_nblocks = block_array_size;
	file_info.fi_version = 1;
	file_info.fi_lastlength = lfsp->lfs_bsize;
	file_info.fi_ino = LFS_IFILE_INUM;

	memmove(sump, &file_info, sizeof(FINFO) - sizeof(ufs_daddr_t));
	sump += sizeof(FINFO) - sizeof(ufs_daddr_t);
	memmove(sump, block_array, sizeof(ufs_daddr_t) * file_info.fi_nblocks);
	sump += sizeof(ufs_daddr_t) * file_info.fi_nblocks;

	/* Now, add the root directory */
	dip = ((struct dinode *)dpagep) + 1;
	file_info.fi_nblocks = 1;
	file_info.fi_version = 1;
	file_info.fi_ino = ROOTINO;
	file_info.fi_blocks[0] = 0;
	file_info.fi_lastlength = dblksize(lfsp, dip, 0);
	memmove(sump, &file_info, sizeof(FINFO));
	sump += sizeof(FINFO);

	/* Now, add the lost and found */
	dip = ((struct dinode *)dpagep) + 2;
	file_info.fi_ino = LOSTFOUNDINO;
	file_info.fi_lastlength = dblksize(lfsp, dip, 0);
	memmove(sump, &file_info, sizeof(FINFO));

	((daddr_t *)ipagep)[LFS_SUMMARY_SIZE / sizeof(daddr_t) - 1] = 
	    lfsp->lfs_idaddr;
	((SEGSUM *)ipagep)->ss_sumsum = cksum(ipagep+sizeof(summary.ss_sumsum), 
	    LFS_SUMMARY_SIZE - sizeof(summary.ss_sumsum));
	put(fd, (off_t)LFS_LABELPAD + LFS_SBPAD, ipagep, LFS_SUMMARY_SIZE);

	sp = (SEGSUM *)ipagep;
	sp->ss_create = 0;
	sp->ss_nfinfo = 0;
	sp->ss_ninos = 0;
	sp->ss_datasum = 0;
	sp->ss_magic = SS_MAGIC;

	free(dpagep);

	/* Now write the summary block for the next partial so it's invalid */
	sp->ss_sumsum =
	    cksum(&sp->ss_datasum, LFS_SUMMARY_SIZE - sizeof(sp->ss_sumsum));
	put(fd, off, sp, LFS_SUMMARY_SIZE);

	/* Now, write the rest of the superblocks */
	lfsp->lfs_cksum = lfs_sb_cksum(&(lfsp->lfs_dlfs));
	printf("super-block backups (for fsck -b #) at:\n");
	for (i = 0; i < LFS_MAXNUMSB; i++) {
		seg_addr = lfsp->lfs_sboffs[i];

		printf("%d%s ", seg_addr, (i == LFS_MAXNUMSB - 1 ? "" : ","));

		/* Leave the time stamp on the alt sb, zero the rest */
		if(i == 2) {
			lfsp->lfs_tstamp = 0;
			lfsp->lfs_cksum = lfs_sb_cksum(&(lfsp->lfs_dlfs));
		}
		seg_seek = (off_t)seg_addr * lp->d_secsize;
		put(fd, seg_seek, &(lfsp->lfs_dlfs), sizeof(struct dlfs));
	}
	printf("\n");
	free(ipagep);
	close(fd);
	return (0);
}

static void
put(fd, off, p, len)
	int fd;
	off_t off;
	void *p;
	size_t len;
{
	int wbytes;

	if (Nflag)
		return;

	if (lseek(fd, off, SEEK_SET) < 0)
		fatal("%s: seek: %s", special, strerror(errno));
	if ((wbytes = write(fd, p, len)) < 0)
		fatal("%s: write: %s", special, strerror(errno));
	if (wbytes != len)
		fatal("%s: short write (%d, not %ld)", 
		      special, wbytes, (u_long) len);
}

/*
 * Create the root directory for this file system and the lost+found
 * directory.
 */

static daddr_t
make_dinode(ino, dip, nfrags, saddr, lfsp)
	ino_t ino;				/* inode we're creating */
	struct dinode *dip;			/* disk inode */
	int nfrags;				/* number of frags in file */
	daddr_t saddr;				/* starting block address */
	struct lfs *lfsp;			/* superblock */
{
	int db_per_fb, i;
	int nblocks;

	nblocks = howmany(nfrags, lfsp->lfs_frag);
	if(nblocks >= NDADDR)
		nfrags = roundup(nfrags, lfsp->lfs_frag);

	dip->di_nlink = 1;
	dip->di_blocks = fragstodb(lfsp, nfrags);

	dip->di_size = (nfrags << lfsp->lfs_ffshift);
	dip->di_atime = dip->di_mtime = dip->di_ctime = lfsp->lfs_tstamp;
	dip->di_atimensec = dip->di_mtimensec = dip->di_ctimensec = 0;
	dip->di_inumber = ino;
	dip->di_gen = 1;

	db_per_fb = 1 << lfsp->lfs_fsbtodb;
#if 0
	if (NDADDR < nblocks)
		fatal("File ino=%d requires more than the number of direct blocks; please increase block or segment size.",ino);
#else
	if (NDADDR+NINDIR(lfsp) < nblocks) {
		fatal("File ino=%d requires more blocks than can be accommodated with a single indirect block; please increase segment or block size.",ino);
	} else if (NDADDR < nblocks) {
		fprintf(stderr,"Using %d single indirect block(s) for inode %d\n",
		     (nblocks-NDADDR)/NINDIR(lfsp) + 1, ino);
		dip->di_blocks += db_per_fb;
	}
#endif
	/* Assign the block addresses for the ifile */
	for (i = 0; i < MIN(nblocks,NDADDR); i++, saddr += db_per_fb) {
		dip->di_db[i] = saddr;
	}
	if (nfrags & lfsp->lfs_fbmask) {
		/* Last block is a fragment */
		saddr -= db_per_fb - fragstodb(lfsp, nfrags & lfsp->lfs_fbmask);
	}
	/* XXX We are only called with nblocks > 1 for Ifile */
	if(ino == LFS_IFILE_INUM && nblocks > NDADDR) {
		ifib = (daddr_t *)malloc(lfsp->lfs_bsize);
		memset(ifib,0,lfsp->lfs_bsize);
		for (i = 0; i < nblocks-NDADDR; i++, saddr += db_per_fb)
			ifib[i] = saddr;
		dip->di_ib[0] = saddr;
		saddr += db_per_fb;
	}

	return (saddr);
}


/*
 * Construct a set of directory entries in "bufp".  We assume that all the
 * entries in protodir fir in the first DIRBLKSIZ.  
 */
static void
make_dir(bufp, protodir, entries)
	void *bufp;
	struct direct *protodir;
	int entries;
{
	char *cp;
	int i, spcleft;

	spcleft = DIRBLKSIZ;
	for (cp = bufp, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(NEWDIRFMT, &protodir[i], 0);
		memmove(cp, &protodir[i], protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		if ((spcleft -= protodir[i].d_reclen) < 0)
			fatal("%s: %s", special, "directory too big");
	}
	protodir[i].d_reclen = spcleft;
	memmove(cp, &protodir[i], DIRSIZ(NEWDIRFMT, &protodir[i], 0));
}
