/*	$NetBSD: if_spppsubr.c,v 1.5.2.3 2001/01/18 09:23:52 bouyer Exp $	 */

/*
 * Synchronous PPP/Cisco link level subroutines.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1994-1996 Cronyx Engineering Ltd.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * Heavily revamped to conform to RFC 1661.
 * Copyright (C) 1997, Joerg Wunsch.
 *
 * RFC2472 IPv6CP support.
 * Copyright (C) 2000, Jun-ichiro itojun Hagino <itojun@iijlab.net>.
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * From: Version 2.4, Thu Apr 30 17:17:21 MSD 1997
 *
 * From: if_spppsubr.c,v 1.39 1998/04/04 13:26:03 phk Exp
 *
 * From: Id: if_spppsubr.c,v 1.23 1999/02/23 14:47:50 hm Exp
 */

#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_iso.h"
#include "opt_ns.h"
#endif

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <machine/random.h>
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>

#if defined (__OpenBSD__)
#include <sys/md5k.h>
#else
#include <sys/md5.h>
#endif

#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/ppp_defs.h>

#include <machine/stdarg.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif
#if defined (__FreeBSD__) || defined (__OpenBSD__)
#include <netinet/if_ether.h>
#else
#include <net/ethertypes.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <net/if_sppp.h>
#else
#include <i4b/sppp/if_sppp.h>
#endif
#if defined(__NetBSD__) || defined (__OpenBSD__)
#include <machine/cpu.h> /* XXX for softnet */
#endif

#define MAXALIVECNT     3               /* max. alive packets */

/*
 * Interface flags that can be set in an ifconfig command.
 *
 * Setting link0 will make the link passive, i.e. it will be marked
 * as being administrative openable, but won't be opened to begin
 * with.  Incoming calls will be answered, or subsequent calls with
 * -link1 will cause the administrative open of the LCP layer.
 *
 * Setting link1 will cause the link to auto-dial only as packets
 * arrive to be sent.
 *
 * Setting IFF_DEBUG will syslog the option negotiation and state
 * transitions at level kern.debug.  Note: all logs consistently look
 * like
 *
 *   <if-name><unit>: <proto-name> <additional info...>
 *
 * with <if-name><unit> being something like "bppp0", and <proto-name>
 * being one of "lcp", "ipcp", "cisco", "chap", "pap", etc.
 */

#define IFF_PASSIVE	IFF_LINK0	/* wait passively for connection */
#define IFF_AUTO	IFF_LINK1	/* auto-dial on output */

#define CONF_REQ	1		/* PPP configure request */
#define CONF_ACK	2		/* PPP configure acknowledge */
#define CONF_NAK	3		/* PPP configure negative ack */
#define CONF_REJ	4		/* PPP configure reject */
#define TERM_REQ	5		/* PPP terminate request */
#define TERM_ACK	6		/* PPP terminate acknowledge */
#define CODE_REJ	7		/* PPP code reject */
#define PROTO_REJ	8		/* PPP protocol reject */
#define ECHO_REQ	9		/* PPP echo request */
#define ECHO_REPLY	10		/* PPP echo reply */
#define DISC_REQ	11		/* PPP discard request */

#define LCP_OPT_MRU		1	/* maximum receive unit */
#define LCP_OPT_ASYNC_MAP	2	/* async control character map */
#define LCP_OPT_AUTH_PROTO	3	/* authentication protocol */
#define LCP_OPT_QUAL_PROTO	4	/* quality protocol */
#define LCP_OPT_MAGIC		5	/* magic number */
#define LCP_OPT_RESERVED	6	/* reserved */
#define LCP_OPT_PROTO_COMP	7	/* protocol field compression */
#define LCP_OPT_ADDR_COMP	8	/* address/control field compression */

#define IPCP_OPT_ADDRESSES	1	/* both IP addresses; deprecated */
#define IPCP_OPT_COMPRESSION	2	/* IP compression protocol */
#define IPCP_OPT_ADDRESS	3	/* local IP address */

#define IPV6CP_OPT_IFID		1	/* interface identifier */
#define IPV6CP_OPT_COMPRESSION	2	/* IPv6 compression protocol */

#define PAP_REQ			1	/* PAP name/password request */
#define PAP_ACK			2	/* PAP acknowledge */
#define PAP_NAK			3	/* PAP fail */

#define CHAP_CHALLENGE		1	/* CHAP challenge request */
#define CHAP_RESPONSE		2	/* CHAP challenge response */
#define CHAP_SUCCESS		3	/* CHAP response ok */
#define CHAP_FAILURE		4	/* CHAP response failed */

#define CHAP_MD5		5	/* hash algorithm - MD5 */

#define CISCO_MULTICAST		0x8f	/* Cisco multicast address */
#define CISCO_UNICAST		0x0f	/* Cisco unicast address */
#define CISCO_KEEPALIVE		0x8035	/* Cisco keepalive protocol */
#define CISCO_ADDR_REQ		0	/* Cisco address request */
#define CISCO_ADDR_REPLY	1	/* Cisco address reply */
#define CISCO_KEEPALIVE_REQ	2	/* Cisco keepalive request */

/* states are named and numbered according to RFC 1661 */
#define STATE_INITIAL	0
#define STATE_STARTING	1
#define STATE_CLOSED	2
#define STATE_STOPPED	3
#define STATE_CLOSING	4
#define STATE_STOPPING	5
#define STATE_REQ_SENT	6
#define STATE_ACK_RCVD	7
#define STATE_ACK_SENT	8
#define STATE_OPENED	9

struct ppp_header {
	u_char address;
	u_char control;
	u_short protocol;
} __attribute__((__packed__));
#define PPP_HEADER_LEN          sizeof (struct ppp_header)

struct lcp_header {
	u_char type;
	u_char ident;
	u_short len;
} __attribute__((__packed__));
#define LCP_HEADER_LEN          sizeof (struct lcp_header)

struct cisco_packet {
	u_long type;
	u_long par1;
	u_long par2;
	u_short rel;
	u_short time0;
	u_short time1;
} __attribute__((__packed__));
#define CISCO_PACKET_LEN 18

/*
 * We follow the spelling and capitalization of RFC 1661 here, to make
 * it easier comparing with the standard.  Please refer to this RFC in
 * case you can't make sense out of these abbreviation; it will also
 * explain the semantics related to the various events and actions.
 */
struct cp {
	u_short	proto;		/* PPP control protocol number */
	u_char protoidx;	/* index into state table in struct sppp */
	u_char flags;
#define CP_LCP		0x01	/* this is the LCP */
#define CP_AUTH		0x02	/* this is an authentication protocol */
#define CP_NCP		0x04	/* this is a NCP */
#define CP_QUAL		0x08	/* this is a quality reporting protocol */
	const char *name;	/* name of this control protocol */
	/* event handlers */
	void	(*Up)(struct sppp *sp);
	void	(*Down)(struct sppp *sp);
	void	(*Open)(struct sppp *sp);
	void	(*Close)(struct sppp *sp);
	void	(*TO)(void *sp);
	int	(*RCR)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_rej)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_nak)(struct sppp *sp, struct lcp_header *h, int len);
	/* actions */
	void	(*tlu)(struct sppp *sp);
	void	(*tld)(struct sppp *sp);
	void	(*tls)(struct sppp *sp);
	void	(*tlf)(struct sppp *sp);
	void	(*scr)(struct sppp *sp);
};

static struct sppp *spppq;
#if defined(__NetBSD__)
static struct callout keepalive_ch;
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
static struct callout_handle keepalive_ch;
#endif

#ifdef __FreeBSD__
#define	SPP_FMT		"%s%d: "
#define	SPP_ARGS(ifp)	(ifp)->if_name, (ifp)->if_unit
#else
#define	SPP_FMT		"%s: "
#define	SPP_ARGS(ifp)	(ifp)->if_xname
#endif

#ifdef INET
/*
 * The following disgusting hack gets around the problem that IP TOS
 * can't be set yet.  We want to put "interactive" traffic on a high
 * priority queue.  To decide if traffic is interactive, we check that
 * a) it is TCP and b) one of its ports is telnet, rlogin or ftp control.
 *
 * XXX is this really still necessary?  - joerg -
 */
static u_short interactive_ports[8] = {
	0,	513,	0,	0,
	0,	21,	0,	23,
};
#define INTERACTIVE(p) (interactive_ports[(p) & 7] == (p))
#endif

/* almost every function needs these */
#define STDDCL							\
	struct ifnet *ifp = &sp->pp_if;				\
	int debug = ifp->if_flags & IFF_DEBUG

static int sppp_output(struct ifnet *ifp, struct mbuf *m,
		       struct sockaddr *dst, struct rtentry *rt);

static void sppp_cisco_send(struct sppp *sp, int type, long par1, long par2);
static void sppp_cisco_input(struct sppp *sp, struct mbuf *m);

static void sppp_cp_input(const struct cp *cp, struct sppp *sp,
			  struct mbuf *m);
static void sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
			 u_char ident, u_short len, void *data);
/* static void sppp_cp_timeout(void *arg); */
static void sppp_cp_change_state(const struct cp *cp, struct sppp *sp,
				 int newstate);
static void sppp_auth_send(const struct cp *cp,
			   struct sppp *sp, unsigned int type, unsigned int id,
			   ...);

static void sppp_up_event(const struct cp *cp, struct sppp *sp);
static void sppp_down_event(const struct cp *cp, struct sppp *sp);
static void sppp_open_event(const struct cp *cp, struct sppp *sp);
static void sppp_close_event(const struct cp *cp, struct sppp *sp);
static void sppp_to_event(const struct cp *cp, struct sppp *sp);

static void sppp_null(struct sppp *sp);

static void sppp_lcp_init(struct sppp *sp);
static void sppp_lcp_up(struct sppp *sp);
static void sppp_lcp_down(struct sppp *sp);
static void sppp_lcp_open(struct sppp *sp);
static void sppp_lcp_close(struct sppp *sp);
static void sppp_lcp_TO(void *sp);
static int sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_tlu(struct sppp *sp);
static void sppp_lcp_tld(struct sppp *sp);
static void sppp_lcp_tls(struct sppp *sp);
static void sppp_lcp_tlf(struct sppp *sp);
static void sppp_lcp_scr(struct sppp *sp);
static void sppp_lcp_check_and_close(struct sppp *sp);
static int sppp_ncp_check(struct sppp *sp);

static void sppp_ipcp_init(struct sppp *sp);
static void sppp_ipcp_up(struct sppp *sp);
static void sppp_ipcp_down(struct sppp *sp);
static void sppp_ipcp_open(struct sppp *sp);
static void sppp_ipcp_close(struct sppp *sp);
static void sppp_ipcp_TO(void *sp);
static int sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_tlu(struct sppp *sp);
static void sppp_ipcp_tld(struct sppp *sp);
static void sppp_ipcp_tls(struct sppp *sp);
static void sppp_ipcp_tlf(struct sppp *sp);
static void sppp_ipcp_scr(struct sppp *sp);

static void sppp_ipv6cp_init(struct sppp *sp);
static void sppp_ipv6cp_up(struct sppp *sp);
static void sppp_ipv6cp_down(struct sppp *sp);
static void sppp_ipv6cp_open(struct sppp *sp);
static void sppp_ipv6cp_close(struct sppp *sp);
static void sppp_ipv6cp_TO(void *sp);
static int sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipv6cp_tlu(struct sppp *sp);
static void sppp_ipv6cp_tld(struct sppp *sp);
static void sppp_ipv6cp_tls(struct sppp *sp);
static void sppp_ipv6cp_tlf(struct sppp *sp);
static void sppp_ipv6cp_scr(struct sppp *sp);

static void sppp_pap_input(struct sppp *sp, struct mbuf *m);
static void sppp_pap_init(struct sppp *sp);
static void sppp_pap_open(struct sppp *sp);
static void sppp_pap_close(struct sppp *sp);
static void sppp_pap_TO(void *sp);
static void sppp_pap_my_TO(void *sp);
static void sppp_pap_tlu(struct sppp *sp);
static void sppp_pap_tld(struct sppp *sp);
static void sppp_pap_scr(struct sppp *sp);

static void sppp_chap_input(struct sppp *sp, struct mbuf *m);
static void sppp_chap_init(struct sppp *sp);
static void sppp_chap_open(struct sppp *sp);
static void sppp_chap_close(struct sppp *sp);
static void sppp_chap_TO(void *sp);
static void sppp_chap_tlu(struct sppp *sp);
static void sppp_chap_tld(struct sppp *sp);
static void sppp_chap_scr(struct sppp *sp);

static const char *sppp_auth_type_name(u_short proto, u_char type);
static const char *sppp_cp_type_name(u_char type);
static const char *sppp_dotted_quad(u_long addr);
static const char *sppp_ipcp_opt_name(u_char opt);
#ifdef INET6
static const char *sppp_ipv6cp_opt_name(u_char opt);
#endif
static const char *sppp_lcp_opt_name(u_char opt);
static const char *sppp_phase_name(enum ppp_phase phase);
static const char *sppp_proto_name(u_short proto);
static const char *sppp_state_name(int state);
static int sppp_params(struct sppp *sp, int cmd, void *data);
static int sppp_strnlen(u_char *p, int max);
static void sppp_get_ip_addrs(struct sppp *sp, u_long *src, u_long *dst,
			      u_long *srcmask);
static void sppp_keepalive(void *dummy);
static void sppp_phase_network(struct sppp *sp);
static void sppp_print_bytes(const u_char *p, u_short len);
static void sppp_print_string(const char *p, u_short len);
static void sppp_set_ip_addr(struct sppp *sp, u_long src);
#ifdef INET6
static void sppp_get_ip6_addrs(struct sppp *sp, struct in6_addr *src,
				struct in6_addr *dst, struct in6_addr *srcmask);
#ifdef IPV6CP_MYIFID_DYN
static void sppp_set_ip6_addr(struct sppp *sp, const struct in6_addr *src);
static void sppp_gen_ip6_addr(struct sppp *sp, const struct in6_addr *src);
#endif
static void sppp_suggest_ip6_addr(struct sppp *sp, struct in6_addr *src);
#endif

/* our control protocol descriptors */
static const struct cp lcp = {
	PPP_LCP, IDX_LCP, CP_LCP, "lcp",
	sppp_lcp_up, sppp_lcp_down, sppp_lcp_open, sppp_lcp_close,
	sppp_lcp_TO, sppp_lcp_RCR, sppp_lcp_RCN_rej, sppp_lcp_RCN_nak,
	sppp_lcp_tlu, sppp_lcp_tld, sppp_lcp_tls, sppp_lcp_tlf,
	sppp_lcp_scr
};

static const struct cp ipcp = {
	PPP_IPCP, IDX_IPCP,
#ifdef INET
	CP_NCP,	/*don't run IPCP if there's no IPv4 support*/
#else
	0,
#endif
	"ipcp",
	sppp_ipcp_up, sppp_ipcp_down, sppp_ipcp_open, sppp_ipcp_close,
	sppp_ipcp_TO, sppp_ipcp_RCR, sppp_ipcp_RCN_rej, sppp_ipcp_RCN_nak,
	sppp_ipcp_tlu, sppp_ipcp_tld, sppp_ipcp_tls, sppp_ipcp_tlf,
	sppp_ipcp_scr
};

static const struct cp ipv6cp = {
	PPP_IPV6CP, IDX_IPV6CP,
#ifdef INET6	/*don't run IPv6CP if there's no IPv6 support*/
	CP_NCP,
#else
	0,
#endif
	"ipv6cp",
	sppp_ipv6cp_up, sppp_ipv6cp_down, sppp_ipv6cp_open, sppp_ipv6cp_close,
	sppp_ipv6cp_TO, sppp_ipv6cp_RCR, sppp_ipv6cp_RCN_rej, sppp_ipv6cp_RCN_nak,
	sppp_ipv6cp_tlu, sppp_ipv6cp_tld, sppp_ipv6cp_tls, sppp_ipv6cp_tlf,
	sppp_ipv6cp_scr
};

static const struct cp pap = {
	PPP_PAP, IDX_PAP, CP_AUTH, "pap",
	sppp_null, sppp_null, sppp_pap_open, sppp_pap_close,
	sppp_pap_TO, 0, 0, 0,
	sppp_pap_tlu, sppp_pap_tld, sppp_null, sppp_null,
	sppp_pap_scr
};

static const struct cp chap = {
	PPP_CHAP, IDX_CHAP, CP_AUTH, "chap",
	sppp_null, sppp_null, sppp_chap_open, sppp_chap_close,
	sppp_chap_TO, 0, 0, 0,
	sppp_chap_tlu, sppp_chap_tld, sppp_null, sppp_null,
	sppp_chap_scr
};

static const struct cp *cps[IDX_COUNT] = {
	&lcp,			/* IDX_LCP */
	&ipcp,			/* IDX_IPCP */
	&ipv6cp,		/* IDX_IPV6CP */
	&pap,			/* IDX_PAP */
	&chap,			/* IDX_CHAP */
};


/*
 * Exported functions, comprising our interface to the lower layer.
 */

/*
 * Process the received packet.
 */
