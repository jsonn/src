/*	$NetBSD: types.h,v 1.11.18.1 2007/10/03 19:24:54 garbled Exp $	*/

#define _MIPS_PADDR_T_64BIT

#include <mips/types.h>

#define	__HAVE_DEVICE_REGISTER
#define	__HAVE_GENERIC_TODR
#define	__HAVE_TIMECOUNTER

/* MIPS specific options */
#define	__HAVE_BOOTINFO_H
#define	__HAVE_MIPS_MACHDEP_CACHE_CONFIG
