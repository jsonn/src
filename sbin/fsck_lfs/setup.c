/* $NetBSD: setup.c,v 1.19.2.1 2005/05/07 11:21:29 tron Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1980, 1986, 1993
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
 */

/* #define DKTYPENAMES */
#define FSTYPENAMES
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/file.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#define vnode uvnode
#include <ufs/lfs/lfs.h>
#undef vnode

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bufcache.h"
#include "vnode.h"
#include "lfs.h"

#include "fsck.h"
#include "extern.h"
#include "fsutil.h"

static struct disklabel *getdisklabel(const char *, int);
static uint64_t calcmaxfilesize(int);

ufs_daddr_t *din_table;
SEGUSE *seg_table;

#ifdef DKTYPENAMES
int useless(void);

int
useless(void)
{
	char **foo = (char **) dktypenames;
	char **bar = (char **) fscknames;

	return foo - bar;
}
#endif

/*
 * calculate the maximum file size allowed with the specified block shift.
 */
static uint64_t
calcmaxfilesize(int bshift)
{
	uint64_t nptr; /* number of block pointers per block */
	uint64_t maxblock;

	nptr = (1 << bshift) / sizeof(uint32_t);
	maxblock = NDADDR + nptr + nptr * nptr + nptr * nptr * nptr;

	return maxblock << bshift;
}

void
reset_maxino(ino_t len)
{
	din_table = (ufs_daddr_t *) realloc(din_table, len * sizeof(*din_table));
	statemap = realloc(statemap, len * sizeof(char));
	typemap = realloc(typemap, len * sizeof(char));
	lncntp = (int16_t *) realloc(lncntp, len * sizeof(int16_t));

	if (din_table == NULL || statemap == NULL || typemap == NULL ||
	    lncntp == NULL) {
		errexit("expanding tables failed: out of memory");
	}

	memset(din_table + maxino, 0, (len - maxino) * sizeof(*din_table));
	memset(statemap + maxino, 0, (len - maxino) * sizeof(char));
	memset(typemap + maxino, 0, (len - maxino) * sizeof(char));
	memset(lncntp + maxino, 0, (len - maxino) * sizeof(int16_t));

	return;
}
 
