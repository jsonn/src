/*	$NetBSD: trap.c,v 1.22.2.3 2000/12/13 15:49:36 bouyer Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_altivec.h"
#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/ktrace.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>

/* These definitions should probably be somewhere else			XXX */
#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8)) /* more args go here */

volatile int astpending;
volatile int want_resched;

void *syscall = NULL;	/* XXX dummy symbol for emul_netbsd */

static int fix_unaligned __P((struct proc *p, struct trapframe *frame));

void
trap(frame)
	struct trapframe *frame;
{
	struct proc *p = curproc;
	int type = frame->exc;
	u_quad_t sticks;
	int ftype, rv;

	if (frame->srr1 & PSL_PR) {
		type |= EXC_USER;
		sticks = p->p_sticks;
	}

	switch (type) {
	case EXC_TRC|EXC_USER:
		KERNEL_PROC_LOCK(p);
		frame->srr1 &= ~PSL_SE;
		trapsignal(p, SIGTRAP, EXC_TRC);
		KERNEL_PROC_UNLOCK(p);
		break;
	case EXC_DSI:
		{
			vm_map_t map;
			vaddr_t va;
			faultbuf *fb;

			KERNEL_LOCK(LK_CANRECURSE|LK_EXCLUSIVE);
			map = kernel_map;
			va = frame->dar;
			if ((va >> ADDR_SR_SHFT) == USER_SR) {
				sr_t user_sr;

				asm ("mfsr %0, %1"
				     : "=r"(user_sr) : "K"(USER_SR));
				va &= ADDR_PIDX | ADDR_POFF;
				va |= user_sr << ADDR_SR_SHFT;
				map = &p->p_vmspace->vm_map;
			}
			if (frame->dsisr & DSISR_STORE)
				ftype = VM_PROT_READ | VM_PROT_WRITE;
			else
				ftype = VM_PROT_READ;
			rv = uvm_fault(map, trunc_page(va), 0, ftype);
			KERNEL_UNLOCK();
			if (rv == KERN_SUCCESS)
				return;
			if (fb = p->p_addr->u_pcb.pcb_onfault) {
				frame->srr0 = (*fb)[0];
				frame->fixreg[1] = (*fb)[1];
				frame->fixreg[2] = (*fb)[2];
				frame->cr = (*fb)[3];
				bcopy(&(*fb)[4], &frame->fixreg[13],
				      19 * sizeof(register_t));
				return;
			}
			map = kernel_map;
		}
		goto brain_damage;
	case EXC_DSI|EXC_USER:
		KERNEL_PROC_LOCK(p);
		if (frame->dsisr & DSISR_STORE)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		if ((rv = uvm_fault(&p->p_vmspace->vm_map,
				    trunc_page(frame->dar), 0, ftype))
		    == KERN_SUCCESS) {
			KERNEL_PROC_UNLOCK(p);
			break;
		}
		if (rv == KERN_RESOURCE_SHORTAGE) {
			printf("UVM: pid %d (%s), uid %d killed: "
			       "out of swap\n",
			       p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, EXC_DSI);
		} else {
			trapsignal(p, SIGSEGV, EXC_DSI);
		}
		KERNEL_PROC_UNLOCK(p);
		break;
	case EXC_ISI|EXC_USER:
		KERNEL_PROC_LOCK(p);
		ftype = VM_PROT_READ | VM_PROT_EXECUTE;
		if (uvm_fault(&p->p_vmspace->vm_map,
			     trunc_page(frame->srr0), 0, ftype)
		    == KERN_SUCCESS) {
			KERNEL_PROC_UNLOCK(p);
			break;
		}
		trapsignal(p, SIGSEGV, EXC_ISI);
		KERNEL_PROC_UNLOCK(p);
		break;
	case EXC_SC|EXC_USER:
		{
			const struct sysent *callp;
			size_t argsize;
			register_t code, error;
			register_t *params, rval[2];
			int nsys, n;
			register_t args[10];

			KERNEL_PROC_LOCK(p);

			uvmexp.syscalls++;

			nsys = p->p_emul->e_nsysent;
			callp = p->p_emul->e_sysent;

			code = frame->fixreg[0];
			params = frame->fixreg + FIRSTARG;

			switch (code) {
			case SYS_syscall:
				/*
				 * code is first argument,
				 * followed by actual args.
				 */
				code = *params++;
				break;
			case SYS___syscall:
				/*
				 * Like syscall, but code is a quad,
				 * so as to maintain quad alignment
				 * for the rest of the args.
				 */
				if (callp != sysent)
					break;
				params++;
				code = *params++;
				break;
			default:
				break;
			}
			if (code < 0 || code >= nsys)
				callp += p->p_emul->e_nosys;
			else
				callp += code;
			argsize = callp->sy_argsize;
			n = NARGREG - (params - (frame->fixreg + FIRSTARG));
			if (argsize > n * sizeof(register_t)) {
				bcopy(params, args, n * sizeof(register_t));
				if (error = copyin(MOREARGS(frame->fixreg[1]),
						   args + n,
						   argsize - n * sizeof(register_t))) {
#ifdef	KTRACE
					/* Can't get all the arguments! */
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p, code, argsize,
						    args);
#endif
					goto syscall_bad;
				}
				params = args;
			}
#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSCALL))
				ktrsyscall(p, code, argsize, params);
#endif
			rval[0] = 0;
			rval[1] = frame->fixreg[FIRSTARG + 1];

			switch (error = (*callp->sy_call)(p, params, rval)) {
			case 0:
				frame->fixreg[FIRSTARG] = rval[0];
				frame->fixreg[FIRSTARG + 1] = rval[1];
				frame->cr &= ~0x10000000;
				break;
			case ERESTART:
				/*
				 * Set user's pc back to redo the system call.
				 */
				frame->srr0 -= 4;
				break;
			case EJUSTRETURN:
				/* nothing to do */
				break;
			default:
syscall_bad:
				if (p->p_emul->e_errno)
					error = p->p_emul->e_errno[error];
				frame->fixreg[FIRSTARG] = error;
				frame->cr |= 0x10000000;
				break;
			}
#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSRET))
				ktrsysret(p, code, error, rval[0]);
#endif
		}
		KERNEL_PROC_UNLOCK(p);
		break;

