/*	$NetBSD: clnp_frag.c,v 1.18.26.2 2007/05/07 10:56:08 yamt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)clnp_frag.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clnp_frag.c,v 1.18.26.2 2007/05/07 10:56:08 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/clnp.h>
#include <netiso/clnp_stat.h>
#include <netiso/argo_debug.h>

/* all fragments are hung off this list */
struct clnp_fragl *clnp_frags = NULL;

/*
 * FUNCTION:		clnp_fragment
 *
 * PURPOSE:		Fragment a datagram, and send the itty bitty pieces
 *			out over an interface.
 *
 * RETURNS:		success - 0
 *			failure - unix error code
 *
 * SIDE EFFECTS:
 *
 * NOTES:		If there is an error sending the packet, clnp_discard
 *			is called to discard the packet and send an ER. If
 *			clnp_fragment was called from clnp_output, then
 *			we generated the packet, and should not send an
 *			ER -- clnp_emit_er will check for this. Otherwise,
 *			the packet was fragmented during forwarding. In this
 *			case, we ought to send an ER back.
 */
int
clnp_fragment(
	struct ifnet   *ifp,	/* ptr to outgoing interface */
	struct mbuf    *m,	/* ptr to packet */
	const struct sockaddr *first_hop,	/* ptr to first hop */
	int             total_len,	/* length of datagram */
	int             segoff,	/* offset of segpart in hdr */
	int             flags,	/* flags passed to clnp_output */
	struct rtentry *rt)	/* route if direct ether */
{
	struct clnp_fixed *clnp = mtod(m, struct clnp_fixed *);
	int             hdr_len = (int) clnp->cnf_hdr_len;
	int             frag_size = (SN_MTU(ifp, rt) - hdr_len) & ~7;

