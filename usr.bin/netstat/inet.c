/*	$NetBSD: inet.c,v 1.37.4.1 1999/12/27 18:37:06 wrstuden Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
__RCSID("$NetBSD: inet.c,v 1.37.4.1 1999/12/27 18:37:06 wrstuden Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#define	TCPTIMERS
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;

char	*inetname __P((struct in_addr *));
void	inetprint __P((struct in_addr *, u_int16_t, const char *, int));

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
static int width;

void
protopr(off, name)
	u_long off;
	char *name;
{
	struct inpcbtable table;
	struct inpcb *head, *next, *prev;
	struct inpcb inpcb;
	int istcp, compact;
	static int first = 1;
	static char *shorttcpstates[] = {
		"CLOSED",	"LISTEN",	"SYNSEN",	"SYSRCV",
		"ESTABL",	"CLWAIT",	"FWAIT1",	"CLOSNG",
		"LASTAK",	"FWAIT2",	"TMWAIT",
	};

	if (off == 0)
		return;
	istcp = strcmp(name, "tcp") == 0;
	kread(off, (char *)&table, sizeof table);
	prev = head =
	    (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue.cqh_first;
	next = table.inpt_queue.cqh_first;

	compact = 0;
	if (Aflag) {
		if (!nflag)
			width = 18;
		else {
			width = 21;
			compact = 1;
		}
	} else
		width = 22;
	while (next != head) {
		kread((u_long)next, (char *)&inpcb, sizeof inpcb);
		if (inpcb.inp_queue.cqe_prev != prev) {
			printf("???\n");
			break;
		}
		prev = next;
		next = inpcb.inp_queue.cqe_next;

		if (!aflag &&
		    inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
			continue;
		kread((u_long)inpcb.inp_socket, (char *)&sockb, sizeof (sockb));
		if (istcp) {
			kread((u_long)inpcb.inp_ppcb,
			    (char *)&tcpcb, sizeof (tcpcb));
		}
		if (first) {
			printf("Active Internet connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf("%-5.5s %-6.6s %-6.6s %s%-*.*s %-*.*s %s\n",
				"Proto", "Recv-Q", "Send-Q",
				compact ? "" : " ",
				width, width, "Local Address",
				width, width, "Foreign Address", "State");
			first = 0;
		}
		if (Aflag) {
			if (istcp)
				printf("%8lx ", (u_long) inpcb.inp_ppcb);
			else
				printf("%8lx ", (u_long) prev);
		}
		printf("%-5.5s %6ld %6ld%s", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc, compact ? "" : " ");
		if (nflag) {
			inetprint(&inpcb.inp_laddr, inpcb.inp_lport, name, 1);
			inetprint(&inpcb.inp_faddr, inpcb.inp_fport, name, 1);
		} else if (inpcb.inp_flags & INP_ANONPORT) {
			inetprint(&inpcb.inp_laddr, inpcb.inp_lport, name, 1);
			inetprint(&inpcb.inp_faddr, inpcb.inp_fport, name, 0);
		} else {
			inetprint(&inpcb.inp_laddr, inpcb.inp_lport, name, 0);
			inetprint(&inpcb.inp_faddr, inpcb.inp_fport, name, 
			    inpcb.inp_lport != inpcb.inp_fport);
		}
		if (istcp) {
			if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
				printf(" %d", tcpcb.t_state);
			else
				printf(" %s", compact ?
				    shorttcpstates[tcpcb.t_state] :
				    tcpstates[tcpcb.t_state]);
		}
		putchar('\n');
	}
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(off, name)
	u_long off;
	char *name;
{
	struct tcpstat tcpstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&tcpstat, sizeof (tcpstat));

#define	ps(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)tcpstat.f)
#define	p(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)tcpstat.f, plural(tcpstat.f))
#define	p2(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
    printf(m, (unsigned long long)tcpstat.f1, plural(tcpstat.f1), \
    (unsigned long long)tcpstat.f2, plural(tcpstat.f2))
#define	p2s(f1, f2, m) if (tcpstat.f1 || tcpstat.f2 || sflag <= 1) \
    printf(m, (unsigned long long)tcpstat.f1, plural(tcpstat.f1), \
    (unsigned long long)tcpstat.f2)
#define	p3(f, m) if (tcpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)tcpstat.f, plurales(tcpstat.f))

	p(tcps_sndtotal, "\t%llu packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
		"\t\t%llu data packet%s (%llu byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
		"\t\t%llu data packet%s (%llu byte%s) retransmitted\n");
	p2s(tcps_sndacks, tcps_delack,
		"\t\t%llu ack-only packet%s (%llu delayed)\n");
	p(tcps_sndurg, "\t\t%llu URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%llu window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%llu window update packet%s\n");
	p(tcps_sndctrl, "\t\t%llu control packet%s\n");
	p(tcps_rcvtotal, "\t%llu packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte,
		"\t\t%llu ack%s (for %llu byte%s)\n");
	p(tcps_rcvdupack, "\t\t%llu duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%llu ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
		"\t\t%llu packet%s (%llu byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
		"\t\t%llu completely duplicate packet%s (%llu byte%s)\n");
	p(tcps_pawsdrop, "\t\t%llu old duplicate packet%s\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
		"\t\t%llu packet%s with some dup. data (%llu byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
		"\t\t%llu out-of-order packet%s (%llu byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
		"\t\t%llu packet%s (%llu byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%llu window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%llu window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%llu packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%llu discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%llu discarded for bad header offset field%s\n");
	ps(tcps_rcvshort, "\t\t%llu discarded because packet too short\n");
	p(tcps_connattempt, "\t%llu connection request%s\n");
	p(tcps_accepts, "\t%llu connection accept%s\n");
	p(tcps_connects,
		"\t%llu connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
		"\t%llu connection%s closed (including %llu drop%s)\n");
	p(tcps_conndrops, "\t%llu embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
		"\t%llu segment%s updated rtt (of %llu attempt%s)\n");
	p(tcps_rexmttimeo, "\t%llu retransmit timeout%s\n");
	p(tcps_timeoutdrop,
		"\t\t%llu connection%s dropped by rexmit timeout\n");
	p2(tcps_persisttimeo, tcps_persistdrops,
	   "\t%llu persist timeout%s (resulting in %llu dropped "
		"connection%s)\n");
	p(tcps_keeptimeo, "\t%llu keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%llu keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%llu connection%s dropped by keepalive\n");
	p(tcps_predack, "\t%llu correct ACK header prediction%s\n");
	p(tcps_preddat, "\t%llu correct data packet header prediction%s\n");
	p3(tcps_pcbhashmiss, "\t%llu PCB hash miss%s\n");
	ps(tcps_noport, "\t%llu dropped due to no socket\n");
	p(tcps_connsdrained, "\t%llu connection%s drained due to memory "
		"shortage\n");

	p(tcps_badsyn, "\t%llu bad connection attempt%s\n");
	ps(tcps_sc_added, "\t%llu SYN cache entries added\n");
	p(tcps_sc_collisions, "\t\t%llu hash collision%s\n");
	ps(tcps_sc_completed, "\t\t%llu completed\n");
	ps(tcps_sc_aborted, "\t\t%llu aborted (no space to build PCB)\n");
	ps(tcps_sc_timed_out, "\t\t%llu timed out\n");
	ps(tcps_sc_overflowed, "\t\t%llu dropped due to overflow\n");
	ps(tcps_sc_bucketoverflow, "\t\t%llu dropped due to bucket overflow\n");
	ps(tcps_sc_reset, "\t\t%llu dropped due to RST\n");
	ps(tcps_sc_unreach, "\t\t%llu dropped due to ICMP unreachable\n");
	p(tcps_sc_retransmitted, "\t%llu SYN,ACK%s retransmitted\n");
	p(tcps_sc_dupesyn, "\t%llu duplicate SYN%s received for entries "
		"already in the cache\n");
	p(tcps_sc_dropped, "\t%llu SYN%s dropped (no route or no space)\n");

#undef p
#undef ps
#undef p2
#undef p2s
#undef p3
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(off, name)
	u_long off;
	char *name;
{
	struct udpstat udpstat;
	u_quad_t delivered;

	if (off == 0)
		return;
	printf("%s:\n", name);
	kread(off, (char *)&udpstat, sizeof (udpstat));

#define	ps(f, m) if (udpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)udpstat.f)
#define	p(f, m) if (udpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)udpstat.f, plural(udpstat.f))
#define	p3(f, m) if (udpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)udpstat.f, plurales(udpstat.f))

	p(udps_ipackets, "\t%llu datagram%s received\n");
	ps(udps_hdrops, "\t%llu with incomplete header\n");
	ps(udps_badlen, "\t%llu with bad data length field\n");
	ps(udps_badsum, "\t%llu with bad checksum\n");
	ps(udps_noport, "\t%llu dropped due to no socket\n");
	p(udps_noportbcast, "\t%llu broadcast/multicast datagram%s dropped due to no socket\n");
	ps(udps_fullsock, "\t%llu dropped due to full socket buffers\n");
	delivered = udpstat.udps_ipackets -
		    udpstat.udps_hdrops -
		    udpstat.udps_badlen -
		    udpstat.udps_badsum -
		    udpstat.udps_noport -
		    udpstat.udps_noportbcast -
		    udpstat.udps_fullsock;
	if (delivered || sflag <= 1)
		printf("\t%llu delivered\n", (unsigned long long)delivered);
	p3(udps_pcbhashmiss, "\t%llu PCB hash miss%s\n");
	p(udps_opackets, "\t%llu datagram%s output\n");

#undef ps
#undef p
#undef p3
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(off, name)
	u_long off;
	char *name;
{
	struct ipstat ipstat;

	if (off == 0)
		return;
	kread(off, (char *)&ipstat, sizeof (ipstat));
	printf("%s:\n", name);

#define	ps(f, m) if (ipstat.f || sflag <= 1) \
    printf(m, (unsigned long long)ipstat.f)
#define	p(f, m) if (ipstat.f || sflag <= 1) \
    printf(m, (unsigned long long)ipstat.f, plural(ipstat.f))

	p(ips_total, "\t%llu total packet%s received\n");
	p(ips_badsum, "\t%llu bad header checksum%s\n");
	ps(ips_toosmall, "\t%llu with size smaller than minimum\n");
	ps(ips_tooshort, "\t%llu with data size < data length\n");
	ps(ips_toolong, "\t%llu with length > max ip packet size\n");
	ps(ips_badhlen, "\t%llu with header length < data size\n");
	ps(ips_badlen, "\t%llu with data length < header length\n");
	ps(ips_badoptions, "\t%llu with bad options\n");
	ps(ips_badvers, "\t%llu with incorrect version number\n");
	p(ips_fragments, "\t%llu fragment%s received");
	p(ips_fragdropped, "\t%llu fragment%s dropped (dup or out of space)\n");
	p(ips_badfrags, "\t%llu malformed fragment%s dropped\n");
	p(ips_fragtimeout, "\t%llu fragment%s dropped after timeout\n");
	p(ips_reassembled, "\t%llu packet%s reassembled ok\n");
	p(ips_delivered, "\t%llu packet%s for this host\n");
	p(ips_noproto, "\t%llu packet%s for unknown/unsupported protocol\n");
	p(ips_forward, "\t%llu packet%s forwarded");
	p(ips_fastforward, " (%llu packet%s fast forwarded)");
	if (ipstat.ips_forward || sflag <= 1)
		putchar('\n');
	p(ips_cantforward, "\t%llu packet%s not forwardable\n");
	p(ips_redirectsent, "\t%llu redirect%s sent\n");
	p(ips_localout, "\t%llu packet%s sent from this host\n");
	p(ips_rawout, "\t%llu packet%s sent with fabricated ip header\n");
	p(ips_odropped, "\t%llu output packet%s dropped due to no bufs, etc.\n");
	p(ips_noroute, "\t%llu output packet%s discarded due to no route\n");
	p(ips_fragmented, "\t%llu output datagram%s fragmented\n");
	p(ips_ofragments, "\t%llu fragment%s created\n");
	p(ips_cantfrag, "\t%llu datagram%s that can't be fragmented\n");
#undef ps
#undef p
}

static	char *icmpnames[] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"#9",
	"#10",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(off, name)
	u_long off;
	char *name;
{
	struct icmpstat icmpstat;
	int i, first;

	if (off == 0)
		return;
	kread(off, (char *)&icmpstat, sizeof (icmpstat));
	printf("%s:\n", name);

#define	p(f, m) if (icmpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)icmpstat.f, plural(icmpstat.f))

	p(icps_error, "\t%llu call%s to icmp_error\n");
	p(icps_oldicmp,
	    "\t%llu error%s not generated because old message was icmp\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmpnames[i],
				(unsigned long long)icmpstat.icps_outhist[i]);
		}
	p(icps_badcode, "\t%llu message%s with bad code fields\n");
	p(icps_tooshort, "\t%llu message%s < minimum length\n");
	p(icps_checksum, "\t%llu bad checksum%s\n");
	p(icps_badlen, "\t%llu message%s with bad length\n");
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmpnames[i],
				(unsigned long long)icmpstat.icps_inhist[i]);
		}
	p(icps_reflect, "\t%llu message response%s generated\n");
#undef p
}

/*
 * Dump IGMP statistics structure.
 */
