/*	$NetBSD: cardbus.c,v 1.28.2.7 2002/10/18 02:41:33 nathanw Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999 and 2000
 *     HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cardbus.c,v 1.28.2.7 2002/10/18 02:41:33 nathanw Exp $");

#include "opt_cardbus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/reboot.h>		/* for AB_* needed by bootverbose */

#include <machine/bus.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbusdevs.h>

#include <dev/cardbus/cardbus_exrom.h>

#include <dev/pci/pcivar.h>	/* XXX */
#include <dev/pci/pcireg.h>	/* XXX */

#include <dev/pcmcia/pcmciareg.h>

#if defined CARDBUS_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#define STATIC static
#define DPRINTF(a)
#endif


STATIC void cardbusattach(struct device *, struct device *, void *);
int cardbus_attach_card(struct cardbus_softc *);

STATIC int cardbusmatch(struct device *, struct cfdata *, void *);
static int cardbussubmatch(struct device *, struct cfdata *, void *);
static int cardbusprint(void *, const char *);

typedef void (*tuple_decode_func)(u_int8_t*, int, void*);

static int decode_tuples(u_int8_t *, int, tuple_decode_func, void*);
#ifdef CARDBUS_DEBUG
static void print_tuple(u_int8_t*, int, void*);
#endif

static int cardbus_read_tuples(struct cardbus_attach_args *,
    cardbusreg_t, u_int8_t *, size_t);

static void enable_function(struct cardbus_softc *, int, int);
static void disable_function(struct cardbus_softc *, int);

CFATTACH_DECL(cardbus, sizeof(struct cardbus_softc),
    cardbusmatch, cardbusattach, NULL, NULL);

#ifndef __NetBSD_Version__
struct cfdriver cardbus_cd = {
	NULL, "cardbus", DV_DULL
};
#endif


STATIC int
cardbusmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct cbslot_attach_args *cba = aux;

	if (strcmp(cba->cba_busname, cf->cf_name)) {
		DPRINTF(("cardbusmatch: busname differs %s <=> %s\n",
		    cba->cba_busname, cf->cf_name));
		return (0);
	}

	return (1);
}

STATIC void
cardbusattach(struct device *parent, struct device *self, void *aux)
{
	struct cardbus_softc *sc = (void *)self;
	struct cbslot_attach_args *cba = aux;

	sc->sc_bus = cba->cba_bus;
	sc->sc_device = 0;
	sc->sc_intrline = cba->cba_intrline;
	sc->sc_cacheline = cba->cba_cacheline;
	sc->sc_lattimer = cba->cba_lattimer;

	printf(": bus %d device %d", sc->sc_bus, sc->sc_device);
	if (bootverbose)
		printf(" cacheline 0x%x, lattimer 0x%x", sc->sc_cacheline,
		       sc->sc_lattimer);
	printf("\n");

	sc->sc_iot = cba->cba_iot;	/* CardBus I/O space tag */
	sc->sc_memt = cba->cba_memt;	/* CardBus MEM space tag */
	sc->sc_dmat = cba->cba_dmat;	/* DMA tag */
	sc->sc_cc = cba->cba_cc;
	sc->sc_cf = cba->cba_cf;

#if rbus
	sc->sc_rbus_iot = cba->cba_rbus_iot;
	sc->sc_rbus_memt = cba->cba_rbus_memt;
#endif

	sc->sc_funcs = NULL;
}

