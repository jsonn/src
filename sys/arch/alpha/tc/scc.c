/* $NetBSD: scc.c,v 1.57.2.3 2001/10/10 11:55:48 fvdl Exp $ */

/*
 * Copyright (c) 1991,1990,1989,1994,1995,1996 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)scc.c	8.2 (Berkeley) 11/30/93
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: scc.c,v 1.57.2.3 2001/10/10 11:55:48 fvdl Exp $");

#include "opt_ddb.h"
#include "opt_dec_3000_300.h"

/*
 * Intel 82530 dual usart chip driver. Supports the serial port(s) on the
 * Personal DECstation 5000/xx and DECstation 5000/1xx, plus the keyboard
 * and mouse on the 5000/1xx. (Don't ask me where the A channel signals
 * are on the 5000/xx.)
 *
 * See: Intel MicroCommunications Handbook, Section 2, pg. 155-173, 1992.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/vnode.h>

#include <miscfs/specfs/specdev.h>

#include <dev/cons.h>

#include <dev/ic/z8530reg.h>
#include <alpha/tc/sccreg.h>
#include <alpha/tc/sccvar.h>

#include <machine/rpb.h>
#include <machine/conf.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicreg.h>
#include <dev/tc/ioasicvar.h>

#undef	SCCDEV
#define	SCCDEV		15			/* XXX */

#define raster_console() 1	/* Treat test for cn_screen as true */
#define CONSOLE_ON_UNIT(unit) 0	/* No raster console on Alphas */

#define	NSCCLINE	(NSCC*2)
#define	SCCUNIT(dev)	(minor(dev) >> 1)
#define	SCCLINE(dev)	(minor(dev) & 0x1)

#ifdef DEBUG
int	debugChar;
#endif

struct scc_softc {
	struct device sc_dv;
	struct pdma scc_pdma[2];
	struct {
		u_char	wr1;
		u_char	wr3;
		u_char	wr4;
		u_char	wr5;
		u_char	wr14;
	} scc_wreg[2];
	struct tty *scc_tty[2];
	int	scc_softCAR;

	int scc_flags[2];
#define SCC_CHAN_NEEDSDELAY	0x01	/* sw must delay 1.6us between output*/
#define SCC_CHAN_NOMODEM	0x02	/* don't touch modem ctl lines (may
					   be left floating or x-wired */
#define SCC_CHAN_MODEM_CROSSED	0x04	/* modem lines wired to	other channel*/
#define SCC_CHAN_KBDLINE	0x08	/* XXX special-case keyboard lines */
};

/*
 * BRG formula is:
 *				ClockFrequency
 *	BRGconstant =	---------------------------  -  2
 *			2 * BaudRate * ClockDivider
 *
 * Speed selections with Pclk=7.3728Mhz, clock x16
 */
struct speedtab sccspeedtab[] = {
	{ 0,		0,	},
	{ 50,		4606,	},
	{ 75,		3070,	},
	{ 110,		2093,	},
	{ 134.5,	1711,	},
	{ 150,		1534,	},
	{ 200,		1150,	},
	{ 300,		766,	},
	{ 600,		382,	},
	{ 1200,		190,	},
	{ 1800,		126,	},
	{ 2400,		94,	},
	{ 4800,		46,	},
	{ 7200,		30,	},	/* non-POSIX */
	{ 9600,		22,	},
	{ 14400,	14,	},	/* non-POSIX */
	{ 19200,	10,	},
	{ 28800,	6,	},	/* non-POSIX */
	{ 38400,	4,	},	/* non-POSIX */
	{ 57600,	2,	},	/* non-POSIX */
	{ -1,		-1,	},
};

#ifndef	PORTSELECTOR
#define	ISPEED	TTYDEF_SPEED
#define	LFLAG	TTYDEF_LFLAG
#else
#define	ISPEED	B4800
#define	LFLAG	(TTYDEF_LFLAG & ~ECHO)
#endif

/* Definition of the driver for autoconfig. */
static int	sccmatch  __P((struct device *parent, struct cfdata *cf,
		    void *aux));
static void	sccattach __P((struct device *parent, struct device *self,
		    void *aux));

struct cfattach scc_ca = {
	sizeof (struct scc_softc), sccmatch, sccattach,
};

extern struct cfdriver scc_cd;

int		sccGetc __P((dev_t));
void		sccPutc __P((dev_t, int));
void		sccPollc __P((dev_t, int));
int		sccparam __P((struct tty *, struct termios *));
void		sccstart __P((struct tty *));

static int	sccmctl __P((struct scc_softc *, int, int, int));
static int	cold_sccparam __P((struct tty *, struct termios *,
		    struct scc_softc *sc, int line));

