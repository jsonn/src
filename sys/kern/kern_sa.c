/*	$NetBSD: kern_sa.c,v 1.83.4.9 2007/01/25 20:18:37 ad Exp $	*/

/*-
 * Copyright (c) 2001, 2004, 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams, and by Andrew Doran.
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

#include <sys/cdefs.h>

#include "opt_ktrace.h"
#include "opt_multiprocessor.h"
__KERNEL_RCSID(0, "$NetBSD: kern_sa.c,v 1.83.4.9 2007/01/25 20:18:37 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/savar.h>
#include <sys/syscallargs.h>
#include <sys/ktrace.h>
#include <sys/sleepq.h>

#include <uvm/uvm_extern.h>

static POOL_INIT(sadata_pool, sizeof(struct sadata), 0, 0, 0, "sadatapl",
    &pool_allocator_nointr); /* memory pool for sadata structures */
static POOL_INIT(saupcall_pool, sizeof(struct sadata_upcall), 0, 0, 0,
    "saupcpl", &pool_allocator_nointr); /* memory pool for pending upcalls */
static POOL_INIT(sastack_pool, sizeof(struct sastack), 0, 0, 0, "sastackpl",
    &pool_allocator_nointr); /* memory pool for sastack structs */
static POOL_INIT(savp_pool, sizeof(struct sadata_vp), 0, 0, 0, "savppl",
    &pool_allocator_nointr); /* memory pool for sadata_vp structures */

static struct sadata_vp *sa_newsavp(struct proc *);
static inline int sa_stackused(struct sastack *, struct sadata *);
static inline void sa_setstackfree(struct sastack *, struct sadata *);
static struct sastack *sa_getstack(struct sadata *);
static inline struct sastack *sa_getstack0(struct sadata *);
static inline int sast_compare(struct sastack *, struct sastack *);
#ifdef MULTIPROCESSOR
static int sa_increaseconcurrency(struct lwp *, int);
#endif
static void sa_setwoken(struct lwp *);
static void sa_switchcall(void *);
static int sa_newcachelwp(struct lwp *);
static void sa_makeupcalls(struct lwp *, struct sadata_upcall *);
static struct lwp *sa_vp_repossess(struct lwp *l);

static inline int sa_pagefault(struct lwp *, ucontext_t *);

static void sa_upcall0(struct sadata_upcall *, int, struct lwp *, struct lwp *,
    size_t, void *, void (*)(void *));
static void sa_upcall_getstate(union sau_state *, struct lwp *);

#define SA_DEBUG

#ifdef SA_DEBUG
#define DPRINTF(x)	do { if (sadebug) printf_nolog x; } while (0)
#define DPRINTFN(n,x)	do { if (sadebug & (1<<(n-1))) printf_nolog x; } while (0)
int	sadebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


#define SA_LWP_STATE_LOCK(l, f) do {				\
	(f) = (l)->l_flag;     					\
	(l)->l_flag &= ~L_SA;					\
} while (/*CONSTCOND*/ 0)

#define SA_LWP_STATE_UNLOCK(l, f) do {				\
	(l)->l_flag |= (f) & L_SA;				\
} while (/*CONSTCOND*/ 0)

RB_PROTOTYPE(sasttree, sastack, sast_node, sast_compare);
RB_GENERATE(sasttree, sastack, sast_node, sast_compare);

kmutex_t	saupcall_mutex;
SIMPLEQ_HEAD(, sadata_upcall) saupcall_freelist;

/*
 * sadata_upcall_alloc:
 *
 *	Allocate an sadata_upcall structure.
 */
struct sadata_upcall *
sadata_upcall_alloc(int waitok)
{
	struct sadata_upcall *sau;

	sau = NULL;
	if (waitok && !SIMPLEQ_EMPTY(&saupcall_freelist)) {
		mutex_enter(&saupcall_mutex);
		if ((sau = SIMPLEQ_FIRST(&saupcall_freelist)) != NULL)
			SIMPLEQ_REMOVE_HEAD(&saupcall_freelist, sau_next);
		mutex_exit(&saupcall_mutex);
		if (sau != NULL && sau->sau_arg != NULL)
			(*sau->sau_argfreefunc)(sau->sau_arg);
	}

	if (sau == NULL)
		sau = pool_get(&saupcall_pool, waitok ? PR_WAITOK : PR_NOWAIT);
	if (sau != NULL)
		sau->sau_arg = NULL;

	return sau;
}

/*
 * sadata_upcall_free:
 *
 *	Free an sadata_upcall structure and any associated argument data.
 */
void
sadata_upcall_free(struct sadata_upcall *sau)
{
	if (sau == NULL)
		return;

	/*
	 * If our current synchronisation object is a sleep queue or
	 * similar, we must not put the object back to the pool as
	 * doing to could acquire sleep locks.  That could trigger
	 * a recursive sleep.
	 */
	if (curlwp->l_syncobj == &sched_syncobj) {
		if (sau->sau_arg)
			(*sau->sau_argfreefunc)(sau->sau_arg);
		pool_put(&saupcall_pool, sau);
		sadata_upcall_drain();
	} else {
		mutex_enter(&saupcall_mutex);
		SIMPLEQ_INSERT_HEAD(&saupcall_freelist, sau, sau_next);
		mutex_exit(&saupcall_mutex);
	}
}

/*
 * sadata_upcall_drain:
 *
 *	Put freed upcall structures back to the pool.
 */
void
sadata_upcall_drain(void)
{
	struct sadata_upcall *sau;

	sau = SIMPLEQ_FIRST(&saupcall_freelist);
	while (sau != NULL) {
		mutex_enter(&saupcall_mutex);
		if ((sau = SIMPLEQ_FIRST(&saupcall_freelist)) != NULL)
			SIMPLEQ_REMOVE_HEAD(&saupcall_freelist, sau_next);
		mutex_exit(&saupcall_mutex);
		if (sau != NULL)
			pool_put(&saupcall_pool, sau);
	}
}

static struct sadata_vp *
sa_newsavp(struct proc *p)
{
	struct sadata *sa = p->p_sa;
	struct sadata_vp *vp, *qvp;

	/* Allocate virtual processor data structure */
	vp = pool_get(&savp_pool, PR_WAITOK);
	/* Initialize. */
	memset(vp, 0, sizeof(*vp));
	vp->savp_lwp = NULL;
	vp->savp_wokenq_head = NULL;
	vp->savp_faultaddr = 0;
	vp->savp_ofaultaddr = 0;
	LIST_INIT(&vp->savp_lwpcache);
	vp->savp_ncached = 0;
	SIMPLEQ_INIT(&vp->savp_upcalls);

	mutex_enter(&p->p_smutex);
	/* find first free savp_id and add vp to sorted slist */
	if (SLIST_EMPTY(&sa->sa_vps) ||
	    SLIST_FIRST(&sa->sa_vps)->savp_id != 0) {
		vp->savp_id = 0;
		SLIST_INSERT_HEAD(&sa->sa_vps, vp, savp_next);
	} else {
		SLIST_FOREACH(qvp, &sa->sa_vps, savp_next) {
			if (SLIST_NEXT(qvp, savp_next) == NULL ||
			    SLIST_NEXT(qvp, savp_next)->savp_id !=
			    qvp->savp_id + 1)
				break;
		}
		vp->savp_id = qvp->savp_id + 1;
		SLIST_INSERT_AFTER(qvp, vp, savp_next);
	}
	mutex_exit(&p->p_smutex);

	return (vp);
}

int
sys_sa_register(struct lwp *l, void *v, register_t *retval)
{
#if 0
	struct sys_sa_register_args /* {
		syscallarg(sa_upcall_t) new;
		syscallarg(sa_upcall_t *) old;
		syscallarg(int) flags;
		syscallarg(ssize_t) stackinfo_offset;
	} */ *uap = v;
	int error;
	sa_upcall_t prev;

	error = dosa_register(l, SCARG(uap, new), &prev, SCARG(uap, flags),
	    SCARG(uap, stackinfo_offset));
	if (error)
		return error;

	if (SCARG(uap, old))
		return copyout(&prev, SCARG(uap, old),
		    sizeof(prev));
	return 0;
#else
	return ENOSYS;
#endif
}

