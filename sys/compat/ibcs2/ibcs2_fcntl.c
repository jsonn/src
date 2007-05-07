/*	$NetBSD: ibcs2_fcntl.c,v 1.25.2.2 2007/05/07 10:55:10 yamt Exp $	*/

/*
 * Copyright (c) 1995 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ibcs2_fcntl.c,v 1.25.2.2 2007/05/07 10:55:10 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/vnode.h>
#include <sys/kauth.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_fcntl.h>
#include <compat/ibcs2/ibcs2_unistd.h>
#include <compat/ibcs2/ibcs2_signal.h>
#include <compat/ibcs2/ibcs2_syscallargs.h>
#include <compat/ibcs2/ibcs2_util.h>

static int cvt_o_flags __P((int));
static void cvt_flock2iflock __P((struct flock *, struct ibcs2_flock *));
static void cvt_iflock2flock __P((struct ibcs2_flock *, struct flock *));
static int ioflags2oflags __P((int));
static int oflags2ioflags __P((int));

static int
cvt_o_flags(flags)
	int flags;
{
	int r = 0;

        /* convert mode into NetBSD mode */
	if (flags & IBCS2_O_WRONLY) r |= O_WRONLY;
	if (flags & IBCS2_O_RDWR) r |= O_RDWR;
	if (flags & (IBCS2_O_NDELAY | IBCS2_O_NONBLOCK)) r |= O_NONBLOCK;
	if (flags & IBCS2_O_APPEND) r |= O_APPEND;
	if (flags & IBCS2_O_SYNC) r |= O_FSYNC;
	if (flags & IBCS2_O_CREAT) r |= O_CREAT;
	if (flags & IBCS2_O_TRUNC) r |= O_TRUNC;
	if (flags & IBCS2_O_EXCL) r |= O_EXCL;
	return r;
}

static void
cvt_flock2iflock(flp, iflp)
	struct flock *flp;
	struct ibcs2_flock *iflp;
{
	switch (flp->l_type) {
	case F_RDLCK:
		iflp->l_type = IBCS2_F_RDLCK;
		break;
	case F_WRLCK:
		iflp->l_type = IBCS2_F_WRLCK;
		break;
	case F_UNLCK:
		iflp->l_type = IBCS2_F_UNLCK;
		break;
	}
	iflp->l_whence = (short)flp->l_whence;
	iflp->l_start = (ibcs2_off_t)flp->l_start;
	iflp->l_len = (ibcs2_off_t)flp->l_len;
	iflp->l_sysid = 0;
	iflp->l_pid = (ibcs2_pid_t)flp->l_pid;
}

static void
cvt_iflock2flock(iflp, flp)
	struct ibcs2_flock *iflp;
	struct flock *flp;
{
	flp->l_start = (off_t)iflp->l_start;
	flp->l_len = (off_t)iflp->l_len;
	flp->l_pid = (pid_t)iflp->l_pid;
	switch (iflp->l_type) {
	case IBCS2_F_RDLCK:
		flp->l_type = F_RDLCK;
		break;
	case IBCS2_F_WRLCK:
		flp->l_type = F_WRLCK;
		break;
	case IBCS2_F_UNLCK:
		flp->l_type = F_UNLCK;
		break;
	}
	flp->l_whence = iflp->l_whence;
}

/* convert iBCS2 mode into NetBSD mode */
static int
ioflags2oflags(flags)
	int flags;
{
	int r = 0;

	if (flags & IBCS2_O_RDONLY) r |= O_RDONLY;
	if (flags & IBCS2_O_WRONLY) r |= O_WRONLY;
	if (flags & IBCS2_O_RDWR) r |= O_RDWR;
	if (flags & IBCS2_O_NDELAY) r |= O_NONBLOCK;
	if (flags & IBCS2_O_APPEND) r |= O_APPEND;
	if (flags & IBCS2_O_SYNC) r |= O_FSYNC;
	if (flags & IBCS2_O_NONBLOCK) r |= O_NONBLOCK;
	if (flags & IBCS2_O_CREAT) r |= O_CREAT;
	if (flags & IBCS2_O_TRUNC) r |= O_TRUNC;
	if (flags & IBCS2_O_EXCL) r |= O_EXCL;
	if (flags & IBCS2_O_NOCTTY) r |= O_NOCTTY;
	return r;
}

/* convert NetBSD mode into iBCS2 mode */
static int
oflags2ioflags(flags)
	int flags;
{
	int r = 0;

	if (flags & O_RDONLY) r |= IBCS2_O_RDONLY;
	if (flags & O_WRONLY) r |= IBCS2_O_WRONLY;
	if (flags & O_RDWR) r |= IBCS2_O_RDWR;
	if (flags & O_NDELAY) r |= IBCS2_O_NONBLOCK;
	if (flags & O_APPEND) r |= IBCS2_O_APPEND;
	if (flags & O_FSYNC) r |= IBCS2_O_SYNC;
	if (flags & O_NONBLOCK) r |= IBCS2_O_NONBLOCK;
	if (flags & O_CREAT) r |= IBCS2_O_CREAT;
	if (flags & O_TRUNC) r |= IBCS2_O_TRUNC;
	if (flags & O_EXCL) r |= IBCS2_O_EXCL;
	if (flags & O_NOCTTY) r |= IBCS2_O_NOCTTY;
	return r;
}

