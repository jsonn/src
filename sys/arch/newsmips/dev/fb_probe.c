/*	$NetBSD: fb_probe.c,v 1.3.8.1 2000/11/20 20:17:19 bouyer Exp $	*/

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
 * from: $Hdr: fb_probe.c,v 4.300 91/06/09 06:32:57 root Rel41 $ SONY
 *
 *	@(#)fb_probe.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/types.h>
#include <sys/exec.h>

#include <machine/autoconf.h>
#include <machine/framebuf.h>

#include <newsmips/dev/fbdefs.h>

#ifdef CPU_SINGLE
#define SW_CONSOLE	0x07
#define SW_NWB512	0x04
#define SW_NWB225	0x01
#define SW_FBPOP	0x02
#define SW_FBPOP1	0x06
#define SW_FBPOP2	0x03
#define SW_AUTOSEL	0x07
#endif

struct fbdev	*consfb;
int	nfbdev = 0;

static int	cons_dev = -1;
extern struct fbdevsw	fbdevsw[];

#ifdef news3400
static struct autodev autodev[] = {
	{ -1, (char *)0xb8600000, (char *)0xb8610000 },
	{ -1, (char *)0xb8620000, (char *)0xb8630000 },
	{ -1, (char *)0xb8640000, (char *)0xb8650000 },
	{ -1, (char *)0xb8660000, (char *)0xb8670000 },
	{ -1, (char *)0xb8680000, (char *)0xb8690000 },
	{ -1, (char *)0xb86a0000, (char *)0xb86b0000 },
	{ -1, (char *)0xb86c0000, (char *)0xb86d0000 },
	{ -1, (char *)0xb86e0000, (char *)0xb86f0000 },
};

#define AUTOSEL	1
#endif /* news3400 */

#ifdef news3800
static struct autodev autodev[] = {
	{ -1, (char *)0xee600000, (char *)0xee610000 },
	{ -1, (char *)0xee620000, (char *)0xee630000 },
	{ -1, (char *)0xee640000, (char *)0xee650000 },
	{ -1, (char *)0xee660000, (char *)0xee670000 },
	{ -1, (char *)0xee680000, (char *)0xee690000 },
	{ -1, (char *)0xee6a0000, (char *)0xee6b0000 },
	{ -1, (char *)0xee6c0000, (char *)0xee6d0000 },
	{ -1, (char *)0xee6e0000, (char *)0xee6f0000 },
};

#define AUTOSEL	1
#endif /* news3800 */

int
search_fbdev(type, unit)
	int type;
	int unit;
{
	register int i;

	if (type == 0) {
		if (cons_dev < 0)
			return (unit < nfbdev ? unit : -1);
		if (unit == 0)
			return (cons_dev);
		for (i = 0; i < nfbdev; i++)
			if (i != cons_dev && --unit == 0)
				return (i);
	} else {
		for (i = 0; i < nfbdev; i++)
			if (fbdev[i].type == type && fbdev[i].unit == unit)
				return(i);
	}
	return (-1);
}

#ifdef AUTOSEL
int
search_autocons()
{
	register int i;

	for (i = 0; i < 8; i++)
		if (autodev[i].type != -1)
			return (autodev[i].type);
	return (-1);
}

void
fb_auto_probe()
{
	register int i;

	for (i = 0; i < 8; i++) {
		if (badaddr(autodev[i].base, 1)) {
			autodev[i].type = -1;
			continue;
		}
		if (*(long *)autodev[i].base != OMAGIC) {
			autodev[i].type = 1;
			continue;
		}
		switch((*(int *)(autodev[i].base + 0x20))) {
		case 0:
			autodev[i].type = FB_NWB514;
			break;
		case 1:
			autodev[i].type = FB_NWB251;
			break;
		case 2:
			autodev[i].type = FB_NWB518;
			break;
		case 3:
		case 8:
			autodev[i].type = FB_NWB254;
			break;
		case 4:
		case 0x10004:
		case 0x20004:
			autodev[i].type = FB_NWB257;
			break;
		case 0x10005:
		case 0x20005:
			autodev[i].type = FB_NWB256;
			break;
		case 6:
			autodev[i].type = FB_SLB101;
			break;
		case 7:
			autodev[i].type = FB_NWB255;
			break;
		default:
			autodev[i].type = -1;
			break;
		}
	}
}
#endif /* AUTOSEL */

static void
cons_probe(dipsw)
	int dipsw;
{
	switch (dipsw & SW_CONSOLE) {

	case SW_NWB225:
		cons_dev = search_fbdev(FB_NWB225, 0);
		break;

	case SW_NWB512:
		cons_dev = search_fbdev(FB_NWB512, 0);
		break;

#ifdef CPU_SINGLE
	case SW_FBPOP:
	case SW_FBPOP1:
	case SW_FBPOP2:
		if ((cons_dev = search_fbdev(FB_LCDM, 0)) >= 0)
			break;
		if ((cons_dev = search_fbdev(FB_POPM, 0)) >= 0)
			break;
		if ((cons_dev = search_fbdev(FB_POPC, 0)) >= 0)
			break;
		if ((cons_dev = search_fbdev(FB_NWB252, 0)) >= 0)
			break;
		if ((cons_dev = search_fbdev(FB_NWB253, 0)) >= 0)
			break;
		break;
#endif /* CPU_SINGLE */

#ifdef AUTOSEL
	case SW_AUTOSEL:
		cons_dev = search_fbdev(search_autocons(), 0);
		break;
#endif

	default:
		cons_dev = -1;
		break;
	}
	if (cons_dev == -1)
		consfb = 0;
	else
		consfb = &fbdev[cons_dev];
}

void fbbm_probe(dipsw)
	int dipsw;
{
	register int i, j;
	register struct fbdevsw	*pfsw = fbdevsw;

	j = 0;

#ifdef AUTOSEL
	fb_auto_probe();
#endif

	while (pfsw->num) {
		for (i = 0; i < pfsw->num; i++) {
			if (fbdev[j].type = (*pfsw->fb_probe)(i, &fbdev[j])) {
				fbdev[j].unit = i;
				(*pfsw->fb_setup)(&fbdev[j], dipsw);
				j++;
			}
		}
		pfsw++;
	}
	nfbdev = j;

	cons_probe(dipsw);
}
