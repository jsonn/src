/*
 * Copyright (C) 1993-1996 Wolfgang Solfrank.
 * Copyright (C) 1993-1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>

#include <machine/stdarg.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

#include <ipkdb/ipkdb.h>
#include "debuggers.h"
#include <machine/ipkdb.h>

int ipkdbpanic = 0;

#ifdef	IPKDBUSER
char ipkdbuser = 0;	/* allows debugging of user processes by IPKDB when set */
#endif

static struct ipkdb_if ipkdb_if;
#ifdef	IPKDBTEST
static struct ipkdb_if new_if;
static int ipkdb_test = 0;
#endif

static u_char *ipkdbaddr __P((u_char *, int *, void **));
static void peekmem __P((struct ipkdb_if *, u_char *, void *, long));
static void pokemem __P((struct ipkdb_if *, u_char *, void *, long));
static u_int32_t getnl __P((void *));
static u_int getns __P((void *));
static void setnl __P((void *, u_int32_t));
static void setns __P((void *, int));
static u_short cksum __P((u_short, void *, int));
static int assemble __P((struct ipkdb_if *, void *));
static char *inpkt __P((struct ipkdb_if *, char *, int));
static void outpkt __P((struct ipkdb_if *, char *, int, int, int));
static void init __P((struct ipkdb_if *));
static int chksum __P((char *, int));
static void getpkt __P((struct ipkdb_if *, char *, int *));
static void putpkt __P((struct ipkdb_if *, char *, int));
static int maskcmp __P((void *, void *, void *));
static int check_ipkdb __P((struct ipkdb_if *, struct in_addr *, u_short, u_short, char *, int));
static int connectipkdb __P((struct ipkdb_if *, char *));

void
ipkdb_init()
{
	ipkdb_if.connect = IPKDB_DEF;
	ipkdbinit();
	if (   ipkdbifinit(&ipkdb_if, 0) < 0
	    || !(ipkdb_if.flags&IPKDB_MYHW)) {
		/* Interface not found, drop IPKDB */
		printf("IPKDB: No interface found!\n");
		ipkdb_if.connect = IPKDB_NOIF;
		boothowto &= ~RB_KDB;
	}
}

void
ipkdb_connect(when)
	int when;
{
	boothowto |= RB_KDB;
	if (when == 0)
		printf("waiting for remote debugger\n");
	ipkdb_trap();
#ifdef	IPKDBTEST
	new_if.connect = IPKDB_ALL;
	if (   ipkdbifinit(&new_if, 1) < 0
	    || !(new_if.flags&IPKDB_MYHW)) {
		/* Interface not found, no test */
		return;
	}
	init(&new_if);
	putpkt(&new_if, "s", 1);
	for (ipkdb_test = 1; ipkdb_test;) {
		static char buf[512];
		int plen;

		getpkt(&new_if, buf, &plen);
		if (!plen)
			continue;
		putpkt(&new_if, "eunknown command", 16);
	}
#endif
}

void
ipkdb_panic()
{
	if (ipkdb_if.connect == IPKDB_NOIF)
		return;
	ipkdbpanic = 1;
	ipkdb_trap();
}

