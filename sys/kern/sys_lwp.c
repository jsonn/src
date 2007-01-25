/*	$NetBSD: sys_lwp.c,v 1.1.2.9 2007/01/25 20:20:28 ad Exp $	*/

/*-
 * Copyright (c) 2001, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams, and Andrew Doran.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Lightweight process (LWP) system calls.  See kern_lwp.c for a description
 * of LWPs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_lwp.c,v 1.1.2.9 2007/01/25 20:20:28 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/types.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/sleepq.h>

#include <uvm/uvm_extern.h>

#define	LWP_UNPARK_MAX		1024

syncobj_t lwp_park_sobj = {
	SOBJ_SLEEPQ_SORTED,
	sleepq_unsleep,
	sleepq_changepri
};

sleeptab_t	lwp_park_tab;

#ifdef LWP_COUNTERS
struct evcnt	lwp_ev_park_early = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "unparked early");
struct evcnt	lwp_ev_park_raced = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "raced");
struct evcnt	lwp_ev_park_miss = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "not parked");
struct evcnt	lwp_ev_park_bcast = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "broadcast unpark");
struct evcnt	lwp_ev_park_targ = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "targeted unpark");
struct evcnt	lwp_ev_park = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "_lwp_park", "parked");

#define	LWP_COUNT(ev, val)	(ev).ev_count += (val)	/* XXXSMP */
#else
#define	LWP_COUNT(ev, val)	/* nothing */
#endif

void
lwp_sys_init(void)
{
	sleeptab_init(&lwp_park_tab);
#ifdef LWP_COUNTERS
	evcnt_attach_static(&lwp_ev_park_early);
	evcnt_attach_static(&lwp_ev_park_raced);
	evcnt_attach_static(&lwp_ev_park_miss);
	evcnt_attach_static(&lwp_ev_park_bcast);
	evcnt_attach_static(&lwp_ev_park_targ);
	evcnt_attach_static(&lwp_ev_park);
#endif
}

/* ARGSUSED */
int
sys__lwp_create(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_create_args /* {
		syscallarg(const ucontext_t *) ucp;
		syscallarg(u_long) flags;
		syscallarg(lwpid_t *) new_lwp;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct lwp *l2;
	vaddr_t uaddr;
	boolean_t inmem;
	ucontext_t *newuc;
	int error, lid;

	mutex_enter(&p->p_smutex);
	if ((p->p_sflag & (PS_SA | PS_WEXIT)) != 0 || p->p_sa != NULL) {
		mutex_exit(&p->p_smutex);
		return EINVAL;
	}
	p->p_sflag |= PS_NOSA;
	mutex_exit(&p->p_smutex);

	newuc = pool_get(&lwp_uc_pool, PR_WAITOK);

	error = copyin(SCARG(uap, ucp), newuc,
	    l->l_proc->p_emul->e_sa->sae_ucsize);
	if (error) {
		pool_put(&lwp_uc_pool, newuc);
		return error;
	}

	/* XXX check against resource limits */

	inmem = uvm_uarea_alloc(&uaddr);
	if (__predict_false(uaddr == 0)) {
		pool_put(&lwp_uc_pool, newuc);
		return ENOMEM;
	}

	newlwp(l, p, uaddr, inmem,
	    SCARG(uap, flags) & LWP_DETACHED,
	    NULL, 0, startlwp, newuc, &l2);

	/*
	 * Set the new LWP running, unless the caller has requested that
	 * it be created in suspended state.  If the process is stopping,
	 * then the LWP is created stopped.
	 */
	mutex_enter(&p->p_smutex);
	lwp_lock(l2);
	lid = l2->l_lid;
	if ((SCARG(uap, flags) & LWP_SUSPENDED) == 0 &&
	    (l->l_flag & (L_WREBOOT | L_WSUSPEND | L_WEXIT)) == 0) {
	    	if (p->p_stat == SSTOP || (p->p_sflag & PS_STOPPING) != 0)
	    		l2->l_stat = LSSTOP;
		else {
			LOCK_ASSERT(lwp_locked(l2, &sched_mutex));
			p->p_nrlwps++;
			l2->l_stat = LSRUN;
			setrunqueue(l2);
		}
	} else
		l2->l_stat = LSSUSPENDED;
	lwp_unlock(l2);
	mutex_exit(&p->p_smutex);

	error = copyout(&lid, SCARG(uap, new_lwp), sizeof(lid));
	if (error)
		return error;

	return 0;
}

