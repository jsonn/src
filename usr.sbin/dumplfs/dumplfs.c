/*	$NetBSD: dumplfs.c,v 1.13.6.1 2000/06/22 18:00:56 minoura Exp $	*/

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
__COPYRIGHT(
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)dumplfs.c	8.5 (Berkeley) 5/24/95";
#else
__RCSID("$NetBSD: dumplfs.c,v 1.13.6.1 2000/06/22 18:00:56 minoura Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

static void	addseg __P((char *));
static void	dump_cleaner_info __P((struct lfs *, void *));
static void	dump_dinode __P((struct dinode *));
static void	dump_ifile __P((int, struct lfs *, int, int));
static int	dump_ipage_ifile __P((int, IFILE *, int));
static int	dump_ipage_segusage __P((struct lfs *, int, IFILE *, int));
static void	dump_segment __P((int, int, daddr_t, struct lfs *, int));
static int	dump_sum __P((int, struct lfs *, SEGSUM *, int, daddr_t));
static void	dump_super __P((struct lfs *));
static void	usage __P((void));

int		main __P((int, char *[]));

extern u_long	cksum __P((void *, size_t));

typedef struct seglist SEGLIST;
struct seglist {
        SEGLIST *next;
	int num;
};
SEGLIST	*seglist;

int daddr_shift;
char *special;

/* Segment Usage formats */
#define print_suheader \
	(void)printf("segnum\tflags\tnbytes\tninos\tnsums\tlastmod\n")

#define print_suentry(i, sp) \
	(void)printf("%d\t%c%c%c\t%d\t%d\t%d\t%s", i, \
	    (((sp)->su_flags & SEGUSE_ACTIVE) ? 'A' : ' '), \
	    (((sp)->su_flags & SEGUSE_DIRTY) ? 'D' : 'C'), \
	    (((sp)->su_flags & SEGUSE_SUPERBLOCK) ? 'S' : ' '), \
	    (sp)->su_nbytes, (sp)->su_ninos, (sp)->su_nsums, \
	    ctime((time_t *)&(sp)->su_lastmod))

/* Ifile formats */
#define print_iheader \
	(void)printf("inum\tstatus\tversion\tdaddr\t\tfreeptr\n")
#define print_ientry(i, ip) \
	if (ip->if_daddr == LFS_UNUSED_DADDR) \
		(void)printf("%d\tFREE\t%d\t \t\t%d\n", \
		    i, ip->if_version, ip->if_nextfree); \
	else \
		(void)printf("%d\tINUSE\t%d\t%8X    \n", \
		    i, ip->if_version, ip->if_daddr)

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct lfs lfs_sb1, lfs_sb2, *lfs_master;
	daddr_t seg_addr, idaddr, sbdaddr;
	int ch, do_allsb, do_ientries, fd, segnum;

	do_allsb = 0;
	do_ientries = 0;
	idaddr = 0x0;
	sbdaddr = 0x0;
	while ((ch = getopt(argc, argv, "ab:iI:s:")) != -1)
		switch(ch) {
		case 'a':		/* Dump all superblocks */
			do_allsb = 1;
			break;
		case 'b':		/* Use this superblock */
			sbdaddr = strtol(optarg, NULL, 0);
			break;
		case 'i':		/* Dump ifile entries */
			do_ientries = 1;
			break;
		case 'I':		/* Use this ifile inode */
			idaddr = strtol(optarg, NULL, 0);
			break;
		case 's':		/* Dump out these segments */
			addseg(optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	special = argv[0];
	if ((fd = open(special, O_RDONLY, 0)) < 0)
		err(1, "%s", special);

	if (sbdaddr == 0x0) {
		/* Read the first superblock */
		get(fd, LFS_LABELPAD, &(lfs_sb1.lfs_dlfs), sizeof(struct dlfs));
		daddr_shift = lfs_sb1.lfs_bshift - lfs_sb1.lfs_fsbtodb;
	
		/*
	 	* Read the second superblock and figure out which check point is
	 	* most up to date.
	 	*/
		get(fd,
	    	lfs_sb1.lfs_sboffs[1] << daddr_shift, &(lfs_sb2.lfs_dlfs), sizeof(struct dlfs));
	
		lfs_master = &lfs_sb1;
		if (lfs_sb1.lfs_tstamp > lfs_sb2.lfs_tstamp) {
			lfs_master = &lfs_sb2;
			sbdaddr = lfs_sb1.lfs_sboffs[1] << daddr_shift;
		} else
			sbdaddr = btodb(LFS_LABELPAD);
	} else {
		/* Read the first superblock */
		get(fd, dbtob(sbdaddr), &(lfs_sb1.lfs_dlfs), sizeof(struct dlfs));
		daddr_shift = lfs_sb1.lfs_bshift - lfs_sb1.lfs_fsbtodb;
		lfs_master = &lfs_sb1;
	}

	(void)printf("Master Superblock at 0x%x:\n", sbdaddr);
	dump_super(lfs_master);

	dump_ifile(fd, lfs_master, do_ientries, idaddr);

	if (seglist != NULL)
		for (; seglist != NULL; seglist = seglist->next) {
			seg_addr = lfs_master->lfs_sboffs[0] + seglist->num *
			    (lfs_master->lfs_ssize << lfs_master->lfs_fsbtodb);
			dump_segment(fd,
			    seglist->num, seg_addr, lfs_master, do_allsb);
		}
	else
		for (segnum = 0, seg_addr = lfs_master->lfs_sboffs[0];
		    segnum < lfs_master->lfs_nseg; segnum++, seg_addr +=
		    lfs_master->lfs_ssize << lfs_master->lfs_fsbtodb)
			dump_segment(fd,
			    segnum, seg_addr, lfs_master, do_allsb);

	(void)close(fd);
	exit(0);
}

/*
 * We are reading all the blocks of an inode and dumping out the ifile table.
 * This code could be tighter, but this is a first pass at getting the stuff
 * printed out rather than making this code incredibly efficient.
 */
static void
dump_ifile(fd, lfsp, do_ientries, addr)
	int fd;
	struct lfs *lfsp;
	int do_ientries;
	daddr_t addr;
{
	IFILE *ipage;
	struct dinode *dip, *dpage;
	daddr_t *addrp, *dindir, *iaddrp, *indir;
	int block_limit, i, inum, j, nblocks, psize;

	psize = lfsp->lfs_bsize;
	if (!addr)
		addr = lfsp->lfs_idaddr;

	if (!(dpage = malloc(psize)))
		err(1, "malloc");
	get(fd, addr << daddr_shift, dpage, psize);

	for (dip = dpage + INOPB(lfsp) - 1; dip >= dpage; --dip)
		if (dip->di_inumber == LFS_IFILE_INUM)
			break;

	if (dip < dpage)
		errx(1, "unable to locate ifile inode at disk address 0x%x",
		     addr);

	(void)printf("\nIFILE inode\n");
	dump_dinode(dip);

	(void)printf("\nIFILE contents\n");
	nblocks = dip->di_size >> lfsp->lfs_bshift;
	block_limit = MIN(nblocks, NDADDR);

	/* Get the direct block */
	if ((ipage = malloc(psize)) == NULL)
		err(1, "malloc");
	for (inum = 0, addrp = dip->di_db, i = 0; i < block_limit;
	    i++, addrp++) {
		get(fd, *addrp << daddr_shift, ipage, psize);
		if (i < lfsp->lfs_cleansz) {
			dump_cleaner_info(lfsp, ipage);
			print_suheader;
			continue;
		} 

		if (i < (lfsp->lfs_segtabsz + lfsp->lfs_cleansz)) {
			inum = dump_ipage_segusage(lfsp, inum, ipage, 
			    lfsp->lfs_sepb);
			if (!inum) {
				if(!do_ientries)
					goto e0;
				else
					print_iheader;
			}
		} else
			inum = dump_ipage_ifile(inum, ipage, lfsp->lfs_ifpb);

	}

	if (nblocks <= NDADDR)
		goto e0;

	/* Dump out blocks off of single indirect block */
	if (!(indir = malloc(psize)))
		err(1, "malloc");
	get(fd, dip->di_ib[0] << daddr_shift, indir, psize);
	block_limit = MIN(i + lfsp->lfs_nindir, nblocks);
	for (addrp = indir; i < block_limit; i++, addrp++) {
		if (*addrp == LFS_UNUSED_DADDR)
			break;
		get(fd, *addrp << daddr_shift,ipage, psize);
		if (i < lfsp->lfs_cleansz) {
			dump_cleaner_info(lfsp, ipage);
			continue;
		} else 
			i -= lfsp->lfs_cleansz;

		if (i < lfsp->lfs_segtabsz) {
			inum = dump_ipage_segusage(lfsp, inum, ipage, 
			    lfsp->lfs_sepb);
			if (!inum) {
				if(!do_ientries)
					goto e1;
				else
					print_iheader;
			}
		} else
			inum = dump_ipage_ifile(inum, ipage, lfsp->lfs_ifpb);
	}

	if (nblocks <= lfsp->lfs_nindir * lfsp->lfs_ifpb)
		goto e1;

	/* Get the double indirect block */
	if (!(dindir = malloc(psize)))
		err(1, "malloc");
	get(fd, dip->di_ib[1] << daddr_shift, dindir, psize);
	for (iaddrp = dindir, j = 0; j < lfsp->lfs_nindir; j++, iaddrp++) {
		if (*iaddrp == LFS_UNUSED_DADDR)
			break;
		get(fd, *iaddrp << daddr_shift, indir, psize);
		block_limit = MIN(i + lfsp->lfs_nindir, nblocks);
		for (addrp = indir; i < block_limit; i++, addrp++) {
			if (*addrp == LFS_UNUSED_DADDR)
				break;
			get(fd, *addrp << daddr_shift, ipage, psize);
			if (i < lfsp->lfs_cleansz) {
				dump_cleaner_info(lfsp, ipage);
				continue;
			} else 
				i -= lfsp->lfs_cleansz;

			if (i < lfsp->lfs_segtabsz) {
				inum = dump_ipage_segusage(lfsp,
				    inum, ipage, lfsp->lfs_sepb);
				if (!inum) {
					if(!do_ientries)
						goto e2;
					else
						print_iheader;
				}
			} else
				inum = dump_ipage_ifile(inum,
				    ipage, lfsp->lfs_ifpb);
		}
	}
e2:	free(dindir);
e1:	free(indir);
e0:	free(dpage);
	free(ipage);
}

static int
dump_ipage_ifile(i, pp, tot)
	int i;
	IFILE *pp;
	int tot;
{
	IFILE *ip;
	int cnt, max;

	max = i + tot;

	for (ip = pp, cnt = i; cnt < max; cnt++, ip++)
		print_ientry(cnt, ip);
	return (max);
}

static int
dump_ipage_segusage(lfsp, i, pp, tot)
	struct lfs *lfsp;
	int i;
	IFILE *pp;
	int tot;
{
	SEGUSE *sp;
	int cnt, max;

	max = i + tot;
	for (sp = (SEGUSE *)pp, cnt = i;
	     cnt < lfsp->lfs_nseg && cnt < max; cnt++, sp++)
		print_suentry(cnt, sp);
	if (max >= lfsp->lfs_nseg)
		return (0);
	else
		return (max);
}

static void
dump_dinode(dip)
	struct dinode *dip;
{
	int i;
	time_t at, mt, ct;

	at = dip->di_atime;
	mt = dip->di_mtime;
	ct = dip->di_ctime;

	(void)printf("%s%d\t%s%d\t%s%d\t%s%d\t%s%qu\n",
		"mode  ", dip->di_mode,
		"nlink ", dip->di_nlink,
		"uid   ", dip->di_uid,
		"gid   ", dip->di_gid,
		"size  ", (long long)dip->di_size);
	(void)printf("%s%s%s%s%s%s",
		"atime ", ctime(&at),
		"mtime ", ctime(&mt),
		"ctime ", ctime(&ct));
	(void)printf("inum  %d\n", dip->di_inumber);
	(void)printf("Direct Addresses\n");
	for (i = 0; i < NDADDR; i++) {
		(void)printf("\t0x%x", dip->di_db[i]);
		if ((i % 6) == 5)
			(void)printf("\n");
	}
	for (i = 0; i < NIADDR; i++)
		(void)printf("\t0x%x", dip->di_ib[i]);
	(void)printf("\n");
}

static int
dump_sum(fd, lfsp, sp, segnum, addr)
	struct lfs *lfsp;
	SEGSUM *sp;
	int fd, segnum;
	daddr_t addr;
{
	FINFO *fp;
	daddr_t *dp;
	int i, j;
	int ck;
	int numbytes;
	struct dinode *inop;

	if (sp->ss_magic != SS_MAGIC || 
	    sp->ss_sumsum != (ck = cksum(&sp->ss_datasum, 
	    LFS_SUMMARY_SIZE - sizeof(sp->ss_sumsum)))) {
		/* Don't print "corrupt" if we're just too close to the edge */
		if (datosn(lfsp, addr + fsbtodb(lfsp, lfsp->lfs_bsize)) ==
		    datosn(lfsp, addr))
			(void)printf("dumplfs: %s %d address 0x%x\n",
		                     "corrupt summary block; segment", segnum,
				     addr);
		return(0);
	}

	(void)printf("Segment Summary Info at 0x%x\n", addr);
	(void)printf("    %s0x%x\t%s%d\t%s%d\t%s%c%c\n    %s0x%x\t%s0x%x",
		"next     ", sp->ss_next,
		"nfinfo   ", sp->ss_nfinfo,
		"ninos    ", sp->ss_ninos,
		"flags    ", (sp->ss_flags & SS_DIROP) ? 'D' : '-',
			     (sp->ss_flags & SS_CONT)  ? 'C' : '-',
		"sumsum   ", sp->ss_sumsum,
		"datasum  ", sp->ss_datasum );
	(void)printf("\tcreate   %s", ctime((time_t *)&sp->ss_create));

	/* Dump out inode disk addresses */
	dp = (daddr_t *)sp;
	dp += LFS_SUMMARY_SIZE / sizeof(daddr_t);
	inop = malloc(1 << lfsp->lfs_bshift);
	printf("    Inode addresses:");
	numbytes = 0;
	for (dp--, i = 0; i < sp->ss_ninos; dp--) {
		numbytes += lfsp->lfs_bsize;	/* add bytes for inode block */
		printf("\t0x%x {", *dp);
		get(fd, *dp << (lfsp->lfs_bshift - lfsp->lfs_fsbtodb), inop, 
		    (1 << lfsp->lfs_bshift));
		for (j = 0; i < sp->ss_ninos && j < INOPB(lfsp); j++, i++) {
			if (j > 0) 
				(void)printf(", ");
			(void)printf("%d", inop[j].di_inumber);
		}
		(void)printf("}");
		if (((i/INOPB(lfsp)) % 4) == 3)
			(void)printf("\n");
	}
	free(inop);

	printf("\n");
	for (fp = (FINFO *)(sp + 1), i = 0; i < sp->ss_nfinfo; i++) {
		(void)printf("    FINFO for inode: %d version %d nblocks %d lastlength %d\n",
		    fp->fi_ino, fp->fi_version, fp->fi_nblocks,
		    fp->fi_lastlength);
		dp = &(fp->fi_blocks[0]);
		for (j = 0; j < fp->fi_nblocks; j++, dp++) {
			(void)printf("\t%d", *dp);
			if ((j % 8) == 7)
				(void)printf("\n");
			if (j == fp->fi_nblocks - 1)
				numbytes += fp->fi_lastlength;
			else
				numbytes += lfsp->lfs_bsize;
		}
		if ((j % 8) != 0)
			(void)printf("\n");
		fp = (FINFO *)dp;
	}
	return (numbytes);
}

static void
dump_segment(fd, segnum, addr, lfsp, dump_sb)
	int fd, segnum;
	daddr_t addr;
	struct lfs *lfsp;
	int dump_sb;
{
	struct lfs lfs_sb, *sbp;
	SEGSUM *sump;
	char sumblock[LFS_SUMMARY_SIZE];
	int did_one, nbytes, sb;
	off_t sum_offset;
	daddr_t new_addr;

	(void)printf("\nSEGMENT %d (Disk Address 0x%x)\n",
	    /* addr >> (lfsp->lfs_segshift - daddr_shift), */
		datosn(lfsp, addr),
		addr);
	sum_offset = (addr << (lfsp->lfs_bshift - lfsp->lfs_fsbtodb));

	sb = 0;
	did_one = 0;
	do {
		get(fd, sum_offset, sumblock, LFS_SUMMARY_SIZE);
		sump = (SEGSUM *)sumblock;
		if (sump->ss_sumsum != cksum (&sump->ss_datasum, 
			LFS_SUMMARY_SIZE - sizeof(sump->ss_sumsum))) {
			sbp = (struct lfs *)sump;
			if ((sb = (sbp->lfs_magic == LFS_MAGIC))) {
				if (dump_sb)  {
					get(fd, sum_offset, &(lfs_sb.lfs_dlfs),
					    sizeof(struct dlfs));
					(void)printf("\nSuperblock at 0x%x:\n",
						   (unsigned)btodb(sum_offset));
					dump_super(&lfs_sb);
					(void)printf("%s","\n");
				}
				sum_offset += LFS_SBPAD;
			} else if (did_one)
				break;
			else {
				printf("Segment at 0x%x corrupt\n", addr);
				break;
			}
		} else {
			nbytes = dump_sum(fd, lfsp, sump, segnum, sum_offset >>
			     (lfsp->lfs_bshift - lfsp->lfs_fsbtodb));
			if (nbytes)
				sum_offset += LFS_SUMMARY_SIZE + nbytes;
			else
				sum_offset = 0;
			did_one = 1;
		}
		/* If the segment ends right on a boundary, it still ends */
		new_addr = sum_offset >> (lfsp->lfs_bshift - lfsp->lfs_fsbtodb);
		if (datosn(lfsp, new_addr) != datosn(lfsp, addr))
			break;
	} while (sum_offset);

	return;
}

static void
dump_super(lfsp)
	struct lfs *lfsp;
{
	int i;

	(void)printf("%s0x%x\t%s0x%x\t%s%d\t%s%d\n",
		"magic    ", lfsp->lfs_magic,
		"version  ", lfsp->lfs_version,
		"size     ", lfsp->lfs_size,
		"ssize    ", lfsp->lfs_ssize);
	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"dsize    ", lfsp->lfs_dsize,
		"bsize    ", lfsp->lfs_bsize,
		"fsize    ", lfsp->lfs_fsize,
		"frag     ", lfsp->lfs_frag);

	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"minfree  ", lfsp->lfs_minfree,
		"inopb    ", lfsp->lfs_inopb,
		"ifpb     ", lfsp->lfs_ifpb,
		"nindir   ", lfsp->lfs_nindir);

	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"nseg     ", lfsp->lfs_nseg,
		"nspf     ", lfsp->lfs_nspf,
		"cleansz  ", lfsp->lfs_cleansz,
		"segtabsz ", lfsp->lfs_segtabsz);

	(void)printf("%s0x%x\t%s%d\t%s0x%qX\t%s%d\n",
		"segmask  ", lfsp->lfs_segmask,
		"segshift ", lfsp->lfs_segshift,
		"bmask    ", (long long)lfsp->lfs_bmask,
		"bshift   ", lfsp->lfs_bshift);

	(void)printf("%s0x%qX\t\t%s%d\t%s0x%qX\t%s%u\n",
		"ffmask   ", (long long)lfsp->lfs_ffmask,
		"ffshift  ", lfsp->lfs_ffshift,
		"fbmask   ", (long long)lfsp->lfs_fbmask,
		"fbshift  ", lfsp->lfs_fbshift);

	(void)printf("%s%d\t%s%d\t%s0x%x\t%s0x%qx\n",
		"sushift  ", lfsp->lfs_sushift,
		"fsbtodb  ", lfsp->lfs_fsbtodb,
		"cksum    ", lfsp->lfs_cksum,
		"maxfilesize  ", (long long)lfsp->lfs_maxfilesize);

	(void)printf("Superblock disk addresses:\t");
	for (i = 0; i < LFS_MAXNUMSB; i++) {
		(void)printf(" 0x%x", lfsp->lfs_sboffs[i]);
		if ( i == (LFS_MAXNUMSB >> 1))
			(void)printf("\n\t\t\t\t");
	}
	(void)printf("\n");

	(void)printf("Checkpoint Info\n");
	(void)printf("%s%d\t%s0x%x\t%s%d\n",
		"free     ", lfsp->lfs_free,
		"idaddr   ", lfsp->lfs_idaddr,
		"ifile    ", lfsp->lfs_ifile);
	(void)printf("%s%d\t%s%d\t%s%d\n",
		"bfree    ", lfsp->lfs_bfree,
		"avail    ", lfsp->lfs_avail,
		"uinodes  ", lfsp->lfs_uinodes);
	(void)printf("%s%d\t%s0x%x\t%s0x%x\n%s0x%x\t%s0x%x\t",
		"nfiles   ", lfsp->lfs_nfiles,
		"lastseg  ", lfsp->lfs_lastseg,
		"nextseg  ", lfsp->lfs_nextseg,
		"curseg   ", lfsp->lfs_curseg,
		"offset   ", lfsp->lfs_offset);
	(void)printf("tstamp   %s", ctime((time_t *)&lfsp->lfs_tstamp));
