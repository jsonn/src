/*	$NetBSD: trap.c,v 1.44.2.2 2000/11/22 16:00:04 bouyer Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: trap.c 1.32 91/04/06$
 *
 *	@(#)trap.c	7.15 (Berkeley) 8/2/91
 */

#include "opt_ddb.h"
#include "opt_syscall_debug.h"
#include "opt_execfmt.h"
#include "opt_ktrace.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_sunos.h"
#include "opt_compat_linux.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/syslog.h>
#include <sys/syscall.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <m68k/cpu.h>
#include <m68k/cacheops.h>

#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/mtpr.h>
#include <machine/pte.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif

#ifdef DEBUG
#include <dev/cons.h>  /* cngetc() */
#endif

#ifdef FPU_EMULATE
#include <m68k/fpe/fpu_emulate.h>
#endif

#ifdef COMPAT_SUNOS
#include <compat/sunos/sunos_syscall.h>
extern struct emul emul_sunos;
#endif

#ifdef COMPAT_LINUX
extern struct emul emul_linux;
#endif

void syscall __P((register_t, struct frame));
void trap __P((int, u_int, u_int, struct frame));

static void panictrap __P((int, u_int, u_int, struct frame *));
static void trapcpfault __P((struct proc *, struct frame *));
static void userret __P((struct proc *, struct frame *fp, u_quad_t, u_int,int));
#ifdef M68040
static int  writeback __P((struct frame *, int));
#endif /* M68040 */

char	*trap_type[] = {
	"Bus error",
	"Address error",
	"Illegal instruction",
	"Zero divide",
	"CHK instruction",
	"TRAPV instruction",
	"Privilege violation",
	"Trace trap",
	"MMU fault",
	"SSIR trap",
	"Format error",
	"68881 exception",
	"Coprocessor violation",
	"Async system trap"
};
int	trap_types = sizeof trap_type / sizeof trap_type[0];

/*
 * Size of various exception stack frames (minus the standard 8 bytes)
 */
short	exframesize[] = {
	FMT0SIZE,	/* type 0 - normal (68020/030/040/060) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040/060) */
	FMT3SIZE,	/* type 3 - FP post-instruction (68040/060) */
	FMT4SIZE,	/* type 4 - access error/fp disabled (68060) */
	-1, -1,		/* type 5-6 - undefined */
	FMT7SIZE,	/* type 7 - access error (68040) */
	58,		/* type 8 - bus fault (68010) */
	FMT9SIZE,	/* type 9 - coprocessor mid-instruction (68020/030) */
	FMTASIZE,	/* type A - short bus fault (68020/030) */
	FMTBSIZE,	/* type B - long bus fault (68020/030) */
	-1, -1, -1, -1	/* type C-F - undefined */
};

#ifdef M68060
#define	KDFAULT_060(c)	(cputype == CPU_68060 && ((c) & FSLW_TM_SV))
#define	WRFAULT_060(c)	(cputype == CPU_68060 && ((c) & FSLW_RW_W))
#else
#define	KDFAULT_060(c)	0
#define	WRFAULT_060(c)	0
#endif

#ifdef M68040
#define	KDFAULT_040(c)	(cputype == CPU_68040 && \
			 ((c) & SSW4_TMMASK) == SSW4_TMKD)
#define	WRFAULT_040(c)	(cputype == CPU_68040 && \
			 ((c) & SSW4_RW) == 0)
#else
#define	KDFAULT_040(c)	0
#define	WRFAULT_040(c)	0
#endif

#if defined(M68030) || defined(M68020)
#define	KDFAULT_OTH(c)	(cputype <= CPU_68030 && \
			 ((c) & (SSW_DF|SSW_FCMASK)) == (SSW_DF|FC_SUPERD))
#define	WRFAULT_OTH(c)	(cputype <= CPU_68030 && \
			 ((c) & (SSW_DF|SSW_RW)) == SSW_DF)
#else
#define	KDFAULT_OTH(c)	0
#define	WRFAULT_OTH(c)	0
#endif