#ifdef SCC_DEBUG
static void	rr __P((char *, scc_regmap_t *));
#endif
static void	scc_modem_intr __P((dev_t));
static void	sccreset __P((struct scc_softc *));

int	sccintr __P((void *));
void	scc_alphaintr __P((int));

/*
 * console variables, for using serial console while still cold and
 * autoconfig has not attached the scc device.
 */
scc_regmap_t *scc_cons_addr = 0;
static struct consdev scccons = {
	NULL, NULL, sccGetc, sccPutc, sccPollc, NULL, NODEV, 0
};

/*
 * Test to see if device is present.
 * Return true if found.
 */
int
sccmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	extern struct cfdriver ioasic_cd;		/* XXX */
	struct ioasicdev_attach_args *d = aux;
	void *sccaddr;

	if (parent->dv_cfdata->cf_driver != &ioasic_cd) {
#ifdef DIAGNOSTIC
		printf("Cannot attach scc on %s\n", parent->dv_xname);
#endif
		return (0);
	}

	/* Make sure that we're looking for this type of device. */
	if ((strncmp(d->iada_modname, "z8530   ", TC_ROM_LLEN) != 0) &&
	    (strncmp(d->iada_modname, "scc", TC_ROM_LLEN)!= 0))
		return (0);

	/*
	 * Check user-specified offset against the ioasic offset.
	 * Allow it to be wildcarded.
	 */
	if (cf->cf_loc[IOASICCF_OFFSET] != IOASICCF_OFFSET_DEFAULT &&
	    cf->cf_loc[IOASICCF_OFFSET] != d->iada_offset)
		return (0);

	/* Get the address, and check it for validity. */
	sccaddr = (void *)d->iada_addr;
#ifdef SPARSE
	sccaddr = (void *)TC_DENSE_TO_SPARSE((tc_addr_t)sccaddr);
#endif
	if (badaddr(sccaddr, 2))
		return (0);

	return (1);
}

/*
 * Enable ioasic SCC interrupts and scc DMA engine interrupts.
 * XXX does not really belong here.
 */
void
scc_alphaintr(onoff)
	int onoff;
{
	if (onoff) {
		*(volatile u_int *)(ioasic_base + IOASIC_IMSK) |=
		    IOASIC_INTR_SCC_1 | IOASIC_INTR_SCC_0;
#if !defined(DEC_3000_300) && defined(SCC_DMA)
		*(volatile u_int *)(ioasic_base + IOASIC_CSR) |=
		    IOASIC_CSR_DMAEN_T1 | IOASIC_CSR_DMAEN_R1 |
		    IOASIC_CSR_DMAEN_T2 | IOASIC_CSR_DMAEN_R2;
#endif
	} else {
		*(volatile u_int *)(ioasic_base + IOASIC_IMSK) &=
		    ~(IOASIC_INTR_SCC_1 | IOASIC_INTR_SCC_0);
#if !defined(DEC_3000_300) && defined(SCC_DMA)
		*(volatile u_int *)(ioasic_base + IOASIC_CSR) &=
		    ~(IOASIC_CSR_DMAEN_T1 | IOASIC_CSR_DMAEN_R1 |
		    IOASIC_CSR_DMAEN_T2 | IOASIC_CSR_DMAEN_R2);
#endif
	}
	tc_mb();
}

void
sccattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct scc_softc *sc = (struct scc_softc *)self;
	struct ioasicdev_attach_args *d = aux;
	struct pdma *pdp;
	struct tty *tp;
	void *sccaddr;
	int cntr;
	struct termios cterm;
	struct tty ctty;
	int s;
	int unit;

	unit = sc->sc_dv.dv_unit;

	/* Get the address, and check it for validity. */
	sccaddr = (void *)d->iada_addr;
#ifdef SPARSE
	sccaddr = (void *)TC_DENSE_TO_SPARSE((tc_addr_t)sccaddr);
