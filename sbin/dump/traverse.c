/*	$NetBSD: traverse.c,v 1.28.6.1 2002/01/16 09:58:02 he Exp $	*/

/*-
 * Copyright (c) 1980, 1988, 1991, 1993
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
static char sccsid[] = "@(#)traverse.c	8.7 (Berkeley) 6/15/95";
#else
__RCSID("$NetBSD: traverse.c,v 1.28.6.1 2002/01/16 09:58:02 he Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <protocols/dumprestore.h>

#include <ctype.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#ifdef __STDC__
#include <string.h>
#include <unistd.h>
#endif

#include "dump.h"

#define	HASDUMPEDFILE	0x1
#define	HASSUBDIRS	0x2

#ifdef	FS_44INODEFMT
typedef	quad_t fsizeT;
#else
typedef	int32_t fsizeT;
#endif

static	int dirindir __P((ino_t ino, daddr_t blkno, int level, long *size,
    long *tapesize, int nodump));
static	void dmpindir __P((ino_t ino, daddr_t blk, int level, fsizeT *size));
static	int searchdir __P((ino_t ino, daddr_t blkno, long size, long filesize,
    long *tapesize, int nodump));

/*
 * This is an estimation of the number of TP_BSIZE blocks in the file.
 * It estimates the number of blocks in files with holes by assuming
 * that all of the blocks accounted for by di_blocks are data blocks
 * (when some of the blocks are usually used for indirect pointers);
 * hence the estimate may be high.
 */
long
blockest(dp)
	struct dinode *dp;
{
	long blkest, sizeest;

	/*
	 * dp->di_size is the size of the file in bytes.
	 * dp->di_blocks stores the number of sectors actually in the file.
	 * If there are more sectors than the size would indicate, this just
	 *	means that there are indirect blocks in the file or unused
	 *	sectors in the last file block; we can safely ignore these
	 *	(blkest = sizeest below).
	 * If the file is bigger than the number of sectors would indicate,
	 *	then the file has holes in it.	In this case we must use the
	 *	block count to estimate the number of data blocks used, but
	 *	we use the actual size for estimating the number of indirect
	 *	dump blocks (sizeest vs. blkest in the indirect block
	 *	calculation).
	 */
	blkest = howmany(dbtob((u_int64_t)dp->di_blocks), TP_BSIZE);
	sizeest = howmany(dp->di_size, TP_BSIZE);
	if (blkest > sizeest)
		blkest = sizeest;
	if (dp->di_size > ufsib->ufs_bsize * NDADDR) {
		/* calculate the number of indirect blocks on the dump tape */
		blkest +=
			howmany(sizeest - NDADDR * ufsib->ufs_bsize / TP_BSIZE,
			TP_NINDIR);
	}
	return (blkest + 1);
}

/* Auxiliary macro to pick up files changed since previous dump. */
#define	CHANGEDSINCE(dp, t) \
	((dp)->di_mtime >= (t) || (dp)->di_ctime >= (t))

/* The WANTTODUMP macro decides whether a file should be dumped. */
#ifdef UF_NODUMP
#define	WANTTODUMP(dp) \
	(CHANGEDSINCE(dp, iswap32(spcl.c_ddate)) && \
	 (nonodump || ((dp)->di_flags & UF_NODUMP) != UF_NODUMP))
#else
#define	WANTTODUMP(dp) CHANGEDSINCE(dp, iswap32(spcl.c_ddate))
#endif

/*
 * Determine if given inode should be dumped
 */
