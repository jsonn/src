/*	$NetBSD: clock.c,v 1.59.2.3 2004/09/21 13:22:56 skrll Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Paul Kranenburg.
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
 *	@(#)clock.c	8.1 (Berkeley) 6/11/93
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.59.2.3 2004/09/21 13:22:56 skrll Exp $");

#include "opt_multiprocessor.h"

/*
 * Clock driver.  This is the id prom and eeprom driver as well
 * and includes the timer register functions too.
 */

/* Define this for a 1/4s clock to ease debugging */
/* #define INTR_DEBUG */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/eeprom.h>
#include <machine/cpu.h>

#include <dev/clock_subr.h>
#include <dev/ic/mk48txxreg.h>
#include <dev/ic/mk48txxvar.h>
#include <dev/ic/mc146818reg.h>
#include <dev/ic/mc146818var.h>

#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/sbusreg.h>
#include <dev/sbus/sbusvar.h>
#include <dev/ebus/ebusreg.h>
#include <dev/ebus/ebusvar.h>


/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 1024 would
 * give us offsets in [0..1023].  Instead, we take offsets in [1..1023].
 * This is symmetric about the point 512, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
/* XXX fix comment to match value */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */
int timerok;

static long tick_increment;
int schedintr __P((void *));

static struct intrhand level10 = { clockintr };
static struct intrhand level0 = { tickintr };
static struct intrhand level14 = { statintr };
static struct intrhand schedint = { schedintr };

/*
 * clock (eeprom) attaches at the sbus or the ebus (PCI)
 */
static int	clockmatch_sbus __P((struct device *, struct cfdata *, void *));
static void	clockattach_sbus __P((struct device *, struct device *, void *));
static int	clockmatch_ebus __P((struct device *, struct cfdata *, void *));
static void	clockattach_ebus __P((struct device *, struct device *, void *));
static int	clockmatch_rtc __P((struct device *, struct cfdata *, void *));
static void	clockattach_rtc __P((struct device *, struct device *, void *));
static void	clockattach __P((struct mk48txx_softc *, int));


CFATTACH_DECL(clock_sbus, sizeof(struct mk48txx_softc),
    clockmatch_sbus, clockattach_sbus, NULL, NULL);

CFATTACH_DECL(clock_ebus, sizeof(struct mk48txx_softc),
    clockmatch_ebus, clockattach_ebus, NULL, NULL);

CFATTACH_DECL(rtc_ebus, sizeof(struct mc146818_softc),
    clockmatch_rtc, clockattach_rtc, NULL, NULL);

extern struct cfdriver clock_cd;

/* Global TOD clock handle */
static todr_chip_handle_t todr_handle = NULL;

static int	timermatch __P((struct device *, struct cfdata *, void *));
static void	timerattach __P((struct device *, struct device *, void *));

struct timerreg_4u	timerreg_4u;	/* XXX - need more cleanup */

CFATTACH_DECL(timer, sizeof(struct device),
    timermatch, timerattach, NULL, NULL);

int clock_wenable __P((struct todr_chip_handle *, int));
struct chiptime;
int chiptotime __P((int, int, int, int, int, int));
void timetochip __P((struct chiptime *));
void stopcounter __P((struct timer_4u *));

int timerblurb = 10; /* Guess a value; used before clock is attached */

u_int rtc_read_reg(struct mc146818_softc *, u_int);
void rtc_write_reg(struct mc146818_softc *, u_int, u_int);
u_int rtc_getcent(struct mc146818_softc *);
void rtc_setcent(struct mc146818_softc *, u_int);

/*
 * The OPENPROM calls the clock the "eeprom", so we have to have our
 * own special match function to call it the "clock".
 */
static int
clockmatch_sbus(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("eeprom", sa->sa_name) == 0);
}

static int
clockmatch_ebus(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct ebus_attach_args *ea = aux;

	return (strcmp("eeprom", ea->ea_name) == 0);
}

static int
clockmatch_rtc(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct ebus_attach_args *ea = aux;

	return (strcmp("rtc", ea->ea_name) == 0);
}

