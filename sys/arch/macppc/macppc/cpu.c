/*	$NetBSD: cpu.c,v 1.4.2.2 2000/11/22 16:00:39 bouyer Exp $	*/

/*-
 * Copyright (C) 1998, 1999 Internet Research Institute, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_l2cr_config.h"
#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>
#include <dev/ofw/openfirm.h>
#include <powerpc/hid.h>

#include <machine/autoconf.h>
#include <machine/bat.h>
#include <machine/pcb.h>
#include <machine/pio.h>

int cpumatch(struct device *, struct cfdata *, void *);
void cpuattach(struct device *, struct device *, void *);

void identifycpu(char *);
static void ohare_init(void);
static void config_l2cr(void);
int cpu_spinup(void);
void cpu_hatch(void);

void cpu_spinup_trampoline(void);

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};

int ncpus;

#ifdef MULTIPROCESSOR
struct cpu_info cpu_info[2];
#else
struct cpu_info cpu_info_store;
#endif

extern struct cfdriver cpu_cd;
extern int powersave;

#define HAMMERHEAD	0xf8000000
#define HH_ARBCONF	(HAMMERHEAD + 0x90)
#define HH_INTR		(HAMMERHEAD + 0xc0)

int
cpumatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;
	int *reg = ca->ca_reg;
	int q;

	if (strcmp(ca->ca_name, cpu_cd.cd_name))
		return 0;

	q = OF_finddevice("/cpus");
	if (q != -1 && q != 0) {
		for (q = OF_child(q); q != 0; q = OF_peer(q)) {
			uint32_t cpunum;
			int l;
			l = OF_getprop(q, "reg", &cpunum, sizeof(cpunum));
			if (l == 4 && reg[0] == cpunum)
				return 1;
		}
	}
	switch (reg[0]) {
	case 0:	/* master CPU */
		return 1;
	case 1:	/* secondary CPU */
		q = OF_finddevice("/hammerhead");
		if (q != -1) {
			if (in32rb(HH_ARBCONF) & 0x02)
				return 1;
			return 0;
		}
	default: /* impossible CPU */
		return 0;
	}
}

#define MPC601		1
#define MPC603		3
#define MPC604		4
#define MPC603e		6
#define MPC603ev	7
#define MPC750		8
#define MPC604ev	9
#define MPC7400		12

void
cpuattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	int id = ca->ca_reg[0];
	u_int hid0, pvr, vers;
	char model[80];

	ncpus++;
#ifdef MULTIPROCESSOR
	cpu_info[id].ci_cpuid = id;
#endif

	asm volatile ("mfpvr %0" : "=r"(pvr));
	vers = pvr >> 16;

	switch (id) {
	case 0:
		/* load my cpu_number to PIR */
		switch (vers) {
		case MPC604:
		case MPC604ev:
		case MPC7400:
			asm volatile ("mtspr 1023,%0" :: "r"(id));
		}
		identifycpu(model);
		printf(": %s, ID %d (primary)\n", model, cpu_number());
		break;
	case 1:
#ifdef MULTIPROCESSOR
		cpu_spinup();
#else
		printf(" not configured\n");
#endif
		return;
	default:
		printf(": more than 2 cpus?\n");
		panic("cpuattach");
	}

	asm ("mfspr %0,1008" : "=r"(hid0));

	/*
	 * Configure power-saving mode.
	 */
	switch (vers) {
	case MPC603:
	case MPC603e:
	case MPC603ev:
	case MPC750:
	case MPC7400:
		/* Select DOZE mode. */
		hid0 &= ~(HID0_DOZE | HID0_NAP | HID0_SLEEP);
		hid0 |= HID0_DOZE | HID0_DPM;
		powersave = 1;
		break;

	default:
		/* No power-saving mode is available. */
	}

#ifdef NAPMODE
	switch (vers) {
	case MPC750:
	case MPC7400:
		/* Select NAP mode. */
		hid0 &= ~(HID0_DOZE | HID0_NAP | HID0_SLEEP);
		hid0 |= HID0_NAP;
		break;
	}
