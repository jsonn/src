/*	$NetBSD: dec_5100.c,v 1.2.4.11 1999/06/11 00:59:59 nisimura Exp $ */

/*
 * Copyright (c) 1998 Jonathan Stone.  All rights reserved.
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
 *	This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: dec_5100.c,v 1.2.4.11 1999/06/11 00:59:59 nisimura Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/sysconf.h>

#include <mips/mips/mips_mcclock.h>	/* mcclock CPU speed estimation */
#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/pmaxtype.h>

#include <pmax/pmax/kn01.h>		/* common definitions */
#include <pmax/pmax/kn230.h>

#include <pmax/ibus/ibusvar.h>

/* XXX XXX XXX */
#define	SYS_DEV_SCC2 SERIAL2_INTR
/* XXX XXX XXX */

void dec_5100_init __P((void));
void dec_5100_bus_reset __P((void));
void dec_5100_cons_init __P((void));
void dec_5100_device_register __P((struct device *, void *));
int  dec_5100_intr __P((unsigned, unsigned, unsigned, unsigned));
void dec_5100_intr_establish __P((struct device *, void *,
		int, int (*)(void *), void *));
void dec_5100_intr_disestablish __P((struct device *, void *));
void dec_5100_memerr __P((void));

extern void prom_haltbutton __P((void));
extern void kn230_wbflush __P((void));
extern unsigned nullclkread __P((void));
extern unsigned (*clkread) __P((void));

static u_int32_t kn230imsk;
extern char cpu_model[];

int _splraise_kn230 __P((int));
int _spllower_kn230 __P((int));
int _splrestore_kn230 __P((int));

struct splsw spl_5100 = {
	{ _spllower_kn230,	0 },
	{ _splraise_kn230,	IPL_BIO },
	{ _splraise_kn230,	IPL_NET },
	{ _splraise_kn230,	IPL_TTY },
	{ _splraise_kn230,	IPL_IMP },
	{ _splraise,		MIPS_SPL_0_1_2 },
	{ _splrestore_kn230,	0 },
};

extern volatile struct chiptime *mcclock_addr;	/* XXX */

/*
 * Fill in platform struct. 
 */
void
dec_5100_init()
{
	extern void mips_set_wbflush __P((void (*)(void)));

	platform.iobus = "baseboard";

	platform.bus_reset = dec_5100_bus_reset;
	platform.cons_init = dec_5100_cons_init;
	platform.device_register = dec_5100_device_register;

	/* set correct wbflush routine for this motherboard */
	mips_set_wbflush(kn230_wbflush);

	/*
	 * Set up interrupt handling and I/O addresses.
	 */
	mips_hardware_intr = dec_5100_intr;
	mcclock_addr = (void *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);

	/* no high resolution timer circuit; possibly never called */
	clkread = nullclkread;

#ifdef NEWSPL
	__spl = &spl_5100;
#else
	splvec.splbio = MIPS_SPL1;
	splvec.splnet = MIPS_SPL1;
	splvec.spltty = MIPS_SPL_0_1;
	splvec.splimp = MIPS_SPL_0_1_2;
	splvec.splclock = MIPS_SPL_0_1_2;
	splvec.splstatclock = MIPS_SPL_0_1_2;
#endif
	mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_2);

	sprintf(cpu_model, "DECsystem 5100 (MIPSMATE)");
}

/*
 * Initalize the memory system and I/O buses.
 */
void
dec_5100_bus_reset()
{
	u_int32_t icsr;

	icsr = *(u_int32_t *)MIPS_PHYS_TO_KSEG1(KN230_SYS_ICSR);
	icsr |= KN230_CSR_INTR_WMERR;
	*(u_int32_t *)MIPS_PHYS_TO_KSEG1(KN230_SYS_ICSR);
	kn230_wbflush();
}

#include <dev/cons.h>
#include <sys/termios.h>

extern int dc_cnattach __P((paddr_t, int, int, int));

