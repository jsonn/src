/*	$NetBSD: xencons.c,v 1.31.2.4 2009/07/24 11:30:28 jym Exp $	*/

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
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
 *
 */

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian Limpach.
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


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xencons.c,v 1.31.2.4 2009/07/24 11:30:28 jym Exp $");

#include "opt_xen.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kernel.h>

#include <machine/stdarg.h>
#include <xen/xen.h>
#include <xen/hypervisor.h>
#include <xen/evtchn.h>
#ifdef XEN3
#include <uvm/uvm.h>
#include <machine/pmap.h>
#include <xen/xen3-public/io/console.h>
#else
#include <xen/ctrl_if.h>
#endif

#include <dev/cons.h>

#ifdef DDB
#include <ddb/db_output.h>	/* XXX for db_max_line */
#endif

#undef XENDEBUG
 
#ifdef XENDEBUG
#define XENPRINTK(x) printk x
#else 
#define XENPRINTK(x)
#endif

static int xencons_isconsole = 0;
static struct xencons_softc *xencons_console_device = NULL;

#define	XENCONS_UNIT(x)	(minor(x))
#define XENCONS_BURST 128

int xencons_match(device_t, cfdata_t, void *);
void xencons_attach(device_t, device_t, void *);
int xencons_intr(void *);
void xencons_tty_input(struct xencons_softc *, char*, int);


struct xencons_softc {
	device_t sc_dev;
	struct	tty *sc_tty;
	int polling;
#ifndef XEN3
	/* circular buffer when polling */
	char buf[XENCONS_BURST];
	volatile int buf_write;
	volatile int buf_read;
#endif
};
#ifdef XEN3
volatile struct xencons_interface *xencons_interface;
#endif

CFATTACH_DECL_NEW(xencons, sizeof(struct xencons_softc),
    xencons_match, xencons_attach, NULL, NULL);

extern struct cfdriver xencons_cd;

dev_type_open(xencons_open);
dev_type_close(xencons_close);
dev_type_read(xencons_read);
dev_type_write(xencons_write);
dev_type_ioctl(xencons_ioctl);
dev_type_stop(xencons_stop);
dev_type_tty(xencons_tty);
dev_type_poll(xencons_poll);

const struct cdevsw xencons_cdevsw = {
	xencons_open, xencons_close, xencons_read, xencons_write,
	xencons_ioctl, xencons_stop, xencons_tty, xencons_poll,
	NULL, ttykqfilter, D_TTY
};


#ifdef XEN3
static int xencons_handler(void *);
/* power management, for save/restore */
static bool xencons_suspend(device_t PMF_FN_PROTO);
static bool xencons_resume(device_t PMF_FN_PROTO);
#else
static void xencons_rx(ctrl_msg_t *, unsigned long);
#endif
int xenconscn_getc(dev_t);
void xenconscn_putc(dev_t, int);
void xenconscn_pollc(dev_t, int);

static struct consdev xencons = {
	NULL, NULL, xenconscn_getc, xenconscn_putc, xenconscn_pollc,
	NULL, NULL, NULL, NODEV, CN_NORMAL
};

static struct cnm_state xencons_cnm_state;

void	xencons_start (struct tty *);
int	xencons_param (struct tty *, struct termios *);

int
xencons_match(device_t parent, cfdata_t match, void *aux)
{
	struct xencons_attach_args *xa = (struct xencons_attach_args *)aux;

	if (strcmp(xa->xa_device, "xencons") == 0)
		return 1;
	return 0;
}

