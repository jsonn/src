/*	$NetBSD: uvm_pdaemon.c,v 1.29.2.3 2001/08/24 00:13:44 nathanw Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and
 *      its contributors.
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
 *	@(#)vm_pageout.c        8.5 (Berkeley) 2/14/94
 * from: Id: uvm_pdaemon.c,v 1.1.2.32 1998/02/06 05:26:30 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_uvmhist.h"

/*
 * uvm_pdaemon.c: the page daemon
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/buf.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

/*
 * UVMPD_NUMDIRTYREACTS is how many dirty pages the pagedeamon will reactivate
 * in a pass thru the inactive list when swap is full.  the value should be
 * "small"... if it's too large we'll cycle the active pages thru the inactive
 * queue too quickly to for them to be referenced and avoid being freed.
 */

#define UVMPD_NUMDIRTYREACTS 16


/*
 * local prototypes
 */

static void		uvmpd_scan __P((void));
static boolean_t	uvmpd_scan_inactive __P((struct pglist *));
static void		uvmpd_tune __P((void));

/*
 * uvm_wait: wait (sleep) for the page daemon to free some pages
 *
 * => should be called with all locks released
 * => should _not_ be called by the page daemon (to avoid deadlock)
 */

void
uvm_wait(wmsg)
	const char *wmsg;
{
	int timo = 0;
	int s = splbio();

	/*
	 * check for page daemon going to sleep (waiting for itself)
	 */

	if (curproc == uvm.pagedaemon_proc) {
		/*
		 * now we have a problem: the pagedaemon wants to go to
		 * sleep until it frees more memory.   but how can it
		 * free more memory if it is asleep?  that is a deadlock.
		 * we have two options:
		 *  [1] panic now
		 *  [2] put a timeout on the sleep, thus causing the
		 *      pagedaemon to only pause (rather than sleep forever)
		 *
		 * note that option [2] will only help us if we get lucky
		 * and some other process on the system breaks the deadlock
		 * by exiting or freeing memory (thus allowing the pagedaemon
		 * to continue).  for now we panic if DEBUG is defined,
		 * otherwise we hope for the best with option [2] (better
		 * yet, this should never happen in the first place!).
		 */

		printf("pagedaemon: deadlock detected!\n");
		timo = hz >> 3;		/* set timeout */
#if defined(DEBUG)
		/* DEBUG: panic so we can debug it */
		panic("pagedaemon deadlock");
#endif
	}

	simple_lock(&uvm.pagedaemon_lock);
	wakeup(&uvm.pagedaemon);		/* wake the daemon! */
	UVM_UNLOCK_AND_WAIT(&uvmexp.free, &uvm.pagedaemon_lock, FALSE, wmsg,
	    timo);

	splx(s);
}


/*
 * uvmpd_tune: tune paging parameters
 *
 * => called when ever memory is added (or removed?) to the system
 * => caller must call with page queues locked
 */

static void
uvmpd_tune()
{
	UVMHIST_FUNC("uvmpd_tune"); UVMHIST_CALLED(pdhist);

	uvmexp.freemin = uvmexp.npages / 20;

	/* between 16k and 256k */
	/* XXX:  what are these values good for? */
	uvmexp.freemin = max(uvmexp.freemin, (16*1024) >> PAGE_SHIFT);
	uvmexp.freemin = min(uvmexp.freemin, (256*1024) >> PAGE_SHIFT);

	/* Make sure there's always a user page free. */
	if (uvmexp.freemin < uvmexp.reserve_kernel + 1)
		uvmexp.freemin = uvmexp.reserve_kernel + 1;

	uvmexp.freetarg = (uvmexp.freemin * 4) / 3;
	if (uvmexp.freetarg <= uvmexp.freemin)
		uvmexp.freetarg = uvmexp.freemin + 1;

	/* uvmexp.inactarg: computed in main daemon loop */

	uvmexp.wiredmax = uvmexp.npages / 3;
	UVMHIST_LOG(pdhist, "<- done, freemin=%d, freetarg=%d, wiredmax=%d",
	      uvmexp.freemin, uvmexp.freetarg, uvmexp.wiredmax, 0);
}

