/*	$NetBSD: vt100.c,v 1.3.14.1 2000/11/20 20:17:22 bouyer Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sony Corp. and Kazumasa Utashiro of Software Research Associates, Inc.
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
 * from: $Hdr: vt100.c,v 4.300 91/06/09 06:14:56 root Rel41 $ SONY
 *
 *	@(#)vt100.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <machine/framebuf.h>
#include <machine/keyboard.h>
#include <newsmips/dev/kbreg.h>
#include <newsmips/dev/fbdefs.h>
#include <newsmips/dev/vt100.h>
#include <newsmips/dev/bitmapif.h>
#define spl4 cpu_spl1
extern int cpu_spl1();

#ifdef IPC_MRX
#include "config.h"
#define kbd_ioctl(chan, cmd, argp) { \
	if (kb_ioctl) \
		(*kb_ioctl)(chan, cmd, argp); \
}
#endif

#ifdef IPC_MRX
#include "mrx.h"
#include "process.h"
#include "object.h"
#include "console.h"
#endif

#ifdef CPU_SINGLE
#include <newsmips/dev/scc.h>
#endif

extern Key_string key_str;
extern int tmode;
static unsigned int first_code;

#ifdef IPC_MRX
#define SCC_KEYBOARD	0
#endif

SCREEN screen;
struct cursor inner_buf_csr;
int inner_buf_tstat;
char c_pos_mess[C_MESS_SIZ];
extern struct csr_buf local_csr_buf;

#ifdef IPC_MRX
int bitmap_use;
#ifdef IPC_3CPU
#include "../../ubdev/msgio.h"
extern int ipc_ready;
#endif /* IPC_3CPU */
extern struct cons_devsw vt100_cons;
#endif /* IPC_MRX */

#ifdef CPU_DOUBLE
int auto_dimmer();
#endif

#if CPU_SINGLE
extern int hz;
extern int kbd_profun_init();
#endif

extern void esc_store_csr __P((SCREEN *));
extern int vt100_write __P((int, char *, int));
extern int kbd_open __P((int));
extern void vt_flush __P((struct cursor *));
extern void	recover __P((SCREEN *));
extern void	jiskanji __P((SCREEN *, u_int));
extern void	esc_index __P((SCREEN *));

lRectangle char_r1;
lRectangle font_r1;
lRectangle char_r2;
lRectangle font_r2;

int	font_len1;
int	font_len2;

int	fcolor;
int	bcolor;

int	font_w;
int	font_h;
int	char_w;
int	char_h;
int	scr_w;
int	scr_h;
int	ch_pos;
int	ul_pos;
int	x_ofst;
int	y_ofst;
int	rit_m;
int	btm_m;
int	bell_len;
int	dim_cnt;
int	a_dim_on;

unsigned short fbuf[256];
int	fp;
int	fpn;
lPoint	fpp;
int	fpa;

static struct callout auto_dimmer_ch = CALLOUT_INITIALIZER;

void
vt100init()
{
	register int i;
	register SCREEN	*sp = &screen;

	sp->s_term_mode = 0;
	sp->s_term_mode |= (SRM|DECSCLM|DECAWM|DECARM|DECCSR_ACTV);
	sp->s_current_stat = 0;
	sp->s_csr.csr_x = 1;
	sp->s_csr.csr_y = 1;
	sp->s_csr.csr_p.x = x_ofst;
	sp->s_csr.csr_p.y = y_ofst;
	sp->s_csr.csr_attributes = NORMALM;
	sp->s_region.top_margin = TOP_M;
	sp->s_region.btm_margin = btm_m;
	sp->s_plane = consfb->planemask;
	sp->s_bgcol = 0;
	fcolor = sp->s_plane;
	bcolor = sp->s_bgcol;
	for (i = 0; i < RIT_M_MAX; i++)
		sp->s_tab_pos[i] = 0;
	for (i = 9; i < RIT_M_MAX; i +=8)
		sp->s_tab_pos[i] = 1;

	esc_store_csr(sp);
	inner_buf_tstat = sp->s_term_mode & (DECOM|DECAWM);
	local_csr_buf.csr_number = 1;

	cursor_on(&sp->s_csr.csr_p);
}

