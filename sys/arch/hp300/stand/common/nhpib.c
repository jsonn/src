/*	$NetBSD: nhpib.c,v 1.1.60.2 2004/09/18 14:34:20 skrll Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)nhpib.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Internal/98624 HPIB driver
 */

#include <sys/param.h>

#include <hp300/dev/nhpibreg.h>

#include <hp300/stand/common/hpibvar.h>
#include <hp300/stand/common/samachdep.h>

static int nhpibiwait(struct nhpibdevice *);
static int nhpibowait(struct nhpibdevice *);

int
nhpibinit(unit)
{
	struct hpib_softc *hs = &hpib_softc[unit];
	struct nhpibdevice *hd = (void *)hs->sc_addr;

	if ((int)hd == internalhpib) {
		hs->sc_type = HPIBA;
		hs->sc_ba = HPIBA_BA;
	}
	else if (hd->hpib_cid == HPIBB) {
		hs->sc_type = HPIBB;
		hs->sc_ba = hd->hpib_csa & CSA_BA;
	}
	else
		return 0;
	nhpibreset(unit);
	return 1;
}

void
nhpibreset(unit)
{
	struct hpib_softc *hs = &hpib_softc[unit];
	struct nhpibdevice *hd;

	hd = (void *)hs->sc_addr;
	hd->hpib_acr = AUX_SSWRST;
	hd->hpib_ar = hs->sc_ba;
	hd->hpib_lim = 0;
	hd->hpib_mim = 0;
	hd->hpib_acr = AUX_CDAI;
	hd->hpib_acr = AUX_CSHDW;
	hd->hpib_acr = AUX_SSTD1;
	hd->hpib_acr = AUX_SVSTD1;
	hd->hpib_acr = AUX_CPP;
	hd->hpib_acr = AUX_CHDFA;
	hd->hpib_acr = AUX_CHDFE;
	hd->hpib_acr = AUX_RHDF;
	hd->hpib_acr = AUX_CSWRST;
	hd->hpib_acr = AUX_TCA;
	hd->hpib_acr = AUX_CSRE;
	hd->hpib_acr = AUX_SSIC;
	DELAY(100);
	hd->hpib_acr = AUX_CSIC;
	hd->hpib_acr = AUX_SSRE;
	hd->hpib_data = C_DCL;
	DELAY(100000);
}

int
nhpibsend(unit, slave, sec, buf, cnt)
	int unit, slave, sec;
	char *buf;
	int cnt;
{
	struct hpib_softc *hs = &hpib_softc[unit];
	struct nhpibdevice *hd;
	int origcnt = cnt;

	hd = (void *)hs->sc_addr;
	hd->hpib_acr = AUX_TCA;
	hd->hpib_data = C_UNL;
	nhpibowait(hd);
	hd->hpib_data = C_TAG + hs->sc_ba;
	hd->hpib_acr = AUX_STON;
	nhpibowait(hd);
	hd->hpib_data = C_LAG + slave;
	nhpibowait(hd);
	if (sec != -1) {
		hd->hpib_data = C_SCG + sec;
		nhpibowait(hd);
	}
	hd->hpib_acr = AUX_GTS;
	if (cnt) {
		while (--cnt) {
			hd->hpib_data = *buf++;
			if (nhpibowait(hd) < 0)
				break;
		}
		hd->hpib_acr = AUX_EOI;
		hd->hpib_data = *buf;
		if (nhpibowait(hd) < 0)
			cnt++;
		hd->hpib_acr = AUX_TCA;
	}
	return origcnt - cnt;
}

int
nhpibrecv(unit, slave, sec, buf, cnt)
	int unit, slave, sec;
	char *buf;
	int cnt;
{
	struct hpib_softc *hs = &hpib_softc[unit];
	struct nhpibdevice *hd;
	int origcnt = cnt;

	hd = (void *)hs->sc_addr;
	hd->hpib_acr = AUX_TCA;
	hd->hpib_data = C_UNL;
	nhpibowait(hd);
	hd->hpib_data = C_LAG + hs->sc_ba;
	hd->hpib_acr = AUX_SLON;
	nhpibowait(hd);
	hd->hpib_data = C_TAG + slave;
	nhpibowait(hd);
	if (sec != -1) {
		hd->hpib_data = C_SCG + sec;
		nhpibowait(hd);
	}
	hd->hpib_acr = AUX_RHDF;
	hd->hpib_acr = AUX_GTS;
	if (cnt) {
		while (--cnt >= 0) {
			if (nhpibiwait(hd) < 0)
				break;
			*buf++ = hd->hpib_data;
		}
		cnt++;
		hd->hpib_acr = AUX_TCA;
	}
	return origcnt - cnt;
}

int
nhpibppoll(unit)
	int unit;
{
	struct hpib_softc *hs = &hpib_softc[unit];
	struct nhpibdevice *hd;
	int ppoll;

	hd = (void *)hs->sc_addr;
	hd->hpib_acr = AUX_SPP;
	DELAY(25);
	ppoll = hd->hpib_cpt;
	hd->hpib_acr = AUX_CPP;
	return ppoll;
}

static int
nhpibowait(hd)
	struct nhpibdevice *hd;
{
	int timo = 100000;

	while ((hd->hpib_mis & MIS_BO) == 0 && --timo)
		;
	if (timo == 0)
		return -1;
	return 0;
}

static int
nhpibiwait(hd)
	struct nhpibdevice *hd;
{
	int timo = 100000;

	while ((hd->hpib_mis & MIS_BI) == 0 && --timo)
		;
	if (timo == 0)
		return -1;
	return 0;
}
