/*	$NetBSD: ite.c,v 1.47.4.2 2001/10/13 17:42:37 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: ite.c 1.28 92/12/20$
 *
 *	@(#)ite.c	8.2 (Berkeley) 1/12/94
 */

/*
 * Bit-mapped display terminal emulator machine independent code.
 * This is a very rudimentary.  Much more can be abstracted out of
 * the hardware dependent routines.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/vnode.h>

#include <machine/autoconf.h>

#include <dev/cons.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/hilioctl.h>
#include <hp300/dev/hilvar.h>
#include <hp300/dev/itevar.h>
#include <hp300/dev/kbdmap.h>

/* prototypes for devsw entry points */
cdev_decl(ite);

#define set_attr(ip, attr)	((ip)->attribute |= (attr))
#define clr_attr(ip, attr)	((ip)->attribute &= ~(attr))

/*
 * # of chars are output in a single itestart() call.
 * If this is too big, user processes will be blocked out for
 * long periods of time while we are emptying the queue in itestart().
 * If it is too small, console output will be very ragged.
 */
int	iteburst = 64;

struct  ite_data *kbd_ite = NULL;

int	itematch __P((struct device *, struct cfdata *, void *));
void	iteattach __P((struct device *, struct device *, void *));

struct cfattach ite_ca = {
	sizeof(struct ite_softc), itematch, iteattach
};

extern struct cfdriver ite_cd;

/*
 * Terminal emulator state information, statically allocated
 * for the benefit of the console.
 */
struct	ite_data ite_cn;

void	iteinit __P((struct ite_data *));
void	iteputchar __P((int, struct ite_data *));
void	itecheckwrap __P((struct ite_data *, struct itesw *));
void	ite_dchar __P((struct ite_data *, struct itesw *));
void	ite_ichar __P((struct ite_data *, struct itesw *));
void	ite_dline __P((struct ite_data *, struct itesw *));
void	ite_iline __P((struct ite_data *, struct itesw *));
void	ite_clrtoeol __P((struct ite_data *, struct itesw *, int, int));
void	ite_clrtoeos __P((struct ite_data *, struct itesw *));
void	itestart __P((struct tty *));

/*
 * Primary attribute buffer to be used by the first bitmapped console
 * found. Secondary displays alloc the attribute buffer as needed.
 * Size is based on a 68x128 display, which is currently our largest.
 */
u_char  ite_console_attributes[0x2200];

#define ite_erasecursor(ip, sp)	{ \
	if ((ip)->flags & ITE_CURSORON) \
		(*(sp)->ite_cursor)((ip), ERASE_CURSOR); \
}
#define ite_drawcursor(ip, sp) { \
	if ((ip)->flags & ITE_CURSORON) \
		(*(sp)->ite_cursor)((ip), DRAW_CURSOR); \
}
#define ite_movecursor(ip, sp) { \
	if ((ip)->flags & ITE_CURSORON) \
		(*(sp)->ite_cursor)((ip), MOVE_CURSOR); \
}

int
itematch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{

	return (1);
}

void
iteattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ite_softc *ite = (struct ite_softc *)self;
	struct grf_softc *grf = (struct grf_softc *)parent;
	struct grfdev_attach_args *ga = aux;

	/* Allocate the ite_data. */
	if (ga->ga_isconsole) {
		ite->sc_data = &ite_cn;
		printf(": console");

		/*
		 * We didn't know which unit this would be during
		 * the console probe, so we have to fixup cn_dev here.
		 */
		cn_tab->cn_dev = makedev(ite_major(), self->dv_unit);
	} else {
		ite->sc_data =
		    (struct ite_data *)malloc(sizeof(struct ite_data),
		    M_DEVBUF, M_NOWAIT);
		if (ite->sc_data == NULL) {
			printf("\n%s: malloc for ite_data failed\n",
			    ite->sc_dev.dv_xname);
			return;
		}
		bzero(ite->sc_data, sizeof(struct ite_data));
		ite->sc_data->flags = ITE_ALIVE;
	}

	/*
	 * Cross-reference the ite and the grf.
	 */
	ite->sc_grf = grf;
	grf->sc_ite = ite;

	printf("\n");
}

/*
 * Perform functions necessary to setup device as a terminal emulator.
 */
