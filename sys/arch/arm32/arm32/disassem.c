/*	$NetBSD: disassem.c,v 1.6.10.1 1997/10/15 05:25:20 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Mark Brinicombe.
 * Copyright (c) 1996 Brini.
 *
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * db_disasm.c
 *
 * Kernel disassembler
 *
 * Created      : 10/02/96
 *
 * Structured after the sparc/sparc/db_disasm.c by David S. Miller &
 * Paul Kranenburg
 *
 * This code is not complete. Not all instructions are disassembled.
 * Current LDF, STF and CDT are not supported but these are low priority
 * as FP is not used in the kernel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <arm32/arm32/disassem.h>

/*
 * General instruction format
 *
 *	insn[cc][mod]	[operands]
 *
 * Those fields with an uppercase format code indicate that the field
 * follows directly after the instruction before the separator i.e.
 * they modify the instruction rather than just being an operand to
 * the instruction. The only exception is the writeback flag which
 * follows a operand.
 *
 *
 * 2 - print Operand 2 of a data processing instruction
 * d - destination register (bits 12-15)
 * n - n register (bits 16-19)
 * s - s register (bits 8-11)
 * o - indirect register rn (bits 16-19) (used by swap)
 * m - m register (bits 0-4)
 * a - address operand of ldr/str instruction
 * l - register list for ldm/stm instruction
 * f - 1st fp operand (register) (bits 12-14)
 * g - 2nd fp operand (register) (bits 16-18)
 * h - 3rd fp operand (register/immediate) (bits 0-4)
 * b - branch address
 * X - block transfer type
 * Y - block transfer type (r13 base)
 * c - comment field bits(0-23)
 * p - saved or current status register
 * F - PSR transfer fields
 * B - byte transfer flag
 * L - co-processor transfer size
 * S - set status flag
 * T - user mode transfer
 * P - fp precision
 * Q - fp precision (for ldf/stf)
 * R - fp rounding
 * v - co-processor data transfer registers + addressing mode
 * W - writeback flag
 * x - instruction in hex
 * # - co-processor number
 * y - co-processor data processing registers
 * z - co-processor register transfer registers
 */

struct arm32_insn {
	u_int mask;
	u_int pattern;
	char* name;
	char* format;
};

