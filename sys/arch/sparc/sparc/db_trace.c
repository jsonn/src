/*	$NetBSD: db_trace.c,v 1.10.6.2.2.2 1999/07/04 06:46:16 chs Exp $ */

/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

#define INKERNEL(va)	(((vaddr_t)(va)) >= USRSTACK)

void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	struct frame	*frame;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;
	char		c, *cp = modif;

	while ((c = *cp++) != 0) {
		if (c == 't')
			trace_thread = TRUE;
		if (c == 'u')
			kernel_only = FALSE;
	}

	if (count == -1)
		count = 65535;

	if (!have_addr)
		frame = (struct frame *)DDB_TF->tf_out[6];
	else {
		if (trace_thread) {
			struct proc *p;
			struct user *u;
			db_printf ("trace: pid %d ", (int)addr);
			p = pfind(addr);
			if (p == NULL) {
				db_printf("not found\n");
				return;
			}	
			if ((p->p_flag & P_INMEM) == 0) {
				db_printf("swapped out\n");
				return;
			}
			u = p->p_addr;
			frame = (struct frame *)u->u_pcb.pcb_sp;
			db_printf("at %p\n", frame);
		} else {
			frame = (struct frame *)addr;
		}
	}

	while (count--) {
		int		i;
		db_expr_t	offset;
		char		*name;
		db_addr_t	pc;

		pc = frame->fr_pc;
		if (!INKERNEL(pc))
			break;

		/*
		 * Switch to frame that contains arguments
		 */
		frame = frame->fr_fp;
		if (!INKERNEL(frame))
			break;

		db_find_sym_and_offset(pc, &name, &offset);
		if (name == NULL)
			name = "?";

		db_printf("%s(", name);

		/*
		 * Print %i0..%i5, hope these still reflect the
		 * actual arguments somewhat...
		 */
		for (i=0; i < 5; i++)
			db_printf("0x%x, ", frame->fr_arg[i]);
		db_printf("0x%x) at ", frame->fr_arg[i]);
		db_printsym(pc, DB_STGY_PROC);
		db_printf("\n");

	}
}
