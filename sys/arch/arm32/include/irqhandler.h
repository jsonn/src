/*	$NetBSD: irqhandler.h,v 1.9.8.1 1997/10/15 05:36:26 thorpej Exp $	*/

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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * irqhandler.h
 *
 * IRQ related stuff
 *
 * Created      : 30/09/94
 */

#ifndef _ARM32_IRQHANDLER_H_
#define _ARM32_IRQHANDLER_H_

#ifndef _LOCORE
#include <sys/types.h>
#endif /* _LOCORE */

/* Define the IRQ bits */

#ifdef CPU_ARM7500

#ifdef RC7500

/*#define IRQ_PRINTER	0x00*/
#define IRQ_RESERVED0	0x01
#define IRQ_BUTTON	0x02
#define IRQ_FLYBACK	0x03
#define IRQ_POR		0x04
#define IRQ_TIMER0	0x05
#define IRQ_TIMER1 	0x06
#define IRQ_FIQDOWN	0x07

#define IRQ_DREQ3	0x08
/*#define IRQ_HD1		0x09*/
/*#define IRQ_HD		IRQ_HD1*/
#define IRQ_DREQ2	0x0A
#define IRQ_ETHERNET	0x0B
/*#define IRQ_FLOPPY	0x0C*/
/*#define IRQ_SERIAL	0x0D*/
#define IRQ_KBDTX	0x0E
#define IRQ_KBDRX	0x0F

#define IRQ_IRQ3	0x10
#define IRQ_IRQ4	0x11
#define IRQ_IRQ5	0x12
#define IRQ_IRQ6	0x13
#define IRQ_IRQ7	0x14
#define IRQ_IRQ9	0x15
#define IRQ_IRQ10	0x16
#define IRQ_HD2		0x17

#define IRQ_MSDRX	0x18
#define IRQ_MSDTX	0x19
#define IRQ_ATOD	0x1A
#define IRQ_CLOCK	0x1B
#define IRQ_PANIC	0x1C
#define IRQ_RESERVED2	0x1D
#define IRQ_RESERVED3	0x1E
#define IRQ_RESERVED1	IRQ_RESERVED2

/*
 * Note that Sound DMA IRQ is on the 31st vector.
 * It's not part of the IRQD.
 */
#define IRQ_SDMA	0x1F

#else

/*#define IRQ_PRINTER	0x00*/
#define IRQ_RESERVED0	0x01
#define IRQ_BUTTON	0x02
#define IRQ_FLYBACK	0x03
#define IRQ_POR		0x04
#define IRQ_TIMER0	0x05
#define IRQ_TIMER1 	0x06
#define IRQ_RESERVED1	0x07

#define IRQ_DREQ3	0x08
/*#define IRQ_HD1		0x09*/
/*#define IRQ_HD		IRQ_HD1*/
#define IRQ_DREQ2	0x0A
#define IRQ_EXTENDED	0x0B
/*#define IRQ_FLOPPY	0x0C*/
/*#define IRQ_SERIAL	0x0D*/
#define IRQ_PODULE	0x0D
#define IRQ_KBDTX	0x0E
#define IRQ_KBDRX	0x0F

#define IRQ_IRQ3	0x10
#define IRQ_IRQ4	0x11
#define IRQ_IRQ5	0x12
#define IRQ_IRQ6	0x13
#define IRQ_IRQ7	0x14
#define IRQ_IRQ9	0x15
#define IRQ_IRQ10	0x16
#define IRQ_IRQ11	0x17

#define IRQ_MSDRX	0x18
#define IRQ_MSDTX	0x19
#define IRQ_ATOD	0x1A
#define IRQ_CLOCK	0x1B
#define IRQ_PANIC	0x1C
#define IRQ_RESERVED2	0x1D
#define IRQ_RESERVED3	0x1E

/*
 * Note that Sound DMA IRQ is on the 31st vector.
 * It's not part of the IRQD.
 */
#define IRQ_SDMA	0x1F

#define IRQ_EXPCARD0	0x20
#define IRQ_EXPCARD1	0x21
#define IRQ_EXPCARD2	0x22
#define IRQ_EXPCARD3	0x23
#define IRQ_EXPCARD4	0x24
#define IRQ_EXPCARD5	0x25
#define IRQ_EXPCARD6	0x26
#define IRQ_EXPCARD7	0x27


#endif	/* RC7500 */
#else	/* CPU_ARM7500 */

