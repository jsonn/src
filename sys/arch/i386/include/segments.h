/*	$NetBSD: segments.h,v 1.25.14.1 1997/11/13 08:10:04 mellon Exp $	*/

/*-
 * Copyright (c) 1995, 1997
 *	Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)segments.h	7.1 (Berkeley) 5/9/91
 */

/*
 * 386 Segmentation Data Structures and definitions
 *	William F. Jolitz (william@ernie.berkeley.edu) 6/20/1989
 */

#ifndef _I386_SEGMENTS_H_
#define _I386_SEGMENTS_H_

/*
 * Selectors
 */

#define	ISPL(s)		((s) & SEL_RPL)	/* what is the priority level of a selector */
#define	SEL_KPL		0		/* kernel privilege level */	
#define	SEL_UPL		3		/* user privilege level */	
#define	SEL_RPL		3		/* requester's privilege level mask */
#define	ISLDT(s)	((s) & SEL_LDT)	/* is it local or global */
#define	SEL_LDT		4		/* local descriptor table */	
#define	IDXSEL(s)	(((s) >> 3) & 0x1fff)		/* index of selector */
#define	GSEL(s,r)	(((s) << 3) | r)		/* a global selector */
#define	LSEL(s,r)	(((s) << 3) | r | SEL_LDT)	/* a local selector */

#ifdef VM86
#define	USERMODE(c, f)		(ISPL(c) == SEL_UPL || ((f) & PSL_VM) != 0)
#define	KERNELMODE(c, f)	(ISPL(c) == SEL_KPL && ((f) & PSL_VM) == 0)
#else
#define	USERMODE(c, f)		(ISPL(c) == SEL_UPL)
#define	KERNELMODE(c, f)	(ISPL(c) == SEL_KPL)
#endif

#ifndef _LOCORE

#if __GNUC__ == 2 && __GNUC_MINOR__ < 7
#pragma pack(1)
#endif

/*
 * Memory and System segment descriptors
 */
struct segment_descriptor {
	unsigned sd_lolimit:16;		/* segment extent (lsb) */
	unsigned sd_lobase:24;		/* segment base address (lsb) */
	unsigned sd_type:5;		/* segment type */
	unsigned sd_dpl:2;		/* segment descriptor priority level */
	unsigned sd_p:1;		/* segment descriptor present */
	unsigned sd_hilimit:4;		/* segment extent (msb) */
	unsigned sd_xx:2;		/* unused */
	unsigned sd_def32:1;		/* default 32 vs 16 bit size */
	unsigned sd_gran:1;		/* limit granularity (byte/page) */
	unsigned sd_hibase:8;		/* segment base address (msb) */
} __attribute__((packed));

/*
 * Gate descriptors (e.g. indirect descriptors)
 */
struct gate_descriptor {
	unsigned gd_looffset:16;	/* gate offset (lsb) */
	unsigned gd_selector:16;	/* gate segment selector */
	unsigned gd_stkcpy:5;		/* number of stack wds to cpy */
	unsigned gd_xx:3;		/* unused */
	unsigned gd_type:5;		/* segment type */
	unsigned gd_dpl:2;		/* segment descriptor priority level */
	unsigned gd_p:1;		/* segment descriptor present */
	unsigned gd_hioffset:16;	/* gate offset (msb) */
} __attribute__((packed));

/*
 * Generic descriptor
 */
union descriptor {
	struct segment_descriptor sd;
	struct gate_descriptor gd;
} __attribute__((packed));

/*
 * region descriptors, used to load gdt/idt tables before segments yet exist.
 */
struct region_descriptor {
	unsigned rd_limit:16;		/* segment extent */
	unsigned rd_base:32;		/* base address  */
} __attribute__((packed));

#if __GNUC__ == 2 && __GNUC_MINOR__ < 7
#pragma pack(4)
#endif

#ifdef _KERNEL
extern union descriptor static_gdt[], *gdt;
extern union descriptor static_ldt[];
extern struct gate_descriptor static_idt[], *idt;

void setgate __P((struct gate_descriptor *, void *, int, int, int));
void setregion __P((struct region_descriptor *, void *, size_t));
void setsegment __P((struct segment_descriptor *, void *, size_t, int, int,
    int, int));
#endif /* _KERNEL */

#endif /* !_LOCORE */

