/*	$NetBSD: machdep.c,v 1.80.2.1 1997/02/12 12:47:03 mrg Exp $	*/

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
 * from: Utah $Hdr: machdep.c 1.74 92/12/20$
 *
 *	@(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/mount.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>

#include <dev/cons.h>

#define	MAXMEM	64*1024*CLSIZE	/* XXX - from cmap.h */
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

/* the following is used externally (sysctl_hw) */
char machine[] = "hp300";		/* cpu "architecture" */

vm_map_t buffer_map;
extern vm_offset_t avail_end;

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
int	msgbufmapped;		/* set when safe to use msgbuf */
int	maxmem;			/* max memory per process */
int	physmem = MAXMEM;	/* max supported memory, changes to actual */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

extern	u_int lowram;
extern	short exframesize[];

#ifdef COMPAT_HPUX
extern struct emul emul_hpux;
#endif

/* prototypes for local functions */
caddr_t	allocsys __P((caddr_t));
void	parityenable __P((void));
int	parityerror __P((struct frame *));
int	parityerrorfind __P((void));
void    identifycpu __P((void));
void    initcpu __P((void));
void    ledinit __P((void));
void	dumpmem __P((int *, int, int));
char	*hexstr __P((int, int));

/* functions called from locore.s */
void    dumpsys __P((void));
void    straytrap __P((int, u_short));
void	nmihand __P((struct frame));

/*
 * Select code of console.  Set to -1 if console is on
 * "internal" framebuffer.
 */
int	conscode;
int	consinit_active;	/* flag for driver init routines */
caddr_t	conaddr;		/* for drivers in cn_init() */
int	convasize;		/* size of mapped console device */
int	conforced;		/* console has been forced */

/*
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuspeed (where cpuspeed is in MHz) on 68020
 * and 68030 systems.  See clock.c for the delay
 * calibration algorithm.
 */
int	cpuspeed;		/* relative cpu speed; XXX skewed on 68040 */
int	delay_divisor;		/* delay constant */

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	extern struct map extiomap[];

	/*
	 * Initialize some variables for sanity.
	 */
	consinit_active = 1;
	convasize = 0;
	conforced = 0;
	conscode = 1024;		/* invalid */

	/*
	 * Initialize the DIO resource map.
	 */
	rminit(extiomap, (long)EIOMAPSIZE, (long)1, "extio", EIOMAPSIZE/16);

	/*
	 * Initialize the console before we print anything out.
	 */
	hp300_cninit();

	consinit_active = 0;

#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	extern char *etext;
	register unsigned i;
	register caddr_t v;
	int base, residual, sz;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < btoc(sizeof (struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufp,
		    avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and the give everything true virtual addresses.
	 */
	size = (vm_size_t)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(size))) == 0)
		panic("startup: no room for tables");
	if ((allocsys(v) - v) != size)
		panic("startup: talbe size inconsistency");

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
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Tell the VM system that page 0 isn't mapped.
	 *
	 * XXX This is bogus; should just fix KERNBASE and
	 * XXX VM_MIN_KERNEL_ADDRESS, but not right now.
	 */
	if (vm_map_protect(kernel_map, 0, NBPG, VM_PROT_NONE, TRUE)
	    != KERN_SUCCESS)
		panic("can't mark page 0 off-limits");

	/*
	 * Tell the VM system that writing to kernel text isn't allowed.
	 * If we don't, we might end up COW'ing the text segment!
	 *
	 * XXX Should be hp300_trunc_page(&kernel_text) instead
	 * XXX of NBPG.
	 */
	if (vm_map_protect(kernel_map, NBPG, hp300_round_page(&etext),
	    VM_PROT_READ|VM_PROT_EXECUTE, TRUE) != KERN_SUCCESS)
		panic("can't protect kernel text");

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

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
 * allocate that much and fill it with zeroes, and the call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num)	\
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))

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
	 * Determine how many buffers to allocate.  Since HPs tend
	 * to be long on memory and short on disk speed, we allocate
	 * more buffer space than the BSD standard of 10% of memory
	 * for the first 2 Meg, 5% of the remaining.  We just allocate
	 * a flag 10%.  Insure a minimum of 16 buffers.  We allocate
	 * 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem / 10 / CLSIZE;
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
	return (v);
}

