/*	$NetBSD: kern_lock.c,v 1.12.2.1 1998/11/09 06:06:31 chs Exp $	*/

/* 
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code contains ideas from software contributed to Berkeley by
 * Avadis Tevanian, Jr., Michael Wayne Young, and the Mach Operating
 * System project at Carnegie-Mellon University.
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
 *	@(#)kern_lock.c	8.18 (Berkeley) 5/21/95
 */

#include "opt_lockdebug.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/systm.h>
#include <machine/cpu.h>

/*
 * Locking primitives implementation.
 * Locks provide shared/exclusive sychronization.
 */

#ifdef LOCKDEBUG
#define COUNT(p, x) if (p) (p)->p_locks += (x)
#else
#define COUNT(p, x)
#endif

#if 0 /*#was defined(MULTIPROCESSOR)*/
/*-

This macro is Bad Style and it doesn't work either... [pk, 10-14-1998]

-*
 * For multiprocessor system, try spin lock first.
 *
 * This should be inline expanded below, but we cannot have #if
 * inside a multiline define.
 */

int lock_wait_time = 100;
#define PAUSE(lkp, wanted)						\
		if (lock_wait_time > 0) {				\
			int i;						\
									\
			simple_unlock(&lkp->lk_interlock);		\
			for (i = lock_wait_time; i > 0; i--)		\
				if (!(wanted))				\
					break;				\
			simple_lock(&lkp->lk_interlock);		\
		}							\
		if (!(wanted))						\
			break;

#else /* ! MULTIPROCESSOR */

/*
 * It is an error to spin on a uniprocessor as nothing will ever cause
 * the simple lock to clear while we are executing.
 */
#define PAUSE(lkp, wanted)

#endif /* MULTIPROCESSOR */

/*
 * Acquire a resource.
 */
#define ACQUIRE(lkp, error, extflags, wanted)				\
	PAUSE(lkp, wanted);						\
	for (error = 0; wanted; ) {					\
		(lkp)->lk_waitcount++;					\
		simple_unlock(&(lkp)->lk_interlock);			\
		error = tsleep((void *)lkp, (lkp)->lk_prio,		\
		    (lkp)->lk_wmesg, (lkp)->lk_timo);			\
		simple_lock(&(lkp)->lk_interlock);			\
		(lkp)->lk_waitcount--;					\
		if (error)						\
			break;						\
		if ((extflags) & LK_SLEEPFAIL) {			\
			error = ENOLCK;					\
			break;						\
		}							\
	}

/*
 * Initialize a lock; required before use.
 */
void
lockinit(lkp, prio, wmesg, timo, flags)
	struct lock *lkp;
	int prio;
	const char *wmesg;
	int timo;
	int flags;
{

	memset(lkp, 0, sizeof(struct lock));
	simple_lock_init(&lkp->lk_interlock);
	lkp->lk_flags = flags & LK_EXTFLG_MASK;
	lkp->lk_prio = prio;
	lkp->lk_timo = timo;
	lkp->lk_wmesg = wmesg;
	lkp->lk_lockholder = LK_NOPROC;
}

/*
 * Determine the status of a lock.
 */
int
lockstatus(lkp)
	struct lock *lkp;
{
	int lock_type = 0;

	simple_lock(&lkp->lk_interlock);
	if (lkp->lk_exclusivecount != 0)
		lock_type = LK_EXCLUSIVE;
	else if (lkp->lk_sharecount != 0)
		lock_type = LK_SHARED;
	simple_unlock(&lkp->lk_interlock);
	return (lock_type);
}

/*
 * Set, change, or release a lock.
 *
 * Shared requests increment the shared count. Exclusive requests set the
 * LK_WANT_EXCL flag (preventing further shared locks), and wait for already
 * accepted shared locks and shared-to-exclusive upgrades to go away.
 */
