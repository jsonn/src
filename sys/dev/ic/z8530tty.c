/*	$NetBSD: z8530tty.c,v 1.19.2.2 1997/11/14 02:14:25 mellon Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 Dual UART driver (tty interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for plain "tty" async. serial lines.
 *
 * Credits, history:
 *
 * The original version of this code was the sparc/dev/zs.c driver
 * as distributed with the Berkeley 4.4 Lite release.  Since then,
 * Gordon Ross reorganized the code into the current parent/child
 * driver scheme, separating the Sun keyboard and mouse support
 * into independent child drivers.
 *
 * RTS/CTS flow-control support was a collaboration of:
 *	Gordon Ross <gwr@netbsd.org>,
 *	Bill Studenmund <wrstuden@loki.stanford.edu>
 *	Ian Dall <Ian.Dall@dsto.defence.gov.au>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include "locators.h"

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#ifndef	ZSTTY_RING_SIZE
#define	ZSTTY_RING_SIZE	2048
#endif

/*
 * Make this an option variable one can patch.
 * But be warned:  this must be a power of 2!
 */
u_int zstty_rbuf_size = ZSTTY_RING_SIZE;

/* Stop input when 3/4 of the ring is full; restart when only 1/4 is full. */
u_int zstty_rbuf_hiwat = (ZSTTY_RING_SIZE * 1) / 4;
u_int zstty_rbuf_lowat = (ZSTTY_RING_SIZE * 3) / 4;

struct zstty_softc {
	struct	device zst_dev;		/* required first: base device */
	struct  tty *zst_tty;
	struct	zs_chanstate *zst_cs;

	u_int zst_overflows,
	      zst_floods,
	      zst_errors;

	int zst_hwflags,	/* see z8530var.h */
	    zst_swflags;	/* TIOCFLAG_SOFTCAR, ... <ttycom.h> */

	u_int zst_r_hiwat,
	      zst_r_lowat;
	u_char *volatile zst_rbget,
	       *volatile zst_rbput;
	volatile u_int zst_rbavail;
	u_char *zst_rbuf,
	       *zst_ebuf;

	/*
	 * The transmit byte count and address are used for pseudo-DMA
	 * output in the hardware interrupt code.  PDMA can be suspended
	 * to get pending changes done; heldtbc is used for this.  It can
	 * also be stopped for ^S; this sets TS_TTSTOP in tp->t_state.
	 */
	u_char *zst_tba;		/* transmit buffer address */
	u_int zst_tbc,			/* transmit byte count */
	      zst_heldtbc;		/* held tbc while xmission stopped */

	/* Flags to communicate with zstty_softint() */
	volatile u_char zst_rx_flags,	/* receiver blocked */
#define	RX_TTY_BLOCKED		0x01
#define	RX_TTY_OVERFLOWED	0x02
#define	RX_IBUF_BLOCKED		0x04
#define	RX_IBUF_OVERFLOWED	0x08
#define	RX_ANY_BLOCK		0x0f
			zst_tx_busy,	/* working on an output chunk */
			zst_tx_done,	/* done with one output chunk */
			zst_tx_stopped,	/* H/W level stop (lost CTS) */
			zst_st_check,	/* got a status interrupt */
			zst_rx_ready;
};

/* Macros to clear/set/test flags. */
#define SET(t, f)	(t) |= (f)
#define CLR(t, f)	(t) &= ~(f)
#define ISSET(t, f)	((t) & (f))

/* Definition of the driver for autoconfig. */
#ifdef	__BROKEN_INDIRECT_CONFIG
static int	zstty_match(struct device *, void *, void *);
#else
static int	zstty_match(struct device *, struct cfdata *, void *);
#endif
static void	zstty_attach(struct device *, struct device *, void *);

struct cfattach zstty_ca = {
	sizeof(struct zstty_softc), zstty_match, zstty_attach
};

struct cfdriver zstty_cd = {
	NULL, "zstty", DV_TTY
};

struct zsops zsops_tty;

/* Routines called from other code. */
cdev_decl(zs);	/* open, close, read, write, ioctl, stop, ... */

static void	zsstart __P((struct tty *));
static int	zsparam __P((struct tty *, struct termios *));
static void zs_modem __P((struct zstty_softc *zst, int onoff));
static int	zshwiflow __P((struct tty *, int));
static void zs_hwiflow __P((struct zstty_softc *));

