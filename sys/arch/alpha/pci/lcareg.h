/* $NetBSD: lcareg.h,v 1.4.2.2 1997/06/06 00:14:09 cgd Exp $ */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
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
 * 21066 chip registers
 */

#define REGVAL(r)	(*(volatile int32_t *)ALPHA_PHYS_TO_K0SEG(r))
#define REGVAL64(r)	(*(volatile int64_t *)ALPHA_PHYS_TO_K0SEG(r))

/*
 * Base addresses
 */
#define LCA_IOC_BASE	0x180000000L		/* LCA IOC Regs */
#define LCA_PCI_SIO	0x1c0000000L		/* PCI Sp. I/O Space */
#define LCA_PCI_CONF	0x1e0000000L		/* PCI Conf. Space */
#define LCA_PCI_SPARSE	0x200000000L		/* PCI Sparse Space */
#define LCA_PCI_DENSE	0x300000000L		/* PCI Dense Space */

#define LCA_IOC_HAE	LCA_IOC_BASE		/* Host Address Ext. (64) */
#define	IOC_HAE_ADDREXT	0x00000000f8000000UL
#define	IOC_HAE_RSVSD	0xffffffff07ffffffUL

#define LCA_IOC_CONF	(LCA_IOC_BASE + 0x020)	/* Configuration Cycle Type */
#define LCA_IOC_STAT0	(LCA_IOC_BASE + 0x040)	/* Status 0 */
#define LCA_IOC_STAT1	(LCA_IOC_BASE + 0x060)	/* Status 1 */

#define LCA_IOC_W_BASE0	(LCA_IOC_BASE + 0x100)	/* Window Base */
#define LCA_IOC_W_MASK0	(LCA_IOC_BASE + 0x140)	/* Window Mask */
#define LCA_IOC_W_T_BASE0 (LCA_IOC_BASE + 0x180) /* Translated Base */

#define LCA_IOC_W_BASE1	(LCA_IOC_BASE + 0x120)	/* Window Base */
#define LCA_IOC_W_MASK1	(LCA_IOC_BASE + 0x160)	/* Window Mask */
#define LCA_IOC_W_T_BASE1 (LCA_IOC_BASE + 0x1a0) /* Translated Base */
