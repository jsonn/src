/*	$NetBSD: subr_log.c,v 1.35.2.1 2006/06/21 15:09:38 yamt Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)subr_log.c	8.3 (Berkeley) 2/14/95
 */

/*
 * Error log buffer for kernel printf's.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_log.c,v 1.35.2.1 2006/06/21 15:09:38 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/msgbuf.h>
#include <sys/file.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/poll.h>

#define LOG_RDPRI	(PZERO + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	struct	selinfo sc_selp;	/* process waiting on select call */
	pid_t	sc_pgid;		/* process/group for async I/O */
} logsoftc;

int	log_open;			/* also used in log() */
int	msgbufmapped;			/* is the message buffer mapped */
int	msgbufenabled;			/* is logging to the buffer enabled */
struct	kern_msgbuf *msgbufp;		/* the mapped buffer, itself. */

void
initmsgbuf(void *bf, size_t bufsize)
{
	struct kern_msgbuf *mbp;
	long new_bufs;

	/* Sanity-check the given size. */
	if (bufsize < sizeof(struct kern_msgbuf))
		return;

	mbp = msgbufp = (struct kern_msgbuf *)bf;

	new_bufs = bufsize - offsetof(struct kern_msgbuf, msg_bufc);
	if ((mbp->msg_magic != MSG_MAGIC) || (mbp->msg_bufs != new_bufs) ||
	    (mbp->msg_bufr < 0) || (mbp->msg_bufr >= mbp->msg_bufs) ||
	    (mbp->msg_bufx < 0) || (mbp->msg_bufx >= mbp->msg_bufs)) {
		/*
		 * If the buffer magic number is wrong, has changed
		 * size (which shouldn't happen often), or is
		 * internally inconsistent, initialize it.
		 */

		memset(bf, 0, bufsize);
		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufs = new_bufs;
	}

	/* mark it as ready for use. */
	msgbufmapped = msgbufenabled = 1;
}

/*ARGSUSED*/
static int
logopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct kern_msgbuf *mbp = msgbufp;

	if (log_open)
		return (EBUSY);
	log_open = 1;
	logsoftc.sc_pgid = l->l_proc->p_pid;	/* signal process only */
	/*
	 * The message buffer is initialized during system configuration.
	 * If it's been clobbered, note that and return an error.  (This
	 * allows a user to potentially read the buffer via /dev/kmem,
	 * and try to figure out what clobbered it.
	 */
	if (mbp->msg_magic != MSG_MAGIC) {
		msgbufenabled = 0;
		return (ENXIO);
	}

	return (0);
}

/*ARGSUSED*/
static int
logclose(dev_t dev, int flag, int mode, struct lwp *l)
{

	log_open = 0;
	logsoftc.sc_state = 0;
	return (0);
}

/*ARGSUSED*/
static int
logread(dev_t dev, struct uio *uio, int flag)
{
	struct kern_msgbuf *mbp = msgbufp;
	long l;
	int s;
	int error = 0;

	s = splhigh();
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		error = tsleep((caddr_t)mbp, LOG_RDPRI | PCATCH,
			       "klog", 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = mbp->msg_bufs - mbp->msg_bufr;
		l = min(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove((caddr_t)&mbp->msg_bufc[mbp->msg_bufr],
			(int)l, uio);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
	return (error);
}

/*ARGSUSED*/
static int
logpoll(dev_t dev, int events, struct lwp *l)
{
	int revents = 0;
	int s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (msgbufp->msg_bufr != msgbufp->msg_bufx)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(l, &logsoftc.sc_selp);
	}

	splx(s);
	return (revents);
}

static void
filt_logrdetach(struct knote *kn)
{
	int s;

	s = splhigh();
	SLIST_REMOVE(&logsoftc.sc_selp.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_logread(struct knote *kn, long hint)
{

	if (msgbufp->msg_bufr == msgbufp->msg_bufx)
		return (0);

	if (msgbufp->msg_bufr < msgbufp->msg_bufx)
		kn->kn_data = msgbufp->msg_bufx - msgbufp->msg_bufr;
	else
		kn->kn_data = (msgbufp->msg_bufs - msgbufp->msg_bufr) +
		    msgbufp->msg_bufx;

	return (1);
}

static const struct filterops logread_filtops =
	{ 1, NULL, filt_logrdetach, filt_logread };

static int
logkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &logsoftc.sc_selp.sel_klist;
		kn->kn_fop = &logread_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = NULL;

	s = splhigh();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
logwakeup(void)
{
	if (!log_open)
		return;
	selnotify(&logsoftc.sc_selp, 0);
	if (logsoftc.sc_state & LOG_ASYNC)
		fownsignal(logsoftc.sc_pgid, SIGIO, 0, 0, NULL);
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup((caddr_t)msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

/*ARGSUSED*/
static int
logioctl(dev_t dev, u_long com, caddr_t data, int flag, struct lwp *lwp)
{
	struct proc *p = lwp->l_proc;
	long l;
	int s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		splx(s);
		if (l < 0)
			l += msgbufp->msg_bufs;
		*(int *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
	case FIOSETOWN:
		return fsetown(p, &logsoftc.sc_pgid, com, data);

	case TIOCGPGRP:
	case FIOGETOWN:
		return fgetown(p, logsoftc.sc_pgid, com, data);

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

const struct cdevsw log_cdevsw = {
	logopen, logclose, logread, nowrite, logioctl,
	    nostop, notty, logpoll, nommap, logkqfilter,
};
