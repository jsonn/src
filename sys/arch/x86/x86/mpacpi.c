/*	$NetBSD: mpacpi.c,v 1.48.12.2 2007/10/02 22:56:20 joerg Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpacpi.c,v 1.48.12.2 2007/10/02 22:56:20 joerg Exp $");

#include "acpi.h"
#include "opt_acpi.h"
#include "opt_mpbios.h"
#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

#include <machine/specialreg.h>
#include <machine/cpuvar.h>
#include <machine/bus.h>
#include <machine/mpacpi.h>
#include <machine/mpbiosvar.h>

#include <machine/i82093reg.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_madt.h>

#include <dev/cons.h>

#include "pci.h"
#include "ioapic.h"
#include "lapic.h"

#include "locators.h"

#define ACPI_STA_OK (ACPI_STA_DEV_PRESENT|ACPI_STA_DEV_ENABLED|ACPI_STA_DEV_OK)

/* XXX room for PCI-to-PCI bus */
#define BUS_BUFFER (16)

#if NPCI > 0
struct mpacpi_pcibus {
	TAILQ_ENTRY(mpacpi_pcibus) mpr_list;
	ACPI_HANDLE mpr_handle;		/* Same thing really, but.. */
	ACPI_BUFFER mpr_buf;		/* preserve _PRT */
	int mpr_bus;			/* PCI bus number */
};

static TAILQ_HEAD(, mpacpi_pcibus) mpacpi_pcibusses;

#endif

static int mpacpi_cpuprint(void *, const char *);
static int mpacpi_ioapicprint(void *, const char *);

