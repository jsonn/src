/*	$NetBSD: parse.c,v 1.1.1.1.4.1 1999/12/27 18:27:54 wrstuden Exp $	*/

/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include <syslog.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ipf.h"
#include "facpri.h"

#if !defined(lint)
static const char sccsid[] = "@(#)parse.c	1.44 6/5/96 (C) 1993-1996 Darren Reed";
static const char rcsid[] = "@(#)Id: parse.c,v 2.1.2.4 1999/12/11 05:30:26 darrenr Exp";
#endif

extern	struct	ipopt_names	ionames[], secclass[];
extern	int	opts;

int	portnum __P((char *, u_short *, int));
u_char	tcp_flags __P((char *, u_char *, int));
int	addicmp __P((char ***, struct frentry *, int));
int	extras __P((char ***, struct frentry *, int));
char    ***seg;
u_long  *sa, *msk;
u_short *pp, *tp;
u_char  *cp;

int	hostmask __P((char ***, u_32_t *, u_32_t *, u_short *, u_char *,
		      u_short *, int));
int	ports __P((char ***, u_short *, u_char *, u_short *, int));
int	icmpcode __P((char *)), addkeep __P((char ***, struct frentry *, int));
int	to_interface __P((frdest_t *, char *, int));
void	print_toif __P((char *, frdest_t *));
void	optprint __P((u_short *, u_long, u_long));
int	countbits __P((u_32_t));
char	*portname __P((int, int));
int	ratoi __P((char *, int *, int, int));


char	*proto = NULL;
char	flagset[] = "FSRPAU";
u_char	flags[] = { TH_FIN, TH_SYN, TH_RST, TH_PUSH, TH_ACK, TH_URG };

static	char	thishost[MAXHOSTNAMELEN];


void initparse()
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}


/* parse()
 *
 * parse a line read from the input filter rule file
 */