static int
cardbus_read_tuples(struct cardbus_attach_args *ca, cardbusreg_t cis_ptr,
    u_int8_t *tuples, size_t len)
{
	struct cardbus_softc *sc = ca->ca_ct->ct_sc;
	cardbus_chipset_tag_t cc = ca->ca_ct->ct_cc;
	cardbus_function_tag_t cf = ca->ca_ct->ct_cf;
	cardbustag_t tag = ca->ca_tag;
	cardbusreg_t command;
	bus_space_tag_t bar_tag;
	bus_space_handle_t bar_memh;
	bus_size_t bar_size;
	bus_addr_t bar_addr;
	cardbusreg_t reg;
	int found = 0;
	int cardbus_space = cis_ptr & CARDBUS_CIS_ASIMASK;
	int i, j;

	memset(tuples, 0, len);

	cis_ptr = cis_ptr & CARDBUS_CIS_ADDRMASK;

	switch (cardbus_space) {
	case CARDBUS_CIS_ASI_TUPLE:
		DPRINTF(("%s: reading CIS data from configuration space\n",
		    sc->sc_dev.dv_xname));
		for (i = cis_ptr, j = 0; i < 0xff; i += 4) {
			u_int32_t e = (*cf->cardbus_conf_read)(cc, tag, i);
			tuples[j] = 0xff & e;
			e >>= 8;
			tuples[j + 1] = 0xff & e;
			e >>= 8;
			tuples[j + 2] = 0xff & e;
			e >>= 8;
			tuples[j + 3] = 0xff & e;
			j += 4;
		}
		found++;
		break;

	case CARDBUS_CIS_ASI_BAR0:
	case CARDBUS_CIS_ASI_BAR1:
	case CARDBUS_CIS_ASI_BAR2:
	case CARDBUS_CIS_ASI_BAR3:
	case CARDBUS_CIS_ASI_BAR4:
	case CARDBUS_CIS_ASI_BAR5:
	case CARDBUS_CIS_ASI_ROM:
		if (cardbus_space == CARDBUS_CIS_ASI_ROM) {
			reg = CARDBUS_ROM_REG;
			DPRINTF(("%s: reading CIS data from ROM\n",
			    sc->sc_dev.dv_xname));
		} else {
			reg = CARDBUS_BASE0_REG + (cardbus_space - 1) * 4;
			DPRINTF(("%s: reading CIS data from BAR%d\n",
			    sc->sc_dev.dv_xname, cardbus_space - 1));
		}

		/*
		 * XXX zero register so mapreg_map doesn't get confused by old
		 * contents.
		 */
		cardbus_conf_write(cc, cf, tag, reg, 0);
		if (Cardbus_mapreg_map(ca->ca_ct, reg,
		    CARDBUS_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
		    0, &bar_tag, &bar_memh, &bar_addr, &bar_size)) {
			printf("%s: failed to map memory\n",
			    sc->sc_dev.dv_xname);
			return (1);
		}

		if (cardbus_space == CARDBUS_CIS_ASI_ROM) {
			cardbusreg_t exrom;
			int save;
			struct cardbus_rom_image_head rom_image;
			struct cardbus_rom_image *p;

			save = splhigh();
			/* enable rom address decoder */
			exrom = cardbus_conf_read(cc, cf, tag, reg);
			cardbus_conf_write(cc, cf, tag, reg, exrom | 1);

			command = cardbus_conf_read(cc, cf, tag,
			    CARDBUS_COMMAND_STATUS_REG);
			cardbus_conf_write(cc, cf, tag,
			    CARDBUS_COMMAND_STATUS_REG,
			    command | CARDBUS_COMMAND_MEM_ENABLE);

			if (cardbus_read_exrom(ca->ca_memt, bar_memh,
			    &rom_image))
				goto out;

			SIMPLEQ_FOREACH(p, &rom_image, next) {
				if (p->rom_image ==
				    CARDBUS_CIS_ASI_ROM_IMAGE(cis_ptr)) {
					bus_space_read_region_1(p->romt,
					    p->romh, CARDBUS_CIS_ADDR(cis_ptr),
					    tuples, 256);
					found++;
				}
				break;
			}
			while ((p = SIMPLEQ_FIRST(&rom_image)) != NULL) {
				SIMPLEQ_REMOVE_HEAD(&rom_image, next);
				free(p, M_DEVBUF);
			}
		out:
			exrom = cardbus_conf_read(cc, cf, tag, reg);
			cardbus_conf_write(cc, cf, tag, reg, exrom & ~1);
			splx(save);
		} else {
			command = cardbus_conf_read(cc, cf, tag,
			    CARDBUS_COMMAND_STATUS_REG);
			cardbus_conf_write(cc, cf, tag,
			    CARDBUS_COMMAND_STATUS_REG,
			    command | CARDBUS_COMMAND_MEM_ENABLE);
			/* XXX byte order? */
			bus_space_read_region_1(ca->ca_memt, bar_memh,
			    cis_ptr, tuples, 256);
			found++;
		}
		command = cardbus_conf_read(cc, cf, tag,
		    CARDBUS_COMMAND_STATUS_REG);
		cardbus_conf_write(cc, cf, tag, CARDBUS_COMMAND_STATUS_REG,
		    command & ~CARDBUS_COMMAND_MEM_ENABLE);
		cardbus_conf_write(cc, cf, tag, reg, 0);

		Cardbus_mapreg_unmap(ca->ca_ct, reg, bar_tag, bar_memh,
		    bar_size);
		break;

#ifdef DIAGNOSTIC
	default:
		panic("%s: bad CIS space (%d)", sc->sc_dev.dv_xname,
		    cardbus_space);
#endif
	}
	return (!found);
}

