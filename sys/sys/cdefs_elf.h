/*	$NetBSD: cdefs_elf.h,v 1.10.2.1 2002/02/28 04:15:23 nathanw Exp $	*/

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

#ifdef __LEADING_UNDERSCORE
#define	_C_LABEL(x)	__CONCAT(_,x)
#define _C_LABEL_STRING(x)	"_"x
#else
#define	_C_LABEL(x)	x
#define _C_LABEL_STRING(x)	x
#endif

#if __STDC__
#define	___RENAME(x)	__asm__(___STRING(_C_LABEL(x)))
#else
#ifdef __LEADING_UNDERSCORE
#define	___RENAME(x)	____RENAME(_/**/x)
#define	____RENAME(x)	__asm__(___STRING(x))
#else
#define	___RENAME(x)	__asm__(___STRING(x))
#endif
#endif

#undef	__DO_NOT_DO_WEAK__		/* we use weak syms */

#ifndef __DO_NOT_DO_WEAK__
#define	__indr_reference(sym,alias)	/* nada, since we do weak refs */
#endif /* !__DO_NOT_DO_WEAK__ */

#if __STDC__

#ifndef __DO_NOT_DO_WEAK__
#define	__weak_alias(alias,sym)						\
    __asm__(".weak " _C_LABEL_STRING(#alias) " ; "			\
	    _C_LABEL_STRING(#alias) " = " _C_LABEL_STRING(#sym));
#endif /* !__DO_NOT_DO_WEAK__ */
#define	__weak_extern(sym)						\
    __asm__(".weak " _C_LABEL_STRING(#sym));
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning." #sym " ; .ascii \"" msg "\" ; .text");

#else /* !__STDC__ */

#ifndef __DO_NOT_DO_WEAK__
#ifdef __LEADING_UNDERSCORE
#define __weak_alias(alias,sym) ___weak_alias(_/**/alias,_/**/sym)
#define	___weak_alias(alias,sym)					\
    __asm__(".weak alias ; alias = sym");
#else
#define	__weak_alias(alias,sym)						\
    __asm__(".weak alias ; alias = sym");
#endif
#endif /* !__DO_NOT_DO_WEAK__ */
#ifdef __LEADING_UNDERSCORE
#define __weak_extern(sym) ___weak_extern(_/**/sym)
#define	___weak_extern(sym)						\
    __asm__(".weak sym");
#else
#define	__weak_extern(sym)						\
    __asm__(".weak sym");
#endif
#define	__warn_references(sym,msg)					\
    __asm__(".section .gnu.warning.sym ; .ascii msg ; .text");

#endif /* !__STDC__ */

#if __STDC__
#define	__SECTIONSTRING(_sec, _str)					\
	__asm__(".section " #_sec " ; .asciz \"" _str "\" ; .text")
#else
#define	__SECTIONSTRING(_sec, _str)					\
	__asm__(".section _sec ; .asciz _str ; .text")
#endif

#define	__IDSTRING(_n,_s)		__SECTIONSTRING(.ident,_s)

#define	__RCSID(_s)			__IDSTRING(rcsid,_s)
#define	__SCCSID(_s)
#define __SCCSID2(_s)
#if 0	/* XXX userland __COPYRIGHTs have \ns in them */
#define	__COPYRIGHT(_s)			__SECTIONSTRING(.copyright,_s)
#else
#define	__COPYRIGHT(_s)							\
	static const char copyright[] __attribute__((__unused__)) = _s
#endif

#define	__KERNEL_RCSID(_n, _s)		__RCSID(_s)
#define	__KERNEL_SCCSID(_n, _s)
#if 0	/* XXX see above */
#define	__KERNEL_COPYRIGHT(_n, _s)	__COPYRIGHT(_s)
#else
#define	__KERNEL_COPYRIGHT(_n, _s)	__SECTIONSTRING(.copyright, _s)
#endif

#endif /* !_SYS_CDEFS_ELF_H_ */
