/*	$NetBSD: installboot.c,v 1.1.8.2 2002/01/08 00:28:23 nathanw Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "loadfile.h"
#include "byteorder.h"
#include "bbinfo.h"

int	verbose, nowrite;
char	*boot, *proto, *dev;

struct bbinfo *bbinfop;		/* bbinfo in prototype image */

int32_t	max_block_count;

char		*loadprotoblocks __P((char *, size_t *));
int		loadblocknums __P((char *, int));
static void	devread __P((int, void *, daddr_t, size_t, char *));
static void	usage __P((void));
int 		main __P((int, char *[]));

static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] [-h] <boot> <proto> <device>\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	c;
	int	devfd;
	char	*protostore;
	size_t	protosize;

	while ((c = getopt(argc, argv, "vnh")) != -1) {
		switch (c) {
		case 'h':
			/* Don't strip a.out header */
			warnx("-h option is obsolete");
			break;
		case 'n':
			/* Do not actually write the bootblock to disk */
			nowrite = 1;
			break;
		case 'v':
			/* Chat */
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind < 3) {
		usage();
	}

	boot = argv[optind];
	proto = argv[optind + 1];
	dev = argv[optind + 2];

	if (verbose) {
		printf("boot: %s\n", boot);
		printf("proto: %s\n", proto);
		printf("device: %s\n", dev);
	}

	/* Load proto blocks into core */
	if ((protostore = loadprotoblocks(proto, &protosize)) == NULL)
		exit(1);

	if (protosize & (DEV_BSIZE - 1))
		err(1, "proto bootblock bad size=%lu", (u_long)protosize);

	/* Open and check raw disk device */
	if ((devfd = open(dev, O_RDONLY, 0)) < 0)
		err(1, "open: %s", dev);

	/* Extract and load block numbers */
	if (loadblocknums(boot, devfd) != 0)
		exit(1);

	(void)close(devfd);

	if (nowrite)
		return 0;

	/* Write patched proto bootblocks into the superblock */
	if (protosize > SBSIZE - DEV_BSIZE)
		errx(1, "proto bootblocks too big");

	if ((devfd = open(dev, O_RDWR, 0)) < 0)
		err(1, "open: %s", dev);

	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek bootstrap");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync();

	if (write(devfd, protostore, protosize) != protosize)
		err(1, "write bootstrap");
	(void)close(devfd);
	return 0;
}

char *
loadprotoblocks(fname, size)
	char *fname;
	size_t *size;
{
	int	fd, sz;
	u_long	ap, bp, st, en, bbi;
	u_long	marks[MARK_MAX];

	marks[MARK_START] = 0;
	if ((fd = loadfile(fname, marks, COUNT_TEXT|COUNT_DATA)) == -1)
		return NULL;
	(void)close(fd);

	sz = (marks[MARK_END] - marks[MARK_START]);
	sz = roundup(sz, DEV_BSIZE);
	st = marks[MARK_START];
	en = marks[MARK_ENTRY];

	if ((ap = (u_long)malloc(sz)) == NULL) {
		warn("malloc: %s", "");
		return NULL;
	}

	bp = ap;
	marks[MARK_START] = bp - st;
	if ((fd = loadfile(fname, marks, LOAD_TEXT|LOAD_DATA)) == -1)
		return NULL;
	(void)close(fd);

	/* Look for the bbinfo structure. */
	for (bbi = bp; bbi < (bp + sz); bbi += sizeof(uint32_t)) {
		bbinfop = (void *) bbi;
		if (memcmp(bbinfop->bbi_magic, BBINFO_MAGIC,
			    BBINFO_MAGICSIZE) == 0)
		break;
	}
	if (bbi >= (bp + sz)) {
		warn("%s: unable to locate bbinfo structure\n", fname);
		free((void *)ap);
		return NULL;
	}

	max_block_count = sa_be32toh(bbinfop->bbi_block_count);

	if (verbose) {
		printf("%s: entry point %#lx\n", fname, en);
		printf("proto bootblock size %d\n", sz);
		printf("room for %d filesystem blocks at %#lx\n",
			max_block_count,
			bbi - bp + offsetof(struct bbinfo, bbi_block_table));
	}

	*size = sz;
	return (char *)ap;
}

