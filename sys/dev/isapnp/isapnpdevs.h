/*	$NetBSD: isapnpdevs.h,v 1.1.2.1 1998/08/08 03:06:49 eeh Exp $	*/

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

/*
 * List of known drivers
 */
struct isapnp_devinfo {
	const char *const *devlogic;
	const char *const *devcompat;
};

/* Adaptec SCSI */
extern const struct isapnp_devinfo isapnp_aic_devinfo;
/* National Semiconductor Serial */
extern const struct isapnp_devinfo isapnp_com_devinfo;
/* 3Com 3CXXX Ethernet */
extern const struct isapnp_devinfo isapnp_ep_devinfo;
/* Generic Joystick */
extern const struct isapnp_devinfo isapnp_joy_devinfo;
/* Gravis Ultrasound */
extern const struct isapnp_devinfo isapnp_gus_devinfo;
/* Lance Ethernet */
extern const struct isapnp_devinfo isapnp_le_devinfo;
/* NE2000 Ethernet */
extern const struct isapnp_devinfo isapnp_ne_devinfo;
/* PCMCIA bridge */
extern const struct isapnp_devinfo isapnp_pcic_devinfo;
/* Creative Soundblaster */
extern const struct isapnp_devinfo isapnp_sb_devinfo;
/* Western Digital Disk Controller */
extern const struct isapnp_devinfo isapnp_wdc_devinfo;
/* Microsoft Sound System */
extern const struct isapnp_devinfo isapnp_wss_devinfo;
/* Yamaha Sound */
extern const struct isapnp_devinfo isapnp_ym_devinfo;
