/*	$NetBSD: svr4_fcntl.c,v 1.18.6.2 1997/11/17 02:28:15 thorpej Exp $	 */

/*
 * Copyright (c) 1994, 1997 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_fcntl.h>

static u_long svr4_to_bsd_cmd __P((u_long));
static int svr4_to_bsd_flags __P((int));
static int bsd_to_svr4_flags __P((int));
static void bsd_to_svr4_flock __P((struct flock *, struct svr4_flock *));
static void svr4_to_bsd_flock __P((struct svr4_flock *, struct flock *));
static void bsd_to_svr4_flock64 __P((struct flock *, struct svr4_flock64 *));
static void svr4_to_bsd_flock64 __P((struct svr4_flock64 *, struct flock *));
static int fd_revoke __P((struct proc *, int, register_t *));
static int fd_truncate __P((struct proc *, int, struct flock *, register_t *));

static u_long
svr4_to_bsd_cmd(cmd)
	u_long	cmd;
{
	switch (cmd) {
	case SVR4_F_DUPFD:
		return F_DUPFD;
	case SVR4_F_GETFD:
		return F_GETFD;
	case SVR4_F_SETFD:
		return F_SETFD;
	case SVR4_F_GETFL:
		return F_GETFL;
	case SVR4_F_SETFL:
		return F_SETFL;
	case SVR4_F_GETLK:
		return F_GETLK;
	case SVR4_F_SETLK:
		return F_SETLK;
	case SVR4_F_SETLKW:
		return F_SETLKW;
	default:
		return -1;
	}
}


static int
svr4_to_bsd_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & SVR4_O_RDONLY) ? O_RDONLY : 0;
	r |= (l & SVR4_O_WRONLY) ? O_WRONLY : 0;
	r |= (l & SVR4_O_RDWR) ? O_RDWR : 0;
	r |= (l & SVR4_O_NDELAY) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_APPEND) ? O_APPEND : 0;
	r |= (l & SVR4_O_SYNC) ? O_FSYNC : 0;
	r |= (l & SVR4_O_NONBLOCK) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_PRIV) ? O_EXLOCK : 0;
	r |= (l & SVR4_O_CREAT) ? O_CREAT : 0;
	r |= (l & SVR4_O_TRUNC) ? O_TRUNC : 0;
	r |= (l & SVR4_O_EXCL) ? O_EXCL : 0;
	r |= (l & SVR4_O_NOCTTY) ? O_NOCTTY : 0;
	return r;
}


static int
bsd_to_svr4_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & O_RDONLY) ? SVR4_O_RDONLY : 0;
	r |= (l & O_WRONLY) ? SVR4_O_WRONLY : 0;
	r |= (l & O_RDWR) ? SVR4_O_RDWR : 0;
	r |= (l & O_NDELAY) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_APPEND) ? SVR4_O_APPEND : 0;
	r |= (l & O_FSYNC) ? SVR4_O_SYNC : 0;
	r |= (l & O_NONBLOCK) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_EXLOCK) ? SVR4_O_PRIV : 0;
	r |= (l & O_CREAT) ? SVR4_O_CREAT : 0;
	r |= (l & O_TRUNC) ? SVR4_O_TRUNC : 0;
	r |= (l & O_EXCL) ? SVR4_O_EXCL : 0;
	r |= (l & O_NOCTTY) ? SVR4_O_NOCTTY : 0;
	return r;
}


static void
bsd_to_svr4_flock(iflp, oflp)
	struct flock		*iflp;
	struct svr4_flock	*oflp;
{
	switch (iflp->l_type) {
	case F_RDLCK:
		oflp->l_type = SVR4_F_RDLCK;
		break;
	case F_WRLCK:
		oflp->l_type = SVR4_F_WRLCK;
		break;
	case F_UNLCK:
		oflp->l_type = SVR4_F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = (short) iflp->l_whence;
	oflp->l_start = (svr4_off_t) iflp->l_start;
	oflp->l_len = (svr4_off_t) iflp->l_len;
	oflp->l_sysid = 0;
	oflp->l_pid = (svr4_pid_t) iflp->l_pid;
}


static void
svr4_to_bsd_flock(iflp, oflp)
	struct svr4_flock	*iflp;
	struct flock		*oflp;
{
	switch (iflp->l_type) {
	case SVR4_F_RDLCK:
		oflp->l_type = F_RDLCK;
		break;
	case SVR4_F_WRLCK:
		oflp->l_type = F_WRLCK;
		break;
	case SVR4_F_UNLCK:
		oflp->l_type = F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = iflp->l_whence;
	oflp->l_start = (off_t) iflp->l_start;
	oflp->l_len = (off_t) iflp->l_len;
	oflp->l_pid = (pid_t) iflp->l_pid;

}

static void
bsd_to_svr4_flock64(iflp, oflp)
	struct flock		*iflp;
	struct svr4_flock64	*oflp;
{
	switch (iflp->l_type) {
	case F_RDLCK:
		oflp->l_type = SVR4_F_RDLCK;
		break;
	case F_WRLCK:
		oflp->l_type = SVR4_F_WRLCK;
		break;
	case F_UNLCK:
		oflp->l_type = SVR4_F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = (short) iflp->l_whence;
	oflp->l_start = (svr4_off64_t) iflp->l_start;
	oflp->l_len = (svr4_off64_t) iflp->l_len;
	oflp->l_sysid = 0;
	oflp->l_pid = (svr4_pid_t) iflp->l_pid;
}


static void
svr4_to_bsd_flock64(iflp, oflp)
	struct svr4_flock64	*iflp;
	struct flock		*oflp;
{
	switch (iflp->l_type) {
	case SVR4_F_RDLCK:
		oflp->l_type = F_RDLCK;
		break;
	case SVR4_F_WRLCK:
		oflp->l_type = F_WRLCK;
		break;
	case SVR4_F_UNLCK:
		oflp->l_type = F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = iflp->l_whence;
	oflp->l_start = (off_t) iflp->l_start;
	oflp->l_len = (off_t) iflp->l_len;
	oflp->l_pid = (pid_t) iflp->l_pid;

}


static int
fd_revoke(p, fd, retval)
	struct proc *p;
	int fd;
	register_t *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	struct vattr vattr;
	int error;

	if ((u_int)fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return EBADF;

	switch (fp->f_type) {
	case DTYPE_VNODE:
		vp = (struct vnode *) fp->f_data;

	case DTYPE_SOCKET:
		return EINVAL;

	default:
		panic("svr4_fcntl(F_REVOKE)");
		/*NOTREACHED*/
	}

	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		error = EINVAL;
		goto out;
	}

	if ((error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) != 0)
		goto out;

	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)) != 0)
		goto out;

	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		vgoneall(vp);
