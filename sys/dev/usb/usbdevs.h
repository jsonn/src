/*	$NetBSD: usbdevs.h,v 1.54.4.1 1999/11/15 00:41:40 fvdl Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: usbdevs,v 1.60 1999/11/12 00:26:13 augustss Exp 
 */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * List of known USB vendors
 */

#define	USB_VENDOR_HP	0x03f0		/* Hewlett Packard */
#define	USB_VENDOR_NEC	0x0409		/* NEC */
#define	USB_VENDOR_KODAK	0x040a		/* Eastman Kodak Corp. */
#define	USB_VENDOR_CATC	0x0423		/* Computer Access Technology Corp. */
#define	USB_VENDOR_GRAVIS	0x0428		/* Advanced Gravis Computer Tech. Ltd. */
#define	USB_VENDOR_LEXMARK	0x043d		/* Lexmark International Inc. */
#define	USB_VENDOR_NANAO	0x0440		/* NANAO Corp. */
#define	USB_VENDOR_THRUST	0x044f		/* Thrustmaster */
#define	USB_VENDOR_TI	0x0451		/* Texas Instruments */
#define	USB_VENDOR_KYE	0x0458		/* KYE Systems Corp. */
#define	USB_VENDOR_MICROSOFT	0x045e		/* Microsoft */
#define	USB_VENDOR_PRIMAX	0x0461		/* Primax Electronics */
#define	USB_VENDOR_CHERRY	0x046a		/* Cherry Mikroschalter GmbH */
#define	USB_VENDOR_LOGITECH	0x046d		/* Logitech Inc. */
#define	USB_VENDOR_BTC	0x046e		/* Behavior Tech. Computer */
#define	USB_VENDOR_PHILIPS	0x0471		/* Philips */
#define	USB_VENDOR_CONNECTIX	0x0478		/* Connectix Corp. */
#define	USB_VENDOR_LUCENT	0x047e		/* Lucent */
#define	USB_VENDOR_STMICRO	0x0483		/* STMicroelectronics */
#define	USB_VENDOR_ACER	0x04a5		/* Acer Peripheral Inc. */
#define	USB_VENDOR_CANON	0x04a9		/* Canon Inc. */
#define	USB_VENDOR_CYPRESS	0x04b4		/* Cypress Semiconductor */
#define	USB_VENDOR_EPSON	0x04b8		/* Seiko Epson Corp. */
#define	USB_VENDOR_3COM	0x04c1		/* U.S. Robotics */
#define	USB_VENDOR_KONICA	0x04c8		/* Konica Corp. */
#define	USB_VENDOR_ALTEC	0x04d2		/* Altec Lansing */
#define	USB_VENDOR_SHUTTLE	0x04e6		/* Shuttle Technology */
#define	USB_VENDOR_CHICONY	0x04f2		/* Chicony Electronics Co., Ltd. */
#define	USB_VENDOR_BROTHER	0x04f9		/* Brother Industries Corp. */
#define	USB_VENDOR_DALLAS	0x04fa		/* Dallas Semiconductor */
#define	USB_VENDOR_BELKIN	0x050d		/* Belkin Components */
#define	USB_VENDOR_KAWATSU	0x050f		/* Kawatsu Semiconductor, Inc. */
#define	USB_VENDOR_APC	0x051d		/* American Power Conversion */
#define	USB_VENDOR_AKS	0x0529		/* Fast Security AG */
#define	USB_VENDOR_UNIACCESS	0x0540		/* Universal Access */
#define	USB_VENDOR_ANCHOR	0x0547		/* Anchor Chips Inc. */
#define	USB_VENDOR_VISION	0x0553		/* VLSI Vision Ltd. */
#define	USB_VENDOR_ATEN	0x0557		/* ATEN International Corp. Ltd. */
#define	USB_VENDOR_MUSTEK	0x055f		/* Mustek Systems Inc. */
#define	USB_VENDOR_TELEX	0x0562		/* Telex Communications Inc. */
#define	USB_VENDOR_PERACOM	0x0565		/* Peracom Networks Inc. */
#define	USB_VENDOR_WACOM	0x056a		/* WACOM Corp. Ltd. */
#define	USB_VENDOR_ETEK	0x056c		/* e-TEK Labs */
#define	USB_VENDOR_EIZO	0x056d		/* EIZO */
#define	USB_VENDOR_ELECOM	0x056e		/* Elecom Corp. Ltd. */
#define	USB_VENDOR_ROCKFIRE	0x0583		/* Rockfire */
#define	USB_VENDOR_IOMEGA	0x059b		/* Iomega Corp. */
#define	USB_VENDOR_OMNIVISION	0x05a9		/* OmniVision */
#define	USB_VENDOR_INSYSTEM	0x05ab		/* In-System Design */
#define	USB_VENDOR_APPLE	0x05ac		/* Apple Computer */
#define	USB_VENDOR_QTRONIX	0x05c7		/* Qtronix Corp */
#define	USB_VENDOR_ELSA	0x05cc		/* ELSA Gmbh */
#define	USB_VENDOR_EIZONANAO	0x05e7		/* EIZO Nanao */
#define	USB_VENDOR_PIENGINEERING	0x05f3		/* P.I. Engineering */
#define	USB_VENDOR_CHIC	0x05fe		/* Chic Technology */
#define	USB_VENDOR_MACALLY	0x0618		/* Macally */
#define	USB_VENDOR_MULTITECH	0x06e0		/* MultiTech */
#define	USB_VENDOR_ADS	0x06e1		/* ADS Technologies */
#define	USB_VENDOR_MIDIMAN	0x0763		/* Midiman */
#define	USB_VENDOR_SANDISK	0x0781		/* SanDisk Corp */
#define	USB_VENDOR_SIIG	0x07cc		/* SIIG */
#define	USB_VENDOR_HANDSPRING	0x082d		/* Handspring Inc. */
#define	USB_VENDOR_ACTIVEWIRE	0x0854		/* ActiveWire Inc. */
#define	USB_VENDOR_MOTOROLA	0x1063		/* Motorola */
#define	USB_VENDOR_PLX	0x10b5		/* PLX */
#define	USB_VENDOR_INSIDEOUT	0x1608		/* Inside Out Networks */
#define	USB_VENDOR_ENTREGA	0x1645		/* Entrega */
#define	USB_VENDOR_INTEL	0x8086		/* Intel */

