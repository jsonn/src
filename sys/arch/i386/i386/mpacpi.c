/*	$NetBSD: mpacpi.c,v 1.2.2.2 2003/01/07 21:11:41 thorpej Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_acpi.h"
#include "opt_mpbios.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

#include <machine/specialreg.h>
#include <machine/cputypes.h>
#include <machine/cpuvar.h>
#include <machine/bus.h>
#include <machine/mpacpi.h>
#include <machine/mpbiosvar.h>

#include <machine/i82093reg.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <dev/isa/isareg.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_madt.h>
/* XXX */
#include <dev/acpi/acpica/Subsystem/actables.h>
#include <dev/acpi/acpica/Subsystem/acnamesp.h>


int mpacpi_print(void *, const char *);
int mpacpi_match(struct device *, struct cfdata *, void *);

/*
 * acpi_madt_walk callbacks
 */
static ACPI_STATUS mpacpi_count(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_config_cpu(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_config_ioapic(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_nonpci_intr(APIC_HEADER *, void *);

/*
 * Callbacks for the device namespace walk.
 */
static ACPI_STATUS mpacpi_pciroot_cb(ACPI_HANDLE, UINT32, void *, void **);
static ACPI_STATUS mpacpi_pciroute_cb(ACPI_HANDLE, UINT32, void *, void **);
static ACPI_STATUS mpacpi_pcicount_cb(ACPI_HANDLE, UINT32, void *, void **);
static ACPI_STATUS mpacpi_pcircount_cb(ACPI_HANDLE, UINT32, void *, void **);

static int mpacpi_find_pciroots(void);
static void mpacpi_count_pci(void);
static void mpacpi_config_irouting(void);

static void mpacpi_print_intr(struct mp_intr_map *);
static void mpacpi_print_pci_intr(int);
static void mpacpi_print_isa_intr(int);

int mpacpi_nioapic;
int mpacpi_ncpu;
int mpacpi_npci;
int mpacpi_maxpci;
int mpacpi_nintsrc;

static int mpacpi_intr_index;
static paddr_t mpacpi_lapic_base = LAPIC_BASE;

int
mpacpi_print(void *aux, const char *pnp)
{
	struct cpu_attach_args * caa = (struct cpu_attach_args *) aux;
	if (pnp)
		aprint_normal("%s at %s:",caa->caa_name, pnp);
	return (UNCONF);
}

int
mpacpi_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct cpu_attach_args * caa = (struct cpu_attach_args *) aux;
	if (strcmp(caa->caa_name, cf->cf_name))
		return 0;

	return (config_match(parent, cf, aux));
}

/*
 * Handle special interrupt sources and overrides from the MADT.
 * This is a callback function for acpi_madt_walk().
 */
static ACPI_STATUS
mpacpi_nonpci_intr(APIC_HEADER *hdrp, void *aux)
{
	int *index = aux, pin, lindex;
	struct mp_intr_map *mpi;
	INT_IOAPIC_SOURCE_NMI *ioapic_nmi;
	INT_LAPIC_SOURCE_NMI *lapic_nmi;
	INT_SOURCE_OVERRIDE *isa_ovr;
	struct ioapic_softc *ioapic;

	switch (hdrp->Type) {
	case APIC_INTSRC_NMI:
		ioapic_nmi = (INT_IOAPIC_SOURCE_NMI *)hdrp;
		ioapic = ioapic_find_bybase(ioapic_nmi->GlobalInt);
		if (ioapic == NULL)
			break;
		mpi = &mp_intrs[*index];
		(*index)++;
		mpi->next = NULL;
		mpi->bus = NULL;
		mpi->type = MPS_INTTYPE_NMI;
		mpi->ioapic = ioapic;
		pin = ioapic_nmi->GlobalInt - ioapic->sc_apic_vecbase;
		mpi->ioapic_pin = pin;
		mpi->bus_pin = -1;
		mpi->redir = (IOAPIC_REDLO_DEL_NMI<<IOAPIC_REDLO_DEL_SHIFT);
		ioapic->sc_pins[pin].ip_map = mpi;
		mpi->ioapic_ih = APIC_INT_VIA_APIC |
		    (ioapic->sc_apicid << APIC_INT_APIC_SHIFT) |
		    (pin << APIC_INT_PIN_SHIFT); 
		mpi->flags = ioapic_nmi->Polarity | (ioapic_nmi->Trigger << 2);
		break;
	case APIC_LAPIC_NMI:
		lapic_nmi = (INT_LAPIC_SOURCE_NMI *)hdrp;
		mpi = &mp_intrs[*index];
		(*index)++;
		mpi->next = NULL;
		mpi->bus = NULL;
		mpi->ioapic = NULL;
		mpi->type = MPS_INTTYPE_NMI;
		mpi->ioapic_pin = lapic_nmi->Lint;
		mpi->cpu_id = lapic_nmi->ApicId;
		mpi->redir = (IOAPIC_REDLO_DEL_NMI<<IOAPIC_REDLO_DEL_SHIFT);
		break;
	case APIC_INTSRC_OVR:
		isa_ovr = (INT_SOURCE_OVERRIDE *)hdrp;
		if (isa_ovr->Source > 15 || isa_ovr->Source == 2)
			break;
		ioapic = ioapic_find_bybase(isa_ovr->GlobalInt);
		if (ioapic == NULL)
			break;
		pin = isa_ovr->GlobalInt - ioapic->sc_apic_vecbase;
		lindex = isa_ovr->Source;
		/*
		 * IRQ 2 was skipped in the default setup.
		 */
		if (lindex > 2)
			lindex--;
		mpi = &mp_intrs[lindex];
		mpi->ioapic_ih = APIC_INT_VIA_APIC |
		    (ioapic->sc_apicid << APIC_INT_APIC_SHIFT) |
		    (pin << APIC_INT_PIN_SHIFT);
		mpi->bus_pin = isa_ovr->Source;
		mpi->ioapic_pin = pin;
		mpi->redir = 0;
		switch (isa_ovr->Polarity) {
		case MPS_INTPO_ACTHI:
			mpi->redir &= ~IOAPIC_REDLO_ACTLO;
		case MPS_INTPO_DEF:
		case MPS_INTPO_ACTLO:
			mpi->redir |= IOAPIC_REDLO_ACTLO;
			break;
		}
		mpi->redir |= (IOAPIC_REDLO_DEL_LOPRI<<IOAPIC_REDLO_DEL_SHIFT);
		switch (isa_ovr->Trigger) {
		case MPS_INTTR_DEF:
		case MPS_INTTR_LEVEL:
			mpi->redir |= IOAPIC_REDLO_LEVEL;
			break;
		case MPS_INTTR_EDGE:
			mpi->redir &= ~IOAPIC_REDLO_LEVEL;
			break;
		}
		mpi->flags = isa_ovr->Polarity | (isa_ovr->Trigger << 2);
		ioapic->sc_pins[pin].ip_map = mpi;
	default:
		break;
	}
	return AE_OK;
}

/*
 * Count various MP resources present in the MADT.
 * This is a callback function for acpi_madt_walk().
 */
static ACPI_STATUS
mpacpi_count(APIC_HEADER *hdrp, void *aux)
{
	LAPIC_ADDR_OVR *lop;

	switch (hdrp->Type) {
	case APIC_PROC:
		mpacpi_ncpu++;
		break;
	case APIC_IO:
		mpacpi_nioapic++;
		break;
	case APIC_INTSRC_NMI:
	case APIC_LAPIC_NMI:
		mpacpi_nintsrc++;
		break;
	case APIC_ADDR_OVR:
		lop = (LAPIC_ADDR_OVR *)hdrp;
		mpacpi_lapic_base = lop->LocalApicAddress;
	default:
		break;
	}
	return AE_OK;
}

static ACPI_STATUS
mpacpi_config_cpu(APIC_HEADER *hdrp, void *aux)
{
	struct device *parent = aux;
	PROCESSOR_APIC *p;
	struct cpu_attach_args caa;

	if (hdrp->Type == APIC_PROC) {
		p = (PROCESSOR_APIC *)hdrp;
		if (p->ProcessorEnabled) {
			/*
			 * Assume ACPI Id 0 == BSP.
			 * XXX check if that's correct.
			 * XXX field name in structure is wrong.
			 */
			if (p->ProcessorApicId == 0)
				caa.cpu_role = CPU_ROLE_BP;
			else
				caa.cpu_role = CPU_ROLE_AP;
			caa.caa_name = "cpu";
			caa.cpu_number = p->LocalApicId;
			caa.cpu_func = &mp_cpu_funcs;
			config_found_sm(parent, &caa, mpacpi_print,
			    mpacpi_match);
			
		}
	}
	return AE_OK;
}

static ACPI_STATUS
mpacpi_config_ioapic(APIC_HEADER *hdrp, void *aux)
{
	struct device *parent = aux;
	struct apic_attach_args aaa;
	IO_APIC *p;

	if (hdrp->Type == APIC_IO) {
		p = (IO_APIC *)hdrp;
		aaa.aaa_name = "ioapic";
		aaa.apic_id = p->IoApicId;
		aaa.apic_address = p->IoApicAddress;
		aaa.apic_version = -1;
		aaa.flags = IOAPIC_VWIRE;
		aaa.apic_vecbase = p->Vector;
		config_found_sm(parent, &aaa, mpacpi_print, mpacpi_match);
	}
	return AE_OK;
}

int
mpacpi_scan_apics(struct device *self)
{
	if (acpi_madt_map() != AE_OK)
		return 0;

	mpacpi_ncpu = mpacpi_nintsrc = mpacpi_nioapic = 0;
	acpi_madt_walk(mpacpi_count, self);

	lapic_boot_init(mpacpi_lapic_base);

	if (mpacpi_ncpu == 0)
		return 0;

	acpi_madt_walk(mpacpi_config_cpu, self);
	acpi_madt_walk(mpacpi_config_ioapic, self);

	acpi_madt_unmap();
	return 1;
}

struct mpacpi_pciroot {
	TAILQ_ENTRY(mpacpi_pciroot) mpr_list;
	ACPI_DEVICE_INFO mpr_devinfo;
	ACPI_HANDLE mpr_handle;
	unsigned int mpr_bus;
};

TAILQ_HEAD(, mpacpi_pciroot) mpacpi_pciroots;

static int
mpacpi_find_pciroots(void)
{
	ACPI_HANDLE sbhandle;

	TAILQ_INIT(&mpacpi_pciroots);

	if (AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &sbhandle) != AE_OK)
		return ENOENT;

	AcpiWalkNamespace(ACPI_TYPE_DEVICE, sbhandle, 256, mpacpi_pciroot_cb,
	    NULL, NULL);

	return 0;
}

/*
 * Callback function for a namespace walk through ACPI space, finding all
 * PCI root busses. We assume that all PCI roots match PNP0A03.
 * XXX perhaps should check level to make sure?
 */
static ACPI_STATUS
mpacpi_pciroot_cb(ACPI_HANDLE handle, UINT32 level, void *ct, void **status)
{
	ACPI_DEVICE_INFO devinfo;
	ACPI_STATUS ret;
	ACPI_NAMESPACE_NODE *node;
	ACPI_INTEGER val;
	struct mpacpi_pciroot *mpr;

	ret = AcpiGetObjectInfo(handle, &devinfo);
	if (ACPI_FAILURE(ret))
		return AE_OK;

	if (!(devinfo.Valid & ACPI_VALID_HID))
		return AE_OK;

	if (strncmp(devinfo.HardwareId, "PNP0A03", 7))
		return AE_OK;

	mpr = malloc(sizeof (struct mpacpi_pciroot), M_TEMP, M_WAITOK|M_ZERO);
	if (mpr != NULL) {
		node = AcpiNsMapHandleToNode(handle);
		ret = AcpiUtEvaluateNumericObject(METHOD_NAME__BBN, node, &val);
		if (ACPI_FAILURE(ret))
			mpr->mpr_bus = 0;
		else
			mpr->mpr_bus = ACPI_LOWORD(val);
		mpr->mpr_devinfo = devinfo;
		mpr->mpr_handle = handle;
		TAILQ_INSERT_TAIL(&mpacpi_pciroots, mpr, mpr_list);
	}
	return AE_OK;
}

/*
 * Callback for ACPI namespace walk that finds all objects responding to
 * the _PRT method, and retrieves all static (not having a link device)
 * entries. The link device cases are used for legacy IRQ get/set,
 * which we're not interested in here.
 */
static ACPI_STATUS
mpacpi_pciroute_cb(ACPI_HANDLE handle, UINT32 level, void *ct, void **status)
{
	unsigned int *bus = ct;
	ACPI_STATUS ret;
	ACPI_BUFFER buf;
	ACPI_PCI_ROUTING_TABLE *ptrp;
	char *p;
	struct mp_intr_map *mpi;
	struct mp_bus *mpb;
	struct ioapic_softc *ioapic;
	unsigned dev;
	int pin;

	ret = acpi_get(handle, &buf, AcpiGetIrqRoutingTable);
	if (ACPI_FAILURE(ret))
		return AE_OK;

	mpb = &mp_busses[*bus];
	mpb->mb_intrs = NULL;
	mpb->mb_name = "pci";
	mpb->mb_idx = *bus;
	mpb->mb_intr_print = mpacpi_print_pci_intr;
	mpb->mb_intr_cfg = NULL;
	mpb->mb_data = 0;

	for (p = buf.Pointer; ; p += ptrp->Length) {
		ptrp = (ACPI_PCI_ROUTING_TABLE *)p;
		if (ptrp->Length == 0)
			break;
		dev = ACPI_HIWORD(ptrp->Address);
		if (ptrp->Source[0] != 0)
			continue;
		ioapic = ioapic_find_bybase(ptrp->SourceIndex);
		if (ioapic == NULL)
			continue;
		mpi = &mp_intrs[mpacpi_intr_index++];
		mpi->bus = mpb;
		mpi->bus_pin = (dev << 2) | ptrp->Pin;
		mpi->type = MPS_INTTYPE_INT;

		/* Defaults for PCI (active low, level triggered) */
		mpi->redir = (IOAPIC_REDLO_DEL_LOPRI<<IOAPIC_REDLO_DEL_SHIFT) |
		    IOAPIC_REDLO_LEVEL | IOAPIC_REDLO_ACTLO;
		mpi->flags = MPS_INTPO_ACTLO | (MPS_INTTR_LEVEL << 2);
		mpi->cpu_id = 0;

		pin = ptrp->SourceIndex - ioapic->sc_apic_vecbase;
		mpi->ioapic = ioapic;
		mpi->ioapic_pin = pin;
		mpi->ioapic_ih = APIC_INT_VIA_APIC |
		    (ioapic->sc_apicid << APIC_INT_APIC_SHIFT) |
		    (pin << APIC_INT_PIN_SHIFT);
		ioapic->sc_pins[pin].ip_map = mpi;
		mpi->next = mpb->mb_intrs;
		mpb->mb_intrs = mpi;
	}
	AcpiOsFree(buf.Pointer);
	(*bus)++;
	return AE_OK;
}

static ACPI_STATUS
mpacpi_pcicount_cb(ACPI_HANDLE handle, UINT32 level, void *ct, void **status)
{
	unsigned int *countp = ct;
	ACPI_STATUS ret;
	ACPI_BUFFER buf;

	ret = acpi_get(handle, &buf, AcpiGetIrqRoutingTable);
	if (!ACPI_FAILURE(ret)) {
		AcpiOsFree(buf.Pointer);
		(*countp)++;
	}
	return AE_OK;
}

static ACPI_STATUS
mpacpi_pcircount_cb(ACPI_HANDLE handle, UINT32 level, void *ct, void **status)
{
	int *countp = ct;
	ACPI_STATUS ret;
	ACPI_BUFFER buf;
	ACPI_PCI_ROUTING_TABLE *PrtElement;
	UINT8 *Buffer;

	ret = acpi_get(handle, &buf, AcpiGetIrqRoutingTable);
	if (!ACPI_FAILURE(ret)) {
		for (Buffer = buf.Pointer; ; Buffer += PrtElement->Length) {
			PrtElement = (ACPI_PCI_ROUTING_TABLE *)Buffer;
			if (PrtElement->Length == 0)
				break;
			(*countp)++;
		}
		AcpiOsFree(buf.Pointer);
	}
	return AE_OK;
}

/*
 * Get the number of PCI busses and their numbers. Walk down from each
 * root bus, starting at that root bus number.
 */
static void
mpacpi_count_pci(void)
{
	struct mpacpi_pciroot *mpr;
	unsigned int bus;

	mpacpi_maxpci = 0;
	mpacpi_npci = 0;

	TAILQ_FOREACH(mpr, &mpacpi_pciroots, mpr_list) {
		bus = mpr->mpr_bus;
		mpacpi_pcicount_cb(mpr->mpr_handle, 0, &bus, NULL);
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, mpr->mpr_handle, 256,
		    mpacpi_pcicount_cb, &bus, NULL);
		mpacpi_npci += (bus - mpr->mpr_bus);
		if (--bus > mpacpi_maxpci)
			mpacpi_maxpci = bus;
	}
}

