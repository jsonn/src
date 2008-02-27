/*	$NetBSD: ipifuncs.c,v 1.1.18.6 2008/02/27 08:36:25 yamt Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ipifuncs.c,v 1.1.18.6 2008/02/27 08:36:25 yamt Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/simplelock.h>

#include <machine/db_machdep.h>

#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/pte.h>
#include <machine/sparc64.h>

#define SPARC64_IPI_RETRIES	10000

#define	sparc64_ipi_sleep()	delay(1000)

#if defined(DDB) || defined(KGDB)
extern int db_active;
#endif

/* CPU sets containing halted, paused and resumed cpus */
static volatile sparc64_cpuset_t cpus_halted;
static volatile sparc64_cpuset_t cpus_paused;
static volatile sparc64_cpuset_t cpus_resumed;

/* IPI handlers. */
static int	sparc64_ipi_wait(sparc64_cpuset_t volatile *, sparc64_cpuset_t);
static void	sparc64_ipi_error(const char *, sparc64_cpuset_t, sparc64_cpuset_t);

 
/*
 * These are the "function" entry points in locore.s to handle IPI's.
 */
void	sparc64_ipi_halt(void *);
void	sparc64_ipi_pause(void *);
void	sparc64_ipi_flush_pte(void *);
void	sparc64_ipi_flush_ctx(void *);
void	sparc64_ipi_flush_all(void *);

/*
 * Process cpu stop-self event.
 */
int
sparc64_ipi_halt_thiscpu(void *arg)
{

	printf("cpu%d: shutting down\n", cpu_number());
	CPUSET_ADD(cpus_halted, cpu_number());
	prom_stopself();

	return(1);
}

/*
 * Pause cpu.  This is called from locore.s after setting up a trapframe.
 */
int
sparc64_ipi_pause_thiscpu(void *arg)
{
	cpuid_t cpuid;
	int s;
#if defined(DDB)
	struct trapframe64 *tf = arg;
	volatile db_regs_t dbregs;

	if (tf) {
		/* Initialise local dbregs storage from trap frame */
		dbregs.db_tf = *tf;
		dbregs.db_fr = *(struct frame64 *)(u_long)tf->tf_out[6];

		curcpu()->ci_ddb_regs = &dbregs;
	}
#endif

	cpuid = cpu_number();
	printf("cpu%ld paused.\n", cpuid);

	s = intr_disable();
	CPUSET_ADD(cpus_paused, cpuid);

	do {
		membar_sync();
	} while(CPUSET_HAS(cpus_paused, cpuid));
	membar_sync();

	CPUSET_ADD(cpus_resumed, cpuid);

#if defined(DDB)
	if (tf)
		curcpu()->ci_ddb_regs = NULL;
#endif

	intr_restore(s);
	printf("cpu%ld resumed.\n", cpuid);
	return (1);
}

/*
 * Initialize IPI machinery.
 */
void
sparc64_ipi_init()
{

	/* Clear all cpu sets. */
	CPUSET_CLEAR(cpus_halted);
	CPUSET_CLEAR(cpus_paused);
	CPUSET_CLEAR(cpus_resumed);
}

/*
 * Send an IPI to all in the list but ourselves.
 */
void
sparc64_multicast_ipi(sparc64_cpuset_t cpuset, ipifunc_t func, uint64_t arg1)
{
	struct cpu_info *ci;

	CPUSET_DEL(cpuset, cpu_number());
	if (CPUSET_EMPTY(cpuset))
		return;

	for (ci = cpus; ci != NULL; ci = ci->ci_next) {
		if (CPUSET_HAS(cpuset, ci->ci_index)) {
			CPUSET_DEL(cpuset, ci->ci_index);
			sparc64_send_ipi(ci->ci_cpuid, func, arg1);
		}
	}
}

/*
 * Broadcast an IPI to all but ourselves.
 */
void
sparc64_broadcast_ipi(ipifunc_t func, uint64_t arg1)
{

	sparc64_multicast_ipi(CPUSET_EXCEPT(cpus_active, cpu_number()), func,
		arg1);
}

/*
 * Send an interprocessor interrupt.
 */
