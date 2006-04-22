/*	$NetBSD: pio.h,v 1.4.6.1 2006/04/22 11:37:53 simonb Exp $ */
/*	$OpenBSD: pio.h,v 1.1 1997/10/13 10:53:47 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom, Opsycon AB and RTMX Inc, USA.
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom Opsycon AB for RTMX Inc, North Carolina, USA.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _POWERPC_PIO_H_
#define _POWERPC_PIO_H_
/*
 * I/O macros.
 */

static __inline void __outb __P((volatile u_int8_t *a, u_int8_t v));
static __inline void __outw __P((volatile u_int16_t *a, u_int16_t v));
static __inline void __outl __P((volatile u_int32_t *a, u_int32_t v));
static __inline void __outwrb __P((volatile u_int16_t *a, u_int16_t v));
static __inline void __outlrb __P((volatile u_int32_t *a, u_int32_t v));
static __inline u_int8_t __inb __P((volatile u_int8_t *a));
static __inline u_int16_t __inw __P((volatile u_int16_t *a));
static __inline u_int32_t __inl __P((volatile u_int32_t *a));
static __inline u_int16_t __inwrb __P((volatile u_int16_t *a));
static __inline u_int32_t __inlrb __P((volatile u_int32_t *a));
static __inline void __outsb __P((volatile u_int8_t *, const u_int8_t *,
	size_t));
static __inline void __outsw __P((volatile u_int16_t *, const u_int16_t *,
	size_t));
static __inline void __outsl __P((volatile u_int32_t *, const u_int32_t *,
	size_t));
static __inline void __outswrb __P((volatile u_int16_t *, const u_int16_t *,
	size_t));
static __inline void __outslrb __P((volatile u_int32_t *, const u_int32_t *,
	size_t));
static __inline void __insb __P((volatile u_int8_t *, u_int8_t *, size_t));
static __inline void __insw __P((volatile u_int16_t *, u_int16_t *, size_t));
static __inline void __insl __P((volatile u_int32_t *, u_int32_t *, size_t));
static __inline void __inswrb __P((volatile u_int16_t *, u_int16_t *, size_t));
static __inline void __inslrb __P((volatile u_int32_t *, u_int32_t *, size_t));

static __inline void
__outb(a,v)
	volatile u_int8_t *a;
	u_int8_t v;
{
	*a = v;
	__asm volatile("eieio; sync");
}

static __inline void
__outw(a,v)
	volatile u_int16_t *a;
	u_int16_t v;
{
	*a = v;
	__asm volatile("eieio; sync");
}

static __inline void
__outl(a,v)
	volatile u_int32_t *a;
	u_int32_t v;
{
	*a = v;
	__asm volatile("eieio; sync");
}

static __inline void
__outwrb(a,v)
	volatile u_int16_t *a;
	u_int16_t v;
{
	__asm volatile("sthbrx %0, 0, %1" :: "r"(v), "r"(a));
	__asm volatile("eieio; sync");
}

static __inline void
__outlrb(a,v)
	volatile u_int32_t *a;
	u_int32_t v;
{
	__asm volatile("stwbrx %0, 0, %1" :: "r"(v), "r"(a));
	__asm volatile("eieio; sync");
}

static __inline u_int8_t
__inb(a)
	volatile u_int8_t *a;
{
	u_int8_t _v_;

	_v_ = *a;
	__asm volatile("eieio; sync");
	return _v_;
}

static __inline u_int16_t
__inw(a)
	volatile u_int16_t *a;
{
	u_int16_t _v_;

	_v_ = *a;
	__asm volatile("eieio; sync");
	return _v_;
}

static __inline u_int32_t
__inl(a)
	volatile u_int32_t *a;
{
	u_int32_t _v_;

	_v_ = *a;
	__asm volatile("eieio; sync");
	return _v_;
}

static __inline u_int16_t
__inwrb(a)
	volatile u_int16_t *a;
{
	u_int16_t _v_;

	__asm volatile("lhbrx %0, 0, %1" : "=r"(_v_) : "r"(a));
	__asm volatile("eieio; sync");
	return _v_;
}

static __inline u_int32_t
__inlrb(a)
	volatile u_int32_t *a;
{
	u_int32_t _v_;

	__asm volatile("lwbrx %0, 0, %1" : "=r"(_v_) : "r"(a));
	__asm volatile("eieio; sync");
	return _v_;
}

#define	outb(a,v)	(__outb((volatile u_int8_t *)(a), v))
#define	out8(a,v)	outb(a,v)
#define	outw(a,v)	(__outw((volatile u_int16_t *)(a), v))
#define	out16(a,v)	outw(a,v)
#define	outl(a,v)	(__outl((volatile u_int32_t *)(a), v))
#define	out32(a,v)	outl(a,v)
#define	inb(a)		(__inb((volatile u_int8_t *)(a)))
#define	in8(a)		inb(a)
#define	inw(a)		(__inw((volatile u_int16_t *)(a)))
#define	in16(a)		inw(a)
#define	inl(a)		(__inl((volatile u_int32_t *)(a)))
#define	in32(a)		inl(a)

