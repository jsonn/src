/*	$NetBSD: ifconfig.c,v 1.79.4.7 2001/05/01 11:06:43 he Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ifconfig.c	8.2 (Berkeley) 2/16/94";
#else
__RCSID("$NetBSD: ifconfig.c,v 1.79.4.7 2001/05/01 11:06:43 he Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <net/if_ieee80211.h>
#include <net/if_vlanvar.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef INET6
#include <netinet6/nd6.h>
#endif
#include <arpa/inet.h>

#include <netatalk/at.h>

#define	NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netdb.h>

#define EON
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <sys/protosw.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

struct	ifreq		ifr, ridreq;
struct	ifaliasreq	addreq __attribute__((aligned(4)));
#ifdef INET6
struct	in6_ifreq	ifr6;
struct	in6_ifreq	in6_ridreq;
struct	in6_aliasreq	in6_addreq __attribute__((aligned(4)));
#endif
struct	iso_ifreq	iso_ridreq;
struct	iso_aliasreq	iso_addreq;
struct	sockaddr_in	netmask;
struct	netrange	at_nr;		/* AppleTalk net range */

char	name[30];
int	flags, metric, mtu, setaddr, setipdst, doalias;
int	clearaddr, s;
int	newaddr = -1;
int	nsellength = 1;
int	af;
int	aflag, bflag, Cflag, dflag, lflag, mflag, sflag, uflag;
#ifdef INET6
int	Lflag;
#endif
int	reset_if_flags;
int	explicit_prefix = 0;
u_int	vlan_tag = (u_int)-1;

void 	notealias __P((const char *, int));
void 	notrailers __P((const char *, int));
void 	setifaddr __P((const char *, int));
void 	setifdstaddr __P((const char *, int));
void 	setifflags __P((const char *, int));
void 	setifbroadaddr __P((const char *, int));
void 	setifipdst __P((const char *, int));
void 	setifmetric __P((const char *, int));
void 	setifmtu __P((const char *, int));
void	setifnwid __P((const char *, int));
void	setifnwkey __P((const char *, int));
void 	setifnetmask __P((const char *, int));
void	setifprefixlen __P((const char *, int));
void 	setnsellength __P((const char *, int));
void 	setsnpaoffset __P((const char *, int));
void	setatrange __P((const char *, int));
void	setatphase __P((const char *, int));
void	settunnel __P((const char *, const char *));
void	deletetunnel __P((const char *, int));
#ifdef INET6
void 	setia6flags __P((const char *, int));
void	setia6pltime __P((const char *, int));
void	setia6vltime __P((const char *, int));
void	setia6lifetime __P((const char *, const char *));
#endif
void	checkatrange __P ((struct sockaddr_at *));
void	setmedia __P((const char *, int));
void	setmediaopt __P((const char *, int));
void	unsetmediaopt __P((const char *, int));
void	setmediainst __P((const char *, int));
void	clone_create __P((const char *, int));
void	clone_destroy __P((const char *, int));
void	fixnsel __P((struct sockaddr_iso *));
void	setvlan __P((const char *, int));
void	setvlanif __P((const char *, int));
void	unsetvlanif __P((const char *, int));
int	main __P((int, char *[]));

/*
 * Media stuff.  Whenever a media command is first performed, the
 * currently select media is grabbed for this interface.  If `media'
 * is given, the current media word is modifed.  `mediaopt' commands
 * only modify the set and clear words.  They then operate on the
 * current media word later.
 */
int	media_current;
int	mediaopt_set;
int	mediaopt_clear;

int	actions;			/* Actions performed */

#define	A_MEDIA		0x0001		/* media command */
#define	A_MEDIAOPTSET	0x0002		/* mediaopt command */
#define	A_MEDIAOPTCLR	0x0004		/* -mediaopt command */
#define	A_MEDIAOPT	(A_MEDIAOPTSET|A_MEDIAOPTCLR)
#define	A_MEDIAINST	0x0008		/* instance or inst command */

#define	NEXTARG		0xffffff
#define	NEXTARG2	0xfffffe

const struct cmd {
	const char *c_name;
	int	c_parameter;	/* NEXTARG means next argv */
	int	c_action;	/* defered action */
	void	(*c_func) __P((const char *, int));
	void	(*c_func2) __P((const char *, const char *));
} cmds[] = {
	{ "up",		IFF_UP,		0,		setifflags } ,
	{ "down",	-IFF_UP,	0,		setifflags },
	{ "trailers",	-1,		0,		notrailers },
	{ "-trailers",	1,		0,		notrailers },
	{ "arp",	-IFF_NOARP,	0,		setifflags },
	{ "-arp",	IFF_NOARP,	0,		setifflags },
	{ "debug",	IFF_DEBUG,	0,		setifflags },
	{ "-debug",	-IFF_DEBUG,	0,		setifflags },
	{ "alias",	IFF_UP,		0,		notealias },
	{ "-alias",	-IFF_UP,	0,		notealias },
	{ "delete",	-IFF_UP,	0,		notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	0,		setifflags },
	{ "-swabips",	-EN_SWABIPS,	0,		setifflags },
#endif
	{ "netmask",	NEXTARG,	0,		setifnetmask },
	{ "metric",	NEXTARG,	0,		setifmetric },
	{ "mtu",	NEXTARG,	0,		setifmtu },
	{ "nwid",	NEXTARG,	0,		setifnwid },
	{ "nwkey",	NEXTARG,	0,		setifnwkey },
	{ "-nwkey",	-1,		0,		setifnwkey },
	{ "broadcast",	NEXTARG,	0,		setifbroadaddr },
	{ "ipdst",	NEXTARG,	0,		setifipdst },
	{ "prefixlen",  NEXTARG,	0,		setifprefixlen},
#ifdef INET6
	{ "anycast",	IN6_IFF_ANYCAST,	0,	setia6flags },
	{ "-anycast",	-IN6_IFF_ANYCAST,	0,	setia6flags },
	{ "tentative",	IN6_IFF_TENTATIVE,	0,	setia6flags },
	{ "-tentative",	-IN6_IFF_TENTATIVE,	0,	setia6flags },
	{ "pltime",	NEXTARG,	0,		setia6pltime },
	{ "vltime",	NEXTARG,	0,		setia6vltime },
#endif /*INET6*/
#ifndef INET_ONLY
	{ "range",	NEXTARG,	0,		setatrange },
	{ "phase",	NEXTARG,	0,		setatphase },
	{ "snpaoffset",	NEXTARG,	0,		setsnpaoffset },
	{ "nsellength",	NEXTARG,	0,		setnsellength },
#endif	/* INET_ONLY */
	{ "tunnel",	NEXTARG2,	0,		NULL,
							settunnel } ,
	{ "deletetunnel", 0,		0,		deletetunnel },
	{ "vlan",	NEXTARG,	0,		setvlan } ,
	{ "vlanif",	NEXTARG,	0,		setvlanif } ,
	{ "-vlanif",	0,		0,		unsetvlanif } ,
	{ "deletetunnel", 0,		0,		deletetunnel },
#if 0
	/* XXX `create' special-cased below */
	{ "create",	0,		0,		clone_create } ,
#endif
	{ "destroy",	0,		0,		clone_destroy } ,
	{ "link0",	IFF_LINK0,	0,		setifflags } ,
	{ "-link0",	-IFF_LINK0,	0,		setifflags } ,
	{ "link1",	IFF_LINK1,	0,		setifflags } ,
	{ "-link1",	-IFF_LINK1,	0,		setifflags } ,
	{ "link2",	IFF_LINK2,	0,		setifflags } ,
	{ "-link2",	-IFF_LINK2,	0,		setifflags } ,
	{ "media",	NEXTARG,	A_MEDIA,	setmedia },
	{ "mediaopt",	NEXTARG,	A_MEDIAOPTSET,	setmediaopt },
	{ "-mediaopt",	NEXTARG,	A_MEDIAOPTCLR,	unsetmediaopt },
	{ "instance",	NEXTARG,	A_MEDIAINST,	setmediainst },
	{ "inst",	NEXTARG,	A_MEDIAINST,	setmediainst },
	{ 0,		0,		0,		setifaddr },
	{ 0,		0,		0,		setifdstaddr },
};

void 	adjust_nsellength __P((void));
int	getinfo __P((struct ifreq *));
int	carrier __P((void));
void	getsock __P((int));
void	printall __P((const char *));
void	list_cloners __P((void));
void 	printb __P((const char *, unsigned short, const char *));
int	prefix __P((void *, int));
void 	status __P((const u_int8_t *, int));
void 	usage __P((void));
const char *get_string __P((const char *, const char *, u_int8_t *, int *));
void	print_string __P((const u_int8_t *, int));
char	*sec2str __P((time_t));

const char *get_media_type_string __P((int));
const char *get_media_subtype_string __P((int));
int	get_media_subtype __P((int, const char *));
int	get_media_options __P((int, const char *));
int	lookup_media_word __P((struct ifmedia_description *, int,
	    const char *));
void	print_media_word __P((int, int, int));
void	process_media_commands __P((void));
void	init_current_media __P((void));

/*
 * XNS support liberally adapted from code written at the University of
 * Maryland principally by James O'Toole and Chris Torek.
 */
