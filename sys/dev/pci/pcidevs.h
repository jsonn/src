/*	$NetBSD: pcidevs.h,v 1.772.2.3 2006/05/11 23:28:48 elad Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: pcidevs,v 1.786 2006/04/30 18:57:51 xtraeme Exp
 */

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 * NOTE: a fairly complete list of PCI codes can be found at:
 *
 *	http://www.pcidatabase.com/
 *
 * (but it doesn't always seem to match vendor documentation)
 *
 * NOTE: As per tron@NetBSD.org, the proper update procedure is
 *
 * 1.) Change "src/sys/dev/pci/pcidevs".
 * 2.) Commit "src/sys/dev/pci/pcidevs".
 * 3.) Execute "make -f Makefile.pcidevs" in "src/sys/dev/pci".
 * 4.) Commit "src/sys/dev/pci/pcidevs.h" and "src/sys/dev/pci/pcidevs_data.h".
 */

/*
 * List of known PCI vendors
 */

#define	PCI_VENDOR_PEAK	0x001c		/* Peak System Technik */
#define	PCI_VENDOR_MARTINMARIETTA	0x003d		/* Martin-Marietta */
#define	PCI_VENDOR_HAUPPAUGE	0x0070		/* Hauppauge Computer Works */
#define	PCI_VENDOR_DYNALINK	0x0675		/* Dynalink */
#define	PCI_VENDOR_COMPAQ	0x0e11		/* Compaq */
#define	PCI_VENDOR_SYMBIOS	0x1000		/* Symbios Logic */
#define	PCI_VENDOR_ATI	0x1002		/* ATI Technologies */
#define	PCI_VENDOR_ULSI	0x1003		/* ULSI Systems */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI Technology */
#define	PCI_VENDOR_AVANCE	0x1005		/* Avance Logic */
#define	PCI_VENDOR_REPLY	0x1006		/* Reply Group */
#define	PCI_VENDOR_NETFRAME	0x1007		/* NetFrame Systems */
#define	PCI_VENDOR_EPSON	0x1008		/* Epson */
#define	PCI_VENDOR_PHOENIX	0x100a		/* Phoenix Technologies */
#define	PCI_VENDOR_NS	0x100b		/* National Semiconductor */
#define	PCI_VENDOR_TSENG	0x100c		/* Tseng Labs */
#define	PCI_VENDOR_AST	0x100d		/* AST Research */
#define	PCI_VENDOR_WEITEK	0x100e		/* Weitek */
#define	PCI_VENDOR_VIDEOLOGIC	0x1010		/* Video Logic */
#define	PCI_VENDOR_DEC	0x1011		/* Digital Equipment */
#define	PCI_VENDOR_MICRONICS	0x1012		/* Micronics Computers */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_LSIL	0x1015		/* LSI Logic of Canada */
#define	PCI_VENDOR_ICLPERSONAL	0x1016		/* ICL Personal Systems */
#define	PCI_VENDOR_SPEA	0x1017		/* SPEA Software */
#define	PCI_VENDOR_UNISYS	0x1018		/* Unisys Systems */
#define	PCI_VENDOR_ELITEGROUP	0x1019		/* Elitegroup Computer Systems */
#define	PCI_VENDOR_NCR	0x101a		/* AT&T Global Information Systems */
#define	PCI_VENDOR_VITESSE	0x101b		/* Vitesse Semiconductor */
#define	PCI_VENDOR_WD	0x101c		/* Western Digital */
#define	PCI_VENDOR_AMI	0x101e		/* American Megatrends */
#define	PCI_VENDOR_PICTURETEL	0x101f		/* PictureTel */
#define	PCI_VENDOR_HITACHICOMP	0x1020		/* Hitachi Computer Products */
#define	PCI_VENDOR_OKI	0x1021		/* OKI Electric Industry */
#define	PCI_VENDOR_AMD	0x1022		/* Advanced Micro Devices */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident Microsystems */
#define	PCI_VENDOR_ZENITH	0x1024		/* Zenith Data Systems */
#define	PCI_VENDOR_ACER	0x1025		/* Acer */
#define	PCI_VENDOR_DELL	0x1028		/* Dell Computer */
#define	PCI_VENDOR_SNI	0x1029		/* Siemens Nixdorf AG */
#define	PCI_VENDOR_LSILOGIC	0x102a		/* LSI Logic, Headland div. */
#define	PCI_VENDOR_MATROX	0x102b		/* Matrox */
#define	PCI_VENDOR_CHIPS	0x102c		/* Chips and Technologies */
#define	PCI_VENDOR_WYSE	0x102d		/* WYSE Technology */
#define	PCI_VENDOR_OLIVETTI	0x102e		/* Olivetti Advanced Technology */
#define	PCI_VENDOR_TOSHIBA	0x102f		/* Toshiba America */
#define	PCI_VENDOR_TMCRESEARCH	0x1030		/* TMC Research */
#define	PCI_VENDOR_MIRO	0x1031		/* Miro Computer Products */
#define	PCI_VENDOR_COMPAQ2	0x1032		/* Compaq (2nd PCI Vendor ID) */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_BURNDY	0x1034		/* Burndy */
#define	PCI_VENDOR_COMPCOMM	0x1035		/* Comp. & Comm. Research Lab */
#define	PCI_VENDOR_FUTUREDOMAIN	0x1036		/* Future Domain */
#define	PCI_VENDOR_HITACHIMICRO	0x1037		/* Hitach Microsystems */
#define	PCI_VENDOR_AMP	0x1038		/* AMP */
#define	PCI_VENDOR_SIS	0x1039		/* Silicon Integrated System */
#define	PCI_VENDOR_SEIKOEPSON	0x103a		/* Seiko Epson */
#define	PCI_VENDOR_TATUNGAMERICA	0x103b		/* Tatung of America */
#define	PCI_VENDOR_HP	0x103c		/* Hewlett-Packard */
#define	PCI_VENDOR_SOLLIDAY	0x103e		/* Solliday Engineering */
#define	PCI_VENDOR_LOGICMODELLING	0x103f		/* Logic Modeling */
#define	PCI_VENDOR_KPC	0x1040		/* Kubota Pacific */
#define	PCI_VENDOR_COMPUTREND	0x1041		/* Computrend */
#define	PCI_VENDOR_PCTECH	0x1042		/* PC Technology */
#define	PCI_VENDOR_ASUSTEK	0x1043		/* Asustek Computer */
#define	PCI_VENDOR_DPT	0x1044		/* Distributed Processing Technology */
#define	PCI_VENDOR_OPTI	0x1045		/* Opti */
#define	PCI_VENDOR_IPCCORP	0x1046		/* IPC */
#define	PCI_VENDOR_GENOA	0x1047		/* Genoa Systems */
#define	PCI_VENDOR_ELSA	0x1048		/* Elsa */
#define	PCI_VENDOR_FOUNTAINTECH	0x1049		/* Fountain Technology */
#define	PCI_VENDOR_SGSTHOMSON	0x104a		/* SGS-Thomson Microelectronics */
#define	PCI_VENDOR_BUSLOGIC	0x104b		/* BusLogic */
#define	PCI_VENDOR_TI	0x104c		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104d		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_COTIME	0x104f		/* Co-time Computer */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond Electronics */
#define	PCI_VENDOR_ANIGMA	0x1051		/* Anigma */
#define	PCI_VENDOR_YOUNGMICRO	0x1052		/* Young Micro Systems */
#define	PCI_VENDOR_HITACHI	0x1054		/* Hitachi */
#define	PCI_VENDOR_EFARMICRO	0x1055		/* Efar Microsystems */
#define	PCI_VENDOR_ICL	0x1056		/* ICL */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_ETR	0x1058		/* Electronics & Telec. RSH */
#define	PCI_VENDOR_TEKNOR	0x1059		/* Teknor Microsystems */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise Technology */
#define	PCI_VENDOR_FOXCONN	0x105b		/* Foxconn International */
#define	PCI_VENDOR_WIPRO	0x105c		/* Wipro Infotech */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 Computer Company */
#define	PCI_VENDOR_VTECH	0x105e		/* Vtech Computers */
#define	PCI_VENDOR_INFOTRONIC	0x105f		/* Infotronic America */
#define	PCI_VENDOR_UMC	0x1060		/* United Microelectronics */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. */
#define	PCI_VENDOR_MASPAR	0x1062		/* MasPar Computer */
#define	PCI_VENDOR_OCEANOA	0x1063		/* Ocean Office Automation */
#define	PCI_VENDOR_ALCATEL	0x1064		/* Alcatel CIT */
#define	PCI_VENDOR_TEXASMICRO	0x1065		/* Texas Microsystems */
#define	PCI_VENDOR_PICOPOWER	0x1066		/* Picopower Technology */
#define	PCI_VENDOR_MITSUBISHI	0x1067		/* Mitsubishi Electronics */
#define	PCI_VENDOR_DIVERSIFIED	0x1068		/* Diversified Technology */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_ATEN	0x106a		/* Aten Research */
#define	PCI_VENDOR_APPLE	0x106b		/* Apple Computer */
#define	PCI_VENDOR_HYUNDAI	0x106c		/* Hyundai Electronics America */
#define	PCI_VENDOR_SEQUENT	0x106d		/* Sequent */
#define	PCI_VENDOR_DFI	0x106e		/* DFI */
#define	PCI_VENDOR_CITYGATE	0x106f		/* City Gate Development */
#define	PCI_VENDOR_DAEWOO	0x1070		/* Daewoo Telecom */
#define	PCI_VENDOR_MITAC	0x1071		/* Mitac */
#define	PCI_VENDOR_GIT	0x1072		/* GIT */
#define	PCI_VENDOR_YAMAHA	0x1073		/* Yamaha */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen Microsystems */
#define	PCI_VENDOR_AIR	0x1075		/* Advanced Integration Research */
#define	PCI_VENDOR_CHAINTECH	0x1076		/* Chaintech Computer */
#define	PCI_VENDOR_QLOGIC	0x1077		/* Q Logic */
#define	PCI_VENDOR_CYRIX	0x1078		/* Cyrix */
#define	PCI_VENDOR_IBUS	0x1079		/* I-Bus */
#define	PCI_VENDOR_NETWORTH	0x107a		/* NetWorth */
#define	PCI_VENDOR_GATEWAY	0x107b		/* Gateway 2000 */
#define	PCI_VENDOR_GOLDSTART	0x107c		/* Goldstar */
#define	PCI_VENDOR_LEADTEK	0x107d		/* LeadTek Research */
#define	PCI_VENDOR_INTERPHASE	0x107e		/* Interphase */
#define	PCI_VENDOR_DATATECH	0x107f		/* Data Technology */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq Microsystems */
#define	PCI_VENDOR_SUPERMAC	0x1081		/* Supermac Technology */
#define	PCI_VENDOR_EFA	0x1082		/* EFA of America */
#define	PCI_VENDOR_FOREX	0x1083		/* Forex Computer */
#define	PCI_VENDOR_PARADOR	0x1084		/* Parador */
#define	PCI_VENDOR_TULIP	0x1085		/* Tulip Computers */
#define	PCI_VENDOR_JBOND	0x1086		/* J. Bond Computer Systems */
#define	PCI_VENDOR_CACHECOMP	0x1087		/* Cache Computer */
#define	PCI_VENDOR_MICROCOMP	0x1088		/* Microcomputer Systems */
#define	PCI_VENDOR_DG	0x1089		/* Data General */
#define	PCI_VENDOR_BIT3	0x108a		/* Bit3 Computer */
#define	PCI_VENDOR_ELONEX	0x108c		/* Elonex PLC c/o Oakleigh Systems */
#define	PCI_VENDOR_OLICOM	0x108d		/* Olicom */
#define	PCI_VENDOR_SUN	0x108e		/* Sun Microsystems */
#define	PCI_VENDOR_SYSTEMSOFT	0x108f		/* Systemsoft */
#define	PCI_VENDOR_ENCORE	0x1090		/* Encore Computer */
#define	PCI_VENDOR_INTERGRAPH	0x1091		/* Intergraph */
#define	PCI_VENDOR_DIAMOND	0x1092		/* Diamond Computer Systems */
#define	PCI_VENDOR_NATIONALINST	0x1093		/* National Instruments */
#define	PCI_VENDOR_FICOMP	0x1094		/* First Int'l Computers */
#define	PCI_VENDOR_CMDTECH	0x1095		/* CMD Technology */
#define	PCI_VENDOR_ALACRON	0x1096		/* Alacron */
#define	PCI_VENDOR_APPIAN	0x1097		/* Appian Technology */
#define	PCI_VENDOR_QUANTUMDESIGNS	0x1098		/* Quantum Designs */
#define	PCI_VENDOR_SAMSUNGELEC	0x1099		/* Samsung Electronics */
#define	PCI_VENDOR_PACKARDBELL	0x109a		/* Packard Bell */
#define	PCI_VENDOR_GEMLIGHT	0x109b		/* Gemlight Computer */
#define	PCI_VENDOR_MEGACHIPS	0x109c		/* Megachips */
#define	PCI_VENDOR_ZIDA	0x109d		/* Zida Technologies */
#define	PCI_VENDOR_BROOKTREE	0x109e		/* Brooktree */
#define	PCI_VENDOR_TRIGEM	0x109f		/* Trigem Computer */
#define	PCI_VENDOR_MEIDENSHA	0x10a0		/* Meidensha */
#define	PCI_VENDOR_JUKO	0x10a1		/* Juko Electronics */
#define	PCI_VENDOR_QUANTUM	0x10a2		/* Quantum */
#define	PCI_VENDOR_EVEREX	0x10a3		/* Everex Systems */
#define	PCI_VENDOR_GLOBE	0x10a4		/* Globe Manufacturing Sales */
#define	PCI_VENDOR_RACAL	0x10a5		/* Racal Interlan */
#define	PCI_VENDOR_INFORMTECH	0x10a6		/* Informtech Industrial */
#define	PCI_VENDOR_BENCHMARQ	0x10a7		/* Benchmarq Microelectronics */
#define	PCI_VENDOR_SIERRA	0x10a8		/* Sierra Semiconductor */
#define	PCI_VENDOR_SGI	0x10a9		/* Silicon Graphics */
#define	PCI_VENDOR_ACC	0x10aa		/* ACC Microelectronics */
#define	PCI_VENDOR_DIGICOM	0x10ab		/* Digicom */
#define	PCI_VENDOR_HONEYWELL	0x10ac		/* Honeywell IASD */
#define	PCI_VENDOR_SYMPHONY	0x10ad		/* Symphony Labs */
#define	PCI_VENDOR_CORNERSTONE	0x10ae		/* Cornerstone Technology */
#define	PCI_VENDOR_MICROCOMPSON	0x10af		/* Micro Computer Sysytems (M) SON */
#define	PCI_VENDOR_CARDEXPER	0x10b0		/* CardExpert Technology */
#define	PCI_VENDOR_CABLETRON	0x10b1		/* Cabletron Systems */
#define	PCI_VENDOR_RAYETHON	0x10b2		/* Raytheon */
#define	PCI_VENDOR_DATABOOK	0x10b3		/* Databook */
#define	PCI_VENDOR_STB	0x10b4		/* STB Systems */
#define	PCI_VENDOR_PLX	0x10b5		/* PLX Technology */
#define	PCI_VENDOR_MADGE	0x10b6		/* Madge Networks */
#define	PCI_VENDOR_3COM	0x10b7		/* 3Com */
#define	PCI_VENDOR_SMC	0x10b8		/* Standard Microsystems */
#define	PCI_VENDOR_ALI	0x10b9		/* Acer Labs */
#define	PCI_VENDOR_MITSUBISHIELEC	0x10ba		/* Mitsubishi Electronics */
#define	PCI_VENDOR_DAPHA	0x10bb		/* Dapha Electronics */
#define	PCI_VENDOR_ALR	0x10bc		/* Advanced Logic Research */
#define	PCI_VENDOR_SURECOM	0x10bd		/* Surecom Technology */
#define	PCI_VENDOR_TSENGLABS	0x10be		/* Tseng Labs International */
#define	PCI_VENDOR_MOST	0x10bf		/* Most */
#define	PCI_VENDOR_BOCA	0x10c0		/* Boca Research */
#define	PCI_VENDOR_ICM	0x10c1		/* ICM */
#define	PCI_VENDOR_AUSPEX	0x10c2		/* Auspex Systems */
#define	PCI_VENDOR_SAMSUNGSEMI	0x10c3		/* Samsung Semiconductors */
#define	PCI_VENDOR_AWARD	0x10c4		/* Award Software Int'l */
#define	PCI_VENDOR_XEROX	0x10c5		/* Xerox */
#define	PCI_VENDOR_RAMBUS	0x10c6		/* Rambus */
#define	PCI_VENDOR_MEDIAVIS	0x10c7		/* Media Vision */
#define	PCI_VENDOR_NEOMAGIC	0x10c8		/* Neomagic */
#define	PCI_VENDOR_DATAEXPERT	0x10c9		/* Dataexpert */
#define	PCI_VENDOR_FUJITSU	0x10ca		/* Fujitsu */
#define	PCI_VENDOR_OMRON	0x10cb		/* Omron */
#define	PCI_VENDOR_MENTOR	0x10cc		/* Mentor ARC */
#define	PCI_VENDOR_ADVSYS	0x10cd		/* Advanced System Products */
#define	PCI_VENDOR_RADIUS	0x10ce		/* Radius */
#define	PCI_VENDOR_CITICORP	0x10cf		/* Citicorp TTI */
#define	PCI_VENDOR_FUJITSU2	0x10d0		/* Fujitsu Limited (2nd PCI Vendor ID) */
#define	PCI_VENDOR_FUTUREPLUS	0x10d1		/* Future+ Systems */
#define	PCI_VENDOR_MOLEX	0x10d2		/* Molex */
#define	PCI_VENDOR_JABIL	0x10d3		/* Jabil Circuit */
#define	PCI_VENDOR_HAULON	0x10d4		/* Hualon Microelectronics */
#define	PCI_VENDOR_AUTOLOGIC	0x10d5		/* Autologic */
#define	PCI_VENDOR_CETIA	0x10d6		/* Cetia */
#define	PCI_VENDOR_BCM	0x10d7		/* BCM Advanced */
#define	PCI_VENDOR_APL	0x10d8		/* Advanced Peripherals Labs */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_THOMASCONRAD	0x10da		/* Thomas-Conrad */
#define	PCI_VENDOR_ROHM	0x10db		/* Rohm Research */
#define	PCI_VENDOR_CERN	0x10dc		/* CERN/ECP/EDU */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* NVIDIA */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_TEKRAM	0x10e1		/* Tekram Technology (1st PCI Vendor ID) */
#define	PCI_VENDOR_APTIX	0x10e2		/* Aptix */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge Microsystems / Tundra Semiconductor */
#define	PCI_VENDOR_TANDEM	0x10e4		/* Tandem Computers */
#define	PCI_VENDOR_MICROINDUSTRIES	0x10e5		/* Micro Industries */
#define	PCI_VENDOR_GAINBERY	0x10e6		/* Gainbery Computer Products */
#define	PCI_VENDOR_VADEM	0x10e7		/* Vadem */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
#define	PCI_VENDOR_ALPSELECTIC	0x10e9		/* Alps Electric */
#define	PCI_VENDOR_INTEGRAPHICS	0x10ea		/* Integraphics Systems */
#define	PCI_VENDOR_ARTISTSGRAPHICS	0x10eb		/* Artists Graphics */
#define	PCI_VENDOR_REALTEK	0x10ec		/* Realtek Semiconductor */
#define	PCI_VENDOR_ASCIICORP	0x10ed		/* ASCII */
#define	PCI_VENDOR_XILINX	0x10ee		/* Xilinx */
#define	PCI_VENDOR_RACORE	0x10ef		/* Racore Computer Products */
#define	PCI_VENDOR_PERITEK	0x10f0		/* Peritek */
#define	PCI_VENDOR_TYAN	0x10f1		/* Tyan Computer */
#define	PCI_VENDOR_ACHME	0x10f2		/* Achme Computer */
#define	PCI_VENDOR_ALARIS	0x10f3		/* Alaris */
#define	PCI_VENDOR_SMOS	0x10f4		/* S-MOS Systems */
#define	PCI_VENDOR_NKK	0x10f5		/* NKK */
#define	PCI_VENDOR_CREATIVE	0x10f6		/* Creative Electronic Systems */
#define	PCI_VENDOR_MATSUSHITA	0x10f7		/* Matsushita */
#define	PCI_VENDOR_ALTOS	0x10f8		/* Altos India */
#define	PCI_VENDOR_PCDIRECT	0x10f9		/* PC Direct */
#define	PCI_VENDOR_TRUEVISIO	0x10fa		/* Truevision */
#define	PCI_VENDOR_THESYS	0x10fb		/* Thesys Ges. F. Mikroelektronik */
#define	PCI_VENDOR_IODATA	0x10fc		/* I-O Data Device */
#define	PCI_VENDOR_SOYO	0x10fd		/* Soyo Technology */
#define	PCI_VENDOR_FAST	0x10fe		/* Fast Electronic */
#define	PCI_VENDOR_NCUBE	0x10ff		/* NCube */
#define	PCI_VENDOR_JAZZ	0x1100		/* Jazz Multimedia */
#define	PCI_VENDOR_INITIO	0x1101		/* Initio */
#define	PCI_VENDOR_CREATIVELABS	0x1102		/* Creative Labs */
#define	PCI_VENDOR_TRIONES	0x1103		/* Triones Technologies */
#define	PCI_VENDOR_RASTEROPS	0x1104		/* RasterOps */
#define	PCI_VENDOR_SIGMA	0x1105		/* Sigma Designs */
#define	PCI_VENDOR_VIATECH	0x1106		/* VIA Technologies */
#define	PCI_VENDOR_STRATIS	0x1107		/* Stratus Computer */
#define	PCI_VENDOR_PROTEON	0x1108		/* Proteon */
#define	PCI_VENDOR_COGENT	0x1109		/* Cogent Data Technologies */
#define	PCI_VENDOR_SIEMENS	0x110a		/* Siemens AG / Siemens Nixdorf AG */
#define	PCI_VENDOR_XENON	0x110b		/* Xenon Microsystems */
#define	PCI_VENDOR_MINIMAX	0x110c		/* Mini-Max Technology */
#define	PCI_VENDOR_ZNYX	0x110d		/* Znyx Advanced Systems */
#define	PCI_VENDOR_CPUTECH	0x110e		/* CPU Technology */
#define	PCI_VENDOR_ROSS	0x110f		/* Ross Technology */
#define	PCI_VENDOR_POWERHOUSE	0x1110		/* Powerhouse Systems */
#define	PCI_VENDOR_SCO	0x1111		/* Santa Cruz Operation */
#define	PCI_VENDOR_RNS	0x1112		/* RNS */
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton Technology */
#define	PCI_VENDOR_ATMEL	0x1114		/* Atmel */
#define	PCI_VENDOR_DUPONT	0x1115		/* DuPont Pixel Systems */
#define	PCI_VENDOR_DATATRANSLATION	0x1116		/* Data Translation */
#define	PCI_VENDOR_DATACUBE	0x1117		/* Datacube */
#define	PCI_VENDOR_BERG	0x1118		/* Berg Electronics */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex Computer Systems */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_TELEDYNE	0x111b		/* Teledyne Electronic Systems */
#define	PCI_VENDOR_TRICORD	0x111c		/* Tricord Systems */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_ELDEC	0x111e		/* Eldec */
#define	PCI_VENDOR_PDI	0x111f		/* Prescision Digital Images */
#define	PCI_VENDOR_EMC	0x1120		/* Emc */
#define	PCI_VENDOR_ZILOG	0x1121		/* Zilog */
#define	PCI_VENDOR_MULTITECH	0x1122		/* Multi-tech Systems */
#define	PCI_VENDOR_LEUTRON	0x1124		/* Leutron Vision */
#define	PCI_VENDOR_EUROCORE	0x1125		/* Eurocore/Vigra */
#define	PCI_VENDOR_VIGRA	0x1126		/* Vigra */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_FIRMWORKS	0x1129		/* Firmworks */
#define	PCI_VENDOR_HERMES	0x112a		/* Hermes Electronics */
#define	PCI_VENDOR_LINOTYPE	0x112b		/* Linotype */
#define	PCI_VENDOR_RAVICAD	0x112d		/* Ravicad */
#define	PCI_VENDOR_INFOMEDIA	0x112e		/* Infomedia Microelectronics */
#define	PCI_VENDOR_IMAGINGTECH	0x112f		/* Imaging Technlogy */
#define	PCI_VENDOR_COMPUTERVISION	0x1130		/* Computervision */
#define	PCI_VENDOR_PHILIPS	0x1131		/* Philips */
#define	PCI_VENDOR_MITEL	0x1132		/* Mitel */
#define	PCI_VENDOR_EICON	0x1133		/* Eicon Technology */
#define	PCI_VENDOR_MCS	0x1134		/* Mercury Computer Systems */
#define	PCI_VENDOR_FUJIXEROX	0x1135		/* Fuji Xerox */
#define	PCI_VENDOR_MOMENTUM	0x1136		/* Momentum Data Systems */
#define	PCI_VENDOR_CISCO	0x1137		/* Cisco Systems */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_DYNPIC	0x1139		/* Dynamic Pictures */
#define	PCI_VENDOR_FWB	0x113a		/* FWB */
#define	PCI_VENDOR_CYCLONE	0x113c		/* Cyclone Micro */
#define	PCI_VENDOR_LEADINGEDGE	0x113d		/* Leading Edge */
#define	PCI_VENDOR_SANYO	0x113e		/* Sanyo Electric */
#define	PCI_VENDOR_EQUINOX	0x113f		/* Equinox Systems */
#define	PCI_VENDOR_INTERVOICE	0x1140		/* Intervoice */
#define	PCI_VENDOR_CREST	0x1141		/* Crest Microsystem */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_NETPOWER	0x1143		/* NetPower */
#define	PCI_VENDOR_CINMILACRON	0x1144		/* Cincinnati Milacron */
#define	PCI_VENDOR_WORKBIT	0x1145		/* Workbit */
#define	PCI_VENDOR_FORCE	0x1146		/* Force Computers */
#define	PCI_VENDOR_INTERFACE	0x1147		/* Interface */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_WINSYSTEM	0x1149		/* Win System */
#define	PCI_VENDOR_VMIC	0x114a		/* VMIC */
#define	PCI_VENDOR_CANOPUS	0x114b		/* Canopus */
#define	PCI_VENDOR_ANNABOOKS	0x114c		/* Annabooks */
#define	PCI_VENDOR_IC	0x114d		/* IC */
#define	PCI_VENDOR_NIKON	0x114e		/* Nikon Systems */
#define	PCI_VENDOR_DIGI	0x114f		/* Digi International */
#define	PCI_VENDOR_TMC	0x1150		/* Thinking Machines */
#define	PCI_VENDOR_JAE	0x1151		/* JAE Electronics */
#define	PCI_VENDOR_MEGATEK	0x1152		/* Megatek */
#define	PCI_VENDOR_LANDWIN	0x1153		/* Land Win Electronic */
#define	PCI_VENDOR_MELCO	0x1154		/* Melco */
#define	PCI_VENDOR_PINETECH	0x1155		/* Pine Technology */
#define	PCI_VENDOR_PERISCOPE	0x1156		/* Periscope Engineering */
#define	PCI_VENDOR_AVSYS	0x1157		/* Avsys */
#define	PCI_VENDOR_VOARX	0x1158		/* Voarx R & D */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_HARLEQUIN	0x115a		/* Harlequin */
#define	PCI_VENDOR_PARALLAX	0x115b		/* Parallax Graphics */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_PEERPROTO	0x115e		/* Peer Protocols */
#define	PCI_VENDOR_MAXTOR	0x115f		/* Maxtor */
#define	PCI_VENDOR_MEGASOFT	0x1160		/* Megasoft */
#define	PCI_VENDOR_PFU	0x1161		/* PFU Limited */
#define	PCI_VENDOR_OALAB	0x1162		/* OA Laboratory */
#define	PCI_VENDOR_RENDITION	0x1163		/* Rendition */
#define	PCI_VENDOR_APT	0x1164		/* Advanced Peripherals Technologies */
#define	PCI_VENDOR_IMAGRAPH	0x1165		/* Imagraph */
#define	PCI_VENDOR_SERVERWORKS	0x1166		/* ServerWorks */
#define	PCI_VENDOR_MUTOH	0x1167		/* Mutoh Industries */
#define	PCI_VENDOR_THINE	0x1168		/* Thine Electronics */
#define	PCI_VENDOR_CDAC	0x1169		/* Centre for Dev. of Advanced Computing */
#define	PCI_VENDOR_POLARIS	0x116a		/* Polaris Communications */
#define	PCI_VENDOR_CONNECTWARE	0x116b		/* Connectware */
#define	PCI_VENDOR_WSTECH	0x116f		/* Workstation Technology */
#define	PCI_VENDOR_INVENTEC	0x1170		/* Inventec */
#define	PCI_VENDOR_LOUGHSOUND	0x1171		/* Loughborough Sound Images */
#define	PCI_VENDOR_ALTERA	0x1172		/* Altera */
#define	PCI_VENDOR_ADOBE	0x1173		/* Adobe Systems */
#define	PCI_VENDOR_BRIDGEPORT	0x1174		/* Bridgeport Machines */
#define	PCI_VENDOR_MIRTRON	0x1175		/* Mitron Computer */
#define	PCI_VENDOR_SBE	0x1176		/* SBE */
#define	PCI_VENDOR_SILICONENG	0x1177		/* Silicon Engineering */
#define	PCI_VENDOR_ALFA	0x1178		/* Alfa */
#define	PCI_VENDOR_TOSHIBA2	0x1179		/* Toshiba */
#define	PCI_VENDOR_ATREND	0x117a		/* A-Trend Technology */
#define	PCI_VENDOR_ATTO	0x117c		/* Atto Technology */
#define	PCI_VENDOR_TR	0x117e		/* T/R Systems */
#define	PCI_VENDOR_RICOH	0x1180		/* Ricoh */
#define	PCI_VENDOR_TELEMATICS	0x1181		/* Telematics International */
#define	PCI_VENDOR_FUJIKURA	0x1183		/* Fujikura */
#define	PCI_VENDOR_FORKS	0x1184		/* Forks */
#define	PCI_VENDOR_DATAWORLD	0x1185		/* Dataworld */
#define	PCI_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	PCI_VENDOR_ATL	0x1187		/* Advanced Techonoloy Labratories */
#define	PCI_VENDOR_SHIMA	0x1188		/* Shima Seiki Manufacturing */
#define	PCI_VENDOR_MATSUSHITA2	0x1189		/* Matsushita Electronics (2nd PCI Vendor ID) */
#define	PCI_VENDOR_HILEVEL	0x118a		/* HiLevel Technology */
#define	PCI_VENDOR_COROLLARY	0x118c		/* Corrollary */
#define	PCI_VENDOR_BITFLOW	0x118d		/* BitFlow */
#define	PCI_VENDOR_HERMSTEDT	0x118e		/* Hermstedt */
#define	PCI_VENDOR_ACARD	0x1191		/* Acard */
#define	PCI_VENDOR_DENSAN	0x1192		/* Densan */
#define	PCI_VENDOR_ZEINET	0x1193		/* Zeinet */
#define	PCI_VENDOR_TOUCAN	0x1194		/* Toucan Technology */
#define	PCI_VENDOR_RATOC	0x1195		/* Ratoc Systems */
#define	PCI_VENDOR_HYTEC	0x1196		/* Hytec Electronic */
#define	PCI_VENDOR_GAGE	0x1197		/* Gage Applied Sciences */
#define	PCI_VENDOR_LAMBDA	0x1198		/* Lambda Systems */
#define	PCI_VENDOR_DCA	0x1199		/* Digital Communications Associates */
#define	PCI_VENDOR_MINDSHARE	0x119a		/* Mind Share */
#define	PCI_VENDOR_OMEGA	0x119b		/* Omega Micro */
#define	PCI_VENDOR_ITI	0x119c		/* Information Technology Institute */
#define	PCI_VENDOR_BUG	0x119d		/* Bug Sapporo */
#define	PCI_VENDOR_FUJITSU3	0x119e		/* Fujitsu (3th PCI Vendor ID) */
#define	PCI_VENDOR_BULL	0x119f		/* Bull Hn Information Systems */
#define	PCI_VENDOR_CONVEX	0x11a0		/* Convex Computer */
#define	PCI_VENDOR_HAMAMATSU	0x11a1		/* Hamamatsu Photonics */
#define	PCI_VENDOR_SIERRA2	0x11a2		/* Sierra Research & Technology (2nd PCI Vendor ID) */
#define	PCI_VENDOR_BARCO	0x11a4		/* Barco */
#define	PCI_VENDOR_MICROUNITY	0x11a5		/* MicroUnity Systems Engineering */
#define	PCI_VENDOR_PUREDATA	0x11a6		/* Pure Data */
#define	PCI_VENDOR_POWERCC	0x11a7		/* Power Computing */
#define	PCI_VENDOR_INNOSYS	0x11a9		/* InnoSys */
#define	PCI_VENDOR_ACTEL	0x11aa		/* Actel */
#define	PCI_VENDOR_GALILEO	0x11ab		/* Galileo (Marvell) Technology */
#define	PCI_VENDOR_CANNON	0x11ac		/* Cannon IS */
#define	PCI_VENDOR_LITEON	0x11ad		/* Lite-On Communications */
#define	PCI_VENDOR_SCITEX	0x11ae		/* Scitex */
#define	PCI_VENDOR_AVID	0x11af		/* Avid Technology */
#define	PCI_VENDOR_V3	0x11b0		/* V3 Semiconductor */
#define	PCI_VENDOR_APRICOT	0x11b1		/* Apricot Computer */
#define	PCI_VENDOR_KODAK	0x11b2		/* Eastman Kodak */
#define	PCI_VENDOR_BARR	0x11b3		/* Barr Systems */
#define	PCI_VENDOR_LEITECH	0x11b4		/* Leitch Technology */
#define	PCI_VENDOR_RADSTONE	0x11b5		/* Radstone Technology */
#define	PCI_VENDOR_UNITEDVIDEO	0x11b6		/* United Video */
#define	PCI_VENDOR_MOT2	0x11b7		/* Motorola (2nd PCI Vendor ID) */
#define	PCI_VENDOR_XPOINT	0x11b8		/* Xpoint Technologies */
#define	PCI_VENDOR_PATHLIGHT	0x11b9		/* Pathlight Technology */
#define	PCI_VENDOR_VIDEOTRON	0x11ba		/* VideoTron */
#define	PCI_VENDOR_PYRAMID	0x11bb		/* Pyramid Technologies */
#define	PCI_VENDOR_NETPERIPH	0x11bc		/* Network Peripherals */
#define	PCI_VENDOR_PINNACLE	0x11bd		/* Pinnacle Systems */
#define	PCI_VENDOR_IMI	0x11be		/* International Microcircuts */
#define	PCI_VENDOR_LUCENT	0x11c1		/* Lucent Technologies */
#define	PCI_VENDOR_NEC2	0x11c3		/* NEC (2nd PCI Vendor ID) */
#define	PCI_VENDOR_DOCTECH	0x11c4		/* Document Technologies */
#define	PCI_VENDOR_SHIVA	0x11c5		/* Shiva */
#define	PCI_VENDOR_DCMDATA	0x11c7		/* DCM Data Systems */
#define	PCI_VENDOR_DOLPHIN	0x11c8		/* Dolphin Interconnect Solutions */
#define	PCI_VENDOR_MAGMA	0x11c9		/* Mesa Ridge Technologies (MAGMA) */
#define	PCI_VENDOR_LSISYS	0x11ca		/* LSI Systems */
#define	PCI_VENDOR_SPECIALIX	0x11cb		/* Specialix Research */
#define	PCI_VENDOR_MKC	0x11cc		/* Michels & Kleberhoff Computer */
#define	PCI_VENDOR_HAL	0x11cd		/* HAL Computer Systems */
#define	PCI_VENDOR_AURAVISION	0x11d1		/* Auravision */
#define	PCI_VENDOR_ANALOG	0x11d4		/* Analog Devices */
#define	PCI_VENDOR_SEGA	0x11db		/* SEGA Enterprises */
#define	PCI_VENDOR_ZORAN	0x11de		/* Zoran */
#define	PCI_VENDOR_QUICKLOGIC	0x11e3		/* QuickLogic */
#define	PCI_VENDOR_COMPEX	0x11f6		/* Compex */
#define	PCI_VENDOR_PMCSIERRA	0x11f8		/* PMC-Sierra */
#define	PCI_VENDOR_COMTROL	0x11fe		/* Comtrol */
#define	PCI_VENDOR_CYCLADES	0x120e		/* Cyclades */
#define	PCI_VENDOR_ESSENTIAL	0x120f		/* Essential Communications */
#define	PCI_VENDOR_O2MICRO	0x1217		/* O2 Micro */
#define	PCI_VENDOR_3DFX	0x121a		/* 3Dfx Interactive */
#define	PCI_VENDOR_ARIEL	0x1220		/* Ariel */
#define	PCI_VENDOR_HEURICON	0x1223		/* Heurikon/Computer Products */
#define	PCI_VENDOR_AZTECH	0x122d		/* Aztech */
#define	PCI_VENDOR_3DO	0x1239		/* The 3D0 Company */
#define	PCI_VENDOR_CCUBE	0x123f		/* C-Cube Microsystems */
#define	PCI_VENDOR_JNI	0x1242		/* JNI */
#define	PCI_VENDOR_AVM	0x1244		/* AVM */
#define	PCI_VENDOR_SAMSUNGELEC2	0x1249		/* Samsung Electronics (2nd vendor ID) */
#define	PCI_VENDOR_STALLION	0x124d		/* Stallion Technologies */
#define	PCI_VENDOR_LINEARSYS	0x1254		/* Linear Systems */
#define	PCI_VENDOR_COREGA	0x1259		/* Corega */
#define	PCI_VENDOR_ASIX	0x125b		/* ASIX Electronics */
#define	PCI_VENDOR_AURORA	0x125c		/* Aurora Technologies */
#define	PCI_VENDOR_ESSTECH	0x125d		/* ESS Technology */
#define	PCI_VENDOR_INTERSIL	0x1260		/* Intersil */
#define	PCI_VENDOR_NORTEL	0x126c		/* Nortel Networks (Northern Telecom) */
#define	PCI_VENDOR_SILMOTION	0x126f		/* Silicon Motion */
#define	PCI_VENDOR_ENSONIQ	0x1274		/* Ensoniq */
#define	PCI_VENDOR_NETAPP	0x1275		/* Network Appliance */
#define	PCI_VENDOR_TRANSMETA	0x1279		/* Transmeta */
#define	PCI_VENDOR_ROCKWELL	0x127a		/* Rockwell Semiconductor Systems */
#define	PCI_VENDOR_DAVICOM	0x1282		/* Davicom Semiconductor */
#define	PCI_VENDOR_ITE	0x1283		/* Integrated Technology Express */
#define	PCI_VENDOR_ESSTECH2	0x1285		/* ESS Technology */
#define	PCI_VENDOR_TRITECH	0x1292		/* TriTech Microelectronics */
#define	PCI_VENDOR_KOFAX	0x1296		/* Kofax Image Products */
#define	PCI_VENDOR_ALTEON	0x12ae		/* Alteon */
#define	PCI_VENDOR_RISCOM	0x12aa		/* RISCom */
#define	PCI_VENDOR_USR	0x12b9		/* US Robotics (3Com) */
#define	PCI_VENDOR_USR2	0x16ec		/* US Robotics */
#define	PCI_VENDOR_PICTUREEL	0x12c5		/* Picture Elements */
#define	PCI_VENDOR_NVIDIA_SGS	0x12d2		/* Nvidia & SGS-Thomson Microelectronics */
#define	PCI_VENDOR_RAINBOW	0x12de		/* Rainbow Technologies */
#define	PCI_VENDOR_AUREAL	0x12eb		/* Aureal Semiconductor */
#define	PCI_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	PCI_VENDOR_PACKETENGINES	0x1318		/* Packet Engines */
#define	PCI_VENDOR_FORTEMEDIA	0x1319		/* Forte Media */
#define	PCI_VENDOR_SIIG	0x131f		/* Siig */
#define	PCI_VENDOR_DOMEX	0x134a		/* Domex */
#define	PCI_VENDOR_LMC	0x1376		/* LAN Media */
#define	PCI_VENDOR_NETGEAR	0x1385		/* Netgear */
#define	PCI_VENDOR_MOXA	0x1393		/* Moxa Technologies */
#define	PCI_VENDOR_LEVELONE	0x1394		/* Level One */
#define	PCI_VENDOR_COLOGNECHIP	0x1397		/* Cologne Chip Designs */
#define	PCI_VENDOR_HIFN	0x13a3		/* Hifn */
#define	PCI_VENDOR_EXAR	0x13a8		/* EXAR */
#define	PCI_VENDOR_3WARE	0x13c1		/* 3ware */
#define	PCI_VENDOR_ABOCOM	0x13d1		/* AboCom Systems */
#define	PCI_VENDOR_NETBOOST	0x13dc		/* NetBoost */
#define	PCI_VENDOR_SUNDANCETI	0x13f0		/* Sundance Technology */
#define	PCI_VENDOR_CMEDIA	0x13f6		/* C-Media Electronics */
#define	PCI_VENDOR_LAVA	0x1407		/* Lava Semiconductor Manufacturing */
#define	PCI_VENDOR_ETIMEDIA	0x1409		/* eTIMedia Technology */
#define	PCI_VENDOR_ICENSEMBLE	0x1412		/* IC Ensemble / VIA Technologies */
#define	PCI_VENDOR_MICROSOFT	0x1414		/* Microsoft */
#define	PCI_VENDOR_OXFORDSEMI	0x1415		/* Oxford Semiconductor */
#define	PCI_VENDOR_TAMARACK	0x143d		/* Tamarack Microelectronics */
#define	PCI_VENDOR_SAMSUNGELEC3	0x144d		/* Samsung Electronics (3rd vendor ID) */
#define	PCI_VENDOR_ASKEY	0x144f		/* Askey Computer */
#define	PCI_VENDOR_AVERMEDIA	0x1461		/* Avermedia Technologies */
#define	PCI_VENDOR_AIRONET	0x14b9		/* Aironet Wireless Communications */
#define	PCI_VENDOR_COMPAL	0x14c0		/* COMPAL Electronics */
#define	PCI_VENDOR_MYRICOM	0x14c1		/* Myricom */
#define	PCI_VENDOR_TITAN	0x14d2		/* Titan Electronics */
#define	PCI_VENDOR_AVLAB	0x14db		/* Avlab Technology */
#define	PCI_VENDOR_INVERTEX	0x14e1		/* Invertex */
#define	PCI_VENDOR_BROADCOM	0x14e4		/* Broadcom */
#define	PCI_VENDOR_PLANEX	0x14ea		/* Planex Communications */
#define	PCI_VENDOR_CONEXANT	0x14f1		/* Conexant Systems */
#define	PCI_VENDOR_DELTA	0x1500		/* Delta Electronics */
#define	PCI_VENDOR_ENE	0x1524		/* ENE Technology */
#define	PCI_VENDOR_TERRATEC	0x153b		/* TerraTec Electronic */
#define	PCI_VENDOR_SOLIDUM	0x1588		/* Solidum Systems */
#define	PCI_VENDOR_FARADAY	0x159b		/* Faraday Technology */
#define	PCI_VENDOR_GEOCAST	0x15a1		/* Geocast Network Systems */
#define	PCI_VENDOR_BLUESTEEL	0x15ab		/* Bluesteel Networks */
#define	PCI_VENDOR_VMWARE	0x15ad		/* VMware */
#define	PCI_VENDOR_AGILENT	0x15bc		/* Agilent Technologies */
#define	PCI_VENDOR_EUMITCOM	0x1638		/* Eumitcom */
#define	PCI_VENDOR_NETSEC	0x1660		/* NetSec */
#define	PCI_VENDOR_SIBYTE	0x166d		/* Broadcom (SiByte) */
#define	PCI_VENDOR_MYSON	0x1516		/* Myson-Century Technology */
#define	PCI_VENDOR_NDC	0x15e8		/* National Datacomm */
#define	PCI_VENDOR_ACTIONTEC	0x1668		/* Action Tec Electronics */
#define	PCI_VENDOR_ATHEROS	0x168c		/* Atheros Communications */
#define	PCI_VENDOR_GLOBALSUN	0x16ab		/* Global Sun Tech */
#define	PCI_VENDOR_SAFENET	0x16ae		/* SafeNet */
#define	PCI_VENDOR_LINKSYS	0x1737		/* Linksys */
#define	PCI_VENDOR_ALTIMA	0x173b		/* Altima */
#define	PCI_VENDOR_ANTARES	0x1754		/* Antares Microsystems */
#define	PCI_VENDOR_CAVIUM	0x177d		/* Cavium */
#define	PCI_VENDOR_FZJZEL	0x1796		/* FZ Juelich / ZEL */
#define	PCI_VENDOR_BELKIN	0x1799		/* Belkin */
#define	PCI_VENDOR_HAWKING	0x17b3		/* Hawking Technology */
#define	PCI_VENDOR_SANDBURST	0x17ba		/* Sandburst */
#define	PCI_VENDOR_I4	0x17cf		/* I4 */
#define	PCI_VENDOR_S2IO	0x17d5		/* S2io Technologies */
#define	PCI_VENDOR_LINKSYS2	0x17fe		/* Linksys */
#define	PCI_VENDOR_RALINK	0x1814		/* Ralink Technologies */
#define	PCI_VENDOR_SYMPHONY2	0x1c1c		/* Symphony Labs (2nd PCI Vendor ID) */
#define	PCI_VENDOR_TEKRAM2	0x1de1		/* Tekram Technology (2nd PCI Vendor ID) */
#define	PCI_VENDOR_HINT	0x3388		/* HiNT */
#define	PCI_VENDOR_3DLABS	0x3d3d		/* 3D Labs */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ADDTRON	0x4033		/* Addtron Technology */
#define	PCI_VENDOR_ICOMPRESSION	0x4444		/* Conexant (iCompression) */
#define	PCI_VENDOR_INDCOMPSRC	0x494f		/* Industrial Computer Source */
#define	PCI_VENDOR_NETVIN	0x4a14		/* NetVin */
#define	PCI_VENDOR_BUSLOGIC2	0x4b10		/* Buslogic (2nd PCI Vendor ID) */
#define	PCI_VENDOR_MEDIAQ	0x4d51		/* MediaQ */
#define	PCI_VENDOR_GUILLEMOT	0x5046		/* Guillemot */
#define	PCI_VENDOR_TURTLE_BEACH	0x5053		/* Turtle Beach */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_NETPOWER2	0x5700		/* NetPower (2nd PCI Vendor ID) */
#define	PCI_VENDOR_C4T	0x6374		/* c't Magazin */
#define	PCI_VENDOR_KURUSUGAWA	0x6809		/* Kurusugawa Electronics */
#define	PCI_VENDOR_PCHDTV	0x7063		/* pcHDTV */
#define	PCI_VENDOR_QUANCM	0x8008		/* Quancm Electronic GmbH */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_TRIGEM2	0x8800		/* Trigem Computer (2nd PCI Vendor ID) */
#define	PCI_VENDOR_PROLAN	0x8c4a		/* ProLAN */
#define	PCI_VENDOR_COMPUTONE	0x8e0e		/* Computone */
#define	PCI_VENDOR_KTI	0x8e2e		/* KTI */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ADP2	0x9005		/* Adaptec (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_NETMOS	0x9710		/* Netmos */
#define	PCI_VENDOR_CHRYSALIS	0xcafe		/* Chrysalis-ITS */
#define	PCI_VENDOR_MIDDLE_DIGITAL	0xdeaf		/* Middle Digital */
#define	PCI_VENDOR_ARC	0xedd8		/* ARC Logic */
#define	PCI_VENDOR_INVALID	0xffff		/* INVALID VENDOR ID */

