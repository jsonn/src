/*	$NetBSD: cardbusvar.h,v 1.3.4.1 1999/11/15 00:40:19 fvdl Exp $	*/

/*
 * Copyright (c) 1998 and 1999
 *       HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by the author.
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

#if !defined SYS_DEV_CARDBUS_CARDBUSVAR_H
#define SYS_DEV_CARDBUS_CARDBUSVAR_H

#include <dev/pci/pcivar.h>	/* for pcitag_t */

#if 1
#include <dev/cardbus/rbus.h>
#endif



typedef void *cardbus_chipset_tag_t;
typedef int cardbus_intr_handle_t;


/* XXX they must be in cardbusreg.h */
typedef u_int32_t cardbusreg_t;
typedef pcitag_t cardbustag_t;
typedef int cardbus_intr_line_t;

#define CARDBUS_ID_REG          0x00

typedef u_int16_t cardbus_vendor_id_t;
typedef u_int16_t cardbus_product_id_t;

#  define CARDBUS_VENDOR_SHIFT  0
#  define CARDBUS_VENDOR_MASK   0xffff
#  define CARDBUS_VENDOR(id) \
	    (((id) >> CARDBUS_VENDOR_SHIFT) & CARDBUS_VENDOR_MASK)

#  define CARDBUS_PRODUCT_SHIFT  16
#  define CARDBUS_PRODUCT_MASK   0xffff
#  define CARDBUS_PRODUCT(id) \
	    (((id) >> CARDBUS_PRODUCT_SHIFT) & CARDBUS_PRODUCT_MASK)


#define	CARDBUS_COMMAND_STATUS_REG  0x04

#  define CARDBUS_COMMAND_IO_ENABLE     0x00000001
#  define CARDBUS_COMMAND_MEM_ENABLE    0x00000002
#  define CARDBUS_COMMAND_MASTER_ENABLE 0x00000004


#define CARDBUS_CLASS_REG       0x08

#define	CARDBUS_CLASS_SHIFT				24
#define	CARDBUS_CLASS_MASK				0xff
#define	CARDBUS_CLASS(cr) \
	    (((cr) >> CARDBUS_CLASS_SHIFT) & CARDBUS_CLASS_MASK)

#define	CARDBUS_SUBCLASS_SHIFT			16
#define	CARDBUS_SUBCLASS_MASK			0xff
#define	CARDBUS_SUBCLASS(cr) \
	    (((cr) >> CARDBUS_SUBCLASS_SHIFT) & CARDBUS_SUBCLASS_MASK)

#define	CARDBUS_INTERFACE_SHIFT			8
#define	CARDBUS_INTERFACE_MASK			0xff
#define	CARDBUS_INTERFACE(cr) \
	    (((cr) >> CARDBUS_INTERFACE_SHIFT) & CARDBUS_INTERFACE_MASK)

#define	CARDBUS_REVISION_SHIFT			0
#define	CARDBUS_REVISION_MASK			0xff
#define	CARDBUS_REVISION(cr) \
	    (((cr) >> CARDBUS_REVISION_SHIFT) & CARDBUS_REVISION_MASK)

/* base classes */
#define	CARDBUS_CLASS_PREHISTORIC		0x00
#define	CARDBUS_CLASS_MASS_STORAGE		0x01
#define	CARDBUS_CLASS_NETWORK			0x02
#define	CARDBUS_CLASS_DISPLAY			0x03
#define	CARDBUS_CLASS_MULTIMEDIA		0x04
#define	CARDBUS_CLASS_MEMORY			0x05
#define	CARDBUS_CLASS_BRIDGE			0x06
#define	CARDBUS_CLASS_COMMUNICATIONS		0x07
#define	CARDBUS_CLASS_SYSTEM			0x08
#define	CARDBUS_CLASS_INPUT			0x09
#define	CARDBUS_CLASS_DOCK			0x0a
#define	CARDBUS_CLASS_PROCESSOR			0x0b
#define	CARDBUS_CLASS_SERIALBUS			0x0c
#define	CARDBUS_CLASS_UNDEFINED			0xff

