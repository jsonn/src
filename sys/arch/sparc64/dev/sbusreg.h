/*	$NetBSD: sbusreg.h,v 1.1.1.1.2.1 1998/07/30 14:03:51 eeh Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)sbusreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4c S-bus definitions.  (Should be made generic!)
 *
 * Sbus slot 0 is not a separate slot; it talks to the onboard I/O devices.
 * It is, however, addressed just like any `real' Sbus.
 *
 * Sbus device addresses are obtained from the FORTH PROMs.  They come
 * in `absolute' and `relative' address flavors, so we have to handle both.
 * Relative addresses do *not* include the slot number.
 */
#define	SBUS_BASE		0xf8000000
#define	SBUS_ADDR(slot, off)	(SBUS_BASE + ((slot) << 25) + (off))
#define	SBUS_ABS(a)		((unsigned)(a) >= SBUS_BASE)
#define	SBUS_ABS_TO_SLOT(a)	(((a) - SBUS_BASE) >> 25)
#define	SBUS_ABS_TO_OFFSET(a)	(((a) - SBUS_BASE) & 0x1ffffff)

/*
 * Sun4u S-bus definitions.  Here's where we deal w/the machine
 * dependencies of sysio.
 *
 * SYSIO implements or is the interface to several things:  
 *
 * o The SBUS interface itself
 * o The IOMMU
 * o The DVMA units
 * o The interrupt controller
 * o The counter/timers
 *
 * Since it has registers to control lots of different things
 * as well as several on-board SBUS devices and external SBUS
 * slots scattered throughout its address space, it's a pain.
 *
 * One good point, however, is that all registers are 64-bit.
 */

struct sysioreg {
	struct upareg {
		u_int64_t	upa_portid;		/* UPA port ID register */		/* 1fe.0000.0000 */
		u_int64_t	upa_config;		/* UPA config register */		/* 1fe.0000.0008 */
	} sys_upa;

	u_int64_t	sys_csr;		/* SYSIO control/status register */	/* 1fe.0000.0010 */
	u_int64_t	pad0;
	u_int64_t	sys_ecccr;		/* ECC control register */		/* 1fe.0000.0020 */
	u_int64_t	reserved;							/* 1fe.0000.0028 */
	u_int64_t	sys_ue_afsr;		/* Uncorrectable Error AFSR */		/* 1fe.0000.0030 */
	u_int64_t	sys_ue_afar;		/* Uncorrectable Error AFAR */		/* 1fe.0000.0038 */
	u_int64_t	sys_ce_afsr;		/* Correctable Error AFSR */		/* 1fe.0000.0040 */
	u_int64_t	sys_ce_afar;		/* Correctable Error AFAR */		/* 1fe.0000.0048 */

	u_int64_t	pad1[22];

	struct perfmon {
		u_int64_t	pm_cr;			/* Performance monitor control reg */	/* 1fe.0000.0100 */
		u_int64_t	pm_count;		/* Performance monitor counter reg */	/* 1fe.0000.0108 */
	} sys_pm;

	u_int64_t	pad2[990];

	struct sbusreg {
		u_int64_t	sbus_cr;		/* SBUS Control Register */		/* 1fe.0000.2000 */
		u_int64_t	reserved;							/* 1fe.0000.2008 */
		u_int64_t	sbus_afsr;		/* SBUS AFSR */				/* 1fe.0000.2010 */
		u_int64_t	sbus_afar;		/* SBUS AFAR */				/* 1fe.0000.2018 */
		u_int64_t	sbus_config0;	/* SBUS Slot 0 config register */	/* 1fe.0000.2020 */
		u_int64_t	sbus_config1;	/* SBUS Slot 1 config register */	/* 1fe.0000.2028 */
		u_int64_t	sbus_config2;	/* SBUS Slot 2 config register */	/* 1fe.0000.2030 */
		u_int64_t	sbus_config3;	/* SBUS Slot 3 config register */	/* 1fe.0000.2038 */
		u_int64_t	sbus_config13;	/* Slot 13 config register <audio> */	/* 1fe.0000.2040 */
		u_int64_t	sbus_config14;	/* Slot 14 config register <macio> */	/* 1fe.0000.2048 */
		u_int64_t	sbus_config15;	/* Slot 15 config register <slavio> */	/* 1fe.0000.2050 */
	} sys_sbus;

	u_int64_t	pad3[117];

	struct iommureg {
		u_int64_t	iommu_cr;	/* IOMMU control register */		/* 1fe.0000.2400 */
#define IOMMUCR_TSB1K		0x000000000000000000LL	/* Nummber of entries in IOTSB */
#define IOMMUCR_TSB2K		0x000000000000010000LL
#define IOMMUCR_TSB4K		0x000000000000020000LL
#define IOMMUCR_TSB8K		0x000000000000030000LL
#define IOMMUCR_TSB16K		0x000000000000040000LL
#define IOMMUCR_TSB32K		0x000000000000050000LL
#define IOMMUCR_TSB64K		0x000000000000060000LL
#define IOMMUCR_TSB128K		0x000000000000070000LL
#define IOMMUCR_8KPG		0x000000000000000000LL	/* 8K iommu page size */
#define IOMMUCR_64KPG		0x000000000000000004LL	/* 64K iommu page size */
#define IOMMUCR_DE		0x000000000000000002LL	/* Diag enable */
#define IOMMUCR_EN		0x000000000000000001LL	/* Enable IOMMU */
		u_int64_t	iommu_tsb;	/* IOMMU TSB base register */		/* 1fe.0000.2408 */
		u_int64_t	iommu_flush;	/* IOMMU flush register */		/* 1fe.0000.2410 */
	} sys_iommu;