/*
 * Attach a clock (really `eeprom') to the sbus or ebus.
 *
 * We ignore any existing virtual address as we need to map
 * this read-only and make it read-write only temporarily,
 * whenever we read or write the clock chip.  The clock also
 * contains the ID ``PROM'', and I have already had the pleasure
 * of reloading the CPU type, Ethernet address, etc, by hand from
 * the console FORTH interpreter.  I intend not to enjoy it again.
 *
 * the MK48T02 is 2K.  the MK48T08 is 8K, and the MK48T59 is
 * supposed to be identical to it.
 *
 * This is *UGLY*!  We probably have multiple mappings.  But I do
 * know that this all fits inside an 8K page, so I'll just map in
 * once.
 *
 * What we really need is some way to record the bus attach args
 * so we can call *_bus_map() later with BUS_SPACE_MAP_READONLY
 * or not to write enable/disable the device registers.  This is
 * a non-trivial operation.  
 */

/* ARGSUSED */
static void
clockattach_sbus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mk48txx_softc *sc = (void *)self;
	struct sbus_attach_args *sa = aux;
	int sz;

	sc->sc_bst = sa->sa_bustag;

	/* use sa->sa_regs[0].size? */
	sz = 8192;

	if (sbus_bus_map(sc->sc_bst,
			 sa->sa_slot,
			 (sa->sa_offset & ~(PAGE_SIZE - 1)),
			 sz,
			 BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_READONLY,
			 &sc->sc_bsh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}
	clockattach(sc, sa->sa_node);

	/* Save info for the clock wenable call. */
	todr_handle->todr_setwen = clock_wenable;
}

/*
 * Write en/dis-able clock registers.  We coordinate so that several
 * writers can run simultaneously.
 */
int
clock_wenable(handle, onoff)
	struct todr_chip_handle *handle;
	int onoff;
{
	struct mk48txx_softc *sc;
	vm_prot_t prot;
	vaddr_t va;
	int s, err = 0;
	static int writers;

	s = splhigh();
	if (onoff)
		prot = writers++ == 0 ? VM_PROT_READ|VM_PROT_WRITE : 0;
	else
		prot = --writers == 0 ? VM_PROT_READ : 0;
	splx(s);
	if (prot == VM_PROT_NONE) {
		return 0;
	}
	sc = handle->cookie;
	va = (vaddr_t)bus_space_vaddr(sc->sc_bst, sc->sc_bsh);
	if (va == 0UL) {
		printf("clock_wenable: WARNING -- cannot get va\n");
		return EIO;
	}
	pmap_kprotect(va, prot);
	return (err);
}


/* ARGSUSED */
static void
clockattach_ebus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mk48txx_softc *sc = (void *)self;
	struct ebus_attach_args *ea = aux;
	int sz;

	sc->sc_bst = ea->ea_bustag;

	/* hard code to 8K? */
	sz = ea->ea_reg[0].size;

	if (bus_space_map(sc->sc_bst,
			 EBUS_ADDR_FROM_REG(&ea->ea_reg[0]),
			 sz,
			 BUS_SPACE_MAP_LINEAR,
			 &sc->sc_bsh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}
	clockattach(sc, ea->ea_node);

	/* Save info for the clock wenable call. */
	todr_handle->todr_setwen = clock_wenable;
}


static void
clockattach(sc, node)
	struct mk48txx_softc *sc;
	int node;
{

	sc->sc_model = prom_getpropstring(node, "model");

#ifdef DIAGNOSTIC
	if (sc->sc_model == NULL)
		panic("clockattach: no model property");
#endif

	/* Our TOD clock year 0 is 1968 */
	sc->sc_year0 = 1968;
	mk48txx_attach(sc);

	printf("\n");

	/* XXX should be done by todr_attach() */
	todr_handle = &sc->sc_handle;
}

/*
 * `rtc' is a ds1287 on an ebus (actually an isa bus, but we use the
 * ebus driver for isa.)  So we can use ebus_wenable() but need to do
 * different attach work and use different todr routines.  It does not
 * incorporate an IDPROM.
 */

