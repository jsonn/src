/*	$NetBSD: endian.h,v 1.20.2.1 1997/10/14 09:10:01 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1987, 1991 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)endian.h	7.8 (Berkeley) 4/3/91
 */

#ifndef _I386_ENDIAN_H_
#define	_I386_ENDIAN_H_

/*
 * Define the order of 32-bit words in 64-bit words.
 */
#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD 0


#ifndef _POSIX_SOURCE
/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	LITTLE_ENDIAN

#include <sys/cdefs.h>

typedef u_int32_t	in_addr_t;
typedef u_int16_t	in_port_t;

__BEGIN_DECLS
in_addr_t htonl __P((in_addr_t));
in_port_t htons __P((in_port_t));
in_addr_t ntohl __P((in_addr_t));
in_port_t ntohs __P((in_port_t));
u_int16_t bswap16 __P((u_int16_t));
u_int32_t bswap32 __P((u_int32_t));
u_int64_t bswap64 __P((u_int64_t));
__END_DECLS


#ifdef __GNUC__

#if defined(_KERNEL) && !defined(I386_CPU)
#define	__byte_swap_long_variable(x) \
({ register in_addr_t __x = (x); \
   __asm ("bswap %1" \
	: "=r" (__x) \
	: "0" (__x)); \
   __x; })
#else
#define	__byte_swap_long_variable(x) \
({ register in_addr_t __x = (x); \
   __asm ("rorw $8, %w1\n\trorl $16, %1\n\trorw $8, %w1" \
	: "=r" (__x) \
	: "0" (__x)); \
   __x; })
#endif	/* _KERNEL && ... */

#define	__byte_swap_word_variable(x) \
({ register in_port_t __x = (x); \
   __asm ("rorw $8, %w1" \
	: "=r" (__x) \
	: "0" (__x)); \
   __x; })

#ifdef __OPTIMIZE__

#define	__byte_swap_long_constant(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))
#define	__byte_swap_word_constant(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define	__byte_swap_long(x) \
	(__builtin_constant_p((x)) ? \
	 __byte_swap_long_constant(x) : __byte_swap_long_variable(x))
#define	__byte_swap_word(x) \
	(__builtin_constant_p((x)) ? \
	 __byte_swap_word_constant(x) : __byte_swap_word_variable(x))

#else /* __OPTIMIZE__ */

#define	__byte_swap_long(x)	__byte_swap_long_variable(x)
#define	__byte_swap_word(x)	__byte_swap_word_variable(x)

#endif /* __OPTIMIZE__ */

#define	ntohl(x)	((in_addr_t)__byte_swap_long((in_addr_t)(x)))
#define	ntohs(x)	((in_port_t)__byte_swap_word((in_port_t)(x)))
#define	htonl(x)	((in_addr_t)__byte_swap_long((in_addr_t)(x)))
#define	htons(x)	((in_port_t)__byte_swap_word((in_port_t)(x)))

#endif	/* __GNUC__ */


/*
 * Macros for network/external number representation conversion.
 */
#define	NTOHL(x)	(x) = ntohl((in_addr_t)(x))
#define	NTOHS(x)	(x) = ntohs((in_port_t)(x))
#define	HTONL(x)	(x) = htonl((in_addr_t)(x))
#define	HTONS(x)	(x) = htons((in_port_t)(x))

#endif /* _POSIX_SOURCE */

#endif /* !_I386_ENDIAN_H_ */