void
ncp_str(p, q, n)
	register char *p, *q;
	register int n;
{
	while (n-- > 0)
		*q++ = *p++;
}

/*
 *  default parameter set
 */
void
set_default_param()
{
	register struct fbdev *cfb = consfb;

	font_w = cfb->font_w;
	font_h = cfb->font_h;
	char_w = cfb->char_w;
	char_h = cfb->char_h;
	scr_w  = cfb->scr_w;
	scr_h  = cfb->scr_h;
	ch_pos = cfb->ch_pos;
	ul_pos = cfb->ul_pos;
	x_ofst = cfb->x_offset;
	y_ofst = cfb->y_offset;
	rit_m  = cfb->rit_m;
	btm_m  = cfb->btm_m;
	a_dim_on = 1;

	font_r1.extent.x = font_w;
	font_r1.extent.y = font_h;
	font_r2.extent.x = font_w * 2;
	font_r2.extent.y = font_h;
	font_len1 = (font_w + 0x0f) >> 4;
	font_len2 = (font_w*2 + 0x0f) >> 4;
	char_r1.extent.x = char_w;
	char_r1.extent.y = char_h;
	char_r2.extent.x = char_w * 2;
	char_r2.extent.y = char_h;

	dim_cnt = DIM_CNT_DFLT;
	bell_len = BELL_LEN_DFLT;
}

void
vt100_open()
{
	static int only_one = 0;

	set_default_param();
	vt100init();
	bitmapinit();
	if (only_one == 0) {
#ifdef IPC_MRX
#ifdef IPC_3CPU
		while (ipc_ready == 0)
			proc_sleep_self(100);
#endif
		while ((bitmap_use = object_query(BITMAP)) <= 0)
			proc_sleep_self(100);

		proc_create("auto_dimmer", auto_dimmer, 401, 512, 0);
#endif /* IPC_MRX */
		only_one = 1;
	}
#define	INIT_STRING	"\033[42;1H"
	vt100_write(0, INIT_STRING, sizeof(INIT_STRING) - 1);
#ifdef CPU_SINGLE
	kbd_open(SCC_KEYBOARD);
#endif
}

#ifdef IPC_MRX
vt100_cons_setup()
{
	int vt100_open(), vt100_read(), vt100_write(), vt100_ioctl();

	vt100_cons.open = vt100_open;
	vt100_cons.read = vt100_read;
	vt100_cons.write = vt100_write;
	vt100_cons.ioctl = vt100_ioctl;
}

#define DIMMER_RESET	0
#define DIMMER_ON	1
#define DIMMER_OFF	2
#define DIMMER_INTERVAL	60		/* sec */

static int dimmer_stdport;

auto_dimmer()
{
	register int select, i;
	register int dimm_counter = DIM_CNT_DFLT;
	register int dimm_level = 0;
	int ports[2], *mode;

	spl0();
	ports[0] = dimmer_stdport = STDPORT;
	ports[1] = port_create("auto_dimmer_sub");
	register_interval(ports[1], DIMMER_INTERVAL);
	for(;;) {
		select = msg_select(2, ports);
		if (select == 0) {
			msg_recv(ports[0], NULL, &mode, NULL, 0);
			switch (*mode) {
			case DIMMER_RESET:
				if (!a_dim_on)
					break;
				dimm_counter = dim_cnt;
				if (dimm_level > 0) {
					dimm_level =0;
					for (i = 0; i < nfbdev; i++)
						fbbm_set_dimmer(&fbdev[i], 0);
				}
				break;
			case DIMMER_ON:
				dimm_counter = dim_cnt;
				dimm_level =0;
				for (i = 0; i < nfbdev; i++)
					fbbm_set_dimmer(&fbdev[i], dimm_level);
				a_dim_on = 1;
				break;
			case DIMMER_OFF:
				dimm_counter = dim_cnt;
				dimm_level =0;
				for (i = 0; i < nfbdev; i++)
					fbbm_set_dimmer(&fbdev[i], dimm_level);
				a_dim_on = 0;
				break;
			}
		} else {
			msg_recv(ports[1], NULL, NULL, NULL, 0);
			if (a_dim_on && (dimm_counter-- <= 0)) {
				if (dimm_level < 3) {
					dimm_level++;
				}
				for (i = 0; i < nfbdev; i++)
					fbbm_set_dimmer(&fbdev[i], dimm_level);
				dimm_counter = dim_cnt;
			}
		}
	}
}