/*
 * Set registers on exec.
 * XXX Should clear registers except sp, pc,
 * but would break init; should be fixed soon.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct frame *frame = (struct frame *)p->p_md.md_regs;

	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[SP] = stack;
	frame->f_regs[A2] = (int)PS_STRINGS;
#ifdef FPCOPROC
	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);
#endif
#ifdef COMPAT_HPUX
	p->p_md.md_flags &= ~MDP_HPUXMMAP;
	if (p->p_emul == &emul_hpux) {
		frame->f_regs[A0] = 0; /* not 68010 (bit 31), no FPA (30) */
		retval[0] = 0;		/* no float card */
#ifdef FPCOPROC
		retval[1] = 1;		/* yes 68881 */
#else
		retval[1] = 0;		/* no 68881 */
#endif
	}
	/*
	 * XXX This doesn't have much to do with setting registers but
	 * I didn't want to muck up kern_exec.c with this code, so I
	 * stuck it here.
	 *
	 * Ensure we perform the right action on traps type 1 and 2:
	 * If our parent is an HPUX process and we are being traced, turn
	 * on HPUX style interpretation.  Else if we were using the HPUX
	 * style interpretation, revert to the BSD interpretation.
	 *
	 * Note that we do this by changing the trap instruction in the
	 * global "sigcode" array which then gets copied out to the user's
	 * sigcode in the stack.  Since we are changing it in the global
	 * array we must always reset it, even for non-HPUX processes.
	 *
	 * Note also that implementing it in this way creates a potential
	 * race where we could have tweaked it for process A which then
	 * blocks in the copyout to the stack and process B comes along
	 * and untweaks it causing A to wind up with the wrong setting
	 * when the copyout continues.  However, since we have already
	 * copied something out to this user stack page (thereby faulting
	 * it in), this scenerio is extremely unlikely.
	 */
	{
		extern short sigcodetrap[];

		if ((p->p_pptr->p_emul == &emul_hpux) &&
		    (p->p_flag & P_TRACED)) {
			p->p_md.md_flags |= MDP_HPUXTRACE;
			*sigcodetrap = 0x4E42;
		} else {
			p->p_md.md_flags &= ~MDP_HPUXTRACE;
			*sigcodetrap = 0x4E41;
		}
	}
#endif
}

/*
 * Info for CTL_HW
 */
char	cpu_model[120];
extern	char version[];

void
identifycpu()
{
	char *t, *mc;
	int len;

	switch (machineid) {
	case HP_320:
		t = "320 (16.67MHz";
		break;
	case HP_330:
		t = "318/319/330 (16.67MHz";
		break;
	case HP_340:
		t = "340 (16.67MHz";
		break;
	case HP_350:
		t = "350 (25MHz";
		break;
	case HP_360:
		t = "360 (25MHz";
		break;
	case HP_370:
		t = "370 (33.33MHz";
		break;
	case HP_375:
		t = "345/375 (50MHz";
		break;
	case HP_380:
		t = "380/425 (25MHz";
		break;
	case HP_433:
		t = "433 (33MHz";
		break;
	default:
		printf("\nunknown machine type %d\n", machineid);
		panic("startup");
	}
	mc = (mmutype == MMU_68040 ? "40" :
	       (mmutype == MMU_68030 ? "30" : "20"));
	sprintf(cpu_model, "HP9000/%s MC680%s CPU", t, mc);
	switch (mmutype) {
	case MMU_68040:
	case MMU_68030:
		strcat(cpu_model, "+MMU");
		break;
	case MMU_68851:
		strcat(cpu_model, ", MC68851 MMU");
		break;
	case MMU_HP:
		strcat(cpu_model, ", HP MMU");
		break;
	default:
		printf("%s\nunknown MMU type %d\n", cpu_model, mmutype);
		panic("startup");
	}
	len = strlen(cpu_model);
	if (mmutype == MMU_68040)
		len += sprintf(cpu_model + len,
		    "+FPU, 4k on-chip physical I/D caches");
	else if (mmutype == MMU_68030)
		len += sprintf(cpu_model + len, ", %sMHz MC68882 FPU",
		       machineid == HP_340 ? "16.67" :
		       (machineid == HP_360 ? "25" :
			(machineid == HP_370 ? "33.33" : "50")));
	else
		len += sprintf(cpu_model + len, ", %sMHz MC68881 FPU",
		       machineid == HP_350 ? "20" : "16.67");
	switch (ectype) {
	case EC_VIRT:
		sprintf(cpu_model + len, ", %dK virtual-address cache",
		       machineid == HP_320 ? 16 : 32);
		break;
	case EC_PHYS:
		sprintf(cpu_model + len, ", %dK physical-address cache",
		       machineid == HP_370 ? 64 : 32);
		break;
	}
	strcat(cpu_model, ")");
	printf("%s\n", cpu_model);
	printf("delay constant for this cpu: %d\n", delay_divisor);
	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (machineid) {
	case -1:		/* keep compilers happy */
#if !defined(HP320) && !defined(HP350)
	case HP_320:
	case HP_350:
#endif
#ifndef HP330
	case HP_330:
#endif
#if !defined(HP340) && !defined(HP360) && !defined(HP370)
	case HP_340:
	case HP_360:
	case HP_370:
#endif
#if !defined(HP380)
	case HP_380:
	case HP_433:
#endif
		panic("CPU type not configured");
	default:
		break;
	}
}

