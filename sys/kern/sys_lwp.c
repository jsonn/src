/*	$NetBSD: sys_lwp.c,v 1.26.6.3 2007/12/27 00:46:10 mjf Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: sys_lwp.c,v 1.26.6.3 2007/12/27 00:46:10 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/sleepq.h>
#include <sys/lwpctl.h>

#include <uvm/uvm_extern.h>

#define	LWP_UNPARK_MAX		1024

syncobj_t lwp_park_sobj = {
	SOBJ_SLEEPQ_LIFO,
	sleepq_unsleep,
	sleepq_changepri,
	sleepq_lendpri,
	syncobj_noowner,
};

sleeptab_t	lwp_park_tab;

void
lwp_sys_init(void)
{
	sleeptab_init(&lwp_park_tab);
}

/* ARGSUSED */
int
sys__lwp_create(struct lwp *l, const struct sys__lwp_create_args *uap, register_t *retval)
{
	/* {
		syscallarg(const ucontext_t *) ucp;
		syscallarg(u_long) flags;
		syscallarg(lwpid_t *) new_lwp;
	} */
	struct proc *p = l->l_proc;
	struct lwp *l2;
	vaddr_t uaddr;
	bool inmem;
	ucontext_t *newuc;
	int error, lid;

	newuc = pool_get(&lwp_uc_pool, PR_WAITOK);

	error = copyin(SCARG(uap, ucp), newuc, p->p_emul->e_ucsize);
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

	error = lwp_create(l, p, uaddr, inmem, SCARG(uap, flags) & LWP_DETACHED,
	    NULL, 0, p->p_emul->e_startlwp, newuc, &l2, l->l_class);
	if (error) {
		uvm_uarea_free(uaddr, curcpu());
		pool_put(&lwp_uc_pool, newuc);
		return error;
	}

	lid = l2->l_lid;
	error = copyout(&lid, SCARG(uap, new_lwp), sizeof(lid));
	if (error) {
		lwp_exit(l2);
		pool_put(&lwp_uc_pool, newuc);
		return error;
	}

	/*
	 * Set the new LWP running, unless the caller has requested that
	 * it be created in suspended state.  If the process is stopping,
	 * then the LWP is created stopped.
	 */
	mutex_enter(&p->p_smutex);
	lwp_lock(l2);
	if ((SCARG(uap, flags) & LWP_SUSPENDED) == 0 &&
	    (l->l_flag & (LW_WREBOOT | LW_WSUSPEND | LW_WEXIT)) == 0) {
	    	if (p->p_stat == SSTOP || (p->p_sflag & PS_STOPPING) != 0)
	    		l2->l_stat = LSSTOP;
		else {
			KASSERT(lwp_locked(l2, l2->l_cpu->ci_schedstate.spc_mutex));
			p->p_nrlwps++;
			l2->l_stat = LSRUN;
			sched_enqueue(l2, false);
		}
		lwp_unlock(l2);
	} else {
		l2->l_stat = LSSUSPENDED;
		lwp_unlock_to(l2, &l2->l_cpu->ci_schedstate.spc_lwplock);
	}
	mutex_exit(&p->p_smutex);

	return 0;
}

int
sys__lwp_exit(struct lwp *l, const void *v, register_t *retval)
{

	lwp_exit(l);
	return 0;
}

int
sys__lwp_self(struct lwp *l, const void *v, register_t *retval)
{

	*retval = l->l_lid;
	return 0;
}

int
sys__lwp_getprivate(struct lwp *l, const void *v, register_t *retval)
{

	*retval = (uintptr_t)l->l_private;
	return 0;
}

int
sys__lwp_setprivate(struct lwp *l, const struct sys__lwp_setprivate_args *uap, register_t *retval)
{
	/* {
		syscallarg(void *) ptr;
	} */

	l->l_private = SCARG(uap, ptr);
	return 0;
}

