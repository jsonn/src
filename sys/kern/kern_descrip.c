/*	$NetBSD: kern_descrip.c,v 1.141.2.4 2006/05/06 23:31:30 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)kern_descrip.c	8.8 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_descrip.c,v 1.141.2.4 2006/05/06 23:31:30 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/kauth.h>

#include <sys/mount.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>

/*
 * Descriptor management.
 */
struct filelist	filehead;	/* head of list of open files */
int		nfiles;		/* actual number of open files */
POOL_INIT(file_pool, sizeof(struct file), 0, 0, 0, "filepl",
    &pool_allocator_nointr);
POOL_INIT(cwdi_pool, sizeof(struct cwdinfo), 0, 0, 0, "cwdipl",
    &pool_allocator_nointr);
POOL_INIT(filedesc0_pool, sizeof(struct filedesc0), 0, 0, 0, "fdescpl",
    &pool_allocator_nointr);

/* Global file list lock */
static struct simplelock filelist_slock = SIMPLELOCK_INITIALIZER;

MALLOC_DEFINE(M_FILE, "file", "Open file structure");
MALLOC_DEFINE(M_FILEDESC, "file desc", "Open file descriptor table");
MALLOC_DEFINE(M_IOCTLOPS, "ioctlops", "ioctl data buffer");

static inline int
find_next_zero(uint32_t *bitmap, int want, u_int bits)
{
	int i, off, maxoff;
	uint32_t sub;

	if (want > bits)
		return -1;

	off = want >> NDENTRYSHIFT;
	i = want & NDENTRYMASK;
	if (i) {
		sub = bitmap[off] | ((u_int)~0 >> (NDENTRIES - i));
		if (sub != ~0)
			goto found;
		off++;
	}

	maxoff = NDLOSLOTS(bits);
	while (off < maxoff) {
		if ((sub = bitmap[off]) != ~0)
			goto found;
		off++;
	}

	return (-1);

 found:
	return (off << NDENTRYSHIFT) + ffs(~sub) - 1;
}

static int
find_last_set(struct filedesc *fd, int last)
{
	int off, i;
	struct file **ofiles = fd->fd_ofiles;
	uint32_t *bitmap = fd->fd_lomap;

	off = (last - 1) >> NDENTRYSHIFT;

	while (off >= 0 && !bitmap[off])
		off--;

	if (off < 0)
		return (-1);

	i = ((off + 1) << NDENTRYSHIFT) - 1;
	if (i >= last)
		i = last - 1;

	while (i > 0 && ofiles[i] == NULL)
		i--;

	return (i);
}

static inline void
fd_used(struct filedesc *fdp, int fd)
{
	u_int off = fd >> NDENTRYSHIFT;

	LOCK_ASSERT(simple_lock_held(&fdp->fd_slock));
	KDASSERT((fdp->fd_lomap[off] & (1 << (fd & NDENTRYMASK))) == 0);

	fdp->fd_lomap[off] |= 1 << (fd & NDENTRYMASK);
	if (fdp->fd_lomap[off] == ~0) {
		KDASSERT((fdp->fd_himap[off >> NDENTRYSHIFT] &
		    (1 << (off & NDENTRYMASK))) == 0);
		fdp->fd_himap[off >> NDENTRYSHIFT] |= 1 << (off & NDENTRYMASK);
	}

	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
}

static inline void
fd_unused(struct filedesc *fdp, int fd)
{
	u_int off = fd >> NDENTRYSHIFT;

	LOCK_ASSERT(simple_lock_held(&fdp->fd_slock));
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;

	if (fdp->fd_lomap[off] == ~0) {
		KDASSERT((fdp->fd_himap[off >> NDENTRYSHIFT] &
		    (1 << (off & NDENTRYMASK))) != 0);
		fdp->fd_himap[off >> NDENTRYSHIFT] &=
		    ~(1 << (off & NDENTRYMASK));
	}
	KDASSERT((fdp->fd_lomap[off] & (1 << (fd & NDENTRYMASK))) != 0);
	fdp->fd_lomap[off] &= ~(1 << (fd & NDENTRYMASK));

#ifdef DIAGNOSTIC
	if (fd > fdp->fd_lastfile)
		panic("fd_unused: fd_lastfile inconsistent");
#endif
	if (fd == fdp->fd_lastfile)
		fdp->fd_lastfile = find_last_set(fdp, fd);
}

/*
 * Lookup the file structure corresponding to a file descriptor
 * and return it locked.
 * Note: typical usage is: `fp = fd_getfile(..); FILE_USE(fp);'
 * The locking strategy has been optimised for this case, i.e.
 * fd_getfile() returns the file locked while FILE_USE() will increment
 * the file's use count and unlock.
 */