static struct arm32_insn arm32_i[] = {
    { 0x0fffffff, 0x0ff00000, "imb",	"c" },		/* Before swi */
    { 0x0fffffff, 0x0ff00001, "imbrange",	"c" },	/* Before swi */
    { 0x0f000000, 0x0f000000, "swi",	"c" },
    { 0x0f000000, 0x0a000000, "b",	"b" },
    { 0x0f000000, 0x0b000000, "bl",	"b" },
    { 0x0fe000f0, 0x00000090, "mul",	"Snms" },
    { 0x0fe000f0, 0x00200090, "mla",	"Snmsd" },
    { 0x0fe000f0, 0x00800090, "umull",	"Sdnms" },
    { 0x0fe000f0, 0x00c00090, "smull",	"Sdnms" },
    { 0x0fe000f0, 0x00a00090, "umlal",	"Sdnms" },
    { 0x0fe000f0, 0x00e00090, "smlal",	"Sdnms" },
    { 0x0e100000, 0x04000000, "str",	"BTdaW" },
    { 0x0e100000, 0x04100000, "ldr",	"BTdaW" },
    { 0x0c100010, 0x04000000, "str",	"BTdaW" },
    { 0x0c100010, 0x04100000, "ldr",	"BTdaW" },
    { 0x0e1f0000, 0x080d0000, "stm",	"YnWl" },	/* separate out r13 base */
    { 0x0e1f0000, 0x081d0000, "ldm",	"YnWl" },	/* separate out r13 base */    
    { 0x0e100000, 0x08000000, "stm",	"XnWl" },
    { 0x0e100000, 0x08100000, "ldm",	"XnWl" },    
    { 0x0e500ff0, 0x001000b0, "ldrh",	"daW" },
    { 0x0e500ff0, 0x000000b0, "strh",	"daW" },
    { 0x0e500ff0, 0x001000d0, "ldrsb",	"daW" },
    { 0x0e500ff0, 0x001000f0, "ldrsh",	"daW" },
    { 0x0f200090, 0x00200090, "und",	"x" },	/* Before data processing */
    { 0x0e1000d0, 0x000000d0, "und",	"x" },	/* Before data processing */
    { 0x0fb00ff0, 0x01000090, "swap",	"Bdmo" },
    { 0x0fbf0fff, 0x010f0000, "mrs",	"dp" },		/* Before data processing */
    { 0x0fb0fff0, 0x0120f000, "msr",	"pFm" },	/* Before data processing */
    { 0x0fb0f000, 0x0320f000, "msr",	"pF2" },	/* Before data processing */
    { 0x0de00000, 0x00000000, "and",	"Sdn2" },
    { 0x0de00000, 0x00200000, "eor",	"Sdn2" },
    { 0x0de00000, 0x00400000, "sub",	"Sdn2" },
    { 0x0de00000, 0x00600000, "rsb",	"Sdn2" },
    { 0x0de00000, 0x00800000, "add",	"Sdn2" },
    { 0x0de00000, 0x00a00000, "adc",	"Sdn2" },
    { 0x0de00000, 0x00c00000, "sbc",	"Sdn2" },
    { 0x0de00000, 0x00e00000, "rsc",	"Sdn2" },
    { 0x0df00000, 0x01100000, "tst",	"Sn2" },
    { 0x0df00000, 0x01300000, "teq",	"Sn2" },
    { 0x0de00000, 0x01400000, "cmp",	"Sn2" },
    { 0x0de00000, 0x01600000, "cmn",	"Sn2" },
    { 0x0de00000, 0x01800000, "orr",	"Sdn2" },
    { 0x0de00000, 0x01a00000, "mov",	"Sd2" },
    { 0x0de00000, 0x01c00000, "bic",	"Sdn2" },
    { 0x0de00000, 0x01e00000, "mvn",	"Sd2" },
    { 0x0ff08f10, 0x0e000100, "adf",	"PRfgh" },
    { 0x0ff08f10, 0x0e100100, "muf",	"PRfgh" },
    { 0x0ff08f10, 0x0e200100, "suf",	"PRfgh" },
    { 0x0ff08f10, 0x0e300100, "rsf",	"PRfgh" },
    { 0x0ff08f10, 0x0e400100, "dvf",	"PRfgh" },
    { 0x0ff08f10, 0x0e500100, "rdf",	"PRfgh" },
    { 0x0ff08f10, 0x0e600100, "pow",	"PRfgh" },
    { 0x0ff08f10, 0x0e700100, "rpw",	"PRfgh" },
    { 0x0ff08f10, 0x0e800100, "rmf",	"PRfgh" },
    { 0x0ff08f10, 0x0e900100, "fml",	"PRfgh" },
    { 0x0ff08f10, 0x0ea00100, "fdv",	"PRfgh" },
    { 0x0ff08f10, 0x0eb00100, "frd",	"PRfgh" },
    { 0x0ff08f10, 0x0ec00100, "pol",	"PRfgh" },
    { 0x0f008f10, 0x0e000100, "fpbop",	"PRfgh" },
    { 0x0ff08f10, 0x0e008100, "mvf",	"PRfh" },
    { 0x0ff08f10, 0x0e108100, "mnf",	"PRfh" },
    { 0x0ff08f10, 0x0e208100, "abs",	"PRfh" },
    { 0x0ff08f10, 0x0e308100, "rnd",	"PRfh" },
    { 0x0ff08f10, 0x0e408100, "sqt",	"PRfh" },
    { 0x0ff08f10, 0x0e508100, "log",	"PRfh" },
    { 0x0ff08f10, 0x0e608100, "lgn",	"PRfh" },
    { 0x0ff08f10, 0x0e708100, "exp",	"PRfh" },
    { 0x0ff08f10, 0x0e808100, "sin",	"PRfh" },
    { 0x0ff08f10, 0x0e908100, "cos",	"PRfh" },
    { 0x0ff08f10, 0x0ea08100, "tan",	"PRfh" },
    { 0x0ff08f10, 0x0eb08100, "asn",	"PRfh" },
    { 0x0ff08f10, 0x0ec08100, "acs",	"PRfh" },
    { 0x0ff08f10, 0x0ed08100, "atn",	"PRfh" },
    { 0x0f008f10, 0x0e008100, "fpuop",	"PRfh" },
    { 0x0e100f00, 0x0c000100, "stf",	"QLv" },
    { 0x0e100f00, 0x0c100100, "ldf",	"QLv" },
    { 0x0ff00f10, 0x0e000110, "flt",	"PRgd" },
    { 0x0ff00f10, 0x0e100110, "fix",	"PRdh" },
    { 0x0ff00f10, 0x0e200110, "wfs",	"d" },
    { 0x0ff00f10, 0x0e300110, "rfs",	"d" },
    { 0x0ff00f10, 0x0e400110, "wfc",	"d" },
    { 0x0ff00f10, 0x0e500110, "rfc",	"d" },
    { 0x0ff0ff10, 0x0e90f110, "cmf",	"PRgh" },
    { 0x0ff0ff10, 0x0eb0f110, "cnf",	"PRgh" },
    { 0x0ff0ff10, 0x0ed0f110, "cmfe",	"PRgh" },
    { 0x0ff0ff10, 0x0ef0f110, "cnfe",	"PRgh" },
    { 0x0f100010, 0x0e000010, "mcr",	"#z" },
    { 0x0f100010, 0x0e100010, "mrc",	"#z" },
    { 0x0f000010, 0x0e000000, "cdp",	"#y" },
    { 0x0e100090, 0x0c100000, "ldc",	"L#v" },
    { 0x0e100090, 0x0c000000, "stc",	"L#v" },
    { 0x00000000, 0x00000000, NULL,	NULL }
};

