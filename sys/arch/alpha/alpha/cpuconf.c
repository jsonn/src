/*	$NetBSD: cpuconf.c,v 1.6.2.2 1997/09/04 00:52:22 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * CPU (machine) type configuration switch.
 *
 * This table should probably go at the end of conf.c, but
 * I didn't want to make conf.c "different."
 */

#include <sys/param.h>
#include <sys/device.h>
#include <machine/cpuconf.h>

#include "dec_2100_a50.h"
#include "dec_3000_300.h"
#include "dec_3000_500.h"
#include "dec_axppci_33.h"
#include "dec_eb164.h"
#include "dec_eb64plus.h"
#include "dec_kn20aa.h"
#include "dec_kn8ae.h"

const struct cpusw cpusw[] = {
	cpu_unknown(),				/*  0: ??? */
	cpu_notdef("Alpha Demonstration Unit"),	/*  1: ST_ADU */
	cpu_notdef("DEC 4000 (\"Cobra\")"),	/*  2: ST_DEC_4000 */
	cpu_notdef("DEC 7000 (\"Ruby\")"),	/*  3: ST_DEC_7000 */
	cpu_init("DEC 3000/500 (\"Flamingo\")",DEC_3000_500,dec_3000_500),
						/*  4: ST_DEC_3000_500 */
	cpu_unknown(),				/*  5: ??? */
	cpu_notdef("DEC 2000/300 (\"Jensen\")"),
						/*  6: ST_DEC_2000_300 */
	cpu_init("DEC 3000/300 (\"Pelican\")",DEC_3000_300,dec_3000_300),
						/*  7: ST_DEC_3000_300 */
	cpu_unknown(),				/*  8: ??? */
	cpu_notdef("DEC 2100/A500 (\"Sable\")"),
						/*  9: ST_DEC_2100_A500 */
	cpu_notdef("AXPvme 64"),		/* 10: ST_DEC_APXVME_64 */
	cpu_init("DEC AXPpci",DEC_AXPPCI_33,dec_axppci_33),
						/* 11: ST_DEC_AXPPCI_33 */
	cpu_init("AlphaServer 8400",DEC_KN8AE,dec_kn8ae),
						/* 12: ST_DEC_21000 */
	cpu_init("AlphaStation 200/400 (\"Avanti\")",DEC_2100_A50,dec_2100_a50),
						/* 13: ST_DEC_2100_A50 */
	cpu_notdef("Mustang"),			/* 14: ST_DEC_MUSTANG */
	cpu_init("AlphaStation 600 (KN20AA)",DEC_KN20AA,dec_kn20aa),
						/* 15: ST_DEC_KN20AA */
	cpu_unknown(),				/* 16: ??? */
	cpu_notdef("DEC 1000 (\"Mikasa\")"),	/* 17: ST_DEC_1000 */
	cpu_unknown(),				/* 18: ??? */
	cpu_notdef("EB66"),			/* 19: ST_EB66 */
	cpu_init("EB64+",DEC_EB64PLUS,dec_eb64plus),
						/* 20: ST_EB64P */
	cpu_unknown(),				/* 21: ??? */
	cpu_notdef("DEC 4100 (\"Rawhide\")"),	/* 22: ST_DEC_4100 */
	cpu_notdef("??? (\"Lego\")"),		/* 23: ST_DEC_EV45_PBP */
	cpu_notdef("DEC 2100A/A500 (\"Lynx\")"),
						/* 24: ST_DEC_2100A_A500 */
	cpu_unknown(),				/* 25: ??? */
	cpu_init("EB164",DEC_EB164,dec_eb164),	/* 26: ST_EB164 */
	cpu_notdef("DEC 1000A (\"Noritake\")"),	/* 27: ST_DEC_1000A */
	cpu_notdef("AlphaVME 224 (\"Cortex\")"),
						/* 28: ST_DEC_ALPHAVME_224 */
};
const int ncpusw = sizeof (cpusw) / sizeof (cpusw[0]);