void	in_alias __P((struct ifreq *));
void	in_status __P((int));
void 	in_getaddr __P((const char *, int));
#ifdef INET6
void	in6_fillscopeid __P((struct sockaddr_in6 *sin6));
void	in6_alias __P((struct in6_ifreq *));
void	in6_status __P((int));
void 	in6_getaddr __P((const char *, int));
void 	in6_getprefix __P((const char *, int));
#endif
void	at_status __P((int));
void	at_getaddr __P((const char *, int));
void 	xns_status __P((int));
void 	xns_getaddr __P((const char *, int));
void 	iso_status __P((int));
void 	iso_getaddr __P((const char *, int));

void	ieee80211_status __P((void));
void	tunnel_status __P((void));
void	vlan_status __P((void));

/* Known address families */
struct afswtch {
	const char *af_name;
	short af_af;
	void (*af_status) __P((int));
	void (*af_getaddr) __P((const char *, int));
	void (*af_getprefix) __P((const char *, int));
	u_long af_difaddr;
	u_long af_aifaddr;
	u_long af_gifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, SIOCGIFADDR, C(ridreq), C(addreq) },
#ifdef INET6
	{ "inet6", AF_INET6, in6_status, in6_getaddr, in6_getprefix,
	     SIOCDIFADDR_IN6, SIOCAIFADDR_IN6,
	     /*
	      * Deleting the first address before setting new one is
	      * not prefered way in this protocol.
	      */
	     0,
	     C(in6_ridreq), C(in6_addreq) },
#endif
#ifndef INET_ONLY	/* small version, for boot media */
	{ "atalk", AF_APPLETALK, at_status, at_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, SIOCGIFADDR, C(addreq), C(addreq) },
	{ "ns", AF_NS, xns_status, xns_getaddr, NULL,
	     SIOCDIFADDR, SIOCAIFADDR, SIOCGIFADDR, C(ridreq), C(addreq) },
	{ "iso", AF_ISO, iso_status, iso_getaddr, NULL,
	     SIOCDIFADDR_ISO, SIOCAIFADDR_ISO, SIOCGIFADDR_ISO,
	     C(iso_ridreq), C(iso_addreq) },
#endif	/* INET_ONLY */
	{ 0,	0,	    0,		0 }
};

struct afswtch *afp;	/*the address family being set or asked about*/

struct afswtch *lookup_af __P((const char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct ifreq ifreq;
	int ch;

	/* Parse command-line options */
	aflag = mflag = 0;
	while ((ch = getopt(argc, argv, "AabCdlmsu"
#ifdef INET6
					"L"
#endif
			)) != -1) {
		switch (ch) {
		case 'A':
			warnx("-A is deprecated");
			break;

		case 'a':
			aflag = 1;
			break;

		case 'b':
			bflag = 1;
			break;
			

		case 'C':
			Cflag = 1;
			break;

		case 'd':
			dflag = 1;
			break;

#ifdef INET6
		case 'L':
			Lflag = 1;
			break;
#endif

		case 'l':
			lflag = 1;
			break;

		case 'm':
			mflag = 1;
			break;

		case 's':
			sflag = 1;
			break;

		case 'u':
			uflag = 1;
			break;

			
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * -l means "list all interfaces", and is mutally exclusive with
	 * all other flags/commands.
	 *
	 * -C means "list all names of cloners", and it mutually exclusive
	 * with all other flags/commands.
	 *
	 * -a means "print status of all interfaces".
	 */
	if ((lflag || Cflag) && (aflag || mflag || argc))
		usage();
#ifdef INET6
	if ((lflag || Cflag) && Lflag)
		usage();
#endif
	if (lflag && Cflag)
		usage();
	if (Cflag) {
		if (argc)
			usage();
		list_cloners();
		exit(0);
	}
	if (aflag || lflag) {
		if (argc > 1)
			usage();
		else if (argc == 1) {
			afp = lookup_af(argv[0]);
			if (afp == NULL)
				usage();
		}
		if (afp)
			af = ifr.ifr_addr.sa_family = afp->af_af;
		else
			af = ifr.ifr_addr.sa_family = afs[0].af_af;
		printall(NULL);
		exit(0);
	}

	/* Make sure there's an interface name. */
	if (argc < 1)
		usage();
	(void) strncpy(name, argv[0], sizeof(name));
	argc--; argv++;

	/*
	 * NOTE:  We must special-case the `create' command right
	 * here as we would otherwise fail in getinfo().
	 */
	if (argc > 0 && strcmp(argv[0], "create") == 0) {
		clone_create(argv[0], 0);
		argc--, argv++;
		if (argc == 0)
			exit(0);
	}

	/* Check for address family. */
	afp = NULL;
	if (argc > 0) {
		afp = lookup_af(argv[0]);
		if (afp != NULL) {
			argv++;
			argc--;
		}
	}

	/* Initialize af, just for use in getinfo(). */
	if (afp == NULL)
		af = afs->af_af;
	else
		af = afp->af_af;

	/* Get information about the interface. */
	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (getinfo(&ifr) < 0)
		exit(1);

	if (sflag) {
		if (argc != 0)
			usage();
		else
			exit(carrier());
	}

	/* No more arguments means interface status. */
	if (argc == 0) {
		printall(name);
		exit(0);
	}

	/* The following operations assume inet family as the default. */
	if (afp == NULL)
		afp = afs;
	af = ifr.ifr_addr.sa_family = afp->af_af;

#ifdef INET6
	/* initialization */
	in6_addreq.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
	in6_addreq.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
#endif

	/* Process commands. */
	while (argc > 0) {
		const struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(argv[0], p->c_name) == 0)
				break;
		if (p->c_name == 0 && setaddr) {
			if ((flags & IFF_POINTOPOINT) == 0) {
				errx(1, "can't set destination address %s",
				     "on non-point-to-point link");
			}
			p++;	/* got src, do dst */
		}
		if (p->c_func != NULL || p->c_func2 != NULL) {
			if (p->c_parameter == NEXTARG) {
				if (argc < 2)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0);
				argc--, argv++;
			} else if (p->c_parameter == NEXTARG2) {
				if (argc < 3)
					errx(1, "'%s' requires 2 arguments",
					    p->c_name);
				(*p->c_func2)(argv[1], argv[2]);
				argc -= 2, argv += 2;
			} else
				(*p->c_func)(argv[0], p->c_parameter);
			actions |= p->c_action;
		}
		argc--, argv++;
	}

	/* Process any media commands that may have been issued. */
	process_media_commands();

	if (af == AF_INET6 && explicit_prefix == 0) {
		/*
		 * Aggregatable address architecture defines all prefixes
		 * are 64. So, it is convenient to set prefixlen to 64 if
		 * it is not specified.
		 */
		setifprefixlen("64", 0);
		/* in6_getprefix("64", MASK) if MASK is available here... */
	}

#ifndef INET_ONLY
	if (af == AF_ISO)
		adjust_nsellength();

	if (af == AF_APPLETALK)
		checkatrange((struct sockaddr_at *) &addreq.ifra_addr);

	if (setipdst && af==AF_NS) {
		struct nsip_req rq;
		int size = sizeof(rq);

		rq.rq_ns = addreq.ifra_addr;
		rq.rq_ip = addreq.ifra_dstaddr;

		if (setsockopt(s, 0, SO_NSIP_ROUTE, &rq, size) < 0)
			warn("encapsulation routing");
	}

#endif	/* INET_ONLY */

	if (clearaddr) {
		(void) strncpy(afp->af_ridreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, afp->af_difaddr, afp->af_ridreq) < 0)
			err(1, "SIOCDIFADDR");
	}
	if (newaddr > 0) {
		(void) strncpy(afp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, afp->af_aifaddr, afp->af_addreq) < 0)
			warn("SIOCAIFADDR");
	}

	if (reset_if_flags) {
		(void) strncpy(ifreq.ifr_name, name, sizeof(ifreq.ifr_name));
		ifreq.ifr_flags = flags;
		if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifreq) < 0)
			err(1, "SIOCSIFFLAGS");
	}
	exit(0);
}

struct afswtch *
lookup_af(cp)
	const char *cp;
{
	struct afswtch *a;

	for (a = afs; a->af_name != NULL; a++)
		if (strcmp(a->af_name, cp) == 0)
			return (a);
	return (NULL);
}

void
getsock(naf)
	int naf;
{
	static int oaf = -1;

	if (oaf == naf)
		return;
	if (oaf != -1)
		close(s);
	s = socket(naf, SOCK_DGRAM, 0);
	if (s < 0)
		oaf = -1;
	else
		oaf = naf;
}

int
getinfo(ifr)
	struct ifreq *ifr;
{

	getsock(af);
	if (s < 0)
		err(1, "socket");
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)ifr) < 0) {
		warn("SIOCGIFFLAGS %s", ifr->ifr_name);
		return (-1);
	}
	flags = ifr->ifr_flags;
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)ifr) < 0) {
		warn("SIOCGIFMETRIC %s", ifr->ifr_name);
		metric = 0;
	} else
		metric = ifr->ifr_metric;
	if (ioctl(s, SIOCGIFMTU, (caddr_t)ifr) < 0)
		mtu = 0;
	else
		mtu = ifr->ifr_mtu;
	return (0);
}