/*
 * machine dependent system variables.
 */
int
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

#ifdef USELEDS
#include <hp300/hp300/led.h>

int inledcontrol = 0;	/* 1 if we are in ledcontrol already, cheap mutex */
char *ledaddr;

/*
 * Map the LED page and setup the KVA to access it.
 */
void
ledinit()
{
	extern caddr_t ledbase;

	pmap_enter(pmap_kernel(), (vm_offset_t)ledbase, (vm_offset_t)LED_ADDR,
		   VM_PROT_READ|VM_PROT_WRITE, TRUE);
	ledaddr = (char *) ((int)ledbase | (LED_ADDR & PGOFSET));
}

/*
 * Do lights:
 *	`ons' is a mask of LEDs to turn on,
 *	`offs' is a mask of LEDs to turn off,
 *	`togs' is a mask of LEDs to toggle.
 * Note we don't use splclock/splx for mutual exclusion.
 * They are expensive and we really don't need to be that precise.
 * Besides we would like to be able to profile this routine.
 */
void
ledcontrol(ons, offs, togs)
	register int ons, offs, togs;
{
	static char currentleds;
	register char leds;

	inledcontrol = 1;
	leds = currentleds;
	if (ons)
		leds |= ons;
	if (offs)
		leds &= ~offs;
	if (togs)
		leds ^= togs;
	currentleds = leds;
	*ledaddr = ~leds;
	inledcontrol = 0;
}
#endif

#define SS_RTEFRAME	1
#define SS_FPSTATE	2
#define SS_USERREGS	4

struct sigstate {
	int	ss_flags;		/* which of the following are valid */
	struct	frame ss_frame;		/* original exception frame */
	struct	fpframe ss_fpstate;	/* 68881/68882 state info */
};

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signum;		/* signo for handler */
	int	sf_code;		/* additional info for handler */
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigstate sf_state;	/* state of the hardware */
	struct	sigcontext sf_sc;	/* actual context */
};

#ifdef COMPAT_HPUX
struct	hpuxsigcontext {
	int	hsc_syscall;
	char	hsc_action;
	char	hsc_pad1;
	char	hsc_pad2;
	char	hsc_onstack;
	int	hsc_mask;
	int	hsc_sp;
	short	hsc_ps;
	int	hsc_pc;
/* the rest aren't part of the context but are included for our convenience */
	short	hsc_pad;
	u_int	hsc_magic;		/* XXX sigreturn: cookie */
	struct	sigcontext *hsc_realsc;	/* XXX sigreturn: ptr to BSD context */
};

/*
 * For an HP-UX process, a partial hpuxsigframe follows the normal sigframe.
 * Tremendous waste of space, but some HP-UX applications (e.g. LCL) need it.
 */
struct hpuxsigframe {
	int	hsf_signum;
	int	hsf_code;
	struct	sigcontext *hsf_scp;
	struct	hpuxsigcontext hsf_sc;
	int	hsf_regs[15];
};
#endif

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct sigframe *fp, *kfp;
	register struct frame *frame;
	register struct sigacts *psp = p->p_sigacts;
	register short ft;
	int oonstack, fsize;
	extern char sigcode[], esigcode[];

	frame = (struct frame *)p->p_md.md_regs;
	ft = frame->f_format;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
#ifdef COMPAT_HPUX
	if (p->p_emul == &emul_hpux)
		fsize = sizeof(struct sigframe) + sizeof(struct hpuxsigframe);
	else
#endif
	fsize = sizeof(struct sigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)(frame->f_regs[SP] - fsize);
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)grow(p, (unsigned)fp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x ft %d\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc, ft);
#endif
	if (useracc((caddr_t)fp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): useracc failed on sig %d\n",
			       p->p_pid, sig);