/*
 * List of known products.  Grouped by vendor.
 */

/* HP products */
#define	USB_PRODUCT_HP_4100C	0x0101		/* Scanjet 4100C */
#define	USB_PRODUCT_HP_6300C	0x0601		/* Scanjet 6300C */

/* NEC products */
#define	USB_PRODUCT_NEC_HUB	0x55aa		/* hub */
#define	USB_PRODUCT_NEC_HUB_B	0x55ab		/* hub */

/* Kodak products */
#define	USB_PRODUCT_KODAK_DC260	0x0110		/* Digital Science DC260 */
#define	USB_PRODUCT_KODAK_DC240	0x0120		/* Digital Science DC240 */
#define	USB_PRODUCT_KODAK_DC280	0x0130		/* Digital Science DC280 */

/* CATC products */
#define	USB_PRODUCT_CATC_ANDROMEDA	0x1237		/* Andromeda hub */

/* Gravis products */
#define	USB_PRODUCT_GRAVIS_GAMEPADPRO	0x4001		/* GamePad Pro */

/* Lexmark products */
#define	USB_PRODUCT_LEXMARK_S2450	0x0009		/* Optra S 2450 */

/* Thrustmaster products */
#define	USB_PRODUCT_THRUST_FUSION_PAD	0xa0a3		/* Fusion Digital Gamepad */

/* Texas Intel products */
#define	USB_PRODUCT_TI_UTUSB41	0x1446		/* UT-USB41 hub */

/* (KYE) Genius products */
#define	USB_PRODUCT_KYE_NICHE	0x0001		/* Niche mouse */
#define	USB_PRODUCT_KYE_FLIGHT2000	0x1004		/* Flight 2000 joystick */

/* Microsoft products */
#define	USB_PRODUCT_MICROSOFT_INTELLIMOUSE	0x0009		/* IntelliMouse */
#define	USB_PRODUCT_MICROSOFT_NATURALKBD	0x000b		/* Natural Keyboard Elite */
#define	USB_PRODUCT_MICROSOFT_DDS80	0x0014		/* Digital Sound System 80 */

/* Primax products */
#define	USB_PRODUCT_PRIMAX_COMFORT	0x4d01		/* Comfort */
#define	USB_PRODUCT_PRIMAX_MOUSEINABOX	0x4d02		/* Mouse-in-a-Box */

/* Cherry products */
#define	USB_PRODUCT_CHERRY_MY3000KBD	0x0001		/* My3000 keyboard */
#define	USB_PRODUCT_CHERRY_MY3000HUB	0x0003		/* My3000 hub */

