/*	$NetBSD: ww.h,v 1.17.42.1 2009/05/13 19:20:12 jym Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	@(#)ww.h	8.1 (Berkeley) 6/6/93
 */

#ifndef __WW_H__
#define __WW_H__

#include <sys/types.h>
#ifdef OLD_TTY
#include <sgtty.h>
#else
#include <termios.h>
#endif
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#ifndef EXTERN
#define EXTERN extern
#endif

#define NWW	30		/* maximum number of windows */

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

	/* a rectangle */
struct ww_dim {
	int nr;			/* number of rows */
	int nc;			/* number of columns */
	int t, b;		/* top, bottom */
	int l, r;		/* left, right */
};

	/* a coordinate */
struct ww_pos {
	int r;			/* row */
	int c;			/* column */
};

	/* the window structure */
struct ww {
	int ww_flags;

		/* general flags and states */
	int ww_state;		/* state of window */
#define WWS_INITIAL	0	/* just opened */
#define WWS_HASPROC	1	/* has process on pty */
#define WWS_DEAD	3	/* child died */
#define	ww_oflags	ww_flags
#define WWO_REVERSE	0x0001	/* make it all reverse video */
#define WWO_GLASS	0x0002	/* make it all glass */
#define WWO_FRAME	0x0004	/* this is a frame window */
#define	WWO_ALLFLAGS	0x0007

		/* information for overlap */
	struct ww *ww_forw;	/* doubly linked list, for overlapping info */
	struct ww *ww_back;
	unsigned char ww_index;	/* the window index, for wwindex[] */
#define WWX_NOBODY	NWW
	int ww_order;		/* the overlapping order */

		/* sizes and positions */
	struct ww_dim ww_w;	/* window size and pos */
	struct ww_dim ww_b;	/* buffer size and pos */
	struct ww_dim ww_i;	/* the part inside the screen */
	struct ww_pos ww_cur;	/* the cursor position, relative to ww_w */

		/* arrays */
	char **ww_win;		/* the window */
	union ww_char **ww_buf;	/* the buffer */
	char **ww_fmap;		/* map for frame and box windows */
	short *ww_nvis;		/* how many ww_buf chars are visible per row */

		/* information for wwwrite() and company */
	int ww_wstate;		/* state for outputting characters */
	char ww_modes;		/* current display modes */
#define	ww_wflags	ww_flags
#define	WWW_INSERT	0x0008	/* insert mode */
#define	WWW_MAPNL	0x0010	/* map \n to \r\n */
#define	WWW_NOUPDATE	0x0020	/* don't do updates in wwwrite() */
#define	WWW_UNCTRL	0x0040	/* expand control characters */
#define	WWW_NOINTR	0x0080	/* wwwrite() not interruptable */
#define	WWW_HASCURSOR	0x0100	/* has fake cursor */

		/* things for the window process and io */
	int ww_type;
#define	WWT_PTY		0	/* pty */
#define	WWT_SOCKET	1	/* socket pair */
#define	WWT_INTERNAL	2
#define	ww_pflags	ww_flags
#define	WWP_STOPPED	0x0200	/* output stopped */
	int ww_pty;		/* file descriptor of pty or socket pair */
	int ww_socket;		/* other end of socket pair */
	int ww_pid;		/* pid of process, if WWS_HASPROC true */
	char ww_ttyname[11];	/* "/dev/ttyp?" */
	char *ww_ob;		/* output buffer */
	char *ww_obe;		/* end of ww_ob */
	char *ww_obp;		/* current read position in ww_ob */
	char *ww_obq;		/* current write position in ww_ob */

		/* things for the user, they really don't belong here */
	int ww_id;		/* the user window id */
#define	ww_uflags	ww_flags
#define	WWU_CENTER	0x0400	/* center the label */
#define	WWU_HASFRAME	0x0800	/* frame it */
#define	WWU_KEEPOPEN	0x1000	/* keep it open after the process dies */
#define	WWU_ALLFLAGS	0x1c00
	char *ww_label;		/* the user supplied label */
	struct ww_dim ww_alt;	/* alternate position and size */
};

	/* state of a tty */
struct ww_tty {
#ifdef OLD_TTY
	struct sgttyb ww_sgttyb;
	struct tchars ww_tchars;
	struct ltchars ww_ltchars;
	int ww_lmode;
	int ww_ldisc;
#else
	struct termios ww_termios;
#endif
};

union ww_char {
	short c_w;		/* as a word */
	struct {
#if BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == PDP_ENDIAN
		char C_c;	/* the character part */
		char C_m;	/* the mode part */
#endif
#if BYTE_ORDER == BIG_ENDIAN
		char C_m;	/* the mode part */
		char C_c;	/* the character part */
#endif
	} c_un;
};
#define c_c c_un.C_c
#define c_m c_un.C_m

	/* for display update */
struct ww_update {
	int best_gain;
	int best_col;
	int gain;
};

	/* parts of ww_char */
#define WWC_CMASK	0x00ff
#define WWC_MMASK	0xff00
#define WWC_MSHIFT	8

	/* c_m bits */
