/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 *	$Id: pcivar.h,v 1.1.2.2 1994/08/09 00:47:55 mycroft Exp $
 */

/*
 * Definitions for PCI autoconfiguration.
 *
 * This file describes types and functions which are used for PCI
 * configuration.  Some of this information is machine-specific, and is
 * separated into pci_machdep.h.
 */

#include <i386/pci/pci_machdep.h>

struct pci_attach_args {
	int pa_bus;
	int pa_device;
	pcitag_t pa_tag;
	pcireg_t pa_id, pa_class;
};

pcitag_t pci_make_tag __P((int, int, int));
pcireg_t pci_conf_read __P((pcitag_t, int));
void pci_conf_write __P((pcitag_t, int, pcireg_t));
int pci_targmatch __P((struct cfdata *, struct pci_attach_args *));
int pci_map_mem __P((pcitag_t, int, vm_offset_t *, vm_offset_t *));
int pci_map_int __P((pcitag_t, struct intrhand *));
