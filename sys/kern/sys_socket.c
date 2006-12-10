/*	$NetBSD: sys_socket.c,v 1.47.10.2 2006/12/10 07:18:45 yamt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)sys_socket.c	8.3 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sys_socket.c,v 1.47.10.2 2006/12/10 07:18:45 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/route.h>

struct	fileops socketops = {
	soo_read, soo_write, soo_ioctl, soo_fcntl, soo_poll,
	soo_stat, soo_close, soo_kqfilter
};

/* ARGSUSED */
int
soo_read(struct file *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct socket *so = (struct socket *) fp->f_data;
	return ((*so->so_receive)(so, (struct mbuf **)0,
		uio, (struct mbuf **)0, (struct mbuf **)0, (int *)0));
}

/* ARGSUSED */
int
soo_write(struct file *fp, off_t *offset, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	struct socket *so = (struct socket *) fp->f_data;
	return (*so->so_send)(so, (struct mbuf *)0,
		uio, (struct mbuf *)0, (struct mbuf *)0, 0, curlwp);
}

int
soo_ioctl(struct file *fp, u_long cmd, void *data, struct lwp *l)
{
	struct socket *so = (struct socket *)fp->f_data;
	struct proc *p = l->l_proc;

	switch (cmd) {

	case FIONBIO:
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		return (0);

	case FIOASYNC:
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~SB_ASYNC;
			so->so_snd.sb_flags &= ~SB_ASYNC;
		}
		return (0);

	case FIONREAD:
		*(int *)data = so->so_rcv.sb_cc;
		return (0);

	case FIONWRITE:
		*(int *)data = so->so_snd.sb_cc;
		return (0);

	case FIONSPACE:
		/*
		 * See the comment around sbspace()'s definition
		 * in sys/socketvar.h in face of counts about maximum
		 * to understand the following test. We detect overflow
		 * and return zero.
		 */
		if ((so->so_snd.sb_hiwat < so->so_snd.sb_cc)
		    || (so->so_snd.sb_mbmax < so->so_snd.sb_mbcnt))
			*(int *)data = 0;
		else
			*(int *)data = sbspace(&so->so_snd);
		return (0);

	case SIOCSPGRP:
	case FIOSETOWN:
	case TIOCSPGRP:
		return fsetown(p, &so->so_pgid, cmd, data);

	case SIOCGPGRP:
	case FIOGETOWN:
	case TIOCGPGRP:
		return fgetown(p, so->so_pgid, cmd, data);

	case SIOCATMARK:
		*(int *)data = (so->so_state&SS_RCVATMARK) != 0;
		return (0);
	}
	/*
	 * Interface/routing/protocol specific ioctls:
	 * interface and routing ioctls should have a
	 * different entry since a socket's unnecessary
	 */
	if (IOCGROUP(cmd) == 'i')
		return (ifioctl(so, cmd, data, l));
	if (IOCGROUP(cmd) == 'r')
		return (rtioctl(cmd, data, l));
	return ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
	    (struct mbuf *)cmd, (struct mbuf *)data, (struct mbuf *)0, l));
}

int
soo_fcntl(struct file *fp, u_int cmd, void *data, struct lwp *l)
{
	if (cmd == F_SETFL)
		return (0);
	else
		return (EOPNOTSUPP);
}

int
soo_poll(struct file *fp, int events, struct lwp *l)
{
	struct socket *so = (struct socket *)fp->f_data;
	int revents = 0;
	int s = splsoftnet();

	if (events & (POLLIN | POLLRDNORM))
		if (soreadable(so))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (sowritable(so))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & (POLLPRI | POLLRDBAND))
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK))
			revents |= events & (POLLPRI | POLLRDBAND);

	if (revents == 0) {
		if (events & (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)) {
			selrecord(l, &so->so_rcv.sb_sel);
			so->so_rcv.sb_flags |= SB_SEL;
		}

		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(l, &so->so_snd.sb_sel);
			so->so_snd.sb_flags |= SB_SEL;
		}
	}

	splx(s);
	return (revents);
}

int
soo_stat(struct file *fp, struct stat *ub, struct lwp *l)
{
	struct socket *so = (struct socket *)fp->f_data;

	memset((caddr_t)ub, 0, sizeof(*ub));
	ub->st_mode = S_IFSOCK;
	return ((*so->so_proto->pr_usrreq)(so, PRU_SENSE,
	    (struct mbuf *)ub, (struct mbuf *)0, (struct mbuf *)0, l));
}

/* ARGSUSED */
int
soo_close(struct file *fp, struct lwp *l)
{
	int error = 0;

	if (fp->f_data)
		error = soclose((struct socket *)fp->f_data);
	fp->f_data = 0;
	return (error);
}
