/*	$NetBSD: puffs_msgif.c,v 1.20.2.1 2007/03/29 19:27:54 reinoud Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_msgif.c,v 1.20.2.1 2007/03/29 19:27:54 reinoud Exp $");

#include <sys/param.h>
#include <sys/fstrans.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/lock.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

/*
 * waitq data structures
 */

/*
 * While a request is going to userspace, park the caller within the
 * kernel.  This is the kernel counterpart of "struct puffs_req".
 */
struct puffs_park {
	struct puffs_req	*park_preq;	/* req followed by buf	*/
	uint64_t		park_id;	/* duplicate of preq_id */

	size_t			park_copylen;	/* userspace copylength	*/
	size_t			park_maxlen;	/* max size in comeback */
	struct buf		*park_bp;	/* bp, ASYNCBIOREAD	*/

	int			park_flags;

	kcondvar_t		park_cv;
	kmutex_t		park_ilck;

	TAILQ_ENTRY(puffs_park) park_entries;
};
#define PARKFLAG_WAITERGONE	0x01
#define PARKFLAG_PROCESSING	0x02
#define PARKFLAG_ASYNCBIOREAD	0x04

static struct pool_cache parkpc;
static struct pool parkpool;

static int
makepark(void *arg, void *obj, int flags)
{
	struct puffs_park *park = obj;

	cv_init(&park->park_cv, "puffsrpl");

	return 0;
}

static void
nukepark(void *arg, void *obj)
{
	struct puffs_park *park = obj;

	cv_destroy(&park->park_cv);
}

void
puffs_msgif_init()
{

	pool_init(&parkpool, sizeof(struct puffs_park), 0, 0, 0,
	    "puffprkl", &pool_allocator_nointr, IPL_NONE);
	pool_cache_init(&parkpc, &parkpool, makepark, nukepark, NULL);
}

void
puffs_msgif_destroy()
{

	pool_cache_destroy(&parkpc);
	pool_destroy(&parkpool);
}

void *
puffs_parkmem_alloc(int waitok)
{

	return pool_cache_get(&parkpc, waitok ? PR_WAITOK : PR_NOWAIT);
}

void
puffs_parkmem_free(void *ppark)
{

	pool_cache_put(&parkpc, ppark);
}


/*
 * Converts a non-FAF op to a FAF.  This simply involves making copies
 * of the park and request structures and tagging the request as a FAF.
 * It is safe to block here, since the original op is not a FAF.
 */
#if 0
static void
puffs_reqtofaf(struct puffs_park *ppark)
{
	struct puffs_req *newpreq;

	KASSERT((ppark->park_preq->preq_opclass & PUFFSOPFLAG_FAF) == 0);

	MALLOC(newpreq, struct puffs_req *, ppark->park_copylen,
	    M_PUFFS, M_ZERO | M_WAITOK);

	memcpy(newpreq, ppark->park_preq, ppark->park_copylen);

	ppark->park_preq = newpreq;
	ppark->park_preq->preq_opclass |= PUFFSOPFLAG_FAF;
}
#endif


/*
 * kernel-user-kernel waitqueues
 */

static int touser(struct puffs_mount *, struct puffs_park *, uint64_t,
		  struct vnode *, struct vnode *);

uint64_t
puffs_getreqid(struct puffs_mount *pmp)
{
	uint64_t rv;

	mutex_enter(&pmp->pmp_lock);
	rv = pmp->pmp_nextreq++;
	mutex_exit(&pmp->pmp_lock);

	return rv;
}

/* vfs request */
int
puffs_vfstouser(struct puffs_mount *pmp, int optype, void *kbuf, size_t buflen)
{
	struct puffs_park *ppark;

	ppark = pool_cache_get(&parkpc, PR_WAITOK);
	ppark->park_preq = kbuf;

	ppark->park_preq->preq_opclass = PUFFSOP_VFS; 
	ppark->park_preq->preq_optype = optype;

	ppark->park_maxlen = ppark->park_copylen = buflen;
	ppark->park_flags = 0;

	return touser(pmp, ppark, puffs_getreqid(pmp), NULL, NULL);
}

