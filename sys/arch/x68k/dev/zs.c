/*	$NetBSD: zs.c,v 1.2.4.1 1996/06/05 18:06:42 oki Exp $ */

/*
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
 * Zilog Z8530 (ZSCC) driver.
 *
 * Runs two tty ports (ttya and ttyb) on zs0,
 * and runs a keyboard and mouse on zs1.
 *
 * This driver knows far too much about chip to usage mappings.
 */
#include "zs.h"
#if NZS > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <x68k/x68k/iodevice.h>
#include <dev/ic/z8530reg.h>
#include <x68k/dev/zsvar.h>

#ifdef KGDB
#include <machine/remote-sl.h>
#endif

#define	ZSMAJOR	12		/* XXX */

#define	ZS_MOUSE	1	/* XXX */

#define PCLK	(5*1000*1000)	/* PCLK pin input clock rate */

#if 0
/*
 * Select software interrupt bit based on TTY ipl.
 */
#if PIL_TTY == 1
# define IE_ZSSOFT IE_L1
#elif PIL_TTY == 4
# define IE_ZSSOFT IE_L4
#elif PIL_TTY == 6
# define IE_ZSSOFT IE_L6
#else
# error "no suitable software interrupt bit"
#endif
#endif

/*
 * Software state per found chip.  This would be called `zs_softc',
 * but the previous driver had a rather different zs_softc....
 */
struct zs_softc {
	struct	device zi_dev;		/* base device */
	volatile struct zsdevice *zi_zs;/* chip registers */
	struct	zs_chanstate zi_cs[2];	/* channel A and B software state */
};

struct tty *zs_tty[NZS * 2];		/* XXX should be dynamic */

/* Definition of the driver for autoconfig. */
static int	zsmatch __P((struct device *, void *, void *));
static void	zsattach __P((struct device *, struct device *, void *));

struct cfattach zs_ca = {
	sizeof(struct zs_softc), zsmatch, zsattach
};

struct cfdriver zs_cd = {
	NULL, "zs", DV_TTY, NULL, 0
};

#ifdef x68k
static struct zs_chanstate *zsms;
void zs_msmodem __P((int));
#endif

/* Interrupt handlers. */
void	zshard __P((int));
int	zssoft __P((void *));

struct zs_chanstate *zslist;

/* Routines called from other code. */
static void	zsiopen __P((struct tty *));
static void	zsiclose __P((struct tty *));
static void	zsstart __P((struct tty *));
int		zsstop __P((struct tty *, int));
static int	zsparam __P((struct tty *, struct termios *));
static int	zshwiflow __P((struct tty *, int));

/* Routines purely local to this driver. */
static int	zs_getspeed __P((volatile struct zschan *));
#ifdef KGDB
static void	zs_reset __P((volatile struct zschan *, int, int));
#endif
static void	zs_modem __P((struct zs_chanstate *, int));
static void	zs_loadchannelregs __P((volatile struct zschan *, u_char *));
static void	zsabort __P((void));

/* Console stuff. */
static struct tty *zs_ctty;	/* console `struct tty *' */
static int zs_consin = -1, zs_consout = -1;
static void zscnputc __P((int));	/* console putc function */
static volatile struct zschan *zs_conschan;
static struct tty *zs_checkcons __P((struct zs_softc *, int, struct zs_chanstate *));

#ifdef KGDB
/* KGDB stuff.  Must reboot to change zs_kgdbunit. */
extern int kgdb_dev, kgdb_rate;
static int zs_kgdb_savedspeed;
static void zs_checkkgdb __P((int, struct zs_chanstate *, struct tty *));
#endif

static volatile struct zsdevice *findzs __P((int));
static volatile struct zsdevice *zsaddr[NZS];	/* XXX, but saves work */

int zshardscope;
int zsshortcuts;		/* number of "shortcut" software interrupts */

static u_char
zs_read(zc, reg)
	volatile struct zschan *zc;
	u_char reg;
{
	u_char val;

	zc->zc_csr = reg;
	ZS_DELAY();
	val = zc->zc_csr;
	ZS_DELAY();
	return val;
}

static u_char
zs_write(zc, reg, val)
	volatile struct zschan *zc;
	u_char reg, val;
{
	zc->zc_csr = reg;
	ZS_DELAY();
	zc->zc_csr = val;
	ZS_DELAY();
	return val;
}

/*
 * find zs address for x68k architecture
 */
static volatile struct zsdevice *
findzs(zs)
	int zs;
{
	if (zs == 0)
		return &IODEVbase->io_inscc;
	if (1 <= zs && zs <= 4)
		return &(IODEVbase->io_exscc)[zs - 1];
	/* none */
	return 0;
}

/*
 * Match slave number to zs unit number, so that misconfiguration will
 * not set up the keyboard as ttya, etc.
 */
static int
zsmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cfp = match;
	volatile void *addr;

	if(strcmp("zs", aux) || (addr = findzs(cfp->cf_unit)) == 0)
		return(0);
	if (badaddr(addr))
		return 0;
	return(1);
}

/*
 * Attach a found zs.
 *
 * USE ROM PROPERTIES port-a-ignore-cd AND port-b-ignore-cd FOR
 * SOFT CARRIER, AND keyboard PROPERTY FOR KEYBOARD/MOUSE?
 */
