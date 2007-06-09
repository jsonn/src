/*	$NetBSD: sys_generic.c,v 1.100.2.7 2007/06/09 23:58:06 ad Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)sys_generic.c	8.9 (Berkeley) 2/14/95
 */

/*
 * System calls relating to files.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_generic.c,v 1.100.2.7 2007/06/09 23:58:06 ad Exp $");

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <uvm/uvm_extern.h>

/* Flags for lwp::l_selflag. */
#define	SEL_RESET	0	/* awoken, interrupted, or not yet polling */
#define	SEL_SCANNING	1	/* polling descriptors */
#define	SEL_BLOCKING	2	/* about to block on select_cv */

static int	selscan(lwp_t *, fd_mask *, fd_mask *, int, register_t *);
static int	pollscan(lwp_t *, struct pollfd *, int, register_t *);
static void	selclear(void);

/* Global state for select()/poll(). */
kmutex_t	select_lock;
kcondvar_t	select_cv;
int		nselcoll;

/*
 * Read system call.
 */
/* ARGSUSED */
int
sys_read(lwp_t *l, void *v, register_t *retval)
{
	struct sys_read_args /* {
		syscallarg(int)		fd;
		syscallarg(void *)	buf;
		syscallarg(size_t)	nbyte;
	} */ *uap = v;
	int		fd;
	struct file	*fp;
	proc_t		*p;
	struct filedesc	*fdp;

	fd = SCARG(uap, fd);
	p = l->l_proc;
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FREAD) == 0) {
		mutex_exit(&fp->f_lock);
		return (EBADF);
	}

	FILE_USE(fp);

	/* dofileread() will unuse the descriptor for us */
	return (dofileread(l, fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &fp->f_offset, FOF_UPDATE_OFFSET, retval));
}

int
dofileread(lwp_t *l, int fd, struct file *fp, void *buf, size_t nbyte,
	off_t *offset, int flags, register_t *retval)
{
	struct iovec aiov;
	struct uio auio;
	proc_t *p;
	struct vmspace *vm;
	size_t cnt;
	int error;
#ifdef KTRACE
	struct iovec	ktriov;
#endif
	p = l->l_proc;

	error = proc_vmspace_getref(p, &vm);
	if (error) {
		goto out;
	}

	aiov.iov_base = (void *)buf;
	aiov.iov_len = nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = nbyte;
	auio.uio_rw = UIO_READ;
	auio.uio_vmspace = vm;

	/*
	 * Reads return ssize_t because -1 is returned on error.  Therefore
	 * we must restrict the length to SSIZE_MAX to avoid garbage return
	 * values.
	 */
	if (auio.uio_resid > SSIZE_MAX) {
		error = EINVAL;
		goto out;
	}

#ifdef KTRACE
	/* In case we are tracing, save a copy of iovec */
	ktriov = aiov;
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_read)(fp, offset, &auio, fp->f_cred, flags);
	if (error)
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(l, fd, UIO_READ, &ktriov, cnt, error);
#endif
	*retval = cnt;
 out:
	FILE_UNUSE(fp, l);
	uvmspace_free(vm);
	return (error);
}

/*
 * Scatter read system call.
 */
int
sys_readv(lwp_t *l, void *v, register_t *retval)
{
	struct sys_readv_args /* {
		syscallarg(int)				fd;
		syscallarg(const struct iovec *)	iovp;
		syscallarg(int)				iovcnt;
	} */ *uap = v;
	struct filedesc	*fdp;
	struct file *fp;
	proc_t *p;
	int fd;

	fd = SCARG(uap, fd);
	p = l->l_proc;
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FREAD) == 0) {
		mutex_exit(&fp->f_lock);
		return (EBADF);
	}

	FILE_USE(fp);

	/* dofilereadv() will unuse the descriptor for us */
	return (dofilereadv(l, fd, fp, SCARG(uap, iovp), SCARG(uap, iovcnt),
	    &fp->f_offset, FOF_UPDATE_OFFSET, retval));
}

