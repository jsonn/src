/*	$NetBSD: trap.c,v 1.46.2.1 2002/03/16 15:59:08 jdolecek Exp $	*/

/*-
 * Copyright (c) 1996 Matthias Pfaller. All rights reserved.
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
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
 *	@(#)trap.c	7.4 (Berkeley) 5/13/91
 */

/*
 * 532 Trap and System call handling
 */

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_syscall_debug.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/syscall.h>
#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/fpu.h>
#include <machine/reg.h>
#include <machine/trap.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif

struct proc *fpu_proc;		/* Process owning the FPU. */

/* Allow turning off if ieee_handler for debugging */
int ieee_handler_disable = 0;

#if defined(MRTD)
#define __CDECL__ __attribute__ ((cdecl))
#else
#define __CDECL__
#endif

void syscall __P((struct syscframe)) __CDECL__;
void trap __P((struct trapframe)) __CDECL__;
static __inline void userret __P((struct proc *, int, u_quad_t));

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
static __inline void
userret(p, pc, oticks)
	struct proc *p;
	int pc;
	u_quad_t oticks;
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * We are being preempted.
		 */
		preempt(NULL);
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

	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

char	*trap_type[] = {
	"non-vectored interrupt",		/*  0 T_NVI */
	"non-maskable interrupt",		/*  1 T_NMI */
	"abort trap",				/*  2 T_ABT */
	"coprocessor trap",			/*  3 T_SLAVE */
	"illegal operation in user mode",	/*  4 T_ILL */
	"supervisor call",			/*  5 T_SVC */
	"divide by zero",			/*  6 T_DVZ */
	"flag instruction",			/*  7 T_FLG */
	"breakpoint instruction",		/*  8 T_BPT */
	"trace trap",				/*  9 T_TRC */
	"undefined instruction",		/* 10 T_UND */
	"restartable bus error",		/* 11 T_RBE */
	"non-restartable bus error",		/* 12 T_NBE */
	"integer overflow trap",		/* 13 T_OVF */
	"debug trap",				/* 14 T_DBG */
	"reserved trap",			/* 15 T_RESERVED */
	"unused",				/* 16 unused */
	"watchpoint",				/* 17 T_WATCHPOINT */
	"asynchronous system trap"		/* 18 T_AST */
};
int	trap_types = sizeof trap_type / sizeof trap_type[0];

#ifdef DEBUG
int	trapdebug = 0;
#endif

/*
 * trap(frame):
 *	Exception, fault, and trap interface to BSD kernel. This
 * common code is called from assembly language trap vector
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed. Note that the
 * effect is as if the arguments were passed call by reference.
 */
/*ARGSUSED*/
void
trap(frame)
	struct trapframe frame;
{
	struct proc *p = curproc;
	int type = frame.tf_trapno;
	u_quad_t sticks;
	struct pcb *pcb = NULL;
	extern char fubail[], subail[];
#ifdef CINVSMALL
	extern char cinvstart[], cinvend[];
#endif

	uvmexp.traps++;

#ifdef DEBUG
	if (trapdebug) {
		printf("trap type=%d, pc=0x%x, tear=0x%x, msr=0x%x\n",
			type, frame.tf_regs.r_pc,
			frame.tf_tear, frame.tf_msr);
		printf("curproc %p\n", curproc);
	}
#endif

