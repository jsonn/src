/*	$NetBSD: kern_mutex.c,v 1.1.36.21 2007/02/09 19:58:10 ad Exp $	*/

/*-
 * Copyright (c) 2002, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Andrew Doran.
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

/*
 * Kernel mutex implementation, modeled after those found in Solaris,
 * a description of which can be found in:
 *
 *	Solaris Internals: Core Kernel Architecture, Jim Mauro and
 *	    Richard McDougall.
 */

#include "opt_multiprocessor.h"

#define	__MUTEX_PRIVATE

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_mutex.c,v 1.1.36.21 2007/02/09 19:58:10 ad Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/sleepq.h>
#include <sys/systm.h>
#include <sys/lockdebug.h>
#include <sys/kernel.h>

#include <dev/lockstat.h>

#include <machine/intr.h>

/*
 * When not running a debug kernel, spin mutexes are not much
 * more than an splraiseipl() and splx() pair.
 */

#if defined(DIAGNOSTIC) || defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
#define	FULL
#endif

/*
 * Debugging support.
 */

#define	MUTEX_WANTLOCK(mtx)					\
    LOCKDEBUG_WANTLOCK(MUTEX_GETID(mtx),			\
        (uintptr_t)__builtin_return_address(0), 0)
#define	MUTEX_LOCKED(mtx)					\
    LOCKDEBUG_LOCKED(MUTEX_GETID(mtx),				\
        (uintptr_t)__builtin_return_address(0), 0)
#define	MUTEX_UNLOCKED(mtx)					\
    LOCKDEBUG_UNLOCKED(MUTEX_GETID(mtx),			\
        (uintptr_t)__builtin_return_address(0), 0)
#define	MUTEX_ABORT(mtx, msg)					\
    mutex_abort(mtx, __FUNCTION__, msg)

#if defined(LOCKDEBUG)

