/*	$NetBSD: pcmcia_cis_quirks.c,v 1.8.2.4 2002/01/08 00:31:27 nathanw Exp $	*/

/*
 * Copyright (c) 1998 Marc Horowitz.  All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcmcia_cis_quirks.c,v 1.8.2.4 2002/01/08 00:31:27 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>

#include <dev/pcmcia/pcmciadevs.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/pcmcia/pcmciavar.h>

/* There are cards out there whose CIS flat-out lies.  This file
   contains struct pcmcia_function chains for those devices. */

/* these structures are just static templates which are then copied
   into "live" allocated structures */

static const struct pcmcia_function pcmcia_dlink_de650_func0 = {
	0,
	PCMCIA_FUNCTION_NETWORK,
	0x23,
	0x400,
	0x0b,
};

static const struct pcmcia_config_entry pcmcia_dlink_de650_func0_cfe0 = {
	0x20,
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,
	5,
	{ { 0x20, 0x360 } },
	0xbe7c,
	0,
	{ { 0 } },
	0,
};

static const struct pcmcia_config_entry pcmcia_dlink_de650_func0_cfe1 = {
	0x21,
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,
	5,
	{ { 0x20, 0x340 } },
	0xbe7c,
	0,
	{ { 0 } },
	0,
};

static const struct pcmcia_config_entry pcmcia_dlink_de650_func0_cfe2 = {
	0x22,
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,
	5,
	{ { 0x20, 0x320 } },
	0xbe7c,
	0,
	{ { 0 } },
	0,
};

static const struct pcmcia_config_entry pcmcia_dlink_de650_func0_cfe3 = {
	0x23,
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,
	5,
	{ { 0x20, 0x300 } },
	0xbe7c,
	0,
	{ { 0 } },
	0,
};