void
printall(ifname)
	const char *ifname;
{
#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifap, *ifa;
	struct ifreq ifr;
	const struct sockaddr_dl *sdl = NULL;
	int idx;
	char *p;

	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");
	p = NULL;
	idx = 0;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		if (sizeof(ifr.ifr_addr) >= ifa->ifa_addr->sa_len) {
			memcpy(&ifr.ifr_addr, ifa->ifa_addr,
			    ifa->ifa_addr->sa_len);
		}

		if (ifname && strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family == AF_LINK)
			sdl = (const struct sockaddr_dl *) ifa->ifa_addr;
		if (p && strcmp(p, ifa->ifa_name) == 0)
			continue;
		(void) strncpy(name, ifa->ifa_name, sizeof(name));
		name[sizeof(name) - 1] = '\0';
		p = ifa->ifa_name;

		if (getinfo(&ifr) < 0)
			continue;
		if (bflag && (ifa->ifa_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)))
			continue;
		if (dflag && (ifa->ifa_flags & IFF_UP) != 0)
			continue;
		if (uflag && (ifa->ifa_flags & IFF_UP) == 0)
			continue;

		if (sflag && carrier())
			continue;
		idx++;
		/*
		 * Are we just listing the interfaces?
		 */
		if (lflag) {
			if (idx > 1)
				putchar(' ');
			fputs(name, stdout);
			continue;
		}

		if (sdl == NULL) {
			status(NULL, 0);
		} else {
			status(LLADDR(sdl), sdl->sdl_alen);
			sdl = NULL;
		}
	}
	if (lflag)
		putchar('\n');
	freeifaddrs(ifap);
#else
	char inbuf[8192];
	const struct sockaddr_dl *sdl = NULL;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	int i, siz, idx;
	char ifrbuf[8192], *cp;

	ifc.ifc_len = sizeof(inbuf);
	ifc.ifc_buf = inbuf;
	getsock(af);
	if (s < 0)
		err(1, "socket");
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
		err(1, "SIOCGIFCONF");
	ifreq.ifr_name[0] = '\0';
	for (i = 0, idx = 0; i < ifc.ifc_len; ) {
		/* Copy the mininum ifreq into the buffer. */
		cp = ((caddr_t)ifc.ifc_req + i);
		memcpy(ifrbuf, cp, sizeof(*ifr));

		/* Now compute the actual size of the ifreq. */
		ifr = (struct ifreq *)ifrbuf;
		siz = ifr->ifr_addr.sa_len;
		if (siz < sizeof(ifr->ifr_addr))
			siz = sizeof(ifr->ifr_addr);
		siz += sizeof(ifr->ifr_name);
		i += siz;

		/* Now copy the whole thing. */
		if (sizeof(ifrbuf) < siz)
			errx(1, "ifr too big");
		memcpy(ifrbuf, cp, siz);

		if (ifname && strncmp(ifname, ifr->ifr_name,
		    sizeof(ifr->ifr_name)))
			continue;
		if (ifr->ifr_addr.sa_family == AF_LINK)
			sdl = (const struct sockaddr_dl *) &ifr->ifr_addr;
		if (!strncmp(ifreq.ifr_name, ifr->ifr_name,
			     sizeof(ifr->ifr_name)))
			continue;
		(void) strncpy(name, ifr->ifr_name, sizeof(ifr->ifr_name));
		ifreq = *ifr;

		if (getinfo(&ifreq) < 0)
			continue;
		if (bflag && (flags & (IFF_POINTOPOINT|IFF_LOOPBACK)))
			continue;
		if (dflag && (flags & IFF_UP) != 0)
			continue;
		if (uflag && (flags & IFF_UP) == 0)
			continue;

		if (sflag && carrier())
			continue;
		idx++;
		/*
		 * Are we just listing the interfaces?
		 */
		if (lflag) {
			if (idx > 1)
				putchar(' ');
			fputs(name, stdout);
			continue;
		}

		if (sdl == NULL) {
			status(NULL, 0);
		} else {
			status(LLADDR(sdl), sdl->sdl_alen);
			sdl = NULL;
		}
	}
	if (lflag)
		putchar('\n');
#endif
}

void
list_cloners(void)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx;

	memset(&ifcr, 0, sizeof(ifcr));

	getsock(AF_INET);

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for count");

	buf = malloc(ifcr.ifcr_total * IFNAMSIZ);
	if (buf == NULL)
		err(1, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (ioctl(s, SIOCIFGCLONERS, &ifcr) < 0)
		err(1, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (idx > 0)
			putchar(' ');
		printf("%s", cp);
	}

	putchar('\n');
	free(buf);
	return;
}

/*ARGSUSED*/
void
clone_create(addr, param)
	const char *addr;
	int param;
{

	/* We're called early... */
	getsock(AF_INET);

	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFCREATE, &ifr) < 0)
		err(1, "SIOCIFCREATE");
}

/*ARGSUSED*/
void
clone_destroy(addr, param)
	const char *addr;
	int param;
{

	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCIFDESTROY, &ifr) < 0)
		err(1, "SIOCIFDESTROY");
}

#define RIDADDR 0
#define ADDR	1
#define MASK	2
#define DSTADDR	3

/*ARGSUSED*/
void
setifaddr(addr, param)
	const char *addr;
	int param;
{
	struct ifreq *ifr;		/* XXX */

	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (newaddr == -1)
		newaddr = 1;
	if (doalias == 0 && afp->af_gifaddr != 0) {
		ifr = (struct ifreq *)afp->af_ridreq;
		(void) strncpy(ifr->ifr_name, name, sizeof(ifr->ifr_name));
		ifr->ifr_addr.sa_family = afp->af_af;
		if (ioctl(s, afp->af_gifaddr, afp->af_ridreq) == 0)
			clearaddr = 1;
		else if (errno == EADDRNOTAVAIL)
			/* No address was assigned yet. */
			;
		else
			err(1, "SIOCGIFADDR");
	}

	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

void
settunnel(src, dst)
	const char *src, *dst;
{
	struct addrinfo hints, *srcres, *dstres;
	struct ifaliasreq addreq;
	int ecode;
#ifdef INET6
	struct in6_aliasreq in6_addreq; 
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = afp->af_af;

	if ((ecode = getaddrinfo(src, NULL, NULL, &srcres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if ((ecode = getaddrinfo(dst, NULL, NULL, &dstres)) != 0)  
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (srcres->ai_addr->sa_family != dstres->ai_addr->sa_family)
		errx(1,
		    "source and destination address families do not match");

	switch (srcres->ai_addr->sa_family) {
	case AF_INET:
		memset(&addreq, 0, sizeof(addreq));
		strncpy(addreq.ifra_name, name, IFNAMSIZ);
		memcpy(&addreq.ifra_addr, srcres->ai_addr,
		    srcres->ai_addr->sa_len);
		memcpy(&addreq.ifra_dstaddr, dstres->ai_addr,
		    dstres->ai_addr->sa_len);

		if (ioctl(s, SIOCSIFPHYADDR, &addreq) < 0)
			warn("SIOCSIFPHYADDR");
		break;

#ifdef INET6
	case AF_INET6:
		memset(&in6_addreq, 0, sizeof(in6_addreq));
		strncpy(in6_addreq.ifra_name, name, IFNAMSIZ);
		memcpy(&in6_addreq.ifra_addr, srcres->ai_addr,
		    srcres->ai_addr->sa_len);
		memcpy(&in6_addreq.ifra_dstaddr, dstres->ai_addr,
		    dstres->ai_addr->sa_len);

		if (ioctl(s, SIOCSIFPHYADDR_IN6, &in6_addreq) < 0)
			warn("SIOCSIFPHYADDR_IN6");
		break;
#endif /* INET6 */

	default:
		warn("address family not supported");
	}

	freeaddrinfo(srcres);
	freeaddrinfo(dstres);
}

/* ARGSUSED */
void
deletetunnel(vname, param)
	const char *vname;
	int param;
{

	if (ioctl(s, SIOCDIFPHYADDR, &ifr) < 0)
		err(1, "SIOCDIFPHYADDR");
}

void setvlan(val, d)
	const char *val;
	int d;
{
	struct vlanreq vlr;

	if (strncmp(ifr.ifr_name, "vlan", 4) != 0 ||
	    !isdigit(ifr.ifr_name[4]))
		errx(EXIT_FAILURE,
		    "``vlan'' valid only with vlan(4) interfaces");

	vlan_tag = atoi(val);

	memset(&vlr, 0, sizeof(vlr));
	ifr.ifr_data = (caddr_t)&vlr;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCGETVLAN");

	vlr.vlr_tag = vlan_tag;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCSETVLAN");
}

void setvlanif(val, d)
	const char *val;
	int d;
{
	struct vlanreq vlr;

	if (strncmp(ifr.ifr_name, "vlan", 4) != 0 ||
	    !isdigit(ifr.ifr_name[4]))
		errx(EXIT_FAILURE,
		    "``vlanif'' valid only with vlan(4) interfaces");

	if (vlan_tag == (u_int)-1)
		errx(EXIT_FAILURE,
		    "must specify both ``vlan'' and ``vlanif''");

	memset(&vlr, 0, sizeof(vlr));
	ifr.ifr_data = (caddr_t)&vlr;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCGETVLAN");

	strlcpy(vlr.vlr_parent, val, sizeof(vlr.vlr_parent));
	vlr.vlr_tag = vlan_tag;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCSETVLAN");
}

void unsetvlanif(val, d)
	const char *val;
	int d;
{
	struct vlanreq vlr;

	if (strncmp(ifr.ifr_name, "vlan", 4) != 0 ||
	    !isdigit(ifr.ifr_name[4]))
		errx(EXIT_FAILURE,
		    "``vlanif'' valid only with vlan(4) interfaces");

	memset(&vlr, 0, sizeof(vlr));
	ifr.ifr_data = (caddr_t)&vlr;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCGETVLAN");

	vlr.vlr_parent[0] = '\0';
	vlr.vlr_tag = 0;

	if (ioctl(s, SIOCSETVLAN, (caddr_t)&ifr) == -1)
		err(EXIT_FAILURE, "SIOCSETVLAN");
}

void
setifnetmask(addr, d)
	const char *addr;
	int d;
{
	(*afp->af_getaddr)(addr, MASK);
}

void
setifbroadaddr(addr, d)
	const char *addr;
	int d;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

void
setifipdst(addr, d)
	const char *addr;
	int d;
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}

#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))
/*ARGSUSED*/
void
notealias(addr, param)
	const char *addr;
	int param;
{
	if (setaddr && doalias == 0 && param < 0)
		(void) memcpy(rqtosa(af_ridreq), rqtosa(af_addreq),
		    rqtosa(af_addreq)->sa_len);
	doalias = param;
	if (param < 0) {
		clearaddr = 1;
		newaddr = 0;
	} else
		clearaddr = 0;
}

/*ARGSUSED*/
void
notrailers(vname, value)
	const char *vname;
	int value;
{
	puts("Note: trailers are no longer sent, but always received");
}

/*ARGSUSED*/
void
setifdstaddr(addr, param)
	const char *addr;
	int param;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

void
setifflags(vname, value)
	const char *vname;
	int value;
{
	struct ifreq ifreq;

	(void) strncpy(ifreq.ifr_name, name, sizeof(ifreq.ifr_name));
 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifreq) < 0)
		err(1, "SIOCGIFFLAGS");
 	flags = ifreq.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	ifreq.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifreq) < 0)
		err(1, "SIOCSIFFLAGS");

	reset_if_flags = 1;
}

