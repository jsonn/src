/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 *	$Id: machdep.c,v 1.111.2.3 1994/08/14 09:04:46 mycroft Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>

#include <i386/isa/isa.h>
#include <i386/isa/isavar.h>
#include <i386/isa/rtc.h>

#include "isa.h"
#include "npx.h"

/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* cpu "architecture" */

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int	physmem;
int	boothowto;
int	cpu_class;

struct	msgbuf *msgbufp;
int	msgbufmapped;

vm_map_t buffer_map;

extern	vm_offset_t avail_start, avail_end;
static	vm_offset_t hole_start, hole_end;
static	vm_offset_t avail_next;
static	vm_size_t avail_remaining;

int	_udatasel, _ucodesel, _gsel_tss;

long dumplo;

void dumpsys __P((void));

caddr_t allocsys();

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v;
	int sz;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;

	/*
	 * Initialize error message buffer (at end of core).
	 */
	/* avail_end was pre-decremented in pmap_bootstrap to compensate */
	for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
		pmap_enter(kernel_pmap,
		    (vm_offset_t)((caddr_t)msgbufp + i * NBPG),
		    avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];

	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
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
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
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
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / (10 * CLSIZE);
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) /
			    (20 * CLSIZE);
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	return v;
}

/*  
 * Info for CTL_HW
 */
char	cpu_model[120];
extern	char version[];

struct cpu_nameclass i386_cpus[] = {
	{ "i386SX",	CPUCLASS_386 },	/* CPU_386SX */
	{ "i386DX",	CPUCLASS_386 },	/* CPU_386   */
	{ "i486SX",	CPUCLASS_486 },	/* CPU_486SX */
	{ "i486DX",	CPUCLASS_486 },	/* CPU_486   */
	{ "Pentium",	CPUCLASS_586 },	/* CPU_586   */
	{ "Cx486DLC",	CPUCLASS_486 },	/* CPU_486DLC (Cyrix) */
};

identifycpu()
{
	int len;
	extern char cpu_vendor[];

	printf("CPU: ");
#ifdef DIAGNOSTIC
	if (cpu < 0 || cpu >= (sizeof i386_cpus/sizeof(struct cpu_nameclass)))
		panic("unknown cpu type %d\n", cpu);
#endif
	sprintf(cpu_model, "%s (", i386_cpus[cpu].cpu_name);
	if (cpu_vendor[0] != '\0') {
		strcat(cpu_model, cpu_vendor);
		strcat(cpu_model, " ");
	}

	cpu_class = i386_cpus[cpu].cpu_class;
	switch(cpu_class) {
	case CPUCLASS_386:
		strcat(cpu_model, "386");
		break;
	case CPUCLASS_486:
		strcat(cpu_model, "486");
		break;
	case CPUCLASS_586:
		strcat(cpu_model, "586");
		break;
	default:
		strcat(cpu_model, "unknown");	/* will panic below... */
	}
	strcat(cpu_model, "-class CPU)");
	printf("%s\n", cpu_model);	/* cpu speed would be nice, but how? */

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU)
#error No CPU classes configured.
#endif
#if !defined(I386_CPU)
	case CPUCLASS_386:
#endif
#if !defined(I486_CPU)
	case CPUCLASS_486:
#endif
#if !defined(I586_CPU)
	case CPUCLASS_586:
#endif
#if !defined(I386_CPU) || !defined(I486_CPU) || !defined(I586_CPU)
		panic("CPU class not configured");
#endif
	default:
		break;
	}
}

/*  
 * machine dependent system variables.
 */ 
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	dev_t consdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

#ifdef PGINPROF
/*
 * Return the difference (in microseconds) between the current time and a
 * previous time as represented by the arguments.  If there is a pending
 * clock interrupt which has not been serviced due to high ipl, return error
 * code.
 */