int
dosa_register(struct lwp *l, sa_upcall_t new, sa_upcall_t *prev, int flags,
    ssize_t stackinfo_offset)
{
	struct proc *p = l->l_proc;
	struct sadata *sa;

	if (p->p_sa == NULL) {
		/* Allocate scheduler activations data structure */
		sa = pool_get(&sadata_pool, PR_WAITOK);

		mutex_init(&sa->sa_mutex, MUTEX_DEFAULT, IPL_NONE);
		mutex_enter(&p->p_smutex);
		if ((p->p_sflag & PS_NOSA) != 0) {
			mutex_exit(&p->p_smutex);
			mutex_destroy(&sa->sa_mutex);
			pool_put(&sadata_pool, sa);
			return EINVAL;
		}

		/* Initialize. */
		memset(sa, 0, sizeof(*sa));
		sa->sa_flag = flags & SA_FLAG_ALL;
		sa->sa_maxconcurrency = 1;
		sa->sa_concurrency = 1;
		RB_INIT(&sa->sa_stackstree);
		sa->sa_stacknext = NULL;
		if (flags & SA_FLAG_STACKINFO)
			sa->sa_stackinfo_offset = stackinfo_offset;
		else
			sa->sa_stackinfo_offset = 0;
		sa->sa_nstacks = 0;
		SLIST_INIT(&sa->sa_vps);
		mb_write();
		p->p_sa = sa;
		KASSERT(l->l_savp == NULL);
		mutex_exit(&p->p_smutex);
	}
	if (l->l_savp == NULL) {	/* XXXSMP */
		l->l_savp = sa_newsavp(p);
		sa_newcachelwp(l);
	}

	*prev = p->p_sa->sa_upcall;
	p->p_sa->sa_upcall = new;

	return (0);
}

void
sa_release(struct proc *p)
{
	struct sadata *sa;
	struct sastack *sast, *next;
	struct sadata_vp *vp;
	struct lwp *l;

	sa = p->p_sa;
	KDASSERT(sa != NULL);
	KASSERT(p->p_nlwps <= 1);

	for (sast = RB_MIN(sasttree, &sa->sa_stackstree); sast != NULL;
	     sast = next) {
		next = RB_NEXT(sasttree, &sa->sa_stackstree, sast);
		RB_REMOVE(sasttree, &sa->sa_stackstree, sast);
		pool_put(&sastack_pool, sast);
	}

	mutex_enter(&p->p_smutex);
	p->p_sflag = (p->p_sflag & ~PS_SA) | PS_NOSA;
	p->p_sa = NULL;
	l = LIST_FIRST(&p->p_lwps);
	if (l) {
		lwp_lock(l);
		KASSERT(LIST_NEXT(l, l_sibling) == NULL);
		l->l_savp = NULL;
		lwp_unlock(l);
	}
	mutex_exit(&p->p_smutex);

	while ((vp = SLIST_FIRST(&p->p_sa->sa_vps)) != NULL) {
		SLIST_REMOVE_HEAD(&p->p_sa->sa_vps, savp_next);
		pool_put(&savp_pool, vp);
	}

	mutex_destroy(&sa->sa_mutex);
	pool_put(&sadata_pool, sa);

	mutex_enter(&p->p_smutex);
	p->p_sflag &= ~PS_NOSA;
	mutex_exit(&p->p_smutex);
}

static int
sa_fetchstackgen(struct sastack *sast, struct sadata *sa, unsigned int *gen)
{
	int error;

	/* COMPAT_NETBSD32:  believe it or not, but the following is ok */
	mutex_exit(&sa->sa_mutex);
	error = copyin(&((struct sa_stackinfo_t *)
	    ((char *)sast->sast_stack.ss_sp +
	    sa->sa_stackinfo_offset))->sasi_stackgen, gen, sizeof(*gen));
	mutex_enter(&sa->sa_mutex);

	return error;
}

static inline int
sa_stackused(struct sastack *sast, struct sadata *sa)
{
	unsigned int gen;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));

	if (sa_fetchstackgen(sast, sa, &gen)) {
#ifdef DIAGNOSTIC
		printf("sa_stackused: couldn't copyin sasi_stackgen");
#endif
		sigexit(curlwp, SIGILL);
		/* NOTREACHED */
	}
	return (sast->sast_gen != gen);
}

static inline void
sa_setstackfree(struct sastack *sast, struct sadata *sa)
{
	unsigned int gen;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));

	if (sa_fetchstackgen(sast, sa, &gen)) {
#ifdef DIAGNOSTIC
		printf("sa_setstackfree: couldn't copyin sasi_stackgen");
#endif
		sigexit(curlwp, SIGILL);
		/* NOTREACHED */
	}
	sast->sast_gen = gen;
}

/*
 * Find next free stack, starting at sa->sa_stacknext.  Must be called
 * with sa->sa_mutex held, and will release while checking for stack
 * availability.
 */
static struct sastack *
sa_getstack(struct sadata *sa)
{
	struct sastack *sast;
	int chg;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));

	do {
		chg = sa->sa_stackchg;
		sast = sa->sa_stacknext;
		if (sast == NULL || sa_stackused(sast, sa))
			sast = sa_getstack0(sa);
	} while (chg != sa->sa_stackchg);

	if (sast == NULL)
		return NULL;

	sast->sast_gen++;
	sa->sa_stackchg++;

	return sast;
}

static inline struct sastack *
sa_getstack0(struct sadata *sa)
{
	struct sastack *start;
	int chg;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));

 retry:
	chg = sa->sa_stackchg;
	if (sa->sa_stacknext == NULL) {
		sa->sa_stacknext = RB_MIN(sasttree, &sa->sa_stackstree);
		if (sa->sa_stacknext == NULL)
			return NULL;
	}
	start = sa->sa_stacknext;

	while (sa_stackused(sa->sa_stacknext, sa)) {
		if (sa->sa_stackchg != chg)
			goto retry;
		sa->sa_stacknext = RB_NEXT(sasttree, &sa->sa_stackstree,
		    sa->sa_stacknext);
		if (sa->sa_stacknext == NULL)
			sa->sa_stacknext = RB_MIN(sasttree,
			    &sa->sa_stackstree);
		if (sa->sa_stacknext == start)
			return NULL;
	}
	return sa->sa_stacknext;
}

static inline int
sast_compare(struct sastack *a, struct sastack *b)
{
	if ((vaddr_t)a->sast_stack.ss_sp + a->sast_stack.ss_size <=
	    (vaddr_t)b->sast_stack.ss_sp)
		return (-1);
	if ((vaddr_t)a->sast_stack.ss_sp >=
	    (vaddr_t)b->sast_stack.ss_sp + b->sast_stack.ss_size)
		return (1);
	return (0);
}

static int
sa_copyin_stack(stack_t *stacks, int index, stack_t *dest)
{
	return copyin(stacks + index, dest, sizeof(stack_t));
}

int
sys_sa_stacks(struct lwp *l, void *v, register_t *retval)
{
	struct sys_sa_stacks_args /* {
		syscallarg(int) num;
		syscallarg(stack_t *) stacks;
	} */ *uap = v;

	return sa_stacks1(l, retval, SCARG(uap, num), SCARG(uap, stacks), sa_copyin_stack);
}

int
sa_stacks1(struct lwp *l, register_t *retval, int num, stack_t *stacks,
    sa_copyin_stack_t do_sa_copyin_stack)
{
	struct sadata *sa = l->l_proc->p_sa;
	struct sastack *sast, *new;
	int count, error, f, i, chg;

	/* We have to be using scheduler activations */
	if (sa == NULL)
		return (EINVAL);

	count = num;
	if (count < 0)
		return (EINVAL);

	SA_LWP_STATE_LOCK(l, f);

	error = 0;

	for (i = 0; i < count; i++) {
		new = pool_get(&sastack_pool, PR_WAITOK);
		error = do_sa_copyin_stack(stacks, i, &new->sast_stack);
		if (error) {
			count = i;
			break;
		}
		mutex_enter(&sa->sa_mutex);
	 restart:
	 	chg = sa->sa_stackchg;
		sa_setstackfree(new, sa);
		sast = RB_FIND(sasttree, &sa->sa_stackstree, new);
		if (sast != NULL) {
			DPRINTFN(9, ("sa_stacks(%d.%d) returning stack %p\n",
				     l->l_proc->p_pid, l->l_lid,
				     new->sast_stack.ss_sp));
			if (sa_stackused(sast, sa) == 0) {
				count = i;
				error = EEXIST;
				mutex_exit(&sa->sa_mutex);
				pool_put(&sastack_pool, new);
				break;
			}
			if (chg != sa->sa_stackchg)
				goto restart;
		} else if (sa->sa_nstacks >=
		    SA_MAXNUMSTACKS * sa->sa_concurrency) {
			DPRINTFN(9,
			    ("sa_stacks(%d.%d) already using %d stacks\n",
			    l->l_proc->p_pid, l->l_lid,
			    SA_MAXNUMSTACKS * sa->sa_concurrency));
			count = i;
			error = ENOMEM;
			mutex_exit(&sa->sa_mutex);
			pool_put(&sastack_pool, new);
			break;
		} else {
			DPRINTFN(9, ("sa_stacks(%d.%d) adding stack %p\n",
				     l->l_proc->p_pid, l->l_lid,
				     new->sast_stack.ss_sp));
			RB_INSERT(sasttree, &sa->sa_stackstree, new);
			sa->sa_nstacks++;
			sa->sa_stackchg++;
		}
		mutex_exit(&sa->sa_mutex);
	}

	SA_LWP_STATE_UNLOCK(l, f);

	*retval = count;
	return (error);
}