void
sparc64_send_ipi(int upaid, ipifunc_t func, uint64_t arg1)
{
	int i, ik;
	uint64_t intr_func;

	if (ldxa(0, ASR_IDSR) & IDSR_BUSY)
		panic("recursive IPI?");

	intr_func = (uint64_t)(u_long)func;

	/* Schedule an interrupt. */
	for (i = 0; i < SPARC64_IPI_RETRIES; i++) {
		int s = intr_disable();

		stxa(IDDR_0H, ASI_INTERRUPT_DISPATCH, 0);
		stxa(IDDR_1H, ASI_INTERRUPT_DISPATCH, intr_func);
		stxa(IDDR_2H, ASI_INTERRUPT_DISPATCH, arg1);
		stxa(IDCR(upaid), ASI_INTERRUPT_DISPATCH, 0);
		membar_sync();

		for (ik = 0; ik < 1000000; ik++) {
			if (ldxa(0, ASR_IDSR) & IDSR_BUSY)
				continue;
			else
				break;
		}
		intr_restore(s);

		if (ik == 1000000)
			break;

		if ((ldxa(0, ASR_IDSR) & IDSR_NACK) == 0)
			return;
	}

	if (!db_active && panicstr == NULL)
		panic("cpu%d: ipi_send: couldn't send ipi to UPAID %u", 
			cpu_number(), upaid);
	else
		printf("\noops, can't send IPI from cpu%d to UPAID %u\n",
			cpu_number(), upaid);
}

/*
 * Wait for IPI operation to complete.
 */
int
sparc64_ipi_wait(sparc64_cpuset_t volatile *cpus_watchset, sparc64_cpuset_t cpus_mask)
{
	int i;

	for (i = 0; i < SPARC64_IPI_RETRIES; i++) {
		membar_sync();
		if (CPUSET_EQUAL(*cpus_watchset, cpus_mask))
			break;
		sparc64_ipi_sleep();
	}
	return (i == SPARC64_IPI_RETRIES);
}

/*
 * Halt all cpus but ourselves.
 */
void
mp_halt_cpus()
{
	sparc64_cpuset_t cpumask, cpuset;

	CPUSET_ASSIGN(cpuset, cpus_active);
	CPUSET_DEL(cpuset, cpu_number());
	CPUSET_ASSIGN(cpumask, cpuset);
	CPUSET_SUB(cpuset, cpus_halted);

	if (CPUSET_EMPTY(cpuset))
		return;

	sparc64_multicast_ipi(cpuset, sparc64_ipi_halt, 0);
	if (sparc64_ipi_wait(&cpus_halted, cpumask))
		sparc64_ipi_error("halt", cpumask, cpus_halted);
}

/*
 * Pause all cpus but ourselves.
 */
void
mp_pause_cpus()
{
	sparc64_cpuset_t cpuset;

	CPUSET_ASSIGN(cpuset, cpus_active);
	CPUSET_DEL(cpuset, cpu_number());

	if (CPUSET_EMPTY(cpuset))
		return;

	sparc64_multicast_ipi(cpuset, sparc64_ipi_pause, 0);
	if (sparc64_ipi_wait(&cpus_paused, cpuset))
		sparc64_ipi_error("pause", cpus_paused, cpuset);
}

/*
 * Resume all paused cpus.
 */
void
mp_resume_cpus()
{
	sparc64_cpuset_t cpuset;

	CPUSET_CLEAR(cpus_resumed);
	CPUSET_ASSIGN(cpuset, cpus_paused);
	membar_sync();
	CPUSET_CLEAR(cpus_paused);

	/* CPUs awake on cpus_paused clear */
	if (sparc64_ipi_wait(&cpus_resumed, cpuset))
		sparc64_ipi_error("resume", cpus_resumed, cpuset);
}

int
mp_cpu_is_paused(sparc64_cpuset_t cpunum)
{

	return CPUSET_HAS(cpus_paused, cpunum);
}

/*
 * Flush pte on all active processors.
 */
void
smp_tlb_flush_pte(vaddr_t va, int ctx)
{
	/* Flush our own TLB */
	sp_tlb_flush_pte(va, ctx);

	/* Flush others */
	/* sparc64_broadcast_ipi(sparc64_ipi_flush_ctx, ctx); */
}

/*
 * Flush context on all active processors.
 */
void
smp_tlb_flush_ctx(int ctx)
{
	/* Flush our own TLB */
	sp_tlb_flush_ctx(ctx);

	/* Flush others */
	sparc64_broadcast_ipi(sparc64_ipi_flush_ctx, ctx);
}

/*
 * Flush whole TLB on all active processors.
 */
void
smp_tlb_flush_all()
{
	/* Flush our own TLB */
	sp_tlb_flush_all();

	/* Flush others */
	sparc64_broadcast_ipi(sparc64_ipi_flush_all, 0);
}

/*
 * Print an error message.
 */
void
sparc64_ipi_error(const char *s, sparc64_cpuset_t cpus_succeeded,
	sparc64_cpuset_t cpus_expected)
{
	int cpuid;

	CPUSET_DEL(cpus_expected, cpus_succeeded);
	if (!CPUSET_EMPTY(cpus_expected)) {
		printf("Failed to %s:", s);
		do {
			cpuid = CPUSET_NEXT(cpus_expected);
			CPUSET_DEL(cpus_expected, cpuid);
			printf(" cpu%d", cpuid);
		} while(!CPUSET_EMPTY(cpus_expected));
	}

	printf("\n");
}