#endif

	/* Register the interrupt handler. */
	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_TTY,
	    sccintr, (void *)sc);

	/*
	 * For a remote console, wait a while for previous output to
	 * complete.
	 */
	if ((cputype == ST_DEC_3000_500 && sc->sc_dv.dv_unit == 1) ||
	    (cputype == ST_DEC_3000_300 && sc->sc_dv.dv_unit == 0))
		DELAY(10000);
	pdp = &sc->scc_pdma[0];

	/* init pseudo DMA structures */
	for (cntr = 0; cntr < 2; cntr++) {
		pdp->p_addr = (void *)sccaddr;
		tp = sc->scc_tty[cntr] = ttymalloc();
		if (cntr == 0)
			tty_attach(tp);
		pdp->p_arg = (long)tp;
		pdp->p_fcn = (void (*)__P((struct tty*)))0;
		pdp++;
	}
	/* What's the warning here? Defaulting to softCAR on line 2? */
	sc->scc_softCAR = sc->sc_dv.dv_cfdata->cf_flags | 0x2;	/* XXX */

	/* reset chip, initialize  register-copies in softc */
	sccreset(sc);

	/*
	 * Special handling for consoles.
	 */
	if (1 /* SCCUNIT(cn_tab.cn_dev) == sc->sc_dv.dv_unit */) {
		s = spltty();
		cterm.c_cflag = (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8;
		cterm.c_ospeed = cterm.c_ispeed = 9600;
		(void) cold_sccparam(&ctty, &cterm, sc,
		    SCCLINE((sc->sc_dv.dv_unit == 0) ?
			    SCCCOMM2_PORT : SCCCOMM3_PORT));
		DELAY(1000);
		splx(s);
	}

	/*
	 * XXX
	 * Unit 1 is the remote console, wire it up now.
	 */
	if ((cputype == ST_DEC_3000_500 && sc->sc_dv.dv_unit == 1) ||
	    (cputype == ST_DEC_3000_300 && sc->sc_dv.dv_unit == 0)) {
		if (alpha_donot_kludge_scc)
			printf("\nSWITCHING TO SERIAL CONSOLE!\n");
		cn_tab = &scccons;
		cn_tab->cn_dev = makedev(SCCDEV, sc->sc_dv.dv_unit * 2);

		printf("%s console\n", alpha_donot_kludge_scc ? "\n***" : ":");

		/* wire carrier for console. */
		sc->scc_softCAR |= SCCLINE(cn_tab->cn_dev);
	} else
		printf("\n");
}

/*
 * Reset the chip.
 */
static void
sccreset(sc)
	register struct scc_softc *sc;
{
	register scc_regmap_t *regs;
	register u_char val;

	regs = (scc_regmap_t *)sc->scc_pdma[0].p_addr;
	/*
	 * Chip once-only initialization
	 *
	 * NOTE: The wiring we assume is the one on the 3min:
	 *
	 *	out	A-TxD	-->	TxD	keybd or mouse
	 *	in	A-RxD	-->	RxD	keybd or mouse
	 *	out	A-DTR~	-->	DTR	comm
	 *	out	A-RTS~	-->	RTS	comm
	 *	in	A-CTS~	-->	SI	comm
	 *	in	A-DCD~	-->	RI	comm
	 *	in	A-SYNCH~-->	DSR	comm
	 *	out	B-TxD	-->	TxD	comm
	 *	in	B-RxD	-->	RxD	comm
	 *	in	B-RxC	-->	TRxCB	comm
	 *	in	B-TxC	-->	RTxCB	comm
	 *	out	B-RTS~	-->	SS	comm
	 *	in	B-CTS~	-->	CTS	comm
	 *	in	B-DCD~	-->	CD	comm
	 */
	SCC_INIT_REG(regs, SCC_CHANNEL_A);
	SCC_INIT_REG(regs, SCC_CHANNEL_B);

	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, ZSWR9_HARD_RESET);
	DELAY(50000);	/*enough ? */
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, 0);

	/* program the interrupt vector */
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, ZSWR_IVEC, 0xf0);
	SCC_WRITE_REG(regs, SCC_CHANNEL_B, ZSWR_IVEC, 0xf0);
	SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR9, ZSWR9_VECTOR_INCL_STAT);

	/* receive parameters and control */
	sc->scc_wreg[SCC_CHANNEL_A].wr3 = 0;
	sc->scc_wreg[SCC_CHANNEL_B].wr3 = 0;

	/* timing base defaults */
	sc->scc_wreg[SCC_CHANNEL_A].wr4 = ZSWR4_CLK_X16;
	sc->scc_wreg[SCC_CHANNEL_B].wr4 = ZSWR4_CLK_X16;

	/* enable DTR, RTS and SS */
	sc->scc_wreg[SCC_CHANNEL_B].wr5 = 0;
	sc->scc_wreg[SCC_CHANNEL_A].wr5 = ZSWR5_RTS | ZSWR5_DTR;

	/* baud rates */
	val = ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK;
	sc->scc_wreg[SCC_CHANNEL_B].wr14 = val;
	sc->scc_wreg[SCC_CHANNEL_A].wr14 = val;

	/* interrupt conditions */
	val =	ZSWR1_RIE | ZSWR1_PE_SC | ZSWR1_SIE | ZSWR1_TIE;
	sc->scc_wreg[SCC_CHANNEL_A].wr1 = val;
	sc->scc_wreg[SCC_CHANNEL_B].wr1 = val;
}

