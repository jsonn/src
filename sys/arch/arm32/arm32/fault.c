/*	$NetBSD: fault.c,v 1.41.2.1.2.1 1999/06/21 00:47:30 thorpej Exp $	*/

/*
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * fault.c
 *
 * Fault handlers
 *
 * Created      : 28/11/94
 */

#include "opt_ddb.h"
#include "opt_pmap_debug.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm_extern.h>

#include <machine/frame.h>
#include <machine/katelib.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/irqhandler.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif

#include <arm32/arm32/disassem.h>

#ifdef PMAP_DEBUG
extern int pmap_debug_level;
#endif	/* PMAP_DEBUG */

int pmap_modified_emulation __P((pmap_t, vm_offset_t));
int pmap_handled_emulation __P((pmap_t, vm_offset_t));
pt_entry_t *pmap_pte __P((pmap_t pmap, vm_offset_t va));
int cowfault __P((vaddr_t));

int fetchuserword __P((u_int address, u_int *location));
extern char fusubailout[];

/* Abort code */

/* Define text descriptions of the different aborts */

static const char *aborts[16] = {
	"Write buffer fault",
	"Alignment fault",
	"Write buffer fault",
	"Alignment fault",
	"Bus error (LF section)",
	"Translation fault (section)",
	"Bus error (page)",
	"Translation fault (page)",
	"Bus error (section)",
	"Domain error (section)",
	"Bus error (page)",
	"Domain error (page)", 
	"Bus error trans (L1)",
	"Permission error (section)",
	"Bus error trans (L2)",
	"Permission error (page)"
};

void
report_abort(prefix, fault_status, fault_address, fault_pc)
	const char *prefix;
	u_int fault_status;
	u_int fault_address;
	u_int fault_pc;
{
#ifndef DEBUG
	if (prefix == NULL) {
#endif
		if (prefix)
			printf("%s ", prefix);
		printf("Data abort: '%s' status=%03x address=%08x PC=%08x\n",
		    aborts[fault_status & FAULT_TYPE_MASK],
		    fault_status & 0xfff, fault_address, fault_pc);
#ifndef DEBUG
	}
#endif
}

/*
 * void data_abort_handler(trapframe_t *frame)
 *
 * Abort handler called when read/write occurs at an address of
 * a non existant or restricted (access permissions) memory page.
 * We first need to identify the type of page fault.
 */

#define TRAP_CODE ((fault_status & 0x0f) | (fault_address & 0xfffffff0))

