/*	$NetBSD: conf.c,v 1.61.6.2 2004/09/18 14:42:13 skrll Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)conf.c	7.18 (Berkeley) 5/9/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: conf.c,v 1.61.6.2 2004/09/18 14:42:13 skrll Exp $");

#include "opt_cputype.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <machine/cpu.h>

/*
 * Console routines for VAX console.
 */
#include <dev/cons.h>

#include "smg.h"
#include "lcspx.h"
#include "lcg.h"
#include "wskbd.h"
#if NSMG > 0
#if NWSKBD > 0
#define smgcngetc wskbd_cngetc
#else
static int
smgcngetc(dev_t dev)
{
	return 0;
}
#endif

#define smgcnputc wsdisplay_cnputc
#define	smgcnpollc nullcnpollc
#endif
#if NLCSPX > 0
#if NWSKBD > 0
#define lcspxcngetc wskbd_cngetc
#else
static int
lcspxcngetc(dev_t dev)
{
	return 0;
}
#endif

#define lcspxcnputc wsdisplay_cnputc
#define lcspxcnpollc nullcnpollc
#endif
#if NLCG > 0
#if NWSKBD > 0
#define lcgcngetc wskbd_cngetc
#else
static int
lcgcngetc(dev_t dev)
{
	return 0;
}
#endif

#define lcgcnputc wsdisplay_cnputc
#define lcgcnpollc nullcnpollc
#endif

cons_decl(gen);
cons_decl(dz);
cons_decl(qd);
cons_decl(smg);
cons_decl(lcspx);
cons_decl(lcg);
#include "qv.h"
#include "qd.h"

struct	consdev constab[]={
#if VAX8600 || VAX8200 || VAX780 || VAX750 || VAX650 || VAX630 || VAX660 || \
	VAX670 || VAX680 || VAX8800 || VAXANY
	cons_init(gen), /* Generic console type; mtpr/mfpr */
#endif
#if VAX410 || VAX43 || VAX46 || VAX48 || VAX49 || VAX53 || VAXANY
	cons_init(dz),	/* DZ11-like serial console on VAXstations */
#endif
#if VAX650 || VAX630 || VAXANY
#if NQV
	cons_init(qv),	/* QVSS/QDSS bit-mapped console driver */
#endif
#if NQD
	cons_init(qd),
#endif
#endif
#if NSMG
	cons_init(smg),
#endif
#if NLCSPX
	cons_init(lcspx),
#endif
#if NLCG
	cons_init(lcg),
#endif

#ifdef notyet
/* We may not always use builtin console, sometimes RD */
	{ rdcnprobe, rdcninit, rdcngetc, rdcnputc },
#endif
	{ 0 }
};
