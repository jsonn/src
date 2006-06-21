/*	$NetBSD: vmparam.h,v 1.7.22.1 2006/06/21 14:49:07 yamt Exp $	*/
/*	$OpenBSD: vmparam.h,v 1.3 1997/04/19 17:19:59 pefo Exp $	*/
/*	NetBSD: vmparam.h,v 1.5 1994/10/26 21:10:10 cgd Exp 	*/

#include <mips/vmparam.h>

/* VA 0xe0000000-0xffffffff is used for wired_map TLB entries. */
#undef  VM_MAX_KERNEL_ADDRESS
#define VM_MAX_KERNEL_ADDRESS		((vaddr_t)0xDFFFF000)

#define VM_MIN_WIRED_MAP_ADDRESS	((vaddr_t)0xE0000000)
#define VM_MAX_WIRED_MAP_ADDRESS	((vaddr_t)0xFFFFC000)

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
