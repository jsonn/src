/* $NetBSD: machdep.c,v 1.47.2.1 1997/11/19 00:09:41 mellon Exp $	 */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 * 
 * Changed for the VAX port (and for readability) /IC
 * 
 * This code is derived from software contributed to Berkeley by the Systems
 * Programming Group of the University of Utah Computer Science Department.
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
 * from: Utah Hdr: machdep.c 1.63 91/04/24
 * 
 * @(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/ptrace.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <net/netisr.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include "arp.h"	/* For NERISR_ARP */
#if NARP > 0
#include <netinet/if_inarp.h>
#endif
#include <netinet/ip_var.h>
#endif
#ifdef NETATALK
#include <netatalk/at_extern.h>
#endif
#ifdef NS
#include <netns/ns_var.h>
#endif
#include "ppp.h"	/* For NERISR_PPP */
#if NPPP > 0
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

#include <machine/sid.h>
#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>
#include <machine/macros.h>
#include <machine/nexus.h>
#include <machine/trap.h>
#include <machine/reg.h>
#include <machine/db_machdep.h>
#include <vax/vax/gencons.h>

#ifdef DDB
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

void	netintr __P((void));
void	machinecheck __P((caddr_t));
void	cmrerr __P((void));

extern int virtual_avail, virtual_end;
/*
 * We do these external declarations here, maybe they should be done
 * somewhere else...
 */
int		nmba, cold = 1;
caddr_t		mcraddr[MAXNMCR];
int		astpending;
int		want_resched;
char		machine[] = MACHINE;		/* from <machine/param.h> */
char		machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char		cpu_model[100];
caddr_t		msgbufaddr;
int		physmem;
struct cfdriver nexuscd;
int		todrstopped = 0;
int		dumpsize = 0;

caddr_t allocsys __P((caddr_t));

#define valloclim(name, type, num, lim) \
		(name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))

#ifdef	BUFPAGES
int		bufpages = BUFPAGES;
#else
int		bufpages = 0;
#endif
int		nswbuf = 0;
#ifdef	NBUF
int		nbuf = NBUF;
#else
int		nbuf = 0;
#endif

void
cpu_startup()
{
	caddr_t		v;
	extern char	version[];
	int		base, residual, i, sz;
	vm_offset_t	minaddr, maxaddr;
	vm_size_t	size;
	extern unsigned int avail_end;

	/*
	 * Initialize error message buffer.
	 */
	initmsgbuf(msgbufaddr, round_page(MSGBUFSIZE));

#if VAX750 || VAX650
	if (vax_cputype == VAX_750 || vax_cputype == VAX_650)
		if (!mfpr(PR_TODR))
			mtpr(todrstopped = 1, PR_TODR);
#endif
	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s\n", version);
	printf("realmem = %d\n", avail_end);
	physmem = btoc(avail_end);
	panicstr = NULL;
	mtpr(AST_NO, PR_ASTLVL);
	spl0();

	dumpsize = physmem + 1;

	/*
	 * Find out how much space we need, allocate it, and then give
	 * everything true virtual addresses.
	 */

	sz = (int) allocsys((caddr_t) 0);
	if ((v = (caddr_t) kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.	 They are different than the above in
	 * that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *) & buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t) buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t) 0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t	curbufsize;
		vm_offset_t	curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.	The rest get (base) physical pages.
		 * 
		 * The rest of each buffer occupies virtual space, but has no
		 * physical memory allocated for it.
		 */
		curbuf = (vm_offset_t) buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively limits
	 * the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, TRUE);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *) & mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, TRUE);

	/*
	 * Initialize callouts
	 */

	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i - 1].c_next = &callout[i];
	callout[i - 1].c_next = NULL;

	printf("avail mem = %d\n", (int)ptoa(cnt.v_free_count));
	printf("Using %d buffers containing %d bytes of memory.\n",
	       nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */

	bufinit();

	/*
	 * Configure the system.
	 */
	configure();
}

/*
 * Allocate space for system data structures.  We are given a starting
 * virtual address and we return a final virtual address; along the way we
 * set each data structure pointer.
 * 
 * We call allocsys() with 0 to find out how much space we want, allocate that
 * much and fill it with zeroes, and then call allocsys() again with the
 * correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate (enough to hold 5% of total
	 * physical memory, but at least 16). Allocate 1/2 as many swap
	 * buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = (physmem / 10) / CLSIZE;
		else
			bufpages = (physmem / 20) / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;	/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	return v;
}

long	dumplo = 0;
long	dumpmag = 0x8fca0101;

void
cpu_dumpconf()
{
	int		nblks;

	/*
	 * XXX include the final RAM page which is not included in physmem.
	 */
	dumpsize = physmem + 1;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize) (dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(dumpsize));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?) in case the dump
	 * device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
}