static const struct pcmcia_function pcmcia_3cxem556_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x07,			/* last cfe number */
	0x800,			/* ccr_base */
	0x63,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3cxem556_func0_cfe0 = {
	0x07,			/* cfe number */
	PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	4,			/* iomask */
	{ { 0x0010, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_3cxem556_func1 = {
	1,			/* function number */
	PCMCIA_FUNCTION_SERIAL,
	0x27,			/* last cfe number */
	0x900,			/* ccr_base */
	0x63,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3cxem556_func1_cfe0 = {
	0x27,			/* cfe number */
	PCMCIA_CFE_IO8 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	3,			/* iomask */
	{ { 0x0008, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_3ccfem556bi_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x07,			/* last cfe number */
	0x1000,			/* ccr_base */
	0x267,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3ccfem556bi_func0_cfe0 = {
	0x07,			/* cfe number */
	PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x0020, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_3ccfem556bi_func1 = {
	1,			/* function number */
	PCMCIA_FUNCTION_SERIAL,
	0x27,			/* last cfe number */
	0x1100,			/* ccr_base */
	0x277,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3ccfem556bi_func1_cfe0 = {
	0x27,			/* cfe number */
	PCMCIA_CFE_IO8 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	3,			/* iomask */
	{ { 0x0008, 0 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_sveclancard_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x1,			/* last cfe number */
	0x100,			/* ccr_base */
	0x1,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_sveclancard_func0_cfe0 = {
	0x1,			/* cfe number */
	PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_RDYBSY_ACTIVE |
	PCMCIA_CFE_WP_ACTIVE | PCMCIA_CFE_BVD_ACTIVE | PCMCIA_CFE_IO16,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x20, 0x300 } },	/* iospace */
	0xdeb8,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_ndc_nd5100_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x23,			/* last cfe number */
	0x3f8,			/* ccr_base */
	0x3,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_ndc_nd5100_func0_cfe0 = {
	0x20,			/* cfe number */
	PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	5,			/* iomask */
	{ { 0x20, 0x300 } },	/* iospace */
	0xdeb8,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_emtac_a2424i_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x21,			/* last cfe number */
	0x3e0,			/* ccr_base */
	0x1,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_emtac_a2424i_func0_cfe0 = {
	0x21,			/* cfe number */
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL | PCMCIA_CFE_IRQPULSE,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	6,			/* iomask */
	{ { 0x40, 0x100 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_function pcmcia_fujitsu_j181_func0 = {
	0,			/* function number */
	PCMCIA_FUNCTION_NETWORK,
	0x21,			/* last cfe number */
	0xfe0,			/* ccr_base */
	0xf,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_fujitsu_j181_func0_cfe0 = {
	0xc,			/* cfe number */
	PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_WP_ACTIVE | PCMCIA_CFE_IO8 |
	PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL | PCMCIA_CFE_IRQPULSE,
	PCMCIA_IFTYPE_IO,
	1,			/* num_iospace */
	10,			/* iomask */
	{ { 0x20, 0x140 } },	/* iospace */
	0xffff,			/* irqmask */
	0,			/* num_memspace */
	{ { 0 } },		/* memspace */
	0,			/* maxtwins */
};

static const struct pcmcia_cis_quirk pcmcia_cis_quirks[] = {
	{ PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_dlink_de650_func0, &pcmcia_dlink_de650_func0_cfe0 },
	{ PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_dlink_de650_func0, &pcmcia_dlink_de650_func0_cfe1 },
	{ PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_dlink_de650_func0, &pcmcia_dlink_de650_func0_cfe2 },
	{ PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_dlink_de650_func0, &pcmcia_dlink_de650_func0_cfe3 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556, PCMCIA_CIS_INVALID, 
	  &pcmcia_3cxem556_func0, &pcmcia_3cxem556_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556, PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func1, &pcmcia_3cxem556_func1_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT, PCMCIA_CIS_INVALID, 
	  &pcmcia_3cxem556_func0, &pcmcia_3cxem556_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT, PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func1, &pcmcia_3cxem556_func1_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3ccfem556bi_func0, &pcmcia_3ccfem556bi_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3ccfem556bi_func1, &pcmcia_3ccfem556bi_func1_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID, PCMCIA_CIS_SVEC_LANCARD,
	  &pcmcia_sveclancard_func0, &pcmcia_sveclancard_func0_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID, PCMCIA_CIS_NDC_ND5100_E,
	  &pcmcia_ndc_nd5100_func0, &pcmcia_ndc_nd5100_func0_cfe0 },
	{ PCMCIA_VENDOR_EMTAC, PCMCIA_PRODUCT_EMTAC_WLAN, PCMCIA_CIS_INVALID,
	  &pcmcia_emtac_a2424i_func0, &pcmcia_emtac_a2424i_func0_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_FUJITSU_FMV_J181,
	  &pcmcia_fujitsu_j181_func0, &pcmcia_fujitsu_j181_func0_cfe0 },
};
	
static int n_pcmcia_cis_quirks =
	sizeof(pcmcia_cis_quirks)/sizeof(pcmcia_cis_quirks[0]);

void pcmcia_check_cis_quirks(sc)
	struct pcmcia_softc *sc;
{
	int wiped = 0;
	int i, j;
	struct pcmcia_function *pf, *pf_next;
	const struct pcmcia_function *pf_last;
	struct pcmcia_config_entry *cfe, *cfe_next;

	pf = NULL;
	pf_last = NULL;

	for (i=0; i<n_pcmcia_cis_quirks; i++) {
		if ((sc->card.manufacturer == pcmcia_cis_quirks[i].manufacturer) &&
			(sc->card.product == pcmcia_cis_quirks[i].product) &&
			(((sc->card.manufacturer != PCMCIA_VENDOR_INVALID) &&
			  (sc->card.product != PCMCIA_PRODUCT_INVALID)) ||
			 ((sc->card.manufacturer == PCMCIA_VENDOR_INVALID) &&
			  (sc->card.product == PCMCIA_PRODUCT_INVALID) &&
			  sc->card.cis1_info[0] &&
			  (strcmp(sc->card.cis1_info[0],
					  pcmcia_cis_quirks[i].cis1_info[0]) == 0) &&
			  sc->card.cis1_info[1] &&
			  (strcmp(sc->card.cis1_info[1],
					  pcmcia_cis_quirks[i].cis1_info[1]) == 0)))) {
			if (!wiped) {
				if (pcmcia_verbose) {
					printf("%s: using CIS quirks for ", sc->dev.dv_xname);
					for (j = 0; j < 4; j++) {
						if (sc->card.cis1_info[j] == NULL)
							break;
						if (j)
							printf(", ");
						printf("%s", sc->card.cis1_info[j]);
					}
					printf("\n");
				}

				for (pf = SIMPLEQ_FIRST(&sc->card.pf_head); pf != NULL;
				     pf = pf_next) {
					for (cfe = SIMPLEQ_FIRST(&pf->cfe_head); cfe != NULL;
					     cfe = cfe_next) {
						cfe_next = SIMPLEQ_NEXT(cfe, cfe_list);
						free(cfe, M_DEVBUF);
					}
					pf_next = SIMPLEQ_NEXT(pf, pf_list);
					free(pf, M_DEVBUF);
				}

				SIMPLEQ_INIT(&sc->card.pf_head);
				wiped = 1;
			}

			if (pf_last == pcmcia_cis_quirks[i].pf) {
				cfe = malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);
				*cfe = *pcmcia_cis_quirks[i].cfe;

				SIMPLEQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);
			} else {
				pf = malloc(sizeof(*pf), M_DEVBUF, M_NOWAIT);
				*pf = *pcmcia_cis_quirks[i].pf;
				SIMPLEQ_INIT(&pf->cfe_head);

				cfe = malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);
				*cfe = *pcmcia_cis_quirks[i].cfe;

				SIMPLEQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);
				SIMPLEQ_INSERT_TAIL(&sc->card.pf_head, pf, pf_list);

				pf_last = pcmcia_cis_quirks[i].pf;
			}
		}
	}
}
