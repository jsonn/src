/*	$NetBSD: clock.c,v 1.1.1.1.2.1 1998/07/30 14:03:54 eeh Exp $ */

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

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/eeprom.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>

#include <sparc64/sparc64/vaddrs.h>
#include <sparc64/sparc64/clockreg.h>
#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <sparc64/dev/sbusreg.h>
#include <sparc64/dev/sbusvar.h>
#include <sparc64/sparc64/asm.h>
#include "kbd.h"

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

#include <dev/ic/intersil7170.h>

extern struct idprom idprom;

#define intersil_command(run, interrupt) \
    (run | interrupt | INTERSIL_CMD_FREQ_32K | INTERSIL_CMD_24HR_MODE | \
     INTERSIL_CMD_NORMAL_MODE)

#define intersil_disable(CLOCK) \
    CLOCK->clk_cmd_reg = \
    intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IDISABLE)

#define intersil_enable(CLOCK) \
    CLOCK->clk_cmd_reg = \
    intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE)

#define intersil_clear(CLOCK) CLOCK->clk_intr_reg


static struct intrhand level10 = { clockintr };
static struct intrhand level14 = { statintr };

static int	clockmatch __P((struct device *, struct cfdata *, void *));
static void	clockattach __P((struct device *, struct device *, void *));

static struct clockreg *clock_map __P((bus_space_handle_t, char *));

struct cfattach clock_ca = {
	sizeof(struct device), clockmatch, clockattach
};

extern struct cfdriver clock_cd;

static int	timermatch __P((struct device *, struct cfdata *, void *));
static void	timerattach __P((struct device *, struct device *, void *));

struct timerreg_4u	timerreg_4u;	/* XXX - need more cleanup */

struct cfattach timer_ca = {
	sizeof(struct device), timermatch, timerattach
};

struct chiptime;
void clk_wenable __P((int));
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
clockmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	return (strcmp("eeprom", ma->ma_name) == 0);
}

static struct clockreg *
clock_map(bh, model)
	bus_space_handle_t bh;
	char *model;
{
	struct clockreg *cl;

	pmap_changeprot(pmap_kernel(), (vaddr_t)bh, VM_PROT_READ, 1);
	cl = (struct clockreg *)((int)bh + CLK_MK48T08_OFF);

	return (cl);
}

/* ARGSUSED */
static void
clockattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	char *model;
	int sz;
	struct clockreg *cl;
	struct idprom *idp;
	bus_space_handle_t bh;
	int h;

	model = getpropstring(sa->sa_node, "model");
#ifdef DIAGNOSTIC
	if (model == NULL)
		panic("no model");
#endif
	/*
	 * the MK48T08 is 8K; the MK48T02 is 2K
	 */
	/*
	 * the MK48T08 is 8K, and the MK48T59 is supposed to be identical to it
	 */
	sz = 8192;
	printf(": %s (eeprom)\n", model);

	/*
	 * We ignore any existing virtual address as we need to map
	 * this read-only and make it read-write only temporarily,
	 * whenever we read or write the clock chip.  The clock also
	 * contains the ID ``PROM'', and I have already had the pleasure
	 * of reloading the cpu type, Ethernet address, etc, by hand from
	 * the console FORTH interpreter.  I intend not to enjoy it again.
	 */

	/* 
	 * This is *UGLY*!  We probably have multiple mappings.  But I do
	 * know that this all fits inside an 8K page, so I'll just map in
	 * once.
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 (sa->sa_offset & ~NBPG),
			 sz,
			 BUS_SPACE_MAP_LINEAR,
			 0,
			 &bh) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}

	cl = clock_map(bh, model);
/*	cl = (struct clockreg *)bh; */
	idp = &cl->cl_idprom;

	h = idp->id_machine << 24;
	h |= idp->id_hostid[0] << 16;
	h |= idp->id_hostid[1] << 8;
	h |= idp->id_hostid[2];
	hostid = h;
	clockreg = cl;
}