/*
 * zstty_match: how is this zs channel configured?
 */
#ifdef	__BROKEN_INDIRECT_CONFIG
int 
zstty_match(parent, vcf, aux)
	struct device *parent;
	void   *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct zsc_attach_args *args = aux;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT)
		return 1;

	return 0;
}
#else	/* __BROKEN_INDIRECT_CONFIG */
int 
zstty_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void   *aux;
{
	struct zsc_attach_args *args = aux;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT)
		return 1;

	return 0;
}
#endif	/* __BROKEN_INDIRECT_CONFIG */

void 
zstty_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct zsc_softc *zsc = (void *) parent;
	struct zstty_softc *zst = (void *) self;
	struct cfdata *cf = self->dv_cfdata;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct tty *tp;
	int channel, s, tty_unit;
	dev_t dev;

	tty_unit = zst->zst_dev.dv_unit;
	channel = args->channel;
	cs = zsc->zsc_cs[channel];
	cs->cs_private = zst;
	cs->cs_ops = &zsops_tty;

	zst->zst_cs = cs;
	zst->zst_swflags = cf->cf_flags;	/* softcar, etc. */
	zst->zst_hwflags = args->hwflags;
	dev = makedev(zs_major, tty_unit);

	if (zst->zst_swflags)
		printf(" flags 0x%x", zst->zst_swflags);

	if (ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE))
		printf(" (console)");
	else {
#ifdef KGDB
		/*
		 * Allow kgdb to "take over" this port.  Returns true
		 * if this serial port is in-use by kgdb.
		 */
		if (zs_check_kgdb(cs, dev)) {
			printf(" (kgdb)\n");
			/*
			 * This is the kgdb port (exclusive use)
			 * so skip the normal attach code.
			 */
			return;
		}
#endif
	}
	printf("\n");

	tp = ttymalloc();
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_hwiflow = zshwiflow;
	tty_attach(tp);

	zst->zst_tty = tp;
	zst->zst_rbuf = malloc(zstty_rbuf_size << 1, M_DEVBUF, M_WAITOK);
	zst->zst_ebuf = zst->zst_rbuf + (zstty_rbuf_size << 1);
	/* Disable the high water mark. */
	zst->zst_r_hiwat = 0;
	zst->zst_r_lowat = 0;
	zst->zst_rbget = zst->zst_rbput = zst->zst_rbuf;
	zst->zst_rbavail = zstty_rbuf_size;

	/* XXX - Do we need an MD hook here? */

	/*
	 * Hardware init
	 */
	if (ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE)) {
		/* Call zsparam similar to open. */
		struct termios t;

		s = splzs();

		/* Turn on interrupts. */
		cs->cs_creg[1] = cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_SIE;
		zs_write_reg(cs, 1, cs->cs_creg[1]);

		/* Fetch the current modem control status, needed later. */
		cs->cs_rr0 = zs_read_csr(cs);

		splx(s);

		/* Setup the "new" parameters in t. */
		t.c_ispeed = 0;
		t.c_ospeed = cs->cs_defspeed;
		t.c_cflag = cs->cs_defcflag;
		/* Make sure zsparam will see changes. */
		tp->t_ospeed = 0;
		(void) zsparam(tp, &t);

		/* Make sure DTR is on now. */
		zs_modem(zst, 1);
	} else {
		/* Not the console; may need reset. */
		int reset;
		reset = (channel == 0) ?
			ZSWR9_A_RESET : ZSWR9_B_RESET;
		s = splzs();
		zs_write_reg(cs, 9, reset);
		splx(s);

		/* Will raise DTR in open. */
		zs_modem(zst, 0);
	}
}


/*
 * Return pointer to our tty.
 */
struct tty *
zstty(dev)
	dev_t dev;
{
	struct zstty_softc *zst;
	int unit = minor(dev);

#ifdef	DIAGNOSTIC
	if (unit >= zstty_cd.cd_ndevs)
		panic("zstty");
#endif
	zst = zstty_cd.cd_devs[unit];
	return (zst->zst_tty);
}


/*
 * Open a zs serial (tty) port.
 */