#define	KDFAULT(c)	(KDFAULT_060(c) || KDFAULT_040(c) || KDFAULT_OTH(c))
#define	WRFAULT(c)	(WRFAULT_060(c) || WRFAULT_040(c) || WRFAULT_OTH(c))

#ifdef DEBUG
int mmudebug = 0;
int mmupid   = -1;
#define MDB_FOLLOW	1
#define MDB_WBFOLLOW	2
#define MDB_WBFAILED	4
#define MDB_ISPID(pid)	((pid) == mmupid)
#endif

extern struct pcb *curpcb;

/*
 * trap and syscall both need the following work done before returning
 * to user mode.
 */
static inline void
userret(p, fp, oticks, faultaddr, fromtrap)
	register struct proc *p;
	register struct frame *fp;
	u_quad_t oticks;
	u_int faultaddr;
	int fromtrap;
{
	int sig;
#ifdef M68040
	int beenhere = 0;

again:
#endif
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
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, fp->f_pc,
			    (int)(p->p_sticks - oticks) * psratio);
	}
#ifdef M68040
	/*
	 * Deal with user mode writebacks (from trap, or from sigreturn).
	 * If any writeback fails, go back and attempt signal delivery.
	 * unless we have already been here and attempted the writeback
	 * (e.g. bad address with user ignoring SIGSEGV).  In that case
	 * we just return to the user without sucessfully completing
	 * the writebacks.  Maybe we should just drop the sucker?
	 */
	if (cputype == CPU_68040 && fp->f_format == FMT7) {
		if (beenhere) {
#ifdef DEBUG
			if (mmudebug & MDB_WBFAILED)
				printf(fromtrap ?
		"pid %d(%s): writeback aborted, pc=%x, fa=%x\n" :
		"pid %d(%s): writeback aborted in sigreturn, pc=%x\n",
				    p->p_pid, p->p_comm, fp->f_pc, faultaddr);
#endif
		} else if ((sig = writeback(fp, fromtrap))) {
			beenhere = 1;
			oticks = p->p_sticks;
			trapsignal(p, sig, faultaddr);
			goto again;
		}
	}
#endif
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

static void
panictrap(type, code, v, fp)
	int type;
	u_int code, v;
	struct frame *fp;
{
	int	s;

	printf("trap type %d, code = %x, v = %x\n", type, code, v);
	printf("%s program counter = 0x%x\n",
			(type & T_USER) ? "user" : "kernel", fp->f_pc);

	/*
	 * Let the kernel debugger see the trap frame that
	 * caused us to panic.  This is a convenience so
	 * one can see registers at the point of failure.
	 */
	s = splhigh();
#ifdef KGDB
	/* If connected, step or cont returns 1 */
	if (kgdb_trap(type, &fp))
		goto kgdb_cont;
#endif
#ifdef DDB
	(void)kdb_trap(type, (db_regs_t *)fp);
#endif
#ifdef KGDB
kgdb_cont:
#endif
	splx(s);

	if (panicstr) {
		printf("Double panic\n");
#ifdef DEBUG
		/* XXX Should be a machine dependent hook */
		printf("(press a key)\n"); (void)cngetc();
#endif
	}

	regdump((struct trapframe *)fp, 128);
	DCIS(); /* XXX? push cache */

	type &= ~T_USER;
	if ((u_int)type < trap_types)
		panic(trap_type[type]);
	panic("trap");
	/*NOTREACHED*/
}

/*
 * return to fault handler
 */
static void
trapcpfault(p, fp)
	struct proc *p;
	struct frame *fp;
{
	/*
	 * We have arranged to catch this fault in one of the
	 * copy to/from user space routines, set PC to return to
	 * indicated location and set flag informing buserror code
	 * that it may need to clean up stack frame.
	 */
	fp->f_stackadj = exframesize[fp->f_format];
	fp->f_format = fp->f_vector = 0;
	fp->f_pc = (int) p->p_addr->u_pcb.pcb_onfault;
}

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
void
trap(type, code, v, frame)
	int		type;
	u_int		code, v;
	struct frame	frame;
{
	struct proc	*p;
	u_int		ucode;
	u_quad_t	sticks;
	int		i;
	extern char	fubail[], subail[];

