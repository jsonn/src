/*	$NetBSD: db_machdep.c,v 1.31.2.1 2002/06/23 17:43:05 jdolecek Exp $	*/

/* 
 * :set tabs=4
 *
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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 *
 * VAX enhancements by cmcmanis@mcmanis.com no rights reserved :-)
 *
 */

/*
 * Interface to new debugger.
 * Taken from i386 port and modified for vax.
 */
#include "opt_ddb.h"
#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/intr.h>
#include <machine/rpb.h>
#include <vax/vax/gencons.h>

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_variables.h>

#include "ioconf.h"

db_regs_t ddb_regs;

void	kdbprinttrap(int, int);

int	db_active = 0;

extern int qdpolling;
static	int splsave; /* IPL before entering debugger */

#ifdef MULTIPROCESSOR
static struct cpu_info *stopcpu;
/*
 * Only the master CPU is allowed to enter DDB, but the correct frames
 * must still be there. Keep the state-machine here.
 */
static int
pause_cpus(void)
{
	volatile struct cpu_info *ci = curcpu();

	if (stopcpu == NULL) {
		stopcpu = curcpu();
		cpu_send_ipi(IPI_DEST_ALL, IPI_DDB);
	}
	if ((ci->ci_flags & CI_MASTERCPU) == 0) {
		ci->ci_flags |= CI_STOPPED;
		while (ci->ci_flags & CI_STOPPED)
			;
		return 1;
	} else
		return 0;
}

static void
resume_cpus(void)
{
	struct cpu_mp_softc *sc;
	struct cpu_info *ci;
	int i;

	stopcpu = NULL;
	for (i = 0; i < cpu_cd.cd_ndevs; i++) {
		if ((sc = cpu_cd.cd_devs[i]) == NULL)
			continue;
		ci = &sc->sc_ci;
		ci->ci_flags &= ~CI_STOPPED;
	}
}
#endif
/*
 * VAX Call frame on the stack, this from
 * "Computer Programming and Architecture, The VAX-11"
 *		Henry Levy & Richard Eckhouse Jr.
 *			ISBN 0-932376-07-X
 */
typedef struct __vax_frame {
	u_int	vax_cond;		/* condition handler		   */
	u_int	vax_psw:16;		/* 16 bit processor status word	   */
	u_int	vax_regs:12;		/* Register save mask.		   */
	u_int	vax_zero:1;		/* Always zero			   */
	u_int	vax_calls:1;		/* True if CALLS, false if CALLG   */
	u_int	vax_spa:2;		/* Stack pointer alignment	   */
	u_int	*vax_ap;		/* argument pointer		   */
	struct __vax_frame *vax_fp;	/* frame pointer of previous frame */
	u_int	vax_pc;			/* program counter		   */
	u_int	vax_args[1];		/* 0 or more arguments		   */
} VAX_CALLFRAME;

/*
 * DDB is called by either <ESC> - D on keyboard, via a TRACE or
 * BPT trap or from kernel, normally as a result of a panic.
 * If it is the result of a panic, set the ddb register frame to
 * contain the registers when panic was called. (easy to debug).
 */