#ifdef INET6
void
setia6flags(vname, value)
	const char *vname;
	int value;
{
	if (value < 0) {
		value = -value;
		in6_addreq.ifra_flags &= ~value;
	} else
		in6_addreq.ifra_flags |= value;
}

void
setia6pltime(val, d)
	const char *val;
	int d;
{
	setia6lifetime("pltime", val);
}

void
setia6vltime(val, d)
	const char *val;
	int d;
{
	setia6lifetime("vltime", val);
}

void
setia6lifetime(cmd, val)
	const char *cmd;
	const char *val;
{
	time_t newval, t;
	char *ep;

	t = time(NULL);
	newval = (time_t)strtoul(val, &ep, 0);
	if (val == ep)
		errx(1, "invalid %s", cmd);
	if (afp->af_af != AF_INET6)
		errx(1, "%s not allowed for the AF", cmd);
	if (strcmp(cmd, "vltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_expire = t + newval;
		in6_addreq.ifra_lifetime.ia6t_vltime = newval;
	} else if (strcmp(cmd, "pltime") == 0) {
		in6_addreq.ifra_lifetime.ia6t_preferred = t + newval;
		in6_addreq.ifra_lifetime.ia6t_pltime = newval;
	}
}
#endif

void
setifmetric(val, d)
	const char *val;
	int d;
{
	(void) strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		warn("SIOCSIFMETRIC");
}

void
setifmtu(val, d)
	const char *val;
	int d;
{
	(void)strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = atoi(val);
	if (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		warn("SIOCSIFMTU");
}

const char *
get_string(val, sep, buf, lenp)
	const char *val, *sep;
	u_int8_t *buf;
	int *lenp;
{
	int len;
	int hexstr;
	u_int8_t *p;

	len = *lenp;
	p = buf;
	hexstr = (val[0] == '0' && tolower((u_char)val[1]) == 'x');
	if (hexstr)
		val += 2;
	for (;;) {
		if (*val == '\0')
			break;
		if (sep != NULL && strchr(sep, *val) != NULL) {
			val++;
			break;
		}
		if (hexstr) {
			if (!isxdigit((u_char)val[0]) ||
			    !isxdigit((u_char)val[1])) {
				warnx("bad hexadecimal digits");
				return NULL;
			}
		}
		if (p > buf + len) {
			if (hexstr)
				warnx("hexadecimal digits too long");
			else
				warnx("strings too long");
			return NULL;
		}
		if (hexstr) {
#define	tohex(x)	(isdigit(x) ? (x) - '0' : tolower(x) - 'a' + 10)
			*p++ = (tohex((u_char)val[0]) << 4) |
			    tohex((u_char)val[1]);
#undef tohex
			val += 2;
		} else
			*p++ = *val++;
	}
	len = p - buf;
	if (len < *lenp)
		memset(p, 0, *lenp - len);
	*lenp = len;
	return val;
}

void
print_string(buf, len)
	const u_int8_t *buf;
	int len;
{
	int i;
	int hasspc;

	i = 0;
	hasspc = 0;
	if (len < 2 || buf[0] != '0' || tolower(buf[1]) != 'x') {
		for (; i < len; i++) {
			if (!isprint(buf[i]))
				break;
			if (isspace(buf[i]))
				hasspc++;
		}
	}
	if (i == len) {
		if (hasspc || len == 0)
			printf("\"%.*s\"", len, buf);
		else
			printf("%.*s", len, buf);
	} else {
		printf("0x");
		for (i = 0; i < len; i++)
			printf("%02x", buf[i]);
	}
}

void
setifnwid(val, d)
	const char *val;
	int d;
{
	struct ieee80211_nwid nwid;
	int len;

	len = sizeof(nwid.i_nwid);
	if (get_string(val, NULL, nwid.i_nwid, &len) == NULL)
		return;
	nwid.i_len = len;
	(void)strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&nwid;
	if (ioctl(s, SIOCS80211NWID, (caddr_t)&ifr) < 0)
		warn("SIOCS80211NWID");
}

void
setifnwkey(val, d)
	const char *val;
	int d;
{
	struct ieee80211_nwkey nwkey;
	int i;
	u_int8_t keybuf[IEEE80211_WEP_NKID][16];

	nwkey.i_wepon = 1;
	nwkey.i_defkid = 1;
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		nwkey.i_key[i].i_keylen = sizeof(keybuf[i]);
		nwkey.i_key[i].i_keydat = keybuf[i];
	}
	if (d != 0) {
		/* disable WEP encryption */
		nwkey.i_wepon = 0;
		i = 0;
	} else if (isdigit(val[0]) && val[1] == ':') {
		/* specifying a full set of four keys */
		nwkey.i_defkid = val[0] - '0';
		val += 2;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			val = get_string(val, ",", keybuf[i],
			    &nwkey.i_key[i].i_keylen);
			if (val == NULL)
				return;
		}
		if (*val != '\0') {
			warnx("SIOCS80211NWKEY: too many keys.");
			return;
		}
	} else {
		val = get_string(val, NULL, keybuf[0],
		    &nwkey.i_key[0].i_keylen);
		if (val == NULL)
			return;
		i = 1;
	}
	for (; i < IEEE80211_WEP_NKID; i++)
		nwkey.i_key[i].i_keylen = 0;
	(void)strncpy(nwkey.i_name, name, sizeof(nwkey.i_name));
	if (ioctl(s, SIOCS80211NWKEY, (caddr_t)&nwkey) < 0)
		warn("SIOCS80211NWKEY");
}

void
ieee80211_status()
{
	int i;
	struct ieee80211_nwid nwid;
	struct ieee80211_nwkey nwkey;
	u_int8_t keybuf[IEEE80211_WEP_NKID][16];

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_data = (caddr_t)&nwid;
	(void)strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCG80211NWID, (caddr_t)&ifr) != 0)
		return;
	if (nwid.i_len > IEEE80211_NWID_LEN) {
		warnx("SIOCG80211NWID: wrong length of nwid (%d)", nwid.i_len);
		return;
	}
	printf("\tnwid ");
	print_string(nwid.i_nwid, nwid.i_len);
	memset(&nwkey, 0, sizeof(nwkey));
	(void)strncpy(nwkey.i_name, name, sizeof(nwkey.i_name));
	/* show nwkey only when WEP is enabled */
	if (ioctl(s, SIOCG80211NWKEY, (caddr_t)&nwkey) != 0 ||
	    nwkey.i_wepon == 0) {
		printf("\n");
		return;
	}

	printf(" nwkey ");
	/* try to retrieve WEP keys */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		nwkey.i_key[i].i_keydat = keybuf[i];
		nwkey.i_key[i].i_keylen = sizeof(keybuf[i]);
	}
	if (ioctl(s, SIOCG80211NWKEY, (caddr_t)&nwkey) != 0) {
		printf("*****");
	} else {
		if (nwkey.i_defkid != 1) {
			/* non default key or multiple keys defined */
			i = 0;
		} else if (nwkey.i_key[0].i_keylen >= 2 &&
		    isdigit(nwkey.i_key[0].i_keydat[0]) &&
		    nwkey.i_key[0].i_keydat[1] == ':') {
			/* ambiguous */
			i = 0;
		} else {
			for (i = 1; i < IEEE80211_WEP_NKID; i++) {
				if (nwkey.i_key[i].i_keylen != 0)
					break;
			}
		}
		if (i == IEEE80211_WEP_NKID) {
			/* only show the first key */
			print_string(nwkey.i_key[0].i_keydat,
			    nwkey.i_key[0].i_keylen);
		} else {
			printf("%d:", nwkey.i_defkid);
			for (i = 0; i < IEEE80211_WEP_NKID; i++) {
				if (i > 0)
					printf(",");
				print_string(nwkey.i_key[i].i_keydat,
				    nwkey.i_key[i].i_keylen);
			}
		}
	}
	printf("\n");
}

