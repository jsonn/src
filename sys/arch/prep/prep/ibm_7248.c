/*	$NetBSD: ibm_7248.c,v 1.6.6.1 2004/08/03 10:39:48 skrll Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by NONAKA Kimihiro.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ibm_7248.c,v 1.6.6.1 2004/08/03 10:39:48 skrll Exp $");

#include <sys/param.h>

#include <machine/intr.h>
#include <machine/platform.h>

static void pci_intr_fixup_ibm_7248(int, int, int *);

struct platform platform_ibm_7248 = {
	"IBM PPS Model 7248 (E)",		/* model */
	platform_generic_match,			/* match */
	prep_pci_get_chipset_tag_indirect,	/* pci_get_chipset_tag */
	pci_intr_fixup_ibm_7248,		/* pci_intr_fixup */
	init_intr_ivr,				/* init_intr */
	cpu_setup_ibm_generic,			/* cpu_setup */
	reset_prep_generic,			/* reset */
	obiodevs_nodev,				/* obiodevs */
};

static void
pci_intr_fixup_ibm_7248(int bus, int dev, int *line)
{
	if (bus != 0)
		return;

	switch (dev) {
	case 12:
	case 13:
	case 16:
	case 18:
	case 22:
		*line = 15;
		break;
	}
}