#if 0  /* This is no longer stored on disk! --ks */
	(void)printf("\nIn-Memory Information\n");
	(void)printf("%s%d\t%s0x%x\t%s%d%s%d\t%s%d\n",
		"seglock  ", lfsp->lfs_seglock,
		"iocount  ", lfsp->lfs_iocount,
		"writer   ", lfsp->lfs_writer,
		"dirops   ", lfsp->lfs_dirops,
		"doifile  ", lfsp->lfs_doifile);
	(void)printf("%s%d\t%s%d\t%s0x%x\t%s%d\n",
		"nactive  ", lfsp->lfs_nactive,
		"fmod     ", lfsp->lfs_fmod,
		"clean    ", lfsp->lfs_clean,
		"ronly    ", lfsp->lfs_ronly);
#endif
}

static void
addseg(arg)
	char *arg;
{
	SEGLIST *p;

	if ((p = malloc(sizeof(SEGLIST))) == NULL)
		err(1, "malloc");
	p->next = seglist;
	p->num = atoi(arg);
	seglist = p;
}

static void
dump_cleaner_info(lfsp, ipage)
	struct lfs *lfsp;
	void *ipage;
{
	CLEANERINFO *cip;

	cip = (CLEANERINFO *)ipage;
	(void)printf("segments clean\t%d\tsegments dirty\t%d\n\n",
	    cip->clean, cip->dirty);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: dumplfs [-ai] [-s segnum] file\n");
	exit(1);
}
