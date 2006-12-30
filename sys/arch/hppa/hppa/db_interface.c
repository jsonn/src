/*	$NetBSD: db_interface.c,v 1.4.2.2 2006/12/30 20:46:04 yamt Exp $	*/

/*	$OpenBSD: db_interface.c,v 1.16 2001/03/22 23:31:45 mickey Exp $	*/

/*
 * Copyright (c) 1999-2000 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_interface.c,v 1.4.2.2 2006/12/30 20:46:04 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/cpufunc.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_run.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/ddbvar.h>

#include <dev/cons.h>

void kdbprinttrap(int, int);

extern label_t *db_recover;
extern int db_active;
extern char *trap_type[];
extern int trap_types;

db_regs_t	ddb_regs;
const struct db_variable db_regs[] = {
	{ "flags", (long *)&ddb_regs.tf_flags,  FCN_NULL },
	{ "r1",    (long *)&ddb_regs.tf_r1,  FCN_NULL },
	{ "rp",    (long *)&ddb_regs.tf_rp,  FCN_NULL },
	{ "r3",    (long *)&ddb_regs.tf_r3,  FCN_NULL },
	{ "r4",    (long *)&ddb_regs.tf_r4,  FCN_NULL },
	{ "r5",    (long *)&ddb_regs.tf_r5,  FCN_NULL },
	{ "r6",    (long *)&ddb_regs.tf_r6,  FCN_NULL },
	{ "r7",    (long *)&ddb_regs.tf_r7,  FCN_NULL },
	{ "r8",    (long *)&ddb_regs.tf_r8,  FCN_NULL },
	{ "r9",    (long *)&ddb_regs.tf_r9,  FCN_NULL },
	{ "r10",   (long *)&ddb_regs.tf_r10, FCN_NULL },
	{ "r11",   (long *)&ddb_regs.tf_r11, FCN_NULL },
	{ "r12",   (long *)&ddb_regs.tf_r12, FCN_NULL },
	{ "r13",   (long *)&ddb_regs.tf_r13, FCN_NULL },
	{ "r14",   (long *)&ddb_regs.tf_r14, FCN_NULL },
	{ "r15",   (long *)&ddb_regs.tf_r15, FCN_NULL },
	{ "r16",   (long *)&ddb_regs.tf_r16, FCN_NULL },
	{ "r17",   (long *)&ddb_regs.tf_r17, FCN_NULL },
	{ "r18",   (long *)&ddb_regs.tf_r18, FCN_NULL },
	{ "t4",    (long *)&ddb_regs.tf_t4,  FCN_NULL },
	{ "t3",    (long *)&ddb_regs.tf_t3,  FCN_NULL },
	{ "t2",    (long *)&ddb_regs.tf_t2,  FCN_NULL },
	{ "t1",    (long *)&ddb_regs.tf_t1,  FCN_NULL },
	{ "arg3",  (long *)&ddb_regs.tf_arg3,  FCN_NULL },
	{ "arg2",  (long *)&ddb_regs.tf_arg2,  FCN_NULL },
	{ "arg1",  (long *)&ddb_regs.tf_arg1,  FCN_NULL },
	{ "arg0",  (long *)&ddb_regs.tf_arg0,  FCN_NULL },
	{ "dp",    (long *)&ddb_regs.tf_dp,    FCN_NULL },
	{ "ret0",  (long *)&ddb_regs.tf_ret0,  FCN_NULL },
	{ "ret1",  (long *)&ddb_regs.tf_ret1,  FCN_NULL },
	{ "sp",    (long *)&ddb_regs.tf_sp,    FCN_NULL },
	{ "r31",   (long *)&ddb_regs.tf_r31,   FCN_NULL },
	{ "sar",   (long *)&ddb_regs.tf_sar,   FCN_NULL },

	{ "eirr",  (long *)&ddb_regs.tf_eirr,  FCN_NULL },
	{ "eiem",  (long *)&ddb_regs.tf_eiem,  FCN_NULL },
	{ "iir",   (long *)&ddb_regs.tf_iir,   FCN_NULL },
	{ "isr",   (long *)&ddb_regs.tf_isr,   FCN_NULL },
	{ "ior",   (long *)&ddb_regs.tf_ior,   FCN_NULL },
	{ "ipsw",  (long *)&ddb_regs.tf_ipsw,  FCN_NULL },
	{ "iisqh", (long *)&ddb_regs.tf_iisq_head,  FCN_NULL },
	{ "iioqh", (long *)&ddb_regs.tf_iioq_head,  FCN_NULL },
	{ "iisqt", (long *)&ddb_regs.tf_iisq_tail,  FCN_NULL },
	{ "iioqt", (long *)&ddb_regs.tf_iioq_tail,  FCN_NULL },

	{ "sr0",   (long *)&ddb_regs.tf_sr0,   FCN_NULL },
	{ "sr1",   (long *)&ddb_regs.tf_sr1,   FCN_NULL },
	{ "sr2",   (long *)&ddb_regs.tf_sr2,   FCN_NULL },
	{ "sr3",   (long *)&ddb_regs.tf_sr3,   FCN_NULL },
	{ "sr4",   (long *)&ddb_regs.tf_sr4,   FCN_NULL },
	{ "sr5",   (long *)&ddb_regs.tf_sr5,   FCN_NULL },
	{ "sr6",   (long *)&ddb_regs.tf_sr6,   FCN_NULL },
	{ "sr7",   (long *)&ddb_regs.tf_sr7,   FCN_NULL },

	{ "pidr1", (long *)&ddb_regs.tf_pidr1, FCN_NULL },
	{ "pidr2", (long *)&ddb_regs.tf_pidr2, FCN_NULL },
#ifdef pbably_not_worth_it
	{ "pidr3", (long *)&ddb_regs.tf_pidr3, FCN_NULL },
	{ "pidr4", (long *)&ddb_regs.tf_pidr4, FCN_NULL },
#endif

	{ "hptm",  (long *)&ddb_regs.tf_hptm,  FCN_NULL },
	{ "vtop",  (long *)&ddb_regs.tf_vtop,  FCN_NULL },
	{ "cr28",  (long *)&ddb_regs.tf_cr28,  FCN_NULL },
	{ "cr30",  (long *)&ddb_regs.tf_cr30,  FCN_NULL },
};
const struct db_variable * const db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);
int db_active = 0;

void
Debugger(void)
{
	__asm volatile ("break	%0, %1"
			  :: "i" (HPPA_BREAK_KERNEL), "i" (HPPA_BREAK_KGDB));
}

/*
 * Print trap reason.
 */
