/* $NetBSD: cpu.c,v 1.39.8.1 1999/12/27 18:31:19 wrstuden Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.39.8.1 1999/12/27 18:31:19 wrstuden Exp $");

#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <vm/vm.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/alpha.h>

#if defined(MULTIPROCESSOR)
#include <sys/malloc.h>
#include <sys/kthread.h>

/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */
struct cpu_info cpu_info[ALPHA_MAXPROCS];

/* Bitmask of CPUs currently running. */
__volatile u_long cpus_running;

void	cpu_boot_secondary __P((struct cpu_info *));
#else
paddr_t curpcb;				/* PA of our current context */
struct	proc *fpcurproc;		/* current owner of FPU */
#endif /* MULTIPROCESSOR */

/* Definition of the driver for autoconfig. */
static int	cpumatch(struct device *, struct cfdata *, void *);
static void	cpuattach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};

extern struct cfdriver cpu_cd;

static char *ev4minor[] = {
	"pass 2 or 2.1", "pass 3", 0
}, *lcaminor[] = {
	"",
	"21066 pass 1 or 1.1", "21066 pass 2",
	"21068 pass 1 or 1.1", "21068 pass 2",
	"21066A pass 1", "21068A pass 1", 0
}, *ev5minor[] = {
	"", "pass 2, rev BA or 2.2, rev CA", "pass 2.3, rev DA or EA",
	"pass 3", "pass 3.2", "pass 4", 0
}, *ev45minor[] = {
	"", "pass 1", "pass 1.1", "pass 2", 0
}, *ev56minor[] = {
	"", "pass 1", "pass 2", 0
}, *ev6minor[] = {
	"pass 1", "pass 2", "pass 2.2", "pass 2.3", "pass 3", 0
}, *pca56minor[] = {
	"", "pass 1", 0
};

struct cputable_struct {
	int	cpu_major_code;
	char	*cpu_major_name;
	char	**cpu_minor_names;
} cpunametable[] = {
	{ PCS_PROC_EV3,		"EV3",		0		},
	{ PCS_PROC_EV4,		"21064",	ev4minor	},
	{ PCS_PROC_SIMULATION,	"Sim",		0		},
	{ PCS_PROC_LCA4,	"LCA",		lcaminor	},
	{ PCS_PROC_EV5,		"21164",	ev5minor	},
	{ PCS_PROC_EV45,	"21064A",	ev45minor	},
	{ PCS_PROC_EV56,	"21164A",	ev56minor	},
	{ PCS_PROC_EV6,		"21264",	ev6minor	},
	{ PCS_PROC_PCA56,	"PCA56",	pca56minor	}
};

/*
 * The following is an attempt to map out how booting secondary CPUs
 * works.
 *
 * As we find processors during the autoconfiguration sequence, all
 * processors have idle stacks and PCBs created for them, including
 * the primary (although the primary idles on proc0's PCB until its
 * idle PCB is created).
 *
 * Right before calling uvm_scheduler(), main() calls, on proc0's
 * context, cpu_boot_secondary_processors().  This is our key to
 * actually spin up the additional processor's we've found.  We
 * run through our cpu_info[] array looking for secondary processors
 * with idle PCBs, and spin them up.
 *
 * The spinup involves switching the secondary processor to the
 * OSF/1 PALcode, setting the entry point to cpu_spinup_trampoline(),
 * and sending a "START" message to the secondary's console.
 *
 * Upon successful processor bootup, the cpu_spinup_trampoline will call
 * cpu_hatch(), which will print a message indicating that the processor
 * is running, and will set the "hatched" flag in its softc.  At the end
 * of cpu_hatch() is a spin-forever loop; we do not yet attempt to schedule
 * anything on secondary CPUs.
 */

static int
cpumatch(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ma->ma_name, cpu_cd.cd_name) != 0)
		return (0);

	/* XXX CHECK SLOT? */
	/* XXX CHECK PRIMARY? */

	return (1);
}

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
	int i;
	char **s;
        struct pcs *p;
#ifdef DEBUG
	int needcomma;
#endif
	u_int32_t major, minor;
#if defined(MULTIPROCESSOR)
	extern paddr_t avail_start, avail_end;
	struct pcb *pcb;
	struct cpu_info *ci;
	struct pglist mlist;
	int error;
