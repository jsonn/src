/*	$NetBSD: i82365.c,v 1.1.2.14 1997/10/16 19:41:51 thorpej Exp $	*/

#define	PCICDEBUG

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365var.h>

#ifdef PCICDEBUG
int	pcic_debug = 0;
#define	DPRINTF(arg) if (pcic_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

#define	PCIC_VENDOR_UNKNOWN		0
#define	PCIC_VENDOR_I82365SLR0		1
#define	PCIC_VENDOR_I82365SLR1		2
#define	PCIC_VENDOR_CIRRUS_PD6710	3
#define	PCIC_VENDOR_CIRRUS_PD672X	4

/*
 * Individual drivers will allocate their own memory and io regions. Memory
 * regions must be a multiple of 4k, aligned on a 4k boundary.
 */

#define	PCIC_MEM_ALIGN	PCIC_MEM_PAGESIZE

void	pcic_attach_socket __P((struct pcic_handle *));
void	pcic_init_socket __P((struct pcic_handle *));

#ifdef __BROKEN_INDIRECT_CONFIG
int	pcic_submatch __P((struct device *, void *, void *));
#else
int	pcic_submatch __P((struct device *, struct cfdata *, void *));
#endif
int	pcic_print  __P((void *arg, const char *pnp));
int	pcic_intr_socket __P((struct pcic_handle *));

void	pcic_attach_card __P((struct pcic_handle *));
void	pcic_detach_card __P((struct pcic_handle *));

void	pcic_chip_do_mem_map __P((struct pcic_handle *, int));
void	pcic_chip_do_io_map __P((struct pcic_handle *, int));

struct cfdriver pcic_cd = {
	NULL, "pcic", DV_DULL
};

int
pcic_ident_ok(ident)
	int ident;
{
	/* this is very empirical and heuristic */

	if ((ident == 0) || (ident == 0xff) || (ident & PCIC_IDENT_ZERO))
		return (0);

	if ((ident & PCIC_IDENT_IFTYPE_MASK) != PCIC_IDENT_IFTYPE_MEM_AND_IO) {
#ifdef DIAGNOSTIC
		printf("pcic: does not support memory and I/O cards, "
		    "ignored (ident=%0x)\n", ident);
#endif
		return (0);
	}
	return (1);
}

int
pcic_vendor(h)
	struct pcic_handle *h;
{
	int reg;

	/*
	 * the chip_id of the cirrus toggles between 11 and 00 after a write.
	 * weird.
	 */

	pcic_write(h, PCIC_CIRRUS_CHIP_INFO, 0);
	reg = pcic_read(h, -1);

	if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) ==
	    PCIC_CIRRUS_CHIP_INFO_CHIP_ID) {
		reg = pcic_read(h, -1);
		if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) == 0) {
			if (reg & PCIC_CIRRUS_CHIP_INFO_SLOTS)
				return (PCIC_VENDOR_CIRRUS_PD672X);
			else
				return (PCIC_VENDOR_CIRRUS_PD6710);
		}
	}
	/* XXX how do I identify the GD6729? */

	reg = pcic_read(h, PCIC_IDENT);

	if ((reg & PCIC_IDENT_REV_MASK) == PCIC_IDENT_REV_I82365SLR0)
		return (PCIC_VENDOR_I82365SLR0);
	else
		return (PCIC_VENDOR_I82365SLR1);

	return (PCIC_VENDOR_UNKNOWN);
}

char *
pcic_vendor_to_string(vendor)
	int vendor;
{
	switch (vendor) {
	case PCIC_VENDOR_I82365SLR0:
		return ("Intel 82365SL Revision 0");
	case PCIC_VENDOR_I82365SLR1:
		return ("Intel 82365SL Revision 1");
	case PCIC_VENDOR_CIRRUS_PD6710:
		return ("Cirrus PD6710");
	case PCIC_VENDOR_CIRRUS_PD672X:
		return ("Cirrus PD672X");
	}

	return ("Unknown controller");
}