int
sys__lwp_suspend(struct lwp *l, const struct sys__lwp_suspend_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t) target;
	} */
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
	    (l->l_flag & (LW_WCORE | LW_WEXIT)) != 0) {
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
	if (error) {
		mutex_exit(&p->p_smutex);
		return error;
	}

	/*
	 * Wait for:
	 *  o process exiting
	 *  o target LWP suspended
	 *  o target LWP not suspended and L_WSUSPEND clear
	 *  o target LWP exited
	 */
	for (;;) {
		error = cv_wait_sig(&p->p_lwpcv, &p->p_smutex);
		if (error) {
			error = ERESTART;
			break;
		}
		if (lwp_find(p, SCARG(uap, target)) == NULL) {
			error = ESRCH;
			break;
		}
		if ((l->l_flag | t->l_flag) & (LW_WCORE | LW_WEXIT)) {
			error = ERESTART;
			break;
		}
		if (t->l_stat == LSSUSPENDED ||
		    (t->l_flag & LW_WSUSPEND) == 0)
			break;
	}
	mutex_exit(&p->p_smutex);

	return error;
}

int
sys__lwp_continue(struct lwp *l, const struct sys__lwp_continue_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t) target;
	} */
	int error;
	struct proc *p = l->l_proc;
	struct lwp *t;

	error = 0;

	mutex_enter(&p->p_smutex);
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
sys__lwp_wakeup(struct lwp *l, const struct sys__lwp_wakeup_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t) target;
	} */
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
	t->l_flag |= (LW_CANCELLED | LW_UNPARKED);

	if (t->l_stat != LSSLEEP) {
		lwp_unlock(t);
		error = ENODEV;
	} else if ((t->l_flag & LW_SINTR) == 0) {
		lwp_unlock(t);
		error = EBUSY;
	} else {
		/* Wake it up.  lwp_unsleep() will release the LWP lock. */
		lwp_unsleep(t);
		error = 0;
	}

	mutex_exit(&p->p_smutex);

	return error;
}

int
sys__lwp_wait(struct lwp *l, const struct sys__lwp_wait_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t) wait_for;
		syscallarg(lwpid_t *) departed;
	} */
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
sys__lwp_kill(struct lwp *l, const struct sys__lwp_kill_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t)	target;
		syscallarg(int)		signo;
	} */
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
sys__lwp_detach(struct lwp *l, const struct sys__lwp_detach_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t)	target;
	} */
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
				/* Releases proc mutex. */
				lwp_free(t, false, false);
				return 0;
			}
			error = 0;

			/*
			 * Have any LWPs sleeping in lwp_wait() recheck
			 * for deadlock.
			 */
			cv_broadcast(&p->p_lwpcv);
		} else
			error = EINVAL;
	} else
		error = ESRCH;

	mutex_exit(&p->p_smutex);

	return error;
}

static inline wchan_t
lwp_park_wchan(struct proc *p, const void *hint)
{

	return (wchan_t)((uintptr_t)p ^ (uintptr_t)hint);
}

int
lwp_unpark(lwpid_t target, const void *hint)
{
	sleepq_t *sq;
	wchan_t wchan;
	int swapin;
	proc_t *p;
	lwp_t *t;

	/*
	 * Easy case: search for the LWP on the sleep queue.  If
	 * it's parked, remove it from the queue and set running.
	 */
	p = curproc;
	wchan = lwp_park_wchan(p, hint);
	sq = sleeptab_lookup(&lwp_park_tab, wchan);

	TAILQ_FOREACH(t, &sq->sq_queue, l_sleepchain)
		if (t->l_proc == p && t->l_lid == target)
			break;

	if (__predict_true(t != NULL)) {
		swapin = sleepq_remove(sq, t);
		sleepq_unlock(sq);
		if (swapin)
			uvm_kick_scheduler();
		return 0;
	}

	/*
	 * The LWP hasn't parked yet.  Take the hit and mark the
	 * operation as pending.
	 */
	sleepq_unlock(sq);

	mutex_enter(&p->p_smutex);
	if ((t = lwp_find(p, target)) == NULL) {
		mutex_exit(&p->p_smutex);
		return ESRCH;
	}

	/*
	 * It may not have parked yet, we may have raced, or it
	 * is parked on a different user sync object.
	 */
	lwp_lock(t);
	if (t->l_syncobj == &lwp_park_sobj) {
		/* Releases the LWP lock. */
		lwp_unsleep(t);
	} else {
		/*
		 * Set the operation pending.  The next call to _lwp_park
		 * will return early.
		 */
		t->l_flag |= LW_UNPARKED;
		lwp_unlock(t);
	}

	mutex_exit(&p->p_smutex);
	return 0;
}

