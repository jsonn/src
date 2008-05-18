/*	$NetBSD: cons_machdep.h,v 1.1.78.1 2008/05/18 12:31:54 yamt Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#define	FB_LINEBYTES	2048
#define	FB_WIDTH	1284
#define	FB_HEIGHT	1024

#define	CONS_WIDTH	(FB_WIDTH / ROM_FONT_WIDTH)
#define	CONS_HEIGHT	(FB_HEIGHT / ROM_FONT_HEIGHT)

#define	X_INIT		0
#define	Y_INIT		0

enum console_type {
	CONS_ROM,	/* ROM console I/O */
	CONS_FB_KSEG2,	/* direct fb device access via KSEG2 */
	CONS_FB_KSEG1,	/* direct fb device access via KSEG1 */
	CONS_SIO1,	/* serial console port 1 */
	CONS_SIO2,	/* serial console port 2 */
};

struct cons {
	enum console_type type;
	int x;
	int y;
};

extern struct cons cons;

void rom_cons_init(void);
