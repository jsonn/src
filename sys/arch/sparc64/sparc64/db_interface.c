/*	$NetBSD: db_interface.c,v 1.64.2.1 2002/07/15 01:21:50 gehenna Exp $ */

/*
 * Copyright (c) 1996-2002 Eduardo Horvath.  All rights reserved.
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
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>

#include <machine/instr.h>
#include <machine/cpu.h>
#include <machine/openfirm.h>
#include <machine/ctlreg.h>
#include <machine/pmap.h>

#include "fb.h"
#include "esp_sbus.h"

extern void OF_enter __P((void));

extern struct traptrace {
	unsigned short tl:3,	/* Trap level */
		ns:4,		/* PCB nsaved */
		tt:9;		/* Trap type */
	unsigned short pid;	/* PID */
	u_int tstate;		/* tstate */
	u_int tsp;		/* sp */
	u_int tpc;		/* pc */
	u_int tfault;		/* MMU tag access */
} trap_trace[], trap_trace_end[];

static int nil;

static int
db__char_value(const struct db_variable *var, db_expr_t *expr, int mode)
{

	switch (mode) {
	case DB_VAR_SET:
		*var->valuep = *(char *)expr;
		break;
	case DB_VAR_GET:
		*expr = *(char *)var->valuep;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("db__char_value: mode %d\n", mode);
		break;
#endif
	}

	return 0;
}

#ifdef notdef_yet
static int
db__short_value(const struct db_variable *var, db_expr_t *expr, int mode)
{

	switch (mode) {
	case DB_VAR_SET:
		*var->valuep = *(short *)expr;
		break;
	case DB_VAR_GET:
		*expr = *(short *)var->valuep;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("db__short_value: mode %d\n", mode);
		break;
#endif
	}

	return 0;
}
#endif

static int
db__int_value(const struct db_variable *var, db_expr_t *expr, int mode)
{

	switch (mode) {
	case DB_VAR_SET:
		*var->valuep = *(int *)expr;
		break;
	case DB_VAR_GET:
		*expr = *(int *)var->valuep;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("db__int_value: mode %d\n", mode);
		break;
#endif
	}

	return 0;
}

