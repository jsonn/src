/* $NetBSD: utilities.c,v 1.7.2.1 2001/06/27 03:49:41 perseant Exp $	 */

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

#include <sys/param.h>
#include <sys/time.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <sys/mount.h>
#include <ufs/lfs/lfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <signal.h>

#include "fsutil.h"
#include "fsck.h"
#include "extern.h"

long            diskreads, totalreads;	/* Disk cache statistics */

static void     rwerror(char *, daddr_t);

extern int      returntosingle;

int
ftypeok(struct dinode * dp)
{
	switch (dp->di_mode & IFMT) {

		case IFDIR:
		case IFREG:
		case IFBLK:
		case IFCHR:
		case IFLNK:
		case IFSOCK:
		case IFIFO:
		return (1);

	default:
		if (debug)
			printf("bad file type 0%o\n", dp->di_mode);
		return (0);
	}
}

int
reply(char *question)
{
	int             persevere;
	char            c;

	if (preen)
		pfatal("INTERNAL ERROR: GOT TO reply()");
	persevere = !strcmp(question, "CONTINUE");
	printf("\n");
	if (!persevere && (nflag || fswritefd < 0)) {
		printf("%s? no\n\n", question);
		return (0);
	}
	if (yflag || (persevere && nflag)) {
		printf("%s? yes\n\n", question);
		return (1);
	}
	do {
		printf("%s? [yn] ", question);
		(void)fflush(stdout);
		c = getc(stdin);
		while (c != '\n' && getc(stdin) != '\n')
			if (feof(stdin))
				return (0);
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
	printf("\n");
	if (c == 'y' || c == 'Y')
		return (1);
	return (0);
}

/*
 * Malloc buffers and set up cache.
 */
void
bufinit()
{
	register struct bufarea *bp;
	long            bufcnt, i;
	char           *bufp;

	pbp = pdirbp = (struct bufarea *)0;
	bufp = malloc((unsigned int)sblock.lfs_bsize);
	if (bufp == 0)
		errexit("cannot allocate buffer pool\n");
	/* cgblk.b_un.b_buf = bufp; */
	/* initbarea(&cgblk); */
	bufhead.b_next = bufhead.b_prev = &bufhead;
	bufcnt = MAXBUFSPACE / sblock.lfs_bsize;
	if (bufcnt < MINBUFS)
		bufcnt = MINBUFS;
	for (i = 0; i < bufcnt; i++) {
		bp = (struct bufarea *)malloc(sizeof(struct bufarea));
		bufp = malloc((unsigned int)sblock.lfs_bsize);
		if (bp == NULL || bufp == NULL) {
			if (i >= MINBUFS)
				break;
			errexit("cannot allocate buffer pool\n");
		}
		bp->b_un.b_buf = bufp;
		bp->b_prev = &bufhead;
		bp->b_next = bufhead.b_next;
		bufhead.b_next->b_prev = bp;
		bufhead.b_next = bp;
		initbarea(bp);
	}
	bufhead.b_size = i;	/* save number of buffers */
}

/*
 * Manage a cache of directory blocks.
 */
struct bufarea *
getddblk(daddr_t blkno, long size)
{
	register struct bufarea *bp;

	for (bp = bufhead.b_next; bp != &bufhead; bp = bp->b_next)
		if (bp->b_bno == blkno) {
			if (bp->b_size <= size)
				getdblk(bp, blkno, size);
			goto foundit;
		}
	for (bp = bufhead.b_prev; bp != &bufhead; bp = bp->b_prev)
		if ((bp->b_flags & B_INUSE) == 0)
			break;
	if (bp == &bufhead)
		errexit("deadlocked buffer pool\n");
	getdblk(bp, blkno, size);
	/* fall through */
foundit:
	totalreads++;
	bp->b_prev->b_next = bp->b_next;
	bp->b_next->b_prev = bp->b_prev;
	bp->b_prev = &bufhead;
	bp->b_next = bufhead.b_next;
	bufhead.b_next->b_prev = bp;
	bufhead.b_next = bp;
	bp->b_flags |= B_INUSE;
	return (bp);
}

struct bufarea *
getdatablk(daddr_t blkno, long size)
{
	return getddblk(fsbtodb(&sblock, blkno), size);
}

void
getdblk(struct bufarea * bp, daddr_t blk, long size)
{
	if (bp->b_bno != blk) {
		flush(fswritefd, bp);
		diskreads++;
		bp->b_errs = bread(fsreadfd, bp->b_un.b_buf, blk, size);
		bp->b_bno = blk;
		bp->b_size = size;
	}
}

void
getblk(struct bufarea * bp, daddr_t blk, long size)
{
	getdblk(bp, fsbtodb(&sblock, blk), size);
}

void
flush(int fd, struct bufarea * bp)
{
	if (!bp->b_dirty)
		return;
	if (bp->b_errs != 0)
		pfatal("WRITING %sZERO'ED BLOCK %d TO DISK\n",
		 (bp->b_errs == bp->b_size / dev_bsize) ? "" : "PARTIALLY ",
		       bp->b_bno);
	bp->b_dirty = 0;
	bp->b_errs = 0;
	bwrite(fd, bp->b_un.b_buf, bp->b_bno, (long)bp->b_size);
	if (bp != &sblk)
		return;
#if 0				/* XXX - FFS */
	for (i = 0, j = 0; i < sblock.lfs_cssize; i += sblock.lfs_bsize, j++) {
		bwrite(fswritefd, (char *)sblock.lfs_csp[j],
		  fsbtodb(&sblock, sblock.lfs_csaddr + j * sblock.lfs_frag),
		       sblock.lfs_cssize - i < sblock.lfs_bsize ?
		       sblock.lfs_cssize - i : sblock.lfs_bsize);
	}
#endif
}

static void
rwerror(char *mesg, daddr_t blk)
{

	if (preen == 0)
		printf("\n");
	pfatal("CANNOT %s: BLK %d", mesg, blk);
	if (reply("CONTINUE") == 0)
		errexit("Program terminated\n");
}

void
ckfini(int markclean)
{
	register struct bufarea *bp, *nbp;
	int             cnt = 0;

	if (fswritefd < 0) {
		(void)close(fsreadfd);
		return;
	}
	flush(fswritefd, &sblk);
	if (havesb && sblk.b_bno != LFS_LABELPAD / dev_bsize &&
	    sblk.b_bno != sblock.lfs_sboffs[0] &&
	    !preen && reply("UPDATE STANDARD SUPERBLOCKS")) {
		sblk.b_bno = LFS_LABELPAD / dev_bsize;
		sbdirty();
		flush(fswritefd, &sblk);
	}
	if (havesb) {
		if (sblk.b_bno == LFS_LABELPAD / dev_bsize) {
			/* Do the first alternate */
			sblk.b_bno = sblock.lfs_sboffs[1];
			sbdirty();
			flush(fswritefd, &sblk);
		} else if (sblk.b_bno == sblock.lfs_sboffs[1]) {
			/* Do the primary */
			sblk.b_bno = LFS_LABELPAD / dev_bsize;
			sbdirty();
			flush(fswritefd, &sblk);
		}
	}
	/* flush(fswritefd, &cgblk); */
	/* free(cgblk.b_un.b_buf); */
	for (bp = bufhead.b_prev; bp && bp != &bufhead; bp = nbp) {
		cnt++;
		flush(fswritefd, bp);
		nbp = bp->b_prev;
		free(bp->b_un.b_buf);
		free((char *)bp);
	}
	if (bufhead.b_size != cnt)
		errexit("Panic: lost %d buffers\n", bufhead.b_size - cnt);
	pbp = pdirbp = (struct bufarea *)0;
	if (markclean && !(sblock.lfs_pflags & LFS_PF_CLEAN)) {
		/*
		 * Mark the file system as clean, and sync the superblock.
		 */
		if (preen)
			pwarn("MARKING FILE SYSTEM CLEAN\n");
		else if (!reply("MARK FILE SYSTEM CLEAN"))
			markclean = 0;
		if (markclean) {
			sblock.lfs_pflags |= LFS_PF_CLEAN;
			sbdirty();
			flush(fswritefd, &sblk);
			if (sblk.b_bno == LFS_LABELPAD / dev_bsize) {
				/* Do the first alternate */
				sblk.b_bno = sblock.lfs_sboffs[0];
				flush(fswritefd, &sblk);
			} else if (sblk.b_bno == sblock.lfs_sboffs[0]) {
				/* Do the primary */
				sblk.b_bno = LFS_LABELPAD / dev_bsize;
				flush(fswritefd, &sblk);
			}
		}
	}
	if (debug)
		printf("cache missed %ld of %ld (%d%%)\n", diskreads,
		       totalreads, (int)(diskreads * 100 / totalreads));
	(void)close(fsreadfd);
	(void)close(fswritefd);
}

int
bread(int fd, char *buf, daddr_t blk, long size)
{
	char           *cp;
	int             i, errs;
	off_t           offset;

	offset = blk;
	offset *= dev_bsize;
	if (lseek(fd, offset, 0) < 0) {
		rwerror("SEEK", blk);
	} else if (read(fd, buf, (int)size) == size)
		return (0);
	rwerror("READ", blk);
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK", blk);
	errs = 0;
	memset(buf, 0, (size_t)size);
	printf("THE FOLLOWING DISK SECTORS COULD NOT BE READ:");
	for (cp = buf, i = 0; i < size; i += secsize, cp += secsize) {
		if (read(fd, cp, (int)secsize) != secsize) {
			(void)lseek(fd, offset + i + secsize, 0);
			if (secsize != dev_bsize && dev_bsize != 1)
				printf(" %ld (%ld),",
				       (blk * dev_bsize + i) / secsize,
				       blk + i / dev_bsize);
			else
				printf(" %ld,", blk + i / dev_bsize);
			errs++;
		}
	}
	printf("\n");
	return (errs);
}

void
bwrite(int fd, char *buf, daddr_t blk, long size)
{
	int             i;
	char           *cp;
	off_t           offset;

	if (fd < 0)
		return;
	offset = blk;
	offset *= dev_bsize;
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK", blk);
	else if (write(fd, buf, (int)size) == size) {
		fsmodified = 1;
		return;
	}
	rwerror("WRITE", blk);
	if (lseek(fd, offset, 0) < 0)
		rwerror("SEEK", blk);
	printf("THE FOLLOWING SECTORS COULD NOT BE WRITTEN:");
	for (cp = buf, i = 0; i < size; i += dev_bsize, cp += dev_bsize)
		if (write(fd, cp, (int)dev_bsize) != dev_bsize) {
			(void)lseek(fd, offset + i + dev_bsize, 0);
			printf(" %ld,", blk + i / dev_bsize);
		}
	printf("\n");
	return;
}

/*
 * allocate a data block with the specified number of fragments
 */
int
allocblk(long frags)
{
#if 1
	/*
	 * XXX Can't allocate blocks right now because we would have to do
	 * a full partial segment write.
	 */
	return 0;
#else				/* 0 */
	register int    i, j, k;

	if (frags <= 0 || frags > sblock.lfs_frag)
		return (0);
	for (i = 0; i < maxfsblock - sblock.lfs_frag; i += sblock.lfs_frag) {
		for (j = 0; j <= sblock.lfs_frag - frags; j++) {
			if (testbmap(i + j))
				continue;
			for (k = 1; k < frags; k++)
				if (testbmap(i + j + k))
					break;
			if (k < frags) {
				j += k;
				continue;
			}
			for (k = 0; k < frags; k++) {
#ifndef VERBOSE_BLOCKMAP
				setbmap(i + j + k);
#else
				setbmap(i + j + k, -1);
#endif
			}
			n_blks += frags;
			return (i + j);
		}
	}
	return (0);
#endif				/* 0 */
}

/*
 * Free a previously allocated block
 */
void
freeblk(daddr_t blkno, long frags)
{
	struct inodesc  idesc;

	idesc.id_blkno = blkno;
	idesc.id_numfrags = frags;
	(void)pass4check(&idesc);
}

/*
 * Find a pathname
 */
void
getpathname(char *namebuf, ino_t curdir, ino_t ino)
{
	int             len;
	register char  *cp;
	struct inodesc  idesc;
	static int      busy = 0;

	if (curdir == ino && ino == ROOTINO) {
		(void)strcpy(namebuf, "/");
		return;
	}
	if (busy ||
	    (statemap[curdir] != DSTATE && statemap[curdir] != DFOUND)) {
		(void)strcpy(namebuf, "?");
		return;
	}
	busy = 1;
	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = DATA;
	idesc.id_fix = IGNORE;
	cp = &namebuf[MAXPATHLEN - 1];
	*cp = '\0';
	if (curdir != ino) {
		idesc.id_parent = curdir;
		goto namelookup;
	}
	while (ino != ROOTINO) {
		idesc.id_number = ino;
		idesc.id_func = findino;
		idesc.id_name = "..";
		if ((ckinode(ginode(ino), &idesc) & FOUND) == 0)
			break;
namelookup:
		idesc.id_number = idesc.id_parent;
		idesc.id_parent = ino;
		idesc.id_func = findname;
		idesc.id_name = namebuf;
		if ((ckinode(ginode(idesc.id_number), &idesc) & FOUND) == 0)
			break;
		len = strlen(namebuf);
		cp -= len;
		memcpy(cp, namebuf, (size_t)len);
		*--cp = '/';
		if (cp < &namebuf[MAXNAMLEN])
			break;
		ino = idesc.id_number;
	}
	busy = 0;
	if (ino != ROOTINO)
		*--cp = '?';
	memcpy(namebuf, cp, (size_t)(&namebuf[MAXPATHLEN] - cp));
}

void
catch(int n)
{
	if (!doinglevel2)
		ckfini(0);
	exit(12);
}

/*
 * When preening, allow a single quit to signal
 * a special exit after filesystem checks complete
 * so that reboot sequence may be interrupted.
 */
void
catchquit(int n)
{
	printf("returning to single-user after filesystem check\n");
	returntosingle = 1;
	(void)signal(SIGQUIT, SIG_DFL);
}

/*
 * Ignore a single quit signal; wait and flush just in case.
 * Used by child processes in preen.
 */
void
voidquit(int n)
{

	sleep(1);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_DFL);
}

/*
 * determine whether an inode should be fixed.
 */
int
dofix(struct inodesc * idesc, char *msg)
{

	switch (idesc->id_fix) {

		case DONTKNOW:
		if (idesc->id_type == DATA)
			direrror(idesc->id_number, msg);
		else
			pwarn("%s", msg);
		if (preen) {
			printf(" (SALVAGED)\n");
			idesc->id_fix = FIX;
			return (ALTERED);
		}
		if (reply("SALVAGE") == 0) {
			idesc->id_fix = NOFIX;
			return (0);
		}
		idesc->id_fix = FIX;
		return (ALTERED);

	case FIX:
		return (ALTERED);

	case NOFIX:
	case IGNORE:
		return (0);

	default:
		errexit("UNKNOWN INODESC FIX MODE %d\n", idesc->id_fix);
	}
	/* NOTREACHED */
}