#define	MUTEX_DASSERT(mtx, cond)				\
do {								\
	if (!(cond))						\
		MUTEX_ABORT(mtx, "assertion failed: " #cond);	\
} while (/* CONSTCOND */ 0);

#else	/* LOCKDEBUG */

#define	MUTEX_DASSERT(mtx, cond)	/* nothing */

#endif /* LOCKDEBUG */

#if defined(DIAGNOSTIC)

#define	MUTEX_ASSERT(mtx, cond)					\
do {								\
	if (!(cond))						\
		MUTEX_ABORT(mtx, "assertion failed: " #cond);	\
} while (/* CONSTCOND */ 0)

#else	/* DIAGNOSTIC */

#define	MUTEX_ASSERT(mtx, cond)	/* nothing */

#endif	/* DIAGNOSTIC */

/*
 * Spin mutex SPL save / restore.
 */

#define	MUTEX_SPIN_SPLRAISE(mtx)					\
do {									\
	struct cpu_info *x__ci = curcpu();				\
	int x__cnt, s;							\
	x__cnt = x__ci->ci_mtx_count--;					\
	s = splraiseipl(mtx->mtx_ipl);					\
	if (x__cnt == 0)						\
		x__ci->ci_mtx_oldspl = (s);				\
} while (/* CONSTCOND */ 0)

#define	MUTEX_SPIN_SPLRESTORE(mtx)					\
do {									\
	struct cpu_info *x__ci = curcpu();				\
	int s = x__ci->ci_mtx_oldspl;					\
	__insn_barrier();						\
	if (++(x__ci->ci_mtx_count) == 0)				\
		splx(s);						\
} while (/* CONSTCOND */ 0)

/*
 * For architectures that provide 'simple' mutexes: they provide a
 * CAS function that is either MP-safe, or does not need to be MP
 * safe.  Adaptive mutexes on these architectures do not require an
 * additional interlock.
 */

#ifdef __HAVE_SIMPLE_MUTEXES

#define	MUTEX_OWNER(owner)						\
	(owner & MUTEX_THREAD)
#define	MUTEX_OWNED(owner)						\
	(owner != 0)
#define	MUTEX_HAS_WAITERS(mtx)						\
	(((int)(mtx)->mtx_owner & MUTEX_BIT_WAITERS) != 0)

#define	MUTEX_INITIALIZE_ADAPTIVE(mtx, id)				\
do {									\
	(mtx)->mtx_id = (id);						\
} while (/* CONSTCOND */ 0);

#define	MUTEX_INITIALIZE_SPIN(mtx, id, ipl)				\
do {									\
	(mtx)->mtx_owner = MUTEX_BIT_SPIN;				\
	(mtx)->mtx_ipl = makeiplcookie((ipl));				\
	(mtx)->mtx_id = (id);						\
	__cpu_simple_lock_init(&(mtx)->mtx_lock);			\
} while (/* CONSTCOND */ 0)

#define	MUTEX_DESTROY(mtx)						\
do {									\
	(mtx)->mtx_owner = MUTEX_THREAD;				\
	(mtx)->mtx_id = -1;						\
} while (/* CONSTCOND */ 0);

#define	MUTEX_SPIN_P(mtx)		\
    (((mtx)->mtx_owner & MUTEX_BIT_SPIN) != 0)
#define	MUTEX_ADAPTIVE_P(mtx)		\
    (((mtx)->mtx_owner & MUTEX_BIT_SPIN) == 0)

#define	MUTEX_GETID(mtx)		((mtx)->mtx_id)

static inline int
MUTEX_ACQUIRE(kmutex_t *mtx, uintptr_t curthread)
{
	int rv;
	rv = MUTEX_CAS(&mtx->mtx_owner, 0UL, curthread);
	MUTEX_RECEIVE();
	return rv;
}

static inline int
MUTEX_SET_WAITERS(kmutex_t *mtx, uintptr_t owner)
{
	int rv;
	rv = MUTEX_CAS(&mtx->mtx_owner, owner, owner | MUTEX_BIT_WAITERS);
	MUTEX_RECEIVE();
	return rv;
}

static inline void
MUTEX_RELEASE(kmutex_t *mtx)
{
	MUTEX_GIVE();
	mtx->mtx_owner = 0;
}
#endif	/* __HAVE_SIMPLE_MUTEXES */

/*
 * Patch in stubs via strong alias where they are not available.
 */

#if defined(LOCKDEBUG)
#undef	__HAVE_MUTEX_STUBS
#undef	__HAVE_SPIN_MUTEX_STUBS
#endif

#ifndef __HAVE_MUTEX_STUBS
__strong_alias(mutex_enter, mutex_vector_enter);
__strong_alias(mutex_exit, mutex_vector_exit);
#endif

#ifndef __HAVE_SPIN_MUTEX_STUBS
__strong_alias(mutex_spin_enter, mutex_vector_enter);
__strong_alias(mutex_spin_exit, mutex_vector_exit);
#endif

void	mutex_abort(kmutex_t *, const char *, const char *);
void	mutex_dump(volatile void *);
int	mutex_onproc(uintptr_t, struct cpu_info **);

lockops_t mutex_spin_lockops = {
	"Mutex",
	0,
	mutex_dump
};

lockops_t mutex_adaptive_lockops = {
	"Mutex",
	1,
	mutex_dump
};

/*
 * mutex_dump:
 *
 *	Dump the contents of a mutex structure.
 */
void
mutex_dump(volatile void *cookie)
{
	volatile kmutex_t *mtx = cookie;

	printf_nolog("owner field  : %#018lx wait/spin: %16d/%d\n",
	    (long)MUTEX_OWNER(mtx->mtx_owner), MUTEX_HAS_WAITERS(mtx),
	    MUTEX_SPIN_P(mtx));
}

/*
 * mutex_abort:
 *
 *	Dump information about an error and panic the system.
 */
__attribute ((noinline)) __attribute ((noreturn)) void
mutex_abort(kmutex_t *mtx, const char *func, const char *msg)
{

	LOCKDEBUG_ABORT(MUTEX_GETID(mtx), mtx, (MUTEX_SPIN_P(mtx) ?
	    &mutex_spin_lockops : &mutex_adaptive_lockops),
	    __FUNCTION__, msg);
	/* NOTREACHED */
}

/*
 * mutex_init:
 *
 *	Initialize a mutex for use.  Note that adaptive mutexes are in
 *	essence spin mutexes that can sleep to avoid deadlock and wasting
 *	CPU time.  We can't easily provide a type of mutex that always
 *	sleeps - see comments in mutex_vector_enter() about releasing
 *	mutexes unlocked.
 */
void
mutex_init(kmutex_t *mtx, kmutex_type_t type, int ipl)
{
	u_int id;

	memset(mtx, 0, sizeof(*mtx));

	if (type == MUTEX_DRIVER)
		type = (ipl == IPL_NONE ? MUTEX_ADAPTIVE : MUTEX_SPIN);

	switch (type) {
	case MUTEX_ADAPTIVE:
	case MUTEX_DEFAULT:
		KASSERT(ipl == IPL_NONE);
		id = LOCKDEBUG_ALLOC(mtx, &mutex_adaptive_lockops);
		MUTEX_INITIALIZE_ADAPTIVE(mtx, id);
		break;
	case MUTEX_SPIN:
		id = LOCKDEBUG_ALLOC(mtx, &mutex_spin_lockops);
		MUTEX_INITIALIZE_SPIN(mtx, id, ipl);
		break;
	default:
		panic("mutex_init: impossible type");
		break;
	}
}

/*
 * mutex_destroy:
 *
 *	Tear down a mutex.
 */
void
mutex_destroy(kmutex_t *mtx)
{

	if (MUTEX_ADAPTIVE_P(mtx)) {
		MUTEX_ASSERT(mtx, !MUTEX_OWNED(mtx->mtx_owner) &&
		    !MUTEX_HAS_WAITERS(mtx));
	} else {
		MUTEX_ASSERT(mtx, mtx->mtx_lock != __SIMPLELOCK_LOCKED);
	}

	LOCKDEBUG_FREE(mtx, MUTEX_GETID(mtx));
	MUTEX_DESTROY(mtx);
}

/*
 * mutex_onproc:
 *
 *	Return true if an adaptive mutex owner is running on a CPU in the
 *	system.  If the target is waiting on the kernel big lock, then we
 *	return false immediately.  This is necessary to avoid deadlock
 *	against the big lock.
 *
 *	Note that we can't use the mutex owner field as an LWP pointer.  We
 *	don't have full control over the timing of our execution, and so the
 *	pointer could be completely invalid by the time we dereference it.
 */
#ifdef MULTIPROCESSOR
int
mutex_onproc(uintptr_t owner, struct cpu_info **cip)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct lwp *l;

	if (!MUTEX_OWNED(owner))
		return 0;
	l = (struct lwp *)MUTEX_OWNER(owner);

	if ((ci = *cip) != NULL && ci->ci_curlwp == l) {
		mb_read();	/* XXXSMP Necessary? */
		return ci->ci_biglock_wanted != l;
	}

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (ci->ci_curlwp == l) {
			*cip = ci;
			mb_read();	/* XXXSMP Necessary? */
			return ci->ci_biglock_wanted != l;
		}
	}

	*cip = NULL;
	return 0;
}
#endif