rst_dimmer_cnt()
{
	register int diff;
	static unsigned last_time;
	extern unsigned sys_time;
	int mode = DIMMER_RESET;

	diff = sys_time - last_time;
	if (diff > DIMMER_INTERVAL*HZ || diff < 0) {
		dimmer(DIMMER_RESET);
		last_time = sys_time;
	}
}

auto_dimmer_on()
{
	dimmer(DIMMER_ON);
}

auto_dimmer_off()
{
	dimmer(DIMMER_OFF);
}

dimmer(mode)
	int mode;
{
	if (dimmer_stdport)
		msg_send(dimmer_stdport, 0, &mode, sizeof(mode), 0);
}
#else /* IPC_MRX */

static int dimmer_counter = DIM_CNT_DFLT;
static int dim_level = 0;

#ifdef CPU_SINGLE
void
auto_dimmer()
{
	register int s, i;

	s = spl4();
	if (a_dim_on && (dimmer_counter-- <= 0)) {
		if (dim_level < 3)
			dim_level++;
		for (i = 0; i < nfbdev; i++)
			fbbm_set_dimmer(&fbdev[i], dim_level);
		dimmer_counter = dim_cnt;
	}
	splx(s);
	callout_reset(&auto_dimmer_ch, 60 * hz, auto_dimmer, NULL);
}
#endif

void
rst_dimmer_cnt()
{
	register int	s, i;

	if (!a_dim_on)
		return;
#ifdef CPU_SINGLE
	s = spl4();
#endif
	dimmer_counter = dim_cnt;

	if (dim_level > 0) {
		dim_level =0;
		for (i = 0; i < nfbdev; i++)
			fbbm_set_dimmer(&fbdev[i], 0);
	}
#ifdef CPU_SINGLE
	splx(s);
#endif
}

void
auto_dimmer_on()
{
	register int s, i;

#ifdef CPU_SINGLE
	s = spl4();
#endif
	dimmer_counter = dim_cnt;
	dim_level =0;
	for (i = 0; i < nfbdev; i++)
		fbbm_set_dimmer(&fbdev[i], dim_level);
	a_dim_on = 1;
#ifdef CPU_SINGLE
	splx(s);
#endif
}

void
auto_dimmer_off()
{
	register int s, i;

#ifdef CPU_SINGLE
	s = spl4();
#endif
	dimmer_counter = dim_cnt;
	dim_level =0;
	for (i = 0; i < nfbdev; i++)
		fbbm_set_dimmer(&fbdev[i], dim_level);
	a_dim_on = 0;
#ifdef CPU_SINGLE
	splx(s);
#endif
}
#endif /* IPC_MRX */
/*
 *  The routine `_putc(sp, c)' only prints a character c with the cursor
 *  attributes by using `copy_char(x, y, c, attributes)'.
 *  And when IRM (terminal insertion-replacement mode) is set, the characters
 *  righthand side of the cursor are shifted right and lost when they passed
 *  beyond the right margin.
 *  The position is specified by the sp pointer of the structure SCREEN.
 *
 */
static void
_putc(sp, c, kanji)
	register SCREEN	*sp;
	unsigned int c;
{
	if (sp->s_term_mode & IRM) {
		vt_flush(&(sp->s_csr));
		move_chars(sp->s_csr.csr_x, sp->s_csr.csr_y, 
			   rit_m - sp->s_csr.csr_x - ((kanji)? 1: 0),
			   sp->s_csr.csr_x + ((kanji) ? 2: 1));
		copy_char(sp, c, kanji);
	}
	if (fp) {
		fbuf[fp++] = c;
		fpn += kanji + 1;
	} else {
		fbuf[fp++] = c;
		fpp = sp->s_csr.csr_p;
		fpa = sp->s_csr.csr_attributes;
		fpn = kanji + 1;
	}
}

/*
 *  Scroll up and down in the scroll region.
 *  New oriented line must be cleared with terminal mode, that is whether
 *  the screen is reverse mode or not.
 */
