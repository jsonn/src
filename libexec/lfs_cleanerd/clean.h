/*	$NetBSD: clean.h,v 1.10.2.3 2001/07/02 17:48:15 perseant Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *
 *	@(#)clean.h	8.2 (Berkeley) 5/4/95
 */

/*
 * The LFS user-level library will be used when writing cleaners and
 * checkers for LFS file systems.  It will have facilities for finding
 * and parsing LFS segments.
 */

#define DUMP_SUM_HEADER		0x0001
#define DUMP_INODE_ADDRS	0x0002
#define DUMP_FINFOS		0x0004
#define	DUMP_ALL		0xFFFF

#define IFILE_NAME "ifile"

/*
 * Cleaner parameters
 *	BUSY_LIM: lower bound of the number of segments currently available
 *		as a percentage of the total number of free segments possibly
 *		available.
 *	IDLE_LIM: Same as BUSY_LIM but used when the system is idle.
 *	MIN_SEGS: Minimum number of segments you should always have.
 *		I have no idea what this should be, but it should probably
 *		be a function of lfsp.
 *	NUM_TO_CLEAN: Number of segments to clean at once.  Again, this
 *		should probably be based on the file system size and how
 *		full or empty the segments being cleaned are.
 */

#define	BUSY_LIM	0.50
#define	IDLE_LIM	0.90

#define	MIN_SEGS(lfsp)		(3)
#define	NUM_TO_CLEAN(fsp)	(1)

#define MAXLOADS	3
#define	ONE_MIN		0
#define	FIVE_MIN	1
#define	FIFTEEN_MIN	2

#define TIME_THRESHOLD	5	/* Time to tell looping from running */
#define LOOP_THRESHOLD	5	/* Number of looping respawns before exit */

#include <sys/time.h>

typedef struct fs_info {
	struct	statfs	*fi_statfsp;	/* fsstat info from getfsstat */
	struct	lfs	fi_lfs;		/* superblock */
	CLEANERINFO	*fi_cip;	/* Cleaner info from ifile */
	SEGUSE	*fi_segusep;		/* segment usage table (from ifile) */
	IFILE	*fi_ifilep;		/* ifile table (from ifile) */
	u_long	fi_ifile_count;		/* # entries in the ifile table */
	off_t	fi_ifile_length;	/* length of the ifile */
	time_t  fi_fs_tstamp;           /* last fs activity, per ifile */
} FS_INFO;

/* 
 * XXX: size (in bytes) of a segment
 *	should lfs_bsize be fsbtodb(fs,1), blksize(fs), or lfs_dsize? 
 */
#define seg_size(fs) fsbtob((fs), segtod((fs), 1))

#define CLEANSIZE(fsp)	(fsp->fi_lfs.lfs_cleansz << fsp->fi_lfs.lfs_bshift)
#define SEGTABSIZE(fsp)	(fsp->fi_lfs.lfs_segtabsz << fsp->fi_lfs.lfs_bshift)

#define IFILE_ENTRY(fs, ife, i) 					\
	((fs)->lfs_version == 1 ?					\
	(IFILE *)((IFILE_V1 *)((caddr_t)(ife) + ((i) / (fs)->lfs_ifpb <<\
		(fs)->lfs_bshift)) + (i) % (fs)->lfs_ifpb) :		\
	((IFILE *)((caddr_t)(ife) + ((i) / (fs)->lfs_ifpb <<		\
		(fs)->lfs_bshift)) + (i) % (fs)->lfs_ifpb))

#define SEGUSE_ENTRY(fs, su, i) 					\
	((fs)->lfs_version == 1 ?					\
	(SEGUSE *)((SEGUSE_V1 *)((caddr_t)(su) + (fs)->lfs_bsize *	\
				((i) / (fs)->lfs_sepb)) +		\
				(i) % (fs)->lfs_sepb) :			\
	((SEGUSE *)((caddr_t)(su) + (fs)->lfs_bsize *			\
				((i) / (fs)->lfs_sepb)) +		\
				(i) % (fs)->lfs_sepb))

/*
 * USEFUL DEBUGGING FUNCTIONS:
 */
#define PRINT_FINFO(fp, ip) if(debug > 1) { \
	syslog(LOG_DEBUG,"    %s %s%d version %d nblocks %d\n", \
	    (ip)->if_version > (fp)->fi_version ? "TOSSING" : "KEEPING", \
	    "FINFO for inode: ", (fp)->fi_ino, \
	    (fp)->fi_version, (fp)->fi_nblocks); \
}

#define PRINT_INODE(b, bip) if(debug > 1) { \
	syslog(LOG_DEBUG,"\t%s inode: %d daddr: 0x%lx create: %s\n", \
	    b ? "KEEPING" : "TOSSING", (bip)->bi_inode, (long)(bip)->bi_daddr, \
	    ctime((time_t *)&(bip)->bi_segcreate)); \
}

#define PRINT_BINFO(bip) if(debug > 1 ) { \
	syslog(LOG_DEBUG,"\tinode: %d lbn: %d daddr: 0x%lx create: %s\n", \
	    (bip)->bi_inode, (bip)->bi_lbn, (unsigned long)(bip)->bi_daddr, \
	    ctime((time_t *)&(bip)->bi_segcreate)); \
}

#define PRINT_SEGUSE(sup, n) if(debug > 1) { \
	syslog(LOG_DEBUG,"Segment %d nbytes=%lu\tflags=%c%c%c ninos=%d nsums=%d lastmod: %s", \
			n, (unsigned long)(sup)->su_nbytes, \
			(sup)->su_flags & SEGUSE_DIRTY ? 'D' : 'C', \
			(sup)->su_flags & SEGUSE_ACTIVE ? 'A' : ' ', \
			(sup)->su_flags & SEGUSE_SUPERBLOCK ? 'S' : ' ', \
			(sup)->su_ninos, (sup)->su_nsums, \
			ctime((time_t *)&(sup)->su_lastmod)); \
}

__BEGIN_DECLS
int	 dump_summary(struct lfs *, SEGSUM *, u_long, daddr_t **, daddr_t);
int	 fs_getmntinfo(struct statfs **, char *, const char *);
void	 get(int, off_t, void *, size_t);
FS_INFO	*get_fs_info(struct statfs *, int);
int 	 lfs_segmapv(FS_INFO *, int, caddr_t, BLOCK_INFO **, int *);
int	 mmap_segment(FS_INFO *, int, caddr_t *, int);
void	 munmap_segment(FS_INFO *, caddr_t, int);
void	 reread_fs_info(FS_INFO *, int);
void	 toss __P((void *, int *, size_t,
	      int (*)(const void *, const void *, const void *), void *));

void	 dump_super(struct lfs *);
void	 dump_cleaner_info(void *);
void	 print_SEGSUM(struct lfs *, SEGSUM *, daddr_t);
void	 print_CLEANERINFO(CLEANERINFO *);
__END_DECLS