void
mapfileino(ino, tapesize, dirskipped)
	ino_t ino;
	long *tapesize;
	int *dirskipped;
{
	int mode;
	struct dinode *dp;

	/*
	 * Skip inode if we've already marked it for dumping
	 */
	if (TSTINO(ino, usedinomap))
		return;
	dp = getino(ino);
	if ((mode = (dp->di_mode & IFMT)) == 0)
		return;
	/*
	 * Put all dirs in dumpdirmap, inodes that are to be dumped in the
	 * used map. All inode but dirs who have the nodump attribute go
	 * to the usedinomap.
	 */
	SETINO(ino, usedinomap);
	if (mode == IFDIR)
		SETINO(ino, dumpdirmap);
	if (WANTTODUMP(dp)) {
		SETINO(ino, dumpinomap);
		if (mode != IFREG && mode != IFDIR && mode != IFLNK)
			*tapesize += 1;
		else
			*tapesize += blockest(dp);
		return;
	}
	if (mode == IFDIR) {
#ifdef UF_NODUMP
		if (!nonodump && (dp->di_flags & UF_NODUMP))
			CLRINO(ino, usedinomap);
#endif
		*dirskipped = 1;
	}
}

/*
 * Dump pass 1.
 *
 * Walk the inode list for a filesystem to find all allocated inodes
 * that have been modified since the previous dump time. Also, find all
 * the directories in the filesystem.
 */
int
mapfiles(maxino, tapesize, disk, dirv)
	ino_t maxino;
	long *tapesize;
	char *disk;
	char * const *dirv;
{
	int anydirskipped = 0;

	if (dirv != NULL) {
		char	 curdir[MAXPATHLEN];
		FTS	*dirh;
		FTSENT	*entry;
		int	 d;

		if (getcwd(curdir, sizeof(curdir)) == NULL) {
			msg("Can't determine cwd: %s\n", strerror(errno));
			dumpabort(0);
		}
		if ((dirh = fts_open(dirv, FTS_PHYSICAL|FTS_SEEDOT|FTS_XDEV,
		    		    NULL)) == NULL) {
			msg("fts_open failed: %s\n", strerror(errno));
			dumpabort(0);
		}
		while ((entry = fts_read(dirh)) != NULL) {
			switch (entry->fts_info) {
			case FTS_DNR:		/* an error */
			case FTS_ERR:
			case FTS_NS:
				msg("Can't fts_read %s: %s\n", entry->fts_path,
				    strerror(errno));
			case FTS_DP:		/* already seen dir */
				continue;
			}
			mapfileino(entry->fts_statp->st_ino, tapesize,
			    &anydirskipped);
		}
		(void)fts_close(dirh);

		/*
		 * Add any parent directories
		 */
		for (d = 0 ; dirv[d] != NULL ; d++) {
			char path[MAXPATHLEN];

			if (dirv[d][0] != '/')
				(void)snprintf(path, sizeof(path), "%s/%s",
				    curdir, dirv[d]);
			else
				(void)snprintf(path, sizeof(path), "%s",
				    dirv[d]);
			while (strcmp(path, disk) != 0) {
				char *p;
				struct stat sb;

				if (*path == '\0')
					break;
				if ((p = strrchr(path, '/')) == NULL)
					break;
				if (p == path)
					break;
				*p = '\0';
				if (stat(path, &sb) == -1) {
					msg("Can't stat %s: %s\n", path,
					    strerror(errno));
					break;
				}
				mapfileino(sb.st_ino, tapesize, &anydirskipped);
			}
		}

		/*
		 * Ensure that the root inode actually appears in the
		 * file list for a subdir
		 */
		mapfileino(ROOTINO, tapesize, &anydirskipped);
	} else {
		ino_t ino;

		for (ino = ROOTINO; ino < maxino; ino++) {
			mapfileino(ino, tapesize, &anydirskipped);
		}
	}
	/*
	 * Restore gets very upset if the root is not dumped,
	 * so ensure that it always is dumped.
	 */
	SETINO(ROOTINO, dumpinomap);
	return (anydirskipped);
}

/*
 * Dump pass 2.
 *
 * Scan each directory on the filesystem to see if it has any modified
 * files in it. If it does, and has not already been added to the dump
 * list (because it was itself modified), then add it. If a directory
 * has not been modified itself, contains no modified files and has no
 * subdirectories, then it can be deleted from the dump list and from
 * the list of directories. By deleting it from the list of directories,
 * its parent may now qualify for the same treatment on this or a later
 * pass using this algorithm.
 */
