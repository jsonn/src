/*	$NetBSD: ip22.c,v 1.5.8.1 2001/11/12 21:17:30 thorpej Exp $	*/

/*
 * Copyright (c) 2001 Rafal K. Boni
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include "opt_machtypes.h"

#ifdef IP22

#include <sys/param.h>
#include <sys/proc.h> 
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/sysconf.h>
#include <machine/machtype.h>
#include <mips/locore.h>

static struct evcnt mips_int5_evcnt =
    EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "mips", "int 5 (clock)");

static u_int32_t iocwrite;	/* IOC write register: read-only */
static u_int32_t iocreset;	/* IOC reset register: read-only */

static unsigned long last_clk_intr;

static unsigned long ticks_per_hz;
static unsigned long ticks_per_usec;


void		ip22_init(void);
void 		ip22_bus_reset(void);
int 		ip22_local0_intr(void);
int		ip22_local1_intr(void);
int 		ip22_mappable_intr(void *);
void 		ip22_intr(u_int, u_int, u_int, u_int);
void		ip22_intr_establish(int, int, int (*)(void *), void *);

unsigned long 	ip22_clkread(void);
unsigned long	ip22_cal_timer(u_int32_t, u_int32_t);

void 
ip22_init(void)
{
	int i;
	u_int32_t sysid;
	u_int32_t int23addr;
	unsigned long cps;
	unsigned long ctrdiff[3];

	mach_type = MACH_SGI_IP22;

	/* enable watchdog timer, clear it */
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa00004) |= 0x100;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa00014) = 0;

	sysid = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fbd9858);

	if (sysid & 1)
	    mach_subtype = MACH_SGI_IP22_FULLHOUSE;
	else
	    mach_subtype = MACH_SGI_IP22_GUINESS;

	mach_boardrev = (sysid >> 1) & 0x0f;

	printf("IOC rev %d, machine %s, board rev %d\n", (sysid >> 5) & 0x07,
			(sysid & 1) ?  "Indigo2 (Fullhouse)" : "Indy (Guiness)",
			(sysid >> 1) & 0x0f);
	
	if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
	    int23addr = 0x1fbd9000;
	else
	    int23addr = 0x1fbd9880;

	/* Reset timer interrupts */
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x20) = 3;

	/* 
	 * Reset Parallel port, Keyboard/mouse and EISA.  Turn LED off.
	 * For Fullhouse, toggle magic GIO reset bit.
	 */
	iocreset = 0x17;
	if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
		iocreset |= 0x08;

	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fbd9870) = iocreset;
		
	/*
	 * Set the 10BaseT port to use UTP cable, set autoselect mode for
	 * the ethernet interface (AUI vs. TP), set the two serial ports
	 * to PC mode.
	 */
	iocwrite = 0x3a;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fbd9878) = iocwrite;

	/* Clean out interrupt masks */
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x04) = 0x00;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x0c) = 0x00;

	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x14) = 0x00;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x18) = 0x00;

	/* Set the general control registers for Guiness */
	if (mach_subtype == MACH_SGI_IP22_GUINESS) {
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fbd9848) = 0xff;
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fbd984c) = 0xff;
	}

	platform.iointr = ip22_intr;
	platform.bus_reset = ip22_bus_reset;
	platform.intr_establish = ip22_intr_establish;

	biomask = 0x0700;
	netmask = 0x0700;
	ttymask = 0x0f00;
	clockmask = 0xbf00;

	/* Hardcode interrupts 7, 11 to mappable interrupt 0,1 handlers */
	intrtab[7].ih_fun = ip22_mappable_intr;
	intrtab[7].ih_arg	= (void*) 0;

	intrtab[11].ih_fun = ip22_mappable_intr;
	intrtab[11].ih_arg	= (void*) 1;

	/* Prime cache */
	ip22_cal_timer(int23addr + 0x3c, int23addr + 0x38);

	cps = 0;
	for(i = 0; i < sizeof(ctrdiff) / sizeof(ctrdiff[0]); i++) {
	    do {
		ctrdiff[i] = ip22_cal_timer(int23addr + 0x3c, int23addr + 0x38);
	    } while (ctrdiff[i] == 0);

	    cps += ctrdiff[i];
	}

	cps = cps / (sizeof(ctrdiff) / sizeof(ctrdiff[0]));

	printf("Timer calibration, got %lu cycles (%lu, %lu, %lu)\n", cps, 
				ctrdiff[0], ctrdiff[1], ctrdiff[2]);
	printf("CPU clock speed = %lu.%02luMhz\n", cps / (1000000 / hz), 
						(cps % (1000000 / hz) / 100));

	platform.clkread = ip22_clkread;

	ticks_per_hz = cps;
	ticks_per_usec = cps * hz / 1000000;

	evcnt_attach_static(&mips_int5_evcnt);
}

