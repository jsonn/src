/*	$NetBSD: cpu.c,v 1.51.2.1 2004/08/03 10:32:29 skrll Exp $	*/

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpu.c
 *
 * Probing and configuration for the master CPU
 *
 * Created      : 10/10/95
 */

#include "opt_armfpe.h"
#include "opt_multiprocessor.h"

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.51.2.1 2004/08/03 10:32:29 skrll Exp $");

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <uvm/uvm_extern.h>
#include <machine/cpu.h>

#include <arm/cpuconf.h>
#include <arm/undefined.h>

#ifdef ARMFPE
#include <machine/bootconfig.h> /* For boot args */
#include <arm/fpe-arm/armfpe.h>
#endif

char cpu_model[256];

/* Prototypes */
void identify_arm_cpu(struct device *dv, struct cpu_info *);

/*
 * Identify the master (boot) CPU
 */
  
void
cpu_attach(struct device *dv)
{
	int usearmfpe;

	usearmfpe = 1;	/* when compiled in, its enabled by default */

	curcpu()->ci_dev = dv;

	evcnt_attach_dynamic(&curcpu()->ci_arm700bugcount, EVCNT_TYPE_MISC,
	    NULL, dv->dv_xname, "arm700swibug");
	
	/* Get the CPU ID from coprocessor 15 */

	curcpu()->ci_arm_cpuid = cpu_id();
	curcpu()->ci_arm_cputype = curcpu()->ci_arm_cpuid & CPU_ID_CPU_MASK;
	curcpu()->ci_arm_cpurev =
	    curcpu()->ci_arm_cpuid & CPU_ID_REVISION_MASK;

	identify_arm_cpu(dv, curcpu());

	if (curcpu()->ci_arm_cputype == CPU_ID_SA110 &&
	    curcpu()->ci_arm_cpurev < 3) {
		aprint_normal("%s: SA-110 with bugged STM^ instruction\n",
		       dv->dv_xname);
	}

#ifdef CPU_ARM8
	if ((curcpu()->ci_arm_cpuid & CPU_ID_CPU_MASK) == CPU_ID_ARM810) {
		int clock = arm8_clock_config(0, 0);
		char *fclk;
		aprint_normal("%s: ARM810 cp15=%02x", dv->dv_xname, clock);
		aprint_normal(" clock:%s", (clock & 1) ? " dynamic" : "");
		aprint_normal("%s", (clock & 2) ? " sync" : "");
		switch ((clock >> 2) & 3) {
		case 0:
			fclk = "bus clock";
			break;
		case 1:
			fclk = "ref clock";
			break;
		case 3:
			fclk = "pll";
			break;
		default:
			fclk = "illegal";
			break;
		}
		aprint_normal(" fclk source=%s\n", fclk);
 	}
#endif

#ifdef ARMFPE
	/*
	 * Ok now we test for an FPA
	 * At this point no floating point emulator has been installed.
	 * This means any FP instruction will cause undefined exception.
	 * We install a temporay coproc 1 handler which will modify
	 * undefined_test if it is called.
	 * We then try to read the FP status register. If undefined_test
	 * has been decremented then the instruction was not handled by
	 * an FPA so we know the FPA is missing. If undefined_test is
	 * still 1 then we know the instruction was handled by an FPA.
	 * We then remove our test handler and look at the
	 * FP status register for identification.
	 */
 
	/*
	 * Ok if ARMFPE is defined and the boot options request the 
	 * ARM FPE then it will be installed as the FPE.
	 * This is just while I work on integrating the new FPE.
	 * It means the new FPE gets installed if compiled int (ARMFPE
	 * defined) and also gives me a on/off option when I boot in
	 * case the new FPE is causing panics.
	 */


	if (boot_args)
		get_bootconf_option(boot_args, "armfpe",
		    BOOTOPT_TYPE_BOOLEAN, &usearmfpe);
	if (usearmfpe)
		initialise_arm_fpe();
#endif
}

enum cpu_class {
	CPU_CLASS_NONE,
	CPU_CLASS_ARM2,
	CPU_CLASS_ARM2AS,
	CPU_CLASS_ARM3,
	CPU_CLASS_ARM6,
	CPU_CLASS_ARM7,
	CPU_CLASS_ARM7TDMI,
	CPU_CLASS_ARM8,
	CPU_CLASS_ARM9TDMI,
	CPU_CLASS_ARM9ES,
	CPU_CLASS_ARM10E,
	CPU_CLASS_SA1,
	CPU_CLASS_XSCALE
};

