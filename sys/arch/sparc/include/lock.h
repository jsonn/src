/*	$NetBSD: lock.h,v 1.6.2.1 2000/11/20 20:25:39 bouyer Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#ifndef _MACHINE_LOCK_H
#define _MACHINE_LOCK_H

/*
 * Machine dependent spin lock operations.
 */

/*
 * The value for __SIMPLELOCK_LOCKED is what ldstub() naturally stores
 * `lock_data' given its address (and the fact that SPARC is big-endian).
 */

typedef	__volatile int		__cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	0xff000000
#define	__SIMPLELOCK_UNLOCKED	0

/* XXX So we can expose this to userland. */
#ifdef __lint__
#define __ldstub(__addr)	(__addr)
#else /* !__lint__ */
#define	__ldstub(__addr)						\
({									\
	int __v;							\
									\
	__asm __volatile("ldstub [%1],%0"				\
	    : "=r" (__v)						\
	    : "r" (__addr)						\
	    : "memory");						\
									\
	__v;								\
})
#endif /* __lint__ */

static __inline void __cpu_simple_lock_init __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline void __cpu_simple_lock __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline int __cpu_simple_lock_try __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));
static __inline void __cpu_simple_unlock __P((__cpu_simple_lock_t *))
	__attribute__((__unused__));

static __inline void
__cpu_simple_lock_init(__cpu_simple_lock_t *alp)
{

	*alp = __SIMPLELOCK_UNLOCKED;
}

static __inline void
__cpu_simple_lock(__cpu_simple_lock_t *alp)
{

	/*
	 * If someone else holds the lock use simple reads until it
	 * is released, then retry the atomic operation. This reduces
	 * memory bus contention because the cache-coherency logic
	 * does not have to broadcast invalidates on the lock while
	 * we spin on it.
	 */
	while (__ldstub(alp) != __SIMPLELOCK_UNLOCKED) {
		while (*alp != __SIMPLELOCK_UNLOCKED)
			/* spin */ ;
	}
}

static __inline int
__cpu_simple_lock_try(__cpu_simple_lock_t *alp)
{

	return (__ldstub(alp) == __SIMPLELOCK_UNLOCKED);
}

static __inline void
__cpu_simple_unlock(__cpu_simple_lock_t *alp)
{

	*alp = __SIMPLELOCK_UNLOCKED;
}

#endif /* _MACHINE_LOCK_H */
