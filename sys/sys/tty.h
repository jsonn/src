/*	$NetBSD: tty.h,v 1.30.4.1 1996/06/02 09:08:13 mrg Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)tty.h	8.6 (Berkeley) 1/21/94
 */

#include <sys/termios.h>
#include <sys/select.h>		/* For struct selinfo. */
#include <sys/queue.h>

#ifndef REAL_CLISTS
/*
 * Clists are actually ring buffers. The c_cc, c_cf, c_cl fields have
 * exactly the same behaviour as in true clists.
 * if c_cq is NULL, the ring buffer has no TTY_QUOTE functionality
 * (but, saves memory and cpu time)
 *
 * *DON'T* play with c_cs, c_ce, c_cq, or c_cl outside tty_subr.c!!!
 */
struct clist {
	int	c_cc;		/* count of characters in queue */
	int	c_cn;		/* total ring buffer length */
	u_char	*c_cf;		/* points to first character */
	u_char	*c_cl;		/* points to next open character */
	u_char	*c_cs;		/* start of ring buffer */
	u_char	*c_ce;		/* c_ce + c_len */
	u_char	*c_cq;		/* N bits/bytes long, see tty_subr.c */
};
#else
/*
 * Clists are character lists, which is a variable length linked list
 * of cblocks, with a count of the number of characters in the list.
 */
struct clist {
	int	c_cc;		/* Number of characters in the clist. */
	u_char	*c_cf;		/* Pointer to the first cblock. */
	u_char	*c_cl;		/* Pointer to the last cblock. */
};
#endif

/*
 * Per-tty structure.
 *
 * Should be split in two, into device and tty drivers.
 * Glue could be masks of what to echo and circular buffer
 * (low, high, timeout).
 */
struct tty {
	TAILQ_ENTRY(tty) tty_link;	/* Link in global tty list. */
	struct	clist t_rawq;		/* Device raw input queue. */
	long	t_rawcc;		/* Raw input queue statistics. */
	struct	clist t_canq;		/* Device canonical queue. */
	long	t_cancc;		/* Canonical queue statistics. */
	struct	clist t_outq;		/* Device output queue. */
	long	t_outcc;		/* Output queue statistics. */
	u_char	t_line;			/* Interface to device drivers. */
	dev_t	t_dev;			/* Device. */
	int	t_state;		/* Device and driver (TS*) state. */
	int	t_flags;		/* Tty flags. */
	struct	pgrp *t_pgrp;		/* Foreground process group. */
	struct	session *t_session;	/* Enclosing session. */
	struct	selinfo t_rsel;		/* Tty read/oob select. */
	struct	selinfo t_wsel;		/* Tty write select. */
	struct	termios t_termios;	/* Termios state. */
	struct	winsize t_winsize;	/* Window size. */
					/* Start output. */
	void	(*t_oproc) __P((struct tty *));
					/* Set hardware state. */
	int	(*t_param) __P((struct tty *, struct termios *));
					/* Set hardware flow control. */
	int	(*t_hwiflow) __P((struct tty *tp, int flag));
	void	*t_sc;			/* XXX: net/if_sl.c:sl_softc. */
	short	t_column;		/* Tty output column. */
	short	t_rocount, t_rocol;	/* Tty. */
	short	t_hiwat;		/* High water mark. */
	short	t_lowat;		/* Low water mark. */
	short	t_gen;			/* Generation number. */
};

#define	t_cc		t_termios.c_cc
#define	t_cflag		t_termios.c_cflag
#define	t_iflag		t_termios.c_iflag
#define	t_ispeed	t_termios.c_ispeed
#define	t_lflag		t_termios.c_lflag
#define	t_min		t_termios.c_min
#define	t_oflag		t_termios.c_oflag
#define	t_ospeed	t_termios.c_ospeed
#define	t_time		t_termios.c_time

#define	TTIPRI	25			/* Sleep priority for tty reads. */
#define	TTOPRI	26			/* Sleep priority for tty writes. */

#define	TTMASK	15
#define	OBUFSIZ	100
#define	TTYHOG	1024

#ifdef _KERNEL
#define	TTMAXHIWAT	roundup(2048, CBSIZE)
#define	TTMINHIWAT	roundup(100, CBSIZE)
#define	TTMAXLOWAT	256
#define	TTMINLOWAT	32
#endif

/* These flags are kept in t_state. */
#define	TS_ASLEEP	0x00001		/* Process waiting for tty. */
#define	TS_ASYNC	0x00002		/* Tty in async I/O mode. */
#define	TS_BUSY		0x00004		/* Draining output. */
#define	TS_CARR_ON	0x00008		/* Carrier is present. */
#define	TS_FLUSH	0x00010		/* Outq has been flushed during DMA. */
#define	TS_ISOPEN	0x00020		/* Open has completed. */
#define	TS_TBLOCK	0x00040		/* Further input blocked. */
#define	TS_TIMEOUT	0x00080		/* Wait for output char processing. */
#define	TS_TTSTOP	0x00100		/* Output paused. */
#define	TS_WOPEN	0x00200		/* Open in progress. */
#define	TS_XCLUDE	0x00400		/* Tty requires exclusivity. */

