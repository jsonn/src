/*	$NetBSD: pica.h,v 1.1.2.2 2001/01/05 17:33:59 bouyer Exp $	*/
/*	$OpenBSD: pica.h,v 1.4 1996/09/14 15:58:28 pefo Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef	_PICA_H_
#define	_PICA_H_ 1

/*
 * PICA's Physical address space
 */

#define PICA_PHYS_MIN		0x00000000	/* 256 Meg */
#define PICA_PHYS_MAX		0x0fffffff

/*
 * Memory map
 */

#define PICA_PHYS_MEMORY_START	0x00000000
#define PICA_PHYS_MEMORY_END	0x0fffffff	/* 256 Meg in 8 slots */

#define PICA_MEMORY_SIZE_REG	0xe00fffe0	/* Memory size register */
#define	PICA_CONFIG_REG		0xe00ffff0	/* Hardware config reg  */

/*
 * I/O map
 */

#define	R4030_P_LOCAL_IO_BASE	0x80000000	/* I/O Base address */
#define	R4030_V_LOCAL_IO_BASE	0xe0000000
#define	R4030_S_LOCAL_IO_BASE	0x00040000	/* Size */
#define R4030 R4030_V_LOCAL_IO_BASE

#define	R4030_SYS_CONFIG	(R4030+0x0000)	/* Global config register */
#define	R4030_SYS_TL_BASE	(R4030+0x0018)	/* DMA transl. table base */
#define	R4030_SYS_TL_LIMIT	(R4030+0x0020)	/* DMA transl. table limit */
#define	R4030_SYS_TL_IVALID	(R4030+0x0028)	/* DMA transl. cache inval */
#define	R4030_SYS_DMA0_REGS	(R4030+0x0100)	/* DMA ch0 base address */
#define	R4030_SYS_DMA1_REGS	(R4030+0x0120)	/* DMA ch0 base address */
#define	R4030_SYS_DMA2_REGS	(R4030+0x0140)	/* DMA ch0 base address */
#define	R4030_SYS_DMA3_REGS	(R4030+0x0160)	/* DMA ch0 base address */
#define	R4030_SYS_DMA_INT_SRC	(R4030+0x0200)	/* DMA int source status reg */
#define	R4030_SYS_NVRAM_PROT	(R4030+0x0220)	/* NV ram protect register */
#define	R4030_SYS_IT_VALUE	(R4030+0x0228)	/* Interval timer reload */
#define	R4030_SYS_IT_STAT	(R4030+0x0230)	/* Interval timer count */
#define	R4030_SYS_ISA_VECTOR	(R4030+0x0238)	/* ISA Interrupt vector */
#define	R4030_SYS_EXT_IMASK	(R4030+0x00e8)	/* External int enable mask */

#define PVLB R4030_V_LOCAL_IO_BASE
#define	PICA_SYS_SONIC		(PVLB+0x1000)	/* SONIC base address */
#define	PICA_SYS_SCSI		(PVLB+0x2000)	/* SCSI base address */
#define	PICA_SYS_FLOPPY		(PVLB+0x3000)	/* Floppy base address */
#define	PICA_SYS_CLOCK		(PVLB+0x4000)	/* Clock base address */
#define	PICA_SYS_KBD		(PVLB+0x5000)	/* Keybrd/mouse base address */
#define	PICA_SYS_COM1		(PVLB+0x6000)	/* Com port 1 */
#define	PICA_SYS_COM2		(PVLB+0x7000)	/* Com port 2 */
#define	PICA_SYS_PAR1		(PVLB+0x8000)	/* Parallel port 1 */
#define	PICA_SYS_NVRAM		(PVLB+0x9000)	/* Unprotected NV-ram */
#define	PICA_SYS_PNVRAM		(PVLB+0xa000)	/* Protected NV-ram */
#define	PICA_SYS_NVPROM		(PVLB+0xb000)	/* Read only NV-ram */
#define	PICA_SYS_SOUND		(PVLB+0xc000)	/* Sound port */

