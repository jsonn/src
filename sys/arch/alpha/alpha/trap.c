/* $NetBSD: trap.c,v 1.49.8.1 1999/12/27 18:31:23 wrstuden Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#include "opt_fix_unaligned_vax_fp.h"
#include "opt_ktrace.h"
#include "opt_compat_osf1.h"
#include "opt_ddb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: trap.c,v 1.49.8.1 1999/12/27 18:31:23 wrstuden Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/buf.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/alpha.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <alpha/alpha/db_instruction.h>		/* for handle_opdec() */

#ifdef COMPAT_OSF1
#include <compat/osf1/osf1_syscall.h>
#endif

void		userret __P((struct proc *, u_int64_t, u_quad_t));

unsigned long	Sfloat_to_reg __P((unsigned int));
unsigned int	reg_to_Sfloat __P((unsigned long));
unsigned long	Tfloat_reg_cvt __P((unsigned long));
#ifdef FIX_UNALIGNED_VAX_FP
unsigned long	Ffloat_to_reg __P((unsigned int));
unsigned int	reg_to_Ffloat __P((unsigned long));
unsigned long	Gfloat_reg_cvt __P((unsigned long));
#endif

int		unaligned_fixup __P((unsigned long, unsigned long,
		    unsigned long, struct proc *));
int		handle_opdec(struct proc *p, u_int64_t *ucodep);

static void printtrap __P((const unsigned long, const unsigned long,
      const unsigned long, const unsigned long, struct trapframe *, int, int));

/*
 * Initialize the trap vectors for the current processor.
 */
void
trap_init()
{

	/*
	 * Point interrupt/exception vectors to our own.
	 */
	alpha_pal_wrent(XentInt, ALPHA_KENTRY_INT); 
	alpha_pal_wrent(XentArith, ALPHA_KENTRY_ARITH);
	alpha_pal_wrent(XentMM, ALPHA_KENTRY_MM);
	alpha_pal_wrent(XentIF, ALPHA_KENTRY_IF);
	alpha_pal_wrent(XentUna, ALPHA_KENTRY_UNA); 
	alpha_pal_wrent(XentSys, ALPHA_KENTRY_SYS);

	/*
	 * Clear pending machine checks and error reports, and enable
	 * system- and processor-correctable error reporting.
	 */
	alpha_pal_wrmces(alpha_pal_rdmces() & 
	    ~(ALPHA_MCES_DSC|ALPHA_MCES_DPC));
}

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
void
userret(p, pc, oticks)
	register struct proc *p;
	u_int64_t pc;
	u_quad_t oticks;
{
	int sig, s;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
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

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}

	curpriority = p->p_priority;
}

static void
printtrap(a0, a1, a2, entry, framep, isfatal, user)
	const unsigned long a0, a1, a2, entry;
	struct trapframe *framep;
	int isfatal, user;
{
	char ubuf[64];
	const char *entryname;

	switch (entry) {
	case ALPHA_KENTRY_INT:
		entryname = "interrupt";
		break;
	case ALPHA_KENTRY_ARITH:
		entryname = "arithmetic trap";
		break;
	case ALPHA_KENTRY_MM:
		entryname = "memory management fault";
		break;
	case ALPHA_KENTRY_IF:
		entryname = "instruction fault";
		break;
	case ALPHA_KENTRY_UNA:
		entryname = "unaligned access fault";
		break;
	case ALPHA_KENTRY_SYS:
		entryname = "system call";
		break;
	default:
		sprintf(ubuf, "type %lx", entry);
		entryname = (const char *) ubuf;
		break;
	}

	printf("\n");
	printf("%s %s trap:\n", isfatal? "fatal" : "handled",
	       user ? "user" : "kernel");
	printf("\n");
	printf("    trap entry = 0x%lx (%s)\n", entry, entryname);
	printf("    a0         = 0x%lx\n", a0);
	printf("    a1         = 0x%lx\n", a1);
	printf("    a2         = 0x%lx\n", a2);
	printf("    pc         = 0x%lx\n", framep->tf_regs[FRAME_PC]);
	printf("    ra         = 0x%lx\n", framep->tf_regs[FRAME_RA]);
	printf("    curproc    = %p\n", curproc);
	if (curproc != NULL)
		printf("        pid = %d, comm = %s\n", curproc->p_pid,
		       curproc->p_comm);
	printf("\n");
}