int
zsopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct tty *tp;
	struct zs_chanstate *cs;
	struct zstty_softc *zst;
	int error, s, s2, unit;

	unit = minor(dev);
	if (unit >= zstty_cd.cd_ndevs)
		return (ENXIO);
	zst = zstty_cd.cd_devs[unit];
	if (zst == NULL)
		return (ENXIO);
	tp = zst->zst_tty;
	cs = zst->zst_cs;

	/* If KGDB took the line, then tp==NULL */
	if (tp == NULL)
		return (EBUSY);

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    p->p_ucred->cr_uid != 0)
		return (EBUSY);

	s = spltty();

	/* We need to set this early for the benefit of zssoft(). */
	SET(tp->t_state, TS_WOPEN);

	/*
	 * Do the following iff this is a first open.
	 */
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		struct termios t;

		tp->t_dev = dev;

		s2 = splzs();

		/* Turn on interrupts. */
		cs->cs_creg[1] = cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_SIE;
		zs_write_reg(cs, 1, cs->cs_creg[1]);

		/* Fetch the current modem control status, needed later. */
		cs->cs_rr0 = zs_read_csr(cs);

		splx(s2);

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = cs->cs_defspeed;
		t.c_cflag = cs->cs_defcflag;
		if (ISSET(zst->zst_swflags, TIOCFLAG_CLOCAL))
			SET(t.c_cflag, CLOCAL);
		if (ISSET(zst->zst_swflags, TIOCFLAG_CRTSCTS))
			SET(t.c_cflag, CRTSCTS);
		if (ISSET(zst->zst_swflags, TIOCFLAG_CDTRCTS))
			SET(t.c_cflag, CDTRCTS);
		if (ISSET(zst->zst_swflags, TIOCFLAG_MDMBUF))
			SET(t.c_cflag, MDMBUF);
		/* Make sure zsparam will see changes. */
		tp->t_ospeed = 0;
		(void) zsparam(tp, &t);
		/*
		 * Note: zsparam has done: cflag, ispeed, ospeed
		 * so we just need to do: iflag, oflag, lflag, cc
		 * For "raw" mode, just leave all zeros.
		 */
		if (!ISSET(zst->zst_hwflags, ZS_HWFLAG_RAW)) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
		} else {
			tp->t_iflag = 0;
			tp->t_oflag = 0;
			tp->t_lflag = 0;
		}
		ttychars(tp);
		ttsetwater(tp);

		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		zs_modem(zst, 1);

		s2 = splzs();

		/* Clear the input ring, and unblock. */
		zst->zst_rbget = zst->zst_rbput = zst->zst_rbuf;
		zst->zst_rbavail = zstty_rbuf_size;
		zs_iflush(cs);
		CLR(zst->zst_rx_flags, RX_ANY_BLOCK);
		zs_hwiflow(zst);

		splx(s2);
	}
	error = 0;

	/* If we're doing a blocking open... */
	if (!ISSET(flags, O_NONBLOCK))
		/* ...then wait for carrier. */
		while (!ISSET(tp->t_state, TS_CARR_ON) &&
		    !ISSET(tp->t_cflag, CLOCAL | MDMBUF)) {
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    ttopen, 0);
			if (error) {
				/*
				 * If the open was interrupted and nobody
				 * else has the device open, then hang up.
				 */
				if (!ISSET(tp->t_state, TS_ISOPEN)) {
					zs_modem(zst, 0);
					CLR(tp->t_state, TS_WOPEN);
					ttwakeup(tp);
				}
				break;
			}
			SET(tp->t_state, TS_WOPEN);
		}

	splx(s);
	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp);
	return (error);
}

/*
 * Close a zs serial port.
 */
int
zsclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(dev)];
	struct zs_chanstate *cs = zst->zst_cs;
	struct tty *tp = zst->zst_tty;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flags);
	ttyclose(tp);

	s = splzs();

	/* If we were asserting flow control, then deassert it. */
	SET(zst->zst_rx_flags, RX_IBUF_BLOCKED);
	zs_hwiflow(zst);

	splx(s);

	/* Clear any break condition set with TIOCSBRK. */
	zs_break(cs, 0);

	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		zs_modem(zst, 0);
		(void) tsleep(cs, TTIPRI, ttclos, hz);
	}

	s = splzs();

	/* Turn off interrupts if not the console. */
	if (ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE))
		cs->cs_creg[1] = cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_SIE;
	else
		cs->cs_creg[1] = cs->cs_preg[1] = 0;
	zs_write_reg(cs, 1, cs->cs_creg[1]);

	splx(s);

	return (0);
}

