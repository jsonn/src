/*	$NetBSD: crx.h,v 1.2.42.3 2004/09/21 13:23:57 skrll Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)rx50reg.h	7.2 (Berkeley) 6/28/90
 */

/*
 * RX50 registers.
 */

/*
 * The names below do not quite match the DEC documentation simply because
 * the names in the documentation are so bad.
 */
struct rx50device {
	u_short	rxid;		/* identification */
	u_short	reserved;
	u_short	rxcmd;		/* command function reg */
	u_short	rxtrk;		/* track */
	u_short	rxsec;		/* sector */
	u_short	rxcsc;		/* current sector */
	u_short	rxict;		/* incorrect track (???) */
	u_short	rxext;		/* extend command register */
	u_short	rxedb;		/* empty data buffer (read) */
	u_short	rxrda;		/* reset data address */
	volatile u_short	rxgo;	/* read to start current cmd */
	u_short	rxfdb;		/* fill data buffer (write) */
};

#define	RX50SEC		10	/* sectors per track */
#define	RX50MAXSEC	800	/* 10 sectors times 80 tracks */

/* Interrupt vector */
#define	SCB_RX50	0xf0

/*
 * Do the sector skew given the sector and track
 * number (it depends on both!).
 */
/*			(((((s) / 5) + 2 * ((s) + (t))) % 10) + 1) */
#define	RX50SKEW(s, t)	(((s) / 5) + "\1\3\5\7\11\1\3\5\7"[((s) + (t)) % 5])

/*
 * Values in the command function register.
 */
#define	RXCMD_ERROR	0x80	/* error bit (composite?) */
#define	RXCMD_READ	0x40	/* read command */
#define	RXCMD_WRITE	0x70	/* write command */
#define	RXCMD_RESET	0x20	/* reset command */
#define	RXCMD_DONE	0x08	/* operation done (status) */
#define	RXCMD_DRIVE0	0x00	/* select drive 0 (csa1) */
#define	RXCMD_DRIVE1	0x02	/* select drive 1 (csa2) */
