/*	$NetBSD: if_uba.c,v 1.17.6.2 2004/09/18 14:42:05 skrll Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	@(#)if_uba.c	7.16 (Berkeley) 12/16/90
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_uba.c,v 1.17.6.2 2004/09/18 14:42:05 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>

#include <vax/if/if_uba.h>
#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

static	int if_ubaalloc __P((struct ifubinfo *, struct ifrw *, int));
static	void rcv_xmtbuf __P((struct ifxmt *));
static	void restor_xmtbuf __P((struct ifxmt *));

/*
 * Routines supporting UNIBUS network interfaces.
 *
 * TODO:
 *	Support interfaces using only one BDP statically.
 */

/*
 * Init UNIBUS for interface whose headers of size hlen are to
 * end on a page boundary.  We allocate a UNIBUS map register for the page
 * with the header, and nmr more UNIBUS map registers for i/o on the adapter,
 * doing this once for each read and once for each write buffer.  We also
 * allocate page frames in the mbuffer pool for these pages.
 */
int
if_ubaminit(ifu, uh, hlen, nmr, ifr, nr, ifw, nw)
	register struct ifubinfo *ifu;
	struct uba_softc *uh;
	int hlen, nmr, nr, nw;
	register struct ifrw *ifr;
	register struct ifxmt *ifw;
{
	register caddr_t p;
	caddr_t cp;
	int i, nclbytes, off;

	if (hlen)
		off = MCLBYTES - hlen;
	else
		off = 0;
	nclbytes = roundup(nmr * VAX_NBPG, MCLBYTES);
	if (hlen)
		nclbytes += MCLBYTES;
	if (ifr[0].ifrw_addr)
		cp = ifr[0].ifrw_addr - off;
	else {
		cp = (caddr_t)malloc((u_long)((nr + nw) * nclbytes), M_DEVBUF,
		    M_NOWAIT);
		if (cp == 0)
			return (0);
		p = cp;
		for (i = 0; i < nr; i++) {
			ifr[i].ifrw_addr = p + off;
			p += nclbytes;
		}
		for (i = 0; i < nw; i++) {
			ifw[i].ifw_base = p;
			ifw[i].ifw_addr = p + off;
			p += nclbytes;
		}
		ifu->iff_hlen = hlen;
		ifu->iff_softc = uh;
		ifu->iff_uba = uh->uh_uba;
		ifu->iff_ubamr = uh->uh_mr;
	}
	for (i = 0; i < nr; i++)
		if (if_ubaalloc(ifu, &ifr[i], nmr) == 0) {
			nr = i;
			nw = 0;
			goto bad;
		}
	for (i = 0; i < nw; i++)
		if (if_ubaalloc(ifu, &ifw[i].ifrw, nmr) == 0) {
			nw = i;
			goto bad;
		}
	while (--nw >= 0) {
		for (i = 0; i < nmr; i++)
			ifw[nw].ifw_wmap[i] = ifw[nw].ifw_mr[i];
		ifw[nw].ifw_xswapd = 0;
		ifw[nw].ifw_flags = IFRW_W;
		ifw[nw].ifw_nmr = nmr;
	}
	return (1);
bad:
	while (--nw >= 0)
		ubarelse(ifu->iff_softc, &ifw[nw].ifw_info);
	while (--nr >= 0)
		ubarelse(ifu->iff_softc, &ifr[nr].ifrw_info);
	free(cp, M_DEVBUF);
	ifr[0].ifrw_addr = 0;
	return (0);
}

/*
 * Setup an ifrw structure by allocating UNIBUS map registers,
 * possibly a buffered data path, and initializing the fields of
 * the ifrw structure to minimize run-time overhead.
 */
