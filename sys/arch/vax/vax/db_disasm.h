/*	$NetBSD: db_disasm.h,v 1.4.74.1 2008/04/03 12:42:28 mjf Exp $ */
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

const struct {
	const char *mnemonic;
	const char *argdesc;
} vax_inst[256] = {

/*
 * argdesc describes each arguments by two characters denoting
 * the access-type and the data-type.
 *
 * Arguments (Access-Types):
 *	r: operand is read only
 *	w: operand is written only
 *	m: operand is modified (both R and W)
 *	b: no operand reference. Branch displacement is specified. 
 *	a: calculate the address of the specified operand
 *	v: if not "Rn", same as a. If "RN," R[n+1]R[n]
 * Arguments (Data-Types):
 *	b: Byte
 *	w: Word
 *	l: Longword
 *	q: Quadword
 *	o: Octaword
 *	d: D_floating
 *	f: F_floating
 *	g: G_floating
 *	h: H_floating
 *	r: Register
 *	x: first data type specified by instruction
 *	y: second data type spcified by instructin
 *	-: no-args
 *	?: unknown (variable?)
 */

/* 0x00 */	{	"halt", 	""			},
/* 0x01 */	{	"nop", 		""			},
/* 0x02 */	{	"rei", 		""			},
/* 0x03 */	{	"bpt", 		""			},
/* 0x04 */	{	"ret", 		""			},
/* 0x05 */	{	"rsb", 		""			},
/* 0x06 */	{	"ldpctx", 	""			},
/* 0x07 */	{	"svpctx", 	""			},
/* 0x08 */	{	"cvtps", 	"rw,ab,rw,ab"		},
/* 0x09 */	{	"cvtsp", 	"rw,ab,rw,ab"		},
/* 0x0a */	{	"index", 	"rl,rl,rl,rl,rl,wl" 	},
/* 0x0b */	{	"crc", 		"ab,rl,rw,ab"		},
/* 0x0c */	{	"prober", 	"rb,rw,ab"		},
/* 0x0d */	{	"probew", 	"rb,rw,ab"		},
/* 0x0e */	{	"insque", 	"ab,wl"			},
/* 0x0f */	{	"remque", 	"ab,wl"			},
	   
/* 0x10 */	{	"bsbb", 	"bb"			},
/* 0x11 */	{	"brb", 		"bb"			},
/* 0x12 */	{	"*bneq",	"bb"			},
/* 0x13 */	{	"*beql", 	"bb"			},
/* 0x14 */	{	"bgtr", 	"bb"			},
/* 0x15 */	{	"bleq", 	"bb"			},
/* 0x16 */	{	"jsb", 		"ab"			},
/* 0x17 */	{	"jmp", 		"ab"			},
/* 0x18 */	{	"bgeq", 	"bb"			},
/* 0x19 */	{	"blss", 	"bb"			},
/* 0x1a */	{	"bgtru", 	"bb"			},
/* 0x1b */	{	"blequ", 	"bb"			},
/* 0x1c */	{	"bvc", 		"bb"			},
/* 0x1d */	{	"bvs", 		"bb"			},
/* 0x1e */	{	"*bcc", 	"bb"			},
/* 0x1f */	{	"*bcs", 	"bb"			},
	   
/* 0x20 */	{	"addp4", 	"rw,ab,rw,ab"		},
/* 0x21 */	{	"addp6", 	"rw,ab,rw,ab,rw,ab"	},
/* 0x22 */	{	"subp4", 	"rw,ab,rw,ab"		},
/* 0x23 */	{	"subp6", 	"rw,ab,rw,ab,rw,ab"	},
/* 0x24 */	{	"cvtpt", 	"rw,ab,ab,rw,ab"	},
/* 0x25 */	{	"mulp", 	"rw,ab,rw,ab,rw,ab"	},
/* 0x26 */	{	"cvttp", 	"rw,ab,ab,rw,ab"	},
/* 0x27 */	{	"divp", 	"rw,ab,rw,ab,rw,ab"	},
/* 0x28 */	{	"movc3", 	"rw,ab,ab"		},
/* 0x29 */	{	"cmpc3", 	"rw,ab,ab"		},
/* 0x2a */	{	"scanc", 	"rw,ab,ab,rb"		},
/* 0x2b */	{	"spanc", 	"rw,ab,ab,rb"		},
/* 0x2c */	{	"movc5", 	"rw,ab,rb,rw,ab"	},
/* 0x2d */	{	"cmpc5", 	"rw,ab,rb,rw,ab"	},
/* 0x2e */	{	"movtc", 	"rw,ab,rb,ab,rw,ab"	},
/* 0x2f */	{	"movtuc", 	"rw,ab,rb,ab,rw,ab"	},
	   
/* 0x30 */	{	"bsbw", 	"bw"			},
/* 0x31 */	{	"brw", 		"bw"			},
/* 0x32 */	{	"cvtwl", 	"rw,wl"			},
/* 0x33 */	{	"cvtwb", 	"rw,wb"			},
/* 0x34 */	{	"movp", 	"rw,ab,ab"		},
/* 0x35 */	{	"cmpp3", 	"rw,ab,ab"		},
/* 0x36 */	{	"cvtpl", 	"rw,ab,wl"		},
/* 0x37 */	{	"cmpp4", 	"rw,ab,rw,ab"		},
/* 0x38 */	{	"editpc", 	"rw,ab,ab,ab"		},
/* 0x39 */	{	"matchc", 	"rw,ab,rw,ab"		},
/* 0x3a */	{	"locc", 	"rb,rw,ab"		},
/* 0x3b */	{	"skpc", 	"rb,rw,ab"		},
/* 0x3c */	{	"movzwl", 	"rw,wl"			},
/* 0x3d */	{	"acbw", 	"rw,rw,mw,bw"		},
/* 0x3e */	{	"movaw", 	"aw,wl"			},
/* 0x3f */	{	"pushaw", 	"aw"			},
	   
/* 0x40 */	{	"addf2", 	"rf,mf"			},
/* 0x41 */	{	"addf3", 	"rf,rf,wf"		},
/* 0x42 */	{	"subf2", 	"rf,mf"			},
/* 0x43 */	{	"subf3", 	"rf,rf,wf"		},
/* 0x44 */	{	"mulf2", 	"rf,mf"			},
/* 0x45 */	{	"mulf3", 	"rf,rf,wf"		},
/* 0x46 */	{	"divf2", 	"rf,mf"			},
/* 0x47 */	{	"divf3", 	"rf,rf,wf"		},
/* 0x48 */	{	"cvtfb", 	"rf,wb"			},
/* 0x49 */	{	"cvtfw", 	"rf,ww"			},
/* 0x4a */	{	"cvtfl", 	"rf,wl"			},
/* 0x4b */	{	"cvtrfl", 	"rf,wl"			},
/* 0x4c */	{	"cvtbf", 	"rb,wf"			},
/* 0x4d */	{	"cvtwf", 	"rw,wf"			},
/* 0x4e */	{	"cvtlf", 	"rl,wf"			},
/* 0x4f */	{	"acbf", 	"rf,rf,rf,bw"		},
	   
/* 0x50 */	{	"movf", 	"rf,wf"			},
/* 0x51 */	{	"cmpf", 	"rf,rf"			},
/* 0x52 */	{	"mnegf", 	"rf,wf"			},
/* 0x53 */	{	"tstf", 	"rf"			},
/* 0x54 */	{	"emodf", 	"rf,rb,rf,wl,wf"	},
/* 0x55 */	{	"polyf", 	"rf,rw,ab"		},
/* 0x56 */	{	"cvtfd", 	"rf,wd"			},
/* 0x57 */	{	"-reserved-", 	""			},
/* 0x58 */	{	"adawi", 	"rw,mw"			},
/* 0x59 */	{	"-reserved-", 	""			},
/* 0x5a */	{	"-reserved-", 	""			},
/* 0x5b */	{	"-reserved-", 	""			},
/* 0x5c */	{	"insqhi", 	"ab,aq"			},
/* 0x5d */	{	"insqti", 	"ab,aq"			},
/* 0x5e */	{	"remqhi", 	"aq,wl"			},
/* 0x5f */	{	"remqti", 	"aq,wl"			},
	   
/* 0x60 */	{	"addd2", 	"rd,md"			},
/* 0x61 */	{	"addd3", 	"rd,rd,wd"		},
/* 0x62 */	{	"subd2", 	"rd,md"			},
/* 0x63 */	{	"subd3", 	"rd,rd,wd"		},
/* 0x64 */	{	"muld2", 	"rd,md"			},
/* 0x65 */	{	"muld3", 	"rd,rd,wd"		},
/* 0x66 */	{	"divd2", 	"rd,md"			},
/* 0x67 */	{	"divd3", 	"rd,rd,wd"		},
/* 0x68 */	{	"cvtdb", 	"rd,wb"			},
/* 0x69 */	{	"cvtdw", 	"rd,ww"			},
/* 0x6a */	{	"cvtdl", 	"rd,wl"			},
/* 0x6b */	{	"cvtrdl", 	"rd,wl"			},
/* 0x6c */	{	"cvtbd", 	"rb,wd"			},
/* 0x6d */	{	"cvtwd", 	"rw,wd"			},
/* 0x6e */	{	"cvtld", 	"rl,wd"			},
/* 0x6f */	{	"acbd", 	"rd,rd,md,bw"		},
	   
/* 0x70 */	{	"movd", 	"rd,wd"			},
/* 0x71 */	{	"cmpd", 	"rd,rd"			},
/* 0x72 */	{	"mnegd", 	"rd,wd"			},
/* 0x73 */	{	"tstd", 	"rd"			},
/* 0x74 */	{	"emodd", 	"rd,rb,rd,wl,wd"	},
/* 0x75 */	{	"polyd", 	"rd,rw,ab"		},
/* 0x76 */	{	"cvtdf", 	"rd,wf"			},
/* 0x77 */	{	"-reserved-", 	""			},
/* 0x78 */	{	"ashl", 	"rb,rl,wl"		},
/* 0x79 */	{	"ashq", 	"rb,rq,wq"		},
/* 0x7a */	{	"emul", 	"rl,rl,rl,wq"		},
/* 0x7b */	{	"ediv", 	"rl,rq,wl,wl"		},
/* 0x7c */	{	"*clrq", 	"wq"			},
/* 0x7d */	{	"movq", 	"rq,wq"			},
/* 0x7e */	{	"*movaq", 	"aq,wl"			},
/* 0x7f */	{	"*pushaq", 	"aq"			},
	   
/* 0x80 */	{	"addb2", 	"rb,mb"			},
/* 0x81 */	{	"addb3", 	"rb,rb,wb"		},
/* 0x82 */	{	"subb2", 	"rb,mb"			},
/* 0x83 */	{	"subb3", 	"rb,rb,wb"		},
/* 0x84 */	{	"mulb2", 	"rb,mb"			},
/* 0x85 */	{	"mulb3", 	"rb,rb,wb"		},
/* 0x86 */	{	"divb2", 	"rb,mb"			},
/* 0x87 */	{	"divb3", 	"rb,rb,wb"		},
/* 0x88 */	{	"bisb2", 	"rb,mb"			},
/* 0x89 */	{	"bisb3", 	"rb,rb,wb"		},
/* 0x8a */	{	"bicb2", 	"rb,mb"			},
/* 0x8b */	{	"bicb3", 	"rb,rb,wb"		},
/* 0x8c */	{	"xorb2", 	"rb,mb"			},
/* 0x8d */	{	"xorb3", 	"rb,rb,wb"		},
/* 0x8e */	{	"mnegb", 	"rb,wb"			},
/* 0x8f */	{	"caseb", 	"rb,rb,rb,bw-list"	},
	   
/* 0x90 */	{	"movb", 	"rb,wb"			},
/* 0x91 */	{	"cmpb", 	"rb,rb"			},
/* 0x92 */	{	"mcomb", 	"rb,wb"			},
/* 0x93 */	{	"bitb", 	"rb,rb"			},
/* 0x94 */	{	"clrb", 	"wb"			},
/* 0x95 */	{	"tstb", 	"rb"			},
/* 0x96 */	{	"incb", 	"mb"			},
/* 0x97 */	{	"decb", 	"mb"			},
/* 0x98 */	{	"cvtbl", 	"rb,wl"			},
/* 0x99 */	{	"cvtbw", 	"rb,ww"			},
/* 0x9a */	{	"movzbl", 	"rb,wl"			},
/* 0x9b */	{	"movzbw", 	"wb,ww"			},
/* 0x9c */	{	"rotl", 	"rb,rl,wl"		},
/* 0x9d */	{	"acbb", 	"rb,rb,mb,bw"		},
/* 0x9e */	{	"movab", 	"ab,wl"			},
/* 0x9f */	{	"pushab", 	"ab"			},
	   
/* 0xa0 */	{	"addw2", 	"rw,mw"			},
/* 0xa1 */	{	"addw3", 	"rw,rw,ww"		},
/* 0xa2 */	{	"subw2", 	"rw,mw"			},
/* 0xa3 */	{	"subw3", 	"rw,rw,ww"		},
/* 0xa4 */	{	"mulw2", 	"rw,mw"			},
/* 0xa5 */	{	"mulw3", 	"rw,rw,ww"		},
/* 0xa6 */	{	"divw2", 	"rw,mw"			},
/* 0xa7 */	{	"divw3", 	"rw,rw,ww"		},
/* 0xa8 */	{	"bisw2", 	"rw,mw"			},
/* 0xa9 */	{	"bisw3", 	"rw,rw,ww"		},
/* 0xaa */	{	"bicw2", 	"rw,mw"			},
/* 0xab */	{	"bicw3", 	"rw,rw,ww"		},
/* 0xac */	{	"xorw2", 	"rw,mw"			},
/* 0xad */	{	"xorw3", 	"rw,rw,ww"		},
/* 0xae */	{	"mnegw", 	"rw,ww"			},
/* 0xaf */	{	"casew", 	"rw,rw,rw,bw-list"	},
	   
/* 0xb0 */	{	"movw", 	"rw,ww"			},
/* 0xb1 */	{	"cmpw", 	"rw,rw"			},
/* 0xb2 */	{	"mcomw", 	"rw,ww"			},
/* 0xb3 */	{	"bitw", 	"rw,rw"			},
/* 0xb4 */	{	"clrw", 	"mw"			},
/* 0xb5 */	{	"tstw", 	"rw"			},
/* 0xb6 */	{	"incw", 	"mw"			},
/* 0xb7 */	{	"decw", 	"mw"			},
/* 0xb8 */	{	"bispsw", 	"rw"			},
/* 0xb9 */	{	"bicpsw", 	"rw"			},
/* 0xba */	{	"popr", 	"rw"			},
/* 0xbb */	{	"pushr", 	"rw"			},
/* 0xbc */	{	"chmk", 	"rw"			},
/* 0xbd */	{	"chme", 	"rw"			},
/* 0xbe */	{	"chms", 	"rw"			},
/* 0xbf */	{	"chmu", 	"rw"			},
	   
/* 0xc0 */	{	"addl2", 	"rl,ml"			},
/* 0xc1 */	{	"addl3", 	"rl,rl,wl"		},
/* 0xc2 */	{	"subl2", 	"rl,ml"			},
/* 0xc3 */	{	"subl3", 	"rl,rl,wl"		},
/* 0xc4 */	{	"mull2", 	"rl,ml"			},
/* 0xc5 */	{	"mull3", 	"rl,rl,wl"		},
/* 0xc6 */	{	"divl2", 	"rl,ml"			},
/* 0xc7 */	{	"divl3", 	"rl,rl,wl"		},
/* 0xc8 */	{	"bisl2", 	"rl,ml"			},
/* 0xc9 */	{	"bisl3", 	"rl,rl,wl"		},
/* 0xca */	{	"bicl2", 	"rl,ml"			},
/* 0xcb */	{	"bicl3", 	"rl,rl,wl"		},
/* 0xcc */	{	"xorl2", 	"rl,ml"			},
/* 0xcd */	{	"xorl3", 	"rl,rl,wl"		},
/* 0xce */	{	"mnegl", 	"rl,wl"			},
/* 0xcf */	{	"casel", 	"rl,rl,rl,bw-list"	},
	   
/* 0xd0 */	{	"movl", 	"rl,wl"			},
/* 0xd1 */	{	"cmpl", 	"rl,rl"			},
/* 0xd2 */	{	"mcoml", 	"rl,wl"			},
/* 0xd3 */	{	"bitl", 	"rl,rl"			},
/* 0xd4 */	{	"*clrl", 	"wl"			},
/* 0xd5 */	{	"tstl", 	"rl"			},
/* 0xd6 */	{	"incl", 	"ml"			},
/* 0xd7 */	{	"decl", 	"ml"			},
/* 0xd8 */	{	"adwc", 	"rl,ml"			},
/* 0xd9 */	{	"sbwc", 	"rl,ml"			},
/* 0xda */	{	"mtpr", 	"rl,rl"			},
/* 0xdb */	{	"mfpr", 	"rl,wl"			},
/* 0xdc */	{	"movpsl", 	"wl"			},
/* 0xdd */	{	"pushl", 	"rl"			},
/* 0xde */	{	"*moval", 	"al,wl"			},
/* 0xdf */	{	"*pushal", 	"al"			},
	   
/* 0xe0 */	{	"bbs", 		"rl,vb,bb"		},
/* 0xe1 */	{	"bbc", 		"rl,vb,bb"		},
/* 0xe2 */	{	"bbss", 	"rl,vb,bb"		},
/* 0xe3 */	{	"bbcs", 	"rl,vb,bb"		},
/* 0xe4 */	{	"bbsc", 	"rl,vb,bb"		},
/* 0xe5 */	{	"bbcc", 	"rl,vb,bb"		},
/* 0xe6 */	{	"bbssi", 	"rl,vb,bb"		},
/* 0xe7 */	{	"bbcci", 	"rl,vb,bb"		},
/* 0xe8 */	{	"blbs", 	"rl,bb"			},
/* 0xe9 */	{	"blbc", 	"rl,bb"			},
/* 0xea */	{	"ffs", 		"rl,rb,vb"		},
/* 0xeb */	{	"ffc", 		"rl,rb,vb"		},
/* 0xec */	{	"cmpv", 	"rl,rb,vb,rl"		},
/* 0xed */	{	"cmpzv", 	"rl,rb,vb,rl"		},
/* 0xee */	{	"extv", 	"rl,rb,vb,wl"		},
/* 0xef */	{	"extzv", 	"rl,rb,vb,wl"		},
	   
/* 0xf0 */	{	"insv", 	"rl,rl,rb,vb"		},
/* 0xf1 */	{	"acbl", 	"rl,rl,ml,bw"		},
/* 0xf2 */	{	"aoblss", 	"rl,ml,bb"		},
/* 0xf3 */	{	"aobleq", 	"rl,ml,bb"		},
/* 0xf4 */	{	"sobgeq", 	"ml,bb"			},
/* 0xf5 */	{	"sobgtr", 	"ml,bb"			},
/* 0xf6 */	{	"cvtlb", 	"rl,wb"			},
/* 0xf7 */	{	"cvtlw", 	"rl,ww"			},
/* 0xf8 */	{	"ashp", 	"rb,rw,ab,rb,rw,ab"	},
/* 0xf9 */	{	"cvtlp", 	"rl,rw,ab"		},
/* 0xfa */	{	"callg", 	"ab,ab"			},
/* 0xfb */	{	"calls", 	"rl,ab"			},
/* 0xfc */	{	"xfc", 		"?"			},
/* 0xfd */	{	"-reserved-", 	""			},
/* 0xfe */	{	"-reserved-", 	""			},
/* 0xff */	{	"-reserved-", 	""			},
  
};