	p = curproc;
	sticks = ucode = 0;

	uvmexp.traps++;

	/* I have verified that this DOES happen! -gwr */
	if (p == NULL)
		p = &proc0;
#ifdef DIAGNOSTIC
	if (p->p_addr == NULL)
		panic("trap: no pcb");
#endif

	if (USERMODE(frame.f_sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = frame.f_regs;
	}
	switch (type) {
	default:
		panictrap(type, code, v, &frame);
	/*
	 * Kernel Bus error
	 */
	case T_BUSERR:
		if (p->p_addr->u_pcb.pcb_onfault == 0)
			panictrap(type, code, v, &frame);
		trapcpfault(p, &frame);
		return;
	/*
	 * User Bus/Addr error.
	 */
	case T_BUSERR|T_USER:
	case T_ADDRERR|T_USER:
		ucode = v;
		i = SIGBUS;
		break;

	/* 
	 * Kernel coprocessor violation
	 */
	case T_COPERR:
		/*FALLTHROUGH*/
	/*
	 * Kernel/User format error
	 */
	case T_FMTERR|T_USER:	/* do all RTE errors come in as T_USER? */
	case T_FMTERR:
		/*
		 * The user has most likely trashed the RTE or FP state info
		 * in the stack frame of a signal handler.
		 */
		type |= T_USER;
#ifdef DEBUG
		printf("pid %d: kernel %s exception\n", p->p_pid,
		    type==T_COPERR ? "coprocessor" : "format");
#endif
		p->p_sigacts->ps_sigact[SIGILL].sa_handler = SIG_DFL;
		sigdelset(&p->p_sigignore, SIGILL);
		sigdelset(&p->p_sigcatch, SIGILL);
		sigdelset(&p->p_sigmask, SIGILL);
		i = SIGILL;
		ucode = frame.f_format;	/* XXX was ILL_RESAD_FAULT */
		break;

	/* 
	 * User coprocessor violation
	 */
	case T_COPERR|T_USER:
		ucode = 0;
		i = SIGFPE;	/* XXX What is a proper response here? */
		break;

	/* 
	 * 6888x exceptions 
	 */
	case T_FPERR|T_USER:
		/*
		 * We pass along the 68881 status register which locore
		 * stashed in code for us.  Note that there is a
		 * possibility that the bit pattern of this register
		 * will conflict with one of the FPE_* codes defined
		 * in signal.h.  Fortunately for us, the only such
		 * codes we use are all in the range 1-7 and the low
		 * 3 bits of the status register are defined as 0 so
		 * there is no clash.
		 */
		ucode = code;
		i = SIGFPE;
		break;

	/*
	 * Unimplemented FPU instructions/datatypes.
	 */
	case T_FPEMULI|T_USER:
	case T_FPEMULD|T_USER:
#ifdef FPU_EMULATE
		i = fpu_emulate(&frame, &p->p_addr->u_pcb.pcb_fpregs);
		/* XXX -- deal with tracing? (frame.f_sr & PSL_T) */
#else
		uprintf("pid %d killed: no floating point support.\n",
			p->p_pid);
		i = SIGILL;
#endif
		break;

	/*
	 * FPU faults in supervisor mode.
	 */
	case T_FPEMULI:
	case T_FPEMULD: {
		extern int	*nofault;

		if (nofault)	/* If we're probing. */
			longjmp((label_t *) nofault);
		panictrap(type, code, v, &frame);
	}

	/*
	 * User illegal/privileged inst fault
	 */
	case T_ILLINST|T_USER:
	case T_PRIVINST|T_USER:
		ucode = frame.f_format;	/* XXX was ILL_PRIVIN_FAULT */
		i = SIGILL;
		break;

	/*
	 * divde by zero, CHK/TRAPV inst 
	 */
	case T_ZERODIV|T_USER:
	case T_CHKINST|T_USER:
	case T_TRAPVINST|T_USER:
		ucode = frame.f_format;
		i = SIGFPE;
		break;
	/*
	 * XXX: Trace traps are a nightmare.
	 *
	 *	HP-UX uses trap #1 for breakpoints,
	 *	NetBSD/m68k uses trap #2,
	 *	SUN 3.x uses trap #15,
	 *	DDB and KGDB use trap #15 (for kernel breakpoints;
	 *	handled elsewhere).
	 *
	 * NetBSD and HP-UX traps get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 *
	 * XXX: We should never get kernel-mode T_TRAP15
	 * XXX: because locore.s now gives them special treatment.
	 */
	case T_TRAP15:
		frame.f_sr &= ~PSL_T;
		return;

	case T_TRACE|T_USER:
#ifdef COMPAT_SUNOS
		/*
		 * SunOS uses Trap #2 for a "CPU cache flush".
		 * Just flush the on-chip caches and return.
		 */
		if (p->p_emul == &emul_sunos) {
			ICIA();
			DCIU();
			return;
		}
#endif
		/* FALLTHROUGH */
	case T_TRACE:		/* tracing a trap instruction */
	case T_TRAP15|T_USER:
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;
	/* 
	 * Kernel AST (should not happen)
	 */
	case T_ASTFLT:
		panictrap(type, code, v, &frame);
	/*
	 * User AST
	 */
	case T_ASTFLT|T_USER:
		astpending = 0;
		/*
		 * We check for software interrupts first.  This is because
		 * they are at a higher level than ASTs, and on a VAX would
		 * interrupt the AST.  We assume that if we are processing
		 * an AST that we must be at IPL0 so we don't bother to
		 * check.  Note that we ensure that we are at least at SIR
		 * IPL while processing the SIR.
		 */
		spl1();
		/*FALLTHROUGH*/
	/*
	 * Software interrupt
	 */
	case T_SSIR:
	case T_SSIR|T_USER:
		if(ssir)
			softint();
		/*
		 * If this was not an AST trap, we are all done.
		 */
		if (type != (T_ASTFLT|T_USER)) {
			uvmexp.traps--;
			return;
		}
		spl0();
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto out;
	/*
	 * Kernel/User page fault
	 */
	case T_MMUFLT:
		/*
		 * If we were doing profiling ticks or other user mode
		 * stuff from interrupt code, Just Say No.
		 */
		if (p->p_addr->u_pcb.pcb_onfault == (caddr_t)fubail ||
		    p->p_addr->u_pcb.pcb_onfault == (caddr_t)subail) {
			trapcpfault(p, &frame);
			return;
		}
		/*FALLTHROUGH*/
	case T_MMUFLT|T_USER:	/* page fault */
	    {
		register vaddr_t	va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t	map;
		int			rv;
		vm_prot_t		ftype;
		extern vm_map_t		kernel_map;

#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
		printf("trap: T_MMUFLT pid=%d, code=%x, v=%x, pc=%x, sr=%x\n",
		       p ? p->p_pid : -1, code, v, frame.f_pc, frame.f_sr);
#endif
		/*
		 * It is only a kernel address space fault iff:
		 * 	1. (type & T_USER) == 0  and
		 * 	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_MMUFLT &&
		    ((p->p_addr->u_pcb.pcb_onfault == 0) || KDFAULT(code)))
			map = kernel_map;
		else
			map = vm ? &vm->vm_map : kernel_map;
		if (WRFAULT(code))
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		va = trunc_page(v);
#ifdef DEBUG
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %x\n", v);
			panictrap(type, code, v, &frame);
		}
#endif
		rv = uvm_fault(map, va, 0, ftype);
#ifdef DEBUG
		if (rv && MDB_ISPID(p->p_pid))
			printf("vm_fault(%p, %lx, %x, 0) -> %x\n",
			       map, va, ftype, rv);
#endif
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((vm != NULL && (caddr_t)va >= vm->vm_maxsaddr)
		    && map != kernel_map) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = btoc(USRSTACK-(unsigned)va);
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			if (type == T_MMUFLT) {
#ifdef M68040
				if (cputype == CPU_68040)
					(void) writeback(&frame, 1);
#endif
				return;
			}
			goto out;
		}
		if (type == T_MMUFLT) {
			if (p->p_addr->u_pcb.pcb_onfault) {
				trapcpfault(p, &frame);
				return;
			}
			printf("\nvm_fault(%p, %lx, %x, 0) -> %x\n",
			       map, va, ftype, rv);
			printf("  type %x, code [mmu,,ssw]: %x\n",
			       type, code);
			panictrap(type, code, v, &frame);
		}
		ucode = v;
		if (rv == KERN_RESOURCE_SHORTAGE) {
			printf("UVM: pid %d (%s), uid %d killed: out of swap\n",
			       p->p_pid, p->p_comm,
			       p->p_cred && p->p_ucred ?
			       p->p_ucred->cr_uid : -1);
			i = SIGKILL;
		} else {
			i = SIGSEGV;
		}
		break;
	    }
	}

	if (i)
		trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
out:
	userret(p, &frame, sticks, v, 1); 
}

#ifdef M68040
#ifdef DEBUG
struct writebackstats {
	int calls;
	int cpushes;
	int move16s;
	int wb1s, wb2s, wb3s;
	int wbsize[4];
} wbstats;

char *f7sz[] = { "longword", "byte", "word", "line" };
char *f7tt[] = { "normal", "MOVE16", "AFC", "ACK" };
char *f7tm[] = { "d-push", "u-data", "u-code", "M-data",
		 "M-code", "k-data", "k-code", "RES" };
char wberrstr[] =
    "WARNING: pid %d(%s) writeback [%s] failed, pc=%x fa=%x wba=%x wbd=%x\n";

static void dumpwb __P((int, u_short, u_int, u_int));
static void dumpssw __P((u_short));
#endif /* DEBUG */

static int
writeback(fp, docachepush)
	struct frame *fp;
	int docachepush;
{
	register struct fmt7 *f = &fp->f_fmt7;
	register struct proc *p = curproc;
	int	err = 0;
	u_int	fa = 0;
	caddr_t oonfault = p->p_addr->u_pcb.pcb_onfault;
	paddr_t pa;

#ifdef DEBUG
	if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid)) {
		printf(" pid=%d, fa=%x,", p->p_pid, f->f_fa);
		dumpssw(f->f_ssw);
	}
	wbstats.calls++;
#endif
	/*
	 * Deal with special cases first.
	 */
	if ((f->f_ssw & SSW4_TMMASK) == SSW4_TMDCP) {
		/*
		 * Dcache push fault.
		 * Line-align the address and write out the push data to
		 * the indicated physical address.
		 */
#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid)) {
			printf(" pushing %s to PA %x, data %x",
			       f7sz[(f->f_ssw & SSW4_SZMASK) >> 5],
			       f->f_fa, f->f_pd0);
			if ((f->f_ssw & SSW4_SZMASK) == SSW4_SZLN)
				printf("/%x/%x/%x",
				       f->f_pd1, f->f_pd2, f->f_pd3);
			printf("\n");
		}
		if (f->f_wb1s & SSW4_WBSV)
			panic("writeback: cache push with WB1S valid");
		wbstats.cpushes++;
#endif
		/*
		 * XXX there are security problems if we attempt to do a
		 * cache push after a signal handler has been called.
		 */
		if (docachepush) {
			pmap_enter(pmap_kernel(), (vaddr_t)vmmap,
			    trunc_page(f->f_fa), VM_PROT_WRITE,
			    VM_PROT_WRITE|PMAP_WIRED);
			fa = (u_int)&vmmap[(f->f_fa & PGOFSET) & ~0xF];
			bcopy((caddr_t)&f->f_pd0, (caddr_t)fa, 16);
			(void) pmap_extract(pmap_kernel(), (vaddr_t)fa, &pa);
			DCFL(pa);
			pmap_remove(pmap_kernel(), (vaddr_t)vmmap,
				    (vaddr_t)&vmmap[NBPG]);
		} else
			printf("WARNING: pid %d(%s) uid %d: CPUSH not done\n",
			       p->p_pid, p->p_comm, p->p_ucred->cr_uid);
	} else if ((f->f_ssw & (SSW4_RW|SSW4_TTMASK)) == SSW4_TTM16) {
		/*
		 * MOVE16 fault.
		 * Line-align the address and write out the push data to
		 * the indicated virtual address.
		 */
#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
			printf(" MOVE16 to VA %x(%x), data %x/%x/%x/%x\n",
			       f->f_fa, f->f_fa & ~0xF, f->f_pd0, f->f_pd1,
			       f->f_pd2, f->f_pd3);
		if (f->f_wb1s & SSW4_WBSV)
			panic("writeback: MOVE16 with WB1S valid");
		wbstats.move16s++;
#endif
		if (KDFAULT(f->f_wb1s))
			bcopy((caddr_t)&f->f_pd0, (caddr_t)(f->f_fa & ~0xF), 16);
		else
			err = suline((caddr_t)(f->f_fa & ~0xF), (caddr_t)&f->f_pd0);
		if (err) {
			fa = f->f_fa & ~0xF;
#ifdef DEBUG
			if (mmudebug & MDB_WBFAILED)
				printf(wberrstr, p->p_pid, p->p_comm,
				       "MOVE16", fp->f_pc, f->f_fa,
				       f->f_fa & ~0xF, f->f_pd0);
#endif
		}
	} else if (f->f_wb1s & SSW4_WBSV) {
		/*
		 * Writeback #1.
		 * Position the "memory-aligned" data and write it out.
		 */
		register u_int wb1d = f->f_wb1d;
		register int off;

#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
			dumpwb(1, f->f_wb1s, f->f_wb1a, f->f_wb1d);
		wbstats.wb1s++;
		wbstats.wbsize[(f->f_wb2s&SSW4_SZMASK)>>5]++;
#endif
		off = (f->f_wb1a & 3) * 8;
		switch (f->f_wb1s & SSW4_SZMASK) {
		case SSW4_SZLW:
			if (off)
				wb1d = (wb1d >> (32 - off)) | (wb1d << off);
			if (KDFAULT(f->f_wb1s))
				*(long *)f->f_wb1a = wb1d;
			else
				err = suword((caddr_t)f->f_wb1a, wb1d);
			break;
		case SSW4_SZB:
			off = 24 - off;
			if (off)
				wb1d >>= off;
			if (KDFAULT(f->f_wb1s))
				*(char *)f->f_wb1a = wb1d;
			else
				err = subyte((caddr_t)f->f_wb1a, wb1d);
			break;
		case SSW4_SZW:
			off = (off + 16) % 32;
			if (off)
				wb1d = (wb1d >> (32 - off)) | (wb1d << off);
			if (KDFAULT(f->f_wb1s))
				*(short *)f->f_wb1a = wb1d;
			else
				err = susword((caddr_t)f->f_wb1a, wb1d);
			break;
		}
		if (err) {
			fa = f->f_wb1a;
#ifdef DEBUG
			if (mmudebug & MDB_WBFAILED)
				printf(wberrstr, p->p_pid, p->p_comm,
				       "#1", fp->f_pc, f->f_fa,
				       f->f_wb1a, f->f_wb1d);
#endif
		}
	}
	/*
	 * Deal with the "normal" writebacks.
	 *
	 * XXX writeback2 is known to reflect a LINE size writeback after
	 * a MOVE16 was already dealt with above.  Ignore it.
	 */
	if (err == 0 && (f->f_wb2s & SSW4_WBSV) &&
	    (f->f_wb2s & SSW4_SZMASK) != SSW4_SZLN) {
#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
			dumpwb(2, f->f_wb2s, f->f_wb2a, f->f_wb2d);
		wbstats.wb2s++;
		wbstats.wbsize[(f->f_wb2s&SSW4_SZMASK)>>5]++;
#endif
		switch (f->f_wb2s & SSW4_SZMASK) {
		case SSW4_SZLW:
			if (KDFAULT(f->f_wb2s))
				*(long *)f->f_wb2a = f->f_wb2d;
			else
				err = suword((caddr_t)f->f_wb2a, f->f_wb2d);
			break;
		case SSW4_SZB:
			if (KDFAULT(f->f_wb2s))
				*(char *)f->f_wb2a = f->f_wb2d;
			else
				err = subyte((caddr_t)f->f_wb2a, f->f_wb2d);
			break;
		case SSW4_SZW:
			if (KDFAULT(f->f_wb2s))
				*(short *)f->f_wb2a = f->f_wb2d;
			else
				err = susword((caddr_t)f->f_wb2a, f->f_wb2d);
			break;
		}
		if (err) {
			fa = f->f_wb2a;
#ifdef DEBUG
			if (mmudebug & MDB_WBFAILED) {
				printf(wberrstr, p->p_pid, p->p_comm,
				       "#2", fp->f_pc, f->f_fa,
				       f->f_wb2a, f->f_wb2d);
				dumpssw(f->f_ssw);
				dumpwb(2, f->f_wb2s, f->f_wb2a, f->f_wb2d);
			}
#endif
		}
	}
	if (err == 0 && (f->f_wb3s & SSW4_WBSV)) {
#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
			dumpwb(3, f->f_wb3s, f->f_wb3a, f->f_wb3d);
		wbstats.wb3s++;
		wbstats.wbsize[(f->f_wb3s&SSW4_SZMASK)>>5]++;
#endif
		switch (f->f_wb3s & SSW4_SZMASK) {
		case SSW4_SZLW:
			if (KDFAULT(f->f_wb3s))
				*(long *)f->f_wb3a = f->f_wb3d;
			else
				err = suword((caddr_t)f->f_wb3a, f->f_wb3d);
			break;
		case SSW4_SZB:
			if (KDFAULT(f->f_wb3s))
				*(char *)f->f_wb3a = f->f_wb3d;
			else
				err = subyte((caddr_t)f->f_wb3a, f->f_wb3d);
			break;
		case SSW4_SZW:
			if (KDFAULT(f->f_wb3s))
				*(short *)f->f_wb3a = f->f_wb3d;
			else
				err = susword((caddr_t)f->f_wb3a, f->f_wb3d);
			break;
#ifdef DEBUG
		case SSW4_SZLN:
			panic("writeback: wb3s indicates LINE write");
#endif
		}
		if (err) {
			fa = f->f_wb3a;
#ifdef DEBUG
			if (mmudebug & MDB_WBFAILED)
				printf(wberrstr, p->p_pid, p->p_comm,
				       "#3", fp->f_pc, f->f_fa,
				       f->f_wb3a, f->f_wb3d);
#endif
		}
	}
	p->p_addr->u_pcb.pcb_onfault = oonfault;
	if (err)
		err = SIGSEGV;
	return(err);
}

#ifdef DEBUG
static void
dumpssw(ssw)
	register u_short ssw;
{
	printf(" SSW: %x: ", ssw);
	if (ssw & SSW4_CP)
		printf("CP,");
	if (ssw & SSW4_CU)
		printf("CU,");
	if (ssw & SSW4_CT)
		printf("CT,");
	if (ssw & SSW4_CM)
		printf("CM,");
	if (ssw & SSW4_MA)
		printf("MA,");
	if (ssw & SSW4_ATC)
		printf("ATC,");
	if (ssw & SSW4_LK)
		printf("LK,");
	if (ssw & SSW4_RW)
		printf("RW,");
	printf(" SZ=%s, TT=%s, TM=%s\n",
	       f7sz[(ssw & SSW4_SZMASK) >> 5],
	       f7tt[(ssw & SSW4_TTMASK) >> 3],
	       f7tm[ssw & SSW4_TMMASK]);
}

static void
dumpwb(num, s, a, d)
	int num;
	u_short s;
	u_int a, d;
{
	register struct proc *p = curproc;
	paddr_t pa;

	printf(" writeback #%d: VA %x, data %x, SZ=%s, TT=%s, TM=%s\n",
	       num, a, d, f7sz[(s & SSW4_SZMASK) >> 5],
	       f7tt[(s & SSW4_TTMASK) >> 3], f7tm[s & SSW4_TMMASK]);
	printf("               PA ");
	if (pmap_extract(p->p_vmspace->vm_map.pmap, (vaddr_t)a, &pa) == FALSE)
		printf("<invalid address>");
	else
		printf("%lx, current value %lx", pa, fuword((caddr_t)a));
	printf("\n");
}
#endif /* DEBUG  */
#endif /* M68040 */

/*
 * Process a system call.
 */
void
syscall(code, frame)
	register_t code;
	struct frame frame;
{
	register caddr_t params;
	register const struct sysent *callp;
	register struct proc *p;
	int error, opc, nsys;
	size_t argsize;
	register_t args[8], rval[2];
	u_quad_t sticks;