void
init_current_media()
{
	struct ifmediareq ifmr;

	/*
	 * If we have not yet done so, grab the currently-selected
	 * media.
	 */
	if ((actions & (A_MEDIA|A_MEDIAOPT)) == 0) {
		(void) memset(&ifmr, 0, sizeof(ifmr));
		(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

		if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
			/*
			 * If we get E2BIG, the kernel is telling us
			 * that there are more, so we can ignore it.
			 */
			if (errno != E2BIG)
				err(1, "SGIOCGIFMEDIA");
		}

		media_current = ifmr.ifm_current;
	}

	/* Sanity. */
	if (IFM_TYPE(media_current) == 0)
		errx(1, "%s: no link type?", name);
}

void
process_media_commands()
{

	if ((actions & (A_MEDIA|A_MEDIAOPT)) == 0) {
		/* Nothing to do. */
		return;
	}

	/*
	 * Media already set up, and commands sanity-checked.  Set/clear
	 * any options, and we're ready to go.
	 */
	media_current |= mediaopt_set;
	media_current &= ~mediaopt_clear;

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = media_current;

	if (ioctl(s, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
		err(1, "SIOCSIFMEDIA");
}

void
setmedia(val, d)
	const char *val;
	int d;
{
	int type, subtype, inst;

	init_current_media();

	/* Only one media command may be given. */
	if (actions & A_MEDIA)
		errx(1, "only one `media' command may be issued");

	/* Must not come after mediaopt commands */
	if (actions & A_MEDIAOPT)
		errx(1, "may not issue `media' after `mediaopt' commands");

	/*
	 * No need to check if `instance' has been issued; setmediainst()
	 * craps out if `media' has not been specified.
	 */

	type = IFM_TYPE(media_current);
	inst = IFM_INST(media_current);

	/* Look up the subtype. */
	subtype = get_media_subtype(type, val);

	/* Build the new current media word. */
	media_current = IFM_MAKEWORD(type, subtype, 0, inst);

	/* Media will be set after other processing is complete. */
}

void
setmediaopt(val, d)
	const char *val;
	int d;
{

	init_current_media();

	/* Can only issue `mediaopt' once. */
	if (actions & A_MEDIAOPTSET)
		errx(1, "only one `mediaopt' command may be issued");

	/* Can't issue `mediaopt' if `instance' has already been issued. */
	if (actions & A_MEDIAINST)
		errx(1, "may not issue `mediaopt' after `instance'");

	mediaopt_set = get_media_options(IFM_TYPE(media_current), val);

	/* Media will be set after other processing is complete. */
}

void
unsetmediaopt(val, d)
	const char *val;
	int d;
{

	init_current_media();

	/* Can only issue `-mediaopt' once. */
	if (actions & A_MEDIAOPTCLR)
		errx(1, "only one `-mediaopt' command may be issued");

	/* May not issue `media' and `-mediaopt'. */
	if (actions & A_MEDIA)
		errx(1, "may not issue both `media' and `-mediaopt'");

	/*
	 * No need to check for A_MEDIAINST, since the test for A_MEDIA
	 * implicitly checks for A_MEDIAINST.
	 */

	mediaopt_clear = get_media_options(IFM_TYPE(media_current), val);

	/* Media will be set after other processing is complete. */
}

void
setmediainst(val, d)
	const char *val;
	int d;
{
	int type, subtype, options, inst;

	init_current_media();

	/* Can only issue `instance' once. */
	if (actions & A_MEDIAINST)
		errx(1, "only one `instance' command may be issued");

	/* Must have already specified `media' */
	if ((actions & A_MEDIA) == 0)
		errx(1, "must specify `media' before `instance'");

	type = IFM_TYPE(media_current);
	subtype = IFM_SUBTYPE(media_current);
	options = IFM_OPTIONS(media_current);

	inst = atoi(val);
	if (inst < 0 || inst > IFM_INST_MAX)
		errx(1, "invalid media instance: %s", val);

	media_current = IFM_MAKEWORD(type, subtype, options, inst);

	/* Media will be set after other processing is complete. */
}

struct ifmedia_description ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_descriptions[] =
    IFM_SUBTYPE_DESCRIPTIONS;

struct ifmedia_description ifm_option_descriptions[] =
    IFM_OPTION_DESCRIPTIONS;

const char *
get_media_type_string(mword)
	int mword;
{
	struct ifmedia_description *desc;

	for (desc = ifm_type_descriptions; desc->ifmt_string != NULL;
	     desc++) {
		if (IFM_TYPE(mword) == desc->ifmt_word)
			return (desc->ifmt_string);
	}
	return ("<unknown type>");
}

const char *
get_media_subtype_string(mword)
	int mword;
{
	struct ifmedia_description *desc;

	for (desc = ifm_subtype_descriptions; desc->ifmt_string != NULL;
	     desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, mword) &&
		    IFM_SUBTYPE(desc->ifmt_word) == IFM_SUBTYPE(mword))
			return (desc->ifmt_string);
	}
	return ("<unknown subtype>");
}

int
get_media_subtype(type, val)
	int type;
	const char *val;
{
	int rval;

	rval = lookup_media_word(ifm_subtype_descriptions, type, val);
	if (rval == -1)
		errx(1, "unknown %s media subtype: %s",
		    get_media_type_string(type), val);

	return (rval);
}

int
get_media_options(type, val)
	int type;
	const char *val;
{
	char *optlist, *str;
	int option, rval = 0;

	/* We muck with the string, so copy it. */
	optlist = strdup(val);
	if (optlist == NULL)
		err(1, "strdup");
	str = optlist;

	/*
	 * Look up the options in the user-provided comma-separated list.
	 */
	for (; (str = strtok(str, ",")) != NULL; str = NULL) {
		option = lookup_media_word(ifm_option_descriptions, type, str);
		if (option == -1)
			errx(1, "unknown %s media option: %s",
			    get_media_type_string(type), str);
		rval |= IFM_OPTIONS(option);
	}

	free(optlist);
	return (rval);
}

int
lookup_media_word(desc, type, val)
	struct ifmedia_description *desc;
	int type;
	const char *val;
{

	for (; desc->ifmt_string != NULL; desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, type) &&
		    strcasecmp(desc->ifmt_string, val) == 0)
			return (desc->ifmt_word);
	}
	return (-1);
}

void
print_media_word(ifmw, print_type, as_syntax)
	int ifmw, print_type, as_syntax;
{
	struct ifmedia_description *desc;
	int seen_option = 0;

	if (print_type)
		printf("%s ", get_media_type_string(ifmw));
	printf("%s%s", as_syntax ? "media " : "",
	    get_media_subtype_string(ifmw));

	/* Find options. */
	for (desc = ifm_option_descriptions; desc->ifmt_string != NULL;
	     desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, ifmw) &&
		    (ifmw & IFM_OPTIONS(desc->ifmt_word)) != 0 &&
		    (seen_option & IFM_OPTIONS(desc->ifmt_word)) == 0) {
			if (seen_option == 0)
				printf(" %s", as_syntax ? "mediaopt " : "");
			printf("%s%s", seen_option ? "," : "",
			    desc->ifmt_string);
			seen_option |= IFM_OPTIONS(desc->ifmt_word);
		}
	}
	if (IFM_INST(ifmw) != 0)
		printf(" instance %d", IFM_INST(ifmw));
}

int carrier()
{
	struct ifmediareq ifmr;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA;
		 * assume ok.
		 */
		return 0;
	}
	if ((ifmr.ifm_status & IFM_AVALID) == 0) {
		/*
		 * Interface doesn't report media-valid status.
		 * assume ok.
		 */
		return 0;
	}
	/* otherwise, return ok for active, not-ok if not active. */
	return !(ifmr.ifm_status & IFM_ACTIVE);
}


#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

const int ifm_status_valid_list[] = IFM_STATUS_VALID_LIST;

