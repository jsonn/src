/*	$NetBSD: dz_vsbus.c,v 1.20.2.3 2002/10/10 18:37:25 jdolecek Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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



#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <dev/cons.h>

#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/uvax.h>
#include <machine/vsbus.h>
#include <machine/cpu.h>
#include <machine/scb.h>

#include <arch/vax/vax/gencons.h>

#include <dev/dec/dzreg.h>
#include <dev/dec/dzvar.h>

#include "ioconf.h"
#include "dzkbd.h"
#include "dzms.h"
#include "opt_cputype.h"

#if NDZKBD > 0 || NDZMS > 0
#include <dev/dec/dzkbdvar.h>

#if 0
static	struct dz_linestate dz_conslinestate = { NULL, -1, NULL, NULL, NULL };
#endif
#endif

static  int     dz_vsbus_match(struct device *, struct cfdata *, void *);
static  void    dz_vsbus_attach(struct device *, struct device *, void *);

static	vaddr_t dz_regs; /* Used for console */

CFATTACH_DECL(dz_vsbus, sizeof(struct dz_softc),
    dz_vsbus_match, dz_vsbus_attach, NULL, NULL);

#define REG(name)     short name; short X##name##X;
static volatile struct ss_dz {/* base address of DZ-controller: 0x200A0000 */
	REG(csr);	/* 00 Csr: control/status register */
	REG(rbuf);	/* 04 Rbuf/Lpr: receive buffer/line param reg. */
	REG(tcr);	/* 08 Tcr: transmit console register */
	REG(tdr);	/* 0C Msr/Tdr: modem status reg/transmit data reg */
	REG(lpr0);	/* 10 Lpr0: */
	REG(lpr1);	/* 14 Lpr0: */
	REG(lpr2);	/* 18 Lpr0: */
	REG(lpr3);	/* 1C Lpr0: */
} *dz;
#undef REG

cons_decl(dz);

#if NDZKBD > 0 || NDZMS > 0
static int
dz_print(void *aux, const char *name)
{
#if 0
#if NDZKBD > 0 || NDZMS > 0
	struct dz_attach_args *dz_args = aux;
	if (name == NULL) {
		printf (" line %d", dz_args->line);
		if (dz_args->hwflags & DZ_HWFLAG_CONSOLE)
			printf (" (console)");
	}
	return (QUIET);
#else
	if (name)
		printf ("lkc at %s", name);
	return (UNCONF);
#endif
#endif
	return (UNCONF);
}
#endif

static int
dz_vsbus_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct vsbus_attach_args *va = aux;
	struct ss_dz *dzP;
	short i;

#if VAX53 || VAX49 || VAXANY
	if (vax_boardtype == VAX_BTYP_53 || vax_boardtype == VAX_BTYP_49)
		if (cf->cf_loc[0] != 0x25000000)
			return 0; /* Ugly */
#endif

	dzP = (struct ss_dz *)va->va_addr;
	i = dzP->tcr;
	dzP->csr = DZ_CSR_MSE|DZ_CSR_TXIE;
	dzP->tcr = 0;
	DELAY(1000);
	dzP->tcr = 1;
	DELAY(100000);
	dzP->tcr = i;

	/* If the device doesn't exist, no interrupt has been generated */
	return 1;
}

static void
dz_vsbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct dz_softc *sc = (void *)self;
	struct vsbus_attach_args *va = aux;
#if NDZKBD > 0
	extern const struct cdevsw dz_cdevsw;
#endif
#if NDZKBD > 0 || NDZMS > 0
	struct dzkm_attach_args daa;
#endif
	int s, consline;

	/* 
	 * XXX - This is evil and ugly, but...
	 * due to the nature of how bus_space_* works on VAX, this will
	 * be perfectly good until everything is converted.
	 */
	if (dz_regs == 0) /* This isn't console */ {
		dz_regs = vax_map_physmem(va->va_paddr, 1);
		consline = -1;
	} else
		consline = minor(cn_tab->cn_dev);
	sc->sc_ioh = dz_regs;
	sc->sc_dr.dr_csr = 0;
	sc->sc_dr.dr_rbuf = 4;
	sc->sc_dr.dr_dtr = 9;
	sc->sc_dr.dr_break = 13;
	sc->sc_dr.dr_tbuf = 12;
	sc->sc_dr.dr_tcr = 8;
	sc->sc_dr.dr_dcd = 13;
	sc->sc_dr.dr_ring = 13;

	sc->sc_type = DZ_DZV;

	sc->sc_dsr = 0x0f; /* XXX check if VS has modem ctrl bits */

	scb_vecalloc(va->va_cvec, dzxint, sc, SCB_ISTACK, &sc->sc_tintrcnt);
	scb_vecalloc(va->va_cvec - 4, dzrint, sc, SCB_ISTACK, &sc->sc_rintrcnt);

	printf("\n%s: 4 lines", self->dv_xname);

	dzattach(sc, NULL, consline);
	DELAY(10000);