/*
 * List of known products. Grouped by vendor.
 */

/* 3COM Products */
#define	PCI_PRODUCT_3COM_3C985	0x0001		/* 3c985 Gigabit Ethernet */
#define	PCI_PRODUCT_3COM_3C996	0x0003		/* 3c996 10/100/1000 Ethernet */
#define	PCI_PRODUCT_3COM_3C556MODEM	0x1007		/* 3c556 V.90 Mini-PCI Modem */
#define	PCI_PRODUCT_3COM_3C940	0x1700		/* 3c940 Gigabit Ethernet */
#define	PCI_PRODUCT_3COM_3C359	0x3590		/* 3c359 TokenLink XL */
#define	PCI_PRODUCT_3COM_3C450TX	0x4500		/* 3c450-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575TX	0x5057		/* 3c575-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575BTX	0x5157		/* 3CCFE575BT 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575CTX	0x5257		/* 3CCFE575CT 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 Ethernet */
#define	PCI_PRODUCT_3COM_3C595TX	0x5950		/* 3c595-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595T4	0x5951		/* 3c595-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595MII	0x5952		/* 3c595-MII 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C555	0x5055		/* 3c555 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C154G72	0x6001		/* 3CRWE154G72 Wireless LAN adapter */
#define	PCI_PRODUCT_3COM_3C556	0x6055		/* 3c556 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C556B	0x6056		/* 3c556B 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C656_E	0x6560		/* 3CCFEM656 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656_M	0x6561		/* 3CCFEM656 56k Modem */
#define	PCI_PRODUCT_3COM_3C656B_E	0x6562		/* 3CCFEM656B 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656B_M	0x6563		/* 3CCFEM656B 56k Modem */
#define	PCI_PRODUCT_3COM_3C656C_E	0x6564		/* 3CXFEM656C 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656C_M	0x6565		/* 3CXFEM656C 56k Modem */
#define	PCI_PRODUCT_3COM_3CSOHO100TX	0x7646		/* 3cSOHO100-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3CRWE777A	0x7770		/* 3crwe777a AirConnect */
#define	PCI_PRODUCT_3COM_3C804	0x7980		/* 3c804 FDDILink SAS */
#define	PCI_PRODUCT_3COM_3C900TPO	0x9000		/* 3c900-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900COMBO	0x9001		/* 3c900-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C905TX	0x9050		/* 3c905-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905T4	0x9051		/* 3c905-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPO	0x9004		/* 3c900B-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BCOMBO	0x9005		/* 3c900B-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPC	0x9006		/* 3c900B-TPC Ethernet */
#define	PCI_PRODUCT_3COM_3C905BTX	0x9055		/* 3c905B-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BT4	0x9056		/* 3c905B-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BCOMBO	0x9058		/* 3c905B-COMBO 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BFX	0x905a		/* 3c905B-FX 100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905CTX	0x9200		/* 3c905C-TX 10/100 Ethernet with mngmt */
#define	PCI_PRODUCT_3COM_3C905CXTX	0x9201		/* 3c905CX-TX 10/100 Ethernet with mngmt */
#define	PCI_PRODUCT_3COM_3C920BEMBW	0x9202		/* 3c920B-EMB-WNM Integrated Fast Ethernet */
#define	PCI_PRODUCT_3COM_3C910SOHOB	0x9300		/* 3c910 OfficeConnect 10/100B Ethernet */
#define	PCI_PRODUCT_3COM_3C980SRV	0x9800		/* 3c980 Server Adapter 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C980CTXM	0x9805		/* 3c980C-TXM 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3CR990	0x9900		/* 3c990-TX 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3CR990TX95	0x9902		/* 3CR990-TX-95 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3CR990TX97	0x9903		/* 3CR990-TX-97 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3C990B	0x9904		/* 3c990B 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3CR990FX	0x9905		/* 3CR990-FX 100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3CR990SVR95	0x9908		/* 3CR990-SVR-95 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3CR990SVR97	0x9909		/* 3CR990-SVR-97 10/100 Ethernet with 3XP */
#define	PCI_PRODUCT_3COM_3C990BSVR	0x990a		/* 3c990BSVR 10/100 Ethernet with 3XP */

/* 3Dfx Interactive products */
#define	PCI_PRODUCT_3DFX_VOODOO	0x0001		/* Voodoo */
#define	PCI_PRODUCT_3DFX_VOODOO2	0x0002		/* Voodoo2 */
#define	PCI_PRODUCT_3DFX_BANSHEE	0x0003		/* Banshee */
#define	PCI_PRODUCT_3DFX_VOODOO3	0x0005		/* Voodoo3 */
#define	PCI_PRODUCT_3DFX_VOODOO5	0x0009		/* Voodoo 4/5 */

/* 3D Labs products */
#define	PCI_PRODUCT_3DLABS_300SX	0x0001		/* GLINT 300SX */
#define	PCI_PRODUCT_3DLABS_500TX	0x0002		/* GLINT 500TX */
#define	PCI_PRODUCT_3DLABS_DELTA	0x0003		/* GLINT DELTA */
#define	PCI_PRODUCT_3DLABS_PERMEDIA	0x0004		/* GLINT Permedia */
#define	PCI_PRODUCT_3DLABS_500MX	0x0006		/* GLINT 500MX */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2	0x0007		/* GLINT Permedia 2 */
#define	PCI_PRODUCT_3DLABS_GAMMA	0x0008		/* GLINT GAMMA */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2V	0x0009		/* GLINT Permedia 2V */
#define	PCI_PRODUCT_3DLABS_PERMEDIA3	0x000a		/* GLINT Permedia 3 */

/* 3ware products */
#define	PCI_PRODUCT_3WARE_ESCALADE	0x1000		/* Escalade ATA RAID Controller */
#define	PCI_PRODUCT_3WARE_ESCALADE_ASIC	0x1001		/* Escalade ATA RAID 7000/8000 series Controller */
#define	PCI_PRODUCT_3WARE_9000	0x1002		/* 9000-series RAID */
#define	PCI_PRODUCT_3WARE_9550	0x1003		/* 9550-series RAID */

/* AboCom products */
#define	PCI_PRODUCT_ABOCOM_FE2500	0xab02		/* FE2500 10/100 Ethernet */
#define	PCI_PRODUCT_ABOCOM_PCM200	0xab03		/* PCM200 10/100 Ethernet */
#define	PCI_PRODUCT_ABOCOM_FE2000VX	0xab06		/* FE2000VX 10/100 Ethernet (OEM) */
#define	PCI_PRODUCT_ABOCOM_FE2500MX	0xab08		/* FE2500MX 10/100 Ethernet */

/* ACC Products */
#define	PCI_PRODUCT_ACC_2188	0x0000		/* ACCM 2188 VL-PCI Bridge */
#define	PCI_PRODUCT_ACC_2051_HB	0x2051		/* 2051 PCI Single Chip Solution (host bridge) */
#define	PCI_PRODUCT_ACC_2051_ISA	0x5842		/* 2051 PCI Single Chip Solution (ISA bridge) */

/* Acard products */
#define	PCI_PRODUCT_ACARD_ATP850U	0x0005		/* ATP850U/UF UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP860	0x0006		/* ATP860 UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP860A	0x0007		/* ATP860-A UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP865	0x0008		/* ATP865 UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP865A	0x0009		/* ATP865-A UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_AEC6710	0x8002		/* AEC6710 SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712UW	0x8010		/* AEC6712UW SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712U	0x8020		/* AEC6712U SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712S	0x8030		/* AEC6712S SCSI */
#define	PCI_PRODUCT_ACARD_AEC6710D	0x8040		/* AEC6710D SCSI */
#define	PCI_PRODUCT_ACARD_AEC6715UW	0x8050		/* AEC6715UW SCSI */

/* Accton products */
#define	PCI_PRODUCT_ACCTON_MPX5030	0x1211		/* MPX 5030/5038 Ethernet */
#define	PCI_PRODUCT_ACCTON_EN2242	0x1216		/* EN2242 10/100 Ethernet */

/* Acer products */
#define	PCI_PRODUCT_ACER_M1435	0x1435		/* M1435 VL-PCI Bridge */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 VL-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1461	0x1461		/* M1461 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1531	0x1531		/* M1531 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1541	0x1541		/* M1541 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1543	0x1533		/* M1543 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1563	0x1563		/* M1563 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M3309	0x3309		/* M3309 MPEG Decoder */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */
#define	PCI_PRODUCT_ALI_M5257	0x5257		/* M5257 PCI Software Modem */
#define	PCI_PRODUCT_ALI_M5229	0x5229		/* M5229 UDMA IDE Controller */
#define	PCI_PRODUCT_ALI_M5237	0x5237		/* M5237 USB 1.1 Host Controller */
#define	PCI_PRODUCT_ALI_M5239	0x5239		/* M5239 USB 2.0 Host Controller */
#define	PCI_PRODUCT_ALI_M5243	0x5243		/* M5243 PCI-AGP Bridge */
#define	PCI_PRODUCT_ALI_M5249	0x5249		/* M5249 Hypertransport to PCI bridge */
#define	PCI_PRODUCT_ALI_M5451	0x5451		/* M5451 AC-Link Controller Audio Device */
#define	PCI_PRODUCT_ALI_M5453	0x5453		/* M5453 AC-Link Controller Modem Device */
#define	PCI_PRODUCT_ALI_M5455	0x5455		/* M5455 AC-Link Controller Audio Device */
#define	PCI_PRODUCT_ALI_M7101	0x7101		/* M7101 Power Management Controller */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC7850	0x5078		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7855	0x5578		/* AIC-7855 */
#define	PCI_PRODUCT_ADP_AIC5900	0x5900		/* AIC-5900 ATM */
#define	PCI_PRODUCT_ADP_AIC5905	0x5905		/* AIC-5905 ATM */
#define	PCI_PRODUCT_ADP_AIC6915	0x6915		/* AIC-6915 10/100 Ethernet */
#define	PCI_PRODUCT_ADP_AIC7860	0x6078		/* AIC-7860 */
#define	PCI_PRODUCT_ADP_APA1480	0x6075		/* APA-1480 Ultra */
#define	PCI_PRODUCT_ADP_2940AU	0x6178		/* AHA-2940A Ultra */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_2940	0x7178		/* AHA-2940 */
#define	PCI_PRODUCT_ADP_3940	0x7278		/* AHA-3940 */
#define	PCI_PRODUCT_ADP_3985	0x7378		/* AHA-3985 */
#define	PCI_PRODUCT_ADP_2944	0x7478		/* AHA-2944 */
#define	PCI_PRODUCT_ADP_AIC7895	0x7895		/* AIC-7895 Ultra */
#define	PCI_PRODUCT_ADP_AIC7880	0x8078		/* AIC-7880 Ultra */
#define	PCI_PRODUCT_ADP_2940U	0x8178		/* AHA-2940 Ultra */
#define	PCI_PRODUCT_ADP_3940U	0x8278		/* AHA-3940 Ultra */
#define	PCI_PRODUCT_ADP_389XU	0x8378		/* AHA-389X Ultra */
#define	PCI_PRODUCT_ADP_2944U	0x8478		/* AHA-2944 Ultra */
#define	PCI_PRODUCT_ADP_2940UP	0x8778		/* AHA-2940 Ultra Pro */

