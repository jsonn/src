/*	$NetBSD: trap.c,v 1.5.2.1 1998/07/30 14:03:57 eeh Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by Harvard University.
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
 *	@(#)trap.c	8.4 (Berkeley) 9/23/93
 */

#include "opt_ddb.h"
#include "opt_ktrace.h"
#include "opt_uvm.h"
#include "opt_compat_svr4.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/syslog.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <sparc64/sparc64/asm.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/trap.h>
#include <machine/instr.h>
#include <machine/pmap.h>

#ifdef DDB
#include <machine/db_machdep.h>
#else
#include <machine/frame.h>
#endif
#ifdef COMPAT_SVR4
#include <machine/svr4_machdep.h>
#endif

#include <sparc64/fpu/fpu_extern.h>
#include <sparc64/sparc64/memreg.h>
#include <sparc64/sparc64/cache.h>

#ifndef offsetof
#define	offsetof(s, f) ((int)&((s *)0)->f)
#endif

#if defined(UVM)
/* We can either do this or really ugly up the code. */
#define	vm_fault(m,v,t,w)	uvm_fault((m),(v),(w),(t))
#endif

#ifdef DEBUG
/* What trap level are we running? */
#define tl() ({ \
	int l; \
	__asm __volatile("rdpr %%tl, %0" : "=r" (l) :); \
	l; \
})
#endif

/* trapstats */
int trapstats = 0;
int protfix = 0;
int protmmu = 0;
int missmmu = 0;
int udmiss = 0;	/* Number of normal/nucleus data/text miss/protection faults */
int udhit = 0;	
int udprot = 0;
int utmiss = 0;
int kdmiss = 0;
int kdhit = 0;	
int kdprot = 0;
int ktmiss = 0;
int iveccnt = 0; /* number if normal/nucleus interrupt/interrupt vector faults */
int uintrcnt = 0;
int kiveccnt = 0;
int kintrcnt = 0;
int intristk = 0; /* interrupts when already on intrstack */
int wfill = 0;
int kwfill = 0;
int wspill = 0;
int wspillskip = 0;
int rftucnt = 0;
int rftuld = 0;
int rftudone = 0;
int rftkcnt[5] = { 0, 0, 0, 0, 0 };

extern int cold;

#ifdef DEBUG
#define RW_64		0x1
#define RW_ERR		0x2
#define RW_FOLLOW	0x4
int	rwindow_debug = RW_64|RW_ERR;
#define TDB_ADDFLT	0x1
#define TDB_TXTFLT	0x2
#define TDB_TRAP	0x4
#define TDB_SYSCALL	0x8
#define TDB_FOLLOW	0x10
#define TDB_FRAME	0x20
#define TDB_NSAVED	0x40
#define TDB_TL		0x80
#define TDB_STOPSIG	0x100
#define TDB_STOPCALL	0x200
#define TDB_STOPCPIO	0x400
#define TDB_SYSTOP	0x800
int	trapdebug = 0;
/* #define __inline */
#endif

#ifdef DDB
#if 1
#define DEBUGGER(t,f)	do { kdb_trap(t,f); } while (0)
#else
#define DEBUGGER(t,f)	Debugger()
#endif
#else
#define DEBUGGER(t,f)
#endif

/*
 * Initial FPU state is all registers == all 1s, everything else == all 0s.
 * This makes every floating point register a signalling NaN, with sign bit
 * set, no matter how it is interpreted.  Appendix N of the Sparc V8 document
 * seems to imply that we should do this, and it does make sense.
 */
struct	fpstate initfpstate = {
	{ ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0,
	  ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0, ~0 }
};

/*
 * There are more than 100 trap types, but most are unused.
 *
 * Trap type 0 is taken over as an `Asynchronous System Trap'.
 * This is left-over Vax emulation crap that should be fixed.
 *
 * Traps not supported on the spitfire are marked with `*',
 * and additions are marked with `+'
 */
static const char T[] = "*trap";
const char *trap_type[] = {
	/* non-user vectors */
	"ast",			/* 0 */
	"power on reset",	/* 1 */
	"watchdog reset",	/* 2 */
	"externally initiated reset",/*3 */
	"software initiated reset",/* 4 */
	"RED state exception",	/* 5 */
	T, T,			/* 6..7 */
	"instruction access exception",	/* 8 */
	"*instruction MMU miss",/* 9 */
	"instruction access error",/* 0a */
	T, T, T, T, T,		/* 0b..0f */
	"illegal instruction",	/* 10 */
	"privileged opcode",	/* 11 */
	"*unimplemented LDD",	/* 12 */
	"*unimplemented STD",	/* 13 */
	T, T, T, T,		/* 14..17 */
	T, T, T, T, T, T, T, T, /* 18..1f */
	"fp disabled",		/* 20 */
	"fp exception ieee 754",/* 21 */
	"fp exception other",	/* 22 */
	"tag overflow",		/* 23 */
	"clean window",		/* 24 */
	T, T, T,		/* 25..27 -- trap continues */
	"division by zero",	/* 28 */
	"*internal processor error",/* 29 */
	T, T, T, T, T, T,	/* 2a..2f */
	"data access exception",/* 30 */
	"*data access MMU miss",/* 31 */
	"data access error",	/* 32 */
	"*data access protection",/* 33 */
	"mem address not aligned",	/* 34 */
	"LDDF mem address not aligned",/* 35 */
	"STDF mem address not aligned",/* 36 */
	"privileged action",	/* 37 */
	"LDQF mem address not aligned",/* 38 */
	"STQF mem address not aligned",/* 39 */
	T, T, T, T, T, T,	/* 3a..3f */
	"*async data error",	/* 40 */
	"level 1 int",		/* 41 */
	"level 2 int",		/* 42 */
	"level 3 int",		/* 43 */
	"level 4 int",		/* 44 */
	"level 5 int",		/* 45 */
	"level 6 int",		/* 46 */
	"level 7 int",		/* 47 */
	"level 8 int",		/* 48 */
	"level 9 int",		/* 49 */
	"level 10 int",		/* 4a */
	"level 11 int",		/* 4b */
	"level 12 int",		/* 4c */
	"level 13 int",		/* 4d */
	"level 14 int",		/* 4e */
	"level 15 int",		/* 4f */
	T, T, T, T, T, T, T, T, /* 50..57 */
	T, T, T, T, T, T, T, T, /* 58..5f */
	"+interrupt vector",	/* 60 */
	"+PA_watchpoint",	/* 61 */
	"+VA_watchpoint",	/* 62 */
	"+corrected ECC error",	/* 63 */
	"+fast instruction access MMU miss",/* 64 */
	T, T, T,		/* 65..67 -- trap continues */
	"+fast data access MMU miss",/* 68 */
	T, T, T,		/* 69..6b -- trap continues */
	"+fast data access protection",/* 6c */
	T, T, T,		/* 6d..6f -- trap continues */
	T, T, T, T, T, T, T, T, /* 70..77 */
	T, T, T, T, T, T, T, T, /* 78..7f */
	"spill 0 normal",	/* 80 */
	T, T, T,		/* 81..83 -- trap continues */
	"spill 1 normal",	/* 84 */
	T, T, T,		/* 85..87 -- trap continues */
	"spill 2 normal",	/* 88 */
	T, T, T,		/* 89..8b -- trap continues */
	"spill 3 normal",	/* 8c */
	T, T, T,		/* 8d..8f -- trap continues */
	"spill 4 normal",	/* 90 */
	T, T, T,		/* 91..93 -- trap continues */
	"spill 5 normal",	/* 94 */
	T, T, T,		/* 95..97 -- trap continues */
	"spill 6 normal",	/* 98 */
	T, T, T,		/* 99..9b -- trap continues */
	"spill 7 normal",	/* 9c */
	T, T, T,		/* 9c..9f -- trap continues */
	"spill 0 other",	/* a0 */
	T, T, T,		/* a1..a3 -- trap continues */
	"spill 1 other",	/* a4 */
	T, T, T,		/* a5..a7 -- trap continues */
	"spill 2 other",	/* a8 */
	T, T, T,		/* a9..ab -- trap continues */
	"spill 3 other",	/* ac */
	T, T, T,		/* ad..af -- trap continues */
	"spill 4 other",	/* b0 */
	T, T, T,		/* b1..b3 -- trap continues */
	"spill 5 other",	/* b4 */
	T, T, T,		/* b5..b7 -- trap continues */
	"spill 6 other",	/* b8 */
	T, T, T,		/* b9..bb -- trap continues */
	"spill 7 other",	/* bc */
	T, T, T,		/* bc..bf -- trap continues */
	"fill 0 normal",	/* c0 */
	T, T, T,		/* c1..c3 -- trap continues */
	"fill 1 normal",	/* c4 */
	T, T, T,		/* c5..c7 -- trap continues */
	"fill 2 normal",	/* c8 */
	T, T, T,		/* c9..cb -- trap continues */
	"fill 3 normal",	/* cc */
	T, T, T,		/* cd..cf -- trap continues */
	"fill 4 normal",	/* d0 */
	T, T, T,		/* d1..d3 -- trap continues */
	"fill 5 normal",	/* d4 */
	T, T, T,		/* d5..d7 -- trap continues */
	"fill 6 normal",	/* d8 */
	T, T, T,		/* d9..db -- trap continues */
	"fill 7 normal",	/* dc */
	T, T, T,		/* dc..df -- trap continues */
	"fill 0 other",		/* e0 */
	T, T, T,		/* e1..e3 -- trap continues */
	"fill 1 other",		/* e4 */
	T, T, T,		/* e5..e7 -- trap continues */
	"fill 2 other",		/* e8 */
	T, T, T,		/* e9..eb -- trap continues */
	"fill 3 other",		/* ec */
	T, T, T,		/* ed..ef -- trap continues */
	"fill 4 other",		/* f0 */
	T, T, T,		/* f1..f3 -- trap continues */
	"fill 5 other",		/* f4 */
	T, T, T,		/* f5..f7 -- trap continues */
	"fill 6 other",		/* f8 */
	T, T, T,		/* f9..fb -- trap continues */
	"fill 7 other",		/* fc */
	T, T, T,		/* fc..ff -- trap continues */

	/* user (software trap) vectors */
	"syscall",		/* 100 */
	"breakpoint",		/* 101 */
	"zero divide",		/* 102 */
	"flush windows",	/* 103 */
	"clean windows",	/* 104 */
	"range check",		/* 105 */
	"fix align",		/* 106 */
	"integer overflow",	/* 107 */
	"svr4 syscall",		/* 108 */
	"4.4 syscall",		/* 109 */
	"kgdb exec",		/* 10a */
	T, T, T, T, T,		/* 10b..10f */
	T, T, T, T, T, T, T, T,	/* 11a..117 */
	T, T, T, T, T, T, T, T,	/* 118..11f */
	"svr4 getcc",		/* 120 */
	"svr4 setcc",		/* 121 */
	"svr4 getpsr",		/* 122 */
	"svr4 setpsr",		/* 123 */
	"svr4 gethrtime",	/* 124 */
	"svr4 gethrvtime",	/* 125 */
	T,			/* 126 */
	"svr4 gethrestime",	/* 127 */
};