int
sys_sa_enable(struct lwp *l, void *v, register_t *retval)
{
#if 0
	struct proc *p = l->l_proc;
	struct sadata *sa = p->p_sa;
	struct sadata_vp *vp = l->l_savp;
	int error;

	KASSERT((p->p_sflag & PS_NOSA) == 0);

	DPRINTF(("sys_sa_enable(%d.%d)\n", l->l_proc->p_pid,
	    l->l_lid));

	/* We have to be using scheduler activations */
	if (sa == NULL || vp == NULL)
		return (EINVAL);

	if (p->p_sflag & PS_SA) /* Already running! */
		return (EBUSY);

	error = sa_upcall(l, SA_UPCALL_NEWPROC, l, NULL, 0, NULL, NULL);
	if (error)
		return (error);

	/* Assign this LWP to the virtual processor */
	mutex_enter(&p->p_smutex);
	vp->savp_lwp = l;
	p->p_sflag |= PS_SA;
	lwp_lock(l);
	l->l_flag |= L_SA; /* We are now an activation LWP */
	lwp_unlock(l);
	mutex_exit(&p->p_smutex);

	/* This will not return to the place in user space it came from. */
	return (0);
#else
	return ENOSYS;
#endif
}


#ifdef MULTIPROCESSOR
static int
sa_increaseconcurrency(struct lwp *l, int concurrency)
{
	struct proc *p;
	struct lwp *l2;
	struct sadata *sa;
	struct sadata_vp *vp;
	vaddr_t uaddr;
	boolean_t inmem;
	int addedconcurrency, error;

	p = l->l_proc;
	sa = p->p_sa;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));

	addedconcurrency = 0;
	while (sa->sa_maxconcurrency < concurrency) {
		sa->sa_maxconcurrency++;
		sa->sa_concurrency++;
		mutex_exit(&sa->sa_mutex);

		inmem = uvm_uarea_alloc(&uaddr);
		if (__predict_false(uaddr == 0)) {
			/* reset concurrency */
			mutex_enter(&sa->sa_mutex);
			sa->sa_maxconcurrency--;
			sa->sa_concurrency--;
			return (addedconcurrency);
		} else {
			newlwp(l, p, uaddr, inmem, 0, NULL, 0,
			    child_return, 0, &l2);
			vp = sa_newsavp(p);
			mutex_enter(&sa->sa_mutex);
			lwp_lock(l2);
			l2->l_flag |= L_SA;
			l2->l_savp = vp;
			if (vp) {
				vp->savp_lwp = l2;
				cpu_setfunc(l2, sa_switchcall, NULL);
				lwp_unlock(l2);
				mutex_exit(&p->p_smutex);
				error = sa_upcall(l2, SA_UPCALL_NEWPROC,
				    NULL, NULL, 0, NULL, NULL);
				if (error) {
					/* free new savp */
					mutex_enter(&p->p_smutex);
					SLIST_REMOVE(&sa->sa_vps, l2->l_savp,
					    sadata_vp, savp_next);
					mutex_exit(&p->p_smutex);
					pool_put(&savp_pool, l2->l_savp);
					mutex_enter(&p->p_smutex);
					lwp_lock(l2);
				}
			} else
				error = ENOMEM;

			if (error) {
				/* put l2 into l's LWP cache */
				l2->l_savp = l->l_savp;
				PHOLD(l2);
				lwp_unlock(l2);
				mutex_enter(&p->p_smutex);
				sa_putcachelwp(p, l2);
				mutex_exit(&p->p_smutex);
				/* reset concurrency */
				sa->sa_maxconcurrency--;
				sa->sa_concurrency--;
				return (addedconcurrency);
			}
			/* setrunnable() will unlock l2. */
			setrunnable(l2);
			addedconcurrency++;
		}
	}

	return (addedconcurrency);
}
#endif

int
sys_sa_setconcurrency(struct lwp *l, void *v, register_t *retval)
{
	struct sys_sa_setconcurrency_args /* {
		syscallarg(int) concurrency;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct sadata *sa = p->p_sa;
#ifdef MULTIPROCESSOR
	struct sadata_vp *vp = l->l_savp;
	struct lwp *l2;
	int ncpus;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
#endif

	DPRINTFN(11,("sys_sa_concurrency(%d.%d)\n", p->p_pid,
		     l->l_lid));

	/* We have to be using scheduler activations */
	if (sa == NULL)
		return (EINVAL);

	if ((p->p_sflag & PS_SA) == 0)
		return (EINVAL);

	if (SCARG(uap, concurrency) < 1)
		return (EINVAL);

	*retval = 0;
	/*
	 * Concurrency greater than the number of physical CPUs does
	 * not make sense.
	 * XXX Should we ever support hot-plug CPUs, this will need
	 * adjustment.
	 */
#ifdef MULTIPROCESSOR
	mutex_enter(&sa->sa_mutex);

	if (SCARG(uap, concurrency) > sa->sa_maxconcurrency) {
		ncpus = 0;
		for (CPU_INFO_FOREACH(cii, ci))
			ncpus++;
		*retval += sa_increaseconcurrency(l,
		    min(SCARG(uap, concurrency), ncpus));
	}
#endif

	DPRINTFN(11,("sys_sa_concurrency(%d.%d) want %d, have %d, max %d\n",
		     p->p_pid, l->l_lid, SCARG(uap, concurrency),
		     sa->sa_concurrency, sa->sa_maxconcurrency));
#ifdef MULTIPROCESSOR
	if (SCARG(uap, concurrency) > sa->sa_concurrency) {
		mutex_enter(&p->p_smutex);
		SLIST_FOREACH(vp, &sa->sa_vps, savp_next) {
			l2 = vp->savp_lwp;
			lwp_lock(l2);
			if (l2->l_flag & L_SA_IDLE) {
				l2->l_flag &=
					~(L_SA_IDLE|L_SA_YIELD|L_SINTR);
				lwp_unlock(l2);
				DPRINTFN(11,("sys_sa_concurrency(%d.%d) "
					     "NEWPROC vp %d\n",
					     p->p_pid, l->l_lid,
					     vp->savp_id));
				cpu_setfunc(l2, sa_switchcall, NULL);
				sa->sa_concurrency++;
				mutex_exit(&p->p_smutex);
				mutex_exit(&sa->sa_mutex);
				/* error = */ sa_upcall(l2,
				    SA_UPCALL_NEWPROC,
				    NULL, NULL, 0, NULL, NULL);
				lwp_lock(l2);
				/* setrunnable() will unlock l2 */
				setrunnable(l2);
				KDASSERT((l2->l_flag & L_SINTR) == 0);
				(*retval)++;
				/* XXXAD holding sa_mutex, p_smutex */
			} else
				lwp_unlock(l2);
			if (sa->sa_concurrency == SCARG(uap, concurrency))
				break;
		}
		mutex_exit(&p->p_smutex);
	}

	mutex_exit(&sa->sa_mutex);
#endif

	return (0);
}

int
sys_sa_yield(struct lwp *l, void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	mutex_enter(&p->p_smutex);
	if (p->p_sa == NULL || !(p->p_sflag & PS_SA)) {
		mutex_exit(&p->p_smutex);
		DPRINTFN(1,
		    ("sys_sa_yield(%d.%d) proc %p not SA (p_sa %p, flag %s)\n",
		    p->p_pid, l->l_lid, p, p->p_sa,
		    p->p_sflag & PS_SA ? "T" : "F"));
		return (EINVAL);
	}

	sa_yield(l);

	return (EJUSTRETURN);
}

