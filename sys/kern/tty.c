/*	$NetBSD: tty.c,v 1.99.2.1 1997/10/28 01:59:37 thorpej Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty.c	8.8 (Berkeley) 1/21/94
 */

#include "opt_uconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#define	TTYDEFCHARS
#include <sys/tty.h>
#undef	TTYDEFCHARS
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/poll.h>

#include <vm/vm.h>

static int ttnread __P((struct tty *));
static void ttyblock __P((struct tty *));
static void ttyecho __P((int, struct tty *));
static void ttyrubo __P((struct tty *, int));
static int proc_compare __P((struct proc *, struct proc *));

/* Symbolic sleep message strings. */
const char	ttclos[] = "ttycls";
const char	ttopen[] = "ttyopn";
const char	ttybg[] = "ttybg";
#ifdef REAL_CLISTS
const char	ttybuf[] = "ttybuf";
#endif
const char	ttyin[] = "ttyin";
const char	ttyout[] = "ttyout";

/*
 * Used to determine whether we still have a connection.  This is true in
 * one of 3 cases:
 * 1) We have carrier.
 * 2) It's a locally attached terminal, and we are therefore ignoring carrier.
 * 3) We're using a flow control mechanism that overloads the carrier signal.
 */
#define	CONNECTED(tp)	(ISSET(tp->t_state, TS_CARR_ON) ||	\
			 ISSET(tp->t_cflag, CLOCAL | MDMBUF))

/*
 * Table with character classes and parity. The 8th bit indicates parity,
 * the 7th bit indicates the character is an alphameric or underscore (for
 * ALTWERASE), and the low 6 bits indicate delay type.  If the low 6 bits
 * are 0 then the character needs no special processing on output; classes
 * other than 0 might be translated or (not currently) require delays.
 */
#define	E	0x00	/* Even parity. */
#define	O	0x80	/* Odd parity. */
#define	PARITY(c)	(char_type[c] & O)

#define	ALPHA	0x40	/* Alpha or underscore. */
#define	ISALPHA(c)	(char_type[(c) & TTY_CHARMASK] & ALPHA)

#define	CCLASSMASK	0x3f
#define	CCLASS(c)	(char_type[c] & CCLASSMASK)

#define	BS	BACKSPACE
#define	CC	CONTROL
#define	CR	RETURN
#define	NA	ORDINARY | ALPHA
#define	NL	NEWLINE
#define	NO	ORDINARY
#define	TB	TAB
#define	VT	VTAB

char const char_type[] = {
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC,	/* nul - bel */
	O|BS, E|TB, E|NL, O|CC, E|VT, O|CR, O|CC, E|CC, /* bs - si */
	O|CC, E|CC, E|CC, O|CC, E|CC, O|CC, O|CC, E|CC, /* dle - etb */
	E|CC, O|CC, O|CC, E|CC, O|CC, E|CC, E|CC, O|CC, /* can - us */
	O|NO, E|NO, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* sp - ' */
	E|NO, O|NO, O|NO, E|NO, O|NO, E|NO, E|NO, O|NO, /* ( - / */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* 0 - 7 */
	O|NA, E|NA, E|NO, O|NO, E|NO, O|NO, O|NO, E|NO, /* 8 - ? */
	O|NO, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* @ - G */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* H - O */
	E|NA, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* P - W */
	O|NA, E|NA, E|NA, O|NO, E|NO, O|NO, O|NO, O|NA, /* X - _ */
	E|NO, O|NA, O|NA, E|NA, O|NA, E|NA, E|NA, O|NA, /* ` - g */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* h - o */
	O|NA, E|NA, E|NA, O|NA, E|NA, O|NA, O|NA, E|NA, /* p - w */
	E|NA, O|NA, O|NA, E|NO, O|NO, E|NO, E|NO, O|CC, /* x - del */
	/*
	 * Meta chars; should be settable per character set;
	 * for now, treat them all as normal characters.
	 */
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
	NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,
};
#undef	BS
#undef	CC
#undef	CR
#undef	NA
#undef	NL
#undef	NO
#undef	TB
#undef	VT

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~((unsigned)(f))
#define	ISSET(t, f)	((t) & (f))

struct ttylist_head ttylist;	/* TAILQ_HEAD */
int tty_count;

/*
 * Initial open of tty, or (re)entry to standard tty line discipline.
 */
int
ttyopen(device, tp)
	dev_t device;
	register struct tty *tp;
{
	int s;

	s = spltty();
	tp->t_dev = device;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_ISOPEN);
		bzero(&tp->t_winsize, sizeof(tp->t_winsize));
#ifdef COMPAT_OLDTTY
		tp->t_flags = 0;
#endif
	}
	CLR(tp->t_state, TS_WOPEN);
	splx(s);
	return (0);
}

/*
 * Handle close() on a tty line: flush and set to initial state,
 * bumping generation number so that pending read/write calls
 * can detect recycling of the tty.
 */
int
ttyclose(tp)
	register struct tty *tp;
{
	extern struct tty *constty;	/* Temporary virtual console. */

	if (constty == tp)
		constty = NULL;

	ttyflush(tp, FREAD | FWRITE);

	tp->t_gen++;
	tp->t_pgrp = NULL;
	tp->t_session = NULL;
	tp->t_state = 0;
	return (0);
}

#define	FLUSHQ(q) {							\
	if ((q)->c_cc)							\
		ndflush(q, (q)->c_cc);					\
}

/*
 * This macro is used in canonical mode input processing, where a read
 * request shall not return unless a 'line delimiter' ('\n') or 'break'
 * (EOF, EOL, EOL2) character (or a signal) has been received. As EOL2
 * is an extension to the POSIX.1 defined set of special characters,
 * recognize it only if IEXTEN is set in the set of local flags.
 */
#define	TTBREAKC(c, lflg)						\
	((c) == '\n' || (((c) == cc[VEOF] || (c) == cc[VEOL] ||		\
	((c) == cc[VEOL2] && ISSET(lflg, IEXTEN))) && (c) != _POSIX_VDISABLE))


/*
 * Process input of a single character received on a tty.
 */
