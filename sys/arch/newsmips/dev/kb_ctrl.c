/*	$NetBSD: kb_ctrl.c,v 1.3.8.1 2000/11/20 20:17:21 bouyer Exp $	*/

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
 * from: $Hdr: kb_ctrl.c,v 4.300 91/06/09 06:14:49 root Rel41 $ SONY
 *
 *	@(#)kb_ctrl.c	8.1 (Berkeley) 6/10/93
 */

/*
 *	Keyboard driver
 */

#ifdef IPC_MRX
#include <sys/ioctl.h>
#include <machine/keyboard.h>
#include <newsmips/dev/kbreg.h>
#else
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <machine/keyboard.h>
#include <newsmips/dev/kbreg.h>
#endif

#include "fb.h"

extern int tmode;
extern int kbd_status;

int iscaps = 0;
int change_country = K_JAPANESE_J;
int country;

extern Key_table default_table[];

#ifdef CPU_SINGLE
extern Key_table key_table[];
Key_table *key_table_addr = key_table;
#endif

#ifdef CPU_DOUBLE
Key_table *key_table_addr = default_table;
#endif

#ifdef CPU_SINGLE
#include "ms.h"
#include <sys/clist.h>	
#include <sys/ttydev.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <machine/mouse.h>
#include <newsmips/dev/scc.h>

extern int bmcnrint __P((char));
extern void rst_dimmer_cnt __P((void));
extern int kbd_encode __P((int));
extern int mskeytrigger __P((int, int, int));
extern int kbd_ioctl(), kbd_string(), kbd_repeat();

#if NFB == 0
#define bmcnrint(x)
#define rst_dimmer_cnt()
#endif

int
kbd_open(chan)
	int chan;
{
#if defined(news3400)
	kbm_open(chan);
#endif
	return (0);
}

int
kbd_read(chan, buf, n)
	int chan;
	char *buf;
	int n;
{
#if defined(news1700) || defined(news1200)
	return (kbd_read_raw(chan, buf, n));
#else
	return 0;
#endif
}

int
kbd_write(chan, buf, n)
	int chan;
	char *buf;
	int n;
{

#if defined(news3400)
	return (kbm_write(SCC_KEYBOARD, buf, n));
#endif
}

int
kbd_back(buf, len)
	register char *buf;
	register int len;
{
	int s;

	while (--len >= 0) {
		s = spltty();
		bmcnrint(*buf++);
		(void) splx(s);
	}
	return (0);
}

#define	KBPRI	(PZERO+1)

struct clist scode_buf;
struct clist keyboard_buf;
char	kb_rwait;

int
kbd_flush()
{

	ndflush(&scode_buf, scode_buf.c_cc);
	return (0);
}

char kb_busy;

static int
kbd_put_raw(scode)
	int scode;
{
/*	extern char kb_busy;*/

	if (kb_busy) {
		if (scode_buf.c_cc < CBSIZE)
			putc(scode, &scode_buf);
		if (kb_rwait) {
			kb_rwait = 0;
			wakeup((caddr_t)&kb_rwait);
		}
	}
	return (scode);
}

int
kbd_read_raw(chan, buf, count)
	int chan;
	char *buf;
	register int count;
{
	register int i;
	register int n;
	register int s;

	if (count <= 0)
		return (count);
	s = splscc();
	while ((n = imin(scode_buf.c_cc, count)) == 0) {
		kb_rwait = 1;
		(void) tsleep(&kb_rwait, KBPRI, "kbdrdraw", 0);
		kb_rwait = 0;
	}
	(void) splx(s);
	for (i = n; i > 0 ; i--)
		*buf++ = getc(&scode_buf);
	return (n);
}

int
kbd_nread()
{

	return (scode_buf.c_cc);
}

int
kbd_bell(n)
	register int n;
{

#if defined(news3400)
	(void) kbm_write(SCC_KEYBOARD, NULL, n);
#endif
	return (0);
}

void
kbd_putcode(code)
	int code;
{
	int c;

	kbd_put_raw(code);
	kbd_encode(code);
	while ((c = getc(&keyboard_buf)) != -1)
		bmcnrint(c); 
}

int
put_code(buf, cnt)
	register char *buf;
	register int cnt;
{

	while (--cnt >= 0)
		putc(*buf++, &keyboard_buf);
	return 0;
}

void
kb_softint()
{
	int code;
#if 0
	extern int tty00_is_console;
#endif

	while ((code = xgetc(SCC_KEYBOARD)) >= 0) {
#if defined(news3200)		/* BEGIN reiko */
		if ((code & 0x7f) == KEY_EISUU) {
			int up = code & OFF;
			static int kana = 0;

			if (kana) {
				if (up) {
					kana = 0;
				}
			} else {
				if (up) {
					code = KEY_KANA | OFF;
					kana = 1;
				} else {
					code = KEY_KANA;
				}
			}
		}
#endif

#ifdef NOTDEF /* KU:XXX */
		if (!tty00_is_console) 
#endif
			rst_dimmer_cnt();
#if NMS > 0
		if (!mskeytrigger(0, code & OFF, code & 0x7f))
#endif
		kbd_putcode(code);
	}
}
#endif /* CPU_SINGLE */

#ifdef IPC_MRX
#include "mrx.h"
#include "queue.h"
#include "process.h"
#include "buffer.h"
#include "port.h"
#include "message.h"
#include "machdep.h"
#include "malloc.h"
#include "config.h"
#include "kbms.h"

static struct buffer *kbd_buf;
static int port_kbd_intr;
static int port_kbd_back;
static int port_kbd_ctrl;

keyboard(chan)
	int chan;
{
	int kbd_ctrl(), kbd_output();
	int kbd_read(), kbd_ioctl(), kbd_io();

#ifdef news3800
	extern int (*Xkb_intr)();
	int kb_intr();

	Xkb_intr = kb_intr;
#endif
	kb_ioctl = kbd_ioctl;
	kb_read = kbd_read;
	kbd_init();
	proc_create("kbd_ctrl", kbd_ctrl, 300, DEFAULT_STACK_SIZE, 0);
	proc_create("kbd_output", kbd_output, 300, DEFAULT_STACK_SIZE, 0);
	proc_create("kbd_io", kbd_io, 300, DEFAULT_STACK_SIZE, 0);
}

int (*reset_dimmer)();

kbd_ctrl()
{
	register int m, n;
	register int select;
	int *len, from, count;
	char *addr;
	int ports[3];
	static char buf[16];

	ports[0] = port_kbd_intr = port_create("kb_intr");
	ports[1] = port_kbd_back = port_create("kb_echoback");
	ports[2] = port_kbd_ctrl = STDPORT;

#ifdef news3800
	*(char *)KEYBD_RESET = 0;
	*(char *)KEYBD_INTE = 1;
#endif

	kbd_buf = buffer_alloc(32);
	(void) spl0();
	for (;;) {
		if (buffer_status(kbd_buf) > 0)
			m = 3;
		else
			m = 2;
		if ((select = msg_select(m, ports)) == 0) {
			msg_recv(ports[0], NULL, &addr, &count, 0);
			if (reset_dimmer)
				(*reset_dimmer)();
			while (--count >= 0) {
				if (send_mouse == 0 || (*send_mouse)(*addr) == 0)
					kbd_encode(*addr);
				addr++;
			}
		} else if (select == 1) {	/* ESC [ 6 n */
			msg_recv(ports[select], NULL, &addr, &count, 0);
			put(kbd_buf, addr, count);
		} else {
			msg_recv(ports[select], &from, &len, NULL, 0); 
			n = buffer_status(kbd_buf);
			n = imin(n, *len);
			n = get(kbd_buf, buf, imin(n, sizeof (buf)));
			msg_send(from, ports[select], buf, n, 0);
		}
	}
}

kbd_output()
{
	char *addr;
	int from, len;

	(void) spl0();
	for (;;) {
		msg_recv(STDPORT, &from, &addr, &len, 0);
#ifdef news3800
		len = kbd_write(0, addr, len);
#endif
		msg_send(from, STDPORT, &len, sizeof(len), 0);
	}
}

kbd_io()
{
	struct kb_ctrl_req *req;
	int from, reply;

	(void) spl0();
	for (;;) {
		msg_recv(STDPORT, &from, &req, NULL, 0);
		if (req->kb_func == KIOCCHTBL || req->kb_func == KIOCOYATBL)
			kbd_ioctl(0, req->kb_func, req->kb_arg);
		else
			kbd_ioctl(0, req->kb_func, &req->kb_arg);
		reply = req->kb_arg;
		msg_send(from, STDPORT, &reply, sizeof(reply), 0);
	}
}

kbd_read(chan, buf, n)
	int chan;
	char *buf;
	int n;
{
	static int port;
	char *addr;
	int len;

	if (port == 0)
		port = port_create("port_kbd_read");
	if (n <= 0)
		return (0);
	msg_send(port_kbd_ctrl, port, &n, sizeof (n), 0);
	msg_recv(port, NULL, &addr, &len, 0);
	bcopy(addr, buf, len);
	msg_free(port);
	return (len);
}

kbd_write(chan, buf, n)
	int chan;
	char *buf;
	int n;
{

#ifdef news3800
	*(char *)BEEP_FREQ = ~(n & 1);
	*(char *)KEYBD_BEEP = 1;
	return (n);
#endif
}

kbd_back(buf, len)
	char *buf;
	int len;
{

	msg_send(port_kbd_back, 0, buf, len, 0);
	return (0);
}

kbd_nread()
{

	return (buffer_status(kbd_buf));
}

kbd_flush()
{

	buffer_flush(kbd_buf);
	return (0);
}

#ifdef news3800
kb_intr()
{
	char c;

	if (port_kbd_intr > 0)
		while (*(char *)KBMS_STAT & (1 << b_KBREADY)) {
			c = *(char *)KEYBD_DATA;
			msg_send(port_kbd_intr, 0, &c, sizeof (char), 0);
		}
}
#endif /* news3800 */

kbd_bell(n, port)
	int n, port;
{

#ifdef news3800
	(void) kbd_write(0, NULL, n);
#else
	kbd_bell_scc(n, port);
#endif
	return (0);
}

put_code(buf, cnt)
	char *buf;
	int cnt;
{
	put(kbd_buf, buf, cnt);
}
#endif /* IPC_MRX */

int
kbd_ioctl(chan, cmd, argp)
	int chan;
	int cmd;
	int *argp;
{
	switch (cmd) {

	case KIOCFLUSH:
		return (kbd_flush());

	case KIOCSETS:
	case KIOCGETS:
		return (kbd_string(cmd, (Pfk_string *)argp));

	case KIOCBELL:
		return (kbd_bell(*argp));

	case KIOCBACK:
		if (argp == NULL)
			return (-1);
		if ((int)((Key_string *)argp)->key_string == NULL)
			return (-1);
		if ((int)((Key_string *)argp)->key_length <= 0)
			return (-1);
		return (kbd_back(((Key_string *)argp)->key_string,
		    ((Key_string *)argp)->key_length));

	case KIOCREPT:
		return (kbd_repeat(1));

	case KIOCNRPT:
		return (kbd_repeat(0));

	case KIOCNREAD:
		*argp = kbd_nread();
		return (0);

	case KIOCSETLOCK:
		iscaps = *argp;
		return (0);

	case KIOCGETCNUM:
		*argp = country;
		return (0);

	case KIOCSETCNUM:
		country = *argp;
		change_country = country;
		return (0);

	case KIOCDEFTBL:
		key_table_addr = default_table;
		country = K_JAPANESE_J;
		return (0);

	case KIOCCHTBL:
		key_table_addr = (Key_table *)argp;
		country = change_country;
		return (0);

	case KIOCGETSTAT:
		*argp = kbd_status;
		return (0);

	case KIOCSETSTAT:
		kbd_status = *argp;
		return (0);

	default:
		return (-1);
	}
}

void
kbd_bell_scc(n)
	register int n;
{
	register int i;
	static char bell_data[] = {
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0
	};

	while (n > 0) {
		i = imin(n, sizeof (bell_data));
		(void) kbd_write(0, bell_data, i);
		n -= i;
	}
}

#ifdef KBDEBUG
scc_error_puts(chan, buf)
	int chan;
	char *buf;
{
	while (*buf) {
		scc_error_write(chan, *buf++, 1);
	}
}

scc_error_write_hex(chan, n, zs)
	int chan;
	unsigned int n;
	int zs;
{
	int i;
	int tmp, al;
	static char hex[] = "0123456789abcdef";

	al = 0;

	for (i = 28; i >= 0; i -= 4) {
		tmp = (n >> i) & 0x0f;
		if (tmp || al || !zs || !i) {
			al++;
			scc_error_write(chan, hex[tmp], 1);
		}
	}
}
#endif /* KBDEBUG */