	case EXC_FPU|EXC_USER:
		if (fpuproc)
			save_fpu(fpuproc);
		fpuproc = p;
		enable_fpu(p);
		break;

#ifdef ALTIVEC
	case EXC_VEC|EXC_USER:
		if (vecproc)
			save_vec(vecproc);
		vecproc = p;
		enable_vec(p);
		break;
#endif

	case EXC_AST|EXC_USER:
		/* This is just here that we trap */
		break;

	case EXC_ALI|EXC_USER:
		KERNEL_PROC_LOCK(p);
		if (fix_unaligned(p, frame) != 0)
			trapsignal(p, SIGBUS, EXC_ALI);
		else
			frame->srr0 += 4;
		KERNEL_PROC_UNLOCK(p);
		break;

	case EXC_PGM|EXC_USER:
/* XXX temporarily */
		KERNEL_PROC_LOCK(p);
		if (frame->srr1 & 0x0002000)
			trapsignal(p, SIGTRAP, EXC_PGM);
		else
			trapsignal(p, SIGILL, EXC_PGM);
		KERNEL_PROC_UNLOCK(p);
		break;

	case EXC_MCHK:
		{
			faultbuf *fb;

			if (fb = p->p_addr->u_pcb.pcb_onfault) {
				frame->srr0 = (*fb)[0];
				frame->fixreg[1] = (*fb)[1];
				frame->fixreg[2] = (*fb)[2];
				frame->cr = (*fb)[3];
				bcopy(&(*fb)[4], &frame->fixreg[13],
				      19 * sizeof(register_t));
				return;
			}
		}
		goto brain_damage;

	default:
brain_damage:
		printf("trap type %x at %x\n", type, frame->srr0);
#ifdef DDB
		if (kdb_trap(type, frame))
			return;
#endif
#ifdef TRAP_PANICWAIT
		printf("Press a key to panic.\n");
		cngetc();
#endif
		panic("trap");
	}

	astpending = 0;		/* we are about to do it */

	uvmexp.softs++;

	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	/* take pending signals */
	{
		int sig;

		while (sig = CURSIG(p))
			postsig(sig);
	}

	p->p_priority = p->p_usrpri;
	if (want_resched) {
		int sig;
		/*
		 * We are being preempted.
		 */
		preempt(NULL);
		while (sig = CURSIG(p))
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, frame->srr0,
			    (int)(p->p_sticks - sticks) * psratio);
	}
	/*
	 * If someone stole the fp or vector unit while we were away,
	 * disable it
	 */
	if (p != fpuproc)
		frame->srr1 &= ~PSL_FP;
#ifdef ALTIVEC
	if (p != vecproc)
		frame->srr1 &= ~PSL_VEC;