int
sccopen(devvp, flag, mode, p)
	struct vnode *devvp;
	int flag, mode;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	register int unit, line;
	int s, error = 0;
	int firstopen = 0;
	dev_t rdev;

	rdev = vdev_rdev(devvp);

	unit = SCCUNIT(rdev);
	if (unit >= scc_cd.cd_ndevs)
		return (ENXIO);
	sc = scc_cd.cd_devs[unit];
	if (!sc)
		return (ENXIO);

	vdev_setprivdata(devvp, sc);

	line = SCCLINE(rdev);
	if (sc->scc_pdma[line].p_addr == NULL)
		return (ENXIO);
	tp = sc->scc_tty[line];
	if (tp == NULL) {
		tp = sc->scc_tty[line] = ttymalloc();
		tty_attach(tp);
	}
	tp->t_oproc = sccstart;
	tp->t_param = sccparam;
	tp->t_devvp = devvp;
	if ((tp->t_state & TS_ISOPEN) == 0 && tp->t_wopen == 0) {
		ttychars(tp);
		firstopen = 1;
#ifndef PORTSELECTOR
		if (tp->t_ispeed == 0) {
#endif
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = LFLAG;
			tp->t_ispeed = tp->t_ospeed = ISPEED;
#ifdef PORTSELECTOR
			tp->t_cflag |= HUPCL;
#else
		}
#endif
		(void) sccparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && curproc->p_ucred->cr_uid != 0)
		return (EBUSY);
	(void) sccmctl(sc, SCCLINE(rdev), DML_DTR, DMSET);
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	    !(tp->t_state & TS_CARR_ON)) {
		tp->t_wopen++;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq, TTIPRI | PCATCH,
		    ttopen, 0);
		tp->t_wopen--;
		if (error != 0)
			break;
	}
	splx(s);
	if (error)
		return (error);
	error = (*tp->t_linesw->l_open)(devvp, tp);

	return (error);
}

/*ARGSUSED*/
int
sccclose(devvp, flag, mode, p)
	struct vnode *devvp;
	int flag, mode;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	register int line;
	dev_t rdev;

	rdev = vdev_rdev(devvp);
	sc = vdev_privdata(devvp);

	line = SCCLINE(rdev);
	tp = sc->scc_tty[line];
	if (sc->scc_wreg[line].wr5 & ZSWR5_BREAK) {
		sc->scc_wreg[line].wr5 &= ~ZSWR5_BREAK;
		ttyoutput(0, tp);
	}
	(*tp->t_linesw->l_close)(tp, flag);
	if ((tp->t_cflag & HUPCL) || tp->t_wopen ||
	    !(tp->t_state & TS_ISOPEN))
		(void) sccmctl(sc, line, 0, DMSET);
	return (ttyclose(tp));
}

int
sccread(devvp, uio, flag)
	struct vnode *devvp;
	struct uio *uio;
	int flag;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = vdev_privdata(devvp);

	tp = sc->scc_tty[SCCLINE(vdev_rdev(devvp))];
	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
sccwrite(devvp, uio, flag)
	struct vnode *devvp;
	struct uio *uio;
	int flag;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = vdev_privdata(devvp);
	tp = sc->scc_tty[SCCLINE(vdev_rdev(devvp))];
	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
sccpoll(devvp, events, p)
	struct vnode *devvp;
	int events;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = vdev_privdata(devvp);
	tp = sc->scc_tty[SCCLINE(vdev_rdev(devvp))];
	return ((*tp->t_linesw->l_poll)(tp, events, p));
}

struct tty *
scctty(devvp)
	struct vnode *devvp;
{
	register struct scc_softc *sc;
	register struct tty *tp;

	sc = vdev_privdata(devvp);

	tp = sc->scc_tty[SCCLINE(vdev_rdev(devvp))];
	return (tp);
}

/*ARGSUSED*/
int
sccioctl(devvp, cmd, data, flag, p)
	struct vnode *devvp;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct scc_softc *sc;
	register struct tty *tp;
	int error, line;

	sc = vdev_privdata(devvp);
	line = SCCLINE(vdev_rdev(devvp));
	tp = sc->scc_tty[line];
	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		sc->scc_wreg[line].wr5 |= ZSWR5_BREAK;
		ttyoutput(0, tp);
		break;

	case TIOCCBRK:
		sc->scc_wreg[line].wr5 &= ~ZSWR5_BREAK;
		ttyoutput(0, tp);
		break;

	case TIOCSDTR:
		(void) sccmctl(sc, line, DML_DTR|DML_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) sccmctl(sc, line, DML_DTR|DML_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) sccmctl(sc, line, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) sccmctl(sc, line, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) sccmctl(sc, line, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = sccmctl(sc, line, 0, DMGET);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}



/*
 * Set line parameters --  tty t_param entry point.
 */
int
sccparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register struct scc_softc *sc;

	sc = vdev_privdata(tp->t_devvp);

	/* Extract the softc and call cold_sccparam to do all the work. */
	return cold_sccparam(tp, t, sc, SCCLINE(vdev_rdev(tp->t_devvp)));
}


