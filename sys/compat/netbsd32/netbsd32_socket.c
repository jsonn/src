/*	$NetBSD: netbsd32_socket.c,v 1.11.2.4 2004/09/18 14:43:58 skrll Exp $	*/

/*
 * Copyright (c) 1998, 2001 Matthew R. Green
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
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: netbsd32_socket.c,v 1.11.2.4 2004/09/18 14:43:58 skrll Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ktrace.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#define msg __msg /* Don't ask me! */
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/ktrace.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/sa.h>
#include <sys/syscallargs.h>
#include <sys/proc.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>
#include <compat/netbsd32/netbsd32_conv.h>

/* note that the netbsd32_msghdr's iov really points to a struct iovec, not a netbsd32_iovec. */
static int recvit32 __P((struct proc *, int, struct netbsd32_msghdr *, struct iovec *, caddr_t, 
			 register_t *));

int
netbsd32_recvmsg(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct netbsd32_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(netbsd32_msghdrp_t) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct netbsd32_msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	int error;

	error = copyin((caddr_t)NETBSD32PTR64(SCARG(uap, msg)), (caddr_t)&msg,
	    sizeof(msg));
		/* netbsd32_msghdr needs the iov pre-allocated */
	if (error)
		return (error);
	if ((u_int)msg.msg_iovlen > UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen > IOV_MAX)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else if ((u_int)msg.msg_iovlen > 0)
		iov = aiov;
	else
		return (EMSGSIZE);
	msg.msg_flags = SCARG(uap, flags);
	uiov = (struct iovec *)NETBSD32PTR64(msg.msg_iov);
	error = netbsd32_to_iovecin((struct netbsd32_iovec *)uiov, 
				   iov, msg.msg_iovlen);
	if (error)
		goto done;
	if ((error = recvit32(l->l_proc, SCARG(uap, s), &msg, iov, (caddr_t)0,
	    retval)) == 0) {
		error = copyout((caddr_t)&msg,
		    (caddr_t)NETBSD32PTR64(SCARG(uap, msg)), sizeof(msg));
	}
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

int
recvit32(p, s, mp, iov, namelenp, retsize)
	struct proc *p;
	int s;
	struct netbsd32_msghdr *mp;
	struct iovec *iov;
	caddr_t namelenp;
	register_t *retsize;
{
	struct file *fp;
	struct uio auio;
	int i;
	int len, error;
	struct mbuf *from = 0, *control = 0;
	struct socket *so;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif
	
	/* getsock() will use the descriptor for us */
	if ((error = getsock(p->p_fd, s, &fp)) != 0)
		return (error);
	auio.uio_iov = iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = p;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
#if 0
		/* cannot happen iov_len is unsigned */
		if (iov->iov_len < 0) {
			error = EINVAL;
			goto out1;
		}
#endif
		/*
		 * Reads return ssize_t because -1 is returned on error.
		 * Therefore we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.
		 */
		auio.uio_resid += iov->iov_len;
		if (iov->iov_len > SSIZE_MAX || auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto out1;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof(struct iovec);

		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		memcpy((caddr_t)ktriov, (caddr_t)auio.uio_iov, iovlen);
	}
#endif
	len = auio.uio_resid;
	so = (struct socket *)fp->f_data;
	error = (*so->so_receive)(so, &from, &auio, NULL,
			  mp->msg_control ? &control : NULL, &mp->msg_flags);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, s, UIO_READ, ktriov,
			    len - auio.uio_resid, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	if (error)
		goto out;
	*retsize = len - auio.uio_resid;
	if (mp->msg_name) {
		len = mp->msg_namelen;
		if (len <= 0 || from == 0)
			len = 0;
		else {
			if (len > from->m_len)
				len = from->m_len;
			/* else if len < from->m_len ??? */
			error = copyout(mtod(from, caddr_t),
			    (caddr_t)NETBSD32PTR64(mp->msg_name),
			    (unsigned)len);
			if (error)
				goto out;
		}
		mp->msg_namelen = len;
		if (namelenp &&
		    (error = copyout((caddr_t)&len, namelenp, sizeof(int))))
			goto out;
	}
	if (mp->msg_control) {
		len = mp->msg_controllen;
		if (len <= 0 || control == 0)
			len = 0;
		else {
			struct mbuf *m = control;
			caddr_t p = (caddr_t)NETBSD32PTR64(mp->msg_control);

			do {
				i = m->m_len;
				if (len < i) {
					mp->msg_flags |= MSG_CTRUNC;
					i = len;
				}
				error = copyout(mtod(m, caddr_t), p,
				    (unsigned)i);
				if (m->m_next)
					i = ALIGN(i);
				p += i;
				len -= i;
				if (error != 0 || len <= 0)
					break;
			} while ((m = m->m_next) != NULL);
			len = p - (caddr_t)NETBSD32PTR64(mp->msg_control);
		}
		mp->msg_controllen = len;
	}
 out:
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);
 out1:
	FILE_UNUSE(fp, p);
	return (error);
}