void
puffs_suspendtouser(struct puffs_mount *pmp, int status)
{
	struct puffs_vfsreq_suspend *pvfsr_susp;
	struct puffs_park *ppark;

	pvfsr_susp = malloc(sizeof(struct puffs_vfsreq_suspend),
	    M_PUFFS, M_WAITOK | M_ZERO);
	ppark = pool_cache_get(&parkpc, PR_WAITOK);

	pvfsr_susp->pvfsr_status = status;
	ppark->park_preq = (struct puffs_req *)pvfsr_susp;

	ppark->park_preq->preq_opclass = PUFFSOP_VFS | PUFFSOPFLAG_FAF;
	ppark->park_preq->preq_optype = PUFFS_VFS_SUSPEND;

	ppark->park_maxlen = ppark->park_copylen
	    = sizeof(struct puffs_vfsreq_suspend);
	ppark->park_flags = 0;

	(void)touser(pmp, ppark, 0, NULL, NULL);
}

/*
 * vnode level request
 */
int
puffs_vntouser(struct puffs_mount *pmp, int optype,
	void *kbuf, size_t buflen, void *cookie,
	struct vnode *vp1, struct vnode *vp2)
{
	struct puffs_park *ppark;

	ppark = pool_cache_get(&parkpc, PR_WAITOK);
	ppark->park_preq = kbuf;

	ppark->park_preq->preq_opclass = PUFFSOP_VN; 
	ppark->park_preq->preq_optype = optype;
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_maxlen = ppark->park_copylen = buflen;
	ppark->park_flags = 0;

	return touser(pmp, ppark, puffs_getreqid(pmp), vp1, vp2);
}

/*
 * vnode level request, caller-controller req id
 */
int
puffs_vntouser_req(struct puffs_mount *pmp, int optype,
	void *kbuf, size_t buflen, void *cookie, uint64_t reqid,
	struct vnode *vp1, struct vnode *vp2)
{
	struct puffs_park *ppark;

	ppark = pool_cache_get(&parkpc, PR_WAITOK);
	ppark->park_preq = kbuf;

	ppark->park_preq->preq_opclass = PUFFSOP_VN; 
	ppark->park_preq->preq_optype = optype;
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_maxlen = ppark->park_copylen = buflen;
	ppark->park_flags = 0;

	return touser(pmp, ppark, reqid, vp1, vp2);
}

int
puffs_vntouser_delta(struct puffs_mount *pmp, int optype,
	void *kbuf, size_t buflen, size_t maxdelta,
	void *cookie, struct vnode *vp1, struct vnode *vp2)
{
	struct puffs_park *ppark;

	ppark = pool_cache_get(&parkpc, PR_WAITOK);
	ppark->park_preq = kbuf;

	ppark->park_preq->preq_opclass = PUFFSOP_VN; 
	ppark->park_preq->preq_optype = optype;
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_copylen = buflen;
	ppark->park_maxlen = buflen + maxdelta;
	ppark->park_flags = 0;

	return touser(pmp, ppark, puffs_getreqid(pmp), vp1, vp2);
}

/*
 * File server interaction is async from caller perspective.
 * biodone(bp) is signalled in putop.
 */
void
puffs_vntouser_bioread_async(struct puffs_mount *pmp, void *cookie,
	size_t tomove, off_t offset, struct buf *bp,
	struct vnode *vp1, struct vnode *vp2)
{
	struct puffs_park *ppark;
	struct puffs_vnreq_read *read_argp;

	ppark = pool_cache_get(&parkpc, PR_WAITOK);
	MALLOC(read_argp, struct puffs_vnreq_read *,
	    sizeof(struct puffs_vnreq_read) + tomove,
	    M_PUFFS, M_WAITOK | M_ZERO);

	read_argp->pvnr_ioflag = 0;
	read_argp->pvnr_resid = tomove;
	read_argp->pvnr_offset = offset;
	puffs_credcvt(&read_argp->pvnr_cred, FSCRED);

	ppark->park_preq = (void *)read_argp;
	ppark->park_preq->preq_opclass = PUFFSOP_VN;
	ppark->park_preq->preq_optype = PUFFS_VN_READ;
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_copylen = sizeof(struct puffs_vnreq_read);
	ppark->park_maxlen = sizeof(struct puffs_vnreq_read) + tomove;
	ppark->park_bp = bp;
	ppark->park_flags = PARKFLAG_ASYNCBIOREAD;

