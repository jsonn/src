/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: podules,v 1.7 1997/10/14 22:22:08 mark Exp 
 */

/*
 * Copyright (c) 1996 Mark Brinicombe
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
 *      This product includes software developed by Mark Brinicombe
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
 * List of known podule manufacturers
 */

#define	MANUFACTURER_ACORN	0x0000		/* Acorn Computers */
#define	MANUFACTURER_OLIVETTI	0x0002		/* Olivetti */
#define	MANUFACTURER_WATFORD	0x0003		/* Watford Electronics */
#define	MANUFACTURER_CCONCEPTS	0x0004		/* Computer Concepts */
#define	MANUFACTURER_ARMADILLO	0x0007		/* Armadillo Systems */
#define	MANUFACTURER_WILDVISION	0x0009		/* Wild Vision */
#define	MANUFACTURER_ATOMWIDE	0x0011		/* Atomwide */
#define	MANUFACTURER_ATOMWIDE2	0x0017		/* Atomwide */
#define	MANUFACTURER_LINGENUITY	0x001a		/* Lingenuity */
#define	MANUFACTURER_IRLAM	0x001f		/* Irlam Instruments */
#define	MANUFACTURER_OAK	0x0021		/* Oak Solutions */
#define	MANUFACTURER_MORLEY	0x002b		/* Morley */
#define	MANUFACTURER_VTI	0x0035		/* Vertical Twist */
#define	MANUFACTURER_CUMANA	0x003a		/* Cumana */
#define	MANUFACTURER_ICS	0x003c		/* ICS */
#define	MANUFACTURER_SERIALPORT	0x003f		/* Serial Port */
#define	MANUFACTURER_ARXE	0x0041		/* ARXE */
#define	MANUFACTURER_ALEPH1	0x0042		/* Aleph 1 */
#define	MANUFACTURER_ICUBED	0x0046		/* I-Cubed */
#define	MANUFACTURER_BRINI	0x0050		/* Brini */
#define	MANUFACTURER_ANT	0x0053		/* ANT */
#define	MANUFACTURER_ALSYSTEMS	0x005b		/* Alsystems */
#define	MANUFACTURER_SIMTEC	0x005f		/* Simtec Electronics */
#define	MANUFACTURER_YES	0x0060		/* Yellowstone Educational Solutions */
#define	MANUFACTURER_MCS	0x0063		/* MCS */

/*
 * List of known podules.  Grouped by vendor.
 */

#define	PODULE_ACORN_ETHER3XXX	0x0000		/* Ether3 (NOROM) */
#define	PODULE_ACORN_SCSI	0x0002		/* SCSI 1 interface */
#define	PODULE_ACORN_ETHER1	0x0003		/* ether 1 interface */
#define	PODULE_ACORN_RAMROM	0x0005		/* RAM/ROM podule */
#define	PODULE_ACORN_BBCIO	0x0006		/* BBC IO interface */
#define	PODULE_ACORN_ST506	0x000b		/* ST506 HD interface */
#define	PODULE_ACORN_MIDI	0x0013		/* MIDI interface */
#define	PODULE_ACORN_ETHER2	0x0061		/* ether 2 interface */

#define	PODULE_CCONCEPTS_LASERDIRECT	0x0014		/* laser direct (Canon LBP-4) */

#define	PODULE_ARMADILLO_A448	0x0016		/* A448 sound sampler */

#define	PODULE_WILDVISION_HAWKV9	0x0052		/* hawk v9 mark2 */
#define	PODULE_WILDVISION_SCANLIGHTV256	0x00cb		/* scanlight video 256 */
#define	PODULE_WILDVISION_EAGLEM2	0x00cc		/* eagle M2 */
#define	PODULE_WILDVISION_LARKA16	0x00ce		/* lark A16 */
#define	PODULE_WILDVISION_MIDIMAX	0x0200		/* MIDI max */

#define	PODULE_ATOMWIDE_ETHER3	0x00A4		/* ether 3/5 interface */

#define	PODULE_ATOMWIDE2_SERIAL	0x0090		/* serial interface */

#define	PODULE_LINGENUITY_SCSI	0x0095		/* 16 bit SCSI interface */

#define	PODULE_IRLAM_24I16	0x00e6		/* 24i16 digitiser */

#define	PODULE_OAK_SCSI	0x0058		/* 16 bit SCSI interface */

#define	PODULE_MORLEY_SCSI	0x0067		/* SCSI interface */

#define	PODULE_VTI_SCSI	0x008d		/* SCSI interface */

#define	PODULE_CUMANA_SCSI2	0x003a		/* SCSI II interface */
#define	PODULE_CUMANA_SCSI1	0x00a0		/* SCSI I interface */
#define	PODULE_CUMANA_SLCD	0x00dd		/* CDFS & SLCD expansion card */

#define	PODULE_ICS_IDE	0x00ae		/* IDE Interface */

#define	PODULE_SERIALPORT_DUALSERIAL	0x00b9		/* Serial interface */

#define	PODULE_ARXE_SCSI	0x0041		/* 16 bit SCSI interface */

#define	PODULE_ALEPH1_PCCARD	0x00ea		/* PC card */

#define	PODULE_ICUBED_ETHERLAN600	0x00ec		/* etherlan 600 network slot interface */
#define	PODULE_ICUBED_ETHERLAN600A	0x011e		/* etherlan 600A network slot interface */
#define	PODULE_ICUBED_ETHERLAN500	0x00d4		/* etherlen 500 interface */
#define	PODULE_ICUBED_ETHERLAN500A	0x011f		/* etherlen 500A interface */
#define	PODULE_ICUBED_ETHERLAN200	0x00bd		/* etherlen 200 interface */
#define	PODULE_ICUBED_ETHERLAN200A	0x011d		/* etherlen 200A interface */
#define	PODULE_ICUBED_ETHERLAN100	0x00c4		/* etherlen 100 interface */
#define	PODULE_ICUBED_ETHERLAN100A	0x011c		/* etherlen 100A interface */

#define	PODULE_BRINI_PORT	0x0000		/* BriniPort intelligent I/O interface */
#define	PODULE_BRINI_LINK	0x00df		/* BriniLink transputer link adapter */

#define	PODULE_ANT_ETHER3	0x00a4		/* ether 3/5 interface */
#define	PODULE_ANT_ETHERB	0x00e4		/* ether B network slot interface */
#define	PODULE_ANT_ETHERM	0x00d8		/* ether M dual interface NIC */

#define	PODULE_ALSYSTEMS_SCSI	0x0107		/* SCSI II host adapter */

#define	PODULE_SIMTEC_IDE8	0x0130		/* 8 bit IDE interface */
#define	PODULE_SIMTEC_IDE	0x0131		/* 16 bit IDE interface */

#define	PODULE_YES_RAPIDE	0x0114		/* RapIDE32 interface */

#define	PODULE_MCS_SCSI	0x0125		/* Connect32 SCSI II interface */
