/*	$NetBSD: mpconfig.h,v 1.1.2.2 2003/01/07 21:11:49 thorpej Exp $	*/

/*
 * Definitions originally from the mpbios code, but now used for ACPI
 * MP config as well.
 */

#ifndef _I386_MPCONFIG_H
#define _I386_MPCONFIG_H

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
	void (*mb_intr_print) __P((int));
	void (*mb_intr_cfg) __P((const struct mpbios_int *, u_int32_t *));
	struct mp_intr_map *mb_intrs;
	u_int32_t mb_data;	/* random bus-specific datum. */
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
};

#if defined(_KERNEL)
extern int mp_verbose;
extern struct mp_bus *mp_busses;
extern struct mp_intr_map *mp_intrs;
extern int mp_nintr;
extern int mp_isa_bus, mp_eisa_bus;
extern int mp_nbus;
#endif
#endif

#endif /* _I386_MPCONFIG_H */