	(void)touser(pmp, ppark, puffs_getreqid(pmp), vp1, vp2);
}

/*
 * Notice: kbuf will be free'd later.  I must be allocated from the
 * kernel heap and it's ownership is shifted to this function from
 * now on, i.e. the caller is not allowed to use it anymore!
 */
void
puffs_vntouser_faf(struct puffs_mount *pmp, int optype,
	void *kbuf, size_t buflen, void *cookie)
{
	struct puffs_park *ppark;

	/* XXX: is it allowable to sleep here? */
	ppark = pool_cache_get(&parkpc, PR_NOWAIT);
	if (ppark == NULL)
		return; /* 2bad */

	ppark->park_preq = kbuf;

	ppark->park_preq->preq_opclass = PUFFSOP_VN | PUFFSOPFLAG_FAF;
	ppark->park_preq->preq_optype = optype;
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_maxlen = ppark->park_copylen = buflen;
	ppark->park_flags = 0;

	(void)touser(pmp, ppark, 0, NULL, NULL);
}

void
puffs_cacheop(struct puffs_mount *pmp, struct puffs_park *ppark,
	struct puffs_cacheinfo *pcinfo, size_t pcilen, void *cookie)
{

	ppark->park_preq = (struct puffs_req *)pcinfo;
	ppark->park_preq->preq_opclass = PUFFSOP_CACHE | PUFFSOPFLAG_FAF;
	ppark->park_preq->preq_optype = PCACHE_TYPE_WRITE; /* XXX */
	ppark->park_preq->preq_cookie = cookie;

	ppark->park_maxlen = ppark->park_copylen = pcilen;

	(void)touser(pmp, ppark, 0, NULL, NULL); 
}

/*
 * Wait for the userspace ping-pong game in calling process context.
 *
 * This unlocks vnodes if they are supplied.  vp1 is the vnode
 * before in the locking order, i.e. the one which must be locked
 * before accessing vp2.  This is done here so that operations are
 * already ordered in the queue when vnodes are unlocked (I'm not
 * sure if that's really necessary, but it can't hurt).  Okok, maybe
 * there's a slight ugly-factor also, but let's not worry about that.
 */
static int
touser(struct puffs_mount *pmp, struct puffs_park *ppark, uint64_t reqid,
	struct vnode *vp1, struct vnode *vp2)
{
	struct mount *mp;
	struct puffs_req *preq;
	int rv = 0;

	mp = PMPTOMP(pmp);
	preq = ppark->park_preq;
	preq->preq_id = ppark->park_id = reqid;
	preq->preq_buflen = ALIGN(ppark->park_maxlen);

#if 0
	/*
	 * We don't trap signals currently
	 */
	struct lwp *l = curlwp;

	/*
	 * To support PCATCH, yet another movie: check if there are signals
	 * pending and we are issueing a non-FAF.  If so, return an error
	 * directly UNLESS we are issueing INACTIVE.  In that case, convert
	 * it to a FAF, fire off to the file server and return an error.
	 * Yes, this is bordering disgusting.  Barfbags are on me.
	 */
	if (PUFFSOP_WANTREPLY(preq->preq_opclass)
	   && (ppark->park_flags & PARKFLAG_ASYNCBIOREAD) == 0
	   && (l->l_flag & LW_PENDSIG) != 0 && sigispending(l, 0)) {
		if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN
		    && preq->preq_optype == PUFFS_VN_INACTIVE) {
			puffs_reqtofaf(ppark);
			DPRINTF(("puffs touser: converted to FAF %p\n", ppark));
			rv = EINTR;
		} else {
			return EINTR;
		}
	}
