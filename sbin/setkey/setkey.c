/*	$NetBSD: setkey.c,v 1.1.4.2 2000/06/22 16:05:50 minoura Exp $	*/
/*	$KAME: setkey.c,v 1.14 2000/06/10 06:47:09 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <err.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <netkey/key_debug.h>
#include <netinet6/ipsec.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include "libpfkey.h"

void Usage __P((void));
int main __P((int, char **));
int get_supported __P((void));
void sendkeyshort __P((u_int));
void promisc __P((void));
int sendkeymsg __P((void));
int postproc __P((struct sadb_msg *, int));
const char *numstr __P((int));
void shortdump_hdr __P((void));
void shortdump __P((struct sadb_msg *));

#define MODE_SCRIPT	1
#define MODE_CMDDUMP	2
#define MODE_CMDFLUSH	3
#define MODE_PROMISC	4

int so;

int f_forever = 0;
int f_all = 0;
int f_debug = 0;
int f_verbose = 0;
int f_mode = 0;
int f_cmddump = 0;
int f_policy = 0;
int f_hexdump = 0;
char *pname;

u_char m_buf[BUFSIZ];
u_int m_len;

extern int lineno;

extern int parse __P((FILE **));

void
Usage()
{
	printf("Usage:\t%s [-dv] -c\n", pname);
	printf("\t%s [-dv] -f (file)\n", pname);
	printf("\t%s [-Padlv] -D\n", pname);
	printf("\t%s [-Pdv] -F\n", pname);
	printf("\t%s [-h] -x\n", pname);
	pfkey_close(so);
	exit(1);
}

int
main(ac, av)
	int ac;
	char **av;
{
	FILE *fp = stdin;
	int c;

	pname = *av;

	if (ac == 1) Usage();

	while ((c = getopt(ac, av, "acdf:hlvxDFP")) != EOF) {
		switch (c) {
		case 'c':
			f_mode = MODE_SCRIPT;
			fp = stdin;
			break;
		case 'f':
			f_mode = MODE_SCRIPT;
			if ((fp = fopen(optarg, "r")) == NULL) {
				err(-1, "fopen");
				/*NOTREACHED*/
			}
			break;
		case 'D':
			f_mode = MODE_CMDDUMP;
			break;
		case 'F':
			f_mode = MODE_CMDFLUSH;
			break;
		case 'a':
			f_all = 1;
			break;
		case 'l':
			f_forever = 1;
			break;
		case 'h':
			f_hexdump = 1;
			break;
		case 'x':
			f_mode = MODE_PROMISC;
			break;
		case 'P':
			f_policy = 1;
			break;
		case 'd':
			f_debug = 1;
			break;
		case 'v':
			f_verbose = 1;
			break;
		default:
			Usage();
			/*NOTREACHED*/
		}
	}

	switch (f_mode) {
	case MODE_CMDDUMP:
		sendkeyshort(f_policy ? SADB_X_SPDDUMP: SADB_DUMP);
		break;
	case MODE_CMDFLUSH:
		sendkeyshort(f_policy ? SADB_X_SPDFLUSH: SADB_FLUSH);
		pfkey_close(so);
		break;
	case MODE_SCRIPT:
		if (get_supported() < 0) {
			errx(-1, "%s", ipsec_strerror());
			/*NOTREACHED*/
		}
		if (parse(&fp))
			exit (1);
		break;
	case MODE_PROMISC:
		promisc();
		/*NOTREACHED*/
	default:
		Usage();
		/*NOTREACHED*/
	}

	exit(0);
}

int
get_supported()
{
	int so;

	if ((so = pfkey_open()) < 0) {
		perror("pfkey_open");
		return -1;
	}

	/* debug mode ? */
	if (f_debug)
		return 0;

	if (pfkey_send_register(so, PF_UNSPEC) < 0)
		return -1;

	if (pfkey_recv_register(so) < 0)
		return -1;

	return 0;
}

void
sendkeyshort(type)
        u_int type;
{
	struct sadb_msg *m_msg = (struct sadb_msg *)m_buf;

	m_len = sizeof(struct sadb_msg);

	m_msg->sadb_msg_version = PF_KEY_V2;
	m_msg->sadb_msg_type = type;
	m_msg->sadb_msg_errno = 0;
	m_msg->sadb_msg_satype = SADB_SATYPE_UNSPEC;
	m_msg->sadb_msg_len = PFKEY_UNIT64(m_len);
	m_msg->sadb_msg_reserved = 0;
	m_msg->sadb_msg_seq = 0;
	m_msg->sadb_msg_pid = getpid();

	sendkeymsg();

	return;
}

