/*	$NetBSD: machdep.c,v 1.92.2.2 1997/09/22 06:32:22 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

/* from: Utah Hdr: machdep.c 1.63 91/04/24 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm_kern.h>
#include <ufs/mfs/mfs_extern.h>		/* mfs_initminiroot() */

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/autoconf.h>
#include <mips/locore.h>		/* wbflush() */
#include <mips/mips/mips_mcclock.h>	/* mclock CPU setimation */

#ifdef DDB
#include <mips/db_machdep.h>
#endif

#include <pmax/stand/dec_prom.h>

#include <pmax/dev/ascreg.h>

#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/turbochannel.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/trap.h>		/* mboard-specific interrupt fns */
#include <pmax/pmax/cons.h>

#include "pm.h"
#include "cfb.h"
#include "mfb.h"
#include "xcfb.h"
#include "sfb.h"
#include "dtop.h"
#include "scc.h"
#include "le_ioasic.h"
#include "asc.h"


extern struct consdev *cn_tab;	/* Console I/O table... */
extern struct consdev cd;


#if defined(DS5000_25) || defined(DS5000_100) || defined(DS5000_240)
/* Will scan from max to min, inclusive */
static int tc_max_slot = KN02_TC_MAX;
static int tc_min_slot = KN02_TC_MIN;
static u_int tc_slot_phys_base [TC_MAX_SLOTS] = {
	/* use 3max for default values */
	KN02_PHYS_TC_0_START, KN02_PHYS_TC_1_START,
	KN02_PHYS_TC_2_START, KN02_PHYS_TC_3_START,
	KN02_PHYS_TC_4_START, KN02_PHYS_TC_5_START,
	KN02_PHYS_TC_6_START, KN02_PHYS_TC_7_START
};
#endif

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;	/* from <machine/param.h> */
char	cpu_model[30];

vm_map_t buffer_map;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
caddr_t	msgbufaddr;
int	msgbufmapped = 0;	/* set when safe to use msgbuf */
int	maxmem;			/* max memory per process */
int	physmem;		/* max supported memory, changes to actual */
int	physmem_boardmax;	/* {model,simm}-specific bound on physmem */
int	pmax_boardtype;		/* Mother board type */
u_long	le_iomem;		/* 128K for lance chip via. ASIC */
#ifdef ASC_IOASIC_BOUNCE
u_long	asc_iomem;		/* and 7 * 8K buffers for the scsi */
#endif
u_long	ioasic_base;		/* Base address of I/O asic */
const	struct callback *callv;	/* pointer to PROM entry points */

extern void	(*tc_enable_interrupt)  __P ((u_int slotno,
					      int (*handler) __P((void *sc)),
					      void *sc, int onoff)); 
void	(*tc_enable_interrupt) __P ((u_int slotno,
				     int (*handler) __P ((void *sc)),
				     void *sc, int onoff));
#ifdef DS3100
#include <pmax/pmax/kn01var.h>
void	kn01_enable_intr  __P ((u_int slotno,
				int (*handler) __P ((intr_arg_t sc)),
				intr_arg_t sc, int onoff));
#endif /* DS3100 */

#ifdef DS5100 /* mipsmate */
# include <pmax/pmax/kn230var.h>   /* kn230_establish_intr(), kn230_intr() */
#endif

/*
 * Interrupt-blocking functions defined in locore. These names aren't used
 * directly except here and in interrupt handlers.
 */

/* Block out one hardware interrupt-enable bit. */
extern int	Mach_spl0 __P((void)), Mach_spl1 __P((void));
extern int	Mach_spl2 __P((void)), Mach_spl3 __P((void));

/* Block out nested interrupt-enable bits. */
extern int	cpu_spl0 __P((void)), cpu_spl1 __P((void));
extern int	cpu_spl2 __P((void)), cpu_spl3 __P((void));
extern int	splhigh __P((void));

/*
 * Instead, we declare the standard splXXX names as function pointers,
 * and initialie them to point to the above functions to match
 * the way a specific motherboard is  wired up.
 */
int	(*Mach_splbio) __P((void)) = splhigh;
int	(*Mach_splnet)__P((void)) = splhigh;
int	(*Mach_spltty)__P((void)) = splhigh;
int	(*Mach_splimp)__P((void)) = splhigh;
int	(*Mach_splclock)__P((void)) = splhigh;
int	(*Mach_splstatclock)__P((void)) = splhigh;

volatile struct chiptime *mcclock_addr;
u_long	kmin_tc3_imask, xine_tc3_imask;

int	savectx __P((struct user *up));		/* XXX save state b4 crash*/


tc_option_t tc_slot_info[TC_MAX_LOGICAL_SLOTS];


/*
 *  Local functions.
 */
#ifdef DS5000_240	/* XXX */
static	void asic_init __P((int isa_maxine));
#endif
extern	int	atoi __P((const char *cp));
int	initcpu __P((void));
#if defined(DS5000_240) || defined(DS5000_25)
static	u_long	clkread __P((void));	/* get usec-resolution clock */
#endif
void	dumpsys __P((void));		/* do a dump */

/* initialize bss, etc. from kernel start, before main() is called. */
extern	void
mach_init __P((int argc, char *argv[], u_int code,
    const struct callback *cv));

