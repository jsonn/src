/* $NetBSD: machdep.c,v 1.2.2.1 2000/06/22 16:59:01 minoura Exp $ */

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *      This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_ipkdb.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/map.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/powerpc.h>
#include <machine/bat.h>
#include <machine/trap.h>
#include <machine/hid.h>
#include <machine/cpu.h>

#include <amiga/amiga/cc.h>
#include <amiga/amiga/cia.h>

/*
 * Global variables used here and there
 * from macppc/machdep.c
 */
struct pcb *curpcb;
struct pmap *curpm;
struct proc *fpuproc;

extern struct user *proc0paddr;

/* from amiga/machdep.c */
char cpu_model[80];
char machine[] = MACHINE;
char machine_arch[] = MACHINE_ARCH;

/* Our exported CPU info; we can have only one. */
struct cpu_info cpu_info_store;

vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;

struct bat battable[16];
extern int aga_enable, eclockfreq;

#define PPCMEMREGIONS 32
static struct mem_region PPCmem[PPCMEMREGIONS + 1], PPCavail[PPCMEMREGIONS + 3];

void show_me_regs(void);

void
initppc(startkernel, endkernel)
        u_int startkernel, endkernel;
{
	extern void cpu_fail(void);
	extern adamint, adamintsize;
	extern extint, extsize;
	extern trapcode, trapsize;
	extern alitrap, alisize;
	extern dsitrap, dsisize;
	extern isitrap, isisize;
	extern decrint, decrsize;
	extern tlbimiss, tlbimsize;
	extern tlbdlmiss, tlbdlmsize;
	extern tlbdsmiss, tlbdsmsize;
#ifdef DDB
	extern ddblow, ddbsize;
#endif
#ifdef IPKDB
	extern ipkdblow, ipkdbsize;
#endif
	int exc, scratch;

	/* force memory mapping */
	PPCmem[0].start = 0x8000000;
	PPCmem[0].size  = 0x5f80000;
	PPCmem[1].start = 0x7c00000;
	PPCmem[1].size  = 0x0400000;
	PPCmem[2].start = 0x0;
	PPCmem[2].size  = 0x0;

	PPCavail[0].start = 0x8000000;
	PPCavail[0].size  = 0x5f80000;
/*
	PPCavail[1].start = (0x7c00000 + endkernel + PGOFSET) & ~PGOFSET;
	PPCavail[1].size  = 0x8000000 - PPCavail[1].start;
*/
	PPCavail[1].start = 0x7c00000;
	PPCavail[1].size  = 0x0400000;
	PPCavail[2].start = 0x0;
	PPCavail[2].size  = 0x0;

	CHIPMEMADDR = 0x0;
	chipmem_start = 0x0;
	chipmem_end  = 0x200000;

	CIAADDR = 0xbfd000;
	CIAAbase = CIAADDR + 0x1001;
	CIABbase = CIAADDR;

	CUSTOMADDR = 0xdff000;
	CUSTOMbase = CUSTOMADDR;

	eclockfreq = 709379;

	aga_enable = 1;
	machineid = 4000 << 16;

	/* Initialize BAT tables */
	battable[0].batl = BATL(0x00000000, BAT_I|BAT_G, BAT_PP_RW);
	battable[0].batu = BATU(0x00000000, BAT_BL_16M, BAT_Vs);
	battable[1].batl = BATL(0x08000000, 0, BAT_PP_RW);
	battable[1].batu = BATU(0x08000000, BAT_BL_128M, BAT_Vs);
	battable[2].batl = BATL(0x07000000, 0, BAT_PP_RW);
	battable[2].batu = BATU(0x07000000, BAT_BL_16M, BAT_Vs);
	battable[3].batl = BATL(0xfff00000, 0, BAT_PP_RW);
	battable[3].batu = BATU(0xfff00000, BAT_BL_512K, BAT_Vs);

	/* Load BAT registers */
	asm volatile ("mtibatl 0,%0; mtibatu 0,%1;"
		"mtdbatl 0,%0; mtdbatu 0,%1" ::
		"r"(battable[0].batl), "r"(battable[0].batu));
	asm volatile ("mtibatl 1,%0; mtibatu 1,%1;"
		"mtdbatl 1,%0; mtdbatu 1,%1" ::
		"r"(battable[1].batl), "r"(battable[1].batu));
	asm volatile ("mtibatl 2,%0; mtibatu 2,%1;"
		"mtdbatl 2,%0; mtdbatu 2,%1" ::
		"r"(battable[2].batl), "r"(battable[2].batu));
	asm volatile ("mtibatl 3,%0; mtibatu 3,%1;"
		"mtdbatl 3,%0; mtdbatu 3,%1" ::
		"r"(battable[3].batl), "r"(battable[3].batu));

	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);
	curpcb = &proc0paddr->u_pcb;
	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD + EXC_UPPER; exc <= EXC_LAST + EXC_UPPER; exc += 0x100) {
		switch (exc - EXC_UPPER) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_MCHK:
			bcopy(&adamint, (void *)exc, (size_t)&adamintsize);
			break;
		case EXC_EXI:
			bcopy(&extint, (void *)exc, (size_t)&extsize);
			/*
			 * This one is (potentially) installed during autoconf
			 */
			break;
		case EXC_ALI:
			bcopy(&alitrap, (void *)exc, (size_t)&alisize);
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)exc, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)exc, (size_t)&isisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)exc, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)exc, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)exc, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)exc, (size_t)&tlbdsmsize);
			break;