int
sys__lwp_exit(struct lwp *l, void *v, register_t *retval)
{

	lwp_exit(l);
	return 0;
}

int
sys__lwp_self(struct lwp *l, void *v, register_t *retval)
{

	*retval = l->l_lid;
	return 0;
}

int
sys__lwp_getprivate(struct lwp *l, void *v, register_t *retval)
{

	*retval = (uintptr_t)l->l_private;
	return 0;
}

int
sys__lwp_setprivate(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_setprivate_args /* {
		syscallarg(void *) ptr;
	} */ *uap = v;

	l->l_private = SCARG(uap, ptr);
	return 0;
}

int
sys__lwp_suspend(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_suspend_args /* {
		syscallarg(lwpid_t) target;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct lwp *t;
	int error;

	mutex_enter(&p->p_smutex);
	if ((t = lwp_find(p, SCARG(uap, target))) == NULL) {
		mutex_exit(&p->p_smutex);
		return ESRCH;
	}

	/*
	 * Check for deadlock, which is only possible when we're suspending
	 * ourself.  XXX There is a short race here, as p_nrlwps is only
	 * incremented when an LWP suspends itself on the kernel/user
	 * boundary.  It's still possible to kill -9 the process so we
	 * don't bother checking further.
	 */
	lwp_lock(t);
	if ((t == l && p->p_nrlwps == 1) ||
	    (l->l_flag & (L_WCORE | L_WEXIT)) != 0) {
		lwp_unlock(t);
		mutex_exit(&p->p_smutex);
		return EDEADLK;
	}

	/*
	 * Suspend the LWP.  XXX If it's on a different CPU, we should wait
	 * for it to be preempted, where it will put itself to sleep. 
	 *
	 * Suspension of the current LWP will happen on return to userspace.
	 */
	error = lwp_suspend(l, t);
	mutex_exit(&p->p_smutex);

	return error;
}

int
sys__lwp_continue(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_continue_args /* {
		syscallarg(lwpid_t) target;
	} */ *uap = v;
	int error;
	struct proc *p = l->l_proc;
	struct lwp *t;

	error = 0;

	mutex_enter(&p->p_smutex);

	if ((p->p_sflag & PS_SA) != 0 || p->p_sa != NULL) {
		mutex_exit(&p->p_smutex);
		return EINVAL;
	}

	if ((t = lwp_find(p, SCARG(uap, target))) == NULL) {
		mutex_exit(&p->p_smutex);
		return ESRCH;
	}

	lwp_lock(t);
	lwp_continue(t);
	mutex_exit(&p->p_smutex);

	return error;
}

int
sys__lwp_wakeup(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_wakeup_args /* {
		syscallarg(lwpid_t) target;
	} */ *uap = v;
	struct lwp *t;
	struct proc *p;
	int error;

	p = l->l_proc;
	mutex_enter(&p->p_smutex);

	if ((t = lwp_find(p, SCARG(uap, target))) == NULL) {
		mutex_exit(&p->p_smutex);
		return ESRCH;
	}

	lwp_lock(t);

	if (t->l_stat != LSSLEEP) {
		error = ENODEV;
		goto bad;
	}

	if ((t->l_flag & L_SINTR) == 0) {
		error = EBUSY;
		goto bad;
	}

	/* wake it up  setrunnable() will release the LWP lock. */
	t->l_flag |= L_CANCELLED;
	setrunnable(t);
	mutex_exit(&p->p_smutex);
	return 0;

 bad:
 	lwp_unlock(t);
	mutex_exit(&p->p_smutex);
	return error;
}