#endif
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
	kfp = (struct sigframe *)malloc((u_long)fsize, M_TEMP, M_WAITOK);
	/* 
	 * Build the argument list for the signal handler.
	 */
	kfp->sf_signum = sig;
	kfp->sf_code = code;
	kfp->sf_scp = &fp->sf_sc;
	kfp->sf_handler = catcher;
	/*
	 * Save necessary hardware state.  Currently this includes:
	 *	- general registers
	 *	- original exception frame (if not a "normal" frame)
	 *	- FP coprocessor state
	 */
	kfp->sf_state.ss_flags = SS_USERREGS;
	bcopy((caddr_t)frame->f_regs,
	      (caddr_t)kfp->sf_state.ss_frame.f_regs, sizeof frame->f_regs);
	if (ft >= FMT7) {
#ifdef DEBUG
		if (ft > 15 || exframesize[ft] < 0)
			panic("sendsig: bogus frame type");
#endif
		kfp->sf_state.ss_flags |= SS_RTEFRAME;
		kfp->sf_state.ss_frame.f_format = frame->f_format;
		kfp->sf_state.ss_frame.f_vector = frame->f_vector;
		bcopy((caddr_t)&frame->F_u,
		      (caddr_t)&kfp->sf_state.ss_frame.F_u, exframesize[ft]);
		/*
		 * Leave an indicator that we need to clean up the kernel
		 * stack.  We do this by setting the "pad word" above the
		 * hardware stack frame to the amount the stack must be
		 * adjusted by.
		 *
		 * N.B. we increment rather than just set f_stackadj in
		 * case we are called from syscall when processing a
		 * sigreturn.  In that case, f_stackadj may be non-zero.
		 */
		frame->f_stackadj += exframesize[ft];
		frame->f_format = frame->f_vector = 0;
#ifdef DEBUG
		if (sigdebug & SDB_FOLLOW)
			printf("sendsig(%d): copy out %d of frame %d\n",
			       p->p_pid, exframesize[ft], ft);
#endif
	}
#ifdef FPCOPROC
	kfp->sf_state.ss_flags |= SS_FPSTATE;
	m68881_save(&kfp->sf_state.ss_fpstate);
#ifdef DEBUG
	if ((sigdebug & SDB_FPSTATE) && *(char *)&kfp->sf_state.ss_fpstate)
		printf("sendsig(%d): copy out FP state (%x) to %x\n",
		       p->p_pid, *(u_int *)&kfp->sf_state.ss_fpstate,
		       &kfp->sf_state.ss_fpstate);
#endif
#endif
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	kfp->sf_sc.sc_onstack = oonstack;
	kfp->sf_sc.sc_mask = mask;
	kfp->sf_sc.sc_sp = frame->f_regs[SP];
	kfp->sf_sc.sc_fp = frame->f_regs[A6];
	kfp->sf_sc.sc_ap = (int)&fp->sf_state;
	kfp->sf_sc.sc_pc = frame->f_pc;
	kfp->sf_sc.sc_ps = frame->f_sr;
#ifdef COMPAT_HPUX
	/*
	 * Create an HP-UX style sigcontext structure and associated goo
	 */
	if (p->p_emul == &emul_hpux) {
		register struct hpuxsigframe *hkfp;

		hkfp = (struct hpuxsigframe *)&kfp[1];
		hkfp->hsf_signum = bsdtohpuxsig(kfp->sf_signum);
		hkfp->hsf_code = kfp->sf_code;
		hkfp->hsf_scp = (struct sigcontext *)
			&((struct hpuxsigframe *)(&fp[1]))->hsf_sc;
		hkfp->hsf_sc.hsc_syscall = 0;		/* XXX */
		hkfp->hsf_sc.hsc_action = 0;		/* XXX */
		hkfp->hsf_sc.hsc_pad1 = hkfp->hsf_sc.hsc_pad2 = 0;
		hkfp->hsf_sc.hsc_onstack = kfp->sf_sc.sc_onstack;
		hkfp->hsf_sc.hsc_mask = kfp->sf_sc.sc_mask;
		hkfp->hsf_sc.hsc_sp = kfp->sf_sc.sc_sp;
		hkfp->hsf_sc.hsc_ps = kfp->sf_sc.sc_ps;
		hkfp->hsf_sc.hsc_pc = kfp->sf_sc.sc_pc;
		hkfp->hsf_sc.hsc_pad = 0;
		hkfp->hsf_sc.hsc_magic = 0xdeadbeef;
		hkfp->hsf_sc.hsc_realsc = kfp->sf_scp;
		bcopy((caddr_t)frame->f_regs, (caddr_t)hkfp->hsf_regs,
		      sizeof (hkfp->hsf_regs));

		kfp->sf_signum = hkfp->hsf_signum;
		kfp->sf_scp = hkfp->hsf_scp;
	}