int
ipkdbcmds()
{
	static char buf[512];
	char *cp;
	int plen;

	if (!(ipkdb_if.flags&IPKDB_MYHW))	/* no interface */
		return 2;
	init(&ipkdb_if);
	if (ipkdbpanic > 1) {
		ipkdb_if.leave(&ipkdb_if);
		return 0;
	}
	putpkt(&ipkdb_if, "s", 1);
	while (1) {
		getpkt(&ipkdb_if, buf, &plen);
		if (!plen) {
			if (ipkdbpanic && ipkdb_poll()) {
				ipkdb_if.leave(&ipkdb_if);
				return 0;
			} else
				continue;
		} else
			ipkdbpanic = 0;
		switch (*buf) {
		default:
			putpkt(&ipkdb_if, "eunknown command", 16);
			break;
		case 'O':
			/* This is an allowed reconnect, ack it */
			putpkt(&ipkdb_if, "s", 1);
			break;
		case 'R':
			peekmem(&ipkdb_if, buf, ipkdbregs, sizeof ipkdbregs);
			break;
		case 'W':
			if (plen != sizeof ipkdbregs + 1) {
				putpkt(&ipkdb_if, "einvalid register size", 22);
				break;
			}
			pokemem(&ipkdb_if, buf + 1, ipkdbregs, sizeof ipkdbregs);
			break;
		case 'M':
			{
				void *addr, *len;

				plen--;
				if (   !(cp = ipkdbaddr(buf + 1, &plen, &addr))
				    || !ipkdbaddr(cp, &plen, &len)) {
					putpkt(&ipkdb_if, "einvalid peek format", 20);
					break;
				}
				peekmem(&ipkdb_if, buf, addr, (long)len);
				break;
			}
		case 'N':
			{
				void *addr, *len;

				plen--;
				if (   !(cp = ipkdbaddr(buf + 1, &plen, &addr))
				    || !(cp = ipkdbaddr(cp, &plen, &len))
				    || plen < (long)len) {
					putpkt(&ipkdb_if, "einvalid poke format", 20);
					break;
				}
				pokemem(&ipkdb_if, cp, addr, (long)len);
				break;
			}
		case 'S':
			ipkdb_if.leave(&ipkdb_if);
			return 1;
		case 'X':
			putpkt(&ipkdb_if, "ok",2);
#ifdef	IPKDBUSER
			ipkdbuser = 0;
#endif
			ipkdb_if.connect = IPKDB_DEF; /* ??? */
			ipkdb_if.leave(&ipkdb_if);
			return 2;
		case 'C':
			ipkdb_if.leave(&ipkdb_if);
			return 0;
		}
	}
}

static u_char *
ipkdbaddr(cp, pl, dp)
	u_char *cp;
	int *pl;
	void **dp;
{
	/* Assume that sizeof(void *) <= sizeof(u_long) */
	u_long l;
	int i;

	if ((*pl -= sizeof *dp) < 0)
		return 0;
	for (i = sizeof *dp, l = 0; --i >= 0;) {
		l <<= 8;
		l |= *cp++;
	}
	*dp = (void *)l;
	return cp;
}

static void
peekmem(ifp, buf, addr, len)
	struct ipkdb_if *ifp;
	u_char *buf;
	void *addr;
	long len;
{
	u_char *cp, *p = addr;
	int l;

	cp = buf;
	*cp++ = 'p';
	for (l = len; --l >= 0;)
		*cp++ = ipkdbfbyte(p++);
	putpkt(ifp, buf, len + 1);
}

static void
pokemem(ifp, cp, addr, len)
	struct ipkdb_if *ifp;
	u_char *cp;
	void *addr;
	long len;
{
	int c;
	u_char *p = addr;

	while (--len >= 0)
		ipkdbsbyte(p++, *cp++);
	putpkt(ifp, "ok", 2);
}

__inline static u_int32_t
getnl(vs)
	void *vs;
{
	u_char *s = vs;

	return (*s << 24)|(s[1] << 16)|(s[2] << 8)|s[3];
}

__inline static u_int
getns(vs)
	void *vs;
{
	u_char *s = vs;

	return (*s << 8)|s[1];
}

__inline static void
setnl(vs, l)
	void *vs;
	u_int32_t l;
{
	u_char *s = vs;

	*s++ = l >> 24;
	*s++ = l >> 16;
	*s++ = l >> 8;
	*s = l;
}

__inline static void
setns(vs, l)
	void *vs;
	int l;
{
	u_char *s = vs;

	*s++ = l >> 8;
	*s = l;
}