const struct ifmedia_status_description ifm_status_descriptions[] =
    IFM_STATUS_DESCRIPTIONS;

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status(ap, alen)
	const u_int8_t *ap;
	int alen;
{
	struct afswtch *p = afp;
	struct ifmediareq ifmr;
	int *media_list, i;

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (metric)
		printf(" metric %d", metric);
	if (mtu)
		printf(" mtu %d", mtu);
	putchar('\n');

	ieee80211_status();
	vlan_status();
	tunnel_status();

	if (ap && alen > 0) {
		printf("\taddress:");
		for (i = 0; i < alen; i++, ap++)
			printf("%c%02x", i > 0 ? ':' : ' ', *ap);
		putchar('\n');
	}

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA.
		 */
		goto proto_status;
	}

	if (ifmr.ifm_count == 0) {
		warnx("%s: no media types?", name);
		goto proto_status;
	}

	media_list = (int *)malloc(ifmr.ifm_count * sizeof(int));
	if (media_list == NULL)
		err(1, "malloc");
	ifmr.ifm_ulist = media_list;

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	printf("\tmedia: ");
	print_media_word(ifmr.ifm_current, 1, 0);
	if (ifmr.ifm_active != ifmr.ifm_current) {
		putchar(' ');
		putchar('(');
		print_media_word(ifmr.ifm_active, 0, 0);
		putchar(')');
	}
	putchar('\n');

	if (ifmr.ifm_status & IFM_STATUS_VALID) {
		const struct ifmedia_status_description *ifms;
		int bitno, found = 0;

		printf("\tstatus: ");
		for (bitno = 0; ifm_status_valid_list[bitno] != 0; bitno++) {
			for (ifms = ifm_status_descriptions;
			     ifms->ifms_valid != 0; ifms++) {
				if (ifms->ifms_type !=
				      IFM_TYPE(ifmr.ifm_current) ||
				    ifms->ifms_valid !=
				      ifm_status_valid_list[bitno])
					continue;
				printf("%s%s", found ? ", " : "",
				    IFM_STATUS_DESC(ifms, ifmr.ifm_status));
				found = 1;

				/*
				 * For each valid indicator bit, there's
				 * only one entry for each media type, so
				 * terminate the inner loop now.
				 */
				break;
			}
		}

		if (found == 0)
			printf("unknown");
		putchar('\n');
	}

	if (mflag) {
		int type, printed_type;

		for (type = IFM_NMIN; type <= IFM_NMAX; type += IFM_NMIN) {
			for (i = 0, printed_type = 0; i < ifmr.ifm_count; i++) {
				if (IFM_TYPE(media_list[i]) == type) {
					if (printed_type == 0) {
					    printf("\tsupported %s media:\n",
					      get_media_type_string(type));
					    printed_type = 1;
					}
					printf("\t\t");
					print_media_word(media_list[i], 0, 1);
					printf("\n");
				}
			}
		}
	}

	free(media_list);

 proto_status:
	if ((p = afp) != NULL) {
		(*p->af_status)(1);
	} else for (p = afs; p->af_name; p++) {
		ifr.ifr_addr.sa_family = p->af_af;
		(*p->af_status)(0);
	}
}

void
tunnel_status()
{
	char psrcaddr[NI_MAXHOST];
	char pdstaddr[NI_MAXHOST];
	u_long srccmd, dstcmd;
	struct ifreq *ifrp;
	const char *ver = "";
#ifdef NI_WITHSCOPEID
	const int niflag = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflag = NI_NUMERICHOST;
#endif
#ifdef INET6
	struct in6_ifreq in6_ifr;
	int s6;
#endif /* INET6 */

	psrcaddr[0] = pdstaddr[0] = '\0';

#ifdef INET6
	memset(&in6_ifr, 0, sizeof(in6_ifr));
	strncpy(in6_ifr.ifr_name, name, IFNAMSIZ);
	s6 = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s6 < 0) {
		srccmd = SIOCGIFPSRCADDR;
		dstcmd = SIOCGIFPDSTADDR;
		ifrp = &ifr;
	} else {
		close(s6);
		srccmd = SIOCGIFPSRCADDR_IN6;
		dstcmd = SIOCGIFPDSTADDR_IN6;
		ifrp = (struct ifreq *)&in6_ifr;
	}
#else /* INET6 */
	srccmd = SIOCGIFPSRCADDR;
	dstcmd = SIOCGIFPDSTADDR;
	ifrp = &ifr;
#endif /* INET6 */

	if (ioctl(s, srccmd, (caddr_t)ifrp) < 0)
		return;
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&ifrp->ifr_addr);
#endif
	getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
	    psrcaddr, sizeof(psrcaddr), 0, 0, niflag);
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		ver = "6";
#endif

	if (ioctl(s, dstcmd, (caddr_t)ifrp) < 0)
		return;
#ifdef INET6
	if (ifrp->ifr_addr.sa_family == AF_INET6)
		in6_fillscopeid((struct sockaddr_in6 *)&ifrp->ifr_addr);
#endif
	getnameinfo(&ifrp->ifr_addr, ifrp->ifr_addr.sa_len,
	    pdstaddr, sizeof(pdstaddr), 0, 0, niflag);

	printf("\ttunnel inet%s %s --> %s\n", ver,
	    psrcaddr, pdstaddr);
}

void
vlan_status()
{
	struct vlanreq vlr;

	if (strncmp(ifr.ifr_name, "vlan", 4) != 0 ||
	    !isdigit(ifr.ifr_name[4]))
		return;

	memset(&vlr, 0, sizeof(vlr));
	ifr.ifr_data = (caddr_t)&vlr;

	if (ioctl(s, SIOCGETVLAN, (caddr_t)&ifr) == -1)
		return;

	if (vlr.vlr_tag || vlr.vlr_parent[0] != '\0')
		printf("\tvlan: %d parent: %s\n",
		    vlr.vlr_tag, vlr.vlr_parent[0] == '\0' ?
		    "<none>" : vlr.vlr_parent);
}

void
in_alias(creq)
	struct ifreq *creq;
{
	struct sockaddr_in *sin;
	int alias;

	if (lflag)
		return;

	alias = 1;

	/* Get the non-alias address for this interface. */
	getsock(AF_INET);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) memset(&ifr, 0, sizeof(ifr));
	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			return;
		} else
			warn("SIOCGIFADDR");
	}
	/* If creq and ifr are the same address, this is not an alias. */
	if (memcmp(&ifr.ifr_addr, &creq->ifr_addr,
		   sizeof(creq->ifr_addr)) == 0)
		alias = 0;
	(void) memset(&addreq, 0, sizeof(addreq));
	(void) strncpy(addreq.ifra_name, name, sizeof(addreq.ifra_name));
	addreq.ifra_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFALIAS, (caddr_t)&addreq) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			return;
		} else
			warn("SIOCGIFALIAS");
	}

	sin = (struct sockaddr_in *)&addreq.ifra_addr;
	printf("\tinet %s%s", alias ? "alias " : "", inet_ntoa(sin->sin_addr));

	if (flags & IFF_POINTOPOINT) {
		sin = (struct sockaddr_in *)&addreq.ifra_dstaddr;
		printf(" -> %s", inet_ntoa(sin->sin_addr));
	}

	sin = (struct sockaddr_in *)&addreq.ifra_mask;
	printf(" netmask 0x%x", ntohl(sin->sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		sin = (struct sockaddr_in *)&addreq.ifra_broadaddr;
		printf(" broadcast %s", inet_ntoa(sin->sin_addr));
	}
	printf("\n");
}

void
in_status(force)
	int force;
{
#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifap, *ifa;
	struct ifreq ifr;

	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(name, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (sizeof(ifr.ifr_addr) < ifa->ifa_addr->sa_len)
			continue;

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		in_alias(&ifr);
	}
	freeifaddrs(ifap);
#else
	char inbuf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	int i, siz;
	char ifrbuf[8192], *cp;

	ifc.ifc_len = sizeof(inbuf);
	ifc.ifc_buf = inbuf;
	getsock(af);
	if (s < 0)
		err(1, "socket");
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
		err(1, "SIOCGIFCONF");
	for (i = 0; i < ifc.ifc_len; ) {
		/* Copy the mininum ifreq into the buffer. */ 
		cp = ((caddr_t)ifc.ifc_req + i);
		memcpy(ifrbuf, cp, sizeof(*ifr));

		/* Now compute the actual size of the ifreq. */
		ifr = (struct ifreq *)ifrbuf;
		siz = ifr->ifr_addr.sa_len;
		if (siz < sizeof(ifr->ifr_addr))
			siz = sizeof(ifr->ifr_addr);
		siz += sizeof(ifr->ifr_name);
		i += siz;

		/* Now copy the whole thing. */
		if (sizeof(ifrbuf) < siz)
			errx(1, "ifr too big");
		memcpy(ifrbuf, cp, siz);

		if (!strncmp(name, ifr->ifr_name, sizeof(ifr->ifr_name))) {
			if (ifr->ifr_addr.sa_family == AF_INET)
				in_alias(ifr);
		}
	}
#endif
}

void
setifprefixlen(addr, d)
	const char *addr;
	int d;
{
	if (*afp->af_getprefix)
		(*afp->af_getprefix)(addr, MASK);
	explicit_prefix = 1;
}

#ifdef INET6
void
in6_fillscopeid(sin6)
	struct sockaddr_in6 *sin6;
{
#if defined(__KAME__) && defined(KAME_SCOPEID)
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		sin6->sin6_scope_id =
			ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
		sin6->sin6_addr.s6_addr[2] = sin6->sin6_addr.s6_addr[3] = 0;
	}
#endif
}

/* XXX not really an alias */
void
in6_alias(creq)
	struct in6_ifreq *creq;
{
	struct sockaddr_in6 *sin6;
	char hbuf[NI_MAXHOST];
	u_int32_t scopeid;
#ifdef NI_WITHSCOPEID
	const int niflag = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflag = NI_NUMERICHOST;
#endif

