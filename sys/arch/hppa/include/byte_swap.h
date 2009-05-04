/*	$NetBSD: byte_swap.h,v 1.7.74.1 2009/05/04 08:11:14 yamt Exp $	*/

/*	$OpenBSD: endian.h,v 1.8 2004/04/07 18:24:19 mickey Exp $	*/

/*
 * Copyright (c) 1998-2004 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HPPA_BYTE_SWAP_H_
#define	_HPPA_BYTE_SWAP_H_

#ifdef __GNUC__
#include <sys/types.h>
__BEGIN_DECLS


#define	__BYTE_SWAP_U32_VARIABLE __byte_swap_u32_variable
static __inline uint32_t __byte_swap_u32_variable(uint32_t);
static __inline uint32_t
__byte_swap_u32_variable(uint32_t x)
{
	register uint32_t __swap32md_x;	\
						\
	__asm  ("extru	%1, 7,8,%%r22\n\t"	\
		"shd	%1,%1,8,%0\n\t"		\
		"dep	%0,15,8,%0\n\t"		\
		"dep	%%r22,31,8,%0"		\
		: "=&r" (__swap32md_x)		\
		: "r" (x) : "r22");		\
	return(__swap32md_x);
}

#if 0
/*
 * Use generic C version because w/ asm __inline below
 * gcc inserts extra "extru r,31,16,r" to convert
 * to 16 bit entity, which produces overhead we don't need.
 * Besides, gcc does swap16 same way by itself.
 */
#define	__swap16md(x)	__swap16gen(x)
#else
#define	__BYTE_SWAP_U16_VARIABLE __byte_swap_u16_variable
static __inline uint16_t __byte_swap_u16_variable(uint16_t);
static __inline uint16_t
__byte_swap_u16_variable(uint16_t x)
{
	register uint16_t __swap16md_x;				\
									\
	__asm  ("extru	%1,23,8,%0\n\t"					\
		"dep	%1,23,8,%0"					\
	       : "=&r" (__swap16md_x) : "r" (x));			\
	return(__swap16md_x);
}
#endif

__END_DECLS
#endif

#endif /* !_HPPA_BYTE_SWAP_H_ */