static void
parse_tuple(u_int8_t *tuple, int len, void *data)
{
	struct cardbus_cis_info *cis = data;
	char *p;
	int i, bar_index;

	switch (tuple[0]) {
	case PCMCIA_CISTPL_MANFID:
		if (tuple[1] != 5) {
			DPRINTF(("%s: wrong length manufacturer id (%d)\n",
			    __func__, tuple[1]));
			break;
		}
		cis->manufacturer = tuple[2] | (tuple[3] << 8);
		cis->product = tuple[4] | (tuple[5] << 8);
		break;

	case PCMCIA_CISTPL_VERS_1:
		memcpy(cis->cis1_info_buf, tuple + 2, tuple[1]);
		i = 0;
		p = cis->cis1_info_buf + 2;
		while (i <
		    sizeof(cis->cis1_info) / sizeof(cis->cis1_info[0])) {
			cis->cis1_info[i++] = p;
			while (*p != '\0' && *p != '\xff')
				p++;
			if (*p == '\xff')
				break;
			p++;
		}
		break;

	case PCMCIA_CISTPL_BAR:
		if (tuple[1] != 6) {
			DPRINTF(("%s: BAR with short length (%d)\n",
			    __func__, tuple[1]));
			break;
		}
		bar_index = tuple[2] & 7;
		if (bar_index == 0) {
			DPRINTF(("%s: invalid ASI in BAR tuple\n", __func__));
			break;
		}
		bar_index--;
		cis->bar[bar_index].flags = tuple[2];
		cis->bar[bar_index].size =
		    (tuple[4] << 0) |
		    (tuple[5] << 8) |
		    (tuple[6] << 16) |
		    (tuple[7] << 24);
		break;

	case PCMCIA_CISTPL_FUNCID:
		cis->funcid = tuple[2];
		break;

	case PCMCIA_CISTPL_FUNCE:
		switch (cis->funcid) {
		case PCMCIA_FUNCTION_SERIAL:
			if (tuple[1] >= 2 &&
			    /* XXX PCMCIA_TPLFE_TYPE_SERIAL_??? */
			    tuple[2] == 0) {
				cis->funce.serial.uart_type = tuple[3] & 0x1f;
				cis->funce.serial.uart_present = 1;
			}
			break;

		case PCMCIA_FUNCTION_NETWORK:
			if (tuple[1] >= 8 &&
			    tuple[2] == PCMCIA_TPLFE_TYPE_LAN_NID) {
				if (tuple[3] >
				    sizeof(cis->funce.network.netid)) {
					DPRINTF(("%s: unknown network id type "
					    "(len = %d)\n",
					    __func__, tuple[3]));
				} else {
					cis->funce.network.netid_present = 1;
					memcpy(cis->funce.network.netid,
					    tuple + 4, tuple[3]);
				}
			}
			break;
		}
		break;
	}
}

/*
 * int cardbus_attach_card(struct cardbus_softc *sc)
 *
 *    This function attaches the card on the slot: turns on power,
 *    reads and analyses tuple, sets configuration index.
 *
 *    This function returns the number of recognised device functions.
 *    If no functions are recognised, return 0.
 */
