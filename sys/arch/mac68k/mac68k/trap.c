/*	$NetBSD: trap.c,v 1.54.4.1 1997/11/11 01:40:19 mellon Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * from: Utah $Hdr: trap.c 1.37 92/12/20$
 *
 *	@(#)trap.c	8.5 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <machine/db_machdep.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/cpu.h>
#include <machine/reg.h>

#include <m68k/fpe/fpu_emulate.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#ifdef COMPAT_SUNOS
#include <compat/sunos/sunos_syscall.h>
extern struct emul emul_sunos;
#endif

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
	FMT0SIZE,	/* type 0 - normal (68020/030/040) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040) */
	FMT3SIZE,	/* type 3 - FP post-instruction (68040) */
	FMT4SIZE,	/* type 4 - LC040 FP exception (68LC040) */
	-1, -1,		/* type 5-6 - undefined */
	FMT7SIZE,	/* type 7 - access error (68040) */
	58,		/* type 8 - bus fault (68010) */
	FMT9SIZE,	/* type 9 - coprocessor mid-instruction (68020/030) */
	FMTASIZE,	/* type A - short bus fault (68020/030) */
	FMTBSIZE,	/* type B - long bus fault (68020/030) */
	-1, -1, -1, -1	/* type C-F - undefined */
};

#ifdef M68040
#define KDFAULT(c)	(mmutype == MMU_68040 ?			\
			  ((c) & SSW4_TMMASK) == SSW4_TMKD : 	\
			  ((c) & (SSW_DF|FC_SUPERD)) == (SSW_DF|FC_SUPERD))
#define WRFAULT(c)	(mmutype == MMU_68040 ?		\
			  ((c) & SSW4_RW) == 0 : 	\
			  ((c) & (SSW_DF|SSW_RW)) == SSW_DF)
#else
#define KDFAULT(c)	(((c) & (SSW_DF|SSW_FCMASK)) == (SSW_DF|FC_SUPERD))
#define WRFAULT(c)	(((c) & (SSW_DF|SSW_RW)) == SSW_DF)
#endif

#ifdef DEBUG
int mmudebug = 0;
int mmupid = -1;
#define MDB_FOLLOW	1
#define MDB_WBFOLLOW	2
#define MDB_WBFAILED	4
#define MDB_ISPID(pid)	((pid) == mmupid)
#endif

/* trap() and syscall() only called from locore */
void	trap __P((int, unsigned, register unsigned, struct frame));
void	syscall __P((register_t, struct frame));
void	child_return __P((struct proc *, struct frame)); /* XXX */

static inline void userret __P((struct proc *p, struct frame *fp,
	    u_quad_t oticks, u_int faultaddr, int fromtrap));

#if defined(M68040)
static int	writeback __P((struct frame *, int));
#if DEBUG
static void dumpssw __P((register u_short));
static void dumpwb __P((int, u_short, u_int, u_int));
#endif
#endif

