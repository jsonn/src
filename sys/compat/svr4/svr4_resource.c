/*	$NetBSD: svr4_resource.c,v 1.16.10.1 2009/04/01 00:25:22 snj Exp $	 */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: svr4_resource.c,v 1.16.10.1 2009/04/01 00:25:22 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_resource.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_lwp.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>

static inline int svr4_to_native_rl(int);

static inline int
svr4_to_native_rl(int rl)
{
	switch (rl) {
	case SVR4_RLIMIT_CPU:
		return RLIMIT_CPU;
	case SVR4_RLIMIT_FSIZE:
		return RLIMIT_FSIZE;
	case SVR4_RLIMIT_DATA:
		return RLIMIT_DATA;
	case SVR4_RLIMIT_STACK:
		return RLIMIT_STACK;
	case SVR4_RLIMIT_CORE:
		return RLIMIT_CORE;
	case SVR4_RLIMIT_NOFILE:
		return RLIMIT_NOFILE;
	case SVR4_RLIMIT_VMEM:
		return RLIMIT_AS;
	default:
		return -1;
	}
}

/*
 * Check if the resource limit fits within the BSD range and it is not
 * one of the magic SVR4 limit values
 */
#define OKLIMIT(l) (((int32_t)(l)) >= 0 && ((int32_t)(l)) < 0x7fffffff && \
	((svr4_rlim_t)(l)) != SVR4_RLIM_INFINITY && \
	((svr4_rlim_t)(l)) != SVR4_RLIM_SAVED_CUR && \
	((svr4_rlim_t)(l)) != SVR4_RLIM_SAVED_MAX)

#define OKLIMIT64(l) (((rlim_t)(l)) >= 0 && ((rlim_t)(l)) < RLIM_INFINITY && \
	((svr4_rlim64_t)(l)) != SVR4_RLIM64_INFINITY && \
	((svr4_rlim64_t)(l)) != SVR4_RLIM64_SAVED_CUR && \
	((svr4_rlim64_t)(l)) != SVR4_RLIM64_SAVED_MAX)

int
svr4_sys_getrlimit(struct lwp *l, const struct svr4_sys_getrlimit_args *uap, register_t *retval)
{
	int rl = svr4_to_native_rl(SCARG(uap, which));
	struct proc *p = l->l_proc;
	struct rlimit blim;
	struct svr4_rlimit slim;

	if (rl == -1)
		return EINVAL;

	blim = p->p_rlimit[rl];

	/*
	 * Our infinity, is their maxfiles.
	 */
	if (rl == RLIMIT_NOFILE && blim.rlim_max == RLIM_INFINITY)
		blim.rlim_max = maxfiles;

	/*
	 * If the limit can be be represented, it is returned.
	 * Otherwise, if rlim_cur == rlim_max, return RLIM_SAVED_MAX
	 * else return RLIM_SAVED_CUR
	 */
	if (blim.rlim_max == RLIM_INFINITY)
		slim.rlim_max = SVR4_RLIM_INFINITY;
	else if (OKLIMIT(blim.rlim_max))
		slim.rlim_max = (svr4_rlim_t) blim.rlim_max;
	else
		slim.rlim_max = SVR4_RLIM_SAVED_MAX;

	if (blim.rlim_cur == RLIM_INFINITY)
		slim.rlim_cur = SVR4_RLIM_INFINITY;
	else if (OKLIMIT(blim.rlim_cur))
		slim.rlim_cur = (svr4_rlim_t) blim.rlim_cur;
	else if (blim.rlim_max == blim.rlim_cur)
		slim.rlim_cur = SVR4_RLIM_SAVED_MAX;
	else
		slim.rlim_cur = SVR4_RLIM_SAVED_CUR;

	return copyout(&slim, SCARG(uap, rlp), sizeof(*SCARG(uap, rlp)));
}


int
svr4_sys_setrlimit(struct lwp *l, const struct svr4_sys_setrlimit_args *uap, register_t *retval)
{
	int rl = svr4_to_native_rl(SCARG(uap, which));
	struct rlimit blim, *limp;
	struct svr4_rlimit slim;
	int error;

	if (rl == -1)
		return EINVAL;

	limp = &l->l_proc->p_rlimit[rl];

	if ((error = copyin(SCARG(uap, rlp), &slim, sizeof(slim))) != 0)
		return error;

	/*
	 * if the limit is SVR4_RLIM_INFINITY, then we set it to our
	 * unlimited.
	 * We should also: If it is SVR4_RLIM_SAVED_MAX, we should set the
	 * new limit to the corresponding saved hard limit, and if
	 * it is equal to SVR4_RLIM_SAVED_CUR, we should set it to the
	 * corresponding saved soft limit.
	 *
	 */
	if (slim.rlim_max == SVR4_RLIM_INFINITY)
		blim.rlim_max = RLIM_INFINITY;
	else if (OKLIMIT(slim.rlim_max))
		blim.rlim_max = (rlim_t) slim.rlim_max;
	else if (slim.rlim_max == SVR4_RLIM_SAVED_MAX)
		blim.rlim_max = limp->rlim_max;
	else if (slim.rlim_max == SVR4_RLIM_SAVED_CUR)
		blim.rlim_max = limp->rlim_cur;

	if (slim.rlim_cur == SVR4_RLIM_INFINITY)
		blim.rlim_cur = RLIM_INFINITY;
	else if (OKLIMIT(slim.rlim_cur))
		blim.rlim_cur = (rlim_t) slim.rlim_cur;
	else if (slim.rlim_cur == SVR4_RLIM_SAVED_MAX)
		blim.rlim_cur = limp->rlim_max;
	else if (slim.rlim_cur == SVR4_RLIM_SAVED_CUR)
		blim.rlim_cur = limp->rlim_cur;

	return dosetrlimit(l, l->l_proc, rl, &blim);
}


