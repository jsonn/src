/*	$NetBSD: bootp.c,v 1.28.4.1 2006/09/09 02:57:53 rpaulo Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	California, Lawrence Berkeley Laboratory and its contributors.
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
 * @(#) Header: bootp.c,v 1.4 93/09/11 03:13:51 leres Exp  (LBL)
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "net.h"
#include "bootp.h"

struct in_addr servip;
#ifdef SUPPORT_LINUX
char linuxcmdline[256];
#ifndef TAG_LINUX_CMDLINE
#define TAG_LINUX_CMDLINE 123
#endif
#endif

static n_long	nmask, smask;

static time_t	bot;

static	char vm_rfc1048[4] = VM_RFC1048;
#ifdef BOOTP_VEND_CMU
static	char vm_cmu[4] = VM_CMU;
#endif

/* Local forwards */
static	ssize_t bootpsend __P((struct iodesc *, void *, size_t));
static	ssize_t bootprecv __P((struct iodesc *, void *, size_t, time_t));
static	int vend_rfc1048 __P((u_char *, u_int));
#ifdef BOOTP_VEND_CMU
static	void vend_cmu __P((u_char *));
#endif

#ifdef SUPPORT_DHCP
static char expected_dhcpmsgtype = -1, dhcp_ok;
struct in_addr dhcp_serverip;
#endif

/*
 * Boot programs can patch this at run-time to change the behavior
 * of bootp/dhcp.
 */
int bootp_flags;