int
lockmgr(lkp, flags, interlkp)
	__volatile struct lock *lkp;
	u_int flags;
	struct simplelock *interlkp;
{
	int error;
	pid_t pid;
	int extflags;
	struct proc *p = curproc;

	error = 0;
	if (p)
		pid = p->p_pid;
	else
		pid = LK_KERNPROC;
	simple_lock(&lkp->lk_interlock);
	if (flags & LK_INTERLOCK)
		simple_unlock(interlkp);
	extflags = (flags | lkp->lk_flags) & LK_EXTFLG_MASK;
#ifdef DIAGNOSTIC
	/*
	 * Once a lock has drained, the LK_DRAINING flag is set and an
	 * exclusive lock is returned. The only valid operation thereafter
	 * is a single release of that exclusive lock. This final release
	 * clears the LK_DRAINING flag and sets the LK_DRAINED flag. Any
	 * further requests of any sort will result in a panic. The bits
	 * selected for these two flags are chosen so that they will be set
	 * in memory that is freed (freed memory is filled with 0xdeadbeef).
	 * The final release is permitted to give a new lease on life to
	 * the lock by specifying LK_REENABLE.
	 */
	if (lkp->lk_flags & (LK_DRAINING|LK_DRAINED)) {
		if (lkp->lk_flags & LK_DRAINED)
			panic("lockmgr: using decommissioned lock");
		if ((flags & LK_TYPE_MASK) != LK_RELEASE ||
		    lkp->lk_lockholder != pid)
			panic("lockmgr: non-release on draining lock: %d\n",
			    flags & LK_TYPE_MASK);
		lkp->lk_flags &= ~LK_DRAINING;
		if ((flags & LK_REENABLE) == 0)
			lkp->lk_flags |= LK_DRAINED;
	}
#endif DIAGNOSTIC