	uvmexp.syscalls++;
	if (!USERMODE(frame.f_sr))
		panic("syscall");
	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_regs = frame.f_regs;
	opc = frame.f_pc;

	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;
#ifdef COMPAT_SUNOS
	if (p->p_emul == &emul_sunos) {

		/*
		 * SunOS passes the syscall-number on the stack, whereas
		 * BSD passes it in D0. So, we have to get the real "code"
		 * from the stack, and clean up the stack, as SunOS glue
		 * code assumes the kernel pops the syscall argument the
		 * glue pushed on the stack. Sigh...
		 */
		code = fuword((caddr_t)frame.f_regs[SP]);

		/*
		 * XXX
		 * Don't do this for sunos_sigreturn, as there's no stored pc
		 * on the stack to skip, the argument follows the syscall
		 * number without a gap.
		 */
		if (code != SUNOS_SYS_sigreturn) {
			frame.f_regs[SP] += sizeof (int);
			/*
			 * remember that we adjusted the SP, 
			 * might have to undo this if the system call
			 * returns ERESTART.
			 */
			p->p_md.md_flags |= MDP_STACKADJ;
		} else
			p->p_md.md_flags &= ~MDP_STACKADJ;
	}
#endif

	params = (caddr_t)frame.f_regs[SP] + sizeof(int);