int
mapdirs(maxino, tapesize)
	ino_t maxino;
	long *tapesize;
{
	struct  dinode *dp, di;
	int i, isdir, nodump;
	char *map;
	ino_t ino;
	long filesize;
	int ret, change = 0;

	isdir = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			isdir = *map++;
		else
			isdir >>= 1;
		/*
		 * If dir has been removed from the used map, it's either
		 * because it had the nodump flag, or it herited it from
		 * its parent. A directory can't be in dumpinomap if
		 * not in usedinomap, but we have to go throuh it anyway
		 * to propagate the nodump attribute.
		 */
		nodump = (TSTINO(ino, usedinomap) == 0);
		if ((isdir & 1) == 0 ||
		    (TSTINO(ino, dumpinomap) && nodump == 0))
			continue;

		dp = getino(ino);
		di = *dp; /* inode buf may be changed in searchdir */
		filesize = di.di_size;
		for (ret = 0, i = 0; filesize > 0 && i < NDADDR; i++) {
			if (di.di_db[i] != 0)
				ret |= searchdir(ino, iswap32(di.di_db[i]),
					(long)ufs_dblksize(ufsib, &di, i),
					filesize, tapesize, nodump);
			if (ret & HASDUMPEDFILE)
				filesize = 0;
			else
				filesize -= ufsib->ufs_bsize;
		}
		for (i = 0; filesize > 0 && i < NIADDR; i++) {
			if (di.di_ib[i] == 0)
				continue;
			ret |= dirindir(ino, di.di_ib[i], i, &filesize,
			    tapesize, nodump);
		}
		if (ret & HASDUMPEDFILE) {
			SETINO(ino, dumpinomap);
			*tapesize += blockest(&di);
			change = 1;
			continue;
		}
		if (nodump) {
			if (ret & HASSUBDIRS)
				change = 1; /* subdirs have inherited nodump */
			CLRINO(ino, dumpdirmap);
		} else if ((ret & HASSUBDIRS) == 0) {
			if (!TSTINO(ino, dumpinomap)) {
				CLRINO(ino, dumpdirmap);
				change = 1;
			}
		}
	}
	return (change);
}

/*
 * Read indirect blocks, and pass the data blocks to be searched
 * as directories. Quit as soon as any entry is found that will
 * require the directory to be dumped.
 */
static int
dirindir(ino, blkno, ind_level, filesize, tapesize, nodump)
	ino_t ino;
	daddr_t blkno;
	int ind_level;
	long *filesize;
	long *tapesize;
	int nodump;
{
	int ret = 0;
	int i;
	daddr_t	idblk[MAXNINDIR];

	bread(fsatoda(ufsib, iswap32(blkno)), (char *)idblk,
		(int)ufsib->ufs_bsize);
	if (ind_level <= 0) {
		for (i = 0; *filesize > 0 && i < ufsib->ufs_nindir; i++) {
			blkno = idblk[i];
			if (blkno != 0)
				ret |= searchdir(ino, iswap32(blkno),
				    ufsib->ufs_bsize, *filesize,
				    tapesize, nodump);
			if (ret & HASDUMPEDFILE)
				*filesize = 0;
			else
				*filesize -= ufsib->ufs_bsize;
		}
		return (ret);
	}
	ind_level--;
	for (i = 0; *filesize > 0 && i < ufsib->ufs_nindir; i++) {
		blkno = idblk[i];
		if (blkno != 0)
			ret |= dirindir(ino, blkno, ind_level, filesize,
			    tapesize, nodump);
	}
	return (ret);
}

/*
 * Scan a disk block containing directory information looking to see if
 * any of the entries are on the dump list and to see if the directory
 * contains any subdirectories.
 */