	/* Get the non-alias address for this interface. */
	getsock(AF_INET6);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}

	sin6 = (struct sockaddr_in6 *)&creq->ifr_addr;

	in6_fillscopeid(sin6);
	scopeid = sin6->sin6_scope_id;
	if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
			hbuf, sizeof(hbuf), NULL, 0, niflag))
		strncpy(hbuf, "", sizeof(hbuf));	/* some message? */
	printf("\tinet6 %s", hbuf);

	if (flags & IFF_POINTOPOINT) {
		(void) memset(&ifr6, 0, sizeof(ifr6));
		(void) strncpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		if (ioctl(s, SIOCGIFDSTADDR_IN6, (caddr_t)&ifr6) < 0) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFDSTADDR_IN6");
			(void) memset(&ifr6.ifr_addr, 0, sizeof(ifr6.ifr_addr));
			ifr6.ifr_addr.sin6_family = AF_INET6;
			ifr6.ifr_addr.sin6_len = sizeof(struct sockaddr_in6);
		}
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		in6_fillscopeid(sin6);
		hbuf[0] = '\0';
		if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag))
			strncpy(hbuf, "", sizeof(hbuf)); /* some message? */
		printf(" -> %s", hbuf);
	}

	(void) memset(&ifr6, 0, sizeof(ifr6));
	(void) strncpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFNETMASK_IN6, (caddr_t)&ifr6) < 0) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFNETMASK_IN6");
	} else {
		sin6 = (struct sockaddr_in6 *)&ifr6.ifr_addr;
		printf(" prefixlen %d", prefix(&sin6->sin6_addr,
					       sizeof(struct in6_addr)));
	}

	(void) memset(&ifr6, 0, sizeof(ifr6));
	(void) strncpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = creq->ifr_addr;
	if (ioctl(s, SIOCGIFAFLAG_IN6, (caddr_t)&ifr6) < 0) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFAFLAG_IN6");
	} else {
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST)
			printf(" anycast");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE)
			printf(" tentative");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED)
			printf(" duplicated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED)
			printf(" detached");
	}

	if (scopeid)
		printf(" scopeid 0x%x", scopeid);

	if (Lflag) {
		struct in6_addrlifetime *lifetime;
		(void) memset(&ifr6, 0, sizeof(ifr6));
		(void) strncpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = creq->ifr_addr;
		lifetime = &ifr6.ifr_ifru.ifru_lifetime;
		if (ioctl(s, SIOCGIFALIFETIME_IN6, (caddr_t)&ifr6) < 0) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFALIFETIME_IN6");
		} else if (lifetime->ia6t_preferred || lifetime->ia6t_expire) {
			time_t t = time(NULL);
			printf(" pltime ");
			if (lifetime->ia6t_preferred) {
				printf("%s", lifetime->ia6t_preferred < t
					? "0"
					: sec2str(lifetime->ia6t_preferred - t));
			} else
				printf("infty");

			printf(" vltime ");
			if (lifetime->ia6t_expire) {
				printf("%s", lifetime->ia6t_expire < t
					? "0"
					: sec2str(lifetime->ia6t_expire - t));
			} else
				printf("infty");
		}
	}

	printf("\n");
}

void
in6_status(force)
	int force;
{
#ifdef HAVE_IFADDRS_H
	struct ifaddrs *ifap, *ifa;
	struct in6_ifreq ifr;

	if (getifaddrs(&ifap) != 0)
		err(1, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(name, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (sizeof(ifr.ifr_addr) < ifa->ifa_addr->sa_len)
			continue;

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		in6_alias(&ifr);
	}
	freeifaddrs(ifap);
#else
	char inbuf[8192];
	struct ifconf ifc;
	struct ifreq *ifr;
	int i, siz;
	char ifrbuf[8192], *cp;

	ifc.ifc_len = sizeof(inbuf);
	ifc.ifc_buf = inbuf;
	getsock(af);
	if (s < 0)
		err(1, "socket");
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
		err(1, "SIOCGIFCONF");
	for (i = 0; i < ifc.ifc_len; ) {
		/* Copy the mininum ifreq into the buffer. */ 
		cp = ((caddr_t)ifc.ifc_req + i);
		memcpy(ifrbuf, cp, sizeof(*ifr));

		/* Now compute the actual size of the ifreq. */
		ifr = (struct ifreq *)ifrbuf;
		siz = ifr->ifr_addr.sa_len;
		if (siz < sizeof(ifr->ifr_addr))
			siz = sizeof(ifr->ifr_addr);
		siz += sizeof(ifr->ifr_name);
		i += siz;

		/* Now copy the whole thing. */
		if (sizeof(ifrbuf) < siz)
			errx(1, "ifr too big");
		memcpy(ifrbuf, cp, siz);

		if (!strncmp(name, ifr->ifr_name, sizeof(ifr->ifr_name))) {
			if (ifr->ifr_addr.sa_family == AF_INET6)
				in6_alias((struct in6_ifreq *)ifr);
		}
	}
#endif
}
#endif /*INET6*/

#ifndef INET_ONLY

void
at_status(force)
	int force;
{
	struct sockaddr_at *sat, null_sat;
	struct netrange *nr;

	getsock(AF_APPLETALK);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) memset(&ifr, 0, sizeof(ifr));
	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			(void) memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
		} else
			warn("SIOCGIFADDR");
	}
	(void) strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	sat = (struct sockaddr_at *)&ifr.ifr_addr;

	(void) memset(&null_sat, 0, sizeof(null_sat));

	nr = (struct netrange *) &sat->sat_zero;
	printf("\tatalk %d.%d range %d-%d phase %d",
	    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	    ntohs(nr->nr_firstnet), ntohs(nr->nr_lastnet), nr->nr_phase);
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    (void) memset(&ifr.ifr_addr, 0,
				sizeof(ifr.ifr_addr));
			else
			    warn("SIOCGIFDSTADDR");
		}
		(void) strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sat = (struct sockaddr_at *)&ifr.ifr_dstaddr;
		if (!sat)
			sat = &null_sat;
		printf("--> %d.%d",
		    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
	}
	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sat = (struct sockaddr_at *)&ifr.ifr_broadaddr;
		if (sat)
			printf(" broadcast %d.%d", ntohs(sat->sat_addr.s_net),
			    sat->sat_addr.s_node);
	}
	putchar('\n');
}

void
xns_status(force)
	int force;
{
	struct sockaddr_ns *sns;

	getsock(AF_NS);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) memset(&ifr, 0, sizeof(ifr));
	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
		} else
			warn("SIOCGIFADDR");
	}
	(void) strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	sns = (struct sockaddr_ns *)&ifr.ifr_addr;
	printf("\tns %s ", ns_ntoa(sns->sns_addr));
	if (flags & IFF_POINTOPOINT) { /* by W. Nesheim@Cornell */
		if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
			else
			    warn("SIOCGIFDSTADDR");
		}
		(void) strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		sns = (struct sockaddr_ns *)&ifr.ifr_dstaddr;
		printf("--> %s ", ns_ntoa(sns->sns_addr));
	}
	putchar('\n');
}

void
iso_status(force)
	int force;
{
	struct sockaddr_iso *siso;
	struct iso_ifreq ifr;

	getsock(AF_ISO);
	if (s < 0) {
		if (errno == EPROTONOSUPPORT)
			return;
		err(1, "socket");
	}
	(void) memset(&ifr, 0, sizeof(ifr));
	(void) strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFADDR_ISO, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			if (!force)
				return;
			(void) memset(&ifr.ifr_Addr, 0, sizeof(ifr.ifr_Addr));
		} else
			warn("SIOCGIFADDR_ISO");
	}
	(void) strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	siso = &ifr.ifr_Addr;
	printf("\tiso %s ", iso_ntoa(&siso->siso_addr));
	if (ioctl(s, SIOCGIFNETMASK_ISO, (caddr_t)&ifr) < 0) {
		if (errno == EADDRNOTAVAIL)
			memset(&ifr.ifr_Addr, 0, sizeof(ifr.ifr_Addr));
		else
			warn("SIOCGIFNETMASK_ISO");
	} else {
		if (siso->siso_len > offsetof(struct sockaddr_iso, siso_addr))
			siso->siso_addr.isoa_len = siso->siso_len
			    - offsetof(struct sockaddr_iso, siso_addr);
		printf("\n\t\tnetmask %s ", iso_ntoa(&siso->siso_addr));
	}
	if (flags & IFF_POINTOPOINT) {
		if (ioctl(s, SIOCGIFDSTADDR_ISO, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL)
			    memset(&ifr.ifr_Addr, 0, sizeof(ifr.ifr_Addr));
			else
			    warn("SIOCGIFDSTADDR_ISO");
		}
		(void) strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
		siso = &ifr.ifr_Addr;
		printf("--> %s ", iso_ntoa(&siso->siso_addr));
	}
	putchar('\n');
}

#endif	/* INET_ONLY */

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifr_addr), SIN(addreq.ifra_addr),
SIN(addreq.ifra_mask), SIN(addreq.ifra_broadaddr)};

void
in_getaddr(s, which)
	const char *s;
	int which;
{
	struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;

	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;

	if (which == ADDR) {
		char *p = NULL;
	    
		if((p = strrchr(s, '/')) != NULL) {
			/* address is `name/masklen' */
			int masklen;
			int ret;
			struct sockaddr_in *min = sintab[MASK];
			*p = '\0';
			ret = sscanf(p+1, "%u", &masklen);
			if(ret != 1 || (masklen < 0 || masklen > 32)) {
				*p = '/';
				errx(1, "%s: bad value", s);
			}
			min->sin_len = sizeof(*min);
			min->sin_addr.s_addr = 
				htonl(~((1LL << (32 - masklen)) - 1) & 
				      0xffffffff);
		}
	}

	if (inet_aton(s, &sin->sin_addr) == 0) {
		if ((hp = gethostbyname(s)) != NULL)
			(void) memcpy(&sin->sin_addr, hp->h_addr, hp->h_length);
		else if ((np = getnetbyname(s)) != NULL)
			sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
		else
			errx(1, "%s: bad value", s);
	}
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(s, v, bits)
	const char *s;
	unsigned short v;
	const char *bits;
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++) != 0) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