	switch (flags & LK_TYPE_MASK) {

	case LK_SHARED:
		if (lkp->lk_lockholder != pid) {
			/*
			 * If just polling, check to see if we will block.
			 */
			if ((extflags & LK_NOWAIT) && (lkp->lk_flags &
			    (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE))) {
				error = EBUSY;
				break;
			}
			/*
			 * Wait for exclusive locks and upgrades to clear.
			 */
			ACQUIRE(lkp, error, extflags, lkp->lk_flags &
			    (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE));
			if (error)
				break;
			lkp->lk_sharecount++;
			COUNT(p, 1);
			break;
		}
		/*
		 * We hold an exclusive lock, so downgrade it to shared.
		 * An alternative would be to fail with EDEADLK.
		 */
		lkp->lk_sharecount++;
		COUNT(p, 1);
		/* fall into downgrade */

	case LK_DOWNGRADE:
		if (lkp->lk_lockholder != pid || lkp->lk_exclusivecount == 0)
			panic("lockmgr: not holding exclusive lock");
		lkp->lk_sharecount += lkp->lk_exclusivecount;
		lkp->lk_exclusivecount = 0;
		lkp->lk_flags &= ~LK_HAVE_EXCL;
		lkp->lk_lockholder = LK_NOPROC;
		if (lkp->lk_waitcount)
			wakeup((void *)lkp);
		break;

	case LK_EXCLUPGRADE:
		/*
		 * If another process is ahead of us to get an upgrade,
		 * then we want to fail rather than have an intervening
		 * exclusive access.
		 */
		if (lkp->lk_flags & LK_WANT_UPGRADE) {
			lkp->lk_sharecount--;
			COUNT(p, -1);
			error = EBUSY;
			break;
		}
		/* fall into normal upgrade */

	case LK_UPGRADE:
		/*
		 * Upgrade a shared lock to an exclusive one. If another
		 * shared lock has already requested an upgrade to an
		 * exclusive lock, our shared lock is released and an
		 * exclusive lock is requested (which will be granted
		 * after the upgrade). If we return an error, the file
		 * will always be unlocked.
		 */
		if (lkp->lk_lockholder == pid || lkp->lk_sharecount <= 0)
			panic("lockmgr: upgrade exclusive lock");
		lkp->lk_sharecount--;
		COUNT(p, -1);
		/*
		 * If we are just polling, check to see if we will block.
		 */
		if ((extflags & LK_NOWAIT) &&
		    ((lkp->lk_flags & LK_WANT_UPGRADE) ||
		     lkp->lk_sharecount > 1)) {
			error = EBUSY;
			break;
		}
		if ((lkp->lk_flags & LK_WANT_UPGRADE) == 0) {
			/*
			 * We are first shared lock to request an upgrade, so
			 * request upgrade and wait for the shared count to
			 * drop to zero, then take exclusive lock.
			 */
			lkp->lk_flags |= LK_WANT_UPGRADE;
			ACQUIRE(lkp, error, extflags, lkp->lk_sharecount);
			lkp->lk_flags &= ~LK_WANT_UPGRADE;
			if (error)
				break;
			lkp->lk_flags |= LK_HAVE_EXCL;
			lkp->lk_lockholder = pid;
			if (lkp->lk_exclusivecount != 0)
				panic("lockmgr: non-zero exclusive count");
			lkp->lk_exclusivecount = 1;
			COUNT(p, 1);
			break;
		}
		/*
		 * Someone else has requested upgrade. Release our shared
		 * lock, awaken upgrade requestor if we are the last shared
		 * lock, then request an exclusive lock.
		 */
		if (lkp->lk_sharecount == 0 && lkp->lk_waitcount)
			wakeup((void *)lkp);
		/* fall into exclusive request */

	case LK_EXCLUSIVE:
		if (lkp->lk_lockholder == pid && pid != LK_KERNPROC) {
			/*
			 *	Recursive lock.
			 */
			if ((extflags & LK_CANRECURSE) == 0)
				panic("lockmgr: locking against myself");
			lkp->lk_exclusivecount++;
			COUNT(p, 1);
			break;
		}
		/*
		 * If we are just polling, check to see if we will sleep.
		 */
		if ((extflags & LK_NOWAIT) && ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0)) {
			error = EBUSY;
			break;
		}
		/*
		 * Try to acquire the want_exclusive flag.
		 */
		ACQUIRE(lkp, error, extflags, lkp->lk_flags &
		    (LK_HAVE_EXCL | LK_WANT_EXCL));
		if (error)
			break;
		lkp->lk_flags |= LK_WANT_EXCL;
		/*
		 * Wait for shared locks and upgrades to finish.
		 */
		ACQUIRE(lkp, error, extflags, lkp->lk_sharecount != 0 ||
		       (lkp->lk_flags & LK_WANT_UPGRADE));
		lkp->lk_flags &= ~LK_WANT_EXCL;
		if (error)
			break;
		lkp->lk_flags |= LK_HAVE_EXCL;
		lkp->lk_lockholder = pid;
		if (lkp->lk_exclusivecount != 0)
			panic("lockmgr: non-zero exclusive count");
		lkp->lk_exclusivecount = 1;
		COUNT(p, 1);
		break;

	case LK_RELEASE:
		if (lkp->lk_exclusivecount != 0) {
			if (pid != lkp->lk_lockholder)
				panic("lockmgr: pid %d, not %s %d unlocking",
				    pid, "exclusive lock holder",
				    lkp->lk_lockholder);
			lkp->lk_exclusivecount--;
			COUNT(p, -1);
			if (lkp->lk_exclusivecount == 0) {
				lkp->lk_flags &= ~LK_HAVE_EXCL;
				lkp->lk_lockholder = LK_NOPROC;
			}
		} else if (lkp->lk_sharecount != 0) {
			lkp->lk_sharecount--;
			COUNT(p, -1);
		}
		if (lkp->lk_waitcount)
			wakeup((void *)lkp);
		break;