void
data_abort_handler(frame)
	trapframe_t *frame;
{
	struct proc *p;
	struct pcb *pcb;
	u_int fault_address;
	u_int fault_status;
	u_int fault_pc;
	u_int fault_instruction;
	int fault_code;
	int user;
	u_quad_t sticks = 0;
	int error;

	/*
	 * Must get fault address and status from the CPU before
	 * re-enabling interrupts.  (Interrupt handlers may take
	 * R/M emulation faults.)
	 */
	fault_address = cpu_faultaddress();
	fault_status = cpu_faultstatus();
	fault_pc = frame->tf_pc;

	/*
	 * Enable IRQ's (disabled by CPU on abort) if trapframe
	 * shows they were enabled.
	 */
	if (!(frame->tf_spsr & I32_bit))
		enable_interrupts(I32_bit);

	/* Update vmmeter statistics */
	uvmexp.traps++;

	/* Extract the fault code from the fault status */
	fault_code = fault_status & FAULT_TYPE_MASK;

	/* Get the current proc structure or proc0 if there is none */
	if ((p = curproc) == NULL)
		p = &proc0;

	/*
	 * can't use curpcb, as it might be NULL; and we have p in
	 * a register anyway
	 */
	pcb = &p->p_addr->u_pcb;

	/* fusubailout is used by [fs]uswintr to avoid page faulting */
	if (pcb->pcb_onfault
	    && ((fault_code != FAULT_TRANS_S && fault_code != FAULT_TRANS_P)
	        || pcb->pcb_onfault == fusubailout)) {
copyfault:
#ifdef DEBUG
		printf("Using pcb_onfault=%p addr=%08x st=%08x p=%p\n",
		    pcb->pcb_onfault, fault_address, fault_status, p);
#endif
		frame->tf_pc = (u_int)pcb->pcb_onfault;
		if ((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE)
			panic("Yikes pcb_onfault=%p during USR mode fault\n",
			    pcb->pcb_onfault);
		return;
	}

	/* More debug stuff */

	fault_instruction = ReadWord(fault_pc);

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0) {
		report_abort(NULL, fault_status, fault_address, fault_pc);
		printf("Instruction @V%08x = %08x\n",
		    fault_pc, fault_instruction);
	}
#endif               

	/* Call the cpu specific abort fixup routine */
	error = cpu_dataabt_fixup(frame);
	if (error == ABORT_FIXUP_RETURN)
		return;
	if (error == ABORT_FIXUP_FAILED)
		panic("data abort fixup failed\n");

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("fault in process %p\n", p);
#endif

#ifdef DEBUG
	/* Is this needed ? */
	if (pcb != curpcb) {
		printf("data_abort: Alert ! pcb(%p) != curpcb(%p)\n",
		    pcb, curpcb);
		printf("data_abort: Alert ! proc(%p), curproc(%p)\n",
		    p, curproc);
	}
#endif	/* DEBUG */

	/* Were we in user mode when the abort occurred ? */
	if ((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE) {
		sticks = p->p_sticks;
        
		/*
		 * Note that the fault was from USR mode.
		 */
		user = 1;
		p->p_md.md_regs = frame;
	} else
		user = 0;

	/* Now act on the fault type */
	switch (fault_code) {
	case FAULT_WRTBUF_0:              /* Write Buffer Fault */
	case FAULT_WRTBUF_1:              /* Write Buffer Fault */
		/* If this happens forget it no point in continuing */

		/* FALLTHROUGH */

	case FAULT_ALIGN_0:              /* Alignment Fault */
	case FAULT_ALIGN_1:              /* Alignment Fault */
		/*
		 * Really this should just kill the process.
		 * Alignment faults are turned off in the kernel
		 * in order to get better performance from shorts with
		 * GCC so an alignment fault means somebody has played
		 * with the control register in the CPU. Might as well
		 * panic as the kernel was not compiled for aligned accesses.
		 */

		/* FALLTHROUGH */
          
	case FAULT_BUSERR_0:              /* Bus Error LF Section */
	case FAULT_BUSERR_1:              /* Bus Error Page */
	case FAULT_BUSERR_2:              /* Bus Error Section */
	case FAULT_BUSERR_3:              /* Bus Error Page */
		/* What will accutally cause a bus error ? */
		/* Real bus errors are not a process problem but hardware */

		/* FALLTHROUGH */
          
	case FAULT_DOMAIN_S:              /* Section Domain Error Fault */
	case FAULT_DOMAIN_P:              /* Page Domain Error Fault*/
		/*
		 * Right well we dont use domains, everything is
		 * always a client and thus subject to access permissions.
		 * If we get a domain error then we have corrupts PTE's
		 * so we might as well die !
		 * I suppose eventually this should just kill the process
		 * who owns the PTE's but if this happens it implies a
		 * kernel problem.
		 */

		/* FALLTHROUGH */

	case FAULT_BUSTRNL1:              /* Bus Error Trans L1 Fault */
	case FAULT_BUSTRNL2:              /* Bus Error Trans L2 Fault */
		/*
		 * These faults imply that the PTE is corrupt.
		 * Likely to be a kernel fault so we had better stop.
		 */

		/* FALLTHROUGH */

	default :
		/* Are there any combinations I have missed ? */
		report_abort(NULL, fault_status, fault_address, fault_pc);

	we_re_toast:
		/*
		 * Were are dead, try and provide some debug
		 * infomation before dying
		 */
#ifdef DDB
		printf("Unhandled trap (frame = %p)\n", frame);
		report_abort(NULL, fault_status, fault_address, fault_pc);
		kdb_trap(-1, frame);
		return;
#else
		panic("Unhandled trap (frame = %p)", frame);
#endif	/* DDB */
          
	case FAULT_TRANS_P:              /* Page Translation Fault */
	case FAULT_PERM_P:		 /* Page Permission Fault */
	case FAULT_TRANS_S:              /* Section Translation Fault */
	case FAULT_PERM_S:		 /* Section Permission Fault */
	/*
	 * Page/section translation/permission fault -- need to fault in
	 * the page and possibly the page table page.
	 */
	{
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;

		va = trunc_page((vm_offset_t)fault_address);

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("page fault: addr=V%08lx ", va);
#endif
          
		/*
		 * It is only a kernel address space fault iff:
		 *	1. user == 0  and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (!user &&
		    (va >= VM_MIN_KERNEL_ADDRESS || va < VM_MIN_ADDRESS)) {
			/* Was the fault due to the FPE/IPKDB ? */
			if ((frame->tf_spsr & PSR_MODE) == PSR_UND32_MODE) {
				report_abort("UND32", fault_status,
				    fault_address, fault_pc);
				trapsignal(p, SIGSEGV, TRAP_CODE);

				/*
				 * Force exit via userret()
				 * This is necessary as the FPE is an extension
				 * to userland that actually runs in a
				 * priveledged mode but uses USR mode
				 * permissions for its accesses.
				 */
				userret(p, frame->tf_pc, p->p_sticks);
				return;
			}
			map = kernel_map;
		} else
			map = &vm->vm_map;

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("vmmap=%p ", map);
#endif

		if (map == NULL)
			panic("No map for fault address\n");

		/*
		 * We need to know whether the page should be mapped
		 * as R or R/W. The MMU does not give us the info as
		 * to whether the fault was caused by a read or a write.
		 * This means we need to disassemble the instruction
		 * responsible and determine if it was a read or write
		 * instruction.
		 */
		/* STR instruction ? */
		if ((fault_instruction & 0x0c100000) == 0x04000000)
			ftype = VM_PROT_READ | VM_PROT_WRITE; 
		/* STM or CDT instruction ? */
		else if ((fault_instruction & 0x0a100000) == 0x08000000)
			ftype = VM_PROT_READ | VM_PROT_WRITE; 
		/* STRH, STRSH or STRSB instruction ? */
		else if ((fault_instruction & 0x0e100090) == 0x00000090)
			ftype = VM_PROT_READ | VM_PROT_WRITE; 
		/* SWP instruction ? */
		else if ((fault_instruction & 0x0fb00ff0) == 0x01000090)
			ftype = VM_PROT_READ | VM_PROT_WRITE; 
		else
			ftype = VM_PROT_READ;

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("fault protection = %d\n", ftype);
#endif
            
		if ((ftype & VM_PROT_WRITE) ?
		    pmap_modified_emulation(map->pmap, va) :
		    pmap_handled_emulation(map->pmap, va))
			goto out;

#ifdef DIAGNOSTIC
		if (current_intr_depth > 0) {
#ifdef DDB
			printf("Non-emulated page fault with intr_depth > 0\n");
			report_abort(NULL, fault_status, fault_address, fault_pc);
			kdb_trap(-1, frame);
			return;
#else
			panic("Fault with intr_depth > 0");
#endif	/* DDB */
		}
#endif	/* DIAGNOSTIC */

		rv = uvm_fault(map, va, 0, ftype);
		if (rv == KERN_SUCCESS)
			goto out;

		if (user == 0) {
			if (pcb->pcb_onfault)
				goto copyfault;
			printf("[u]vm_fault(%p, %lx, %x, 0) -> %x\n",
			    map, va, ftype, rv);
			goto we_re_toast;
		}

		report_abort("", fault_status, fault_address, fault_pc);
		if (rv == KERN_RESOURCE_SHORTAGE) {
			printf("UVM: pid %d (%s), uid %d killed: "
			       "out of swap\n", p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, TRAP_CODE);
		} else
			trapsignal(p, SIGSEGV, TRAP_CODE);
		break;
	}            
	}
          
out:
	/* Call userret() if it was a USR mode fault */
	if (user)
		userret(p, frame->tf_pc, sticks);
}


