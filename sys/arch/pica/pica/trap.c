/*	$NetBSD: trap.c,v 1.10.8.1 1997/11/15 00:56:40 mellon Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah Hdr: trap.c 1.32 91/04/06
 *
 *	@(#)trap.c	8.5 (Berkeley) 1/11/94
 */

#if #defined(CPU_R4000) && !defined(CPU_R3000)
#error Must define at least one of CPU_R3000 or CPU_R4000.
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <net/netisr.h>

#include <mips/locore.h>

#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/mips_opcode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <sys/cdefs.h>
#include <sys/syslog.h>
#include <miscfs/procfs/procfs.h>

/* all this to get prototypes for ipintr() and arpintr() */
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/ip_var.h>

#ifdef NETATALK
#include <netatalk/at_extern.h>
#endif

struct	proc *machFPCurProcPtr;		/* pointer to last proc to use FP */

/*
 * Port-specific hardware interrupt handler
 */

int (*mips_hardware_intr) __P((u_int mask, u_int pc, u_int status,
			       u_int cause)) =
	( int (*) __P((u_int, u_int, u_int, u_int)) ) 0;

/*
 * Exception-handling functions, called via machExceptionTable from locore
 */
extern void MachTLBModException __P((void));
extern void MachTLBInvalidException __P((void));
extern unsigned MachEmulateBranch __P((unsigned *regsPtr,
			     unsigned instPC,
			     unsigned fpcCSR,
			     int allowNonBranch));

extern void mips_r2000_KernGenException __P((void));
extern void mips_r2000_UserGenException __P((void));
extern void mips_r2000_KernIntr __P((void));
extern void mips_r2000_UserIntr __P((void));
extern void mips_r2000_TLBModException  __P((void));
extern void mips_r2000_TLBMissException __P((void));


extern void mips_r4000_KernGenException __P((void));
extern void mips_r4000_UserGenException __P((void));
extern void mips_r4000_KernIntr __P((void));
extern void mips_r4000_UserIntr __P((void));
extern void mips_r4000_TLBModException  __P((void));
extern void mips_r4000_TLBMissException __P((void));
extern void mips_r4000_TLBInvalidException __P((void));


void (*mips_r4000_ExceptionTable[]) __P((void)) = {
/*
 * The kernel exception handlers.
 */
	mips_r4000_KernIntr,			/* external interrupt */
	mips_r4000_KernGenException,		/* TLB modification */
	mips_r4000_TLBInvalidException,	/* TLB miss (load or instr. fetch) */
	mips_r4000_TLBInvalidException,	/* TLB miss (store) */
	mips_r4000_KernGenException,		/* address error (load or I-fetch) */
	mips_r4000_KernGenException,		/* address error (store) */
	mips_r4000_KernGenException,		/* bus error (I-fetch) */
	mips_r4000_KernGenException,		/* bus error (load or store) */
	mips_r4000_KernGenException,		/* system call */
	mips_r4000_KernGenException,		/* breakpoint */
	mips_r4000_KernGenException,		/* reserved instruction */
	mips_r4000_KernGenException,		/* coprocessor unusable */
	mips_r4000_KernGenException,		/* arithmetic overflow */
	mips_r4000_KernGenException,		/* trap exception */
	mips_r4000_KernGenException,		/* virtual coherence exception inst */
	mips_r4000_KernGenException,		/* floating point exception */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* watch exception */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* virtual coherence exception data */
/*
 * The user exception handlers.
 */
	mips_r4000_UserIntr,			/*  0 */
	mips_r4000_UserGenException,		/*  1 */
	mips_r4000_UserGenException,		/*  2 */
	mips_r4000_UserGenException,		/*  3 */
	mips_r4000_UserGenException,		/*  4 */
	mips_r4000_UserGenException,		/*  5 */
	mips_r4000_UserGenException,		/*  6 */
	mips_r4000_UserGenException,		/*  7 */
	mips_r4000_UserGenException,		/*  8 */
	mips_r4000_UserGenException,		/*  9 */
	mips_r4000_UserGenException,		/* 10 */
	mips_r4000_UserGenException,		/* 11 */
	mips_r4000_UserGenException,		/* 12 */
	mips_r4000_UserGenException,		/* 13 */
	mips_r4000_UserGenException,		/* 14 */
	mips_r4000_UserGenException,		/* 15 */
	mips_r4000_UserGenException,		/* 16 */
	mips_r4000_UserGenException,		/* 17 */
	mips_r4000_UserGenException,		/* 18 */
	mips_r4000_UserGenException,		/* 19 */
	mips_r4000_UserGenException,		/* 20 */
	mips_r4000_UserGenException,		/* 21 */
	mips_r4000_UserGenException,		/* 22 */
	mips_r4000_UserGenException,		/* 23 */
	mips_r4000_UserGenException,		/* 24 */
	mips_r4000_UserGenException,		/* 25 */
	mips_r4000_UserGenException,		/* 26 */
	mips_r4000_UserGenException,		/* 27 */
	mips_r4000_UserGenException,		/* 28 */
	mips_r4000_UserGenException,		/* 29 */
	mips_r4000_UserGenException,		/* 20 */
	mips_r4000_UserGenException,		/* 31 */
};

/*
 * XXX bug compatibility. Some versions of the locore code still refer
 * to the exception vector by the Sprite name.
 * Note these are all different from the r2000 equivalents. At the very
 * least,  the MIPS-1 returns from an exception using rfe, and the
 * MIPS-III uses eret.
*/