static void
zsattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	register int zs = dev->dv_unit, unit;
	register struct zs_softc *zi;
	register struct zs_chanstate *cs;
	register volatile struct zsdevice *addr;
	register struct tty *tp, *ctp;
	register struct confargs *ca = aux;
	int pri;

	if ((addr = zsaddr[zs]) == NULL)
		addr = zsaddr[zs] = findzs(zs);
	printf(" (%s)\n", zs ? "external" : "onboard");
	zi = (struct zs_softc *)dev;
	zi->zi_zs = addr;
	unit = zs * 2;
	cs = zi->zi_cs;
	cs->cs_ttyp = tp = ttymalloc();

	/* link into interrupt list with order (A,B) (B=A+1) */
	cs[0].cs_next = &cs[1];
	cs[1].cs_next = zslist;
	zslist = cs;

	cs->cs_unit = unit;
	cs->cs_speed = zs_getspeed(&addr->zs_chan[ZS_CHAN_A]);
	cs->cs_zc = &addr->zs_chan[ZS_CHAN_A];
	tp->t_dev = makedev(ZSMAJOR, unit);
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_hwiflow = zshwiflow;
	if ((ctp = zs_checkcons(zi, unit, cs)) != NULL)
		cs->cs_ttyp = tp = ctp;
#ifdef KGDB
	if (ctp == NULL)
		zs_checkkgdb(unit, cs, tp);
#endif
#ifdef sun
	if (unit == ZS_KBD) {
		/*
		 * Keyboard: tell /dev/kbd driver how to talk to us.
		 */
		tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		tp->t_cflag = CS8;
		kbd_serial(tp, zsiopen, zsiclose);
		cs->cs_conk = 1;		/* do L1-A processing */
	}
#endif
	if (tp != ctp)
		tty_attach(tp);
	ZS_WRITE(cs->cs_zc, 2, 0x70 + zs); /* XXX interrupt vector */
	unit++;
	cs++;
	cs->cs_ttyp = tp = ttymalloc();
	cs->cs_unit = unit;
	cs->cs_speed = zs_getspeed(&addr->zs_chan[ZS_CHAN_B]);
	cs->cs_zc = &addr->zs_chan[ZS_CHAN_B];
	tp->t_dev = makedev(ZSMAJOR, unit);
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	if (unit != ZS_MOUSE)
		tp->t_hwiflow = zshwiflow;
	if ((ctp = zs_checkcons(zi, unit, cs)) != NULL)
		cs->cs_ttyp = tp = ctp;
#ifdef KGDB
	if (ctp == NULL)
		zs_checkkgdb(unit, cs, tp);
#endif
	if (unit == ZS_MOUSE) {
		/*
		 * Mouse: tell /dev/mouse driver how to talk to us.
		 */
		tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		tp->t_cflag = CS8 | CSTOPB;
		ms_serial(tp, zsiopen, zsiclose);
#ifdef x68k
		zsms = cs;
#endif
	} else {
		if (tp != ctp)
			tty_attach(tp);
	}
}

#ifdef KGDB
/*
 * Put a channel in a known state.  Interrupts may be left disabled
 * or enabled, as desired.
 */
static void
zs_reset(zc, inten, speed)
	volatile struct zschan *zc;
	int inten, speed;
{
	int tconst;
	static u_char reg[16] = {
		0,
		0,
		0,
		ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
		ZSWR4_CLK_X16 | ZSWR4_ONESB | ZSWR4_EVENP,
		ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
		0,
		0,
		0,
		0,
		ZSWR10_NRZ,
		ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
		0,
		0,
		ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA,
		ZSWR15_BREAK_IE | ZSWR15_DCD_IE,
	};

	reg[9] = inten ? ZSWR9_MASTER_IE | ZSWR9_NO_VECTOR : ZSWR9_NO_VECTOR;
	tconst = BPS_TO_TCONST(PCLK / 16, speed);
	reg[12] = tconst;
	reg[13] = tconst >> 8;
	zs_loadchannelregs(zc, reg);
}
#endif

/*
 * Declare the given tty (which is in fact &cons) as a console input
 * or output.  This happens before the zs chip is attached; the hookup
 * is finished later, in zs_setcons() below.
 *
 * This is used only for ports a and b.  The console keyboard is decoded
 * independently (we always send unit-2 input to /dev/kbd, which will
 * direct it to /dev/console if appropriate).
 */
void
zsconsole(tp, unit, out, fnstop)
	register struct tty *tp;
	register int unit;
	int out;
	int (**fnstop) __P((struct tty *, int));
{
	int zs;
	volatile struct zsdevice *addr;

	if (out) {
		zs_consout = unit;
		zs = unit >> 1;
		if ((addr = zsaddr[zs]) == NULL)
			addr = zsaddr[zs] = findzs(zs);
		zs_conschan = (unit & 1) == 0 ? &addr->zs_chan[ZS_CHAN_A] :
		    &addr->zs_chan[ZS_CHAN_B];
		v_putc = zscnputc;
	} else
		zs_consin = unit;
	if(fnstop)
		*fnstop = &zsstop;
	zs_ctty = tp;
}

/*
 * Polled console output putchar.
 */
static void
zscnputc(c)
	int c;
{
	register volatile struct zschan *zc = zs_conschan;
	register int s;

	if (c == '\n')
		zscnputc('\r');
	/*
	 * Must block output interrupts (i.e., raise to >= splzs) without
	 * lowering current ipl.  Need a better way.
	 */
	s = splhigh();
#ifdef SUN4C		/* XXX */
	if (CPU_ISSUN4C && s <= (12 << 8))
		(void) splzs();
#endif
	while ((zc->zc_csr & ZSRR0_TX_READY) == 0)
		ZS_DELAY();
	zc->zc_data = c;
	ZS_DELAY();
	splx(s);
}

/*
 * Set up the given unit as console input, output, both, or neither, as
 * needed.  Return console tty if it is to receive console input.
 */