int
dofilereadv(lwp_t *l, int fd, struct file *fp, const struct iovec *iovp,
	int iovcnt, off_t *offset, int flags, register_t *retval)
{
	proc_t		*p;
	struct uio	auio;
	struct iovec	*iov, *needfree, aiov[UIO_SMALLIOV];
	struct vmspace	*vm;
	int		i, error;
	size_t		cnt;
	u_int		iovlen;
#ifdef KTRACE
	struct iovec	*ktriov;
#endif

	p = l->l_proc;
	error = proc_vmspace_getref(p, &vm);
	if (error) {
		goto out;
	}

#ifdef KTRACE
	ktriov = NULL;
#endif
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = iovcnt * sizeof(struct iovec);
	if ((u_int)iovcnt > UIO_SMALLIOV) {
		if ((u_int)iovcnt > IOV_MAX) {
			error = EINVAL;
			goto out;
		}
		iov = kmem_alloc(iovlen, KM_SLEEP);
		needfree = iov;
	} else if ((u_int)iovcnt > 0) {
		iov = aiov;
		needfree = NULL;
	} else {
		error = EINVAL;
		goto out;
	}

	auio.uio_iov = iov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_rw = UIO_READ;
	auio.uio_vmspace = vm;
	error = copyin(iovp, iov, iovlen);
	if (error)
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		auio.uio_resid += iov->iov_len;
		/*
		 * Reads return ssize_t because -1 is returned on error.
		 * Therefore we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.
		 */
		if (iov->iov_len > SSIZE_MAX || auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		ktriov = kmem_alloc(iovlen, KM_SLEEP);
		memcpy((void *)ktriov, (void *)auio.uio_iov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_read)(fp, offset, &auio, fp->f_cred, flags);
	if (error)
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (KTRPOINT(p, KTR_GENIO) && (error == 0))
			ktrgenio(l, fd, UIO_READ, ktriov, cnt, error);
		kmem_free(ktriov, iovlen);
	}
#endif
	*retval = cnt;
 done:
	if (needfree)
		kmem_free(needfree, iovlen);
 out:
	FILE_UNUSE(fp, l);
	uvmspace_free(vm);
	return (error);
}

/*
 * Write system call
 */
int
sys_write(lwp_t *l, void *v, register_t *retval)
{
	struct sys_write_args /* {
		syscallarg(int)			fd;
		syscallarg(const void *)	buf;
		syscallarg(size_t)		nbyte;
	} */ *uap = v;
	int		fd;
	struct file	*fp;
	proc_t		*p;
	struct filedesc	*fdp;

	fd = SCARG(uap, fd);
	p = l->l_proc;
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FWRITE) == 0) {
		mutex_exit(&fp->f_lock);
		return (EBADF);
	}

	FILE_USE(fp);

	/* dofilewrite() will unuse the descriptor for us */
	return (dofilewrite(l, fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &fp->f_offset, FOF_UPDATE_OFFSET, retval));
}

int
dofilewrite(lwp_t *l, int fd, struct file *fp, const void *buf,
	size_t nbyte, off_t *offset, int flags, register_t *retval)
{
	struct iovec aiov;
	struct uio auio;
	proc_t *p;
	struct vmspace *vm;
	size_t cnt;
	int error;
#ifdef KTRACE
	struct iovec	ktriov;
#endif

	p = l->l_proc;
	error = proc_vmspace_getref(p, &vm);
	if (error) {
		goto out;
	}
	aiov.iov_base = __UNCONST(buf);		/* XXXUNCONST kills const */
	aiov.iov_len = nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = nbyte;
	auio.uio_rw = UIO_WRITE;
	auio.uio_vmspace = vm;

	/*
	 * Writes return ssize_t because -1 is returned on error.  Therefore
	 * we must restrict the length to SSIZE_MAX to avoid garbage return
	 * values.
	 */
	if (auio.uio_resid > SSIZE_MAX) {
		error = EINVAL;
		goto out;
	}

#ifdef KTRACE
	/* In case we are tracing, save a copy of iovec */
	ktriov = aiov;
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_write)(fp, offset, &auio, fp->f_cred, flags);
	if (error) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE) {
			mutex_enter(&proclist_mutex);
			psignal(p, SIGPIPE);
			mutex_exit(&proclist_mutex);
		}
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(l, fd, UIO_WRITE, &ktriov, cnt, error);
#endif
	*retval = cnt;
 out:
	FILE_UNUSE(fp, l);
	uvmspace_free(vm);
	return (error);
}

/*
 * Gather write system call
 */
