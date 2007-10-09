/* $NetBSD: cpu.c,v 1.1.2.3 2007/10/09 15:22:07 ad Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.1.2.3 2007/10/09 15:22:07 ad Exp $");

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

#ifdef i386
#include <machine/tlog.h>
#endif

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
#ifdef TRAPLOG
struct tlog tlog_primary;
#endif
struct cpu_info cpu_info_primary = {
	.ci_dev = 0,
	.ci_self = &cpu_info_primary,
	.ci_idepth = -1,
	.ci_curlwp = &lwp0,
#ifdef TRAPLOG
	.ci_tlog_base = &tlog_primary,
#endif /* !TRAPLOG */
};

struct cpu_info *cpu_info_list = &cpu_info_primary;

static void	cpu_set_tss_gates(struct cpu_info *ci);

#ifdef i386
static void	cpu_init_tss(struct i386tss *, void *, void *);
#endif

uint32_t cpus_attached = 0;

extern char x86_64_doubleflt_stack[];

#ifdef MULTIPROCESSOR
/*
 * Array of CPU info structures.  Must be statically-allocated because
 * curproc, etc. are used early.
 */
struct cpu_info *cpu_info[X86_MAXPROCS] = { &cpu_info_primary, };

uint32_t cpus_running = 0;

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
cpu_init_first()
{
	int cpunum = lapic_cpu_number();

	if (cpunum != 0) {
		cpu_info[0] = NULL;
		cpu_info[cpunum] = &cpu_info_primary;
	}

	cpu_info_primary.ci_cpuid = cpunum;
	cpu_copy_trampoline();
}
#endif

int
cpu_match(struct device *parent, struct cfdata *match,
    void *aux)
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
	aprint_verbose("%s: %d page colors\n", ci->ci_dev->dv_xname, ncolors);
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
		aprint_naive(": Application Processor\n");
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK);
		memset(ci, 0, sizeof(*ci));
#if defined(MULTIPROCESSOR)
		if (cpu_info[cpunum] != NULL) {
			printf("\n");
			panic("cpu at apic id %d already attached?", cpunum);
		}
		cpu_info[cpunum] = ci;
#endif
#ifdef TRAPLOG
		ci->ci_tlog_base = malloc(sizeof(struct tlog),
		    M_DEVBUF, M_WAITOK);
#endif
	} else {
		aprint_naive(": %s Processor\n",
		    caa->cpu_role == CPU_ROLE_SP ? "Single" : "Boot");
		ci = &cpu_info_primary;
#if defined(MULTIPROCESSOR)
		if (cpunum != lapic_cpu_number()) {
			printf("\n");
			panic("%s: running CPU is at apic %d"
			    " instead of at expected %d",
			    sc->sc_dev.dv_xname, lapic_cpu_number(), cpunum);
		}
#endif
	}

	ci->ci_self = ci;
	sc->sc_info = ci;

	ci->ci_dev = self;
	ci->ci_apicid = caa->cpu_number;
#ifdef MULTIPROCESSOR
	ci->ci_cpuid = ci->ci_apicid;
#else
	ci->ci_cpuid = 0;	/* False for APs, but they're not used anyway */
