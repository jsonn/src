/*	$NetBSD: sysioreg.h,v 1.1.1.1.32.1 2003/01/07 21:23:36 thorpej Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1995 	Paul Kranenburg
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/*
 * sysio is the sun5/sun4u SBUS controller/DMA/IOMMU/etc. ASIC.
 */

struct sysioreg {
	struct upareg {
		u_int64_t	upa_portid;		/* UPA port ID register */		/* 1fe.0000.0000 */
		u_int64_t	upa_config;		/* UPA config register */		/* 1fe.0000.0008 */
	} upa;
	u_int64_t	sys_csr;		/* SYSIO control/status register */	/* 1fe.0000.0010 */
	u_int64_t	pad0;
	u_int64_t	sys_ecccr;		/* ECC control register */		/* 1fe.0000.0020 */
	u_int64_t	reserved;							/* 1fe.0000.0028 */
	u_int64_t	sys_ue_afsr;		/* Uncorrectable Error AFSR */		/* 1fe.0000.0030 */
	u_int64_t	sys_ue_afar;		/* Uncorrectable Error AFAR */		/* 1fe.0000.0038 */
	u_int64_t	sys_ce_afsr;		/* Correctable Error AFSR */		/* 1fe.0000.0040 */
	u_int64_t	sys_ce_afar;		/* Correctable Error AFAR */		/* 1fe.0000.0048 */
	char		pad1[0x2000 - 0x50];
	struct sbusreg {
		u_int64_t	sys_sbus_cr;		/* SBUS Control Register */		/* 1fe.0000.2000 */
		u_int64_t	reserved;							/* 1fe.0000.2008 */
		u_int64_t	sys_sbus_afsr;		/* SBUS AFSR */				/* 1fe.0000.2010 */
		u_int64_t	sys_sbus_afar;		/* SBUS AFAR */				/* 1fe.0000.2018 */
		u_int64_t	sys_sbus_config0;	/* SBUS Slot 0 config register */	/* 1fe.0000.2020 */
		u_int64_t	sys_sbus_config1;	/* SBUS Slot 1 config register */	/* 1fe.0000.2028 */
		u_int64_t	sys_sbus_config2;	/* SBUS Slot 2 config register */	/* 1fe.0000.2030 */
		u_int64_t	sys_sbus_config3;	/* SBUS Slot 3 config register */	/* 1fe.0000.2038 */
		u_int64_t	sys_sbus_config13;	/* Slot 13 config register <audio> */	/* 1fe.0000.2040 */
		u_int64_t	sys_sbus_config14;	/* Slot 14 config register <Macio> */	/* 1fe.0000.2048 */
		u_int64_t	sys_sbus_config15;	/* Slot 15 config register <slavio> */	/* 1fe.0000.2050 */
	} sbus;
	char		pad2[0x400 - 0x50];
	struct iommureg {
		u_int64_t	iommu_cr;	/* IOMMU control register */		/* 1fe.0000.2400 */
		u_int64_t	iommu_tsb;	/* IOMMU TSB base register */		/* 1fe.0000.2408 */
		u_int64_t	iommu_flush;	/* IOMMU flush register */		/* 1fe.0000.2410 */
	} iommu;
	u_int64_t	strbuf_ctl;		/* streaming buffer control reg */	/* 1fe.0000.2800 */
	u_int64_t	strbuf_pgflush;		/* streaming buffer page flush */	/* 1fe.0000.2808 */
	u_int64_t	strbuf_flushsync;	/* streaming buffer flush sync */	/* 1fe.0000.2810 */

	u_int64_t	sbus_slot0_int;		/* SBUS slot 0 interrupt map reg */	/* 1fe.0000.2c00 */
	u_int64_t	sbus_slot1_int;		/* SBUS slot 1 interrupt map reg */	/* 1fe.0000.2c08 */
	u_int64_t	sbus_slot2_int;		/* SBUS slot 2 interrupt map reg */	/* 1fe.0000.2c10 */
	u_int64_t	sbus_slot3_int;		/* SBUS slot 3 interrupt map reg */	/* 1fe.0000.2c18 */