int
ttyinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register int iflag, lflag;
	register u_char *cc;
	int i, error;

	/*
	 * Unless the receiver is enabled, drop incoming data.
	 */
	if (!ISSET(tp->t_cflag, CREAD))
		return (0);

	/*
	 * If input is pending take it first.
	 */
	lflag = tp->t_lflag;
	if (ISSET(lflag, PENDIN))
		ttypend(tp);
	/*
	 * Gather stats.
	 */
	if (ISSET(lflag, ICANON)) {
		++tk_cancc;
		++tp->t_cancc;
	} else {
		++tk_rawcc;
		++tp->t_rawcc;
	}
	++tk_nin;

	cc = tp->t_cc;

	/*
	 * Handle exceptional conditions (break, parity, framing).
	 */
	iflag = tp->t_iflag;
	if ((error = (ISSET(c, TTY_ERRORMASK))) != 0) {
		CLR(c, TTY_ERRORMASK);
		if (ISSET(error, TTY_FE) && c == 0) {		/* Break. */
			if (ISSET(iflag, IGNBRK))
				return (0);
			else if (ISSET(iflag, BRKINT)) {
				ttyflush(tp, FREAD | FWRITE);
				pgsignal(tp->t_pgrp, SIGINT, 1);
				return (0);
			}
			else if (ISSET(iflag, PARMRK))
				goto parmrk;
		}
		else if ((ISSET(error, TTY_PE) && ISSET(iflag, INPCK)) ||
		    ISSET(error, TTY_FE)) {
			if (ISSET(iflag, IGNPAR))
				return (0);
			else if (ISSET(iflag, PARMRK)) {
parmrk:				(void)putc(0377 | TTY_QUOTE, &tp->t_rawq);
				(void)putc(0    | TTY_QUOTE, &tp->t_rawq);
				(void)putc(c    | TTY_QUOTE, &tp->t_rawq);
				return (0);
			}
			else
				c = 0;
		}
	}
	else if (c == 0377 &&
	    ISSET(iflag, ISTRIP|IGNPAR|INPCK|PARMRK) == (INPCK|PARMRK)) {
		/* "Escape" a valid character of '\377'. */
		(void)putc(0377 | TTY_QUOTE, &tp->t_rawq);
		(void)putc(0377 | TTY_QUOTE, &tp->t_rawq);
		goto endcase;
	}

	/*
	 * In tandem mode, check high water mark.
	 */
	if (ISSET(iflag, IXOFF) || ISSET(tp->t_cflag, CHWFLOW))
		ttyblock(tp);
	if (!ISSET(tp->t_state, TS_TYPEN) && ISSET(iflag, ISTRIP))
		CLR(c, 0x80);
	if (!ISSET(lflag, EXTPROC)) {
		/*
		 * Check for literal nexting very first
		 */
		if (ISSET(tp->t_state, TS_LNCH)) {
			SET(c, TTY_QUOTE);
			CLR(tp->t_state, TS_LNCH);
		}
		/*
		 * Scan for special characters.  This code
		 * is really just a big case statement with
		 * non-constant cases.  The bottom of the
		 * case statement is labeled ``endcase'', so goto
		 * it after a case match, or similar.
		 */

		/*
		 * Control chars which aren't controlled
		 * by ICANON, ISIG, or IXON.
		 */
		if (ISSET(lflag, IEXTEN)) {
			if (CCEQ(cc[VLNEXT], c)) {
				if (ISSET(lflag, ECHO)) {
					if (ISSET(lflag, ECHOE)) {
						(void)ttyoutput('^', tp);
						(void)ttyoutput('\b', tp);
					} else
						ttyecho(c, tp);
				}
				SET(tp->t_state, TS_LNCH);
				goto endcase;
			}
			if (CCEQ(cc[VDISCARD], c)) {
				if (ISSET(lflag, FLUSHO))
					CLR(tp->t_lflag, FLUSHO);
				else {
					ttyflush(tp, FWRITE);
					ttyecho(c, tp);
					if (tp->t_rawq.c_cc + tp->t_canq.c_cc)
						ttyretype(tp);
					SET(tp->t_lflag, FLUSHO);
				}
				goto startoutput;
			}
		}
		/*
		 * Signals.
		 */
		if (ISSET(lflag, ISIG)) {
			if (CCEQ(cc[VINTR], c) || CCEQ(cc[VQUIT], c)) {
				if (!ISSET(lflag, NOFLSH))
					ttyflush(tp, FREAD | FWRITE);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp,
				    CCEQ(cc[VINTR], c) ? SIGINT : SIGQUIT, 1);
				goto endcase;
			}
			if (CCEQ(cc[VSUSP], c)) {
				if (!ISSET(lflag, NOFLSH))
					ttyflush(tp, FREAD);
				ttyecho(c, tp);
				pgsignal(tp->t_pgrp, SIGTSTP, 1);
				goto endcase;
			}
		}
		/*
		 * Handle start/stop characters.
		 */
		if (ISSET(iflag, IXON)) {
			if (CCEQ(cc[VSTOP], c)) {
				if (!ISSET(tp->t_state, TS_TTSTOP)) {
					SET(tp->t_state, TS_TTSTOP);
					(*cdevsw[major(tp->t_dev)].d_stop)(tp,
					   0);
					return (0);
				}
				if (!CCEQ(cc[VSTART], c))
					return (0);
				/*
				 * if VSTART == VSTOP then toggle
				 */
				goto endcase;
			}
			if (CCEQ(cc[VSTART], c))
				goto restartoutput;
		}
		/*
		 * IGNCR, ICRNL, & INLCR
		 */
		if (c == '\r') {
			if (ISSET(iflag, IGNCR))
				goto endcase;
			else if (ISSET(iflag, ICRNL))
				c = '\n';
		} else if (c == '\n' && ISSET(iflag, INLCR))
			c = '\r';
	}
	if (!ISSET(tp->t_lflag, EXTPROC) && ISSET(lflag, ICANON)) {
		/*
		 * From here on down canonical mode character
		 * processing takes place.
		 */
		/*
		 * erase (^H / ^?)
		 */
		if (CCEQ(cc[VERASE], c)) {
			if (tp->t_rawq.c_cc)
				ttyrub(unputc(&tp->t_rawq), tp);
			goto endcase;
		}
		/*
		 * kill (^U)
		 */
		if (CCEQ(cc[VKILL], c)) {
			if (ISSET(lflag, ECHOKE) &&
			    tp->t_rawq.c_cc == tp->t_rocount &&
			    !ISSET(lflag, ECHOPRT))
				while (tp->t_rawq.c_cc)
					ttyrub(unputc(&tp->t_rawq), tp);
			else {
				ttyecho(c, tp);
				if (ISSET(lflag, ECHOK) ||
				    ISSET(lflag, ECHOKE))
					ttyecho('\n', tp);
				FLUSHQ(&tp->t_rawq);
				tp->t_rocount = 0;
			}
			CLR(tp->t_state, TS_LOCAL);
			goto endcase;
		}
		/*
		 * Extensions to the POSIX.1 GTI set of functions.
		 */
		if (ISSET(lflag, IEXTEN)) {
			/*
			 * word erase (^W)
			 */
			if (CCEQ(cc[VWERASE], c)) {
				int alt = ISSET(lflag, ALTWERASE);
				int ctype;

				/*
				 * erase whitespace
				 */
				while ((c = unputc(&tp->t_rawq)) == ' ' ||
				       c == '\t')
					ttyrub(c, tp);
				if (c == -1)
					goto endcase;
				/*
				 * erase last char of word and remember the
				 * next chars type (for ALTWERASE)
				 */
				ttyrub(c, tp);
				c = unputc(&tp->t_rawq);
				if (c == -1)
					goto endcase;
				if (c == ' ' || c == '\t') {
					(void)putc(c, &tp->t_rawq);
					goto endcase;
				}
				ctype = ISALPHA(c);
				/*
				 * erase rest of word
				 */
				do {
					ttyrub(c, tp);
					c = unputc(&tp->t_rawq);
					if (c == -1)
						goto endcase;
				} while (c != ' ' && c != '\t' &&
				         (alt == 0 || ISALPHA(c) == ctype));
				(void)putc(c, &tp->t_rawq);
				goto endcase;
			}
			/*
			 * reprint line (^R)
			 */
			if (CCEQ(cc[VREPRINT], c)) {
				ttyretype(tp);
				goto endcase;
			}
			/*
			 * ^T - kernel info and generate SIGINFO
			 */
			if (CCEQ(cc[VSTATUS], c)) {
				if (ISSET(lflag, ISIG))
					pgsignal(tp->t_pgrp, SIGINFO, 1);
				if (!ISSET(lflag, NOKERNINFO))
					ttyinfo(tp);
				goto endcase;
			}
		}
	}
	/*
	 * Check for input buffer overflow
	 */
	if (tp->t_rawq.c_cc + tp->t_canq.c_cc >= TTYHOG) {
		if (ISSET(iflag, IMAXBEL)) {
			if (tp->t_outq.c_cc < tp->t_hiwat)
				(void)ttyoutput(CTRL('g'), tp);
		} else
			ttyflush(tp, FREAD | FWRITE);
		goto endcase;
	}
	/*
	 * Put data char in q for user and
	 * wakeup on seeing a line delimiter.
	 */
	if (putc(c, &tp->t_rawq) >= 0) {
		if (!ISSET(lflag, ICANON)) {
			ttwakeup(tp);
			ttyecho(c, tp);
			goto endcase;
		}
		if (TTBREAKC(c, lflag)) {
			tp->t_rocount = 0;
			catq(&tp->t_rawq, &tp->t_canq);
			ttwakeup(tp);
		} else if (tp->t_rocount++ == 0)
			tp->t_rocol = tp->t_column;
		if (ISSET(tp->t_state, TS_ERASE)) {
			/*
			 * end of prterase \.../
			 */
			CLR(tp->t_state, TS_ERASE);
			(void)ttyoutput('/', tp);
		}
		i = tp->t_column;
		ttyecho(c, tp);
		if (CCEQ(cc[VEOF], c) && ISSET(lflag, ECHO)) {
			/*
			 * Place the cursor over the '^' of the ^D.
			 */
			i = min(2, tp->t_column - i);
			while (i > 0) {
				(void)ttyoutput('\b', tp);
				i--;
			}
		}
	}
endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if (ISSET(tp->t_state, TS_TTSTOP) &&
	    !ISSET(iflag, IXANY) && cc[VSTART] != cc[VSTOP])
		return (0);
restartoutput:
	CLR(tp->t_lflag, FLUSHO);
	CLR(tp->t_state, TS_TTSTOP);
startoutput:
	return (ttstart(tp));
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 * Returns < 0 if succeeds, otherwise returns char to resend.
 * Must be recursive.
 */
int
ttyoutput(c, tp)
	register int c;
	register struct tty *tp;
{
	register long oflag;
	register int col, notout, s;

	oflag = tp->t_oflag;
	if (!ISSET(oflag, OPOST)) {
		tk_nout++;
		tp->t_outcc++;
		if (!ISSET(tp->t_lflag, FLUSHO) && putc(c, &tp->t_outq))
			return (c);
		return (-1);
	}
	/*
	 * Do tab expansion if OXTABS is set.  Special case if we external
	 * processing, we don't do the tab expansion because we'll probably
	 * get it wrong.  If tab expansion needs to be done, let it happen
	 * externally.
	 */
	CLR(c, ~TTY_CHARMASK);
	if (c == '\t' &&
	    ISSET(oflag, OXTABS) && !ISSET(tp->t_lflag, EXTPROC)) {
		c = 8 - (tp->t_column & 7);
		if (ISSET(tp->t_lflag, FLUSHO)) {
			notout = 0;
		} else {
			s = spltty();		/* Don't interrupt tabs. */
			notout = b_to_q("        ", c, &tp->t_outq);
			c -= notout;
			tk_nout += c;
			tp->t_outcc += c;
			splx(s);
		}
		tp->t_column += c;
		return (notout ? '\t' : -1);
	}
	if (c == CEOT && ISSET(oflag, ONOEOT))
		return (-1);

	/*
	 * Newline translation: if ONLCR is set,
	 * translate newline into "\r\n".
	 */
	if (c == '\n' && ISSET(tp->t_oflag, ONLCR)) {
		tk_nout++;
		tp->t_outcc++;
		if (!ISSET(tp->t_lflag, FLUSHO) && putc('\r', &tp->t_outq))
			return (c);
	}
	/*
	 * If OCRNL is set, translate "\r" into "\n".
	 */
	else if (c == '\r' && ISSET(tp->t_oflag, OCRNL))
		c = '\n';

	tk_nout++;
	tp->t_outcc++;
	if (!ISSET(tp->t_lflag, FLUSHO) && putc(c, &tp->t_outq))
		return (c);

	col = tp->t_column;
	switch (CCLASS(c)) {
	case BACKSPACE:
		if (col > 0)
			--col;
		break;
	case CONTROL:
		break;
	case NEWLINE:
		if (ISSET(tp->t_oflag, ONLCR))
			col = 0;
		break;
	case RETURN:
		col = 0;
		break;
	case ORDINARY:
		++col;
		break;
	case TAB:
		col = (col + 8) & ~7;
		break;
	}
	tp->t_column = col;
	return (-1);
}

/*
 * Ioctls for all tty devices.  Called after line-discipline specific ioctl
 * has been called to do discipline-specific functions and/or reject any
 * of these ioctl commands.
 */