int
lwp_park(struct timespec *ts, const void *hint)
{
	struct timespec tsx;
	sleepq_t *sq;
	wchan_t wchan;
	int timo, error;
	lwp_t *l;

	/* Fix up the given timeout value. */
	if (ts != NULL) {
		getnanotime(&tsx);
		timespecsub(ts, &tsx, &tsx);
		if (tsx.tv_sec < 0 || (tsx.tv_sec == 0 && tsx.tv_nsec <= 0))
			return ETIMEDOUT;
		if ((error = itimespecfix(&tsx)) != 0)
			return error;
		timo = tstohz(&tsx);
		KASSERT(timo != 0);
	} else
		timo = 0;

	/* Find and lock the sleep queue. */
	l = curlwp;
	wchan = lwp_park_wchan(l->l_proc, hint);
	sq = sleeptab_lookup(&lwp_park_tab, wchan);

	/*
	 * Before going the full route and blocking, check to see if an
	 * unpark op is pending.
	 */
	lwp_lock(l);
	if ((l->l_flag & (LW_CANCELLED | LW_UNPARKED)) != 0) {
		l->l_flag &= ~(LW_CANCELLED | LW_UNPARKED);
		lwp_unlock(l);
		sleepq_unlock(sq);
		return EALREADY;
	}
	lwp_unlock_to(l, sq->sq_mutex);
	l->l_biglocks = 0;
	sleepq_enqueue(sq, wchan, "parked", &lwp_park_sobj);
	error = sleepq_block(timo, true);
	switch (error) {
	case EWOULDBLOCK:
		error = ETIMEDOUT;
		break;
	case ERESTART:
		error = EINTR;
		break;
	default:
		/* nothing */
		break;
	}
	return error;
}

/*
 * 'park' an LWP waiting on a user-level synchronisation object.  The LWP
 * will remain parked until another LWP in the same process calls in and
 * requests that it be unparked.
 */
int
sys__lwp_park(struct lwp *l, const struct sys__lwp_park_args *uap, register_t *retval)
{
	/* {
		syscallarg(const struct timespec *)	ts;
		syscallarg(lwpid_t)			unpark;
		syscallarg(const void *)		hint;
		syscallarg(const void *)		unparkhint;
	} */
	struct timespec ts, *tsp;
	int error;

	if (SCARG(uap, ts) == NULL)
		tsp = NULL;
	else {
		error = copyin(SCARG(uap, ts), &ts, sizeof(ts));
		if (error != 0)
			return error;
		tsp = &ts;
	}

	if (SCARG(uap, unpark) != 0) {
		error = lwp_unpark(SCARG(uap, unpark), SCARG(uap, unparkhint));
		if (error != 0)
			return error;
	}

	return lwp_park(tsp, SCARG(uap, hint));
}

int
sys__lwp_unpark(struct lwp *l, const struct sys__lwp_unpark_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t)		target;
		syscallarg(const void *)	hint;
	} */

	return lwp_unpark(SCARG(uap, target), SCARG(uap, hint));
}