static struct tty *
zs_checkcons(zi, unit, cs)
	struct zs_softc *zi;
	int unit;
	struct zs_chanstate *cs;
{
	register struct tty *tp;
	char *i, *o;

	if ((tp = zs_ctty) == NULL) /* XXX */
		return (0);
	i = zs_consin == unit ? "input" : NULL;
	o = zs_consout == unit ? "output" : NULL;
	if (i == NULL && o == NULL)
		return (0);

	/* rewire the minor device (gack) */
	tp->t_dev = makedev(major(tp->t_dev), unit);

	/*
	 * Rewire input and/or output.  Note that baud rate reflects
	 * input settings, not output settings, but we can do no better
	 * if the console is split across two ports.
	 *
	 * XXX	split consoles don't work anyway -- this needs to be
	 *	thrown away and redone
	 */
	if (i) {
		tp->t_param = zsparam;
		tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		tp->t_cflag = CS8;
		ttsetwater(tp);
	}
	if (o) {
		tp->t_oproc = zsstart;
	}
	printf("%s%c: console %s\n",
	    zi->zi_dev.dv_xname, (unit & 1) + 'a', i ? (o ? "i/o" : i) : o);
	cs->cs_consio = 1;
	cs->cs_brkabort = 1;
	return (tp);
}

#ifdef KGDB
/*
 * The kgdb zs port, if any, was altered at boot time (see zs_kgdb_init).
 * Pick up the current speed and character size and restore the original
 * speed.
 */
static void
zs_checkkgdb(unit, cs, tp)
	int unit;
	struct zs_chanstate *cs;
	struct tty *tp;
{

	if (kgdb_dev == makedev(ZSMAJOR, unit)) {
		tp->t_ispeed = tp->t_ospeed = kgdb_rate;
		tp->t_cflag = CS8;
		cs->cs_kgdb = 1;
		cs->cs_speed = zs_kgdb_savedspeed;
		(void) zsparam(tp, &tp->t_termios);
	}
}
#endif

/*
 * Compute the current baud rate given a ZSCC channel.
 */
static int
zs_getspeed(zc)
	register volatile struct zschan *zc;
{
	register int tconst;

	tconst = ZS_READ(zc, 12);
	tconst |= ZS_READ(zc, 13) << 8;
	return (TCONST_TO_BPS(PCLK / 16, tconst));
}


/*
 * Do an internal open.
 */
static void
zsiopen(tp)
	struct tty *tp;
{

	(void) zsparam(tp, &tp->t_termios);
	ttsetwater(tp);
	tp->t_state = TS_ISOPEN | TS_CARR_ON;
}

/*
 * Do an internal close.  Eventually we should shut off the chip when both
 * ports on it are closed.
 */
static void
zsiclose(tp)
	struct tty *tp;
{

	ttylclose(tp, 0);	/* ??? */
	ttyclose(tp);		/* ??? */
	tp->t_state = 0;
}


/*
 * Open a zs serial port.  This interface may not be used to open
 * the keyboard and mouse ports. (XXX)
 */
int
zsopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	register struct tty *tp;
	register struct zs_chanstate *cs;
	struct zs_softc *zi;
	int unit = minor(dev), zs = unit >> 1, error, s;

	if (zs >= zs_cd.cd_ndevs || (zi = zs_cd.cd_devs[zs]) == NULL ||
	    unit == ZS_MOUSE)
		return (ENXIO);
	if (zi->zi_zs == NULL)
		return (ENXIO);
	cs = &zi->zi_cs[unit & 1];
	if (cs->cs_consio)
		return (ENXIO);		/* ??? */
	tp = cs->cs_ttyp;
	s = spltty();
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cs->cs_speed;
		}
		(void) zsparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}
	error = 0;
	for (;;) {
		register int rr0;

		/* loop, turning on the device, until carrier present */
		zs_modem(cs, 1);
		/* May never get status intr if carrier already on. -gwr */
		rr0 = cs->cs_zc->zc_csr;
		ZS_DELAY();
		if ((rr0 & ZSRR0_DCD) || cs->cs_softcar)
			tp->t_state |= TS_CARR_ON;
		if (flags & O_NONBLOCK || tp->t_cflag & CLOCAL ||
		    tp->t_state & TS_CARR_ON)
			break;
		tp->t_state |= TS_WOPEN;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
				 ttopen, 0);
		if (error) {
			if (!(tp->t_state & TS_ISOPEN)) {
				zs_modem(cs, 0);
				tp->t_state &= ~TS_WOPEN;
				ttwakeup(tp);
			}
			splx(s);
			return error;
		}
	}
	splx(s);
	if (error == 0)
		error = linesw[tp->t_line].l_open(dev, tp);
	if (error)
		zs_modem(cs, 0);
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
	register struct zs_chanstate *cs;
	register struct tty *tp;
	struct zs_softc *zi;
	int unit = minor(dev), s;

	zi = zs_cd.cd_devs[unit >> 1];
	cs = &zi->zi_cs[unit & 1];
	tp = cs->cs_ttyp;
	linesw[tp->t_line].l_close(tp, flags);
	if (tp->t_cflag & HUPCL || tp->t_state & TS_WOPEN ||
	    (tp->t_state & TS_ISOPEN) == 0) {
		zs_modem(cs, 0);
		/* hold low for 1 second */
		(void) tsleep((caddr_t)cs, TTIPRI, ttclos, hz);
	}
	if (cs->cs_creg[5] & ZSWR5_BREAK)
	{
		s = splzs();
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
	}
	ttyclose(tp);
#ifdef KGDB
	/* Reset the speed if we're doing kgdb on this port */
	if (cs->cs_kgdb) {
		tp->t_ispeed = tp->t_ospeed = kgdb_rate;
		(void) zsparam(tp, &tp->t_termios);
	}