#ifdef DS5000_200
void	kn02_enable_intr __P ((u_int slotno,
			       int (*handler) __P((intr_arg_t sc)),
			       intr_arg_t sc, int onoff));
#endif /*DS5000_200*/

#ifdef DS5000_100
void	kmin_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			     intr_arg_t sc, int onoff));
void kmin_mcclock_cpuspeed __P((volatile struct chiptime *mcclock_addr,
			       int clockmask));
#endif /*DS5000_100*/

#ifdef DS5000_25
void	xine_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			    intr_arg_t sc, int onoff));
#endif /*DS5000_25*/

#ifdef DS5000_240
u_long	kn03_tc3_imask;
void	kn03_tc_reset __P((void));		/* XXX unused? */
void	kn03_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			       intr_arg_t sc, int onoff));
#endif /*DS5000_240*/

#if defined(DS5000_200) || defined(DS5000_25) || defined(DS5000_100) || \
    defined(DS5000_240)
volatile u_int *Mach_reset_addr;
#endif /* DS5000_200 || DS5000_25 || DS5000_100 || DS5000_240 */


void	prom_halt __P((int, char *))   __attribute__((__noreturn__));

#ifdef DEBUG
/* stacktrace code violates prototypes to get callee's registers */
extern void stacktrace __P((void)); /*XXX*/
#endif

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.  Used as an argument to splx().
 * XXX disables interrupt 5 to disable mips3 on-chip clock, which also
 * disables mips1 FPU interrupts.
 */
int	safepri = MIPS3_PSL_LOWIPL;	/* XXX */

struct	user *proc0paddr;
struct	proc nullproc;		/* for use by switch_exit() */

/* locore callback-vector setup */
extern void mips_vector_init  __P((void));

/*
 * Do all the stuff that locore normally does before calling main().
 * Process arguments passed to us by the prom monitor.
 * Return the first page address following the system.
 */
void
mach_init(argc, argv, code, cv)
	int argc;
	char *argv[];
	u_int code;
	const struct callback *cv;
{
	register char *cp;
	register int i;
	register unsigned firstaddr;
	register caddr_t v;
	caddr_t start;
	extern char edata[], end[];

	/* clear the BSS segment */
	v = (caddr_t)mips_round_page(end);
	bzero(edata, v - edata);

	/* Initialize callv so we can do PROM output... */
	if (code == DEC_PROM_MAGIC) {
		callv = cv;
	} else {
		callv = &callvec;
	}

	/* Use PROM console output until we initialize a console driver. */
	cn_tab = &cd;

	/* check for direct boot from DS5000 PROM */
	if (argc > 0 && strcmp(argv[0], "boot") == 0) {
		argc--;
		argv++;
	}

	/*
	 * Copy exception-dispatch code down to exception vector.
	 * Initialize locore-function vector.
	 * Clear out the I and D caches.
	 */
	mips_vector_init();

#ifdef DDB
	/*
	 * Initialize machine-dependent DDB commands, in case of early panic.
	 */
	db_machine_init();
#endif

	/* look at argv[0] and compute bootdev */
	makebootdev(argv[0]);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_SINGLE;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			for (cp = argv[i]; *cp; cp++) {
				switch (*cp) {
				case 'a': /* autoboot */
					boothowto &= ~RB_SINGLE;
					break;

				case 'd': /* use compiled in default root */
					boothowto |= RB_DFLTROOT;
					break;

				case 'm': /* mini root present in memory */
					boothowto |= RB_MINIROOT;
					break;

				case 'n': /* ask for names */
					boothowto |= RB_ASKNAME;
					break;

				case 'N': /* don't ask for names */
					boothowto &= ~RB_ASKNAME;
				}
			}
		}
	}

#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	if (boothowto & RB_MINIROOT) {
		boothowto |= RB_DFLTROOT;
		v += mfs_initminiroot(v);
	}
