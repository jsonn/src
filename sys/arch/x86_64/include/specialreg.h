/*	$NetBSD: specialreg.h,v 1.1.2.1 2002/06/23 17:43:30 jdolecek Exp $	*/

#ifdef _KERNEL
#include <i386/include/specialreg.h>
#else
#include <i386/specialreg.h>
#endif

/*
 * Extended Feature Enable Register of the x86-64
 */

#define MSR_EFER	0xc0000080

#define EFER_SCE	0x00000001	/* SYSCALL extension */
#define EFER_LME	0x00000100	/* Long Mode Active */
#define EFER_LMA	0x00000400	/* Long Mode Enabled */

#define MSR_STAR	0xc0000081		/* 32 bit syscall gate addr */
#define MSR_LSTAR	0xc0000082		/* 64 bit syscall gate addr */
#define MSR_CSTAR	0xc0000083		/* compat syscall gate addr */
#define MSR_SFMASK	0xc0000084		/* flags to clear on syscall */

#define MSR_FSBASE		0xc0000100	/* 64bit offset for fs: */
#define MSR_GSBASE		0xc0000101	/* 64bit offset for gs: */
#define MSR_KERNELGSBASE	0xc0000102	/* storage for swapgs ins */