#endif
	(void) copyout((caddr_t)kfp, (caddr_t)fp, fsize);
	frame->f_regs[SP] = (int)fp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): sig %d scp %x fp %x sc_sp %x sc_ap %x\n",
		       p->p_pid, sig, kfp->sf_scp, fp,
		       kfp->sf_sc.sc_sp, kfp->sf_sc.sc_ap);
#endif
	/*
	 * Signal trampoline code is at base of user stack.
	 */
	frame->f_pc = (int)PS_STRINGS - (esigcode - sigcode);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
	free((caddr_t)kfp, M_TEMP);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct frame *frame;
	register int rf;
	struct sigcontext tsigc;
	struct sigstate tstate;
	int flags;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int)scp & 1)
		return (EINVAL);
#ifdef COMPAT_HPUX
	/*
	 * Grab context as an HP-UX style context and determine if it
	 * was one that we contructed in sendsig.
	 */
	if (p->p_emul == &emul_hpux) {
		struct hpuxsigcontext *hscp = (struct hpuxsigcontext *)scp;
		struct hpuxsigcontext htsigc;

		if (useracc((caddr_t)hscp, sizeof (*hscp), B_WRITE) == 0 ||
		    copyin((caddr_t)hscp, (caddr_t)&htsigc, sizeof htsigc))
			return (EINVAL);
		/*
		 * If not generated by sendsig or we cannot restore the
		 * BSD-style sigcontext, just restore what we can -- state
		 * will be lost, but them's the breaks.
		 */
		hscp = &htsigc;
		if (hscp->hsc_magic != 0xdeadbeef ||
		    (scp = hscp->hsc_realsc) == 0 ||
		    useracc((caddr_t)scp, sizeof (*scp), B_WRITE) == 0 ||
		    copyin((caddr_t)scp, (caddr_t)&tsigc, sizeof tsigc)) {
			if (hscp->hsc_onstack & 01)
				p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
			else
				p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
			p->p_sigmask = hscp->hsc_mask &~ sigcantmask;
			frame = (struct frame *) p->p_md.md_regs;
			frame->f_regs[SP] = hscp->hsc_sp;
			frame->f_pc = hscp->hsc_pc;
			frame->f_sr = hscp->hsc_ps &~ PSL_USERCLR;
			return (EJUSTRETURN);
		}
		/*
		 * Otherwise, overlay BSD context with possibly modified
		 * HP-UX values.
		 */
		tsigc.sc_onstack = hscp->hsc_onstack;
		tsigc.sc_mask = hscp->hsc_mask;
		tsigc.sc_sp = hscp->hsc_sp;
		tsigc.sc_ps = hscp->hsc_ps;
		tsigc.sc_pc = hscp->hsc_pc;
	} else
#endif
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t)scp, sizeof (*scp), B_WRITE) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&tsigc, sizeof tsigc))
		return (EINVAL);
	scp = &tsigc;
	if ((scp->sc_ps & (PSL_MBZ|PSL_IPL|PSL_S)) != 0)
		return (EINVAL);
	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask &~ sigcantmask;
	frame = (struct frame *) p->p_md.md_regs;
	frame->f_regs[SP] = scp->sc_sp;
	frame->f_regs[A6] = scp->sc_fp;
	frame->f_pc = scp->sc_pc;
	frame->f_sr = scp->sc_ps;
	/*
	 * Grab pointer to hardware state information.
	 * If zero, the user is probably doing a longjmp.
	 */
	if ((rf = scp->sc_ap) == 0)
		return (EJUSTRETURN);
	/*
	 * See if there is anything to do before we go to the
	 * expense of copying in close to 1/2K of data
	 */
	flags = fuword((caddr_t)rf);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): sc_ap %x flags %x\n",
		       p->p_pid, rf, flags);
#endif
	/*
	 * fuword failed (bogus sc_ap value).
	 */
	if (flags == -1)
		return (EINVAL);
	if (flags == 0 || copyin((caddr_t)rf, (caddr_t)&tstate, sizeof tstate))
		return (EJUSTRETURN);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sigreturn(%d): ssp %x usp %x scp %x ft %d\n",
		       p->p_pid, &flags, scp->sc_sp, SCARG(uap, sigcntxp),
		       (flags&SS_RTEFRAME) ? tstate.ss_frame.f_format : -1);