/* 0x0c serial bus subclasses */
#define	CARDBUS_SUBCLASS_SERIALBUS_FIREWIRE	0x00
#define	CARDBUS_SUBCLASS_SERIALBUS_ACCESS	0x01
#define	CARDBUS_SUBCLASS_SERIALBUS_SSA		0x02
#define	CARDBUS_SUBCLASS_SERIALBUS_USB		0x03
#define	CARDBUS_SUBCLASS_SERIALBUS_FIBER	0x04

/* BIST, Header Type, Latency Timer, Cache Line Size */
#define CARDBUS_BHLC_REG        0x0c

#define	CARDBUS_BIST_SHIFT        24
#define	CARDBUS_BIST_MASK       0xff
#define	CARDBUS_BIST(bhlcr) \
	    (((bhlcr) >> CARDBUS_BIST_SHIFT) & CARDBUS_BIST_MASK)

#define	CARDBUS_HDRTYPE_SHIFT     16
#define	CARDBUS_HDRTYPE_MASK    0xff
#define	CARDBUS_HDRTYPE(bhlcr) \
	    (((bhlcr) >> CARDBUS_HDRTYPE_SHIFT) & CARDBUS_HDRTYPE_MASK)

#define	CARDBUS_HDRTYPE_TYPE(bhlcr) \
	    (CARDBUS_HDRTYPE(bhlcr) & 0x7f)
#define	CARDBUS_HDRTYPE_MULTIFN(bhlcr) \
	    ((CARDBUS_HDRTYPE(bhlcr) & 0x80) != 0)

#define	CARDBUS_LATTIMER_SHIFT      8
#define	CARDBUS_LATTIMER_MASK    0xff
#define	CARDBUS_LATTIMER(bhlcr) \
	    (((bhlcr) >> CARDBUS_LATTIMER_SHIFT) & CARDBUS_LATTIMER_MASK)

#define	CARDBUS_CACHELINE_SHIFT     0
#define	CARDBUS_CACHELINE_MASK   0xff
#define	CARDBUS_CACHELINE(bhlcr) \
	    (((bhlcr) >> CARDBUS_CACHELINE_SHIFT) & CARDBUS_CACHELINE_MASK)


/* Base Resisters */
#define CARDBUS_BASE0_REG  0x10
#define CARDBUS_BASE1_REG  0x14
#define CARDBUS_BASE2_REG  0x18
#define CARDBUS_BASE3_REG  0x1C
#define CARDBUS_BASE4_REG  0x20
#define CARDBUS_BASE5_REG  0x24
#define CARDBUS_CIS_REG    0x28
#define CARDBUS_ROM_REG	   0x30
#  define CARDBUS_CIS_ASIMASK 0x07
#    define CARDBUS_CIS_ASI(x) (CARDBUS_CIS_ASIMASK & (x))
#  define CARDBUS_CIS_ASI_TUPLE 0x00
#  define CARDBUS_CIS_ASI_BAR0  0x01
#  define CARDBUS_CIS_ASI_BAR1  0x02
#  define CARDBUS_CIS_ASI_BAR2  0x03
#  define CARDBUS_CIS_ASI_BAR3  0x04
#  define CARDBUS_CIS_ASI_BAR4  0x05
#  define CARDBUS_CIS_ASI_BAR5  0x06
#  define CARDBUS_CIS_ASI_ROM   0x07
#  define CARDBUS_CIS_ADDRMASK 0x0ffffff8
#    define CARDBUS_CIS_ADDR(x) (CARDBUS_CIS_ADDRMASK & (x))
#    define CARDBUS_CIS_ASI_BAR(x) (((CARDBUS_CIS_ASIMASK & (x))-1)*4+0x10)
#    define CARDBUS_CIS_ASI_ROM_IMAGE(x) (((x) >> 28) & 0xf)

#define	CARDBUS_INTERRUPT_REG   0x3c

#define CARDBUS_MAPREG_TYPE_MEM		0x00000000
#define CARDBUS_MAPREG_TYPE_IO		0x00000001