/*
 * mutex_vector_enter:
 *
 *	Support routine for mutex_enter() that must handles all cases.  In
 *	the LOCKDEBUG case, mutex_enter() is always aliased here, even if
 *	fast-path stubs are available.  If an mutex_spin_enter() stub is
 *	not available, then it is also aliased directly here.
 */
void
mutex_vector_enter(kmutex_t *mtx)
{
	uintptr_t owner, curthread;
	turnstile_t *ts;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = NULL;
	u_int count;
#endif
	LOCKSTAT_COUNTER(spincnt);
	LOCKSTAT_COUNTER(slpcnt);
	LOCKSTAT_TIMER(spintime);
	LOCKSTAT_TIMER(slptime);
	LOCKSTAT_FLAG(lsflag);

	/*
	 * Handle spin mutexes.
	 */
	if (MUTEX_SPIN_P(mtx)) {
#if defined(LOCKDEBUG) && defined(MULTIPROCESSOR)
		u_int spins = 0;
#endif
		MUTEX_SPIN_SPLRAISE(mtx);
		MUTEX_WANTLOCK(mtx);
#ifdef FULL
		if (__cpu_simple_lock_try(&mtx->mtx_lock)) {
			MUTEX_LOCKED(mtx);
			return;
		}
#if !defined(MULTIPROCESSOR)
		MUTEX_ABORT(mtx, "locking against myself");
#else /* !MULTIPROCESSOR */

		LOCKSTAT_ENTER(lsflag);
		LOCKSTAT_START_TIMER(lsflag, spintime);
		count = SPINLOCK_BACKOFF_MIN;

		/*
		 * Spin testing the lock word and do exponential backoff
		 * to reduce cache line ping-ponging between CPUs.
		 */
		do {
			if (panicstr != NULL)
				break;
			while (mtx->mtx_lock == __SIMPLELOCK_LOCKED) {
				SPINLOCK_BACKOFF(count); 
#ifdef LOCKDEBUG
				if (SPINLOCK_SPINOUT(spins))
					MUTEX_ABORT(mtx, "spinout");
#endif	/* LOCKDEBUG */
			}
		} while (!__cpu_simple_lock_try(&mtx->mtx_lock));

		if (count != SPINLOCK_BACKOFF_MIN) {
			LOCKSTAT_STOP_TIMER(lsflag, spintime);
			LOCKSTAT_EVENT(lsflag, mtx,
			    LB_SPIN_MUTEX | LB_SPIN, 1, spintime);
		}
		LOCKSTAT_EXIT(lsflag);
#endif	/* !MULTIPROCESSOR */
#endif	/* FULL */
		MUTEX_LOCKED(mtx);
		return;
	}

	curthread = (uintptr_t)curlwp;

	MUTEX_DASSERT(mtx, MUTEX_ADAPTIVE_P(mtx));
	MUTEX_ASSERT(mtx, curthread != 0);
	MUTEX_WANTLOCK(mtx);

#ifdef LOCKDEBUG
	if (panicstr == NULL) {
		simple_lock_only_held(NULL, "mutex_enter");
#ifdef MULTIPROCESSOR
		LOCKDEBUG_BARRIER(&kernel_lock, 1);
#else
		LOCKDEBUG_BARRIER(NULL, 1);
#endif
	}
#endif

	LOCKSTAT_ENTER(lsflag);

	/*
	 * Adaptive mutex; spin trying to acquire the mutex.  If we
	 * determine that the owner is not running on a processor,
	 * then we stop spinning, and sleep instead.
	 */
	for (;;) {
		owner = mtx->mtx_owner;
		if (!MUTEX_OWNED(owner)) {
			/*
			 * Mutex owner clear could mean two things:
			 *
			 *	* The mutex has been released.
			 *	* The owner field hasn't been set yet.
			 *
			 * Try to acquire it again.  If that fails,
			 * we'll just loop again.
			 */
			if (MUTEX_ACQUIRE(mtx, curthread))
				break;
			continue;
		}

		if (panicstr != NULL)
			return;
		if (MUTEX_OWNER(owner) == curthread)
			MUTEX_ABORT(mtx, "locking against myself");

#ifdef MULTIPROCESSOR
		/*
		 * Check to see if the owner is running on a processor.
		 * If so, then we should just spin, as the owner will
		 * likely release the lock very soon.
		 */
		if (mutex_onproc(owner, &ci)) {
			LOCKSTAT_START_TIMER(lsflag, spintime);
			count = SPINLOCK_BACKOFF_MIN;
			for (;;) {
				owner = mtx->mtx_owner;
				if (!mutex_onproc(owner, &ci))
					break;
				SPINLOCK_BACKOFF(count);
			}
			LOCKSTAT_STOP_TIMER(lsflag, spintime);
			LOCKSTAT_COUNT(spincnt, 1);
			if (!MUTEX_OWNED(owner))
				continue;
		}
#endif

		ts = turnstile_lookup(mtx);

		/*
		 * Once we have the turnstile chain interlock, mark the
		 * mutex has having waiters.  If that fails, spin again:
		 * chances are that the mutex has been released.
		 */
		if (!MUTEX_SET_WAITERS(mtx, owner)) {
			turnstile_exit(mtx);
			continue;
		}

#ifdef MULTIPROCESSOR
		/*
		 * mutex_exit() is permitted to release the mutex without
		 * any interlocking instructions, and the following can
		 * occur as a result:
		 *
		 *  CPU 1: MUTEX_SET_WAITERS()      CPU2: mutex_exit()
		 * ---------------------------- ----------------------------
		 *		..		    acquire cache line
		 *		..                   test for waiters
		 *	acquire cache line    <-      lose cache line
		 *	 lock cache line	           ..
		 *     verify mutex is held                ..
		 *	    set waiters  	           ..
		 *	 unlock cache line		   ..
		 *	  lose cache line     ->    acquire cache line
		 *		..	          clear lock word, waiters 
		 *	  return success
		 *
		 * There is a another race that can occur: a third CPU could
		 * acquire the mutex as soon as it is released.  Since
		 * adaptive mutexes are primarily spin mutexes, this is not
		 * something that we need to worry about too much.  What we
		 * do need to ensure is that the waiters bit gets set.
		 *
		 * To allow the unlocked release, we need to make some
		 * assumptions here:
		 *
		 * o Release is the only non-atomic/unlocked operation
		 *   that can be performed on the mutex.  (It must still
		 *   be atomic on the local CPU, e.g. in case interrupted
		 *   or preempted).
		 *
		 * o At any given time, MUTEX_SET_WAITERS() can only ever
		 *   be in progress on one CPU in the system - guarenteed
		 *   by the turnstile chain lock.
		 *
		 * o No other operations other than MUTEX_SET_WAITERS()
		 *   and release can modify a mutex with a non-zero
		 *   owner field.
		 *
		 * o The result of a successful MUTEX_SET_WAITERS() call
		 *   is an unbuffered write that is immediately visible
		 *   to all other processors in the system.
		 *
		 * o If the holding LWP switches away, it posts a store
		 *   fence before changing curlwp, ensuring that any
		 *   overwrite of the mutex waiters flag by mutex_exit()
		 *   completes before the modification of curlwp becomes
		 *   visible to this CPU.
		 *
		 * o cpu_switch() posts a store fence before setting curlwp
		 *   and before resuming execution of an LWP.
		 * 
		 * o _kernel_lock() posts a store fence before setting
		 *   curcpu()->ci_biglock_wanted, and after clearing it. 
		 *   This ensures that any overwrite of the mutex waiters
		 *   flag by mutex_exit() completes before the modification
		 *   of ci_biglock_wanted becomes visible.
		 *
		 * We now post a read memory barrier (after setting the
		 * waiters field) and check the lock holder's status again.
		 * Some of the possible outcomes (not an exhaustive list):
		 *
		 * 1. The onproc check returns true: the holding LWP is
		 *    running again.  The lock may be released soon and
		 *    we should spin.  Importantly, we can't trust the
		 *    value of the waiters flag.
		 *
		 * 2. The onproc check returns false: the holding LWP is
		 *    not running.  We now have the oppertunity to check
		 *    if mutex_exit() has blatted the modifications made
		 *    by MUTEX_SET_WAITERS().
		 *
		 * 3. The onproc check returns false: the holding LWP may
		 *    or may not be running.  It has context switched at
		 *    some point during our check.  Again, we have the
		 *    chance to see if the waiters bit is still set or
		 *    has been overwritten.
		 *
		 * 4. The onproc check returns false: the holding LWP is
		 *    running on a CPU, but wants the big lock.  It's OK
		 *    to check the waiters field in this case.
		 *
		 * 5. The has-waiters check fails: the mutex has been
		 *    released, the waiters flag cleared and another LWP
		 *    now owns the mutex.
		 *
		 * 6. The has-waiters check fails: the mutex has been
		 *    released.
		 *
		 * If the waiters bit is not set it's unsafe to go asleep,
		 * as we might never be awoken.
		 */
		mb_read();
		if (mutex_onproc(owner, &ci) || !MUTEX_HAS_WAITERS(mtx)) {
			turnstile_exit(mtx);
			continue;
		}
#endif	/* MULTIPROCESSOR */

		LOCKSTAT_START_TIMER(lsflag, slptime);

		turnstile_block(ts, TS_WRITER_Q, mtx);

		LOCKSTAT_STOP_TIMER(lsflag, slptime);
		LOCKSTAT_COUNT(slpcnt, 1);

		turnstile_unblock();
	}

	LOCKSTAT_EVENT(lsflag, mtx, LB_ADAPTIVE_MUTEX | LB_SLEEP1,
	    slpcnt, slptime);
	LOCKSTAT_EVENT(lsflag, mtx, LB_ADAPTIVE_MUTEX | LB_SPIN,
	    spincnt, spintime);
	LOCKSTAT_EXIT(lsflag);

	MUTEX_DASSERT(mtx, MUTEX_OWNER(mtx->mtx_owner) == curthread);
	MUTEX_LOCKED(mtx);
}

