/*	$NetBSD: intr.h,v 1.17.2.8 2002/12/11 16:00:52 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_INTR_H_
#define _I386_INTR_H_

#include <machine/intrdefs.h>

#ifndef _LOCORE
#include <machine/cpu.h>
#include <machine/pic.h>

/*
 * Struct describing an interrupt source for a CPU. struct cpu_info
 * has an array of MAX_INTR_SOURCES of these. The index in the array
 * is equal to the stub number of the stubcode as present in vector.s
 *
 * The primary CPU's array of interrupt sources has its first 16
 * entries reserved for legacy ISA irq handlers. This means that
 * they have a 1:1 mapping for arrayindex:irq_num. This is not
 * true for interrupts that come in through IO APICs, to find
 * their source, go through ci->ci_isources[index].is_pic
 *
 * It's possible to always maintain a 1:1 mapping, but that means
 * limiting the total number of interrupt sources to MAX_INTR_SOURCES
 * (32), instead of 32 per CPU. It also would mean that having multiple
 * IO APICs which deliver interrupts from an equal pin number would
 * overlap if they were to be sent to the same CPU.
 */

struct intrstub {
	void *ist_entry;
	void *ist_recurse; 
	void *ist_resume;
};

struct intrsource {
	int is_maxlevel;		/* max. IPL for this source */
	int is_pin;			/* IRQ for legacy; pin for IO APIC */
	struct intrhand *is_handlers;	/* handler chain */
	struct pic *is_pic;		/* originating PIC */
	void *is_recurse;		/* entry for spllower */
	void *is_resume;		/* entry for doreti */
	struct evcnt is_evcnt;		/* interrupt counter */
	char is_evname[32];		/* event counter name */
	int is_flags;			/* see below */
	int is_type;			/* level, edge */
	int is_idtvec;
	int is_minlevel;
};

#define IS_LEGACY	0x0001		/* legacy ISA irq source */
#define IS_IPI		0x0002
#define IS_LOG		0x0004


/*
 * Interrupt handler chains.  *_intr_establish() insert a handler into
 * the list.  The handler is called with its (single) argument.
 */

struct intrhand {
	int	(*ih_fun)(void *);
	void	*ih_arg;
	int	ih_level;
	struct	intrhand *ih_next;
	int	ih_pin;
	int	ih_slot;
	struct cpu_info *ih_cpu;
};

#define IMASK(ci,level) (ci)->ci_imask[(level)]
#define IUNMASK(ci,level) (ci)->ci_iunmask[(level)]

extern void Xspllower __P((int));

static __inline int splraise __P((int));
static __inline void spllower __P((int));
static __inline void softintr __P((int));

/*
 * Convert spl level to local APIC level
 */
#define APIC_LEVEL(l)   ((l) << 4)

/*
 * compiler barrier: prevent reordering of instructions.
 * XXX something similar will move to <sys/cdefs.h>
 * or thereabouts.
 * This prevents the compiler from reordering code around
 * this "instruction", acting as a sequence point for code generation.
 */

#define	__splbarrier() __asm __volatile("":::"memory")

/*
 * Add a mask to cpl, and return the old value of cpl.
 */
static __inline int
splraise(int nlevel)
{
	int olevel;
	struct cpu_info *ci = curcpu();

	olevel = ci->ci_ilevel;
	if (nlevel > olevel)
		ci->ci_ilevel = nlevel;
	__splbarrier();
	return (olevel);
}

void    cpu_Debugger(void);
void    printf(const char *, ...)
    __attribute__((__format__(__printf__,1,2)));

/*
 * Restore a value to cpl (unmasking interrupts).  If any unmasked
 * interrupts are pending, call Xspllower() to process them.
 */
static __inline void
spllower(int nlevel)
{
	struct cpu_info *ci = curcpu();

	__splbarrier();
	/*
	 * Since this should only lower the interrupt level,
	 * the XOR below should only show interrupts that
	 * are being unmasked.
	 */
	if (ci->ci_ipending & IUNMASK(ci,nlevel))
		Xspllower(nlevel);
	else
		ci->ci_ilevel = nlevel;
}

/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splstatclock()	splclock()
#define	splserial()	splraise(IPL_SERIAL)
#define splipi()	splraise(IPL_IPI)

#define spllpt()	spltty()