void
scroll_up(top, bottom, revsw, fcol, bcol)
	int top;
	int bottom;
	int revsw;
	int fcol;
	int bcol;
{
	move_lines(top + 1, bottom - top, top);
	clear_lines(bottom, 1, revsw, fcol, bcol);
}

void
scroll_down(top, bottom, revsw, fcol, bcol)
	int top;
	int bottom;
	int revsw;
	int fcol;
	int bcol;
{
	move_lines(top, bottom - top, top + 1);
	clear_lines(top, 1, revsw, fcol, bcol);
}

/*
 *  Back space
 *  back_space(sp) moves cursor next to left at current cursor position.
 *  The cursor can not move beyond left or right margin.
 */
void
back_space(sp)
	register SCREEN *sp;
{
	register struct cursor *spc = &sp->s_csr;

	cursor_off();
	if (spc->csr_x > LFT_M) {
		spc->csr_x -= 1;
		spc->csr_p.x -= char_w;
	}
	cursor_on(&spc->csr_p);
}

/*
 *  Tab stop
 *  next_tab_stop(sp) moves cursor to next tab stop.
 */
void
next_tab_stop(sp)
	register SCREEN *sp;
{
	register int i;

	cursor_off();
	for (i = sp->s_csr.csr_x + 1; i < rit_m; i++)
		if (sp->s_tab_pos[i] == 1)
			break;
	sp->s_csr.csr_x = imin(i, rit_m);
	sp->s_csr.csr_p.x = (sp->s_csr.csr_x - 1) * char_w + x_ofst;
	cursor_on(&sp->s_csr.csr_p);
}

/*
 *  Carriage return
 *  carriage_ret(sp) moves cursor at beginning of the current line.
 */
void
carriage_ret(sp)
	register SCREEN *sp;
{
	cursor_off();
	sp->s_csr.csr_x = LFT_M;
	sp->s_csr.csr_p.x = x_ofst;
	cursor_on(&sp->s_csr.csr_p);
}

/*
 *  Bell
 */
static int
bell()
{
#ifdef news1800
	static int port;

	if (port == 0)
		port = port_create("port_cons_bell");
	kbd_ioctl(port, KIOCBELL, &bell_len);
#else
	kbd_ioctl(SCC_KEYBOARD, KIOCBELL, &bell_len);
#endif
	return (0);
}

