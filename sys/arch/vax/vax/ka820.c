/*	$NetBSD: ka820.c,v 1.14.4.1 1999/06/21 01:03:46 thorpej Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)ka820.c	7.4 (Berkeley) 12/16/90
 */

/*
 * KA820 specific CPU code.  (Note that the VAX8200 uses a KA820, not
 * a KA8200.  Sigh.)
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <vm/vm.h> 
#include <vm/vm_kern.h>

#include <machine/ka820.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/clock.h>
#include <machine/scb.h>

#include <arch/vax/bi/bireg.h>
#include <arch/vax/bi/bivar.h>

#include <vax/vax/crx.h>

#include "locators.h"

struct ka820port *ka820port_ptr;
struct rx50device *rx50device_ptr;

static	int ka820_match __P((struct device *, struct cfdata *, void *));
static	void ka820_attach __P((struct device *, struct device *, void*));
static	void rxcdintr __P((int));
void crxintr __P((int));

struct	cpu_dep ka820_calls = {
	0,
	ka820_mchk,
	ka820_memerr,
	NULL,
	chip_clkread,
	chip_clkwrite,
	3,      /* ~VUPS */
	5,	/* SCB pages */
};

struct cfattach cpu_bi_ca = {
	sizeof(struct device), ka820_match, ka820_attach
};

#ifdef notyet
extern struct pte BRAMmap[];
extern struct pte EEPROMmap[];
char bootram[KA820_BRPAGES * VAX_NBPG];
char eeprom[KA820_EEPAGES * VAX_NBPG];
#endif

int
ka820_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void	*aux;
{
	struct bi_attach_args *ba = aux;

	if (ba->ba_node->biic.bi_dtype != BIDT_KA820)
		return 0;

	if (ba->ba_nodenr != mastercpu)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

void
ka820_attach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct bi_attach_args *ba = aux;
	register int csr;
	extern	short *clk_page;
	extern	int clk_adrshift, clk_tweak;
	u_short rev = ba->ba_node->biic.bi_revs;
	extern	char cpu_model[];

	strcpy(cpu_model,"VAX 8200");
	cpu_model[6] = rev & 0x8000 ? '5' : '0';
	printf(": ka82%c (%s) cpu rev %d, u patch rev %d, sec patch %d\n",
	    cpu_model[6], mastercpu == ba->ba_nodenr ? "master" : "slave",
	    ((rev >> 11) & 15), ((rev >> 1) &1023), rev & 1);

	/* reset the console and enable the RX50 */
	csr = ka820port_ptr->csr;
	csr &= ~KA820PORT_RSTHALT;	/* ??? */
	csr |= KA820PORT_CONSCLR | KA820PORT_CRDCLR | KA820PORT_CONSEN |
		KA820PORT_RXIE;
	ka820port_ptr->csr = csr;
	ba->ba_node->biic.bi_intrdes = ba->ba_intcpu;
	ba->ba_node->biic.bi_csr |= BICSR_SEIE | BICSR_HEIE;

	/* XXX - should be done somewhere else */
	scb_vecalloc(SCB_RX50, crxintr, 0, SCB_ISTACK);

	clk_adrshift = 0;	/* clk regs are addressed at short's */
	clk_tweak = 1; 		/* ...but not exactly in each short */
	clk_page = (short *)vax_map_physmem((paddr_t)KA820_CLOCKADDR, 1);

	/* Steal the interrupt vectors that are unique for us */
	scb_vecalloc(KA820_INT_RXCD, rxcdintr, 0, SCB_ISTACK);

	rx50device_ptr = (void *)vax_map_physmem(KA820_RX50ADDR, 1);
	ka820port_ptr = (void *)vax_map_physmem(KA820_PORTADDR, 1);
}

/*
 * MS820 support.
 */
struct ms820regs {
	struct	biiregs biic;		/* BI interface chip */
	u_long	ms_gpr[4];		/* the four gprs (unused) */
	int	ms_csr1;		/* control/status register 1 */
	int	ms_csr2;		/* control/status register 2 */
};

/*
 * Bits in CSR1.
 */