int
svr4_sys_getrlimit64(struct lwp *l, const struct svr4_sys_getrlimit64_args *uap, register_t *retval)
{
	int rl = svr4_to_native_rl(SCARG(uap, which));
	struct proc *p = l->l_proc;
	struct rlimit blim;
	struct svr4_rlimit64 slim;

	if (rl == -1)
		return EINVAL;

	blim = p->p_rlimit[rl];

	/*
	 * Our infinity, is their maxfiles.
	 */
	if (rl == RLIMIT_NOFILE && blim.rlim_max == RLIM_INFINITY)
		blim.rlim_max = maxfiles;

	/*
	 * If the limit can be be represented, it is returned.
	 * Otherwise, if rlim_cur == rlim_max, return SVR4_RLIM_SAVED_MAX
	 * else return SVR4_RLIM_SAVED_CUR
	 */
	if (blim.rlim_max == RLIM_INFINITY)
		slim.rlim_max = SVR4_RLIM64_INFINITY;
	else if (OKLIMIT64(blim.rlim_max))
		slim.rlim_max = (svr4_rlim64_t) blim.rlim_max;
	else
		slim.rlim_max = SVR4_RLIM64_SAVED_MAX;

	if (blim.rlim_cur == RLIM_INFINITY)
		slim.rlim_cur = SVR4_RLIM64_INFINITY;
	else if (OKLIMIT64(blim.rlim_cur))
		slim.rlim_cur = (svr4_rlim64_t) blim.rlim_cur;
	else if (blim.rlim_max == blim.rlim_cur)
		slim.rlim_cur = SVR4_RLIM64_SAVED_MAX;
	else
		slim.rlim_cur = SVR4_RLIM64_SAVED_CUR;

	return copyout(&slim, SCARG(uap, rlp), sizeof(*SCARG(uap, rlp)));
}


int
svr4_sys_setrlimit64(struct lwp *l, const struct svr4_sys_setrlimit64_args *uap, register_t *retval)
{
	int rl = svr4_to_native_rl(SCARG(uap, which));
	struct rlimit blim, *limp;
	struct svr4_rlimit64 slim;
	int error;

	if (rl == -1)
		return EINVAL;

	limp = &l->l_proc->p_rlimit[rl];

	if ((error = copyin(SCARG(uap, rlp), &slim, sizeof(slim))) != 0)
		return error;

	/*
	 * if the limit is SVR4_RLIM64_INFINITY, then we set it to our
	 * unlimited.
	 * We should also: If it is SVR4_RLIM64_SAVED_MAX, we should set the
	 * new limit to the corresponding saved hard limit, and if
	 * it is equal to SVR4_RLIM64_SAVED_CUR, we should set it to the
	 * corresponding saved soft limit.
	 *
	 */
	if (slim.rlim_max == SVR4_RLIM64_INFINITY)
		blim.rlim_max = RLIM_INFINITY;
	else if (OKLIMIT64(slim.rlim_max))
		blim.rlim_max = (rlim_t) slim.rlim_max;
	else if (slim.rlim_max == SVR4_RLIM64_SAVED_MAX)
		blim.rlim_max = limp->rlim_max;
	else if (slim.rlim_max == SVR4_RLIM64_SAVED_CUR)
		blim.rlim_max = limp->rlim_cur;

	if (slim.rlim_cur == SVR4_RLIM64_INFINITY)
		blim.rlim_cur = RLIM_INFINITY;
	else if (OKLIMIT64(slim.rlim_cur))
		blim.rlim_cur = (rlim_t) slim.rlim_cur;
	else if (slim.rlim_cur == SVR4_RLIM64_SAVED_MAX)
		blim.rlim_cur = limp->rlim_max;
	else if (slim.rlim_cur == SVR4_RLIM64_SAVED_CUR)
		blim.rlim_cur = limp->rlim_cur;

	return dosetrlimit(l, l->l_proc, rl, &blim);
}