#endif

	/*
	 * test for suspension lock.
	 *
	 * Note that we *DO NOT* keep the lock, since that might block
	 * lock acquiring PLUS it would give userlandia control over
	 * the lock.  The operation queue enforces a strict ordering:
	 * when the fs server gets in the op stream, it knows things
	 * are in order.  The kernel locks can't guarantee that for
	 * userspace, in any case.
	 *
	 * BUT: this presents a problem for ops which have a consistency
	 * clause based on more than one operation.  Unfortunately such
	 * operations (read, write) do not reliably work yet.
	 *
	 * Ya, Ya, it's wrong wong wrong, me be fixink this someday.
	 *
	 * XXX: and there is one more problem.  We sometimes need to
	 * take a lazy lock in case the fs is suspending and we are
	 * executing as the fs server context.  This might happen
	 * e.g. in the case that the user server triggers a reclaim
	 * in the kernel while the fs is suspending.  It's not a very
	 * likely event, but it needs to be fixed some day.
	 */

	/*
	 * MOREXXX: once PUFFS_WCACHEINFO is enabled, we can't take
	 * the mutex here, since getpages() might be called locked.
	 */
	fstrans_start(mp, FSTRANS_NORMAL);
	mutex_enter(&pmp->pmp_lock);
	fstrans_done(mp);

	if (pmp->pmp_status != PUFFSTAT_RUNNING) {
		mutex_exit(&pmp->pmp_lock);
		pool_cache_put(&parkpc, ppark);
		return ENXIO;
	}

	TAILQ_INSERT_TAIL(&pmp->pmp_req_touser, ppark, park_entries);
	pmp->pmp_req_waiters++;

#if 0
	/*
	 * Don't do unlock-relock dance yet.  There are a couple of
	 * unsolved issues with it.  If we don't unlock, we can have
	 * processes wanting vn_lock in case userspace hangs.  But
	 * that can be "solved" by killing the userspace process.  It
	 * would of course be nicer to have antilocking in the userspace
	 * interface protocol itself.. your patience will be rewarded.
	 */
	/* unlock */
	if (vp2)
		VOP_UNLOCK(vp2, 0);
	if (vp1)
		VOP_UNLOCK(vp1, 0);
#endif

	DPRINTF(("touser: req %" PRIu64 ", preq: %p, park: %p, "
	    "c/t: 0x%x/0x%x, f: 0x%x\n", preq->preq_id, preq, ppark,
	    preq->preq_opclass, preq->preq_optype, ppark->park_flags));

	cv_broadcast(&pmp->pmp_req_waiter_cv);
	selnotify(pmp->pmp_sel, 0);

	if (PUFFSOP_WANTREPLY(preq->preq_opclass)
	    && (ppark->park_flags & PARKFLAG_ASYNCBIOREAD) == 0) {
		int error;

		error = 0; /* XXX: no interrupt for now */

		cv_wait(&ppark->park_cv, &pmp->pmp_lock);
		if (error) {
			ppark->park_flags |= PARKFLAG_WAITERGONE;
			if (ppark->park_flags & PARKFLAG_PROCESSING) {
				cv_wait(&ppark->park_cv, &pmp->pmp_lock);
				rv = preq->preq_rv;
			} else {
				rv = error;
			}
		} else {
			rv = preq->preq_rv;
		}
		mutex_exit(&pmp->pmp_lock);
		pool_cache_put(&parkpc, ppark);

		/*
		 * retake the lock and release.  This makes sure (haha,
		 * I'm humorous) that we don't process the same vnode in
		 * multiple threads due to the locks hacks we have in
		 * puffs_lock().  In reality this is well protected by
		 * the biglock, but once that's gone, well, hopefully
		 * this will be fixed for real.  (and when you read this
		 * comment in 2017 and subsequently barf, my condolences ;).
		 */
		if (rv == 0 && !fstrans_is_owner(mp)) {
			fstrans_start(mp, FSTRANS_NORMAL);
			fstrans_done(mp);
		}
	} else {
		mutex_exit(&pmp->pmp_lock);
	}

#if 0
	/* relock */
	if (vp1)
		KASSERT(vn_lock(vp1, LK_EXCLUSIVE | LK_RETRY) == 0);
	if (vp2)
		KASSERT(vn_lock(vp2, LK_EXCLUSIVE | LK_RETRY) == 0);
