/*	$NetBSD: irix_dirent.c,v 1.23.10.1 2010/03/17 02:59:52 snj Exp $ */

/*-
 * Copyright (c) 1994, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Emmanuel Dreyfus.
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
__KERNEL_RCSID(0, "$NetBSD: irix_dirent.c,v 1.23.10.1 2010/03/17 02:59:52 snj Exp $");

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <compat/common/compat_util.h>

#include <compat/irix/irix_types.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_syscall.h>
#include <compat/irix/irix_syscallargs.h>

/*
 * irix_sys_ngetdents() is nearly a plain copy of svr4_sys_getdents(), from
 * sys/compat/svr4/svr4_misc.c. We need a customized version to handle the
 * eof flag.
 * Obviously the code should be merged, but it would require some
 * change to the way COMPAT_SVR4 code is set up.
 */
#define SVR4_RECLEN(de,namlen) ALIGN((SVR4_NAMEOFF(de) + (namlen) + 1))
#define SVR4_NAMEOFF(dp)       ((char *)&(dp)->d_name - (char *)dp)

int
irix_sys_ngetdents(struct lwp *l, const struct irix_sys_ngetdents_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fildes;
		syscallarg(irix_dirent_t *) buf;
		syscallarg(unsigned short) nbyte;
		syscallarg(int *) eof;
	} */
	struct dirent *bdp;
	struct vnode *vp;
	char *inp, *buf;	/* BSD-format */
	int len, reclen;	/* BSD-format */
	char *outp;		/* SVR4-format */
	int resid, svr4_reclen;	/* SVR4-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct irix_dirent idb;
	off_t off;		/* true file offset */
	int buflen, error, eofflag;
	off_t *cookiebuf = NULL, *cookie;
	int ncookies, fd;

	fd = SCARG(uap, fildes);
	if ((error = fd_getvnode(fd, &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out1;
	}

	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto out1;
	}

	buflen = min(MAXBSIZE, SCARG(uap, nbyte));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	UIO_SETUP_SYSSPACE(&auio);
	/*
         * First we read into the malloc'ed buffer, then
         * we massage it into user space, one record at a time.
         */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &cookiebuf,
	    &ncookies);
	if (error)
		goto out;

	inp = buf;
	outp = (char *)SCARG(uap, buf);
	resid = SCARG(uap, nbyte);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("irix_getdents: bad reclen");
		if (cookie)
			off = *cookie++; /* each entry points to the next */
		else
			off += reclen;
		if ((off >> 32) != 0) {
			compat_offseterr(vp, "irix_getdents");
			error = EINVAL;
			goto out;
		}
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			continue;
		}
		svr4_reclen = SVR4_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < svr4_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a SVR4-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = (irix_ino_t)bdp->d_fileno;
		idb.d_off = (irix_off_t)off;
		idb.d_reclen = (u_short)svr4_reclen;
		strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
		if ((error = copyout((void *)&idb, outp, svr4_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past SVR4-shaped entry */
		outp += svr4_reclen;
		resid -= svr4_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == (char *)SCARG(uap, buf)) {
		if (cookiebuf)
			free(cookiebuf, M_TEMP);
		cookiebuf = NULL;
		goto again;
	}
	fp->f_offset = off;	/* update the vnode offset */

eof:
	*retval = SCARG(uap, nbyte) - resid;
out:
	VOP_UNLOCK(vp, 0);
	if (cookiebuf)
		free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
out1:
	fd_putfile(fd);
	if (SCARG(uap, eof) != NULL)
		error = copyout(&eofflag, SCARG(uap, eof), sizeof(int));
	return error;
}

int
irix_sys_getdents(struct lwp *l, const struct irix_sys_getdents_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fildes;
		syscallarg(irix_dirent_t *) buf;
		syscallarg(unsigned short) nbyte;
		syscallarg(int *) eof;
	} */
	struct irix_sys_ngetdents_args cup;

	SCARG(&cup, fildes) = SCARG(uap, fildes);
	SCARG(&cup, buf) = SCARG(uap, buf);
	SCARG(&cup, nbyte) = SCARG(uap, nbytes);
	SCARG(&cup, eof) = NULL;

	return irix_sys_ngetdents(l, (void *)&cup, retval);
}


/*
 * The 64 versions are very close to the
 * 32 bit versions (only 3 lines of diff)
 */
int
irix_sys_ngetdents64(struct lwp *l, const struct irix_sys_ngetdents64_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fildes;
		syscallarg(irix_dirent64_t *) buf;
		syscallarg(unsigned short) nbyte;
		syscallarg(int *) eof;
	} */
	struct dirent *bdp;
	struct vnode *vp;
	char *inp, *buf;	/* BSD-format */
	int len, reclen;	/* BSD-format */
	char *outp;		/* SVR4-format */
	int resid, svr4_reclen;	/* SVR4-format */
	file_t *fp;
	struct uio auio;
	struct iovec aiov;
	struct irix_dirent64 idb;
	off_t off;		/* true file offset */
	int buflen, error, eofflag;
	off_t *cookiebuf = NULL, *cookie;
	int ncookies, fd;

	fd = SCARG(uap, fildes);
	if ((error = fd_getvnode(fd, &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out1;
	}

	vp = fp->f_data;
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto out1;
	}

	buflen = min(MAXBSIZE, SCARG(uap, nbyte));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	UIO_SETUP_SYSSPACE(&auio);
	/*
         * First we read into the malloc'ed buffer, then
         * we massage it into user space, one record at a time.
         */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &cookiebuf,
	    &ncookies);
	if (error)
		goto out;

	inp = buf;
	outp = (char *)SCARG(uap, buf);
	resid = SCARG(uap, nbyte);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("irix_getdents64: bad reclen");
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			if (cookie)
				off = *cookie++;
			else
				off += reclen;
			continue;
		}
		svr4_reclen = SVR4_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < svr4_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		if (cookie)
			off = *cookie++; /* each entry points to the next */
		else
			off += reclen;
		/*
		 * Massage in place to make a SVR4-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = (irix_ino64_t)bdp->d_fileno;
		idb.d_off = (irix_off64_t)off;
		idb.d_reclen = (u_short)svr4_reclen;
		strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
		if ((error = copyout((void *)&idb, outp, svr4_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past SVR4-shaped entry */
		outp += svr4_reclen;
		resid -= svr4_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == (char *)SCARG(uap, buf)) {
		if (cookiebuf)
			free(cookiebuf, M_TEMP);
		cookiebuf = NULL;
		goto again;
	}
	fp->f_offset = off;	/* update the vnode offset */

eof:
	*retval = SCARG(uap, nbyte) - resid;
out:
	VOP_UNLOCK(vp, 0);
	if (cookiebuf)
		free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
out1:
	fd_putfile(fd);
	if (SCARG(uap, eof) != NULL)
		error = copyout(&eofflag, SCARG(uap, eof), sizeof(int));
	return error;
}

int
irix_sys_getdents64(struct lwp *l, const struct irix_sys_getdents64_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fildes;
		syscallarg(irix_dirent64_t *) buf;
		syscallarg(unsigned short) nbyte;
		syscallarg(int *) eof;
	} */
	struct irix_sys_ngetdents64_args cup;

	SCARG(&cup, fildes) = SCARG(uap, fildes);
	SCARG(&cup, buf) = SCARG(uap, buf);
	SCARG(&cup, nbyte) = SCARG(uap, nbytes);
	SCARG(&cup, eof) = NULL;

	return irix_sys_ngetdents64(l, (void *)&cup, retval);
}
