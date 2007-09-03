/*	$NetBSD: dino.c,v 1.3.12.1 2007/09/03 14:25:34 yamt Exp $ */

/*	$OpenBSD: dino.c,v 1.5 2004/02/13 20:39:31 mickey Exp $	*/

/*
 * Copyright (c) 2003 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dino.c,v 1.3.12.1 2007/09/03 14:25:34 yamt Exp $");

/* #include "cardbus.h" */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/extent.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/intr.h>
#include <hp700/hp700/intr.h>

#include <hppa/include/vmparam.h>
#include <hp700/dev/cpudevs.h>

#if NCARDBUS > 0
#include <dev/cardbus/rbus.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define	DINO_MEM_CHUNK	0x800000

/* from machdep.c */
extern struct extent *hp700_io_extent;

struct dino_regs {
	u_int32_t	pad0;		/* 0x000 */
	u_int32_t	iar0;		/* 0x004 rw intr addr reg 0 */
	u_int32_t	iodc;		/* 0x008 rw iodc data/addr */
	u_int32_t	irr0;		/* 0x00c r  intr req reg 0 */
	u_int32_t	iar1;		/* 0x010 rw intr addr reg 1 */
	u_int32_t	irr1;		/* 0x014 r  intr req reg 1 */
	u_int32_t	imr;		/* 0x018 rw intr mask reg */
	u_int32_t	ipr;		/* 0x01c rw intr pending reg */
	u_int32_t	toc_addr;	/* 0x020 rw TOC addr reg */
	u_int32_t	icr;		/* 0x024 rw intr control reg */
	u_int32_t	ilr;		/* 0x028 r  intr level reg */
	u_int32_t	pad1;		/* 0x02c */
	u_int32_t	io_command;	/* 0x030  w command register */
	u_int32_t	io_status;	/* 0x034 r  status register */
	u_int32_t	io_control;	/* 0x038 rw control register */
	u_int32_t	pad2;		/* 0x03c AUX registers follow */
	u_int32_t	io_gsc_err_addr;/* 0x040 GSC error address */
	u_int32_t	io_err_info;	/* 0x044 error info register */
	u_int32_t	io_pci_err_addr;/* 0x048 PCI error address */
	u_int32_t	pad3[4];	/* 0x04c */
	u_int32_t	io_fbb_en;	/* 0x05c fast back2back enable reg */
	u_int32_t	io_addr_en;	/* 0x060 address enable reg */
	u_int32_t	pci_addr;	/* 0x064 PCI conf/io/mem addr reg */
	u_int32_t	pci_conf_data;	/* 0x068 PCI conf data reg */
	u_int32_t	pci_io_data;	/* 0x06c PCI io data reg */
	u_int32_t	pci_mem_data;	/* 0x070 PCI memory data reg */
	u_int32_t	pad4[0x740/4];	/* 0x074 */
	u_int32_t	gsc2x_config;	/* 0x7b4 GSC2X config reg */
	u_int32_t	pad5[0x48/4];	/* 0x7b8: BSRS registers follow */
	u_int32_t	gmask;		/* 0x800 GSC arbitration mask */
	u_int32_t	pamr;		/* 0x804 PCI arbitration mask */
	u_int32_t	papr;		/* 0x808 PCI arbitration priority */
	u_int32_t	damode;		/* 0x80c PCI arbitration mode */
	u_int32_t	pcicmd;		/* 0x810 PCI command register */
	u_int32_t	pcists;		/* 0x814 PCI status register */
	u_int32_t	pad6;		/* 0x818 */
	u_int32_t	mltim;		/* 0x81c PCI master latency timer */
	u_int32_t	brdg_feat;	/* 0x820 PCI bridge feature enable */
	u_int32_t	pciror;		/* 0x824 PCI read optimization reg */
	u_int32_t	pciwor;		/* 0x828 PCI write optimization reg */
	u_int32_t	pad7;		/* 0x82c */
	u_int32_t	tltim;		/* 0x830 PCI target latency reg */
};

struct dino_softc {
	struct  device sc_dv;

	int sc_ver;
	void *sc_ih;
	struct hp700_int_reg sc_int_reg;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_bh;
	bus_space_handle_t sc_memh;
	bus_dma_tag_t sc_dmat;
	volatile struct dino_regs *sc_regs;

	struct hppa_pci_chipset_tag sc_pc;
	struct hppa_bus_space_tag sc_iot;
	char sc_ioexname[20];
	struct extent *sc_ioex;
	struct hppa_bus_space_tag sc_memt;
	int sc_memrefcount[30];
	struct hppa_bus_dma_tag sc_dmatag;
};

int	dinomatch(struct device *, struct cfdata *, void *);
void	dinoattach(struct device *, struct device *, void *);

CFATTACH_DECL(dino, sizeof(struct dino_softc), dinomatch, dinoattach, NULL, 
    NULL);


void dino_attach_hook(struct device *, struct device *,
    struct pcibus_attach_args *);
void dino_enable_bus(struct dino_softc *, int);
int dino_maxdevs(void *, int);
pcitag_t dino_make_tag(void *, int, int, int);
void dino_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t dino_conf_read(void *, pcitag_t, int);
void dino_conf_write(void *, pcitag_t, int, pcireg_t);
int dino_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *dino_intr_string(void *, pci_intr_handle_t);
void *dino_intr_establish(void *, pci_intr_handle_t, int, 
    int (*handler)(void *), void *);
