/*	$NetBSD: cdefs_elf.h,v 1.2.4.1 1999/06/21 01:30:15 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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

#ifndef _SYS_CDEFS_ELF_H_
#define	_SYS_CDEFS_ELF_H_

#define	_C_LABEL(x)	x

#define	__DO_NOT_DO_WEAK__		/* NO WEAK SYMS IN LIBC YET */

#ifndef __DO_NOT_DO_WEAK__
#define	__indr_reference(sym,alias)	/* nada, since we do weak refs */
#endif /* !__DO_NOT_DO_WEAK__ */

#ifdef __STDC__

#ifndef __DO_NOT_DO_WEAK__
#define	__weak_alias(alias,sym)						\
    __asm__(".weak " #alias " ; " #alias " = " #sym);
#endif /* !__DO_NOT_DO_WEAK__ */
#define	__weak_extern(sym)						\
    __asm__(".weak " #sym);
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning." #sym " ; .ascii \"" msg "\" ; .text");

#else /* !__STDC__ */

#ifndef __DO_NOT_DO_WEAK__
#define	__weak_alias(alias,sym)						\
    __asm__(".weak alias ; alias = sym");
#endif /* !__DO_NOT_DO_WEAK__ */
#define	__weak_extern(sym)						\
    __asm__(".weak sym");
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning.sym ; .ascii msg ; .text");

#endif /* !__STDC__ */

#ifdef __STDC__
#define	__SECTIONSTRING(_sec, _str)					\
	__asm__(".section " #_sec " ; .asciz \"" _str "\" ; .text")
#else
#define	__SECTIONSTRING(_sec, _str)					\
	__asm__(".section _sec ; .asciz _str ; .text")
#endif

#define	__IDSTRING(_n,_s)		__SECTIONSTRING(.ident,_s)

#define	__RCSID(_s)			__IDSTRING(rcsid,_s)
#if 0	/* XXX userland __COPYRIGHTs have \ns in them */
#define	__COPYRIGHT(_s)			__SECTIONSTRING(.copyright,_s)
#else
#define	__COPYRIGHT(_s)							\
	static const char copyright[] __attribute__((__unused__)) = _s
#endif

#define	__KERNEL_RCSID(_n, _s)		__RCSID(_s)
#if 0	/* XXX see above */
#define	__KERNEL_COPYRIGHT(_n, _s)	__COPYRIGHT(_s)
#else
#define	__KERNEL_COPYRIGHT(_n, _s)	__SECTIONSTRING(.copyright, _s)
#endif

#endif /* !_SYS_CDEFS_ELF_H_ */