int
setup(const char *dev)
{
	long bmapsize;
	struct disklabel *lp;
	struct stat statb;
	int doskipclean;
	u_int64_t maxfilesize;
	int open_flags;
	struct uvnode *ivp;
	struct ubuf *bp;
	int i;
	SEGUSE *sup;

	havesb = 0;
	doskipclean = skipclean;
	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (!S_ISCHR(statb.st_mode)) {
		pfatal("%s is not a character device", dev);
		if (reply("CONTINUE") == 0)
			return (0);
	}
	if (nflag)
		open_flags = O_RDONLY;
	else
		open_flags = O_RDWR;

	if ((fsreadfd = open(dev, open_flags)) < 0) {
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if (nflag) {
		if (preen)
			pfatal("NO WRITE ACCESS");
		printf("** %s (NO WRITE)\n", dev);
		quiet = 0;
	} else if (!preen && !quiet)
		printf("** %s\n", dev);

	fsmodified = 0;
	lfdir = 0;

	bufinit(0); /* XXX we could make a better guess */
	fs = lfs_init(fsreadfd, bflag, idaddr, 0, debug);
	if (fs == NULL) {
		if (preen)
			printf("%s: ", cdevname());
		pfatal("BAD SUPER BLOCK\n");
	}
	if ((lp = getdisklabel((char *) NULL, fsreadfd)) != NULL)
		dev_bsize = secsize = lp->d_secsize;
	else
		dev_bsize = secsize = DEV_BSIZE;

        /* Resize buffer cache now that we have a superblock to guess from. */ 
        bufrehash((fs->lfs_segtabsz + maxino / fs->lfs_ifpb) << 4);

	if (fs->lfs_pflags & LFS_PF_CLEAN) {
		if (doskipclean) {
			if (!quiet)
				pwarn("%sile system is clean; not checking\n",
				      preen ? "f" : "** F");
			return (-1);
		}
		if (!preen)
			pwarn("** File system is already clean\n");
	}

	if (debug) {
		printf("idaddr    = 0x%lx\n", idaddr ? (unsigned long)idaddr :
			(unsigned long)fs->lfs_idaddr);
		printf("dev_bsize = %lu\n", dev_bsize);
		printf("lfs_bsize = %lu\n", (unsigned long) fs->lfs_bsize);
		printf("lfs_fsize = %lu\n", (unsigned long) fs->lfs_fsize);
		printf("lfs_frag  = %lu\n", (unsigned long) fs->lfs_frag);
		printf("lfs_inopb = %lu\n", (unsigned long) fs->lfs_inopb);
	}
	if (fs->lfs_version == 1)
		maxfsblock = fs->lfs_size * (fs->lfs_bsize / dev_bsize);
	else
		maxfsblock = fs->lfs_size;
	maxfilesize = calcmaxfilesize(fs->lfs_bshift);
	if ((fs->lfs_minfree < 0 || fs->lfs_minfree > 99)) {
		pfatal("IMPOSSIBLE MINFREE=%d IN SUPERBLOCK",
		    fs->lfs_minfree);
		if (reply("SET TO DEFAULT") == 1) {
			fs->lfs_minfree = 10;
			sbdirty();
		}
	}
	if (fs->lfs_bmask != fs->lfs_bsize - 1) {
		pwarn("INCORRECT BMASK=%x IN SUPERBLOCK (should be %x)",
		    (unsigned int) fs->lfs_bmask,
		    (unsigned int) fs->lfs_bsize - 1);
		fs->lfs_bmask = fs->lfs_bsize - 1;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("FIX") == 1) {
			sbdirty();
		}
	}
	if (fs->lfs_ffmask != fs->lfs_fsize - 1) {
		pwarn("INCORRECT FFMASK=%" PRId64 " IN SUPERBLOCK",
		    fs->lfs_ffmask);
		fs->lfs_ffmask = fs->lfs_fsize - 1;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("FIX") == 1) {
			sbdirty();
		}
	}
	if (fs->lfs_fbmask != (1 << fs->lfs_fbshift) - 1) {
		pwarn("INCORRECT FFMASK=%" PRId64 " IN SUPERBLOCK",
		    fs->lfs_ffmask);
		fs->lfs_fbmask = (1 << fs->lfs_fbshift) - 1;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("FIX") == 1) {
			sbdirty();
		}
	}
	if (fs->lfs_maxfilesize != maxfilesize) {
		pwarn(
		    "INCORRECT MAXFILESIZE=%llu IN SUPERBLOCK (should be %llu with bshift %d)",
		    (unsigned long long) fs->lfs_maxfilesize,
		    (unsigned long long) maxfilesize, (int)fs->lfs_bshift);
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("FIX") == 1) {
			fs->lfs_maxfilesize = maxfilesize;
			sbdirty();
		}
	}
	if (fs->lfs_maxsymlinklen != MAXSYMLINKLEN_UFS1) {
		pwarn("INCORRECT MAXSYMLINKLEN=%d IN SUPERBLOCK",
		    fs->lfs_maxsymlinklen);
		fs->lfs_maxsymlinklen = MAXSYMLINKLEN_UFS1;
		if (preen)
			printf(" (FIXED)\n");
		if (preen || reply("FIX") == 1) {
			sbdirty();
		}
	}

	/*
	 * Read in the Ifile; we'll be using it a lot.
	 * XXX If the Ifile is corrupted we are in bad shape.  We need to
	 * XXX run through the segment headers of the entire disk to
	 * XXX reconstruct the inode table, then pretend all segments are
	 * XXX dirty while we do the rest.
	 */
	ivp = fs->lfs_ivnode;
	maxino = ((VTOI(ivp)->i_ffs1_size - (fs->lfs_cleansz + fs->lfs_segtabsz)
		* fs->lfs_bsize) / fs->lfs_bsize) * fs->lfs_ifpb;
	if (debug)
		printf("maxino    = %d\n", maxino);
	for (i = 0; i < VTOI(ivp)->i_ffs1_size; i += fs->lfs_bsize) {
		bread(ivp, i >> fs->lfs_bshift, fs->lfs_bsize, NOCRED, &bp);
		/* XXX check B_ERROR */
		brelse(bp);
	}

	/*
	 * allocate and initialize the necessary maps
	 */
	din_table = (ufs_daddr_t *) malloc(maxino * sizeof(*din_table));
	memset(din_table, 0, maxino * sizeof(*din_table));
	seg_table = (SEGUSE *) malloc(fs->lfs_nseg * sizeof(SEGUSE));
	memset(seg_table, 0, fs->lfs_nseg * sizeof(SEGUSE));
	/* Get segment flags */
	for (i = 0; i < fs->lfs_nseg; i++) {
		LFS_SEGENTRY(sup, fs, i, bp);
		seg_table[i].su_flags = sup->su_flags & ~SEGUSE_ACTIVE;
		if (preen)
			seg_table[i].su_nbytes = sup->su_nbytes;
		brelse(bp);
	}

	/* Initialize Ifile entry */
	din_table[fs->lfs_ifile] = fs->lfs_idaddr;
	seg_table[dtosn(fs, fs->lfs_idaddr)].su_nbytes += DINODE1_SIZE;