void dino_intr_disestablish(void *, void *);
void *dino_alloc_parent(struct device *, struct pci_attach_args *, int);
int dino_iomap(void *, bus_addr_t, bus_size_t, int, bus_space_handle_t *);
int dino_memmap(void *, bus_addr_t, bus_size_t, int, bus_space_handle_t *);
int dino_subregion(void *, bus_space_handle_t, bus_size_t, bus_size_t, 
    bus_space_handle_t *);
int dino_ioalloc(void *, bus_addr_t, bus_addr_t, bus_size_t,
    bus_size_t, bus_size_t, int, bus_addr_t *, bus_space_handle_t *);
int dino_memalloc(void *, bus_addr_t, bus_addr_t, bus_size_t, bus_size_t, 
    bus_size_t, int, bus_addr_t *, bus_space_handle_t *);
void dino_unmap(void *, bus_space_handle_t, bus_size_t);
void dino_free(void *, bus_space_handle_t, bus_size_t);
void dino_barrier(void *, bus_space_handle_t, bus_size_t, bus_size_t, int);
void *dino_vaddr(void *, bus_space_handle_t);
u_int8_t dino_r1(void *, bus_space_handle_t, bus_size_t);
u_int16_t dino_r2(void *, bus_space_handle_t, bus_size_t);
u_int32_t dino_r4(void *, bus_space_handle_t, bus_size_t);
u_int64_t dino_r8(void *, bus_space_handle_t, bus_size_t);
void dino_w1(void *, bus_space_handle_t, bus_size_t, u_int8_t);
void dino_w2(void *, bus_space_handle_t, bus_size_t, u_int16_t);
void dino_w4(void *, bus_space_handle_t, bus_size_t, u_int32_t);
void dino_w8(void *, bus_space_handle_t, bus_size_t, u_int64_t);
void dino_rm_1(void *, bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t);
void dino_rm_2(void *, bus_space_handle_t, bus_size_t, u_int16_t *, bus_size_t);
void dino_rm_4(void *, bus_space_handle_t, bus_size_t, u_int32_t *, bus_size_t);
void dino_rm_8(void *, bus_space_handle_t, bus_size_t, u_int64_t *, bus_size_t);
void dino_wm_1(void *, bus_space_handle_t, bus_size_t, const u_int8_t *, 
    bus_size_t);
void dino_wm_2(void *, bus_space_handle_t, bus_size_t, const u_int16_t *, 
    bus_size_t);
void dino_wm_4(void *, bus_space_handle_t, bus_size_t, const u_int32_t *, 
    bus_size_t);
void dino_wm_8(void *, bus_space_handle_t, bus_size_t, const u_int64_t *, 
    bus_size_t);
void dino_sm_1(void *, bus_space_handle_t, bus_size_t, u_int8_t, bus_size_t);
void dino_sm_2(void *, bus_space_handle_t, bus_size_t, u_int16_t, bus_size_t);
void dino_sm_4(void *, bus_space_handle_t, bus_size_t, u_int32_t, bus_size_t);
void dino_sm_8(void *, bus_space_handle_t, bus_size_t, u_int64_t, bus_size_t);
void dino_rrm_2(void *, bus_space_handle_t, bus_size_t, u_int16_t *, 
    bus_size_t);
void dino_rrm_4(void *, bus_space_handle_t, bus_size_t, u_int32_t *, 
    bus_size_t);
void dino_rrm_8(void *, bus_space_handle_t, bus_size_t, u_int64_t *, 
    bus_size_t);
void dino_wrm_2(void *, bus_space_handle_t, bus_size_t, const u_int16_t *, 
    bus_size_t);
void dino_wrm_4(void *, bus_space_handle_t, bus_size_t, const u_int32_t *, 
    bus_size_t);
void dino_wrm_8(void *, bus_space_handle_t, bus_size_t, const u_int64_t *, 
    bus_size_t);
void dino_rr_1(void *, bus_space_handle_t, bus_size_t, u_int8_t *, bus_size_t);
void dino_rr_2(void *, bus_space_handle_t, bus_size_t, u_int16_t *, bus_size_t);
void dino_rr_4(void *, bus_space_handle_t, bus_size_t, u_int32_t *, bus_size_t);
void dino_rr_8(void *, bus_space_handle_t, bus_size_t, u_int64_t *, bus_size_t);
void dino_wr_1(void *, bus_space_handle_t, bus_size_t, const u_int8_t *, 
    bus_size_t);
void dino_wr_2(void *, bus_space_handle_t, bus_size_t, const u_int16_t *, 
    bus_size_t);
void dino_wr_4(void *, bus_space_handle_t, bus_size_t, const u_int32_t *, 
    bus_size_t);
void dino_wr_8(void *, bus_space_handle_t, bus_size_t, const u_int64_t *, 
    bus_size_t);
void dino_rrr_2(void *, bus_space_handle_t, bus_size_t, u_int16_t *, 
    bus_size_t);
void dino_rrr_4(void *, bus_space_handle_t, bus_size_t, u_int32_t *, 
    bus_size_t);
void dino_rrr_8(void *, bus_space_handle_t, bus_size_t, u_int64_t *, 
    bus_size_t);
void dino_wrr_2(void *, bus_space_handle_t, bus_size_t, const u_int16_t *, 
    bus_size_t);
void dino_wrr_4(void *, bus_space_handle_t, bus_size_t, const u_int32_t *, 
    bus_size_t);