/*
 * Read/write zs serial port.
 */
int
zsread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(dev)];
	struct tty *tp = zst->zst_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flags));
}

int
zswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(dev)];
	struct tty *tp = zst->zst_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flags));
}

int
zsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(dev)];
	struct zs_chanstate *cs = zst->zst_cs;
	struct tty *tp = zst->zst_tty;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

#ifdef	ZS_MD_IOCTL
	error = ZS_MD_IOCTL;
	if (error >= 0)
		return (error);
#endif	/* ZS_MD_IOCTL */

	switch (cmd) {
	case TIOCSBRK:
		zs_break(cs, 1);
		break;

	case TIOCCBRK:
		zs_break(cs, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = zst->zst_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (error);
		zst->zst_swflags = *(int *)data;
		break;

	case TIOCSDTR:
		zs_modem(zst, 1);
		break;

	case TIOCCDTR:
		zs_modem(zst, 0);
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
	case TIOCMGET:
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Start or restart transmission.
 */
static void
zsstart(tp)
	struct tty *tp;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	struct zs_chanstate *cs = zst->zst_cs;
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (zst->zst_tx_stopped)
		goto out;

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}

	/* Grab the first contiguous region of buffer space. */
	{
		u_char *tba;
		int tbc;

		tba = tp->t_outq.c_cf;
		tbc = ndqb(&tp->t_outq, 0);
	
		(void) splzs();

		zst->zst_tba = tba;
		zst->zst_tbc = tbc;
	}

	SET(tp->t_state, TS_BUSY);
	zst->zst_tx_busy = 1;

	/* Enable transmit completion interrupts if necessary. */
	if (!ISSET(cs->cs_preg[1], ZSWR1_TIE)) {
		SET(cs->cs_preg[1], ZSWR1_TIE);
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
	}

	/* Output the first character of the contiguous buffer. */
	{
		zs_write_data(cs, *zst->zst_tba);
		zst->zst_tbc--;
		zst->zst_tba++;
	}
out:
	splx(s);
	return;
}

/*
 * Stop output, e.g., for ^S or output flush.
 */
void
zsstop(tp, flag)
	struct tty *tp;
	int flag;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	int s;

	s = splzs();
	if (ISSET(tp->t_state, TS_BUSY)) {
		/* Stop transmitting at the next chunk. */
		zst->zst_tbc = 0;
		zst->zst_heldtbc = 0;
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
}

/*
 * Set ZS tty parameters from termios.
 * XXX - Should just copy the whole termios after
 * making sure all the changes could be done.
 */
static int
zsparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	struct zs_chanstate *cs = zst->zst_cs;
	int ospeed, cflag;
	u_char tmp3, tmp4, tmp5, tmp15;
	int s, error;

	ospeed = t->c_ospeed;
	cflag = t->c_cflag;

	/* Check requested parameters. */
	if (ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != ospeed)
		return (EINVAL);

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(zst->zst_swflags, TIOCFLAG_SOFTCAR) ||
	    ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE)) {
		SET(cflag, CLOCAL);
		CLR(cflag, HUPCL);
	}

	/*
	 * Only whack the UART when params change.
	 * Some callers need to clear tp->t_ospeed
	 * to make sure initialization gets done.
	 */
	if (tp->t_ospeed == ospeed &&
	    tp->t_cflag == cflag)
		return (0);

	/*
	 * Call MD functions to deal with changed
	 * clock modes or H/W flow control modes.
	 * The BRG divisor is set now. (reg 12,13)
	 */
	error = zs_set_speed(cs, ospeed);
	if (error)
		return (error);
	error = zs_set_modes(cs, cflag);
	if (error)
		return (error);

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 *
	 * Initial values in cs_preg are set before
	 * our attach routine is called.  The master
	 * interrupt enable is handled by zsc.c
	 *
	 */
	s = splzs();

	cs->cs_rr0_mask = cs->cs_rr0_cts | cs->cs_rr0_dcd;
	tmp15 = cs->cs_preg[15];