struct	frentry	*parse(line, linenum)
char	*line;
int     linenum;
{
	static	struct	frentry	fil;
	struct	protoent	*p = NULL;
	char	*cps[31], **cpp, *endptr;
	u_char	ch;
	int	i, cnt = 1, j;

	while (*line && isspace(*line))
		line++;
	if (!*line)
		return NULL;

	bzero((char *)&fil, sizeof(fil));
	fil.fr_mip.fi_v = 0xf;
	fil.fr_ip.fi_v = 4;
	fil.fr_loglevel = 0xffff;

	/*
	 * break line up into max of 20 segments
	 */
	if (opts & OPT_DEBUG)
		fprintf(stderr, "parse [%s]\n", line);
	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");
	cps[i] = NULL;

	if (cnt < 3) {
		fprintf(stderr, "%d: not enough segments in line\n", linenum);
		return NULL;
	}

	cpp = cps;
	if (**cpp == '@')
		fil.fr_hits = (U_QUAD_T)atoi(*cpp++ + 1) + 1;


	if (!strcasecmp("block", *cpp)) {
		fil.fr_flags |= FR_BLOCK;
		if (!strncasecmp(*(cpp+1), "return-icmp-as-dest", 19))
			fil.fr_flags |= FR_FAKEICMP;
		else if (!strncasecmp(*(cpp+1), "return-icmp", 11))
			fil.fr_flags |= FR_RETICMP;
		if (fil.fr_flags & FR_RETICMP) {
			cpp++;
			i = 11;
			if ((strlen(*cpp) > i) && (*(*cpp + i) != '('))
				i = 19;
			if (*(*cpp + i) == '(') {
				i++;
				j = icmpcode(*cpp + i);
				if (j == -1) {
					fprintf(stderr,
					"%d: unrecognised icmp code %s\n",
						linenum, *cpp + 20);
					return NULL;
				}
				fil.fr_icode = j;
			}
		} else if (!strncasecmp(*(cpp+1), "return-rst", 10)) {
			fil.fr_flags |= FR_RETRST;
			cpp++;
		}
	} else if (!strcasecmp("count", *cpp)) {
		fil.fr_flags |= FR_ACCOUNT;
	} else if (!strcasecmp("pass", *cpp)) {
		fil.fr_flags |= FR_PASS;
	} else if (!strcasecmp("auth", *cpp)) {
		 fil.fr_flags |= FR_AUTH;
	} else if (!strcasecmp("preauth", *cpp)) {
		 fil.fr_flags |= FR_PREAUTH;
	} else if (!strcasecmp("skip", *cpp)) {
		cpp++;
		if (ratoi(*cpp, &i, 0, USHRT_MAX))
			fil.fr_skip = i;
		else {
			fprintf(stderr, "%d: integer must follow skip\n",
				linenum);
			return NULL;
		}
	} else if (!strcasecmp("log", *cpp)) {
		fil.fr_flags |= FR_LOG;
		if (!strcasecmp(*(cpp+1), "body")) {
			fil.fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (!strcasecmp(*(cpp+1), "first")) {
			fil.fr_flags |= FR_LOGFIRST;
		}
		if (!strcasecmp(*(cpp+1), "level")) {
			int fac, pri;
			char *s;

			fac = 0;
			pri = 0;
			cpp++;
			s = index(*cpp, '.');
			if (s) {
				*s++ = '\0';
				fac = fac_findname(*cpp);
				if (fac == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown facility", *cpp);
					return NULL;
				}
				pri = pri_findname(s);
				if (pri == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown priority", s);
					return NULL;
				}
			} else {
				pri = pri_findname(*cpp);
				if (pri == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown priority", *cpp);
					return NULL;
				}
			}
			fil.fr_loglevel = fac|pri;
			cpp++;
		}
	} else {
		/*
		 * Doesn't start with one of the action words
		 */
		fprintf(stderr, "%d: unknown keyword (%s)\n", linenum, *cpp);
		return NULL;
	}
	cpp++;

	if (!strcasecmp("in", *cpp))
		fil.fr_flags |= FR_INQUE;
	else if (!strcasecmp("out", *cpp)) {
		fil.fr_flags |= FR_OUTQUE;
		if (fil.fr_flags & FR_RETICMP) {
			fprintf(stderr,
				"%d: Can only use return-icmp with 'in'\n",
				linenum);
			return NULL;
		} else if (fil.fr_flags & FR_RETRST) {
			fprintf(stderr,
				"%d: Can only use return-rst with 'in'\n", 
				linenum);
			return NULL;
		}
	} else {
		fprintf(stderr, "%d: missing 'in'/'out' keyword (%s)\n",
			linenum, *cpp);
		return NULL;
	}
	if (!*++cpp)
		return NULL;

	if (!strcasecmp("log", *cpp)) {
		cpp++;
		if (fil.fr_flags & FR_PASS)
			fil.fr_flags |= FR_LOGP;
		else if (fil.fr_flags & FR_BLOCK)
			fil.fr_flags |= FR_LOGB;
		if (!strcasecmp(*cpp, "body")) {
			fil.fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (!strcasecmp(*cpp, "first")) {
			fil.fr_flags |= FR_LOGFIRST;
			cpp++;
		}
		if (!strcasecmp(*cpp, "or-block")) {
			if (!(fil.fr_flags & FR_PASS)) {
				fprintf(stderr,
					"%d: or-block must be used with pass\n",
					linenum);
				return NULL;
			}
			fil.fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
		if (!strcasecmp(*cpp, "level")) {
			int fac, pri;
			char *s;

			fac = 0;
			pri = 0;
			cpp++;
			s = index(*cpp, '.');
			if (s) {
				*s++ = '\0';
				fac = fac_findname(*cpp);
				if (fac == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown facility", *cpp);
					return NULL;
				}
				pri = pri_findname(s);
				if (pri == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown priority", s);
					return NULL;
				}
			} else {
				pri = pri_findname(*cpp);
				if (pri == -1) {
					fprintf(stderr, "%d: %s %s\n", linenum,
						"Unknown priority", *cpp);
					return NULL;
				}
			}
			fil.fr_loglevel = fac|pri;
			cpp++;
		}
	}

	if (!strcasecmp("quick", *cpp)) {
		cpp++;
		fil.fr_flags |= FR_QUICK;
	}

	*fil.fr_ifname = '\0';
	if (*cpp && !strcasecmp(*cpp, "on")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: interface name missing\n",
				linenum);
			return NULL;
		}
		(void)strncpy(fil.fr_ifname, *cpp, IFNAMSIZ-1);
		fil.fr_ifname[IFNAMSIZ-1] = '\0';
		cpp++;
		if (!*cpp) {
			if ((fil.fr_flags & FR_RETMASK) == FR_RETRST) {
				fprintf(stderr,
					"%d: %s can only be used with TCP\n",
					linenum, "return-rst");
				return NULL;
			}
			return &fil;
		}

		if (*cpp) {
			if (!strcasecmp(*cpp, "dup-to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_dif, *cpp, linenum))
					return NULL;
				cpp++;
			}
			if (!strcasecmp(*cpp, "to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_tif, *cpp, linenum))
					return NULL;
				cpp++;
			} else if (!strcasecmp(*cpp, "fastroute")) {
				if (!(fil.fr_flags & FR_INQUE)) {
					fprintf(stderr,
						"can only use %s with 'in'\n",
						"fastroute");
					return NULL;
				}
				fil.fr_flags |= FR_FASTROUTE;
				cpp++;
			}
		}
	}
	if (*cpp && !strcasecmp(*cpp, "tos")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: tos missing value\n", linenum);
			return NULL;
		}
		fil.fr_tos = strtol(*cpp, NULL, 0);
		fil.fr_mip.fi_tos = 0xff;
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "ttl")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: ttl missing hopcount value\n",
				linenum);
			return NULL;
		}
		if (ratoi(*cpp, &i, 0, 255))
			fil.fr_ttl = i;
		else {
			fprintf(stderr, "%d: invalid ttl (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		fil.fr_mip.fi_ttl = 0xff;
		cpp++;
	}

	/*
	 * check for "proto <protoname>" only decode udp/tcp/icmp as protoname
	 */
	proto = NULL;
	if (*cpp && !strcasecmp(*cpp, "proto")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: protocol name missing\n", linenum);
			return NULL;
		}
		proto = *cpp++;
		if (!strcasecmp(proto, "tcp/udp")) {
			fil.fr_ip.fi_fl |= FI_TCPUDP;
			fil.fr_mip.fi_fl |= FI_TCPUDP;
		} else {
			if (!(p = getprotobyname(proto)) && !isdigit(*proto)) {
				fprintf(stderr,
					"%d: unknown protocol (%s)\n",
					linenum, proto);
				return NULL;
			}
			if (p)
				fil.fr_proto = p->p_proto;
			else if (isdigit(*proto)) {
				i = (int)strtol(proto, &endptr, 0);
				if (*endptr != '\0' || i < 0 || i > 255) {
					fprintf(stderr,
						"%d: unknown protocol (%s)\n",
						linenum, proto);
					return NULL;		
				}
				fil.fr_proto = i;
			}
			fil.fr_mip.fi_p = 0xff;
		}
	}
	if ((fil.fr_proto != IPPROTO_TCP) &&
	    ((fil.fr_flags & FR_RETMASK) == FR_RETRST)) {
		fprintf(stderr, "%d: %s can only be used with TCP\n",
			linenum, "return-rst");
		return NULL;
	}

	/*
	 * get the from host and bit mask to use against packets
	 */

	if (!*cpp) {
		fprintf(stderr, "%d: missing source specification\n", linenum);
		return NULL;
	}
	if (!strcasecmp(*cpp, "all")) {
		cpp++;
		if (!*cpp)
			return &fil;
	} else {
		if (strcasecmp(*cpp, "from")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - from\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after from\n",
				linenum);
			return NULL;
		}
		ch = 0;
		if (**cpp == '!') {
			fil.fr_flags |= FR_NOTSRCIP;
			(*cpp)++;
		}
		if (hostmask(&cpp, (u_32_t *)&fil.fr_src,
			     (u_32_t *)&fil.fr_smsk, &fil.fr_sport, &ch,
			     &fil.fr_stop, linenum)) {
			return NULL;
		}
		fil.fr_scmp = ch;
		if (!*cpp) {
			fprintf(stderr, "%d: missing to fields\n", linenum);
			return NULL;
		}

		/*
		 * do the same for the to field (destination host)
		 */
		if (strcasecmp(*cpp, "to")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - to\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after to\n", linenum);
			return NULL;
		}
		ch = 0;
		if (**cpp == '!') {
			fil.fr_flags |= FR_NOTDSTIP;
			(*cpp)++;
		}
		if (hostmask(&cpp, (u_32_t *)&fil.fr_dst,
			     (u_32_t *)&fil.fr_dmsk, &fil.fr_dport, &ch,
			     &fil.fr_dtop, linenum)) {
			return NULL;
		}
		fil.fr_dcmp = ch;
	}

	/*
	 * check some sanity, make sure we don't have icmp checks with tcp
	 * or udp or visa versa.
	 */
	if (fil.fr_proto && (fil.fr_dcmp || fil.fr_scmp) &&
	    fil.fr_proto != IPPROTO_TCP && fil.fr_proto != IPPROTO_UDP) {
		fprintf(stderr, "%d: port operation on non tcp/udp\n", linenum);
		return NULL;
	}
	if (fil.fr_icmp && fil.fr_proto != IPPROTO_ICMP) {
		fprintf(stderr, "%d: icmp comparisons on wrong protocol\n",
			linenum);
		return NULL;
	}

	if (!*cpp)
		return &fil;

	if (*cpp && !strcasecmp(*cpp, "flags")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: no flags present\n", linenum);
			return NULL;
		}
		fil.fr_tcpf = tcp_flags(*cpp, &fil.fr_tcpfm, linenum);
		cpp++;
	}

	/*
	 * extras...
	 */
	if (*cpp && (!strcasecmp(*cpp, "with") || !strcasecmp(*cpp, "and")))
		if (extras(&cpp, &fil, linenum))
			return NULL;

	/*
	 * icmp types for use with the icmp protocol
	 */
	if (*cpp && !strcasecmp(*cpp, "icmp-type")) {
		if (fil.fr_proto != IPPROTO_ICMP) {
			fprintf(stderr,
				"%d: icmp with wrong protocol (%d)\n",
				linenum, fil.fr_proto);
			return NULL;
		}
		if (addicmp(&cpp, &fil, linenum))
			return NULL;
		fil.fr_icmp = htons(fil.fr_icmp);
		fil.fr_icmpm = htons(fil.fr_icmpm);
	}

	/*
	 * Keep something...
	 */
	while (*cpp && !strcasecmp(*cpp, "keep"))
		if (addkeep(&cpp, &fil, linenum))
			return NULL;

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "head")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: head without group #\n", linenum);
			return NULL;
		}
		if (ratoi(*cpp, &i, 0, USHRT_MAX))
			fil.fr_grhead = i;
		else {
			fprintf(stderr, "%d: invalid group (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
	}

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "group")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: group without group #\n",
				linenum);
			return NULL;
		}
		if (ratoi(*cpp, &i, 0, USHRT_MAX))
			fil.fr_group = i;
		else {
			fprintf(stderr, "%d: invalid group (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
	}

	/*
	 * leftovers...yuck
	 */
	if (*cpp && **cpp) {
		fprintf(stderr, "%d: unknown words at end: [", linenum);
		for (; *cpp; cpp++)
			fprintf(stderr, "%s ", *cpp);
		fprintf(stderr, "]\n");
		return NULL;
	}

	/*
	 * lazy users...
	 */
	if ((fil.fr_tcpf || fil.fr_tcpfm) && fil.fr_proto != IPPROTO_TCP) {
		fprintf(stderr, "%d: TCP protocol not specified\n", linenum);
		return NULL;
	}
	if (!(fil.fr_ip.fi_fl & FI_TCPUDP) && (fil.fr_proto != IPPROTO_TCP) &&
	    (fil.fr_proto != IPPROTO_UDP) && (fil.fr_dcmp || fil.fr_scmp)) {
		if (!fil.fr_proto) {
			fil.fr_ip.fi_fl |= FI_TCPUDP;
			fil.fr_mip.fi_fl |= FI_TCPUDP;
		} else {
			fprintf(stderr,
				"%d: port comparisons for non-TCP/UDP\n",
				linenum);
			return NULL;
		}
	}
/*
	if ((fil.fr_flags & FR_KEEPFRAG) &&
	    (!(fil.fr_ip.fi_fl & FI_FRAG) || !(fil.fr_ip.fi_fl & FI_FRAG))) {
		fprintf(stderr,
			"%d: must use 'with frags' with 'keep frags'\n",
			linenum);
		return NULL;
	}
*/
	return &fil;
}


