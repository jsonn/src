/* $NetBSD: profile.h,v 1.4.10.2 1997/10/15 05:37:23 thorpej Exp $ */

/*
 * Copyright (c) 1995-1996 Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define	_MCOUNT_DECL void _mcount

/*
 * Cannot implement mcount in C as GCC will trash the ip register when it
 * pushes a trapframe. Pity we cannot insert assembly before the function
 * prologue.
 */
#define	MCOUNT								\
	asm(".text");							\
	asm(".align	0");						\
	asm(".type	mcount,@function");				\
	asm(".global	mcount");					\
	asm("mcount:");							\
	/*								\
	 * Preserve registers that are trashed during mcount		\
	 */								\
	asm("stmfd	sp!, {r0-r3, lr}");				\
	/*								\
	 * find the return address for mcount,				\
	 * and the return address for mcount's caller.			\
	 *								\
	 * frompcindex = pc pushed by call into self.			\
	 */								\
	asm("mov	r0, ip");					\
	/*								\
	 * selfpc = pc pushed by mcount call				\
	 */								\
	asm("mov	r1, lr");					\
	/*								\
	 * Call the real mcount code					\
	 */								\
	asm("bl	__mcount");						\
	/*								\
	 * Restore registers that were trashed during mcount		\
	 */								\
	asm("ldmfd	sp!, {r0-r3, pc}");

#ifdef _KERNEL
/*
 * Note that we assume splhigh() and splx() cannot call mcount()
 * recursively.
 */
#define	MCOUNT_ENTER	s = splhigh()
#define	MCOUNT_EXIT	splx(s)
#endif /* _KERNEL */
