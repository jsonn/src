/*	$NetBSD: superio.c,v 1.13.2.1 2004/08/03 10:34:22 skrll Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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

/*
 * Cayman's "Super IO" device, which looks like an ISA bus once
 * we're finished with it.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: superio.c,v 1.13.2.1 2004/08/03 10:34:22 skrll Exp $");

#include "locators.h"
#include "com.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/endian.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>

#if NCOM > 0
#include <evbsh5/evbsh5/machdep.h>
#include <evbsh5/dev/sysfpgareg.h>
#endif

#include <evbsh5/dev/sysfpgavar.h>
#include <evbsh5/dev/superioreg.h>
#include <evbsh5/dev/superiovar.h>

struct superio_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_space_handle_t sc_bush;
	struct extent *sc_isaext;
};

static int superiomatch(struct device *, struct cfdata *, void *);
static void superioattach(struct device *, struct device *, void *);
static int superioprint(void *, const char *);

CFATTACH_DECL(superio, sizeof(struct superio_softc),
    superiomatch, superioattach, NULL, NULL);
extern struct cfdriver superio_cd;


#define	superio_reg_read(s,r)		\
	    ((u_int8_t)bus_space_read_4((s)->sc_bust,(s)->sc_bush,(r)))
#define	superio_reg_write(s,r,v)	\
	    bus_space_write_4((s)->sc_bust,(s)->sc_bush,(r),(u_int32_t)(v)&0xff)


static int superio_bs_map(void *, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);
static void superio_bs_unmap(void *, bus_space_handle_t, bus_size_t);
static int superio_bs_alloc(void *, bus_addr_t, bus_addr_t, bus_size_t,
	    bus_size_t, bus_size_t, int, bus_addr_t *, bus_space_handle_t *);
static void superio_bs_free(void *, bus_space_handle_t, bus_size_t);
static u_int8_t superio_bs_read_1(void *, bus_space_handle_t, bus_size_t);
static u_int16_t superio_bs_read_2(void *, bus_space_handle_t, bus_size_t);
static void superio_bs_write_1(void *, bus_space_handle_t,
	    bus_size_t, u_int8_t);
static void superio_bs_write_2(void *, bus_space_handle_t,
	    bus_size_t, u_int16_t);
static u_int16_t superio_bs_read_stream_2(void *, bus_space_handle_t,
	    bus_size_t);
static void superio_bs_write_stream_2(void *, bus_space_handle_t,
	    bus_size_t, u_int16_t);

static struct sh5_bus_space_tag superio_bus_space_tag = {
	NULL,
	superio_bs_map,
	superio_bs_unmap,
	superio_bs_alloc,
	superio_bs_free,
	NULL,
	NULL,
	superio_bs_read_1,
	superio_bs_read_2,
	NULL,
	NULL,
	superio_bs_write_1,
	superio_bs_write_2,
	NULL,
	NULL,
	superio_bs_read_stream_2,
	NULL,
	NULL,
	superio_bs_write_stream_2,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static int superio_isa_irq_to_inum(int, int *);
static void superio_cfgmode_enable(struct superio_softc *);
static void superio_cfgmode_disable(struct superio_softc *);
static u_int8_t superio_cfgreg_read(struct superio_softc *, int);
static void superio_cfgreg_write(struct superio_softc *, int, u_int8_t);

static struct superio_softc *superio_sc;

struct superio_devs {
	u_int8_t	sd_irq0;
	u_int8_t	sd_irq1;
	u_int16_t	sd_bar0;
	u_int16_t	sd_bar1;
};

#define	SUPERIO_BAR_COM0	0x3f8
#define	SUPERIO_BAR_COM1	0x2f8

static struct superio_devs superio_devs[] = {
	/* Logical Device 0: FDD */
	{0, 0, 0, 0},

	/* Logical Device 1: IDE1 */
	{14, 0, 0x1f0, 0x3f6},

	/* Logical Device 2: IDE2 */
	{0, 0, 0, 0},

	/* Logical Device 3: LPT */
	{7, 0, 0x378, 0},

	/* Logical Device 4: UART1 */
	{4, 0, SUPERIO_BAR_COM0, 0},

	/* Logical Device 5: UART2 */
	{3, 0, SUPERIO_BAR_COM1, 0},

	/* Logical Device 6: RTC */
	{0, 0, 0, 0},

	/* Logical Device 7: Keyboard */
	{0, 0, 0, 0},

	/* Logical Device 8: Aux I/O */
	{0, 0, 0, 0}
};