#define	PCI_PRODUCT_ADP2_2940U2	0x0010		/* AHA-2940U2 U2 */
#define	PCI_PRODUCT_ADP2_2930U2	0x0011		/* AHA-2930U2 U2 */
#define	PCI_PRODUCT_ADP2_AIC7890	0x001f		/* AIC-7890/1 U2 */
#define	PCI_PRODUCT_ADP2_3950U2B	0x0050		/* AHA-3950U2B U2 */
#define	PCI_PRODUCT_ADP2_3950U2D	0x0051		/* AHA-3950U2D U2 */
#define	PCI_PRODUCT_ADP2_AIC7896	0x005f		/* AIC-7896/7 U2 */
#define	PCI_PRODUCT_ADP2_AIC7892A	0x0080		/* AIC-7892A U160 */
#define	PCI_PRODUCT_ADP2_AIC7892B	0x0081		/* AIC-7892B U160 */
#define	PCI_PRODUCT_ADP2_AIC7892D	0x0083		/* AIC-7892D U160 */
#define	PCI_PRODUCT_ADP2_AIC7892P	0x008f		/* AIC-7892P U160 */
#define	PCI_PRODUCT_ADP2_AIC7899A	0x00c0		/* AIC-7899A U160 */
#define	PCI_PRODUCT_ADP2_AIC7899B	0x00c1		/* AIC-7899B U160 */
#define	PCI_PRODUCT_ADP2_AIC7899D	0x00c3		/* AIC-7899D U160 */
#define	PCI_PRODUCT_ADP2_AIC7899F	0x00c5		/* AIC-7899F RAID */
#define	PCI_PRODUCT_ADP2_AIC7899P	0x00cf		/* AIC-7899P U160 */
#define	PCI_PRODUCT_ADP2_AAC2622	0x0282		/* AAC-2622 */
#define	PCI_PRODUCT_ADP2_ASR2200S	0x0285		/* ASR-2200S */
#define	PCI_PRODUCT_ADP2_ASR2120S	0x0286		/* ASR-2120S */
#define	PCI_PRODUCT_ADP2_ASR2200S_SUB2M	0x0287		/* ASR-2200S */
#define	PCI_PRODUCT_ADP2_ASR2410SA	0x0290		/* ASR-2410SA */
#define	PCI_PRODUCT_ADP2_AAR2810SA	0x0292		/* AAR-2810SA */
#define	PCI_PRODUCT_ADP2_AAC364	0x0364		/* AAC-364 */
#define	PCI_PRODUCT_ADP2_ASR5400S	0x0365		/* ASR-5400S */
#define	PCI_PRODUCT_ADP2_PERC_2QC	0x1364		/* Dell PERC 2/QC */
/* XXX guess */
#define	PCI_PRODUCT_ADP2_PERC_3QC	0x1365		/* Dell PERC 3/QC */

/* Addtron Products */
#define	PCI_PRODUCT_ADDTRON_8139	0x1360		/* 8139 Ethernet */
#define	PCI_PRODUCT_ADDTRON_RHINEII	0x1320		/* Rhine II 10/100 Ethernet */

/* ADMtek products */
#define	PCI_PRODUCT_ADMTEK_AL981	0x0981		/* AL981 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_AN985	0x0985		/* AN985 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_AN985_2	0x1985		/* AN985 10/100 Ethernet (OEM) */
#define	PCI_PRODUCT_ADMTEK_ADM8211	0x8201		/* ADM8211 11Mbps 802.11b WLAN */

/* Advanced System Products */
#define	PCI_PRODUCT_ADVSYS_1200A	0x1100
#define	PCI_PRODUCT_ADVSYS_1200B	0x1200
#define	PCI_PRODUCT_ADVSYS_ULTRA	0x1300		/* ABP-930/40UA */
#define	PCI_PRODUCT_ADVSYS_WIDE	0x2300		/* ABP-940UW */
#define	PCI_PRODUCT_ADVSYS_U2W	0x2500		/* ASB-3940U2W */
#define	PCI_PRODUCT_ADVSYS_U3W	0x2700		/* ASB-3940U3W */

/* Agilent Technologies Products */
#define	PCI_PRODUCT_AGILENT_TACHYON_DX2	0x0100		/* Tachyon DX2 FC controller */

/* Aironet Wireless Communicasions products */
#define	PCI_PRODUCT_AIRONET_PC4xxx	0x0001		/* PC4500/PC4800 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PCI350	0x0350		/* PCI350 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_MPI350	0xa504		/* MPI350 Mini-PCI Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PC4500	0x4500		/* PC4500 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PC4800	0x4800		/* PC4800 Wireless LAN Adapter */

/* Alliance products */
#define	PCI_PRODUCT_ALLIANCE_AT24	0x6424		/* AT24 */
#define	PCI_PRODUCT_ALLIANCE_AT25	0x643d		/* AT25 */

/* Alteon products */
#define	PCI_PRODUCT_ALTEON_ACENIC	0x0001		/* ACEnic 1000baseSX Ethernet */
#define	PCI_PRODUCT_ALTEON_ACENIC_COPPER	0x0002		/* ACEnic 1000baseT Ethernet */
#define	PCI_PRODUCT_ALTEON_BCM5700	0x0003		/* ACEnic BCM5700 10/100/1000 Ethernet */
#define	PCI_PRODUCT_ALTEON_BCM5701	0x0004		/* ACEnic BCM5701 10/100/1000 Ethernet */

/* Altima products */
#define	PCI_PRODUCT_ALTIMA_AC1000	0x03e8		/* AC1000 Gigabit Ethernet */
#define	PCI_PRODUCT_ALTIMA_AC1001	0x03e9		/* AC1001 Gigabit Ethernet */
#define	PCI_PRODUCT_ALTIMA_AC9100	0x03ea		/* AC9100 Gigabit Ethernet */

/* AMD products */
#define	PCI_PRODUCT_AMD_AMD64_HT	0x1100		/* AMD64 HyperTransport configuration */
#define	PCI_PRODUCT_AMD_AMD64_ADDR	0x1101		/* AMD64 Address Map configuration */
#define	PCI_PRODUCT_AMD_AMD64_DRAM	0x1102		/* AMD64 DRAM configuration */
#define	PCI_PRODUCT_AMD_AMD64_MISC	0x1103		/* AMD64 Miscellaneous configuration */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* PCnet-PCI Ethernet */
#define	PCI_PRODUCT_AMD_PCNET_HOME	0x2001		/* PCnet-Home HomePNA Ethernet */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* PCscsi-PCI SCSI */
#define	PCI_PRODUCT_AMD_SC520_SC	0x3000		/* Elan SC520 System Controller */
#define	PCI_PRODUCT_AMD_SC751_SC	0x7006		/* AMD751 System Controller */
#define	PCI_PRODUCT_AMD_SC751_PPB	0x7007		/* AMD751 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC756_ISA	0x7408		/* AMD756 PCI-ISA Bridge */
#define	PCI_PRODUCT_AMD_PBC756_IDE	0x7409		/* AMD756 IDE controller */
#define	PCI_PRODUCT_AMD_PBC756_PMC	0x740b		/* AMD756 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC756_USB	0x740c		/* AMD756 USB Host Controller */
#define	PCI_PRODUCT_AMD_SC762_NB	0x700c		/* AMD762 Northbridge */
#define	PCI_PRODUCT_AMD_SC762_PPB	0x700d		/* AMD762 AGP Bridge */
#define	PCI_PRODUCT_AMD_SC761_SC	0x700e		/* AMD761 System Controller */
#define	PCI_PRODUCT_AMD_SC761_PPB	0x700f		/* AMD761 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC766_ISA	0x7410		/* AMD766 Southbridge */
#define	PCI_PRODUCT_AMD_PBC766_IDE	0x7411		/* AMD766 IDE controller */
#define	PCI_PRODUCT_AMD_PBC766_PMC	0x7413		/* AMD766 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC766_USB	0x7414		/* AMD766 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC768_ISA	0x7440		/* AMD768 PCI-ISA/LPC Bridge */
#define	PCI_PRODUCT_AMD_PBC768_IDE	0x7441		/* AMD768 EIDE Controller */
#define	PCI_PRODUCT_AMD_PBC768_PMC	0x7443		/* AMD768 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC768_AC	0x7445		/* AMD768 AC97 Audio */
#define	PCI_PRODUCT_AMD_PBC768_MD	0x7446		/* AMD768 AC97 Modem */
#define	PCI_PRODUCT_AMD_PBC768_PPB	0x7448		/* AMD768 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC768_USB	0x7449		/* AMD768 USB Controller */
#define	PCI_PRODUCT_AMD_PCIX8131_PPB	0x7450		/* AMD8131 PCI-X Tunnel */
#define	PCI_PRODUCT_AMD_PCIX8131_APIC	0x7451		/* AMD8131 IO Apic */
#define	PCI_PRODUCT_AMD_AGP8151_DEV	0x7454		/* AMD8151 AGP Device */
#define	PCI_PRODUCT_AMD_AGP8151_PPB	0x7455		/* AMD8151 AGP Bridge */
#define	PCI_PRODUCT_AMD_PBC8111	0x7460		/* AMD8111 I/O Hub */
#define	PCI_PRODUCT_AMD_PBC8111_USB	0x7464		/* AMD8111 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC8111_LPC	0x7468		/* AMD8111 LPC Controller */
#define	PCI_PRODUCT_AMD_PBC8111_IDE	0x7469		/* AMD8111 IDE Controller */
#define	PCI_PRODUCT_AMD_PBC8111_SMB	0x746a		/* AMD8111 SMBus Controller */
#define	PCI_PRODUCT_AMD_PBC8111_ACPI	0x746b		/* AMD8111 ACPI Controller */
#define	PCI_PRODUCT_AMD_PBC8111_AC	0x746d		/* AMD8111 AC97 Audio */

/* American Megatrends products */
#define	PCI_PRODUCT_AMI_MEGARAID	0x9010		/* MegaRAID */
#define	PCI_PRODUCT_AMI_MEGARAID2	0x9060		/* MegaRAID 2 */
#define	PCI_PRODUCT_AMI_MEGARAID3	0x1960		/* MegaRAID 3 */

/* Analog Devices products */
#define	PCI_PRODUCT_ANALOG_SAFENET	0x2f44		/* SafeNet Crypto Accelerator ADSP-2141 */

/* Antares Microsystems products */
#define	PCI_PRODUCT_ANTARES_TC9021	0x1021		/* Antares Gigabit Ethernet */

/* Apple products */
#define	PCI_PRODUCT_APPLE_BANDIT	0x0001		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_GC	0x0002		/* Grand Central I/O Controller */
#define	PCI_PRODUCT_APPLE_CONTROL	0x0003		/* Control */
#define	PCI_PRODUCT_APPLE_PLANB	0x0004		/* PlanB */
#define	PCI_PRODUCT_APPLE_OHARE	0x0007		/* OHare I/O Controller */
#define	PCI_PRODUCT_APPLE_BANDIT2	0x0008		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_HEATHROW	0x0010		/* Heathrow I/O Controller */
#define	PCI_PRODUCT_APPLE_PADDINGTON	0x0017		/* Paddington I/O Controller */
#define	PCI_PRODUCT_APPLE_KEYLARGO_USB	0x0019		/* KeyLargo USB Controller */
#define	PCI_PRODUCT_APPLE_UNINORTH1	0x001e		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH2	0x001f		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP	0x0020		/* UniNorth AGP Interface */
#define	PCI_PRODUCT_APPLE_GMAC	0x0021		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_KEYLARGO	0x0022		/* KeyLargo I/O Controller */
#define	PCI_PRODUCT_APPLE_GMAC2	0x0024		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_PANGEA_MACIO	0x0025		/* Pangea I/O Controller */
#define	PCI_PRODUCT_APPLE_PANGEA_USB	0x0026		/* Pangea USB Controller */
#define	PCI_PRODUCT_APPLE_PANGEA_AGP	0x0027		/* Pangea AGP Interface */
#define	PCI_PRODUCT_APPLE_PANGEA_PCI1	0x0028		/* Pangea Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_PANGEA_PCI2	0x0029		/* Pangea Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP2	0x002d		/* UniNorth AGP Interface */
#define	PCI_PRODUCT_APPLE_UNINORTH3	0x002e		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH4	0x002f		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_PANGEA_FW	0x0030		/* Pangea Firewire */
#define	PCI_PRODUCT_APPLE_UNINORTH_FW	0x0031		/* UniNorth Firewire */
#define	PCI_PRODUCT_APPLE_GMAC3	0x0032		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_UNINORTH_ATA	0x0033		/* UniNorth ATA/100 Controller */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP3	0x0034		/* UniNorth AGP Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH5	0x0035		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH6	0x0036		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_KAUAI	0x003b		/* Kauai ATA Controller */
#define	PCI_PRODUCT_APPLE_INTREPID	0x003e		/* Intrepid I/O Controller */
#define	PCI_PRODUCT_APPLE_INTREPID_USB	0x003f		/* Intrepid USB Controller */
#define	PCI_PRODUCT_APPLE_K2_USB	0x0040		/* K2 USB Controller */
#define	PCI_PRODUCT_APPLE_K2	0x0041		/* K2 MAC-IO Controller */
#define	PCI_PRODUCT_APPLE_K2_FW	0x0042		/* K2 Firewire */
#define	PCI_PRODUCT_APPLE_K2_UATA	0x0043		/* K2 UATA Controller */
#define	PCI_PRODUCT_APPLE_U3_PPB1	0x0045		/* U3 PCI-PCI bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB2	0x0046		/* U3 PCI-PCI bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB3	0x0047		/* U3 PCI-PCI bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB4	0x0048		/* U3 PCI-PCI bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB5	0x0049		/* U3 PCI-PCI bridge */
#define	PCI_PRODUCT_APPLE_U3_AGP	0x004b		/* U3 AGP Interface */
#define	PCI_PRODUCT_APPLE_K2_GMAC	0x004c		/* GMAC Ethernet */

/* ARC Logic products */
#define	PCI_PRODUCT_ARC_1000PV	0xa091		/* 1000PV */
#define	PCI_PRODUCT_ARC_2000PV	0xa099		/* 2000PV */
#define	PCI_PRODUCT_ARC_2000MT	0xa0a1		/* 2000MT */

/* ASIX Electronics products */
#define	PCI_PRODUCT_ASIX_AX88140A	0x1400		/* AX88140A 10/100 Ethernet */

/* Asustek products */
#define	PCI_PRODUCT_ASUSTEK_HFCPCI	0x0675		/* ISDN */

/* ATI products */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3150	0x3150		/* Radeon Mobility X600 (M24) 3150 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3154	0x3154		/* FireGL M24 GL 3154 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3E50	0x3e50		/* Radeon X600 (RV380) 3E50 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3E54	0x3e54		/* FireGL V3200 (RV380) 3E54 */
#define	PCI_PRODUCT_ATI_RADEON_RS100_4136	0x4136		/* Radeon IGP320 (A3) 4136 */
#define	PCI_PRODUCT_ATI_RADEON_RS200_A7	0x4137		/* Radeon IGP330/340/350 (A4) 4137 */
#define	PCI_PRODUCT_ATI_RADEON_R300_AD	0x4144		/* Radeon 9500 AD */
#define	PCI_PRODUCT_ATI_RADEON_R300_AE	0x4145		/* Radeon 9500 AE */
#define	PCI_PRODUCT_ATI_RADEON_R300_AF	0x4146		/* Radeon 9600TX AF */
#define	PCI_PRODUCT_ATI_RADEON_R300_AG	0x4147		/* FireGL Z1 AG */
#define	PCI_PRODUCT_ATI_RADEON_R350_AH	0x4148		/* Radeon 9800SE AH */
#define	PCI_PRODUCT_ATI_RADEON_R350_AI	0x4149		/* Radeon 9800 AI */
#define	PCI_PRODUCT_ATI_RADEON_R350_AJ	0x414a		/* Radeon 9800 AJ */
#define	PCI_PRODUCT_ATI_RADEON_R350_AK	0x414b		/* FireGL X2 AK */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AP	0x4150		/* Radeon 9600 AP */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AQ	0x4151		/* Radeon 9600SE AQ */
#define	PCI_PRODUCT_ATI_RADEON_RV360_AR	0x4152		/* Radeon 9600XT AR */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AS	0x4153		/* Radeon 9600 AS */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AT	0x4154		/* FireGL T2 AT */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AV	0x4154		/* FireGL RV360 AV */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_RADEON_9600_LE_S	0x4171		/* Radeon 9600 LE Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9600_XT_S	0x4172		/* Radeon 9600 XT Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RS250_B7	0x4237		/* Radeon 7000 IGP (A4+) */
#define	PCI_PRODUCT_ATI_RADEON_R200_BB	0x4242		/* Radeon 8500 AIW BB */
#define	PCI_PRODUCT_ATI_RADEON_R200_BC	0x4243		/* Radeon 8500 AIW BC */
#define	PCI_PRODUCT_ATI_RADEON_RS100_4336	0x4336		/* Radeon IGP320M (U1) 4336 */
#define	PCI_PRODUCT_ATI_RADEON_RS200_4337	0x4337		/* Radeon IGP330M/340M/350M (U2) 4337 */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_200	0x4341		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_SB200_PPB	0x4342		/* SB200 PCI-PCI Bridge */
#define	PCI_PRODUCT_ATI_SB200_EHCI	0x4345		/* SB200 USB2 Host Controller */
#define	PCI_PRODUCT_ATI_SB200_OHCI_1	0x4347		/* SB200 USB Host Controller */
#define	PCI_PRODUCT_ATI_SB200_OHCI_2	0x4348		/* SB200 USB Host Controller */
#define	PCI_PRODUCT_ATI_SB200_ISA	0x434c		/* SB200 PCI-ISA Bridge */
#define	PCI_PRODUCT_ATI_SB200_SMB	0x4353		/* SB200 SMBus Controller */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_300	0x4361		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_400	0x4370		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_200	0x4349		/* IXP IDE Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_300	0x4369		/* IXP IDE Controller */
#define	PCI_PRODUCT_ATI_SB400_PPB	0x4371		/* SB400 PCI-PCI Bridge */
#define	PCI_PRODUCT_ATI_SB400_SMB	0x4372		/* SB400 SMB Controller */
#define	PCI_PRODUCT_ATI_SB400_EHCI	0x4373		/* SB400 USB2 Host Controller */
#define	PCI_PRODUCT_ATI_SB400_OHCI_1	0x4374		/* SB400 USB Host Controller */
#define	PCI_PRODUCT_ATI_SB400_OHCI_2	0x4375		/* SB400 USB Host Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_400	0x4376		/* SB400 IXP IDE Controller */
#define	PCI_PRODUCT_ATI_SB400_ISA	0x4377		/* SB400 PCI-ISA Bridge */
#define	PCI_PRODUCT_ATI_SB400_SATA_1	0x4379		/* SB400 SATA Controller */
#define	PCI_PRODUCT_ATI_SB400_SATA_2	0x437a		/* SB400 SATA Controller */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_RADEON_RS250_D7	0x4437		/* Radeon Mobility 7000 IGP */
#define	PCI_PRODUCT_ATI_RAGE_PRO_AGP	0x4742		/* 3D Rage Pro (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_AGP1X	0x4744		/* 3D Rage Pro (AGP 1x) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_B	0x4749		/* 3D Rage Pro Turbo */
#define	PCI_PRODUCT_ATI_RAGE_XC_PCI66	0x474c		/* Rage XC (PCI66) */
#define	PCI_PRODUCT_ATI_RAGE_XL_AGP	0x474d		/* Rage XL (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_XC_AGP	0x474e		/* Rage XC (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_XL_PCI66	0x474f		/* Rage XL (PCI66) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_P	0x4750		/* 3D Rage Pro */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_L	0x4751		/* 3D Rage Pro (limited 3D) */
#define	PCI_PRODUCT_ATI_RAGE_XL_PCI	0x4752		/* Rage XL */
#define	PCI_PRODUCT_ATI_RAGE_XC_PCI	0x4753		/* Rage XC */
#define	PCI_PRODUCT_ATI_RAGE_II	0x4754		/* 3D Rage I/II */
#define	PCI_PRODUCT_ATI_RAGE_IIP	0x4755		/* 3D Rage II+ */
#define	PCI_PRODUCT_ATI_RAGE_IIC_PCI	0x4756		/* 3D Rage IIC */
#define	PCI_PRODUCT_ATI_RAGE_IIC_AGP_B	0x4757		/* 3D Rage IIC (AGP) */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 GX */
#define	PCI_PRODUCT_ATI_RAGE_IIC	0x4759		/* 3D Rage IIC */
#define	PCI_PRODUCT_ATI_RAGE_IIC_AGP_P	0x475a		/* 3D Rage IIC (AGP) */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4966	0x4966		/* Radeon 9000/PRO If */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4967	0x4967		/* Radeon 9000 Ig */
#define	PCI_PRODUCT_ATI_RADEON_R420_JH	0x4a48		/* Radeon X800 (R420) JH */
#define	PCI_PRODUCT_ATI_RADEON_R420_JI	0x4a49		/* Radeon X800PRO (R420) JI */
#define	PCI_PRODUCT_ATI_RADEON_R420_JJ	0x4a4a		/* Radeon X800SE (R420) JJ */
#define	PCI_PRODUCT_ATI_RADEON_R420_JK	0x4a4b		/* Radeon X800 (R420) JK */
#define	PCI_PRODUCT_ATI_RADEON_R420_JL	0x4a4c		/* Radeon X800 (R420) JL */
#define	PCI_PRODUCT_ATI_RADEON_R420_JM	0x4a4d		/* FireGL X3 (R420) JM */
#define	PCI_PRODUCT_ATI_RADEON_R420_JN	0x4a4e		/* Radeon Mobility 9800 (M18) JN */
#define	PCI_PRODUCT_ATI_RADEON_R420_JP	0x4a4e		/* Radeon X800XT (R420) JP */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_AGP	0x4c42		/* 3D Rage LT Pro (AGP 133MHz) */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_AGP66	0x4c44		/* 3D Rage LT Pro (AGP 66MHz) */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M3_PCI	0x4c45		/* Rage Mobility M3 */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M3_AGP	0x4c46		/* Rage Mobility M3 (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_LT	0x4c47		/* 3D Rage LT */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_PCI	0x4c49		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_MOBILITY	0x4c4d		/* Rage Mobility */
#define	PCI_PRODUCT_ATI_RAGE_L_MOBILITY	0x4c4e		/* Rage L Mobility */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO	0x4c50		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO2	0x4c51		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M1_PCI	0x4c52		/* Rage Mobility M1 (PCI) */
#define	PCI_PRODUCT_ATI_RAGE_L_MOB_M1_PCI	0x4c53		/* Rage L Mobility (PCI) */
#define	PCI_PRODUCT_ATI_RADEON_RV200_LW	0x4c57		/* Radeon Mobility M7 LW */
#define	PCI_PRODUCT_ATI_RADEON_RV200_LX	0x4c58		/* FireGL Mobility 7800 M7 LX */
#define	PCI_PRODUCT_ATI_RADEON_RV100_LY	0x4c59		/* Radeon Mobility M6 LY */
#define	PCI_PRODUCT_ATI_RADEON_RV100_LZ	0x4c5a		/* Radeon Mobility M6 LZ */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C64	0x4c64		/* FireGL Mobility 9000 (M9) Ld */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C66	0x4c66		/* Radeon Mobility 9000 (M9) Lf */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C67	0x4c67		/* Radeon Mobility 9000 (M9) Lg */
#define	PCI_PRODUCT_ATI_RADEON_128_AGP4X	0x4d46		/* Radeon Mobility 128 AGP 4x */
#define	PCI_PRODUCT_ATI_RADEON_128_AGP2X	0x4d4c		/* Radeon Mobility 128 AGP 2x */
#define	PCI_PRODUCT_ATI_RADEON_R300_ND	0x4e44		/* Radeon 9700 ND */
#define	PCI_PRODUCT_ATI_RADEON_R300_NE	0x4e45		/* Radeon 9700/9500Pro NE */
#define	PCI_PRODUCT_ATI_RADEON_R300_NF	0x4e46		/* Radeon 9700 NF */
#define	PCI_PRODUCT_ATI_RADEON_R300_NG	0x4e47		/* FireGL X1 NG */
#define	PCI_PRODUCT_ATI_RADEON_R350_NH	0x4e48		/* Radeon 9800PRO NH */
#define	PCI_PRODUCT_ATI_RADEON_R350_NI	0x4e49		/* Radeon 9800 NI */
#define	PCI_PRODUCT_ATI_RADEON_R360_NJ	0x4e4a		/* Radeon 9800XT NJ */
#define	PCI_PRODUCT_ATI_RADEON_R350_NK	0x4e4b		/* FireGL X2 NK */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NP	0x4e50		/* Radeon Mobility 9600/9700 (M10/11) NP */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NQ	0x4e41		/* Radeon Mobility 9600 (M10) NQ */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NR	0x4e52		/* Radeon Mobility 9600 (M11) NR */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NS	0x4e53		/* Radeon Mobility 9600 (M10) NS */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NT	0x4e54		/* FireGL Mobility T2 (M10) NT */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NV	0x4e56		/* FireGL Mobility T2e (M11) NV */
#define	PCI_PRODUCT_ATI_RADEON_9700_9500_S	0x4e64		/* Radeon 9700/9500 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9700_9500_S2	0x4e65		/* Radeon 9700/9500 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9600_2	0x4e66		/* Radeon 9600TX Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9800_PRO_2	0x4e68		/* Radeon 9800 Pro Secondary */
#define	PCI_PRODUCT_ATI_RAGE1PCI	0x5041		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE1AGP2X	0x5042		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE1AGP4X	0x5043		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE1PCIT	0x5044		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE1AGP2XT	0x5045		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE1AGP4XT	0x5046		/* Rage Fury MAXX AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2PCI	0x5047		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE2AGP2X	0x5048		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE2AGP4X	0x5049		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE2PCIT	0x504a		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2AGP2XT	0x504b		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2AGP4XT	0x504c		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3PCI	0x504d		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE3AGP2X	0x504e		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE3AGP4X	0x504f		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE3PCIT	0x5050		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3AGP2XT	0x5051		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3AGP4XT	0x5052		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4PCI	0x5053		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE4AGP2X	0x5054		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4AGP4X	0x5055		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE4PCIT	0x5056		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4AGP2XT	0x5057		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4AGP4XT	0x5058		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RADEON_R100_QD	0x5144		/* Radeon QD */
#define	PCI_PRODUCT_ATI_RADEON_R100_QE	0x5145		/* Radeon QE */
#define	PCI_PRODUCT_ATI_RADEON_R100_QF	0x5146		/* Radeon QF */
#define	PCI_PRODUCT_ATI_RADEON_R100_QG	0x5147		/* Radeon QG */
#define	PCI_PRODUCT_ATI_RADEON_R200_QH	0x5148		/* FireGL 8700/8800 QH */
#define	PCI_PRODUCT_ATI_RADEON_R200_QL	0x514c		/* Radeon 8500 QL */
#define	PCI_PRODUCT_ATI_RADEON_R200_QM	0x514d		/* Radeon 9100 QM */
#define	PCI_PRODUCT_ATI_RADEON_RV200_QW	0x5157		/* Radeon 7500 QW */
#define	PCI_PRODUCT_ATI_RADEON_RV200_QX	0x5158		/* Radeon 7500 QX */
#define	PCI_PRODUCT_ATI_RADEON_RV100_QY	0x5159		/* Radeon 7000/VE QY */
#define	PCI_PRODUCT_ATI_RADEON_RV100_QZ	0x515a		/* Radeon 7000/VE QZ */
#define	PCI_PRODUCT_ATI_RADEON_9100_S	0x516d		/* Radeon 9100 Series Secondary */
#define	PCI_PRODUCT_ATI_RAGEGLPCI	0x5245		/* Rage 128 GL PCI */
#define	PCI_PRODUCT_ATI_RAGEGLAGP	0x5246		/* Rage 128 GL AGP 2x */
#define	PCI_PRODUCT_ATI_RAGEVRPCI	0x524b		/* Rage 128 VR PCI */
#define	PCI_PRODUCT_ATI_RAGEVRAGP	0x524c		/* Rage 128 VR AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4XPCI	0x5345		/* Rage 128 4x PCI */
#define	PCI_PRODUCT_ATI_RAGE4XA2X	0x5346		/* Rage 128 4x AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4XA4X	0x5347		/* Rage 128 4x AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE4X	0x5348		/* Rage 128 4x */
#define	PCI_PRODUCT_ATI_RAGE24XPCI	0x534b		/* Rage 128 4x PCI */
#define	PCI_PRODUCT_ATI_RAGE24XA2X	0x534c		/* Rage 128 4x AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE24XA4X	0x534d		/* Rage 128 4x AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE24X	0x534e		/* Rage 128 4x */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5460	0x5460		/* Radeon Mobility M300 (M22) 5460 */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5464	0x5464		/* FireGL M22 GL 5464 */
#define	PCI_PRODUCT_ATI_RADEON_R423_UH	0x5548		/* Radeon X800 (R423) UH */
#define	PCI_PRODUCT_ATI_RADEON_R423_UI	0x5549		/* Radeon X800PRO (R423) UI */
#define	PCI_PRODUCT_ATI_RADEON_R423_UJ	0x554a		/* Radeon X800LE (R423) UJ */
#define	PCI_PRODUCT_ATI_RADEON_R423_UK	0x554b		/* Radeon X800SE (R423) UK */
#define	PCI_PRODUCT_ATI_RADEON_R423_UQ	0x5551		/* FireGL V7200 (R423) UQ */
#define	PCI_PRODUCT_ATI_RADEON_R423_UR	0x5552		/* FireGL V5100 (R423) UR */
#define	PCI_PRODUCT_ATI_RADEON_R423_UT	0x5554		/* FireGL V7100 (R423) UT */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x5654		/* Mach64 VT */
#define	PCI_PRODUCT_ATI_MACH64_VTB	0x5655		/* Mach64 VTB */
#define	PCI_PRODUCT_ATI_MACH64_VT4	0x5656		/* Mach64 VT4 */
#define	PCI_PRODUCT_ATI_RS300_HB	0x5833		/* RS300 Host Bridge */
#define	PCI_PRODUCT_ATI_RADEON_RS300_X4	0x5834		/* Radeon 9100 IGP (A4) */
#define	PCI_PRODUCT_ATI_RADEON_RS300_X5	0x5835		/* Radeon Mobility 9100 IGP (U3) */
#define	PCI_PRODUCT_ATI_RS300_AGP	0x5838		/* RS300 AGP Interface */
#define	PCI_PRODUCT_ATI_RADEON_9200_PRO_S	0x5940		/* Radeon 9200 Pro Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9200_S	0x5941		/* Radeon 9200 Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5960	0x5960		/* Radeon 9200PRO 5960 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5961	0x5961		/* Radeon 9200 5961 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5962	0x5962		/* Radeon 9200 5962 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5963	0x5963		/* Radeon 9200 5963 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5964	0x5964		/* Radeon 9200SE 5964 */
#define	PCI_PRODUCT_ATI_RS480_HB	0x5950		/* RS480 Host Bridge */
#define	PCI_PRODUCT_ATI_RS480_XRP	0x5a34		/* RS480 PCI Express Root Port */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B60	0x5b60		/* Radeon X300 (RV370) 5B60 */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B64	0x5b64		/* FireGL V3100 (RV370) 5B64 */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B65	0x5b64		/* FireGL D1100 (RV370) 5B65 */
#define	PCI_PRODUCT_ATI_RADEON_X300_S	0x5b70		/* Radeon X300 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5C61	0x5c61		/* Radeon Mobility 9200 (M9+) */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5C63	0x5c63		/* Radeon Mobility 9200 (M9+) */
#define	PCI_PRODUCT_ATI_RADEON_9200SE_S	0x5d44		/* Radeon 9200SE Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X850XT	0x5d52		/* Radeon X850 XT */
#define	PCI_PRODUCT_ATI_RADEON_R423_5D57	0x5d57		/* Radeon X800XT (R423) 5D57 */
#define	PCI_PRODUCT_ATI_RADEON_X850XT_S	0x5d72		/* Radeon X850 XT Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X700	0x5e4b		/* Radeon X700 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X700_S	0x5e6b		/* Radeon X700 Pro Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RS300_7834	0x7834		/* Radeon 9100 PRO IGP */
#define	PCI_PRODUCT_ATI_RADEON_RS300_7835	0x7835		/* Radeon 9200 IGP */