#endif
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	struct trapframe *tf = trapframe(p);

	KERNEL_PROC_UNLOCK(p);

	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
	tf->srr1 &= ~(PSL_FP|PSL_VEC);	/* Disable FP & AltiVec, as we can't be them */
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		KERNEL_PROC_LOCK(p);
		ktrsysret(p, SYS_fork, 0, 0);
		KERNEL_PROC_UNLOCK(p);
	}
#endif
	/* Profiling?							XXX */
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

static inline void
setusr(content)
	int content;
{
	asm volatile ("isync; mtsr %0,%1; isync"
		      :: "n"(USER_SR), "r"(content));
}

int
copyin(udaddr, kaddr, len)
	const void *udaddr;
	void *kaddr;
	size_t len;
{
	const char *up = udaddr;
	char *kp = kaddr;
	char *p;
	size_t l;
	faultbuf env;

	if (setfault(env)) {
		curpcb->pcb_onfault = 0;
		return EFAULT;
	}
	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);
		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		setusr(curpcb->pcb_pm->pm_sr[(u_int)up >> ADDR_SR_SHFT]);
		bcopy(p, kp, l);
		up += l;
		kp += l;
		len -= l;
	}
	curpcb->pcb_onfault = 0;
	return 0;
}

int
copyout(kaddr, udaddr, len)
	const void *kaddr;
	void *udaddr;
	size_t len;
{
	const char *kp = kaddr;
	char *up = udaddr;
	char *p;
	size_t l;
	faultbuf env;

	if (setfault(env)) {
		curpcb->pcb_onfault = 0;
		return EFAULT;
	}
	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);
		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		setusr(curpcb->pcb_pm->pm_sr[(u_int)up >> ADDR_SR_SHFT]);
		bcopy(kp, p, l);
		up += l;
		kp += l;
		len -= l;
	}
	curpcb->pcb_onfault = 0;
	return 0;
}

/*
 * kcopy(const void *src, void *dst, size_t len);
 *
 * Copy len bytes from src to dst, aborting if we encounter a fatal
 * page fault.
 *
 * kcopy() _must_ save and restore the old fault handler since it is
 * called by uiomove(), which may be in the path of servicing a non-fatal
 * page fault.
 */
int
kcopy(src, dst, len)
	const void *src;
	void *dst;
	size_t len;
{
	faultbuf env, *oldfault;

	oldfault = curpcb->pcb_onfault;
	if (setfault(env)) {
		curpcb->pcb_onfault = oldfault;
		return EFAULT;
	}

	bcopy(src, dst, len);

	curpcb->pcb_onfault = oldfault;
	return 0;
}

int
badaddr(addr, size)
	void *addr;
	size_t size;
{
	return badaddr_read(addr, size, NULL);
}

int
badaddr_read(addr, size, rptr)
	void *addr;
	size_t size;
	int *rptr;
{
	faultbuf env;
	int x;

	/* Get rid of any stale machine checks that have been waiting.  */
	__asm __volatile ("sync; isync");

	if (setfault(env)) {
		curpcb->pcb_onfault = 0;
		__asm __volatile ("sync");
		return 1;
	}

	__asm __volatile ("sync");

	switch (size) {
	case 1:
		x = *(volatile int8_t *)addr;
		break;
	case 2:
		x = *(volatile int16_t *)addr;
		break;
	case 4:
		x = *(volatile int32_t *)addr;
		break;
	default:
		panic("badaddr: invalid size (%d)", size);
	}

	/* Make sure we took the machine check, if we caused one. */
	__asm __volatile ("sync; isync");

	curpcb->pcb_onfault = 0;
	__asm __volatile ("sync");	/* To be sure. */

	/* Use the value to avoid reorder. */
	if (rptr)
		*rptr = x;

	return 0;
}

/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

static int
fix_unaligned(p, frame)
	struct proc *p;
	struct trapframe *frame;
{
	int indicator = EXC_ALI_OPCODE_INDICATOR(frame->dsisr);

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
		{
			int reg = EXC_ALI_RST(frame->dsisr);
			double *fpr = &p->p_addr->u_pcb.pcb_fpu.fpr[reg];

			/* Juggle the FPU to ensure that we've initialized
			 * the FPRs, and that their current state is in
			 * the PCB.
			 */
			if (fpuproc != p) {
				if (fpuproc)
					save_fpu(fpuproc);
				enable_fpu(p);
			}
			save_fpu(p);

			if (indicator == EXC_ALI_LFD) {
				if (copyin((void *)frame->dar, fpr,
				    sizeof(double)) != 0)
					return -1;
				enable_fpu(p);
			} else {
				if (copyout(fpr, (void *)frame->dar,
				    sizeof(double)) != 0)
					return -1;
			}
			return 0;
		}
		break;
	}

	return -1;
}
