/* $NetBSD: fsck_vars.h,v 1.3.2.1 2000/06/22 16:05:25 minoura Exp $	 */

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
 *
 *	@(#)fsck.h	8.1 (Berkeley) 6/5/93
 */

extern struct bufarea bufhead;	/* head of list of other blks in filesys */
extern struct bufarea sblk;	/* file system superblock */
extern struct bufarea iblk;	/* ifile on-disk inode block */
extern struct bufarea *pdirbp;	/* current directory contents */
extern struct bufarea *pbp;	/* current inode block */
extern struct bufarea *getddblk(daddr_t, long);
extern struct bufarea *getdatablk(daddr_t, long);
extern int      iinooff;	/* ifile inode offset in block of inodes */

extern struct dups *duplist;	/* head of dup list */
extern struct dups *muldup;	/* end of unique duplicate dup block numbers */

extern struct zlncnt *zlnhead;	/* head of zero link count list */

extern daddr_t	idaddr;         /* inode block containing ifile inode */
extern long     numdirs, listmax, inplast;

extern long     dev_bsize;	/* computed value of DEV_BSIZE */
extern long     secsize;	/* actual disk sector size */
extern char     nflag;		/* assume a no response */
extern char     yflag;		/* assume a yes response */
extern int      bflag;		/* location of alternate super block */
extern int      debug;		/* output debugging info */
#ifdef DEBUG_IFILE
extern int      debug_ifile;	/* cat ifile and exit */
#endif
extern int      exitonfail;
extern int      cvtlevel;	/* convert to newer file system format */
extern int      doinglevel1;	/* converting to new cylinder group format */
extern int      doinglevel2;	/* converting to new inode format */
extern int      newinofmt;	/* filesystem has new inode format */
extern int      preen;		/* just fix normal inconsistencies */
extern char     havesb;		/* superblock has been read */
extern char     skipclean;	/* skip clean file systems if preening */
extern int      fsmodified;	/* 1 => write done to file system */
extern int      fsreadfd;	/* file descriptor for reading file system */
extern int      fswritefd;	/* file descriptor for writing file system */
extern int      rerun;		/* rerun fsck.  Only used in non-preen mode */

extern daddr_t  maxfsblock;	/* number of blocks in the file system */
#ifndef VERBOSE_BLOCKMAP
extern char    *blockmap;	/* ptr to primary blk allocation map */
#else
extern ino_t   *blockmap;	/* ptr to primary blk allocation map */
#endif
extern ino_t    maxino;		/* number of inodes in file system */
extern ino_t    lastino;	/* last inode in use */
extern char    *statemap;	/* ptr to inode state table */
extern char    *typemap;	/* ptr to inode type table */
extern int16_t *lncntp;		/* ptr to link count table */

extern ino_t    lfdir;		/* lost & found directory inode number */
extern char    *lfname;		/* lost & found directory name */
extern int      lfmode;		/* lost & found directory creation mode */

extern daddr_t  n_blks;		/* number of blocks in use */
extern daddr_t  n_files;	/* number of files in use */

extern struct dinode zino;