#if NDZKBD > 0
	/* Don't touch this port if this is the console */
	if (cn_tab->cn_dev != makedev(cdevsw_lookup_major(&dz_cdevsw), 0)) {
		dz->rbuf = DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) 
		    | DZ_LPR_8_BIT_CHAR;
		daa.daa_line = 0;
		daa.daa_flags =
		    (cn_tab->cn_pri == CN_INTERNAL ? DZKBD_CONSOLE : 0);
		config_found(self, &daa, dz_print);
	}
#endif
#if NDZMS > 0
	dz->rbuf = DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) | DZ_LPR_8_BIT_CHAR \
	    | DZ_LPR_PARENB | DZ_LPR_OPAR | 1 /* line */;
	daa.daa_line = 1;
	daa.daa_flags = 0;
	config_found(self, &daa, dz_print);
#endif
	s = spltty();
	dzrint(sc);
	dzxint(sc);
	splx(s);
}

int
dzcngetc(dev_t dev)
{
	int c = 0;
	int mino = minor(dev);
	u_short rbuf;

	do {
		while ((dz->csr & 0x80) == 0)
			; /* Wait for char */
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) != mino)
			continue;
		c = rbuf & 0x7f;
	} while (c == 17 || c == 19);		/* ignore XON/XOFF */

	if (c == 13)
		c = 10;

	return (c);
}

void
dzcnprobe(struct consdev *cndev)
{
	extern	vaddr_t iospace;
	int diagcons;
	paddr_t ioaddr = 0x200A0000;
	extern const struct cdevsw dz_cdevsw;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		diagcons = (vax_confdata & 0x20 ? 3 : 0);
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		diagcons = (vax_confdata & 0x100 ? 3 : 0);
		break;

	case VAX_BTYP_49:
	case VAX_BTYP_53:
		ioaddr = 0x25000000;
		diagcons = 3;
		break;

	default:
		cndev->cn_pri = CN_DEAD;
		return;
	}
	if (diagcons)
		cndev->cn_pri = CN_REMOTE;
	else
		cndev->cn_pri = CN_NORMAL;
	cndev->cn_dev = makedev(cdevsw_lookup_major(&dz_cdevsw), diagcons);
	(vaddr_t)dz = dz_regs = iospace;
	ioaccess(iospace, ioaddr, 1);
}

void
dzcninit(struct consdev *cndev)
{
	dz = (void*)dz_regs;

	dz->csr = 0;    /* Disable scanning until initting is done */
	dz->tcr = (1 << minor(cndev->cn_dev));    /* Turn on xmitter */
	dz->csr = 0x20; /* Turn scanning back on */
}


void
dzcnputc(dev_t dev, int	ch)
{
	int timeout = 1<<15;            /* don't hang the machine! */
	int mino = minor(dev);
	u_short tcr;

	if (mfpr(PR_MAPEN) == 0)
		return;

	tcr = dz->tcr;	/* remember which lines to scan */
	dz->tcr = (1 << mino);

	while ((dz->csr & 0x8000) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;
	dz->tdr = ch;                    /* Put the  character */
	timeout = 1<<15;
	while ((dz->csr & 0x8000) == 0) /* Wait until ready */
		if (--timeout < 0)
			break;

	dz->tcr = tcr;
}

void 
dzcnpollc(dev_t dev, int pollflag)
{
	static	u_char mask;

	if (pollflag)
		mask = vsbus_setmask(0);
	else
		vsbus_setmask(mask);
}

#if NDZKBD > 0 || NDZMS > 0
int
dzgetc(struct dz_linestate *ls)
{
	int line = ls->dz_line;
	u_short rbuf;

	for (;;) {
		for(; (dz->csr & DZ_CSR_RX_DONE) == 0;);
		rbuf = dz->rbuf;
		if (((rbuf >> 8) & 3) == line)
			return (rbuf & 0xff);
	}
}

void
dzputc(struct dz_linestate *ls, int ch)
{
	int line;
	u_short tcr;
	int s;
	extern const struct cdevsw dz_cdevsw;

	/* if the dz has already been attached, the MI
	   driver will do the transmitting: */
	if (ls && ls->dz_sc) {
		s = spltty();
		line = ls->dz_line;
		putc(ch, &ls->dz_tty->t_outq);
		tcr = dz->tcr;
		if (!(tcr & (1 << line)))
			dz->tcr = tcr | (1 << line);
		dzxint(ls->dz_sc);
		splx(s);
		return;
	}

	/* use dzcnputc to do the transmitting: */
	dzcnputc(makedev(cdevsw_lookup_major(&dz_cdevsw), 0), ch);
}
#endif /* NDZKBD > 0 || NDZMS > 0 */
