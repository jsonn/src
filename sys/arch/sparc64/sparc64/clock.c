/*	$NetBSD: clock.c,v 1.15.2.1 2000/11/20 20:26:50 bouyer Exp $ */

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
#include <machine/idprom.h>

#include <dev/clock_subr.h>
#include <dev/ic/mk48txxreg.h>

#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/sbusreg.h>
#include <dev/sbus/sbusvar.h>
#include <sparc64/dev/ebusreg.h>
#include <sparc64/dev/ebusvar.h>

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

static struct intrhand level10 = { clockintr };
static struct intrhand level0 = { tickintr };
static struct intrhand level14 = { statintr };

/*
 * clock (eeprom) attaches at the sbus or the ebus (PCI)
 */
static int	clockmatch_sbus __P((struct device *, struct cfdata *, void *));
static void	clockattach_sbus __P((struct device *, struct device *, void *));
static int	clockmatch_ebus __P((struct device *, struct cfdata *, void *));
static void	clockattach_ebus __P((struct device *, struct device *, void *));
static void	clockattach __P((int, bus_space_tag_t, bus_space_handle_t));


struct cfattach clock_sbus_ca = {
	sizeof(struct device), clockmatch_sbus, clockattach_sbus
};

struct cfattach clock_ebus_ca = {
	sizeof(struct device), clockmatch_ebus, clockattach_ebus
};

extern struct cfdriver clock_cd;

/* Global TOD clock handle & idprom pointer */
static todr_chip_handle_t todr_handle;
static struct idprom *idprom;

static int	timermatch __P((struct device *, struct cfdata *, void *));
static void	timerattach __P((struct device *, struct device *, void *));

struct timerreg_4u	timerreg_4u;	/* XXX - need more cleanup */

struct cfattach timer_ca = {
	sizeof(struct device), timermatch, timerattach
};

int sbus_wenable __P((struct todr_chip_handle *, int));
int ebus_wenable __P((struct todr_chip_handle *, int));
struct chiptime;
void myetheraddr __P((u_char *));
int chiptotime __P((int, int, int, int, int, int));
void timetochip __P((struct chiptime *));
void stopcounter __P((struct timer_4u *));

int timerblurb = 10; /* Guess a value; used before clock is attached */

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

/*
 * Attach a clock (really `eeprom') to the sbus or ebus.
 *
 * We ignore any existing virtual address as we need to map
 * this read-only and make it read-write only temporarily,
 * whenever we read or write the clock chip.  The clock also
 * contains the ID ``PROM'', and I have already had the pleasure
 * of reloading the cpu type, Ethernet address, etc, by hand from
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

/* Somewhere to keep info that sbus_wenable() needs */
struct sbus_info {
	bus_space_tag_t		si_bt;
	bus_space_handle_t	si_bh;
	struct sbus_reg		si_reg;
};

/* ARGSUSED */
static void
clockattach_sbus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	bus_space_tag_t bt = sa->sa_bustag;
	int sz;
	static struct sbus_info sbi;

	/* use sa->sa_regs[0].size? */
	sz = 8192;

	if (sbus_bus_map(bt,
			 sa->sa_slot,
			 (sa->sa_offset & ~NBPG),
			 sz,
			 BUS_SPACE_MAP_LINEAR|BUS_SPACE_MAP_READONLY,
			 0,
			 &sbi.si_bh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}
	clockattach(sa->sa_node, bt, sbi.si_bh);

	/* Save info for the clock wenable call. */
	sbi.si_bt = bt;
	sbi.si_reg = sa->sa_reg[0];
	todr_handle->bus_cookie = &sbi;
	todr_handle->todr_setwen = sbus_wenable;
}

/*
 * Write en/dis-able clock registers.  We coordinate so that several
 * writers can run simultaneously.
 */
int
sbus_wenable(handle, onoff)
	struct todr_chip_handle *handle;
	int onoff;
{
	register int s, err = 0;
	register int prot;/* nonzero => change prot */
	static int writers;

	s = splhigh();
	if (onoff)
		prot = writers++ == 0 ? BUS_SPACE_MAP_LINEAR : 0;
	else
		prot = --writers == 0 ? 
			BUS_SPACE_MAP_LINEAR|BUS_SPACE_MAP_READONLY : 0;
	splx(s);
	if (prot) {
		struct sbus_info *sbi = (struct sbus_info *)handle->bus_cookie;
		bus_space_handle_t newaddr;

		err = sbus_bus_map(sbi->si_bt, sbi->si_reg.sbr_slot,
			(sbi->si_reg.sbr_offset & ~NBPG),
			8192, prot, (vaddr_t)sbi->si_bh, &newaddr);
		/* We can panic now or take a datafault later... */
		if (sbi->si_bh != newaddr)
			panic("sbus_wenable: address %p changed to %p\n",
			      (vaddr_t)sbi->si_bh, (vaddr_t)newaddr);
	}
	return (err);
}


struct ebus_info {
	bus_space_tag_t		ei_bt;
	bus_space_handle_t	ei_bh;
	struct ebus_regs	ei_reg;
};

/* ARGSUSED */
static void
clockattach_ebus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ebus_attach_args *ea = aux;
	bus_space_tag_t bt = ea->ea_bustag;
	int sz;
	static struct ebus_info ebi;

	/* hard code to 8K? */
	sz = ea->ea_regs[0].size;

	if (ebus_bus_map(bt,
			 0,
			 EBUS_PADDR_FROM_REG(&ea->ea_regs[0]),
			 sz,
			 BUS_SPACE_MAP_LINEAR,
			 0,
			 &ebi.ei_bh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}
	clockattach(ea->ea_node, bt, ebi.ei_bh);

	/* Save info for the clock wenable call. */
	ebi.ei_bt = bt;
	ebi.ei_reg = ea->ea_regs[0];
	todr_handle->bus_cookie = &ebi;
	todr_handle->todr_setwen = ebus_wenable;
}