int
sys_writev(lwp_t *l, void *v, register_t *retval)
{
	struct sys_writev_args /* {
		syscallarg(int)				fd;
		syscallarg(const struct iovec *)	iovp;
		syscallarg(int)				iovcnt;
	} */ *uap = v;
	int		fd;
	struct file	*fp;
	proc_t		*p;
	struct filedesc	*fdp;

	fd = SCARG(uap, fd);
	p = l->l_proc;
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FWRITE) == 0) {
		mutex_exit(&fp->f_lock);
		return (EBADF);
	}

	FILE_USE(fp);

	/* dofilewritev() will unuse the descriptor for us */
	return (dofilewritev(l, fd, fp, SCARG(uap, iovp), SCARG(uap, iovcnt),
	    &fp->f_offset, FOF_UPDATE_OFFSET, retval));
}

int
dofilewritev(lwp_t *l, int fd, struct file *fp, const struct iovec *iovp,
	int iovcnt, off_t *offset, int flags, register_t *retval)
{
	proc_t		*p;
	struct uio	auio;
	struct iovec	*iov, *needfree, aiov[UIO_SMALLIOV];
	struct vmspace	*vm;
	int		i, error;
	size_t		cnt;
	u_int		iovlen;
#ifdef KTRACE
	struct iovec	*ktriov;
#endif

	p = l->l_proc;
	error = proc_vmspace_getref(p, &vm);
	if (error) {
		goto out;
	}
#ifdef KTRACE
	ktriov = NULL;
#endif
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = iovcnt * sizeof(struct iovec);
	if ((u_int)iovcnt > UIO_SMALLIOV) {
		if ((u_int)iovcnt > IOV_MAX) {
			error = EINVAL;
			goto out;
		}
		iov = kmem_alloc(iovlen, KM_SLEEP);
		needfree = iov;
	} else if ((u_int)iovcnt > 0) {
		iov = aiov;
		needfree = NULL;
	} else {
		error = EINVAL;
		goto out;
	}

	auio.uio_iov = iov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_rw = UIO_WRITE;
	auio.uio_vmspace = vm;
	error = copyin(iovp, iov, iovlen);
	if (error)
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		auio.uio_resid += iov->iov_len;
		/*
		 * Writes return ssize_t because -1 is returned on error.
		 * Therefore we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.
		 */
		if (iov->iov_len > SSIZE_MAX || auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto done;
		}
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		ktriov = kmem_alloc(iovlen, KM_SLEEP);
		memcpy((void *)ktriov, (void *)auio.uio_iov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_write)(fp, offset, &auio, fp->f_cred, flags);
	if (error) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE) {
			mutex_enter(&proclist_mutex);
			psignal(p, SIGPIPE);
			mutex_exit(&proclist_mutex);
		}
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (KTRPOINT(p, KTR_GENIO) && (error == 0))
			ktrgenio(l, fd, UIO_WRITE, ktriov, cnt, error);
		kmem_free(ktriov, iovlen);
	}
#endif
	*retval = cnt;
 done:
	if (needfree)
		kmem_free(needfree, iovlen);
 out:
	FILE_UNUSE(fp, l);
	uvmspace_free(vm);
	return (error);
}

/*
 * Ioctl system call
 */
