/*	$NetBSD: emul.c,v 1.8.8.3 2002/01/04 19:12:27 eeh Exp $	*/

/*-
 * Copyright (c) 1997, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <machine/reg.h>
#include <machine/instr.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <sparc64/sparc64/cpuvar.h>

#define DEBUG_EMUL
#ifdef DEBUG_EMUL
# define DPRINTF(a) uprintf a
#else
# define DPRINTF(a)
#endif

#define GPR(tf, i)	((int32_t *)(u_long)&tf->tf_global)[i]
#define IPR(tf, i)	((int32_t *)(u_long)tf->tf_out[6])[i - 16]
#define FPR(l, i)	((int32_t) l->l_md.md_fpstate->fs_regs[i])

static __inline int readgpreg __P((struct trapframe64 *, int, void *));
static __inline int readfpreg __P((struct lwp *, int, void *));
static __inline int writegpreg __P((struct trapframe64 *, int, const void *));
static __inline int writefpreg __P((struct lwp *, int, const void *));
static __inline int decodeaddr __P((struct trapframe64 *, union instr *, void *));
static int muldiv __P((struct trapframe64 *, union instr *, int32_t *, int32_t *,
    int32_t *));

#define	REGNAME(i)	"goli"[i >> 3], i & 7


static __inline int
readgpreg(tf, i, val)
	struct trapframe64 *tf;
	int i;
	void *val;
{
	int error = 0;
	if (i == 0)
		*(int32_t *) val = 0;
	else if (i < 16)
		*(int32_t *) val = GPR(tf, i);
	else
		error = copyin(&IPR(tf, i), val, sizeof(int32_t));

	return error;
}

		
static __inline int
writegpreg(tf, i, val)
	struct trapframe64 *tf;
	int i;
	const void *val;
{
	int error = 0;

	if (i == 0)
		return error;
	else if (i < 16)
		GPR(tf, i) = *(int32_t *) val;
	else
		/* XXX: Fix copyout prototype */
		error = copyout((caddr_t) val, &IPR(tf, i), sizeof(int32_t));

	return error;
}
	

static __inline int
readfpreg(l, i, val)
	struct lwp *l;
	int i;
	void *val;
{
	*(int32_t *) val = FPR(l, i);
	return 0;
}

		
static __inline int
writefpreg(l, i, val)
	struct lwp *l;
	int i;
	const void *val;
{
	FPR(l, i) = *(const int32_t *) val;
	return 0;
}

static __inline int
decodeaddr(tf, code, val)
	struct trapframe64 *tf;
	union instr *code;
	void *val;
{
	if (code->i_simm13.i_i)
		*((int32_t *) val) = code->i_simm13.i_simm13;
	else {
		int error;

		if (code->i_asi.i_asi)
			return EINVAL;
		if ((error = readgpreg(tf, code->i_asi.i_rs2, val)) != 0)
			return error;
	}
	return 0;
}