void
cpu_initclocks()
{
	(*dep_call->cpu_clock) ();
}

int
cpu_sysctl(a, b, c, d, e, f, g)
	int	*a;
	u_int	b;
	void	*c, *e;
	size_t	*d, f;
	struct	proc *g;
{
	return (EOPNOTSUPP);
}

void
setstatclockrate(hzrate)
	int hzrate;
{
	panic("setstatclockrate");
}

void
consinit()
{
	cninit();
#ifdef DDB
/*	db_machine_init(); */
	ddb_init();
#ifdef donotworkbyunknownreason
	if (boothowto & RB_KDB)
		Debugger();
#endif
#endif
}

int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct trapframe *scf;
	struct sigcontext *cntx;

	scf = p->p_addr->u_pcb.framep;
	cntx = SCARG(uap, sigcntxp);

	/* Compatibility mode? */
	if ((cntx->sc_ps & (PSL_IPL | PSL_IS)) ||
	    ((cntx->sc_ps & (PSL_U | PSL_PREVU)) != (PSL_U | PSL_PREVU)) ||
	    (cntx->sc_ps & PSL_CM)) {
		return (EINVAL);
	}
	if (cntx->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = cntx->sc_mask & ~sigcantmask;

	scf->fp = cntx->sc_fp;
	scf->ap = cntx->sc_ap;
	scf->pc = cntx->sc_pc;
	scf->sp = cntx->sc_sp;
	scf->psl = cntx->sc_ps;
	return (EJUSTRETURN);
}

struct trampframe {
	unsigned	sig;	/* Signal number */
	unsigned	code;	/* Info code */
	unsigned	scp;	/* Pointer to struct sigcontext */
	unsigned	r0, r1, r2, r3, r4, r5; /* Registers saved when
						 * interrupt */
	unsigned	pc;	/* Address of signal handler */
	unsigned	arg;	/* Pointer to first (and only) sigreturn
				 * argument */
};