void
sa_yield(struct lwp *l)
{
	struct proc *p = l->l_proc;
	struct sadata *sa = p->p_sa;
	struct sadata_vp *vp = l->l_savp;
	int ret;

	lwp_lock(l);

	if (vp->savp_lwp != l) {
		lwp_unlock(l);
		mutex_exit(&p->p_smutex);

		/*
		 * We lost the VP on our way here, this happens for
		 * instance when we sleep in systrace.  This will end
		 * in an SA_UNBLOCKED_UPCALL in sa_setwoken().
		 */
		DPRINTFN(1,("sa_yield(%d.%d) lost VP\n",
			     p->p_pid, l->l_lid));
		KDASSERT(l->l_flag & L_SA_BLOCKING);
		return;
	}

	mutex_exit(&p->p_smutex);

	/*
	 * If we're the last running LWP, stick around to receive
	 * signals.
	 */
	KDASSERT((l->l_flag & L_SA_YIELD) == 0);
	DPRINTFN(1,("sa_yield(%d.%d) going dormant\n",
		     p->p_pid, l->l_lid));
	/*
	 * A signal will probably wake us up. Worst case, the upcall
	 * happens and just causes the process to yield again.
	 */
	KDASSERT(vp->savp_lwp == l);

	/*
	 * If we were told to make an upcall or exit before
	 * the splsched(), make sure we process it instead of
	 * going to sleep. It might make more sense for this to
	 * be handled inside of tsleep....
	 */
	ret = 0;
	l->l_flag |= L_SA_YIELD;
	if (l->l_flag & L_SA_UPCALL) {
		/* KERNEL_UNLOCK(); in upcallret() */
		lwp_unlock(l);
		upcallret(l);
		KERNEL_LOCK(1, l);	/* XXXSMP */
	} else {
		do {
			lwp_unlock(l);
			DPRINTFN(1,("sa_yield(%d.%d) really going dormant\n",
				     p->p_pid, l->l_lid));

			mutex_enter(&sa->sa_mutex);
			sa->sa_concurrency--;
			ret = tsleep(l, PUSER | PCATCH, "sawait", 0);
			sa->sa_concurrency++;
			mutex_exit(&sa->sa_mutex);

			KDASSERT(vp->savp_lwp == l || l->l_flag & L_WEXIT);

			/* KERNEL_UNLOCK(); in upcallret() */
			upcallret(l);
			KERNEL_LOCK(1, l);	/* XXXSMP */

			lwp_lock(l);
		} while (l->l_flag & L_SA_YIELD);
	}

	DPRINTFN(1,("sa_yield(%d.%d) returned, ret %d\n",
		     p->p_pid, l->l_lid, ret));
}


int
sys_sa_preempt(struct lwp *l, void *v, register_t *retval)
{

	/* XXX Implement me. */
	return (ENOSYS);
}


/* XXX Hm, naming collision. */
void
sa_preempt(struct lwp *l)
{
	struct proc *p = l->l_proc;
	struct sadata *sa = p->p_sa;

	/*
	 * Defer saving the lwp's state because on some ports
	 * preemption can occur between generating an unblocked upcall
	 * and processing the upcall queue.
	 */
	if (sa->sa_flag & SA_FLAG_PREEMPT)
		sa_upcall(l, SA_UPCALL_PREEMPTED | SA_UPCALL_DEFER_EVENT,
		    l, NULL, 0, NULL, NULL);
}


/*
 * Set up the user-level stack and trapframe to do an upcall.
 *
 * NOTE: This routine WILL FREE "arg" in the case of failure!  Callers
 * should not touch the "arg" pointer once calling sa_upcall().
 */
int
sa_upcall(struct lwp *l, int type, struct lwp *event, struct lwp *interrupted,
	size_t argsize, void *arg, void (*func)(void *))
{
	struct sadata_upcall *sau;
	struct sadata *sa = l->l_proc->p_sa;
	struct sadata_vp *vp = l->l_savp;
	struct sastack *sast;
	int f, error;

	/* XXX prevent recursive upcalls if we sleep for memory */
	SA_LWP_STATE_LOCK(l, f);
	sau = sadata_upcall_alloc(1);
	mutex_enter(&sa->sa_mutex);
	sast = sa_getstack(sa);
	mutex_exit(&sa->sa_mutex);
	SA_LWP_STATE_UNLOCK(l, f);

	if (sau == NULL || sast == NULL) {
		if (sast != NULL) {
			mutex_enter(&sa->sa_mutex);
			sa_setstackfree(sast, sa);
			mutex_exit(&sa->sa_mutex);
		}
		if (sau != NULL)
			sadata_upcall_free(sau);
		return (ENOMEM);
	}
	DPRINTFN(9,("sa_upcall(%d.%d) using stack %p\n",
	    l->l_proc->p_pid, l->l_lid, sast->sast_stack.ss_sp));

	if (l->l_proc->p_emul->e_sa->sae_upcallconv) {
		error = (*l->l_proc->p_emul->e_sa->sae_upcallconv)(l, type,
		    &argsize, &arg, &func);
		if (error) {
			mutex_enter(&sa->sa_mutex);
			sa_setstackfree(sast, sa);
			mutex_exit(&sa->sa_mutex);
			sadata_upcall_free(sau);
			return error;
		}
	}

	sa_upcall0(sau, type, event, interrupted, argsize, arg, func);
	sau->sau_stack = sast->sast_stack;
	mutex_enter(&sa->sa_mutex);
	SIMPLEQ_INSERT_TAIL(&vp->savp_upcalls, sau, sau_next);
	lwp_lock(l);
	l->l_flag |= L_SA_UPCALL;
	lwp_unlock(l);
	mutex_exit(&sa->sa_mutex);

	return (0);
}

static void
sa_upcall0(struct sadata_upcall *sau, int type, struct lwp *event,
    struct lwp *interrupted, size_t argsize, void *arg, void (*func)(void *))
{

	KDASSERT((event == NULL) || (event != interrupted));

	sau->sau_flags = 0;

	if (type & SA_UPCALL_DEFER_EVENT) {
		sau->sau_event.ss_deferred.ss_lwp = event;
		sau->sau_flags |= SAU_FLAG_DEFERRED_EVENT;
	} else
		sa_upcall_getstate(&sau->sau_event, event);
	if (type & SA_UPCALL_DEFER_INTERRUPTED) {
		sau->sau_interrupted.ss_deferred.ss_lwp = interrupted;
		sau->sau_flags |= SAU_FLAG_DEFERRED_INTERRUPTED;
	} else
		sa_upcall_getstate(&sau->sau_interrupted, interrupted);

	sau->sau_type = type & SA_UPCALL_TYPE_MASK;
	sau->sau_argsize = argsize;
	sau->sau_arg = arg;
	sau->sau_argfreefunc = func;
}

void *
sa_ucsp(void *arg)
{
	ucontext_t *uc = arg;

	return (void *)(uintptr_t)_UC_MACHINE_SP(uc);
}

static void
sa_upcall_getstate(union sau_state *ss, struct lwp *l)
{
	caddr_t sp;
	size_t ucsize;

	if (l) {
		l->l_pflag |= LP_SA_SWITCHING;
		(*l->l_proc->p_emul->e_sa->sae_getucontext)(l,
		    (void *)&ss->ss_captured.ss_ctx);
		l->l_pflag &= ~LP_SA_SWITCHING;
		sp = (*l->l_proc->p_emul->e_sa->sae_ucsp)
		    (&ss->ss_captured.ss_ctx);
		/* XXX COMPAT_NETBSD32: _UC_UCONTEXT_ALIGN */
		sp = STACK_ALIGN(sp, ~_UC_UCONTEXT_ALIGN);
		ucsize = roundup(l->l_proc->p_emul->e_sa->sae_ucsize,
		    (~_UC_UCONTEXT_ALIGN) + 1);
		ss->ss_captured.ss_sa.sa_context =
		    (ucontext_t *)STACK_ALLOC(sp, ucsize);
		ss->ss_captured.ss_sa.sa_id = l->l_lid;
		ss->ss_captured.ss_sa.sa_cpu = l->l_savp->savp_id;
	} else
		ss->ss_captured.ss_sa.sa_context = NULL;
}


/*
 * Detect double pagefaults and pagefaults on upcalls.
 * - double pagefaults are detected by comparing the previous faultaddr
 *   against the current faultaddr
 * - pagefaults on upcalls are detected by checking if the userspace
 *   thread is running on an upcall stack
 */
static inline int
sa_pagefault(struct lwp *l, ucontext_t *l_ctx)
{
	struct proc *p;
	struct sadata *sa;
	struct sadata_vp *vp;
	struct sastack sast;
	int found;

	p = l->l_proc;
	sa = p->p_sa;
	vp = l->l_savp;

	LOCK_ASSERT(mutex_owned(&sa->sa_mutex));
	KDASSERT(vp->savp_lwp == l);

	if (vp->savp_faultaddr == vp->savp_ofaultaddr) {
		DPRINTFN(10,("sa_pagefault(%d.%d) double page fault\n",
			     p->p_pid, l->l_lid));
		return 1;
	}

	sast.sast_stack.ss_sp = (*p->p_emul->e_sa->sae_ucsp)(l_ctx);
	sast.sast_stack.ss_size = 1;
	found = (RB_FIND(sasttree, &sa->sa_stackstree, &sast)) != NULL;

	if (found) {
		DPRINTFN(10,("sa_pagefault(%d.%d) upcall page fault\n",
			     p->p_pid, l->l_lid));
		return 1;
	}

	vp->savp_ofaultaddr = vp->savp_faultaddr;
	return 0;
}


/*
 * Called by tsleep(). Block current LWP and switch to another.
 *
 * WE ARE NOT ALLOWED TO SLEEP HERE!  WE ARE CALLED FROM WITHIN
 * SLEEPQ_BLOCK() ITSELF!
 */
