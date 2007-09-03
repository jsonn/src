/* $NetBSD: mcpcia_pci.c,v 1.4.40.1 2007/09/03 14:22:26 yamt Exp $ */

/*
 * Copyright (c) 1998 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: mcpcia_pci.c,v 1.4.40.1 2007/09/03 14:22:26 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/mcpciareg.h>
#include <alpha/pci/mcpciavar.h>

#define	KV(_addr)	((void *)ALPHA_PHYS_TO_K0SEG((_addr)))

static void mcpcia_attach_hook __P((struct device *, struct device *,
	struct pcibus_attach_args *));
static int
mcpcia_bus_maxdevs __P((void *, int));
static pcitag_t
mcpcia_make_tag __P((void *, int, int, int));
static void
mcpcia_decompose_tag __P((void *, pcitag_t, int *, int *, int *));
static pcireg_t
mcpcia_conf_read __P((void *, pcitag_t, int));
static void
mcpcia_conf_write __P((void *, pcitag_t, int, pcireg_t));

void
mcpcia_pci_init(pc, v)
	pci_chipset_tag_t pc;
	void *v;
{
	pc->pc_conf_v = v;
	pc->pc_attach_hook = mcpcia_attach_hook;
	pc->pc_bus_maxdevs = mcpcia_bus_maxdevs;
	pc->pc_make_tag = mcpcia_make_tag;
	pc->pc_decompose_tag = mcpcia_decompose_tag;
	pc->pc_conf_read = mcpcia_conf_read;
	pc->pc_conf_write = mcpcia_conf_write;
}

static void
mcpcia_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

static int
mcpcia_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return (MCPCIA_MAXDEV);
}

static pcitag_t
mcpcia_make_tag(cpv, b, d, f)
	void *cpv;
	int b, d, f;
{
	pcitag_t tag;
	tag = (b << 21) | (d << 16) | (f << 13);
	return (tag);
}

static void
mcpcia_decompose_tag(cpv, tag, bp, dp, fp)
	void *cpv;
	pcitag_t tag;
	int *bp, *dp, *fp;
{
	if (bp != NULL)
		*bp = (tag >> 21) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 16) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 13) & 0x7;
}

static pcireg_t
mcpcia_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	struct mcpcia_config *ccp = cpv;
	pcireg_t *dp, data = (pcireg_t) -1;
	unsigned long paddr;

	/*
	 * There's nothing in slot 0 on a primary bus- don't even try.
	 */
	if ((tag >> 21) == 0 && ((u_int32_t) tag & 0x1f0000) == 0)
		return (data);

	if (ccp == NULL) {
		panic("NULL ccp in mcpcia_conf_read");
	}
	paddr =	(unsigned long) tag;
	paddr |= (3LL << 3);	/* 32 Bit PCI byte enables */
	paddr |= ((unsigned long) ((offset >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= ccp->cc_sysbase;
	dp = (pcireg_t *)KV(paddr);
	if (badaddr(dp, sizeof (*dp)) == 0) {
		data = *dp;
	}
	return (data);
}

static void
mcpcia_conf_write(cpv, tag, offset, data)
	void *cpv;
	pcitag_t tag;
	int offset;
	pcireg_t data;
{
	struct mcpcia_config *ccp = cpv;
	pcireg_t *dp;
	unsigned long paddr;

	/*
	 * There's nothing in slot 0 on a primary bus- don't even try.
	 */
	if ((tag >> 21) == 0 && ((u_int32_t) tag & 0x1f0000) == 0)
		return;

	if (ccp == NULL) {
		panic("NULL ccp in mcpcia_conf_write");
	}
	paddr =	(unsigned long) tag;
	paddr |= (3LL << 3);	/* 32 Bit PCI byte enables */
	paddr |= ((unsigned long) ((offset >> 2) << 7));
	paddr |= MCPCIA_PCI_CONF;
	paddr |= ccp->cc_sysbase;

	dp = (pcireg_t *)KV(paddr);
	*dp = data;
}