static u_short
cksum(st, vcp, l)
	u_short st;
	void *vcp;
	int l;
{
	u_char *cp = vcp;
	u_long s;

	for (s = st; (l -= 2) >= 0; cp += 2)
		s += (*cp << 8) + cp[1];
	if (l == -1)
		s += *cp << 8;
	while (s&0xffff0000)
		s = (s&0xffff) + (s >> 16);
	return s == 0xffff ? 0 : s;
}

static int
assemble(ifp, buf)
	struct ipkdb_if *ifp;
	void *buf;
{
	struct ip *ip, iph;
	int off, len, i;
	u_char *cp, *ecp;

	ip = (struct ip *)buf;
	ipkdbcopy(ip, &iph, sizeof iph);
	iph.ip_hl = 5;
	iph.ip_tos = 0;
	iph.ip_len = 0;
	iph.ip_off = 0;
	iph.ip_ttl = 0;
	iph.ip_sum = 0;
	if (ifp->asslen) {
		if (ipkdbcmp(&iph, ifp->ass, sizeof iph)) {
			/*
			 * different packet
			 * decide whether to keep the old
			 * or start a new one
			 */
			i = getns(&ip->ip_id)^getns(&((struct ip *)ifp->ass)->ip_id);
			i ^= (i >> 2)^(i >> 4)^(i >> 8)^(i >> 12);
			if (i&1)
				/* keep the old */
				return 0;
			ifp->asslen = 0;
		}
	}
	if (!ifp->asslen) {
		ipkdbzero(ifp->assbit, sizeof ifp->assbit);
		ipkdbcopy(&iph, ifp->ass, sizeof iph);
	}
	off = getns(&ip->ip_off);
	len = ((off&IP_OFFMASK) << 3) + getns(&ip->ip_len) - ip->ip_hl * 4;
	if (ifp->asslen < len)
		ifp->asslen = len;
	if (ifp->asslen + sizeof *ip > sizeof ifp->ass) {
		/* packet too long */
		ifp->asslen = 0;
		return 0;
	}
	if (!(off&IP_MF)) {
		off &= IP_OFFMASK;
		cp = ifp->assbit + (off >> 3);
		for (i = off & 7; i < 8; *cp |= 1 << i++);
		for (; cp < ifp->assbit + sizeof ifp->assbit; *cp++ = -1);
	} else {
		off &= IP_OFFMASK;
		cp = ifp->assbit + (off >> 3);
		ecp = ifp->assbit + (len >> 6);
		if (cp == ecp)
			for (i = off & 7; i <= (len >> 3)&7; *cp |= 1 << i++);
		else {
			for (i = off & 7; i < 8; *cp |= 1 << i++);
			for (; ++cp < ecp; *cp = -1);
			for (i = 0; i < ((len >> 3)&7); *cp |= 1 << i++);
		}
	}
	ipkdbcopy(buf + ip->ip_hl * 4,
		  ifp->ass + sizeof *ip + (off << 3),
		  len - (off << 3));
	for (cp = ifp->assbit; cp < ifp->assbit + sizeof ifp->assbit;)
		if (*cp++ != (u_char)-1)
			/* not complete */
			return 0;
	ip = (struct ip *)ifp->ass;
	setns(&ip->ip_len, sizeof *ip + ifp->asslen);
	/* complete */
	return 1;
}