const struct db_variable db_regs[] = {
	{ "tstate", (long *)&DDB_TF->tf_tstate, FCN_NULL, },
	{ "pc", (long *)&DDB_TF->tf_pc, FCN_NULL, },
	{ "npc", (long *)&DDB_TF->tf_npc, FCN_NULL, },
	{ "ipl", (long *)&DDB_TF->tf_oldpil, db__char_value, },
	{ "y", (long *)&DDB_TF->tf_y, db__int_value, },
	{ "g0", (long *)&nil, FCN_NULL, },
	{ "g1", (long *)&DDB_TF->tf_global[1], FCN_NULL, },
	{ "g2", (long *)&DDB_TF->tf_global[2], FCN_NULL, },
	{ "g3", (long *)&DDB_TF->tf_global[3], FCN_NULL, },
	{ "g4", (long *)&DDB_TF->tf_global[4], FCN_NULL, },
	{ "g5", (long *)&DDB_TF->tf_global[5], FCN_NULL, },
	{ "g6", (long *)&DDB_TF->tf_global[6], FCN_NULL, },
	{ "g7", (long *)&DDB_TF->tf_global[7], FCN_NULL, },
	{ "o0", (long *)&DDB_TF->tf_out[0], FCN_NULL, },
	{ "o1", (long *)&DDB_TF->tf_out[1], FCN_NULL, },
	{ "o2", (long *)&DDB_TF->tf_out[2], FCN_NULL, },
	{ "o3", (long *)&DDB_TF->tf_out[3], FCN_NULL, },
	{ "o4", (long *)&DDB_TF->tf_out[4], FCN_NULL, },
	{ "o5", (long *)&DDB_TF->tf_out[5], FCN_NULL, },
	{ "o6", (long *)&DDB_TF->tf_out[6], FCN_NULL, },
	{ "o7", (long *)&DDB_TF->tf_out[7], FCN_NULL, },
	{ "l0", (long *)&DDB_FR->fr_local[0], FCN_NULL, },
	{ "l1", (long *)&DDB_FR->fr_local[1], FCN_NULL, },
	{ "l2", (long *)&DDB_FR->fr_local[2], FCN_NULL, },
	{ "l3", (long *)&DDB_FR->fr_local[3], FCN_NULL, },
	{ "l4", (long *)&DDB_FR->fr_local[4], FCN_NULL, },
	{ "l5", (long *)&DDB_FR->fr_local[5], FCN_NULL, },
	{ "l6", (long *)&DDB_FR->fr_local[6], FCN_NULL, },
	{ "l7", (long *)&DDB_FR->fr_local[7], FCN_NULL, },
	{ "i0", (long *)&DDB_FR->fr_arg[0], FCN_NULL, },
	{ "i1", (long *)&DDB_FR->fr_arg[1], FCN_NULL, },
	{ "i2", (long *)&DDB_FR->fr_arg[2], FCN_NULL, },
	{ "i3", (long *)&DDB_FR->fr_arg[3], FCN_NULL, },
	{ "i4", (long *)&DDB_FR->fr_arg[4], FCN_NULL, },
	{ "i5", (long *)&DDB_FR->fr_arg[5], FCN_NULL, },
	{ "i6", (long *)&DDB_FR->fr_arg[6], FCN_NULL, },
	{ "i7", (long *)&DDB_FR->fr_arg[7], FCN_NULL, },
	{ "f0", (long *)&DDB_FP->fs_regs[0], FCN_NULL, },
	{ "f2", (long *)&DDB_FP->fs_regs[2], FCN_NULL, },
	{ "f4", (long *)&DDB_FP->fs_regs[4], FCN_NULL, },
	{ "f6", (long *)&DDB_FP->fs_regs[6], FCN_NULL, },
	{ "f8", (long *)&DDB_FP->fs_regs[8], FCN_NULL, },
	{ "f10", (long *)&DDB_FP->fs_regs[10], FCN_NULL, },
	{ "f12", (long *)&DDB_FP->fs_regs[12], FCN_NULL, },
	{ "f14", (long *)&DDB_FP->fs_regs[14], FCN_NULL, },
	{ "f16", (long *)&DDB_FP->fs_regs[16], FCN_NULL, },
	{ "f18", (long *)&DDB_FP->fs_regs[18], FCN_NULL, },
	{ "f20", (long *)&DDB_FP->fs_regs[20], FCN_NULL, },
	{ "f22", (long *)&DDB_FP->fs_regs[22], FCN_NULL, },
	{ "f24", (long *)&DDB_FP->fs_regs[24], FCN_NULL, },
	{ "f26", (long *)&DDB_FP->fs_regs[26], FCN_NULL, },
	{ "f28", (long *)&DDB_FP->fs_regs[28], FCN_NULL, },
	{ "f30", (long *)&DDB_FP->fs_regs[30], FCN_NULL, },
	{ "f32", (long *)&DDB_FP->fs_regs[32], FCN_NULL, },
	{ "f34", (long *)&DDB_FP->fs_regs[34], FCN_NULL, },
	{ "f36", (long *)&DDB_FP->fs_regs[36], FCN_NULL, },
	{ "f38", (long *)&DDB_FP->fs_regs[38], FCN_NULL, },
	{ "f40", (long *)&DDB_FP->fs_regs[40], FCN_NULL, },
	{ "f42", (long *)&DDB_FP->fs_regs[42], FCN_NULL, },
	{ "f44", (long *)&DDB_FP->fs_regs[44], FCN_NULL, },
	{ "f46", (long *)&DDB_FP->fs_regs[46], FCN_NULL, },
	{ "f48", (long *)&DDB_FP->fs_regs[48], FCN_NULL, },
	{ "f50", (long *)&DDB_FP->fs_regs[50], FCN_NULL, },
	{ "f52", (long *)&DDB_FP->fs_regs[52], FCN_NULL, },
	{ "f54", (long *)&DDB_FP->fs_regs[54], FCN_NULL, },
	{ "f56", (long *)&DDB_FP->fs_regs[56], FCN_NULL, },
	{ "f58", (long *)&DDB_FP->fs_regs[58], FCN_NULL, },
	{ "f60", (long *)&DDB_FP->fs_regs[60], FCN_NULL, },
	{ "f62", (long *)&DDB_FP->fs_regs[62], FCN_NULL, },
	{ "fsr", (long *)&DDB_FP->fs_fsr, FCN_NULL, },
	{ "gsr", (long *)&DDB_FP->fs_gsr, FCN_NULL, },

};
const struct db_variable * const db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

int	db_active = 0;

extern char *trap_type[];

void kdb_kbd_trap __P((struct trapframe64 *));
void db_prom_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_proc_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_ctx_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_dump_window __P((db_expr_t, int, db_expr_t, char *));
void db_dump_stack __P((db_expr_t, int, db_expr_t, char *));
void db_dump_trap __P((db_expr_t, int, db_expr_t, char *));
void db_dump_fpstate __P((db_expr_t, int, db_expr_t, char *));
void db_dump_ts __P((db_expr_t, int, db_expr_t, char *));
void db_dump_pcb __P((db_expr_t, int, db_expr_t, char *));
void db_dump_pv __P((db_expr_t, int, db_expr_t, char *));
void db_setpcb __P((db_expr_t, int, db_expr_t, char *));
void db_dump_dtlb __P((db_expr_t, int, db_expr_t, char *));
void db_dump_dtsb __P((db_expr_t, int, db_expr_t, char *));
void db_pmap_kernel __P((db_expr_t, int, db_expr_t, char *));
void db_pload_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_pmap_cmd __P((db_expr_t, int, db_expr_t, char *));
void db_lock __P((db_expr_t, int, db_expr_t, char *));
void db_traptrace __P((db_expr_t, int, db_expr_t, char *));
void db_dump_buf __P((db_expr_t, int, db_expr_t, char *));
void db_dump_espcmd __P((db_expr_t, int, db_expr_t, char *));
void db_watch __P((db_expr_t, int, db_expr_t, char *));
void db_pm_extract __P((db_expr_t, int, db_expr_t, char *));

