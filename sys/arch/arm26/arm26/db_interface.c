/*	$NetBSD: db_interface.c,v 1.3.2.2 2000/11/20 20:02:25 bouyer Exp $	*/

/* 
 * Copyright (c) 1996 Scott K. Stevens
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
 *
 *	From: db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>	/* just for boothowto */
#include <sys/exec.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <dev/cons.h>

int db_access_und_sp __P((struct db_variable *, db_expr_t *, int));
int db_access_abt_sp __P((struct db_variable *, db_expr_t *, int));
int db_access_irq_sp __P((struct db_variable *, db_expr_t *, int));
u_int db_fetch_reg __P((int, db_regs_t *));

static int db_validate_address __P((vm_offset_t));
static void db_write_text __P((unsigned char *,	int ch));

struct db_variable db_regs[] = {
	{ "r0", (long *)&DDB_TF->tf_r0, FCN_NULL },
	{ "r1", (long *)&DDB_TF->tf_r1, FCN_NULL },
	{ "r2", (long *)&DDB_TF->tf_r2, FCN_NULL },
	{ "r3", (long *)&DDB_TF->tf_r3, FCN_NULL },
	{ "r4", (long *)&DDB_TF->tf_r4, FCN_NULL },
	{ "r5", (long *)&DDB_TF->tf_r5, FCN_NULL },
	{ "r6", (long *)&DDB_TF->tf_r6, FCN_NULL },
	{ "r7", (long *)&DDB_TF->tf_r7, FCN_NULL },
	{ "r8", (long *)&DDB_TF->tf_r8, FCN_NULL },
	{ "r9", (long *)&DDB_TF->tf_r9, FCN_NULL },
	{ "r10", (long *)&DDB_TF->tf_r10, FCN_NULL },
	{ "r11", (long *)&DDB_TF->tf_r11, FCN_NULL },
	{ "r12", (long *)&DDB_TF->tf_r12, FCN_NULL },
	{ "r13", (long *)&DDB_TF->tf_r13, FCN_NULL },
	{ "r14", (long *)&DDB_TF->tf_r14, FCN_NULL },
	{ "r15", (long *)&DDB_TF->tf_r15, FCN_NULL },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern label_t	*db_recover;

int	db_active = 0;

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(type, regs)
	int type;
	db_regs_t *regs;
{
	int s;

	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
	case -1:		/* keyboard interrupt */
		break;
	default:
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0/*code*/);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	*regs = ddb_regs;

	return 1;
}

static int
db_validate_address(addr)
	vm_offset_t addr;
{

	/* FIXME */
	return 0;
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t	size;
	char	*data;
{
	char	*src;

	src = (char *)addr;
	for (; size > 0; size--) {
		if (db_validate_address((u_int)src)) {
			db_printf("address %p is invalid\n", src);
			return;
		}
		*data++ = *src++;
	}
}

static void
db_write_text(dst, ch)
	unsigned char *dst;
	int ch;
{        

	if (db_validate_address((u_int)dst)) {
		db_printf(" address %p not a valid page\n", dst);
		return;
	}

	*dst = (unsigned char)ch;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t	size;
	char	*data;
{
#if 0
	extern char	_stext_[], _etext[];
#endif
	char	*dst;
	int	loop;

	dst = (char *)addr;
	loop = size;
	while (--loop >= 0) {
#if 0 /* FIXME */
		if ((dst >= _stext_) && (dst < _etext))
#endif
			db_write_text(dst, *data);
#if 0
		else {
			if (db_validate_address((u_int)dst)) {
				db_printf("address %p is invalid\n", dst);
				return;
			}
			*dst = *data;
		}
#endif
		dst++, data++;
	}
}

void db_show_vmstat_cmd __P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_vnode_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_intrchain_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_panic_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_frame_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));

struct db_command arm26_db_command_table[] = {
	{ "frame",	db_show_frame_cmd,	0, NULL },
#if 0
	{ "intrchain",	db_show_intrchain_cmd,	0, NULL },
#endif
	{ "panic",	db_show_panic_cmd,	0, NULL },
	{ "vmstat",	db_show_vmstat_cmd,	0, NULL },
	{ "vnode",	db_show_vnode_cmd,	0, NULL },
	{ NULL, 	NULL, 			0, NULL }
};