void (*machExceptionTable[]) __P((void)) = {
/*
 * The kernel exception handlers.
 */
	mips_r4000_KernIntr,			/* external interrupt */
	mips_r4000_KernGenException,		/* TLB modification */
	mips_r4000_TLBInvalidException,	/* TLB miss (load or instr. fetch) */
	mips_r4000_TLBInvalidException,	/* TLB miss (store) */
	mips_r4000_KernGenException,		/* address error (load or I-fetch) */
	mips_r4000_KernGenException,		/* address error (store) */
	mips_r4000_KernGenException,		/* bus error (I-fetch) */
	mips_r4000_KernGenException,		/* bus error (load or store) */
	mips_r4000_KernGenException,		/* system call */
	mips_r4000_KernGenException,		/* breakpoint */
	mips_r4000_KernGenException,		/* reserved instruction */
	mips_r4000_KernGenException,		/* coprocessor unusable */
	mips_r4000_KernGenException,		/* arithmetic overflow */
	mips_r4000_KernGenException,		/* trap exception */
	mips_r4000_KernGenException,		/* virtual coherence exception inst */
	mips_r4000_KernGenException,		/* floating point exception */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* watch exception */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* reserved */
	mips_r4000_KernGenException,		/* virtual coherence exception data */
/*
 * The user exception handlers.
 */
	mips_r4000_UserIntr,			/*  0 */
	mips_r4000_UserGenException,		/*  1 */
	mips_r4000_UserGenException,		/*  2 */
	mips_r4000_UserGenException,		/*  3 */
	mips_r4000_UserGenException,		/*  4 */
	mips_r4000_UserGenException,		/*  5 */
	mips_r4000_UserGenException,		/*  6 */
	mips_r4000_UserGenException,		/*  7 */
	mips_r4000_UserGenException,		/*  8 */
	mips_r4000_UserGenException,		/*  9 */
	mips_r4000_UserGenException,		/* 10 */
	mips_r4000_UserGenException,		/* 11 */
	mips_r4000_UserGenException,		/* 12 */
	mips_r4000_UserGenException,		/* 13 */
	mips_r4000_UserGenException,		/* 14 */
	mips_r4000_UserGenException,		/* 15 */
	mips_r4000_UserGenException,		/* 16 */
	mips_r4000_UserGenException,		/* 17 */
	mips_r4000_UserGenException,		/* 18 */
	mips_r4000_UserGenException,		/* 19 */
	mips_r4000_UserGenException,		/* 20 */
	mips_r4000_UserGenException,		/* 21 */
	mips_r4000_UserGenException,		/* 22 */
	mips_r4000_UserGenException,		/* 23 */
	mips_r4000_UserGenException,		/* 24 */
	mips_r4000_UserGenException,		/* 25 */
	mips_r4000_UserGenException,		/* 26 */
	mips_r4000_UserGenException,		/* 27 */
	mips_r4000_UserGenException,		/* 28 */
	mips_r4000_UserGenException,		/* 29 */
	mips_r4000_UserGenException,		/* 20 */
	mips_r4000_UserGenException,		/* 31 */
};

char	*trap_type[] = {
	"external interrupt",
	"TLB modification",
	"TLB miss (load or instr. fetch)",
	"TLB miss (store)",
	"address error (load or I-fetch)",
	"address error (store)",
	"bus error (I-fetch)",
	"bus error (load or store)",
	"system call",
	"breakpoint",
	"reserved instruction",
	"coprocessor unusable",
	"arithmetic overflow",
	"r4k trap/r3k reserved 13",
	"r4k virtual coherency instruction/r3k reserved 14",
	"r4k floating point/ r3k reserved 15",
	"reserved 16",
	"reserved 17",
	"reserved 18",
	"reserved 19",
	"reserved 20",
	"reserved 21",
	"reserved 22",
	"r4000 watch",
	"reserved 24",
	"reserved 25",
	"reserved 26",
	"reserved 27",
	"reserved 28",
	"reserved 29",
	"reserved 30",
	"r4000 virtual coherency data",
};

#ifdef DEBUG
#define TRAPSIZE	10
struct trapdebug {		/* trap history buffer for debugging */
	u_int	status;
	u_int	cause;
	u_int	vadr;
	u_int	pc;
	u_int	ra;
	u_int	sp;
	u_int	code;
} trapdebug[TRAPSIZE], *trp = trapdebug;

void trapDump __P((char * msg));
void cpu_getregs __P((int *regs));
#endif	/* DEBUG */

#ifdef DEBUG	/* stack trace code, also useful for DDB one day */
extern void stacktrace();
extern void logstacktrace();

/* extern functions printed by name in stack backtraces */
extern void idle(), cpu_switch(), wbflush();
extern void MachTLBMiss();
extern int main __P((void*));
#endif	/* DEBUG */

extern volatile struct chiptime *Mach_clock_addr;
extern u_long intrcnt[];


/*
 * Handle an exception.
 * Called from MachKernGenException() or MachUserGenException()
 * when a processor trap occurs.
 * In the case of a kernel trap, we return the pc where to resume if
 * ((struct pcb *)UADDR)->pcb_onfault is set, otherwise, return old pc.
 */
