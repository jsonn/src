/* $NetBSD: hypervisor.c,v 1.8.2.3 2005/01/18 15:09:04 bouyer Exp $ */

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

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 *      This product includes software developed by Christian Limpach.
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
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hypervisor.c,v 1.8.2.3 2005/01/18 15:09:04 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include "xencons.h"
#include "xennet.h"
#include "xbd.h"
#include "xenkbc.h"
#include "vga_xen.h"
#include "npx.h"

#include "opt_xen.h"

#include <machine/xen.h>
#include <machine/hypervisor.h>
#include <machine/evtchn.h>

#ifdef DOM0OPS
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>
#include <machine/kernfs_machdep.h>
#include <dev/pci/pcivar.h>
#endif

#if NXENNET > 0
#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <machine/if_xennetvar.h>
#endif

#if NXBD > 0
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/bufq.h>
#include <dev/dkvar.h>
#include <machine/xbdvar.h>
#endif

#if NXENKBC > 0
#include <dev/pckbport/pckbportvar.h>
#include <machine/xenkbcvar.h>
#endif

#if NVGA_XEN > 0
#include <machine/bus.h>
#include <machine/vga_xenvar.h>
#endif

int	hypervisor_match(struct device *, struct cfdata *, void *);
void	hypervisor_attach(struct device *, struct device *, void *);

CFATTACH_DECL(hypervisor, sizeof(struct device),
    hypervisor_match, hypervisor_attach, NULL, NULL);

static int hypervisor_print(void *, const char *);

union hypervisor_attach_cookie {
	const char *hac_device;		/* first elem of all */
#if NXENKBC > 0
	struct xenkbc_attach_args hac_xenkbc;
#endif
#if NVGA_XEN > 0
	struct xen_vga_attach_args hac_vga_xen;
#endif
#if NXENCONS > 0
	struct xencons_attach_args hac_xencons;
#endif
#if NXENNET > 0
	struct xennet_attach_args hac_xennet;
#endif
#if NXBD > 0
	struct xbd_attach_args hac_xbd;
#endif
#if NNPX > 0
	struct xen_npx_attach_args hac_xennpx;
#endif
};


/*
 * Probe for the hypervisor; always succeeds.
 */
int
hypervisor_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct hypervisor_attach_args *haa = aux;

	if (strcmp(haa->haa_busname, "hypervisor") == 0)
		return 1;
	return 0;
}

#if NXENNET > 0
static void
scan_finish(struct device *parent)
{

	xennet_scan_finish(parent);
}
#endif /* NXENNET > 0 */

/*
 * Attach the hypervisor.
 */
void
hypervisor_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#ifdef DOM0OPS
	struct pcibus_attach_args pba;
#endif
	union hypervisor_attach_cookie hac;

	printf("\n");

	init_events();

#if NXENKBC > 0
	hac.hac_xenkbc.xa_device = "xenkbc";
	config_found(self, &hac.hac_xenkbc, hypervisor_print);
#endif

#if NVGA_XEN > 0
	hac.hac_vga_xen.xa_device = "vga_xen";
	hac.hac_vga_xen.xa_iot = X86_BUS_SPACE_IO;
	hac.hac_vga_xen.xa_memt = X86_BUS_SPACE_MEM;
	config_found(self, &hac.hac_vga_xen, hypervisor_print);
#endif

#if NXENCONS > 0
	hac.hac_xencons.xa_device = "xencons";
	config_found(self, &hac.hac_xencons, hypervisor_print);
#endif
#if NXENNET > 0
	hac.hac_xennet.xa_device = "xennet";
	xennet_scan(self, &hac.hac_xennet, hypervisor_print);
#endif
#if NXBD > 0
	hac.hac_xbd.xa_device = "xbd";
	xbd_scan(self, &hac.hac_xbd, hypervisor_print);
#endif
#if NNPX > 0
	hac.hac_xennpx.xa_device = "npx";
	config_found(self, &hac.hac_xennpx, hypervisor_print);
#endif
#ifdef DOM0OPS
	if (xen_start_info.flags & SIF_PRIVILEGED) {
		physdev_op_t physdev_op;
		int i, j, busnum;

		physdev_op.cmd = PHYSDEVOP_PCI_PROBE_ROOT_BUSES;
		if (HYPERVISOR_physdev_op(&physdev_op) < 0) {
			printf("hypervisor: PHYSDEVOP_PCI_PROBE_ROOT_BUSES failed\n");
		}
#ifdef DEBUG
		printf("PCI_PROBE_ROOT_BUSES: ");
		for (i = 0; i < 256/32; i++)
			printf("0x%x ", physdev_op.u.pci_probe_root_buses.busmask[i]);
		printf("\n");
#endif
		memset(pci_bus_attached, 0, sizeof(u_int32_t) * 256 / 32);
		for (i = 0, busnum = 0; i < 256/32; i++) {
			u_int32_t mask = 
			    physdev_op.u.pci_probe_root_buses.busmask[i];
			for (j = 0; j < 32; j++, busnum++) {
				if ((mask & (1 << j)) == 0)
					continue;
				if (pci_bus_attached[i] & (1 << j)) {
					printf("bus %d already attached\n",
					    busnum);
					continue;
				}
				pba.pba_iot = X86_BUS_SPACE_IO;
				pba.pba_memt = X86_BUS_SPACE_MEM;
				pba.pba_dmat = &pci_bus_dma_tag;
				pba.pba_dmat64 = 0;
				pba.pba_flags = PCI_FLAGS_MEM_ENABLED |
						PCI_FLAGS_IO_ENABLED;
				pba.pba_bridgetag = NULL;
				pba.pba_bus = busnum;
				config_found_ia(self, "pcibus", &pba,
				    pcibusprint);
			}
		}

		xenkernfs_init();
		xenprivcmd_init();
		xenmachmem_init();
		xenvfr_init();
	}
#endif
#if NXENNET > 0
	config_interrupts(self, scan_finish);
#endif
}

static int
hypervisor_print(aux, parent)
	void *aux;
	const char *parent;
{
	union hypervisor_attach_cookie *hac = aux;

	if (parent)
		aprint_normal("%s at %s", hac->hac_device, parent);
	return (UNCONF);
}

void
hypervisor_notify_via_evtchn(unsigned int port)
{
	evtchn_op_t op;

	op.cmd = EVTCHNOP_send;
	op.u.send.local_port = port;
	(void)HYPERVISOR_event_channel_op(&op);
}

#ifdef DOM0OPS

#define DIR_MODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

kernfs_parentdir_t *kernxen_pkt;

void
xenkernfs_init()
{
	kernfs_entry_t *dkt;

	KERNFS_ALLOCENTRY(dkt, M_TEMP, M_WAITOK);
	KERNFS_INITENTRY(dkt, DT_DIR, "xen", NULL, KFSsubdir, VDIR, DIR_MODE);
	kernfs_addentry(NULL, dkt);
	kernxen_pkt = KERNFS_ENTOPARENTDIR(dkt);
}
#endif
