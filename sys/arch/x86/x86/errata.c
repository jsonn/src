/*	$NetBSD: errata.c,v 1.8.14.1 2007/11/06 23:23:47 matt Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Detect, report on, and work around known errata with x86 CPUs.
 *
 * This currently only handles AMD CPUs, and is generalised because
 * there are quite a few problems that the BIOS can patch via MSR,
 * but it is not known if the OS can patch these yet.  The list is
 * expected to grow over time.
 *
 * The data here are from: Revision Guide for AMD Athlon 64 and
 * AMD Opteron Processors, Publication #25759, Revision: 3.69,
 * Issue Date: September 2006
 *
 * XXX This should perhaps be integrated with the identcpu code.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: errata.c,v 1.8.14.1 2007/11/06 23:23:47 matt Exp $");

#include "opt_multiprocessor.h"
#ifdef i386
#include "opt_cputype.h"
#endif

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <x86/cpuvar.h>
#include <x86/cputypes.h>

#if defined(I686_CPU) || defined(__x86_64__)

typedef struct errata {
	u_short		e_num;
	u_short		e_reported;
	u_int		e_data1;
	const uint8_t	*e_set;
	bool		(*e_act)(struct cpu_info *, struct errata *);
	uint64_t	e_data2;
} errata_t;

typedef enum cpurev {
	BH_E4, CH_CG, CH_D0, DH_CG, DH_D0, DH_E3, DH_E6, JH_E1,
	JH_E6, SH_B0, SH_B3, SH_C0, SH_CG, SH_D0, SH_E4, SH_E5,
	OINK
} cpurev_t;

static const u_int cpurevs[] = {
	BH_E4, 0x0020fb1, CH_CG, 0x0000f82, CH_CG, 0x0000fb2,
	CH_D0, 0x0010f80, CH_D0, 0x0010fb0, DH_CG, 0x0000fc0,
	DH_CG, 0x0000fe0, DH_CG, 0x0000ff0, DH_D0, 0x0010fc0,
	DH_D0, 0x0010ff0, DH_E3, 0x0020fc0, DH_E3, 0x0020ff0,
	DH_E6, 0x0020fc2, DH_E6, 0x0020ff2, JH_E1, 0x0020f10,
	JH_E6, 0x0020f12, JH_E6, 0x0020f32, SH_B0, 0x0000f40,
	SH_B3, 0x0000f51, SH_C0, 0x0000f48, SH_C0, 0x0000f58,
	SH_CG, 0x0000f4a, SH_CG, 0x0000f5a, SH_CG, 0x0000f7a,
	SH_D0, 0x0010f40, SH_D0, 0x0010f50, SH_D0, 0x0010f70,
	SH_E4, 0x0020f51, SH_E4, 0x0020f71, SH_E5, 0x0020f42,
	OINK
};

static const uint8_t x86_errata_set1[] = {
	SH_B3, SH_C0, SH_CG, DH_CG, CH_CG, OINK
};

static const uint8_t x86_errata_set2[] = {
	SH_B3, SH_C0, SH_CG, DH_CG, CH_CG, SH_D0, DH_D0, CH_D0, OINK
};

static const uint8_t x86_errata_set3[] = {
	JH_E1, DH_E3, OINK
};

static const uint8_t x86_errata_set4[] = {
	SH_C0, SH_CG, DH_CG, CH_CG, SH_D0, DH_D0, CH_D0, JH_E1,
	DH_E3, SH_E4, BH_E4, SH_E5, DH_E6, JH_E6, OINK
};

static const uint8_t x86_errata_set5[] = {
	SH_B3, OINK
};

static const uint8_t x86_errata_set6[] = {
	SH_C0, SH_CG, DH_CG, CH_CG, OINK
};

static const uint8_t x86_errata_set7[] = {
	SH_C0, SH_CG, DH_CG, CH_CG, SH_D0, DH_D0, CH_D0, OINK
};

static const uint8_t x86_errata_set8[] = {
	BH_E4, CH_CG, CH_CG, CH_D0, CH_D0, DH_CG, DH_CG, DH_CG,
	DH_D0, DH_D0, DH_E3, DH_E3, DH_E6, DH_E6, JH_E1, JH_E6,
	JH_E6, SH_B0, SH_B3, SH_C0, SH_C0, SH_CG, SH_CG, SH_CG, 
	SH_D0, SH_D0, SH_D0, SH_E4, SH_E4, SH_E5, OINK
};

static bool x86_errata_setmsr(struct cpu_info *, errata_t *);
static bool x86_errata_testmsr(struct cpu_info *, errata_t *);

static errata_t errata[] = {
	/*
	 * 81: Cache Coherency Problem with Hardware Prefetching
	 * and Streaming Stores
	 */
	{
		81, FALSE, MSR_DC_CFG, x86_errata_set5,
		x86_errata_testmsr, DC_CFG_DIS_SMC_CHK_BUF
	},
	/*
	 * 86: DRAM Data Masking Feature Can Cause ECC Failures
	 */
	{
		86, FALSE, MSR_NB_CFG, x86_errata_set1,
		x86_errata_testmsr, NB_CFG_DISDATMSK
	},
	/*
	 * 89: Potential Deadlock With Locked Transactions
	 */
	{
		89, FALSE, MSR_NB_CFG, x86_errata_set8,
		x86_errata_testmsr, NB_CFG_DISIOREQLOCK
	},
	/*
	 * 94: Sequential Prefetch Feature May Cause Incorrect
	 * Processor Operation
	 */
	{
		94, FALSE, MSR_IC_CFG, x86_errata_set1,
		x86_errata_testmsr, IC_CFG_DIS_SEQ_PREFETCH
	},
	/*
	 * 97: 128-Bit Streaming Stores May Cause Coherency
	 * Failure
	 *
	 * XXX "This workaround must not be applied to processors
	 * prior to revision C0."  We don't apply it, but if it
	 * can't be applied, it shouldn't be reported.
	 */
	{
		97, FALSE, MSR_DC_CFG, x86_errata_set6,
		x86_errata_testmsr, DC_CFG_DIS_CNV_WC_SSO
	},
	/*
	 * 104: DRAM Data Masking Feature Causes ChipKill ECC
	 * Failures When Enabled With x8/x16 DRAM Devices
	 */
	{
		104, FALSE, MSR_NB_CFG, x86_errata_set7,
		x86_errata_testmsr, NB_CFG_DISDATMSK
	},
	/*
	 * 113: Enhanced Write-Combining Feature Causes System Hang
	 */
	{
		113, FALSE, MSR_BU_CFG, x86_errata_set3,
		x86_errata_setmsr, BU_CFG_WBENHWSBDIS
	},