/*
 * Set up the interrupt config lists, in the same format as the mpbios
 * does.
 */
static void
mpacpi_config_irouting(void)
{
	struct mpacpi_pciroot *mpr;
	int nintr;
	int i, index;
	struct mp_bus *mbp;
	struct mp_intr_map *mpi;
	struct ioapic_softc *ioapic;
	unsigned int bus;

	nintr = mpacpi_nintsrc + NUM_LEGACY_IRQS - 1;
	TAILQ_FOREACH(mpr, &mpacpi_pciroots, mpr_list) {
		mpacpi_pcircount_cb(mpr->mpr_handle, 0, &nintr, NULL);
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, mpr->mpr_handle, 256,
		    mpacpi_pcircount_cb, &nintr, NULL);
	}

	mp_isa_bus = mpacpi_maxpci + 1;
	mp_nbus = mp_isa_bus  + 1;
	mp_nintr = nintr + mpacpi_nintsrc + NUM_LEGACY_IRQS - 1;

	mp_busses = malloc(sizeof(struct mp_bus) * mp_nbus, M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (mp_busses == NULL)
		panic("can't allocate mp_busses");

	mp_intrs = malloc(sizeof(struct mp_intr_map) * mp_nintr, M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (mp_intrs == NULL)
		panic("can't allocate mp_intrs");

	mbp = &mp_busses[mp_isa_bus];
	mbp->mb_name = "isa";
	mbp->mb_idx = 0;
	mbp->mb_intr_print = mpacpi_print_isa_intr;
	mbp->mb_intr_cfg = NULL;
	mbp->mb_intrs = &mp_intrs[0];
	mbp->mb_data = 0;

	ioapic = ioapic_find_bybase(0);
	if (ioapic == NULL)
		panic("can't find first ioapic");

	/*
	 * Set up default identity mapping for ISA irqs to first ioapic.
	 */
	for (i = index = 0; i < NUM_LEGACY_IRQS; i++) {
		if (i == 2)
			continue;
		mpi = &mp_intrs[index];
		if (index < (NUM_LEGACY_IRQS - 2))
			mpi->next = &mp_intrs[index + 1];
		else
			mpi->next = NULL;
		mpi->bus = mbp;
		mpi->bus_pin = i;
		mpi->ioapic_pin = i;
		mpi->ioapic = ioapic;
		mpi->type = MPS_INTTYPE_INT;
		mpi->flags = MPS_INTPO_ACTHI | (MPS_INTTR_EDGE << 2);
		mpi->cpu_id = 0;
		mpi->redir = (IOAPIC_REDLO_DEL_LOPRI<<IOAPIC_REDLO_DEL_SHIFT);
		mpi->ioapic_ih = APIC_INT_VIA_APIC |
		    (ioapic->sc_apicid << APIC_INT_APIC_SHIFT) |
		    (i << APIC_INT_PIN_SHIFT);
		ioapic->sc_pins[i].ip_map = mpi;
		index++;
	}
	
	mpacpi_intr_index = index;
	if (acpi_madt_map() != AE_OK)
		panic("failed to map the MADT a second time");

	acpi_madt_walk(mpacpi_nonpci_intr, &mpacpi_intr_index);
	acpi_madt_unmap();

	TAILQ_FOREACH(mpr, &mpacpi_pciroots, mpr_list) {
		bus = mpr->mpr_bus;
		mpacpi_pciroute_cb(mpr->mpr_handle, 0, &bus, NULL);
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, mpr->mpr_handle, 256,
		    mpacpi_pciroute_cb, &bus, NULL);
	}
	mp_nintr = mpacpi_intr_index;
}