void
pcic_attach(sc)
	struct pcic_softc *sc;
{
	int vendor, count, i, reg;

	/* now check for each controller/socket */

	/*
	 * this could be done with a loop, but it would violate the
	 * abstraction
	 */

	count = 0;

	DPRINTF(("pcic ident regs:"));

	sc->handle[0].sc = sc;
	sc->handle[0].sock = C0SA;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[0], PCIC_IDENT))) {
		sc->handle[0].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[0].flags = 0;
	}

	DPRINTF((" 0x%02x", reg));

	sc->handle[1].sc = sc;
	sc->handle[1].sock = C0SB;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[1], PCIC_IDENT))) {
		sc->handle[1].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[1].flags = 0;
	}

	DPRINTF((" 0x%02x", reg));

	sc->handle[2].sc = sc;
	sc->handle[2].sock = C1SA;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[2], PCIC_IDENT))) {
		sc->handle[2].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[2].flags = 0;
	}

	DPRINTF((" 0x%02x", reg));

	sc->handle[3].sc = sc;
	sc->handle[3].sock = C1SB;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[3], PCIC_IDENT))) {
		sc->handle[3].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[3].flags = 0;
	}

	DPRINTF((" 0x%02x\n", reg));

	if (count == 0)
		panic("pcic_attach: attach found no sockets");

	/* establish the interrupt */

	/* XXX block interrupts? */

	for (i = 0; i < PCIC_NSLOTS; i++) {
#if 0
		/*
		 * this should work, but w/o it, setting tty flags hangs at
		 * boot time.
		 */
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
#endif
		{
			pcic_write(&sc->handle[i], PCIC_CSC_INTR, 0);
			pcic_read(&sc->handle[i], PCIC_CSC);
		}
	}

	if ((sc->handle[0].flags & PCIC_FLAG_SOCKETP) ||
	    (sc->handle[1].flags & PCIC_FLAG_SOCKETP)) {
		vendor = pcic_vendor(&sc->handle[0]);

		printf("%s: controller 0 (%s) has ", sc->dev.dv_xname,
		       pcic_vendor_to_string(vendor));

		if ((sc->handle[0].flags & PCIC_FLAG_SOCKETP) &&
		    (sc->handle[1].flags & PCIC_FLAG_SOCKETP))
			printf("sockets A and B\n");
		else if (sc->handle[0].flags & PCIC_FLAG_SOCKETP)
			printf("socket A only\n");
		else
			printf("socket B only\n");

		if (sc->handle[0].flags & PCIC_FLAG_SOCKETP)
			sc->handle[0].vendor = vendor;
		if (sc->handle[1].flags & PCIC_FLAG_SOCKETP)
			sc->handle[1].vendor = vendor;
	}
	if ((sc->handle[2].flags & PCIC_FLAG_SOCKETP) ||
	    (sc->handle[3].flags & PCIC_FLAG_SOCKETP)) {
		vendor = pcic_vendor(&sc->handle[2]);

		printf("%s: controller 1 (%s) has ", sc->dev.dv_xname,
		       pcic_vendor_to_string(vendor));

		if ((sc->handle[2].flags & PCIC_FLAG_SOCKETP) &&
		    (sc->handle[3].flags & PCIC_FLAG_SOCKETP))
			printf("sockets A and B\n");
		else if (sc->handle[2].flags & PCIC_FLAG_SOCKETP)
			printf("socket A only\n");
		else
			printf("socket B only\n");

		if (sc->handle[2].flags & PCIC_FLAG_SOCKETP)
			sc->handle[2].vendor = vendor;
		if (sc->handle[3].flags & PCIC_FLAG_SOCKETP)
			sc->handle[3].vendor = vendor;
	}
}

void
pcic_attach_sockets(sc)
	struct pcic_softc *sc;
{
	int i;