/* ARGSUSED */
int
ttioctl(tp, cmd, data, flag, p)
	register struct tty *tp;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	extern struct tty *constty;	/* Temporary virtual console. */
	extern int nlinesw;
	int s, error;

	/* If the ioctl involves modification, hang if in the background. */
	switch (cmd) {
	case  TIOCFLUSH:
	case  TIOCDRAIN:
	case  TIOCSBRK:
	case  TIOCCBRK:
	case  TIOCSTART:
	case  TIOCSETA:
	case  TIOCSETD:
	case  TIOCSETAF:
	case  TIOCSETAW:
#ifdef notdef
	case  TIOCSPGRP:
#endif
	case  TIOCSTAT:
	case  TIOCSTI:
	case  TIOCSWINSZ:
#ifdef COMPAT_OLDTTY
	case  TIOCLBIC:
	case  TIOCLBIS:
	case  TIOCLSET:
	case  TIOCSETC:
	case OTIOCSETD:
	case  TIOCSETN:
	case  TIOCSETP:
	case  TIOCSLTC:
#endif
		while (isbackground(curproc, tp) &&
		    p->p_pgrp->pg_jobc && (p->p_flag & P_PPWAIT) == 0 &&
		    (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
		    (p->p_sigmask & sigmask(SIGTTOU)) == 0) {
			pgsignal(p->p_pgrp, SIGTTOU, 1);
			error = ttysleep(tp,
					 &lbolt, TTOPRI | PCATCH, ttybg, 0);
			if (error)
				return (error);
		}
		break;
	}

	switch (cmd) {			/* Process the ioctl. */
	case FIOASYNC:			/* set/clear async i/o */
		s = spltty();
		if (*(int *)data)
			SET(tp->t_state, TS_ASYNC);
		else
			CLR(tp->t_state, TS_ASYNC);
		splx(s);
		break;
	case FIONBIO:			/* set/clear non-blocking i/o */
		break;			/* XXX: delete. */
	case FIONREAD:			/* get # bytes to read */
		*(int *)data = ttnread(tp);
		break;
	case TIOCEXCL:			/* set exclusive use of tty */
		s = spltty();
		SET(tp->t_state, TS_XCLUDE);
		splx(s);
		break;
	case TIOCFLUSH: {		/* flush buffers */
		register int flags = *(int *)data;

		if (flags == 0)
			flags = FREAD | FWRITE;
		else
			flags &= FREAD | FWRITE;
		ttyflush(tp, flags);
		break;
	}
	case TIOCCONS:			/* become virtual console */
		if (*(int *)data) {
			if (constty && constty != tp &&
			    ISSET(constty->t_state, TS_CARR_ON | TS_ISOPEN) ==
			    (TS_CARR_ON | TS_ISOPEN))
				return (EBUSY);
#ifndef	UCONSOLE
			if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
				return (error);
#endif
			constty = tp;
		} else if (tp == constty)
			constty = NULL;
		break;
	case TIOCDRAIN:			/* wait till output drained */
		if ((error = ttywait(tp)) != 0)
			return (error);
		break;
	case TIOCGETA: {		/* get termios struct */
		struct termios *t = (struct termios *)data;

		bcopy(&tp->t_termios, t, sizeof(struct termios));
		break;
	}
	case TIOCGETD:			/* get line discipline */
		*(int *)data = tp->t_line;
		break;
	case TIOCGWINSZ:		/* get window size */
		*(struct winsize *)data = tp->t_winsize;
		break;
	case TIOCGPGRP:			/* get pgrp of tty */
		if (!isctty(p, tp))
			return (ENOTTY);
		*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		break;
#ifdef TIOCHPCL
	case TIOCHPCL:			/* hang up on last close */
		s = spltty();
		SET(tp->t_cflag, HUPCL);
		splx(s);
		break;
#endif
	case TIOCNXCL:			/* reset exclusive use of tty */
		s = spltty();
		CLR(tp->t_state, TS_XCLUDE);
		splx(s);
		break;
	case TIOCOUTQ:			/* output queue size */
		*(int *)data = tp->t_outq.c_cc;
		break;
	case TIOCSETA:			/* set termios struct */
	case TIOCSETAW:			/* drain output, set */
	case TIOCSETAF: {		/* drn out, fls in, set */
		register struct termios *t = (struct termios *)data;

		s = spltty();
		if (cmd == TIOCSETAW || cmd == TIOCSETAF) {
			if ((error = ttywait(tp)) != 0) {
				splx(s);
				return (error);
			}
			if (cmd == TIOCSETAF)
				ttyflush(tp, FREAD);
		}
		if (!ISSET(t->c_cflag, CIGNORE)) {
			/*
			 * Set device hardware.
			 */
			if (tp->t_param && (error = (*tp->t_param)(tp, t))) {
				splx(s);
				return (error);
			} else {
				if (!ISSET(tp->t_state, TS_CARR_ON) &&
				    ISSET(tp->t_cflag, CLOCAL) &&
				    !ISSET(t->c_cflag, CLOCAL)) {
					CLR(tp->t_state, TS_ISOPEN);
					SET(tp->t_state, TS_WOPEN);
					ttwakeup(tp);
				}
				tp->t_cflag = t->c_cflag;
				tp->t_ispeed = t->c_ispeed;
				tp->t_ospeed = t->c_ospeed;
				if (t->c_ospeed == 0 && tp->t_session &&
				    tp->t_session->s_leader)
					psignal(tp->t_session->s_leader,
					    SIGHUP);
			}
			ttsetwater(tp);
		}
		if (cmd != TIOCSETAF) {
			if (ISSET(t->c_lflag, ICANON) !=
			    ISSET(tp->t_lflag, ICANON))
				if (ISSET(t->c_lflag, ICANON)) {
					SET(tp->t_lflag, PENDIN);
					ttwakeup(tp);
				} else {
					struct clist tq;

					catq(&tp->t_rawq, &tp->t_canq);
					tq = tp->t_rawq;
					tp->t_rawq = tp->t_canq;
					tp->t_canq = tq;
					CLR(tp->t_lflag, PENDIN);
				}
		}
		tp->t_iflag = t->c_iflag;
		tp->t_oflag = t->c_oflag;
		/*
		 * Make the EXTPROC bit read only.
		 */
		if (ISSET(tp->t_lflag, EXTPROC))
			SET(t->c_lflag, EXTPROC);
		else
			CLR(t->c_lflag, EXTPROC);
		tp->t_lflag = t->c_lflag | ISSET(tp->t_lflag, PENDIN);
		bcopy(t->c_cc, tp->t_cc, sizeof(t->c_cc));
		splx(s);
		break;
	}
	case TIOCSETD: {		/* set line discipline */
		register int t = *(int *)data;
		dev_t device = tp->t_dev;

		if ((u_int)t >= nlinesw)
			return (ENXIO);
		if (t != tp->t_line) {
			s = spltty();
			(*linesw[tp->t_line].l_close)(tp, flag);
			error = (*linesw[t].l_open)(device, tp);
			if (error) {
				(void)(*linesw[tp->t_line].l_open)(device, tp);
				splx(s);
				return (error);
			}
			tp->t_line = t;
			splx(s);
		}
		break;
	}
	case TIOCSTART:			/* start output, like ^Q */
		s = spltty();
		if (ISSET(tp->t_state, TS_TTSTOP) ||
		    ISSET(tp->t_lflag, FLUSHO)) {
			CLR(tp->t_lflag, FLUSHO);
			CLR(tp->t_state, TS_TTSTOP);
			ttstart(tp);
		}
		splx(s);
		break;
	case TIOCSTI:			/* simulate terminal input */
		if (p->p_ucred->cr_uid && (flag & FREAD) == 0)
			return (EPERM);
		if (p->p_ucred->cr_uid && !isctty(p, tp))
			return (EACCES);
		(*linesw[tp->t_line].l_rint)(*(u_char *)data, tp);
		break;
	case TIOCSTOP:			/* stop output, like ^S */
		s = spltty();
		if (!ISSET(tp->t_state, TS_TTSTOP)) {
			SET(tp->t_state, TS_TTSTOP);
			(*cdevsw[major(tp->t_dev)].d_stop)(tp, 0);
		}
		splx(s);
		break;
	case TIOCSCTTY:			/* become controlling tty */
		/* Session ctty vnode pointer set in vnode layer. */
		if (!SESS_LEADER(p) ||
		    ((p->p_session->s_ttyvp || tp->t_session) &&
		     (tp->t_session != p->p_session)))
			return (EPERM);
		tp->t_session = p->p_session;
		tp->t_pgrp = p->p_pgrp;
		p->p_session->s_ttyp = tp;
		p->p_flag |= P_CONTROLT;
		break;
	case TIOCSPGRP: {		/* set pgrp of tty */
		register struct pgrp *pgrp = pgfind(*(int *)data);

		if (!isctty(p, tp))
			return (ENOTTY);
		else if (pgrp == NULL)
			return (EINVAL);
		else if (pgrp->pg_session != p->p_session)
			return (EPERM);
		tp->t_pgrp = pgrp;
		break;
	}
	case TIOCSTAT:			/* get load avg stats */
		ttyinfo(tp);
		break;
	case TIOCSWINSZ:		/* set window size */
		if (bcmp((caddr_t)&tp->t_winsize, data,
		    sizeof (struct winsize))) {
			tp->t_winsize = *(struct winsize *)data;
			pgsignal(tp->t_pgrp, SIGWINCH, 1);
		}
		break;
	default:
#ifdef COMPAT_OLDTTY
		return (ttcompat(tp, cmd, data, flag, p));
#else
		return (-1);
#endif
	}
	return (0);
}

int
ttpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	register struct tty *tp = (*cdevsw[major(dev)].d_tty)(dev);
	int revents = 0;
	int s = spltty();

	if (events & (POLLIN | POLLRDNORM))
		if (ttnread(tp) > 0)
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_outq.c_cc <= tp->t_lowat)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if (!CONNECTED(tp))
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLHUP | POLLRDNORM))
			selrecord(p, &tp->t_rsel);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &tp->t_wsel);
	}

	splx(s);
	return (revents);
}

static int
ttnread(tp)
	struct tty *tp;
{
	int nread;

	if (ISSET(tp->t_lflag, PENDIN))
		ttypend(tp);
	nread = tp->t_canq.c_cc;
	if (!ISSET(tp->t_lflag, ICANON)) {
		nread += tp->t_rawq.c_cc;
		if (nread < tp->t_cc[VMIN] && !tp->t_cc[VTIME])
			nread = 0;
	}
	return (nread);
}

/*
 * Wait for output to drain.
 */
int
ttywait(tp)
	register struct tty *tp;
{
	int error, s;

	error = 0;
	s = spltty();
	while ((tp->t_outq.c_cc || ISSET(tp->t_state, TS_BUSY)) &&
	    CONNECTED(tp) && tp->t_oproc) {
		(*tp->t_oproc)(tp);
		SET(tp->t_state, TS_ASLEEP);
		error = ttysleep(tp, &tp->t_outq, TTOPRI | PCATCH, ttyout, 0);
		if (error)
			break;
	}
	splx(s);
	return (error);
}

/*
 * Flush if successfully wait.
 */
int
ttywflush(tp)
	struct tty *tp;
{
	int error;

	if ((error = ttywait(tp)) == 0)
		ttyflush(tp, FREAD);
	return (error);
}

/*
 * Flush tty read and/or write queues, notifying anyone waiting.
 */
void
ttyflush(tp, rw)
	register struct tty *tp;
	int rw;
{
	register int s;

	s = spltty();
	if (rw & FREAD) {
		FLUSHQ(&tp->t_canq);
		FLUSHQ(&tp->t_rawq);
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		CLR(tp->t_state, TS_LOCAL);
		ttwakeup(tp);
	}
	if (rw & FWRITE) {
		CLR(tp->t_state, TS_TTSTOP);
		(*cdevsw[major(tp->t_dev)].d_stop)(tp, rw);
		FLUSHQ(&tp->t_outq);
		wakeup((caddr_t)&tp->t_outq);
		selwakeup(&tp->t_wsel);
	}
	splx(s);
}