/*
 * uvm_pageout: the main loop for the pagedaemon
 */

void
uvm_pageout(void *arg)
{
	int npages = 0;
	UVMHIST_FUNC("uvm_pageout"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist,"<starting uvm pagedaemon>", 0, 0, 0, 0);

	/*
	 * ensure correct priority and set paging parameters...
	 */

	uvm.pagedaemon_proc = curproc;
	(void) spl0();
	uvm_lock_pageq();
	npages = uvmexp.npages;
	uvmpd_tune();
	uvm_unlock_pageq();

	/*
	 * main loop
	 */

	for (;;) {
		simple_lock(&uvm.pagedaemon_lock);

		UVMHIST_LOG(pdhist,"  <<SLEEPING>>",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(&uvm.pagedaemon,
		    &uvm.pagedaemon_lock, FALSE, "pgdaemon", 0);
		uvmexp.pdwoke++;
		UVMHIST_LOG(pdhist,"  <<WOKE UP>>",0,0,0,0);

		/* drain pool resources */
		pool_drain(0);

		/*
		 * now lock page queues and recompute inactive count
		 */

		uvm_lock_pageq();
		if (npages != uvmexp.npages) {	/* check for new pages? */
			npages = uvmexp.npages;
			uvmpd_tune();
		}

		uvmexp.inactarg = (uvmexp.active + uvmexp.inactive) / 3;
		if (uvmexp.inactarg <= uvmexp.freetarg) {
			uvmexp.inactarg = uvmexp.freetarg + 1;
		}

		UVMHIST_LOG(pdhist,"  free/ftarg=%d/%d, inact/itarg=%d/%d",
		    uvmexp.free, uvmexp.freetarg, uvmexp.inactive,
		    uvmexp.inactarg);

		/*
		 * scan if needed
		 */

		if (uvmexp.free + uvmexp.paging < uvmexp.freetarg ||
		    uvmexp.inactive < uvmexp.inactarg) {
			uvmpd_scan();
		}

		/*
		 * if there's any free memory to be had,
		 * wake up any waiters.
		 */

		if (uvmexp.free > uvmexp.reserve_kernel ||
		    uvmexp.paging == 0) {
			wakeup(&uvmexp.free);
		}

		/*
		 * scan done.  unlock page queues (the only lock we are holding)
		 */

		uvm_unlock_pageq();
	}
	/*NOTREACHED*/
}


/*
 * uvm_aiodone_daemon:  main loop for the aiodone daemon.
 */

void
uvm_aiodone_daemon(void *arg)
{
	int s, free;
	struct buf *bp, *nbp;
	UVMHIST_FUNC("uvm_aiodoned"); UVMHIST_CALLED(pdhist);

	for (;;) {

		/*
		 * carefully attempt to go to sleep (without losing "wakeups"!).
		 * we need splbio because we want to make sure the aio_done list
		 * is totally empty before we go to sleep.
		 */

		s = splbio();
		simple_lock(&uvm.aiodoned_lock);
		if (TAILQ_FIRST(&uvm.aio_done) == NULL) {
			UVMHIST_LOG(pdhist,"  <<SLEEPING>>",0,0,0,0);
			UVM_UNLOCK_AND_WAIT(&uvm.aiodoned,
			    &uvm.aiodoned_lock, FALSE, "aiodoned", 0);
			UVMHIST_LOG(pdhist,"  <<WOKE UP>>",0,0,0,0);

			/* relock aiodoned_lock, still at splbio */
			simple_lock(&uvm.aiodoned_lock);
		}

		/*
		 * check for done aio structures
		 */

		bp = TAILQ_FIRST(&uvm.aio_done);
		if (bp) {
			TAILQ_INIT(&uvm.aio_done);
		}

		simple_unlock(&uvm.aiodoned_lock);
		splx(s);

		/*
		 * process each i/o that's done.
		 */

		free = uvmexp.free;
		while (bp != NULL) {
			if (bp->b_flags & B_PDAEMON) {
				uvmexp.paging -= bp->b_bufsize >> PAGE_SHIFT;
			}
			nbp = TAILQ_NEXT(bp, b_freelist);
			(*bp->b_iodone)(bp);
			bp = nbp;
		}
		if (free <= uvmexp.reserve_kernel) {
			s = uvm_lock_fpageq();
			wakeup(&uvm.pagedaemon);
			uvm_unlock_fpageq(s);
		} else {
			simple_lock(&uvm.pagedaemon_lock);
			wakeup(&uvmexp.free);
			simple_unlock(&uvm.pagedaemon_lock);
		}
	}
}



/*
 * uvmpd_scan_inactive: scan an inactive list for pages to clean or free.
 *
 * => called with page queues locked
 * => we work on meeting our free target by converting inactive pages
 *    into free pages.
 * => we handle the building of swap-backed clusters
 * => we return TRUE if we are exiting because we met our target
 */

static boolean_t
uvmpd_scan_inactive(pglst)
	struct pglist *pglst;
{
	boolean_t retval = FALSE;	/* assume we haven't hit target */
	int s, free, result;
	struct vm_page *p, *nextpg;
	struct uvm_object *uobj;
	struct vm_page *pps[MAXBSIZE >> PAGE_SHIFT], **ppsp;
	int npages;
	struct vm_page *swpps[MAXBSIZE >> PAGE_SHIFT]; 	/* XXX: see below */
	int swnpages, swcpages;				/* XXX: see below */
	int swslot;
	struct vm_anon *anon;
	boolean_t swap_backed;
	vaddr_t start;
	int dirtyreacts, t;
	UVMHIST_FUNC("uvmpd_scan_inactive"); UVMHIST_CALLED(pdhist);

	/*
	 * swslot is non-zero if we are building a swap cluster.  we want
	 * to stay in the loop while we have a page to scan or we have
	 * a swap-cluster to build.
	 */

	swslot = 0;
	swnpages = swcpages = 0;
	free = 0;
	dirtyreacts = 0;

	for (p = TAILQ_FIRST(pglst); p != NULL || swslot != 0; p = nextpg) {

		/*
		 * note that p can be NULL iff we have traversed the whole
		 * list and need to do one final swap-backed clustered pageout.
		 */

		uobj = NULL;
		anon = NULL;

		if (p) {

			/*
			 * update our copy of "free" and see if we've met
			 * our target
			 */

			s = uvm_lock_fpageq();
			free = uvmexp.free;
			uvm_unlock_fpageq(s);

			if (free + uvmexp.paging >= uvmexp.freetarg << 2 ||
			    dirtyreacts == UVMPD_NUMDIRTYREACTS) {
				UVMHIST_LOG(pdhist,"  met free target: "
					    "exit loop", 0, 0, 0, 0);
				retval = TRUE;

				if (swslot == 0) {
					/* exit now if no swap-i/o pending */
					break;
				}

				/* set p to null to signal final swap i/o */
				p = NULL;
			}
		}

		if (p) {	/* if (we have a new page to consider) */

			/*
			 * we are below target and have a new page to consider.
			 */
			uvmexp.pdscans++;
			nextpg = TAILQ_NEXT(p, pageq);

			/*
			 * move referenced pages back to active queue and
			 * skip to next page.
			 */

			if (pmap_is_referenced(p)) {
				uvm_pageactivate(p);
				uvmexp.pdreact++;
				continue;
			}

			/*
			 * enforce the minimum thresholds on different
			 * types of memory usage.  if reusing the current
			 * page would reduce that type of usage below its
			 * minimum, reactivate the page instead and move
			 * on to the next page.
			 */

			t = uvmexp.active + uvmexp.inactive + uvmexp.free;
			if (p->uanon &&
			    uvmexp.anonpages <= (t * uvmexp.anonmin) >> 8) {
				uvm_pageactivate(p);
				uvmexp.pdreanon++;
				continue;
			}
			if (p->uobject && UVM_OBJ_IS_VTEXT(p->uobject) &&
			    uvmexp.vtextpages <= (t * uvmexp.vtextmin) >> 8) {
				uvm_pageactivate(p);
				uvmexp.pdrevtext++;
				continue;
			}
			if (p->uobject && UVM_OBJ_IS_VNODE(p->uobject) &&
			    !UVM_OBJ_IS_VTEXT(p->uobject) &&
			    uvmexp.vnodepages <= (t * uvmexp.vnodemin) >> 8) {
				uvm_pageactivate(p);
				uvmexp.pdrevnode++;
				continue;
			}

			/*
			 * first we attempt to lock the object that this page
			 * belongs to.  if our attempt fails we skip on to
			 * the next page (no harm done).  it is important to
			 * "try" locking the object as we are locking in the
			 * wrong order (pageq -> object) and we don't want to
			 * deadlock.
			 *
			 * the only time we expect to see an ownerless page
			 * (i.e. a page with no uobject and !PQ_ANON) is if an
			 * anon has loaned a page from a uvm_object and the
			 * uvm_object has dropped the ownership.  in that
			 * case, the anon can "take over" the loaned page
			 * and make it its own.
			 */

			/* is page part of an anon or ownerless ? */
			if ((p->pqflags & PQ_ANON) || p->uobject == NULL) {
				anon = p->uanon;
				KASSERT(anon != NULL);
				if (!simple_lock_try(&anon->an_lock)) {
					/* lock failed, skip this page */
					continue;
				}

				/*
				 * if the page is ownerless, claim it in the
				 * name of "anon"!
				 */

				if ((p->pqflags & PQ_ANON) == 0) {
					KASSERT(p->loan_count > 0);
					p->loan_count--;
					p->pqflags |= PQ_ANON;
					/* anon now owns it */
				}
				if (p->flags & PG_BUSY) {
					simple_unlock(&anon->an_lock);
					uvmexp.pdbusy++;
					/* someone else owns page, skip it */
					continue;
				}
				uvmexp.pdanscan++;
			} else {
				uobj = p->uobject;
				KASSERT(uobj != NULL);
				if (!simple_lock_try(&uobj->vmobjlock)) {
					/* lock failed, skip this page */
					continue;
				}
				if (p->flags & PG_BUSY) {
					simple_unlock(&uobj->vmobjlock);
					uvmexp.pdbusy++;
					/* someone else owns page, skip it */
					continue;
				}
				uvmexp.pdobscan++;
			}

			/*
			 * we now have the object and the page queues locked.
			 * the page is not busy.  remove all the permissions
			 * from the page so we can sync the modified info
			 * without any race conditions.  if the page is clean
			 * we can free it now and continue.
			 */

			pmap_page_protect(p, VM_PROT_NONE);
			if ((p->flags & PG_CLEAN) != 0 && pmap_is_modified(p)) {
				p->flags &= ~PG_CLEAN;
			}

			if (p->flags & PG_CLEAN) {
				if (p->pqflags & PQ_SWAPBACKED) {
					/* this page now lives only in swap */
					simple_lock(&uvm.swap_data_lock);
					uvmexp.swpgonly++;
					simple_unlock(&uvm.swap_data_lock);
				}

				uvm_pagefree(p);
				uvmexp.pdfreed++;

				if (anon) {

					/*
					 * an anonymous page can only be clean
					 * if it has backing store assigned.
					 */

					KASSERT(anon->an_swslot != 0);

					/* remove from object */
					anon->u.an_page = NULL;
					simple_unlock(&anon->an_lock);
				} else {
					/* pagefree has already removed the
					 * page from the object */
					simple_unlock(&uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * this page is dirty, skip it if we'll have met our
			 * free target when all the current pageouts complete.
			 */

			if (free + uvmexp.paging > uvmexp.freetarg << 2) {
				if (anon) {
					simple_unlock(&anon->an_lock);
				} else {
					simple_unlock(&uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * this page is dirty, but we can't page it out
			 * since all pages in swap are only in swap.
			 * reactivate it so that we eventually cycle
			 * all pages thru the inactive queue.
			 */

			KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
			if ((p->pqflags & PQ_SWAPBACKED) &&
			    uvmexp.swpgonly == uvmexp.swpages) {
				dirtyreacts++;
				uvm_pageactivate(p);
				if (anon) {
					simple_unlock(&anon->an_lock);
				} else {
					simple_unlock(&uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * if the page is swap-backed and dirty and swap space
			 * is full, free any swap allocated to the page
			 * so that other pages can be paged out.
			 */

			KASSERT(uvmexp.swpginuse <= uvmexp.swpages);
			if ((p->pqflags & PQ_SWAPBACKED) &&
			    uvmexp.swpginuse == uvmexp.swpages) {

				if ((p->pqflags & PQ_ANON) &&
				    p->uanon->an_swslot) {
					uvm_swap_free(p->uanon->an_swslot, 1);
					p->uanon->an_swslot = 0;
				}
				if (p->pqflags & PQ_AOBJ) {
					uao_dropswap(p->uobject,
						     p->offset >> PAGE_SHIFT);
				}
			}

			/*
			 * the page we are looking at is dirty.   we must
			 * clean it before it can be freed.  to do this we
			 * first mark the page busy so that no one else will
			 * touch the page.
			 */

			swap_backed = ((p->pqflags & PQ_SWAPBACKED) != 0);
			p->flags |= PG_BUSY;		/* now we own it */
			UVM_PAGE_OWN(p, "scan_inactive");
			uvmexp.pgswapout++;

			/*
			 * for swap-backed pages we need to (re)allocate
			 * swap space.
			 */

			if (swap_backed) {

				/*
				 * free old swap slot (if any)
				 */

				if (anon) {
					if (anon->an_swslot) {
						uvm_swap_free(anon->an_swslot,
						    1);
						anon->an_swslot = 0;
					}
				} else {
					uao_dropswap(uobj,
						     p->offset >> PAGE_SHIFT);
				}

				/*
				 * start new cluster (if necessary)
				 */

				if (swslot == 0) {
					swnpages = MAXBSIZE >> PAGE_SHIFT;
					swslot = uvm_swap_alloc(&swnpages,
					    TRUE);
					if (swslot == 0) {
						/* no swap?  give up! */
						p->flags &= ~PG_BUSY;
						UVM_PAGE_OWN(p, NULL);
						if (anon)
							simple_unlock(
							    &anon->an_lock);
						else
							simple_unlock(
							    &uobj->vmobjlock);
						continue;
					}
					swcpages = 0;	/* cluster is empty */
				}

				/*
				 * add block to cluster
				 */

				if (anon) {
					anon->an_swslot = swslot + swcpages;
				} else {
					result = uao_set_swslot(uobj,
					    p->offset >> PAGE_SHIFT,
					    swslot + swcpages);
					if (result == -1) {
						p->flags &= ~PG_BUSY;
						UVM_PAGE_OWN(p, NULL);
						simple_unlock(&uobj->vmobjlock);
						continue;
					}
				}
				swpps[swcpages] = p;
				swcpages++;
			}
		} else {

			/* if p == NULL we must be doing a last swap i/o */
			swap_backed = TRUE;
		}

		/*
		 * now consider doing the pageout.
		 *
		 * for swap-backed pages, we do the pageout if we have either
		 * filled the cluster (in which case (swnpages == swcpages) or
		 * run out of pages (p == NULL).
		 *
		 * for object pages, we always do the pageout.
		 */

		if (swap_backed) {
			if (p) {	/* if we just added a page to cluster */
				if (anon)
					simple_unlock(&anon->an_lock);
				else
					simple_unlock(&uobj->vmobjlock);

				/* cluster not full yet? */
				if (swcpages < swnpages)
					continue;
			}

			/* starting I/O now... set up for it */
			npages = swcpages;
			ppsp = swpps;
			/* for swap-backed pages only */
			start = (vaddr_t) swslot;

			/* if this is final pageout we could have a few
			 * extra swap blocks */
			if (swcpages < swnpages) {
				uvm_swap_free(swslot + swcpages,
				    (swnpages - swcpages));
			}
		} else {
			/* normal object pageout */
			ppsp = pps;
			npages = sizeof(pps) / sizeof(struct vm_page *);
			/* not looked at because PGO_ALLPAGES is set */
			start = 0;
		}

		/*
		 * now do the pageout.
		 *
		 * for swap_backed pages we have already built the cluster.
		 * for !swap_backed pages, uvm_pager_put will call the object's
		 * "make put cluster" function to build a cluster on our behalf.
		 *
		 * we pass the PGO_PDFREECLUST flag to uvm_pager_put to instruct
		 * it to free the cluster pages for us on a successful I/O (it
		 * always does this for un-successful I/O requests).  this
		 * allows us to do clustered pageout without having to deal
		 * with cluster pages at this level.
		 *
		 * note locking semantics of uvm_pager_put with PGO_PDFREECLUST:
		 *  IN: locked: uobj (if !swap_backed), page queues
		 * OUT:!locked: pageqs, uobj
		 */

		/* locked: uobj (if !swap_backed), page queues */
		uvmexp.pdpageouts++;
		result = uvm_pager_put(swap_backed ? NULL : uobj, p,
		    &ppsp, &npages, PGO_ALLPAGES|PGO_PDFREECLUST, start, 0);
		/* unlocked: pageqs, uobj */

		/*
		 * if we did i/o to swap, zero swslot to indicate that we are
		 * no longer building a swap-backed cluster.
		 */

		if (swap_backed)
			swslot = 0;		/* done with this cluster */

		/*
		 * if the pageout failed, reactivate the page and continue.
		 */

		if (result == EIO && curproc == uvm.pagedaemon_proc) {
			uvm_lock_pageq();
			nextpg = TAILQ_NEXT(p, pageq);
			uvm_pageactivate(p);
			continue;
		}

		/*
		 * the pageout is in progress.  bump counters and set up
		 * for the next loop.
		 */

		uvm_lock_pageq();
		uvmexp.paging += npages;
		uvmexp.pdpending++;
		if (p) {
			if (p->pqflags & PQ_INACTIVE)
				nextpg = TAILQ_NEXT(p, pageq);
			else
				nextpg = TAILQ_FIRST(pglst);
		} else {
			nextpg = NULL;
		}
	}
	return (retval);
}

/*
 * uvmpd_scan: scan the page queues and attempt to meet our targets.
 *
 * => called with pageq's locked
 */

void
uvmpd_scan()
{
	int s, free, inactive_shortage, swap_shortage, pages_freed;
	struct vm_page *p, *nextpg;
	struct uvm_object *uobj;
	boolean_t got_it;
	UVMHIST_FUNC("uvmpd_scan"); UVMHIST_CALLED(pdhist);

	uvmexp.pdrevs++;		/* counter */
	uobj = NULL;

	/*
	 * get current "free" page count
	 */
	s = uvm_lock_fpageq();
	free = uvmexp.free;
	uvm_unlock_fpageq(s);

#ifndef __SWAP_BROKEN
	/*
	 * swap out some processes if we are below our free target.
	 * we need to unlock the page queues for this.
	 */
	if (free < uvmexp.freetarg) {
		uvmexp.pdswout++;
		UVMHIST_LOG(pdhist,"  free %d < target %d: swapout", free,
		    uvmexp.freetarg, 0, 0);
		uvm_unlock_pageq();
		uvm_swapout_threads();
		uvm_lock_pageq();

	}
#endif

	/*
	 * now we want to work on meeting our targets.   first we work on our
	 * free target by converting inactive pages into free pages.  then
	 * we work on meeting our inactive target by converting active pages
	 * to inactive ones.
	 */

	UVMHIST_LOG(pdhist, "  starting 'free' loop",0,0,0,0);

	/*
	 * alternate starting queue between swap and object based on the
	 * low bit of uvmexp.pdrevs (which we bump by one each call).
	 */

	got_it = FALSE;
	pages_freed = uvmexp.pdfreed;
	(void) uvmpd_scan_inactive(&uvm.page_inactive);
	pages_freed = uvmexp.pdfreed - pages_freed;

	/*
	 * we have done the scan to get free pages.   now we work on meeting
	 * our inactive target.
	 */

	inactive_shortage = uvmexp.inactarg - uvmexp.inactive;

	/*
	 * detect if we're not going to be able to page anything out
	 * until we free some swap resources from active pages.
	 */

	swap_shortage = 0;
	if (uvmexp.free < uvmexp.freetarg &&
	    uvmexp.swpginuse == uvmexp.swpages &&
	    uvmexp.swpgonly < uvmexp.swpages &&
	    pages_freed == 0) {
		swap_shortage = uvmexp.freetarg - uvmexp.free;
	}

	UVMHIST_LOG(pdhist, "  loop 2: inactive_shortage=%d swap_shortage=%d",
		    inactive_shortage, swap_shortage,0,0);
	for (p = TAILQ_FIRST(&uvm.page_active);
	     p != NULL && (inactive_shortage > 0 || swap_shortage > 0);
	     p = nextpg) {
		nextpg = TAILQ_NEXT(p, pageq);
		if (p->flags & PG_BUSY)
			continue;	/* quick check before trying to lock */

		/*
		 * lock the page's owner.
		 */
		/* is page anon owned or ownerless? */
		if ((p->pqflags & PQ_ANON) || p->uobject == NULL) {
			KASSERT(p->uanon != NULL);
			if (!simple_lock_try(&p->uanon->an_lock))
				continue;

			/* take over the page? */
			if ((p->pqflags & PQ_ANON) == 0) {
				KASSERT(p->loan_count > 0);
				p->loan_count--;
				p->pqflags |= PQ_ANON;
			}
		} else {
			if (!simple_lock_try(&p->uobject->vmobjlock))
				continue;
		}

		/*
		 * skip this page if it's busy.
		 */

		if ((p->flags & PG_BUSY) != 0) {
			if (p->pqflags & PQ_ANON)
				simple_unlock(&p->uanon->an_lock);
			else
				simple_unlock(&p->uobject->vmobjlock);
			continue;
		}

		/*
		 * if there's a shortage of swap, free any swap allocated
		 * to this page so that other pages can be paged out.
		 */

		if (swap_shortage > 0) {
			if ((p->pqflags & PQ_ANON) && p->uanon->an_swslot) {
				uvm_swap_free(p->uanon->an_swslot, 1);
				p->uanon->an_swslot = 0;
				p->flags &= ~PG_CLEAN;
				swap_shortage--;
			}
			if (p->pqflags & PQ_AOBJ) {
				int slot = uao_set_swslot(p->uobject,
					p->offset >> PAGE_SHIFT, 0);
				if (slot) {
					uvm_swap_free(slot, 1);
					p->flags &= ~PG_CLEAN;
					swap_shortage--;
				}
			}
		}

		/*
		 * If we're short on inactive pages, move this over
		 * to the inactive list.  The second hand will sweep
		 * it later, and if it has been referenced again, it
		 * will be moved back to active.
		 */

		if (inactive_shortage > 0) {
			pmap_clear_reference(p);
			/* no need to check wire_count as pg is "active" */
			uvm_pagedeactivate(p);
			uvmexp.pddeact++;
			inactive_shortage--;
		}
		if (p->pqflags & PQ_ANON)
			simple_unlock(&p->uanon->an_lock);
		else
			simple_unlock(&p->uobject->vmobjlock);
	}
}