	for (i = 0; i < PCIC_NSLOTS; i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			pcic_attach_socket(&sc->handle[i]);
}

void
pcic_attach_socket(h)
	struct pcic_handle *h;
{
	struct pcmciabus_attach_args paa;

	/* initialize the rest of the handle */

	h->memalloc = 0;
	h->ioalloc = 0;
	h->ih_irq = 0;

	/* now, config one pcmcia device per socket */

	paa.pct = (pcmcia_chipset_tag_t) h->sc->pct;
	paa.pch = (pcmcia_chipset_handle_t) h;
	paa.iobase = h->sc->iobase;
	paa.iosize = h->sc->iosize;

	h->pcmcia = config_found_sm(&h->sc->dev, &paa, pcic_print,
	    pcic_submatch);

	/* if there's actually a pcmcia device attached, initialize the slot */

	if (h->pcmcia)
		pcic_init_socket(h);
}

void
pcic_init_socket(h)
	struct pcic_handle *h;
{
	int reg;

	/* set up the card to interrupt on card detect */

	pcic_write(h, PCIC_CSC_INTR, (h->sc->irq << PCIC_CSC_INTR_IRQ_SHIFT) |
	    PCIC_CSC_INTR_CD_ENABLE);
	pcic_write(h, PCIC_INTR, 0);
	pcic_read(h, PCIC_CSC);

	/* unsleep the cirrus controller */

	if ((h->vendor == PCIC_VENDOR_CIRRUS_PD6710) ||
	    (h->vendor == PCIC_VENDOR_CIRRUS_PD672X)) {
		reg = pcic_read(h, PCIC_CIRRUS_MISC_CTL_2);
		if (reg & PCIC_CIRRUS_MISC_CTL_2_SUSPEND) {
			DPRINTF(("%s: socket %02x was suspended\n",
			    h->sc->dev.dv_xname, h->sock));
			reg &= ~PCIC_CIRRUS_MISC_CTL_2_SUSPEND;
			pcic_write(h, PCIC_CIRRUS_MISC_CTL_2, reg);
		}
	}
	/* if there's a card there, then attach it. */

	reg = pcic_read(h, PCIC_IF_STATUS);

	if ((reg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
	    PCIC_IF_STATUS_CARDDETECT_PRESENT)
		pcic_attach_card(h);
}

int
#ifdef __BROKEN_INDIRECT_CONFIG
pcic_submatch(parent, match, aux)
#else
pcic_submatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = match;
#endif

	struct pcmciabus_attach_args *paa =
	    (struct pcmciabus_attach_args *) aux;
	struct pcic_handle *h = (struct pcic_handle *) paa->pch;

	switch (h->sock) {
	case C0SA:
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != 0)
			return 0;
		if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != 0)
			return 0;

		break;
	case C0SB:
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != 0)
			return 0;
		if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != 1)
			return 0;

		break;
	case C1SA:
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != 1)
			return 0;
		if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != 0)
			return 0;

		break;
	case C1SB:
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != 1)
			return 0;
		if (cf->cf_loc[1] != -1 && cf->cf_loc[1] != 1)
			return 0;

		break;
	default:
		panic("unknown pcic socket");
	}

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
pcic_print(arg, pnp)
	void *arg;
	const char *pnp;
{
	struct pcmciabus_attach_args *paa =
	    (struct pcmciabus_attach_args *) arg;
	struct pcic_handle *h = (struct pcic_handle *) paa->pch;

	/* Only "pcmcia"s can attach to "pcic"s... easy. */
	if (pnp)
		printf("pcmcia at %s", pnp);

	switch (h->sock) {
	case C0SA:
		printf(" controller 0 socket 0");
		break;
	case C0SB:
		printf(" controller 0 socket 1");
		break;
	case C1SA:
		printf(" controller 1 socket 0");
		break;
	case C1SB:
		printf(" controller 1 socket 1");
		break;
	default:
		panic("unknown pcic socket");
	}

	return (UNCONF);
}