void dino_wrr_8(void *, bus_space_handle_t, bus_size_t, const u_int64_t *, 
    bus_size_t);
void dino_sr_1(void *, bus_space_handle_t, bus_size_t, u_int8_t, bus_size_t);
void dino_sr_2(void *, bus_space_handle_t, bus_size_t, u_int16_t, bus_size_t);
void dino_sr_4(void *, bus_space_handle_t, bus_size_t, u_int32_t, bus_size_t);
void dino_sr_8(void *, bus_space_handle_t, bus_size_t, u_int64_t, bus_size_t);
void dino_cp_1(void *, bus_space_handle_t, bus_size_t, bus_space_handle_t, 
    bus_size_t, bus_size_t);
void dino_cp_2(void *, bus_space_handle_t, bus_size_t, bus_space_handle_t, 
    bus_size_t, bus_size_t);
void dino_cp_4(void *, bus_space_handle_t, bus_size_t, bus_space_handle_t, 
    bus_size_t, bus_size_t);
void dino_cp_8(void *, bus_space_handle_t, bus_size_t, bus_space_handle_t, 
    bus_size_t, bus_size_t);
int dino_dmamap_create(void *, bus_size_t, int, bus_size_t, bus_size_t, int, 
    bus_dmamap_t *);
void dino_dmamap_destroy(void *, bus_dmamap_t);
int dino_dmamap_load(void *, bus_dmamap_t, void *, bus_size_t, struct proc *, 
    int);
int dino_dmamap_load_mbuf(void *, bus_dmamap_t, struct mbuf *, int);
int dino_dmamap_load_uio(void *, bus_dmamap_t, struct uio *, int);
int dino_dmamap_load_raw(void *, bus_dmamap_t, bus_dma_segment_t *, int, 
    bus_size_t, int);
void dino_dmamap_unload(void *, bus_dmamap_t);
void dino_dmamap_sync(void *, bus_dmamap_t, bus_addr_t, bus_size_t, int);
int dino_dmamem_alloc(void *, bus_size_t, bus_size_t, bus_size_t, 
    bus_dma_segment_t *, int, int *, int);
void dino_dmamem_free(void *, bus_dma_segment_t *, int);
int dino_dmamem_map(void *, bus_dma_segment_t *, int, size_t, void **, int);
void dino_dmamem_unmap(void *, void *, size_t);
paddr_t dino_dmamem_mmap(void *, bus_dma_segment_t *, int, off_t, int, int);


void
dino_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	struct dino_softc *sc = pba->pba_pc->_cookie;

	/* 
	 * The firmware enables only devices that are needed for booting. 
	 * So other devices will fail to map PCI MEM / IO when they attach. 
	 * Therefore we recursively walk all buses to simply enable everything.
	 */
	dino_enable_bus(sc, 0);
}

void
dino_enable_bus(struct dino_softc *sc, int bus)
{
	int func;
	int dev;
	pcitag_t tag;
	pcireg_t data;
	pcireg_t class;

	for (dev = 0; dev < 32; dev++) {
		tag = dino_make_tag(sc, bus, dev, 0);
		if (tag != -1 && dino_conf_read(sc, tag, 0) != 0xffffffff) {
			for (func = 0; func < 8; func++) {
				tag = dino_make_tag(sc, bus, dev, func);
				if (dino_conf_read(sc, tag, 0) != 0xffffffff) {
					data = dino_conf_read(sc, tag, 
					    PCI_COMMAND_STATUS_REG);
					dino_conf_write(sc, tag, 
					    PCI_COMMAND_STATUS_REG, 
					    PCI_COMMAND_IO_ENABLE |
					    PCI_COMMAND_MEM_ENABLE |
					    PCI_COMMAND_MASTER_ENABLE | data);
				}
			}
			class = dino_conf_read(sc, tag, PCI_CLASS_REG);
			if (PCI_CLASS(class) == PCI_CLASS_BRIDGE &&
			    PCI_SUBCLASS(class) == PCI_SUBCLASS_BRIDGE_PCI)
				dino_enable_bus(sc, bus + 1);
		}
	}
}

int
dino_maxdevs(void *v, int bus)
{
	return 32;
}

pcitag_t
dino_make_tag(void *v, int bus, int dev, int func)
{
	if (bus > 255 || dev > 31 || func > 7)
		panic("dino_make_tag: bad request");

	return (bus << 16) | (dev << 11) | (func << 8);
}

void
dino_decompose_tag(void *v, pcitag_t tag, int *bus, int *dev, int *func)
{
	*bus = (tag >> 16) & 0xff;
	*dev = (tag >> 11) & 0x1f;
	*func= (tag >>  8) & 0x07;
}

pcireg_t
dino_conf_read(void *v, pcitag_t tag, int reg)
{
	struct dino_softc *sc = v;
	volatile struct dino_regs *r = sc->sc_regs;
	pcireg_t data;

	r->pci_addr = tag | reg;
	data = r->pci_conf_data;
	return le32toh(data);
}

void
dino_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct dino_softc *sc = v;
	volatile struct dino_regs *r = sc->sc_regs;
	pcireg_t data1;

	/* fix coalescing config writes errata by interleaving w/ a read */
	r->pci_addr = tag | PCI_ID_REG;
	data1 = r->pci_conf_data;

	r->pci_addr = tag | reg;
	r->pci_conf_data = htole32(data);

	r->pci_addr = tag | PCI_ID_REG;
	data1 = r->pci_conf_data;
}