/*
 * The sun4u OPENPROM calls the timer the "counter-timer".
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
	bus_space_handle_t bh;
	struct upa_reg *ur = NULL;
	int64_t *va = NULL;
	int nreg;
	volatile int64_t *cnt = NULL, *lim = NULL;
	/* XXX: must init to NULL to avoid stupid gcc -Wall warning */

	/* Get full-size register property */
	if (getpropA(ma->ma_node, "reg", sizeof(*ur),
		     &nreg, (void **)&ur) != 0) {
		printf("%s: can't map register\n", self->dv_xname);
		return;
	}
	
	if (nreg < 2) {
		printf("%s: only %d register sets\n", self->dv_xname,
		       nreg);
		return;
	}
	
	/*
	 * What we should have are 3 sets of registers that reside on
	 * different parts of sysio.  We'll use the prom mappings cause we
	 * can't get rid of them and set up appropriate pointers on the
	 * timerreg_4u structure.
	 */
	/* Get address property */
	if (getpropA(ma->ma_node, "address", sizeof(*va),
		     &nreg, (void **)&va) == 0) {
printf("timerattach: using PROM mappings\n");
		timerreg_4u.t_timer = (struct timer_4u *)(int)va[0];
		timerreg_4u.t_clrintr = (int64_t *)(int)va[1];
		timerreg_4u.t_mapintr = (int64_t *)(int)va[2];
	} else {
printf("timerattach: using new mappings\n");
		/* Map the system timer -- Not an SBUS device */
		if (bus_space_map2(ma->ma_bustag, 0,
				 ur[0].ur_paddr,
				 NBPG,
				 BUS_SPACE_MAP_LINEAR,
				 TIMERREG_VA, &bh) != 0) {
			printf("%s: can't map register\n", self->dv_xname);
			return;
		}
		
		timerreg_4u.t_timer = (struct timer_4u *)
			(TIMERREG_VA + (((int)ur[0].ur_paddr)&PGOFSET));
		timerreg_4u.t_clrintr = (int64_t *)
			(TIMERREG_VA + (((int)ur[1].ur_paddr)&PGOFSET));
		timerreg_4u.t_mapintr = (int64_t *)
			(TIMERREG_VA + (((int)ur[2].ur_paddr)&PGOFSET));
	}


#ifdef DEBUG
	printf("timerattach: timer=%x clrintr=%x mapintr=%x\n",
	       timerreg_4u.t_timer, timerreg_4u.t_clrintr, timerreg_4u.t_mapintr);
#endif  
	cnt = &(timerreg_4u.t_timer[0].t_count);
	lim = &(timerreg_4u.t_timer[0].t_limit);

	/* Install the appropriate interrupt vector here */
	level10.ih_number = ma->ma_interrupts[0];
	intr_establish(10, &level10);
	level14.ih_number = ma->ma_interrupts[1];
	intr_establish(14, &level14);

	timerok = 1;

#if 0
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
	for (timerblurb = 1; timerblurb>0; timerblurb++) {
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
	timerok = 1;
#endif

#if 0	/* Done earlier */
	/* link interrupt handlers */
	intr_establish(10, &level10);
	intr_establish(14, &level14);
#endif
}

/*
 * Write en/dis-able clock registers.  We coordinate so that several
 * writers can run simultaneously.
 */
void
clk_wenable(onoff)
	int onoff;
{
	register int s;
	register vm_prot_t prot;/* nonzero => change prot */
	static int writers;

	s = splhigh();
	if (onoff)
		prot = writers++ == 0 ? VM_PROT_READ|VM_PROT_WRITE : 0;
	else
		prot = --writers == 0 ? VM_PROT_READ : 0;
	splx(s);
	if (prot)
		pmap_changeprot(pmap_kernel(),
				(vaddr_t)clockreg & ~(NBPG-1),
				prot, 1);
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
	register struct clockreg *cl = clockreg;
	register struct idprom *idp = &cl->cl_idprom;

	cp[0] = idp->id_ether[0];
	cp[1] = idp->id_ether[1];
	cp[2] = idp->id_ether[2];
	cp[3] = idp->id_ether[3];
	cp[4] = idp->id_ether[4];
	cp[5] = idp->id_ether[5];
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
	register int statint, minint;

	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}