void
sppp_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ppp_header *h;
	struct ifqueue *inq = 0;
	int s;
	struct sppp *sp = (struct sppp *)ifp;
	int debug = ifp->if_flags & IFF_DEBUG;

	if (ifp->if_flags & IFF_UP)
		/* Count received bytes, add FCS and one flag */
		ifp->if_ibytes += m->m_pkthdr.len + 3;

	if (m->m_pkthdr.len <= PPP_HEADER_LEN) {
		/* Too small packet, drop it. */
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "input packet is too small, %d bytes\n",
			    SPP_ARGS(ifp), m->m_pkthdr.len);
	  drop:
		++ifp->if_ierrors;
		++ifp->if_iqdrops;
		m_freem (m);
		return;
	}

	/* Get PPP header. */
	h = mtod (m, struct ppp_header*);
	m_adj (m, PPP_HEADER_LEN);

	switch (h->address) {
	case PPP_ALLSTATIONS:
		if (h->control != PPP_UI)
			goto invalid;
		if (sp->pp_flags & PP_CISCO) {
			if (debug)
				log(LOG_DEBUG,
				    SPP_FMT "PPP packet in Cisco mode "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    SPP_ARGS(ifp),
				    h->address, h->control, ntohs(h->protocol));
			goto drop;
		}
		switch (ntohs (h->protocol)) {
		default:
			if (sp->state[IDX_LCP] == STATE_OPENED)
				sppp_cp_send (sp, PPP_LCP, PROTO_REJ,
				    ++sp->pp_seq[IDX_LCP], m->m_pkthdr.len + 2,
				    &h->protocol);
			if (debug)
				log(LOG_DEBUG,
				    SPP_FMT "invalid input protocol "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    SPP_ARGS(ifp),
				    h->address, h->control, ntohs(h->protocol));
			++ifp->if_noproto;
			goto drop;
		case PPP_LCP:
			sppp_cp_input(&lcp, sp, m);
			m_freem (m);
			return;
		case PPP_PAP:
			if (sp->pp_phase >= PHASE_AUTHENTICATE)
				sppp_pap_input(sp, m);
			m_freem (m);
			return;
		case PPP_CHAP:
			if (sp->pp_phase >= PHASE_AUTHENTICATE)
				sppp_chap_input(sp, m);
			m_freem (m);
			return;
#ifdef INET
		case PPP_IPCP:
			if (sp->pp_phase == PHASE_NETWORK)
				sppp_cp_input(&ipcp, sp, m);
			m_freem (m);
			return;
		case PPP_IP:
			if (sp->state[IDX_IPCP] == STATE_OPENED) {
				schednetisr (NETISR_IP);
				inq = &ipintrq;
			}
			break;
#endif
#ifdef INET6
		case PPP_IPV6CP:
			if (sp->pp_phase == PHASE_NETWORK)
				sppp_cp_input(&ipv6cp, sp, m);
			m_freem (m);
			return;

		case PPP_IPV6:
			if (sp->state[IDX_IPV6CP] == STATE_OPENED) {
				schednetisr (NETISR_IPV6);
				inq = &ip6intrq;
			}
			break;
#endif
#ifdef IPX
		case PPP_IPX:
			/* IPX IPXCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
				schednetisr (NETISR_IPX);
				inq = &ipxintrq;
			}
			break;
#endif
#ifdef NS
		case PPP_XNS:
			/* XNS IDPCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
				schednetisr (NETISR_NS);
				inq = &nsintrq;
			}
			break;
#endif
#ifdef ISO
		case PPP_ISO:
			/* OSI NLCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
				schednetisr (NETISR_ISO);
				inq = &clnlintrq;
			}
			break;
#endif
		}
		break;
	case CISCO_MULTICAST:
	case CISCO_UNICAST:
		/* Don't check the control field here (RFC 1547). */
		if (! (sp->pp_flags & PP_CISCO)) {
			if (debug)
				log(LOG_DEBUG,
				    SPP_FMT "Cisco packet in PPP mode "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    SPP_ARGS(ifp),
				    h->address, h->control, ntohs(h->protocol));
			goto drop;
		}
		switch (ntohs (h->protocol)) {
		default:
			++ifp->if_noproto;
			goto invalid;
		case CISCO_KEEPALIVE:
			sppp_cisco_input ((struct sppp*) ifp, m);
			m_freem (m);
			return;
#ifdef INET
		case ETHERTYPE_IP:
			schednetisr (NETISR_IP);
			inq = &ipintrq;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			schednetisr (NETISR_IPV6);
			inq = &ip6intrq;
			break;
#endif
#ifdef IPX
		case ETHERTYPE_IPX:
			schednetisr (NETISR_IPX);
			inq = &ipxintrq;
			break;
#endif
#ifdef NS
		case ETHERTYPE_NS:
			schednetisr (NETISR_NS);
			inq = &nsintrq;
			break;
#endif
		}
		break;
	default:        /* Invalid PPP packet. */
	  invalid:
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "invalid input packet "
			    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
			    SPP_ARGS(ifp),
			    h->address, h->control, ntohs(h->protocol));
		goto drop;
	}

	if (! (ifp->if_flags & IFF_UP) || ! inq)
		goto drop;

	/* Check queue. */
	s = splimp();
	if (IF_QFULL (inq)) {
		/* Queue overflow. */
		IF_DROP(inq);
		splx(s);
		if (debug)
			log(LOG_DEBUG, SPP_FMT "protocol queue overflow\n",
				SPP_ARGS(ifp));
		goto drop;
	}
	IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Enqueue transmit packet.
 */
static int
sppp_output(struct ifnet *ifp, struct mbuf *m,
	    struct sockaddr *dst, struct rtentry *rt)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct ppp_header *h;
	struct ifqueue *ifq = NULL;		/* XXX */
	int s, len, rv = 0;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	s = splimp();

	if ((ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == 0) {
		m_freem (m);
		splx (s);
		return (ENETDOWN);
	}

	if ((ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == IFF_AUTO) {
		/*
		 * Interface is not yet running, but auto-dial.  Need
		 * to start LCP for it.
		 */
		ifp->if_flags |= IFF_RUNNING;
		splx(s);
		lcp.Open(sp);
		s = splimp();
	}

	/*
	 * If the queueing discipline needs packet classification,
	 * do it before prepending link headers.
	 */
	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

#ifdef INET
	if (dst->sa_family == AF_INET)
	{
		/* Check mbuf length here??? */
		struct ip *ip = mtod (m, struct ip*);
		struct tcphdr *tcp = (struct tcphdr*) ((long*)ip + ip->ip_hl);

		/*
		 * When using dynamic local IP address assignment by using
		 * 0.0.0.0 as a local address, the first TCP session will
		 * not connect because the local TCP checksum is computed
		 * using 0.0.0.0 which will later become our real IP address
		 * so the TCP checksum computed at the remote end will
		 * become invalid. So we
		 * - don't let packets with src ip addr 0 thru
		 * - we flag TCP packets with src ip 0 as an error
		 */	

		if(ip->ip_src.s_addr == INADDR_ANY)	/* -hm */
		{
			m_freem(m);
			splx(s);
			if(ip->ip_p == IPPROTO_TCP)
				return(EADDRNOTAVAIL);
			else
				return(0);
		}
		
		/*
		 * Put low delay, telnet, rlogin and ftp control packets
		 * in front of the queue.
		 */
		 
		if (! IF_QFULL (&sp->pp_fastq) &&
		    ((ip->ip_tos & IPTOS_LOWDELAY) ||
	    	    ((ip->ip_p == IPPROTO_TCP &&
	    	    m->m_len >= sizeof (struct ip) + sizeof (struct tcphdr) &&
	    	    (INTERACTIVE (ntohs (tcp->th_sport)))) ||
	    	    INTERACTIVE (ntohs (tcp->th_dport)))))
			ifq = &sp->pp_fastq;
	}
#endif

#ifdef INET6
	if (dst->sa_family == AF_INET6) {
		/* XXX do something tricky here? */
	}
#endif

	/*
	 * Prepend general data packet PPP header. For now, IP only.
	 */
	M_PREPEND (m, PPP_HEADER_LEN, M_DONTWAIT);
	if (! m) {
		if (ifp->if_flags & IFF_DEBUG)
			log(LOG_DEBUG, SPP_FMT "no memory for transmit header\n",
				SPP_ARGS(ifp));
		++ifp->if_oerrors;
		splx (s);
		return (ENOBUFS);
	}
	/*
	 * May want to check size of packet
	 * (albeit due to the implementation it's always enough)
	 */
	h = mtod (m, struct ppp_header*);
	if (sp->pp_flags & PP_CISCO) {
		h->address = CISCO_UNICAST;        /* unicast address */
		h->control = 0;
	} else {
		h->address = PPP_ALLSTATIONS;        /* broadcast address */
		h->control = PPP_UI;                 /* Unnumbered Info */
	}

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:   /* Internet Protocol */
		if (sp->pp_flags & PP_CISCO)
			h->protocol = htons (ETHERTYPE_IP);
		else {
			/*
			 * Don't choke with an ENETDOWN early.  It's
			 * possible that we just started dialing out,
			 * so don't drop the packet immediately.  If
			 * we notice that we run out of buffer space
			 * below, we will however remember that we are
			 * not ready to carry IP packets, and return
			 * ENETDOWN, as opposed to ENOBUFS.
			 */
			h->protocol = htons(PPP_IP);
			if (sp->state[IDX_IPCP] != STATE_OPENED)
				rv = ENETDOWN;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:   /* Internet Protocol version 6 */
		if (sp->pp_flags & PP_CISCO)
			h->protocol = htons (ETHERTYPE_IPV6);
		else {
			/*
			 * Don't choke with an ENETDOWN early.  It's
			 * possible that we just started dialing out,
			 * so don't drop the packet immediately.  If
			 * we notice that we run out of buffer space
			 * below, we will however remember that we are
			 * not ready to carry IP packets, and return
			 * ENETDOWN, as opposed to ENOBUFS.
			 */
			h->protocol = htons(PPP_IPV6);
			if (sp->state[IDX_IPV6CP] != STATE_OPENED)
				rv = ENETDOWN;
		}
		break;
#endif
#ifdef NS
	case AF_NS:     /* Xerox NS Protocol */
		h->protocol = htons ((sp->pp_flags & PP_CISCO) ?
			ETHERTYPE_NS : PPP_XNS);
		break;
#endif
#ifdef IPX
	case AF_IPX:     /* Novell IPX Protocol */
		h->protocol = htons ((sp->pp_flags & PP_CISCO) ?
			ETHERTYPE_IPX : PPP_IPX);
		break;
#endif
#ifdef ISO
	case AF_ISO:    /* ISO OSI Protocol */
		if (sp->pp_flags & PP_CISCO)
			goto nosupport;
		h->protocol = htons (PPP_ISO);
		break;
nosupport:
#endif
	default:
		m_freem (m);
		++ifp->if_oerrors;
		splx (s);
		return (EAFNOSUPPORT);
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	len = m->m_pkthdr.len;
	if (ifq != NULL
#ifdef ALTQ
	    && ALTQ_IS_ENABLED(&ifp->if_snd) == 0
#endif
	    ) {
		if (IF_QFULL (ifq)) {
			IF_DROP (&ifp->if_snd);
			m_freem (m);
			if (rv == 0)
				rv = ENOBUFS;
		}
		IF_ENQUEUE(ifq, m);
	} else
		IFQ_ENQUEUE(&ifp->if_snd, m, &pktattr, rv);
	if (rv != 0) {
		++ifp->if_oerrors;
		splx(s);
		return (rv);
	}

	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);

	/*
	 * Count output packets and bytes.
	 * The packet length includes header, FCS and 1 flag,
	 * according to RFC 1333.
	 */
	ifp->if_obytes += len + 3;
	splx (s);
	return (0);
}

void
sppp_attach(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;

	/* Initialize keepalive handler. */
	if (! spppq) {
#if defined(__NetBSD__)
		callout_init(&keepalive_ch);
		callout_reset(&keepalive_ch, hz * 10, sppp_keepalive, NULL);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		keepalive_ch = 
#endif
		timeout(sppp_keepalive, 0, hz * 10);
#endif /* __NetBSD__ */
	}

	/* Insert new entry into the keepalive list. */
	sp->pp_next = spppq;
	spppq = sp;

	sp->pp_if.if_type = IFT_PPP;
	sp->pp_if.if_output = sppp_output;
	sp->pp_fastq.ifq_maxlen = 32;
	sp->pp_cpq.ifq_maxlen = 20;
	sp->pp_loopcnt = 0;
	sp->pp_alivecnt = 0;
	bzero(&sp->pp_seq[0], sizeof(sp->pp_seq));
	bzero(&sp->pp_rseq[0], sizeof(sp->pp_rseq));
	sp->pp_phase = PHASE_DEAD;
	sp->pp_up = lcp.Up;
	sp->pp_down = lcp.Down;

	if_alloc_sadl(ifp);

	sppp_lcp_init(sp);
	sppp_ipcp_init(sp);
	sppp_ipv6cp_init(sp);
	sppp_pap_init(sp);
	sppp_chap_init(sp);
}

void
sppp_detach(struct ifnet *ifp)
{
	struct sppp **q, *p, *sp = (struct sppp*) ifp;
	int i;

	/* Remove the entry from the keepalive list. */
	for (q = &spppq; (p = *q); q = &p->pp_next)
		if (p == sp) {
			*q = p->pp_next;
			break;
		}

	/* Stop keepalive handler. */
	if (! spppq) {
#if defined(__NetBSD__)
		callout_stop(&keepalive_ch);
#else
		untimeout(sppp_keepalive, 0
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		,keepalive_ch
#endif
		);
#endif /* __NetBSD__ */
	}

	for (i = 0; i < IDX_COUNT; i++) {
#if defined(__NetBSD__)
		callout_stop(&sp->ch[i]);
#else
		untimeout((cps[i])->TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		, sp->ch[i]
#endif
		);
#endif /* __NetBSD__ */
	}
#if defined(__NetBSD__)
	callout_stop(&sp->pap_my_to_ch);
#else
	untimeout(sppp_pap_my_TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	, sp->pap_my_to_ch
#endif
	);
#endif /* __NetBSD__ */

	if_free_sadl(ifp);
}

/*
 * Flush the interface output queue.
 */
void
sppp_flush(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;

	IFQ_PURGE (&sp->pp_if.if_snd);
	IFQ_PURGE (&sp->pp_fastq);
	IFQ_PURGE (&sp->pp_cpq);
}

/*
 * Check if the output queue is empty.
 */
int
sppp_isempty(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	int empty, s;

	s = splimp();
	empty = !sp->pp_fastq.ifq_head && !sp->pp_cpq.ifq_head &&
		!sp->pp_if.if_snd.ifq_head;
	splx(s);
	return (empty);
}

/*
 * Get next packet to send.
 */
struct mbuf *
sppp_dequeue(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct mbuf *m;
	int s;

	s = splimp();
	/*
	 * Process only the control protocol queue until we have at
	 * least one NCP open.
	 *
	 * Do always serve all three queues in Cisco mode.
	 */
	IF_DEQUEUE(&sp->pp_cpq, m);
	if (m == NULL &&
	    (sppp_ncp_check(sp) || (sp->pp_flags & PP_CISCO) != 0)) {
		IF_DEQUEUE(&sp->pp_fastq, m);
		if (m == NULL)
			IF_DEQUEUE (&sp->pp_if.if_snd, m);
	}
	splx(s);
	return m;
}

/*
 * Pick the next packet, do not remove it from the queue.
 */
struct mbuf *
sppp_pick(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*)ifp;
	struct mbuf *m;
	int s;

	s= splimp ();

	m = sp->pp_cpq.ifq_head;
	if (m == NULL &&
	    (sp->pp_phase == PHASE_NETWORK ||
	     (sp->pp_flags & PP_CISCO) != 0))
		if ((m = sp->pp_fastq.ifq_head) == NULL)
			m = sp->pp_if.if_snd.ifq_head;
	splx (s);
	return (m);
}

/*
 * Process an ioctl request.  Called on low priority level.
 */
int
#if defined(__FreeBSD_version) && __FreeBSD_version >= 300003
sppp_ioctl(struct ifnet *ifp, u_long cmd, void *data)
#else
sppp_ioctl(struct ifnet *ifp, int cmd, void *data)
#endif
{
	struct ifreq *ifr = (struct ifreq*) data;
	struct sppp *sp = (struct sppp*) ifp;
	int s, rv, going_up, going_down, newmode;

	s = splimp();
	rv = 0;
	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
		break;

	case SIOCSIFADDR:
		if_up(ifp);
		/* fall through... */

	case SIOCSIFFLAGS:
		going_up = ifp->if_flags & IFF_UP &&
			(ifp->if_flags & IFF_RUNNING) == 0;
		going_down = (ifp->if_flags & IFF_UP) == 0 &&
			ifp->if_flags & IFF_RUNNING;
		newmode = ifp->if_flags & (IFF_AUTO | IFF_PASSIVE);
		if (newmode == (IFF_AUTO | IFF_PASSIVE)) {
			/* sanity */
			newmode = IFF_PASSIVE;
			ifp->if_flags &= ~IFF_AUTO;
		}

		if (going_up || going_down)
			lcp.Close(sp);
		if (going_up && newmode == 0) {
			/* neither auto-dial nor passive */
			ifp->if_flags |= IFF_RUNNING;
			if (!(sp->pp_flags & PP_CISCO))
				lcp.Open(sp);
		} else if (going_down) {
			sppp_flush(ifp);
			ifp->if_flags &= ~IFF_RUNNING;
		}

		break;

#ifdef SIOCSIFMTU
#ifndef ifr_mtu
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 128 || ifr->ifr_mtu > sp->lcp.their_mru)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif
#ifdef SLIOCSETMTU
	case SLIOCSETMTU:
		if (*(short*)data < 128 || *(short*)data > sp->lcp.their_mru)
			return (EINVAL);
		ifp->if_mtu = *(short*)data;
		break;