/* Behavior Technology Computer products */
#define	USB_PRODUCT_BTC_BTC7932	0x6782		/* Keyboard with mouse port */

/* Philips products */
#define	USB_PRODUCT_PHILIPS_DSS350	0x0101		/* DSS 350 Digital Speaker System */
#define	USB_PRODUCT_PHILIPS_DSS	0x0104		/* DSS XXX Digital Speaker System */
#define	USB_PRODUCT_PHILIPS_HUB	0x0201		/* hub */
#define	USB_PRODUCT_PHILIPS_DSS150	0x0471		/* DSS XXX Digital Speaker System */

/* Connectix products */
#define	USB_PRODUCT_CONNECTIX_QUICKCAM	0x0001		/* QuickCam */

/* Lucent products */
#define	USB_PRODUCT_LUCENT_EVALKIT	0x1001		/* USS-720 evaluation kit */

/* STMicroelectronics products */
#define	USB_PRODUCT_STMICRO_COMMUNICATOR	0x7554		/* USB Communicator */

/* Acer products */
#define	USB_PRODUCT_ACER_ACERSCAN_C310U	0x12a6		/* Acerscan C310U */

/* Canon Inc. products */
#define	USB_PRODUCT_CANON_S10	0x3041		/* PowerShot S10 */

/* Cypress Semiconductor products */
#define	USB_PRODUCT_CYPRESS_MOUSE	0x0001		/* mouse */
#define	USB_PRODUCT_CYPRESS_THERMO	0x0002		/* thermometer */

/* Epson products */
#define	USB_PRODUCT_EPSON_PRINTER3	0x0003		/* ISD USB Smart Cable */

/* 3Com products */
#define	USB_PRODUCT_3COM_USR56K	0x3021		/* U.S.Robotics 56000 Voice Faxmodem Pro */

/* Konica Corp. Products */
#define	USB_PRODUCT_KONICA_CAMERA	0x0720		/* Digital Color Camera */

/* Altec Lansing products */
#define	USB_PRODUCT_ALTEC_ASC495	0xff05		/* ASC495 Speakers */

/* Shuttle Technology products */
#define	USB_PRODUCT_SHUTTLE_EUSB	0x0001		/* E-USB Bridge */

/* Chicony products */
#define	USB_PRODUCT_CHICONY_KB8933	0x0001		/* KB-8933 keyboard */

/* Brother Industries products */
#define	USB_PRODUCT_BROTHER_HL1050	0x0002		/* HL-1050 laser printer */

/* Dallas Semiconductor products */
#define	USB_PRODUCT_DALLAS_J6502	0x4201		/* J-6502 speakers */

/* Kawatsu products */
#define	USB_PRODUCT_KAWATSU_MH4000P	0x0003		/* MiniHub 4000P */

/* American Power Conversion products */
#define	USB_PRODUCT_APC_UPSPRO500	0x0002		/* Back-UPS Pro 500 */

/* AKS products */
#define	USB_PRODUCT_AKS_USBHASP	0x0001		/* USB-HASP 0.06 */

/* Universal Access products */
#define	USB_PRODUCT_UNIACCESS_PANACHE	0x0101		/* Panache Surf USB ISDN Adapter */

/* Anchor products */
#define	USB_PRODUCT_ANCHOR_EZUSB	0x2131		/* EZUSB */

/* Vision products */
#define	USB_PRODUCT_VISION_VC6452V002	0x0002		/* VC6452V002 Camera */

/* ATen products */
#define	USB_PRODUCT_ATEN_UC1284	0x2001		/* Parallel printer adapter */

/* Mustek products */
#define	USB_PRODUCT_MUSTEK_MDC800	0xa800		/* MDC-800 digital camera */

/* Telex Communications products */
#define	USB_PRODUCT_TELEX_MIC1	0x0001		/* Enhanced USB Microphone */

/* Peracom products */
#define	USB_PRODUCT_PERACOM_SERIAL1	0x0001		/* Serial Converter */

/* Wacom products */
#define	USB_PRODUCT_WACOM_CT0405U	0x0000		/* CT-0405-U Tablet */

/* e-TEK Labs products */
#define	USB_PRODUCT_ETEK_1COM	0x8007		/* Serial port */

/* EIZO products */
#define	USB_PRODUCT_EIZO_HUB	0x0000		/* hub */
#define	USB_PRODUCT_EIZO_MONITOR	0x0001		/* monitor */

/* Agiler products */
#define	USB_PRODUCT_ELECOM_MOUSE29UO	0x0002		/* mouse 29UO */

