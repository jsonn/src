/*	$NetBSD: callcontext.c,v 1.7.4.1 2007/11/06 23:11:50 matt Exp $	*/

/*
 * Copyright (c) 2006 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: callcontext.c,v 1.7.4.1 2007/11/06 23:11:50 matt Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#ifdef PUFFS_WITH_THREADS
#include <pthread.h>
#endif
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "puffs_priv.h"

/*
 * user stuff
 */

void
puffs_cc_yield(struct puffs_cc *pcc)
{

	assert(pcc->pcc_flags & PCC_REALCC);
	pcc->pcc_flags &= ~PCC_BORROWED;

	/* romanes eunt domus */
	swapcontext(&pcc->pcc_uc, &pcc->pcc_uc_ret);
}

void
puffs_cc_continue(struct puffs_cc *pcc)
{

	assert(pcc->pcc_flags & PCC_REALCC);

	/* ramble on */
	swapcontext(&pcc->pcc_uc_ret, &pcc->pcc_uc);
}

/*
 * "Borrows" pcc, *NOT* called from pcc owner.  Acts like continue.
 * So the idea is to use this, give something the context back to
 * run to completion and then jump back to where ever this was called
 * from after the op dispatching is complete (or if the pcc decides to
 * yield again).
 */
void
puffs_goto(struct puffs_cc *loanpcc)
{

	assert(loanpcc->pcc_flags & PCC_REALCC);
	loanpcc->pcc_flags |= PCC_BORROWED;

	swapcontext(&loanpcc->pcc_uc_ret, &loanpcc->pcc_uc);
}

void
puffs_cc_schedule(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;

	assert(pu->pu_state & PU_INLOOP);
	TAILQ_INSERT_TAIL(&pu->pu_sched, pcc, entries);
}

struct puffs_usermount *
puffs_cc_getusermount(struct puffs_cc *pcc)
{

	return pcc->pcc_pu;
}

void *
puffs_cc_getspecific(struct puffs_cc *pcc)
{

	return puffs_getspecific(pcc->pcc_pu);
}

int
puffs_cc_getcaller(struct puffs_cc *pcc, pid_t *pid, lwpid_t *lid)
{

	if ((pcc->pcc_flags & PCC_HASCALLER) == 0) {
		errno = ESRCH;
		return -1;
	}

	if (pid)
		*pid = pcc->pcc_pid;
	if (lid)
		*lid = pcc->pcc_lid;
	return 0;
}

#ifdef PUFFS_WITH_THREADS
int pthread__stackid_setup(void *, size_t, pthread_t *);
#endif

int
puffs_cc_create(struct puffs_usermount *pu, struct puffs_req *preq,
	int type, struct puffs_cc **pccp)
{
	struct puffs_cc *volatile pcc;
	size_t stacksize = 1<<pu->pu_cc_stackshift;
	stack_t *st;
	void *sp;

#ifdef PUFFS_WITH_THREADS
	extern size_t pthread__stacksize;
	stacksize = pthread__stacksize;
#endif

	pcc = malloc(sizeof(struct puffs_cc));
	if (!pcc)
		return -1;
	memset(pcc, 0, sizeof(struct puffs_cc));
	pcc->pcc_pu = pu;
	pcc->pcc_preq = preq;

	pcc->pcc_flags = type;
	if (pcc->pcc_flags != PCC_REALCC)
		goto out;

	/* initialize both ucontext's */
	if (getcontext(&pcc->pcc_uc) == -1) {
		free(pcc);
		return -1;
	}
	if (getcontext(&pcc->pcc_uc_ret) == -1) {
		free(pcc);
		return -1;
	}
	/* return here.  it won't actually be "here" due to swapcontext() */
	pcc->pcc_uc.uc_link = &pcc->pcc_uc_ret;

	/* allocate stack for execution */
	st = &pcc->pcc_uc.uc_stack;
	sp = mmap(NULL, stacksize, PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE|MAP_ALIGNED(pu->pu_cc_stackshift), -1, 0);
	if (sp == MAP_FAILED) {
		free(pcc);
		return -1;
	}
	st->ss_sp = pcc->pcc_stack = sp;
	st->ss_size = stacksize;
	st->ss_flags = 0;

#ifdef PUFFS_WITH_THREADS
	{
	pthread_t pt;
	extern int __isthreaded;
	if (__isthreaded)
		pthread__stackid_setup(sp, stacksize, &pt);
	}
#endif

	/*
	 * Give us an initial context to jump to.
	 *
	 * XXX: Our manual page says that portable code shouldn't rely on
	 * being able to pass pointers through makecontext().  kjk says
	 * that NetBSD code doesn't need to worry about this.  uwe says
	 * it would be like putting a "keep away from children" sign on a
	 * box of toys.  I didn't ask what simon says; he's probably busy
	 * "fixing" typos in comments.
	 */
	makecontext(&pcc->pcc_uc, (void *)puffs_calldispatcher,
	    1, (uintptr_t)pcc);

 out:
	*pccp = pcc;
	return 0;
}

void
puffs_cc_setcaller(struct puffs_cc *pcc, pid_t pid, lwpid_t lid)
{

	pcc->pcc_pid = pid;
	pcc->pcc_lid = lid;
	pcc->pcc_flags |= PCC_HASCALLER;
}

void
puffs_cc_destroy(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	size_t stacksize = 1<<pu->pu_cc_stackshift;

#ifdef PUFFS_WITH_THREADS
	extern size_t pthread__stacksize;
	stacksize = pthread__stacksize;
#endif

	if (pcc->pcc_flags & PCC_REALCC)
		munmap(pcc->pcc_stack, stacksize);
	free(pcc->pcc_preq);
	free(pcc);
}
