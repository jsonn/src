/*	$NetBSD: pool.h,v 1.27.4.1 2001/10/11 00:02:35 fvdl Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg; by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#ifndef _SYS_POOL_H_
#define _SYS_POOL_H_

#ifdef _KERNEL
#define	__POOL_EXPOSE
#endif

#if defined(_KERNEL_OPT)
#include "opt_pool.h"
#endif

#ifdef __POOL_EXPOSE
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/time.h>
#endif

#define PR_HASHTABSIZE		8

#ifdef __POOL_EXPOSE
struct pool_cache {
	TAILQ_ENTRY(pool_cache)
			pc_poollist;	/* entry on pool's group list */
	TAILQ_HEAD(, pool_cache_group)
			pc_grouplist;	/* Cache group list */
	struct pool_cache_group
			*pc_allocfrom;	/* group to allocate from */
	struct pool_cache_group
			*pc_freeto;	/* grop to free to */
	struct pool	*pc_pool;	/* parent pool */
	struct simplelock pc_slock;	/* mutex */

	int		(*pc_ctor)(void *, void *, int);
	void		(*pc_dtor)(void *, void *);
	void		*pc_arg;

	/* Statistics. */
	unsigned long	pc_hits;	/* cache hits */
	unsigned long	pc_misses;	/* cache misses */

	unsigned long	pc_ngroups;	/* # cache groups */

	unsigned long	pc_nitems;	/* # objects currently in cache */
};

struct pool {
	TAILQ_ENTRY(pool)
			pr_poollist;
	TAILQ_HEAD(,pool_item_header)
			pr_pagelist;	/* Allocated pages */
	struct pool_item_header	*pr_curpage;
	TAILQ_HEAD(,pool_cache)
			pr_cachelist;	/* Caches for this pool */
	unsigned int	pr_size;	/* Size of item */
	unsigned int	pr_align;	/* Requested alignment, must be 2^n */
	unsigned int	pr_itemoffset;	/* Align this offset in item */
	unsigned int	pr_minitems;	/* minimum # of items to keep */
	unsigned int	pr_minpages;	/* same in page units */
	unsigned int	pr_maxpages;	/* maximum # of pages to keep */
	unsigned int	pr_npages;	/* # of pages allocated */
	unsigned int	pr_pagesz;	/* page size, must be 2^n */
	unsigned long	pr_pagemask;	/* abbrev. of above */
	unsigned int	pr_pageshift;	/* shift corr. to above */
	unsigned int	pr_itemsperpage;/* # items that fit in a page */
	unsigned int	pr_slack;	/* unused space in a page */
	unsigned int	pr_nitems;	/* number of available items in pool */
	unsigned int	pr_nout;	/* # items currently allocated */
	unsigned int	pr_hardlimit;	/* hard limit to number of allocated
					   items */
	void		*(*pr_alloc)(unsigned long, int, int);
	void		(*pr_free)(void *, unsigned long, int);
	int		pr_mtype;	/* memory allocator tag */
	const char	*pr_wchan;	/* tsleep(9) identifier */
	unsigned int	pr_flags;	/* r/w flags */
	unsigned int	pr_roflags;	/* r/o flags */
#define PR_MALLOCOK	1
#define	PR_NOWAIT	0		/* for symmetry */
#define PR_WAITOK	2
#define PR_WANTED	4
#define PR_STATIC	8
#define PR_FREEHEADER	16
#define PR_URGENT	32
#define PR_PHINPAGE	64
#define PR_LOGGING	128
#define PR_LIMITFAIL	256	/* even if waiting, fail if we hit limit */
#define PR_RECURSIVE	512	/* pool contains pools, for vmstat(8) */

	/*
	 * `pr_slock' protects the pool's data structures when removing
	 * items from or returning items to the pool, or when reading
	 * or updating read/write fields in the pool descriptor.
	 *
	 * We assume back-end page allocators provide their own locking
	 * scheme.  They will be called with the pool descriptor _unlocked_,
	 * since the page allocators may block.
	 */
	struct simplelock	pr_slock;

