/*	$NetBSD: dns_nw.c,v 1.1.1.1.4.1 1999/12/27 18:27:32 wrstuden Exp $	*/

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "Id: dns_nw.c,v 1.19 1999/10/15 19:49:10 vixie Exp";
#endif /* LIBC_SCCS and not lint */

/* Imports. */

#include "port_before.h"

#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "dns_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) sprintf x
#endif

/* Definitions. */

#define	MAXALIASES	35

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

struct pvt {
	struct nwent	net;
	char *		ali[MAXALIASES];
	char		buf[BUFSIZ+1];
	struct __res_state * res;
	void		(*free_res)(void *);
};

typedef union {
	long	al;
	char	ac;
} align;

enum by_what { by_addr, by_name };

/* Forwards. */

static void		nw_close(struct irs_nw *);
static struct nwent *	nw_byname(struct irs_nw *, const char *, int);
static struct nwent *	nw_byaddr(struct irs_nw *, void *, int, int);
static struct nwent *	nw_next(struct irs_nw *);
static void		nw_rewind(struct irs_nw *);
static void		nw_minimize(struct irs_nw *);
static struct __res_state * nw_res_get(struct irs_nw *this);
static void		nw_res_set(struct irs_nw *this,
				   struct __res_state *res,
				   void (*free_res)(void *));

static struct nwent *	get1101byaddr(struct irs_nw *, u_char *, int);
static struct nwent *	get1101byname(struct irs_nw *, const char *);
static struct nwent *	get1101answer(struct irs_nw *,
				      u_char *ansbuf, int anslen,
				      enum by_what by_what,
				      int af, const char *name,
				      const u_char *addr, int addrlen);
static struct nwent *	get1101mask(struct irs_nw *this, struct nwent *);
static int		make1101inaddr(const u_char *, int, char *, int);
static void		normalize_name(char *name);
static int		init(struct irs_nw *this);

/* Exports. */

struct irs_nw *
irs_dns_nw(struct irs_acc *this) {
	struct irs_nw *nw;
	struct pvt *pvt;

	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(nw = memget(sizeof *nw))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(nw, 0x5e, sizeof *nw);
	nw->private = pvt;
	nw->close = nw_close;
	nw->byname = nw_byname;
	nw->byaddr = nw_byaddr;
	nw->next = nw_next;
	nw->rewind = nw_rewind;
	nw->minimize = nw_minimize;
	nw->res_get = nw_res_get;
	nw->res_set = nw_res_set;
	return (nw);
}

/* Methods. */

static void
nw_close(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	nw_minimize(this);

	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct nwent *
nw_byname(struct irs_nw *this, const char *name, int af) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (init(this) == -1)
		return (NULL);

	switch (af) {
	case AF_INET:
		return (get1101byname(this, name));
	default:
		(void)NULL;
	}
	RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
	errno = EAFNOSUPPORT;
	return (NULL);
}

static struct nwent *
nw_byaddr(struct irs_nw *this, void *net, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (init(this) == -1)
		return (NULL);

	switch (af) {
	case AF_INET:
		return (get1101byaddr(this, net, len));
	default:
		(void)NULL;
	}
	RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
	errno = EAFNOSUPPORT;
	return (NULL);
}

static struct nwent *
nw_next(struct irs_nw *this) {
	return (NULL);
}

static void
nw_rewind(struct irs_nw *this) {
	/* NOOP */
}

static void
nw_minimize(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->res)
		res_nclose(pvt->res);
}

static struct __res_state *
nw_res_get(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		nw_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
nw_res_set(struct irs_nw *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->res && pvt->free_res) {
		res_nclose(pvt->res);
		(*pvt->free_res)(pvt->res);
	}

	pvt->res = res;
	pvt->free_res = free_res;
}

/* Private. */

static struct nwent *
get1101byname(struct irs_nw *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	u_char ansbuf[MAXPACKET];
	int anslen;

	anslen = res_nsearch(pvt->res, name, C_IN, T_PTR,
			     ansbuf, sizeof ansbuf);
	if (anslen < 0)
		return (NULL);
	return (get1101mask(this, get1101answer(this, ansbuf, anslen, by_name,
						AF_INET, name, NULL, 0)));
}