int
cardbus_attach_card(struct cardbus_softc *sc)
{
	cardbus_chipset_tag_t cc;
	cardbus_function_tag_t cf;
	cardbustag_t tag;
	cardbusreg_t id, class, cis_ptr;
	cardbusreg_t bhlc;
	u_int8_t tuple[2048];
	int cdstatus;
	int function, nfunction;
	struct cardbus_devfunc **previous_next = &(sc->sc_funcs);
	struct device *csc;
	int no_work_funcs = 0;
	cardbus_devfunc_t ct;

	cc = sc->sc_cc;
	cf = sc->sc_cf;

	DPRINTF(("cardbus_attach_card: cb%d start\n", sc->sc_dev.dv_unit));

	/* inspect initial voltage */
	if ((cdstatus = (*cf->cardbus_ctrl)(cc, CARDBUS_CD)) == 0) {
		DPRINTF(("cardbusattach: no CardBus card on cb%d\n",
		    sc->sc_dev.dv_unit));
		return (0);
	}

	/*
	 * XXX use fake function 8 to keep power on during whole
	 * configuration.
	 */
	enable_function(sc, cdstatus, 8);
	function = 0;

	tag = cardbus_make_tag(cc, cf, sc->sc_bus, sc->sc_device, function);

	/*
	 * Wait until power comes up.  Maxmum 500 ms.
	 */
	{
		int i;

		for (i = 0; i < 5; ++i) {
			id = cardbus_conf_read(cc, cf, tag, CARDBUS_ID_REG);
			if (id != 0xffffffff && id != 0) {
				break;
			}
			if (cold) {	/* before kernel thread invoked */
				delay(100 * 1000);
			} else {	/* thread context */
				if (tsleep((void *)sc, PCATCH, "cardbus",
				    hz / 10) != EWOULDBLOCK) {
					break;
				}
			}
		}
		if (i == 5) {
			return (0);
		}
	}

	bhlc = cardbus_conf_read(cc, cf, tag, CARDBUS_BHLC_REG);
	DPRINTF(("%s bhlc 0x%08x -> ", sc->sc_dev.dv_xname, bhlc));
	nfunction = CARDBUS_HDRTYPE_MULTIFN(bhlc) ? 8 : 1;

	for (function = 0; function < nfunction; function++) {
		struct cardbus_attach_args ca;

		tag = cardbus_make_tag(cc, cf, sc->sc_bus, sc->sc_device,
		    function);

		id = cardbus_conf_read(cc, cf, tag, CARDBUS_ID_REG);
		class = cardbus_conf_read(cc, cf, tag, CARDBUS_CLASS_REG);
		cis_ptr = cardbus_conf_read(cc, cf, tag, CARDBUS_CIS_REG);

		/* Invalid vendor ID value? */
		if (CARDBUS_VENDOR(id) == CARDBUS_VENDOR_INVALID) {
			continue;
		}

		DPRINTF(("cardbus_attach_card: "
		    "Vendor 0x%x, Product 0x%x, CIS 0x%x\n",
		    CARDBUS_VENDOR(id), CARDBUS_PRODUCT(id), cis_ptr));

		enable_function(sc, cdstatus, function);

		/* clean up every BAR */
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE0_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE1_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE2_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE3_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE4_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_BASE5_REG, 0);
		cardbus_conf_write(cc, cf, tag, CARDBUS_ROM_REG, 0);

		/* set initial latency and cacheline size */
		bhlc = cardbus_conf_read(cc, cf, tag, CARDBUS_BHLC_REG);
		DPRINTF(("%s func%d bhlc 0x%08x -> ", sc->sc_dev.dv_xname,
		    function, bhlc));
		bhlc &= ~((CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT) |
		    (CARDBUS_CACHELINE_MASK << CARDBUS_CACHELINE_SHIFT));
		bhlc |= ((sc->sc_cacheline & CARDBUS_CACHELINE_MASK) << CARDBUS_CACHELINE_SHIFT);
		bhlc |= ((sc->sc_lattimer & CARDBUS_LATTIMER_MASK) << CARDBUS_LATTIMER_SHIFT);

		cardbus_conf_write(cc, cf, tag, CARDBUS_BHLC_REG, bhlc);
		bhlc = cardbus_conf_read(cc, cf, tag, CARDBUS_BHLC_REG);
		DPRINTF(("0x%08x\n", bhlc));
		
		if (CARDBUS_LATTIMER(bhlc) < 0x10) {
			bhlc &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
			bhlc |= (0x10 << CARDBUS_LATTIMER_SHIFT);
			cardbus_conf_write(cc, cf, tag, CARDBUS_BHLC_REG, bhlc);
		}

		/*
		 * We need to allocate the ct here, since we might
		 * need it when reading the CIS
		 */
		if ((ct = malloc(sizeof(struct cardbus_devfunc),
		    M_DEVBUF, M_NOWAIT)) == NULL) {
			panic("no room for cardbus_tag");
		}

		ct->ct_cc = sc->sc_cc;
		ct->ct_cf = sc->sc_cf;
		ct->ct_bus = sc->sc_bus;
		ct->ct_dev = sc->sc_device;
		ct->ct_func = function;
		ct->ct_sc = sc;
		ct->ct_next = NULL;
		*previous_next = ct;

		memset(&ca, 0, sizeof(ca));

		ca.ca_unit = sc->sc_dev.dv_unit;
		ca.ca_ct = ct;

		ca.ca_iot = sc->sc_iot;
		ca.ca_memt = sc->sc_memt;
		ca.ca_dmat = sc->sc_dmat;

#if rbus
		ca.ca_rbus_iot = sc->sc_rbus_iot;
		ca.ca_rbus_memt= sc->sc_rbus_memt;
#endif

		ca.ca_tag = tag;
		ca.ca_bus = sc->sc_bus;
		ca.ca_device = sc->sc_device;
		ca.ca_function = function;
		ca.ca_id = id;
		ca.ca_class = class;

		ca.ca_intrline = sc->sc_intrline;

		if (cardbus_read_tuples(&ca, cis_ptr, tuple, sizeof(tuple))) {
			printf("cardbus_attach_card: failed to read CIS\n");
		} else {
#ifdef CARDBUS_DEBUG
			decode_tuples(tuple, 2048, print_tuple, NULL);
#endif
			decode_tuples(tuple, 2048, parse_tuple, &ca.ca_cis);
		}

		if ((csc = config_found_sm((void *)sc, &ca, cardbusprint,
		    cardbussubmatch)) == NULL) {
			/* do not match */
			disable_function(sc, function);
			free(ct, M_DEVBUF);
			*previous_next = NULL;
		} else {
			/* found */
			previous_next = &(ct->ct_next);
			ct->ct_device = csc;
			++no_work_funcs;
		}
	}
	/*
	 * XXX power down pseudo function 8 (this will power down the card
	 * if no functions were attached).
	 */
	disable_function(sc, 8);

	return (no_work_funcs);
}