#define	PICA_SYS_ISA_AS		(PICA_V_ISA_IO+0x70)

#define	PICA_P_DRAM_CONF	0x800e0000	/* Dram config registers */
#define	PICA_V_DRAM_CONF	0xe00e0000
#define	PICA_S_DRAM_CONF	0x00020000

#define	PICA_P_INT_SOURCE	0xf0000000	/* Interrupt src registers */
#define	PICA_V_INT_SOURCE	R4030_V_LOCAL_IO_BASE+R4030_S_LOCAL_IO_BASE
#define	PICA_S_INT_SOURCE	0x00001000
#define PVIS PICA_V_INT_SOURCE
#define	PICA_SYS_LB_IS		(PVIS+0x0000)	/* Local bus int source */
#define	PICA_SYS_LB_IE		(PVIS+0x0002)	/* Local bus int enables */
#define PICA_SYS_LB_IE_PAR1	0x0001		/* Parallel port enable */
#define	PICA_SYS_LB_IE_FLOPPY	0x0002		/* Floppy ctrl enable */
#define	PICA_SYS_LB_IE_SOUND	0x0004		/* Sound port enable */
#define	PICA_SYS_LB_IE_VIDEO	0x0008		/* Video int enable */
#define	PICA_SYS_LB_IE_SONIC	0x0010		/* Ethernet ctrl enable */
#define	PICA_SYS_LB_IE_SCSI	0x0020		/* Scsi crtl enable */
#define PICA_SYS_LB_IE_KBD	0x0040		/* Keyboard ctrl enable */
#define PICA_SYS_LB_IE_MOUSE	0x0080		/* Mouse ctrl enable */
#define	PICA_SYS_LB_IE_COM1	0x0100		/* Serial port 1 enable */
#define	PICA_SYS_LB_IE_COM2	0x0200		/* Serial port 2 enable */

#define	PICA_P_LOCAL_VIDEO_CTRL	0x60000000	/* Local video control */
#define	PICA_V_LOCAL_VIDEO_CTRL	0xe0200000
#define	PICA_S_LOCAL_VIDEO_CTRL	0x00200000

#define	PICA_P_EXTND_VIDEO_CTRL	0x60200000	/* Extended video control */
#define	PICA_V_EXTND_VIDEO_CTRL	0xe0400000
#define	PICA_S_EXTND_VIDEO_CTRL	0x00200000

#define	PICA_P_LOCAL_VIDEO	0x40000000	/* Local video memory */
#define	PICA_V_LOCAL_VIDEO	0xe0800000
#define	PICA_S_LOCAL_VIDEO	0x00800000

#define	PICA_P_ISA_IO		0x90000000	/* ISA I/O control */
#define	PICA_V_ISA_IO		0xe2000000
#define	PICA_S_ISA_IO		0x01000000

#define	PICA_P_ISA_MEM		0x91000000	/* ISA Memory control */
#define	PICA_V_ISA_MEM		0xe3000000
#define	PICA_S_ISA_MEM		0x01000000

/*
 *  Addresses used by various display drivers.
 */
#define PICA_MONO_BASE	(PICA_V_LOCAL_VIDEO_CTRL + 0x3B4)
#define PICA_MONO_BUF	(PICA_V_LOCAL_VIDEO + 0xB0000)
#define PICA_CGA_BASE	(PICA_V_LOCAL_VIDEO_CTRL + 0x3D4)
#define PICA_CGA_BUF	(PICA_V_LOCAL_VIDEO + 0xB8000)

/*
 *  Interrupt vector descriptor for device on pica bus.
 */
struct pica_int_desc {
	int		int_mask;	/* Mask used in PICA_SYS_LB_IE */
	intr_handler_t	int_hand;	/* Interrupt handler */
	void		*param;		/* Parameter to send to handler */
	int		spl_mask;	/* Spl mask for interrupt */
};

int	pica_intrnull __P((void *));
#endif	/* _PICA_H_ */