/* ARGSUSED */
int
sys_ioctl(lwp_t *l, void *v, register_t *retval)
{
	struct sys_ioctl_args /* {
		syscallarg(int)		fd;
		syscallarg(u_long)	com;
		syscallarg(void *)	data;
	} */ *uap = v;
	struct file	*fp;
	proc_t		*p;
	struct filedesc	*fdp;
	u_long		com;
	int		error;
	u_int		size;
	void 		*data, *memp;
#define	STK_PARAMS	128
	u_long		stkbuf[STK_PARAMS/sizeof(u_long)];

	error = 0;
	p = l->l_proc;
	fdp = p->p_fd;

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);

	FILE_USE(fp);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0) {
		error = EBADF;
		com = 0;
		goto out;
	}

	switch (com = SCARG(uap, com)) {
	case FIONCLEX:
		rw_enter(&fdp->fd_lock, RW_WRITER);
		fdp->fd_ofileflags[SCARG(uap, fd)] &= ~UF_EXCLOSE;
		rw_exit(&fdp->fd_lock);
		goto out;

	case FIOCLEX:
		rw_enter(&fdp->fd_lock, RW_WRITER);
		fdp->fd_ofileflags[SCARG(uap, fd)] |= UF_EXCLOSE;
		rw_exit(&fdp->fd_lock);
		goto out;
	}

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX) {
		error = ENOTTY;
		goto out;
	}
	memp = NULL;
	if (size > sizeof(stkbuf)) {
		memp = kmem_alloc(size, KM_SLEEP);
		data = memp;
	} else
		data = (void *)stkbuf;
	if (com&IOC_IN) {
		if (size) {
			error = copyin(SCARG(uap, data), data, size);
			if (error) {
				if (memp)
					kmem_free(memp, size);
				goto out;
			}
#ifdef KTRACE
			if (KTRPOINT(p, KTR_GENIO)) {
				struct iovec iov;
				iov.iov_base = SCARG(uap, data);
				iov.iov_len = size;
				ktrgenio(l, SCARG(uap, fd), UIO_WRITE, &iov,
					size, 0);
			}
#endif
		} else
			*(void **)data = SCARG(uap, data);
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		memset(data, 0, size);
	else if (com&IOC_VOID)
		*(void **)data = SCARG(uap, data);

	switch (com) {

	case FIONBIO:
		mutex_enter(&fp->f_lock);
		if (*(int *)data != 0)
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		mutex_exit(&fp->f_lock);
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, data, l);
		break;

	case FIOASYNC:
		mutex_enter(&fp->f_lock);
		if (*(int *)data != 0)
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		mutex_exit(&fp->f_lock);
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, data, l);
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, l);
		/*
		 * Copy any data to user, size was
		 * already set and checked above.
		 */
		if (error == 0 && (com&IOC_OUT) && size) {
			error = copyout(data, SCARG(uap, data), size);
#ifdef KTRACE
			if (KTRPOINT(p, KTR_GENIO)) {
				struct iovec iov;
				iov.iov_base = SCARG(uap, data);
				iov.iov_len = size;
				ktrgenio(l, SCARG(uap, fd), UIO_READ, &iov,
					size, error);
			}
#endif
		}
		break;
	}
	if (memp)
		kmem_free(memp, size);
 out:
	FILE_UNUSE(fp, l);
	switch (error) {
	case -1:
		printf("sys_ioctl: _IO%s%s('%c', %lu, %lu) returned -1: "
		    "pid=%d comm=%s\n",
		    (com & IOC_IN) ? "W" : "", (com & IOC_OUT) ? "R" : "",
		    (char)IOCGROUP(com), (com & 0xff), IOCPARM_LEN(com),
		    p->p_pid, p->p_comm);
		/* FALLTHROUGH */
	case EPASSTHROUGH:
		error = ENOTTY;
		/* FALLTHROUGH */
	default:
		return (error);
	}
}

/*
 * Select system call.
 */
int
sys_pselect(lwp_t *l, void *v, register_t *retval)
{
	struct sys_pselect_args /* {
		syscallarg(int)				nd;
		syscallarg(fd_set *)			in;
		syscallarg(fd_set *)			ou;
		syscallarg(fd_set *)			ex;
		syscallarg(const struct timespec *)	ts;
		syscallarg(sigset_t *)			mask;
	} */ * const uap = v;
	struct timespec	ats;
	struct timeval	atv, *tv = NULL;
	sigset_t	amask, *mask = NULL;
	int		error;

	if (SCARG(uap, ts)) {
		error = copyin(SCARG(uap, ts), &ats, sizeof(ats));
		if (error)
			return error;
		atv.tv_sec = ats.tv_sec;
		atv.tv_usec = ats.tv_nsec / 1000;
		tv = &atv;
	}
	if (SCARG(uap, mask) != NULL) {
		error = copyin(SCARG(uap, mask), &amask, sizeof(amask));
		if (error)
			return error;
		mask = &amask;
	}

	return selcommon(l, retval, SCARG(uap, nd), SCARG(uap, in),
	    SCARG(uap, ou), SCARG(uap, ex), tv, mask);
}

int
inittimeleft(struct timeval *tv, struct timeval *sleeptv)
{
	if (itimerfix(tv))
		return -1;
	getmicrouptime(sleeptv);
	return 0;
}

int
gettimeleft(struct timeval *tv, struct timeval *sleeptv)
{
	/*
	 * We have to recalculate the timeout on every retry.
	 */
	struct timeval slepttv;
	/*
	 * reduce tv by elapsed time
	 * based on monotonic time scale
	 */
	getmicrouptime(&slepttv);
	timeradd(tv, sleeptv, tv);
	timersub(tv, &slepttv, tv);
	*sleeptv = slepttv;
	return tvtohz(tv);
}