static char *
inpkt(ifp, ibuf, poll)
	struct ipkdb_if *ifp;
	char *ibuf;
	int poll;
{
	int cnt = 1000000;
	int l, ul;
	struct ether_header *eh;
	struct ether_arp *ah;
	struct ip *ip;
	struct udphdr *udp;
	struct ipovly ipo;

	while (1) {
		l = ifp->receive(ifp, ibuf, poll != 0);
		if (!l) {
			if (poll == 1 || (poll == 2 && --cnt <= 0))
				break;
			else
				continue;
		}
		eh = (struct ether_header *)ibuf;
		switch (getns(&eh->ether_type)) {
		case ETHERTYPE_ARP:
			ah = (struct ether_arp *)(ibuf + 14);
			if (   getns(&ah->arp_hrd) != ARPHRD_ETHER
			    || getns(&ah->arp_pro) != ETHERTYPE_IP
			    || ah->arp_hln != 6
			    || ah->arp_pln != 4)
				/* unsupported arp packet */
				break;
			switch (getns(&ah->arp_op)) {
			case ARPOP_REQUEST:
				if (   (ifp->flags&IPKDB_MYIP)
				    && !ipkdbcmp(ah->arp_tpa,
						 ifp->myinetaddr,
						 sizeof ifp->myinetaddr)) {
					/* someone requested my address */
					ipkdbcopy(eh->ether_shost,
						  eh->ether_dhost,
						  sizeof eh->ether_dhost);
					ipkdbcopy(ifp->myenetaddr,
						  eh->ether_shost,
						  sizeof eh->ether_shost);
					setns(&ah->arp_op, ARPOP_REPLY);
					ipkdbcopy(ah->arp_sha,
						  ah->arp_tha,
						  sizeof ah->arp_tha);
					ipkdbcopy(ah->arp_spa,
						  ah->arp_tpa,
						  sizeof ah->arp_tpa);
					ipkdbcopy(ifp->myenetaddr,
						  ah->arp_sha,
						  sizeof ah->arp_sha);
					ipkdbcopy(ifp->myinetaddr,
						  ah->arp_spa,
						  sizeof ah->arp_spa);
					ifp->send(ifp, ibuf, 74);
					continue;
				}
				break;
			default:
				break;
			}
			break;
		case ETHERTYPE_IP:
			if (ipkdbcmp(eh->ether_dhost,
				     ifp->myenetaddr,
				     sizeof ifp->myenetaddr))
				/* not only for us */
				break;
			ip = (struct ip *)(ibuf + 14);
			if (   ip->ip_v != IPVERSION
			    || ip->ip_hl < 5
			    || getns(&ip->ip_len) + 14 > l)
				/* invalid packet */
				break;
			if (cksum(0, ip, ip->ip_hl * 4))
				/* wrong checksum */
				break;
			if (ip->ip_p != IPPROTO_UDP)
				break;
			if (getns(&ip->ip_off)&~IP_DF) {
				if (!assemble(ifp, ip))
					break;
				ip = (struct ip *)ifp->ass;
				ifp->asslen = 0;
			}
			udp = (struct udphdr *)((char *)ip + ip->ip_hl * 4);
			ul = getns(&ip->ip_len) - ip->ip_hl * 4;
			if (getns(&udp->uh_ulen) != ul)
				/* invalid UDP packet length */
				break;
			ipkdbcopy(ip, &ipo, sizeof ipo);
			ipkdbzero(ipo.ih_x1, sizeof ipo.ih_x1);
			ipo.ih_len = udp->uh_ulen;
			if (   udp->uh_sum
			    && cksum(cksum(0, &ipo, sizeof ipo), udp, ul))
				/* wrong checksum */
				break;
			if (!(ifp->flags&IPKDB_MYIP)) {
				if (   getns(&udp->uh_sport) == 67
				    && getns(&udp->uh_dport) == 68
				    && *(char *)(udp + 1) == 2) {
					/* this is a BOOTP reply to our ethernet address */
					/* should check a bit more?		XXX */
					ipkdbcopy(&ip->ip_dst,
						  ifp->myinetaddr,
						  sizeof ifp->myinetaddr);
					ifp->flags |= IPKDB_MYIP;
				}
				/* give caller a chance to resend his request */
				return 0;
			}
			if (   ipkdbcmp(&ip->ip_dst, ifp->myinetaddr, sizeof ifp->myinetaddr)
			    || getns(&udp->uh_sport) != IPKDBPORT
			    || getns(&udp->uh_dport) != IPKDBPORT)
				break;
			/* so now it's a UDP packet for the debugger */
			{
				/* Check for reconnect packet */
				u_char *p;

				p = (u_char *)(udp + 1);
				if (!getnl(p) && p[6] == 'O') {
					l = getns(p + 4);
					if (   l <= ul - sizeof *udp - 6
					    && check_ipkdb(ifp, &ip->ip_src, udp->uh_sport,
							   udp->uh_dport, p + 6, l)) {
						ipkdbcopy(&ip->ip_src,
							  ifp->hisinetaddr,
							  sizeof ifp->hisinetaddr);
						ipkdbcopy(eh->ether_shost,
							  ifp->hisenetaddr,
							  sizeof ifp->hisenetaddr);
						ifp->flags |= IPKDB_HISHW|IPKDB_HISIP;
						return p;
					}
				}
			}
			if (   (ifp->flags&IPKDB_HISIP)
			    && ipkdbcmp(&ip->ip_src,
					ifp->hisinetaddr, sizeof ifp->hisinetaddr))
				/* It's a packet from someone else */
				break;
			if (!(ifp->flags&IPKDB_HISIP)) {
				ifp->flags |= IPKDB_HISIP;
				ipkdbcopy(&ip->ip_src,
					  ifp->hisinetaddr, sizeof ifp->hisinetaddr);
			}
			if (!(ifp->flags&IPKDB_HISHW)) {
				ifp->flags |= IPKDB_HISHW;
				ipkdbcopy(eh->ether_shost,
					  ifp->hisenetaddr, sizeof ifp->hisenetaddr);
			}
			return (char *)(udp + 1);
		default:
			/* unknown type */
			break;
		}
		if (l)
			ipkdbgotpkt(ifp, ibuf, l);
	}
	return 0;
}