#endif

	/*
	 * Init mapping for u page(s) for proc[0], pm_tlbpid 1.
	 */
	start = v;
	proc0.p_addr = proc0paddr = (struct user *)v;
	curpcb = (struct pcb *)proc0.p_addr;
	proc0.p_md.md_regs = proc0paddr->u_pcb.pcb_regs;
	firstaddr = MIPS_KSEG0_TO_PHYS(v);

	if (CPUISMIPS3) for (i = 0; i < UPAGES; i+=2) {
		struct tlb tlb;

		tlb.tlb_mask = MIPS3_PG_SIZE_4K;
		tlb.tlb_hi = mips3_vad_to_vpn((UADDR + (i << PGSHIFT))) | 1;
		tlb.tlb_lo0 = vad_to_pfn(firstaddr) |
			MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
		tlb.tlb_lo1 = vad_to_pfn(firstaddr + NBPG) |
			MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
		proc0.p_md.md_upte[i] = tlb.tlb_lo0;
		proc0.p_md.md_upte[i+1] = tlb.tlb_lo1;
		mips3_TLBWriteIndexedVPS(i,&tlb);
		firstaddr += NBPG * 2;
	}
	else for (i = 0; i < UPAGES; i++) {
		mips1_TLBWriteIndexed(i,
			(UADDR + (i << PGSHIFT)) | (1 << MIPS1_TLB_PID_SHIFT),
			proc0.p_md.md_upte[i] = firstaddr |
				      MIPS1_PG_V | MIPS1_PG_M);
		firstaddr += NBPG;
	}
	v += UPAGES * NBPG;
	MachSetPID(1);

	/*
	 * init nullproc for switch_exit().
	 * init mapping for u page(s), pm_tlbpid 0
	 * This could be used for an idle process.
	 */
	nullproc.p_addr = (struct user *)v;
	nullproc.p_md.md_regs = nullproc.p_addr->u_pcb.pcb_regs;
	bcopy("nullproc", nullproc.p_comm, sizeof("nullproc"));
	if (CPUISMIPS3) {
		/* mips3 */
		for (i = 0; i < UPAGES; i+=2) {
			nullproc.p_md.md_upte[i] = vad_to_pfn(firstaddr) |
			    MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			nullproc.p_md.md_upte[i+1] =
			    vad_to_pfn(firstaddr + NBPG) |
			         MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			firstaddr += NBPG * 2;
		}
	} else { 
		/* mips1 */
		for (i = 0; i < UPAGES; i++) {
			nullproc.p_md.md_upte[i] = firstaddr |
				MIPS1_PG_V | MIPS1_PG_M;
			firstaddr += NBPG;
		}
	}

	v += UPAGES * NBPG;

	/* clear pages for u areas */
	bzero(start, v - start);

	if (CPUISMIPS3) {
		mips3_FlushDCache(MIPS_KSEG0_TO_PHYS(start), v - start);
		mips3_HitFlushDCache(UADDR, UPAGES * NBPG);
	}

	/*
	 * Determine what model of computer we are running on.
	 */
	if (code == DEC_PROM_MAGIC) {
		i = (*cv->_getsysid)();
		cp = "";
	} else {
		cp = (*callv->_getenv)("systype");
		if (cp)
			i = atoi(cp);
		else {
			cp = "";
			i = 0;
		}
	}

	/* check for MIPS based platform */
	/* 0x82 -> MIPS1, 0x84 -> MIPS3 */
	if (((i >> 24) & 0xFF) != 0x82 && ((i >> 24) & 0xff) != 0x84) {
		printf("Unknown System type '%s' 0x%x\n", cp, i);
		cpu_reboot(RB_HALT | RB_NOSYNC, NULL);
	}

	/*
	 * Initialize physmem_boardmax; assume no SIMM-bank limits.
	 * Adjst later in model-specific code if necessary.
	 */
	physmem_boardmax = MIPS_MAX_MEM_ADDR;

	/* check what model platform we are running on */
	pmax_boardtype = ((i >> 16) & 0xff);

	switch (pmax_boardtype) {

#ifdef DS3100
	case DS_PMAX:	/* DS3100 Pmax */
		/*
		 * Set up interrupt handling and I/O addresses.
		 */
		mips_hardware_intr = kn01_intr;
		tc_enable_interrupt = kn01_enable_intr; /*XXX*/
		Mach_splbio = cpu_spl0;
		Mach_splnet = cpu_spl1;
		Mach_spltty = cpu_spl2;
		Mach_splimp = splhigh; /*XXX Mach_spl1(), if not for malloc()*/
		Mach_splclock = cpu_spl3;
		Mach_splstatclock = cpu_spl3;

		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
		mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_3);

		strcpy(cpu_model, "3100");
		break;
#endif /* DS3100 */


#ifdef DS5100
	case DS_MIPSMATE:	/* DS5100 aka mipsmate aka kn230 */
		/* XXX just a guess */
		/*
		 * Set up interrupt handling and I/O addresses.
		 */
		mips_hardware_intr = kn230_intr;
		tc_enable_interrupt = kn01_enable_intr; /*XXX*/
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl1;
		Mach_spltty = Mach_spl2;
		Mach_splimp = Mach_spl2;
		Mach_splclock = Mach_spl3;
		Mach_splstatclock = Mach_spl3;
		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
		strcpy(cpu_model, "5100");
		break;
#endif /* DS5100 */

#ifdef DS5000_200
	case DS_3MAX:	/* DS5000/200 3max */
		{
		volatile int *csr_addr =
			(volatile int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);

		Mach_reset_addr =
		    (unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_ERRADR);
		/* clear any memory errors from new-config probes */
		*Mach_reset_addr = 0;

		/*
		 * Enable ECC memory correction, turn off LEDs, and
		 * disable all TURBOchannel interrupts.
		 */
		i = *csr_addr;
		*csr_addr = (i & ~(KN02_CSR_WRESERVED | KN02_CSR_IOINTEN)) |
			KN02_CSR_CORRECT | 0xff;
		mips_hardware_intr = kn02_intr;
		tc_enable_interrupt = kn02_enable_intr;
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl0;
		Mach_spltty = Mach_spl0;
		Mach_splimp = Mach_spl0;
		Mach_splclock = cpu_spl1;
		Mach_splstatclock = cpu_spl1;
		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN02_SYS_CLOCK);

		}
		mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_1);
		strcpy(cpu_model, "5000/200");
		break;
#endif /* DS5000_200 */