/*
 * XXX code duplication with mpbios.c
 */
static void
mpacpi_print_pci_intr(int intr)
{
	aprint_normal(" device %d INT_%c", (intr>>2)&0x1f, 'A' + (intr & 0x3));
}

static void
mpacpi_print_isa_intr(int intr)
{
	aprint_normal(" irq %d", intr);
}

static const char inttype_fmt[] = "\177\020"
		"f\0\2type\0" "=\1NMI\0" "=\2SMI\0" "=\3ExtINT\0";

static const char flagtype_fmt[] = "\177\020"
		"f\0\2pol\0" "=\1Act Hi\0" "=\3Act Lo\0"
		"f\2\2trig\0" "=\1Edge\0" "=\3Level\0";

static void
mpacpi_print_intr(struct mp_intr_map *mpi)
{
	char buf[256];
	int pin;
	struct ioapic_softc *sc;
	char *busname;

	sc = mpi->ioapic;
	pin = mpi->ioapic_pin;
	if (mpi->bus != NULL)
		busname = mpi->bus->mb_name;
	else {
		switch (mpi->type) {
		case MPS_INTTYPE_NMI:
			busname = "NMI";
			break;
		case MPS_INTTYPE_SMI:
			busname = "SMI";
			break;
		case MPS_INTTYPE_ExtINT:
			busname = "ExtINT";
			break;
		default:
			busname = "<unknown>";
			break;
		}
	}

	aprint_normal("%s: int%d attached to %s",
	    sc ? sc->sc_pic.pic_dev.dv_xname : "local apic",
	    pin, busname);

	if (mpi->bus != NULL) {
		if (mpi->bus->mb_idx != -1)
			aprint_normal("%d", mpi->bus->mb_idx);
		(*(mpi->bus->mb_intr_print))(mpi->bus_pin);
	}

	aprint_normal(" (type %s",
	    bitmask_snprintf(mpi->type, inttype_fmt, buf, sizeof(buf)));

	aprint_normal(" flags %s)\n",
	    bitmask_snprintf(mpi->flags, flagtype_fmt, buf, sizeof(buf)));
}



