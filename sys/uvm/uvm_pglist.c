/*	$NetBSD: uvm_pglist.c,v 1.13.2.5 2002/06/20 03:50:45 nathanw Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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

/*
 * uvm_pglist.c: pglist functions
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pglist.c,v 1.13.2.5 2002/06/20 03:50:45 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#ifdef VM_PAGE_ALLOC_MEMORY_STATS
#define	STAT_INCR(v)	(v)++
#define	STAT_DECR(v)	do { \
		if ((v) == 0) \
			printf("%s:%d -- Already 0!\n", __FILE__, __LINE__); \
		else \
			(v)--; \
	} while (0)
u_long	uvm_pglistalloc_npages;
#else
#define	STAT_INCR(v)
#define	STAT_DECR(v)
#endif

/*
 * uvm_pglistalloc: allocate a list of pages
 *
 * => allocated pages are placed at the tail of rlist.  rlist is
 *    assumed to be properly initialized by caller.
 * => returns 0 on success or errno on failure
 * => XXX: implementation allocates only a single segment, also
 *	might be able to better advantage of vm_physeg[].
 * => doesn't take into account clean non-busy pages on inactive list
 *	that could be used(?)
 * => params:
 *	size		the size of the allocation, rounded to page size.
 *	low		the low address of the allowed allocation range.
 *	high		the high address of the allowed allocation range.
 *	alignment	memory must be aligned to this power-of-two boundary.
 *	boundary	no segment in the allocation may cross this
 *			power-of-two boundary (relative to zero).
 */

static void uvm_pglist_add(struct vm_page *, struct pglist *);
static int uvm_pglistalloc_c_ps(int, psize_t, paddr_t, paddr_t,
				paddr_t, paddr_t, struct pglist *);
static int uvm_pglistalloc_contig(psize_t, paddr_t, paddr_t, paddr_t, paddr_t,
				  struct pglist *);
static void uvm_pglistalloc_s_ps(int, paddr_t, paddr_t,
				 struct pglist *, int *);
static int uvm_pglistalloc_simple(psize_t, paddr_t, paddr_t,
				  struct pglist *, int);

static void
uvm_pglist_add(pg, rlist)
	struct vm_page *pg;
	struct pglist *rlist;
{
	int free_list, color, pgflidx;
#ifdef DEBUG
	struct vm_page *tp;
#endif

#if PGFL_NQUEUES != 2
#error uvm_pglistalloc needs to be updated
#endif

	free_list = uvm_page_lookup_freelist(pg);
	color = VM_PGCOLOR_BUCKET(pg);
	pgflidx = (pg->flags & PG_ZERO) ? PGFL_ZEROS : PGFL_UNKNOWN;
#ifdef DEBUG
	for (tp = TAILQ_FIRST(&uvm.page_free[
		free_list].pgfl_buckets[color].pgfl_queues[pgflidx]);
	     tp != NULL;
	     tp = TAILQ_NEXT(tp, pageq)) {
		if (tp == pg)
			break;
	}
	if (tp == NULL)
		panic("uvm_pglistalloc: page not on freelist");
#endif
	TAILQ_REMOVE(&uvm.page_free[free_list].pgfl_buckets[
			color].pgfl_queues[pgflidx], pg, pageq);
	uvmexp.free--;
	if (pg->flags & PG_ZERO)
		uvmexp.zeropages--;
	pg->flags = PG_CLEAN;
	pg->pqflags = 0;
	pg->uobject = NULL;
	pg->uanon = NULL;
	TAILQ_INSERT_TAIL(rlist, pg, pageq);
	STAT_INCR(uvm_pglistalloc_npages);
}

static int
uvm_pglistalloc_c_ps(psi, size, low, high, alignment, boundary, rlist)
	int psi;
	psize_t size;
	paddr_t low, high, alignment, boundary;
	struct pglist *rlist;
{
	int try, limit, tryidx, end, idx;
	struct vm_page *pgs;
	paddr_t idxpa, lastidxpa;
	u_long pagemask;
#ifdef DEBUG
	int cidx;
#endif