#endif
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
	register struct zs_chanstate *cs;
	register struct zs_softc *zi;
	register struct tty *tp;
	int unit = minor(dev);

	zi = zs_cd.cd_devs[unit >> 1];
	cs = &zi->zi_cs[unit & 1];
	tp = cs->cs_ttyp;

	return (linesw[tp->t_line].l_read(tp, uio, flags));

}

int
zswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register struct zs_chanstate *cs;
	register struct zs_softc *zi;
	register struct tty *tp;
	int unit = minor(dev);

	zi = zs_cd.cd_devs[unit >> 1];
	cs = &zi->zi_cs[unit & 1];
	tp = cs->cs_ttyp;

	return (linesw[tp->t_line].l_write(tp, uio, flags));
}

struct tty *
zstty(dev)
	dev_t dev;
{
	register struct zs_chanstate *cs;
	register struct zs_softc *zi;
	int unit = minor(dev);

	zi = zs_cd.cd_devs[unit >> 1];
	cs = &zi->zi_cs[unit & 1];

	return (cs->cs_ttyp);

}

/*
 * ZS hardware interrupt.  Scan all ZS channels.  NB: we know here that
 * channels are kept in (A,B) pairs.
 *
 * Do just a little, then get out; set a software interrupt if more
 * work is needed.
 *
 * We deliberately ignore the vectoring Zilog gives us, and match up
 * only the number of `reset interrupt under service' operations, not
 * the order.
 */
