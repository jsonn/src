/* $NetBSD: if_prom.c,v 1.13.8.1 1999/12/27 18:31:30 wrstuden Exp $ */

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>
#include <machine/prom.h>
#include <lib/libkern/libkern.h>

#include "stand/common/common.h"
#include "stand/common/bbinfo.h"

int prom_probe();
int prom_match();
void prom_init();
int prom_get();
int prom_put();
void prom_end();

extern struct netif_stats	prom_stats[];

struct netif_dif prom_ifs[] = {
/*	dif_unit	dif_nsel	dif_stats	dif_private	*/
{	0,		1,		&prom_stats[0],	0,		},
};

struct netif_stats prom_stats[NENTS(prom_ifs)];

struct netbbinfo netbbinfo = {
	0xfeedbabedeadbeef,			/* magic number */
	0,					/* set */
	{0, 0, 0, 0, 0, 0},			/* ether address */
	0,					/* force */
	{ 0, },					/* pad2 */
	0,					/* cksum */
	0xfeedbeefdeadbabe,			/* magic number */
};

struct netif_driver prom_netif_driver = {
	"prom",			/* netif_bname */
	prom_match,		/* netif_match */
	prom_probe,		/* netif_probe */
	prom_init,		/* netif_init */
	prom_get,		/* netif_get */
	prom_put,		/* netif_put */
	prom_end,		/* netif_end */
	prom_ifs,		/* netif_ifs */
	NENTS(prom_ifs)		/* netif_nifs */
};

int broken_firmware;

int
prom_match(nif, machdep_hint)
	struct netif *nif;
	void *machdep_hint;
{

	return (1);
}

int
prom_probe(nif, machdep_hint)
	struct netif *nif;
	void *machdep_hint;
{

	return 0;
}

int
prom_put(desc, pkt, len)
	struct iodesc *desc;
	void *pkt;
	int len;
{

	prom_write(booted_dev_fd, len, pkt, 0);

	return len;
}


int
prom_get(desc, pkt, len, timeout)
	struct iodesc *desc;
	void *pkt;
	int len;
	time_t timeout;
{
	prom_return_t ret;
	time_t t;
	int cc;
	char hate[2000];

	t = getsecs();
	cc = 0;
	while (((getsecs() - t) < timeout) && !cc) {
		if (broken_firmware)
			ret.bits = prom_read(booted_dev_fd, 0, hate, 0);
		else
			ret.bits = prom_read(booted_dev_fd, sizeof hate, hate, 0);
		if (ret.u.status == 0)
			cc = ret.u.retval;
	}
	if (broken_firmware)
		cc = min(cc, len);
	else
		cc = len;
	bcopy(hate, pkt, cc);

	return cc;
}

extern char *strchr();

void
prom_init(desc, machdep_hint)
	struct iodesc *desc;
	void *machdep_hint;
{
	int i, netbbinfovalid;
	const char *enet_addr;
	u_int64_t *qp, csum;

	broken_firmware = 0;

	csum = 0;
	for (i = 0, qp = (u_int64_t *)&netbbinfo;
	    i < (sizeof netbbinfo / sizeof (u_int64_t)); i++, qp++)
		csum += *qp;
	netbbinfovalid = (csum == 0);
	if (netbbinfovalid)
		netbbinfovalid = netbbinfo.set;

#if 0
	printf("netbbinfo ");
	if (!netbbinfovalid)
		printf("invalid\n");
	else
		printf("valid: force = %d, ea = %s\n", netbbinfo.force,
		    ether_sprintf(netbbinfo.ether_addr));
#endif

	/* Ethernet address is the 9th component of the booted_dev string. */
	enet_addr = booted_dev_name;
	for (i = 0; i < 8; i++) {
		enet_addr = strchr(enet_addr, ' ');
		if (enet_addr == NULL) {
			printf("boot: boot device name does not contain ethernet address.\n");
			goto punt;
		}
		enet_addr++;
	}
	if (enet_addr != NULL) {
		int hv, lv;

#define	dval(c)	(((c) >= '0' && (c) <= '9') ? ((c) - '0') : \
		 (((c) >= 'A' && (c) <= 'F') ? (10 + (c) - 'A') : \
		  (((c) >= 'a' && (c) <= 'f') ? (10 + (c) - 'a') : -1)))

		for (i = 0; i < 6; i++) {
			hv = dval(*enet_addr); enet_addr++;
			lv = dval(*enet_addr); enet_addr++;
			enet_addr++;

			if (hv == -1 || lv == -1) {
				printf("boot: boot device name contains bogus ethernet address.\n");
				goto punt;
			}

			desc->myea[i] = (hv << 4) | lv;
		}
#undef dval
	}

	if (netbbinfovalid && netbbinfo.force) {
		printf("boot: using hard-coded ethernet address (forced).\n");
		bcopy(netbbinfo.ether_addr, desc->myea, sizeof desc->myea);
	}

gotit:
	printf("boot: ethernet address: %s\n", ether_sprintf(desc->myea));
	return;

punt:
	broken_firmware = 1;
        if (netbbinfovalid) {
                printf("boot: using hard-coded ethernet address.\n");
                bcopy(netbbinfo.ether_addr, desc->myea, sizeof desc->myea);
                goto gotit;
        }

	printf("\n");
	printf("Boot device name was: \"%s\"\n", booted_dev_name);
	printf("\n");
	printf("Your firmware may be too old to network-boot NetBSD/alpha,\n");
	printf("or you might have to hard-code an ethernet address into\n");
	printf("your network boot block with setnetbootinfo(8).\n");

	booted_dev_close();
	halt();
}

void
prom_end(nif)
	struct netif *nif;
{

	/* nothing to do */
}
