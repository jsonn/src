/*	$NetBSD: vmparam.h,v 1.6.12.1 2002/12/11 05:52:10 thorpej Exp $	*/
/*	$OpenBSD: vmparam.h,v 1.3 1997/04/19 17:19:59 pefo Exp $	*/
/*	NetBSD: vmparam.h,v 1.5 1994/10/26 21:10:10 cgd Exp 	*/

#include <mips/vmparam.h>

/*
 * Maximum number of contigous physical memory segment.
 */
#define	VM_PHYSSEG_MAX		16

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

#ifndef KSEG2IOBUFSIZE
#define KSEG2IOBUFSIZE	kseg2iobufsize	/* reserve PTEs for KSEG2 I/O space */

extern vsize_t kseg2iobufsize;
#endif