/* Rockfire products */
#define	USB_PRODUCT_ROCKFIRE_GAMEPAD	0x2033		/* gamepad 203USB */

/* Iomega products */
#define	USB_PRODUCT_IOMEGA_ZIP100	0x0001		/* Zip 100 */

/* OmniVision Technologies Inc. products */
#define	USB_PRODUCT_OMNIVISION_OV511	0x0511		/* OV511 Camera */

/* In-System products */
#define	USB_PRODUCT_INSYSTEM_F5U002	0x0002		/* Parallel printer adapter */
#define	USB_PRODUCT_INSYSTEM_ISD110	0x0200		/* IDE adapter */

/* Qtronix products */
#define	USB_PRODUCT_QTRONIX_980N	0x2011		/* Scorpion-980N keyboard */

/* Elsa products */
#define	USB_PRODUCT_ELSA_MODEM1	0x2265		/* ELSA Modem Board */

/* Logitech products */
#define	USB_PRODUCT_LOGITECH_M2452	0x0203		/* M2452 keyboard */
#define	USB_PRODUCT_LOGITECH_M4848	0x0301		/* M4848 mouse */
#define	USB_PRODUCT_LOGITECH_QUICKCAM	0x0801		/* QuickCam */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRO	0x0810		/* QuickCam Pro */
#define	USB_PRODUCT_LOGITECH_N48	0xc001		/* N48 mouse */
#define	USB_PRODUCT_LOGITECH_MBA47	0xc002		/* M-BA47 mouse */

/* P.I. Engineering products */
#define	USB_PRODUCT_PIENGINEERING_PS2USB	0x020b		/* PS2 to Mac USB Adapter */

/* Chic Technology products */
#define	USB_PRODUCT_CHIC_MOUSE1	0x0001		/* mouse */

/* Macally products */
#define	USB_PRODUCT_MACALLY_MOUSE1	0x0101		/* mouse */

/* MultiTech products */
#define	USB_PRODUCT_MULTITECH_ATLAS	0xf101		/* MT5634ZBA-USB modem */

/* ADS products */
#define	USB_PRODUCT_ADS_ENET	0x0008		/* Ethernet adapter */

/* Entrega products */
#define	USB_PRODUCT_ENTREGA_1S	0x0001		/* 1S serial connector */
#define	USB_PRODUCT_ENTREGA_2S	0x0002		/* 2S serial connector */
#define	USB_PRODUCT_ENTREGA_1S25	0x0003		/* 1S25 serial connector */
#define	USB_PRODUCT_ENTREGA_4S	0x0004		/* 4S serial connector */
#define	USB_PRODUCT_ENTREGA_CENTRONICS	0x0006		/* Centronics connector */
#define	USB_PRODUCT_ENTREGA_1S9	0x0093		/* 1S9 serial connector */
#define	USB_PRODUCT_ENTREGA_EZUSB	0x8000		/* EZ-USB */
#define	USB_PRODUCT_ENTREGA_SERIAL	0x8001		/* DB25 Serial connector */
#define	USB_PRODUCT_ENTREGA_SERIAL_DB9	0x8093		/* DB9 Serial connector */

/* Midiman products */
#define	USB_PRODUCT_MIDIMAN_MIDISPORT2X2	0x1001		/* Midisport 2x2 */

/* SanDisk products */
#define	USB_PRODUCT_SANDISK_IMAGEMATE	0x0001		/* USB ImageMate */

/* SIIG products */
#define	USB_PRODUCT_SIIG_DIGIFILMREADER	0x0004		/* DigiFilm-Combo Reader */

/* Handspring Inc. */
#define	USB_PRODUCT_HANDSPRING_VISOR	0x0100		/* Handspring Visor */

/* ActiveWire Inc. products */
#define	USB_PRODUCT_ACTIVEWIRE_IOBOARD	0x0100		/* I/O Board */
#define	USB_PRODUCT_ACTIVEWIRE_IOBOARD_FW1	0x0101		/* I/O Board, rev. 1 firmware */

/* Motorola products */
#define	USB_PRODUCT_MOTOROLA_MC141555	0x1555		/* MC141555 hub controller */

/* PLX products */
#define	USB_PRODUCT_PLX_TESTBOARD	0x9060		/* test board */

/* Inside Out Networks products */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT4	0x0001		/* EdgePort/4 serial ports */

/* Intel products */
#define	USB_PRODUCT_INTEL_TESTBOARD	0x9890		/* 82930 test board */