struct file *
fd_getfile(struct filedesc *fdp, int fd)
{
	struct file *fp;

	if ((u_int) fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return (NULL);

	simple_lock(&fp->f_slock);
	if (FILE_IS_USABLE(fp) == 0) {
		simple_unlock(&fp->f_slock);
		return (NULL);
	}

	return (fp);
}

/*
 * Common code for dup, dup2, and fcntl(F_DUPFD).
 */
static int
finishdup(struct lwp *l, int old, int new, register_t *retval)
{
	struct filedesc	*fdp;
	struct file	*fp, *delfp;

	fdp = l->l_proc->p_fd;

	/*
	 * If there is a file in the new slot, remember it so we
	 * can close it after we've finished the dup.  We need
	 * to do it after the dup is finished, since closing
	 * the file may block.
	 *
	 * Note: `old' is already used for us.
	 * Note: Caller already marked `new' slot "used".
	 */
	simple_lock(&fdp->fd_slock);
	delfp = fdp->fd_ofiles[new];

	fp = fdp->fd_ofiles[old];
	KDASSERT(fp != NULL);
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	simple_unlock(&fdp->fd_slock);

	*retval = new;
	simple_lock(&fp->f_slock);
	fp->f_count++;
	FILE_UNUSE_HAVELOCK(fp, l);

	if (delfp != NULL) {
		simple_lock(&delfp->f_slock);
		FILE_USE(delfp);
		if (new < fdp->fd_knlistsize)
			knote_fdclose(l, new);
		(void) closef(delfp, l);
	}
	return (0);
}

/*
 * System calls on descriptors.
 */

/*
 * Duplicate a file descriptor.
 */
/* ARGSUSED */
int
sys_dup(struct lwp *l, void *v, register_t *retval)
{
	struct sys_dup_args /* {
		syscallarg(int)	fd;
	} */ *uap = v;
	struct file	*fp;
	struct filedesc	*fdp;
	struct proc	*p;
	int		old, new, error;

	p = l->l_proc;
	fdp = p->p_fd;
	old = SCARG(uap, fd);

 restart:
	if ((fp = fd_getfile(fdp, old)) == NULL)
		return (EBADF);

	FILE_USE(fp);

	if ((error = fdalloc(p, 0, &new)) != 0) {
		if (error == ENOSPC) {
			fdexpand(p);
			FILE_UNUSE(fp, l);
			goto restart;
		}
		FILE_UNUSE(fp, l);
		return (error);
	}

	/* finishdup() will unuse the descriptors for us */
	return (finishdup(l, old, new, retval));
}

/*
 * Duplicate a file descriptor to a particular value.
 */
/* ARGSUSED */
int
sys_dup2(struct lwp *l, void *v, register_t *retval)
{
	struct sys_dup2_args /* {
		syscallarg(int)	from;
		syscallarg(int)	to;
	} */ *uap = v;
	struct file	*fp;
	struct filedesc	*fdp;
	struct proc	*p;
	int		old, new, i, error;

	p = l->l_proc;
	fdp = p->p_fd;
	old = SCARG(uap, from);
	new = SCARG(uap, to);

 restart:
	if ((fp = fd_getfile(fdp, old)) == NULL)
		return (EBADF);

	if ((u_int)new >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
	    (u_int)new >= maxfiles) {
		simple_unlock(&fp->f_slock);
		return (EBADF);
	}

	if (old == new) {
		simple_unlock(&fp->f_slock);
		*retval = new;
		return (0);
	}

	FILE_USE(fp);

	if (new >= fdp->fd_nfiles) {
		if ((error = fdalloc(p, new, &i)) != 0) {
			if (error == ENOSPC) {
				fdexpand(p);
				FILE_UNUSE(fp, l);
				goto restart;
			}
			FILE_UNUSE(fp, l);
			return (error);
		}
		if (new != i)
			panic("dup2: fdalloc");
	} else {
		simple_lock(&fdp->fd_slock);
		/*
		 * Mark `new' slot "used" only if it was empty.
		 */
		if (fdp->fd_ofiles[new] == NULL)
			fd_used(fdp, new);
		simple_unlock(&fdp->fd_slock);
	}

	/*
	 * finishdup() will close the file that's in the `new'
	 * slot, if there's one there.
	 */

	/* finishdup() will unuse the descriptors for us */
	return (finishdup(l, old, new, retval));
}

/*
 * fcntl call which is being passed to the file's fs.
 */
static int
fcntl_forfs(int fd, struct lwp *l, int cmd, void *arg)
{
	struct file	*fp;
	struct filedesc	*fdp;
	int		error;
	u_int		size;
	void		*data, *memp;
#define STK_PARAMS	128
	char		stkbuf[STK_PARAMS];

	/* fd's value was validated in sys_fcntl before calling this routine */
	fdp = l->l_proc->p_fd;
	fp = fdp->fd_ofiles[fd];

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = (size_t)F_PARAM_LEN(cmd);
	if (size > F_PARAM_MAX)
		return (EINVAL);
	memp = NULL;
	if (size > sizeof(stkbuf)) {
		memp = malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	} else
		data = stkbuf;
	if (cmd & F_FSIN) {
		if (size) {
			error = copyin(arg, data, size);
			if (error) {
				if (memp)
					free(memp, M_IOCTLOPS);
				return (error);
			}
		} else
			*(void **)data = arg;
	} else if ((cmd & F_FSOUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		memset(data, 0, size);
	else if (cmd & F_FSVOID)
		*(void **)data = arg;


	error = (*fp->f_ops->fo_fcntl)(fp, cmd, data, l);

	/*
	 * Copy any data to user, size was
	 * already set and checked above.
	 */
	if (error == 0 && (cmd & F_FSOUT) && size)
		error = copyout(data, arg, size);
	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}

/*
 * The file control system call.
 */
/* ARGSUSED */
int
sys_fcntl(struct lwp *l, void *v, register_t *retval)
{
	struct sys_fcntl_args /* {
		syscallarg(int)		fd;
		syscallarg(int)		cmd;
		syscallarg(void *)	arg;
	} */ *uap = v;
	struct filedesc *fdp;
	struct file	*fp;
	struct proc	*p;
	struct vnode	*vp;
	int		fd, i, tmp, error, flg, cmd, newmin;
	struct flock	fl;

	p = l->l_proc;
	fd = SCARG(uap, fd);
	cmd = SCARG(uap, cmd);
	fdp = p->p_fd;
	error = 0;
	flg = F_POSIX;

	switch (cmd) {
	case F_CLOSEM:
		if (fd < 0)
			return EBADF;
		while (fdp->fd_lastfile >= fd)
			fdrelease(l, fdp->fd_lastfile);
		return 0;

	case F_MAXFD:
		*retval = fdp->fd_lastfile;
		return 0;

	default:
		/* Handled below */
		break;
	}

 restart:
	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	FILE_USE(fp);

	if ((cmd & F_FSCTL)) {
		error = fcntl_forfs(fd, l, cmd, SCARG(uap, arg));
		goto out;
	}

	switch (cmd) {

	case F_DUPFD:
		newmin = (long)SCARG(uap, arg);
		if ((u_int)newmin >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
		    (u_int)newmin >= maxfiles) {
			error = EINVAL;
			goto out;
		}
		if ((error = fdalloc(p, newmin, &i)) != 0) {
			if (error == ENOSPC) {
				fdexpand(p);
				FILE_UNUSE(fp, l);
				goto restart;
			}
			goto out;
		}

		/* finishdup() will unuse the descriptors for us */
		return (finishdup(l, fd, i, retval));

	case F_GETFD:
		*retval = fdp->fd_ofileflags[fd] & UF_EXCLOSE ? 1 : 0;
		break;

	case F_SETFD:
		if ((long)SCARG(uap, arg) & 1)
			fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
		else
			fdp->fd_ofileflags[fd] &= ~UF_EXCLOSE;
		break;

	case F_GETFL:
		*retval = OFLAGS(fp->f_flag);
		break;

	case F_SETFL:
		tmp = FFLAGS((long)SCARG(uap, arg)) & FCNTLFLAGS;
		error = (*fp->f_ops->fo_fcntl)(fp, F_SETFL, &tmp, l);
		if (error)
			break;
		i = tmp ^ fp->f_flag;
		if (i & FNONBLOCK) {
			int flgs = tmp & FNONBLOCK;
			error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, &flgs, l);
			if (error)
				goto reset_fcntl;
		}
		if (i & FASYNC) {
			int flgs = tmp & FASYNC;
			error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, &flgs, l);
			if (error) {
				if (i & FNONBLOCK) {
					tmp = fp->f_flag & FNONBLOCK;
					(void)(*fp->f_ops->fo_ioctl)(fp,
						FIONBIO, &tmp, l);
				}
				goto reset_fcntl;
			}
		}
		fp->f_flag = (fp->f_flag & ~FCNTLFLAGS) | tmp;
		break;
	    reset_fcntl:
		(void)(*fp->f_ops->fo_fcntl)(fp, F_SETFL, &fp->f_flag, l);
		break;

	case F_GETOWN:
		error = (*fp->f_ops->fo_ioctl)(fp, FIOGETOWN, &tmp, l);
		*retval = tmp;
		break;

	case F_SETOWN:
		tmp = (int)(intptr_t) SCARG(uap, arg);
		error = (*fp->f_ops->fo_ioctl)(fp, FIOSETOWN, &tmp, l);
		break;

	case F_SETLKW:
		flg |= F_WAIT;
		/* Fall into F_SETLK */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE) {
			error = EINVAL;
			goto out;
		}
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin(SCARG(uap, arg), &fl, sizeof(fl));
		if (error)
			goto out;
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		switch (fl.l_type) {
		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0) {
				error = EBADF;
				goto out;
			}
			p->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, p, F_SETLK, &fl, flg);
			goto out;

		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0) {
				error = EBADF;
				goto out;
			}
			p->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, p, F_SETLK, &fl, flg);
			goto out;

		case F_UNLCK:
			error = VOP_ADVLOCK(vp, p, F_UNLCK, &fl, F_POSIX);
			goto out;

		default:
			error = EINVAL;
			goto out;
		}

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE) {
			error = EINVAL;
			goto out;
		}
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin(SCARG(uap, arg), &fl, sizeof(fl));
		if (error)
			goto out;
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		if (fl.l_type != F_RDLCK &&
		    fl.l_type != F_WRLCK &&
		    fl.l_type != F_UNLCK) {
			error = EINVAL;
			goto out;
		}
		error = VOP_ADVLOCK(vp, p, F_GETLK, &fl, F_POSIX);
		if (error)
			goto out;
		error = copyout(&fl, SCARG(uap, arg), sizeof(fl));
		break;

	default:
		error = EINVAL;
	}

 out:
	FILE_UNUSE(fp, l);
	return (error);
}

