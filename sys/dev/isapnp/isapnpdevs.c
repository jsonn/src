/*	$NetBSD: isapnpdevs.c,v 1.1.2.1 1998/08/08 03:06:48 eeh Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: isapnpdevs,v 1.2 1998/07/30 09:45:16 drochner Exp 
 */

/*
 * Copyright (c) 1998, Christos Zoulas
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
 *      This product includes software developed by Christos Zoulas
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
#include <sys/param.h>
#include <dev/isapnp/isapnpdevs.h>


/* Adaptec SCSI */
static const char *isapnp_aic_devlogic[] = {
	"ADP1520",	/* Adaptec AHA-1520B */
	NULL
};
static const char *isapnp_aic_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_aic_devinfo = {
	isapnp_aic_devlogic, isapnp_aic_devcompat
};

/* National Semiconductor Serial */
static const char *isapnp_com_devlogic[] = {
	"BDP3336",	/* Best Data Prods. 336F */
	"OZO8039",	/* Zoom 56k flex */
	"BRI1400",	/* Boca 33.6 PnP */
	"BRIB400",	/* Boca 56k PnP */
	"ROK0010",	/* Rockwell ? */
	"USR2070",	/* USR Sportster 56k */
	NULL
};
static const char *isapnp_com_devcompat[] = {
	"PNP0500",	/* Generic 8250/16450 */
	"PNP0501",	/* Generic 16550A */
	NULL
};
const struct isapnp_devinfo isapnp_com_devinfo = {
	isapnp_com_devlogic, isapnp_com_devcompat
};

/* 3Com 3CXXX Ethernet */
static const char *isapnp_ep_devlogic[] = {
	"TCM5090",	/* 3Com 3c509B */
	"TCM5091",	/* 3Com 3c509B-1 */
	"TCM5094",	/* 3Com 3c509B-4 */
	"TCM5095",	/* 3Com 3c509B-5 */
	"TCM5098",	/* 3Com 3c509B-8 */
	NULL
};
static const char *isapnp_ep_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_ep_devinfo = {
	isapnp_ep_devlogic, isapnp_ep_devcompat
};

/* Generic Joystick */
static const char *isapnp_joy_devlogic[] = {
	"CSCA801",	/* Terratec EWS64XL */
	"CTL7002",	/* Creative Vibra16CL */
	"ESS0001",	/* ESS1868 */
	"OPT0001",	/* OPTi Audio 16 */
	"PNPB02F",	/* XXX broken GUS PnP */
	NULL
};
static const char *isapnp_joy_devcompat[] = {
	"PNPB02F",	/* generic */
	NULL
};
const struct isapnp_devinfo isapnp_joy_devinfo = {
	isapnp_joy_devlogic, isapnp_joy_devcompat
};

/* Gravis Ultrasound */
static const char *isapnp_gus_devlogic[] = {
	"GRV0000",	/* Gravis Ultrasound */
	NULL
};
static const char *isapnp_gus_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_gus_devinfo = {
	isapnp_gus_devlogic, isapnp_gus_devcompat
};

/* Lance Ethernet */
static const char *isapnp_le_devlogic[] = {
	"TKN0010",	/* Lance Ethernet */
	NULL
};
static const char *isapnp_le_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_le_devinfo = {
	isapnp_le_devlogic, isapnp_le_devcompat
};

/* NE2000 Ethernet */
static const char *isapnp_ne_devlogic[] = {
	NULL
};
static const char *isapnp_ne_devcompat[] = {
	"PNP80D6",	/* Digital DE305 ISAPnP */
	NULL
};
const struct isapnp_devinfo isapnp_ne_devinfo = {
	isapnp_ne_devlogic, isapnp_ne_devcompat
};

/* PCMCIA bridge */
static const char *isapnp_pcic_devlogic[] = {
	NULL
};
static const char *isapnp_pcic_devcompat[] = {
	"PNP0E00",	/* PCIC Compatible PCMCIA Bridge */
	NULL
};
const struct isapnp_devinfo isapnp_pcic_devinfo = {
	isapnp_pcic_devlogic, isapnp_pcic_devcompat
};

/* Creative Soundblaster */
static const char *isapnp_sb_devlogic[] = {
	"ADS7150",	/* AD1815 */
	"ADS7180",	/* AD1816 */
	"CTL0001",	/* SB */
	"CTL0031",	/* SB AWE32 */
	"CTL0041",	/* SB16 PnP (CT4131) */
	"CTL0043",	/* SB16 PnP (CT4170) */
	"CTL0042",	/* SB AWE64 Value */
	"CTL0044",	/* SB AWE64 Gold */
	"CTL0045",	/* SB AWE64 Value */
	"ESS1868",	/* ESS1868 */
	"OPT9250",	/* Televideo card, Opti */
	NULL
};
static const char *isapnp_sb_devcompat[] = {
	"PNPB000",	/* Generic SB 1.5 */
	"PNPB001",	/* Generic SB 2.0 */
	"PNPB002",	/* Generic SB Pro */
	"PNPB003",	/* Generic SB 16 */
	NULL
};
const struct isapnp_devinfo isapnp_sb_devinfo = {
	isapnp_sb_devlogic, isapnp_sb_devcompat
};

/* Western Digital Disk Controller */
static const char *isapnp_wdc_devlogic[] = {
	NULL
};
static const char *isapnp_wdc_devcompat[] = {
	"PNP0600",	/* Western Digital Compatible Controller */
	NULL
};
const struct isapnp_devinfo isapnp_wdc_devinfo = {
	isapnp_wdc_devlogic, isapnp_wdc_devcompat
};

/* Microsoft Sound System */
static const char *isapnp_wss_devlogic[] = {
	"CSC0000",	/* Windows Sound System */
	NULL
};
static const char *isapnp_wss_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_wss_devinfo = {
	isapnp_wss_devlogic, isapnp_wss_devcompat
};

/* Yamaha Sound */
static const char *isapnp_ym_devlogic[] = {
	"YMH0021",	/* OPL3-SA2, OPL3-SA3 */
	NULL
};
static const char *isapnp_ym_devcompat[] = {
	NULL
};
const struct isapnp_devinfo isapnp_ym_devinfo = {
	isapnp_ym_devlogic, isapnp_ym_devcompat
};