static int
searchdir(dino, blkno, size, filesize, tapesize, nodump)
	ino_t dino;
	daddr_t blkno;
	long size;
	long filesize;
	long *tapesize;
	int nodump;
{
	struct direct *dp;
	struct dinode *ip;
	long loc, ret = 0;
	char dblk[MAXBSIZE];
	ino_t ino;

	bread(fsatoda(ufsib, blkno), dblk, (int)size);
	if (filesize < size)
		size = filesize;
	for (loc = 0; loc < size; ) {
		dp = (struct direct *)(dblk + loc);
		if (dp->d_reclen == 0) {
			msg("corrupted directory, inumber %d\n", dino);
			break;
		}
		loc += iswap16(dp->d_reclen);
		if (dp->d_ino == 0)
			continue;
		if (dp->d_name[0] == '.') {
			if (dp->d_name[1] == '\0')
				continue;
			if (dp->d_name[1] == '.' && dp->d_name[2] == '\0')
				continue;
		}
		ino = iswap32(dp->d_ino);
		if (nodump) {
			ip = getino(ino);
			if (TSTINO(ino, dumpinomap)) {
				CLRINO(ino, dumpinomap);
				CLRINO(ino, usedinomap);
				*tapesize -= blockest(ip);
			}
			/* Add dir back to the dir map, to propagate nodump */
			if ((ip->di_mode & IFMT) == IFDIR) {
				SETINO(ino, dumpdirmap);
				ret |= HASSUBDIRS;
			}
		} else {
			if (TSTINO(ino, dumpinomap)) {
				ret |= HASDUMPEDFILE;
				if (ret & HASSUBDIRS)
					break;
			}
			if (TSTINO(ino, dumpdirmap)) {
				ret |= HASSUBDIRS;
				if (ret & HASDUMPEDFILE)
					break;
			}
		}
	}
	return (ret);
}

/*
 * Dump passes 3 and 4.
 *
 * Dump the contents of an inode to tape.
 */
void
dumpino(dp, ino)
	struct dinode *dp;
	ino_t ino;
{
	int ind_level, cnt;
	fsizeT size;
	char buf[TP_BSIZE];

	if (newtape) {
		newtape = 0;
		dumpmap(dumpinomap, TS_BITS, ino);
	}
	CLRINO(ino, dumpinomap);
	if (needswap)
		ffs_dinode_swap(dp, &spcl.c_dinode);
	else
		spcl.c_dinode = *dp;
	spcl.c_type = iswap32(TS_INODE);
	spcl.c_count = 0;
	switch (dp->di_mode & IFMT) {

	case 0:
		/*
		 * Freed inode.
		 */
		return;

	case IFLNK:
		/*
		 * Check for short symbolic link.
		 */
		if (dp->di_size > 0 &&
#ifdef FS_44INODEFMT
		    (dp->di_size < ufsib->ufs_maxsymlinklen ||
		     (ufsib->ufs_maxsymlinklen == 0 && dp->di_blocks == 0))) {
#else
		    dp->di_blocks == 0) {
#endif
			spcl.c_addr[0] = 1;
			spcl.c_count = iswap32(1);
			writeheader(ino);
			memmove(buf, dp->di_shortlink, (u_long)dp->di_size);
			buf[dp->di_size] = '\0';
			writerec(buf, 0);
			return;
		}
		/* fall through */

	case IFDIR:
	case IFREG:
		if (dp->di_size > 0)
			break;
		/* fall through */

	case IFIFO:
	case IFSOCK:
	case IFCHR:
	case IFBLK:
		writeheader(ino);
		return;

	default:
		msg("Warning: undefined file type 0%o\n", dp->di_mode & IFMT);
		return;
	}
	if (dp->di_size > NDADDR * ufsib->ufs_bsize)
		cnt = NDADDR * ufsib->ufs_frag;
	else
		cnt = howmany(dp->di_size, ufsib->ufs_fsize);
	blksout(&dp->di_db[0], cnt, ino);
	if ((size = dp->di_size - NDADDR * ufsib->ufs_bsize) <= 0)
		return;
	for (ind_level = 0; ind_level < NIADDR; ind_level++) {
		dmpindir(ino, dp->di_ib[ind_level], ind_level, &size);
		if (size <= 0)
			return;
	}
}