#define MS1_ERRSUM	0x80000000	/* error summary (ro) */
#define MS1_ECCDIAG	0x40000000	/* ecc diagnostic (rw) */
#define MS1_ECCDISABLE	0x20000000	/* ecc disable (rw) */
#define MS1_MSIZEMASK	0x1ffc0000	/* mask for memory size (ro) */
#define MS1_RAMTYMASK	0x00030000	/* mask for ram type (ro) */
#define MS1_RAMTY64K	0x00000000	/* 64K chips */
#define MS1_RAMTY256K	0x00010000	/* 256K chips */
#define MS1_RAMTY1MB	0x00020000	/* 1MB chips */
					/* type 3 reserved */
#define MS1_CRDINH	0x00008000	/* inhibit crd interrupts (rw) */
#define MS1_MEMVALID	0x00004000	/* memory has been written (ro) */
#define MS1_INTLK	0x00002000	/* interlock flag (ro) */
#define MS1_BROKE	0x00001000	/* broken (rw) */
#define MS1_MBZ		0x00000880	/* zero */
#define MS1_MWRITEERR	0x00000400	/* rds during masked write (rw) */
#define MS1_CNTLERR	0x00000200	/* internal timing busted (rw) */
#define MS1_INTLV	0x00000100	/* internally interleaved (ro) */
#define MS1_DIAGC	0x0000007f	/* ecc diagnostic bits (rw) */

/*
 * Bits in CSR2.
 */
#define MS2_RDSERR	0x80000000	/* rds error (rw) */
#define MS2_HIERR	0x40000000	/* high error rate (rw) */
#define MS2_CRDERR	0x20000000	/* crd error (rw) */
#define MS2_ADRSERR	0x10000000	/* rds due to addr par err (rw) */
#define MS2_MBZ		0x0f000080	/* zero */
#define MS2_ADDR	0x00fffe00	/* address in error (relative) (ro) */
#define MS2_INTLVADDR	0x00000100	/* error was in bank 1 (ro) */
#define MS2_SYN		0x0000007f	/* error syndrome (ro, rw diag) */

static int ms820_match __P((struct device *, struct cfdata *, void *));
static void ms820_attach __P((struct device *, struct device *, void*));

struct mem_bi_softc {
	struct device mem_dev;
	struct ms820regs *mem_regs;
};

struct cfattach mem_bi_ca = {
	sizeof(struct mem_bi_softc), ms820_match, ms820_attach
};

static int
ms820_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	struct bi_attach_args *ba = aux;

	if (ba->ba_node->biic.bi_dtype != BIDT_MS820)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

static void
ms820_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct mem_bi_softc *ms = (void *)self;
	struct bi_attach_args *ba = aux;

	ms->mem_regs = (void *)ba->ba_node;

	if ((ms->mem_regs->biic.bi_csr & BICSR_STS) == 0)
		printf(": failed self test\n");
	else
		printf(": size %dMB, %s chips\n", ((ms->mem_regs->ms_csr1 & 
		    MS1_MSIZEMASK) >> 20), (ms->mem_regs->ms_csr1&MS1_RAMTYMASK
		    ?ms->mem_regs->ms_csr1 & MS1_RAMTY256K?"256K":"1M":"64K"));

	ms->mem_regs->biic.bi_intrdes = ba->ba_intcpu;
	ms->mem_regs->biic.bi_csr |= BICSR_SEIE | BICSR_HEIE;

	ms->mem_regs->ms_csr1 = MS1_MWRITEERR | MS1_CNTLERR;
	ms->mem_regs->ms_csr2 = MS2_RDSERR | MS2_HIERR |
	    MS2_CRDERR | MS2_ADRSERR;
}