/*
 * Do what sccparam() (t_param entry point) does, but callable when cold.
 */
static int
cold_sccparam(tp, t, sc, line)
	register struct tty *tp;
	register struct termios *t;
	register struct scc_softc *sc;
	register int line;
{
	register scc_regmap_t *regs;
	register u_char value, wvalue;
	register int cflag = t->c_cflag;
	int ospeed;

	/* Check arguments */
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);
	ospeed = ttspeedtab(t->c_ospeed, sccspeedtab);
	if (ospeed < 0)
		return (EINVAL);
	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	/*
	 * Handle console specially.
	 */
	{
		cflag = CS8;
		ospeed = ttspeedtab(9600, sccspeedtab);
	}
	if (ospeed == 0) {
		(void) sccmctl(sc, line, 0, DMSET);	/* hang up line */
		return (0);
	}

	regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;

	/*
	 * pmax driver used to reset the SCC here. That reset causes the
	 * other channel on the SCC to drop outpur chars: at least that's
	 * what CGD reports for the Alpha.  It's a bug.
	 */
#if 0
	/* reset line */
	if (line == SCC_CHANNEL_A)
		value = ZSWR9_A_RESET;
	else
		value = ZSWR9_B_RESET;
	SCC_WRITE_REG(regs, line, SCC_WR9, value);
	DELAY(25);
#endif

	/* stop bits, normally 1 */
	value = sc->scc_wreg[line].wr4 & 0xf0;
	if (cflag & CSTOPB)
		value |= ZSWR4_TWOSB;
	else
		value |= ZSWR4_ONESB;
	if ((cflag & PARODD) == 0)
		value |= ZSWR4_EVENP;
	if (cflag & PARENB)
		value |= ZSWR4_PARENB;

	/* set it now, remember it must be first after reset */
	sc->scc_wreg[line].wr4 = value;
	SCC_WRITE_REG(regs, line, SCC_WR4, value);

	/* vector again */
	SCC_WRITE_REG(regs, line, ZSWR_IVEC, 0xf0);

	/* clear break, keep rts dtr */
	wvalue = sc->scc_wreg[line].wr5 & (ZSWR5_DTR|ZSWR5_RTS);
	switch (cflag & CSIZE) {
	case CS5:
		value = ZSWR3_RX_5;
		wvalue |= ZSWR5_TX_5;
		break;
	case CS6:
		value = ZSWR3_RX_6;
		wvalue |= ZSWR5_TX_6;
		break;
	case CS7:
		value = ZSWR3_RX_7;
		wvalue |= ZSWR5_TX_7;
		break;
	case CS8:
	default:
		value = ZSWR3_RX_8;
		wvalue |= ZSWR5_TX_8;
	};
	sc->scc_wreg[line].wr3 = value;
	SCC_WRITE_REG(regs, line, SCC_WR3, value);

	sc->scc_wreg[line].wr5 = wvalue;
	SCC_WRITE_REG(regs, line, SCC_WR5, wvalue);

	/*
	 * XXX Does the SCC chip require us to refresh the WR5 register
	 * for the other channel after writing the other, or not?
	 */
#ifdef notdef
	/* XXX */
	{
	int otherline = (line + 1) & 1;
	SCC_WRITE_REG(regs, otherline, SCC_WR5, sc->scc_wreg[otherline].wr5);
	}
#endif

	SCC_WRITE_REG(regs, line, ZSWR_SYNCLO, 0);
	SCC_WRITE_REG(regs, line, ZSWR_SYNCHI, 0);
	SCC_WRITE_REG(regs, line, SCC_WR9, ZSWR9_VECTOR_INCL_STAT);
	SCC_WRITE_REG(regs, line, SCC_WR10, 0);
	value = ZSWR11_RXCLK_BAUD | ZSWR11_TXCLK_BAUD |
		ZSWR11_TRXC_OUT_ENA | ZSWR11_TRXC_BAUD;
	SCC_WRITE_REG(regs, line, SCC_WR11, value);
	SCC_SET_TIMING_BASE(regs, line, ospeed);
	value = sc->scc_wreg[line].wr14;
	SCC_WRITE_REG(regs, line, SCC_WR14, value);

	if (sc->sc_dv.dv_unit == 1) {
		/* On unit one, on the flamingo, modem control is floating! */
		value = ZSWR15_BREAK_IE;
	} else
	{
		value = ZSWR15_BREAK_IE | ZSWR15_CTS_IE | ZSWR15_DCD_IE;
	}
	SCC_WRITE_REG(regs, line, SCC_WR15, value);

	/* and now the enables */
	value = sc->scc_wreg[line].wr3 | ZSWR3_RX_ENABLE;
	SCC_WRITE_REG(regs, line, SCC_WR3, value);
	value = sc->scc_wreg[line].wr5 | ZSWR5_TX_ENABLE;
	sc->scc_wreg[line].wr5 = value;
	SCC_WRITE_REG(regs, line, SCC_WR5, value);

	/* master inter enable */
	value = ZSWR9_MASTER_IE | ZSWR9_VECTOR_INCL_STAT;
	SCC_WRITE_REG(regs, line, SCC_WR9, value);
	SCC_WRITE_REG(regs, line, SCC_WR1, sc->scc_wreg[line].wr1);
	tc_mb();

	scc_alphaintr(1);			/* XXX XXX XXX */

	return (0);
}