#endif
#ifdef SIOCGIFMTU
	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;
#endif
#ifdef SLIOCGETMTU
	case SLIOCGETMTU:
		*(short*)data = ifp->if_mtu;
		break;
#endif
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCGIFGENERIC:
	case SIOCSIFGENERIC:
		rv = sppp_params(sp, cmd, data);
		break;

	default:
		rv = ENOTTY;
	}
	splx(s);
	return rv;
}


/*
 * Cisco framing implementation.
 */

/*
 * Handle incoming Cisco keepalive protocol packets.
 */
static void
sppp_cisco_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct cisco_packet *h;
	u_long me, mymask;

	if (m->m_pkthdr.len < CISCO_PACKET_LEN) {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "cisco invalid packet length: %d bytes\n",
			    SPP_ARGS(ifp), m->m_pkthdr.len);
		return;
	}
	h = mtod (m, struct cisco_packet*);
	if (debug)
		log(LOG_DEBUG,
		    SPP_FMT "cisco input: %d bytes "
		    "<0x%lx 0x%lx 0x%lx 0x%x 0x%x-0x%x>\n",
		    SPP_ARGS(ifp), m->m_pkthdr.len,
		    (u_long)ntohl (h->type), (u_long)h->par1, (u_long)h->par2, (u_int)h->rel,
		    (u_int)h->time0, (u_int)h->time1);
	switch (ntohl (h->type)) {
	default:
		if (debug)
			addlog(SPP_FMT "cisco unknown packet type: 0x%lx\n",
			       SPP_ARGS(ifp), (u_long)ntohl (h->type));
		break;
	case CISCO_ADDR_REPLY:
		/* Reply on address request, ignore */
		break;
	case CISCO_KEEPALIVE_REQ:
		sp->pp_alivecnt = 0;
		sp->pp_rseq[IDX_LCP] = ntohl (h->par1);
		if (sp->pp_seq[IDX_LCP] == sp->pp_rseq[IDX_LCP]) {
			/* Local and remote sequence numbers are equal.
			 * Probably, the line is in loopback mode. */
			if (sp->pp_loopcnt >= MAXALIVECNT) {
				printf (SPP_FMT "loopback\n",
					SPP_ARGS(ifp));
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down (ifp);
					IFQ_PURGE (&sp->pp_cpq);
				}
			}
			++sp->pp_loopcnt;

			/* Generate new local sequence number */
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
			sp->pp_seq[IDX_LCP] = random();
#else
			sp->pp_seq[IDX_LCP] ^= time.tv_sec ^ time.tv_usec;
#endif
			break;
		}
		sp->pp_loopcnt = 0;
		if (! (ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			if_up(ifp);
			printf (SPP_FMT "up\n", SPP_ARGS(ifp));
		}
		break;
	case CISCO_ADDR_REQ:
		sppp_get_ip_addrs(sp, &me, 0, &mymask);
		if (me != 0L)
			sppp_cisco_send(sp, CISCO_ADDR_REPLY, me, mymask);
		break;
	}
}

/*
 * Send Cisco keepalive packet.
 */
static void
sppp_cisco_send(struct sppp *sp, int type, long par1, long par2)
{
	STDDCL;
	struct ppp_header *h;
	struct cisco_packet *ch;
	struct mbuf *m;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct timeval tv;
#else
	u_long t = (time.tv_sec - boottime.tv_sec) * 1000;
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	getmicrouptime(&tv);
#endif
	
	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + CISCO_PACKET_LEN;
	m->m_pkthdr.rcvif = 0;

	h = mtod (m, struct ppp_header*);
	h->address = CISCO_MULTICAST;
	h->control = 0;
	h->protocol = htons (CISCO_KEEPALIVE);

	ch = (struct cisco_packet*) (h + 1);
	ch->type = htonl (type);
	ch->par1 = htonl (par1);
	ch->par2 = htonl (par2);
	ch->rel = -1;

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	ch->time0 = htons ((u_short) (tv.tv_sec >> 16));
	ch->time1 = htons ((u_short) tv.tv_sec);
#else
	ch->time0 = htons ((u_short) (t >> 16));
	ch->time1 = htons ((u_short) t);
#endif

	if (debug)
		log(LOG_DEBUG,
		    SPP_FMT "cisco output: <0x%lx 0x%lx 0x%lx 0x%x 0x%x-0x%x>\n",
			SPP_ARGS(ifp), (u_long)ntohl (ch->type), (u_long)ch->par1,
			(u_long)ch->par2, (u_int)ch->rel, (u_int)ch->time0, (u_int)ch->time1);

	if (IF_QFULL (&sp->pp_cpq)) {
		IF_DROP (&sp->pp_fastq);
		IF_DROP (&ifp->if_snd);
		m_freem (m);
	} else
		IF_ENQUEUE (&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * PPP protocol implementation.
 */

/*
 * Send PPP control protocol packet.
 */
static void
sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
	     u_char ident, u_short len, void *data)
{
	STDDCL;
	struct ppp_header *h;
	struct lcp_header *lh;
	struct mbuf *m;

	if (len > MHLEN - PPP_HEADER_LEN - LCP_HEADER_LEN)
		len = MHLEN - PPP_HEADER_LEN - LCP_HEADER_LEN;
	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + LCP_HEADER_LEN + len;
	m->m_pkthdr.rcvif = 0;

	h = mtod (m, struct ppp_header*);
	h->address = PPP_ALLSTATIONS;        /* broadcast address */
	h->control = PPP_UI;                 /* Unnumbered Info */
	h->protocol = htons (proto);         /* Link Control Protocol */

	lh = (struct lcp_header*) (h + 1);
	lh->type = type;
	lh->ident = ident;
	lh->len = htons (LCP_HEADER_LEN + len);
	if (len)
		bcopy (data, lh+1, len);

	if (debug) {
		log(LOG_DEBUG, SPP_FMT "%s output <%s id=0x%x len=%d",
		    SPP_ARGS(ifp),
		    sppp_proto_name(proto),
		    sppp_cp_type_name (lh->type), lh->ident,
		    ntohs (lh->len));
		if (len)
			sppp_print_bytes ((u_char*) (lh+1), len);
		addlog(">\n");
	}
	if (IF_QFULL (&sp->pp_cpq)) {
		IF_DROP (&sp->pp_fastq);
		IF_DROP (&ifp->if_snd);
		m_freem (m);
		++ifp->if_oerrors;
	} else
		IF_ENQUEUE (&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Handle incoming PPP control protocol packets.
 */
static void
sppp_cp_input(const struct cp *cp, struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len = m->m_pkthdr.len;
	int rv;
	u_char *p;

	if (len < 4) {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "%s invalid packet length: %d bytes\n",
			    SPP_ARGS(ifp), cp->name, len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (debug) {
		log(LOG_DEBUG,
		    SPP_FMT "%s input(%s): <%s id=0x%x len=%d",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sppp_cp_type_name (h->type), h->ident, ntohs (h->len));
		if (len > 4)
			sppp_print_bytes ((u_char*) (h+1), len-4);
		addlog(">\n");
	}
	if (len > ntohs (h->len))
		len = ntohs (h->len);
	p = (u_char *)(h + 1);
	switch (h->type) {
	case CONF_REQ:
		if (len < 4) {
			if (debug)
				addlog(SPP_FMT "%s invalid conf-req length %d\n",
				       SPP_ARGS(ifp), cp->name,
				       len);
			++ifp->if_ierrors;
			break;
		}
		/* handle states where RCR doesn't get a SCA/SCN */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
		case STATE_STOPPING:
			return;
		case STATE_CLOSED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident,
				     0, 0);
			return;
		}
		rv = (cp->RCR)(sp, h, len);
		switch (sp->state[cp->protoidx]) {
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			/* fall through... */
		case STATE_ACK_SENT:
		case STATE_REQ_SENT:
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_STOPPED:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_ACK_RCVD:
			if (rv) {
				sppp_cp_change_state(cp, sp, STATE_OPENED);
				if (debug)
					log(LOG_DEBUG, SPP_FMT "%s tlu\n",
					    SPP_ARGS(ifp),
					    cp->name);
				(cp->tlu)(sp);
			} else
				sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_ACK:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog(SPP_FMT "%s id mismatch 0x%x != 0x%x\n",
				       SPP_ARGS(ifp), cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		case STATE_REQ_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_OPENED);
			if (debug)
				log(LOG_DEBUG, SPP_FMT "%s tlu\n",
				       SPP_ARGS(ifp), cp->name);
			(cp->tlu)(sp);
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_NAK:
	case CONF_REJ:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog(SPP_FMT "%s id mismatch 0x%x != 0x%x\n",
				       SPP_ARGS(ifp), cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		if (h->type == CONF_NAK)
			(cp->RCN_nak)(sp, h, len);
		else /* CONF_REJ */
			(cp->RCN_rej)(sp, h, len);

		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_ACK_SENT);
			(cp->scr)(sp);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;

	case TERM_REQ:
		switch (sp->state[cp->protoidx]) {
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			/* fall through */
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_REQ_SENT:
		  sta:
			/* Send Terminate-Ack packet. */
			if (debug)
				log(LOG_DEBUG, SPP_FMT "%s send terminate-ack\n",
				    SPP_ARGS(ifp), cp->name);
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			sp->rst_counter[cp->protoidx] = 0;
			sppp_cp_change_state(cp, sp, STATE_STOPPING);
			goto sta;
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case TERM_ACK:
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			break;
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CODE_REJ:
		/* XXX catastrophic rejects (RXJ-) aren't handled yet. */
		log(LOG_INFO,
		    SPP_FMT "%s: ignoring RXJ (%s) for code ?, "
		    "danger will robinson\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_cp_type_name(h->type));
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_OPENED:
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case PROTO_REJ:
	    {
		int catastrophic;
		const struct cp *upper;
		int i;
		u_int16_t proto;

		catastrophic = 0;
		upper = NULL;
		proto = ntohs(*((u_int16_t *)p));
		for (i = 0; i < IDX_COUNT; i++) {
			if (cps[i]->proto == proto) {
				upper = cps[i];
				break;
			}
		}
		if (upper == NULL)
			catastrophic++;

		log(LOG_INFO,
		    SPP_FMT "%s: RXJ%c (%s) for proto 0x%x (%s/%s)\n",
		    SPP_ARGS(ifp), cp->name, catastrophic ? '-' : '+',
		    sppp_cp_type_name(h->type), proto,
		    upper ? upper->name : "unknown",
		    upper ? sppp_state_name(sp->state[upper->protoidx]) : "?");

		/*
		 * if we got RXJ+ against conf-req, the peer does not implement
		 * this particular protocol type.  terminate the protocol.
		 */
		if (upper && !catastrophic) {
			if (sp->state[upper->protoidx] == STATE_REQ_SENT) {
				upper->Close(sp);
				break;
			}
		}

		/* XXX catastrophic rejects (RXJ-) aren't handled yet. */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_OPENED:
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		default:
			printf(SPP_FMT "%s illegal %s in state %s\n",
			       SPP_ARGS(ifp), cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	    }
	case DISC_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		/* Discard the packet. */
		break;
	case ECHO_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (sp->state[cp->protoidx] != STATE_OPENED) {
			if (debug)
				addlog(SPP_FMT "lcp echo req but lcp closed\n",
				       SPP_ARGS(ifp));
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog(SPP_FMT "invalid lcp echo request "
				       "packet length: %d bytes\n",
				       SPP_ARGS(ifp), len);
			break;
		}
		if (ntohl (*(long*)(h+1)) == sp->lcp.magic) {
			/* Line loopback mode detected. */
			printf(SPP_FMT "loopback\n", SPP_ARGS(ifp));
			if_down (ifp);
			IFQ_PURGE (&sp->pp_cpq);

			/* Shut down the PPP link. */
			/* XXX */
			lcp.Down(sp);
			lcp.Up(sp);
			break;
		}
		*(long*)(h+1) = htonl (sp->lcp.magic);
		if (debug)
			addlog(SPP_FMT "got lcp echo req, sending echo rep\n",
			       SPP_ARGS(ifp));
		sppp_cp_send (sp, PPP_LCP, ECHO_REPLY, h->ident, len-4, h+1);
		break;
	case ECHO_REPLY:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (h->ident != sp->lcp.echoid) {
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog(SPP_FMT "lcp invalid echo reply "
				       "packet length: %d bytes\n",
				       SPP_ARGS(ifp), len);
			break;
		}
		if (debug)
			addlog(SPP_FMT "lcp got echo rep\n",
			       SPP_ARGS(ifp));
		if (ntohl (*(long*)(h+1)) != sp->lcp.magic)
			sp->pp_alivecnt = 0;
		break;
	default:
		/* Unknown packet type -- send Code-Reject packet. */
	  illegal:
		if (debug)
			addlog(SPP_FMT "%s send code-rej for 0x%x\n",
			       SPP_ARGS(ifp), cp->name, h->type);
		sppp_cp_send(sp, cp->proto, CODE_REJ,
		    ++sp->pp_seq[cp->protoidx], m->m_pkthdr.len, h);
		++ifp->if_ierrors;
	}
}


/*
 * The generic part of all Up/Down/Open/Close/TO event handlers.
 * Basically, the state transition handling in the automaton.
 */
static void
sppp_up_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s up(%s)\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STARTING:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	default:
		printf(SPP_FMT "%s illegal up in state %s\n",
		       SPP_ARGS(ifp), cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}

static void
sppp_down_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s down(%s)\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_CLOSED:
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		break;
	case STATE_STOPPED:
		(cp->tls)(sp);
		/* fall through */
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	default:
		printf(SPP_FMT "%s illegal down in state %s\n",
		       SPP_ARGS(ifp), cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}


static void
sppp_open_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s open(%s)\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		(cp->tls)(sp);
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	case STATE_STARTING:
		break;
	case STATE_CLOSED:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	case STATE_STOPPED:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_STOPPING);
		break;
	}
}


static void
sppp_close_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s close(%s)\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
	case STATE_CLOSED:
	case STATE_CLOSING:
		break;
	case STATE_STARTING:
		(cp->tlf)(sp);
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		break;
	case STATE_STOPPED:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STOPPING:
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		/* fall through */
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_terminate;
		sppp_cp_send(sp, cp->proto, TERM_REQ,
		    ++sp->pp_seq[cp->protoidx], 0, 0);
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	}
}

static void
sppp_to_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;
	int s;

	s = splimp();
	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s TO(%s) rst_counter = %d\n",
		    SPP_ARGS(ifp), cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sp->rst_counter[cp->protoidx]);

	if (--sp->rst_counter[cp->protoidx] < 0)
		/* TO- event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		}
	else
		/* TO+ event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
		case STATE_STOPPING:
			sppp_cp_send(sp, cp->proto, TERM_REQ,
			    ++sp->pp_seq[cp->protoidx], 0, 0);
#if defined(__NetBSD__)
			callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
			    cp->TO, sp);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
			sp->ch[cp->protoidx] =
#endif
			timeout(cp->TO, (void *)sp, sp->lcp.timeout);
#endif /* __NetBSD__ */
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			(cp->scr)(sp);
#if defined(__NetBSD__)
			callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
			    cp->TO, sp);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
			sp->ch[cp->protoidx] = 
#endif
			timeout(cp->TO, (void *)sp, sp->lcp.timeout);
#endif /* __NetBSD__ */
			break;
		}

	splx(s);
}

/*
 * Change the state of a control protocol in the state automaton.
 * Takes care of starting/stopping the restart timer.
 */
void
sppp_cp_change_state(const struct cp *cp, struct sppp *sp, int newstate)
{
	sp->state[cp->protoidx] = newstate;

#if defined(__NetBSD__)
	callout_stop(&sp->ch[cp->protoidx]);
#else
	untimeout(cp->TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	, sp->ch[cp->protoidx]
#endif
	);
#endif /* __NetBSD__ */
	switch (newstate) {
	case STATE_INITIAL:
	case STATE_STARTING:
	case STATE_CLOSED:
	case STATE_STOPPED:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
#if defined(__NetBSD__)
		callout_reset(&sp->ch[cp->protoidx], sp->lcp.timeout,
		    cp->TO, sp);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		sp->ch[cp->protoidx]  =
#endif
		timeout(cp->TO, (void *)sp, sp->lcp.timeout);
#endif /* __NetBSD__ */
		break;
	}
}
/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                         The LCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */
static void
sppp_lcp_init(struct sppp *sp)
{
	sp->lcp.opts = (1 << LCP_OPT_MAGIC);
	sp->lcp.magic = 0;
	sp->state[IDX_LCP] = STATE_INITIAL;
	sp->fail_counter[IDX_LCP] = 0;
	sp->pp_seq[IDX_LCP] = 0;
	sp->pp_rseq[IDX_LCP] = 0;
	sp->lcp.protos = 0;
	sp->lcp.mru = sp->lcp.their_mru = PP_MTU;

	/*
	 * Initialize counters and timeout values.  Note that we don't
	 * use the 3 seconds suggested in RFC 1661 since we are likely
	 * running on a fast link.  XXX We should probably implement
	 * the exponential backoff option.  Note that these values are
	 * relevant for all control protocols, not just LCP only.
	 */
	sp->lcp.timeout = 1 * hz;
	sp->lcp.max_terminate = 2;
	sp->lcp.max_configure = 10;
	sp->lcp.max_failure = 10;
#if defined(__NetBSD__)
	callout_init(&sp->ch[IDX_LCP]);
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sp->ch[IDX_LCP]);
#endif
}