static int
if_ubaalloc(ifu, ifrw, nmr)
	struct ifubinfo *ifu;
	register struct ifrw *ifrw;
	int nmr;
{
	register int info;

	info =
	    uballoc(ifu->iff_softc, ifrw->ifrw_addr, nmr*VAX_NBPG + ifu->iff_hlen,
	        ifu->iff_flags);
	if (info == 0)
		return (0);
	ifrw->ifrw_info = info;
	ifrw->ifrw_bdp = UBAI_BDP(info);
	ifrw->ifrw_proto = UBAMR_MRV | (UBAI_BDP(info) << UBAMR_DPSHIFT);
	ifrw->ifrw_mr = &ifu->iff_ubamr[UBAI_MR(info) + (ifu->iff_hlen? 1 : 0)];
	return (1);
}

/*
 * Pull read data off a interface.
 * Totlen is length of data, with local net header stripped.
 * When full cluster sized units are present
 * on the interface on cluster boundaries we can get them more
 * easily by remapping, and take advantage of this here.
 * Save a pointer to the interface structure and the total length,
 * so that protocols can determine where incoming packets arrived.
 * Note: we may be called to receive from a transmit buffer by some
 * devices.  In that case, we must force normal mapping of the buffer,
 * so that the correct data will appear (only unibus maps are 
 * changed when remapping the transmit buffers).
 */
struct mbuf *
if_ubaget(ifu, ifr, totlen, ifp)
	struct ifubinfo *ifu;
	register struct ifrw *ifr;
	register int totlen;
	struct ifnet *ifp;
{
	struct mbuf *top, **mp;
	register struct mbuf *m;
	register caddr_t cp = ifr->ifrw_addr + ifu->iff_hlen, pp;
	register int len;
	top = 0;
	mp = &top;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0){
		return ((struct mbuf *)NULL);
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	if (ifr->ifrw_flags & IFRW_W){
		rcv_xmtbuf((struct ifxmt *)ifr);
	}
	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				top = 0;
				goto out;
			}
			m->m_len = MLEN;
		}
		len = totlen;
		if (len >= MINCLSIZE) {
			struct pte *cpte, *ppte;
			int x, *ip, i;

			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0){
				goto nopage;
			}
			len = min(len, MCLBYTES);
			m->m_len = len;
			if (!claligned(cp)){
				goto copy;
			}
			/*
			 * Switch pages mapped to UNIBUS with new page pp,
			 * as quick form of copy.  Remap UNIBUS and invalidate.
			 */
			pp = mtod(m, char *);
			cpte = (struct pte *)kvtopte(cp);
			ppte = (struct pte *)kvtopte(pp);
			x = vax_btop(cp - ifr->ifrw_addr);
			ip = (int *)&ifr->ifrw_mr[x];
			for (i = 0; i < MCLBYTES/VAX_NBPG; i++) {
				struct pte t;
				t = *ppte; *ppte++ = *cpte; *cpte = t;
				*ip++ = cpte++->pg_pfn|ifr->ifrw_proto;
				mtpr(cp,PR_TBIS);
				cp += VAX_NBPG;
				mtpr((caddr_t)pp,PR_TBIS);
				pp += VAX_NBPG;
			}
			goto nocopy;
		}
nopage:
		if (len < m->m_len) {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (top == 0 && len + max_linkhdr <= m->m_len)
				m->m_data += max_linkhdr;
			m->m_len = len;
		} else
			len = m->m_len;
copy:
		bcopy(cp, mtod(m, caddr_t), (unsigned)len);
		cp += len;
nocopy:
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
	}
out:
	if (ifr->ifrw_flags & IFRW_W){
		restor_xmtbuf((struct ifxmt *)ifr);
	}
	return (top);
}

/*
 * Change the mapping on a transmit buffer so that if_ubaget may
 * receive from that buffer.  Copy data from any pages mapped to Unibus
 * into the pages mapped to normal kernel virtual memory, so that
 * they can be accessed and swapped as usual.  We take advantage
 * of the fact that clusters are placed on the xtofree list
 * in inverse order, finding the last one.
 */
