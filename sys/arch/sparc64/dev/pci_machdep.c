/*	$NetBSD: pci_machdep.c,v 1.25.4.6 2002/12/11 06:12:23 thorpej Exp $	*/

/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * functions expected by the MI PCI code.
 */

#ifdef DEBUG
#define SPDB_CONF	0x01
#define SPDB_INTR	0x04
#define SPDB_INTMAP	0x08
#define SPDB_INTFIX	0x10
#define SPDB_PROBE	0x20
int sparc_pci_debug = 0x0;
#define DPRINTF(l, s)	do { if (sparc_pci_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/ofw/ofw_pci.h>

#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/psychoreg.h>
#include <sparc64/dev/psychovar.h>

/* this is a base to be copied */
struct sparc_pci_chipset _sparc_pci_chipset = {
	NULL,
};

static pcitag_t
ofpci_make_tag(pci_chipset_tag_t pc, int node, int b, int d, int f)
{
	pcitag_t tag;

	tag = PCITAG_CREATE(node, b, d, f);

	/* Enable all the different spaces for this device */
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
		PCI_COMMAND_MEM_ENABLE|PCI_COMMAND_MASTER_ENABLE|
		PCI_COMMAND_IO_ENABLE);
	return (tag);
}

/*
 * functions provided to the MI code.
 */

void
pci_attach_hook(parent, self, pba)
	struct device *parent;
	struct device *self;
	struct pcibus_attach_args *pba;
{
}

int
pci_bus_maxdevs(pc, busno)
	pci_chipset_tag_t pc;
	int busno;
{

	return 32;
}

pcitag_t
pci_make_tag(pc, b, d, f)
	pci_chipset_tag_t pc;
	int b;
	int d;
	int f;
{
	struct ofw_pci_register reg;
	pcitag_t tag;
	int busrange[2];
	int node, len;
#ifdef DEBUG
	char name[80];
	bzero(name, sizeof(name));
#endif

	/* 
	 * Hunt for the node that corresponds to this device 
	 *
	 * We could cache this info in an array in the parent
	 * device... except then we have problems with devices
	 * attached below pci-pci bridges, and we would need to
	 * add special code to the pci-pci bridge to cache this
	 * info.
	 */

	tag = PCITAG_CREATE(-1, b, d, f);
	node = pc->rootnode;
	/*
	 * First make sure we're on the right bus.  If our parent
	 * has a bus-range property and we're not in the range,
	 * then we're obviously on the wrong bus.  So go up one
	 * level.
	 */
#ifdef DEBUG
	if (sparc_pci_debug & SPDB_PROBE) {
		OF_getprop(node, "name", &name, sizeof(name));
		printf("curnode %x %s\n", node, name);
	}
#endif
#if 0
	while ((OF_getprop(OF_parent(node), "bus-range", (void *)&busrange,
		sizeof(busrange)) == sizeof(busrange)) &&
		(b < busrange[0] || b > busrange[1])) {
		/* Out of range, go up one */
		node = OF_parent(node);
#ifdef DEBUG
		if (sparc_pci_debug & SPDB_PROBE) {
			OF_getprop(node, "name", &name, sizeof(name));
			printf("going up to node %x %s\n", node, name);
		}
#endif
	}
#endif	
	/*
	 * Now traverse all peers until we find the node or we find
	 * the right bridge. 
	 *
	 * XXX We go up one and down one to make sure nobody's missed.
	 * but this should not be necessary.
	 */
	for (node = ((node)); node; node = OF_peer(node)) {

#ifdef DEBUG
		if (sparc_pci_debug & SPDB_PROBE) {
			OF_getprop(node, "name", &name, sizeof(name));
			printf("checking node %x %s\n", node, name);
		}
#endif

#if 1
		/*
		 * Check for PCI-PCI bridges.  If the device we want is
		 * in the bus-range for that bridge, work our way down.
		 */
		while ((OF_getprop(node, "bus-range", (void *)&busrange,
			sizeof(busrange)) == sizeof(busrange)) &&
			(b >= busrange[0] && b <= busrange[1])) {
			/* Go down 1 level */
			node = OF_child(node);
#ifdef DEBUG
			if (sparc_pci_debug & SPDB_PROBE) {
				OF_getprop(node, "name", &name, sizeof(name));
				printf("going down to node %x %s\n",
					node, name);
			}
#endif
		}
#endif
		/* 
		 * We only really need the first `reg' property. 
		 *
		 * For simplicity, we'll query the `reg' when we
		 * need it.  Otherwise we could malloc() it, but
		 * that gets more complicated.
		 */
		len = OF_getproplen(node, "reg");
		if (len < sizeof(reg))
			continue;
		if (OF_getprop(node, "reg", (void *)&reg, sizeof(reg)) != len)
			panic("pci_probe_bus: OF_getprop len botch");

		if (b != OFW_PCI_PHYS_HI_BUS(reg.phys_hi))
			continue;
		if (d != OFW_PCI_PHYS_HI_DEVICE(reg.phys_hi))
			continue;
		if (f != OFW_PCI_PHYS_HI_FUNCTION(reg.phys_hi))
			continue;

		/* Got a match */
		tag = ofpci_make_tag(pc, node, b, d, f);

		return (tag);
	}
	/* No device found -- return a dead tag */
	return (tag);
}