unsigned
trap(statusReg, causeReg, vadr, pc, args)
	unsigned statusReg;	/* status register at time of the exception */
	unsigned causeReg;	/* cause register at time of exception */
	unsigned vadr;		/* address (if any) the fault occured on */
	unsigned pc;		/* program counter where to continue */
{
	register int type, i;
	unsigned ucode = 0;
	register struct proc *p = curproc;
	u_quad_t sticks;
	vm_prot_t ftype;
	extern unsigned onfault_table[];

#ifdef DEBUG
	trp->status = statusReg;
	trp->cause = causeReg;
	trp->vadr = vadr;
	trp->pc = pc;
	trp->ra = !USERMODE(statusReg) ? ((int *)&args)[19] :
		p->p_md.md_regs[RA];
	trp->sp = (int)&args;
	trp->code = 0;
	if (++trp == &trapdebug[TRAPSIZE])
		trp = trapdebug;
#endif

	cnt.v_trap++;
	type = (causeReg & MIPS_CR_EXC_CODE) >> MIPS_CR_EXC_CODE_SHIFT;
	if (USERMODE(statusReg)) {
		type |= T_USER;
		sticks = p->p_sticks;
	}

	/*
	 * Enable hardware interrupts if they were on before.
	 * We only respond to software interrupts when returning to user mode.
	 */
	if (statusReg & MIPS_SR_INT_IE)
		splx((statusReg & MIPS_HARD_INT_MASK) | MIPS_SR_INT_IE);

	switch (type) {
	case T_TLB_MOD:
		/* check for kernel address */
		if ((int)vadr < 0) {
			register pt_entry_t *pte;
			register unsigned entry;
			register vm_offset_t pa;

			pte = kvtopte(vadr);
			entry = pte->pt_entry;
#ifdef DIAGNOSTIC
			if (!(entry & PG_V) || (entry & PG_M))
				panic("trap: ktlbmod: invalid pte");
#endif
			if (pmap_is_page_ro(pmap_kernel(), pica_trunc_page(vadr), entry)) {
				/* write to read only page in the kernel */
				ftype = VM_PROT_WRITE;
				goto kernel_fault;
			}
			entry |= PG_M;
			pte->pt_entry = entry;
			vadr &= ~PGOFSET;
			MachTLBUpdate(vadr, entry);
			pa = pfn_to_vad(entry);
#ifdef ATTR
			pmap_attributes[atop(pa)] |= PMAP_ATTR_MOD;
#else
			if (!IS_VM_PHYSADDR(pa))
				panic("trap: ktlbmod: unmanaged page");
			PHYS_TO_VM_PAGE(pa)->flags &= ~PG_CLEAN;
#endif
			return (pc);
		}
		/* FALLTHROUGH */

	case T_TLB_MOD+T_USER:
	    {
		register pt_entry_t *pte;
		register unsigned entry;
		register vm_offset_t pa;
		pmap_t pmap = &p->p_vmspace->vm_pmap;

		if (!(pte = pmap_segmap(pmap, vadr)))
			panic("trap: utlbmod: invalid segmap");
		pte += (vadr >> PGSHIFT) & (NPTEPG - 1);
		entry = pte->pt_entry;
#ifdef DIAGNOSTIC
		if (!(entry & PG_V) || (entry & PG_M)) {
			panic("trap: utlbmod: invalid pte");
		}
#endif
		if (pmap_is_page_ro(pmap, pica_trunc_page(vadr), entry)) {
			/* write to read only page */
			ftype = VM_PROT_WRITE;
			goto dofault;
		}
		entry |= PG_M;
		pte->pt_entry = entry;
		vadr = (vadr & ~PGOFSET) |
			(pmap->pm_tlbpid << VMMACH_TLB_PID_SHIFT);
		MachTLBUpdate(vadr, entry);
		pa = pfn_to_vad(entry);
#ifdef ATTR
		pmap_attributes[atop(pa)] |= PMAP_ATTR_MOD;
#else
		if (!IS_VM_PHYSADDR(pa)) {
			panic("trap: utlbmod: unmanaged page");
		}
		PHYS_TO_VM_PAGE(pa)->flags &= ~PG_CLEAN;
#endif
		if (!USERMODE(statusReg))
			return (pc);
		goto out;
	    }

	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		ftype = (type == T_TLB_ST_MISS) ? VM_PROT_WRITE : VM_PROT_READ;
		/* check for kernel address */
		if ((int)vadr < 0) {
			register vm_offset_t va;
			int rv;

		kernel_fault:
			/*kernelfaults++;*/
			va = trunc_page((vm_offset_t)vadr);
			rv = vm_fault(kernel_map, va, ftype, FALSE);
			if (rv == KERN_SUCCESS)
				return (pc);
			if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
				((struct pcb *)UADDR)->pcb_onfault = 0;
				return (onfault_table[i]);
			}
			goto err;
		}
		/*
		 * It is an error for the kernel to access user space except
		 * through the copyin/copyout routines.
		 */
		if ((i = ((struct pcb *)UADDR)->pcb_onfault) == 0)
			goto err;
		/* check for fuswintr() or suswintr() getting a page fault */
		if (i == 4)
			return (onfault_table[i]);
		goto dofault;

	case T_TLB_LD_MISS+T_USER:
		ftype = VM_PROT_READ;
		goto dofault;

	case T_TLB_ST_MISS+T_USER:
		ftype = VM_PROT_WRITE;
	dofault:
	    {
		register vm_offset_t va;
		register struct vmspace *vm;
		register vm_map_t map;
		int rv;

		vm = p->p_vmspace;
		map = &vm->vm_map;
		va = trunc_page((vm_offset_t)vadr);
		rv = vm_fault(map, va, ftype, FALSE);
#ifdef VMFAULT_TRACE
		printf("vm_fault(%x (pmap %x), %x (%x), %x, %d) -> %x at pc %x\n",
		       map, &vm->vm_pmap, va, vadr, ftype, FALSE, rv, pc);
#endif
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			if (!USERMODE(statusReg))
				return (pc);
			goto out;
		}
		if (!USERMODE(statusReg)) {
			if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
				((struct pcb *)UADDR)->pcb_onfault = 0;
				return (onfault_table[i]);
			}
			goto err;
		}
		ucode = vadr;
		i = SIGSEGV;
		break;
	    }

	case T_ADDR_ERR_LD+T_USER:	/* misaligned or kseg access */
	case T_ADDR_ERR_ST+T_USER:	/* misaligned or kseg access */
	case T_BUS_ERR_IFETCH+T_USER:	/* BERR asserted to cpu */
	case T_BUS_ERR_LD_ST+T_USER:	/* BERR asserted to cpu */
		i = SIGBUS;
		break;

	case T_SYSCALL+T_USER:
	    {
		register int *locr0 = p->p_md.md_regs;
		register struct sysent *callp;
		unsigned int code;
		int numsys;
		struct args {
			int i[8];
		} args;
		int rval[2];

		cnt.v_syscall++;
		/* compute next PC after syscall instruction */
		if ((int)causeReg < 0)
			locr0[PC] = MachEmulateBranch(locr0, pc, 0, 0);
		else
			locr0[PC] += 4;
		callp = p->p_emul->e_sysent;
		numsys = p->p_emul->e_nsysent;
		code = locr0[V0];
		switch (code) {
		case SYS_syscall:
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = locr0[A0];
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(int);
			args.i[0] = locr0[A1];
			args.i[1] = locr0[A2];
			args.i[2] = locr0[A3];
			if (i > 3) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[3],
					(u_int)(i - 3) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
			break;

		case SYS___syscall:
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 */
			code = locr0[A0 + _QUAD_LOWWORD];
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(int);
			args.i[0] = locr0[A2];
			args.i[1] = locr0[A3];
			if (i > 2) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[2],
					(u_int)(i - 2) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
			break;

		default:
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_narg;
			args.i[0] = locr0[A0];
			args.i[1] = locr0[A1];
			args.i[2] = locr0[A2];
			args.i[3] = locr0[A3];
			if (i > 4) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[4],
					(u_int)(i - 4) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
		}
#ifdef SYSCALL_DEBUG
		scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, callp->sy_argsize, args.i);