void
xencons_attach(device_t parent, device_t self, void *aux)
{
	struct xencons_softc *sc = device_private(self);

	aprint_normal(": Xen Virtual Console Driver\n");

	sc->sc_dev = self;
	sc->sc_tty = ttymalloc();
	tty_attach(sc->sc_tty);
	sc->sc_tty->t_oproc = xencons_start;
	sc->sc_tty->t_param = xencons_param;

	if (xencons_isconsole) {
		int maj;

		/* Locate the major number. */
		maj = cdevsw_lookup_major(&xencons_cdevsw);

		/* There can be only one, but it can have any unit number. */
		cn_tab->cn_dev = makedev(maj, device_unit(self));

		aprint_verbose_dev(self, "console major %d, unit %d\n",
		    maj, device_unit(self));

		sc->sc_tty->t_dev = cn_tab->cn_dev;

#ifdef DDB
		/* Set db_max_line to avoid paging. */
		db_max_line = 0x7fffffff;
#endif
		xencons_console_device = sc;

		xencons_resume(self, PMF_F_NONE);
	}
	sc->polling = 0;

	if (!pmf_device_register(self, xencons_suspend, xencons_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static bool
xencons_suspend(device_t dev PMF_FN_ARGS) {

	int evtch;

	/* dom0 console should not be suspended */
	if (!xendomain_is_dom0()) {
#ifdef XEN3
		evtch = xen_start_info.console_evtchn;
		hypervisor_mask_event(evtch);
		if (event_remove_handler(evtch, xencons_handler,
		    xencons_console_device) != 0) {
			aprint_error_dev(dev,
			    "can't remove handler: xencons_handler\n");
		}
#endif

		aprint_verbose_dev(dev, "removed event channel %d\n", evtch);
	}


	return true;
}

static bool
xencons_resume(device_t dev PMF_FN_ARGS) {

	int evtch = -1;

	if (xendomain_is_dom0()) {
	/* dom0 console resume is required only during first start-up */
		if (cold) {
			evtch = bind_virq_to_evtch(VIRQ_CONSOLE);
			event_set_handler(evtch, xencons_intr,
			    xencons_console_device, IPL_TTY, "xencons");
		}
	} else {
#ifdef XEN3
		evtch = xen_start_info.console_evtchn;
		event_set_handler(evtch, xencons_handler,
		    xencons_console_device, IPL_TTY, "xencons");

#else
		(void)ctrl_if_register_receiver(CMSG_CONSOLE,
		    xencons_rx, 0);
#endif
	}

	if (evtch != -1) {
		aprint_verbose_dev(dev, "using event channel %d\n", evtch);
		hypervisor_enable_event(evtch);
	}

	return true;
}

int
xencons_open(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct xencons_softc *sc;
	struct tty *tp;

	sc = device_lookup_private(&xencons_cd, XENCONS_UNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	tp = sc->sc_tty;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);

	if ((tp->t_state & TS_ISOPEN) == 0 && tp->t_wopen == 0) {
		tp->t_dev = dev;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		xencons_param(tp, &tp->t_termios);
		ttsetwater(tp);
	}
	tp->t_state |= TS_CARR_ON;

	return ((*tp->t_linesw->l_open)(dev, tp));
}

int
xencons_close(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;

	if (tp == NULL)
		return (0);
	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);
#ifdef notyet /* XXX */
	ttyfree(tp);
#endif
	return (0);
}

int
xencons_read(dev_t dev, struct uio *uio, int flag)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
xencons_write(dev_t dev, struct uio *uio, int flag)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
xencons_poll(dev_t dev, int events, struct lwp *l)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;
 
	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

struct tty *
xencons_tty(dev_t dev)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;

	return (tp);
}

int
xencons_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct xencons_softc *sc = device_lookup_private(&xencons_cd,
	    XENCONS_UNIT(dev));
	struct tty *tp = sc->sc_tty;
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	switch (cmd) {
	default:
		return (EPASSTHROUGH);
	}

#ifdef DIAGNOSTIC
	panic("xencons_ioctl: impossible");
#endif
}

void
xencons_start(struct tty *tp)
{
	struct clist *cl;
	int s;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;
	tp->t_state |= TS_BUSY;
	splx(s);

	/*
	 * We need to do this outside spl since it could be fairly
	 * expensive and we don't want our serial ports to overflow.
	 */
	cl = &tp->t_outq;
	if (xendomain_is_dom0()) {
		int len, r;
		u_char buf[XENCONS_BURST+1];

		len = q_to_b(cl, buf, XENCONS_BURST);
		while (len > 0) {
			r = HYPERVISOR_console_io(CONSOLEIO_write, len, buf);
			if (r <= 0)
				break;
			len -= r;
		}
	} else {
#ifdef XEN3
		XENCONS_RING_IDX cons, prod, len;

#define XNC_OUT (xencons_interface->out)
		cons = xencons_interface->out_cons;
		prod = xencons_interface->out_prod;
		xen_rmb();
		while (prod != cons + sizeof(xencons_interface->out)) {
			if (MASK_XENCONS_IDX(prod, XNC_OUT) <
			    MASK_XENCONS_IDX(cons, XNC_OUT)) {
				len = MASK_XENCONS_IDX(cons, XNC_OUT) -
				    MASK_XENCONS_IDX(prod, XNC_OUT);
			} else {
				len = sizeof(XNC_OUT) -
				    MASK_XENCONS_IDX(prod, XNC_OUT);
			}
			len = q_to_b(cl, __UNVOLATILE(
			    &XNC_OUT[MASK_XENCONS_IDX(prod, XNC_OUT)]), len);
			if (len == 0)
				break;
			prod = prod + len;
		}
		xen_wmb();
		xencons_interface->out_prod = prod;
		xen_wmb();
		hypervisor_notify_via_evtchn(xen_start_info.console.domU.evtchn);
#undef XNC_OUT
#else /* XEN3 */
		ctrl_msg_t msg;
		int len;

		len = q_to_b(cl, msg.msg, sizeof(msg.msg));
		msg.type = CMSG_CONSOLE;
		msg.subtype = CMSG_CONSOLE_DATA;
		msg.length = len;
		while (ctrl_if_send_message_noblock(&msg, NULL, 0) == EAGAIN) {
			HYPERVISOR_yield();
			/* XXX check return value and queue wait for space
			 * thread/softint */
		}
#endif /* XEN3 */
	}

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (ttypull(tp)) {
		tp->t_state |= TS_TIMEOUT;
		callout_schedule(&tp->t_rstrt_ch, 1);
	}
out:
	splx(s);
}