out:
	vrele(vp);
	return error;
}


static int
fd_truncate(p, fd, flp, retval)
	struct proc *p;
	int fd;
	struct flock *flp;
	register_t *retval;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	off_t start, length;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct sys_ftruncate_args ft;

	/*
	 * We only support truncating the file.
	 */
	if ((u_int)fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return EBADF;

	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE
#ifdef FIFO
	    || vp->v_type == VFIFO
#endif
	)
		return ESPIPE;

	if ((error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) != 0)
		return error;

	length = vattr.va_size;

	switch (flp->l_whence) {
	case SEEK_CUR:
		start = fp->f_offset + flp->l_start;
		break;

	case SEEK_END:
		start = flp->l_start + length;
		break;

	case SEEK_SET:
		start = flp->l_start;
		break;

	default:
		return EINVAL;
	}

	if (start + flp->l_len < length) {
		/* We don't support free'ing in the middle of the file */
		return EINVAL;
	}

	SCARG(&ft, fd) = fd;
	SCARG(&ft, length) = start;

	return sys_ftruncate(p, &ft, retval);
}


int
svr4_sys_open(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_open_args	*uap = v;
	int			error;
	struct sys_open_args	cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, flags) = svr4_to_bsd_flags(SCARG(uap, flags));
	SCARG(&cup, mode) = SCARG(uap, mode);
	error = sys_open(p, &cup, retval);

	if (error)
		return error;

	if (!(SCARG(&cup, flags) & O_NOCTTY) && SESS_LEADER(p) &&
	    !(p->p_flag & P_CONTROLT)) {
		struct filedesc	*fdp = p->p_fd;
		struct file	*fp = fdp->fd_ofiles[*retval];

		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl) (fp, TIOCSCTTY, (caddr_t) 0, p);
	}
	return 0;
}


int
svr4_sys_open64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	return svr4_sys_open(p, v, retval);
}


int
svr4_sys_creat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_creat_args *uap = v;
	struct sys_open_args cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, mode) = SCARG(uap, mode);
	SCARG(&cup, flags) = O_WRONLY | O_CREAT | O_TRUNC;

	return sys_open(p, &cup, retval);
}


int
svr4_sys_creat64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	return svr4_sys_creat(p, v, retval);
}


int
svr4_sys_llseek(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_llseek_args *uap = v;
	struct sys_lseek_args ap;

	SCARG(&ap, fd) = SCARG(uap, fd);

#if BYTE_ORDER == BIG_ENDIAN
	SCARG(&ap, offset) = (((long long) SCARG(uap, offset1)) << 32) | 
		SCARG(uap, offset2);
#else
	SCARG(&ap, offset) = (((long long) SCARG(uap, offset2)) << 32) | 
		SCARG(uap, offset1);
#endif
	SCARG(&ap, whence) = SCARG(uap, whence);

	return sys_lseek(p, &ap, retval);
}

