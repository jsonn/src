/*	$NetBSD: if_ne_pcmcia.c,v 1.70.2.2 2001/08/24 00:10:27 nathanw Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

#include <dev/ic/dl10019var.h>

#include <dev/ic/ax88190reg.h>
#include <dev/ic/ax88190var.h>

int	ne_pcmcia_match __P((struct device *, struct cfdata *, void *));
void	ne_pcmcia_attach __P((struct device *, struct device *, void *));
int	ne_pcmcia_detach __P((struct device *, int));

int	ne_pcmcia_enable __P((struct dp8390_softc *));
void	ne_pcmcia_disable __P((struct dp8390_softc *));

struct ne_pcmcia_softc {
	struct ne2000_softc sc_ne2000;		/* real "ne2000" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o information */
	int sc_asic_io_window;			/* i/o window for ASIC */
	int sc_nic_io_window;			/* i/o window for NIC */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handle */
};

u_int8_t *
	ne_pcmcia_get_enaddr __P((struct ne_pcmcia_softc *, int,
	    u_int8_t [ETHER_ADDR_LEN]));
u_int8_t *
	ne_pcmcia_dl10019_get_enaddr __P((struct ne_pcmcia_softc *,
	    u_int8_t [ETHER_ADDR_LEN]));
int	ne_pcmcia_ax88190_set_iobase __P((struct ne_pcmcia_softc *));

struct cfattach ne_pcmcia_ca = {
	sizeof(struct ne_pcmcia_softc), ne_pcmcia_match, ne_pcmcia_attach,
	    ne_pcmcia_detach, dp8390_activate
};