void
sa_switch(struct lwp *l, struct sadata_upcall *sau, int type)
{
	struct proc *p = l->l_proc;
	struct sadata_vp *vp = l->l_savp;
	struct lwp *l2;
	struct sadata_upcall *freesau = NULL;
	struct sadata *sa = p->p_sa;

	LOCK_ASSERT(lwp_locked(l, &sched_mutex));

	DPRINTFN(4,("sa_switch(%d.%d type %d VP %d)\n", p->p_pid, l->l_lid,
	    type, vp->savp_lwp ? vp->savp_lwp->l_lid : 0));

	if (l->l_flag & L_WEXIT) {
		mi_switch(l, NULL);
		sadata_upcall_free(sau);
		return;
	}

	/* We're locking in reverse order.  XXXSMP SA must die. */
	if (!mutex_tryenter(&sa->sa_mutex)) {
		lwp_unlock(l);
		mutex_enter(&sa->sa_mutex);
		mutex_enter(&p->p_smutex);
		lwp_lock(l);
	} else if (!mutex_tryenter(&p->p_smutex)) {
		lwp_unlock(l);
		mutex_enter(&p->p_smutex);
		lwp_lock(l);
	}
	if (l->l_stat == LSONPROC) {
		lwp_unlock(l);
		mutex_exit(&p->p_smutex);
		mutex_exit(&sa->sa_mutex);
		return;
	}

	if (l->l_flag & L_SA_YIELD) {
		/*
		 * Case 0: we're blocking in sa_yield
		 */
		if (vp->savp_wokenq_head == NULL && p->p_timerpend == 0) {
			l->l_flag |= L_SA_IDLE;
			mutex_exit(&p->p_smutex);
			mutex_exit(&sa->sa_mutex);
			mi_switch(l, NULL);
		} else {
			/*
			 * Make us running again. lwp_unsleep() will
			 * release the lock.
			 */
			mutex_exit(&p->p_smutex);
			mutex_exit(&sa->sa_mutex);
			lwp_unsleep(l);
		}
		sadata_upcall_free(sau);
		return;
	}

	if (vp->savp_lwp == l) {
		/*
		 * Case 1: we're blocking for the first time; generate
		 * a SA_BLOCKED upcall and allocate resources for the
		 * UNBLOCKED upcall.
		 */
		if (sau == NULL) {
#ifdef DIAGNOSTIC
			printf("sa_switch(%d.%d): no upcall data.\n",
			    p->p_pid, l->l_lid);
#endif
			mutex_exit(&p->p_smutex);
			mutex_exit(&sa->sa_mutex);
			mi_switch(l, NULL);
			return;
		}

		/*
		 * The process of allocating a new LWP could cause
		 * sleeps. We're called from inside sleep, so that
		 * would be Bad. Therefore, we must use a cached new
		 * LWP. The first thing that this new LWP must do is
		 * allocate another LWP for the cache.  */
		l2 = sa_getcachelwp(p, vp);
		if (l2 == NULL) {
			/* XXXSMP */
			/* No upcall for you! */
			/* XXX The consequences of this are more subtle and
			 * XXX the recovery from this situation deserves
			 * XXX more thought.
			 */

			/* XXXUPSXXX Should only happen with concurrency > 1 */
#ifdef DIAGNOSTIC
			printf("sa_switch(%d.%d): no cached LWP for upcall.\n",
			    p->p_pid, l->l_lid);
#endif
			mutex_exit(&p->p_smutex);
			mutex_exit(&sa->sa_mutex);
			mi_switch(l, NULL);
			sadata_upcall_free(sau);
			return;
		}

		cpu_setfunc(l2, sa_switchcall, sau);
		sa_upcall0(sau, SA_UPCALL_BLOCKED, l, NULL, 0, NULL, NULL);

		/*
		 * Perform the double/upcall pagefault check.
		 * We do this only here since we need l's ucontext to
		 * get l's userspace stack. sa_upcall0 above has saved
		 * it for us.
		 * The LP_SA_PAGEFAULT flag is set in the MD
		 * pagefault code to indicate a pagefault.  The MD
		 * pagefault code also saves the faultaddr for us.
		 */
		if ((l->l_pflag & LP_SA_PAGEFAULT) && sa_pagefault(l,
		    &sau->sau_event.ss_captured.ss_ctx) != 0) {
			cpu_setfunc(l2, sa_switchcall, NULL);
			sa_putcachelwp(p, l2); /* PHOLD from sa_getcachelwp */
			mutex_exit(&p->p_smutex);
			mutex_exit(&sa->sa_mutex);
			mi_switch(l, NULL);
			sadata_upcall_free(sau);
			DPRINTFN(10,("sa_switch(%d.%d) page fault resolved\n",
				     p->p_pid, l->l_lid));
			mutex_enter(&sa->sa_mutex);
			if (vp->savp_faultaddr == vp->savp_ofaultaddr)
				vp->savp_ofaultaddr = -1;
			mutex_exit(&sa->sa_mutex);
			return;
		}

		DPRINTFN(8,("sa_switch(%d.%d) blocked upcall %d\n",
			     p->p_pid, l->l_lid, l2->l_lid));

		if (l->l_mutex != l2->l_mutex)	/* XXXSMP */
			lwp_lock(l2);
		l->l_flag |= L_SA_BLOCKING;
		l2->l_priority = l2->l_usrpri;
		l2->l_stat = LSRUN;
		vp->savp_blocker = l;
		vp->savp_lwp = l2;
		setrunqueue(l2);
		PRELE(l2); /* Remove the artificial hold-count */
		KDASSERT(l2 != l);
		if (l->l_mutex != l2->l_mutex)	/* XXXSMP */
			lwp_unlock(l2);
	} else if (vp->savp_lwp != NULL) {
		/*
		 * Case 2: We've been woken up while another LWP was
		 * on the VP, but we're going back to sleep without
		 * having returned to userland and delivering the
		 * SA_UNBLOCKED upcall (select and poll cause this
		 * kind of behavior a lot).
		 */
		freesau = sau;
		l2 = NULL;
	} else {
		/* NOTREACHED */
		mutex_exit(&p->p_smutex);
		mutex_exit(&sa->sa_mutex);
		lwp_unlock(l);
		panic("sa_vp empty");
	}

	DPRINTFN(4,("sa_switch(%d.%d) switching to LWP %d.\n",
	    p->p_pid, l->l_lid, l2 ? l2->l_lid : 0));
	mutex_exit(&p->p_smutex);
	mutex_exit(&sa->sa_mutex);
	mi_switch(l, l2);
	sadata_upcall_free(freesau);
	DPRINTFN(4,("sa_switch(%d.%d flag %x) returned.\n",
	    p->p_pid, l->l_lid, l->l_flag));
	KDASSERT(l->l_wchan == 0);
}

static void
sa_switchcall(void *arg)
{
	struct lwp *l, *l2;
	struct proc *p;
	struct sadata_vp *vp;
	struct sadata_upcall *sau;
	struct sastack *sast;
	struct sadata *sa;

	l2 = curlwp;
	p = l2->l_proc;
	vp = l2->l_savp;
	sau = arg;
	sa = p->p_sa;

	lwp_lock(l2);
	if (l2->l_flag & L_WEXIT) {
		lwp_unlock(l2);
		sadata_upcall_free(sau);
		lwp_exit(l2);
	}

	KASSERT(vp->savp_lwp == l2);
	DPRINTFN(6,("sa_switchcall(%d.%d)\n", p->p_pid, l2->l_lid));

	l2->l_flag &= ~L_SA;
	lwp_unlock(l2);

	if (LIST_EMPTY(&vp->savp_lwpcache)) {
		/* Allocate the next cache LWP */
		DPRINTFN(6,("sa_switchcall(%d.%d) allocating LWP\n",
		    p->p_pid, l2->l_lid));
		sa_newcachelwp(l2);
	}

	if (sau) {
		mutex_enter(&sa->sa_mutex);
		sast = sa_getstack(p->p_sa);
		l = vp->savp_blocker;		
		if (sast) {
			sau->sau_stack = sast->sast_stack;
			SIMPLEQ_INSERT_TAIL(&vp->savp_upcalls, sau, sau_next);
			lwp_lock(l2);
			l2->l_flag |= L_SA_UPCALL;
			mutex_exit(&sa->sa_mutex);
		} else {
#ifdef DIAGNOSTIC
			printf("sa_switchcall(%d.%d flag %x): Not enough stacks.\n",
			    p->p_pid, l->l_lid, l->l_flag);
#endif
			lwp_unlock(l2);
			sadata_upcall_free(sau);
			PHOLD(l2);
			mutex_enter(&p->p_smutex);	/* XXXAD */
			sa_putcachelwp(p, l2); /* sets L_SA */
			vp->savp_lwp = l;
			lwp_lock(l);
			l->l_flag &= ~L_SA_BLOCKING;
			lwp_unlock(l);
			p->p_nrlwps--;
			mutex_exit(&p->p_smutex);
			lwp_lock(l2);
			mi_switch(l2, NULL);
			/* mostly NOTREACHED */
			lwp_lock(l2);
		}
	}

	l2->l_flag |= L_SA;
	lwp_unlock(l2);
	upcallret(l2);
}