#define SPL_ASSERT_BELOW(x) KDASSERT(curcpu()->ci_ilevel < (x))
#define	spllpt()	spltty()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	spllowersoftclock() spllower(IPL_SOFTCLOCK)

#define	splsoftclock()	splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	splraise(IPL_SOFTNET)
#define	splsoftserial()	splraise(IPL_SOFTSERIAL)

/*
 * Miscellaneous
 */
#define	splvm()		splraise(IPL_IMP)
#define	splhigh()	splraise(IPL_HIGH)
#define	spl0()		spllower(IPL_NONE)
#define	splsched()	splraise(IPL_SCHED)
#define spllock() 	splhigh()
#define	splx(x)		spllower(x)

/*
 * Software interrupt registration
 *
 * We hand-code this to ensure that it's atomic.
 *
 * XXX always scheduled on the current CPU.
 */
static __inline void
softintr(int sir)
{
	struct cpu_info *ci = curcpu();

	__asm __volatile("lock ; orl %1, %0" :
	    "=m"(ci->ci_ipending) : "ir" (1 << sir));
}

/*
 * XXX
 */
#define	setsoftnet()	softintr(SIR_NET)

/*
 * Stub declarations.
 */

extern void Xsoftclock(void);
extern void Xsoftnet(void);
extern void Xsoftserial(void);

extern struct intrstub i8259_stubs[];
extern struct intrstub ioapic_stubs[];

struct cpu_info;

extern char idt_allocmap[];

void intr_default_setup(void);
int i386_nmi(void);
void intr_calculatemasks(struct cpu_info *);
int intr_allocate_slot_cpu(struct cpu_info *, struct pic *, int, int *);
int intr_allocate_slot(struct pic *, int, int, int, struct cpu_info **, int *,
		       int *);
void *intr_establish(int, struct pic *, int, int, int, int (*)(void *), void *);
void intr_disestablish(struct intrhand *);
void cpu_intr_init(struct cpu_info *);
int intr_find_mpmapping(int bus, int pin, int *handle);
#ifdef INTRDEBUG
void intr_printconfig(void);
#endif

#ifdef MULTIPROCESSOR
int i386_send_ipi(struct cpu_info *, int);
void i386_broadcast_ipi(int);
void i386_multicast_ipi(int, int);
void i386_ipi_handler(void);
void i386_intlock(struct intrframe);
void i386_intunlock(struct intrframe);
void i386_softintlock(void);
void i386_softintunlock(void);
#endif

#endif /* !_LOCORE */

/*
 * Generic software interrupt support.
 */

#define	I386_SOFTINTR_SOFTCLOCK		0
#define	I386_SOFTINTR_SOFTNET		1
#define	I386_SOFTINTR_SOFTSERIAL	2
#define	I386_NSOFTINTR			3

#ifndef _LOCORE
#include <sys/queue.h>

struct i386_soft_intrhand {
	TAILQ_ENTRY(i386_soft_intrhand)
		sih_q;
	struct i386_soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	*sih_arg;
	int	sih_pending;
};

struct i386_soft_intr {
	TAILQ_HEAD(, i386_soft_intrhand)
		softintr_q;
	int softintr_ssir;
	struct simplelock softintr_slock;
};

#define	i386_softintr_lock(si, s)					\
do {									\
	(s) = splhigh();						\
	simple_lock(&si->softintr_slock);				\
} while (/*CONSTCOND*/ 0)

#define	i386_softintr_unlock(si, s)					\
do {									\
	simple_unlock(&si->softintr_slock);				\
	splx((s));							\
} while (/*CONSTCOND*/ 0)

void	*softintr_establish(int, void (*)(void *), void *);
void	softintr_disestablish(void *);
void	softintr_init(void);
void	softintr_dispatch(int);

#define	softintr_schedule(arg)						\
do {									\
	struct i386_soft_intrhand *__sih = (arg);			\
	struct i386_soft_intr *__si = __sih->sih_intrhead;		\
	int __s;							\
									\
	i386_softintr_lock(__si, __s);					\
	if (__sih->sih_pending == 0) {					\
		TAILQ_INSERT_TAIL(&__si->softintr_q, __sih, sih_q);	\
		__sih->sih_pending = 1;					\
		softintr(__si->softintr_ssir);				\
	}								\
	i386_softintr_unlock(__si, __s);				\
} while (/*CONSTCOND*/ 0)
#endif /* _LOCORE */

#endif /* !_I386_INTR_H_ */
