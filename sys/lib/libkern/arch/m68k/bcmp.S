/*	$NetBSD: bcmp.S,v 1.6 1997/05/15 16:20:54 jtc Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)bcmp.s 5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: bcmp.S,v 1.6 1997/05/15 16:20:54 jtc Exp $")
#endif
#endif /* LIBC_SCCS and not lint */

ENTRY(bcmp)
	movl	sp@(4),a0		| string 1
	movl	sp@(8),a1		| string 2
	movl	sp@(12),d1		| length

	/*
	 * It isn't worth the overhead of aligning to {long}word boundries
	 * if the string is too short.
	 */
	cmpl	#8,d1
	jlt	Lbcbyte			

	/* word align */
	movl	a0,d0
	btst	#0,d0
	jeq	Lbcalgndw
	cmpmb	a0@+,a1@+
	jne	Lbcnoteq
	subql	#1,d1
Lbcalgndw:
	/* long word align */
	btst	#1,d0
	jeq	Lbcalgndl
	cmpmw	a0@+,a1@+
	jne	Lbcnoteq
	subql	#2,d1
Lbcalgndl:
	/* compare by 8 longwords */
	movl	d1,d0
	lsrl	#5,d0			| cnt = len / 32
	jeq	Lbclong			| if (cnt)
	andl	#31,d1			|	len %= 32
	subql	#1,d0			|	set up for dbf
Lbc32loop:
	cmpml	a0@+,a1@+		|	compare 8 longwords
	jne	Lbcnoteq		|	not equal, return non-zero
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	cmpml	a0@+,a1@+
	jne	Lbcnoteq
	dbf	d0,Lbc32loop		|	till done
	clrw	d0
	subql	#1,d0
	jcc	Lbc32loop

Lbclong:
	/* compare by longwords */
	movl	d1,d0
	lsrl	#2,d0			| cnt = len / 4
	jeq	Lbcbyte			| if (cnt)
	subql	#1,d0			|	set up for dbf
Lbclloop:
	cmpml	a0@+,a1@+		|	compare a longword
	jne	Lbcnoteq		|	not equal, return non-zero
	dbf	d0,Lbclloop		|	till done
	andl	#3,d1			|	len %= 4
	jeq	Lbcdone 

	subql	#1,d1			| set up for dbf
Lbcbloop:
	cmpmb	a0@+,a1@+		| compare a byte
	jne	Lbcnoteq		| not equal, return non-zero
Lbcbyte:
	dbf	d1,Lbcbloop
Lbcdone:
	movql	#0,d0
	rts

Lbcnoteq:
	movql	#1,d0
	rts