#if 0
	if (ISSET(cs->cs_rr0_mask, ZSRR0_DCD))
		SET(tmp15, ZSWR15_DCD_IE);
	else
		CLR(tmp15, ZSWR15_DCD_IE);
	if (ISSET(cs->cs_rr0_mask, ZSRR0_CTS))
		SET(tmp15, ZSWR15_CTS_IE);
	else
		CLR(tmp15, ZSWR15_CTS_IE);
#else
	SET(tmp15, ZSWR15_DCD_IE | ZSWR15_CTS_IE);
#endif
	cs->cs_preg[15] = tmp15;

	/* Recompute character size bits. */
	tmp3 = cs->cs_preg[3];
	tmp5 = cs->cs_preg[5];
	CLR(tmp3, ZSWR3_RXSIZE);
	CLR(tmp5, ZSWR5_TXSIZE);
	switch (ISSET(cflag, CSIZE)) {
	case CS5:
		SET(tmp3, ZSWR3_RX_5);
		SET(tmp5, ZSWR5_TX_5);
		break;
	case CS6:
		SET(tmp3, ZSWR3_RX_6);
		SET(tmp5, ZSWR5_TX_6);
		break;
	case CS7:
		SET(tmp3, ZSWR3_RX_7);
		SET(tmp5, ZSWR5_TX_7);
		break;
	case CS8:
		SET(tmp3, ZSWR3_RX_8);
		SET(tmp5, ZSWR5_TX_8);
		break;
	}
	cs->cs_preg[3] = tmp3;
	cs->cs_preg[5] = tmp5;

	/*
	 * Recompute the stop bits and parity bits.  Note that
	 * zs_set_speed() may have set clock selection bits etc.
	 * in wr4, so those must preserved.
	 */
	tmp4 = cs->cs_preg[4];
	CLR(tmp4, ZSWR4_SBMASK | ZSWR4_PARMASK);
	if (ISSET(cflag, CSTOPB))
		SET(tmp4, ZSWR4_TWOSB);
	else
		SET(tmp4, ZSWR4_ONESB);
	if (!ISSET(cflag, PARODD))
		SET(tmp4, ZSWR4_EVENP);
	if (ISSET(cflag, PARENB))
		SET(tmp4, ZSWR4_PARENB);
	cs->cs_preg[4] = tmp4;

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = ospeed;
	tp->t_cflag = cflag;

	/*
	 * If nothing is being transmitted, set up new current values,
	 * else mark them as pending.
	 */
	if (!cs->cs_heldchange) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 1;
		} else
			zs_loadchannelregs(cs);
	}

	if (!ISSET(cflag, CHWFLOW)) {
		/* Disable the high water mark. */
		zst->zst_r_hiwat = 0;
		zst->zst_r_lowat = 0;
		if (ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			zst->zst_rx_ready = 1;
			cs->cs_softreq = 1;
		}
		if (ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
			CLR(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
			zs_hwiflow(zst);
		}
	} else {
		zst->zst_r_hiwat = zstty_rbuf_hiwat;
		zst->zst_r_lowat = zstty_rbuf_lowat;
	}

	splx(s);

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	(void) (*linesw[tp->t_line].l_modem)(tp, ISSET(cs->cs_rr0, ZSRR0_DCD));

	if (!ISSET(cflag, CHWFLOW)) {
		if (zst->zst_tx_stopped) {
			zst->zst_tx_stopped = 0;
			zsstart(tp);
		}
	}

	return (0);
}

/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
zs_modem(zst, onoff)
	struct zstty_softc *zst;
	int onoff;
{
	struct zs_chanstate *cs = zst->zst_cs;
	int s;

	if (cs->cs_wr5_dtr == 0)
		return;

	s = splzs();
	if (onoff)
		SET(cs->cs_preg[5], cs->cs_wr5_dtr);
	else
		CLR(cs->cs_preg[5], cs->cs_wr5_dtr);

	if (!cs->cs_heldchange) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 1;
		} else
			zs_loadchannelregs(cs);
	}
	splx(s);
}

/*
 * Try to block or unblock input using hardware flow-control.
 * This is called by kern/tty.c if MDMBUF|CRTSCTS is set, and
 * if this function returns non-zero, the TS_TBLOCK flag will
 * be set or cleared according to the "block" arg passed.
 */