#endif

	p = LOCATE_PCS(hwrpb, ma->ma_slot);
	major = PCS_CPU_MAJORTYPE(p);
	minor = PCS_CPU_MINORTYPE(p);

	printf(": ID %d%s, ", ma->ma_slot,
	    ma->ma_slot == hwrpb->rpb_primary_cpu_id ? " (primary)" : "");

	for(i = 0; i < sizeof cpunametable / sizeof cpunametable[0]; ++i) {
		if (cpunametable[i].cpu_major_code == major) {
			printf("%s-%d", cpunametable[i].cpu_major_name, minor);
			s = cpunametable[i].cpu_minor_names;
			for(i = 0; s && s[i]; ++i) {
				if (i == minor && strlen(s[i]) != 0) {
					printf(" (%s)\n", s[i]);
					goto recognized;
				}
			}
			printf(" (unknown minor type %d)\n", minor);
			goto recognized;
		}
	}
	printf("UNKNOWN CPU TYPE (%d:%d)", major, minor);

recognized:

#ifdef DEBUG
	/* XXX SHOULD CHECK ARCHITECTURE MASK, TOO */
	if (p->pcs_proc_var != 0) {
		printf("%s: ", dev->dv_xname);

		needcomma = 0;
		if (p->pcs_proc_var & PCS_VAR_VAXFP) {
			printf("VAX FP support");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_IEEEFP) {
			printf("%sIEEE FP support", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_PE) {
			printf("%sPrimary Eligible", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_RESERVED)
			printf("%sreserved bits: 0x%lx", needcomma ? ", " : "",
			    p->pcs_proc_var & PCS_VAR_RESERVED);
		printf("\n");
	}
#endif

#if defined(MULTIPROCESSOR)
	if (ma->ma_slot > ALPHA_WHAMI_MAXID) {
		printf("%s: procssor ID too large, ignoring\n", dev->dv_xname);
		return;
	}

	ci = &cpu_info[ma->ma_slot];
	simple_lock_init(&ci->ci_slock);
	ci->ci_cpuid = ma->ma_slot;
	ci->ci_dev = dev;
#endif /* MULTIPROCESSOR */

	/*
	 * Though we could (should?) attach the LCA cpus' PCI
	 * bus here there is no good reason to do so, and
	 * the bus attachment code is easier to understand
	 * and more compact if done the 'normal' way.
	 */

#if defined(MULTIPROCESSOR)
	/*
	 * Make sure the processor is available for use.
	 */
	if ((p->pcs_flags & PCS_PA) == 0) {
		if (ma->ma_slot == hwrpb->rpb_primary_cpu_id)
			panic("cpu_attach: primary not available?!");
		printf("%s: processor not available for use\n", dev->dv_xname);
		return;
	}

	/* Make sure the processor has valid PALcode. */
	if ((p->pcs_flags & PCS_PV) == 0) {
		if (ma->ma_slot == hwrpb->rpb_primary_cpu_id)
			panic("cpu_attach: primary has invalid PALcode?!");
		printf("%s: PALcode not valid\n", ci->ci_dev->dv_xname);
		return;
	}

	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack.
	 */
	TAILQ_INIT(&mlist);
	error = uvm_pglistalloc(USPACE, avail_start, avail_end, 0, 0,
	    &mlist, 1, 1);
	if (error != 0) {
		if (ma->ma_slot == hwrpb->rpb_primary_cpu_id) {
			panic("cpu_attach: unable to alloate idle stack for"
			    " primary");
		}
		printf("%s: unable to allocate idle stack\n", dev->dv_xname);
		return;
	}

	ci->ci_idle_pcb_paddr = VM_PAGE_TO_PHYS(TAILQ_FIRST(&mlist));
	pcb = ci->ci_idle_pcb = (struct pcb *)
	    ALPHA_PHYS_TO_K0SEG(ci->ci_idle_pcb_paddr);
	memset(pcb, 0, USPACE);

	/*
	 * Initialize the idle stack pointer, reserving space for an
	 * (empty) trapframe (XXX is the trapframe really necessary?)
	 */
	pcb->pcb_hw.apcb_ksp =
	    (u_int64_t)pcb + USPACE - sizeof(struct trapframe);

	/*
	 * Initialize the idle PCB.
	 */
	pcb->pcb_hw.apcb_backup_ksp = pcb->pcb_hw.apcb_ksp;
	pcb->pcb_hw.apcb_asn = proc0.p_addr->u_pcb.pcb_hw.apcb_asn;
	pcb->pcb_hw.apcb_ptbr = proc0.p_addr->u_pcb.pcb_hw.apcb_ptbr;
#if 0
	printf("%s: hwpcb ksp = 0x%lx\n", sc->sc_dev.dv_xname,
	    pcb->pcb_hw.apcb_ksp);
	printf("%s: hwpcb ptbr = 0x%lx\n", sc->sc_dev.dv_xname,
	    pcb->pcb_hw.apcb_ptbr);
#endif

	/*
	 * If we're the primary CPU, no more work to do; we're already
	 * running!
	 */
	if (ma->ma_slot == hwrpb->rpb_primary_cpu_id) {
		ci->ci_flags |= CPUF_PRIMARY;
		alpha_atomic_setbits_q(&cpus_running, (1UL << ma->ma_slot));
	}
#endif /* MULTIPROCESSOR */
}

#if defined(MULTIPROCESSOR)
void
cpu_boot_secondary_processors()
{
	struct cpu_info *ci;
	u_long i;

	for (i = 0; i < ALPHA_MAXPROCS; i++) {
		ci = &cpu_info[i];
		if (ci->ci_idle_pcb == NULL)
			continue;
		if (ci->ci_flags & CPUF_PRIMARY)
			continue;

		/* This processor is all set up; boot it! */
		cpu_boot_secondary(ci);
	}
}

void
cpu_boot_secondary(ci)
	struct cpu_info *ci;
{
	long timeout;
	struct pcs *pcsp, *primary_pcsp;
	struct pcb *pcb;
	u_long cpumask;

	pcb = ci->ci_idle_pcb;
	primary_pcsp = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
	pcsp = LOCATE_PCS(hwrpb, ci->ci_cpuid);
	cpumask = (1UL << ci->ci_cpuid);

	/*
	 * Set up the PCS's HWPCB to match ours.
	 */
	memcpy(pcsp->pcs_hwpcb, &pcb->pcb_hw, sizeof(pcb->pcb_hw));

	/*
	 * Set up the HWRPB to restart the secondary processor
	 * with our spin-up trampoline.
	 */
	hwrpb->rpb_restart = (u_int64_t) cpu_spinup_trampoline;
	hwrpb->rpb_restart_val = (u_int64_t) ci;
	hwrpb->rpb_checksum = hwrpb_checksum();

	/*
	 * Configure the CPU to start in OSF/1 PALcode by copying
	 * the primary CPU's PALcode revision info to the secondary
	 * CPUs PCS.
	 */

	/*
	 * XXX Until I can update the boot block on my test system.
	 * XXX --thorpej
	 */
#if 0
	memcpy(&pcsp->pcs_pal_rev, &primary_pcsp->pcs_pal_rev,
	    sizeof(pcsp->pcs_pal_rev));
#else
	memcpy(&pcsp->pcs_pal_rev, &pcsp->pcs_palrevisions[PALvar_OSF1],
	    sizeof(pcsp->pcs_pal_rev));
#endif
	pcsp->pcs_flags |= (PCS_CV|PCS_RC);
	pcsp->pcs_flags &= ~PCS_BIP;

	/* Make sure the secondary console sees all this. */
	alpha_mb();

	/* Send a "START" command to the secondary CPU's console. */
	if (cpu_iccb_send(ci->ci_cpuid, "START\r\n")) {
		printf("%s: unable to issue `START' command\n",
		    ci->ci_dev->dv_xname);
		return;
	}

	/* Wait for the processor to boot. */
	for (timeout = 10000; timeout != 0; timeout--) {
		alpha_mb();
		if (pcsp->pcs_flags & PCS_BIP)
			break;
		delay(1000);
	}
	if (timeout == 0)
		printf("%s: processor failed to boot\n", ci->ci_dev->dv_xname);

	/*
	 * ...and now wait for verification that it's running kernel
	 * code.
	 */
	for (timeout = 10000; timeout != 0; timeout--) {
		alpha_mb();
		if (cpus_running & cpumask)
			break;
		delay(1000);
	}
	if (timeout == 0)
		printf("%s: processor failed to hatch\n", ci->ci_dev->dv_xname);
}

void
cpu_halt_secondary(cpu_id)
	u_long cpu_id;
{
	long timeout;
	u_long cpumask = (1UL << cpu_id);

#ifdef DIAGNOSTIC
	if (cpu_id >= hwrpb->rpb_pcs_cnt ||
	    cpu_info[cpu_id].ci_dev == NULL)
		panic("cpu_halt_secondary: bogus cpu_id");
#endif

	alpha_mb();
	if ((cpus_running & cpumask) == 0) {
		/* Processor not running. */
		return;
	}

	/* Send the HALT IPI to the secondary. */
	alpha_send_ipi(cpu_id, ALPHA_IPI_HALT);

	/* ...and wait for it to shut down. */
	for (timeout = 10000; timeout != 0; timeout--) {
		alpha_mb();
		if ((cpus_running & cpumask) == 0)
			return;
		delay(1000);
	}

	/* Erk, secondary failed to halt. */
	printf("WARNING: %s (ID %lu) failed to halt\n",
	    cpu_info[cpu_id].ci_dev->dv_xname, cpu_id);
}

void
cpu_hatch(ci)
	struct cpu_info *ci;
{
	u_long cpumask = (1UL << ci->ci_cpuid);

	/* Set our `curpcb' to reflect our context. */
	curpcb = ci->ci_idle_pcb_paddr;

	/* Mark the kernel pmap active on this processor. */
	alpha_atomic_setbits_q(&pmap_kernel()->pm_cpus, cpumask);

	/* Initialize trap vectors for this processor. */
	trap_init();

	/* Yahoo!  We're running kernel code!  Announce it! */
	printf("%s: processor ID %lu running\n", ci->ci_dev->dv_xname,
	    alpha_pal_whami());
	alpha_atomic_setbits_q(&cpus_running, cpumask);

	/*
	 * Lower interrupt level so that we can get IPIs.  Don't use
	 * spl0() because we don't want to hassle w/ software interrupts
	 * right now.  Note that interrupt() prevents the secondaries
	 * from servicing DEVICE and CLOCK interrupts.
	 */
	(void) alpha_pal_swpipl(ALPHA_PSL_IPL_0);

	/* Ok, so all we do is spin for now... */
	for (;;)
		/* nothing */ ;
}

int
cpu_iccb_send(cpu_id, msg)
	long cpu_id;
	const char *msg;
{
	struct pcs *pcsp = LOCATE_PCS(hwrpb, cpu_id);
	int timeout;
	u_long cpumask = (1UL << cpu_id);

	/* Wait for the ICCB to become available. */
	for (timeout = 10000; timeout != 0; timeout--) {
		alpha_mb();
		if ((hwrpb->rpb_rxrdy & cpumask) == 0)
			break;
		delay(1000);
	}
	if (timeout == 0)
		return (EIO);

	/*
	 * Copy the message into the ICCB, and tell the secondary console
	 * that it's there.  The atomic operation performs a memory barrier.
	 */
	strcpy(pcsp->pcs_iccb.iccb_rxbuf, msg);
	pcsp->pcs_iccb.iccb_rxlen = strlen(msg);
	alpha_atomic_setbits_q(&hwrpb->rpb_rxrdy, cpumask);

	/* Wait for the message to be received. */
	for (timeout = 10000; timeout != 0; timeout--) {
		alpha_mb();
		if ((hwrpb->rpb_rxrdy & cpumask) == 0)
			break;
		delay(1000);
	}
	if (timeout == 0)
		return (EIO);

	return (0);
}

void
cpu_iccb_receive()
{
#if 0	/* Don't bother... we don't get any important messages anyhow. */
	u_int64_t txrdy;
	char *cp1, *cp2, buf[80];
	struct pcs *pcsp;
	u_int cnt;
	long cpu_id;

	txrdy = hwrpb->rpb_txrdy;

	for (cpu_id = 0; cpu_id < hwrpb->rpb_pcs_cnt; cpu_id++) {
		if (txrdy & (1UL << cpu_id)) {
			pcsp = LOCATE_PCS(hwrpb, cpu_id);
			printf("Inter-console message from CPU %lu "
			    "HALT REASON = 0x%lx, FLAGS = 0x%lx\n",
			    cpu_id, pcsp->pcs_halt_reason, pcsp->pcs_flags);
			
			cnt = pcsp->pcs_iccb.iccb_txlen;
			if (cnt >= 80) {
				printf("Malformed inter-console message\n");
				continue;
			}
			cp1 = pcsp->pcs_iccb.iccb_txbuf;
			cp2 = buf;
			while (cnt--) {
				if (*cp1 != '\r' && *cp1 != '\n')
					*cp2++ = *cp1;
				cp1++;
			}
			*cp2 = '\0';
			printf("Message from CPU %lu: %s\n", cpu_id, buf);
		}
	}
#endif /* 0 */
	hwrpb->rpb_txrdy = 0;
	alpha_mb();
}
#endif /* MULTIPROCESSOR */