static int
cardbussubmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (cf->cardbuscf_dev != CARDBUS_UNK_DEV &&
	    cf->cardbuscf_dev != ca->ca_unit) {
		return (0);
	}
	if (cf->cardbuscf_function != CARDBUS_UNK_FUNCTION &&
	    cf->cardbuscf_function != ca->ca_function) {
		return (0);
	}

	return (config_match(parent, cf, aux));
}

static int
cardbusprint(void *aux, const char *pnp)
{
	struct cardbus_attach_args *ca = aux;
	char devinfo[256];
	int i;

	if (pnp) {
		pci_devinfo(ca->ca_id, ca->ca_class, 1, devinfo);
		for (i = 0; i < 4; i++) {
			if (ca->ca_cis.cis1_info[i] == NULL)
				break;
			if (i)
				printf(", ");
			printf("%s", ca->ca_cis.cis1_info[i]);
		}
		if (bootverbose) {
			if (i)
				printf(" ");
			printf("(manufacturer 0x%x, product 0x%x)",
			       ca->ca_cis.manufacturer, ca->ca_cis.product);
		}
		printf(" %s at %s", devinfo, pnp);
	}
	printf(" dev %d function %d", ca->ca_device, ca->ca_function);

	return (UNCONF);
}

/*
 * void cardbus_detach_card(struct cardbus_softc *sc)
 *
 *    This function detaches the card on the slot: detach device data
 *    structure and turns off the power.
 *
 *    This function must not be called under interrupt context.
 */
void
cardbus_detach_card(struct cardbus_softc *sc)
{
	struct cardbus_devfunc *ct, *ct_next, **prev_next;

	prev_next = &(sc->sc_funcs->ct_next);

	for (ct = sc->sc_funcs; ct != NULL; ct = ct_next) {
		struct device *fndev = ct->ct_device;
		ct_next = ct->ct_next;

		DPRINTF(("%s: detaching %s\n", sc->sc_dev.dv_xname,
		    fndev->dv_xname));
		/* call device detach function */

		if (0 != config_detach(fndev, 0)) {
			printf("%s: cannot detach dev %s, function %d\n",
			    sc->sc_dev.dv_xname, fndev->dv_xname, ct->ct_func);
			prev_next = &(ct->ct_next);
		} else {
			sc->sc_poweron_func &= ~(1 << ct->ct_func);
			*prev_next = ct->ct_next;
			free(ct, M_DEVBUF);
		}
	}

	sc->sc_poweron_func = 0;
	(*sc->sc_cf->cardbus_power)(sc->sc_cc,
	    CARDBUS_VCC_0V | CARDBUS_VPP_0V);
}