/* Auravision products */
#define	PCI_PRODUCT_AURAVISION_VXP524	0x01f7		/* VxP524 PCI Video Processor */

/* Aureal Semiconductor */
#define	PCI_PRODUCT_AUREAL_AU8820	0x0001		/* AU8820 Vortex Digital Audio Processor */

/* Applied Micro Circuts products */
#define	PCI_PRODUCT_AMCIRCUITS_S5933	0x4750		/* S5933 PCI Matchmaker */
#define	PCI_PRODUCT_AMCIRCUITS_LANAI	0x8043		/* Myrinet LANai Interface */
#define	PCI_PRODUCT_AMCIRCUITS_CAMAC	0x812d		/* FZJ/ZEL CAMAC controller */
#define	PCI_PRODUCT_AMCIRCUITS_VICBUS	0x812e		/* FZJ/ZEL VICBUS interface */
#define	PCI_PRODUCT_AMCIRCUITS_PCISYNC	0x812f		/* FZJ/ZEL Synchronisation module */
#define	PCI_PRODUCT_AMCIRCUITS_ADDI7800	0x818e		/* ADDI-DATA APCI-7800 8-port serial */
#define	PCI_PRODUCT_AMCIRCUITS_S5920	0x5920		/* S5920 PCI Target */

/* Atheros Communications products */
#define	PCI_PRODUCT_ATHEROS_AR5201	0x0007		/* AR5201 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5311	0x0011		/* AR5211 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5211	0x0012		/* AR5211 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5212	0x0013		/* AR5212 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5201_AP	0x0207		/* AR5201 Wireless LAN Reference Card (Early AP11) */
#define	PCI_PRODUCT_ATHEROS_AR5201_DEFAULT	0x1107		/* AR5201 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_DEFAULT	0x1113		/* AR5212 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5211_DEFAULT	0x1112		/* AR5211 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_FPGA	0xf013		/* AR5212 Wireless LAN Reference Card (emulation board) */
#define	PCI_PRODUCT_ATHEROS_AR5211_FPGA11B	0xf11b		/* AR5211 Wireless LAN Reference Card (11b emulation board) */
#define	PCI_PRODUCT_ATHEROS_AR5211_LEGACY	0xff12		/* AR5211 Wireless LAN Reference Card (original emulation board) */

/* Atronics products */
#define	PCI_PRODUCT_ATRONICS_IDE_2015PL	0x2015		/* IDE-2015PL */

/* Avance Logic products */
#define	PCI_PRODUCT_AVANCE_AVL2301	0x2301		/* AVL2301 */
#define	PCI_PRODUCT_AVANCE_AVG2302	0x2302		/* AVG2302 */
#define	PCI_PRODUCT_AVANCE2_ALG2301	0x2301		/* ALG2301 */
#define	PCI_PRODUCT_AVANCE2_ALG2302	0x2302		/* ALG2302 */
#define	PCI_PRODUCT_AVANCE2_ALS4000	0x4000		/* ALS4000 Audio */

/* Avlab Technology products */
#define	PCI_PRODUCT_AVLAB_LPPCI4S	0x2150		/* Low Profile PCI 4 Serial */

/* CCUBE products */
#define	PCI_PRODUCT_CCUBE_CINEMASTER	0x8888		/* Cinemaster C 3.0 DVD Decoder */

/* AVM products */
#define	PCI_PRODUCT_AVM_FRITZ_CARD	0x0a00		/* Fritz! Card ISDN Interface */
#define	PCI_PRODUCT_AVM_FRITZ_PCI_V2_ISDN	0x0e00		/* Fritz!PCI v2.0 ISDN Interface */
#define	PCI_PRODUCT_AVM_B1	0x0700		/* Basic Rate B1 ISDN Interface */
#define	PCI_PRODUCT_AVM_T1	0x1200		/* Primary Rate T1 ISDN Interface */

/* Belkin products */
#define	PCI_PRODUCT_BELKIN_F5D6001	0x6001		/* F5D6001 */
#define	PCI_PRODUCT_BELKIN_F5D6020V3	0x6020		/* F5D6020v3 802.11b */

/* Stallion products */
#define	PCI_PRODUCT_STALLION_EC8_32	0x0000		/* EC8/32 */
#define	PCI_PRODUCT_STALLION_EC8_64	0x0002		/* EC8/64 */
#define	PCI_PRODUCT_STALLION_EASYIO	0x0003		/* EasyIO */

/* Bit3 products */
#define	PCI_PRODUCT_BIT3_PCIVME617	0x0001		/* PCI-VME Interface Mod. 617 */
#define	PCI_PRODUCT_BIT3_PCIVME618	0x0010		/* PCI-VME Interface Mod. 618 */
#define	PCI_PRODUCT_BIT3_PCIVME2706	0x0300		/* PCI-VME Interface Mod. 2706 */

/* Bluesteel Networks */
#define	PCI_PRODUCT_BLUESTEEL_5501	0x0000		/* 5501 */
#define	PCI_PRODUCT_BLUESTEEL_5601	0x5601		/* 5601 */

/* Broadcom products */
#define	PCI_PRODUCT_BROADCOM_BCM5700	0x1644		/* BCM5700 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5701	0x1645		/* BCM5701 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702	0x1646		/* BCM5702 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702X	0x16a6		/* BCM5702X 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702FE	0x164d		/* BCM5702FE 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703	0x1647		/* BCM5703 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703X	0x16a7		/* BCM5703X 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703A3	0x16c7		/* BCM5703 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5704C	0x1648		/* BCM5704C 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5704S	0x16a8		/* BCM5704S 1000baseX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705	0x1653		/* BCM5705 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705K	0x1654		/* BCM5705K 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705M	0x165d		/* BCM5705M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705_ALT	0x165e		/* BCM5705 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5714	0x1668		/* BCM5714 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5714S	0x1669		/* BCM5714S 1000baseX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5715	0x1678		/* BCM5715 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5715S	0x1679		/* BCM5715S 1000baseX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5721	0x1659		/* BCM5721 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5750	0x1676		/* BCM5750 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5751	0x1677		/* BCM5751 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5752	0x1600		/* BCM5752 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5750M	0x167c		/* BCM5750M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5751M	0x167d		/* BCM5751M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5780	0x166a		/* BCM5780 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5780S	0x166b		/* BCM5780S 1000baseX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5782	0x1696		/* BCM5782 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5788	0x169c		/* BCM5788 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5789	0x169d		/* BCM5789 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM4401_B0	0x170c		/* BCM4401-B0 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5901	0x170d		/* BCM5901 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5901A2	0x170e		/* BCM5901A 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM4401	0x4401		/* BCM4401 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_5801	0x5801		/* 5801 Security processor */
#define	PCI_PRODUCT_BROADCOM_5802	0x5802		/* 5802 Security processor */
#define	PCI_PRODUCT_BROADCOM_5805	0x5805		/* 5805 Security processor */
#define	PCI_PRODUCT_BROADCOM_5820	0x5820		/* 5820 Security processor */
#define	PCI_PRODUCT_BROADCOM_5821	0x5821		/* 5821 Security processor */
#define	PCI_PRODUCT_BROADCOM_5822	0x5822		/* 5822 Security processor */
#define	PCI_PRODUCT_BROADCOM_5823	0x5823		/* 5823 Security processor */

/* Brooktree products */
#define	PCI_PRODUCT_BROOKTREE_BT848	0x0350		/* Bt848 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT849	0x0351		/* Bt849 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT878	0x036e		/* Bt878 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT879	0x036f		/* Bt879 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT880	0x0370		/* Bt880 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT878A	0x0878		/* Bt878 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT879A	0x0879		/* Bt879 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT880A	0x0880		/* Bt880 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT8474	0x8474		/* Bt8474 Multichannel HDLC Controller */

/* BusLogic products */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC	0x0140		/* MultiMaster NC */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER	0x1040		/* MultiMaster */
#define	PCI_PRODUCT_BUSLOGIC_FLASHPOINT	0x8130		/* FlashPoint */

/* c't Magazin products */
#define	PCI_PRODUCT_C4T_GPPCI	0x6773		/* GPPCI */

/* Cavium products */
#define	PCI_PRODUCT_CAVIUM_NITROX	0x0001		/* Nitrox XL */

/* Chips and Technologies products */
#define	PCI_PRODUCT_CHIPS_64310	0x00b8		/* 64310 */
#define	PCI_PRODUCT_CHIPS_69000	0x00c0		/* 69000 */
#define	PCI_PRODUCT_CHIPS_65545	0x00d8		/* 65545 */
#define	PCI_PRODUCT_CHIPS_65548	0x00dc		/* 65548 */
#define	PCI_PRODUCT_CHIPS_65550	0x00e0		/* 65550 */
#define	PCI_PRODUCT_CHIPS_65554	0x00e4		/* 65554 */
#define	PCI_PRODUCT_CHIPS_69030	0x0c30		/* 69030 */

/* Chrysalis products */
#define	PCI_PRODUCT_CHRYSALIS_LUNAVPN	0x0001		/* LunaVPN */

/* Cirrus Logic products */
#define	PCI_PRODUCT_CIRRUS_CL_GD7548	0x0038		/* CL-GD7548 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5430	0x00a0		/* CL-GD5430 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_4	0x00a4		/* CL-GD5434-4 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_8	0x00a8		/* CL-GD5434-8 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5436	0x00ac		/* CL-GD5436 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5446	0x00b8		/* CL-GD5446 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5480	0x00bc		/* CL-GD5480 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6729	0x1100		/* CL-PD6729 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6832	0x1110		/* CL-PD6832 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_PD6833	0x1113		/* CL-PD6833 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_GD7542	0x1200		/* CL-GD7542 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7543	0x1202		/* CL-GD7543 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7541	0x1204		/* CL-GD7541 */
#define	PCI_PRODUCT_CIRRUS_CL_CD4400	0x4400		/* CL-CD4400 Communications Controller */
#define	PCI_PRODUCT_CIRRUS_CS4610	0x6001		/* CS4610 SoundFusion Audio Accelerator */
#define	PCI_PRODUCT_CIRRUS_CS4280	0x6003		/* CS4280 CrystalClear Audio Interface */
#define	PCI_PRODUCT_CIRRUS_CS4615	0x6004		/* CS4615 */
#define	PCI_PRODUCT_CIRRUS_CS4281	0x6005		/* CS4281 CrystalClear Audio Interface */

/* Adaptec's AAR-1210SA serial ATA RAID controller uses the CMDTECH chip */
#define	PCI_PRODUCT_CMDTECH_AAR_1210SA	0x0240		/* AAR-1210SA serial ATA RAID controller */
/* CMD Technology products -- info gleaned from their web site */
#define	PCI_PRODUCT_CMDTECH_640	0x0640		/* PCI0640 */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_642	0x0642		/* PCI0642 */
/* datasheets available from www.cmd.com for the followings */
#define	PCI_PRODUCT_CMDTECH_643	0x0643		/* PCI0643 */
#define	PCI_PRODUCT_CMDTECH_646	0x0646		/* PCI0646 */
#define	PCI_PRODUCT_CMDTECH_647	0x0647		/* PCI0647 */
#define	PCI_PRODUCT_CMDTECH_648	0x0648		/* PCI0648 */
#define	PCI_PRODUCT_CMDTECH_649	0x0649		/* PCI0649 */

/* Inclusion of 'A' in the following entry is probably wrong. */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_650A	0x0650		/* PCI0650A */
#define	PCI_PRODUCT_CMDTECH_670	0x0670		/* USB0670 */
#define	PCI_PRODUCT_CMDTECH_673	0x0673		/* USB0673 */
#define	PCI_PRODUCT_CMDTECH_680	0x0680		/* SiI0680 */
#define	PCI_PRODUCT_CMDTECH_3112	0x3112		/* SiI3112 SATALink */
#define	PCI_PRODUCT_CMDTECH_3512	0x3512		/* SiI3512 SATALink */
#define	PCI_PRODUCT_CMDTECH_3114	0x3114		/* SiI3114 SATALink */
#define	PCI_PRODUCT_CMDTECH_3124	0x3124		/* SiI3124 SATALink */
#define	PCI_PRODUCT_CMDTECH_3132	0x3132		/* SiI3132 SATALink */

/* C-Media products */
#define	PCI_PRODUCT_CMEDIA_CMI8338A	0x0100		/* CMI8338A PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8338B	0x0101		/* CMI8338B PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8738	0x0111		/* CMI8738/C3DX PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8738B	0x0112		/* CMI8738B PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_HSP56	0x0211		/* HSP56 Audiomodem Riser */

/* Cogent Data Technologies products */
#define	PCI_PRODUCT_COGENT_EM110TX	0x1400		/* EX110TX PCI Fast Ethernet Adapter */

/* Cologne Chip Designs */
#define	PCI_PRODUCT_COLOGNECHIP_HFC	0x2bd0		/* HFC-S */

/* COMPAL products */
#define	PCI_PRODUCT_COMPAL_38W2	0x0011		/* 38W2 OEM Notebook */

/* Compaq products */
#define	PCI_PRODUCT_COMPAQ_PCI_EISA_BRIDGE	0x0001		/* PCI-EISA Bridge */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE	0x0002		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX1	0x1000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX2	0x2000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_QVISION_V0	0x3032		/* QVision */
#define	PCI_PRODUCT_COMPAQ_QVISION_1280P	0x3033		/* QVision 1280/p */
#define	PCI_PRODUCT_COMPAQ_QVISION_V2	0x3034		/* QVision */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX4	0x4000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_CSA5300	0x4070		/* Smart Array 5300 */
#define	PCI_PRODUCT_COMPAQ_CSA5i	0x4080		/* Smart Array 5i */
#define	PCI_PRODUCT_COMPAQ_CSA532	0x4082		/* Smart Array 532 */
#define	PCI_PRODUCT_COMPAQ_CSA5312	0x4083		/* Smart Array 5312 */
#define	PCI_PRODUCT_COMPAQ_CSA6i	0x4091		/* Smart Array 6i */
#define	PCI_PRODUCT_COMPAQ_CSA641	0x409a		/* Smart Array 641 */
#define	PCI_PRODUCT_COMPAQ_CSA642	0x409b		/* Smart Array 642 */
#define	PCI_PRODUCT_COMPAQ_CSA6400	0x409c		/* Smart Array 6400 */
#define	PCI_PRODUCT_COMPAQ_CSA6400EM	0x409d		/* Smart Array 6400 EM */
#define	PCI_PRODUCT_COMPAQ_CSA6422	0x409e		/* Smart Array 6422 */
#define	PCI_PRODUCT_COMPAQ_CSA64XX	0x0046		/* Smart Array 64xx */
#define	PCI_PRODUCT_COMPAQ_USB	0x7020		/* USB Controller */
#define	PCI_PRODUCT_COMPAQ_ASMC	0xa0f0		/* Advanced Systems Management Controller */
/* MediaGX Cx55x0 built-in OHCI seems to have this ID */
#define	PCI_PRODUCT_COMPAQ_USB_MEDIAGX	0xa0f8		/* USB Controller */
#define	PCI_PRODUCT_COMPAQ_SMART2P	0xae10		/* SMART2P RAID */
#define	PCI_PRODUCT_COMPAQ_N100TX	0xae32		/* Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_N10T2	0xb012		/* Netelligent 10 T/2 UTP/Coax */
#define	PCI_PRODUCT_COMPAQ_INT100TX	0xb030		/* Integrated Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_N10T	0xae34		/* Netelligent 10 T */
#define	PCI_PRODUCT_COMPAQ_IntNF3P	0xae35		/* Integrated NetFlex 3/P */
#define	PCI_PRODUCT_COMPAQ_DPNet100TX	0xae40		/* Dual Port Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_IntPL100TX	0xae43		/* ProLiant Integrated Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_DP4000	0xb011		/* Deskpro 4000 5233MMX */
#define	PCI_PRODUCT_COMPAQ_CSA5300_2	0xb060		/* Smart Array 5300 rev.2 */
#define	PCI_PRODUCT_COMPAQ_PRESARIO56XX	0xb0b8		/* Presario 56xx */
#define	PCI_PRODUCT_COMPAQ_M700	0xb112		/* Armada M700 */
#define	PCI_PRODUCT_COMPAQ_CSA5i_2	0xb178		/* Smart Array 5i/532 rev.2 */
#define	PCI_PRODUCT_COMPAQ_NF3P_BNC	0xf150		/* NetFlex 3/P w/ BNC */
#define	PCI_PRODUCT_COMPAQ_NF3P	0xf130		/* NetFlex 3/P */

/* Compex products - XXX better descriptions */
#define	PCI_PRODUCT_COMPEX_NE2KETHER	0x1401		/* Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100ATX	0x2011		/* RL100-ATX 10/100 Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100TX	0x9881		/* RL100-TX 10/100 Ethernet */

/* Comtrol products */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT32EXT	0x0001		/* RocketPort 32 port external */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8EXT	0x0002		/* RocketPort 8 port external */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT16EXT	0x0003		/* RocketPort 16 port external */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT4QUAD	0x0004		/* RocketPort 4 port w/ quad cable */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8OCTA	0x0005		/* RocketPort 8 port w/ octa cable */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8RJ	0x0006		/* RocketPort 8 port w/ RJ11s */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT4RJ	0x0007		/* RocketPort 4 port w/ RJ11s */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8DB	0x0008		/* RocketPort 8 port w/ DB78 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT16DB	0x0009		/* RocketPort 16 port w/ DB78 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP4	0x000a		/* RocketPort Plus 4 port */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP8	0x000b		/* RocketPort Plus 8 port */
#define	PCI_PRODUCT_COMTROL_ROCKETMODEM6	0x000c		/* RocketModem 6 port */
#define	PCI_PRODUCT_COMTROL_ROCKETMODEM4	0x000d		/* RocketModem 4 port */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP232	0x000e		/* RocketPort 2 port RS232 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP422	0x000f		/* RocketPort 2 port RS422 */

/* Conexant Systems products */
#define	PCI_PRODUCT_CONEXANT_SOFTK56	0x2443		/* SoftK56 PCI Software Modem */
#define	PCI_PRODUCT_CONEXANT_56KFAXMODEM	0x1085		/* HW 56K Fax Modem */
#define	PCI_PRODUCT_CONEXANT_LANFINITY	0x1803		/* LANfinity MiniPCI 10/100 Ethernet */

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 PCI-VLB Bridge */
#define	PCI_PRODUCT_CONTAQ_82C693	0xc693		/* 82C693 PCI-ISA Bridge */

/* Corega products */
#define	PCI_PRODUCT_COREGA_CB_TXD	0xa117		/* FEther CB-TXD 10/100 Ethernet */
#define	PCI_PRODUCT_COREGA_2CB_TXD	0xa11e		/* FEther II CB-TXD 10/100 Ethernet */
#define	PCI_PRODUCT_COREGA_LAPCIGT	0xc107		/* CG-LAPCIGT */

/* Corollary Products */
#define	PCI_PRODUCT_COROLLARY_CBUSII_PCIB	0x0014		/* \"C-Bus II\"-PCI Bridge */

/* Creative Labs products */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE	0x0002		/* SBLive! EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGY	0x0004		/* SB Audigy EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE2	0x0006		/* SBLive! EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_SBAUDIGYLS	0x0007		/* SB Audigy LS */
#define	PCI_PRODUCT_CREATIVELABS_SBAUDIGY4	0x0008		/* SB Audigy 4 */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY	0x7002		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY2	0x7004		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_EV1938	0x8938		/* Ectiva 1938 */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_1	0x0102		/* Cyclom-4Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_2	0x0103		/* Cyclom-4Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_1	0x0104		/* Cyclom-8Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_2	0x0105		/* Cyclom-8Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_1	0x0200		/* Cyclom-Z below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_2	0x0201		/* Cyclom-Z above 1M */

/* Cyclone Microsystems products */
#define	PCI_PRODUCT_CYCLONE_PCI_700	0x0700		/* IQ80310 (PCI-700) */

/* Cyrix (now National) products */
#define	PCI_PRODUCT_CYRIX_MEDIAGX_PCHB	0x0001		/* MediaGX Built-in PCI Host Controller */
#define	PCI_PRODUCT_CYRIX_CX5520_PCIB	0x0002		/* Cx5520 I/O Companion */
#define	PCI_PRODUCT_CYRIX_CX5530_PCIB	0x0100		/* Cx5530 I/O Companion Multi-Function Southbridge */
#define	PCI_PRODUCT_CYRIX_CX5530_SMI	0x0101		/* Cx5530 I/O Companion (SMI Status and ACPI Timer) */
#define	PCI_PRODUCT_CYRIX_CX5530_IDE	0x0102		/* Cx5530 I/O Companion (IDE Controller) */
#define	PCI_PRODUCT_CYRIX_CX5530_AUDIO	0x0103		/* Cx5530 I/O Companion (XpressAUDIO) */
#define	PCI_PRODUCT_CYRIX_CX5530_VIDEO	0x0104		/* Cx5530 I/O Companion (Video Controller) */