/* system segments and gate types */
#define	SDT_SYSNULL	 0	/* system null */
#define	SDT_SYS286TSS	 1	/* system 286 TSS available */
#define	SDT_SYSLDT	 2	/* system local descriptor table */
#define	SDT_SYS286BSY	 3	/* system 286 TSS busy */
#define	SDT_SYS286CGT	 4	/* system 286 call gate */
#define	SDT_SYSTASKGT	 5	/* system task gate */
#define	SDT_SYS286IGT	 6	/* system 286 interrupt gate */
#define	SDT_SYS286TGT	 7	/* system 286 trap gate */
#define	SDT_SYSNULL2	 8	/* system null again */
#define	SDT_SYS386TSS	 9	/* system 386 TSS available */
#define	SDT_SYSNULL3	10	/* system null again */
#define	SDT_SYS386BSY	11	/* system 386 TSS busy */
#define	SDT_SYS386CGT	12	/* system 386 call gate */
#define	SDT_SYSNULL4	13	/* system null again */
#define	SDT_SYS386IGT	14	/* system 386 interrupt gate */
#define	SDT_SYS386TGT	15	/* system 386 trap gate */

/* memory segment types */
#define	SDT_MEMRO	16	/* memory read only */
#define	SDT_MEMROA	17	/* memory read only accessed */
#define	SDT_MEMRW	18	/* memory read write */
#define	SDT_MEMRWA	19	/* memory read write accessed */
#define	SDT_MEMROD	20	/* memory read only expand dwn limit */
#define	SDT_MEMRODA	21	/* memory read only expand dwn limit accessed */
#define	SDT_MEMRWD	22	/* memory read write expand dwn limit */
#define	SDT_MEMRWDA	23	/* memory read write expand dwn limit acessed */
#define	SDT_MEME	24	/* memory execute only */
#define	SDT_MEMEA	25	/* memory execute only accessed */
#define	SDT_MEMER	26	/* memory execute read */
#define	SDT_MEMERA	27	/* memory execute read accessed */
#define	SDT_MEMEC	28	/* memory execute only conforming */
#define	SDT_MEMEAC	29	/* memory execute only accessed conforming */
#define	SDT_MEMERC	30	/* memory execute read conforming */
#define	SDT_MEMERAC	31	/* memory execute read accessed conforming */

/* is memory segment descriptor pointer ? */
#define ISMEMSDP(s)	((s->d_type) >= SDT_MEMRO && \
			 (s->d_type) <= SDT_MEMERAC)

/* is 286 gate descriptor pointer ? */
#define IS286GDP(s)	((s->d_type) >= SDT_SYS286CGT && \
			 (s->d_type) < SDT_SYS286TGT)

/* is 386 gate descriptor pointer ? */
#define IS386GDP(s)	((s->d_type) >= SDT_SYS386CGT && \
			 (s->d_type) < SDT_SYS386TGT)

/* is gate descriptor pointer ? */
#define ISGDP(s)	(IS286GDP(s) || IS386GDP(s))

/* is segment descriptor pointer ? */
#define ISSDP(s)	(ISMEMSDP(s) || !ISGDP(s))

/* is system segment descriptor pointer ? */
#define ISSYSSDP(s)	(!ISMEMSDP(s) && !ISGDP(s))

/*
 * Segment Protection Exception code bits
 */
#define	SEGEX_EXT	0x01	/* recursive or externally induced */
#define	SEGEX_IDT	0x02	/* interrupt descriptor table */
#define	SEGEX_TI	0x04	/* local descriptor table */

/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define	NIDT	256
#define	NRSVIDT	32		/* reserved entries for cpu exceptions */

/*
 * Entries in the Global Descriptor Table (GDT)
 */
#define	GNULL_SEL	0	/* Null descriptor */
#define	GCODE_SEL	1	/* Kernel code descriptor */
#define	GDATA_SEL	2	/* Kernel data descriptor */
#define	GLDT_SEL	3	/* Default LDT descriptor */
#define	GUCODE_SEL	4	/* User code descriptor */
#define	GUDATA_SEL	5	/* User data descriptor */
#define	GAPM32CODE_SEL	6
#define	GAPM16CODE_SEL	7
#define	GAPMDATA_SEL	8
#define	GBIOSCODE_SEL	9
#define	GBIOSDATA_SEL	10
#define	NGDT		11

/*
 * Entries in the Local Descriptor Table (LDT)
 */
#define	LSYS5CALLS_SEL	0	/* iBCS system call gate */
#define	LSYS5SIGR_SEL	1	/* iBCS sigreturn gate */
#define	LUCODE_SEL	2	/* User code descriptor */
#define	LUDATA_SEL	3	/* User data descriptor */
#define	LBSDICALLS_SEL	16	/* BSDI system call gate */
#define	NLDT		17

#endif /* _I386_SEGMENTS_H_ */