/*
 * void *cardbus_intr_establish(cc, cf, irq, level, func, arg)
 *   Interrupt handler of pccard.
 *  args:
 *   cardbus_chipset_tag_t *cc
 *   int irq:
 */
void *
cardbus_intr_establish(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    cardbus_intr_handle_t irq, int level, int (*func)(void *), void *arg)
{

	DPRINTF(("- cardbus_intr_establish: irq %d\n", irq));
	return ((*cf->cardbus_intr_establish)(cc, irq, level, func, arg));
}

/*
 * void cardbus_intr_disestablish(cc, cf, handler)
 *   Interrupt handler of pccard.
 *  args:
 *   cardbus_chipset_tag_t *cc
 */
void
cardbus_intr_disestablish(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    void *handler)
{

	DPRINTF(("- pccard_intr_disestablish\n"));
	(*cf->cardbus_intr_disestablish)(cc, handler);
}

/*
 * XXX this should be merged with cardbus_function_{enable,disable},
 * but we don't have a ct when these functions are called.
 */
static void
enable_function(struct cardbus_softc *sc, int cdstatus, int function)
{

	if (sc->sc_poweron_func == 0) {
		/* switch to 3V and/or wait for power to stabilize */
		if (cdstatus & CARDBUS_3V_CARD) {
			/*
			 * sc_poweron_func must be substituted before
			 * entering sleep, in order to avoid turn on
			 * power twice.
			 */
			sc->sc_poweron_func |= (1 << function);
			(*sc->sc_cf->cardbus_power)(sc->sc_cc, CARDBUS_VCC_3V);
		} else {
			/* No cards other than 3.3V cards. */
			return;
		}
		(*sc->sc_cf->cardbus_ctrl)(sc->sc_cc, CARDBUS_RESET);
	}
	sc->sc_poweron_func |= (1 << function);
}

static void
disable_function(struct cardbus_softc *sc, int function)
{

	sc->sc_poweron_func &= ~(1 << function);
	if (sc->sc_poweron_func == 0) {
		/* power-off because no functions are enabled */
		(*sc->sc_cf->cardbus_power)(sc->sc_cc, CARDBUS_VCC_0V);
	}
}

/*
 * int cardbus_function_enable(struct cardbus_softc *sc, int func)
 *
 *   This function enables a function on a card.  When no power is
 *  applied on the card, power will be applied on it.
 */
int
cardbus_function_enable(struct cardbus_softc *sc, int func)
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	cardbusreg_t command;
	cardbustag_t tag;

	DPRINTF(("entering cardbus_function_enable...  "));

	/* entering critical area */

	/* XXX: sc_vold should be used */
	enable_function(sc, CARDBUS_3V_CARD, func);

	/* exiting critical area */

	tag = cardbus_make_tag(cc, cf, sc->sc_bus, sc->sc_device, func);

	command = cardbus_conf_read(cc, cf, tag, CARDBUS_COMMAND_STATUS_REG);
	command |= (CARDBUS_COMMAND_MEM_ENABLE | CARDBUS_COMMAND_IO_ENABLE |
	    CARDBUS_COMMAND_MASTER_ENABLE); /* XXX: good guess needed */

	cardbus_conf_write(cc, cf, tag, CARDBUS_COMMAND_STATUS_REG, command);

	cardbus_free_tag(cc, cf, tag);

	DPRINTF(("%x\n", sc->sc_poweron_func));

	return (0);
}

/*
 * int cardbus_function_disable(struct cardbus_softc *, int func)
 *
 *   This function disable a function on a card.  When no functions are
 *  enabled, it turns off the power.
 */
int
cardbus_function_disable(struct cardbus_softc *sc, int func)
{

	DPRINTF(("entering cardbus_function_disable...  "));

	disable_function(sc, func);

	return (0);
}

