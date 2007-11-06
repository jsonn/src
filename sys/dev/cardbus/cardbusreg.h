/*	$NetBSD: cardbusreg.h,v 1.1.98.1 2007/11/06 23:25:49 matt Exp $ */

/*
 * Copyright (c) 2001
 *       HAYAKAWA Koichi.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_CARDBUS_CARDBUSREG_H_
#define _DEV_CARDBUS_CARDBUSREG_H_

#include <dev/pci/pcivar.h>	/* for pcitag_t */

typedef u_int32_t cardbusreg_t;
typedef pcitag_t cardbustag_t;
typedef int cardbus_intr_line_t;

#define CARDBUS_ID_REG          0x00

typedef u_int16_t cardbus_vendor_id_t;
typedef u_int16_t cardbus_product_id_t;

#  define CARDBUS_VENDOR_SHIFT  0
#  define CARDBUS_VENDOR_MASK   0xffff
#  define CARDBUS_VENDOR(id) \
	    (((id) >> CARDBUS_VENDOR_SHIFT) & CARDBUS_VENDOR_MASK)

#  define CARDBUS_PRODUCT_SHIFT  16
#  define CARDBUS_PRODUCT_MASK   0xffff
#  define CARDBUS_PRODUCT(id) \
	    (((id) >> CARDBUS_PRODUCT_SHIFT) & CARDBUS_PRODUCT_MASK)


#define	CARDBUS_COMMAND_STATUS_REG  0x04

#  define CARDBUS_COMMAND_IO_ENABLE          0x00000001
#  define CARDBUS_COMMAND_MEM_ENABLE         0x00000002
#  define CARDBUS_COMMAND_MASTER_ENABLE      0x00000004
#  define CARDBUS_COMMAND_SPECIAL_ENABLE     0x00000008
#  define CARDBUS_COMMAND_INVALIDATE_ENABLE  0x00000010
#  define CARDBUS_COMMAND_PALETTE_ENABLE     0x00000020
#  define CARDBUS_COMMAND_PARITY_ENABLE      0x00000040
#  define CARDBUS_COMMAND_STEPPING_ENABLE    0x00000080
#  define CARDBUS_COMMAND_SERR_ENABLE        0x00000100
#  define CARDBUS_COMMAND_BACKTOBACK_ENABLE  0x00000200


#define CARDBUS_CLASS_REG       0x08

#define	CARDBUS_CLASS_SHIFT				24
#define	CARDBUS_CLASS_MASK				0xff
#define	CARDBUS_CLASS(cr) \
	    (((cr) >> CARDBUS_CLASS_SHIFT) & CARDBUS_CLASS_MASK)

#define	CARDBUS_SUBCLASS_SHIFT			16
#define	CARDBUS_SUBCLASS_MASK			0xff
#define	CARDBUS_SUBCLASS(cr) \
	    (((cr) >> CARDBUS_SUBCLASS_SHIFT) & CARDBUS_SUBCLASS_MASK)

#define	CARDBUS_INTERFACE_SHIFT			8
#define	CARDBUS_INTERFACE_MASK			0xff
#define	CARDBUS_INTERFACE(cr) \
	    (((cr) >> CARDBUS_INTERFACE_SHIFT) & CARDBUS_INTERFACE_MASK)

#define	CARDBUS_REVISION_SHIFT			0
#define	CARDBUS_REVISION_MASK			0xff
#define	CARDBUS_REVISION(cr) \
	    (((cr) >> CARDBUS_REVISION_SHIFT) & CARDBUS_REVISION_MASK)

/* base classes */
#define	CARDBUS_CLASS_PREHISTORIC		0x00
#define	CARDBUS_CLASS_MASS_STORAGE		0x01
#define	CARDBUS_CLASS_NETWORK			0x02
#define	CARDBUS_CLASS_DISPLAY			0x03
#define	CARDBUS_CLASS_MULTIMEDIA		0x04
#define	CARDBUS_CLASS_MEMORY			0x05
#define	CARDBUS_CLASS_BRIDGE			0x06
#define	CARDBUS_CLASS_COMMUNICATIONS		0x07
#define	CARDBUS_CLASS_SYSTEM			0x08
#define	CARDBUS_CLASS_INPUT			0x09
#define	CARDBUS_CLASS_DOCK			0x0a
#define	CARDBUS_CLASS_PROCESSOR			0x0b
#define	CARDBUS_CLASS_SERIALBUS			0x0c
#define	CARDBUS_CLASS_UNDEFINED			0xff