static void db_dump_pmap __P((struct pmap*));
static void db_print_trace_entry __P((struct traptrace *, int));


/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kbd_trap(tf)
	struct trapframe64 *tf;
{
	if (db_active == 0 /* && (boothowto & RB_KDB) */) {
		printf("\n\nkernel: keyboard interrupt tf=%p\n", tf);
		kdb_trap(-1, tf);
	}
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(type, tf)
	int	type;
	register struct trapframe64 *tf;
{
	int s, tl;
	struct trapstate *ts = &ddb_regs.ddb_ts[0];
	extern int savetstate(struct trapstate *ts);
	extern void restoretstate(int tl, struct trapstate *ts);
	extern int trap_trace_dis;
	extern int doing_shutdown;

	trap_trace_dis++;
	doing_shutdown++;
#if NFB > 0
	fb_unblank();
#endif
	switch (type) {
	case T_BREAKPOINT:	/* breakpoint */
		printf("kdb breakpoint at %llx\n",
		    (unsigned long long)tf->tf_pc);
		break;
	case -1:		/* keyboard interrupt */
		printf("kdb tf=%p\n", tf);
		break;
	default:
		printf("kernel trap %x: %s\n", type, trap_type[type & 0x1ff]);
		if (db_recover != 0) {
			OF_enter();
			db_error("Faulted in DDB; continuing...\n");
			OF_enter();
			/*NOTREACHED*/
		}
		db_recover = (label_t *)1;
	}

	/* Should switch to kdb`s own stack here. */
	write_all_windows();

	ddb_regs.ddb_tf = *tf;
	if (fpproc) {
		savefpstate(fpproc->p_md.md_fpstate);
		ddb_regs.ddb_fpstate = *fpproc->p_md.md_fpstate;
		loadfpstate(fpproc->p_md.md_fpstate);
	}
	/* We should do a proper copyin and xlate 64-bit stack frames, but... */
/*	if (tf->tf_tstate & TSTATE_PRIV) { */
	
#if 0
	/* make sure this is not causing ddb problems. */
	if (tf->tf_out[6] & 1) {
		if ((unsigned)(tf->tf_out[6] + BIAS) > (unsigned)KERNBASE)
			ddb_regs.ddb_fr = *(struct frame64 *)(tf->tf_out[6] + BIAS);
		else
			copyin((caddr_t)(tf->tf_out[6] + BIAS), &ddb_regs.ddb_fr, sizeof(struct frame64));
	} else {
		struct frame32 tfr;
		
		/* First get a local copy of the frame32 */
		if ((unsigned)(tf->tf_out[6]) > (unsigned)KERNBASE)
			tfr = *(struct frame32 *)tf->tf_out[6];
		else
			copyin((caddr_t)(tf->tf_out[6]), &tfr, sizeof(struct frame32));
		/* Now copy each field from the 32-bit value to the 64-bit value */
		for (i=0; i<8; i++)
			ddb_regs.ddb_fr.fr_local[i] = tfr.fr_local[i];
		for (i=0; i<6; i++)
			ddb_regs.ddb_fr.fr_arg[i] = tfr.fr_arg[i];
		ddb_regs.ddb_fr.fr_fp = (long)tfr.fr_fp;
		ddb_regs.ddb_fr.fr_pc = tfr.fr_pc;
	}
#endif

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	/* Need to do spl stuff till cnpollc works */
	tl = ddb_regs.ddb_tl = savetstate(ts);
	db_dump_ts(0, 0, 0, 0);
	db_trap(type, 0/*code*/);
	restoretstate(tl,ts);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	if (fpproc) {	
		*fpproc->p_md.md_fpstate = ddb_regs.ddb_fpstate;
		loadfpstate(fpproc->p_md.md_fpstate);
	}
#if 0
	/* We will not alter the machine's running state until we get everything else working */
	*(struct frame *)tf->tf_out[6] = ddb_regs.ddb_fr;
#endif
	*tf = ddb_regs.ddb_tf;
	trap_trace_dis--;
	doing_shutdown--;

	return (1);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vaddr_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*src;

	src = (char *)addr;
	while (size-- > 0) {
		if (src >= (char *)VM_MIN_KERNEL_ADDRESS)
			*data++ = probeget((paddr_t)(u_long)src++, ASI_P, 1);
		else
			*data++ = fubyte(src++);
	}
}


/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vaddr_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*dst;
	extern vaddr_t ktext;
	extern paddr_t ktextp;

