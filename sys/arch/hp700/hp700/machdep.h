/*	$NetBSD: machdep.h,v 1.1.2.2 2002/06/23 17:36:23 jdolecek Exp $	*/

/*	$OpenBSD: cpufunc.h,v 1.17 2000/05/15 17:22:40 mickey Exp $	*/

/*
 * Copyright (c) 1998,2000 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */
/*
 * Copyright (c) 1990,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: c_support.s 1.8 94/12/14$
 *	Author: Bob Wheeler, University of Utah CSL
 */

/*
 * Definitions for the hp700 that are completely private 
 * to the machine-dependent code.  Anything needed by
 * machine-independent code is covered in cpu.h or in
 * other headers.
 */

/*
 * XXX there is a lot of stuff in various headers under 
 * hp700/include and hppa/include, and a lot of one-off 
 * `extern's in C files that could probably be moved here.
 */

#ifdef _KERNEL

#include <hppa/hppa/machdep.h>

/* This forcefully reboots the machine. */
void cpu_die __P((void));

/* These map and unmap page zero. */
int hp700_pagezero_map __P((void));
void hp700_pagezero_unmap __P((int));

#ifdef USELEDS
#define	PALED_NETSND	0x01
#define	PALED_NETRCV	0x02
#define	PALED_DISK	0x04
#define	PALED_HEARTBEAT	0x08
#define	PALED_LOADMASK	0xf0

#define	PALED_DATA	0x01
#define	PALED_STROBE	0x02

extern volatile u_int8_t *machine_ledaddr;
extern int machine_ledword, machine_leds;

static __inline void
ledctl(int on, int off, int toggle)
{
	if (machine_ledaddr) {
		int r;

		if (on)
			machine_leds |= on;
		if (off)
			machine_leds &= ~off;
		if (toggle)
			machine_leds ^= toggle;
			
		r = ~machine_leds;	/* it seems they should be reversed */

		if (machine_ledword)
			*machine_ledaddr = r;
		else {
			register int b;
			for (b = 0x80; b; b >>= 1) {
				*machine_ledaddr = (r & b)? PALED_DATA : 0;
				DELAY(1);
				*machine_ledaddr = ((r & b)? PALED_DATA : 0) |
				    PALED_STROBE;
			}
		}
	}
}
#endif /* USELEDS */

#endif /* _MACHINE_CPUFUNC_H_ */