#endif
		rval[0] = 0;
		rval[1] = locr0[V1];
#ifdef DEBUG
		if (trp == trapdebug)
			trapdebug[TRAPSIZE - 1].code = code;
		else
			trp[-1].code = code;
#endif
		i = (*callp->sy_call)(p, &args, rval);
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		locr0 = p->p_md.md_regs;
#ifdef DEBUG
		{ int s;
		s = splhigh();
		trp->status = statusReg;
		trp->cause = causeReg;
		trp->vadr = locr0[SP];
		trp->pc = locr0[PC];
		trp->ra = locr0[RA];
		/*trp->sp = (int)&args;*/	/* XXX */
		trp->code = -code;
		if (++trp == &trapdebug[TRAPSIZE])
			trp = trapdebug;
		splx(s);
		}
#endif
		switch (i) {
		case 0:
			locr0[V0] = rval[0];
			locr0[V1] = rval[1];
			locr0[A3] = 0;
			break;

		case ERESTART:
			locr0[PC] = pc;
			break;

		case EJUSTRETURN:
			break;	/* nothing to do */

		default:
			locr0[V0] = i;
			locr0[A3] = 1;
		}

		/*
		 * If we modified code or data, flush caches.
		 * XXX code unyderling ptrace() and/or proc fs should do this?
		 */
		if(code == SYS_ptrace)
			MachFlushCache();
	done:
#ifdef SYSCALL_DEBUG
		scdebug_ret(p, code, i, rval);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSRET))
			ktrsysret(p->p_tracep, code, i, rval[0]);
#endif
		goto out;
	    }

	case T_BREAK+T_USER:
	    {
		register unsigned va, instr;
		struct uio uio;
		struct iovec iov;

		/* compute address of break instruction */
		va = pc;
		if ((int)causeReg < 0)
			va += 4;

		/* read break instruction */
		instr = fuiword((caddr_t)va);
#if 0
		printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			p->p_comm, p->p_pid, instr, pc,
			p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
		if (p->p_md.md_ss_addr != va || instr != MIPS_BREAK_SSTEP) {
			i = SIGTRAP;
			break;
		}

		/*
		 * Restore original instruction and clear BP
		 */
		iov.iov_base = (caddr_t)&p->p_md.md_ss_instr;
		iov.iov_len = sizeof(int); 
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1; 
		uio.uio_offset = (off_t)va;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_WRITE;
		uio.uio_procp = curproc;
		i = procfs_domem(p, p, NULL, &uio);
		MachFlushCache();

		if (i < 0)
			printf("Warning: can't restore instruction at %x: %x\n",
				p->p_md.md_ss_addr, p->p_md.md_ss_instr);

		p->p_md.md_ss_addr = 0;
		i = SIGTRAP;
		break;
	    }

	case T_RES_INST+T_USER:
		i = SIGILL;
		break;

	case T_COP_UNUSABLE+T_USER:
		if ((causeReg & MIPS_CR_COP_ERR) != 0x10000000) {
			i = SIGILL;	/* only FPU instructions allowed */
			break;
		}
		MachSwitchFPState(machFPCurProcPtr,
				  (struct user*)p->p_md.md_regs);
		machFPCurProcPtr = p;
		p->p_md.md_regs[PS] |= MIPS_SR_COP_1_BIT;
		p->p_md.md_flags |= MDP_FPUSED;
		goto out;

	case T_FPE:
#ifdef DEBUG
		trapDump("fpintr");
#else
		printf("FPU Trap: PC %x CR %x SR %x\n",
			pc, causeReg, statusReg);
		goto err;
#endif

	case T_FPE+T_USER:
		MachFPTrap(statusReg, causeReg, pc);
		goto out;

	case T_OVFLOW+T_USER:
		i = SIGFPE;
		break;

	case T_ADDR_ERR_LD:	/* misaligned access */
	case T_ADDR_ERR_ST:	/* misaligned access */
	case T_BUS_ERR_LD_ST:	/* BERR asserted to cpu */
		if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
			((struct pcb *)UADDR)->pcb_onfault = 0;
			return (onfault_table[i]);
		}
		/* FALLTHROUGH */

	default:
	err:
