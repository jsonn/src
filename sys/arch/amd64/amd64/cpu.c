/* $NetBSD: cpu.c,v 1.15.2.2 2007/06/09 23:54:50 ad Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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

/*
 * Copyright (c) 1999 Stefan Grefen
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.15.2.2 2007/06/09 23:54:50 ad Exp $");

#include "opt_ddb.h"
#include "opt_multiprocessor.h"
#include "opt_mpbios.h"		/* for MPDEBUG */
#include "opt_mtrr.h"

#include "lapic.h"
#include "ioapic.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/mpbiosvar.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#include <machine/segments.h>
#include <machine/gdt.h>
#include <machine/mtrr.h>
#include <machine/pio.h>

#if NLAPIC > 0
#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#include <dev/ic/mc146818reg.h>
#include <i386/isa/nvram.h>
#include <dev/isa/isareg.h>

int     cpu_match(struct device *, struct cfdata *, void *);
void    cpu_attach(struct device *, struct device *, void *);

struct cpu_softc {
	struct device sc_dev;		/* device tree glue */
	struct cpu_info *sc_info;	/* pointer to CPU info */
};

int mp_cpu_start(struct cpu_info *); 
void mp_cpu_start_cleanup(struct cpu_info *);
const struct cpu_functions mp_cpu_funcs = { mp_cpu_start, NULL,
					    mp_cpu_start_cleanup };


CFATTACH_DECL(cpu, sizeof(struct cpu_softc),
    cpu_match, cpu_attach, NULL, NULL);

/*
 * Statically-allocated CPU info for the primary CPU (or the only
 * CPU, on uniprocessors).  The CPU info list is initialized to
 * point at it.
 */
struct cpu_info cpu_info_primary = {
	.ci_dev = 0,
	.ci_self = &cpu_info_primary,
	.ci_self200 = (uint8_t *)&cpu_info_primary + 0x200,
};

struct cpu_info *cpu_info_list = &cpu_info_primary;

u_int32_t cpus_attached = 0;

#ifdef MULTIPROCESSOR
/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */
struct cpu_info *cpu_info[X86_MAXPROCS] = { &cpu_info_primary };

u_int32_t cpus_running = 0;

extern char x86_64_doubleflt_stack[];

void    	cpu_hatch(void *);
static void    	cpu_boot_secondary(struct cpu_info *ci);
static void    	cpu_start_secondary(struct cpu_info *ci);
static void	cpu_copy_trampoline(void);

/*
 * Runs once per boot once multiprocessor goo has been detected and
 * the local APIC on the boot processor has been mapped.
 *
 * Called from lapic_boot_init() (from mpbios_scan()).
 */
void
cpu_init_first(void)
{
	int cpunum = lapic_cpu_number();

	if (cpunum != 0) {
		cpu_info[0] = NULL;
		cpu_info[cpunum] = &cpu_info_primary;
	}

	cpu_copy_trampoline();
}
#endif

int
cpu_match(struct device *parent, struct cfdata *match, void *aux)
{

	return 1;
}

static void
cpu_vm_init(struct cpu_info *ci)
{
	int ncolors = 2, i;

	for (i = CAI_ICACHE; i <= CAI_L2CACHE; i++) {
		struct x86_cache_info *cai;
		int tcolors;

		cai = &ci->ci_cinfo[i];

		tcolors = atop(cai->cai_totalsize);
		switch(cai->cai_associativity) {
		case 0xff:
			tcolors = 1; /* fully associative */
			break;
		case 0:
		case 1:
			break;
		default:
			tcolors /= cai->cai_associativity;
		}
		ncolors = max(ncolors, tcolors);
	}

	/*
	 * Knowing the size of the largest cache on this CPU, re-color
	 * our pages.
	 */
	if (ncolors <= uvmexp.ncolors)
		return;
	printf("%s: %d page colors\n", ci->ci_dev->dv_xname, ncolors);
	uvm_page_recolor(ncolors);
}


