/*	$NetBSD: intr.h,v 1.2.2.2 2001/03/12 13:28:53 bouyer Exp $	*/
#ifndef _HPCSH_INTR_H_
#define _HPCSH_INTR_H_

/* Interrupt priority `levels'. */
#define	IPL_NONE	9	/* nothing */
#define	IPL_SOFTCLOCK	8	/* timeouts */
#define	IPL_SOFTNET	7	/* protocol stacks */
#define	IPL_BIO		6	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	4	/* serial */
#define	IPL_TTY		3	/* terminal */
#define	IPL_IMP		3	/* memory allocation */
#define	IPL_AUDIO	2	/* audio */
#define	IPL_CLOCK	1	/* clock */
#define	IPL_HIGH	1	/* everything */
#define	IPL_SERIAL	0	/* serial */
#define	NIPL		10

#include <sh3/intr.h>

/* Soft interrupt masks. */
#define	SIR_CLOCK	31
#define	SIR_NET		30
#define	SIR_SERIAL	29

#define SIR_LOW		29
#define SIR_HIGH	31

/* IRQ */
#define TMU1_IRQ	2
#define SCI_IRQ		6
#define SCIF_IRQ	7
#define IRQ4_IRQ	4
#define WDOG_IRQ	1

#define IRQ_LOW  1
#define IRQ_HIGH 15

#endif /* _HPCSH_INTR_H_ */
