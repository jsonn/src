/*	$NetBSD: types.h,v 1.39.8.1 1999/12/21 23:20:05 wrstuden Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993, 1994
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
 *	@(#)types.h	8.4 (Berkeley) 1/21/94
 */

#ifndef _SYS_TYPES_H_
#define	_SYS_TYPES_H_

/* Machine type dependent parameters. */
#include <machine/types.h>

#include <machine/ansi.h>
#include <machine/endian.h>

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef unsigned char	unchar;		/* Sys V compatibility */
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
typedef unsigned long	ulong;		/* Sys V compatibility */

typedef	u_long		cpuid_t;
#endif

typedef	u_int64_t	u_quad_t;	/* quads */
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

typedef	quad_t		longlong_t;	/* ANSI long long type */
typedef	u_quad_t	u_longlong_t;	/* ANSI unsigned long long type */

typedef	int64_t		blkcnt_t;	/* fs block count */
typedef	u_int32_t	blksize_t;	/* fs optimal block size */
typedef	char *		caddr_t;	/* core address */
typedef	int32_t		daddr_t;	/* disk address */
typedef	u_int32_t	dev_t;		/* device number */
typedef	u_int32_t	fixpt_t;	/* fixed point number */
typedef	u_int32_t	gid_t;		/* group id */
typedef	u_int32_t	id_t;		/* group id, process id or user id */
typedef	u_int32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */
typedef	u_int32_t	mode_t;		/* permissions */
typedef	u_int32_t	nlink_t;	/* link count */
typedef	quad_t		off_t;		/* file offset */
typedef	int32_t		pid_t;		/* process id */
typedef quad_t		rlim_t;		/* resource limit */
typedef	int32_t		segsz_t;	/* segment size */
typedef	int32_t		swblk_t;	/* swap offset */
typedef	u_int32_t	uid_t;		/* user id */

#if defined(_KERNEL) || defined(_LIBC)
/*
 * semctl(2)'s argument structure.  This is here for the benefit of
 * <sys/syscallargs.h>.  It is not in the user's namespace in SUSv2.
 * The SUSv2 semctl(2) takes variable arguments.
 */
union __semun {
	int		val;		/* value for SETVAL */
	struct semid_ds	*buf;		/* buffer for IPC_STAT & IPC_SET */
	unsigned short	*array;		/* array for GETALL & SETALL */
};
#endif /* _KERNEL || __LIBC12_SOURCE__ */

/*
 * These belong in unistd.h, but are placed here too to ensure that
 * long arguments will be promoted to off_t if the program fails to
 * include that header or explicitly cast them to off_t.
 */
#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#ifndef _KERNEL
#include <sys/cdefs.h>
__BEGIN_DECLS
off_t	 lseek __P((int, off_t, int));
int	 ftruncate __P((int, off_t));
int	 truncate __P((const char *, off_t));
__END_DECLS
#endif /* !_KERNEL */
#endif /* !defined(_POSIX_SOURCE) ... */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
/* Major, minor numbers, dev_t's. */
#define	major(x)	((int32_t)((((x) & 0x000fff00) >>  8)))
#define	minor(x)	((int32_t)((((x) & 0xfff00000) >> 12) | \
				   (((x) & 0x000000ff) >>  0)))
#define	makedev(x,y)	((dev_t)((((x) <<  8) & 0x000fff00) | \
				 (((y) << 12) & 0xfff00000) | \
				 (((y) <<  0) & 0x000000ff)))
#endif

#ifdef	_BSD_CLOCK_T_
typedef	_BSD_CLOCK_T_		clock_t;
#undef	_BSD_CLOCK_T_
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_		size_t;
#define _SIZE_T
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_		ssize_t;
#undef	_BSD_SSIZE_T_
#endif

#ifdef	_BSD_TIME_T_
typedef	_BSD_TIME_T_		time_t;
#undef	_BSD_TIME_T_
#endif

#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_		clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

#ifdef	_BSD_TIMER_T_
typedef	_BSD_TIMER_T_		timer_t;
#undef	_BSD_TIMER_T_
#endif

#ifdef	_BSD_SUSECONDS_T_
typedef	_BSD_SUSECONDS_T_	suseconds_t;
#undef	_BSD_SUSECONDS_T_
#endif

#ifdef	_BSD_USECONDS_T_
typedef	_BSD_USECONDS_T_	useconds_t;
#undef	_BSD_USECONDS_T_
#endif

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	NBBY	8		/* number of bits in a byte */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	256
#endif

typedef int32_t	fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */

#ifndef howmany
#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define	FD_COPY(f, t)	(void)memcpy((t), (f), sizeof(*(f)))
#define	FD_ZERO(p)	(void)memset((p), 0, sizeof(*(p)))

#if defined(__STDC__) && defined(_KERNEL)
/*
 * Forward structure declarations for function prototypes.  We include the
 * common structures that cross subsystem boundaries here; others are mostly
 * used in the same place that the structure is defined.
 */
struct	proc;
struct	pgrp;
struct	ucred;
struct	rusage;
struct	file;
struct	buf;
struct	tty;
struct	uio;
#endif

#endif /* !defined(_POSIX_SOURCE) ... */
#endif /* !_SYS_TYPES_H_ */