void
fdremove(struct filedesc *fdp, int fd)
{

	simple_lock(&fdp->fd_slock);
	fdp->fd_ofiles[fd] = NULL;
	fd_unused(fdp, fd);
	simple_unlock(&fdp->fd_slock);
}

int
fdrelease(struct lwp *l, int fd)
{
	struct proc *p = l->l_proc;
	struct filedesc	*fdp;
	struct file	**fpp, *fp;

	fdp = p->p_fd;
	simple_lock(&fdp->fd_slock);
	if (fd < 0 || fd > fdp->fd_lastfile)
		goto badf;
	fpp = &fdp->fd_ofiles[fd];
	fp = *fpp;
	if (fp == NULL)
		goto badf;

	simple_lock(&fp->f_slock);
	if (!FILE_IS_USABLE(fp)) {
		simple_unlock(&fp->f_slock);
		goto badf;
	}

	FILE_USE(fp);

	*fpp = NULL;
	fdp->fd_ofileflags[fd] = 0;
	fd_unused(fdp, fd);
	simple_unlock(&fdp->fd_slock);
	if (fd < fdp->fd_knlistsize)
		knote_fdclose(l, fd);
	return (closef(fp, l));

badf:
	simple_unlock(&fdp->fd_slock);
	return (EBADF);
}

/*
 * Close a file descriptor.
 */
