/*	$NetBSD: asm.h,v 1.9.16.1 2006/06/21 14:56:12 yamt Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)asm.h	8.1 (Berkeley) 6/11/93
 */

/*
 * GCC __asm constructs for doing assembly stuff.
 */

/*
 * ``Routines'' to load and store from/to alternate address space.
 * The location can be a variable, the asi value (address space indicator)
 * must be a constant.
 *
 * N.B.: You can put as many special functions here as you like, since
 * they cost no kernel space or time if they are not used.
 *
 * These were static inline functions, but gcc screws up the constraints
 * on the address space identifiers (the "n"umeric value part) because
 * it inlines too late, so we have to use the funny valued-macro syntax.
 */

/* load byte from alternate address space */
#define	lduba(loc, asi) ({ \
	register int _lduba_v; \
	__asm volatile("lduba [%1]%2,%0" : "=r" (_lduba_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lduba_v; \
})

/* load half-word from alternate address space */
#define	lduha(loc, asi) ({ \
	register int _lduha_v; \
	__asm volatile("lduha [%1]%2,%0" : "=r" (_lduha_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lduha_v; \
})

/* load int from alternate address space */
#define	lda(loc, asi) ({ \
	register int _lda_v; \
	__asm volatile("lda [%1]%2,%0" : "=r" (_lda_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lda_v; \
})

/* store byte to alternate address space */
#define	stba(loc, asi, value) ({ \
	__asm volatile("stba %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* store half-word to alternate address space */
#define	stha(loc, asi, value) ({ \
	__asm volatile("stha %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* store int to alternate address space */
#define	sta(loc, asi, value) ({ \
	__asm volatile("sta %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* load 64-bit int from alternate address space */
#define	ldda(loc, asi) ({ \
	register long long _lda_v; \
	__asm volatile("ldda [%1]%2,%0" : "=r" (_lda_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lda_v; \
})

/* store 64-bit int to alternate address space */
#define	stda(loc, asi, value) ({ \
	__asm volatile("stda %0,[%1]%2" : : \
	    "r" ((long long)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* atomic swap of a word between a register and memory */
#define	swap(loc, val) ({ \
	__asm volatile("swap [%2],%0" : "=&r" (val) : "0" (val), "r" (loc)); \
})

/* atomic load/store of a byte in memory */
#define	ldstub(loc) ({ \
	int _v; \
	__asm volatile("ldstub [%1],%0" : "=r" (_v) : "r" (loc) : "memory"); \
	_v; \
})

/* read ancillary state register */
#define	rdasr(asr) _rdasr(asr)
#define	_rdasr(asr) ({ \
	register int _rdasr_v; \
	__asm volatile("rd %%asr" #asr ",%0" : "=r" (_rdasr_v)); \
	_rdasr_v; \
})

/* write ancillary state register */
#define	wrasr(value, asr) _wrasr(value, asr)
#define	_wrasr(value, asr) ({ \
	__asm volatile("wr %0,%%asr" #asr : : "r" ((int)(value))); \
})