#endif
	/*
	 * Restore most of the users registers except for A6 and SP
	 * which were handled above.
	 */
	if (flags & SS_USERREGS)
		bcopy((caddr_t)tstate.ss_frame.f_regs,
		      (caddr_t)frame->f_regs, sizeof(frame->f_regs)-2*NBPW);
	/*
	 * Restore long stack frames.  Note that we do not copy
	 * back the saved SR or PC, they were picked up above from
	 * the sigcontext structure.
	 */
	if (flags & SS_RTEFRAME) {
		register int sz;
		
		/* grab frame type and validate */
		sz = tstate.ss_frame.f_format;
		if (sz > 15 || (sz = exframesize[sz]) < 0)
			return (EINVAL);
		frame->f_stackadj -= sz;
		frame->f_format = tstate.ss_frame.f_format;
		frame->f_vector = tstate.ss_frame.f_vector;
		bcopy((caddr_t)&tstate.ss_frame.F_u, (caddr_t)&frame->F_u, sz);
#ifdef DEBUG
		if (sigdebug & SDB_FOLLOW)
			printf("sigreturn(%d): copy in %d of frame type %d\n",
			       p->p_pid, sz, tstate.ss_frame.f_format);
#endif
	}
#ifdef FPCOPROC
	/*
	 * Finally we restore the original FP context
	 */
	if (flags & SS_FPSTATE)
		m68881_restore(&tstate.ss_fpstate);
#ifdef DEBUG
	if ((sigdebug & SDB_FPSTATE) && *(char *)&tstate.ss_fpstate)
		printf("sigreturn(%d): copied in FP state (%x) at %x\n",
		       p->p_pid, *(u_int *)&tstate.ss_fpstate,
		       &tstate.ss_fpstate);
#endif
#endif
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

int	waittime = -1;

void
boot(howto, bootstr)
	register int howto;
	char *bootstr;
{
	extern int cold;

	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(curproc->p_addr);

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
	if (howto & RB_DUMP)
		dumpsys();

 haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

#if defined(PANICWAIT) && !defined(DDB)
	if ((howto & RB_HALT) == 0 && panicstr) {
		printf("hit any key to reboot...\n");
		(void)cngetc();
		printf("\n");
	}
#endif

	/* Finally, halt/reboot the system. */
	if (howto & RB_HALT) {
		printf("System halted.  Hit any key to reboot.\n\n");
		(void)cngetc();
	}

	printf("rebooting...\n");
	DELAY(1000000);
	doboot();
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	/*
	 * XXX include the final RAM page which is not included in physmem.
	 */
	dumpsize = physmem + 1;

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Dump physical memory onto the dump device.  Called by doadump()
 * in locore.s or by boot() here in machdep.c
 */
void
dumpsys()
{
	daddr_t blkno;		/* current block to write */
				/* dump routine */
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int pg;			/* page being dumped */
	vm_offset_t maddr;	/* PA being dumped */
	int error;		/* error code from (*dump)() */

	/* Don't put dump messages in msgbuf. */
	msgbufmapped = 0;

	/* Make sure dump device is valid. */
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	if (dumplo < 0)
		return;
	dump = bdevsw[major(dumpdev)].d_dump;
	blkno = dumplo;

	printf("\ndumping to dev 0x%x, offset %d\n", dumpdev, dumplo);

	printf("dump ");
	maddr = lowram;
	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many MBs we have dumped */
		if (pg && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		pmap_enter(pmap_kernel(), (vm_offset_t)vmmap, maddr,
		    VM_PROT_READ, TRUE);

		error = (*dump)(dumpdev, blkno, vmmap, NBPG);
		switch (error) {
		case 0:
			maddr += NBPG;
			blkno += btodb(NBPG);
			break;

		case ENXIO:
			printf("device bad\n");
			return;

		case EFAULT:
			printf("device not ready\n");
			return;

		case EINVAL:
			printf("area improper\n");
			return;

		case EIO:
			printf("i/o error\n");
			return;

		case EINTR:
			printf("aborted from console\n");
			return;

		default:
			printf("error %d\n", error);
			return;
		}
	}
	printf("succeeded\n");
}

void
initcpu()
{

#ifdef MAPPEDCOPY
	/*
	 * Initialize lower bound for doing copyin/copyout using
	 * page mapping (if not already set).  We don't do this on
	 * VAC machines as it loses big time.
	 */
	if (ectype == EC_VIRT)
		mappedcopysize = -1;	/* in case it was patched */
	else
		mappedcopysize = NBPG;
#endif
	parityenable();
#ifdef USELEDS
	ledinit();
#endif
}

void
straytrap(pc, evec)
	int pc;
	u_short evec;
{
	printf("unexpected trap (vector offset %x) from %x\n",
	       evec & 0xFFF, pc);
}

/* XXX should change the interface, and make one badaddr() function */

int	*nofault;

int
badaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile short *)addr;
	nofault = (int *) 0;
	return(0);
}

