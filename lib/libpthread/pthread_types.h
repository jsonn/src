/*	$NetBSD: pthread_types.h,v 1.1.2.4 2001/07/25 21:24:13 nathanw Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nathan J. Williams.
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

#ifndef _LIB_PTHREAD_TYPES_H
#define _LIB_PTHREAD_TYPES_H

#include <machine/lock.h>
#include "pthread_queue.h"

typedef __cpu_simple_lock_t	pt_spin_t;

PTQ_HEAD(pt_queue_t, pthread_st);

struct	pthread_st;
struct	pthread_attr_st;
struct	pthread_mutex_st;
struct	pthread_mutexattr_st;
struct	pthread_cond_st;
struct	pthread_condattr_st;

typedef struct pthread_st *pthread_t;
typedef struct pthread_attr_st pthread_attr_t;
typedef struct pthread_mutex_st pthread_mutex_t;
typedef struct pthread_mutexattr_st pthread_mutexattr_t;
typedef struct pthread_cond_st pthread_cond_t;
typedef struct pthread_condattr_st pthread_condattr_t;


struct	pthread_attr_st {
	unsigned int	pta_magic;

	int	pta_flags;
};

struct	pthread_mutex_st {
	unsigned int	ptm_magic;

	/* Not a real spinlock; will never be spun on. Locked with
	 * __cpu_simple_lock_try() or not at all. Therefore, not
	 * subject to preempted-spinlock-continuation.
	 * 
	 * Open research question: Would it help threaded applications if
	 * preempted-lock-continuation were applied to mutexes?
	 */
	pt_spin_t	ptm_lock; 

	/* Protects the owner and blocked queue */
	pt_spin_t	ptm_interlock;
	pthread_t	ptm_owner;
	struct pt_queue_t	ptm_blocked;
};

#define	_PT_MUTEX_MAGIC	0xCAFEFEED
#define	_PT_MUTEX_DEAD	0xFEEDDEAD

#define PTHREAD_MUTEX_INITIALIZER { PT_MUTEX_MAGIC, 			\
				    __SIMPLELOCK_UNLOCKED,		\
				    __SIMPLELOCK_UNLOCKED,		\
				    NULL,				\
				    PTQ_HEAD_INITIALIZER		\
				  }
	

struct	pthread_mutexattr_st {
	int	pad;
};


struct	pthread_cond_st {
	unsigned int	ptc_magic;

	/* Protects the queue of waiters */
	pt_spin_t	ptc_lock;
	struct pt_queue_t	ptc_waiters;

	pthread_mutex_t	*ptc_mutex;	/* Current mutex */
};

#define	_PT_COND_MAGIC	0xCAFEC00D
#define	_PT_COND_DEAD	0xDEADC00D

struct	pthread_condattr_st {
	int	pad;
};

#endif	/* _LIB_PTHREAD_TYPES_H */
