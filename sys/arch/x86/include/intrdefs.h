/*	$NetBSD: intrdefs.h,v 1.3.2.1 2005/11/10 14:00:20 skrll Exp $	*/

#ifndef _X86_INTRDEFS_H_
#define _X86_INTRDEFS_H_

/*
 * Interrupt priority levels.
 * 
 * There are tty, network and disk drivers that use free() at interrupt
 * time, so imp > (tty | net | bio).
 *
 * Since run queues may be manipulated by both the statclock and tty,
 * network, and disk drivers, clock > imp.
 *
 * IPL_HIGH must block everything that can manipulate a run queue.
 *
 * We need serial drivers to run at the absolute highest priority to
 * avoid overruns, so serial > high.
 *
 * The level numbers are picked to fit into APIC vector priorities.
 *
 */
#define	IPL_NONE	0x0	/* nothing */
#define	IPL_SOFTCLOCK	0x4	/* timeouts */
#define	IPL_SOFTNET	0x5	/* protocol stacks */
#define	IPL_BIO		0x6	/* block I/O */
#define	IPL_NET		0x7	/* network */
#define	IPL_SOFTSERIAL	0x8	/* serial */
#define	IPL_TTY		0x9	/* terminal */
#define	IPL_LPT		IPL_TTY
#define	IPL_VM		0xa	/* memory allocation */
#define	IPL_AUDIO	0xb	/* audio */
#define	IPL_CLOCK	0xc	/* clock */
#define	IPL_STATCLOCK	IPL_CLOCK
#define IPL_SCHED	IPL_CLOCK
#define	IPL_HIGH	0xd	/* everything */
#define	IPL_LOCK	IPL_HIGH
#define	IPL_SERIAL	0xd	/* serial */
#define IPL_IPI		0xe	/* inter-processor interrupts */
#define	NIPL		16

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

/*
 * Local APIC masks. Must not conflict with SIR_* above, and must
 * be >= NUM_LEGACY_IRQs. Note that LIR_IPI must be first.
 */
#define LIR_IPI		31
#define LIR_TIMER	30

/* Soft interrupt masks. */
#define	SIR_CLOCK	29
#define	SIR_NET		28
#define	SIR_SERIAL	27


/*
 * Maximum # of interrupt sources per CPU. 32 to fit in one word.
 * ioapics can theoretically produce more, but it's not likely to
 * happen. For multiple ioapics, things can be routed to different
 * CPUs.
 */
#define MAX_INTR_SOURCES	32
#define NUM_LEGACY_IRQS		16

/*
 * Low and high boundaries between which interrupt gates will
 * be allocated in the IDT.
 */
#define IDT_INTR_LOW	(0x20 + NUM_LEGACY_IRQS)
#define IDT_INTR_HIGH	0xef

#define X86_IPI_HALT			0x00000001
#define X86_IPI_MICROSET		0x00000002
#define X86_IPI_FLUSH_FPU		0x00000004
#define X86_IPI_SYNCH_FPU		0x00000008
#define X86_IPI_TLB			0x00000010
#define X86_IPI_MTRR			0x00000020
#define X86_IPI_GDT			0x00000040

#define X86_NIPI		7

#define X86_IPI_NAMES { "halt IPI", "timeset IPI", "FPU flush IPI", \
			 "FPU synch IPI", "TLB shootdown IPI", \
			 "MTRR update IPI", "GDT update IPI" }

#define IREENT_MAGIC	0x18041969

#endif /* _X86_INTRDEFS_H_ */
