/*	$NetBSD: link_proto.c,v 1.2.6.1 2007/11/06 23:33:38 matt Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)uipc_proto.c	8.2 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: link_proto.c,v 1.2.6.1 2007/11/06 23:33:38 matt Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/un.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/raw_cb.h>

static int sockaddr_dl_cmp(const struct sockaddr *, const struct sockaddr *);

/*
 * Definitions of protocols supported in the link-layer domain.
 */

DOMAIN_DEFINE(linkdomain);	/* forward define and add to link set */

struct domain linkdomain = {
	.dom_family = AF_LINK,
	.dom_name = "link",
	.dom_externalize = NULL,
	.dom_dispose = NULL,
	.dom_protosw = NULL,
	.dom_protoswNPROTOSW = NULL,
	.dom_sockaddr_cmp = sockaddr_dl_cmp
};

/* Compare the field at byte offsets [fieldstart, fieldend) in
 * two memory regions, [l, l + llen) and [r, r + llen).
 */
static inline int
submemcmp(const void *l, const void *r,
    const uint_fast8_t llen, const uint_fast8_t rlen,
    const uint_fast8_t fieldstart, const uint_fast8_t fieldend)
{
	uint_fast8_t cmpend, minlen;
	const uint8_t *lb = l, *rb = r;
	int rc;

	minlen = MIN(llen, rlen);

	/* The field is missing from one region.  The shorter region is the
	 * lesser region.
	 */
	if (fieldstart >= minlen)
		return llen - rlen;

	/* Two empty, present fields are always equal. */
	if (fieldstart > fieldend)
		return 0;

	cmpend = MIN(fieldend, minlen);

	rc = memcmp(&lb[fieldstart], &rb[fieldstart], cmpend - fieldstart);

	if (rc != 0)
		return rc;
	/* If one or both fields are truncated, then the shorter is the lesser
	 * field.
	 */
	if (minlen < fieldend)
		return llen - rlen;
	/* Fields are full-length and equal.  The fields are equal. */
	return 0;
}

uint8_t
sockaddr_dl_measure(uint8_t namelen, uint8_t addrlen)
{
	return offsetof(struct sockaddr_dl, sdl_data[namelen + addrlen]);
}

struct sockaddr *
sockaddr_dl_alloc(uint16_t ifindex, uint8_t type,
    const void *name, uint8_t namelen, const void *addr, uint8_t addrlen,
    int flags)
{
	struct sockaddr *sa;
	socklen_t len;

	len = sockaddr_dl_measure(namelen, addrlen);
	sa = sockaddr_alloc(AF_LINK, len, flags);

	if (sa == NULL)
		return NULL;

	if (sockaddr_dl_init(satosdl(sa), len, ifindex, type, name, namelen,
	    addr, addrlen) == NULL) {
		sockaddr_free(sa);
		return NULL;
	}

	return sa;
}

struct sockaddr_dl *
sockaddr_dl_init(struct sockaddr_dl *sdl, socklen_t socklen, uint16_t ifindex,
    uint8_t type, const void *name, uint8_t namelen, const void *addr,
    uint8_t addrlen)
{
	socklen_t len;

	sdl->sdl_family = AF_LINK;
	sdl->sdl_slen = 0;
	len = sockaddr_dl_measure(namelen, addrlen);
	if (len > socklen) {
		sdl->sdl_len = socklen;
#ifdef DIAGNOSTIC
		printf("%s: too long: %" PRIu8 " > %" PRIu8 "\n", __func__, len,
		    socklen);
#endif
		return NULL;
	}
	sdl->sdl_len = len;
	sdl->sdl_index = ifindex;
	sdl->sdl_type = type;
	memset(&sdl->sdl_data[0], 0, namelen + addrlen);
	if (name != NULL) {
		memcpy(&sdl->sdl_data[0], name, namelen);
		sdl->sdl_nlen = namelen;
	} else
		sdl->sdl_nlen = 0;
	if (addr != NULL) {
		memcpy(&sdl->sdl_data[sdl->sdl_nlen], addr, addrlen);
		sdl->sdl_alen = addrlen;
	} else
		sdl->sdl_alen = 0;
	return sdl;
}

static int
sockaddr_dl_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	int rc;
	const uint_fast8_t indexofs = offsetof(struct sockaddr_dl, sdl_index);
	const uint_fast8_t nlenofs = offsetof(struct sockaddr_dl, sdl_nlen);
	uint_fast8_t dataofs = offsetof(struct sockaddr_dl, sdl_data[0]);
	const struct sockaddr_dl *sdl1, *sdl2;

	sdl1 = satocsdl(sa1);
	sdl2 = satocsdl(sa2);

	rc = submemcmp(sdl1, sdl2, sdl1->sdl_len, sdl2->sdl_len,
	    indexofs, nlenofs);

	if (rc != 0)
		return rc;

	rc = submemcmp(sdl1, sdl2, sdl1->sdl_len, sdl2->sdl_len,
	    dataofs, dataofs + MIN(sdl1->sdl_nlen, sdl2->sdl_nlen));

	if (rc != 0)
		return rc;

	if (sdl1->sdl_nlen != sdl2->sdl_nlen)
		return sdl1->sdl_nlen - sdl2->sdl_nlen;

	dataofs += sdl1->sdl_nlen;

	rc = submemcmp(sdl1, sdl2, sdl1->sdl_len, sdl2->sdl_len,
	    dataofs, dataofs + MIN(sdl1->sdl_alen, sdl2->sdl_alen));

	if (rc != 0)
		return rc;

	if (sdl1->sdl_alen != sdl2->sdl_alen)
		return sdl1->sdl_alen - sdl2->sdl_alen;

	dataofs += sdl1->sdl_alen;

	rc = submemcmp(sdl1, sdl2, sdl1->sdl_len, sdl2->sdl_len,
	    dataofs, dataofs + MIN(sdl1->sdl_slen, sdl2->sdl_slen));

	if (sdl1->sdl_slen != sdl2->sdl_slen)
		return sdl1->sdl_slen - sdl2->sdl_slen;

	return sdl1->sdl_len - sdl2->sdl_len;
}

struct sockaddr_dl *
sockaddr_dl_setaddr(struct sockaddr_dl *sdl, socklen_t socklen,
    const void *addr, uint8_t addrlen)
{
	socklen_t len;

	len = sockaddr_dl_measure(sdl->sdl_nlen, addrlen);
	if (len > socklen) {
#ifdef DIAGNOSTIC
		printf("%s: too long: %" PRIu8 " > %" PRIu8 "\n", __func__, len,
		    socklen);
#endif
		return NULL;
	}
	memcpy(&sdl->sdl_data[sdl->sdl_nlen], addr, addrlen);
	sdl->sdl_alen = addrlen;
	sdl->sdl_len = len;
	return sdl;
}
