/*	$NetBSD: trap.c,v 1.44.4.1 1999/06/21 01:03:46 thorpej Exp $     */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		
#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/exec.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/mtpr.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/cpu.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <kern/syscalls.c>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#ifdef TRAPDEBUG
volatile int startsysc = 0, faultdebug = 0;
#endif

void	arithflt __P((struct trapframe *));
void	syscall __P((struct trapframe *));

char *traptypes[]={
	"reserved addressing",
	"privileged instruction",
	"reserved operand",
	"breakpoint instruction",
	"XFC instruction",
	"system call ",
	"arithmetic trap",
	"asynchronous system trap",
	"page table length fault",
	"translation violation fault",
	"trace trap",
	"compatibility mode fault",
	"access violation fault",
	"",
	"",
	"KSP invalid",
	"",
	"kernel debugger trap"
};
int no_traps = 18;

#define USERMODE(framep)   ((((framep)->psl) & (PSL_U)) == PSL_U)
#define FAULTCHK						\
	if (p->p_addr->u_pcb.iftrap) {				\
		frame->pc = (unsigned)p->p_addr->u_pcb.iftrap;	\
		frame->psl &= ~PSL_FPD;				\
		frame->r0 = EFAULT;/* for copyin/out */		\
		frame->r1 = -1; /* for fetch/store */		\
		return;						\
	}

void
arithflt(frame)
	struct trapframe *frame;
{
	u_int	sig = 0, type = frame->trap, trapsig = 1;
	u_int	rv, addr, umode;
	struct	proc *p = curproc;
	struct	pmap *pm;
	u_quad_t oticks = 0;
	vm_map_t map;
	vm_prot_t ftype;
	extern vm_map_t pte_map;
	
	uvmexp.traps++;
	if ((umode = USERMODE(frame))) {
		type |= T_USER;
		oticks = p->p_sticks;
		p->p_addr->u_pcb.framep = frame; 
	}

	type&=~(T_WRITE|T_PTEFETCH);


#ifdef TRAPDEBUG
if(frame->trap==7) goto fram;
if(faultdebug)printf("Trap: type %lx, code %lx, pc %lx, psl %lx\n",
		frame->trap, frame->code, frame->pc, frame->psl);
fram:
#endif
	switch(type){

	default:
#ifdef DDB
		kdb_trap(frame);
#endif
		printf("Trap: type %x, code %x, pc %x, psl %x\n",
		    (u_int)frame->trap, (u_int)frame->code,
		    (u_int)frame->pc, (u_int)frame->psl);
		panic("trap");

	case T_KSPNOTVAL:
		panic("kernel stack invalid");

	case T_TRANSFLT|T_USER:
	case T_TRANSFLT:
		/*
		 * BUG! BUG! BUG! BUG! BUG!
		 * Due to a hardware bug (at in least KA65x CPUs) a double
		 * page table fetch trap will cause a translation fault
		 * even if access in the SPT PTE entry specifies 'no access'.
		 * In for example section 6.4.2 in VAX Architecture 
		 * Reference Manual it states that if a page both are invalid
		 * and have no access set, a 'access violation fault' occurs.
		 * Therefore, we must fall through here...
		 */
#ifdef nohwbug
		panic("translation fault");
#endif
	case T_ACCFLT|T_USER:
		if (frame->code < 0) { /* Check for kernel space */
			sig = SIGSEGV;
			break;
		}
	case T_ACCFLT:
#ifdef TRAPDEBUG
if(faultdebug)printf("trap accflt type %lx, code %lx, pc %lx, psl %lx\n",
			frame->trap, frame->code, frame->pc, frame->psl);
#endif
#ifdef DIAGNOSTIC
		if (p == 0)
			panic("trap: access fault: addr %lx code %lx",
			    frame->pc, frame->code);
#endif
		/*
		 * First check for ptefetch. Can only happen to pages
		 * in user space.
		 */
		if (frame->trap & T_PTEFETCH) {
			pm = p->p_vmspace->vm_map.pmap;
			if (frame->code < 0x40000000) {
				addr = trunc_page((unsigned)&pm->pm_p0br[
				    frame->code >> VAX_PGSHIFT]);
#ifdef DEBUG
			} else if (frame->code < 0) {
				panic("ptefetch in kernel");
#endif
			} else {
				addr = trunc_page((unsigned)&pm->pm_p1br[
				    (frame->code & 0x3fffffff) >> VAX_PGSHIFT]);
			}
			rv = uvm_fault(pte_map, addr, 0,
			    VM_PROT_WRITE|VM_PROT_READ);
			if (rv != KERN_SUCCESS)
				goto ufault;
		}
		addr = trunc_page(frame->code);
		if ((umode == 0) && (frame->code < 0))
			map = kernel_map;
		else
			map = &p->p_vmspace->vm_map;

		if (frame->trap & T_WRITE)
			ftype = VM_PROT_WRITE|VM_PROT_READ;
		else
			ftype = VM_PROT_READ;

		rv = uvm_fault(map, addr, 0, ftype);
		if (rv != KERN_SUCCESS) {
			if (umode == 0) {
				FAULTCHK;
				panic("Segv in kernel mode: pc %x addr %x",
				    (u_int)frame->pc, (u_int)frame->code);
			}
ufault:			if (rv == KERN_RESOURCE_SHORTAGE) {
				printf("UVM: pid %d (%s), uid %d killed: "
				       "out of swap\n",
				       p->p_pid, p->p_comm,
				       p->p_cred && p->p_ucred ?
				       p->p_ucred->cr_uid : -1);
				sig = SIGKILL;
			} else {
				sig = SIGSEGV;
			}
		} else
			trapsig = 0;
		break;

	case T_PTELEN:
		if (p && p->p_addr)
			FAULTCHK;
		panic("ptelen fault in system space: addr %lx pc %lx",
		    frame->code, frame->pc);

	case T_PTELEN|T_USER:	/* Page table length exceeded */
		sig = SIGSEGV;
		break;

	case T_BPTFLT|T_USER:
	case T_TRCTRAP|T_USER:
		sig = SIGTRAP;
		frame->psl &= ~PSL_T;
		break;

	case T_PRIVINFLT|T_USER:
	case T_RESADFLT|T_USER:
	case T_RESOPFLT|T_USER:
		sig = SIGILL;
		break;

	case T_XFCFLT|T_USER:
		sig = SIGEMT;
		break;

	case T_ARITHFLT|T_USER:
		sig = SIGFPE;
		break;

	case T_ASTFLT|T_USER:
		mtpr(AST_NO,PR_ASTLVL);
		trapsig = 0;
		break;

#ifdef DDB
	case T_BPTFLT: /* Kernel breakpoint */
	case T_KDBTRAP:
	case T_TRCTRAP:
		kdb_trap(frame);
		return;
#endif
	}