#ifdef DEBUG
	    {
		extern struct pcb mdbpcb;

		if (USERMODE(statusReg))
			mdbpcb = p->p_addr->u_pcb;
		else {
			mdbpcb.pcb_regs[ZERO] = 0;
			mdbpcb.pcb_regs[AST] = ((int *)&args)[2];
			mdbpcb.pcb_regs[V0] = ((int *)&args)[3];
			mdbpcb.pcb_regs[V1] = ((int *)&args)[4];
			mdbpcb.pcb_regs[A0] = ((int *)&args)[5];
			mdbpcb.pcb_regs[A1] = ((int *)&args)[6];
			mdbpcb.pcb_regs[A2] = ((int *)&args)[7];
			mdbpcb.pcb_regs[A3] = ((int *)&args)[8];
			mdbpcb.pcb_regs[T0] = ((int *)&args)[9];
			mdbpcb.pcb_regs[T1] = ((int *)&args)[10];
			mdbpcb.pcb_regs[T2] = ((int *)&args)[11];
			mdbpcb.pcb_regs[T3] = ((int *)&args)[12];
			mdbpcb.pcb_regs[T4] = ((int *)&args)[13];
			mdbpcb.pcb_regs[T5] = ((int *)&args)[14];
			mdbpcb.pcb_regs[T6] = ((int *)&args)[15];
			mdbpcb.pcb_regs[T7] = ((int *)&args)[16];
			mdbpcb.pcb_regs[T8] = ((int *)&args)[17];
			mdbpcb.pcb_regs[T9] = ((int *)&args)[18];
			mdbpcb.pcb_regs[RA] = ((int *)&args)[19];
			mdbpcb.pcb_regs[MULLO] = ((int *)&args)[21];
			mdbpcb.pcb_regs[MULHI] = ((int *)&args)[22];
			mdbpcb.pcb_regs[PC] = pc;
			mdbpcb.pcb_regs[SR] = statusReg;
			bzero((caddr_t)&mdbpcb.pcb_regs[F0], 33 * sizeof(int));
		}
		if (mdb(causeReg, vadr, p, !USERMODE(statusReg)))
			return (mdbpcb.pcb_regs[PC]);
	    }
#else
#ifdef DEBUG
		stacktrace();
		trapDump("trap");
#endif
#endif
		panic("trap");
	}
	p->p_md.md_regs [PC] = pc;
	p->p_md.md_regs [CAUSE] = causeReg;
	p->p_md.md_regs [BADVADDR] = vadr;
	trapsignal(p, i, ucode);
out:
	/*
	 * Note: we should only get here if returning to user mode.
	 */
	/* take pending signals */
	while ((i = CURSIG(p)) != 0)
		postsig(i);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (want_resched) {
		int s;

		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((i = CURSIG(p)) != 0)
			postsig(i);
	}

	/*
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - sticks) * psratio);
	}

	curpriority = p->p_priority;
	return (pc);
}

/*
 * Handle an interrupt.
 * Called from MachKernIntr() or MachUserIntr()
 * Note: curproc might be NULL.
 */
void
interrupt(statusReg, causeReg, pc, what, args)
	unsigned statusReg;	/* status register at time of the exception */
	unsigned causeReg;	/* cause register at time of exception */
	unsigned pc;		/* program counter where to continue */
{
	register unsigned mask;
	register int i;
	struct clockframe cf;

#ifdef DEBUG
	trp->status = statusReg;
	trp->cause = causeReg;
	trp->vadr = 0;
	trp->pc = pc;
	trp->ra = 0;
	trp->sp = (int)&args;
	trp->code = 0;
	if (++trp == &trapdebug[TRAPSIZE])
		trp = trapdebug;
#endif

	cnt.v_intr++;
	mask = causeReg & statusReg;	/* pending interrupts & enable mask */

	splx(pica_hardware_intr(mask, pc, statusReg, causeReg));


	if (mask & MIPS_SOFT_INT_MASK_0) {
		clearsoftclock();
		cnt.v_soft++;
		softclock();
	}
	/*
	 *  Process network interrupt if we trapped or will very soon
	 */
	if ((mask & MIPS_SOFT_INT_MASK_1) ||
	    netisr && (statusReg & MIPS_SOFT_INT_MASK_1)) {
		clearsoftnet();
		cnt.v_soft++;
		intrcnt[1]++;
#ifdef INET
#include "arp.h"
#if NARP > 0
		if (netisr & (1 << NETISR_ARP)) {
			netisr &= ~(1 << NETISR_ARP);
			arpintr();
		}
#endif
		if (netisr & (1 << NETISR_IP)) {
			netisr &= ~(1 << NETISR_IP);
			ipintr();
		}
#endif
#ifdef NETATALK
		if (netisr & (1 << NETISR_ATALK)) {
			netisr &= ~(1 << NETISR_ATALK);
			atintr();
		}
#endif
#ifdef NS
		if (netisr & (1 << NETISR_NS)) {
			netisr &= ~(1 << NETISR_NS);
			nsintr();
		}
#endif
#ifdef ISO
		if (netisr & (1 << NETISR_ISO)) {
			netisr &= ~(1 << NETISR_ISO);
			clnlintr();
		}
#endif
#include "ppp.h"
#if NPPP > 0
		if (netisr & (1 << NETISR_PPP)) {
			netisr &= ~(1 << NETISR_PPP);
			pppintr();
		}
#endif
	}

	if (mask & MIPS_SOFT_INT_MASK_0) {
		clearsoftclock();
		intrcnt[0]++;
		cnt.v_soft++;
		softclock();
	}
}


/*
 * This is called from MachUserIntr() if astpending is set.
 * This is very similar to the tail of trap().
 */