int
iteon(ip, flag)
	struct ite_data *ip;
	int flag;
{

	if ((ip->flags & ITE_ALIVE) == 0)
		return(ENXIO);

	/* force ite active, overriding graphics mode */
	if (flag & 1) {
		ip->flags |= ITE_ACTIVE;
		ip->flags &= ~(ITE_INGRF|ITE_INITED);
	}

	/* leave graphics mode */
	if (flag & 2) {
		ip->flags &= ~ITE_INGRF;
		if ((ip->flags & ITE_ACTIVE) == 0)
			return(0);
	}

	ip->flags |= ITE_ACTIVE;
	if (ip->flags & ITE_INGRF)
		return(0);

	if (kbd_ite == NULL || kbd_ite == ip) {
		kbd_ite = ip;
		kbdenable(0);		/* XXX */
	}

	iteinit(ip);
	return(0);
}

void
iteinit(ip)
	struct ite_data *ip;
{

	if (ip->flags & ITE_INITED)
		return;
	
	ip->curx = 0;
	ip->cury = 0;
	ip->cursorx = 0;
	ip->cursory = 0;

	(*ip->isw->ite_init)(ip);
	ip->flags |= ITE_CURSORON;
	ite_drawcursor(ip, ip->isw);

	ip->attribute = 0;
	if (ip->attrbuf == NULL)
		ip->attrbuf = (u_char *)
			malloc(ip->rows * ip->cols, M_DEVBUF, M_WAITOK);
	bzero(ip->attrbuf, (ip->rows * ip->cols));

	ip->imode = 0;
	ip->flags |= ITE_INITED;
}

/*
 * "Shut down" device as terminal emulator.
 * Note that we do not deinit the console device unless forced.
 * Deinit'ing the console every time leads to a very active
 * screen when processing /etc/rc.
 */
void
iteoff(ip, flag)
	struct ite_data *ip;
	int flag;
{

	if (flag & 2) {
		ip->flags |= ITE_INGRF;
		ip->flags &= ~ITE_CURSORON;
	}
	if ((ip->flags & ITE_ACTIVE) == 0)
		return;
	if ((flag & 1) ||
	    (ip->flags & (ITE_INGRF|ITE_ISCONS|ITE_INITED)) == ITE_INITED)
		(*ip->isw->ite_deinit)(ip);

	/*
	 * XXX When the system is rebooted with "reboot", init(8)
	 * kills the last process to have the console open.
	 * If we don't prevent the ITE_ACTIVE bit from being
	 * cleared, we will never see messages printed during
	 * the process of rebooting.
	 */
	if ((flag & 2) == 0 && (ip->flags & ITE_ISCONS) == 0)
		ip->flags &= ~ITE_ACTIVE;
}

/* ARGSUSED */
int
iteopen(devvp, mode, devtype, p)
	struct vnode *devvp;
	int mode, devtype;
	struct proc *p;
{
	dev_t dev = vdev_rdev(devvp);
	int unit = ITEUNIT(dev);
	struct tty *tp;
	struct ite_softc *sc;
	struct ite_data *ip;
	int error;
	int first = 0;

	if (unit >= ite_cd.cd_ndevs ||
	    (sc = ite_cd.cd_devs[unit]) == NULL)
		return (ENXIO);
	ip = sc->sc_data;