/*
 * Copy in the default termios characters.
 */
void
ttychars(tp)
	struct tty *tp;
{

	bcopy(ttydefchars, tp->t_cc, sizeof(ttydefchars));
}

/*
 * Send stop character on input overflow.
 */
static void
ttyblock(tp)
	register struct tty *tp;
{
	register int total;

	total = tp->t_rawq.c_cc + tp->t_canq.c_cc;
	if (tp->t_rawq.c_cc > TTYHOG) {
		ttyflush(tp, FREAD | FWRITE);
		CLR(tp->t_state, TS_TBLOCK);
	}
	/*
	 * Block further input iff: current input > threshold
	 * AND input is available to user program.
	 */
	if ((total >= TTYHOG / 2 &&
	     !ISSET(tp->t_state, TS_TBLOCK) &&
	     !ISSET(tp->t_lflag, ICANON)) || tp->t_canq.c_cc > 0) {
		if (ISSET(tp->t_iflag, IXOFF) &&
		    tp->t_cc[VSTOP] != _POSIX_VDISABLE &&
		    putc(tp->t_cc[VSTOP], &tp->t_outq) == 0) {
			SET(tp->t_state, TS_TBLOCK);
			ttstart(tp);
		}
		/* Try to block remote output via hardware flow control. */
		if (ISSET(tp->t_cflag, CHWFLOW) && tp->t_hwiflow &&
		    (*tp->t_hwiflow)(tp, 1) != 0)
			SET(tp->t_state, TS_TBLOCK);
	}
}

void
ttrstrt(tp_arg)
	void *tp_arg;
{
	struct tty *tp;
	int s;

#ifdef DIAGNOSTIC
	if (tp_arg == NULL)
		panic("ttrstrt");
#endif
	tp = tp_arg;
	s = spltty();

	CLR(tp->t_state, TS_TIMEOUT);
	ttstart(tp);

	splx(s);
}

int
ttstart(tp)
	struct tty *tp;
{

	if (tp->t_oproc != NULL)	/* XXX: Kludge for pty. */
		(*tp->t_oproc)(tp);
	return (0);
}

/*
 * "close" a line discipline
 */
int
ttylclose(tp, flag)
	struct tty *tp;
	int flag;
{

	if (flag & FNONBLOCK)
		ttyflush(tp, FREAD | FWRITE);
	else
		ttywflush(tp);
	return (0);
}

/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns 0 if the line should be turned off, otherwise 1.
 */
int
ttymodem(tp, flag)
	register struct tty *tp;
	int flag;
{

	if (flag == 0) {
		if (ISSET(tp->t_state, TS_CARR_ON)) {
			/*
			 * Lost carrier.
			 */
			CLR(tp->t_state, TS_CARR_ON);
			if (ISSET(tp->t_state, TS_ISOPEN) && !CONNECTED(tp)) {
				if (tp->t_session && tp->t_session->s_leader)
					psignal(tp->t_session->s_leader, SIGHUP);
				ttyflush(tp, FREAD | FWRITE);
				return (0);
			}
		}
	} else {
		if (!ISSET(tp->t_state, TS_CARR_ON)) {
			/*
			 * Carrier now on.
			 */
			SET(tp->t_state, TS_CARR_ON);
			ttwakeup(tp);
		}
	}
	return (1);
}

/*
 * Default modem control routine (for other line disciplines).
 * Return argument flag, to turn off device on carrier drop.
 */
int
nullmodem(tp, flag)
	register struct tty *tp;
	int flag;
{

	if (flag)
		SET(tp->t_state, TS_CARR_ON);
	else {
		CLR(tp->t_state, TS_CARR_ON);
		if (!CONNECTED(tp)) {
			if (tp->t_session && tp->t_session->s_leader)
				psignal(tp->t_session->s_leader, SIGHUP);
			return (0);
		}
	}
	return (1);
}

/*
 * Reinput pending characters after state switch
 * call at spltty().
 */
void
ttypend(tp)
	register struct tty *tp;
{
	struct clist tq;
	register c;

	CLR(tp->t_lflag, PENDIN);
	SET(tp->t_state, TS_TYPEN);
	tq = tp->t_rawq;
	tp->t_rawq.c_cc = 0;
	tp->t_rawq.c_cf = tp->t_rawq.c_cl = 0;
	while ((c = getc(&tq)) >= 0)
		ttyinput(c, tp);
	CLR(tp->t_state, TS_TYPEN);
}

/*
 * Process a read call on a tty device.
 */
int
ttread(tp, uio, flag)
	register struct tty *tp;
	struct uio *uio;
	int flag;
{
	register struct clist *qp;
	register int c;
	register long lflag;
	register u_char *cc = tp->t_cc;
	register struct proc *p = curproc;
	int s, first, error = 0;
	struct timeval stime;
	int has_stime = 0, last_cc = 0;
	long slp = 0;

loop:	lflag = tp->t_lflag;
	s = spltty();
	/*
	 * take pending input first
	 */
	if (ISSET(lflag, PENDIN))
		ttypend(tp);
	splx(s);

	/*
	 * Hang process if it's in the background.
	 */
	if (isbackground(p, tp)) {
		if ((p->p_sigignore & sigmask(SIGTTIN)) ||
		   (p->p_sigmask & sigmask(SIGTTIN)) ||
		    p->p_flag & P_PPWAIT || p->p_pgrp->pg_jobc == 0)
			return (EIO);
		pgsignal(p->p_pgrp, SIGTTIN, 1);
		error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, ttybg, 0);
		if (error)
			return (error);
		goto loop;
	}

	s = spltty();
	if (!ISSET(lflag, ICANON)) {
		int m = cc[VMIN];
		long t = cc[VTIME];

		qp = &tp->t_rawq;
		/*
		 * Check each of the four combinations.
		 * (m > 0 && t == 0) is the normal read case.
		 * It should be fairly efficient, so we check that and its
		 * companion case (m == 0 && t == 0) first.
		 * For the other two cases, we compute the target sleep time
		 * into slp.
		 */
		if (t == 0) {
			if (qp->c_cc < m)
				goto sleep;
			goto read;
		}
		t *= 100000;		/* time in us */
#define diff(t1, t2) (((t1).tv_sec - (t2).tv_sec) * 1000000 + \
			 ((t1).tv_usec - (t2).tv_usec))
		if (m > 0) {
			if (qp->c_cc <= 0)
				goto sleep;
			if (qp->c_cc >= m)
				goto read;
			if (!has_stime) {
				/* first character, start timer */
				has_stime = 1;
				stime = time;
				slp = t;
			} else if (qp->c_cc > last_cc) {
				/* got a character, restart timer */
				stime = time;
				slp = t;
			} else {
				/* nothing, check expiration */
				slp = t - diff(time, stime);
			}
		} else {	/* m == 0 */
			if (qp->c_cc > 0)
				goto read;
			if (!has_stime) {
				has_stime = 1;
				stime = time;
				slp = t;
			} else
				slp = t - diff(time, stime);
		}
		last_cc = qp->c_cc;
#undef diff
		if (slp > 0) {
			/*
			 * Rounding down may make us wake up just short
			 * of the target, so we round up.
			 * The formula is ceiling(slp * hz/1000000).
			 * 32-bit arithmetic is enough for hz < 169.
			 *
			 * Also, use plain wakeup() not ttwakeup().
			 */
			slp = (long) (((u_long)slp * hz) + 999999) / 1000000;
			goto sleep;
		}
	} else if ((qp = &tp->t_canq)->c_cc <= 0) {
		int carrier;

sleep:
		/*
		 * If there is no input, sleep on rawq
		 * awaiting hardware receipt and notification.
		 * If we have data, we don't need to check for carrier.
		 */
		carrier = CONNECTED(tp);
		if (!carrier && ISSET(tp->t_state, TS_ISOPEN)) {
			splx(s);
			return (0);	/* EOF */
		}
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
		    carrier ? ttyin : ttopen, slp);
		splx(s);
		/* VMIN == 0: any quantity read satisfies */
		if (cc[VMIN] == 0 && error == EWOULDBLOCK)
			return (0);
		if (error && error != EWOULDBLOCK)
			return (error);
		goto loop;
	}
