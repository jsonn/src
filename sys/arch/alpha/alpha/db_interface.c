/* $NetBSD: db_interface.c,v 1.1.2.2 1997/09/06 17:59:40 thorpej Exp $ */

/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Parts of this file are derived from Mach 3:
 *
 *	File: alpha_instruction.c
 *	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	6/92
 */

/*
 * Interface to DDB.
 *
 * Modified for NetBSD/alpha by:
 *
 *	Christopher G. Demetriou, Carnegie Mellon University
 *
 *	Jason R. Thorpe, Numerical Aerospace Simulation Facility,
 *	NASA Ames Research Center
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: db_interface.c,v 1.1.2.2 1997/09/06 17:59:40 thorpej Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <machine/prom.h>

#include <alpha/alpha/db_instruction.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_interface.h>


extern label_t	*db_recover;

#if 0
extern char *trap_type[];
extern int trap_types;
#endif

int	db_active = 0;

void	ddbprinttrap __P((unsigned long, unsigned long, unsigned long,
	    unsigned long));

void	db_mach_halt __P((db_expr_t, int, db_expr_t, char *));
void	db_mach_reboot __P((db_expr_t, int, db_expr_t, char *));

struct db_command db_machine_cmds[] = {
	{ "halt",	db_mach_halt,	0,	0 },
	{ "reboot",	db_mach_reboot,	0,	0 },
	{ (char *)0, },
};

struct db_variable db_regs[] = {
	{	"v0",	&ddb_regs.tf_regs[FRAME_V0],	FCN_NULL	},
	{	"t0",	&ddb_regs.tf_regs[FRAME_T0],	FCN_NULL	},
	{	"t1",	&ddb_regs.tf_regs[FRAME_T1],	FCN_NULL	},
	{	"t2",	&ddb_regs.tf_regs[FRAME_T2],	FCN_NULL	},
	{	"t3",	&ddb_regs.tf_regs[FRAME_T3],	FCN_NULL	},
	{	"t4",	&ddb_regs.tf_regs[FRAME_T4],	FCN_NULL	},
	{	"t5",	&ddb_regs.tf_regs[FRAME_T5],	FCN_NULL	},
	{	"t6",	&ddb_regs.tf_regs[FRAME_T6],	FCN_NULL	},
	{	"t7",	&ddb_regs.tf_regs[FRAME_T7],	FCN_NULL	},
	{	"s0",	&ddb_regs.tf_regs[FRAME_S0],	FCN_NULL	},
	{	"s1",	&ddb_regs.tf_regs[FRAME_S1],	FCN_NULL	},
	{	"s2",	&ddb_regs.tf_regs[FRAME_S2],	FCN_NULL	},
	{	"s3",	&ddb_regs.tf_regs[FRAME_S3],	FCN_NULL	},
	{	"s4",	&ddb_regs.tf_regs[FRAME_S4],	FCN_NULL	},
	{	"s5",	&ddb_regs.tf_regs[FRAME_S5],	FCN_NULL	},
	{	"s6",	&ddb_regs.tf_regs[FRAME_S6],	FCN_NULL	},
	{	"a0",	&ddb_regs.tf_regs[FRAME_A0],	FCN_NULL	},
	{	"a1",	&ddb_regs.tf_regs[FRAME_A1],	FCN_NULL	},
	{	"a2",	&ddb_regs.tf_regs[FRAME_A2],	FCN_NULL	},
	{	"a3",	&ddb_regs.tf_regs[FRAME_A3],	FCN_NULL	},
	{	"a4",	&ddb_regs.tf_regs[FRAME_A4],	FCN_NULL	},
	{	"a5",	&ddb_regs.tf_regs[FRAME_A5],	FCN_NULL	},
	{	"t8",	&ddb_regs.tf_regs[FRAME_T8],	FCN_NULL	},
	{	"t9",	&ddb_regs.tf_regs[FRAME_T9],	FCN_NULL	},
	{	"t10",	&ddb_regs.tf_regs[FRAME_T10],	FCN_NULL	},
	{	"t11",	&ddb_regs.tf_regs[FRAME_T11],	FCN_NULL	},
	{	"ra",	&ddb_regs.tf_regs[FRAME_RA],	FCN_NULL	},
	{	"t12",	&ddb_regs.tf_regs[FRAME_T12],	FCN_NULL	},
	{	"at",	&ddb_regs.tf_regs[FRAME_AT],	FCN_NULL	},
	{	"gp",	&ddb_regs.tf_regs[FRAME_GP],	FCN_NULL	},
	{	"sp",	&ddb_regs.tf_regs[FRAME_SP],	FCN_NULL	},
	{	"pc",	&ddb_regs.tf_regs[FRAME_PC],	FCN_NULL	},
	{	"ps",	&ddb_regs.tf_regs[FRAME_PS],	FCN_NULL	},
	{	"ai",	&ddb_regs.tf_regs[FRAME_T11],	FCN_NULL	},
	{	"pv",	&ddb_regs.tf_regs[FRAME_T12],	FCN_NULL	},
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Print trap reason.
 */