void
softintr(statusReg, pc)
	unsigned statusReg;	/* status register at time of the exception */
	unsigned pc;		/* program counter where to continue */
{
	register struct proc *p = curproc;
	int sig;

	cnt.v_soft++;
	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}
	if (want_resched) {
		int s;

		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	curpriority = p->p_priority;
}

#ifdef DEBUG
void
trapDump(msg)
	char *msg;
{
	register int i;
	int s;

	s = splhigh();
	printf("trapDump(%s)\n", msg);
	for (i = 0; i < TRAPSIZE; i++) {
		if (trp == trapdebug)
			trp = &trapdebug[TRAPSIZE - 1];
		else
			trp--;
		if (trp->cause == 0)
			break;
		printf("%s: ADR %x PC %x CR %x SR %x\n",
			trap_type[(trp->cause & MIPS_CR_EXC_CODE) >>
				MIPS_CR_EXC_CODE_SHIFT],
			trp->vadr, trp->pc, trp->cause, trp->status);
		printf("   RA %x SP %x code %d\n", trp->ra, trp->sp, trp->code);
	}
	splx(s);
}
#endif


/*
 * Return the resulting PC as if the branch was executed.
 */
unsigned
MachEmulateBranch(regsPtr, instPC, fpcCSR, allowNonBranch)
	unsigned *regsPtr;
	unsigned instPC;
	unsigned fpcCSR;
	int allowNonBranch;
{
	InstFmt inst;
	unsigned retAddr;
	int condition;

#define GetBranchDest(InstPtr, inst) \
	((unsigned)InstPtr + 4 + ((short)inst.IType.imm << 2))


	if(allowNonBranch == 0) {
		inst = *(InstFmt *)instPC;
	}
	else {
		inst = *(InstFmt *)&allowNonBranch;
	}
#if 0
	printf("regsPtr=%x PC=%x Inst=%x fpcCsr=%x\n", regsPtr, instPC,
		inst.word, fpcCSR); /* XXX */
#endif
	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
		case OP_JALR:
			retAddr = regsPtr[inst.RType.rs];
			break;

		default:
			if (!allowNonBranch)
				panic("MachEmulateBranch: Non-branch");
			retAddr = instPC + 4;
			break;
		}
		break;

	case OP_BCOND:
		switch ((int)inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BLTZAL:
		case OP_BLTZALL:
			if ((int)(regsPtr[inst.RType.rs]) < 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		case OP_BGEZ:
		case OP_BGEZL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			if ((int)(regsPtr[inst.RType.rs]) >= 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		default:
			panic("MachEmulateBranch: Bad branch cond");
		}
		break;

	case OP_J:
	case OP_JAL:
		retAddr = (inst.JType.target << 2) | 
			((unsigned)instPC & 0xF0000000);
		break;

	case OP_BEQ:
	case OP_BEQL:
		if (regsPtr[inst.RType.rs] == regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BNE:
	case OP_BNEL:
		if (regsPtr[inst.RType.rs] != regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BLEZ:
	case OP_BLEZL:
		if ((int)(regsPtr[inst.RType.rs]) <= 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BGTZ:
	case OP_BGTZL:
		if ((int)(regsPtr[inst.RType.rs]) > 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			if ((inst.RType.rt & COPz_BC_TF_MASK) == COPz_BC_TRUE)
				condition = fpcCSR & MIPS_FPU_COND_BIT;
			else
				condition = !(fpcCSR & MIPS_FPU_COND_BIT);
			if (condition)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		default:
			if (!allowNonBranch)
				panic("MachEmulateBranch: Bad coproc branch instruction");
			retAddr = instPC + 4;
		}
		break;

	default:
		if (!allowNonBranch)
			panic("MachEmulateBranch: Non-branch instruction");
		retAddr = instPC + 4;
	}
#if 0
	printf("Target addr=%x\n", retAddr); /* XXX */
#endif
	return (retAddr);
}

/*
 * This routine is called by procxmt() to single step one instruction.
 * We do this by storing a break instruction after the current instruction,
 * resuming execution, and then restoring the old instruction.
 */
int
cpu_singlestep(p)
	register struct proc *p;
{
	register unsigned va;
	register int *locr0 = p->p_md.md_regs;
	int i;
	int bpinstr = MIPS_BREAK_SSTEP;
	int curinstr;
	struct uio uio;
	struct iovec iov;

	/*
	 * Fetch what's at the current location.
	 */
	iov.iov_base = (caddr_t)&curinstr;
	iov.iov_len = sizeof(int); 
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1; 
	uio.uio_offset = (off_t)locr0[PC];
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);

	/* compute next address after current location */
	if(curinstr != 0) {
		va = MachEmulateBranch(locr0, locr0[PC], locr0[FSR], curinstr);
	}
	else {
		va = locr0[PC] + 4;
	}
	if (p->p_md.md_ss_addr) {
		printf("SS %s (%d): breakpoint already set at %x (va %x)\n",
			p->p_comm, p->p_pid, p->p_md.md_ss_addr, va); /* XXX */
		return (EFAULT);
	}
	p->p_md.md_ss_addr = va;

	/*
	 * Fetch what's at the current location.
	 */
	iov.iov_base = (caddr_t)&p->p_md.md_ss_instr;
	iov.iov_len = sizeof(int); 
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1; 
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);

	/*
	 * Store breakpoint instruction at the "next" location now.
	 */
	iov.iov_base = (caddr_t)&bpinstr;
	iov.iov_len = sizeof(int); 
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1; 
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = curproc;
	i = procfs_domem(curproc, p, NULL, &uio);
	MachFlushCache(); /* XXX memory barrier followed by flush icache? */

	if (i < 0)
		return (EFAULT);
#if 0
	printf("SS %s (%d): breakpoint set at %x: %x (pc %x) br %x\n",
		p->p_comm, p->p_pid, p->p_md.md_ss_addr,
		p->p_md.md_ss_instr, locr0[PC], curinstr); /* XXX */
#endif
	return (0);
}

#ifdef DEBUG
int
kdbpeek(addr)
{
	if (addr & 3) {
		printf("kdbpeek: unaligned address %x\n", addr);
		return (-1);
	}
	return (*(int *)addr);
}
#endif

#ifdef DEBUG
#define MIPS_JR_RA	0x03e00008	/* instruction code for jr ra */

/* forward */
char *fn_name(unsigned addr);
void stacktrace_subr __P((int, int, int, int, void (*)(const char*, ...)));

/*
 * Print a stack backtrace.
 */
void
stacktrace(a0, a1, a2, a3)
	int a0, a1, a2, a3;
{
	stacktrace_subr(a0, a1, a2, a3, printf);
}

void
logstacktrace(a0, a1, a2, a3)
	int a0, a1, a2, a3;
{
	stacktrace_subr(a0, a1, a2, a3, addlog);
}

void
stacktrace_subr(a0, a1, a2, a3, printfn)
	int a0, a1, a2, a3;
	void (*printfn) __P((const char*, ...));
{
	unsigned pc, sp, fp, ra, va, subr;
	unsigned instr, mask;
	InstFmt i;
	int more, stksize;
	int regs[3];
	extern char start[], edata[];
	unsigned int frames =  0;

	cpu_getregs(regs);

	/* get initial values from the exception frame */
	sp = regs[0];
	pc = regs[1];
	ra = 0;
	fp = regs[2];

/* Jump here when done with a frame, to start a new one */
loop:
	ra = 0;

/* Jump here after a nonstandard (interrupt handler) frame */
specialframe:
	stksize = 0;
	subr = 0;
	if	(frames++ > 100) {
		(*printfn)("\nstackframe count exceeded\n");
		/* return breaks stackframe-size heuristics with gcc -O2 */
		goto finish;	/*XXX*/
	}

	/* check for bad SP: could foul up next frame */
	if (sp & 3 || sp < 0x80000000) {
		(*printfn)("SP 0x%x: not in kernel\n", sp);
		ra = 0;
		subr = 0;
		goto done;
	}

/*
 * check for PC between two entry points
 */
# define Between(x, y, z) \
		( ((x) <= (y)) && ((y) < (z)) )
# define pcBetween(a,b) \
		Between((unsigned)a, pc, (unsigned)b)


	/* Backtraces should contine through interrupts from kernel mode */
#ifdef CPU_R3000
	if (pcBetween(mips_r2000_KernIntr, mips_r2000_UserIntr)) {
		/* NOTE: the offsets depend on the code in locore.s */
		(*printfn)("r3000 KernIntr+%x: (%x, %x ,%x) -------\n",
		       pc-(unsigned)mips_r2000_KernIntr, a0, a1, a2);
		a0 = kdbpeek(sp + 36);
		a1 = kdbpeek(sp + 40);
		a2 = kdbpeek(sp + 44);
		a3 = kdbpeek(sp + 48);

		pc = kdbpeek(sp + 20);	/* exc_pc - pc at time of exception */
		ra = kdbpeek(sp + 92);	/* ra at time of exception */
		sp = sp + 108;
		goto specialframe;
	}
#endif	/* CPU_R3000 */

#ifdef CPU_R4000
	if (pcBetween(mips_r4000_KernIntr, mips_r4000_UserIntr)) {
		/* NOTE: the offsets depend on the code in locore.s */
		(*printfn)("R4000 KernIntr+%x: (%x, %x ,%x) -------\n",
		       pc-(unsigned)mips_r4000_KernIntr, a0, a1, a2);
		a0 = kdbpeek(sp + 36);
		a1 = kdbpeek(sp + 40);
		a2 = kdbpeek(sp + 44);
		a3 = kdbpeek(sp + 48);

		pc = kdbpeek(sp + 20);	/* exc_pc - pc at time of exception */
		ra = kdbpeek(sp + 92);	/* ra at time of exception */
		sp = sp + 108;
		goto specialframe;
	}
#endif	/* cpu_r4000 */



	/*
	 * Check for current PC in  exception handler code that don't
	 * have a preceding "j ra" at the tail of the preceding function. 
	 * Depends on relative ordering of functions in locore.
	 */

	/* XXX fixup tests after cutting and pasting in locore.S */
	/* R4000  exception handlers */

#ifdef CPU_R2000
	if (pcBetween(mips_r2000_KernGenException, mips_r2000_UserGenException))
		subr = (unsigned) mips_r2000_KernGenException;
	else if (pcBetween(mips_r2000_UserGenException,mips_r2000_KernIntr))
		subr = (unsigned) mips_r2000_UserGenException;
	else if (pcBetween(mips_r2000_KernIntr, mips_r2000_UserIntr))
		subr = (unsigned) mips_r2000_KernIntr;
	else if (pcBetween(mips_r2000_UserIntr, mips_r2000_TLBMissException))
		subr = (unsigned) mips_r2000_UserIntr;

	else if (pcBetween(mips_r2000_UserIntr, mips_r2000_TLBMissException))
		subr = (unsigned) mips_r2000_UserIntr;
	else
#endif /* CPU_R2000 */


#ifdef CPU_R4000
	/* R4000  exception handlers */
	if (pcBetween(mips_r4000_KernGenException, mips_r4000_UserGenException))
		subr = (unsigned) mips_r4000_KernGenException;
	else if (pcBetween(mips_r4000_UserGenException,mips_r4000_KernIntr))
		subr = (unsigned) mips_r4000_UserGenException;
	else if (pcBetween(mips_r4000_KernIntr, mips_r4000_UserIntr))
		subr = (unsigned) mips_r4000_KernIntr;


	else if (pcBetween(mips_r4000_UserIntr, mips_r4000_TLBMissException))
		subr = (unsigned) mips_r4000_UserIntr;
	else
#endif /* CPU_R4000 */


	if (pcBetween(splx, wbflush))
		subr = (unsigned) splx;
	else if (pcBetween(cpu_switch, fuword))
		subr = (unsigned) cpu_switch;
	else if (pcBetween(idle, cpu_switch))	{
		subr = (unsigned) idle;
		ra = 0;
		goto done;
	}
#ifdef notyet /* XXX FIXME: the order changed with merged locore */
	else if (pc >= (unsigned)MachUTLBMiss && pc < (unsigned)setsoftclock) {
		(*printfn)("<<locore>>");
		goto done;
	}
#endif	/* notyet */

	/* check for bad PC */
	if (pc & 3 || pc < 0x80000000 || pc >= (unsigned)edata) {
		(*printfn)("PC 0x%x: not in kernel space\n", pc);
		ra = 0;
		goto done;
	}
	if (!pcBetween(start, (unsigned) edata)) {
		(*printfn)("PC 0x%x: not in kernel text\n", pc);
		ra = 0;
		goto done;
	}

	/*
	 * Find the beginning of the current subroutine by scanning backwards
	 * from the current PC for the end of the previous subroutine.
	 */
	if (!subr) {
		va = pc - sizeof(int);
		while ((instr = kdbpeek(va)) != MIPS_JR_RA)
		va -= sizeof(int);
		va += 2 * sizeof(int);	/* skip back over branch & delay slot */
		/* skip over nulls which might separate .o files */
		while ((instr = kdbpeek(va)) == 0)
			va += sizeof(int);
		subr = va;
	}

	/*
	 * Jump here for locore entry pointsn for which the preceding
	 * function doesn't end in "j ra"
	 */
#if 0
stackscan:
#endif
	/* scan forwards to find stack size and any saved registers */
	stksize = 0;
	more = 3;
	mask = 0;
	for (va = subr; more; va += sizeof(int),
	     		      more = (more == 3) ? 3 : more - 1) {
		/* stop if hit our current position */
		if (va >= pc)
			break;
		instr = kdbpeek(va);
		i.word = instr;
		switch (i.JType.op) {
		case OP_SPECIAL:
			switch (i.RType.func) {
			case OP_JR:
			case OP_JALR:
				more = 2; /* stop after next instruction */
				break;

			case OP_SYSCALL:
			case OP_BREAK:
				more = 1; /* stop now */
			};
			break;

		case OP_BCOND:
		case OP_J:
		case OP_JAL:
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
			more = 2; /* stop after next instruction */
			break;

		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			switch (i.RType.rs) {
			case OP_BCx:
			case OP_BCy:
				more = 2; /* stop after next instruction */
			};
			break;

		case OP_SW:
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case 4: /* a0 */
				a0 = kdbpeek(sp + (short)i.IType.imm);
				break;

			case 5: /* a1 */
				a1 = kdbpeek(sp + (short)i.IType.imm);
				break;

			case 6: /* a2 */
				a2 = kdbpeek(sp + (short)i.IType.imm);
				break;

			case 7: /* a3 */
				a3 = kdbpeek(sp + (short)i.IType.imm);
				break;

			case 30: /* fp */
				fp = kdbpeek(sp + (short)i.IType.imm);
				break;

			case 31: /* ra */
				ra = kdbpeek(sp + (short)i.IType.imm);
			}
			break;

		case OP_ADDI:
		case OP_ADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != 29 || i.IType.rt != 29)
				break;
			stksize = - ((short)i.IType.imm);
		}
	}