int
pcic_intr(arg)
	void *arg;
{
	struct pcic_softc *sc = (struct pcic_softc *) arg;
	int i, ret = 0;

	DPRINTF(("%s: intr\n", sc->dev.dv_xname));

	for (i = 0; i < PCIC_NSLOTS; i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			ret += pcic_intr_socket(&sc->handle[i]);

	return (ret ? 1 : 0);
}

int
pcic_intr_socket(h)
	struct pcic_handle *h;
{
	int cscreg;

	cscreg = pcic_read(h, PCIC_CSC);

	cscreg &= (PCIC_CSC_GPI |
		   PCIC_CSC_CD |
		   PCIC_CSC_READY |
		   PCIC_CSC_BATTWARN |
		   PCIC_CSC_BATTDEAD);

	if (cscreg & PCIC_CSC_GPI) {
		DPRINTF(("%s: %02x GPI\n", h->sc->dev.dv_xname, h->sock));
	}
	if (cscreg & PCIC_CSC_CD) {
		int statreg;

		statreg = pcic_read(h, PCIC_IF_STATUS);

		DPRINTF(("%s: %02x CD %x\n", h->sc->dev.dv_xname, h->sock,
		    statreg));

		/*
		 * XXX This should probably schedule something to happen
		 * after the interrupt handler completes
		 */

		if ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
		    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
			if (!(h->flags & PCIC_FLAG_CARDP))
				pcic_attach_card(h);
		} else {
			if (h->flags & PCIC_FLAG_CARDP)
				pcic_detach_card(h);
		}
	}
	if (cscreg & PCIC_CSC_READY) {
		DPRINTF(("%s: %02x READY\n", h->sc->dev.dv_xname, h->sock));
		/* shouldn't happen */
	}
	if (cscreg & PCIC_CSC_BATTWARN) {
		DPRINTF(("%s: %02x BATTWARN\n", h->sc->dev.dv_xname, h->sock));
	}
	if (cscreg & PCIC_CSC_BATTDEAD) {
		DPRINTF(("%s: %02x BATTDEAD\n", h->sc->dev.dv_xname, h->sock));
	}
	return (cscreg ? 1 : 0);
}

void
pcic_attach_card(h)
	struct pcic_handle *h;
{
	if (h->flags & PCIC_FLAG_CARDP)
		panic("pcic_attach_card: already attached");

	/* call the MI attach function */

	pcmcia_card_attach(h->pcmcia);

	h->flags |= PCIC_FLAG_CARDP;
}

void
pcic_detach_card(h)
	struct pcic_handle *h;
{
	if (!(h->flags & PCIC_FLAG_CARDP))
		panic("pcic_attach_card: already detached");

	h->flags &= ~PCIC_FLAG_CARDP;

	/* call the MI attach function */

	pcmcia_card_detach(h->pcmcia);

	/* disable card detect resume and configuration reset */

	/* power down the socket */

	pcic_write(h, PCIC_PWRCTL, 0);

	/* reset the card */

	pcic_write(h, PCIC_INTR, 0);
}

int 
pcic_chip_mem_alloc(pch, size, pcmhp)
	pcmcia_chipset_handle_t pch;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_handle_t memh;
	bus_addr_t addr;
	bus_size_t sizepg;
	int i, mask, mhandle;

	/* out of sc->memh, allocate as many pages as necessary */

	/* convert size to PCIC pages */
	sizepg = (size + (PCIC_MEM_ALIGN - 1)) / PCIC_MEM_ALIGN;

	mask = (1 << sizepg) - 1;

	addr = 0;		/* XXX gcc -Wuninitialized */
	mhandle = 0;		/* XXX gcc -Wuninitialized */

	for (i = 0; i < (PCIC_MEM_PAGES + 1 - sizepg); i++) {
		if ((h->sc->subregionmask & (mask << i)) == (mask << i)) {
			if (bus_space_subregion(h->sc->memt, h->sc->memh,
			    i * PCIC_MEM_PAGESIZE,
			    sizepg * PCIC_MEM_PAGESIZE, &memh))
				return (1);
			mhandle = mask << i;
			addr = h->sc->membase + (i * PCIC_MEM_PAGESIZE);
			h->sc->subregionmask &= ~(mhandle);
			break;
		}
	}

	if (i == (PCIC_MEM_PAGES + 1 - size))
		return (1);

	DPRINTF(("pcic_chip_mem_alloc bus addr 0x%lx+0x%lx\n", (u_long) addr,
		 (u_long) size));

	pcmhp->memt = h->sc->memt;
	pcmhp->memh = memh;
	pcmhp->addr = addr;
	pcmhp->size = size;
	pcmhp->mhandle = mhandle;
	pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;

	return (0);
}