/*
 * Check for interrupts from all devices.
 */
int
sccintr(xxxsc)
	void *xxxsc;
{
	register struct scc_softc *sc = (struct scc_softc *)xxxsc;
	register int unit = (long)sc->sc_dv.dv_unit;
	register scc_regmap_t *regs;
	register struct tty *tp;
	register struct pdma *dp;
	register int cc, chan, rr1, rr2, rr3;
	int overrun = 0;

	rr1 = 0;		/* shut up gcc -Wall */
	regs = (scc_regmap_t *)sc->scc_pdma[0].p_addr;
	unit <<= 1;
	for (;;) {
	    SCC_READ_REG(regs, SCC_CHANNEL_B, ZSRR_IVEC, rr2);
	    rr2 = SCC_RR2_STATUS(rr2);
	    /* are we done yet ? */
	    if (rr2 == 6) {	/* strange, distinguished value */
		SCC_READ_REG(regs, SCC_CHANNEL_A, ZSRR_IPEND, rr3);
		if (rr3 == 0)
			return 1;
	    }

	    SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_RR0, ZSWR0_CLR_INTR);
	    if ((rr2 == SCC_RR2_A_XMIT_DONE) || (rr2 == SCC_RR2_B_XMIT_DONE)) {
		chan = (rr2 == SCC_RR2_A_XMIT_DONE) ?
			SCC_CHANNEL_A : SCC_CHANNEL_B;
		tp = sc->scc_tty[chan];
		dp = &sc->scc_pdma[chan];
		if (dp->p_mem < dp->p_end) {
			SCC_WRITE_DATA(regs, chan, *dp->p_mem++);
			tc_mb();
		} else {
			tp->t_state &= ~TS_BUSY;
			if (tp->t_state & TS_FLUSH)
				tp->t_state &= ~TS_FLUSH;
			else {
				ndflush(&tp->t_outq, dp->p_mem -
					(caddr_t) tp->t_outq.c_cf);
				dp->p_end = dp->p_mem = tp->t_outq.c_cf;
			}
			(*tp->t_linesw->l_start)(tp);
			if (tp->t_outq.c_cc == 0 || !(tp->t_state & TS_BUSY)) {
				SCC_READ_REG(regs, chan, SCC_RR15, cc);
				cc &= ~ZSWR15_TXUEOM_IE;
				SCC_WRITE_REG(regs, chan, SCC_WR15, cc);
				cc = sc->scc_wreg[chan].wr1 & ~ZSWR1_TIE;
				SCC_WRITE_REG(regs, chan, SCC_WR1, cc);
				sc->scc_wreg[chan].wr1 = cc;
				tc_mb();
			}
		}
	    } else if (rr2 == SCC_RR2_A_RECV_DONE ||
		rr2 == SCC_RR2_B_RECV_DONE || rr2 == SCC_RR2_A_RECV_SPECIAL ||
		rr2 == SCC_RR2_B_RECV_SPECIAL) {
		if (rr2 == SCC_RR2_A_RECV_DONE || rr2 == SCC_RR2_A_RECV_SPECIAL)
			chan = SCC_CHANNEL_A;
		else
			chan = SCC_CHANNEL_B;
		tp = sc->scc_tty[chan];
		SCC_READ_DATA(regs, chan, cc);
		if (rr2 == SCC_RR2_A_RECV_SPECIAL ||
			rr2 == SCC_RR2_B_RECV_SPECIAL) {
			SCC_READ_REG(regs, chan, SCC_RR1, rr1);
			SCC_WRITE_REG(regs, chan, SCC_RR0, ZSWR0_RESET_ERRORS);
			if ((rr1 & ZSRR1_DO) && overrun == 0) {
				log(LOG_WARNING, "scc%d,%d: silo overflow\n",
					unit >> 1, chan);
				overrun = 1;
			}
		}

		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
#ifdef PORTSELECTOR
			if (tp->t_wopen == 0)
#endif
				continue;
		}
		if (rr2 == SCC_RR2_A_RECV_SPECIAL ||
			rr2 == SCC_RR2_B_RECV_SPECIAL) {
			if (rr1 & ZSRR1_PE)
				cc |= TTY_PE;
			if (rr1 & ZSRR1_FE)
				cc |= TTY_FE;
		}
		(*tp->t_linesw->l_rint)(cc, tp);
	    } else if ((rr2 == SCC_RR2_A_EXT_STATUS) || (rr2 == SCC_RR2_B_EXT_STATUS)) {
		chan = (rr2 == SCC_RR2_A_EXT_STATUS) ?
		    SCC_CHANNEL_A : SCC_CHANNEL_B;
		SCC_WRITE_REG(regs, chan, SCC_RR0, ZSWR0_RESET_STATUS);
		scc_modem_intr(unit | chan);
	    }
	}
}