#define	N_TRAP_TYPES	(sizeof trap_type / sizeof *trap_type)

static __inline void userret __P((struct proc *, int,  u_quad_t));
void trap __P((unsigned, int, int, struct trapframe *));
static __inline void share_fpu __P((struct proc *, struct trapframe *));
static int fixalign __P((struct proc *, struct trapframe *));
void mem_access_fault __P((unsigned, int, u_int, int, int, struct trapframe *));
void data_access_fault __P((unsigned type, u_int va, u_int pc, struct trapframe *));
void data_access_error __P((unsigned, u_int, u_int, u_int, u_int, struct trapframe *));
void text_access_fault __P((unsigned, u_int, struct trapframe *));
void text_access_error __P((unsigned, u_int, u_int, u_int, u_int, struct trapframe *));
void syscall __P((register_t, struct trapframe *, register_t));

#ifdef DEBUG
void print_trapframe __P((struct trapframe *));
void
print_trapframe(tf)
	struct trapframe *tf;
{

	printf("Trapframe %p:\ttstate: %x\tpc: %x\tnpc: %x\n",
	       tf, (long)tf->tf_tstate, (long)tf->tf_pc, (long)tf->tf_npc);
	printf("fault: %p\tkstack: %p\ty: %x\t", 
	       (long)tf->tf_fault, (long)tf->tf_kstack, (int)tf->tf_y);
	printf("pil: %d\toldpil: %d\ttt: %x\tGlobals:\n", 
	       (int)tf->tf_pil, (int)tf->tf_oldpil, (int)tf->tf_tt);
	printf("%08x%08x %08x%08x %08x%08x %08x%08x\n",
	       (long)(tf->tf_global[0]>>32), (long)tf->tf_global[0],
	       (long)(tf->tf_global[1]>>32), (long)tf->tf_global[1],
	       (long)(tf->tf_global[2]>>32), (long)tf->tf_global[2],
	       (long)(tf->tf_global[3]>>32), (long)tf->tf_global[3]);
	printf("%08x%08x %08x%08x %08x%08x %08x%08x\nouts:\n",
	       (long)(tf->tf_global[4]>>32), (long)tf->tf_global[4],
	       (long)(tf->tf_global[5]>>32), (long)tf->tf_global[5],
	       (long)(tf->tf_global[6]>>32), (long)tf->tf_global[6],
	       (long)(tf->tf_global[7]>>32), (long)tf->tf_global[7]);
	printf("%08x%08x %08x%08x %08x%08x %08x%08x\n",
	       (long)(tf->tf_out[0]>>32), (long)tf->tf_out[0],
	       (long)(tf->tf_out[1]>>32), (long)tf->tf_out[1],
	       (long)(tf->tf_out[2]>>32), (long)tf->tf_out[2],
	       (long)(tf->tf_out[3]>>32), (long)tf->tf_out[3]);
	printf("%08x%08x %08x%08x %08x%08x %08x%08x\n",
	       (long)(tf->tf_out[4]>>32), (long)tf->tf_out[4],
	       (long)(tf->tf_out[5]>>32), (long)tf->tf_out[5],
	       (long)(tf->tf_out[6]>>32), (long)tf->tf_out[6],
	       (long)(tf->tf_out[7]>>32), (long)tf->tf_out[7]);

}
#endif

/*
 * Define the code needed before returning to user mode, for
 * trap, mem_access_fault, and syscall.
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
	if (want_ast) {
		want_ast = 0;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
	}
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		(void) splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		(void) spl0();
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL)
		addupc_task(p, pc, (int)(p->p_sticks - oticks));

	curpriority = p->p_priority;
}

/*
 * If someone stole the FPU while we were away, do not enable it
 * on return.  This is not done in userret() above as it must follow
 * the ktrsysret() in syscall().  Actually, it is likely that the
 * ktrsysret should occur before the call to userret.
 */
static __inline void share_fpu(p, tf)
	struct proc *p;
	struct trapframe *tf;
{
	if ((tf->tf_tstate & (PSTATE_PEF<<TSTATE_PSTATE_SHIFT)) != 0 && fpproc != p)
		tf->tf_tstate &= ~(PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
}

/*
 * Called from locore.s trap handling, for non-MMU-related traps.
 * (MMU-related traps go through mem_access_fault, below.)
 */
void
trap(type, tstate, pc, tf)
	register unsigned type;
	register int tstate, pc;
	register struct trapframe *tf;
{
	register struct proc *p;
	register struct pcb *pcb;
	register int pstate = (tstate>>TSTATE_PSTATE_SHIFT);
	register int64_t n;
	u_quad_t sticks;

	/* This steps the PC over the trap. */
#define	ADVANCE (n = tf->tf_npc, tf->tf_pc = n, tf->tf_npc = n + 4)

#ifdef DEBUG
	{
		/* Check to make sure we're on the normal stack */
		int* sp;

		__asm("mov %%sp, %0" : "=r" (sp) :);
		if (sp < eintstack) {
			printf("trap: We're on the interrupt stack!\ntype=0x%x tf=%p %s\n", 
			       type, tf, type < N_TRAP_TYPES ? trap_type[type] : 
			       ((type == T_AST) ? "ast" : 
				((type == T_RWRET) ? "rwret" : T)));
		}
	}
#endif


#ifdef DEBUG
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_FOLLOW|TDB_TRAP)) {
		printf("trap: type 0x%x: pc=%x &tf=%x",
		       type, pc, tf); 
		printf(" npc=%x pstate=%b %s\n",
		       (long)tf->tf_npc, pstate, PSTATE_BITS, 
		       type < N_TRAP_TYPES ? trap_type[type] : 
		       ((type == T_AST) ? "ast" : 
			((type == T_RWRET) ? "rwret" : T)));
	}
#if 0
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
#endif

#if defined(UVM)
	uvmexp.traps++;
#else
	cnt.v_trap++;
#endif
#ifdef DEBUG
	if ((trapdebug & TDB_TL) && tl()) {
		extern int trap_trace_dis;
		trap_trace_dis = 1;
		printf("trap: type 0x%x: lvl=%d pc=%x &tf=%x",
		       type, tl(), pc, tf); 
		printf(" npc=%x pstate=%b %s\n",
		       (long)tf->tf_npc, pstate, PSTATE_BITS, 
		       type < N_TRAP_TYPES ? trap_type[type] : 
		       ((type == T_AST) ? "ast" : 
			((type == T_RWRET) ? "rwret" : T)));
		kdb_trap(type, tf);
	}