int
netbsd32_sendmsg(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct netbsd32_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(const netbsd32_msghdrp_t) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct netbsd32_msghdr msg32;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin((caddr_t)NETBSD32PTR64(SCARG(uap, msg)), (caddr_t)&msg32,
	    sizeof(msg32));
	if (error)
		return (error);
	netbsd32_to_msghdr(&msg32, &msg);
	if ((u_int)msg.msg_iovlen > UIO_SMALLIOV) {
		if ((u_int)msg.msg_iovlen > IOV_MAX)
			return (EMSGSIZE);
		MALLOC(iov, struct iovec *,
		       sizeof(struct iovec) * (u_int)msg.msg_iovlen, M_IOV,
		       M_WAITOK);
	} else if ((u_int)msg.msg_iovlen > 0)
		iov = aiov;
	else
		return (EMSGSIZE);
	error = netbsd32_to_iovecin((struct netbsd32_iovec *)msg.msg_iov, 
				   iov, msg.msg_iovlen);
	if (error)
		goto done;
	msg.msg_iov = iov;
	/* Luckily we can use this directly */
	error = sendit(l->l_proc, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		FREE(iov, M_IOV);
	return (error);
}

int
netbsd32_recvfrom(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct netbsd32_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(netbsd32_voidp) buf;
		syscallarg(netbsd32_size_t) len;
		syscallarg(int) flags;
		syscallarg(netbsd32_sockaddrp_t) from;
		syscallarg(netbsd32_intp) fromlenaddr;
	} */ *uap = v;
	struct netbsd32_msghdr msg;
	struct iovec aiov;
	int error;

	if (SCARG(uap, fromlenaddr)) {
		error = copyin((caddr_t)NETBSD32PTR64(SCARG(uap, fromlenaddr)),
		    (caddr_t)&msg.msg_namelen, sizeof(msg.msg_namelen));
		if (error)
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = SCARG(uap, from);
	msg.msg_iov = 0; /* ignored in recvit32(), uses iov */
	msg.msg_iovlen = 1;
	aiov.iov_base = (caddr_t)NETBSD32PTR64(SCARG(uap, buf));
	aiov.iov_len = (u_long)SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
	return (recvit32(l->l_proc, SCARG(uap, s), &msg, &aiov,
	    (caddr_t)NETBSD32PTR64(SCARG(uap, fromlenaddr)), retval));
}

int
netbsd32_sendto(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct netbsd32_sendto_args /* {
		syscallarg(int) s;
		syscallarg(const netbsd32_voidp) buf;
		syscallarg(netbsd32_size_t) len;
		syscallarg(int) flags;
		syscallarg(const netbsd32_sockaddrp_t) to;
		syscallarg(int) tolen;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = (caddr_t)NETBSD32PTR64(SCARG(uap, to)); /* XXX kills const */
	msg.msg_namelen = SCARG(uap, tolen);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	aiov.iov_base = (char *)NETBSD32PTR64(SCARG(uap, buf));	/* XXX kills const */
	aiov.iov_len = SCARG(uap, len);
	return (sendit(l->l_proc, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
}