int
ibcs2_sys_open(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int noctty = SCARG(uap, flags) & IBCS2_O_NOCTTY;
	int ret;

	SCARG(uap, flags) = cvt_o_flags(SCARG(uap, flags));
	ret = sys_open(l, uap, retval);

	if (!ret && !noctty && SESS_LEADER(p) && !(p->p_lflag & PL_CONTROLT)) {
		struct filedesc *fdp = p->p_fd;
		struct file *fp;

		if ((fp = fd_getfile(fdp, *retval)) != NULL) {
			FILE_USE(fp);
			/* ignore any error, just give it a try */
			if (fp->f_type == DTYPE_VNODE)
				(fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, NULL, l);
			FILE_UNUSE(fp, l);
		}
	}
	return ret;
}

int
ibcs2_sys_creat(l, v, retval)
        struct lwp *l;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_creat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	struct sys_open_args cup;

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, mode) = SCARG(uap, mode);
	SCARG(&cup, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	return sys_open(l, &cup, retval);
}

int
ibcs2_sys_access(l, v, retval)
        struct lwp *l;
	void *v;
        register_t *retval;
{
	struct ibcs2_sys_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
        struct sys_access_args cup;

        SCARG(&cup, path) = SCARG(uap, path);
        SCARG(&cup, flags) = SCARG(uap, flags);
        return sys_access(l, &cup, retval);
}

int
ibcs2_sys_eaccess(struct lwp *l, void *v, register_t *retval)
{
	struct ibcs2_sys_eaccess_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	struct vnode *vp;
        int error, flags;
        struct nameidata nd;

        NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, UIO_USERSPACE,
            SCARG(uap, path), l);
        if ((error = namei(&nd)) != 0)
                return error;
        vp = nd.ni_vp;

        /* Flags == 0 means only check for existence. */
        if (SCARG(uap, flags)) {
                flags = 0;
                if (SCARG(uap, flags) & IBCS2_R_OK)
                        flags |= VREAD;
                if (SCARG(uap, flags) & IBCS2_W_OK)
                        flags |= VWRITE;
                if (SCARG(uap, flags) & IBCS2_X_OK)
			flags |= VEXEC;
                if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
                        error = VOP_ACCESS(vp, flags, l->l_cred, l);
        }
        vput(vp);
        return error;
}

int
ibcs2_sys_fcntl(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(char *) arg;
	} */ *uap = v;
	struct proc *p = l->l_proc;
	int error;
	struct sys_fcntl_args fa;
	struct flock *flp;
	struct ibcs2_flock ifl;

	switch(SCARG(uap, cmd)) {
	case IBCS2_F_DUPFD:
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_DUPFD;
		SCARG(&fa, arg) = SCARG(uap, arg);
		return sys_fcntl(l, &fa, retval);
	case IBCS2_F_GETFD:
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_GETFD;
		SCARG(&fa, arg) = SCARG(uap, arg);
		return sys_fcntl(l, &fa, retval);
	case IBCS2_F_SETFD:
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_SETFD;
		SCARG(&fa, arg) = SCARG(uap, arg);
		return sys_fcntl(l, &fa, retval);
	case IBCS2_F_GETFL:
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_GETFL;
		SCARG(&fa, arg) = SCARG(uap, arg);
		error = sys_fcntl(l, &fa, retval);
		if (error)
			return error;
		*retval = oflags2ioflags(*retval);
		return error;
	case IBCS2_F_SETFL:
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_SETFL;
		SCARG(&fa, arg) = (void *)ioflags2oflags((int) SCARG(uap, arg));
		return sys_fcntl(l, &fa, retval);

	case IBCS2_F_GETLK:
	    {
		void *sg = stackgap_init(p, 0);
		flp = stackgap_alloc(p, &sg, sizeof(*flp));
		error = copyin((void *)SCARG(uap, arg), (void *)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, flp);
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_GETLK;
		SCARG(&fa, arg) = (void *)flp;
		error = sys_fcntl(l, &fa, retval);
		if (error)
			return error;
		cvt_flock2iflock(flp, &ifl);
		return copyout((void *)&ifl, (void *)SCARG(uap, arg),
			       ibcs2_flock_len);
	    }

	case IBCS2_F_SETLK:
	    {
		void *sg = stackgap_init(p, 0);
		flp = stackgap_alloc(p, &sg, sizeof(*flp));
		error = copyin((void *)SCARG(uap, arg), (void *)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, flp);
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_SETLK;
		SCARG(&fa, arg) = (void *)flp;
		return sys_fcntl(l, &fa, retval);
	    }

	case IBCS2_F_SETLKW:
	    {
		void *sg = stackgap_init(p, 0);
		flp = stackgap_alloc(p, &sg, sizeof(*flp));
		error = copyin((void *)SCARG(uap, arg), (void *)&ifl,
			       ibcs2_flock_len);
		if (error)
			return error;
		cvt_iflock2flock(&ifl, flp);
		SCARG(&fa, fd) = SCARG(uap, fd);
		SCARG(&fa, cmd) = F_SETLKW;
		SCARG(&fa, arg) = (void *)flp;
		return sys_fcntl(l, &fa, retval);
	    }
	}
	return ENOSYS;
}
