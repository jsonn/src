/* $NetBSD: db_machdep.h,v 1.2.2.4 2000/12/13 15:49:19 bouyer Exp $ */

/*
 * Copyright (c) 1996 Scott K Stevens
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_ARM26_DB_MACHDEP_H_
#define	_ARM26_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include <uvm/uvm_extern.h>
#include <machine/armreg.h>
#include <machine/frame.h>

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct {
	trapframe_t	ddb_tf;
} db_regs_t;

db_regs_t		ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)
#define	DDB_TF		(&ddb_regs.ddb_tf)

#define	PC_REGS(regs)	((db_addr_t)(regs)->ddb_tf.tf_r15 & R15_PC)
#define PC_ADVANCE(regs) ((regs)->ddb_tf.tf_r15 += 4)

#define	BKPT_INST	(0xe7ffffff)	/* breakpoint instruction */
#define	BKPT_SIZE	(INSN_SIZE)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

/*#define FIXUP_PC_AFTER_BREAK(regs)	((regs)->ddb_tf.tf_pc -= BKPT_SIZE)*/

#define T_BREAKPOINT			(1)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BREAKPOINT)
#define IS_WATCHPOINT_TRAP(type, code)	(0)

#define	inst_trap_return(ins)	((ins)&0)
#define	inst_return(ins)	((ins)&0)
#define	inst_call(ins)		(((ins) & 0x0f000000) == 0x0b000000)
#define	inst_branch(ins)	(((ins) & 0x0f000000) == 0x0a000000)
#define inst_load(ins)		0
#define inst_store(ins)		0
#define inst_unconditional_flow_transfer(ins)	(((ins) & 0xf0000000) == 0xe0000000)

#define getreg_val			(0)
#define next_instr_address(pc, bd)	(pc + INSN_SIZE)

#define DB_MACHINE_COMMANDS

#define SOFTWARE_SSTEP

u_int branch_taken __P((u_int insn, u_int pc, db_regs_t *db_regs));

/*
 * We use ELF symbols in DDB.
 */
#define DB_ELF_SYMBOLS
#define DB_ELFSIZE 32

/* Entry point from undefined instruction handler */
int kdb_trap __P((int, db_regs_t *));

#endif	/* _ARM26_DB_MACHDEP_H_ */