#endif
	/*
	 * Generally, kernel traps cause a panic.  Any exceptions are
	 * handled early here.
	 */
	if (pstate & PSTATE_PRIV) {
#ifdef DDB
		if (type == T_BREAKPOINT) {
			write_all_windows();
			if (kdb_trap(type, tf)) {
				ADVANCE;
				return;
			}
		}
#endif
#ifdef DIAGNOSTIC
		/*
		 * Currently, we allow DIAGNOSTIC kernel code to
		 * flush the windows to record stack traces.
		 */
		if (type == T_FLUSHWIN) {
			write_all_windows();
			ADVANCE;
			return;
		}
#endif
		/*
		 * Storing %fsr in cpu_attach will cause this trap
		 * even though the fpu has been enabled, if and only
		 * if there is no FPU.
		 */
		if (type == T_FPDISABLED && cold) {
			ADVANCE;
			return;
		}
		goto dopanic;
	}
	if ((p = curproc) == NULL)
		p = &proc0;
	sticks = p->p_sticks;
	pcb = &p->p_addr->u_pcb;
	p->p_md.md_tf = tf;	/* for ptrace/signals */

	switch (type) {

	default:
		if (type < 0x100) {
dopanic:
			printf("trap type 0x%x: pc=%x",
			       type, pc); 
			printf(" npc=%x pstate=%b\n",
			       (long)tf->tf_npc, pstate, PSTATE_BITS);
			DEBUGGER(type, tf);
			panic(type < N_TRAP_TYPES ? trap_type[type] : T);
			/* NOTREACHED */
		}
#if defined(COMPAT_SVR4) || defined(SUN4M)
badtrap:
#endif
		/* the following message is gratuitous */
		/* ... but leave it in until we find anything */
		printf("%s[%d]: unimplemented software trap 0x%x\n",
		    p->p_comm, p->p_pid, type);
		trapsignal(p, SIGILL, type);
		break;

#ifdef COMPAT_SVR4
	case T_SVR4_GETCC:
	case T_SVR4_SETCC:
	case T_SVR4_GETPSR:
	case T_SVR4_SETPSR:
	case T_SVR4_GETHRTIME:
	case T_SVR4_GETHRVTIME:
	case T_SVR4_GETHRESTIME:
		if (!svr4_trap(type, p))
			goto badtrap;
		break;
#endif

	case T_AST:
		break;	/* the work is all in userret() */

	case T_ILLINST:
	case T_INST_EXCEPT:
	case T_TEXTFAULT:
		printf("trap: textfault at %p!! sending SIGILL due to trap %d: %s\n", 
		       pc, type, type < N_TRAP_TYPES ? trap_type[type] : T);
		Debugger();
		trapsignal(p, SIGILL, 0);	/* XXX code?? */
		break;

	case T_PRIVINST:
		printf("trap: privinst!! sending SIGILL due to trap %d: %s\n", 
		       type, type < N_TRAP_TYPES ? trap_type[type] : T);
		Debugger();
		trapsignal(p, SIGILL, 0);	/* XXX code?? */
		break;

	case T_FPDISABLED: {
		register struct fpstate *fs = p->p_md.md_fpstate;

		if (fs == NULL) {
			/* NOTE: fpstate must be 64-bit aligned */
			fs = malloc((sizeof *fs), M_SUBPROC, M_WAITOK);
			*fs = initfpstate;
			p->p_md.md_fpstate = fs;
		}
		/*
		 * If we have not found an FPU, we have to emulate it.
		 */
		if (!foundfpu) {
#ifdef notyet
			fpu_emulate(p, tf, fs);
			break;
#else
			trapsignal(p, SIGFPE, 0);	/* XXX code?? */
			break;
#endif
		}
		/*
		 * We may have more FPEs stored up and/or ops queued.
		 * If they exist, handle them and get out.  Otherwise,
		 * resolve the FPU state, turn it on, and try again.
		 */
		if (fs->fs_qsize) {
			fpu_cleanup(p, fs);
			break;
		}
		if (fpproc != p) {		/* we do not have it */
			if (fpproc != NULL)	/* someone else had it */
				savefpstate(fpproc->p_md.md_fpstate);
			loadfpstate(fs);
			fpproc = p;		/* now we do have it */
		}
#ifdef NOT_DEBUG
		printf("Enabling floating point in tstate\n");
#endif
		tf->tf_tstate |= (PSTATE_PEF<<TSTATE_PSTATE_SHIFT);
		break;
	}

#define read_rw(src, dst) \
	((src&1)?copyin((caddr_t)(src), (caddr_t)(dst), sizeof(struct rwindow32))\
	 :copyin((caddr_t)(src+BIAS), (caddr_t)(dst), sizeof(struct rwindow64)))

	case T_RWRET:
		/*
		 * T_RWRET is a window load needed in order to rett.
		 * It simply needs the window to which tf->tf_out[6]
		 * (%sp) points.  There are no user or saved windows now.
		 * Copy the one from %sp into pcb->pcb_rw[0] and set
		 * nsaved to -1.  If we decide to deliver a signal on
		 * our way out, we will clear nsaved.
		 */
		if (pcb->pcb_nsaved)
			panic("trap T_RWRET 1");
#ifdef DEBUG
		if (rwindow_debug&RW_FOLLOW)
			printf("%s[%d]: rwindow: pcb<-stack: %x\n",
				p->p_comm, p->p_pid, tf->tf_out[6]);
		printf("trap: T_RWRET sent!?!\n");
#endif
		if (read_rw(tf->tf_out[6], &pcb->pcb_rw[0])) 
			sigexit(p, SIGILL);
		if (pcb->pcb_nsaved)
			panic("trap T_RWRET 2");
		pcb->pcb_nsaved = -1;		/* mark success */
		break;

	case T_ALIGN:
	case T_LDDF_ALIGN:
	case T_STDF_ALIGN:
	{
		int64_t dsfsr, dsfar=0, isfsr;

		dsfsr = ldxa(SFSR, ASI_DMMU);
		if (dsfsr & SFSR_FV)
			dsfar = ldxa(SFAR, ASI_DMMU);
		isfsr = ldxa(SFSR, ASI_IMMU);
		/* 
		 * If we're busy doing copyin/copyout continue
		 */
		if (p->p_addr && p->p_addr->u_pcb.pcb_onfault) {
			tf->tf_pc = (vaddr_t)p->p_addr->u_pcb.pcb_onfault;
			tf->tf_npc = tf->tf_pc + 4;
			break;
		}
		
#define fmt64(x)	(int)((x)>>32), (int)((x))
		printf("Alignment error: dsfsr=%08x:%08x dsfar=%x:%x isfsr=%08x:%08x pc=%p\n",
		       fmt64(dsfsr), fmt64(dsfar), fmt64(isfsr), pc);
	}
		
#ifdef DDB
		write_all_windows();
		kdb_trap(type, tf);
#endif
		if ((p->p_md.md_flags & MDP_FIXALIGN) != 0 && 
		    fixalign(p, tf) == 0) {
			ADVANCE;
			break;
		}
		trapsignal(p, SIGBUS, 0);	/* XXX code?? */
		break;

	case T_FP_IEEE_754:
	case T_FP_OTHER:
		/*
		 * Clean up after a floating point exception.
		 * fpu_cleanup can (and usually does) modify the
		 * state we save here, so we must `give up' the FPU
		 * chip context.  (The software and hardware states
		 * will not match once fpu_cleanup does its job, so
		 * we must not save again later.)
		 */
		if (p != fpproc)
			panic("fpe without being the FP user");
		savefpstate(p->p_md.md_fpstate);
		fpproc = NULL;
		/* tf->tf_psr &= ~PSR_EF; */	/* share_fpu will do this */
		if (p->p_md.md_fpstate->fs_qsize == 0) {
			p->p_md.md_fpstate->fs_queue[0].fq_instr = fuword((caddr_t)pc);
			p->p_md.md_fpstate->fs_qsize = 1;
			fpu_cleanup(p, p->p_md.md_fpstate);
			ADVANCE;
		} else
			fpu_cleanup(p, p->p_md.md_fpstate);
		/* fpu_cleanup posts signals if needed */
#if 0		/* ??? really never??? */
		ADVANCE;
#endif
		break;

	case T_TAGOF:
		trapsignal(p, SIGEMT, 0);	/* XXX code?? */
		break;

	case T_BREAKPOINT:
		trapsignal(p, SIGTRAP, 0);
		break;

	case T_DIV0:
		ADVANCE;
		trapsignal(p, SIGFPE, FPE_INTDIV_TRAP);
		break;

	case T_CLEANWIN:
		uprintf("T_CLEANWIN\n");	/* XXX Should not get this */
		ADVANCE;
		break;

	case T_RANGECHECK:
		printf("T_RANGECHECK\n");	/* XXX */
		ADVANCE;
		trapsignal(p, SIGILL, 0);	/* XXX code?? */
		break;

	case T_FIXALIGN:
#ifdef DEBUG_ALIGN
		uprintf("T_FIXALIGN\n");
#endif
		/* User wants us to fix alignment faults */
		p->p_md.md_flags |= MDP_FIXALIGN;
		ADVANCE;
		break;

	case T_INTOF:
		uprintf("T_INTOF\n");		/* XXX */
		ADVANCE;
		trapsignal(p, SIGFPE, FPE_INTOVF_TRAP);
		break;
	}
	userret(p, pc, sticks);
	share_fpu(p, tf);
