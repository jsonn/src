/*	$NetBSD: arcbios_tty.c,v 1.1.4.2 2001/10/10 11:56:52 fvdl Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/uio.h> 
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/vnode.h>

#include <dev/cons.h>

#include <dev/arcbios/arcbios.h>
#include <dev/arcbios/arcbiosvar.h>

struct callout arcbios_tty_ch = CALLOUT_INITIALIZER;

static struct tty *arcbios_tty[1];

void	arcbios_tty_start(struct tty *);
void	arcbios_tty_poll(void *);
int	arcbios_tty_param(struct tty *, struct termios *);

cdev_decl(arcbios_tty);

int
arcbios_ttyopen(struct vnode *devvp, int flag, int mode, struct proc *p)
{
	dev_t dev = vdev_rdev(devvp);
	int unit = minor(dev);
	struct tty *tp;
	int s, error = 0, setuptimeout = 0;

	if (unit != 0)
		return (ENODEV);

	s = spltty();

	if (arcbios_tty[unit] == NULL) {
		tp = arcbios_tty[unit] = ttymalloc();
		tty_attach(tp);
	} else
		tp = arcbios_tty[unit];

	vdev_setprivdata(devvp, tp);

	tp->t_oproc = arcbios_tty_start;
	tp->t_param = arcbios_tty_param;
	tp->t_devvp = devvp;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_CARR_ON;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = 9600;
		ttsetwater(tp);

		setuptimeout = 1;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}

	splx(s);

	error = (*tp->t_linesw->l_open)(devvp, tp);
	if (error == 0 && setuptimeout)
		callout_reset(&arcbios_tty_ch, 1, arcbios_tty_poll, tp);

	return (error);
}
 
int
arcbios_ttyclose(struct vnode *devvp, int flag, int mode, struct proc *p)
{
	struct tty *tp = vdev_privdata(devvp);

	callout_stop(&arcbios_tty_ch);
	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);
	return (0);
}
 
int
arcbios_ttyread(struct vnode *devvp, struct uio *uio, int flag)
{
	struct tty *tp = vdev_privdata(devvp);

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}
 
int
arcbios_ttywrite(struct vnode *devvp, struct uio *uio, int flag)
{
	struct tty *tp = vdev_privdata(devvp);
 
	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
arcbios_ttypoll(struct vnode *devvp, int events, struct proc *p)
{
	struct tty *tp = vdev_privdata(devvp);
 
	return ((*tp->t_linesw->l_poll)(tp, events, p));
}
 
int
arcbios_ttyioctl(struct vnode *devvp, u_long cmd, caddr_t data, int flag,
		 struct proc *p)
{
	struct tty *tp = vdev_privdata(devvp);
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	return (ENOTTY);
}

int
arcbios_tty_param(struct tty *tp, struct termios *t)
{

	return (0);
}

void
arcbios_tty_start(struct tty *tp)
{
	uint32_t count;
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {
		(*ARCBIOS->Write)(ARCBIOS_STDOUT, tp->t_outq.c_cf,
		    ndqb(&tp->t_outq, 0), &count);
		ndflush(&tp->t_outq, count);
	}
	tp->t_state &= ~TS_BUSY;
 out:
	splx(s);
}

void
arcbios_ttystop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

static int
arcbios_tty_getchar(int *cp)
{
	char c;
	int32_t q;
	u_int32_t count;

	q = ARCBIOS->GetReadStatus(ARCBIOS_STDIN);

	if (q == 0) {
		ARCBIOS->Read(ARCBIOS_STDIN, &c, 1, &count);
		*cp = c;	

		return 1;
	}

	return 0;
}

void
arcbios_tty_poll(void *v)
{
	struct tty *tp = v;
	int c, l_r;

	while (arcbios_tty_getchar(&c)) {
		if (tp->t_state & TS_ISOPEN)
			l_r = (*tp->t_linesw->l_rint)(c, tp);
	}
	callout_reset(&arcbios_tty_ch, 1, arcbios_tty_poll, tp);
}

struct tty *
arcbios_ttytty(struct vnode *devvp)
{
	return vdev_privdata(devvp);
}