#endif
	ci->ci_cpumask = (1 << ci->ci_cpuid);
	ci->ci_func = caa->cpu_func;

	if (caa->cpu_role == CPU_ROLE_AP) {
#ifdef MULTIPROCESSOR
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

#ifdef i386
	pmap_reference(pmap_kernel());
	ci->ci_pmap = pmap_kernel();
	ci->ci_tlbstate = TLBSTATE_STALE;
#endif

	/* further PCB init done later. */

	switch (caa->cpu_role) {
	case CPU_ROLE_SP:
		aprint_normal(": (uniprocessor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_SP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
		cpu_init(ci);
		cpu_set_tss_gates(ci);
		pmap_cpu_init_late(ci);
		break;

	case CPU_ROLE_BP:
		aprint_normal(": (boot processor)\n");
		ci->ci_flags |= CPUF_PRESENT | CPUF_BSP | CPUF_PRIMARY;
		cpu_intr_init(ci);
		identifycpu(ci);
		cpu_init(ci);
		cpu_set_tss_gates(ci);
		pmap_cpu_init_late(ci);
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
		aprint_normal(": (application processor)\n");

#if defined(MULTIPROCESSOR)
		cpu_intr_init(ci);
		gdt_alloc_cpu(ci);
		cpu_set_tss_gates(ci);
		pmap_cpu_init_early(ci);
		pmap_cpu_init_late(ci);
		cpu_start_secondary(ci);
		if (ci->ci_flags & CPUF_PRESENT) {
			identifycpu(ci);
			ci->ci_next = cpu_info_list->ci_next;
			cpu_info_list->ci_next = ci;
		}
#else
		aprint_normal("%s: not started\n", sc->sc_dev.dv_xname);
#endif
		break;

	default:
		printf("\n");
		panic("unknown processor type??\n");
	}
	cpu_vm_init(ci);

	cpus_attached |= ci->ci_cpumask;

#if defined(MULTIPROCESSOR)
	if (mp_verbose) {
		struct lwp *l = ci->ci_data.cpu_idlelwp;

		aprint_verbose(
		    "%s: idle lwp at %p, idle sp at %p\n",
		    sc->sc_dev.dv_xname, l,
#ifdef i386
		    (void *)l->l_addr->u_pcb.pcb_esp
#else
		    (void *)l->l_addr->u_pcb.pcb_rsp
#endif
		);
	}
#endif
}

/*
 * Initialize the processor appropriately.
 */

void
cpu_init(ci)
	struct cpu_info *ci;
{
	/* configure the CPU if needed */
	if (ci->cpu_setup != NULL)
		(*ci->cpu_setup)(ci);

#ifdef i386
	/*
	 * On a 486 or above, enable ring 0 write protection.
	 */
	if (ci->ci_cpu_class >= CPUCLASS_486)
		lcr0(rcr0() | CR0_WP);
#else
	lcr0(rcr0() | CR0_WP);
#endif

	/*
	 * On a P6 or above, enable global TLB caching if the
	 * hardware supports it.
	 */
	if (cpu_feature & CPUID_PGE)
		lcr4(rcr4() | CR4_PGE);	/* enable global TLB caching */

	/*
	 * If we have FXSAVE/FXRESTOR, use them.
	 */
	if (cpu_feature & CPUID_FXSR) {
		lcr4(rcr4() | CR4_OSFXSR);

		/*
		 * If we have SSE/SSE2, enable XMM exceptions.
		 */
		if (cpu_feature & (CPUID_SSE|CPUID_SSE2))
			lcr4(rcr4() | CR4_OSXMMEXCPT);
	}

#ifdef MTRR
	/*
	 * On a P6 or above, initialize MTRR's if the hardware supports them.
	 */
	if (cpu_feature & CPUID_MTRR) {
		if ((ci->ci_flags & CPUF_AP) == 0)
			i686_mtrr_init_first();
		mtrr_init_cpu(ci);
	}

#ifdef i386
	if (strcmp((char *)(ci->ci_vendor), "AuthenticAMD") == 0) {
		/*
		 * Must be a K6-2 Step >= 7 or a K6-III.
		 */
		if (CPUID2FAMILY(ci->ci_signature) == 5) {
			if (CPUID2MODEL(ci->ci_signature) > 8 ||
			    (CPUID2MODEL(ci->ci_signature) == 8 &&
			     CPUID2STEPPING(ci->ci_signature) >= 7)) {
				mtrr_funcs = &k6_mtrr_funcs;
				k6_mtrr_init_first();
				mtrr_init_cpu(ci);
			}
		}
	}
#endif	/* i386 */
#endif /* MTRR */

#ifdef MULTIPROCESSOR
	ci->ci_flags |= CPUF_RUNNING;
	cpus_running |= ci->ci_cpumask;
#endif
}

bool x86_mp_online;

#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors()
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

	x86_mp_online = true;
}

static void
cpu_init_idle_lwp(struct cpu_info *ci)
{
	struct lwp *l = ci->ci_data.cpu_idlelwp;
	struct pcb *pcb = &l->l_addr->u_pcb;

	pcb->pcb_cr0 = rcr0();
}

void
cpu_init_idle_lwps()
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
cpu_start_secondary(ci)
	struct cpu_info *ci;
{
	int i;
	struct pmap *kpm = pmap_kernel();
	extern paddr_t mp_pdirpa;

#ifdef __x86_64__
	/*
	 * The initial PML4 pointer must be below 4G, so if the
	 * current one isn't, use a "bounce buffer"
	 *
	 * XXX move elsewhere, not per CPU.
	 */
	if (kpm->pm_pdirpa > 0xffffffff) {
		extern vaddr_t lo32_vaddr;
		extern paddr_t lo32_paddr;
		memcpy((void *)lo32_vaddr, kpm->pm_pdir, PAGE_SIZE);
		mp_pdirpa = lo32_paddr;
	} else
#endif
		mp_pdirpa = kpm->pm_pdirpa;

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
		aprint_error("%s: failed to become ready\n",
		    ci->ci_dev->dv_xname);
#if defined(MPDEBUG) && defined(DDB)
		printf("dropping into debugger; continue from here to resume boot\n");
		Debugger();
#endif
	}

	CPU_START_CLEANUP(ci);
}

void
cpu_boot_secondary(ci)
	struct cpu_info *ci;
{
	int i;

	ci->ci_flags |= CPUF_GO; /* XXX atomic */