	LIST_HEAD(,pool_item_header)		/* Off-page page headers */
			pr_hashtab[PR_HASHTABSIZE];

	int		pr_maxcolor;	/* Cache colouring */
	int		pr_curcolor;
	int		pr_phoffset;	/* Offset in page of page header */

	/*
	 * Warning message to be issued, and a per-time-delta rate cap,
	 * if the hard limit is reached.
	 */
	const char	*pr_hardlimit_warning;
	struct timeval	pr_hardlimit_ratecap;
	struct timeval	pr_hardlimit_warning_last;

	/*
	 * Instrumentation
	 */
	unsigned long	pr_nget;	/* # of successful requests */
	unsigned long	pr_nfail;	/* # of unsuccessful requests */
	unsigned long	pr_nput;	/* # of releases */
	unsigned long	pr_npagealloc;	/* # of pages allocated */
	unsigned long	pr_npagefree;	/* # of pages released */
	unsigned int	pr_hiwat;	/* max # of pages in pool */
	unsigned long	pr_nidle;	/* # of idle pages */

	/*
	 * Diagnostic aides.
	 */
	struct pool_log	*pr_log;
	int		pr_curlogentry;
	int		pr_logsize;

	const char	*pr_entered_file; /* reentrancy check */
	long		pr_entered_line;
};
#endif /* __POOL_EXPOSE */

#ifdef _KERNEL
void		pool_init(struct pool *, size_t, u_int, u_int,
				 int, const char *, size_t,
				 void *(*)__P((unsigned long, int, int)),
				 void  (*)__P((void *, unsigned long, int)),
				 int);
void		pool_destroy(struct pool *);

void		*pool_get(struct pool *, int);
void		pool_put(struct pool *, void *);
void		pool_reclaim(struct pool *);

#ifdef POOL_DIAGNOSTIC
/*
 * These versions do reentrancy checking.
 */
void		*_pool_get(struct pool *, int, const char *, long);
void		_pool_put(struct pool *, void *, const char *, long);
void		_pool_reclaim(struct pool *, const char *, long);
#define		pool_get(h, f)	_pool_get((h), (f), __FILE__, __LINE__)
#define		pool_put(h, v)	_pool_put((h), (v), __FILE__, __LINE__)
#define		pool_reclaim(h)	_pool_reclaim((h), __FILE__, __LINE__)
#endif /* POOL_DIAGNOSTIC */

int		pool_prime(struct pool *, int);
void		pool_setlowat(struct pool *, int);
void		pool_sethiwat(struct pool *, int);
void		pool_sethardlimit(struct pool *, int, const char *, int);
void		pool_drain(void *);

/*
 * Debugging and diagnostic aides.
 */
void		pool_print(struct pool *, const char *);
void		pool_printit(struct pool *, const char *,
		    void (*)(const char *, ...));
int		pool_chk(struct pool *, const char *);

/*
 * Alternate pool page allocator, provided for pools that know they
 * will never be accessed in interrupt context.
 */
void		*pool_page_alloc_nointr(unsigned long, int, int);
void		pool_page_free_nointr(void *, unsigned long, int);

/*
 * Pool cache routines.
 */
void		pool_cache_init(struct pool_cache *, struct pool *,
		    int (*ctor)(void *, void *, int),
		    void (*dtor)(void *, void *),
		    void *);
void		pool_cache_destroy(struct pool_cache *);
void		*pool_cache_get(struct pool_cache *, int);
void		pool_cache_put(struct pool_cache *, void *);
void		pool_cache_destruct_object(struct pool_cache *, void *);
void		pool_cache_invalidate(struct pool_cache *);
#endif /* _KERNEL */

#endif /* _SYS_POOL_H_ */