int
dino_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t reg;

	reg = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
	*ihp = PCI_INTERRUPT_LINE(reg);
	return *ihp < 0;
}

const char *
dino_intr_string(void *v, pci_intr_handle_t ih)
{
	static char buf[32];

	snprintf(buf, 32, "irq %ld", ih);
	return buf;
}

extern int cold;


void *
dino_intr_establish(void *v, pci_intr_handle_t ih,
    int pri, int (*handler)(void *), void *arg)
{
	struct dino_softc *sc = v;

	return hp700_intr_establish(&sc->sc_dv, pri, handler, arg, 
	    &sc->sc_int_reg, ih);
}

void
dino_intr_disestablish(void *v, void *cookie)
{
	panic("There is no hp700_intr_disestablish()!");
}


#if NCARDBUS > 0
void *
dino_alloc_parent(struct device *self, struct pci_attach_args *pa, int io)
{
	struct dino_softc *sc = pa->pa_pc->_cookie;
	struct extent *ex;
	bus_space_tag_t tag;
	bus_addr_t start;
	bus_size_t size;

	if (io) {
		ex = sc->sc_ioex;
		tag = pa->pa_iot;
		start = 0xa000;
		size = 0x1000;
	} else {
		ex = hp700_io_extent;
		tag = pa->pa_memt;
		start = ex->ex_start; /* XXX or 0xf0800000? */
		size = DINO_MEM_CHUNK;
	}

	if (extent_alloc_subregion(ex, start, ex->ex_end, size, size,
	    EX_NOBOUNDARY, EX_NOWAIT, &start))
		return NULL;
	extent_free(ex, start, size, EX_NOWAIT);
	return rbus_new_root_share(tag, ex, start, size, start);
}
#endif

int
dino_iomap(void *v, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct dino_softc *sc = v;
	int error;

	if (!(flags & BUS_SPACE_MAP_NOEXTENT) &&
	    (error = extent_alloc_region(sc->sc_ioex, bpa, size, EX_NOWAIT)))
		return error;

	if (bshp)
		*bshp = bpa;

	return 0;
}

int
dino_memmap(void *v, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct dino_softc *sc = v;
	volatile struct dino_regs *r = sc->sc_regs;
	u_int32_t reg;
	int error;

	reg = r->io_addr_en;
	reg |= 1 << ((bpa >> 23) & 0x1f);
#ifdef DEBUG
	if (reg & 0x80000001)
		panic("mapping outside the mem extent range");
#endif
	if ((error = bus_space_map(sc->sc_bt, bpa, size, flags, bshp)))
		return error;
	++sc->sc_memrefcount[((bpa >> 23) & 0x1f)];
	/* map into the upper bus space, if not yet mapped this 8M */
	if (reg != r->io_addr_en)
		r->io_addr_en = reg;
	return 0;
}

int
dino_subregion(void *v, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return 0;
}

int
dino_ioalloc(void *v, bus_addr_t rstart, bus_addr_t rend, bus_size_t size,
    bus_size_t align, bus_size_t boundary, int flags, bus_addr_t *addrp,
    bus_space_handle_t *bshp)
{
	struct dino_softc *sc = v;
	struct extent *ex = sc->sc_ioex;
	bus_addr_t bpa;
	int error;

	if (rstart < ex->ex_start || rend > ex->ex_end)
		panic("dino_ioalloc: bad region start/end");

	if ((error = extent_alloc_subregion(ex, rstart, rend, size,
	    align, boundary, EX_NOWAIT, &bpa)))
		return error;

	if (addrp)
		*addrp = bpa;
	if (bshp)
		*bshp = bpa;

	return 0;
}

int
dino_memalloc(void *v, bus_addr_t rstart, bus_addr_t rend, bus_size_t size,
    bus_size_t align, bus_size_t boundary, int flags, bus_addr_t *addrp,
    bus_space_handle_t *bshp)
{
	struct dino_softc *sc = v;
	volatile struct dino_regs *r = sc->sc_regs;
	u_int32_t reg;
	int i, error;

	/*
	 * Allow allocation only when PCI MEM is already maped.
	 * Needed to avoid allocation of I/O space used by devices that 
	 * have no driver in the current kernel.
	 * Dino can map PCI MEM in the range 0xf0800000..0xff800000 only.
	 */
	reg = r->io_addr_en;
	if (rstart < 0xf0800000 || rend >= 0xff800000 || reg == 0)
		return -1;
	/* Find used PCI MEM and narrow allocateble region down to it. */
	for (i = 1; i < 31; i++)
		if ((reg & 1 << i) != 0) {
			rstart = 0xf0000000 | i << 23;
			rend = (0xf0000000 | (i + 1) << 23) - 1;
			break;
		}
	if ((error = bus_space_alloc(sc->sc_bt, rstart, rend, size, align, 
	    boundary, flags, addrp, bshp)))
		return error;
	++sc->sc_memrefcount[((*bshp >> 23) & 0x1f)];
	return 0;
}

void
dino_unmap(void *v, bus_space_handle_t bsh, bus_size_t size)
{
	struct dino_softc *sc = v;
	volatile struct dino_regs *r = sc->sc_regs;

	if (bsh & 0xf0000000) {
		bus_space_unmap(sc->sc_bt, bsh, size);
		if (--sc->sc_memrefcount[((bsh >> 23) & 0x1f)] == 0)
			/* Unmap the upper PCI MEM space. */
			r->io_addr_en &= ~(1 << ((bsh >> 23) & 0x1f));
	} else {
		/* XXX gotta follow the BUS_SPACE_MAP_NOEXTENT flag */
		if (extent_free(sc->sc_ioex, bsh, size, EX_NOWAIT))
			printf("dino_unmap: ps 0x%lx, size 0x%lx\n"
			    "dino_unmap: can't free region\n", bsh, size);
	}
}