	dst = (char *)addr;
	while (size-- > 0) {
		if ((dst >= (char *)VM_MIN_KERNEL_ADDRESS+0x400000))
			*dst = *data;
		else if ((dst >= (char *)VM_MIN_KERNEL_ADDRESS) &&
			 (dst < (char *)VM_MIN_KERNEL_ADDRESS+0x400000))
			/* Read Only mapping -- need to do a bypass access */
			stba((u_long)dst - ktext + ktextp, ASI_PHYS_CACHED, *data);
		else
			subyte(dst, *data);
		dst++, data++;
	}

}

void
Debugger()
{
	/* We use the breakpoint to trap into DDB */
	asm("ta 1; nop");
}

void
db_prom_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	OF_enter();
}

void
db_dump_dtlb(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	extern void print_dtlb __P((void));

	if (have_addr) {
		int i;
		int64_t* p = (int64_t*)addr;
		static int64_t buf[128];
		extern void dump_dtlb(int64_t *);
		
		dump_dtlb(buf);
		p = buf;
		for (i=0; i<64;) {
#ifdef __arch64__
			db_printf("%2d:%16.16lx %16.16lx ", i++, p[0], p[1]);
			p += 2;
			db_printf("%2d:%16.16lx %16.16lx\n", i++, p[0], p[1]);
			p += 2;
#else
			db_printf("%2d:%16.16qx %16.16qx ", i++, p[0], p[1]);
			p += 2;
			db_printf("%2d:%16.16qx %16.16qx\n", i++, p[0], p[1]);
			p += 2;
#endif
		}
	} else {
#ifdef DEBUG
		print_dtlb();
#endif
	}
}

void
db_pload_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	static paddr_t oldaddr = -1;
	int asi = ASI_PHYS_CACHED;

	if (!have_addr) {
		addr = oldaddr;
	}
	if (addr == -1) {
		db_printf("no address\n");
		return;
	}
	addr &= ~0x7; /* align */
	{
		register char c, *cp = modif;
		while ((c = *cp++) != 0)
			if (c == 'u')
				asi = ASI_AIUS;
	}
	while (count--) {
		if (db_print_position() == 0) {
			/* Always print the address. */
			db_printf("%16.16lx:\t", addr);
		}
		oldaddr=addr;
		db_printf("%8.8lx\n", (long)ldxa(addr, asi));
		addr += 8;
		if (db_print_position() != 0)
			db_end_line();
	}
}

int64_t pseg_get __P((struct pmap *, vaddr_t));

void
db_dump_pmap(pm)
struct pmap* pm;
{
	/* print all valid pages in the kernel pmap */
	long i, j, k, n;
	paddr_t *pdir, *ptbl;
	/* Almost the same as pmap_collect() */
	
	n = 0;
	for (i=0; i<STSZ; i++) {
		if((pdir = (paddr_t *)(u_long)ldxa((vaddr_t)&pm->pm_segs[i], ASI_PHYS_CACHED))) {
			db_printf("pdir %ld at %lx:\n", i, (long)pdir);
			for (k=0; k<PDSZ; k++) {
				if ((ptbl = (paddr_t *)(u_long)ldxa((vaddr_t)&pdir[k], ASI_PHYS_CACHED))) {
					db_printf("\tptable %ld:%ld at %lx:\n", i, k, (long)ptbl);
					for (j=0; j<PTSZ; j++) {
						int64_t data0, data1;
						data0 = ldxa((vaddr_t)&ptbl[j], ASI_PHYS_CACHED);
						j++;
						data1 = ldxa((vaddr_t)&ptbl[j], ASI_PHYS_CACHED);
						if (data0 || data1) {
							db_printf("%llx: %llx\t",
								  (unsigned long long)(((u_int64_t)i<<STSHIFT)|(k<<PDSHIFT)|((j-1)<<PTSHIFT)),
								  (unsigned long long)(data0));
							db_printf("%llx: %llx\n",
								  (unsigned long long)(((u_int64_t)i<<STSHIFT)|(k<<PDSHIFT)|(j<<PTSHIFT)),
								  (unsigned long long)(data1));
						}
					}
				}
			}
		}
	}
}

void
db_pmap_kernel(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	extern struct pmap kernel_pmap_;
	int i, j, full = 0;
	u_int64_t data;

	{
		register char c, *cp = modif;
		while ((c = *cp++) != 0)
			if (c == 'f')
				full = 1;
	}
	if (have_addr) {
		/* lookup an entry for this VA */
		
		if ((data = pseg_get(&kernel_pmap_, (vaddr_t)addr))) {
			db_printf("pmap_kernel(%p)->pm_segs[%lx][%lx][%lx]=>%qx\n",
				  (void *)addr, (u_long)va_to_seg(addr), 
				  (u_long)va_to_dir(addr), (u_long)va_to_pte(addr),
				  (unsigned long long)data);
		} else {
			db_printf("No mapping for %p\n", (void *)addr);
		}
		return;
	}

	db_printf("pmap_kernel(%p) psegs %p phys %llx\n",
		  &kernel_pmap_, kernel_pmap_.pm_segs,
		  (unsigned long long)kernel_pmap_.pm_physaddr);
	if (full) {
		db_dump_pmap(&kernel_pmap_);
	} else {
		for (j=i=0; i<STSZ; i++) {
			long seg = (long)ldxa((vaddr_t)&kernel_pmap_.pm_segs[i], ASI_PHYS_CACHED);
			if (seg)
				db_printf("seg %d => %lx%c", i, seg, (j++%4)?'\t':'\n');
		}
	}
}