/* ARGSUSED */
void
zshard(intrarg)
	int intrarg;
{
	register struct zs_chanstate *a;
#define	b (a + 1)
	register volatile struct zschan *zc;
	register int rr3, intflags = 0, v, i;
	static int zsrint __P((struct zs_chanstate *, volatile struct zschan *));
	static int zsxint __P((struct zs_chanstate *, volatile struct zschan *));
	static int zssint __P((struct zs_chanstate *, volatile struct zschan *));

	{
		a = &((struct zs_softc*)zs_cd.cd_devs[(intrarg >> 2) & 0x0f])->zi_cs[0];
		rr3 = ZS_READ(a->cs_zc, 3);
		if (rr3 & (ZSRR3_IP_A_RX|ZSRR3_IP_A_TX|ZSRR3_IP_A_STAT)) {
			intflags |= 2;
			zc = a->cs_zc;
			i = a->cs_rbput;
			if (rr3 & ZSRR3_IP_A_RX && (v = zsrint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
			}
			if (rr3 & ZSRR3_IP_A_TX && (v = zsxint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
				intflags |= 4;
			}
			if (rr3 & ZSRR3_IP_A_STAT && (v = zssint(a, zc)) != 0) {
				a->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
			}
			a->cs_rbput = i;
		}
		if (rr3 & (ZSRR3_IP_B_RX|ZSRR3_IP_B_TX|ZSRR3_IP_B_STAT)) {
			intflags |= 2;
			zc = b->cs_zc;
			i = b->cs_rbput;
			if (rr3 & ZSRR3_IP_B_RX && (v = zsrint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
			}
			if (rr3 & ZSRR3_IP_B_TX && (v = zsxint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
				intflags |= 4;
			}
			if (rr3 & ZSRR3_IP_B_STAT && (v = zssint(b, zc)) != 0) {
				b->cs_rbuf[i++ & ZLRB_RING_MASK] = v;
				intflags |= 1;
			}
			b->cs_rbput = i;
		}
	}
#undef b
	if (intflags & 1) {
#if defined(SUN4C) || defined(SUN4M)
		if (CPU_ISSUN4M || CPU_ISSUN4C) {
			/* XXX -- but this will go away when zshard moves to locore.s */
			struct clockframe *p = intrarg;

			if ((p->psr & PSR_PIL) < (PIL_TTY << 8)) {
				zsshortcuts++;
				(void) spltty();
				if (zshardscope) {
					LED_ON;
					LED_OFF;
				}
				return (zssoft(intrarg));
			}
		}
#endif
#if x68k
#define PSL_TTY PSL_IPL4 /* XXX */
		if (((intrarg >> 16) & PSL_IPL) < PSL_TTY) {
			zsshortcuts++;
			(void) spltty();
			zssoft(0/*intrarg*/);
			return;
		}
		setsoftserial();
#else
		ienab_bis(IE_ZSSOFT);
#endif
	}
}

static int
zsrint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register int c = zc->zc_data;

	ZS_DELAY();
#ifndef x68k
	if (cs->cs_conk) {
		register struct conk_state *conk = &zsconk_state;

		/*
		 * Check here for console abort function, so that we
		 * can abort even when interrupts are locking up the
		 * machine.
		 */
		if (c == KBD_RESET) {
			conk->conk_id = 1;	/* ignore next byte */
			conk->conk_l1 = 0;
		} else if (conk->conk_id)
			conk->conk_id = 0;	/* stop ignoring bytes */
		else if (c == KBD_L1)
			conk->conk_l1 = 1;	/* L1 went down */
		else if (c == (KBD_L1|KBD_UP))
			conk->conk_l1 = 0;	/* L1 went up */
		else if (c == KBD_A && conk->conk_l1) {
			zsabort();
			conk->conk_l1 = 0;	/* we never see the up */
			goto clearit;		/* eat the A after L1-A */
		}
	}
#endif
#ifdef KGDB
	if (c == FRAME_START && cs->cs_kgdb && 
	    (cs->cs_ttyp->t_state & TS_ISOPEN) == 0) {
		zskgdb(cs->cs_unit);
		goto clearit;
	}
#endif
	/* compose receive character and status */
	c <<= 8;
	c |= ZS_READ(zc, 1);

	/* clear receive error & interrupt condition */
	zc->zc_csr = ZSWR0_RESET_ERRORS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();

	return (ZRING_MAKE(ZRING_RINT, c));

clearit:
	zc->zc_csr = ZSWR0_RESET_ERRORS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	return (0);
}

static int
zsxint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register int i = cs->cs_tbc;

	if (i == 0) {
		zc->zc_csr = ZSWR0_RESET_TXINT;
		ZS_DELAY();
		zc->zc_csr = ZSWR0_CLR_INTR;
		ZS_DELAY();
		return (ZRING_MAKE(ZRING_XINT, 0));
	}
	cs->cs_tbc = i - 1;
	zc->zc_data = *cs->cs_tba++;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	return (0);
}

static int
zssint(cs, zc)
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
{
	register int rr0;

	rr0 = zc->zc_csr;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_RESET_STATUS;
	ZS_DELAY();
	zc->zc_csr = ZSWR0_CLR_INTR;
	ZS_DELAY();
	/*
	 * The chip's hardware flow control is, as noted in zsreg.h,
	 * busted---if the DCD line goes low the chip shuts off the
	 * receiver (!).  If we want hardware CTS flow control but do
	 * not have it, and carrier is now on, turn HFC on; if we have
	 * HFC now but carrier has gone low, turn it off.
	 */
	if (rr0 & ZSRR0_DCD) {
		if (cs->cs_ttyp->t_cflag & CCTS_OFLOW &&
		    (cs->cs_creg[3] & ZSWR3_HFC) == 0) {
			cs->cs_creg[3] |= ZSWR3_HFC;
			ZS_WRITE(zc, 3, cs->cs_creg[3]);
		}
	} else {
		if (cs->cs_creg[3] & ZSWR3_HFC) {
			cs->cs_creg[3] &= ~ZSWR3_HFC;
			ZS_WRITE(zc, 3, cs->cs_creg[3]);
		}
	}
	if ((rr0 & ZSRR0_BREAK) && cs->cs_brkabort) {
#ifdef SUN4
		/*
		 * XXX This might not be necessary. Test and
		 * delete if it isn't.
		 */
		if (CPU_ISSUN4) {
			while (zc->zc_csr & ZSRR0_BREAK)
				ZS_DELAY();
		}
#endif
		zsabort();
		return (0);
	}
	return (ZRING_MAKE(ZRING_SINT, rr0));
}

static void
zsabort()
{

#ifdef DDB
	Debugger();
#else
	printf("stopping on keyboard abort\n");
#ifndef x68k
	callrom();
#endif
#endif
}

#ifdef KGDB
/*
 * KGDB framing character received: enter kernel debugger.  This probably
 * should time out after a few seconds to avoid hanging on spurious input.
 */
void
zskgdb(unit)
	int unit;
{

	printf("zs%d%c: kgdb interrupt\n", unit >> 1, (unit & 1) + 'a');
	kgdb_connect(1);
}
#endif

/*
 * Print out a ring or fifo overrun error message.
 */
static void
zsoverrun(unit, ptime, what)
	int unit;
	long *ptime;
	char *what;
{

	if (*ptime != time.tv_sec) {
		*ptime = time.tv_sec;
		log(LOG_WARNING, "zs%d%c: %s overrun\n", unit >> 1,
		    (unit & 1) + 'a', what);
	}
}

/*
 * ZS software interrupt.  Scan all channels for deferred interrupts.
 */
int
zssoft(arg)
	void *arg;
{
	register struct zs_chanstate *cs;
	register volatile struct zschan *zc;
	register struct linesw *line;
	register struct tty *tp;
	register int get, n, c, cc, unit, s;
	int	retval = 0;

	for (cs = zslist; cs != NULL; cs = cs->cs_next) {
		get = cs->cs_rbget;
again:
		n = cs->cs_rbput;	/* atomic */
		if (get == n)		/* nothing more on this line */
			continue;
		retval = 1;
		unit = cs->cs_unit;	/* set up to handle interrupts */
		zc = cs->cs_zc;
		tp = cs->cs_ttyp;
		line = &linesw[tp->t_line];
		/*
		 * Compute the number of interrupts in the receive ring.
		 * If the count is overlarge, we lost some events, and
		 * must advance to the first valid one.  It may get
		 * overwritten if more data are arriving, but this is
		 * too expensive to check and gains nothing (we already
		 * lost out; all we can do at this point is trade one
		 * kind of loss for another).
		 */
		n -= get;
		if (n > ZLRB_RING_SIZE) {
			zsoverrun(unit, &cs->cs_rotime, "ring");
			get += n - ZLRB_RING_SIZE;
			n = ZLRB_RING_SIZE;
		}
		while (--n >= 0) {
			/* race to keep ahead of incoming interrupts */
			c = cs->cs_rbuf[get++ & ZLRB_RING_MASK];
			switch (ZRING_TYPE(c)) {

			case ZRING_RINT:
				c = ZRING_VALUE(c);
				if (c & ZSRR1_DO)
					zsoverrun(unit, &cs->cs_fotime, "fifo");
				cc = c >> 8;
				if (c & ZSRR1_FE)
					cc |= TTY_FE;
				if (c & ZSRR1_PE)
					cc |= TTY_PE;
				/*
				 * this should be done through
				 * bstreams	XXX gag choke
				 */
				else if (unit == ZS_MOUSE)
					ms_rint(cc);
				else
					line->l_rint(cc, tp);
				break;

			case ZRING_XINT:
				/*
				 * Transmit done: change registers and resume,
				 * or clear BUSY.
				 */
				if (cs->cs_heldchange) {
					s = splzs();
					c = zc->zc_csr;
					ZS_DELAY();
					if ((c & ZSRR0_DCD) == 0)
						cs->cs_preg[3] &= ~ZSWR3_HFC;
					bcopy((caddr_t)cs->cs_preg,
					    (caddr_t)cs->cs_creg, 16);
					zs_loadchannelregs(zc, cs->cs_creg);
					splx(s);
					cs->cs_heldchange = 0;
					if (cs->cs_heldtbc &&
					    (tp->t_state & TS_TTSTOP) == 0) {
						cs->cs_tbc = cs->cs_heldtbc - 1;
						zc->zc_data = *cs->cs_tba++;
						ZS_DELAY();
						goto again;
					}
				}
				tp->t_state &= ~TS_BUSY;
				if (tp->t_state & TS_FLUSH)
					tp->t_state &= ~TS_FLUSH;
				else
					ndflush(&tp->t_outq,
					 cs->cs_tba - (caddr_t)tp->t_outq.c_cf);
				line->l_start(tp);
				break;

			case ZRING_SINT:
				/*
				 * Status line change.  HFC bit is run in
				 * hardware interrupt, to avoid locking
				 * at splzs here.
				 */
				c = ZRING_VALUE(c);
				if ((c ^ cs->cs_rr0) & ZSRR0_DCD) {
					cc = (c & ZSRR0_DCD) != 0;
					if (line->l_modem(tp, cc) == 0)
						zs_modem(cs, cc);
				}
				cs->cs_rr0 = c;
				break;

			default:
				log(LOG_ERR, "zs%d%c: bad ZRING_TYPE (%x)\n",
				    unit >> 1, (unit & 1) + 'a', c);
				break;
			}
		}
		cs->cs_rbget = get;
		goto again;
	}
	return (retval);
}

int
zsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct zs_softc *zi = zs_cd.cd_devs[unit >> 1];
	register struct zs_chanstate *cs = &zi->zi_cs[unit & 1];
	register struct tty *tp = cs->cs_ttyp;
	register int error, s;

	error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		s = splzs();
		cs->cs_preg[5] |= ZSWR5_BREAK;
		cs->cs_creg[5] |= ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
		break;
	case TIOCCBRK:
		s = splzs();
		cs->cs_preg[5] &= ~ZSWR5_BREAK;
		cs->cs_creg[5] &= ~ZSWR5_BREAK;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		splx(s);
		break;
	case TIOCGFLAGS: {
		int bits = 0;

		if (cs->cs_softcar)
			bits |= TIOCFLAG_SOFTCAR;
		if (cs->cs_creg[15] & ZSWR15_DCD_IE)
			bits |= TIOCFLAG_CLOCAL;
		if (cs->cs_creg[3] & ZSWR3_HFC)
			bits |= TIOCFLAG_CRTSCTS;
		*(int *)data = bits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0)
			return (EPERM);

		userbits = *(int *)data;

		/*
		 * can have `local' or `softcar', and `rtscts' or `mdmbuf'
		 # defaulting to software flow control.
		 */
		if (userbits & TIOCFLAG_SOFTCAR && userbits & TIOCFLAG_CLOCAL)
			return(EINVAL);
		if (userbits & TIOCFLAG_MDMBUF)	/* don't support this (yet?) */
			return(ENXIO);

		s = splzs();
		if ((userbits & TIOCFLAG_SOFTCAR) || cs->cs_consio) {
			cs->cs_softcar = 1;	/* turn on softcar */
			cs->cs_preg[15] &= ~ZSWR15_DCD_IE; /* turn off dcd */
			cs->cs_creg[15] &= ~ZSWR15_DCD_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
		} else if (userbits & TIOCFLAG_CLOCAL) {
			cs->cs_softcar = 0; 	/* turn off softcar */
			cs->cs_preg[15] |= ZSWR15_DCD_IE; /* turn on dcd */
			cs->cs_creg[15] |= ZSWR15_DCD_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			tp->t_termios.c_cflag |= CLOCAL;
		}
		if (userbits & TIOCFLAG_CRTSCTS) {
			cs->cs_preg[15] |= ZSWR15_CTS_IE;
			cs->cs_creg[15] |= ZSWR15_CTS_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			cs->cs_preg[3] |= ZSWR3_HFC;
			cs->cs_creg[3] |= ZSWR3_HFC;
			ZS_WRITE(cs->cs_zc, 3, cs->cs_creg[3]);
			tp->t_termios.c_cflag |= CRTSCTS;
		} else {
			/* no mdmbuf, so we must want software flow control */
			cs->cs_preg[15] &= ~ZSWR15_CTS_IE;
			cs->cs_creg[15] &= ~ZSWR15_CTS_IE;
			ZS_WRITE(cs->cs_zc, 15, cs->cs_creg[15]);
			cs->cs_preg[3] &= ~ZSWR3_HFC;
			cs->cs_creg[3] &= ~ZSWR3_HFC;
			ZS_WRITE(cs->cs_zc, 3, cs->cs_creg[3]);
			tp->t_termios.c_cflag &= ~CRTSCTS;
		}
		splx(s);
		break;
	}
	case TIOCSDTR:
		zs_modem(cs, 1);
		break;
	case TIOCCDTR:
		zs_modem(cs, 0);
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
	register struct tty *tp;
{
	register struct zs_chanstate *cs;
	register int s, nch;
	int unit = minor(tp->t_dev);
	struct zs_softc *zi = zs_cd.cd_devs[unit >> 1];

	cs = &zi->zi_cs[unit & 1];
	s = spltty();

	/*
	 * If currently active or delaying, no need to do anything.
	 */
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;

	/*
	 * If there are sleepers, and output has drained below low
	 * water mark, awaken.
	 */
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}

	nch = ndqb(&tp->t_outq, 0);	/* XXX */
	if (nch) {
		register char *p = tp->t_outq.c_cf;

		/* mark busy, enable tx done interrupts, & send first byte */
		tp->t_state |= TS_BUSY;
		(void) splzs();
		cs->cs_preg[1] |= ZSWR1_TIE;
		cs->cs_creg[1] |= ZSWR1_TIE;
		ZS_WRITE(cs->cs_zc, 1, cs->cs_creg[1]);
		cs->cs_zc->zc_data = *p;
		ZS_DELAY();
		cs->cs_tba = p + 1;
		cs->cs_tbc = nch - 1;
	} else {
		/*
		 * Nothing to send, turn off transmit done interrupts.
		 * This is useful if something is doing polled output.
		 */
		(void) splzs();
		cs->cs_preg[1] &= ~ZSWR1_TIE;
		cs->cs_creg[1] &= ~ZSWR1_TIE;
		ZS_WRITE(cs->cs_zc, 1, cs->cs_creg[1]);
	}
out:
	splx(s);
}