int
zshwiflow(tp, block)
	struct tty *tp;
	int block;
{
	struct zstty_softc *zst = zstty_cd.cd_devs[minor(tp->t_dev)];
	struct zs_chanstate *cs = zst->zst_cs;
	int s;

	if (cs->cs_wr5_rts == 0)
		return (0);

	s = splzs();
	if (block) {
		if (!ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
			SET(zst->zst_rx_flags, RX_TTY_BLOCKED);
			zs_hwiflow(zst);
		}
	} else {
		if (ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			zst->zst_rx_ready = 1;
			cs->cs_softreq = 1;
		}
		if (ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
			CLR(zst->zst_rx_flags, RX_TTY_BLOCKED);
			zs_hwiflow(zst);
		}
	}
	splx(s);
	return (1);
}

/*
 * Internal version of zshwiflow
 * called at splzs
 */
static void
zs_hwiflow(zst)
	struct zstty_softc *zst;
{
	struct zs_chanstate *cs = zst->zst_cs;

	if (cs->cs_wr5_rts == 0)
		return;

	if (ISSET(zst->zst_rx_flags, RX_ANY_BLOCK)) {
		CLR(cs->cs_preg[5], cs->cs_wr5_rts);
		CLR(cs->cs_creg[5], cs->cs_wr5_rts);
	} else {
		SET(cs->cs_preg[5], cs->cs_wr5_rts);
		SET(cs->cs_creg[5], cs->cs_wr5_rts);
	}
	zs_write_reg(cs, 5, cs->cs_creg[5]);
}


/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static void zstty_rxint __P((struct zs_chanstate *));
static void zstty_txint __P((struct zs_chanstate *));
static void zstty_stint __P((struct zs_chanstate *));

#define	integrate	static inline
static void zstty_softint  __P((struct zs_chanstate *));
integrate void zstty_rxsoft __P((struct zstty_softc *, struct tty *));
integrate void zstty_txsoft __P((struct zstty_softc *, struct tty *));
integrate void zstty_stsoft __P((struct zstty_softc *, struct tty *));
static void zstty_diag __P((void *));

/*
 * receiver ready interrupt.
 * called at splzs
 */
static void
zstty_rxint(cs)
	struct zs_chanstate *cs;
{
	struct zstty_softc *zst = cs->cs_private;
	u_char *put, *end;
	u_int cc;
	u_char rr0, rr1, c;

	end = zst->zst_ebuf;
	put = zst->zst_rbput;
	cc = zst->zst_rbavail;

	while (cc > 0) {
		/*
		 * First read the status, because reading the received char
		 * destroys the status of this char.
		 */
		rr1 = zs_read_reg(cs, 1);
		c = zs_read_data(cs);

		if (ISSET(rr1, ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
			/* Clear the receive error. */
			zs_write_csr(cs, ZSWR0_RESET_ERRORS);
		}

		put[0] = c;
		put[1] = rr1;
		put += 2;
		if (put >= end)
			put = zst->zst_rbuf;
		cc--;

		rr0 = zs_read_csr(cs);
		if (!ISSET(rr0, ZSRR0_RX_READY))
			break;
	}

	/*
	 * Current string of incoming characters ended because
	 * no more data was available or we ran out of space.
	 * Schedule a receive event if any data was received.
	 * If we're out of space, turn off receive interrupts.
	 */
	zst->zst_rbput = put;
	zst->zst_rbavail = cc;
	if (!ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
		zst->zst_rx_ready = 1;
		cs->cs_softreq = 1;
	}

	/*
	 * See if we are in danger of overflowing a buffer. If
	 * so, use hardware flow control to ease the pressure.
	 */
	if (!ISSET(zst->zst_rx_flags, RX_IBUF_BLOCKED) &&
	    cc < zst->zst_r_hiwat) {
		SET(zst->zst_rx_flags, RX_IBUF_BLOCKED);
		zs_hwiflow(zst);
	}

	/*
	 * If we're out of space, disable receive interrupts
	 * until the queue has drained a bit.
	 */
	if (!cc) {
		SET(zst->zst_rx_flags, RX_IBUF_OVERFLOWED);
		CLR(cs->cs_preg[1], ZSWR1_RIE);
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
	}

#if 0
	printf("%xH%04d\n", zst->zst_rx_flags, zst->zst_rbavail);
#endif
}

/*
 * transmitter ready interrupt.  (splzs)
 */
static void
zstty_txint(cs)
	struct zs_chanstate *cs;
{
	struct zstty_softc *zst = cs->cs_private;

