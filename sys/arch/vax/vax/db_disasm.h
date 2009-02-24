/*	$NetBSD: db_disasm.h,v 1.5.14.1 2009/02/24 03:01:10 snj Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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



#define SIZE_BYTE	 1		/* Byte */
#define SIZE_WORD	 2		/* Word */
#define SIZE_LONG 	 4		/* Longword */
#define SIZE_QWORD	 8		/* Quadword */
#define SIZE_OWORD	16		/* Octaword */

/*
 * The VAX instruction set has a variable length instruction format which 
 * may be as short as one byte and as long as needed depending on the type 
 * of instruction. [...] Each instruction consists of an opcode followed 
 * by zero to six operand specifiers whose number and type depend on the 
 * opcode. All operand specidiers are, themselves, of the same format -- 
 * i.e. an address mode plus additional information.
 *
 * [VAX Architecture Handbook, p.52:  Instruction Format]
 */

typedef const struct {
	const char *mnemonic;
	const char *argdesc;
} vax_instr_t;

extern vax_instr_t vax_inst[256];
extern vax_instr_t vax_inst2[0x56];

long skip_opcode(long);
