/*	$NetBSD: intr.c,v 1.56.4.8 2002/12/29 19:40:27 thorpej Exp $ */

/*
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
 *	@(#)intr.c	8.3 (Berkeley) 11/11/93
 */

#include "opt_multiprocessor.h"
#include "opt_sparc_arch.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <net/netisr.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/instr.h>
#include <machine/intr.h>
#include <machine/trap.h>
#include <machine/promlib.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cpuvar.h>

#if defined(MULTIPROCESSOR) && defined(DDB)
#include <machine/db_machdep.h>
#endif

void *softnet_cookie;
#if defined(MULTIPROCESSOR)
void *xcall_cookie;
#endif

void	strayintr __P((struct clockframe *));
#ifdef DIAGNOSTIC
void	bogusintr __P((struct clockframe *));
#endif
void	softnet __P((void *));

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 * XXXSMP: We are holding the kernel lock at entry & exit.
 */
void
strayintr(fp)
	struct clockframe *fp;
{
	static int straytime, nstray;
	char bits[64];
	int timesince;

	printf("stray interrupt ipl 0x%x pc=0x%x npc=0x%x psr=%s\n",
		fp->ipl, fp->pc, fp->npc, bitmask_snprintf(fp->psr,
		       PSR_BITS, bits, sizeof(bits)));

	timesince = time.tv_sec - straytime;
	if (timesince <= 10) {
		if (++nstray > 10)
			panic("crazy interrupts");
	} else {
		straytime = time.tv_sec;
		nstray = 1;
	}
}


#ifdef DIAGNOSTIC
/*
 * Bogus interrupt for which neither hard nor soft interrupt bit in
 * the IPR was set.
 */
void
bogusintr(fp)
	struct clockframe *fp;
{
	char bits[64];

	printf("bogus interrupt ipl 0x%x pc=0x%x npc=0x%x psr=%s\n",
		fp->ipl, fp->pc, fp->npc, bitmask_snprintf(fp->psr,
		       PSR_BITS, bits, sizeof(bits)));
}
#endif /* DIAGNOSTIC */


/*
 * Process software network interrupts.
 */
void
softnet(fp)
	void *fp;
{
	int n, s;

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

	if (n == 0)
		return;

#define DONETISR(bit, fn) do {		\
	if (n & (1 << bit))		\
		fn();			\
	} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR
}

#if defined(SUN4M) || defined(SUN4D)
void	nmi_hard __P((void));
void	nmi_soft __P((struct trapframe *));

int	(*memerr_handler) __P((void));
int	(*sbuserr_handler) __P((void));
int	(*vmeerr_handler) __P((void));
int	(*moduleerr_handler) __P((void));

#if defined(MULTIPROCESSOR)
volatile int nmi_hard_wait = 0;
struct simplelock nmihard_lock = SIMPLELOCK_INITIALIZER;
#endif

void
nmi_hard()
{
	/*
	 * A level 15 hard interrupt.
	 */
	int fatal = 0;
	u_int32_t si;
	char bits[64];
	u_int afsr, afva;

	afsr = afva = 0;
	if ((*cpuinfo.get_asyncflt)(&afsr, &afva) == 0) {
		printf("Async registers (mid %d): afsr=%s; afva=0x%x%x\n",
			cpuinfo.mid,
			bitmask_snprintf(afsr, AFSR_BITS, bits, sizeof(bits)),
			(afsr & AFSR_AFA) >> AFSR_AFA_RSHIFT, afva);
	}

#if defined(MULTIPROCESSOR)
	/*
	 * Increase nmi_hard_wait.  If we aren't the master, loop while this
	 * variable is non-zero.  If we are the master, loop while this
	 * variable is less than the number of cpus.
	 */
	simple_lock(&nmihard_lock);
	nmi_hard_wait++;
	simple_unlock(&nmihard_lock);

	if (cpuinfo.master == 0) {
		while (nmi_hard_wait)
			;
		return;
	} else {
		int n = 0;

		while (nmi_hard_wait < ncpu)
			if (n++ > 100000)
				panic("nmi_hard: SMP botch.");
	}
#endif

	/*
	 * Examine pending system interrupts.
	 */
	si = *((u_int32_t *)ICR_SI_PEND);
	printf("cpu%d: NMI: system interrupts: %s\n", cpu_number(),
		bitmask_snprintf(si, SINTR_BITS, bits, sizeof(bits)));

	if ((si & SINTR_M) != 0) {
		/* ECC memory error */
		if (memerr_handler != NULL)
			fatal |= (*memerr_handler)();
	}
	if ((si & SINTR_I) != 0) {
		/* MBus/SBus async error */
		if (sbuserr_handler != NULL)
			fatal |= (*sbuserr_handler)();
	}
	if ((si & SINTR_V) != 0) {
		/* VME async error */
		if (vmeerr_handler != NULL)
			fatal |= (*vmeerr_handler)();
	}
	if ((si & SINTR_ME) != 0) {
		/* Module async error */
		if (moduleerr_handler != NULL)
			fatal |= (*moduleerr_handler)();
	}

#if defined(MULTIPROCESSOR)
	/*
	 * Tell everyone else we've finished dealing with the hard NMI.
	 */
	simple_lock(&nmihard_lock);
	nmi_hard_wait = 0;
	simple_unlock(&nmihard_lock);
#endif

	if (fatal)
		panic("nmi");
}