done:
	(*printfn)("%s+%x (%x,%x,%x,%x) ra %x sz %d\n",
		fn_name(subr), pc - subr, a0, a1, a2, a3, ra, stksize);

	if (ra) {
		if (pc == ra && stksize == 0)
			(*printfn)("stacktrace: loop!\n");
		else {
			pc = ra;
			sp += stksize;
			ra = 0;
			goto loop;
		}
	} else {
finish:
		if (curproc)
			(*printfn)("User-level: pid %d\n", curproc->p_pid);
		else
			(*printfn)("User-level: curproc NULL\n");
	}
}

/*
 * Functions ``special'' enough to print by name
 */
#ifdef __STDC__
#define Name(_fn)  { (void*)_fn, # _fn }
#else
#define Name(_fn) { _fn, "_fn"}
#endif
static struct { void *addr; char *name;} names[] = {
	Name(stacktrace),
	Name(stacktrace_subr),
	Name(main),
	Name(interrupt),
	Name(trap),
#ifdef pmax
	Name(am7990_meminit),
#endif
#ifdef CPU_R3000
	Name(mips_r2000_KernGenException),
	Name(mips_r2000_UserGenException),
	Name(mips_r2000_KernIntr),
	Name(mips_r2000_UserIntr),
#endif	/* CPU_R3000 */

#ifdef CPU_R4000
	Name(mips_r4000_KernGenException),
	Name(mips_r4000_UserGenException),
	Name(mips_r4000_KernIntr),
	Name(mips_r4000_UserIntr),
#endif	/* CPU_R4000 */

	Name(splx),
	Name(idle),
	Name(cpu_switch),
	{0, 0}
};

/*
 * Map a function address to a string name, if known; or a hex string.
 */
char *
fn_name(unsigned addr)
{
	static char buf[17];
	int i = 0;

	for (i = 0; names[i].name; i++)
		if (names[i].addr == (void*)addr)
			return (names[i].name);
	sprintf(buf, "%x", addr);
	return (buf);
}

#endif /* DEBUG */