void
db_pm_extract(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	if (have_addr) {
		paddr_t pa;

		if (pmap_extract(pmap_kernel(), addr, &pa))
			db_printf("pa = %llx\n", (long long)pa);
		else
			db_printf("%p not found\n", (void *)addr);
	} else
		db_printf("pmap_extract: no address\n");
}

void
db_pmap_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	struct pmap* pm=NULL;
	int i, j=0, full = 0;

	{
		register char c, *cp = modif;
		if (modif)
			while ((c = *cp++) != 0)
				if (c == 'f')
					full = 1;
	}
	if (curproc && curproc->p_vmspace)
		pm = curproc->p_vmspace->vm_map.pmap;
	if (have_addr) {
		pm = (struct pmap*)addr;
	}

	db_printf("pmap %p: ctx %x refs %d physaddr %llx psegs %p\n",
		pm, pm->pm_ctx, pm->pm_refs,
		(unsigned long long)pm->pm_physaddr, pm->pm_segs);

	if (full) {
		db_dump_pmap(pm);
	} else {
		for (i=0; i<STSZ; i++) {
			long seg = (long)ldxa((vaddr_t)&kernel_pmap_.pm_segs[i], ASI_PHYS_CACHED);
			if (seg)
				db_printf("seg %d => %lx%c", i, seg, (j++%4)?'\t':'\n');
		}
	}
}


void
db_lock(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
#if 0
	struct lock *l;

	if (!have_addr) {
		db_printf("What lock address?\n");
		return;
	}

	l = (struct lock *)addr;
	db_printf("interlock=%x flags=%x\n waitcount=%x sharecount=%x "
	    "exclusivecount=%x\n wmesg=%s recurselevel=%x\n",
	    l->lk_interlock.lock_data, l->lk_flags, l->lk_waitcount,
	    l->lk_sharecount, l->lk_exclusivecount, l->lk_wmesg,
	    l->lk_recurselevel);
#else
	db_printf("locks unsupported\n");
#endif
}

void
db_dump_dtsb(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	extern pte_t *tsb;
	extern int tsbsize;
#define TSBENTS (512<<tsbsize)
	int i;

	db_printf("TSB:\n");
	for (i=0; i<TSBENTS; i++) {
		db_printf("%4d:%4d:%08x %08x:%08x ", i, 
			  (int)((tsb[i].tag&TSB_TAG_G)?-1:TSB_TAG_CTX(tsb[i].tag)),
			  (int)((i<<13)|TSB_TAG_VA(tsb[i].tag)),
			  (int)(tsb[i].data>>32), (int)tsb[i].data);
		i++;
		db_printf("%4d:%4d:%08x %08x:%08x\n", i,
			  (int)((tsb[i].tag&TSB_TAG_G)?-1:TSB_TAG_CTX(tsb[i].tag)),
			  (int)((i<<13)|TSB_TAG_VA(tsb[i].tag)),
			  (int)(tsb[i].data>>32), (int)tsb[i].data);
	}
}

void db_page_cmd __P((db_expr_t, int, db_expr_t, char *));
void
db_page_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{

	if (!have_addr) {
		db_printf("Need paddr for page\n");
		return;
	}

	db_printf("pa %llx pg %p\n", (unsigned long long)addr,
	    PHYS_TO_VM_PAGE(addr));
}


void
db_proc_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	struct proc *p;

	p = curproc;
	if (have_addr) 
		p = (struct proc*) addr;
	if (p == NULL) {
		db_printf("no current process\n");
		return;
	}
	db_printf("process %p:", p);
	db_printf("pid:%d vmspace:%p pmap:%p ctx:%x wchan:%p pri:%d upri:%d\n",
		  p->p_pid, p->p_vmspace, p->p_vmspace->vm_map.pmap, 
		  p->p_vmspace->vm_map.pmap->pm_ctx,
		  p->p_wchan, p->p_priority, p->p_usrpri);
	db_printf("thread @ %p = %p tf:%p ", &p->p_thread, p->p_thread,
		  p->p_md.md_tf);
	db_printf("maxsaddr:%p ssiz:%dpg or %llxB\n",
		  p->p_vmspace->vm_maxsaddr, p->p_vmspace->vm_ssize, 
		  (unsigned long long)ctob(p->p_vmspace->vm_ssize));
	db_printf("profile timer: %ld sec %ld usec\n",
		  p->p_stats->p_timer[ITIMER_PROF].it_value.tv_sec,
		  p->p_stats->p_timer[ITIMER_PROF].it_value.tv_usec);
	db_printf("pcb: %p fpstate: %p\n", &p->p_addr->u_pcb, 
		p->p_md.md_fpstate);
	return;
}