#ifndef VERBOSE_BLOCKMAP
	bmapsize = roundup(howmany(maxfsblock, NBBY), sizeof(int16_t));
	blockmap = calloc((unsigned) bmapsize, sizeof(char));
#else
	bmapsize = maxfsblock * sizeof(ino_t);
	blockmap = (ino_t *) calloc(maxfsblock, sizeof(ino_t));
#endif
	if (blockmap == NULL) {
		printf("cannot alloc %u bytes for blockmap\n",
		    (unsigned) bmapsize);
		goto badsblabel;
	}
	statemap = calloc((unsigned) maxino, sizeof(char));
	if (statemap == NULL) {
		printf("cannot alloc %u bytes for statemap\n",
		    (unsigned) maxino);
		goto badsblabel;
	}
	typemap = calloc((unsigned) maxino, sizeof(char));
	if (typemap == NULL) {
		printf("cannot alloc %u bytes for typemap\n",
		    (unsigned) maxino);
		goto badsblabel;
	}
	lncntp = (int16_t *) calloc((unsigned) maxino, sizeof(int16_t));
	if (lncntp == NULL) {
		printf("cannot alloc %lu bytes for lncntp\n",
		    (unsigned long) maxino * sizeof(int16_t));
		goto badsblabel;
	}

	if (preen) {
		n_files = fs->lfs_nfiles;
		n_blks  = fs->lfs_dsize - fs->lfs_bfree;
		numdirs = maxino;
		inplast = 0; 
		listmax = numdirs + 10;
		inpsort = (struct inoinfo **) calloc((unsigned) listmax,
			sizeof(struct inoinfo *));
		inphead = (struct inoinfo **) calloc((unsigned) numdirs, 
			sizeof(struct inoinfo *));
		if (inpsort == NULL || inphead == NULL) { 
			printf("cannot alloc %lu bytes for inphead\n",
				(unsigned long) numdirs * sizeof(struct inoinfo *));  
			exit(1);
		}
	}

	return (1);

badsblabel:
	ckfini(0);
	return (0);
}

static struct disklabel *
getdisklabel(const char *s, int fd)
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *) &lab) < 0) {
		if (s == NULL)
			return ((struct disklabel *) NULL);
		pwarn("ioctl (GCINFO): %s\n", strerror(errno));
		return NULL;
	}
	return (&lab);
}
