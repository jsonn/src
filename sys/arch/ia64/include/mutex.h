/*	$NetBSD: mutex.h,v 1.2.8.2 2008/04/03 12:42:19 mjf Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takayoshi Kochi.
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

#ifndef _IA64_MUTEX_H_
#define	_IA64_MUTEX_H_

struct kmutex {
	union {
		volatile uintptr_t	mtxa_owner;
#ifdef __MUTEX_PRIVATE
		struct {
			volatile uint8_t	mtxs_dummy;
			ipl_cookie_t		mtxs_ipl;
			__cpu_simple_lock_t	mtxs_lock;
			volatile uint8_t	mtxs_unused;
		} s;
#endif
	} u;
};

#ifdef __MUTEX_PRIVATE

/* uintptr_t */
#define	mtx_owner	u.mtxa_owner
/* ipl_cookie_t */
#define	mtx_ipl		u.s.mtxs_ipl
/* __cpu_simple_lock_t */
#define	mtx_lock	u.s.mtxs_lock

/* XXX when we implement mutex_enter()/mutex_exit(), uncomment this
#define __HAVE_MUTEX_STUBS		1
*/
/* XXX when we implement mutex_spin_enter()/mutex_spin_exit(), uncomment this
#define __HAVE_SPIN_MUTEX_STUBS		1
*/
#define	__HAVE_SIMPLE_MUTEXES		1

/*
 * MUTEX_RECEIVE: no memory barrier required, atomic_cas implies a load fence.
 */
#define	MUTEX_RECEIVE(mtx)		/* nothing */

/*
 * MUTEX_GIVE: no memory barrier required, as _lock_cas() will take care of it.
 */
#define	MUTEX_GIVE(mtx)			/* nothing */

#define	MUTEX_CAS(ptr, old, new)		\
    (atomic_cas_ulong((volatile unsigned long *)(ptr), (old), (new)) == (old))

#endif	/* __MUTEX_PRIVATE */

#endif	/* _IA64_MUTEX_H_ */