static int
muldiv(tf, code, rd, rs1, rs2)
	struct trapframe64 *tf;
	union instr *code;
	int32_t *rd, *rs1, *rs2;
{
	/*
	 * We check for {S,U}{MUL,DIV}{,cc}
	 *
	 * [c = condition code, s = sign]
	 * Mul = 0c101s
	 * Div = 0c111s
	 */
	union {
		struct {
			unsigned unused:26;	/* padding */
			unsigned zero:1;	/* zero by opcode */
			unsigned cc:1;		/* one to send condition code */
			unsigned one1:1;	/* one by opcode */
			unsigned div:1;		/* one if divide */
			unsigned one2:1;	/* one by opcode */
			unsigned sgn:1;		/* sign bit */
		} bits;
		int num;
	} op;

	op.num = code->i_op3.i_op3;

#ifdef DEBUG_EMUL
	uprintf("muldiv 0x%x: %c%s%s %c%d, %c%d, ", code->i_int,
	    "us"[op.bits.sgn], op.bits.div ? "div" : "mul",
	    op.bits.cc ? "cc" : "", REGNAME(code->i_op3.i_rd),
	    REGNAME(code->i_op3.i_rs1));
	if (code->i_loadstore.i_i)
		uprintf("0x%x\n", *rs2);
	else
		uprintf("%c%d\n", REGNAME(code->i_asi.i_rs2));
#endif

	if (op.bits.div) {
		if (*rs2 == 0) {
			/*
			 * XXX: to be 100% correct here, on sunos we need to
			 *	ignore the error and return *rd = *rs1.
			 *	It should be easy to fix by passing struct
			 *	proc in here.
			 */
			DPRINTF(("emulinstr: avoid zerodivide\n"));
			return EINVAL;
		}
		*rd = *rs1 / *rs2;
		DPRINTF(("muldiv: %d / %d = %d\n", *rs1, *rs2, *rd));
	}
	else {
		*rd = *rs1 * *rs2;
		DPRINTF(("muldiv: %d * %d = %d\n", *rs1, *rs2, *rd));
	}

	if (op.bits.cc) {
		/* Set condition codes */
		tf->tf_tstate &= ~(TSTATE_CCR);

		if (*rd == 0)
			tf->tf_tstate |= (u_int64_t)(ICC_Z|XCC_Z) << TSTATE_CCR_SHIFT;
		else {
			if (op.bits.sgn && *rd < 0)
				tf->tf_tstate |= (u_int64_t)(ICC_N|XCC_N) << TSTATE_CCR_SHIFT;
			if (op.bits.div) {
				if (*rd * *rs2 != *rs1)
					tf->tf_tstate |= (u_int64_t)(ICC_V|XCC_V) << TSTATE_CCR_SHIFT;
			}
			else {
				if (*rd / *rs2 != *rs1)
					tf->tf_tstate |= (u_int64_t)(ICC_V|XCC_V) << TSTATE_CCR_SHIFT;
			}
		}
	}

	return 0;
}

/*
 * Code to handle alignment faults on the sparc. This is enabled by sending
 * a fixalign trap. Such code is generated by compiling with cc -misalign
 * on SunOS, but we don't have such a feature yet on our gcc.
 */

int
fixalign(l, tf)
	struct lwp *l;
	struct trapframe64 *tf;
{
	static u_char sizedef[] = { 0x4, 0xff, 0x2, 0x8 };

	/*
	 * This is particular to load and store instructions
	 */
	union {
		struct {
			unsigned unused:26;	/* 26 padding */
			unsigned fl:1;		/* 1 bit float flag */
			unsigned op:1;		/* 1 bit opcode */
			unsigned sgn:1;		/* 1 bit sign */
			unsigned st:1;		/* 1 bit load/store */
			unsigned sz:2;		/* 2 bit size register */
		} bits;
		int num;
	} op;

	union {
		double	d;
		int32_t i[2];
		int16_t s[4];
		int8_t  c[8];
	} data;

	union instr code;
	size_t size;
	int64_t rs1, rs2;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t)(u_long)tf->tf_pc, &code.i_int, sizeof(code.i_int));
	if (error != 0) {
		DPRINTF(("fixalign: Bad instruction fetch\n"));
		return EINVAL;
	}

	/* Only support format 3 */
	if (code.i_any.i_op != 3) {
		DPRINTF(("fixalign: Not a load or store\n"));
		return EINVAL;
	}

	op.num = code.i_loadstore.i_op3;

	/* Check operand size */
	if ((size = sizedef[op.bits.sz]) == 0xff) {
		DPRINTF(("fixalign: Bad operand size\n"));
		return EINVAL;
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.i_op3.i_rs1, &rs1)) != 0) {
		DPRINTF(("emulinstr: read rs1 %d\n", error));
		return error;
	}

	if ((error = decodeaddr(tf, &code, &rs2)) != 0) {
		DPRINTF(("emulinstr: decode addr %d\n", error));
		return error;
	}


	rs1 += rs2;

