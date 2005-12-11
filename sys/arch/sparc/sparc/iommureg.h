/*	$NetBSD: iommureg.h,v 1.8.2.1 2005/12/11 10:28:36 christos Exp $ */

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

/* IOMMU registers */
struct iommureg {
	uint32_t	io_cr;		/* IOMMU control register */
	uint32_t	io_bar;		/* IOMMU PTE base register */
	uint32_t	io_fill1[3];
	uint32_t	io_flashclear;	/* Flush all TLB entries */
	uint32_t	io_flushpage;	/* Flush page from TLB */
};

/* Control register bits */
#define IOMMU_CTL_IMPL		0xf0000000	/* Hardware implementation */
#define IOMMU_CTL_VER		0x0f000000	/* Hardware version */
#define IOMMU_CTL_RSVD1		0x00ffffe0	/* Reserved bits */
#define IOMMU_CTL_RANGE		0x0000001c	/* Available DVMA range */
#define IOMMU_CTL_RANGESHFT	2		/*  == log2(range/16MB) */
#define IOMMU_CTL_DE		0x00000002	/* Diagnostic access enable */
#define IOMMU_CTL_ME		0x00000001	/* Enable translations */

/* Base register bits */
#define IOMMU_BAR_IBA		0xfffffc00
#define IOMMU_BAR_IBASHFT	10

/* Flushpage fields */
#define IOMMU_FLPG_VADDR	0xfffff000
#define IOMMU_FLUSH_MASK	0xfffff000

/*
 * A few empty cycles after touching the IOMMU registers seems to
 * avoid utter lossage on some machines (SS4s & SS5s) where our caller
 * would see some of its local (`%lx') registers trashed.
 */
#define IOMMU_FLUSHPAGE(sc, va)	do {				\
	(sc)->sc_reg->io_flushpage = (va) & IOMMU_FLUSH_MASK;	\
	__asm("nop;nop;nop;nop;nop;nop;");			\
} while (0);
#define IOMMU_FLUSHALL(sc)	do {				\
	(sc)->sc_reg->io_flashclear = 0;			\
	__asm("nop;nop;nop;nop;nop;nop;");			\
} while (0)

typedef uint32_t iopte_t;

/* IOMMU PTE bits */
#define IOPTE_PPN	0xffffff00	/* PA<35:12> */
#define IOPTE_C		0x00000080 	/* cacheable */
#define IOPTE_W		0x00000004	/* writable */
#define IOPTE_V		0x00000002	/* valid */
#define IOPTE_WAZ	0x00000001	/* must write as zero */

#define IOPTE_PPNSHFT	8		/* shift to get ppn from IOPTE */
#define IOPTE_PPNPASHFT	4		/* shift to get pa from ioppn */

#define IOPTE_BITS "\20\10C\3W\2V\1WAZ"