void
ddbprinttrap(a0, a1, a2, entry)
	unsigned long a0, a1, a2, entry;
{

	/* XXX Implement. */

	printf("ddbprinttrap(0x%lx, 0x%lx, 0x%lx, 0x%lx)\n", a0, a1, a2,
	    entry);
}

/*
 *  ddb_trap - field a kernel trap
 */
int
ddb_trap(a0, a1, a2, entry, regs)
	unsigned long a0, a1, a2, entry;
	db_regs_t *regs;
{
	int s;

	/*
	 * Don't bother checking for usermode, since a benign entry
	 * by the kernel (call to Debugger() or a breakpoint) has
	 * already checked for usermode.  If neither of those
	 * conditions exist, something Bad has happened.
	 */

	if (entry != ALPHA_KENTRY_IF ||
	    (a0 != ALPHA_IF_CODE_BUGCHK && a0 != ALPHA_IF_CODE_BPT)) {
		db_printf("ddbprinttrap from 0x%lx\n",	/* XXX */
		    regs->tf_regs[FRAME_PC]);
		ddbprinttrap(a0, a1, a2, entry);
		if (db_recover != 0) {
			/*
			 * XXX Sould longjump back into command loop!
			 */
			db_printf("Faulted in DDB; continuing...\n");
			alpha_pal_halt();		/* XXX */
			db_error("Faulted in DDB; continuing...\n");
			/* NOTREACHED */
		}

		/*
		 * Tell caller "We did NOT handle the trap."
		 * Caller should panic, or whatever.
		 */
		return (0);
	}

	/*
	 * XXX Should switch to DDB's own stack, here.
	 */

	ddb_regs = *regs;

	s = splhigh();

	db_active++;
	cnpollc(TRUE);		/* Set polling mode, unblank video */

	db_trap(entry, a0);	/* Where the work happens */

	cnpollc(FALSE);		/* Resume interrupt mode */
	db_active--;

	splx(s);

	*regs = ddb_regs;

	/*
	 * Tell caller "We HAVE handled the trap."
	 */
	return (1);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*src;

	src = (char *)addr;
	while (size-- > 0)
		*data++ = *src++;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*dst;

	dst = (char *)addr;
	while (size-- > 0)
		*dst++ = *data++;
	alpha_pal_imb();
}

void
Debugger()
{
	asm("call_pal 0x81");		/* XXX bugchk */
}

/*
 * This is called before ddb_init() to install the
 * machine-specific command table.  (see machdep.c)
 */
void
db_machine_init()
{

	db_machine_commands_install(db_machine_cmds);
}

/*
 * Alpha-specific ddb commands:
 *
 *	halt		set halt bit in rpb and halt
 *	reboot		set reboot bit in rpb and halt
 */

void
db_mach_halt(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{

	prom_halt(1);
}

void
db_mach_reboot(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{

	prom_halt(0);
}

/*
 * Map Alpha register numbers to trapframe/db_regs_t offsets.
 */
static int reg_to_frame[32] = {
	FRAME_V0,
	FRAME_T0,
	FRAME_T1,
	FRAME_T2,
	FRAME_T3,
	FRAME_T4,
	FRAME_T5,
	FRAME_T6,
	FRAME_T7,

	FRAME_S0,
	FRAME_S1,
	FRAME_S2,
	FRAME_S3,
	FRAME_S4,
	FRAME_S5,
	FRAME_S6,

	FRAME_A0,
	FRAME_A1,
	FRAME_A2,
	FRAME_A3,
	FRAME_A4,
	FRAME_A5,

	FRAME_T8,
	FRAME_T9,
	FRAME_T10,
	FRAME_T11,
	FRAME_RA,
	FRAME_T12,
	FRAME_AT,
	FRAME_GP,
	FRAME_SP,
	-1,		/* zero */
};

u_long
db_register_value(regs, regno)
	db_regs_t *regs;
	int regno;
{

	if (regno > 31 || regno < 0) {
		db_printf(" **** STRANGE REGISTER NUMBER %d **** ", regno);
		return (0);
	}

	if (regno == 31)
		return (0);

	return (regs->tf_regs[reg_to_frame[regno]]);
}

/*
 * Support functions for software single-step.
 */

boolean_t
db_inst_call(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.branch_format.opcode == op_bsr) ||
	    ((insn.jump_format.opcode == op_j) &&
	     (insn.jump_format.action & 1)));
}