	if (trapsig)
		trapsignal(p, sig, frame->code);

	if (umode == 0)
		return;

	while ((sig = CURSIG(p)) !=0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before
		 * we swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		splstatclock();
		setrunqueue(p);
		mi_switch();
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	if (p->p_flag & P_PROFIL) { 
		extern int psratio;
		addupc_task(p, frame->pc, (int)(p->p_sticks-oticks) * psratio);
	}
	curpriority = p->p_priority;
}

void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct trapframe *exptr;

	exptr = p->p_addr->u_pcb.framep;
	exptr->pc = pack->ep_entry + 2;
	exptr->sp = stack;
}

void
syscall(frame)
	struct	trapframe *frame;
{
	struct sysent *callp;
	u_quad_t oticks;
	int nsys, sig;
	int err, rval[2], args[8];
	struct trapframe *exptr;
	struct proc *p = curproc;

#ifdef TRAPDEBUG
if(startsysc)printf("trap syscall %s pc %lx, psl %lx, sp %lx, pid %d, frame %p\n",
	       syscallnames[frame->code], frame->pc, frame->psl,frame->sp,
		curproc->p_pid,frame);
#endif
	uvmexp.syscalls++;
 
	exptr = p->p_addr->u_pcb.framep = frame;
	callp = p->p_emul->e_sysent;
	nsys = p->p_emul->e_nsysent;
	oticks = p->p_sticks;

	if(frame->code == SYS___syscall){
		int g = *(int *)(frame->ap);

		frame->code = *(int *)(frame->ap + 4);
		frame->ap += 8;
		*(int *)(frame->ap) = g - 2;
	}

	if(frame->code < 0 || frame->code >= nsys)
		callp += p->p_emul->e_nosys;
	else
		callp += frame->code;

	rval[0] = 0;
	rval[1] = frame->r1;
	if(callp->sy_narg) {
		err = copyin((char*)frame->ap + 4, args, callp->sy_argsize);
		if (err) {
#ifdef KTRACE
			if (KTRPOINT(p, KTR_SYSCALL))
				ktrsyscall(p->p_tracep, frame->code,
				    callp->sy_argsize, args);
#endif
			goto bad;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, frame->code, callp->sy_argsize, args);
#endif
	err = (*callp->sy_call)(curproc, args, rval);
	exptr = curproc->p_addr->u_pcb.framep;

#ifdef TRAPDEBUG
if(startsysc)
	printf("retur %s pc %lx, psl %lx, sp %lx, pid %d, v{rde %d r0 %d, r1 %d, frame %p\n",
	       syscallnames[exptr->code], exptr->pc, exptr->psl,exptr->sp,
		curproc->p_pid,err,rval[0],rval[1],exptr);
#endif

bad:
	switch (err) {
	case 0:
		exptr->r1 = rval[1];
		exptr->r0 = rval[0];
		exptr->psl &= ~PSL_C;
		break;

	case EJUSTRETURN:
		return;

	case ERESTART:
		exptr->pc -= (exptr->code > 63 ? 4 : 2);
		break;

	default:
		exptr->r0 = err;
		exptr->psl |= PSL_C;
		break;
	}
	p = curproc;
	while ((sig = CURSIG(p)) !=0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before
		 * we swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		splstatclock();
		setrunqueue(p);
		mi_switch();
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	if (p->p_flag & P_PROFIL) { 
		extern int psratio;
		addupc_task(p, frame->pc, (int)(p->p_sticks-oticks) * psratio);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, frame->code, err, rval[0]);
#endif
}
