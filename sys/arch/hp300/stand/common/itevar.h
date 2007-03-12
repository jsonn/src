/*	$NetBSD: itevar.h,v 1.4.26.1 2007/03/12 05:47:58 rmind Exp $	*/

/*
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
 * from: Utah $Hdr: itevar.h 1.15 92/12/20$
 *
 *	@(#)itevar.h	8.1 (Berkeley) 6/10/93
 */
/*
 * Copyright (c) 1988 University of Utah.
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
 * from: Utah $Hdr: itevar.h 1.15 92/12/20$
 *
 *	@(#)itevar.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Standalone version of hp300 ITE.
 */

#define ITEUNIT(dev)       minor(dev)

#define getbyte(ip, offset) \
	((*(ip)->isw->ite_readbyte)(ip, offset))

#define getword(ip, offset) \
	((getbyte(ip, offset) << 8) | getbyte(ip, (offset) + 2))

#define writeglyph(ip, offset, fontbuf) \
	((*(ip)->isw->ite_writeglyph)((ip), (offset), (fontbuf)))

struct ite_data {
	int	flags;
	struct	tty *tty;
	struct  itesw *isw;
	struct  grf_data *grf;
	void 	*regbase, *fbbase;
	short	curx, cury;
	short   cursorx, cursory;
	short   cblankx, cblanky;
	short	rows, cols;
	short   cpl;
	short	dheight, dwidth;
	short	fbheight, fbwidth;
	short	ftheight, ftwidth;
	short	fontx, fonty;
	short   attribute;
	u_char	*attrbuf;
	short	planemask;
	short	pos;
	char	imode, escape, fpd, hold;
	void *	devdata;			/* display dependent data */
};

struct itesw {
	int	ite_hwid;
	void	(*ite_init)(struct ite_data *);
	void	(*ite_deinit)(struct ite_data *);
	void	(*ite_clear)(struct ite_data *, int, int, int, int);
	void	(*ite_putc)(struct ite_data *, int, int, int, int);
	void	(*ite_cursor)(struct ite_data *, int);
	void	(*ite_scroll)(struct ite_data *, int, int, int, int);
	u_char	(*ite_readbyte)(struct ite_data *, int);
	void	(*ite_writeglyph)(struct ite_data *, u_char *, u_char *);
};

/* Flags */
#define ITE_ALIVE	0x01	/* hardware exists */
#define ITE_INITED	0x02	/* device has been initialized */
#define ITE_CONSOLE	0x04	/* device can be console */
#define ITE_ISCONS	0x08	/* device is console */
#define ITE_ACTIVE	0x10	/* device is being used as ITE */
#define ITE_INGRF	0x20	/* device in use as non-ITE */
#define ITE_CURSORON	0x40	/* cursor being tracked */

#define attrloc(ip, y, x) \
	(ip->attrbuf + ((y) * ip->cols) + (x))

#define attrclr(ip, sy, sx, h, w) \
	bzero(ip->attrbuf + ((sy) * ip->cols) + (sx), (h) * (w))
  
#define attrmov(ip, sy, sx, dy, dx, h, w) \
	bcopy(ip->attrbuf + ((sy) * ip->cols) + (sx), \
	      ip->attrbuf + ((dy) * ip->cols) + (dx), \
	      (h) * (w))

#define attrtest(ip, attr) \
	((* (u_char *) attrloc(ip, ip->cury, ip->curx)) & attr)

#define attrset(ip, attr) \
	((* (u_char *) attrloc(ip, ip->cury, ip->curx)) = attr)
  
/*
 * X and Y location of character 'c' in the framebuffer, in pixels.
 */
#define	charX(ip,c)	\
	(((c) % (ip)->cpl) * (ip)->ftwidth + (ip)->fontx)

#define	charY(ip,c)	\
	(((c) / (ip)->cpl) * (ip)->ftheight + (ip)->fonty)

/*
 * The cursor is just an inverted space.
 */
