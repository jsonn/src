/*	$NetBSD: ne2000reg.h,v 1.2.16.1 2000/11/20 11:40:50 bouyer Exp $	*/

/*
 * National Semiconductor DS8390 NIC register definitions.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

#ifndef _DEV_IC_NE2000REG_H_
#define	_DEV_IC_NE2000REG_H_

/*
 * Register group offsets from base.
 */
#define	NE2000_NIC_OFFSET	0x00
#define	NE2000_ASIC_OFFSET	0x10

#define	NE2000_NIC_NPORTS	0x10
#define	NE2000_ASIC_NPORTS	0x10
#define	NE2000_NPORTS		0x20

/*
 * NE2000 ASIC registers (given as offsets from ASIC base).
 */
#define	NE2000_ASIC_DATA	0x00	/* remote DMA/data register */
#define	NE2000_ASIC_RESET	0x0f	/* reset on read */

/*
 * Offset of NODE ID in SRAM memory of ASIX AX88190.
 */
#define	NE2000_AX88190_NODEID_OFFSET	0x400

/*
 * Offset of LAN IOBASE0 and IOBASE1, and its size.
 */
#define NE2000_AX88190_LAN_IOBASE	0x3ca
#define NE2000_AX88190_LAN_IOSIZE	4

#endif /* _DEV_IC_NE2000REG_H_ */