/*
 * Stop output, e.g., for ^S or output flush.
 */
int
zsstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct zs_chanstate *cs;
	register int s, unit = minor(tp->t_dev);
	struct zs_softc *zi = zs_cd.cd_devs[unit >> 1];

	cs = &zi->zi_cs[unit & 1];
	s = splzs();
	if (tp->t_state & TS_BUSY) {
		/*
		 * Device is transmitting; must stop it.
		 */
		cs->cs_tbc = 0;
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

/*
 * Set ZS tty parameters from termios.
 *
 * This routine makes use of the fact that only registers
 * 1, 3, 4, 5, 9, 10, 11, 12, 13, 14, and 15 are written.
 */
static int
zsparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	int unit = minor(tp->t_dev);
	struct zs_softc *zi = zs_cd.cd_devs[unit >> 1];
	register struct zs_chanstate *cs = &zi->zi_cs[unit & 1];
	register int tmp, tmp5, cflag, s;

	/*
	 * Because PCLK is only run at 5 MHz, the fastest we
	 * can go is 51200 baud (this corresponds to TC=1).
	 * This is somewhat unfortunate as there is no real
	 * reason we should not be able to handle higher rates.
	 */
	tmp = t->c_ospeed;
	if (tmp < 0 || (t->c_ispeed && t->c_ispeed != tmp))
		return (EINVAL);
	if (tmp == 0) {
		/* stty 0 => drop DTR and RTS */
		zs_modem(cs, 0);
		return (0);
	}
	tmp = BPS_TO_TCONST(PCLK / 16, tmp);
	if (tmp < 2)
		return (EINVAL);

	cflag = t->c_cflag;
	tp->t_ispeed = tp->t_ospeed = TCONST_TO_BPS(PCLK / 16, tmp);
	tp->t_cflag = cflag;

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 */
	s = splzs();
	cs->cs_preg[12] = tmp;
	cs->cs_preg[13] = tmp >> 8;
	cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_TIE | ZSWR1_SIE;
	switch (cflag & CSIZE) {
	case CS5:
		tmp = ZSWR3_RX_5;
		tmp5 = ZSWR5_TX_5;
		break;
	case CS6:
		tmp = ZSWR3_RX_6;
		tmp5 = ZSWR5_TX_6;
		break;
	case CS7:
		tmp = ZSWR3_RX_7;
		tmp5 = ZSWR5_TX_7;
		break;
	case CS8:
	default:
		tmp = ZSWR3_RX_8;
		tmp5 = ZSWR5_TX_8;
		break;
	}

	/*
	 * Output hardware flow control on the chip is horrendous: if
	 * carrier detect drops, the receiver is disabled.  Hence we
	 * can only do this when the carrier is on.
	 */
	tmp |= ZSWR3_RX_ENABLE;
	if (cflag & CCTS_OFLOW) {
		if (cs->cs_zc->zc_csr & ZSRR0_DCD)
			tmp |= ZSWR3_HFC;
		ZS_DELAY();
	}
	cs->cs_preg[3] = tmp;
	cs->cs_preg[5] = tmp5 | ZSWR5_TX_ENABLE | ZSWR5_DTR | ZSWR5_RTS;

	tmp = ZSWR4_CLK_X16 | (cflag & CSTOPB ? ZSWR4_TWOSB : ZSWR4_ONESB);
	if ((cflag & PARODD) == 0)
		tmp |= ZSWR4_EVENP;
	if (cflag & PARENB)
		tmp |= ZSWR4_PARENB;
	cs->cs_preg[4] = tmp;
	cs->cs_preg[9] = ZSWR9_MASTER_IE /*| ZSWR9_NO_VECTOR*/;
	cs->cs_preg[10] = ZSWR10_NRZ;
	cs->cs_preg[11] = ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD;
	cs->cs_preg[14] = ZSWR14_BAUD_FROM_PCLK | ZSWR14_BAUD_ENA;
	cs->cs_preg[15] = ZSWR15_BREAK_IE | ZSWR15_DCD_IE;

	/*
	 * If nothing is being transmitted, set up new current values,
	 * else mark them as pending.
	 */
	if (cs->cs_heldchange == 0) {
		if (cs->cs_ttyp->t_state & TS_BUSY) {
			cs->cs_heldtbc = cs->cs_tbc;
			cs->cs_tbc = 0;
			cs->cs_heldchange = 1;
		} else {
			bcopy((caddr_t)cs->cs_preg, (caddr_t)cs->cs_creg, 16);
			zs_loadchannelregs(cs->cs_zc, cs->cs_creg);
		}
	}
	splx(s);
	return (0);
}