#define draw_cursor(ip) { \
	WINDOWMOVER(ip, ip->cblanky, ip->cblankx, \
		    ip->cury * ip->ftheight, \
		    ip->curx * ip->ftwidth, \
		    ip->ftheight, ip->ftwidth, RR_XOR); \
        ip->cursorx = ip->curx; \
	ip->cursory = ip->cury; }

#define erase_cursor(ip) \
  	WINDOWMOVER(ip, ip->cblanky, ip->cblankx, \
		    ip->cursory * ip->ftheight, \
		    ip->cursorx * ip->ftwidth, \
		    ip->ftheight, ip->ftwidth, RR_XOR);

/* Character attributes */
#define ATTR_NOR        0x0             /* normal */
#define	ATTR_INV	0x1		/* inverse */
#define	ATTR_UL		0x2		/* underline */
#define ATTR_ALL	(ATTR_INV | ATTR_UL)

/* Keyboard attributes */
#define ATTR_KPAD	0x4		/* keypad transmit */
  
/* Replacement Rules */
#define RR_CLEAR		0x0
#define RR_COPY			0x3
#define RR_XOR			0x6
#define RR_COPYINVERTED  	0xc

#define SCROLL_UP	0x01
#define SCROLL_DOWN	0x02
#define SCROLL_LEFT	0x03
#define SCROLL_RIGHT	0x04
#define DRAW_CURSOR	0x05
#define ERASE_CURSOR    0x06
#define MOVE_CURSOR	0x07

#define KBD_SSHIFT	4		/* bits to shift status */
#define	KBD_CHARMASK	0x7F

/* keyboard status */
#define	KBD_SMASK	0xF		/* service request status mask */
#define	KBD_CTRLSHIFT	0x8		/* key + CTRL + SHIFT */
#define	KBD_CTRL	0x9		/* key + CTRL */
#define	KBD_SHIFT	0xA		/* key + SHIFT */
#define	KBD_KEY		0xB		/* key only */

#define KBD_CAPSLOCK    0x18

#define KBD_EXT_LEFT_DOWN     0x12
#define KBD_EXT_LEFT_UP       0x92
#define KBD_EXT_RIGHT_DOWN    0x13
#define KBD_EXT_RIGHT_UP      0x93

#define	TABSIZE		8
#define	TABEND(ip)	((ip)->tty->t_winsize.ws_col - TABSIZE)

extern	struct ite_data ite_data[];
extern	struct itesw itesw[];
extern	int nitesw;

/*
 * Prototypes.
 */
u_char ite_readbyte(struct ite_data *, int);
void ite_writeglyph(struct ite_data *, u_char *, u_char *);
void ite_fontinfo(struct ite_data *);
void ite_fontinit(struct ite_data *);

/*
 * Framebuffer-specific ITE prototypes.
 */
void topcat_init(struct ite_data *);
void topcat_clear(struct ite_data *, int, int, int, int);
void topcat_putc(struct ite_data *, int, int, int, int);
void topcat_cursor(struct ite_data *, int);
void topcat_scroll(struct ite_data *, int, int, int, int);

void gbox_init(struct ite_data *);
void gbox_clear(struct ite_data *, int, int, int, int);
void gbox_putc(struct ite_data *, int, int, int, int);
void gbox_cursor(struct ite_data *, int);
void gbox_scroll(struct ite_data *, int, int, int, int);

void rbox_init(struct ite_data *);
void rbox_clear(struct ite_data *, int, int, int, int);
void rbox_putc(struct ite_data *, int, int, int, int);
void rbox_cursor(struct ite_data *, int);
void rbox_scroll(struct ite_data *, int, int, int, int);

void dvbox_init(struct ite_data *);
void dvbox_clear(struct ite_data *, int, int, int, int);
void dvbox_putc(struct ite_data *, int, int, int, int);
void dvbox_cursor(struct ite_data *, int);
void dvbox_scroll(struct ite_data *, int, int, int, int);

void hyper_init(struct ite_data *);
void hyper_clear(struct ite_data *, int, int, int, int);
void hyper_putc(struct ite_data *, int, int, int, int);
void hyper_cursor(struct ite_data *, int);
void hyper_scroll(struct ite_data *, int, int, int, int);
