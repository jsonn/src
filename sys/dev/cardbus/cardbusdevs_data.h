/*	$NetBSD: cardbusdevs_data.h,v 1.16.2.2 2002/03/16 16:00:50 jdolecek Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: cardbusdevs,v 1.21 2002/02/17 21:20:46 augustss Exp 
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

struct cardbus_knowndev {
	u_int32_t vendorid;
	u_int32_t deviceid;
	int flags;
	char *vendorname;
	char *devicename;
};

#define CARDBUS_KNOWNDEV_NOPROD 0x01

struct cardbus_knowndev cardbus_knowndevs[] = {
	{
	    CARDBUS_VENDOR_DEC, CARDBUS_PRODUCT_DEC_21142,
	    0,
	    "Digital Equipment",
	    "DECchip 21142/3",
	},
	{
	    CARDBUS_VENDOR_OPTI, CARDBUS_PRODUCT_OPTI_82C861,
	    0,
	    "Opti",
	    "82C861 USB Host Controller (OHCI)",
	},
	{
	    CARDBUS_VENDOR_HITACHI, CARDBUS_PRODUCT_HITACHI_SWC,
	    0,
	    "Hitachi",
	    "MSVCC01/02/03/04 Video Capture Cards",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575TX,
	    0,
	    "3Com",
	    "3c575-TX",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575BTX,
	    0,
	    "3Com",
	    "3CCFE575BT",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575CTX,
	    0,
	    "3Com",
	    "3CCFE575CT",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C656_E,
	    0,
	    "3Com",
	    "3CCFEM656 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C656B_E,
	    0,
	    "3Com",
	    "3CCFEM656B 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C656C_E,
	    0,
	    "3Com",
	    "3CXFEM656C 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_ADVSYS, CARDBUS_PRODUCT_ADVSYS_ULTRA,
	    0,
	    "Advanced System Products",
	    "Ultra SCSI",
	},
	{
	    CARDBUS_VENDOR_REALTEK, CARDBUS_PRODUCT_REALTEK_RT8138,
	    0,
	    "Realtek Semiconductor",
	    "8138 Ethernet",
	},
	{
	    CARDBUS_VENDOR_REALTEK, CARDBUS_PRODUCT_REALTEK_RT8139,
	    0,
	    "Realtek Semiconductor",
	    "8139 Ethernet",
	},
	{
	    CARDBUS_VENDOR_IODATA, CARDBUS_PRODUCT_IODATA_CBIDE2,
	    0,
	    "IO Data",
	    "CBIDE2 IDE controller",
	},
	{
	    CARDBUS_VENDOR_ACCTON, CARDBUS_PRODUCT_ACCTON_MPX5030,
	    0,
	    "Accton Technology",
	    "MPX 5030/5038 Ethernet",
	},
	{
	    CARDBUS_VENDOR_ACCTON, CARDBUS_PRODUCT_ACCTON_EN2242,
	    0,
	    "Accton Technology",
	    "EN2242 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_XIRCOM, CARDBUS_PRODUCT_XIRCOM_X3201_3,
	    0,
	    "Xircom",
	    "X3201-3 Fast Ethernet Controller",
	},
	{
	    CARDBUS_VENDOR_XIRCOM, CARDBUS_PRODUCT_XIRCOM_X3201_3_21143,
	    0,
	    "Xircom",
	    "X3201-3 Fast Ethernet Controller (21143)",
	},
	{
	    CARDBUS_VENDOR_XIRCOM, CARDBUS_PRODUCT_XIRCOM_MODEM56,
	    0,
	    "Xircom",
	    "56k Modem",
	},
	{
	    CARDBUS_VENDOR_DLINK, CARDBUS_PRODUCT_DLINK_DFE_690TXD,
	    0,
	    "D-Link Systems",
	    "DFE-690TXD 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_COREGA, CARDBUS_PRODUCT_COREGA_CB_TXD,
	    0,
	    "Corega",
	    "FEther CB-TXD 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_ABOCOM, CARDBUS_PRODUCT_ABOCOM_FE2500,
	    0,
	    "AboCom Systems",
	    "FE2500 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_ABOCOM, CARDBUS_PRODUCT_ABOCOM_PCM200,
	    0,
	    "AboCom Systems",
	    "FE2500 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_HAWKING, CARDBUS_PRODUCT_HAWKING_PN672TX,
	    0,
	    "Hawking Technology",
	    "PN672TX 10/100 Ethernet",
	},
	{
	    CARDBUS_VENDOR_INTEL, CARDBUS_PRODUCT_INTEL_82557,
	    0,
	    "Intel",
	    "82557 Fast Ethernet LAN Controller",
	},
	{
	    CARDBUS_VENDOR_INTEL, CARDBUS_PRODUCT_INTEL_MODEM56,
	    0,
	    "Intel",
	    "56k Modem",
	},
	{
	    CARDBUS_VENDOR_ADP, CARDBUS_PRODUCT_ADP_1480,
	    0,
	    "Adaptec",
	    "APA-1480",
	},
	{
	    CARDBUS_VENDOR_DEC, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Digital Equipment",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_OPTI, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Opti",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_HITACHI, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Hitachi",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_3COM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "3Com",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADVSYS, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Advanced System Products",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_REALTEK, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Realtek Semiconductor",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_IODATA, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "IO Data",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ACCTON, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Accton Technology",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_XIRCOM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Xircom",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_DLINK, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "D-Link Systems",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_COREGA, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Corega",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ALLIEDTELESYN, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Allied Telesyn International",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ABOCOM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "AboCom Systems",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_HAWKING, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Hawking Technology",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_INTEL, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Intel",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP2, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec (2nd PCI Vendor ID)",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_INVALID, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "INVALID VENDOR ID",
	    NULL,
	},
	{ 0, 0, 0, NULL, NULL, }
};