#ifdef DS5000_100
	case DS_3MIN:	/* DS5000/1xx 3min */
		tc_max_slot = KMIN_TC_MAX;
		tc_min_slot = KMIN_TC_MIN;
		tc_slot_phys_base[0] = KMIN_PHYS_TC_0_START;
		tc_slot_phys_base[1] = KMIN_PHYS_TC_1_START;
		tc_slot_phys_base[2] = KMIN_PHYS_TC_2_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(KMIN_SYS_ASIC);
		mips_hardware_intr = kmin_intr;
		tc_enable_interrupt = kmin_enable_intr;
		kmin_tc3_imask = (KMIN_INTR_CLOCK | KMIN_INTR_PSWARN |
			KMIN_INTR_TIMEOUT);

		/*
		 * Since all the motherboard interrupts come through the
		 * I/O ASIC, it has to be turned off for all the spls and
		 * since we don't know what kinds of devices are in the
		 * turbochannel option slots, just splhigh().
		 */
		Mach_splbio = splhigh;
		Mach_splnet = splhigh;
		Mach_spltty = splhigh;
		Mach_splimp = splhigh;
		Mach_splclock = splhigh;
		Mach_splstatclock = splhigh;
		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KMIN_SYS_CLOCK);
		kmin_mcclock_cpuspeed(mcclock_addr, MIPS_INT_MASK_3);

		/*
		 * Initialize interrupts.
		 */
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = KMIN_IM0;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;

		/* clear any memory errors from probes */
		Mach_reset_addr =
		    (u_int*)MIPS_PHYS_TO_KSEG1(KMIN_REG_TIMEOUT);
		(*Mach_reset_addr) = 0;

		sprintf(cpu_model, "5000/1%d", cpu_mhz);

		/*
		 * The kmin memory hardware seems to wrap  memory addresses
		 * with 4Mbyte SIMMs, which causes the physmem computation
		 * to lose.  Find out how big the SIMMS are and set
		 * max_	physmem accordingly.
		 * XXX Do MAXINEs lose the same way?
		 */
		physmem_boardmax = KMIN_PHYS_MEMORY_END + 1;
		if ((*(int*)(MIPS_PHYS_TO_KSEG1(KMIN_REG_MSR)) &
		     KMIN_MSR_SIZE_16Mb) == 0)
			physmem_boardmax = physmem_boardmax >> 2;
		physmem_boardmax = MIPS_PHYS_TO_KSEG1(physmem_boardmax);

		break;
#endif /* ds5000_100 */

#ifdef DS5000_25
	case DS_MAXINE:	/* DS5000/xx maxine */
		tc_max_slot = XINE_TC_MAX;
		tc_min_slot = XINE_TC_MIN;
		tc_slot_phys_base[0] = XINE_PHYS_TC_0_START;
		tc_slot_phys_base[1] = XINE_PHYS_TC_1_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(XINE_SYS_ASIC);
		mips_hardware_intr = xine_intr;
		tc_enable_interrupt = xine_enable_intr;

		/* On the MAXINE ioasic interrupts at level 3. */
		Mach_splbio = Mach_spl3;
		Mach_splnet = Mach_spl3;
		Mach_spltty = Mach_spl3;
		Mach_splimp = Mach_spl3;

		/*
		 * Note priority inversion of ioasic and clock:
		 * clock interrupts are at hw priority 1, and when blocking
		 * clock interrups we we must block hw priority 3
		 * (bio,net,tty) also.
		 *
		 * XXX hw priority 2 is used for memory errors, we
		 * should not disable memory errors during clock interrupts!
		 */
		Mach_splclock = cpu_spl3;
		Mach_splstatclock = cpu_spl3;
		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(XINE_SYS_CLOCK);
		mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_1);

		/*
		 * Initialize interrupts.
		 */
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = XINE_IM0;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		/* clear any memory errors from probes */
		Mach_reset_addr =
		    (u_int*)MIPS_PHYS_TO_KSEG1(XINE_REG_TIMEOUT);
		(*Mach_reset_addr) = 0;

		sprintf(cpu_model, "5000/%d", cpu_mhz);

		break;
#endif /*DS5000_25*/

#ifdef DS5000_240
	case DS_3MAXPLUS:	/* DS5000/240 3max+ */
		tc_max_slot = KN03_TC_MAX;
		tc_min_slot = KN03_TC_MIN;
		tc_slot_phys_base[0] = KN03_PHYS_TC_0_START;
		tc_slot_phys_base[1] = KN03_PHYS_TC_1_START;
		tc_slot_phys_base[2] = KN03_PHYS_TC_2_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(KN03_SYS_ASIC);
		mips_hardware_intr = kn03_intr;
		tc_enable_interrupt = kn03_enable_intr;
		Mach_reset_addr =
		    (u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR);
		*Mach_reset_addr = 0;

		/*
		 * Reset interrupts, clear any errors from newconf probes
		 */
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl0;
		Mach_spltty = Mach_spl0;
		Mach_splimp = Mach_spl0;	/* XXX */
		/*
		 * Clock interrupts at hw priority 1 must block bio,net,tty
		 * at hw priority 0.
		 */
		Mach_splclock = cpu_spl1;
		Mach_splstatclock = cpu_spl1;
		mcclock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN03_SYS_CLOCK);
		mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_1);

		asic_init(0);
		/*
		 * Initialize interrupts.
		 */
		kn03_tc3_imask = KN03_IM0 &
			~(KN03_INTR_TC_0|KN03_INTR_TC_1|KN03_INTR_TC_2);
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = kn03_tc3_imask;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		wbflush();
		/* XXX hard-reset LANCE */
		 *(u_int *)IOASIC_REG_CSR(ioasic_base) |= 0x100;

		/* clear any memory errors from probes */
		*Mach_reset_addr = 0;
		sprintf(cpu_model, "5000/2%d", cpu_mhz);
		break;