/*
 * Trap is called from locore to handle most types of processor traps.
 * System calls are broken out for efficiency and ASTs are broken out
 * to make the code a bit cleaner and more representative of the
 * Alpha architecture.
 */
/*ARGSUSED*/
void
trap(a0, a1, a2, entry, framep)
	const unsigned long a0, a1, a2, entry;
	struct trapframe *framep;
{
	register struct proc *p;
	register int i;
	u_int64_t ucode;
	u_quad_t sticks;
	int user;
#if defined(DDB)
	int call_debugger = 1;
#endif

	uvmexp.traps++;
	p = curproc;
	ucode = 0;
	user = (framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) != 0;
	if (user)  {
		sticks = p->p_sticks;
		p->p_md.md_tf = framep;
#if	0
/* This is to catch some wierd stuff on the UDB (mj) */
		if (framep->tf_regs[FRAME_PC] > 0 && 
		    framep->tf_regs[FRAME_PC] < 0x120000000) {
			printf("PC Out of Whack\n");
			printtrap(a0, a1, a2, entry, framep, 1, user);
		}
#endif
	} else {
		sticks = 0;		/* XXX bogus -Wuninitialized warning */
	}

	switch (entry) {
	case ALPHA_KENTRY_UNA:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			if ((i = unaligned_fixup(a0, a1, a2, p)) == 0)
				goto out;

			ucode = a0;		/* VA */
			break;
		}

		/*
		 * Unaligned access from kernel mode is always an error,
		 * EVEN IF A COPY FAULT HANDLER IS SET!
		 *
		 * It's an error if a copy fault handler is set because
		 * the various routines which do user-initiated copies
		 * do so in a bcopy-like manner.  In other words, the
		 * kernel never assumes that pointers provided by the
		 * user are properly aligned, and so if the kernel
		 * does cause an unaligned access it's a kernel bug.
		 */
		goto dopanic;

	case ALPHA_KENTRY_ARITH:
		/* 
		 * If user-land, just give a SIGFPE.  Should do
		 * software completion and IEEE handling, if the
		 * user has requested that.
		 */
		if (user) {
#ifdef COMPAT_OSF1
			extern struct emul emul_osf1;

			/* just punt on OSF/1.  XXX THIS IS EVIL */
			if (p->p_emul == &emul_osf1) 
				goto out;
#endif
			i = SIGFPE;
			ucode =  a0;		/* exception summary */
			break;
		}

		/* Always fatal in kernel.  Should never happen. */
		goto dopanic;

	case ALPHA_KENTRY_IF:
		/*
		 * These are always fatal in kernel, and should never
		 * happen.  (Debugger entry is handled in XentIF.)
		 */
		if (!user) {
#if defined(DDB)
			/*
			 * ...unless a debugger is configured.  It will
			 * inform us if the trap was handled.
			 */
			if (alpha_debug(a0, a1, a2, entry, framep))
				goto out;

			/*
			 * Debugger did NOT handle the trap, don't
			 * call the debugger again!
			 */
			call_debugger = 0;
#endif
			goto dopanic;
		}
		i = 0;
		switch (a0) {
		case ALPHA_IF_CODE_GENTRAP:
			if (framep->tf_regs[FRAME_A0] == -2) { /* weird! */
				i = SIGFPE;
				ucode =  a0;	/* exception summary */
				break;
			}
			/* FALLTHROUTH */
		case ALPHA_IF_CODE_BPT:
		case ALPHA_IF_CODE_BUGCHK:
			ucode = a0;		/* trap type */
			i = SIGTRAP;
			break;

		case ALPHA_IF_CODE_OPDEC:
			if ((i = handle_opdec(p, &ucode)) == 0)
				goto out;
			break;

		case ALPHA_IF_CODE_FEN:
			/*
			 * on exit from the kernel, if proc == fpcurproc,
			 * FP is enabled.
			 */
			if (fpcurproc == p) {
				printf("trap: fp disabled for fpcurproc == %p",
				    p);
				goto dopanic;
			}
	
			alpha_pal_wrfen(1);
			if (fpcurproc)
				savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
			fpcurproc = p;
			restorefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
			alpha_pal_wrfen(0);

			p->p_md.md_flags |= MDP_FPUSED;
			goto out;

		default:
			printf("trap: unknown IF type 0x%lx\n", a0);
			goto dopanic;
		}
		break;