	if (ip->tty == NULL) {
	 	tp = ip->tty = ttymalloc();
		tty_attach(tp);
	} else
		tp = ip->tty;
	if ((tp->t_state&(TS_ISOPEN|TS_XCLUDE)) == (TS_ISOPEN|TS_XCLUDE)
	    && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	if ((ip->flags & ITE_ACTIVE) == 0) {
		error = iteon(ip, 0);
		if (error)
			return (error);
		first = 1;
	}

	vdev_setprivdata(devvp, sc);

	tp->t_oproc = itestart;
	tp->t_param = NULL;
	tp->t_dev = dev;
	if ((tp->t_state&TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = CS8|CREAD;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		tp->t_state = TS_ISOPEN|TS_CARR_ON;
		ttsetwater(tp);
	}
	error = (*tp->t_linesw->l_open)(devvp, tp);
	if (error == 0) {
		tp->t_winsize.ws_row = ip->rows;
		tp->t_winsize.ws_col = ip->cols;
	} else if (first)
		iteoff(ip, 0);
	return (error);
}

/*ARGSUSED*/
int
iteclose(devvp, flag, mode, p)
	struct vnode *devvp;
	int flag, mode;
	struct proc *p;
{
	struct ite_softc *sc = vdev_privdata(devvp);
	struct ite_data *ip = sc->sc_data;
	struct tty *tp = ip->tty;

	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);
	iteoff(ip, 0);
#if 0
	tty_detach(tp);
	ttyfree(tp);
	ip->tty = (struct tty *)0;
#endif
	return(0);
}

int
iteread(devvp, uio, flag)
	struct vnode *devvp;
	struct uio *uio;
	int flag;
{
	struct ite_softc *sc = vdev_privdata(devvp);
	struct tty *tp = sc->sc_data->tty;

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
itewrite(devvp, uio, flag)
	struct vnode *devvp;
	struct uio *uio;
	int flag;
{
	struct ite_softc *sc = vdev_privdata(devvp);
	struct tty *tp = sc->sc_data->tty;

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
itepoll(devvp, events, p)
	struct vnode *devvp;
	int events;
	struct proc *p;
{
	struct ite_softc *sc = vdev_privdata(devvp);
	struct tty *tp = sc->sc_data->tty;
 
	return ((*tp->t_linesw->l_poll)(tp, events, p));
}

struct tty *
itetty(devvp)
	struct vnode *devvp;
{
	struct ite_softc *sc = vdev_privdata(devvp);

	return (sc->sc_data->tty);
}

int
iteioctl(devvp, cmd, addr, flag, p)
	struct vnode *devvp;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct ite_softc *sc = vdev_privdata(devvp);
	struct ite_data *ip = sc->sc_data;
	struct tty *tp = ip->tty;
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, addr, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, devvp, cmd, addr, flag, p);
	if (error >= 0)
		return (error);
	return (ENOTTY);
}

void
itestart(tp)
	struct tty *tp;
{
	int cc, s;
	int hiwat = 0, hadcursor = 0;
	struct ite_softc *sc;
	struct ite_data *ip;

	sc = ite_cd.cd_devs[ITEUNIT(tp->t_dev)];
	ip = sc->sc_data;

	s = splkbd();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	cc = tp->t_outq.c_cc;
	if (cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	/*
	 * Handle common (?) case
	 */
	if (cc == 1) {
		iteputchar(getc(&tp->t_outq), ip);
	} else if (cc) {
		/*
		 * Limit the amount of output we do in one burst
		 * to prevent hogging the CPU.
		 */
		if (cc > iteburst) {
			hiwat++;
			cc = iteburst;
		}
		/*
		 * Turn off cursor while we output multiple characters.
		 * Saves a lot of expensive window move operations.
		 */
		if (ip->flags & ITE_CURSORON) {
			ite_erasecursor(ip, ip->isw);
			ip->flags &= ~ITE_CURSORON;
			hadcursor = 1;
		}
		while (--cc >= 0)
			iteputchar(getc(&tp->t_outq), ip);
		if (hadcursor) {
			ip->flags |= ITE_CURSORON;
			ite_drawcursor(ip, ip->isw);
		}
		if (hiwat) {
			tp->t_state |= TS_TIMEOUT;
			callout_reset(&tp->t_rstrt_ch, 1, ttrstrt, tp);
		}
	}
	tp->t_state &= ~TS_BUSY;
	splx(s);
}

void
itestop(tp, flag)
	struct tty *tp;
	int flag;
{

}

void
itefilter(stat, c)
	char stat, c;
{
	static int capsmode = 0;
	static int metamode = 0;
	char code, *str;
	struct tty *kbd_tty;

	if (kbd_ite == NULL || kbd_ite->tty == NULL)
		return;

	kbd_tty = kbd_ite->tty;

	switch (c & 0xFF) {
	case KBD_CAPSLOCK:
		capsmode = !capsmode;
		return;

	case KBD_EXT_LEFT_DOWN:
	case KBD_EXT_RIGHT_DOWN:
		metamode = 1;
		return;
		
	case KBD_EXT_LEFT_UP:
	case KBD_EXT_RIGHT_UP:
		metamode = 0;
		return;
	}

	c &= KBD_CHARMASK;
	switch ((stat>>KBD_SSHIFT) & KBD_SMASK) {
	default:
	case KBD_KEY:
	        if (!capsmode) {
			code = kbd_keymap[(int)c];
			break;
		}
		/* FALLTHROUGH */

	case KBD_SHIFT:
		code = kbd_shiftmap[(int)c];
		break;

	case KBD_CTRL:
		code = kbd_ctrlmap[(int)c];
		break;
		
	case KBD_CTRLSHIFT:	
		code = kbd_ctrlshiftmap[(int)c];
		break;
        }

	if (code == '\0' && (str = kbd_stringmap[(int)c]) != '\0') {
		while (*str)
			(*kbd_tty->t_linesw->l_rint)(*str++, kbd_tty);
	} else {
		if (metamode)
			code |= 0x80;
		(*kbd_tty->t_linesw->l_rint)(code, kbd_tty);
	}
}

void
iteputchar(c, ip)
	int c;
	struct ite_data *ip;
{
	struct itesw *sp = ip->isw;
	int n;