boolean_t
db_inst_return(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.jump_format.opcode == op_j) &&
	    (insn.jump_format.action == op_ret));
}

boolean_t
db_inst_trap_return(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.pal_format.opcode == op_pal) &&
	    (insn.pal_format.function == op_rei));
}

boolean_t
db_inst_branch(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	case op_j:
	case op_br:
	case op_fbeq:
	case op_fblt:
	case op_fble:
	case op_fbne:
	case op_fbge:
	case op_fbgt:
	case op_blbc:
	case op_beq:
	case op_blt:
	case op_ble:
	case op_blbs:
	case op_bne:
	case op_bge:
	case op_bgt:
		return (TRUE);
	}

	return (FALSE);
}

boolean_t
db_inst_unconditional_flow_transfer(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	case op_j:
	case op_br:
		return (TRUE);

	case op_pal:
		return ((insn.pal_format.function == op_rei) ||
		    (insn.pal_format.function == op_chmk));
	}

	return (FALSE);
}

#if 0
boolean_t
db_inst_spill(ins, regn)
	int ins, regn;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.mem_format.opcode == op_stq) &&
	    (insn.mem_format.rd == regn));
}
#endif

boolean_t
db_inst_load(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	
	/* Loads. */
	if (insn.mem_format.opcode == op_ldq_u)
		return (TRUE);
	if ((insn.mem_format.opcode >= op_ldf) &&
	    (insn.mem_format.opcode <= op_ldt))
		return (TRUE);
	if ((insn.mem_format.opcode >= op_ldl) &&
	    (insn.mem_format.opcode <= op_ldq_l))
		return (TRUE);

	/* Prefetches. */
	if (insn.mem_format.opcode == op_special) {
		/* Note: MB is treated as a store. */
		if ((insn.mem_format.displacement == (short)op_fetch) ||
		    (insn.mem_format.displacement == (short)op_fetch_m))
			return (TRUE);
	}

	return (FALSE);
}

boolean_t
db_inst_store(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;

	/* Stores. */
	if (insn.mem_format.opcode == op_stq_u)
		return (TRUE);
	if ((insn.mem_format.opcode >= op_stf) &&
	    (insn.mem_format.opcode <= op_stt))
		return (TRUE);
	if ((insn.mem_format.opcode >= op_stl) &&
	    (insn.mem_format.opcode <= op_stq_c))
		return (TRUE);

	/* Barriers. */
	if (insn.mem_format.opcode == op_special) {
		if (insn.mem_format.displacement == op_mb)
			return (TRUE);
	}

	return (FALSE);
}

db_addr_t
db_branch_taken(ins, pc, regs)
	int ins;
	db_addr_t pc;
	db_regs_t *regs;
{
	alpha_instruction insn;
	db_addr_t newpc;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	/*
	 * Jump format: target PC is (contents of instruction's "RB") & ~3.
	 */
	case op_j:
		newpc = db_register_value(regs, insn.jump_format.rs) & ~3;
		break;

	/*
	 * Branch format: target PC is
	 *	(new PC) + (4 * sign-ext(displacement)).
	 */
	case op_br:
	case op_fbeq:
	case op_fblt:
	case op_fble:
	case op_bsr:
	case op_fbne:
	case op_fbge:
	case op_fbgt:
	case op_blbc:
	case op_beq:
	case op_blt:
	case op_ble:
	case op_blbs:
	case op_bne:
	case op_bge:
	case op_bgt:
		newpc = (insn.branch_format.displacement << 2) + (pc + 4);
		break;

	default:
		printf("DDB: db_inst_branch_taken on non-branch!\n");
		newpc = pc;	/* XXX */
	}

	return (newpc);
}