#ifdef	RISCPC
/*#define IRQ_PRINTER	0x00*/
#define IRQ_RESERVED0	0x01
/*#define IRQ_FLOPPYIDX	0x02*/
#define IRQ_FLYBACK	0x03
#define IRQ_POR		0x04
#define IRQ_TIMER0	0x05
#define IRQ_TIMER1	0x06
#define IRQ_RESERVED1	0x07

#define IRQ_RESERVED2	0x08
/*#define IRQ_HD		0x09*/
/*#define IRQ_SERIAL	0x0A*/
#define IRQ_EXTENDED	0x0B
/*#define IRQ_FLOPPY	0x0C*/
#define IRQ_PODULE	0x0D
#define IRQ_KBDTX	0x0E
#define IRQ_KBDRX	0x0F

#define IRQ_DMACH0	0x10
#define IRQ_DMACH1	0x11
#define IRQ_DMACH2	0x12
#define IRQ_DMACH3	0x13
#define IRQ_DMASCH0	0x14
#define IRQ_DMASCH1	0x15
#define IRQ_RESERVED3	0x16
#define IRQ_RESERVED4	0x17

#define IRQ_EXPCARD0	0x18
#define IRQ_EXPCARD1	0x19
#define IRQ_EXPCARD2	0x1A
#define IRQ_EXPCARD3	0x1B
#define IRQ_EXPCARD4	0x1C
#define IRQ_EXPCARD5	0x1D
#define IRQ_EXPCARD6	0x1E
#define IRQ_EXPCARD7	0x1F
#endif	/* RISCPC */

#endif	/* CPU_ARM7500 */

#define IRQ_VSYNC	IRQ_FLYBACK	/* Aliased */
#define IRQ_NETSLOT	IRQ_EXTENDED

#define IRQ_SOFTNET	IRQ_RESERVED0	/* Emulated */
#define IRQMASK_SOFTNET	(1 << IRQ_SOFTNET)
#define IRQ_SOFTCLOCK	IRQ_RESERVED1	/* Emulated */
#define IRQMASK_SOFTCLOCK	(1 << IRQ_SOFTCLOCK)
#define IRQMASK_ALLSOFT (IRQMASK_SOFTNET | IRQMASK_SOFTCLOCK)

#define IRQ_INSTRUCT	-1
#define NIRQS		0x20

#include <machine/intr.h>
#if 0
/* Define the various Interrupt Priority Levels */

/* Interrupt Priority Levels are not mutually exclusive. */

#define IPL_BIO        0	/* block I/O */
#define IPL_NET        1	/* network */
#define IPL_TTY        2	/* terminal */
#define IPL_CLOCK      3	/* clock */
#define IPL_IMP        4	/* memory allocation */
#define IPL_NONE       5

#define IPL_LEVELS     6
#endif

#ifndef _LOCORE
typedef struct irqhandler {
	int (*ih_func) __P((void *arg));/* handler function */
	void *ih_arg;			/* Argument to handler */
	int ih_level;			/* Interrupt level */
	int ih_num;			/* Interrupt number (for accounting) */
	const char *ih_name;		/* Name of interrupt (for vmstat -i) */
	u_int ih_flags;			/* Interrupt flags */
	u_int ih_maskaddr;		/* mask address for expansion cards */
	u_int ih_maskbits;		/* interrupt bit for expansion cards */
	struct irqhandler *ih_next;	/* next handler */
} irqhandler_t;

#ifdef _KERNEL
extern u_int irqmasks[IPL_LEVELS];
extern irqhandler_t *irqhandlers[NIRQS];

void irq_init __P((void));
int irq_claim __P((int, irqhandler_t *));
int irq_release __P((int, irqhandler_t *));
void *intr_claim __P((int irq, int level, const char *name, int (*func) __P((void *)), void *arg));
int intr_release __P((void *ih));
void irq_setmasks __P((void));
void disable_irq __P((int));
void enable_irq __P((int));
u_int enable_interrupts __P((u_int));
u_int disable_interrupts __P((u_int));
u_int restore_interrupts __P((u_int));
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

#ifdef _KERNEL
int fiq_claim __P((fiqhandler_t *));
int fiq_release __P((fiqhandler_t *));
#endif	/* _KERNEL */
#endif	/* _LOCORE */

#endif	/* _ARM32_IRQHANDLER_H_ */

/* End of irqhandler.h */