static void
sppp_lcp_up(struct sppp *sp)
{
	STDDCL;

	/*
	 * If this interface is passive or dial-on-demand, and we are
	 * still in Initial state, it means we've got an incoming
	 * call.  Activate the interface.
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) != 0) {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "Up event", SPP_ARGS(ifp));
		ifp->if_flags |= IFF_RUNNING;
		if (sp->state[IDX_LCP] == STATE_INITIAL) {
			if (debug)
				addlog("(incoming call)\n");
			sp->pp_flags |= PP_CALLIN;
			lcp.Open(sp);
		} else if (debug)
			addlog("\n");
	}

	sppp_up_event(&lcp, sp);
}

static void
sppp_lcp_down(struct sppp *sp)
{
	STDDCL;

	sppp_down_event(&lcp, sp);

	/*
	 * If this is neither a dial-on-demand nor a passive
	 * interface, simulate an ``ifconfig down'' action, so the
	 * administrator can force a redial by another ``ifconfig
	 * up''.  XXX For leased line operation, should we immediately
	 * try to reopen the connection here?
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) == 0) {
		log(LOG_INFO,
		    SPP_FMT "Down event (carrier loss), taking interface down.\n",
		    SPP_ARGS(ifp));
		if_down(ifp);
	} else {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "Down event (carrier loss)\n",
			    SPP_ARGS(ifp));
	}
	sp->pp_flags &= ~PP_CALLIN;
	if (sp->state[IDX_LCP] != STATE_INITIAL)
		lcp.Close(sp);
	ifp->if_flags &= ~IFF_RUNNING;
}

static void
sppp_lcp_open(struct sppp *sp)
{
	/*
	 * If we are authenticator, negotiate LCP_AUTH
	 */
	if (sp->hisauth.proto != 0)
		sp->lcp.opts |= (1 << LCP_OPT_AUTH_PROTO);
	else
		sp->lcp.opts &= ~(1 << LCP_OPT_AUTH_PROTO);
	sp->pp_flags &= ~PP_NEEDAUTH;
	sppp_open_event(&lcp, sp);
}

static void
sppp_lcp_close(struct sppp *sp)
{
	sppp_close_event(&lcp, sp);
}

static void
sppp_lcp_TO(void *cookie)
{
	sppp_to_event(&lcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *r, *p;
	int origlen, rlen;
	u_long nmagic;
	u_short authproto;

	len -= 4;
	origlen = len;
	buf = r = malloc (len, M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	if (debug)
		log(LOG_DEBUG, SPP_FMT "lcp parse opts:",
		    SPP_ARGS(ifp));

	/* pass 1: check for things that need to be rejected */
	p = (void*) (h+1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number. */
			/* fall through, both are same length */
		case LCP_OPT_ASYNC_MAP:
			/* Async control character map. */
			if (len >= 6 || p[1] == 6)
				continue;
			if (debug)
				addlog(" [invalid]");
			break;
		case LCP_OPT_MRU:
			/* Maximum receive unit. */
			if (len >= 4 && p[1] == 4)
				continue;
			if (debug)
				addlog(" [invalid]");
			break;
		case LCP_OPT_AUTH_PROTO:
			if (len < 4) {
				if (debug)
					addlog(" [invalid]");
				break;
			}
			authproto = (p[2] << 8) + p[3];
			if (authproto == PPP_CHAP && p[1] != 5) {
				if (debug)
					addlog(" [invalid chap len]");
				break;
			}
			if (sp->myauth.proto == 0) {
				/* we are not configured to do auth */
				if (debug)
					addlog(" [not configured]");
				break;
			}
			/*
			 * Remote want us to authenticate, remember this,
			 * so we stay in PHASE_AUTHENTICATE after LCP got
			 * up.
			 */
			sp->pp_flags |= PP_NEEDAUTH;
			continue;
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send (sp, PPP_LCP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/*
	 * pass 2: check for option values that are unacceptable and
	 * thus require to be nak'ed.
	 */
	if (debug)
		log(LOG_DEBUG, SPP_FMT "lcp parse opt values: ",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- extract. */
			nmagic = (u_long)p[2] << 24 |
				(u_long)p[3] << 16 | p[4] << 8 | p[5];
			if (nmagic != sp->lcp.magic) {
				if (debug)
					addlog(" 0x%lx", nmagic);
				continue;
			}
			/*
			 * Local and remote magics equal -- loopback?
			 */
			if (sp->pp_loopcnt >= MAXALIVECNT*5) {
				printf (SPP_FMT "loopback\n",
					SPP_ARGS(ifp));
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down(ifp);
					IFQ_PURGE(&sp->pp_cpq);
					/* XXX ? */
					lcp.Down(sp);
					lcp.Up(sp);
				}
			} else if (debug)
				addlog(" [glitch]");
			++sp->pp_loopcnt;
			/*
			 * We negate our magic here, and NAK it.  If
			 * we see it later in an NAK packet, we
			 * suggest a new one.
			 */
			nmagic = ~sp->lcp.magic;
			/* Gonna NAK it. */
			p[2] = nmagic >> 24;
			p[3] = nmagic >> 16;
			p[4] = nmagic >> 8;
			p[5] = nmagic;
			break;

		case LCP_OPT_ASYNC_MAP:
			/* Async control character map -- check to be zero. */
			if (! p[2] && ! p[3] && ! p[4] && ! p[5]) {
				if (debug)
					addlog(" [empty]");
				continue;
			}
			if (debug)
				addlog(" [non-empty]");
			/* suggest a zero one */
			p[2] = p[3] = p[4] = p[5] = 0;
			break;

		case LCP_OPT_MRU:
			/*
			 * Maximum receive unit.  Always agreeable,
			 * but ignored by now.
			 */
			sp->lcp.their_mru = p[2] * 256 + p[3];
			if (debug)
				addlog(" %ld", sp->lcp.their_mru);
			continue;

		case LCP_OPT_AUTH_PROTO:
			authproto = (p[2] << 8) + p[3];
			if (sp->myauth.proto != authproto) {
				/* not agreed, nak */
				if (debug)
					addlog(" [mine %s != his %s]",
					       sppp_proto_name(sp->hisauth.proto),
					       sppp_proto_name(authproto));
				p[2] = sp->myauth.proto >> 8;
				p[3] = sp->myauth.proto;
				break;
			}
			if (authproto == PPP_CHAP && p[4] != CHAP_MD5) {
				if (debug)
					addlog(" [chap not MD5]");
				p[4] = CHAP_MD5;
				break;
			}
			continue;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (++sp->fail_counter[IDX_LCP] >= sp->lcp.max_failure) {
			if (debug)
				addlog(" max_failure (%d) exceeded, "
				       "send conf-rej\n",
				       sp->lcp.max_failure);
			sppp_cp_send(sp, PPP_LCP, CONF_REJ, h->ident, rlen, buf);
		} else {
			if (debug)
				addlog(" send conf-nak\n");
			sppp_cp_send (sp, PPP_LCP, CONF_NAK, h->ident, rlen, buf);
		}
		goto end;
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sp->fail_counter[IDX_LCP] = 0;
		sp->pp_loopcnt = 0;
		sppp_cp_send (sp, PPP_LCP, CONF_ACK,
			      h->ident, origlen, h+1);
	}

 end:
	free (buf, M_TEMP);
	return (rlen == 0);
}

/*
 * Analyze the LCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "lcp rej opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- can't use it, use 0 */
			sp->lcp.opts &= ~(1 << LCP_OPT_MAGIC);
			sp->lcp.magic = 0;
			break;
		case LCP_OPT_MRU:
			/*
			 * Should not be rejected anyway, since we only
			 * negotiate a MRU if explicitly requested by
			 * peer.
			 */
			sp->lcp.opts &= ~(1 << LCP_OPT_MRU);
			break;
		case LCP_OPT_AUTH_PROTO:
			/*
			 * Peer doesn't want to authenticate himself,
			 * deny unless this is a dialout call, and
			 * AUTHFLAG_NOCALLOUT is set.
			 */
			if ((sp->pp_flags & PP_CALLIN) == 0 &&
			    (sp->hisauth.flags & AUTHFLAG_NOCALLOUT) != 0) {
				if (debug)
					addlog(" [don't insist on auth "
					       "for callout]");
				sp->lcp.opts &= ~(1 << LCP_OPT_AUTH_PROTO);
				break;
			}
			if (debug)
				addlog("[access denied]\n");
			lcp.Close(sp);
			break;
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

/*
 * Analyze the LCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;
	u_long magic;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "lcp nak opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- renegotiate */
			if ((sp->lcp.opts & (1 << LCP_OPT_MAGIC)) &&
			    len >= 6 && p[1] == 6) {
				magic = (u_long)p[2] << 24 |
					(u_long)p[3] << 16 | p[4] << 8 | p[5];
				/*
				 * If the remote magic is our negated one,
				 * this looks like a loopback problem.
				 * Suggest a new magic to make sure.
				 */
				if (magic == ~sp->lcp.magic) {
					if (debug)
						addlog(" magic glitch");
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
					sp->lcp.magic = random();
#else
					sp->lcp.magic = time.tv_sec + time.tv_usec;
#endif
				} else {
					sp->lcp.magic = magic;
					if (debug)
						addlog(" %ld", magic);
				}
			}
			break;
		case LCP_OPT_MRU:
			/*
			 * Peer wants to advise us to negotiate an MRU.
			 * Agree on it if it's reasonable, or use
			 * default otherwise.
			 */
			if (len >= 4 && p[1] == 4) {
				u_int mru = p[2] * 256 + p[3];
				if (debug)
					addlog(" %d", mru);
				if (mru < PP_MTU || mru > PP_MAX_MRU)
					mru = PP_MTU;
				sp->lcp.mru = mru;
				sp->lcp.opts |= (1 << LCP_OPT_MRU);
			}
			break;
		case LCP_OPT_AUTH_PROTO:
			/*
			 * Peer doesn't like our authentication method,
			 * deny.
			 */
			if (debug)
				addlog("[access denied]\n");
			lcp.Close(sp);
			break;
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

static void
sppp_lcp_tlu(struct sppp *sp)
{
	STDDCL;
	int i;
	u_long mask;

	/* XXX ? */
	if (! (ifp->if_flags & IFF_UP) &&
	    (ifp->if_flags & IFF_RUNNING)) {
		/* Coming out of loopback mode. */
		if_up(ifp);
		printf (SPP_FMT "up\n", SPP_ARGS(ifp));
	}

	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_QUAL)
			(cps[i])->Open(sp);

	if ((sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0 ||
	    (sp->pp_flags & PP_NEEDAUTH) != 0)
		sp->pp_phase = PHASE_AUTHENTICATE;
	else
		sp->pp_phase = PHASE_NETWORK;

	if(debug)
	{
		log(LOG_INFO, SPP_FMT "phase %s\n", SPP_ARGS(ifp),
		    sppp_phase_name(sp->pp_phase));
	}

	/*
	 * Open all authentication protocols.  This is even required
	 * if we already proceeded to network phase, since it might be
	 * that remote wants us to authenticate, so we might have to
	 * send a PAP request.  Undesired authentication protocols
	 * don't do anything when they get an Open event.
	 */
	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_AUTH)
			(cps[i])->Open(sp);

	if (sp->pp_phase == PHASE_NETWORK) {
		/* Notify all NCPs. */
		for (i = 0; i < IDX_COUNT; i++)
			if ((cps[i])->flags & CP_NCP)
				(cps[i])->Open(sp);
	}

	/* Send Up events to all started protos. */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_LCP) == 0)
			(cps[i])->Up(sp);

	/* notify low-level driver of state change */
	if (sp->pp_chg)
		sp->pp_chg(sp, (int)sp->pp_phase);
	
	if (sp->pp_phase == PHASE_NETWORK)
		/* if no NCP is starting, close down */
		sppp_lcp_check_and_close(sp);
}

static void
sppp_lcp_tld(struct sppp *sp)
{
	STDDCL;
	int i;
	u_long mask;

	sp->pp_phase = PHASE_TERMINATE;

	if(debug)
	{
		log(LOG_INFO, SPP_FMT "phase %s\n", SPP_ARGS(ifp),
			sppp_phase_name(sp->pp_phase));
	}

	/*
	 * Take upper layers down.  We send the Down event first and
	 * the Close second to prevent the upper layers from sending
	 * ``a flurry of terminate-request packets'', as the RFC
	 * describes it.
	 */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_LCP) == 0) {
			(cps[i])->Down(sp);
			(cps[i])->Close(sp);
		}
}