int
mpacpi_find_interrupts(void)
{
	ACPI_OBJECT_LIST arglist;
	ACPI_OBJECT arg;
	ACPI_STATUS ret;
	int i;

#ifdef MPBIOS
	/*
	 * If MPBIOS was enabled, and did the work (because the initial
	 * MADT scan failed for some reason), there's nothing left to
	 * do here. Same goes for the case where no I/O APICS were found.
	 */
	if (mpbios_scanned || mpacpi_nioapic == 0)
		return 0;
#endif

	/*
	 * Switch us into APIC mode by evaluating the _PIC(1).
	 * Needs to be done now, since it has an effect on
	 * the interrupt information we're about to retrieve.
	 */
	arglist.Count = 1;
	arglist.Pointer = &arg;
	arg.Type = ACPI_TYPE_INTEGER;
	arg.Integer.Value = 1;	/* I/O APIC mode (0 = PIC, 2 = IOSAPIC) */
	ret = AcpiEvaluateObject(NULL, "\\_PIC", &arglist, NULL);
	if (ACPI_FAILURE(ret)) {
		if (mp_verbose)
			aprint_normal("mpacpi: switch to APIC mode failed\n");
		return 0;
	}

	mpacpi_find_pciroots();
	mpacpi_count_pci();
	if (mp_verbose)
		aprint_normal("mpacpi: found %d PCI busses, max bus # is %d\n",
		    mpacpi_npci, mpacpi_maxpci);
	mpacpi_config_irouting();
	if (mp_verbose)
		for (i = 0; i < mp_nintr; i++)
			mpacpi_print_intr(&mp_intrs[i]);
	mp_verbose = 1;
	return 0;
}