void 	
ip22_bus_reset(void)
{
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000ec) = 0;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000fc) = 0;
}

void
ip22_intr(status, cause, pc, ipending)
	u_int32_t status;
	u_int32_t cause;
	u_int32_t pc;
	u_int32_t ipending;
{
	struct clockframe cf;

	/* Tickle Indy/I2 MC watchdog timer */ 
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa00014) = 0;

	if (ipending & MIPS_INT_MASK_5) {
		last_clk_intr = mips3_cp0_count_read();
		mips3_cp0_compare_write(last_clk_intr + ticks_per_hz);

		cf.pc = pc;
		cf.sr = status;

		hardclock(&cf);
		mips_int5_evcnt.ev_count++;

		cause &= ~MIPS_INT_MASK_5;
	}

	if (ipending & MIPS_INT_MASK_0) {
		if (ip22_local0_intr())
		    cause &= ~MIPS_INT_MASK_0;
	}

	if (ipending & MIPS_INT_MASK_1) {
		if (ip22_local1_intr())
		    cause &= ~MIPS_INT_MASK_1;
	}

	if (ipending & MIPS_INT_MASK_4) {
		printf("IP22 bus error: cpu_stat %08x addr %08x, "
		       "gio_stat %08x addr %08x\n", 
		       *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000ec),
		       *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000e4),
		       *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000fc),
		       *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(0x1fa000f4));
		ip22_bus_reset();
		cause &= ~MIPS_INT_MASK_4;
	}

	_splset((status & ~cause & MIPS_HARD_INT_MASK) | MIPS_SR_INT_IE);
}

int 
ip22_mappable_intr(void* arg)
{
    int i;
    int ret;
    int intnum;
    u_int32_t mstat;
    u_int32_t mmask;
    u_int32_t int23addr;
    int which = (int)arg;

    if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
	int23addr = 0x1fbd9000;
    else
	int23addr = 0x1fbd9880;

    ret = 0;
    mstat = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x10);
    mmask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x14 +
								(which * 4));

    mstat &= mmask;

    for (i = 0; i < 8; i++) {
	intnum = i + 16 + (which * 8);
	if (mstat & (1 << i)) {
		if (intrtab[intnum].ih_fun != NULL)
		    ret |= (intrtab[intnum].ih_fun)(intrtab[intnum].ih_arg);
		else 
                   printf("Unexpected mappable interrupt %d\n", intnum); 
	}
    }

    return ret;
}

int
ip22_local0_intr()
{
    int i;
    int ret;
    u_int32_t l0stat;
    u_int32_t l0mask;
    u_int32_t int23addr;

    if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
	int23addr = 0x1fbd9000;
    else
	int23addr = 0x1fbd9880;

    ret = 0;
    l0stat = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x00);
    l0mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x04);

    l0stat &= l0mask;

    for (i = 0; i < 8; i++) {
	if (l0stat & (1 << i)) {
		if (intrtab[i].ih_fun != NULL)
		    ret |= (intrtab[i].ih_fun)(intrtab[i].ih_arg);
		else 
                   printf("Unexpected local0 interrupt %d\n", i); 
	}
    }

    return ret;
}