	if ((ip->flags & (ITE_ACTIVE|ITE_INGRF)) != ITE_ACTIVE)
	  	return;

	if (ip->escape) {
doesc:
		switch (ip->escape) {

		case '&':			/* Next can be a,d, or s */
			if (ip->fpd++) {
				ip->escape = c;
				ip->fpd = 0;
			}
			return;

		case 'a':				/* cursor change */
			switch (c) {

			case 'Y':			/* Only y coord. */
				ip->cury = min(ip->pos, ip->rows-1);
				ip->pos = 0;
				ip->escape = 0;
				ite_movecursor(ip, sp);
				clr_attr(ip, ATTR_INV);
				break;

			case 'y':			/* y coord first */
				ip->cury = min(ip->pos, ip->rows-1);
				ip->pos = 0;
				ip->fpd = 0;
				break;

			case 'C':			/* x coord */
				ip->curx = min(ip->pos, ip->cols-1);
				ip->pos = 0;
				ip->escape = 0;
				ite_movecursor(ip, sp);
				clr_attr(ip, ATTR_INV);
				break;

			default:	     /* Possibly a 3 digit number. */
				if (c >= '0' && c <= '9' && ip->fpd < 3) {
					ip->pos = ip->pos * 10 + (c - '0');
					ip->fpd++;
				} else {
					ip->pos = 0;
					ip->escape = 0;
				}
				break;
			}
			return;

		case 'd':				/* attribute change */
			switch (c) {

			case 'B':
				set_attr(ip, ATTR_INV);
				break;
		        case 'D':
				/* XXX: we don't do anything for underline */
				set_attr(ip, ATTR_UL);
				break;
		        case '@':
				clr_attr(ip, ATTR_ALL);
				break;
			}
			ip->escape = 0;
			return;

		case 's':				/* keypad control */
			switch (ip->fpd) {

			case 0:
				ip->hold = c;
				ip->fpd++;
				return;

			case 1:
				if (c == 'A') {
					switch (ip->hold) {
	
					case '0':
						clr_attr(ip, ATTR_KPAD);
						break;
					case '1':
						set_attr(ip, ATTR_KPAD);
						break;
					}
				}
				ip->hold = 0;
			}
			ip->escape = 0;
			return;

		case 'i':			/* back tab */
			if (ip->curx > TABSIZE) {
				n = ip->curx - (ip->curx & (TABSIZE - 1));
				ip->curx -= n;
			} else
				ip->curx = 0;
			ite_movecursor(ip, sp);
			ip->escape = 0;
			return;

		case '3':			/* clear all tabs */
			goto ignore;

		case 'K':			/* clear_eol */
			ite_clrtoeol(ip, sp, ip->cury, ip->curx);
			ip->escape = 0;
			return;

		case 'J':			/* clear_eos */
			ite_clrtoeos(ip, sp);
			ip->escape = 0;
			return;

		case 'B':			/* cursor down 1 line */
			if (++ip->cury == ip->rows) {
				--ip->cury;
				ite_erasecursor(ip, sp);
				(*sp->ite_scroll)(ip, 1, 0, 1, SCROLL_UP);
				ite_clrtoeol(ip, sp, ip->cury, 0);
			}
			else
				ite_movecursor(ip, sp);
			clr_attr(ip, ATTR_INV);
			ip->escape = 0;
			return;

		case 'C':			/* cursor forward 1 char */
			ip->escape = 0;
			itecheckwrap(ip, sp);
			return;

		case 'A':			/* cursor up 1 line */
			if (ip->cury > 0) {
				ip->cury--;
				ite_movecursor(ip, sp);
			}
			ip->escape = 0;
			clr_attr(ip, ATTR_INV);
			return;

		case 'P':			/* delete character */
			ite_dchar(ip, sp);
			ip->escape = 0;
			return;

		case 'M':			/* delete line */
			ite_dline(ip, sp);
			ip->escape = 0;
			return;

		case 'Q':			/* enter insert mode */
			ip->imode = 1;
			ip->escape = 0;
			return;

		case 'R':			/* exit insert mode */
			ip->imode = 0;
			ip->escape = 0;
			return;

		case 'L':			/* insert blank line */
			ite_iline(ip, sp);
			ip->escape = 0;
			return;

		case 'h':			/* home key */
			ip->cury = ip->curx = 0;
			ite_movecursor(ip, sp);
			ip->escape = 0;
			return;

		case 'D':			/* left arrow key */
			if (ip->curx > 0) {
				ip->curx--;
				ite_movecursor(ip, sp);
			}
			ip->escape = 0;
			return;

		case '1':			/* set tab in all rows */
			goto ignore;

		case ESC:
			if ((ip->escape = c) == ESC)
				break;
			ip->fpd = 0;
			goto doesc;

		default:
ignore:
			ip->escape = 0;
			return;

		}
	}

