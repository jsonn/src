/* $NetBSD: pci_machdep.h,v 1.11.4.1 2006/09/09 02:42:35 rpaulo Exp $ */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <prop/proplib.h>

/*
 * Machine-specific definitions for PCI autoconfiguration.
 */

#define __HAVE_PCI_CONF_HOOK

/*
 * Types provided to machine-independent PCI code
 */
typedef struct prep_pci_chipset *pci_chipset_tag_t;
typedef int pcitag_t;
typedef int pci_intr_handle_t;

/*
 * Forward declarations.
 */
struct pci_attach_args;

/* Per bus information structure */
struct prep_pci_chipset_businfo {
	SIMPLEQ_ENTRY(prep_pci_chipset_businfo) next;
	prop_dictionary_t	pbi_properties; /* chipset properties */
};

/*
 * prep-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
struct prep_pci_chipset {
	void		*pc_conf_v;
	void		(*pc_attach_hook)(struct device *,
			    struct device *, struct pcibus_attach_args *);
	int		(*pc_bus_maxdevs)(pci_chipset_tag_t, int);
	pcitag_t	(*pc_make_tag)(void *, int, int, int);
	pcireg_t	(*pc_conf_read)(void *, pcitag_t, int);
	void		(*pc_conf_write)(void *, pcitag_t, int, pcireg_t);

	void		*pc_intr_v;
	int		(*pc_intr_map)(struct pci_attach_args *, 
			    pci_intr_handle_t *);
	const char	*(*pc_intr_string)(void *, pci_intr_handle_t);
	const struct evcnt *(*pc_intr_evcnt)(void *, pci_intr_handle_t);
	void		*(*pc_intr_establish)(void *, pci_intr_handle_t,
			    int, int (*)(void *), void *);
	void		(*pc_intr_disestablish)(void *, void *);
	void		(*pc_conf_interrupt)(void *, int, int, int, int, int *);
	void		(*pc_decompose_tag)(void *, pcitag_t, int *,
			    int *, int *);
	int		(*pc_conf_hook)(pci_chipset_tag_t, int, int, int,
			    pcireg_t);
	SIMPLEQ_HEAD(, prep_pci_chipset_businfo) pc_pbi;
};

/*
 * Functions provided to machine-independent PCI code.
 */
#define	pci_attach_hook(p, s, pba)					\
    (*(pba)->pba_pc->pc_attach_hook)((p), (s), (pba))
#define	pci_bus_maxdevs(c, b)						\
    (*(c)->pc_bus_maxdevs)((c)->pc_conf_v, (b))
#define	pci_make_tag(c, b, d, f)					\
    (*(c)->pc_make_tag)((c)->pc_conf_v, (b), (d), (f))
#define	pci_conf_read(c, t, r)						\
    (*(c)->pc_conf_read)((c)->pc_conf_v, (t), (r))
#define	pci_conf_write(c, t, r, v)					\
    (*(c)->pc_conf_write)((c)->pc_conf_v, (t), (r), (v))
#define	pci_intr_map(pa, ihp)						\
    (*(pa)->pa_pc->pc_intr_map)((pa), (ihp))
#define	pci_intr_string(c, ih)						\
    (*(c)->pc_intr_string)((c)->pc_intr_v, (ih))
#define	pci_intr_evcnt(c, ih)						\
    (*(c)->pc_intr_evcnt)((c)->pc_intr_v, (ih))
#define	pci_intr_establish(c, ih, l, h, a)				\
    (*(c)->pc_intr_establish)((c)->pc_intr_v, (ih), (l), (h), (a))
#define	pci_intr_disestablish(c, iv)					\
    (*(c)->pc_intr_disestablish)((c)->pc_intr_v, (iv))
#define	pci_conf_interrupt(c, b, d, p, s, ip)				\
    (*(c)->pc_conf_interrupt)((c)->pc_conf_v, (b), (d), (p), (s), (ip))
#define	pci_decompose_tag(c, t, bp, dp, fp)				\
    (*(c)->pc_decompose_tag)((c)->pc_conf_v, (t), (bp), (dp), (fp))
#define	pci_conf_hook(c, b, d, f, i)					\
    (*(c)->pc_conf_hook)((c)->pc_conf_v, (b), (d), (f), (i))

#ifdef _KERNEL
/*
 * prep-specific PCI functions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
int prep_pci_bus_maxdevs(pci_chipset_tag_t, int);
int prep_pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *prep_pci_intr_string(void *, pci_intr_handle_t);
const struct evcnt *prep_pci_intr_evcnt(void *, pci_intr_handle_t);
void *prep_pci_intr_establish(void *, pci_intr_handle_t, int, int (*)(void *),
    void *);
void prep_pci_intr_disestablish(void *, void *);
void prep_pci_conf_interrupt(void *, int, int, int, int, int *);
int prep_pci_conf_hook(pci_chipset_tag_t, int, int, int, pcireg_t);

void prep_pci_get_chipset_tag_direct(pci_chipset_tag_t);
void prep_pci_get_chipset_tag_indirect(pci_chipset_tag_t);
void prep_pci_get_chipset_tag(pci_chipset_tag_t pc);

/*
 * prep-specific PCI data.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
extern struct powerpc_bus_dma_tag pci_bus_dma_tag;
#endif