#define WWM_REV		0x01	/* reverse video */
#define WWM_BLK		0x02	/* blinking */
#define WWM_UL		0x04	/* underlined */
#define WWM_GRP		0x08	/* graphics */
#define WWM_DIM		0x10	/* half intensity */
#define WWM_USR		0x20	/* user specified mode */
#define WWM_GLS		0x40	/* window only, glass, i.e., transparent */

	/* flags for ww_fmap */
#define WWF_U		0x01
#define WWF_R		0x02
#define WWF_D		0x04
#define WWF_L		0x08
#define WWF_MASK	(WWF_U|WWF_R|WWF_D|WWF_L)
#define WWF_LABEL	0x40
#define WWF_TOP		0x80

	/* error codes */
#define WWE_NOERR	0
#define WWE_SYS		1		/* system error */
#define WWE_NOMEM	2		/* out of memory */
#define WWE_TOOMANY	3		/* too many windows */
#define WWE_NOPTY	4		/* no more ptys */
#define WWE_SIZE	5		/* bad window size */
#define WWE_BADTERM	6		/* bad terminal type */
#define WWE_CANTDO	7		/* dumb terminal */

	/* wwtouched[] bits, there used to be more than one */
#define WWU_TOUCHED	0x01		/* touched */

	/* the window structures */
EXTERN struct ww wwhead;
EXTERN struct ww *wwindex[NWW + 1];	/* last location is for wwnobody */
EXTERN struct ww wwnobody;

	/* tty things */
EXTERN struct ww_tty wwoldtty;		/* the old (saved) terminal settings */
EXTERN struct ww_tty wwnewtty;		/* the new (current) terminal settings */
EXTERN struct ww_tty wwwintty;		/* the terminal settings for windows */
EXTERN char *wwterm;			/* the terminal name */
EXTERN char wwtermcap[1024];		/* place for the termcap */

	/* generally useful variables */
EXTERN int wwnrow, wwncol;		/* the screen size */
EXTERN char wwavailmodes;		/* actually supported modes */
EXTERN char wwcursormodes;		/* the modes for the fake cursor */
EXTERN char wwwrap;			/* terminal has auto wrap around */
EXTERN int wwdtablesize;		/* result of getdtablesize() call */
EXTERN unsigned char **wwsmap;		/* the screen map */
EXTERN union ww_char **wwos;		/* the old (current) screen */
EXTERN union ww_char **wwns;		/* the new (desired) screen */
EXTERN union ww_char **wwcs;		/* the checkpointed screen */
EXTERN char *wwtouched;			/* wwns changed flags */
EXTERN struct ww_update *wwupd;		/* for display update */
EXTERN int wwospeed;			/* output baud rate, copied from wwoldtty */
EXTERN int wwbaud;			/* wwospeed converted into actual number */
EXTERN int wwcursorrow, wwcursorcol;	/* where we want the cursor to be */
EXTERN int wwerrno;			/* error number */

	/* statistics */
EXTERN int wwnflush, wwnwr, wwnwre, wwnwrz, wwnwrc;
EXTERN int wwnwwr, wwnwwra, wwnwwrc;
EXTERN int wwntokdef, wwntokuse, wwntokbad, wwntoksave, wwntokc;
EXTERN int wwnupdate, wwnupdline, wwnupdmiss;
EXTERN int wwnupdscan, wwnupdclreol, wwnupdclreos, wwnupdclreosmiss, wwnupdclreosline;
EXTERN int wwnread, wwnreade, wwnreadz;
EXTERN int wwnreadc, wwnreadack, wwnreadnack, wwnreadstat, wwnreadec;
EXTERN int wwnwread, wwnwreade, wwnwreadz, wwnwreadd, wwnwreadc, wwnwreadp;
EXTERN int wwnselect, wwnselecte, wwnselectz;

	/* quicky macros */
#define wwsetcursor(r,c) (wwcursorrow = (r), wwcursorcol = (c))
#define wwcurtowin(w)	wwsetcursor((w)->ww_cur.r, (w)->ww_cur.c)
#define wwunbox(w)	wwunframe(w)
#define wwclreol(w,r,c)	wwclreol1((w), (r), (c), 0)
#define wwredrawwin(w)	wwredrawwin1((w), (w)->ww_i.t, (w)->ww_i.b, 0)
#define wwupdate()	wwupdate1(0, wwnrow);

	/* things for handling input */
EXTERN struct ww *wwcurwin;	/* window to copy input into */
EXTERN char *wwib;		/* input (keyboard) buffer */
EXTERN char *wwibe;		/* wwib + sizeof buffer */
EXTERN char *wwibp;		/* current read position in buffer */
EXTERN char *wwibq;		/* current write position in buffer */
#define wwmaskc(c)	((c) & 0x7f)
#define wwgetc()	(wwibp < wwibq ? wwmaskc(*wwibp++) : -1)
#define wwpeekc()	(wwibp < wwibq ? wwmaskc(*wwibp) : -1)
#define wwungetc(c)	(wwibp > wwib ? *--wwibp = (c) : -1)

	/* things for short circuiting wwiomux() */