void
cpu_attach(struct device *parent, struct device *self, void *aux)
{
	struct cpu_softc *sc = (void *) self;
	struct cpu_attach_args *caa = aux;
	struct cpu_info *ci;
#if defined(MULTIPROCESSOR)
	int cpunum = caa->cpu_number;
#endif

	/*
	 * If we're an Application Processor, allocate a cpu_info
	 * structure, otherwise use the primary's.
	 */
	if (caa->cpu_role == CPU_ROLE_AP) {
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK);
		memset(ci, 0, sizeof(*ci));
#if defined(MULTIPROCESSOR)
		if (cpu_info[cpunum] != NULL)
			panic("CPU at apic id %d already attached?", cpunum);
		cpu_info[cpunum] = ci;
#endif
#ifdef TRAPLOG
		ci->ci_tlog_base = malloc(sizeof(struct tlog),
		    M_DEVBUF, M_WAITOK);
#endif
	} else {
		ci = &cpu_info_primary;
#if defined(MULTIPROCESSOR)
		if (cpunum != lapic_cpu_number()) {
			panic("%s: running CPU is at apic %d"
			    " instead of at expected %d",
			    sc->sc_dev.dv_xname, lapic_cpu_number(), cpunum);
		}
#endif
	}

	ci->ci_self = ci;
	ci->ci_self200 = (uint8_t *)ci + 0x200;
	sc->sc_info = ci;

	ci->ci_dev = self;
	ci->ci_apicid = caa->cpu_number;
#ifdef MULTIPROCESSOR
	ci->ci_cpuid = ci->ci_apicid;
#else
	ci->ci_cpuid = 0;	/* False for APs, but they're not used anyway */