static char *arm32_insn_conditions[] = {
	"eq", "ne", "cs", "cc",
	"mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt",
	"gt", "le", "",   "nv"
};

static char *insn_block_transfers[] = {
	"da", "ia", "db", "ib"
};

static char *insn_stack_block_transfers[] = {
	"fa", "ea", "fd", "fa"
};

static char *op_shifts[] = {
	"lsl", "lsr", "asr", "ror"
};

static char *insn_fpa_rounding[] = {
	"", "p", "m", "z"
};

static char *insn_fpa_precision[] = {
	"s", "d", "e", "p"
};

static char *insn_fpaconstants[] = {
	"0.0", "1.0", "2.0", "3.0",
	"4.0", "5.0", "0.5", "10.0"
};

#define insn_condition(x)	arm32_insn_conditions[(x >> 28) & 0x0f]
#define insn_blktrans(x)	insn_block_transfers[(x >> 23) & 3]
#define insn_stkblktrans(x)	insn_stack_block_transfers[(x >> 23) & 3]
#define op2_shift(x)		op_shifts[(x >> 5) & 3]
#define insn_fparnd(x)		insn_fpa_rounding[(x >> 5) & 0x03]
#define insn_fpaprec(x)		insn_fpa_precision[(((x >> 18) & 2)|(x >> 7)) & 1]
#define insn_fpaprect(x)	insn_fpa_precision[(((x >> 21) & 2)|(x >> 15)) & 1]
#define insn_fpaimm(x)		insn_fpaconstants[x & 0x07]

/* Local prototypes */
static void disasm_register_shift __P((disasm_interface_t *di, u_int insn));
static void disasm_print_reglist  __P((disasm_interface_t *di, u_int insn));
static void disasm_insn_ldrstr    __P((disasm_interface_t *di, u_int insn, u_int loc));
static void disasm_insn_cdt	  __P((disasm_interface_t *di, u_int insn, u_int loc));