static void
sppp_lcp_tls(struct sppp *sp)
{
	STDDCL;

	sp->pp_phase = PHASE_ESTABLISH;

	if(debug)
	{
		log(LOG_INFO, SPP_FMT "phase %s\n", SPP_ARGS(ifp),
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify lower layer if desired. */
	if (sp->pp_tls)
		(sp->pp_tls)(sp);
}

static void
sppp_lcp_tlf(struct sppp *sp)
{
	STDDCL;

	sp->pp_phase = PHASE_DEAD;

	if(debug)
	{
		log(LOG_INFO, SPP_FMT "phase %s\n", SPP_ARGS(ifp),
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify lower layer if desired. */
	if (sp->pp_tlf)
		(sp->pp_tlf)(sp);
}

static void
sppp_lcp_scr(struct sppp *sp)
{
	char opt[6 /* magicnum */ + 4 /* mru */ + 5 /* chap */];
	int i = 0;
	u_short authproto;

	if (sp->lcp.opts & (1 << LCP_OPT_MAGIC)) {
		if (! sp->lcp.magic)
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
			sp->lcp.magic = random();
#else
			sp->lcp.magic = time.tv_sec + time.tv_usec;
#endif
		opt[i++] = LCP_OPT_MAGIC;
		opt[i++] = 6;
		opt[i++] = sp->lcp.magic >> 24;
		opt[i++] = sp->lcp.magic >> 16;
		opt[i++] = sp->lcp.magic >> 8;
		opt[i++] = sp->lcp.magic;
	}

	if (sp->lcp.opts & (1 << LCP_OPT_MRU)) {
		opt[i++] = LCP_OPT_MRU;
		opt[i++] = 4;
		opt[i++] = sp->lcp.mru >> 8;
		opt[i++] = sp->lcp.mru;
	}

	if (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) {
		authproto = sp->hisauth.proto;
		opt[i++] = LCP_OPT_AUTH_PROTO;
		opt[i++] = authproto == PPP_CHAP? 5: 4;
		opt[i++] = authproto >> 8;
		opt[i++] = authproto;
		if (authproto == PPP_CHAP)
			opt[i++] = CHAP_MD5;
	}

	sp->confid[IDX_LCP] = ++sp->pp_seq[IDX_LCP];
	sppp_cp_send (sp, PPP_LCP, CONF_REQ, sp->confid[IDX_LCP], i, &opt);
}

/*
 * Check the open NCPs, return true if at least one NCP is open.
 */
static int
sppp_ncp_check(struct sppp *sp)
{
	int i, mask;

	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && (cps[i])->flags & CP_NCP)
			return 1;
	return 0;
}

/*
 * Re-check the open NCPs and see if we should terminate the link.
 * Called by the NCPs during their tlf action handling.
 */
static void
sppp_lcp_check_and_close(struct sppp *sp)
{

	if (sp->pp_phase < PHASE_NETWORK)
		/* don't bother, we are already going down */
		return;

	if (sppp_ncp_check(sp))
		return;

	lcp.Close(sp);
}


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The IPCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

static void
sppp_ipcp_init(struct sppp *sp)
{
	sp->ipcp.opts = 0;
	sp->ipcp.flags = 0;
	sp->state[IDX_IPCP] = STATE_INITIAL;
	sp->fail_counter[IDX_IPCP] = 0;
	sp->pp_seq[IDX_IPCP] = 0;
	sp->pp_rseq[IDX_IPCP] = 0;
#if defined(__NetBSD__)
	callout_init(&sp->ch[IDX_IPCP]);
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sp->ch[IDX_IPCP]);
#endif
}

static void
sppp_ipcp_up(struct sppp *sp)
{
	sppp_up_event(&ipcp, sp);
}

static void
sppp_ipcp_down(struct sppp *sp)
{
	sppp_down_event(&ipcp, sp);
}

static void
sppp_ipcp_open(struct sppp *sp)
{
	STDDCL;
	u_long myaddr, hisaddr;

	sp->ipcp.flags &= ~(IPCP_HISADDR_SEEN|IPCP_MYADDR_SEEN|IPCP_MYADDR_DYN);

	sppp_get_ip_addrs(sp, &myaddr, &hisaddr, 0);
	/*
	 * If we don't have his address, this probably means our
	 * interface doesn't want to talk IP at all.  (This could
	 * be the case if somebody wants to speak only IPX, for
	 * example.)  Don't open IPCP in this case.
	 */
	if (hisaddr == 0L) {
		/* XXX this message should go away */
		if (debug)
			log(LOG_DEBUG, SPP_FMT "ipcp_open(): no IP interface\n",
			    SPP_ARGS(ifp));
		return;
	}

	if (myaddr == 0L) {
		/*
		 * I don't have an assigned address, so i need to
		 * negotiate my address.
		 */
		sp->ipcp.flags |= IPCP_MYADDR_DYN;
		sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
	} else
		sp->ipcp.flags |= IPCP_MYADDR_SEEN;
	sppp_open_event(&ipcp, sp);
}

static void
sppp_ipcp_close(struct sppp *sp)
{
	sppp_close_event(&ipcp, sp);
	if (sp->ipcp.flags & IPCP_MYADDR_DYN)
		/*
		 * My address was dynamic, clear it again.
		 */
		sppp_set_ip_addr(sp, 0L);
}

static void
sppp_ipcp_TO(void *cookie)
{
	sppp_to_event(&ipcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *r, *p;
	struct ifnet *ifp = &sp->pp_if;
	int rlen, origlen, debug = ifp->if_flags & IFF_DEBUG;
	u_long hisaddr, desiredaddr;
	int gotmyaddr = 0;

	len -= 4;
	origlen = len;
	/*
	 * Make sure to allocate a buf that can at least hold a
	 * conf-nak with an `address' option.  We might need it below.
	 */
	buf = r = malloc ((len < 6? 6: len), M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	/* pass 1: see if we can recognize them */
	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipcp parse opts:",
		    SPP_ARGS(ifp));
	p = (void*) (h+1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			if (len >= 6 && p[1] >= 6) {
				/* correctly formed compress option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#endif
		case IPCP_OPT_ADDRESS:
			if (len >= 6 && p[1] == 6) {
				/* correctly formed address option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/* pass 2: parse option values */
	sppp_get_ip_addrs(sp, 0, &hisaddr, 0);
	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipcp parse opt values: ",
		       SPP_ARGS(ifp));
	p = (void*) (h+1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			continue;
#endif
		case IPCP_OPT_ADDRESS:
			desiredaddr = p[2] << 24 | p[3] << 16 |
				p[4] << 8 | p[5];
			if (!(sp->ipcp.flags & IPCP_MYADDR_SEEN) &&
			        (sp->ipcp.flags & IPCP_MYADDR_DYN)) {
				/*
				 * hopefully this is our address !!
				 */
			 	if (debug)
					addlog(" [wantmyaddr %s]",
						sppp_dotted_quad(desiredaddr));
				/*
				 * When doing dynamic address assignment,
			   	 * we accept his offer.  Otherwise, we
			    	 * ignore it and thus continue to negotiate
			     	 * our already existing value.
		      		 */
				sppp_set_ip_addr(sp, desiredaddr);
				if (debug)
					addlog(" [agree]");
				sp->ipcp.flags |= IPCP_MYADDR_SEEN;
				gotmyaddr++;
				continue;
			} else {
				if (desiredaddr == hisaddr ||
			    	(hisaddr == 1 && desiredaddr != 0)) {
					/*
				 	* Peer's address is same as our value,
				 	* this is agreeable.  Gonna conf-ack
				 	* it.
				 	*/
					if (debug)
						addlog(" %s [ack]",
					       		sppp_dotted_quad(hisaddr));
					/* record that we've seen it already */
					sp->ipcp.flags |= IPCP_HISADDR_SEEN;
					continue;
				}
				/*
			 	* The address wasn't agreeable.  This is either
			 	* he sent us 0.0.0.0, asking to assign him an
			 	* address, or he send us another address not
			 	* matching our value.  Either case, we gonna
			 	* conf-nak it with our value.
			 	*/
				if (debug) {
					if (desiredaddr == 0)
						addlog(" [addr requested]");
					else
						addlog(" %s [not agreed]",
					       		sppp_dotted_quad(desiredaddr));
				}

				p[2] = hisaddr >> 24;
				p[3] = hisaddr >> 16;
				p[4] = hisaddr >> 8;
				p[5] = hisaddr;
				break;
			}
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}

	/*
	 * If we are about to conf-ack the request, but haven't seen
	 * his address so far, gonna conf-nak it instead, with the
	 * `address' option present and our idea of his address being
	 * filled in there, to request negotiation of both addresses.
	 *
	 * XXX This can result in an endless req - nak loop if peer
	 * doesn't want to send us his address.  Q: What should we do
	 * about it?  XXX  A: implement the max-failure counter.
	 */
	if (rlen == 0 && !(sp->ipcp.flags & IPCP_HISADDR_SEEN) && !gotmyaddr) {
		buf[0] = IPCP_OPT_ADDRESS;
		buf[1] = 6;
		buf[2] = hisaddr >> 24;
		buf[3] = hisaddr >> 16;
		buf[4] = hisaddr >> 8;
		buf[5] = hisaddr;
		rlen = 6;
		if (debug)
			addlog(" still need hisaddr");
	}

	if (rlen) {
		if (debug)
			addlog(" send conf-nak\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_NAK, h->ident, rlen, buf);
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_ACK,
			      h->ident, origlen, h+1);
	}

 end:
	free (buf, M_TEMP);
	return (rlen == 0);
}

/*
 * Analyze the IPCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipcp rej opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't grok address option.  This is
			 * bad.  XXX  Should we better give up here?
			 */
			sp->ipcp.opts &= ~(1 << IPCP_OPT_ADDRESS);
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			sp->ipcp.opts &= ~(1 << IPCP_OPT_COMPRESS);
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

/*
 * Analyze the IPCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;
	u_long wantaddr;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipcp nak opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't like our local IP address.  See
			 * if we can do something for him.  We'll drop
			 * him our address then.
			 */
			if (len >= 6 && p[1] == 6) {
				wantaddr = p[2] << 24 | p[3] << 16 |
					p[4] << 8 | p[5];
				sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
				if (debug)
					addlog(" [wantaddr %s]",
					       sppp_dotted_quad(wantaddr));
				/*
				 * When doing dynamic address assignment,
				 * we accept his offer.  Otherwise, we
				 * ignore it and thus continue to negotiate
				 * our already existing value.
				 */
				if (sp->ipcp.flags & IPCP_MYADDR_DYN) {
					sppp_set_ip_addr(sp, wantaddr);
					if (debug)
						addlog(" [agree]");
					sp->ipcp.flags |= IPCP_MYADDR_SEEN;
				}
			}
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			/*
			 * Peer wants different compression parameters.
			 */
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

static void
sppp_ipcp_tlu(struct sppp *sp)
{
	/* we are up - notify isdn daemon */
	if (sp->pp_con)
		sp->pp_con(sp);
}

static void
sppp_ipcp_tld(struct sppp *sp)
{
}

static void
sppp_ipcp_tls(struct sppp *sp)
{
	/* indicate to LCP that it must stay alive */
	sp->lcp.protos |= (1 << IDX_IPCP);
}

static void
sppp_ipcp_tlf(struct sppp *sp)
{

#if 0 /* need #if 0 to close IPCP properly */
	/* we no longer need LCP */
	sp->lcp.protos &= ~(1 << IDX_IPCP);
	sppp_lcp_check_and_close(sp);
#endif
}

static void
sppp_ipcp_scr(struct sppp *sp)
{
	char opt[6 /* compression */ + 6 /* address */];
	u_long ouraddr;
	int i = 0;

#ifdef notyet
	if (sp->ipcp.opts & (1 << IPCP_OPT_COMPRESSION)) {
		opt[i++] = IPCP_OPT_COMPRESSION;
		opt[i++] = 6;
		opt[i++] = 0;	/* VJ header compression */
		opt[i++] = 0x2d; /* VJ header compression */
		opt[i++] = max_slot_id;
		opt[i++] = comp_slot_id;
	}
#endif

	if (sp->ipcp.opts & (1 << IPCP_OPT_ADDRESS)) {
		sppp_get_ip_addrs(sp, &ouraddr, 0, 0);
		opt[i++] = IPCP_OPT_ADDRESS;
		opt[i++] = 6;
		opt[i++] = ouraddr >> 24;
		opt[i++] = ouraddr >> 16;
		opt[i++] = ouraddr >> 8;
		opt[i++] = ouraddr;
	}

	sp->confid[IDX_IPCP] = ++sp->pp_seq[IDX_IPCP];
	sppp_cp_send(sp, PPP_IPCP, CONF_REQ, sp->confid[IDX_IPCP], i, &opt);
}


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                      The IPv6CP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

#ifdef INET6
static void
sppp_ipv6cp_init(struct sppp *sp)
{
	sp->ipv6cp.opts = 0;
	sp->ipv6cp.flags = 0;
	sp->state[IDX_IPV6CP] = STATE_INITIAL;
	sp->fail_counter[IDX_IPV6CP] = 0;
	sp->pp_seq[IDX_IPV6CP] = 0;
	sp->pp_rseq[IDX_IPV6CP] = 0;
#if defined(__NetBSD__)
	callout_init(&sp->ch[IDX_IPV6CP]);
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sp->ch[IDX_IPV6CP]);
#endif
}

static void
sppp_ipv6cp_up(struct sppp *sp)
{
	sppp_up_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_down(struct sppp *sp)
{
	sppp_down_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_open(struct sppp *sp)
{
	STDDCL;
	struct in6_addr myaddr, hisaddr;

#ifdef IPV6CP_MYIFID_DYN
	sp->ipv6cp.flags &= ~(IPV6CP_MYIFID_SEEN|IPV6CP_MYIFID_DYN);
#else
	sp->ipv6cp.flags &= ~IPV6CP_MYIFID_SEEN;
#endif

	sppp_get_ip6_addrs(sp, &myaddr, &hisaddr, 0);
	/*
	 * If we don't have our address, this probably means our
	 * interface doesn't want to talk IPv6 at all.  (This could
	 * be the case if somebody wants to speak only IPX, for
	 * example.)  Don't open IPv6CP in this case.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&myaddr)) {
		/* XXX this message should go away */
		if (debug)
			log(LOG_DEBUG, SPP_FMT "ipv6cp_open(): no IPv6 interface\n",
			    SPP_ARGS(ifp));
		return;
	}

	sp->ipv6cp.flags |= IPV6CP_MYIFID_SEEN;
	sp->ipv6cp.opts |= (1 << IPV6CP_OPT_IFID);
	sppp_open_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_close(struct sppp *sp)
{
	sppp_close_event(&ipv6cp, sp);
}

static void
sppp_ipv6cp_TO(void *cookie)
{
	sppp_to_event(&ipv6cp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *r, *p;
	struct ifnet *ifp = &sp->pp_if;
	int rlen, origlen, debug = ifp->if_flags & IFF_DEBUG;
	struct in6_addr myaddr, desiredaddr, suggestaddr;
	int ifidcount;
	int type;
	int collision, nohisaddr;

	len -= 4;
	origlen = len;
	/*
	 * Make sure to allocate a buf that can at least hold a
	 * conf-nak with an `address' option.  We might need it below.
	 */
	buf = r = malloc ((len < 6? 6: len), M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	/* pass 1: see if we can recognize them */
	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipv6cp parse opts:",
		    SPP_ARGS(ifp));
	p = (void*) (h+1);
	ifidcount = 0;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			if (len >= 10 && p[1] == 10 && ifidcount == 0) {
				/* correctly formed address option */
				ifidcount++;
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESSION:
			if (len >= 4 && p[1] >= 4) {
				/* correctly formed compress option */
				continue;
			}
			if (debug)
				addlog(" [invalid]");
			break;
#endif
		default:
			/* Others not supported. */
			if (debug)
				addlog(" [rej]");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send (sp, PPP_IPV6CP, CONF_REJ, h->ident, rlen, buf);
		goto end;
	} else if (debug)
		addlog("\n");

	/* pass 2: parse option values */
	sppp_get_ip6_addrs(sp, &myaddr, 0, 0);
	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipv6cp parse opt values: ",
		       SPP_ARGS(ifp));
	p = (void*) (h+1);
	len = origlen;
	type = CONF_ACK;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPV6CP_OPT_COMPRESSION:
			continue;
#endif
		case IPV6CP_OPT_IFID:
			bzero(&desiredaddr, sizeof(desiredaddr));
			bcopy(&p[2], &desiredaddr.s6_addr[8], 8);
			collision = (bcmp(&desiredaddr.s6_addr[8],
					&myaddr.s6_addr[8], 8) == 0);
			nohisaddr = IN6_IS_ADDR_UNSPECIFIED(&desiredaddr);

			desiredaddr.s6_addr16[0] = htons(0xfe80);
			desiredaddr.s6_addr16[1] = htons(sp->pp_if.if_index);

			if (!collision && !nohisaddr) {
				/* no collision, hisaddr known - Conf-Ack */
				type = CONF_ACK;

				if (debug) {
					addlog(" %s [%s]",
					    ip6_sprintf(&desiredaddr),
					    sppp_cp_type_name(type));
				}
				continue;
			}

			bzero(&suggestaddr, sizeof(&suggestaddr));
			if (collision && nohisaddr) {
				/* collision, hisaddr unknown - Conf-Rej */
				type = CONF_REJ;
				bzero(&p[2], 8);
			} else {
				/*
				 * - no collision, hisaddr unknown, or
				 * - collision, hisaddr known
				 * Conf-Nak, suggest hisaddr
				 */
				type = CONF_NAK;
				sppp_suggest_ip6_addr(sp, &suggestaddr);
				bcopy(&suggestaddr.s6_addr[8], &p[2], 8);
			}
			if (debug)
				addlog(" %s [%s]", ip6_sprintf(&desiredaddr),
				    sppp_cp_type_name(type));
			break;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}

	if (rlen == 0 && type == CONF_ACK) {
		if (debug)
			addlog(" send %s\n", sppp_cp_type_name(type));
		sppp_cp_send (sp, PPP_IPV6CP, type, h->ident, origlen, h+1);
	} else {
#ifdef DIAGNOSTIC
		if (type == CONF_ACK)
			panic("IPv6CP RCR: CONF_ACK with non-zero rlen");
#endif

		if (debug) {
			addlog(" send %s suggest %s\n",
			    sppp_cp_type_name(type), ip6_sprintf(&suggestaddr));
		}
		sppp_cp_send (sp, PPP_IPV6CP, type, h->ident, rlen, buf);
	}

 end:
	free (buf, M_TEMP);
	return (rlen == 0);
}

/*
 * Analyze the IPv6CP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipv6cp rej opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			/*
			 * Peer doesn't grok address option.  This is
			 * bad.  XXX  Should we better give up here?
			 */
			sp->ipv6cp.opts &= ~(1 << IPV6CP_OPT_IFID);
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESS:
			sp->ipv6cp.opts &= ~(1 << IPV6CP_OPT_COMPRESS);
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

/*
 * Analyze the IPv6CP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;
	struct in6_addr suggestaddr;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "ipv6cp nak opts:",
		    SPP_ARGS(ifp));

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s", sppp_ipv6cp_opt_name(*p));
		switch (*p) {
		case IPV6CP_OPT_IFID:
			/*
			 * Peer doesn't like our local ifid.  See
			 * if we can do something for him.  We'll drop
			 * him our address then.
			 */
			if (len < 10 || p[1] != 10)
				break;
			bzero(&suggestaddr, sizeof(suggestaddr));
			suggestaddr.s6_addr16[0] = htons(0xfe80);
			suggestaddr.s6_addr16[1] = htons(sp->pp_if.if_index);
			bcopy(&p[2], &suggestaddr.s6_addr[8], 8);

			sp->ipv6cp.opts |= (1 << IPV6CP_OPT_IFID);
			if (debug)
				addlog(" [suggestaddr %s]",
				       ip6_sprintf(&suggestaddr));
#ifdef IPV6CP_MYIFID_DYN
			/*
			 * When doing dynamic address assignment,
			 * we accept his offer.
			 */
			if (sp->ipv6cp.flags & IPV6CP_MYIFID_DYN) {
				struct in6_addr lastsuggest;
				/*
				 * If <suggested myaddr from peer> equals to
				 * <hisaddr we have suggested last time>,
				 * we have a collision.  generate new random
				 * ifid.
				 */
				sppp_suggest_ip6_addr(&lastsuggest);
				if (IN6_ARE_ADDR_EQUAL(&suggestaddr,
						 lastsuggest)) {
					if (debug)
						addlog(" [random]");
					sppp_gen_ip6_addr(sp, &suggestaddr);
				}
				sppp_set_ip6_addr(sp, &suggestaddr, 0);
				if (debug)
					addlog(" [agree]");
				sp->ipv6cp.flags |= IPV6CP_MYIFID_SEEN;
			}
#else
			/*
			 * Since we do not do dynamic address assignment,
			 * we ignore it and thus continue to negotiate
			 * our already existing value.  This can possibly
			 * go into infinite request-reject loop.
			 *
			 * This is not likely because we normally use
			 * ifid based on MAC-address.
			 * If you have no ethernet card on the node, too bad.
			 * XXX should we use fail_counter?
			 */
#endif
			break;
#ifdef notyet
		case IPV6CP_OPT_COMPRESS:
			/*
			 * Peer wants different compression parameters.
			 */
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

static void
sppp_ipv6cp_tlu(struct sppp *sp)
{
	/* we are up - notify isdn daemon */
	if (sp->pp_con)
		sp->pp_con(sp);
}

static void
sppp_ipv6cp_tld(struct sppp *sp)
{
}

static void
sppp_ipv6cp_tls(struct sppp *sp)
{
	/* indicate to LCP that it must stay alive */
	sp->lcp.protos |= (1 << IDX_IPV6CP);
}

static void
sppp_ipv6cp_tlf(struct sppp *sp)
{

#if 0 /* need #if 0 to close IPv6CP properly */
	/* we no longer need LCP */
	sp->lcp.protos &= ~(1 << IDX_IPV6CP);
	sppp_lcp_check_and_close(sp);
#endif
}

static void
sppp_ipv6cp_scr(struct sppp *sp)
{
	char opt[10 /* ifid */ + 4 /* compression, minimum */];
	struct in6_addr ouraddr;
	int i = 0;

	if (sp->ipv6cp.opts & (1 << IPV6CP_OPT_IFID)) {
		sppp_get_ip6_addrs(sp, &ouraddr, 0, 0);
		opt[i++] = IPV6CP_OPT_IFID;
		opt[i++] = 10;
		bcopy(&ouraddr.s6_addr[8], &opt[i], 8);
		i += 8;
	}

#ifdef notyet
	if (sp->ipv6cp.opts & (1 << IPV6CP_OPT_COMPRESSION)) {
		opt[i++] = IPV6CP_OPT_COMPRESSION;
		opt[i++] = 4;
		opt[i++] = 0;	/* TBD */
		opt[i++] = 0;	/* TBD */
		/* variable length data may follow */
	}
#endif

	sp->confid[IDX_IPV6CP] = ++sp->pp_seq[IDX_IPV6CP];
	sppp_cp_send(sp, PPP_IPV6CP, CONF_REQ, sp->confid[IDX_IPV6CP], i, &opt);
}
#else /*INET6*/
static void sppp_ipv6cp_init(struct sppp *sp)
{
}

static void sppp_ipv6cp_up(struct sppp *sp)
{
}

static void sppp_ipv6cp_down(struct sppp *sp)
{
}


static void sppp_ipv6cp_open(struct sppp *sp)
{
}

static void sppp_ipv6cp_close(struct sppp *sp)
{
}

static void sppp_ipv6cp_TO(void *sp)
{
}

static int sppp_ipv6cp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	return 0;
}

static void sppp_ipv6cp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
}

static void sppp_ipv6cp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
}

static void sppp_ipv6cp_tlu(struct sppp *sp)
{
}

static void sppp_ipv6cp_tld(struct sppp *sp)
{
}

static void sppp_ipv6cp_tls(struct sppp *sp)
{
}

static void sppp_ipv6cp_tlf(struct sppp *sp)
{
}

static void sppp_ipv6cp_scr(struct sppp *sp)
{
}
#endif /*INET6*/


/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The CHAP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

/*
 * The authentication protocols don't employ a full-fledged state machine as
 * the control protocols do, since they do have Open and Close events, but
 * not Up and Down, nor are they explicitly terminated.  Also, use of the
 * authentication protocols may be different in both directions (this makes
 * sense, think of a machine that never accepts incoming calls but only
 * calls out, it doesn't require the called party to authenticate itself).
 *
 * Our state machine for the local authentication protocol (we are requesting
 * the peer to authenticate) looks like:
 *
 *						    RCA-
 *	      +--------------------------------------------+
 *	      V					    scn,tld|
 *	  +--------+			       Close   +---------+ RCA+
 *	  |	   |<----------------------------------|	 |------+
 *   +--->| Closed |				TO*    | Opened	 | sca	|
 *   |	  |	   |-----+		       +-------|	 |<-----+
 *   |	  +--------+ irc |		       |       +---------+
 *   |	    ^		 |		       |	   ^
 *   |	    |		 |		       |	   |
 *   |	    |		 |		       |	   |
 *   |	 TO-|		 |		       |	   |
 *   |	    |tld  TO+	 V		       |	   |
 *   |	    |	+------->+		       |	   |
 *   |	    |	|	 |		       |	   |
 *   |	  +--------+	 V		       |	   |
 *   |	  |	   |<----+<--------------------+	   |
 *   |	  | Req-   | scr				   |
 *   |	  | Sent   |					   |
 *   |	  |	   |					   |
 *   |	  +--------+					   |
 *   | RCA- |	| RCA+					   |
 *   +------+	+------------------------------------------+
 *   scn,tld	  sca,irc,ict,tlu
 *
 *
 *   with:
 *
 *	Open:	LCP reached authentication phase
 *	Close:	LCP reached terminate phase
 *
 *	RCA+:	received reply (pap-req, chap-response), acceptable
 *	RCN:	received reply (pap-req, chap-response), not acceptable
 *	TO+:	timeout with restart counter >= 0
 *	TO-:	timeout with restart counter < 0
 *	TO*:	reschedule timeout for CHAP
 *
 *	scr:	send request packet (none for PAP, chap-challenge)
 *	sca:	send ack packet (pap-ack, chap-success)
 *	scn:	send nak packet (pap-nak, chap-failure)
 *	ict:	initialize re-challenge timer (CHAP only)
 *
 *	tlu:	this-layer-up, LCP reaches network phase
 *	tld:	this-layer-down, LCP enters terminate phase
 *
 * Note that in CHAP mode, after sending a new challenge, while the state
 * automaton falls back into Req-Sent state, it doesn't signal a tld
 * event to LCP, so LCP remains in network phase.  Only after not getting
 * any response (or after getting an unacceptable response), CHAP closes,
 * causing LCP to enter terminate phase.
 *
 * With PAP, there is no initial request that can be sent.  The peer is
 * expected to send one based on the successful negotiation of PAP as
 * the authentication protocol during the LCP option negotiation.
 *
 * Incoming authentication protocol requests (remote requests
 * authentication, we are peer) don't employ a state machine at all,
 * they are simply answered.  Some peers [Ascend P50 firmware rev
 * 4.50] react allergically when sending IPCP/IPv6CP requests while they are
 * still in authentication phase (thereby violating the standard that
 * demands that these NCP packets are to be discarded), so we keep
 * track of the peer demanding us to authenticate, and only proceed to
 * phase network once we've seen a positive acknowledge for the
 * authentication.
 */

/*
 * Handle incoming CHAP packets.
 */
void
sppp_chap_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len, x;
	u_char *value, *name, digest[AUTHKEYLEN], dsize;
	int value_len, name_len;
	MD5_CTX ctx;

	len = m->m_pkthdr.len;
	if (len < 4) {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "chap invalid packet length: %d bytes\n",
			    SPP_ARGS(ifp), len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (len > ntohs (h->len))
		len = ntohs (h->len);

	switch (h->type) {
	/* challenge, failure and success are his authproto */
	case CHAP_CHALLENGE:
		value = 1 + (u_char*)(h+1);
		value_len = value[-1];
		name = value + value_len;
		name_len = len - value_len - 5;
		if (name_len < 0) {
			if (debug) {
				log(LOG_DEBUG,
				    SPP_FMT "chap corrupted challenge "
				    "<%s id=0x%x len=%d",
				    SPP_ARGS(ifp),
				    sppp_auth_type_name(PPP_CHAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char*) (h+1), len-4);
				addlog(">\n");
			}
			break;
		}
		
		if (debug) {
			log(LOG_DEBUG,
			    SPP_FMT "chap input <%s id=0x%x len=%d name=",
			    SPP_ARGS(ifp),
			    sppp_auth_type_name(PPP_CHAP, h->type), h->ident,
			    ntohs(h->len));
			sppp_print_string((char*) name, name_len);
			addlog(" value-size=%d value=", value_len);
			sppp_print_bytes(value, value_len);
			addlog(">\n");
		}

		/* Compute reply value. */
		MD5Init(&ctx);
		MD5Update(&ctx, &h->ident, 1);
		MD5Update(&ctx, sp->myauth.secret,
			  sppp_strnlen(sp->myauth.secret, AUTHKEYLEN));
		MD5Update(&ctx, value, value_len);
		MD5Final(digest, &ctx);
		dsize = sizeof digest;

		sppp_auth_send(&chap, sp, CHAP_RESPONSE, h->ident,
			       sizeof dsize, (const char *)&dsize,
			       sizeof digest, digest,
			       (size_t)sppp_strnlen(sp->myauth.name, AUTHNAMELEN),
			       sp->myauth.name,
			       0);
		break;

	case CHAP_SUCCESS:
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "chap success",
			    SPP_ARGS(ifp));
			if (len > 4) {
				addlog(": ");
				sppp_print_string((char*)(h + 1), len - 4);
			}
			addlog("\n");
		}
		x = splimp();
		sp->pp_flags &= ~PP_NEEDAUTH;
		if (sp->myauth.proto == PPP_CHAP &&
		    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) &&
		    (sp->lcp.protos & (1 << IDX_CHAP)) == 0) {
			/*
			 * We are authenticator for CHAP but didn't
			 * complete yet.  Leave it to tlu to proceed
			 * to network phase.
			 */
			splx(x);
			break;
		}
		splx(x);
		sppp_phase_network(sp);
		break;

	case CHAP_FAILURE:
		if (debug) {
			log(LOG_INFO, SPP_FMT "chap failure",
			    SPP_ARGS(ifp));
			if (len > 4) {
				addlog(": ");
				sppp_print_string((char*)(h + 1), len - 4);
			}
			addlog("\n");
		} else
			log(LOG_INFO, SPP_FMT "chap failure\n",
			    SPP_ARGS(ifp));
		/* await LCP shutdown by authenticator */
		break;

	/* response is my authproto */
	case CHAP_RESPONSE:
		value = 1 + (u_char*)(h+1);
		value_len = value[-1];
		name = value + value_len;
		name_len = len - value_len - 5;
		if (name_len < 0) {
			if (debug) {
				log(LOG_DEBUG,
				    SPP_FMT "chap corrupted response "
				    "<%s id=0x%x len=%d",
				    SPP_ARGS(ifp),
				    sppp_auth_type_name(PPP_CHAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char*)(h+1), len-4);
				addlog(">\n");
			}
			break;
		}
		if (h->ident != sp->confid[IDX_CHAP]) {
			if (debug)
				log(LOG_DEBUG,
				    SPP_FMT "chap dropping response for old ID "
				    "(got %d, expected %d)\n",
				    SPP_ARGS(ifp),
				    h->ident, sp->confid[IDX_CHAP]);
			break;
		}
		if (name_len != sppp_strnlen(sp->hisauth.name, AUTHNAMELEN)
		    || bcmp(name, sp->hisauth.name, name_len) != 0) {
			log(LOG_INFO, SPP_FMT "chap response, his name ",
			    SPP_ARGS(ifp));
			sppp_print_string(name, name_len);
			addlog(" != expected ");
			sppp_print_string(sp->hisauth.name,
					  sppp_strnlen(sp->hisauth.name, AUTHNAMELEN));
			addlog("\n");
		}    
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "chap input(%s) "
			    "<%s id=0x%x len=%d name=",
			    SPP_ARGS(ifp),
			    sppp_state_name(sp->state[IDX_CHAP]),
			    sppp_auth_type_name(PPP_CHAP, h->type),
			    h->ident, ntohs (h->len));
			sppp_print_string((char*)name, name_len);
			addlog(" value-size=%d value=", value_len);
			sppp_print_bytes(value, value_len);
			addlog(">\n");
		}
		if (value_len != AUTHKEYLEN) {
			if (debug)
				log(LOG_DEBUG,
				    SPP_FMT "chap bad hash value length: "
				    "%d bytes, should be %d\n",
				    SPP_ARGS(ifp), value_len,
				    AUTHKEYLEN);
			break;
		}

		MD5Init(&ctx);
		MD5Update(&ctx, &h->ident, 1);
		MD5Update(&ctx, sp->hisauth.secret,
			  sppp_strnlen(sp->hisauth.secret, AUTHKEYLEN));
		MD5Update(&ctx, sp->myauth.challenge, AUTHKEYLEN);
		MD5Final(digest, &ctx);