void
igmp_stats(off, name)
	u_long off;
	char *name;
{
	struct igmpstat igmpstat;

	if (off == 0)
		return;
	kread(off, (char *)&igmpstat, sizeof (igmpstat));
	printf("%s:\n", name);

#define	p(f, m) if (igmpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)igmpstat.f, plural(igmpstat.f))
#define	py(f, m) if (igmpstat.f || sflag <= 1) \
    printf(m, (unsigned long long)igmpstat.f, igmpstat.f != 1 ? "ies" : "y")
	p(igps_rcv_total, "\t%llu message%s received\n");
        p(igps_rcv_tooshort, "\t%llu message%s received with too few bytes\n");
        p(igps_rcv_badsum, "\t%llu message%s received with bad checksum\n");
        py(igps_rcv_queries, "\t%llu membership quer%s received\n");
        py(igps_rcv_badqueries, "\t%llu membership quer%s received with invalid field(s)\n");
        p(igps_rcv_reports, "\t%llu membership report%s received\n");
        p(igps_rcv_badreports, "\t%llu membership report%s received with invalid field(s)\n");
        p(igps_rcv_ourreports, "\t%llu membership report%s received for groups to which we belong\n");
        p(igps_snd_reports, "\t%llu membership report%s sent\n");
#undef p
#undef py
}