#define SUPERIO_NDEVS	(sizeof(superio_devs) / sizeof(struct superio_devs))

/*ARGSUSED*/
static int
superiomatch(struct device *parent, struct cfdata *cf, void *args)
{
	struct sysfpga_attach_args *sa = args;

	if (superio_sc)
		return (0);

	return (strcmp(sa->sa_name, superio_cd.cd_name) == 0);
}

/*ARGSUSED*/
static void
superioattach(struct device *parent, struct device *self, void *args)
{
	struct superio_softc *sc = (struct superio_softc *)self;
	struct sysfpga_attach_args *sa = args;
	struct isabus_attach_args iba;
	int i;

	superio_bus_space_tag.bs_cookie = sc; 

	sc->sc_bust = sa->sa_bust;

	bus_space_map(sc->sc_bust, sa->sa_offset,
	    SUPERIO_REG_SZ, 0, &sc->sc_bush);

	superio_cfgmode_enable(sc);

	printf(": FDC37C935 Super IO Controller, Id 0x%02x, Revision %d\n",
	    superio_cfgreg_read(sc, SUPERIO_GLBL_REG_DEVID),
	    superio_cfgreg_read(sc, SUPERIO_GLBL_REG_DEVREV));

	/*
	 * Power up the relevant devices for Cayman
	 */
	superio_cfgreg_write(sc, SUPERIO_GLBL_REG_POWER_CTRL, 0x3a);

	delay(1000);

	/*
	 * Activate/Deactivate devices as appropriate for Cayman
	 */
	for (i = 0; i < SUPERIO_NDEVS; i++) {
		superio_cfgreg_write(sc, SUPERIO_GLBL_REG_LDN, i);

		if (superio_devs[i].sd_irq0 == 0 &&
		    superio_devs[i].sd_irq1 == 0 &&
		    superio_devs[i].sd_bar0 == 0 &&
		    superio_devs[i].sd_bar1 == 0) {
			superio_cfgreg_write(sc, SUPERIO_DEV_REG_ACTIVATE, 0);
			continue;
		}

		superio_cfgreg_write(sc, SUPERIO_DEV_REG_BAR0_HI,
		    (superio_devs[i].sd_bar0 >> 8) & 0xff);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_BAR0_LO,
		    superio_devs[i].sd_bar0 & 0xff);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_BAR1_HI,
		    (superio_devs[i].sd_bar1 >> 8) & 0xff);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_BAR1_LO,
		    superio_devs[i].sd_bar1 & 0xff);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_INT0_SEL,
		    superio_devs[i].sd_irq0);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_INT1_SEL,
		    superio_devs[i].sd_irq1);
		superio_cfgreg_write(sc, SUPERIO_DEV_REG_ACTIVATE, 1);
	}

	superio_cfgmode_disable(sc);

	sc->sc_isaext = extent_create("isaio", 0, 0xfff, M_DEVBUF, 0, 0, 0);
	if (sc->sc_isaext == NULL) {
		bus_space_unmap(sc->sc_bust, sc->sc_bush, SUPERIO_REG_SZ);
		printf("%s: Failed to create isaio extent\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Attach the isa bus
	 */
	iba.iba_busname = "isa";
	iba.iba_iot = &superio_bus_space_tag;
	iba.iba_memt = NULL;
	iba.iba_dmat = NULL;/* XXX Should be able to do DMA thru dmac */
	iba.iba_ic = (void *)sc;
	config_found(self, &iba, superioprint);
}

static int
superioprint(void *arg, const char *cp)
{
	struct superio_attach_args *saa = arg;

	if (cp)
		aprint_normal("%s at %s", saa->saa_name, cp);

	return (UNCONF);
}

static void
superio_cfgmode_enable(struct superio_softc *sc)
{

	superio_reg_write(sc, SUPERIO_REG_INDEX, 0x55);
	bus_space_barrier(sc->sc_bust, sc->sc_bush,
	    SUPERIO_REG_INDEX, 4, BUS_SPACE_BARRIER_WRITE);

	superio_reg_write(sc, SUPERIO_REG_INDEX, 0x55);
	bus_space_barrier(sc->sc_bust, sc->sc_bush,
	    SUPERIO_REG_INDEX, 4, BUS_SPACE_BARRIER_WRITE);
}

static void
superio_cfgmode_disable(struct superio_softc *sc)
{

	superio_reg_write(sc, SUPERIO_REG_INDEX, 0xaa);
	bus_space_barrier(sc->sc_bust, sc->sc_bush,
	    SUPERIO_REG_INDEX, 4, BUS_SPACE_BARRIER_WRITE);
}

static u_int8_t
superio_cfgreg_read(struct superio_softc *sc, int reg)
{

	superio_reg_write(sc, SUPERIO_REG_INDEX, reg);
	bus_space_barrier(sc->sc_bust, sc->sc_bush, SUPERIO_REG_INDEX, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (superio_reg_read(sc, SUPERIO_REG_DATA));
}

static void
superio_cfgreg_write(struct superio_softc *sc, int reg, u_int8_t val)
{

	superio_reg_write(sc, SUPERIO_REG_INDEX, reg);
	bus_space_barrier(sc->sc_bust, sc->sc_bush, SUPERIO_REG_INDEX, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	superio_reg_write(sc, SUPERIO_REG_DATA, val);
}

/*
 * The SuperIO's data bus is only 8-bits wide, but it is connected to
 * a 32-bit wide data bus in the FEMI area. Software must access the
 * SuperIO using 32-bit operations, where only the lowest 8 bits are
 * valid.
 *
 * This implies that any `address', `size' or `offset' value we get
 * passed here needs to be scaled appropriately.
 */
static int
superio_bs_map(void *arg, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *hp)
{
	struct superio_softc *sc = arg;
	int rv;

	addr *= 4;
	size *= 4;

	if (sc->sc_isaext) {
		rv = extent_alloc_region(sc->sc_isaext, addr, size, EX_NOWAIT);
		if (rv != 0)
			return (rv);
	}

	*hp = (bus_space_handle_t) addr;

	return (0);
}

/*ARGSUSED*/
static void
superio_bs_unmap(void *arg, bus_space_handle_t bh, bus_size_t size)
{
	struct superio_softc *sc = arg;
	bus_addr_t addr = (bus_addr_t)bh;

	if (sc->sc_isaext)
		extent_free(sc->sc_isaext, addr, size * 4, EX_NOWAIT);
}

/*ARGSUSED*/
static int
superio_bs_alloc(void *arg, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t align, bus_size_t boundary, int flags,
    bus_addr_t *addrp, bus_space_handle_t *hp)
{

	return (ENOMEM);
}

/*ARGSUSED*/
static void
superio_bs_free(void *arg, bus_space_handle_t bh, bus_size_t size)
{
}

static u_int8_t
superio_bs_read_1(void *arg, bus_space_handle_t bh, bus_size_t off)
{
	struct superio_softc *sc = arg;

	off = (bus_size_t)bh + (off * 4);

	return ((u_int8_t)bus_space_read_4(sc->sc_bust, sc->sc_bush, off));
}

static u_int16_t
superio_bs_read_2(void *arg, bus_space_handle_t bh, bus_size_t off)
{
	struct superio_softc *sc = arg;
	u_int32_t reg;
	u_int16_t rv;

	off = (bus_size_t)bh + (off * 4);

	reg = bus_space_read_4(sc->sc_bust, sc->sc_bush, off);
	rv = reg & 0xff;
	reg = bus_space_read_4(sc->sc_bust, sc->sc_bush, off + 4);
	rv |= (reg & 0xff) << 8;

	return (rv);
}

static void
superio_bs_write_1(void *arg, bus_space_handle_t bh, bus_size_t off,
    u_int8_t val)
{
	struct superio_softc *sc = arg;

	off = (bus_size_t)bh + (off * 4);

	bus_space_write_4(sc->sc_bust, sc->sc_bush, off, (u_int32_t)val & 0xff);
}

static void
superio_bs_write_2(void *arg, bus_space_handle_t bh, bus_size_t off,
    u_int16_t val)
{
	struct superio_softc *sc = arg;

	off = (bus_size_t)bh + (off * 4);

	bus_space_write_4(sc->sc_bust, sc->sc_bush, off,
	    (u_int32_t)val & 0xff);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, off + 4,
	    (u_int32_t)(val >> 8) & 0xff);
}

static u_int16_t
superio_bs_read_stream_2(void *arg, bus_space_handle_t bh, bus_size_t off)
{
	struct superio_softc *sc = arg;
	u_int32_t reg;
	u_int16_t rv;

	off = (bus_size_t)bh + (off * 4);

	reg = bus_space_read_4(sc->sc_bust, sc->sc_bush, off);
	rv = reg & 0xff;
	reg = bus_space_read_4(sc->sc_bust, sc->sc_bush, off + 4);
	rv = (rv << 8) | (reg & 0xff);

	return (rv);
}

static void
superio_bs_write_stream_2(void *arg, bus_space_handle_t bh, bus_size_t off,
    u_int16_t val)
{
	struct superio_softc *sc = arg;

	off = (bus_size_t)bh + (off * 4);

	bus_space_write_4(sc->sc_bust, sc->sc_bush, off,
	    (u_int32_t)(val >> 8) & 0xff);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, off + 4,
	    (u_int32_t)val & 0xff);
}

static int
superio_isa_irq_to_inum(int irq, int *level)
{
	int inum;

	switch (irq) {
	case 1:
		inum = SYSFPGA_IRL1_INUM_KBD;
		*level = IPL_TTY;
		break;

	case 3:
		inum = SYSFPGA_IRL1_INUM_UART2;
		*level = IPL_SERIAL;
		break;

	case 4:
		inum = SYSFPGA_IRL1_INUM_UART1;
		*level = IPL_SERIAL;
		break;

	case 7:
		inum = SYSFPGA_IRL1_INUM_LPT;
		*level = IPL_TTY;
		break;

	case 12:
		inum = SYSFPGA_IRL1_INUM_MOUSE;
		*level = IPL_TTY;
		break;

	case 14:
		inum = SYSFPGA_IRL1_INUM_IDE;
		*level = IPL_BIO;
		break;

	default:
		inum = -1;
		break;
	}

	return (inum);
}

/*ARGSUSED*/
void
isa_attach_hook(struct device *parent, struct device *self,
    struct isabus_attach_args *iba)
{

	iba->iba_ic = superio_sc;
}

/*ARGSUSED*/
const struct evcnt *
isa_intr_evcnt(isa_chipset_tag_t ic, int irq)
{
	int inum, dummy;

	if ((inum = superio_isa_irq_to_inum(irq, &dummy)) < 0)
		return (NULL);

	return (sysfpga_intr_evcnt(SYSFPGA_IGROUP_IRL1, inum));
}

int
isa_intr_alloc(isa_chipset_tag_t ic, int mask, int type, int *irq)
{

	return (EINVAL);
}

/*ARGSUSED*/
void *
isa_intr_establish(isa_chipset_tag_t ic, int irq, int type, int level,
    int (*handler)(void *), void *arg)
{
	void *ih;
	int inum;
	int shlev;

	if (type != IST_EDGE)
		return (NULL);

	if ((inum = superio_isa_irq_to_inum(irq, &shlev)) < 0)
		return (NULL);

	KDASSERT(level == shlev);

	ih = sysfpga_intr_establish(SYSFPGA_IGROUP_IRL1, shlev, inum,
	    handler, arg);

	return (ih);
}

void
isa_intr_disestablish(isa_chipset_tag_t ic, void *cookie)
{

	sysfpga_intr_disestablish(cookie);
}

/*
 * Dummy ISA DMA routines
 */
/*ARGSUSED*/
void
isa_dmainit(struct isa_dma_state *ids, bus_space_tag_t bt,
    bus_dma_tag_t dt, struct device *dev)
{
}

/*ARGSUSED*/
bus_size_t
isa_dmamaxsize(struct isa_dma_state *ids, int chan)
{

	return (0);
}

/*ARGSUSED*/
int
isa_dmamap_create(struct isa_dma_state *ids, int chan,
    bus_size_t size, int flags)
{

	return (EINVAL);
}

/*ARGSUSED*/
int
isa_dmastart(struct isa_dma_state *ids, int chan, void *addr,
    bus_size_t nbytes, struct proc *p, int flags, int busdmaflags)
{

	return (EINVAL);
}

/*ARGSUSED*/
int
isa_dmadone(struct isa_dma_state *ids, int chan)
{

	return (EINVAL);
}

#if NCOM > 0
/*
 * Helper function for using one of the Super IO's com ports as console
 */
int
superio_console_tag(bus_space_tag_t bt, int port,
    bus_space_tag_t *tp, bus_addr_t *ap)
{
	static struct superio_softc sc;
	bus_addr_t base;
	u_int8_t reg;
	int i;

	if (bus_space_subregion(bt, _evbsh5_bh_sysfpga,
	    SYSFPGA_OFFSET_SUPERIO, SUPERIO_REG_SZ, &sc.sc_bush))
		return (-1);
	sc.sc_bust = bt;

	switch (port) {
	case 0:
		base = SUPERIO_BAR_COM0;
		break;

	case 1:
		base = SUPERIO_BAR_COM1;
		break;

	default:
		return (-1);
	}

	superio_bus_space_tag.bs_cookie = &sc;
	*tp = &superio_bus_space_tag;
	*ap = base;

	/*
	 * Ensure the relevant com port is enabled
	 */
	superio_cfgmode_enable(&sc);

	reg = superio_cfgreg_read(&sc, SUPERIO_GLBL_REG_POWER_CTRL);
	reg |= (1 << (4 + port));
	superio_cfgreg_write(&sc, SUPERIO_GLBL_REG_POWER_CTRL, reg);

	for (i = 0; i < SUPERIO_NDEVS; i++) {
		if (superio_devs[i].sd_bar0 != base)
			continue;

		superio_cfgreg_write(&sc, SUPERIO_GLBL_REG_LDN, i);

		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_BAR0_HI,
		    (superio_devs[i].sd_bar0 >> 8) & 0xff);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_BAR0_LO,
		    superio_devs[i].sd_bar0 & 0xff);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_BAR1_HI,
		    (superio_devs[i].sd_bar1 >> 8) & 0xff);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_BAR1_LO,
		    superio_devs[i].sd_bar1 & 0xff);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_INT0_SEL,
		    superio_devs[i].sd_irq0);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_INT1_SEL,
		    superio_devs[i].sd_irq1);
		superio_cfgreg_write(&sc, SUPERIO_DEV_REG_ACTIVATE, 1);

		break;
	}

	superio_cfgmode_disable(&sc);

	return (0);
}
#endif
