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

static struct podule_description podules_acorn[] = {
	{ PODULE_ACORN_ETHER3XXX,	"Ether3 (NOROM)" },
	{ PODULE_ACORN_SCSI,	"SCSI 1 interface" },
	{ PODULE_ACORN_ETHER1,	"ether 1 interface" },
	{ PODULE_ACORN_RAMROM,	"RAM/ROM podule" },
	{ PODULE_ACORN_BBCIO,	"BBC IO interface" },
	{ PODULE_ACORN_ST506,	"ST506 HD interface" },
	{ PODULE_ACORN_MIDI,	"MIDI interface" },
	{ PODULE_ACORN_ETHER2,	"ether 2 interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_olivetti[] = {
	{ 0x0000, NULL }
};

static struct podule_description podules_watford[] = {
	{ 0x0000, NULL }
};

static struct podule_description podules_cconcepts[] = {
	{ PODULE_CCONCEPTS_LASERDIRECT,	"laser direct (Canon LBP-4)" },
	{ 0x0000, NULL }
};

static struct podule_description podules_armadillo[] = {
	{ PODULE_ARMADILLO_A448,	"A448 sound sampler" },
	{ 0x0000, NULL }
};

static struct podule_description podules_wildvision[] = {
	{ PODULE_WILDVISION_HAWKV9,	"hawk v9 mark2" },
	{ PODULE_WILDVISION_SCANLIGHTV256,	"scanlight video 256" },
	{ PODULE_WILDVISION_EAGLEM2,	"eagle M2" },
	{ PODULE_WILDVISION_LARKA16,	"lark A16" },
	{ PODULE_WILDVISION_MIDIMAX,	"MIDI max" },
	{ 0x0000, NULL }
};

static struct podule_description podules_atomwide[] = {
	{ PODULE_ATOMWIDE_ETHER3,	"ether 3/5 interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_atomwide2[] = {
	{ PODULE_ATOMWIDE2_SERIAL,	"serial interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_lingenuity[] = {
	{ PODULE_LINGENUITY_SCSI,	"16 bit SCSI interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_irlam[] = {
	{ PODULE_IRLAM_24I16,	"24i16 digitiser" },
	{ 0x0000, NULL }
};

static struct podule_description podules_oak[] = {
	{ PODULE_OAK_SCSI,	"16 bit SCSI interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_morley[] = {
	{ PODULE_MORLEY_SCSI,	"SCSI interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_vti[] = {
	{ PODULE_VTI_SCSI,	"SCSI interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_cumana[] = {
	{ PODULE_CUMANA_SCSI2,	"SCSI II interface" },
	{ PODULE_CUMANA_SCSI1,	"SCSI I interface" },
	{ PODULE_CUMANA_SLCD,	"CDFS & SLCD expansion card" },
	{ 0x0000, NULL }
};

static struct podule_description podules_ics[] = {
	{ PODULE_ICS_IDE,	"IDE Interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_serialport[] = {
	{ PODULE_SERIALPORT_DUALSERIAL,	"Serial interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_arxe[] = {
	{ PODULE_ARXE_SCSI,	"16 bit SCSI interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_aleph1[] = {
	{ PODULE_ALEPH1_PCCARD,	"PC card" },
	{ 0x0000, NULL }
};

static struct podule_description podules_icubed[] = {
	{ PODULE_ICUBED_ETHERLAN600,	"etherlan 600 network slot interface" },
	{ PODULE_ICUBED_ETHERLAN600A,	"etherlan 600A network slot interface" },
	{ PODULE_ICUBED_ETHERLAN500,	"etherlen 500 interface" },
	{ PODULE_ICUBED_ETHERLAN500A,	"etherlen 500A interface" },
	{ PODULE_ICUBED_ETHERLAN200,	"etherlen 200 interface" },
	{ PODULE_ICUBED_ETHERLAN200A,	"etherlen 200A interface" },
	{ PODULE_ICUBED_ETHERLAN100,	"etherlen 100 interface" },
	{ PODULE_ICUBED_ETHERLAN100A,	"etherlen 100A interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_brini[] = {
	{ PODULE_BRINI_PORT,	"BriniPort intelligent I/O interface" },
	{ PODULE_BRINI_LINK,	"BriniLink transputer link adapter" },
	{ 0x0000, NULL }
};

static struct podule_description podules_ant[] = {
	{ PODULE_ANT_ETHER3,	"ether 3/5 interface" },
	{ PODULE_ANT_ETHERB,	"ether B network slot interface" },
	{ PODULE_ANT_ETHERM,	"ether M dual interface NIC" },
	{ 0x0000, NULL }
};

static struct podule_description podules_alsystems[] = {
	{ PODULE_ALSYSTEMS_SCSI,	"SCSI II host adapter" },
	{ 0x0000, NULL }
};

static struct podule_description podules_simtec[] = {
	{ PODULE_SIMTEC_IDE8,	"8 bit IDE interface" },
	{ PODULE_SIMTEC_IDE,	"16 bit IDE interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_yes[] = {
	{ PODULE_YES_RAPIDE,	"RapIDE32 interface" },
	{ 0x0000, NULL }
};

static struct podule_description podules_mcs[] = {
	{ PODULE_MCS_SCSI,	"Connect32 SCSI II interface" },
	{ 0x0000, NULL }
};


struct podule_list known_podules[] = {
	{ MANUFACTURER_ACORN, 		"Acorn Computers", 	podules_acorn },
	{ MANUFACTURER_OLIVETTI, 	"Olivetti", 	podules_olivetti },
	{ MANUFACTURER_WATFORD, 	"Watford Electronics", 	podules_watford },
	{ MANUFACTURER_CCONCEPTS, 	"Computer Concepts", 	podules_cconcepts },
	{ MANUFACTURER_ARMADILLO, 	"Armadillo Systems", 	podules_armadillo },
	{ MANUFACTURER_WILDVISION, 	"Wild Vision", 	podules_wildvision },
	{ MANUFACTURER_ATOMWIDE, 	"Atomwide", 	podules_atomwide },
	{ MANUFACTURER_ATOMWIDE2, 	"Atomwide", 	podules_atomwide2 },
	{ MANUFACTURER_LINGENUITY, 	"Lingenuity", 	podules_lingenuity },
	{ MANUFACTURER_IRLAM, 		"Irlam Instruments", 	podules_irlam },
	{ MANUFACTURER_OAK, 		"Oak Solutions", 	podules_oak },
	{ MANUFACTURER_MORLEY, 		"Morley", 	podules_morley },
	{ MANUFACTURER_VTI, 		"Vertical Twist", 	podules_vti },
	{ MANUFACTURER_CUMANA, 		"Cumana", 	podules_cumana },
	{ MANUFACTURER_ICS, 		"ICS", 	podules_ics },
	{ MANUFACTURER_SERIALPORT, 	"Serial Port", 	podules_serialport },
	{ MANUFACTURER_ARXE, 		"ARXE", 	podules_arxe },
	{ MANUFACTURER_ALEPH1, 		"Aleph 1", 	podules_aleph1 },
	{ MANUFACTURER_ICUBED, 		"I-Cubed", 	podules_icubed },
	{ MANUFACTURER_BRINI, 		"Brini", 	podules_brini },
	{ MANUFACTURER_ANT, 		"ANT", 	podules_ant },
	{ MANUFACTURER_ALSYSTEMS, 	"Alsystems", 	podules_alsystems },
	{ MANUFACTURER_SIMTEC, 		"Simtec Electronics", 	podules_simtec },
	{ MANUFACTURER_YES, 		"Yellowstone Educational Solutions", 	podules_yes },
	{ MANUFACTURER_MCS, 		"MCS", 	podules_mcs },
	{ 0, NULL, NULL }
};