void
sccstart(tp)
	register struct tty *tp;
{
	register struct pdma *dp;
	register scc_regmap_t *regs;
	register struct scc_softc *sc;
	register int cc, chan;
	u_char temp;
	int s, sendone;
	dev_t rdev;

	rdev = vdev_rdev(tp->t_devvp);
	sc = vdev_privdata(tp->t_devvp);
	dp = &sc->scc_pdma[SCCLINE(rdev)];
	regs = (scc_regmap_t *)dp->p_addr;
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc == 0)
		goto out;
	cc = ndqb(&tp->t_outq, 0);

	tp->t_state |= TS_BUSY;
	dp->p_end = dp->p_mem = tp->t_outq.c_cf;
	dp->p_end += cc;

	/*
	 * Enable transmission and send the first char, as required.
	 */
	chan = SCCLINE(rdev);
	SCC_READ_REG(regs, chan, SCC_RR0, temp);
	sendone = (temp & ZSRR0_TX_READY);
	SCC_READ_REG(regs, chan, SCC_RR15, temp);
	temp |= ZSWR15_TXUEOM_IE;
	SCC_WRITE_REG(regs, chan, SCC_WR15, temp);
	temp = sc->scc_wreg[chan].wr1 | ZSWR1_TIE;
	SCC_WRITE_REG(regs, chan, SCC_WR1, temp);
	sc->scc_wreg[chan].wr1 = temp;
	if (sendone) {
#ifdef DIAGNOSTIC
		if (cc == 0)
			panic("sccstart: No chars");
#endif
		SCC_WRITE_DATA(regs, chan, *dp->p_mem++);
	}
	tc_mb();
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
void
sccstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct pdma *dp;
	register struct scc_softc *sc;
	register int s;
	dev_t rdev;

	sc = vdev_privdata(tp->t_devvp);
	rdev = vdev_rdev(tp->t_devvp);

	dp = &sc->scc_pdma[SCCLINE(rdev)];
	s = spltty();
	if (tp->t_state & TS_BUSY) {
		dp->p_end = dp->p_mem;
		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

int
sccmctl(sc, line, bits, how)
	struct scc_softc *sc;
	int line, bits, how;
{
	register scc_regmap_t *regs;
	register int mbits;
	register u_char value;
	int s;

	regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	s = spltty();
	/*
	 * only channel B has modem control, however the DTR and RTS
	 * pins on the comm port are wired to the DTR and RTS A channel
	 * signals.
	 */
	mbits = DML_DTR | DML_DSR | DML_CAR;
	if (line == SCC_CHANNEL_B) {
		if (sc->scc_wreg[SCC_CHANNEL_A].wr5 & ZSWR5_DTR)
			mbits = DML_DTR | DML_DSR;
		else
			mbits = 0;
		SCC_READ_REG_ZERO(regs, SCC_CHANNEL_B, value);
		if (value & ZSRR0_DCD)
			mbits |= DML_CAR;
	}
	switch (how) {
	case DMSET:
		mbits = bits;
		break;

	case DMBIS:
		mbits |= bits;
		break;

	case DMBIC:
		mbits &= ~bits;
		break;

	case DMGET:
		(void) splx(s);
		return (mbits);
	}
	if (line == SCC_CHANNEL_B) {
		if (mbits & DML_DTR)
			sc->scc_wreg[SCC_CHANNEL_A].wr5 |= ZSWR5_DTR;
		else
			sc->scc_wreg[SCC_CHANNEL_A].wr5 &= ~ZSWR5_DTR;
		SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR5,
			sc->scc_wreg[SCC_CHANNEL_A].wr5);
	}
	if ((mbits & DML_DTR) || (sc->scc_softCAR & (1 << line)))
		sc->scc_tty[line]->t_state |= TS_CARR_ON;
	(void) splx(s);
	return (mbits);
}

/*
 * Check for carrier transition.
 */