#endif

	switch (vers) {
	case MPC750:
		hid0 &= ~HID0_DBP;		/* XXX correct? */
		hid0 |= HID0_EMCP | HID0_BTIC | HID0_SGE | HID0_BHT;
		break;
	case MPC7400:
		hid0 &= ~HID0_SPD;
		hid0 |= HID0_EMCP | HID0_BTIC | HID0_SGE | HID0_BHT;
		hid0 |= HID0_EIEC;
		break;
	}

	asm volatile ("mtspr 1008,%0" :: "r"(hid0));

#if 0
	if (1) {
		char hidbuf[128];
		bitmask_snprintf(hid0, HID0_BITMASK, hidbuf, sizeof hidbuf);
		printf("%s: HID0 %s\n", self->dv_xname, hidbuf);
	}
#endif

	/*
	 * Display cache configuration.
	 */
	if (vers == MPC750 || vers == MPC7400) {
		printf("%s", self->dv_xname);
		config_l2cr();
	} else if (OF_finddevice("/bandit/ohare") != -1) {
		printf("%s", self->dv_xname);
		ohare_init();
	}
}

struct cputab {
	int version;
	char *name;
};
static struct cputab models[] = {
	{ MPC601,   "601" },
	{ MPC603,   "603" },
	{ MPC604,   "604" },
	{ 5,	    "602" },
	{ MPC603e,  "603e" },
	{ MPC603ev, "603ev" },
	{ MPC750,   "750" },
	{ MPC604ev, "604ev" },
	{ MPC7400,  "7400" },
	{ 20,	    "620" },
	{ 0,	    NULL }
};

void
identifycpu(cpu_model)
	char *cpu_model;
{
	int pvr, vers, rev;
	struct cputab *cp = models;

	asm ("mfpvr %0" : "=r"(pvr));
	vers = pvr >> 16;
	rev = pvr & 0xffff;

	while (cp->name) {
		if (cp->version == vers)
			break;
		cp++;
	}
	if (cp->name)
		strcpy(cpu_model, cp->name);
	else
		sprintf(cpu_model, "Version %x", vers);
	sprintf(cpu_model + strlen(cpu_model), " (Revision %x)", rev);
}

#define CACHE_REG 0xf8000000

void
ohare_init()
{
	u_int *cache_reg, x;

	/* enable L2 cache */
	cache_reg = mapiodev(CACHE_REG, NBPG);
	if (((cache_reg[2] >> 24) & 0x0f) >= 3) {
		x = cache_reg[4];
		if ((x & 0x10) == 0)
                	x |= 0x04000000;
		else
                	x |= 0x04000020;

		cache_reg[4] = x;
		printf(": ohare L2 cache enabled\n");
	}
}

#define L2CR 1017

#define L2CR_L2E	0x80000000 /* 0: L2 enable */
#define L2CR_L2PE	0x40000000 /* 1: L2 data parity enable */
#define L2CR_L2SIZ	0x30000000 /* 2-3: L2 size */
#define  L2SIZ_RESERVED		0x00000000
#define  L2SIZ_256K		0x10000000
#define  L2SIZ_512K		0x20000000
#define  L2SIZ_1M	0x30000000
#define L2CR_L2CLK	0x0e000000 /* 4-6: L2 clock ratio */
#define  L2CLK_DIS		0x00000000 /* disable L2 clock */
#define  L2CLK_10		0x02000000 /* core clock / 1   */
#define  L2CLK_15		0x04000000 /*            / 1.5 */
#define  L2CLK_20		0x08000000 /*            / 2   */
#define  L2CLK_25		0x0a000000 /*            / 2.5 */
#define  L2CLK_30		0x0c000000 /*            / 3   */
#define L2CR_L2RAM	0x01800000 /* 7-8: L2 RAM type */
#define  L2RAM_FLOWTHRU_BURST	0x00000000
#define  L2RAM_PIPELINE_BURST	0x01000000
#define  L2RAM_PIPELINE_LATE	0x01800000
#define L2CR_L2DO	0x00400000 /* 9: L2 data-only.
				      Setting this bit disables instruction
				      caching. */
