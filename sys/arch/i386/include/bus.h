/*	$NetBSD: bus.h,v 1.9.2.1 1997/08/23 07:09:03 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_BUS_H_
#define _I386_BUS_H_

#include <machine/pio.h>

#ifndef __BUS_SPACE_COMPAT_OLDDEFS
#define	__BUS_SPACE_COMPAT_OLDDEFS
#endif

/*
 * Values for the i386 bus space tag, not to be used directly by MI code.
 */
#define	I386_BUS_SPACE_IO	0	/* space is i/o space */
#define I386_BUS_SPACE_MEM	1	/* space is mem space */

/*
 * Bus address and size types
 */
typedef u_long bus_addr_t;
typedef u_long bus_size_t;

/*
 * Access methods for bus resources and address space.
 */
typedef	int bus_space_tag_t;
typedef	u_long bus_space_handle_t;

/*
 *	int bus_space_map  __P((bus_space_tag_t t, bus_addr_t addr,
 *	    bus_size_t size, int flags, bus_space_handle_t *bshp));
 *
 * Map a region of bus space.
 */

#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02

int	i386_memio_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int flags, bus_space_handle_t *bshp));
/* like map, but without extent map checking/allocation */
int	_i386_memio_map __P((bus_space_tag_t t, bus_addr_t addr,
	    bus_size_t size, int flags, bus_space_handle_t *bshp));

#define	bus_space_map(t, a, s, f, hp)					\
	i386_memio_map((t), (a), (s), (f), (hp))

/*
 *	int bus_space_unmap __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Unmap a region of bus space.
 */

void	i386_memio_unmap __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

#define bus_space_unmap(t, h, s)					\
	i386_memio_unmap((t), (h), (s))

/*
 *	int bus_space_subregion __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *	    bus_space_handle_t *nbshp));
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int	i386_memio_subregion __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp));

#define bus_space_subregion(t, h, o, s, nhp)				\
	i386_memio_subregion((t), (h), (o), (s), (nhp))

/*
 *	int bus_space_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
 *	    bus_addr_t rend, bus_size_t size, bus_size_t align,
 *	    bus_size_t boundary, int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp));
 *
 * Allocate a region of bus space.
 */

int	i386_memio_alloc __P((bus_space_tag_t t, bus_addr_t rstart,
	    bus_addr_t rend, bus_size_t size, bus_size_t align,
	    bus_size_t boundary, int flags, bus_addr_t *addrp,
	    bus_space_handle_t *bshp));

#define bus_space_alloc(t, rs, re, s, a, b, f, ap, hp)			\
	i386_memio_alloc((t), (rs), (re), (s), (a), (b), (f), (ap), (hp))

/*
 *	int bus_space_free __P((bus_space_tag_t t,
 *	    bus_space_handle_t bsh, bus_size_t size));
 *
 * Free a region of bus space.
 */

void	i386_memio_free __P((bus_space_tag_t t, bus_space_handle_t bsh,
	    bus_size_t size));

#define bus_space_free(t, h, s)						\
	i386_memio_free((t), (h), (s))

/*
 *	u_intN_t bus_space_read_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset));
 *
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 */

#define	bus_space_read_1(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inb((h) + (o))) :			\
	    (*(volatile u_int8_t *)((h) + (o))))

#define	bus_space_read_2(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inw((h) + (o))) :			\
	    (*(volatile u_int16_t *)((h) + (o))))

#define	bus_space_read_4(t, h, o)					\
	((t) == I386_BUS_SPACE_IO ? (inl((h) + (o))) :			\
	    (*(volatile u_int32_t *)((h) + (o))))

#if 0	/* Cause a link error for bus_space_read_8 */
#define	bus_space_read_8(t, h, o)	!!! bus_space_read_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */

#define	bus_space_read_multi_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insb((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movb (%0),%%al				;	\
			stosb					;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_read_multi_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insw((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movw (%0),%%ax				;	\
			stosw					;	\
			loop 1b"				:	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#define	bus_space_read_multi_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		insl((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	movl (%0),%%eax				;	\
			stosl					;	\
			loop 1b"				:	\
								:	\
		    "r" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edi", "%ecx", "%eax", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_read_multi_8 */
#define	bus_space_read_multi_8	!!! bus_space_read_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_read_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t *addr, size_t count));
 *
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */

#define	bus_space_read_region_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inb %w0,%%al				;	\
			stosb					;	\
			incl %0					;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsb"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_read_region_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inw %w0,%%ax				;	\
			stosw					;	\
			addl $2,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsw"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_read_region_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	inl %w0,%%eax				;	\
			stosl					;	\
			addl $4,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%edx", "%edi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsl"					:	\
								:	\
		    "S" ((h) + (o)), "D" ((a)), "c" ((c))	:	\
		    "%esi", "%edi", "%ecx", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_read_region_8 */
#define	bus_space_read_region_8	!!! bus_space_read_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_write_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    u_intN_t value));
 *
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */

#define	bus_space_write_1(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outb((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int8_t *)((h) + (o)) = (v)));	\
} while (0)

#define	bus_space_write_2(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outw((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int16_t *)((h) + (o)) = (v)));	\
} while (0)

#define	bus_space_write_4(t, h, o, v)	do {				\
	if ((t) == I386_BUS_SPACE_IO)					\
		outl((h) + (o), (v));					\
	else								\
		((void)(*(volatile u_int32_t *)((h) + (o)) = (v)));	\
} while (0)

#if 0	/* Cause a link error for bus_space_write_8 */
#define	bus_space_write_8	!!! bus_space_write_8 not implemented !!!
#endif

/*
 *	void bus_space_write_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */

#define	bus_space_write_multi_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsb((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsb					;	\
			movb %%al,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#define bus_space_write_multi_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsw((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsw					;	\
			movw %%ax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#define bus_space_write_multi_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		outsl((h) + (o), (a), (c));				\
	} else {							\
		__asm __volatile("					\
			cld					;	\
		1:	lodsl					;	\
			movl %%eax,(%0)				;	\
			loop 1b"				: 	\
								:	\
		    "r" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%esi", "%ecx", "%eax");				\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_write_multi_8 */
#define	bus_space_write_multi_8(t, h, o, a, c)				\
			!!! bus_space_write_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_write_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    const u_intN_t *addr, size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */

#define	bus_space_write_region_1(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsb					;	\
			outb %%al,%w0				;	\
			incl %0					;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsb"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_write_region_2(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsw					;	\
			outw %%ax,%w0				;	\
			addl $2,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsw"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#define	bus_space_write_region_4(t, h, o, a, c) do {			\
	if ((t) == I386_BUS_SPACE_IO) {					\
		__asm __volatile("					\
			cld					;	\
		1:	lodsl					;	\
			outl %%eax,%w0				;	\
			addl $4,%0				;	\
			loop 1b"				: 	\
								:	\
		    "d" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edx", "%esi", "%ecx", "%eax", "memory");		\
	} else {							\
		__asm __volatile("					\
			cld					;	\
			repne					;	\
			movsl"					:	\
								:	\
		    "D" ((h) + (o)), "S" ((a)), "c" ((c))	:	\
		    "%edi", "%esi", "%ecx", "memory");			\
	}								\
} while (0)

#if 0	/* Cause a link error for bus_space_write_region_8 */
#define	bus_space_write_region_8					\
			!!! bus_space_write_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_set_multi_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */

static __inline void bus_space_set_multi_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t));
static __inline void bus_space_set_multi_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t));
static __inline void bus_space_set_multi_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t));

static __inline void
bus_space_set_multi_1(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		while (c--)
			outb(addr, v);
	else
		while (c--)
			*(volatile u_int8_t *)(addr) = v;
}

static __inline void
bus_space_set_multi_2(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		while (c--)
			outw(addr, v);
	else
		while (c--)
			*(volatile u_int16_t *)(addr) = v;
}

static __inline void
bus_space_set_multi_4(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		while (c--)
			outl(addr, v);
	else
		while (c--)
			*(volatile u_int32_t *)(addr) = v;
}

#if 0	/* Cause a link error for bus_space_set_multi_8 */
#define	bus_space_set_multi_8 !!! bus_space_set_multi_8 unimplemented !!!
#endif