int
sys__lwp_wait(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_wait_args /* {
		syscallarg(lwpid_t) wait_for;
		syscallarg(lwpid_t *) departed;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;
	lwpid_t dep;

	mutex_enter(&p->p_smutex);
	error = lwp_wait1(l, SCARG(uap, wait_for), &dep, 0);
	mutex_exit(&p->p_smutex);

	if (error)
		return error;

	if (SCARG(uap, departed)) {
		error = copyout(&dep, SCARG(uap, departed), sizeof(dep));
		if (error)
			return error;
	}

	return 0;
}

/* ARGSUSED */
int
sys__lwp_kill(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_kill_args /* {
		syscallarg(lwpid_t)	target;
		syscallarg(int)		signo;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct lwp *t;
	ksiginfo_t ksi;
	int signo = SCARG(uap, signo);
	int error = 0;

	if ((u_int)signo >= NSIG)
		return EINVAL;

	KSI_INIT(&ksi);
	ksi.ksi_signo = signo;
	ksi.ksi_code = SI_USER;
	ksi.ksi_pid = p->p_pid;
	ksi.ksi_uid = kauth_cred_geteuid(l->l_cred);
	ksi.ksi_lid = SCARG(uap, target);

	mutex_enter(&proclist_mutex);
	mutex_enter(&p->p_smutex);
	if ((t = lwp_find(p, ksi.ksi_lid)) == NULL)
		error = ESRCH;
	else if (signo != 0)
		kpsignal2(p, &ksi);
	mutex_exit(&p->p_smutex);
	mutex_exit(&proclist_mutex);

	return error;
}

int
sys__lwp_detach(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_detach_args /* {
		syscallarg(lwpid_t)	target;
	} */ *uap = v;
	struct proc *p;
	struct lwp *t;
	lwpid_t target;
	int error;

	target = SCARG(uap, target);
	p = l->l_proc;

	mutex_enter(&p->p_smutex);

	if (l->l_lid == target)
		t = l;
	else {
		/*
		 * We can't use lwp_find() here because the target might
		 * be a zombie.
		 */
		LIST_FOREACH(t, &p->p_lwps, l_sibling)
			if (t->l_lid == target)
				break;
	}

	/*
	 * If the LWP is already detached, there's nothing to do.
	 * If it's a zombie, we need to clean up after it.  LSZOMB
	 * is visible with the proc mutex held.
	 *
	 * After we have detached or released the LWP, kick any
	 * other LWPs that may be sitting in _lwp_wait(), waiting
	 * for the target LWP to exit.
	 */
	if (t != NULL && t->l_stat != LSIDL) {
		if ((t->l_prflag & LPR_DETACHED) == 0) {
			p->p_ndlwps++;
			t->l_prflag |= LPR_DETACHED;
			if (t->l_stat == LSZOMB) {
				lwp_free(t, 0, 0); /* releases proc mutex */
				cv_broadcast(&p->p_lwpcv);
				return 0;
			}
			error = 0;
		} else
			error = EINVAL;
	} else
		error = ESRCH;

	mutex_exit(&p->p_smutex);
	cv_broadcast(&p->p_lwpcv);

	return error;
}

static inline wchan_t
lwp_park_wchan(struct proc *p, const void *hint)
{
	return (wchan_t)((uintptr_t)p ^ (uintptr_t)hint);
}

/*
 * 'park' an LWP waiting on a user-level synchronisation object.  The LWP
 * will remain parked until another LWP in the same process calls in and
 * requests that it be unparked.
 */
int
sys__lwp_park(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_park_args /* {
		syscallarg(const struct timespec *)	ts;
		syscallarg(ucontext_t *)		uc;
		syscallarg(const void *)		hint;
	} */ *uap = v;
	const struct timespec *tsp;
	struct timespec ts, tsx;
	struct timeval tv;
	sleepq_t *sq;
	wchan_t wchan;
	int timo, error;

	/* Fix up the given timeout value. */
	if ((tsp = SCARG(uap, ts)) != NULL) {
		if ((error = copyin(tsp, &ts, sizeof(ts))) != 0)
			return error;
		getnanotime(&tsx);
		timespecsub(&ts, &tsx, &ts);
		tv.tv_sec = ts.tv_sec;
		tv.tv_usec = ts.tv_nsec / 1000;
		if (tv.tv_sec < 0 || (tv.tv_sec == 0 && tv.tv_usec < 0))
			return ETIMEDOUT;
		if ((error = itimerfix(&tv)) != 0)
			return error;
		timo = tvtohz(&tv);
	} else
		timo = 0;

	/* Find and lock the sleep queue. */
	wchan = lwp_park_wchan(l->l_proc, SCARG(uap, hint));
	sq = sleeptab_lookup(&lwp_park_tab, wchan);

	/*
	 * Before going the full route and blocking, check to see if an
	 * unpark op is pending.
	 */
	if ((l->l_flag & L_CANCELLED) != 0) {
		sleepq_lwp_lock(l);
		l->l_flag &= ~L_CANCELLED;
		sleepq_lwp_unlock(l);
		sleepq_unlock(sq);
		LWP_COUNT(lwp_ev_park_early, 1);
		return EALREADY;
	}

	/*
	 * For now we ignore the ucontext argument.  In the future, we may
	 * put our stack up to be recycled.  If it's binned, a trampoline
	 * function could call sleepq_unblock() on our behalf.
	 */
	LWP_COUNT(lwp_ev_park, 1);
	sleepq_enter(sq, l);
	sleepq_block(sq, sched_kpri(l), wchan, "parked", timo, 1,
	    &lwp_park_sobj);
	error = sleepq_unblock(timo, 1);
	return error == EWOULDBLOCK ? ETIMEDOUT : error;
}

int
sys__lwp_unpark(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_unpark_args /* {
		syscallarg(lwpid_t)		target;
		syscallarg(const void *)	hint;
	} */ *uap = v;
	struct proc *p;
	struct lwp *t;
	sleepq_t *sq;
	lwpid_t target;
	wchan_t wchan;
	int swapin;

	p = l->l_proc;
	target = SCARG(uap, target);

	/*
	 * Easy case: search for the LWP on the sleep queue.  If
	 * it's parked, remove it from the queue and set running.
	 */
	wchan = lwp_park_wchan(p, SCARG(uap, hint));
	sq = sleeptab_lookup(&lwp_park_tab, wchan);

	TAILQ_FOREACH(t, &sq->sq_queue, l_sleepchain)
		if (t->l_proc == p && t->l_lid == target)
			break;

	if (t == NULL) {
		/*
		 * The LWP hasn't parked yet.  Take the hit
		 * and mark the operation as pending.
		 */
		sleepq_unlock(sq);
		mutex_enter(&p->p_smutex);
		if ((t = lwp_find(p, target)) == NULL) {
			mutex_exit(&p->p_smutex);
			return ESRCH;
		}
		lwp_lock(t);
		mutex_exit(&p->p_smutex);

		if (t->l_sleepq == sq) {
			/*
			 * We have raced, and the LWP is now parked.
			 * Wake it in the usual way.
			 */
			KASSERT(t->l_syncobj == &lwp_park_sobj);
			LOCK_ASSERT(lwp_locked(t, sq->sq_mutex));
			LWP_COUNT(lwp_ev_park_raced, 1);
		} else {
			/*
			 * It many not have parked yet, or is parked
			 * on a different user sync object.  The
			 * latter is an application error.
			 */
			t->l_flag |= L_CANCELLED;
			lwp_unlock(t);
			return 0;
		}
	}

	swapin = sleepq_remove(sq, t);
	sleepq_unlock(sq);
	if (swapin)
		wakeup(&proc0);
	LWP_COUNT(lwp_ev_park_targ, 1);
	return 0;
}

int
sys__lwp_unpark_all(struct lwp *l, void *v, register_t *retval)
{
	struct sys__lwp_unpark_all_args /* {
		syscallarg(const lwpid_t *)	targets;
		syscallarg(size_t)		ntargets;
		syscallarg(const void *)	hint;
	} */ *uap = v;
	struct proc *p;
	struct lwp *t;
	sleepq_t *sq;
	wchan_t wchan;
	lwpid_t targets[32], *tp, *tpp, *tmax, target;
	int swapin, error;
	u_int ntargets, unparked;
	size_t sz;

	p = l->l_proc;
	ntargets = SCARG(uap, ntargets);

	if (SCARG(uap, targets) == NULL) {
		/*
		 * Let the caller know how much we are willing to do, and
		 * let it unpark the LWPs in blocks.
		 */
		*retval = LWP_UNPARK_MAX;
		return 0;
	}
	if (ntargets > LWP_UNPARK_MAX || ntargets == 0)
		return EINVAL;

	/*
	 * Copy in the target array.  If it's a small number of LWPs, then
	 * place the numbers on the stack.
	 */
	sz = sizeof(target) * ntargets;
	if (sz <= sizeof(targets))
		tp = targets;
	else if ((tp = kmem_alloc(sz, KM_SLEEP)) == NULL)
		return ENOMEM;
	error = copyin(SCARG(uap, targets), tp, sz);
	if (error != 0) {
		if (tp != targets)
			kmem_free(tp, sz);
		return error;
	}

	unparked = 0;
	swapin = 0;
	wchan = lwp_park_wchan(p, SCARG(uap, hint));
	sq = sleeptab_lookup(&lwp_park_tab, wchan);

	for (tmax = tp + ntargets, tpp = tp; tpp < tmax; tpp++) {
		target = *tpp;

		/*
		 * Easy case: search for the LWP on the sleep queue.  If
		 * it's parked, remove it from the queue and set running.
		 */
		TAILQ_FOREACH(t, &sq->sq_queue, l_sleepchain)
			if (t->l_proc == p && t->l_lid == target)
				break;

		if (t != NULL) {
			swapin |= sleepq_remove(sq, t);
			unparked++;
			continue;
		}

		/*
		 * The LWP hasn't parked yet.  Take the hit and
		 * mark the operation as pending.
		 */
		sleepq_unlock(sq);
		mutex_enter(&p->p_smutex);
		if ((t = lwp_find(p, target)) == NULL) {
			mutex_exit(&p->p_smutex);
			sleepq_lock(sq);
			continue;
		}
		lwp_lock(t);
		mutex_exit(&p->p_smutex);

		if (t->l_sleepq == sq) {
			/*
			 * We have raced, and the LWP is now parked.
			 * Wake it in the usual way.
			 */
			KASSERT(t->l_syncobj == &lwp_park_sobj);
			LOCK_ASSERT(lwp_locked(t, sq->sq_mutex));
			LWP_COUNT(lwp_ev_park_raced, 1);
			swapin |= sleepq_remove(sq, t);
			unparked++;
		} else {
			/*
			 * It many not have parked yet, or is parked
			 * on a different user sync object.  The
			 * latter is an application error.
			 */
			t->l_flag |= L_CANCELLED;
			lwp_unlock(t);
			sleepq_lock(sq);
		}
	}

	sleepq_unlock(sq);
	if (tp != targets)
		kmem_free(tp, sz);
	if (swapin)
		wakeup(&proc0);
	LWP_COUNT(lwp_ev_park_bcast, unparked);
	LWP_COUNT(lwp_ev_park_miss, (ntargets - unparked));
	/* XXXAD return unparked; */
	return 0;
}