static void
rcv_xmtbuf(ifw)
	register struct ifxmt *ifw;
{
	register struct mbuf *m;
	struct mbuf **mprev;
	register int i;
	char *cp;

	while ((i = ffs(ifw->ifw_xswapd)) != 0) {
		cp = ifw->ifw_base + i * MCLBYTES;
		i--;
		ifw->ifw_xswapd &= ~(1<<i);
		mprev = &ifw->ifw_xtofree;
		for (m = ifw->ifw_xtofree; m && m->m_next; m = m->m_next)
			mprev = &m->m_next;
		if (m == NULL)
			break;
		bcopy(mtod(m, caddr_t), cp, MCLBYTES);
		(void) m_free(m);
		*mprev = NULL;
	}
	ifw->ifw_xswapd = 0;
	for (i = 0; i < ifw->ifw_nmr; i++)
		ifw->ifw_mr[i] = ifw->ifw_wmap[i];
}

/*
 * Put a transmit buffer back together after doing an if_ubaget on it,
 * which may have swapped pages.
 */
static void
restor_xmtbuf(ifw)
	register struct ifxmt *ifw;
{
	register int i;

	for (i = 0; i < ifw->ifw_nmr; i++)
		ifw->ifw_wmap[i] = ifw->ifw_mr[i];
}

/*
 * Map a chain of mbufs onto a network interface
 * in preparation for an i/o operation.
 * The argument chain of mbufs includes the local network
 * header which is copied to be in the mapped, aligned
 * i/o space.
 */
int
if_ubaput(ifu, ifw, m)
	struct ifubinfo *ifu;
	register struct ifxmt *ifw;
	register struct mbuf *m;
{
	register struct mbuf *mp;
	register caddr_t cp, dp;
	register int i;
	int xswapd = 0;
	int x, cc, t;

	cp = ifw->ifw_addr;
	while (m) {
		dp = mtod(m, char *);
		if (claligned(cp) && claligned(dp) &&
		    (m->m_len == MCLBYTES || m->m_next == (struct mbuf *)0)) {
			struct pte *pte;
			int *ip;

			pte = (struct pte *)kvtopte(dp);
			x = vax_btop(cp - ifw->ifw_addr);
			ip = (int *)&ifw->ifw_mr[x];
			for (i = 0; i < MCLBYTES/VAX_NBPG; i++)
				*ip++ = ifw->ifw_proto | pte++->pg_pfn;
			xswapd |= 1 << (x>>(MCLSHIFT-VAX_PGSHIFT));
			mp = m->m_next;
			m->m_next = ifw->ifw_xtofree;
			ifw->ifw_xtofree = m;
			cp += m->m_len;
		} else {
			bcopy(mtod(m, caddr_t), cp, (unsigned)m->m_len);
			cp += m->m_len;
			MFREE(m, mp);
		}
		m = mp;
	}

	/*
	 * Xswapd is the set of clusters we just mapped out.  Ifu->iff_xswapd
	 * is the set of clusters mapped out from before.  We compute
	 * the number of clusters involved in this operation in x.
	 * Clusters mapped out before and involved in this operation
	 * should be unmapped so original pages will be accessed by the device.
	 */
	cc = cp - ifw->ifw_addr;
	x = ((cc - ifu->iff_hlen) + MCLBYTES - 1) >> MCLSHIFT;
	ifw->ifw_xswapd &= ~xswapd;
	while ((i = ffs(ifw->ifw_xswapd)) != 0) {
		i--;
		if (i >= x)
			break;
		ifw->ifw_xswapd &= ~(1<<i);
		i *= MCLBYTES/VAX_NBPG;
		for (t = 0; t < MCLBYTES/VAX_NBPG; t++) {
			ifw->ifw_mr[i] = ifw->ifw_wmap[i];
			i++;
		}
	}
	ifw->ifw_xswapd |= xswapd;
	return (cc);
}