#ifdef MULTIPROCESSOR
	/*
	 * 69: Multiprocessor Coherency Problem with Hardware
	 * Prefetch Mechanism
	 */
	{
		69, FALSE, MSR_BU_CFG, x86_errata_set5,
		x86_errata_setmsr, BU_CFG_WBPFSMCCHKDIS
	},
	/*
	 * 101: DRAM Scrubber May Cause Data Corruption When Using
	 * Node-Interleaved Memory
	 */
	{
		101, FALSE, 0, x86_errata_set2,
		NULL, 0
	},
	/*
	 * 106: Potential Deadlock with Tightly Coupled Semaphores
	 * in an MP System
	 */
	{
		106, FALSE, MSR_LS_CFG, x86_errata_set2,
		x86_errata_testmsr, LS_CFG_DIS_LS2_SQUISH
	},
	/*
	 * 107: Possible Multiprocessor Coherency Problem with
	 * Setting Page Table A/D Bits
	 */
	{
		107, FALSE, MSR_BU_CFG, x86_errata_set2,
		x86_errata_testmsr, BU_CFG_THRL2IDXCMPDIS
	},
	/*
	 * 122: TLB Flush Filter May Cause Coherency Problem in
	 * Multiprocessor Systems
	 */
	{
		122, FALSE, MSR_HWCR, x86_errata_set4,
		x86_errata_setmsr, HWCR_FFDIS
	},
#endif	/* MULTIPROCESSOR */
};

static bool 
x86_errata_testmsr(struct cpu_info *ci, errata_t *e)
{
	uint64_t val;

	(void)ci;

	val = rdmsr_locked(e->e_data1, OPTERON_MSR_PASSCODE);
	if ((val & e->e_data2) != 0)
		return FALSE;

	e->e_reported = TRUE;
	return TRUE;
}

static bool 
x86_errata_setmsr(struct cpu_info *ci, errata_t *e)
{
	uint64_t val;

	(void)ci;

	val = rdmsr_locked(e->e_data1, OPTERON_MSR_PASSCODE);
	if ((val & e->e_data2) != 0)
		return FALSE;
	wrmsr_locked(e->e_data1, OPTERON_MSR_PASSCODE, val | e->e_data2);
	aprint_debug("%s: erratum %d patched\n",
	    ci->ci_dev->dv_xname, e->e_num);

	return FALSE;
}

void
x86_errata(struct cpu_info *ci, int vendor)
{
	uint32_t descs[4];
	errata_t *e, *ex;
	cpurev_t rev;
	int i, j, upgrade;
	static int again;

	if (vendor != CPUVENDOR_AMD)
		return;

	x86_cpuid(0x80000001, descs);

	for (i = 0;; i += 2) {
		if ((rev = cpurevs[i]) == OINK)
			return;
		if (cpurevs[i + 1] == descs[0])
			break;
	}

	ex = errata + sizeof(errata) / sizeof(errata[0]);
	for (upgrade = 0, e = errata; e < ex; e++) {
		if (e->e_reported)
			continue;
		if (e->e_set != NULL) {
			for (j = 0; e->e_set[j] != OINK; j++)
				if (e->e_set[j] == rev)
					break;
			if (e->e_set[j] == OINK)
				continue;
		}

		aprint_debug("%s: testing for erratum %d\n",
		    ci->ci_dev->dv_xname, e->e_num);

		if (e->e_act == NULL)
			e->e_reported = TRUE;
		else if ((*e->e_act)(ci, e) == FALSE)
			continue;

		aprint_debug("%s: erratum %d present\n",
		    ci->ci_dev->dv_xname, e->e_num);
		upgrade = 1;
	}

	if (upgrade && !again) {
		again = 1;
		aprint_normal("%s: WARNING: AMD errata present, BIOS upgrade "
		    "may be\n", ci->ci_dev->dv_xname);
		aprint_normal("%s: WARNING: necessary to ensure reliable "
		    "operation\n", ci->ci_dev->dv_xname);
	}
}

#else	/* defined(I686_CPU) || defined(__x86_64__) */

void
x86_errata(struct cpu_info *ci, int vendor)
{
	(void)ci;
	(void)vendor;
}

#endif	/* defined(I686_CPU) || defined(__x86_64__) */