read:
	splx(s);

	/*
	 * Input present, check for input mapping and processing.
	 */
	first = 1;
	while ((c = getc(qp)) >= 0) {
		/*
		 * delayed suspend (^Y)
		 */
		if (CCEQ(cc[VDSUSP], c) &&
		    ISSET(lflag, IEXTEN|ISIG) == (IEXTEN|ISIG)) {
			pgsignal(tp->t_pgrp, SIGTSTP, 1);
			if (first) {
				error = ttysleep(tp, &lbolt,
						 TTIPRI | PCATCH, ttybg, 0);
				if (error)
					break;
				goto loop;
			}
			break;
		}
		/*
		 * Interpret EOF only in canonical mode.
		 */
		if (CCEQ(cc[VEOF], c) && ISSET(lflag, ICANON))
			break;
		/*
		 * Give user character.
		 */
 		error = ureadc(c, uio);
		if (error)
			break;
 		if (uio->uio_resid == 0)
			break;
		/*
		 * In canonical mode check for a "break character"
		 * marking the end of a "line of input".
		 */
		if (ISSET(lflag, ICANON) && TTBREAKC(c, lflag))
			break;
		first = 0;
	}
	/*
	 * Look to unblock output now that (presumably)
	 * the input queue has gone down.
	 */
	s = spltty();
	if (ISSET(tp->t_state, TS_TBLOCK) && tp->t_rawq.c_cc < TTYHOG/5) {
		if (ISSET(tp->t_iflag, IXOFF) &&
		    cc[VSTART] != _POSIX_VDISABLE &&
		    putc(cc[VSTART], &tp->t_outq) == 0) {
			CLR(tp->t_state, TS_TBLOCK);
			ttstart(tp);
		}
		/* Try to unblock remote output via hardware flow control. */
		if (ISSET(tp->t_cflag, CHWFLOW) && tp->t_hwiflow &&
		    (*tp->t_hwiflow)(tp, 0) != 0)
			CLR(tp->t_state, TS_TBLOCK);
	}
	splx(s);
	return (error);
}

/*
 * Check the output queue on tp for space for a kernel message (from uprintf
 * or tprintf).  Allow some space over the normal hiwater mark so we don't
 * lose messages due to normal flow control, but don't let the tty run amok.
 * Sleeps here are not interruptible, but we return prematurely if new signals
 * arrive.
 */
int
ttycheckoutq(tp, wait)
	register struct tty *tp;
	int wait;
{
	int hiwat, s, oldsig;

	hiwat = tp->t_hiwat;
	s = spltty();
	oldsig = wait ? curproc->p_siglist : 0;
	if (tp->t_outq.c_cc > hiwat + 200)
		while (tp->t_outq.c_cc > hiwat) {
			ttstart(tp);
			if (wait == 0 || curproc->p_siglist != oldsig) {
				splx(s);
				return (0);
			}
			timeout((void (*)__P((void *)))wakeup,
			    (void *)&tp->t_outq, hz);
			SET(tp->t_state, TS_ASLEEP);
			tsleep(&tp->t_outq, PZERO - 1, "ttckoutq", 0);
		}
	splx(s);
	return (1);
}

/*
 * Process a write call on a tty device.
 */
int
ttwrite(tp, uio, flag)
	register struct tty *tp;
	register struct uio *uio;
	int flag;
{
	register u_char *cp = NULL;
	register int cc, ce;
	register struct proc *p;
	int i, hiwat, cnt, error, s;
	u_char obuf[OBUFSIZ];

	hiwat = tp->t_hiwat;
	cnt = uio->uio_resid;
	error = 0;
	cc = 0;
loop:
	s = spltty();
	if (!CONNECTED(tp)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			splx(s);
			return (EIO);
		} else if (flag & IO_NDELAY) {
			splx(s);
			error = EWOULDBLOCK;
			goto out;
		} else {
			/* Sleep awaiting carrier. */
			error = ttysleep(tp,
			    &tp->t_rawq, TTIPRI | PCATCH, ttopen, 0);
			splx(s);
			if (error)
				goto out;
			goto loop;
		}
	}
	splx(s);
	/*
	 * Hang the process if it's in the background.
	 */
	p = curproc;
	if (isbackground(p, tp) &&
	    ISSET(tp->t_lflag, TOSTOP) && (p->p_flag & P_PPWAIT) == 0 &&
	    (p->p_sigignore & sigmask(SIGTTOU)) == 0 &&
	    (p->p_sigmask & sigmask(SIGTTOU)) == 0) {
		if (p->p_pgrp->pg_jobc == 0) {
			error = EIO;
			goto out;
		}
		pgsignal(p->p_pgrp, SIGTTOU, 1);
		error = ttysleep(tp, &lbolt, TTIPRI | PCATCH, ttybg, 0);
		if (error)
			goto out;
		goto loop;
	}
	/*
	 * Process the user's data in at most OBUFSIZ chunks.  Perform any
	 * output translation.  Keep track of high water mark, sleep on
	 * overflow awaiting device aid in acquiring new space.
	 */
	while (uio->uio_resid > 0 || cc > 0) {
		if (ISSET(tp->t_lflag, FLUSHO)) {
			uio->uio_resid = 0;
			return (0);
		}
		if (tp->t_outq.c_cc > hiwat)
			goto ovhiwat;
		/*
		 * Grab a hunk of data from the user, unless we have some
		 * leftover from last time.
		 */
		if (cc == 0) {
			cc = min(uio->uio_resid, OBUFSIZ);
			cp = obuf;
			error = uiomove(cp, cc, uio);
			if (error) {
				cc = 0;
				break;
			}
		}
		/*
		 * If nothing fancy need be done, grab those characters we
		 * can handle without any of ttyoutput's processing and
		 * just transfer them to the output q.  For those chars
		 * which require special processing (as indicated by the
		 * bits in char_type), call ttyoutput.  After processing
		 * a hunk of data, look for FLUSHO so ^O's will take effect
		 * immediately.
		 */
		while (cc > 0) {
			if (!ISSET(tp->t_oflag, OPOST))
				ce = cc;
			else {
				ce = cc - scanc((u_int)cc, cp, char_type,
				    CCLASSMASK);
				/*
				 * If ce is zero, then we're processing
				 * a special character through ttyoutput.
				 */
				if (ce == 0) {
					tp->t_rocount = 0;
					if (ttyoutput(*cp, tp) >= 0) {
#ifdef REAL_CLISTS
						/* No Clists, wait a bit. */
						ttstart(tp);
						if (error = ttysleep(tp, &lbolt,
						    TTOPRI | PCATCH, ttybuf, 0))
							break;
						goto loop;
#else
						/* out of space */
						goto overfull;
#endif
					}
					cp++;
					cc--;
					if (ISSET(tp->t_lflag, FLUSHO) ||
					    tp->t_outq.c_cc > hiwat)
						goto ovhiwat;
					continue;
				}
			}
			/*
			 * A bunch of normal characters have been found.
			 * Transfer them en masse to the output queue and
			 * continue processing at the top of the loop.
			 * If there are any further characters in this
			 * <= OBUFSIZ chunk, the first should be a character
			 * requiring special handling by ttyoutput.
			 */
			tp->t_rocount = 0;
			i = b_to_q(cp, ce, &tp->t_outq);
			ce -= i;
			tp->t_column += ce;
			cp += ce, cc -= ce, tk_nout += ce;
			tp->t_outcc += ce;
			if (i > 0) {
#ifdef REAL_CLISTS
				/* No Clists, wait a bit. */
				ttstart(tp);
				if (error = ttysleep(tp,
				    &lbolt, TTOPRI | PCATCH, ttybuf, 0))
					break;
				goto loop;
#else
				/* out of space */
				goto overfull;
#endif
			}
			if (ISSET(tp->t_lflag, FLUSHO) ||
			    tp->t_outq.c_cc > hiwat)
				break;
		}
		ttstart(tp);
	}
