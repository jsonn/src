/*	$NetBSD: vfs_syscalls_12.c,v 1.6.6.2 2001/04/09 01:55:31 nathanw Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From: @(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#include <sys/syscallargs.h>

static void cvtstat __P((struct stat *, struct stat12 *));

/*
 * Convert from a new to an old stat structure.
 */
static void
cvtstat(st, ost)
	struct stat *st;
	struct stat12 *ost;
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode & 0xffff;
	if (st->st_nlink >= (1 << 15))
		ost->st_nlink = (1 << 15) - 1;
	else
		ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	ost->st_atimespec = st->st_atimespec;
	ost->st_mtimespec = st->st_mtimespec;
	ost->st_ctimespec = st->st_ctimespec;
	ost->st_size = st->st_size;
	ost->st_blocks = st->st_blocks;
	ost->st_blksize = st->st_blksize;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

/*
 * Read a block of directory entries in a file system independent format.
 */
int
compat_12_sys_getdirentries(struct lwp *l, void *v, register_t *retval)
{
	struct compat_12_sys_getdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct file *fp;
	int error, done;
	long loff;

	/* getvnode() will use the descriptor for us */
	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return error;
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out;
	}

	loff = fp->f_offset;

	error = vn_readdir(fp, SCARG(uap, buf), UIO_USERSPACE,
			SCARG(uap, count), &done, p, 0, 0);

	error = copyout(&loff, SCARG(uap, basep), sizeof(long));
	*retval = done;
 out:
	FILE_UNUSE(fp, p);
	return error;
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
compat_12_sys_stat(struct lwp *l, void *v, register_t *retval)
{
	struct compat_12_sys_stat_args /* {
		syscallarg(const char *) path;
		syscallarg(struct stat12 *) ub;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct stat sb;
	struct stat12 osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout(&osb, SCARG(uap, ub), sizeof (osb));
	return (error);
}


/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
compat_12_sys_lstat(struct lwp *l, void *v, register_t *retval)
{
	struct compat_12_sys_lstat_args /* {
		syscallarg(const char *) path;
		syscallarg(struct stat12 *) ub;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	struct stat sb;
	struct stat12 osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout(&osb, SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
compat_12_sys_fstat(struct lwp *l, void *v, register_t *retval)
{
	struct compat_12_sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct stat12 *) sb;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat ub;
	struct stat12 oub;
	int error;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);

	FILE_USE(fp);
	error = (*fp->f_ops->fo_stat)(fp->f_data, &ub, p);
	FILE_UNUSE(fp, p);

	if (error == 0) {
		cvtstat(&ub, &oub);
		error = copyout(&oub, SCARG(uap, sb), sizeof (oub));
	}
	return (error);
}