/*
 * Non-maskable soft interrupt level 15 handler
 */
void
nmi_soft(tf)
	struct trapframe *tf;
{

	/* XXX - Most of this is superseded by xcallintr() below */
#ifdef MULTIPROCESSOR
	switch (cpuinfo.msg.tag) {
	case XPMSG_SAVEFPU:
		savefpstate(cpuinfo.fpproc->p_md.md_fpstate);
		cpuinfo.fpproc->p_md.md_fpumid = -1;
		cpuinfo.fpproc = NULL;
		break;
	case XPMSG_PAUSECPU:
		/* XXX - assumes DDB is the only user of mp_pause_cpu() */
		cpuinfo.flags |= CPUFLG_PAUSED|CPUFLG_GOTMSG;
#if defined(DDB)
		__asm("ta 0x8b");	/* trap(T_DBPAUSE) */
#else
		while (cpuinfo.flags & CPUFLG_PAUSED) /**/;
#endif
		return;
	case XPMSG_FUNC:
	    {
		volatile struct xpmsg_func *p = &cpuinfo.msg.u.xpmsg_func;

		p->retval = (*p->func)(p->arg0, p->arg1, p->arg2, p->arg3); 
		break;
	    }
	}
	cpuinfo.flags |= CPUFLG_GOTMSG;
#endif
}

#if defined(MULTIPROCESSOR)
/*
 * Respond to an xcall() request from another CPU.
 */
static void xcallintr(void *v)
{

	switch (cpuinfo.msg.tag) {
	case XPMSG_PAUSECPU:
		/* XXX - assumes DDB is the only user of mp_pause_cpu() */
		cpuinfo.flags |= CPUFLG_PAUSED|CPUFLG_GOTMSG;
#if defined(DDB)
		__asm("ta 0x8b");	/* trap(T_DBPAUSE) */
#else
		while (cpuinfo.flags & CPUFLG_PAUSED) /**/;
#endif
		return;
	case XPMSG_FUNC:
	    {
		volatile struct xpmsg_func *p = &cpuinfo.msg.u.xpmsg_func;

		if (p->func)
			p->retval = (*p->func)(p->arg0, p->arg1, p->arg2, p->arg3); 
		break;
	    }
	}
	cpuinfo.flags |= CPUFLG_GOTMSG;
}
#endif /* MULTIPROCESSOR */
#endif /* SUN4M || SUN4D */

/*
 * Level 15 interrupts are special, and not vectored here.
 * Only `prewired' interrupts appear here; boot-time configured devices
 * are attached via intr_establish() below.
 */
struct intrhand *intrhand[15] = {
	NULL,			/*  0 = error */
	NULL,			/*  1 = software level 1 + Sbus */
	NULL,	 		/*  2 = Sbus level 2 (4m: Sbus L1) */
	NULL,			/*  3 = SCSI + DMA + Sbus level 3 (4m: L2,lpt)*/
	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
	NULL,			/*  5 = Ethernet + Sbus level 4 (4m: Sbus L3) */
	NULL,			/*  6 = software level 6 (not used) (4m: enet)*/
	NULL,			/*  7 = video + Sbus level 5 */
	NULL,			/*  8 = Sbus level 6 */
	NULL,			/*  9 = Sbus level 7 */
	NULL, 			/* 10 = counter 0 = clock */
	NULL,			/* 11 = floppy */
	NULL,			/* 12 = zs hardware interrupt */
	NULL,			/* 13 = audio chip */
	NULL, 			/* 14 = counter 1 = profiling timer */
};

/*
 * Soft interrupts use a separate set of handler chains.
 * This is necessary since soft interrupt handlers do not return a value
 * and therefore cannot be mixed with hardware interrupt handlers on a
 * shared handler chain. 
 */
struct intrhand *sintrhand[15] = { NULL };

static void ih_insert(struct intrhand **head, struct intrhand *ih)
{
	struct intrhand **p, *q;
	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	for (p = head; (q = *p) != NULL; p = &q->ih_next)
		continue;
	*p = ih;
	ih->ih_next = NULL;
}