static void
scc_modem_intr(dev)
	dev_t dev;
{
	register scc_regmap_t *regs;
	register struct scc_softc *sc;
	register struct tty *tp;
	register int car, chan;
	register u_char value;
	int s;

	chan = SCCLINE(dev);
	sc = scc_cd.cd_devs[SCCUNIT(dev)];
	tp = sc->scc_tty[chan];
	regs = (scc_regmap_t *)sc->scc_pdma[chan].p_addr;
	if (chan == SCC_CHANNEL_A)
		return;
	s = spltty();
	if (sc->scc_softCAR & (1 << chan))
		car = 1;
	else {
		SCC_READ_REG_ZERO(regs, chan, value);
		car = value & ZSRR0_DCD;
	}

	/* Break on serial console drops into the debugger */
	if ((value & ZSRR0_BREAK) && CONSOLE_ON_UNIT(sc->sc_dv.dv_unit)) {
#ifdef DDB
		splx(s);		/* spl0()? */
		Debugger();
		return;
#else
		/* XXX maybe fall back to PROM? */
#endif
	}

	splx(s);
}

/*
 * Get a char off the appropriate line via. a busy wait loop.
 */
int
sccGetc(dev)
	dev_t dev;
{
	register scc_regmap_t *regs;
	register int c, line;
	register u_char value;
	int s;

	line = SCCLINE(dev);
	if (cold && scc_cons_addr) {
		regs = scc_cons_addr;
	} else {
		register struct scc_softc *sc;
		sc = scc_cd.cd_devs[SCCUNIT(dev)];
		regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	}

	if (!regs)
		return (0);
	s = splhigh();
	for (;;) {
		SCC_READ_REG(regs, line, SCC_RR0, value);
		if (value & ZSRR0_RX_READY) {
			SCC_READ_REG(regs, line, SCC_RR1, value);
			SCC_READ_DATA(regs, line, c);
			if (value & (ZSRR1_PE | ZSRR1_DO | ZSRR1_FE)) {
				SCC_WRITE_REG(regs, line, SCC_WR0,
				    ZSWR0_RESET_ERRORS);
				SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR0,
				    ZSWR0_CLR_INTR);
			} else {
				SCC_WRITE_REG(regs, SCC_CHANNEL_A, SCC_WR0,
				    ZSWR0_CLR_INTR);
				splx(s);
				return (c & 0xff);
			}
		} else
			DELAY(10);
	}
}

/*
 * Send a char on a port, via a busy wait loop.
 */
void
sccPutc(dev, c)
	dev_t dev;
	int c;
{
	register scc_regmap_t *regs;
	register int line;
	register u_char value;
	int s;

	s = splhigh();
	line = SCCLINE(dev);
	if (cold && scc_cons_addr) {
		regs = scc_cons_addr;
	} else {
		register struct scc_softc *sc;
		sc = scc_cd.cd_devs[SCCUNIT(dev)];
		regs = (scc_regmap_t *)sc->scc_pdma[line].p_addr;
	}

	/*
	 * Wait for transmitter to be not busy.
	 */
	do {
		SCC_READ_REG(regs, line, SCC_RR0, value);
		if (value & ZSRR0_TX_READY)
			break;
		DELAY(100);
	} while (1);

	/*
	 * Send the char.
	 */
	SCC_WRITE_DATA(regs, line, c);
	tc_mb();
	splx(s);

	return;
}

/*
 * Enable/disable polling mode
 */
void
sccPollc(dev, on)
	dev_t dev;
	int on;
{
}

#ifdef	SCC_DEBUG
static void
rr(msg, regs)
	char *msg;
	scc_regmap_t *regs;
{
	u_char value;
	int r0, r1, r2, r3, r10, r15;

	printf("%s: register: %lx\n", msg, regs);
#define	L(reg, r) {							\
	SCC_READ_REG(regs, SCC_CHANNEL_A, reg, value);			\
	r = value;							\
}
	L(SCC_RR0, r0);
	L(SCC_RR1, r1);
	L(ZSRR_IVEC, r2);
	L(ZSRR_IPEND, r3);
	L(SCC_RR10, r10);
	L(SCC_RR15, r15);
	printf("A: 0: %x  1: %x    2(vec): %x  3: %x  10: %x  15: %x\n",
	    r0, r1, r2, r3, r10, r15);
#undef L
#define	L(reg, r) {							\
	SCC_READ_REG(regs, SCC_CHANNEL_B, reg, value);			\
	r = value;							\
}
	L(SCC_RR0, r0);
	L(SCC_RR1, r1);
	L(ZSRR_IVEC, r2);
	L(SCC_RR10, r10);
	L(SCC_RR15, r15);
	printf("B: 0: %x  1: %x  2(state): %x        10: %x  15: %x\n",
	    r0, r1, r2, r10, r15);
}
#endif /* SCC_DEBUG */