int
sys__lwp_unpark_all(struct lwp *l, const struct sys__lwp_unpark_all_args *uap, register_t *retval)
{
	/* {
		syscallarg(const lwpid_t *)	targets;
		syscallarg(size_t)		ntargets;
		syscallarg(const void *)	hint;
	} */
	struct proc *p;
	struct lwp *t;
	sleepq_t *sq;
	wchan_t wchan;
	lwpid_t targets[32], *tp, *tpp, *tmax, target;
	int swapin, error;
	u_int ntargets;
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
	else {
		KERNEL_LOCK(1, l);		/* XXXSMP */
		tp = kmem_alloc(sz, KM_SLEEP);
		KERNEL_UNLOCK_ONE(l);		/* XXXSMP */
		if (tp == NULL)
			return ENOMEM;
	}
	error = copyin(SCARG(uap, targets), tp, sz);
	if (error != 0) {
		if (tp != targets) {
			KERNEL_LOCK(1, l);	/* XXXSMP */
			kmem_free(tp, sz);
			KERNEL_UNLOCK_ONE(l);	/* XXXSMP */
		}
		return error;
	}

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

		/*
		 * It may not have parked yet, we may have raced, or
		 * it is parked on a different user sync object.
		 */
		if (t->l_syncobj == &lwp_park_sobj) {
			/* Releases the LWP lock. */
			lwp_unsleep(t);
		} else {
			/*
			 * Set the operation pending.  The next call to
			 * _lwp_park will return early.
			 */
			t->l_flag |= LW_UNPARKED;
			lwp_unlock(t);
		}

		mutex_exit(&p->p_smutex);
		sleepq_lock(sq);
	}

	sleepq_unlock(sq);
	if (tp != targets) {
		KERNEL_LOCK(1, l);		/* XXXSMP */
		kmem_free(tp, sz);
		KERNEL_UNLOCK_ONE(l);		/* XXXSMP */
	}
	if (swapin)
		uvm_kick_scheduler();

	return 0;
}

int
sys__lwp_setname(struct lwp *l, const struct sys__lwp_setname_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t)		target;
		syscallarg(const char *)	name;
	} */
	char *name, *oname;
	lwpid_t target;
	proc_t *p;
	lwp_t *t;
	int error;

	if ((target = SCARG(uap, target)) == 0)
		target = l->l_lid;

	name = kmem_alloc(MAXCOMLEN, KM_SLEEP);
	if (name == NULL)
		return ENOMEM;
	error = copyinstr(SCARG(uap, name), name, MAXCOMLEN, NULL);
	switch (error) {
	case ENAMETOOLONG:
	case 0:
		name[MAXCOMLEN - 1] = '\0';
		break;
	default:
		kmem_free(name, MAXCOMLEN);
		return error;
	}

	p = curproc;
	mutex_enter(&p->p_smutex);
	if ((t = lwp_find(p, target)) == NULL) {
		mutex_exit(&p->p_smutex);
		kmem_free(name, MAXCOMLEN);
		return ESRCH;
	}
	lwp_lock(t);
	oname = t->l_name;
	t->l_name = name;
	lwp_unlock(t);
	mutex_exit(&p->p_smutex);

	if (oname != NULL)
		kmem_free(oname, MAXCOMLEN);

	return 0;
}

int
sys__lwp_getname(struct lwp *l, const struct sys__lwp_getname_args *uap, register_t *retval)
{
	/* {
		syscallarg(lwpid_t)		target;
		syscallarg(char *)		name;
		syscallarg(size_t)		len;
	} */
	char name[MAXCOMLEN];
	lwpid_t target;
	proc_t *p;
	lwp_t *t;

	if ((target = SCARG(uap, target)) == 0)
		target = l->l_lid;

	p = curproc;
	mutex_enter(&p->p_smutex);
	if ((t = lwp_find(p, target)) == NULL) {
		mutex_exit(&p->p_smutex);
		return ESRCH;
	}
	lwp_lock(t);
	if (t->l_name == NULL)
		name[0] = '\0';
	else
		strcpy(name, t->l_name);
	lwp_unlock(t);
	mutex_exit(&p->p_smutex);

	return copyoutstr(name, SCARG(uap, name), SCARG(uap, len), NULL);
}

int
sys__lwp_ctl(struct lwp *l, const struct sys__lwp_ctl_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			features;
		syscallarg(struct lwpctl **)	address;
	} */
	int error, features;
	vaddr_t vaddr;

	features = SCARG(uap, features);
	if ((features & ~LWPCTL_FEATURE_CURCPU) != 0)
		return ENODEV;
	if ((error = lwp_ctl_alloc(&vaddr)) != 0)
		return error;
	return copyout(&vaddr, SCARG(uap, address), sizeof(void *));
}