void
dino_free(void *v, bus_space_handle_t bh, bus_size_t size)
{
	/* should be enough */
	dino_unmap(v, bh, size);
}

void
dino_barrier(void *v, bus_space_handle_t h, bus_size_t o, bus_size_t l, int op)
{
	sync_caches();
}

void*
dino_vaddr(void *v, bus_space_handle_t h)
{
	struct dino_softc *sc = v;

	return bus_space_vaddr(sc->sc_bt, h);
}

u_int8_t
dino_r1(void *v, bus_space_handle_t h, bus_size_t o)
{
	h += o;
	if (h & 0xf0000000)
		return *(volatile u_int8_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		return *((volatile u_int8_t *)&r->pci_io_data + (h & 3));
	}
}

u_int16_t
dino_r2(void *v, bus_space_handle_t h, bus_size_t o)
{
	volatile u_int16_t *p;
	volatile u_int16_t d;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		d = le16toh(*p);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
		d = le16toh(*p);
	}

	return d;
}

u_int32_t
dino_r4(void *v, bus_space_handle_t h, bus_size_t o)
{
	u_int32_t data;

	h += o;
	if (h & 0xf0000000)
		data = *(volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		data = r->pci_io_data;
	}

	return le32toh(data);
}

u_int64_t
dino_r8(void *v, bus_space_handle_t h, bus_size_t o)
{
	h += o;
	if (h & 0xf0000000)
		return *(volatile u_int64_t *)h;
	else
		panic("dino_r8: not implemented");
}

void
dino_w1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv)
{
	h += o;
	if (h & 0xf0000000)
		*(volatile u_int8_t *)h = vv;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		*((volatile u_int8_t *)&r->pci_io_data + (h & 3)) = vv;
	}
}

void
dino_w2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	*p = htole16(vv);
}

void
dino_w4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv)
{
	h += o;
	vv = htole32(vv);
	if (h & 0xf0000000)
		*(volatile u_int32_t *)h = vv;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		r->pci_io_data = vv;
	}
}

void
dino_w8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv)
{
	h += o;
	if (h & 0xf0000000)
		*(volatile u_int64_t *)h = vv;
	else
		panic("dino_w8: not implemented");
}


void
dino_rm_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t *a, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int8_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
	}

	while (c--)
		*a++ = *p;
}

void
dino_rm_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	while (c--)
		*a++ = le16toh(*p);
}

void
dino_rm_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		p = (volatile u_int32_t *)&r->pci_io_data;
	}

	while (c--)
		*a++ = le32toh(*p);
}

void
dino_rm_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t *a, bus_size_t c)
{
	panic("dino_rm_8: not implemented");
}

void
dino_wm_1(void *v, bus_space_handle_t h, bus_size_t o, const u_int8_t *a, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int8_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
	}

	while (c--)
		*p = *a++;
}

void
dino_wm_2(void *v, bus_space_handle_t h, bus_size_t o, const u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	while (c--)
		*p = htole16(*a++);
}

void
dino_wm_4(void *v, bus_space_handle_t h, bus_size_t o, const u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		p = (volatile u_int32_t *)&r->pci_io_data;
	}

	while (c--)
		*p = htole32(*a++);
}

void
dino_wm_8(void *v, bus_space_handle_t h, bus_size_t o, const u_int64_t *a, bus_size_t c)
{
	panic("dino_wm_8: not implemented");
}

void
dino_sm_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int8_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
	}

	while (c--)
		*p = vv;
}

void
dino_sm_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	while (c--)
		*p = htole16(vv);
}

void
dino_sm_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		p = (volatile u_int32_t *)&r->pci_io_data;
	}

	while (c--)
		*p = htole32(vv);
}

void
dino_sm_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv, bus_size_t c)
{
	panic("dino_sm_8: not implemented");
}

void
dino_rrm_2(void *v, bus_space_handle_t h, bus_size_t o,
    u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	while (c--)
		*a++ = *p;
}

void
dino_rrm_4(void *v, bus_space_handle_t h, bus_size_t o,
    u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		p = (volatile u_int32_t *)&r->pci_io_data;
	}

	while (c--)
		*a++ = *p;
}

void
dino_rrm_8(void *v, bus_space_handle_t h, bus_size_t o,
    u_int64_t *a, bus_size_t c)
{
	panic("dino_rrm_8: not implemented");
}

void
dino_wrm_2(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int16_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		p = (volatile u_int16_t *)&r->pci_io_data;
		if (h & 2)
			p++;
	}

	while (c--)
		*p = *a++;
}

void
dino_wrm_4(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000)
		p = (volatile u_int32_t *)h;
	else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h;
		p = (volatile u_int32_t *)&r->pci_io_data;
	}

	while (c--)
		*p = *a++;
}

void
dino_wrm_8(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int64_t *a, bus_size_t c)
{
	panic("dino_wrm_8: not implemented");
}

void
dino_rr_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t *a, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int8_t *)h;
		while (c--)
			*a++ = *p++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
			*a++ = *p;
			if (!(++h & 3))
				r->pci_addr = h;
		}
	}
}