	switch (c &= 0x7F) {

	case '\n':

		if (++ip->cury == ip->rows) {
			--ip->cury;
			ite_erasecursor(ip, sp);
			(*sp->ite_scroll)(ip, 1, 0, 1, SCROLL_UP);
			ite_clrtoeol(ip, sp, ip->cury, 0);
		} else
			ite_movecursor(ip, sp);
		clr_attr(ip, ATTR_INV);
		break;

	case '\r':
		if (ip->curx) {
			ip->curx = 0;
			ite_movecursor(ip, sp);
		}
		break;
	
	case '\b':
		if (--ip->curx < 0)
			ip->curx = 0;
		else
			ite_movecursor(ip, sp);
		break;

	case '\t':
		if (ip->curx < TABEND(ip)) {
			n = TABSIZE - (ip->curx & (TABSIZE - 1));
			ip->curx += n;
			ite_movecursor(ip, sp);
		} else
			itecheckwrap(ip, sp);
		break;

	case CTRL('G'):
		if (ip == kbd_ite)
			kbdbell(0);	/* XXX */
		break;

	case ESC:
		ip->escape = ESC;
		break;

	default:
		if (c < ' ' || c == DEL)
			break;
		if (ip->imode)
			ite_ichar(ip, sp);
		if ((ip->attribute & ATTR_INV) || attrtest(ip, ATTR_INV)) {
			attrset(ip, ATTR_INV);
			(*sp->ite_putc)(ip, c, ip->cury, ip->curx, ATTR_INV);
		} else
			(*sp->ite_putc)(ip, c, ip->cury, ip->curx, ATTR_NOR);
		ite_drawcursor(ip, sp);
		itecheckwrap(ip, sp);
		break;
	}
}

void
itecheckwrap(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	if (++ip->curx == ip->cols) {
		ip->curx = 0;
		clr_attr(ip, ATTR_INV);
		if (++ip->cury == ip->rows) {
			--ip->cury;
			ite_erasecursor(ip, sp);
			(*sp->ite_scroll)(ip, 1, 0, 1, SCROLL_UP);
			ite_clrtoeol(ip, sp, ip->cury, 0);
			return;
		}
	}
	ite_movecursor(ip, sp);
}

void
ite_dchar(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	if (ip->curx < ip->cols - 1) {
		ite_erasecursor(ip, sp);
		(*sp->ite_scroll)(ip, ip->cury, ip->curx + 1, 1, SCROLL_LEFT);
		attrmov(ip, ip->cury, ip->curx + 1, ip->cury, ip->curx,
			1, ip->cols - ip->curx - 1);
	}
	attrclr(ip, ip->cury, ip->cols - 1, 1, 1);
	(*sp->ite_putc)(ip, ' ', ip->cury, ip->cols - 1, ATTR_NOR);
	ite_drawcursor(ip, sp);
}

void
ite_ichar(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	if (ip->curx < ip->cols - 1) {
		ite_erasecursor(ip, sp);
		(*sp->ite_scroll)(ip, ip->cury, ip->curx, 1, SCROLL_RIGHT);
		attrmov(ip, ip->cury, ip->curx, ip->cury, ip->curx + 1,
			1, ip->cols - ip->curx - 1);
	}
	attrclr(ip, ip->cury, ip->curx, 1, 1);
	(*sp->ite_putc)(ip, ' ', ip->cury, ip->curx, ATTR_NOR);
	ite_drawcursor(ip, sp);
}

void
ite_dline(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	if (ip->cury < ip->rows - 1) {
		ite_erasecursor(ip, sp);
		(*sp->ite_scroll)(ip, ip->cury + 1, 0, 1, SCROLL_UP);
		attrmov(ip, ip->cury + 1, 0, ip->cury, 0,
			ip->rows - ip->cury - 1, ip->cols);
	}
	ite_clrtoeol(ip, sp, ip->rows - 1, 0);
}

void
ite_iline(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	if (ip->cury < ip->rows - 1) {
		ite_erasecursor(ip, sp);
		(*sp->ite_scroll)(ip, ip->cury, 0, 1, SCROLL_DOWN);
		attrmov(ip, ip->cury, 0, ip->cury + 1, 0,
			ip->rows - ip->cury - 1, ip->cols);
	}
	ite_clrtoeol(ip, sp, ip->cury, 0);
}

void
ite_clrtoeol(ip, sp, y, x)
     struct ite_data *ip;
     struct itesw *sp;
     int y, x;
{
	(*sp->ite_clear)(ip, y, x, 1, ip->cols - x);
	attrclr(ip, y, x, 1, ip->cols - x);
	ite_drawcursor(ip, sp);
}

void
ite_clrtoeos(ip, sp)
     struct ite_data *ip;
     struct itesw *sp;
{
	(*sp->ite_clear)(ip, ip->cury, 0, ip->rows - ip->cury, ip->cols);
	attrclr(ip, ip->cury, 0, ip->rows - ip->cury, ip->cols);
	ite_drawcursor(ip, sp);
}

int
ite_major()
{
	static int itemaj, initialized;

	/* Only compute once. */
	if (initialized)
		return (itemaj);
	initialized = 1;

	/* locate the major number */
	for (itemaj = 0; itemaj < nchrdev; itemaj++)
		if (cdevsw[itemaj].d_open == iteopen)
			break;

	return (itemaj);
}

/*
 * Console functions.  Console probes are done by the individual
 * framebuffer drivers.
 */

void
itecninit(gp, isw)
	struct grf_data *gp;
	struct itesw *isw;
{
	struct ite_data *ip = &ite_cn;