void
promisc()
{
	struct sadb_msg *m_msg = (struct sadb_msg *)m_buf;
	u_char rbuf[1024 * 32];	/* XXX: Enough ? Should I do MSG_PEEK ? */
	int so, len;

	m_len = sizeof(struct sadb_msg);

	m_msg->sadb_msg_version = PF_KEY_V2;
	m_msg->sadb_msg_type = SADB_X_PROMISC;
	m_msg->sadb_msg_errno = 0;
	m_msg->sadb_msg_satype = 1;
	m_msg->sadb_msg_len = PFKEY_UNIT64(m_len);
	m_msg->sadb_msg_reserved = 0;
	m_msg->sadb_msg_seq = 0;
	m_msg->sadb_msg_pid = getpid();

	if ((so = socket(PF_KEY, SOCK_RAW, PF_KEY_V2)) < 0) {
		err(1, "socket(PF_KEY)");
		/*NOTREACHED*/
	}

	if ((len = send(so, m_buf, m_len, 0)) < 0) {
		err(1, "send");
		/*NOTREACHED*/
	}

	while (1) {
		struct sadb_msg *base;

		if ((len = recv(so, rbuf, sizeof(*base), MSG_PEEK)) < 0) {
			err(1, "recv");
			/*NOTREACHED*/
		}

		if (len != sizeof(*base))
			continue;

		base = (struct sadb_msg *)rbuf;
		if ((len = recv(so, rbuf, PFKEY_UNUNIT64(base->sadb_msg_len),
				0)) < 0) {
			err(1, "recv");
			/*NOTREACHED*/
		}
		if (f_hexdump) {
			int i;
			for (i = 0; i < len; i++) {
				if (i % 16 == 0)
					printf("%08x: ", i);
				printf("%02x ", rbuf[i] & 0xff);
				if (i % 16 == 15)
					printf("\n");
			}
			if (len % 16)
				printf("\n");
		}
		/* adjust base pointer for promisc mode */
		if (base->sadb_msg_type == SADB_X_PROMISC) {
			if (sizeof(*base) < len)
				base++;
			else
				base = NULL;
		}
		if (base) {
			kdebug_sadb(base);
			printf("\n");
			fflush(stdout);
		}
	}
}

int
sendkeymsg()
{
	int so;

	u_char rbuf[1024 * 32];	/* XXX: Enough ? Should I do MSG_PEEK ? */
	int len;
	struct sadb_msg *msg;

	if ((so = pfkey_open()) < 0) {
		perror("pfkey_open");
		return -1;
	}

    {
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (setsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("setsockopt");
		goto end;
	}
    }

	if (f_forever)
		shortdump_hdr();
again:
	if (f_verbose) {
		kdebug_sadb((struct sadb_msg *)m_buf);
		printf("\n");
	}

	if ((len = send(so, m_buf, m_len, 0)) < 0) {
		perror("send");
		goto end;
	}

	msg = (struct sadb_msg *)rbuf;
	do {
		if ((len = recv(so, rbuf, sizeof(rbuf), 0)) < 0) {
			perror("recv");
			goto end;
		}

		if (PFKEY_UNUNIT64(msg->sadb_msg_len) != len) {
			warnx("invalid keymsg length");
			break;
		}

		if (f_verbose) {
			kdebug_sadb((struct sadb_msg *)rbuf);
			printf("\n");
		}
		if (postproc(msg, len) < 0)
			break;
	} while (msg->sadb_msg_errno || msg->sadb_msg_seq);

	if (f_forever) {
		fflush(stdout);
		sleep(1);
		goto again;
	}

end:
	pfkey_close(so);
	return(0);
}

int
postproc(msg, len)
	struct sadb_msg *msg;
	int len;
{

	if (msg->sadb_msg_errno != 0) {
		char inf[80];
		char *errmsg = NULL;

		if (f_mode == MODE_SCRIPT)
			snprintf(inf, sizeof(inf), "The result of line %d: ", lineno);
		else
			inf[0] = '\0';

		switch (msg->sadb_msg_errno) {
		case ENOENT:
			switch (msg->sadb_msg_type) {
			case SADB_DELETE:
			case SADB_GET:
			case SADB_X_SPDDELETE:
				errmsg = "No entry";
				break;
			case SADB_DUMP:
				errmsg = "No SAD entries";
				break;
			case SADB_X_SPDDUMP:
				errmsg = "No SPD entries";
				break;
			}
			break;
		default:
			errmsg = strerror(msg->sadb_msg_errno);
		}
		printf("%s%s.\n", inf, errmsg);
		return(-1);
	}

	switch (msg->sadb_msg_type) {
	case SADB_GET:
		pfkey_sadump(msg);
		break;

	case SADB_DUMP:
		/* filter out DEAD SAs */
		if (!f_all) {
			caddr_t mhp[SADB_EXT_MAX + 1];
			struct sadb_sa *sa;
			pfkey_align(msg, mhp);
			pfkey_check(mhp);
			if ((sa = (struct sadb_sa *)mhp[SADB_EXT_SA]) != NULL) {
				if (sa->sadb_sa_state == SADB_SASTATE_DEAD)
					break;
			}
		}
		if (f_forever)
			shortdump(msg);
		else
			pfkey_sadump(msg);
		msg = (struct sadb_msg *)((caddr_t)msg +
				     PFKEY_UNUNIT64(msg->sadb_msg_len));
		if (f_verbose) {
			kdebug_sadb((struct sadb_msg *)msg);
			printf("\n");
		}
		break;

	case SADB_X_SPDDUMP:
		pfkey_spdump(msg);
		if (msg->sadb_msg_seq == 0) break;
		msg = (struct sadb_msg *)((caddr_t)msg +
				     PFKEY_UNUNIT64(msg->sadb_msg_len));
		if (f_verbose) {
			kdebug_sadb((struct sadb_msg *)msg);
			printf("\n");
		}
		break;
	}

	return(0);
}