	u_int64_t	pad4[125];

	struct strbuf {
		u_int64_t	strbuf_ctl;		/* streaming buffer control reg */	/* 1fe.0000.2800 */
#define STRBUF_EN		0x000000000000000001LL
#define STRBUF_D		0x000000000000000002LL
		u_int64_t	strbuf_pgflush;		/* streaming buffer page flush */	/* 1fe.0000.2808 */
		u_int64_t	strbuf_flushsync;	/* streaming buffer flush sync */	/* 1fe.0000.2810 */
	} sys_strbuf;

	u_int64_t	pad5[125];

	u_int64_t	sbus_slot0_int;		/* SBUS slot 0 interrupt map reg */	/* 1fe.0000.2c00 */
	u_int64_t	sbus_slot1_int;		/* SBUS slot 1 interrupt map reg */	/* 1fe.0000.2c08 */
	u_int64_t	sbus_slot2_int;		/* SBUS slot 2 interrupt map reg */	/* 1fe.0000.2c10 */
	u_int64_t	sbus_slot3_int;		/* SBUS slot 3 interrupt map reg */	/* 1fe.0000.2c18 */
	u_int64_t	intr_retry;		/* interrupt retry timer reg */		/* 1fe.0000.2c20 */

	u_int64_t	pad6[123];

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
	u_int64_t	pad7;
	u_int64_t	timer0_int_map;		/* timer 0 interrupt map reg */		/* 1fe.0000.3060 */
	u_int64_t	timer1_int_map;		/* timer 1 interrupt map reg */		/* 1fe.0000.3068 */
	u_int64_t	ue_int_map;		/* UE interrupt map reg */		/* 1fe.0000.3070 */
	u_int64_t	ce_int_map;		/* CE interrupt map reg */		/* 1fe.0000.3078 */
	u_int64_t	sbus_async_int_map;	/* SBUS error interrupt map reg */	/* 1fe.0000.3080 */
	u_int64_t	pwrmgt_int_map;		/* power mgmt wake interrupt map reg */	/* 1fe.0000.3088 */
	u_int64_t	upagr_int_map;		/* UPA graphics interrupt map reg */	/* 1fe.0000.3090 */
	u_int64_t	reserved_int_map;	/* reserved interrupt map reg */	/* 1fe.0000.3098 */

	u_int64_t	pad8[108];

	/* Note: clear interrupt 0 registers are not really used */
	u_int64_t	sbus0_clr_int[8];	/* SBUS slot 0 clear int regs 0..7 */	/* 1fe.0000.3400-3438 */
	u_int64_t	sbus1_clr_int[8];	/* SBUS slot 1 clear int regs 0..7 */	/* 1fe.0000.3440-3478 */
	u_int64_t	sbus2_clr_int[8];	/* SBUS slot 2 clear int regs 0..7 */	/* 1fe.0000.3480-34b8 */
	u_int64_t	sbus3_clr_int[8];	/* SBUS slot 3 clear int regs 0..7 */	/* 1fe.0000.34c0-34f8 */

	u_int64_t	pad9[96];

	u_int64_t	scsi_clr_int;		/* SCSI clear int reg */		/* 1fe.0000.3800 */
	u_int64_t	ether_clr_int;		/* ethernet clear int reg */		/* 1fe.0000.3808 */
	u_int64_t	bpp_clr_int;		/* parallel clear int reg */		/* 1fe.0000.3810 */
	u_int64_t	audio_clr_int;		/* audio clear int reg */		/* 1fe.0000.3818 */
	u_int64_t	power_clr_int;		/* power fail clear int reg */		/* 1fe.0000.3820 */
	u_int64_t	ser_kb_ms_clr_int;	/* serial/kbd/mouse clear int reg */	/* 1fe.0000.3828 */
	u_int64_t	fd_clr_int;		/* floppy clear int reg */		/* 1fe.0000.3830 */
	u_int64_t	therm_clr_int;		/* thermal warn clear int reg */	/* 1fe.0000.3838 */
	u_int64_t	kbd_clr_int;		/* kbd [unused] clear int reg */	/* 1fe.0000.3840 */
	u_int64_t	mouse_clr_int;		/* mouse [unused] clear int reg */	/* 1fe.0000.3848 */
	u_int64_t	serial_clr_int;		/* second serial clear int reg */	/* 1fe.0000.3850 */
	u_int64_t	pad10;
	u_int64_t	timer0_clr_int;		/* timer 0 clear int reg */		/* 1fe.0000.3860 */
	u_int64_t	timer1_clr_int;		/* timer 1 clear int reg */		/* 1fe.0000.3868 */
	u_int64_t	ue_clr_int;		/* UE clear int reg */			/* 1fe.0000.3870 */
	u_int64_t	ce_clr_int;		/* CE clear int reg */			/* 1fe.0000.3878 */
	u_int64_t	sbus_clr_async_int;	/* SBUS error clr interrupt reg */	/* 1fe.0000.3880 */
	u_int64_t	pwrmgt_clr_int;		/* power mgmt wake clr interrupt reg */	/* 1fe.0000.3888 */

