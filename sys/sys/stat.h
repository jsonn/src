/*	$NetBSD: stat.h,v 1.26.2.2 1997/11/04 23:34:57 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)stat.h	8.9 (Berkeley) 8/17/94
 */

#ifndef _SYS_STAT_H_
#define	_SYS_STAT_H_

#include <sys/time.h>

#ifdef _KERNEL
struct stat43 {				/* BSD-4.3 stat struct */
	u_int16_t st_dev;		/* inode's device */
	ino_t	  st_ino;		/* inode's number */
	u_int16_t st_mode;		/* inode protection mode */
	u_int16_t st_nlink;		/* number of hard links */
	u_int16_t st_uid;		/* user ID of the file's owner */
	u_int16_t st_gid;		/* group ID of the file's group */
	u_int16_t st_rdev;		/* device type */
	int32_t	  st_size;		/* file size, in bytes */
	struct	  timespec st_atimespec;/* time of last access */
	struct	  timespec st_mtimespec;/* time of last data modification */
	struct	  timespec st_ctimespec;/* time of last file status change */
	int32_t	  st_blksize;		/* optimal blocksize for I/O */
	int32_t	  st_blocks;		/* blocks allocated for file */
	u_int32_t st_flags;		/* user defined flags for file */
	u_int32_t st_gen;		/* file generation number */
};
#endif /* !_KERNEL */

struct stat12 {				/* NetBSD-1.2 stat struct */
	dev_t	  st_dev;		/* inode's device */
	ino_t	  st_ino;		/* inode's number */
	u_int16_t st_mode;		/* inode protection mode */
	u_int16_t st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	dev_t	  st_rdev;		/* device type */
	struct	  timespec st_atimespec;/* time of last access */
	struct	  timespec st_mtimespec;/* time of last data modification */
	struct	  timespec st_ctimespec;/* time of last file status change */
	off_t	  st_size;		/* file size, in bytes */
	int64_t	  st_blocks;		/* blocks allocated for file */
	u_int32_t st_blksize;		/* optimal blocksize for I/O */
	u_int32_t st_flags;		/* user defined flags for file */
	u_int32_t st_gen;		/* file generation number */
	int32_t	  st_lspare;
	int64_t	  st_qspare[2];
};

struct stat {
	dev_t	  st_dev;		/* inode's device */
	ino_t	  st_ino;		/* inode's number */
	mode_t	  st_mode;		/* inode protection mode */
	nlink_t	  st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	dev_t	  st_rdev;		/* device type */
#ifndef _POSIX_SOURCE
	struct	  timespec st_atimespec;/* time of last access */
	struct	  timespec st_mtimespec;/* time of last data modification */
	struct	  timespec st_ctimespec;/* time of last file status change */
#else
	time_t	  st_atime;		/* time of last access */
	long	  st_atimensec;		/* nsec of last access */
	time_t	  st_mtime;		/* time of last data modification */
	long	  st_mtimensec;		/* nsec of last data modification */
	time_t	  st_ctime;		/* time of last file status change */
	long	  st_ctimensec;		/* nsec of last file status change */
#endif
	off_t	  st_size;		/* file size, in bytes */
	int64_t	  st_blocks;		/* blocks allocated for file */
	u_int32_t st_blksize;		/* optimal blocksize for I/O */
	u_int32_t st_flags;		/* user defined flags for file */
	u_int32_t st_gen;		/* file generation number */
	int64_t	  st_qspare[2];
};

#ifndef _POSIX_SOURCE
#define	st_atime	st_atimespec.tv_sec
#define	st_atimensec	st_atimespec.tv_nsec
#define	st_mtime	st_mtimespec.tv_sec
#define	st_mtimensec	st_mtimespec.tv_nsec
#define	st_ctime	st_ctimespec.tv_sec
#define	st_ctimensec	st_ctimespec.tv_nsec
#endif

#define	S_ISUID	0004000			/* set user id on execution */
#define	S_ISGID	0002000			/* set group id on execution */
#ifndef _POSIX_SOURCE
#define	S_ISTXT	0001000			/* sticky bit */
#endif

#define	S_IRWXU	0000700			/* RWX mask for owner */
#define	S_IRUSR	0000400			/* R for owner */
#define	S_IWUSR	0000200			/* W for owner */
#define	S_IXUSR	0000100			/* X for owner */