void
ka820_memerr()
{
	extern struct cfdriver mem_cd;
	register struct ms820regs *mcr;
	struct mem_bi_softc *mc;
	register int m, hard;
	register char *type;
static char b1[] = "\20\40ERRSUM\37ECCDIAG\36ECCDISABLE\20CRDINH\17VALID\
\16INTLK\15BROKE\13MWRITEERR\12CNTLERR\11INTLV";
static char b2[] = "\20\40RDS\37HIERR\36CRD\35ADRS";

	for (m = 0; m < mem_cd.cd_ndevs; m++) {
		mc = mem_cd.cd_devs[m];
		if (mc == NULL)
			continue;
		mcr = mc->mem_regs;
		printf("%s: csr1=%b csr2=%b\n", mc->mem_dev.dv_xname,
		    mcr->ms_csr1, b1, mcr->ms_csr2, b2);
		if ((mcr->ms_csr1 & MS1_ERRSUM) == 0)
			continue;
		hard = 1;
		if (mcr->ms_csr1 & MS1_BROKE)
			type = "broke";
		else if (mcr->ms_csr1 & MS1_CNTLERR)
			type = "cntl err";
		else if (mcr->ms_csr2 & MS2_ADRSERR)
			type = "address parity err";
		else if (mcr->ms_csr2 & MS2_RDSERR)
			type = "rds err";
		else if (mcr->ms_csr2 & MS2_CRDERR) {
			hard = 0;
			type = "";
		} else
			type = "mysterious error";
		printf("%s: %s%s%s addr %lx bank %x syn %x\n",
		    mc->mem_dev.dv_xname,
		    hard ? "hard error: " : "soft ecc",
		    type, mcr->ms_csr2 & MS2_HIERR ?
		    " (+ other rds or crd err)" : "",
		    ((mcr->ms_csr2 & MS2_ADDR) + mcr->biic.bi_sadr) >> 9,
		    (mcr->ms_csr2 & MS2_INTLVADDR) != 0,
		    mcr->ms_csr2 & MS2_SYN);
		mcr->ms_csr1 = mcr->ms_csr1 | MS1_CRDINH;
		mcr->ms_csr2 = mcr->ms_csr2;
	}
}

/* these are bits 0 to 6 in the summary field */
char *mc8200[] = {
	"cpu bad ipl",		"ucode lost err",
	"ucode par err",	"DAL par err",
	"BI bus err",		"BTB tag par",
	"cache tag par",
};
#define MC8200_BADIPL	0x01
#define MC8200_UERR	0x02
#define MC8200_UPAR	0x04
#define MC8200_DPAR	0x08
#define MC8200_BIERR	0x10
#define MC8200_BTAGPAR	0x20
#define MC8200_CTAGPAR	0x40

struct mc8200frame {
	int	mc82_bcnt;		/* byte count == 0x20 */
	int	mc82_summary;		/* summary parameter */
	int	mc82_param1;		/* parameter 1 */
	int	mc82_va;		/* va register */
	int	mc82_vap;		/* va prime register */
	int	mc82_ma;		/* memory address */
	int	mc82_status;		/* status word */
	int	mc82_epc;		/* error pc */
	int	mc82_upc;		/* micro pc */
	int	mc82_pc;		/* current pc */
	int	mc82_psl;		/* current psl */
};

int
ka820_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc8200frame *mcf = (struct mc8200frame *)cmcf;
	register int i, type = mcf->mc82_summary;

	/* ignore BI bus errors during configuration */
	if (cold && type == MC8200_BIERR) {
		mtpr(PR_MCESR, 0xf);
		return (MCHK_RECOVERED);
	}

	/*
	 * SOME ERRORS ARE RECOVERABLE
	 * do it later
	 */
	printf("machine check %x: ", type);
	for (i = 0; i < sizeof (mc8200) / sizeof (mc8200[0]); i++)
		if (type & (1 << i))
			printf(" %s,", mc8200[i]);
	printf(" param1 %x\n", mcf->mc82_param1);
	printf(
"\tva %x va' %x ma %x pc %x psl %x\n\tstatus %x errpc %x upc %x\n",
		mcf->mc82_va, mcf->mc82_vap, mcf->mc82_ma,
		mcf->mc82_pc, mcf->mc82_psl,
		mcf->mc82_status, mcf->mc82_epc, mcf->mc82_upc);
	return (MCHK_PANIC);
}

/*
 * Receive a character from logical console.
 */
void
rxcdintr(arg)
	int arg;
{
	register int c = mfpr(PR_RXCD);

	/* not sure what (if anything) to do with these */
	printf("rxcd node %x c=0x%x\n", (c >> 8) & 0xf, c & 0xff);
}