/*ARGSUSED*/
vmtime(otime, olbolt, oicr)
	register int otime, olbolt, oicr;
{

	return (((time.tv_sec-otime)*HZ + lbolt-olbolt)*(1000000/HZ));
}
#endif

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	unsigned code;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;
	extern char sigcode[], esigcode[];

	tf = (struct trapframe *)p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_base +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else {
		fp = (struct sigframe *)tf->tf_esp - 1;
	}

	/* 
	 * Build the argument list for the signal handler.
	 */
	frame.sf_signum = sig;
	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask = mask;
	frame.sf_sc.sc_es = tf->tf_es;
	frame.sf_sc.sc_ds = tf->tf_ds;
	frame.sf_sc.sc_edi = tf->tf_edi;
	frame.sf_sc.sc_esi = tf->tf_esi;
	frame.sf_sc.sc_ebp = tf->tf_ebp;
	frame.sf_sc.sc_ebx = tf->tf_ebx;
	frame.sf_sc.sc_edx = tf->tf_edx;
	frame.sf_sc.sc_ecx = tf->tf_ecx;
	frame.sf_sc.sc_eax = tf->tf_eax;
	frame.sf_sc.sc_eip = tf->tf_eip;
	frame.sf_sc.sc_cs = tf->tf_cs;
	frame.sf_sc.sc_efl = tf->tf_eflags;
	frame.sf_sc.sc_esp = tf->tf_esp;
	frame.sf_sc.sc_ss = tf->tf_ss;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_esp = (int)fp;
	tf->tf_eip = (int)(((char *)PS_STRINGS) - (esigcode - sigcode));
	tf->tf_eflags &= ~PSL_VM;
	tf->tf_cs = _ucodesel;
	tf->tf_ds = _udatasel;
	tf->tf_es = _udatasel;
	tf->tf_ss = _udatasel;
}