#define FAILMSG "Failed..."
#define SUCCMSG "Welcome!"

		if (value_len != sizeof digest ||
		    bcmp(digest, value, value_len) != 0) {
			/* action scn, tld */
			sppp_auth_send(&chap, sp, CHAP_FAILURE, h->ident,
				       sizeof(FAILMSG) - 1, (u_char *)FAILMSG,
				       0);
			chap.tld(sp);
			break;
		}
		/* action sca, perhaps tlu */
		if (sp->state[IDX_CHAP] == STATE_REQ_SENT ||
		    sp->state[IDX_CHAP] == STATE_OPENED)
			sppp_auth_send(&chap, sp, CHAP_SUCCESS, h->ident,
				       sizeof(SUCCMSG) - 1, (u_char *)SUCCMSG,
				       0);
		if (sp->state[IDX_CHAP] == STATE_REQ_SENT) {
			sppp_cp_change_state(&chap, sp, STATE_OPENED);
			chap.tlu(sp);
		}
		break;

	default:
		/* Unknown CHAP packet type -- ignore. */
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "chap unknown input(%s) "
			    "<0x%x id=0x%xh len=%d",
			    SPP_ARGS(ifp),
			    sppp_state_name(sp->state[IDX_CHAP]),
			    h->type, h->ident, ntohs(h->len));
			if (len > 4)
				sppp_print_bytes((u_char*)(h+1), len-4);
			addlog(">\n");
		}
		break;

	}
}

static void
sppp_chap_init(struct sppp *sp)
{
	/* Chap doesn't have STATE_INITIAL at all. */
	sp->state[IDX_CHAP] = STATE_CLOSED;
	sp->fail_counter[IDX_CHAP] = 0;
	sp->pp_seq[IDX_CHAP] = 0;
	sp->pp_rseq[IDX_CHAP] = 0;
#if defined(__NetBSD__)
	callout_init(&sp->ch[IDX_CHAP]);
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sp->ch[IDX_CHAP]);
#endif
}

static void
sppp_chap_open(struct sppp *sp)
{
	if (sp->myauth.proto == PPP_CHAP &&
	    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0) {
		/* we are authenticator for CHAP, start it */
		chap.scr(sp);
		sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;
		sppp_cp_change_state(&chap, sp, STATE_REQ_SENT);
	}
	/* nothing to be done if we are peer, await a challenge */
}

static void
sppp_chap_close(struct sppp *sp)
{
	if (sp->state[IDX_CHAP] != STATE_CLOSED)
		sppp_cp_change_state(&chap, sp, STATE_CLOSED);
}

static void
sppp_chap_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;
	int s;

	s = splimp();
	if (debug)
		log(LOG_DEBUG, SPP_FMT "chap TO(%s) rst_counter = %d\n",
		    SPP_ARGS(ifp),
		    sppp_state_name(sp->state[IDX_CHAP]),
		    sp->rst_counter[IDX_CHAP]);

	if (--sp->rst_counter[IDX_CHAP] < 0)
		/* TO- event */
		switch (sp->state[IDX_CHAP]) {
		case STATE_REQ_SENT:
			chap.tld(sp);
			sppp_cp_change_state(&chap, sp, STATE_CLOSED);
			break;
		}
	else
		/* TO+ (or TO*) event */
		switch (sp->state[IDX_CHAP]) {
		case STATE_OPENED:
			/* TO* event */
			sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;
			/* fall through */
		case STATE_REQ_SENT:
			chap.scr(sp);
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(&chap, sp, STATE_REQ_SENT);
			break;
		}

	splx(s);
}

static void
sppp_chap_tlu(struct sppp *sp)
{
	STDDCL;
	int i, x;

	i = 0;
	sp->rst_counter[IDX_CHAP] = sp->lcp.max_configure;

	/*
	 * Some broken CHAP implementations (Conware CoNet, firmware
	 * 4.0.?) don't want to re-authenticate their CHAP once the
	 * initial challenge-response exchange has taken place.
	 * Provide for an option to avoid rechallenges.
	 */
	if ((sp->hisauth.flags & AUTHFLAG_NORECHALLENGE) == 0) {
		/*
		 * Compute the re-challenge timeout.  This will yield
		 * a number between 300 and 810 seconds.
		 */
		i = 300 + ((unsigned)(random() & 0xff00) >> 7);

#if defined(__NetBSD__)
		callout_reset(&sp->ch[IDX_CHAP], i * hz, chap.TO, sp);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		sp->ch[IDX_CHAP] =
#endif
		timeout(chap.TO, (void *)sp, i * hz);
#endif /* __NetBSD__ */
	}

	if (debug) {
		log(LOG_DEBUG,
		    SPP_FMT "chap %s, ",
		    SPP_ARGS(ifp),
		    sp->pp_phase == PHASE_NETWORK? "reconfirmed": "tlu");
		if ((sp->hisauth.flags & AUTHFLAG_NORECHALLENGE) == 0)
			addlog("next re-challenge in %d seconds\n", i);
		else
			addlog("re-challenging supressed\n");
	}

	x = splimp();
	/* indicate to LCP that we need to be closed down */
	sp->lcp.protos |= (1 << IDX_CHAP);

	if (sp->pp_flags & PP_NEEDAUTH) {
		/*
		 * Remote is authenticator, but his auth proto didn't
		 * complete yet.  Defer the transition to network
		 * phase.
		 */
		splx(x);
		return;
	}
	splx(x);

	/*
	 * If we are already in phase network, we are done here.  This
	 * is the case if this is a dummy tlu event after a re-challenge.
	 */
	if (sp->pp_phase != PHASE_NETWORK)
		sppp_phase_network(sp);
}