#endif /* DS5000_240 */

	default:
		printf("kernel not configured for systype 0x%x\n", i);
		cpu_reboot(RB_HALT | RB_NOSYNC, NULL);
	}

	/*
	 * Find out how much memory is available.
	 * Be careful to save and restore the original contents for msgbuf.
	 */
	physmem = btoc((vm_offset_t)v - KERNBASE);
	cp = (char *)MIPS_PHYS_TO_KSEG1(physmem << PGSHIFT);	
	while (cp < (char *)physmem_boardmax) {
	  	int j;
		if (badaddr(cp, 4))
			break;
		i = *(int *)cp;
		j = ((int *)cp)[4];
		*(int *)cp = 0xa5a5a5a5;
		/*
		 * Data will persist on the bus if we read it right away.
		 * Have to be tricky here.
		 */
		((int *)cp)[4] = 0x5a5a5a5a;
		wbflush();
		if (*(int *)cp != 0xa5a5a5a5)
			break;
		*(int *)cp = i;
		((int *)cp)[4] = j;
		cp += NBPG;
		physmem++;
	}

	maxmem = physmem;

#if NLE_IOASIC > 0
	/*
	 * Grab 128K at the top of physical memory for the lance chip
	 * on machines where it does dma through the I/O ASIC.
	 * It must be physically contiguous and aligned on a 128K boundary.
	 */
	if (pmax_boardtype == DS_3MIN || pmax_boardtype == DS_MAXINE ||
		pmax_boardtype == DS_3MAXPLUS) {
		maxmem -= btoc(128 * 1024);
		le_iomem = (maxmem << PGSHIFT);
	}
#endif /* NLE_IOASIC */
#if (NASC > 0) && defined(ASC_IOASIC_BOUNCE)
	/*
	 * Ditto for the scsi chip. There is probably a way to make asc.c
	 * do dma without these buffers, but it would require major
	 * re-engineering of the asc driver.
	 * They must be 8K in size and page aligned.
	 * (now 16K, as that's how big clustered FFS reads/writes get).
	 */
	if (pmax_boardtype == DS_3MIN || pmax_boardtype == DS_MAXINE ||
		pmax_boardtype == DS_3MAXPLUS) {
		maxmem -= btoc(ASC_NCMD * (16 *1024));
		asc_iomem = (maxmem << PGSHIFT);
	}
#endif /* NASC */

	/*
	 * Initialize error message buffer (at end of core).
	 */
	maxmem -= btoc(MSGBUFSIZE);
	msgbufaddr = (caddr_t)(MIPS_PHYS_TO_KSEG0(maxmem << PGSHIFT));
	initmsgbuf(msgbufaddr, mips_round_page(MSGBUFSIZE));

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 *
	 * These data structures are allocated here instead of cpu_startup()
	 * because physical memory is directly addressable. We don't have
	 * to map these into virtual address space.
	 */
	start = v;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.
	 * We allocate more buffer space than the BSD standard of
	 * using 10% of memory for the first 2 Meg, 5% of remaining.
	 * We just allocate a flat 10%.  Ensure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem / 10 / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * Clear allocated memory.
	 */
	bzero(start, v - start);

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap((vm_offset_t)v);
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	register unsigned i;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, TRUE);
	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Configure the system.
	 */
	configure();
}


/*
 * machine dependent system variables.
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
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &cn_tab->cn_dev,
		    sizeof cn_tab->cn_dev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int	waittime = -1;
struct user dumppcb;	/* Actually, struct pcb would do. */


/*
 * These variables are needed by /sbin/savecore
 */
int	dumpmag = (int)0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
cpu_dumpconf()
{
	int nblks;

	dumpsize = physmem;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(physmem));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?)
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);
	printf("dump ");
	/*
	 * XXX
	 * All but first arguments to  dump() bogus.
	 * What should blkno, va, size be?
	 */
	error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, 0, 0, 0);
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	default:
		printf("error %d\n", error);
		break;

	case 0:
		printf("succeeded\n");
	}
}


/*
 * call PROM to halt or reboot.
 */
volatile void
prom_halt(howto, bootstr)
	int howto;
	char *bootstr;

{
	if (callv != &callvec) {
		if (howto & RB_HALT)
			(*callv->_rex)('h');
		else {
			(*callv->_rex)('b');
		}
	} else if (howto & RB_HALT) {
		volatile void (*f) __P((void)) = 
		    (volatile void (*) __P((void))) DEC_PROM_REINIT;

		(*f)();	/* jump back to prom monitor */
	} else {
		volatile void (*f) __P((void)) = 
		    (volatile void (*) __P((void)))DEC_PROM_AUTOBOOT;
		(*f)();	/* jump back to prom monitor and do 'auto' cmd */
	}

	while(1) ;	/* fool gcc */
	/*NOTREACHED*/
}