/*
 *	void bus_space_set_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset, u_intN_t val,
 *	    size_t count));
 *
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */

static __inline void bus_space_set_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int8_t, size_t));
static __inline void bus_space_set_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int16_t, size_t));
static __inline void bus_space_set_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, u_int32_t, size_t));

static __inline void
bus_space_set_region_1(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int8_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		for (; c != 0; c--, addr++)
			outb(addr, v);
	else
		for (; c != 0; c--, addr++)
			*(volatile u_int8_t *)(addr) = v;
}

static __inline void
bus_space_set_region_2(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int16_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		for (; c != 0; c--, addr += 2)
			outw(addr, v);
	else
		for (; c != 0; c--, addr += 2)
			*(volatile u_int16_t *)(addr) = v;
}

static __inline void
bus_space_set_region_4(t, h, o, v, c)
	bus_space_tag_t t;
	bus_space_handle_t h;
	bus_size_t o;
	u_int32_t v;
	size_t c;
{
	bus_addr_t addr = h + o;

	if (t == I386_BUS_SPACE_IO)
		for (; c != 0; c--, addr += 4)
			outl(addr, v);
	else
		for (; c != 0; c--, addr += 4)
			*(volatile u_int32_t *)(addr) = v;
}

#if 0	/* Cause a link error for bus_space_set_region_8 */
#define	bus_space_set_region_8	!!! bus_space_set_region_8 unimplemented !!!
#endif

/*
 *	void bus_space_copy_region_N __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh1, bus_size_t off1,
 *	    bus_space_handle_t bsh2, bus_size_t off2,
 *	    size_t count));
 *
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */

static __inline void bus_space_copy_region_1 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));
static __inline void bus_space_copy_region_2 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));
static __inline void bus_space_copy_region_4 __P((bus_space_tag_t,
	bus_space_handle_t, bus_size_t, bus_space_handle_t,
	bus_size_t, size_t));

static __inline void
bus_space_copy_region_1(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (t == I386_BUS_SPACE_IO) {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1++, addr2++)
				outb(addr2, inb(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += (c - 1), addr2 += (c - 1);
			    c != 0; c--, addr1--, addr2--)
				outb(addr2, inb(addr1));
		}
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1++, addr2++)
				*(volatile u_int8_t *)(addr2) =
				    *(volatile u_int8_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += (c - 1), addr2 += (c - 1);
			    c != 0; c--, addr1--, addr2--)
				*(volatile u_int8_t *)(addr2) =
				    *(volatile u_int8_t *)(addr1);
		}
	}
}

static __inline void
bus_space_copy_region_2(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (t == I386_BUS_SPACE_IO) {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1 += 2, addr2 += 2)
				outw(addr2, inw(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 2 * (c - 1), addr2 += 2 * (c - 1);
			    c != 0; c--, addr1 -= 2, addr2 -= 2)
				outw(addr2, inw(addr1));
		}
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1 += 2, addr2 += 2)
				*(volatile u_int16_t *)(addr2) =
				    *(volatile u_int16_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 2 * (c - 1), addr2 += 2 * (c - 1);
			    c != 0; c--, addr1 -= 2, addr2 -= 2)
				*(volatile u_int16_t *)(addr2) =
				    *(volatile u_int16_t *)(addr1);
		}
	}
}

static __inline void
bus_space_copy_region_4(t, h1, o1, h2, o2, c)
	bus_space_tag_t t;
	bus_space_handle_t h1;
	bus_size_t o1;
	bus_space_handle_t h2;
	bus_size_t o2;
	size_t c;
{
	bus_addr_t addr1 = h1 + o1;
	bus_addr_t addr2 = h2 + o2;

	if (t == I386_BUS_SPACE_IO) {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1 += 4, addr2 += 4)
				outl(addr2, inl(addr1));
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 4 * (c - 1), addr2 += 4 * (c - 1);
			    c != 0; c--, addr1 -= 4, addr2 -= 4)
				outl(addr2, inl(addr1));
		}
	} else {
		if (addr1 >= addr2) {
			/* src after dest: copy forward */
			for (; c != 0; c--, addr1 += 4, addr2 += 4)
				*(volatile u_int32_t *)(addr2) =
				    *(volatile u_int32_t *)(addr1);
		} else {
			/* dest after src: copy backwards */
			for (addr1 += 4 * (c - 1), addr2 += 4 * (c - 1);
			    c != 0; c--, addr1 -= 4, addr2 -= 4)
				*(volatile u_int32_t *)(addr2) =
				    *(volatile u_int32_t *)(addr1);
		}
	}
}