static void
devread(fd, buf, blk, size, msg)
	int	fd;
	void	*buf;
	daddr_t	blk;
	size_t	size;
	char	*msg;
{
	if (lseek(fd, dbtob(blk), SEEK_SET) != dbtob(blk))
		err(1, "%s: devread: lseek", msg);

	if (read(fd, buf, size) != size)
		err(1, "%s: devread: read", msg);
}

static char sblock[SBSIZE];

int
loadblocknums(boot, devfd)
char	*boot;
int	devfd;
{
	int		i, fd;
	struct	stat	statbuf;
	struct	statfs	statfsbuf;
	struct fs	*fs;
	char		*buf;
	daddr_t		blk, *ap;
	struct dinode	*ip;
	int		ndb;
	int		needswap;

	/*
	 * Open 2nd-level boot program and record the block numbers
	 * it occupies on the filesystem represented by `devfd'.
	 */

	/* Make sure the (probably new) boot file is on disk. */
	sync(); sleep(1);

	if ((fd = open(boot, O_RDONLY)) < 0)
		err(1, "open: %s", boot);

	if (fstatfs(fd, &statfsbuf) != 0)
		err(1, "statfs: %s", boot);

	if (strncmp(statfsbuf.f_fstypename, "ffs", MFSNAMELEN) &&
	    strncmp(statfsbuf.f_fstypename, "ufs", MFSNAMELEN)) {
		errx(1, "%s: must be on an FFS filesystem", boot);
	}

	if (fsync(fd) != 0)
		err(1, "fsync: %s", boot);

	if (fstat(fd, &statbuf) != 0)
		err(1, "fstat: %s", boot);

	close(fd);

	/* Read superblock */
	devread(devfd, sblock, SBLOCK, SBSIZE, "superblock");
	fs = (struct fs *)sblock;
	needswap = (fs->fs_magic != FS_MAGIC);
	if (needswap)
		ffs_sb_swap(fs, fs);

	/* Sanity-check super-block. */
	if (fs->fs_magic != FS_MAGIC)
		errx(1, "Bad magic number in superblock");
	if (fs->fs_inopb <= 0)
		err(1, "Bad inopb=%d in superblock", fs->fs_inopb);

	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL)
		errx(1, "No memory for filesystem block");

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));
	devread(devfd, buf, blk, fs->fs_bsize, "inode");
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);
	if (needswap)
		ffs_dinode_swap(ip, ip);

	/*
	 * Have the inode.  Figure out how many blocks we need.
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb > max_block_count)
		errx(1, "%s: Too many blocks", boot);
	bbinfop->bbi_block_count = sa_htobe32(ndb);
	bbinfop->bbi_block_size = sa_htobe32(fs->fs_bsize);
	if (verbose)
		printf("Will load %d blocks of size %d each.\n",
			   ndb, fs->fs_bsize);

	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--) {
		if (needswap)
			*ap = bswap32(*ap);
		blk = fsbtodb(fs, *ap);
		bbinfop->bbi_block_table[i] = sa_htobe32(blk);
		if (verbose)
			printf("%d: %d\n", i, blk);
	}
	if (ndb == 0)
		return 0;

	/*
	 * Just one level of indirections; there isn't much room
	 * for more in the 1st-level bootblocks anyway.
	 */
	if (needswap)
		ip->di_ib[0] = bswap32(ip->di_ib[0]);
	blk = fsbtodb(fs, ip->di_ib[0]);
	devread(devfd, buf, blk, fs->fs_bsize, "indirect block");
	ap = (daddr_t *)buf;
	for (; i < NINDIR(fs) && *ap && ndb; i++, ap++, ndb--) {
		if (needswap)
			*ap = bswap32(*ap);
		blk = fsbtodb(fs, *ap);
		bbinfop->bbi_block_table[i] = sa_htobe32(blk);
		if (verbose)
			printf("%d: %d\n", i, blk);
	}

	return 0;
}