void
kdb_trap(struct trapframe *frame)
{
	int s;
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
#endif

	switch (frame->trap) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP: /* single_step */
		break;

	/* XXX todo: should be migrated to use VAX_CALLFRAME at some point */
	case T_KDBTRAP:
#ifndef MULTIPROCESSOR	/* No fancy panic stack conversion here */
		if (panicstr) {
			struct	callsframe *pf, *df;

			df = (void *)frame->fp; /* start of debug's calls */
			pf = (void *)df->ca_fp; /* start of panic's calls */
			bcopy(&pf->ca_argno, &ddb_regs.r0, sizeof(int) * 12);
			ddb_regs.fp = pf->ca_fp;
			ddb_regs.pc = pf->ca_pc;
			ddb_regs.ap = pf->ca_ap;
			ddb_regs.sp = (unsigned)pf;
			ddb_regs.psl = frame->psl & ~0x1fffe0;
			ddb_regs.psl |= pf->ca_maskpsw & 0xffe0;
			ddb_regs.psl |= (splsave << 16);
		}
#endif
		break;

	default:
		if ((boothowto & RB_KDB) == 0)
			return;

		kdbprinttrap(frame->trap, frame->code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

#ifdef MULTIPROCESSOR
	ci->ci_ddb_regs = frame;
	if (pause_cpus())
		return;
#endif
#ifndef MULTIPROCESSOR
	if (!panicstr)
		bcopy(frame, &ddb_regs, sizeof(struct trapframe));
#else
	bcopy(stopcpu->ci_ddb_regs, &ddb_regs, sizeof(struct trapframe));
	printf("stopped on cpu %d\n", stopcpu->ci_cpuid);
#endif

	/* XXX Should switch to interrupt stack here, if needed. */

	s = splhigh();
	db_active++;
	cnpollc(TRUE);
	db_trap(frame->trap, frame->code);
	cnpollc(FALSE);
	db_active--;
	splx(s);

#ifndef MULTIPROCESSOR
	if (!panicstr)
		bcopy(&ddb_regs, frame, sizeof(struct trapframe));
#else
	bcopy(&ddb_regs, stopcpu->ci_ddb_regs, sizeof(struct trapframe));
#endif
	frame->sp = mfpr(PR_USP);
#ifdef MULTIPROCESSOR
	rpb.wait = 0;
	resume_cpus();
#endif
}

extern char *traptypes[];
extern int no_traps;

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= no_traps || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", traptypes[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vaddr_t addr;
	register size_t size;
	register char	*data;
{

	memcpy(data, (caddr_t)addr, size);
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vaddr_t addr;
	register size_t size;
	register char	*data;
{

	memcpy((caddr_t)addr, data, size);
}

void
Debugger()
{
	splsave = splx(0xe);	/* XXX WRONG (this can lower IPL) */
	setsoftddb();		/* beg for debugger */
	splx(splsave);
}

/*
 * Machine register set.
 */
const struct db_variable db_regs[] = {
	{"r0",	&ddb_regs.r0,	FCN_NULL},
	{"r1",	&ddb_regs.r1,	FCN_NULL},
	{"r2",	&ddb_regs.r2,	FCN_NULL},
	{"r3",	&ddb_regs.r3,	FCN_NULL},
	{"r4",	&ddb_regs.r4,	FCN_NULL},
	{"r5",	&ddb_regs.r5,	FCN_NULL},
	{"r6",	&ddb_regs.r6,	FCN_NULL},
	{"r7",	&ddb_regs.r7,	FCN_NULL},
	{"r8",	&ddb_regs.r8,	FCN_NULL},
	{"r9",	&ddb_regs.r9,	FCN_NULL},
	{"r10", &ddb_regs.r10,	FCN_NULL},
	{"r11", &ddb_regs.r11,	FCN_NULL},
	{"ap",	&ddb_regs.ap,	FCN_NULL},
	{"fp",	&ddb_regs.fp,	FCN_NULL},
	{"sp",	&ddb_regs.sp,	FCN_NULL},
	{"pc",	&ddb_regs.pc,	FCN_NULL},
	{"psl", &ddb_regs.psl,	FCN_NULL},
};
const struct db_variable * const db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

#define IN_USERLAND(x)	(((u_int)(x) & 0x80000000) == 0)

/*
 * Dump a stack traceback. Takes two arguments:
 *	fp - CALL FRAME pointer
 *	stackbase - Lowest stack value
 */
static void
db_dump_stack(VAX_CALLFRAME *fp, u_int stackbase,
    void (*pr)(const char *, ...)) {
	u_int nargs, arg_base, regs;
	VAX_CALLFRAME *tmp_frame;
	db_expr_t	diff;
	db_sym_t	sym;
	char		*symname;
	extern int	sret, etext;

	(*pr)("Stack traceback : \n");
	if (IN_USERLAND(fp)) {
		(*pr)("	 Process is executing in user space.\n");
		return;
	}

#if 0
	while (((u_int)(fp->vax_fp) > stackbase - 0x100) && 
			((u_int)(fp->vax_fp) < (stackbase + USPACE))) {
#endif
	while (!IN_USERLAND(fp->vax_fp)) {
		u_int pc = fp->vax_pc;

		/*
		 * Figure out the arguments by using a bit of subtlety.
		 * As the argument pointer may have been used as a temporary
		 * by the callee ... recreate what it would have pointed to
		 * as follows:
		 *  The vax_regs value has a 12 bit bitmask of the registers
		 *    that were saved on the stack.
		 *	Store that in 'regs' and then for every bit that is
		 *    on (indicates the register contents are on the stack)
		 *    increment the argument base (arg_base) by one.
		 *  When that is done, args[arg_base] points to the longword
		 *    that identifies the number of arguments.
		 *	arg_base+1 - arg_base+n are the argument pointers/contents.
		 */


		/* If this was due to a trap/fault, pull the correct pc
		 * out of the trap frame.  */
		if (pc == (u_int) &sret && fp->vax_fp != 0) {
			struct trapframe *tf;
			/* Isolate the saved register bits, and count them */
			regs = fp->vax_regs;
			for (arg_base = 0; regs != 0; regs >>= 1) {
				if (regs & 1)
					arg_base++;
			}
			tf = (struct trapframe *) &fp->vax_args[arg_base + 2];
			(*pr)("0x%lx: trap type=0x%lx code=0x%lx pc=0x%lx psl=0x%lx\n",
			      tf, tf->trap, tf->code, tf->pc, tf->psl);
			pc = tf->pc;
		}

		diff = INT_MAX;
		symname = NULL;
		if (pc >= 0x80000000 && pc < (u_int) &etext) {
			sym = db_search_symbol(pc, DB_STGY_ANY, &diff);
			db_symbol_values(sym, &symname, 0);
		}
		if (symname != NULL)
			(*pr)("0x%lx: %s+0x%lx(", fp, symname, diff);
		else
			(*pr)("0x%lx: %#x(", fp, pc);

		/* First get the frame that called this function ... */
		tmp_frame = fp->vax_fp;

		/* Isolate the saved register bits, and count them */
		regs = tmp_frame->vax_regs;
		for (arg_base = 0; regs != 0; regs >>= 1) {
			if (regs & 1)
				arg_base++;
		}

		/* number of arguments is then pointed to by vax_args[arg_base] */
		nargs = tmp_frame->vax_args[arg_base];
		if (nargs) {
			nargs--; /* reduce by one for formatting niceties */
			arg_base++; /* skip past the actual number of arguments */
			while (nargs--)
				(*pr)("%#x,", tmp_frame->vax_args[arg_base++]);

			/* now print out the last arg with closing brace and \n */
			(*pr)("%#x)\n", tmp_frame->vax_args[arg_base]);
		} else
			(*pr)("void)\n");
		/* move to the next frame */
		fp = fp->vax_fp;
	}
}

/*
 * Implement the trace command which has the form:
 *
 *	trace			<-- Trace panic (same as before)
 *	trace	0x88888		<-- Trace frame whose address is 888888
 *	trace/t			<-- Trace current process (0 if no current proc)
 *	trace/t 0tnn		<-- Trace process nn (0t for decimal)
 */
void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t	addr;		/* Address parameter */
	boolean_t	have_addr;	/* True if addr is valid */
	db_expr_t	count;		/* Optional count */
	char		*modif;		/* pointer to flag modifier 't' */
	void		(*pr) __P((const char *, ...)); /* Print function */
{
	extern vaddr_t	proc0paddr;
	struct proc	*p = curproc;
	struct user	*uarea;
	int		trace_proc;
	pid_t	curpid;
	char	*s;
 
	/* Check to see if we're tracing a process */
	trace_proc = 0;
	s = modif;
	while (!trace_proc && *s) {
		if (*s++ == 't')
			trace_proc++;	/* why yes we are */
	}

	/* Trace a panic */
	if (panicstr) {
		(*pr)("panic: %s\n", panicstr);
		/* xxx ? where did we panic and whose stack are we using? */
#ifdef MULTIPROCESSOR
		db_dump_stack((VAX_CALLFRAME *)(ddb_regs.fp), ddb_regs.ap, pr);
#else
		db_dump_stack((VAX_CALLFRAME *)(ddb_regs.sp), ddb_regs.ap, pr);
#endif
		return;
	}

	/* 
	 * If user typed an address its either a PID, or a Frame 
	 * if no address then either current proc or panic
	 */
	if (have_addr) {
		if (trace_proc) {
			p = pfind((int)addr);
			/* Try to be helpful by looking at it as if it were decimal */
			if (p == NULL) {
				u_int	tpid = 0;
				u_int	foo = addr;

				while (foo != 0) {
					int digit = (foo >> 28) & 0xf;
					if (digit > 9) {
						(*pr)("	 No such process.\n");
						return;
					}
					tpid = tpid * 10 + digit;
					foo = foo << 4;
				}
				p = pfind(tpid);
				if (p == NULL) {
					(*pr)("	 No such process.\n");
					return;
				}
			}
		} else {
			db_dump_stack((VAX_CALLFRAME *)addr, 0, pr);
			return;
		}
	} else {
		if (trace_proc) {
			p = curproc;
			if (p == NULL) {
				(*pr)("trace: no current process! (ignored)\n");
				return;
			}
		} else {
			if (! panicstr) {
				(*pr)("Not a panic, no active process, ignored.\n");
				return;
			}
		}
	}
	if (p == NULL) {
		uarea = (struct user *)proc0paddr;
		curpid = 0;
	} else {
		uarea = p->p_addr;
		curpid = p->p_pid;
	}
	(*pr)("Process %d\n", curpid);
	(*pr)("	 PCB contents:\n");
	(*pr)(" KSP = 0x%x\n", (unsigned int)(uarea->u_pcb.KSP));
	(*pr)(" ESP = 0x%x\n", (unsigned int)(uarea->u_pcb.ESP));
	(*pr)(" SSP = 0x%x\n", (unsigned int)(uarea->u_pcb.SSP));
	(*pr)(" USP = 0x%x\n", (unsigned int)(uarea->u_pcb.USP));
	(*pr)(" R[00] = 0x%08x	  R[06] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[0]), (unsigned int)(uarea->u_pcb.R[6]));
	(*pr)(" R[01] = 0x%08x	  R[07] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[1]), (unsigned int)(uarea->u_pcb.R[7]));
	(*pr)(" R[02] = 0x%08x	  R[08] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[2]), (unsigned int)(uarea->u_pcb.R[8]));
	(*pr)(" R[03] = 0x%08x	  R[09] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[3]), (unsigned int)(uarea->u_pcb.R[9]));
	(*pr)(" R[04] = 0x%08x	  R[10] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[4]), (unsigned int)(uarea->u_pcb.R[10]));
	(*pr)(" R[05] = 0x%08x	  R[11] = 0x%08x\n", 
		(unsigned int)(uarea->u_pcb.R[5]), (unsigned int)(uarea->u_pcb.R[11]));
	(*pr)(" AP = 0x%x\n", (unsigned int)(uarea->u_pcb.AP));
	(*pr)(" FP = 0x%x\n", (unsigned int)(uarea->u_pcb.FP));
	(*pr)(" PC = 0x%x\n", (unsigned int)(uarea->u_pcb.PC));
	(*pr)(" PSL = 0x%x\n", (unsigned int)(uarea->u_pcb.PSL));
	(*pr)(" Trap frame pointer: 0x%x\n", 
							(unsigned int)(uarea->u_pcb.framep));
	db_dump_stack((VAX_CALLFRAME *)(uarea->u_pcb.FP),
	    (u_int) uarea->u_pcb.KSP, pr);
	return;
#if 0
	while (((u_int)(cur_frame->vax_fp) > stackbase) && 
			((u_int)(cur_frame->vax_fp) < (stackbase + USPACE))) {
		u_int nargs;
		VAX_CALLFRAME *tmp_frame;

		diff = INT_MAX;
		symname = NULL;
		sym = db_search_symbol(cur_frame->vax_pc, DB_STGY_ANY, &diff);
		db_symbol_values(sym, &symname, 0);
		(*pr)("%s+0x%lx(", symname, diff);

		/*
		 * Figure out the arguments by using a bit of subterfuge
		 * since the argument pointer may have been used as a temporary
		 * by the callee ... recreate what it would have pointed to
		 * as follows:
		 *  The vax_regs value has a 12 bit bitmask of the registers
		 *    that were saved on the stack.
		 *	Store that in 'regs' and then for every bit that is
		 *    on (indicates the register contents are on the stack)
		 *    increment the argument base (arg_base) by one.
		 *  When that is done, args[arg_base] points to the longword
		 *    that identifies the number of arguments.
		 *	arg_base+1 - arg_base+n are the argument pointers/contents.
		 */

		/* First get the frame that called this function ... */
		tmp_frame = cur_frame->vax_fp;

		/* Isolate the saved register bits, and count them */
		regs = tmp_frame->vax_regs;
		for (arg_base = 0; regs != 0; regs >>= 1) {
			if (regs & 1)
				arg_base++;
		}

		/* number of arguments is then pointed to by vax_args[arg_base] */
		nargs = tmp_frame->vax_args[arg_base];
		if (nargs) {
			nargs--; /* reduce by one for formatting niceties */
			arg_base++; /* skip past the actual number of arguments */
			while (nargs--)
				(*pr)("0x%x,", tmp_frame->vax_args[arg_base++]);

			/* now print out the last arg with closing brace and \n */
			(*pr)("0x%x)\n", tmp_frame->vax_args[++arg_base]);
		} else
			(*pr)("void)\n");
		/* move to the next frame */
		cur_frame = cur_frame->vax_fp;
	}

	/*
	 * DEAD CODE, previous panic tracing code.
	 */
	if (! have_addr) {
		printf("Trace default\n");
		if (panicstr) {
			cf = (int *)ddb_regs.sp;
		} else {
			printf("Don't know what to do without panic\n");
			return;
		}
		if (p)
			paddr = (u_int)p->p_addr;
		else
			paddr = proc0paddr;

		stackbase = (ddb_regs.psl & PSL_IS ? istack : paddr);
	}
#endif
}

static int ddbescape = 0;

int
kdbrint(tkn)
	int tkn;
{

	if (ddbescape && ((tkn & 0x7f) == 'D')) {
		setsoftddb();
		ddbescape = 0;
		return 1;
	}

	if ((ddbescape == 0) && ((tkn & 0x7f) == 27)) {
		ddbescape = 1;
		return 1;
	}

	if (ddbescape) {
		ddbescape = 0;
		return 2;
	}
	
	ddbescape = 0;
	return 0;
}

#ifdef MULTIPROCESSOR

static void
db_mach_cpu(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	struct cpu_mp_softc *sc;
	struct cpu_info *ci;

	if ((addr < 0) || (addr >= cpu_cd.cd_ndevs))
		return db_printf("%ld: cpu out of range\n", addr);
	if ((sc = cpu_cd.cd_devs[addr]) == NULL)
		return db_printf("%ld: cpu not configured\n", addr);

	ci = &sc->sc_ci;
	if ((ci != curcpu()) && ((ci->ci_flags & CI_STOPPED) == 0))
		return db_printf("cpu %ld not stopped???\n", addr);

	bcopy(&ddb_regs, stopcpu->ci_ddb_regs, sizeof(struct trapframe));
	stopcpu = ci;
	bcopy(stopcpu->ci_ddb_regs, &ddb_regs, sizeof(struct trapframe));
	db_printf("using cpu %ld", addr);
	if (ci->ci_curproc)
		db_printf(" in proc %d (%s)\n", ci->ci_curproc->p_pid,
		    ci->ci_curproc->p_comm);
}
#endif

const struct db_command db_machine_command_table[] = {
#ifdef MULTIPROCESSOR
	{ "cpu",	db_mach_cpu,	0,	0 },
#endif
	{ NULL },
};