#undef ADVANCE
#ifdef DEBUG
	if (trapdebug&(TDB_FOLLOW|TDB_TRAP)) {
		printf("trap: done\n");
		/* if (type != T_BREAKPOINT) Debugger(); */
	}
#if 0
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
#endif
}

/*
 * Save windows from PCB into user stack, and return 0.  This is used on
 * window overflow pseudo-traps (from locore.s, just before returning to
 * user mode) and when ptrace or sendsig needs a consistent state.
 * As a side effect, rwindow_save() always sets pcb_nsaved to 0.
 *
 * If the windows cannot be saved, pcb_nsaved is restored and we return -1.
 * 
 * XXXXXX This cannot work properly.  I need to re-examine this register
 * window thing entirely.  
 */
int
rwindow_save(p)
	register struct proc *p;
{
	register struct pcb *pcb = &p->p_addr->u_pcb;
	register struct rwindow64 *rw = &pcb->pcb_rw[0];
	register u_int64_t rwdest;
	register int i, j;
#ifndef TRAPWIN
	register struct trapframe *tf = p->p_md.md_tf;
#endif

	/* Make sure our D$ is not polluted w/bad data */
	blast_vcache();

	i = pcb->pcb_nsaved;
#ifdef DEBUG
	if (rwindow_debug&RW_FOLLOW)
		printf("rwindow_save(%p): nsaved %d\n", p, i);
#endif
	if (i == 0)
		return (0);
#ifdef DEBUG
	if (rwindow_debug&RW_FOLLOW)
		printf("%s[%d]: rwindow: pcb->stack:", p->p_comm, p->p_pid);
#endif
	 while (i > 0) {
		rwdest = rw[i--].rw_in[6];
#ifdef DEBUG
		if (rwindow_debug&RW_FOLLOW)
			printf("window %d at %x:%x\n", i, rwdest);
#endif
		if (rwdest & 1) {
#ifndef TRAPWIN
			struct rwindow64 *rwstack;
			/* 64-bit window */
#endif
#ifdef DEBUG
			if (rwindow_debug&RW_64) {
				printf("rwindow_save: 64-bit tf to %p-BIAS or %p\n", 
				       rwdest, rwdest-BIAS);
				Debugger();
			}
#endif
			rwdest -= BIAS;
			if (copyout((caddr_t)rw, (caddr_t)rwdest,
				    sizeof(*rw))) {
#ifdef DEBUG
			if (rwindow_debug&(RW_ERR|RW_64))
				printf("rwindow_save: 64-bit pcb copyout to %p failed\n", rwdest);
#endif
				return (-1);
			}
#ifndef TRAPWIN
			rwstack = (struct rwindow64 *)rwdest;
			for (j=0; j<8; j++) { 
				if (copyout((void *)(&rwstack->rw_local[j]), &tf->tf_local[j], 
					    sizeof (tf->tf_local[j]))) {
#ifdef DEBUG
					if (rwindow_debug&(RW_64|RW_ERR))
						printf("rwindow_save: 64-bit tf suword to %p failed\n", 
						       &rwstack->rw_local[j]);
#endif
					return (-1);
				}
			}
#endif
		} else {
			struct rwindow32 *rwstack;

			/* 32-bit window */
			rwstack = (struct rwindow32 *)rwdest;
			for (j=0; j<8; j++) { 
#ifdef DEBUG
				if (rwindow_debug&RW_FOLLOW)
					printf("%%l%d[%x]->%p\t", j, (int)rw[i].rw_local[j], &rwstack->rw_local[j]);
#endif
				if(suword((void *)(&rwstack->rw_local[j]), (int)rw[i].rw_local[j])) {
#ifdef DEBUG
					if (rwindow_debug&RW_ERR)
						printf("rwindow_save: 32-bit pcb suword of %%l%d to %p failed\n", j, &rwstack->rw_local[j]);
#endif
					return (-1);
				}
#ifdef DEBUG
				if (rwindow_debug&RW_FOLLOW)
					printf("%%i%d[%x]->%p\n", j, (int)rw[i].rw_in[j],  &rwstack->rw_in[j]);
#endif
				if(suword((void *)(&rwstack->rw_in[j]), (int)rw[i].rw_in[j])) {
#ifdef DEBUG
					if (rwindow_debug&RW_ERR)
						printf("rwindow_save: 32-bit pcb suword of %%i%d to %p failed\n", j, &rwstack->rw_in[j]);
#endif
					return (-1);
				}
			}
		}
/*		rw++; */
	}
	pcb->pcb_nsaved = 0;
#ifdef DEBUG
	if (rwindow_debug&RW_FOLLOW) {
		printf("\n");
		Debugger();
	}
#endif
	return (0);
}

/*
 * Kill user windows (before exec) by writing back to stack or pcb
 * and then erasing any pcb tracks.  Otherwise we might try to write
 * the registers into the new process after the exec.
 */
void
kill_user_windows(p)
	struct proc *p;
{

	write_user_windows();
	p->p_addr->u_pcb.pcb_nsaved = 0;
}

#ifdef DEBUG
int dfdebug = 0;
#endif
extern struct proc *masterpaddr;

void
data_access_fault(type, addr, pc, tf)
	register unsigned type;
	register u_int addr;
	register u_int pc;
	register struct trapframe *tf;
{
	register u_int64_t tstate;
	register struct proc *p;
	register struct vmspace *vm;
	register vaddr_t va;
	register int rv;
	vm_prot_t ftype;
	int onfault;
	u_quad_t sticks;
#if DEBUG
	static int lastdouble;
	extern struct pcb* cpcb;
#endif

#ifdef DEBUG
	if (protmmu || missmmu) {
		extern int trap_trace_dis;
		trap_trace_dis = 1;
		printf("%d: data_access_fault(%x, %x, %x, %x) %s=%d\n",
		       curproc?curproc->p_pid:-1, type, addr, pc, tf, 
		       (protmmu)?"protmmu":"missmmu", (protmmu)?protmmu:missmmu);
		Debugger();
	}
	write_user_windows();
/*	if (cpcb->pcb_nsaved > 6) trapdebug |= TDB_NSAVED; */
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_ADDFLT|TDB_FOLLOW)) {
		printf("%d: data_access_fault(%x, %x, %x, %x) nsaved=%d\n",
		       curproc?curproc->p_pid:-1, type, addr, pc, tf, 
		       cpcb->pcb_nsaved);
		if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved)) Debugger();
	}
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if ((trapdebug & TDB_TL) && tl()) {
		printf("%d: tl %d data_access_fault(%x, %x, %x, %x) nsaved=%d\n",
		       curproc?curproc->p_pid:-1, tl(), type, addr, pc, tf, 
		       cpcb->pcb_nsaved);
		Debugger();
	}
	if (trapdebug&TDB_STOPCALL) { 
		Debugger();
	}
#endif

#if defined(UVM)
	uvmexp.traps++;
#else
	cnt.v_trap++;
#endif
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

#if 0
	/* This can happen when we're in DDB w/curproc == NULL and try
	 * to access user space.
	 */
#ifdef DIAGNOSTIC
	if ((addr & PAGE_MASK) && 
	    (addr & PAGE_MASK) != p->p_vmspace->vm_map.pmap->pm_ctx) {
		printf("data_access_fault: va ctx %x != pm ctx %x\n",
		       (addr & PAGE_MASK), p->p_vmspace->vm_map.pmap->pm_ctx);
		Debugger();
	}
#endif
#endif
	tstate = tf->tf_tstate;

	/* Find the faulting va to give to vm_fault */
	va = trunc_page(addr);

