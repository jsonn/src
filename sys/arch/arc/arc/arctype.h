/*	$NetBSD: arctype.h,v 1.6.2.1 2000/06/22 16:59:04 minoura Exp $	*/
/*	$OpenBSD: archtype.h,v 1.6 1999/01/27 04:46:04 imp Exp $	*/

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Define architectural identitys for the different Mips machines.
 */
#define	ARC_CLASS		0x00	/* Arch class ARC */
#define	ACER_PICA_61		0x01	/* Acer Labs Pica 61 */
#define	MAGNUM			0x02	/* Mips MAGNUM R4000 */
#define NEC_R94			0x03	/* NEC RISCstation 2200 EISA */
#define NEC_RAx94		0x04	/* NEC RISCstation 2200 PCI */
#define	NEC_RD94		0x05	/* NEC RISCstation 2250 */
#define	NEC_R96			0x06	/* NEC RISCserver 2200 */
#define	DESKSTATION_RPC44	0x07	/* Deskstation xxx */
#define	DESKSTATION_TYNE	0x08	/* Deskstation xxx */
#define	NKK_AQUARIUS		0x09	/* NKK R4{67}00 PC */
#define	SNI_RM200		0x0a	/* Siemens Nixdorf RM200 */

#if 0 /* XXX */
#define	SGI_CLASS		0x10	/* Silicon Graphics Class */
#define	SGI_CRIMSON		0x11	/* Crimson */
#define	SGI_ONYX		0x12	/* Onyx (!S model Challenge) */
#define	SGI_INDIGO		0x13	/* Indigo */
#define	SGI_POWER		0x14	/* POWER Challenge, POWER Onyx */
#define	SGI_INDY		0x15	/* Indy, Indigo2, Challenge S */
#define	SGI_POWER10		0x16	/* POWER Challenge R10k */
#define	SGI_POWERI		0x17	/* POWER Indigo2 */
#define	SGI_O2			0x18	/* O2/Moosehead */
#endif

#define	ALGOR_CLASS		0x20	/* Algorithmics Class */
#define	ALGOR_P4032		0x21	/* ALGORITHMICS P-4032 */
#define	ALGOR_P5064		0x22	/* ALGORITHMICS P-5064 */

extern int cputype;			/* Global system type indicator */
