/*	$NetBSD: cardbusdevs.h,v 1.8.2.4 2002/12/15 15:50:01 he Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: cardbusdevs,v 1.10.2.4 2002/12/15 15:49:34 he Exp 
 */

/*
 * Copyright (C) 1999  Hayakawa Koichi.
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
 *      This product includes software developed by the author
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
 * This code is stolen from sys/dev/pci/pcidevs.
 */


/*
 * List of known CardBus vendors
 */

#define	CARDBUS_VENDOR_DEC	0x1011		/* Digital Equipment */
#define	CARDBUS_VENDOR_3COM	0x10B7		/* 3Com */
#define	CARDBUS_VENDOR_ADP	0x9004		/* Adaptec */
#define	CARDBUS_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	CARDBUS_VENDOR_COREGA	0x1259		/* Corega */
#define	CARDBUS_VENDOR_ALLIEDTELESYN	0x1259		/* Allied Telesyn International */
#define	CARDBUS_VENDOR_ADP2	0x9005		/* Adaptec (2nd PCI Vendor ID) */
#define	CARDBUS_VENDOR_OPTI	0x1045		/* Opti */
#define	CARDBUS_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	CARDBUS_VENDOR_ACCTON	0x1113		/* Accton Technology */
#define	CARDBUS_VENDOR_ABOCOM	0x13d1		/* AboCom Systems */
#define	CARDBUS_VENDOR_PLANEX	0x14ea		/* Planex Communications Inc */
#define	CARDBUS_VENDOR_REALTEK	0x10ec		/* Realtek Semiconductor */
#define	CARDBUS_VENDOR_INTEL	0x8086		/* Intel */
#define	CARDBUS_VENDOR_INVALID	0xffff		/* INVALID VENDOR ID */

/*
 * List of known products.  Grouped by vendor.
 */

/* 3COM Products */

#define	CARDBUS_PRODUCT_3COM_3C575TX	0x5057		/* 3c575-TX */
#define	CARDBUS_PRODUCT_3COM_3C575BTX	0x5157		/* 3c575B-TX */
#define	CARDBUS_PRODUCT_3COM_3C575CTX	0x5257		/* 3c575C-TX */

/* AboCom products */
#define	CARDBUS_PRODUCT_ABOCOM_FE2500	0xab02		/* FE2500 10/100 Ethernet */
#define	CARDBUS_PRODUCT_ABOCOM_PCM200	0xab03		/* FE2500 10/100 Ethernet */

/* Planex products */
#define	CARDBUS_PRODUCT_PLANEX_FNW_3603_TX	0xab06		/* FNW-3603-TX 10/100 Ethernet */

/* Adaptec products */
#define	CARDBUS_PRODUCT_ADP_1480	0x6075		/* APA-1480 */

/* Accton products */
#define	CARDBUS_PRODUCT_ACCTON_MPX5030	0x1211		/* MPX 5030/5038 Ethernet */
#define	CARDBUS_PRODUCT_ACCTON_EN2242	0x1216		/* EN2242 10/100 Ethernet */

/* DEC products */
#define	CARDBUS_PRODUCT_DEC_21142	0x0019		/* DECchip 21142/3 */

/* Intel products */
#define	CARDBUS_PRODUCT_INTEL_82557	0x1229		/* 82557 Fast Ethernet LAN Controller */
/* XXX product name? */
#define	CARDBUS_PRODUCT_INTEL_MODEM56	0x1002		/* 56k Modem */

/* Opti products */
#define	CARDBUS_PRODUCT_OPTI_82C861	0xc861		/* 82C861 USB Host Controller (OHCI) */

/* Xircom products */
/* is the `-3' here just indicating revision 3, or is it really part
   of the device name? */
#define	CARDBUS_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 Fast Ethernet Controller */
/* this is the device id `indicating 21143 driver compatibility' */
#define	CARDBUS_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 Fast Ethernet Controller (21143) */
#define	CARDBUS_PRODUCT_XIRCOM_MODEM56	0x0103		/* 56k Modem */

/* D-Link products */
#define	CARDBUS_PRODUCT_DLINK_DFE_690TXD	0x1340		/* DFE-690TXD 10/100 Ethernet */

/* Corega products */
#define	CARDBUS_PRODUCT_COREGA_CB_TXD	0xa117		/* FEther CB-TXD 10/100 Ethernet */

/* Realtek (Creative Labs?) products */
#define	CARDBUS_PRODUCT_REALTEK_RT8138	0x8138		/* 8138 Ethernet */
#define	CARDBUS_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 Ethernet */