#ifdef DEBUG
	if (lastdouble) {
		printf("stacked data fault @ %x (pc %x);", addr, pc);
		lastdouble = 0;
		if (curproc == NULL)
			printf("NULL proc\n");
		else
			printf("pid %d(%s); sigmask %x, sigcatch %x\n",
				curproc->p_pid, curproc->p_comm,
				curproc->p_sigmask, curproc->p_sigcatch);
	}
#endif
	/* Now munch on protections... */

	ftype = (type == T_FDMMU_PROT)? VM_PROT_READ|VM_PROT_WRITE:VM_PROT_READ;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		extern char Lfsbail[];
		/*
		 * If this was an access that we shouldn't try to page in,
		 * resume at the fault handler without any action.
		 */
		if (p->p_addr && p->p_addr->u_pcb.pcb_onfault == Lfsbail)
			goto kfault;

		/*
		 * During autoconfiguration, faults are never OK unless
		 * pcb_onfault is set.  Once running normally we must allow
		 * exec() to cause copy-on-write faults to kernel addresses.
		 */
		if (cold)
			goto kfault;
		if (!(addr&TLB_TAG_ACCESS_CTX)) {
			/* CTXT == NUCLEUS */
			if ((rv=vm_fault(kernel_map, va, ftype, 0)) == KERN_SUCCESS) {
#ifdef DEBUG
				if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
					printf("data_access_fault: kernel vm_fault(%x, %x, %x, 0) sez %x -- success\n",
					       kernel_map, (vaddr_t)va, ftype, rv);
#endif
				return;
			}
			if ((rv=vm_fault(kernel_map, va, ftype, 0)) == KERN_SUCCESS) {
#ifdef DEBUG
				if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
					printf("data_access_fault: kernel vm_fault(%x, %x, %x, 0) sez %x -- success\n",
					       kernel_map, (vaddr_t)va, ftype, rv);
#endif
				return;
			}
#ifdef DEBUG
			if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
				printf("data_access_fault: kernel vm_fault(%x, %x, %x, 0) sez %x -- failure\n",
				       kernel_map, (vaddr_t)va, ftype, rv);
#endif
			goto kfault;
		}
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	rv = vm_fault(&vm->vm_map, (vaddr_t)va, ftype, FALSE);

#ifdef DEBUG
	if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
		printf("data_access_fault: user vm_fault(%x, %x, %x, FALSE) sez %x\n",
		       &vm->vm_map, (vaddr_t)va, ftype, rv);
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
			unsigned nss = clrnd(btoc(USRSTACK - va));
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == KERN_PROTECTION_FAILURE)
			rv = KERN_INVALID_ADDRESS;
	}
	if (rv != KERN_SUCCESS) {
		/*
		 * Pagein failed.  If doing copyin/out, return to onfault
		 * address.  Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
kfault:
			onfault = p->p_addr ?
			    (int)p->p_addr->u_pcb.pcb_onfault : 0;
			if (!onfault) {
				(void) splhigh();
				printf("data fault: pc=%x addr=%x\n",
				    pc, addr);
				DEBUGGER(type, tf);
				panic("kernel fault");
				/* NOTREACHED */
			}
#ifdef DEBUG
			if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW|TDB_STOPCPIO)) {
				printf("data_access_fault: copyin/out fault -- recover\n");
				Debugger();
			}
#endif
			tf->tf_pc = onfault;
			tf->tf_npc = onfault + 4;
			return;
		}
#ifdef DEBUG
		if (trapdebug&(TDB_ADDFLT|TDB_STOPSIG)) {
			extern int trap_trace_dis;
			trap_trace_dis = 1;
			printf("data_access_fault at addr %p: sending SIGSEGV\n", addr);
			Debugger();
		}
#endif
		trapsignal(p, SIGSEGV, (u_int)addr);
	}
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
#ifdef DEBUG
	if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
		printf("data_access_fault: done\n");
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW)) {
		extern void* return_from_trap __P((void));
		if ((void*)tf->tf_pc == (void*)return_from_trap) {
			printf("Returning from stack datafault\n");
		}
	}
#endif
}

void
data_access_error(type, sfva, sfsr, afva, afsr, tf)
	register unsigned type;
	register u_int sfva;
	register u_int sfsr;
	register u_int afva;
	register u_int afsr;
	register struct trapframe *tf;
{
	register int pc;
	register u_int64_t tstate;
	register struct proc *p;
	register struct vmspace *vm;
	register vaddr_t va;
	register int rv;
	vm_prot_t ftype;
	int onfault;
	u_quad_t sticks;
#ifdef DEBUG
	static int lastdouble;
#endif

#if DEBUG
	if (protmmu || missmmu) {
		extern int trap_trace_dis;
		trap_trace_dis = 1;
		printf("%d: data_access_error(%x, %x, %x, %x) %s=%d\n",
		       curproc?curproc->p_pid:-1, type, sfva, afva, tf, 
		       (protmmu)?"protmmu":"missmmu", (protmmu)?protmmu:missmmu);
		Debugger();
	}
	write_user_windows();
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
		printf("%d data_access_error(%x, %x, %x, %x)=%x:%x @ %x %b\n",
		       curproc?curproc->p_pid:-1, 
		       type, sfva, afva, tf, (long)(tf->tf_tstate>>32), 
		       (long)tf->tf_tstate, (long)tf->tf_pc, sfsr, SFSR_BITS); 
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if ((trapdebug & TDB_TL) && tl()) {
		printf("%d tl %d data_access_error(%x, %x, %x, %x)=%x:%x @ %x %b\n",
		       curproc?curproc->p_pid:-1, tl(),
		       type, sfva, afva, tf, (long)(tf->tf_tstate>>32), 
		       (long)tf->tf_tstate, (long)tf->tf_pc, sfsr, SFSR_BITS); 
		Debugger();
	}
	if (trapdebug&TDB_STOPCALL) { 
		Debugger();
	}
#endif

#if defined(UVM)
	uvmexp.traps++;
#else
	cnt.v_trap++;
#endif
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	pc = tf->tf_pc;
	tstate = tf->tf_tstate;

	/*
	 * Our first priority is handling serious faults, such as
	 * parity errors or async faults that might have come through here.
	 * If we have a data fault, but SFSR_FAV is not set in the sfsr,
	 * then things are really bizarre, and we treat it as a hard
	 * error and pass it on to memerr4m. 
	 */
	if ((afsr) != 0 ||
	    (type == T_DATAFAULT && !(sfsr & SFSR_FV))) {
#ifdef not4u
		memerr4m(type, sfsr, sfva, afsr, afva, tf);
		/*
		 * If we get here, exit the trap handler and wait for the
		 * trap to reoccur
		 */
		goto out;
#else
		printf("data memory error type %x sfsr=%p sfva=%p afsr=%p afva=%p tf=%p\n",
		       type, sfsr, sfva, afsr, afva, tf);
		DEBUGGER(type, tf);
		panic("trap: memory error");
#endif
	}

	/*
	 * Figure out what to pass the VM code. We cannot ignore the sfva
	 * register on text faults, since this might be a trap on an
	 * alternate-ASI access to code space. However, we can't help using 
	 * have a DMMU sfar.
	 * Kernel faults are somewhat different: text faults are always
	 * illegal, and data faults are extra complex.  User faults must
	 * set p->p_md.md_tf, in case we decide to deliver a signal.  Check
	 * for illegal virtual addresses early since those can induce more
	 * faults.
	 * All translation faults are illegal, and result in a SIGSEGV
	 * being delivered to the running process (or a kernel panic, for
	 * a kernel fault). We check the translation first to make sure
	 * it is not spurious.
	 * Also, note that in the case where we have an overwritten
	 * text fault (OW==1, AT==2,3), we attempt to service the
	 * second (overwriting) fault, then restart the instruction
	 * (which is from the first fault) and allow the first trap
	 * to reappear. XXX is this right? It will probably change...
	 */
	if ((sfsr & SFSR_FV) == 0 || (sfsr & SFSR_FT) == 0) {
		printf("data_access_error: no fault\n");
		goto out;	/* No fault. Why were we called? */
	}

	/*
	 * This next section is a mess since some chips use sfva, and others
	 * don't on text faults. We want to use sfva where possible, since
	 * we _could_ be dealing with an ASI 0x8,0x9 data access to text space,
	 * which would trap as a text fault, at least on a HyperSPARC. Ugh.
	 * XXX: Find out about MicroSPARCs.
	 */

	if (!(sfsr & SFSR_FV)) {
#ifdef DEBUG
		if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
			printf("data_access_error: got fault without valid SFVA\n");
#endif
		goto fault;
	}

	va = trunc_page(sfva);

#ifdef DEBUG
	if (lastdouble) {
		printf("stacked data error @ %x (pc %x); sfsr %x", sfva, pc, sfsr);
		lastdouble = 0;
		if (curproc == NULL)
			printf("NULL proc\n");
		else
			printf("pid %d(%s); sigmask %x, sigcatch %x\n",
				curproc->p_pid, curproc->p_comm,
				curproc->p_sigmask, curproc->p_sigcatch);
	}
#endif
	/* Now munch on protections... */

	ftype = sfsr & SFSR_W ? VM_PROT_READ|VM_PROT_WRITE:VM_PROT_READ;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		extern char Lfsbail[];
		/*
		 * If this was an access that we shouldn't try to page in,
		 * resume at the fault handler without any action.
		 */
		if (p->p_addr && p->p_addr->u_pcb.pcb_onfault == Lfsbail)
			goto kfault;

		/*
		 * During autoconfiguration, faults are never OK unless
		 * pcb_onfault is set.  Once running normally we must allow
		 * exec() to cause copy-on-write faults to kernel addresses.
		 */
		if (cold)
			goto kfault;
		if (SFSR_CTXT_IS_PRIM(sfsr) || SFSR_CTXT_IS_NUCLEUS(sfsr)) {
			/* NUCLEUS context */
			if (vm_fault(kernel_map, va, ftype, 0) == KERN_SUCCESS)
				return;
			if (SFSR_CTXT_IS_NUCLEUS(sfsr))
				goto kfault;
		}
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
#ifdef DEBUG
	if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
		printf("data_access_error: calling vm_fault\n");
#endif
	rv = vm_fault(&vm->vm_map, (vaddr_t)va, ftype, FALSE);

	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if vm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (rv == KERN_SUCCESS) {
			unsigned nss = clrnd(btoc(USRSTACK - va));
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == KERN_PROTECTION_FAILURE)
			rv = KERN_INVALID_ADDRESS;
	}
	if (rv != KERN_SUCCESS) {
		/*
		 * Pagein failed.  If doing copyin/out, return to onfault
		 * address.  Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
fault:
		if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
kfault:
			onfault = p->p_addr ?
			    (int)p->p_addr->u_pcb.pcb_onfault : 0;
			if (!onfault) {
				(void) splhigh();
				printf("data fault: pc=%x addr=%x sfsr=%b\n",
				    pc, sfva, sfsr, SFSR_BITS);
				DEBUGGER(type, tf);
				panic("kernel fault");
				/* NOTREACHED */
			}