void
dino_rr_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		while (c--)
			*a++ = le16toh(*p++);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int16_t *)&r->pci_io_data;
			if (h & 2)
				p++;
			*a++ = le16toh(*p);
			h += 2;
			if (!(h & 2))
				r->pci_addr = h;
		}
	}
}

void
dino_rr_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int32_t *)h;
		while (c--)
			*a++ = le32toh(*p++);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		for (; c--; h += 4) {
			r->pci_addr = h;
			*a++ = le32toh(r->pci_io_data);
		}
	}
}

void
dino_rr_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t *a, bus_size_t c)
{
	panic("dino_rr_8: not implemented");
}

void
dino_wr_1(void *v, bus_space_handle_t h, bus_size_t o, const u_int8_t *a, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int8_t *)h;
		while (c--)
			*p++ = *a++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
			*p = *a++;
			if (!(++h & 3))
				r->pci_addr = h;
		}
	}
}

void
dino_wr_2(void *v, bus_space_handle_t h, bus_size_t o, const u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		while (c--)
			*p++ = htole16(*a++);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int16_t *)&r->pci_io_data;
			if (h & 2)
				p++;
			*p = htole16(*a++);
			h += 2;
			if (!(h & 2))
				r->pci_addr = h;
		}
	}
}

void
dino_wr_4(void *v, bus_space_handle_t h, bus_size_t o, const u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int32_t *)h;
		while (c--)
			*p++ = htole32(*a++);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		for (; c--; h += 4) {
			r->pci_addr = h;
			r->pci_io_data = htole32(*a++);
		}
	}
}

void
dino_wr_8(void *v, bus_space_handle_t h, bus_size_t o, const u_int64_t *a, bus_size_t c)
{
	panic("dino_wr_8: not implemented");
}

void
dino_rrr_2(void *v, bus_space_handle_t h, bus_size_t o,
    u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		while (c--)
			*a++ = *p++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int16_t *)&r->pci_io_data;
			if (h & 2)
				p++;
			*a++ = *p;
			h += 2;
			if (!(h & 2))
				r->pci_addr = h;
		}
	}
}

void
dino_rrr_4(void *v, bus_space_handle_t h, bus_size_t o,
    u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int32_t *)h;
		while (c--)
			*a++ = *p++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		for (; c--; h += 4) {
			r->pci_addr = h;
			*a++ = r->pci_io_data;
		}
	}
}

void
dino_rrr_8(void *v, bus_space_handle_t h, bus_size_t o,
    u_int64_t *a, bus_size_t c)
{
	panic("dino_rrr_8: not implemented");
}

void
dino_wrr_2(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int16_t *a, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		while (c--)
			*p++ = *a++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int16_t *)&r->pci_io_data;
			if (h & 2)
				p++;
			*p = *a++;
			h += 2;
			if (!(h & 2))
				r->pci_addr = h;
		}
	}
}

void
dino_wrr_4(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int32_t *a, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int32_t *)h;
		while (c--)
			*p++ = *a++;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		for (; c--; h += 4) {
			r->pci_addr = h;
			r->pci_io_data = *a++;
		}
	}
}

void
dino_wrr_8(void *v, bus_space_handle_t h, bus_size_t o,
    const u_int64_t *a, bus_size_t c)
{
	panic("dino_wrr_8: not implemented");
}

void
dino_sr_1(void *v, bus_space_handle_t h, bus_size_t o, u_int8_t vv, bus_size_t c)
{
	volatile u_int8_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int8_t *)h;
		while (c--)
			*p++ = vv;
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int8_t *)&r->pci_io_data + (h & 3);
			*p = vv;
			if (!(++h & 3))
				r->pci_addr = h;
		}
	}
}

void
dino_sr_2(void *v, bus_space_handle_t h, bus_size_t o, u_int16_t vv, bus_size_t c)
{
	volatile u_int16_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int16_t *)h;
		while (c--)
			*p++ = htole16(vv);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		r->pci_addr = h & ~3;
		while (c--) {
			p = (volatile u_int16_t *)&r->pci_io_data;
			if (h & 2)
				p++;
			*p = htole16(vv);
			h += 2;
			if (!(h & 2))
				r->pci_addr = h;
		}
	}
}

void
dino_sr_4(void *v, bus_space_handle_t h, bus_size_t o, u_int32_t vv, bus_size_t c)
{
	volatile u_int32_t *p;

	h += o;
	if (h & 0xf0000000) {
		p = (volatile u_int32_t *)h;
		while (c--)
			*p++ = htole32(vv);
	} else {
		struct dino_softc *sc = v;
		volatile struct dino_regs *r = sc->sc_regs;

		for (; c--; h += 4) {
			r->pci_addr = h;
			r->pci_io_data = htole32(vv);
		}
	}
}

void
dino_sr_8(void *v, bus_space_handle_t h, bus_size_t o, u_int64_t vv, bus_size_t c)
{
	panic("dino_sr_8: not implemented");
}

void
dino_cp_1(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	while (c--)
		dino_w1(v, h1, o1++, dino_r1(v, h2, o2++));
}

void
dino_cp_2(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	while (c--) {
		dino_w2(v, h1, o1, dino_r2(v, h2, o2));
		o1 += 2;
		o2 += 2;
	}
}

void
dino_cp_4(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	while (c--) {
		dino_w4(v, h1, o1, dino_r4(v, h2, o2));
		o1 += 4;
		o2 += 4;
	}
}

