/*	$NetBSD: vrip.c,v 1.11.2.2 2002/02/11 20:08:14 jdolecek Exp $	*/

/*-
 * Copyright (c) 1999, 2002
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "opt_vr41xx.h"
#include "opt_tx39xx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>

#include <hpcmips/vr/vr.h>
#include <hpcmips/vr/vrcpudef.h>
#include <hpcmips/vr/vripunit.h>
#include <hpcmips/vr/vripif.h>
#include <hpcmips/vr/vripreg.h>
#include <hpcmips/vr/vripvar.h>
#include <hpcmips/vr/icureg.h>
#include <hpcmips/vr/cmureg.h>
#include "locators.h"

#ifdef VRIP_DEBUG
#define DPRINTF_ENABLE
#define DPRINTF_DEBUG	vrip_debug
#endif
#define USE_HPC_DPRINTF
#include <machine/debug.h>

#ifdef VRIP_DEBUG
#define DBG_BIT_PRINT(reg) if (vrip_debug) dbg_bit_print(reg);
#define DUMP_LEVEL2MASK(sc,arg) if (vrip_debug) __vrip_dump_level2mask(sc,arg)
#else
#define DBG_BIT_PRINT(arg)
#define DUMP_LEVEL2MASK(sc,arg)
#endif

#define MAX_LEVEL1 32
#define VALID_UNIT(sc, unit)	(0 <= (unit) && (unit) < (sc)->sc_nunits)

struct vrip_softc {
	struct	device sc_dv;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	hpcio_chip_t sc_gpio_chips[VRIP_NIOCHIPS];
	vrcmu_chipset_tag_t sc_cc;
	int sc_pri; /* attaching device priority */
	u_int32_t sc_intrmask;
	struct vrip_chipset_tag sc_chipset;
	const struct vrip_unit *sc_units;
	int sc_nunits;
	struct intrhand {
		int	(*ih_fun)(void *);
		void	*ih_arg;
		const struct vrip_unit *ih_unit;
	} sc_intrhands[MAX_LEVEL1];
};

int	vripmatch(struct device *, struct cfdata *, void *);
void	vripattach(struct device *, struct device *, void *);
int	vrip_print(void *, const char *);
int	vrip_search(struct device *, struct cfdata *, void *);
int	vrip_intr(void *, u_int32_t, u_int32_t);

int __vrip_power(vrip_chipset_tag_t, int, int);
vrip_intr_handle_t __vrip_intr_establish(vrip_chipset_tag_t, int, int,
    int, int(*)(void*), void*);
void __vrip_intr_disestablish(vrip_chipset_tag_t, vrip_intr_handle_t);
void __vrip_intr_setmask1(vrip_chipset_tag_t, vrip_intr_handle_t, int);
void __vrip_intr_setmask2(vrip_chipset_tag_t, vrip_intr_handle_t,
    u_int32_t, int);
void __vrip_intr_getstatus2(vrip_chipset_tag_t, vrip_intr_handle_t,
    u_int32_t*);
void __vrip_register_cmu(vrip_chipset_tag_t, vrcmu_chipset_tag_t);
void __vrip_register_gpio(vrip_chipset_tag_t, hpcio_chip_t);
void __vrip_dump_level2mask(vrip_chipset_tag_t, void *);

struct cfattach vrip_ca = {
	sizeof(struct vrip_softc), vripmatch, vripattach
};

struct vrip_softc *the_vrip_sc = NULL;

static const struct vrip_chipset_tag vrip_chipset_methods = {
	.vc_power		= __vrip_power,
	.vc_intr_establish	= __vrip_intr_establish,
	.vc_intr_disestablish	= __vrip_intr_disestablish,
	.vc_intr_setmask1	= __vrip_intr_setmask1,
	.vc_intr_setmask2	= __vrip_intr_setmask2,
	.vc_intr_getstatus2	= __vrip_intr_getstatus2,
	.vc_register_cmu	= __vrip_register_cmu,
	.vc_register_gpio	= __vrip_register_gpio,
};

