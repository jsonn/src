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
 *	$Id: db_interface.c,v 1.7.4.2 1994/10/09 09:14:21 mycroft Exp $
 */

/*
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */
#include <setjmp.h>

#include <vm/vm.h>

#include <machine/cpufunc.h>
#include <machine/db_machdep.h>

extern jmp_buf	*db_recover;

int	db_active = 0;

/*
 * Received keyboard interrupt sequence.
 */
kdb_kbd_trap(regs)
	struct i386_saved_state *regs;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
		printf("\n\nkernel: keyboard interrupt\n");
		kdb_trap(-1, 0, regs);
	}
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
kdb_trap(type, code, regs)
	int	type, code;
	register struct i386_saved_state *regs;
{
	int s;

#if 0
	if ((boothowto&RB_KDB) == 0)
		return(0);
#endif

	switch (type) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
	case -1:	/* keyboard interrupt */
		break;
	default:
		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

	/* Should switch to kdb`s own stack here. */

	ddb_regs = *regs;

	if ((regs->tf_cs & 0x3) == 0) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
#if 0
		ddb_regs.ss   = KERNEL_DS;
#endif
		asm(" movw %%ss,%%ax; movl %%eax,%0 " 
		    : "=g" (ddb_regs.tf_ss) 
		    :
		    : "ax");
	}

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

	regs->tf_eip = ddb_regs.tf_eip;
	regs->tf_eflags = ddb_regs.tf_eflags;
	regs->tf_eax = ddb_regs.tf_eax;
	regs->tf_ecx = ddb_regs.tf_ecx;
	regs->tf_edx = ddb_regs.tf_edx;
	regs->tf_ebx = ddb_regs.tf_ebx;
	if (regs->tf_cs & 0x3) {
		/*
		 * user mode - saved esp and ss valid
		 */
		regs->tf_esp = ddb_regs.tf_esp;		/* user stack pointer */
		regs->tf_ss  = ddb_regs.tf_ss & 0xffff;	/* user stack segment */
	}
	regs->tf_ebp = ddb_regs.tf_ebp;
	regs->tf_esi = ddb_regs.tf_esi;
	regs->tf_edi = ddb_regs.tf_edi;
	regs->tf_es = ddb_regs.tf_es & 0xffff;
	regs->tf_cs = ddb_regs.tf_cs & 0xffff;
	regs->tf_ds = ddb_regs.tf_ds & 0xffff;
#if 0
	regs->tf_fs = ddb_regs.tf_fs & 0xffff;
	regs->tf_gs = ddb_regs.tf_gs & 0xffff;
#endif

	return (1);
}

extern char *trap_type[];
extern int trap_types;

/*
 * Print trap reason.
 */
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	if (type >= trap_types || type < 0)
		printf("type %d", type);
	else
		printf("%s", trap_type[type]);
	printf(" trap, code=%x\n", code);
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

pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*dst;

	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = { 0 };
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = { 0 };
	extern char	etext;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr < (vm_offset_t)&etext) {
		ptep0 = pmap_pte(kernel_pmap, addr);
		oldmap0 = *ptep0;
		*(int *)ptep0 |= /* INTEL_PTE_WRITE */ PG_RW;

		addr1 = i386_trunc_page(addr + size - 1);
		if (i386_trunc_page(addr) != addr1) {
			/* data crosses a page boundary */
			ptep1 = pmap_pte(kernel_pmap, addr1);
			oldmap1 = *ptep1;
			*(int *)ptep1 |= /* INTEL_PTE_WRITE */ PG_RW;
		}
		tlbflush();
	}

	dst = (char *)addr;

	while (--size >= 0)
		*dst++ = *data++;

	if (ptep0) {
		*ptep0 = oldmap0;
		if (ptep1)
			*ptep1 = oldmap1;
		tlbflush();
	}
}

int
Debugger()
{
	asm ("int $3");
}