#define	out8rb(a,v)	outb(a,v)
#define	outwrb(a,v)	(__outwrb((volatile u_int16_t *)(a), v))
#define	out16rb(a,v)	outwrb(a,v)
#define	outlrb(a,v)	(__outlrb((volatile u_int32_t *)(a), v))
#define	out32rb(a,v)	outlrb(a,v)
#define	in8rb(a)	inb(a)
#define	inwrb(a)	(__inwrb((volatile u_int16_t *)(a)))
#define	in16rb(a)	inwrb(a)
#define	inlrb(a)	(__inlrb((volatile u_int32_t *)(a)))
#define	in32rb(a)	inlrb(a)


static __inline void
__outsb(a,s,c)
	volatile u_int8_t *a;
	const u_int8_t *s;
	size_t c;
{
	while (c--)
		*a = *s++;
	__asm volatile("eieio; sync");
}

static __inline void
__outsw(a,s,c)
	volatile u_int16_t *a;
	const u_int16_t *s;
	size_t c;
{
	while (c--)
		*a = *s++;
	__asm volatile("eieio; sync");
}

static __inline void
__outsl(a,s,c)
	volatile u_int32_t *a;
	const u_int32_t *s;
	size_t c;
{
	while (c--)
		*a = *s++;
	__asm volatile("eieio; sync");
}

static __inline void
__outswrb(a,s,c)
	volatile u_int16_t *a;
	const u_int16_t *s;
	size_t c;
{
	while (c--)
		__asm volatile("sthbrx %0, 0, %1" :: "r"(*s++), "r"(a));
	__asm volatile("eieio; sync");
}

static __inline void
__outslrb(a,s,c)
	volatile u_int32_t *a;
	const u_int32_t *s;
	size_t c;
{
	while (c--)
		__asm volatile("stwbrx %0, 0, %1" :: "r"(*s++), "r"(a));
	__asm volatile("eieio; sync");
}

static __inline void
__insb(a,d,c)
	volatile u_int8_t *a;
	u_int8_t *d;
	size_t c;
{
	while (c--)
		*d++ = *a;
	__asm volatile("eieio; sync");
}

static __inline void
__insw(a,d,c)
	volatile u_int16_t *a;
	u_int16_t *d;
	size_t c;
{
	while (c--)
		*d++ = *a;
	__asm volatile("eieio; sync");
}

static __inline void
__insl(a,d,c)
	volatile u_int32_t *a;
	u_int32_t *d;
	size_t c;
{
	while (c--)
		*d++ = *a;
	__asm volatile("eieio; sync");
}

static __inline void
__inswrb(a,d,c)
	volatile u_int16_t *a;
	u_int16_t *d;
	size_t c;
{
	while (c--)
		__asm volatile("lhbrx %0, 0, %1" : "=r"(*d++) : "r"(a));
	__asm volatile("eieio; sync");
}

static __inline void
__inslrb(a,d,c)
	volatile u_int32_t *a;
	u_int32_t *d;
	size_t c;
{
	while (c--)
		__asm volatile("lwbrx %0, 0, %1" : "=r"(*d++) : "r"(a));
	__asm volatile("eieio; sync");
}

#define	outsb(a,s,c)	(__outsb((volatile u_int8_t *)(a), s, c))
#define	outs8(a,s,c)	outsb(a,s,c)
#define	outsw(a,s,c)	(__outsw((volatile u_int16_t *)(a), s, c))
#define	outs16(a,s,c)	outsw(a,s,c)
#define	outsl(a,s,c)	(__outsl((volatile u_int32_t *)(a), s, c))
#define	outs32(a,s,c)	outsl(a,s,c)
#define	insb(a,d,c)	(__insb((volatile u_int8_t *)(a), d, c))
#define	ins8(a,d,c)	insb(a,d,c)
#define	insw(a,d,c)	(__insw((volatile u_int16_t *)(a), d, c))
#define	ins16(a,d,c)	insw(a,d,c)
#define	insl(a,d,c)	(__insl((volatile u_int32_t *)(a), d, c))
#define	ins32(a,d,c)	insl(a,d,c)

#define	outs8rb(a,s,c)	outsb(a,s,c)
#define	outswrb(a,s,c)	(__outswrb((volatile u_int16_t *)(a), s, c))
#define	outs16rb(a,s,c)	outswrb(a,s,c)
#define	outslrb(a,s,c)	(__outslrb((volatile u_int32_t *)(a), s, c))
#define	outs32rb(a,s,c)	outslrb(a,s,c)
#define	ins8rb(a,d,c)	insb(a,d,c)
#define	inswrb(a,d,c)	(__inswrb((volatile u_int16_t *)(a), d, c))
#define	ins16rb(a,d,c)	inswrb(a,d,c)
#define	inslrb(a,d,c)	(__inslrb((volatile u_int32_t *)(a), d, c))
#define	ins32rb(a,d,c)	inslrb(a,d,c)

#endif /*_POWERPC_PIO_H_*/