	limit = min(atop(high), vm_physmem[psi].avail_end);
	pagemask = ~(boundary - 1);

	for (try = roundup(max(atop(low), vm_physmem[psi].avail_start),
			   atop(alignment));; try += atop(alignment)) {
		if (try + atop(size) >= limit) {

			/*
			 * We've run past the allowable range.
			 */

			return (0); /* FAIL */
		}
#ifdef DEBUG
		/*
		 * Make sure this is a managed physical page.
		 */

		if (vm_physseg_find(try, &cidx) != psi)
			panic("pgalloc contig: botch1");
		if (cidx != try - vm_physmem[psi].start)
			panic("pgalloc contig: botch2");
		if (vm_physseg_find(try + atop(size), &cidx) != psi)
			panic("pgalloc contig: botch3");
		if (cidx != try - vm_physmem[psi].start + atop(size))
			panic("pgalloc contig: botch4");		
#endif
		tryidx = try - vm_physmem[psi].start;
		end = tryidx + (size / PAGE_SIZE);
		pgs = vm_physmem[psi].pgs;

		/*
		 * Found a suitable starting page.  See of the range is free.
		 */

		for (idx = tryidx; idx < end; idx++) {
			if (VM_PAGE_IS_FREE(&pgs[idx]) == 0) {
				break;
			}
			idxpa = VM_PAGE_TO_PHYS(&pgs[idx]);
			if (idx > tryidx) {
				lastidxpa = VM_PAGE_TO_PHYS(&pgs[idx - 1]);
				if ((lastidxpa + PAGE_SIZE) != idxpa) {

					/*
					 * Region not contiguous.
					 */

					panic("pgalloc contig: botch5");
				}
				if (boundary != 0 &&
				    ((lastidxpa ^ idxpa) & pagemask) != 0) {

					/*
					 * Region crosses boundary.
					 */

					break;
				}
			}
		}
		if (idx == end) {
			break;
		}
	}

	/*
	 * we have a chunk of memory that conforms to the requested constraints.
	 */
	idx = tryidx;
	while (idx < end) {
		uvm_pglist_add(&pgs[idx++], rlist);
	}
	return (1);
}

static int
uvm_pglistalloc_contig(size, low, high, alignment, boundary, rlist)
	psize_t size;
	paddr_t low, high, alignment, boundary;
	struct pglist *rlist;
{
	int fl, psi;
	int s, error;

	if (boundary != 0 && boundary < size)
		return (EINVAL);

	/* Default to "lose". */
	error = ENOMEM;

	/*
	 * Block all memory allocation and lock the free list.
	 */

	s = uvm_lock_fpageq();

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (fl = 0; fl < VM_NFREELIST; fl++) {
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
		for (psi = vm_nphysseg - 1 ; psi >= 0 ; psi--)
#else
		for (psi = 0 ; psi < vm_nphysseg ; psi++)
#endif
		{
			if (vm_physmem[psi].free_list != fl)
				continue;

			if (uvm_pglistalloc_c_ps(psi, size, low, high,
						 alignment, boundary, rlist)) {
#if 0
				printf("pgalloc: %lx-%lx\n",
				       TAILQ_FIRST(rlist)->phys_addr,
				       TAILQ_LAST(rlist, pglist)->phys_addr);
#endif
				error = 0;
				goto out;
			}
		}
	}

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	UVM_KICK_PDAEMON();
	uvm_unlock_fpageq(s);
	return (error);
}

static void
uvm_pglistalloc_s_ps(psi, low, high, rlist, todo)
	int psi;
	paddr_t low, high;
	struct pglist *rlist;
	int *todo;
{
	int limit, try;
	struct vm_page *pg;
#ifdef DEBUG
	int cidx;
#endif

	limit = min(atop(high), vm_physmem[psi].avail_end);

	for (try = max(atop(low), vm_physmem[psi].avail_start);
	     try < limit; try ++) {
#ifdef DEBUG
		if (vm_physseg_find(try, &cidx) != psi)
			panic("pgalloc simple: botch1");
		if (cidx != (try - vm_physmem[psi].start))
			panic("pgalloc simple: botch2");
#endif
		pg = &vm_physmem[psi].pgs[try - vm_physmem[psi].start];
		if (VM_PAGE_IS_FREE(pg) == 0)
			continue;

		uvm_pglist_add(pg, rlist);
		if (--(*todo) == 0)
			break;
	}
}

