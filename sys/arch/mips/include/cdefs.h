/*	$NetBSD: cdefs.h,v 1.11.52.1 2006/12/30 20:46:32 yamt Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef _MIPS_CDEFS_H_
#define	_MIPS_CDEFS_H_

/*      MIPS Subprogram Interface Model */
#define _MIPS_SIM_ABIX32	4	/* 64 bit safe, ILP32 o32 model */
#define _MIPS_SIM_ABI64		3
#define _MIPS_SIM_NABI32	2	/* 64bit safe, ILP32 n32 model */
#define _MIPS_SIM_ABI32		1

#define _MIPS_BSD_API_LP32	_MIPS_SIM_ABI32
#define	_MIPS_BSD_API_LP32_64CLEAN	_MIPS_SIM_ABIX32
#define	_MIPS_BSD_API_N32	_MIPS_SIM_NABI32
#define	_MIPS_BSD_API_LP64	_MIPS_SIM_ABI64

#if __mips_n64
#define	_MIPS_BSD_API		_MIPS_BSD_API_LP64
#elif __mips_n32
#define	_MIPS_BSD_API		_MIPS_BSD_API_N32
#elif __mips_o64
#define	_MIPS_BSD_API		_MIPS_BSD_API_LP32_64CLEAN
#else
#define	_MIPS_BSD_API		_MIPS_BSD_API_LP32
#endif

#endif /* !_MIPS_CDEFS_H_ */