	total_len -= hdr_len;
	if ((clnp->cnf_type & CNF_SEG_OK) &&
	    (total_len >= 8) &&
	    (frag_size > 8 || (frag_size == 8 && !(total_len & 7)))) {
		struct mbuf    *hdr = NULL;	/* save copy of clnp hdr */
		struct mbuf    *frag_hdr = NULL;
		struct mbuf    *frag_data = NULL;
		struct clnp_segment seg_part;	/* segmentation header */
		int             frag_base;
		int             error = 0;


		INCSTAT(cns_fragmented);
		(void)memmove(&seg_part, segoff + mtod(m, char *),
		    sizeof(seg_part));
		frag_base = ntohs(seg_part.cng_off);
		/*
		 *	Duplicate header, and remove from packet
		 */
		if ((hdr = m_copy(m, 0, hdr_len)) == NULL) {
			clnp_discard(m, GEN_CONGEST);
			return (ENOBUFS);
		}
		m_adj(m, hdr_len);

		while (total_len > 0) {
			int             remaining, last_frag;

#ifdef ARGO_DEBUG
			if (argo_debug[D_FRAG]) {
				struct mbuf    *mdump = frag_hdr;
				int             tot_mlen = 0;
				printf("clnp_fragment: total_len %d:\n",
				    total_len);
				while (mdump != NULL) {
					printf("\tmbuf %p, m_len %d\n",
					    mdump, mdump->m_len);
					tot_mlen += mdump->m_len;
					mdump = mdump->m_next;
				}
				printf("clnp_fragment: sum of mbuf chain %d:\n",
				    tot_mlen);
			}
#endif

			frag_size = min(total_len, frag_size);
			if ((remaining = total_len - frag_size) == 0)
				last_frag = 1;
			else {
				/*
				 * If this fragment will cause the last one to
				 * be less than 8 bytes, shorten this fragment
				 * a bit. The obscure test on frag_size above
				 * ensures that frag_size will be positive.
				 */
				last_frag = 0;
				if (remaining < 8)
					frag_size -= 8;
			}


#ifdef ARGO_DEBUG
			if (argo_debug[D_FRAG]) {
				printf(
				    "clnp_fragment: seg off %d, size %d, rem %d\n",
				    ntohs(seg_part.cng_off), frag_size,
				       total_len - frag_size);
				if (last_frag)
					printf(
					  "clnp_fragment: last fragment\n");
			}
#endif

			if (last_frag) {
				/*
				 * this is the last fragment; we don't need
				 * to get any other mbufs.
				 */
				frag_hdr = hdr;
				frag_data = m;
			} else {
				/* duplicate header and data mbufs */
				frag_hdr = m_copy(hdr, 0, (int) M_COPYALL);
				if (frag_hdr == NULL) {
					clnp_discard(hdr, GEN_CONGEST);
					m_freem(m);
					return (ENOBUFS);
				}
				frag_data = m_copy(m, 0, frag_size);
				if (frag_data == NULL) {
					clnp_discard(hdr, GEN_CONGEST);
					m_freem(m);
					m_freem(frag_hdr);
					return (ENOBUFS);
				}
				INCSTAT(cns_fragments);
			}
			clnp = mtod(frag_hdr, struct clnp_fixed *);

			if (!last_frag)
				clnp->cnf_type |= CNF_MORE_SEGS;

			/* link together */
			m_cat(frag_hdr, frag_data);

			/* insert segmentation part; updated below */
			(void)memmove(mtod(frag_hdr, char *) + segoff,
			    &seg_part,
			    sizeof(struct clnp_segment));

			{
				int             derived_len = hdr_len + frag_size;
				HTOC(clnp->cnf_seglen_msb,
				     clnp->cnf_seglen_lsb, derived_len);
				if ((frag_hdr->m_flags & M_PKTHDR) == 0)
					panic("clnp_frag:lost header");
				frag_hdr->m_pkthdr.len = derived_len;
			}

			/* compute clnp checksum (on header only) */
			if (flags & CLNP_NO_CKSUM) {
				HTOC(clnp->cnf_cksum_msb,
				     clnp->cnf_cksum_lsb, 0);
			} else {
				iso_gen_csum(frag_hdr, CLNP_CKSUM_OFF, hdr_len);
			}

#ifdef ARGO_DEBUG
			if (argo_debug[D_DUMPOUT]) {
				struct mbuf    *mdump = frag_hdr;
				printf("clnp_fragment: sending dg:\n");
				while (mdump != NULL) {
					printf("\tmbuf %p, m_len %d\n",
					    mdump, mdump->m_len);
					mdump = mdump->m_next;
				}
			}
#endif

#ifdef	TROLL
			error = troll_output(ifp, frag_hdr, first_hop, rt);
#else
			error = (*ifp->if_output) (ifp, frag_hdr, first_hop, rt);
#endif				/* TROLL */

			/*
			 * Tough situation: if the error occurred on the last
			 * fragment, we can not send an ER, as the if_output
			 * routine consumed the packet. If the error occurred
			 * on any intermediate packets, we can send an ER
			 * because we still have the original header in (m).
			 */
			if (error) {
				if (frag_hdr != hdr) {
					/*
					 * The error was not on the last
					 * fragment. We must free hdr and m
					 * before returning
					 */
					clnp_discard(hdr, GEN_NOREAS);
					m_freem(m);
				}
				return (error);
			}
			/*
			 * bump segment offset, trim data mbuf, and decrement
			 * count left
			 */
#ifdef	TROLL
			/*
			 * Decrement frag_size by some fraction. This will
			 * cause the next fragment to start 'early', thus
			 * duplicating the end of the current fragment.
			 * troll.tr_dup_size controls the fraction. If
			 * positive, it specifies the fraction. If
			 * negative, a random fraction is used.
			 */
			if ((trollctl.tr_ops & TR_DUPEND) && (!last_frag)) {
				int             num_bytes = frag_size;

				if (trollctl.tr_dup_size > 0)
					num_bytes *= trollctl.tr_dup_size;
				else
					num_bytes *= troll_random();
				frag_size -= num_bytes;
			}
#endif				/* TROLL */
			total_len -= frag_size;
			if (!last_frag) {
				frag_base += frag_size;
				seg_part.cng_off = htons(frag_base);
				m_adj(m, frag_size);
			}
		}
		return (0);
	} else {
		INCSTAT(cns_cantfrag);
		clnp_discard(m, GEN_SEGNEEDED);
		return (EMSGSIZE);
	}
}