int
badbaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile char *)addr;
	nofault = (int *) 0;
	return(0);
}

#ifdef PANICBUTTON
/*
 * Declare these so they can be patched.
 */
int panicbutton = 1;	/* non-zero if panic buttons are enabled */
int candbdiv = 2;	/* give em half a second (hz / candbdiv) */

void	candbtimer __P((void *));

int crashandburn;

void
candbtimer(arg)
	void *arg;
{

	crashandburn = 0;
}
#endif /* PANICBUTTON */

static int innmihand;	/* simple mutex */

/*
 * Level 7 interrupts can be caused by the keyboard or parity errors.
 */
void
nmihand(frame)
	struct frame frame;
{

	/* Prevent unwanted recursion. */
	if (innmihand)
		return;
	innmihand = 1;

	/* Check for keyboard <CRTL>+<SHIFT>+<RESET>. */
	if (kbdnmi()) {
		printf("Got a keyboard NMI");

		/*
		 * We can:
		 *
		 *	- enter DDB
		 *
		 *	- Start the crashandburn sequence
		 *
		 *	- Ignore it.
		 */
#ifdef DDB
		printf(": entering debugger\n");
		Debugger();
#else
#ifdef PANICBUTTON
		if (panicbutton) {
			if (crashandburn) {
				crashandburn = 0;
				printf(": CRASH AND BURN!\n");
				panic("forced crash");
			} else {
				/* Start the crashandburn sequence */
				printf("\n");
				crashandburn = 1;
				timeout(candbtimer, NULL, hz / candbdiv);
			}
		} else
#endif /* PANICBUTTON */
			printf(": ignoring\n");
#endif /* DDB */

		goto nmihand_out;	/* no more work to do */
	}

	if (parityerror(&frame))
		return;
	/* panic?? */
	printf("unexpected level 7 interrupt ignored\n");

 nmihand_out:
	innmihand = 0;
}

/*
 * Parity error section.  Contains magic.
 */
#define PARREG		((volatile short *)IIOV(0x5B0000))
static int gotparmem = 0;
#ifdef DEBUG
int ignorekperr = 0;	/* ignore kernel parity errors */
#endif

/*
 * Enable parity detection
 */
void
parityenable()
{
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		printf("No parity memory\n");
		return;
	}
	*PARREG = 1;
	nofault = (int *) 0;
	gotparmem = 1;
	printf("Parity detection enabled\n");
}

/*
 * Determine if level 7 interrupt was caused by a parity error
 * and deal with it if it was.  Returns 1 if it was a parity error.
 */
int
parityerror(fp)
	struct frame *fp;
{
	if (!gotparmem)
		return(0);
	*PARREG = 0;
	DELAY(10);
	*PARREG = 1;
	if (panicstr) {
		printf("parity error after panic ignored\n");
		return(1);
	}
	if (!parityerrorfind())
		printf("WARNING: transient parity error ignored\n");
	else if (USERMODE(fp->f_sr)) {
		printf("pid %d: parity error\n", curproc->p_pid);
		uprintf("sorry, pid %d killed due to memory parity error\n",
			curproc->p_pid);
		psignal(curproc, SIGKILL);
#ifdef DEBUG
	} else if (ignorekperr) {
		printf("WARNING: kernel parity error ignored\n");
#endif
	} else {
		regdump(fp, 128);
		panic("kernel parity error");
	}
	return(1);
}

/*
 * Yuk!  There has got to be a better way to do this!
 * Searching all of memory with interrupts blocked can lead to disaster.
 */
