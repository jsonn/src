/*	$NetBSD: hpibvar.h,v 1.1.60.3 2004/09/21 13:15:27 skrll Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)hpibvar.h	8.1 (Berkeley) 6/10/93
 */

#define	HPIBA		32
#define	HPIBB		1
#define	HPIBC		8
#define	HPIBA_BA	21
#define	HPIBC_BA	30

#define	CSA_BA		0x1F

#define	C_DCL		20
#define	C_LAG		32
#define	C_UNL		63
#define	C_TAG		64
#define	C_UNA		94
#define	C_UNT		95
#define	C_SCG		96

struct	hpib_softc {
	char	sc_alive;
	char	sc_type;
	int	sc_ba;
	char	*sc_addr;
};

extern struct hpib_softc hpib_softc[];
extern int internalhpib;

/* hpib.c */
void hpibinit(void);
int hpibalive(int);
int hpibid(int, int);
int hpibsend(int, int, int, char *, int);
int hpibrecv(int, int, int, char *, int);
int hpibswait(int, int);
void hpibgo(int, int, int, char *, int, int);

/* fhpib.c */
int fhpibinit(int);
void fhpibreset(int);
int fhpibsend(int, int, int, char *, int);
int fhpibrecv(int, int, int, char *, int);
int fhpibppoll(int);

/* nhpib.c */
int nhpibinit(int);
void nhpibreset(int);
int nhpibsend(int, int, int, char *, int);
int nhpibrecv(int, int, int, char *, int);
int nhpibppoll(int);