static void
sppp_chap_tld(struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "chap tld\n", SPP_ARGS(ifp));
#if defined(__NetBSD__)
	callout_stop(&sp->ch[IDX_CHAP]);
#else
	untimeout(chap.TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	, sp->ch[IDX_CHAP]
#endif
	);
#endif /* __NetBSD__ */
	sp->lcp.protos &= ~(1 << IDX_CHAP);

	lcp.Close(sp);
}

static void
sppp_chap_scr(struct sppp *sp)
{
	struct timeval tv;
	u_long *ch, seed;
	u_char clen;

	/* Compute random challenge. */
	ch = (u_long *)sp->myauth.challenge;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	read_random(&seed, sizeof seed);
#else
	microtime(&tv);
	seed = tv.tv_sec ^ tv.tv_usec;
#endif
	ch[0] = seed ^ random();
	ch[1] = seed ^ random();
	ch[2] = seed ^ random();
	ch[3] = seed ^ random();
	clen = AUTHKEYLEN;

	sp->confid[IDX_CHAP] = ++sp->pp_seq[IDX_CHAP];

	sppp_auth_send(&chap, sp, CHAP_CHALLENGE, sp->confid[IDX_CHAP],
		       sizeof clen, (const char *)&clen,
		       (size_t)AUTHKEYLEN, sp->myauth.challenge,
		       (size_t)sppp_strnlen(sp->myauth.name, AUTHNAMELEN),
		       sp->myauth.name,
		       0);
}
/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The PAP implementation.                           *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */
/*
 * For PAP, we need to keep a little state also if we are the peer, not the
 * authenticator.  This is since we don't get a request to authenticate, but
 * have to repeatedly authenticate ourself until we got a response (or the
 * retry counter is expired).
 */

/*
 * Handle incoming PAP packets.  */
static void
sppp_pap_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len, x;
	u_char *name, *passwd, mlen;
	int name_len, passwd_len;

	len = m->m_pkthdr.len;
	if (len < 5) {
		if (debug)
			log(LOG_DEBUG,
			    SPP_FMT "pap invalid packet length: %d bytes\n",
			    SPP_ARGS(ifp), len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (len > ntohs (h->len))
		len = ntohs (h->len);
	switch (h->type) {
	/* PAP request is my authproto */
	case PAP_REQ:
		name = 1 + (u_char*)(h+1);
		name_len = name[-1];
		passwd = name + name_len + 1;
		if (name_len > len - 6 ||
		    (passwd_len = passwd[-1]) > len - 6 - name_len) {
			if (debug) {
				log(LOG_DEBUG, SPP_FMT "pap corrupted input "
				    "<%s id=0x%x len=%d",
				    SPP_ARGS(ifp),
				    sppp_auth_type_name(PPP_PAP, h->type),
				    h->ident, ntohs(h->len));
				if (len > 4)
					sppp_print_bytes((u_char*)(h+1), len-4);
				addlog(">\n");
			}
			break;
		}
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "pap input(%s) "
			    "<%s id=0x%x len=%d name=",
			    SPP_ARGS(ifp),
			    sppp_state_name(sp->state[IDX_PAP]),
			    sppp_auth_type_name(PPP_PAP, h->type),
			    h->ident, ntohs(h->len));
			sppp_print_string((char*)name, name_len);
			addlog(" passwd=");
			sppp_print_string((char*)passwd, passwd_len);
			addlog(">\n");
		}
		if (name_len > AUTHNAMELEN ||
		    passwd_len > AUTHKEYLEN ||
		    bcmp(name, sp->hisauth.name, name_len) != 0 ||
		    bcmp(passwd, sp->hisauth.secret, passwd_len) != 0) {
			/* action scn, tld */
			mlen = sizeof(FAILMSG) - 1;
			sppp_auth_send(&pap, sp, PAP_NAK, h->ident,
				       sizeof mlen, (const char *)&mlen,
				       sizeof(FAILMSG) - 1, (u_char *)FAILMSG,
				       0);
			pap.tld(sp);
			break;
		}
		/* action sca, perhaps tlu */
		if (sp->state[IDX_PAP] == STATE_REQ_SENT ||
		    sp->state[IDX_PAP] == STATE_OPENED) {
			mlen = sizeof(SUCCMSG) - 1;
			sppp_auth_send(&pap, sp, PAP_ACK, h->ident,
				       sizeof mlen, (const char *)&mlen,
				       sizeof(SUCCMSG) - 1, (u_char *)SUCCMSG,
				       0);
		}
		if (sp->state[IDX_PAP] == STATE_REQ_SENT) {
			sppp_cp_change_state(&pap, sp, STATE_OPENED);
			pap.tlu(sp);
		}
		break;

	/* ack and nak are his authproto */
	case PAP_ACK:
#if defined(__NetBSD__)
		callout_stop(&sp->pap_my_to_ch);
#else
		untimeout(sppp_pap_my_TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		, sp->pap_my_to_ch
#endif
		);
#endif /* __NetBSD__ */
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "pap success",
			    SPP_ARGS(ifp));
			name_len = *((char *)h);
			if (len > 5 && name_len) {
				addlog(": ");
				sppp_print_string((char*)(h+1), name_len);
			}
			addlog("\n");
		}
		x = splimp();
		sp->pp_flags &= ~PP_NEEDAUTH;
		if (sp->myauth.proto == PPP_PAP &&
		    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) &&
		    (sp->lcp.protos & (1 << IDX_PAP)) == 0) {
			/*
			 * We are authenticator for PAP but didn't
			 * complete yet.  Leave it to tlu to proceed
			 * to network phase.
			 */
			splx(x);
			break;
		}
		splx(x);
		sppp_phase_network(sp);
		break;

	case PAP_NAK:
#if defined(__NetBSD__)
		callout_stop(&sp->pap_my_to_ch);
#else
		untimeout(sppp_pap_my_TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		, sp->pap_my_to_ch
#endif
		);
#endif /* __NetBSD__ */
		if (debug) {
			log(LOG_INFO, SPP_FMT "pap failure",
			    SPP_ARGS(ifp));
			name_len = *((char *)h);
			if (len > 5 && name_len) {
				addlog(": ");
				sppp_print_string((char*)(h+1), name_len);
			}
			addlog("\n");
		} else
			log(LOG_INFO, SPP_FMT "pap failure\n",
			    SPP_ARGS(ifp));
		/* await LCP shutdown by authenticator */
		break;

	default:
		/* Unknown PAP packet type -- ignore. */
		if (debug) {
			log(LOG_DEBUG, SPP_FMT "pap corrupted input "
			    "<0x%x id=0x%x len=%d",
			    SPP_ARGS(ifp),
			    h->type, h->ident, ntohs(h->len));
			if (len > 4)
				sppp_print_bytes((u_char*)(h+1), len-4);
			addlog(">\n");
		}
		break;

	}
}

static void
sppp_pap_init(struct sppp *sp)
{
	/* PAP doesn't have STATE_INITIAL at all. */
	sp->state[IDX_PAP] = STATE_CLOSED;
	sp->fail_counter[IDX_PAP] = 0;
	sp->pp_seq[IDX_PAP] = 0;
	sp->pp_rseq[IDX_PAP] = 0;
#if defined(__NetBSD__)
	callout_init(&sp->ch[IDX_PAP]);
	callout_init(&sp->pap_my_to_ch);
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	callout_handle_init(&sp->ch[IDX_PAP]);
	callout_handle_init(&sp->pap_my_to_ch);
#endif
}

static void
sppp_pap_open(struct sppp *sp)
{
	if (sp->hisauth.proto == PPP_PAP &&
	    (sp->lcp.opts & (1 << LCP_OPT_AUTH_PROTO)) != 0) {
		/* we are authenticator for PAP, start our timer */
		sp->rst_counter[IDX_PAP] = sp->lcp.max_configure;
		sppp_cp_change_state(&pap, sp, STATE_REQ_SENT);
	}
	if (sp->myauth.proto == PPP_PAP) {
		/* we are peer, send a request, and start a timer */
		pap.scr(sp);
#if defined(__NetBSD__)
		callout_reset(&sp->pap_my_to_ch, sp->lcp.timeout,
		    sppp_pap_my_TO, sp);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		sp->pap_my_to_ch =
#endif
		timeout(sppp_pap_my_TO, (void *)sp, sp->lcp.timeout);
#endif /* __NetBSD__ */
	}
}

static void
sppp_pap_close(struct sppp *sp)
{
	if (sp->state[IDX_PAP] != STATE_CLOSED)
		sppp_cp_change_state(&pap, sp, STATE_CLOSED);
}

/*
 * That's the timeout routine if we are authenticator.  Since the
 * authenticator is basically passive in PAP, we can't do much here.
 */
static void
sppp_pap_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;
	int s;

	s = splimp();
	if (debug)
		log(LOG_DEBUG, SPP_FMT "pap TO(%s) rst_counter = %d\n",
		    SPP_ARGS(ifp),
		    sppp_state_name(sp->state[IDX_PAP]),
		    sp->rst_counter[IDX_PAP]);

	if (--sp->rst_counter[IDX_PAP] < 0)
		/* TO- event */
		switch (sp->state[IDX_PAP]) {
		case STATE_REQ_SENT:
			pap.tld(sp);
			sppp_cp_change_state(&pap, sp, STATE_CLOSED);
			break;
		}
	else
		/* TO+ event, not very much we could do */
		switch (sp->state[IDX_PAP]) {
		case STATE_REQ_SENT:
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(&pap, sp, STATE_REQ_SENT);
			break;
		}

	splx(s);
}

/*
 * That's the timeout handler if we are peer.  Since the peer is active,
 * we need to retransmit our PAP request since it is apparently lost.
 * XXX We should impose a max counter.
 */
static void
sppp_pap_my_TO(void *cookie)
{
	struct sppp *sp = (struct sppp *)cookie;
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "pap peer TO\n",
		    SPP_ARGS(ifp));

	pap.scr(sp);
}

static void
sppp_pap_tlu(struct sppp *sp)
{
	STDDCL;
	int x;

	sp->rst_counter[IDX_PAP] = sp->lcp.max_configure;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "%s tlu\n",
		    SPP_ARGS(ifp), pap.name);

	x = splimp();
	/* indicate to LCP that we need to be closed down */
	sp->lcp.protos |= (1 << IDX_PAP);

	if (sp->pp_flags & PP_NEEDAUTH) {
		/*
		 * Remote is authenticator, but his auth proto didn't
		 * complete yet.  Defer the transition to network
		 * phase.
		 */
		splx(x);
		return;
	}
	splx(x);
	sppp_phase_network(sp);
}

static void
sppp_pap_tld(struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, SPP_FMT "pap tld\n", SPP_ARGS(ifp));
#if defined(__NetBSD__)
	callout_stop(&sp->ch[IDX_PAP]);
#else
	untimeout(pap.TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	, sp->ch[IDX_PAP]
#endif
	);
#endif /* __NetBSD__ */

#if defined(__NetBSD__)
	callout_stop(&sp->pap_my_to_ch);
#else
	untimeout(sppp_pap_my_TO, (void *)sp
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	, sp->pap_my_to_ch
#endif
	);
#endif /* __NetBSD__ */
	sp->lcp.protos &= ~(1 << IDX_PAP);

	lcp.Close(sp);
}

static void
sppp_pap_scr(struct sppp *sp)
{
	u_char idlen, pwdlen;

	sp->confid[IDX_PAP] = ++sp->pp_seq[IDX_PAP];
	pwdlen = sppp_strnlen(sp->myauth.secret, AUTHKEYLEN);
	idlen = sppp_strnlen(sp->myauth.name, AUTHNAMELEN);

	sppp_auth_send(&pap, sp, PAP_REQ, sp->confid[IDX_PAP],
		       sizeof idlen, (const char *)&idlen,
		       (size_t)idlen, sp->myauth.name,
		       sizeof pwdlen, (const char *)&pwdlen,
		       (size_t)pwdlen, sp->myauth.secret,
		       0);
}
/*
 * Random miscellaneous functions.
 */

/*
 * Send a PAP or CHAP proto packet.
 *
 * Varadic function, each of the elements for the ellipsis is of type
 * ``size_t mlen, const u_char *msg''.  Processing will stop iff
 * mlen == 0.
 * NOTE: never declare variadic functions with types subject to type
 * promotion (i.e. u_char). This is asking for big trouble depending
 * on the architecture you are on...
 */

static void
sppp_auth_send(const struct cp *cp, struct sppp *sp,
               unsigned int type, unsigned int id,
	       ...)
{
	STDDCL;
	struct ppp_header *h;
	struct lcp_header *lh;
	struct mbuf *m;
	u_char *p;
	int len;
	unsigned int mlen;
	const char *msg;
	va_list ap;

	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.rcvif = 0;

	h = mtod (m, struct ppp_header*);
	h->address = PPP_ALLSTATIONS;		/* broadcast address */
	h->control = PPP_UI;			/* Unnumbered Info */
	h->protocol = htons(cp->proto);

	lh = (struct lcp_header*)(h + 1);
	lh->type = type;
	lh->ident = id;
	p = (u_char*) (lh+1);

	va_start(ap, id);
	len = 0;

	while ((mlen = (unsigned int)va_arg(ap, size_t)) != 0) {
		msg = va_arg(ap, const char *);
		len += mlen;
		if (len > MHLEN - PPP_HEADER_LEN - LCP_HEADER_LEN) {
			va_end(ap);
			m_freem(m);
			return;
		}

		bcopy(msg, p, mlen);
		p += mlen;
	}
	va_end(ap);

	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + LCP_HEADER_LEN + len;
	lh->len = htons (LCP_HEADER_LEN + len);

	if (debug) {
		log(LOG_DEBUG, SPP_FMT "%s output <%s id=0x%x len=%d",
		    SPP_ARGS(ifp), cp->name,
		    sppp_auth_type_name(cp->proto, lh->type),
		    lh->ident, ntohs(lh->len));
		if (len)
			sppp_print_bytes((u_char*) (lh+1), len);
		addlog(">\n");
	}
	if (IF_QFULL (&sp->pp_cpq)) {
		IF_DROP (&sp->pp_fastq);
		IF_DROP (&ifp->if_snd);
		m_freem (m);
		++ifp->if_oerrors;
	} else
		IF_ENQUEUE (&sp->pp_cpq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Send keepalive packets, every 10 seconds.
 */
static void
sppp_keepalive(void *dummy)
{
	struct sppp *sp;
	int s;

	s = splimp();
	for (sp=spppq; sp; sp=sp->pp_next) {
		struct ifnet *ifp = &sp->pp_if;

		/* Keepalive mode disabled or channel down? */
		if (! (sp->pp_flags & PP_KEEPALIVE) ||
		    ! (ifp->if_flags & IFF_RUNNING))
			continue;

		/* No keepalive in PPP mode if LCP not opened yet. */
		if (! (sp->pp_flags & PP_CISCO) &&
		    sp->pp_phase < PHASE_AUTHENTICATE)
			continue;

		if (sp->pp_alivecnt == MAXALIVECNT) {
			/* No keepalive packets got.  Stop the interface. */
			printf (SPP_FMT "down\n", SPP_ARGS(ifp));
			if_down (ifp);
			IFQ_PURGE (&sp->pp_cpq);
			if (! (sp->pp_flags & PP_CISCO)) {
				/* XXX */
				/* Shut down the PPP link. */
				lcp.Down(sp);
				/* Initiate negotiation. XXX */
				lcp.Up(sp);
			}
		}
		if (sp->pp_alivecnt <= MAXALIVECNT)
			++sp->pp_alivecnt;
		if (sp->pp_flags & PP_CISCO)
			sppp_cisco_send (sp, CISCO_KEEPALIVE_REQ,
			    ++sp->pp_seq[IDX_LCP], sp->pp_rseq[IDX_LCP]);
		else if (sp->pp_phase >= PHASE_AUTHENTICATE) {
			long nmagic = htonl (sp->lcp.magic);
			sp->lcp.echoid = ++sp->pp_seq[IDX_LCP];
			sppp_cp_send (sp, PPP_LCP, ECHO_REQ,
				sp->lcp.echoid, 4, &nmagic);
		}
	}
	splx(s);
#if defined(__NetBSD__)
	callout_reset(&keepalive_ch, hz * 10, sppp_keepalive, NULL);
#else
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	keepalive_ch =
#endif
	timeout(sppp_keepalive, 0, hz * 10);
#endif /* __NetBSD__ */
}

/*
 * Get both IP addresses.
 */
static void
sppp_get_ip_addrs(struct sppp *sp, u_long *src, u_long *dst, u_long *srcmask)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in *si, *sm;
	u_long ssrc, ddst;

	sm = NULL;
	ssrc = ddst = 0L;
	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	for (ifa = ifp->if_addrhead.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_link.tqe_next)
#elif defined(__NetBSD__) || defined (__OpenBSD__)
	for (ifa = ifp->if_addrlist.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_list.tqe_next)
#else
	for (ifa = ifp->if_addrlist, si = 0;
	     ifa;
	     ifa = ifa->ifa_next)
#endif
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			sm = (struct sockaddr_in *)ifa->ifa_netmask;
			if (si)
				break;
		}
	if (ifa) {
		if (si && si->sin_addr.s_addr) {
			ssrc = si->sin_addr.s_addr;
			if (srcmask)
				*srcmask = ntohl(sm->sin_addr.s_addr);
		}

		si = (struct sockaddr_in *)ifa->ifa_dstaddr;
		if (si && si->sin_addr.s_addr)
			ddst = si->sin_addr.s_addr;
	}

	if (dst) *dst = ntohl(ddst);
	if (src) *src = ntohl(ssrc);
}