/*
 * Write en/dis-able clock registers.  We coordinate so that several
 * writers can run simultaneously.
 */
int
ebus_wenable(handle, onoff)
	struct todr_chip_handle *handle;
	int onoff;
{
	register int s, err = 0;
	register int prot;/* nonzero => change prot */
	static int writers;

	s = splhigh();
	if (onoff)
		prot = writers++ == 0 ? BUS_SPACE_MAP_LINEAR : 0;
	else
		prot = --writers == 0 ? 
			BUS_SPACE_MAP_LINEAR|BUS_SPACE_MAP_READONLY : 0;
	splx(s);
	if (prot) {
		struct ebus_info *ebi = (struct ebus_info *)handle->bus_cookie;
		bus_space_handle_t newaddr;

		err = sbus_bus_map(ebi->ei_bt, 0,
			EBUS_PADDR_FROM_REG(&ebi->ei_reg), 8192, prot,
			(vaddr_t)ebi->ei_bh, &newaddr);
		/* We can panic now or take a datafault later... */
		if (ebi->ei_bh != newaddr)
			panic("ebus_wenable: address %p changed to %p\n",
			      (vaddr_t)ebi->ei_bh, (vaddr_t)newaddr);
	}
	return (err);
}

static void
clockattach(node, bt, bh)
	int node;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
{
	char *model;
	struct idprom *idp;
	int h;

	model = getpropstring(node, "model");

#ifdef DIAGNOSTIC
	if (model == NULL)
		panic("clockattach: no model property");
#endif

	/* Our TOD clock year 0 is 1968 */
	if ((todr_handle = mk48txx_attach(bt, bh, model, 1968)) == NULL)
		panic("Can't attach %s tod clock", model);

#define IDPROM_OFFSET (8*1024 - 40)	/* XXX - get nvram sz from driver */
	idp = (struct idprom *)((u_long)bh + IDPROM_OFFSET);

	h = idp->id_machine << 24;
	h |= idp->id_hostid[0] << 16;
	h |= idp->id_hostid[1] << 8;
	h |= idp->id_hostid[2];
	hostid = h;
	printf(": hostid %lx\n", (long)hostid);

	idprom = idp;
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
 * XXX this belongs elsewhere
 */
void
myetheraddr(cp)
	u_char *cp;
{
	struct idprom *idp;

	if ((idp = idprom) == NULL) {
		int node, n;

		node = findroot();
		if (getprop(node, "idprom", sizeof *idp, &n, (void **)&idp) ||
		    n != 1) {
			printf("\nmyetheraddr: clock not setup yet, "
			       "and no idprom property in /\n");
			return;
		}
	}

	cp[0] = idp->id_ether[0];
	cp[1] = idp->id_ether[1];
	cp[2] = idp->id_ether[2];
	cp[3] = idp->id_ether[3];
	cp[4] = idp->id_ether[4];
	cp[5] = idp->id_ether[5];
	if (idprom == NULL)
		free(idp, M_DEVBUF);
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
	extern u_int64_t cpu_clockrate;
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
	if (!cpu_clockrate) 
		/* Default to 200MHz clock XXXXX */
		cpu_clockrate = 200000000;
	
	/*
	 * Calculate the starting %tick value.  We set that to the same
	 * as time, scaled for the CPU clockrate.  This gets nasty, but
	 * we can handle it.  time.tv_usec is in microseconds.  
	 * cpu_clockrate is in MHz.  
	 */
	start_time = time.tv_sec * cpu_clockrate;
	/* Now fine tune the usecs */
	start_time += cpu_clockrate / 1000000 * time.tv_usec;
	
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
			(long)(cpu_clockrate/1000000));
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
		tick_increment = cpu_clockrate / hz;
#ifdef DEBUG
		printf("Using %tick -- intr in %ld cycles...", tick_increment);
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
	 * Enable timers 
	 *
	 * Also need to map the interrupts cause we're not a child of the sbus.
	 * N.B. By default timer[0] is disabled and timer[1] is enabled.
	 */
	stxa((vaddr_t)&timerreg_4u.t_timer[0].t_limit, ASI_NUCLEUS,
	     tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC|TMR_LIM_RELOAD); 
	stxa((vaddr_t)&timerreg_4u.t_mapintr[0], ASI_NUCLEUS, 
	     timerreg_4u.t_mapintr[0]|INTMAP_V); 

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
	     timerreg_4u.t_mapintr[1]|INTMAP_V); 

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
int
clockintr(cap)
	void *cap;
{
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
	int s;

#if	NKBD	> 0
	extern int cnrom __P((void));
	extern int rom_console_input;
#endif

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

	stxa((vaddr_t)&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, 
	     tmr_ustolim(newint)|TMR_LIM_IEN|TMR_LIM_RELOAD);
	return (1);
}

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
		base = 21*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	}

	if (todr_gettime(todr_handle, (struct timeval *)&time) != 0 ||
	    time.tv_sec == 0) {
		printf("WARNING: bad date in battery clock");
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		time.tv_sec = base;
		if (!badbase)
			resettodr();
	} else {
		int deltat = time.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;
		if (waszero || deltat < 2 * SECDAY)
			return;
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

	if (todr_settime(todr_handle, (struct timeval *)&time) != 0)
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