	case LK_DRAIN:
		/*
		 * Check that we do not already hold the lock, as it can 
		 * never drain if we do. Unfortunately, we have no way to
		 * check for holding a shared lock, but at least we can
		 * check for an exclusive one.
		 */
		if (lkp->lk_lockholder == pid)
			panic("lockmgr: draining against myself");
		/*
		 * If we are just polling, check to see if we will sleep.
		 */
		if ((extflags & LK_NOWAIT) && ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0 || lkp->lk_waitcount != 0)) {
			error = EBUSY;
			break;
		}
		PAUSE(lkp, ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0 || lkp->lk_waitcount != 0));
		for (error = 0; ((lkp->lk_flags &
		     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) ||
		     lkp->lk_sharecount != 0 || lkp->lk_waitcount != 0); ) {
			lkp->lk_flags |= LK_WAITDRAIN;
			simple_unlock(&lkp->lk_interlock);
			if ((error = tsleep((void *)&lkp->lk_flags,
			    lkp->lk_prio, lkp->lk_wmesg, lkp->lk_timo)))
				return (error);
			if ((extflags) & LK_SLEEPFAIL)
				return (ENOLCK);
			simple_lock(&lkp->lk_interlock);
		}
		lkp->lk_flags |= LK_DRAINING | LK_HAVE_EXCL;
		lkp->lk_lockholder = pid;
		lkp->lk_exclusivecount = 1;
		COUNT(p, 1);
		break;

	default:
		simple_unlock(&lkp->lk_interlock);
		panic("lockmgr: unknown locktype request %d",
		    flags & LK_TYPE_MASK);
		/* NOTREACHED */
	}
	if ((lkp->lk_flags & LK_WAITDRAIN) && ((lkp->lk_flags &
	     (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE)) == 0 &&
	     lkp->lk_sharecount == 0 && lkp->lk_waitcount == 0)) {
		lkp->lk_flags &= ~LK_WAITDRAIN;
		wakeup((void *)&lkp->lk_flags);
	}
	simple_unlock(&lkp->lk_interlock);
	return (error);
}

/*
 * Print out information about state of a lock. Used by VOP_PRINT
 * routines to display ststus about contained locks.
 */
void
lockmgr_printinfo(lkp)
	struct lock *lkp;
{

	if (lkp->lk_sharecount)
		printf(" lock type %s: SHARED (count %d)", lkp->lk_wmesg,
		    lkp->lk_sharecount);
	else if (lkp->lk_flags & LK_HAVE_EXCL)
		printf(" lock type %s: EXCL (count %d) by pid %d",
		    lkp->lk_wmesg, lkp->lk_exclusivecount, lkp->lk_lockholder);
	if (lkp->lk_waitcount > 0)
		printf(" with %d pending", lkp->lk_waitcount);
}

#if defined(LOCKDEBUG) && !defined(MULTIPROCESSOR)
#include <sys/kernel.h>
#include <vm/vm.h>
#include <sys/sysctl.h>
int lockpausetime = 0;
struct ctldebug debug2 = { "lockpausetime", &lockpausetime };
int simplelockrecurse;
LIST_HEAD(slocklist, simplelock) slockdebuglist;

/*
 * Simple lock functions so that the debugger can see from whence
 * they are being called.
 */
void
simple_lock_init(alp)
	struct simplelock *alp;
{
	alp->lock_data = 0;
	alp->lock_file = NULL;
	alp->lock_line = 0;
	alp->unlock_file = NULL;
	alp->unlock_line = 0;
	alp->lock_holder = 0;
}