/*------------------------------------------------------------*/
static char *satype[] = {
	NULL, NULL, "ah", "esp"
};
static char *sastate[] = {
	"L", "M", "D", "d"
};
static char *ipproto[] = {
/*0*/	"ip", "icmp", "igmp", "ggp", "ip4",
	NULL, "tcp", NULL, "egp", NULL,
/*10*/	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, "udp", NULL, NULL,
/*20*/	NULL, NULL, "idp", NULL, NULL,
	NULL, NULL, NULL, NULL, "tp",
/*30*/	NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL,
/*40*/	NULL, "ip6", NULL, "rt6", "frag6",
	NULL, "rsvp", "gre", NULL, NULL,
/*50*/	"esp", "ah", NULL, NULL, NULL,
	NULL, NULL, NULL, "icmp6", "none",
/*60*/	"dst6",
};

#define STR_OR_ID(x, tab) \
	(((x) < sizeof(tab)/sizeof(tab[0]) && tab[(x)])	? tab[(x)] : numstr(x))

const char *
numstr(x)
	int x;
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

void
shortdump_hdr()
{
	printf("%-4s %-3s %-1s %-8s %-7s %s -> %s\n",
		"time", "p", "s", "spi", "ltime", "src", "dst");
}

void
shortdump(msg)
	struct sadb_msg *msg;
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	char buf[1024], pbuf[10];
	struct sadb_sa *sa;
	struct sadb_address *saddr;
	struct sadb_lifetime *lts, *lth, *ltc;
	struct sockaddr *s;
	u_int t;
	time_t cur = time(0);

	pfkey_align(msg, mhp);
	pfkey_check(mhp);

	printf("%02lu%02lu", (u_long)(cur % 3600) / 60, (u_long)(cur % 60));

	printf(" %-3s", STR_OR_ID(msg->sadb_msg_satype, satype));

	if ((sa = (struct sadb_sa *)mhp[SADB_EXT_SA]) != NULL) {
		printf(" %-1s", STR_OR_ID(sa->sadb_sa_state, sastate));
		printf(" %08x", (u_int32_t)ntohl(sa->sadb_sa_spi));
	} else
		printf("%-1s %-8s", "?", "?");

	lts = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_SOFT];
	lth = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_HARD];
	ltc = (struct sadb_lifetime *)mhp[SADB_EXT_LIFETIME_CURRENT];
	if (lts && lth && ltc) {
		if (ltc->sadb_lifetime_addtime == 0)
			t = (u_long)0;
		else
			t = (u_long)(cur - ltc->sadb_lifetime_addtime);
		if (t >= 1000)
			strcpy(buf, " big/");
		else
			snprintf(buf, sizeof(buf), " %3lu/", (u_long)t);
		printf("%s", buf);

		t = (u_long)lth->sadb_lifetime_addtime;
		if (t >= 1000)
			strcpy(buf, "big");
		else
			snprintf(buf, sizeof(buf), "%-3lu", (u_long)t);
		printf("%s", buf);
	} else
		printf(" ???/???");

	printf(" ");

	if ((saddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC]) != NULL) {
		if (saddr->sadb_address_proto)
			printf("%s ", STR_OR_ID(saddr->sadb_address_proto, ipproto));
		s = (struct sockaddr *)(saddr + 1);
		getnameinfo(s, s->sa_len, buf, sizeof(buf),
			pbuf, sizeof(pbuf), NI_NUMERICHOST|NI_NUMERICSERV);
		if (strcmp(pbuf, "0") != 0)
			printf("%s[%s]", buf, pbuf);
		else
			printf("%s", buf);
	} else
		printf("?");

	printf(" -> ");

	if ((saddr = (struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST]) != NULL) {
		if (saddr->sadb_address_proto)
			printf("%s ", STR_OR_ID(saddr->sadb_address_proto, ipproto));

		s = (struct sockaddr *)(saddr + 1);
		getnameinfo(s, s->sa_len, buf, sizeof(buf),
			pbuf, sizeof(pbuf), NI_NUMERICHOST|NI_NUMERICSERV);
		if (strcmp(pbuf, "0") != 0)
			printf("%s[%s]", buf, pbuf);
		else
			printf("%s", buf);
	} else
		printf("?");

	printf("\n");
}