static const struct vrip_unit vrip_units[] = {
	[VRIP_UNIT_PMU] = { "pmu",
			    { VRIP_INTR_POWER,	VRIP_INTR_BAT,	},	},
	[VRIP_UNIT_RTC] = { "rtc",
			    { VRIP_INTR_RTCL1,	},		},
	[VRIP_UNIT_PIU] = { "piu",
			    { VRIP_INTR_PIU, },
			    CMUMASK_PIU,
			    ICUPIUINT_REG_W,	MPIUINT_REG_W	},
	[VRIP_UNIT_KIU] = { "kiu",
			    { VRIP_INTR_KIU,	},
			    CMUMASK_KIU,
			    KIUINT_REG_W,	MKIUINT_REG_W	},
	[VRIP_UNIT_SIU] = { "siu",
			    { VRIP_INTR_SIU,	},		},
	[VRIP_UNIT_GIU] = { "giu",
			    { VRIP_INTR_GIU,	},
			    0,
			    GIUINT_L_REG_W,MGIUINT_L_REG_W,
			    GIUINT_H_REG_W,	MGIUINT_H_REG_W	},
	[VRIP_UNIT_LED] = { "led",
			    { VRIP_INTR_LED,	},		},
	[VRIP_UNIT_AIU] = { "aiu",
			    { VRIP_INTR_AIU,	},
			    CMUMASK_AIU,
			    AIUINT_REG_W,	MAIUINT_REG_W	},
	[VRIP_UNIT_FIR] = { "fir",
			    { VRIP_INTR_FIR,	},
			    CMUMASK_FIR,
			    FIRINT_REG_W,	MFIRINT_REG_W	},
	[VRIP_UNIT_DSIU]= { "dsiu",
			    { VRIP_INTR_DSIU,	},
			    CMUMASK_DSIU,
			    DSIUINT_REG_W,	MDSIUINT_REG_W	},
	[VRIP_UNIT_PCIU]= { "pciu",
			    { VRIP_INTR_PCI,	},
			    CMUMASK_PCIU,
			    PCIINT_REG_W,	MPCIINT_REG_W	},
	[VRIP_UNIT_SCU] = { "scu",
			    { VRIP_INTR_SCU,	},
			    0,
			    SCUINT_REG_W,	MSCUINT_REG_W	},
	[VRIP_UNIT_CSI] = { "csi",
			    { VRIP_INTR_CSI,	},
			    CMUMASK_CSI,
			    CSIINT_REG_W,	MCSIINT_REG_W	},
	[VRIP_UNIT_BCU] = { "bcu",
			    { VRIP_INTR_BCU,	},
			    0,
			    BCUINT_REG_W,	MBCUINT_REG_W	}
};

int
vripmatch(struct device *parent, struct cfdata *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
   
#ifdef TX39XX 
	if (!platid_match(&platid, &platid_mask_CPU_MIPS_VR_41XX))
		return (0);
#endif /* TX39XX */
	if (strcmp(ma->ma_name, match->cf_driver->cd_name))
		return (0);

	return (1);
}

void
vripattach(struct device *parent, struct device *self, void *aux)
{
	struct vrip_softc *sc = (struct vrip_softc*)self;

	printf("\n");

	sc->sc_units = vrip_units;
	sc->sc_nunits = sizeof(vrip_units)/sizeof(struct vrip_unit);

	vripattach_common(parent, self, aux);
}

void
vripattach_common(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct vrip_softc *sc = (struct vrip_softc*)self;

	sc->sc_chipset = vrip_chipset_methods; /* structure assignment */
	sc->sc_chipset.vc_sc = sc;

	/*
	 *  Map ICU (Interrupt Control Unit) register space.
	 */
	sc->sc_iot = ma->ma_iot;
	if (bus_space_map(sc->sc_iot, VRIP_ICU_ADDR,
	    0x20	/*XXX lower area only*/,
	    0,		/* no flags */
	    &sc->sc_ioh)) {
		printf("vripattach: can't map ICU register.\n");
		return;
	}
	
	/*
	 *  Disable all Level 1 interrupts.
	 */
	sc->sc_intrmask = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, MSYSINT1_REG_W, 0x0000);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, MSYSINT2_REG_W, 0x0000);
	/*
	 *  Level 1 interrupts are redirected to HwInt0
	 */
	vr_intr_establish(VR_INTR0, vrip_intr, self);
	the_vrip_sc = sc;
	/*
	 *  Attach each devices
	 *	GIU CMU interface interface is used by other system device.
	 *	so attach first
	 */
	sc->sc_pri = 2;
	config_search(vrip_search, self, vrip_print);
	/* Other system devices. */
	sc->sc_pri = 1;
	config_search(vrip_search, self, vrip_print);
}

int
vrip_print(void *aux, const char *hoge)
{
	struct vrip_attach_args *va = (struct vrip_attach_args*)aux;

	if (va->va_addr)
		printf(" addr 0x%lx", va->va_addr);
	if (va->va_size > 1)
		printf("-0x%lx", va->va_addr + va->va_size - 1);

	return (UNCONF);
}