/*
 * Trap and syscall both need the following work done before returning
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
	int sig, s;
#if defined(M68040)
	int beenhere = 0;

again:
#endif
	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_priority = p->p_usrpri;

	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before
		 * we switch'ed, we might not be on the queue indicated by
		 * our priority.
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
	 * If profiling, charge recent system time.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;
		
		addupc_task(p, fp->f_pc,
		    (int)(p->p_sticks - oticks) * psratio);
	}
#if defined(M68040)
	/*
	 * Deal with user mode writebacks (from trap, or from sigreturn).
	 * If any writeback fails, go back and attempt signal delivery
	 * unless we have already been here and attempted the writeback
	 * (e.g. bad address with user ignoring SIGSEGV).  In that case,
	 * we just return to the user without successfully completing
	 * the writebacks.  Maybe we should just drop the sucker?
	 */
	if (mmutype == MMU_68040 && fp->f_format == FMT7) {
		if (beenhere) {
#if DEBUG
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
	curpriority = p->p_priority;
}

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
void
trap(type, code, v, frame)
	int type;
	unsigned code;
	register unsigned v;
	struct frame frame;
{
	extern char fubail[], subail[];
#ifdef DDB
	extern char trap0[], trap1[], trap2[], trap12[], trap15[], illinst[];
#endif
	register struct proc *p;
	register int i;
	u_int ucode;
	u_quad_t sticks;

	cnt.v_trap++;
	p = curproc;
	ucode = 0;

	if (USERMODE(frame.f_sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = frame.f_regs;
	} else
		sticks = 0;

	/* I have verified that this DOES happen! -gwr */
	if (p == NULL)
		p = &proc0;
#ifdef DIAGNOSTIC
	if (p->p_addr == NULL)
		panic("trap: type 0x%x, code 0x%x, v 0x%x -- no pcb\n",
			type, code, v);
#endif

	switch (type) {
	default:
dopanic:
		printf("trap: type 0x%x, code 0x%x, v 0x%x\n", type, code, v);
#ifdef DDB
		if (kdb_trap(type, (db_regs_t *) &frame))
			return;
#endif
		regdump((struct trapframe *)&frame, 128);
		type &= ~T_USER;
		if ((unsigned)type < trap_types)
			panic(trap_type[type]);
		panic("trap");

	case T_BUSERR:		/* Kernel bus error */
		if (!p->p_addr->u_pcb.pcb_onfault)
			goto dopanic;
		/*
		 * If we have arranged to catch this fault in any of the
		 * copy to/from user space routines, set PC to return to
		 * indicated location and set flag informing buserror code
		 * that it may need to clean up stack frame.
		 */
copyfault:
		frame.f_stackadj = exframesize[frame.f_format];
		frame.f_format = frame.f_vector = 0;
		frame.f_pc = (int) p->p_addr->u_pcb.pcb_onfault;
		return;

	case T_BUSERR|T_USER:	/* Bus error */
	case T_ADDRERR|T_USER:	/* Address error */
		ucode = v;
		i = SIGBUS;
		break;

	case T_ILLINST|T_USER:	/* Illegal instruction fault */
	case T_PRIVINST|T_USER:	/* Privileged instruction fault */
		ucode = frame.f_format;	/* XXX was ILL_PRIVIN_FAULT */
		i = SIGILL;
		break;
	/*
	 * divde by zero, CHK/TRAPV inst 
	 */
	case T_ZERODIV|T_USER:		/* Divide by zero trap */
	case T_CHKINST|T_USER:		/* CHK instruction trap */
	case T_TRAPVINST|T_USER:	/* TRAPV instruction trap */
		ucode = frame.f_format;
		i = SIGFPE;
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
	 * FPU faults in supervisor mode.
	 */
	case T_ILLINST:	/* fnop generates this, apparently. */
	case T_FPEMULI:
	case T_FPEMULD: {
		extern int	*nofault;

		if (nofault)	/* If we're probing. */
			longjmp((label_t *) nofault);
		if (type == T_ILLINST)
			printf("Kernel Illegal Instruction trap.\n");
		else
			printf("Kernel FPU trap.\n");
		goto dopanic;
	}

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

	case T_COPERR:		/* Kernel coprocessor violation */
	case T_FMTERR:		/* Kernel format error */
	case T_FMTERR|T_USER:	/* User format error */
		/*
		 * The user has most likely trashed the RTE or FP state info
		 * in the stack frame of a signal handler.
		 */
		printf("pid %d: kernel %s exception\n", p->p_pid,
		    type==T_COPERR ? "coprocessor" : "format");
		type |= T_USER;
		p->p_sigacts->ps_sigact[SIGILL] = SIG_DFL;
		i = sigmask(SIGILL);
		p->p_sigignore &= ~i;
		p->p_sigcatch &= ~i;
		p->p_sigmask &= ~i;
		i = SIGILL;
		ucode = frame.f_format;	/* XXX was ILL_RESAD_FAULT */
		break;

	/*
	 * Trace traps.
	 *
	 * M68k NetBSD uses trap #2,
	 * SUN 3.x uses trap #15,
	 * KGDB uses trap #15 (for kernel breakpoints; handled elsewhere).
	 *
	 * M68k NetBSD traps get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 */
	case T_TRACE:		/* Kernel trace trap */
	case T_TRAP15:		/* SUN trace trap */
#ifdef DDB
		if (type == T_TRAP15 ||
		    ((caddr_t) frame.f_pc != trap0 &&
		     (caddr_t) frame.f_pc != trap1 &&
		     (caddr_t) frame.f_pc != trap2 &&
		     (caddr_t) frame.f_pc != trap12 &&
		     (caddr_t) frame.f_pc != trap15 &&
		     (caddr_t) frame.f_pc != illinst)) {
			if (kdb_trap(type, (db_regs_t *) &frame))
				return;
		}
#endif
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;

	case T_TRACE|T_USER:	/* user trace trap */
	case T_TRAP15|T_USER:	/* Sun user trace trap */
#ifdef COMPAT_SUNOS
		/*
		 * SunOS uses Trap #2 for a "CPU cache flush"
		 * Just flush the on-chip caches and return.
		 * XXX - Too bad NetBSD uses trap 2...
		 */
		if (p->p_emul == &emul_sunos) {
			ICIA();
			DCIU();
			/* get out fast */
			goto done;
		}
#endif
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;

	case T_ASTFLT:		/* System async trap, cannot happen */
		goto dopanic;

	case T_ASTFLT|T_USER:	/* User async trap. */
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
		/* fall into... */

	case T_SSIR:		/* Software interrupt */
	case T_SSIR|T_USER:
		if (ssir & SIR_SERIAL) {
			void zssoft __P((int));
			siroff(SIR_SERIAL);
			cnt.v_soft++;
			zssoft(0);
		}
		if (ssir & SIR_NET) {
			void netintr __P((void));
			siroff(SIR_NET);
			cnt.v_soft++;
			netintr();
		}
		if (ssir & SIR_CLOCK) {
			void softclock __P((void));
			siroff(SIR_CLOCK);
			cnt.v_soft++;
			softclock();
		}
		if (ssir & SIR_DTMGR) {
			void mrg_execute_deferred __P((void));
			siroff(SIR_DTMGR);
			cnt.v_soft++;
			mrg_execute_deferred();
		}
		if (ssir & SIR_ADB) {
			void adb_soft_intr __P((void));
			siroff(SIR_ADB);
			cnt.v_soft++;
			adb_soft_intr();
		}
		/*
		 * If this was not an AST trap, we are all done.
		 */
		if (type != (T_ASTFLT|T_USER)) {
			cnt.v_trap--;
			return;
		}
		spl0();
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto out;

	case T_MMUFLT:		/* Kernel mode page fault */
		/*
		 * If we were doing profiling ticks or other user mode
		 * stuff from interrupt code, Just Say No.
		 */
		if (p->p_addr->u_pcb.pcb_onfault == fubail ||
		    p->p_addr->u_pcb.pcb_onfault == subail)
			goto copyfault;
		/* fall into... */

	case T_MMUFLT|T_USER:	/* page fault */
	    {
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;

#ifdef DEBUG
		if ((mmudebug & MDB_WBFOLLOW) || MDB_ISPID(p->p_pid))
		printf("trap: T_MMUFLT pid=%d, code=%x, v=%x, pc=%x, sr=%x\n",
			p->p_pid, code, v, frame.f_pc, frame.f_sr);
#endif
		/*
		 * It is only a kernel address space fault iff:
		 *	1. (type & T_USER) == 0 and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_MMUFLT &&
		    (!p->p_addr->u_pcb.pcb_onfault || KDFAULT(code)))
			map = kernel_map;
		else
			map = vm ? &vm->vm_map : kernel_map;
		if (WRFAULT(code))
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		va = trunc_page((vm_offset_t) v);
#ifdef DEBUG
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %x\n", v);
			goto dopanic;
		}
#endif
		rv = vm_fault(map, va, ftype, FALSE);
#ifdef DEBUG
		if (rv && MDB_ISPID(p->p_pid))
			printf("vm_fault(%p, %lx, %x, 0) -> %x\n",
				map, va, ftype, rv);
#endif
		/*
		 * If this was a stack access, we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure, it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((vm != NULL && (caddr_t)va >= vm->vm_maxsaddr)
		    && map != kernel_map) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			if (type == T_MMUFLT) {
#if defined(M68040)
				if (mmutype == MMU_68040)
					(void) writeback(&frame, 1);
#endif
				return;
			}
			goto out;
		}
		if (type == T_MMUFLT) {
			if (p->p_addr->u_pcb.pcb_onfault)
				goto copyfault;
			printf("vm_fault(%x, %x, %x, 0) -> %x\n",
				(unsigned) map, (unsigned) va, ftype, rv);
			printf("  type %x, code [mmu,,ssw]: %x\n",
				type, code);
			goto dopanic;
		}
		ucode = v;
		i = SIGSEGV;
		break;
	    }
	}
	if (i)
		trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
out:
	userret(p, &frame, sticks, v, 1); 

#ifdef COMPAT_SUNOS
done:
#endif
}

#if defined(M68040)
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
#endif

static int
writeback(fp, docachepush)
	struct frame *fp;
	int docachepush;
{
	register struct fmt7 *f = &fp->f_fmt7;
	register struct proc *p = curproc;
	int err = 0;
	u_int fa;
	caddr_t oonfault = p->p_addr->u_pcb.pcb_onfault;

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
			pmap_enter(pmap_kernel(), (vm_offset_t)vmmap,
				   trunc_page(f->f_fa), VM_PROT_WRITE, TRUE);
			fa = (u_int)&vmmap[(f->f_fa & PGOFSET) & ~0xF];
			bcopy((caddr_t)&f->f_pd0, (caddr_t)fa, 16);
			DCFL(pmap_extract(pmap_kernel(), (vm_offset_t)fa));
			pmap_remove(pmap_kernel(), (vm_offset_t)vmmap,
				    (vm_offset_t)&vmmap[NBPG]);
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
	return (err);
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

static
void
dumpwb(num, s, a, d)
	int num;
	u_short s;
	u_int a, d;
{
	register struct proc *p = curproc;
	vm_offset_t pa;

	printf(" writeback #%d: VA %x, data %x, SZ=%s, TT=%s, TM=%s\n",
	       num, a, d, f7sz[(s & SSW4_SZMASK) >> 5],
	       f7tt[(s & SSW4_TTMASK) >> 3], f7tm[s & SSW4_TMMASK]);
	printf("               PA ");
	pa = pmap_extract(p->p_vmspace->vm_map.pmap, (vm_offset_t)a);
	if (pa == 0)
		printf("<invalid address>");
	else
		printf("%lx, current value %lx", pa, fuword((caddr_t)a));
	printf("\n");
}
#endif
#endif

/*
 * Process a system call.
 */
void
syscall(code, frame)
	register_t code;
	struct frame frame;
{
	register caddr_t params;
	register struct sysent *callp;
	register struct proc *p;
	int error, opc, nsys;
	size_t argsize;
	register_t args[8], rval[2];
	u_quad_t sticks;

	cnt.v_syscall++;
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
		 * trap.  Cannot allow here, so make sure we fail.
		 */
		if (code == SYS_sigreturn)
			code = nsys;
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
		ktrsyscall(p->p_tracep, code, argsize, args);
#endif
	if (error)
		goto bad;
	rval[0] = 0;
	rval[1] = frame.f_regs[D1];
	error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
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
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

void	child_return __P((struct proc *, struct frame));

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(p, frame)
	struct proc	*p;
	struct frame	frame;
{

	frame.f_regs[D0] = 0;	/* Return value. */
	frame.f_sr &= ~PSL_C;	/* carry bit indicates error */
	frame.f_format = FMT0;

	userret(p, &frame, 0, (u_int)0, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}