/*
 * XXX the stupid ds1287 is not mapped directly but uses an address
 * and a data reg so we cannot access the stuuupid thing w/o having
 * write access to the registers.
 *
 * XXXX We really need to mutex register access!
 */
#define	RTC_ADDR	0
#define	RTC_DATA	1
u_int
rtc_read_reg(struct mc146818_softc *sc, u_int reg)
{

	bus_space_write_1(sc->sc_bst, sc->sc_bsh, RTC_ADDR, reg);
	return (bus_space_read_1(sc->sc_bst, sc->sc_bsh, RTC_DATA));
}
void 
rtc_write_reg(struct mc146818_softc *sc, u_int reg, u_int val)
{

	bus_space_write_1(sc->sc_bst, sc->sc_bsh, RTC_ADDR, reg);
	bus_space_write_1(sc->sc_bst, sc->sc_bsh, RTC_DATA, val);
}

/* ARGSUSED */
static void
clockattach_rtc(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mc146818_softc *sc = (void *)self;
	struct ebus_attach_args *ea = aux;
	char *model;
	int sz;

	sc->sc_bst = ea->ea_bustag;

	/* hard code to 8K? */
	sz = ea->ea_reg[0].size;

	if (bus_space_map(sc->sc_bst,
			 EBUS_ADDR_FROM_REG(&ea->ea_reg[0]),
			 sz,
			 BUS_SPACE_MAP_LINEAR,
			 &sc->sc_bsh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}

	model = prom_getpropstring(ea->ea_node, "model");
#ifdef DIAGNOSTIC
	if (model == NULL)
		panic("clockattach_rtc: no model property");
#endif

	/* Our TOD clock year 0 is 0 */
	sc->sc_year0 = 0;
	sc->sc_flag = MC146818_NO_CENT_ADJUST;
	sc->sc_mcread = rtc_read_reg;
	sc->sc_mcwrite = rtc_write_reg;
	sc->sc_getcent = rtc_getcent;
	sc->sc_setcent = rtc_setcent;
	mc146818_attach(sc);

	printf(": %s\n", model);

	/*
	 * Turn interrupts off, just in case. (Although they shouldn't
	 * be wired to an interrupt controller on sparcs).
	 */
	rtc_write_reg(sc, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);

	/*
	 * Apparently on some machines the TOD registers are on the same
	 * physical page as the COM registers.  So we won't protect them.
	 */
	/*sc->sc_handle.todr_setwen = NULL;*/

	/* XXX should be done by todr_attach() */
	todr_handle = &sc->sc_handle;
}

/*
 * The sun4u OPENPROMs call the timer the "counter-timer", except for
 * the lame UltraSPARC IIi PCI machines that don't have them.
 */
static int
timermatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	return (strcmp("counter-timer", ma->ma_name) == 0);
}

static void
timerattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
	u_int *va = ma->ma_address;
#if 0
	volatile int64_t *cnt = NULL, *lim = NULL;
#endif
	
	/*
	 * What we should have are 3 sets of registers that reside on
	 * different parts of SYSIO or PSYCHO.  We'll use the prom
	 * mappings cause we can't get rid of them and set up appropriate
	 * pointers on the timerreg_4u structure.
	 */
	timerreg_4u.t_timer = (struct timer_4u *)(u_long)va[0];
	timerreg_4u.t_clrintr = (int64_t *)(u_long)va[1];
	timerreg_4u.t_mapintr = (int64_t *)(u_long)va[2];

	/* Install the appropriate interrupt vector here */
	level10.ih_number = ma->ma_interrupts[0];
	level10.ih_clr = (void*)&timerreg_4u.t_clrintr[0];
	intr_establish(10, &level10);
	level14.ih_number = ma->ma_interrupts[1];
	level14.ih_clr = (void*)&timerreg_4u.t_clrintr[1];

	intr_establish(14, &level14);
	printf(" irq vectors %lx and %lx", 
	       (u_long)level10.ih_number, 
	       (u_long)level14.ih_number);

