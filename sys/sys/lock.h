/*	$NetBSD: lock.h,v 1.27.2.1 2000/06/22 17:10:23 minoura Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
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
 *	@(#)lock.h	8.12 (Berkeley) 5/19/95
 */

#ifndef	_SYS_LOCK_H_
#define	_SYS_LOCK_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#endif

#include <sys/queue.h>
#include <machine/lock.h>

/*
 * The simple lock.  Provides a simple spinning mutex.  Note the
 * member which is used in atomic operations must be aligned in
 * order for it to work on the widest range of processor types.
 */
struct simplelock {
	__cpu_simple_lock_t lock_data;
#ifdef LOCKDEBUG
	const char *lock_file;
	const char *unlock_file;
	short lock_line;
	short unlock_line;
	TAILQ_ENTRY(simplelock) list;
	cpuid_t lock_holder;		/* CPU ID */
#endif
};

#ifdef LOCKDEBUG
#define	SIMPLELOCK_INITIALIZER	{ __SIMPLELOCK_UNLOCKED, NULL, NULL, 0,	\
				  0, { NULL, NULL }, 0 }
#else
#define	SIMPLELOCK_INITIALIZER	{ __SIMPLELOCK_UNLOCKED }
#endif

/* XXXCDC: kill typedefs later? */
typedef struct simplelock       simple_lock_data_t;
typedef struct simplelock       *simple_lock_t;
typedef struct lock             lock_data_t;
typedef struct lock             *lock_t;

/*
 * The general lock structure.  Provides for multiple shared locks,
 * upgrading from shared to exclusive, and sleeping/spinning until the
 * lock can be gained.
 */
struct lock {
	struct	simplelock lk_interlock; /* lock on remaining fields */
	u_int	lk_flags;		/* see below */
	int	lk_sharecount;		/* # of accepted shared locks */
	short	lk_exclusivecount;	/* # of recursive exclusive locks */
	short	lk_recurselevel;	/* lvl above which recursion ok */
	int	lk_waitcount;		/* # of sleepers/spinners */

	/*
	 * This is the sleep message for sleep locks, and a simple name
	 * for spin locks.
	 */
	const char *lk_wmesg;

	union {
		struct {
			/* pid of exclusive lock holder */
			pid_t lk_sleep_lockholder;

			/* priority at which to sleep */
			int lk_sleep_prio;

			/* maximum sleep time (for tsleep) */
			int lk_sleep_timo;
		} lk_un_sleep;
		struct {
			/* CPU ID of exclusive lock holder */
			cpuid_t lk_spin_cpu;
#if defined(LOCKDEBUG)
			TAILQ_ENTRY(lock) lk_spin_list;
#endif
		} lk_un_spin;
	} lk_un;

#define	lk_lockholder	lk_un.lk_un_sleep.lk_sleep_lockholder
#define	lk_prio		lk_un.lk_un_sleep.lk_sleep_prio
#define	lk_timo		lk_un.lk_un_sleep.lk_sleep_timo

#define	lk_cpu		lk_un.lk_un_spin.lk_spin_cpu
#if defined(LOCKDEBUG)
#define	lk_list		lk_un.lk_un_spin.lk_spin_list
#endif
};

/*
 * Lock request types:
 *   LK_SHARED - get one of many possible shared locks. If a process
 *	holding an exclusive lock requests a shared lock, the exclusive
 *	lock(s) will be downgraded to shared locks.
 *   LK_EXCLUSIVE - stop further shared locks, when they are cleared,
 *	grant a pending upgrade if it exists, then grant an exclusive
 *	lock. Only one exclusive lock may exist at a time, except that
 *	a process holding an exclusive lock may get additional exclusive
 *	locks if it explicitly sets the LK_CANRECURSE flag in the lock
 *	request, or if the LK_CANRECUSE flag was set when the lock was
 *	initialized.
 *   LK_UPGRADE - the process must hold a shared lock that it wants to
 *	have upgraded to an exclusive lock. Other processes may get
 *	exclusive access to the resource between the time that the upgrade
 *	is requested and the time that it is granted.
 *   LK_EXCLUPGRADE - the process must hold a shared lock that it wants to
 *	have upgraded to an exclusive lock. If the request succeeds, no
 *	other processes will have gotten exclusive access to the resource
 *	between the time that the upgrade is requested and the time that
 *	it is granted. However, if another process has already requested
 *	an upgrade, the request will fail (see error returns below).
 *   LK_DOWNGRADE - the process must hold an exclusive lock that it wants
 *	to have downgraded to a shared lock. If the process holds multiple
 *	(recursive) exclusive locks, they will all be downgraded to shared
 *	locks.
 *   LK_RELEASE - release one instance of a lock.
 *   LK_DRAIN - wait for all activity on the lock to end, then mark it
 *	decommissioned. This feature is used before freeing a lock that
 *	is part of a piece of memory that is about to be freed.
 *
 * These are flags that are passed to the lockmgr routine.
 */
