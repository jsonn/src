/*	$NetBSD: db_trace.c,v 1.3.8.3 2001/04/21 17:54:32 bouyer Exp $	*/
/*	$OpenBSD: db_trace.c,v 1.3 1997/03/21 02:10:48 niklas Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <machine/pmap.h>

#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

const struct db_variable db_regs[] = {
	{ "r0",  (long *)&ddb_regs.r[0],  FCN_NULL },
	{ "r1",  (long *)&ddb_regs.r[1],  FCN_NULL },
	{ "r2",  (long *)&ddb_regs.r[2],  FCN_NULL },
	{ "r3",  (long *)&ddb_regs.r[3],  FCN_NULL },
	{ "r4",  (long *)&ddb_regs.r[4],  FCN_NULL },
	{ "r5",  (long *)&ddb_regs.r[5],  FCN_NULL },
	{ "r6",  (long *)&ddb_regs.r[6],  FCN_NULL },
	{ "r7",  (long *)&ddb_regs.r[7],  FCN_NULL },
	{ "r8",  (long *)&ddb_regs.r[8],  FCN_NULL },
	{ "r9",  (long *)&ddb_regs.r[9],  FCN_NULL },
	{ "r10", (long *)&ddb_regs.r[10], FCN_NULL },
	{ "r11", (long *)&ddb_regs.r[11], FCN_NULL },
	{ "r12", (long *)&ddb_regs.r[12], FCN_NULL },
	{ "r13", (long *)&ddb_regs.r[13], FCN_NULL },
	{ "r14", (long *)&ddb_regs.r[14], FCN_NULL },
	{ "r15", (long *)&ddb_regs.r[15], FCN_NULL },
	{ "r16", (long *)&ddb_regs.r[16], FCN_NULL },
	{ "r17", (long *)&ddb_regs.r[17], FCN_NULL },
	{ "r18", (long *)&ddb_regs.r[18], FCN_NULL },
	{ "r19", (long *)&ddb_regs.r[19], FCN_NULL },
	{ "r20", (long *)&ddb_regs.r[20], FCN_NULL },
	{ "r21", (long *)&ddb_regs.r[21], FCN_NULL },
	{ "r22", (long *)&ddb_regs.r[22], FCN_NULL },
	{ "r23", (long *)&ddb_regs.r[23], FCN_NULL },
	{ "r24", (long *)&ddb_regs.r[24], FCN_NULL },
	{ "r25", (long *)&ddb_regs.r[25], FCN_NULL },
	{ "r26", (long *)&ddb_regs.r[26], FCN_NULL },
	{ "r27", (long *)&ddb_regs.r[27], FCN_NULL },
	{ "r28", (long *)&ddb_regs.r[28], FCN_NULL },
	{ "r29", (long *)&ddb_regs.r[29], FCN_NULL },
	{ "r30", (long *)&ddb_regs.r[30], FCN_NULL },
	{ "r31", (long *)&ddb_regs.r[31], FCN_NULL },
	{ "iar", (long *)&ddb_regs.iar,   FCN_NULL },
	{ "msr", (long *)&ddb_regs.msr,   FCN_NULL },
};
const struct db_variable * const db_eregs = db_regs + sizeof (db_regs)/sizeof (db_regs[0]);

extern label_t	*db_recover;

/*
 *	Frame tracing.
 */
void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
	void (*pr) __P((const char *, ...));
{
	db_addr_t frame, lr, caller;
	db_expr_t diff;
	db_sym_t sym;
	char *symname;
	boolean_t kernel_only = TRUE;
	boolean_t trace_thread = FALSE;
	boolean_t full = FALSE;

	{
		register char *cp = modif;
		register char c;

		while ((c = *cp++) != 0) {
			if (c == 't')
				trace_thread = TRUE;
			if (c == 'u')
				kernel_only = FALSE;
			if (c == 'f')
				full = TRUE;
		}
	}

	frame = (db_addr_t)ddb_regs.r[1];
	while ((frame = *(db_addr_t *)frame) && count--) {
		db_addr_t *args = (db_addr_t *)(frame + 8);

		lr = *(db_addr_t *)(frame + 4) - 4;
		if (lr & 3) {
			(*pr)("saved LR(0x%x) is invalid.", lr);
			break;
		}
		if ((caller = (db_addr_t)vtophys(lr)) == 0)
			caller = lr;

		if (full)
			/* Print all the args stored in that stackframe. */
			printf("(%lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx) %lx ",
				args[0], args[1], args[2], args[3],
				args[4], args[5], args[6], args[7], frame);

		diff = 0;
		symname = NULL;
		sym = db_search_symbol(caller, DB_STGY_ANY, &diff);
		db_symbol_values(sym, &symname, 0);
		if (symname == NULL)
			(*pr)("at %p\n", caller);
		else
			(*pr)("at %s+%x\n", symname, diff);
	}
}
