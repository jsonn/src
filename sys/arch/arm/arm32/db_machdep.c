/*	$NetBSD: db_machdep.c,v 1.11.56.1 2009/04/28 07:33:43 skrll Exp $	*/

/* 
 * Copyright (c) 1996 Mark Brinicombe
 *
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_machdep.c,v 1.11.56.1 2009/04/28 07:33:43 skrll Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/systm.h>

#include <arm/arm32/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>


void
db_show_panic_cmd(db_expr_t addr, bool have_addr, db_expr_t count, const char *modif)
{
	int s;
	
	s = splhigh();

	db_printf("Panic string: %s\n", panicstr);

	(void)splx(s);
}


void
db_show_frame_cmd(db_expr_t addr, bool have_addr, db_expr_t count, const char *modif)
{
	struct trapframe *frame;

	if (!have_addr) {
		db_printf("frame address must be specified\n");
		return;
	}

	frame = (struct trapframe *)addr;

	db_printf("frame address = %08x  ", (u_int)frame);
	db_printf("spsr=%08x\n", frame->tf_spsr);
	db_printf("r0 =%08x r1 =%08x r2 =%08x r3 =%08x\n",
	    frame->tf_r0, frame->tf_r1, frame->tf_r2, frame->tf_r3);
	db_printf("r4 =%08x r5 =%08x r6 =%08x r7 =%08x\n",
	    frame->tf_r4, frame->tf_r5, frame->tf_r6, frame->tf_r7);
	db_printf("r8 =%08x r9 =%08x r10=%08x r11=%08x\n",
	    frame->tf_r8, frame->tf_r9, frame->tf_r10, frame->tf_r11);
	db_printf("r12=%08x r13=%08x r14=%08x r15=%08x\n",
	    frame->tf_r12, frame->tf_usr_sp, frame->tf_usr_lr, frame->tf_pc);
	db_printf("slr=%08x\n", frame->tf_svc_lr);
}