/*
 * mutex_vector_exit:
 *
 *	Support routine for mutex_exit() that handles all cases.
 */
void
mutex_vector_exit(kmutex_t *mtx)
{
	turnstile_t *ts;
	uintptr_t curthread;

	if (MUTEX_SPIN_P(mtx)) {
#ifdef FULL
		if (mtx->mtx_lock != __SIMPLELOCK_LOCKED)
			MUTEX_ABORT(mtx, "exiting unheld spin mutex");
		MUTEX_UNLOCKED(mtx);
		__cpu_simple_unlock(&mtx->mtx_lock);
#endif
		MUTEX_SPIN_SPLRESTORE(mtx);
		return;
	}

	if (__predict_false(panicstr != NULL) || __predict_false(cold)) {
		MUTEX_UNLOCKED(mtx);
		MUTEX_RELEASE(mtx);
		return;
	}

	curthread = (uintptr_t)curlwp;
	MUTEX_DASSERT(mtx, curthread != 0);
	MUTEX_ASSERT(mtx, MUTEX_OWNER(mtx->mtx_owner) == curthread);
	MUTEX_UNLOCKED(mtx);

	/*
	 * Get this lock's turnstile.  This gets the interlock on
	 * the sleep queue.  Once we have that, we can clear the
	 * lock.  If there was no turnstile for the lock, there
	 * were no waiters remaining.
	 */
	ts = turnstile_lookup(mtx);

	if (ts == NULL) {
		MUTEX_RELEASE(mtx);
		turnstile_exit(mtx);
	} else {
		MUTEX_RELEASE(mtx);
		turnstile_wakeup(ts, TS_WRITER_Q,
		    TS_WAITERS(ts, TS_WRITER_Q), NULL);
	}
}

