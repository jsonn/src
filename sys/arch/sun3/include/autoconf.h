/*	$NetBSD: autoconf.h,v 1.18.48.2 2005/11/10 13:59:54 skrll Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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

/*
 * Autoconfiguration information.
 * (machdep parts of driver/kernel interface)
 */

/*
 * These are the "bus" types, in attach order.
 * Note tables in bus_subr.c and vme.c that
 * care about the order of these.
 */
#define	BUS_OBIO    	0	/* on-board I/O */
#define	BUS_OBMEM   	1	/* on-board memory */
#define	BUS_VME16D16	2	/* VME A16/D16 */
#define	BUS_VME16D32	3	/* VME A16/D32 */
#define	BUS_VME24D16	4	/* VME A24/D16 */
#define	BUS_VME24D32	5	/* VME A24/D32 */
#define	BUS_VME32D16	6	/* VME A32/D16 */
#define	BUS_VME32D32	7	/* VME A32/D32 */
#define BUS__NTYPES 	8	/* not a valid bus type */

/*
 * This is the "args" parameter to the bus match/attach functions.
 */
struct confargs {
	int ca_bustype;		/* BUS_OBIO, ... */
	int ca_paddr;		/* physical address */
	int ca_intpri;		/* interrupt priority level */
	int ca_intvec;		/* interrupt vector index */
};

/* Locator aliases */
#define cf_paddr	cf_loc[0]
#define cf_intpri	cf_loc[1]
#define cf_intvec	cf_loc[2]

int bus_scan(struct device *, struct cfdata *, const int *, void *);
int bus_print(void *, const char *);
int bus_peek(int, int, int);
void * bus_mapin(int, int, int);
void bus_mapout(void *, int);
void * bus_tmapin(int, int);
void bus_tmapout(void *);

/* These are how drivers connect interrupt handlers. */
typedef int (*isr_func_t)(void *);
void isr_add_autovect(isr_func_t, void *, int);
void isr_add_vectored(isr_func_t, void *, int, int);
void isr_add_custom(int, void *);

/* These control the software interrupt register. */
void isr_soft_request(int);
void isr_soft_clear(int);

/* Bus-error tolerant access to mapped address. */
int 	peek_byte(caddr_t);
int 	peek_word(caddr_t);
int 	peek_long(caddr_t);