void
dino_cp_8(void *v, bus_space_handle_t h1, bus_size_t o1,
	  bus_space_handle_t h2, bus_size_t o2, bus_size_t c)
{
	while (c--) {
		dino_w8(v, h1, o1, dino_r8(v, h2, o2));
		o1 += 8;
		o2 += 8;
	}
}


const struct hppa_bus_space_tag dino_iomemt = {
	NULL,

	NULL, dino_unmap, dino_subregion, NULL, dino_free,
	dino_barrier, dino_vaddr,
	dino_r1,    dino_r2,    dino_r4,    dino_r8,
	dino_w1,    dino_w2,    dino_w4,    dino_w8,
	dino_rm_1,  dino_rm_2,  dino_rm_4,  dino_rm_8,
	dino_wm_1,  dino_wm_2,  dino_wm_4,  dino_wm_8,
	dino_sm_1,  dino_sm_2,  dino_sm_4,  dino_sm_8,
	            dino_rrm_2, dino_rrm_4, dino_rrm_8,
	            dino_wrm_2, dino_wrm_4, dino_wrm_8,
	dino_rr_1,  dino_rr_2,  dino_rr_4,  dino_rr_8,
	dino_wr_1,  dino_wr_2,  dino_wr_4,  dino_wr_8,
	            dino_rrr_2, dino_rrr_4, dino_rrr_8,
	            dino_wrr_2, dino_wrr_4, dino_wrr_8,
	dino_sr_1,  dino_sr_2,  dino_sr_4,  dino_sr_8,
	dino_cp_1,  dino_cp_2,  dino_cp_4,  dino_cp_8
};