#define L2CR_L2I	0x00200000 /* 10: L2 global invalidate. */
#define L2CR_L2CTL	0x00100000 /* 11: L2 RAM control (ZZ enable).
				      Enables automatic operation of the
				      L2ZZ (low-power mode) signal. */
#define L2CR_L2WT	0x00080000 /* 12: L2 write-through. */
#define L2CR_L2TS	0x00040000 /* 13: L2 test support. */
#define L2CR_L2OH	0x00030000 /* 14-15: L2 output hold. */
#define L2CR_L2SL	0x00008000 /* 16: L2 DLL slow. */
#define L2CR_L2DF	0x00004000 /* 17: L2 differential clock. */
#define L2CR_L2BYP	0x00002000 /* 18: L2 DLL bypass. */
#define L2CR_L2IP	0x00000001 /* 31: L2 global invalidate in progress
				      (read only). */
#ifdef L2CR_CONFIG
u_int l2cr_config = L2CR_CONFIG;
#else
u_int l2cr_config = 0;
#endif

void
config_l2cr()
{
	u_int l2cr, x;

	__asm __volatile ("mfspr %0, 1017" : "=r"(l2cr));

	/*
	 * Configure L2 cache if not enabled.
	 */
	if ((l2cr & L2CR_L2E) == 0 && l2cr_config != 0) {
		l2cr = l2cr_config;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));

		/* Wait for L2 clock to be stable (640 L2 clocks). */
		delay(100);

		/* Invalidate all L2 contents. */
		l2cr |= L2CR_L2I;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));
		do {
			asm volatile ("mfspr %0, 1017" : "=r"(x));
		} while (x & L2CR_L2IP);

		/* Enable L2 cache. */
		l2cr &= ~L2CR_L2I;
		l2cr |= L2CR_L2E;
		asm volatile ("mtspr 1017,%0" :: "r"(l2cr));
	}

	if (l2cr & L2CR_L2E) {
		switch (l2cr & L2CR_L2SIZ) {
		case L2SIZ_256K:
			printf(": 256KB");
			break;
		case L2SIZ_512K:
			printf(": 512KB");
			break;
		case L2SIZ_1M:
			printf(": 1MB");
			break;
		default:
			printf(": unknown size");
		}
#if 0
		switch (l2cr & L2CR_L2RAM) {
		case L2RAM_FLOWTHRU_BURST:
			printf(" Flow-through synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_BURST:
			printf(" Pipelined synchronous burst SRAM");
			break;
		case L2RAM_PIPELINE_LATE:
			printf(" Pipelined synchronous late-write SRAM");
			break;
		default:
			printf(" unknown type");
		}

		if (l2cr & L2CR_L2PE)
			printf(" with parity");
#endif
		printf(" backside cache");
	} else
		printf(": L2 cache not enabled");

	printf("\n");
}

#ifdef MULTIPROCESSOR

struct cpu_hatch_data {
	int running;
	int pir;
	int hid0;
	int sdr1;
	int sr[16];
	int tbu, tbl;
};

volatile struct cpu_hatch_data *cpu_hatch_data;
volatile int cpu_hatchstack;