static const struct ne2000dev {
    char *name;
    int32_t manufacturer;
    int32_t product;
    char *cis_info[4];
    int function;
    int enet_maddr;
    unsigned char enet_vendor[3];
    int flags;
#define	NE2000DVF_DL10019	0x0001		/* chip is D-Link DL10019 */
#define	NE2000DVF_AX88190	0x0002		/* chip is ASIX AX88190 */
} ne2000devs[] = {
    { PCMCIA_STR_SYNERGY21_S21810,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SYNERGY21_S21810,
      0, -1, { 0x00, 0x48, 0x54 } },

    { PCMCIA_STR_AMBICOM_AMB8002T,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_AMBICOM_AMB8002T,
      0, -1, { 0x00, 0x10, 0x7a } },

    { PCMCIA_STR_PREMAX_PE200,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PREMAX_PE200,
      0, 0x07f0, { 0x00, 0x20, 0xe0 } },

    { PCMCIA_STR_DIGITAL_DEPCMXX,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DIGITAL_DEPCMXX,
      0, 0x0ff0, { 0x00, 0x00, 0xe8 } },

    { PCMCIA_STR_PLANET_SMARTCOM2000,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PLANET_SMARTCOM2000,
      0, 0xff0, { 0x00, 0x00, 0xe8 } },

    { PCMCIA_STR_DLINK_DE660,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE660,
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_DLINK_DE660PLUS,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE660PLUS,
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_RPTI_EP400,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_RPTI_EP400,
      0, -1, { 0x00, 0x40, 0x95 } },

    { PCMCIA_STR_RPTI_EP401,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_RPTI_EP401,
      0, -1, { 0x00, 0x40, 0x95 } },

    { PCMCIA_STR_ACCTON_EN2212,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_ACCTON_EN2212,
      0, 0x0ff0, { 0x00, 0x00, 0xe8 } },

    { PCMCIA_STR_SVEC_COMBOCARD,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SVEC_COMBOCARD,
      0, -1, { 0x00, 0xe0, 0x98 } },

    { PCMCIA_STR_SVEC_LANCARD,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SVEC_LANCARD,
      0, 0x7f0, { 0x00, 0xc0, 0x6c } },

    { PCMCIA_STR_EPSON_EEN10B,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_EPSON_EEN10B,
      PCMCIA_CIS_EPSON_EEN10B,
      0, 0xff0, { 0x00, 0x00, 0x48 } },

    { PCMCIA_STR_CNET_NE2000,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_CNET_NE2000,
      0, -1, { 0x00, 0x80, 0xad } },

    { PCMCIA_STR_ZONET_ZEN,
      PCMCIA_VENDOR_ZONET, PCMCIA_PRODUCT_ZONET_ZEN,
      PCMCIA_CIS_ZONET_ZEN,
      0, -1, { 0x00, 0x00, 0x00 } },       

    /*
     * You have to add new entries which contains
     * PCMCIA_VENDOR_INVALID and/or PCMCIA_PRODUCT_INVALID 
     * in front of this comment.
     *
     * There are cards which use a generic vendor and product id but needs
     * a different handling depending on the cis_info, so ne2000_match
     * needs a table where the exceptions comes first and then the normal
     * product and vendor entries.
     */

    { PCMCIA_STR_IBM_INFOMOVER,
      PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
      PCMCIA_CIS_IBM_INFOMOVER,
      0, 0x0ff0, { 0x08, 0x00, 0x5a } },

    { PCMCIA_STR_IBM_INFOMOVER,
      PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
      PCMCIA_CIS_IBM_INFOMOVER,
      0, 0x0ff0, { 0x00, 0x04, 0xac } },

    { PCMCIA_STR_IBM_INFOMOVER,
      PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
      PCMCIA_CIS_IBM_INFOMOVER,
      0, 0x0ff0, { 0x00, 0x06, 0x29 } },

    { PCMCIA_STR_LINKSYS_ECARD_1, 
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
      PCMCIA_CIS_LINKSYS_ECARD_1, 
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_PLANEX_FNW3600T,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_PLANEX_FNW3600T,
      0, -1, { 0x00, 0x90, 0xcc }, NE2000DVF_DL10019 },

    { PCMCIA_STR_SVEC_PN650TX,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_SVEC_PN650TX,
      0, -1, { 0x00, 0xe0, 0x98 }, NE2000DVF_DL10019 },

    /*
     * This entry should be here so that above two cards doesn't
     * match with this.  FNW-3700T won't match above entries due to
     * MAC address check.
     */
    { PCMCIA_STR_PLANEX_FNW3700T, 
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_PLANEX_FNW3700T, 
      0, -1, { 0x00, 0x90, 0xcc }, NE2000DVF_AX88190 },

    { PCMCIA_STR_LINKSYS_ETHERFAST,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
      PCMCIA_CIS_LINKSYS_ETHERFAST,
      0, -1, { 0x00, 0x80, 0xc8 }, NE2000DVF_DL10019 },

    { PCMCIA_STR_LINKSYS_ETHERFAST,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
      PCMCIA_CIS_LINKSYS_ETHERFAST,
      0, -1, { 0x00, 0x90, 0xfe }, NE2000DVF_DL10019 },

    { PCMCIA_STR_DLINK_DE650,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
      PCMCIA_CIS_DLINK_DE650,
      0, -1, { 0x00, 0xe0, 0x98 }, NE2000DVF_DL10019 },

    { PCMCIA_STR_MELCO_LPC2_TX,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
      PCMCIA_CIS_MELCO_LPC2_TX,
      0, -1, { 0x00, 0x40, 0x26 }, NE2000DVF_DL10019 },

    { PCMCIA_STR_LINKSYS_COMBO_ECARD, 
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_LINKSYS_COMBO_ECARD, 
      0, -1, { 0x00, 0x80, 0xc8 } },

    { PCMCIA_STR_LINKSYS_TRUST_COMBO_ECARD,
      PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD,
      PCMCIA_CIS_LINKSYS_TRUST_COMBO_ECARD,
      0, 0x0120, { 0x20, 0x04, 0x49 } },

    /* Although the comments above say to put VENDOR/PRODUCT INVALID IDs
       above this list, we need to keep this one below the ECARD_1, or else
       both will match the same more-generic entry rather than the more
       specific one above with proper vendor and product IDs. */
    { PCMCIA_STR_LINKSYS_ECARD_2, 
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_LINKSYS_ECARD_2,
      0, -1, { 0x00, 0x80, 0xc8 } },

    /*
     * D-Link DE-650 has many minor versions:
     *
     *   CIS information          Manufacturer Product  Note
     * 1 "D-Link, DE-650"             INVALID  INVALID  white card
     * 2 "D-Link, DE-650, Ver 01.00"  INVALID  INVALID  became bare metal
     * 3 "D-Link, DE-650, Ver 01.00"   0x149    0x265   minor change in look
     * 4 "D-Link, DE-650, Ver 01.00"   0x149    0x265   collision LED added
     *
     * While the 1st and the 2nd types should use the "D-Link DE-650" entry,
     * the 3rd and the 4th types should use the "Linksys EtherCard" entry.
     * Therefore, this enty must be below the LINKSYS_ECARD_1.  --itohy
     */
    { PCMCIA_STR_DLINK_DE650,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE650,
      0, 0x0040, { 0x00, 0x80, 0xc8 } },

    /*
     * IO-DATA PCLA/TE and later version of PCLA/T has valid
     * vendor/product ID and it is possible to read MAC address
     * using standard I/O ports.  It also read from CIS offset 0x01c0.
     * On the other hand, earlier version of PCLA/T doesn't have valid
     * vendor/product ID and MAC address must be read from CIS offset
     * 0x0ff0 (i.e., usual ne2000 way to read it doesn't work).
     * And CIS information of earlier and later version of PCLA/T are
     * same except fourth element.  So, for now, we place the entry for
     * PCLA/TE (and later version of PCLA/T) followed by entry
     * for the earlier version of PCLA/T (or, modify to match all CIS
     * information and have three or more individual entries).
     */
    { PCMCIA_STR_IODATA_PCLATE,
      PCMCIA_VENDOR_IODATA, PCMCIA_PRODUCT_IODATA_PCLATE,
      PCMCIA_CIS_IODATA_PCLATE,
      0, -1, { 0x00, 0xa0, 0xb0 } },

    /*
     * This entry should be placed after above PCLA-TE entry.
     * See above comments for detail.
     */
    { PCMCIA_STR_IODATA_PCLAT,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_IODATA_PCLAT,
      0, 0x0ff0, { 0x00, 0xa0, 0xb0 } },

    { PCMCIA_STR_DAYNA_COMMUNICARD_E_1,
      PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_1,
      PCMCIA_CIS_DAYNA_COMMUNICARD_E_1,
      0, 0x0110, { 0x00, 0x80, 0x19 } },

    { PCMCIA_STR_DAYNA_COMMUNICARD_E_2,
      PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_2,
      PCMCIA_CIS_DAYNA_COMMUNICARD_E_2,
      0, -1, { 0x00, 0x80, 0x19 } },

    { PCMCIA_STR_COREGA_ETHER_PCC_T,
      PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_ETHER_PCC_T,
      PCMCIA_CIS_COREGA_ETHER_PCC_T,
      0, -1, { 0x00, 0x00, 0xf4 } },

    { PCMCIA_STR_COREGA_ETHER_II_PCC_T,
      PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_ETHER_II_PCC_T,
      PCMCIA_CIS_COREGA_ETHER_II_PCC_T,
      0, -1, { 0x00, 0x00, 0xf4 } },

    { PCMCIA_STR_COREGA_FAST_ETHER_PCC_TX,
      PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_FAST_ETHER_PCC_TX,
      PCMCIA_CIS_COREGA_FAST_ETHER_PCC_TX,
      0, -1, { 0x00, 0x00, 0xf4 }, NE2000DVF_DL10019 },

    { PCMCIA_STR_COREGA_FETHER_PCC_TXF,
      PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_FETHER_PCC_TXF,
      PCMCIA_CIS_COREGA_FETHER_PCC_TXF,
      0, -1, { 0x00, 0x90, 0x99 }, NE2000DVF_DL10019 },

    { PCMCIA_STR_COREGA_FETHER_PCC_TXD,
      PCMCIA_VENDOR_COREGA, PCMCIA_PRODUCT_COREGA_FETHER_PCC_TXD,
      PCMCIA_CIS_COREGA_FETHER_PCC_TXD,
      0, -1, { 0x00, 0x90, 0x99 } },

    { PCMCIA_STR_COMPEX_LINKPORT_ENET_B,
      PCMCIA_VENDOR_COMPEX, PCMCIA_PRODUCT_COMPEX_LINKPORT_ENET_B,
      PCMCIA_CIS_COMPEX_LINKPORT_ENET_B,
      0, 0x01c0, { 0x00, 0xa0, 0x0c } },

    { PCMCIA_STR_SMC_EZCARD,
      PCMCIA_VENDOR_SMC, PCMCIA_PRODUCT_SMC_EZCARD,
      PCMCIA_CIS_SMC_EZCARD,
      0, 0x01c0, { 0x00, 0xe0, 0x29 } },

    { PCMCIA_STR_SOCKET_EA_ETHER,
      PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_EA_ETHER,
      PCMCIA_CIS_SOCKET_EA_ETHER,
      0, -1, { 0x00, 0xc0, 0x1b } },

    { PCMCIA_STR_SOCKET_LP_ETHER_CF,
      PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER_CF,
      PCMCIA_CIS_SOCKET_LP_ETHER_CF,
      0, -1, { 0x00, 0xc0, 0x1b } },

    { PCMCIA_STR_SOCKET_LP_ETHER,
      PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER,
      PCMCIA_CIS_SOCKET_LP_ETHER,
      0, -1, { 0x00, 0xc0, 0x1b } },

    { PCMCIA_STR_KINGSTON_KNE2,
      PCMCIA_VENDOR_KINGSTON, PCMCIA_PRODUCT_KINGSTON_KNE2,
      PCMCIA_CIS_KINGSTON_KNE2,
      0, -1, { 0x00, 0xc0, 0xf0 } },

    { PCMCIA_STR_XIRCOM_CFE_10,
      PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CFE_10,
      PCMCIA_CIS_XIRCOM_CFE_10,
      0, -1, { 0x00, 0x10, 0xa4 } },

    { PCMCIA_STR_MELCO_LPC3_TX, 
      PCMCIA_VENDOR_MELCO, PCMCIA_PRODUCT_MELCO_LPC3_TX,
      PCMCIA_CIS_MELCO_LPC3_TX, 
      0, -1, { 0x00, 0x40, 0x26 }, NE2000DVF_AX88190 },

    { PCMCIA_STR_BILLIONTON_LNT10TN,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_BILLIONTON_LNT10TN,
      0, -1, { 0x00, 0x00, 0x00 } },

    { PCMCIA_STR_NDC_ND5100_E,
      PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_NDC_ND5100_E,
      0, -1, { 0x00, 0x80, 0xc6 } },

    { PCMCIA_STR_TELECOMDEVICE_TCD_HPC100,
      PCMCIA_VENDOR_TELECOMDEVICE, PCMCIA_PRODUCT_TELECOMDEVICE_TCD_HPC100,
      PCMCIA_CIS_TELECOMDEVICE_TCD_HPC100,
      0, -1, { 0x00, 0x40, 0x26 }, NE2000DVF_AX88190 },

    { PCMCIA_STR_MACNICA_ME1_JEIDA,
      PCMCIA_VENDOR_MACNICA, PCMCIA_PRODUCT_MACNICA_ME1_JEIDA,
      PCMCIA_CIS_MACNICA_ME1_JEIDA,
      0, 0x00b8, { 0x08, 0x00, 0x42 } },

#if 0
    /* the rest of these are stolen from the linux pcnet pcmcia device
       driver.  Since I don't know the manfid or cis info strings for
       any of them, they're not compiled in until I do. */
    { "APEX MultiCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x03f4, { 0x00, 0x20, 0xe5 } },
    { "ASANTE FriendlyNet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x4910, { 0x00, 0x00, 0x94 } },
    { "Danpex EN-6200P2",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xc7 } },
    { "DataTrek NetCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xe8 } },
    { "Dayna CommuniCard E",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x80, 0x19 } },
    { "EP-210 Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0x33 } },
    { "ELECOM Laneed LD-CDWA",
      0x0000, 0x0000, NULL, NULL, 0,
      0x00b8, { 0x08, 0x00, 0x42 } },
    { "Grey Cell GCS2220",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x00, 0x47, 0x43 } },
    { "Hypertec Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x01c0, { 0x00, 0x40, 0x4c } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x08, 0x00, 0x5a } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x04, 0xac } },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x06, 0x29 } },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x00, 0x04, 0xac } },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x08, 0x00, 0x5a } },
    { "Katron PE-520",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xf6 } },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xc0, 0xf0 } },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0xe2, 0x0c, 0x0f } },
    { "Longshine LCS-8534",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x08, 0x00, 0x00 } },
    { "Maxtech PCN2000",
      0x0000, 0x0000, NULL, NULL, 0,
      0x5000, { 0x00, 0x00, 0xe8 } },
    { "NDC Instant-Link",
      0x0000, 0x0000, NULL, NULL, 0,
      0x003a, { 0x00, 0x80, 0xc6 } },
    { "NE2000 Compatible",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xa0, 0x0c } },
    { "Network General Sniffer",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x00, 0x65 } },
    { "Panasonic VEL211",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x80, 0x45 } },
    { "SCM Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xcb } },
    { "Volktek NPL-402CT",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0060, { 0x00, 0x40, 0x05 } },
#endif

    { PCMCIA_STR_ALLIEDTELESIS_LA_PCM,
      PCMCIA_VENDOR_ALLIEDTELESIS, PCMCIA_PRODUCT_ALLIEDTELESIS_LA_PCM,
      PCMCIA_CIS_ALLIEDTELESIS_LA_PCM,
      0, 0x0ff0, { 0x00, 0x00, 0xf4 } },
};