#ifdef DEBUG_EMUL
	uprintf("memalign 0x%x: %s%c%c %c%d, %c%d, ", code.i_int,
	    op.bits.st ? "st" : "ld", "us"[op.bits.sgn],
	    "w*hd"[op.bits.sz], op.bits.fl ? 'f' : REGNAME(code.i_op3.i_rd),
	    REGNAME(code.i_op3.i_rs1));
	if (code.i_loadstore.i_i)
		uprintf("0x%llx\n", (unsigned long long)rs2);
	else
		uprintf("%c%d\n", REGNAME(code.i_asi.i_rs2));
#endif
#ifdef DIAGNOSTIC
	if (op.bits.fl && l != fplwp)
		panic("fp align without being the FP owning process");
#endif

	if (op.bits.st) {
		if (op.bits.fl) {
			if (l == fplwp) {
				savefpstate(l->l_md.md_fpstate);
				fplwp = NULL;
			}

			error = readfpreg(l, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readfpreg(l, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}
		else {
			error = readgpreg(tf, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readgpreg(tf, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}

		if (size == 2)
			return copyout(&data.s[1], (caddr_t)(u_long)rs1, size);
		else
			return copyout(&data.d, (caddr_t)(u_long)rs1, size);
	}
	else { /* load */
		if (size == 2) {
			error = copyin((caddr_t)(u_long)rs1, &data.s[1], size);
			if (error)
				return error;

			/* Sign extend if necessary */
			if (op.bits.sgn && (data.s[1] & 0x8000) != 0)
				data.s[0] = ~0;
			else
				data.s[0] = 0;
		}
		else
			error = copyin((caddr_t)(u_long)rs1, &data.d, size);

		if (error)
			return error;

		if (op.bits.fl) {
			error = writefpreg(l, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = writefpreg(l, code.i_op3.i_rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
			loadfpstate(l->l_md.md_fpstate);
			fplwp = l;
		}
		else {
			error = writegpreg(tf, code.i_op3.i_rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8)
				error = writegpreg(tf, code.i_op3.i_rd + 1,
				    &data.i[1]);
		}
	}
	return error;
}

/*
 * Emulate unimplemented instructions on earlier sparc chips.
 */
int
emulinstr(pc, tf)
	vaddr_t pc;
	struct trapframe64 *tf;
{
	union instr code;
	int32_t rs1, rs2, rd;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t) pc, &code.i_int, sizeof(code.i_int));
	if (error != 0) {
		DPRINTF(("emulinstr: Bad instruction fetch\n"));
		return SIGILL;
	}

	/* Only support format 2 */
	if (code.i_any.i_op != 2) {
		DPRINTF(("emulinstr: Not a format 2 instruction\n"));
		return SIGILL;
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.i_op3.i_rs1, &rs1)) != 0) {
		DPRINTF(("emulinstr: read rs1 %d\n", error));
		return SIGILL;
	}

	if ((error = decodeaddr(tf, &code, &rs2)) != 0) {
		DPRINTF(("emulinstr: decode addr %d\n", error));
		return SIGILL;
	}

	switch (code.i_op3.i_op3) {
	case IOP3_FLUSH:
/*		cpuinfo.cache_flush((caddr_t)(rs1 + rs2), 4); XXX */
		return 0;

	default:
		if ((code.i_op3.i_op3 & 0x2a) != 0xa) {
			DPRINTF(("emulinstr: Unsupported op3 0x%x\n",
			    code.i_op3.i_op3));
			return SIGILL;
		}
		else if ((error = muldiv(tf, &code, &rd, &rs1, &rs2)) != 0)
			return SIGFPE;
	}

	if ((error = writegpreg(tf, code.i_op3.i_rd, &rd)) != 0) {
		DPRINTF(("muldiv: write rd %d\n", error));
		return SIGILL;
	}

	return 0;
}
