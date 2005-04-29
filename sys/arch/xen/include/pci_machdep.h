/* $NetBSD: pci_machdep.h,v 1.3.2.2 2005/04/29 11:28:29 kent Exp $ */

/*
 * Copyright (c) 2005 Manuel Bouyer.
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
 *      This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _XEN_PCI_MACHDEP_H_
#define _XEN_PCI_MACHDEP_H_

extern struct x86_bus_dma_tag pci_bus_dma_tag;
extern u_int32_t pci_bus_attached[];

/* Some values appropriate for x86, from x86/include/pci_machdep.h */
#define __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
#define PCI_PREFER_IOSPACE

/* types provided to MI PCI */
struct xen_pci_tag {
	u_int8_t bus;
	u_int8_t device;
	u_int8_t function;
	u_int8_t _pad; /* pad to 32bits */
};

struct xen_intr_handle {
	int pirq;
	int evtch;
};

typedef struct xen_pci_tag pcitag_t;
typedef void *pci_chipset_tag_t;
typedef struct xen_intr_handle pci_intr_handle_t;

/* functions provided to MI PCI */
struct pci_attach_args;

void		pci_attach_hook(struct device *, struct device *,
		    struct pcibus_attach_args *);
int		pci_bus_maxdevs(pci_chipset_tag_t, int);
pcitag_t	pci_make_tag(pci_chipset_tag_t, int, int, int);
void		pci_decompose_tag(pci_chipset_tag_t, pcitag_t,
		    int *, int *, int *);
pcireg_t	pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void		pci_conf_write(pci_chipset_tag_t, pcitag_t, int,
		    pcireg_t);
int		pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char	*pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
const struct evcnt *pci_intr_evcnt(pci_chipset_tag_t, pci_intr_handle_t);
void		*pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t,
		    int, int (*)(void *), void *);
void		pci_intr_disestablish(pci_chipset_tag_t, void *);

/*
 * Section 6.2.4, `Miscellaneous Functions' of the PCI Specification,
 * says that 255 means `unknown' or `no connection' to the interrupt
 * controller on a PC.
 */
#define	X86_PCI_INTERRUPT_LINE_NO_CONNECTION	0xff
#endif /* _XEN_PCI_MACHDEP_H_ */