#define	NE2000_NDEVS	(sizeof(ne2000devs) / sizeof(ne2000devs[0]))

#define ne2000_match(card, fct, n) \
((((((card)->manufacturer != PCMCIA_VENDOR_INVALID) && \
    ((card)->manufacturer == ne2000devs[(n)].manufacturer) && \
    ((card)->product != PCMCIA_PRODUCT_INVALID) && \
    ((card)->product == ne2000devs[(n)].product)) || \
   ((ne2000devs[(n)].cis_info[0]) && (ne2000devs[(n)].cis_info[1]) && \
    (strcmp((card)->cis1_info[0], ne2000devs[(n)].cis_info[0]) == 0) && \
    (strcmp((card)->cis1_info[1], ne2000devs[(n)].cis_info[1]) == 0))) && \
  ((fct) == ne2000devs[(n)].function))? \
 &ne2000devs[(n)]:NULL)

int
ne_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;
	int i;

	for (i = 0; i < NE2000_NDEVS; i++) {
		if (ne2000_match(pa->card, pa->pf->number, i))
			return (1);
	}

	return (0);
}

void
ne_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ne_pcmcia_softc *psc = (void *) self;
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const struct ne2000dev *ne_dev;
	int i;
	u_int8_t myea[6], *enaddr;
	const char *typestr = "";

	psc->sc_pf = pa->pf;

	for (cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head); cfe != NULL;
	    cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {
#if 0
		/*
		 * Some ne2000 driver's claim to have memory; others don't.
		 * Since I don't care, I don't check.
		 */

		if (cfe->num_memspace != 1) {
			printf(": unexpected number of memory spaces "
			    " %d should be 1\n", cfe->num_memspace);
			continue;
		}
#endif

		if (cfe->num_iospace == 1) {
			if (cfe->iospace[0].length != NE2000_NPORTS) {
				printf(": unexpected I/O space configuration"
				    " (continued)\n%s", dsc->sc_dev.dv_xname);
				/* XXX really safe for all other cards? */
			}
		} else if (cfe->num_iospace == 2) {
			/*
			 * Some cards report a separate space for NIC and ASIC.
			 * This make some sense, but we must allocate a single
			 * NE2000_NPORTS-sized chunk, due to brain damaged
			 * address decoders on some of these cards.
			 */
			if (cfe->iospace[0].length + cfe->iospace[1].length !=
			    NE2000_NPORTS) {
#ifdef DIAGNOSTIC
				printf(": unexpected I/O "
				    "space configuration; ignored\n%s",
				    dsc->sc_dev.dv_xname);
#endif
				continue;
			}
		} else {
#ifdef DIAGNOSTIC
			printf(": unexpected number of i/o spaces %d"
			    " should be 1 or 2; ignored\n%s",
			    cfe->num_iospace, dsc->sc_dev.dv_xname);
#endif
			continue;
		}

		if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
		    NE2000_NPORTS, NE2000_NPORTS, &psc->sc_pcioh)) {
#ifdef DIAGNOSTIC
			printf(": can't allocate i/o space %lx; ignored\n%s",
			    cfe->iospace[0].start, dsc->sc_dev.dv_xname);
#endif
			continue;
		}

		/* Ok, found. */
		break;
	}

	if (cfe == NULL) {
		printf(": no suitable config entry\n");
		goto fail_1;
	}

	dsc->sc_regt = psc->sc_pcioh.iot;
	dsc->sc_regh = psc->sc_pcioh.ioh;

	nsc->sc_asict = psc->sc_pcioh.iot;
	if (bus_space_subregion(dsc->sc_regt, dsc->sc_regh,
	    NE2000_ASIC_OFFSET, NE2000_ASIC_NPORTS,
	    &nsc->sc_asich)) {
		printf(": can't get subregion for asic\n");
		goto fail_2;
	}

	/* Set up power management hooks. */
	dsc->sc_enable = ne_pcmcia_enable;
	dsc->sc_disable = ne_pcmcia_disable;

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		printf(": function enable failed\n");
		goto fail_2;
	}

	/* some cards claim to be io16, but they're lying. */
	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_IO8,
	    NE2000_NIC_OFFSET, NE2000_NIC_NPORTS,
	    &psc->sc_pcioh, &psc->sc_nic_io_window)) {
		printf(": can't map NIC i/o space\n");
		goto fail_3;
	}

	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_IO16,
	    NE2000_ASIC_OFFSET, NE2000_ASIC_NPORTS,
	    &psc->sc_pcioh, &psc->sc_asic_io_window)) {
		printf(": can't map ASIC i/o space\n");
		goto fail_4;
	}

	printf("\n");

	/*
	 * Read the station address from the board.
	 */
	i = 0;