static short ipkdb_ipid = 0;

static void
outpkt(ifp, in, l, srcport, dstport)
	struct ipkdb_if *ifp;
	char *in;
	int l;
	int srcport, dstport;
{
	u_char *sp;
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	u_char *cp;
	char obuf[ETHERMTU+14];
	struct ipovly ipo;
	int i, off;

	ipkdbzero(obuf, sizeof obuf);
	eh = (struct ether_header *)obuf;
	if (!(ifp->flags&IPKDB_HISHW))
		for (cp = eh->ether_dhost; cp < eh->ether_dhost + sizeof eh->ether_dhost;)
			*cp++ = -1;
	else
		ipkdbcopy(ifp->hisenetaddr, eh->ether_dhost, sizeof eh->ether_dhost);
	ipkdbcopy(ifp->myenetaddr, eh->ether_shost, sizeof eh->ether_shost);
	setns(&eh->ether_type, ETHERTYPE_IP);
	ip = (struct ip *)(obuf + 14);
	ip->ip_v = IPVERSION;
	ip->ip_hl = 5;
	setns(&ip->ip_id, ipkdb_ipid++);
	ip->ip_ttl = 255;
	ip->ip_p = IPPROTO_UDP;
	ipkdbcopy(ifp->myinetaddr, &ip->ip_src, sizeof ip->ip_src);
	ipkdbcopy(ifp->hisinetaddr, &ip->ip_dst, sizeof ip->ip_dst);
	udp = (struct udphdr *)(ip + 1);
	setns(&udp->uh_sport, srcport);
	setns(&udp->uh_dport, dstport);
	setns(&udp->uh_ulen, l + sizeof *udp);
	ipkdbcopy(ip, &ipo, sizeof ipo);
	ipkdbzero(ipo.ih_x1, sizeof ipo.ih_x1);
	ipo.ih_len = udp->uh_ulen;
	setns(&udp->uh_sum,
	      ~cksum(cksum(cksum(0, &ipo, sizeof ipo),
			   udp, sizeof *udp),
		     in, l));
	for (cp = (u_char *)(udp + 1), l += sizeof *udp, off = 0;
	     l > 0;
	     l -= i, in += i, off += i, cp = (u_char *)udp) {
		i = l > ifp->mtu - sizeof *ip ? ((ifp->mtu - sizeof *ip)&~7) : l;
		ipkdbcopy(in, cp, i);
		setns(&ip->ip_len, i + sizeof *ip);
		setns(&ip->ip_off, (l > i ? IP_MF : 0)|(off >> 3));
		ip->ip_sum = 0;
		setns(&ip->ip_sum, ~cksum(0, ip, sizeof *ip));
		if (i + sizeof *ip < ETHERMIN)
			i = ETHERMIN - sizeof *ip;
		ifp->send(ifp, obuf, i + sizeof *ip + 14);
	}
}

