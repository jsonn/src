/*	$NetBSD: pthread_spin.c,v 1.1.4.3 2007/09/10 11:06:21 skrll Exp $	*/

/*-
 * Copyright (c) 2001, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams and Andrew Doran.
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
 * Public (POSIX-specified) spinlocks.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: pthread_spin.c,v 1.1.4.3 2007/09/10 11:06:21 skrll Exp $");

#include <sys/types.h>
#include <sys/lock.h>
#include <sys/ras.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "pthread.h"
#include "pthread_int.h"

int
pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{

#ifdef ERRORCHECK
	if (lock == NULL || (pshared != PTHREAD_PROCESS_PRIVATE &&
	    pshared != PTHREAD_PROCESS_SHARED))
		return EINVAL;
#endif
	lock->pts_magic = _PT_SPINLOCK_MAGIC;

	/*
	 * We don't actually use the pshared flag for anything;
	 * CPU simple locks have all the process-shared properties 
	 * that we want anyway.
	 */
	lock->pts_flags = pshared;
	pthread_lockinit(&lock->pts_spin);

	return 0;
}

int
pthread_spin_destroy(pthread_spinlock_t *lock)
{

#ifdef ERRORCHECK
	if (lock == NULL || lock->pts_magic != _PT_SPINLOCK_MAGIC)
		return EINVAL;
	if (!__SIMPLELOCK_UNLOCKED_P(&lock->pts_spin))
		return EBUSY;
#endif

	lock->pts_magic = _PT_SPINLOCK_DEAD;

	return 0;
}

int
pthread_spin_lock(pthread_spinlock_t *lock)
{

#ifdef ERRORCHECK
	if (lock == NULL || lock->pts_magic != _PT_SPINLOCK_MAGIC)
		return EINVAL;
#endif

	while (pthread__simple_lock_try(&lock->pts_spin) == 0) {
		pthread__smt_pause();
	}

	return 0;
}

int
pthread_spin_trylock(pthread_spinlock_t *lock)
{

#ifdef ERRORCHECK
	if (lock == NULL || lock->pts_magic != _PT_SPINLOCK_MAGIC)
		return EINVAL;
#endif

	if (pthread__simple_lock_try(&lock->pts_spin) == 0)
		return EBUSY;

	return 0;
}

int
pthread_spin_unlock(pthread_spinlock_t *lock)
{

#ifdef ERRORCHECK
	if (lock == NULL || lock->pts_magic != _PT_SPINLOCK_MAGIC)
		return EINVAL;
#endif

	pthread__simple_unlock(&lock->pts_spin);

	return 0;
}