#if 0	/* Cause a link error for bus_space_copy_8 */
#define	bus_space_copy_region_8	!!! bus_space_copy_region_8 unimplemented !!!
#endif

#ifdef __BUS_SPACE_COMPAT_OLDDEFS
/* compatibility definitions; deprecated */
#define	bus_space_copy_1(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_2(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_4(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_8(t, h1, o1, h2, o2, c)				\
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#endif


/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier __P((bus_space_tag_t tag,
 *	    bus_space_handle_t bsh, bus_size_t offset,
 *	    bus_size_t len, int flags));
 *
 * Note: the i386 does not currently require barriers, but we must
 * provide the flags to MI code.
 */
#define	bus_space_barrier(t, h, o, l, f)	\
	((void)((void)(t), (void)(h), (void)(o), (void)(l), (void)(f)))
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#ifdef __BUS_SPACE_COMPAT_OLDDEFS
/* compatibility definitions; deprecated */
#define	BUS_BARRIER_READ	BUS_SPACE_BARRIER_READ
#define	BUS_BARRIER_WRITE	BUS_SPACE_BARRIER_WRITE
#endif


/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMAMEM_NOSYNC	0x04	/* map memory to not require sync */
#define	BUS_DMA_BUS1		0x10	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x20
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 *	bus_dmasync_op_t
 *
 *	Operations performed by bus_dmamap_sync().
 */
typedef enum {
	BUS_DMASYNC_PREREAD,
	BUS_DMASYNC_POSTREAD,
	BUS_DMASYNC_PREWRITE,
	BUS_DMASYNC_POSTWRITE,
} bus_dmasync_op_t;

typedef struct i386_bus_dma_tag		*bus_dma_tag_t;
typedef struct i386_bus_dmamap		*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
struct i386_bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
};
typedef struct i386_bus_dma_segment	bus_dma_segment_t;

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the implementation of
 *	DMA for a given bus.
 */

struct i386_bus_dma_tag {
	void	*_cookie;		/* cookie used in the guts */

	/*
	 * DMA mapping methods.
	 */
	int	(*_dmamap_create) __P((bus_dma_tag_t, bus_size_t, int,
		    bus_size_t, bus_size_t, int, bus_dmamap_t *));
	void	(*_dmamap_destroy) __P((bus_dma_tag_t, bus_dmamap_t));
	int	(*_dmamap_load) __P((bus_dma_tag_t, bus_dmamap_t, void *,
		    bus_size_t, struct proc *, int));
	int	(*_dmamap_load_mbuf) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct mbuf *, int));
	int	(*_dmamap_load_uio) __P((bus_dma_tag_t, bus_dmamap_t,
		    struct uio *, int));
	int	(*_dmamap_load_raw) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_dma_segment_t *, int, bus_size_t, int));
	void	(*_dmamap_unload) __P((bus_dma_tag_t, bus_dmamap_t));
	void	(*_dmamap_sync) __P((bus_dma_tag_t, bus_dmamap_t,
		    bus_dmasync_op_t));

	/*
	 * DMA memory utility functions.
	 */
	int	(*_dmamem_alloc) __P((bus_dma_tag_t, bus_size_t, bus_size_t,
		    bus_size_t, bus_dma_segment_t *, int, int *, int));
	void	(*_dmamem_free) __P((bus_dma_tag_t,
		    bus_dma_segment_t *, int));
	int	(*_dmamem_map) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, size_t, caddr_t *, int));
	void	(*_dmamem_unmap) __P((bus_dma_tag_t, caddr_t, size_t));
	int	(*_dmamem_mmap) __P((bus_dma_tag_t, bus_dma_segment_t *,
		    int, int, int, int));
};

