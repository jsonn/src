/*	$NetBSD: db_interface.c,v 1.13.8.1 1997/08/23 07:07:38 thorpej Exp $	*/

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
 *	From: db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>	/* just for boothowto */
#include <sys/exec.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/katelib.h>
#include <machine/pte.h>
#include <machine/undefined.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <dev/cons.h>

static int nil;

int db_access_svc_sp __P((struct db_variable *, db_expr_t *, int));

struct db_variable db_regs[] = {
	{ "spsr", (long *)&DDB_TF->tf_spsr, FCN_NULL, },
	{ "r0", (long *)&DDB_TF->tf_r0, FCN_NULL, },
	{ "r1", (long *)&DDB_TF->tf_r1, FCN_NULL, },
	{ "r2", (long *)&DDB_TF->tf_r2, FCN_NULL, },
	{ "r3", (long *)&DDB_TF->tf_r3, FCN_NULL, },
	{ "r4", (long *)&DDB_TF->tf_r4, FCN_NULL, },
	{ "r5", (long *)&DDB_TF->tf_r5, FCN_NULL, },
	{ "r6", (long *)&DDB_TF->tf_r6, FCN_NULL, },
	{ "r7", (long *)&DDB_TF->tf_r7, FCN_NULL, },
	{ "r8", (long *)&DDB_TF->tf_r8, FCN_NULL, },
	{ "r9", (long *)&DDB_TF->tf_r9, FCN_NULL, },
	{ "r10", (long *)&DDB_TF->tf_r10, FCN_NULL, },
	{ "r11", (long *)&DDB_TF->tf_r11, FCN_NULL, },
	{ "r12", (long *)&DDB_TF->tf_r12, FCN_NULL, },
	{ "usr_sp", (long *)&DDB_TF->tf_usr_sp, FCN_NULL, },
	{ "svc_sp", (long *)&nil, db_access_svc_sp, },
	{ "usr_lr", (long *)&DDB_TF->tf_usr_lr, FCN_NULL, },
	{ "svc_lr", (long *)&DDB_TF->tf_svc_lr, FCN_NULL, },
	{ "pc", (long *)&DDB_TF->tf_pc, FCN_NULL, },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern label_t	*db_recover;

int	db_active = 0;

int db_access_svc_sp(vp, valp, rw)
	struct db_variable *vp;
	db_expr_t *valp;
	int rw;
{
	if(rw == DB_VAR_GET)
		*valp = get_stackptr(PSR_SVC32_MODE);
	return(0);
}


/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(type, tf)
	int	type;
	register struct trapframe *tf;
{

#if 0
	fb_unblank();
#endif

	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
	case -1:		/* keyboard interrupt */
		break;
	default:
		db_printf("kernel: trap");
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs.ddb_tf = *tf;
	ddb_regs.ddb_tf.tf_pc -= 4;

	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0/*code*/);
	cnpollc(FALSE);
	db_active--;

	*tf = ddb_regs.ddb_tf;

	return(1);
}


/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kbd_trap(tf)
	struct trapframe *tf;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
		printf("\n\nkernel: keyboard interrupt\n");
		kdb_trap(-1, tf);
	}
}


/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;

	src = (char *)addr;
	while (--size >= 0)
		*data++ = *src++;
}

#define	splpmap() splimp()

static void
db_write_text(dst, ch)
	unsigned char *dst;
	int ch;
{        
	pt_entry_t *ptep, pte, pteo;
	int s;
	vm_offset_t va;

	s = splpmap();
	va = (unsigned long)dst & (~PGOFSET);
	ptep = vtopte(va);

	if ((*ptep & L2_MASK) == L2_INVAL) { 
		db_printf(" address 0x%08x not a valid page\n", (u_int)dst);
		splx(s);
		return;
	}

	/*
	 * This code should purge the TLB and sync a single cache line
	 */

	pteo = ReadWord(ptep);
	pte = pteo | PT_AP(AP_KRW);
	WriteWord(ptep, pte);
	tlb_flush();

	*dst = (unsigned char)ch;

	/* make sure the caches and memory are in sync */
	cpu_cache_syncI_rng((u_int)dst, 4);

	WriteWord(ptep, pteo);
	tlb_flush();
	splx(s);
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	extern char	etext[];
	register char	*dst;
	register int	loop;

	dst = (char *)addr;
	loop = size;
	while (--loop >= 0) {
		if ((dst >= (char *)VM_MIN_KERNEL_ADDRESS) && (dst < etext))
			db_write_text(dst, *data);
		else
			*dst = *data;
		dst++, data++;
	}
	/* make sure the caches and memory are in sync */
	cpu_cache_syncI_rng(addr, size);
}

void
Debugger()
{
	asm(".word	0xe7ffffff");
}

void db_show_vmstat_cmd __P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_fs_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_vnode_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_intrchain_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_panic_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));
void db_show_frame_cmd	__P((db_expr_t addr, int have_addr, db_expr_t count, char *modif));

struct db_command arm32_db_command_table[] = {
	{ "vmstat",	db_show_vmstat_cmd,	0, NULL },
	{ "fs",		db_show_fs_cmd,		0, NULL },
	{ "vnode",	db_show_vnode_cmd,	0, NULL },
	{ "intrchain",	db_show_intrchain_cmd,	0, NULL },
	{ "panic",	db_show_panic_cmd,	0, NULL },
	{ "frame",	db_show_frame_cmd,	0, NULL },
	{ NULL, 	NULL, 			0, NULL }
};

int
db_trapper(addr, inst, frame, fault_code)
	u_int		addr;
	u_int		inst;
	trapframe_t	*frame;
	int		fault_code;
{
	if (fault_code == 0) {
		if (inst == BKPT_INST)
			kdb_trap(1, frame);
		else
			panic("Undefined instruction 0x%08x @ 0x%08x in kernel\n", inst, addr);
	} else
		return(1);
	return(0);
}

extern u_int esym;
extern u_int end;

void
db_machine_init()
{
	struct exec *kernexec = (struct exec *)KERNEL_BASE;
	int len;

/*
 * The boot loader currently loads the kernel with the a.out header still attached.
 */

	if (kernexec->a_syms == 0) {
		printf("[No symbol table]\n");
	} else {
		esym = (int)&end + kernexec->a_syms + sizeof(int);
		len = *((u_int *)esym);
		esym += (len + (sizeof(u_int) - 1)) & ~(sizeof(u_int) - 1);
	}

	install_coproc_handler(0, db_trapper);
	db_machine_commands_install(arm32_db_command_table);
}


u_int
branch_taken(insn, pc, db_regs)
	u_int insn;
	u_int pc;
	db_regs_t *db_regs;
{
	int branch;

	branch = ((insn << 2) & 0x03ffffff);
	if (branch & 0x02000000)
		branch |= 0xfc000000;
	return(pc + 8 + branch);
}