/*
 * Set my IP address.  Must be called at splimp.
 */
static void
sppp_set_ip_addr(struct sppp *sp, u_long src)
{
	STDDCL;
	struct ifaddr *ifa;
	struct sockaddr_in *si;

	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	for (ifa = ifp->if_addrhead.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_link.tqe_next)
#elif defined(__NetBSD__) || defined (__OpenBSD__)
	for (ifa = ifp->if_addrlist.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_list.tqe_next)
#else
	for (ifa = ifp->if_addrlist, si = 0;
	     ifa;
	     ifa = ifa->ifa_next)
#endif
	{
		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			si = (struct sockaddr_in *)ifa->ifa_addr;
			if (si)
				break;
		}
	}

	if (ifa && si)
	{
		int error;
#if __NetBSD_Version__ >= 103080000
		struct sockaddr_in new_sin = *si;

		new_sin.sin_addr.s_addr = htonl(src);
		error = in_ifinit(ifp, ifatoia(ifa), &new_sin, 1);
		if(debug && error)
		{
			log(LOG_DEBUG, SPP_FMT "sppp_set_ip_addr: in_ifinit "
			" failed, error=%d\n", SPP_ARGS(ifp), error);
		}
#else
		/* delete old route */
		error = rtinit(ifa, (int)RTM_DELETE, RTF_HOST);
		if(debug && error)
		{
			log(LOG_DEBUG, SPP_FMT "sppp_set_ip_addr: rtinit DEL failed, error=%d\n",
		    		SPP_ARGS(ifp), error);
		}

		/* set new address */
		si->sin_addr.s_addr = htonl(src);

		/* add new route */
		error = rtinit(ifa, (int)RTM_ADD, RTF_HOST);		
		if (debug && error)
		{
			log(LOG_DEBUG, SPP_FMT "sppp_set_ip_addr: rtinit ADD failed, error=%d",
		    		SPP_ARGS(ifp), error);
		}
#endif
	}
}			

#ifdef INET6
/*
 * Get both IPv6 addresses.
 */
static void
sppp_get_ip6_addrs(struct sppp *sp, struct in6_addr *src, struct in6_addr *dst,
		   struct in6_addr *srcmask)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in6 *si, *sm;
	struct in6_addr ssrc, ddst;

	sm = NULL;
	bzero(&ssrc, sizeof(ssrc));
	bzero(&ddst, sizeof(ddst));
	/*
	 * Pick the first link-local AF_INET6 address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	for (ifa = ifp->if_addrhead.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_link.tqe_next)
#elif defined(__NetBSD__) || defined (__OpenBSD__)
	for (ifa = ifp->if_addrlist.tqh_first, si = 0;
	     ifa;
	     ifa = ifa->ifa_list.tqe_next)
#else
	for (ifa = ifp->if_addrlist, si = 0;
	     ifa;
	     ifa = ifa->ifa_next)
#endif
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			si = (struct sockaddr_in6 *)ifa->ifa_addr;
			sm = (struct sockaddr_in6 *)ifa->ifa_netmask;
			if (si && IN6_IS_ADDR_LINKLOCAL(&si->sin6_addr))
				break;
		}
	if (ifa) {
		if (si && !IN6_IS_ADDR_UNSPECIFIED(&si->sin6_addr)) {
			bcopy(&si->sin6_addr, &ssrc, sizeof(ssrc));
			if (srcmask) {
				bcopy(&sm->sin6_addr, srcmask,
				    sizeof(*srcmask));
			}
		}

		si = (struct sockaddr_in6 *)ifa->ifa_dstaddr;
		if (si && !IN6_IS_ADDR_UNSPECIFIED(&si->sin6_addr))
			bcopy(&si->sin6_addr, &ddst, sizeof(ddst));
	}

	if (dst)
		bcopy(&ddst, dst, sizeof(*dst));
	if (src)
		bcopy(&ssrc, src, sizeof(*src));
}

#ifdef IPV6CP_MYIFID_DYN
/*
 * Generate random ifid.
 */
static void
sppp_gen_ip6_addr(struct sppp *sp, struct in6_addr *addr)
{
	/* TBD */
}

/*
 * Set my IPv6 address.  Must be called at splimp.
 */
static void
sppp_set_ip6_addr(struct sppp *sp, const struct in6_addr *src)
{
	STDDCL;
	struct ifaddr *ifa;
	struct sockaddr_in6 *sin6;

	/*
	 * Pick the first link-local AF_INET6 address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */

	sin6 = NULL;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	for (ifa = ifp->if_addrhead.tqh_first;
	     ifa;
	     ifa = ifa->ifa_link.tqe_next)
#elif defined(__NetBSD__) || defined (__OpenBSD__)
	for (ifa = ifp->if_addrlist.tqh_first;
	     ifa;
	     ifa = ifa->ifa_list.tqe_next)
#else
	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif
	{
		if (ifa->ifa_addr->sa_family == AF_INET6)
		{
			sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
			if (sin6 && IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				break;
		}
	}

	if (ifa && sin6)
	{
		int error;
		struct sockaddr_in6 new_sin6 = *sin6;

		bcopy(src, &new_sin6.sin6_addr, sizeof(new_sin6.sin6_addr));
		error = in6_ifinit(ifp, ifatoia6(ifa), &new_sin6, 1);
		if (debug && error)
		{
			log(LOG_DEBUG, SPP_FMT "sppp_set_ip6_addr: in6_ifinit "
			" failed, error=%d\n", SPP_ARGS(ifp), error);
		}
	}
}
#endif

/*
 * Suggest a candidate address to be used by peer.
 */
static void
sppp_suggest_ip6_addr(struct sppp *sp, struct in6_addr *suggest)
{
	struct in6_addr myaddr;
	struct timeval tv;

	sppp_get_ip6_addrs(sp, &myaddr, 0, 0);

	myaddr.s6_addr[8] &= ~0x02;	/* u bit to "local" */
	microtime(&tv);
	if ((tv.tv_usec & 0xff) == 0 && (tv.tv_sec & 0xff) == 0) {
		myaddr.s6_addr[14] ^= 0xff;
		myaddr.s6_addr[15] ^= 0xff;
	} else {
		myaddr.s6_addr[14] ^= (tv.tv_usec & 0xff);
		myaddr.s6_addr[15] ^= (tv.tv_sec & 0xff);
	}
	if (suggest)
		bcopy(&myaddr, suggest, sizeof(myaddr));
}
#endif /*INET6*/

static int
sppp_params(struct sppp *sp, int cmd, void *data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct spppreq spr;

#if 0
	/*
	 * ifr->ifr_data is supposed to point to a struct spppreq.
	 * Check the cmd word first before attempting to fetch all the
	 * data.
	 */
	if ((subcmd = fuword(ifr->ifr_data)) == -1)
		return EFAULT;
#endif

	if (copyin((caddr_t)ifr->ifr_data, &spr, sizeof spr) != 0)
		return EFAULT;

	switch (spr.cmd) {
	case SPPPIOGDEFS:
		if (cmd != (int)SIOCGIFGENERIC)
			return EINVAL;
		/*
		 * We copy over the entire current state, but clean
		 * out some of the stuff we don't wanna pass up.
		 * Remember, SIOCGIFGENERIC is unprotected, and can be
		 * called by any user.  No need to ever get PAP or
		 * CHAP secrets back to userland anyway.
		 */
		bcopy(sp, &spr.defs, sizeof(struct sppp));
		bzero(spr.defs.myauth.secret, AUTHKEYLEN);
		bzero(spr.defs.myauth.challenge, AUTHKEYLEN);
		bzero(spr.defs.hisauth.secret, AUTHKEYLEN);
		bzero(spr.defs.hisauth.challenge, AUTHKEYLEN);
		return copyout(&spr, (caddr_t)ifr->ifr_data, sizeof spr);

	case SPPPIOSDEFS:
		if (cmd != (int)SIOCSIFGENERIC)
			return EINVAL;
		/*
		 * We have a very specific idea of which fields we allow
		 * being passed back from userland, so to not clobber our
		 * current state.  For one, we only allow setting
		 * anything if LCP is in dead phase.  Once the LCP
		 * negotiations started, the authentication settings must
		 * not be changed again.  (The administrator can force an
		 * ifconfig down in order to get LCP back into dead
		 * phase.)
		 *
		 * Also, we only allow for authentication parameters to be
		 * specified.
		 *
		 * XXX Should allow to set or clear pp_flags.
		 *
		 * Finally, if the respective authentication protocol to
		 * be used is set differently than 0, but the secret is
		 * passed as all zeros, we don't trash the existing secret.
		 * This allows an administrator to change the system name
		 * only without clobbering the secret (which he didn't get
		 * back in a previous SPPPIOGDEFS call).  However, the
		 * secrets are cleared if the authentication protocol is
		 * reset to 0.
		 */
		if (sp->pp_phase != PHASE_DEAD)
			return EBUSY;

		if ((spr.defs.myauth.proto != 0 && spr.defs.myauth.proto != PPP_PAP &&
		     spr.defs.myauth.proto != PPP_CHAP) ||
		    (spr.defs.hisauth.proto != 0 && spr.defs.hisauth.proto != PPP_PAP &&
		     spr.defs.hisauth.proto != PPP_CHAP))
			return EINVAL;

		if (spr.defs.myauth.proto == 0)
			/* resetting myauth */
			bzero(&sp->myauth, sizeof sp->myauth);
		else {
			/* setting/changing myauth */
			sp->myauth.proto = spr.defs.myauth.proto;
			bcopy(spr.defs.myauth.name, sp->myauth.name, AUTHNAMELEN);
			if (spr.defs.myauth.secret[0] != '\0')
				bcopy(spr.defs.myauth.secret, sp->myauth.secret,
				      AUTHKEYLEN);
		}
		if (spr.defs.hisauth.proto == 0)
			/* resetting hisauth */
			bzero(&sp->hisauth, sizeof sp->hisauth);
		else {
			/* setting/changing hisauth */
			sp->hisauth.proto = spr.defs.hisauth.proto;
			sp->hisauth.flags = spr.defs.hisauth.flags;
			bcopy(spr.defs.hisauth.name, sp->hisauth.name, AUTHNAMELEN);
			if (spr.defs.hisauth.secret[0] != '\0')
				bcopy(spr.defs.hisauth.secret, sp->hisauth.secret,
				      AUTHKEYLEN);
		}
		break;

	default:
		return EINVAL;
	}

	return 0;
}

static void
sppp_phase_network(struct sppp *sp)
{
	STDDCL;
	int i;
	u_long mask;

	sp->pp_phase = PHASE_NETWORK;

	if(debug)
	{
		log(LOG_INFO, SPP_FMT "phase %s\n", SPP_ARGS(ifp),
			sppp_phase_name(sp->pp_phase));
	}

	/* Notify NCPs now. */
	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_NCP)
			(cps[i])->Open(sp);

	/* Send Up events to all NCPs. */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if ((sp->lcp.protos & mask) && ((cps[i])->flags & CP_NCP))
			(cps[i])->Up(sp);

	/* if no NCP is starting, all this was in vain, close down */
	sppp_lcp_check_and_close(sp);
}
	

static const char *
sppp_cp_type_name(u_char type)
{
	static char buf[12];
	switch (type) {
	case CONF_REQ:   return "conf-req";
	case CONF_ACK:   return "conf-ack";
	case CONF_NAK:   return "conf-nak";
	case CONF_REJ:   return "conf-rej";
	case TERM_REQ:   return "term-req";
	case TERM_ACK:   return "term-ack";
	case CODE_REJ:   return "code-rej";
	case PROTO_REJ:  return "proto-rej";
	case ECHO_REQ:   return "echo-req";
	case ECHO_REPLY: return "echo-reply";
	case DISC_REQ:   return "discard-req";
	}
	sprintf (buf, "0x%x", type);
	return buf;
}

static const char *
sppp_auth_type_name(u_short proto, u_char type)
{
	static char buf[12];
	switch (proto) {
	case PPP_CHAP:
		switch (type) {
		case CHAP_CHALLENGE:	return "challenge";
		case CHAP_RESPONSE:	return "response";
		case CHAP_SUCCESS:	return "success";
		case CHAP_FAILURE:	return "failure";
		}
	case PPP_PAP:
		switch (type) {
		case PAP_REQ:		return "req";
		case PAP_ACK:		return "ack";
		case PAP_NAK:		return "nak";
		}
	}
	sprintf (buf, "0x%x", type);
	return buf;
}

static const char *
sppp_lcp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case LCP_OPT_MRU:		return "mru";
	case LCP_OPT_ASYNC_MAP:		return "async-map";
	case LCP_OPT_AUTH_PROTO:	return "auth-proto";
	case LCP_OPT_QUAL_PROTO:	return "qual-proto";
	case LCP_OPT_MAGIC:		return "magic";
	case LCP_OPT_PROTO_COMP:	return "proto-comp";
	case LCP_OPT_ADDR_COMP:		return "addr-comp";
	}
	sprintf (buf, "0x%x", opt);
	return buf;
}

static const char *
sppp_ipcp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case IPCP_OPT_ADDRESSES:	return "addresses";
	case IPCP_OPT_COMPRESSION:	return "compression";
	case IPCP_OPT_ADDRESS:		return "address";
	}
	sprintf (buf, "0x%x", opt);
	return buf;
}

#ifdef INET6
static const char *
sppp_ipv6cp_opt_name(u_char opt)
{
	static char buf[12];
	switch (opt) {
	case IPV6CP_OPT_IFID:		return "ifid";
	case IPV6CP_OPT_COMPRESSION:	return "compression";
	}
	sprintf (buf, "0x%x", opt);
	return buf;
}
#endif

static const char *
sppp_state_name(int state)
{
	switch (state) {
	case STATE_INITIAL:	return "initial";
	case STATE_STARTING:	return "starting";
	case STATE_CLOSED:	return "closed";
	case STATE_STOPPED:	return "stopped";
	case STATE_CLOSING:	return "closing";
	case STATE_STOPPING:	return "stopping";
	case STATE_REQ_SENT:	return "req-sent";
	case STATE_ACK_RCVD:	return "ack-rcvd";
	case STATE_ACK_SENT:	return "ack-sent";
	case STATE_OPENED:	return "opened";
	}
	return "illegal";
}

static const char *
sppp_phase_name(enum ppp_phase phase)
{
	switch (phase) {
	case PHASE_DEAD:	return "dead";
	case PHASE_ESTABLISH:	return "establish";
	case PHASE_TERMINATE:	return "terminate";
	case PHASE_AUTHENTICATE: return "authenticate";
	case PHASE_NETWORK:	return "network";
	}
	return "illegal";
}

static const char *
sppp_proto_name(u_short proto)
{
	static char buf[12];
	switch (proto) {
	case PPP_LCP:	return "lcp";
	case PPP_IPCP:	return "ipcp";
	case PPP_PAP:	return "pap";
	case PPP_CHAP:	return "chap";
	case PPP_IPV6CP: return "ipv6cp";
	}
	sprintf(buf, "0x%x", (unsigned)proto);
	return buf;
}

static void
sppp_print_bytes(const u_char *p, u_short len)
{
	addlog(" %02x", *p++);
	while (--len > 0)
		addlog("-%02x", *p++);
}

static void
sppp_print_string(const char *p, u_short len)
{
	u_char c;

	while (len-- > 0) {
		c = *p++;
		/*
		 * Print only ASCII chars directly.  RFC 1994 recommends
		 * using only them, but we don't rely on it.  */
		if (c < ' ' || c > '~')
			addlog("\\x%x", c);
		else
			addlog("%c", c);
	}
}

static const char *
sppp_dotted_quad(u_long addr)
{
	static char s[16];
	sprintf(s, "%d.%d.%d.%d",
		(int)((addr >> 24) & 0xff),
		(int)((addr >> 16) & 0xff),
		(int)((addr >> 8) & 0xff),
		(int)(addr & 0xff));
	return s;
}

static int
sppp_strnlen(u_char *p, int max)
{
	int len;

	for (len = 0; len < max && *p; ++p)
		++len;
	return len;
}

/* a dummy, used to drop uninteresting events */
static void
sppp_null(struct sppp *unused)
{
	/* do just nothing */
}
/*
 * This file is large.  Tell emacs to highlight it nevertheless.
 *
 * Local Variables:
 * hilit-auto-highlight-maxout: 120000
 * End:
 */
