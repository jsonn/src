/*	$NetBSD: irqhandler.h,v 1.1.4.2 2001/03/12 13:28:26 bouyer Exp $	*/

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * IRQ related stuff (defines + structures)
 *
 * Created      : 30/09/94
 */

#ifndef _ARM32_IRQHANDLER_H_
#define _ARM32_IRQHANDLER_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_cputypes.h"
#endif

#ifndef _LOCORE
#include <sys/types.h>
#endif /* _LOCORE */

/* Define the IRQ bits */

#define IRQ_VSYNC	IRQ_FLYBACK	/* Aliased */
#define IRQ_NETSLOT	IRQ_EXTENDED

#define IRQ_INSTRUCT	-1
/* XXX ICU_LEN is used for the same purpose. Either one should be nuked */
#define NIRQS		0x20

#include <machine/intr.h>

#ifndef _LOCORE
typedef struct irqhandler {
	int (*ih_func) __P((void *arg));/* handler function */
	void *ih_arg;			/* Argument to handler */
	int ih_level;			/* Interrupt level */
	int ih_count;			/* Interrupt number (for accounting) */
	int ih_irq;			/* Interrupt register pin */
	const char *ih_name;		/* Name of interrupt (for vmstat -i) */
	u_int ih_flags;			/* Interrupt flags */
	u_int ih_maskaddr;		/* mask address for expansion cards */
	u_int ih_maskbits;		/* interrupt bit for expansion cards */
	struct irqhandler *ih_next;	/* next handler */
} irqhandler_t;

#ifdef _KERNEL
extern u_int imask[NIPL];
extern irqhandler_t *irqhandlers[NIRQS];

void irq_init __P((void));
void irq_setmasks __P((void));
void disable_irq __P((int));
void enable_irq __P((int));
#endif	/* _KERNEL */
#endif	/* _LOCORE */

#define IRQ_FLAG_ACTIVE 0x00000001	/* This is the active handler in list */

#ifndef _LOCORE
typedef struct fiqhandler {
	void (*fh_func) __P((void));/* handler function */
	u_int fh_size;		/* Size of handler function */
	u_int fh_mask;		/* FIQ mask */
	u_int fh_r8;		/* FIQ mode r8 */
	u_int fh_r9;		/* FIQ mode r9 */
	u_int fh_r10;		/* FIQ mode r10 */
	u_int fh_r11;		/* FIQ mode r11 */
	u_int fh_r12;		/* FIQ mode r12 */
	u_int fh_r13;		/* FIQ mode r13 */
} fiqhandler_t;

#endif	/* _LOCORE */

#endif	/* _ARM32_IRQHANDLER_H_ */

/* End of irqhandler.h */
