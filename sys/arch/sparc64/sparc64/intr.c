/*	$NetBSD: intr.c,v 1.6.2.1 1998/07/30 14:03:55 eeh Exp $ */

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

#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_iso.h"
#include "opt_ns.h"
#include "opt_ccitt.h"
#include "opt_natm.h"
#include "ppp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <vm/vm.h>

#include <dev/cons.h>

#include <net/netisr.h>
#include <net/if.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/instr.h>
#include <machine/trap.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/ip_var.h>
#endif
#ifdef NS
#include <netns/ns_var.h>
#endif
#ifdef ISO
#include <netiso/iso.h>
#include <netiso/clnp.h>
#endif
#ifdef NETATALK
#include <netatalk/at_extern.h>
#endif
#include "ppp.h"
#if NPPP > 0
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

/*
 * The following array is to used by locore.s to map interrupt packets
 * to the proper IPL to send ourselves a softint.  It should be filled
 * in as the devices are probed.  We should eventually change this to a
 * vector table and call these things directly.
 */
struct intrhand *intrlev[MAXINTNUM];

void	strayintr __P((const struct trapframe *));
int	soft01intr __P((void *));

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 */
void
strayintr(fp)
	const struct trapframe *fp;
{
	static int straytime, nstray;
	int timesince;
	int swallow_zsintrs;

	/* If we're in polled mode ignore spurious interrupts */
	if (swallow_zsintrs) return;

	printf("stray interrupt ipl %x pc=%x npc=%x pstate=%b\n",
		fp->tf_pil, (int)fp->tf_pc, (int)fp->tf_npc, 
	       (unsigned int)(fp->tf_tstate>>TSTATE_PSTATE_SHIFT), PSTATE_BITS);
	timesince = time.tv_sec - straytime;
	if (timesince <= 10) {
		if (++nstray > 500)
			panic("crazy interrupts");
	} else {
		straytime = time.tv_sec;
		nstray = 1;
	}
}

/*
 * Level 1 software interrupt (could also be Sbus level 1 interrupt).
 * Three possible reasons:
 *	ROM console input needed
 *	Network software interrupt
 *	Soft clock interrupt
 */
int
soft01intr(fp)
	void *fp;
{
	extern int rom_console_input;

	if (rom_console_input && cnrom())
		cnrint();
	if (sir.sir_any) {
		/*
		 * XXX	this is bogus: should just have a list of
		 *	routines to call, a la timeouts.  Mods to
		 *	netisr are not atomic and must be protected (gah).
		 */
		if (sir.sir_which[SIR_NET]) {
			int n, s;

			s = splhigh();
			n = netisr;
			netisr = 0;
			splx(s);
			sir.sir_which[SIR_NET] = 0;
#ifdef INET
			if (n & (1 << NETISR_ARP))
				arpintr();
			if (n & (1 << NETISR_IP))
				ipintr();
#endif
#ifdef NETATALK
			if (n & (1 << NETISR_ATALK))
				atintr();
#endif
#ifdef NS
			if (n & (1 << NETISR_NS))
				nsintr();
#endif
#ifdef ISO
			if (n & (1 << NETISR_ISO))
				clnlintr();
#endif
#ifdef NATM
			if (n & (1 << NETISR_NATM))
				natmintr();
#endif
#ifdef CCITT
			if (n & (1 << NETISR_CCITT))
				ccittintr();
#endif
#if NPPP > 0
			if (n & (1 << NETISR_PPP))
				pppintr();
#endif
		}
		if (sir.sir_which[SIR_CLOCK]) {
			sir.sir_which[SIR_CLOCK] = 0;
			softclock();
		}
	}
	return (1);
}

struct intrhand level01 = { soft01intr, NULL, 1 };

/*
 * Level 15 interrupts are special, and not vectored here.
 * Only `prewired' interrupts appear here; boot-time configured devices
 * are attached via intr_establish() below.
 */
struct intrhand *intrhand[15] = {
	NULL,			/*  0 = error */
	&level01,		/*  1 = software level 1 + Sbus */
	NULL,	 		/*  2 = Sbus level 2 (4m: Sbus L1) */
	NULL,			/*  3 = SCSI + DMA + Sbus level 3 (4m: L2,lpt)*/
	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
	NULL,			/*  5 = Ethernet + Sbus level 4 (4m: Sbus L3) */
	NULL,			/*  6 = software level 6 (not used) (4m: enet)*/
	NULL,			/*  7 = video + Sbus level 5 */
	NULL,			/*  8 = Sbus level 6 */
	NULL,			/*  9 = Sbus level 7 */
	NULL,			/* 10 = counter 0 = clock */
	NULL,			/* 11 = floppy */
	NULL,			/* 12 = zs hardware interrupt */
	NULL,			/* 13 = audio chip */
	NULL			/* 14 = counter 1 = profiling timer */
};