/*
 * Read indirect blocks, and pass the data blocks to be dumped.
 */
static void
dmpindir(ino, blk, ind_level, size)
	ino_t ino;
	daddr_t blk;
	int ind_level;
	fsizeT *size;
{
	int i, cnt;
	daddr_t idblk[MAXNINDIR];

	if (blk != 0)
		bread(fsatoda(ufsib, iswap32(blk)), (char *)idblk,
			(int) ufsib->ufs_bsize);
	else
		memset(idblk, 0, (int)ufsib->ufs_bsize);
	if (ind_level <= 0) {
		if (*size < ufsib->ufs_nindir * ufsib->ufs_bsize)
			cnt = howmany(*size, ufsib->ufs_fsize);
		else
			cnt = ufsib->ufs_nindir * ufsib->ufs_frag;
		*size -= ufsib->ufs_nindir * ufsib->ufs_bsize;
		blksout(&idblk[0], cnt, ino);
		return;
	}
	ind_level--;
	for (i = 0; i < ufsib->ufs_nindir; i++) {
		dmpindir(ino, idblk[i], ind_level, size);
		if (*size <= 0)
			return;
	}
}

/*
 * Collect up the data into tape record sized buffers and output them.
 */
void
blksout(blkp, frags, ino)
	daddr_t *blkp;
	int frags;
	ino_t ino;
{
	daddr_t *bp;
	int i, j, count, blks, tbperdb;

	blks = howmany(frags * ufsib->ufs_fsize, TP_BSIZE);
	tbperdb = ufsib->ufs_bsize >> tp_bshift;
	for (i = 0; i < blks; i += TP_NINDIR) {
		if (i + TP_NINDIR > blks)
			count = blks;
		else
			count = i + TP_NINDIR;
		for (j = i; j < count; j++)
			if (blkp[j / tbperdb] != 0)
				spcl.c_addr[j - i] = 1;
			else
				spcl.c_addr[j - i] = 0;
		spcl.c_count = iswap32(count - i);
		writeheader(ino);
		bp = &blkp[i / tbperdb];
		for (j = i; j < count; j += tbperdb, bp++)
			if (*bp != 0) {
				if (j + tbperdb <= count)
					dumpblock(iswap32(*bp), (int)ufsib->ufs_bsize);
				else
					dumpblock(iswap32(*bp), (count - j) * TP_BSIZE);
			}
		spcl.c_type = iswap32(TS_ADDR);
	}
}

/*
 * Dump a map to the tape.
 */
void
dumpmap(map, type, ino)
	char *map;
	int type;
	ino_t ino;
{
	int i;
	char *cp;

	spcl.c_type = iswap32(type);
	spcl.c_count = iswap32(howmany(mapsize * sizeof(char), TP_BSIZE));
	writeheader(ino);
	for (i = 0, cp = map; i < iswap32(spcl.c_count); i++, cp += TP_BSIZE)
		writerec(cp, 0);
}

/*
 * Write a header record to the dump tape.
 */
void
writeheader(ino)
	ino_t ino;
{
	int32_t sum, cnt, *lp;

	spcl.c_inumber = iswap32(ino);
	spcl.c_magic = iswap32(NFS_MAGIC);
	spcl.c_checksum = 0;
	lp = (int32_t *)&spcl;
	sum = 0;
	cnt = sizeof(union u_spcl) / (4 * sizeof(int32_t));
	while (--cnt >= 0) {
		sum += iswap32(*lp++);
		sum += iswap32(*lp++);
		sum += iswap32(*lp++);
		sum += iswap32(*lp++);
	}
	spcl.c_checksum = iswap32(CHECKSUM - sum);
	writerec((char *)&spcl, 1);
}