#if defined(DDB) || defined(IPKDB)
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
#if defined(DDB)
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
#else
			bcopy(&ipkdblow, (void *)exc, (size_t)&ipkdbsize);
#endif
			break;
#endif /* DDB || IPKDB */
		}
	}

	/* External interrupt handler install
	*/
	__syncicache((void *)(EXC_RST + EXC_UPPER), EXC_LAST - EXC_RST + 0x100);

	/*
	 * Enable translation and interrupts
	 */
	asm volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0; isync" :
		"=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI|PSL_EE));
	custom.intreq = 0xc000;

	/*
	 * Set the page size
	 */
	uvm_setpagesize();

	/*
	 * Initialize pmap module
	 */
	pmap_bootstrap(startkernel, endkernel);
}


/* show PPC registers */
void show_me_regs()
{
	register u_long	scr0, scr1, scr2, scr3;

	asm volatile ("mfspr %0,1; mfspr %1,8; mfspr %2,9; mfspr %3,18"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("XER   %08lx\tLR    %08lx\tCTR   %08lx\tDSISR %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,19; mfspr %1,22; mfspr %2,25; mfspr %3,26"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("DAR   %08lx\tDEC   %08lx\tSDR1  %08lx\tSRR0  %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,27; mfspr %1,268; mfspr %2,269; mfspr %3,272"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("SRR1  %08lx\tTBL   %08lx\tTBU   %08lx\tSPRG0 %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,273; mfspr %1,274; mfspr %2,275; mfspr %3,282"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("SPRG1 %08lx\tSPRG2 %08lx\tSPRG3 %08lx\tEAR   %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,528; mfspr %1,529; mfspr %2,530; mfspr %3,531"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("IBAT0U%08lx\tIBAT0L%08lx\tIBAT1U%08lx\tIBAT1L%08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,532; mfspr %1,533; mfspr %2,534; mfspr %3,535" 
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("IBAT2U%08lx\tIBAT2L%08lx\tIBAT3U%08lx\tIBAT3L%08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,536; mfspr %1,537; mfspr %2,538; mfspr %3,539"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("DBAT0U%08lx\tDBAT0L%08lx\tDBAT1U%08lx\tDBAT1L%08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,540; mfspr %1,541; mfspr %2,542; mfspr %3,543"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("DBAT2U%08lx\tDBAT2L%08lx\tDBAT3U%08lx\tDBAT3L%08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,1008; mfspr %1,1009; mfspr %2,1010; mfspr %3,1013"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("HID0  %08lx\tHID1  %08lx\tIABR  %08lx\tDABR  %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,953; mfspr %1,954; mfspr %2,957; mfspr %3,958"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("PCM1  %08lx\tPCM2  %08lx\tPCM3  %08lx\tPCM4  %08lx\n",
		scr0, scr1, scr2, scr3);

	asm volatile ("mfspr %0,952; mfspr %1,956; mfspr %2,959; mfspr %3,955"
		: "=r"(scr0),"=r"(scr1),"=r"(scr2),"=r"(scr3) :);
	printf("MMCR0 %08lx\tMMCR1 %08lx\tSDA   %08lx\tSIA   %08lx\n",
		scr0, scr1, scr2, scr3);
}


paddr_t msgbuf_paddr;

/*
 * This is called during initppc, before the system is really initialized.
 * It shall provide the total and the available regions of RAM.
 * Both lists must have a zero-size entry as terminator.
 * The available regions need not take the kernel into account, but needs
 * to provide space for two additional entry beyond the terminating one.
 */
void
mem_regions(memp, availp)
	struct mem_region **memp, **availp;
{
	*memp = PPCmem;
	*availp = PPCavail;
}



/*
 * Interrupt handler
 */
void
intrhand()
{
	register unsigned short ireq;

	ireq = custom.intreqr;

	/* transmit buffer empty */
	if (ireq & INTF_TBE) {
#if NSER > 0
		ser_outintr();
#else
		custom.intreq = INTF_TBE;
#endif
	}

	/* disk block */
	if (ireq & INTF_DSKBLK) {
#if NFD > 0
		fdintr(0);
#endif
		custom.intreq = INTF_DSKBLK;
	}

	/* software */
	if (ireq & INTF_SOFTINT) {
		custom.intreq = INTF_SOFTINT;
	}

	/* ports */
	if (ireq & INTF_PORTS) {
		custom.intreq = INTF_PORTS;
	}

	/* vertical blank */
	if (ireq & INTF_VERTB) {
		vbl_handler();
	}

	/* blitter */
	if (ireq & INTF_BLIT) {
		blitter_handler();
	}

	/* copper */
	if (ireq & INTF_COPER) {
		copper_handler();
	}
}


struct isr *isr_ports;
struct isr *isr_exter;

void
add_isr(isr)
	struct isr *isr;
{
	struct isr **p, *q;

	p = isr->isr_ipl == 2 ? &isr_ports : &isr_exter;

	while ((q = *p) != NULL) {
		p = &q->isr_forw;
	}
	isr->isr_forw = NULL;
	*p = isr;
	/* enable interrupt */
	custom.intena = isr->isr_ipl == 2 ? INTF_SETCLR | INTF_PORTS :
						INTF_SETCLR | INTF_EXTER;
}

void
remove_isr(isr)
	struct isr *isr;
{
	struct isr **p, *q;

	p = isr->isr_ipl == 6 ? &isr_exter : &isr_ports;

	while ((q = *p) != NULL && q != isr) {
		p = &q->isr_forw;
	}
	if (q) {
		*p = q->isr_forw;
	}
	else {
		panic("remove_isr: handler not registered");
	}

	/* disable interrupt if no more handlers */
	p = isr->isr_ipl == 6 ? &isr_exter : &isr_ports;
	if (*p == NULL) {
		custom.intena = isr->isr_ipl == 6 ? INTF_EXTER : INTF_PORTS;
	}
}


/*
 * this is a handy package to have asynchronously executed
 * function calls executed at very low interrupt priority.
 * Example for use is keyboard repeat, where the repeat 
 * handler running at splclock() triggers such a (hardware
 * aided) software interrupt.
 * Note: the installed functions are currently called in a
 * LIFO fashion, might want to change this to FIFO
 * later.
 */
struct si_callback {
	struct si_callback *next;
	void (*function) __P((void *rock1, void *rock2));
	void *rock1, *rock2;
};
static struct si_callback *si_callbacks;
static struct si_callback *si_free;
#ifdef DIAGNOSTIC
static int ncb;		/* number of callback blocks allocated */
static int ncbd;	/* number of callback blocks dynamically allocated */
#endif

void
alloc_sicallback()
{
	struct si_callback *si;
	int s;

	si = (struct si_callback *)malloc(sizeof(*si), M_TEMP, M_NOWAIT);
	if (si == NULL)	{
		return;
	}
	s = splhigh();
	si->next = si_free;
	si_free = si;
	splx(s);
#ifdef DIAGNOSTIC
	++ncb;
#endif
}



/* should be in clock.c */
volatile int tickspending;

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
static u_long ticks_per_sec = 12500000;
static u_long ns_per_tick = 80;
static long ticks_per_intr;
static volatile u_long lasttb;

void
decr_intr(frame)
	struct clockframe *frame;
{
	u_long tb;
	long tick;
	int nticks;

	/*
	 * Check whether we are initialized
	 */
	if (!ticks_per_intr) {
		return;
	}

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */
	asm ("mftb %0; mfdec %1" : "=r"(tb), "=r"(tick));
	for (nticks = 0; tick < 0; nticks++) {
		tick += ticks_per_intr;
	}
	asm volatile ("mtdec %0" :: "r"(tick));
	/*
	 * lasttb is used during microtime. Set it to the virtual
	 * start of this tick interval.
	 */
	lasttb = tb + tick - ticks_per_intr;

	uvmexp.intrs++;
	intrcnt[CNT_CLOCK]++;
	{
	int pri, msr;

	pri = splclock();
	if (pri & (1 << SPL_CLOCK)) {
		tickspending += nticks;
	}
	else {
		nticks += tickspending;
		tickspending = 0;

		/*
		 * Reenable interrupts
		 */
		asm volatile ("mfmsr %0; ori %0, %0, %1; mtmsr %0"
				: "=r"(msr) : "K"(PSL_EE));

		/*
		 * Do standard timer interrupt stuff.
		 * Do softclock stuff only on the last iteration.
		 */
		frame->pri = pri | (1 << SIR_CLOCK);
		while (--nticks > 0) {
			hardclock(frame);
		}
		frame->pri = pri;
		hardclock(frame);
	}
	splx(pri);
	}
}


static inline u_quad_t
mftb()
{
	u_long scratch;
	u_quad_t tb;

	asm ("1: mftbu %0; mftb %0+1; mftbu %1; cmpw 0,%0,%1; bne 1b"
		: "=r"(tb), "=r"(scratch));
	return tb;
}

/*
 * Fill in *tvp with current time with microsecond resolution.
 */
void
microtime(tvp)
        struct timeval *tvp;
{
	u_long tb, ticks;
	int msr, scratch;

	asm volatile ("mfmsr %0; andi. %1,%0,%2; mtmsr %1"
			: "=r"(msr), "=r"(scratch) : "K"((u_short)~PSL_EE));
	asm ("mftb %0" : "=r"(tb));
	ticks = (tb - lasttb) * ns_per_tick;
	*tvp = time;
	asm volatile ("mtmsr %0" :: "r"(msr));
	ticks /= 1000;
	tvp->tv_usec += ticks;
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}
}

void
delay(n)
	unsigned n;
{
	u_quad_t tb;
	u_long tbh, tbl, scratch;

	tb = mftb();
	tb += (n * 1000 + ns_per_tick - 1) / ns_per_tick;
	tbh = tb >> 32;
	tbl = tb;
	asm ("1: mftbu %0; cmplw %0,%1; blt 1b; bgt 2f;"
		"mftb %0; cmplw %0,%2; blt 1b; 2:"
		:: "r"(scratch), "r"(tbh), "r"(tbl));
}

/*
int
sys_sysarch()
{
return 0;
}*/

void
identifycpu()
{
	register int pvr, hid1;
	char *mach, *pup, *cpu;

	const char *p5type_p = (const char *)0xf00010;
	int cpuclock, busclock;

	/* Amiga type */
	if (is_a4000()) {
		mach = "Amiga 4000";
	}
	else {
		mach = "Amiga 1200";
	}
	/* XXX removethis */printf("p5serial = %8s\n", p5type_p);
	switch(p5type_p[0]) {
	case 'D':
		pup = "[PowerUP]";
		break;
	case 'E':
		pup = "[CSPPC]";
		break;
	case 'F':
		pup = "[CS Mk.III]";
		break;
	case 'I':
		pup = "[BlizzardPPC]";
		break;
	default:
		pup = "";
		break;
	}

	switch(p5type_p[1]) {
	case 'A':
		cpuclock = 150;
		busclock = 60000000/4;
		break;
	case 'B':
		cpuclock = 180;
		busclock = 66000000/4;
		break;
	case 'C':
		cpuclock = 200;
		busclock = 66000000/4;
		break;
	case 'D':
		cpuclock = 233;
		busclock = 66000000/4;
		break;
	default:
		cpuclock = 0;
		break;
	}

	/* find CPU type */
	asm ("mfpvr %0" : "=r"(pvr));
	switch (pvr >> 16) {
	case 1:
		cpu = "601";
		break;
	case 3:
		cpu = "603";
		break;
	case 4:
		cpu = "604";
		break;
	case 5:
		cpu = "602";
		break;
	case 6:
		cpu = "603e";
		break;
	case 7:
		cpu = "603e+";
		break;
	case 8:
		cpu = "750";
		break;
	case 9:
		cpu = "604e";
		break;
	case 12:
		cpu = "7400";
		break;
	case 20:
		cpu = "620";
		break;
	default:
		cpu = "unknown";
		break;
	}

	snprintf(cpu_model, sizeof(cpu_model), 
	    "%s %s (%s rev.%x %d MHz, busclk %d kHz)",
	    mach, pup, cpu, pvr & 0xffff, cpuclock, busclock / 1000);
	printf("%s\n", cpu_model);
}

/*
 * Machine dependent startup code
 */
void
cpu_startup()
{
	int i, size, base, residual;
	caddr_t	v;
	vaddr_t minaddr, maxaddr;
	char pbuf[9];

	initmsgbuf((caddr_t)msgbuf_paddr, round_page(MSGBUFSIZE));

	proc0.p_addr = proc0paddr;
	v = (caddr_t)proc0paddr + USPACE;

	printf(version);
	identifycpu();

	format_bytes(pbuf, sizeof(pbuf), ctob(physmem));
	printf("total memory = %s\n", pbuf);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses
	 */
	size = (int)allocsys(NULL, NULL);
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(size))) == 0) {
		panic("startup: no room for tables");
	}
	if (allocsys(v, NULL) - v != size) {
		panic("startup: table size inconsistency");
	}

	/*
	 * Now allocate buffers proper; they are different than the above
	 * in that they usually occupy more virtual memory than physical
	 */
	size = MAXBSIZE * nbuf;
	minaddr = 0;
	if (uvm_map(kernel_map, (vaddr_t *)&minaddr, round_page(size), NULL,
		UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
		UVM_INH_NONE, UVM_ADV_NORMAL, 0)) != KERN_SUCCESS) {
		panic("startup: cannot allocate VM for buffers");
	}
	buffers = (char *)minaddr;
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* Don't want to alloc more physical mem than ever needed */
		base = MAXBSIZE;
		residual = 0;
	}
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.
		 * Of that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t)buffers + i * MAXBSIZE;
		curbufsize = NBPG * (i < residual ? base + 1 : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL) {
				panic("cpu_startup: not enough memory for "
					"buffer cache");
			}
			pmap_enter(kernel_map->pmap, curbuf,
				VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
				VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * No need to allocate an mbuf cluster submap.  Mbuf clusters
	 * are allocated via the pool allocator, and we use direct-mapped
	 * pool pages
	 */

	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * NBPG);
	printf("using %d buffers containing %s of memory\n", nbuf, pbuf);

	/*
	 * Set up the buffers, so they can be used to read disk labels
	 */
	bufinit();
}

void
cpu_dumpconf()
{
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	custom_chips_init();
	/*
	** Initialize the console before we print anything out.
	*/
	cninit();
}

/*
 * Set set up registers on exec.
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct trapframe *tf = trapframe(p);
	struct ps_strings arginfo;
	paddr_t pa;

	bzero(tf, sizeof *tf);
	tf->fixreg[1] = -roundup(-stack + 8, 16);

	/*
	 * XXX Machine-independent code has already copied arguments and
	 * XXX environment to userland.  Get them back here
	 */
	(void)copyin((char *)PS_STRINGS, &arginfo, sizeof(arginfo));

	/*
	 * Set up arguments for _start():
	 *      _start(argc, argv, envp, obj, cleanup, ps_strings);
	 * Notes:
	 *      - obj and cleanup are the auxilliary and termination
	 *        vectors.  They are fixed up by ld.elf_so.
	 *      - ps_strings is a NetBSD extention, and will be
	 *        ignored by executables which are strictly
	 *        compliant with the SVR4 ABI.
	 *
	 * XXX We have to set both regs and retval here due to different
	 * XXX calling convention in trap.c and init_main.c.
	 */
	tf->fixreg[3] = arginfo.ps_nargvstr;
	tf->fixreg[4] = (register_t)arginfo.ps_argvstr;
	tf->fixreg[5] = (register_t)arginfo.ps_envstr;
	tf->fixreg[6] = 0;			/* auxillary vector */
	tf->fixreg[7] = 0;			/* termination vector */
	tf->fixreg[8] = (register_t)PS_STRINGS;	/* NetBSD extension */

	tf->srr0 = pack->ep_entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

/*
 * Machine dependent system variables
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
	/* all sysctl names at this level are terminal */
	if (namelen != 1) {
		return ENOTDIR;
	}

	switch (name[0]) {
	case CPU_CACHELINE:
		return sysctl_rdint(oldp, oldlenp, newp, CACHELINESIZE);
	default:
		return EOPNOTSUPP;
	}
}


/*
 * Halt or reboot the machine after syncing/dumping according to howto
 */
void
cpu_reboot(howto, what)
	int howto;
	char *what;
{
	static int syncing;
	static char str[256];

	howto = 0;
}

int
lcsplx(ipl)
	int ipl;
{
	return spllower(ipl);   /* XXX */
}