static const char * const generic_steppings[16] = {
	"rev 0",	"rev 1",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa110_steppings[16] = {
	"rev 0",	"step J",	"step K",	"step S",
	"step T",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa1100_steppings[16] = {
	"rev 0",	"step B",	"step C",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"step D",	"step E",	"rev 10"	"step G",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa1110_steppings[16] = {
	"step A-0",	"rev 1",	"rev 2",	"rev 3",
	"step B-0",	"step B-1",	"step B-2",	"step B-3",
	"step B-4",	"step B-5",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const ixp12x0_steppings[16] = {
	"(IXP1200 step A)",		"(IXP1200 step B)",
	"rev 2",			"(IXP1200 step C)",
	"(IXP1200 step D)",		"(IXP1240/1250 step A)",
	"(IXP1240 step B)",		"(IXP1250 step B)",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const xscale_steppings[16] = {
	"step A-0",	"step A-1",	"step B-0",	"step C-0",
	"step D-0",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const i80321_steppings[16] = {
	"step A-0",	"step B-0",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

/* Steppings for PXA2[15]0 */
static const char * const pxa2x0_steppings[16] = {
	"step A-0",	"step A-1",	"step B-0",	"step B-1",
	"step B-2",	"step C-0",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

/* Steppings for PXA255/26x.
 * rev 5: PXA26x B0, rev 6: PXA255 A0  
 */
static const char * const pxa255_steppings[16] = {
	"rev 0",	"rev 1",	"rev 2",	"step A-0",
	"rev 4",	"step B-0",	"step A-0",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const ixp425_steppings[16] = {
	"step 0",	"rev 1",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

struct cpuidtab {
	u_int32_t	cpuid;
	enum		cpu_class cpu_class;
	const char	*cpu_name;
	const char * const *cpu_steppings;
};

const struct cpuidtab cpuids[] = {
	{ CPU_ID_ARM2,		CPU_CLASS_ARM2,		"ARM2",
	  generic_steppings },
	{ CPU_ID_ARM250,	CPU_CLASS_ARM2AS,	"ARM250",
	  generic_steppings },

	{ CPU_ID_ARM3,		CPU_CLASS_ARM3,		"ARM3",
	  generic_steppings },

	{ CPU_ID_ARM600,	CPU_CLASS_ARM6,		"ARM600",
	  generic_steppings },
	{ CPU_ID_ARM610,	CPU_CLASS_ARM6,		"ARM610",
	  generic_steppings },
	{ CPU_ID_ARM620,	CPU_CLASS_ARM6,		"ARM620",
	  generic_steppings },

	{ CPU_ID_ARM700,	CPU_CLASS_ARM7,		"ARM700",
	  generic_steppings },
	{ CPU_ID_ARM710,	CPU_CLASS_ARM7,		"ARM710",
	  generic_steppings },
	{ CPU_ID_ARM7500,	CPU_CLASS_ARM7,		"ARM7500",
	  generic_steppings },
	{ CPU_ID_ARM710A,	CPU_CLASS_ARM7,		"ARM710a",
	  generic_steppings },
	{ CPU_ID_ARM7500FE,	CPU_CLASS_ARM7,		"ARM7500FE",
	  generic_steppings },
	{ CPU_ID_ARM710T,	CPU_CLASS_ARM7TDMI,	"ARM710T",
	  generic_steppings },
	{ CPU_ID_ARM720T,	CPU_CLASS_ARM7TDMI,	"ARM720T",
	  generic_steppings },
	{ CPU_ID_ARM740T8K,	CPU_CLASS_ARM7TDMI, "ARM740T (8 KB cache)",
	  generic_steppings },
	{ CPU_ID_ARM740T4K,	CPU_CLASS_ARM7TDMI, "ARM740T (4 KB cache)",
	  generic_steppings },

	{ CPU_ID_ARM810,	CPU_CLASS_ARM8,		"ARM810",
	  generic_steppings },

	{ CPU_ID_ARM920T,	CPU_CLASS_ARM9TDMI,	"ARM920T",
	  generic_steppings },
	{ CPU_ID_ARM922T,	CPU_CLASS_ARM9TDMI,	"ARM922T",
	  generic_steppings },
	{ CPU_ID_ARM940T,	CPU_CLASS_ARM9TDMI,	"ARM940T",
	  generic_steppings },
	{ CPU_ID_ARM946ES,	CPU_CLASS_ARM9ES,	"ARM946E-S",
	  generic_steppings },
	{ CPU_ID_ARM966ES,	CPU_CLASS_ARM9ES,	"ARM966E-S",
	  generic_steppings },
	{ CPU_ID_ARM966ESR1,	CPU_CLASS_ARM9ES,	"ARM966E-S",
	  generic_steppings },
	{ CPU_ID_TI925T,	CPU_CLASS_ARM9TDMI,	"TI ARM925T",
	  generic_steppings },

	{ CPU_ID_ARM1020E,	CPU_CLASS_ARM10E,	"ARM1020E",
	  generic_steppings },
	{ CPU_ID_ARM1022ES,	CPU_CLASS_ARM10E,	"ARM1022E-S",
	  generic_steppings },

	{ CPU_ID_SA110,		CPU_CLASS_SA1,		"SA-110",
	  sa110_steppings },
	{ CPU_ID_SA1100,	CPU_CLASS_SA1,		"SA-1100",
	  sa1100_steppings },
	{ CPU_ID_SA1110,	CPU_CLASS_SA1,		"SA-1110",
	  sa1110_steppings },

	{ CPU_ID_IXP1200,	CPU_CLASS_SA1,		"IXP1200",
	  ixp12x0_steppings },

	{ CPU_ID_80200,		CPU_CLASS_XSCALE,	"i80200",
	  xscale_steppings },

	{ CPU_ID_80321_400,	CPU_CLASS_XSCALE,	"i80321 400MHz",
	  i80321_steppings },
	{ CPU_ID_80321_600,	CPU_CLASS_XSCALE,	"i80321 600MHz",
	  i80321_steppings },
	{ CPU_ID_80321_400_B0,	CPU_CLASS_XSCALE,	"i80321 400MHz",
	  i80321_steppings },
	{ CPU_ID_80321_600_B0,	CPU_CLASS_XSCALE,	"i80321 600MHz",
	  i80321_steppings },

	{ CPU_ID_PXA250A,	CPU_CLASS_XSCALE,	"PXA250",
	  pxa2x0_steppings },
	{ CPU_ID_PXA210A,	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },
	{ CPU_ID_PXA250B,	CPU_CLASS_XSCALE,	"PXA250",
	  pxa2x0_steppings },
	{ CPU_ID_PXA210B,	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },
	{ CPU_ID_PXA250C, 	CPU_CLASS_XSCALE,	"PXA255/26x",
	  pxa255_steppings },
	{ CPU_ID_PXA210C, 	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },

	{ CPU_ID_IXP425_533,	CPU_CLASS_XSCALE,	"IXP425 533MHz",
	  ixp425_steppings },
	{ CPU_ID_IXP425_400,	CPU_CLASS_XSCALE,	"IXP425 400MHz",
	  ixp425_steppings },
	{ CPU_ID_IXP425_266,	CPU_CLASS_XSCALE,	"IXP425 266MHz",
	  ixp425_steppings },

	{ 0, CPU_CLASS_NONE, NULL, NULL }
};

struct cpu_classtab {
	const char	*class_name;
	const char	*class_option;
};

const struct cpu_classtab cpu_classes[] = {
	{ "unknown",	NULL },			/* CPU_CLASS_NONE */
	{ "ARM2",	"CPU_ARM2" },		/* CPU_CLASS_ARM2 */
	{ "ARM2as",	"CPU_ARM250" },		/* CPU_CLASS_ARM2AS */
	{ "ARM3",	"CPU_ARM3" },		/* CPU_CLASS_ARM3 */
	{ "ARM6",	"CPU_ARM6" },		/* CPU_CLASS_ARM6 */
	{ "ARM7",	"CPU_ARM7" },		/* CPU_CLASS_ARM7 */
	{ "ARM7TDMI",	"CPU_ARM7TDMI" },	/* CPU_CLASS_ARM7TDMI */
	{ "ARM8",	"CPU_ARM8" },		/* CPU_CLASS_ARM8 */
	{ "ARM9TDMI",	NULL },			/* CPU_CLASS_ARM9TDMI */
	{ "ARM9E-S",	NULL },			/* CPU_CLASS_ARM9ES */
	{ "ARM10E",	"CPU_ARM10" },		/* CPU_CLASS_ARM10E */
	{ "SA-1",	"CPU_SA110" },		/* CPU_CLASS_SA1 */
	{ "XScale",	"CPU_XSCALE_..." },	/* CPU_CLASS_XSCALE */
};

/*
 * Report the type of the specified arm processor. This uses the generic and
 * arm specific information in the CPU structure to identify the processor.
 * The remaining fields in the CPU structure are filled in appropriately.
 */

static const char * const wtnames[] = {
	"write-through",
	"write-back",
	"write-back",
	"**unknown 3**",
	"**unknown 4**",
	"write-back-locking",		/* XXX XScale-specific? */
	"write-back-locking-A",
	"write-back-locking-B",
	"**unknown 8**",
	"**unknown 9**",
	"**unknown 10**",
	"**unknown 11**",
	"**unknown 12**",
	"**unknown 13**",
	"**unknown 14**",
	"**unknown 15**",
};

void
identify_arm_cpu(struct device *dv, struct cpu_info *ci)
{
	u_int cpuid;
	enum cpu_class cpu_class = CPU_CLASS_NONE;
	int i;

	cpuid = ci->ci_arm_cpuid;

	if (cpuid == 0) {
		aprint_error("Processor failed probe - no CPU ID\n");
		return;
	}

	for (i = 0; cpuids[i].cpuid != 0; i++)
		if (cpuids[i].cpuid == (cpuid & CPU_ID_CPU_MASK)) {
			cpu_class = cpuids[i].cpu_class;
			sprintf(cpu_model, "%s %s (%s core)",
			    cpuids[i].cpu_name,
			    cpuids[i].cpu_steppings[cpuid &
						    CPU_ID_REVISION_MASK],
			    cpu_classes[cpu_class].class_name);
			break;
		}

	if (cpuids[i].cpuid == 0)
		sprintf(cpu_model, "unknown CPU (ID = 0x%x)", cpuid);

	aprint_naive(": %s\n", cpu_model);
	aprint_normal(": %s\n", cpu_model);

	aprint_normal("%s:", dv->dv_xname);

	switch (cpu_class) {
	case CPU_CLASS_ARM6:
	case CPU_CLASS_ARM7:
	case CPU_CLASS_ARM7TDMI:
	case CPU_CLASS_ARM8:
		if ((ci->ci_ctrl & CPU_CONTROL_IDC_ENABLE) == 0)
			aprint_normal(" IDC disabled");
		else
			aprint_normal(" IDC enabled");
		break;
	case CPU_CLASS_ARM9TDMI:
	case CPU_CLASS_ARM10E:
	case CPU_CLASS_SA1:
	case CPU_CLASS_XSCALE:
		if ((ci->ci_ctrl & CPU_CONTROL_DC_ENABLE) == 0)
			aprint_normal(" DC disabled");
		else
			aprint_normal(" DC enabled");
		if ((ci->ci_ctrl & CPU_CONTROL_IC_ENABLE) == 0)
			aprint_normal(" IC disabled");
		else
			aprint_normal(" IC enabled");
		break;
	default:
		break;
	}
	if ((ci->ci_ctrl & CPU_CONTROL_WBUF_ENABLE) == 0)
		aprint_normal(" WB disabled");
	else
		aprint_normal(" WB enabled");

	if (ci->ci_ctrl & CPU_CONTROL_LABT_ENABLE)
		aprint_normal(" LABT");
	else
		aprint_normal(" EABT");

	if (ci->ci_ctrl & CPU_CONTROL_BPRD_ENABLE)
		aprint_normal(" branch prediction enabled");

	aprint_normal("\n");

	/* Print cache info. */
	if (arm_picache_line_size == 0 && arm_pdcache_line_size == 0)
		goto skip_pcache;

	if (arm_pcache_unified) {
		aprint_normal("%s: %dKB/%dB %d-way %s unified cache\n",
		    dv->dv_xname, arm_pdcache_size / 1024,
		    arm_pdcache_line_size, arm_pdcache_ways,
		    wtnames[arm_pcache_type]);
	} else {
		aprint_normal("%s: %dKB/%dB %d-way Instruction cache\n",
		    dv->dv_xname, arm_picache_size / 1024,
		    arm_picache_line_size, arm_picache_ways);
		aprint_normal("%s: %dKB/%dB %d-way %s Data cache\n",
		    dv->dv_xname, arm_pdcache_size / 1024, 
		    arm_pdcache_line_size, arm_pdcache_ways,
		    wtnames[arm_pcache_type]);
	}

 skip_pcache:

	switch (cpu_class) {
#ifdef CPU_ARM2
	case CPU_CLASS_ARM2:
#endif
#ifdef CPU_ARM250
	case CPU_CLASS_ARM2AS:
#endif
#ifdef CPU_ARM3
	case CPU_CLASS_ARM3:
#endif
#ifdef CPU_ARM6
	case CPU_CLASS_ARM6:
#endif
#ifdef CPU_ARM7
	case CPU_CLASS_ARM7:
#endif
#ifdef CPU_ARM7TDMI
	case CPU_CLASS_ARM7TDMI:
#endif		
#ifdef CPU_ARM8
	case CPU_CLASS_ARM8:
#endif
#ifdef CPU_ARM9
	case CPU_CLASS_ARM9TDMI:
#endif
#ifdef CPU_ARM10
	case CPU_CLASS_ARM10E:
#endif
#if defined(CPU_SA110) || defined(CPU_SA1100) || \
    defined(CPU_SA1110) || defined(CPU_IXP12X0)
	case CPU_CLASS_SA1:
#endif
#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425)
	case CPU_CLASS_XSCALE:
#endif
		break;
	default:
		if (cpu_classes[cpu_class].class_option != NULL)
			aprint_error("%s: %s does not fully support this CPU."
			       "\n", dv->dv_xname, ostype);
		else {
			aprint_error("%s: This kernel does not fully support "
			       "this CPU.\n", dv->dv_xname);
			aprint_normal("%s: Recompile with \"options %s\" to "
			       "correct this.\n", dv->dv_xname,
			       cpu_classes[cpu_class].class_option);
		}
		break;
	}
			       
}
#ifdef MULTIPROCESSOR
int
cpu_alloc_idlepcb(struct cpu_info *ci)
{
	vaddr_t uaddr;
	struct pcb *pcb;
	struct trapframe *tf;
	int error;

	/*
	 * Generate a kernel stack and PCB (in essence, a u-area) for the
	 * new CPU.
	 */
	if (uvm_uarea_alloc(&uaddr)) {
		error = uvm_fault_wire(kernel_map, uaddr, uaddr + USPACE,
		    VM_FAULT_WIRE, VM_PROT_READ | VM_PROT_WRITE);
		if (error)
			return error;
	}
	ci->ci_idlepcb = pcb = (struct pcb *)uaddr;

	/*
	 * This code is largely derived from cpu_fork(), with which it
	 * should perhaps be shared.
	 */

	/* Copy the pcb */
	*pcb = proc0.p_addr->u_pcb;

	/* Set up the undefined stack for the process. */
	pcb->pcb_un.un_32.pcb32_und_sp = uaddr + USPACE_UNDEF_STACK_TOP;
	pcb->pcb_un.un_32.pcb32_sp = uaddr + USPACE_SVC_STACK_TOP;

#ifdef STACKCHECKS
	/* Fill the undefined stack with a known pattern */
	memset(((u_char *)uaddr) + USPACE_UNDEF_STACK_BOTTOM, 0xdd,
	    (USPACE_UNDEF_STACK_TOP - USPACE_UNDEF_STACK_BOTTOM));
	/* Fill the kernel stack with a known pattern */
	memset(((u_char *)uaddr) + USPACE_SVC_STACK_BOTTOM, 0xdd,
	    (USPACE_SVC_STACK_TOP - USPACE_SVC_STACK_BOTTOM));
#endif	/* STACKCHECKS */

	pcb->pcb_tf = tf =
	    (struct trapframe *)pcb->pcb_un.un_32.pcb32_sp - 1;
	*tf = *proc0.p_addr->u_pcb.pcb_tf;
	return 0;
}
#endif /* MULTIPROCESSOR */

/* End of cpu.c */