#ifdef IPSEC
static	char *ipsec_ahnames[] = {
	"none",
	"hmac MD5",
	"hmac SHA1",
	"keyed MD5",
	"keyed SHA1",
	"null",
};

static	char *ipsec_espnames[] = {
	"none",
	"DES CBC",
	"3DES CBC",
	"simple",
	"blowfish CBC",
	"CAST128 CBC",
	"DES derived IV",
};

/*
 * Dump IPSEC statistics structure.
 */
void
ipsec_stats(off, name)
	u_long off;
	char *name;
{
	struct ipsecstat ipsecstat;
	int first, proto;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&ipsecstat, sizeof (ipsecstat));

#define	p(f, m) if (ipsecstat.f || sflag <= 1) \
    printf(m, (unsigned long long)ipsecstat.f, plural(ipsecstat.f))

	p(in_success, "\t%llu inbound packet%s processed successfully\n");
	p(in_polvio, "\t%llu inbound packet%s violated process security "
		"policy\n");
	p(in_nosa, "\t%llu inbound packet%s with no SA available\n");
	p(in_inval,
		"\t%llu inbound packet%s failed processing due to EINVAL\n");
	p(in_badspi, "\t%llu inbound packet%s failed getting SPI\n");
	p(in_ahreplay, "\t%llu inbound packet%s failed on AH replay check\n");
	p(in_espreplay, "\t%llu inbound packet%s failed on ESP replay check\n");
	p(in_ahauthsucc, "\t%llu inbound packet%s considered authentic\n");
	p(in_ahauthfail, "\t%llu inbound packet%s failed on authentication\n");
	for (first = 1, proto = 0; proto < SADB_AALG_MAX; proto++) {
		if (ipsecstat.in_ahhist[proto] <= 0)
			continue;
		if (first) {
			printf("\tAH input histogram:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n",
			ipsec_ahnames[proto],
			(unsigned long long)ipsecstat.in_ahhist[proto]);
	}
	for (first = 1, proto = 0; proto < SADB_EALG_MAX; proto++) {
		if (ipsecstat.in_esphist[proto] <= 0)
			continue;
		if (first) {
			printf("\tESP input histogram:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", ipsec_espnames[proto],
			(unsigned long long)ipsecstat.in_esphist[proto]);
	}

	p(out_success, "\t%llu outbound packet%s processed successfully\n");
	p(out_polvio, "\t%llu outbound packet%s violated process security "
		"policy\n");
	p(out_nosa, "\t%llu outbound packet%s with no SA available\n");
	p(out_inval, "\t%llu outbound packet%s failed processing due to "
		"EINVAL\n");
	p(out_noroute, "\t%llu outbound packet%s with no route\n");
	for (first = 1, proto = 0; proto < SADB_AALG_MAX; proto++) {
		if (ipsecstat.out_ahhist[proto] <= 0)
			continue;
		if (first) {
			printf("\tAH output histogram:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", ipsec_ahnames[proto],
			(unsigned long long)ipsecstat.out_ahhist[proto]);
	}
	for (first = 1, proto = 0; proto < SADB_EALG_MAX; proto++) {
		if (ipsecstat.out_esphist[proto] <= 0)
			continue;
		if (first) {
			printf("\tESP output histogram:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", ipsec_espnames[proto],
			(unsigned long long)ipsecstat.out_esphist[proto]);
	}
#undef p
}
#endif /*IPSEC*/

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
void
inetprint(in, port, proto, numeric)
	struct in_addr *in;
	u_int16_t port;
	const char *proto;
	int numeric;
{
	struct servent *sp = 0;
	char line[80], *cp;
	size_t space;

	(void)snprintf(line, sizeof line, "%.*s.",
	    (Aflag && !nflag) ? 12 : 16, inetname(in));
	cp = strchr(line, '\0');
	if (!numeric && port)
		sp = getservbyport((int)port, proto);
	space = sizeof line - (cp-line);
	if (sp || port == 0)
		(void)snprintf(cp, space, "%.8s", sp ? sp->s_name : "*");
	else
		(void)snprintf(cp, space, "%u", ntohs(port));
	(void)printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(inp)
	struct in_addr *inp;
{
	char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0) {
			domain[sizeof(domain) - 1] = '\0';
			if ((cp = strchr(domain, '.')))
				(void) strcpy(domain, cp + 1);
			else
				domain[0] = 0;
		} else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)inp, sizeof (*inp), AF_INET);
			if (hp) {
				if ((cp = strchr(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
	}
	if (inp->s_addr == INADDR_ANY)
		strncpy(line, "*", sizeof line);
	else if (cp)
		strncpy(line, cp, sizeof line);
	else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	((x) & 0xff)
		(void)snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(inp->s_addr >> 24), C(inp->s_addr >> 16),
		    C(inp->s_addr >> 8), C(inp->s_addr));
#undef C
	}
	line[sizeof(line) - 1] = '\0';
	return (line);
}

/*
 * Dump the contents of a TCP PCB.
 */
void
tcp_dump(pcbaddr)
	u_long pcbaddr;
{
	struct tcpcb tcpcb;
	int i;

	kread(pcbaddr, (char *)&tcpcb, sizeof(tcpcb));

	printf("TCP Protocol Control Block at 0x%08lx:\n\n", pcbaddr);

	printf("Timers:\n");
	for (i = 0; i < TCPT_NTIMERS; i++)
		printf("\t%s: %u", tcptimers[i], tcpcb.t_timer[i]);
	printf("\n\n");

	if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
		printf("State: %d", tcpcb.t_state);
	else
		printf("State: %s", tcpstates[tcpcb.t_state]);
	printf(", flags 0x%x, inpcb 0x%lx\n\n", tcpcb.t_flags,
	    (u_long)tcpcb.t_inpcb);

	printf("rxtshift %d, rxtcur %d, dupacks %d\n", tcpcb.t_rxtshift,
	    tcpcb.t_rxtcur, tcpcb.t_dupacks);
	printf("peermss %u, ourmss %u, segsz %u\n\n", tcpcb.t_peermss,
	    tcpcb.t_ourmss, tcpcb.t_segsz);

	printf("snd_una %u, snd_nxt %u, snd_up %u\n",
	    tcpcb.snd_una, tcpcb.snd_nxt, tcpcb.snd_up);
	printf("snd_wl1 %u, snd_wl2 %u, iss %u, snd_wnd %lu\n\n",
	    tcpcb.snd_wl1, tcpcb.snd_wl2, tcpcb.iss, tcpcb.snd_wnd);

	printf("rcv_wnd %lu, rcv_nxt %u, rcv_up %u, irs %u\n\n",
	    tcpcb.rcv_wnd, tcpcb.rcv_nxt, tcpcb.rcv_up, tcpcb.irs);

	printf("rcv_adv %u, snd_max %u, snd_cwnd %lu, snd_ssthresh %lu\n",
	    tcpcb.rcv_adv, tcpcb.snd_max, tcpcb.snd_cwnd, tcpcb.snd_ssthresh);

	printf("idle %d, rtt %d, rtseq %u, srtt %d, rttvar %d, rttmin %d, "
	    "max_sndwnd %lu\n\n", tcpcb.t_idle, tcpcb.t_rtt, tcpcb.t_rtseq,
	    tcpcb.t_srtt, tcpcb.t_rttvar, tcpcb.t_rttmin, tcpcb.max_sndwnd);

	printf("oobflags %d, iobc %d, softerror %d\n\n", tcpcb.t_oobflags,
	    tcpcb.t_iobc, tcpcb.t_softerror);

	printf("snd_scale %d, rcv_scale %d, req_r_scale %d, req_s_scale %d\n",
	    tcpcb.snd_scale, tcpcb.rcv_scale, tcpcb.request_r_scale,
	    tcpcb.requested_s_scale);
	printf("ts_recent %u, ts_regent_age %d, last_ack_sent %u\n",
	    tcpcb.ts_recent, tcpcb.ts_recent_age, tcpcb.last_ack_sent);
}