int
dino_dmamap_create(void *v, bus_size_t size, int nsegments,
    bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{
	struct dino_softc *sc = v;

	/* TODO check the addresses, boundary, enable dma */

	return bus_dmamap_create(sc->sc_dmat, size, nsegments,
	    maxsegsz, boundary, flags, dmamp);
}

void
dino_dmamap_destroy(void *v, bus_dmamap_t map)
{
	struct dino_softc *sc = v;

	bus_dmamap_destroy(sc->sc_dmat, map);
}

int
dino_dmamap_load(void *v, bus_dmamap_t map, void *addr, bus_size_t size,
    struct proc *p, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamap_load(sc->sc_dmat, map, addr, size, p, flags);
}

int
dino_dmamap_load_mbuf(void *v, bus_dmamap_t map, struct mbuf *m, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamap_load_mbuf(sc->sc_dmat, map, m, flags);
}

int
dino_dmamap_load_uio(void *v, bus_dmamap_t map, struct uio *uio, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamap_load_uio(sc->sc_dmat, map, uio, flags);
}

int
dino_dmamap_load_raw(void *v, bus_dmamap_t map, bus_dma_segment_t *segs,
    int nsegs, bus_size_t size, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamap_load_raw(sc->sc_dmat, map, segs, nsegs, size, flags);
}

void
dino_dmamap_unload(void *v, bus_dmamap_t map)
{
	struct dino_softc *sc = v;

	bus_dmamap_unload(sc->sc_dmat, map);
}

void
dino_dmamap_sync(void *v, bus_dmamap_t map, bus_addr_t off,
    bus_size_t len, int ops)
{
	struct dino_softc *sc = v;

	return bus_dmamap_sync(sc->sc_dmat, map, off, len, ops);
}

int
dino_dmamem_alloc(void *v, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs,
    int nsegs, int *rsegs, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamem_alloc(sc->sc_dmat, size, alignment, boundary,
	    segs, nsegs, rsegs, flags);
}

void
dino_dmamem_free(void *v, bus_dma_segment_t *segs, int nsegs)
{
	struct dino_softc *sc = v;

	bus_dmamem_free(sc->sc_dmat, segs, nsegs);
}

int
dino_dmamem_map(void *v, bus_dma_segment_t *segs, int nsegs, size_t size,
    void **kvap, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamem_map(sc->sc_dmat, segs, nsegs, size, kvap, flags);
}

void
dino_dmamem_unmap(void *v, void *kva, size_t size)
{
	struct dino_softc *sc = v;

	bus_dmamem_unmap(sc->sc_dmat, kva, size);
}

paddr_t
dino_dmamem_mmap(void *v, bus_dma_segment_t *segs, int nsegs, off_t off,
    int prot, int flags)
{
	struct dino_softc *sc = v;

	return bus_dmamem_mmap(sc->sc_dmat, segs, nsegs, off, prot, flags);
}

const struct hppa_bus_dma_tag dino_dmat = {
	NULL,
	dino_dmamap_create, dino_dmamap_destroy,
	dino_dmamap_load, dino_dmamap_load_mbuf,
	dino_dmamap_load_uio, dino_dmamap_load_raw,
	dino_dmamap_unload, dino_dmamap_sync,

	dino_dmamem_alloc, dino_dmamem_free, dino_dmamem_map,
	dino_dmamem_unmap, dino_dmamem_mmap
};

const struct hppa_pci_chipset_tag dino_pc = {
	NULL,
	dino_attach_hook, dino_maxdevs, dino_make_tag, dino_decompose_tag,
	dino_conf_read, dino_conf_write,
	dino_intr_map, dino_intr_string,
	dino_intr_establish, dino_intr_disestablish,
#if NCARDBUS > 0
	dino_alloc_parent
#else
	NULL
#endif
};

int
dinomatch(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	/* there will be only one */
	if (ca->ca_type.iodc_type != HPPA_TYPE_BRIDGE ||
	    ca->ca_type.iodc_sv_model != HPPA_BRIDGE_DINO)
		return 0;

	/* Make sure we have an IRQ. */
	if (ca->ca_irq == HP700CF_IRQ_UNDEF) {
		ca->ca_irq = hp700_intr_allocate_bit(&int_reg_cpu);
		if (ca->ca_irq == HP700CF_IRQ_UNDEF) {
			aprint_normal("dinomatch: Can't allocate IRQ\n");
			return 0;
		}
	}

	return 1;
}

void
dinoattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct dino_softc *sc = (struct dino_softc *)self;
	struct confargs *ca = (struct confargs *)aux;
	struct pcibus_attach_args pba;
	volatile struct dino_regs *r;
	const char *p;
	u_int data;
	int s, ver;

	sc->sc_bt = ca->ca_iot;
	sc->sc_dmat = ca->ca_dmatag;
	if (bus_space_map(sc->sc_bt, ca->ca_hpa, PAGE_SIZE, 0, &sc->sc_bh)) {
		printf(": can't map space\n");
		return;
	}

	sc->sc_regs = r = (volatile struct dino_regs *)sc->sc_bh;
#ifdef trust_the_firmware_to_proper_initialize_everything
	r->io_addr_en = 0;
	r->io_control = 0x80;
	r->pamr = 0;
	r->papr = 0;
	r->io_fbb_en |= 1;
	r->damode = 0;
	r->gmask &= ~1;	/* allow GSC bus req */
	r->pciror = 0;
	r->pciwor = 0;
	r->brdg_feat = 0xc0000000;
#endif

	snprintf(sc->sc_ioexname, sizeof(sc->sc_ioexname),
	    "%s_io", sc->sc_dv.dv_xname);
	if ((sc->sc_ioex = extent_create(sc->sc_ioexname, 0, 0xffff,
	    M_DEVBUF, NULL, 0, EX_NOWAIT | EX_MALLOCOK)) == NULL) {
		printf(": cannot allocate I/O extent map\n");
		bus_space_unmap(sc->sc_bt, sc->sc_bh, PAGE_SIZE);
		return;
	}

        /* interrupts guts */
	s = splhigh();
	r->icr = 0;
	r->imr = ~0;
	data = r->irr0;
	r->imr = 0;
	r->iar0 = cpu_gethpa(0) | (31 - ca->ca_irq);
	splx(s);
	/* Establish the interrupt register. */
	hp700_intr_reg_establish(&sc->sc_int_reg);
	sc->sc_int_reg.int_reg_mask = &r->imr;
	sc->sc_int_reg.int_reg_req = &r->irr0;
	sc->sc_int_reg.int_reg_level = &r->ilr;
	/* Add the I/O interrupt register. */
	sc->sc_int_reg.int_reg_dev = sc->sc_dv.dv_xname;
	sc->sc_ih = hp700_intr_establish(&sc->sc_dv, IPL_NONE,
	    NULL, &sc->sc_int_reg, &int_reg_cpu, ca->ca_irq);

	/* TODO establish the bus error interrupt */

	r->iodc = 0;
	data = r->iodc;
	ver = (ca->ca_type.iodc_model << 4) |
	    (ca->ca_type.iodc_revision >> 4);
	switch (ver) {
	case 0x05d:	p = "Dino";	/* j2240 */
	case 0x680:	p = "Dino";
		switch (data >> 16) {
		case 0x6800:	ver = 0x20;	break;
		case 0x6801:	ver = 0x21;	break;
		case 0x6802:	ver = 0x30;	break;
		case 0x6803:	ver = 0x31;	break;
		default:	ver = 0x40;	break;
		}
		break;

	case 0x682:	p = "Cujo";
		switch (data >> 16) {
		case 0x6820:	ver = 0x10;	break;
		case 0x6821:	ver = 0x20;	break;
		default:	ver = 0x30;	break;
		}
		break;

	default:	p = "Mojo";
		ver = (data >> 16) & 0xff;
		break;
	}
	sc->sc_ver = ver;
	printf(": %s V%d.%d\n", p, ver >> 4, ver & 0xf);

	sc->sc_iot = dino_iomemt;
	sc->sc_iot.hbt_cookie = sc;
	sc->sc_iot.hbt_map = dino_iomap;
	sc->sc_iot.hbt_alloc = dino_ioalloc;
	sc->sc_memt = dino_iomemt;
	sc->sc_memt.hbt_cookie = sc;
	sc->sc_memt.hbt_map = dino_memmap;
	sc->sc_memt.hbt_alloc = dino_memalloc;
	sc->sc_pc = dino_pc;
	sc->sc_pc._cookie = sc;
	sc->sc_dmatag = dino_dmat;
	sc->sc_dmatag._cookie = sc;

#ifdef no_hardware_to_test_so_leave_it_for_now
	/* scan for ps2 kbd/ms, serial, and flying toasters */
	ca->ca_hpamask = -1;
	pdc_scanbus(self, ca, MAXMODBUS);
#endif

	pba.pba_iot = &sc->sc_iot;
	pba.pba_memt = &sc->sc_memt;
	pba.pba_dmat = &sc->sc_dmatag;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_bus = 0;
	pba.pba_bridgetag = NULL;
	pba.pba_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;
	config_found_ia(self, "pcibus", &pba, pcibusprint);
}