void
db_ctx_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	struct proc *p;

	/* XXX LOCKING XXX */
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_stat) {
			db_printf("process %p:", p);
			db_printf("pid:%d pmap:%p ctx:%x tf:%p fpstate %p "
				"lastcall:%s\n",
				p->p_pid, p->p_vmspace->vm_map.pmap,
				p->p_vmspace->vm_map.pmap->pm_ctx,
				p->p_md.md_tf, p->p_md.md_fpstate,
				(p->p_addr->u_pcb.lastcall)?
				p->p_addr->u_pcb.lastcall : "Null");
		}
	}
	return;
}

void
db_dump_pcb(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	extern struct pcb *cpcb;
	struct pcb *pcb;
	int i;

	pcb = cpcb;
	if (have_addr) 
		pcb = (struct pcb*) addr;

	db_printf("pcb@%p sp:%p pc:%p cwp:%d pil:%d nsaved:%x onfault:%p\nlastcall:%s\nfull windows:\n",
		  pcb, (void *)(long)pcb->pcb_sp, (void *)(long)pcb->pcb_pc, pcb->pcb_cwp,
		  pcb->pcb_pil, pcb->pcb_nsaved, (void *)pcb->pcb_onfault,
		  (pcb->lastcall)?pcb->lastcall:"Null");
	
	for (i=0; i<pcb->pcb_nsaved; i++) {
		db_printf("win %d: at %llx local, in\n", i, 
			  (unsigned long long)pcb->pcb_rw[i+1].rw_in[6]);
		db_printf("%16llx %16llx %16llx %16llx\n",
			  (unsigned long long)pcb->pcb_rw[i].rw_local[0],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[1],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[2],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[3]);
		db_printf("%16llx %16llx %16llx %16llx\n",
			  (unsigned long long)pcb->pcb_rw[i].rw_local[4],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[5],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[6],
			  (unsigned long long)pcb->pcb_rw[i].rw_local[7]);
		db_printf("%16llx %16llx %16llx %16llx\n",
			  (unsigned long long)pcb->pcb_rw[i].rw_in[0],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[1],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[2],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[3]);
		db_printf("%16llx %16llx %16llx %16llx\n",
			  (unsigned long long)pcb->pcb_rw[i].rw_in[4],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[5],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[6],
			  (unsigned long long)pcb->pcb_rw[i].rw_in[7]);
	}
}


void
db_setpcb(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	struct proc *p, *pp;

	extern struct pcb *cpcb;

	if (!have_addr) {
		db_printf("What PID do you want to map in?\n");
		return;
	}
    
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		pp = p->p_pptr;
		if (p->p_stat && p->p_pid == addr) {
			curproc = p;
			cpcb = (struct pcb*)p->p_addr;
			if (p->p_vmspace->vm_map.pmap->pm_ctx) {
				switchtoctx(p->p_vmspace->vm_map.pmap->pm_ctx);
				return;
			}
			db_printf("PID %ld has a null context.\n", addr);
			return;
		}
	}
	db_printf("PID %ld not found.\n", addr);
}

static void
db_print_trace_entry(te, i)
	struct traptrace *te;
	int i;
{
	db_printf("%d:%d p:%d tt:%x:%llx:%llx %llx:%llx ", i, 
		  (int)te->tl, (int)te->pid, 
		  (int)te->tt, (unsigned long long)te->tstate, 
		  (unsigned long long)te->tfault, (unsigned long long)te->tsp,
		  (unsigned long long)te->tpc);
	db_printsym((u_long)te->tpc, DB_STGY_PROC, db_printf);
	db_printf(": ");
	if ((te->tpc && !(te->tpc&0x3)) &&
	    curproc &&
	    (curproc->p_pid == te->pid)) {
		db_disasm((u_long)te->tpc, 0);
	} else db_printf("\n");
}

void
db_traptrace(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	int i, start = 0, full = 0, reverse = 0;
	struct traptrace *end;

	start = 0;
	end = &trap_trace_end[0];

	{
		register char c, *cp = modif;
		if (modif)
			while ((c = *cp++) != 0) {
				if (c == 'f')
					full = 1;
				if (c == 'r')
					reverse = 1;
			}
	}

	if (have_addr) {
		start = addr / (sizeof (struct traptrace));
		if (&trap_trace[start] > &trap_trace_end[0]) {
			db_printf("Address out of range.\n");
			return;
		}
		if (!full) end =  &trap_trace[start+1];
	}

	db_printf("#:tl p:pid tt:tt:tstate:tfault sp:pc\n");
	if (reverse) {
		if (full && start)
			for (i=start; --i;) {
				db_print_trace_entry(&trap_trace[i], i);
			}
		i = (end - &trap_trace[0]);
		while(--i > start) {
			db_print_trace_entry(&trap_trace[i], i);
		}
	} else {
		for (i=start; &trap_trace[i] < end ; i++) {
			db_print_trace_entry(&trap_trace[i], i);
		}
		if (full && start)
			for (i=0; i < start ; i++) {
				db_print_trace_entry(&trap_trace[i], i);
			}
	}
}