	/*
	 * If we've delayed a parameter change, do it now, and restart
	 * output.
	 */
	if (cs->cs_heldchange) {
		zs_loadchannelregs(cs);
		cs->cs_heldchange = 0;
		zst->zst_tbc = zst->zst_heldtbc;
		zst->zst_heldtbc = 0;
	}

	/* Output the next character in the buffer, if any. */
	if (cs->cs_heldchar != 0) {
		/* An "out-of-band" character is waiting to be output */
		zs_write_data(cs, cs->cs_heldchar);
		cs->cs_heldchar = 0;
	} else if (zst->zst_tbc > 0) {
		zs_write_data(cs, *zst->zst_tba);
		zst->zst_tbc--;
		zst->zst_tba++;
	} else {
		/* Disable transmit completion interrupts if necessary. */
		if (ISSET(cs->cs_preg[1], ZSWR1_TIE)) {
			CLR(cs->cs_preg[1], ZSWR1_TIE);
			cs->cs_creg[1] = cs->cs_preg[1];
			zs_write_reg(cs, 1, cs->cs_creg[1]);
		}
		if (zst->zst_tx_busy) {
			zst->zst_tx_busy = 0;
			zst->zst_tx_done = 1;
			cs->cs_softreq = 1;
		}
	}
}

/*
 * status change interrupt.  (splzs)
 */
static void
zstty_stint(cs)
	struct zs_chanstate *cs;
{
	struct zstty_softc *zst = cs->cs_private;
	u_char rr0, delta;

	rr0 = zs_read_csr(cs);
	zs_write_csr(cs, ZSWR0_RESET_STATUS);

	/*
	 * Check here for console break, so that we can abort
	 * even when interrupts are locking up the machine.
	 */
	if (ISSET(rr0, ZSRR0_BREAK) &&
	    ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE)) {
		zs_abort(cs);
		return;
	}

	delta = rr0 ^ cs->cs_rr0;
	cs->cs_rr0 = rr0;
	if (ISSET(delta, cs->cs_rr0_mask)) {
		SET(cs->cs_rr0_delta, delta);

		/*
		 * Stop output immediately if we lose the output
		 * flow control signal or carrier detect.
		 */
		if (ISSET(~rr0, cs->cs_rr0_mask)) {
			zst->zst_tbc = 0;
			zst->zst_heldtbc = 0;
		}

		zst->zst_st_check = 1;
		cs->cs_softreq = 1;
	}
}