	/*
	 * Set up required ite data and initialize ite.
	 */
	ip->isw = isw;
	ip->grf = gp;
	ip->flags = ITE_ALIVE|ITE_CONSOLE|ITE_ACTIVE|ITE_ISCONS;
	ip->attrbuf = ite_console_attributes;
	iteinit(ip);

	/*
	 * Initialize the console keyboard.
	 */
	kbdcninit();

	kbd_ite = ip;		/* XXX */
}

/*ARGSUSED*/
int
itecngetc(dev)
	dev_t dev;
{
	int c;
	int stat;

	c = kbdgetc(&stat);
	switch ((stat >> KBD_SSHIFT) & KBD_SMASK) {
	case KBD_SHIFT:
		c = kbd_cn_shiftmap[c & KBD_CHARMASK];
		break;
	case KBD_CTRL:
		c = kbd_cn_ctrlmap[c & KBD_CHARMASK];
		break;
	case KBD_KEY:
		c = kbd_cn_keymap[c & KBD_CHARMASK];
		break;
	default:
		c = 0;
		break;
	}
	return(c);
}

/* ARGSUSED */
void
itecnputc(dev, c)
	dev_t dev;
	int c;
{
	static int paniced = 0;
	struct ite_data *ip = &ite_cn;

	if (panicstr && !paniced &&
	    (ip->flags & (ITE_ACTIVE|ITE_INGRF)) != ITE_ACTIVE) {
		(void) iteon(ip, 3);
		paniced = 1;
	}
	iteputchar(c, ip);
}