static int
sa_newcachelwp(struct lwp *l)
{
	struct proc *p;
	struct lwp *l2;
	vaddr_t uaddr;
	boolean_t inmem;

	p = l->l_proc;
	if (l->l_flag & L_WEXIT)
		return (0);

	inmem = uvm_uarea_alloc(&uaddr);
	if (__predict_false(uaddr == 0)) {
		return (ENOMEM);
	} else {
		newlwp(l, p, uaddr, inmem, 0, NULL, 0, child_return, 0, &l2);
		/* We don't want this LWP on the process's main LWP list, but
		 * newlwp helpfully puts it there. Unclear if newlwp should
		 * be tweaked.
		 */
		PHOLD(l2);
		mutex_enter(&p->p_smutex);
		l2->l_savp = l->l_savp;
		sa_putcachelwp(p, l2);
		mutex_exit(&p->p_smutex);
	}

	return (0);
}

/*
 * Take a normal process LWP and place it in the SA cache.
 * LWP must not be running!
 */
void
sa_putcachelwp(struct proc *p, struct lwp *l)
{
	struct sadata_vp *vp;

	LOCK_ASSERT(mutex_owned(&p->p_smutex));

	vp = l->l_savp;

	LIST_REMOVE(l, l_sibling);
	p->p_nlwps--;
	l->l_stat = LSSUSPENDED;
	l->l_prflag |= LPR_DETACHED;
	l->l_flag |= L_SA;
	mb_write();
	DPRINTFN(5,("sa_putcachelwp(%d.%d) Adding LWP %d to cache\n",
	    p->p_pid, curlwp->l_lid, l->l_lid));
	LIST_INSERT_HEAD(&vp->savp_lwpcache, l, l_sibling);
	vp->savp_ncached++;
}

/*
 * Fetch a LWP from the cache.
 */
struct lwp *
sa_getcachelwp(struct proc *p, struct sadata_vp *vp)
{
	struct lwp *l;

	LOCK_ASSERT(mutex_owned(&p->p_smutex));

	if (vp->savp_ncached == 0)
		return NULL;

	vp->savp_ncached--;
	l = LIST_FIRST(&vp->savp_lwpcache);
	LIST_REMOVE(l, l_sibling);
	p = l->l_proc;
	LIST_INSERT_HEAD(&p->p_lwps, l, l_sibling);
	l->l_prflag &= ~LPR_DETACHED;
	p->p_nlwps++;
	DPRINTFN(5,("sa_getcachelwp(%d.%d) Got LWP %d from cache.\n",
	    p->p_pid, curlwp->l_lid, l->l_lid));

	return l;
}


void
sa_unblock_userret(struct lwp *l)
{
	struct proc *p;
	struct lwp *l2;
	struct sadata *sa;
	struct sadata_vp *vp;
	struct sadata_upcall *sau;
	struct sastack *sast;
	int f;

	p = l->l_proc;
	sa = p->p_sa;
	vp = l->l_savp;

	if (l->l_flag & L_WEXIT)
		return;

	KERNEL_LOCK(1, l);
	SA_LWP_STATE_LOCK(l, f);

	DPRINTFN(7,("sa_unblock_userret(%d.%d %x) \n", p->p_pid, l->l_lid,
	    l->l_flag));

	mutex_enter(&sa->sa_mutex);

	sa_setwoken(l);
	/* maybe NOTREACHED */

	if (l != vp->savp_lwp) {
		/* Invoke an "unblocked" upcall */
		DPRINTFN(8,("sa_unblock_userret(%d.%d) unblocking\n",
		    p->p_pid, l->l_lid));

		l2 = sa_vp_repossess(l);

		if (l2 == NULL) {
			lwp_exit(l);
			/* NOTREACHED */
		}

		sast = sa_getstack(sa);
		mutex_exit(&sa->sa_mutex);

		if (l->l_flag & L_WEXIT) {
			lwp_exit(l);
			/* NOTREACHED */
		}

		sau = sadata_upcall_alloc(1);
		if (l->l_flag & L_WEXIT) {
			sadata_upcall_free(sau);
			lwp_exit(l);
			/* NOTREACHED */
		}

		KDASSERT(l2 != NULL);
		PHOLD(l2);

		KDASSERT(sast != NULL);
		DPRINTFN(9,("sa_unblock_userret(%d.%d) using stack %p\n",
		    l->l_proc->p_pid, l->l_lid, sast->sast_stack.ss_sp));

		/*
		 * Defer saving the event lwp's state because a
		 * PREEMPT upcall could be on the queue already.
		 */
		sa_upcall0(sau, SA_UPCALL_UNBLOCKED | SA_UPCALL_DEFER_EVENT,
			   l, l2, 0, NULL, NULL);
		sau->sau_stack = sast->sast_stack;

		mutex_enter(&sa->sa_mutex);
		SIMPLEQ_INSERT_TAIL(&vp->savp_upcalls, sau, sau_next);
		lwp_lock(l);
		l->l_flag |= L_SA_UPCALL;
		l->l_flag &= ~L_SA_BLOCKING;
		lwp_unlock(l);
		mutex_enter(&p->p_smutex);
		sa_putcachelwp(p, l2);
		mutex_exit(&p->p_smutex);
	}
	mutex_exit(&sa->sa_mutex);

	SA_LWP_STATE_UNLOCK(l, f);
	KERNEL_UNLOCK_LAST(l);
}

void
sa_upcall_userret(struct lwp *l)
{
	struct lwp *l2;
	struct proc *p;
	struct sadata *sa;
	struct sadata_vp *vp;
	struct sadata_upcall *sau;
	struct sastack *sast;
	int f;

	p = l->l_proc;
	sa = p->p_sa;
	vp = l->l_savp;

	KERNEL_LOCK(1, l);
	SA_LWP_STATE_LOCK(l, f);

	DPRINTFN(7,("sa_upcall_userret(%d.%d %x) \n", p->p_pid, l->l_lid,
	    l->l_flag));

	KDASSERT((l->l_flag & L_SA_BLOCKING) == 0);

	mutex_enter(&sa->sa_mutex);
	sast = NULL;
	if (SIMPLEQ_EMPTY(&vp->savp_upcalls) && vp->savp_wokenq_head != NULL) {
		sast = sa_getstack(sa);
		if (sast == NULL) {
			mutex_exit(&sa->sa_mutex);
			SA_LWP_STATE_UNLOCK(l, f);
			KERNEL_UNLOCK_LAST(l);
			preempt(1);
			return;
		}
	}
	if (SIMPLEQ_EMPTY(&vp->savp_upcalls) && vp->savp_wokenq_head != NULL &&
	    sast != NULL) {
		/* Invoke an "unblocked" upcall */
		l2 = vp->savp_wokenq_head;
		vp->savp_wokenq_head = l2->l_forw;
		mutex_exit(&sa->sa_mutex);

		DPRINTFN(9,("sa_upcall_userret(%d.%d) using stack %p\n",
		    l->l_proc->p_pid, l->l_lid, sast->sast_stack.ss_sp));

		if (l->l_flag & L_WEXIT) {
			lwp_exit(l);
			/* NOTREACHED */
		}

		DPRINTFN(8,("sa_upcall_userret(%d.%d) unblocking %d\n",
		    p->p_pid, l->l_lid, l2->l_lid));

		sau = sadata_upcall_alloc(1);
		if (l->l_flag & L_WEXIT) {
			sadata_upcall_free(sau);
			lwp_exit(l);
			/* NOTREACHED */
		}

		sa_upcall0(sau, SA_UPCALL_UNBLOCKED, l2, l, 0, NULL, NULL);
		sau->sau_stack = sast->sast_stack;
		SIMPLEQ_INSERT_TAIL(&vp->savp_upcalls, sau, sau_next);
		lwp_lock(l2);
		l2->l_flag &= ~L_SA_BLOCKING;
		lwp_unlock(l2);
		mutex_enter(&p->p_smutex);
		sa_putcachelwp(p, l2); /* PHOLD from sa_setwoken */
		mutex_exit(&p->p_smutex);
	} else if (sast)
		sa_setstackfree(sast, sa);

	KDASSERT(vp->savp_lwp == l);

	while ((sau = SIMPLEQ_FIRST(&vp->savp_upcalls)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&vp->savp_upcalls, sau_next);
		mutex_exit(&sa->sa_mutex);
		sa_makeupcalls(l, sau);
		mutex_enter(&sa->sa_mutex);
	}

	if (vp->savp_wokenq_head == NULL) {
		lwp_lock(l);
		l->l_flag &= ~L_SA_UPCALL;
		lwp_unlock(l);
	}

	mutex_exit(&sa->sa_mutex);
	KERNEL_UNLOCK_LAST(l);
	return;
}

