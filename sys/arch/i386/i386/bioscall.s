/*	$NetBSD: bioscall.s,v 1.1.2.2 1997/10/15 05:26:53 thorpej Exp $ */
/*
 *  Copyright (c) 1997 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <machine/param.h>
#include <machine/bioscall.h>

#include <machine/asm.h>

	.globl	_PTDpaddr	/* from locore.s */
	
_biostramp_image:
	.globl	_biostramp_image

8:
#include "i386/bioscall/biostramp.inc"
9:

_biostramp_image_size:
	.globl	_biostramp_image_size
	.long	9b - 8b

/*
 * void bioscall(int function, struct apmregs *regs):
 * 	call the BIOS interrupt "function" from real mode with
 *	registers as specified in "regs"
 *	for the flags, though, only these flags are passed to the BIOS--
 *	the remainder come from the flags register at the time of the call:
 *	(PSL_C|PSL_PF|PSL_AF|PSL_Z|PSL_N|PSL_D|PSL_V)
 *
 *	Fills in *regs with registers as returned by BIOS.
 */
NENTRY(bioscall)
	pushl	%ebp
	movl	%esp,%ebp		/* set up frame ptr */
	
	movl	%cr3,%eax		/* save PTDB register */
	pushl	%eax
	
	movl	_PTDpaddr,%eax		/* install proc0 PTD */
	movl	%eax,%cr3

	movl $(BIOSTRAMP_BASE),%eax	/* address of trampoline area */
	pushl 12(%ebp)
	pushl 8(%ebp)
	call %eax			/* machdep.c initializes it */
	addl $8,%esp			/* clear args from stack */
		
	popl %eax
	movl %eax,%cr3			/* restore PTDB register */
	
	leave
	ret