	u_int64_t	scsi_int_map;		/* SCSI interrupt map reg */		/* 1fe.0000.3000 */
	u_int64_t	ether_int_map;		/* ethernet interrupt map reg */	/* 1fe.0000.3008 */
	u_int64_t	bpp_int_map;		/* parallel interrupt map reg */	/* 1fe.0000.3010 */
	u_int64_t	audio_int_map;		/* audio interrupt map reg */		/* 1fe.0000.3018 */
	u_int64_t	power_int_map;		/* power fail interrupt map reg */	/* 1fe.0000.3020 */
	u_int64_t	ser_kbd_ms_int_map;	/* serial/kbd/mouse interrupt map reg *//* 1fe.0000.3028 */
	u_int64_t	fd_int_map;		/* floppy interrupt map reg */		/* 1fe.0000.3030 */
	u_int64_t	therm_int_map;		/* thermal warn interrupt map reg */	/* 1fe.0000.3038 */
	u_int64_t	kbd_int_map;		/* kbd [unused] interrupt map reg */	/* 1fe.0000.3040 */
	u_int64_t	mouse_int_map;		/* mouse [unused] interrupt map reg */	/* 1fe.0000.3048 */
	u_int64_t	serial_int_map;		/* second serial interrupt map reg */	/* 1fe.0000.3050 */
	u_int64_t	timer0_int_map;		/* timer 0 interrupt map reg */		/* 1fe.0000.3060 */
	u_int64_t	timer1_int_map;		/* timer 1 interrupt map reg */		/* 1fe.0000.3068 */
	u_int64_t	ue_int_map;		/* UE interrupt map reg */		/* 1fe.0000.3070 */
	u_int64_t	ce_int_map;		/* CE interrupt map reg */		/* 1fe.0000.3078 */
	u_int64_t	sbus_int_map;		/* SBUS error interrupt map reg */	/* 1fe.0000.3080 */
	u_int64_t	pwrmgt_int_map;		/* power mgmt wake interrupt map reg */	/* 1fe.0000.3088 */
	u_int64_t	upagr_int_map;		/* UPA graphics interrupt map reg */	/* 1fe.0000.3090 */
	u_int64_t	reserved_int_map;	/* SCSI interrupt map reg */		/* 1fe.0000.3098 */

	u_int64_t	sys_svadiag;		/* SBUS virtual addr diag reg */	/* 1fe.0000.4400 */
	u_int64_t	iommu_queue_diag[16];	/* IOMMU LRU queue diag */		/* 1fe.0000.4500-457f */
	u_int64_t	tlb_tag_diag[16];	/* TLB tag diag */			/* 1fe.0000.4580-45ff */
	u_int64_t	tlb_data_diag[32];	/* TLB data RAM diag */			/* 1fe.0000.4600-46ff */
	u_int64_t	strbuf_data_diag[128];	/* streaming buffer data RAM diag */	/* 1fe.0000.5000-53f8 */
	u_int64_t	strbuf_error_diag[128];	/* streaming buffer error status diag *//* 1fe.0000.5400-57f8 */
	u_int64_t	strbuf_pg_tag_diag[16];	/* streaming buffer page tag diag */	/* 1fe.0000.5800-5878 */
	u_int64_t	strbuf_ln_tag_diag[16];	/* streaming buffer line tag diag */	/* 1fe.0000.5900-5978 */
};

#define IOMMU_CTL_IMPL		0xf0000000
#define IOMMU_CTL_VER		0x0f000000
#define IOMMU_CTL_RSVD1		0x00ffffe0
#define IOMMU_CTL_RANGE		0x0000001c
#define IOMMU_CTL_RANGESHFT	2
#define IOMMU_CTL_RSVD2		0x00000002
#define IOMMU_CTL_ME		0x00000001

#define IOMMU_BAR_IBA		0xfffffc00
#define IOMMU_BAR_IBASHFT	10

/* Flushpage fields */
#define IOMMU_FLPG_VADDR	0xfffff000
#define IOMMU_FLUSH_MASK	0xfffff000

#define IOMMU_FLUSHPAGE(sc, va)	do {				\
	(sc)->sc_reg->io_flushpage = (va) & IOMMU_FLUSH_MASK;	\
} while (0);
#define IOMMU_FLUSHALL(sc)	do {				\
	(sc)->sc_reg->io_flashclear = 0;			\
} while (0)

/* to pte.h ? */
typedef u_int32_t iopte_t;

#define IOPTE_PPN	0xffffff00	/* PA<35:12> */
#define IOPTE_C		0x00000080 	/* cacheable */
#define IOPTE_W		0x00000004	/* writable */
#define IOPTE_V		0x00000002	/* valid */
#define IOPTE_WAZ	0x00000001	/* must write as zero */

#define IOPTE_PPNSHFT	8		/* shift to get ppn from IOPTE */
#define IOPTE_PPNPASHFT	4		/* shift to get pa from ioppn */

#define IOPTE_BITS "\20\10C\3W\2V\1WAZ"