void
kdbprinttrap(int type, int code)
{
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 *  kdb_trap - field a BPT trap
 */
int
kdb_trap(int type, int code, db_regs_t *regs)
{
	int s;

	switch (type) {
	case T_RECOVERY:
	case T_IBREAK:
	case T_DBREAK:
	case -1:
		break;
	default:
		if (!db_onpanic && db_recover == 0)
			return 0;

		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Caught exception in DDB; continuing...\n");
			/* NOT REACHED */
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	*regs = ddb_regs;
	return (1);
}

/*
 *  Validate an address for use as a breakpoint.
 *  Any address is allowed for now.
 */
int
db_valid_breakpoint(db_addr_t addr)
{
	return (1);
}

void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count,
    const char *modif, void (*pr)(const char *, ...))
{
	register_t *fp, pc, rp, nargs, *argp;
	db_sym_t sym;
	db_expr_t off;
	const char *name;
	char **argnp, *argnames[HPPA_FRAME_NARGS];

	if (count < 0)
		count = 65536;

	if (!have_addr) {
		fp = (register_t *)ddb_regs.tf_r3;
		pc = ddb_regs.tf_iioq_head;
		rp = ddb_regs.tf_rp;
	} else {
		fp = (register_t *)addr;
		pc = 0;
		rp = ((register_t *)fp)[-5];
	}

#ifdef DDB_DEBUG
	pr(">> %x, %x, %x\t", fp, pc, rp);
#endif
	while (fp && count--) {

		if (USERMODE(pc))
			return;

		sym = db_search_symbol(pc, DB_STGY_ANY, &off);
		db_symbol_values (sym, &name, NULL);

		pr("%s(", name);

		/* args */
		nargs = HPPA_FRAME_NARGS;
		argnp = NULL;
		if (db_sym_numargs(sym, &nargs, argnames))
			argnp = argnames;
		else
			nargs = 4;
		/*
		 * XXX first four args are passed on registers, and may not
		 * be stored on stack, dunno how to recover their values yet
		 */
		for (argp = &fp[-9]; nargs--; argp--) {
			if (argnp)
				pr("%s=", *argnp++);
			pr("%lx%s", db_get_value((int)argp, 4, FALSE),
				  nargs? ",":"");
		}
		pr(") at ");
		db_printsym(pc, DB_STGY_PROC, pr);
		pr("\n");

		/* TODO: print locals */

		/* next frame */
		pc = rp;
		rp = fp[-5];

		/* if a terminal frame and not a start of a page
		 * then skip the trapframe and the terminal frame */
		if (!fp[0]) {
			struct trapframe *tf;

			tf = (struct trapframe *)((char *)fp - sizeof(*tf));

			if (tf->tf_flags & TFF_SYS)
				pr("-- syscall #%d(%x, %x, %x, %x, ...)\n",
				    tf->tf_t1, tf->tf_arg0, tf->tf_arg1,
				    tf->tf_arg2, tf->tf_arg3);
			else
				pr("-- trap #%d%s\n", tf->tf_flags & 0x3f,
				    (tf->tf_flags & T_USER)? " from user" : "");

			if (!(tf->tf_flags & TFF_LAST)) {
				fp = (register_t *)tf->tf_r3;
				pc = tf->tf_iioq_head;
				rp = tf->tf_rp;
			} else
				fp = 0;
		} else
			fp = (register_t *)fp[0];
#ifdef DDB_DEBUG
		pr(">> %x, %x, %x\t", fp, pc, rp);
#endif
	}

	if (count && pc) {
		db_printsym(pc, DB_STGY_XTRN, pr);
		pr(":\n");
	}
}