void
dec_5100_cons_init()
{
	/*
	 * Delay to allow PROM putchars to complete.
	 * FIFO depth * character time,
	 * character time = (1000000 / (defaultrate / 10))
	 */
	DELAY(160000000 / 9600);        /* XXX */

	if (dc_cnattach(0x1c000000, 0,
	    9600, (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
		panic("can't init serial console");
}

void
dec_5100_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	panic("dec_5100_device_register unimplemented");
}

struct {
	int cookie;
	int intrbit;
} kn230intrs[] = {
	{ SYS_DEV_SCC0, KN230_CSR_INTR_DZ0 },
	{ SYS_DEV_LANCE, KN230_CSR_INTR_LANCE },
	{ SYS_DEV_SCSI, KN230_CSR_INTR_SII },
	{ SYS_DEV_SCC1, KN230_CSR_INTR_OPT0 },
	{ SYS_DEV_SCC2, KN230_CSR_INTR_OPT1 },
};

void
dec_5100_intr_establish(ioa, cookie, level, func, arg)
	struct device *ioa;
	void *cookie, *arg;
	int level;
	int (*func) __P((void *));
{
	int dev, i;
	u_int32_t icsr;

	dev = (int)cookie;

	for (i = 0; i < sizeof(kn230intrs)/sizeof(kn230intrs[0]); i++) {
		if (kn230intrs[i].cookie == dev)
			goto found;
	}
	panic("ibus_intr_establish: invalid cookie %d", dev);

found:
	intrtab[dev].ih_func = func;
	intrtab[dev].ih_arg = arg;

	iplmask[level] |= kn230intrs[i].intrbit;
	kn230imsk |= kn230intrs[i].intrbit;
}

void
dec_5100_intr_disestablish(dev, cookie)
	struct device *dev;
	void *cookie;
{
	printf("dec_5100_intr_distestablish: not implemented\n");
}

/*
 * Handle mipsmate interrupts.
 */
int
dec_5100_intr(cpumask, pc, status, cause)
	unsigned cpumask;
	unsigned pc;
	unsigned status;
	unsigned cause;
{
	u_int32_t icsr;

	if (cpumask & MIPS_INT_MASK_4) {
#ifdef DDB
		Debugger();
#else
		prom_haltbutton();
#endif
	}

	if (cpumask & MIPS_INT_MASK_2) {
		struct clockframe cf;
		struct chiptime *clk;
		volatile int temp;

		clk = (void *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
		temp = clk->regc;	/* XXX clear interrupt bits */

		cf.pc = pc;
		cf.sr = status;
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;

		/* keep clock interrupts enabled when we return */
		cause &= ~MIPS_INT_MASK_2;
	}
	/* allow clock interrupt posted when enabled */
	_splset(MIPS_SR_INT_IE | (status & MIPS_INT_MASK_2));

#define	CHECKINTR(slot, bits) 					\
	if (icsr & (bits)) {					\
		intrcnt[slot] += 1;				\
		(*intrtab[slot].ih_func)(intrtab[slot].ih_arg);	\
	}

	icsr = *(u_int32_t *)MIPS_PHYS_TO_KSEG1(KN230_SYS_ICSR);
	icsr &= kn230imsk;	
	if (cpumask & (MIPS_INT_MASK_0 | MIPS_INT_MASK_1)) {
		CHECKINTR(SYS_DEV_SCC0, KN230_CSR_INTR_DZ0);
		CHECKINTR(SYS_DEV_SCC1, KN230_CSR_INTR_OPT0);
		CHECKINTR(SYS_DEV_SCC2, KN230_CSR_INTR_OPT1);
		CHECKINTR(SYS_DEV_LANCE, KN230_CSR_INTR_LANCE);
		CHECKINTR(SYS_DEV_SCSI, KN230_CSR_INTR_SII);
	}
#undef CHECKINTR

	if (cpumask & MIPS_INT_MASK_3) {
		dec_5100_memerr();
		intrcnt[ERROR_INTR]++;
	}

	return (MIPS_SR_INT_IE | (status & ~cause & MIPS_HARD_INT_MASK));
}


/*
 * Handle write-to-nonexistent-address memory errors on MIPS_INT_MASK_3.
 * These are reported asynchronously, due to hardware write buffering.
 * we can't easily figure out process context, so just panic.
 *
 * XXX drain writebuffer on contextswitch to avoid panic?
 */
void
dec_5100_memerr()
{
	u_int32_t icsr;
	extern int cold;

	/* read icsr and clear error  */
	icsr = *(u_int32_t *)MIPS_PHYS_TO_KSEG1(KN230_SYS_ICSR);
	icsr |= KN230_CSR_INTR_WMERR;
	*(u_int32_t *)MIPS_PHYS_TO_KSEG1(KN230_SYS_ICSR) = icsr;
	kn230_wbflush();
	
#ifdef DIAGNOSTIC
	printf("\nMemory interrupt\n");
#endif

	/* ignore errors during probes */
	if (cold)
		return;

	if (icsr & KN230_CSR_INTR_WMERR) {
		panic("write to non-existant memory");
	}
	else {
		panic("stray memory error interrupt");
	}
}

/*
 * spl(9) for DECsystem 5100
 */
int
_splraise_kn230(lvl)
	int lvl;
{
	oldiplmask[lvl] = kn230imsk;
	kn230imsk &= ~iplmask[lvl];
	return lvl;
}

int
_spllower_kn230(lvl)
	int lvl;
{
	oldiplmask[lvl] = kn230imsk;
	kn230imsk = iplmask[IPL_HIGH] &~ iplmask[lvl]
	return lvl;
}

int
_splrestore_kn230(lvl)
	int lvl;
{
	if (lvl > IPL_HIGH)
		_splset(MIPS_SR_INT_IE | lvl);
	else
		kn230imsk = oldiplmask[lvl];
	return lvl;
}
