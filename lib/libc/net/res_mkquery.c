/*	$NetBSD: res_mkquery.c,v 1.6.2.1 1996/09/20 17:00:45 jtc Exp $	*/

/*-
 * Copyright (c) 1985, 1993
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)res_mkquery.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "$Id: res_mkquery.c,v 8.3 1995/06/29 09:26:28 vixie Exp ";
#else
static char rcsid[] = "$NetBSD: res_mkquery.c,v 1.6.2.1 1996/09/20 17:00:45 jtc Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(res_mkquery,_res_mkquery);
#endif

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
int
res_mkquery(op, dname, class, type, data, datalen, newrr_in, buf, buflen)
	int op;			/* opcode of query */
	const char *dname;		/* domain name */
	int class, type;	/* class and type of query */
	const u_char *data;		/* resource record data */
	int datalen;		/* length of data */
	const u_char *newrr_in;	/* new rr for modify or append */
	u_char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
{
	register HEADER *hp;
	register u_char *cp;
	register int n;
	struct rrec *newrr = (struct rrec *) newrr_in;
	u_char *dnptrs[10], **dpp, **lastdnptr;

#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_mkquery(%d, %s, %d, %d)\n",
		       op, dname, class, type);
#endif
	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < sizeof(HEADER)))
		return(-1);
	memset(buf, 0, sizeof(HEADER));
	hp = (HEADER *) buf;
	hp->id = htons(++_res.id);
	hp->opcode = op;
	hp->pr = (_res.options & RES_PRIMARY) != 0;
	hp->rd = (_res.options & RES_RECURSE) != 0;
	hp->rcode = NOERROR;
	cp = buf + sizeof(HEADER);
	buflen -= sizeof(HEADER);
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof(dnptrs)/sizeof(dnptrs[0]);
	/*
	 * perform opcode specific processing
	 */
	switch (op) {
	case QUERY:
	case NS_NOTIFY_OP:
		if ((buflen -= QFIXEDSZ) < 0)
			return(-1);
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(type, cp);
		cp += sizeof(u_int16_t);
		__putshort(class, cp);
		cp += sizeof(u_int16_t);
		hp->qdcount = htons(1);
		if (op == QUERY || data == NULL)
			break;
		/*
		 * Make an additional record for completion domain.
		 */
		buflen -= RRFIXEDSZ;
		if ((n = dn_comp(data, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(T_NULL, cp);
		cp += sizeof(u_int16_t);
		__putshort(class, cp);
		cp += sizeof(u_int16_t);
		__putlong(0, cp);
		cp += sizeof(u_int32_t);
		__putshort(0, cp);
		cp += sizeof(u_int16_t);
		hp->arcount = htons(1);
		break;

	case IQUERY:
		/*
		 * Initialize answer section
		 */
		if (buflen < 1 + RRFIXEDSZ + datalen)
			return (-1);
		*cp++ = '\0';	/* no domain name */
		__putshort(type, cp);
		cp += sizeof(u_int16_t);
		__putshort(class, cp);
		cp += sizeof(u_int16_t);
		__putlong(0, cp);
		cp += sizeof(u_int32_t);
		__putshort(datalen, cp);
		cp += sizeof(u_int16_t);
		if (datalen) {
			bcopy(data, cp, datalen);
			cp += datalen;
		}
		hp->ancount = htons(1);
		break;

#ifdef ALLOW_UPDATES
	/*
	 * For UPDATEM/UPDATEMA, do UPDATED/UPDATEDA followed by UPDATEA
	 * (Record to be modified is followed by its replacement in msg.)
	 */
	case UPDATEM:
	case UPDATEMA:

	case UPDATED:
		/*
		 * The res code for UPDATED and UPDATEDA is the same; user
		 * calls them differently: specifies data for UPDATED; server
		 * ignores data if specified for UPDATEDA.
		 */
	case UPDATEDA:
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		__putshort(type, cp);
                cp += sizeof(u_int16_t);
                __putshort(class, cp);
                cp += sizeof(u_int16_t);
		__putlong(0, cp);
		cp += sizeof(u_int32_t);
		__putshort(datalen, cp);
                cp += sizeof(u_int16_t);
		if (datalen) {
			bcopy(data, cp, datalen);
			cp += datalen;
		}
		if ( (op == UPDATED) || (op == UPDATEDA) ) {
			hp->ancount = htons(0);
			break;
		}
		/* Else UPDATEM/UPDATEMA, so drop into code for UPDATEA */

	case UPDATEA:	/* Add new resource record */
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		__putshort(newrr->r_type, cp);
                cp += sizeof(u_int16_t);
                __putshort(newrr->r_class, cp);
                cp += sizeof(u_int16_t);
		__putlong(0, cp);
		cp += sizeof(u_int32_t);
		__putshort(newrr->r_size, cp);
                cp += sizeof(u_int16_t);
		if (newrr->r_size) {
			bcopy(newrr->r_data, cp, newrr->r_size);
			cp += newrr->r_size;
		}
		hp->ancount = htons(0);
		break;

#endif /* ALLOW_UPDATES */
	default:
		return (-1);
	}
	return (cp - buf);
}