	if (USERMODE(frame.tf_regs.r_psr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = &frame.tf_regs;
	} else
		sticks = 0;

	switch (type) {

	default:
	we_re_toast:
#if defined(DDB) || defined(KGDB)
	{
		static struct trapframe *db_frame = (struct trapframe *)
			(VM_MIN_KERNEL_ADDRESS + 0x1000 - sizeof(frame));
		static int usp, r;
		register int ret __asm("r3");
		register int s __asm("r4") = splhigh();
		register int sp __asm("r5");

		r = 0;
		ret = ((int *)&frame)[-1];
		*++db_frame = frame;
		usp = db_frame->tf_regs.r_sp;
		db_frame->tf_regs.r_sp = ((int)&frame) + sizeof(frame);
		sprd(sp, sp);
		if (sp >= (VM_MIN_KERNEL_ADDRESS + 0x1000))
			lprd(sp, VM_MIN_KERNEL_ADDRESS + 0x1000 - 4);
		else
			lprd(sp, sp - 64);
# ifdef KGDB
		r = kgdb_trap(type, db_frame);
# endif
# ifdef DDB
		if (r == 0) {
			extern int db_active_ipl;
			db_active_ipl = s;
			r = kdb_trap(type, 0, db_frame);
		}
# endif
		splx(s);
		if (r) {
			sp = db_frame->tf_regs.r_sp - sizeof(frame);
			db_frame->tf_regs.r_sp = usp;
			*(struct trapframe *)sp = *db_frame--;
			lprd(fp, &((struct trapframe *)sp)->tf_regs.r_fp);
			lprd(sp, sp);
			__asm __volatile("jump 0(%0)" : : "g" (ret));
		}
		db_frame--;
		lprd(sp, sp); /* Frame should be intact */
	}
#endif
		if (frame.tf_trapno < trap_types)
			printf("fatal %s", trap_type[frame.tf_trapno]);
		else
			printf("unknown trap %ld", frame.tf_trapno);
		printf(" in %s mode\n", (type & T_USER) ? "user" : "supervisor");
		printf("trap type=%d, pc=0x%x, tear=0x%lx, msr=0x%lx\n",
			type, frame.tf_regs.r_pc, frame.tf_tear, frame.tf_msr);

		panic("trap");
		/*NOTREACHED*/

	case T_UND | T_USER: {		/* undefined instruction fault */
		int cfg;
#ifndef NS381
		extern int _have_fpu;
		if (!_have_fpu) {
# ifdef MATH_EMULATE
			int rv;
			if ((rv = math_emulate(&frame)) == 0) {
				if (frame.tf_psr & PSL_T) {
					type = T_TRC | T_USER;
					goto trace;
				}
				return;
			}
# endif
		} else
#endif
		{
			sprd(cfg, cfg);
			if ((cfg & CFG_F) == 0) {
				lprd(cfg, cfg | CFG_F);
				if (fpu_proc == p)
					return;
				pcb = &p->p_addr->u_pcb;
				if (fpu_proc != 0)
					save_fpu_context(&fpu_proc->p_addr->u_pcb);
				restore_fpu_context(pcb);
				fpu_proc = p;
				return;
			}
		}
	}

	case T_ILL | T_USER:		/* privileged instruction fault */
		trapsignal(p, SIGILL, type &~ T_USER);
		goto out;

	case T_AST | T_USER:		/* Allow process switch */
		uvmexp.softs++;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto out;

	case T_OVF | T_USER:
	case T_DVZ | T_USER:
		trapsignal(p, SIGFPE, type &~ T_USER);
		goto out;

	case T_SLAVE | T_USER: {
		int fsr, sig = SIGFPE;
		pcb = &p->p_addr->u_pcb;
		if (ieee_handler_disable == 0) {
			save_fpu_context(pcb);
			switch(ieee_handle_exception(p)) {
			case FPC_TT_NONE:
				restore_fpu_context(pcb);
				if (frame.tf_regs.r_psr &  PSL_T) {
					type = T_TRC | T_USER;
					goto trace;
				}
				return;
			case FPC_TT_ILL:
				sig = SIGILL;
				break;
			default:
				break;
			}
			restore_fpu_context(pcb);
		}
		sfsr(fsr);
		trapsignal(p, sig, 0x80000000 | fsr);
		goto out;
	}

	case T_ABT:			/* allow page faults in kernel mode */
		if ((frame.tf_msr & MSR_STT) == STT_SEQ_INS ||
		    (frame.tf_msr & MSR_STT) == STT_NSQ_INS ||
		    (p == 0))
			goto we_re_toast;
		pcb = &p->p_addr->u_pcb;
		/*
		 * {fu,su}bail is used by [fs]uswintr() to prevent page
		 * faulting from inside the profiling interrupt.
		 */
		if (pcb->pcb_onfault == fubail || pcb->pcb_onfault == subail)
			goto copyfault;
#ifdef CINVSMALL
		/*
		 * If a address translation for a cache invalidate
		 * request fails, reset the pc and return.
		 */
		if ((unsigned int)frame.tf_pc >= (unsigned int)cinvstart &&
		    (unsigned int)frame.tf_pc <  (unsigned int)cinvend) {
			frame.tf_pc = (int)cinvend;
			return;
		}
#endif
		/* FALLTHROUGH */

	case T_ABT | T_USER: {		/* page fault */
		vaddr_t va;
		struct vmspace *vm = p->p_vmspace;
		struct vm_map *map;
		int rv;
		vm_prot_t ftype;
		extern struct vm_map *kernel_map;
		unsigned nss;

		va = trunc_page((vaddr_t)frame.tf_tear);
		/*
		 * It is only a kernel address space fault iff:
		 *	1. (type & T_USER) == 0  and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_ABT && va >= KERNBASE)
			map = kernel_map;
		else
			map = &vm->vm_map;
		if ((frame.tf_msr & MSR_DDT) == DDT_WRITE ||
		    (frame.tf_msr & MSR_STT) == STT_RMW)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

#ifdef DIAGNOSTIC
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %lx\n", va);
			goto we_re_toast;
		}
#endif

		nss = 0;
		if ((caddr_t)va >= vm->vm_maxsaddr
		    && (caddr_t)va < (caddr_t)VM_MAXUSER_ADDRESS
		    && map != kernel_map) {
			nss = btoc(USRSTACK-(unsigned)va);
			if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
				nss = 0;
			}
		}

		/* Fault the original page in. */
		rv = uvm_fault(map, va, 0, ftype);
		if (rv == 0) {
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;

			if (type == T_ABT)
				return;
			goto out;
		}

		if (type == T_ABT) {
			if (pcb->pcb_onfault != 0) {
			copyfault:
				frame.tf_regs.r_pc = (int)curpcb->pcb_onfault;
				return;
			}
			printf("uvm_fault(%p, 0x%lx, 0, %d) -> %x\n",
			    map, va, ftype, rv);
			goto we_re_toast;
		}
		if (rv == ENOMEM) {
			printf("UVM: pid %d (%s), uid %d killed: out of swap\n",
			       p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, T_ABT);
		} else {
			trapsignal(p, SIGSEGV, T_ABT);
		}
		break;
	}

	case T_TRC | T_USER: 	/* trace trap */
	case T_BPT | T_USER: 	/* breakpoint instruction */
	case T_DBG | T_USER: 	/* debug trap */
	trace:
		trapsignal(p, SIGTRAP, type &~ T_USER);
		break;

	case T_NMI:		/* non-maskable interrupt */
	case T_NMI | T_USER: 
#if defined(KGDB) || defined(DDB)
		/* NMI can be hooked up to a pushbutton for debugging */
		printf ("NMI ... going to debugger\n");
# ifdef KGDB
		if (kgdb_trap(type, &frame))
			return;
# endif
# ifdef DDB
		if (kdb_trap (type, 0, &frame))
			return;
# endif
#endif
		goto we_re_toast;
	}

	if ((type & T_USER) == 0)
		return;
out:
	userret(p, frame.tf_regs.r_pc, sticks);
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
/*ARGSUSED*/
void
syscall(frame)
	struct syscframe frame;
{
	caddr_t params;
	const struct sysent *callp;
	struct proc *p;
	int error, opc, nsys;
	size_t argsize;
	register_t code, args[8], rval[2];
	u_quad_t sticks;

	uvmexp.syscalls++;
	if (!USERMODE(frame.sf_regs.r_psr))
		panic("syscall");
	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_regs = &frame.sf_regs;
	opc = frame.sf_regs.r_pc++;
	code = frame.sf_regs.r_r0;

	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;

	params = (caddr_t)frame.sf_regs.r_sp + sizeof(int);

	switch (code) {
	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		break;
	case SYS___syscall:
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		if (callp != sysent)
			break;
		code = fuword(params + _QUAD_LOWWORD * sizeof(int));
		params += sizeof(quad_t);
		break;
	default:
		break;
	}
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;		/* illegal */
	else
		callp += code;
	argsize = callp->sy_argsize;
	if (argsize)
		error = copyin(params, (caddr_t)args, argsize);
	else
		error = 0;
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, argsize, args);
#endif
	if (error)
		goto bad;
	rval[0] = 0;
	rval[1] = frame.sf_regs.r_r1;
	error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.sf_regs.r_r0 = rval[0];
		frame.sf_regs.r_r1 = rval[1];
		frame.sf_regs.r_psr &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/*
		 * Just reset the pc to the SVC instruction.
		 */
		frame.sf_regs.r_pc = opc;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame.sf_regs.r_r0 = error;
		frame.sf_regs.r_psr |= PSL_C;	/* carry bit */
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, frame.sf_regs.r_pc, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, error, rval[0]);
#endif
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;

	p->p_md.md_regs->r_r0 = 0;
	p->p_md.md_regs->r_psr &= ~PSL_C;

	userret(p, p->p_md.md_regs->r_pc, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
}