void
xencons_stop(struct tty *tp, int flag)
{

}


#ifdef XEN3
/* Non-privileged console interrupt routine */
static int 
xencons_handler(void *arg)
{
	struct xencons_softc *sc = arg;
	XENCONS_RING_IDX cons, prod, len;
	int s = spltty();

	if (sc->polling) {
		splx(s);
		return 1;
	}
		

#define XNC_IN (xencons_interface->in)

	cons = xencons_interface->in_cons;
	prod = xencons_interface->in_prod;
	xen_rmb();
	while (cons != prod) {
		if (MASK_XENCONS_IDX(cons, XNC_IN) <
		    MASK_XENCONS_IDX(prod, XNC_IN))
			len = MASK_XENCONS_IDX(prod, XNC_IN) -
			    MASK_XENCONS_IDX(cons, XNC_IN);
		else
			len = sizeof(XNC_IN) - MASK_XENCONS_IDX(cons, XNC_IN);

		xencons_tty_input(sc, __UNVOLATILE(
		    &XNC_IN[MASK_XENCONS_IDX(cons, XNC_IN)]), len);
		if (__predict_false(xencons_interface->in_cons != cons)) {
			/* catch up with xenconscn_getc() */
			cons = xencons_interface->in_cons;
			prod = xencons_interface->in_prod;
			xen_rmb();
		} else {
			cons += len;
			xen_wmb();
			xencons_interface->in_cons = cons;
			xen_wmb();
		}
	}
	hypervisor_notify_via_evtchn(xen_start_info.console.domU.evtchn);
	splx(s);
	return 1;
#undef XNC_IN
}

#else
/* Non-privileged receive callback. */
static void
xencons_rx(ctrl_msg_t *msg, unsigned long id)
{
	int i;
	int s;
	// unsigned long flags;
	struct xencons_softc *sc;

	sc = device_lookup_private(&xencons_cd, XENCONS_UNIT(cn_tab->cn_dev));
	if (sc == NULL)
		goto out2;

	s = spltty();
	if (sc->polling) {
		for (i = 0; i < msg->length; i++) {
			cn_check_magic(sc->sc_tty->t_dev, msg->msg[i],
			    xencons_cnm_state);
			sc->buf[sc->buf_write] = msg->msg[i];
			sc->buf_write++;
			if (sc->buf_write == XENCONS_BURST)
				sc->buf_write = 0;
			if (sc->buf_write == sc->buf_read) {
				/*
				 * we overflowed the circular buffer
				 * advance the read pointer, meaning
				 * we loose one char at the beginning
				 * of the buf
				 */
				sc->buf_read++;
				if (sc->buf_read == XENCONS_BURST)
					sc->buf_read = 0;
			}
		}
		goto out;
	}

	xencons_tty_input(sc, msg->msg, msg->length);
 out:
	splx(s);
 out2:
	msg->length = 0;
	ctrl_if_send_response(msg);
}
#endif /* !XEN3 */

void
xencons_tty_input(struct xencons_softc *sc, char* buf, int len)
{
	struct tty *tp;
	int i;

	tp = sc->sc_tty;
	if (tp == NULL)
		return;

	for (i = 0; i < len; i++) {
		cn_check_magic(sc->sc_tty->t_dev, buf[i], xencons_cnm_state);
		(*tp->t_linesw->l_rint)(buf[i], tp);
	}
}