#ifdef DEBUG
			if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
				printf("data_access_error: kern fault -- skipping instr\n");
#endif
			tf->tf_pc = onfault;
			tf->tf_npc = onfault + 4;
			return;
		}
#ifdef DEBUG
		if (trapdebug&(TDB_ADDFLT|TDB_STOPSIG)) {
			extern int trap_trace_dis;
			trap_trace_dis = 1;
			printf("data_access_error at %p: sending SIGSEGV\n", va);
			Debugger();
		}
#endif
		trapsignal(p, SIGSEGV, (u_int)sfva);
	}
out:
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
#ifdef DEBUG
	if (trapdebug&(TDB_ADDFLT|TDB_FOLLOW))
		printf("data_access_error: done\n");
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
}

void
text_access_fault(type, pc, tf)
	register unsigned type;
	register u_int pc;
	register struct trapframe *tf;
{
	register u_int64_t tstate;
	register struct proc *p;
	register struct vmspace *vm;
	register vaddr_t va;
	register int rv;
	vm_prot_t ftype;
	u_quad_t sticks;

#if DEBUG
	if (protmmu || missmmu) {
		extern int trap_trace_dis;
		trap_trace_dis = 1;
		printf("%d: text_access_fault(%x, %x, %x, %x) %s=%d\n",
		       curproc?curproc->p_pid:-1, type, pc, tf, 
		       (protmmu)?"protmmu":"missmmu", (protmmu)?protmmu:missmmu);
		Debugger();
	}
	write_user_windows();
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_TXTFLT|TDB_FOLLOW))
		printf("%d text_access_fault(%x, %x, %x)\n",
		       curproc?curproc->p_pid:-1, type, pc, tf); 
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if ((trapdebug & TDB_TL) && tl()) {
		printf("%d tl %d text_access_fault(%x, %x, %x)\n",
		       curproc?curproc->p_pid:-1, tl(), type, pc, tf); 
		Debugger();
	}
	if (trapdebug&TDB_STOPCALL) { 
		Debugger();
	}
#endif

#if defined(UVM)
	uvmexp.traps++;
#else
	cnt.v_trap++;
#endif
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	tstate = tf->tf_tstate;

	va = trunc_page(pc);

	/* Now munch on protections... */

	ftype = VM_PROT_READ;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		(void) splhigh();
		printf("text_access_fault: pc=%x\n", pc);
		DEBUGGER(type, tf);
		panic("kernel fault");
		/* NOTREACHED */
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	rv = vm_fault(&vm->vm_map, (vaddr_t)va, ftype, FALSE);

#ifdef DEBUG
	if (trapdebug&(TDB_TXTFLT|TDB_FOLLOW))
		printf("text_access_fault: vm_fault(%x, %x, %x, FALSE) sez %x\n",
		       &vm->vm_map, (vaddr_t)va, ftype, rv);
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
			unsigned nss = clrnd(btoc(USRSTACK - va));
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == KERN_PROTECTION_FAILURE)
			rv = KERN_INVALID_ADDRESS;
	}
	if (rv != KERN_SUCCESS) {
		/*
		 * Pagein failed. Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & TSTATE_PRIV) {
			(void) splhigh();
			printf("text fault: pc=%x\n",
			       pc);
			DEBUGGER(type, tf);
			panic("kernel fault");
			/* NOTREACHED */
		}
#ifdef DEBUG
		if (trapdebug&(TDB_TXTFLT|TDB_STOPSIG)) {
			extern int trap_trace_dis;
			trap_trace_dis = 1;
			printf("text_access_fault at %p: sending SIGSEGV\n", va);
			Debugger();
		}
#endif
		trapsignal(p, SIGSEGV, (u_int)pc);
	}
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
#ifdef DEBUG
	if (trapdebug&(TDB_TXTFLT|TDB_FOLLOW)) {
		printf("text_access_fault: done\n");
		/* kdb_trap(T_BREAKPOINT, tf); */
	}
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
}


void
text_access_error(type, pc, sfsr, afva, afsr, tf)
	register unsigned type;
	register u_int pc;
	register u_int sfsr;
	register u_int afva;
	register u_int afsr;
	register struct trapframe *tf;
{
	register int64_t tstate;
	register struct proc *p;
	register struct vmspace *vm;
	register vaddr_t va;
	register int rv;
	vm_prot_t ftype;
	u_quad_t sticks;
#if DEBUG
	static int lastdouble;
#endif
	
#if DEBUG
	if (protmmu || missmmu) {
		extern int trap_trace_dis;
		trap_trace_dis = 1;
		printf("%d: text_access_error(%x, %x, %x, %x) %s=%d\n",
		       curproc?curproc->p_pid:-1, type, sfsr, afsr, tf, 
		       (protmmu)?"protmmu":"missmmu", (protmmu)?protmmu:missmmu);
		Debugger();
	}
	write_user_windows();
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_TXTFLT|TDB_FOLLOW))
		printf("%d text_access_error(%x, %x, %x, %x)=%x:%x @ %x %b\n",
		       curproc?curproc->p_pid:-1, 
		       type, pc, afva, tf, (long)(tf->tf_tstate>>32), 
		       (long)tf->tf_tstate, (long)tf->tf_pc, sfsr, SFSR_BITS); 
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if ((trapdebug & TDB_TL) && tl()) {
		printf("%d tl %d text_access_error(%x, %x, %x, %x)=%x:%x @ %x %b\n",
		       curproc?curproc->p_pid:-1, tl(),
		       type, pc, afva, tf, (long)(tf->tf_tstate>>32), 
		       (long)tf->tf_tstate, (long)tf->tf_pc, sfsr, SFSR_BITS); 
		Debugger();
	}
	if (trapdebug&TDB_STOPCALL) { 
		Debugger();
	}
#endif
#if defined(UVM)
	uvmexp.traps++;
#else
	cnt.v_trap++;
#endif
	if ((p = curproc) == NULL)	/* safety check */
		p = &proc0;
	sticks = p->p_sticks;

	tstate = tf->tf_tstate;

	if ((afsr) != 0) {
#ifdef not4u
		/* Async text fault??? */
		memerr4m(type, sfsr, pc, afsr, afva, tf);
		/*
		 * If we get here, exit the trap handler and wait for the
		 * trap to reoccur
		 */
		goto out;
#else
		printf("text_access_error: memory error...");
		printf("text memory error type %d sfsr=%p sfva=%p afsr=%p afva=%p tf=%p\n",
		       type, sfsr, pc, afsr, afva, tf);
		DEBUGGER(type, tf);
		panic("text_access_error: memory error");
#endif
	}

	if ((sfsr & SFSR_FV) == 0 || (sfsr & SFSR_FT) == 0)
		goto out;	/* No fault. Why were we called? */

	va = trunc_page(pc);