/* acpi_madt_walk callbacks */
static ACPI_STATUS mpacpi_count(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_config_cpu(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_config_ioapic(APIC_HEADER *, void *);
static ACPI_STATUS mpacpi_nonpci_intr(APIC_HEADER *, void *);

#if NPCI > 0
/* Callbacks for the ACPI namespace walk */
static ACPI_STATUS mpacpi_pcibus_cb(ACPI_HANDLE, UINT32, void *, void **);
static int mpacpi_derive_bus(ACPI_HANDLE, struct acpi_softc *);

static int mpacpi_pcircount(struct mpacpi_pcibus *);
static int mpacpi_pciroute(struct mpacpi_pcibus *);
static int mpacpi_find_pcibusses(struct acpi_softc *);

static void mpacpi_print_pci_intr(int);
#endif

static void mpacpi_config_irouting(struct acpi_softc *);

static void mpacpi_print_intr(struct mp_intr_map *);
static void mpacpi_print_isa_intr(int);

static void mpacpi_user_continue(const char *fmt, ...);

int mpacpi_nioapic;			/* number of ioapics */
int mpacpi_ncpu;			/* number of cpus */
int mpacpi_nintsrc;			/* number of non-device interrupts */

#if NPCI > 0
static int mpacpi_npci;
static int mpacpi_maxpci;
static int mpacpi_npciroots;
#endif

struct mp_intr_map *mpacpi_sci_override;

static int mpacpi_intr_index;
static paddr_t mpacpi_lapic_base = LAPIC_BASE;

int mpacpi_step;
int mpacpi_force;

static int
mpacpi_cpuprint(void *aux, const char *pnp)
{
	struct cpu_attach_args *caa = aux;

	if (pnp)
		printf("cpu at %s", pnp);
	printf(" apid %d", caa->cpu_number);
	return (UNCONF);
}

static int
mpacpi_ioapicprint(void *aux, const char *pnp)
{
	struct apic_attach_args *aaa = aux;

	if (pnp)
		printf("ioapic at %s", pnp);
	printf(" apid %d", aaa->apic_id);
	return (UNCONF);
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
	MADT_NMI_SOURCE *ioapic_nmi;
	MADT_LOCAL_APIC_NMI *lapic_nmi;
	MADT_INTERRUPT_OVERRIDE *isa_ovr;
	struct pic *pic;
	extern struct acpi_softc *acpi_softc;	/* XXX */

	switch (hdrp->Type) {
	case APIC_NMI:
		ioapic_nmi = (MADT_NMI_SOURCE *)hdrp;
		pic = intr_findpic(ioapic_nmi->Interrupt);
		if (pic == NULL)
			break;
#if NIOAPIC == 0
		if (pic->pic_type == PIC_IOAPIC)
			break;
#endif
		mpi = &mp_intrs[*index];
		(*index)++;
		mpi->next = NULL;
		mpi->bus = NULL;
		mpi->type = MPS_INTTYPE_NMI;
		mpi->ioapic = pic;
		pin = ioapic_nmi->Interrupt - pic->pic_vecbase;
		mpi->ioapic_pin = pin;
		mpi->bus_pin = -1;
		mpi->redir = (IOAPIC_REDLO_DEL_NMI<<IOAPIC_REDLO_DEL_SHIFT);
#if NIOAPIC > 0
		if (pic->pic_type == PIC_IOAPIC) {
			((struct ioapic_softc *)pic)->sc_pins[pin].ip_map = mpi;
			mpi->ioapic_ih = APIC_INT_VIA_APIC |
			    (pic->pic_apicid << APIC_INT_APIC_SHIFT) |
			    (pin << APIC_INT_PIN_SHIFT);
		} else
#endif
			mpi->ioapic_ih = pin;
		mpi->flags = ioapic_nmi->Polarity |
		    (ioapic_nmi->TriggerMode << 2);
		mpi->global_int = ioapic_nmi->Interrupt;
		break;
	case APIC_LOCAL_NMI:
		lapic_nmi = (MADT_LOCAL_APIC_NMI *)hdrp;
		mpi = &mp_intrs[*index];
		(*index)++;
		mpi->next = NULL;
		mpi->bus = NULL;
		mpi->ioapic = NULL;
		mpi->type = MPS_INTTYPE_NMI;
		mpi->ioapic_pin = lapic_nmi->Lint;
		mpi->cpu_id = lapic_nmi->ProcessorId;
		mpi->redir = (IOAPIC_REDLO_DEL_NMI<<IOAPIC_REDLO_DEL_SHIFT);
		mpi->global_int = -1;
		break;
	case APIC_XRUPT_OVERRIDE:
		isa_ovr = (MADT_INTERRUPT_OVERRIDE *)hdrp;
		if (mp_verbose) {
			printf("mpacpi: ISA interrupt override %d -> %d (%d/%d)\n",
			    isa_ovr->Source, isa_ovr->Interrupt,
			    isa_ovr->Polarity,
			    isa_ovr->TriggerMode);
		}
		if (isa_ovr->Source > 15 || isa_ovr->Source == 2 ||
		    (isa_ovr->Source == 0 && isa_ovr->Interrupt == 2 &&
			(acpi_softc->sc_quirks & ACPI_QUIRK_IRQ0)))
			break;
		pic = intr_findpic(isa_ovr->Interrupt);
		if (pic == NULL)
			break;
#if NIOAPIC == 0
		if (pic->pic_type == PIC_IOAPIC)
			break;
#endif
		pin = isa_ovr->Interrupt - pic->pic_vecbase;
		lindex = isa_ovr->Source;
		/*
		 * IRQ 2 was skipped in the default setup.
		 */
		if (lindex > 2)
			lindex--;
		mpi = &mp_intrs[lindex];
#if NIOAPIC > 0
		if (pic->pic_type == PIC_IOAPIC) {
			mpi->ioapic_ih = APIC_INT_VIA_APIC |
			    (pic->pic_apicid << APIC_INT_APIC_SHIFT) |
			    (pin << APIC_INT_PIN_SHIFT);
		} else
#endif
			mpi->ioapic_ih = pin;
		mpi->bus_pin = isa_ovr->Source;
		mpi->ioapic = (struct pic *)pic;
		mpi->ioapic_pin = pin;
		mpi->sflags |= MPI_OVR;
		mpi->redir = 0;
		mpi->global_int = isa_ovr->Interrupt;
		switch (isa_ovr->Polarity) {
		case MPS_INTPO_ACTHI:
			mpi->redir &= ~IOAPIC_REDLO_ACTLO;
			break;
		case MPS_INTPO_DEF:
		case MPS_INTPO_ACTLO:
			mpi->redir |= IOAPIC_REDLO_ACTLO;
			break;
		}
		mpi->redir |= (IOAPIC_REDLO_DEL_FIXED<<IOAPIC_REDLO_DEL_SHIFT);
		switch (isa_ovr->TriggerMode) {
		case MPS_INTTR_DEF:
		case MPS_INTTR_LEVEL:
			mpi->redir |= IOAPIC_REDLO_LEVEL;
			break;
		case MPS_INTTR_EDGE:
			mpi->redir &= ~IOAPIC_REDLO_LEVEL;
			break;
		}
		mpi->flags = isa_ovr->Polarity | (isa_ovr->TriggerMode << 2);
#if NIOAPIC > 0
		if (pic->pic_type == PIC_IOAPIC)
			((struct ioapic_softc *)pic)->sc_pins[pin].ip_map = mpi;
#endif
		if (isa_ovr->Source == AcpiGbl_FADT->SciInt)
			mpacpi_sci_override = mpi;
			
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
	MADT_ADDRESS_OVERRIDE *lop;

	switch (hdrp->Type) {
	case APIC_PROCESSOR:
		mpacpi_ncpu++;
		break;
	case APIC_IO:
		mpacpi_nioapic++;
		break;
	case APIC_NMI:
	case APIC_LOCAL_NMI:
		mpacpi_nintsrc++;
		break;
	case APIC_ADDRESS_OVERRIDE:
		lop = (MADT_ADDRESS_OVERRIDE *)hdrp;
		mpacpi_lapic_base = lop->Address;;
	default:
		break;
	}
	return AE_OK;
}

static ACPI_STATUS
mpacpi_config_cpu(APIC_HEADER *hdrp, void *aux)
{
	struct device *parent = aux;
	MADT_PROCESSOR_APIC *p;
	struct cpu_attach_args caa;
	int cpunum = 0;
	int locs[CPUBUSCF_NLOCS];

#if defined(MULTIPROCESSOR) || defined(IOAPIC)
	if (mpacpi_ncpu > 1)
		cpunum = lapic_cpu_number();
#endif

	if (hdrp->Type == APIC_PROCESSOR) {
		p = (MADT_PROCESSOR_APIC *)hdrp;
		if (p->ProcessorEnabled) {
			if (p->LocalApicId != cpunum)
				caa.cpu_role = CPU_ROLE_AP;
			else
				caa.cpu_role = CPU_ROLE_BP;
			caa.cpu_number = p->LocalApicId;
			caa.cpu_func = &mp_cpu_funcs;
			locs[CPUBUSCF_APID] = caa.cpu_number;
			config_found_sm_loc(parent, "cpubus", locs,
				&caa, mpacpi_cpuprint, config_stdsubmatch);
		}
	}
	return AE_OK;
}

static ACPI_STATUS
mpacpi_config_ioapic(APIC_HEADER *hdrp, void *aux)
{
	struct device *parent = aux;
	struct apic_attach_args aaa;
	MADT_IO_APIC *p;
	int locs[IOAPICBUSCF_NLOCS];

	if (hdrp->Type == APIC_IO) {
		p = (MADT_IO_APIC *)hdrp;
		aaa.apic_id = p->IoApicId;
		aaa.apic_address = p->Address;
		aaa.apic_version = -1;
		aaa.flags = IOAPIC_VWIRE;
		aaa.apic_vecbase = p->Interrupt;
		locs[IOAPICBUSCF_APID] = aaa.apic_id;
		config_found_sm_loc(parent, "ioapicbus", locs, &aaa,
			mpacpi_ioapicprint, config_stdsubmatch);
	}
	return AE_OK;
}

int
mpacpi_scan_apics(struct device *self, int *ncpup, int *napic)
{
	int rv = 0;

	if (acpi_madt_map() != AE_OK)
		return 0;

	mpacpi_ncpu = mpacpi_nintsrc = mpacpi_nioapic = 0;
	acpi_madt_walk(mpacpi_count, self);

#if NLAPIC > 0
	lapic_boot_init(mpacpi_lapic_base);
#endif

	acpi_madt_walk(mpacpi_config_cpu, self);

	if (mpacpi_ncpu == 0)
		goto done;

	acpi_madt_walk(mpacpi_config_ioapic, self);

#if NPCI > 0
	/*
	 * If PCI routing tables can't be built we report failure
	 * and let MPBIOS do the work.
	 */
	if (!mpacpi_force &&
	    (acpi_find_quirks() & (ACPI_QUIRK_BADPCI)) != 0)
		goto done;
#endif
	rv = 1;
done:
	*ncpup = mpacpi_ncpu;
	*napic = mpacpi_nioapic;
	acpi_madt_unmap();
	return rv;
}

#if NPCI > 0

/*
 * Find all PCI busses from ACPI namespace and construct mpacpi_pcibusses list.
 *
 * Note:
 * We cannot find all PCI busses in the system from ACPI namespace.
 * For example, a PCI-to-PCI bridge on an add-on PCI card is not
 * described in the ACPI namespace.
 * We search valid devices which have _PRT (PCI interrupt routing table)
 * method.
 * Such devices are either one of PCI root bridge or PCI-to-PCI bridge.
 */
static int
mpacpi_find_pcibusses(struct acpi_softc *acpi)
{
	ACPI_HANDLE sbhandle;

	if (AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &sbhandle) != AE_OK)
		return ENOENT;
	TAILQ_INIT(&mpacpi_pcibusses);
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, sbhandle, 100,
		    mpacpi_pcibus_cb, acpi, NULL);
	return 0;
}

static const char * const pciroot_hid[] = {
	"PNP0A03",			/* PCI root bridge */
	NULL
};

/*
 * mpacpi_derive_bus:
 *
 * Derive PCI bus number for the ACPI handle.
 *
 * If a device is not a PCI root bridge, it doesn't have _BBN method
 * and we have no direct method to know the bus number.
 * We have to walk up to search its root bridge and then walk down
 * to resolve the bus number.
 */
static int
mpacpi_derive_bus(ACPI_HANDLE handle, struct acpi_softc *acpi)
{
	ACPI_HANDLE parent, current;
	ACPI_STATUS rv;
	ACPI_INTEGER val;
	ACPI_DEVICE_INFO *devinfo;
	ACPI_BUFFER buf;
	struct ac_dev {
		TAILQ_ENTRY(ac_dev) list;
		ACPI_HANDLE handle;
	};
	TAILQ_HEAD(, ac_dev) dev_list;
	struct ac_dev *dev;
	pcireg_t binf, class, dvid;
	pcitag_t tag;
	int bus;

	bus = -1;
	TAILQ_INIT(&dev_list);

	/* first, search parent root bus */
	for (current = handle;; current = parent) {
		buf.Pointer = NULL;
		buf.Length = ACPI_ALLOCATE_BUFFER;
		rv = AcpiGetObjectInfo(current, &buf);
		if (ACPI_FAILURE(rv))
			return -1;

		devinfo = buf.Pointer;
		/* add this device to the list only if it's active */
		if ((devinfo->Valid & ACPI_VALID_STA) == 0 ||
		    (devinfo->CurrentStatus & ACPI_STA_OK) == ACPI_STA_OK) {
			AcpiOsFree(buf.Pointer);
			dev = malloc(sizeof(struct ac_dev), M_TEMP,
			    M_WAITOK|M_ZERO);
			if (dev == NULL)
				return -1;
			dev->handle = current;
			TAILQ_INSERT_HEAD(&dev_list, dev, list);
		} else
			AcpiOsFree(buf.Pointer);

		rv = AcpiGetParent(current, &parent);
		if (ACPI_FAILURE(rv))
			return -1;

		buf.Pointer = NULL;
		buf.Length = ACPI_ALLOCATE_BUFFER;
		rv = AcpiGetObjectInfo(parent, &buf);
		if (ACPI_FAILURE(rv))
			return -1;

		devinfo = buf.Pointer;

		if (acpi_match_hid(devinfo, pciroot_hid)) {
			rv = acpi_eval_integer(parent, METHOD_NAME__BBN, &val);
			AcpiOsFree(buf.Pointer);
			if (ACPI_SUCCESS(rv))
				bus = ACPI_LOWORD(val);
			else
				/* assume bus = 0 */
				bus = 0;
			break;
		}

		AcpiOsFree(buf.Pointer);
	}

	/*
	 * second, we walk down from the root to the target
	 * resolving the bus number
	 */
	TAILQ_FOREACH(dev, &dev_list, list) {
		rv = acpi_eval_integer(dev->handle, METHOD_NAME__ADR, &val);
		if (ACPI_FAILURE(rv))
			return -1;

		tag = pci_make_tag(acpi->sc_pc, bus,
		    ACPI_HIWORD(val), ACPI_LOWORD(val));

		/* check if this device exists */
		dvid = pci_conf_read(acpi->sc_pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(dvid) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(dvid) == 0)
			return -1;

		/* check if this is a bridge device */
		class = pci_conf_read(acpi->sc_pc, tag, PCI_CLASS_REG);
		if (PCI_CLASS(class) != PCI_CLASS_BRIDGE ||
		    PCI_SUBCLASS(class) != PCI_SUBCLASS_BRIDGE_PCI)
			return -1;

		/* if this is a bridge, get secondary bus */
		binf = pci_conf_read(acpi->sc_pc, tag, PPB_REG_BUSINFO);
		bus = PPB_BUSINFO_SECONDARY(binf);
	}

	/* cleanup */
	while (!TAILQ_EMPTY(&dev_list)) {
		dev = TAILQ_FIRST(&dev_list);
		TAILQ_REMOVE(&dev_list, dev, list);
		free(dev, M_TEMP);
	}

	return bus;
}

/*
 * Callback function for a namespace walk through ACPI space, finding all
 * PCI root and subordinate busses.
 */
static ACPI_STATUS
mpacpi_pcibus_cb(ACPI_HANDLE handle, UINT32 level, void *p,
    void **status)
{
	ACPI_STATUS rv;
	ACPI_BUFFER buf;
	ACPI_INTEGER val;
	ACPI_DEVICE_INFO *devinfo;
	struct mpacpi_pcibus *mpr;
	struct acpi_softc *acpi = p;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;

	/* get _HID, _CID and _STA */
	rv = AcpiGetObjectInfo(handle, &buf);
	if (ACPI_FAILURE(rv))
		return AE_OK;

	devinfo = buf.Pointer;

	/* if this device is not active, ignore it */
	if ((devinfo->Valid & ACPI_VALID_STA) &&
	    (devinfo->CurrentStatus & ACPI_STA_OK) != ACPI_STA_OK)
		goto out;

	mpr = malloc(sizeof (struct mpacpi_pcibus), M_TEMP, M_WAITOK|M_ZERO);
	if (mpr == NULL) {
		AcpiOsFree(buf.Pointer);
		return AE_NO_MEMORY;
	}

	/* try get _PRT. if this fails, we're not interested in it */
	rv = acpi_get(handle, &mpr->mpr_buf, AcpiGetIrqRoutingTable);
	if (ACPI_FAILURE(rv)) {
		free(mpr, M_TEMP);
		goto out;
	}

	/* check whether this is PCI root bridge or not */
	if (acpi_match_hid(devinfo, pciroot_hid)) {
		/* this is PCI root bridge */

		/* get the bus number */
		rv = acpi_eval_integer(handle, METHOD_NAME__BBN, &val);
		if (ACPI_SUCCESS(rv)) {
			mpr->mpr_bus = ACPI_LOWORD(val);
		} else {
			/*
			 * This often happens on systems which have only
			 * one PCI root bus, assuming 0 will be ok.
			 *
			 * If there is a system that have multiple PCI
			 * root but doesn't describe _BBN for every root,
			 * the ASL is *broken*.
			 */
			if (mpacpi_npciroots != 0)
				panic("mpacpi: ASL is broken");

			printf("mpacpi: could not get bus number, "
				    "assuming bus 0\n");
			mpr->mpr_bus = 0;
		}
		if (mp_verbose)
			printf("mpacpi: found root PCI bus %d at level %u\n",
			    mpr->mpr_bus, level);
		mpacpi_npciroots++;
	} else {
		/* this is subordinate PCI bus (behind PCI-to-PCI bridge) */

		/* we have no direct method to get the bus number... */
		mpr->mpr_bus = mpacpi_derive_bus(handle, acpi);

		if (mpr->mpr_bus < 0) {
			if (mp_verbose)
				printf("mpacpi: failed to derive bus number, ignoring\n");
			free(mpr, M_TEMP);
			goto out;
		}
		if (mp_verbose)
			printf("mpacpi: found subordinate bus %d at level %u\n",
			    mpr->mpr_bus, level);
	}

	mpr->mpr_handle = handle;
	TAILQ_INSERT_TAIL(&mpacpi_pcibusses, mpr, mpr_list);

	if (mpr->mpr_bus > mpacpi_maxpci)
		mpacpi_maxpci = mpr->mpr_bus;

	mpacpi_npci++;

 out:
	AcpiOsFree(buf.Pointer);
	return AE_OK;
}

/*
 * Find all static PRT entries for a PCI bus.
 */
static int
mpacpi_pciroute(struct mpacpi_pcibus *mpr)
{
	ACPI_PCI_ROUTING_TABLE *ptrp;
        ACPI_HANDLE linkdev;
	char *p;
	struct mp_intr_map *mpi;
	struct mp_bus *mpb;
	struct pic *pic;
	unsigned dev;
	int pin;

	if (mp_verbose)
		printf("mpacpi: configuring PCI bus %d int routing\n",
		    mpr->mpr_bus);

	mpb = &mp_busses[mpr->mpr_bus];
	mpb->mb_intrs = NULL;
	mpb->mb_name = "pci";
	mpb->mb_idx = mpr->mpr_bus;
	mpb->mb_intr_print = mpacpi_print_pci_intr;
	mpb->mb_intr_cfg = NULL;
	mpb->mb_data = 0;

	for (p = mpr->mpr_buf.Pointer; ; p += ptrp->Length) {
		ptrp = (ACPI_PCI_ROUTING_TABLE *)p;
		if (ptrp->Length == 0)
			break;
		dev = ACPI_HIWORD(ptrp->Address);

		mpi = &mp_intrs[mpacpi_intr_index++];
		mpi->bus_pin = (dev << 2) | ptrp->Pin;
		mpi->bus = mpb;
		mpi->type = MPS_INTTYPE_INT;

		if (ptrp->Source[0] != 0) {
			if (mp_verbose > 1)
				printf("pciroute: dev %d INT%c on lnkdev %s\n",
				    dev, 'A' + ptrp->Pin, ptrp->Source);
			mpi->global_int = -1;
			mpi->sourceindex = ptrp->SourceIndex;
			if (AcpiGetHandle(ACPI_ROOT_OBJECT, ptrp->Source,
			    &linkdev) != AE_OK) {
				printf("AcpiGetHandle failed for '%s'\n",
				    ptrp->Source);
				continue;
			}
			/* acpi_allocate_resources(linkdev); */
			mpi->ioapic_pin = -1;
			mpi->linkdev = acpi_pci_link_devbyhandle(linkdev);
			acpi_pci_link_add_reference(mpi->linkdev, 0,
			    mpr->mpr_bus, dev, ptrp->Pin);
			mpi->ioapic = NULL;
			mpi->flags = MPS_INTPO_ACTLO | (MPS_INTTR_LEVEL << 2);
			if (mp_verbose > 1)
				printf("pciroute: done adding entry\n");
		} else {
			if (mp_verbose > 1)
				printf("pciroute: dev %d INT%c on globint %d\n",
				    dev, 'A' + ptrp->Pin, ptrp->SourceIndex);
			mpi->sourceindex = 0;
			mpi->global_int = ptrp->SourceIndex;
			pic = intr_findpic(ptrp->SourceIndex);
			if (pic == NULL)
				continue;
			/* Defaults for PCI (active low, level triggered) */
			mpi->redir =
			    (IOAPIC_REDLO_DEL_FIXED<<IOAPIC_REDLO_DEL_SHIFT) |
			    IOAPIC_REDLO_LEVEL | IOAPIC_REDLO_ACTLO;
			mpi->ioapic = pic;
			pin = ptrp->SourceIndex - pic->pic_vecbase;
			if (pic->pic_type == PIC_I8259 && pin > 15)
				panic("bad pin %d for legacy IRQ", pin);
			mpi->ioapic_pin = pin;
#if NIOAPIC > 0
			if (pic->pic_type == PIC_IOAPIC) {
				((struct ioapic_softc *)pic)->sc_pins[pin].ip_map = mpi;
				mpi->ioapic_ih = APIC_INT_VIA_APIC |
				    (pic->pic_apicid << APIC_INT_APIC_SHIFT) |
				    (pin << APIC_INT_PIN_SHIFT);
			} else
#endif
				mpi->ioapic_ih = pin;
			mpi->linkdev = NULL;
			mpi->flags = MPS_INTPO_ACTLO | (MPS_INTTR_LEVEL << 2);
			if (mp_verbose > 1)
				printf("pciroute: done adding entry\n");
		}

		mpi->cpu_id = 0;

		mpi->next = mpb->mb_intrs;
		mpb->mb_intrs = mpi;
	}

	AcpiOsFree(mpr->mpr_buf.Pointer);
	mpr->mpr_buf.Pointer = NULL;	/* be preventive to bugs */

	if (mp_verbose > 1)
		printf("pciroute: done\n");

	return 0;
}

/*
 * Count number of elements in _PRT
 */
static int
mpacpi_pcircount(struct mpacpi_pcibus *mpr)
{
	int count = 0;
	ACPI_PCI_ROUTING_TABLE *PrtElement;
	UINT8 *Buffer;

	for (Buffer = mpr->mpr_buf.Pointer;; Buffer += PrtElement->Length) {
		PrtElement = (ACPI_PCI_ROUTING_TABLE *)Buffer;
		if (PrtElement->Length == 0)
			break;
		count++;
	}

	return count;
}
#endif

/*
 * Set up the interrupt config lists, in the same format as the mpbios does.
 */
static void
mpacpi_config_irouting(struct acpi_softc *acpi)
{
#if NPCI > 0
	struct mpacpi_pcibus *mpr;
#endif
	int nintr;
	int i;
	struct mp_bus *mbp;
	struct mp_intr_map *mpi;
	struct pic *pic;

	nintr = mpacpi_nintsrc + NUM_LEGACY_IRQS - 1;
#if NPCI > 0
	TAILQ_FOREACH(mpr, &mpacpi_pcibusses, mpr_list) {
		nintr += mpacpi_pcircount(mpr);
	}

	mp_isa_bus = mpacpi_maxpci + BUS_BUFFER; /* XXX */
#else
	mp_isa_bus = 0;
#endif
	mp_nbus = mp_isa_bus + 1;
	mp_nintr = nintr;

	mp_busses = malloc(sizeof(struct mp_bus) * mp_nbus, M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (mp_busses == NULL)
		panic("can't allocate mp_busses");

	mp_intrs = malloc(sizeof(struct mp_intr_map) * mp_nintr, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (mp_intrs == NULL)
		panic("can't allocate mp_intrs");

	mbp = &mp_busses[mp_isa_bus];
	mbp->mb_name = "isa";
	mbp->mb_idx = 0;
	mbp->mb_intr_print = mpacpi_print_isa_intr;
	mbp->mb_intr_cfg = NULL;
	mbp->mb_intrs = &mp_intrs[0];
	mbp->mb_data = 0;

	pic = intr_findpic(0);
	if (pic == NULL)
		panic("mpacpi: can't find first PIC");
#if NIOAPIC == 0
	if (pic->pic_type == PIC_IOAPIC)
		panic("mpacpi: ioapic but no i8259?");
#endif

	/*
	 * Set up default identity mapping for ISA irqs to first ioapic.
	 */
	for (i = mpacpi_intr_index = 0; i < NUM_LEGACY_IRQS; i++) {
		if (i == 2)
			continue;
		mpi = &mp_intrs[mpacpi_intr_index];
		if (mpacpi_intr_index < (NUM_LEGACY_IRQS - 2))
			mpi->next = &mp_intrs[mpacpi_intr_index + 1];
		else
			mpi->next = NULL;
		mpi->bus = mbp;
		mpi->bus_pin = i;
		mpi->ioapic_pin = i;
		mpi->ioapic = pic;
		mpi->type = MPS_INTTYPE_INT;
		mpi->cpu_id = 0;
		mpi->redir = 0;
#if NIOAPIC > 0
		if (pic->pic_type == PIC_IOAPIC) {
			mpi->ioapic_ih = APIC_INT_VIA_APIC |
			    (pic->pic_apicid << APIC_INT_APIC_SHIFT) |
			    (i << APIC_INT_PIN_SHIFT);
			mpi->redir =
			    (IOAPIC_REDLO_DEL_FIXED<<IOAPIC_REDLO_DEL_SHIFT);
			((struct ioapic_softc *)pic)->sc_pins[i].ip_map = mpi;
		} else
#endif
			mpi->ioapic_ih = i;

		mpi->flags = MPS_INTPO_DEF | (MPS_INTTR_DEF << 2);
		mpi->global_int = i;
		mpacpi_intr_index++;
	}

	mpacpi_user_continue("done setting up mp_bus array and ISA maps");

	if (acpi_madt_map() == AE_OK) {
		acpi_madt_walk(mpacpi_nonpci_intr, &mpacpi_intr_index);
		acpi_madt_unmap();
	}

	mpacpi_user_continue("done with non-PCI interrupts");

#if NPCI > 0
	TAILQ_FOREACH(mpr, &mpacpi_pcibusses, mpr_list) {
		mpacpi_pciroute(mpr);
	}
#endif

	mpacpi_user_continue("done routing PCI interrupts");

	mp_nintr = mpacpi_intr_index;
}

/*
 * XXX code duplication with mpbios.c
 */

#if NPCI > 0
static void
mpacpi_print_pci_intr(int intr)
{
	printf(" device %d INT_%c", (intr>>2)&0x1f, 'A' + (intr & 0x3));
}
#endif

static void
mpacpi_print_isa_intr(int intr)
{
	printf(" irq %d", intr);
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
	struct pic *sc;
	const char *busname;

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

	if (mpi->linkdev != NULL)
		printf("linkdev %s attached to %s",
		    acpi_pci_link_name(mpi->linkdev), busname);
	else
		printf("%s: pin %d attached to %s",
		    sc ? sc->pic_dev.dv_xname : "local apic",
		    pin, busname);

	if (mpi->bus != NULL) {
		if (mpi->bus->mb_idx != -1)
			printf("%d", mpi->bus->mb_idx);
		(*(mpi->bus->mb_intr_print))(mpi->bus_pin);
	}

	printf(" (type %s",
	    bitmask_snprintf(mpi->type, inttype_fmt, buf, sizeof(buf)));

	printf(" flags %s)\n",
	    bitmask_snprintf(mpi->flags, flagtype_fmt, buf, sizeof(buf)));
}


int
mpacpi_find_interrupts(void *self)
{
#if NIOAPIC > 0
	ACPI_OBJECT_LIST arglist;
	ACPI_OBJECT arg;
	ACPI_STATUS rv;
#endif
	struct acpi_softc *acpi = self;
	int i;

#ifdef MPBIOS
	/*
	 * If MPBIOS was enabled, and did the work (because the initial
	 * MADT scan failed for some reason), there's nothing left to
	 * do here. Same goes for the case where no I/O APICS were found.
	 */
	if (mpbios_scanned)
		return 0;
#endif

#if NIOAPIC > 0
	if (mpacpi_nioapic != 0) {
		/*
		 * Switch us into APIC mode by evaluating _PIC(1).
		 * Needs to be done now, since it has an effect on
		 * the interrupt information we're about to retrieve.
		 */
		arglist.Count = 1;
		arglist.Pointer = &arg;
		arg.Type = ACPI_TYPE_INTEGER;
		arg.Integer.Value = 1;	/* I/O APIC (0 = PIC, 2 = IOSAPIC) */
		rv = AcpiEvaluateObject(NULL, "\\_PIC", &arglist, NULL);
		if (ACPI_FAILURE(rv)) {
			if (mp_verbose)
				printf("mpacpi: switch to APIC mode failed\n");
			return 0;
		}
	}
#endif

#if NPCI > 0
	mpacpi_user_continue("finding PCI busses ");
	mpacpi_find_pcibusses(acpi);
	if (mp_verbose)
		printf("mpacpi: %d PCI busses\n", mpacpi_npci);
#endif
	mpacpi_config_irouting(acpi);
	if (mp_verbose)
		for (i = 0; i < mp_nintr; i++)
			mpacpi_print_intr(&mp_intrs[i]);
	return 0;
}

#if NPCI > 0

int
mpacpi_pci_attach_hook(struct device *parent, struct device *self,
		       struct pcibus_attach_args *pba)
{
	struct mp_bus *mpb;

#ifdef MPBIOS
	if (mpbios_scanned != 0)
		return ENOENT;
#endif

	if (TAILQ_EMPTY(&mpacpi_pcibusses))
		return 0;

	/*
	 * If this bus is not found in mpacpi_find_pcibusses
	 * (i.e. behind PCI-to-PCI bridge), register as an extra bus.
	 *
	 * at this point, mp_busses[] are as follows:
	 *  mp_busses[0 .. mpacpi_maxpci] : PCI
	 *  mp_busses[mpacpi_maxpci + BUS_BUFFER] : ISA
	 */
	if (pba->pba_bus >= mp_isa_bus) {
		intr_add_pcibus(pba);
		return 0;
	}

	mpb = &mp_busses[pba->pba_bus];
	if (mpb->mb_name != NULL) {
		if (strcmp(mpb->mb_name, "pci"))
			return EINVAL;
	} else
		/*
		 * As we cannot find all PCI-to-PCI bridge in
		 * mpacpi_find_pcibusses, some of the MP_busses may remain
		 * uninitialized.
		 */
		mpb->mb_name = "pci";

	mpb->mb_configured = 1;
	mpb->mb_pci_bridge_tag = pba->pba_bridgetag;
	mpb->mb_pci_chipset_tag = pba->pba_pc;

	if (mp_verbose)
		printf("%s: added to list as bus %d\n", parent->dv_xname,
		    pba->pba_bus);


	if (pba->pba_bus > mpacpi_maxpci)
		mpacpi_maxpci = pba->pba_bus;

	return 0;
}

int
mpacpi_scan_pci(struct device *self, struct pcibus_attach_args *pba,
	        cfprint_t print)
{
	int i;
	struct mp_bus *mpb;
	struct pci_attach_args;

	for (i = 0; i < mp_nbus; i++) {
		mpb = &mp_busses[i];
		if (mpb->mb_name == NULL)
			continue;
		if (!strcmp(mpb->mb_name, "pci") && mpb->mb_configured == 0) {
			pba->pba_bus = i;
			config_found_ia(self, "pcibus", pba, print);
		}
	}
	return 0;
}

#endif

int
mpacpi_findintr_linkdev(struct mp_intr_map *mip)
{
	int irq, line, pol, trig;
	struct pic *pic;
	int pin;

	if (mip->linkdev == NULL)
		return ENOENT;

	irq = acpi_pci_link_route_interrupt(mip->linkdev, mip->sourceindex,
	    &line, &pol, &trig);
	if (mp_verbose)
		printf("linkdev %s returned ACPI global int %d\n",
		    acpi_pci_link_name(mip->linkdev), line);
	if (irq == X86_PCI_INTERRUPT_LINE_NO_CONNECTION)
		return ENOENT;
	if (irq != line)
		panic("mpacpi_findintr_linkdev: irq mismatch");

	/*
	 * Convert ACPICA values to MPS values
	 */
	if (pol == ACPI_ACTIVE_LOW)
		pol = MPS_INTPO_ACTLO;
	else 
		pol = MPS_INTPO_ACTHI;
 
	if (trig == ACPI_EDGE_SENSITIVE)
		trig = MPS_INTTR_EDGE;
	else
		trig = MPS_INTTR_LEVEL;
 
	mip->flags = pol | (trig << 2);
	mip->global_int = irq;
	pic = intr_findpic(irq);
	if (pic == NULL)
		return ENOENT;
	mip->ioapic = pic;
	pin = irq - pic->pic_vecbase;

	if (pic->pic_type == PIC_IOAPIC) {
#if NIOAPIC > 0
		mip->redir = (IOAPIC_REDLO_DEL_FIXED<<IOAPIC_REDLO_DEL_SHIFT);
		if (pol ==  MPS_INTPO_ACTLO)
			mip->redir |= IOAPIC_REDLO_ACTLO;
		if (trig ==  MPS_INTTR_LEVEL)
			mip->redir |= IOAPIC_REDLO_LEVEL;
		mip->ioapic_ih = APIC_INT_VIA_APIC |
		    (pic->pic_apicid << APIC_INT_APIC_SHIFT) |
		    (pin << APIC_INT_PIN_SHIFT);
		((struct ioapic_softc *)pic)->sc_pins[pin].ip_map = mip;
		mip->ioapic_pin = pin;
#else
		return ENOENT;
#endif
	} else
		mip->ioapic_ih = pin;
	return 0;
}

static void
mpacpi_user_continue(const char *fmt, ...)
{
	va_list ap;

	if (!mpacpi_step)
		return;

	printf("mpacpi: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("<press any key to continue>\n>");
	cngetc();
}
