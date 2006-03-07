/*	$NetBSD: pecoff_misc.c,v 1.14.2.2 2006/03/07 03:32:07 thorpej Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pecoff_misc.c,v 1.14.2.2 2006/03/07 03:32:07 thorpej Exp $");

#if defined(_KERNEL_OPT)
#include "opt_nfsserver.h"
#include "opt_compat_netbsd.h"
#include "opt_sysv.h"
#include "opt_compat_43.h"

#include "fs_lfs.h"
#include "fs_nfs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/proc.h>

#include <sys/sa.h>
#include <sys/syscallargs.h>

#include <compat/common/compat_util.h>
#include <compat/pecoff/pecoff_syscallargs.h>

int
pecoff_sys_open(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_open_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

#if 0
	if (SCARG(uap, flags) & O_CREAT)
		CHECK_ALT_CREAT(l, &sg, SCARG(uap, path));
	else
#endif
		CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_open(l, v, retval);
}


int
pecoff_sys_link(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_link_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));
#if 0
	CHECK_ALT_CREAT(l, &sg, SCARG(uap, link));
#endif

	return sys_link(l, v, retval);
}


int
pecoff_sys_unlink(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_unlink_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_unlink(l, v, retval);
}


int
pecoff_sys_chdir(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_chdir_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_chdir(l, v, retval);
}


int
pecoff_sys_chmod(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_chmod_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_chmod(l, v, retval);
}


int
pecoff_sys_chown(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_chown_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_chown(l, v, retval);
}


int
pecoff_sys_unmount(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_unmount_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_unmount(l, v, retval);
}


int
pecoff_sys_access(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_access_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_access(l, v, retval);
}


int
pecoff_sys_chflags(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_chflags_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_chflags(l, v, retval);
}


#ifdef PECOFF_INCLUDE_COMPAT
int
pecoff_compat_43_sys_stat(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_compat_43_sys_stat_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_43_sys_stat(l, v, retval);
}


int
pecoff_compat_43_sys_lstat(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_compat_43_sys_lstat_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_43_sys_lstat(l, v, retval);
}
#endif /* PECOFF_INCLUDE_COMPAT */


int
pecoff_sys_revoke(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_revoke_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_revoke(l, v, retval);
}


int
pecoff_sys_symlink(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_symlink_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));
#if 0
	CHECK_ALT_CREAT(l, &sg, SCARG(uap, link));
#endif

	return sys_symlink(l, v, retval);
}


ssize_t
pecoff_sys_readlink(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_readlink_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys_readlink(l, v, retval);
}


int
pecoff_sys_execve(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_execve_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_execve(l, v, retval);
}


int
pecoff_sys_chroot(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_chroot_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_chroot(l, v, retval);
}


int
pecoff_sys_rename(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_rename_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, from));
#if 0
	CHECK_ALT_CREAT(l, &sg, SCARG(uap, to));
#endif

	return sys_rename(l, v, retval);
}


#ifdef PECOFF_INCLUDE_COMPAT
int
pecoff_compat_43_sys_truncate(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_compat_43_sys_truncate_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_43_sys_truncate(l, v, retval);
}
#endif /* PECOFF_INCLUDE_COMPAT */


int
pecoff_sys_rmdir(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_rmdir_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_rmdir(l, v, retval);
}


int
pecoff_sys_utimes(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_utimes_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_utimes(l, v, retval);
}


int
pecoff_sys_statfs(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_statfs_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_20_sys_statfs(l, v, retval);
}


int
pecoff_sys_getfh(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_getfh_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, fname));

	return sys_getfh(l, v, retval);
}


#ifdef PECOFF_INCLUDE_COMPAT
int
pecoff_compat_12_sys_stat(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_compat_12_sys_stat_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_12_sys_stat(l, v, retval);
}


int
pecoff_compat_12_sys_lstat(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_compat_12_sys_lstat_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_12_sys_lstat(l, v, retval);
}
#endif /* PECOFF_INCLUDE_COMPAT */

int
pecoff_sys_pathconf(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_pathconf_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_pathconf(l, v, retval);
}


int
pecoff_sys_truncate(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_truncate_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_truncate(l, v, retval);
}


int
pecoff_sys_undelete(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_undelete_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys_undelete(l, v, retval);
}


int
pecoff_sys___posix_rename(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___posix_rename_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, from));
#if 0
	CHECK_ALT_CREAT(l, &sg, SCARG(uap, to));
#endif

	return sys___posix_rename(l, v, retval);
}


int
pecoff_sys_lchmod(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_lchmod_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys_lchmod(l, v, retval);
}


int
pecoff_sys_lchown(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_lchown_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys_lchown(l, v, retval);
}


int
pecoff_sys_lutimes(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_lutimes_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys_lutimes(l, v, retval);
}

#ifdef COMPAT_30
int
pecoff_sys___stat13(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___stat13_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return compat_30_sys___stat13(l, v, retval);
}


int
pecoff_sys___lstat13(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___lstat13_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return compat_30_sys___lstat13(l, v, retval);
}
#endif

int
pecoff_sys___stat30(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___stat30_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys___stat30(l, v, retval);
}


int
pecoff_sys___lstat30(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___lstat30_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys___lstat30(l, v, retval);
}


int
pecoff_sys___posix_chown(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___posix_chown_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_EXIST(l, &sg, SCARG(uap, path));

	return sys___posix_chown(l, v, retval);
}


int
pecoff_sys___posix_lchown(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys___posix_lchown_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys___posix_lchown(l, v, retval);
}


int
pecoff_sys_lchflags(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct proc *p = l->l_proc;
	struct pecoff_sys_lchflags_args *uap = v;
	caddr_t sg = stackgap_init(p, 0);

	CHECK_ALT_SYMLINK(l, &sg, SCARG(uap, path));

	return sys_lchflags(l, v, retval);
}