/* XXX end */

#if rbus

typedef struct cardbus_functions {
  int (*cardbus_space_alloc) __P((cardbus_chipset_tag_t, rbus_tag_t,
				  bus_addr_t addr, bus_size_t size,
				  bus_addr_t mask, bus_size_t align,
				  int flags, bus_addr_t *addrp,
				  bus_space_handle_t *bshp));
  int (*cardbus_space_free) __P((cardbus_chipset_tag_t, rbus_tag_t,
				 bus_space_handle_t, bus_size_t));
  void *(*cardbus_intr_establish) __P((cardbus_chipset_tag_t, int irq, int level, int (*ih)(void *), void *sc));
  void (*cardbus_intr_disestablish) __P((cardbus_chipset_tag_t ct, void *ih));
  int (*cardbus_ctrl) __P((cardbus_chipset_tag_t, int));
  int (*cardbus_power) __P((cardbus_chipset_tag_t, int));

  cardbustag_t (*cardbus_make_tag) __P((cardbus_chipset_tag_t, int, int, int));
  void (*cardbus_free_tag) __P((cardbus_chipset_tag_t, cardbustag_t));
  cardbusreg_t (*cardbus_conf_read) __P((cardbus_chipset_tag_t, cardbustag_t, int));
  void (*cardbus_conf_write) __P((cardbus_chipset_tag_t, cardbustag_t, int, cardbusreg_t));
} cardbus_function_t, *cardbus_function_tag_t;

#else

typedef struct cardbus_functions {
  int (*cardbus_ctrl) __P((cardbus_chipset_tag_t, int));
  int (*cardbus_power) __P((cardbus_chipset_tag_t, int));
  int (*cardbus_mem_open) __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
  int (*cardbus_mem_close) __P((cardbus_chipset_tag_t, int));
  int (*cardbus_io_open) __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
  int (*cardbus_io_close) __P((cardbus_chipset_tag_t, int));
  void *(*cardbus_intr_establish) __P((cardbus_chipset_tag_t, int irq, int level, int (*ih)(void *), void *sc));
  void (*cardbus_intr_disestablish) __P((cardbus_chipset_tag_t ct, void *ih));

  cardbustag_t (*cardbus_make_tag) __P((cardbus_chipset_tag_t, int, int, int));  cardbusreg_t (*cardbus_conf_read) __P((cardbus_chipset_tag_t, cardbustag_t, int));
  void (*cardbus_conf_write) __P((cardbus_chipset_tag_t, cardbustag_t, int, cardbusreg_t));
} cardbus_function_t, *cardbus_function_tag_t;
#endif /* rbus */

/*
 * struct cbslot_attach_args is the attach argument for cardbus card.
 */
struct cbslot_attach_args {
  char *cba_busname;
  bus_space_tag_t cba_iot;	/* cardbus i/o space tag */
  bus_space_tag_t cba_memt;	/* cardbus mem space tag */
  bus_dma_tag_t cba_dmat;	/* DMA tag */

  int cba_bus;			/* cardbus bus number */
  int cba_function;		/* slot number on this Host Bus Adaptor */

  cardbus_chipset_tag_t cba_cc;	/* cardbus chipset */
  cardbus_function_tag_t cba_cf; /* cardbus functions */
  int cba_intrline;		/* interrupt line */

#if rbus
  rbus_tag_t cba_rbus_iot;	/* CardBus i/o rbus tag */
  rbus_tag_t cba_rbus_memt;	/* CardBus mem rbus tag */
#endif

  int cba_cacheline;		/* cache line size */
  int cba_lattimer;		/* latency timer */
};


#define cbslotcf_dev  cf_loc[0]
#define cbslotcf_func cf_loc[1]
#define CBSLOT_UNK_DEV -1
#define CBSLOT_UNK_FUNC -1


struct cardbus_devfunc;

/*
 * struct cardbus_softc is the softc for cardbus card.
 */
struct cardbus_softc {
  struct device sc_dev;		/* fundamental device structure */

