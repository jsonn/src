/*	$NetBSD: sbi.c,v 1.28.2.3 2004/09/21 13:23:59 skrll Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Still to do: Write all SBI error handling.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sbi.c,v 1.28.2.3 2004/09/21 13:23:59 skrll Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/nexus.h>

static	int sbi_print(void *, const char *);
static	int sbi_match(struct device *, struct cfdata *, void *);
static	int sbi_match_abus(struct device *, struct cfdata *, void *);
static	void sbi_attach(struct device *, struct device *, void*);

int
sbi_print(void *aux, const char *name)
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	int unsupp = 0;

	if (name) {
		switch (sa->sa_type) {
		case NEX_MBA:
			aprint_normal("mba at %s", name);
			break;
		case NEX_CI:
			aprint_normal("ci at %s", name);
			unsupp++;
			break;
		default:
			aprint_normal("unknown device 0x%x at %s",
			    sa->sa_type, name);
			unsupp++;
		}		
	}
	aprint_normal(" tr%d", sa->sa_nexnum);
	return (unsupp ? UNSUPP : UNCONF);
}

int
sbi_match_abus(struct device *parent, struct cfdata *cf, void *aux)
{
	return 1;	/* We've already done the matching... */
}

int
sbi_match(struct device *parent, struct cfdata *cf, void *aux)
{
	if (vax_bustype == VAX_SBIBUS)
		return 1;
	return 0;
}

void
sbi_attach(struct device *parent, struct device *self, void *aux)
{
	struct bp_conf *bp = aux;
	u_int	nexnum, minnex = 0; /* default only one SBI, as on 780 */
	struct	sbi_attach_args sa;

	printf("\n");

#define NEXPAGES (sizeof(struct nexus) / VAX_NBPG)
	if (vax_boardtype == VAX_BTYP_780) {
		sa.sa_sbinum = 0;
	}
	if (vax_boardtype == VAX_BTYP_790) {
		minnex = bp->num * NNEXSBI;
		sa.sa_sbinum = bp->num;
	}
	for (nexnum = minnex; nexnum < minnex + NNEXSBI; nexnum++) {
		struct	nexus *nexusP = 0;
		volatile int tmp;

		nexusP = (struct nexus *)vax_map_physmem((paddr_t)NEXA8600 +
		    sizeof(struct nexus) * nexnum, NEXPAGES);
		if (badaddr((caddr_t)nexusP, 4)) {
			vax_unmap_physmem((vaddr_t)nexusP, NEXPAGES);
		} else {
			tmp = nexusP->nexcsr.nex_csr; /* no byte reads */
			sa.sa_type = tmp & 255;

			sa.sa_nexnum = nexnum;
			sa.sa_ioh = (vaddr_t)nexusP;
			config_found(self, (void*)&sa, sbi_print);
		}
	}
}

CFATTACH_DECL(sbi_mainbus, sizeof(struct device),
    sbi_match, sbi_attach, NULL, NULL);

CFATTACH_DECL(sbi_abus, sizeof(struct device),
    sbi_match_abus, sbi_attach, NULL, NULL);