static void
init(ifp)
	struct ipkdb_if *ifp;
{
	u_char *cp;
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	u_char buf[ETHERMTU+14];
	struct ipovly ipo;
	int secs = 0;

	ifp->start(ifp);
#ifdef	__notyet__
	if (!(ifp->flags&IPKDB_MYIP))
		ipkdbinet(ifp);
#endif
	if (ifp->flags&IPKDB_MYIP)
		return;

	while (!(ifp->flags&IPKDB_MYIP)) {
		ipkdbzero(buf, sizeof buf);
		cp = buf;
		*cp++ = 1;		/* BOOTP_REQUEST */
		*cp++ = 1;		/* Ethernet hardware */
		*cp++ = 6;		/* length of address */
		setnl(++cp, 0x12345678); /* some random number? */
		setns(cp + 4, secs++);
		ipkdbcopy(ifp->myenetaddr, cp + 24, sizeof ifp->myenetaddr);
		outpkt(ifp, buf, 300, 68, 67);
		inpkt(ifp, buf, 2);
		if (ipkdbpanic && ipkdb_poll()) {
			ipkdbpanic++;
			return;
		}
	}
	cp = ifp->myinetaddr;
	printf("My IP address is %d.%d.%d.%d\n",
	       cp[0], cp[1], cp[2], cp[3]);
}

static int
chksum(p, l)
	char *p;
	int l;
{
	char csum;

	for (csum = 0; --l >= 0; csum += *p++);
	return csum;
}

static void
getpkt(ifp, buf, lp)
	struct ipkdb_if *ifp;
	char *buf;
	int *lp;
{
	char *got;
	int l;
	char ibuf[ETHERMTU+14];

	*lp = 0;
	while (1) {
		if (!(got = inpkt(ifp, ibuf, ipkdbpanic != 0))) {
			*lp = 0;
			return;
		}
		if (   ifp->seq == getnl(got)
		    && got[6] >= 'A'
		    && got[6] <= 'Z'
		    && (l = getns(got + 4))
		    && (got[6] == 'O' || chksum(got + 6, l) == got[l + 6])) {
			ipkdbcopy(got + 6, buf, *lp = l);
			return;
		}
		if (   ifp->pktlen
		    && ((ifp->flags&(IPKDB_MYIP|IPKDB_HISIP|IPKDB_CONNECTED))
			== (IPKDB_MYIP|IPKDB_HISIP|IPKDB_CONNECTED)))
			outpkt(ifp, ifp->pkt, ifp->pktlen, IPKDBPORT, IPKDBPORT);
	}
}

static void
putpkt(ifp, buf, l)
	struct ipkdb_if *ifp;
	char *buf;
	int l;
{
	setnl(ifp->pkt, ifp->seq++);
	setns(ifp->pkt + 4, l);
	ipkdbcopy(buf, ifp->pkt + 6, l);
	ifp->pkt[l + 6] = chksum(ifp->pkt + 6, l);
	ifp->pktlen = l + 7;
	if (   (ifp->flags&(IPKDB_MYIP|IPKDB_HISIP|IPKDB_CONNECTED))
	    != (IPKDB_MYIP|IPKDB_HISIP|IPKDB_CONNECTED))
		return;
	outpkt(ifp, ifp->pkt, ifp->pktlen, IPKDBPORT, IPKDBPORT);
}