#endif
	ci->ci_func = caa->cpu_func;

	simple_lock_init(&ci->ci_slock);

	if (caa->cpu_role == CPU_ROLE_AP) {
#if defined(MULTIPROCESSOR)
		int error;

		error = mi_cpu_attach(ci);
		if (error != 0) {
			aprint_normal("\n");
			aprint_error("%s: mi_cpu_attach failed with %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
#endif
	} else {
		KASSERT(ci->ci_data.cpu_idlelwp != NULL);
	}

	/* further PCB init done later. */

	printf(": ");

	switch (caa->cpu_role) {
	case CPU_ROLE_SP:
		printf("(uniprocessor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_SP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
		cpu_init(ci);
		break;

	case CPU_ROLE_BP:
		printf("(boot processor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_BSP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
		cpu_init(ci);

#if NLAPIC > 0
		/*
		 * Enable local apic
		 */
		lapic_enable();
		lapic_calibrate_timer(ci);
#endif
#if NIOAPIC > 0
		ioapic_bsp_id = caa->cpu_number;
#endif
		break;

	case CPU_ROLE_AP:
		/*
		 * report on an AP
		 */
		printf("(application processor)\n");

#if defined(MULTIPROCESSOR)
		cpu_intr_init(ci);
		gdt_alloc_cpu(ci);
		cpu_start_secondary(ci);
		if (ci->ci_flags & CPUF_PRESENT) {
			identifycpu(ci);
			ci->ci_next = cpu_info_list->ci_next;
			cpu_info_list->ci_next = ci;
		}
#else
		printf("%s: not started\n", sc->sc_dev.dv_xname);
#endif
		break;

	default:
		panic("unknown processor type??\n");
	}
	cpu_vm_init(ci);

	cpus_attached |= (1 << ci->ci_cpuid);

#if defined(MULTIPROCESSOR)
	if (mp_verbose) {
		struct lwp *l = ci->ci_data.cpu_idlelwp;

		aprint_verbose("%s: idle lwp at %p, idle sp at 0x%lx\n",
		    sc->sc_dev.dv_xname, l, l->l_addr->u_pcb.pcb_rsp);
	}
#endif
}

/*
 * Initialize the processor appropriately.
 */

void
cpu_init(struct cpu_info *ci)
{
	/* configure the CPU if needed */
	if (ci->cpu_setup != NULL)
		(*ci->cpu_setup)(ci);

	lcr0(rcr0() | CR0_WP);
	lcr4(rcr4() | CR4_PGE | CR4_OSFXSR | CR4_OSXMMEXCPT);

#ifdef MTRR
	if ((ci->ci_flags & CPUF_AP) == 0)
		i686_mtrr_init_first();
	mtrr_init_cpu(ci);
#endif

#ifdef MULTIPROCESSOR
	ci->ci_flags |= CPUF_RUNNING;
	cpus_running |= 1 << ci->ci_cpuid;
#endif
}


#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors(void)
{
	struct cpu_info *ci;
	u_long i;

	for (i=0; i < X86_MAXPROCS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_data.cpu_idlelwp == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		if (ci->ci_flags & (CPUF_BSP|CPUF_SP|CPUF_PRIMARY))
			continue;
		cpu_boot_secondary(ci);
	}
}

static void
cpu_init_idle_lwp(struct cpu_info *ci)
{
	struct lwp *l = ci->ci_data.cpu_idlelwp;
	struct pcb *pcb = &l->l_addr->u_pcb;

	pcb->pcb_cr0 = rcr0();
}

void
cpu_init_idle_lwps(void)
{
	struct cpu_info *ci;
	u_long i;

	for (i = 0; i < X86_MAXPROCS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_data.cpu_idlelwp == NULL)
			continue;
		if ((ci->ci_flags & CPUF_PRESENT) == 0)
			continue;
		cpu_init_idle_lwp(ci);
	}
}

void
cpu_start_secondary(struct cpu_info *ci)
{
	int i;
	struct pmap *kmp = pmap_kernel();
	extern u_int64_t mp_pdirpa;
	extern vaddr_t lo32_vaddr;
	extern paddr_t lo32_paddr;

	/*
	 * The initial PML4 pointer must be below 4G, so if the
	 * current one isn't, use a "bounce buffer"
	 */
	if (kmp->pm_pdirpa > 0xffffffff) {
		memcpy((void *)lo32_vaddr, kmp->pm_pdir, PAGE_SIZE);
		mp_pdirpa = lo32_paddr;
	} else
		mp_pdirpa = kmp->pm_pdirpa;

	ci->ci_flags |= CPUF_AP;

	aprint_debug("%s: starting\n", ci->ci_dev->dv_xname);

	ci->ci_curlwp = ci->ci_data.cpu_idlelwp;
	CPU_STARTUP(ci);

	/*
	 * wait for it to become ready
	 */
	for (i = 100000; (!(ci->ci_flags & CPUF_PRESENT)) && i>0;i--) {
		delay(10);
	}
	if (! (ci->ci_flags & CPUF_PRESENT)) {
		aprint_error("%s: failed to become ready\n", ci->ci_dev->dv_xname);
#if defined(MPDEBUG) && defined(DDB)
		printf("dropping into debugger; continue from here to resume boot\n");
		Debugger();
#endif
	}

	CPU_START_CLEANUP(ci);
}

void
cpu_boot_secondary(struct cpu_info *ci)
{
	int i;

	ci->ci_flags |= CPUF_GO; /* XXX atomic */

	for (i = 100000; (!(ci->ci_flags & CPUF_RUNNING)) && i>0;i--) {
		delay(10);
	}
	if (! (ci->ci_flags & CPUF_RUNNING)) {
		aprint_error("CPU failed to start\n");
#if defined(MPDEBUG) && defined(DDB)
		printf("dropping into debugger; continue from here to resume boot\n");
		Debugger();
#endif
	}
}

/*
 * The CPU ends up here when its ready to run
 * This is called from code in mptramp.s; at this point, we are running
 * in the idle pcb/idle stack of the new CPU.  When this function returns,
 * this processor will enter the idle loop and start looking for work.
 *
 * XXX should share some of this with init386 in machdep.c
 */
void
cpu_hatch(void *v)
{
	struct cpu_info *ci = (struct cpu_info *)v;
	int s;

	cpu_init_msrs(ci);

	cpu_probe_features(ci);
	cpu_feature &= ci->ci_feature_flags;

#ifdef DEBUG
	if (ci->ci_flags & CPUF_PRESENT)
		panic("%s: already running!?", ci->ci_dev->dv_xname);
#endif

	ci->ci_flags |= CPUF_PRESENT;

	lapic_enable();
	lapic_initclocks();

	while ((ci->ci_flags & CPUF_GO) == 0)
		delay(10);
#ifdef DEBUG
	if (ci->ci_flags & CPUF_RUNNING)
		panic("%s: already running!?", ci->ci_dev->dv_xname);
#endif

	lcr0(ci->ci_data.cpu_idlelwp->l_addr->u_pcb.pcb_cr0);
	cpu_init_idt();
	lapic_set_lvt();
	gdt_init_cpu(ci);
	fpuinit(ci);

	lldt(GSYSSEL(GLDT_SEL, SEL_KPL));

	cpu_init(ci);

	s = splhigh();
	lcr8(0);
	enable_intr();

	aprint_debug("%s: CPU %u running\n",ci->ci_dev->dv_xname, ci->ci_cpuid);
	microtime(&ci->ci_schedstate.spc_runtime);
	splx(s);
}

#if defined(DDB)

#include <ddb/db_output.h>
#include <machine/db_machdep.h>

/*
 * Dump CPU information from ddb.
 */
void
cpu_debug_dump(void)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	db_printf("addr		dev	id	flags	ipis	curproc		fpcurproc\n");
	for (CPU_INFO_FOREACH(cii, ci)) {
		db_printf("%p	%s	%u	%x	%x	%10p	%10p\n",
		    ci,
		    ci->ci_dev == NULL ? "BOOT" : ci->ci_dev->dv_xname,
		    ci->ci_cpuid,
		    ci->ci_flags, ci->ci_ipis,
		    ci->ci_curlwp,
		    ci->ci_fpcurlwp);
	}
}
#endif

static void
cpu_copy_trampoline(void)
{
	/*
	 * Copy boot code.
	 */
	extern u_char cpu_spinup_trampoline[];
	extern u_char cpu_spinup_trampoline_end[];
	pmap_kenter_pa((vaddr_t)MP_TRAMPOLINE,	/* virtual */
	    (paddr_t)MP_TRAMPOLINE,	/* physical */
	    VM_PROT_ALL);		/* protection */
	memcpy((void *)MP_TRAMPOLINE,
	    cpu_spinup_trampoline,
	    cpu_spinup_trampoline_end-cpu_spinup_trampoline);
}

#endif


int
mp_cpu_start(struct cpu_info *ci)
{
#if NLAPIC > 0
	int error;
#endif
	unsigned short dwordptr[2];

	/*
	 * "The BSP must initialize CMOS shutdown code to 0Ah ..."
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_JUMP);

	/*
	 * "and the warm reset vector (DWORD based at 40:67) to point
	 * to the AP startup code ..."
	 */

	dwordptr[0] = 0;
	dwordptr[1] = MP_TRAMPOLINE >> 4;

	pmap_kenter_pa (0, 0, VM_PROT_READ|VM_PROT_WRITE);
	memcpy ((u_int8_t *) 0x467, dwordptr, 4);
	pmap_kremove (0, PAGE_SIZE);

#if NLAPIC > 0
	/*
	 * ... prior to executing the following sequence:"
	 */

	if (ci->ci_flags & CPUF_AP) {
		if ((error = x86_ipi_init(ci->ci_apicid)) != 0)
			return error;

		delay(10000);

		if (cpu_feature & CPUID_APIC) {

			if ((error = x86_ipi(MP_TRAMPOLINE/PAGE_SIZE,
					     ci->ci_apicid,
					     LAPIC_DLMODE_STARTUP)) != 0)
				return error;
			delay(200);

			if ((error = x86_ipi(MP_TRAMPOLINE/PAGE_SIZE,
					     ci->ci_apicid,
					     LAPIC_DLMODE_STARTUP)) != 0)
				return error;
			delay(200);
		}
	}
#endif
	return 0;
}

void
mp_cpu_start_cleanup(struct cpu_info *ci)
{
	/*
	 * Ensure the NVRAM reset byte contains something vaguely sane.
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_RST);
}

typedef void (vector)(void);
extern vector Xsyscall, Xsyscall32;

void
cpu_init_msrs(struct cpu_info *ci)
{
	wrmsr(MSR_STAR,
	    ((uint64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	    ((uint64_t)LSEL(LSYSRETBASE_SEL, SEL_UPL) << 48));
	wrmsr(MSR_LSTAR, (uint64_t)Xsyscall);
	wrmsr(MSR_CSTAR, (uint64_t)Xsyscall32);
	wrmsr(MSR_SFMASK, PSL_NT|PSL_T|PSL_I|PSL_C);

	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_GSBASE, (u_int64_t)ci);
	wrmsr(MSR_KERNELGSBASE, 0);

	if (cpu_feature & CPUID_NOX)
		wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_NXE);
}