#ifdef INTR_DEBUG
	/* Set a 1/4s clock */
	tick = 200000;
#endif
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
#if 0
	timerreg_4u.t_timer[0].t_limit = tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC|TMR_LIM_RELOAD;
	timerreg_4u.t_mapintr[0] |= INTMAP_V; 
	timerreg_4u.t_timer[1].t_limit = tmr_ustolim(statint)|TMR_LIM_IEN|TMR_LIM_RELOAD;
	timerreg_4u.t_mapintr[1] |= INTMAP_V; 
#else
	stxa(&timerreg_4u.t_timer[0].t_limit, ASI_NUCLEUS, tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC|TMR_LIM_RELOAD); 
/*	stxa(&timerreg_4u.t_timer[0].t_limit, ASI_NUCLEUS, tmr_ustolim(tick)|TMR_LIM_PERIODIC|TMR_LIM_RELOAD); */
	stxa(&timerreg_4u.t_mapintr[0], ASI_NUCLEUS, timerreg_4u.t_mapintr[0]|INTMAP_V); 
#ifdef INTR_DEBUG
	/* Neglect to enable profile timer */
	stxa(&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, tmr_ustolim(statint)|TMR_LIM_RELOAD); 
#else
	stxa(&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, tmr_ustolim(statint)|TMR_LIM_IEN|TMR_LIM_RELOAD); 
#endif
	stxa(&timerreg_4u.t_mapintr[1], ASI_NUCLEUS, timerreg_4u.t_mapintr[1]|INTMAP_V); 
#endif
	statmin = statint - (statvar >> 1);
	
	/* Also zero out %tick which should be valid for at least 10 years */
	__asm __volatile("wrpr %%g0, 0, %%tick" : : );
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
	int s;
#if	NKBD	> 0
	extern int cnrom __P((void));
	extern int rom_console_input;
#endif
#ifdef NOTDEF_DEBUG
	static int deadman = 0;

	if (deadman++ > 100) {
		deadman = 0;
		Debugger();
	}
#endif

	/*
	 * Protect the clearing of the clock interrupt.  If we don't
	 * do this, and we're interrupted (by the zs, for example),
	 * the clock stops!
	 * XXX WHY DOES THIS HAPPEN?
	 */
	s = splhigh();

	/* read the register to clear the interrupt */
#if 0
	timerreg_4u.t_clrintr[0] = 0;
#else
	stxa(&timerreg_4u.t_clrintr[0], ASI_NUCLEUS, 0LL);
#endif

#if 0
	/* reset timer interrupt?!?!?! */
#if 0
	timerreg_4u.t_timer[0].t_limit = tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC;
#else
	stxa(&timerreg_4u.t_timer[0].t_limit, ASI_NUCLEUS, tmr_ustolim(tick)|TMR_LIM_IEN|TMR_LIM_PERIODIC); 
#endif
#endif
	splx(s);

	hardclock((struct clockframe *)cap);
#if	NKBD > 0
	if (rom_console_input && cnrom())
		setsoftint();
#endif

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

	/* read the limit register to clear the interrupt */
#ifdef NOT_DEBUG
	printf("statclock: count %x:%x, limit %x:%x\n", 
	       timerreg_4u.t_timer[1].t_count, timerreg_4u.t_timer[1].t_limit);
#endif
#ifdef NOT_DEBUG
	prom_printf("!");
#endif
#if 0
	timerreg_4u.t_clrintr[1]=0;
#else
	stxa(&timerreg_4u.t_clrintr[1], ASI_NUCLEUS, 0LL);
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

#if 0
	timerreg_4u.t_timer[1].t_limit = tmr_ustolim(newint)|TMR_LIM_IEN|TMR_LIM_RELOAD;
#else
	stxa(&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, tmr_ustolim(newint)|TMR_LIM_IEN|TMR_LIM_RELOAD);