/*
 * FUNCTION:		clnp_reass
 *
 * PURPOSE:		Attempt to reassemble a clnp packet given the current
 *			fragment. If reassembly succeeds (all the fragments
 *			are present), then return a pointer to an mbuf chain
 *			containing the reassembled packet. This packet will
 *			appear in the mbufs as if it had just arrived in
 *			one piece.
 *
 *			If reassembly fails, then save this fragment and
 *			return 0.
 *
 * RETURNS:		Ptr to assembled packet, or 0
 *
 * SIDE EFFECTS:
 *
 * NOTES: 		clnp_slowtimo can not affect this code because
 *			clnpintr, and thus this code, is called at a higher
 *			priority than clnp_slowtimo.
 */
struct mbuf    *
clnp_reass(
	struct mbuf    *m,	/* new fragment */
	struct iso_addr *src,	/* src of new fragment */
	struct iso_addr *dst,	/* dst of new fragment */
	struct clnp_segment *seg)	/* segment part of fragment header */
{
	struct clnp_fragl *cfh;

	/* look for other fragments of this datagram */
	for (cfh = clnp_frags; cfh != NULL; cfh = cfh->cfl_next) {
		if (seg->cng_id == cfh->cfl_id &&
		    iso_addrmatch1(src, &cfh->cfl_src) &&
		    iso_addrmatch1(dst, &cfh->cfl_dst)) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_REASS]) {
				printf("clnp_reass: found packet\n");
			}
#endif
			/*
			 * There are other fragments here already. Lets see if
			 * this fragment is of any help
			 */
			clnp_insert_frag(cfh, m, seg);
			if ((m = clnp_comp_pdu(cfh)) != NULL) {
				struct clnp_fixed *clnp =
				mtod(m, struct clnp_fixed *);
				HTOC(clnp->cnf_seglen_msb,
				     clnp->cnf_seglen_lsb,
				     seg->cng_tot_len);
			}
			return (m);
		}
	}

#ifdef ARGO_DEBUG
	if (argo_debug[D_REASS]) {
		printf("clnp_reass: new packet!\n");
	}
#endif

	/*
	 * This is the first fragment. If src is not consuming too many
	 * resources, then create a new fragment list and add
	 * this fragment to the list.
	 */
	/* TODO: don't let one src hog all the reassembly buffers */
	if (!clnp_newpkt(m, src, dst, seg) /* || this src is a hog */ ) {
		INCSTAT(cns_fragdropped);
		clnp_discard(m, GEN_CONGEST);
	}
	return (NULL);
}

/*
 * FUNCTION:		clnp_newpkt
 *
 * PURPOSE:		Create the necessary structures to handle a new
 *			fragmented clnp packet.
 *
 * RETURNS:		non-zero if it succeeds, zero if fails.
 *
 * SIDE EFFECTS:
 *
 * NOTES:		Failure is only due to insufficient resources.
 */
int
clnp_newpkt(
	struct mbuf *m,		/* new fragment */
	struct iso_addr *src,	/* src of new fragment */
	struct iso_addr *dst,	/* dst of new fragment */
	struct clnp_segment *seg)	/* segment part of fragment header */
{
	struct clnp_fragl *cfh;
	struct clnp_fixed *clnp;

	clnp = mtod(m, struct clnp_fixed *);

	/*
	 * Allocate new clnp fragl structure to act as header of all
	 * fragments for this datagram.
	 */
	MALLOC(cfh, struct clnp_fragl *, sizeof (struct clnp_fragl),
	   M_FTABLE, M_NOWAIT);
	if (cfh == NULL) {
		return (0);
	}

