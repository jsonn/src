/*	$NetBSD: mtpr.h,v 1.2.16.1 2004/08/03 10:32:09 skrll Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 * from: Utah $Hdr: mtpr.h 1.1 90/07/09$
 *
 *	@(#)mtpr.h	7.2 (Berkeley) 11/3/90
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
 * from: Utah $Hdr: mtpr.h 1.1 90/07/09$
 *
 *	@(#)mtpr.h	7.2 (Berkeley) 11/3/90
 */
#ifndef _AMIGAPPC_MPTR_H_
#define _AMIGAPPC_MPTR_H_

#ifdef _KERNEL
/*
 * simulated software interrupt register (extends hardware
 * SOFTINT bit)
 */

/*
 * this makes it pretty machine dependant. Should this go into
 * <amiga/amiga/mtpr.h> ?
 */
#include <amiga/amiga/custom.h>
#ifdef DRACO
#include <amiga/amiga/drcustom.h>
#include <m68k/include/asm_single.h>
#endif

extern unsigned char ssir;

#define SIR_NET		0x1	/* call netintr */
#define SIR_CLOCK	0x2	/* call softclock */
#define	SIR_CBACK	0x4	/* walk the sicallback-chain */

#define siroff(x)	ssir &= ~(x)
#ifdef DRACO
#define setsoftint()	do { if (is_draco()) \
				single_inst_bset_b(*draco_intfrc, DRIRQ_SOFT); \
			    else \
				custom.intreq = INTF_SETCLR|INTF_SOFTINT; \
			} while (0)
#define clrsoftint()	do { if (is_draco()) \
				single_inst_bclr_b(*draco_intfrc, DRIRQ_SOFT); \
			    else \
				custom.intreq = INTF_SOFTINT; \
			} while (0)
#else
#define setsoftint()	do {custom.intreq = INTF_SETCLR|INTF_SOFTINT;} while (0)
#define clrsoftint()	do {custom.intreq = INTF_SOFTINT;} while (0)
#endif

#define setsoftnet()	do {ssir |= SIR_NET; setsoftint(); } while (0)
#define setsoftclock()	do {ssir |= SIR_CLOCK; setsoftint(); } while (0)
#define setsoftcback()	do {ssir |= SIR_CBACK; setsoftint(); } while (0)

void softintr_schedule __P((void *));
void *softintr_establish __P((int,  void (*)(void *), void *));
void softintr_disestablish __P((void *));

#endif /* _KERNEL */

#endif /* !_AMIGAPPC_MPTR_H_ */