int to_interface(fdp, to, linenum)
frdest_t *fdp;
char *to;
int linenum;
{
	int	r = 0;
	char	*s;

	s = index(to, ':');
	fdp->fd_ifp = NULL;
	if (s) {
		*s++ = '\0';
		fdp->fd_ip.s_addr = hostnum(s, &r, linenum);
		if (r == -1)
			return -1;
	}
	(void) strncpy(fdp->fd_ifname, to, sizeof(fdp->fd_ifname) - 1);
	fdp->fd_ifname[sizeof(fdp->fd_ifname) - 1] = '\0';
	return 0;
}


void print_toif(tag, fdp)
char *tag;
frdest_t *fdp;
{
	printf("%s %s%s", tag, fdp->fd_ifname,
		     (fdp->fd_ifp || (long)fdp->fd_ifp == -1) ? "" : "(!)");
	if (fdp->fd_ip.s_addr)
		printf(":%s", inet_ntoa(fdp->fd_ip));
	putchar(' ');
}


/*
 * returns -1 if neither "hostmask/num" or "hostmask mask addr" are
 * found in the line segments, there is an error processing this information,
 * or there is an error processing ports information.
 */
int	hostmask(seg, sa, msk, pp, cp, tp, linenum)
char	***seg;
u_32_t	*sa, *msk;
u_short	*pp, *tp;
u_char	*cp;
int     linenum;
{
	char	*s, *endptr;
	int	bits = -1, resolved;
	struct	in_addr	maskaddr;

	/*
	 * is it possibly hostname/num ?
	 */
	if ((s = index(**seg, '/')) || (s = index(**seg, ':'))) {
		*s++ = '\0';
		if (index(s, '.') || index(s, 'x')) {
			/* possibly of the form xxx.xxx.xxx.xxx
			 * or 0xYYYYYYYY */
			if (inet_aton(s, &maskaddr) == 0) {
				fprintf(stderr, "%d: bad mask (%s)\n",
					linenum, s);
				return -1;
			} 
			*msk = maskaddr.s_addr;
		} else {
			/*
			 * set x most significant bits
			 */
			bits = (int)strtol(s, &endptr, 0);
			if (*endptr != '\0' || bits > 32 || bits < 0) {
				fprintf(stderr, "%d: bad mask (/%s)\n",
					linenum, s);
				return -1;
			}
			if (bits == 0)
				*msk = 0;
			else
				*msk = htonl(0xffffffff << (32 - bits));
		}
		*sa = hostnum(**seg, &resolved, linenum) & *msk;
		if (resolved == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		(*seg)++;
		return ports(seg, pp, cp, tp, linenum);
	}

	/*
	 * look for extra segments if "mask" found in right spot
	 */
	if (*(*seg+1) && *(*seg+2) && !strcasecmp(*(*seg+1), "mask")) {
		*sa = hostnum(**seg, &resolved, linenum);
		if (resolved == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		(*seg)++;
		(*seg)++;
		if (inet_aton(**seg, &maskaddr) == 0) {
			fprintf(stderr, "%d: bad mask (%s)\n", linenum, **seg);
			return -1;
		}
		*msk = maskaddr.s_addr;
		(*seg)++;
		*sa &= *msk;
		return ports(seg, pp, cp, tp, linenum);
	}

	if (**seg) {
		*sa = hostnum(**seg, &resolved, linenum);
		if (resolved == -1) {
			fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
			return -1;
		}
		(*seg)++;
		*msk = (*sa ? inet_addr("255.255.255.255") : 0L);
		*sa &= *msk;
		return ports(seg, pp, cp, tp, linenum);
	}
	fprintf(stderr, "%d: bad host (%s)\n", linenum, **seg);
	return -1;
}

/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
u_32_t	hostnum(host, resolved, linenum)
char	*host;
int	*resolved;
int     linenum;
{
	struct	hostent	*hp;
	struct	netent	*np;
	struct	in_addr	ip;

	*resolved = 0;
	if (!strcasecmp("any", host))
		return 0;
	if (isdigit(*host) && inet_aton(host, &ip))
		return ip.s_addr;

	if (!strcasecmp("<thishost>", host))
		host = thishost;

	if (!(hp = gethostbyname(host))) {
		if (!(np = getnetbyname(host))) {
			*resolved = -1;
			fprintf(stderr, "%d: can't resolve hostname: %s\n",
				linenum, host);
			return 0;
		}
		return htonl(np->n_net);
	}
	return *(u_32_t *)hp->h_addr;
}

/*
 * check for possible presence of the port fields in the line
 */
int	ports(seg, pp, cp, tp, linenum)
char	***seg;
u_short	*pp, *tp;
u_char	*cp;
int     linenum;
{
	int	comp = -1;

	if (!*seg || !**seg || !***seg)
		return 0;
	if (!strcasecmp(**seg, "port") && *(*seg + 1) && *(*seg + 2)) {
		(*seg)++;
		if (isdigit(***seg) && *(*seg + 2)) {
			if (portnum(**seg, pp, linenum) == 0)
				return -1;
			(*seg)++;
			if (!strcmp(**seg, "<>"))
				comp = FR_OUTRANGE;
			else if (!strcmp(**seg, "><"))
				comp = FR_INRANGE;
			else {
				fprintf(stderr,
					"%d: unknown range operator (%s)\n",
					linenum, **seg);
				return -1;
			}
			(*seg)++;
			if (**seg == NULL) {
				fprintf(stderr, "%d: missing 2nd port value\n",
					linenum);
				return -1;
			}
			if (portnum(**seg, tp, linenum) == 0)
				return -1;
		} else if (!strcmp(**seg, "=") || !strcasecmp(**seg, "eq"))
			comp = FR_EQUAL;
		else if (!strcmp(**seg, "!=") || !strcasecmp(**seg, "ne"))
			comp = FR_NEQUAL;
		else if (!strcmp(**seg, "<") || !strcasecmp(**seg, "lt"))
			comp = FR_LESST;
		else if (!strcmp(**seg, ">") || !strcasecmp(**seg, "gt"))
			comp = FR_GREATERT;
		else if (!strcmp(**seg, "<=") || !strcasecmp(**seg, "le"))
			comp = FR_LESSTE;
		else if (!strcmp(**seg, ">=") || !strcasecmp(**seg, "ge"))
			comp = FR_GREATERTE;
		else {
			fprintf(stderr, "%d: unknown comparator (%s)\n",
					linenum, **seg);
			return -1;
		}
		if (comp != FR_OUTRANGE && comp != FR_INRANGE) {
			(*seg)++;
			if (portnum(**seg, pp, linenum) == 0)
				return -1;
		}
		*cp = comp;
		(*seg)++;
	}
	return 0;
}

/*
 * find the port number given by the name, either from getservbyname() or
 * straight atoi(). Return 1 on success, 0 on failure
 */
int	portnum(name, port, linenum)
char	*name;
u_short	*port;
int     linenum;
{
	struct	servent	*sp, *sp2;
	u_short	p1 = 0;
	int i;
	if (isdigit(*name)) {
		if (ratoi(name, &i, 0, USHRT_MAX)) {
			*port = (u_short)i;
			return 1;
		}
		fprintf(stderr, "%d: unknown port \"%s\"\n", linenum, name);
		return 0;
	}
	if (proto != NULL && strcasecmp(proto, "tcp/udp") != 0) {
		sp = getservbyname(name, proto);
		if (sp) {
			*port = ntohs(sp->s_port);
			return 1;
		}
		fprintf(stderr, "%d: unknown service \"%s\".\n", linenum, name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp) 
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		fprintf(stderr, "%d: unknown tcp/udp service \"%s\".\n",
			linenum, name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		fprintf(stderr, "%d: %s %d/tcp is a different port to ",
			linenum, name, p1);
		fprintf(stderr, "%d: %s %d/udp\n", linenum, name, sp->s_port);
		return 0;
	}
	*port = ntohs(p1);
	return 1;
}


u_char tcp_flags(flgs, mask, linenum)
char *flgs;
u_char *mask;
int    linenum;
{
	u_char tcpf = 0, tcpfm = 0, *fp = &tcpf;
	char *s, *t;

	for (s = flgs; *s; s++) {
		if (*s == '/' && fp == &tcpf) {
			fp = &tcpfm;
			continue;
		}
		if (!(t = index(flagset, *s))) {
			fprintf(stderr, "%d: unknown flag (%c)\n", linenum, *s);
			return 0;
		}
		*fp |= flags[t - flagset];
	}
	if (!tcpfm)
		tcpfm = 0xff;
	*mask = tcpfm;
	return tcpf;
}


/*
 * deal with extra bits on end of the line
 */
int	extras(cp, fr, linenum)
char	***cp;
struct	frentry	*fr;
int     linenum;
{
	u_short	secmsk;
	u_long	opts;
	int	notopt;
	char	oflags;

	opts = 0;
	secmsk = 0;
	notopt = 0;
	(*cp)++;
	if (!**cp)
		return -1;

	while (**cp && (!strncasecmp(**cp, "ipopt", 5) ||
	       !strncasecmp(**cp, "not", 3) || !strncasecmp(**cp, "opt", 4) ||
	       !strncasecmp(**cp, "frag", 3) || !strncasecmp(**cp, "no", 2) ||
	       !strncasecmp(**cp, "short", 5))) {
		if (***cp == 'n' || ***cp == 'N') {
			notopt = 1;
			(*cp)++;
			continue;
		} else if (***cp == 'i' || ***cp == 'I') {
			if (!notopt)
				fr->fr_ip.fi_fl |= FI_OPTIONS;
			fr->fr_mip.fi_fl |= FI_OPTIONS;
			goto nextopt;
		} else if (***cp == 'f' || ***cp == 'F') {
			if (!notopt)
				fr->fr_ip.fi_fl |= FI_FRAG;
			fr->fr_mip.fi_fl |= FI_FRAG;
			goto nextopt;
		} else if (***cp == 'o' || ***cp == 'O') {
			if (!*(*cp + 1)) {
				fprintf(stderr,
					"%d: opt missing arguements\n",
					linenum);
				return -1;
			}
			(*cp)++;
			if (!(opts = optname(cp, &secmsk, linenum)))
				return -1;
			oflags = FI_OPTIONS;
		} else if (***cp == 's' || ***cp == 'S') {
			if (fr->fr_tcpf) {
				fprintf(stderr,
				"%d: short cannot be used with TCP flags\n",
					linenum);
				return -1;
			}

			if (!notopt)
				fr->fr_ip.fi_fl |= FI_SHORT;
			fr->fr_mip.fi_fl |= FI_SHORT;
			goto nextopt;
		} else
			return -1;

		if (!notopt || !opts)
			fr->fr_mip.fi_fl |= oflags;
		if (notopt) {
		  if (!secmsk) {
				fr->fr_mip.fi_optmsk |= opts;
		  } else {
				fr->fr_mip.fi_optmsk |= (opts & ~0x0100);
		  }
		} else {
				fr->fr_mip.fi_optmsk |= opts;
		}
		fr->fr_mip.fi_secmsk |= secmsk;

		if (notopt) {
			fr->fr_ip.fi_fl &= (~oflags & 0xf);
			fr->fr_ip.fi_optmsk &= ~opts;
			fr->fr_ip.fi_secmsk &= ~secmsk;
		} else {
			fr->fr_ip.fi_fl |= oflags;
			fr->fr_ip.fi_optmsk |= opts;
			fr->fr_ip.fi_secmsk |= secmsk;
		}
nextopt:
		notopt = 0;
		opts = 0;
		oflags = 0;
		secmsk = 0;
		(*cp)++;
	}
	return 0;
}


u_32_t optname(cp, sp, linenum)
char ***cp;
u_short *sp;
int linenum;
{
	struct ipopt_names *io, *so;
	u_long msk = 0;
	u_short smsk = 0;
	char *s;
	int sec = 0;

	for (s = strtok(**cp, ","); s; s = strtok(NULL, ",")) {
		for (io = ionames; io->on_name; io++)
			if (!strcasecmp(s, io->on_name)) {
				msk |= io->on_bit;
				break;
			}
		if (!io->on_name) {
			fprintf(stderr, "%d: unknown IP option name %s\n",
				linenum, s);
			return 0;
		}
		if (!strcasecmp(s, "sec-class"))
			sec = 1;
	}

	if (sec && !*(*cp + 1)) {
		fprintf(stderr, "%d: missing security level after sec-class\n",
			linenum);
		return 0;
	}

	if (sec) {
		(*cp)++;
		for (s = strtok(**cp, ","); s; s = strtok(NULL, ",")) {
			for (so = secclass; so->on_name; so++)
				if (!strcasecmp(s, so->on_name)) {
					smsk |= so->on_bit;
					break;
				}
			if (!so->on_name) {
				fprintf(stderr,
					"%d: no such security level: %s\n",
					linenum, s);
				return 0;
			}
		}
		if (smsk)
			*sp = smsk;
	}
	return msk;
}


#ifdef __STDC__
void optprint(u_short *sec, u_long optmsk, u_long optbits)
#else
void optprint(sec, optmsk, optbits)
u_short *sec;
u_long optmsk, optbits;
#endif
{
	u_short secmsk = sec[0], secbits = sec[1];
	struct ipopt_names *io, *so;
	char *s;
	int secflag = 0;

	s = " opt ";
	for (io = ionames; io->on_name; io++)
		if ((io->on_bit & optmsk) &&
		    ((io->on_bit & optmsk) == (io->on_bit & optbits))) {
			if ((io->on_value != IPOPT_SECURITY) ||
			    (!secmsk && !secbits)) {
				printf("%s%s", s, io->on_name);
				if (io->on_value == IPOPT_SECURITY)
					io++;
				s = ",";
			} else
				secflag = 1;
		}


	if (secmsk & secbits) {
		printf("%ssec-class", s);
		s = " ";
		for (so = secclass; so->on_name; so++)
			if ((secmsk & so->on_bit) &&
			    ((so->on_bit & secmsk) == (so->on_bit & secbits))) {
				printf("%s%s", s, so->on_name);
				s = ",";
			}
	}

	if ((optmsk && (optmsk != optbits)) ||
	    (secmsk && (secmsk != secbits))) {
		s = " ";
		printf(" not opt");
		if (optmsk != optbits) {
			for (io = ionames; io->on_name; io++)
				if ((io->on_bit & optmsk) &&
				    ((io->on_bit & optmsk) !=
				     (io->on_bit & optbits))) {
					if ((io->on_value != IPOPT_SECURITY) ||
					    (!secmsk && !secbits)) {
						printf("%s%s", s, io->on_name);
						s = ",";
						if (io->on_value ==
						    IPOPT_SECURITY)
							io++;
					} else
						io++;
				}
		}

		if (secmsk != secbits) {
			printf("%ssec-class", s);
			s = " ";
			for (so = secclass; so->on_name; so++)
				if ((so->on_bit & secmsk) &&
				    ((so->on_bit & secmsk) !=
				     (so->on_bit & secbits))) {
					printf("%s%s", s, so->on_name);
					s = ",";
				}
		}
	}
}

char	*icmptypes[] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", "routerad",
	"routersol", "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};

/*
 * set the icmp field to the correct type if "icmp" word is found
 */
int	addicmp(cp, fp, linenum)
char	***cp;
struct	frentry	*fp;
int     linenum;
{
	char	**t;
	int	i;

	(*cp)++;
	if (!**cp)
		return -1;
	if (!fp->fr_proto)	/* to catch lusers */
		fp->fr_proto = IPPROTO_ICMP;
	if (isdigit(***cp)) {
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr,
				"%d: Invalid icmp-type (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	} else {
		for (t = icmptypes, i = 0; ; t++, i++) {
			if (!*t)
				continue;
			if (!strcasecmp("END", *t)) {
				i = -1;
				break;
			}
			if (!strcasecmp(*t, **cp))
				break;
		}
		if (i == -1) {
			fprintf(stderr,
				"%d: Invalid icmp-type (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	}
	fp->fr_icmp = (u_short)(i << 8);
	fp->fr_icmpm = (u_short)0xff00;
	(*cp)++;
	if (!**cp)
		return 0;

	if (**cp && strcasecmp("code", **cp))
		return 0;
	(*cp)++;
	if (isdigit(***cp)) {
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr, 
				"%d: Invalid icmp code (%s) specified\n",
				linenum, **cp);
			return -1;
		}
		fp->fr_icmp |= (u_short)i;
		fp->fr_icmpm = (u_short)0xffff;
		(*cp)++;
		return 0;
	}
	fprintf(stderr, "%d: Invalid icmp code (%s) specified\n",
		linenum, **cp);
	return -1;
}


#define	MAX_ICMPCODE	12

char	*icmpcodes[] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", NULL };
/*
 * Return the number for the associated ICMP unreachable code.
 */
int icmpcode(str)
char *str;
{
	char	*s;
	int	i, len;

	if (!(s = strrchr(str, ')')))
		return -1;
	*s = '\0';
	if (isdigit(*str)) {
		if (!ratoi(str, &i, 0, 255))
			return -1;
		else
			return i;
	}
	len = strlen(str);
	for (i = 0; icmpcodes[i]; i++)
		if (!strncasecmp(str, icmpcodes[i], MIN(len,
				 strlen(icmpcodes[i])) ))
			return i;
	return -1;
}


/*
 * set the icmp field to the correct type if "icmp" word is found
 */
int	addkeep(cp, fp, linenum)
char	***cp;
struct	frentry	*fp;
int     linenum; 
{
	if (fp->fr_proto != IPPROTO_TCP && fp->fr_proto != IPPROTO_UDP &&
	    fp->fr_proto != IPPROTO_ICMP && !(fp->fr_ip.fi_fl & FI_TCPUDP)) {
		fprintf(stderr, "%d: Can only use keep with UDP/ICMP/TCP\n",
			linenum);
		return -1;
	}

	(*cp)++;
	if (**cp && strcasecmp(**cp, "state") && strcasecmp(**cp, "frags")) {
		fprintf(stderr, "%d: Unrecognised state keyword \"%s\"\n",
			linenum, **cp);
		return -1;
	}

	if (***cp == 's' || ***cp == 'S')
		fp->fr_flags |= FR_KEEPSTATE;
	else if (***cp == 'f' || ***cp == 'F')
		fp->fr_flags |= FR_KEEPFRAG;
	(*cp)++;
	return 0;
}


/*
 * count consecutive 1's in bit mask.  If the mask generated by counting
 * consecutive 1's is different to that passed, return -1, else return #
 * of bits.
 */
int	countbits(ip)
u_32_t	ip;
{
	u_32_t	ipn;
	int	cnt = 0, i, j;

	ip = ipn = ntohl(ip);
	for (i = 32; i; i--, ipn *= 2)
		if (ipn & 0x80000000)
			cnt++;
		else
			break;
	ipn = 0;
	for (i = 32, j = cnt; i; i--, j--) {
		ipn *= 2;
		if (j > 0)
			ipn++;
	}
	if (ipn == ip)
		return cnt;
	return -1;
}


char	*portname(pr, port)
int	pr, port;
{
	static	char	buf[32];
	struct	protoent	*p = NULL;
	struct	servent	*sv = NULL, *sv1 = NULL;

	if (pr == -1) {
		if ((sv = getservbyport(htons(port), "tcp"))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			sv1 = getservbyport(htons(port), "udp");
			sv = strncasecmp(buf, sv->s_name, strlen(buf)) ?
			     NULL : sv1;
		}
		if (sv)
			return buf;
	} else if (pr && (p = getprotobynumber(pr))) {
		if ((sv = getservbyport(htons(port), p->p_name))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			return buf;
		}
	}

	(void) sprintf(buf, "%d", port);
	return buf;
}


/*
 * print the filter structure in a useful way
 */
void	printfr(fp)
struct	frentry	*fp;
{
	static	char	*pcmp1[] = { "*", "=", "!=", "<", ">", "<=", ">=",
				    "<>", "><"};
	struct	protoent	*p;
	int	ones = 0, pr;
	char	*s, *u;
	u_char	*t;
	u_short	sec[2];

	if (fp->fr_flags & FR_PASS)
		printf("pass");
	else if (fp->fr_flags & FR_BLOCK) {
		printf("block");
		if (fp->fr_flags & FR_RETICMP) {
			if ((fp->fr_flags & FR_RETMASK) == FR_FAKEICMP)
				printf(" return-icmp-as-dest");
			else if ((fp->fr_flags & FR_RETMASK) == FR_RETICMP)
				printf(" return-icmp");
			if (fp->fr_icode) {
				if (fp->fr_icode <= MAX_ICMPCODE)
					printf("(%s)",
						icmpcodes[(int)fp->fr_icode]);
				else
					printf("(%d)", fp->fr_icode);
			}
		} else if ((fp->fr_flags & FR_RETMASK) == FR_RETRST)
			printf(" return-rst");
	} else if ((fp->fr_flags & FR_LOGMASK) == FR_LOG) {
		printf("log");
		if (fp->fr_flags & FR_LOGBODY)
			printf(" body");
		if (fp->fr_flags & FR_LOGFIRST)
			printf(" first");
	} else if (fp->fr_flags & FR_ACCOUNT)
		printf("count");
	else if (fp->fr_flags & FR_AUTH)
		printf("auth");
	else if (fp->fr_flags & FR_PREAUTH)
		printf("preauth");
	else if (fp->fr_skip)
		printf("skip %hu", fp->fr_skip);

	if (fp->fr_flags & FR_OUTQUE)
		printf(" out ");
	else
		printf(" in ");

	if (((fp->fr_flags & FR_LOGB) == FR_LOGB) ||
	    ((fp->fr_flags & FR_LOGP) == FR_LOGP)) {
		printf("log ");
		if (fp->fr_flags & FR_LOGBODY)
			printf("body ");
		if (fp->fr_flags & FR_LOGFIRST)
			printf("first ");
		if (fp->fr_flags & FR_LOGORBLOCK)
			printf("or-block ");
		if (fp->fr_loglevel != 0xffff) {
			if (fp->fr_loglevel & LOG_FACMASK) {
				s = fac_toname(fp->fr_loglevel);
				if (s == NULL)
					s = "!!!";
			} else
				s = "";
			u = pri_toname(fp->fr_loglevel);
			if (u == NULL)
				u = "!!!";
			if (*s)
				printf("level %s.%s ", s, u);
			else
				printf("level %s ", u);
		}
			
	}
	if (fp->fr_flags & FR_QUICK)
		printf("quick ");

	if (*fp->fr_ifname) {
		printf("on %s%s ", fp->fr_ifname,
			(fp->fr_ifa || (long)fp->fr_ifa == -1) ? "" : "(!)");
		if (*fp->fr_dif.fd_ifname)
			print_toif("dup-to", &fp->fr_dif);
		if (*fp->fr_tif.fd_ifname)
			print_toif("to", &fp->fr_tif);
		if (fp->fr_flags & FR_FASTROUTE)
			printf("fastroute ");

	}
	if (fp->fr_mip.fi_tos)
		printf("tos %#x ", fp->fr_tos);
	if (fp->fr_mip.fi_ttl)
		printf("ttl %d ", fp->fr_ttl);
	if (fp->fr_ip.fi_fl & FI_TCPUDP) {
			printf("proto tcp/udp ");
			pr = -1;
	} else if ((pr = fp->fr_mip.fi_p)) {
		if ((p = getprotobynumber(fp->fr_proto)))
			printf("proto %s ", p->p_name);
		else
			printf("proto %d ", fp->fr_proto);
	}

	printf("from %s", fp->fr_flags & FR_NOTSRCIP ? "!" : "");
	if (!fp->fr_src.s_addr && !fp->fr_smsk.s_addr)
		printf("any ");
	else {
		printf("%s", inet_ntoa(fp->fr_src));
		if ((ones = countbits(fp->fr_smsk.s_addr)) == -1)
			printf("/%s ", inet_ntoa(fp->fr_smsk));
		else
			printf("/%d ", ones);
	}
	if (fp->fr_scmp) {
		if (fp->fr_scmp == FR_INRANGE || fp->fr_scmp == FR_OUTRANGE)
			printf("port %d %s %d ", fp->fr_sport,
				     pcmp1[fp->fr_scmp], fp->fr_stop);
		else
			printf("port %s %s ", pcmp1[fp->fr_scmp],
				     portname(pr, fp->fr_sport));
	}

	printf("to %s", fp->fr_flags & FR_NOTDSTIP ? "!" : "");
	if (!fp->fr_dst.s_addr && !fp->fr_dmsk.s_addr)
		printf("any");
	else {
		printf("%s", inet_ntoa(fp->fr_dst));
		if ((ones = countbits(fp->fr_dmsk.s_addr)) == -1)
			printf("/%s", inet_ntoa(fp->fr_dmsk));
		else
			printf("/%d", ones);
	}
	if (fp->fr_dcmp) {
		if (fp->fr_dcmp == FR_INRANGE || fp->fr_dcmp == FR_OUTRANGE)
			printf(" port %d %s %d", fp->fr_dport,
				     pcmp1[fp->fr_dcmp], fp->fr_dtop);
		else
			printf(" port %s %s", pcmp1[fp->fr_dcmp],
				     portname(pr, fp->fr_dport));
	}
	if ((fp->fr_ip.fi_fl & ~FI_TCPUDP) ||
	    (fp->fr_mip.fi_fl & ~FI_TCPUDP) ||
	    fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
	    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk) {
		printf(" with");
		if (fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
		    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk) {
			sec[0] = fp->fr_mip.fi_secmsk;
			sec[1] = fp->fr_ip.fi_secmsk;
			optprint(sec,
				 fp->fr_mip.fi_optmsk, fp->fr_ip.fi_optmsk);
		} else if (fp->fr_mip.fi_fl & FI_OPTIONS) {
			if (!(fp->fr_ip.fi_fl & FI_OPTIONS))
				printf(" not");
			printf(" ipopt");
		}
		if (fp->fr_mip.fi_fl & FI_SHORT) {
			if (!(fp->fr_ip.fi_fl & FI_SHORT))
				printf(" not");
			printf(" short");
		}
		if (fp->fr_mip.fi_fl & FI_FRAG) {
			if (!(fp->fr_ip.fi_fl & FI_FRAG))
				printf(" not");
			printf(" frag");
		}
	}
	if (fp->fr_proto == IPPROTO_ICMP && fp->fr_icmpm) {
		int	type = fp->fr_icmp, code;

		type = ntohs(fp->fr_icmp);
		code = type & 0xff;
		type /= 256;
		if (type < (sizeof(icmptypes) / sizeof(char *)) &&
		    icmptypes[type])
			printf(" icmp-type %s", icmptypes[type]);
		else
			printf(" icmp-type %d", type);
		if (code)
			printf(" code %d", code);
	}
	if (fp->fr_proto == IPPROTO_TCP && (fp->fr_tcpf || fp->fr_tcpfm)) {
		printf(" flags ");
		for (s = flagset, t = flags; *s; s++, t++)
			if (fp->fr_tcpf & *t)
				(void)putchar(*s);
		if (fp->fr_tcpfm) {
			(void)putchar('/');
			for (s = flagset, t = flags; *s; s++, t++)
				if (fp->fr_tcpfm & *t)
					(void)putchar(*s);
		}
	}

	if (fp->fr_flags & FR_KEEPSTATE)
		printf(" keep state");
	if (fp->fr_flags & FR_KEEPFRAG)
		printf(" keep frags");
	if (fp->fr_grhead)
		printf(" head %d", fp->fr_grhead);
	if (fp->fr_group)
		printf(" group %d", fp->fr_group);
	(void)putchar('\n');
}

void	binprint(fp)
struct frentry *fp;
{
	int i = sizeof(*fp), j = 0;
	u_char *s;

	for (s = (u_char *)fp; i; i--, s++) {
		j++;
		printf("%02x ", *s);
		if (j == 16) {
			printf("\n");
			j = 0;
		}
	}
	putchar('\n');
	(void)fflush(stdout);
}


int	ratoi(ps, pi, min, max)
char 	*ps;
int	*pi, min, max;
{
	int i;
	char *pe;

	i = (int)strtol(ps, &pe, 0);
	if (*pe != '\0' || i < min || i > max)
		return 0;
	*pi = i;
	return 1;
}