/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
zs_modem(cs, onoff)
	struct zs_chanstate *cs;
	int onoff;
{
	int s, bis, and;

	if (onoff) {
		bis = ZSWR5_DTR | ZSWR5_RTS;
		and = ~0;
	} else {
		bis = 0;
		and = ~(ZSWR5_DTR | ZSWR5_RTS);
	}
	s = splzs();
	cs->cs_preg[5] = (cs->cs_preg[5] | bis) & and;
	if (cs->cs_heldchange == 0) {
		if (cs->cs_ttyp->t_state & TS_BUSY) {
			cs->cs_heldtbc = cs->cs_tbc;
			cs->cs_tbc = 0;
			cs->cs_heldchange = 1;
		} else {
			cs->cs_creg[5] = (cs->cs_creg[5] | bis) & and;
			ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
		}
	}
	splx(s);
}

/*
 * Hardware flow (RTS) control.
 */
static int
zshwiflow(tp, flag)
	struct tty *tp;
	int flag;
{
	int unit = minor(tp->t_dev);
	struct zs_softc *zi = zs_cd.cd_devs[unit >> 1];
	register struct zs_chanstate *cs = &zi->zi_cs[unit & 1];
	int s;

#if 0
       printf ("zshwiflow %d\n", flag);
#endif
	s = splzs();
	if (flag) {
		cs->cs_preg[5] &= ~ZSWR5_RTS;
		cs->cs_creg[5] &= ~ZSWR5_RTS;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
	} else {
		cs->cs_preg[5] |= ZSWR5_RTS;
		cs->cs_creg[5] |= ZSWR5_RTS;
		ZS_WRITE(cs->cs_zc, 5, cs->cs_creg[5]);
	}
	splx(s);
	return 1;
}