/* 0x07 serial bus subclasses */
#define	CARDBUS_SUBCLASS_COMMUNICATIONS_SERIAL	0x00

/* 0x0c serial bus subclasses */
#define	CARDBUS_SUBCLASS_SERIALBUS_FIREWIRE	0x00
#define	CARDBUS_SUBCLASS_SERIALBUS_ACCESS	0x01
#define	CARDBUS_SUBCLASS_SERIALBUS_SSA		0x02
#define	CARDBUS_SUBCLASS_SERIALBUS_USB		0x03
#define	CARDBUS_SUBCLASS_SERIALBUS_FIBER	0x04

/* BIST, Header Type, Latency Timer, Cache Line Size */
#define CARDBUS_BHLC_REG        0x0c

#define	CARDBUS_BIST_SHIFT        24
#define	CARDBUS_BIST_MASK       0xff
#define	CARDBUS_BIST(bhlcr) \
	    (((bhlcr) >> CARDBUS_BIST_SHIFT) & CARDBUS_BIST_MASK)

#define	CARDBUS_HDRTYPE_SHIFT     16
#define	CARDBUS_HDRTYPE_MASK    0xff
#define	CARDBUS_HDRTYPE(bhlcr) \
	    (((bhlcr) >> CARDBUS_HDRTYPE_SHIFT) & CARDBUS_HDRTYPE_MASK)

#define	CARDBUS_HDRTYPE_TYPE(bhlcr) \
	    (CARDBUS_HDRTYPE(bhlcr) & 0x7f)
#define	CARDBUS_HDRTYPE_MULTIFN(bhlcr) \
	    ((CARDBUS_HDRTYPE(bhlcr) & 0x80) != 0)

#define	CARDBUS_LATTIMER_SHIFT      8
#define	CARDBUS_LATTIMER_MASK    0xff
#define	CARDBUS_LATTIMER(bhlcr) \
	    (((bhlcr) >> CARDBUS_LATTIMER_SHIFT) & CARDBUS_LATTIMER_MASK)

#define	CARDBUS_CACHELINE_SHIFT     0
#define	CARDBUS_CACHELINE_MASK   0xff
#define	CARDBUS_CACHELINE(bhlcr) \
	    (((bhlcr) >> CARDBUS_CACHELINE_SHIFT) & CARDBUS_CACHELINE_MASK)


/* Base Resisters */
#define CARDBUS_BASE0_REG  0x10
#define CARDBUS_BASE1_REG  0x14
#define CARDBUS_BASE2_REG  0x18
#define CARDBUS_BASE3_REG  0x1C
#define CARDBUS_BASE4_REG  0x20
#define CARDBUS_BASE5_REG  0x24
#define CARDBUS_CIS_REG    0x28
#define CARDBUS_ROM_REG	   0x30
#  define CARDBUS_CIS_ASIMASK 0x07
#    define CARDBUS_CIS_ASI(x) (CARDBUS_CIS_ASIMASK & (x))
#  define CARDBUS_CIS_ASI_TUPLE 0x00
#  define CARDBUS_CIS_ASI_BAR0  0x01
#  define CARDBUS_CIS_ASI_BAR1  0x02
#  define CARDBUS_CIS_ASI_BAR2  0x03
#  define CARDBUS_CIS_ASI_BAR3  0x04
#  define CARDBUS_CIS_ASI_BAR4  0x05
#  define CARDBUS_CIS_ASI_BAR5  0x06
#  define CARDBUS_CIS_ASI_ROM   0x07
#  define CARDBUS_CIS_ADDRMASK 0x0ffffff8
#    define CARDBUS_CIS_ADDR(x) (CARDBUS_CIS_ADDRMASK & (x))
#    define CARDBUS_CIS_ASI_BAR(x) (((CARDBUS_CIS_ASIMASK & (x))-1)*4+CARDBUS_BASE0_REG)
#    define CARDBUS_CIS_ASI_ROM_IMAGE(x) (((x) >> 28) & 0xf)

#define	CARDBUS_INTERRUPT_REG   0x3c

#define CARDBUS_MAPREG_TYPE_MEM		0x00000000
#define CARDBUS_MAPREG_TYPE_IO		0x00000001

#endif /* !_DEV_CARDBUS_CARDBUSREG_H_ */