int fastvec = 0;
#ifdef DIAGNOSTIC
extern int sparc_interrupt[];
#endif

/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This is not possible if it has been taken away as a fast vector.
 */
void
intr_establish(level, ih)
	int level;
	struct intrhand *ih;
{
	register struct intrhand **p, *q;
#ifdef DIAGNOSTIC
	register struct trapvec *tv;
	register int displ;
#endif
	int s;

	s = splhigh();
#if 0
#ifdef DIAGNOSTIC
	/* double check for legal hardware interrupt */
	if ((level != 1 && level != 4 && level != 6) || CPU_ISSUN4M ) {
		tv = &trapbase[T_L1INT - 1 + level];
		displ = &sparc_interrupt[0] - &tv->tv_instr[1];

		/* has to be `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
		if (tv->tv_instr[0] != I_MOVi(I_L3, level) ||
		    tv->tv_instr[1] != I_BA(0, displ) ||
		    tv->tv_instr[2] != I_RDPSR(I_L0))
			panic("intr_establish(%d, %p)\n%x %x %x != %x %x %x",
			    level, ih,
			    tv->tv_instr[0], tv->tv_instr[1], tv->tv_instr[2],
			    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
	}
#endif
#endif
	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	ih->ih_pil = (1<<level); /* XXXX caller should have done this before */
	ih->ih_next = NULL;
	for (p = &intrhand[level]; (q = *p) != NULL; p = &q->ih_next);
	*p = ih;
	/*
	 * Store in fast lookup table
	 */
#ifdef NOTDEF_DEBUG
	if (!ih->ih_number) {
		printf("intr_establish: NULL vector fun %p arg %p pil %p\n",
			  ih->ih_fun, ih->ih_arg, ih->ih_number, ih->ih_pil);
		Debugger();
	}
#endif
	if (ih->ih_number < MAXINTNUM || ih->ih_number <= 0) {
		if (intrlev[ih->ih_number]) 
			panic("intr_establish: intr reused %d", ih->ih_number);
		intrlev[ih->ih_number] = ih;
#ifdef NOTDEF_DEBUG
		printf("intr_establish: vector %p ipl %d clrintr %p fun %p arg %p\n",
		       ih->ih_number, ih->ih_pil, ih->ih_clr, ih->ih_fun, ih->ih_arg);
		Debugger();
#endif
	} else
		panic("intr_establish: bad intr number %d", ih->ih_number);
	splx(s);
}

/*
 * Like intr_establish, but wires a fast trap vector.  Only one such fast
 * trap is legal for any interrupt, and it must be a hardware interrupt.
 */
void
intr_fasttrap(level, vec)
	int level;
	void (*vec) __P((void));
{
	register struct trapvec *tv;
	register u_long hi22, lo10;
#ifdef DIAGNOSTIC
	register int displ;	/* suspenders, belt, and buttons too */
#endif
	int s;

	printf("trying to establish a level %d fast interrupt!", level);
	panic("intr_fasttrap");

	tv = &trapbase[T_L1INT - 1 + level];
	hi22 = ((u_long)vec) >> 10;
	lo10 = ((u_long)vec) & 0x3ff;
	s = splhigh();
	if ((fastvec & (1 << level)) != 0 || intrhand[level] != NULL)
		panic("intr_fasttrap: already handling level %d interrupts",
		    level);
#ifdef DIAGNOSTIC
	displ = &sparc_interrupt[0] - &tv->tv_instr[1];

	/* has to be `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
	if (tv->tv_instr[0] != I_MOVi(I_L3, level) ||
	    tv->tv_instr[1] != I_BA(0, displ) ||
	    tv->tv_instr[2] != I_RDPSR(I_L0))
		panic("intr_fasttrap(%d, %p)\n%x %x %x != %x %x %x",
		    level, vec,
		    tv->tv_instr[0], tv->tv_instr[1], tv->tv_instr[2],
		    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
#endif
	/* kernel text is write protected -- let us in for a moment */
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv,
	    VM_PROT_READ|VM_PROT_WRITE, 1);
	tv->tv_instr[0] = I_SETHI(I_L3, hi22);	/* sethi %hi(vec),%l3 */
	tv->tv_instr[1] = I_JMPLri(I_G0, I_L3, lo10);/* jmpl %l3+%lo(vec),%g0 */
	tv->tv_instr[2] = I_RDPSR(I_L0);	/* mov %psr, %l0 */
	pmap_changeprot(pmap_kernel(), (vaddr_t)tv, VM_PROT_READ, 1);
	fastvec |= 1 << level;
	splx(s);
}