#endif

	mutex_enter(&pmp->pmp_lock);
	if (--pmp->pmp_req_waiters == 0) {
		KASSERT(cv_has_waiters(&pmp->pmp_req_waitersink_cv) <= 1);
		cv_signal(&pmp->pmp_req_waitersink_cv);
	}
	mutex_exit(&pmp->pmp_lock);

	return rv;
}


/*
 * getop: scan through queued requests until:
 *  1) max number of requests satisfied
 *     OR
 *  2) buffer runs out of space
 *     OR
 *  3) nonblocking is set AND there are no operations available
 *     OR
 *  4) at least one operation was transferred AND there are no more waiting
 */
int
puffs_getop(struct puffs_mount *pmp, struct puffs_reqh_get *phg, int nonblock)
{
	struct puffs_park *park;
	struct puffs_req *preq;
	uint8_t *bufpos;
	int error, donesome;

	donesome = error = 0;
	bufpos = phg->phg_buf;

	mutex_enter(&pmp->pmp_lock);
	while (phg->phg_nops == 0 || donesome != phg->phg_nops) {
 again:
		if (pmp->pmp_status != PUFFSTAT_RUNNING) {
			/* if we got some, they don't really matter anymore */
			error = ENXIO;
			goto out;
		}
		if (TAILQ_EMPTY(&pmp->pmp_req_touser)) {
			if (donesome)
				goto out;

			if (nonblock) {
				error = EWOULDBLOCK;
				goto out;
			}

			error = cv_wait_sig(&pmp->pmp_req_waiter_cv,
			    &pmp->pmp_lock);
			if (error)
				goto out;
			else
				goto again;
		}

		park = TAILQ_FIRST(&pmp->pmp_req_touser);
		preq = park->park_preq;
		if (phg->phg_buflen < preq->preq_buflen) {
			if (!donesome)
				error = E2BIG;
			goto out;
		}
		TAILQ_REMOVE(&pmp->pmp_req_touser, park, park_entries);

		/* If it's a goner, don't process any furher */
		if (park->park_flags & PARKFLAG_WAITERGONE) {
			panic("impossible for now");
			pool_cache_put(&parkpc, park);
			continue;
		}

		mutex_exit(&pmp->pmp_lock);

		DPRINTF(("puffsgetop: get op %" PRIu64 " (%d.), from %p "
		    "len %zu (buflen %zu), target %p\n", preq->preq_id,
		    donesome, preq, park->park_copylen, preq->preq_buflen,
		    bufpos));

		if ((error = copyout(preq, bufpos, park->park_copylen)) != 0) {
			DPRINTF(("puffs_getop: copyout failed\n"));
			/*
			 * ok, user server is probably trying to cheat.
			 * stuff op back & return error to user
			 */
			 mutex_enter(&pmp->pmp_lock);
			 TAILQ_INSERT_HEAD(&pmp->pmp_req_touser, park,
			     park_entries);

			 if (donesome)
				error = 0;
			 goto out;
		}
		bufpos += preq->preq_buflen;
		phg->phg_buflen -= preq->preq_buflen;
		donesome++;

		mutex_enter(&pmp->pmp_lock);
		if (PUFFSOP_WANTREPLY(preq->preq_opclass)) {
			TAILQ_INSERT_TAIL(&pmp->pmp_req_replywait, park,
			    park_entries);
		} else {
			free(preq, M_PUFFS);
			pool_cache_put(&parkpc, park);
		}
	}

 out:
	phg->phg_more = pmp->pmp_req_waiters;
	mutex_exit(&pmp->pmp_lock);

	phg->phg_nops = donesome;

	return error;
}