#ifndef _POSIX_SOURCE
#define	S_IREAD		S_IRUSR
#define	S_IWRITE	S_IWUSR
#define	S_IEXEC		S_IXUSR
#endif

#define	S_IRWXG	0000070			/* RWX mask for group */
#define	S_IRGRP	0000040			/* R for group */
#define	S_IWGRP	0000020			/* W for group */
#define	S_IXGRP	0000010			/* X for group */

#define	S_IRWXO	0000007			/* RWX mask for other */
#define	S_IROTH	0000004			/* R for other */
#define	S_IWOTH	0000002			/* W for other */
#define	S_IXOTH	0000001			/* X for other */

#ifndef _POSIX_SOURCE
#define	S_IFMT	 0170000		/* type of file mask */
#define	S_IFIFO	 0010000		/* named pipe (fifo) */
#define	S_IFCHR	 0020000		/* character special */
#define	S_IFDIR	 0040000		/* directory */
#define	S_IFBLK	 0060000		/* block special */
#define	S_IFREG	 0100000		/* regular */
#define	S_IFLNK	 0120000		/* symbolic link */
#define	S_IFSOCK 0140000		/* socket */
#define	S_IFWHT  0160000		/* whiteout */
#define	S_ISVTX	 0001000		/* save swapped text even after use */
#endif

#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#ifndef _POSIX_SOURCE
#define	S_ISLNK(m)	((m & 0170000) == 0120000)	/* symbolic link */
#define	S_ISSOCK(m)	((m & 0170000) == 0140000)	/* socket */
#define	S_ISWHT(m)	((m & 0170000) == 0160000)	/* whiteout */
#endif

#ifndef _POSIX_SOURCE
#define	ACCESSPERMS	(S_IRWXU|S_IRWXG|S_IRWXO)	/* 0777 */
							/* 7777 */
#define	ALLPERMS	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
							/* 0666 */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

#define S_BLKSIZE	512		/* block size used in the stat struct */

/*
 * Definitions of flags stored in file flags word.
 *
 * Super-user and owner changeable flags.
 */
#define	UF_SETTABLE	0x0000ffff	/* mask of owner changeable flags */
#define	UF_NODUMP	0x00000001	/* do not dump file */
#define	UF_IMMUTABLE	0x00000002	/* file may not be changed */
#define	UF_APPEND	0x00000004	/* writes to file may only append */
#define UF_OPAQUE	0x00000008	/* directory is opaque wrt. union */
/*
 * Super-user changeable flags.
 */
#define	SF_SETTABLE	0xffff0000	/* mask of superuser changeable flags */
#define	SF_ARCHIVED	0x00010000	/* file is archived */
#define	SF_IMMUTABLE	0x00020000	/* file may not be changed */
#define	SF_APPEND	0x00040000	/* writes to file may only append */

#ifdef _KERNEL
/*
 * Shorthand abbreviations of above.
 */
#define	OPAQUE		(UF_OPAQUE)
#define	APPEND		(UF_APPEND | SF_APPEND)
#define	IMMUTABLE	(UF_IMMUTABLE | SF_IMMUTABLE)
#endif
#endif

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	chmod __P((const char *, mode_t));
int	mkdir __P((const char *, mode_t));
int	mkfifo __P((const char *, mode_t));
#ifdef __LIBC12_SOURCE__
int	stat __P((const char *, struct stat12 *));
int	fstat __P((int, struct stat12 *));
int	__stat13 __P((const char *, struct stat *));
int	__fstat13 __P((int, struct stat *));
#else
int	stat __P((const char *, struct stat *))	__RENAME(__stat13);
int	fstat __P((int, struct stat *))		__RENAME(__fstat13);
#endif
mode_t	umask __P((mode_t));
#ifndef _POSIX_SOURCE
int	chflags __P((const char *, u_long));
int	fchflags __P((int, u_long));
int	fchmod __P((int, mode_t));
int	lchmod __P((const char *, mode_t));
#ifdef __LIBC12_SOURCE__
int	lstat __P((const char *, struct stat12 *));
int	__lstat13 __P((const char *, struct stat *));
#else
int	lstat __P((const char *, struct stat *))	__RENAME(__lstat13);
#endif
#endif

__END_DECLS

#endif
#endif /* !_SYS_STAT_H_ */