#define	SACOPYOUT(sae, type, kp, up) \
	(((sae)->sae_sacopyout != NULL) ? \
	(*(sae)->sae_sacopyout)((type), (kp), (void *)(up)) : \
	copyout((kp), (void *)(up), sizeof(*(kp))))

static void
sa_makeupcalls(struct lwp *l, struct sadata_upcall *sau)
{
	struct lwp *l2, *eventq;
	struct proc *p;
	const struct sa_emul *sae;
	struct sadata *sa;
	struct sadata_vp *vp;
	uintptr_t sapp, sap;
	struct sa_t self_sa;
	struct sa_t *sas[3];
	void *stack, *ap;
	union sau_state *e_ss;
	ucontext_t *kup, *up;
	size_t sz, ucsize;
	int i, nint, nevents, type, error;

	p = l->l_proc;
	sae = p->p_emul->e_sa;
	sa = p->p_sa;
	vp = l->l_savp;
	ucsize = sae->sae_ucsize;

	if (sau->sau_flags & SAU_FLAG_DEFERRED_EVENT)
		sa_upcall_getstate(&sau->sau_event,
		    sau->sau_event.ss_deferred.ss_lwp);
	if (sau->sau_flags & SAU_FLAG_DEFERRED_INTERRUPTED)
		sa_upcall_getstate(&sau->sau_interrupted,
		    sau->sau_interrupted.ss_deferred.ss_lwp);

#ifdef __MACHINE_STACK_GROWS_UP
	stack = sau->sau_stack.ss_sp;
#else
	stack = (caddr_t)sau->sau_stack.ss_sp + sau->sau_stack.ss_size;
#endif
	stack = STACK_ALIGN(stack, ALIGNBYTES);

	self_sa.sa_id = l->l_lid;
	self_sa.sa_cpu = vp->savp_id;
	sas[0] = &self_sa;
	nevents = 0;
	nint = 0;
	if (sau->sau_event.ss_captured.ss_sa.sa_context != NULL) {
		if (copyout(&sau->sau_event.ss_captured.ss_ctx,
		    sau->sau_event.ss_captured.ss_sa.sa_context,
		    ucsize) != 0) {
#ifdef DIAGNOSTIC
			printf("sa_makeupcalls(%d.%d): couldn't copyout"
			    " context of event LWP %d\n",
			    p->p_pid, l->l_lid,
			    sau->sau_event.ss_captured.ss_sa.sa_id);
#endif
			sigexit(l, SIGILL);
			/* NOTREACHED */
		}
		sas[1] = &sau->sau_event.ss_captured.ss_sa;
		nevents = 1;
	}
	if (sau->sau_interrupted.ss_captured.ss_sa.sa_context != NULL) {
		KDASSERT(sau->sau_interrupted.ss_captured.ss_sa.sa_context !=
		    sau->sau_event.ss_captured.ss_sa.sa_context);
		if (copyout(&sau->sau_interrupted.ss_captured.ss_ctx,
		    sau->sau_interrupted.ss_captured.ss_sa.sa_context,
		    ucsize) != 0) {
#ifdef DIAGNOSTIC
			printf("sa_makeupcalls(%d.%d): couldn't copyout"
			    " context of interrupted LWP %d\n",
			    p->p_pid, l->l_lid,
			    sau->sau_interrupted.ss_captured.ss_sa.sa_id);
#endif
			sigexit(l, SIGILL);
			/* NOTREACHED */
		}
		sas[2] = &sau->sau_interrupted.ss_captured.ss_sa;
		nint = 1;
	}
	eventq = NULL;
	if (sau->sau_type == SA_UPCALL_UNBLOCKED) {
		mutex_enter(&sa->sa_mutex);
		eventq = vp->savp_wokenq_head;
		vp->savp_wokenq_head = NULL;
		l2 = eventq;
		while (l2 != NULL) {
			nevents++;
			l2 = l2->l_forw;
		}
		mutex_exit(&sa->sa_mutex);
	}

	/* Copy out the activation's ucontext */
	up = (void *)STACK_ALLOC(stack, ucsize);
	stack = STACK_GROW(stack, ucsize);
	kup = kmem_zalloc(sizeof(*kup), KM_SLEEP);
	KASSERT(kup != NULL);
	kup->uc_stack = sau->sau_stack;
	kup->uc_flags = _UC_STACK;
	error = SACOPYOUT(sae, SAOUT_UCONTEXT, kup, up);
	kmem_free(kup, sizeof(*kup));
	if (error) {
		sadata_upcall_free(sau);
#ifdef DIAGNOSTIC
		printf("sa_makeupcalls: couldn't copyout activation"
		    " ucontext for %d.%d to %p\n", l->l_proc->p_pid, l->l_lid,
		    up);
#endif
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}
	sas[0]->sa_context = up;

	/* Next, copy out the sa_t's and pointers to them. */

	sz = (1 + nevents + nint) * sae->sae_sasize;
	sap = (uintptr_t)STACK_ALLOC(stack, sz);
	sap += sz;
	stack = STACK_GROW(stack, sz);

	sz = (1 + nevents + nint) * sae->sae_sapsize;
	sapp = (uintptr_t)STACK_ALLOC(stack, sz);
	sapp += sz;
	stack = STACK_GROW(stack, sz);

	KDASSERT(nint <= 1);
	e_ss = NULL;
	for (i = nevents + nint; i >= 0; i--) {
		struct sa_t *sasp;

		sap -= sae->sae_sasize;
		sapp -= sae->sae_sapsize;
		error = 0;
		if (i == 1 + nevents)	/* interrupted sa */
			sasp = sas[2];
		else if (i <= 1)	/* self_sa and event sa */
			sasp = sas[i];
		else {			/* extra sas */
			KDASSERT(sau->sau_type == SA_UPCALL_UNBLOCKED);
			KDASSERT(eventq != NULL);
			l2 = eventq;
			KDASSERT(l2 != NULL);
			eventq = l2->l_forw;
			DPRINTFN(8,
			    ("sa_makeupcalls(%d.%d) unblocking extra %d\n",
			    p->p_pid, l->l_lid, l2->l_lid));
			if (e_ss == NULL) {
				e_ss = kmem_alloc(sizeof(*e_ss), KM_SLEEP);
			}
			mutex_enter(&p->p_smutex);
			sa_upcall_getstate(e_ss, l2);
			lwp_lock(l2);
			l2->l_flag &= ~L_SA_BLOCKING;
			lwp_unlock(l2);
			sa_putcachelwp(p, l2); /* PHOLD from sa_setwoken */
			mutex_exit(&p->p_smutex);

			error = copyout(&e_ss->ss_captured.ss_ctx,
			    e_ss->ss_captured.ss_sa.sa_context, ucsize);
			sasp = &e_ss->ss_captured.ss_sa;
		}
		if (error != 0 ||
		    SACOPYOUT(sae, SAOUT_SA_T, sasp, sap) ||
		    SACOPYOUT(sae, SAOUT_SAP_T, &sap, sapp)) {
			/* Copying onto the stack didn't work. Die. */
			sadata_upcall_free(sau);
#ifdef DIAGNOSTIC
			printf("sa_makeupcalls(%d.%d): couldn't copyout\n",
			    p->p_pid, l->l_lid);
#endif
			if (e_ss != NULL) {
				kmem_free(e_ss, sizeof(*e_ss));
			}
			sigexit(l, SIGILL);
			/* NOTREACHED */
		}
	}
	if (e_ss != NULL) {
		kmem_free(e_ss, sizeof(*e_ss));
	}
	KDASSERT(eventq == NULL);

	/* Copy out the arg, if any */
	/* xxx assume alignment works out; everything so far has been
	 * a structure, so...
	 */
	if (sau->sau_arg) {
		ap = STACK_ALLOC(stack, sau->sau_argsize);
		stack = STACK_GROW(stack, sau->sau_argsize);
		if (copyout(sau->sau_arg, ap, sau->sau_argsize) != 0) {
			/* Copying onto the stack didn't work. Die. */
			sadata_upcall_free(sau);
#ifdef DIAGNOSTIC
			printf("sa_makeupcalls(%d.%d): couldn't copyout"
			    " sadata_upcall arg %p size %ld to %p \n",
			    p->p_pid, l->l_lid,
			    sau->sau_arg, (long) sau->sau_argsize, ap);
#endif
			sigexit(l, SIGILL);
			/* NOTREACHED */
		}
	} else {
		ap = NULL;
#ifdef __hppa__
		stack = STACK_ALIGN(stack, HPPA_FRAME_SIZE);
#endif
	}
	type = sau->sau_type;

	sadata_upcall_free(sau);

	DPRINTFN(7,("sa_makeupcalls(%d.%d): type %d\n", p->p_pid,
	    l->l_lid, type));

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SAUPCALL))
		ktrsaupcall(l, type, nevents, nint, (void *)sapp, ap);