void
pci_decompose_tag(pc, tag, bp, dp, fp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int *bp, *dp, *fp;
{

	if (bp != NULL)
		*bp = PCITAG_BUS(tag);
	if (dp != NULL)
		*dp = PCITAG_DEV(tag);
	if (fp != NULL)
		*fp = PCITAG_FUN(tag);
}

int
pci_enumerate_bus(struct pci_softc *sc,
    int (*match)(struct pci_attach_args *), struct pci_attach_args *pap)
{
	struct ofw_pci_register reg;
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag;
	pcireg_t class, csr;
	int node, b, d, f, ret;
	char name[30];
	extern int pci_config_dump;

	if (sc->sc_bridgetag)
		node = PCITAG_NODE(*sc->sc_bridgetag);
	else
		node = pc->rootnode;

	/* Turn on parity for the bus. */
	tag = ofpci_make_tag(pc, node, sc->sc_bus, 0, 0);
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_PARITY_ENABLE;
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

	if (pci_config_dump) pci_conf_print(pc, tag, NULL);

	for (node = OF_child(node); node != 0 && node != -1;
	     node = OF_peer(node)) {
		name[0] = name[29] = 0;
		OF_getprop(node, "name", name, sizeof(name));

		if (OF_getprop(node, "class-code", &class, sizeof(class)) != 
		    sizeof(class))
			continue;
		if (OF_getprop(node, "reg", &reg, sizeof(reg)) < sizeof(reg))
			panic("pci_enumerate_bus: \"%s\" regs too small", name);

		b = OFW_PCI_PHYS_HI_BUS(reg.phys_hi);
		d = OFW_PCI_PHYS_HI_DEVICE(reg.phys_hi);
		f = OFW_PCI_PHYS_HI_FUNCTION(reg.phys_hi);

		if (sc->sc_bus != b) {
			printf("%s: WARNING: incorrect bus # for \"%s\" "
			"(%d/%d/%d)\n", sc->sc_dev.dv_xname, name, b, d, f);
			continue;
		}

		tag = ofpci_make_tag(pc, node, b, d, f);

		/*
		 * Turn on parity and fast-back-to-back for the device.
		 */
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		if (csr & PCI_STATUS_BACKTOBACK_SUPPORT)
			csr |= PCI_COMMAND_BACKTOBACK_ENABLE;
		csr |= PCI_COMMAND_PARITY_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

		ret = pci_probe_device(sc, tag, match, pap);
		if (match != NULL && ret != 0)
			return (ret);
	}
	return (0);
}

/* assume we are mapped little-endian/side-effect */
pcireg_t
pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	struct psycho_pbm *pp = pc->cookie;
	struct psycho_softc *sc = pp->pp_sc;
	pcireg_t val = (pcireg_t)~0;

	DPRINTF(SPDB_CONF, ("pci_conf_read: tag %lx reg %x ", 
		(long)tag, reg));
	if (PCITAG_NODE(tag) != -1) {
		DPRINTF(SPDB_CONF, ("asi=%x addr=%qx (offset=%x) ...",
			sc->sc_configaddr._asi,
			(long long)(sc->sc_configaddr._ptr + 
				PCITAG_OFFSET(tag) + reg),
			(int)PCITAG_OFFSET(tag) + reg));

		val = bus_space_read_4(sc->sc_configtag, sc->sc_configaddr,
			PCITAG_OFFSET(tag) + reg);
	}
#ifdef DEBUG
	else DPRINTF(SPDB_CONF, ("pci_conf_read: bogus pcitag %x\n",
		(int)PCITAG_OFFSET(tag)));
#endif
	DPRINTF(SPDB_CONF, (" returning %08x\n", (u_int)val));

	return (val);
}

