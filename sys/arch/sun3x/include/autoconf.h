/*	$NetBSD: autoconf.h,v 1.1.1.1.2.2 1997/01/14 20:57:05 gwr Exp $	*/

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

/* These are the "bus" types: */
#define	BUS_OBMEM	0	/* "obmem" */
#define	BUS_OBIO	1	/* "obio"  */
#define	BUS_VME16	2	/* "vmes"  */
#define	BUS_VME32	3	/* "vmel"  */

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

int bus_scan __P((struct device *, struct cfdata *, void *));
int bus_print __P((void *, const char *));
int bus_peek __P((int, int, int));
char * bus_mapin __P((int, int, int));

/* These are how drivers connect interrupt handlers. */
typedef int (*isr_func_t) __P((void *));
void isr_add_autovect __P((isr_func_t, void *arg, int level));
void isr_add_vectored __P((isr_func_t, void *arg, int pri, int vec));
void isr_add_custom __P((int, void *));

/* These control the software interrupt register. */
void isr_soft_request __P((int level));
void isr_soft_clear __P((int level));

/* Bus-error tolerant access to mapped address. */
int 	peek_byte __P((caddr_t));
int 	peek_word __P((caddr_t));