int
parityerrorfind()
{
	static label_t parcatch;
	static int looking = 0;
	volatile int pg, o, s;
	register volatile int *ip;
	register int i;
	int found;

#ifdef lint
	i = o = pg = 0; if (i) return(0);
#endif
	/*
	 * If looking is true we are searching for a known parity error
	 * and it has just occured.  All we do is return to the higher
	 * level invocation.
	 */
	if (looking)
		longjmp(&parcatch);
	s = splhigh();
	/*
	 * If setjmp returns true, the parity error we were searching
	 * for has just occured (longjmp above) at the current pg+o
	 */
	if (setjmp(&parcatch)) {
		printf("Parity error at 0x%x\n", ctob(pg)|o);
		found = 1;
		goto done;
	}
	/*
	 * If we get here, a parity error has occured for the first time
	 * and we need to find it.  We turn off any external caches and
	 * loop thru memory, testing every longword til a fault occurs and
	 * we regain control at setjmp above.  Note that because of the
	 * setjmp, pg and o need to be volatile or their values will be lost.
	 */
	looking = 1;
	ecacheoff();
	for (pg = btoc(lowram); pg < btoc(lowram)+physmem; pg++) {
		pmap_enter(pmap_kernel(), (vm_offset_t)vmmap, ctob(pg),
		    VM_PROT_READ, TRUE);
		ip = (int *)vmmap;
		for (o = 0; o < NBPG; o += sizeof(int))
			i = *ip++;
	}
	/*
	 * Getting here implies no fault was found.  Should never happen.
	 */
	printf("Couldn't locate parity error\n");
	found = 0;
done:
	looking = 0;
	pmap_remove(pmap_kernel(), (vm_offset_t)vmmap, (vm_offset_t)&vmmap[NBPG]);
	ecacheon();
	splx(s);
	return(found);
}

void
regdump(fp, sbytes)
	struct frame *fp; /* must not be register */
	int sbytes;
{
	static int doingdump = 0;
	register int i;
	int s;

	if (doingdump)
		return;
	s = splhigh();
	doingdump = 1;
	printf("pid = %d, pc = %s, ",
	       curproc ? curproc->p_pid : -1, hexstr(fp->f_pc, 8));
	printf("ps = %s, ", hexstr(fp->f_sr, 4));
	printf("sfc = %s, ", hexstr(getsfc(), 4));
	printf("dfc = %s\n", hexstr(getdfc(), 4));
	printf("Registers:\n     ");
	for (i = 0; i < 8; i++)
		printf("        %d", i);
	printf("\ndreg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(fp->f_regs[i], 8));
	printf("\nareg:");
	for (i = 0; i < 8; i++)
		printf(" %s", hexstr(fp->f_regs[i+8], 8));
	if (sbytes > 0) {
		if (fp->f_sr & PSL_S) {
			printf("\n\nKernel stack (%s):",
			       hexstr((int)(((int *)&fp)-1), 8));
			dumpmem(((int *)&fp)-1, sbytes, 0);
		} else {
			printf("\n\nUser stack (%s):", hexstr(fp->f_regs[SP], 8));
			dumpmem((int *)fp->f_regs[SP], sbytes, 1);
		}
	}
	doingdump = 0;
	splx(s);
}

#define KSADDR	((int *)((u_int)curproc->p_addr + USPACE - NBPG))

void
dumpmem(ptr, sz, ustack)
	register int *ptr;
	int sz, ustack;
{
	register int i, val;

	for (i = 0; i < sz; i++) {
		if ((i & 7) == 0)
			printf("\n%s: ", hexstr((int)ptr, 6));
		else
			printf(" ");
		if (ustack == 1) {
			if ((val = fuword(ptr++)) == -1)
				break;
		} else {
			if (ustack == 0 &&
			    (ptr < KSADDR || ptr > KSADDR+(NBPG/4-1)))
				break;
			val = *ptr++;
		}
		printf("%s", hexstr(val, 8));
	}
	printf("\n");
}

char *
hexstr(val, len)
	register int val;
	int len;
{
	static char nbuf[9];
	register int x, i;

	if (len > 8)
		return("");
	nbuf[len] = '\0';
	for (i = len-1; i >= 0; --i) {
		x = val & 0xF;
		if (x > 9)
			nbuf[i] = x - 10 + 'A';
		else
			nbuf[i] = x + '0';
		val >>= 4;
	}
	return(nbuf);
}

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 * 
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 * XXX what are the special cases for the hp300?
 * XXX why is this COMPAT_NOMID?  was something generating
 *	hp300 binaries with an a_mid of 0?  i thought that was only
 *	done on little-endian machines...  -- cgd
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
#if defined(COMPAT_NOMID) || defined(COMPAT_44)
	u_long midmag, magic;
	u_short mid;
	int error;
	struct exec *execp = epp->ep_hdr;

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	midmag = mid << 16 | magic;

	switch (midmag) {
#ifdef COMPAT_NOMID
	case (MID_ZERO << 16) | ZMAGIC:
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
#endif
#ifdef COMPAT_44
	case (MID_HP300 << 16) | ZMAGIC:
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
#endif
	default:
		error = ENOEXEC;
	}

	return error;
#else /* !(defined(COMPAT_NOMID) || defined(COMPAT_44)) */
	return ENOEXEC;
#endif
}