void
pci_conf_write(pc, tag, reg, data)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{
	struct psycho_pbm *pp = pc->cookie;
	struct psycho_softc *sc = pp->pp_sc;

	DPRINTF(SPDB_CONF, ("pci_conf_write: tag %lx; reg %x; data %x; ", 
		(long)PCITAG_OFFSET(tag), reg, (int)data));
	DPRINTF(SPDB_CONF, ("asi = %x; readaddr = %qx (offset = %x)\n",
		sc->sc_configaddr._asi,
		(long long)(sc->sc_configaddr._ptr + PCITAG_OFFSET(tag) + reg), 
		(int)PCITAG_OFFSET(tag) + reg));

	/* If we don't know it, just punt it.  */
	if (PCITAG_NODE(tag) == -1) {
		DPRINTF(SPDB_CONF, ("pci_conf_write: bad addr"));
		return;
	}
		
	bus_space_write_4(sc->sc_configtag, sc->sc_configaddr, 
		PCITAG_OFFSET(tag) + reg, data);
}

/*
 * interrupt mapping foo.
 * XXX: how does this deal with multiple interrupts for a device?
 */
int
pci_intr_map(pa, ihp)
	struct pci_attach_args *pa;
	pci_intr_handle_t *ihp;
{
	pcitag_t tag = pa->pa_tag;
	pcireg_t bhlc, ic;
	int bus_frequency, val, bus_node;
	int interrupts;
	int len, node = PCITAG_NODE(tag);
	char devtype[30];

	bus_node = OF_parent(node);
	len = OF_getproplen(bus_node, "clock-frequency");
	if (len < sizeof(bus_frequency)) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: clock-frequency len %d too small\n", len));
		return (ENODEV);
	}
	if (OF_getprop(bus_node, "clock-frequency", (void *)&bus_frequency, 
		sizeof(bus_frequency)) != len) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: could not read bus_frequency\n"));
		return (ENODEV);
	}

	len = OF_getproplen(node, "interrupts");
	if (len < sizeof(interrupts)) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: interrupts len %d too small\n", len));
		return (ENODEV);
	}
	if (OF_getprop(node, "interrupts", (void *)&interrupts, 
		sizeof(interrupts)) != len) {
		DPRINTF(SPDB_INTMAP,
			("pci_intr_map: could not read interrupts\n"));
		return (ENODEV);
	}

	if (OF_mapintr(node, &interrupts, sizeof(interrupts), 
		sizeof(interrupts)) < 0) {
		printf("OF_mapintr failed\n");
	}
	/* Try to find an IPL for this type of device. */
	if (OF_getprop(node, "device_type", &devtype, sizeof(devtype)) > 0) {
		for (len = 0;  intrmap[len].in_class; len++)
			if (strcmp(intrmap[len].in_class, devtype) == 0) {
				interrupts |= INTLEVENCODE(intrmap[len].in_lev);
				break;
			}
	}

	/*
	 * Initialize the latency timer register for busmaster devices
	 * to work properly.
	 *   latency-timer = min-grant * bus-freq / 4  (from FreeBSD)
	 */
	ic = pci_conf_read(pa->pa_pc, tag, PCI_INTERRUPT_REG);
	bus_frequency /= 1000000;
	val = min(PCI_MIN_GNT(ic) * bus_frequency / 4, 255);
	if (val > 0) {
		bhlc = pci_conf_read(pa->pa_pc, tag, PCI_BHLC_REG);
		if (PCI_LATTIMER(bhlc) < val) {
			bhlc &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
			bhlc |= (val << PCI_LATTIMER_SHIFT);
			pci_conf_write(pa->pa_pc, tag, PCI_BHLC_REG, bhlc);
		}
	}

	/* XXXX -- we use the ino.  What if there is a valid IGN? */
	*ihp = interrupts;
	return (0);
}

const char *
pci_intr_string(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{
	static char str[16];

	DPRINTF(SPDB_INTR, ("pci_intr_string: ih %u", ih));
	sprintf(str, "ivec %x", ih);
	DPRINTF(SPDB_INTR, ("; returning %s\n", str));

	return (str);
}

const struct evcnt *
pci_intr_evcnt(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{

	/* XXX for now, no evcnt parent reported */
	return NULL;
}

void *
pci_intr_establish(pc, ih, level, func, arg)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	int level;
	int (*func) __P((void *));
	void *arg;
{
	void *cookie;
	struct psycho_pbm *pp = (struct psycho_pbm *)pc->cookie;

	DPRINTF(SPDB_INTR, ("pci_intr_establish: ih %lu; level %d", (u_long)ih, level));
	cookie = bus_intr_establish(pp->pp_memt, ih, level, func, arg);

	DPRINTF(SPDB_INTR, ("; returning handle %p\n", cookie));
	return (cookie);
}

void
pci_intr_disestablish(pc, cookie)
	pci_chipset_tag_t pc;
	void *cookie;
{

	DPRINTF(SPDB_INTR, ("pci_intr_disestablish: cookie %p\n", cookie));

	/* XXX */
	panic("can't disestablish PCI interrupts yet");
}