#define	bus_dmamap_create(t, s, n, m, b, f, p)			\
	(*(t)->_dmamap_create)((t), (s), (n), (m), (b), (f), (p))
#define	bus_dmamap_destroy(t, p)				\
	(*(t)->_dmamap_destroy)((t), (p))
#define	bus_dmamap_load(t, m, b, s, p, f)			\
	(*(t)->_dmamap_load)((t), (m), (b), (s), (p), (f))
#define	bus_dmamap_load_mbuf(t, m, b, f)			\
	(*(t)->_dmamap_load_mbuf)((t), (m), (b), (f))
#define	bus_dmamap_load_uio(t, m, u, f)				\
	(*(t)->_dmamap_load_uio)((t), (m), (u), (f))
#define	bus_dmamap_load_raw(t, m, sg, n, s, f)			\
	(*(t)->_dmamap_load_raw)((t), (m), (sg), (n), (s), (f))
#define	bus_dmamap_unload(t, p)					\
	(*(t)->_dmamap_unload)((t), (p))
#define	bus_dmamap_sync(t, p, o)				\
	(void)((t)->_dmamap_sync ?				\
	    (*(t)->_dmamap_sync)((t), (p), (o)) : (void)0)

#define	bus_dmamem_alloc(t, s, a, b, sg, n, r, f)		\
	(*(t)->_dmamem_alloc)((t), (s), (a), (b), (sg), (n), (r), (f))
#define	bus_dmamem_free(t, sg, n)				\
	(*(t)->_dmamem_free)((t), (sg), (n))
#define	bus_dmamem_map(t, sg, n, s, k, f)			\
	(*(t)->_dmamem_map)((t), (sg), (n), (s), (k), (f))
#define	bus_dmamem_unmap(t, k, s)				\
	(*(t)->_dmamem_unmap)((t), (k), (s))
#define	bus_dmamem_mmap(t, sg, n, o, p, f)			\
	(*(t)->_dmamem_mmap)((t), (sg), (n), (o), (p), (f))

/*
 *	bus_dmamap_t
 *
 *	Describes a DMA mapping.
 */
struct i386_bus_dmamap {
	/*
	 * PRIVATE MEMBERS: not for use my machine-independent code.
	 */
	bus_size_t	_dm_size;	/* largest DMA transfer mappable */
	int		_dm_segcnt;	/* number of segs this map can map */
	bus_size_t	_dm_maxsegsz;	/* largest possible segment */
	bus_size_t	_dm_boundary;	/* don't cross this */
	int		_dm_flags;	/* misc. flags */

	void		*_dm_cookie;	/* cookie for bus-specific functions */

	/*
	 * PUBLIC MEMBERS: these are used by machine-independent code.
	 */
	int		dm_nsegs;	/* # valid segments in mapping */
	bus_dma_segment_t dm_segs[1];	/* segments; variable length */
};

#ifdef _I386_BUS_DMA_PRIVATE
int	_bus_dmamap_create __P((bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *));
void	_bus_dmamap_destroy __P((bus_dma_tag_t, bus_dmamap_t));
int	_bus_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int));
int	_bus_dmamap_load_mbuf __P((bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int));
int	_bus_dmamap_load_uio __P((bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int));
int	_bus_dmamap_load_raw __P((bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int));
void	_bus_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void	_bus_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t, bus_dmasync_op_t));

int	_bus_dmamem_alloc __P((bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags));
void	_bus_dmamem_free __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs));
int	_bus_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, size_t size, caddr_t *kvap, int flags));
void	_bus_dmamem_unmap __P((bus_dma_tag_t tag, caddr_t kva,
	    size_t size));
int	_bus_dmamem_mmap __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
	    int nsegs, int off, int prot, int flags));

int	_bus_dmamem_alloc_range __P((bus_dma_tag_t tag, bus_size_t size,
	    bus_size_t alignment, bus_size_t boundary,
	    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
	    vm_offset_t low, vm_offset_t high));
#endif /* _I386_BUS_DMA_PRIVATE */

#endif /* _I386_BUS_H_ */