static void ih_remove(struct intrhand **head, struct intrhand *ih)
{
	struct intrhand **p, *q;

	for (p = head; (q = *p) != ih; p = &q->ih_next)
		continue;
	if (q == NULL)
		panic("intr_remove: intrhand %p fun %p arg %p",
			ih, ih->ih_fun, ih->ih_arg);

	*p = q->ih_next;
	q->ih_next = NULL;
}

static int fastvec;		/* marks fast vectors (see below) */
extern int sparc_interrupt4m[];
extern int sparc_interrupt44c[];

#ifdef DIAGNOSTIC
static void check_tv(int level)
{
	struct trapvec *tv;
	int displ;

	/* double check for legal hardware interrupt */
	tv = &trapbase[T_L1INT - 1 + level];
	displ = (CPU_ISSUN4M || CPU_ISSUN4D)
		? &sparc_interrupt4m[0] - &tv->tv_instr[1]
		: &sparc_interrupt44c[0] - &tv->tv_instr[1];

	/* has to be `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
	if (tv->tv_instr[0] != I_MOVi(I_L3, level) ||
	    tv->tv_instr[1] != I_BA(0, displ) ||
	    tv->tv_instr[2] != I_RDPSR(I_L0))
		panic("intr_establish(%d)\n0x%x 0x%x 0x%x != 0x%x 0x%x 0x%x",
		    level,
		    tv->tv_instr[0], tv->tv_instr[1], tv->tv_instr[2],
		    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
}
#endif

/*
 * Wire a fast trap vector.  Only one such fast trap is legal for any
 * interrupt, and it must be a hardware interrupt.
 */
static void
inst_fasttrap(int level, void (*vec)(void))
{
	struct trapvec *tv;
	u_long hi22, lo10;
	int s;

	if (CPU_ISSUN4 || CPU_ISSUN4C) {
		/* Can't wire to softintr slots */
		if (level == 1 || level == 4 || level == 6)
			return;
	}

#ifdef DIAGNOSTIC
	check_tv(level);
#endif

	tv = &trapbase[T_L1INT - 1 + level];
	hi22 = ((u_long)vec) >> 10;
	lo10 = ((u_long)vec) & 0x3ff;
	s = splhigh();

	/* kernel text is write protected -- let us in for a moment */
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv,
	    VM_PROT_READ|VM_PROT_WRITE, 1);
	cpuinfo.cache_flush_all();
	tv->tv_instr[0] = I_SETHI(I_L3, hi22);	/* sethi %hi(vec),%l3 */
	tv->tv_instr[1] = I_JMPLri(I_G0, I_L3, lo10);/* jmpl %l3+%lo(vec),%g0 */
	tv->tv_instr[2] = I_RDPSR(I_L0);	/* mov %psr, %l0 */
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv, VM_PROT_READ, 1);
	cpuinfo.cache_flush_all();
	fastvec |= 1 << level;
	splx(s);
}

/*
 * Uninstall a fast trap handler.
 */
static void
uninst_fasttrap(int level)
{
	struct trapvec *tv;
	int displ;	/* suspenders, belt, and buttons too */
	int s;

	tv = &trapbase[T_L1INT - 1 + level];
	s = splhigh();
	displ = (CPU_ISSUN4M || CPU_ISSUN4D)
		? &sparc_interrupt4m[0] - &tv->tv_instr[1]
		: &sparc_interrupt44c[0] - &tv->tv_instr[1];

	/* kernel text is write protected -- let us in for a moment */
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv,
	    VM_PROT_READ|VM_PROT_WRITE, 1);
	cpuinfo.cache_flush_all();
	tv->tv_instr[0] = I_MOVi(I_L3, level);
	tv->tv_instr[1] = I_BA(0, displ);
	tv->tv_instr[2] = I_RDPSR(I_L0);
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv, VM_PROT_READ, 1);
	cpuinfo.cache_flush_all();
	fastvec &= ~(1 << level);
	splx(s);
}

/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This is not possible if it has been taken away as a fast vector.
 */
void
intr_establish(level, classipl, ih, vec)
	int level;
	int classipl;
	struct intrhand *ih;
	void (*vec)(void);
{
	int s = splhigh();

#ifdef DIAGNOSTIC
	if (CPU_ISSUN4 || CPU_ISSUN4C) {
		/* Check reserved softintr slots */
		if (level == 1 || level == 4 || level == 6)
			panic("intr_establish: reserved softintr level");
	}
#endif

	/*
	 * If a `fast vector' is currently tied to this level, we must
	 * first undo that.
	 */
	if (fastvec & (1 << level)) {
		printf("intr_establish: untie fast vector at level %d\n",
		    level);
		uninst_fasttrap(level);
	} else if (vec != NULL && intrhand[level] == NULL) {
		inst_fasttrap(level, vec);
	}

	if (classipl == 0)
		classipl = level;

	/* A requested IPL cannot exceed its device class level */
	if (classipl < level)
		panic("intr_establish: class lvl (%d) < pil (%d)\n",
			classipl, level);

	/* pre-shift to PIL field in %psr */
	ih->ih_classipl = (classipl << 8) & PSR_PIL;

	ih_insert(&intrhand[level], ih);
	splx(s);
}