	case ALPHA_KENTRY_MM:
		switch (a1) {
		case ALPHA_MMCSR_FOR:
		case ALPHA_MMCSR_FOE:
			pmap_emulate_reference(p, a0, user, 0);
			goto out;

		case ALPHA_MMCSR_FOW:
			pmap_emulate_reference(p, a0, user, 1);
			goto out;

		case ALPHA_MMCSR_INVALTRANS:
		case ALPHA_MMCSR_ACCESS:
	    	{
			register vaddr_t va;
			register struct vmspace *vm = NULL;
			register vm_map_t map;
			vm_prot_t ftype;
			int rv;
			extern vm_map_t kernel_map;

			/*
			 * If it was caused by fuswintr or suswintr,
			 * just punt.  Note that we check the faulting
			 * address against the address accessed by
			 * [fs]uswintr, in case another fault happens
			 * when they are running.
			 */
			if (!user &&
			    p != NULL &&
			    p->p_addr->u_pcb.pcb_onfault ==
			      (unsigned long)fswintrberr &&
			    p->p_addr->u_pcb.pcb_accessaddr == a0) {
				framep->tf_regs[FRAME_PC] =
				    p->p_addr->u_pcb.pcb_onfault;
				p->p_addr->u_pcb.pcb_onfault = 0;
				goto out;
			}

			/*
			 * It is only a kernel address space fault iff:
			 *	1. !user and
			 *	2. pcb_onfault not set or
			 *	3. pcb_onfault set but kernel space data fault
			 * The last can occur during an exec() copyin where the
			 * argument space is lazy-allocated.
			 */
			if (!user && (a0 >= VM_MIN_KERNEL_ADDRESS ||
			    p == NULL || p->p_addr->u_pcb.pcb_onfault == 0))
				map = kernel_map;
			else {
				vm = p->p_vmspace;
				map = &vm->vm_map;
			}
	
			switch (a2) {
			case -1:		/* instruction fetch fault */
			case 0:			/* load instruction */
				ftype = VM_PROT_READ;
				break;
			case 1:			/* store instruction */
				ftype = VM_PROT_WRITE;
				break;
#ifdef DIAGNOSTIC
			default:		/* XXX gcc -Wuninitialized */
				goto dopanic;
#endif
			}
	
			va = trunc_page((vaddr_t)a0);
			rv = uvm_fault(map, va, 0, ftype);
			/*
			 * If this was a stack access we keep track of the
			 * maximum accessed stack size.  Also, if vm_fault
			 * gets a protection failure it is due to accessing
			 * the stack region outside the current limit and
			 * we need to reflect that as an access error.
			 */
			if (map != kernel_map &&
			    (caddr_t)va >= vm->vm_maxsaddr &&
			    va < USRSTACK) {
				if (rv == KERN_SUCCESS) {
					unsigned nss;
	
					nss = btoc(USRSTACK -
					    (unsigned long)va);
					if (nss > vm->vm_ssize)
						vm->vm_ssize = nss;
				} else if (rv == KERN_PROTECTION_FAILURE)
					rv = KERN_INVALID_ADDRESS;
			}
			if (rv == KERN_SUCCESS) {
				goto out;
			}

			if (!user) {
				/* Check for copyin/copyout fault */
				if (p != NULL &&
				    p->p_addr->u_pcb.pcb_onfault != 0) {
					framep->tf_regs[FRAME_PC] =
					    p->p_addr->u_pcb.pcb_onfault;
					p->p_addr->u_pcb.pcb_onfault = 0;
					goto out;
				}
				goto dopanic;
			}
			ucode = a0;
			if (rv == KERN_RESOURCE_SHORTAGE) {
				printf("UVM: pid %d (%s), uid %d killed: "
				       "out of swap\n", p->p_pid, p->p_comm,
				       p->p_cred && p->p_ucred ?
				       p->p_ucred->cr_uid : -1);
				i = SIGKILL;
			} else {
				i = SIGSEGV;
			}
			break;
		    }

		default:
			printf("trap: unknown MMCSR value 0x%lx\n", a1);
			goto dopanic;
		}
		break;

