/*	$NetBSD: installboot.c,v 1.1.6.1 1999/12/27 18:33:11 wrstuden Exp $ */

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
#include <sys/sysctl.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <err.h>
#ifdef BOOT_AOUT
#include <a.out.h>
#endif
#include <sys/exec_elf.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int	verbose, nowrite;
char	*boot, *proto, *dev;

#define BOOTSECTOR_OFFSET 512

#ifndef DEFAULT_ENTRY
#define DEFAULT_ENTRY 0xa0700000
#endif

struct nlist nl[] = {
#define X_BLOCKTABLE	0
	{"_block_table"},
#define X_BLOCKCOUNT	1
	{"_block_count"},
#define X_BLOCKSIZE	2
	{"_block_size"},
#define X_ENTRY_POINT	3
	{"_entry_point"},
	{NULL}
};

daddr_t	*block_table;		/* block number array in prototype image */
int32_t	*block_count_p;		/* size of this array */
int32_t	*block_size_p;		/* filesystem block size */
int32_t	*entry_point_p;		/* entry point */
int32_t	max_block_count;

char		*loadprotoblocks __P((char *, long *));
int		loadblocknums __P((char *, int));
static void	devread __P((int, void *, daddr_t, size_t, char *));
static void	usage __P((void));
int 		main __P((int, char *[]));


static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] <boot> <proto> <device>\n");
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
	long	protosize;
	size_t	size;
	int boot00[512/4];

	while ((c = getopt(argc, argv, "vn")) != EOF) {
		switch (c) {

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

	if (lseek(devfd, BOOTSECTOR_OFFSET, SEEK_SET) != BOOTSECTOR_OFFSET)
		err(1, "lseek bootstrap");

	/* Sync filesystems (to clean in-memory superblock?) */
	sync(); sync(); sync();

	if (write(devfd, protostore, protosize) != protosize)
		err(1, "write bootstrap");

	/* Write boot00 */
	if (lseek(devfd, 0, SEEK_SET) != 0)
		err(1, "lseek 0a");
	if (read(devfd, boot00, sizeof(boot00)) != sizeof(boot00))
		err(1, "read boot00");

	bzero(boot00, 64);
	boot00[0] = 0x1000007f;	/* b .+0x200 */
	boot00[2] = 0x19900106;
	if (lseek(devfd, 0, SEEK_SET) != 0)
		err(1, "lseek 0b");
	if (write(devfd, boot00, sizeof(boot00)) != sizeof(boot00))
		err(1, "write boot00");

	(void)close(devfd);
	return 0;
}

char *
loadprotoblocks(fname, size)
	char *fname;
	long *size;
{
	int	fd, sz;
	char	*bp;
	struct	stat statbuf;
#ifdef BOOT_AOUT
	struct	exec *hp;
#endif
	long	off;
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
#ifdef BOOT_AOUT
	if (nl[X_BLOCKTABLE].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKTABLE].n_un.n_name);
		return NULL;
	}
	if (nl[X_BLOCKCOUNT].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKCOUNT].n_un.n_name);
		return NULL;
	}
	if (nl[X_BLOCKSIZE].n_type != N_DATA + N_EXT) {
		warnx("nlist: %s: wrong type", nl[X_BLOCKSIZE].n_un.n_name);
		return NULL;
	}
