/*	$NetBSD: math_emulate.c,v 1.25.2.1 2004/08/03 10:35:50 skrll Exp $	*/

/*
 * expediant "port" of linux 8087 emulator to 386BSD, with apologies -wfj
 */

/*
 * linux/kernel/math/math_emulate.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * Limited emulation 27.12.91 - mostly loads/stores, which gcc wants
 * even for soft-float, unless you use bruce evans' patches. The patches
 * are great, but they have to be re-applied for every version, and the
 * library is different for soft-float and 80387. So emulation is more
 * practical, even though it's slower.
 *
 * 28.12.91 - loads/stores work, even BCD. I'll have to start thinking
 * about add/sub/mul/div. Urgel. I should find some good source, but I'll
 * just fake up something.
 *
 * 30.12.91 - add/sub/mul/div/com seem to work mostly. I should really
 * test every possible combination.
 */

/*
 * This file is full of ugly macros etc: one problem was that gcc simply
 * didn't want to make the structures as they should be: it has to try to
 * align them. Sickening code, but at least I've hidden the ugly things
 * in this one file: the other files don't need to know about these things.
 *
 * The other files also don't care about ST(x) etc - they just get addresses
 * to 80-bit temporary reals, and do with them as they please. I wanted to
 * hide most of the 387-specific things here.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: math_emulate.c,v 1.25.2.1 2004/08/03 10:35:50 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <machine/cpu.h>
#include <machine/reg.h>

#define __ALIGNED_TEMP_REAL 1
#include <i386/i386/math_emu.h>

#define ST(x) (*__st((x)))
#define PST(x) ((const temp_real *) __st((x)))
#define	math_abort(tfp, ksi, signo, code) 	\
    do {					\
	    KSI_INIT_TRAP(ksi);			\
	    ksi->ksi_signo = signo;		\
	    ksi->ksi_code = code;		\
	    ksi->ksi_addr = (void *)info->tf_eip;\
	    tfp->tf_eip = oldeip;		\
	    return -1;				\
    } while (/*CONSTCOND*/0)

/*
 * We don't want these inlined - it gets too messy in the machine-code.
 */
static void fpop(void);
static void fpush(void);
static void fxchg(temp_real_unaligned * a, temp_real_unaligned * b);
static temp_real_unaligned * __st(int i);

#define	fninit()	do { \
	I387.cwd = __INITIAL_NPXCW__;	\
	I387.swd = 0x0000;		\
	I387.twd = 0x0000;		\
} while (0)