static struct nwent *
get1101byaddr(struct irs_nw *this, u_char *net, int len) {
	struct pvt *pvt = (struct pvt *)this->private;
	char qbuf[sizeof "255.255.255.255.in-addr.arpa"];
	u_char ansbuf[MAXPACKET];
	int anslen;

	if (len < 1 || len > 32) {
		errno = EINVAL;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		return (NULL);
	}
	if (make1101inaddr(net, len, qbuf, sizeof qbuf) < 0)
		return (NULL);
	anslen = res_nquery(pvt->res, qbuf, C_IN, T_PTR,
			    ansbuf, sizeof ansbuf);
	if (anslen < 0)
		return (NULL);
	return (get1101mask(this, get1101answer(this, ansbuf, anslen, by_addr,
						AF_INET, NULL, net, len)));
}

static struct nwent *
get1101answer(struct irs_nw *this,
	      u_char *ansbuf, int anslen, enum by_what by_what,
	      int af, const char *name, const u_char *addr, int addrlen)
{
	struct pvt *pvt = (struct pvt *)this->private;
	int type, class, buflen, ancount, qdcount, haveanswer;
	char *bp, **ap;
	u_char *cp, *eom;
	HEADER *hp;

	/* Initialize, and parse header. */
	eom = ansbuf + anslen;
	if (ansbuf + HFIXEDSZ > eom) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	hp = (HEADER *)ansbuf;
	cp = ansbuf + HFIXEDSZ;
	qdcount = ntohs(hp->qdcount);
	while (qdcount-- > 0) {
		int n = dn_skipname(cp, eom);
		cp += n + QFIXEDSZ;
		if (n < 0 || cp > eom) {
			RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
			return (NULL);
		}
	}
	ancount = ntohs(hp->ancount);
	if (!ancount) {
		if (hp->aa)
			RES_SET_H_ERRNO(pvt->res, HOST_NOT_FOUND);
		else
			RES_SET_H_ERRNO(pvt->res, TRY_AGAIN);
		return (NULL);
	}

	/* Prepare a return structure. */
	bp = pvt->buf;
	buflen = sizeof pvt->buf;
	pvt->net.n_name = NULL;
	pvt->net.n_aliases = pvt->ali;
	pvt->net.n_addrtype = af;
	pvt->net.n_addr = NULL;
	pvt->net.n_length = addrlen;

	/* Save input key if given. */
	switch (by_what) {
	case by_name:
		if (name != NULL) {
			int n = strlen(name) + 1;

			if (n > buflen) {
				RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
				return (NULL);
			}
			pvt->net.n_name = strcpy(bp, name);
			bp += n;
			buflen -= n;
		}
		break;
	case by_addr:
		if (addr != NULL && addrlen != 0) {
			int n = addrlen / 8 + ((addrlen % 8) != 0);

			if (INADDRSZ > buflen) {
				RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
				return (NULL);
			}
			memset(bp, 0, INADDRSZ);
			memcpy(bp, addr, n);
			pvt->net.n_addr = bp;
			bp += INADDRSZ;
			buflen -= INADDRSZ;
		}
		break;
	default:
		abort();
	}

	/* Parse the answer, collect aliases. */
	ap = pvt->ali;
	haveanswer = 0;
	while (--ancount >= 0 && cp < eom) {
		int n = dn_expand(ansbuf, eom, cp, bp, buflen);

		cp += n;		/* Owner */
		if (n < 0 || !maybe_dnok(pvt->res, bp) ||
		    cp + 3 * INT16SZ + INT32SZ > eom) {
			RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
			return (NULL);
		}
		GETSHORT(type, cp);	/* Type */
		GETSHORT(class, cp);	/* Class */
		cp += INT32SZ;		/* TTL */
		GETSHORT(n, cp);	/* RDLENGTH */
		if (class == C_IN && type == T_PTR) {
			int nn;

			nn = dn_expand(ansbuf, eom, cp, bp, buflen);
			if (nn < 0 || !maybe_hnok(pvt->res, bp) || nn != n) {
				RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
				return (NULL);
			}
			normalize_name(bp);
			switch (by_what) {
			case by_addr: {
				if (pvt->net.n_name == NULL)
					pvt->net.n_name = bp;
				else if (ns_samename(pvt->net.n_name, bp) == 1)
					break;
				else
					*ap++ = bp;
				nn = strlen(bp) + 1;
				bp += nn;
				buflen -= nn;
				haveanswer++;
				break;
			    }
			case by_name: {
				u_int b1, b2, b3, b4;

				if (pvt->net.n_addr != NULL ||
				    sscanf(bp, "%u.%u.%u.%u.in-addr.arpa",
					   &b1, &b2, &b3, &b4) != 4)
					break;
				if (buflen < INADDRSZ) {
					RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
					return (NULL);
				}
				pvt->net.n_addr = bp;
				*bp++ = b4;
				*bp++ = b3;
				*bp++ = b2;
				*bp++ = b1;
				buflen -= INADDRSZ;
				pvt->net.n_length = INADDRSZ * 8;
				haveanswer++;
			    }
			}
		}
		cp += n;		/* RDATA */
	}
	if (!haveanswer) {
		RES_SET_H_ERRNO(pvt->res, TRY_AGAIN);
		return (NULL);
	}
	*ap = NULL;

	return (&pvt->net);
}