	/*
	 * Duplicate the header of this fragment, and save in cfh. Free m0
	 * and return if m_copy does not succeed.
	 */
	cfh->cfl_orighdr = m_copy(m, 0, (int) clnp->cnf_hdr_len);
	if (cfh->cfl_orighdr == NULL) {
		FREE(cfh, M_FTABLE);
		return (0);
	}
	/* Fill in rest of fragl structure */
	bcopy((void *) src, (void *) & cfh->cfl_src, sizeof(struct iso_addr));
	bcopy((void *) dst, (void *) & cfh->cfl_dst, sizeof(struct iso_addr));
	cfh->cfl_id = seg->cng_id;
	cfh->cfl_ttl = clnp->cnf_ttl;
	cfh->cfl_last = (seg->cng_tot_len - clnp->cnf_hdr_len) - 1;
	cfh->cfl_frags = NULL;
	cfh->cfl_next = NULL;

	/* Insert into list of packets */
	cfh->cfl_next = clnp_frags;
	clnp_frags = cfh;

	/* Insert this fragment into list headed by cfh */
	clnp_insert_frag(cfh, m, seg);
	return (1);
}

/*
 * FUNCTION:		clnp_insert_frag
 *
 * PURPOSE:		Insert fragment into list headed by 'cf'.
 *
 * RETURNS:		nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:		This is the 'guts' of the reassembly algorithm.
 *			Each fragment in this list contains a clnp_frag
 *			structure followed by the data of the fragment.
 *			The clnp_frag structure actually lies on top of
 *			part of the old clnp header.
 */
void
clnp_insert_frag(
	struct clnp_fragl *cfh,	/* header of list of packet fragments */
	struct mbuf *m,		/* new fragment */
	struct clnp_segment *seg)	/* segment part of fragment header */
{
	struct clnp_fixed *clnp;		/* clnp hdr of fragment */
	struct clnp_frag *cf;			/* generic fragment ptr */
	struct clnp_frag *cf_sub = NULL;	/* frag subseq to new
							 * one */
	struct clnp_frag *cf_prev = NULL;	/* frag prev to new one */
	u_short         first;	/* offset of first byte of initial pdu */
	u_short         last;	/* offset of last byte of initial pdu */
	u_short         fraglen;/* length of fragment */

	clnp = mtod(m, struct clnp_fixed *);
	first = seg->cng_off;
	CTOH(clnp->cnf_seglen_msb, clnp->cnf_seglen_lsb, fraglen);
	fraglen -= clnp->cnf_hdr_len;
	last = (first + fraglen) - 1;

#ifdef ARGO_DEBUG
	if (argo_debug[D_REASS]) {
		printf("clnp_insert_frag: New fragment: [%d-%d], len %d\n",
		    first, last, fraglen);
		printf("clnp_insert_frag: current fragments:\n");
		for (cf = cfh->cfl_frags; cf != NULL; cf = cf->cfr_next) {
			printf("\tcf %p: [%d-%d]\n",
			    cf, cf->cfr_first, cf->cfr_last);
		}
	}
#endif

