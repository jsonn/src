/*	$NetBSD: mman.h,v 1.28.18.1 2003/08/17 10:14:59 tron Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)mman.h	8.2 (Berkeley) 1/9/95
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/featuretest.h>

#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#include <sys/ansi.h>

#ifndef	mode_t
typedef	__mode_t	mode_t;
#define	mode_t		__mode_t
#endif

#ifndef	off_t
typedef	__off_t		off_t;		/* file offset */
#define	off_t		__off_t
#endif


/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_NONE	0x00	/* no permissions */
#define	PROT_READ	0x01	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x04	/* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define	MAP_SHARED	0x0001	/* share changes */
#define	MAP_PRIVATE	0x0002	/* changes are private */

#ifdef _KERNEL
/*
 * Deprecated flag; these are treated as MAP_PRIVATE internally by
 * the kernel.
 */
#define	MAP_COPY	0x0004	/* "copy" region at mmap time */
#endif

/*
 * Other flags
 */
#define	MAP_FIXED	 0x0010	/* map addr must be exactly as requested */
#define	MAP_RENAME	 0x0020	/* Sun: rename private pages to file */
#define	MAP_NORESERVE	 0x0040	/* Sun: don't reserve needed swap area */
#define	MAP_INHERIT	 0x0080	/* region is retained after exec */
#define	MAP_NOEXTEND	 0x0100	/* for MAP_FILE, don't change file size */
#define	MAP_HASSEMAPHORE 0x0200	/* region may contain semaphores */
#define	MAP_TRYFIXED     0x0400 /* attempt hint address, even within break */

/*
 * Mapping type
 */
#define	MAP_FILE	0x0000	/* map from file (default) */
#define	MAP_ANON	0x1000	/* allocated from memory, swap space */

/*
 * Error indicator returned by mmap(2)
 */
#define	MAP_FAILED	((void *) -1)	/* mmap() failed */

/*
 * Flags to msync
 */
#define	MS_ASYNC	0x01	/* perform asynchronous writes */
#define	MS_INVALIDATE	0x02	/* invalidate cached data */
#define	MS_SYNC		0x04	/* perform synchronous writes */

/*
 * Flags to mlockall
 */
#define	MCL_CURRENT	0x01	/* lock all pages currently mapped */
#define	MCL_FUTURE	0x02	/* lock all pages mapped in the future */

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
/*
 * Advice to madvise
 */
#define	MADV_NORMAL	0	/* no further special treatment */
#define	MADV_RANDOM	1	/* expect random page references */
#define	MADV_SEQUENTIAL	2	/* expect sequential page references */
#define	MADV_WILLNEED	3	/* will need these pages */
#define	MADV_DONTNEED	4	/* dont need these pages */
#define	MADV_SPACEAVAIL	5	/* insure that resources are reserved */
#define	MADV_FREE	6	/* pages are empty, free them */
/*
 * Flags to minherit
 */
#define	MAP_INHERIT_SHARE	0	/* share with child */
#define	MAP_INHERIT_COPY	1	/* copy into child */
#define	MAP_INHERIT_NONE	2	/* absent from child */
#define	MAP_INHERIT_DONATE_COPY	3	/* copy and delete -- not
					   implemented in UVM */
#define	MAP_INHERIT_DEFAULT	MAP_INHERIT_COPY
#endif

#ifndef _KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
void   *mmap __P((void *, size_t, int, int, int, off_t));
int	munmap __P((void *, size_t));
int	mprotect __P((void *, size_t, int));
#ifdef __LIBC12_SOURCE__
int	msync __P((void *, size_t));
int	__msync13 __P((void *, size_t, int));
#else
int	msync __P((void *, size_t, int))	__RENAME(__msync13);
#endif
int	mlock __P((const void *, size_t));
int	munlock __P((const void *, size_t));
int	mlockall __P((int));
int	munlockall __P((void));
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
int	madvise __P((void *, size_t, int));
int	mincore __P((void *, size_t, char *));
int	minherit __P((void *, size_t, int));
#endif
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_MMAN_H_ */