void 
pcic_chip_mem_free(pch, pcmhp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_mem_handle *pcmhp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;

	h->sc->subregionmask |= pcmhp->mhandle;
}

static struct mem_map_index_st {
	int	sysmem_start_lsb;
	int	sysmem_start_msb;
	int	sysmem_stop_lsb;
	int	sysmem_stop_msb;
	int	cardmem_lsb;
	int	cardmem_msb;
	int	memenable;
} mem_map_index[] = {
	{
		PCIC_SYSMEM_ADDR0_START_LSB,
		PCIC_SYSMEM_ADDR0_START_MSB,
		PCIC_SYSMEM_ADDR0_STOP_LSB,
		PCIC_SYSMEM_ADDR0_STOP_MSB,
		PCIC_CARDMEM_ADDR0_LSB,
		PCIC_CARDMEM_ADDR0_MSB,
		PCIC_ADDRWIN_ENABLE_MEM0,
	},
	{
		PCIC_SYSMEM_ADDR1_START_LSB,
		PCIC_SYSMEM_ADDR1_START_MSB,
		PCIC_SYSMEM_ADDR1_STOP_LSB,
		PCIC_SYSMEM_ADDR1_STOP_MSB,
		PCIC_CARDMEM_ADDR1_LSB,
		PCIC_CARDMEM_ADDR1_MSB,
		PCIC_ADDRWIN_ENABLE_MEM1,
	},
	{
		PCIC_SYSMEM_ADDR2_START_LSB,
		PCIC_SYSMEM_ADDR2_START_MSB,
		PCIC_SYSMEM_ADDR2_STOP_LSB,
		PCIC_SYSMEM_ADDR2_STOP_MSB,
		PCIC_CARDMEM_ADDR2_LSB,
		PCIC_CARDMEM_ADDR2_MSB,
		PCIC_ADDRWIN_ENABLE_MEM2,
	},
	{
		PCIC_SYSMEM_ADDR3_START_LSB,
		PCIC_SYSMEM_ADDR3_START_MSB,
		PCIC_SYSMEM_ADDR3_STOP_LSB,
		PCIC_SYSMEM_ADDR3_STOP_MSB,
		PCIC_CARDMEM_ADDR3_LSB,
		PCIC_CARDMEM_ADDR3_MSB,
		PCIC_ADDRWIN_ENABLE_MEM3,
	},
	{
		PCIC_SYSMEM_ADDR4_START_LSB,
		PCIC_SYSMEM_ADDR4_START_MSB,
		PCIC_SYSMEM_ADDR4_STOP_LSB,
		PCIC_SYSMEM_ADDR4_STOP_MSB,
		PCIC_CARDMEM_ADDR4_LSB,
		PCIC_CARDMEM_ADDR4_MSB,
		PCIC_ADDRWIN_ENABLE_MEM4,
	},
};