/* Fetch required bootp information */
void
bootp(sock)
	int sock;
{
	struct iodesc *d;
	struct bootp *bp;
	struct {
		u_char header[HEADER_SIZE];
		struct bootp wbootp;
	} wbuf;
	struct {
		u_char header[HEADER_SIZE];
		struct bootp rbootp;
	} rbuf;
#ifdef SUPPORT_DHCP
	char vci[64];
	int vcilen;
#endif

#ifdef BOOTP_DEBUG
 	if (debug)
		printf("bootp: socket=%d\n", sock);
#endif
	if (!bot)
		bot = getsecs();

	if (!(d = socktodesc(sock))) {
		printf("bootp: bad socket. %d\n", sock);
		return;
	}
#ifdef BOOTP_DEBUG
 	if (debug)
		printf("bootp: d=%lx\n", (long)d);
#endif

	bp = &wbuf.wbootp;
	bzero(bp, sizeof(*bp));

	bp->bp_op = BOOTREQUEST;
	bp->bp_htype = 1;		/* 10Mb Ethernet (48 bits) */
	bp->bp_hlen = 6;
	bp->bp_xid = htonl(d->xid);
	MACPY(d->myea, bp->bp_chaddr);
	strncpy((char *)bp->bp_file, bootfile, sizeof(bp->bp_file));
	bcopy(vm_rfc1048, bp->bp_vend, sizeof(vm_rfc1048));
#ifdef SUPPORT_DHCP
	bp->bp_vend[4] = TAG_DHCP_MSGTYPE;
	bp->bp_vend[5] = 1;
	bp->bp_vend[6] = DHCPDISCOVER;
	/*
	 * Insert a NetBSD Vendor Class Identifier option.
	 */
	sprintf(vci, "NetBSD:%s:libsa", MACHINE);
	vcilen = strlen(vci);
	bp->bp_vend[7] = TAG_CLASSID;
	bp->bp_vend[8] = vcilen;
	bcopy(vci, &bp->bp_vend[9], vcilen);
	bp->bp_vend[9 + vcilen] = TAG_END;
#else
	bp->bp_vend[4] = TAG_END;
#endif

	d->myip.s_addr = INADDR_ANY;
	d->myport = htons(IPPORT_BOOTPC);
	d->destip.s_addr = INADDR_BROADCAST;
	d->destport = htons(IPPORT_BOOTPS);

#ifdef SUPPORT_DHCP
	expected_dhcpmsgtype = DHCPOFFER;
	dhcp_ok = 0;
#endif

	if (sendrecv(d,
		    bootpsend, bp, sizeof(*bp),
		    bootprecv, &rbuf.rbootp, sizeof(rbuf.rbootp))
	   == -1) {
		printf("bootp: no reply\n");
		return;
	}

#ifdef SUPPORT_DHCP
	if (dhcp_ok) {
		u_int32_t leasetime;
		bp->bp_vend[6] = DHCPREQUEST;
		bp->bp_vend[7] = TAG_REQ_ADDR;
		bp->bp_vend[8] = 4;
		bcopy(&rbuf.rbootp.bp_yiaddr, &bp->bp_vend[9], 4);
		bp->bp_vend[13] = TAG_SERVERID;
		bp->bp_vend[14] = 4;
		bcopy(&dhcp_serverip.s_addr, &bp->bp_vend[15], 4);
		bp->bp_vend[19] = TAG_LEASETIME;
		bp->bp_vend[20] = 4;
		leasetime = htonl(300);
		bcopy(&leasetime, &bp->bp_vend[21], 4);
		/*
		 * Insert a NetBSD Vendor Class Identifier option.
		 */
		sprintf(vci, "NetBSD:%s:libsa", MACHINE);
		vcilen = strlen(vci);
		bp->bp_vend[25] = TAG_CLASSID;
		bp->bp_vend[26] = vcilen;
		bcopy(vci, &bp->bp_vend[27], vcilen);
		bp->bp_vend[27 + vcilen] = TAG_END;

		expected_dhcpmsgtype = DHCPACK;

		if (sendrecv(d,
			    bootpsend, bp, sizeof(*bp),
			    bootprecv, &rbuf.rbootp, sizeof(rbuf.rbootp))
		   == -1) {
			printf("DHCPREQUEST failed\n");
			return;
		}
	}
#endif

	myip = d->myip = rbuf.rbootp.bp_yiaddr;
	servip = rbuf.rbootp.bp_siaddr;
	if (rootip.s_addr == INADDR_ANY)
		rootip = servip;
	bcopy(rbuf.rbootp.bp_file, bootfile, sizeof(bootfile));
	bootfile[sizeof(bootfile) - 1] = '\0';

	if (IN_CLASSA(myip.s_addr))
		nmask = IN_CLASSA_NET;
	else if (IN_CLASSB(myip.s_addr))
		nmask = IN_CLASSB_NET;
	else
		nmask = IN_CLASSC_NET;
#ifdef BOOTP_DEBUG
	if (debug)
		printf("'native netmask' is %s\n", intoa(nmask));
#endif

	/* Get subnet (or natural net) mask */
	netmask = nmask;
	if (smask)
		netmask = smask;
#ifdef BOOTP_DEBUG
	if (debug)
		printf("mask: %s\n", intoa(netmask));
#endif

	/* We need a gateway if root is on a different net */
	if (!SAMENET(myip, rootip, netmask)) {
#ifdef BOOTP_DEBUG
		if (debug)
			printf("need gateway for root ip\n");
#endif
	}

	/* Toss gateway if on a different net */
	if (!SAMENET(myip, gateip, netmask)) {
#ifdef BOOTP_DEBUG
		if (debug)
			printf("gateway ip (%s) bad\n", inet_ntoa(gateip));
#endif
		gateip.s_addr = 0;
	}

	printf("net_open: client addr: %s\n", inet_ntoa(myip));
	if (smask)
		printf("net_open: subnet mask: %s\n", intoa(smask));
	if (gateip.s_addr != 0)
		printf("net_open: net gateway: %s\n", inet_ntoa(gateip));
	printf("net_open: server addr: %s\n", inet_ntoa(rootip));
	if (rootpath[0] != '\0')
		printf("net_open: server path: %s\n", rootpath);
	if (bootfile[0] != '\0')
		printf("net_open: file name: %s\n", bootfile);

	/* Bump xid so next request will be unique. */
	++d->xid;
}

/* Transmit a bootp request */
static ssize_t
bootpsend(d, pkt, len)
	struct iodesc *d;
	void *pkt;
	size_t len;
{
	struct bootp *bp;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootpsend: d=%lx called.\n", (long)d);
#endif

	bp = pkt;
	bp->bp_secs = htons((u_short)(getsecs() - bot));

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootpsend: calling sendudp\n");
#endif

	return (sendudp(d, pkt, len));
}

