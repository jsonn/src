/*	$NetBSD: acpi_wakeup.c,v 1.3.48.11 2007/09/25 13:16:36 joerg Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_wakeup.c,v 1.3.48.11 2007/09/25 13:16:36 joerg Exp $");

/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      FreeBSD: src/sys/i386/acpica/acpi_wakeup.c,v 1.9 2002/01/10 03:26:46 wes Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/bus.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

#include "ioapic.h"
#include "lapic.h"

#if NLAPIC > 0
#include <machine/i82489var.h>
#endif
#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif
#include <machine/i8259.h>

#include "acpi.h"

#include <dev/ic/i8253reg.h>
#include <dev/acpi/acpica.h>
#include <dev/acpi/acpivar.h>
#define ACPI_MACHDEP_PRIVATE
#include <machine/acpi_machdep.h>
#include <machine/cpu.h>
#include <machine/fpu.h>

#include <x86/cpuvar.h>

#include "acpi_wakecode.h"

static paddr_t acpi_wakeup_paddr = 3 * PAGE_SIZE;
static vaddr_t acpi_wakeup_vaddr;

static int acpi_md_node = CTL_EOL;
static int acpi_md_vbios_reset = 1;
static int acpi_md_beep_on_reset = 1;

static int	sysctl_md_acpi_vbios_reset(SYSCTLFN_ARGS);
static int	sysctl_md_acpi_beep_on_reset(SYSCTLFN_ARGS);

extern uint8_t acpi_wakeup_gdt[10];
extern uint64_t acpi_wakeup_ds;
extern uint64_t acpi_wakeup_cr0;
/* XXX shut gcc up */
extern int		acpi_savecpu(void) __attribute__((__returns_twice__));
extern int		acpi_restorecpu(void);

int
acpi_md_sleep(int state)
{
#define WAKECODE_FIXUP(offset, type, val) do	{		\
	type	*addr;						\
	addr = (type *)(acpi_wakeup_vaddr + offset);		\
	*addr = val;						\
} while (0)

#define WAKECODE_BCOPY(offset, type, val) do	{		\
	void	**addr;						\
	addr = (void **)(acpi_wakeup_vaddr + offset);		\
	bcopy(&(val), addr, sizeof(type));			\
} while (0)

	ACPI_STATUS			status;
	int				ret = 0;
	paddr_t				tmp_pdir;

	KASSERT(acpi_wakeup_paddr != 0);
	KASSERT(sizeof(wakecode) <= PAGE_SIZE);

	if (acpi_wakeup_vaddr == 0) {
		/* Map ACPI wakecode */
		acpi_wakeup_vaddr = uvm_km_alloc(kernel_map, PAGE_SIZE, 0,
		    UVM_KMF_VAONLY);
		if (acpi_wakeup_vaddr == 0) {
			printf("acpi: can't allocate address for wakecode.\n");
			return -1;
		}
		pmap_kenter_pa(acpi_wakeup_vaddr, acpi_wakeup_paddr,
		    VM_PROT_READ | VM_PROT_WRITE);
	}

	if (!CPU_IS_PRIMARY(curcpu()) {
		printf("acpi0: WARNING: ignoring sleep from secondary CPU\n");
		return -1;
	}

	AcpiSetFirmwareWakingVector(acpi_wakeup_paddr);

#ifdef MULTIPROCESSOR
	/* Shutdown all other CPUs */
	x86_broadcast_ipi(X86_IPI_HALT);
#endif

	tmp_pdir = pmap_init_tmp_pgtbl(acpi_wakeup_paddr);

	disable_intr();

	if (acpi_savecpu()) {

		/* Execute Sleep */
		bcopy(wakecode, (void *)acpi_wakeup_vaddr, sizeof(wakecode));

		WAKECODE_FIXUP(WAKEUP_vbios_reset, uint8_t, acpi_md_vbios_reset);
		WAKECODE_FIXUP(WAKEUP_beep_on_reset, uint8_t, acpi_md_beep_on_reset);

		WAKECODE_FIXUP(WAKEUP_r_cr0, uint64_t, acpi_wakeup_cr0);
		WAKECODE_FIXUP(WAKEUP_r_cr3, uint64_t, tmp_pdir);

		WAKECODE_BCOPY(WAKEUP_r_gdt, struct region_descriptor, acpi_wakeup_gdt);

		WAKECODE_FIXUP(WAKEUP_restorecpu, void *, acpi_restorecpu);

		ACPI_FLUSH_CPU_CACHE();

		status = AcpiEnterSleepState(state);

		if (ACPI_FAILURE(status)) {
			printf("acpi: AcpiEnterSleepState failed: %s\n",
			       AcpiFormatException(status));
			ret = -1;
			goto out;
		}

		for (;;) ;
	} else {
		/* Execute Wakeup */
		cpu_init_msrs(&cpu_info_primary);
		fpuinit(&cpu_info_primary);
		i8259_reinit();
#if NLAPIC > 0
		lapic_enable();
		lapic_initclocks();
#endif
#if NIOAPIC > 0
		ioapic_enable();
		lapic_set_lvt();
#endif

		initrtclock(TIMER_FREQ);
		inittodr(time_second);
	}

out:
	enable_intr();

	return (ret);
#undef WAKECODE_FIXUP
#undef WAKECODE_BCOPY
}

SYSCTL_SETUP(sysctl_md_acpi_setup, "acpi amd64 sysctl setup")
{
	const struct sysctlnode *node;
	const struct sysctlnode *ssnode;

	if (sysctl_createv(NULL, 0, NULL, &node, CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "machdep", NULL, NULL, 0, NULL, 0, CTL_MACHDEP,
	    CTL_EOL) != 0)
		return;
	if (sysctl_createv(NULL, 0, &node, &ssnode, CTLFLAG_READWRITE,
	    CTLTYPE_INT, "acpi_vbios_reset", NULL, sysctl_md_acpi_vbios_reset,
	    0, NULL, 0, CTL_CREATE, CTL_EOL) != 0)
		return;
	if (sysctl_createv(NULL, 0, &node, &ssnode, CTLFLAG_READWRITE,
	    CTLTYPE_INT, "acpi_beep_on_reset", NULL, sysctl_md_acpi_beep_on_reset,
	    0, NULL, 0, CTL_CREATE, CTL_EOL) != 0)
		return;

	acpi_md_node = node->sysctl_num;
}

static int
sysctl_md_acpi_vbios_reset(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = acpi_md_vbios_reset;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 0 || t > 1)
		return EINVAL;

	acpi_md_vbios_reset = t;

	return 0;
}

static int
sysctl_md_acpi_beep_on_reset(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = acpi_md_beep_on_reset;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < 0 || t > 1)
		return EINVAL;

	acpi_md_beep_on_reset = t;

	return 0;
}