#ifdef DEBUG
	if (lastdouble) {
		printf("stacked text error @ %x (pc %x); sfsr %x", pc, sfsr);
		lastdouble = 0;
		if (curproc == NULL)
			printf("NULL proc\n");
		else
			printf("pid %d(%s); sigmask %x, sigcatch %x\n",
				curproc->p_pid, curproc->p_comm,
				curproc->p_sigmask, curproc->p_sigcatch);
	}
#endif
	/* Now munch on protections... */

	ftype = VM_PROT_READ;
	if (tstate & (PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)) {
		(void) splhigh();
		printf("text error: pc=%x sfsr=%b\n", pc,
		       sfsr, SFSR_BITS, pc);
		DEBUGGER(type, tf);
		panic("kernel fault");
		/* NOTREACHED */
	} else
		p->p_md.md_tf = tf;

	vm = p->p_vmspace;
	/* alas! must call the horrible vm code */
	rv = vm_fault(&vm->vm_map, (vaddr_t)va, ftype, FALSE);

	/*
	 * If this was a stack access we keep track of the maximum
	 * accessed stack size.  Also, if vm_fault gets a protection
	 * failure it is due to accessing the stack region outside
	 * the current limit and we need to reflect that as an access
	 * error.
	 */
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (rv == KERN_SUCCESS) {
			unsigned nss = clrnd(btoc(USRSTACK - va));
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (rv == KERN_PROTECTION_FAILURE)
			rv = KERN_INVALID_ADDRESS;
	}
	if (rv != KERN_SUCCESS) {
		/*
		 * Pagein failed.  If doing copyin/out, return to onfault
		 * address.  Any other page fault in kernel, die; if user
		 * fault, deliver SIGSEGV.
		 */
		if (tstate & TSTATE_PRIV) {
			(void) splhigh();
			printf("text error: pc=%x sfsr=%b\n",
			       pc, sfsr, SFSR_BITS);
			DEBUGGER(type, tf);
			panic("kernel fault");
			/* NOTREACHED */
		}
#ifdef DEBUG
		if (trapdebug&(TDB_TXTFLT|TDB_STOPSIG)) {
			extern int trap_trace_dis;
			trap_trace_dis = 1;
			printf("text_access_error at %p: sending SIGSEGV\n", va);
			Debugger();
		}
#endif
		trapsignal(p, SIGSEGV, (u_int)pc);
	}
out:
	if ((tstate & TSTATE_PRIV) == 0) {
		userret(p, pc, sticks);
		share_fpu(p, tf);
	}
#ifdef DEBUG
	if (trapdebug&(TDB_TXTFLT|TDB_FOLLOW))
		printf("text_access_error: done\n");
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
}

/*
 * System calls.  `pc' is just a copy of tf->tf_pc.
 *
 * Note that the things labelled `out' registers in the trapframe were the
 * `in' registers within the syscall trap code (because of the automatic
 * `save' effect of each trap).  They are, however, the %o registers of the
 * thing that made the system call, and are named that way here.
 */
void
syscall(code, tf, pc)
	register_t code;
	register struct trapframe *tf;
	register_t pc;
{
	register int i, nsys, nap;
	register int64_t *ap;
	register struct sysent *callp;
	register struct proc *p;
	int error, new;
	struct args {
		register_t i[8];
	} args;
	register_t rval[2], *argp;
	u_quad_t sticks;
#ifdef DIAGNOSTIC
	extern struct pcb *cpcb;
#endif

#ifdef DEBUG
	write_user_windows();
	if ((trapdebug&TDB_NSAVED && cpcb->pcb_nsaved) || trapdebug&(TDB_SYSCALL|TDB_FOLLOW))
		printf("%d syscall(%x, %x, %x)\n",
		       curproc?curproc->p_pid:-1, code, tf, pc); 
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
	if (trapdebug & TDB_STOPCALL)
		Debugger();
	if ((trapdebug & TDB_TL) && tl()) {
		printf("%d tl %d syscall(%x, %x, %x)\n",
		       curproc?curproc->p_pid:-1, tl(), code, tf, pc); 
		Debugger();
	}
#endif

#if defined(UVM)
	uvmexp.syscalls++;
#else
	cnt.v_syscall++;
#endif
	p = curproc;
#ifdef DIAGNOSTIC
	if (tf->tf_tstate & TSTATE_PRIV)
		panic("syscall from kernel");
	if (cpcb != &p->p_addr->u_pcb)
		panic("syscall: cpcb/ppcb mismatch");
	if (tf != (struct trapframe *)((caddr_t)cpcb + USPACE) - 1)
		panic("syscall: trapframe");
#endif
	sticks = p->p_sticks;
	p->p_md.md_tf = tf;
	new = code & (SYSCALL_G7RFLAG | SYSCALL_G2RFLAG);
	code &= ~(SYSCALL_G7RFLAG | SYSCALL_G2RFLAG);

	callp = p->p_emul->e_sysent;
	nsys = p->p_emul->e_nsysent;

	/*
	 * The first six system call arguments are in the six %o registers.
	 * Any arguments beyond that are in the `argument extension' area
	 * of the user's stack frame (see <machine/frame.h>).
	 *
	 * Check for ``special'' codes that alter this, namely syscall and
	 * __syscall.  The latter takes a quad syscall number, so that other
	 * arguments are at their natural alignments.  Adjust the number
	 * of ``easy'' arguments as appropriate; we will copy the hard
	 * ones later as needed.
	 */
	ap = &tf->tf_out[0];
	nap = 6;

	switch (code) {
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
	case SYS___syscall:
		if (callp != sysent)
			break;
		code = ap[_QUAD_LOWWORD];
		ap += 2;
		nap -= 2;
		break;
	}

#ifdef DEBUG
/*	printf("code=%x, nsys=%x\n", code, nsys);*/
	if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW))
		printf("%d syscall(%x): tstate=%x:%x %s\n", curproc?curproc->p_pid:-1, code,
		       (int)(tf->tf_tstate>>32), (int)(tf->tf_tstate),
		       (code < 0 || code >= nsys)? "illegal syscall" : p->p_emul->e_syscallnames[code]);
	p->p_addr->u_pcb.lastcall = ((code < 0 || code >= nsys)? "illegal syscall" : p->p_emul->e_syscallnames[code]);
#endif
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;
	else {
		callp += code;
		i = callp->sy_argsize / sizeof(register_t);
		if (i > nap) {	/* usually false */
#ifdef DEBUG
			if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW))
				printf("Args %d>%d -- need to copyin\n", i , nap);
#endif
			if (i > 8)
				panic("syscall nargs");
			error = copyin((caddr_t)tf->tf_out[6] +
			    offsetof(struct frame, fr_argx),
			    (caddr_t)&args.i[nap], (i - nap) * sizeof(register_t));
			if (error) {
#ifdef KTRACE
				if (KTRPOINT(p, KTR_SYSCALL))
					ktrsyscall(p->p_tracep, code,
					    callp->sy_argsize, args.i);
#endif
				goto bad;
			}
			i = nap;
		}
#if 0
		copywords(ap, args.i, i * sizeof(register_t));
#else
		/* Need to convert from int64 to int32 or we lose */
		for (argp = &args.i[0]; i--;) 
				*argp++ = *ap++;
#endif
#ifdef DEBUG
		if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW)) {
			for (i=0; i < callp->sy_argsize / sizeof(register_t); i++) 
				printf("arg[%d]=%x ", i, (int)(args.i[i]));
			printf("\n");
		}
		if (trapdebug&(TDB_STOPCALL|TDB_SYSTOP)) { 
			printf("stop precall\n");
			Debugger();
		}
#endif
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_argsize, args.i);
#endif
	rval[0] = 0;
	rval[1] = tf->tf_out[1];
	error = (*callp->sy_call)(p, &args, rval);

	switch (error) {
	case 0:
		/* Note: fork() does not return here in the child */
		tf->tf_out[0] = rval[0];
		tf->tf_out[1] = rval[1];
		if (new) {
			/* jmp %g2 (or %g7, deprecated) on success */
			i = tf->tf_global[new & SYSCALL_G2RFLAG ? 2 : 7];
#ifdef DEBUG
			if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW))
				printf("syscall: return tstate=%x:%x new success to %p retval %x:%x\n", 
				       (int)(tf->tf_tstate>>32), (int)(tf->tf_tstate),
				       i, rval[0], rval[1]);