out:
	/*
	 * If cc is nonzero, we leave the uio structure inconsistent, as the
	 * offset and iov pointers have moved forward, but it doesn't matter
	 * (the call will either return short or restart with a new uio).
	 */
	uio->uio_resid += cc;
	return (error);

#ifndef REAL_CLISTS
overfull:
	/*
	 * Since we are using ring buffers, if we can't insert any more into
	 * the output queue, we can assume the ring is full and that someone
	 * forgot to set the high water mark correctly.  We set it and then
	 * proceed as normal.
	 */
	hiwat = tp->t_outq.c_cc - 1;
#endif

ovhiwat:
	ttstart(tp);
	s = spltty();
	/*
	 * This can only occur if FLUSHO is set in t_lflag,
	 * or if ttstart/oproc is synchronous (or very fast).
	 */
	if (tp->t_outq.c_cc <= hiwat) {
		splx(s);
		goto loop;
	}
	if (flag & IO_NDELAY) {
		splx(s);
		uio->uio_resid += cc;
		return (uio->uio_resid == cnt ? EWOULDBLOCK : 0);
	}
	SET(tp->t_state, TS_ASLEEP);
	error = ttysleep(tp, &tp->t_outq, TTOPRI | PCATCH, ttyout, 0);
	splx(s);
	if (error)
		goto out;
	goto loop;
}

/*
 * Rubout one character from the rawq of tp
 * as cleanly as possible.
 */
void
ttyrub(c, tp)
	int c;
	register struct tty *tp;
{
	register u_char *cp;
	register int savecol;
	int tabc, s;

	if (!ISSET(tp->t_lflag, ECHO) || ISSET(tp->t_lflag, EXTPROC))
		return;
	CLR(tp->t_lflag, FLUSHO);
	if (ISSET(tp->t_lflag, ECHOE)) {
		if (tp->t_rocount == 0) {
			/*
			 * Screwed by ttwrite; retype
			 */
			ttyretype(tp);
			return;
		}
		if (c == ('\t' | TTY_QUOTE) || c == ('\n' | TTY_QUOTE))
			ttyrubo(tp, 2);
		else {
			CLR(c, ~TTY_CHARMASK);
			switch (CCLASS(c)) {
			case ORDINARY:
				ttyrubo(tp, 1);
				break;
			case BACKSPACE:
			case CONTROL:
			case NEWLINE:
			case RETURN:
			case VTAB:
				if (ISSET(tp->t_lflag, ECHOCTL))
					ttyrubo(tp, 2);
				break;
			case TAB:
				if (tp->t_rocount < tp->t_rawq.c_cc) {
					ttyretype(tp);
					return;
				}
				s = spltty();
				savecol = tp->t_column;
				SET(tp->t_state, TS_CNTTB);
				SET(tp->t_lflag, FLUSHO);
				tp->t_column = tp->t_rocol;
				for (cp = firstc(&tp->t_rawq, &tabc); cp;
				    cp = nextc(&tp->t_rawq, cp, &tabc))
					ttyecho(tabc, tp);
				CLR(tp->t_lflag, FLUSHO);
				CLR(tp->t_state, TS_CNTTB);
				splx(s);

				/* savecol will now be length of the tab. */
				savecol -= tp->t_column;
				tp->t_column += savecol;
				if (savecol > 8)
					savecol = 8;	/* overflow screw */
				while (--savecol >= 0)
					(void)ttyoutput('\b', tp);
				break;
			default:			/* XXX */
#define	PANICSTR	"ttyrub: would panic c = %d, val = %d\n"
				(void)printf(PANICSTR, c, CCLASS(c));
#ifdef notdef
				panic(PANICSTR, c, CCLASS(c));
#endif
			}
		}
	} else if (ISSET(tp->t_lflag, ECHOPRT)) {
		if (!ISSET(tp->t_state, TS_ERASE)) {
			SET(tp->t_state, TS_ERASE);
			(void)ttyoutput('\\', tp);
		}
		ttyecho(c, tp);
	} else
		ttyecho(tp->t_cc[VERASE], tp);
	--tp->t_rocount;
}

/*
 * Back over cnt characters, erasing them.
 */
static void
ttyrubo(tp, cnt)
	register struct tty *tp;
	int cnt;
{

	while (cnt-- > 0) {
		(void)ttyoutput('\b', tp);
		(void)ttyoutput(' ', tp);
		(void)ttyoutput('\b', tp);
	}
}

/*
 * ttyretype --
 *	Reprint the rawq line.  Note, it is assumed that c_cc has already
 *	been checked.
 */
void
ttyretype(tp)
	register struct tty *tp;
{
	register u_char *cp;
	int s, c;

	/* Echo the reprint character. */
	if (tp->t_cc[VREPRINT] != _POSIX_VDISABLE)
		ttyecho(tp->t_cc[VREPRINT], tp);

	(void)ttyoutput('\n', tp);

	s = spltty();
	for (cp = firstc(&tp->t_canq, &c); cp; cp = nextc(&tp->t_canq, cp, &c))
		ttyecho(c, tp);
	for (cp = firstc(&tp->t_rawq, &c); cp; cp = nextc(&tp->t_rawq, cp, &c))
		ttyecho(c, tp);
	CLR(tp->t_state, TS_ERASE);
	splx(s);

	tp->t_rocount = tp->t_rawq.c_cc;
	tp->t_rocol = 0;
}

/*
 * Echo a typed character to the terminal.
 */
static void
ttyecho(c, tp)
	register int c;
	register struct tty *tp;
{

	if (!ISSET(tp->t_state, TS_CNTTB))
		CLR(tp->t_lflag, FLUSHO);
	if ((!ISSET(tp->t_lflag, ECHO) &&
	    (!ISSET(tp->t_lflag, ECHONL) || c != '\n')) ||
	    ISSET(tp->t_lflag, EXTPROC))
		return;
	if (((ISSET(tp->t_lflag, ECHOCTL) &&
	     (ISSET(c, TTY_CHARMASK) <= 037 && c != '\t' && c != '\n')) ||
	    ISSET(c, TTY_CHARMASK) == 0177)) {
		(void)ttyoutput('^', tp);
		CLR(c, ~TTY_CHARMASK);
		if (c == 0177)
			c = '?';
		else
			c += 'A' - 1;
	}
	(void)ttyoutput(c, tp);
}

/*
 * Wake up any readers on a tty.
 */
void
ttwakeup(tp)
	register struct tty *tp;
{

	selwakeup(&tp->t_rsel);
	if (ISSET(tp->t_state, TS_ASYNC))
		pgsignal(tp->t_pgrp, SIGIO, 1);
	wakeup((caddr_t)&tp->t_rawq);
}

/*
 * Look up a code for a specified speed in a conversion table;
 * used by drivers to map software speed values to hardware parameters.
 */
int
ttspeedtab(speed, table)
	int speed;
	register struct speedtab *table;
{

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return (-1);
}

/*
 * Set tty hi and low water marks.
 *
 * Try to arrange the dynamics so there's about one second
 * from hi to low water.
 */
void
ttsetwater(tp)
	struct tty *tp;
{
	register int cps, x;

#define CLAMP(x, h, l)	((x) > h ? h : ((x) < l) ? l : (x))

	cps = tp->t_ospeed / 10;
	tp->t_lowat = x = CLAMP(cps / 2, TTMAXLOWAT, TTMINLOWAT);
	x += cps;
	x = CLAMP(x, TTMAXHIWAT, TTMINHIWAT);
	tp->t_hiwat = roundup(x, CBSIZE);
#undef	CLAMP
}