void
cpu_reboot(howto, bootstr)
	/*register*/ int howto;
	char *bootstr;
{
	extern int cold;

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx((struct user *)curpcb);

#ifdef DEBUG
	if (panicstr)
		stacktrace();
#endif

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	/* If "always halt" was specified as a boot flag, obey. */
	if ((boothowto & RB_HALT) != 0)
		howto |= RB_HALT;

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		/*
		 * Synchronize the disks....
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
#if 0
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
#else
	if (howto & RB_DUMP)
#endif
		dumpsys();

haltsys:

	/* run any shutdown hooks */
	doshutdownhooks();


	/* Finally, halt/reboot the system. */
	printf("%s\n\n", howto & RB_HALT ? "halted." : "rebooting...");
	prom_halt(howto & RB_HALT, bootstr);
	/*NOTREACHED*/
}


/*
 * Read a high-resolution clock, if one is available, and return
 * the current microsecond offset from time-of-day.
 */

#if !defined(DS5000_240) && !defined(DS5000_25)
# define clkread() (0)
#else

/*
 * IOASIC TC cycle counter, latched on every interrupt from RTC chip.
 * [Or free-running microsecond counter on Maxine.]
 */
u_long latched_cycle_cnt;

/*
 * On a Decstation 5000/240,  use the turbochannel bus-cycle counter
 * to interpolate micro-seconds since the  last RTC clock tick.
 * The interpolation base is the copy of the bus cycle-counter taken
 * by the RTC interrupt handler.
 * On XINE, use the microsecond free-running counter.
 *
 */
static inline u_long
clkread()
{

#ifdef DS5000_240
	register u_long usec, cycles;	/* really 32 bits? */
#endif

#ifdef DS5000_25
	if (pmax_boardtype == DS_MAXINE)
		return (*(u_long*)(MIPS_PHYS_TO_KSEG1(XINE_REG_FCTR)) -
		    latched_cycle_cnt);
	else
#endif
#ifdef DS5000_240
	if (pmax_boardtype == DS_3MAXPLUS)
		/* 5k/240 TC bus counter */
		cycles = *(u_long*)IOASIC_REG_CTR(ioasic_base);
	else
#endif
		return (0);

#ifdef DS5000_240
	/* Compute difference in cycle count from last hardclock() to now */
#if 1
	/* my code, using u_ints */
	cycles = cycles - latched_cycle_cnt;
#else
	/* Mills code, using (signed) ints */
	if (cycles >= latched_cycle_cnt)
		cycles = cycles - latched_cycle_cnt;
	else
		cycles = latched_cycle_cnt - cycles;
#endif

	/*
	 * Scale from 40ns to microseconds.
	 * Avoid a kernel FP divide (by 25) using the approximation 
	 * 1/25 = 40/1000 =~ 41/ 1024, which is good to 0.0975 %
	 */
	usec = cycles + (cycles << 3) + (cycles << 5);
	usec = usec >> 10;

#ifdef CLOCK_DEBUG
	if (usec > 3906 +4) {
		 addlog("clkread: usec %d, counter=%lx\n",
			 usec, latched_cycle_cnt);
		stacktrace();
	}
#endif /*CLOCK_DEBUG*/
	return usec;
#endif /*DS5000_240*/
}

#if 0
void
microset()
{
		latched_cycle_cnt = *(u_long*)(IOASIC_REG_CTR(ioasic_base));
}
#endif
#endif /*DS5000_240 || DS5000_25*/


/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  Unfortunately, we can't read the hardware registers.
 * We guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splclock();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec += clkread();
	if (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

int
initcpu()
{
	register volatile struct chiptime *c;
	int i = 0;

#if defined(DS5000_200) || defined(DS5000_25) || defined(DS5000_100) || \
    defined(DS5000_240)
	/* Reset after bus errors during probe */
	if (Mach_reset_addr) {
		*Mach_reset_addr = 0;
		wbflush();
	}
#endif

	/* clear any pending interrupts */
	switch (pmax_boardtype) {
	case DS_PMAX:
		break;	/* nothing to  do for KN01. */
	case DS_3MAXPLUS:
	case DS_3MIN:
	case DS_MAXINE:
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		break;
	case DS_3MAX:
		*(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CHKSYN) = 0;
		wbflush();
		break;
	default:
		printf("initcpu(): unknown system type 0x%x\n", pmax_boardtype);
		break;
	}

	/*
	 * With newconf, this should be  done elswhere, but without it
	 * we hang (?)
	 */
#if 1 /*XXX*/
	/* disable clock interrupts (until startrtclock()) */
	if (mcclock_addr) {
		c = mcclock_addr;
		c->regb = REGB_DATA_MODE | REGB_HOURS_FORMAT;
		i = c->regc;
	}
	return (i);
#endif
}

/*
 * Convert an ASCII string into an integer.
 */
int
atoi(s)
	const char *s;
{
	int c;
	unsigned base = 10, d;
	int neg = 0, val = 0;

	if (s == 0 || (c = *s++) == 0)
		goto out;

	/* skip spaces if any */
	while (c == ' ' || c == '\t')
		c = *s++;

	/* parse sign, allow more than one (compat) */
	while (c == '-') {
		neg = !neg;
		c = *s++;
	}

	/* parse base specification, if any */
	if (c == '0') {
		c = *s++;
		switch (c) {
		case 'X':
		case 'x':
			base = 16;
			break;
		case 'B':
		case 'b':
			base = 2;
			break;
		default:
			base = 8;
		}
	}

	/* parse number proper */
	for (;;) {
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'z')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			d = c - 'A' + 10;
		else
			break;
		val *= base;
		val += d;
		c = *s++;
	}
	if (neg)
		val = -val;
out:
	return val;	
}