static __inline int
maskcmp(vin, vmask, vmatch)
	void *vin, *vmask, *vmatch;
{
	int i;
	u_char *in = vin, *mask = vmask, *match = vmatch;

	for (i = 4; --i >= 0;)
		if ((*in++&*mask++) != *match++)
			return 0;
	return 1;
}

static int
check_ipkdb(ifp, shost, sport, dport, p, l)
	struct ipkdb_if *ifp;
	struct in_addr *shost;
	u_short sport, dport;
	char *p;
	int l;
{
	u_char hisenet[6];
	u_char hisinet[4];
	char save;
	struct ipkdb_allow *kap;

	if (chksum(p, l) != p[l])
		return 0;
	p[l] = 0;
	switch (ifp->connect) {
	default:
		return 0;
	case IPKDB_SAME:
		if (ipkdbcmp(shost, ifp->hisinetaddr, sizeof ifp->hisinetaddr))
			return 0;
		if (getns(&sport) != IPKDBPORT || getns(&dport) != IPKDBPORT)
			return 0;
		bzero(&hisinet, sizeof hisinet);
		break;
	case IPKDB_ALL:
		for (kap = ipkdballow; kap < ipkdballow + ipkdbcount; kap++) {
			if (maskcmp(shost, kap->mask, kap->match))
				break;
		}
		if (kap >= ipkdballow + ipkdbcount)
			return 0;
		if (getns(&sport) != IPKDBPORT || getns(&dport) != IPKDBPORT)
			return 0;
		ipkdbcopy(ifp->hisenetaddr, hisenet, sizeof hisenet);
		ipkdbcopy(ifp->hisinetaddr, hisinet, sizeof hisinet);
		save = ifp->flags;
		ipkdbcopy(shost, ifp->hisinetaddr, sizeof ifp->hisinetaddr);
		ifp->flags &= ~IPKDB_HISHW;
		ifp->flags |= IPKDB_HISIP;
		break;
	}
	if (connectipkdb(ifp, p) < 0) {
		if (ifp->connect == IPKDB_ALL) {
			ipkdbcopy(hisenet, ifp->hisenetaddr, sizeof ifp->hisenetaddr);
			ipkdbcopy(hisinet, ifp->hisinetaddr, sizeof ifp->hisinetaddr);
			ipkdb_if.flags = save;
		}
		return 0;
	}
	return 1;
}

/*
 * Should check whether packet came across the correct interface
 */
int
checkipkdb(shost, sport, dport, m, off, len)
	struct in_addr *shost;
	u_short sport, dport;
	struct mbuf *m;
{
	char *p;
	int l;
	char ibuf[ETHERMTU+50];

	m_copydata(m, off, len, ibuf);
	p = ibuf;
	if (getnl(p) || p[6] != 'O')
		return 0;
	l = getns(p + 4);
	if (l > len - 6 || !check_ipkdb(&ipkdb_if, shost, sport, dport, p + 6, l))
		return 0;
	ipkdb_connect(1);
	return 1;
}

static int
connectipkdb(ifp, buf)
	struct ipkdb_if *ifp;
	char *buf;
{
	char *cp;
	u_char *ip;

	if (*buf != 'O')
		return -1;
	if (getnl(buf + 1) == ifp->id)
		/* It's a retry of a connect packet, ignore it */
		return -1;
	for (cp = buf + 1 + sizeof(u_int32_t); *cp && *cp != ':'; cp++);
	if (!*cp)
		return -1;
	*cp++ = 0;
	ip = ifp->hisinetaddr;
	printf("debugged by %s@%s (%d.%d.%d.%d)\n", buf + 1 + sizeof(u_int32_t), cp,
	       ip[0], ip[1], ip[2], ip[3]);
	ifp->connect = IPKDB_SAME; /* if someone once connected, he may do so again */
	ifp->flags |= IPKDB_CONNECTED;
	ifp->seq = 0;
	ifp->pktlen = 0;
	ifp->id = getnl(buf + 1);
	return 0;
}