/*
 * void prefetch_abort_handler(trapframe_t *frame)
 *
 * Abort handler called when instruction execution occurs at
 * a non existant or restricted (access permissions) memory page.
 * If the address is invalid and we were in SVC mode then panic as
 * the kernel should never prefetch abort.
 * If the address is invalid and the page is mapped then the user process
 * does no have read permission so send it a signal.
 * Otherwise fault the page in and try again.
 */

extern int kernel_debug;

void
prefetch_abort_handler(frame)
	trapframe_t *frame;
{
	register u_int fault_pc;
	register struct proc *p;
	register struct pcb *pcb;
	u_int fault_instruction;
	u_quad_t sticks;
	pt_entry_t *pte;
	int error;

	/*
	 * Enable IRQ's (disabled by the abort) This always comes
	 * from user mode so we know interrupts were not disabled.
	 * But we check anyway.
	 */
	if (!(frame->tf_spsr & I32_bit))
		enable_interrupts(I32_bit);

#ifdef DIAGNOSTIC
	/* Paranoia: We should always be in SVC32 mode at this point */
	if ((GetCPSR() & PSR_MODE) != PSR_SVC32_MODE) {
		panic("Fault handler not in SVC mode\n");
	}
#endif	/* DIAGNOSTIC */

	/* Update vmmeter statistics */
	uvmexp.traps++;

	/* Call the cpu specific abort fixup routine */
	error = cpu_prefetchabt_fixup(frame);
	if (error == ABORT_FIXUP_RETURN)
		return;
	if (error == ABORT_FIXUP_FAILED)
		panic("prefetch abort fixup failed\n");

	/* Get the current proc structure or proc0 if there is none */
	if ((p = curproc) == 0) {
		p = &proc0;
#ifdef DEBUG
		printf("Prefetch abort with curproc == 0\n");
#endif
	}

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("prefetch fault in process %p %s\n", p, p->p_comm);
#endif
	/*
	 * can't use curpcb, as it might be NULL; and we have p in a
	 * register anyway
	 */
	pcb = &p->p_addr->u_pcb;
	if (pcb == 0)
		panic("prefetch_abort_handler: no pcb ... we're toast !\n");

#ifdef DEBUG
	if (pcb != curpcb) {
		printf("data_abort: Alert ! pcb(%p) != curpcb(%p)\n",
		    pcb, curpcb);
		printf("data_abort: Alert ! proc(%p), curproc(%p)\n",
		    p, curproc);
	}
#endif	/* DEBUG */

	/* Was the prefectch abort from USR32 mode ? */

	if ((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE) {
		sticks = p->p_sticks;
		p->p_md.md_regs = frame;
	} else {
		/*
		 * All the kernel code pages are loaded at boot time
		 * and do not get paged
		 */
	        panic("Prefetch abort in non-USR mode (frame=%p)\n", frame);
	}

	/* Get fault address */
	fault_pc = frame->tf_pc;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("prefetch_abort: PC = %08x\n", fault_pc);
#endif
	/* Ok validate the address, can only execute in USER space */
	if (fault_pc < VM_MIN_ADDRESS || fault_pc >= VM_MAXUSER_ADDRESS) {
#ifdef DEBUG
		printf("prefetch: pc (%08x) not in user process space\n",
		    fault_pc);
#endif
		trapsignal(p, SIGSEGV, fault_pc);
		userret(p, frame->tf_pc, sticks);
		return;
	}

	/* Is the page already mapped ? */
	/* This is debugging for rev K SA110 silicon */
	pte = pmap_pte(p->p_vmspace->vm_map.pmap, (vm_offset_t)fault_pc);
	if (pte && *pte != 0) {
		if (kernel_debug & 1) {
			printf("prefetch_abort: page is already mapped - pte=%p *pte=%08x\n",
			    pte, *pte);
			printf("prefetch_abort: pc=%08x proc=%p process=%s\n", fault_pc, p, p->p_comm);
			printf("prefetch_abort: far=%08x fs=%x\n", cpu_faultaddress(), cpu_faultstatus());
			printf("prefetch_abort: trapframe=%08x\n", (u_int)frame);
		}
#ifdef DDB
		if (kernel_debug & 2)
			Debugger();
#endif
	}
	
	/* Ok read the fault address. This will fault the page in for us */
	if (fetchuserword(fault_pc, &fault_instruction) != 0) {
#ifdef DEBUG
		printf("prefetch: faultin failed for address %08x\n",
		    fault_pc);
#endif
		trapsignal(p, SIGSEGV, fault_pc);
	} else {

#ifdef DIAGNOSTIC
		/* More debug stuff */

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0) {
			printf("Instruction @V%08x = %08x\n", fault_pc,
			    fault_instruction);
			disassemble(fault_pc);
			printf("return addr=%08x", frame->tf_pc);
			pte = pmap_pte(p->p_vmspace->vm_map.pmap,
			    (vm_offset_t)fault_pc);
			if (pte)
				printf(" pte=%p *pte=%08x\n", pte, *pte);
			else
				printf("\n");

		}
#endif	/* PMAP_DEBUG */
#endif	/* DIAGNOSTIC */
	}

	userret(p, frame->tf_pc, sticks);
}

int
cowfault(va)
	vaddr_t va;
{
	struct vmspace *vm;

	if (va >= VM_MAXUSER_ADDRESS)
		return (EFAULT);

	vm = curproc->p_vmspace;
	if (uvm_fault(&vm->vm_map, va, 0, VM_PROT_READ | VM_PROT_WRITE)
	    != KERN_SUCCESS)
		return (EFAULT);

	return (0);
}

/* End of fault.c */