void 
pcic_chip_do_mem_map(h, win)
	struct pcic_handle *h;
	int win;
{
	int reg;

	pcic_write(h, mem_map_index[win].sysmem_start_lsb,
	    (h->mem[win].addr >> PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_start_msb,
	    ((h->mem[win].addr >> (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_START_MSB_ADDR_MASK));

#if 0
	/* XXX do I want 16 bit all the time? */
	PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT;
#endif

	pcic_write(h, mem_map_index[win].sysmem_stop_lsb,
	    ((h->mem[win].addr + h->mem[win].size) >>
	    PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_stop_msb,
	    (((h->mem[win].addr + h->mem[win].size) >>
	    (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_STOP_MSB_ADDR_MASK) |
	    PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2);

	pcic_write(h, mem_map_index[win].cardmem_lsb,
	    (h->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].cardmem_msb,
	    ((h->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8)) &
	    PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK) |
	    ((h->mem[win].kind == PCMCIA_MEM_ATTR) ?
	    PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0));

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= (mem_map_index[win].memenable | PCIC_ADDRWIN_ENABLE_MEMCS16);
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

#ifdef PCICDEBUG
	{
		int r1, r2, r3, r4, r5, r6;

		r1 = pcic_read(h, mem_map_index[win].sysmem_start_msb);
		r2 = pcic_read(h, mem_map_index[win].sysmem_start_lsb);
		r3 = pcic_read(h, mem_map_index[win].sysmem_stop_msb);
		r4 = pcic_read(h, mem_map_index[win].sysmem_stop_lsb);
		r5 = pcic_read(h, mem_map_index[win].cardmem_msb);
		r6 = pcic_read(h, mem_map_index[win].cardmem_lsb);

		DPRINTF(("pcic_chip_do_mem_map window %d: %02x%02x %02x%02x "
		    "%02x%02x\n", win, r1, r2, r3, r4, r5, r6));
	}
#endif
}

int 
pcic_chip_mem_map(pch, kind, card_addr, size, pcmhp, offsetp, windowp)
	pcmcia_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pcmcia_mem_handle *pcmhp;
	bus_addr_t *offsetp;
	int *windowp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_addr_t busaddr;
	long card_offset;
	int i, win;

	win = -1;
	for (i = 0; i < (sizeof(mem_map_index) / sizeof(mem_map_index[0]));
	    i++) {
		if ((h->memalloc & (1 << i)) == 0) {
			win = i;
			h->memalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return (1);

	*windowp = win;

	/* XXX this is pretty gross */

	if (h->sc->memt != pcmhp->memt)
		panic("pcic_chip_mem_map memt is bogus");

	busaddr = pcmhp->addr;

	/*
	 * compute the address offset to the pcmcia address space for the
	 * pcic.  this is intentionally signed.  The masks and shifts below
	 * will cause TRT to happen in the pcic registers.  Deal with making
	 * sure the address is aligned, and return the alignment offset.
	 */

	*offsetp = card_addr % PCIC_MEM_ALIGN;
	card_addr -= *offsetp;

	DPRINTF(("pcic_chip_mem_map window %d bus %lx+%lx+%lx at card addr "
	    "%lx\n", win, (u_long) busaddr, (u_long) * offsetp, (u_long) size,
	    (u_long) card_addr));

	/*
	 * include the offset in the size, and decrement size by one, since
	 * the hw wants start/stop
	 */
	size += *offsetp - 1;

	card_offset = (((long) card_addr) - ((long) busaddr));

	h->mem[win].addr = busaddr;
	h->mem[win].size = size;
	h->mem[win].offset = card_offset;
	h->mem[win].kind = kind;

	pcic_chip_do_mem_map(h, win);

	return (0);
}

void 
pcic_chip_mem_unmap(pch, window)
	pcmcia_chipset_handle_t pch;
	int window;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(mem_map_index) / sizeof(mem_map_index[0])))
		panic("pcic_chip_mem_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~mem_map_index[window].memenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->memalloc &= ~(1 << window);
}

int 
pcic_chip_io_alloc(pch, start, size, align, pcihp)
	pcmcia_chipset_handle_t pch;
	bus_addr_t start;
	bus_size_t size;
	bus_size_t align;
	struct pcmcia_io_handle *pcihp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t ioaddr;
	int flags = 0;

	/*
	 * Allocate some arbitrary I/O space.
	 */

	iot = h->sc->iot;

	if (start) {
		ioaddr = start;
		if (bus_space_map(iot, start, size, 0, &ioh))
			return (1);
		DPRINTF(("pcic_chip_io_alloc map port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	} else {
		flags |= PCMCIA_IO_ALLOCATED;
		if (bus_space_alloc(iot, h->sc->iobase,
		    h->sc->iobase + h->sc->iosize, size, align, 0, 0,
		    &ioaddr, &ioh))
			return (1);
		DPRINTF(("pcic_chip_io_alloc alloc port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	}

	pcihp->iot = iot;
	pcihp->ioh = ioh;
	pcihp->addr = ioaddr;
	pcihp->size = size;
	pcihp->flags = flags;

	return (0);
}

void 
pcic_chip_io_free(pch, pcihp)
	pcmcia_chipset_handle_t pch;
	struct pcmcia_io_handle *pcihp;
{
	bus_space_tag_t iot = pcihp->iot;
	bus_space_handle_t ioh = pcihp->ioh;
	bus_size_t size = pcihp->size;

	if (pcihp->flags & PCMCIA_IO_ALLOCATED)
		bus_space_free(iot, ioh, size);
	else
		bus_space_unmap(iot, ioh, size);
}


static struct io_map_index_st {
	int	start_lsb;
	int	start_msb;
	int	stop_lsb;
	int	stop_msb;
	int	ioenable;
	int	ioctlmask;
	int	ioctlbits[3];		/* indexed by PCMCIA_WIDTH_* */
}               io_map_index[] = {
	{
		PCIC_IOADDR0_START_LSB,
		PCIC_IOADDR0_START_MSB,
		PCIC_IOADDR0_STOP_LSB,
		PCIC_IOADDR0_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO0,
		PCIC_IOCTL_IO0_WAITSTATE | PCIC_IOCTL_IO0_ZEROWAIT |
		PCIC_IOCTL_IO0_IOCS16SRC_MASK | PCIC_IOCTL_IO0_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO0_IOCS16SRC_CARD,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE
			    | PCIC_IOCTL_IO0_DATASIZE_8BIT,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE
			    | PCIC_IOCTL_IO0_DATASIZE_16BIT,
		},
	},
	{
		PCIC_IOADDR1_START_LSB,
		PCIC_IOADDR1_START_MSB,
		PCIC_IOADDR1_STOP_LSB,
		PCIC_IOADDR1_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO1,
		PCIC_IOCTL_IO1_WAITSTATE | PCIC_IOCTL_IO1_ZEROWAIT |
		PCIC_IOCTL_IO1_IOCS16SRC_MASK | PCIC_IOCTL_IO1_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO1_IOCS16SRC_CARD,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_8BIT,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_16BIT,
		},
	},
};

void 
pcic_chip_do_io_map(h, win)
	struct pcic_handle *h;
	int win;
{
	int reg;

	DPRINTF(("pcic_chip_do_io_map win %d addr %lx size %lx width %d\n",
	    win, (long) h->io[win].addr, (long) h->io[win].size,
	    h->io[win].width * 8));

	pcic_write(h, io_map_index[win].start_lsb, h->io[win].addr & 0xff);
	pcic_write(h, io_map_index[win].start_msb,
	    (h->io[win].addr >> 8) & 0xff);

	pcic_write(h, io_map_index[win].stop_lsb,
	    (h->io[win].addr + h->io[win].size - 1) & 0xff);
	pcic_write(h, io_map_index[win].stop_msb,
	    ((h->io[win].addr + h->io[win].size - 1) >> 8) & 0xff);

	reg = pcic_read(h, PCIC_IOCTL);
	reg &= ~io_map_index[win].ioctlmask;
	reg |= io_map_index[win].ioctlbits[h->io[win].width];
	pcic_write(h, PCIC_IOCTL, reg);

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= io_map_index[win].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);
}

int 
pcic_chip_io_map(pch, width, offset, size, pcihp, windowp)
	pcmcia_chipset_handle_t pch;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pcmcia_io_handle *pcihp;
	int *windowp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_addr_t ioaddr = pcihp->addr + offset;
	static char *width_names[] = { "auto", "io8", "io16" };
	int i, win;

	/* XXX Sanity check offset/size. */

	win = -1;
	for (i = 0; i < (sizeof(io_map_index) / sizeof(io_map_index[0])); i++) {
		if ((h->ioalloc & (1 << i)) == 0) {
			win = i;
			h->ioalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return (1);

	*windowp = win;

	/* XXX this is pretty gross */

	if (h->sc->iot != pcihp->iot)
		panic("pcic_chip_io_map iot is bogus");

	DPRINTF(("pcic_chip_io_map window %d %s port %lx+%lx\n",
		 win, width_names[width], (u_long) ioaddr, (u_long) size));

	/* XXX wtf is this doing here? */

	printf(" port 0x%lx", (u_long) ioaddr);
	if (size > 1)
		printf("-0x%lx", (u_long) ioaddr + (u_long) size - 1);

	h->io[win].addr = ioaddr;
	h->io[win].size = size;
	h->io[win].width = width;

	pcic_chip_do_io_map(h, win);

	return (0);
}

void 
pcic_chip_io_unmap(pch, window)
	pcmcia_chipset_handle_t pch;
	int window;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(io_map_index) / sizeof(io_map_index[0])))
		panic("pcic_chip_io_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~io_map_index[window].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->ioalloc &= ~(1 << window);
}

void
pcic_chip_socket_enable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int cardtype, reg, win;

	/* this bit is mostly stolen from pcic_attach_card */

	/* power down the socket to reset it, clear the card reset pin */

	pcic_write(h, PCIC_PWRCTL, 0);

	/* power up the socket */

	pcic_write(h, PCIC_PWRCTL, PCIC_PWRCTL_PWR_ENABLE);
	delay(10000);
	pcic_write(h, PCIC_PWRCTL, PCIC_PWRCTL_PWR_ENABLE | PCIC_PWRCTL_OE);

	/* clear the reset flag */

	pcic_write(h, PCIC_INTR, PCIC_INTR_RESET);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */

	delay(20000);

	/* wait for the chip to finish initializing */

	pcic_wait_ready(h);

	/* zero out the address windows */

	pcic_write(h, PCIC_ADDRWIN_ENABLE, 0);

	/* set the card type */

	cardtype = pcmcia_card_gettype(h->pcmcia);

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~PCIC_INTR_CARDTYPE_MASK;
	reg |= ((cardtype == PCMCIA_IFTYPE_IO) ?
		PCIC_INTR_CARDTYPE_IO :
		PCIC_INTR_CARDTYPE_MEM);
	reg |= h->ih_irq;
	pcic_write(h, PCIC_INTR, reg);

	DPRINTF(("%s: pcic_chip_socket_enable %02x cardtype %s %02x\n",
	    h->sc->dev.dv_xname, h->sock,
	    ((cardtype == PCMCIA_IFTYPE_IO) ? "io" : "mem"), reg));

	/* reinstall all the memory and io mappings */

	for (win = 0; win < PCIC_MEM_WINS; win++)
		if (h->memalloc & (1 << win))
			pcic_chip_do_mem_map(h, win);

	for (win = 0; win < PCIC_IO_WINS; win++)
		if (h->ioalloc & (1 << win))
			pcic_chip_do_io_map(h, win);
}

void
pcic_chip_socket_disable(pch)
	pcmcia_chipset_handle_t pch;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;

	DPRINTF(("pcic_chip_socket_disable\n"));

	/* power down the socket */

	pcic_write(h, PCIC_PWRCTL, 0);
}
