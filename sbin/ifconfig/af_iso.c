/*	$NetBSD: af_iso.c,v 1.4.18.1 2008/06/02 13:21:22 mjf Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 */

#ifndef INET_ONLY

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: af_iso.c,v 1.4.18.1 2008/06/02 13:21:22 mjf Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 

#include <err.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "parse.h"
#include "extern.h"
#include "af_iso.h"

#define	DEFNSELLEN	1

struct	iso_ifreq	iso_ridreq = {.ifr_Addr = {.siso_tlen = DEFNSELLEN}};
struct	iso_aliasreq	iso_addreq = {.ifra_dstaddr = {.siso_tlen = DEFNSELLEN},
			              .ifra_addr = {.siso_tlen = DEFNSELLEN}};

#define SISO(x) ((struct sockaddr_iso *) &(x))
struct sockaddr_iso *sisotab[] = {
    SISO(iso_ridreq.ifr_Addr), SISO(iso_addreq.ifra_addr),
    SISO(iso_addreq.ifra_mask), SISO(iso_addreq.ifra_dstaddr)};

static void adjust_nsellength(uint8_t);

struct pinteger parse_snpaoffset = PINTEGER_INITIALIZER1(&snpaoffset,
    "snpaoffset", INT_MIN, INT_MAX, 10, setsnpaoffset, "snpaoffset",
    &command_root.pb_parser);
struct pinteger parse_nsellength = PINTEGER_INITIALIZER1(&nsellength,
    "nsellength", 0, UINT8_MAX, 10, setnsellength, "nsellength",
    &command_root.pb_parser);

static const struct kwinst isokw[] = {
	  {.k_word = "nsellength", .k_nextparser = &parse_nsellength.pi_parser}
	, {.k_word = "snpaoffset", .k_nextparser = &parse_snpaoffset.pi_parser}
};

struct pkw iso = PKW_INITIALIZER(&iso, "ISO", NULL, NULL,
    isokw, __arraycount(isokw), NULL);

void
iso_getaddr(const struct paddr_prefix *pfx, int which)
{
	struct sockaddr_iso *siso = sisotab[which];

	siso->siso_addr =
	    ((const struct sockaddr_iso *)&pfx->pfx_addr)->siso_addr;

	if (which == MASK) {
		siso->siso_len = TSEL(siso) - (char *)(siso);
		siso->siso_nlen = 0;
	}
}

int
setsnpaoffset(prop_dictionary_t env, prop_dictionary_t xenv)
{
	int64_t snpaoffset;

	if (!prop_dictionary_get_int64(env, "snpaoffset", &snpaoffset)) {
		errno = ENOENT;
		return -1;
	}
	iso_addreq.ifra_snpaoffset = snpaoffset;
	return 0;
}

int
setnsellength(prop_dictionary_t env, prop_dictionary_t xenv)
{
	int af;
	uint8_t nsellength;

	if ((af = getaf(env)) == -1 || af != AF_ISO)
		errx(EXIT_FAILURE, "Setting NSEL length valid only for iso");

	if (!prop_dictionary_get_uint8(env, "nsellength", &nsellength)) {
		errno = ENOENT;
		return -1;
	}
	adjust_nsellength(nsellength);
	return 0;
}

static void
fixnsel(struct sockaddr_iso *siso, uint8_t nsellength)
{
	siso->siso_tlen = nsellength;
}

static void
adjust_nsellength(uint8_t nsellength)
{
	fixnsel(sisotab[RIDADDR], nsellength);
	fixnsel(sisotab[ADDR], nsellength);
	fixnsel(sisotab[DSTADDR], nsellength);
}

void
iso_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	struct sockaddr_iso *siso;
	struct iso_ifreq isoifr;
	int s;
	const char *ifname;
	unsigned short flags;

	if ((ifname = getifinfo(env, oenv, &flags)) == NULL)
		err(EXIT_FAILURE, "%s: getifinfo", __func__);

	if ((s = getsock(AF_ISO)) == -1) {
		if (errno == EAFNOSUPPORT)
			return;
		err(EXIT_FAILURE, "socket");
	}
	memset(&isoifr, 0, sizeof(isoifr));
	estrlcpy(isoifr.ifr_name, ifname, sizeof(isoifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR_ISO, &isoifr) == -1) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			(void) memset(&isoifr.ifr_Addr, 0,
			    sizeof(isoifr.ifr_Addr));
		} else
			warn("SIOCGIFADDR_ISO");
	}
	strlcpy(isoifr.ifr_name, ifname, sizeof isoifr.ifr_name);
	siso = &isoifr.ifr_Addr;
	printf("\tiso %s ", iso_ntoa(&siso->siso_addr));
	if (ioctl(s, SIOCGIFNETMASK_ISO, &isoifr) == -1) {
		if (errno == EADDRNOTAVAIL)
			memset(&isoifr.ifr_Addr, 0, sizeof(isoifr.ifr_Addr));
		else
			warn("SIOCGIFNETMASK_ISO");
	} else {
		if (siso->siso_len > offsetof(struct sockaddr_iso, siso_addr))
			siso->siso_addr.isoa_len = siso->siso_len
			    - offsetof(struct sockaddr_iso, siso_addr);
		printf("\n\t\tnetmask %s ", iso_ntoa(&siso->siso_addr));
	}

	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR_ISO, &isoifr) == -1) {
			if (errno == EADDRNOTAVAIL)
			    memset(&isoifr.ifr_Addr, 0,
				sizeof(isoifr.ifr_Addr));
			else
			    warn("SIOCGIFDSTADDR_ISO");
		}
		strlcpy(isoifr.ifr_name, ifname, sizeof (isoifr.ifr_name));
		siso = &isoifr.ifr_Addr;
		printf("--> %s ", iso_ntoa(&siso->siso_addr));
	}
	printf("\n");
}

#endif /* ! INET_ONLY */