	default:
		goto dopanic;
	}

#ifdef DEBUG
	printtrap(a0, a1, a2, entry, framep, 1, user);
#endif
	trapsignal(p, i, ucode);
out:
	if (user)
		userret(p, framep->tf_regs[FRAME_PC], sticks);
	return;

dopanic:
	printtrap(a0, a1, a2, entry, framep, 1, user);

	/* XXX dump registers */

#if defined(DDB)
	if (call_debugger && alpha_debug(a0, a1, a2, entry, framep)) {
		/*
		 * The debugger has handled the trap; just return.
		 */
		goto out;
	}
#endif

	panic("trap");
}

/*
 * Process a system call.
 *
 * System calls are strange beasts.  They are passed the syscall number
 * in v0, and the arguments in the registers (as normal).  They return
 * an error flag in a3 (if a3 != 0 on return, the syscall had an error),
 * and the return value (if any) in v0.
 *
 * The assembly stub takes care of moving the call number into a register
 * we can get to, and moves all of the argument registers into their places
 * in the trap frame.  On return, it restores the callee-saved registers,
 * a3, and v0 from the frame before returning to the user process.
 */
void
syscall(code, framep)
	u_int64_t code;
	struct trapframe *framep;
{
	struct sysent *callp;
	struct proc *p;
	int error, numsys;
	u_int64_t opc;
	u_quad_t sticks;
	u_int64_t rval[2];
	u_int64_t args[10];					/* XXX */
	u_int hidden, nargs;
#ifdef COMPAT_OSF1
	extern struct emul emul_osf1;
#endif

#if notdef				/* can't happen, ever. */
	if ((framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) == 0)
		panic("syscall");
#endif
	uvmexp.syscalls++;
	p = curproc;
	p->p_md.md_tf = framep;
	opc = framep->tf_regs[FRAME_PC] - 4;
	sticks = p->p_sticks;

	callp = p->p_emul->e_sysent;
	numsys = p->p_emul->e_nsysent;

#ifdef COMPAT_OSF1
	if (p->p_emul == &emul_osf1) 
		switch (code) {
		case OSF1_SYS_syscall:
			/* OSF/1 syscall() */
			code = framep->tf_regs[FRAME_A0];
			hidden = 1;
			break;
		default:
			hidden = 0;
		}
	else
#endif
	switch(code) {
	case SYS_syscall:
	case SYS___syscall:
		/*
		 * syscall() and __syscall() are handled the same on
		 * the alpha, as everything is 64-bit aligned, anyway.
		 */
		code = framep->tf_regs[FRAME_A0];
		hidden = 1;
		break;
	default:
		hidden = 0;
	}

	error = 0;
	if (code < numsys)
		callp += code;
	else
		callp += p->p_emul->e_nosys;