#endif
			if (i & 3) {
				error = EINVAL;
				goto bad;
			}
		} else {
			/* old system call convention: clear C on success */
			tf->tf_tstate &= ~(((int64_t)(ICC_C|XCC_C))<<TSTATE_CCR_SHIFT);	/* success */
			i = tf->tf_npc;
#ifdef DEBUG
			if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW))
				printf("syscall: return tstate=%x:%x old success to %p retval %x:%x\n", 
				       (int)(tf->tf_tstate>>32), (int)(tf->tf_tstate),
				       i, rval[0], rval[1]);
#endif
		}
		tf->tf_pc = i;
		tf->tf_npc = i + 4;
		break;

	case ERESTART:
	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->tf_out[0] = error;
		tf->tf_tstate |= (((int64_t)(ICC_C|XCC_C))<<TSTATE_CCR_SHIFT);	/* fail */
		i = tf->tf_npc;
		tf->tf_pc = i;
		tf->tf_npc = i + 4;
#ifdef DEBUG
		if (trapdebug&(TDB_SYSCALL|TDB_FOLLOW)) 
			printf("syscall: return tstate=%x:%x fail %d to %p\n", 
			       (int)(tf->tf_tstate>>32), (int)(tf->tf_tstate),
			       error, i);
#endif
		break;
	}

	userret(p, pc, sticks);
#ifdef NOTDEF_DEBUG
	if ( code == 202) {
		/* Trap on __sysctl */
		Debugger();
	}
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
	share_fpu(p, tf);
#ifdef DEBUG
	if (trapdebug&(TDB_STOPCALL|TDB_SYSTOP)) { 
		Debugger();
	}
#endif
#ifdef DEBUG
	if (trapdebug & TDB_FRAME) {
		print_trapframe(tf);
	}
#endif
}

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(p)
	struct proc *p;
{

	/*
	 * Return values in the frame set by cpu_fork().
	 */
#ifdef NOTDEF_DEBUG
	printf("child_return: proc=%p\n", p);
#endif
	userret(p, p->p_md.md_tf->tf_pc, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep,
			  (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}


/*
 * Code to handle alignment faults on the sparc. This is enabled by sending
 * a fixalign trap.  * Such code is generated by compiling with cc -misalign
 * on SunOS, but we don't have such a feature yet on our gcc.
 */
#ifdef DEBUG_ALIGN
# define DPRINTF(a) uprintf a
#else
# define DPRINTF(a)
#endif

#define GPR(tf, i)	((int32_t *) &tf->tf_global)[i]
#define IPR(tf, i)	((int32_t *) tf->tf_out[6])[i - 16]
#define FPR(p, i)	((int32_t) p->p_md.md_fpstate->fs_regs[i])

static __inline int readgpreg __P((struct trapframe *, int, void *));
static __inline int readfpreg __P((struct proc *, int, void *));
static __inline int writegpreg __P((struct trapframe *, int, const void *));
static __inline int writefpreg __P((struct proc *, int, const void *));


static __inline int
readgpreg(tf, i, val)
	struct trapframe *tf;
	int i;
	void *val;
{
	int error = 0;
	if (i == 0)
		*(int32_t *) val = 0;
	else if (i < 16)
		*(int32_t *) val = GPR(tf, i);
	else
		error = copyin(&IPR(tf, i), val, sizeof(int32_t));

	return error;
}

		
static __inline int
writegpreg(tf, i, val)
	struct trapframe *tf;
	int i;
	const void *val;
{
	int error = 0;

	if (i == 0)
		return error;
	else if (i < 16)
		GPR(tf, i) = *(int32_t *) val;
	else
		/* XXX: Fix copyout prototype */
		error = copyout((caddr_t) val, &IPR(tf, i), sizeof(int32_t));

	return error;
}
	

static __inline int
readfpreg(p, i, val)
	struct proc *p;
	int i;
	void *val;
{
	*(int32_t *) val = FPR(p, i);
	return 0;
}

		
static __inline int
writefpreg(p, i, val)
	struct proc *p;
	int i;
	const void *val;
{
	FPR(p, i) = *(const int32_t *) val;
	return 0;
}


static int
fixalign(p, tf)
	struct proc *p;
	struct trapframe *tf;
{
	static u_char sizedef[] = { 0x4, 0xff, 0x2, 0x8 };

	/*
	 * The following is not really a general instruction format;
	 * it is tailored to our needs
	 */
	union {
		struct {
			unsigned fmt:2;	/* 31..30 - 2 bit format */
			unsigned rd:5;	/* 29..25 - 5 bit destination reg */
			unsigned fl:1;	/* 24..24 - 1 bit float flag */
			unsigned op:1;	/* 23..23 - 1 bit opcode */
			unsigned sgn:1;	/* 22..22 - 1 bit sign */
			unsigned st:1;	/* 21..21 - 1 bit load/store */
			unsigned sz:2;	/* 20..19 - 2 bit size register */
			unsigned rs1:5;	/* 18..14 - 5 bit source reg 1 */
			unsigned imm:1;	/* 13..13 - 1 bit immediate flag */
			unsigned asi:8;	/* 12.. 5 - 8 bit asi bits */
			unsigned rs2:5;	/*  4.. 0 - 5 bit source reg 2 */
		} bits;
		int32_t word;
	} code;

	union {
		double	d;
		int32_t i[2];
		int16_t s[4];
		int8_t  c[8];
	} data;

	size_t size;
	int32_t offs, addr;
	int error;

	/* fetch and check the instruction that caused the fault */
	error = copyin((caddr_t) tf->tf_pc, &code.word, sizeof(code.word));
	if (error != 0) {
		DPRINTF(("fixalign: Bad instruction fetch\n"));
		return EINVAL;
	}

	/* Only support format 3 */
	if (code.bits.fmt != 3) {
		DPRINTF(("fixalign: Not a load or store\n"));
		return EINVAL;
	}

	/* Check operand size */
	if ((size = sizedef[code.bits.sz]) == 0xff) {
		DPRINTF(("fixalign: Bad operand size\n"));
		return EINVAL;
	}

	write_user_windows();

	if ((error = readgpreg(tf, code.bits.rs1, &addr)) != 0) {
		DPRINTF(("fixalign: read rs1 %d\n", error));
		return error;
	}

	/* Handle immediate operands */
	if (code.bits.imm) {
		offs = code.word & 0x1fff;
		if (offs & 0x1000)	/* Sign extend */
			offs |= 0xffffe;
	}
	else {
		if (code.bits.asi) {
			DPRINTF(("fixalign: Illegal instruction\n"));
			return EINVAL;
		}
		if ((error = readgpreg(tf, code.bits.rs2, &offs)) != 0) {
			DPRINTF(("fixalign: read rs2 %d\n", error));
			return error;
		}
	}
	addr += offs;

#ifdef DEBUG_ALIGN
	uprintf("memalign %x: %s%c%c %c%d, r%d, ", code.word,
	    code.bits.st ? "st" : "ld", "us"[code.bits.sgn],
	    "w*hd"[code.bits.sz], "rf"[code.bits.fl], code.bits.rd,
	    code.bits.rs1);
	if (code.bits.imm)
		uprintf("0x%x\n", offs);
	else
		uprintf("r%d\n", code.bits.rs2);
#endif
#ifdef DIAGNOSTIC
	if (code.bits.fl && p != fpproc)
		panic("fp align without being the FP owning process");
#endif

	if (code.bits.st) {
		if (code.bits.fl) {
			savefpstate(p->p_md.md_fpstate);

			error = readfpreg(p, code.bits.rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readfpreg(p, code.bits.rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}
		else {
			error = readgpreg(tf, code.bits.rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = readgpreg(tf, code.bits.rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
		}

		if (size == 2)
			return copyout(&data.s[1], (caddr_t) addr, size);
		else
			return copyout(&data.d, (caddr_t) addr, size);
	}
	else { /* load */
		if (size == 2) {
			error = copyin((caddr_t) addr, &data.s[1], size);
			if (error)
				return error;

			/* Sign extend if necessary */
			if (code.bits.sgn && (data.s[1] & 0x8000) != 0)
				data.s[0] = ~0;
			else
				data.s[0] = 0;
		}
		else
			error = copyin((caddr_t) addr, &data.d, size);

		if (error)
			return error;

		if (code.bits.fl) {
			error = writefpreg(p, code.bits.rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8) {
				error = writefpreg(p, code.bits.rd + 1,
				    &data.i[1]);
				if (error)
					return error;
			}
			loadfpstate(p->p_md.md_fpstate);
		}
		else {
			error = writegpreg(tf, code.bits.rd, &data.i[0]);
			if (error)
				return error;
			if (size == 8)
				error = writegpreg(tf, code.bits.rd + 1,
				    &data.i[1]);
		}
	}
	return error;
}