/* privileged receive callback */
int
xencons_intr(void *p)
{
	static char rbuf[16];
	int len;
	struct xencons_softc *sc = p;

	if (sc == NULL)
		/* Interrupt may happen during resume */
		return 1;

	if (sc->polling)
		return 1;

	while ((len =
	    HYPERVISOR_console_io(CONSOLEIO_read, sizeof(rbuf), rbuf)) > 0) {
		xencons_tty_input(sc, rbuf, len);
	}
	return 1;
}

void
xenconscn_attach(void)
{

	cn_tab = &xencons;

#ifdef XEN3
	/* console ring mapped in locore.S */
#else /* XEN3 */
	ctrl_if_early_init();
#endif /* XEN3 */

	cn_init_magic(&xencons_cnm_state);
	cn_set_magic("+++++");

	xencons_isconsole = 1;
}

int
xenconscn_getc(dev_t dev)
{
	char c;
	int s = spltty();
#ifdef XEN3
	XENCONS_RING_IDX cons, prod;
#else
	int ret;
#endif

	if (xencons_console_device && xencons_console_device->polling == 0) {
		printf("xenconscn_getc() but not polling\n");
		splx(s);
		return 0;
	}
	if (xendomain_is_dom0()) {
		while (HYPERVISOR_console_io(CONSOLEIO_read, 1, &c) == 0)
			;
		cn_check_magic(dev, c, xencons_cnm_state);
		splx(s);
		return c;
	}
	if (xencons_console_device == NULL) {
		printf("xenconscn_getc(): not console\n");
		while (1)
			;  /* loop here instead of in ddb */
		splx(s);
		return 0;
	}

	if (xencons_console_device->polling == 0) {
		printf("xenconscn_getc() but not polling\n");
		splx(s);
		return 0;
	}

#ifdef XEN3
	cons = xencons_interface->in_cons;
	prod = xencons_interface->in_prod;
	xen_rmb();
	while (cons == prod) {
		HYPERVISOR_yield();
		prod = xencons_interface->in_prod;
	}
	xen_rmb();
	c = xencons_interface->in[MASK_XENCONS_IDX(xencons_interface->in_cons,
	    xencons_interface->in)];
	xen_rmb();
	xencons_interface->in_cons = cons + 1;
	cn_check_magic(dev, c, xencons_cnm_state);
	splx(s);
	return c;
#else /* XEN3 */
	while (xencons_console_device->buf_write ==
	    xencons_console_device->buf_read) {
		ctrl_if_console_poll();
	}

	ret = xencons_console_device->buf[xencons_console_device->buf_read];
	xencons_console_device->buf_read++;
	if (xencons_console_device->buf_read == XENCONS_BURST)
		xencons_console_device->buf_read = 0;
	splx(s);
	return ret;
#endif /* XEN3 */
}

void
xenconscn_putc(dev_t dev, int c)
{
	int s = spltty();
#ifdef XEN3
	XENCONS_RING_IDX cons, prod;
	if (xendomain_is_dom0()) {
#else
	extern int ctrl_if_evtchn;
	if (xendomain_is_dom0() || ctrl_if_evtchn == -1) {
#endif
		u_char buf[1];

		buf[0] = c;
		(void)HYPERVISOR_console_io(CONSOLEIO_write, 1, buf);
	} else {
		XENPRINTK(("xenconscn_putc(%c)\n", c));
#ifdef XEN3
		cons = xencons_interface->out_cons;
		prod = xencons_interface->out_prod;
		xen_rmb();
		while (prod == cons + sizeof(xencons_interface->out)) {
			cons = xencons_interface->out_cons;
			prod = xencons_interface->out_prod;
			xen_rmb();
		}
		xencons_interface->out[MASK_XENCONS_IDX(xencons_interface->out_prod,
		    xencons_interface->out)] = c;
		xen_rmb();
		xencons_interface->out_prod++;
		xen_rmb();
		hypervisor_notify_via_evtchn(xen_start_info.console.domU.evtchn);
#else
		ctrl_msg_t msg;

		msg.type = CMSG_CONSOLE;
		msg.subtype = CMSG_CONSOLE_DATA;
		msg.length = 1;
		msg.msg[0] = c;
		while (ctrl_if_send_message_noblock(&msg, NULL, 0) == EAGAIN) {
			ctrl_if_console_poll();
		}
#endif /* !XEN3 */
		splx(s);
	}
}

void
xenconscn_pollc(dev_t dev, int on)
{
	if (xencons_console_device)
		xencons_console_device->polling = on;
#ifndef XEN3
	if (on) {
		xencons_console_device->buf_write = 0;
		xencons_console_device->buf_read = 0;
	}
#endif
}

/*
 * Set line parameters.
 */
int
xencons_param(struct tty *tp, struct termios *t)
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return (0);
}