	for (i = 100000; (!(ci->ci_flags & CPUF_RUNNING)) && i>0;i--) {
		delay(10);
	}
	if (! (ci->ci_flags & CPUF_RUNNING)) {
		aprint_error("%s: failed to start\n", ci->ci_dev->dv_xname);
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

#ifdef __x86_64__
	cpu_init_msrs(ci);
#endif
	cpu_probe_features(ci);
	cpu_feature &= ci->ci_feature_flags;
	cpu_feature2 &= ci->ci_feature2_flags;

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

#ifdef i386
	npxinit(ci);
	lldt(GSEL(GLDT_SEL, SEL_KPL));
#else
	fpuinit(ci);
	lldt(GSYSSEL(GLDT_SEL, SEL_KPL));
#endif

	cpu_init(ci);

	s = splhigh();
#ifdef i386
	lapic_tpr = 0;
#else
	lcr8(0);
#endif
	x86_enable_intr();
	splx(s);

	aprint_debug("%s: CPU %ld running\n", ci->ci_dev->dv_xname,
	    (long)ci->ci_cpuid);
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
		db_printf("%p	%s	%ld	%x	%x	%10p	%10p\n",
		    ci,
		    ci->ci_dev == NULL ? "BOOT" : ci->ci_dev->dv_xname,
		    (long)ci->ci_cpuid,
		    ci->ci_flags, ci->ci_ipis,
		    ci->ci_curlwp,
		    ci->ci_fpcurlwp);
	}
}
#endif

static void
cpu_copy_trampoline()
{
	/*
	 * Copy boot code.
	 */
	extern u_char cpu_spinup_trampoline[];
	extern u_char cpu_spinup_trampoline_end[];
	pmap_kenter_pa((vaddr_t)MP_TRAMPOLINE,	/* virtual */
	    (paddr_t)MP_TRAMPOLINE,	/* physical */
	    VM_PROT_ALL);		/* protection */
	pmap_update(pmap_kernel());
	memcpy((void *)MP_TRAMPOLINE,
	    cpu_spinup_trampoline,
	    cpu_spinup_trampoline_end-cpu_spinup_trampoline);
}

#endif

#ifdef i386
static void
cpu_init_tss(struct i386tss *tss, void *stack, void *func)
{
	memset(tss, 0, sizeof *tss);
	tss->tss_esp0 = tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	tss->__tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	tss->tss_fs = GSEL(GCPU_SEL, SEL_KPL);
	tss->tss_gs = tss->__tss_es = tss->__tss_ds =
	    tss->__tss_ss = GSEL(GDATA_SEL, SEL_KPL);
	tss->tss_cr3 = pmap_kernel()->pm_pdirpa;
	tss->tss_esp = (int)((char *)stack + USPACE - 16);
	tss->tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	tss->__tss_eflags = PSL_MBO | PSL_NT;	/* XXX not needed? */
	tss->__tss_eip = (int)func;
}

/* XXX */
#define IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector IDTVEC(tss_trap08);
#ifdef DDB
extern vector Xintrddbipi;
extern int ddb_vec;
#endif

static void
cpu_set_tss_gates(struct cpu_info *ci)
{
	struct segment_descriptor sd;

	ci->ci_doubleflt_stack = (char *)uvm_km_alloc(kernel_map, USPACE, 0,
	    UVM_KMF_WIRED);
	cpu_init_tss(&ci->ci_doubleflt_tss, ci->ci_doubleflt_stack,
	    IDTVEC(tss_trap08));
	setsegment(&sd, &ci->ci_doubleflt_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GTRAPTSS_SEL].sd = sd;
	setgate(&idt[8], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GTRAPTSS_SEL, SEL_KPL));

#if defined(DDB) && defined(MULTIPROCESSOR)
	/*
	 * Set up separate handler for the DDB IPI, so that it doesn't
	 * stomp on a possibly corrupted stack.
	 *
	 * XXX overwriting the gate set in db_machine_init.
	 * Should rearrange the code so that it's set only once.
	 */
	ci->ci_ddbipi_stack = (char *)uvm_km_alloc(kernel_map, USPACE, 0,
	    UVM_KMF_WIRED);
	cpu_init_tss(&ci->ci_ddbipi_tss, ci->ci_ddbipi_stack,
	    Xintrddbipi);

	setsegment(&sd, &ci->ci_ddbipi_tss, sizeof(struct i386tss) - 1,
	    SDT_SYS386TSS, SEL_KPL, 0, 0);
	ci->ci_gdt[GIPITSS_SEL].sd = sd;

	setgate(&idt[ddb_vec], NULL, 0, SDT_SYSTASKGT, SEL_KPL,
	    GSEL(GIPITSS_SEL, SEL_KPL));
#endif
}
#else
static void
cpu_set_tss_gates(struct cpu_info *ci)
{

}
#endif	/* i386 */


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

	pmap_kenter_pa(0, 0, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	memcpy((uint8_t *)0x467, dwordptr, 4);
	pmap_kremove(0, PAGE_SIZE);
	pmap_update(pmap_kernel());

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

#ifdef __x86_64__
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
#endif	/* __x86_64__ */