int
puffs_putop(struct puffs_mount *pmp, struct puffs_reqh_put *php)
{
	struct puffs_park *park;
	struct puffs_req tmpreq;
	struct puffs_req *nextpreq;
	struct buf *bp;
	void *userbuf;
	uint64_t id;
	size_t reqlen;
	int donesome, error, wgone;

	donesome = error = wgone = 0;

	id = php->php_id;
	userbuf = php->php_buf;
	reqlen = php->php_buflen;

	mutex_enter(&pmp->pmp_lock);
	while (donesome != php->php_nops) {
#ifdef PUFFSDEBUG
		DPRINTF(("puffsputop: searching for %" PRIu64 ", ubuf: %p, "
		    "len %zu\n", id, userbuf, reqlen));
#endif
		TAILQ_FOREACH(park, &pmp->pmp_req_replywait, park_entries) {
			if (park->park_id == id)
				break;
		}

		if (park == NULL) {
			error = EINVAL;
			break;
		}
		TAILQ_REMOVE(&pmp->pmp_req_replywait, park, park_entries);
		wgone = park->park_flags & PARKFLAG_WAITERGONE;
		park->park_flags |= PARKFLAG_PROCESSING;
		mutex_exit(&pmp->pmp_lock);

		/*
		 * If the caller has gone south, go to next, collect
		 * $200 and free the structure there instead of wakeup.
		 * We also need to copyin the 
		 */
		if (wgone) {
			panic("puffs: wgone impossible for now\n");
			DPRINTF(("puffs_putop: bad service - waiter gone for "
			    "park %p\n", park));
			error = copyin(userbuf, &tmpreq,
			    sizeof(struct puffs_req));
			if (error)
				goto loopout;
			nextpreq = &tmpreq;
			goto next;
		}

		if (reqlen == 0 || reqlen > park->park_maxlen) {
			reqlen = park->park_maxlen;
			DPRINTF(("puffsputop: kernel bufsize override: "
			    "%zu\n", reqlen));
		}

		DPRINTF(("puffsputpop: copyin from %p to %p, len %zu\n",
		    userbuf, park->park_preq, reqlen));
		error = copyin(userbuf, park->park_preq, reqlen);
		if (error)
			goto loopout;
		nextpreq = park->park_preq;
		bp = park->park_bp;

		if (park->park_flags & PARKFLAG_ASYNCBIOREAD) {
			struct puffs_vnreq_read *read_argp;
			size_t moved;

			bp->b_error = park->park_preq->preq_rv;

			DPRINTF(("puffs_putop: async bioread for park %p, "
			    "bp %p, error %d\n", park, bp, bp->b_error));

			if (bp->b_error == 0) {
				read_argp = (void *)park->park_preq;
				moved = park->park_maxlen
				    - sizeof(struct puffs_vnreq_read)
				    - read_argp->pvnr_resid;
				memcpy(bp->b_data, read_argp->pvnr_data, moved);
				bp->b_resid = bp->b_bcount - moved;
				biodone(bp);
			}
		}

 next:
		/* all's well, prepare for next op */
		id = nextpreq->preq_id;
		reqlen = nextpreq->preq_buflen;
		userbuf = nextpreq->preq_nextbuf;
		donesome++;

 loopout:
		if (error && park->park_preq) {
			park->park_preq->preq_rv = error;
			if (park->park_flags & PARKFLAG_ASYNCBIOREAD) {
				bp = park->park_bp;
				bp->b_error = error;
				bp->b_flags |= B_ERROR;
				biodone(bp);
			}
		}

		mutex_enter(&pmp->pmp_lock);
		if (!wgone) {
			if (park->park_flags & PARKFLAG_ASYNCBIOREAD) {
				free(park->park_preq, M_PUFFS);
				pool_cache_put(&parkpc, park);
			} else {
				DPRINTF(("puffs_putop: flagging done for "
				    "park %p\n", park));

				cv_signal(&park->park_cv);
			}
		}

		if (error)
			break;
		wgone = 0;
	}

	mutex_exit(&pmp->pmp_lock);
	php->php_nops -= donesome;

	return error;
}

/*
 * We're dead, kaput, RIP, slightly more than merely pining for the
 * fjords, belly-up, fallen, lifeless, finished, expired, gone to meet
 * our maker, ceased to be, etcetc.  YASD.  It's a dead FS!
 *
 * Caller must hold puffs mutex.
 */
