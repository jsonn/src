/*	$NetBSD: db_machdep.h,v 1.7.14.1 1999/12/27 18:33:57 wrstuden Exp $ */

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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_SPARC_DB_MACHDEP_H_
#define	_SPARC_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */


#include <vm/vm.h>
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/reg.h>

/* end of mangling */

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

#if 1
typedef struct {
	struct trapframe64 ddb_tf;
	struct frame64	 ddb_fr;
} db_regs_t;
#else
struct trapregs {
	int64_t	tt;
	int64_t tstate;
	int64_t tpc;
	int64_t	tnpc;
};
typedef struct db_regs {
	struct trapregs dbr_traps[4];
	int		dbr_y;
	char		dbr_tl;
	char		dbr_canrestore;
	char		dbr_cansave;
	char		dbr_cleanwin;
	char		dbr_cwp;
	char		dbr_wstate;
	int64_t		dbr_g[8];
	int64_t		dbr_ag[8];
	int64_t		dbr_ig[8];
	int64_t		dbr_mg[8];
	int64_t		dbr_out[8];
	int64_t		dbr_local[8];
	int64_t		dbr_in[8];
} db_regs_t;
#endif

db_regs_t		ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)
#define	DDB_TF		(&ddb_regs.ddb_tf)
#define	DDB_FR		(&ddb_regs.ddb_fr)

#if defined(lint)
#define	PC_REGS(regs)	((regs)->ddb_tf.tf_pc)
#else
#define	PC_REGS(regs)	((db_addr_t)(regs)->ddb_tf.tf_pc)
#endif
#define	PC_ADVANCE(regs) do {				\
	vaddr_t n = (regs)->ddb_tf.tf_npc;		\
	(regs)->ddb_tf.tf_pc = n;			\
	(regs)->ddb_tf.tf_npc = n + 4;			\
} while(0)

#define	BKPT_INST	0x91d02001	/* breakpoint instruction */
#define	BKPT_SIZE	(4)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	db_clear_single_step(regs)	(void) (0)
#define	db_set_single_step(regs)	(void) (0)

#define	IS_BREAKPOINT_TRAP(type, code)	\
	((type) == T_BREAKPOINT || (type) == T_KGDB_EXEC)
#define IS_WATCHPOINT_TRAP(type, code)	(0)

#define	inst_trap_return(ins)	((ins)&0)
#define	inst_return(ins)	((ins)&0)
#define	inst_call(ins)		((ins)&0)
#define inst_load(ins)		0
#define inst_store(ins)		0

#define DB_MACHINE_COMMANDS

void db_machine_init __P((void));
int kdb_trap __P((int, struct trapframe64 *));

/*
 * We will use elf symbols in DDB when they work.
 */
#if 1
#define	DB_ELF_SYMBOLS
#ifdef __arch64__
#define DB_ELFSIZE	64
#else
#define DB_ELFSIZE	32
#endif
#else
#define DB_AOUT_SYMBOLS
#endif
/*
 * KGDB definitions
 */
typedef u_long		kgdb_reg_t;
#define KGDB_NUMREGS	72
#define KGDB_BUFLEN	1024

#endif	/* _SPARC_DB_MACHDEP_H_ */