/* 
 * Use physical or virtul watchpoint registers -- ugh
 *
 * UltraSPARC I and II have both a virtual and physical
 * watchpoint register.  They are controlled by the LSU 
 * control register.  
 */
void
db_watch(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	int phys = 0;
	int read = 0;
	int width = 8; /* Default to 8 bytes */
	int64_t mask = 0xff;

#define	WATCH_VR	(1L<<22)
#define	WATCH_VW	(1L<<21)
#define	WATCH_PR	(1L<<24)
#define	WATCH_PW	(1L<<23)
#define	WATCH_PM_SHIFT	33
#define	WATCH_PM	(((u_int64_t)0xffffL)<<WATCH_PM_SHIFT)
#define	WATCH_VM_SHIFT	25
#define	WATCH_VM	(((u_int64_t)0xffffL)<<WATCH_VM_SHIFT)

	{
		register char c, *cp = modif;
		if (modif)
			while ((c = *cp++) != 0)
				switch (c) {
				case 'p':
					/* Physical watchpoint */
					phys = 1;
					break;
				case 'r':
					/* Trap reads too */
					read = 1;
					break;
				case 'b':
					width = 1;
					mask = 0x1 << (addr & 0x7);
					break;
				case 'h':
					width = 2;
					mask = 0x3 << (addr & 0x6);
					break;
				case 'l':
					width = 4;
					mask = 0x7 << (addr & 0x4);
					break;
				case 'L':
					width = 8;
					mask = 0xf;
					break;
				default:
					break;
				}
	}

	if (have_addr) {
		/* turn on the watchpoint */
		int64_t tmp = ldxa(0, ASI_MCCR);
		
		if (phys) {
			tmp &= ~WATCH_PM;
			tmp |= WATCH_PW | (mask << WATCH_PM_SHIFT);
			if (read) tmp |= WATCH_PR;

			stxa(PHYSICAL_WATCHPOINT, ASI_DMMU, addr);
			db_printf("Setting physical watchpoint to %llx-%llx\n",
				(long long)addr, (long long)addr + width);
		} else {
			tmp &= ~WATCH_VM;
			tmp |= WATCH_VW | (mask << WATCH_VM_SHIFT);
			if (read) tmp |= WATCH_VR;

			stxa(VIRTUAL_WATCHPOINT, ASI_DMMU, addr);
			db_printf("Setting virtual watchpoint to %llx-%llx\n",
				(long long)addr, (long long)addr + width);
		}
		stxa(0, ASI_MCCR, tmp);
	} else {
		/* turn off the watchpoint */
		int64_t tmp = ldxa(0, ASI_MCCR);
		if (phys) {
			tmp &= ~(WATCH_PM|WATCH_PR|WATCH_PW);
			db_printf("Disabling physical watchpoint\n");
		} else {
			tmp &= ~(WATCH_VM|WATCH_VR|WATCH_VW);
			db_printf("Disabling virtual watchpoint\n");
		}
		stxa(0, ASI_MCCR, tmp);
	}
}


#include <uvm/uvm.h>

void db_uvmhistdump __P((db_expr_t, int, db_expr_t, char *));
extern void uvmhist_dump __P((struct uvm_history *));
extern struct uvm_history_head uvm_histories;