/* State for intra-line fancy editing work. */
#define	TS_BKSL		0x00800		/* State for lowercase \ work. */
#define	TS_CNTTB	0x01000		/* Counting tab width, ignore FLUSHO. */
#define	TS_ERASE	0x02000		/* Within a \.../ for PRTRUB. */
#define	TS_LNCH		0x04000		/* Next character is literal. */
#define	TS_TYPEN	0x08000		/* Retyping suspended input (PENDIN). */
#define	TS_LOCAL	(TS_BKSL | TS_CNTTB | TS_ERASE | TS_LNCH | TS_TYPEN)

/* Character type information. */
#define	ORDINARY	0
#define	CONTROL		1
#define	BACKSPACE	2
#define	NEWLINE		3
#define	TAB		4
#define	VTAB		5
#define	RETURN		6

struct speedtab {
	int sp_speed;			/* Speed. */
	int sp_code;			/* Code. */
};

/* Modem control commands (driver). */
#define	DMSET		0
#define	DMBIS		1
#define	DMBIC		2
#define	DMGET		3

/* Flags on a character passed to ttyinput. */
#define	TTY_CHARMASK	0x000000ff	/* Character mask */
#define	TTY_QUOTE	0x00000100	/* Character quoted */
#define	TTY_ERRORMASK	0xff000000	/* Error mask */
#define	TTY_FE		0x01000000	/* Framing error or BREAK condition */
#define	TTY_PE		0x02000000	/* Parity error */

/* Is tp controlling terminal for p? */
#define	isctty(p, tp)							\
	((p)->p_session == (tp)->t_session && (p)->p_flag & P_CONTROLT)

/* Is p in background of tp? */
#define	isbackground(p, tp)						\
	(isctty((p), (tp)) && (p)->p_pgrp != (tp)->t_pgrp)

/*
 * ttylist_head is defined here so that user-land has access to it.
 */
TAILQ_HEAD(ttylist_head, tty);		/* the ttylist is a TAILQ */

#ifdef _KERNEL

extern	int tty_count;			/* number of ttys in global ttylist */
extern	struct ttychars ttydefaults;

/* Symbolic sleep message strings. */
extern	 char ttyin[], ttyout[], ttopen[], ttclos[], ttybg[], ttybuf[];

int	 b_to_q __P((u_char *cp, int cc, struct clist *q));
void	 catq __P((struct clist *from, struct clist *to));
void	 clist_init __P((void));
int	 getc __P((struct clist *q));
void	 ndflush __P((struct clist *q, int cc));
int	 ndqb __P((struct clist *q, int flag));
u_char	*nextc __P((struct clist *q, u_char *cp, int *c));
int	 putc __P((int c, struct clist *q));
int	 q_to_b __P((struct clist *q, u_char *cp, int cc));
int	 unputc __P((struct clist *q));

int	 nullmodem __P((struct tty *tp, int flag));
int	 tputchar __P((int c, struct tty *tp));
int	 ttioctl __P((struct tty *tp, u_long com, caddr_t data, int flag,
	    struct proc *p));
int	 ttread __P((struct tty *tp, struct uio *uio, int flag));
void	 ttrstrt __P((void *tp));
int	 ttselect __P((dev_t device, int rw, struct proc *p));
void	 ttsetwater __P((struct tty *tp));
int	 ttspeedtab __P((int speed, struct speedtab *table));
int	 ttstart __P((struct tty *tp));
void	 ttwakeup __P((struct tty *tp));
int	 ttwrite __P((struct tty *tp, struct uio *uio, int flag));
void	 ttychars __P((struct tty *tp));
int	 ttycheckoutq __P((struct tty *tp, int wait));
int	 ttyclose __P((struct tty *tp));
void	 ttyflush __P((struct tty *tp, int rw));
void	 ttyinfo __P((struct tty *tp));
int	 ttyinput __P((int c, struct tty *tp));
int	 ttylclose __P((struct tty *tp, int flag));
int	 ttymodem __P((struct tty *tp, int flag));
int	 ttyopen __P((dev_t device, struct tty *tp));
int	 ttyoutput __P((int c, struct tty *tp));
void	 ttypend __P((struct tty *tp));
void	 ttyretype __P((struct tty *tp));
void	 ttyrub __P((int c, struct tty *tp));
int	 ttysleep __P((struct tty *tp,
	    void *chan, int pri, char *wmesg, int timeout));
int	 ttywait __P((struct tty *tp));
int	 ttywflush __P((struct tty *tp));

void	tty_init __P((void));
void	tty_attach __P((struct tty *));
void	tty_detach __P((struct tty *));
struct tty *ttymalloc __P((void));
void	 ttyfree __P((struct tty *));
u_char	*firstc           __P((struct clist *clp, int *c));

int	cttyopen __P((dev_t, int, int, struct proc *));
int	cttyread __P((dev_t, struct uio *, int));
int	cttywrite __P((dev_t, struct uio *, int));
int	cttyioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	cttyselect __P((dev_t, int, struct proc *));

int	clalloc __P((struct clist *, int, int));
void	clfree __P((struct clist *));

#if defined(COMPAT_43) || defined(COMPAT_SUNOS) || defined(COMPAT_SVR4) || \
    defined(COMPAT_FREEBSD)
# define COMPAT_OLDTTY
int 	ttcompat __P((struct tty *, u_long, caddr_t, int, struct proc *));
#endif

#endif