	if (cfh->cfl_frags != NULL) {
		/*
		 * Find fragment which begins after the new one
		 */
		for (cf = cfh->cfl_frags; cf != NULL;
		     cf_prev = cf, cf = cf->cfr_next) {
			if (cf->cfr_first > first) {
				cf_sub = cf;
				break;
			}
		}

#ifdef ARGO_DEBUG
		if (argo_debug[D_REASS]) {
			printf("clnp_insert_frag: Previous frag is ");
			if (cf_prev == NULL)
				printf("NULL\n");
			else
				printf("[%d-%d]\n", cf_prev->cfr_first,
				    cf_prev->cfr_last);
			printf("clnp_insert_frag: Subsequent frag is ");
			if (cf_sub == NULL)
				printf("NULL\n");
			else
				printf("[%d-%d]\n", cf_sub->cfr_first,
				    cf_sub->cfr_last);
		}
#endif

		/*
		 * If there is a fragment before the new one, check if it
		 * overlaps the new one. If so, then trim the end of the
		 * previous one.
		 */
		if (cf_prev != NULL) {
			if (cf_prev->cfr_last > first) {
				u_short         overlap = cf_prev->cfr_last - first;

#ifdef ARGO_DEBUG
				if (argo_debug[D_REASS]) {
					printf(
					    "clnp_insert_frag: previous overlaps by %d\n",
					    overlap);
				}
#endif

				if (overlap > fraglen) {
					/*
					 * The new fragment is entirely
					 * contained in the preceding one.
					 * We can punt on the new frag
					 * completely.
					 */
					m_freem(m);
					return;
				} else {
					/*
					 * Trim data off of end of previous
					 * fragment
					 */
					/*
					 * inc overlap to prevent duplication
					 * of last byte
					 */
					overlap++;
					m_adj(cf_prev->cfr_data, -(int) overlap);
					cf_prev->cfr_last -= overlap;
				}
			}
		}
		/*
		 *	For all fragments past the new one, check if any data on
		 *	the new one overlaps data on existing fragments. If so,
		 *	then trim the extra data off the end of the new one.
		 */
		for (cf = cf_sub; cf != NULL; cf = cf->cfr_next) {
			if (cf->cfr_first < last) {
				u_short         overlap = last - cf->cfr_first;

#ifdef ARGO_DEBUG
				if (argo_debug[D_REASS]) {
					printf(
					    "clnp_insert_frag: subsequent overlaps by %d\n",
					    overlap);
				}
#endif

				if (overlap > fraglen) {
					/*
					 * The new fragment is entirely
					 * contained in the succeeding one.
					 * This should not happen, because
					 * early on in this code we scanned
					 * for the fragment which started
					 * after the new one!
					 */
					m_freem(m);
					printf(
					    "clnp_insert_frag: internal error!\n");
					return;
				} else {
					/*
					 * Trim data off of end of new fragment
					 * inc overlap to prevent duplication
					 * of last byte
					 */
					overlap++;
					m_adj(m, -(int) overlap);
					last -= overlap;
				}
			}
		}
	}
	/*
	 * Insert the new fragment beween cf_prev and cf_sub
	 *
	 * Note: the clnp hdr is still in the mbuf.
	 * If the data of the mbuf is not word aligned, shave off enough
	 * so that it is. Then, cast the clnp_frag structure on top
	 * of the clnp header.
	 * The clnp_hdr will not be used again (as we already have
	 * saved a copy of it).
	 *
	 * Save in cfr_bytes the number of bytes to shave off to get to
	 * the data of the packet. This is used when we coalesce fragments;
	 * the clnp_frag structure must be removed before joining mbufs.
	 */
	{
		int             pad;
		u_int           bytes;

		/* determine if header is not word aligned */
		pad = (long) clnp % 4;
		if (pad < 0)
			pad = -pad;

		/* bytes is number of bytes left in front of data */
		bytes = clnp->cnf_hdr_len - pad;

#ifdef ARGO_DEBUG
		if (argo_debug[D_REASS]) {
			printf(
			"clnp_insert_frag: clnp %p requires %d alignment\n",
			       clnp, pad);
		}
#endif

		/* make it word aligned if necessary */
		if (pad)
			m_adj(m, pad);

		cf = mtod(m, struct clnp_frag *);
		cf->cfr_bytes = bytes;

#ifdef ARGO_DEBUG
		if (argo_debug[D_REASS]) {
			printf("clnp_insert_frag: cf now %p, cfr_bytes %d\n",
			       cf, cf->cfr_bytes);
		}
#endif
	}
	cf->cfr_first = first;
	cf->cfr_last = last;


	/*
	 * The data is the mbuf itself, although we must remember that the
	 * first few bytes are actually a clnp_frag structure
	 */
	cf->cfr_data = m;

	/* link into place */
	cf->cfr_next = cf_sub;
	if (cf_prev == NULL)
		cfh->cfl_frags = cf;
	else
		cf_prev->cfr_next = cf;
}