int
vrip_search(struct device *parent, struct cfdata *cf, void *aux)
{
	struct vrip_softc *sc = (struct vrip_softc *)parent;
	struct vrip_attach_args va;

	va.va_vc = &sc->sc_chipset;
	va.va_iot = sc->sc_iot;
	va.va_unit = cf->cf_loc[VRIPIFCF_UNIT];
	va.va_addr = cf->cf_loc[VRIPIFCF_ADDR];
	va.va_size = cf->cf_loc[VRIPIFCF_SIZE];
	va.va_addr2 = cf->cf_loc[VRIPIFCF_ADDR2];
	va.va_size2 = cf->cf_loc[VRIPIFCF_SIZE2];
	va.va_gpio_chips = sc->sc_gpio_chips;
	if (((*cf->cf_attach->ca_match)(parent, cf, &va) == sc->sc_pri))
		config_attach(parent, cf, &va, vrip_print);

	return (0);
}

int
__vrip_power(vrip_chipset_tag_t vc, int unit, int onoff)
{
	struct vrip_softc *sc = vc->vc_sc;
	const struct vrip_unit *vu;

	if (sc->sc_chipset.vc_cc == NULL)
		return (0);	/* You have no clock mask unit yet. */
	if (!VALID_UNIT(sc, unit))
		return (0);
	vu = &sc->sc_units[unit];

	return (*sc->sc_chipset.vc_cc->cc_clock)(sc->sc_chipset.vc_cc, 
	    vu->vu_clkmask, onoff);
}

vrip_intr_handle_t
__vrip_intr_establish(vrip_chipset_tag_t vc, int unit, int line, int level,
    int (*ih_fun)(void *), void *ih_arg)
{
	struct vrip_softc *sc = vc->vc_sc;
	const struct vrip_unit *vu;
	struct intrhand *ih;

	if (!VALID_UNIT(sc, unit))
		return (NULL);
	vu = &sc->sc_units[unit];
	ih = &sc->sc_intrhands[vu->vu_intr[line]];
	if (ih->ih_fun) /* Can't share level 1 interrupt */
		return (NULL);
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_unit = vu;
    
	/* Mask level 2 interrupt mask register. (disable interrupt) */
	vrip_intr_setmask2(vc, ih, ~0, 0);
	/* Unmask  Level 1 interrupt mask register (enable interrupt) */
	vrip_intr_setmask1(vc, ih, 1);

	return ((void *)ih);
}

void
__vrip_intr_disestablish(vrip_chipset_tag_t vc, vrip_intr_handle_t handle)
{
	struct intrhand *ih = handle;

	ih->ih_fun = NULL;
	ih->ih_arg = NULL;
	/* Mask level 2 interrupt mask register(if any). (disable interrupt) */
	vrip_intr_setmask2(vc, ih, ~0, 0);
	/* Mask  Level 1 interrupt mask register (disable interrupt) */
	vrip_intr_setmask1(vc, ih, 0);
}

void
vrip_intr_suspend()
{
	bus_space_tag_t iot = the_vrip_sc->sc_iot;
	bus_space_handle_t ioh = the_vrip_sc->sc_ioh;

	bus_space_write_2 (iot, ioh, MSYSINT1_REG_W, (1<<VRIP_INTR_POWER));
	bus_space_write_2 (iot, ioh, MSYSINT2_REG_W, 0);
}

void
vrip_intr_resume()
{
	u_int32_t reg = the_vrip_sc->sc_intrmask;
	bus_space_tag_t iot = the_vrip_sc->sc_iot;
	bus_space_handle_t ioh = the_vrip_sc->sc_ioh;

	bus_space_write_2 (iot, ioh, MSYSINT1_REG_W, reg & 0xffff);
	bus_space_write_2 (iot, ioh, MSYSINT2_REG_W, (reg >> 16) & 0xffff);
}

/* Set level 1 interrupt mask. */
void
__vrip_intr_setmask1(vrip_chipset_tag_t vc, vrip_intr_handle_t handle,
    int enable)
{
	struct vrip_softc *sc = vc->vc_sc;
	struct intrhand *ih = handle;
	int level1 = ih - sc->sc_intrhands;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t reg = sc->sc_intrmask;

	DPRINTF(("__vrip_intr_setmask1: SYSINT: %s %d\n",
		 enable ? "enable" : "disable", level1));
	reg = (bus_space_read_2 (iot, ioh, MSYSINT1_REG_W)&0xffff) |
	    ((bus_space_read_2 (iot, ioh, MSYSINT2_REG_W)<< 16)&0xffff0000);
	if (enable)
		reg |= (1 << level1);
	else {
		reg &= ~(1 << level1);	
	}
	sc->sc_intrmask = reg;
	bus_space_write_2 (iot, ioh, MSYSINT1_REG_W, reg & 0xffff);
	bus_space_write_2 (iot, ioh, MSYSINT2_REG_W, (reg >> 16) & 0xffff);
	DBG_BIT_PRINT(reg);
    
	return;
}