#if 0 /* unused? */
int
db_trapper(addr, inst, frame, fault_code)
	u_int		addr;
	u_int		inst;
	trapframe_t	*frame;
	int		fault_code;
{
	if (fault_code == 0) {
		if ((inst & ~0xf0000000) == (BKPT_INST & ~0xf0000000))
			kdb_trap(T_BREAKPOINT, frame);
		else
			kdb_trap(-1, frame);
	} else
		return 1;
	return 0;
}
#endif

extern u_int esym;
extern u_int end;

void
db_machine_init()
{

	db_machine_commands_install(arm26_db_command_table);
}

u_int
db_fetch_reg(reg, db_regs)
	int reg;
	db_regs_t *db_regs;
{

	switch (reg) {
	case 0:
		return (db_regs->ddb_tf.tf_r0);
	case 1:
		return (db_regs->ddb_tf.tf_r1);
	case 2:
		return (db_regs->ddb_tf.tf_r2);
	case 3:
		return (db_regs->ddb_tf.tf_r3);
	case 4:
		return (db_regs->ddb_tf.tf_r4);
	case 5:
		return (db_regs->ddb_tf.tf_r5);
	case 6:
		return (db_regs->ddb_tf.tf_r6);
	case 7:
		return (db_regs->ddb_tf.tf_r7);
	case 8:
		return (db_regs->ddb_tf.tf_r8);
	case 9:
		return (db_regs->ddb_tf.tf_r9);
	case 10:
		return (db_regs->ddb_tf.tf_r10);
	case 11:
		return (db_regs->ddb_tf.tf_r11);
	case 12:
		return (db_regs->ddb_tf.tf_r12);
	case 13:
		return (db_regs->ddb_tf.tf_r13);
	case 14:
		return (db_regs->ddb_tf.tf_r14);
	case 15:
		return (db_regs->ddb_tf.tf_r15);
	default:
		panic("db_fetch_reg: botch");
	}
}

u_int
branch_taken(insn, pc, db_regs)
	u_int insn;
	u_int pc;
	db_regs_t *db_regs;
{
	u_int addr, nregs;

	switch ((insn >> 24) & 0xf) {
	case 0xa:	/* b ... */
	case 0xb:	/* bl ... */
		addr = ((insn << 2) & 0x03ffffff);
		if (addr & 0x02000000)
			addr |= 0xfc000000;
		return (pc + 8 + addr);
	case 0x7:	/* ldr pc, [pc, reg, lsl #2] */
		addr = db_fetch_reg(insn & 0xf, db_regs);
		addr = pc + 8 + (addr << 2);
		db_read_bytes(addr, 4, (char *)&addr);
		return (addr);
	case 0x1:	/* mov pc, reg */
		addr = db_fetch_reg(insn & 0xf, db_regs);
		return (addr);
	case 0x8:	/* ldmxx reg, {..., pc} */
	case 0x9:
		addr = db_fetch_reg((insn >> 16) & 0xf, db_regs);
		nregs = (insn  & 0x5555) + ((insn  >> 1) & 0x5555);
		nregs = (nregs & 0x3333) + ((nregs >> 2) & 0x3333);
		nregs = (nregs + (nregs >> 4)) & 0x0f0f;
		nregs = (nregs + (nregs >> 8)) & 0x001f;
		switch ((insn >> 23) & 0x3) {
		case 0x0:	/* ldmda */
			addr = addr - 0;
			break;
		case 0x1:	/* ldmia */
			addr = addr + 0 + ((nregs - 1) << 2);
			break;
		case 0x2:	/* ldmdb */
			addr = addr - 4;
			break;
		case 0x3:	/* ldmib */
			addr = addr + 4 + ((nregs - 1) << 2);
			break;
		}
		db_read_bytes(addr, 4, (char *)&addr);
		return (addr);
	default:
		panic("branch_taken: botch");
	}
}