/*
 * FUNCTION:		clnp_comp_pdu
 *
 * PURPOSE:		Scan the list of fragments headed by cfh. Merge
 *			any contigious fragments into one. If, after
 *			traversing all the fragments, it is determined that
 *			the packet is complete, then return a pointer to
 *			the packet (with header prepended). Otherwise,
 *			return NULL.
 *
 * RETURNS:		NULL, or a pointer to the assembled pdu in an mbuf
 *			chain.
 *
 * SIDE EFFECTS:	Will colapse contigious fragments into one.
 *
 * NOTES:		This code assumes that there are no overlaps of
 *			fragment pdus.
 */
struct mbuf    *
clnp_comp_pdu(
	struct clnp_fragl *cfh)	/* fragment header */
{
	struct clnp_frag *cf = cfh->cfl_frags;

	while (cf->cfr_next != NULL) {
		struct clnp_frag *cf_next = cf->cfr_next;

#ifdef ARGO_DEBUG
		if (argo_debug[D_REASS]) {
			printf("clnp_comp_pdu: comparing: [%d-%d] to [%d-%d]\n",
			    cf->cfr_first, cf->cfr_last, cf_next->cfr_first,
			    cf_next->cfr_last);
		}
#endif

		if (cf->cfr_last == (cf_next->cfr_first - 1)) {
			/*
			 * Merge fragment cf and cf_next
			 *
			 * - update cf header
			 * - trim clnp_frag structure off of cf_next
			 * - append cf_next to cf
			 */
			struct clnp_frag cf_next_hdr;
			struct clnp_frag *next_frag;

			cf_next_hdr = *cf_next;
			next_frag = cf_next->cfr_next;

#ifdef ARGO_DEBUG
			if (argo_debug[D_REASS]) {
				struct mbuf    *mdump;
				int             l;
				printf("clnp_comp_pdu: merging fragments\n");
				printf(
				    "clnp_comp_pdu: 1st: [%d-%d] (bytes %d)\n",
				    cf->cfr_first, cf->cfr_last,
				    cf->cfr_bytes);
				mdump = cf->cfr_data;
				l = 0;
				while (mdump != NULL) {
					printf("\tmbuf %p, m_len %d\n",
					    mdump, mdump->m_len);
					l += mdump->m_len;
					mdump = mdump->m_next;
				}
				printf("\ttotal len: %d\n", l);
				printf(
				    "clnp_comp_pdu: 2nd: [%d-%d] (bytes %d)\n",
				    cf_next->cfr_first, cf_next->cfr_last,
				       cf_next->cfr_bytes);
				mdump = cf_next->cfr_data;
				l = 0;
				while (mdump != NULL) {
					printf("\tmbuf %p, m_len %d\n",
					       mdump, mdump->m_len);
					l += mdump->m_len;
					mdump = mdump->m_next;
				}
				printf("\ttotal len: %d\n", l);
			}
#endif

			cf->cfr_last = cf_next->cfr_last;
			/*
			 * After this m_adj, the cf_next ptr is useless
			 * because we have adjusted the clnp_frag structure
			 * away...
			 */
#ifdef ARGO_DEBUG
			if (argo_debug[D_REASS]) {
				printf("clnp_comp_pdu: shaving off %d bytes\n",
				       cf_next_hdr.cfr_bytes);
			}
#endif
			m_adj(cf_next_hdr.cfr_data,
			      (int) cf_next_hdr.cfr_bytes);
			m_cat(cf->cfr_data, cf_next_hdr.cfr_data);
			cf->cfr_next = next_frag;
		} else {
			cf = cf->cfr_next;
		}
	}

	cf = cfh->cfl_frags;

#ifdef ARGO_DEBUG
	if (argo_debug[D_REASS]) {
		struct mbuf    *mdump = cf->cfr_data;
		printf("clnp_comp_pdu: first frag now: [%d-%d]\n",
		    cf->cfr_first, cf->cfr_last);
		printf("clnp_comp_pdu: data for frag:\n");
		while (mdump != NULL) {
			printf("mbuf %p, m_len %d\n", mdump, mdump->m_len);
			/* dump_buf(mtod(mdump, void *), mdump->m_len); */
			mdump = mdump->m_next;
		}
	}
#endif

