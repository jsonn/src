/*	$NetBSD: pio.h,v 1.1.2.1 1997/11/28 19:43:56 mellon Exp $ */
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

#ifndef _MACHINE_PIO_H_
#define _MACHINE_PIO_H_
/*
 * I/O macros.
 */

static __inline void
__outb(a,v)
	volatile u_int8_t *a;
	int v;
{
	*a = v;
	__asm__ volatile("eieio; sync");
}

static __inline void
__outw(a,v)
	volatile u_int16_t *a;
	u_int16_t v;
{
	*a = v;
	__asm__ volatile("eieio; sync");
}

static __inline void
__outl(a,v)
	volatile u_int32_t *a;
	int v;
{
	*a = v;
	__asm__ volatile("eieio; sync");
}

static __inline void
__outwrb(a,v)
	volatile u_int16_t *a;
	u_int16_t v;
{
	u_int32_t _p_ = (u_int32_t)a;

	__asm__ volatile("sthbrx %0, 0, %1" :: "r"(v), "r"(_p_));
	__asm__ volatile("eieio; sync");
}

static __inline void
__outlrb(a,v)
	volatile u_int32_t *a;
	u_int32_t v;
{
	u_int32_t _p_ = (u_int32_t)a;

	__asm__ volatile("stwbrx %0, 0, %1" :: "r"(v), "r"(_p_));
	__asm__ volatile("eieio; sync");
}

static __inline u_int8_t
__inb(a)
	volatile u_int8_t *a;
{
	u_int8_t _v_;

	_v_ = *a;
	__asm__ volatile("eieio; sync");
	return _v_;
}

static __inline u_int16_t
__inw(a)
	volatile u_int16_t *a;
{
	u_int16_t _v_;

	_v_ = *a;
	__asm__ volatile("eieio; sync");
	return _v_;
}

static __inline u_int32_t
__inl(a)
	volatile u_int32_t *a;
{
	u_int32_t _v_;

	_v_ = *a;
	__asm__ volatile("eieio; sync");
	return _v_;
}

static __inline u_int16_t
__inwrb(a)
	volatile u_int16_t *a;
{
	u_int16_t _v_;
	u_int32_t _p_ = (u_int32_t)a;

	__asm__ volatile("lhbrx %0, 0, %1" : "=r"(_v_) : "r"(_p_));
	__asm__ volatile("eieio; sync");
	return _v_;
}

static __inline u_int32_t
__inlrb(a)
	volatile u_int32_t *a;
{
	u_int32_t _v_;
	u_int32_t _p_ = (u_int32_t)a;

	__asm__ volatile("lwbrx %0, 0, %1" : "=r"(_v_) : "r"(_p_));
	__asm__ volatile("eieio; sync");
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

#endif /*_MACHINE_PIO_H_*/