  int sc_bus;			/* cardbus bus number */
  int sc_device;		/* cardbus device number */
  int sc_intrline;		/* CardBus intrline */
  
  bus_space_tag_t sc_iot;	/* CardBus I/O space tag */
  bus_space_tag_t sc_memt;	/* CardBus MEM space tag */
  bus_dma_tag_t sc_dmat;	/* DMA tag */

  cardbus_chipset_tag_t sc_cc;	/* CardBus chipset */
  cardbus_function_tag_t sc_cf;	/* CardBus function */

#if rbus
  rbus_tag_t sc_rbus_iot;	/* CardBus i/o rbus tag */
  rbus_tag_t sc_rbus_memt;	/* CardBus mem rbus tag */
#endif

  int sc_cacheline;		/* cache line size */
  int sc_lattimer;		/* latency timer */
  int sc_volt;			/* applied Vcc voltage */
#define PCCARD_33V  0x02
#define PCCARD_XXV  0x04
#define PCCARD_YYV  0x08
  int sc_poweron_func;
  struct cardbus_devfunc *sc_funcs;	/* list of cardbus device functions */
};


/*
 * struct cardbus_devfunc:
 *
 *   This is the data deposit for each function of a CardBus device.
 *   This structure is used for memory or i/o space allocation and
 *   disallocation.
 */
typedef struct cardbus_devfunc {
  cardbus_chipset_tag_t ct_cc;
  cardbus_function_tag_t ct_cf;
  struct cardbus_softc *ct_sc;	/* pointer to the parent */
  int ct_bus;			/* bus number */
  int ct_dev;			/* device number */
  int ct_func;			/* function number */

#if rbus
  rbus_tag_t ct_rbus_iot;	/* CardBus i/o rbus tag */
  rbus_tag_t ct_rbus_memt;	/* CardBus mem rbus tag */
#endif

  u_int32_t ct_bar[6];		/* Base Address Regs 0 to 6 */
  u_int32_t ct_lc;		/* Latency timer and cache line size */
  /* u_int32_t ct_cisreg; */    /* CIS reg: is it needed??? */

  struct device *ct_device;	/* pointer to the device */

  struct cardbus_devfunc *ct_next;

  /* some data structure needed for tuple??? */
} *cardbus_devfunc_t;


/* XXX various things extracted from CIS */
struct cardbus_cis_info {
    int32_t		manufacturer;
    int32_t		product;
    char		cis1_info_buf[256];
    char*		cis1_info[4];
    struct cb_bar_info {
	unsigned int flags;
	unsigned int size;
    } bar[7];
    unsigned int	funcid;
    union {
	struct {
	    char netid[6];
	} network;
    } funce;
};

struct cardbus_attach_args {
  int ca_unit;
  cardbus_devfunc_t ca_ct;

  bus_space_tag_t ca_iot;	/* CardBus I/O space tag */
  bus_space_tag_t ca_memt;	/* CardBus MEM space tag */
  bus_dma_tag_t ca_dmat;	/* DMA tag */

  u_int ca_device;
  u_int ca_function;
  cardbustag_t ca_tag;
  cardbusreg_t ca_id;
  cardbusreg_t ca_class;

  /* interrupt information */
  cardbus_intr_line_t ca_intrline;

#if rbus
  rbus_tag_t ca_rbus_iot;	/* CardBus i/o rbus tag */
  rbus_tag_t ca_rbus_memt;	/* CardBus mem rbus tag */
#endif

  struct cardbus_cis_info ca_cis;
};


#define CARDBUS_ENABLE  1	/* enable the channel */
#define CARDBUS_DISABLE 2	/* disable the channel */
#define CARDBUS_RESET   4
#define CARDBUS_CD      7
#  define CARDBUS_NOCARD 0
#  define CARDBUS_5V_CARD 0x01	/* XXX: It must not exist */
#  define CARDBUS_3V_CARD 0x02
#  define CARDBUS_XV_CARD 0x04
#  define CARDBUS_YV_CARD 0x08
#define CARDBUS_IO_ENABLE    100
#define CARDBUS_IO_DISABLE   101
#define CARDBUS_MEM_ENABLE   102
#define CARDBUS_MEM_DISABLE  103
#define CARDBUS_BM_ENABLE    104 /* bus master */
#define CARDBUS_BM_DISABLE   105