/*
 * mutex_owned:
 *
 *	Return true if the current thread holds the mutex.
 */
int
mutex_owned(kmutex_t *mtx)
{

	if (MUTEX_ADAPTIVE_P(mtx))
		return MUTEX_OWNER(mtx->mtx_owner) == (uintptr_t)curlwp;
#ifdef FULL
	return mtx->mtx_lock == __SIMPLELOCK_LOCKED;
#else
	return 1;
#endif
}

/*
 * mutex_owner:
 *
 *	Return the current owner of an adaptive mutex.
 */
struct lwp *
mutex_owner(kmutex_t *mtx)
{

	MUTEX_ASSERT(mtx, MUTEX_ADAPTIVE_P(mtx));
	return (struct lwp *)MUTEX_OWNER(mtx->mtx_owner);
}

/*
 * mutex_tryenter:
 *
 *	Try to acquire the mutex; return non-zero if we did.
 */
int
mutex_tryenter(kmutex_t *mtx)
{
	uintptr_t curthread;

	MUTEX_WANTLOCK(mtx);

	/*
	 * Handle spin mutexes.
	 */
	if (MUTEX_SPIN_P(mtx)) {
		MUTEX_SPIN_SPLRAISE(mtx);
#ifdef FULL
		if (__cpu_simple_lock_try(&mtx->mtx_lock)) {
			MUTEX_LOCKED(mtx);
			return 1;
		}
		MUTEX_SPIN_SPLRESTORE(mtx);
#else
		MUTEX_LOCKED(mtx);
		return 1;
#endif
	} else {
		curthread = (uintptr_t)curlwp;
		MUTEX_ASSERT(mtx, curthread != 0);
		if (MUTEX_ACQUIRE(mtx, curthread)) {
			MUTEX_LOCKED(mtx);
			MUTEX_DASSERT(mtx,
			    MUTEX_OWNER(mtx->mtx_owner) == curthread);
			return 1;
		}
	}

	return 0;
}

