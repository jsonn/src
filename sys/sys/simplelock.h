/*	$NetBSD: simplelock.h,v 1.3.14.1 2007/10/14 11:49:10 yamt Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2006, 2007 The NetBSD Foundation, Inc.
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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef	_SYS_SIMPLELOCK_H_
#define	_SYS_SIMPLELOCK_H_

#if defined(_KERNEL_OPT)
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#endif

#include <machine/types.h>
#include <machine/lock.h>

/*
 * The simple lock.  Provides a simple spinning mutex.  Note the
 * member which is used in atomic operations must be aligned in
 * order for it to work on the widest range of processor types.
 */
struct simplelock {
	__cpu_simple_lock_t lock_data;
#ifdef __CPU_SIMPLE_LOCK_PAD
	/*
	 * For binary compatibility where the lock word has been
	 * made shorter.
	 */
	uint8_t lock_pad[3];
#endif
};

/*
 * Indicator that no process/cpu holds exclusive lock
 */
#define	LK_KERNPROC	((pid_t) -2)
#define	LK_NOPROC	((pid_t) -1)
#define	LK_NOCPU	((cpuid_t) -1)

#define	SIMPLELOCK_INITIALIZER	{ .lock_data = __SIMPLELOCK_UNLOCKED }

#ifdef _KERNEL

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
#define	simple_lock_init(alp)	__cpu_simple_lock_init(&(alp)->lock_data)
#define	simple_lock(alp)	__cpu_simple_lock(&(alp)->lock_data)
#define	simple_lock_held(alp)	(__SIMPLELOCK_LOCKED_P(&(alp)->lock_data))
#define	simple_lock_try(alp)	__cpu_simple_lock_try(&(alp)->lock_data)
#define	simple_unlock(alp)	__cpu_simple_unlock(&(alp)->lock_data)
#else
#define	simple_lock_nothing(alp) 	\
do {					\
	(void)alp;			\
} while (0);
#define	simple_lock_init(alp)	simple_lock_nothing(alp)
#define	simple_lock(alp)	simple_lock_nothing(alp)
#define	simple_lock_held(alp)	1
#define	simple_lock_try(alp)	1
#define	simple_unlock(alp)	simple_lock_nothing(alp)
#endif

#define	simple_lock_only_held(x,y)			/* nothing */
#define simple_lock_assert_locked(alp,lockname)		/* nothing */
#define simple_lock_assert_unlocked(alp,lockname)	/* nothing */

#ifdef LOCKDEBUG
#define	LOCK_ASSERT(x)		KASSERT(x)
#else
#define	LOCK_ASSERT(x)		/* nothing */
#endif

#endif	/* _KERNEL */

#endif	/* _SYS_SIMPLELOCK_H_ */
