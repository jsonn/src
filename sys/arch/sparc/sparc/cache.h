/*	$NetBSD: cache.h,v 1.7.4.1 1996/06/12 20:41:22 pk Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
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
 *	@(#)cache.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4 and Sun-4c virtual address cache.
 *
 * Sun-4 virtual caches come in two flavors, write-through (Sun-4c)
 * and write-back (Sun-4).  The write-back caches are much faster
 * but require a bit more care.
 *
 * VAC_NONE is not actually used now, but if someone builds a physical
 * cache Sun-4 (or, more likely, a virtual index/physical tag cache)
 * everything will work (after pulling out the #ifdef notdef's: grep
 * for VAC_NONE to find them).
 */
enum vactype { VAC_NONE, VAC_WRITETHROUGH, VAC_WRITEBACK };

extern enum vactype vactype;	/* XXX  move into cacheinfo struct */

/*
 * Cache tags can be written in control space, and must be set to 0
 * (or invalid anyway) before turning on the cache.  The tags are
 * addressed as an array of 32-bit structures of the form:
 *
 *	struct cache_tag {
 *		u_int	:7,		(unused; must be zero)
 *			ct_cid:3,	(context ID)
 *			ct_w:1,		(write flag from PTE)
 *			ct_s:1,		(supervisor flag from PTE)
 *			ct_v:1,		(set => cache entry is valid)
 *			:3,		(unused; must be zero)
 *			ct_tid:14,	(cache tag ID)
 *			:2;		(unused; must be zero)
 *	};
 *
 * The SPARCstation 1 cache sees virtual addresses as:
 *
 *	struct cache_va {
 *		u_int	:2,		(unused; probably copies of va_tid<13>)
 *			cva_tid:14,	(tag ID)
 *			cva_line:12,	(cache line number)
 *			cva_byte:4;	(byte in cache line)
 *	};
 *
 * (The SS2 cache is similar but has half as many lines, each twice as long.)
 *
 * Note that, because the 12-bit line ID is `wider' than the page offset,
 * it is possible to have one page map to two different cache lines.
 * This can happen whenever two different physical pages have the same bits
 * in the part of the virtual address that overlaps the cache line ID, i.e.,
 * bits <15:12>.  In order to prevent cache duplication, we have to
 * make sure that no one page has more than one virtual address where
 * (va1 & 0xf000) != (va2 & 0xf000).  (The cache hardware turns off ct_v
 * when a cache miss occurs on a write, i.e., if va1 is in the cache and
 * va2 is not, and you write to va2, va1 goes out of the cache.  If va1
 * is in the cache and va2 is not, reading va2 also causes va1 to become
 * uncached, and the [same] data is then read from main memory into the
 * cache.)
 *
 * The other alternative, of course, is to disable caching of aliased
 * pages.  (In a few cases this might be faster anyway, but we do it
 * only when forced.)
 *
 * The Sun4, since it has an 8K pagesize instead of 4K, needs to check
 * bits that are one position higher.
 */

/* Some more well-known values: */

#define	CACHE_ALIAS_DIST_SUN4	0x20000
#define	CACHE_ALIAS_DIST_SUN4C	0x10000

#define	CACHE_ALIAS_BITS_SUN4	0x1e000
#define	CACHE_ALIAS_BITS_SUN4C	0xf000

#define CACHE_ALIAS_DIST_HS128k		0x20000
#define CACHE_ALIAS_BITS_HS128k		0x1f000
#define CACHE_ALIAS_DIST_HS256k		0x40000
#define CACHE_ALIAS_BITS_HS256k		0x3f000

#define CACHE_ALIAS_DIST_SS		0x0
#define CACHE_ALIAS_BITS_SS		0x0

#define CACHE_ALIAS_DIST_MS		GUESS_CACHE_ALIAS_DIST /* fix! */
#define CACHE_ALIAS_BITS_MS		GUESS_CACHE_ALIAS_BITS /* %%%  */

/*
 * Assuming a tag format where the least significant bits are the byte offset
 * into the cache line, and the next-most significant bits are the line id,
 * we can calculate the appropriate aliasing constants. We also assume that
 * the linesize and total cache size are powers of 2.
 */
#define GUESS_CACHE_ALIAS_BITS		((cacheinfo.c_totalsize - 1) & ~PGOFSET)
#define GUESS_CACHE_ALIAS_DIST		(cacheinfo.c_totalsize)

extern int cache_alias_dist;		/* If `guessed' */
extern int cache_alias_bits;

#define	CACHE_ALIAS_DIST	(CPU_ISSUN4M				 \
					? cache_alias_dist		 \
					: (CPU_ISSUN4C			 \
						? CACHE_ALIAS_DIST_SUN4C \
						: CACHE_ALIAS_DIST_SUN4))

#define	CACHE_ALIAS_BITS	(CPU_ISSUN4M				 \
					? cache_alias_bits		 \
					: (CPU_ISSUN4C			 \
						? CACHE_ALIAS_BITS_SUN4C \
						: CACHE_ALIAS_BITS_SUN4))

/*
 * True iff a1 and a2 are `bad' aliases (will cause cache duplication).
 */
#define	BADALIAS(a1, a2) (((int)(a1) ^ (int)(a2)) & CACHE_ALIAS_BITS)

/*
 * Routines for dealing with the cache.
 */
void	cache_enable __P((void));		/* turn it on */
void	cache_flush_context __P((void));	/* flush current context */
void	cache_flush_region __P((int));		/* flush region in cur ctx */
void	cache_flush_segment __P((int, int));	/* flush seg in cur ctx */
void	cache_flush_page __P((int va));		/* flush page in cur ctx */
void	cache_flush __P((caddr_t, u_int));	/* flush region */

/*
 * Cache control information.
 */
struct cacheinfo {
	int	c_totalsize;		/* total size, in bytes */
					/* if split, MAX(icache,dcache) */
	int	c_enabled;		/* true => cache is enabled */
	int	c_hwflush;		/* true => have hardware flush */
	int	c_linesize;		/* line size, in bytes */
	int	c_l2linesize;		/* log2(linesize) */
	int	c_physical;		/* true => cache is physical */
	int 	c_split;		/* true => cache is split */
	int 	ic_totalsize;		/* instruction cache */
	int 	ic_enabled;
	int 	ic_linesize;
	int 	ic_l2linesize;
	int 	dc_totalsize;		/* data cache */
	int 	dc_enabled;
	int 	dc_linesize;
	int 	dc_l2linesize;
	int	ec_totalsize;		/* external cache info */
	int 	ec_enabled;
	int	ec_linesize;
	int	ec_l2linesize;
};
extern struct cacheinfo cacheinfo;

/*
 * Cache control statistics.
 */
struct cachestats {
	int	cs_npgflush;		/* # page flushes */
	int	cs_nsgflush;		/* # seg flushes */
	int	cs_nrgflush;		/* # seg flushes */
	int	cs_ncxflush;		/* # context flushes */
	int	cs_nraflush;		/* # range flushes */
#ifdef notyet
	int	cs_ra[65];		/* pages/range */
#endif
};
