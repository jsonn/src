/* $NetBSD: db_machdep.c,v 1.6.10.1 1997/10/15 05:24:59 thorpej Exp $ */

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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

#include <machine/irqhandler.h>

void
db_show_fs_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	struct vfsops **vfsp;
	int s;
	
	s = splhigh();

	db_printf("Registered filesystems (%d)\n", nvfssw);
         
	for (vfsp = &vfssw[0]; vfsp < &vfssw[nvfssw]; vfsp++) {
		if (*vfsp == NULL)
			continue;
		db_printf("  %s\n", (*vfsp)->vfs_name);
	}
	(void)splx(s);
}


/*
 * Print out a description of a vnode.
 * Some parts borrowed from kern/vfs_subr.c
 */
 
static char *typename[] =
   { "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD" };

void
db_show_vnode_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	char buf[64];
	struct vnode *vp;

	if (!have_addr) {
		db_printf("vnode address must be specified\n");
		return;
	}

	vp = (struct vnode *)addr;

/* Decode the one argument */

	db_printf("vp : %08x\n", (u_int)vp);
	db_printf("vp->v_type = %d\n", vp->v_type);
	db_printf("vp->v_flag = %ld\n", vp->v_flag);
	db_printf("vp->v_numoutput = %ld\n", vp->v_numoutput);

	db_printf("type %s, usecount %d, writecount %d, refcount %ld,",
		typename[vp->v_type], vp->v_usecount, vp->v_writecount,
		vp->v_holdcnt);
	buf[0] = '\0';
	if (vp->v_flag & VROOT)
		strcat(buf, "|VROOT");
	if (vp->v_flag & VTEXT)
		strcat(buf, "|VTEXT");
	if (vp->v_flag & VSYSTEM)
		strcat(buf, "|VSYSTEM");
	if (vp->v_flag & VXLOCK)
		strcat(buf, "|VXLOCK");
	if (vp->v_flag & VXWANT)
		strcat(buf, "|VXWANT");
	if (vp->v_flag & VBWAIT)
		strcat(buf, "|VBWAIT");
	if (vp->v_flag & VALIASED)
		strcat(buf, "|VALIASED");
	if (buf[0] != '\0')
		db_printf(" flags (%s)", &buf[1]);
		db_printf("\n");
	if (vp->v_data != NULL) {
		db_printf("data=%08x\n", (u_int)vp->v_data);
	}
}


void
db_show_vmstat_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	struct vmmeter sum;
    
	sum = cnt;
	db_printf("%9u cpu context switches\n", sum.v_swtch);
	db_printf("%9u device interrupts\n", sum.v_intr);
	db_printf("%9u software interrupts\n", sum.v_soft);
	db_printf("%9u traps\n", sum.v_trap);
	db_printf("%9u system calls\n", sum.v_syscall);
	db_printf("%9u total faults taken\n", sum.v_faults);
	db_printf("%9u swap ins\n", sum.v_swpin);
	db_printf("%9u swap outs\n", sum.v_swpout);
	db_printf("%9u pages swapped in\n", sum.v_pswpin / CLSIZE);
	db_printf("%9u pages swapped out\n", sum.v_pswpout / CLSIZE);
	db_printf("%9u page ins\n", sum.v_pageins);
	db_printf("%9u page outs\n", sum.v_pageouts);
	db_printf("%9u pages paged in\n", sum.v_pgpgin);
	db_printf("%9u pages paged out\n", sum.v_pgpgout);
	db_printf("%9u pages reactivated\n", sum.v_reactivated);
	db_printf("%9u intransit blocking page faults\n", sum.v_intrans);
	db_printf("%9u zero fill pages created\n", sum.v_nzfod / CLSIZE);
	db_printf("%9u zero fill page faults\n", sum.v_zfod / CLSIZE);
	db_printf("%9u pages examined by the clock daemon\n", sum.v_scan);
	db_printf("%9u revolutions of the clock hand\n", sum.v_rev);
	db_printf("%9u VM object cache lookups\n", sum.v_lookups);
	db_printf("%9u VM object hits\n", sum.v_hits);
	db_printf("%9u total VM faults taken\n", sum.v_vm_faults);
	db_printf("%9u copy-on-write faults\n", sum.v_cow_faults);
	db_printf("%9u pages freed by daemon\n", sum.v_dfree);
	db_printf("%9u pages freed by exiting processes\n", sum.v_pfree);
	db_printf("%9u pages free\n", sum.v_free_count);
	db_printf("%9u pages wired down\n", sum.v_wire_count);
	db_printf("%9u pages active\n", sum.v_active_count);
	db_printf("%9u pages inactive\n", sum.v_inactive_count);
	db_printf("%9u bytes per page\n", sum.v_page_size);
}

void
db_show_intrchain_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	int loop;
	irqhandler_t *ptr;
	char *name;
	db_expr_t offset;

	for (loop = 0; loop < NIRQS; ++loop) {
		ptr = irqhandlers[loop];
		if (ptr) {
			db_printf("IRQ %d\n", loop);

			while (ptr) {
				db_printf("  %-13s %d ", ptr->ih_name, ptr->ih_level);
				db_find_sym_and_offset((u_int)ptr->ih_func, &name, &offset);
				if (name == NULL)
					name = "?";

				db_printf("%s(", name);
				db_printsym((u_int)ptr->ih_func, DB_STGY_PROC);
				db_printf(") %08x\n", (u_int)ptr->ih_arg);
				ptr = ptr->ih_next;
			}
		}
	}
}


void
db_show_panic_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
{
	int s;
	
	s = splhigh();

	db_printf("Panic string: %s\n", panicstr);

	(void)splx(s);
}


void
db_show_frame_cmd(addr, have_addr, count, modif)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
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