/*
 * Report on state of foreground process group.
 */
void
ttyinfo(tp)
	register struct tty *tp;
{
	register struct proc *p, *pick;
	struct timeval utime, stime;
	int tmp;

	if (ttycheckoutq(tp,0) == 0)
		return;

	/* Print load average. */
	tmp = (averunnable.ldavg[0] * 100 + FSCALE / 2) >> FSHIFT;
	ttyprintf(tp, "load: %d.%02d ", tmp / 100, tmp % 100);

	if (tp->t_session == NULL)
		ttyprintf(tp, "not a controlling terminal\n");
	else if (tp->t_pgrp == NULL)
		ttyprintf(tp, "no foreground process group\n");
	else if ((p = tp->t_pgrp->pg_members.lh_first) == 0)
		ttyprintf(tp, "empty foreground process group\n");
	else {
		/* Pick interesting process. */
		for (pick = NULL; p != 0; p = p->p_pglist.le_next)
			if (proc_compare(pick, p))
				pick = p;

		ttyprintf(tp, " cmd: %s %d [%s] ", pick->p_comm, pick->p_pid,
		    pick->p_stat == SRUN ? "running" :
		    pick->p_wmesg ? pick->p_wmesg : "iowait");

		calcru(pick, &utime, &stime, NULL);

		/* Round up and print user time. */
		utime.tv_usec += 5000;
		if (utime.tv_usec >= 1000000) {
			utime.tv_sec += 1;
			utime.tv_usec -= 1000000;
		}
		ttyprintf(tp, "%ld.%02ldu ", utime.tv_sec,
		    utime.tv_usec / 10000);

		/* Round up and print system time. */
		stime.tv_usec += 5000;
		if (stime.tv_usec >= 1000000) {
			stime.tv_sec += 1;
			stime.tv_usec -= 1000000;
		}
		ttyprintf(tp, "%ld.%02lds ", stime.tv_sec,
		    stime.tv_usec / 10000);

#define	pgtok(a)	(((u_long) ((a) * NBPG) / 1024))
		/* Print percentage cpu. */
		tmp = (pick->p_pctcpu * 10000 + FSCALE / 2) >> FSHIFT;
		ttyprintf(tp, "%d%% ", tmp / 100);

		/* Print resident set size. */
		if (pick->p_stat == SIDL || pick->p_stat == SZOMB)
			tmp = 0;
		else {
			register struct vmspace *vm = pick->p_vmspace;
			tmp = pgtok(vm_resident_count(vm));
		}
		ttyprintf(tp, "%dk\n", tmp);
	}
	tp->t_rocount = 0;	/* so pending input will be retyped if BS */
}

/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) Only foreground processes are eligible - implied.
 *	2) Runnable processes are favored over anything else.  The runner
 *	   with the highest cpu utilization is picked (p_estcpu).  Ties are
 *	   broken by picking the highest pid.
 *	3) The sleeper with the shortest sleep time is next.  With ties,
 *	   we pick out just "short-term" sleepers (P_SINTR == 0).
 *	4) Further ties are broken by picking the highest pid.
 */
#define ISRUN(p)	(((p)->p_stat == SRUN) || ((p)->p_stat == SIDL))
#define TESTAB(a, b)    ((a)<<1 | (b))
#define ONLYA   2
#define ONLYB   1
#define BOTH    3

static int
proc_compare(p1, p2)
	register struct proc *p1, *p2;
{

	if (p1 == NULL)
		return (1);
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(ISRUN(p1), ISRUN(p2))) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		/*
		 * tie - favor one with highest recent cpu utilization
		 */
		if (p2->p_estcpu > p1->p_estcpu)
			return (1);
		if (p1->p_estcpu > p2->p_estcpu)
			return (0);
		return (p2->p_pid > p1->p_pid);	/* tie - return highest pid */
	}
	/*
 	 * weed out zombies
	 */
	switch (TESTAB(p1->p_stat == SZOMB, p2->p_stat == SZOMB)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		return (p2->p_pid > p1->p_pid); /* tie - return highest pid */
	}
	/*
	 * pick the one with the smallest sleep time
	 */
	if (p2->p_slptime > p1->p_slptime)
		return (0);
	if (p1->p_slptime > p2->p_slptime)
		return (1);
	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	if (p1->p_flag & P_SINTR && (p2->p_flag & P_SINTR) == 0)
		return (1);
	if (p2->p_flag & P_SINTR && (p1->p_flag & P_SINTR) == 0)
		return (0);
	return (p2->p_pid > p1->p_pid);		/* tie - return highest pid */
}

/*
 * Output char to tty; console putchar style.
 */
int
tputchar(c, tp)
	int c;
	struct tty *tp;
{
	register int s;

	s = spltty();
	if (ISSET(tp->t_state,
	    TS_CARR_ON | TS_ISOPEN) != (TS_CARR_ON | TS_ISOPEN)) {
		splx(s);
		return (-1);
	}
	if (c == '\n')
		(void)ttyoutput('\r', tp);
	(void)ttyoutput(c, tp);
	ttstart(tp);
	splx(s);
	return (0);
}

/*
 * Sleep on chan, returning ERESTART if tty changed while we napped and
 * returning any errors (e.g. EINTR/ETIMEDOUT) reported by tsleep.  If
 * the tty is revoked, restarting a pending call will redo validation done
 * at the start of the call.
 */
int
ttysleep(tp, chan, pri, wmesg, timo)
	struct tty *tp;
	void *chan;
	int pri, timo;
	const char *wmesg;
{
	int error;
	short gen;

	gen = tp->t_gen;
	if ((error = tsleep(chan, pri, wmesg, timo)) != 0)
		return (error);
	return (tp->t_gen == gen ? 0 : ERESTART);
}

/*
 * Initialise the global tty list.
 */
void
tty_init()
{

	TAILQ_INIT(&ttylist);
	tty_count = 0;
}

/*
 * Attach a tty to the tty list.
 *
 * This should be called ONLY once per real tty (including pty's).
 * eg, on the sparc, the keyboard and mouse have struct tty's that are
 * distinctly NOT usable as tty's, and thus should not be attached to
 * the ttylist.  This is why this call is not done from ttymalloc().
 *
 * Device drivers should attach tty's at a similar time that they are
 * ttymalloc()'ed, or, for the case of statically allocated struct tty's
 * either in the attach or (first) open routine.
 */
void
tty_attach(tp)
	struct tty *tp;
{

	TAILQ_INSERT_TAIL(&ttylist, tp, tty_link);
	++tty_count;
}

/*
 * Remove a tty from the tty list.
 */
void
tty_detach(tp)
	struct tty *tp;
{

	--tty_count;
#ifdef DIAGNOSTIC
	if (tty_count < 0)
		panic("tty_detach: tty_count < 0");
#endif
	TAILQ_REMOVE(&ttylist, tp, tty_link);
}

/*
 * Allocate a tty structure and its associated buffers.
 */
struct tty *
ttymalloc()
{
	struct tty *tp;

	MALLOC(tp, struct tty *, sizeof(struct tty), M_TTYS, M_WAITOK);
	bzero(tp, sizeof *tp);
	/* XXX: default to 1024 chars for now */
	clalloc(&tp->t_rawq, 1024, 1);
	clalloc(&tp->t_canq, 1024, 1);
	/* output queue doesn't need quoting */
	clalloc(&tp->t_outq, 1024, 0);
	return(tp);
}

/*
 * Free a tty structure and its buffers.
 *
 * Be sure to call tty_detach() for any tty that has been
 * tty_attach()ed.
 */
void
ttyfree(tp)
	struct tty *tp;
{

	clfree(&tp->t_rawq);
	clfree(&tp->t_canq);
	clfree(&tp->t_outq);
	FREE(tp, M_TTYS);
}