int
sys_select(lwp_t *l, void *v, register_t *retval)
{
	struct sys_select_args /* {
		syscallarg(int)			nd;
		syscallarg(fd_set *)		in;
		syscallarg(fd_set *)		ou;
		syscallarg(fd_set *)		ex;
		syscallarg(struct timeval *)	tv;
	} */ * const uap = v;
	struct timeval atv, *tv = NULL;
	int error;

	if (SCARG(uap, tv)) {
		error = copyin(SCARG(uap, tv), (void *)&atv,
			sizeof(atv));
		if (error)
			return error;
		tv = &atv;
	}

	return selcommon(l, retval, SCARG(uap, nd), SCARG(uap, in),
	    SCARG(uap, ou), SCARG(uap, ex), tv, NULL);
}

int
selcommon(lwp_t *l, register_t *retval, int nd, fd_set *u_in,
	  fd_set *u_ou, fd_set *u_ex, struct timeval *tv, sigset_t *mask)
{
	char		smallbits[howmany(FD_SETSIZE, NFDBITS) *
			    sizeof(fd_mask) * 6];
	proc_t		* const p = l->l_proc;
	char 		*bits;
	int		ncoll, error, timo;
	size_t		ni;
	sigset_t	oldmask;
	struct timeval  sleeptv;

	error = 0;
	if (nd < 0)
		return (EINVAL);
	if (nd > p->p_fd->fd_nfiles) {
		/* forgiving; slightly wrong */
		nd = p->p_fd->fd_nfiles;
	}
	ni = howmany(nd, NFDBITS) * sizeof(fd_mask);
	if (ni * 6 > sizeof(smallbits))
		bits = kmem_alloc(ni * 6, KM_SLEEP);
	else
		bits = smallbits;

#define	getbits(name, x)						\
	if (u_ ## name) {						\
		error = copyin(u_ ## name, bits + ni * x, ni);		\
		if (error)						\
			goto done;					\
	} else								\
		memset(bits + ni * x, 0, ni);
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits

	timo = 0;
	if (tv && inittimeleft(tv, &sleeptv) == -1) {
		error = EINVAL;
		goto done;
	}

	if (mask) {
		sigminusset(&sigcantmask, mask);
		mutex_enter(&p->p_smutex);
		oldmask = l->l_sigmask;
		l->l_sigmask = *mask;
		mutex_exit(&p->p_smutex);
	} else
		oldmask = l->l_sigmask;	/* XXXgcc */

	mutex_enter(&select_lock);
	SLIST_INIT(&l->l_selwait);
	for (;;) {
	 	l->l_selflag = SEL_SCANNING;
		ncoll = nselcoll;
 		mutex_exit(&select_lock);

		error = selscan(l, (fd_mask *)(bits + ni * 0),
		    (fd_mask *)(bits + ni * 3), nd, retval);

		mutex_enter(&select_lock);
		if (error || *retval)
			break;
		if (tv && (timo = gettimeleft(tv, &sleeptv)) <= 0)
			break;
		if (l->l_selflag != SEL_SCANNING || ncoll != nselcoll)
			continue;
		l->l_selflag = SEL_BLOCKING;
		error = cv_timedwait_sig(&select_cv, &select_lock, timo);
		if (error != 0)
			break;
	}
	selclear();
	mutex_exit(&select_lock);

	if (mask) {
		mutex_enter(&p->p_smutex);
		l->l_sigmask = oldmask;
		mutex_exit(&p->p_smutex);
	}

 done:
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	if (error == 0 && u_in != NULL)
		error = copyout(bits + ni * 3, u_in, ni);
	if (error == 0 && u_ou != NULL)
		error = copyout(bits + ni * 4, u_ou, ni);
	if (error == 0 && u_ex != NULL)
		error = copyout(bits + ni * 5, u_ex, ni);
	if (bits != smallbits)
		kmem_free(bits, ni * 6);
	return (error);
}

int
selscan(lwp_t *l, fd_mask *ibitp, fd_mask *obitp, int nfd,
	register_t *retval)
{
	static const int flag[3] = { POLLRDNORM | POLLHUP | POLLERR,
			       POLLWRNORM | POLLHUP | POLLERR,
			       POLLRDBAND };
	proc_t *p = l->l_proc;
	struct filedesc	*fdp;
	int msk, i, j, fd, n;
	fd_mask ibits, obits;
	struct file *fp;

	fdp = p->p_fd;
	n = 0;
	for (msk = 0; msk < 3; msk++) {
		for (i = 0; i < nfd; i += NFDBITS) {
			ibits = *ibitp++;
			obits = 0;
			while ((j = ffs(ibits)) && (fd = i + --j) < nfd) {
				ibits &= ~(1 << j);
				if ((fp = fd_getfile(fdp, fd)) == NULL)
					return (EBADF);
				FILE_USE(fp);
				if ((*fp->f_ops->fo_poll)(fp, flag[msk], l)) {
					obits |= (1 << j);
					n++;
				}
				FILE_UNUSE(fp, l);
			}
			*obitp++ = obits;
		}
	}
	*retval = n;
	return (0);
}

/*
 * Poll system call.
 */
int
sys_poll(lwp_t *l, void *v, register_t *retval)
{
	struct sys_poll_args /* {
		syscallarg(struct pollfd *)	fds;
		syscallarg(u_int)		nfds;
		syscallarg(int)			timeout;
	} */ * const uap = v;
	struct timeval	atv, *tv = NULL;

	if (SCARG(uap, timeout) != INFTIM) {
		atv.tv_sec = SCARG(uap, timeout) / 1000;
		atv.tv_usec = (SCARG(uap, timeout) % 1000) * 1000;
		tv = &atv;
	}

	return pollcommon(l, retval, SCARG(uap, fds), SCARG(uap, nfds),
		tv, NULL);
}

/*
 * Poll system call.
 */
int
sys_pollts(lwp_t *l, void *v, register_t *retval)
{
	struct sys_pollts_args /* {
		syscallarg(struct pollfd *)		fds;
		syscallarg(u_int)			nfds;
		syscallarg(const struct timespec *)	ts;
		syscallarg(const sigset_t *)		mask;
	} */ * const uap = v;
	struct timespec	ats;
	struct timeval	atv, *tv = NULL;
	sigset_t	amask, *mask = NULL;
	int		error;

	if (SCARG(uap, ts)) {
		error = copyin(SCARG(uap, ts), &ats, sizeof(ats));
		if (error)
			return error;
		atv.tv_sec = ats.tv_sec;
		atv.tv_usec = ats.tv_nsec / 1000;
		tv = &atv;
	}
	if (SCARG(uap, mask)) {
		error = copyin(SCARG(uap, mask), &amask, sizeof(amask));
		if (error)
			return error;
		mask = &amask;
	}

	return pollcommon(l, retval, SCARG(uap, fds), SCARG(uap, nfds),
		tv, mask);
}

int
pollcommon(lwp_t *l, register_t *retval,
	struct pollfd *u_fds, u_int nfds,
	struct timeval *tv, sigset_t *mask)
{
	char		smallbits[32 * sizeof(struct pollfd)];
	proc_t		* const p = l->l_proc;
	void *		bits;
	sigset_t	oldmask;
	int		ncoll, error, timo;
	size_t		ni;
	struct timeval	sleeptv;

	if (nfds > p->p_fd->fd_nfiles) {
		/* forgiving; slightly wrong */
		nfds = p->p_fd->fd_nfiles;
	}
	ni = nfds * sizeof(struct pollfd);
	if (ni > sizeof(smallbits))
		bits = kmem_alloc(ni, KM_SLEEP);
	else
		bits = smallbits;

	error = copyin(u_fds, bits, ni);
	if (error)
		goto done;

	timo = 0;
	if (tv && inittimeleft(tv, &sleeptv) == -1) {
		error = EINVAL;
		goto done;
	}

	if (mask) {
		sigminusset(&sigcantmask, mask);
		mutex_enter(&p->p_smutex);
		oldmask = l->l_sigmask;
		l->l_sigmask = *mask;
		mutex_exit(&p->p_smutex);
	} else
		oldmask = l->l_sigmask;	/* XXXgcc */

	mutex_enter(&select_lock);
	SLIST_INIT(&l->l_selwait);
	for (;;) {
		ncoll = nselcoll;
		l->l_selflag = SEL_SCANNING;
		mutex_exit(&select_lock);

		error = pollscan(l, (struct pollfd *)bits, nfds, retval);

		mutex_enter(&select_lock);
		if (error || *retval)
			break;
		if (tv && (timo = gettimeleft(tv, &sleeptv)) <= 0)
			break;
		if (l->l_selflag != SEL_SCANNING || nselcoll != ncoll)
			continue;
		l->l_selflag = SEL_BLOCKING;
		error = cv_timedwait_sig(&select_cv, &select_lock, timo);
		if (error != 0)
			break;
	}
	selclear();
	mutex_exit(&select_lock);

	if (mask) {
		mutex_enter(&p->p_smutex);
		l->l_sigmask = oldmask;
		mutex_exit(&p->p_smutex);
	}
 done:
	/* poll is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	if (error == 0)
		error = copyout(bits, u_fds, ni);
	if (bits != smallbits)
		kmem_free(bits, ni);
	return (error);
}

int
pollscan(lwp_t *l, struct pollfd *fds, int nfd, register_t *retval)
{
	proc_t		*p = l->l_proc;
	struct filedesc	*fdp;
	int		i, n;
	struct file	*fp;

	fdp = p->p_fd;
	n = 0;
	for (i = 0; i < nfd; i++, fds++) {
		if (fds->fd >= fdp->fd_nfiles) {
			fds->revents = POLLNVAL;
			n++;
		} else if (fds->fd < 0) {
			fds->revents = 0;
		} else {
			if ((fp = fd_getfile(fdp, fds->fd)) == NULL) {
				fds->revents = POLLNVAL;
				n++;
			} else {
				FILE_USE(fp);
				fds->revents = (*fp->f_ops->fo_poll)(fp,
				    fds->events | POLLERR | POLLHUP, l);
				if (fds->revents != 0)
					n++;
				FILE_UNUSE(fp, l);
			}
		}
	}
	*retval = n;
	return (0);
}

/*ARGSUSED*/
int
seltrue(dev_t dev, int events, lwp_t *l)
{

	return (events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Record a select request.
 */
void
selrecord(lwp_t *selector, struct selinfo *sip)
{

	mutex_enter(&select_lock);
	if (sip->sel_lwp == NULL) {
		/* First named waiter, although there may be more. */
		sip->sel_lwp = selector;
		SLIST_INSERT_HEAD(&selector->l_selwait, sip, sel_chain);
	} else if (sip->sel_lwp != selector) {
		/* Multiple waiters. */
		sip->sel_collision = true;
	}
	mutex_exit(&select_lock);
}

/*
 * Do a wakeup when a selectable event occurs.
 */
void
selwakeup(struct selinfo *sip)
{
	lwp_t *l;

	mutex_enter(&select_lock);
	if (sip->sel_collision) {
		/* Multiple waiters - just notify everybody. */
		nselcoll++;
		sip->sel_collision = false;
		cv_broadcast(&select_cv);
	} else if (sip->sel_lwp != NULL) {
		/* Only one LWP waiting. */
		l = sip->sel_lwp;
		if (l->l_selflag == SEL_BLOCKING) {
			/*
			 * If it's sleeping, wake it up.  If not, it's
			 * already awake but hasn't yet removed itself
			 * from the selector.  We reset the state below
			 * so that we only attempt to do this once.
			 */
			lwp_lock(l);
			if (l->l_wchan == &select_cv) {
				/* lwp_unsleep() releases the LWP lock. */
				lwp_unsleep(l);
			} else
				lwp_unlock(l);
		} else {
			/*
			 * Not yet asleep.  Reset its state below so that
			 * it will go around again.
			 */
		}
		l->l_selflag = SEL_RESET;
	}
	mutex_exit(&select_lock);
}

void
selnotify(struct selinfo *sip, long knhint)
{

	selwakeup(sip);
	KNOTE(&sip->sel_klist, knhint);
}

/*
 * Remove an LWP from all objects that it is waiting for.
 */
static void
selclear(void)
{
	struct selinfo *sip;
	lwp_t *l = curlwp;

	KASSERT(mutex_owned(&select_lock));

	SLIST_FOREACH(sip, &l->l_selwait, sel_chain) {
		KASSERT(sip->sel_lwp == l);
		sip->sel_lwp = NULL;
	}
}

/*
 * Initialize the select/poll system calls.
 */
void
selsysinit(void)
{

	mutex_init(&select_lock, MUTEX_DRIVER, IPL_VM);
	cv_init(&select_cv, "select");
}