#ifdef DS3100

/*
 * Enable an interrupt from a slot on the KN01 internal bus.
 *
 * The 4.4bsd kn01 interrupt handler hard-codes r3000 CAUSE register
 * bits to particular device interrupt handlers.  We may choose to store
 * function and softc pointers at some future point.
 */
void
kn01_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	/*
	 */
	if (on)  {
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
}
#endif /* DS3100 */


#ifdef DS5000_200

/*
 * Enable/Disable interrupts for a TURBOchannel slot on the 3MAX.
 */
void
kn02_enable_intr(slotno, handler, sc, on)
	register u_int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register volatile int *p_csr =
		(volatile int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);
	int csr;
	int s;

#if 0
	printf("3MAX enable_intr: imask %x, %sabling slot %d, sc %p\n",
	       kn03_tc3_imask, (on? "en" : "dis"), slotno, sc);
#endif

	if (slotno > TC_MAX_LOGICAL_SLOTS)
		panic("kn02_enable_intr: bogus slot %d\n", slotno);

	if (on)  {
		/*printf("kn02: slot %d handler 0x%x\n", slotno, handler);*/
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}

	slotno = 1 << (slotno + KN02_CSR_IOINTEN_SHIFT);
	s = Mach_spl0();
	csr = *p_csr & ~(KN02_CSR_WRESERVED | 0xFF);
	if (on)
		*p_csr = csr | slotno;
	else
		*p_csr = csr & ~slotno;
	splx(s);
}
#endif /*DS5000_200*/

#ifdef DS5000_100
/*
 *	Object:
 *		kmin_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 8 slots even if we really have
 *	only 4: TCslots 0-2 maps to slots 0-2, TCslot3 maps to
 *	slots 3-7 (see pmax/tc/ds-asic-conf.c).
 *
 *	3MIN TURBOchannel interrupts are unlike other decstations,
 *	in that interrupt requests from the option slots (0-2) map
 *	directly to R3000 interrupt lines, not to IOASIC interrupt
 *	bits.  If it weren't for that, the 3MIN and 3MAXPLUS could
 *	share   interrupt handlers and interrupt-enable code
 */
void
kmin_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

	switch (slotno) {
		/* slots 0-2 don't interrupt through the IOASIC. */
	case 0:
		mask = MIPS_INT_MASK_0;	break;
	case 1:
		mask = MIPS_INT_MASK_1; break;
	case 2:
		mask = MIPS_INT_MASK_2; break;

	case KMIN_SCSI_SLOT:
		mask = (KMIN_INTR_SCSI | KMIN_INTR_SCSI_PTR_LOAD |
			KMIN_INTR_SCSI_OVRUN | KMIN_INTR_SCSI_READ_E);
		break;

	case KMIN_LANCE_SLOT:
		mask = KMIN_INTR_LANCE;
		break;
	case KMIN_SCC0_SLOT:
		mask = KMIN_INTR_SCC_0;
		break;
	case KMIN_SCC1_SLOT:
		mask = KMIN_INTR_SCC_1;
		break;
	case KMIN_ASIC_SLOT:
		mask = KMIN_INTR_ASIC;
		break;
	default:
		return;
	}

#if defined(DEBUG) || defined(DIAGNOSTIC)
	printf("3MIN: imask %lx, %sabling slot %d, sc %p handler %p\n",
	       kmin_tc3_imask, (on? "en" : "dis"), slotno, sc, handler);
#endif

	/*
	 * Enable the interrupt  handler, and if it's an IOASIC
	 * slot, set the IOASIC interrupt mask.
	 * Otherwise, set the appropriate spl level in the R3000
	 * register.
	 * Be careful to set handlers  before enabling, and disable
	 * interrupts before clearing handlers.
	 */

	if (on) {
		/* Set the interrupt handler and argument ... */
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;

		/* ... and set the relevant mask */
		if (slotno <= 2) {
			/* it's an option slot */
			int s = splhigh();
			s  |= mask;
			splx(s);
		} else {
			/* it's a baseboard device going via the ASIC */
			kmin_tc3_imask |= mask;
		}
	} else {
		/* Clear the relevant mask... */
		if (slotno <= 2) {	
			/* it's an option slot */
			int s = splhigh();
			printf("kmin_intr: cannot disable option slot %d\n",
			    slotno);
			s &= ~mask;
			splx(s);
		} else {
			/* it's a baseboard device going via the ASIC */
			kmin_tc3_imask &= ~mask;
		}
		/* ... and clear the handler */
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
}