/* Davicom Semiconductor products */
#define	PCI_PRODUCT_DAVICOM_DM9102	0x9102		/* DM9102 10/100 Ethernet */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* DC21050 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* DC21040 (\"Tulip\") Ethernet */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* DC21030 (\"TGA\") */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* DC21140 (\"FasterNet\") 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_PBXGB	0x000d		/* TGA2 */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
/* product DEC ???	0x0010	??? VME Interface */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* DC21041 (\"Tulip Plus\") Ethernet */
#define	PCI_PRODUCT_DEC_DGLPB	0x0016		/* DGLPB (\"OPPO\") */
#define	PCI_PRODUCT_DEC_21142	0x0019		/* DC21142/21143 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_21052	0x0021		/* DC21052 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21150	0x0022		/* DC21150 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21152	0x0024		/* DC21152 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21153	0x0025		/* DC21153 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21154	0x0026		/* DC21154 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21554	0x0046		/* DC21554 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_SWXCR	0x1065		/* SWXCR RAID */

/* Dell Computer products */
#define	PCI_PRODUCT_DELL_PERC_2SI	0x0001		/* PERC 2/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI	0x0002		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI	0x0003		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3SI_2	0x0004		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_2	0x0008		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3	0x000a		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_4DI	0x000e		/* PERC 4/Di */
#define	PCI_PRODUCT_DELL_PERC_4DI_2	0x000f		/* PERC 4/Di */
#define	PCI_PRODUCT_DELL_PERC_4ESI	0x0013		/* PERC 4e/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_2_SUB	0x00cf		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI_2_SUB	0x00d0		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB2	0x00d1		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB3	0x00d9		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB	0x0106		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB2	0x011b		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB3	0x0121		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_CERC_1_5	0x0291		/* CERC SATA RAID 1.5/6ch */

/* Delta products */
#define	PCI_PRODUCT_DELTA_8139	0x1360		/* 8139 Ethernet */
#define	PCI_PRODUCT_DELTA_RHINEII	0x1320		/* Rhine II 10/100 Ethernet */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_VIPER	0x9001		/* Viper/PCI */

/* D-Link Systems products */
#define	PCI_PRODUCT_DLINK_DL1002	0x1002		/* DL-1002 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DFE530TXPLUS	0x1300		/* DFE-530TXPLUS 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DFE690TXD	0x1340		/* DFE-690TXD 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DL4000	0x4000		/* DL-4000 Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE528T	0x4300		/* DGE-528T Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE530T	0x4c00		/* DGE-530T Gigabit Ethernet */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/SmartRAID (EATA) */
#define	PCI_PRODUCT_DPT_I960_PPB	0xa500		/* PCI-PCI Bridge */
#define	PCI_PRODUCT_DPT_RAID_I2O	0xa501		/* SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_RAID_2005S	0xa511		/* Zero Channel SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_MEMCTLR	0x1012		/* Memory Controller */

/* Dolphin products */
#define	PCI_PRODUCT_DOLPHIN_PCISCI32	0x0658		/* PCI-SCI Bridge (32-bit, 33 MHz) */
#define	PCI_PRODUCT_DOLPHIN_PCISCI64	0xd665		/* PCI-SCI Bridge (64-bit, 33 MHz) */
#define	PCI_PRODUCT_DOLPHIN_PCISCI66	0xd667		/* PCI-SCI Bridge (64-bit, 66 MHz) */

/* Domex products */
#define	PCI_PRODUCT_DOMEX_PCISCSI	0x0001		/* DMX-3191D */

/* Dynalink products */
#define	PCI_PRODUCT_DYNALINK_IS64PH	0x1702		/* IS64PH ISDN Adapter */

/* ELSA products */
#define	PCI_PRODUCT_ELSA_QS1PCI	0x1000		/* QuickStep 1000 ISDN card */
#define	PCI_PRODUCT_ELSA_GLORIAXL	0x8901		/* Gloria XL 1624 */

/* Emulex products */
#define	PCI_PRODUCT_EMULEX_LP6000	0x1ae5		/* LP6000 FibreChannel adapter */
#define	PCI_PRODUCT_EMULEX_LP982	0xf098		/* LP982 FibreChannel adapter */
#define	PCI_PRODUCT_EMULEX_LP7000	0xf700		/* LP7000 FibreChannel adapter */
#define	PCI_PRODUCT_EMULEX_LP8000	0xf800		/* LP8000 FibreChannel adapter */
#define	PCI_PRODUCT_EMULEX_LP9000	0xf900		/* LP9000 FibreChannel adapter */
#define	PCI_PRODUCT_EMULEX_LP9802	0xf980		/* LP9802 FibreChannel adapter */

/* ENE Technology products */
#define	PCI_PRODUCT_ENE_MCR510	0x0510		/* MCR510 PCI Memory Card Reader Controller */
#define	PCI_PRODUCT_ENE_CB1211	0x1211		/* CB1211 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1225	0x1225		/* CB1225 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1410	0x1410		/* CB1410 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB710	0x1411		/* CB710 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1420	0x1420		/* CB1420 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB720	0x1421		/* CB720 CardBus Controller */

/* Ensoniq products */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI	0x5000		/* AudioPCI */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI97	0x1371		/* AudioPCI 97 */
#define	PCI_PRODUCT_ENSONIQ_CT5880	0x5880		/* CT5880 */

/* Equinox Systems product */
#define	PCI_PRODUCT_EQUINOX_SST64P	0x0808		/* SST-64P adapter */
#define	PCI_PRODUCT_EQUINOX_SST128P	0x1010		/* SST-128P adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_1	0x80c0		/* SST-16P adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_2	0x80c4		/* SST-16P adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_3	0x80c8		/* SST-16P adapter */
#define	PCI_PRODUCT_EQUINOX_SST4P	0x8888		/* SST-4P adapter */
#define	PCI_PRODUCT_EQUINOX_SST8P	0x9090		/* SST-8P adapter */

/* Essential Communications products */
#define	PCI_PRODUCT_ESSENTIAL_RR_HIPPI	0x0001		/* RoadRunner HIPPI Interface */
#define	PCI_PRODUCT_ESSENTIAL_RR_GIGE	0x0005		/* RoadRunner Gig-E Interface */

/* ESS Technology products */
#define	PCI_PRODUCT_ESSTECH_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2	0x1968		/* Maestro 2 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_SOLO1	0x1969		/* Solo-1 PCI AudioDrive */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2E	0x1978		/* Maestro 2E PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_ALLEGRO1	0x1988		/* Allegro-1 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3	0x1998		/* Maestro 3 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3MODEM	0x1999		/* Maestro 3 Modem */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3_2	0x199a		/* Maestro 3 PCI Audio Accelerator */

/* ESS Technology products */
#define	PCI_PRODUCT_ESSTECH2_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */

/* Eumitcom products */
#define	PCI_PRODUCT_EUMITCOM_WL11000P	0x1100		/* WL11000P PCI WaveLAN/IEEE 802.11 */

/* O2 Micro */
#define	PCI_PRODUCT_O2MICRO_OZ6729	0x6729		/* OZ6729 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6730	0x673A		/* OZ6730 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6832	0x6832		/* OZ6832/OZ6833 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6836	0x6836		/* OZ6836/OZ6860 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6872	0x6872		/* OZ6812/OZ6872 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6922	0x6925		/* OZ6922 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6933	0x6933		/* OZ6933 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6972	0x6972		/* OZ6912/OZ6972 PCI-CardBus Bridge */

/* Evans & Sutherland products */
#define	PCI_PRODUCT_ES_FREEDOM	0x0001		/* Freedom PCI-GBus Interface */

/* EXAR products */
#define	PCI_PRODUCT_EXAR_XR17D152	0x0152		/* dual-channel Universal PCI UART */
#define	PCI_PRODUCT_EXAR_XR17D154	0x0154		/* quad-channel Universal PCI UART */
#define	PCI_PRODUCT_EXAR_XR17D158	0x0158		/* octal-channel Universal PCI UART */

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Forte Media products */
#define	PCI_PRODUCT_FORTEMEDIA_FM801	0x0801		/* 801 Sound */
#define	PCI_PRODUCT_FORTEMEDIA_PCIJOY	0x0802		/* PCI Gameport Joystick */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* FZ Juelich / ZEL products */
#define	PCI_PRODUCT_FZJZEL_GIGALINK	0x0001		/* Gigabit link / STR1100 */
#define	PCI_PRODUCT_FZJZEL_PLXHOTLINK	0x0002		/* HOTlink interface */
#define	PCI_PRODUCT_FZJZEL_COUNTTIME	0x0003		/* Counter / Timer */
#define	PCI_PRODUCT_FZJZEL_PLXCAMAC	0x0004		/* CAMAC controller */
#define	PCI_PRODUCT_FZJZEL_PROFIBUS	0x0005		/* PROFIBUS interface */
#define	PCI_PRODUCT_FZJZEL_AMCCHOTLINK	0x0006		/* old HOTlink interface */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI25P	0x0003		/* SpeedStream ENI-25p */
#define	PCI_PRODUCT_EFFICIENTNETS_SS3000	0x0005		/* SpeedStream 3000 */

/* Galileo Technology products */
#define	PCI_PRODUCT_GALILEO_GT64010A	0x0146		/* GT-64010A System Controller */
#define	PCI_PRODUCT_GALILEO_GT64115	0x4111		/* GT-64115 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64011	0x4146		/* GT-64011 System Controller */
#define	PCI_PRODUCT_GALILEO_SKNET	0x4320		/* SK-NET Gigabit Ethernet */
#define	PCI_PRODUCT_GALILEO_GT64120	0x4620		/* GT-64120 System Controller */
#define	PCI_PRODUCT_GALILEO_BELKIN	0x5005		/* Belkin Gigabit Ethernet */
#define	PCI_PRODUCT_GALILEO_GT64130	0x6320		/* GT-64130 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64260	0x6430		/* GT-64260 System Controller */
#define	PCI_PRODUCT_GALILEO_GT64360	0x6460		/* MV6436x System Controller */

/* Global Sun Tech products */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P	0x1101		/* GL24110P PCI IEEE 802.11b */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P02	0x1102		/* GL24110P PCI IEEE 802.11b */

/* Guillemot products */
#define	PCI_PRODUCT_GUILLEMOT_MAXIRADIO	0x1001		/* MAXIRADIO */

/* Hawking products */
#define	PCI_PRODUCT_HAWKING_PN672TX	0xab08		/* PN672TX 10/100 Ethernet */

/* Heuricon products */
#define	PCI_PRODUCT_HEURICON_PMPPC	0x000e		/* PM/PPC */

/* Hewlett-Packard products */
#define	PCI_PRODUCT_HP_A4977A	0x1005		/* A4977A Visualize EG */
#define	PCI_PRODUCT_HP_TACHYON_TL	0x1028		/* Tachyon TL FC controller */
#define	PCI_PRODUCT_HP_TACHYON_TS	0x102A		/* Tachyon TS FC controller */
#define	PCI_PRODUCT_HP_TACHYON_XL2	0x1030		/* Tachyon XL2 FC controller */
#define	PCI_PRODUCT_HP_J2585A	0x1030		/* J2585A */
#define	PCI_PRODUCT_HP_J2585B	0x1031		/* J2585B */
#define	PCI_PRODUCT_HP_82557B	0x1200		/* 82557B 10/100 NIC */
#define	PCI_PRODUCT_HP_NETRAID_4M	0x10c2		/* NetRaid-4M */

#define	PCI_PRODUCT_HP_HPSAV100	0x3210		/* Smart Array V100 */
#define	PCI_PRODUCT_HP_HPSAE200I_1	0x3211		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200	0x3212		/* Smart Array E200 */
#define	PCI_PRODUCT_HP_HPSAE200I_2	0x3213		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_3	0x3214		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_4	0x3215		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSA_1	0x3220		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_2	0x3222		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP800	0x3223		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSAP600	0x3225		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSA_3	0x3230		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_4	0x3231		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_5	0x3232		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_6	0x3233		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP400	0x3234		/* Smart Array P400 */
#define	PCI_PRODUCT_HP_HPSAP400I	0x3235		/* Smart Array P400i */
#define	PCI_PRODUCT_HP_HPSA_7	0x3236		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_8	0x3237		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_9	0x3238		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_10	0x3239		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_11	0x323a		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_12	0x323b		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_13	0x323c		/* Smart Array */

/* Hifn products */
#define	PCI_PRODUCT_HIFN_7751	0x0005		/* 7751 */
#define	PCI_PRODUCT_HIFN_6500	0x0006		/* 6500 */
#define	PCI_PRODUCT_HIFN_7811	0x0007		/* 7811 */
#define	PCI_PRODUCT_HIFN_7951	0x0012		/* 7951 */
#define	PCI_PRODUCT_HIFN_7955	0x0020		/* 7954/7955 */
#define	PCI_PRODUCT_HIFN_7956	0x001d		/* 7956 */
#define	PCI_PRODUCT_HIFN_78XX	0x0014		/* 7814/7851/7854 */
#define	PCI_PRODUCT_HIFN_8065	0x0016		/* 8065 */
#define	PCI_PRODUCT_HIFN_8165	0x0017		/* 8165 */
#define	PCI_PRODUCT_HIFN_8154	0x0018		/* 8154 */

/* HiNT products */
#define	PCI_PRODUCT_HINT_HB1	0x0021		/* HB1 PCI-PCI Bridge */
#define	PCI_PRODUCT_HINT_HB4	0x0022		/* HB4 PCI-PCI Bridge */

/* Hitachi products */
#define	PCI_PRODUCT_HITACHI_SWC	0x0101		/* MSVCC01/02/03/04 Video Capture Cards */
#define	PCI_PRODUCT_HITACHI_SH7751	0x3505		/* SH7751 PCI Controller */
#define	PCI_PRODUCT_HITACHI_SH7751R	0x350e		/* SH7751R PCI Controller */

/* IBM products */
#define	PCI_PRODUCT_IBM_MCABRIDGE	0x0002		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_ALTALITE	0x0005		/* CPU Bridge - Alta Lite */
#define	PCI_PRODUCT_IBM_ALTAMP	0x0007		/* CPU Bridge - Alta MP */
#define	PCI_PRODUCT_IBM_ISABRIDGE	0x000a		/* ISA Bridge w/PnP */
#define	PCI_PRODUCT_IBM_CPUBRIDGE	0x0017		/* CPU Bridge */
#define	PCI_PRODUCT_IBM_LANSTREAMER	0x0018		/* Auto LANStreamer */
#define	PCI_PRODUCT_IBM_GXT150P	0x001b		/* GXT-150P 2D Accelerator */
#define	PCI_PRODUCT_IBM_MCABRIDGE2	0x0020		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI Bridge */
#define	PCI_PRODUCT_IBM_SERVERAID	0x002e		/* ServeRAID */
#define	PCI_PRODUCT_IBM_OLYMPIC	0x003e		/* Token Ring */
#define	PCI_PRODUCT_IBM_MIAMI	0x0036		/* Miami/PCI */
#define	PCI_PRODUCT_IBM_82660	0x0037		/* 82660 PowerPC to PCI Bridge and Memory Controller */
#define	PCI_PRODUCT_IBM_MPIC	0x0046		/* MPIC */
#define	PCI_PRODUCT_IBM_TURBOWAYS25	0x0053		/* Turboways 25 ATM */
#define	PCI_PRODUCT_IBM_GXT800P	0x005e		/* GXT-800P */
#define	PCI_PRODUCT_IBM_405GP	0x0156		/* PPC 405GP PCI Bridge */
#define	PCI_PRODUCT_IBM_133PCIX	0x01a7		/* 133 PCI-X Bridge */
#define	PCI_PRODUCT_IBM_SERVERAID4	0x01bd		/* ServeRAID 4/5 */
#define	PCI_PRODUCT_IBM_MPIC2	0xffff		/* MPIC-II */

/* IC Ensemble / VIA Technologies products */
#define	PCI_PRODUCT_ICENSEMBLE_ICE1712	0x1712		/* Envy24 Multichannel Audio Controller */
#define	PCI_PRODUCT_ICENSEMBLE_VT1720	0x1724		/* Envy24PT/HT Multi-Channel Audio Controller */

/* Conexant (iCompression, GlobeSpan) products */
#define	PCI_PRODUCT_ICOMPRESSION_ITVC15	0x0803		/* iTVC15 MPEG2 codec */

/* IDT products */
#define	PCI_PRODUCT_IDT_77201	0x0001		/* 77201/77211 ATM (\"NICStAR\") */
#define	PCI_PRODUCT_IDT_RC32334	0x0204		/* RC32334 System Controller */
#define	PCI_PRODUCT_IDT_RC32332	0x0205		/* RC32332 System Controller */

/* Industrial Computer Source */
#define	PCI_PRODUCT_INDCOMPSRC_WDT50x	0x22c0		/* PCI-WDT50x Watchdog Timer */

/* Initio products */
#define	PCI_PRODUCT_INITIO_I920	0x0002		/* INIC-920 SCSI */
#define	PCI_PRODUCT_INITIO_I850	0x0850		/* INIC-850 SCSI */
#define	PCI_PRODUCT_INITIO_I1060	0x1060		/* INIC-1060 SCSI */
#define	PCI_PRODUCT_INITIO_I940	0x9400		/* INIC-940 SCSI */
#define	PCI_PRODUCT_INITIO_I935	0x9401		/* INIC-935 SCSI */
#define	PCI_PRODUCT_INITIO_I950	0x9500		/* INIC-950 SCSI */

/* Integraphics Systems products */
#define	PCI_PRODUCT_INTEGRAPHICS_IGA1680	0x1680		/* IGA 1680 */
#define	PCI_PRODUCT_INTEGRAPHICS_IGA1682	0x1682		/* IGA 1682 */
#define	PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2000	0x2000		/* CyberPro 2000 */
#define	PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2010	0x2010		/* CyberPro 2010 */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */
#define	PCI_PRODUCT_IMS_TT128M	0x9128		/* TwinTurbo 128M */

/* Intel products */
#define	PCI_PRODUCT_INTEL_80312	0x030d		/* 80312 I/O Companion Chip */
#define	PCI_PRODUCT_INTEL_80321	0x0319		/* 80321 I/O Processor */
#define	PCI_PRODUCT_INTEL_6700PXH_PCIE0	0x0329		/* 6700PXH PCI Express-to-PCI Bridge #0 */
#define	PCI_PRODUCT_INTEL_6700PXH_PCIE1	0x032a		/* 6700PXH PCI Express-to-PCI Bridge #1 */
#define	PCI_PRODUCT_INTEL_SRCZCRX	0x0407		/* RAID controller */
#define	PCI_PRODUCT_INTEL_SRCU42E	0x0408		/* SCSI RAID controller */
#define	PCI_PRODUCT_INTEL_SRCS28X	0x0409		/* SATA RAID controller */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB/SB PCI-EISA Bridge */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424ZX Cache and DRAM controller */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378ZB System I/O */
#define	PCI_PRODUCT_INTEL_82426EX	0x0486		/* 82426EX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX/NX PCI, Cache and Memory Controller (PCMC) */
#define	PCI_PRODUCT_INTEL_GDT_RAID1	0x0600		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_GDT_RAID2	0x061f		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_80960RM	0x0962		/* i960 RM PCI-PCI */
#define	PCI_PRODUCT_INTEL_80960RN	0x0964		/* i960 RN PCI-PCI */
#define	PCI_PRODUCT_INTEL_82542	0x1000		/* i82542 Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82543GC_FIBER	0x1001		/* i82453GC 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_MODEM56	0x1002		/* 56k Modem */
#define	PCI_PRODUCT_INTEL_82543GC_COPPER	0x1004		/* i82543GC 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544EI_COPPER	0x1008		/* i82544EI 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544EI_FIBER	0x1009		/* i82544EI 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82544GC_COPPER	0x100c		/* i82544GC 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544GC_LOM	0x100d		/* i82544GC (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EM	0x100e		/* i82540EM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545EM_COPPER	0x100f		/* i82545EM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_COPPER	0x1010		/* i82546EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545EM_FIBER	0x1011		/* i82545EM 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_FIBER	0x1012		/* i82546EB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82541EI	0x1013		/* i82541EI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EM_LOM	0x1015		/* i82540EM (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP_LOM	0x1016		/* i82540EP (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP	0x1017		/* i82540EP Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541EI_MOBILE	0x1018		/* i82541EI Mobile Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82547EI	0x1019		/* i82547EI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_QUAD	0x101d		/* i82546EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP_LP	0x101e		/* i82540EP Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_COPPER	0x1026		/* i82545GM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_FIBER	0x1027		/* i82545GM 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_SERDES	0x1028		/* i82545GM Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_IN_BUSINESS	0x1030		/* InBusiness Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_0	0x1031		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_1	0x1032		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_0	0x1033		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_1	0x1034		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_0	0x1035		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_1	0x1036		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_2	0x1037		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_2	0x1038		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_2	0x1039		/* PRO/100 VE Network Controller with 82562ET/EZ PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_3	0x103a		/* PRO/100 VE Network Controller with 82562ET/EZ (CNR) PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_3	0x103b		/* PRO/100 VM Network Controller with 82562EM/EX PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_4	0x103c		/* PRO/100 VM Network Controller with 82562EM/EX (CNR) PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_4	0x103d		/* PRO/100 VE (MOB) Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_5	0x103e		/* PRO/100 VM (MOB) Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_WL_2100	0x1043		/* PRO/Wireless LAN 2100 3B Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_82597EX	0x1048		/* PRO/10GbE LR Server Adapter */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_6	0x1050		/* PRO/100 VM Network Controller with 82562ET/EZ PHY */
#define	PCI_PRODUCT_INTEL_82801EB_LAN	0x1051		/* 82801EB/ER 10/100 Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_M	0x1059		/* PRO/100 M Network Controller */
#define	PCI_PRODUCT_INTEL_82571EB_COPPER	0x105e		/* i82571EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_FIBER	0x105f		/* i82571EB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_SERDES	0x1060		/* i82571EB Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82801FB_LAN	0x1064		/* 82801FB 10/100 Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_5	0x1068		/* PRO/100 VE (LOM) Network Controller */
#define	PCI_PRODUCT_INTEL_82547GI	0x1075		/* i82547GI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541GI	0x1076		/* i82541GI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541GI_MOBILE	0x1077		/* i82541GI Mobile Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541ER	0x1078		/* i82541ER Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_COPPER	0x1079		/* i82546GB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_FIBER	0x107a		/* i82546GB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_SERDES	0x107b		/* i82546GB Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82541PI	0x107c		/* i82541PI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_COPPER	0x107d		/* i82572EI 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_FIBER	0x107e		/* i82572EI 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_SERDES	0x107f		/* i82572EI Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82573E	0x108b		/* i82573E Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82573E_IAMT	0x108c		/* i82573E Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82573L	0x109a		/* i82573L Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82815_DC100_HUB	0x1100		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_DC100_AGP	0x1101		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_DC100_GRAPH	0x1102		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_HUB	0x1110		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_GRAPH	0x1112		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_HUB	0x1120		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_AGP	0x1121		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_HUB	0x1130		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_FULL_AGP	0x1131		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_GRAPH	0x1132		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82806AA	0x1161		/* 82806AA PCI64 Hub Advanced Programmable Interrupt Controller */
#define	PCI_PRODUCT_INTEL_ADI_BECC	0x1162		/* ADI i80200 Big Endian Companion Chip */
#define	PCI_PRODUCT_INTEL_IXP1200	0x1200		/* IXP1200 Network Processor */
#define	PCI_PRODUCT_INTEL_82559ER	0x1209		/* 82559ER Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82092AA	0x1222		/* 82092AA IDE controller */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82452_PB	0x1225		/* 82452KX/GX Orion Extended Express Processor to PCI Bridge */
#define	PCI_PRODUCT_INTEL_82596	0x1226		/* 82596 LAN Controller */
#define	PCI_PRODUCT_INTEL_EEPRO100	0x1227		/* EE Pro 100 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_EEPRO100S	0x1228		/* EE Pro 100 Smart 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_82557	0x1229		/* 82557 Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82437FX	0x122d		/* 82437FX (TSC) System Controller */
#define	PCI_PRODUCT_INTEL_82371FB_ISA	0x122e		/* 82371FB (PIIX) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371FB_IDE	0x1230		/* 82371FB (PIIX) IDE controller */
#define	PCI_PRODUCT_INTEL_82371MX	0x1234		/* 82371MX (MPIIX) Mobile PCI I/O IDE Xcelerator */
#define	PCI_PRODUCT_INTEL_82437MX	0x1235		/* 82437MX (MTSC) Mobile System Controller */
#define	PCI_PRODUCT_INTEL_82441FX	0x1237		/* 82441FX (PMC) PCI and Memory Controller */
#define	PCI_PRODUCT_INTEL_82380AB	0x123c		/* 82380AB (MISA) Mobile PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82380FB	0x124b		/* 82380FB (MPCI2) Mobile PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82439HX	0x1250		/* 82439HX (TXC) System Controller */
#define	PCI_PRODUCT_INTEL_82870P2_PPB	0x1460		/* 82870P2 P64H2 PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82870P2_IOxAPIC	0x1461		/* 82870P2 P64H2 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82870P2_HPLUG	0x1462		/* 82870P2 P64H2 Hot Plug Controller */
#define	PCI_PRODUCT_INTEL_80960_RP	0x1960		/* ROB-in i960RP Microprocessor */
#define	PCI_PRODUCT_INTEL_80960RM_2	0x1962		/* i960 RM PCI-PCI */
#define	PCI_PRODUCT_INTEL_82840_HB	0x1a21		/* 82840 Host */
#define	PCI_PRODUCT_INTEL_82840_AGP	0x1a23		/* 82840 AGP */
#define	PCI_PRODUCT_INTEL_82840_PCI	0x1a24		/* 82840 PCI */
#define	PCI_PRODUCT_INTEL_82845_HB	0x1a30		/* 82845 Host */
#define	PCI_PRODUCT_INTEL_82845_AGP	0x1a31		/* 82845 AGP */
#define	PCI_PRODUCT_INTEL_82801AA_LPC	0x2410		/* 82801AA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AA_IDE	0x2411		/* 82801AA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AA_USB	0x2412		/* 82801AA USB Controller */
#define	PCI_PRODUCT_INTEL_82801AA_SMB	0x2413		/* 82801AA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACA	0x2415		/* 82801AA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACM	0x2416		/* 82801AA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AA_HPB	0x2418		/* 82801AA Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_LPC	0x2420		/* 82801AB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_IDE	0x2421		/* 82801AB IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AB_USB	0x2422		/* 82801AB USB Controller */
#define	PCI_PRODUCT_INTEL_82801AB_SMB	0x2423		/* 82801AB SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACA	0x2425		/* 82801AB AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACM	0x2426		/* 82801AB AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AB_HPB	0x2428		/* 82801AB Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_LPC	0x2440		/* 82801BA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_USB1	0x2442		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_SMB	0x2443		/* 82801BA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801BA_USB2	0x2444		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACA	0x2445		/* 82801BA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACM	0x2446		/* 82801BA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801BAM_HPB	0x2448		/* 82801BAM Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_LAN	0x2449		/* 82801BA LAN Controller */
#define	PCI_PRODUCT_INTEL_82801BAM_IDE	0x244a		/* 82801BAM IDE Controller */
#define	PCI_PRODUCT_INTEL_82801BA_IDE	0x244b		/* 82801BA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801BAM_LPC	0x244c		/* 82801BAM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_HPB	0x244e		/* 82801BA Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801E_SMB	0x2453		/* 82801E SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801E_LAN_1	0x2459		/* 82801E LAN Controller */
#define	PCI_PRODUCT_INTEL_82801E_LAN_2	0x245d		/* 82801E LAN Controller */
#define	PCI_PRODUCT_INTEL_82801CA_LPC	0x2480		/* 82801CA LPC Interface */
#define	PCI_PRODUCT_INTEL_82801CA_USB_1	0x2482		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_SMB	0x2483		/* 82801CA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801CA_USB_2	0x2484		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_AC	0x2485		/* 82801CA AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801CA_MOD	0x2486		/* 82801CA AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801CA_USBC	0x2487		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_IDE_1	0x248A		/* 82801CA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801CA_IDE_2	0x248B		/* 82801CA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801CAM_LPC	0x248C		/* 82801CAM LPC Interface */
#define	PCI_PRODUCT_INTEL_82801DB_LPC	0x24C0		/* 82801DB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801DB_USB_1	0x24C2		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DB_SMB	0x24C3		/* 82801DB SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801DB_USB_2	0x24C4		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DB_AC	0x24C5		/* 82801DB AC97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801DB_MOD	0x24C6		/* 82801DB AC97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801DB_USB_3	0x24C7		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DBM_IDE	0x24CA		/* 82801DBM IDE Controller */
#define	PCI_PRODUCT_INTEL_82801DB_IDE	0x24CB		/* 82801DB IDE Controller (UltraATA/100) */
#define	PCI_PRODUCT_INTEL_82801DB_ISA	0x24CC		/* 82801DB ISA Bridge */
#define	PCI_PRODUCT_INTEL_82801DB_USBC	0x24CD		/* 82801DB USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_LPC	0x24D0		/* 82801EB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801EB_SATA	0x24D1		/* 82801EB Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_0	0x24D2		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_SMB	0x24D3		/* 82801EB/ER SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_1	0x24D4		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_AC	0x24D5		/* 82801EB/ER AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801EB_MOD	0x24D6		/* 82801EB/ER AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_2	0x24D7		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_IDE	0x24DB		/* 82801EB/ER IDE Controller */
#define	PCI_PRODUCT_INTEL_82801EB_EHCI	0x24DD		/* 82801EB/ER USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_3	0x24DE		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801ER_SATA	0x24DF		/* 82801ER Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82820_MCH	0x2501		/* 82820 MCH (Camino) */
#define	PCI_PRODUCT_INTEL_82820_AGP	0x250f		/* 82820 AGP */
#define	PCI_PRODUCT_INTEL_82850_HB	0x2530		/* 82850 Host */
#define	PCI_PRODUCT_INTEL_82860_HB	0x2531		/* 82860 Host */
#define	PCI_PRODUCT_INTEL_82850_AGP	0x2532		/* 82850/82860 AGP */
#define	PCI_PRODUCT_INTEL_82860_PCI1	0x2533		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI2	0x2534		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI3	0x2535		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI4	0x2536		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_E7500_HB	0x2540		/* E7500 MCH Host */
#define	PCI_PRODUCT_INTEL_E7500_DRAM	0x2541		/* E7500 MCH DRAM Controller */
#define	PCI_PRODUCT_INTEL_E7500_HI_B1	0x2543		/* E7500 MCH HI_B vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_B2	0x2544		/* E7500 MCH HI_B vppb 2 */
#define	PCI_PRODUCT_INTEL_E7500_HI_C1	0x2545		/* E7500 MCH HI_C vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_C2	0x2546		/* E7500 MCH HI_C vppb 2 */
#define	PCI_PRODUCT_INTEL_E7500_HI_D1	0x2547		/* E7500 MCH HI_D vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_D2	0x2548		/* E7500 MCH HI_D vppb 2 */
#define	PCI_PRODUCT_INTEL_E7501_HB	0x254c		/* E7501 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_HB	0x2550		/* E7505 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_RAS	0x2551		/* E7505 MCH RAS Controller */
#define	PCI_PRODUCT_INTEL_E7505_AGP	0x2552		/* E7505 MCH Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_E7505_HI_B1	0x2553		/* E7505 MCH HI_B PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E7505_HI_B2	0x2554		/* E7505 MCH HI_B PCI-PCI Error Reporting */
#define	PCI_PRODUCT_INTEL_82845G_DRAM	0x2560		/* 82845G/GL DRAM Controller / Host-Hub I/F Bridge */
#define	PCI_PRODUCT_INTEL_82845G_AGP	0x2561		/* 82845G/GL Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_82845G_IGD	0x2562		/* 82845G/GL Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82865_HB	0x2570		/* 82865 Host */
#define	PCI_PRODUCT_INTEL_82865_AGP	0x2571		/* 82865 AGP */
#define	PCI_PRODUCT_INTEL_82865_IGD	0x2572		/* 82865G Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82801EB_HPB	0x2573		/* 82801EB Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82875P_HB	0x2578		/* 82875P Host */
#define	PCI_PRODUCT_INTEL_82875P_AGP	0x2579		/* 82875P AGP */
#define	PCI_PRODUCT_INTEL_82875P_CSA	0x257b		/* 82875P PCI-CSA Bridge */
#define	PCI_PRODUCT_INTEL_82915G_HB	0x2580		/* 82915P/G/GL Host */
#define	PCI_PRODUCT_INTEL_82915G_EX	0x2581		/* 82915P/G/GL PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915G_IGD	0x2582		/* 82915G/GL Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82925X_HB	0x2584		/* 82925X Host */
#define	PCI_PRODUCT_INTEL_82925X_EX	0x2585		/* 82925X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_HB	0x2590		/* 82915PM/GM/GMS,82910GML Host Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_EX	0x2591		/* 82915PM/GM PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_IGD	0x2592		/* 82915GM/GMS Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82915PM_EX	0x2591		/* 82915PM/GM PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_IGD	0x2592		/* 82915GM/GMS,82910GML Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_6300ESB_LPC	0x25a1		/* 6300ESB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_6300ESB_IDE	0x25a2		/* 6300ESB IDE Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA	0x25a3		/* 6300ESB SATA Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_SMB	0x25a4		/* 6300ESB SMBus Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_ACA	0x25a6		/* 6300ESB AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_ACM	0x25a7		/* 6300ESB AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_0	0x25a9		/* 6300ESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_1	0x25aa		/* 6300ESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_WDT	0x25ab		/* 6300ESB Watchdog Timer */
#define	PCI_PRODUCT_INTEL_6300ESB_APIC	0x25ac		/* 6300ESB Advanced Interrupt Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_EHCI	0x25ad		/* 6300ESB USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_PCIX	0x25ae		/* 6300ESB PCI-X Bridge */
#define	PCI_PRODUCT_INTEL_6300ESB_RAID	0x25b0		/* 6300ESB SATA RAID Controller */
#define	PCI_PRODUCT_INTEL_82801FB_LPC	0x2640		/* 82801FB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801FBM_LPC	0x2641		/* 82801FBM ICH6M LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801FB_SATA	0x2651		/* 82801FB Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FR_SATA	0x2652		/* 82801FR Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FBM_SATA	0x2653		/* 82801FBM Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_0	0x2658		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_1	0x2659		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_2	0x265a		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_3	0x265b		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_EHCI	0x265c		/* 82801FB/FR USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_0	0x2660		/* 82801FB/FR PCI Express Port #0 */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_1	0x2662		/* 82801FB/FR PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_2	0x2664		/* 82801FB/FR PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801FB_HDA	0x2668		/* 82801FB/FR High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801FB_SMB	0x266a		/* 82801FB/FR SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801FB_ACM	0x266d		/* 82801FB/FR AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801FB_AC	0x266e		/* 82801FB/FR AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801FB_IDE	0x266f		/* 82801FB/FR IDE Controller */
#define	PCI_PRODUCT_INTEL_82945P_MCH	0x2770		/* 82945G/P Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82945P_EXP	0x2771		/* 82945G/P PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82955X_HB	0x2774		/* 82955X Host */
#define	PCI_PRODUCT_INTEL_82955X_EXP	0x2775		/* 82955X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915G_IGDC	0x2782		/* 82915G/GL IGD Companion */
#define	PCI_PRODUCT_INTEL_82915GM_IGDC	0x2792		/* 82915GM/GMS IGD Companion */
#define	PCI_PRODUCT_INTEL_82801G_LPC	0x27b8		/* 82801GB/GR LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801GBM_LPC	0x27b9		/* 82801GBM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801GHM_LPC	0x27bd		/* 82801GHM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801G_SATA	0x27c0		/* 82801GB/GR SATA Controller */
#define	PCI_PRODUCT_INTEL_82801G_SATA_AHCI	0x27c1		/* 82801GB/GR AHCI SATA Controller */
#define	PCI_PRODUCT_INTEL_82801G_SATA_RAID	0x27c3		/* 82801GB/GR RAID SATA Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_1	0x27c8		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_2	0x27c9		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_3	0x27ca		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_4	0x27cb		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_EHCI	0x27cc		/* 82801GB/GR USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_EXP_1	0x27d0		/* 82801GB/GR PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_2	0x27d2		/* 82801GB/GR PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_3	0x27d4		/* 82801GB/GR PCI Express Port #3 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_4	0x27d6		/* 82801GB/GR PCI Express Port #4 */
#define	PCI_PRODUCT_INTEL_82801G_HDA	0x27d8		/* 82801GB/GR High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801G_SMB	0x27da		/* 82801GB/GR SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801G_LAN	0x27dc		/* 82801GB/GR LAN Controller */
#define	PCI_PRODUCT_INTEL_82801G_ACM	0x27dd		/* 82801GB/GR AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801G_ACA	0x27de		/* 82801GB/GR AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801G_IDE	0x27df		/* 82801GB/GR IDE Controller */
#define	PCI_PRODUCT_INTEL_82801G_EXP_5	0x27e0		/* 82801GB/GR PCI Express Port #5 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_6	0x27e2		/* 82801GB/GR PCI Express Port #6 */
#define	PCI_PRODUCT_INTEL_31244	0x3200		/* 31244 Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82855PM_DDR	0x3340		/* 82855PM MCH Host Controller */
#define	PCI_PRODUCT_INTEL_82855PM_AGP	0x3341		/* 82855PM Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_82855PM_PM	0x3342		/* 82855PM Power Management Controller */
#define	PCI_PRODUCT_INTEL_82830MP_IO_1	0x3575		/* 82830MP CPU to I/O Bridge 1 */
#define	PCI_PRODUCT_INTEL_82830MP_AGP	0x3576		/* 82830MP CPU to AGP Bridge */
#define	PCI_PRODUCT_INTEL_82830MP_IV	0x3577		/* 82830MP Integrated Video */
#define	PCI_PRODUCT_INTEL_82830MP_IO_2	0x3578		/* 82830MP CPU to I/O Bridge 2 */
#define	PCI_PRODUCT_INTEL_82855GM_MCH	0x3580		/* 82855GM Host-Hub Controller */
#define	PCI_PRODUCT_INTEL_82855GM_IGD	0x3582		/* 82855GM GMCH Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82855GM_MC	0x3584		/* 82855GM GMCH Memory Controller */
#define	PCI_PRODUCT_INTEL_82855GM_CP	0x3585		/* 82855GM GMCH Configuration Process */
#define	PCI_PRODUCT_INTEL_E7525_MCH	0x3590		/* E7525 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_E7525_MCHER	0x3591		/* E7525 Error Reporting Device */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_A	0x3595		/* E7525 PCI Express Port A */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_A1	0x3596		/* E7525 PCI Express Port A1 */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_B	0x3597		/* E7525 PCI Express Port B */
#define	PCI_PRODUCT_INTEL_PRO_WL_2200BG	0x4220		/* PRO/Wireless LAN 2200BG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2225BG	0x4221		/* PRO/Wireless LAN 2225BG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1	0x4223		/* PRO/Wireless LAN 2915ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2	0x4224		/* PRO/Wireless LAN 2915ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_80312_ATU	0x530d		/* 80310 ATU */
#define	PCI_PRODUCT_INTEL_82371SB_ISA	0x7000		/* 82371SB (PIIX3) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371SB_IDE	0x7010		/* 82371SB (PIIX3) IDE Interface */
#define	PCI_PRODUCT_INTEL_82371SB_USB	0x7020		/* 82371SB (PIIX3) USB Host Controller */
#define	PCI_PRODUCT_INTEL_82437VX	0x7030		/* 82437VX (TVX) System Controller */
#define	PCI_PRODUCT_INTEL_82439TX	0x7100		/* 82439TX (MTXC) System Controller */
#define	PCI_PRODUCT_INTEL_82371AB_ISA	0x7110		/* 82371AB (PIIX4) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371AB_IDE	0x7111		/* 82371AB (PIIX4) IDE controller */
#define	PCI_PRODUCT_INTEL_82371AB_USB	0x7112		/* 82371AB (PIIX4) USB Host Controller */
#define	PCI_PRODUCT_INTEL_82371AB_PMC	0x7113		/* 82371AB (PIIX4) Power Management Controller */
#define	PCI_PRODUCT_INTEL_82810_MCH	0x7120		/* 82810 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_GC	0x7121		/* 82810 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810_DC100_MCH	0x7122		/* 82810-DC100 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_DC100_GC	0x7123		/* 82810-DC100 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810E_MCH	0x7124		/* 82810E Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810E_GC	0x7125		/* 82810E Graphics Controller */
#define	PCI_PRODUCT_INTEL_82443LX	0x7180		/* 82443LX PCI AGP Controller */
#define	PCI_PRODUCT_INTEL_82443LX_AGP	0x7181		/* 82443LX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443BX	0x7190		/* 82443BX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82443BX_AGP	0x7191		/* 82443BX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443BX_NOAGP	0x7192		/* 82443BX Host Bridge/Controller (AGP disabled) */
#define	PCI_PRODUCT_INTEL_82440MX	0x7194		/* 82443MX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ACA	0x7195		/* 82443MX AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ISA	0x7198		/* 82443MX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82440MX_IDE	0x7199		/* 82443MX IDE Controller */
#define	PCI_PRODUCT_INTEL_82440MX_USB	0x719a		/* 82443MX USB Host Controller */
#define	PCI_PRODUCT_INTEL_82440MX_PMC	0x719b		/* 82443MX Power Management Controller */
#define	PCI_PRODUCT_INTEL_82443GX	0x71a0		/* 82443GX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82443GX_AGP	0x71a1		/* 82443GX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443GX_NOAGP	0x71a2		/* 82443GX Host Bridge/Controller (AGP disabled) */
#define	PCI_PRODUCT_INTEL_I740	0x7800		/* i740 Graphics Accelerator */
#define	PCI_PRODUCT_INTEL_PCI450_PB	0x84c4		/* 82454KX/GX PCI Bridge (PB) */
#define	PCI_PRODUCT_INTEL_PCI450_MC	0x84c5		/* 82451KX/GX Memory Controller (MC) */
#define	PCI_PRODUCT_INTEL_82451NX_MIOC	0x84ca		/* 82451NX Memory & I/O Controller (MIOC) */
#define	PCI_PRODUCT_INTEL_82451NX_PXB	0x84cb		/* 82451NX PCI Expander Bridge (PXB) */
#define	PCI_PRODUCT_INTEL_21152	0xb152		/* S21152BB PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_21154	0xb154		/* S21152BA,S21154AE/BE PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_21555	0xb555		/* 21555 Non-Transparent PCI-PCI Bridge */

/* Intergraph products */
#define	PCI_PRODUCT_INTERGRAPH_4D50T	0x00e4		/* Powerstorm 4D50T */
#define	PCI_PRODUCT_INTERGRAPH_4D60T	0x00e3		/* Powerstorm 4D60T */

/* Intersil products */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_WLAN	0x3873		/* PRISM2.5 Mini-PCI WLAN */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_3877	0x3877		/* PRISM Indigo Mini-PCI WLAN */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_3890	0x3890		/* PRISM Duette Mini-PCI WLAN */

/* Invertex */
#define	PCI_PRODUCT_INVERTEX_AEON	0x0005		/* AEON */

/* IO Data products */
#define	PCI_PRODUCT_IODATA_CBIDE2	0x0003		/* CBIDE2 IDE controller */
#define	PCI_PRODUCT_IODATA_CBSCII	0x0005		/* CBSCII NinjaSCSI-32Bi SCSI */

/* ITE products */
#define	PCI_PRODUCT_ITE_IT8152	0x8152		/* IT8152 Host Bridge */
#define	PCI_PRODUCT_ITE_IT8212	0x8212		/* IT8212 IDE controller */

/* I. T. T. products */
#define	PCI_PRODUCT_ITT_AGX016	0x0001		/* AGX016 */
#define	PCI_PRODUCT_ITT_ITT3204	0x0002		/* ITT3204 MPEG Decoder */

/* JNI products */
#define	PCI_PRODUCT_JNI_JNIC1460	0x1460		/* JNIC-1460 Fibre-Channel adapter */
#define	PCI_PRODUCT_JNI_JNIC1560	0x1560		/* JNIC-1560 dual Fibre-Channel adapter */
#define	PCI_PRODUCT_JNI_FCI1063	0x4643		/* FCI-1063 Fibre-Channel adapter */
#define	PCI_PRODUCT_JNI_FCX26562	0x6562		/* FCX2-6562 dual Fibre-Channel adapter */
#define	PCI_PRODUCT_JNI_FCX6562	0x656a		/* FCX-6562 Fibre-Channel adapter */

/* KTI products - XXX better descriptions */
#define	PCI_PRODUCT_KTI_NE2KETHER	0x3000		/* Ethernet */

/* LAN Media */
#define	PCI_PRODUCT_LMC_HSSI	0x0003		/* HSSI Interface */
#define	PCI_PRODUCT_LMC_DS3	0x0004		/* DS3 Interface */
#define	PCI_PRODUCT_LMC_SSI	0x0005		/* SSI */
#define	PCI_PRODUCT_LMC_DS1	0x0006		/* DS1 */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */

/* Level One products */
#define	PCI_PRODUCT_LEVELONE_LXT1001	0x0001		/* LXT-1001 10/100/1000 Ethernet */

/* Linear Systems / CompuModules */
#define	PCI_PRODUCT_LINEARSYS_DVB_TX	0x7629		/* DVB Transmitter */
#define	PCI_PRODUCT_LINEARSYS_DVB_RX	0x7630		/* DVB Receiver */

/* Linksys products */
#define	PCI_PRODUCT_LINKSYS_EG1032	0x1032		/* EG1032 v2 Instant Gigabit Network Adapter */
#define	PCI_PRODUCT_LINKSYS_EG1064	0x1064		/* EG1064 v2 Instant Gigabit Network Adapter */
#define	PCI_PRODUCT_LINKSYS_PCMPC200	0xab08		/* PCMPC200 */
#define	PCI_PRODUCT_LINKSYS_PCM200	0xab09		/* PCM200 */
#define	PCI_PRODUCT_LINKSYS2_IPN2220	0x2220		/* IPN 2220 Wireless LAN Adapter (rev 01) */

/* Lite-On products */
#define	PCI_PRODUCT_LITEON_82C168	0x0002		/* 82C168/82C169 (PNIC) 10/100 Ethernet */
#define	PCI_PRODUCT_LITEON_82C115	0xc115		/* 82C115 (PNIC II) 10/100 Ethernet */

/* Lucent Technologies products */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0440	0x0440		/* K56flex DSVD LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0441	0x0441		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0442	0x0442		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0443	0x0443		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0444	0x0444		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0445	0x0445		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0446	0x0446		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0447	0x0447		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0448	0x0448		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0449	0x0449		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044a	0x044a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044b	0x044b		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044c	0x044c		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044d	0x044d		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044e	0x044e		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0450	0x0450		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0451	0x0451		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0452	0x0452		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0453	0x0453		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0454	0x0454		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0455	0x0455		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0456	0x0456		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0457	0x0457		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0458	0x0458		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0459	0x0459		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_045a	0x045a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_OR3LP26	0x5400		/* ORCA FPGA w/32-bit PCI ASIC core */
#define	PCI_PRODUCT_LUCENT_OR3TP12	0x5401		/* ORCA FPGA w/64-bit PCI ASIC core */
#define	PCI_PRODUCT_LUCENT_USBHC	0x5801		/* USB Host Controller */
#define	PCI_PRODUCT_LUCENT_EVDO	0x5802		/* Sierra Wireless AirCard 580 */
#define	PCI_PRODUCT_LUCENT_FW322_323	0x5811		/* FW322/323 IEEE 1394 Host Controller */

/* Macronix */
#define	PCI_PRODUCT_MACRONIX_MX98713	0x0512		/* MX98713 (PMAC) 10/100 Ethernet */
#define	PCI_PRODUCT_MACRONIX_MX987x5	0x0531		/* MX987x5 (PMAC) 10/100 Ethernet */

/* Madge Networks products */
#define	PCI_PRODUCT_MADGE_SMARTRN2	0x0002		/* Smart 16/4 PCI Ringnode Mk2 */
#define	PCI_PRODUCT_MADGE_COLLAGE25	0x1000		/* Collage 25 ATM adapter */
#define	PCI_PRODUCT_MADGE_COLLAGE155	0x1001		/* Collage 155 ATM adapter */

/* MAGMA products */
#define	PCI_PRODUCT_MAGMA_SERIAL16	0x0010		/* 16 DMA PCI-SLRS */
#define	PCI_PRODUCT_MAGMA_SERIAL4	0x0011		/* 4 DMA PCI-SLRS */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA PX2085 (\"Atlas\") */
#define	PCI_PRODUCT_MATROX_MILLENNIUM	0x0519		/* MGA Millennium 2064W (\"Storm\") */
#define	PCI_PRODUCT_MATROX_MYSTIQUE	0x051a		/* MGA Mystique 1064SG */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2	0x051b		/* MGA Millennium II 2164W */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2_AGP	0x051f		/* MGA Millennium II 2164WA-B AGP */
#define	PCI_PRODUCT_MATROX_G200_PCI	0x0520		/* MGA G200 PCI */
#define	PCI_PRODUCT_MATROX_G200_AGP	0x0521		/* MGA G200 AGP */
#define	PCI_PRODUCT_MATROX_G400_AGP	0x0525		/* MGA G400 AGP */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */
#define	PCI_PRODUCT_MATROX_G100_PCI	0x1000		/* MGA G100 PCI */
#define	PCI_PRODUCT_MATROX_G100_AGP	0x1001		/* MGA G100 AGP */
#define	PCI_PRODUCT_MATROX_G550_AGP	0x2527		/* MGA G550 AGP */

/* MediaQ products */
#define	PCI_PRODUCT_MEDIAQ_MQ200	0x0200		/* MQ200 */

/* Microsoft products */
#define	PCI_PRODUCT_MICROSOFT_MN120	0x0001		/* MN-120 10/100 Ethernet Notebook Adapter */

/* Middle Digital products */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_VGA	0x9050		/* Weasel Virtual VGA */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_SERIAL	0x9051		/* Weasel Serial Port */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_CONTROL	0x9052		/* Weasel Control */

/* Mitsubishi products */
#define	PCI_PRODUCT_MITSUBISHIELEC_TORNADO	0x0308		/* Tornado 3000 AGP */

/* Motorola products */
#define	PCI_PRODUCT_MOT_MPC105	0x0001		/* MPC105 \"Eagle\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC106	0x0002		/* MPC106 \"Grackle\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC8240	0x0003		/* MPC8240 \"Kahlua\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC107	0x0004		/* MPC107 \"Chaparral\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC8245	0x0006		/* MPC8245 \"Kahlua II\" Host Bridge */
#define	PCI_PRODUCT_MOT_RAVEN	0x4801		/* Raven Host Bridge & Multi-Processor Interrupt Controller */
#define	PCI_PRODUCT_MOT_FALCON	0x4802		/* Falcon ECC Memory Controller Chip Set */
#define	PCI_PRODUCT_MOT_HAWK	0x4803		/* Hawk System Memory Controller & PCI Host Bridge */

/* Moxa Technologies products */
#define	PCI_PRODUCT_MOXA_C104H	0x1040		/* C104H */
#define	PCI_PRODUCT_MOXA_CP104	0x1041		/* CP104UL */
#define	PCI_PRODUCT_MOXA_CP114	0x1141		/* CP114 */
#define	PCI_PRODUCT_MOXA_C168H	0x1680		/* C168H */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_RAID_V2	0x0001		/* DAC960 RAID (v2 interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V3	0x0002		/* DAC960 RAID (v3 interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V4	0x0010		/* DAC960 RAID (v4 interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V5	0x0020		/* DAC960 RAID (v5 interface) */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID_3000	0x0030		/* eXtremeRAID 3000 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID_2000	0x0040		/* eXtremeRAID 2000 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID	0x0050		/* AcceleRAID 352 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID_170	0x0052		/* AcceleRAID 170 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID_160	0x0054		/* AcceleRAID 160 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID1100	0xba55		/* eXtremeRAID 1100 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID	0xba56		/* eXtremeRAID 2000/3000 */

/* Myricom products */
#define	PCI_PRODUCT_MYRICOM_MYRINET	0x8043		/* Myrinet */

/* Myson-Century Technology products */
#define	PCI_PRODUCT_MYSON_MTD803	0x0803		/* MTD803 3-in-1 Fast Ethernet Controller */

/* National Datacomm products */
#define	PCI_PRODUCT_NDC_NCP130	0x0130		/* NCP130 Wireless NIC */
#define	PCI_PRODUCT_NDC_NCP130A2	0x0131		/* NCP130 rev A2 Wireless NIC */

/* NetVin products - XXX better descriptions */
#define	PCI_PRODUCT_NETVIN_5000	0x5000		/* 5000 Ethernet */

/* NetBoost (now Intel) products */
#define	PCI_PRODUCT_NETBOOST_POLICY	0x0000		/* Policy Accelerator */

/* Newbridge / Tundra products */
#define	PCI_PRODUCT_NEWBRIDGE_CA91CX42	0x0000		/* Universe VME bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L826A	0x0826		/* QSpan II PCI bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L8260	0x8260		/* PowerSpan PCI bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L8261	0x8261		/* PowerSpan II PCI bridge */

/* National Instruments products */
#define	PCI_PRODUCT_NATIONALINST_MXI3	0x2c30		/* MXI-3 PCI extender */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_DP83810	0x0001		/* DP83810 10/100 Ethernet */
#define	PCI_PRODUCT_NS_DP83815	0x0020		/* DP83815 10/100 Ethernet */
#define	PCI_PRODUCT_NS_DP83820	0x0022		/* DP83820 10/100/1000 Ethernet */
#define	PCI_PRODUCT_NS_NS87410	0xd001		/* NS87410 */
#define	PCI_PRODUCT_NS_SC1100_IDE	0x0502		/* SC1100 PCI IDE */
#define	PCI_PRODUCT_NS_SC1100_AUDIO	0x0503		/* SC1100 XpressAUDIO */
#define	PCI_PRODUCT_NS_SC1100_ISA	0x0510		/* SC1100 PCI-ISA bridge */
#define	PCI_PRODUCT_NS_SC1100_ACPI	0x0511		/* SC1100 SMI/ACPI */
#define	PCI_PRODUCT_NS_SC1100_XBUS	0x0515		/* SC1100 X-Bus */

/* Philips products */
#define	PCI_PRODUCT_PHILIPS_SAA7130HL	0x7130		/* SAA7130HL PCI video broadcast decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7133HL	0x7133		/* SAA7133HL PCI A/V broadcast decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7134HL	0x7134		/* SAA7134HL PCI A/V broadcast decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7135HL	0x7135		/* SAA7135HL PCI A/V broadcast decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7146AH	0x7146		/* SAA7146AH PCI Multimedia bridge */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_SYMBIOS_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_SYMBIOS_820	0x0002		/* 53c820 */
#define	PCI_PRODUCT_SYMBIOS_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_SYMBIOS_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_SYMBIOS_810AP	0x0005		/* 53c810AP */
#define	PCI_PRODUCT_SYMBIOS_860	0x0006		/* 53c860 */
#define	PCI_PRODUCT_SYMBIOS_1510D	0x000a		/* 53c1510D */
#define	PCI_PRODUCT_SYMBIOS_896	0x000b		/* 53c896 */
#define	PCI_PRODUCT_SYMBIOS_895	0x000c		/* 53c895 */
#define	PCI_PRODUCT_SYMBIOS_885	0x000d		/* 53c885 */
#define	PCI_PRODUCT_SYMBIOS_875	0x000f		/* 53c875/876 */
#define	PCI_PRODUCT_SYMBIOS_1510	0x0010		/* 53c1510 */
#define	PCI_PRODUCT_SYMBIOS_895A	0x0012		/* 53c895A */
#define	PCI_PRODUCT_SYMBIOS_875A	0x0013		/* 53c875A */
#define	PCI_PRODUCT_SYMBIOS_1010	0x0020		/* 53c1010 */
#define	PCI_PRODUCT_SYMBIOS_1010_2	0x0021		/* 53c1010 (66MHz) */
#define	PCI_PRODUCT_SYMBIOS_1030	0x0030		/* 53c1030 */
#define	PCI_PRODUCT_SYMBIOS_1030R	0x1030		/* 53c1030R */
#define	PCI_PRODUCT_SYMBIOS_875J	0x008f		/* 53c875J */
#define	PCI_PRODUCT_SYMBIOS_FC909	0x0620		/* FC909 */
#define	PCI_PRODUCT_SYMBIOS_FC909A	0x0621		/* FC909A */
#define	PCI_PRODUCT_SYMBIOS_FC929	0x0622		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC929_1	0x0623		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC919	0x0624		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC919_1	0x0625		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC929X	0x0626		/* FC929X */
#define	PCI_PRODUCT_SYMBIOS_FC919X	0x0628		/* FC919X */
#define	PCI_PRODUCT_SYMBIOS_PERC_4SC	0x1960		/* PERC 4/SC */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320X	0x0407		/* LSI Megaraid SCSI 320-X */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320E	0x0408		/* LSI Megaraid SCSI 320-E */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_300X	0x0409		/* LSI Megaraid SATA (300-6X/300-8X) */

/* Packet Engines products */
#define	PCI_PRODUCT_SYMBIOS_PE_GNIC	0x0702		/* Packet Engines G-NIC Ethernet */

/* NEC products */
#define	PCI_PRODUCT_NEC_USB	0x0035		/* USB Host Controller */
#define	PCI_PRODUCT_NEC_VRC4173_CARDU	0x003e		/* VRC4173 PC-Card Unit */
#define	PCI_PRODUCT_NEC_POWERVR2	0x0046		/* PowerVR PCX2 */
#define	PCI_PRODUCT_NEC_PD72872	0x0063		/* uPD72872 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_VRC4173_BCU	0x00a5		/* VRC4173 Bus Control Unit */
#define	PCI_PRODUCT_NEC_VRC4173_AC97U	0x00a6		/* VRC4173 AC97 Unit */
#define	PCI_PRODUCT_NEC_PD72870	0x00cd		/* uPD72870 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_PD72871	0x00ce		/* uPD72871 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_PD720100A	0x00e0		/* USB Host Controller */
#define	PCI_PRODUCT_NEC_VA26D	0x803c		/* Versa Pro LX VA26D */
#define	PCI_PRODUCT_NEC_VERSALX	0x8058		/* Versa LX */

/* Neomagic products */
#define	PCI_PRODUCT_NEOMAGIC_NMMG2070	0x0001		/* MagicGraph NM2070 */
#define	PCI_PRODUCT_NEOMAGIC_NMMG128V	0x0002		/* MagicGraph 128V */
#define	PCI_PRODUCT_NEOMAGIC_NMMG128ZV	0x0003		/* MagicGraph 128ZV */
#define	PCI_PRODUCT_NEOMAGIC_NMMG2160	0x0004		/* MagicGraph 128XD */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_VGA	0x0005		/* MagicMedia 256AV VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_VGA	0x0006		/* MagicMedia 256ZX VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256XLP_AU	0x0016		/* MagicMedia 256XL+ Audio */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_AU	0x8005		/* MagicMedia 256AV Audio */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_AU	0x8006		/* MagicMedia 256ZX Audio */

/* Netgear products */
#define	PCI_PRODUCT_NETGEAR_GA620	0x620a		/* GA620 1000baseSX Ethernet */
#define	PCI_PRODUCT_NETGEAR_GA620T	0x630a		/* GA620 1000baseT Ethernet */
#define	PCI_PRODUCT_NETGEAR_MA301	0x4100		/* MA301 PCI IEEE 802.11b */

/* Netmos products */
#define	PCI_PRODUCT_NETMOS_NM9805	0x9805		/* 1284 Printer port */
#define	PCI_PRODUCT_NETMOS_NM9815	0x9815		/* Dual 1284 Printer port */
#define	PCI_PRODUCT_NETMOS_NM9835	0x9835		/* Dual UART and 1284 Printer port */
#define	PCI_PRODUCT_NETMOS_NM9845	0x9845		/* Quad UART and 1284 Printer port */

/* Network Security Technologies */
#define	PCI_PRODUCT_NETSEC_7751	0x7751		/* 7751 */

/* NexGen products */
#define	PCI_PRODUCT_NEXGEN_NX82C501	0x4e78		/* NX82C501 Host-PCI Bridge */

/* NKK products */
#define	PCI_PRODUCT_NKK_NDR4600	0xa001		/* NDR4600 Host-PCI Bridge */

/* Nortel products */
#define	PCI_PRODUCT_NORTEL_BAYSTACK_21	0x1211		/* Baystack 21 (Accton MPX EN5038) */

/* Number Nine products */
#define	PCI_PRODUCT_NUMBER9_I128	0x2309		/* Imagine-128 */
#define	PCI_PRODUCT_NUMBER9_I128_2	0x2339		/* Imagine-128 II */

/* Nvidia products */
#define	PCI_PRODUCT_NVIDIA_RIVATNT	0x0020		/* RIVA TNT */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2	0x0028		/* RIVA TNT2 */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2U	0x0029		/* RIVA TNT2 Ultra */
#define	PCI_PRODUCT_NVIDIA_VANTA	0x002c		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2M64	0x002d		/* RIVA TNT2 Model 64 */
#define	PCI_PRODUCT_NVIDIA_MCP04_IDE	0x0035		/* MCP04 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA	0x0036		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN1	0x0037		/* MCP04 Ethernet */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN2	0x0038		/* MCP04 Ethernet */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA2	0x003e		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800U	0x0040		/* GeForce 6800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800	0x0041		/* GeForce 6800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800LE	0x0042		/* GeForce 6800LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800GT	0x0045		/* GeForce 6800 GT */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCIB	0x0051		/* nForce4 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SMBUS	0x0052		/* nForce4 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ATA133	0x0053		/* nForce4 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA1	0x0054		/* nForce4 Serial ATA 1 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA2	0x0055		/* nForce4 Serial ATA 2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_LAN1	0x0056		/* nForce4 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_LAN2	0x0057		/* nForce4 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_AC	0x0059		/* nForce4 AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB	0x005a		/* nForce4 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB2	0x005b		/* nForce4 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCI	0x005c		/* nForce4 PCI Host Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCIE	0x005d		/* nForce4 PCIe Host Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_MEM	0x005e		/* nForce4 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PCIB	0x0060		/* nForce2 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_SMBUS	0x0064		/* nForce2 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ATA133	0x0065		/* nForce2 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_LAN	0x0066		/* nForce2 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_OHCI	0x0067		/* nForce2 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_EHCI	0x0068		/* nForce2 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MCPT_AC	0x006a		/* nForce2 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MCPT_AP	0x006b		/* nForce2 MCP-T Audio Processing Unit */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB	0x006c		/* nForce2 PCI-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_FW	0x006e		/* nForce2 Firewire Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PCIB	0x0080		/* nForce2 Ultra 400 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SMBUS	0x0084		/* nForce2 Ultra 400 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ATA133	0x0085		/* nForce2 Ultra 400 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN1	0x0086		/* nForce2 Ultra 400 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_OHCI	0x0087		/* nForce2 Ultra 400 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_EHCI	0x0088		/* nForce2 Ultra 400 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_MCPT_AC	0x008a		/* nForce2 Ultra 400 AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PPB	0x008b		/* nForce2 Ultra 400 PCI-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN2	0x008c		/* nForce2 Ultra 400 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SATA	0x008e		/* nForce2 Ultra 400 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCHB	0x00d1		/* nForce3 Host-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCIB	0x00d0		/* nForce3 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_SMBUS	0x00d4		/* nForce3 SMBus controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_ATA133	0x00d5		/* nForce3 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN1	0x00d6		/* nForce3 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_OHCI	0x00d7		/* nForce3 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_EHCI	0x00d8		/* nForce3 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_MCPT_AC	0x00da		/* nForce3 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB	0x00dd		/* nForce3 PCI-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN4	0x00df		/* nForce3 ethernet #4 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PCIB	0x00e0		/* nForce3 250 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PCHB	0x00e1		/* nForce3 250 Host-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_AGP	0x00e2		/* nForce3 250 AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SATA	0x00e3		/* nForce3 250 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SMBUS	0x00e4		/* nForce3 250 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_ATA133	0x00e5		/* nForce3 250 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_LAN	0x00e6		/* nForce3 250 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_OHCI	0x00e7		/* nForce3 250 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_EHCI	0x00e8		/* nForce3 250 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_MCPT_AC	0x00ea		/* nForce3 250 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PPB	0x00ed		/* nForce3 250 PCI-PCI bridge */
#define	PCI_PRODUCT_NVIDIA_ALADDINTNT2	0x00a0		/* Aladdin TNT2 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_280NVS2	0x00fd		/* Quadro4 280 NVS */
#define	PCI_PRODUCT_NVIDIA_QUADROFX3400SLI	0x00fd		/* Quadro FX 3400 SLI */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256	0x0100		/* GeForce 256 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEDDR	0x0101		/* GeForce DDR */
#define	PCI_PRODUCT_NVIDIA_QUADRO	0x0103		/* Quadro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX	0x0110		/* GeForce2 MX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX200	0x0111		/* GeForce2 MX 100/200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GO	0x0112		/* GeForce2 Go */
#define	PCI_PRODUCT_NVIDIA_QUADRO2_MXR	0x0113		/* Quadro2 MXR/EX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2	0x0150		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2DDR	0x0151		/* GeForce2 GTS (DDR) */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2BR	0x0152		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_QUADRO2	0x0153		/* Quadro2 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200	0x0161		/* GeForce 6200TC */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX460	0x0170		/* GeForce4 MX 460 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX440	0x0171		/* GeForce4 MX 440 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX420	0x0172		/* GeForce4 MX 420 */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_SE	0x0173		/* GeForce4 MX 440 SE */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_500XGL	0x0178		/* Quadro4 500XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_200NVS	0x017a		/* Quadro4 200/400NVS */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_8X	0x0181		/* GeForce4 MX 440 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_SE_8X	0x0182		/* GeForce4 MX 440 SE (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_MX420_8X	0x0183		/* GeForce4 MX 420 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_580XGL	0x0188		/* Quadro4 580 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_280NVS	0x018a		/* Quadro4 280 NVS */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_380XGL	0x018b		/* Quadro4 380 XGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2_IGP	0x01a0		/* GeForce2 Integrated GPU */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MCP_AC	0x01b1		/* nForce MCP AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_ATA100	0x01bc		/* nForce ATA100 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE_USB	0x01c2		/* nForce USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE_LAN	0x01c3		/* nForce Ethetnet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PCHB	0x01e0		/* nForce2 Host-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB2	0x01e8		/* nForce2 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM1	0x01eb		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM2	0x01ec		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM3	0x01ed		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM4	0x01ee		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM5	0x01ef		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_GF4_MX_IGP	0x01f0		/* GeForce4 MX Integrated GPU */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3	0x0200		/* GeForce3 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3_TI200	0x0201		/* GeForce3 Ti 200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3_TI500	0x0202		/* GeForce3 Ti 500 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_DCC	0x0203		/* Quadro DCC */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4600	0x0250		/* GeForce4 Ti 4600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4400	0x0251		/* GeForce4 Ti 4400 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4200	0x0253		/* GeForce4 Ti 4200 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_900XGL	0x0258		/* Quadro4 900XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_750XGL	0x0259		/* Quadro4 750XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_700XGL	0x025b		/* Quadro4 700XGL */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_PCIB	0x0260		/* nForce430 PCI-ISA bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SMBUS	0x0264		/* nForce430 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_ATA133	0x0265		/* nForce430 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SATA1	0x0266		/* nForce430 Serial SATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SATA2	0x0267		/* nForce430 Serial SATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_LAN1	0x0268		/* nForce430 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_LAN2	0x0269		/* nForce430 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_AC	0x026c		/* nForce430 AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_OHCI	0x026d		/* nForce430 USB Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_EHCI	0x026e		/* nForce430 USB2 Controller */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4800	0x0280		/* GeForce4 Ti 4800 */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4200_8X	0x0281		/* GeForce4 Ti 4200 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4800_SE	0x0282		/* GeForce4 Ti 4800 SE */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4200_GO	0x0286		/* GeForce4 Ti 4200 Go AGP 8x */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_980_XGL	0x0288		/* Quadro4 980 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_780_XGL	0x0289		/* Quadro4 780 XGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_FX5800U	0x0301		/* GeForce FX 5800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_FX5800	0x0302		/* GeForce FX 5800 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_2000	0x0308		/* Quadro FX 2000 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_1000	0x0309		/* Quadro FX 1000 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600U	0x0311		/* GeForce FX 5600 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600	0x0312		/* GeForce FX 5600 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600_SE	0x0314		/* GeForce FX 5600 SE */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200U	0x0321		/* GeForce FX 5200 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200	0x0322		/* GeForce FX 5200 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200SE	0x0323		/* GeForce FX 5200SE */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_500	0x032B		/* Quadro FX 500 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900U	0x0330		/* GeForce FX 5900 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900	0x0331		/* GeForce FX 5900 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900XT	0x0332		/* GeForce FX 5900XT */
#define	PCI_PRODUCT_NVIDIA_GF_FX5950U	0x0333		/* GeForce FX 5950 Ultra */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_3000	0x0338		/* Quadro FX 3000 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5700_LE	0x0343		/* GeForce FX 5700 LE */
#define	PCI_PRODUCT_NVIDIA_MCP55_SMB	0x0368		/* MCP55 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP55_IDE	0x036e		/* MCP55 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP55_HDA	0x0371		/* HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN1	0x0372		/* MCP55 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN2	0x0373		/* MCP55 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA	0x037e		/* MCP55 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA2	0x037f		/* MCP55 SATA */

/* Nvidia & SGS-Thomson Microelectronics */
#define	PCI_PRODUCT_NVIDIA_SGS_RIVA128	0x0018		/* Riva 128 */

/* Oak Technologies products */
#define	PCI_PRODUCT_OAKTECH_OTI1007	0x0107		/* OTI107 */

/* Olicom products */
#define	PCI_PRODUCT_OLICOM_OC2183	0x0013		/* OC-2183/2185 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2325	0x0012		/* OC-2325 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2326	0x0014		/* OC-2326 10/100-TX Ethernet */

/* Opti products */
#define	PCI_PRODUCT_OPTI_82C557	0xc557		/* 82C557 */
#define	PCI_PRODUCT_OPTI_82C558	0xc558		/* 82C558 */
#define	PCI_PRODUCT_OPTI_82C568	0xc568		/* 82C568 */
#define	PCI_PRODUCT_OPTI_82D568	0xd568		/* 82D568 */
#define	PCI_PRODUCT_OPTI_82C621	0xc621		/* 82C621 */
#define	PCI_PRODUCT_OPTI_82C822	0xc822		/* 82C822 */
#define	PCI_PRODUCT_OPTI_82C861	0xc861		/* 82C861 */
#define	PCI_PRODUCT_OPTI_82C700	0xc700		/* 82C700 */
#define	PCI_PRODUCT_OPTI_82C701	0xc701		/* 82C701 */

/* Packet Engines products */
#define	PCI_PRODUCT_PACKETENGINES_GNICII	0x0911		/* G-NIC II Ethernet */

/* pcHDTV products */
#define	PCI_PRODUCT_PCHDTV_HD2000	0x2000		/* HD-2000 HDTV video capture */

/* PC Tech products */
#define	PCI_PRODUCT_PCTECH_RZ1000	0x1000		/* RZ1000 */

/* Peak System Technik products */
#define	PCI_PRODUCT_PEAK_PCAN	0x0001		/* PCAN CAN controller */

/* Planex products */
#define	PCI_PRODUCT_PLANEX_FNW_3603_TX	0xab06		/* FNW-3603-TX 10/100 Ethernet */
#define	PCI_PRODUCT_PLANEX_FNW_3800_TX	0xab07		/* FNW-3800-TX 10/100 Ethernet */

/* PLX Technology products */
#define	PCI_PRODUCT_PLX_9054	0x9054		/* 9054 I/O Accelerator */
#define	PCI_PRODUCT_PLX_9060ES	0x906e		/* 9060ES PCI bus controller */
#define	PCI_PRODUCT_PLX_9656	0x9656		/* 9656 I/O Accelerator */

/* Powerhouse Systems products */
#define	PCI_PRODUCT_POWERHOUSE_POWERTOP	0x6037		/* PowerTop PowerPC system controller */
#define	PCI_PRODUCT_POWERHOUSE_POWERPRO	0x6073		/* PowerPro PowerPC system controller */

/* ProLAN products - XXX better descriptions */
#define	PCI_PRODUCT_PROLAN_NE2KETHER	0x1980		/* Ethernet */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_PDC20246	0x4d33		/* PDC20246 Ultra/33 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20262	0x4d38		/* PDC20262 Ultra/66 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20263	0x0d38		/* PDC20263 Ultra/66 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20265	0x0d30		/* PDC20265 Ultra/100 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20267	0x4d30		/* PDC20267 Ultra/100 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20268	0x4d68		/* PDC20268 Ultra/100 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20269	0x4d69		/* PDC20269 Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20270	0x6268		/* PDC20270 Ultra/100 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20271	0x6269		/* PDC20271 Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20275	0x1275		/* PDC20275 Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20276	0x5275		/* PDC20276 Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20277	0x7275		/* PDC20277 Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20318	0x3318		/* PDC20318 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20319	0x3319		/* PDC20319 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20371	0x3371		/* PDC20371 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20375	0x3375		/* PDC20375 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20376	0x3376		/* PDC20376 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20377	0x3377		/* PDC20377 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20378	0x3373		/* PDC20378 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20379	0x3372		/* PDC20379 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20617	0x6617		/* PDC20617 dual Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20618	0x6626		/* PDC20618 dual Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20619	0x6629		/* PDC20619 dual Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20620	0x6620		/* PDC20620 dual Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20621	0x6621		/* PDC20621 dual Ultra/133 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC40718	0x3d17		/* PDC40718 SATA/300 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC40719	0x3515		/* PDC40719 SATA/300 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20571	0x3571		/* PDC20571 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20575	0x3d75		/* PDC20575 SATA/150 IDE controller */
#define	PCI_PRODUCT_PROMISE_PDC20579	0x3574		/* PDC20579 SATA/150 IDE controller */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */
#define	PCI_PRODUCT_QLOGIC_ISP1022	0x1022		/* ISP1022 */
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080		/* ISP1080 */
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240		/* ISP1240 */
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100		/* ISP2100 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* QuickLogic products */
#define	PCI_PRODUCT_QUICKLOGIC_PCWATCHDOG	0x5030		/* PC Watchdog */

/* Rainbow Technologies products */
#define	PCI_PRODUCT_RAINBOW_CS200	0x0200		/* CryptoSwift 200 PKI Accelerator */

/* Ralink Technologies products */
#define	PCI_PRODUCT_RALINK_RT2460	0x0101		/* RT2460 802.11b */
#define	PCI_PRODUCT_RALINK_RT2560	0x0201		/* RT2560 802.11b/g */

/* RATOC Systems products */
#define	PCI_PRODUCT_RATOC_REXPCI31	0x0853		/* REX PCI-31/33 SCSI */

/* Realtek (Creative Labs?) products */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* 8029 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8129	0x8129		/* 8129 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8138	0x8138		/* 8138 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8139B	0x8138		/* 8139B 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8169	0x8169		/* 8169 10/100/1000 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8180	0x8180		/* 8180 802.11b */

/* RICOH products */
#define	PCI_PRODUCT_RICOH_Rx5C465	0x0465		/* 5C465 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C466	0x0466		/* 5C466 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C475	0x0475		/* 5C475 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_RL5C476	0x0476		/* 5C476 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C477	0x0477		/* 5C477 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C478	0x0478		/* 5C478 PCI-CardBus bridge */
#define	PCI_PRODUCT_RICOH_Rx5C551	0x0551		/* 5C551 PCI-CardBus bridge/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C552	0x0552		/* 5C552 PCI-CardBus bridge/Firewire */

/* RISCom (SDL Communications?) products */
#define	PCI_PRODUCT_RISCOM_N2	0x5568		/* N2 */

/* RNS products */
#define	PCI_PRODUCT_RNS_FDDI	0x2200		/* 2200 FDDI */

/* S2io products */
#define	PCI_PRODUCT_S2IO_XFRAME	0x5831		/* Xframe 10 Gigabit ethernet adapter */

/* S3 products */
#define	PCI_PRODUCT_S3_VIRGE	0x5631		/* ViRGE */
#define	PCI_PRODUCT_S3_TRIO32	0x8810		/* Trio32 */
#define	PCI_PRODUCT_S3_TRIO64	0x8811		/* Trio32/64 */
#define	PCI_PRODUCT_S3_AURORA64P	0x8812		/* Aurora64V+ */
#define	PCI_PRODUCT_S3_TRIO64UVP	0x8814		/* Trio64UV+ */
#define	PCI_PRODUCT_S3_VIRGE_VX	0x883d		/* ViRGE/VX */
#define	PCI_PRODUCT_S3_868	0x8880		/* 868 */
#define	PCI_PRODUCT_S3_928	0x88b0		/* 86C928 */
#define	PCI_PRODUCT_S3_864_0	0x88c0		/* 86C864-0 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_1	0x88c1		/* 86C864-1 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_2	0x88c2		/* 86C864-2 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_3	0x88c3		/* 86C864-3 (\"Vision864\") */
#define	PCI_PRODUCT_S3_964_0	0x88d0		/* 86C964-0 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_1	0x88d1		/* 86C964-1 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_2	0x88d2		/* 86C964-2 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_3	0x88d3		/* 86C964-3 (\"Vision964\") */
#define	PCI_PRODUCT_S3_968_0	0x88f0		/* 86C968-0 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_1	0x88f1		/* 86C968-1 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_2	0x88f2		/* 86C968-2 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_3	0x88f3		/* 86C968-3 (\"Vision968\") */
#define	PCI_PRODUCT_S3_TRIO64V2_DX	0x8901		/* Trio64V2/DX */
#define	PCI_PRODUCT_S3_PLATO_PX	0x8901		/* Plato/PX */
#define	PCI_PRODUCT_S3_TRIO3D	0x8904		/* 86C365 Trio3D */
#define	PCI_PRODUCT_S3_VIRGE_DX	0x8a01		/* ViRGE/DX */
#define	PCI_PRODUCT_S3_VIRGE_GX2	0x8a10		/* ViRGE/GX2 */
#define	PCI_PRODUCT_S3_TRIO3D2X	0x8a13		/* Trio3D/2X */
#define	PCI_PRODUCT_S3_SAVAGE3D	0x8a20		/* Savage3D */
#define	PCI_PRODUCT_S3_SAVAGE3D_MV	0x8a21		/* Savage3D+MV */
#define	PCI_PRODUCT_S3_SAVAGE4	0x8a22		/* Savage4 */
#define	PCI_PRODUCT_S3_PROSAVAGE_KM133	0x8a26		/* ProSavage KM133 */
#define	PCI_PRODUCT_S3_VIRGE_MX	0x8c01		/* ViRGE/MX */
#define	PCI_PRODUCT_S3_VIRGE_MXP	0x8c03		/* ViRGE/MXP */
#define	PCI_PRODUCT_S3_SAVAGE_MX_MV	0x8c10		/* Savage/MX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_MX	0x8c11		/* Savage/MX */
#define	PCI_PRODUCT_S3_SAVAGE_IX_MV	0x8c12		/* Savage/IX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_IX	0x8c13		/* Savage/IX */
#define	PCI_PRODUCT_S3_SAVAGE_IXC	0x8c2e		/* Savage/IXC */
#define	PCI_PRODUCT_S3_SAVAGE2000	0x9102		/* Savage2000 */
#define	PCI_PRODUCT_S3_SONICVIBES	0xca00		/* SonicVibes */

/* SafeNet products */
#define	PCI_PRODUCT_SAFENET_SAFEXCEL	0x1141		/* SafeXcel */

/* Samsung Semiconductor products */
#define	PCI_PRODUCT_SAMSUNGSEMI_KS8920	0x8920		/* KS8920 10/100 Ethernet */

/* Sandburst products */
#define	PCI_PRODUCT_SANDBURST_QE1000	0x0180		/* QE1000 */
#define	PCI_PRODUCT_SANDBURST_FE1000	0x0200		/* FE1000 */
/*product SANDBURST	SE1600	0x0100	SE1600*/

/* SEGA Enterprises products */
#define	PCI_PRODUCT_SEGA_BROADBAND	0x1234		/* Broadband Adapter */

/* ServerWorks products */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_LE_AGP	0x0005		/* CNB20-LE PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB30_LE_PCI	0x0006		/* CNB30-LE PCI bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_LE_PCI	0x0007		/* CNB20-LE PCI bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_PCI	0x0008		/* CNB20-HE PCI bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_AGP	0x0009		/* CNB20-HE PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_X	0x0010		/* CIOB-X PCI-X bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_HE	0x0011		/* CMIC-HE PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB30_HE	0x0012		/* CNB30-HE PCI bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_PCI2	0x0013		/* CNB20-HE PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_LE	0x0014		/* CMIC-LE PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_SL	0x0017		/* CMIC-SL PCI/AGP bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_X2	0x0101		/* CIOB-X2 PCI-X bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_E	0x0110		/* CIOB-E PCI-X bridge */
#define	PCI_PRODUCT_SERVERWORKS_OSB4	0x0200		/* OSB4 southbridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB5	0x0201		/* CSB5 southbridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB6	0x0203		/* CSB6 southbridge */
#define	PCI_PRODUCT_SERVERWORKS_OSB4_IDE	0x0211		/* OSB4 IDE */
#define	PCI_PRODUCT_SERVERWORKS_CSB5_IDE	0x0212		/* CSB5 IDE */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_RAID	0x0213		/* CSB6 IDE/RAID */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_IDE	0x0217		/* CSB6 IDE/RAID */
#define	PCI_PRODUCT_SERVERWORKS_OSB4_USB	0x0220		/* OSB4/CSB5 USB Host Controller */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_USB	0x0221		/* CSB6 USB Host Controller */
#define	PCI_PRODUCT_SERVERWORKS_CSB5_LPC	0x0225		/* CSB5 ISA/LPC bridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_LPC	0x0227		/* CSB6 ISA/LPC bridge */
#define	PCI_PRODUCT_SERVERWORKS_K2_SATA	0x0240		/* K2 SATA */
#define	PCI_PRODUCT_SERVERWORKS_FRODO4_SATA	0x0241		/* Frodo4 SATA */
#define	PCI_PRODUCT_SERVERWORKS_FRODO8_SATA	0x0242		/* Frodo8 SATA */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_SATA	0x024a		/* HT-1000 SATA */
#define	PCI_PRODUCT_SERVERWORKS_BCM5714	0x0103		/* bcm5714/bcm5715 integral PCI-E to PCI-X bridge */

/* SGI products */
#define	PCI_PRODUCT_SGI_IOC3	0x0003		/* IOC3 */
#define	PCI_PRODUCT_SGI_RAD1	0x0005		/* PsiTech RAD1 */
#define	PCI_PRODUCT_SGI_TIGON	0x0009		/* Tigon Gigabit Ethernet */

/* SGS-Thomson products */
#define	PCI_PRODUCT_SGSTHOMSON_2000	0x0008		/* STG 2000X */
#define	PCI_PRODUCT_SGSTHOMSON_1764	0x1746		/* STG 1764X */

/* Broadcom (SiByte) products */
#define	PCI_PRODUCT_SIBYTE_BCM1250_PCIHB	0x0001		/* BCM1250 PCI Host Bridge */
#define	PCI_PRODUCT_SIBYTE_BCM1250_LDTHB	0x0002		/* BCM1250 LDT Host Bridge */

/* Sigma Designs products */
#define	PCI_PRODUCT_SIGMA_HOLLYWOODPLUS	0x8300		/* REALmagic Hollywood-Plus MPEG-2 Decoder */

/* SIIG Inc products */
#define	PCI_PRODUCT_SIIG_CYBER10_S550	0x1000		/* Cyber10x Serial 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_S650	0x1001		/* Cyber10x Serial 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_S850	0x1002		/* Cyber10x Serial 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO550	0x1010		/* Cyber10x I/O 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO650	0x1011		/* Cyber10x I/O 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO850	0x1010		/* Cyber10x I/O 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_P	0x1020		/* Cyber10x Parallel PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2P	0x1021		/* Cyber10x Parallel Dual PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S550	0x1030		/* Cyber10x Serial Dual 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S650	0x1031		/* Cyber10x Serial Dual 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S850	0x1032		/* Cyber10x Serial Dual 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P550	0x1034		/* Cyber10x 2S1P 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P650	0x1035		/* Cyber10x 2S1P 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P850	0x1036		/* Cyber10x 2S1P 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S550	0x1050		/* Cyber10x 4S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S650	0x1051		/* Cyber10x 4S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S850	0x1052		/* Cyber10x 4S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S550	0x2000		/* Cyber20x Serial 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S650	0x2001		/* Cyber20x Serial 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S850	0x2002		/* Cyber20x Serial 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO550	0x2010		/* Cyber20x I/O 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO650	0x2011		/* Cyber20x I/O 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO850	0x2010		/* Cyber20x I/O 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_P	0x2020		/* Cyber20x Parallel PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P	0x2021		/* Cyber20x Parallel Dual PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S550	0x2030		/* Cyber20x Serial Dual 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S650	0x2031		/* Cyber20x Serial Dual 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S850	0x2032		/* Cyber20x Serial Dual 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S550	0x2040		/* Cyber20x 2P1S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S650	0x2041		/* Cyber20x 2P1S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S850	0x2042		/* Cyber20x 2P1S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S550	0x2050		/* Cyber20x 4S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S650	0x2051		/* Cyber20x 4S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S850	0x2052		/* Cyber20x 4S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P550	0x2060		/* Cyber20x 2S1P 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P650	0x2061		/* Cyber20x 2S1P 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P850	0x2062		/* Cyber20x 2S1P 16850 PCI */
#define	PCI_PRODUCT_SIIG_PCISERIAL8000P	0x2082		/* PCI Serial 8000 Plus */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 */
#define	PCI_PRODUCT_SIS_86C205	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 or 5597/5598 ISA bridge */
#define	PCI_PRODUCT_SIS_600PMC	0x0009		/* 600 Power Mngmt Controller */
#define	PCI_PRODUCT_SIS_180_SATA	0x0180		/* 180 SATA controller */
#define	PCI_PRODUCT_SIS_190	0x0190		/* 190 Ethernet */
#define	PCI_PRODUCT_SIS_5597_VGA	0x0200		/* 5597/5598 integrated VGA */
#define	PCI_PRODUCT_SIS_300	0x0300		/* 300/305 AGP VGA */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_530HB	0x0530		/* 530 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_540HB	0x0540		/* 540 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_550HB	0x0550		/* 550 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_85C601	0x0601		/* 85C601 */
#define	PCI_PRODUCT_SIS_620	0x0620		/* 620 Host Bridge */
#define	PCI_PRODUCT_SIS_630	0x0630		/* 630 Host Bridge */
#define	PCI_PRODUCT_SIS_633	0x0633		/* 633 Host Bridge */
#define	PCI_PRODUCT_SIS_635	0x0635		/* 635 Host Bridge */
#define	PCI_PRODUCT_SIS_640	0x0640		/* 640 Host Bridge */
#define	PCI_PRODUCT_SIS_645	0x0645		/* 645 Host Bridge */
#define	PCI_PRODUCT_SIS_646	0x0646		/* 646 Host Bridge */
#define	PCI_PRODUCT_SIS_648	0x0648		/* 648 Host Bridge */
#define	PCI_PRODUCT_SIS_650	0x0650		/* 650 Host Bridge */
#define	PCI_PRODUCT_SIS_651	0x0651		/* 651 Host Bridge */
#define	PCI_PRODUCT_SIS_652	0x0652		/* 652 Host Bridge */
#define	PCI_PRODUCT_SIS_655	0x0655		/* 655 Host Bridge */
#define	PCI_PRODUCT_SIS_658	0x0658		/* 658 Host Bridge */
#define	PCI_PRODUCT_SIS_730	0x0730		/* 730 Host Bridge */
#define	PCI_PRODUCT_SIS_733	0x0733		/* 733 Host Bridge */
#define	PCI_PRODUCT_SIS_735	0x0735		/* 735 Host Bridge */
#define	PCI_PRODUCT_SIS_740	0x0740		/* 740 Host Bridge */
#define	PCI_PRODUCT_SIS_741	0x0741		/* 741 Host Bridge */
#define	PCI_PRODUCT_SIS_745	0x0745		/* 745 Host Bridge */
#define	PCI_PRODUCT_SIS_746	0x0746		/* 746 Host Bridge */
#define	PCI_PRODUCT_SIS_748	0x0748		/* 748 Host Bridge */
#define	PCI_PRODUCT_SIS_750	0x0750		/* 750 Host Bridge */
#define	PCI_PRODUCT_SIS_751	0x0751		/* 751 Host Bridge */
#define	PCI_PRODUCT_SIS_752	0x0752		/* 752 Host Bridge */
#define	PCI_PRODUCT_SIS_755	0x0755		/* 755 Host Bridge */
#define	PCI_PRODUCT_SIS_900	0x0900		/* 900 10/100 Ethernet */
#define	PCI_PRODUCT_SIS_961	0x0961		/* 961 Host Bridge */
#define	PCI_PRODUCT_SIS_962	0x0962		/* 962 Host Bridge */
#define	PCI_PRODUCT_SIS_963	0x0963		/* 963 Host Bridge */
#define	PCI_PRODUCT_SIS_964	0x0964		/* 964 Host Bridge */
#define	PCI_PRODUCT_SIS_965	0x0965		/* 965 Host Bridge */
#define	PCI_PRODUCT_SIS_5597_IDE	0x5513		/* 5597/5598 IDE controller */
#define	PCI_PRODUCT_SIS_5597_HB	0x5597		/* 5597/5598 host bridge */
#define	PCI_PRODUCT_SIS_530VGA	0x6306		/* 530 GUI Accelerator+3D */
#define	PCI_PRODUCT_SIS_6325	0x6325		/* 6325 AGP VGA */
#define	PCI_PRODUCT_SIS_6326	0x6326		/* 6326 AGP VGA */
#define	PCI_PRODUCT_SIS_5597_USB	0x7001		/* 5597/5598 USB host controller */
#define	PCI_PRODUCT_SIS_7002	0x7002		/* 7002 USB 2.0 host controller */
#define	PCI_PRODUCT_SIS_7012_AC	0x7012		/* 7012 AC-97 Sound */
#define	PCI_PRODUCT_SIS_7016	0x7016		/* 7016 10/100 Ethernet */
#define	PCI_PRODUCT_SIS_7018	0x7018		/* 7018 Sound */

/* Silicon Motion products */
#define	PCI_PRODUCT_SILMOTION_SM710	0x0710		/* LynxEM */
#define	PCI_PRODUCT_SILMOTION_SM712	0x0712		/* LynxEM+ */
#define	PCI_PRODUCT_SILMOTION_SM720	0x0720		/* Lynx3DM */
#define	PCI_PRODUCT_SILMOTION_SM810	0x0810		/* LynxE */
#define	PCI_PRODUCT_SILMOTION_SM811	0x0811		/* LynxE */
#define	PCI_PRODUCT_SILMOTION_SM820	0x0820		/* Lynx3D */
#define	PCI_PRODUCT_SILMOTION_SM910	0x0910		/* Lynx */

/* SMC products */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC37C665 */
#define	PCI_PRODUCT_SMC_37C922	0x1001		/* FDC37C922 */
#define	PCI_PRODUCT_SMC_83C170	0x0005		/* 83C170 (\"EPIC/100\") Fast Ethernet */
#define	PCI_PRODUCT_SMC_83C175	0x0006		/* 83C175 (\"EPIC/100\") Fast Ethernet */

/* Solidum Systems */
#define	PCI_PRODUCT_SOLIDUM_AMD971	0x2000		/* SNP8023: AMD 971 */
#define	PCI_PRODUCT_SOLIDUM_CLASS802	0x8023		/* SNP8023: Classifier Engine */
#define	PCI_PRODUCT_SOLIDUM_PAXWARE1100	0x1100		/* PAX.ware 1100 dual Gb Classifier Engine */

/* Sony products */
#define	PCI_PRODUCT_SONY_CXD1947A	0x8009		/* CXD1947A IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_CXD3222	0x8039		/* CXD3222 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_MEMSTICK	0x808a		/* Memory Stick I/F Controller */

/* Sun Microsystems products */
#define	PCI_PRODUCT_SUN_EBUS	0x1000		/* PCIO Ebus2 */
#define	PCI_PRODUCT_SUN_HMENETWORK	0x1001		/* PCIO Happy Meal Ethernet */
#define	PCI_PRODUCT_SUN_EBUSIII	0x1100		/* PCIO Ebus2 (US III) */
#define	PCI_PRODUCT_SUN_ERINETWORK	0x1101		/* ERI Ethernet */
#define	PCI_PRODUCT_SUN_FIREWIRE	0x1102		/* FireWire controller */
#define	PCI_PRODUCT_SUN_USB	0x1103		/* USB controller */
#define	PCI_PRODUCT_SUN_GEMNETWORK	0x2bad		/* GEM Gigabit Ethernet */
#define	PCI_PRODUCT_SUN_SIMBA	0x5000		/* Simba PCI bridge */
#define	PCI_PRODUCT_SUN_5821	0x5454		/* BCM5821 */
#define	PCI_PRODUCT_SUN_SCA1K	0x5455		/* Crypto Accelerator 1000 */
#define	PCI_PRODUCT_SUN_PSYCHO	0x8000		/* psycho PCI controller */
#define	PCI_PRODUCT_SUN_MS_IIep	0x9000		/* microSPARC IIep PCI */
#define	PCI_PRODUCT_SUN_US_IIi	0xa000		/* UltraSPARC IIi PCI */
#define	PCI_PRODUCT_SUN_US_IIe	0xa001		/* UltraSPARC IIe PCI */

/* Sundance Technology products */
#define	PCI_PRODUCT_SUNDANCETI_ST201	0x0201		/* ST201 10/100 Ethernet */
#define	PCI_PRODUCT_SUNDANCETI_ST1023	0x1023		/* ST1023 Gigabit Ethernet */
#define	PCI_PRODUCT_SUNDANCETI_ST2021	0x2021		/* ST2021 Gigabit Ethernet */

/* Surecom Technology products */
#define	PCI_PRODUCT_SURECOM_NE34	0x0e34		/* NE-34 Ethernet */

/* Symphony Labs products */
#define	PCI_PRODUCT_SYMPHONY_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_82C103	0x0103		/* 82C103 */
#define	PCI_PRODUCT_SYMPHONY_82C105	0x0105		/* 82C105 */
#define	PCI_PRODUCT_SYMPHONY2_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_83C553	0x0565		/* 83C553 PCI-ISA Bridge */

/* Schneider & Koch (really SysKonnect) products */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SKNET_FDDI	0x4000		/* SK-NET FDDI-xP */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SKNET_GE	0x4300		/* SK-NET GE */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2	0x4320		/* SK-9821 v2.0 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9DX1	0x4400		/* SK-NET SK-9DX1 Gigabit Ethernet */
/* These next two are are really subsystem IDs */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9D21	0x4421		/* SK-9D21 1000BASE-T */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9D41	0x4441		/* SK-9D41 1000BASE-X */
/* This next entry is used for both single-port (SK-9E21D) and dual-port
 * (SK-9E22) gig-e based on Marvell Yukon-2, with PCI revision	0x17 for
 * the single-port and 0x12 for the	dual-port.
 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9E21	0x9e00		/* SK-9E21D/SK-9E22 1000base-T */

/* Tamarack Microelectronics */
#define	PCI_PRODUCT_TAMARACK_TC9021	0x1021		/* TC9021 Gigabit Ethernet */
#define	PCI_PRODUCT_TAMARACK_TC9021_ALT	0x9021		/* TC9021 Gigabit Ethernet (alt ID) */

/* Tandem Computers */
#define	PCI_PRODUCT_TANDEM_SERVERNETII	0x0005		/* ServerNet II VIA adapter */

/* Tekram Technology products (1st PCI Vendor ID) */
#define	PCI_PRODUCT_TEKRAM_DC290	0xdc29		/* DC-290(M) */

/* Tekram Technology products (2nd PCI Vendor ID) */
#define	PCI_PRODUCT_TEKRAM2_DC690C	0x690c		/* DC-690C */
#define	PCI_PRODUCT_TEKRAM2_DC315	0x0391		/* DC-315/DC-395 */

/* Texas Instruments products */
#define	PCI_PRODUCT_TI_TLAN	0x0500		/* TLAN */
#define	PCI_PRODUCT_TI_TVP4020	0x3d07		/* TVP4020 Permedia 2 */
#define	PCI_PRODUCT_TI_TSB12LV21	0x8000		/* TSB12LV21 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV22	0x8009		/* TSB12LV22 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4450LYNX	0x8011		/* PCI4450 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4410LYNX	0x8017		/* PCI4410 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_TSB12LV23	0x8019		/* TSB12LV23 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV26	0x8020		/* TSB12LV26 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA22	0x8021		/* TSB43AA22 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA22A	0x8023		/* TSB43AA22/A IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA23	0x8024		/* TSB43AA23 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AB21	0x8026		/* TSB43AA21 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4451LYNX	0x8027		/* PCI4451 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4510LYNX	0x8029		/* PCI4510 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4520LYNX	0x802A		/* PCI4520 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI7410LYNX	0x802B		/* PCI7[4-6]10 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI72111CB	0x8031		/* PCI7x21/7x11 Cardbus Controller */
#define	PCI_PRODUCT_TI_PCI72111FW	0x8032		/* PCI7x21/7x11 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI72111FM	0x8033		/* PCI7x21/7x11 Integrated FlashMedia Controller */
#define	PCI_PRODUCT_TI_PCI6515A	0x8036		/* PCI6515A Cardbus Controller */
#define	PCI_PRODUCT_TI_PCI6515ASM	0x8038		/* PCI6515A Cardbus Controller (Smart Card mode) */
#define	PCI_PRODUCT_TI_ACX100	0x8400		/* ACX100 802.11b */
#define	PCI_PRODUCT_TI_ACX111	0x9066		/* ACX111 802.11b/g */
#define	PCI_PRODUCT_TI_PCI1130	0xac12		/* PCI1130 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1031	0xac13		/* PCI1031 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_TI_PCI1131	0xac15		/* PCI1131 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1250	0xac16		/* PCI1250 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1220	0xac17		/* PCI1220 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1221	0xac19		/* PCI1221 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1210	0xac1a		/* PCI1210 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1450	0xac1b		/* PCI1450 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1225	0xac1c		/* PCI1225 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251	0xac1d		/* PCI1251 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1211	0xac1e		/* PCI1211 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251B	0xac1f		/* PCI1251B PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI2030	0xac20		/* PCI2030 PCI-PCI Bridge */
#define	PCI_PRODUCT_TI_PCI2050	0xac28		/* PCI2050 PCI-PCI Bridge */
#define	PCI_PRODUCT_TI_PCI4450YENTA	0xac40		/* PCI4450 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4410YENTA	0xac41		/* PCI4410 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4451YENTA	0xac42		/* PCI4451 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4510YENTA	0xac44		/* PCI4510 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4520YENTA	0xac46		/* PCI4520 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7510YENTA	0xac47		/* PCI7510 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7610YENTA	0xac48		/* PCI7610 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7410YENTA	0xac49		/* PCI7410 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7610SM	0xac4A		/* PCI7610 PCI-CardBus Bridge (Smart Card mode) */
#define	PCI_PRODUCT_TI_PCI7410SD	0xac4B		/* PCI7[46]10 PCI-CardBus Bridge (SD/MMC mode) */
#define	PCI_PRODUCT_TI_PCI7410MS	0xac4C		/* PCI7[46]10 PCI-CardBus Bridge (Memory stick mode) */
#define	PCI_PRODUCT_TI_PCI1410	0xac50		/* PCI1410 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1420	0xac51		/* PCI1420 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1451	0xac52		/* PCI1451 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1421	0xac53		/* PCI1421 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1620	0xac54		/* PCI1620 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1520	0xac55		/* PCI1520 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1510	0xac56		/* PCI1510 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1530	0xac57		/* PCI1530 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1515	0xac58		/* PCI1515 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI2040	0xac60		/* PCI2040 PCI-DSP Bridge */

/* Titan Electronics products */
#define	PCI_PRODUCT_TITAN_PCI800L	0x8080		/* PCI-800L */
#define	PCI_PRODUCT_TITAN_PCI800H	0xa003		/* PCI-800H */
#define	PCI_PRODUCT_TITAN_PCI100H	0xa001		/* PCI-100H */

/* Toshiba America products */
#define	PCI_PRODUCT_TOSHIBA_R4X00	0x0009		/* R4x00 Host-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA_TC35856F	0x0020		/* TC35856F ATM (\"Meteor\") */

/* Toshiba products */
#define	PCI_PRODUCT_TOSHIBA2_PORTEGE	0x0001		/* Portege Notebook */
#define	PCI_PRODUCT_TOSHIBA2_HOST	0x0601		/* Host Bridge/Controller */
#define	PCI_PRODUCT_TOSHIBA2_ISA	0x0602		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95	0x0603		/* ToPIC95 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95B	0x060a		/* ToPIC95B PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC97	0x060f		/* ToPIC97 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_SMCARD	0x0804		/* Smart Media Controller */
#define	PCI_PRODUCT_TOSHIBA2_SDCARD	0x0805		/* Secure Digital Card Controller Type-A */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC100	0x0617		/* ToPIC100 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_OBOE	0x0701		/* Fast Infrared Type O */
#define	PCI_PRODUCT_TOSHIBA2_DONAUOBOE	0x0d01		/* Fast Infrared Type DO */

/* Transmeta products */
#define	PCI_PRODUCT_TRANSMETA_TM8000NB	0x0061		/* TM8000 Integrated Northbridge */
#define	PCI_PRODUCT_TRANSMETA_NORTHBRIDGE	0x0295		/* Virtual Northbridge */
#define	PCI_PRODUCT_TRANSMETA_LONGRUN	0x0395		/* LongRun Northbridge */
#define	PCI_PRODUCT_TRANSMETA_SDRAM	0x0396		/* SDRAM Controller */
#define	PCI_PRODUCT_TRANSMETA_BIOS_SCRATCH	0x0397		/* BIOS Scratchpad */

/* Trident products */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_DX	0x2000		/* 4DWAVE DX */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_NX	0x2001		/* 4DWAVE NX */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADE_I7	0x8420		/* CyberBlade i7 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9320	0x9320		/* TGUI 9320 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9350	0x9350		/* TGUI 9350 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9360	0x9360		/* TGUI 9360 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397	0x9397		/* CYBER 9397 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397DVD	0x939a		/* CYBER 9397DVD */
#define	PCI_PRODUCT_TRIDENT_CYBER_9525	0x9525		/* CYBER 9525 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9420	0x9420		/* TGUI 9420 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9440	0x9440		/* TGUI 9440 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9660	0x9660		/* TGUI 9660 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9680	0x9680		/* TGUI 9680 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9682	0x9682		/* TGUI 9682 */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADE	0x9910		/* CyberBlade */

/* Triones Technologies products */
/* The 366 and 370 controllers have the same product ID */
#define	PCI_PRODUCT_TRIONES_HPT343	0x0003		/* HPT343/345 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT366	0x0004		/* HPT366/370/372 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT372A	0x0005		/* HPT372A IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT302	0x0006		/* HPT302 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT371	0x0007		/* HPT371 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT374	0x0008		/* HPT374 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT372N	0x0009		/* HPT372N IDE Controller */

/* TriTech Microelectronics products*/
#define	PCI_PRODUCT_TRITECH_TR25202	0xfc02		/* Pyramid3D TR25202 */

/* Tseng Labs products */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_A	0x3202		/* ET4000w32p rev A */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_B	0x3205		/* ET4000w32p rev B */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_C	0x3206		/* ET4000w32p rev C */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_D	0x3207		/* ET4000w32p rev D */
#define	PCI_PRODUCT_TSENG_ET6000	0x3208		/* ET6000 */

/* Turtle Beach products */
#define	PCI_PRODUCT_TURTLE_BEACH_SANTA_CRUZ	0x3357		/* Santa Cruz */

/* UMC products */
#define	PCI_PRODUCT_UMC_UM82C881	0x0001		/* UM82C881 486 Chipset */
#define	PCI_PRODUCT_UMC_UM82C886	0x0002		/* UM82C886 PCI-ISA Bridge */
#define	PCI_PRODUCT_UMC_UM8673F	0x0101		/* UM8673F EIDE Controller */
#define	PCI_PRODUCT_UMC_UM8881	0x0881		/* UM8881 HB4 486 PCI Chipset */
#define	PCI_PRODUCT_UMC_UM82C891	0x0891		/* UM82C891 */
#define	PCI_PRODUCT_UMC_UM886A	0x1001		/* UM886A */
#define	PCI_PRODUCT_UMC_UM8886BF	0x673a		/* UM8886BF */
#define	PCI_PRODUCT_UMC_UM8710	0x8710		/* UM8710 */
#define	PCI_PRODUCT_UMC_UM8886	0x886a		/* UM8886 */
#define	PCI_PRODUCT_UMC_UM8881F	0x8881		/* UM8881F PCI-Host bridge */
#define	PCI_PRODUCT_UMC_UM8886F	0x8886		/* UM8886F PCI-ISA bridge */
#define	PCI_PRODUCT_UMC_UM8886A	0x888a		/* UM8886A */
#define	PCI_PRODUCT_UMC_UM8891A	0x8891		/* UM8891A */
#define	PCI_PRODUCT_UMC_UM9017F	0x9017		/* UM9017F */
#define	PCI_PRODUCT_UMC_UM8886N	0xe88a		/* UM8886N */
#define	PCI_PRODUCT_UMC_UM8891N	0xe891		/* UM8891N */

/* ULSI Systems products */
#define	PCI_PRODUCT_ULSI_US201	0x0201		/* US201 */

/* US Robotics products */
#define	PCI_PRODUCT_USR_3C2884A	0x1007		/* 56K Voice Internal PCI Modem (WinModem) */
#define	PCI_PRODUCT_USR_3CP5609	0x1008		/* 3CP5609 PCI 16550 Modem */
#define	PCI_PRODUCT_USR2_USR997902	0x0116		/* USR997902 Gigabit Ethernet */
#define	PCI_PRODUCT_USR2_2415	0x3685		/* Wireless PCI-PCMCIA adapter */

/* V3 Semiconductor products */
#define	PCI_PRODUCT_V3_V292PBC	0x0292		/* V292PBC AMD290x0 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V960PBC	0x0960		/* V960PBC i960 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V96DPC	0xc960		/* V96DPC i960 (Dual) Host-PCI Bridge */

/* VIA Technologies products, from http://www.via.com.tw/ */
#define	PCI_PRODUCT_VIATECH_VT6305	0x0130		/* VT6305 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_VIATECH_KT880	0x0269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8363_HB	0x0305		/* VT8363 (Apollo KT133) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8371_HB	0x0391		/* VT8371 (Apollo KX133) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501_MVP4	0x0501		/* VT8501 (Apollo MVP4) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C505	0x0505		/* VT82C505 (Pluto) */
#define	PCI_PRODUCT_VIATECH_VT82C561	0x0561		/* VT82C561 */
#define	PCI_PRODUCT_VIATECH_VT82C586A_IDE	0x0571		/* VT82C586A IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C576	0x0576		/* VT82C576 3V */
#define	PCI_PRODUCT_VIATECH_VT82C580VP	0x0585		/* VT82C580 (Apollo VP) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_ISA	0x0586		/* VT82C586 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C595	0x0595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C596A	0x0596		/* VT82C596A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C597	0x0597		/* VT82C597 (Apollo VP3) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C598PCI	0x0598		/* VT82C598 (Apollo MVP3) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8605PCI	0x0605		/* VT8605 (Apollo ProMedia 133) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C686A_ISA	0x0686		/* VT82C686A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C691	0x0691		/* VT82C691 (Apollo Pro) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT82C693	0x0693		/* VT82C693 (Apollo Pro Plus) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT86C926	0x0926		/* VT86C926 Amazon PCI-Ethernet Controller */
#define	PCI_PRODUCT_VIATECH_VT82C570M	0x1000		/* VT82C570M (Apollo) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C570MV	0x1006		/* VT82C570M (Apollo) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_1	0x1269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_IDE	0x1571		/* VT82C586 IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C595_2	0x1595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_2	0x2269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT83C572	0x3038		/* VT83C572 USB Controller */
#define	PCI_PRODUCT_VIATECH_VT82C586_PWR	0x3040		/* VT82C586 Power Management Controller */
#define	PCI_PRODUCT_VIATECH_VT3043	0x3043		/* VT3043 (Rhine) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT6306	0x3044		/* VT6306 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_VIATECH_VT6105M	0x3053		/* VT6105M (Rhine III) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT82C686A_SMB	0x3057		/* VT82C686A SMBus Controller */
#define	PCI_PRODUCT_VIATECH_VT82C686A_AC97	0x3058		/* VT82C686A AC-97 Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT8233_AC97	0x3059		/* VT8233/VT8235 AC-97 Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT6102	0x3065		/* VT6102 (Rhine II) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT82C686A_MC97	0x3068		/* VT82C686A MC-97 Modem Controller */
#define	PCI_PRODUCT_VIATECH_VT8233	0x3074		/* VT8233 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8366	0x3099		/* VT8366 (Apollo KT266) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8653	0x3101		/* VT8653 (Apollo Pro 266T) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237_EHCI	0x3104		/* VT8237 EHCI USB Controller */
#define	PCI_PRODUCT_VIATECH_VT6105	0x3106		/* VT6105 (Rhine III) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT612X	0x3119		/* VT612X (Velocity) 10/100/1000 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT8623_VGA	0x3122		/* VT8623 (Apollo CLE266) VGA Controller */
#define	PCI_PRODUCT_VIATECH_VT8623	0x3123		/* VT8623 (Apollo CLE266) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8233A	0x3147		/* VT8233A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237_SATA	0x3149		/* VT8237 Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT8237R_SATA	0x3349		/* VT8237R Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT6421_RAID	0x3249		/* VT6421 Serial RAID Controller */
#define	PCI_PRODUCT_VIATECH_KT880_3	0x3269		/* KT880 CPU to PCI bridge */
#define	PCI_PRODUCT_VIATECH_VT8235	0x3177		/* VT8235 (Apollo KT400) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8377	0x3189		/* VT8377 Apollo KT400 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8378	0x3205		/* VT8378 Apollo KM400 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237	0x3227		/* VT8237 (Apollo KT600) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_4	0x4269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT86C100A	0x6100		/* VT86C100A (Rhine-II) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT8378_IG	0x7205		/* VT8378 KM400 UniChrome Integrated Graphics */
#define	PCI_PRODUCT_VIATECH_KT880_5	0x7269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8231	0x8231		/* VT8231 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8231_PWR	0x8235		/* VT8231 Power Management Controller */
#define	PCI_PRODUCT_VIATECH_VT8363_PPB	0x8305		/* VT8363 (Apollo KT133) PCI to AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8371_PPB	0x8391		/* VT8371 (Apollo KX133) PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501AGP	0x8501		/* VT8501 (Apollo MVP4) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C597AGP	0x8597		/* VT82C597 (Apollo VP3) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C598AGP	0x8598		/* VT82C598 (Apollo MVP3) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8605AGP	0x8605		/* VT8605 (Apollo ProMedia 133) Host-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8633AGP	0xb091		/* VT8633 (Apollo Pro 266) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8366AGP	0xb099		/* VT8366 (Apollo KT266) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8377AGP	0xb168		/* VT8377 CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8377CEAGP	0xb198		/* VT8377CE CPU-AGP Bridge */

/* Vortex Computer Systems products */
#define	PCI_PRODUCT_VORTEX_GDT_60x0	0x0000		/* GDT6000/6020/6050 */
#define	PCI_PRODUCT_VORTEX_GDT_6000B	0x0001		/* GDT6000B/6010 */
#define	PCI_PRODUCT_VORTEX_GDT_6x10	0x0002		/* GDT6110/6510 */
#define	PCI_PRODUCT_VORTEX_GDT_6x20	0x0003		/* GDT6120/6520 */
#define	PCI_PRODUCT_VORTEX_GDT_6530	0x0004		/* GDT6530 */
#define	PCI_PRODUCT_VORTEX_GDT_6550	0x0005		/* GDT6550 */
#define	PCI_PRODUCT_VORTEX_GDT_6x17	0x0006		/* GDT6117/6517 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27	0x0007		/* GDT6127/6527 */
#define	PCI_PRODUCT_VORTEX_GDT_6537	0x0008		/* GDT6537 */
#define	PCI_PRODUCT_VORTEX_GDT_6557	0x0009		/* GDT6557/6557-ECC */
#define	PCI_PRODUCT_VORTEX_GDT_6x15	0x000a		/* GDT6115/6515 */
#define	PCI_PRODUCT_VORTEX_GDT_6x25	0x000b		/* GDT6125/6525 */
#define	PCI_PRODUCT_VORTEX_GDT_6535	0x000c		/* GDT6535 */
#define	PCI_PRODUCT_VORTEX_GDT_6555	0x000d		/* GDT6555/6555-ECC */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP	0x0100		/* GDT6[15]17RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP	0x0101		/* GDT6[15]27RP */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP	0x0102		/* GDT6537RP */
#define	PCI_PRODUCT_VORTEX_GDT_6557RP	0x0103		/* GDT6557RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP	0x0104		/* GDT6[15]11RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP	0x0105		/* GDT6[15]21RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RD	0x0110		/* GDT6[15]17RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RD	0x0111		/* GDT6[5]127RD */
#define	PCI_PRODUCT_VORTEX_GDT_6537RD	0x0112		/* GDT6537RD */
#define	PCI_PRODUCT_VORTEX_GDT_6557RD	0x0113		/* GDT6557RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RD	0x0114		/* GDT6[15]11RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RD	0x0115		/* GDT6[15]21RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x18RD	0x0118		/* GDT6[156]18RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RD	0x0119		/* GDT6[156]28RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RD	0x011a		/* GDT6[56]38RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RD	0x011b		/* GDT6[56]58RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP2	0x0120		/* GDT6[15]17RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP2	0x0121		/* GDT6[15]27RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP2	0x0123		/* GDT6537RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP2	0x0124		/* GDT6[15]11RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP2	0x0125		/* GDT6[15]21RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x13RS	0x0136		/* GDT6513RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x23RS	0x0137		/* GDT6523RS */
#define	PCI_PRODUCT_VORTEX_GDT_6518RS	0x0138		/* GDT6518RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RS	0x0139		/* GDT6x28RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RS	0x013a		/* GDT6x38RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RS	0x013b		/* GDT6x58RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x33RS	0x013c		/* GDT6x33RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x43RS	0x013d		/* GDT6x43RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x53RS	0x013e		/* GDT6x53RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x63RS	0x013f		/* GDT6x63RS */
#define	PCI_PRODUCT_VORTEX_GDT_7x13RN	0x0166		/* GDT7x13RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x23RN	0x0167		/* GDT7x23RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x18RN	0x0168		/* GDT7[156]18RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x28RN	0x0169		/* GDT7[156]28RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x38RN	0x016a		/* GDT7[56]38RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x58RN	0x016b		/* GDT7[56]58RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x43RN	0x016d		/* GDT7[56]43RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x53RN	0x016E		/* GDT7x53RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x63RN	0x016F		/* GDT7x63RN */
#define	PCI_PRODUCT_VORTEX_GDT_4x13RZ	0x01D6		/* GDT4x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_4x23RZ	0x01D7		/* GDT4x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x13RZ	0x01F6		/* GDT8x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x23RZ	0x01F7		/* GDT8x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x33RZ	0x01FC		/* GDT8x33RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x43RZ	0x01FD		/* GDT8x43RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x53RZ	0x01FE		/* GDT8x53RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x63RZ	0x01FF		/* GDT8x63RZ */
#define	PCI_PRODUCT_VORTEX_GDT_6x19RD	0x0210		/* GDT6[56]19RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x29RD	0x0211		/* GDT6[56]29RD */
#define	PCI_PRODUCT_VORTEX_GDT_7x19RN	0x0260		/* GDT7[56]19RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x29RN	0x0261		/* GDT7[56]29RN */
#define	PCI_PRODUCT_VORTEX_GDT_ICP	0x0300		/* ICP */

/* VLSI products */
#define	PCI_PRODUCT_VLSI_82C592	0x0005		/* 82C592 CPU Bridge */
#define	PCI_PRODUCT_VLSI_82C593	0x0006		/* 82C593 ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C594	0x0007		/* 82C594 Wildcat System Controller */
#define	PCI_PRODUCT_VLSI_82C596597	0x0008		/* 82C596/597 Wildcat ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C541	0x000c		/* 82C541 */
#define	PCI_PRODUCT_VLSI_82C543	0x000d		/* 82C543 */
#define	PCI_PRODUCT_VLSI_82C532	0x0101		/* 82C532 */
#define	PCI_PRODUCT_VLSI_82C534	0x0102		/* 82C534 */
#define	PCI_PRODUCT_VLSI_82C535	0x0104		/* 82C535 */
#define	PCI_PRODUCT_VLSI_82C147	0x0105		/* 82C147 */
#define	PCI_PRODUCT_VLSI_82C975	0x0200		/* 82C975 */
#define	PCI_PRODUCT_VLSI_82C925	0x0280		/* 82C925 */

/* VMware products */
#define	PCI_PRODUCT_VMWARE_VIRTUAL	0x0710		/* Virtual SVGA */
#define	PCI_PRODUCT_VMWARE_VIRTUAL2	0x0405		/* Virtual SVGA II */

/* Weitek products */
#define	PCI_PRODUCT_WEITEK_P9000	0x9001		/* P9000 */
#define	PCI_PRODUCT_WEITEK_P9100	0x9100		/* P9100 */

/* Western Digital products */
#define	PCI_PRODUCT_WD_WD33C193A	0x0193		/* WD33C193A */
#define	PCI_PRODUCT_WD_WD33C196A	0x0196		/* WD33C196A */
#define	PCI_PRODUCT_WD_WD33C197A	0x0197		/* WD33C197A */
#define	PCI_PRODUCT_WD_WD7193	0x3193		/* WD7193 */
#define	PCI_PRODUCT_WD_WD7197	0x3197		/* WD7197 */
#define	PCI_PRODUCT_WD_WD33C296A	0x3296		/* WD33C296A */
#define	PCI_PRODUCT_WD_WD34C296	0x4296		/* WD34C296 */
#define	PCI_PRODUCT_WD_90C	0xc24a		/* 90C */

/* Winbond Electronics products */
#define	PCI_PRODUCT_WINBOND_W83769F	0x0001		/* W83769F */
#define	PCI_PRODUCT_WINBOND_W83C553F_0	0x0565		/* W83C553F PCI-ISA Bridge */
#define	PCI_PRODUCT_WINBOND_W83C553F_1	0x0105		/* W83C553F IDE Controller */
#define	PCI_PRODUCT_WINBOND_W89C840F	0x0840		/* W89C840F 10/100 Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F	0x0940		/* W89C940F Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F_1	0x5a5a		/* W89C940F Ethernet */
#define	PCI_PRODUCT_WINBOND_W6692	0x6692		/* W6692 ISDN */

/* Workbit products */
#define	PCI_PRODUCT_WORKBIT_NJSC32BI	0x8007		/* NinjaSCSI-32Bi SCSI */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE	0x8009		/* NinjaSCSI-32UDE SCSI */
#define	PCI_PRODUCT_WORKBIT_NJSC32BI_KME	0xf007		/* NinjaSCSI-32Bi SCSI (KME) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_IODATA	0xf010		/* NinjaSCSI-32UDE SCSI (IODATA) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC	0xf012		/* NinjaSCSI-32UDE SCSI (LOGITEC) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC2	0xf013		/* NinjaSCSI-32UDE SCSI (LOGITEC2) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_BUFFALO	0xf015		/* NinjaSCSI-32UDE SCSI (BUFFALO) */

/* Xircom products */
/* is the `-3' here just indicating revision 3, or is it really part
   of the device name? */
#define	PCI_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 Fast Ethernet Controller */
/* this is the device id `indicating 21143 driver compatibility' */
#define	PCI_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 Fast Ethernet Controller (21143) */
#define	PCI_PRODUCT_XIRCOM_WINGLOBAL	0x000c		/* WinGlobal Modem */
#define	PCI_PRODUCT_XIRCOM_MODEM56	0x0103		/* 56k Modem */

/* Yamaha products */
#define	PCI_PRODUCT_YAMAHA_YMF724	0x0004		/* 724 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740	0x000a		/* 740 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740C	0x000c		/* 740C (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF724F	0x000d		/* 724F (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF744B	0x0010		/* 744 (DS-1S) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF754	0x0012		/* 754 (DS-1E) Audio */

/* Zeinet products */
#define	PCI_PRODUCT_ZEINET_1221	0x0001		/* 1221 */

/* Ziatech products */
#define	PCI_PRODUCT_ZIATECH_ZT8905	0x8905		/* PCI-ST32 Bridge */

/* Zoran products */
#define	PCI_PRODUCT_ZORAN_ZR36120	0x6120		/* Video Controller */