void
_simple_lock(alp, id, l)
	__volatile struct simplelock *alp;
	const char *id;
	int l;
{
	int s;

	if (simplelockrecurse)
		return;
	if (alp->lock_data == 1) {
		printf("simple_lock: lock held\n");
		printf("currently at: %s:%d\n", id, l);
		printf("last locked: %s:%d\n",
		       alp->lock_file, alp->lock_line);
		printf("last unlocked: %s:%d\n",
		       alp->unlock_file, alp->unlock_line);
		if (lockpausetime == -1)
			panic("simple_lock: lock held");
		if (lockpausetime == 1) {
#ifdef BACKTRACE
			BACKTRACE(curproc);
#endif
		} else if (lockpausetime > 1) {
			printf("simple_lock: lock held, pausing...");
			tsleep(&lockpausetime, PCATCH | PPAUSE, "slock",
			    lockpausetime * hz);
			printf(" continuing\n");
		}
		return;
	}

	s = splhigh();
	LIST_INSERT_HEAD(&slockdebuglist, (struct simplelock *)alp, list);
	splx(s);

	alp->lock_data = 1;
	alp->lock_file = id;
	alp->lock_line = l;
	if (curproc)
		curproc->p_simple_locks++;
}

int
_simple_lock_try(alp, id, l)
	__volatile struct simplelock *alp;
	const char *id;
	int l;
{
	int s;

	if (alp->lock_data)
		return (0);
	if (simplelockrecurse)
		return (1);
	alp->lock_data = 1;
	alp->lock_file = id;
	alp->lock_line = l;

	s = splhigh();
	LIST_INSERT_HEAD(&slockdebuglist, (struct simplelock *)alp, list);
	splx(s);

	if (curproc)
		curproc->p_simple_locks++;
	return (1);
}

void
_simple_unlock(alp, id, l)
	__volatile struct simplelock *alp;
	const char *id;
	int l;
{
	int s;

	if (simplelockrecurse)
		return;
	if (alp->lock_data == 0) {
		printf("simple_unlock: lock not held\n");
		printf("currently at: %s:%d\n", id, l);
		printf("last locked: %s:%d\n",
		       alp->lock_file, alp->lock_line);
		printf("last unlocked: %s:%d\n",
		       alp->unlock_file, alp->unlock_line);
		if (lockpausetime == -1)
			panic("simple_unlock: lock not held");
		if (lockpausetime == 1) {
#ifdef BACKTRACE
			BACKTRACE(curproc);
#endif
		} else if (lockpausetime > 1) {
			printf("simple_unlock: lock not held, pausing...");
			tsleep(&lockpausetime, PCATCH | PPAUSE, "sunlock",
			    lockpausetime * hz);
			printf(" continuing\n");
		}
		return;
	}

	s = splhigh();
	LIST_REMOVE(alp, list);
	alp->list.le_next = NULL;
	alp->list.le_prev = NULL;
	splx(s);

	alp->lock_data = 0;
	alp->unlock_file = id;
	alp->unlock_line = l;
	if (curproc)
		curproc->p_simple_locks--;
}

void
_simple_lock_assert(alp, value, id, l)
	__volatile struct simplelock *alp;
	int value;
	const char *id;
	int l;
{
	if (alp->lock_data != value) {
		panic("lock %p: value %d != expected %d at %s:%d",
		      alp, alp->lock_data, value, id, l);
	}
}


void
simple_lock_dump()
{
	struct simplelock *alp;
	int s;

	s = splhigh();
	printf("all simple locks:\n");
	for (alp = LIST_FIRST(&slockdebuglist);
	     alp != NULL;
	     alp = LIST_NEXT(alp, list)) {
		printf("%p  %s:%d\n", alp, alp->lock_file, alp->lock_line);
	}
	splx(s);
}

void
simple_lock_freecheck(start, end)
void *start, *end;
{
	struct simplelock *alp;
	int s;

	s = splhigh();
	for (alp = LIST_FIRST(&slockdebuglist);
	     alp != NULL;
	     alp = LIST_NEXT(alp, list)) {
		if ((void *)alp >= start && (void *)alp < end) {
			printf("freeing simple_lock %p\n", alp);
#ifdef DDB
			Debugger();
#endif
		}
	}
	splx(s);
}
#endif /* LOCKDEBUG && ! MULTIPROCESSOR */