#endif

	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (fstat(fd, &statbuf) != 0) {
		warn("fstat: %s", fname);
		close(fd);
		return NULL;
	}
	if ((bp = calloc(roundup(statbuf.st_size, DEV_BSIZE), 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		close(fd);
		return NULL;
	}
	if (read(fd, bp, statbuf.st_size) != statbuf.st_size) {
		warn("read: %s", fname);
		free(bp);
		close(fd);
		return NULL;
	}
	close(fd);

#ifdef BOOT_AOUT
	hp = (struct exec *)bp;
#endif
	eh = (Elf32_Ehdr *)bp;
	ph = (Elf32_Phdr *)(bp + eh->e_phoff);
	sz = 1024*7;

	/* Find first executable psect */
	while ((ph->p_flags & PF_X) == 0) {
		ph++;		/* XXX check overrun (eh->e_phnum) */
		eh->e_phnum--;
		if (eh->e_phnum == 0) {
			warn("%s: no executable psect", fname);
			return NULL;
		}
	}

	/* Calculate the symbols' location within the proto file */
	off = ph->p_offset - eh->e_entry;
	block_table = (daddr_t *)(bp + nl[X_BLOCKTABLE].n_value + off);
	block_count_p = (int32_t *)(bp + nl[X_BLOCKCOUNT].n_value + off);
	block_size_p = (int32_t *)(bp + nl[X_BLOCKSIZE].n_value + off);
	entry_point_p = (int32_t *)(bp + nl[X_ENTRY_POINT].n_value + off);

	if ((int)block_table & 3) {
		warn("%s: invalid address: block_table = %x",
		     fname, block_table);
		free(bp);
		close(fd);
		return NULL;
	}
	if ((int)block_count_p & 3) {
		warn("%s: invalid address: block_count_p = %x",
		     fname, block_count_p);
		free(bp);
		close(fd);
		return NULL;
	}
	if ((int)block_size_p & 3) {
		warn("%s: invalid address: block_size_p = %x",
		     fname, block_size_p);
		free(bp);
		close(fd);
		return NULL;
	}
	if ((int)entry_point_p & 3) {
		warn("%s: invalid address: entry_point_p = %x",
		     fname, entry_point_p);
		free(bp);
		close(fd);
		return NULL;
	}
	max_block_count = *block_count_p;

	if (verbose) {
		printf("proto bootblock size: %ld\n", sz);
	}

	/*
	 * We convert the a.out header in-vitro into something that
	 * Sun PROMs understand.
	 * Old-style (sun4) ROMs do not expect a header at all, so
	 * we turn the first two words into code that gets us past
	 * the 32-byte header where the actual code begins. In assembly
	 * speak:
	 *	.word	MAGIC		! a NOP
	 *	ba,a	start		!
	 *	.skip	24		! pad
	 * start:
	 */

	*size = sz;
	return bp + ph->p_offset + 0x200;	/* XXX 0x200 */
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

	/*
	 * Open 2nd-level boot program and record the block numbers
	 * it occupies on the filesystem represented by `devfd'.
	 */
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
	devread(devfd, sblock, btodb(SBOFF), SBSIZE, "superblock");
	fs = (struct fs *)sblock;

	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL)
		errx(1, "No memory for filesystem block");

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));
	devread(devfd, buf, blk, fs->fs_bsize, "inode");
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Register filesystem block size.
	 */
	*block_size_p = fs->fs_bsize;

	/*
	 * Get the block numbers; we don't handle fragments
	 */
	ndb = howmany(ip->di_size, fs->fs_bsize);
	if (ndb > max_block_count)
		errx(1, "%s: Too many blocks", boot);

	/*
	 * Register block count.
	 */
	*block_count_p = ndb;

	/*
	 * Register entry point.
	 */
	*entry_point_p = DEFAULT_ENTRY;
	if (verbose)
		printf("entry point: 0x%08x\n", *entry_point_p);

	if (verbose)
		printf("%s: block numbers: ", boot);
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		block_table[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb == 0)
		return 0;

	/*
	 * Just one level of indirections; there isn't much room
	 * for more in the 1st-level bootblocks anyway.
	 */
	if (verbose)
		printf("%s: block numbers (indirect): ", boot);
	blk = ip->di_ib[0];
	devread(devfd, buf, blk, fs->fs_bsize, "indirect block");
	ap = (daddr_t *)buf;
	for (; i < NINDIR(fs) && *ap && ndb; i++, ap++, ndb--) {
		blk = fsbtodb(fs, *ap);
		block_table[i] = blk;
		if (verbose)
			printf("%d ", blk);
	}
	if (verbose)
		printf("\n");

	if (ndb)
		errx(1, "%s: Too many blocks", boot);
	return 0;
}
