/*	$NetBSD: ucomvar.h,v 1.7.2.1 2000/09/04 17:53:14 augustss Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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


/* Macros to clear/set/test flags. */
#define SET(t, f)       (t) |= (f)
#define CLR(t, f)       (t) &= ~(f)
#define ISSET(t, f)     ((t) & (f))

#include "locators.h"
#define ucomcf_portno cf_loc[UCOMBUSCF_PORTNO]
#define UCOM_UNK_PORTNO UCOMBUSCF_PORTNO_DEFAULT

struct	ucom_softc;

struct ucom_methods {
	void (*ucom_get_status)(void *sc, int portno, u_char *lsr, u_char *msr);
	void (*ucom_set)(void *sc, int portno, int reg, int onoff);
#define UCOM_SET_DTR 1
#define UCOM_SET_RTS 2
#define UCOM_SET_BREAK 3
	int (*ucom_param)(void *sc, int portno, struct termios *);
	int (*ucom_ioctl)(void *sc, int portno, u_long cmd,
			  caddr_t data, int flag, struct proc *p);
	int (*ucom_open)(void *sc, int portno);
	void (*ucom_close)(void *sc, int portno);
	void (*ucom_read)(void *sc, int portno, u_char **ptr, u_int32_t *count);
	void (*ucom_write)(void *sc, int portno, u_char *to, u_char *from,
			   u_int32_t *count);
};

/* modem control register */
#define	UMCR_RTS	0x02	/* Request To Send */
#define	UMCR_DTR	0x01	/* Data Terminal Ready */

/* line status register */
#define	ULSR_RCV_FIFO	0x80
#define	ULSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define	ULSR_TXRDY	0x20	/* Transmitter buffer empty */
#define	ULSR_BI		0x10	/* Break detected */
#define	ULSR_FE		0x08	/* Framing error: bad stop bit */
#define	ULSR_PE		0x04	/* Parity error */
#define	ULSR_OE		0x02	/* Overrun, lost incoming byte */
#define	ULSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define	ULSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* modem status register */
/* All deltas are from the last read of the MSR. */
#define	UMSR_DCD	0x80	/* Current Data Carrier Detect */
#define	UMSR_RI		0x40	/* Current Ring Indicator */
#define	UMSR_DSR	0x20	/* Current Data Set Ready */
#define	UMSR_CTS	0x10	/* Current Clear to Send */
#define	UMSR_DDCD	0x08	/* DCD has changed state */
#define	UMSR_TERI	0x04	/* RI has toggled low to high */
#define	UMSR_DDSR	0x02	/* DSR has changed state */
#define	UMSR_DCTS	0x01	/* CTS has changed state */

struct ucom_attach_args {
	int portno;
	int bulkin;
	int bulkout;
	u_int ibufsize;
	u_int ibufsizepad;
	u_int obufsize;
	u_int opkthdrlen;
	usbd_device_handle device;
	usbd_interface_handle iface;
	struct ucom_methods *methods;
	void *arg;
};

int ucomprint(void *aux, const char *pnp);
int ucomsubmatch(struct device *parent, struct cfdata *cf, void *aux);
void ucom_status_change(struct ucom_softc *);
