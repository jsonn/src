/*	$NetBSD: comreg.h,v 1.9.10.1 1997/10/15 21:20:03 thorpej Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)comreg.h	7.2 (Berkeley) 5/9/91
 */

#include <dev/ic/ns16550reg.h>

#define	COM_FREQ	1843200	/* 16-bit baud rate divisor */
#define	COM_TOLERANCE	30	/* baud rate tolerance, in 0.1% units */

/* interrupt enable register */
#define	IER_ERXRDY	0x1	/* Enable receiver interrupt */
#define	IER_ETXRDY	0x2	/* Enable transmitter empty interrupt */
#define	IER_ERLS	0x4	/* Enable line status interrupt */
#define	IER_EMSC	0x8	/* Enable modem status interrupt */

/* interrupt identification register */
#define	IIR_IMASK	0xf
#define	IIR_RXTOUT	0xc
#define	IIR_RLS		0x6	/* Line status change */
#define	IIR_RXRDY	0x4	/* Receiver ready */
#define	IIR_TXRDY	0x2	/* Transmitter ready */
#define	IIR_MLSC	0x0	/* Modem status */
#define	IIR_NOPEND	0x1	/* No pending interrupts */
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

/* fifo control register */
#define	FIFO_ENABLE	0x01	/* Turn the FIFO on */
#define	FIFO_RCV_RST	0x02	/* Reset RX FIFO */
#define	FIFO_XMT_RST	0x04	/* Reset TX FIFO */
#define	FIFO_DMA_MODE	0x08
#define	FIFO_TRIGGER_1	0x00	/* Trigger RXRDY intr on 1 character */
#define	FIFO_TRIGGER_4	0x40	/* ibid 4 */
#define	FIFO_TRIGGER_8	0x80	/* ibid 8 */
#define	FIFO_TRIGGER_14	0xc0	/* ibid 14 */

/* line control register */
#define	LCR_DLAB	0x80	/* Divisor latch access enable */
#define	LCR_SBREAK	0x40	/* Break Control */
#define	LCR_PZERO	0x38	/* Space parity */
#define	LCR_PONE	0x28	/* Mark parity */
#define	LCR_PEVEN	0x18	/* Even parity */
#define	LCR_PODD	0x08	/* Odd parity */
#define	LCR_PNONE	0x00	/* No parity */
#define	LCR_PENAB	0x08	/* XXX - low order bit of all parity */
#define	LCR_STOPB	0x04	/* 2 stop bits per serial word */
#define	LCR_8BITS	0x03	/* 8 bits per serial word */
#define	LCR_7BITS	0x02	/* 7 bits */
#define	LCR_6BITS	0x01	/* 6 bits */
#define	LCR_5BITS	0x00	/* 5 bits */

/* modem control register */
#define	MCR_LOOPBACK	0x10	/* Loop test: echos from TX to RX */
#define	MCR_IENABLE	0x08	/* Out2: enables UART interrupts */
#define	MCR_DRS		0x04	/* Out1: resets some internal modems */
#define	MCR_RTS		0x02	/* Request To Send */
#define	MCR_DTR		0x01	/* Data Terminal Ready */

/* line status register */
#define	LSR_RCV_FIFO	0x80
#define	LSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define	LSR_TXRDY	0x20	/* Transmitter buffer empty */
#define	LSR_BI		0x10	/* Break detected */
#define	LSR_FE		0x08	/* Framing error: bad stop bit */
#define	LSR_PE		0x04	/* Parity error */
#define	LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	LSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define	LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* modem status register */
/* All deltas are from the last read of the MSR. */
#define	MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	MSR_RI		0x40	/* Current Ring Indicator */
#define	MSR_DSR		0x20	/* Current Data Set Ready */
#define	MSR_CTS		0x10	/* Current Clear to Send */
#define	MSR_DDCD	0x08	/* DCD has changed state */
#define	MSR_TERI	0x04	/* RI has toggled low to high */
#define	MSR_DDSR	0x02	/* DSR has changed state */
#define	MSR_DCTS	0x01	/* CTS has changed state */

/* XXX ISA-specific. */
#define	COM_NPORTS	8