void
__vrip_dump_level2mask(vrip_chipset_tag_t vc, vrip_intr_handle_t handle)
{
	struct vrip_softc *sc = vc->vc_sc;
	struct intrhand *ih = handle;
	const struct vrip_unit *vu = ih->ih_unit;
	u_int32_t reg;
    
	if (vu->vu_mlreg) {
		DPRINTF(("level1[%d] level2 mask:", vu->vu_intr[0]));
		reg = bus_space_read_2(sc->sc_iot, sc->sc_ioh, vu->vu_mlreg);
		if (vu->vu_mhreg) { /* GIU [16:31] case only */
			reg |= (bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    vu->vu_mhreg) << 16);
			dbg_bit_print(reg);
		} else
			dbg_bit_print(reg);
	}
}

/* Get level 2 interrupt status */
void
__vrip_intr_getstatus2(vrip_chipset_tag_t vc, vrip_intr_handle_t handle,
    u_int32_t *mask /* Level 2 mask */)
{
	struct vrip_softc *sc = vc->vc_sc;
	struct intrhand *ih = handle;
	const struct vrip_unit *vu = ih->ih_unit;
	u_int32_t reg;

	reg = bus_space_read_2(sc->sc_iot, sc->sc_ioh, 
	    vu->vu_lreg);
	reg |= ((bus_space_read_2(sc->sc_iot, sc->sc_ioh, 
	    vu->vu_hreg) << 16)&0xffff0000);
/*    dbg_bit_print(reg);*/
	*mask = reg;
}

/* Set level 2 interrupt mask. */
void
__vrip_intr_setmask2(vrip_chipset_tag_t vc, vrip_intr_handle_t handle,
    u_int32_t mask /* Level 2 mask */, int onoff)
{
	struct vrip_softc *sc = vc->vc_sc;
	struct intrhand *ih = handle;
	const struct vrip_unit *vu = ih->ih_unit;
	u_int16_t reg;

	DPRINTF(("vrip_intr_setmask2:\n"));
	DUMP_LEVEL2MASK(vc, handle);
#ifdef WINCE_DEFAULT_SETTING
#warning WINCE_DEFAULT_SETTING
#else
	if (vu->vu_mlreg) {
		reg = bus_space_read_2(sc->sc_iot, sc->sc_ioh, vu->vu_mlreg);
		if (onoff)
			reg |= (mask&0xffff);
		else
			reg &= ~(mask&0xffff);
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, vu->vu_mlreg, reg);
		if (vu->vu_mhreg != -1) { /* GIU [16:31] case only */
			reg = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    vu->vu_mhreg);
			if (onoff)
				reg |= ((mask >> 16) & 0xffff);
			else
				reg &= ~((mask >> 16) & 0xffff);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    vu->vu_mhreg, reg);
		}
	}
#endif /* WINCE_DEFAULT_SETTING */
	DUMP_LEVEL2MASK(vc, handle);

	return;
}

int
vrip_intr(void *arg, u_int32_t pc, u_int32_t statusReg)
{
	struct vrip_softc *sc = (struct vrip_softc*)arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;
	u_int32_t reg, mask;
	/*
	 *  Read level1 interrupt status.
	 */
	reg = (bus_space_read_2 (iot, ioh, SYSINT1_REG_W)&0xffff) |
	    ((bus_space_read_2 (iot, ioh, SYSINT2_REG_W)<< 16)&0xffff0000);
	mask = (bus_space_read_2 (iot, ioh, MSYSINT1_REG_W)&0xffff) |
	    ((bus_space_read_2 (iot, ioh, MSYSINT2_REG_W)<< 16)&0xffff0000);
	reg &= mask;

	/*
	 *  Dispatch each handler.
	 */
	for (i = 0; i < 32; i++) {
		register struct intrhand *ih = &sc->sc_intrhands[i];
		if (ih->ih_fun && (reg & (1 << i))) {
			ih->ih_fun(ih->ih_arg);
		}
	}

	return (1);
}

void
__vrip_register_cmu(vrip_chipset_tag_t vc, vrcmu_chipset_tag_t cmu)
{
	struct vrip_softc *sc = vc->vc_sc;

	sc->sc_chipset.vc_cc = cmu;
}

void
__vrip_register_gpio(vrip_chipset_tag_t vc, hpcio_chip_t chip)
{
	struct vrip_softc *sc = vc->vc_sc;

	if (chip->hc_chipid < 0 || VRIP_NIOCHIPS <= chip->hc_chipid)
		panic("%s: '%s' has unknown id, %d", __FUNCTION__,
		    chip->hc_name, chip->hc_chipid);
	sc->sc_gpio_chips[chip->hc_chipid] = chip;
}