int
cpu_spinup()
{
	volatile struct cpu_hatch_data hatch_data, *h = &hatch_data;
	int i;
	struct pcb *pcb;
	struct pglist mlist;
	int error;

	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack
	 * from the lowest 256MB (because bat0 always maps it va == pa).
	 */
	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(USPACE, 0x0, 0x10000000, 0, 0, &mlist, 1, 1);
	if (error) {
		printf(": unable to allocate idle stack\n");
		return -1;
	}

	pcb = (void *)VM_PAGE_TO_PHYS(TAILQ_FIRST(&mlist));
	bzero(pcb, USPACE);

	/*
	 * Initialize the idle stack pointer, reserving space for an
	 * (empty) trapframe (XXX is the trapframe really necessary?)
	 */
	pcb->pcb_sp = (paddr_t)pcb + USPACE - sizeof(struct trapframe);

	cpu_hatch_data = h;
	h->running = 0;
	h->pir = 1;
	cpu_hatchstack = pcb->pcb_sp;

	/* copy special registers */
	asm volatile ("mfspr %0,1008" : "=r"(h->hid0));
	asm volatile ("mfsdr1 %0" : "=r"(h->sdr1));
	for (i = 0; i < 16; i++)
		asm ("mfsrin %0,%1" : "=r"(h->sr[i]) : "r"(i << ADDR_SR_SHFT));

	asm volatile ("sync; isync");

	/* Start secondary cpu and stop timebase. */
	out32(0xf2800000, (int)cpu_spinup_trampoline);
	out32(HH_INTR, ~0);
	out32(HH_INTR, 0);

	/* sync timebase (XXX shouldn't be zero'ed) */
	asm volatile ("mttbl %0; mttbu %0; mttbl %0" :: "r"(0));

	/*
	 * wait for secondary spin up (1.5ms @ 604/200MHz)
	 * XXX we cannot use delay() here because timebase is not running.
	 */
	for (i = 0; i < 100000; i++)
		if (h->running)
			break;

	/* Start timebase. */
	out32(0xf2800000, 0x100);
	out32(HH_INTR, ~0);
	out32(HH_INTR, 0);

	delay(100000);		/* wait for secondary printf */

	if (h->running == 0) {
		printf(": secondary cpu didn't start\n");
		return -1;
	}

	return 0;
}

void
cpu_hatch()
{
	volatile struct cpu_hatch_data *h = cpu_hatch_data;
	u_int msr;
	int i;
	char model[80];

	/* Initialize timebase. */
	asm ("mttbl %0; mttbu %0; mttbl %0" :: "r"(0));

	/* Set PIR (Processor Identification Register).  i.e. whoami */
	asm volatile ("mtspr 1023,%0" :: "r"(h->pir));

	/* Initialize MMU. */
	asm ("mtibatu 0,%0" :: "r"(0));
	asm ("mtibatu 1,%0" :: "r"(0));
	asm ("mtibatu 2,%0" :: "r"(0));
	asm ("mtibatu 3,%0" :: "r"(0));
	asm ("mtdbatu 0,%0" :: "r"(0));
	asm ("mtdbatu 1,%0" :: "r"(0));
	asm ("mtdbatu 2,%0" :: "r"(0));
	asm ("mtdbatu 3,%0" :: "r"(0));

	asm ("mtspr 1008,%0" :: "r"(h->hid0));

	asm ("mtibatl 0,%0; mtibatu 0,%1;"
	     "mtdbatl 0,%0; mtdbatu 0,%1;"
		:: "r"(battable[0].batl), "r"(battable[0].batu));

	/* XXX obio (for now) */
	asm ("mtibatl 1,%0; mtibatu 1,%1;"
	     "mtdbatl 1,%0; mtdbatu 1,%1;"
		:: "r"(battable[0xf].batl), "r"(battable[0xf].batu));

	for (i = 0; i < 16; i++)
		asm ("mtsrin %0,%1" :: "r"(h->sr[i]), "r"(i << ADDR_SR_SHFT));
	asm ("mtsdr1 %0" :: "r"(h->sdr1));

	asm volatile ("isync");

	/* Enable I/D address translations. */
	asm volatile ("mfmsr %0" : "=r"(msr));
	msr |= PSL_IR|PSL_DR|PSL_ME|PSL_RI;
	asm volatile ("mtmsr %0" :: "r"(msr));

	asm volatile ("sync; isync");
	h->running = 1;

	identifycpu(model);
	printf(": %s, ID %d\n", model, cpu_number());

	/* XXX Enter power-saving mode and never return. */
	asm volatile ("
	    1:
		sync
		mtmsr %0
		isync
		b 1b
	" :: "r"(PSL_POW));

	for (;;);
}

void
cpu_boot_secondary_processors()
{
	/* currently noop */
}
#endif /* MULTIPROCESSOR */