	nargs = callp->sy_narg + hidden;
	switch (nargs) {
	default:
		if (nargs > 10)		/* XXX */
			panic("syscall: too many args (%d)", nargs);
		error = copyin((caddr_t)(alpha_pal_rdusp()), &args[6],
		    (nargs - 6) * sizeof(u_int64_t));
	case 6:	
		args[5] = framep->tf_regs[FRAME_A5];
	case 5:	
		args[4] = framep->tf_regs[FRAME_A4];
	case 4:	
		args[3] = framep->tf_regs[FRAME_A3];
	case 3:	
		args[2] = framep->tf_regs[FRAME_A2];
	case 2:	
		args[1] = framep->tf_regs[FRAME_A1];
	case 1:	
		args[0] = framep->tf_regs[FRAME_A0];
	case 0:
		break;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_argsize, args + hidden);
#endif
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args + hidden);
#endif
	if (error == 0) {
		rval[0] = 0;
		rval[1] = 0;
		error = (*callp->sy_call)(p, args + hidden, rval);
	}

	switch (error) {
	case 0:
		framep->tf_regs[FRAME_V0] = rval[0];
		framep->tf_regs[FRAME_A4] = rval[1];
		framep->tf_regs[FRAME_A3] = 0;
		break;
	case ERESTART:
		framep->tf_regs[FRAME_PC] = opc;
		break;
	case EJUSTRETURN:
		break;
	default:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		framep->tf_regs[FRAME_V0] = error;
		framep->tf_regs[FRAME_A3] = 1;
		break;
	}

        /*
         * Reinitialize proc pointer `p' as it may be different
         * if this is a child returning from fork syscall.
         */
	p = curproc;
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif

	userret(p, framep->tf_regs[FRAME_PC], sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;

	/*
	 * Return values in the frame set by cpu_fork().
	 */

	userret(p, p->p_md.md_tf->tf_regs[FRAME_PC], 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 */
void
ast(framep)
	struct trapframe *framep;
{
	register struct proc *p;
	u_quad_t sticks;

	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_tf = framep;

	if ((framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) == 0)
		panic("ast and not user");

	uvmexp.softs++;

	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	userret(p, framep->tf_regs[FRAME_PC], sticks);
}

/*
 * Unaligned access handler.  It's not clear that this can get much slower...
 *
 */
const static int reg_to_framereg[32] = {
	FRAME_V0,	FRAME_T0,	FRAME_T1,	FRAME_T2,
	FRAME_T3,	FRAME_T4,	FRAME_T5,	FRAME_T6,
	FRAME_T7,	FRAME_S0,	FRAME_S1,	FRAME_S2,
	FRAME_S3,	FRAME_S4,	FRAME_S5,	FRAME_S6,
	FRAME_A0,	FRAME_A1,	FRAME_A2,	FRAME_A3,
	FRAME_A4,	FRAME_A5,	FRAME_T8,	FRAME_T9,
	FRAME_T10,	FRAME_T11,	FRAME_RA,	FRAME_T12,
	FRAME_AT,	FRAME_GP,	FRAME_SP,	-1,
};

#define	irp(p, reg)							\
	((reg_to_framereg[(reg)] == -1) ? NULL :			\
	    &(p)->p_md.md_tf->tf_regs[reg_to_framereg[(reg)]])

#define	frp(p, reg)							\
	(&(p)->p_addr->u_pcb.pcb_fp.fpr_regs[(reg)])

#define	dump_fp_regs()							\
	if (p == fpcurproc) {						\
		alpha_pal_wrfen(1);					\
		savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);		\
		alpha_pal_wrfen(0);					\
		fpcurproc = NULL;					\
	}

#define	unaligned_load(storage, ptrf, mod)				\
	if (copyin((caddr_t)va, &(storage), sizeof (storage)) != 0)	\
		break;							\
	signal = 0;							\
	if ((regptr = ptrf(p, reg)) != NULL)				\
		*regptr = mod (storage);

#define	unaligned_store(storage, ptrf, mod)				\
	if ((regptr = ptrf(p, reg)) != NULL)				\
		(storage) = mod (*regptr);				\
	else								\
		(storage) = 0;						\
	if (copyout(&(storage), (caddr_t)va, sizeof (storage)) != 0)	\
		break;							\
	signal = 0;

#define	unaligned_load_integer(storage)					\
	unaligned_load(storage, irp, )

#define	unaligned_store_integer(storage)				\
	unaligned_store(storage, irp, )

#define	unaligned_load_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_load(storage, frp, mod)

#define	unaligned_store_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_store(storage, frp, mod)

unsigned long
Sfloat_to_reg(s)
	unsigned int s;
{
	unsigned long sign, expn, frac;
	unsigned long result;

	sign = (s & 0x80000000) >> 31;
	expn = (s & 0x7f800000) >> 23;
	frac = (s & 0x007fffff) >>  0;

	/* map exponent part, as appropriate. */
	if (expn == 0xff)
		expn = 0x7ff;
	else if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frac << 29);
	return (result);
}