static struct nwent *
get1101mask(struct irs_nw *this, struct nwent *nwent) {
	struct pvt *pvt = (struct pvt *)this->private;
	char qbuf[sizeof "255.255.255.255.in-addr.arpa"], owner[MAXDNAME];
	int anslen, type, class, ancount, qdcount;
	u_char ansbuf[MAXPACKET], *cp, *eom;
	HEADER *hp;

	if (!nwent)
		return (NULL);
	if (make1101inaddr(nwent->n_addr, nwent->n_length, qbuf, sizeof qbuf)
	    < 0) {
		/* "First, do no harm." */
		return (nwent);
	}

	/* Query for the A RR that would hold this network's mask. */
	anslen = res_nquery(pvt->res, qbuf, C_IN, T_A, ansbuf, sizeof ansbuf);
	if (anslen < HFIXEDSZ)
		return (nwent);

	/* Initialize, and parse header. */
	hp = (HEADER *)ansbuf;
	cp = ansbuf + HFIXEDSZ;
	eom = ansbuf + anslen;
	qdcount = ntohs(hp->qdcount);
	while (qdcount-- > 0) {
		int n = dn_skipname(cp, eom);
		cp += n + QFIXEDSZ;
		if (n < 0 || cp > eom)
			return (nwent);
	}
	ancount = ntohs(hp->ancount);

	/* Parse the answer, collect aliases. */
	while (--ancount >= 0 && cp < eom) {
		int n = dn_expand(ansbuf, eom, cp, owner, sizeof owner);

		if (n < 0 || !maybe_dnok(pvt->res, owner))
			break;
		cp += n;		/* Owner */
		if (cp + 3 * INT16SZ + INT32SZ > eom)
			break;
		GETSHORT(type, cp);	/* Type */
		GETSHORT(class, cp);	/* Class */
		cp += INT32SZ;		/* TTL */
		GETSHORT(n, cp);	/* RDLENGTH */
		if (cp + n > eom)
			break;
		if (n == INADDRSZ && class == C_IN && type == T_A &&
		    ns_samename(qbuf, owner) == 1) {
			/* This A RR indicates the actual netmask. */
			int nn, mm;

			nwent->n_length = 0;
			for (nn = 0; nn < INADDRSZ; nn++)
				for (mm = 7; mm >= 0; mm--)
					if (cp[nn] & (1 << mm))
						nwent->n_length++;
					else
						break;
		}
		cp += n;		/* RDATA */
	}
	return (nwent);
}

static int
make1101inaddr(const u_char *net, int bits, char *name, int size) {
	int n, m;

	/* Zero fill any whole bytes left out of the prefix. */
	for (n = (32 - bits) / 8; n > 0; n--) {
		if (size < (int)(sizeof "0."))
			goto emsgsize;
		m = SPRINTF((name, "0."));
		name += m;
		size -= m;
	}

	/* Format the partial byte, if any, within the prefix. */
	if ((n = bits % 8) != 0) {
		if (size < (int)(sizeof "255."))
			goto emsgsize;
		m = SPRINTF((name, "%u.",
			     net[bits / 8] & ~((1 << (8 - n)) - 1)));
		name += m;
		size -= m;
	}

	/* Format the whole bytes within the prefix. */
	for (n = bits / 8; n > 0; n--) {
		if (size < (int)(sizeof "255."))
			goto emsgsize;
		m = SPRINTF((name, "%u.", net[n - 1]));
		name += m;
		size -= m;
	}

	/* Add the static text. */
	if (size < (int)(sizeof "in-addr.arpa"))
		goto emsgsize;
	(void) SPRINTF((name, "in-addr.arpa"));
	return (0);

 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}

static void
normalize_name(char *name) {
	char *t;

	/* Make lower case. */
	for (t = name; *t; t++)
		if (isascii(*t) && isupper(*t))
			*t = tolower(*t);

	/* Remove trailing dots. */
	while (t > name && t[-1] == '.')
		*--t = '\0';
}

static int
init(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (!pvt->res && !nw_res_get(this))
		return (-1);
	if (((pvt->res->options & RES_INIT) == 0) &&
	    res_ninit(pvt->res) == -1)
		return (-1);
	return (0);
}