void
puffs_userdead(struct puffs_mount *pmp)
{
	struct puffs_park *park;
	struct buf *bp;

	/*
	 * Mark filesystem status as dying so that operations don't
	 * attempt to march to userspace any longer.
	 */
	pmp->pmp_status = PUFFSTAT_DYING;

	/* signal waiters on REQUEST TO file server queue */
	TAILQ_FOREACH(park, &pmp->pmp_req_touser, park_entries) {
		uint8_t opclass;

		opclass = park->park_preq->preq_rv;
		park->park_preq->preq_rv = ENXIO;

		TAILQ_REMOVE(&pmp->pmp_req_touser, park, park_entries);

		if (park->park_flags & PARKFLAG_ASYNCBIOREAD) {
			bp = park->park_bp;
			bp->b_error = ENXIO;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			free(park->park_preq, M_PUFFS);
			pool_cache_put(&parkpc, park);
		} else if (!PUFFSOP_WANTREPLY(opclass)) {
			free(park->park_preq, M_PUFFS);
			pool_cache_put(&parkpc, park);
		} else {
			cv_signal(&park->park_cv);
		}
	}

	/* signal waiters on RESPONSE FROM file server queue */
	TAILQ_FOREACH(park, &pmp->pmp_req_replywait, park_entries) {
		KASSERT(PUFFSOP_WANTREPLY(park->park_preq->preq_opclass));

		park->park_preq->preq_rv = ENXIO;
		TAILQ_REMOVE(&pmp->pmp_req_replywait, park, park_entries);

		if (park->park_flags & PARKFLAG_ASYNCBIOREAD) {
			bp = park->park_bp;
			bp->b_error = ENXIO;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			free(park->park_preq, M_PUFFS);
			pool_cache_put(&parkpc, park);
		} else {
			cv_signal(&park->park_cv);
		}
	}
}

/* this is probably going to die away at some point? */
/*
 * XXX: currently bitrotted
 */
#if 0
static int
puffssizeop(struct puffs_mount *pmp, struct puffs_sizeop *psop_user)
{
	struct puffs_sizepark *pspark;
	void *kernbuf;
	size_t copylen;
	int error;

	/* locate correct op */
	mutex_enter(&pmp->pmp_lock);
	TAILQ_FOREACH(pspark, &pmp->pmp_req_sizepark, pkso_entries) {
		if (pspark->pkso_reqid == psop_user->pso_reqid) {
			TAILQ_REMOVE(&pmp->pmp_req_sizepark, pspark,
			    pkso_entries);
			break;
		}
	}
	mutex_exit(&pmp->pmp_lock);

	if (pspark == NULL)
		return EINVAL;

	error = 0;
	copylen = MIN(pspark->pkso_bufsize, psop_user->pso_bufsize);

	/*
	 * XXX: uvm stuff to avoid bouncy-bouncy copying?
	 */
	if (PUFFS_SIZEOP_UIO(pspark->pkso_reqtype)) {
		kernbuf = malloc(copylen, M_PUFFS, M_WAITOK | M_ZERO);
		if (pspark->pkso_reqtype == PUFFS_SIZEOPREQ_UIO_IN) {
			error = copyin(psop_user->pso_userbuf,
			    kernbuf, copylen);
			if (error) {
				printf("psop ERROR1 %d\n", error);
				goto escape;
			}
		}
		error = uiomove(kernbuf, copylen, pspark->pkso_uio);
		if (error) {
			printf("uiomove from kernel %p, len %d failed: %d\n",
			    kernbuf, (int)copylen, error);
			goto escape;
		}
			
		if (pspark->pkso_reqtype == PUFFS_SIZEOPREQ_UIO_OUT) {
			error = copyout(kernbuf,
			    psop_user->pso_userbuf, copylen);
			if (error) {
				printf("psop ERROR2 %d\n", error);
				goto escape;
			}
		}
 escape:
		free(kernbuf, M_PUFFS);
	} else if (PUFFS_SIZEOP_BUF(pspark->pkso_reqtype)) {
		copylen = MAX(pspark->pkso_bufsize, psop_user->pso_bufsize);
		if (pspark->pkso_reqtype == PUFFS_SIZEOPREQ_BUF_IN) {
			error = copyin(psop_user->pso_userbuf,
			pspark->pkso_copybuf, copylen);
		} else {
			error = copyout(pspark->pkso_copybuf,
			    psop_user->pso_userbuf, copylen);
		}
	}
#ifdef DIAGNOSTIC
	else
		panic("puffssizeop: invalid reqtype %d\n",
		    pspark->pkso_reqtype);
#endif /* DIAGNOSTIC */

	return error;
}
#endif