int
svr4_sys_access(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_access_args *uap = v;
	struct sys_access_args cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, flags) = SCARG(uap, flags);

	return sys_access(p, &cup, retval);
}


int
svr4_sys_pread(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pread_args *uap = v;
	struct sys_lseek_args lap;
	struct sys_read_args rap;
	register_t retval2;
	off_t oldoff;
	int error, error2;

	/*
	 * Record the current file offset; it will be restored after
	 * completition of the actual read operation.
	 */
	SCARG(&lap, fd) = SCARG(uap, fd);
	SCARG(&lap, offset) = 0;
	SCARG(&lap, whence) = SEEK_CUR;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;
	oldoff = *(off_t *)retval;

	/* Seek to the requested file offset. */
	SCARG(&lap, offset) = SCARG(uap, off);
	SCARG(&lap, whence) = SEEK_SET;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;

	/* Perform the actual read operation. */
	SCARG(&rap, fd) = SCARG(uap, fd);
	SCARG(&rap, buf) = SCARG(uap, buf);
	SCARG(&rap, nbyte) = SCARG(uap, nbyte);
	error = sys_read(p, &rap, retval);

	/* In any case, try to seek back to the old file offset. */
	SCARG(&lap, offset) = oldoff;
	error2 = sys_lseek(p, &lap, &retval2);
	
	return (error == 0 ? error2 : error);
}


int
svr4_sys_pread64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pread64_args *uap = v;
	struct sys_lseek_args lap;
	struct sys_read_args rap;
	register_t retval2;
	off_t oldoff;
	int error, error2;

	/*
	 * Record the current file offset; it will be restored after
	 * completition of the actual read operation.
	 */
	SCARG(&lap, fd) = SCARG(uap, fd);
	SCARG(&lap, offset) = 0;
	SCARG(&lap, whence) = SEEK_CUR;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;
	oldoff = *(off_t *)retval;

	/* Seek to the requested file offset. */
	SCARG(&lap, offset) = SCARG(uap, off);
	SCARG(&lap, whence) = SEEK_SET;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;

	/* Perform the actual read operation. */
	SCARG(&rap, fd) = SCARG(uap, fd);
	SCARG(&rap, buf) = SCARG(uap, buf);
	SCARG(&rap, nbyte) = SCARG(uap, nbyte);
	error = sys_read(p, &rap, retval);

	/* In any case, try to seek back to the old file offset. */
	SCARG(&lap, offset) = oldoff;
	error2 = sys_lseek(p, &lap, &retval2);

	return (error == 0 ? error2 : error);
}


int
svr4_sys_pwrite(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pwrite_args *uap = v;
	struct sys_lseek_args lap;
	struct sys_write_args rap;
	register_t retval2;
	off_t oldoff;
	int error, error2;

	/*
	 * Record the current file offset; it will be restored after
	 * completition of the actual write operation.
	 */
	SCARG(&lap, fd) = SCARG(uap, fd);
	SCARG(&lap, offset) = 0;
	SCARG(&lap, whence) = SEEK_CUR;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;
	oldoff = *(off_t *)retval;

	/* Seek to the requested file offset. */
	SCARG(&lap, offset) = SCARG(uap, off);
	SCARG(&lap, whence) = SEEK_SET;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;

	/* Perform the actual write operation. */
	SCARG(&rap, fd) = SCARG(uap, fd);
	SCARG(&rap, buf) = SCARG(uap, buf);
	SCARG(&rap, nbyte) = SCARG(uap, nbyte);
	error = sys_write(p, &rap, retval);

	/* In any case, try to seek back to the old file offset. */
	SCARG(&lap, offset) = oldoff;
	error2 = sys_lseek(p, &lap, &retval2);

	return (error == 0 ? error2 : error);
}


int
svr4_sys_pwrite64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pwrite64_args *uap = v;
	struct sys_lseek_args lap;
	struct sys_write_args rap;
	register_t retval2;
	off_t oldoff;
	int error, error2;

	/*
	 * Record the current file offset; it will be restored after
	 * completition of the actual write operation.
	 */
	SCARG(&lap, fd) = SCARG(uap, fd);
	SCARG(&lap, offset) = 0;
	SCARG(&lap, whence) = SEEK_CUR;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;
	oldoff = *(off_t *)retval;

	/* Seek to the requested file offset. */
	SCARG(&lap, offset) = SCARG(uap, off);
	SCARG(&lap, whence) = SEEK_SET;
	if ((error = sys_lseek(p, &lap, retval)) != 0)
		return error;

	/* Perform the actual write operation. */
	SCARG(&rap, fd) = SCARG(uap, fd);
	SCARG(&rap, buf) = SCARG(uap, buf);
	SCARG(&rap, nbyte) = SCARG(uap, nbyte);
	error = sys_write(p, &rap, retval);

	/* In any case, try to seek back to the old file offset. */
	SCARG(&lap, offset) = oldoff;
	error2 = sys_lseek(p, &lap, &retval2);

	return (error == 0 ? error2 : error);
}