#if defined(__HAVE_SPIN_MUTEX_STUBS) || defined(FULL)
/*
 * mutex_spin_retry:
 *
 *	Support routine for mutex_spin_enter().  Assumes that the caller
 *	has already raised the SPL, and adjusted counters.
 */
void
mutex_spin_retry(kmutex_t *mtx)
{
#ifdef MULTIPROCESSOR
	u_int count;
	LOCKSTAT_TIMER(spintime);
	LOCKSTAT_FLAG(lsflag);
#ifdef LOCKDEBUG
	u_int spins = 0;
#endif	/* LOCKDEBUG */

	MUTEX_WANTLOCK(mtx);

	LOCKSTAT_ENTER(lsflag);
	LOCKSTAT_START_TIMER(lsflag, spintime);
	count = SPINLOCK_BACKOFF_MIN;

	/*
	 * Spin testing the lock word and do exponential backoff
	 * to reduce cache line ping-ponging between CPUs.
	 */
	do {
		if (panicstr != NULL)
			break;
		while (mtx->mtx_lock == __SIMPLELOCK_LOCKED) {
			SPINLOCK_BACKOFF(count); 
#ifdef LOCKDEBUG
			if (SPINLOCK_SPINOUT(spins))
				MUTEX_ABORT(mtx, "spinout");
#endif	/* LOCKDEBUG */
		}
	} while (!__cpu_simple_lock_try(&mtx->mtx_lock));

	LOCKSTAT_STOP_TIMER(lsflag, spintime);
	LOCKSTAT_EVENT(lsflag, mtx, LB_SPIN_MUTEX | LB_SPIN, 1, spintime);
	LOCKSTAT_EXIT(lsflag);

	MUTEX_LOCKED(mtx);
#else	/* MULTIPROCESSOR */
	MUTEX_ABORT(mtx, "locking against myself");
#endif	/* MULTIPROCESSOR */
}
#endif	/* defined(__HAVE_SPIN_MUTEX_STUBS) || defined(FULL) */

/*
 * sched_lock_idle:
 *
 *	XXX Ugly hack for cpu_switch().
 */
void
sched_lock_idle(void)
{
#ifdef FULL
	kmutex_t *mtx = &sched_mutex;

	curcpu()->ci_mtx_count--;

	if (!__cpu_simple_lock_try(&mtx->mtx_lock)) {
		mutex_spin_retry(mtx);
		return;
	}

	MUTEX_LOCKED(mtx);
#else
	curcpu()->ci_mtx_count--;
#endif	/* FULL */
}

/*
 * sched_unlock_idle:
 *
 *	XXX Ugly hack for cpu_switch().
 */
void
sched_unlock_idle(void)
{
#ifdef FULL
	kmutex_t *mtx = &sched_mutex;

	if (mtx->mtx_lock != __SIMPLELOCK_LOCKED)
		MUTEX_ABORT(mtx, "sched_unlock_idle");

	MUTEX_UNLOCKED(mtx);
	__cpu_simple_unlock(&mtx->mtx_lock);
#endif	/* FULL */
	curcpu()->ci_mtx_count++;
}