int
math_emulate(info, ksi)
	struct trapframe *info;
	ksiginfo_t *ksi;
{
	struct lwp *l = curlwp;
	u_short cw, code;
	temp_real tmp;
	char * address;
	u_long oldeip;
	int override_seg, override_addrsize, override_datasize;
	int prefix;

	override_seg = override_addrsize = override_datasize = 0;

	if (!USERMODE(info->tf_cs, info->tf_eflags))
		panic("math emulator called from supervisor mode");

	/* ever used fp? */
	if ((l->l_md.md_flags & MDL_USEDFPU) == 0) {
		if (i386_use_fxsave)
			cw = l->l_addr->u_pcb.pcb_savefpu.sv_xmm.sv_env.en_cw;
		else
			cw = l->l_addr->u_pcb.pcb_savefpu.sv_87.sv_env.en_cw;
		fninit();
		I387.cwd = cw;
		l->l_md.md_flags |= MDL_USEDFPU;
	}

	if (I387.cwd & I387.swd & 0x3f)
		I387.swd |= 0x8000;
	else
		I387.swd &= 0x7fff;

	I387.fip = oldeip = info->tf_eip;

	/*
	 * Scan for instruction prefixes. More to be politically correct
	 * than anything else. Prefixes aren't useful for the instructions
	 * we can emulate anyway.
	 */
	while (1) {
		prefix = fubyte((const void *)info->tf_eip);
		switch (prefix) {
		case INSPREF_LOCK:
			math_abort(info, ksi, SIGILL, ILL_ILLOPC);
		case INSPREF_REPN:
		case INSPREF_REPE:
			break;
		case INSPREF_CS:
		case INSPREF_SS:
		case INSPREF_DS:
		case INSPREF_ES:
		case INSPREF_FS:
		case INSPREF_GS:
			override_seg = prefix;
			break;
		case INSPREF_OSIZE:
			override_datasize = prefix;	
			break;
		case INSPREF_ASIZE:
			override_addrsize = prefix;
			break;
		case -1:
			math_abort(info, ksi, SIGSEGV, SEGV_ACCERR);
		default:
			goto done;
		}
		info->tf_eip++;
	}

done:
	code = htons(fusword((u_short *) info->tf_eip)) & 0x7ff;
	info->tf_eip += 2;
	*((u_short *) &I387.fcs) = (u_short) info->tf_cs;
	*((u_short *) &I387.fcs + 1) = code;

	switch (code) {
	case 0x1d0: /* fnop */
		return(0);
	case 0x1e0: /* fchs */
		ST(0).exponent ^= 0x8000;
		return(0);
	case 0x1e1: /* fabs */
		ST(0).exponent &= 0x7fff;
		return(0);
	case 0x1e4: /* fxtract XXX */
		ftst(PST(0));
		return(0);
	case 0x1e8: /* fld1 */
		fpush();
		ST(0) = CONST1;
		return(0);
	case 0x1e9: /* fld2t */
		fpush();
		ST(0) = CONSTL2T;
		return(0);
	case 0x1ea: /* fld2e */
		fpush();
		ST(0) = CONSTL2E;
		return(0);
	case 0x1eb: /* fldpi */
		fpush();
		ST(0) = CONSTPI;
		return(0);
	case 0x1ec: /* fldlg2 */
		fpush();
		ST(0) = CONSTLG2;
		return(0);
	case 0x1ed: /* fldln2 */
		fpush();
		ST(0) = CONSTLN2;
		return(0);
	case 0x1ee: /* fldz */
		fpush();
		ST(0) = CONSTZ;
		return(0);
	case 0x1fc: /* frndint */
		frndint(PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x1fd: /* fscale */
		/* incomplete and totally inadequate -wfj */
		Fscale(PST(0), PST(1), &tmp);
		real_to_real(&tmp,&ST(0));
		return(0);			/* 19 Sep 92*/
	case 0x2e9: /* XXX */
		fucom(PST(1),PST(0));
		fpop(); fpop();
		return(0);
	case 0x3d0: case 0x3d1: /* XXX */
		return(0);
	case 0x3e2: /* fclex */
		I387.swd &= 0x7f00;
		return(0);
	case 0x3e3: /* fninit */
		fninit();
		return(0);
	case 0x3e4: /* XXX */
		return(0);
	case 0x6d9: /* fcompp */
		fcom(PST(1),PST(0));
		fpop(); fpop();
		return(0);
	case 0x7e0: /* fstsw ax */
		*(u_short *) &info->tf_eax = I387.swd;
		return(0);
	case 0x1d1: case 0x1d2: case 0x1d3:
	case 0x1d4: case 0x1d5: case 0x1d6: case 0x1d7:
	case 0x1e2: case 0x1e3:
	case 0x1e6: case 0x1e7:
	case 0x1ef:
		math_abort(info, ksi, SIGILL, ILL_ILLOPC);
	case 0x1e5:
		uprintf("math_emulate: fxam not implemented\n\r");
		math_abort(info, ksi, SIGILL, ILL_ILLOPC);
	case 0x1f0: case 0x1f1: case 0x1f2: case 0x1f3:
	case 0x1f4: case 0x1f5: case 0x1f6: case 0x1f7:
	case 0x1f8: case 0x1f9: case 0x1fa: case 0x1fb:
	case 0x1fe: case 0x1ff:
		uprintf("math_emulate: 0x%04x not implemented\n",
		    code + 0xd800);
		math_abort(info, ksi, SIGILL, ILL_ILLOPC);
	}
	switch (code >> 3) {
	case 0x18: /* fadd */
		fadd(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x19: /* fmul */
		fmul(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x1a: /* fcom */
		fcom(PST(code & 7),PST(0));
		return(0);
	case 0x1b: /* fcomp */
		fcom(PST(code & 7),PST(0));
		fpop();
		return(0);
	case 0x1c: /* fsubr */
		real_to_real(&ST(code & 7),&tmp);
		tmp.exponent ^= 0x8000;
		fadd(PST(0),&tmp,&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x1d: /* fsub */
		ST(0).exponent ^= 0x8000;
		fadd(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x1e: /* fdivr */
		fdiv(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x1f: /* fdiv */
		fdiv(PST(code & 7),PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x38: /* fld */
		fpush();
		ST(0) = ST((code & 7)+1);
		return(0);
	case 0x39: /* fxch */
		fxchg(&ST(0),&ST(code & 7));
		return(0);
	case 0x3b: /* XXX */
		ST(code & 7) = ST(0);
		fpop();
		return(0);
	case 0x98: /* fadd */
		fadd(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0x99: /* fmul */
		fmul(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0x9a: /* XXX */
		fcom(PST(code & 7),PST(0));
		return(0);
	case 0x9b: /* XXX */
		fcom(PST(code & 7),PST(0));
		fpop();
		return(0);			
	case 0x9c: /* fsubr */
		ST(code & 7).exponent ^= 0x8000;
		fadd(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0x9d: /* fsub */
		real_to_real(&ST(0),&tmp);
		tmp.exponent ^= 0x8000;
		fadd(PST(code & 7),&tmp,&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0x9e: /* fdivr */
		fdiv(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0x9f: /* fdiv */
		fdiv(PST(code & 7),PST(0),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		return(0);
	case 0xb8: /* ffree */
		printf("ffree not implemented\n\r");
		math_abort(info, ksi, SIGILL, ILL_ILLOPC);
	case 0xb9: /* fstp XXX */
		fxchg(&ST(0),&ST(code & 7));
		return(0);
	case 0xba: /* fst */
		ST(code & 7) = ST(0);
		return(0);
	case 0xbb: /* XXX */
		ST(code & 7) = ST(0);
		fpop();
		return(0);
	case 0xbc: /* fucom */
		fucom(PST(code & 7),PST(0));
		return(0);
	case 0xbd: /* fucomp */
		fucom(PST(code & 7),PST(0));
		fpop();
		return(0);
	case 0xd8: /* faddp */
		fadd(PST(code & 7),PST(0),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xd9: /* fmulp */
		fmul(PST(code & 7),PST(0),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xda: /* XXX */
		fcom(PST(code & 7),PST(0));
		fpop();
		return(0);
	case 0xdc: /* fsubrp */
		ST(code & 7).exponent ^= 0x8000;
		fadd(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xdd: /* fsubp */
		real_to_real(&ST(0),&tmp);
		tmp.exponent ^= 0x8000;
		fadd(PST(code & 7),&tmp,&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xde: /* fdivrp */
		fdiv(PST(0),PST(code & 7),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xdf: /* fdivp */
		fdiv(PST(code & 7),PST(0),&tmp);
		real_to_real(&tmp,&ST(code & 7));
		fpop();
		return(0);
	case 0xf8: /* XXX */
		printf("ffree not implemented\n\r");
		math_abort(info, ksi, SIGILL, ILL_ILLOPC);
	case 0xf9: /* XXX */
		fxchg(&ST(0),&ST(code & 7));
		return(0);
	case 0xfa: /* XXX */
	case 0xfb: /* XXX */
		ST(code & 7) = ST(0);
		fpop();
		return(0);
	}
	switch ((code>>3) & 0xe7) {
	case 0x22:
		put_short_real(PST(0),info,code);
		return(0);
	case 0x23:
		put_short_real(PST(0),info,code);
		fpop();
		return(0);
	case 0x24:
		address = ea(info,code);
		copyin((u_long *) address, (u_long *) &I387, 28);
		return(0);
	case 0x25:
		address = ea(info,code);
		*(u_short *) &I387.cwd =
			fusword((u_short *) address);
		return(0);
	case 0x26:
		address = ea(info,code);
		copyout((u_long *) &I387, (u_long *) address, 28);
		return(0);
	case 0x27:
		address = ea(info,code);
		susword((u_short *) address, I387.cwd);
		return(0);
	case 0x62:
		put_long_int(PST(0),info,code);
		return(0);
	case 0x63:
		put_long_int(PST(0),info,code);
		fpop();
		return(0);
	case 0x65:
		fpush();
		get_temp_real(&tmp,info,code);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0x67:
		put_temp_real(PST(0),info,code);
		fpop();
		return(0);
	case 0xa2:
		put_long_real(PST(0),info,code);
		return(0);
	case 0xa3:
		put_long_real(PST(0),info,code);
		fpop();
		return(0);
	case 0xa4:
		address = ea(info,code);
		copyin((u_long *) address, (u_long *) &I387, 108);
		return(0);
	case 0xa6:
		address = ea(info,code);
		copyout((u_long *) &I387, (u_long *) address, 108);
		fninit();
		return(0);
	case 0xa7:
		address = ea(info,code);
		susword((u_short *) address, I387.swd);
		return(0);
	case 0xe2:
		put_short_int(PST(0),info,code);
		return(0);
	case 0xe3:
		put_short_int(PST(0),info,code);
		fpop();
		return(0);
	case 0xe4:
		fpush();
		get_BCD(&tmp,info,code);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0xe5:
		fpush();
		get_longlong_int(&tmp,info,code);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 0xe6:
		put_BCD(PST(0),info,code);
		fpop();
		return(0);
	case 0xe7:
		put_longlong_int(PST(0),info,code);
		fpop();
		return(0);
	}
	switch (code >> 9) {
	case 0:
		get_short_real(&tmp,info,code);
		break;
	case 1:
		get_long_int(&tmp,info,code);
		break;
	case 2:
		get_long_real(&tmp,info,code);
		break;
	case 4:
		get_short_int(&tmp,info,code);
	}
	switch ((code>>3) & 0x27) {
	case 0:
		fadd(&tmp,PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 1:
		fmul(&tmp,PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 2:
		fcom(&tmp,PST(0));
		return(0);
	case 3:
		fcom(&tmp,PST(0));
		fpop();
		return(0);
	case 4:
		tmp.exponent ^= 0x8000;
		fadd(&tmp,PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 5:
		ST(0).exponent ^= 0x8000;
		fadd(&tmp,PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 6:
		fdiv(PST(0),&tmp,&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	case 7:
		fdiv(&tmp,PST(0),&tmp);
		real_to_real(&tmp,&ST(0));
		return(0);
	}
	if ((code & 0x138) == 0x100) {
		fpush();
		real_to_real(&tmp,&ST(0));
		return(0);
	}
	printf("Unknown math-insns: %04x:%08x %04x\n\r",(u_short)info->tf_cs,
		info->tf_eip,code);
	math_abort(info, ksi, SIGFPE, FPE_FLTINV);
}

static void fpop(void)
{
	u_long tmp;

	tmp = I387.swd & 0xffffc7ff;
	I387.swd += 0x00000800;
	I387.swd &= 0x00003800;
	I387.swd |= tmp;
}

static void fpush(void)
{
	u_long tmp;

	tmp = I387.swd & 0xffffc7ff;
	I387.swd -= 0x00000800;
	I387.swd &= 0x00003800;
	I387.swd |= tmp;
}

static void fxchg(temp_real_unaligned * a, temp_real_unaligned * b)
{
	temp_real_unaligned c;

	c = *a;
	*a = *b;
	*b = c;
}

static temp_real_unaligned * __st(int i)
{
	i += I387.swd >> 11;
	i &= 7;
	return (temp_real_unaligned *) (i*10 + (char *)(I387.st_space));
}

/*
 * linux/kernel/math/ea.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * Calculate the effective address.
 */


static int __regoffset[] = {
	tEAX, tECX, tEDX, tEBX, tESP, tEBP, tESI, tEDI
};

#define REG(x) (((int *)curlwp->l_md.md_regs)[__regoffset[(x)]])

static char * sib(struct trapframe * info, int mod)
{
	u_char ss,index,base;
	long offset = 0;

	base = fubyte((char *) info->tf_eip);
	info->tf_eip++;
	ss = base >> 6;
	index = (base >> 3) & 7;
	base &= 7;
	if (index == 4)
		offset = 0;
	else
		offset = REG(index);
	offset <<= ss;
	if (mod || base != 5)
		offset += REG(base);
	if (mod == 1) {
		offset += (signed char) fubyte((char *) info->tf_eip);
		info->tf_eip++;
	} else if (mod == 2 || base == 5) {
		offset += (signed) fuword((u_long *) info->tf_eip);
		info->tf_eip += 4;
	}
	I387.foo = offset;
	I387.fos = 0x17;
	return (char *) offset;
}

char * ea(struct trapframe * info, u_short code)
{
	u_char mod,rm;
	long * tmp;
	int offset = 0;

	mod = (code >> 6) & 3;
	rm = code & 7;
	if (rm == 4 && mod != 3)
		return sib(info,mod);
	if (rm == 5 && !mod) {
		offset = fuword((u_long *) info->tf_eip);
		info->tf_eip += 4;
		I387.foo = offset;
		I387.fos = 0x17;
		return (char *) offset;
	}
	tmp = (long*)&REG(rm);
	switch (mod) {
		case 0: offset = 0; break;
		case 1:
			offset = (signed char) fubyte((char *) info->tf_eip);
			info->tf_eip++;
			break;
		case 2:
			offset = (signed) fuword((u_long *) info->tf_eip);
			info->tf_eip += 4;
			break;
#ifdef notyet
		case 3:
			math_abort(info, ksi, SIGILL, ILL_ILLOPN);
#endif
	}
	I387.foo = offset;
	I387.fos = 0x17;
	return offset + (char *) *tmp;
}
/*
 * linux/kernel/math/get_put.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This file handles all accesses to user memory: getting and putting
 * ints/reals/BCD etc. This is the only part that concerns itself with
 * other than temporary real format. All other cals are strictly temp_real.
 */

void get_short_real(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	short_real sr;

	addr = ea(info,code);
	sr = fuword((u_long *) addr);
	short_to_temp(&sr,tmp);
}

void get_long_real(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	long_real lr;

	addr = ea(info,code);
	lr.a = fuword((u_long *) addr);
	lr.b = fuword((u_long *) addr + 1);
	long_to_temp(&lr,tmp);
}

void get_temp_real(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;

	addr = ea(info,code);
	tmp->a = fuword((u_long *) addr);
	tmp->b = fuword((u_long *) addr + 1);
	tmp->exponent = fusword((u_short *) addr + 4);
}

void get_short_int(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = (signed short) fusword((u_short *) addr);
	ti.b = 0;
	if ((ti.sign = (ti.a < 0)) != 0)
		ti.a = - ti.a;
	int_to_real(&ti,tmp);
}

void get_long_int(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = fuword((u_long *) addr);
	ti.b = 0;
	if ((ti.sign = (ti.a < 0)) != 0)
		ti.a = - ti.a;
	int_to_real(&ti,tmp);
}

void get_longlong_int(temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = fuword((u_long *) addr);
	ti.b = fuword((u_long *) addr + 1);
	if ((ti.sign = (ti.b < 0)) != 0)
		__asm__("notl %0 ; notl %1\n\t"
			"addl $1,%0 ; adcl $0,%1"
			:"=r" (ti.a),"=r" (ti.b)
			:"0" (ti.a),"1" (ti.b));
	int_to_real(&ti,tmp);
}

#define MUL10(low,high) \
__asm__("addl %0,%0 ; adcl %1,%1\n\t" \
"movl %0,%%ecx ; movl %1,%%ebx\n\t" \
"addl %0,%0 ; adcl %1,%1\n\t" \
"addl %0,%0 ; adcl %1,%1\n\t" \
"addl %%ecx,%0 ; adcl %%ebx,%1" \
:"=a" (low),"=d" (high) \
:"0" (low),"1" (high):"cx","bx")

#define ADD64(val,low,high) \
__asm__("addl %4,%0 ; adcl $0,%1":"=r" (low),"=r" (high) \
:"0" (low),"1" (high),"r" ((u_long) (val)))

void get_BCD(temp_real * tmp, struct trapframe * info, u_short code)
{
	int k;
	char * addr;
	temp_int i;
	u_char c;

	addr = ea(info,code);
	addr += 9;
	i.sign = 0x80 & fubyte(addr--);
	i.a = i.b = 0;
	for (k = 0; k < 9; k++) {
		c = fubyte(addr--);
		MUL10(i.a, i.b);
		ADD64((c>>4), i.a, i.b);
		MUL10(i.a, i.b);
		ADD64((c&0xf), i.a, i.b);
	}
	int_to_real(&i,tmp);
}

void put_short_real(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	short_real sr;

	addr = ea(info,code);
	temp_to_short(tmp,&sr);
	suword((u_long *) addr,sr);
}

void put_long_real(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	long_real lr;

	addr = ea(info,code);
	temp_to_long(tmp,&lr);
	suword((u_long *) addr, lr.a);
	suword((u_long *) addr + 1, lr.b);
}

void put_temp_real(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;

	addr = ea(info,code);
	suword((u_long *) addr, tmp->a);
	suword((u_long *) addr + 1, tmp->b);
	susword((u_short *) addr + 4, tmp->exponent);
}

void put_short_int(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	if (ti.sign)
		ti.a = -ti.a;
	susword((u_short *) addr,ti.a);
}

void put_long_int(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	if (ti.sign)
		ti.a = -ti.a;
	suword((u_long *) addr, ti.a);
}

void put_longlong_int(const temp_real * tmp,
	struct trapframe * info, u_short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	if (ti.sign)
		__asm__("notl %0 ; notl %1\n\t"
			"addl $1,%0 ; adcl $0,%1"
			:"=r" (ti.a),"=r" (ti.b)
			:"0" (ti.a),"1" (ti.b));
	suword((u_long *) addr, ti.a);
	suword((u_long *) addr + 1, ti.b);
}

#define DIV10(low,high,rem) \
__asm__("divl %6 ; xchgl %1,%2 ; divl %6" \
	:"=d" (rem),"=a" (low),"=r" (high) \
	:"0" (0),"1" (high),"2" (low),"c" (10))

void put_BCD(const temp_real * tmp,struct trapframe * info, u_short code)
{
	int k,rem;
	char * addr;
	temp_int i;
	u_char c;

	addr = ea(info,code);
	real_to_int(tmp,&i);
	if (i.sign)
		subyte(addr+9,0x80);
	else
		subyte(addr+9,0x00);
	for (k = 0; k < 9; k++) {
		DIV10(i.a,i.b,rem);
		c = rem;
		DIV10(i.a,i.b,rem);
		c += rem<<4;
		subyte(addr++,c);
	}
}

/*
 * linux/kernel/math/mul.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real multiplication routine.
 */


static void shift(int * c)
{
	__asm__("movl (%0),%%eax ; addl %%eax,(%0)\n\t"
		"movl 4(%0),%%eax ; adcl %%eax,4(%0)\n\t"
		"movl 8(%0),%%eax ; adcl %%eax,8(%0)\n\t"
		"movl 12(%0),%%eax ; adcl %%eax,12(%0)"
		::"r" ((long) c):"ax");
}

static void mul64(const temp_real * a, const temp_real * b, int * c)
{
	__asm__("movl (%0),%%eax\n\t"
		"mull (%1)\n\t"
		"movl %%eax,(%2)\n\t"
		"movl %%edx,4(%2)\n\t"
		"movl 4(%0),%%eax\n\t"
		"mull 4(%1)\n\t"
		"movl %%eax,8(%2)\n\t"
		"movl %%edx,12(%2)\n\t"
		"movl (%0),%%eax\n\t"
		"mull 4(%1)\n\t"
		"addl %%eax,4(%2)\n\t"
		"adcl %%edx,8(%2)\n\t"
		"adcl $0,12(%2)\n\t"
		"movl 4(%0),%%eax\n\t"
		"mull (%1)\n\t"
		"addl %%eax,4(%2)\n\t"
		"adcl %%edx,8(%2)\n\t"
		"adcl $0,12(%2)"
		::"S" ((long) a),"c" ((long) b),"D" ((long) c)
		:"ax","dx");
}

void fmul(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	int i,sign;
	int tmp[4] = {0,0,0,0};

	sign = (src1->exponent ^ src2->exponent) & 0x8000;
	i = (src1->exponent & 0x7fff) + (src2->exponent & 0x7fff) - 16383 + 1;
	if (i<0) {
		result->exponent = sign;
		result->a = result->b = 0;
		return;
	}
	if (i>0x7fff) {
		set_OE();
		return;
	}
	mul64(src1,src2,tmp);
	if (tmp[0] || tmp[1] || tmp[2] || tmp[3])
		while (i && tmp[3] >= 0) {
			i--;
			shift(tmp);
		}
	else
		i = 0;
	result->exponent = i | sign;
	result->a = tmp[2];
	result->b = tmp[3];
}

/*
 * linux/kernel/math/div.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real division routine.
 */

#include <i386/i386/math_emu.h>

static void shift_left(int * c)
{
	__asm__ __volatile__("movl (%0),%%eax ; addl %%eax,(%0)\n\t"
		"movl 4(%0),%%eax ; adcl %%eax,4(%0)\n\t"
		"movl 8(%0),%%eax ; adcl %%eax,8(%0)\n\t"
		"movl 12(%0),%%eax ; adcl %%eax,12(%0)"
		::"r" ((long) c):"ax");
}

static void shift_right(int * c)
{
	__asm__("shrl $1,12(%0) ; rcrl $1,8(%0) ; rcrl $1,4(%0) ; rcrl $1,(%0)"
		::"r" ((long) c));
}

static int try_sub(int * a, int * b)
{
	char ok;

	__asm__ __volatile__("movl (%1),%%eax ; subl %%eax,(%2)\n\t"
		"movl 4(%1),%%eax ; sbbl %%eax,4(%2)\n\t"
		"movl 8(%1),%%eax ; sbbl %%eax,8(%2)\n\t"
		"movl 12(%1),%%eax ; sbbl %%eax,12(%2)\n\t"
		"setae %%al":"=a" (ok):"c" ((long) a),"d" ((long) b));
	return ok;
}

static void div64(int * a, int * b, int * c)
{
	int tmp[4];
	int i;
	u_int mask = 0;

	c += 4;
	for (i = 0 ; i<64 ; i++) {
		if (!(mask >>= 1)) {
			c--;
			mask = 0x80000000;
		}
		tmp[0] = a[0]; tmp[1] = a[1];
		tmp[2] = a[2]; tmp[3] = a[3];
		if (try_sub(b,tmp)) {
			*c |= mask;
			a[0] = tmp[0]; a[1] = tmp[1];
			a[2] = tmp[2]; a[3] = tmp[3];
		}
		shift_right(b);
	}
}

void fdiv(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	int i,sign;
	int a[4],b[4],tmp[4] = {0,0,0,0};

	sign = (src1->exponent ^ src2->exponent) & 0x8000;
	if (!(src2->a || src2->b)) {
		set_ZE();
		return;
	}
	i = (src1->exponent & 0x7fff) - (src2->exponent & 0x7fff) + 16383;
	if (i<0) {
		set_UE();
		result->exponent = sign;
		result->a = result->b = 0;
		return;
	}
	a[0] = a[1] = 0;
	a[2] = src1->a;
	a[3] = src1->b;
	b[0] = b[1] = 0;
	b[2] = src2->a;
	b[3] = src2->b;
	while (b[3] >= 0) {
		i++;
		shift_left(b);
	}
	div64(a,b,tmp);
	if (tmp[0] || tmp[1] || tmp[2] || tmp[3]) {
		while (i && tmp[3] >= 0) {
			i--;
			shift_left(tmp);
		}
		if (tmp[3] >= 0)
			set_DE();
	} else
		i = 0;
	if (i>0x7fff) {
		set_OE();
		return;
	}
	if (tmp[0] || tmp[1])
		set_PE();
	result->exponent = i | sign;
	result->a = tmp[2];
	result->b = tmp[3];
}

/*
 * linux/kernel/math/add.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real addition routine.
 *
 * NOTE! These aren't exact: they are only 62 bits wide, and don't do
 * correct rounding. Fast hack. The reason is that we shift right the
 * values by two, in order not to have overflow (1 bit), and to be able
 * to move the sign into the mantissa (1 bit). Much simpler algorithms,
 * and 62 bits (61 really - no rounding) accuracy is usually enough. The
 * only time you should notice anything weird is when adding 64-bit
 * integers together. When using doubles (52 bits accuracy), the
 * 61-bit accuracy never shows at all.
 */

#define NEGINT(a) \
__asm__("notl %0 ; notl %1 ; addl $1,%0 ; adcl $0,%1" \
	:"=r" (a->a),"=r" (a->b) \
	:"0" (a->a),"1" (a->b))

static void signify(temp_real * a)
{
	a->exponent += 2;
	__asm__("shrdl $2,%1,%0 ; shrl $2,%1"
		:"=r" (a->a),"=r" (a->b)
		:"0" (a->a),"1" (a->b));
	if (a->exponent < 0)
		NEGINT(a);
	a->exponent &= 0x7fff;
}

static void unsignify(temp_real * a)
{
	if (!(a->a || a->b)) {
		a->exponent = 0;
		return;
	}
	a->exponent &= 0x7fff;
	if (a->b < 0) {
		NEGINT(a);
		a->exponent |= 0x8000;
	}
	while (a->b >= 0) {
		a->exponent--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (a->a),"=r" (a->b)
			:"0" (a->a),"1" (a->b));
	}
}

void fadd(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	temp_real a,b;
	int x1,x2,shift;

	x1 = src1->exponent & 0x7fff;
	x2 = src2->exponent & 0x7fff;
	if (x1 > x2) {
		a = *src1;
		b = *src2;
		shift = x1-x2;
	} else {
		a = *src2;
		b = *src1;
		shift = x2-x1;
	}
	if (shift >= 64) {
		*result = a;
		return;
	}
	if (shift >= 32) {
		b.a = b.b;
		b.b = 0;
		shift -= 32;
	}
	__asm__("shrdl %4,%1,%0 ; shrl %4,%1"
		:"=r" (b.a),"=r" (b.b)
		:"0" (b.a),"1" (b.b),"c" ((char) shift));
	signify(&a);
	signify(&b);
	__asm__("addl %4,%0 ; adcl %5,%1"
		:"=r" (a.a),"=r" (a.b)
		:"0" (a.a),"1" (a.b),"g" (b.a),"g" (b.b));
	unsignify(&a);
	*result = a;
}

/*
 * linux/kernel/math/compare.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real comparison routines
 */


#define clear_Cx() (I387.swd &= ~0x4500)

static void normalize(temp_real * a)
{
	int i = a->exponent & 0x7fff;
	int sign = a->exponent & 0x8000;

	if (!(a->a || a->b)) {
		a->exponent = 0;
		return;
	}
	while (i && a->b >= 0) {
		i--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (a->a),"=r" (a->b)
			:"0" (a->a),"1" (a->b));
	}
	a->exponent = i | sign;
}

void ftst(const temp_real * a)
{
	temp_real b;

	clear_Cx();
	b = *a;
	normalize(&b);
	if (b.a || b.b || b.exponent) {
		if (b.exponent < 0)
			set_C0();
	} else
		set_C3();
}

void fcom(const temp_real * src1, const temp_real * src2)
{
	temp_real a;

	a = *src1;
	a.exponent ^= 0x8000;
	fadd(&a,src2,&a);
	ftst(&a);
}

void fucom(const temp_real * src1, const temp_real * src2)
{
	fcom(src1,src2);
}

/*
 * linux/kernel/math/convert.c
 *
 * (C) 1991 Linus Torvalds
 */


/*
 * NOTE!!! There is some "non-obvious" optimisations in the temp_to_long
 * and temp_to_short conversion routines: don't touch them if you don't
 * know what's going on. They are the adding of one in the rounding: the
 * overflow bit is also used for adding one into the exponent. Thus it
 * looks like the overflow would be incorrectly handled, but due to the
 * way the IEEE numbers work, things are correct.
 *
 * There is no checking for total overflow in the conversions, though (ie
 * if the temp-real number simply won't fit in a short- or long-real.)
 */

void short_to_temp(const short_real * a, temp_real * b)
{
	if (!(*a & 0x7fffffff)) {
		b->a = b->b = 0;
		if (*a)
			b->exponent = 0x8000;
		else
			b->exponent = 0;
		return;
	}
	b->exponent = ((*a>>23) & 0xff)-127+16383;
	if (*a<0)
		b->exponent |= 0x8000;
	b->b = (*a<<8) | 0x80000000;
	b->a = 0;
}

void long_to_temp(const long_real * a, temp_real * b)
{
	if (!a->a && !(a->b & 0x7fffffff)) {
		b->a = b->b = 0;
		if (a->b)
			b->exponent = 0x8000;
		else
			b->exponent = 0;
		return;
	}
	b->exponent = ((a->b >> 20) & 0x7ff)-1023+16383;
	if (a->b<0)
		b->exponent |= 0x8000;
	b->b = 0x80000000 | (a->b<<11) | (((u_long)a->a)>>21);
	b->a = a->a<<11;
}

void temp_to_short(const temp_real * a, short_real * b)
{
	if (!(a->exponent & 0x7fff)) {
		*b = (a->exponent)?0x80000000:0;
		return;
	}
	*b = ((((long) a->exponent)-16383+127) << 23) & 0x7f800000;
	if (a->exponent < 0)
		*b |= 0x80000000;
	*b |= (a->b >> 8) & 0x007fffff;
	switch (ROUNDING) {
		case ROUND_NEAREST:
			if ((a->b & 0xff) > 0x80)
				++*b;
			break;
		case ROUND_DOWN:
			if ((a->exponent & 0x8000) && (a->b & 0xff))
				++*b;
			break;
		case ROUND_UP:
			if (!(a->exponent & 0x8000) && (a->b & 0xff))
				++*b;
			break;
	}
}

void temp_to_long(const temp_real * a, long_real * b)
{
	if (!(a->exponent & 0x7fff)) {
		b->a = 0;
		b->b = (a->exponent)?0x80000000:0;
		return;
	}
	b->b = (((0x7fff & (long) a->exponent)-16383+1023) << 20) & 0x7ff00000;
	if (a->exponent < 0)
		b->b |= 0x80000000;
	b->b |= (a->b >> 11) & 0x000fffff;
	b->a = a->b << 21;
	b->a |= (a->a >> 11) & 0x001fffff;
	switch (ROUNDING) {
		case ROUND_NEAREST:
			if ((a->a & 0x7ff) > 0x400)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if ((a->exponent & 0x8000) && (a->b & 0xff))
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_UP:
			if (!(a->exponent & 0x8000) && (a->b & 0xff))
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
}

void frndint(const temp_real * a, temp_real * b)
{
	int shift =  16383 + 63 - (a->exponent & 0x7fff);
	u_long underflow;

	if ((shift < 0) || (shift == 16383+63)) {
		*b = *a;
		return;
	}
	b->a = b->b = underflow = 0;
	b->exponent = a->exponent;
	if (shift < 32) {
		b->b = a->b; b->a = a->a;
	} else if (shift < 64) {
		b->a = a->b; underflow = a->a;
		shift -= 32;
		b->exponent += 32;
	} else if (shift < 96) {
		underflow = a->b;
		shift -= 64;
		b->exponent += 64;
	} else {
		underflow = 1;
		shift = 0;
	}
	b->exponent += shift;
	__asm__("shrdl %2,%1,%0"
		:"=r" (underflow),"=r" (b->a)
		:"c" ((char) shift),"0" (underflow),"1" (b->a));
	__asm__("shrdl %2,%1,%0"
		:"=r" (b->a),"=r" (b->b)
		:"c" ((char) shift),"0" (b->a),"1" (b->b));
	__asm__("shrl %1,%0"
		:"=r" (b->b)
		:"c" ((char) shift),"0" (b->b));
	switch (ROUNDING) {
		case ROUND_NEAREST:
			__asm__("addl %4,%5 ; adcl $0,%0 ; adcl $0,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b)
				,"r" (0x7fffffff + (b->a & 1))
				,"m" (*&underflow));
			break;
		case ROUND_UP:
			if ((b->exponent >= 0) && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if ((b->exponent < 0) && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
	if (b->a || b->b)
		while (b->b >= 0) {
			b->exponent--;
			__asm__("addl %0,%0 ; adcl %1,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b));
		}
	else
		b->exponent = 0;
}

void Fscale(const temp_real *a, const temp_real *b, temp_real *c) 
{
	temp_int ti;

	*c = *a;
	if(!c->a && !c->b) {				/* 19 Sep 92*/
		c->exponent = 0;
		return;
	}
	real_to_int(b, &ti);
	if(ti.sign)
		c->exponent -= ti.a;
	else
		c->exponent += ti.a;
}

void real_to_int(const temp_real * a, temp_int * b)
{
	int shift =  16383 + 63 - (a->exponent & 0x7fff);
	u_long underflow;

	b->a = b->b = underflow = 0;
	b->sign = (a->exponent < 0);
	if (shift < 0) {
		set_OE();
		return;
	}
	if (shift < 32) {
		b->b = a->b; b->a = a->a;
	} else if (shift < 64) {
		b->a = a->b; underflow = a->a;
		shift -= 32;
	} else if (shift < 96) {
		underflow = a->b;
		shift -= 64;
	} else {
		underflow = 1;
		shift = 0;
	}
	__asm__("shrdl %2,%1,%0"
		:"=r" (underflow),"=r" (b->a)
		:"c" ((char) shift),"0" (underflow),"1" (b->a));
	__asm__("shrdl %2,%1,%0"
		:"=r" (b->a),"=r" (b->b)
		:"c" ((char) shift),"0" (b->a),"1" (b->b));
	__asm__("shrl %1,%0"
		:"=r" (b->b)
		:"c" ((char) shift),"0" (b->b));
	switch (ROUNDING) {
		case ROUND_NEAREST:
			__asm__("addl %4,%5 ; adcl $0,%0 ; adcl $0,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b)
				,"r" (0x7fffffff + (b->a & 1))
				,"m" (*&underflow));
			break;
		case ROUND_UP:
			if (!b->sign && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if (b->sign && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
}

void int_to_real(const temp_int * a, temp_real * b)
{
	b->a = a->a;
	b->b = a->b;
	if (b->a || b->b)
		b->exponent = 16383 + 63 + (a->sign? 0x8000:0);
	else {
		b->exponent = 0;
		return;
	}
	while (b->b >= 0) {
		b->exponent--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (b->a),"=r" (b->b)
			:"0" (b->a),"1" (b->b));
	}
}