	u_int64_t	pad11[110];

	struct timer_counter {
		u_int64_t	tc_count;	/* timer/counter 0/1 count register */	/* ife.0000.3c00,3c10 */
		u_int64_t	tc_limit;	/* timer/counter 0/1 limit register */	/* ife.0000.3c08,3c18 */
	} tc[2];

	u_int64_t	pad12[252];

	u_int64_t	sys_svadiag;		/* SBUS virtual addr diag reg */	/* 1fe.0000.4400 */
	
	u_int64_t	pad13[31];

	u_int64_t	iommu_queue_diag[16];	/* IOMMU LRU queue diag */		/* 1fe.0000.4500-457f */
	u_int64_t	tlb_tag_diag[16];	/* TLB tag diag */			/* 1fe.0000.4580-45ff */
	u_int64_t	tlb_data_diag[32];	/* TLB data RAM diag */			/* 1fe.0000.4600-46ff */

	u_int64_t	pad14[32];

	u_int64_t	sbus_int_diag;		/* SBUS int state diag reg */		/* 1fe.0000.4800 */
	u_int64_t	obio_int_diag;		/* OBIO and misc int state diag reg */	/* 1fe.0000.4808 */

	u_int64_t	pad15[254];

	u_int64_t	strbuf_data_diag[128];	/* streaming buffer data RAM diag */	/* 1fe.0000.5000-53f8 */
	u_int64_t	strbuf_error_diag[128];	/* streaming buffer error status diag *//* 1fe.0000.5400-57f8 */
	u_int64_t	strbuf_pg_tag_diag[16];	/* streaming buffer page tag diag */	/* 1fe.0000.5800-5878 */
	u_int64_t	pad16[16];
	u_int64_t	strbuf_ln_tag_diag[16];	/* streaming buffer line tag diag */	/* 1fe.0000.5900-5978 */
};

/* 
 * sun4u iommu stuff.  Probably belongs elsewhere.
 */

#define	IOTTE_V		0x8000000000000000LL	/* Entry valid */
#define IOTTE_64K	0x2000000000000000LL	/* 8K or 64K page? */
#define IOTTE_8K	0x0000000000000000LL
#define IOTTE_STREAM	0x1000000000000000LL	/* Is page streamable? */
#define	IOTTE_LOCAL	0x0800000000000000LL	/* Accesses to same bus segment? */
#define IOTTE_PAMASK	0x000001ffffffe000LL	/* Let's assume this is correct */
#define IOTTE_C		0x0000000000000010LL	/* Accesses to cacheable space */
#define IOTTE_W		0x0000000000000002LL	/* Writeable */

#define MAKEIOTTE(pa,w,c,s)	(((pa)&IOTTE_PAMASK)|((w)?IOTTE_W:0)|((c)?IOTTE_C:0)|((s)?IOTTE_STREAM:0)|(IOTTE_V|IOTTE_8K))
#if 0
/* This version generates a pointer to a int64_t */
#define IOTSBSLOT(va,sz)	((((((vaddr_t)(va))-(0xff800000<<(sz))))>>(13-3))&(~7))
#else
/* Here we just try to create an array index */
#define IOTSBSLOT(va,sz)	((((((vaddr_t)(va))-(0xff800000<<(sz))))>>(13)))
#endif

/*
 * intr map stuff.  Probably belongs elsewhere.
 */

#define INTMAP_V	0x080000000LL	/* Interrupt valid (enabled) */
#define INTMAP_TID	0x07c000000LL	/* UPA target ID mask */
#define INTMAP_IGN	0x0000007c0LL	/* Interrupt group no. */
#define INTMAP_INO	0x00000003fLL	/* Interrupt number */
#define INTMAP_INR	(INTMAP_IGN|INTMAP_INO)
#define INTMAP_SLOT	0x000000018LL	/* SBUS slot # */
#define INTMAP_OBIO	0x000000020LL	/* Onboard device */
#define INTMAP_LSHIFT	11		/* Encode level in vector */
#define	INTLEVENCODE(x)	(((x)&0x0f)<<INTMAP_LSHIFT)
#define INTLEV(x)	(((x)>>INTMAP_LSHIFT)&0x0f)
#define INTVEC(x)	((x)&INTMAP_INR)
#define INTSLOT(x)	(((x)>>3)&0x7)
#define	INTPRI(x)	((x)&0x7)