void
Putchar(c, eob)
	unsigned int c;
{
	register SCREEN *sp = &screen;
	unsigned int sftjis_to_jis();

	c &= 0xff;

	if (eob) {
		vt_flush(&(sp->s_csr));
		return;
	}

	if (c == 0x1b) {	/*  c == esc */
		vt_flush(&(sp->s_csr));
		recover(sp);
		sp->s_current_stat |= ESCAPE;
		return;
	} else if (sp->s_current_stat & ESCAPE) {
		(*sp->s_esc_handler)(sp, c);
		return;
	} else if (sp->s_current_stat & SKANJI) {
		c = sftjis_to_jis(first_code, c);
		if (sp->s_current_stat & JKANJI) {
			addch(sp, c);
		} else {
			sp->s_current_stat |= JKANJI;
			addch(sp, c);
			sp->s_current_stat &= ~JKANJI;
		}
		sp->s_current_stat &= ~SKANJI;
		goto set_csr;
	} else if (sp->s_current_stat & EKANJI) {
		c = (c & 0x7f) | (first_code << 8);
		if (sp->s_current_stat & JKANJI) {
			addch(sp, c);
		} else {
			sp->s_current_stat |= JKANJI;
			addch(sp, c);
			sp->s_current_stat &= ~JKANJI;
		}
		sp->s_current_stat &= ~EKANJI;
		goto set_csr;
	} else if (sp->s_current_stat & JKANJI) {
		jiskanji(sp, c);
		goto set_csr;
	} else if (sp->s_current_stat & EKANA) {
			sp->s_current_stat &= ~EKANA;
			addch(sp, c);
		goto set_csr;
	}
	if (c < 0x20) {		/*  control code	*/
		vt_flush(&(sp->s_csr));
		switch (c) {
		case  0x00:	/*  ignore  */
			break;
		case  0x07:	/*  bell  */
			bell();
			break;
		case  0x08:	/*  back space  */
			back_space(sp);
			break;
		case  0x09:	/*  tabulation  */
			next_tab_stop(sp);
			break;
		case  0x0a:	/*  line feed  */
		case  0x0b:	/*  vertical feed  */
		case  0x0c:	/*  form feed  */
			esc_index(sp);
			break;
		case  0x0d:	/*  carriage return  */
			carriage_ret(sp);
			break;
		case  0x0e:	/*  shift out  */
			break;
		case  0x0f:	/*  shift in  */
			break;
		case  0x11:	/*  xon  */
			break;
		case  0x13:	/*  xoff  */
			break;
		case  0x18:	/*  cancel  */
			sp->s_current_stat &= ~ESCAPE;
			break;
		case  0x1b:	/*  escape  */ 
			/*	NOT REACHED	*/
			recover(sp);
			sp->s_current_stat |= ESCAPE;
			break;
		case  0x7f:	/*  delete  */
			break;

		default:
			break;
		}
	} else {
		switch (tmode) {
#ifdef KM_SJIS
		case KM_SJIS:
			if ((c >= JVR1S && c <= JVR1E) || 
				(c >= JVR2S && c <= JVR2E)) {
				sp->s_current_stat |= SKANJI;
				first_code = c;
			}
			else
				addch(sp, c);
			break;
#endif
#ifdef KM_EUC
		case KM_EUC:
			if (c >= CS1S && c <= CS1E) {
				sp->s_current_stat |= EKANJI;
				first_code = c & 0x7f;
			}
			else if (c == SS2)
				sp->s_current_stat |= EKANA;
			else
				addch(sp, c);
			break;
#endif
#ifdef KM_JIS
		case KM_JIS:
#endif
#ifdef KM_ASCII
		case KM_ASCII:
#endif
		default:
			addch(sp, c);
		}
	}

set_csr:
	cursor_on(&sp->s_csr.csr_p);
		/*  altered	*/
	return ;
}

/*
 *  A printable character is printed in this routine by using
 *  the routine `_putc()'.
 *  Anyway, a character is printed in replacement mode or insertion
 *  mode and if the terminal is autowrap then it takes place wrapping
 *  and if cursor is bottom margin of the scroll region then it takes
 *  place scroll up.
 *  The escape sequence handling is another routine.
 *
 */
void
addch(sp, c)
	register SCREEN	*sp;
	unsigned int c;
{
	register struct cursor *spc = &(sp->s_csr);
	register struct region *spr = &(sp->s_region);

	if (spc->csr_x >= rit_m ||
		((sp->s_current_stat & JKANJI) && (spc->csr_x >= rit_m - 1))) {
		vt_flush(spc);
		if (sp->s_term_mode & DECAWM) {
			if ((sp->s_current_stat & WRAP) || (spc->csr_x == rit_m
					&& sp->s_current_stat & JKANJI)) {
				if (spc->csr_y == spr->btm_margin) {
					cursor_off();
					scroll_up(spr->top_margin,
						  spr->btm_margin,
						  sp->s_term_mode & DECSCNM,
						  sp->s_plane, sp->s_bgcol);
					cursor_on(&(spc->csr_p));
				} else if (spc->csr_y < btm_m) {
					spc->csr_y += 1;
					spc->csr_p.y += char_h;
				}
				spc->csr_x = LFT_M;
				spc->csr_p.x = x_ofst;
				addch(sp, c);
				return;
			}
			sp->s_current_stat |= WRAP;
		}
		if (sp->s_current_stat & JKANJI) {
			if (spc->csr_x != rit_m) {
				_putc(sp, c, 1);
			}
		} else {
			_putc(sp, c, 0);
		}
		if (spc->csr_x < rit_m) {
			spc->csr_x += 1;
			spc->csr_p.x += char_w;
		}

		return ;
	}
	if (sp->s_current_stat & JKANJI) {
		_putc(sp, c, 1);
		spc->csr_x++;
		spc->csr_p.x += char_w;
	} else {
		_putc(sp, c, 0);
	}

	spc->csr_x++;	/*  altered   */
	spc->csr_p.x += char_w;

	sp->s_current_stat &= ~WRAP;
	return ;
}