again:
	enaddr = NULL;			/* Ask ASIC by default */
	for (; i < NE2000_NDEVS; i++) {
		ne_dev = ne2000_match(pa->card, pa->pf->number, i);
		if (ne_dev != NULL) {
			if (ne_dev->enet_maddr >= 0) {
				enaddr = ne_pcmcia_get_enaddr(psc, 
				    ne_dev->enet_maddr, myea);
				if (enaddr == NULL)
					continue;
			}
			break;
		}
	}
	if (i == NE2000_NDEVS) {
		printf("%s: can't match ethernet vendor code\n",
		    dsc->sc_dev.dv_xname);
		goto fail_5;
	}

	if ((ne_dev->flags & NE2000DVF_DL10019) != 0) {
		u_int8_t type;

		enaddr = ne_pcmcia_dl10019_get_enaddr(psc, myea);
		if (enaddr == NULL) {
			++i;
			goto again;
		}

		dsc->sc_mediachange = dl10019_mediachange;
		dsc->sc_mediastatus = dl10019_mediastatus;
		dsc->init_card = dl10019_init_card;
		dsc->stop_card = dl10019_stop_card;
		dsc->sc_media_init = dl10019_media_init;
		dsc->sc_media_fini = dl10019_media_fini;

		/* Determine if this is a DL10019 or a DL10022. */
		type = bus_space_read_1(nsc->sc_asict, nsc->sc_asich, 0x0f);
		if (type == 0x91 || type == 0x99) {
			nsc->sc_type = NE2000_TYPE_DL10022;
			typestr = " (DL10022)";
		} else {
			nsc->sc_type = NE2000_TYPE_DL10019;
			typestr = " (DL10019)";
		}
	}

	if ((ne_dev->flags & NE2000DVF_AX88190) != 0) {
		if (ne_pcmcia_ax88190_set_iobase(psc)) {
			++i;
			goto again;
		}

		dsc->sc_mediachange = ax88190_mediachange;
		dsc->sc_mediastatus = ax88190_mediastatus;
		dsc->init_card = ax88190_init_card;
		dsc->stop_card = ax88190_stop_card;
		dsc->sc_media_init = ax88190_media_init;
		dsc->sc_media_fini = ax88190_media_fini;

		nsc->sc_type = NE2000_TYPE_AX88190;
		typestr = " (AX88190)";
	}

	if (enaddr != NULL) {
		/*
		 * Make sure this is what we expect.
		 */
		if (enaddr[0] != ne_dev->enet_vendor[0] ||
		    enaddr[1] != ne_dev->enet_vendor[1] ||
		    enaddr[2] != ne_dev->enet_vendor[2]) {
			++i;
			goto again;
		}
	}

	/*
	 * Check for a RealTek 8019.
	 */
	if (nsc->sc_type == 0) {
		bus_space_write_1(dsc->sc_regt, dsc->sc_regh, ED_P0_CR,
		    ED_CR_PAGE_0 | ED_CR_STP);
		if (bus_space_read_1(dsc->sc_regt, dsc->sc_regh,
		    NERTL_RTL0_8019ID0) == RTL0_8019ID0 &&
		    bus_space_read_1(dsc->sc_regt, dsc->sc_regh,
		    NERTL_RTL0_8019ID1) == RTL0_8019ID1) {
			typestr = " (RTL8019)";
			dsc->sc_mediachange = rtl80x9_mediachange;
			dsc->sc_mediastatus = rtl80x9_mediastatus;
				dsc->init_card = rtl80x9_init_card;
			dsc->sc_media_init = rtl80x9_media_init;
		}
	}

	printf("%s: %s%s Ethernet\n", dsc->sc_dev.dv_xname, ne_dev->name,
	    typestr);

	if (ne2000_attach(nsc, enaddr))
		goto fail_5;

	pcmcia_function_disable(pa->pf);
	return;

 fail_5:
	/* Unmap ASIC i/o windows. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_asic_io_window);

 fail_4:
	/* Unmap NIC i/o windows. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_nic_io_window);

 fail_3:
	pcmcia_function_disable(pa->pf);

 fail_2:
	/* Free our i/o space. */
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

 fail_1:
	psc->sc_nic_io_window = -1;
}