/*
 * Write the given register set to the given zs channel in the proper order.
 * The channel must not be transmitting at the time.  The receiver will
 * be disabled for the time it takes to write all the registers.
 */
static void
zs_loadchannelregs(zc, reg)
	volatile struct zschan *zc;
	u_char *reg;
{
	int i;

	zc->zc_csr = ZSM_RESET_ERR;	/* reset error condition */
	ZS_DELAY();
	i = zc->zc_data;		/* drain fifo */
	ZS_DELAY();
	i = zc->zc_data;
	ZS_DELAY();
	i = zc->zc_data;
	ZS_DELAY();
	ZS_WRITE(zc, 4, reg[4]);
	ZS_WRITE(zc, 10, reg[10]);
	ZS_WRITE(zc, 3, reg[3] & ~ZSWR3_RX_ENABLE);
	ZS_WRITE(zc, 5, reg[5] & ~ZSWR5_TX_ENABLE);
	ZS_WRITE(zc, 1, reg[1]);
	ZS_WRITE(zc, 9, reg[9]);
	ZS_WRITE(zc, 11, reg[11]);
	ZS_WRITE(zc, 12, reg[12]);
	ZS_WRITE(zc, 13, reg[13]);
	ZS_WRITE(zc, 14, reg[14]);
	ZS_WRITE(zc, 15, reg[15]);
	ZS_WRITE(zc, 3, reg[3]);
	ZS_WRITE(zc, 5, reg[5]);
}

#ifdef x68k
void
zs_msmodem(onoff)
	int onoff;
{
	if (zsms != NULL) {
		zs_modem(zsms, onoff);
		while(!(mfp.tsr & MFP_TSR_BE))
			/* XXX wait */ ;
		mfp.udr = 0x40 | (onoff ? 0 : 1);
	}
}
#endif

#ifdef KGDB
/*
 * Get a character from the given kgdb channel.  Called at splhigh().
 */
static int
zs_kgdb_getc(arg)
	void *arg;
{
	register volatile struct zschan *zc = (volatile struct zschan *)arg;

	while ((zc->zc_csr & ZSRR0_RX_READY) == 0)
		ZS_DELAY();
	return (zc->zc_data);
}

/*
 * Put a character to the given kgdb channel.  Called at splhigh().
 */
static void
zs_kgdb_putc(arg, c)
	void *arg;
	int c;
{
	register volatile struct zschan *zc = (volatile struct zschan *)arg;

	while ((zc->zc_csr & ZSRR0_TX_READY) == 0)
		ZS_DELAY();
	zc->zc_data = c;
	ZS_DELAY();
}

/*
 * Set up for kgdb; called at boot time before configuration.
 * KGDB interrupts will be enabled later when zs0 is configured.
 */
void
zs_kgdb_init()
{
	volatile struct zsdevice *addr;
	volatile struct zschan *zc;
	int unit, zs;

	if (major(kgdb_dev) != ZSMAJOR)
		return;
	unit = minor(kgdb_dev);
	zs = unit >> 1;
	if ((addr = zsaddr[zs]) == NULL)
		addr = zsaddr[zs] = findzs(zs);
	unit &= 1;
	zc = unit == 0 ? &addr->zs_chan[ZS_CHAN_A] : &addr->zs_chan[ZS_CHAN_B];
	zs_kgdb_savedspeed = zs_getspeed(zc);
	printf("zs_kgdb_init: attaching zs%d%c at %d baud\n",
	    zs, unit + 'a', kgdb_rate);
	zs_reset(zc, 1, kgdb_rate);
	kgdb_attach(zs_kgdb_getc, zs_kgdb_putc, (void *)zc);
}
#endif /* KGDB */
#endif