#define CARDBUS_VCC_UC  0x0000
#define CARDBUS_VCC_3V  0x0001
#define CARDBUS_VCC_XV  0x0002
#define CARDBUS_VCC_YV  0x0003
#define CARDBUS_VCC_0V  0x0004
#define CARDBUS_VCC_5V  0x0005	/* ??? */
#define CARDBUS_VCCMASK 0x000f
#define CARDBUS_VPP_UC  0x0000
#define CARDBUS_VPP_VCC 0x0010
#define CARDBUS_VPP_12V 0x0030
#define CARDBUS_VPP_0V  0x0040
#define CARDBUS_VPPMASK 0x00f0


#include "locators.h"

/*
 * Locators devies that attach to 'cardbus', as specified to config.
 */
#define cardbuscf_dev cf_loc[CARDBUSCF_DEV]
#define CARDBUS_UNK_DEV CARDBUSCF_DEV_DEFAULT

#define cardbuscf_function cf_loc[CARDBUSCF_FUNCTION]
#define CARDBUS_UNK_FUNCTION CARDBUSCF_FUNCTION_DEFAULT

int cardbus_attach_card __P((struct cardbus_softc *));
void *cardbus_intr_establish __P((cardbus_chipset_tag_t, cardbus_function_tag_t, cardbus_intr_handle_t irq, int level, int (*func) (void *), void *arg));
void cardbus_intr_disestablish __P((cardbus_chipset_tag_t, cardbus_function_tag_t, void *handler));

int cardbus_mapreg_map __P((struct cardbus_softc *, int, int, cardbusreg_t,
			    int, bus_space_tag_t *, bus_space_handle_t *,
			    bus_addr_t *, bus_size_t *));

int cardbus_save_bar __P((cardbus_devfunc_t));
int cardbus_restore_bar __P((cardbus_devfunc_t));

int cardbus_function_enable __P((struct cardbus_softc *, int function));
int cardbus_function_disable __P((struct cardbus_softc *, int function));

#define Cardbus_function_enable(ct) cardbus_function_enable((ct)->ct_sc, (ct)->ct_func)
#define Cardbus_function_disable(ct) cardbus_function_disable((ct)->ct_sc, (ct)->ct_func)



#define Cardbus_mapreg_map(ct, reg, type, busflags, tagp, handlep, basep, sizep) \
	cardbus_mapreg_map((ct)->ct_sc, (ct->ct_func), (reg), (type),\
			   (busflags), (tagp), (handlep), (basep), (sizep))

#define Cardbus_make_tag(ct) (*(ct)->ct_cf->cardbus_make_tag)((ct)->ct_cc, (ct)->ct_bus, (ct)->ct_dev, (ct)->ct_func)
#define cardbus_make_tag(cc, cf, bus, device, function) ((cf)->cardbus_make_tag)((cc), (bus), (device), (function))

#define Cardbus_free_tag(ct, tag) (*(ct)->ct_cf->cardbus_free_tag)((ct)->ct_cc, (tag))
#define cardbus_free_tag(cc, cf, tag) (*(cf)->cardbus_free_tag)(cc, (tag))

#define Cardbus_conf_read(ct, tag, offs) (*(ct)->ct_cf->cardbus_conf_read)((ct)->ct_cf, (tag), (offs))
#define cardbus_conf_read(cc, cf, tag, offs) ((cf)->cardbus_conf_read)((cc), (tag), (offs))
#define Cardbus_conf_write(ct, tag, offs, val) (*(ct)->ct_cf->cardbus_conf_write)((ct)->ct_cf, (tag), (offs), (val))
#define cardbus_conf_write(cc, cf, tag, offs, val) ((cf)->cardbus_conf_write)((cc), (tag), (offs), (val))

#endif /* SYS_DEV_CARDBUS_CARDBUSVAR_H */