#if 0
	cnt = &(timerreg_4u.t_timer[0].t_count);
	lim = &(timerreg_4u.t_timer[0].t_limit);

	/*
	 * Calibrate delay() by tweaking the magic constant
	 * until a delay(100) actually reads (at least) 100 us 
	 * on the clock.  Since we're using the %tick register 
	 * which should be running at exactly the CPU clock rate, it
	 * has a period of somewhere between 7ns and 3ns.
	 */

#ifdef DEBUG
	printf("Delay calibrarion....\n");
#endif
	for (timerblurb = 1; timerblurb > 0; timerblurb++) {
		volatile int discard;
		register int t0, t1;

		/* Reset counter register by writing some large limit value */
		discard = *lim;
		*lim = tmr_ustolim(TMR_MASK-1);

		t0 = *cnt;
		delay(100);
		t1 = *cnt;

		if (t1 & TMR_LIMIT)
			panic("delay calibration");

		t0 = (t0 >> TMR_SHIFT) & TMR_MASK;
		t1 = (t1 >> TMR_SHIFT) & TMR_MASK;

		if (t1 >= t0 + 100)
			break;
	}

	printf(" delay constant %d\n", timerblurb);
#endif
	printf("\n");
	timerok = 1;
}

void
stopcounter(creg)
	struct timer_4u *creg;
{
	/* Stop the clock */
	volatile int discard;
	discard = creg->t_limit;
	creg->t_limit = 0;
}

/*
 * Set up the real-time and statistics clocks.  Leave stathz 0 only if
 * no alternative timer is available.
 *
 * The frequencies of these clocks must be an even number of microseconds.
 */
void
cpu_initclocks()
{
	int statint, minint;
	static u_int64_t start_time;
#ifdef DEBUG
	extern int intrdebug;
#endif

#ifdef DEBUG
	/* Set a 1s clock */
	if (intrdebug) {
		hz = 1;
		tick = 1000000 / hz;
		printf("intrdebug set: 1Hz clock\n");
	}
#endif

	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}

	/* Make sure we have a sane cpu_clockrate -- we'll need it */
	if (!cpu_clockrate[0]) {
		/* Default to 200MHz clock XXXXX */
		cpu_clockrate[0] = 200000000;
		cpu_clockrate[1] = 200000000 / 1000000;
	}
	
	/*
	 * Calculate the starting %tick value.  We set that to the same
	 * as time, scaled for the CPU clockrate.  This gets nasty, but
	 * we can handle it.  time.tv_usec is in microseconds.  
	 * cpu_clockrate is in MHz.  
	 */
	start_time = time.tv_sec * cpu_clockrate[0];
	/* Now fine tune the usecs */
	start_time += time.tv_usec * cpu_clockrate[1];
	
	/* Initialize the %tick register */
#ifdef __arch64__
	__asm __volatile("wrpr %0, 0, %%tick" : : "r" (start_time));
#else
	{
		int start_hi = (start_time>>32), start_lo = start_time;
		__asm __volatile("sllx %1,32,%0; or %0,%2,%0; wrpr %0, 0, %%tick" 
				 : "=&r" (start_hi) /* scratch register */
				 : "r" ((int)(start_hi)), "r" ((int)(start_lo)));
	}
#endif


	/*
	 * Now handle machines w/o counter-timers.
	 */

	if (!timerreg_4u.t_timer || !timerreg_4u.t_clrintr) {

		printf("No counter-timer -- using %%tick at %ldMHz as system clock.\n",
			(long)cpu_clockrate[1]);
		/* We don't have a counter-timer -- use %tick */
		level0.ih_clr = 0;
		/* 
		 * Establish a level 10 interrupt handler 
		 *
		 * We will have a conflict with the softint handler,
		 * so we set the ih_number to 1.
		 */
		level0.ih_number = 1;
		intr_establish(10, &level0);
		/* We only have one timer so we have no statclock */
		stathz = 0;	

		/* set the next interrupt time */
		tick_increment = cpu_clockrate[0] / hz;
#ifdef DEBUG
		printf("Using %%tick -- intr in %ld cycles...", tick_increment);
#endif
		next_tick(tick_increment);
#ifdef DEBUG
		printf("done.\n");
#endif
		return;
	}

	if (stathz == 0)
		stathz = hz;
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}

	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	/* 
	 * Establish scheduler softint.
	 */
	schedint.ih_pil = PIL_SCHED;
	schedint.ih_clr = NULL;
	schedint.ih_arg = 0;
	schedint.ih_pending = 0;
	schedhz = stathz/4;

	/* 
	 * Enable timers 
	 *
	 * Also need to map the interrupts cause we're not a child of the sbus.
	 * N.B. By default timer[0] is disabled and timer[1] is enabled.
	 */
	stxa((vaddr_t)&timerreg_4u.t_timer[0].t_limit, ASI_NUCLEUS,
	     tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC|TMR_LIM_RELOAD); 
	stxa((vaddr_t)&timerreg_4u.t_mapintr[0], ASI_NUCLEUS, 
	     timerreg_4u.t_mapintr[0]|INTMAP_V|(CPU_UPAID << INTMAP_TID_SHIFT)); 