/*
 * Count instructions between 4ms mcclock interrupt requests,
 * using the ioasic clock-interrupt-pending bit to determine
 * when clock ticks occur.  
 * Set up iosiac to allow only clock interrupts, then
 * call 
 */
void
kmin_mcclock_cpuspeed(mcclock_addr, clockmask)
	volatile struct chiptime *mcclock_addr;
	int clockmask;
{
	register volatile u_int * ioasic_intrmaskp =
		(volatile u_int *)MIPS_PHYS_TO_KSEG1(KMIN_REG_IMSK);

	register int saved_imask = *ioasic_intrmaskp;

	/* Allow only clock interrupts through ioasic. */
	*ioasic_intrmaskp = KMIN_INTR_CLOCK;
	wbflush();
     
	mc_cpuspeed(mcclock_addr, clockmask);

	*ioasic_intrmaskp = saved_imask;
	wbflush();
}


#endif /*DS5000_100*/


#ifdef DS5000_25
/*
 *	Object:
 *		xine_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 11 slots even if we really have
 *	only 3: TCslots 0-1 maps to slots 0-1, TCslot 2 is used for
 *	the system (TCslot3), TCslot3 maps to slots 3-10
 *	 (see pmax/tc/ds-asic-conf.c).
 *	Note that all these interrupts come in via the IMR.
 */
void
xine_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

	switch (slotno) {
	case 0:			/* a real slot, but  */
		mask = XINE_INTR_TC_0;
		break;
	case 1:			/* a real slot, but */
		mask = XINE_INTR_TC_1;
		break;
	case XINE_FLOPPY_SLOT:
		mask = XINE_INTR_FLOPPY;
		break;
	case XINE_SCSI_SLOT:
		mask = (XINE_INTR_SCSI | XINE_INTR_SCSI_PTR_LOAD |
			XINE_INTR_SCSI_OVRUN | XINE_INTR_SCSI_READ_E);
		break;
	case XINE_LANCE_SLOT:
		mask = XINE_INTR_LANCE;
		break;
	case XINE_SCC0_SLOT:
		mask = XINE_INTR_SCC_0;
		break;
	case XINE_DTOP_SLOT:
		mask = XINE_INTR_DTOP_RX;
		break;
	case XINE_ISDN_SLOT:
		mask = XINE_INTR_ISDN;
		break;
	case XINE_ASIC_SLOT:
		mask = XINE_INTR_ASIC;
		break;
	default:
		return;/* ignore */
	}

	if (on) {
		xine_tc3_imask |= mask;
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		xine_tc3_imask &= ~mask;
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = xine_tc3_imask;
}
#endif /*DS5000_25*/

#ifdef DS5000_240
void
kn03_tc_reset()
{
	/*
	 * Reset interrupts, clear any errors from newconf probes
	 */
	*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
	*(unsigned *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR) = 0;
}


/*
 *	Object:
 *		kn03_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 8 slots even if we really have
 *	only 4: TCslots 0-2 maps to slots 0-2, TCslot3 maps to
 *	slots 3-7 (see pmax/tc/ds-asic-conf.c).
 */
void
kn03_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

#if 0
	printf("3MAXPLUS: imask %x, %sabling slot %d, unit %d addr 0x%x\n",
	       kn03_tc3_imask, (on? "en" : "dis"), slotno, unit, handler);
#endif

	switch (slotno) {
	case 0:
		mask = KN03_INTR_TC_0;
		break;
	case 1:
		mask = KN03_INTR_TC_1;
		break;
	case 2:
		mask = KN03_INTR_TC_2;
		break;
	case KN03_SCSI_SLOT:
		mask = (KN03_INTR_SCSI | KN03_INTR_SCSI_PTR_LOAD |
			KN03_INTR_SCSI_OVRUN | KN03_INTR_SCSI_READ_E);
		break;
	case KN03_LANCE_SLOT:
		mask = KN03_INTR_LANCE;
		mask |= IOASIC_INTR_LANCE_READ_E;
		break;
	case KN03_SCC0_SLOT:
		mask = KN03_INTR_SCC_0;
		break;
	case KN03_SCC1_SLOT:
		mask = KN03_INTR_SCC_1;
		break;
	case KN03_ASIC_SLOT:
		mask = KN03_INTR_ASIC;
		break;
	default:
#ifdef DIAGNOSTIC
		printf("warning: enabling unknown intr %x\n", slotno);
#endif
		goto done;
	}
	if (on) {
		kn03_tc3_imask |= mask;
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;

	} else {
		kn03_tc3_imask &= ~mask;
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
done:
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = kn03_tc3_imask;
	wbflush();
}
#endif /* DS5000_240 */


#ifdef DS5000_240	/* XXX */
/*
 * Initialize the I/O asic
 */
static void
asic_init(isa_maxine)
	int isa_maxine;
{
	volatile u_int *decoder;

	/* These are common between 3min and maxine */
	decoder = (volatile u_int *)IOASIC_REG_LANCE_DECODE(ioasic_base);
	*decoder = KMIN_LANCE_CONFIG;

	/* set the SCSI DMA configuration map */
	decoder = (volatile u_int *) IOASIC_REG_SCSI_DECODE(ioasic_base);
	(*decoder) = 0x00000000e;
}
#endif /* DS5000_240 XXX */
