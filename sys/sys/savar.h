/*	$Id: savar.h,v 1.1.2.7 2001/11/17 21:20:10 thorpej Exp $	*/

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

/*
 * Internal data usd by the scheduler activation implementation
 */

#ifndef _SYS_SAVAR_H
#define _SYS_SAVAR_H

#include <sys/lock.h>
#include <sys/queue.h>

struct sadata_upcall {
	LIST_ENTRY(sadata_upcall)	sau_next;
	int	sau_type;
	size_t	sau_argsize;
	void	*sau_arg;
	stack_t	sau_stack;
	struct lwp	*sau_event;
	struct lwp	*sau_interrupted;
};

struct sadata {
	struct simplelock sa_lock;	/* lock on these fields */
	sa_upcall_t	sa_upcall;	/* upcall entry point */
	int	sa_concurrency;		/* desired concurrency */
	LIST_HEAD(, lwp)	sa_lwpcache;	/* list of avaliable lwps */
	int	sa_ncached;		/* list length */
	stack_t	*sa_stacks;		/* pointer to array of upcall stacks */
	int	sa_nstackentries;	/* size of the array */
	int	sa_nstacks;		/* number of valid stacks */
	LIST_HEAD(, sadata_upcall)	sa_upcalls; /* pending upcalls */
};

extern struct pool sadata_pool;		/* memory pool for sadata structures */
extern struct pool saupcall_pool;	/* memory pool for pending upcalls */

#define SA_NUMSTACKS	16	/* Number of stacks allocated. XXX */

struct sadata_upcall *sadata_upcall_alloc(void);
void	sadata_upcall_free(struct sadata_upcall *);

void	sa_switch(struct lwp *, int);
void	sa_switchcall(void *);
int	sa_upcall(struct lwp *, int, struct lwp *, struct lwp *, size_t, void *);
void	cpu_upcall(struct lwp *);
ucontext_t *cpu_stashcontext(struct lwp *);


#endif /* !_SYS_SAVAR_H */