vm_offset_t
disasm(di, loc, altfmt)
	disasm_interface_t *di;
	vm_offset_t loc;
	int altfmt;
{
	struct arm32_insn *i_ptr = (struct arm32_insn *)&arm32_i;

	u_int insn;
	int matchp;
	int branch;
	char* f_ptr;
	int fmt;

	fmt = 0;
	matchp = 0;
	insn = di->di_readword(loc);

/*	di->di_printf("loc=%08x insn=%08x : ", loc, insn);*/

	while (i_ptr->name) {
		if ((insn & i_ptr->mask) ==  i_ptr->pattern) {
			matchp = 1;
			break;
		}
		i_ptr++;
	}

	if (!matchp) {
		di->di_printf("und%s\t%08x\n", insn_condition(insn), insn);
		return(loc + INSN_SIZE);
	}

	di->di_printf("%s%s", i_ptr->name, insn_condition(insn));

	f_ptr = i_ptr->format;

	/* Insert tab if there are no instruction modifiers */

	if (*(f_ptr) < 'A' || *(f_ptr) > 'Z') {
		++fmt;
		di->di_printf("\t");
	}

	while (*f_ptr) {
		switch (*f_ptr) {
		/* 2 - print Operand 2 of a data processing instruction */
		case '2':
			if (insn & 0x02000000) {
				int shift = ((insn >> 7) & 0x1e);

				if (shift == 0)
					di->di_printf("#0x%08x",
					    (insn & 0xff));
				else
					di->di_printf("#0x%08x",
					    ((insn & 0xff) << (32 - shift)));
			} else {  
				disasm_register_shift(di, insn);
			}
			break;
		/* d - destination register (bits 12-15) */
		case 'd':
			di->di_printf("r%d", ((insn >> 12) & 0x0f));
			break;
		/* n - n register (bits 16-19) */
		case 'n':
			di->di_printf("r%d", ((insn >> 16) & 0x0f));
			break;
		/* s - s register (bits 8-11) */
		case 's':
			di->di_printf("r%d", ((insn >> 8) & 0x0f));
			break;
		/* o - indirect register rn (bits 16-19) (used by swap) */
		case 'o':
			di->di_printf("[r%d]", ((insn >> 16) & 0x0f));
			break;
		/* m - m register (bits 0-4) */
		case 'm':
			di->di_printf("r%d", ((insn >> 0) & 0x0f));
			break;
		/* a - address operand of ldr/str instruction */
		case 'a':
			disasm_insn_ldrstr(di, insn, loc);
			break;
		/* l - register list for ldm/stm instruction */
		case 'l':
			disasm_print_reglist(di, insn);
			break;
		/* f - 1st fp operand (register) (bits 12-14) */
		case 'f':
			di->di_printf("f%d", (insn >> 12) & 7);
			break;
		/* g - 2nd fp operand (register) (bits 16-18) */
		case 'g':
			di->di_printf("f%d", (insn >> 16) & 7);
			break;
		/* h - 3rd fp operand (register/immediate) (bits 0-4) */
		case 'h':
			if (insn & (1 << 3))
				di->di_printf("#%s", insn_fpaimm(insn));
			else
				di->di_printf("f%d", insn & 7);
			break;
		/* b - branch address */
		case 'b':
			branch = ((insn << 2) & 0x03ffffff);
			if (branch & 0x02000000)
				branch |= 0xfc000000;
			di->di_printaddr(loc + 8 + branch);
			break;
		/* X - block transfer type */
		case 'X':
			di->di_printf("%s", insn_blktrans(insn));
			break;
		/* Y - block transfer type (r13 base) */
		case 'Y':
			di->di_printf("%s", insn_stkblktrans(insn));
			break;
		/* c - comment field bits(0-23) */
		case 'c':
			di->di_printf("0x%08x", (insn & 0x00ffffff));
			break;
		/* p - saved or current status register */
		case 'p':
			if (insn & 0x00400000)
				di->di_printf("spsr");
			else
				di->di_printf("cpsr");
			break;
		/* F - PSR transfer fields */
		case 'F':
			di->di_printf("_");
			if (insn & (1 << 16))
				di->di_printf("c");
			if (insn & (1 << 17))
				di->di_printf("x");
			if (insn & (1 << 18))
				di->di_printf("s");
			if (insn & (1 << 19))
				di->di_printf("f");
			break;
		/* B - byte transfer flag */
		case 'B':
			if (insn & 0x00400000)
				di->di_printf("b");
			break;
		/* L - co-processor transfer size */
		case 'L':
			if (insn & (1 << 22))
				di->di_printf("l");
			break;
		/* S - set status flag */
		case 'S':
			if (insn & 0x00100000)
				di->di_printf("s");
			break;
		/* T - user mode transfer */
		case 'T':
			if ((insn & 0x01200000) == 0x00200000)
				di->di_printf("t");
			break;
		case 'P':
		/* P - fp precision */
			di->di_printf("%s", insn_fpaprec(insn));
			break;
		/* Q - fp precision (for ldf/stf) */
		case 'Q':
			break;
		/* R - fp rounding */
		case 'R':
			di->di_printf("%s", insn_fparnd(insn));
			break;
		/* W - writeback flag */
		case 'W':
			if (insn & (1 << 21))
				di->di_printf("!");
			break;
		/* # - co-processor number */
		case '#':
			di->di_printf("p%d", (insn >> 8) & 0x0f);
			break;
		/* v - co-processor data transfer registers+addressing mode */
		case 'v':
			break;
		/* x - instruction in hex */
		case 'x':
			db_printf("0x%08x", insn);
			break;
		/* y - co-processor data processing registers */
		case 'y':
			di->di_printf("%d, ", (insn >> 20) & 0x0f);

			di->di_printf("c%d, c%d, c%d", (insn >> 12) & 0x0f,
			    (insn >> 16) & 0x0f, insn & 0x0f);

			di->di_printf(", %d", (insn >> 5) & 0x07);
			break;
		/* z - co-processor register transfer registers */
		case 'z':
			di->di_printf("%d, ", (insn >> 21) & 0x07);
			di->di_printf("r%d, c%d, c%d, %d",
			    (insn >> 12) & 0x0f, (insn >> 16) & 0x0f,
			    insn & 0x0f, (insn >> 5) & 0x07);

/*			if (((insn >> 5) & 0x07) != 0)
				di->di_printf(", %d", (insn >> 5) & 0x07);*/
			break;
		default:
			di->di_printf("[%c - unknown]", *f_ptr);
			break;
		}
		if (*(f_ptr+1) >= 'A' && *(f_ptr+1) <= 'Z')
			++f_ptr;
		else if (*(++f_ptr)) {
			++fmt;
			if (fmt == 1)
				di->di_printf("\t");
			else
				di->di_printf(", ");
		}
	};

	di->di_printf("\n");

	return(loc + INSN_SIZE);
}