#ifdef DEBUG
	if (intrdebug)
		/* Neglect to enable timer */
		stxa((vaddr_t)&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, 
		     tmr_ustolim(statint)|TMR_LIM_RELOAD); 
	else
#endif
		stxa((vaddr_t)&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, 
		     tmr_ustolim(statint)|TMR_LIM_IEN|TMR_LIM_RELOAD); 
	stxa((vaddr_t)&timerreg_4u.t_mapintr[1], ASI_NUCLEUS, 
	     timerreg_4u.t_mapintr[1]|INTMAP_V|(CPU_UPAID << INTMAP_TID_SHIFT));

	statmin = statint - (statvar >> 1);
	
}

/*
 * Dummy setstatclockrate(), since we know profhz==hz.
 */
/* ARGSUSED */
void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing */
}

/*
 * Level 10 (clock) interrupts.  If we are using the FORTH PROM for
 * console input, we need to check for that here as well, and generate
 * a software interrupt to read it.
 */
#ifdef	DEBUG
static int clockcheck = 0;
#endif
int
clockintr(cap)
	void *cap;
{
	static int microset_iter;	/* call cc_microset once/sec */
	struct cpu_info *ci = curcpu();
#ifdef DEBUG
	static int64_t tick_base = 0;
	int64_t t = (u_int64_t)tick();

	if (!tick_base) {
		tick_base = (time.tv_sec * 1000000LL + time.tv_usec) 
			/ cpu_clockrate[1];
		tick_base -= t;
	} else if (clockcheck) {
		int64_t tk = t;
		int64_t clk = (time.tv_sec * 1000000LL + time.tv_usec);
		t -= tick_base;
		t = t / cpu_clockrate[1];
		if (t - clk > hz) {
			printf("Clock lost an interrupt!\n");
			printf("Actual: %llx Expected: %llx tick %llx tick_base %llx\n",
			       (long long)t, (long long)clk, (long long)tk, (long long)tick_base);
			tick_base = 0;
		}
	}	
#endif
	if (
#ifdef MULTIPROCESSOR
	    CPU_IS_PRIMARY(ci) &&
#endif
	    (microset_iter--) == 0) {
		microset_iter = hz - 1;
		cc_microset_time = time;
#ifdef MULTIPROCESSOR
		/* XXX broadcast IPI_MICROSET code here */
#endif
		cc_microset(ci);
	}

	/* Let locore.s clear the interrupt for us. */
	hardclock((struct clockframe *)cap);
	return (1);
}

int poll_console = 0;

/*
 * Level 10 (clock) interrupts.  If we are using the FORTH PROM for
 * console input, we need to check for that here as well, and generate
 * a software interrupt to read it.
 *
 * %tick is really a level-14 interrupt.  We need to remap this in 
 * locore.s to a level 10.
 */
int
tickintr(cap)
	void *cap;
{
	static int microset_iter;	/* call cc_microset once/sec */
	struct cpu_info *ci = curcpu();
	int s;

#if	NKBD	> 0
	extern int cnrom __P((void));
	extern int rom_console_input;
#endif

	if (
#ifdef MULTIPROCESSOR
	    CPU_IS_PRIMARY(ci) &&
#endif
	    (microset_iter--) == 0) {
		microset_iter = hz - 1;
		cc_microset_time = time;
#ifdef MULTIPROCESSOR
		/* XXX broadcast IPI_MICROSET code here */
#endif
		cc_microset(ci);
	}

	hardclock((struct clockframe *)cap);
	if (poll_console)
		setsoftint();

	s = splhigh();
	/* Reset the interrupt */
	next_tick(tick_increment);
	splx(s);

	return (1);
}