int
ne_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)self;
	int error;

	if (psc->sc_nic_io_window == -1)
		/* Nothing to detach. */
		return (0);

	error = ne2000_detach(&psc->sc_ne2000, flags);
	if (error != 0)
		return (error);

	/* Unmap our i/o windows. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_asic_io_window);
	pcmcia_io_unmap(psc->sc_pf, psc->sc_nic_io_window);

	/* Free our i/o space. */
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

	return (0);
}

int
ne_pcmcia_enable(dsc)
	struct dp8390_softc *dsc;
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;
	struct ne2000_softc *nsc = &psc->sc_ne2000;

	/* set up the interrupt */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, dp8390_intr,
	    dsc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    dsc->sc_dev.dv_xname);
		goto fail_1;
	}

	if (pcmcia_function_enable(psc->sc_pf))
		goto fail_2;

	if (nsc->sc_type == NE2000_TYPE_AX88190)
		if (ne_pcmcia_ax88190_set_iobase(psc))
			goto fail_3;

	return (0);

 fail_3:
	pcmcia_function_disable(psc->sc_pf);
 fail_2:
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
 fail_1:
	return (1);
}

void
ne_pcmcia_disable(dsc)
	struct dp8390_softc *dsc;
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
}

u_int8_t *
ne_pcmcia_get_enaddr(psc, maddr, myea)
	struct ne_pcmcia_softc *psc;
	int maddr;
	u_int8_t myea[ETHER_ADDR_LEN];
{
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_mem_handle pcmh;
	bus_addr_t offset;
	u_int8_t *enaddr = NULL;
	int j, mwindow;

	if (maddr < 0)
		return (NULL);

	if (pcmcia_mem_alloc(psc->sc_pf, ETHER_ADDR_LEN * 2, &pcmh)) {
		printf("%s: can't alloc mem for enet addr\n",
		    dsc->sc_dev.dv_xname);
		goto fail_1;
	}
	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR, maddr,
	    ETHER_ADDR_LEN * 2, &pcmh, &offset, &mwindow)) {
		printf("%s: can't map mem for enet addr\n",
		    dsc->sc_dev.dv_xname);
		goto fail_2;
	}
	for (j = 0; j < ETHER_ADDR_LEN; j++)
		myea[j] = bus_space_read_1(pcmh.memt, pcmh.memh,
		    offset + (j * 2));
	enaddr = myea;

	pcmcia_mem_unmap(psc->sc_pf, mwindow);
 fail_2:
	pcmcia_mem_free(psc->sc_pf, &pcmh);
 fail_1:
	return (enaddr);
}