static void
disasm_register_shift(di, insn)
	disasm_interface_t *di;
	u_int insn;
{
	di->di_printf("r%d", (insn & 0x0f));
	if ((insn & 0x00000ff0) == 0)
		;
	else if ((insn & 0x00000ff0) == 0x00000060)
		di->di_printf(", rrx");
	else {
		if (insn & 0x10)
			di->di_printf(", %s r%d", op2_shift(insn),
			    (insn >> 8) & 0x0f);
		else
			di->di_printf(", %s #%d", op2_shift(insn),
			    (insn >> 7) & 0x1f);
	}
}


static void
disasm_print_reglist(di, insn)
	disasm_interface_t *di;
	u_int insn;
{
	int loop;
	int start;
	int comma;

	di->di_printf("{");
	start = -1;
	comma = 0;

	for (loop = 0; loop < 17; ++loop) {
		if (start != -1) {
			if (loop == 16 || !(insn & (1 << loop))) {
				if (comma)
					di->di_printf(", ");
				else
					comma = 1;
        			if (start == loop - 1)
        				di->di_printf("r%d", start);
        			else
        				di->di_printf("r%d-r%d", start, loop - 1);
        			start = -1;
        		}
        	} else {
        		if (insn & (1 << loop))
        			start = loop;
        	}
        }
	di->di_printf("}");

	if (insn & (1 << 22))
		di->di_printf("^");
}

static void
disasm_insn_ldrstr(di, insn, loc)
	disasm_interface_t *di;
	u_int insn;
	u_int loc;
{
	if ((((insn >> 16) & 0x0f) == 15) && ((insn & (1 << 21)) == 0)
	    && ((insn & (1 << 24)) != 0) && ((insn & (1 << 25)) == 0)) {
		if (insn & 0x00800000)
			loc += (insn & 0xfff);
		else
			loc -= (insn & 0xfff);
		di->di_printaddr(loc + 8);
 	} else {
		di->di_printf("[r%d", (insn >> 16) & 0x0f);
		di->di_printf("%s, ", (insn & (1 << 24)) ? "" : "]");

		if (!(insn & 0x00800000))
			di->di_printf("-");
		if (insn & (1 << 25))
			disasm_register_shift(di, insn);
		else
			di->di_printf("#0x%04x", insn & 0xfff);
		if (insn & (1 << 24))
			di->di_printf("]");
	}
}


static void
disasm_insn_ldcstc(di, insn, loc)
	disasm_interface_t *di;
	u_int insn;
	u_int loc;
{
	if (((insn >> 8) & 0xf) == 1)
		di->di_printf("f%d, ", (insn >> 12) & 0x07);
	else
		di->di_printf("c%d, ", (insn >> 12) & 0x0f);

	di->di_printf("[r%d", (insn >> 16) & 0x0f);

	di->di_printf("%s, ", (insn & (1 << 24)) ? "" : "]");

	if (!(insn & (1 << 23)))
		di->di_printf("-");

	di->di_printf("#0x%03x", (insn & 0xff) << 2);

	if (insn & (1 << 24))
		di->di_printf("]");

	if (insn & (1 << 21))
		di->di_printf("!");
}

static u_int
disassemble_readword(address)
	u_int address;
{
	return(*((u_int *)address));
}

static void
disassemble_printaddr(address)
	u_int address;
{
	printf("0x%08x", address);
}

void
disassemble(address)
	u_int address;
{
	disasm_interface_t di;

	di.di_readword = disassemble_readword;
	di.di_printaddr = disassemble_printaddr;
	di.di_printf = printf;

	(void)disasm(&di, address, 0);
}

/* End of disassem.c */
