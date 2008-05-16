/*	$NetBSD: byte_swap.h,v 1.6.78.1 2008/05/16 02:21:56 yamt Exp $	*/

/*-
 * Copyright (c) 1997, 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, Neil A. Carson, and Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_BYTE_SWAP_H_
#define	_ARM_BYTE_SWAP_H_

#ifdef __GNUC__
#include <sys/types.h>
__BEGIN_DECLS

#define	__BYTE_SWAP_U32_VARIABLE __byte_swap_u32_variable
static __inline uint32_t
__byte_swap_u32_variable(uint32_t v)
{
#ifdef _ARM_ARCH_6
	__asm("rev\t%0, %1" : "=r" (v) : "0" (v));
#else
	uint32_t t1;

	t1 = v ^ ((v << 16) | (v >> 16));
	t1 &= 0xff00ffffU;
	v = (v >> 8) | (v << 24);
	v ^= (t1 >> 8);
#endif
	return (v);
}

#define	__BYTE_SWAP_U16_VARIABLE __byte_swap_u16_variable
static __inline uint16_t
__byte_swap_u16_variable(uint16_t v)
{

#ifdef _ARM_ARCH_6
	__asm("rev16\t%0, %1" : "=r" (v) : "0" (v));
#elif !defined(__thumb__)
	__asm volatile(
		"mov	%0, %1, ror #8\n"
		"orr	%0, %0, %0, lsr #16\n"
		"bic	%0, %0, %0, lsl #16"
	: "=r" (v)
	: "0" (v));
#else
	v &= 0xffff;
	v = (v >> 8) | (v << 8);
#endif

	return (v);
}

__END_DECLS
#endif


#endif /* _ARM_BYTE_SWAP_H_ */
