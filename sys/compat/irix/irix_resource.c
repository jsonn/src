/*	$NetBSD: irix_resource.c,v 1.8.4.1 2007/07/11 20:04:03 mjf Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: irix_resource.c,v 1.8.4.1 2007/07/11 20:04:03 mjf Exp $");

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/syscallargs.h>

#include <compat/common/compat_util.h>

#include <compat/irix/irix_types.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_resource.h>
#include <compat/irix/irix_syscallargs.h>

static int irix_to_native_resource __P((int));

static int
irix_to_native_resource(irix_res)
	int irix_res;
{
	int bsd_res;

	switch(irix_res) {
	case IRIX_RLIMIT_CPU:
		bsd_res = RLIMIT_CPU;
		break;
	case IRIX_RLIMIT_FSIZE:
		bsd_res = RLIMIT_FSIZE;
		break;
	case IRIX_RLIMIT_DATA:
		bsd_res = RLIMIT_DATA;
		break;
	case IRIX_RLIMIT_STACK:
		bsd_res = RLIMIT_STACK;
		break;
	case IRIX_RLIMIT_CORE:
		bsd_res = RLIMIT_CORE;
		break;
	case IRIX_RLIMIT_NOFILE:
		bsd_res = RLIMIT_NOFILE;
		break;
	case IRIX_RLIMIT_VMEM:
	case IRIX_RLIMIT_RSS:
		bsd_res = RLIMIT_RSS;
		break;
	case IRIX_RLIMIT_PTHREAD:
		printf("Warning: ignored IRIX pthread rlimit flag\n");
	default:
		bsd_res = -1;
		break;
	}
	return bsd_res;
}

int
irix_sys_getrlimit(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_getrlimit_args /* {
		syscallarg(int) resource;
		syscallarg(struct irix_rlimit *) rlp;
	} */ *uap = v;
	struct rlimit *rlp;
	struct irix_rlimit irlp;
	int which;

	which = irix_to_native_resource(SCARG(uap, resource));
	if (which < 0)
		return EINVAL;

	rlp = &l->l_proc->p_rlimit[which];

	if (rlp->rlim_cur == RLIM_INFINITY)
		irlp.rlim_cur = IRIX_RLIM_INFINITY;
	else
		irlp.rlim_cur = rlp->rlim_cur;

	if (rlp->rlim_max == RLIM_INFINITY)
		irlp.rlim_max = IRIX_RLIM_INFINITY;
	else
		irlp.rlim_max = rlp->rlim_cur;

	return copyout(&irlp, SCARG(uap, rlp), sizeof(irlp));
}

int
irix_sys_getrlimit64(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_getrlimit64_args /* {
		syscallarg(int) resource;
		syscallarg(struct irix_rlimit64 *) rlp;
	} */ *uap = v;
	struct rlimit *rlp;
	struct irix_rlimit64 irlp;
	int which;

	which = irix_to_native_resource(SCARG(uap, resource));
	if (which < 0)
		return EINVAL;

	rlp = &l->l_proc->p_rlimit[which];

	if (rlp->rlim_cur == RLIM_INFINITY)
		irlp.rlim_cur = IRIX_RLIM64_INFINITY;
	else
		irlp.rlim_cur = rlp->rlim_cur;

	if (rlp->rlim_max == RLIM_INFINITY)
		irlp.rlim_max = IRIX_RLIM64_INFINITY;
	else
		irlp.rlim_max = rlp->rlim_cur;

	return copyout(&irlp, SCARG(uap, rlp), sizeof(irlp));
}

int
irix_sys_setrlimit(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_getrlimit_args /* {
		syscallarg(int) resource;
		syscallarg(const struct irix_rlimit *) rlp;
	} */ *uap = v;
	struct irix_rlimit irlp;
	struct rlimit rlp;
	int which;
	int error;

	which = irix_to_native_resource(SCARG(uap, resource));
	if (which < 0)
		return EINVAL;

	if ((error = copyin(SCARG(uap, rlp), &irlp, sizeof(irlp))) != 0)
		return error;

	if (irlp.rlim_cur == IRIX_RLIM_INFINITY)
		rlp.rlim_cur = RLIM_INFINITY;
	else
		rlp.rlim_cur = irlp.rlim_cur;

	if (irlp.rlim_max == IRIX_RLIM_INFINITY)
		rlp.rlim_max = RLIM_INFINITY;
	else
		rlp.rlim_max = irlp.rlim_cur;

	return dosetrlimit(l, l->l_proc, which, &rlp);
}

int
irix_sys_setrlimit64(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct irix_sys_getrlimit_args /* {
		syscallarg(int) resource;
		syscallarg(const struct irix_rlimit64 *) rlp;
	} */ *uap = v;
	struct rlimit rlp;
	struct irix_rlimit64 irlp;
	int which;
	int error;

	which = irix_to_native_resource(SCARG(uap, resource));
	if (which < 0)
		return EINVAL;

	if ((error = copyin(SCARG(uap, rlp), &irlp, sizeof(irlp))) != 0)
		return error;

	if (irlp.rlim_cur == IRIX_RLIM64_INFINITY)
		rlp.rlim_cur = RLIM_INFINITY;
	else
		rlp.rlim_cur = irlp.rlim_cur;

	if (irlp.rlim_max == IRIX_RLIM64_INFINITY)
		rlp.rlim_max = RLIM_INFINITY;
	else
		rlp.rlim_max = irlp.rlim_cur;

	return dosetrlimit(l, l->l_proc, which, &rlp);
}