#define	LK_TYPE_MASK	0x0000000f	/* type of lock sought */
#define	LK_SHARED	0x00000001	/* shared lock */
#define	LK_EXCLUSIVE	0x00000002	/* exclusive lock */
#define	LK_UPGRADE	0x00000003	/* shared-to-exclusive upgrade */
#define	LK_EXCLUPGRADE	0x00000004	/* first shared-to-exclusive upgrade */
#define	LK_DOWNGRADE	0x00000005	/* exclusive-to-shared downgrade */
#define	LK_RELEASE	0x00000006	/* release any type of lock */
#define	LK_DRAIN	0x00000007	/* wait for all lock activity to end */
/*
 * External lock flags.
 *
 * The first three flags may be set in lock_init to set their mode permanently,
 * or passed in as arguments to the lock manager. The LK_REENABLE flag may be
 * set only at the release of a lock obtained by drain.
 */
#define	LK_EXTFLG_MASK	0x00700070	/* mask of external flags */
#define	LK_NOWAIT	0x00000010	/* do not sleep to await lock */
#define	LK_SLEEPFAIL	0x00000020	/* sleep, then return failure */
#define	LK_CANRECURSE	0x00000040	/* this may be recursive lock attempt */
#define	LK_REENABLE	0x00000080	/* lock is be reenabled after drain */
#define	LK_SETRECURSE	0x00100000	/* other locks while we have it OK */
#define	LK_RECURSEFAIL  0x00200000	/* attempt at recursive lock fails */
#define	LK_SPIN		0x00400000	/* lock spins instead of sleeps */
/*
 * Internal lock flags.
 *
 * These flags are used internally to the lock manager.
 */
#define	LK_WANT_UPGRADE	0x00000100	/* waiting for share-to-excl upgrade */
#define	LK_WANT_EXCL	0x00000200	/* exclusive lock sought */
#define	LK_HAVE_EXCL	0x00000400	/* exclusive lock obtained */
#define	LK_WAITDRAIN	0x00000800	/* process waiting for lock to drain */
#define	LK_DRAINING	0x00004000	/* lock is being drained */
#define	LK_DRAINED	0x00008000	/* lock has been decommissioned */
/*
 * Control flags
 *
 * Non-persistent external flags.
 */
#define	LK_INTERLOCK	0x00010000	/* unlock passed simple lock after
					   getting lk_interlock */
#define	LK_RETRY	0x00020000	/* vn_lock: retry until locked */

/*
 * Lock return status.
 *
 * Successfully obtained locks return 0. Locks will always succeed
 * unless one of the following is true:
 *	LK_FORCEUPGRADE is requested and some other process has already
 *	    requested a lock upgrade (returns EBUSY).
 *	LK_WAIT is set and a sleep would be required (returns EBUSY).
 *	LK_SLEEPFAIL is set and a sleep was done (returns ENOLCK).
 *	PCATCH is set in lock priority and a signal arrives (returns
 *	    either EINTR or ERESTART if system calls is to be restarted).
 *	Non-null lock timeout and timeout expires (returns EWOULDBLOCK).
 * A failed lock attempt always returns a non-zero error value. No lock
 * is held after an error return (in particular, a failed LK_UPGRADE
 * or LK_FORCEUPGRADE will have released its shared access lock).
 */

/*
 * Indicator that no process/cpu holds exclusive lock
 */
#define	LK_KERNPROC	((pid_t) -2)
#define	LK_NOPROC	((pid_t) -1)
#define	LK_NOCPU	((cpuid_t) -1)

#ifdef _KERNEL

struct proc;

void	lockinit __P((struct lock *, int prio, const char *wmesg, int timo,
			int flags));
int	lockmgr __P((__volatile struct lock *, u_int flags,
			struct simplelock *));
int	lockstatus __P((struct lock *));
void	lockmgr_printinfo __P((__volatile struct lock *));

#define	spinlockinit(lkp, name, flags)					\
	lockinit((lkp), 0, (name), 0, (flags) | LK_SPIN)

#define	spinlockmgr(lkp, flags, intrlk)					\
	lockmgr((lkp), (flags) | LK_SPIN, (intrlk))

#if defined(LOCKDEBUG)
void	_simple_lock __P((__volatile struct simplelock *, const char *, int));
int	_simple_lock_try __P((__volatile struct simplelock *, const char *,
	    int));
void	_simple_unlock __P((__volatile struct simplelock *, const char *, int));

#define	simple_lock(alp)	_simple_lock((alp), __FILE__, __LINE__)
#define	simple_lock_try(alp)	_simple_lock_try((alp), __FILE__, __LINE__)
#define	simple_unlock(alp)	_simple_unlock((alp), __FILE__, __LINE__)

void	simple_lock_init __P((struct simplelock *));
void	simple_lock_dump __P((void));
void	simple_lock_freecheck __P((void *, void *));
#elif defined(MULTIPROCESSOR)
#define	simple_lock_init(alp)	__cpu_simple_lock_init(&(alp)->lock_data)
#define	simple_lock(alp)	__cpu_simple_lock(&(alp)->lock_data)
#define	simple_lock_try(alp)	__cpu_simple_lock_try(&(alp)->lock_data)
#define	simple_unlock(alp)	__cpu_simple_unlock(&(alp)->lock_data)
#else
#define	simple_lock_init(alp)	(alp)->lock_data = __SIMPLELOCK_UNLOCKED
#define	simple_lock(alp)		/* nothing */
#define	simple_lock_try(alp)	(1)	/* always succeeds */
#define	simple_unlock(alp)		/* nothing */
#endif

#endif /* _KERNEL */

#endif /* _SYS_LOCK_H_ */