static int
uvm_pglistalloc_simple(size, low, high, rlist, waitok)
	psize_t size;
	paddr_t low, high;
	struct pglist *rlist;
	int waitok;
{
	int fl, psi, s, todo, error;

	/* Default to "lose". */
	error = ENOMEM;

	todo = size / PAGE_SIZE;

again:
	/*
	 * Block all memory allocation and lock the free list.
	 */

	s = uvm_lock_fpageq();

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (fl = 0; fl < VM_NFREELIST; fl++) {
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
		for (psi = vm_nphysseg - 1 ; psi >= 0 ; psi--)
#else
		for (psi = 0 ; psi < vm_nphysseg ; psi++)
#endif
		{
			if (vm_physmem[psi].free_list != fl)
				continue;

			uvm_pglistalloc_s_ps(psi, low, high, rlist, &todo);
			if (todo == 0) {
				error = 0;
				goto out;
			}
		}

	}

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	UVM_KICK_PDAEMON();
	uvm_unlock_fpageq(s);
	if (error) {
		if (waitok) {
			/* XXX perhaps some time limitation? */
#ifdef DEBUG
			printf("pglistalloc waiting\n");
#endif
			uvm_wait("pglalloc");
			goto again;
		} else
			uvm_pglistfree(rlist);
	}
#if 0
	if (!error)
		printf("pgalloc: %lx..%lx\n",
		       TAILQ_FIRST(rlist)->phys_addr,
		       TAILQ_LAST(rlist, pglist)->phys_addr);
#endif
	return (error);
}

int
uvm_pglistalloc(size, low, high, alignment, boundary, rlist, nsegs, waitok)
	psize_t size;
	paddr_t low, high, alignment, boundary;
	struct pglist *rlist;
	int nsegs, waitok;
{
	int res;

	KASSERT((alignment & (alignment - 1)) == 0);
	KASSERT((boundary & (boundary - 1)) == 0);

	/*
	 * Our allocations are always page granularity, so our alignment
	 * must be, too.
	 */
	if (alignment < PAGE_SIZE)
		alignment = PAGE_SIZE;
	size = round_page(size);
	low = roundup(low, alignment);

	TAILQ_INIT(rlist);

	if ((nsegs < size / PAGE_SIZE) || (alignment != PAGE_SIZE)
	    || (boundary != 0))
		res = uvm_pglistalloc_contig(size, low, high, alignment,
					     boundary, rlist);
	else
		res = uvm_pglistalloc_simple(size, low, high, rlist, waitok);

	return (res);
}

/*
 * uvm_pglistfree: free a list of pages
 *
 * => pages should already be unmapped
 */

void
uvm_pglistfree(list)
	struct pglist *list;
{
	struct vm_page *pg;
	int s;

	/*
	 * Lock the free list and free each page.
	 */

	s = uvm_lock_fpageq();
	while ((pg = TAILQ_FIRST(list)) != NULL) {
		KASSERT((pg->pqflags & (PQ_ACTIVE|PQ_INACTIVE)) == 0);
		TAILQ_REMOVE(list, pg, pageq);
		pg->pqflags = PQ_FREE;
		TAILQ_INSERT_TAIL(&uvm.page_free[uvm_page_lookup_freelist(pg)].
		    pgfl_buckets[VM_PGCOLOR_BUCKET(pg)].
		    pgfl_queues[PGFL_UNKNOWN], pg, pageq);
		uvmexp.free++;
		if (uvmexp.zeropages < UVM_PAGEZERO_TARGET)
			uvm.page_idle_zero = vm_page_zero_enable;
		STAT_DECR(uvm_pglistalloc_npages);
	}
	uvm_unlock_fpageq(s);
}