void
intr_disestablish(level, ih)
	int level;
	struct intrhand *ih;
{
	ih_remove(&intrhand[level], ih);
}

/*
 * This is a softintr cookie.  NB that sic_pilreq MUST be the
 * first element in the struct, because the softintr_schedule()
 * macro in intr.h casts cookies to int * to get it.  On a
 * sun4m, sic_pilreq is an actual processor interrupt level that 
 * is passed to raise(), and on a sun4 or sun4c sic_pilreq is a 
 * bit to set in the interrupt enable register with ienab_bis().
 */
struct softintr_cookie {
	int sic_pilreq;		/* CPU-specific bits; MUST be first! */
	int sic_pil;		/* Actual machine PIL that is used */
	struct intrhand sic_hand;
};

/*
 * softintr_init(): initialise the MI softintr system.
 */
void
softintr_init()
{

	softnet_cookie = softintr_establish(IPL_SOFTNET, softnet, NULL);
#if defined(MULTIPROCESSOR) && (defined(SUN4M) || defined(SUN4D))
	/* Establish a standard soft interrupt handler for cross calls */
	xcall_cookie = softintr_establish(13, xcallintr, NULL);
#endif
}

/*
 * softintr_establish(): MI interface.  establish a func(arg) as a
 * software interrupt.
 */
void *
softintr_establish(level, fun, arg)
	int level; 
	void (*fun) __P((void *));
	void *arg;
{
	struct softintr_cookie *sic;
	struct intrhand *ih;
	int pilreq;
	int pil;

	/*
	 * On a sun4m, the processor interrupt level is stored
	 * in the softintr cookie to be passed to raise().
	 *
	 * On a sun4 or sun4c the appropriate bit to set
	 * in the interrupt enable register is stored in
	 * the softintr cookie to be passed to ienab_bis().
	 */
	pil = pilreq = level;
	if (CPU_ISSUN4 || CPU_ISSUN4C) {
		/* Select the most suitable of three available softint levels */
		if (level >= 1 && level < 4) {
			pil = 1;
			pilreq = IE_L1;
		} else if (level >= 4 && level < 6) {
			pil = 4;
			pilreq = IE_L4;
		} else {
			pil = 6;
			pilreq = IE_L6;
		}
	}

	sic = malloc(sizeof(*sic), M_DEVBUF, 0);
	sic->sic_pil = pil;
	sic->sic_pilreq = pilreq;
	ih = &sic->sic_hand;
	ih->ih_fun = (int (*) __P((void *)))fun;
	ih->ih_arg = arg;

	/*
	 * Always run the handler at the requested level, which might
	 * be higher than the hardware can provide.
	 *
	 * pre-shift to PIL field in %psr
	 */
	ih->ih_classipl = (level << 8) & PSR_PIL;

	if (fastvec & (1 << pil)) {
		printf("softintr_establish: untie fast vector at level %d\n",
		    pil);
		uninst_fasttrap(level);
	}

	ih_insert(&sintrhand[pil], ih);
	return (void *)sic;
}

/*
 * softintr_disestablish(): MI interface.  disestablish the specified
 * software interrupt.
 */
void
softintr_disestablish(cookie)
	void *cookie;
{
	struct softintr_cookie *sic = cookie;

	ih_remove(&sintrhand[sic->sic_pil], &sic->sic_hand);
	free(cookie, M_DEVBUF);
}

#if 0
void
softintr_schedule(cookie)
	void *cookie;
{
	struct softintr_cookie *sic = cookie;
	if (CPU_ISSUN4M || CPU_ISSUN4D) {
#if defined(SUN4M) || defined(SUN4D)
		extern void raise(int,int);
		raise(0, sic->sic_pilreq);
#endif
	} else {
#if defined(SUN4) || defined(SUN4C)
		ienab_bis(sic->sic_pilreq);
#endif
	}
}
#endif

#ifdef MULTIPROCESSOR
/*
 * Called by interrupt stubs, etc., to lock/unlock the kernel.
 */
void
intr_lock_kernel()
{

	KERNEL_LOCK(LK_CANRECURSE|LK_EXCLUSIVE);
}

void
intr_unlock_kernel()
{

	KERNEL_UNLOCK();
}
#endif