int
svr4_sys_fcntl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fcntl_args	*uap = v;
	int				error;
	struct sys_fcntl_args		fa;

	SCARG(&fa, fd) = SCARG(uap, fd);
	SCARG(&fa, cmd) = svr4_to_bsd_cmd(SCARG(uap, cmd));

	switch (SCARG(&fa, cmd)) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
		SCARG(&fa, arg) = SCARG(uap, arg);
		return sys_fcntl(p, &fa, retval);

	case F_GETFL:
		SCARG(&fa, arg) = SCARG(uap, arg);
		error = sys_fcntl(p, &fa, retval);
		if (error)
			return error;
		*retval = bsd_to_svr4_flags(*retval);
		return error;

	case F_SETFL:
		{
			/*
			 * we must save the O_ASYNC flag, as that is
			 * handled by ioctl(_, I_SETSIG, _) emulation.
			 */
			int cmd, flags;

			cmd = SCARG(&fa, cmd); /* save it for a while */

			SCARG(&fa, cmd) = F_GETFL;
			if ((error = sys_fcntl(p, &fa, &flags)) != 0)
				return error;
			flags &= O_ASYNC;
			flags |= svr4_to_bsd_flags((u_long) SCARG(uap, arg));
			SCARG(&fa, cmd) = cmd;
			SCARG(&fa, arg) = (void *) flags;
			return sys_fcntl(p, &fa, retval);
		}

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		{
			struct svr4_flock	 ifl;
			struct flock		*flp, fl;
			caddr_t sg = stackgap_init(p->p_emul);

			flp = stackgap_alloc(&sg, sizeof(struct flock));
			SCARG(&fa, arg) = (void *) flp;

			error = copyin(SCARG(uap, arg), &ifl, sizeof ifl);
			if (error)
				return error;

			svr4_to_bsd_flock(&ifl, &fl);

			error = copyout(&fl, flp, sizeof fl);
			if (error)
				return error;

			error = sys_fcntl(p, &fa, retval);
			if (error || SCARG(&fa, cmd) != F_GETLK)
				return error;

			error = copyin(flp, &fl, sizeof fl);
			if (error)
				return error;

			bsd_to_svr4_flock(&fl, &ifl);

			return copyout(&ifl, SCARG(uap, arg), sizeof ifl);
		}
	case -1:
		switch (SCARG(uap, cmd)) {
		case SVR4_F_DUP2FD:
			{
				struct sys_dup2_args du;

				SCARG(&du, from) = SCARG(uap, fd);
				error = copyin(SCARG(uap, arg),
				    &SCARG(&du, to), sizeof SCARG(&du, to));
				if (error)
					return error;
				error = sys_dup2(p, &du, retval);
				if (error)
					return error;
				*retval = SCARG(&du, to);
				return 0;
			}

		case SVR4_F_FREESP:
			{
				struct svr4_flock	 ifl;
				struct flock		 fl;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock(&ifl, &fl);
				return fd_truncate(p, SCARG(uap, fd), &fl,
				    retval);
			}

		case SVR4_F_GETLK64:
		case SVR4_F_SETLK64:
		case SVR4_F_SETLKW64:
			{
				struct svr4_flock64	 ifl;
				struct flock		*flp, fl;
				caddr_t sg = stackgap_init(p->p_emul);

				flp = stackgap_alloc(&sg, sizeof(struct flock));
				SCARG(&fa, arg) = (void *) flp;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;

				svr4_to_bsd_flock64(&ifl, &fl);

				error = copyout(&fl, flp, sizeof fl);
				if (error)
					return error;

				error = sys_fcntl(p, &fa, retval);
				if (error || SCARG(&fa, cmd) != F_GETLK)
					return error;

				error = copyin(flp, &fl, sizeof fl);
				if (error)
					return error;

				bsd_to_svr4_flock64(&fl, &ifl);

				return copyout(&ifl, SCARG(uap, arg),
				    sizeof ifl);
			}

		case SVR4_F_FREESP64:
			{
				struct svr4_flock64	 ifl;
				struct flock		 fl;

				error = copyin(SCARG(uap, arg), &ifl,
				    sizeof ifl);
				if (error)
					return error;
				svr4_to_bsd_flock64(&ifl, &fl);
				return fd_truncate(p, SCARG(uap, fd), &fl,
				    retval);
			}

		case SVR4_F_REVOKE:
			return fd_revoke(p, SCARG(uap, fd), retval);

		default:
			return ENOSYS;
		}

	default:
		return ENOSYS;
	}
}