/* ARGSUSED */
int
sys_close(struct lwp *l, void *v, register_t *retval)
{
	struct sys_close_args /* {
		syscallarg(int)	fd;
	} */ *uap = v;
	int		fd;
	struct filedesc	*fdp;
	struct proc *p;

	p = l->l_proc;
	fd = SCARG(uap, fd);
	fdp = p->p_fd;

#if 0
	if (fd_getfile(fdp, fd) == NULL)
		return (EBADF);
#endif

	return (fdrelease(l, fd));
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
sys___fstat30(struct lwp *l, void *v, register_t *retval)
{
	struct sys___fstat30_args /* {
		syscallarg(int)			fd;
		syscallarg(struct stat *)	sb;
	} */ *uap = v;
	int		fd;
	struct filedesc	*fdp;
	struct file	*fp;
	struct proc	*p;
	struct stat	ub;
	int		error;

	p = l->l_proc;
	fd = SCARG(uap, fd);
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	FILE_USE(fp);
	error = (*fp->f_ops->fo_stat)(fp, &ub, l);
	FILE_UNUSE(fp, l);

	if (error == 0)
		error = copyout(&ub, SCARG(uap, sb), sizeof(ub));

	return (error);
}

/*
 * Return pathconf information about a file descriptor.
 */
/* ARGSUSED */
int
sys_fpathconf(struct lwp *l, void *v, register_t *retval)
{
	struct sys_fpathconf_args /* {
		syscallarg(int)	fd;
		syscallarg(int)	name;
	} */ *uap = v;
	int		fd;
	struct filedesc	*fdp;
	struct file	*fp;
	struct proc 	*p;
	struct vnode	*vp;
	int		error;

	p = l->l_proc;
	fd = SCARG(uap, fd);
	fdp = p->p_fd;
	error = 0;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	FILE_USE(fp);

	switch (fp->f_type) {

	case DTYPE_SOCKET:
	case DTYPE_PIPE:
		if (SCARG(uap, name) != _PC_PIPE_BUF)
			error = EINVAL;
		else
			*retval = PIPE_BUF;
		break;

	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		error = VOP_PATHCONF(vp, SCARG(uap, name), retval);
		break;

	case DTYPE_KQUEUE:
		error = EINVAL;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	FILE_UNUSE(fp, l);
	return (error);
}

/*
 * Allocate a file descriptor for the process.
 */
int	fdexpanded;		/* XXX: what else uses this? */

int
fdalloc(struct proc *p, int want, int *result)
{
	struct filedesc	*fdp;
	int i, lim, last, error;
	u_int off, new;

	fdp = p->p_fd;
	simple_lock(&fdp->fd_slock);

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 */
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	last = min(fdp->fd_nfiles, lim);
 again:
	if ((i = want) < fdp->fd_freefile)
		i = fdp->fd_freefile;
	off = i >> NDENTRYSHIFT;
	new = find_next_zero(fdp->fd_himap, off,
	    (last + NDENTRIES - 1) >> NDENTRYSHIFT);
	if (new != -1) {
		i = find_next_zero(&fdp->fd_lomap[new],
		    new > off ? 0 : i & NDENTRYMASK, NDENTRIES);
		if (i == -1) {
			/*
			 * free file descriptor in this block was
			 * below want, try again with higher want.
			 */
			want = (new + 1) << NDENTRYSHIFT;
			goto again;
		}
		i += (new << NDENTRYSHIFT);
		if (i < last) {
			if (fdp->fd_ofiles[i] == NULL) {
				fd_used(fdp, i);
				if (want <= fdp->fd_freefile)
					fdp->fd_freefile = i;
				*result = i;
				error = 0;
				goto out;
			}
		}
	}

	/* No space in current array.  Expand or let the caller do it. */
	error = (fdp->fd_nfiles >= lim) ? EMFILE : ENOSPC;

out:
	simple_unlock(&fdp->fd_slock);
	return (error);
}

void
fdexpand(struct proc *p)
{
	struct filedesc	*fdp;
	int		i, numfiles, oldnfiles;
	struct file	**newofile;
	char		*newofileflags;
	uint32_t	*newhimap = NULL, *newlomap = NULL;

	fdp = p->p_fd;

restart:
	oldnfiles = fdp->fd_nfiles;

	if (oldnfiles < NDEXTENT)
		numfiles = NDEXTENT;
	else
		numfiles = 2 * oldnfiles;

	newofile = malloc(numfiles * OFILESIZE, M_FILEDESC, M_WAITOK);
	if (NDHISLOTS(numfiles) > NDHISLOTS(oldnfiles)) {
		newhimap = malloc(NDHISLOTS(numfiles) * sizeof(uint32_t),
		    M_FILEDESC, M_WAITOK);
		newlomap = malloc(NDLOSLOTS(numfiles) * sizeof(uint32_t),
		    M_FILEDESC, M_WAITOK);
	}

	simple_lock(&fdp->fd_slock);
	/* lock fdp */
	if (fdp->fd_nfiles != oldnfiles) {
		/* fdp changed; retry */
		simple_unlock(&fdp->fd_slock);
		free(newofile, M_FILEDESC);
		if (newhimap != NULL) free(newhimap, M_FILEDESC);
		if (newlomap != NULL) free(newlomap, M_FILEDESC);
		goto restart;
	}

	newofileflags = (char *) &newofile[numfiles];
	/*
	 * Copy the existing ofile and ofileflags arrays
	 * and zero the new portion of each array.
	 */
	memcpy(newofile, fdp->fd_ofiles,
	    (i = sizeof(struct file *) * fdp->fd_nfiles));
	memset((char *)newofile + i, 0,
	    numfiles * sizeof(struct file *) - i);
	memcpy(newofileflags, fdp->fd_ofileflags,
	    (i = sizeof(char) * fdp->fd_nfiles));
	memset(newofileflags + i, 0, numfiles * sizeof(char) - i);
	if (oldnfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);

	if (NDHISLOTS(numfiles) > NDHISLOTS(oldnfiles)) {
		memcpy(newhimap, fdp->fd_himap,
		    (i = NDHISLOTS(oldnfiles) * sizeof(uint32_t)));
		memset((char *)newhimap + i, 0,
		    NDHISLOTS(numfiles) * sizeof(uint32_t) - i);

		memcpy(newlomap, fdp->fd_lomap,
		    (i = NDLOSLOTS(oldnfiles) * sizeof(uint32_t)));
		memset((char *)newlomap + i, 0,
		    NDLOSLOTS(numfiles) * sizeof(uint32_t) - i);

		if (NDHISLOTS(oldnfiles) > NDHISLOTS(NDFILE)) {
			free(fdp->fd_himap, M_FILEDESC);
			free(fdp->fd_lomap, M_FILEDESC);
		}
		fdp->fd_himap = newhimap;
		fdp->fd_lomap = newlomap;
	}

	fdp->fd_ofiles = newofile;
	fdp->fd_ofileflags = newofileflags;
	fdp->fd_nfiles = numfiles;

	simple_unlock(&fdp->fd_slock);

	fdexpanded++;
}

/*
 * Create a new open file structure and allocate
 * a file descriptor for the process that refers to it.
 */
int
falloc(struct proc *p, struct file **resultfp, int *resultfd)
{
	struct file	*fp, *fq;
	int		error, i;

 restart:
	if ((error = fdalloc(p, 0, &i)) != 0) {
		if (error == ENOSPC) {
			fdexpand(p);
			goto restart;
		}
		return (error);
	}

	fp = pool_get(&file_pool, PR_WAITOK);
	simple_lock(&filelist_slock);
	if (nfiles >= maxfiles) {
		tablefull("file", "increase kern.maxfiles or MAXFILES");
		simple_unlock(&filelist_slock);
		simple_lock(&p->p_fd->fd_slock);
		fd_unused(p->p_fd, i);
		simple_unlock(&p->p_fd->fd_slock);
		pool_put(&file_pool, fp);
		return (ENFILE);
	}
	/*
	 * Allocate a new file descriptor.
	 * If the process has file descriptor zero open, add to the list
	 * of open files at that point, otherwise put it at the front of
	 * the list of open files.
	 */
	nfiles++;
	memset(fp, 0, sizeof(struct file));
	fp->f_iflags = FIF_LARVAL;
	if ((fq = p->p_fd->fd_ofiles[0]) != NULL) {
		LIST_INSERT_AFTER(fq, fp, f_list);
	} else {
		LIST_INSERT_HEAD(&filehead, fp, f_list);
	}
	simple_unlock(&filelist_slock);
	KDASSERT(p->p_fd->fd_ofiles[i] == NULL);
	p->p_fd->fd_ofiles[i] = fp;
	simple_lock_init(&fp->f_slock);
	fp->f_count = 1;
	fp->f_cred = p->p_cred;
	kauth_cred_hold(fp->f_cred);
	if (resultfp) {
		fp->f_usecount = 1;
		*resultfp = fp;
	}
	if (resultfd)
		*resultfd = i;
	return (0);
}

/*
 * Free a file descriptor.
 */
void
ffree(struct file *fp)
{

#ifdef DIAGNOSTIC
	if (fp->f_usecount)
		panic("ffree");
#endif

	simple_lock(&filelist_slock);
	LIST_REMOVE(fp, f_list);
	kauth_cred_free(fp->f_cred);
#ifdef DIAGNOSTIC
	fp->f_count = 0; /* What's the point? */
#endif
	nfiles--;
	simple_unlock(&filelist_slock);
	pool_put(&file_pool, fp);
}

/*
 * Create an initial cwdinfo structure, using the same current and root
 * directories as p.
 */
struct cwdinfo *
cwdinit(struct proc *p)
{
	struct cwdinfo *cwdi;

	cwdi = pool_get(&cwdi_pool, PR_WAITOK);

	simple_lock_init(&cwdi->cwdi_slock);
	cwdi->cwdi_cdir = p->p_cwdi->cwdi_cdir;
	if (cwdi->cwdi_cdir)
		VREF(cwdi->cwdi_cdir);
	cwdi->cwdi_rdir = p->p_cwdi->cwdi_rdir;
	if (cwdi->cwdi_rdir)
		VREF(cwdi->cwdi_rdir);
	cwdi->cwdi_cmask =  p->p_cwdi->cwdi_cmask;
	cwdi->cwdi_refcnt = 1;

	return (cwdi);
}

/*
 * Make p2 share p1's cwdinfo.
 */
void
cwdshare(struct proc *p1, struct proc *p2)
{
	struct cwdinfo *cwdi = p1->p_cwdi;

	simple_lock(&cwdi->cwdi_slock);
	cwdi->cwdi_refcnt++;
	simple_unlock(&cwdi->cwdi_slock);
	p2->p_cwdi = cwdi;
}

/*
 * Make this process not share its cwdinfo structure, maintaining
 * all cwdinfo state.
 */
void
cwdunshare(struct proc *p)
{
	struct cwdinfo *oldcwdi, *newcwdi;

	if (p->p_cwdi->cwdi_refcnt == 1)
		return;

	newcwdi = cwdinit(p);
	oldcwdi = p->p_cwdi;
	p->p_cwdi = newcwdi;
	cwdfree(oldcwdi);
}

/*
 * Release a cwdinfo structure.
 */
void
cwdfree(struct cwdinfo *cwdi)
{
	int n;

	simple_lock(&cwdi->cwdi_slock);
	n = --cwdi->cwdi_refcnt;
	simple_unlock(&cwdi->cwdi_slock);
	if (n > 0)
		return;

	vrele(cwdi->cwdi_cdir);
	if (cwdi->cwdi_rdir)
		vrele(cwdi->cwdi_rdir);
	pool_put(&cwdi_pool, cwdi);
}

/*
 * Create an initial filedesc structure, using the same current and root
 * directories as p.
 */
struct filedesc *
fdinit(struct proc *p)
{
	struct filedesc0 *newfdp;

	newfdp = pool_get(&filedesc0_pool, PR_WAITOK);
	memset(newfdp, 0, sizeof(struct filedesc0));

	fdinit1(newfdp);

	return (&newfdp->fd_fd);
}

/*
 * Initialize a file descriptor table.
 */
void
fdinit1(struct filedesc0 *newfdp)
{

	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_knlistsize = -1;
	newfdp->fd_fd.fd_himap = newfdp->fd_dhimap;
	newfdp->fd_fd.fd_lomap = newfdp->fd_dlomap;
	newfdp->fd_fd.fd_lastfile = -1;
	simple_lock_init(&newfdp->fd_fd.fd_slock);
}

/*
 * Make p2 share p1's filedesc structure.
 */
void
fdshare(struct proc *p1, struct proc *p2)
{
	struct filedesc *fdp = p1->p_fd;

	simple_lock(&fdp->fd_slock);
	p2->p_fd = fdp;
	fdp->fd_refcnt++;
	simple_unlock(&fdp->fd_slock);
}

/*
 * Make this process not share its filedesc structure, maintaining
 * all file descriptor state.
 */
void
fdunshare(struct lwp *l)
{
	struct proc *p = l->l_proc;
	struct filedesc *newfd;

	if (p->p_fd->fd_refcnt == 1)
		return;

	newfd = fdcopy(p);
	fdfree(l);
	p->p_fd = newfd;
}

/*
 * Clear a process's fd table.
 */
void
fdclear(struct lwp *l)
{
	struct proc *p = l->l_proc;
	struct filedesc *newfd;

	newfd = fdinit(p);
	fdfree(l);
	p->p_fd = newfd;
}

/*
 * Copy a filedesc structure.
 */
struct filedesc *
fdcopy(struct proc *p)
{
	struct filedesc	*newfdp, *fdp;
	struct file	**fpp, **nfpp;
	int		i, numfiles, lastfile;

	fdp = p->p_fd;
	newfdp = pool_get(&filedesc0_pool, PR_WAITOK);
	newfdp->fd_refcnt = 1;
	simple_lock_init(&newfdp->fd_slock);

restart:
	numfiles = fdp->fd_nfiles;
	lastfile = fdp->fd_lastfile;

	/*
	 * If the number of open files fits in the internal arrays
	 * of the open file structure, use them, otherwise allocate
	 * additional memory for the number of descriptors currently
	 * in use.
	 */
	if (lastfile < NDFILE) {
		i = NDFILE;
	} else {
		/*
		 * Compute the smallest multiple of NDEXTENT needed
		 * for the file descriptors currently in use,
		 * allowing the table to shrink.
		 */
		i = numfiles;
		while (i >= 2 * NDEXTENT && i > lastfile * 2)
			i /= 2;
		newfdp->fd_ofiles = malloc(i * OFILESIZE, M_FILEDESC, M_WAITOK);
	}
	if (NDHISLOTS(i) > NDHISLOTS(NDFILE)) {
		newfdp->fd_himap = malloc(NDHISLOTS(i) * sizeof(uint32_t),
		    M_FILEDESC, M_WAITOK);
		newfdp->fd_lomap = malloc(NDLOSLOTS(i) * sizeof(uint32_t),
		    M_FILEDESC, M_WAITOK);
	}

	simple_lock(&fdp->fd_slock);
	if (numfiles != fdp->fd_nfiles || lastfile != fdp->fd_lastfile) {
		simple_unlock(&fdp->fd_slock);
		if (i > NDFILE)
			free(newfdp->fd_ofiles, M_FILEDESC);
		if (NDHISLOTS(i) > NDHISLOTS(NDFILE)) {
			free(newfdp->fd_himap, M_FILEDESC);
			free(newfdp->fd_lomap, M_FILEDESC);
		}
		goto restart;
	}

	if (lastfile < NDFILE) {
		newfdp->fd_ofiles = ((struct filedesc0 *) newfdp)->fd_dfiles;
		newfdp->fd_ofileflags =
		    ((struct filedesc0 *) newfdp)->fd_dfileflags;
	} else {
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}
	if (NDHISLOTS(i) <= NDHISLOTS(NDFILE)) {
		newfdp->fd_himap =
		    ((struct filedesc0 *) newfdp)->fd_dhimap;
		newfdp->fd_lomap =
		    ((struct filedesc0 *) newfdp)->fd_dlomap;
	}

	newfdp->fd_nfiles = i;
	newfdp->fd_lastfile = lastfile;
	newfdp->fd_freefile = fdp->fd_freefile;

	/* Clear the entries that will not be copied over.
	 * Avoid calling memset with 0 size (i.e. when
	 * lastfile == i-1 */
	if (lastfile < (i-1))
		memset(newfdp->fd_ofiles + lastfile + 1, 0,
		    (i - lastfile - 1) * sizeof(struct file **));
	memcpy(newfdp->fd_ofileflags, fdp->fd_ofileflags, i * sizeof(char));
	if (i < NDENTRIES * NDENTRIES)
		i = NDENTRIES * NDENTRIES; /* size of inlined bitmaps */
	memcpy(newfdp->fd_himap, fdp->fd_himap, NDHISLOTS(i)*sizeof(uint32_t));
	memcpy(newfdp->fd_lomap, fdp->fd_lomap, NDLOSLOTS(i)*sizeof(uint32_t));

	fpp = fdp->fd_ofiles;
	nfpp = newfdp->fd_ofiles;
	for (i = 0; i <= lastfile; i++, fpp++, nfpp++) {
		if ((*nfpp = *fpp) == NULL)
			continue;

		if ((*fpp)->f_type == DTYPE_KQUEUE)
			/* kq descriptors cannot be copied. */
			fdremove(newfdp, i);
		else {
			simple_lock(&(*fpp)->f_slock);
			(*fpp)->f_count++;
			simple_unlock(&(*fpp)->f_slock);
		}
	}

	simple_unlock(&fdp->fd_slock);

	newfdp->fd_knlist = NULL;
	newfdp->fd_knlistsize = -1;
	newfdp->fd_knhash = NULL;
	newfdp->fd_knhashmask = 0;

	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(struct lwp *l)
{
	struct proc	*p = l->l_proc;
	struct filedesc	*fdp;
	struct file	**fpp, *fp;
	int		i;

	fdp = p->p_fd;
	simple_lock(&fdp->fd_slock);
	i = --fdp->fd_refcnt;
	simple_unlock(&fdp->fd_slock);
	if (i > 0)
		return;

	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i >= 0; i--, fpp++) {
		fp = *fpp;
		if (fp != NULL) {
			*fpp = NULL;
			simple_lock(&fp->f_slock);
			FILE_USE(fp);
			if ((fdp->fd_lastfile - i) < fdp->fd_knlistsize)
				knote_fdclose(l, fdp->fd_lastfile - i);
			(void) closef(fp, l);
		}
	}
	p->p_fd = NULL;
	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);
	if (NDHISLOTS(fdp->fd_nfiles) > NDHISLOTS(NDFILE)) {
		free(fdp->fd_himap, M_FILEDESC);
		free(fdp->fd_lomap, M_FILEDESC);
	}
	if (fdp->fd_knlist)
		free(fdp->fd_knlist, M_KEVENT);
	if (fdp->fd_knhash)
		hashdone(fdp->fd_knhash, M_KEVENT);
	pool_put(&filedesc0_pool, fdp);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 * Note: p may be NULL when closing a file
 * that was being passed in a message.
 *
 * Note: we expect the caller is holding a usecount, and expects us
 * to drop it (the caller thinks the file is going away forever).
 */
int
closef(struct file *fp, struct lwp *l)
{
	struct proc	*p = l ? l->l_proc : NULL;
	struct vnode	*vp;
	struct flock	lf;
	int		error;

	if (fp == NULL)
		return (0);

	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor.
	 */
	if (p && (p->p_flag & P_ADVLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, p, F_UNLCK, &lf, F_POSIX);
	}

	/*
	 * If WANTCLOSE is set, then the reference count on the file
	 * is 0, but there were multiple users of the file.  This can
	 * happen if a filedesc structure is shared by multiple
	 * processes.
	 */
	simple_lock(&fp->f_slock);
	if (fp->f_iflags & FIF_WANTCLOSE) {
		/*
		 * Another user of the file is already closing, and is
		 * simply waiting for other users of the file to drain.
		 * Release our usecount, and wake up the closer if it
		 * is the only remaining use.
		 */
#ifdef DIAGNOSTIC
		if (fp->f_count != 0)
			panic("closef: wantclose and count != 0");
		if (fp->f_usecount < 2)
			panic("closef: wantclose and usecount < 2");
#endif
		if (--fp->f_usecount == 1)
			wakeup(&fp->f_usecount);
		simple_unlock(&fp->f_slock);
		return (0);
	} else {
		/*
		 * Decrement the reference count.  If we were not the
		 * last reference, then release our use and just
		 * return.
		 */
		if (--fp->f_count > 0) {
#ifdef DIAGNOSTIC
			if (fp->f_usecount < 1)
				panic("closef: no wantclose and usecount < 1");
#endif
			fp->f_usecount--;
			simple_unlock(&fp->f_slock);
			return (0);
		}
	}

	/*
	 * The reference count is now 0.  However, there may be
	 * multiple potential users of this file.  This can happen
	 * if multiple processes shared a single filedesc structure.
	 *
	 * Notify these potential users that the file is closing.
	 * This will prevent them from adding additional uses to
	 * the file.
	 */
	fp->f_iflags |= FIF_WANTCLOSE;

	/*
	 * We expect the caller to add a use to the file.  So, if we
	 * are the last user, usecount will be 1.  If it is not, we
	 * must wait for the usecount to drain.  When it drains back
	 * to 1, we will be awakened so that we may proceed with the
	 * close.
	 */
#ifdef DIAGNOSTIC
	if (fp->f_usecount < 1)
		panic("closef: usecount < 1");
#endif
	while (fp->f_usecount > 1)
		(void) ltsleep(&fp->f_usecount, PRIBIO, "closef", 0,
				&fp->f_slock);
#ifdef DIAGNOSTIC
	if (fp->f_usecount != 1)
		panic("closef: usecount != 1");
#endif

	simple_unlock(&fp->f_slock);
	if ((fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, fp, F_UNLCK, &lf, F_FLOCK);
	}
	if (fp->f_ops)
		error = (*fp->f_ops->fo_close)(fp, l);
	else
		error = 0;

	/* Nothing references the file now, drop the final use (us). */
	fp->f_usecount--;

	ffree(fp);
	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on
 * the entire file (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
/* ARGSUSED */
int
sys_flock(struct lwp *l, void *v, register_t *retval)
{
	struct sys_flock_args /* {
		syscallarg(int)	fd;
		syscallarg(int)	how;
	} */ *uap = v;
	int		fd, how, error;
	struct proc	*p;
	struct filedesc	*fdp;
	struct file	*fp;
	struct vnode	*vp;
	struct flock	lf;

	p = l->l_proc;
	fd = SCARG(uap, fd);
	how = SCARG(uap, how);
	fdp = p->p_fd;
	error = 0;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	FILE_USE(fp);

	if (fp->f_type != DTYPE_VNODE) {
		error = EOPNOTSUPP;
		goto out;
	}

	vp = (struct vnode *)fp->f_data;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		fp->f_flag &= ~FHASLOCK;
		error = VOP_ADVLOCK(vp, fp, F_UNLCK, &lf, F_FLOCK);
		goto out;
	}
	if (how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else {
		error = EINVAL;
		goto out;
	}
	fp->f_flag |= FHASLOCK;
	if (how & LOCK_NB)
		error = VOP_ADVLOCK(vp, fp, F_SETLK, &lf, F_FLOCK);
	else
		error = VOP_ADVLOCK(vp, fp, F_SETLK, &lf,
		    F_FLOCK|F_WAIT);
 out:
	FILE_UNUSE(fp, l);
	return (error);
}

/* ARGSUSED */
int
sys_posix_fadvise(struct lwp *l, void *v, register_t *retval)
{
	const struct sys_posix_fadvise_args /* {
		syscallarg(int) fd;
		syscallarg(off_t) offset;
		syscallarg(off_t) len;
		syscallarg(int) advice;
	} */ *uap = v;
	const int fd = SCARG(uap, fd);
	const int advice = SCARG(uap, advice);
	struct proc *p = l->l_proc;
	struct file *fp;
	int error = 0;

	fp = fd_getfile(p->p_fd, fd);
	if (fp == NULL) {
		error = EBADF;
		goto out;
	}
	FILE_USE(fp);

	if (fp->f_type != DTYPE_VNODE) {
		if (fp->f_type == DTYPE_PIPE || fp->f_type == DTYPE_SOCKET) {
			error = ESPIPE;
		} else {
			error = EOPNOTSUPP;
		}
		goto out;
	}

	switch (advice) {
	case POSIX_FADV_NORMAL:
	case POSIX_FADV_RANDOM:
	case POSIX_FADV_SEQUENTIAL:
		KASSERT(POSIX_FADV_NORMAL == UVM_ADV_NORMAL);
		KASSERT(POSIX_FADV_RANDOM == UVM_ADV_RANDOM);
		KASSERT(POSIX_FADV_SEQUENTIAL == UVM_ADV_SEQUENTIAL);

		/*
		 * we ignore offset and size.
		 */

		fp->f_advice = advice;
		break;

	case POSIX_FADV_WILLNEED:
	case POSIX_FADV_DONTNEED:
	case POSIX_FADV_NOREUSE:

		/*
		 * not implemented yet.
		 */

		break;
	default:
		error = EINVAL;
		break;
	}
out:
	if (fp != NULL) {
		FILE_UNUSE(fp, l);
	}
	*retval = error;
	return 0;
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 */
/* ARGSUSED */
static int
filedescopen(dev_t dev, int mode, int type, struct lwp *l)
{

	/*
	 * XXX Kludge: set dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	l->l_dupfd = minor(dev);	/* XXX */
	return EDUPFD;
}

const struct cdevsw filedesc_cdevsw = {
	filedescopen, noclose, noread, nowrite, noioctl,
	    nostop, notty, nopoll, nommap, nokqfilter,
};

/*
 * Duplicate the specified descriptor to a free descriptor.
 *
 * 'indx' has been fdalloc'ed (and will be fdremove'ed on error) by the caller.
 */
int
dupfdopen(struct lwp *l, int indx, int dfd, int mode, int error)
{
	struct proc	*p = l->l_proc;
	struct filedesc *fdp;
	struct file	*wfp;

	fdp = p->p_fd;

	/* should be cleared by the caller */
	KASSERT(fdp->fd_ofiles[indx] == NULL);

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject.
	 */

	/*
	 * Note, in the case of indx == dfd, fd_getfile below returns NULL.
	 */
	if ((wfp = fd_getfile(fdp, dfd)) == NULL)
		return (EBADF);

	FILE_USE(wfp);

	/*
	 * There are two cases of interest here.
	 *
	 * For EDUPFD simply dup (dfd) to file descriptor
	 * (indx) and return.
	 *
	 * For EMOVEFD steal away the file structure from (dfd) and
	 * store it in (indx).  (dfd) is effectively closed by
	 * this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case EDUPFD:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag) {
			FILE_UNUSE(wfp, l);
			return (EACCES);
		}
		simple_lock(&fdp->fd_slock);
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		simple_unlock(&fdp->fd_slock);
		simple_lock(&wfp->f_slock);
		wfp->f_count++;
		/* 'indx' has been fd_used'ed by caller */
		FILE_UNUSE_HAVELOCK(wfp, l);
		return (0);

	case EMOVEFD:
		/*
		 * Steal away the file pointer from dfd, and stuff it into indx.
		 */
		simple_lock(&fdp->fd_slock);
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[dfd] = 0;
		/*
		 * Complete the clean up of the filedesc structure by
		 * recomputing the various hints.
		 */
		/* 'indx' has been fd_used'ed by caller */
		fd_unused(fdp, dfd);
		simple_unlock(&fdp->fd_slock);
		FILE_UNUSE(wfp, l);
		return (0);

	default:
		FILE_UNUSE(wfp, l);
		return (error);
	}
	/* NOTREACHED */
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(struct lwp *l)
{
	struct proc	*p = l->l_proc;
	struct filedesc *fdp;
	int		fd;

	fdunshare(l);
	cwdunshare(p);

	fdp = p->p_fd;
	for (fd = 0; fd <= fdp->fd_lastfile; fd++)
		if (fdp->fd_ofileflags[fd] & UF_EXCLOSE)
			(void) fdrelease(l, fd);
}

/*
 * It is unsafe for set[ug]id processes to be started with file
 * descriptors 0..2 closed, as these descriptors are given implicit
 * significance in the Standard C library.  fdcheckstd() will create a
 * descriptor referencing /dev/null for each of stdin, stdout, and
 * stderr that is not already open.
 */
#define CHECK_UPTO 3
int
fdcheckstd(l)
	struct lwp *l;
{
	struct proc *p;
	struct nameidata nd;
	struct filedesc *fdp;
	struct file *fp;
	struct file *devnullfp = NULL;	/* Quell compiler warning */
	struct proc *pp;
	register_t retval;
	int fd, i, error, flags = FREAD|FWRITE, devnull = -1;
	char closed[CHECK_UPTO * 3 + 1], which[3 + 1];

	p = l->l_proc;
	closed[0] = '\0';
	if ((fdp = p->p_fd) == NULL)
		return (0);
	for (i = 0; i < CHECK_UPTO; i++) {
		if (fdp->fd_ofiles[i] != NULL)
			continue;
		snprintf(which, sizeof(which), ",%d", i);
		strlcat(closed, which, sizeof(closed));
		if (devnullfp == NULL) {
			if ((error = falloc(p, &fp, &fd)) != 0)
				return (error);
			NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, "/dev/null",
			    l);
			if ((error = vn_open(&nd, flags, 0)) != 0) {
				FILE_UNUSE(fp, l);
				ffree(fp);
				fdremove(p->p_fd, fd);
				return (error);
			}
			fp->f_data = nd.ni_vp;
			fp->f_flag = flags;
			fp->f_ops = &vnops;
			fp->f_type = DTYPE_VNODE;
			VOP_UNLOCK(nd.ni_vp, 0);
			devnull = fd;
			devnullfp = fp;
			FILE_SET_MATURE(fp);
		} else {
restart:
			if ((error = fdalloc(p, 0, &fd)) != 0) {
				if (error == ENOSPC) {
					fdexpand(p);
					goto restart;
				}
				return (error);
			}

			simple_lock(&devnullfp->f_slock);
			FILE_USE(devnullfp);
			/* finishdup() will unuse the descriptors for us */
			if ((error = finishdup(l, devnull, fd, &retval)) != 0)
				return (error);
		}
	}
	if (devnullfp)
		FILE_UNUSE(devnullfp, l);
	if (closed[0] != '\0') {
		pp = p->p_pptr;
		log(LOG_WARNING, "set{u,g}id pid %d (%s) "
		    "was invoked by uid %d ppid %d (%s) "
		    "with fd %s closed\n",
		    p->p_pid, p->p_comm, kauth_cred_geteuid(pp->p_cred),
		    pp->p_pid, pp->p_comm, &closed[1]);
	}
	return (0);
}
#undef CHECK_UPTO

/*
 * Sets descriptor owner. If the owner is a process, 'pgid'
 * is set to positive value, process ID. If the owner is process group,
 * 'pgid' is set to -pg_id.
 */
int
fsetown(struct proc *p, pid_t *pgid, int cmd, const void *data)
{
	int id = *(const int *)data;
	int error;

	switch (cmd) {
	case TIOCSPGRP:
		if (id < 0)
			return (EINVAL);
		id = -id;
		break;
	default:
		break;
	}

	if (id > 0 && !pfind(id))
		return (ESRCH);
	else if (id < 0 && (error = pgid_in_session(p, -id)))
		return (error);

	*pgid = id;
	return (0);
}

/*
 * Return descriptor owner information. If the value is positive,
 * it's process ID. If it's negative, it's process group ID and
 * needs the sign removed before use.
 */
int
fgetown(struct proc *p, pid_t pgid, int cmd, void *data)
{
	switch (cmd) {
	case TIOCGPGRP:
		*(int *)data = -pgid;
		break;
	default:
		*(int *)data = pgid;
		break;
	}
	return (0);
}

/*
 * Send signal to descriptor owner, either process or process group.
 */
void
fownsignal(pid_t pgid, int signo, int code, int band, void *fdescdata)
{
	struct proc *p1;
	ksiginfo_t ksi;

	memset(&ksi, 0, sizeof(ksi));
	ksi.ksi_signo = signo;
	ksi.ksi_code = code;
	ksi.ksi_band = band;

	if (pgid > 0 && (p1 = pfind(pgid)))
		kpsignal(p1, &ksi, fdescdata);
	else if (pgid < 0)
		kgsignal(-pgid, &ksi, fdescdata);
}

int
fdclone(struct lwp *l, struct file *fp, int fd, int flag,
    const struct fileops *fops, void *data)
{
	fp->f_flag = flag;
	fp->f_type = DTYPE_MISC;
	fp->f_ops = fops;
	fp->f_data = data;

	l->l_dupfd = fd;

	FILE_SET_MATURE(fp);
	FILE_UNUSE(fp, l);
	return EMOVEFD;
}

/* ARGSUSED */
int
fnullop_fcntl(struct file *fp, u_int cmd, void *data, struct lwp *l)
{
	if (cmd == F_SETFL)
		return 0;

	return EOPNOTSUPP;
}

/* ARGSUSED */
int
fnullop_poll(struct file *fp, int which, struct lwp *l)
{
	return 0;
}


/* ARGSUSED */
int
fnullop_kqfilter(struct file *fp, struct knote *kn)
{

	return 0;
}

/* ARGSUSED */
int
fbadop_stat(struct file *fp, struct stat *sb, struct lwp *l)
{
	return EOPNOTSUPP;
}
