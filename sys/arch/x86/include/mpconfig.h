/*	$NetBSD: mpconfig.h,v 1.3.2.1 2004/08/03 10:43:04 skrll Exp $	*/

/*
 * Definitions originally from the mpbios code, but now used for ACPI
 * MP config as well.
 */

#ifndef _X86_MPCONFIG_H
#define _X86_MPCONFIG_H

/*
 * XXX
 */
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#include <machine/pci_machdep.h>

/* 
 * Interrupt typess
 */
#define MPS_INTTYPE_INT         0
#define MPS_INTTYPE_NMI         1
#define MPS_INTTYPE_SMI         2
#define MPS_INTTYPE_ExtINT      3
 
#define MPS_INTPO_DEF           0
#define MPS_INTPO_ACTHI         1
#define MPS_INTPO_ACTLO         3
 
#define MPS_INTTR_DEF           0 
#define MPS_INTTR_EDGE          1
#define MPS_INTTR_LEVEL         3

#ifndef _LOCORE

struct mpbios_int;

struct mp_bus
{
	char *mb_name;		/* XXX bus name */
	int mb_idx;		/* XXX bus index */
	void (*mb_intr_print)(int);
	void (*mb_intr_cfg)(const struct mpbios_int *, u_int32_t *);
	struct mp_intr_map *mb_intrs;
	u_int32_t mb_data;	/* random bus-specific datum. */
	int mb_configured;	/* has been autoconfigured */
	pcitag_t *mb_pci_bridge_tag;
	pci_chipset_tag_t mb_pci_chipset_tag;
};

struct mp_intr_map
{
	struct mp_intr_map *next;
	struct mp_bus *bus;
	int bus_pin;
	struct ioapic_softc *ioapic;
	int ioapic_pin;
	int ioapic_ih;		/* int handle, for apic_intr_est */
	int type;		/* from mp spec intr record */
 	int flags;		/* from mp spec intr record */
	u_int32_t redir;
	int cpu_id;
	int global_int;		/* ACPI global interrupt number */
	int sflags;		/* other, software flags (see below) */
};

#define MPI_OVR		0x0001	/* Was overridden by an ACPI OVR */

#if defined(_KERNEL)
extern int mp_verbose;
extern struct mp_bus *mp_busses;
extern struct mp_intr_map *mp_intrs;
extern int mp_nintr;
extern int mp_isa_bus, mp_eisa_bus;
extern int mp_nbus;
#endif
#endif

#endif /* _X86_MPCONFIG_H */