static __inline int
check_selectors(u_short cs, u_short ss, u_short ds, u_short es)
{
	int result;

	__asm __volatile("
	xorl	%%edx,%%edx

	movw	%1,%%dx
	verr	%%dx
	jnz	1f
	movl	%%edx,%%eax

	movw	%2,%%dx
	verr	%%dx
	jnz	1f
	andl	%%edx,%%eax

	movw	%3,%%dx
	testw	$0xfffc,%%dx
	jz	2f
	verr	%%dx
	jnz	1f
	andl	%%edx,%%eax

2:	movw	%4,%%dx
	testw	$0xfffc,%%dx
	jz	2f
	verr	%%dx
	jnz	1f
	andl	%%edx,%%eax

2:	andl	$3,%%eax
	subl	$3,%%eax
	jmp	3f
1:	movl	$1,%%eax
3:
	": "=&a" (result)
	 : "g" (cs), "g" (ss), "g" (ds), "g" (es)
	 : "%edx");
	return result;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
struct sigreturn_args {
	struct sigcontext *scp;
};

sigreturn(p, uap, retval)
	struct proc *p;
	struct sigreturn_args *uap;
	int *retval;
{
	struct sigcontext *scp, context;
	register struct sigframe *fp;
	register struct trapframe *tf;
	int eflags;

	tf = (struct trapframe *)p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = uap->scp;
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return(EFAULT);

	eflags = context.sc_efl;
	if ((eflags & PSL_USERCLR) != 0 ||
	    (eflags & PSL_USERSET) != PSL_USERSET ||
	    (eflags & PSL_IOPL) > (tf->tf_eflags & PSL_IOPL))
		return(EINVAL);

	/*
	 * Sanity check the user's selectors and error if they are suspect.
	 * We assume that swtch() has loaded the correct LDT descriptor, so
	 * we can just use the `verr' instruction.  We further assume that
	 * none of the segments we wish to protect are conforming.  (If they
	 * were, this check wouldn't help much anyway.)
	 */
	if (check_selectors(context.sc_cs, context.sc_ss, context.sc_ds,
	    context.sc_es)) {
		trapsignal(p, SIGBUS, T_PROTFLT);
		return(EINVAL);
	}

	if (context.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = context.sc_mask &~
	    (sigmask(SIGKILL)|sigmask(SIGCONT)|sigmask(SIGSTOP));

	/*
	 * Restore signal context.
	 */
	tf->tf_es = context.sc_es;
	tf->tf_ds = context.sc_ds;
	tf->tf_edi = context.sc_edi;
	tf->tf_esi = context.sc_esi;
	tf->tf_ebp = context.sc_ebp;
	tf->tf_ebx = context.sc_ebx;
	tf->tf_edx = context.sc_edx;
	tf->tf_ecx = context.sc_ecx;
	tf->tf_eax = context.sc_eax;
	tf->tf_eip = context.sc_eip;
	tf->tf_cs = context.sc_cs;
	tf->tf_eflags = eflags;
	tf->tf_esp = context.sc_esp;
	tf->tf_ss = context.sc_ss;

	return(EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	register int howto;
{
	extern int cold;

	if (cold) {
		printf("hit reset please");
		for(;;);
	}
	boothowto = howto;
	if ((howto&RB_NOSYNC) == 0 && waittime < 0) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		(void) spl0();
		printf("syncing disks... ");
		/*
		 * Release inodes held by texts before update.
		 */
		if (panicstr == 0)
			vnode_pager_umount(NULL);
		sync(&proc0, (void *)0, (int *)0);
#if 0
		/*
		 * Unmount filesystems
		 */
		if (panicstr == 0)
			vfs_unmountall();
#endif
		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
			delay(40000 * iter);
		}
		if (nbusy)
			printf("giving up\n");
		else
			printf("done\n");
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();
	if (howto&RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	} else {
		if (howto & RB_DUMP) {
			savectx(&dumppcb, 0);
			dumppcb.pcb_ptd = rcr3();
			dumpsys();
			/*NOTREACHED*/
		}
	}
	printf("rebooting...\n");
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

unsigned	dumpmag = 0x8fca0101;	/* magic number for savecore */
int		dumpsize = 0;		/* also for savecore */
/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{

	if (dumpdev == NODEV)
		return;
	if ((minor(dumpdev)&07) != 1)
		return;
	dumpsize = physmem;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {

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

	case EINTR:
		printf("aborted from console\n");
		break;

	default:
		printf("succeeded\n");
		break;
	}
	printf("\n\n");
	delay(1000);
}

#ifdef HZ
/*
 * If HZ is defined we use this code, otherwise the code in
 * /sys/i386/i386/microtime.s is used.  The other code only works
 * for HZ=100.
 */
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();

	*tvp = time;
	tvp->tv_usec += tick;
	splx(s);
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
}
#endif /* HZ */

/*
 * Clear registers on exec
 */
void
setregs(p, entry, stack, retval)
	struct proc *p;
	u_long entry;
	u_long stack;
	int retval[2];
{
	register struct trapframe *tf;

	tf = (struct trapframe *)p->p_md.md_regs;
	tf->tf_ebp = 0;	/* bottom of the fp chain */
	tf->tf_eip = entry;
	tf->tf_esp = stack;
	tf->tf_ss = _udatasel;
	tf->tf_ds = _udatasel;
	tf->tf_es = _udatasel;
	tf->tf_cs = _ucodesel;
	tf->tf_eflags = PSL_USERSET | (tf->tf_eflags & PSL_T);

	p->p_addr->u_pcb.pcb_flags &= 0 /* FM_SYSCTRC */; /* no fp at all */
#if NNPX > 0
	npxexit();
	npxinit(__INITIAL_NPXCW__);
#else
	lcr0(rcr0() | CR0_TS);	/* start emulating */
#endif

	retval[1] = 0;
}

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments and descriptor tables
 */

union descriptor gdt[NGDT];
union descriptor ldt[NLDT];
struct gate_descriptor idt[NIDT];

int _default_ldt, currentldt;

struct	i386tss	tss, panic_tss;

extern  struct user *proc0paddr;

/* software prototypes -- in more palatable form */
struct soft_segment_descriptor gdt_segs[] = {
	/* Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* LDT Descriptor */
{	(int) ldt,			/* segment base address  */
	sizeof(ldt)-1,		/* length - all address space */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - Placeholder */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Panic Tss Descriptor */
{	(int) &panic_tss,		/* segment base address  */
	sizeof(tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Proc 0 Tss Descriptor */
{	(int) USRSTACK,		/* segment base address  */
	sizeof(tss)-1,		/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* User LDT Descriptor per process */
{	(int) ldt,			/* segment base address  */
	(512 * sizeof(union descriptor)-1),		/* length */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
};

struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ } };

void
setidt(idx, func, typ, dpl)
	int idx;
	char *func;
	int typ, dpl;
{
	struct gate_descriptor *ip = idt + idx;

	ip->gd_looffset = (int)func;
	ip->gd_selector = 8;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16;
}

#define	IDTVEC(name)	__CONCAT(X, name)
extern	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(dble), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(rsvd1), IDTVEC(rsvd2), IDTVEC(rsvd3), IDTVEC(rsvd4),
	IDTVEC(rsvd5), IDTVEC(rsvd6), IDTVEC(rsvd7), IDTVEC(rsvd8),
	IDTVEC(rsvd9), IDTVEC(rsvd10), IDTVEC(rsvd11), IDTVEC(rsvd12),
	IDTVEC(rsvd13), IDTVEC(rsvd14), IDTVEC(rsvd14), IDTVEC(syscall);

void
init386(first_avail)
	vm_offset_t first_avail;
{
	extern ssdtosd(), lgdt(), etext;
	int x, *pi;
	unsigned biosbasemem, biosextmem;
	struct gate_descriptor *gdp;
	extern char sigcode[], esigcode[];
	/* table descriptors - used to load tables by microp */
	struct region_descriptor r_gdt, r_idt;
	void consinit __P((void));

	proc0.p_addr = proc0paddr;

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

#ifndef LKM		/* don't do this if we're using LKM's */
	/* set code segment limit to end of kernel text */
	gdt_segs[GCODE_SEL].ssd_limit = i386_btop(i386_round_page(&etext)) - 1;
#endif

	for (x = 0; x < NGDT; x++)
		ssdtosd(gdt_segs + x, gdt + x);

	/* make ldt memory segments */
	ldt_segs[LUCODE_SEL].ssd_limit = i386_btop(VM_MAXUSER_ADDRESS) - 1;
	ldt_segs[LUDATA_SEL].ssd_limit = i386_btop(VM_MAXUSER_ADDRESS) - 1;
	for (x = 0; x < NLDT; x++)
		ssdtosd(ldt_segs + x, ldt + x);

	/* exceptions */
	setidt(0, &IDTVEC(div), SDT_SYS386TGT, SEL_KPL);
	setidt(1, &IDTVEC(dbg), SDT_SYS386TGT, SEL_KPL);
	setidt(2, &IDTVEC(nmi), SDT_SYS386TGT, SEL_KPL);
	setidt(3, &IDTVEC(bpt), SDT_SYS386TGT, SEL_UPL);	/* XXXX */
	setidt(4, &IDTVEC(ofl), SDT_SYS386TGT, SEL_KPL);
	setidt(5, &IDTVEC(bnd), SDT_SYS386TGT, SEL_KPL);
	setidt(6, &IDTVEC(ill), SDT_SYS386TGT, SEL_KPL);
	setidt(7, &IDTVEC(dna), SDT_SYS386TGT, SEL_KPL);
	setidt(8, &IDTVEC(dble), SDT_SYS386TGT, SEL_KPL);
	setidt(9, &IDTVEC(fpusegm), SDT_SYS386TGT, SEL_KPL);
	setidt(10, &IDTVEC(tss), SDT_SYS386TGT, SEL_KPL);
	setidt(11, &IDTVEC(missing), SDT_SYS386TGT, SEL_KPL);
	setidt(12, &IDTVEC(stk), SDT_SYS386TGT, SEL_KPL);
	setidt(13, &IDTVEC(prot), SDT_SYS386TGT, SEL_KPL);
	setidt(14, &IDTVEC(page), SDT_SYS386TGT, SEL_KPL);
	setidt(15, &IDTVEC(rsvd), SDT_SYS386TGT, SEL_KPL);
	setidt(16, &IDTVEC(fpu), SDT_SYS386TGT, SEL_KPL);
	setidt(17, &IDTVEC(align), SDT_SYS386TGT, SEL_KPL);
	setidt(18, &IDTVEC(rsvd1), SDT_SYS386TGT, SEL_KPL);
	setidt(19, &IDTVEC(rsvd2), SDT_SYS386TGT, SEL_KPL);
	setidt(20, &IDTVEC(rsvd3), SDT_SYS386TGT, SEL_KPL);
	setidt(21, &IDTVEC(rsvd4), SDT_SYS386TGT, SEL_KPL);
	setidt(22, &IDTVEC(rsvd5), SDT_SYS386TGT, SEL_KPL);
	setidt(23, &IDTVEC(rsvd6), SDT_SYS386TGT, SEL_KPL);
	setidt(24, &IDTVEC(rsvd7), SDT_SYS386TGT, SEL_KPL);
	setidt(25, &IDTVEC(rsvd8), SDT_SYS386TGT, SEL_KPL);
	setidt(26, &IDTVEC(rsvd9), SDT_SYS386TGT, SEL_KPL);
	setidt(27, &IDTVEC(rsvd10), SDT_SYS386TGT, SEL_KPL);
	setidt(28, &IDTVEC(rsvd11), SDT_SYS386TGT, SEL_KPL);
	setidt(29, &IDTVEC(rsvd12), SDT_SYS386TGT, SEL_KPL);
	setidt(30, &IDTVEC(rsvd13), SDT_SYS386TGT, SEL_KPL);
	setidt(31, &IDTVEC(rsvd14), SDT_SYS386TGT, SEL_KPL);

#if NISA > 0
	isa_defaultirq();
#endif
	r_gdt.rd_limit = sizeof(gdt)-1;
	r_gdt.rd_base = (int) gdt;
	lgdt(&r_gdt);
	r_idt.rd_limit = sizeof(idt)-1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);
	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	currentldt = _default_ldt;

#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	if (boothowto & RB_KDB)
		kgdb_connect(0);
#endif

	/*
	 * Use BIOS values stored in RTC CMOS RAM, since probing
	 * breaks certain 386 AT relics.
	 */
	biosbasemem = (rtcin(RTC_BASEHI)<<8) | (rtcin(RTC_BASELO));
	biosextmem = (rtcin(RTC_EXTHI)<<8) | (rtcin(RTC_EXTLO));

#ifndef BIOS_BASEMEM
#define	BIOS_BASEMEM 640
#endif

	if (biosbasemem == 0 || biosbasemem > 640) {
		printf("warning: nvram reports %dk base memory; assuming %dk\n",
		    biosbasemem, BIOS_BASEMEM);
		biosbasemem = BIOS_BASEMEM;
	}

	avail_start = NBPG;	/* BIOS leaves data in low memory */
				/* and VM system doesn't work with phys 0 */
	avail_end = biosextmem ? IOM_END + biosextmem * 1024
	    : biosbasemem * 1024;

	/* number of pages of physmem addr space */
	physmem = btoc((biosbasemem + biosextmem) * 1024);

	/*
	 * Initialize for pmap_free_pages and pmap_next_page.
	 * These guys should be page-aligned.
	 */
	hole_start = biosbasemem * 1024;
	/* we load right after the I/O hole; adjust hole_end to compensate */
	hole_end = round_page((vm_offset_t)first_avail);
	avail_next = avail_start;
	avail_remaining = i386_btop((avail_end - avail_start) -
				    (hole_end - hole_start));

	if (avail_remaining < i386_btop(2 * 1024 * 1024)) {
		printf("warning: too little memory available; running in degraded mode\n"
		    "press a key to confirm\n\n");
		/*
		 * People with less than 2 Meg have to press a key; this way
		 * we see the messages and can tell them why they blow up later.
		 * If they get working well enough to recompile, they can remove
		 * this; otherwise, it's a toy and they have to lump it.
		 */
		cngetc();
	}

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap((vm_offset_t)atdevbase + IOM_SIZE);

	/* now running on new page tables, configured,and u/iom is accessible */

	/* make a initial tss so microp can get interrupt stack on syscall! */
	proc0.p_addr->u_pcb.pcb_tss.tss_esp0 = (int) USRSTACK + UPAGES*NBPG;
	proc0.p_addr->u_pcb.pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	_gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);

	((struct i386tss *)gdt_segs[GPROC0_SEL].ssd_base)->tss_ioopt =
		(sizeof(tss))<<16;

	ltr(_gsel_tss);

	/* make a call gate to reenter kernel with */
	gdp = &ldt[LSYS5CALLS_SEL].gd;

	x = (int) &IDTVEC(syscall);
	gdp->gd_looffset = x++;
	gdp->gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	gdp->gd_stkcpy = 1;	/* leaves room for eflags like a trap */
	gdp->gd_type = SDT_SYS386CGT;
	gdp->gd_dpl = SEL_UPL;
	gdp->gd_p = 1;
	gdp->gd_hioffset = ((int) &IDTVEC(syscall)) >>16;

	/* transfer to user mode */
	_ucodesel = LSEL(LUCODE_SEL, SEL_UPL);
	_udatasel = LSEL(LUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	proc0.p_addr->u_pcb.pcb_flags = 0;
	proc0.p_addr->u_pcb.pcb_ptd = IdlePTD;
}

struct queue {
	struct queue *q_next, *q_prev;
};

/*
 * insert an element into a queue
 */
void
_insque(elem, head)
	register struct queue *elem, *head;
{
	register struct queue *next;

	next = head->q_next;
	elem->q_next = next;
	head->q_next = elem;
	elem->q_prev = head;
	next->q_prev = elem;
}

/*
 * remove an element from a queue
 */
void
_remque(elem)
	register struct queue *elem;
{
	register struct queue *next, *prev;

	next = elem->q_next;
	prev = elem->q_prev;
	next->q_prev = prev;
	prev->q_next = next;
	elem->q_prev = 0;
}


#ifdef COMPAT_NOMID
static int
exec_nomid(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	u_long midmag, magic;
	u_short mid;
	struct exec *execp = epp->ep_hdr;

	/* check on validity of epp->ep_hdr performed by exec_out_makecmds */

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	if (magic == 0) {
		magic = (execp->a_midmag & 0xffff);
		mid = MID_ZERO;
	}

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (MID_ZERO << 16) | ZMAGIC:
		/*
		 * 386BSD's ZMAGIC format:
		 */
		error = cpu_exec_aout_prep_oldzmagic(p, epp);
		break;

	case (MID_ZERO << 16) | QMAGIC:
		/*
		 * BSDI's QMAGIC format:
		 * same as new ZMAGIC format, but with different magic number
		 */
		error = exec_aout_prep_zmagic(p, epp);
		break;

	default:
		error = ENOEXEC;
	}

	return error;
}
#endif

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 *
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 * On the i386, old (386bsd) ZMAGIC binaries and BSDI QMAGIC binaries
 * if COMPAT_NOMID is given as a kernel option.
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;
#ifdef COMPAT_SVR4
	extern int svr4_exec_elf_makecmds __P((struct proc *,
					       struct exec_package *));
#endif /* ! COMPAT_SVR4 */

#ifdef COMPAT_NOMID
	if ((error = exec_nomid(p, epp)) == 0)
		return error;
#endif /* ! COMPAT_NOMID */

#ifdef COMPAT_SVR4
	if ((error = svr4_exec_elf_makecmds(p, epp)) == 0)
		return error;
#endif /* ! COMPAT_SVR4 */

	return error;
}

#ifdef COMPAT_NOMID
/*
 * cpu_exec_aout_prep_oldzmagic():
 *	Prepare the vmcmds to build a vmspace for an old (386BSD) ZMAGIC
 *	binary.
 *
 * Cloned from exec_aout_prep_zmagic() in kern/exec_aout.c; a more verbose
 * description of operation is there.
 */
int
cpu_exec_aout_prep_oldzmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	struct exec_vmcmd *ccmdp;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, NBPG, /* XXX should NBPG be CLBYTES? */
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp,
	    execp->a_text + NBPG, /* XXX should NBPG be CLBYTES? */
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_aout_setup_stack(p, epp);
}
#endif /* COMPAT_NOMID */

unsigned int
pmap_free_pages()
{

	return avail_remaining;
}

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{

	if (avail_next == avail_end)
		return FALSE;

	/* skip the hole */
	if (avail_next == hole_start)
		avail_next = hole_end;

	*addrp = avail_next;
	avail_next += NBPG;
	avail_remaining--;
	return TRUE;
}

unsigned int
pmap_page_index(pa)
	vm_offset_t pa;
{

	if (pa >= avail_start && pa < hole_start)
		return i386_btop(pa - avail_start);
	if (pa >= hole_end && pa < avail_end)
		return i386_btop(pa - hole_end + hole_start - avail_start);
	return -1;
}

/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this initted thing, but then,
 * it shouldn't be called from init386 either.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;
	cninit();
}