void
sendsig(catcher, sig, mask, code)
	sig_t		catcher;
	int		sig, mask;
	u_long		code;
{
	struct	proc	*p = curproc;
	struct	sigacts *psp = p->p_sigacts;
	struct	trapframe *syscf;
	struct	sigcontext *sigctx;
	struct	trampframe *trampf;
	unsigned	cursp;
	int	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	extern	char sigcode[], esigcode[];
	/*
	 * Allocate and validate space for the signal handler context. Note
	 * that if the stack is in P0 space, the call to grow() is a nop, and
	 * the useracc() check will fail if the process has not already
	 * allocated the space with a `brk'. We shall allocate space on the
	 * stack for both struct sigcontext and struct calls...
	 */
	syscf = p->p_addr->u_pcb.framep;

	/* First check what stack to work on */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		cursp = (int)(psp->ps_sigstk.ss_sp + psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		cursp = syscf->sp;
	if (cursp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void) grow(p, cursp);

	/* Set up positions for structs on stack */
	sigctx = (struct sigcontext *) (cursp - sizeof(struct sigcontext));
	trampf = (struct trampframe *) ((unsigned)sigctx -
	    sizeof(struct trampframe));

	 /* Place for pointer to arg list in sigreturn */
	cursp = (unsigned)sigctx - 8;

	if (useracc((caddr_t) cursp, sizeof(struct sigcontext) +
		    sizeof(struct trampframe), B_WRITE) == 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}
	/* Set up pointers for sigreturn args */
	trampf->arg = (int) sigctx;
	trampf->pc = (unsigned) catcher;
	trampf->scp = (int) sigctx;
	trampf->code = code;
	trampf->sig = sig;


	sigctx->sc_pc = syscf->pc;
	sigctx->sc_ps = syscf->psl;
	sigctx->sc_ap = syscf->ap;
	sigctx->sc_fp = syscf->fp;
	sigctx->sc_sp = syscf->sp;
	sigctx->sc_onstack = oonstack;
	sigctx->sc_mask = mask;

	syscf->pc = (unsigned) (((char *) PS_STRINGS) - (esigcode - sigcode));
	syscf->psl = PSL_U | PSL_PREVU;
	syscf->ap = cursp;
	syscf->sp = cursp;
}

int	waittime = -1;
static	volatile int showto; /* Must be volatile to survive MM on -> MM off */

void
cpu_reboot(howto, b)
	register howto;
	char *b;
{
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr will be out of
		 * synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();		/* extreme priority */
	if (howto & RB_HALT) {
		if (dep_call->cpu_halt)
			(*dep_call->cpu_halt) ();
		printf("halting (in tight loop); hit\n\t^P\n\tHALT\n\n");
		for (;;)
			;
	} else {
		showto = howto;
#ifdef notyet
		/*
		 * If we are provided with a bootstring, parse it and send
		 * it to the boot program.
		 */
		if (b)
			while (*b) {
				showto |= (*b == 'a' ? RB_ASKBOOT : (*b == 'd' ?
				    RB_DEBUG : (*b == 's' ? RB_SINGLE : 0)));
				b++;
			}
#endif
		/*
		 * Now it's time to:
		 *  0. Save some registers that are needed in new world.
		 *  1. Change stack to somewhere that will survive MM off.
		 * (RPB page is good page to save things in).
		 *  2. Actually turn MM off.
		 *  3. Dump away memory to disk, if asked.
		 *  4. Reboot as asked.
		 * The RPB page is _always_ first page in memory, we can
		 * rely on that.
		 */
#ifdef notyet
		asm("	movl	sp, (0x80000200)
			movl	0x80000200, sp
			mfpr	$0x10, -(sp)	# PR_PCBB
			mfpr	$0x11, -(sp)	# PR_SCBB
			mfpr	$0xc, -(sp)	# PR_SBR
			mfpr	$0xd, -(sp)	# PR_SLR
			mtpr	$0, $0x38	# PR_MAPEN
		");
#endif

		if (showto & RB_DUMP)
			dumpsys();
		if (dep_call->cpu_reboot)
			(*dep_call->cpu_reboot)(showto);

		switch (vax_cputype) {
			int	state;

#if VAX750 || VAX780
		case VAX_780:
		case VAX_750:
			mtpr(GC_BOOT, PR_TXDB); /* boot command */
			break;
#endif
#if VAX8600
		case VAX_8600:
			state = mfpr(PR_TXCS);
			gencnputc(0, GC_LT | GC_WRT);
			mtpr(0x2, PR_TXDB); /* XXX */
			gencnputc(0, state | GC_WRT);
			break;
#endif
		}

	}
	asm("movl %0,r5":: "g" (showto)); /* How to boot */
	asm("movl %0, r11":: "r"(showto)); /* ??? */
	asm("halt");
	panic("Halt sket sej");
}

void
netintr()
{
#ifdef INET
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
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
	}
#endif
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		pppintr();
	}
#endif
}

void
machinecheck(frame)
	caddr_t frame;
{
	if ((*dep_call->cpu_mchk) (frame) == 0)
		return;
	(*dep_call->cpu_memerr) ();
	panic("machine check");
}

void
dumpsys()
{

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during autoconfiguration, if dump device has already
	 * configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, (int)dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump) (dumpdev, 0, 0, 0)) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	default:
		printf("succeeded\n");
		break;
	}
}

int
fuswintr(addr)
	const void *addr;
{
	panic("fuswintr: need to be implemented");
	return 0;

}

int
suibyte(base, byte)
	void *base;
	short byte;
{
	panic("suibyte: need to be implemented");
	return 0;
}

int
suswintr(addr, cnt)
	void *addr;
	short	cnt;
{
	panic("suswintr: need to be implemented");
	return 0;
}

int
process_read_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&tf->r0, &regs->r0, 12 * sizeof(int));
	regs->ap = tf->ap;
	regs->fp = tf->fp;
	regs->sp = tf->sp;
	regs->pc = tf->pc;
	regs->psl = tf->psl;
	return 0;
}

int
process_write_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&regs->r0, &tf->r0, 12 * sizeof(int));
	tf->ap = regs->ap;
	tf->fp = regs->fp;
	tf->sp = regs->sp;
	tf->pc = regs->pc;
	tf->psl = regs->psl;
	return 0;
}

int
process_set_pc(p, addr)
	struct	proc *p;
	caddr_t addr;
{
	struct	trapframe *tf;
	void	*ptr;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = (char *) p->p_addr->u_pcb.framep;
	tf = ptr;

	tf->pc = (unsigned) addr;

	return (0);
}

int
process_sstep(p, sstep)
	struct proc    *p;
{
	void	       *ptr;
	struct trapframe *tf;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = p->p_addr->u_pcb.framep;
	tf = ptr;

	if (sstep)
		tf->psl |= PSL_T;
	else
		tf->psl &= ~PSL_T;

	return (0);
}

void
cmrerr()
{
	(*dep_call->cpu_memerr) ();
}