int
ip22_local1_intr()
{
    int i;
    int ret;
    u_int32_t l1stat;
    u_int32_t l1mask;
    u_int32_t int23addr;

    if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
	int23addr = 0x1fbd9000;
    else
	int23addr = 0x1fbd9880;

    l1stat = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x08);
    l1mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x0c);

    l1stat &= l1mask;

    ret = 0;
    for (i = 0; i < 8; i++) {
	if (l1stat & (1 << i)) {
		if (intrtab[8 + i].ih_fun != NULL)
		    ret |= (intrtab[8 + i].ih_fun)(intrtab[8 + i].ih_arg);
		else 
                   printf("Unexpected local1 interrupt %x\n", 8 + i ); 
	}
    }

    return ret;
}

void	
ip22_intr_establish(level, ipl, handler, arg)
	int level;
	int ipl;
	int (*handler) __P((void *));
	void *arg;
{
	u_int32_t mask;
	u_int32_t int23addr;

	if (level < 0 || level >= NINTR)
		panic("invalid interrupt level");

	if (intrtab[level].ih_fun != NULL)
		panic("cannot share CPU interrupts");

	intrtab[level].ih_fun = handler;
	intrtab[level].ih_arg = arg;

	if (mach_subtype == MACH_SGI_IP22_FULLHOUSE)
	    int23addr = 0x1fbd9000;
	else
	    int23addr = 0x1fbd9880;

	if (level < 8) {
	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x4);
	    mask |= (1 << level);
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x4) = mask;
	} else if (level < 16) {
	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0xc);
	    mask |= (1 << (level - 8));
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0xc) = mask;
	} else if (level < 24) {
	    /* Map0 interrupt maps to l0 interrupt bit 7, so turn that on too */
	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x4);
	    mask |= (1 << 7);
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x4) = mask;

	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x14);
	    mask |= (1 << (level - 16));
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x14) = mask;
	} else {
	    /* Map1 interrupt maps to l1 interrupt bit 3, so turn that on too */
	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0xc);
	    mask |= (1 << 3);
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0xc) = mask;

	    mask = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x18);
	    mask |= (1 << (level - 24));
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(int23addr + 0x18) = mask;
	}
}

unsigned long
ip22_clkread(void)
{
	unsigned long diff =  mips3_cp0_count_read();

	diff -= last_clk_intr;
	return (diff / ticks_per_usec);
}

unsigned long
ip22_cal_timer(u_int32_t tctrl, u_int32_t tcount)
{
	int s;
	int roundtime;
	int sampletime;
	int startmsb, lsb, msb;
	unsigned long startctr, endctr;

	/* 
	 * NOTE: HZ must be greater than 15 for this to work, as otherwise
	 * we'll overflow the counter.  We round the answer to hearest 1
	 * MHz of the master (2x) clock.
	 */
	roundtime = (1000000 / hz) / 2;
	sampletime = (1000000 / hz) + 0xff;
	startmsb = (sampletime >> 8);

	s = splhigh();

	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tctrl) = 0x80 | 0x30 | 0x04;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tcount) = sampletime & 0xff;
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tcount) = sampletime >> 8;
							
	startctr = mips3_cp0_count_read();

	/* Wait for the MSB to count down to zero */
	do {
	    *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tctrl) = 0x80 | 0x00;
	    lsb = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tcount) & 0xff;
	    msb = *(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tcount) & 0xff;

	    endctr = mips3_cp0_count_read();
	} while (msb);

	/* Turn off timer */
	*(volatile u_int32_t *)MIPS_PHYS_TO_KSEG1(tctrl) = 0x80 | 0x30 | 0x08;

	splx(s);

	return (endctr - startctr) / roundtime * roundtime;
}

#endif	/* IP22 */