	switch (code) {
	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		/*
		 * XXX sigreturn requires special stack manipulation
		 * that is only done if entered via the sigreturn
		 * trap.  Cannot allow it here so make sure we fail.
		 */
		switch (code) {
#ifdef COMPAT_13
		case SYS_compat_13_sigreturn13:
#endif
		case SYS___sigreturn14:
			code = nsys;
			break;
		}
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
#ifdef COMPAT_LINUX
	if (p->p_emul == &emul_linux) {
		/*
		 * Linux passes the args in d1-d5
		 */
		switch (argsize) {
		case 20:
			args[4] = frame.f_regs[D5];
		case 16:
			args[3] = frame.f_regs[D4];
		case 12:
			args[2] = frame.f_regs[D3];
		case 8:
			args[1] = frame.f_regs[D2];
		case 4:
			args[0] = frame.f_regs[D1];
		case 0:
			error = 0;
			break;
		default:
#ifdef DEBUG
			panic("linux syscall %d weird argsize %d",
				code, argsize);
#else
			error = EINVAL;
#endif
			break;
		}
	} else
#endif
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
	rval[1] = frame.f_regs[D1];
	error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.f_regs[D0] = rval[0];
		frame.f_regs[D1] = rval[1];
		frame.f_sr &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/*
		 * We always enter through a `trap' instruction, which is 2
		 * bytes, so adjust the pc by that amount.
		 */
		frame.f_pc = opc - 2;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame.f_regs[D0] = error;
		frame.f_sr |= PSL_C;	/* carry bit */
		break;	
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
#ifdef COMPAT_SUNOS
	/* need new p-value for this */
	if (error == ERESTART && (p->p_md.md_flags & MDP_STACKADJ))
		frame.f_regs[SP] -= sizeof (int);
#endif
	userret(p, &frame, sticks, (u_int)0, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, error, rval[0]);
#endif
}
/*
 * Process the tail end of a fork() for the child
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	/* See cpu_fork() */
	struct frame *f = (struct frame *)p->p_md.md_regs;

	f->f_regs[D0] = 0;
	f->f_sr &= ~PSL_C;	/* carry bit */
	f->f_format = FMT0;

	userret(p, f, 0, (u_int)0, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
}