void
db_uvmhistdump(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{

	uvmhist_dump(uvm_histories.lh_first);
}

#if NESP_SBUS
extern void db_esp(db_expr_t, int, db_expr_t, char*);
#endif

const struct db_command db_machine_command_table[] = {
	{ "ctx",	db_ctx_cmd,	0,	0 },
	{ "dtlb",	db_dump_dtlb,	0,	0 },
	{ "dtsb",	db_dump_dtsb,	0,	0 },
#if NESP_SBUS
	{ "esp",	db_esp,		0,	0 },
#endif
	{ "extract",	db_pm_extract,	0,	0 },
	{ "fpstate",	db_dump_fpstate,0,	0 },
	{ "kmap",	db_pmap_kernel,	0,	0 },
	{ "lock",	db_lock,	0,	0 },
	{ "pcb",	db_dump_pcb,	0,	0 },
	{ "pctx",	db_setpcb,	0,	0 },
	{ "page",	db_page_cmd,	0,	0 },
	{ "phys",	db_pload_cmd,	0,	0 },
	{ "pmap",	db_pmap_cmd,	0,	0 },
	{ "proc",	db_proc_cmd,	0,	0 },
	{ "prom",	db_prom_cmd,	0,	0 },
	{ "pv",		db_dump_pv,	0,	0 },
	{ "stack",	db_dump_stack,	0,	0 },
	{ "tf",		db_dump_trap,	0,	0 },
	{ "ts",		db_dump_ts,	0,	0 },
	{ "traptrace",	db_traptrace,	0,	0 },
	{ "uvmdump",	db_uvmhistdump,	0,	0 },
	{ "watch",	db_watch,	0,	0 },
	{ "window",	db_dump_window,	0,	0 },
	{ (char *)0, }
};

/*
 * support for SOFTWARE_SSTEP:
 * return the next pc if the given branch is taken.
 *
 * note: in the case of conditional branches with annul,
 * this actually returns the next pc in the "not taken" path,
 * but in that case next_instr_address() will return the
 * next pc in the "taken" path.  so even tho the breakpoints
 * are backwards, everything will still work, and the logic is
 * much simpler this way.
 */
db_addr_t
db_branch_taken(inst, pc, regs)
	int inst;
	db_addr_t pc;
	db_regs_t *regs;
{
    union instr insn;
    db_addr_t npc = ddb_regs.ddb_tf.tf_npc;

    insn.i_int = inst;

    /*
     * if this is not an annulled conditional branch, the next pc is "npc".
     */

    if (insn.i_any.i_op != IOP_OP2 || insn.i_branch.i_annul != 1)
	return npc;

    switch (insn.i_op2.i_op2) {
      case IOP2_Bicc:
      case IOP2_FBfcc:
      case IOP2_BPcc:
      case IOP2_FBPfcc:
      case IOP2_CBccc:
	/* branch on some condition-code */
	switch (insn.i_branch.i_cond)
	{
	  case Icc_A: /* always */
	    return pc + ((inst << 10) >> 8);

	  default: /* all other conditions */
	    return npc + 4;
	}

      case IOP2_BPr:
	/* branch on register, always conditional */
	return npc + 4;

      default:
	/* not a branch */
	panic("branch_taken() on non-branch");
    }
}

boolean_t
db_inst_branch(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    if (insn.i_any.i_op != IOP_OP2)
	return FALSE;

    switch (insn.i_op2.i_op2) {
      case IOP2_BPcc:
      case IOP2_Bicc:
      case IOP2_BPr:
      case IOP2_FBPfcc:
      case IOP2_FBfcc:
      case IOP2_CBccc:
	return TRUE;

      default:
	return FALSE;
    }
}


boolean_t
db_inst_call(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    switch (insn.i_any.i_op) {
      case IOP_CALL:
	return TRUE;

      case IOP_reg:
	return (insn.i_op3.i_op3 == IOP3_JMPL) && !db_inst_return(inst);

      default:
	return FALSE;
    }
}


boolean_t
db_inst_unconditional_flow_transfer(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    if (db_inst_call(inst))
	return TRUE;

    if (insn.i_any.i_op != IOP_OP2)
	return FALSE;

    switch (insn.i_op2.i_op2)
    {
      case IOP2_BPcc:
      case IOP2_Bicc:
      case IOP2_FBPfcc:
      case IOP2_FBfcc:
      case IOP2_CBccc:
	return insn.i_branch.i_cond == Icc_A;

      default:
	return FALSE;
    }
}


boolean_t
db_inst_return(inst)
	int inst;
{
    return (inst == I_JMPLri(I_G0, I_O7, 8) ||		/* ret */
	    inst == I_JMPLri(I_G0, I_I7, 8));		/* retl */
}

boolean_t
db_inst_trap_return(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    return (insn.i_any.i_op == IOP_reg &&
	    insn.i_op3.i_op3 == IOP3_RETT);
}


int
db_inst_load(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    if (insn.i_any.i_op != IOP_mem)
	return 0;

    switch (insn.i_op3.i_op3) {
      case IOP3_LD:
      case IOP3_LDUB:
      case IOP3_LDUH:
      case IOP3_LDD:
      case IOP3_LDSB:
      case IOP3_LDSH:
      case IOP3_LDSTUB:
      case IOP3_SWAP:
      case IOP3_LDA:
      case IOP3_LDUBA:
      case IOP3_LDUHA:
      case IOP3_LDDA:
      case IOP3_LDSBA:
      case IOP3_LDSHA:
      case IOP3_LDSTUBA:
      case IOP3_SWAPA:
      case IOP3_LDF:
      case IOP3_LDFSR:
      case IOP3_LDDF:
      case IOP3_LFC:
      case IOP3_LDCSR:
      case IOP3_LDDC:
	return 1;

      default:
	return 0;
    }
}

int
db_inst_store(inst)
	int inst;
{
    union instr insn;

    insn.i_int = inst;

    if (insn.i_any.i_op != IOP_mem)
	return 0;

    switch (insn.i_op3.i_op3) {
      case IOP3_ST:
      case IOP3_STB:
      case IOP3_STH:
      case IOP3_STD:
      case IOP3_LDSTUB:
      case IOP3_SWAP:
      case IOP3_STA:
      case IOP3_STBA:
      case IOP3_STHA:
      case IOP3_STDA:
      case IOP3_LDSTUBA:
      case IOP3_SWAPA:
      case IOP3_STF:
      case IOP3_STFSR:
      case IOP3_STDFQ:
      case IOP3_STDF:
      case IOP3_STC:
      case IOP3_STCSR:
      case IOP3_STDCQ:
      case IOP3_STDC:
	return 1;

      default:
	return 0;
    }
}