static ssize_t
bootprecv(d, pkt, len, tleft)
	struct iodesc *d;
	void *pkt;
	size_t len;
	time_t tleft;
{
	ssize_t n;
	struct bootp *bp;

#ifdef BOOTP_DEBUGx
	if (debug)
		printf("bootp_recvoffer: called\n");
#endif

	n = readudp(d, pkt, len, tleft);
	if (n == -1 || (size_t)n < sizeof(struct bootp) - BOOTP_VENDSIZE)
		goto bad;

	bp = (struct bootp *)pkt;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootprecv: checked.  bp = 0x%lx, n = %d\n",
		    (long)bp, (int)n);
#endif
	if (bp->bp_xid != htonl(d->xid)) {
#ifdef BOOTP_DEBUG
		if (debug) {
			printf("bootprecv: expected xid 0x%lx, got 0x%x\n",
			    d->xid, ntohl(bp->bp_xid));
		}
#endif
		goto bad;
	}

	/* protect against bogus addresses sent by DHCP servers */
	if (bp->bp_yiaddr.s_addr == INADDR_ANY ||
	    bp->bp_yiaddr.s_addr == INADDR_BROADCAST)
		goto bad;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootprecv: got one!\n");
#endif

	/* Suck out vendor info */
	if (memcmp(vm_rfc1048, bp->bp_vend, sizeof(vm_rfc1048)) == 0) {
		if (vend_rfc1048(bp->bp_vend, sizeof(bp->bp_vend)) != 0)
			goto bad;
	}
#ifdef BOOTP_VEND_CMU
	else if (memcmp(vm_cmu, bp->bp_vend, sizeof(vm_cmu)) == 0)
		vend_cmu(bp->bp_vend);
#endif
	else
		printf("bootprecv: unknown vendor 0x%lx\n", (long)bp->bp_vend);

	return (n);
bad:
	errno = 0;
	return (-1);
}

static int
vend_rfc1048(cp, len)
	u_char *cp;
	u_int len;
{
	u_char *ep;
	int size;
	u_char tag;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("vend_rfc1048 bootp info. len=%d\n", len);
#endif
	ep = cp + len;

	/* Step over magic cookie */
	cp += sizeof(int);

	while (cp < ep) {
		tag = *cp++;
		size = *cp++;
		if (tag == TAG_END)
			break;

		if (tag == TAG_SUBNET_MASK) {
			bcopy(cp, &smask, sizeof(smask));
		}
		if (tag == TAG_GATEWAY) {
			bcopy(cp, &gateip.s_addr, sizeof(gateip.s_addr));
		}
		if (tag == TAG_SWAPSERVER) {
			/* let it override bp_siaddr */
			bcopy(cp, &rootip.s_addr, sizeof(rootip.s_addr));
		}
		if (tag == TAG_ROOTPATH) {
			strncpy(rootpath, (char *)cp, sizeof(rootpath));
			rootpath[size] = '\0';
		}
		if (tag == TAG_HOSTNAME) {
			strncpy(hostname, (char *)cp, sizeof(hostname));
			hostname[size] = '\0';
		}
#ifdef SUPPORT_DHCP
		if (tag == TAG_DHCP_MSGTYPE) {
			if (*cp != expected_dhcpmsgtype)
				return (-1);
			dhcp_ok = 1;
		}
		if (tag == TAG_SERVERID) {
			bcopy(cp, &dhcp_serverip.s_addr,
			      sizeof(dhcp_serverip.s_addr));
		}
#endif
#ifdef SUPPORT_LINUX
		if (tag == TAG_LINUX_CMDLINE) {
			strncpy(linuxcmdline, (char *)cp, sizeof(linuxcmdline));
			linuxcmdline[size] = '\0';
		}
#endif
		cp += size;
	}
	return (0);
}

#ifdef BOOTP_VEND_CMU
static void
vend_cmu(cp)
	u_char *cp;
{
	struct cmu_vend *vp;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("vend_cmu bootp info.\n");
#endif
	vp = (struct cmu_vend *)cp;

	if (vp->v_smask.s_addr != 0) {
		smask = vp->v_smask.s_addr;
	}
	if (vp->v_dgate.s_addr != 0) {
		gateip = vp->v_dgate;
	}
}
#endif