unsigned int
reg_to_Sfloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frac;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frac = (r & 0x000fffffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 31) | (expn << 23) | (frac << 0);
	return (result);
}

/*
 * Conversion of T floating datums to and from register format
 * requires no bit reordering whatsoever.
 */
unsigned long
Tfloat_reg_cvt(input)
	unsigned long input;
{

	return (input);
}

#ifdef FIX_UNALIGNED_VAX_FP
unsigned long
Ffloat_to_reg(f)
	unsigned int f;
{
	unsigned long sign, expn, frlo, frhi;
	unsigned long result;

	sign = (f & 0x00008000) >> 15;
	expn = (f & 0x00007f80) >>  7;
	frhi = (f & 0x0000007f) >>  0;
	frlo = (f & 0xffff0000) >> 16;

	/* map exponent part, as appropriate. */
	if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frhi << 45) | (frlo << 29);
	return (result);
}

unsigned int
reg_to_Ffloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frhi, frlo;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frhi = (r & 0x000fe00000000000) >> 45;
	frlo = (r & 0x00001fffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 15) | (expn << 7) | (frhi << 0) | (frlo << 16);
	return (result);
}

/*
 * Conversion of G floating datums to and from register format is
 * symmetrical.  Just swap shorts in the quad...
 */
unsigned long
Gfloat_reg_cvt(input)
	unsigned long input;
{
	unsigned long a, b, c, d;
	unsigned long result;

	a = (input & 0x000000000000ffff) >> 0;
	b = (input & 0x00000000ffff0000) >> 16;
	c = (input & 0x0000ffff00000000) >> 32;
	d = (input & 0xffff000000000000) >> 48;

	result = (a << 48) | (b << 32) | (c << 16) | (d << 0);
	return (result);
}
#endif /* FIX_UNALIGNED_VAX_FP */

extern int	alpha_unaligned_print, alpha_unaligned_fix;
extern int	alpha_unaligned_sigbus;

struct unaligned_fixup_data {
	const char *type;	/* opcode name */
	int fixable;		/* fixable, 0 if fixup not supported */
	int size;		/* size, 0 if unknown */
	int acc;		/* useracc type; B_READ or B_WRITE */
};

#define	UNKNOWN()	{ "0x%lx", 0, 0, 0 }
#define	FIX_LD(n,s)	{ n, 1, s, B_READ }
#define	FIX_ST(n,s)	{ n, 1, s, B_WRITE }
#define	NOFIX_LD(n,s)	{ n, 0, s, B_READ }
#define	NOFIX_ST(n,s)	{ n, 0, s, B_WRITE }

int
unaligned_fixup(va, opcode, reg, p)
	unsigned long va, opcode, reg;
	struct proc *p;
{
	const struct unaligned_fixup_data tab_unknown[1] = {
		UNKNOWN(),
	};
	const struct unaligned_fixup_data tab_0c[0x02] = {
		FIX_LD("ldwu", 2),	FIX_ST("stw", 2),
	};
	const struct unaligned_fixup_data tab_20[0x10] = {
#ifdef FIX_UNALIGNED_VAX_FP
		FIX_LD("ldf", 4),	FIX_LD("ldg", 8),
#else
		NOFIX_LD("ldf", 4),	NOFIX_LD("ldg", 8),
#endif
		FIX_LD("lds", 4),	FIX_LD("ldt", 8),
#ifdef FIX_UNALIGNED_VAX_FP
		FIX_ST("stf", 4),	FIX_ST("stg", 8),
#else
		NOFIX_ST("stf", 4),	NOFIX_ST("stg", 8),
#endif
		FIX_ST("sts", 4),	FIX_ST("stt", 8),
		FIX_LD("ldl", 4),	FIX_LD("ldq", 8),
		NOFIX_LD("ldl_c", 4),	NOFIX_LD("ldq_c", 8),
		FIX_ST("stl", 4),	FIX_ST("stq", 8),
		NOFIX_ST("stl_c", 4),	NOFIX_ST("stq_c", 8),
	};
	const struct unaligned_fixup_data *selected_tab;
	int doprint, dofix, dosigbus, signal;
	unsigned long *regptr, longdata;
	int intdata;		/* signed to get extension when storing */
	u_int16_t worddata;	/* unsigned to _avoid_ extension */