void
zstty_diag(arg)
	void *arg;
{
	struct zstty_softc *zst = arg;
	int overflows, floods;
	int s;

	s = splzs();
	overflows = zst->zst_overflows;
	zst->zst_overflows = 0;
	floods = zst->zst_floods;
	zst->zst_floods = 0;
	zst->zst_errors = 0;
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf flood%s\n",
	    zst->zst_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

integrate void
zstty_rxsoft(zst, tp)
	struct zstty_softc *zst;
	struct tty *tp;
{
	struct zs_chanstate *cs = zst->zst_cs;
	int (*rint) __P((int c, struct tty *tp)) = linesw[tp->t_line].l_rint;
	u_char *get, *end;
	u_int cc, scc;
	u_char rr1;
	int code;
	int s;

	end = zst->zst_ebuf;
	get = zst->zst_rbget;
	scc = cc = zstty_rbuf_size - zst->zst_rbavail;

	if (cc == zstty_rbuf_size) {
		zst->zst_floods++;
		if (zst->zst_errors++ == 0)
			timeout(zstty_diag, zst, 60 * hz);
	}

	while (cc) {
		code = get[0];
		rr1 = get[1];
		if (ISSET(rr1, ZSRR1_DO | ZSRR1_FE | ZSRR1_PE)) {
			if (ISSET(rr1, ZSRR1_DO)) {
				zst->zst_overflows++;
				if (zst->zst_errors++ == 0)
					timeout(zstty_diag, zst, 60 * hz);
			}
			if (ISSET(rr1, ZSRR1_FE))
				SET(code, TTY_FE);
			if (ISSET(rr1, ZSRR1_PE))
				SET(code, TTY_PE);
		}
		if ((*rint)(code, tp) == -1) {
			/*
			 * The line discipline's buffer is out of space.
			 */
			if (!ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
				/*
				 * We're either not using flow control, or the
				 * line discipline didn't tell us to block for
				 * some reason.  Either way, we have no way to
				 * know when there's more space available, so
				 * just drop the rest of the data.
				 */
				get += cc << 1;
				if (get >= end)
					get -= zstty_rbuf_size << 1;
				cc = 0;
			} else {
				/*
				 * Don't schedule any more receive processing
				 * until the line discipline tells us there's
				 * space available (through comhwiflow()).
				 * Leave the rest of the data in the input
				 * buffer.
				 */
				SET(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			}
			break;
		}
		get += 2;
		if (get >= end)
			get = zst->zst_rbuf;
		cc--;
	}

	if (cc != scc) {
		zst->zst_rbget = get;
		s = splzs();
		cc = zst->zst_rbavail += scc - cc;
		/* Buffers should be ok again, release possible block. */
		if (cc >= zst->zst_r_lowat) {
			if (ISSET(zst->zst_rx_flags, RX_IBUF_OVERFLOWED)) {
				CLR(zst->zst_rx_flags, RX_IBUF_OVERFLOWED);
				SET(cs->cs_preg[1], ZSWR1_RIE);
				cs->cs_creg[1] = cs->cs_preg[1];
				zs_write_reg(cs, 1, cs->cs_creg[1]);
			}
			if (ISSET(zst->zst_rx_flags, RX_IBUF_BLOCKED)) {
				CLR(zst->zst_rx_flags, RX_IBUF_BLOCKED);
				zs_hwiflow(zst);
			}
		}
		splx(s);
	}

#if 0
	printf("%xS%04d\n", zst->zst_rx_flags, zst->zst_rbavail);
#endif
}

integrate void
zstty_txsoft(zst, tp)
	struct zstty_softc *zst;
	struct tty *tp;
{

	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, (int)(zst->zst_tba - tp->t_outq.c_cf));
	(*linesw[tp->t_line].l_start)(tp);
}

integrate void
zstty_stsoft(zst, tp)
	struct zstty_softc *zst;
	struct tty *tp;
{
	struct zs_chanstate *cs = zst->zst_cs;
	u_char rr0, delta;
	int s;

	s = splzs();
	rr0 = cs->cs_rr0;
	delta = cs->cs_rr0_delta;
	cs->cs_rr0_delta = 0;
	splx(s);

	if (ISSET(delta, cs->cs_rr0_dcd)) {
		/*
		 * Inform the tty layer that carrier detect changed.
		 */
		(void) (*linesw[tp->t_line].l_modem)(tp, ISSET(rr0, ZSRR0_DCD));
	}

	if (ISSET(delta, cs->cs_rr0_cts)) {
		/* Block or unblock output according to flow control. */
		if (ISSET(rr0, cs->cs_rr0_cts)) {
			zst->zst_tx_stopped = 0;
			(*linesw[tp->t_line].l_start)(tp);
		} else {
			zst->zst_tx_stopped = 1;
		}
	}
}

/*
 * Software interrupt.  Called at zssoft
 *
 * The main job to be done here is to empty the input ring
 * by passing its contents up to the tty layer.  The ring is
 * always emptied during this operation, therefore the ring
 * must not be larger than the space after "high water" in
 * the tty layer, or the tty layer might drop our input.
 *
 * Note: an "input blockage" condition is assumed to exist if
 * EITHER the TS_TBLOCK flag or zst_rx_blocked flag is set.
 */
static void
zstty_softint(cs)
	struct zs_chanstate *cs;
{
	struct zstty_softc *zst = cs->cs_private;
	struct tty *tp = zst->zst_tty;
	int s;

	s = spltty();

	if (zst->zst_rx_ready) {
		zst->zst_rx_ready = 0;
		zstty_rxsoft(zst, tp);
	}

	if (zst->zst_st_check) {
		zst->zst_st_check = 0;
		zstty_stsoft(zst, tp);
	}

	if (zst->zst_tx_done) {
		zst->zst_tx_done = 0;
		zstty_txsoft(zst, tp);
	}

	splx(s);
}

struct zsops zsops_tty = {
	zstty_rxint,	/* receive char available */
	zstty_stint,	/* external/status */
	zstty_txint,	/* xmit buffer empty */
	zstty_softint,	/* process software interrupt */
};