/*
 * Level 14 (stat clock) interrupt handler.
 */
int
statintr(cap)
	void *cap;
{
	register u_long newint, r, var;
	struct cpu_info *ci = curcpu();

#ifdef NOT_DEBUG
	printf("statclock: count %x:%x, limit %x:%x\n", 
	       timerreg_4u.t_timer[1].t_count, timerreg_4u.t_timer[1].t_limit);
#endif
#ifdef NOT_DEBUG
	prom_printf("!");
#endif
	statclock((struct clockframe *)cap);
#ifdef NOTDEF_DEBUG
	/* Don't re-schedule the IRQ */
	return 1;
#endif
	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	if (schedhz)
		if ((++ci->ci_schedstate.spc_schedticks & 3) == 0)
			send_softint(-1, PIL_SCHED, &schedint);
	stxa((vaddr_t)&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, 
	     tmr_ustolim(newint)|TMR_LIM_IEN|TMR_LIM_RELOAD);
	return (1);
}

int
schedintr(arg)
	void *arg;
{
	if (curlwp)
		schedclock(curlwp);
	return (1);
}


/*
 * `sparc_clock_time_is_ok' is used in cpu_reboot() to determine
 * whether it is appropriate to call resettodr() to consolidate
 * pending time adjustments.
 */
int sparc_clock_time_is_ok;

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(base)
	time_t base;
{
	int badbase = 0, waszero = base == 0;

	if (base < 5 * SECYR) {
		/*
		 * If base is 0, assume filesystem time is just unknown
		 * in stead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		base = 33*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	}

	if (todr_handle &&
		(todr_gettime(todr_handle, (struct timeval *)&time) != 0 ||
		time.tv_sec == 0)) {
		printf("WARNING: bad date in battery clock");
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		time.tv_sec = base;
		cc_microset_time = time;
		cc_microset(curcpu());
		if (!badbase)
			resettodr();
	} else {
		int deltat = time.tv_sec - base;

		cc_microset_time = time;
		cc_microset(curcpu());
		sparc_clock_time_is_ok = 1;

		if (waszero)
			return;
		if (deltat < 0) {
			deltat = -deltat;
			if (deltat < 2 * SECDAY)
				return;
		} else if (deltat < 2 * SECYR) {
			return;
		}
		printf("WARNING: clock %s %d days",
		    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock based on the current time.
 * Used when the current clock is preposterous, when the time is changed,
 * and when rebooting.  Do nothing if the time is not yet known, e.g.,
 * when crashing during autoconfig.
 */
void
resettodr()
{

	if (time.tv_sec == 0)
		return;

	cc_microset_time = time;
#ifdef MULTIPROCESSOR
	/* XXX broadcast IPI_MICROSET code here */
#endif
	cc_microset(curcpu());
	sparc_clock_time_is_ok = 1;
	if (todr_handle == 0 ||
		todr_settime(todr_handle, (struct timeval *)&time) != 0)
		printf("Cannot set time in time-of-day clock\n");
}

/*
 * XXX: these may actually belong somewhere else, but since the
 * EEPROM is so closely tied to the clock on some models, perhaps
 * it needs to stay here...
 */
int
eeprom_uio(uio)
	struct uio *uio;
{
	return (ENODEV);
}


/*
 * MD mc146818 RTC todr routines.
 */

/* Loooks like Sun stores the century info somewhere in CMOS RAM */
#define MC_CENT 0x32

u_int
rtc_getcent(sc)
	struct mc146818_softc *sc;
{

	return rtc_read_reg(sc, MC_CENT);
}

void 
rtc_setcent(sc, cent)
	struct mc146818_softc *sc;
	u_int cent;
{

	rtc_write_reg(sc, MC_CENT, cent);
}