#ifdef INET6
#define SIN6(x) ((struct sockaddr_in6 *) &(x))
struct sockaddr_in6 *sin6tab[] = {
SIN6(in6_ridreq.ifr_addr), SIN6(in6_addreq.ifra_addr),
SIN6(in6_addreq.ifra_prefixmask), SIN6(in6_addreq.ifra_dstaddr)};

void
in6_getaddr(s, which)
	const char *s;
	int which;
{
#if defined(__KAME__) && defined(KAME_SCOPEID)
	struct sockaddr_in6 *sin6 = sin6tab[which];
	struct addrinfo hints, *res;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
#if 0 /* in_getaddr() allows FQDN */
	hints.ai_flags = AI_NUMERICHOST;
#endif
	error = getaddrinfo(s, "0", &hints, &res);
	if (error)
		errx(1, "%s: %s", s, gai_strerror(error));
	if (res->ai_next)
		errx(1, "%s: resolved to multiple hosts", s);
	if (res->ai_addrlen != sizeof(struct sockaddr_in6))
		errx(1, "%s: bad value", s);
	memcpy(sin6, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) && sin6->sin6_scope_id) {
		*(u_int16_t *)&sin6->sin6_addr.s6_addr[2] =
			htons(sin6->sin6_scope_id);
		sin6->sin6_scope_id = 0;
	}
#else
	struct sockaddr_in6 *sin = sin6tab[which];

	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;

	if (which == ADDR) {
		char *p = NULL;
		if((p = strrchr(s, '/')) != NULL) {
			*p = '\0';
			in6_getprefix(p + 1, MASK);
			explicit_prefix = 1;
		}
	}

	if (inet_pton(AF_INET6, s, &sin->sin6_addr) != 1)
		errx(1, "%s: bad value", s);
#endif
}

void
in6_getprefix(plen, which)
	const char *plen;
	int which;
{
	register struct sockaddr_in6 *sin = sin6tab[which];
	register u_char *cp;
	int len = strtol(plen, (char **)NULL, 10);

	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);
	sin->sin6_len = sizeof(*sin);
	if (which != MASK)
		sin->sin6_family = AF_INET6;
	if ((len == 0) || (len == 128)) {
		memset(&sin->sin6_addr, 0xff, sizeof(struct in6_addr));
		return;
	}
	memset((void *)&sin->sin6_addr, 0x00, sizeof(sin->sin6_addr));
	for (cp = (u_char *)&sin->sin6_addr; len > 7; len -= 8)
		*cp++ = 0xff;
	*cp = 0xff << (8 - len);
}

int
prefix(val, size)
	void *val;
	int size;
{
	register u_char *name = (u_char *)val;
	register int byte, bit, plen = 0;

	for (byte = 0; byte < size; byte++, plen += 8)
		if (name[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(name[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (name[byte] & (1 << bit))
			return(0);
	byte++;
	for (; byte < size; byte++)
		if (name[byte])
			return(0);
	return (plen);
}
#endif /*INET6*/

#ifndef INET_ONLY
void
at_getaddr(addr, which)
	const char *addr;
	int which;
{
	struct sockaddr_at *sat = (struct sockaddr_at *) &addreq.ifra_addr;
	u_int net, node;

	sat->sat_family = AF_APPLETALK;
	sat->sat_len = sizeof(*sat);
	if (which == MASK)
		errx(1, "AppleTalk does not use netmasks\n");
	if (sscanf(addr, "%u.%u", &net, &node) != 2
	    || net == 0 || net > 0xffff || node == 0 || node > 0xfe)
		errx(1, "%s: illegal address", addr);
	sat->sat_addr.s_net = htons(net);
	sat->sat_addr.s_node = node;
}

void
setatrange(range, d)
	const char *range;
	int d;
{
	u_short	first = 123, last = 123;

	if (sscanf(range, "%hu-%hu", &first, &last) != 2
	    || first == 0 || first > 0xffff
	    || last == 0 || last > 0xffff || first > last)
		errx(1, "%s: illegal net range: %u-%u", range, first, last);
	at_nr.nr_firstnet = htons(first);
	at_nr.nr_lastnet = htons(last);
}

void
setatphase(phase, d)
	const char *phase;
	int d;
{
	if (!strcmp(phase, "1"))
		at_nr.nr_phase = 1;
	else if (!strcmp(phase, "2"))
		at_nr.nr_phase = 2;
	else
		errx(1, "%s: illegal phase", phase);
}

void
checkatrange(sat)
	struct sockaddr_at *sat;
{
	if (at_nr.nr_phase == 0)
		at_nr.nr_phase = 2;	/* Default phase 2 */
	if (at_nr.nr_firstnet == 0)
		at_nr.nr_firstnet =	/* Default range of one */
		at_nr.nr_lastnet = sat->sat_addr.s_net;
	printf("\tatalk %d.%d range %d-%d phase %d\n",
	ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node,
	ntohs(at_nr.nr_firstnet), ntohs(at_nr.nr_lastnet), at_nr.nr_phase);
	if ((u_short) ntohs(at_nr.nr_firstnet) >
			(u_short) ntohs(sat->sat_addr.s_net)
		    || (u_short) ntohs(at_nr.nr_lastnet) <
			(u_short) ntohs(sat->sat_addr.s_net))
		errx(1, "AppleTalk address is not in range");
	*((struct netrange *) &sat->sat_zero) = at_nr;
}

#define SNS(x) ((struct sockaddr_ns *) &(x))
struct sockaddr_ns *snstab[] = {
SNS(ridreq.ifr_addr), SNS(addreq.ifra_addr),
SNS(addreq.ifra_mask), SNS(addreq.ifra_broadaddr)};

void
xns_getaddr(addr, which)
	const char *addr;
	int which;
{
	struct sockaddr_ns *sns = snstab[which];

	sns->sns_family = AF_NS;
	sns->sns_len = sizeof(*sns);
	sns->sns_addr = ns_addr(addr);
	if (which == MASK)
		puts("Attempt to set XNS netmask will be ineffectual");
}

#define SISO(x) ((struct sockaddr_iso *) &(x))
struct sockaddr_iso *sisotab[] = {
SISO(iso_ridreq.ifr_Addr), SISO(iso_addreq.ifra_addr),
SISO(iso_addreq.ifra_mask), SISO(iso_addreq.ifra_dstaddr)};

void
iso_getaddr(addr, which)
	const char *addr;
	int which;
{
	struct sockaddr_iso *siso = sisotab[which];
	siso->siso_addr = *iso_addr(addr);

	if (which == MASK) {
		siso->siso_len = TSEL(siso) - (caddr_t)(siso);
		siso->siso_nlen = 0;
	} else {
		siso->siso_len = sizeof(*siso);
		siso->siso_family = AF_ISO;
	}
}

void
setsnpaoffset(val, d)
	const char *val;
	int d;
{
	iso_addreq.ifra_snpaoffset = atoi(val);
}

void
setnsellength(val, d)
	const char *val;
	int d;
{
	nsellength = atoi(val);
	if (nsellength < 0)
		errx(1, "Negative NSEL length is absurd");
	if (afp == 0 || afp->af_af != AF_ISO)
		errx(1, "Setting NSEL length valid only for iso");
}

void
fixnsel(s)
	struct sockaddr_iso *s;
{
	if (s->siso_family == 0)
		return;
	s->siso_tlen = nsellength;
}

void
adjust_nsellength()
{
	fixnsel(sisotab[RIDADDR]);
	fixnsel(sisotab[ADDR]);
	fixnsel(sisotab[DSTADDR]);
}

#endif	/* INET_ONLY */

void
usage()
{
	extern const char *__progname;

	fprintf(stderr,
	    "usage: %s [ -m ] "
#ifdef INET6
		"[ -L ] "
#endif
		"interface\n"
		"\t[ af [ address [ dest_addr ] ] [ up ] [ down ] "
		"[ netmask mask ] ]\n"
		"\t[ metric n ]\n"
		"\t[ mtu n ]\n"
		"\t[ nwid network_id ]\n"
		"\t[ nwkey network_key | -nwkey ]\n"
		"\t[ [ af ] tunnel src_addr dest_addr ]\n"
		"\t[ deletetunnel ]\n"
		"\t[ arp | -arp ]\n"
		"\t[ media mtype ]\n"
		"\t[ mediaopt mopts ]\n"
		"\t[ -mediaopt mopts ]\n"
		"\t[ instance minst ]\n"
		"\t[ vlan n vlanif i ]\n"
		"\t[ link0 | -link0 ] [ link1 | -link1 ] [ link2 | -link2 ]\n"
		"       %s -a [ -m ] [ -d ] [ -u ] [ af ]\n"
		"       %s -l [ -d ] [ -u ]\n"
		"       %s interface create\n"
		"       %s interface destroy\n",
		__progname, __progname, __progname, __progname, __progname);
	exit(1);
}

#ifdef INET6
char *
sec2str(total)
	time_t total;
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	if (0) {	/*XXX*/
		days = total / 3600 / 24;
		hours = (total / 3600) % 24;
		mins = (total / 60) % 60;
		secs = total % 60;

		if (days) {
			first = 0;
			p += sprintf(p, "%dd", days);
		}
		if (!first || hours) {
			first = 0;
			p += sprintf(p, "%dh", hours);
		}
		if (!first || mins) {
			first = 0;
			p += sprintf(p, "%dm", mins);
		}
		sprintf(p, "%ds", secs);
	} else
		sprintf(p, "%lu", (u_long)total);

	return(result);
}
#endif
