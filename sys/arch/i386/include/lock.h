/*	$NetBSD: lock.h,v 1.1.2.5 2001/04/30 16:58:34 sommerfeld Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Machine-dependent spin lock operations.
 */

#ifndef _I386_LOCK_H_
#define	_I386_LOCK_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_lockdebug.h"
#endif

typedef	__volatile int		__cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

#ifdef LOCKDEBUG

extern void __cpu_simple_lock_init __P((__cpu_simple_lock_t *));
extern void __cpu_simple_lock __P((__cpu_simple_lock_t *));
extern int __cpu_simple_lock_try __P((__cpu_simple_lock_t *));
extern void __cpu_simple_unlock __P((__cpu_simple_lock_t *));

#else

#include <machine/atomic.h>

static __inline void __cpu_simple_lock_init __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline void __cpu_simple_lock __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline int __cpu_simple_lock_try __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline void __cpu_simple_unlock __P((__cpu_simple_lock_t *)) 
	__attribute__((__unused__));

static __inline void
__cpu_simple_lock_init(__cpu_simple_lock_t *lockp)
{
	*lockp = __SIMPLELOCK_UNLOCKED;
}

static __inline void
__cpu_simple_lock(__cpu_simple_lock_t *lockp)
{
	while (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_LOCKED) {
		continue;	/* spin */
	}
}

static __inline int
__cpu_simple_lock_try(__cpu_simple_lock_t *lockp)
{
	return (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_UNLOCKED);
}

static __inline void
__cpu_simple_unlock(__cpu_simple_lock_t *lockp)
{

	*lockp = __SIMPLELOCK_UNLOCKED;
}

#endif /* !LOCKDEBUG */

#endif /* _I386_LOCK_H_ */
