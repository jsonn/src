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
 *	from: @(#)nexus.h	7.3 (Berkeley) 5/9/91
 *	$Id: nexus.h,v 1.1.2.2 1994/08/02 20:20:40 ragge Exp $
 */

/*
 * Information about nexus's.
 *
 * Each machine has an address of backplane slots (nexi).
 * Each nexus is some type of adapter, whose code is the low
 * byte of the first word of the adapter address space.
 * At boot time the system looks through the array of available
 * slots and finds the interconnects for the machine.
 */
#define IO_CMI750       2
#define MAXNMCR         1

#define	NNEXSBI		16
#if VAX8600
#define	NNEX8600	NNEXSBI
#define	NEXA8600	((struct nexus *)(0x20000000))
#define	NEXB8600	((struct nexus *)(0x22000000))
#endif
#if VAX780
#define	NNEX780	NNEXSBI
#define	NEX780	((struct nexus *)0x20000000)
#endif
#if VAX750
#define	NNEX750	NNEXSBI
#ifndef ASSEMBLER
#define NEX750           ((struct nexus*)0xf20000)
#else
#define NEX750           (0xF20000)
#endif
#endif
#if VAX730
#define	NNEX730	NNEXSBI
#define	NEX730	((struct nexus *)0xf20000)
#endif
#define	NEXSIZE	0x2000

#if VAX8600
#define	MAXNNEXUS (2 * NNEXSBI)
#else 
#define	MAXNNEXUS NNEXSBI
#endif

#ifndef ASSEMBLER

#include "sys/types.h"

struct	nexus {
	union nexcsr {
		long	nex_csr;
		u_char	nex_type;
	} nexcsr;
	long	nex_pad[NEXSIZE / sizeof (long) - 1];
};

struct iobus {
        int io_type;
        int io_addr;
        int io_size;
        int io_details;
};

struct nexusconnect {
        int psb_nnexus;
        struct nexus *psb_nexbase;
	int psb_ubatype;
	int psb_nubabdp;
	caddr_t *psb_umaddr;
        int *psb_nextype;
};

extern caddr_t *nex_vec;
#define nex_vec_num(ipl, nexnum) nex_vec[(ipl-14)*16+nexnum]

/* XXX
#ifdef	KERNEL
struct nexus nexus[MAXNNEXUS];
#endif
*/
#endif

/*
 * Bits in high word of nexus's.
 */
#define	SBI_PARFLT	(1<<31)		/* sbi parity fault */
#define	SBI_WSQFLT	(1<<30)		/* write sequence fault */
#define	SBI_URDFLT	(1<<29)		/* unexpected read data fault */
#define	SBI_ISQFLT	(1<<28)		/* interlock sequence fault */
#define	SBI_MXTFLT	(1<<27)		/* multiple transmitter fault */
#define	SBI_XMTFLT	(1<<26)		/* transmit fault */

#define	NEX_CFGFLT	(0xfc000000)

#ifndef LOCORE
#if defined(VAX780) || defined(VAX8600)
#define	NEXFLT_BITS \
"\20\40PARFLT\37WSQFLT\36URDFLT\35ISQFLT\34MXTFLT\33XMTFLT"
#endif
#endif

#define	NEX_APD		(1<<23)		/* adaptor power down */
#define	NEX_APU		(1<<22)		/* adaptor power up */

#define	MBA_OT		(1<<21)		/* overtemperature */

#define	UBA_UBINIT	(1<<18)		/* unibus init */
#define	UBA_UBPDN	(1<<17)		/* unibus power down */
#define	UBA_UBIC	(1<<16)		/* unibus initialization complete */

/*
 * Types for nex_type.
 */
#define	NEX_ANY		0		/* pseudo for handling 11/750 */
#define	NEX_MEM4	0x08		/* 4K chips, non-interleaved mem */
#define	NEX_MEM4I	0x09		/* 4K chips, interleaved mem */
#define	NEX_MEM16	0x10		/* 16K chips, non-interleaved mem */
#define	NEX_MEM16I	0x11		/* 16K chips, interleaved mem */
#define	NEX_MBA		0x20		/* Massbus adaptor */
#define	NEX_UBA0	0x28		/* Unibus adaptor */
#define	NEX_UBA1	0x29		/* 4 flavours for 4 addr spaces */
#define	NEX_UBA2	0x2a
#define	NEX_UBA3	0x2b
#define	NEX_DR32	0x30		/* DR32 user i'face to SBI */
#define	NEX_CI		0x38		/* CI adaptor */
#define	NEX_MPM0	0x40		/* Multi-port mem */
#define	NEX_MPM1	0x41		/* Who knows why 4 different ones ? */
#define	NEX_MPM2	0x42
#define	NEX_MPM3	0x43
#define	NEX_MEM64L	0x68		/* 64K chips, non-interleaved, lower */
#define	NEX_MEM64LI	0x69		/* 64K chips, ext-interleaved, lower */
#define	NEX_MEM64U	0x6a		/* 64K chips, non-interleaved, upper */
#define	NEX_MEM64UI	0x6b		/* 64K chips, ext-interleaved, upper */
#define	NEX_MEM64I	0x6c		/* 64K chips, interleaved */
#define	NEX_MEM256L	0x70		/* 256K chips, non-interleaved, lower */
#define	NEX_MEM256LI	0x71		/* 256K chips, ext-interleaved, lower */
#define	NEX_MEM256U	0x72		/* 256K chips, non-interleaved, upper */
#define	NEX_MEM256UI	0x73		/* 256K chips, ext-interleaved, upper */
#define	NEX_MEM256I	0x74		/* 256K chips, interleaved */