	/*
	 * Read USP into frame in case it's the register to be modified.
	 * This keeps us from having to check for it in lots of places
	 * later.
	 */
	p->p_md.md_tf->tf_regs[FRAME_SP] = alpha_pal_rdusp();

	/*
	 * Figure out what actions to take.
	 *
	 * XXX In the future, this should have a per-process component
	 * as well.
	 */
	doprint = alpha_unaligned_print;
	dofix = alpha_unaligned_fix;
	dosigbus = alpha_unaligned_sigbus;

	/*
	 * Find out which opcode it is.  Arrange to have the opcode
	 * printed if it's an unknown opcode.
	 */
	if (opcode >= 0x0c && opcode <= 0x0d)
		selected_tab = &tab_0c[opcode - 0x0c];
	else if (opcode >= 0x20 && opcode <= 0x2f)
		selected_tab = &tab_20[opcode - 0x20];
	else
		selected_tab = tab_unknown;

	/*
	 * See if the user can access the memory in question.
	 * If it's an unknown opcode, we don't know whether to
	 * read or write, so we don't check.
	 *
	 * We adjust the PC backwards so that the instruction will
	 * be re-run.
	 */
	if (selected_tab->size != 0 &&
	   !uvm_useracc((caddr_t)va, selected_tab->size, selected_tab->acc)) {
		p->p_md.md_tf->tf_regs[FRAME_PC] -= 4;
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf(
		"pid %d (%s): unaligned access: va=0x%lx pc=0x%lx ra=0x%lx op=",
		    p->p_pid, p->p_comm, va,
		    p->p_md.md_tf->tf_regs[FRAME_PC] - 4,
		    p->p_md.md_tf->tf_regs[FRAME_RA]);
		uprintf(selected_tab->type,opcode);
		uprintf("\n");
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = SIGBUS;
	if (dofix && selected_tab->fixable) {
		switch (opcode) {
		case 0x0c:			/* ldwu */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_load_integer(worddata);
			break;

		case 0x0d:			/* stw */
			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			unaligned_store_integer(worddata);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x20:			/* ldf */
			unaligned_load_floating(intdata, Ffloat_to_reg);
			break;

		case 0x21:			/* ldg */
			unaligned_load_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x22:			/* lds */
			unaligned_load_floating(intdata, Sfloat_to_reg);
			break;

		case 0x23:			/* ldt */
			unaligned_load_floating(longdata, Tfloat_reg_cvt);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x24:			/* stf */
			unaligned_store_floating(intdata, reg_to_Ffloat);
			break;

		case 0x25:			/* stg */
			unaligned_store_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x26:			/* sts */
			unaligned_store_floating(intdata, reg_to_Sfloat);
			break;

		case 0x27:			/* stt */
			unaligned_store_floating(longdata, Tfloat_reg_cvt);
			break;

		case 0x28:			/* ldl */
			unaligned_load_integer(intdata);
			break;

		case 0x29:			/* ldq */
			unaligned_load_integer(longdata);
			break;

		case 0x2c:			/* stl */
			unaligned_store_integer(intdata);
			break;

		case 0x2d:			/* stq */
			unaligned_store_integer(longdata);
			break;

#ifdef DIAGNOSTIC
		default:
			panic("unaligned_fixup: can't get here");
#endif
		}
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	/*
	 * Write back USP.
	 */
	alpha_pal_wrusp(p->p_md.md_tf->tf_regs[FRAME_SP]);

	return (signal);
}

/*
 * Reserved/unimplemented instruction (opDec fault) handler
 *
 * Argument is the process that caused it.  No useful information
 * is passed to the trap handler other than the fault type.  The
 * address of the instruction that caused the fault is 4 less than
 * the PC stored in the trap frame.
 *
 * If the instruction is emulated successfully, this function returns 0.
 * Otherwise, this function returns the signal to deliver to the process,
 * and fills in *ucodep with the code to be delivered.
 */
int
handle_opdec(p, ucodep)
	struct proc *p;
	u_int64_t *ucodep;
{
	alpha_instruction inst;
	register_t *regptr, memaddr;
	u_int64_t inst_pc;
	int sig;

	/*
	 * Read USP into frame in case it's going to be used or modified.
	 * This keeps us from having to check for it in lots of places
	 * later.
	 */
	p->p_md.md_tf->tf_regs[FRAME_SP] = alpha_pal_rdusp();

	inst_pc = memaddr = p->p_md.md_tf->tf_regs[FRAME_PC] - 4;
	if (copyin((caddr_t)inst_pc, &inst, sizeof (inst)) != 0) {
		/*
		 * really, this should never happen, but in case it
		 * does we handle it.
		 */
		printf("WARNING: handle_opdec() couldn't fetch instruction\n");
		goto sigsegv;
	}

	switch (inst.generic_format.opcode) {
	case op_ldbu:
	case op_ldwu:
	case op_stw:
	case op_stb:
		regptr = irp(p, inst.mem_format.rb);
		if (regptr != NULL)
			memaddr = *regptr;
		else
			memaddr = 0;
		memaddr += inst.mem_format.displacement;

		regptr = irp(p, inst.mem_format.ra);

		if (inst.mem_format.opcode == op_ldwu ||
		    inst.mem_format.opcode == op_stw) {
			if (memaddr & 0x01) {
				sig = unaligned_fixup(memaddr,
				    inst.mem_format.opcode,
				    inst.mem_format.ra, p);
				if (sig)
					goto unaligned_fixup_sig;
				break;
			}
		}

		if (inst.mem_format.opcode == op_ldbu) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &b, sizeof (b)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = b;
		} else if (inst.mem_format.opcode == op_ldwu) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			if (copyin((caddr_t)memaddr, &w, sizeof (w)) != 0)
				goto sigsegv;
			if (regptr != NULL)
				*regptr = w;
		} else if (inst.mem_format.opcode == op_stw) {
			u_int16_t w;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			w = (regptr != NULL) ? *regptr : 0;
			if (copyout(&w, (caddr_t)memaddr, sizeof (w)) != 0)
				goto sigsegv;
		} else if (inst.mem_format.opcode == op_stb) {
			u_int8_t b;

			/* XXX ONLY WORKS ON LITTLE-ENDIAN ALPHA */
			b = (regptr != NULL) ? *regptr : 0;
			if (copyout(&b, (caddr_t)memaddr, sizeof (b)) != 0)
				goto sigsegv;
		}
		break;

	case op_intmisc:
		if (inst.operate_generic_format.function == op_sextb &&
		    inst.operate_generic_format.ra == 31) {
			int8_t b;

			if (inst.operate_generic_format.is_lit) {
				b = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(p, inst.operate_reg_format.rb);
				b = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(p, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = b;
			break;
		}
		if (inst.operate_generic_format.function == op_sextw &&
		    inst.operate_generic_format.ra == 31) {
			int16_t w;

			if (inst.operate_generic_format.is_lit) {
				w = inst.operate_lit_format.literal;
			} else {
				if (inst.operate_reg_format.sbz != 0)
					goto sigill;
				regptr = irp(p, inst.operate_reg_format.rb);
				w = (regptr != NULL) ? *regptr : 0;
			}

			regptr = irp(p, inst.operate_generic_format.rc);
			if (regptr != NULL)
				*regptr = w;
			break;
		}
		goto sigill;

	default:
		goto sigill;
	}

	/*
	 * Write back USP.  Note that in the error cases below,
	 * nothing will have been successfully modified so we don't
	 * have to write it out.
	 */
	alpha_pal_wrusp(p->p_md.md_tf->tf_regs[FRAME_SP]);

	return (0);

sigill:
	*ucodep = ALPHA_IF_CODE_OPDEC;			/* trap type */
	return (SIGILL);

sigsegv:
	sig = SIGSEGV;
	p->p_md.md_tf->tf_regs[FRAME_PC] = inst_pc;	/* re-run instr. */
unaligned_fixup_sig:
	*ucodep = memaddr;				/* faulting address */
	return (sig);
}
