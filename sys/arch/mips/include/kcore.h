/*	$NetBSD: kcore.h,v 1.1.174.1 2011/12/27 06:54:28 matt Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Modified for NetBSD/mips by Jason R. Thorpe, Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 */

#ifndef _MIPS_KCORE_H_
#define _MIPS_KCORE_H_

typedef struct cpu_kcore_hdr {
	u_int32_t	sysmappa;		/* PA of Sysmap */
	u_int32_t	sysmapsize;		/* size of Sysmap */
	u_int32_t	archlevel;		/* MIPS architecture level */
	u_int32_t	pg_shift;		/* PTE page frame num shift */
	u_int32_t	pg_frame;		/* PTE page frame num mask */
	u_int32_t	pg_v;			/* PTE valid bit */
	u_int32_t	nmemsegs;		/* Number of RAM segments */
	u_int32_t	pg_size;		/* s/w page size */
	u_int32_t	ksegx_pfn;		/* starting pfn of ksegx */
	u_int32_t	ksegx_va;		/* starting va of ksegx */
	u_int32_t	ksegx_size;		/* size of ksegx */
#if 0
	phys_ram_seg_t  memsegs[];		/* RAM segments */
#endif
} cpu_kcore_hdr_t;

#endif /* _MIPS_KCORE_H_ */