EXTERN char wwintr;		/* interrupting */
EXTERN char wwsetjmp;		/* want a longjmp() from wwrint() and wwchild() */
EXTERN jmp_buf wwjmpbuf;	/* jmpbuf for above */
#define wwinterrupt()	wwintr
#define wwsetintr()	do { wwintr = 1; if (wwsetjmp) longjmp(wwjmpbuf, 1); } \
			while (0)
#define wwclrintr()	(wwintr = 0)

	/* checkpointing */
EXTERN int wwdocheckpoint;

	/* the window virtual terminal */
#define WWT_TERM	"window-v2"
#define WWT_TERMCAP	"WW|window-v2|window program version 2:\
	:am:bs:da:db:ms:pt:cr=^M:nl=^J:bl=^G:ta=^I:\
	:cm=\\EY%+ %+ :le=^H:nd=\\EC:up=\\EA:do=\\EB:ho=\\EH:\
	:cd=\\EJ:ce=\\EK:cl=\\EE:me=\\Er^?:"
#define WWT_REV		"se=\\ErA:so=\\EsA:mr=\\EsA:"
#define WWT_BLK		"BE=\\ErB:BS=\\EsB:mb=\\EsB:"
#define WWT_UL		"ue=\\ErD:us=\\EsD:"
#define WWT_GRP		"ae=\\ErH:as=\\EsH:"
#define WWT_DIM		"HE=\\ErP:HS=\\EsP:mh=\\EsP:"
#define WWT_USR		"XE=\\Er`:XS=\\Es`:"
#define WWT_ALDL	"al=\\EL:dl=\\EM:"
#define WWT_IMEI	"im=\\E@:ei=\\EO:ic=:mi:" /* XXX, ic for emacs bug */
#define WWT_IC		"ic=\\EP:"
#define WWT_DC		"dc=\\EN:"
EXTERN char wwwintermcap[1024];	/* terminal-specific but window-independent
				   part of the window termcap */
#ifdef TERMINFO
	/* where to put the temporary terminfo directory */
EXTERN char wwterminfopath[1024];
#endif

struct ww *wwopen(int, int, int, int, int, int, int);
void	wwadd(struct ww *, struct ww *);
void	wwaddcap(const char *, char **);
void	wwaddcap1(const char *, char **);
void	wwalarm(int);
char  **wwalloc(int, int, int, int, int);
void	wwbell(void);
void	wwbox(struct ww *, int, int, int, int);
void	wwcheckpoint(void);
void	wwchild(int);
void	wwclose(struct ww *);
void	wwclreol1(struct ww *, int, int, char);
void	wwclreos(struct ww *, int, int);
void	wwcopyscreen(union ww_char **s1, union ww_char **s2);
void	wwcursor(struct ww *, int);
void	wwdelchar(struct ww *, int, int);
void	wwdelete(struct ww *);
void	wwdelete1(struct ww *, int, int, int, int);
void	wwdelline(struct ww *, int);
void	wwdumpns(void);
void	wwdumpnvis(struct ww *);
void	wwdumpos(void);
void	wwdumpsmap(void);
void	wwdumpwin(struct ww *);
void	wwend(int);
int	wwenviron(struct ww *);
const char *
	wwerror(void);
void	wwflush(void);
void	wwframe(struct ww *, struct ww *);
void	wwframec(struct ww *, int, int, char);
void	wwfree(char **, int);
int	wwgetpty(struct ww *);
int	wwgettty(int, struct ww_tty *);
int	wwgetttysize(int, int *, int *);
void	wwgets(char *, int, struct ww *);
int	wwinit(void);
void	wwinschar(struct ww *, int, int, char, char);
void	wwinsline(struct ww *, int);
void	wwiomux(void);
void	wwlabel(struct ww *, struct ww *, int, char *, int);
void	wwmove(struct ww *, int, int);
void	wwprintf(struct ww *, const char *, ...)
    __attribute__((__format__(__printf__, 2, 3)));
void	wwputc(char, struct ww *);
void	wwputs(const char *, struct ww *);
void	wwredraw(void);
void	wwredrawwin1(struct ww *,int, int, int);
void	wwquit(int);
void	wwreset(void);
void	wwrint(void);
void	wwscroll(struct ww *, int);
int	wwscroll1(struct ww *, int, int, int, int);
void	wwsetcursormodes(int);
int	wwsettty(int, struct ww_tty *);
int	wwsetttysize(int, int, int);
int	wwsize(struct ww *, int, int);
int	wwspawn(struct ww *, char *, char **);
void	wwstart(void);
void	wwstart1(void);
int	wwstarttty(int);
int	wwstoptty(int);
void	wwsuspend(void);
void	wwunframe(struct ww *);
void	wwupdate1(int, int);
int	wwvisible(struct ww *);
void	wwvprintf(struct ww *, const char *, va_list);
int	wwwrite(struct ww *, const char *, int);
#ifdef TERMINFO
int	wwterminfoinit(void);
int	wwterminfoend(void);
#endif

#undef MIN
#undef MAX
#define MIN(x, y)	((x) > (y) ? (y) : (x))
#define MAX(x, y)	((x) > (y) ? (x) : (y))

#endif /* __WW_H__ */