u_int8_t *
ne_pcmcia_dl10019_get_enaddr(psc, myea)
	struct ne_pcmcia_softc *psc;
	u_int8_t myea[ETHER_ADDR_LEN];
{
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	u_int8_t sum;
	int j;

#define PAR0	0x04
	for (j = 0, sum = 0; j < 8; j++)
		sum += bus_space_read_1(nsc->sc_asict, nsc->sc_asich,
		    PAR0 + j);
	if (sum != 0xff)
		return (NULL);
	for (j = 0; j < ETHER_ADDR_LEN; j++)
		myea[j] = bus_space_read_1(nsc->sc_asict,
		    nsc->sc_asich, PAR0 + j);
#undef PAR0
	return (myea);
}

int
ne_pcmcia_ax88190_set_iobase(psc)
	struct ne_pcmcia_softc *psc;
{
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_mem_handle pcmh;
	bus_addr_t offset;
	int rv = 1, mwindow;
	u_int last_liobase, new_liobase;

	if (pcmcia_mem_alloc(psc->sc_pf, AX88190_LAN_IOSIZE, &pcmh)) {
#if 0
		printf("%s: can't alloc mem for LAN iobase\n",
		    dsc->sc_dev.dv_xname);
#endif
		goto fail_1;
	}
	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR,
	    AX88190_LAN_IOBASE, AX88190_LAN_IOSIZE,
	    &pcmh, &offset, &mwindow)) {
		printf("%s: can't map mem for LAN iobase\n",
		    dsc->sc_dev.dv_xname);
		goto fail_2;
	}

	last_liobase = bus_space_read_1(pcmh.memt, pcmh.memh, offset + 0) |
	    (bus_space_read_1(pcmh.memt, pcmh.memh, offset + 2) << 8);
#ifdef DIAGNOSTIC
	printf("%s: LAN iobase 0x%x (0x%x) ->", dsc->sc_dev.dv_xname,
	    last_liobase, (u_int)psc->sc_pcioh.addr);
#endif
	bus_space_write_1(pcmh.memt, pcmh.memh, offset,
	    psc->sc_pcioh.addr & 0xff);
	bus_space_write_1(pcmh.memt, pcmh.memh, offset + 2,
	    psc->sc_pcioh.addr >> 8);

	new_liobase = bus_space_read_1(pcmh.memt, pcmh.memh, offset + 0) |
	    (bus_space_read_1(pcmh.memt, pcmh.memh, offset + 2) << 8);
#ifdef DIAGNOSTIC
	printf(" 0x%x\n", new_liobase);
#endif
	if ((last_liobase == psc->sc_pcioh.addr)
	    || (last_liobase != new_liobase))
		rv = 0;

	pcmcia_mem_unmap(psc->sc_pf, mwindow);
 fail_2:
	pcmcia_mem_free(psc->sc_pf, &pcmh);
 fail_1:
	return (rv);
}