	/* Check if datagram is complete */
	if ((cf->cfr_first == 0) && (cf->cfr_last == cfh->cfl_last)) {
		/*
		 * We have a complete pdu!
		 * - Remove the frag header from (only) remaining fragment
		 *   (which is not really a fragment anymore, as the datagram
		 *    is complete).
		 * - Prepend a clnp header
		 */
		struct mbuf    *data = cf->cfr_data;
		struct mbuf    *hdr = cfh->cfl_orighdr;
		struct clnp_fragl *scan;

#ifdef ARGO_DEBUG
		if (argo_debug[D_REASS]) {
			printf("clnp_comp_pdu: complete pdu!\n");
		}
#endif

		m_adj(data, (int) cf->cfr_bytes);
		m_cat(hdr, data);

#ifdef ARGO_DEBUG
		if (argo_debug[D_DUMPIN]) {
			struct mbuf    *mdump = hdr;
			printf("clnp_comp_pdu: pdu is:\n");
			while (mdump != NULL) {
				printf("mbuf %p, m_len %d\n",
				       mdump, mdump->m_len);
#if 0
				dump_buf(mtod(mdump, void *), mdump->m_len);
#endif
				mdump = mdump->m_next;
			}
		}
#endif

		/*
		 * Remove cfh from the list of fragmented pdus
		 */
		if (clnp_frags == cfh) {
			clnp_frags = cfh->cfl_next;
		} else {
			for (scan = clnp_frags; scan != NULL;
			     scan = scan->cfl_next) {
				if (scan->cfl_next == cfh) {
					scan->cfl_next = cfh->cfl_next;
					break;
				}
			}
		}

		/* free cfh */
		FREE(cfh, M_FTABLE);

		return (hdr);
	}
	return (NULL);
}
#ifdef	TROLL
static int      troll_cnt;
#include <sys/time.h>
/*
 * FUNCTION:		troll_random
 *
 * PURPOSE:		generate a pseudo-random number between 0 and 1
 *
 * RETURNS:		the random number
 *
 * SIDE EFFECTS:
 *
 * NOTES:		This is based on the clock.
 */
float
troll_random()
{
	extern struct timeval time;
	long            t = time.tv_usec % 100;

	return ((float) t / (float) 100);
}

/*
 * FUNCTION:		troll_output
 *
 * PURPOSE:		Do something sneaky with the datagram passed. Possible
 *			operations are:
 *				Duplicate the packet
 *				Drop the packet
 *				Trim some number of bytes from the packet
 *				Munge some byte in the packet
 *
 * RETURNS:		0, or unix error code
 *
 * SIDE EFFECTS:
 *
 * NOTES:		The operation of this procedure is regulated by the
 *			troll control structure (Troll).
 */
int
troll_output(ifp, m, dst, rt)
	struct ifnet   *ifp;
	struct mbuf    *m;
	const struct sockaddr *dst;
	struct rtentry *rt;
{
	int             err = 0;
	troll_cnt++;

	if (trollctl.tr_ops & TR_DUPPKT) {
		/*
		 *	Duplicate every Nth packet
		 *	TODO: random?
		 */
		float           f_freq = troll_cnt * trollctl.tr_dup_freq;
		int             i_freq = troll_cnt * trollctl.tr_dup_freq;
		if (i_freq == f_freq) {
			struct mbuf    *dup = m_copy(m, 0, (int) M_COPYALL);
			if (dup != NULL)
				err = (*ifp->if_output) (ifp, dup, dst, rt);
		}
		if (!err)
			err = (*ifp->if_output) (ifp, m, dst, rt);
		return (err);
	} else if (trollctl.tr_ops & TR_DROPPKT) {
	} else if (trollctl.tr_ops & TR_CHANGE) {
		struct clnp_fixed *clnp = mtod(m, struct clnp_fixed *);
		clnp->cnf_cksum_msb = 0;
		err = (*ifp->if_output) (ifp, m, dst, rt);
		return (err);
	} else {
		err = (*ifp->if_output) (ifp, m, dst, rt);
		return (err);
	}
}

#endif				/* TROLL */