/*
 * int cardbus_get_capability(cardbus_chipset_tag_t cc,
 *	cardbus_function_tag_t cf, cardbustag_t tag, int capid, int *offset,
 *	cardbusreg_t *value)
 *
 *	Find the specified PCI capability.
 */
int
cardbus_get_capability(cardbus_chipset_tag_t cc, cardbus_function_tag_t cf,
    cardbustag_t tag, int capid, int *offset, cardbusreg_t *value)
{
	cardbusreg_t reg;
	unsigned int ofs;

	reg = cardbus_conf_read(cc, cf, tag, PCI_COMMAND_STATUS_REG);
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return (0);

	ofs = PCI_CAPLIST_PTR(cardbus_conf_read(cc, cf, tag,
	    PCI_CAPLISTPTR_REG));
	while (ofs != 0) {
#ifdef DIAGNOSTIC
		if ((ofs & 3) || (ofs < 0x40))
			panic("cardbus_get_capability");
#endif
		reg = cardbus_conf_read(cc, cf, tag, ofs);
		if (PCI_CAPLIST_CAP(reg) == capid) {
			if (offset)
				*offset = ofs;
			if (value)
				*value = reg;
			return (1);
		}
		ofs = PCI_CAPLIST_NEXT(reg);
	}

	return (0);
}

/*
 * below this line, there are some functions for decoding tuples.
 * They should go out from this file.
 */

static u_int8_t *
decode_tuple(u_int8_t *tuple, tuple_decode_func func, void *data);

static int
decode_tuples(u_int8_t *tuple, int buflen, tuple_decode_func func, void *data)
{
	u_int8_t *tp = tuple;

	if (PCMCIA_CISTPL_LINKTARGET != *tuple) {
		DPRINTF(("WRONG TUPLE: 0x%x\n", *tuple));
		return (0);
	}

	while (NULL != (tp = decode_tuple(tp, func, data))) {
		if (tuple + buflen < tp) {
			break;
		}
	}

	return (1);
}

static u_int8_t *
decode_tuple(u_int8_t *tuple, tuple_decode_func func, void *data)
{
	u_int8_t type;
	u_int8_t len;

	type = tuple[0];
	len = tuple[1] + 2;

	(*func)(tuple, len, data);

	if (type == PCMCIA_CISTPL_END) {
		return (NULL);
	}

	return (tuple + len);
}

#ifdef CARDBUS_DEBUG
static const char *tuple_name(int);
static const char *tuple_names[] = {
	"TPL_NULL", "TPL_DEVICE", "Reserved", "Reserved", /* 0-3 */
	"CONFIG_CB", "CFTABLE_ENTRY_CB", "Reserved", "BAR", /* 4-7 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 8-B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* C-F */
	"CHECKSUM", "LONGLINK_A", "LONGLINK_C", "LINKTARGET", /* 10-13 */
	"NO_LINK", "VERS_1", "ALTSTR", "DEVICE_A",
	"JEDEC_C", "JEDEC_A", "CONFIG", "CFTABLE_ENTRY",
	"DEVICE_OC", "DEVICE_OA", "DEVICE_GEO", "DEVICE_GEO_A",
	"MANFID", "FUNCID", "FUNCE", "SWIL", /* 20-23 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 24-27 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 28-2B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 2C-2F */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 30-33 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 34-37 */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 38-3B */
	"Reserved", "Reserved", "Reserved", "Reserved", /* 3C-3F */
	"VERS_2", "FORMAT", "GEOMETRY", "BYTEORDER",
	"DATE", "BATTERY", "ORG"
};
#define NAME_LEN(x) (sizeof x / sizeof(x[0]))

static const char *
tuple_name(int type)
{

	if (0 <= type && type < NAME_LEN(tuple_names)) {
		return (tuple_names[type]);
	} else if (type == 0xff) {
		return ("END");
	} else {
		return ("Reserved");
	}
}

static void
print_tuple(u_int8_t *tuple, int len, void *data)
{
	int i;

	printf("tuple: %s len %d\n", tuple_name(tuple[0]), len);

	for (i = 0; i < len; ++i) {
		if (i % 16 == 0) {
			printf("  0x%2x:", i);
		}
		printf(" %x", tuple[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	if (i % 16 != 0) {
		printf("\n");
	}
}
#endif