#endif
	(*sae->sae_upcall)(l, type, nevents, nint, (void *)sapp, ap, stack,
	    sa->sa_upcall);

	lwp_lock(l);
	l->l_flag &= ~L_SA_YIELD;
	lwp_unlock(l);
}

static void
sa_setwoken(struct lwp *l)
{
	struct lwp *l2, *vp_lwp;
	struct proc *p = l->l_proc;
	struct sadata *sa;
	struct sadata_vp *vp;
	int swapper;

	if ((l->l_flag & L_SA_BLOCKING) == 0)
		return;

	sa = p->p_sa;
	vp = l->l_savp;
	vp_lwp = vp->savp_lwp;
	l2 = NULL;
	swapper = 0;

	KDASSERT(vp_lwp != NULL);
	DPRINTFN(3,("sa_setwoken(%d.%d) woken, flags %x, vp %d\n",
		     l->l_proc->p_pid, l->l_lid, l->l_flag,
		     vp_lwp->l_lid));

#if notyet
	if (vp_lwp->l_flag & L_SA_IDLE) {
		KDASSERT((vp_lwp->l_flag & L_SA_UPCALL) == 0);
		KDASSERT(vp->savp_wokenq_head == NULL);
		DPRINTFN(3,
		    ("sa_setwoken(%d.%d) repossess: idle vp_lwp %d state %d\n",
		    l->l_proc->p_pid, l->l_lid,
		    vp_lwp->l_lid, vp_lwp->l_stat));
		vp_lwp->l_flag &= ~L_SA_IDLE;
		PRELE(l);
		return;
	}
#endif

	DPRINTFN(3,("sa_setwoken(%d.%d) put on wokenq: vp_lwp %d state %d\n",
		     l->l_proc->p_pid, l->l_lid, vp_lwp->l_lid,
		     vp_lwp->l_stat));

	lwp_lock(vp_lwp);

	if (vp->savp_wokenq_head == NULL)
		vp->savp_wokenq_head = l;
	else
		*vp->savp_wokenq_tailp = l;
	*(vp->savp_wokenq_tailp = &l->l_forw) = NULL;

	switch (vp_lwp->l_stat) {
	case LSONPROC:
		if (vp_lwp->l_flag & L_SA_UPCALL)
			break;
		vp_lwp->l_flag |= L_SA_UPCALL;
		if (vp_lwp->l_flag & L_SA_YIELD)
			break;
		cpu_need_resched(vp_lwp->l_cpu);
		break;
	case LSSLEEP:
		if (vp_lwp->l_flag & L_SA_IDLE) {
			vp_lwp->l_flag &= ~L_SA_IDLE;
			vp_lwp->l_flag |= L_SA_UPCALL;
			/* Setrunnable will release the lock. */
			setrunnable(vp_lwp);
			vp_lwp = NULL;
			break;
		}
		vp_lwp->l_flag |= L_SA_UPCALL;
		break;
	case LSSUSPENDED:
#ifdef DIAGNOSTIC
		printf("sa_setwoken(%d.%d) vp lwp %d LSSUSPENDED\n",
		    l->l_proc->p_pid, l->l_lid, vp_lwp->l_lid);
#endif
		break;
	case LSSTOP:
		vp_lwp->l_flag |= L_SA_UPCALL;
		break;
	case LSRUN:
		if (vp_lwp->l_flag & L_SA_UPCALL)
			break;
		vp_lwp->l_flag |= L_SA_UPCALL;
		if (vp_lwp->l_flag & L_SA_YIELD)
			break;
		if (vp_lwp->l_slptime > 1) {
			void updatepri(struct lwp *);
			updatepri(vp_lwp);
		}
		vp_lwp->l_slptime = 0;
		if (vp_lwp->l_flag & L_INMEM) {
			if (vp_lwp->l_cpu == curcpu())
				l2 = vp_lwp;
			else
				cpu_need_resched(vp_lwp->l_cpu);
		} else
			swapper = 1;
		break;
	default:
		panic("sa_vp LWP not sleeping/onproc/runnable");
	}

	if (vp_lwp != NULL)
		lwp_unlock(vp_lwp);

	if (swapper)
		wakeup(&proc0);

	mutex_enter(&p->p_smutex);
	lwp_lock(l);
	/* XXXAD Is this correct? */
	if ((l->l_flag & L_SA_BLOCKING) != 0) {
		l->l_stat = LSSUSPENDED;
		p->p_nrlwps--;
	}
	mutex_exit(&p->p_smutex);
	mutex_exit(&sa->sa_mutex);
	mi_switch(l, l2);

	/* maybe NOTREACHED */
	if (l->l_flag & L_WEXIT)
		lwp_exit(l);

	mutex_enter(&sa->sa_mutex);
}

static struct lwp *
sa_vp_repossess(struct lwp *l)
{
	struct lwp *l2;
	struct proc *p = l->l_proc;
	struct sadata_vp *vp = l->l_savp;
	int ostat;

	LOCK_ASSERT(mutex_owned(&p->p_smutex));
	LOCK_ASSERT(mutex_owned(&p->p_sa->sa_mutex));

	/*
	 * Put ourselves on the virtual processor and note that the
	 * previous occupant of that position was interrupted.
	 */
	l2 = vp->savp_lwp;
	vp->savp_lwp = l;
	if (l2) {
		lwp_lock(l2);

		if (l2->l_flag & L_SA_YIELD)
			l2->l_flag &= ~(L_SA_YIELD|L_SA_IDLE);

		DPRINTFN(1,("sa_vp_repossess(%d.%d) vp lwp %d state %d\n",
			     p->p_pid, l->l_lid, l2->l_lid, l2->l_stat));

		KDASSERT(l2 != l);

		ostat = l2->l_stat;
		l2->l_stat = LSSUSPENDED;

		switch (ostat) {
		case LSRUN:
			p->p_nrlwps--;
			remrunqueue(l2);
			lwp_unlock(l2);
			break;
		case LSSLEEP:
			p->p_nrlwps--;
			lwp_unsleep(l2);
			break;
		case LSSUSPENDED:
#ifdef DIAGNOSTIC
			printf("sa_vp_repossess(%d.%d) vp lwp %d LSSUSPENDED\n",
			    l->l_proc->p_pid, l->l_lid, l2->l_lid);
#endif
			lwp_unlock(l2);
			break;
#ifdef DIAGNOSTIC
		default:
			lwp_unlock(l2);
			panic("SA VP %d.%d is in state %d, not running"
			    " or sleeping\n", p->p_pid, l2->l_lid,
			    l2->l_stat);
#endif
		}
	}

	return l2;
}



#ifdef DEBUG
int debug_print_sa(struct proc *);
int debug_print_lwp(struct lwp *);
int debug_print_proc(int);

int
debug_print_proc(int pid)
{
	struct proc *p;

	p = pfind(pid);
	if (p == NULL)
		printf("No process %d\n", pid);
	else
		debug_print_sa(p);

	return 0;
}

int
debug_print_sa(struct proc *p)
{
	struct lwp *l;
	struct sadata *sa;
	struct sadata_vp *vp;

	printf("Process %d (%s), state %d, address %p, flags %x\n",
	    p->p_pid, p->p_comm, p->p_stat, p, p->p_flag);
	printf("LWPs: %d (%d running, %d zombies)\n", p->p_nlwps, p->p_nrlwps,
	    p->p_nzlwps);
	LIST_FOREACH(l, &p->p_lwps, l_sibling)
		debug_print_lwp(l);
	sa = p->p_sa;
	if (sa) {
		SLIST_FOREACH(vp, &sa->sa_vps, savp_next) {
			if (vp->savp_lwp)
				printf("SA VP: %d %s\n", vp->savp_lwp->l_lid,
				    vp->savp_lwp->l_flag & L_SA_YIELD ?
				    (vp->savp_lwp->l_flag & L_SA_IDLE ?
					"idle" : "yielding") : "");
			printf("SAs: %d cached LWPs\n", vp->savp_ncached);
			LIST_FOREACH(l, &vp->savp_lwpcache, l_sibling)
				debug_print_lwp(l);
		}
	}

	return 0;
}

int
debug_print_lwp(struct lwp *l)
{

	printf("LWP %d address %p ", l->l_lid, l);
	printf("state %d flags %x ", l->l_stat, l->l_flag);
	if (l->l_wchan)
		printf("wait %p %s", l->l_wchan, l->l_wmesg);
	printf("\n");

	return 0;
}

#endif