#ifdef NOT_DEBUG
	/* Use normal clock instead */
	stathz = 0;
	stxa(&timerreg_4u.t_timer[1].t_limit, ASI_NUCLEUS, tmr_ustolim(newint)|TMR_LIM_RELOAD);
#endif
#endif
	return (1);
}

/*
 * BCD to decimal and decimal to BCD.
 */
#define	FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define	TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

#define	SECDAY		(24 * 60 * 60)
#define	SECYR		(SECDAY * 365)
/*
 * should use something like
 * #define LEAPYEAR(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
 * but it's unlikely that we'll still be around in 2100.
 */
#define	LEAPYEAR(y)	(((y) & 3) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

int
chiptotime(sec, min, hour, day, mon, year)
	register int sec, min, hour, day, mon, year;
{
	register int days, yr;

	sec = FROMBCD(sec);
	min = FROMBCD(min);
	hour = FROMBCD(hour);
	day = FROMBCD(day);
	mon = FROMBCD(mon);
	year = FROMBCD(year) + YEAR0;

	/* simple sanity checks */
	if (year < 70 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	days = 0;
	for (yr = 70; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	return (days * SECDAY + hour * 3600 + min * 60 + sec);
}

struct chiptime {
	int	sec;
	int	min;
	int	hour;
	int	wday;
	int	day;
	int	mon;
	int	year;
};

void
timetochip(c)
	register struct chiptime *c;
{
	register int t, t2, t3, now = time.tv_sec;

	/* compute the year */
	t2 = now / SECDAY;
	t3 = (t2 + 2) % 7;	/* day of week */
	c->wday = TOBCD(t3 + 1);

	t = 69;
	while (t2 >= 0) {	/* whittle off years */
		t3 = t2;
		t++;
		t2 -= LEAPYEAR(t) ? 366 : 365;
	}
	c->year = t;

	/* t3 = month + day; separate */
	t = LEAPYEAR(t);
	for (t2 = 1; t2 < 12; t2++)
		if (t3 < dayyr[t2] + (t && t2 > 1))
			break;

	/* t2 is month */
	c->mon = t2;
	c->day = t3 - dayyr[t2 - 1] + 1;
	if (t && t2 > 2)
		c->day--;

	/* the rest is easy */
	t = now % SECDAY;
	c->hour = t / 3600;
	t %= 3600;
	c->min = t / 60;
	c->sec = t % 60;

	c->sec = TOBCD(c->sec);
	c->min = TOBCD(c->min);
	c->hour = TOBCD(c->hour);
	c->day = TOBCD(c->day);
	c->mon = TOBCD(c->mon);
	c->year = TOBCD(c->year - YEAR0);
}

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(base)
	time_t base;
{
	register struct clockreg *cl = clockreg;
	int sec, min, hour, day, mon, year;
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
	clk_wenable(1);
	cl->cl_csr |= CLK_READ;		/* enable read (stop time) */
	sec = cl->cl_sec;
	min = cl->cl_min;
	hour = cl->cl_hour;
	day = cl->cl_mday;
	mon = cl->cl_month;
	year = cl->cl_year;
	cl->cl_csr &= ~CLK_READ;	/* time wears on */
	clk_wenable(0);
	time.tv_sec = chiptotime(sec, min, hour, day, mon, year);

	if (time.tv_sec == 0) {
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
	register struct clockreg *cl;
	struct chiptime c;

	if (!time.tv_sec || (cl = clockreg) == NULL)
		return;
	timetochip(&c);
	clk_wenable(1);
	cl->cl_csr |= CLK_WRITE;	/* enable write */
	cl->cl_sec = c.sec;
	cl->cl_min = c.min;
	cl->cl_hour = c.hour;
	cl->cl_wday = c.wday;
	cl->cl_mday = c.day;
	cl->cl_month = c.mon;
	cl->cl_year = c.year;
	cl->cl_csr &= ~CLK_WRITE;	/* load them up */
	clk_wenable(0);
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

